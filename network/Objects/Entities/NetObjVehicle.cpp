//
// name:        NetObjVehicle.cpp
// description: Derived from netObject, this class handles all vehicle-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjVehicle.h"

// framework headers
#include "fwnet/netlogutils.h"
#include "fwscript/scriptinterface.h"
#include "fwscene/stores/staticboundsstore.h"

// game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "game/Dispatch/RoadBlock.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/Task/TaskVehicleAnimation.h"
#include "vehicleAi/Task/TaskVehicleMissionBase.h"
#include "vehicleAi/Task/TaskVehicleCruise.h"
#include "vehicleAi/Task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/Task/TaskVehicleGoToHelicopter.h"
#include "vehicleAi/Task/TaskVehicleGoToPlane.h"
#include "vehicleAi/Task/TaskVehicleGoToSubmarine.h"
#include "vehicleAi/task/TaskVehiclePlayer.h"
#include "vehicleAi/VehicleAILod.h"
#include "network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/entities/NetObjHeli.h"
#include "network/players/NetworkPlayerMgr.h"
#include "network/Debug/NetworkDebug.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkDamageTracker.h"
#include "network/general/NetworkVehicleModelDoorLockTable.h"
#include "network/objects/Entities/NetObjPlane.h"
#include "network/objects/Entities/NetObjPlayer.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/prediction/NetBlenderBoat.h"
#include "network/objects/prediction/NetBlenderHeli.h"
#include "network/objects/prediction/NetBlenderVehicle.h"
#include "network/objects/Synchronisation/SyncNodes/VehicleSyncNodes.h"
#include "peds/ped.h"
#include "peds/pedintelligence.h"
#include "physics/gtainst.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptIds.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Vehicles/door.h"
#include "vehicles/Heli.h"
#include "vehicles/Submarine.h"
#include "vehicles/Trailer.h"
#include "vehicles/Vehicle.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/VehicleGadgets.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vehicles/vehicle_channel.h"
#include "vfx/systems/VFxVehicle.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Text/TextConversion.h"
#include "Text/Text.h"
#include "vfx/decals/DecalManager.h"
#include "stats/StatsMgr.h"
#include "stats/StatsInterface.h"
#include "modelinfo/ModelSeatInfo.h"
#include "game/ModelIndices.h"
#include "vfx/systems/VfxWeapon.h"
#include "ai/debug/system/AIDebugLogManager.h"
#if __BANK
s32 CNetObjVehicle::ms_updateRateOverride = -1;
#endif

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

FW_INSTANTIATE_BASECLASS_POOL(CNetObjVehicle, MAX_NUM_NETOBJVEHICLES, atHashString("CNetObjVehicle",0x365ab39f), LARGEST_NETOBJVEHICLE_CLASS);
FW_INSTANTIATE_CLASS_POOL(CNetObjVehicle::CVehicleSyncData, MAX_NUM_NETOBJVEHICLES, atHashString("CVehicleSyncData",0x8a0de813));

#define ALLOW_CLONE_OVERRIDE_DELAY 2000

#define STANDARD_FOUR_DOOR 4
#define NUMBER_OF_BITS_PER_DOOR ( 32 / SC_DOOR_NUM ) - 1
#define POSSIBLE_DOOR_POSITIONS (u32)( powf( 2.0f, (float)NUMBER_OF_BITS_PER_DOOR ) - 1.0f )
#define MAX_DOOR_TOLERANCE ( 1.0f / (float)POSSIBLE_DOOR_POSITIONS ) * 0.5f

#define AMPHIBIOUS_ANCHOR_TIME_DELAY 1000

CNetObjVehicle::CVehicleScopeData   CNetObjVehicle::ms_vehicleScopeData;
u32                                 CNetObjVehicle::ms_numVehiclesOutsidePopulationRange = 0;

float CNetObjVehicle::ms_PredictedPosErrorThresholdSqr[CNetworkSyncDataULBase::NUM_UPDATE_LEVELS] =
{
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f
};

// window damage is current stored in a u8
CompileTimeAssert(SIZEOF_WINDOW_DAMAGE <= 8);

extern CBoatBlenderData s_boatBlenderData;
extern CHeliBlenderData	s_planeBlenderData;
CVehicleBlenderData     s_vehicleBlenderData;
CLargeSubmarineBlenderData s_largeSubmarineBlenderData;

CVehicleBlenderData *CNetObjVehicle::ms_vehicleBlenderData = &s_vehicleBlenderData;
CBoatBlenderData    *CNetObjVehicle::ms_boatBlenderData = &s_boatBlenderData;
CHeliBlenderData    *CNetObjVehicle::ms_planeBlenderData = &s_planeBlenderData;
CLargeSubmarineBlenderData *CNetObjVehicle::ms_largeSubmarineBlenderData = &s_largeSubmarineBlenderData;

// ================================================================================================================
// CNetObjVehicle
// ================================================================================================================
static const unsigned int NETWORK_VEHICLE_CLONE_RESPOT_VALUE = 10000;

CNetObjVehicle::CNetObjVehicle(CVehicle* pVehicle,
                        const NetworkObjectType type,
                        const ObjectId objectID,
                        const PhysicalPlayerIndex playerIndex,
                        const NetObjFlags localFlags,
                        const NetObjFlags globalFlags) :
CNetObjPhysical(pVehicle, type, objectID, playerIndex, localFlags, globalFlags),
m_playerLocks(0),
m_teamLocks(0),
m_teamLockOverrides(0),
m_pCloneAITask(0),
m_deformationDataFL(0),
m_deformationDataFR(0),
m_deformationDataML(0),
m_deformationDataMR(0),
m_deformationDataRL(0),
m_deformationDataRR(0),
m_WindowsBroken(0),
m_alarmSet(false),
m_alarmActivated(false),
m_frontWindscreenSmashed(false),
m_rearWindscreenSmashed(false),
m_registeredWithKillTracker(false),
m_hasUnregisteringOccupants(false),
m_lockedForNonScriptPlayers(false),
m_bHornOffPending(false),
m_bHornAppliedThisFrame(false),
m_bHornOnFromNetwork(false),
m_bRemoteIsInAir(false),
m_bTimedExplosionPending(false),
m_bWasBlownUpByTimedExplosion(false),
m_bPhoneExplosionPending(false),
m_PedInSeatStateFullySynced(true),
m_CloneIsResponsibleForCollision(false),
m_LastPositionWithWheelContacts(VEC3_ZERO),
m_fixedCount(0),
m_extinguishedFireCount(0),
m_bHasBadgeData(false),
m_PreventCloneMigrationCountdown(0),
m_FrontBumperDamageState(VEH_BB_NONE),
m_RearBumperDamageState(VEH_BB_NONE),
m_LastThrottleReceived(0.0f),
m_GarageInstanceIndex(INVALID_GARAGE_INDEX),
m_CountedByDispatch(false),
m_bForceProcessApplyNetworkDeformation(false),
m_AllowCloneOverrideCheckDistanceTime(0),
m_vehicleBadgeInProcess(false),
m_IsCargoVehicle(false),
m_FixIfNotDummyBeforePhysics(false),
m_HasPendingLockState(false),
m_HasPendingDontTryToEnterIfLockedForPlayer(false),
m_PendingDontTryToEnterIfLockedForPlayer(false),
m_PendingPlayerLocks(0),
m_bHasPendingExclusiveDriverID(false),
m_bHasPendingLastDriver(false),
m_uLastProcessRemoteDoors(0),
m_remoteDoorOpen(0),
m_bModifiedLowriderSuspension(false),
m_bAllLowriderHydraulicsRaised(false),
m_ForceSendHealthNode(false),
m_ForceSendScriptGameStateNode(false),
m_usingBoatBlenderData(false),
m_usingPlaneBlenderData(false),
m_disableCollisionUponCreation(false),
m_FrameForceSendRequested(0),
m_PlayersUsedExtendedRange(0),
m_LaunchedInAirTimer(0),
m_bLockedToXY(false),
m_attemptedAnchorTime(0),
m_pendingParachuteObjectId(NETWORK_INVALID_OBJECT_ID),
m_cloneGliderState(0),
m_lastReceivedVerticalFlightModeRatio(0.0f),
m_detachedTombStone(false)
{
	for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
	{
		m_PendingExclusiveDriverID[i] = NETWORK_INVALID_OBJECT_ID;
	}

	m_LastDriverID = NETWORK_INVALID_OBJECT_ID;
	m_nLastDriverTime = 0;

	for (int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
	{
		m_pVehicleBadgeData[i] = NULL;
	}

    DEV_ONLY(m_timeCreated = fwTimer::GetSystemTimeInMilliseconds());

    ResetRoadNodeHistory();

    ResetLastPedsInSeats();

	ClearWeaponImpactPointInformation();

	for( u32 i = 0; i < SC_DOOR_NUM; i++ )
	{
		m_remoteDoorOpenRatios[ i ] = 0;
	}

    for( u32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++ )
	{
		m_fLowriderSuspension[ i ] = 0.0f;
	}

#if __BANK
	m_validateFailTimer = 0;
#endif

#if __ASSERT
	if(pVehicle)
	{
		gnetAssertf(pVehicle->IsVehicleAllowedInMultiplayer(), "ERROR : Vehicle %d %s is not allowed in MP!", pVehicle->GetModelId().ConvertToU32(), pVehicle->GetModelName() );
	}
#endif /* __ASSERT */
}

CNetObjVehicle::~CNetObjVehicle()
{
	gnetAssertf(!m_pCloneAITask, "Deleting a network vehicle that still have a vehicle clone task!");

	for (int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
	{
		ResetVehicleBadge(i);
	}
}

netINodeDataAccessor *CNetObjVehicle::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IVehicleNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IVehicleNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjPhysical::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

void CNetObjVehicle::LockForPlayer(PhysicalPlayerIndex playerIndex, bool bLock)
{
	if (gnetVerify(playerIndex != INVALID_PLAYER_INDEX))
	{
		if (bLock)
		{
			m_playerLocks |= (1<<playerIndex);
		}
		else
		{
			m_playerLocks &= ~(1<<playerIndex);
		}
	}
}

void CNetObjVehicle::LockForAllPlayers(bool bLock)
{
	if (bLock)
	{
		m_playerLocks = rage::PlayerFlags(~0);
	}
	else
	{
		m_playerLocks = 0;
	}
}

void CNetObjVehicle::LockForNonScriptPlayers(bool bLock)
{
	m_lockedForNonScriptPlayers = bLock;
}

bool CNetObjVehicle::IsLockedForPlayer(PhysicalPlayerIndex playerIndex) const
{
	bool bLocked = false;

	if (gnetVerify(playerIndex != INVALID_PLAYER_INDEX))
	{
		bLocked = (m_playerLocks & (1<<playerIndex)) != 0;
	}

    if(!bLocked)
    {
        CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));

        if(pPlayer)
        {
            int team = pPlayer->GetTeam();

            if(team >= 0 && team < MAX_NUM_TEAMS)
            {
                if((m_teamLockOverrides & (1<<playerIndex)) == 0)
                {
                    bLocked = (m_teamLocks & (1<<team)) != 0;
                }
            }
        }
    }

	if (!bLocked && m_lockedForNonScriptPlayers && GetScriptObjInfo())
	{
		if (!scriptInterface::IsScriptRegistered(GetScriptObjInfo()->GetScriptId()) || 
			!scriptInterface::IsPlayerAParticipant(GetScriptObjInfo()->GetScriptId(), playerIndex))
		{
			bLocked = true;
		}
	}

	if( !bLocked )
	{
		// if this is a player....
		CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));
		if(pPlayer)
		{
			CVehicle *vehicle = GetVehicle();
			gnetAssert(vehicle);

			int GUID = CTheScripts::GetGUIDFromEntity(*vehicle);

			// Am I locked by model type (e.g all infernus cars are locked)...
			bool modelTypeLocked		= CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelLocked(playerIndex, vehicle->GetModelIndex());

			// is this vehicle registered and specifically locked?
			bool modelInstanceLocked	= CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceRegistered(playerIndex, GUID)
										&& CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceLocked(playerIndex, GUID);
			
			// is this vehicle registered and specifically unlocked?
			bool modelInstanceUnlocked  = CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceRegistered(playerIndex, GUID)
										&& !CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceLocked(playerIndex, GUID);

			// assert that either the (model instance isn't registered) OR it has and it's regsitered as (locked OR unlocked)
			gnetAssert((!modelInstanceLocked && !modelInstanceUnlocked) || (modelInstanceLocked == !modelInstanceUnlocked));

			// i've been model locked and haven't been instance unlocked...
			if((modelTypeLocked) && (!modelInstanceUnlocked))
			{
				// locked...
				bLocked = true;
			}
			
			// I've been deliberately instance locked....
			if(modelInstanceLocked)
			{
				bLocked = true;
			}
		}
	}

	return bLocked;
}

void CNetObjVehicle::LockForTeam(u8 teamIndex, bool bLock)
{
    if (gnetVerify(teamIndex < MAX_NUM_TEAMS))
	{
		if (bLock)
		{
			m_teamLocks |= (1<<teamIndex);
		}
		else
		{
			m_teamLocks &= ~(1<<teamIndex);
		}
	}
}

void CNetObjVehicle::LockForAllTeams(bool bLock)
{
    TeamFlags allTeamsMask = (1<<MAX_NUM_TEAMS)-1;

    if (bLock)
	{
		m_teamLocks = allTeamsMask;
	}
	else
	{
		m_teamLocks = 0;
	}
}

bool CNetObjVehicle::IsLockedForTeam(u8 teamIndex) const
{
    bool locked = false;

    if(teamIndex < MAX_NUM_TEAMS)
    {
        locked = (m_teamLocks & (1<<teamIndex)) != 0;
    }

    return locked;
}

void CNetObjVehicle::SetTeamLockOverrideForPlayer(PhysicalPlayerIndex playerIndex, bool bOverride)
{
    if (gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid player index specified!"))
	{
		if (bOverride)
		{
			m_teamLockOverrides |= (1<<playerIndex);
		}
		else
		{
			m_teamLockOverrides &= ~(1<<playerIndex);
		}
	}
}

void CNetObjVehicle::SetPendingPlayerLocks(PlayerFlags playerLocks)
{
    if((playerLocks != m_PendingPlayerLocks) || (playerLocks != m_playerLocks))
    {
        m_PendingPlayerLocks  = playerLocks;
        m_HasPendingLockState = true;
    }
}

void CNetObjVehicle::SetPendingDontTryToEnterIfLockedForPlayer(bool tryToEnter)
{
    CVehicle *vehicle = GetVehicle();
    
    if(vehicle && vehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer != tryToEnter)
    {
        m_PendingDontTryToEnterIfLockedForPlayer    = tryToEnter;
        m_HasPendingDontTryToEnterIfLockedForPlayer = true;
    }
}

void CNetObjVehicle::SetPendingExclusiveDriverID(ObjectId exclusiveDriverID, u32 exclusiveDriverIndex)
{
    m_bHasPendingExclusiveDriverID = true;
    m_PendingExclusiveDriverID[exclusiveDriverIndex] = exclusiveDriverID;
}

// name:        EvictAllPeds
// description: Evicts all peds inside the vehicle
void CNetObjVehicle::EvictAllPeds()
{
    CVehicle *vehicle = GetVehicle();
    gnetAssert(vehicle);

    // remove any peds inside the car
    for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
    {
        CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

        if(occupant)
        {
			// only evict peds we don't own
			if(occupant->IsClone() || occupant->IsPendingOwnerChange())
			{
				occupant->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck);
			}

            if(!occupant->IsClone())
            {
                occupant->SetVehicleData(NETWORK_INVALID_OBJECT_ID, 0);
            }
        }
        else
        {
            if(NetworkInterface::IsGameInProgress())
            {
                gnetAssertf(!vehicle->GetSeatManager()->GetPedInSeat(seatIndex), "Evicting a non-networked ped from a networked vehicle!");
            }
        }
    }
}

void CNetObjVehicle::SetWindowState(eHierarchyId id, bool isBroken)
{
    if(gnetVerifyf(id >= VEH_WINDSCREEN && id <= VEH_LAST_WINDOW, "Unexpected hierarchy ID passed to SetWindowState!"))
    {
        unsigned bit = id - VEH_WINDSCREEN;

        if(isBroken)
        {
            m_WindowsBroken |= (1<<bit);
        }
        else
        {
            m_WindowsBroken &= ~(1<<bit);
        }
    }
}

void CNetObjVehicle::SetVehiclePartDamage(eHierarchyId ePart, u8 state)
{
    switch(ePart)
    {
    case VEH_BUMPER_F:
        m_FrontBumperDamageState = state;
        break;
    case VEH_BUMPER_R:
        m_RearBumperDamageState = state;
        break;
    default:
        break;
    }
}

void CNetObjVehicle::SetTimedExplosion(ObjectId nCulpritID, u32 nExplosionTime)
{
	m_nTimedExplosionTime = nExplosionTime;
	m_bTimedExplosionPending = true;
	m_TimedExplosionCulpritID = nCulpritID;
}

void CNetObjVehicle::DetonatePhoneExplosive(ObjectId nCulpritID)
{
	m_bPhoneExplosionPending = true;
	m_PhoneExplosionCulpritID = nCulpritID;
}

// name:        SendNetworkEventIncrementAwdCarBombs
// description: Called when a car is exploded to award number of enemy kills to the player responsible for the explosion.
static void  SendNetworkEventIncrementAwdCarBombs(CVehicle* vehicle, CEntity* awardToEntity)
{
	if (!vehicle)
		return;

	if (!vehicle->InheritsFromAutomobile())
		return;

	if (!vehicle->GetNetworkObject())
		return;

	if (vehicle->GetNetworkObject()->IsClone())
		return;

	if (!StatsInterface::IsKeyValid(STAT_AWD_CAR_BOMBS_ENEMY_KILLS))
		return;

	if (!awardToEntity || !awardToEntity->GetIsTypePed())
		return;

	CPed* awardToPlayerPed = SafeCast(CPed, awardToEntity);
	if (!awardToPlayerPed->IsPlayer() || !awardToPlayerPed->GetNetworkObject())
		return;

	const CSeatManager* sm = vehicle->GetSeatManager();
	if (!sm)
		return;

	bool playerWasInsideTheVehicle = (-1 != sm->GetPedsSeatIndex(awardToPlayerPed));
	u32 enemyCount = 0;

	for (int i=0; i<sm->GetMaxSeats() && !playerWasInsideTheVehicle; i++)
	{
		CPed* ped = sm->GetPedInSeat(i);
		if (ped)
		{
			playerWasInsideTheVehicle = (ped == awardToPlayerPed);

			if (!playerWasInsideTheVehicle && ped->IsPlayer())
			{
				gnetAssert(ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner());
				if (ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner())
				{
					if (-1 == ped->GetNetworkObject()->GetPlayerOwner()->GetTeam())
					{
						enemyCount++;
					}
					else if (gnetVerify(awardToPlayerPed->GetPedIntelligence()) && awardToPlayerPed->GetPedIntelligence()->IsThreatenedBy(*ped))
					{
						enemyCount++;
					}
				}
			}
		}
	}

	if (!playerWasInsideTheVehicle && enemyCount > 0)
	{
		if (awardToPlayerPed->IsLocalPlayer())
		{
			CStatsMgr::IncrementStat(STAT_AWD_CAR_BOMBS_ENEMY_KILLS, (float)enemyCount);
		}
		else
		{
			CNetworkIncrementStatEvent::Trigger(ATSTRINGHASH("AWD_CAR_BOMBS_ENEMY_KILLS", 0x6D7F0859), (int)enemyCount, awardToPlayerPed->GetNetworkObject()->GetPlayerOwner());
		}
	}
}

void CNetObjVehicle::ProcessRemoteDoors( bool forceUpdate )
{
	// Make sure we update the door so it knows its being used, that way we can delay syncing up the doors for a small amount of time after a ped has use one.
	CVehicle* pVehicle = GetVehicle();
	if( pVehicle && 
		!pVehicle->InheritsFromBike() &&
		!pVehicle->InheritsFromTrain() )
	{
		
		int numDoors = pVehicle->GetNumDoors();

		for( s32 i = 0; i < numDoors; i++ )
		{
			CCarDoor* pDoor = pVehicle->GetDoor(i);

			if (pDoor)
			{
				if( pDoor->GetIsIntact( pVehicle ) )
				{
					if(pVehicle->IsPedUsingDoor( pDoor ))
					{
						pDoor->SetRecentlyBeingUsed();
					}
				}
			}
		}
	}



	u32 systemTimeInMs = fwTimer::GetSystemTimeInMilliseconds();
	if( forceUpdate ||
		systemTimeInMs > m_uLastProcessRemoteDoors)
	{
		m_uLastProcessRemoteDoors = systemTimeInMs + 500;

		//Update door state to ensure they stay in-line with the owner - except for trains - they are handled differently
		CVehicle* pVehicle = GetVehicle();
		if( pVehicle && 
			!pVehicle->InheritsFromBike() &&
			!pVehicle->InheritsFromTrain() )
		{
			//four + bonnet + boot + boot2 = 7
			gnetAssertf(pVehicle->GetNumDoors() <= 7, "CNetObjVehicle::ProcessRemoteDoors--doors > 7--some doors will not be remotely corrected to closed state properly numdoors[%d]",pVehicle->GetNumDoors());
			int numDoors = pVehicle->GetNumDoors();

			for( s32 i = 0; i < numDoors; i++ )
			{
				CCarDoor* pDoor = pVehicle->GetDoor(i);

				if (pDoor)
				{
					if( pDoor->GetIsIntact( pVehicle ) )
					{
						u32 ownerDoorTarget = 0;
						bool ownerDoorOpen = ( m_remoteDoorOpen & ( 1 << i ) ) > 0;
						bool localDoorOpen = !pDoor->GetIsClosed();

						if( ownerDoorOpen &&
							!pDoor->RecentlySetShutAndLatched( 1500 ) )
						{
							ownerDoorTarget = m_remoteDoorOpenRatios[ i ];

							gnetAssertf( ownerDoorTarget <= POSSIBLE_DOOR_POSITIONS, "Vehicle %d %s door %d position is invalid %d", pVehicle->GetModelId().ConvertToU32(), pVehicle->GetModelName(), i, ownerDoorTarget );
						}

						bool bIsPedUsingDoor = pDoor->GetPedRemoteUsingDoor() || pDoor->RecentlyBeingUsed(1000) || pVehicle->IsPedUsingDoor( pDoor );// Delay moving the doors for a small amount of time after its been used so we dont blat over the door position by accident.

						vehicledoorDebugf3("CNetObjVehicle::ProcessRemoteDoors i[%d] pDoor[%p] vehicle[%p][%s][%s] forceUpdate[%d] bIsPedUsingDoor[%d] GetPedRemoteUsingDoor[%d] RecentlyBeingUsed[%d] IsPedUsingDoor[%d]",
							i,
							pDoor,
							pVehicle,
							pVehicle ? pVehicle->GetModelName() : "",
							pVehicle && pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "",
							forceUpdate,
							bIsPedUsingDoor,
							pDoor->GetPedRemoteUsingDoor(),
							pDoor->RecentlyBeingUsed(1000),
							pVehicle->IsPedUsingDoor(pDoor));

						// if there is a ped using the door we don't need to sync it as the task stuff will handle door position
						if( !bIsPedUsingDoor)
						{
							if(ownerDoorOpen || localDoorOpen)
							{
								u32 currentDoorRatio	= 0;

								// if the hosts door is close but this one is open but nearly closed shut and latch it
								if( !ownerDoorOpen &&
									!pDoor->GetIsLatched( pVehicle ) &&
									pDoor->GetIsClosed( MAX_DOOR_TOLERANCE ) )
								{
									pDoor->SetShutAndLatched( pVehicle, false );
								}
								else
								{
									if( localDoorOpen )
									{
										if( ( pDoor->GetFlag( CCarDoor::IS_ARTICULATED )  &&
											  pDoor->GetFlag( CCarDoor::DRIVEN_SWINGING ) ) ||
											( !pDoor->GetFlag( CCarDoor::DRIVEN_NORESET_NETWORK ) ) )
										{
											currentDoorRatio = (u32)( ( pDoor->GetDoorRatio() * POSSIBLE_DOOR_POSITIONS ) + MAX_DOOR_TOLERANCE );
										}
										else
										{
											currentDoorRatio = (u32)( ( pDoor->GetTargetDoorRatio() * POSSIBLE_DOOR_POSITIONS ) + MAX_DOOR_TOLERANCE );
										}
									}

									if( currentDoorRatio != ownerDoorTarget ||
										localDoorOpen != ownerDoorOpen )
									{
										pDoor->ClearFlag(	CCarDoor::DRIVEN_NORESET | 
															CCarDoor::DRIVEN_NORESET_NETWORK |
															CCarDoor::DRIVEN_AUTORESET |
															CCarDoor::RELEASE_AFTER_DRIVEN |
															CCarDoor::DRIVEN_SMOOTH );

										float targetRatio = (float)ownerDoorTarget / (float)POSSIBLE_DOOR_POSITIONS;

										u32 flags = CCarDoor::DRIVEN_NORESET_NETWORK;

										if( pDoor->GetFlag( CCarDoor::IS_ARTICULATED ) )
										{
											flags |= CCarDoor::DRIVEN_SMOOTH;
										}

										if( !ownerDoorOpen )
										{
											flags |= CCarDoor::WILL_LOCK_DRIVEN;
										}

										if( pDoor->GetIsLatched( pVehicle ) )
										{
											pDoor->BreakLatch( pVehicle );
										}

										pDoor->SetTargetDoorOpenRatio( targetRatio, flags, pVehicle );
									}
								}
							}
							else
							{
								// if the door is not open on the owner and not open for us (the remote), then latch it.
								if(!pDoor->GetIsLatched(pVehicle))
								{
									pDoor->SetShutAndLatched( pVehicle, false );
								}
							}
						}
					}
				}
			}
		}
	}
}


// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns      :   True if the object wants to unregister itself
bool CNetObjVehicle::Update()
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    // keep track of the number of vehicles outside of the population spawn region (vehicle scope distance) - used by the vehicle population code
    if(FindPlayerPed())
    {
        Vector3 vehiclePos     = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
        Vector3 localPlayerPos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
        float   distSquared    = (vehiclePos - localPlayerPos).Mag2();

        if(distSquared > rage::square(GetScopeData().m_scopeDistance))
        {
            ms_numVehiclesOutsidePopulationRange++;
        }
    }

#if ENABLE_NETWORK_LOGGING
    // double-check driver is registered and controlled by the same player
	CPed* pDriver = pVehicle->GetDriver();

    if (pDriver)
    {
        CNetObjGame* pDriverObj = static_cast<CNetObjGame*>(pDriver->GetNetworkObject());

        if (AssertVerify(pDriverObj) &&
            (pDriverObj->GetPhysicalPlayerIndex() != GetPhysicalPlayerIndex()) &&
            pDriverObj->GetPlayerOwner() &&
            GetPlayerOwner() &&
            !pDriver->IsPlayer() &&
            !pDriverObj->IsLocalFlagSet(LOCALFLAG_BEINGREASSIGNED) &&
            !pDriverObj->IsScriptObject(true))
        {
            netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

            NetworkLogUtils::WriteLogEvent(log, "Cloned Driver of Cloned Vehicle have different owners!", "");
            log.WriteDataValue("DRIVER",        pDriverObj->GetLogName());
            log.WriteDataValue("DRIVER_OWNER",  pDriverObj->GetPlayerOwner()->GetLogName());
            log.WriteDataValue("VEHICLE",       GetLogName());
            log.WriteDataValue("VEHICLE_OWNER", GetPlayerOwner()->GetLogName());
        }
    }
#endif // ENABLE_NETWORK_LOGGING

    if (IsClone())
    {
		// apply network deformation with distance checks
		ApplyNetworkDeformation(true);

		ApplyRemoteWeaponImpactPointInformation();

        // apply the car alarm values
        if(m_alarmSet)
        {
            pVehicle->CarAlarmState = CAR_ALARM_SET;
        }
        else if(m_alarmActivated)
        {
            if((pVehicle->CarAlarmState == 0) || (pVehicle->CarAlarmState == CAR_ALARM_SET))
            {
                pVehicle->CarAlarmState = CAR_ALARM_DURATION;
            }
        }

		if (m_pCloneAITask)
		{
			m_pCloneAITask->CloneUpdate(pVehicle);
		}

		if( !IsInCutsceneLocallyOrRemotely() )
		{
			ProcessRemoteDoors( false );
		}

		if (pVehicle->IsModded() || pVehicle->IsPersonalVehicle())
		{
			if (pVehicle->GetLodMultiplier() <= 1.0)
			{
				const float fLODMultiplier = 5.0f;
				pVehicle->SetLodMultiplier(fLODMultiplier);
			}
		}
	}
    else
    {
		if (NetworkDebug::IsOwnershipChangeAllowed())
		{
			CPed* pPlayerDriver = NULL;
			bool preventPassingOwnership = false; 
			// if the driver is a player, or a player is getting in (he has the driver seat reserved) forcibly pass the vehicle to him
			if (pVehicle->GetDriver() && pVehicle->GetDriver()->IsNetworkClone() && pVehicle->GetDriver()->IsPlayer())
			{
				pPlayerDriver = pVehicle->GetDriver();
			}
			else if (!pVehicle->GetDriver() && pVehicle->GetComponentReservationMgr()->GetAreAnyComponentsReserved())
			{
				int iDriveBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetDriverSeat());
				CComponentReservation *pComponentReservation = pVehicle->GetComponentReservationMgr()->FindComponentReservation(iDriveBoneIndex, false);
				pPlayerDriver = pComponentReservation ? pComponentReservation->GetPedUsingComponent() : NULL;

				if (!pPlayerDriver)
				{
					int iDoorFrontLeftBoneIndex = pVehicle->GetBoneIndex(VEH_DOOR_DSIDE_F);
					pComponentReservation = pVehicle->GetComponentReservationMgr()->FindComponentReservation(iDoorFrontLeftBoneIndex, false);
					pPlayerDriver = pComponentReservation ? pComponentReservation->GetPedUsingComponent() : NULL;
				}
	
				// don't bother sending control of vehicles that are locked for players trying to enter
				if (pPlayerDriver && !pVehicle->CanPedEnterCar(pPlayerDriver))
				{
					pPlayerDriver = NULL;
				}
			}

			if (pPlayerDriver && pPlayerDriver->IsNetworkClone())
			{
				CNetGamePlayer *pPlayer = pPlayerDriver->GetNetworkObject()->GetPlayerOwner();
				CPed* pPlayerPed = pPlayer ? pPlayer->GetPlayerPed() : 0;

			    if(pVehicle->InheritsFromTrailer() && pVehicle->GetAttachParent() && pVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
				{
					CVehicle *pAttachParent = SafeCast(CVehicle, pVehicle->GetAttachParent());
					
					if (pAttachParent && !pAttachParent->IsNetworkClone())
					{
						preventPassingOwnership = true;						
					}
				}

				if (pPlayerPed && pPlayer->CanAcceptMigratingObjects() && !pPlayerPed->GetIsDeadOrDying() && !NetworkInterface::IsASpectatingPlayer(pPlayerPed) && !preventPassingOwnership)
				{
					// clear the script migration timer here, otherwise CanPassControl() will return false
					m_scriptMigrationTimer = 0;

					if (CanPassControl(*pPlayer, MIGRATE_FORCED))
					{
						CGiveControlEvent::Trigger(*pPlayer, this, MIGRATE_FORCED);
					}
				}
			}
		}

        // reset the car alarm values
        m_alarmSet       = false;
        m_alarmActivated = false;

        UpdateLastPedsInSeats();
		
		UpdateForceSendRequests();
    }

	// reset the shotgun damage impacts recorded per frame
	m_weaponImpactPointAllowOneShotgunPerFrame = true;

	if (!m_bHasBadgeData)
	{
		m_vehicleBadgeInProcess = false;
	}

	//Although this is handled largely for clones, on transfer of ownership want to ensure that this is also handled and cleared out.
	if (m_vehicleBadgeInProcess)
	{
		m_vehicleBadgeInProcess = false; //reset

		bool bTempVehicleBadgeInProgress = true;

		for(int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
		{
			if(m_pVehicleBadgeData[i] != 0)
			{
				bTempVehicleBadgeInProgress = true;

				DecalRequestState_e decalState = g_decalMan.GetVehicleBadgeRequestState(pVehicle, m_pVehicleBadgeData[i]->m_badgeIndex);
				if (decalState >= DECALREQUESTSTATE_FAILED)
				{
					Warningf("CNetObjVehicle::Update -- IsNetworkClone[%d] GetVehicleBadgeRequestState decalState >= DECALREQUESTSTATE_FAILED [%d] -- reinitiate ProcessVehicleBadge",pVehicle->IsNetworkClone(),decalState);

					bTempVehicleBadgeInProgress = g_decalMan.ProcessVehicleBadge(pVehicle, m_VehicleBadgeDesc, m_pVehicleBadgeData[i]->m_boneIndex, VECTOR3_TO_VEC3V(m_pVehicleBadgeData[i]->m_offsetPos), VECTOR3_TO_VEC3V(m_pVehicleBadgeData[i]->m_dir), VECTOR3_TO_VEC3V(m_pVehicleBadgeData[i]->m_side), m_pVehicleBadgeData[i]->m_size, false, m_pVehicleBadgeData[i]->m_badgeIndex, m_pVehicleBadgeData[i]->m_alpha);
				}
				else if ((decalState == DECALREQUESTSTATE_SUCCEEDED) || (decalState == DECALREQUESTSTATE_NOT_ACTIVE))
				{
					bTempVehicleBadgeInProgress = false;
				}

				//If any of the badge processing is true then we have processing in progress
				m_vehicleBadgeInProcess |= bTempVehicleBadgeInProgress;
			}
		}
	}

	// check timed explosive
	if(m_bTimedExplosionPending)
	{
		// account for time wrap
		u32 u = m_nTimedExplosionTime - NetworkInterface::GetNetworkTime(); 
		if((u & 0x80000000u) != 0)
		{
			CEntity* pCulprit = NULL;

			// our culprit has potentially left the session
			netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_TimedExplosionCulpritID);
			if(pObject)
			{
				CNetObjPed* pNetObjPed = SafeCast(CNetObjPed, pObject);
				pCulprit = pNetObjPed->GetPed();
			}

			if(!IsClone() && !IsPendingOwnerChange())
			{
				pVehicle->BlowUpCar(pCulprit);
				GetEventScriptNetworkGroup()->Add(CEventNetworkTimedExplosion(pVehicle, pCulprit));
				m_bTimedExplosionPending = false;

				if (pVehicle && pCulprit)
				{
					SendNetworkEventIncrementAwdCarBombs(pVehicle, pCulprit);
				}
			}

			// mark for future updates
			m_bWasBlownUpByTimedExplosion = true; 
		}
	}

	// check phone explosive
	if(m_bPhoneExplosionPending)
	{
		CEntity* pCulprit = NULL;

		// our culprit has potentially left the session
		netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PhoneExplosionCulpritID);
		if(pObject)
		{
			CNetObjPed* pNetObjPed = SafeCast(CNetObjPed, pObject);
			pCulprit = pNetObjPed->GetPed();
		}

		if(!IsClone() && !IsPendingOwnerChange())
		{
			pVehicle->BlowUpCar(pCulprit);
			m_bPhoneExplosionPending = false;

			if (pVehicle && pCulprit)
			{
				SendNetworkEventIncrementAwdCarBombs(pVehicle, pCulprit);
			}
		}
	}

	// update networked horn state
	if(m_bHornOffPending && !m_bHornAppliedThisFrame && pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->StopHorn();
		m_bHornOffPending = false;
		m_bHornOnFromNetwork = false;
	}

	m_bHornAppliedThisFrame = false;

    m_PreventCloneMigrationCountdown = static_cast<u8>(rage::Max(0, m_PreventCloneMigrationCountdown - 1));

    UpdateHasUnregisteringOccupants();

#if __BANK
    ValidateDriverAndPassengers();
#endif

    if(pVehicle->HasContactWheels() && !pVehicle->IsAsleep())
    {
        m_LastPositionWithWheelContacts = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
    }

    bool retCode = CNetObjPhysical::Update();

	//disregard for trains
	if (!pVehicle->InheritsFromTrain())
	{
		// override the collision while we are fixed by network
		if(pVehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) && CanBlendWhenFixed())
		{
			SetOverridingLocalCollision(false, "Doing fixed physics network blending");

			CTrailer *trailer = pVehicle->GetAttachedTrailer();

			if(trailer && trailer->GetNetworkObject())
			{
				CNetObjEntity *trailerNetObjEntity = SafeCast(CNetObjEntity, trailer->GetNetworkObject());
				trailerNetObjEntity->SetOverridingLocalCollision(false, "Doing fixed physics network blending");
			}
		}
	}

    UpdatePendingLockState();
    UpdatePendingExclusiveDriver();
	UpdateLastDriver();

	if (IsSuperDummyStillLaunchedIntoAir())
	{
		m_LaunchedInAirTimer -= rage::Min(m_LaunchedInAirTimer, static_cast<u16>(fwTimer::GetSystemTimeStepInMilliseconds()));
	}

    return retCode;
}

void CNetObjVehicle::UpdatePendingLockState()
{
    // check for pending lock state
    if(IsClone() || IsPendingOwnerChange())
    {
        if(m_HasPendingLockState && m_HasPendingDontTryToEnterIfLockedForPlayer)
        {
            CScriptEntityStateChangeEvent::CSetVehicleLockState parameters(m_PendingPlayerLocks, m_PendingDontTryToEnterIfLockedForPlayer);
            CScriptEntityStateChangeEvent::Trigger(this, parameters);
        }
        else if(m_HasPendingLockState)
        {
            CScriptEntityStateChangeEvent::CSetVehicleLockState parameters(m_PendingPlayerLocks);
            CScriptEntityStateChangeEvent::Trigger(this, parameters);
        }
        else if(m_HasPendingDontTryToEnterIfLockedForPlayer)
        {
            CScriptEntityStateChangeEvent::CSetVehicleLockState parameters(m_PendingDontTryToEnterIfLockedForPlayer);
            CScriptEntityStateChangeEvent::Trigger(this, parameters);
        }
    }
    else
    {
        if(m_HasPendingLockState)
        {
            SetPlayerLocks(m_PendingPlayerLocks);
        }
        if(m_HasPendingDontTryToEnterIfLockedForPlayer)
        {
            CVehicle *vehicle = GetVehicle();
            gnetAssert(vehicle);

            if(vehicle)
            {
                vehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = m_PendingDontTryToEnterIfLockedForPlayer;
            }
        }
    }

    m_HasPendingLockState                       = false;
    m_HasPendingDontTryToEnterIfLockedForPlayer = false;
    m_PendingDontTryToEnterIfLockedForPlayer    = false;
    m_PendingPlayerLocks                        = 0;
}

void CNetObjVehicle::UpdateLastDriver()
{
	CVehicle *vehicle = GetVehicle();

	if(vehicle)
	{
		if(m_bHasPendingLastDriver)
		{
			if(m_LastDriverID != NETWORK_INVALID_OBJECT_ID)
			{
				CPed *currentlastDriver = vehicle->GetLastDriver();

				if(!currentlastDriver || !currentlastDriver->GetNetworkObject() || (currentlastDriver->GetNetworkObject()->GetObjectID() != m_LastDriverID))
				{
					netObject *lastDriverNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_LastDriverID);

					if(lastDriverNetObj)
					{
						m_LastDriverID  = NETWORK_INVALID_OBJECT_ID;
						m_bHasPendingLastDriver = false;

						CPed *lastDriver = NetworkUtils::GetPedFromNetworkObject(lastDriverNetObj);

						if(lastDriver)
						{
#if __BANK
							CAILogManager::GetLog().Log("Locally updating last driver %s driver %s, on %s vehicle %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(lastDriver), AILogging::GetDynamicEntityNameSafe(lastDriver), AILogging::GetDynamicEntityIsCloneStringSafe(vehicle), AILogging::GetDynamicEntityNameSafe(vehicle));
#endif // __BANK
							vehicle->SetLastDriverFromNetwork(lastDriver);
							if(m_nLastDriverTime > vehicle->m_LastTimeWeHadADriver)
							{
								vehicle->m_LastTimeWeHadADriver = m_nLastDriverTime;
							}
						}
					}
				}
			}
			else
			{
#if __BANK
				if (vehicle->GetSeatManager()->GetLastDriver() != NULL)
				{
					CAILogManager::GetLog().Log("Locally clearing last driver on %s vehicle %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(vehicle), AILogging::GetDynamicEntityNameSafe(vehicle));
				}
#endif // __BANK
				vehicle->SetLastDriverFromNetwork(NULL);

				m_LastDriverID  = NETWORK_INVALID_OBJECT_ID;
				m_bHasPendingLastDriver = false;
			}
		}
	}
}

void CNetObjVehicle::UpdatePendingExclusiveDriver()
{
    CVehicle *vehicle = GetVehicle();

    if(vehicle)
    {
        if(m_bHasPendingExclusiveDriverID)
        {
			for (u32 i=0;i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS;++i)
			{
				if(m_PendingExclusiveDriverID[i] != NETWORK_INVALID_OBJECT_ID)
				{
					CPed *currentExclusiveDriver = vehicle->GetExclusiveDriverPed(i);

					if(!currentExclusiveDriver || !currentExclusiveDriver->GetNetworkObject() || (currentExclusiveDriver->GetNetworkObject()->GetObjectID() != m_PendingExclusiveDriverID[i]))
					{
						netObject *exclusiveDriverNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_PendingExclusiveDriverID[i]);

						if(exclusiveDriverNetObj)
						{
							m_PendingExclusiveDriverID[i]  = NETWORK_INVALID_OBJECT_ID;
							m_bHasPendingExclusiveDriverID = false;

							CPed *exclusiveDriver = NetworkUtils::GetPedFromNetworkObject(exclusiveDriverNetObj);

							if(exclusiveDriver)
							{
#if __BANK
								CAILogManager::GetLog().Log("[ExclusiveDriver] - Locally updating exclusive %s driver %s, %i on %s vehicle %s (%p)\n", AILogging::GetDynamicEntityIsCloneStringSafe(exclusiveDriver), AILogging::GetDynamicEntityNameSafe(exclusiveDriver), i, AILogging::GetDynamicEntityIsCloneStringSafe(vehicle), AILogging::GetDynamicEntityNameSafe(vehicle), vehicle);
#endif // __BANK
								vehicle->SetExclusiveDriverPed(exclusiveDriver, i);
							}
						}
					}
				}
				else
				{
#if __BANK
					if (vehicle->GetExclusiveDriverPed(i) != NULL)
					{
						CAILogManager::GetLog().Log("[ExclusiveDriver] - Locally clearing exclusive driver %i on %s vehicle %s (%p)\n", i, AILogging::GetDynamicEntityIsCloneStringSafe(vehicle), AILogging::GetDynamicEntityNameSafe(vehicle), vehicle);
					}
#endif // __BANK
					vehicle->SetExclusiveDriverPed(0, i);

					m_PendingExclusiveDriverID[i]  = NETWORK_INVALID_OBJECT_ID;
					m_bHasPendingExclusiveDriverID = false;
				}
			}
        }
    }
}

void CNetObjVehicle::UpdateBeforePhysics()
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    if(!IsClone() && m_FixIfNotDummyBeforePhysics && !pVehicle->IsDummy() && !pVehicle->ConsiderParkedForLodPurposes())
    {
        if(!pVehicle->GetIsHeli() && !pVehicle->TryToMakeIntoDummy(VDM_DUMMY))
        {
            SetFixedByNetwork(true, FBN_CANNOT_SWITCH_TO_DUMMY, NPFBN_NONE);
#if __BANK
			CAILogManager::GetLog().Log("DUMMY CONVERSION: %s Failed to convert to dummy: %s\n",
				AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), pVehicle->GetNonConversionReason());
#endif
        }
    }

    m_FixIfNotDummyBeforePhysics = false;

    CNetObjPhysical::UpdateBeforePhysics();
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjVehicle::ProcessControl()
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    CNetObjPhysical::ProcessControl();

    if (IsClone())
    {   
		// do a stripped down process control
		if (pVehicle->InheritsFromAmphibiousAutomobile())
		{
			CAmphibiousAutomobile* amphibiousVehicle = static_cast<CAmphibiousAutomobile*>(pVehicle);
			// clone amphibious should never be anchored as it causes problems with the prediction code
			if (amphibiousVehicle) 
			{
				amphibiousVehicle->GetAnchorHelper().Anchor(false);
			}
		}		
		else if (pVehicle->InheritsFromPlane())
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
			{
				pSeaPlaneExtension->GetAnchorHelper().Anchor(false);			
			}
		}

        // reset stuff for frame
        pVehicle->m_nVehicleFlags.bRestingOnPhysical = false;
        pVehicle->m_nVehicleFlags.bSuppressBrakeLight = false;

        // this is a hack, not sure how to handle status changes just yet
        // STATUS_PHYSICS is needed for boats and helis
        if ((pVehicle->GetDriver() || pVehicle->IsUsingPretendOccupants() || pVehicle->GetIsBeingTowed()) && pVehicle->GetStatus() != STATUS_PLAYER &&  pVehicle->GetStatus() != STATUS_WRECKED)
            pVehicle->SetStatus(STATUS_PHYSICS);

        // service the audio entity - this should be done once a frame
        // we want to ALWAYS do it, even if car is totally stationary and inert
        //pVehicle->m_VehicleAudioEntity.Update();

		// Apply cached lowrider suspension params (from CNetObjVehicle::GetVehicleControlData).
		pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics = m_bModifiedLowriderSuspension;
		pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised = m_bAllLowriderHydraulicsRaised;
		
		for(int i = 0; i < pVehicle->GetNumWheels(); i++)
		{
			CWheel *pWheel = pVehicle->GetWheel(i);
			if (pWheel && gnetVerifyf(i < NUM_VEH_CWHEELS_MAX, "Vehicle has more than the expected number of wheels!")
				&& pWheel->GetSuspensionRaiseAmount() != m_fLowriderSuspension[i] && !pVehicle->InheritsFromAmphibiousAutomobile())
			{
				pWheel->SetSuspensionRaiseAmount(m_fLowriderSuspension[i]);
				pWheel->GetConfigFlags().SetFlag(WCF_UPDATE_SUSPENSION);
			}
		}		

        pVehicle->CVehicle::ProcessControl();
		const float fTimeStep = fwTimer::GetTimeStep();

		// clones should not have a primary task, but can have a secondary which runs locally (eg the convertible roof task)
		gnetAssertf(pVehicle->InheritsFromTrain() || !pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY), "Task present in primary tree on a network vehicle %s : %s", pVehicle->GetNetworkObject()->GetLogName(), pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY)->GetName().c_str());
		pVehicle->GetIntelligence()->GetTaskManager()->Process(fTimeStep);

		pVehicle->GetVehicleDamage()->Process(fTimeStep);
    }
	else
	{
		if (pVehicle->InheritsFromAmphibiousAutomobile())
		{
			CAmphibiousAutomobile* amphibiousVehicle = static_cast<CAmphibiousAutomobile*>(pVehicle);
			// clone amphibious should never be anchored as it causes problems with the prediction code
			if (amphibiousVehicle) 
			{
				//Establish an anchor because one couldn't be established on ChangeOwner - continue to try at a throttled rate
				//If the amphibious has a driver - it shouldn't be anchored
				if (amphibiousVehicle->GetDriver())
				{
					m_bLockedToXY = false;
				}

				if (amphibiousVehicle->GetBoatHandling()->IsInWater() && m_bLockedToXY && !amphibiousVehicle->GetAnchorHelper().IsAnchored())
				{
					//Throttle this check
					if (m_attemptedAnchorTime < fwTimer::GetSystemTimeInMilliseconds())
					{
						m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + AMPHIBIOUS_ANCHOR_TIME_DELAY;

						//Try to anchor because it should be anchored
						if (amphibiousVehicle->GetAnchorHelper().CanAnchorHere(true))
						{
							amphibiousVehicle->GetAnchorHelper().Anchor(m_bLockedToXY);
						}
					}
				}
			}
		}	
		else if (pVehicle->InheritsFromPlane())
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			if (pPlane->GetDriver())
			{
				m_bLockedToXY = false;
			}

			if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
			{
				if (pPlane->GetIsInWater() && m_bLockedToXY && !pSeaPlaneExtension->GetAnchorHelper().IsAnchored())
				{
					//Throttle this check
					if (m_attemptedAnchorTime < fwTimer::GetSystemTimeInMilliseconds())
					{
						m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + AMPHIBIOUS_ANCHOR_TIME_DELAY;

						//Try to anchor because it should be anchored					
						if (pSeaPlaneExtension->GetAnchorHelper().CanAnchorHere(true))
						{
							pSeaPlaneExtension->GetAnchorHelper().Anchor(m_bLockedToXY);
						}							
					}
				}
			}
		}
	}
    return true;
}

void CNetObjVehicle::CreateNetBlender()
{
	CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);
	gnetAssert(!GetNetBlender());
		
	netBlender* blender = NULL;

	if(vehicle->InheritsFromAmphibiousAutomobile())
	{			
		blender = rage_new CNetBlenderBoat(vehicle, ms_boatBlenderData);
		m_usingBoatBlenderData = true;
	}
	else
	{
		if (vehicle->GetModelIndex() == MI_SUB_KOSATKA)
		{
			blender = rage_new CNetBlenderVehicle(vehicle, ms_largeSubmarineBlenderData);
		}
		else
		{
			blender = rage_new CNetBlenderVehicle(vehicle, ms_vehicleBlenderData);
		}		
	}
	
	if(AssertVerify(blender))
	{
		blender->Reset();
		SetNetBlender(blender);
	}
}

bool CNetObjVehicle::PreventMigrationWhenPhysicallyAttached(const netPlayer& player, eMigrationType migrationType) const
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	CPed* pDriver = pVehicle->GetDriver();
	if(pDriver)
	{
		CPlayerInfo* pInfo = pDriver->GetPlayerInfo();
		if(pInfo && pInfo->GetPhysicalPlayerIndex() == player.GetPhysicalPlayerIndex())
		{
			return false; 
		}
	}

	return migrationType != MIGRATE_SCRIPT;
}

bool CNetObjVehicle::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
    CVehicle *vehicle = GetVehicle();
    gnetAssert(vehicle);
    PlayerFlags clonedStateVeh, createStateVeh, removeStateVeh;
    PlayerFlags clonedStatePed, createStatePed, removeStatePed;

    if (!CNetObjPhysical::CanPassControl(player, migrationType, resultCode))
        return false;

	// vehicles pending creation cannot be passed
	if(vehicle->GetIsScheduled())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_SCHEDULED_FOR_CREATION;
		}
		return false;
	}

    // the below clone state checks for vehicle occupants are not sufficient to handle
    // all cases as unregistering occupants will no longer have a game object point, so
    // there is no way to access their clone state
    if(m_hasUnregisteringOccupants)
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_VEHICLE_OCCUPANT_CLONE_STATE;
        }
        return false;
    }

    // we don't allow the vehicle to be passed to another player while any of the peds last
    // in one of the vehicles seat has not synced their current vehicle state to any of the other players in the session.
    // If this is allowed to happen it is possible for peds to be reassigned with this vehicle incorrectly on other machines
    if(!m_PedInSeatStateFullySynced)
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_VEHICLE_OCCUPANT_SEAT_STATE_UNSYNCED;
        }
        return false;
    }

    // we can only pass control of this vehicle if any occupants that
    // migrate with the vehicle can be passed too
    GetClonedState(clonedStateVeh, createStateVeh, removeStateVeh);

    for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
    {
        CPed *occupantPed = vehicle->GetSeatManager()->GetPedInSeat(seatIndex);
        CNetObjPed *occupant = occupantPed ? static_cast<CNetObjPed*>(occupantPed->GetNetworkObject()) : 0;

        if (occupant && 
			!occupant->IsClone() && 
            occupant->MigratesWithVehicle() &&
            occupant->GetPlayerOwner() != &player)
        {
            if (!occupant->CanPassControl(player, migrationType))
            {
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_VEHICLE_OCCUPANT;
                }
                return false;
            }

            gnetAssert(!occupantPed->IsPlayer());

            // don't pass control if the driver does not have the same clone state as the vehicle
            occupant->GetClonedState(clonedStatePed, createStatePed, removeStatePed);

            if (clonedStateVeh != clonedStatePed || createStatePed != createStateVeh || removeStatePed != removeStateVeh)
            {
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_VEHICLE_OCCUPANT_CLONE_STATE;
                }
                return false;
            }
        }
    }

    // can only pass control if no vehicle components are being used
    u32 componentIndex = 0;

    for(componentIndex = 0; componentIndex < vehicle->GetComponentReservationMgr()->GetNumReserveComponents(); componentIndex++)
    {
        CComponentReservation *componentReservation = vehicle->GetComponentReservationMgr()->FindComponentReservationByArrayIndex(componentIndex);

        if(AssertVerify(componentReservation))
        {
            CPed *pedUsingComponent = componentReservation->GetPedUsingComponent();

            if(pedUsingComponent && !(pedUsingComponent->IsPlayer() && pedUsingComponent->GetNetworkObject() && pedUsingComponent->GetNetworkObject()->GetPlayerOwner() == &player))
            {
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_VEHICLE_COMPONENT_IN_USE;
                }
                return false;
            }
        }
    }

    if (IsProximityMigration(migrationType))
    {
		CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase*>(vehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY));

		if (pTask && !pTask->CanMigrate())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_VEHICLE_TASK;
			}
			return false;
		}

        // don't pass by proximity if we are being respotted
        if(vehicle->IsBeingRespotted())
        {
            if(resultCode)
            {
                *resultCode = CPC_FAIL_VEHICLE_RESPOT;
            }

            return false;
        }

		// Don't want to allow vehicles attached to the cargobob helicopter to migrate.
		if(vehicle->GetIsAttached() && vehicle->GetAttachParentVehicle() && vehicle->GetAttachParentVehicle()->GetIsHeli())
		{
			if(static_cast<CHeli*>(vehicle->GetAttachParentVehicle())->GetIsCargobob())
			{
				if(resultCode)
				{
					*resultCode = CPC_FAIL_VEHICLE_ATTACHED_TO_CARGOBOB;
				}
				return false;
			}
		}

		if ( (vehicle->GetStatus() == STATUS_WRECKED) && vehicle->IsInAir() && vehicle->GetVelocity().IsNonZero() && !vehicle->GetIsFixedFlagSet())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_WRECKED_VEHICLE_IN_AIR;
			}
			return false;
		}
    }

	CPed* driver = vehicle->GetDriver();

	// script ped drivers must migrate with their vehicle
	if (driver && driver->GetNetworkObject() && !driver->IsNetworkClone() && driver->GetNetworkObject()->IsScriptObject())
	{
		if (!driver->GetNetworkObject()->CanPassControl(player, migrationType))
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_VEHICLE_DRIVER_CANT_MIGRATE;
			}

			return false;
		}
	}

	// Can't pass control of vehicles when the local player is the driver (unless its a trailer), and unless the scripts are trying to do it
	// Also if the vehicle is an aircraft, check to ensure that the driver isn't dead - lavalley.
	if (migrationType != MIGRATE_SCRIPT)
	{
		if(driver && driver->IsLocalPlayer() && !vehicle->InheritsFromTrailer() && (!vehicle->GetIsAircraft() || !driver->IsDead()))
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_VEHICLE_PLAYER_DRIVER;
			}
			return false;
		}
	}

	// can't pass control of vehicles that are pending occupants
	if (vehicle->GetSeatManager()->GetNumScheduledOccupants())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_VEHICLE_HAS_SCHEDULED_OCCUPANTS;
		}
		return false;
	}

	// can't pass control of road block vehicles, with a few exceptions
	if(vehicle->m_nVehicleFlags.bIsInRoadBlock)
	{
		const CNetGamePlayer* pNetGamePlayer = SafeCast(const CNetGamePlayer, &player);
		const CVehicle* pVehiclePlayerEntering = pNetGamePlayer->GetPlayerPed()->GetVehiclePedEntering();
		bool bIsPlayerEntering = (vehicle == pVehiclePlayerEntering);
		bool bIsPlayerDriver = (vehicle->GetDriver() == pNetGamePlayer->GetPlayerPed());
		bool bAllowMigration = (bIsPlayerEntering || bIsPlayerDriver || IsCriticalMigration(migrationType));

		if(bAllowMigration)
		{
			CRoadBlock* pRoadBlock = CRoadBlock::Find(*vehicle);
			if(!pRoadBlock || !pRoadBlock->RemoveVeh(*vehicle))
			{
				bAllowMigration = false;
			}
		}

		if(!bAllowMigration)
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_VEHICLE_IN_ROAD_BLOCK;
			}
			return false;
		}
	}

	// If the vehicle has this flag set, it's as good as attached.
	if(vehicle->m_nVehicleFlags.bIsUnderMagnetControl)
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_VEHICLE_ATTACHED_TO_CARGOBOB;
		}
		return false;
	}

    return true;
}

bool CNetObjVehicle::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
    CVehicle* vehicle = GetVehicle();
    gnetAssert(vehicle);

	// in proximity migrations, the occupants migrate with the vehicle
	if (IsProximityMigration(migrationType))
    {
        for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
        {
            CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

            if (occupant && 
                occupant->IsClone() &&
                occupant->MigratesWithVehicle() &&
                !occupant->CanAcceptControl(player, migrationType))
            {
                if(resultCode)
                {
                    *resultCode = CAC_FAIL_VEHICLE_OCCUPANT;
                }
                return false;
            }
        }
    }

    return CNetObjPhysical::CanAcceptControl(player, migrationType, resultCode);
}

bool CNetObjVehicle::ShouldOverrideBlenderWhenStoppedPredicting() const
{
    CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

    if(netBlenderVehicle)
    {
        if(m_LastThrottleReceived > 0.01f || netBlenderVehicle->GetLastVelocityReceived().Mag2() > 0.01f)
        {
            return true;
        }
    }

    return false;
}

bool CNetObjVehicle::CanShowGhostAlpha() const
{
	CVehicle* pVehicle = GetVehicle();

	if (pVehicle)
	{
		bool bShowLocalPlayerAsGhosted = GetInvertGhosting();

		if (pVehicle->IsBeingRespotted())
		{
			return false;
		}

		if (pVehicle->ContainsLocalPlayer())
		{
			return bShowLocalPlayerAsGhosted;
		}

		CPed* pPlayerDriver = CNetworkGhostCollisions::GetDriverFromVehicle(*pVehicle);
		
		if (pPlayerDriver)
		{
			if (pPlayerDriver->IsLocalPlayer() || (pPlayerDriver->IsPlayer() && NetworkInterface::IsSpectatingPed(pPlayerDriver)))
			{
				return bShowLocalPlayerAsGhosted;
			}
		}
		else
		{
			// don't show ghost alpha for the last vehicle we were in, otherwise the vehicle briefly appears alpha'd when the player gets out
			if (pVehicle->GetLastDriver() && pVehicle->GetLastDriver()->IsLocalPlayer())
			{
				return false;
			}
		}
	}
	
	return CNetObjPhysical::CanShowGhostAlpha();
}

void CNetObjVehicle::ProcessGhosting()
{
	CNetObjPhysical::ProcessGhosting();

	if (m_bGhost && !IsClone())
	{
		CVehicle* pVehicle = GetVehicle();

		if (pVehicle)
		{
			CPed* pDriver = CNetworkGhostCollisions::GetDriverFromVehicle(*pVehicle);

			if (m_bGhost)
			{
				// reset the ghost flag if the ghost driver has left the car
				if (!pDriver || !pDriver->GetNetworkObject() || !static_cast<CNetObjPed*>(pDriver->GetNetworkObject())->IsGhost())
				{
					SetAsGhost(false BANK_ONLY(, SPG_RESET_GHOSTING));
				}
			}
		}
	}
}

#if ENABLE_NETWORK_LOGGING
const char *CNetObjVehicle::GetCannotDeleteReason(unsigned reason)
{
	switch(reason)
	{
	case CTD_VEH_OCCUPANT_CANT_DEL:
		return "Vehicle's Occupant Can't Be Deleted";
	case CTD_VEH_CHILD_CANT_DEL:
		return "Vehicle's Child Can't Be Deleted";
	case CTD_VEH_DUMMY_CHILD_CANT_DEL:
		return "Vehicle's Dummy Child Can't Be Deleted";
	case CTD_VEH_BEING_TOWED:
		return "Vehicle Being Towed";
	default:
		return CNetObjGame::GetCannotDeleteReason(reason);
	}
}
#endif // ENABLE_NETWORK_LOGGING

bool CNetObjVehicle::CanBlend(unsigned *resultCode) const
{
	if(NetworkBlenderIsOverridden(resultCode))
	{
		if(resultCode)
		{
			*resultCode = CB_FAIL_BLENDER_OVERRIDDEN;
		}

		return false;
	}

    return CNetObjPhysical::CanBlend(resultCode);
}

bool CNetObjVehicle::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
    bool overridden = CNetObjPhysical::NetworkBlenderIsOverridden(resultCode);

    if(ShouldOverrideBlenderWhenStoppedPredicting())
    {
        CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

        if(netBlenderVehicle && netBlenderVehicle->HasStoppedPredictingPosition())
        {
            if(resultCode)
            {
                *resultCode = NBO_FAIL_STOPPED_PREDICTING;
            }

            overridden = true;
        }
    }

    CVehicle *vehicle = GetVehicle();

    if(vehicle)
    {
        aiTask *secondaryTask = vehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetActiveTask();

        if(secondaryTask && secondaryTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE)
        {
            if(resultCode)
            {
                *resultCode = NBO_FAIL_RUNNING_CUTSCENE;
            }

            overridden = true;
        }

		// A vehicle being picked up by the magnet should stop blending as the pick-up sequence is simulated locally to make it look as smooth as possible on all machines.
		if(vehicle->m_nVehicleFlags.bIsUnderMagnetControl && !vehicle->GetIsAttached())
		{
			if(resultCode)
			{
				*resultCode = NBO_MAGNET_PICKUP;
			}

			overridden = true;
		}
    }

    return overridden;
}

bool CNetObjVehicle::CanBlendWhenFixed() const
{
    CVehicle *vehicle = GetVehicle();
    gnetAssert(vehicle);

    if(vehicle && vehicle->GetDriver() && vehicle->GetDriver()->IsPlayer() && IsClone() && !vehicle->GetIsAircraft())
    {
        return true;
    }

    return CNetObjPhysical::CanBlendWhenFixed();
}

void CNetObjVehicle::PrepareForScriptTermination()
{
	CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

	if(vehicle)
	{
		if(!IsClone())
		{
			// ensure any cargo vehicles are detached prior to the vehicle being destroyed
			if(vehicle->InheritsFromTrailer())
			{
				CTrailer *trailer = SafeCast(CTrailer, vehicle);

				trailer->DetachCargoVehicles();
			}

			fwAttachmentEntityExtension *vehicleAttachExt = vehicle->GetAttachmentExtension();
			if(vehicleAttachExt)
			{
				CPhysical *childAttachment = static_cast<CPhysical*>(vehicleAttachExt->GetChildAttachment());
				while(childAttachment)
				{
					if(childAttachment->GetIsTypeVehicle())
					{
						childAttachment->DetachFromParent(0);
					}

					childAttachment = static_cast<CPhysical*>(childAttachment->GetSiblingAttachment());
				}
			}

			// detach any dummy children
			vehicle->DummyDetachAllChildren();
		}
	}
}

void CNetObjVehicle::RecursiveDetachmentOfChildren(CPhysical* entityToDetach)
{
	if(!entityToDetach)
	{
		return;
	}

	if (entityToDetach->GetChildAttachment())
	{
		CPhysical* childEntity = SafeCast(CPhysical, entityToDetach->GetChildAttachment());
		if(childEntity)
		{
			RecursiveDetachmentOfChildren(childEntity);
		}
	}
	if (entityToDetach->GetSiblingAttachment())
	{
		CPhysical* childSibling = SafeCast(CPhysical, entityToDetach->GetSiblingAttachment());
		if(childSibling)
		{
			RecursiveDetachmentOfChildren(childSibling);
		}
	}

	if(entityToDetach->GetIsTypeVehicle())
	{
		SafeCast(CVehicle, entityToDetach)->DetachFromParent(0);
	}
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjVehicle::CleanUpGameObject(bool bDestroyObject)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    if(IsClone())
    {
        BANK_ONLY(NetworkDebug::AddVehicleCloneRemove());
    }

    // need to restore the game heap here in case the vehicle constructor/destructor allocates from the heap
    // ** EvictAllPeds also deallocates some game heap memory!! **
    sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
    sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

    // remove the driver and passengers from the vehicle being destroyed
    EvictAllPeds();

    // ensure any attached vehicles (i.e. trailers) are detached prior to the vehicle being destroyed
	if(pVehicle->IsNetworkClone())
	{
        // ensure any cargo vehicles are detached prior to the vehicle being destroyed
        if(pVehicle->InheritsFromTrailer())
	    {
            CTrailer *trailer = SafeCast(CTrailer, pVehicle);

		    trailer->DetachCargoVehicles();
	    }

		fwAttachmentEntityExtension *vehicleAttachExt = pVehicle->GetAttachmentExtension();
		if(vehicleAttachExt)
		{
			RecursiveDetachmentOfChildren(pVehicle);
		}

		// detach any dummy children
		pVehicle->DummyDetachAllChildren();
	}

	const bool bVehicleShouldBeDestroyed = (bDestroyObject && !GetEntityLeftAfterCleanup());
	// unregister the vehicle with the scripts when the vehicle still has a network object ptr and the reservations can be adjusted
	CTheScripts::UnregisterEntity(pVehicle, !bVehicleShouldBeDestroyed);

	pVehicle->SetNetworkObject(NULL);
    SetGameObject(0);

    if(m_pCloneAITask)
    {
        delete m_pCloneAITask;
		m_pCloneAITask = NULL;
    }

    // delete the game object if it is a clone and not autonomous
	if (bVehicleShouldBeDestroyed)
	{
        // make sure it is on the scene update so it actually gets removed
        if(!pVehicle->GetIsOnSceneUpdate() || !gnetVerifyf(pVehicle->GetUpdatingThroughAnimQueue() || pVehicle->GetIsMainSceneUpdateFlagSet(),
                                                           "Flagging a vehicle for removal (%s) that is on neither the main scene update or update through anim queue list! This will likely cause a vehicle leak!", GetLogName()))
        {
            pVehicle->AddToSceneUpdate();
        }

		// have to force the pop type here, otherwise FlagToDestroyWhenNextProcessed will fail. The mission state is not cleaned up in CTheScripts::UnregisterEntity, because the vehicle is about 
		// to be deleted
		pVehicle->PopTypeSet(POPTYPE_RANDOM_AMBIENT);

        pVehicle->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
    }
	else
	{
		if(pVehicle->GetNoCollisionFlags() & NO_COLLISION_NETWORK_OBJECTS)
		{
			pVehicle->ResetNoCollision();

			int collisionFlags = pVehicle->GetNoCollisionFlags();
			collisionFlags &= ~NO_COLLISION_NETWORK_OBJECTS;
			pVehicle->SetNoCollisionFlags((u8)collisionFlags); 
		}
	}

    sysMemAllocator::SetCurrent(*oldHeap);
}

// name:        CanDelete
// description: returns true this vehicle can be deleted
bool CNetObjVehicle::CanDelete(unsigned* LOGGING_ONLY(reason))
{
    if (!CNetObjEntity::CanDelete(LOGGING_ONLY(reason)))
        return false;

    if (!IsClone())
    {
		CVehicle* vehicle = GetVehicle();

		if(vehicle)
		{
			for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
			{
				CPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex);

				if(occupant)
				{
#if ENABLE_NETWORK_LOGGING
					unsigned attachmentDeleteReason = 0;
#endif // ENABLE_NETWORK_LOGGING

					CNetObjPed* pOccupantObj = static_cast<CNetObjPed*>(occupant->GetNetworkObject());

                    if(pOccupantObj == 0)
					{
						gnetAssertf(occupant->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD), "%s has a %s(%s[0x%p]) that is not registered as a network object!", GetLogName(), (occupant == vehicle->GetDriver()) ? "driver" : "passenger", occupant->GetLogName(), occupant);
					}
					else if (!pOccupantObj->IsScriptObject() && (occupant->IsNetworkClone() || !pOccupantObj->CanDelete(LOGGING_ONLY(&attachmentDeleteReason))))
					{
#if ENABLE_NETWORK_LOGGING
						if(reason)
							*reason = CTD_VEH_OCCUPANT_CANT_DEL;

						gnetDebug3("Vehicle(%s) can't be deleted because occupant(%s) reason: %s", GetLogName(), pOccupantObj->GetLogName(), occupant->IsNetworkClone() ? "Is Clone" : GetCannotDeleteReason(attachmentDeleteReason));
#endif // ENABLE_NETWORK_LOGGING
						// vehicles can be deleted with mission peds inside - they get kicked out
						return false;
					}
				}
			}

            // check that any attached vehicles are not clones
		    fwAttachmentEntityExtension* pAttachmentExt = vehicle->GetAttachmentExtension();
		    if(pAttachmentExt)
		    {
			    CPhysical* pAttachment = static_cast<CPhysical*>(pAttachmentExt->GetChildAttachment());
			    while(pAttachment)
			    {
				    if(pAttachment->GetIsTypeVehicle())
				    {
#if ENABLE_NETWORK_LOGGING
						unsigned attachmentDeleteReason = 0;
#endif // ENABLE_NETWORK_LOGGING
                        if(pAttachment->GetNetworkObject() == 0)
					    {
						    gnetAssertf(pAttachment->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD), "%s has an attached vehicle that is not registered as a network object!", GetLogName());
					    }
					    else if(pAttachment->IsNetworkClone() || !pAttachment->GetNetworkObject()->CanDelete(LOGGING_ONLY(&attachmentDeleteReason)))
					    {
#if ENABLE_NETWORK_LOGGING
							if(reason)
								*reason = CTD_VEH_CHILD_CANT_DEL;

							gnetDebug3("Vehicle(%s) can't be deleted because attachment(%s) reason: %s", GetLogName(), pAttachment->GetLogName(), (pAttachment->IsNetworkClone() ? "Is clone" : GetCannotDeleteReason(attachmentDeleteReason)));
#endif // ENABLE_NETWORK_LOGGING
						    return false;
					    }
				    }
				    pAttachment = static_cast<CPhysical*>(pAttachment->GetSiblingAttachment());
			    }
		    }

            if(vehicle->HasDummyAttachmentChildren())
	        {
                for(int index = 0; index < vehicle->GetMaxNumDummyAttachmentChildren(); index++)
		        {
			        CVehicle *dummyChildVehicle = vehicle->GetDummyAttachmentChild(index);

			        if(dummyChildVehicle)
			        {
                        if(dummyChildVehicle->GetNetworkObject() == 0)
					    {
						    gnetAssertf(dummyChildVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD), "%s has an dummy child attachment that is not registered as a network object!", GetLogName());
					    }
                        else if(dummyChildVehicle->IsNetworkClone() || !dummyChildVehicle->GetNetworkObject()->CanDelete())
                        {
#if ENABLE_NETWORK_LOGGING
							if(reason)
								*reason = CTD_VEH_DUMMY_CHILD_CANT_DEL;
#endif // ENABLE_NETWORK_LOGGING
                            return false;
                        }
                    }
                }
            }

			if(vehicle->GetIsBeingTowed())
			{
#if ENABLE_NETWORK_LOGGING
				if(reason)
					*reason = CTD_VEH_BEING_TOWED;
#endif // ENABLE_NETWORK_LOGGING
				return false;
			}
		}
	}

    return true;
}

float CNetObjVehicle::GetScopeDistance(const netPlayer* pRelativePlayer) const
{
	static const float kfHeliCopMultiplier = 2.0f;
	static const float SUBMARINE_SCOPE_DISTANCE = 900.0f;
	static const float VEHICLE_SCOPE_EXTENDED_POP_RANGE = 400.0f;
	static const float SCOPED_WEAPON_MAX_POP_RANGE_SCOPE = 600.0f;
	static const float ROAD_BLOCK_SCOPE = 425.0f;

	float fScopeDist = CNetObjProximityMigrateable::GetScopeDistance(pRelativePlayer);

	CVehicle* vehicle = GetVehicle();
	bool usingExtendedRange = false;

	if (vehicle)
	{
		const bool bVehicleIsAircraft = vehicle->GetIsAircraft();
		const bool bVehicleIsWrecked = vehicle->GetStatus() == STATUS_WRECKED;

		CPed* pDriver = vehicle->GetDriver();
		// If the vehicle has no driver, but the last driver was the player and they have bailed out of a plane, keep the vehicle around for longer too
		if (!pDriver && bVehicleIsAircraft && !bVehicleIsWrecked )
		{
			pDriver = vehicle->GetLastDriver();

			// Only count the driver as the player if it's their last vehicle, not if they have subsequently got into a different vehicle.
			// Otherwise you could potentially keep around multiple vehicles per player
			if( pDriver )
			{
				if( !pDriver->IsPlayer() || pDriver->GetMyVehicle() != vehicle )
				{
					pDriver = NULL;
				}
			}
		}

		if(pDriver)
		{
			if (pDriver->IsPlayer())
			{
				CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pDriver->GetNetworkObject());
				if(pNetObjPlayer && pNetObjPlayer->GetAlwaysClonePlayerVehicle())
					return FLT_MAX;

				fScopeDist *= GetPlayerDriverScopeDistanceMultiplier();
				usingExtendedRange = true;
			}
			// 
			else if( bVehicleIsAircraft && !bVehicleIsWrecked )
			{
				ePedType pedType = pDriver->GetPedType();
				if((pedType == PEDTYPE_COP) || (pedType == PEDTYPE_SWAT) || (pedType == PEDTYPE_ARMY) || (pedType == PEDTYPE_MEDIC) || (pedType == PEDTYPE_FIRE))
				{
					fScopeDist *= kfHeliCopMultiplier;
				}
			}
		}
		else if(pRelativePlayer && (m_PlayersUsedExtendedRange & (1<< pRelativePlayer->GetPhysicalPlayerIndex())) != 0)
		{
			if(!vehicle->IsNetworkClone() && vehicle->GetNetworkObject()->HasBeenCloned(*pRelativePlayer))
			{
				fScopeDist *= GetPlayerDriverScopeDistanceMultiplier();
				usingExtendedRange = true;
			}
			else
			{
				usingExtendedRange = false;
			}
			
		}
		else if( bVehicleIsAircraft && !bVehicleIsWrecked )
		{
			if (vehicle->m_nVehicleFlags.bIsLawEnforcementVehicle)
				fScopeDist *= kfHeliCopMultiplier;
		}
		else
		{
			int numSeats = vehicle->GetSeatManager()->GetMaxSeats();
			for(int i = 0; i < numSeats && !usingExtendedRange; i++)
			{
				CPed* pedInSeat = vehicle->GetPedInSeat(i);
				if(pedInSeat && pedInSeat->IsPlayer() && vehicle->GetVehicleWeaponMgr() && vehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(i))
				{
					fScopeDist *= GetPlayerDriverScopeDistanceMultiplier();
					usingExtendedRange = true;
				}
			}

			int numPlayers = NetworkInterface::GetNumPhysicalPlayers();
			for(u32 i = 0; i < numPlayers-1 && !usingExtendedRange; i++)
			{
				CNetGamePlayer* gp = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(i));
				if(gp && gp->IsValid())
				{
					CPed* ped = gp->GetPlayerPed();
					if(ped && ped->GetIsStandingOnMovableObject() && ped->GetLastValidGroundPhysical() == vehicle)
					{
						fScopeDist *= GetPlayerDriverScopeDistanceMultiplier();
						usingExtendedRange = true;
					}
				}
			}
		}

		// Increase the scope range for submarine vehicles because of their large in-game model size.
		// We are not expecting to see many submarines at the same time.
		if (vehicle->GetModelIndex() == MI_SUB_KOSATKA && fScopeDist < SUBMARINE_SCOPE_DISTANCE)
		{
			fScopeDist = SUBMARINE_SCOPE_DISTANCE;
		}
	}

	if (pRelativePlayer)
	{
		bool lookingThroughScope = false;
		bool zoomed = false;
		float weaponRange = 0.0f;

		if (fScopeDist < VEHICLE_SCOPE_EXTENDED_POP_RANGE)
		{
			const CNetGamePlayer &netgameplayer = static_cast<const CNetGamePlayer &>(*pRelativePlayer);
			if (CPed* pPlayerPed = netgameplayer.GetPlayerPed())
			{
				CNetObjPlayer const* netObjPlayer = static_cast<CNetObjPlayer*>(pPlayerPed->GetNetworkObject());			
				if(netObjPlayer && netObjPlayer->IsUsingExtendedPopulationRange())
				{
					fScopeDist = VEHICLE_SCOPE_EXTENDED_POP_RANGE;
				}
			}
		}

		if (vehicle && vehicle->m_nVehicleFlags.bIsInRoadBlock)
		{
			fScopeDist = Max(fScopeDist, ROAD_BLOCK_SCOPE);
		}

		if (fScopeDist < SCOPED_WEAPON_MAX_POP_RANGE_SCOPE && NetworkInterface::IsPlayerUsingScopedWeapon(*pRelativePlayer, &lookingThroughScope, &zoomed, &weaponRange) && lookingThroughScope && zoomed)
		{
			fScopeDist = SCOPED_WEAPON_MAX_POP_RANGE_SCOPE;
		}
	}

	if(pRelativePlayer)
	{
		if(usingExtendedRange)
		{
			m_PlayersUsedExtendedRange |= (1 << pRelativePlayer->GetPhysicalPlayerIndex());
		}
		else
		{
			m_PlayersUsedExtendedRange &= ~(1 << pRelativePlayer->GetPhysicalPlayerIndex());
		}
	}

	return fScopeDist;
}

float CNetObjVehicle::GetPlayerDriverScopeDistanceMultiplier() const
{
    static const float VEHICLE_SCOPE_PLAYER_MODIFIER = 3.0f;
    return VEHICLE_SCOPE_PLAYER_MODIFIER;
}

bool CNetObjVehicle::CanDeleteWhenFlaggedForDeletion(bool useLogging)
{
	CVehicle* vehicle = GetVehicle();
	if(vehicle)
	{
		return vehicle->CanBeDeletedSpecial(true, true, true, true, true, false, false, false, useLogging);
	}
	return false;
}

//
// name:        IsInScope
// description: This is used by the object manager to determine whether we need to create a
//              clone to represent this object on a remote machine. The decision is made using
//              the player that is passed into the method - this decision is usually based on
//              the distance between this player's players and the network object, but other criterion can
//              be used.
// Parameters:  pPlayer - the player that scope is being tested against
bool CNetObjVehicle::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
    CVehicle* vehicle = GetVehicle();
    gnetAssert(vehicle);

    bool bInScope = CNetObjPhysical::IsInScope(player, scopeReason);

    // keep vehicle in scope if its driver or any of its passengers are still in scope (eg they have the GLOBALFLAG_CLONEALWAYS flag set),
    // player vehicles are an exception to this as these are not always cloned
    if (!bInScope)
    {
        for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
        {
            CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

			if(occupant)
			{
				if(!occupant->IsClone() && (occupant->GetObjectType() != NET_OBJ_TYPE_PLAYER))
				{
					if(occupant->IsInScopeNoVehicleChecks(player))
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_IN_VEHICLE_OCCUPANT_IN_SCOPE;
						}
						bInScope = true;
					}
				}
				else if (occupant->IsClone() && (occupant->GetObjectType() != NET_OBJ_TYPE_PLAYER))
				{
					netPlayer* playerOwner = static_cast<netPlayer*>(occupant->GetPlayerOwner());
					//If the Player owns the occupant then the vehicle is in scope.
					if(playerOwner && playerOwner == &player)
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_IN_VEHICLE_OCCUPANT_OWNER;
						}
						bInScope = true;
					}
				}

				if((occupant->GetObjectType() == NET_OBJ_TYPE_PLAYER) && (occupant->GetPlayerOwner() == &player))
				{
					if (scopeReason)
					{
						*scopeReason = SCOPE_IN_VEHICLE_OCCUPANT_PLAYER;
					}
					bInScope = true;
				}
			}
		}

		// if this vehicle is attached to another in scope with the player, make this one in scope also (eg vehicles being carried by a cargobob)
		CVehicle* pAttachVehicle = vehicle->GetAttachParentVehicle();

		if (pAttachVehicle && pAttachVehicle->GetNetworkObject() && pAttachVehicle->GetNetworkObject()->IsInScope(player))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_VEHICLE_ATTACH_PARENT_IN_SCOPE;
			}
			bInScope = true;
		}
			
		if (!bInScope && vehicle->InheritsFromAutomobile() )
		{
			// If we are parachuting, keep it in scope as the parachute set-up is tricky and we want to avoid doing it more than once per flight.
			CAutomobile* automobile = SafeCast(CAutomobile, vehicle);
			if (automobile && automobile->IsParachuting())
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_IN_OBJECT_VEHICLE_GADGET_IN_SCOPE;
				}
				bInScope = true;
			}			
		}		
    }
    else
    {
        if(m_GarageInstanceIndex != INVALID_GARAGE_INDEX)
        {
            u8 playerGarageIndex = SafeCast(const CNetGamePlayer, &player)->GetGarageInstanceIndex();

            if(playerGarageIndex != m_GarageInstanceIndex)
            {
				if (scopeReason)
				{
					*scopeReason = SCOPE_OUT_VEHICLE_GARAGE_INDEX_MISMATCH;
				}
                bInScope = false;
            }
        }
    }

    return bInScope;
}

bool CNetObjVehicle::IsImportant() const
{
	CVehicle *pVehicle = GetVehicle();
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

	// if the local player is in this vehicle, it is important
	if (pLocalPlayer && pLocalPlayer->GetIsInVehicle() && pLocalPlayer->GetMyVehicle() == pVehicle)
	{
		return true;
	}

    if(pVehicle && pVehicle->m_nVehicleFlags.bIsLawEnforcementVehicle && pVehicle->GetIsLandVehicle() && pVehicle->GetVelocity().Mag2() > 0.0f && pVehicle->GetStatus() != STATUS_WRECKED)
    {
        return true;
    }

	return CNetObjPhysical::IsImportant();
}

bool CNetObjVehicle::OnReassigned()
{
    bool destroyObject = CNetObjPhysical::OnReassigned();

    if(destroyObject)
    {
        EvictAllPeds();

        CVehicle *vehicle = GetVehicle();

        if(vehicle && vehicle->ContainsLocalPlayer())
        {
            CPed *playerPed = FindPlayerPed();

            if(playerPed)
            {
                playerPed->SetPedOutOfVehicle();
            }
        }
    }

    return destroyObject;
}

// name:        PostCreate
// description: Called after an object has been created from a clone create message
void CNetObjVehicle::PostCreate()
{
	CVehicle *pVehicle = GetVehicle();
	VALIDATE_GAME_OBJECT(pVehicle);

	// if a convertible has been specified to lower the roof during a create, just pop it open
	if (pVehicle)
	{
		// flush primary task tree (leave secondary tasks, eg convertible roof task)
		pVehicle->GetIntelligence()->GetTaskManager()->AbortTasksAtTreeIndex(VEHICLE_TASK_TREE_PRIMARY);

		aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetActiveTask();
		if(pTask)
		{
			CTaskVehicleConvertibleRoof *pConvertibleRoofTask =  static_cast<CTaskVehicleConvertibleRoof*>(pTask);
			
			pConvertibleRoofTask->MoveImmediately();
		}

        eVehicleDummyMode desiredLod = CVehicleAILodManager::CalcDesiredLodForPoint(pVehicle->GetTransform().GetPosition());
        if(desiredLod != VDM_REAL)
        {
            //remote parked vehicles are created with colliders, so we have to deactivate them before converting to dummy
            if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED && !pVehicle->GetIsStatic())
            {
                pVehicle->DeActivatePhysics();
            }

			bool isAttachedToNonDummyParent = false;
			const CVehicle* attachedParent = static_cast<const CVehicle*>(pVehicle->GetAttachParent());
			if (attachedParent && !attachedParent->IsDummy())
			{
				isAttachedToNonDummyParent = true;
			}

            if(!isAttachedToNonDummyParent && !pVehicle->TryToMakeIntoDummy(desiredLod))
            {
#if __BANK
                CAILogManager::GetLog().Log("DUMMY CONVERSION: %s Failed to convert to real: %s. Net Post create\n", 
                    AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), pVehicle->GetNonConversionReason());
#endif
            }		
        }
		
		if (pVehicle->HasBulletResistantGlass())
		{
			for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
			{
				eHierarchyId windowId = (eHierarchyId)(VEH_WINDSCREEN + window);
				int boneID = pVehicle->GetBoneIndex(windowId);
				if(boneID != -1)
				{
					const int component = pVehicle->GetFragInst() ? pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneID) : -1;
					if (component != -1)
					{				
						CVehicleGlassComponent *pGlassComponent = const_cast<CVehicleGlassComponent*>(g_vehicleGlassMan.GetVehicleGlassComponent((CPhysical*)pVehicle, component));

						// We have a glass component, means that the windows has most likely been damaged
						if (pGlassComponent != NULL)
						{
							if (pGlassComponent->GetArmouredGlassPenetrationDecalsCount() > 0 && pGlassComponent->GetArmouredGlassHealth() > 0)
							{
								pGlassComponent->ApplyArmouredGlassDecals(pVehicle);
							}							
						}		
					}
				}
			}
		}
		
	}

	if(IsClone())
	{
		// set this override when the vehicle is created to account for pre-joining damage, this will allow the damage to be applied regardless of distance when the data is received.
		m_AllowCloneOverrideCheckDistanceTime = fwTimer::GetSystemTimeInMilliseconds() + ALLOW_CLONE_OVERRIDE_DELAY;

        // the blender won't have been created until after the gadget data node was applied,
        // so we need to do this after the creation has completed as well
        CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());

        if(netBlenderPhysical)
        {
            CVehicleGadgetDataNode &data = *(static_cast<CVehicleSyncTree *>(GetSyncTree())->GetVehicleGadgetNode());

            if(data.GetWasUpdated())
            {
                netBlenderPhysical->SetParentVehicleOffset(data.m_IsAttachedTrailer, data.m_OffsetFromParentVehicle);
            }
        }

		CVehiclePopulation::ResetNetworkVisibilityFail();

		ApplyRemoteWeaponImpactPointInformation();

		if (m_disableCollisionUponCreation)
		{
			SetOverridingLocalCollision(false, "m_disableCollisionUponCreation is true");	
		}

        BANK_ONLY(NetworkDebug::AddVehicleCloneCreate());
	}

	CNetObjPhysical::PostCreate();
}

void CNetObjVehicle::PostSync()
{
    CVehicleSyncTree      *vehicleSyncTree   = static_cast<CVehicleSyncTree*>(GetSyncTree());
    const netSyncDataNode *orientationNode   = vehicleSyncTree->GetOrientationNode();
    const netSyncDataNode *velocityNode      = vehicleSyncTree->GetVelocityNode();
    const netSyncDataNode *angVelocityNode   = vehicleSyncTree->GetAngVelocityNode();
    const netSyncDataNode *migrationNode     = vehicleSyncTree->GetProximityMigrationNode();

    bool positionUpdated    = vehicleSyncTree->GetPositionWasUpdated();
    bool orientationUpdated = orientationNode ? orientationNode->GetWasUpdated() : false;
    bool velocityUpdated    = velocityNode    ? velocityNode->GetWasUpdated()    : false;
    bool angVelocityUpdated = angVelocityNode ? angVelocityNode->GetWasUpdated() : false;
    bool migrateNodeUpdated = migrationNode   ? migrationNode->GetWasUpdated()   : false;

    positionUpdated |= migrateNodeUpdated;
    velocityUpdated |= migrateNodeUpdated;

	if (IsClone())
	{
		CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());
		if(netBlenderPhysical)
		{
			CVehicleGadgetDataNode &data = *(static_cast<CVehicleSyncTree *>(GetSyncTree())->GetVehicleGadgetNode());

			if(data.GetWasUpdated())
			{
				netBlenderPhysical->SetParentVehicleOffset(data.m_IsAttachedTrailer, data.m_OffsetFromParentVehicle);
			}
		}
	}	

    UpdateBlenderState(positionUpdated, orientationUpdated, velocityUpdated, angVelocityUpdated);	
}

// name:        ManageUpdateLevel
// description: Decides what the default update level should be for this object
void CNetObjVehicle::ManageUpdateLevel(const netPlayer& player)
{
#if __BANK
    if(ms_updateRateOverride >= 0)
    {
        SetUpdateLevel( player.GetPhysicalPlayerIndex(), static_cast<u8>(ms_updateRateOverride) );
        return;
    }
#endif

    CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    if(pVehicle)
    {
        if(pVehicle->m_nVehicleFlags.bIsThisAStationaryCar)
        {
            CTaskVehicleMissionBase *pTask = pVehicle->GetIntelligence()->GetActiveTask();

            if(pTask && pTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_CRUISE_NEW)
            {
                static const unsigned TIME_TO_FORCE_UPDATE = 1000;
                SetLowestAllowedUpdateLevel(CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH, TIME_TO_FORCE_UPDATE);
            }
        }
    }

	CNetObjPhysical::ManageUpdateLevel(player);

	// don't allow the vehicle's update level to drop too low if the vehicle can potentially viewed through a scope
	if (GetUpdateLevel(player.GetPhysicalPlayerIndex()) < CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM)
	{
		bool lookingThroughScope = false;
		bool zoomed = false;

		if (NetworkInterface::IsPlayerUsingScopedWeapon(player, &lookingThroughScope, &zoomed) && lookingThroughScope && zoomed)
		{
			SetUpdateLevel( player.GetPhysicalPlayerIndex(), CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM );
		}
	}
}

void CNetObjVehicle::LogTaskData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits)
{
	if(log)
	{
		CTaskVehicleMissionBase* pTask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(taskType));

		if(pTask)
		{
			u32 byteSizeOfTaskData = (numBits+7)>>3;
			datBitBuffer messageBuffer;
			messageBuffer.SetReadOnlyBits(dataBlock, byteSizeOfTaskData<<3, 0);

			pTask->ReadTaskData(messageBuffer);
			pTask->LogTaskData(*log);

			delete pTask;
		}
	}
}

void CNetObjVehicle::LogGadgetData(netLoggingInterface *log, IVehicleNodeDataAccessor::GadgetData& gadgetData)
{
	if(log)
	{
		CVehicleGadget* pGadget = GetDummyVehicleGadget((eVehicleGadgetType)gadgetData.Type);

		if (pGadget)
		{
			u32 dataSize = GetSizeOfVehicleGadgetData((eVehicleGadgetType)gadgetData.Type);

			if (dataSize > 0)
			{
				u32 byteSizeOfGadgetData = (dataSize+7)>>3;
				datBitBuffer messageBuffer;
				messageBuffer.SetReadOnlyBits(gadgetData.Data, byteSizeOfGadgetData<<3, 0);

				CSyncDataReader serialiser(messageBuffer, log);

				pGadget->Serialise(serialiser);
			}
		}
	}
}

void CNetObjVehicle::SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate)
{
	CNetObjPhysical::SetIsVisible(isVisible, reason, bNetworkUpdate);

	CVehicle* vehicle = GetVehicle();
	VALIDATE_GAME_OBJECT(vehicle);

	if (!vehicle)
		return;

	for(int i = 0; i < vehicle->GetNumberOfVehicleGadgets(); i++)
	{
		vehicle->GetVehicleGadget(i)->SetIsVisible(vehicle, isVisible);
	}
}


void CNetObjVehicle::ForceVeryHighUpdateLevel(u16 timer)
{
	SetLowestAllowedUpdateLevel(CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH, timer);

	u8 highestLevel = GetHighestAllowedUpdateLevel();
	u8 lowestLevel = GetLowestAllowedUpdateLevel();

	if (lowestLevel < highestLevel)
	{
		for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
		{
			const netPlayer *remotePlayer = netInterface::GetPlayerMgr().GetPhysicalPlayerFromIndex(index);

			if(remotePlayer && remotePlayer != GetPlayerOwner() && HasBeenCloned(*remotePlayer))
			{
				SetUpdateLevel(remotePlayer->GetPhysicalPlayerIndex(), highestLevel);				
			}	
		}
	}	
}


void CNetObjVehicle::SetSuperDummyLaunchedIntoAir(u16 timer)
{
	m_LaunchedInAirTimer = timer;
}

bool CNetObjVehicle::IsSuperDummyStillLaunchedIntoAir() const
{
	return m_LaunchedInAirTimer > 0; 
}

//Called on the vehicle that the player was in when he swaps team.
void CNetObjVehicle::PostSetupForRespawnPed(CPed* ped)
{
	gnetAssert(ped);
	if (!ped)
		return;

	CVehicle* vehicle = GetVehicle();
	VALIDATE_GAME_OBJECT(vehicle);

	if (!vehicle)
		return;

	BANK_ONLY( ValidateDriverAndPassengers(); )

	if (!IsClone())
	{
		CTaskVehicleMissionBase* task = NULL;
        eVehiclePrimaryTaskPriorities taskPriority = VEHICLE_TASK_PRIORITY_PRIMARY;

		//The player was driving this vehicle.
		if (vehicle->GetDriver() == ped)
		{
			bool vehicleIsFlying = false;
			if (vehicle->GetIsAircraft())
			{
				Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
				const float ground = WorldProbe::FindGroundZForCoord(TOP_SURFACE, pos.x, pos.y);

				if (pos.z - ground > 2.0f)
				{
					vehicleIsFlying = true;
				}
			}

			if (vehicleIsFlying)
			{
				netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
				log.WriteDataValue("Set Task on Vehicle:", "TASK_VEHICLE_CRASH");

				task = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(CTaskTypes::TASK_VEHICLE_CRASH));
                taskPriority = VEHICLE_TASK_PRIORITY_CRASH;
			}
		}

		//Set a vehicle TASK
		if (task)
		{
			vehicle->GetIntelligence()->GetTaskManager()->AbortTasks();
			vehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, task, taskPriority, true);
		}
	}
}

void CNetObjVehicle::LogTaskMigrationData(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits)
{
	if(log)
	{
		CTaskVehicleMissionBase* pTask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(taskType));

		if(pTask)
		{
			u32 byteSizeOfTaskData = (numBits+7)>>3;
			datBitBuffer messageBuffer;
			messageBuffer.SetReadOnlyBits(dataBlock, byteSizeOfTaskData<<3, 0);

			pTask->ReadTaskMigrationData(messageBuffer);
			pTask->LogTaskMigrationData(*log);

			delete pTask;
		}
	}
}
#if __BANK

static unsigned int gDamageVehicle[6] = {0,0,0,0,0,0};

static void NetworkBank_ApplyDamageSettings()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CVehicle *closestVehicle = playerPed->GetPedIntelligence()->GetClosestVehicleInRange();

        if(closestVehicle)
        {
            closestVehicle->Fix(false,true);
            closestVehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformationsFromNetwork(gDamageVehicle);

			CNetObjVehicle* pNetObjVehicle = static_cast<CNetObjVehicle*>(closestVehicle->GetNetworkObject());
			if (pNetObjVehicle && !pNetObjVehicle->IsClone())
				pNetObjVehicle->DirtyVehicleDamageStatusDataSyncNode();
        }
    }
}

static Vector4 damage(0.0f, 0.0f, 0.0f, 0.0f);
static Vector4 offset(0.0f, 0.0f, 0.0f, 0.0f);

static void NetworkBank_ApplyCurrentDamage()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CVehicle *closestVehicle = playerPed->GetPedIntelligence()->GetClosestVehicleInRange();

        if(closestVehicle)
        {
            Vector4 normalisedDamage = damage;
            normalisedDamage.w       = 0.0f;
            normalisedDamage.Normalize();
            normalisedDamage.w       = damage.w;
            Vector4 normalisedOffset = offset;
            normalisedOffset.Normalize();
            closestVehicle->GetVehicleDamage()->GetDeformation()->ApplyDamageWithOffset(normalisedDamage, normalisedOffset);

			CNetObjVehicle* pNetObjVehicle = static_cast<CNetObjVehicle*>(closestVehicle->GetNetworkObject());
			if (pNetObjVehicle && !pNetObjVehicle->IsClone())
				pNetObjVehicle->DirtyVehicleDamageStatusDataSyncNode();
        }
    }
}

static void NetworkBank_FixCar()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CVehicle *closestVehicle = playerPed->GetPedIntelligence()->GetClosestVehicleInRange();

        if(closestVehicle)
        {
            closestVehicle->Fix(true,true);
        }
    }
}

void CNetObjVehicle::AddDebugVehicleDamage(bkBank *bank)
{
	if(gnetVerifyf(bank, "Unable to find network bank!"))
	{
		bank->AddSlider("Damage front left",	&gDamageVehicle[CVehicleDeformation::FRONT_LEFT_DAMAGE], 0, 3, 1);
		bank->AddSlider("Damage front right",	&gDamageVehicle[CVehicleDeformation::FRONT_RIGHT_DAMAGE], 0, 3, 1);
		bank->AddSlider("Damage middle left",	&gDamageVehicle[CVehicleDeformation::MIDDLE_LEFT_DAMAGE], 0, 3, 1);
		bank->AddSlider("Damage middle right",	&gDamageVehicle[CVehicleDeformation::MIDDLE_RIGHT_DAMAGE], 0, 3, 1);
		bank->AddSlider("Damage rear left",		&gDamageVehicle[CVehicleDeformation::REAR_LEFT_DAMAGE], 0, 3, 1);
		bank->AddSlider("Damage rear right",	&gDamageVehicle[CVehicleDeformation::REAR_RIGHT_DAMAGE], 0, 3, 1);
		bank->AddButton("Apply damage settings to nearest car", datCallback(NetworkBank_ApplyDamageSettings));
		bank->AddSlider("Damage X",&damage.x, -1.0f, 1.0f, 0.1f);
		bank->AddSlider("Damage Y",&damage.y, -1.0f, 1.0f, 0.1f);
		bank->AddSlider("Damage Z",&damage.z, -1.0f, 1.0f, 0.1f);
		bank->AddSlider("Damage Radius",&damage.w, -TWO_PI, TWO_PI, 0.1f);
		bank->AddSlider("Offset X",&offset.x, -1.0f, 1.0f, 0.1f);
		bank->AddSlider("Offset Y",&offset.y, -1.0f, 1.0f, 0.1f);
		bank->AddSlider("Offset Z",&offset.z, -1.0f, 1.0f, 0.1f);
		bank->AddButton("Apply current damage settings", datCallback(NetworkBank_ApplyCurrentDamage));
		bank->AddButton("Fix car", datCallback(NetworkBank_FixCar));
	}
}

void CNetObjVehicle::AddDebugVehiclePopulation(bkBank *bank)
{
	if(gnetVerifyf(bank, "Unable to find network bank!"))
	{
		bank->AddSlider("Visible Creation Dist Max",&CVehiclePopulation::ms_NetworkVisibleCreationDistMax, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Visible Creation Dist Min",&CVehiclePopulation::ms_NetworkVisibleCreationDistMin, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Fallback Visible Creation Dist Max",&CVehiclePopulation::ms_FallbackNetworkVisibleCreationDistMax, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Fallback Visible Creation Dist Min",&CVehiclePopulation::ms_FallbackNetworkVisibleCreationDistMin, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Offscreen Creation Dist Max",&CVehiclePopulation::ms_NetworkOffscreenCreationDistMax, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Offscreen Creation Dist Max",&CVehiclePopulation::ms_NetworkOffscreenCreationDistMin, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Creation Dist Min Override",&CVehiclePopulation::ms_NetworkCreationDistMinOverride, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Max Network Time Vis Fail",&CVehiclePopulation::ms_NetworkMaxTimeVisibilityFail, 0, 100000, 1);
		bank->AddSlider("Clone Rejection Cooldown Delay",&CVehiclePopulation::ms_NetworkCloneRejectionCooldownDelay, 0, 100000, 1);

		bank->AddToggle("Use player prediction",&CVehiclePopulation::ms_NetworkUsePlayerPrediction);
		bank->AddToggle("Show counter for dummy vehicles that are temporarily real.",&CVehiclePopulation::ms_NetworkShowTemporaryRealVehicles);
		bank->AddSlider("Player prediction time",&CVehiclePopulation::ms_NetworkPlayerPredictionTime, 0.0f, 10.0f, 0.25f);
		bank->AddSlider("Validation Success Cooldown timer",&CVehiclePopulation::ms_CloneSpawnSuccessCooldownTimerScale, 0.0f, 5.0f, 0.01f);
		bank->AddSlider("Validation Fail Score",&CVehiclePopulation::ms_CloneSpawnMultiplierFailScore, 0.0f, 1.0f, 0.01f);

		bank->AddSlider("Min Veh Override Min",&CVehiclePopulation::ms_NetworkMinVehiclesToConsiderOverrideMin, 0, 100, 1);
		bank->AddSlider("Min Veh Override Max",&CVehiclePopulation::ms_NetworkMinVehiclesToConsiderOverrideMax, 0, 100, 1);
	}
}
#endif // __BANK

// sync parser creation functions
void CNetObjVehicle::GetVehicleCreateData(CVehicleCreationDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    CBaseModelInfo *modelInfo = pVehicle->GetBaseModelInfo();
    if(gnetVerifyf(modelInfo, "No model info for networked object!"))
    {
        data.m_modelHash = modelInfo->GetHashKey();
    }
    data.m_popType  = (u32)pVehicle->PopTypeGet();
	data.m_randomSeed = pVehicle->GetRandomSeed();

    if (data.m_popType == POPTYPE_MISSION || data.m_popType == POPTYPE_PERMANENT)
    {
        data.m_takeOutOfParkedCarBudget     = pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation;
    }

    data.m_status         = pVehicle->m_nDEflags.nStatus;
    data.m_lastDriverTime = pVehicle->m_LastTimeWeHadADriver;
    data.m_needsToBeHotwired  = pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired;
    data.m_tyresDontBurst     = pVehicle->m_nVehicleFlags.bTyresDontBurst;

	//Gather max health
	float fMaxHealth = pVehicle->GetMaxHealth();
	data.m_maxHealth = (s32) fMaxHealth;

	// round it up if necessary
	if (fMaxHealth - (float) data.m_maxHealth >= 0.5f)
		data.m_maxHealth++;

	if(pVehicle->InheritsFromPlane() && static_cast<CPlane*>(pVehicle)->GetSpecialFlightModeAllowed())
	{
		data.m_usesVerticalFlightMode = static_cast<CPlane*>(pVehicle)->IsInVerticalFlightMode();
	}
	else
	{
		data.m_usesVerticalFlightMode = false;
	}
	m_lastReceivedVerticalFlightModeRatio = data.m_usesVerticalFlightMode ? 1.0f : 0.0f;
}

void CNetObjVehicle::SetVehicleCreateData(const CVehicleCreationDataNode& data)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(data.m_modelHash, &modelId);

    if(AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)))
    {
        Matrix34 tempMat;
        tempMat.Identity();

#if PHINST_VALIDATE_POSITION
        // set the x,y and z components to random numbers to avoid a physics assert
        tempMat.d.x = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
        tempMat.d.y = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
        tempMat.d.z = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
#endif // PHINST_VALIDATE_POSITION

		eEntityOwnedBy ownedBy = (data.m_popType == POPTYPE_MISSION) ? ENTITY_OWNEDBY_SCRIPT :	ENTITY_OWNEDBY_POPULATION;

		// try to reuse a vehicle
		int vehicleReuseIndex = -1;
		vehicleReuseIndex = CVehiclePopulation::FindVehicleToReuse(modelId.GetModelIndex());
		CVehicle* pVehicle = NULL;
		if(vehicleReuseIndex != -1)
		{
			pVehicle = CVehiclePopulation::GetVehicleFromReusePool(vehicleReuseIndex);
			bool bCreateAsInactive = true;	// This is questionable since we pass in false to CVehicleFactory::Create(), but preserves existing behavior for now.
			if(CVehiclePopulation::ReuseVehicleFromVehicleReusePool(pVehicle, tempMat, (ePopType)data.m_popType, ownedBy, false, bCreateAsInactive))
			{
				// Reuse succeeded
				CVehiclePopulation::RemoveVehicleFromReusePool(vehicleReuseIndex);
			}
			else
			{
				// failed
				pVehicle = NULL;
			}
		}

		if(!pVehicle)
			pVehicle = (CVehicle *)CVehicleFactory::GetFactory()->Create(modelId, ownedBy, data.m_popType, &tempMat, false);

		NETWORK_QUITF(pVehicle, "Failed to create a cloned vehicle. Cannot continue.");

        pVehicle->PopTypeSet((ePopType)data.m_popType);

        if (pVehicle->PopTypeIsMission())
            pVehicle->SetupMissionState();

		SetGameObject((void*) pVehicle);
		pVehicle->SetNetworkObject(this);

		pVehicle->SetRandomSeed((u16)data.m_randomSeed);

        pVehicle->m_nDEflags.nStatus                    = data.m_status;
        m_nLastDriverTime                               = data.m_lastDriverTime;
        pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = data.m_needsToBeHotwired;
        pVehicle->m_nVehicleFlags.bTyresDontBurst       = data.m_tyresDontBurst;

		pVehicle->SetMaxHealth((float)data.m_maxHealth);

		if(pVehicle->InheritsFromPlane())
		{
			CPlane* pPlane = SafeCast(CPlane, pVehicle);
			if(pPlane->GetSpecialFlightModeAllowed())
			{
				pPlane->SetVerticalFlightModeRatio(data.m_usesVerticalFlightMode?1.0f:0.0f);
			}
		}

        CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE );

		CPortalTracker::eProbeType probeType = CVehiclePopulation::GetVehicleProbeType(pVehicle);
		pVehicle->GetPortalTracker()->SetProbeType(probeType);
		pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
		pVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));

        bool bScriptVeh = (data.m_popType == POPTYPE_MISSION || data.m_popType == POPTYPE_PERMANENT);

        DEV_ONLY(m_timeCreated = fwTimer::GetSystemTimeInMilliseconds());

        // take out of the parked car budget if specified
        if(bScriptVeh && data.m_takeOutOfParkedCarBudget)
        {
            CVehiclePopulation::UpdateVehCount(pVehicle, CVehiclePopulation::SUB);
            pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation = data.m_takeOutOfParkedCarBudget;
            CVehiclePopulation::UpdateVehCount(pVehicle, CVehiclePopulation::ADD);
        }
     }
}

// sync parser migration functions
void CNetObjVehicle::GetVehicleMigrationData(CVehicleProximityMigrationDataNode& data)
{
    CVehicle *vehicle = GetVehicle();
    gnetAssert(vehicle);

    PlayerFlags clonedState=0, createState=0, removeState=0;
    GetClonedState(clonedState, createState, removeState);

    // vehicle->pDriver->IsNetworkClone() can happen if a player was in the passenger seat when the event was created and then
    // shuffled over to the drivers seat later on
    data.m_maxOccupants = vehicle->GetSeatManager()->GetMaxSeats();

    for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
    {
        CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

        if (occupant && !occupant->IsClone() && occupant->MigratesWithVehicle())
        {
            gnetAssert(occupant->GetPendingPlayerIndex() == GetPendingPlayerIndex());

            data.m_hasOccupant[seatIndex] = true;
            data.m_occupantID[seatIndex]  = occupant->GetObjectID();

            gnetAssert(GetPendingPlayerOwner() && occupant->HasBeenCloned(*GetPendingPlayerOwner()));

            // calculate the cloned state of the driver (each bit represents a peer)
            PlayerFlags occupantClonedState=0, occupantCreateState=0, occupantRemoveState=0;
            occupant->GetClonedState(occupantClonedState, occupantCreateState, occupantRemoveState);

            // driver must have same cloned state as the vehicle
            if (occupantClonedState != clonedState || occupantCreateState != createState || occupantRemoveState != removeState)
            {
                gnetAssertf(0, "%s %s has a different clone state (%d,%d,%d) from vehicle %s (%d,%d,%d)",
                        (occupant->GetPed() == vehicle->GetDriver()) ? "Driver" : "Passenger",
                        occupant->GetLogName(), occupantClonedState, occupantCreateState, occupantRemoveState,
                        vehicle->GetNetworkObject()->GetLogName(), clonedState, createState, removeState);
            }
        }
        else
        {
            data.m_hasOccupant[seatIndex] = false;
        }
    }

	data.m_hasPopType = vehicle->m_nVehicleFlags.bSyncPopTypeOnMigrate;
	data.m_PopType = (u32)vehicle->PopTypeGet();

    data.m_status         = vehicle->m_nDEflags.nStatus;
    data.m_lastDriverTime = vehicle->m_LastTimeWeHadADriver;

    data.m_position = GetPosition();

    GetVelocity(data.m_packedVelocityX,
                data.m_packedVelocityY,
                data.m_packedVelocityZ);

    data.m_isMoving = (data.m_packedVelocityX != 0) || 
               (data.m_packedVelocityY != 0) ||
               (data.m_packedVelocityZ != 0);

    data.m_SpeedMultiplier = vehicle->GetIntelligence()->SpeedMultiplier;
	data.m_hasTaskData     = IsScriptObject();

	// serialise the current vehicle AI task's migration data
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase*>(vehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY));

	if (pTask)
	{
		data.m_taskType = pTask->GetTaskType();

		datBitBuffer messageBuffer;
		messageBuffer.SetReadWriteBits(data.m_taskMigrationData, CVehicleProximityMigrationDataNode::MAX_TASK_MIGRATION_DATA_SIZE+1, 0);

		pTask->WriteTaskMigrationData(messageBuffer);
	
		data.m_taskMigrationDataSize = messageBuffer.GetCursorPos();
		data.m_hasTaskData |= data.m_taskMigrationDataSize != 0;
	}
	else
	{
		/// DAN TEMP - Vehicle should always have tasks but there are some cases where this
		//             isn't currently the case, Ben Lyons is going to fix this.
		data.m_taskType = CTaskTypes::TASK_VEHICLE_NO_DRIVER;
		data.m_taskMigrationDataSize = 0;
	}

	data.m_RespotCounter = vehicle->GetRespotCounter();
}

void CNetObjVehicle::SetVehicleMigrationData(const CVehicleProximityMigrationDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    PlayerFlags clonedState = 0;
    PlayerFlags createState = 0;
    PlayerFlags removeState = 0;
    GetClonedState(clonedState, createState, removeState);

    gnetAssert(pVehicle->GetSeatManager()->GetMaxSeats() == static_cast<s32>(data.m_maxOccupants));

    for(s32 occupantIndex = 0; occupantIndex < pVehicle->GetSeatManager()->GetMaxSeats(); occupantIndex++)
    {
        CPed *localOccupant = pVehicle->GetSeatManager()->GetPedInSeat(occupantIndex);

        if (data.m_hasOccupant[occupantIndex])
        {
            netObject *occupantObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_occupantID[occupantIndex]);

            if (!occupantObj)
            {
                gnetAssertf(0, "%s has a non-existent passenger %d!", GetLogName(), data.m_occupantID[occupantIndex]);
            }
            else
            {
                CPed *remoteOccupant = NetworkUtils::GetPedFromNetworkObject(occupantObj);

                if (!remoteOccupant)
                {
                    gnetAssertf(0, "%s does not have a ped driver (%s)!",  GetLogName(), occupantObj->GetLogName());
                }

                if (remoteOccupant)
                {
                    if (remoteOccupant != localOccupant)
                    {
                        // we didn't know about the passenger, add him
                        if (remoteOccupant->GetUsingRagdoll())
                        {
							nmEntityDebugf(remoteOccupant, "CNetObjVehicle::SetVehicleMigrationData switching to animated");
                            remoteOccupant->SwitchToAnimated();
                        }

                        ((CNetObjPed*)occupantObj)->SetClonePedInVehicle(pVehicle, occupantIndex);
                    }

                    gnetAssert(pVehicle->GetSeatManager()->GetPedInSeat(occupantIndex) == remoteOccupant);
                    gnetAssert(static_cast<CNetObjPed*>(occupantObj)->MigratesWithVehicle());

 					if (occupantObj->GetPlayerOwner() != NetworkInterface::GetLocalPlayer())
					{
						// set pending peer id to be used in ChangeOwner for the vehicle
						occupantObj->SetPendingPlayerIndex(NetworkInterface::GetLocalPhysicalPlayerIndex());
						occupantObj->SetClonedState(clonedState, createState, removeState);
					}
				}
			}
        }
        else if (localOccupant && localOccupant->IsNetworkClone() && static_cast<CNetObjPed*>(localOccupant->GetNetworkObject())->MigratesWithVehicle())
        {
            // we still think there is a ped in this seat, remove him
            ((CNetObjPed*)localOccupant->GetNetworkObject())->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck);

            gnetAssert(!pVehicle->GetSeatManager()->GetPedInSeat(occupantIndex));
        }
    }

    if(pVehicle->m_nDEflags.nStatus != STATUS_WRECKED && data.m_status == STATUS_WRECKED)
    {
		if(m_bTimedExplosionPending || m_bWasBlownUpByTimedExplosion)
		{
			CEntity* pCulprit = NULL;

			// our culprit has potentially left the session
			netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_TimedExplosionCulpritID);
			if(pObject)
			{
				CNetObjPed* pNetObjPed = SafeCast(CNetObjPed, pObject);
				pCulprit = pNetObjPed->GetPed();
			}

			pVehicle->BlowUpCar(pCulprit);
			GetEventScriptNetworkGroup()->Add(CEventNetworkTimedExplosion(pVehicle, pCulprit));
			m_bTimedExplosionPending = false;
		}
		else if(m_bPhoneExplosionPending)
		{
			CEntity* pCulprit = NULL;

			// our culprit has potentially left the session
			netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PhoneExplosionCulpritID);
			if(pObject)
			{
				CNetObjPed* pNetObjPed = SafeCast(CNetObjPed, pObject);
				pCulprit = pNetObjPed->GetPed();
			}

			pVehicle->BlowUpCar(pCulprit);
			m_bPhoneExplosionPending = false;
		}
		else
			pVehicle->BlowUpCar(pVehicle->GetWeaponDamageEntity(), false, false, true);
    }

	if(data.m_hasPopType)
	{
		pVehicle->PopTypeSet((ePopType)data.m_PopType);

		//This pop type needs to propagate to the new owner, if this vehicle migrates again.
		pVehicle->m_nVehicleFlags.bSyncPopTypeOnMigrate = true;
	}

    pVehicle->m_nDEflags.nStatus     = data.m_status;

    if(data.m_lastDriverTime > m_nLastDriverTime)
    {
        m_nLastDriverTime = data.m_lastDriverTime;
    }

    if(data.m_isMoving && !pVehicle->InheritsFromTrain()) //trains are a special case that shouldn't be adjusted in this way on change of ownership - this causes massive popping for trains - lavalley
    {
        SetPosition(data.m_position);
        SetVelocity(data.m_packedVelocityX, data.m_packedVelocityY, data.m_packedVelocityZ);
    }

    pVehicle->GetIntelligence()->SpeedMultiplier = data.m_SpeedMultiplier;

	if (data.m_hasTaskData)
	{
		if(m_pCloneAITask == 0)
		{
			m_pCloneAITask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(data.m_taskType));
		}

		if (data.m_taskMigrationDataSize > 0 && gnetVerify(m_pCloneAITask && data.m_taskType == (u32)m_pCloneAITask->GetTaskType()))
		{
			u32 byteSizeOfTaskData = (data.m_taskMigrationDataSize+7)>>3;
			datBitBuffer messageBuffer;
			messageBuffer.SetReadOnlyBits(data.m_taskMigrationData, byteSizeOfTaskData<<3, 0);

			m_pCloneAITask->ReadTaskMigrationData(messageBuffer);
		}
	}

	bool notDamagedByAnything = pVehicle->m_nPhysicalFlags.bNotDamagedByAnything;
	if(pVehicle->IsBeingRespotted() && data.m_RespotCounter == 0)
	{
		pVehicle->ResetRespotCounter(false);
	}
	else if(data.m_RespotCounter > 0)
	{
		pVehicle->SetRespotCounter(data.m_RespotCounter);
	}
	pVehicle->m_nPhysicalFlags.bNotDamagedByAnything = notDamagedByAnything;
}

// sync parser getter functions
void CNetObjVehicle::GetVehicleAngularVelocity(CVehicleAngVelocityDataNode& data)
{
    CVehicle *vehicle = GetVehicle();

    if(vehicle)
    {
        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode == VDM_SUPERDUMMY)
        {
            data.m_IsSuperDummyAngVel = true;
        }
        else
        {
            CPhysicalAngVelocityDataNode tempNode;
            GetAngularVelocity(tempNode);
            data.m_IsSuperDummyAngVel = false;
            data.m_packedAngVelocityX = tempNode.m_packedAngVelocityX;
            data.m_packedAngVelocityY = tempNode.m_packedAngVelocityY;
            data.m_packedAngVelocityZ = tempNode.m_packedAngVelocityZ;
        }
    }
    else
    {
        data.m_IsSuperDummyAngVel = false;
        data.m_packedAngVelocityX = 0;
        data.m_packedAngVelocityY = 0;
        data.m_packedAngVelocityZ = 0;
    }
}

void CNetObjVehicle::GetVehicleGameState(CVehicleGameStateDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	if (!pVehicle)
		return;

#if NA_RADIO_ENABLED
	data.m_radioStation = 0;
	data.m_radioStationChangedByDriver = false;
	if(pVehicle->GetVehicleAudioEntity())
	{
		if (pVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOff())
		{
			data.m_radioStation = 1;
		}
		else
		{
			const audRadioStation *station = pVehicle->GetVehicleAudioEntity()->GetRadioStation();
			if(station)
			{
				data.m_radioStation = station->GetStationIndex() + 2;
				data.m_radioStationChangedByDriver = g_RadioAudioEntity.MPDriverHasTriggeredRadioChange();
			}
		}
	}
#else
	data.m_radioStation = 0;
	data.m_radioStationChangedByDriver = false;
#endif // NA_RADIO_ENABLED

    data.m_engineOn				                      = pVehicle->m_nVehicleFlags.bEngineOn;
	data.m_engineStarting		                      = pVehicle->m_nVehicleFlags.bEngineStarting;
	data.m_engineSkipEngineStartup					  = pVehicle->m_nVehicleFlags.bSkipEngineStartup;
	data.m_handBrakeOn								  =	pVehicle->m_vehControls.m_handBrake;
    data.m_scriptSetHandbrakeOn                       = pVehicle->m_nVehicleFlags.bScriptSetHandbrake;
    data.m_sirenOn				                      = pVehicle->m_nVehicleFlags.GetIsSirenOn();
    data.m_pretendOccupants		                      = pVehicle->m_nVehicleFlags.bUsingPretendOccupants;
    data.m_useRespotEffect							  = pVehicle->GetFlashRemotelyDuringRespotting();
	data.m_runningRespotTimer	                      = pVehicle->IsBeingRespotted();
    data.m_isDriveable			                      = pVehicle->m_nVehicleFlags.bIsThisADriveableCar;
	data.m_doorLockState		                      = static_cast<u32>(pVehicle->GetCarDoorLocks());
    data.m_doorsBroken			                      = 0;
	data.m_doorsNotAllowedToBeBrokenOff				  = 0; //use the negation here because the default is allowed to be broken off
	data.m_doorsOpen			                      = 0;
	data.m_windowsDown			                      = 0;
	data.m_xenonLightColor							  = pVehicle->GetXenonLightColor();
	data.m_isStationary			                      = pVehicle->m_nVehicleFlags.bIsThisAStationaryCar;
	data.m_isParked									  = pVehicle->m_nVehicleFlags.bIsThisAParkedCar;
	data.m_forceOtherVehsToStop						  = pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle;
    data.m_DontTryToEnterThisVehicleIfLockedForPlayer = pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer;
	data.m_customPathNodeStreamingRadius = pVehicle->GetIntelligence()->GetCustomPathNodeStreamingRadius();
	data.m_moveAwayFromPlayer						  = pVehicle->m_nVehicleFlags.bMoveAwayFromPlayer;
	data.m_noDamageFromExplosionsOwnedByDriver		  = pVehicle->GetNoDamageFromExplosionsOwnedByDriver();
	data.m_flaggedForCleanup						  = pVehicle->m_nVehicleFlags.bReadyForCleanup;

	data.m_influenceWantedLevel						  = pVehicle->m_nVehicleFlags.bInfluenceWantedLevel;
	data.m_hasBeenOwnedByPlayer						  = pVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer;
	data.m_AICanUseExclusiveSeats					  = pVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats;

	data.m_placeOnRoadQueued						  = pVehicle->m_nVehicleFlags.bPlaceOnRoadQueued;
	data.m_planeResistToExplosion					  = pVehicle->m_nVehicleFlags.bPlaneResistToExplosion;
	data.m_mercVeh									  = pVehicle->m_nVehicleFlags.bMercVeh;
	data.m_disableSuperDummy                          = pVehicle->m_nVehicleFlags.bDisableSuperDummy;
	data.m_checkForEnoughRoomToFitPed                 = pVehicle->m_nVehicleFlags.bCheckForEnoughRoomToFitPed;
	data.m_vehicleOccupantsTakeExplosiveDamage		  = pVehicle->GetVehicleOccupantsTakeExplosiveDamage();
	data.m_canEjectPassengersIfLocked				  = pVehicle->GetCanEjectPassengersIfLocked();
	data.m_PlayerLocks								  = m_playerLocks;

	data.m_fullThrottleActive						  = pVehicle->IsFullThrottleActive();
	data.m_fullThrottleEndTime						  = pVehicle->m_iFullThrottleRecoveryTime;
	data.m_driftTyres                                 = pVehicle->m_bDriftTyres;

	if (pVehicle->GetIntelligence())
	{
		data.m_RemoveAggressivelyForCarjackingMission	= pVehicle->GetIntelligence()->m_bRemoveAggressivelyForCarjackingMission;
	}
	else
	{
		data.m_RemoveAggressivelyForCarjackingMission = false;
	}

	//ensure these are reset
	data.m_doorIndividualLockedStateFilter = 0;
	for(int i=0; i < SIZEOF_NUM_DOORS; ++i)
	{
		data.m_doorIndividualLockedState[i] = CARLOCK_NONE;
	}

	//Reset these even on local - because on ownership change they will become important to ProcessRemoteDoors
	m_remoteDoorOpen = 0;

    if( !pVehicle->InheritsFromBike() &&
		!pVehicle->InheritsFromTrain() )
    {
        for (s32 i=0; i<pVehicle->GetNumDoors(); i++)
        {
			if(i >= SIZEOF_NUM_DOORS)
			{
				gnetAssertf(0, "%s has more doors(%d) than we currently sync(%d)", GetLogName(), pVehicle->GetNumDoors(), SIZEOF_NUM_DOORS);
				continue;
			}
            gnetAssert(i<(1<<SIZEOF_NUM_DOORS));

			data.m_doorsOpenRatio[ i ] = 0;
			m_remoteDoorOpenRatios[ i ] = 0;

		    CCarDoor* pDoor = pVehicle->GetDoor(i);
		    if (pDoor)
		    {
			    if (!pDoor->GetIsIntact(pVehicle))
			    {
				    data.m_doorsBroken |= (1<<i);
			    }

				//Use the negation here because the default is allowed to be broken off
				if (!pDoor->GetDoorAllowedToBeBrokenOff())
				{
					data.m_doorsNotAllowedToBeBrokenOff |= (1<<i);
				}

				// Always get the state here if not closed - otherwise we will sent 0 for m_doorsOpenRatio and m_doorsOpen for this door.
				// Then if we don't send this the remote can get into a weird state - lets give the remote the right information here.  lavalley
			    if( !pDoor->GetIsClosed() )
			    {
					// if the door is broken off we don't need to write any bits for it as we can check it is broken off first
					if( !( data.m_doorsBroken & (1<<i) ) )
					{
						u32 doorRatio = (u32)( ( pDoor->GetTargetDoorRatio() * POSSIBLE_DOOR_POSITIONS ) + MAX_DOOR_TOLERANCE );

						if( pDoor->GetFlag( CCarDoor::IS_ARTICULATED ) &&
							!pVehicle->GetIsAnyFixedFlagSet() &&
							( pDoor->GetFlag( CCarDoor::DRIVEN_SWINGING ) ||
							  !pDoor->GetFlag( CCarDoor::DRIVEN_NOT_SPECIAL_MASK ) ) )
						{
							doorRatio = (u32)( ( pDoor->GetDoorRatio() * POSSIBLE_DOOR_POSITIONS ) + MAX_DOOR_TOLERANCE );
						}

						if( doorRatio > POSSIBLE_DOOR_POSITIONS )
						{
							doorRatio = POSSIBLE_DOOR_POSITIONS;
						}

						data.m_doorsOpen |= ( 1 << ( i ) );
						m_remoteDoorOpen |= ( 1 << ( i ) );

						data.m_doorsOpenRatio[ i ] = (u8)( doorRatio );
						m_remoteDoorOpenRatios[ i ] = (u8)( doorRatio );
					}
			    }

				if (i < SIZEOF_NUM_DOORS)
				{
					if (pDoor->m_eDoorLockState < CARLOCK_NUM_STATES)
					{
						data.m_doorIndividualLockedState[i] = (u8) pDoor->m_eDoorLockState;
						if (pDoor->m_eDoorLockState != CARLOCK_NONE)
						{
							data.m_doorIndividualLockedStateFilter |= (1<<i);
						}
					}
				}
			}
		}
    }

	s32 iNumEntryPoints = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints();

	for (s32 iEntryPoint = 0; iEntryPoint < iNumEntryPoints; ++iEntryPoint)
	{
		if (pVehicle->IsEntryIndexValid(iEntryPoint))
		{
			const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPoint);
			if (pEntryPointInfo)
			{
				eHierarchyId window = pEntryPointInfo->GetWindowId();
				//Note: wicked important to pass in true to HasWindowToSmash otherwise as soon as one crack appears it will consider the window gone and report that it should be down.
				//Need to continue to evaluate this as we do not want windows that are smashed to be rolled down/up incorrectly also.
				if (window != VEH_INVALID_ID && (!pVehicle->HasWindowToSmash(window, true) || pVehicle->IsWindowDown(window)))
				{
					data.m_windowsDown |= (1<<iEntryPoint);
				}
			}
		}
	}

    data.m_alarmSet       = (pVehicle->CarAlarmState == CAR_ALARM_SET);
    data.m_alarmActivated = pVehicle->IsAlarmActivated();

	// timed explosions
	data.m_hasTimedExplosion = m_bTimedExplosionPending;
	if(data.m_hasTimedExplosion)
	{
		data.m_timedExplosionTime = m_nTimedExplosionTime;
		data.m_timedExplosionCulprit = m_TimedExplosionCulpritID;
	}
	else
	{
		data.m_timedExplosionTime = 0;
		data.m_timedExplosionCulprit = NETWORK_INVALID_OBJECT_ID;
	}

	data.m_lightsOn	= pVehicle->m_nVehicleFlags.bLightsOn;
	data.m_headlightsFullBeamOn = pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn;
	data.m_overridelights = pVehicle->m_OverrideLights;

	s32 convertibleRoofState = pVehicle->GetConvertibleRoofState();
	data.m_roofLowered = (convertibleRoofState == CTaskVehicleConvertibleRoof::STATE_LOWERED || convertibleRoofState == CTaskVehicleConvertibleRoof::STATE_LOWERING);
	data.m_removeWithEmptyCopOrWreckedVehs = pVehicle->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs;

	for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
	{
		CPed *exclusiveDriver = pVehicle->GetExclusiveDriverPed(i);

		if(!exclusiveDriver || !exclusiveDriver->GetNetworkObject())
		{
			data.m_exclusiveDriverPedID[i] = NETWORK_INVALID_OBJECT_ID;
		}
		else
		{
			data.m_exclusiveDriverPedID[i] = exclusiveDriver->GetNetworkObject()->GetObjectID();
		}
	}

	CPed *lastDriver = pVehicle->GetLastDriver();
	if(!lastDriver || !lastDriver->GetNetworkObject())
	{
		data.m_lastDriverPedID = NETWORK_INVALID_OBJECT_ID;
		data.m_hasLastDriver = false;
	}
	else
	{
		data.m_lastDriverPedID = lastDriver->GetNetworkObject()->GetObjectID();
		data.m_hasLastDriver = true;
	}

	data.m_ghost = m_bGhost;

	data.m_junctionArrivalTime = pVehicle->GetIntelligence()->GetJunctionArrivalTime();
	data.m_junctionCommand = (u8) pVehicle->GetIntelligence()->GetJunctionCommand();

	data.m_OverridingVehHorn = pVehicle->GetVehicleAudioEntity()->IsHornOverriden();
	if(data.m_OverridingVehHorn)
	{
		data.m_OverridenVehHornHash = pVehicle->GetVehicleAudioEntity()->GetOverridenHornHash();
	}

	data.m_UnFreezeWhenCleaningUp = pVehicle->m_nVehicleFlags.bUnfreezeWhenCleaningUp;

	data.m_usePlayerLightSettings = pVehicle->m_usePlayerLightSettings;

	data.m_HeadlightMultiplier = pVehicle->m_HeadlightMultiplier;
		
	data.m_isTrailerAttachmentEnabled = false;
	if (pVehicle->InheritsFromTrailer())
	{
		CTrailer *pTrailer = SafeCast(CTrailer, pVehicle);

		if(pTrailer)
		{
			data.m_isTrailerAttachmentEnabled = pTrailer->GetTrailerAttachmentEnabled();
		}		
	}	

	data.m_detachedTombStone = m_detachedTombStone;

	data.m_ExtraBrokenFlags = 0;
	if(pVehicle->GetModelIndex() == MI_CAR_PBUS2 ||  pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE) || (pVehicle->GetModelNameHash() == MID_COQUETTE4))
	{
		for(int i = 0; i < data.MAX_EXTRA_BROKEN_FLAGS; i++)
		{
			int index = (int)VEH_EXTRA_1 + i;
			if (pVehicle->GetIsExtraOn((eHierarchyId)index))
			{
				int iBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)index);
				int iFragChild = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
				fragInst* pFragInst = pVehicle->GetFragInst();
				if (pFragInst && iFragChild >= 0 && iFragChild < pFragInst->GetTypePhysics()->GetNumChildren())
				{
					int nGroup = pFragInst->GetTypePhysics()->GetAllChildren()[iFragChild]->GetOwnerGroupPointerIndex();
					if (pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(nGroup))
					{
						data.m_ExtraBrokenFlags |= (1 << i);
					}
				}
			}
		}
	}

	data.m_downforceModifierFront = pVehicle->GetDownforceModifierFront();
	data.m_downforceModifierRear = pVehicle->GetDownforceModifierRear();
}

void CNetObjVehicle::GetVehicleScriptGameState(CVehicleScriptGameStateDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    data.m_VehicleFlags.takeLessDamage				                  = pVehicle->m_nVehicleFlags.bTakeLessDamage;
    data.m_VehicleFlags.vehicleCanBeTargetted		                  = pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted;
    data.m_VehicleFlags.partOfConvoy				                  = pVehicle->m_nVehicleFlags.bPartOfConvoy;
    data.m_VehicleFlags.isDrowning					                  = pVehicle->m_nVehicleFlags.bIsDrowning;
    data.m_VehicleFlags.canBeVisiblyDamaged			                  = pVehicle->m_nVehicleFlags.bCanBeVisiblyDamaged;
    data.m_VehicleFlags.freebies					                  = pVehicle->m_nVehicleFlags.bFreebies;
	data.m_VehicleFlags.lockedForNonScriptPlayers	                  = m_lockedForNonScriptPlayers;
    data.m_VehicleFlags.respectLocksWhenHasDriver	                  = pVehicle->m_nVehicleFlags.bRespectLocksWhenHasDriver;
	data.m_VehicleFlags.lockDoorsOnCleanup			                  = pVehicle->m_nVehicleFlags.bLockDoorsOnCleanup;
	data.m_VehicleFlags.shouldFixIfNoCollision		                  = pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision;
	data.m_VehicleFlags.automaticallyAttaches	                      = pVehicle->m_nVehicleFlags.bAutomaticallyAttaches;
	data.m_VehicleFlags.scansWithNonPlayerDriver	                  = pVehicle->m_nVehicleFlags.bScansWithNonPlayerDriver;
    data.m_VehicleFlags.disableExplodeOnContact	                      = pVehicle->m_nVehicleFlags.bDisableExplodeOnContact;
	data.m_VehicleFlags.disableTowing                                 = pVehicle->m_nVehicleFlags.bDisableTowing;
    data.m_VehicleFlags.canEngineDegrade                              = pVehicle->m_nVehicleFlags.bCanEngineDegrade;
    data.m_VehicleFlags.canPlayerAircraftEngineDegrade                = pVehicle->m_nVehicleFlags.bCanPlayerAircraftEngineDegrade;
    data.m_VehicleFlags.nonAmbientVehicleMayBeUsedByGoToPointAnyMeans = pVehicle->m_nVehicleFlags.bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans;
    data.m_VehicleFlags.canBeUsedByFleeingPeds                        = pVehicle->m_nVehicleFlags.bCanBeUsedByFleeingPeds;
    data.m_VehicleFlags.allowNoPassengersLockOn                       = pVehicle->m_nVehicleFlags.bAllowNoPassengersLockOn;
	data.m_VehicleFlags.allowHomingMissleLockOn						  = pVehicle->m_bAllowHomingMissleLockOnSynced;
    data.m_VehicleFlags.disablePetrolTankDamage                       = pVehicle->m_nVehicleFlags.bDisablePetrolTankDamage;
    data.m_VehicleFlags.isStolen                                      = pVehicle->m_nVehicleFlags.bIsStolen;
	data.m_VehicleFlags.explodesOnHighExplosionDamage                 = pVehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage;
	data.m_VehicleFlags.rearDoorsHaveBeenBlownOffByStickybomb	      = pVehicle->m_nVehicleFlags.bRearDoorsHaveBeenBlownOffByStickybomb;
	data.m_VehicleFlags.isInCarModShop								  = pVehicle->GetVehicleAudioEntity()->IsInCarModShop();
	data.m_VehicleFlags.specialEnterExit							  = pVehicle->m_bSpecialEnterExit;
	data.m_VehicleFlags.onlyStartVehicleEngineOnThrottle			  = pVehicle->m_bOnlyStartVehicleEngineOnThrottle;
	data.m_VehicleFlags.dontOpenRearDoorsOnExplosion				  = pVehicle->m_bDontOpenRearDoorsOnExplosion;
	data.m_VehicleFlags.dontProcessVehicleGlass						  = pVehicle->m_bDontProcessVehicleGlass;
	data.m_VehicleFlags.useDesiredZCruiseSpeedForLanding			  = pVehicle->m_bUseDesiredZCruiseSpeedForLanding;
	data.m_VehicleFlags.dontResetTurretHeadingInScriptedCamera		  = pVehicle->m_bDontResetTurretHeadingInScriptedCamera;
	data.m_VehicleFlags.disableWantedConesResponse					  = pVehicle->m_bDisableWantedConesResponse;


	data.m_VehicleProducingSlipstream				= pVehicle->m_VehicleProducingSlipstream;

    data.m_PopType									= (u32)pVehicle->PopTypeGet();
    data.m_TeamLocks								= m_teamLocks;
    data.m_TeamLockOverrides						= m_teamLockOverrides;
	data.m_isinair									= pVehicle->IsInAir(true);
	data.m_IsCarParachuting							= pVehicle->InheritsFromAutomobile() ? SafeCast(CAutomobile, pVehicle)->IsParachuting() : false;
    
	if (data.m_IsCarParachuting)
	{
		SafeCast(CAutomobile, pVehicle)->GetCloneParachuteStickValues(data.m_parachuteStickX, data.m_parachuteStickY);
	}
	
	data.m_GarageInstanceIndex                      = m_GarageInstanceIndex;
	data.m_DamageThreshold							= (int)pVehicle->m_Transmission.GetPlaneDamageThresholdOverride();
	data.m_fScriptDamageScale						= pVehicle->GetVehicleDamage()->GetScriptDamageScale();
	data.m_fScriptWeaponDamageScale					= pVehicle->GetVehicleDamage()->GetScriptWeaponDamageScale();
	data.m_rocketBoostRechargeRate                  = pVehicle->GetRocketBoostRechargeRate();

	data.m_bBlockWeaponSelection					= pVehicle->GetIntelligence()->IsWeaponSelectionBlocked();
	data.m_isBeastVehicle							= pVehicle->GetIsBeastVehicle();
	data.m_fRampImpulseScale						= pVehicle->GetScriptRammingImpulseScale();
	data.m_bBoatIgnoreLandProbes					= pVehicle->GetIntelligence()->m_bSetBoatIgnoreLandProbes;
	data.m_homingCanLockOnToObjects                 = pVehicle->GetHomingCanLockOnToObjects();
	data.m_fOverrideArriveDistForVehPersuitAttack	= pVehicle->GetOverrideArriveDistForVehPersuitAttack();

	data.m_lockedToXY = false;
	data.m_BuoyancyForceMultiplier = 1.0f;

	if (pVehicle->InheritsFromAmphibiousAutomobile())
	{
		CAmphibiousAutomobile* amphibiousVehicle = static_cast<CAmphibiousAutomobile*>(pVehicle);
		if (amphibiousVehicle)
		{
			data.m_lockedToXY              = amphibiousVehicle->GetAnchorHelper().IsAnchored();
			data.m_BuoyancyForceMultiplier = amphibiousVehicle->m_Buoyancy.m_fForceMult;
		}
	}
	else if (pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
		{
			data.m_lockedToXY              = pSeaPlaneExtension->GetAnchorHelper().IsAnchored();
			data.m_BuoyancyForceMultiplier = pPlane->m_Buoyancy.m_fForceMult;
		}
	}

	data.m_disableCollisionUponCreation = m_disableCollisionUponCreation;

	data.m_parachuteObjectId  = NETWORK_INVALID_OBJECT_ID;
	data.m_hasParachuteObject = false;
	if(pVehicle->InheritsFromAutomobile())
	{		
		CAutomobile* automobile = SafeCast(CAutomobile, pVehicle);
		if (automobile)
		{
			data.m_vehicleParachuteTintIndex = automobile->GetParachuteTintIndex();

			if(automobile->GetParachuteObject() && automobile->GetParachuteObject()->GetNetworkObject())
			{
				data.m_hasParachuteObject = true;
				data.m_parachuteObjectId = automobile->GetParachuteObject()->GetNetworkObject()->GetObjectID();
			}
		}
	}

	data.m_disableRampCarImpactDamage = pVehicle->m_disableRampCarImpactDamage;

	for(int i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{	
		data.m_restrictedAmmoCount[i] = pVehicle->GetRestrictedAmmoCount(i);
	}

	if(pVehicle->InheritsFromAmphibiousQuadBike())
	{
		data.m_tuckInWheelsForQuadBike = static_cast<CAmphibiousQuadBike*>(pVehicle)->ShouldTuckWheelsIn();
	}
	else
	{
		data.m_tuckInWheelsForQuadBike = false;
	}

	data.m_canPickupEntitiesThatHavePickupDisabled = false;
    data.m_ExtraBoundAttachAllowance = 0.0f;
	data.m_hasHeliRopeLengthSet = false;

	if(pVehicle->InheritsFromHeli())
	{
		CHeli* pHeli = SafeCast(CHeli, pVehicle);
		if(pHeli && pHeli->GetIsCargobob())
		{
			data.m_canPickupEntitiesThatHavePickupDisabled = pHeli->GetCanPickupEntitiesThatHavePickupDisabled();
			if(!IsClose(pHeli->GetInitialRopeLength(), 0.0f, 0.00001f))
			{
				data.m_hasHeliRopeLengthSet = true;
				data.m_HeliRopeLength = pHeli->GetInitialRopeLength();

				CVehicleGadgetPickUpRope *pickUpRope = GetPickupRopeGadget(pVehicle);

				if(pickUpRope)
				{
					data.m_ExtraBoundAttachAllowance = pickUpRope->GetExtraPickupBoundAllowance();
				}
			}
		}
	}

	data.m_ScriptForceHd = pVehicle->GetScriptForceHd();
	data.m_CanEngineMissFire = pVehicle->m_nVehicleFlags.bCanEngineMissFire;
	data.m_DisableBreaking = pVehicle->GetFragInst() ? pVehicle->GetFragInst()->IsBreakingDisabled() : false;

	data.m_gliderState = (u8) pVehicle->GetGliderState();

	data.m_disablePlayerCanStandOnTop = pVehicle->m_bDisablePlayerCanStandOnTop;

	data.m_BombAmmoCount = pVehicle->GetBombAmmoCount();
	data.m_CountermeasureAmmoCount = pVehicle->GetCountermeasureAmmoCount();

	data.m_ScriptMaxSpeed = pVehicle->GetScriptMaxSpeed();

	data.m_CollisionWithMapDamageScale = pVehicle->GetVehicleDamage()->GetCollisionWithMapDamageScale();

	data.m_InSubmarineMode	= pVehicle->IsInSubmarineMode();
	data.m_TransformInstantly = false;
	if(pVehicle->InheritsFromSubmarineCar())
	{
		CSubmarineCar* submarineCar = SafeCast(CSubmarineCar, pVehicle);
		if(data.m_InSubmarineMode)
		{
			if(submarineCar->GetAnimationState() == CSubmarineCar::State_Finished)
			{
				data.m_TransformInstantly = true;
			}
		}
		else
		{
			if(submarineCar->GetAnimationState() == CSubmarineCar::State_Finished)
			{
				data.m_TransformInstantly = true;
			}
		}
	}

	data.m_AllowSpecialFlightMode = pVehicle->GetSpecialFlightModeAllowed();
	data.m_SpecialFlightModeUsed = IsClose(pVehicle->GetSpecialFlightModeRatio(), 1.0f, SMALL_FLOAT);
	data.m_DisableVericalFlightModeTransition = false;
	if(pVehicle->InheritsFromPlane())
	{
		data.m_DisableVericalFlightModeTransition = SafeCast(CPlane, pVehicle)->GetDisableVerticalFlightModeTransition();
	}
	data.m_HasOutriggerDeployed = pVehicle->GetOutriggerDeployRatio() == 1.0f;

	data.m_UsingAutoPilot = pVehicle->GetIsUsingScriptAutoPilot();

	data.m_DisableHoverModeFlight = pVehicle->GetDisableHoverModeFlight();

	data.m_RadioEnabledByScript = pVehicle->GetVehicleAudioEntity() ? !pVehicle->GetVehicleAudioEntity()->IsRadioDisabledByScript() : true;
	
	data.m_bIncreaseWheelCrushDamage = pVehicle->GetIncreaseWheelCrushDamage();
}

void CNetObjVehicle::GetVehicleHealth(CVehicleHealthDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    data.m_isWrecked = pVehicle->GetStatus() == STATUS_WRECKED;
	data.m_isBlownUp = data.m_isWrecked ? pVehicle->m_nVehicleFlags.bBlownUp : false;

    float engineHealth = pVehicle->m_Transmission.GetEngineHealth();
    data.m_packedEngineHealth = static_cast<s32>(engineHealth);
    float remainder    = engineHealth - static_cast<float>(data.m_packedEngineHealth);

	// check to make sure that the underlying code hasn't moved the engine damage out of range
	if(data.m_packedEngineHealth < ENGINE_DAMAGE_FINSHED)
	{
		data.m_packedEngineHealth = static_cast<s32>(ENGINE_DAMAGE_FINSHED);
		gnetAssertf(0, "Engine Health < ENGINE_DAMAGE_FINSHED - Something changed in Transmission!");
	}

    if(remainder >= 0.5f || (engineHealth > 0.0f && data.m_packedEngineHealth==0))
    {
        data.m_packedEngineHealth++;
    }

    float petrolTankHealth = pVehicle->GetVehicleDamage()->GetPetrolTankHealth();
    data.m_packedPetrolTankHealth = static_cast<s32>(petrolTankHealth);
    remainder = petrolTankHealth - static_cast<float>(data.m_packedPetrolTankHealth);

    if(remainder >= 0.5f || (petrolTankHealth > 0.0f && data.m_packedPetrolTankHealth==0))
    {
        data.m_packedPetrolTankHealth++;
    }

    data.m_numWheels				= pVehicle->GetNumWheels();
    data.m_tyreHealthDefault		= !pVehicle->AreTyresDamaged();
    data.m_suspensionHealthDefault	= !pVehicle->IsSuspensionDamaged();

    if(!data.m_tyreHealthDefault)
    {
        for(u32 index = 0; index < data.m_numWheels; index++)
        {
            CWheel *wheel = pVehicle->GetWheel(index);
            gnetAssert(wheel);
			data.m_tyreWearRate[index]  = wheel->GetTyreWearRate();
			gnetAssertf(data.m_tyreWearRate[index] >= 0.0f && data.m_tyreWearRate[index] <= 1.0f, "%s has invalid tyre wear rate of: %.2f", GetLogName(), data.m_tyreWearRate[index]);
            data.m_tyreDamaged[index]   = (wheel->GetTyreHealth() != TYRE_HEALTH_DEFAULT);
            data.m_tyreDestroyed[index] = (wheel->GetTyreHealth() == 0.0f);
			data.m_tyreBrokenOff[index] = pVehicle->GetWheelBroken(index);
			data.m_tyreFire[index] 		= wheel->IsWheelOnFire();
        }
    }

    if(!data.m_suspensionHealthDefault)
    {
        for(u32 index = 0; index < data.m_numWheels; index++)
        {
            CWheel *wheel = pVehicle->GetWheel(index);
            gnetAssert(wheel);
            data.m_suspensionHealth[index] = wheel->GetSuspensionHealth();
        }
    }

	// calculate the packed health value
	float health = pVehicle->GetHealth();
	if (health < 0.0f)
	{
		data.m_health = 0;
	}
	else if (health > 0.0f && health < 1.0f)
	{
		// object is just alive, so its health must be above 0
		data.m_health = 1;
	}
	else
	{
		data.m_health = (s32) health;

		// round it up if necessary
		if (health - (float) data.m_health >= 0.5f)
			data.m_health++;
	}

	data.m_hasMaxHealth = (data.m_health == pVehicle->GetMaxHealth());

	// calculate the packed body health value
	float bodyHealth = pVehicle->GetVehicleDamage()->GetBodyHealth();
	if (bodyHealth < 0.0f)
	{
		data.m_bodyhealth = 0;
	}
	else if (bodyHealth > 0.0f && bodyHealth < 1.0f)
	{
		// object is just alive, so its health must be above 0
		data.m_bodyhealth = 1;
	}
	else
	{
		data.m_bodyhealth = (s32) bodyHealth;

		// round it up if necessary
		if (bodyHealth - (float) data.m_bodyhealth >= 0.5f)
			data.m_bodyhealth++;
	}

	data.m_healthsame = (data.m_health == data.m_bodyhealth) ? true : false;

	// calculate the weapon damage entity ID
	data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
	data.m_weaponDamageHash	  = 0;

	if (!data.m_hasMaxHealth || !data.m_suspensionHealthDefault || !data.m_tyreHealthDefault || data.m_packedEngineHealth != ENGINE_HEALTH_MAX || data.m_packedPetrolTankHealth != VEH_DAMAGE_HEALTH_STD)
	{
		netObject* networkObject = NetworkUtils::GetNetworkObjectFromEntity(pVehicle->GetWeaponDamageEntity());
		if (networkObject)
		{
			data.m_weaponDamageEntity = networkObject->GetObjectID();
			data.m_weaponDamageHash   = pVehicle->GetWeaponDamageHash();
		}
	}

	data.m_extinguishedFireCount = m_extinguishedFireCount;
	data.m_fixedCount = m_fixedCount;

	data.m_lastDamagedMaterialId = (u64)GetLastDamagedMaterialId();
}

void CNetObjVehicle::GetSteeringAngle(CVehicleSteeringDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    data.m_steeringAngle = Clamp(pVehicle->m_vehControls.m_steerAngle, -1.0f, 1.0f);
}

void CNetObjVehicle::GetVehicleControlData(CVehicleControlDataNode& data)
{
    CVehicle *vehicle = GetVehicle();
    gnetAssert(vehicle);

    data.m_bNitrousActive = vehicle->GetNitrousActive();
	data.m_bIsNitrousOverrideActive = vehicle->GetOverrideNitrousLevel();	

	data.m_isInBurnout = false;
	if(vehicle->InheritsFromAutomobile())
	{
		 data.m_isInBurnout = SafeCast(CAutomobile, vehicle)->IsInBurnout();
	}
	
	data.m_isSubCar = false;
	if (vehicle->InheritsFromSubmarineCar())
	{
		data.m_isSubCar = true;
		CSubmarineCar* pSubCar = static_cast<CSubmarineCar*>(vehicle);

		CSubmarineHandling* subHandling = pSubCar->GetSubHandling();
		gnetAssert(subHandling);

		data.m_subCarYaw   = subHandling->GetYawControl();
		data.m_SubCarPitch = subHandling->GetPitchControl();
		data.m_SubCarDive  = subHandling->GetDiveControl();

		data.m_subCarYaw   = Clamp(data.m_subCarYaw, -1.0f, 1.0f);
		data.m_SubCarPitch = Clamp(data.m_SubCarPitch, -1.0f, 1.0f);
		data.m_SubCarDive  = Clamp(data.m_SubCarDive, -1.0f, 1.0f);
	}	


    data.m_throttle		= vehicle->m_vehControls.m_throttle;
    data.m_brakePedal	= vehicle->m_vehControls.m_brake;

	data.m_kersActive   = vehicle->GetKERSActive();
	data.m_numWheels    = vehicle->GetNumWheels();
	data.m_bModifiedLowriderSuspension = vehicle->m_nVehicleFlags.bPlayerModifiedHydraulics || vehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED);
	data.m_bAllLowriderHydraulicsRaised = vehicle->m_nVehicleFlags.bAreHydraulicsAllRaised;	
	
	// Set lowrider suspension values.
	if ((data.m_bModifiedLowriderSuspension || data.m_bAllLowriderHydraulicsRaised))
	{
		for(int i = 0; i < vehicle->GetNumWheels(); i++)
		{
			CWheel *pWheel = vehicle->GetWheel(i);
            if (pWheel  && gnetVerifyf(i < NUM_VEH_CWHEELS_MAX, "Vehicle has more than the expected number of wheels!"))
			{
				data.m_fLowriderSuspension[i] = pWheel->GetSuspensionRaiseAmount();
			}
		}
	}

	if(vehicle->InheritsFromAutomobile())
	{
		data.m_reducedSuspensionForce = SafeCast(CAutomobile, vehicle)->GetReducedSuspensionForce();
	}
	else
	{
		data.m_reducedSuspensionForce = false;
	}

	data.m_bPlayHydraulicsBounceSound = m_bHasHydraulicBouncingSound;
	data.m_bPlayHydraulicsActivationSound = m_bHasHydraulicActivationSound;
	data.m_bPlayHydraulicsDeactivationSound = m_bHasHydraulicDeactivationSound;
	m_bHasHydraulicBouncingSound = false;
	m_bHasHydraulicActivationSound = false;
	m_bHasHydraulicDeactivationSound = false;
	
	data.m_bIsClosingAnyDoor = vehicle->GetRemoteDriverDoorClosing();

	data.m_roadNodeAddress = 0;

    CVehicleNodeList *nodeList = vehicle->GetIntelligence()->GetNodeList();

	if(nodeList)
	{
		s32 iTargetNode = nodeList->GetTargetNodeIndex();
		nodeList->GetPathNodeAddr(iTargetNode).ToInt(data.m_roadNodeAddress);
	}

	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase*>(vehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_SECONDARY));

	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_BRING_VEHICLE_TO_HALT)
	{
		data.m_bringVehicleToHalt = true;

		CTaskBringVehicleToHalt* pHaltTask = static_cast<CTaskBringVehicleToHalt*>(pTask);

		data.m_BVTHStoppingDist = pHaltTask->GetStoppingDistance();
		data.m_BVTHControlVertVel = pHaltTask->GetControlVerticalVelocity();
	}
	else
	{
		data.m_bringVehicleToHalt = false;
	}

	if(vehicle->GetTopSpeedPercentage() != -1.0f)
	{
		data.m_HasTopSpeedPercentage = true;
		data.m_topSpeedPercent = vehicle->GetTopSpeedPercentage();
	}
	else
	{
		data.m_HasTopSpeedPercentage = false;
	}

	float targetGravityScale = vehicle->GetTargetGravityScale();	
	if (targetGravityScale != 0.0f)
	{
		data.m_HasTargetGravityScale = true;
		data.m_TargetGravityScale    = targetGravityScale;
		data.m_StickY                = vehicle->GetCachedStickY();
	}	
	else
	{
		data.m_HasTargetGravityScale = false;
	}
}

void CNetObjVehicle::GetVehicleAppearanceData(CVehicleAppearanceDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	if (!pVehicle)
		return;

    // check for and remove any invalid extras
    data.m_disableExtras = pVehicle->m_nDisableExtras;
	for(int i = VEH_EXTRA_1; i <= VEH_LAST_EXTRA; i++)
	{
		u32 nExtraBit = BIT(i - VEH_EXTRA_1 + 1);
		if(data.m_disableExtras & nExtraBit)
		{
			eHierarchyId nExtraID = static_cast<eHierarchyId>(i);
			if(!gnetVerifyf(pVehicle->HasComponent(nExtraID), "Invalid vehicle component! nExtraID[%d] pVehicle[%p][%s][%s]",nExtraID,pVehicle,pVehicle->GetModelName(),GetLogName()))
				data.m_disableExtras &= ~nExtraBit;
		}
	}

	data.m_hasLiveryID = false;
	data.m_liveryID = 0;
	s32 liveryID = pVehicle->GetLiveryId();
	if (liveryID > -1)
	{
		data.m_hasLiveryID = true;
	    data.m_liveryID	= liveryID;
	}

	data.m_hasLivery2ID = false;
	data.m_livery2ID = 0;
	s32 livery2ID = pVehicle->GetLivery2Id();
	if (livery2ID > -1)
	{
		data.m_hasLivery2ID = true;
	    data.m_livery2ID	= livery2ID;
	}

    data.m_bodyColour1	= pVehicle->GetBodyColour1();
    data.m_bodyColour2	= pVehicle->GetBodyColour2();
    data.m_bodyColour3	= pVehicle->GetBodyColour3();
    data.m_bodyColour4	= pVehicle->GetBodyColour4();
    data.m_bodyColour5	= pVehicle->GetBodyColour5();
    data.m_bodyColour6	= pVehicle->GetBodyColour6();
	data.m_bodyDirtLevel = pVehicle->GetBodyDirtLevelUInt8();

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	if (pShader && pShader->GetIsPrimaryColorCustom())
	{
		Color32 col = pShader->GetCustomPrimaryColor();
		data.m_customPrimaryColor = true;
		data.m_customPrimaryR = col.GetRed();
		data.m_customPrimaryG = col.GetGreen();
		data.m_customPrimaryB = col.GetBlue();
	}
	else
	{
		data.m_customPrimaryColor = false;
	}

	if (pShader && pShader->GetIsSecondaryColorCustom())
	{
		Color32 col = pShader->GetCustomSecondaryColor();
		data.m_customSecondaryColor = true;
		data.m_customSecondaryR = col.GetRed();
		data.m_customSecondaryG = col.GetGreen();
		data.m_customSecondaryB = col.GetBlue();
	}
	else
	{
		data.m_customSecondaryColor = false;
	}

	data.m_envEffScale = pShader ? pShader->GetEnvEffScaleU8() : 0;

	data.m_windowTint = pVehicle->GetVariationInstance().GetWindowTint();

	data.m_smokeColorR = pVehicle->GetVariationInstance().GetSmokeColorR();
	data.m_smokeColorG = pVehicle->GetVariationInstance().GetSmokeColorG();
	data.m_smokeColorB = pVehicle->GetVariationInstance().GetSmokeColorB();

	// find kit index
	data.m_kitIndex = pVehicle->GetVariationInstance().GetKitIndex();
	if(data.m_kitIndex == INVALID_VEHICLE_KIT_INDEX)
	{
		// send invalid as 0
		data.m_kitIndex = 0;
	}
	else
	{
		int nKits = pVehicle->GetVehicleModelInfo()->GetNumModKits();
		for(u16 i = 0; i < nKits; i++)
		{
			if(pVehicle->GetVehicleModelInfo()->GetModKit(i) == pVehicle->GetVariationInstance().GetKitIndex())
			{
				// 0 represents invalid, so bump value by 1
				data.m_kitIndex = i + 1;
				break;
			}
		}
	}

	// flatten
	for(unsigned i = 0; i < NUM_KIT_MOD_SYNC_VARIABLES; i++)
	{
		data.m_allKitMods[i] = 0;
	}
	
	data.m_toggleMods = 0;
	data.m_wheelMod = 0;
	data.m_wheelType = 0;

	const u8* pMods = pVehicle->GetVariationInstance().GetMods();
	const CVehicleKit* pKit = pVehicle->GetVariationInstance().GetKit();
	if(pKit)
	{
		// verify that we haven't blown the limit
		int nVisibleMods = pKit->GetVisibleMods().GetCount();
		int nStatsMods = pKit->GetStatMods().GetCount();
#if __ASSERT
		gnetAssertf(nVisibleMods + nStatsMods < SIZEOF_KIT_MODS, "Need to increase the available space for kit mods! vehicle[%s] (nVisibleMods[%d] + nStatMods[%d])[%d] !< SIZEOF_KIT_MODS[%d]",pVehicle->GetModelName(),nVisibleMods,nStatsMods,nVisibleMods+nStatsMods,SIZEOF_KIT_MODS);
#else
		if (nVisibleMods + nStatsMods >= SIZEOF_KIT_MODS)
		{
			Warningf("Need to increase the available space for kit mods! vehicle[%s] (nVisibleMods[%d] + nStatMods[%d])[%d] >= SIZEOF_KIT_MODS[%d]",pVehicle->GetModelName(),nVisibleMods,nStatsMods,nVisibleMods+nStatsMods,SIZEOF_KIT_MODS);
		}
#endif
		
		// build kit mods bitfield
		for(int i = 0; i < VMT_TOGGLE_MODS; i++)
		{
			if(pMods[i] != INVALID_MOD)
			{
				// bit offset
				u32 nOffset = (i < VMT_RENDERABLE) ? 0 : nVisibleMods;

				// write in index + offset
				u32 nShift = (pMods[i] + nOffset);
				
				unsigned index = (unsigned)(nShift / SIZEOF_KIT_MODS_U32);
				unsigned mult = index * SIZEOF_KIT_MODS_U32;

				if(gnetVerifyf(index < NUM_KIT_MOD_SYNC_VARIABLES, "Index is larger than array. Array size might need to be increased. Index: %d", index))
				{
					data.m_allKitMods[index] |= (1 << (nShift - mult));
				}
#if __ASSERT
				if(nShift > (SIZEOF_KIT_MODS))
				{
					gnetAssertf(0,"CNetObjVehicle::GetVehicleAppearanceData trying to bitshift beyond maxbits--SIZEOF_KIT_MODS[%d] - notify network programmers i[%d] pMods[i][%d] nOffset[%d] nShift[%d]",SIZEOF_KIT_MODS,i,pMods[i],nOffset,nShift);
				}
#else
				if(nShift > (SIZEOF_KIT_MODS))
				{
					Warningf("CNetObjVehicle::GetVehicleAppearanceData trying to bitshift beyond maxbits--SIZEOF_KIT_MODS[%d] - notify network programmers i[%d] pMods[i][%d] nOffset[%d] nShift[%d]",SIZEOF_KIT_MODS,i,pMods[i],nOffset,nShift);					
				}
#endif
			}
		}

		// build toggle mods bitfield
		int nToggleOffset = VMT_TOGGLE_MODS; 
		for(int i = 0; i < VMT_TOGGLE_NUM; i++)
		{
			if(pMods[i+nToggleOffset] != INVALID_MOD)
			{
				data.m_toggleMods |= (1 << i);
			}
		}

		// wheel type
		data.m_wheelType = (u8) pVehicle->GetVariationInstance().GetWheelType(pVehicle);

		// wheel mod
		data.m_wheelMod = pMods[VMT_WHEELS];

		const u32 modelNameHash = pVehicle->GetModelNameHash();
		
		// rear wheel mod (if it's a bike)
		if (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))
		{
			data.m_hasDifferentRearWheel = true;
			data.m_rearWheelMod = pMods[VMT_WHEELS_REAR_OR_HYDRAULICS];

			// offset by 1 (+= 1 would do the trick in both scenarios here but better to be explicit)
			if( (data.m_rearWheelMod == INVALID_MOD) || (data.m_rearWheelMod >= CVehicleModelInfo::GetVehicleColours()->m_Wheels[data.m_wheelType].GetCount()) )
				data.m_rearWheelMod = 0;
			else
				data.m_rearWheelMod += 1;
		}
		else
		{
			data.m_hasDifferentRearWheel = false;
		}

		// offset by 1 (+= 1 would do the trick in both scenarios here but better to be explicit)
		if( (data.m_wheelMod == INVALID_MOD) || (data.m_wheelMod >= CVehicleModelInfo::GetVehicleColours()->m_Wheels[data.m_wheelType].GetCount()) )
			data.m_wheelMod = 0;
		else
			data.m_wheelMod += 1;

		vehicleAssertf(data.m_wheelMod <= CVehicleAppearanceDataNode::MAX_WHEEL_MOD_INDEX, "data.m_wheelMod[%d] > MAX_WHEEL_MOD_INDEX[%d]",data.m_wheelMod,CVehicleAppearanceDataNode::MAX_WHEEL_MOD_INDEX);
	
		data.m_wheelVariation0 = pVehicle->GetVariationInstance().HasVariation(0);
		data.m_wheelVariation1 = pVehicle->GetVariationInstance().HasVariation(1);
	}

	CCustomShaderEffectVehicle *pShaderEffectVehicle = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	if(pShaderEffectVehicle)
	{
		memcpy(data.m_licencePlate, pShaderEffectVehicle->GetLicensePlateText(), SIZEOF_LICENCE_PLATE);
		data.m_LicencePlateTexIndex = pShaderEffectVehicle->GetLicensePlateTexIndex();
	}
	else
	{
		memset(data.m_licencePlate, 0x00, sizeof(data.m_licencePlate));
		data.m_LicencePlateTexIndex = -1;
	}

	data.m_horntype = pVehicle->GetVehicleAudioEntity() ? pVehicle->GetVehicleAudioEntity()->GetHornType() : 0;
	
	data.m_VehicleBadge	= m_VehicleBadgeDesc.IsValid();
 	if (data.m_VehicleBadge)
 	{
		data.m_VehicleBadgeDesc = m_VehicleBadgeDesc;
		for(int i = 0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
		{
			data.m_bVehicleBadgeData[i] = false;
			if (m_pVehicleBadgeData[i])
			{
				data.m_bVehicleBadgeData[i] = true;
				data.m_VehicleBadgeData[i] = *(m_pVehicleBadgeData[i]);
			}
		}
 	}

	//@begin neon only used in NG
	data.m_neonLOn = pVehicle->IsNeonLOn();
	data.m_neonROn = pVehicle->IsNeonROn();
	data.m_neonFOn = pVehicle->IsNeonFOn();
	data.m_neonBOn = pVehicle->IsNeonBOn();

	data.m_neonOn = (data.m_neonLOn || data.m_neonROn || data.m_neonFOn || data.m_neonBOn);
	if (data.m_neonOn)
	{
		Color32 col = pVehicle->GetNeonColour();
		data.m_neonColorR = col.GetRed();
		data.m_neonColorG = col.GetGreen();
		data.m_neonColorB = col.GetBlue();
	}

	data.m_neonSuppressed = pVehicle->GetNeonSuppressionState();
	//@end neon
}

void CNetObjVehicle::DirtyVehicleDamageStatusDataSyncNode()
{
	CVehicleSyncTree *vehicleSyncTree = SafeCast(CVehicleSyncTree, GetSyncTree());
	if(vehicleSyncTree && vehicleSyncTree->GetVehicleDamageStatusNode())
	{
		vehicleSyncTree->DirtyNode(this, *vehicleSyncTree->GetVehicleDamageStatusNode());
	}
}

void CNetObjVehicle::GetVehicleDamageStatusData(CVehicleDamageStatusDataNode& data)
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	if (!pVehicle)
		return;

	data.m_frontLeftDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::FRONT_LEFT_DAMAGE) & 0xff);
	data.m_frontRightDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::FRONT_RIGHT_DAMAGE) & 0xff);
	data.m_middleLeftDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::MIDDLE_LEFT_DAMAGE) & 0xff);
	data.m_middleRightDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::MIDDLE_RIGHT_DAMAGE) & 0xff);
	data.m_rearLeftDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::REAR_LEFT_DAMAGE) & 0xff);
	data.m_rearRightDamageLevel = (u8)(pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(CVehicleDeformation::REAR_RIGHT_DAMAGE) & 0xff);

	data.m_hasDeformationDamage = ( data.m_frontLeftDamageLevel || data.m_frontRightDamageLevel || 
									data.m_middleLeftDamageLevel || data.m_middleRightDamageLevel ||
									data.m_rearLeftDamageLevel || data.m_rearRightDamageLevel );

	GetWeaponImpactPointInformation(data);

	data.m_frontBumperState  = m_FrontBumperDamageState;
	data.m_rearBumperState   = m_RearBumperDamageState;
	data.m_hasBrokenBouncing = data.m_frontBumperState || data.m_rearBumperState;

	for(u32 light = 0; light < SIZEOF_LIGHT_DAMAGE; light++)
	{
		data.m_lightsBroken[light] = pVehicle->GetVehicleDamage()->GetLightStateImmediate(light);
	}

	data.m_hasArmouredGlass = pVehicle->HasBulletResistantGlass();

	//Need to get the information about window broken state, m_WindowsBroken just comes from scripted incoming trigger
	for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
	{
		bool bBroken = (m_WindowsBroken & (1<<window)) != 0;
		data.m_armouredWindowsHealth[window] = CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH; // Set the value to max as a default

		eHierarchyId hId = (eHierarchyId)(VEH_WINDSCREEN + window);
		
		int boneID = pVehicle->GetBoneIndex(hId);
		if( boneID != -1 )
		{
			const int component = pVehicle->GetFragInst() ? pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneID) : -1;
			if (component != -1)
			{
				bBroken = pVehicle->IsBrokenFlagSet(component);
				if (data.m_hasArmouredGlass)
				{
					const CVehicleGlassComponent *pGlassComponent = g_vehicleGlassMan.GetVehicleGlassComponent((CPhysical*)pVehicle, component);
					if (pGlassComponent)
					{					
						data.m_armouredWindowsHealth[window] = pGlassComponent->GetArmouredGlassHealth();
						if (data.m_armouredWindowsHealth[window] != CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH)
						{
							data.m_armouredPenetrationDecalsCount[window] = pGlassComponent->GetArmouredGlassPenetrationDecalsCount();
						}
						else
						{
							data.m_armouredPenetrationDecalsCount[window] = 0;
						}						
					}
					// If there is no glass component, that means that it hasn't been initialized, which means it is at MAX health OR it's been blown up!
					else
					{
						bool bCanComponentBreak = CVehicleGlassManager::CanComponentBreak(pVehicle, component) && !pVehicle->IsBrokenFlagSet(component);

						// if it can't break, it means it's been blown up by explosions.
						if (!bCanComponentBreak)
						{
							data.m_armouredWindowsHealth[window] = 0;							
						}
						else
						{
							data.m_armouredWindowsHealth[window] = CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH;
						}
						data.m_armouredPenetrationDecalsCount[window] = 0;
					}
				}				
			}
		}
		SetWindowState(hId,bBroken);
	}

	for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
	{
		data.m_windowsBroken[window] = (m_WindowsBroken & (1<<window)) != 0;

		eHierarchyId windowId = (eHierarchyId)(VEH_WINDSCREEN + window);

		// front & rear windscreens are flagged as smashed when they only have bullet holes applied
		if ((windowId==VEH_WINDSCREEN && !m_frontWindscreenSmashed) ||
			(windowId==VEH_WINDSCREEN_R && !m_rearWindscreenSmashed))
		{
			data.m_windowsBroken[window] = false;
		}
	}

	bool bUsesSiren = pVehicle->UsesSiren();
	for(u32 siren = 0; siren < SIZEOF_SIREN_DAMAGE; siren++)
	{
		data.m_sirensBroken[siren] = bUsesSiren ? pVehicle->GetVehicleDamage()->GetSirenState(VEH_SIREN_1 + siren) : false;
	}
}

void CNetObjVehicle::GetVehicleAITask(CVehicleTaskDataNode& data)
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase*>(pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY));

	bool bSynced = false;

	if (pTask)
	{
		gnetAssertf(pTask->IsSyncedAcrossNetwork() || pTask->IsSyncedAcrossNetworkScript() || pTask->IsIgnoredByNetwork(), "Vehicle %s has a priority task %s that is not flagged to be synced across the network", GetLogName(), pTask->GetTaskName());
		gnetAssertf(!(pTask->IsSyncedAcrossNetwork() && pTask->IsIgnoredByNetwork()), "Task %s is flagged to be synced and ignored across the network", pTask->GetTaskName());
	
		bSynced = (pTask->IsSyncedAcrossNetwork() || (IsScriptObject() && pTask->IsSyncedAcrossNetworkScript()));
	}

	if (bSynced)
	{
		data.m_taskType = pTask->GetTaskType();

		datBitBuffer messageBuffer;
		messageBuffer.SetReadWriteBits(data.m_taskData, CVehicleTaskDataNode::MAX_VEHICLE_TASK_DATA_SIZE, 0);

		pTask->WriteTaskData(messageBuffer);

		data.m_taskDataSize = messageBuffer.GetCursorPos();
	}
	else
	{
		/// DAN TEMP - Vehicle should always have tasks but there are some cases where this
		//             isn't currently the case, Ben Lyons is going to fix this.
		data.m_taskType = CTaskTypes::TASK_VEHICLE_NO_DRIVER;
		data.m_taskDataSize = 0;
	}
}

void CNetObjVehicle::GetComponentReservations(CVehicleComponentReservationDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	CComponentReservationManager* pCRM = pVehicle->GetComponentReservationMgr();
    data.m_numVehicleComponents = pCRM->GetNumReserveComponents();

	data.m_hasReservations = false;

    for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
    {
        CComponentReservation *componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);

        if(AssertVerify(componentReservation))
        {
            CPed *ped = componentReservation->GetPedUsingComponent();
            data.m_componentReservations[componentIndex] = NetworkUtils::GetObjectIDFromGameObject(ped);

			if (data.m_componentReservations[componentIndex] != NETWORK_INVALID_OBJECT_ID)
			{
				data.m_hasReservations = true;
			}
        }
    }
}

void CNetObjVehicle::GetVehicleGadgetData(CVehicleGadgetDataNode& data)
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	data.m_NumGadgets = 0;

	if (pVehicle)
	{
        if(pVehicle->InheritsFromTrailer() && pVehicle->GetAttachParent())
        {
            data.m_IsAttachedTrailer = true;
            Vector3 trailerPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
            Vector3 parentPos  = VEC3V_TO_VECTOR3(pVehicle->GetAttachParent()->GetTransform().GetPosition());
            data.m_OffsetFromParentVehicle = trailerPos - parentPos;
        }
        else
        {
            data.m_IsAttachedTrailer = false;
            data.m_OffsetFromParentVehicle = VEC3_ZERO;
        }

		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
			
		u32 numWeapons = pWeaponMgr ? pWeaponMgr->GetNumVehicleWeapons() : 0;

		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets() + numWeapons; i++)
		{
			CVehicleGadget *pVehicleGadget = NULL;
			
			if (i < pVehicle->GetNumberOfVehicleGadgets())
			{
				pVehicleGadget = pVehicle->GetVehicleGadget(i);
			}
			else
			{
				pVehicleGadget = pWeaponMgr->GetVehicleWeapon(i-pVehicle->GetNumberOfVehicleGadgets());
			}

			if (pVehicleGadget->IsNetworked())
			{
				if (gnetVerifyf(data.m_NumGadgets < CVehicleGadgetDataNode::MAX_NUM_GADGETS, "Num networked gadgets exceeded maximum for %s", GetLogName()))
				{
					data.m_GadgetData[data.m_NumGadgets].Type = pVehicleGadget->GetType();

					u32 dataSize = GetSizeOfVehicleGadgetData((eVehicleGadgetType)pVehicleGadget->GetType());

					if (dataSize > 0)
					{
						datBitBuffer messageBuffer;
						messageBuffer.SetReadWriteBits(data.m_GadgetData[data.m_NumGadgets].Data, IVehicleNodeDataAccessor::MAX_VEHICLE_GADGET_DATA_SIZE, 0);

						CSyncDataWriter serialiser(messageBuffer);
						pVehicleGadget->Serialise(serialiser);

						gnetAssertf(messageBuffer.GetCursorPos() <= dataSize, "Vehicle gadget %d has serialised data (%d) that exceeds the maximum size allowed", messageBuffer.GetCursorPos(), GetSizeOfVehicleGadgetData((eVehicleGadgetType)pVehicleGadget->GetType()));
					}

					data.m_NumGadgets++;
				}
			}
		}
	}
}

// sync parser setter functions
void CNetObjVehicle::SetVehicleAngularVelocity(CVehicleAngVelocityDataNode& data)
{
    CPhysicalAngVelocityDataNode tempNode;
    tempNode.m_packedAngVelocityX = data.m_packedAngVelocityX;
    tempNode.m_packedAngVelocityY = data.m_packedAngVelocityY;
    tempNode.m_packedAngVelocityZ = data.m_packedAngVelocityZ;
    SetAngularVelocity(tempNode);

    // set target state on blender, which object will be blended to from current position
    CNetBlenderVehicle *vehicleBlender = static_cast<CNetBlenderVehicle *>(GetNetBlender());

    if(vehicleBlender)
    {
        vehicleBlender->SetAllowAngularVelocityUpdate(!data.m_IsSuperDummyAngVel);
    }
}

void CNetObjVehicle::SetVehicleGameState(const CVehicleGameStateDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	if (!pVehicle)
		return;

#if NA_RADIO_ENABLED
    if(pVehicle->GetVehicleAudioEntity())
    {
        u32 adjustedRadioStation = data.m_radioStation;

        // we need to handle the NULL and OFF radio station values (these are currently 254 & 255 which is outside of our synced range)
        // we do this by using 0 and 1 as the values synced and moving all other stations along by two
        switch(data.m_radioStation)
        {
        case 0:
            adjustedRadioStation = g_NullRadioStation;
            break;
        case 1:
            adjustedRadioStation = g_OffRadioStation;
            break;
        default:
            adjustedRadioStation -= 2;
            break;
        }

		pVehicle->GetVehicleAudioEntity()->SetRadioStationFromNetwork(static_cast<u8>(adjustedRadioStation), data.m_radioStationChangedByDriver);
    }
#endif

	pVehicle->m_nVehicleFlags.bEngineStarting		 = false;

	if (data.m_engineStarting && !pVehicle->m_nVehicleFlags.bEngineOn)
		pVehicle->SwitchEngineOn(false,false);

	pVehicle->UpdateUserEmissiveMultiplier(data.m_engineOn, pVehicle->m_nVehicleFlags.bEngineOn);

    pVehicle->m_nVehicleFlags.bEngineOn              = data.m_engineOn;
	pVehicle->m_vehControls.m_handBrake				 = data.m_handBrakeOn;
    pVehicle->m_nVehicleFlags.bScriptSetHandbrake    = data.m_scriptSetHandbrakeOn;

	if (pVehicle->m_nVehicleFlags.bEngineOn && !pVehicle->m_nVehicleFlags.bSkipEngineStartup && data.m_engineSkipEngineStartup)
	{
		if(pVehicle->GetIsRotaryAircraft())
		{
			((CRotaryWingAircraft*) pVehicle)->SetMainRotorSpeed(MAX_ROT_SPEED_HELI_BLADES);

			if(pVehicle->InheritsFromAutogyro())
			{
				static_cast<CAutogyro*>(pVehicle)->SetPropellerSpeed(MAX_ROT_SPEED_HELI_BLADES);
			}
		}
		else if(pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			vehicleDebugf3("CNetObjVehicle::SetVehicleGameState--PLANE--SetEngineSpeed(ms_fMaxEngineSpeed[%f])",CPlane::ms_fMaxEngineSpeed);
			((CPlane*) pVehicle)->SetEngineSpeed(CPlane::ms_fMaxEngineSpeed);
		}
	}

	pVehicle->m_nVehicleFlags.bSkipEngineStartup	 = data.m_engineSkipEngineStartup;

	vehicleDebugf3("CNetObjVehicle::SetVehicleGameState--vehicle[%p][%s][%s]--data.m_engineStarting[%d] data.m_engineOn[%d] data.m_engineSkipEngineStartup[%d]",pVehicle,pVehicle->GetModelName(),pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "",data.m_engineStarting,data.m_engineOn,data.m_engineSkipEngineStartup);

	const bool wasDriveableCar = pVehicle->m_nVehicleFlags.bIsThisADriveableCar;

	pVehicle->SetXenonLightColor(data.m_xenonLightColor);

    pVehicle->m_nVehicleFlags.bUsingPretendOccupants                      = data.m_pretendOccupants;
    pVehicle->m_nVehicleFlags.bIsThisADriveableCar                        = data.m_isDriveable;
	pVehicle->m_nVehicleFlags.bIsThisAStationaryCar                       = data.m_isStationary;
	pVehicle->m_nVehicleFlags.bIsThisAParkedCar							  =	data.m_isParked;
	pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle	  = data.m_forceOtherVehsToStop;
    pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = data.m_DontTryToEnterThisVehicleIfLockedForPlayer;
	pVehicle->m_nVehicleFlags.bMoveAwayFromPlayer						  = data.m_moveAwayFromPlayer;
	pVehicle->GetIntelligence()->SetCustomPathNodeStreamingRadius(data.m_customPathNodeStreamingRadius);

    pVehicle->TurnSirenOn(data.m_sirenOn,false);
	pVehicle->SetNoDamageFromExplosionsOwnedByDriver(data.m_noDamageFromExplosionsOwnedByDriver);
	pVehicle->m_nVehicleFlags.bReadyForCleanup = data.m_flaggedForCleanup;

	pVehicle->m_nVehicleFlags.bInfluenceWantedLevel = data.m_influenceWantedLevel;
	pVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = data.m_hasBeenOwnedByPlayer;
	pVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats = data.m_AICanUseExclusiveSeats;
	pVehicle->m_nVehicleFlags.bDisableSuperDummy = data.m_disableSuperDummy;
	pVehicle->m_nVehicleFlags.bCheckForEnoughRoomToFitPed = data.m_checkForEnoughRoomToFitPed;

	pVehicle->m_nVehicleFlags.bPlaceOnRoadQueued = data.m_placeOnRoadQueued;
	pVehicle->m_nVehicleFlags.bPlaneResistToExplosion = data.m_planeResistToExplosion;
	pVehicle->m_nVehicleFlags.bMercVeh = data.m_mercVeh;
	pVehicle->SetVehicleOccupantsTakeExplosiveDamage(data.m_vehicleOccupantsTakeExplosiveDamage);
	pVehicle->SetCanEjectPassengersIfLocked(data.m_canEjectPassengersIfLocked);
	pVehicle->m_iFullThrottleRecoveryTime = data.m_fullThrottleActive ? data.m_fullThrottleEndTime : 0;
	pVehicle->m_bDriftTyres = data.m_driftTyres;

	m_playerLocks = data.m_PlayerLocks;

	if (pVehicle->GetIntelligence())
	{
		pVehicle->GetIntelligence()->m_bRemoveAggressivelyForCarjackingMission = data.m_RemoveAggressivelyForCarjackingMission;
	}

    if(data.m_runningRespotTimer)
    {
		pVehicle->SetFlashRemotelyDuringRespotting(data.m_useRespotEffect);
        pVehicle->SetRespotCounter(NETWORK_VEHICLE_CLONE_RESPOT_VALUE);
    }
    else if (pVehicle->IsBeingRespotted())
    {
        pVehicle->ResetRespotCounter();
    }

    pVehicle->SetRemoteCarDoorLocks((eCarLockState)data.m_doorLockState);

	SetAsGhost(data.m_ghost BANK_ONLY(, SPG_VEHICLE_STATE));

	//-----------------------------------------

	if(!pVehicle->InheritsFromBike())
	{
		if (data.m_doorsNotAllowedToBeBrokenOff != 0)
		{
			// need to set remote door allowed broke values
			for (s32 i=0; i<pVehicle->GetNumDoors(); i++)
			{
				bool bSetAllowedToBeBrokenOff = !(data.m_doorsNotAllowedToBeBrokenOff & (1<<i)); //negate this here for setting appropriately below
				pVehicle->GetDoor(i)->SetDoorAllowedToBeBrokenOff(bSetAllowedToBeBrokenOff, pVehicle);
			}
		}

		if(data.m_doorsBroken != 0)
		{
			// need to detach any panels we haven't already....
			for (s32 i=0; i<pVehicle->GetNumDoors(); i++)
			{
				if (data.m_doorsBroken & (1<<i))
				{
					if (pVehicle->GetDoor(i)->GetIsIntact(pVehicle))
					{
						pVehicle->GetDoor(i)->BreakOff(pVehicle, false);
					}
				}
			}
		}

		if(pVehicle->InheritsFromPlane())
		{
			static_cast<CNetObjPlane*>(this)->SetCloneDoorsBroken(static_cast<u8>(data.m_doorsBroken));
		}


		m_remoteDoorOpen = 0;

		if( !IsInCutsceneLocallyOrRemotely() )
		{
			m_remoteDoorOpen = data.m_doorsOpen;

			for (s32 i=0; i<pVehicle->GetNumDoors(); i++)
			{
				CCarDoor* pDoor = pVehicle->GetDoor(i);
				if (pDoor)
				{
					if (pDoor->GetIsIntact(pVehicle))
					{
						m_remoteDoorOpenRatios[ i ] = data.m_doorsOpenRatio[ i ];

						if (i < SIZEOF_NUM_DOORS)
						{
#if __BANK
							if (pDoor->m_eDoorLockState != (eCarLockState)data.m_doorIndividualLockedState[i])
							{
								vehicledoorDebugf3("CNetObjVehicle::SetVehicleGameState() [%s][%s][%p]: door - %i, Current lock state - %d (%s), NewLockState - %d (%s)", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle->GetModelName(), pVehicle,
									i, (int)pDoor->m_eDoorLockState, VehicleLockStateStrings[pDoor->m_eDoorLockState], (int)data.m_doorIndividualLockedState[i], VehicleLockStateStrings[(eCarLockState)data.m_doorIndividualLockedState[i]]);
							}
#endif
							pDoor->m_eDoorLockState = (eCarLockState) data.m_doorIndividualLockedState[i];
						}
					}
				}
			}

			ProcessRemoteDoors( true );
		}
	}

	s32 iNumEntryPoints = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints();

	for (s32 iEntryPoint = 0; iEntryPoint < iNumEntryPoints; ++iEntryPoint)
	{
		if (data.m_windowsDown & (1<<iEntryPoint))
		{
			if (pVehicle->IsEntryIndexValid(iEntryPoint))
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPoint);
				if (pEntryPointInfo)
				{
					eHierarchyId window = pEntryPointInfo->GetWindowId();
					if (window != VEH_INVALID_ID)
					{
						//ensure the window is fully valid before calling RolldownWindow
						s32 nBoneIndex = pVehicle->GetBoneIndex(window);
						if(nBoneIndex > -1)
						{
							if(pVehicle->HasComponent(window))
							{
								s32 nComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(nBoneIndex);
								if (nComponent >= 0)
									pVehicle->RolldownWindow(window);
							}
						}
					}
				}
			}
		}
	}

	//-----------------------------------------

    m_alarmSet       = data.m_alarmSet;
    m_alarmActivated = data.m_alarmActivated;

	// timed explosions
	if(data.m_hasTimedExplosion && !m_bTimedExplosionPending)
	{
		m_bTimedExplosionPending  = data.m_hasTimedExplosion; 
		m_nTimedExplosionTime     = data.m_timedExplosionTime;
		m_TimedExplosionCulpritID = data.m_timedExplosionCulprit;
	}

	pVehicle->m_nVehicleFlags.bLightsOn = data.m_lightsOn;
	pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn = data.m_headlightsFullBeamOn;
	pVehicle->m_OverrideLights = data.m_overridelights;

	if (pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
	{
		bool bVehicleIsOnScreen = pVehicle->GetIsOnScreen();
		if (data.m_roofLowered)
			pVehicle->LowerConvertibleRoof(!bVehicleIsOnScreen);
		else
			pVehicle->RaiseConvertibleRoof(!bVehicleIsOnScreen);
	}

	pVehicle->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs = data.m_removeWithEmptyCopOrWreckedVehs;

	//Set an event for the vehicle being undrivable.
	if (wasDriveableCar && !pVehicle->m_nVehicleFlags.bIsThisADriveableCar)
	{
		GetEventScriptNetworkGroup()->Add(CEventNetworkVehicleUndrivable(pVehicle, pVehicle->GetWeaponDamageEntity(), (int)pVehicle->GetWeaponDamageHash()));
	}

    m_bHasPendingExclusiveDriverID = true;
	for (u32 i=0;i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS;++i)
    {
		m_PendingExclusiveDriverID[i] = data.m_exclusiveDriverPedID[i];
	}

	m_bHasPendingLastDriver = true;
	m_LastDriverID = data.m_lastDriverPedID;

	pVehicle->GetIntelligence()->SetJunctionArrivalTime(data.m_junctionArrivalTime);
	pVehicle->GetIntelligence()->SetJunctionCommand((s8)data.m_junctionCommand);

	if(data.m_OverridingVehHorn)
	{
		pVehicle->GetVehicleAudioEntity()->OverrideHorn(true, data.m_OverridenVehHornHash);
	}
	else
	{
		pVehicle->GetVehicleAudioEntity()->OverrideHorn(false, data.m_OverridenVehHornHash);
	}

	pVehicle->m_nVehicleFlags.bUnfreezeWhenCleaningUp = data.m_UnFreezeWhenCleaningUp;

	pVehicle->m_usePlayerLightSettings = data.m_usePlayerLightSettings;

	pVehicle->m_HeadlightMultiplier = data.m_HeadlightMultiplier;

	if (pVehicle->InheritsFromTrailer())
	{
		CTrailer *pTrailer = SafeCast(CTrailer, pVehicle);
		if(pTrailer)
		{
			pTrailer->SetTrailerAttachmentEnabled(data.m_isTrailerAttachmentEnabled);
		}		
	}	

	if(!m_detachedTombStone && data.m_detachedTombStone)
	{
		pVehicle->DetachTombstone();
	}
	m_detachedTombStone = data.m_detachedTombStone;

	if(data.m_ExtraBrokenFlags != 0)
	{
		if(pVehicle->GetModelIndex() == MI_CAR_PBUS2 || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE) || (pVehicle->GetModelNameHash() == MID_COQUETTE4))
		{
			SetExtraFragsBreakable(true);
			for(int i = 0; i < data.MAX_EXTRA_BROKEN_FLAGS; i++)
			{
				if((data.m_ExtraBrokenFlags & (1<<i)) != 0)
				{
					int index = (int)VEH_EXTRA_1 + i;
					int iBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)index);
					int iFragChild = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
					fragInst* pFragInst = pVehicle->GetFragInst();
					if(pFragInst && iFragChild >= 0 && iFragChild < pFragInst->GetTypePhysics()->GetNumChildren())
					{
						int nGroup = pFragInst->GetTypePhysics()->GetAllChildren()[ iFragChild  ]->GetOwnerGroupPointerIndex();
						if (fragCacheEntry* entry = pFragInst->GetCacheEntry())
						{
							if(entry->GetHierInst() && entry->GetHierInst()->groupBroken->IsClear(nGroup))
							{
								pFragInst->BreakOffAboveGroup(nGroup);
							}
						}
					}
				}
			}
			SetExtraFragsBreakable(false);
		}
	}

	pVehicle->SetDownforceModifierFront(data.m_downforceModifierFront);
	pVehicle->SetDownforceModifierRear(data.m_downforceModifierRear);
}

void CNetObjVehicle::ProcessBadging(CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, const bool bVehicleBadge, const CVehicleAppearanceDataNode::CVehicleBadgeData& badgeData, int index)
{
	if (bVehicleBadge)
	{
		bool bAddBadge = false;

		if (!m_pVehicleBadgeData[index])
		{
			if (CVehicleAppearanceDataNode::CVehicleBadgeData::GetPool()->GetNoOfFreeSpaces() > 0)
			{
				m_pVehicleBadgeData[index] = rage_new CVehicleAppearanceDataNode::CVehicleBadgeData;
				vehicleDebugf3("CNetObjVehicle::ProcessBadging bVehicleBadge[%d] index[%d]--rage_new",bVehicleBadge,index);
				bAddBadge = true;
			}
		}
		else if ( m_pVehicleBadgeData[index] && ((!m_VehicleBadgeDesc.IsEqual(badgeDesc)) || (*(m_pVehicleBadgeData[index]) != badgeData)) )
		{
			g_decalMan.RemoveVehicleBadge(pVehicle, m_pVehicleBadgeData[index]->m_badgeIndex);
			vehicleDebugf3("CNetObjVehicle::ProcessBadging bVehicleBadge[%d] index[%d]--not equal",bVehicleBadge,index);
			bAddBadge = true;
		}

		if (bAddBadge)
		{
			vehicleDebugf3("CNetObjVehicle::ProcessBadging bVehicleBadge[%d] index[%d]--bAddBadge",bVehicleBadge,index);
			m_VehicleBadgeDesc = badgeDesc;

			*(m_pVehicleBadgeData[index]) = badgeData;

			m_vehicleBadgeInProcess |= g_decalMan.ProcessVehicleBadge(pVehicle, m_VehicleBadgeDesc, m_pVehicleBadgeData[index]->m_boneIndex, VECTOR3_TO_VEC3V(m_pVehicleBadgeData[index]->m_offsetPos), VECTOR3_TO_VEC3V(m_pVehicleBadgeData[index]->m_dir), VECTOR3_TO_VEC3V(m_pVehicleBadgeData[index]->m_side),  m_pVehicleBadgeData[index]->m_size, false, m_pVehicleBadgeData[index]->m_badgeIndex, m_pVehicleBadgeData[index]->m_alpha);
		}
	}
	else
	{
		if (m_pVehicleBadgeData[index])
		{
			vehicleDebugf3("CNetObjVehicle::ProcessBadging bVehicleBadge[%d] index[%d]--remove",bVehicleBadge,index);
			m_pVehicleBadgeData[index]->m_offsetPos.Zero(); //ensure this is reset

			//remove
			g_decalMan.RemoveVehicleBadge(pVehicle, m_pVehicleBadgeData[index]->m_badgeIndex);
			ResetVehicleBadge(index);
		}
	}
}

void CNetObjVehicle::SetVehicleScriptGameState(const CVehicleScriptGameStateDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    pVehicle->m_nVehicleFlags.bTakeLessDamage                                = data.m_VehicleFlags.takeLessDamage;
    pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted                         = data.m_VehicleFlags.vehicleCanBeTargetted;
    pVehicle->m_nVehicleFlags.bPartOfConvoy                                  = data.m_VehicleFlags.partOfConvoy;
    pVehicle->m_nVehicleFlags.bCanBeVisiblyDamaged                           = data.m_VehicleFlags.canBeVisiblyDamaged;
    pVehicle->m_nVehicleFlags.bFreebies                                      = data.m_VehicleFlags.freebies;
    pVehicle->m_nVehicleFlags.bRespectLocksWhenHasDriver                     = data.m_VehicleFlags.respectLocksWhenHasDriver;
	pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision                        = data.m_VehicleFlags.shouldFixIfNoCollision;
	pVehicle->m_nVehicleFlags.bAutomaticallyAttaches                         = data.m_VehicleFlags.automaticallyAttaches;
	pVehicle->m_nVehicleFlags.bScansWithNonPlayerDriver                      = data.m_VehicleFlags.scansWithNonPlayerDriver;
    pVehicle->m_nVehicleFlags.bDisableExplodeOnContact                       = data.m_VehicleFlags.disableExplodeOnContact;
	pVehicle->m_nVehicleFlags.bDisableTowing                                 = data.m_VehicleFlags.disableTowing;
    pVehicle->m_nVehicleFlags.bCanEngineDegrade                              = data.m_VehicleFlags.canEngineDegrade;
    pVehicle->m_nVehicleFlags.bCanPlayerAircraftEngineDegrade                = data.m_VehicleFlags.canPlayerAircraftEngineDegrade;
    pVehicle->m_nVehicleFlags.bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans = data.m_VehicleFlags.nonAmbientVehicleMayBeUsedByGoToPointAnyMeans;
    pVehicle->m_nVehicleFlags.bCanBeUsedByFleeingPeds                        = data.m_VehicleFlags.canBeUsedByFleeingPeds;
    pVehicle->m_nVehicleFlags.bAllowNoPassengersLockOn                       = data.m_VehicleFlags.allowNoPassengersLockOn;
	pVehicle->m_bAllowHomingMissleLockOnSynced								 = data.m_VehicleFlags.allowHomingMissleLockOn;
    pVehicle->m_nVehicleFlags.bDisablePetrolTankDamage                       = data.m_VehicleFlags.disablePetrolTankDamage;
    pVehicle->m_nVehicleFlags.bIsStolen                                      = data.m_VehicleFlags.isStolen;
    pVehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage                 = data.m_VehicleFlags.explodesOnHighExplosionDamage;
	pVehicle->m_nVehicleFlags.bRearDoorsHaveBeenBlownOffByStickybomb		 = data.m_VehicleFlags.rearDoorsHaveBeenBlownOffByStickybomb;
	pVehicle->GetVehicleAudioEntity()->SetIsInCarModShop(data.m_VehicleFlags.isInCarModShop);
	pVehicle->m_bSpecialEnterExit											 = data.m_VehicleFlags.specialEnterExit;
	pVehicle->m_bOnlyStartVehicleEngineOnThrottle							 = data.m_VehicleFlags.onlyStartVehicleEngineOnThrottle;
	pVehicle->m_bDontOpenRearDoorsOnExplosion								 = data.m_VehicleFlags.dontOpenRearDoorsOnExplosion;
	pVehicle->m_bDontProcessVehicleGlass									 = data.m_VehicleFlags.dontProcessVehicleGlass;
	pVehicle->m_bUseDesiredZCruiseSpeedForLanding							 = data.m_VehicleFlags.useDesiredZCruiseSpeedForLanding;
	pVehicle->m_bDontResetTurretHeadingInScriptedCamera						 = data.m_VehicleFlags.dontResetTurretHeadingInScriptedCamera;
	pVehicle->m_bDisableWantedConesResponse									 = data.m_VehicleFlags.disableWantedConesResponse;

	pVehicle->m_VehicleProducingSlipstream = data.m_VehicleProducingSlipstream;

	pVehicle->SetRocketBoostRechargeRate(data.m_rocketBoostRechargeRate);

    // don't set these when the vehicle is reverting to an ambient and this is being called via CProjectSyncTree::ResetScriptState()
    if (IsScriptObject())
	{
		pVehicle->m_nVehicleFlags.bLockDoorsOnCleanup	= data.m_VehicleFlags.lockDoorsOnCleanup;
	}
	else
    {
        pVehicle->m_nVehicleFlags.bIsDrowning			= data.m_VehicleFlags.isDrowning;
    }

    pVehicle->PopTypeSet((ePopType)data.m_PopType);

	m_lockedForNonScriptPlayers = data.m_VehicleFlags.lockedForNonScriptPlayers;

    m_teamLocks                 = data.m_TeamLocks;
    m_teamLockOverrides         = data.m_TeamLockOverrides;
    m_GarageInstanceIndex       = data.m_GarageInstanceIndex;

	m_bRemoteIsInAir = data.m_isinair;
	pVehicle->m_Transmission.SetPlaneDamageThresholdOverride((float)data.m_DamageThreshold);
	pVehicle->GetVehicleDamage()->SetScriptDamageScale(data.m_fScriptDamageScale);
	pVehicle->GetVehicleDamage()->SetScriptWeaponDamageScale(data.m_fScriptWeaponDamageScale);

	pVehicle->GetIntelligence()->BlockWeaponSelection(data.m_bBlockWeaponSelection);

	pVehicle->SetBeastVehicle(data.m_isBeastVehicle);

	pVehicle->SetScriptRammingImpulseScale(data.m_fRampImpulseScale);

	pVehicle->SetOverrideArriveDistForVehPersuitAttack(data.m_fOverrideArriveDistForVehPersuitAttack);

	pVehicle->GetIntelligence()->m_bSetBoatIgnoreLandProbes = data.m_bBoatIgnoreLandProbes;

	if(pVehicle->InheritsFromAutomobile())
	{		
		SafeCast(CAutomobile, pVehicle)->SetCloneParachuting(data.m_IsCarParachuting);
		if (data.m_IsCarParachuting)
		{
			SafeCast(CAutomobile, pVehicle)->SetCloneParachuteStickValues(data.m_parachuteStickX, data.m_parachuteStickY);
		}
	}
	
	if (pVehicle->InheritsFromAmphibiousAutomobile())
	{
		m_bLockedToXY = data.m_lockedToXY;

		CAmphibiousAutomobile* amphibiousVehicle = static_cast<CAmphibiousAutomobile*>(pVehicle);
		if (amphibiousVehicle)
		{
			amphibiousVehicle->m_Buoyancy.m_fForceMult = data.m_BuoyancyForceMultiplier;
		}		
	}	
	else if (pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
		{
			if (pSeaPlaneExtension)
			{
				m_bLockedToXY = data.m_lockedToXY;
				pPlane->m_Buoyancy.m_fForceMult = data.m_BuoyancyForceMultiplier;
			}			
		}
	}


	if(pVehicle->InheritsFromAutomobile())
	{	
		SafeCast(CAutomobile, pVehicle)->SetParachuteTintIndex(data.m_vehicleParachuteTintIndex);
		if(data.m_hasParachuteObject)
		{
			SetPendingParachuteObjectId(data.m_parachuteObjectId);
		}
	}

	m_disableCollisionUponCreation = data.m_disableCollisionUponCreation;
	pVehicle->m_disableRampCarImpactDamage = data.m_disableRampCarImpactDamage;

	for(int i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{
		pVehicle->SetRestrictedAmmoCount(i, data.m_restrictedAmmoCount[i]);
	}

	if(pVehicle->InheritsFromAmphibiousQuadBike())
	{
		static_cast<CAmphibiousQuadBike*>(pVehicle)->SetTuckWheelsIn(data.m_tuckInWheelsForQuadBike);
	}

	if(pVehicle->InheritsFromHeli() && ((CHeli*)pVehicle)->GetIsCargobob())
	{
		((CHeli*)pVehicle)->SetInitialRopeLength(data.m_HeliRopeLength);

        CVehicleGadgetPickUpRope *pickUpRope = GetPickupRopeGadget(pVehicle);

        if(pickUpRope)
        {
            pickUpRope->SetExtraPickupBoundAllowance(data.m_ExtraBoundAttachAllowance);
        }
		((CHeli*)pVehicle)->SetCanPickupEntitiesThatHavePickupDisabled(data.m_canPickupEntitiesThatHavePickupDisabled);
	}

	pVehicle->SetScriptForceHd(data.m_ScriptForceHd);
	pVehicle->m_nVehicleFlags.bCanEngineMissFire = data.m_CanEngineMissFire;
	if(pVehicle->GetFragInst() && pVehicle->GetFragInst()->IsBreakingDisabled() != data.m_DisableBreaking)
	{
		pVehicle->GetFragInst()->SetDisableBreakable(data.m_DisableBreaking);
	}
	m_cloneGliderState = data.m_gliderState;

	pVehicle->m_bDisablePlayerCanStandOnTop = data.m_disablePlayerCanStandOnTop;

	pVehicle->SetBombAmmoCount(data.m_BombAmmoCount);
	pVehicle->SetCountermeasureAmmoCount(data.m_CountermeasureAmmoCount);

	pVehicle->SetScriptMaxSpeed(data.m_ScriptMaxSpeed);

	pVehicle->GetVehicleDamage()->SetCollisionWithMapDamageScale(data.m_CollisionWithMapDamageScale);

	if(pVehicle->InheritsFromSubmarineCar())
	{
		CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
		if(submarineCar)
		{
			if(data.m_InSubmarineMode && !submarineCar->IsInSubmarineMode())
			{
				submarineCar->TransformToSubmarine(data.m_TransformInstantly);
			}
			else if(!data.m_InSubmarineMode && submarineCar->IsInSubmarineMode())
			{
				submarineCar->TransformToCar(data.m_TransformInstantly);
			}
		}
		submarineCar->SetSubmarineMode( data.m_InSubmarineMode, false );
	}

	pVehicle->SetSpecialFlightModeAllowed(data.m_AllowSpecialFlightMode);
	pVehicle->SetSpecialFlightModeTargetRatio(data.m_SpecialFlightModeUsed?1.0f:0.0f);
	pVehicle->SetHomingCanLockOnToObjects(data.m_homingCanLockOnToObjects);
	if(pVehicle->InheritsFromPlane())
	{
		SafeCast(CPlane, pVehicle)->SetDisableVerticalFlightModeTransition(data.m_DisableVericalFlightModeTransition);
	}

	pVehicle->SetDeployOutriggers(data.m_HasOutriggerDeployed);

	pVehicle->GetIntelligence()->SetUsingScriptAutopilot(data.m_UsingAutoPilot);

	pVehicle->SetDisableHoverModeFlight(data.m_DisableHoverModeFlight);

	if(pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->SetRadioEnabled(data.m_RadioEnabledByScript);
	}

	pVehicle->SetIncreaseWheelCrushDamage(data.m_bIncreaseWheelCrushDamage);
}

void CNetObjVehicle::SetVehicleHealth(const CVehicleHealthDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	//Cache if the vehicle is already destroyed.
	const bool isAlreadyWrecked = (pVehicle->GetStatus() == STATUS_WRECKED);

	pVehicle->ResetWeaponDamageInfo();

	CEntity* weaponDamageEntity = NULL;
	if (data.m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID)
	{
		netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_weaponDamageEntity);
		if (pNetObj && pNetObj->GetEntity())
		{
			weaponDamageEntity = pNetObj->GetEntity();
			pVehicle->SetWeaponDamageInfo(weaponDamageEntity, data.m_weaponDamageHash, fwTimer::GetTimeInMilliseconds());
		}
	}

	//Total Damage inflicted to vehicle
	float totalDamage = 0.0f;

	if (data.m_hasMaxHealth)
	{
		pVehicle->SetHealth(pVehicle->GetMaxHealth(), true);
	}
	else
	{
		totalDamage = pVehicle->GetHealth() - (float)data.m_health;
		pVehicle->SetHealth((float)data.m_health, true);		
	}

	if (data.m_healthsame)
	{
		if (data.m_hasMaxHealth)
		{
			pVehicle->GetVehicleDamage()->SetBodyHealth(pVehicle->GetMaxHealth());
		}
		else
		{
			bool bIncludeBodyDamage = data.m_weaponDamageHash == ATSTRINGHASH("VEHICLE_WEAPON_WATER_CANNON",0x67D18297) || data.m_weaponDamageHash == ATSTRINGHASH("WEAPON_HIT_BY_WATER_CANNON",0xCC34325E);
			if(bIncludeBodyDamage)
			{
				totalDamage += (pVehicle->GetVehicleDamage()->GetBodyHealth() - (float)data.m_health);
			}

			pVehicle->GetVehicleDamage()->SetBodyHealth((float)data.m_health);
		}
	}
	else
	{
		bool bIncludeBodyDamage = data.m_weaponDamageHash == ATSTRINGHASH("VEHICLE_WEAPON_WATER_CANNON",0x67D18297) || data.m_weaponDamageHash == ATSTRINGHASH("WEAPON_HIT_BY_WATER_CANNON",0xCC34325E);
		if(bIncludeBodyDamage)
		{
			totalDamage += (pVehicle->GetVehicleDamage()->GetBodyHealth() - (float)data.m_bodyhealth);
		}
		pVehicle->GetVehicleDamage()->SetBodyHealth((float)data.m_bodyhealth);
	}

	bool wreckedBySinking = false;

	if(data.m_isWrecked && (pVehicle->GetStatus() != STATUS_WRECKED))
    {
		vehicleDebugf3("CNetObjVehicle::SetVehicleHealth--data.m_isWrecked && (pVehicle->GetStatus() != STATUS_WRECKED)");

		bool bIsDummy = pVehicle->IsDummy();

		if (bIsDummy && pVehicle->IsCollisionLoadedAroundPosition() && !pVehicle->InheritsFromTrailer())
		{
			bIsDummy = !pVehicle->TryToMakeFromDummy();
		}

		if (data.m_isBlownUp)
		{
			vehicleDebugf3("CNetObjVehicle::SetVehicleHealth--data.m_isWrecked && (pVehicle->GetStatus() != STATUS_WRECKED)--data.m_isWreckedByExplosion-->BlowUpCar");
			pVehicle->BlowUpCar(pVehicle->GetWeaponDamageEntity(), false, false, true);
		}
		else 
        {
			vehicleDebugf3("CNetObjVehicle::SetVehicleHealth--data.m_isWrecked && (pVehicle->GetStatus() != STATUS_WRECKED)--(pVehicle->GetIsInWater() || bIsDummy)-->SetIsWrecked");
			pVehicle->SetIsWrecked();

			if (pVehicle->GetIsInWater())
			{
				wreckedBySinking = true;

				//Make sure if a remote driver is driving your vehicle that 
				//  he gets charged with the correct insurance amount.
				CPed* damager = pVehicle->GetVehicleWasSunkByAPlayerPed();
				if (damager)
				{
					pVehicle->SetWeaponDamageInfo(damager, WEAPONTYPE_UNARMED, fwTimer::GetTimeInMilliseconds());
				}
			}
        }
	}

	if(data.m_isWrecked && pVehicle->InheritsFromBlimp())
	{
		CBlimp* blimp = SafeCast(CBlimp, pVehicle);
		if(!blimp->IsBlimpBroken())
		{
			blimp->BreakBlimp();
		}
	}

	//Not the maximun default health
	if (data.m_packedEngineHealth != static_cast<s32>(ENGINE_HEALTH_MAX))
	{
		totalDamage += pVehicle->m_Transmission.GetEngineHealth() - static_cast<float>(data.m_packedEngineHealth);
	}
	pVehicle->m_Transmission.SetEngineHealth(Max(static_cast<float>(data.m_packedEngineHealth), ENGINE_DAMAGE_FINSHED));

	if(data.m_hasMaxHealth && pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		if(pPlane)
		{
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_L, (float)data.m_packedEngineHealth);
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_R, (float)data.m_packedEngineHealth);
		}		
	}

    // read petrol tank health (can also be negative)
	if (data.m_packedPetrolTankHealth != static_cast<s32>(VEH_DAMAGE_HEALTH_STD))
	{
		totalDamage += pVehicle->GetVehicleDamage()->GetPetrolTankHealth() - static_cast<float>(data.m_packedPetrolTankHealth);
	}
	pVehicle->GetVehicleDamage()->SetPetrolTankHealth(static_cast<float>(data.m_packedPetrolTankHealth));

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	gnetAssert(pShader);
    
    for(s32 index = 0; index < pVehicle->GetNumWheels(); index++)
    {
        CWheel *wheel = pVehicle->GetWheel(index);
        gnetAssert(wheel);

        if(wheel)
        {
			if(!data.m_tyreHealthDefault)
			{
				wheel->SetTyreWearRate(data.m_tyreWearRate[index]);

				if(data.m_tyreBrokenOff[index] && (!pVehicle->GetWheelBroken(index)))
				{
					pVehicle->GetVehicleDamage()->BreakOffWheel(index, 1.0f, 0.0f, 0.0f, false, false); //bNetworkCheck = false
				}
				else
				{
					// if the tyre has been damaged while in perfect health, apply a bit
					// or arbitrary damage to make the tyre deflate
					if(data.m_tyreDamaged[index] && (wheel->GetTyreHealth() == TYRE_HEALTH_DEFAULT))
					{
						totalDamage += 25.0f;
						wheel->ApplyTyreDamage(weaponDamageEntity, 25.0f, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), DAMAGE_TYPE_BULLET, index, false); //bNetworkCheck = false
					}

					if(data.m_tyreDestroyed[index])
					{
						wheel->UpdateTyreHealthFromNetwork(0.0f);
					}
				}

				if(data.m_tyreFire[index] && !wheel->IsWheelOnFire())
				{
					wheel->SetWheelOnFire(false); //bNetworkCheck = false
				}
			}
			else
			{
				if(wheel->GetTyreHealth() != TYRE_HEALTH_DEFAULT)
				{
					wheel->ResetDamage();				
					pShader->SetEnableTyreDeform(false);
				}
				wheel->SetTyreWearRate(0.0f);
			}		
        }
    }
    

    // read the health of the suspension
    if(!data.m_suspensionHealthDefault)
    {
        for(s32 index = 0; index < pVehicle->GetNumWheels(); index++)
        {
            CWheel *wheel = pVehicle->GetWheel(index);
            gnetAssert(wheel);

            if(wheel)
            {
                if(wheel->GetSuspensionHealth() <= data.m_suspensionHealth[index])
                {
                    wheel->SetSuspensionHealth(data.m_suspensionHealth[index]);
                }
                else
                {
                    float healthDelta = wheel->GetSuspensionHealth() - data.m_suspensionHealth[index];
                    wheel->ApplySuspensionDamage(healthDelta);
					totalDamage += healthDelta;
                }
            }
        }
    }

	//We have received a RECEIVED_CLONE_CREATE - so this clone can be receiving a damage event.
	const bool weAlreadyExist = (NetworkInterface::GetObjectManager().GetNetworkObject(GetObjectID()) != NULL);

	//Set network damage trackers
	if (weAlreadyExist && !isAlreadyWrecked && (0.0f < totalDamage || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(pVehicle)))
	{
		CNetObjPhysical::NetworkDamageInfo damageInfo(pVehicle->GetWeaponDamageEntity(), totalDamage, 0.0f, 0.0f, false, pVehicle->GetWeaponDamageHash(), -1, false, pVehicle->GetStatus() == STATUS_WRECKED, false, data.m_lastDamagedMaterialId);
		UpdateDamageTracker(damageInfo, m_CloneIsResponsibleForCollision);
	}
	else if (weAlreadyExist && !isAlreadyWrecked && wreckedBySinking && (pVehicle->InheritsFromPlane() || pVehicle->GetIsRotaryAircraft()))
	{
		CNetObjPhysical::NetworkDamageInfo damageInfo(pVehicle->GetWeaponDamageEntity(), totalDamage, 0.0f, 0.0f, false, pVehicle->GetWeaponDamageHash(), -1, false, pVehicle->GetStatus() == STATUS_WRECKED, false, data.m_lastDamagedMaterialId);
		UpdateDamageTracker(damageInfo, m_CloneIsResponsibleForCollision);
	}

	if (m_extinguishedFireCount != data.m_extinguishedFireCount)
	{
		if (pVehicle->IsOnFire())
		{
			pVehicle->ExtinguishCarFire();
		}
		m_extinguishedFireCount = data.m_extinguishedFireCount;
	}

	RegisterKillWithNetworkTracker();

	if (m_fixedCount != data.m_fixedCount)
	{
		//Only apply fix if max health is full - otherwise we might be applying fix here incorrectly on creation when initial fixed count comes through.
		if (data.m_hasMaxHealth)
		{
			gnetDebug1("CNetObjVehicle::SetVehicleHealth-->Fix %s", GetLogName());

			pVehicle->Fix(true,true);
			// Fixing will set the desired vertical flight mode to 1.0f so reset it here to the last received
			if(pVehicle->InheritsFromPlane() && static_cast<CPlane*>(pVehicle)->GetSpecialFlightModeAllowed())
			{
				static_cast<CPlane*>(pVehicle)->SetDesiredVerticalFlightModeRatio(m_lastReceivedVerticalFlightModeRatio);
			}

			pVehicle->SetHealth(pVehicle->GetMaxHealth(), true);
			if (data.m_healthsame)
			{
				pVehicle->GetVehicleDamage()->SetBodyHealth((float)data.m_health);
			}
			else
			{
				pVehicle->GetVehicleDamage()->SetBodyHealth((float)data.m_bodyhealth);
			}
		}
		m_fixedCount = data.m_fixedCount; //after the fix ensure the fix count now equals the correct fix count
	}
}

void  CNetObjVehicle::CacheCollisionFault(CEntity* pInflictor, u32 nWeaponHash)
{
	m_CloneIsResponsibleForCollision = false;
	CNetObjEntity* netInflictor = static_cast<CNetObjEntity *>(NetworkUtils::GetNetworkObjectFromEntity(pInflictor));
	if (netInflictor)
	{
		CVehicle* victim  = CEventNetworkEntityDamage::GetVehicle(GetEntity()); 
		CVehicle* damager = CEventNetworkEntityDamage::GetVehicle(netInflictor->GetEntity());
		m_CloneIsResponsibleForCollision = CEventNetworkEntityDamage::IsVehicleResponsibleForCollision(victim, damager, nWeaponHash);
	}
}

void CNetObjVehicle::OnVehicleFixed()
{
	gnetDebug1("CNetObjVehicle::OnVehicleFixed %s", GetLogName());

	m_WindowsBroken = 0;
	m_rearWindscreenSmashed = false;
	m_frontWindscreenSmashed = false;

    m_deformationDataFL = 0;
	m_deformationDataFR = 0;
	m_deformationDataML = 0;
	m_deformationDataMR = 0;
    m_deformationDataRL = 0;
    m_deformationDataRR = 0;

	//Only set this on the clone, otherwise it will be set and there after transfer of ownership - causing badness.
	if (IsClone())
		m_bForceProcessApplyNetworkDeformation = true;

	m_fixedCount++;

	if (m_fixedCount > MAX_ALTERED_VALUE)
		m_fixedCount = 0;

	if (!IsClone())
	{
		DirtyVehicleDamageStatusDataSyncNode(); //ensure the cleared data is sent so it's aligned.
	}

	ClearWeaponImpactPointInformation();
}

void CNetObjVehicle::OnVehicleFireExtinguished()
{
	if(IsClone())
	{
		return;
	}

	m_extinguishedFireCount++;

	if (m_extinguishedFireCount > MAX_ALTERED_VALUE)
		m_extinguishedFireCount = 0;
}

void CNetObjVehicle::SetVehicleBadge(const CVehicleBadgeDesc& badgeDesc, const s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, const u32 badgeIndex, u8 alpha)
{ 
	if (gnetVerifyf(badgeIndex < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES, "Skip SetVehicleBadge -- badgeIndex[%u] >= CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES[%u]",badgeIndex,CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES))
	{
		if (!m_pVehicleBadgeData[badgeIndex])
		{
			if (gnetVerifyf(CVehicleAppearanceDataNode::CVehicleBadgeData::GetPool()->GetNoOfFreeSpaces() > 0, "Couldn't create vehicle badge for %s - ran out of badge data", GetLogName()))
			{
				m_pVehicleBadgeData[badgeIndex] = rage_new CVehicleAppearanceDataNode::CVehicleBadgeData;
			}
		}

		m_VehicleBadgeDesc = badgeDesc;

		if (m_pVehicleBadgeData[badgeIndex])
		{
			m_pVehicleBadgeData[badgeIndex]->m_boneIndex = boneIndex; 
			m_pVehicleBadgeData[badgeIndex]->m_offsetPos = VEC3V_TO_VECTOR3(vOffsetPos); 
			m_pVehicleBadgeData[badgeIndex]->m_dir = VEC3V_TO_VECTOR3(vDir); 
			m_pVehicleBadgeData[badgeIndex]->m_side = VEC3V_TO_VECTOR3(vSide); 
			m_pVehicleBadgeData[badgeIndex]->m_size = size; 
			m_pVehicleBadgeData[badgeIndex]->m_badgeIndex = badgeIndex;
			m_pVehicleBadgeData[badgeIndex]->m_alpha = alpha;

			m_bHasBadgeData = true;
		}
	}
}

void CNetObjVehicle::ResetVehicleBadge(int index) 
{
	if (m_bHasBadgeData)
	{
		if (m_pVehicleBadgeData[index])
		{
			delete m_pVehicleBadgeData[index];
			m_pVehicleBadgeData[index] = NULL;
		}

		m_bHasBadgeData = false; //reset
		for(int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
		{
			m_bHasBadgeData |= (m_pVehicleBadgeData[i] != NULL);
		}
	}
}

u8 CNetObjVehicle::ExtractWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup, bool bFromApplyArray)
{
	u8 count = 0;
	if (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN)
	{
		count = bFromApplyArray ? m_ApplyWeaponImpactPointLocationCounts[idx] & 0xF0 : m_weaponImpactPointLocationCounts[idx] & 0xF0;
		count = count >> 4;
	}
	else
	{
		count = bFromApplyArray ? m_ApplyWeaponImpactPointLocationCounts[idx] & 0x0F : m_weaponImpactPointLocationCounts[idx] & 0x0F;
	}

	return count;
}

void CNetObjVehicle::IncrementWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup)
{
	u8 count = ExtractWeaponImpactPointLocationCount(idx,networkEffectGroup);

	//clear the previous storage location portion
	m_weaponImpactPointLocationCounts[idx] = (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) ? m_weaponImpactPointLocationCounts[idx] & 0x0F : m_weaponImpactPointLocationCounts[idx] & 0xF0;

	//increment count
	count++;

	//maximum count
	if (count > 15)
		count = 15;

	//properly position the count for use in replacing the actual storage location
	count = (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) ? count << 4 : count;

	//replace the data in the storage location
	m_weaponImpactPointLocationCounts[idx] = m_weaponImpactPointLocationCounts[idx] | count;
}

void CNetObjVehicle::DecrementApplyWeaponImpactPointLocationCount(int idx, eNetworkEffectGroup networkEffectGroup)
{
	u8 count = ExtractWeaponImpactPointLocationCount(idx,networkEffectGroup,true);

	//can't decrement less then 0
	if (count == 0)
		return;

	//clear the previous storage location portion
	m_ApplyWeaponImpactPointLocationCounts[idx] = (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) ? m_ApplyWeaponImpactPointLocationCounts[idx] & 0x0F : m_ApplyWeaponImpactPointLocationCounts[idx] & 0xF0;

	//decrement count
	count--;

	//properly position the count for use in replacing the actual storage location
	count = (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) ? count << 4 : count;

	//replace the data in the storage location
	m_ApplyWeaponImpactPointLocationCounts[idx] = m_ApplyWeaponImpactPointLocationCounts[idx] | count;
}

void CNetObjVehicle::UpdateForceSendRequests()
{
	CVehicleSyncTree *syncTree = SafeCast(CVehicleSyncTree, GetSyncTree());
	
	if(syncTree)
	{
		u8 frameCount = static_cast<u8>(fwTimer::GetSystemFrameCount());

		if(frameCount != m_FrameForceSendRequested)
		{
			if(m_ForceSendHealthNode)
			{
				m_ForceSendHealthNode = false;
				if(HasSyncData())
				{
					syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetVehicleHealthNode());
				}
			}
			else if (m_ForceSendScriptGameStateNode)
			{
				m_ForceSendScriptGameStateNode = false;
				if(HasSyncData())
				{
					syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetVehicleScriptGameStateNode());
				}
			}			
		}
	}
}

void CNetObjVehicle::ForceSendVehicleDamageNode()
{
	CVehicleSyncTree *syncTree = SafeCast(CVehicleSyncTree, GetSyncTree());

	if(syncTree && HasSyncData())
	{
		syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetVehicleDamageStatusNode());
	}
}

void CNetObjVehicle::SetExtraFragsBreakable(bool breakable)
{
	CVehicle* vehicle = GetVehicle();
	if (vehicle && vehicle->GetVehicleFragInst())
	{
		if (breakable)
		{
			for (int i = VEH_EXTRA_1; i <= VEH_LAST_EXTRA; i++)
			{
				u32 nExtraBit = BIT(i - VEH_EXTRA_1 + 1);
				vehicle->GetVehicleFragInst()->ClearDontBreakFlag(nExtraBit);
			}
		}
		else
		{
			for (int i = VEH_EXTRA_1; i <= VEH_LAST_EXTRA; i++)
			{
				u32 nExtraBit = BIT(i - VEH_EXTRA_1 + 1);
				vehicle->GetVehicleFragInst()->SetDontBreakFlag(nExtraBit);
			}
		}
	}
}

static const unsigned FRONT_LEFT_DAMAGE = 0;
static const unsigned FRONT_RIGHT_DAMAGE = 1;
static const unsigned MIDDLE_LEFT_DAMAGE = 2;
static const unsigned MIDDLE_RIGHT_DAMAGE = 3;
static const unsigned REAR_LEFT_DAMAGE = 4;
static const unsigned REAR_RIGHT_DAMAGE = 5;

void CNetObjVehicle::StoreWeaponImpactPointInformation(const Vector3& vHitPosition, eWeaponEffectGroup weaponEffectGroup, bool bAllowOnClone)
{
	if (IsClone() && !bAllowOnClone)
		return;

	eNetworkEffectGroup networkEffectGroup;

	//only store bullets and shotgun bullets
	switch(weaponEffectGroup)
	{
		case WEAPON_EFFECT_GROUP_PISTOL_SMALL:
		case WEAPON_EFFECT_GROUP_PISTOL_LARGE:
		case WEAPON_EFFECT_GROUP_SMG:
		case WEAPON_EFFECT_GROUP_RIFLE_ASSAULT:
		case WEAPON_EFFECT_GROUP_RIFLE_SNIPER:
		case WEAPON_EFFECT_GROUP_HEAVY_MG:
		case WEAPON_EFFECT_GROUP_VEHICLE_MG:
			networkEffectGroup = WEAPON_NETWORK_EFFECT_GROUP_BULLET;
			break;

		case WEAPON_EFFECT_GROUP_SHOTGUN:
			networkEffectGroup = WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN;
			break;

		//If the effect group isn't in these categories return
		default:
			return;
	}

	//Shotguns induce multiple calls to Store with a single hit, just record the first one
	if ((networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) && !m_weaponImpactPointAllowOneShotgunPerFrame)
		return;

	Vector3 objSpaceVecPos = VEC3V_TO_VECTOR3(GetEntity()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vHitPosition)));
	Vector3 damageOffset = objSpaceVecPos;
	float damageOffsetOriginalLength = damageOffset.Mag();
	damageOffset.NormalizeSafe();
	damageOffset.w = damageOffsetOriginalLength;

	Vector4 offset;
	offset.SetVector3(damageOffset);
	offset.SetW(damageOffset.w);

	// calculate which side of the car has been hit
	float dotProductTop   = offset.Dot3(Vector3(0.0f, 1.0f, 0.0f));
	float dotProductRight = offset.Dot3(Vector3(1.0f, 0.0f, 0.0f));

	int idx = FRONT_LEFT_DAMAGE;

	if(dotProductTop >= 0.0f)
	{
		// front half of car
		if(dotProductTop >= 0.5f)
		{
			if(dotProductRight < 0.0f)
			{
				// left of car
				idx = FRONT_LEFT_DAMAGE;
			}
			else
			{
				// right of car
				idx = FRONT_RIGHT_DAMAGE;
			}
		}
		else
		{
			if(dotProductRight < 0.0f)
			{
				// left of car
				idx = MIDDLE_LEFT_DAMAGE;
			}
			else
			{
				// right of car
				idx = MIDDLE_RIGHT_DAMAGE;
			}
		}
	}
	else
	{
		// rear half of car
		if(-dotProductTop >= 0.5f)
		{
			if(dotProductRight < 0.0f)
			{
				// left of car
				idx = REAR_LEFT_DAMAGE;
			}
			else
			{
				// right of car
				idx = REAR_RIGHT_DAMAGE;
			}
		}
		else
		{
			if(dotProductRight < 0.0f)
			{
				// left of car
				idx = MIDDLE_LEFT_DAMAGE;
			}
			else
			{
				// right of car
				idx = MIDDLE_RIGHT_DAMAGE;
			}
		}
	}

	IncrementWeaponImpactPointLocationCount(idx,networkEffectGroup);

	m_weaponImpactPointLocationSet = true;
	if (networkEffectGroup == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN)
		m_weaponImpactPointAllowOneShotgunPerFrame = false;
}

void CNetObjVehicle::ClearWeaponImpactPointInformation()
{
	m_weaponImpactPointLocationSet = false;
	m_ApplyWeaponImpactPointLocationSet = false;
	m_ApplyWeaponImpactPointLocationProcessCount = 0;

	for(int i=0; i < NUM_NETWORK_DAMAGE_DIRECTIONS; i++)
	{
		m_weaponImpactPointLocationCounts[i] = 0;
		m_ApplyWeaponImpactPointLocationCounts[i] = 0;
	}
}

float fNOVDefaultRadiusDamage[3] = { 0.2f, 0.3f, 0.4f };
float NOVDamageLevelPct[3]       = { 0.4f, 0.55f, 0.7f };

float NOVDamagePosition[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS][2] = {	{ -1.0f,  1.0f },
																					{  1.0f,  1.0f },
																					{ -1.0f,  0.0f },
																					{  1.0f,  0.0f },
																					{ -1.0f, -1.0f },
																					{  1.0f, -1.0f } };
float NOVrandomXMin = 0.0f; //0.9f;
float NOVrandomXMax = 5.0f; //1.2f;

float NOVrandomYMin = 0.0f; //1.5f; // push it toward the front/back.
float NOVrandomYMax = 5.0f; //3.0f;

float NOVrandomZMin = 0.0f; //-0.25f;
float NOVrandomZMax = 1.0f; //0.25f;

void CNetObjVehicle::ApplyRemoteWeaponImpactPointInformation()
{
	if (!m_ApplyWeaponImpactPointLocationSet)
		return;

	//should only apply this on remotes
	if (!IsClone())
		return;

	int addWeaponImpactRetc = 1;
	
	for(int i=0; ((i < NUM_NETWORK_DAMAGE_DIRECTIONS) && (addWeaponImpactRetc > 0)); i++)
	{
		for (int j=0; ((j < WEAPON_NETWORK_EFFECT_GROUP_MAX) && (addWeaponImpactRetc > 0)); j++)
		{
			u8 count = ExtractWeaponImpactPointLocationCount(i,(eNetworkEffectGroup)j,true);

			while ((count > 0) && (addWeaponImpactRetc > 0))
			{
				float fRandomX = fwRandom::GetRandomNumberInRange(NOVrandomXMin, NOVrandomXMax) * NOVDamagePosition[i][0];
				float fRandomY = fwRandom::GetRandomNumberInRange(NOVrandomYMin, NOVrandomYMax) * NOVDamagePosition[i][1];
				float fRandomZ = fwRandom::GetRandomNumberInRange(NOVrandomZMin, NOVrandomZMax);

				phMaterialMgr::Id	materialId = 116; //car_metal
				s32	  componentId = 0;

				Vector3 offset(fRandomX, fRandomY, fRandomZ);

				Vector3 entityPosition = VEC3V_TO_VECTOR3(GetEntity()->GetTransform().GetPosition());
				Vector3 worldPosition = VEC3V_TO_VECTOR3(GetEntity()->GetTransform().Transform(RCC_VEC3V(offset)));

				Vector3 origWorldPosition = worldPosition;

				Vector3 normalWorld = worldPosition - entityPosition;
				normalWorld.Normalize();

				Vector3 directionWorld = entityPosition - worldPosition;
				directionWorld.Normalize();

				WorldProbe::CShapeTestProbeDesc probeDesc;
				WorldProbe::CShapeTestResults* probe_results = NULL;

				WorldProbe::CShapeTestFixedResults<1> probeResult;
				probe_results = &probeResult;

				probeDesc.SetResultsStructure(probe_results);
				probeDesc.SetStartAndEnd(worldPosition, entityPosition);
				probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

				int nLosOptions = 0;
				nLosOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
				nLosOptions |= WorldProbe::LOS_IGNORE_POLY_SHOOT_THRU;
				nLosOptions |= WorldProbe::LOS_TEST_VEH_TYRES;

				probeDesc.SetOptions(nLosOptions);

				probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);

				phInst* pHitInst = GetEntity()->GetFragInst();
				if(!pHitInst) pHitInst = GetEntity()->GetCurrentPhysicsInst();

				probeDesc.SetIncludeInstance(pHitInst);

				bool bSuccess = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
				if(bSuccess)
				{
					normalWorld = (*probe_results)[0].GetHitNormal();

					worldPosition = (*probe_results)[0].GetHitPosition();

					directionWorld = entityPosition - worldPosition;
					directionWorld.Normalize();

					materialId = (*probe_results)[0].GetMaterialId();
					componentId = (*probe_results)[0].GetHitComponent();
				}

				//grcDebugDraw::Sphere(worldPosition,0.1f,Color_red,false,10000);
				//grcDebugDraw::Line(origWorldPosition,entityPosition,Color_red,Color_red,10000);

				// we're not projecting onto a smash group - make sure the object is not smashable if we've got a glass collision
				if (!PGTAMATERIALMGR->GetIsSmashableGlass(materialId))
				{
					// set up the collision info structure
					VfxCollInfo_s vfxCollInfo;
					vfxCollInfo.regdEnt = GetEntity();
					vfxCollInfo.vPositionWld = VECTOR3_TO_VEC3V(worldPosition);
					vfxCollInfo.vNormalWld =  VECTOR3_TO_VEC3V(normalWorld);
					vfxCollInfo.vDirectionWld = VECTOR3_TO_VEC3V(directionWorld);

					vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(materialId);
					vfxCollInfo.roomId = 0;
					vfxCollInfo.componentId = componentId;
					vfxCollInfo.weaponGroup = (j == WEAPON_NETWORK_EFFECT_GROUP_SHOTGUN) ? WEAPON_EFFECT_GROUP_SHOTGUN : WEAPON_EFFECT_GROUP_PISTOL_SMALL;
					vfxCollInfo.force = VEHICLEGLASSFORCE_WEAPON_IMPACT;
					vfxCollInfo.isBloody = false;
					vfxCollInfo.isWater = false;
					vfxCollInfo.isExitFx = false;
					vfxCollInfo.noPtFx = true;
					vfxCollInfo.noPedDamage = true;
					vfxCollInfo.noDecal = false;
					vfxCollInfo.isSnowball = false;
					vfxCollInfo.isFMJAmmo = false;

					VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(vfxCollInfo.materialId);
					VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(vfxGroup, vfxCollInfo.weaponGroup, vfxCollInfo.isBloody);

					//If addWeaponImpactRetc return 0 here something is bad - so the outer loop will bail appropriately
					addWeaponImpactRetc = g_decalMan.AddWeaponImpact(*pFxWeaponInfo, vfxCollInfo);					
				}

				//Always decrement the count - if we found glass it just skips one (decrementing) - if we hit it will properly decrement
				DecrementApplyWeaponImpactPointLocationCount(i,(eNetworkEffectGroup)j);

				count = ExtractWeaponImpactPointLocationCount(i,(eNetworkEffectGroup)j,true);
			}
		}
	}

	m_ApplyWeaponImpactPointLocationProcessCount++;

	//Ensure that we terminate if the all the processing above completes and we have a positive addWeaponImpactRetc, or we have processed the apply several times and haven't completed - as a fallback.
	if (addWeaponImpactRetc || (m_ApplyWeaponImpactPointLocationProcessCount > 30))
		m_ApplyWeaponImpactPointLocationSet = false;

}

void CNetObjVehicle::GetWeaponImpactPointInformation(CVehicleDamageStatusDataNode& data)
{
	data.m_weaponImpactPointLocationSet = m_weaponImpactPointLocationSet;

	if (!data.m_weaponImpactPointLocationSet)
		return;

	for(int i=0; i < NUM_NETWORK_DAMAGE_DIRECTIONS; i++)
	{
		data.m_weaponImpactPointLocationCounts[i] = m_weaponImpactPointLocationCounts[i];
	}
}

void CNetObjVehicle::SetWeaponImpactPointInformation(const CVehicleDamageStatusDataNode& data)
{
	if (m_weaponImpactPointLocationSet && !data.m_weaponImpactPointLocationSet)
	{
		ClearWeaponImpactPointInformation();
		return;
	}

	m_weaponImpactPointLocationSet = data.m_weaponImpactPointLocationSet;

	if (!m_weaponImpactPointLocationSet)
		return;

	//receiving new impact information - calculate the delta between what we have for actual hit information here vs received.
	//place the delta into the applyweaponimpactpointlocationcounts for application
	//set the actual to what was received from the owner.
	bool bPositiveImpacts = false;
	for(int i=0; i < NUM_NETWORK_DAMAGE_DIRECTIONS; i++)
	{
		if (data.m_weaponImpactPointLocationCounts[i] > m_weaponImpactPointLocationCounts[i])
		{
			m_ApplyWeaponImpactPointLocationCounts[i] += data.m_weaponImpactPointLocationCounts[i] - m_weaponImpactPointLocationCounts[i];
		}

		if (m_ApplyWeaponImpactPointLocationCounts[i] > 0)
			bPositiveImpacts = true;

		m_weaponImpactPointLocationCounts[i] = data.m_weaponImpactPointLocationCounts[i];
	}

	if (bPositiveImpacts)
	{
		m_ApplyWeaponImpactPointLocationSet |= bPositiveImpacts;

		//reset this as we are receiving new data - but still want to retain the fallback
		m_ApplyWeaponImpactPointLocationProcessCount = 0;
	}
}

void CNetObjVehicle::ProcessFailmark()
{
#if !__NO_OUTPUT
	CVehicle* pVehicle = GetVehicle();
	if (pVehicle)
	{
		const char* DisplayVehicleTypes[] = 
		{
			"VEHICLE_TYPE_CAR",
			"VEHICLE_TYPE_PLANE",
			"VEHICLE_TYPE_TRAILER",
			"VEHICLE_TYPE_QUADBIKE",
			"VEHICLE_TYPE_DRAFT",
			"VEHICLE_TYPE_SUBMARINECAR",
			"VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE",
			"VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE",
			"VEHICLE_TYPE_HELI",
			"VEHICLE_TYPE_BLIMP",
			"VEHICLE_TYPE_AUTOGYRO",
			"VEHICLE_TYPE_BIKE",
			"VEHICLE_TYPE_BICYCLE",
			"VEHICLE_TYPE_BOAT",
			"VEHICLE_TYPE_TRAIN",
			"VEHICLE_TYPE_SUBMARINE",
		};

		const char* gamertag_owner_vehicle = pVehicle->GetNetworkObject() && pVehicle->GetNetworkObject()->GetPlayerOwner() ? pVehicle->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";

		VehicleType eVehicleType = pVehicle->GetVehicleType();
		Displayf("-->pVehicle[%p] modelname[%s] modelid[%u] networkname[%s] vehicletype[%s] %s %s",
			pVehicle,
			pVehicle->GetModelName(),
			pVehicle->GetModelId().ConvertToU32(),
			pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "",
			(eVehicleType > VEHICLE_TYPE_NONE) && (eVehicleType < VEHICLE_TYPE_NUM) ?  DisplayVehicleTypes[pVehicle->GetVehicleType()] : "",
			pVehicle->IsNetworkClone() ? "REMOTE" : "LOCAL",
			gamertag_owner_vehicle);

#if __BANK
		Displayf("   poptype[%s]",CTheScripts::GetPopTypeName(pVehicle->PopTypeGet()));
#endif

		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			const char* gamertag_owner_driver = pDriver->GetNetworkObject() && pDriver->GetNetworkObject()->GetPlayerOwner() ? pDriver->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
			Displayf("   pdriver[%p] isplayer[%d] gamertag_owner_driver[%s]",pDriver,pDriver->IsPlayer(),gamertag_owner_driver);
		}
	
		Displayf("   bodycolour[%u %u %u %u %u %u] dirtlevel[%u]",pVehicle->GetBodyColour1(),pVehicle->GetBodyColour2(),pVehicle->GetBodyColour3(),pVehicle->GetBodyColour4(),pVehicle->GetBodyColour5(),pVehicle->GetBodyColour6(),pVehicle->GetBodyDirtLevelUInt8());

		CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
		if (pShader && pShader->GetIsPrimaryColorCustom())
		{
			Color32 col = pShader->GetCustomPrimaryColor();
			Displayf("   primarycustomcolor[%u %u %u]",col.GetRed(),col.GetGreen(),col.GetBlue());
		}

		if (pShader && pShader->GetIsSecondaryColorCustom())
		{
			Color32 col = pShader->GetCustomSecondaryColor();
			Displayf("   secondarycustomcolor[%u %u %u]",col.GetRed(),col.GetGreen(),col.GetBlue());
		}

		if (pShader)
		{
			Displayf("   enveffscaleu8[%u]",pShader->GetEnvEffScaleU8());
		}

		Displayf("   ispersonalvehicle[%d]",pVehicle->IsPersonalVehicle());

		Displayf("   m_nDisableExtras[0x%x]",pVehicle->m_nDisableExtras);

		Displayf("   m_nPhysicalFlags.bRenderScorched[%d]",pVehicle->m_nPhysicalFlags.bRenderScorched);

		Displayf("   loddummymode[%d]",(int)pVehicle->GetVehicleAiLod().GetDummyMode());

		Displayf("   windowtint[%u]",pVehicle->GetVariationInstance().GetWindowTint());

		Displayf("   smokecolor[%u %u %u]",pVehicle->GetVariationInstance().GetSmokeColorR(),pVehicle->GetVariationInstance().GetSmokeColorG(),pVehicle->GetVariationInstance().GetSmokeColorB());

		Displayf("   liveryid[%d]",pVehicle->GetLiveryId());

		Displayf("   livery2id[%d]",pVehicle->GetLivery2Id());

		Displayf("   kitindex[%d]",pVehicle->GetVariationInstance().GetKitIndex());

		Displayf("   engineOn[%d] engineStarting[%d] engineSkipEngineStartup[%d] sirenOn[%d] pretendOcupants[%d] runningRespotTimer[%d] isDriveable[%d]"
			,pVehicle->m_nVehicleFlags.bEngineOn
			,pVehicle->m_nVehicleFlags.bEngineStarting
			,pVehicle->m_nVehicleFlags.bSkipEngineStartup
			,pVehicle->m_nVehicleFlags.GetIsSirenOn()
			,pVehicle->m_nVehicleFlags.bUsingPretendOccupants
			,pVehicle->IsBeingRespotted()
			,pVehicle->m_nVehicleFlags.bIsThisADriveableCar);

		Displayf("   throttle[%f] m_LastThrottleReceived[%f]",pVehicle->GetThrottle(),m_LastThrottleReceived);

		s32 maxSeats = pVehicle->GetSeatManager()->GetMaxSeats();
		for(int i=0; i < maxSeats; i++)
		{
			CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(i);
			if (pPedInSeat)
				Displayf("   pedInSeat[%d][%p]",i,pPedInSeat);
		}

		eCarLockState vehicleLockState = pVehicle->GetCarDoorLocks();
#if __BANK
		const char* vehicleLockString = VehicleLockStateStrings[vehicleLockState];
#else
		const char* vehicleLockString = "";
#endif
		Displayf("   doorLockState[%d][%s] isStationary[%d] forceOtherVehsToStop[%d] donttrytoenterthisvehicleiflockedforplayer[%d]"
			,static_cast<u32>(vehicleLockState)
			,vehicleLockString
			,pVehicle->m_nVehicleFlags.bIsThisAStationaryCar
			,pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle
			,pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer);

		Displayf("   nodamagefromexplosionsownedbydriver[%d] flaggedForCleanup[%d]"
			,pVehicle->GetNoDamageFromExplosionsOwnedByDriver(),pVehicle->m_nVehicleFlags.bReadyForCleanup);

		Displayf("   bAnimateJoints[%d]",pVehicle->m_nVehicleFlags.bAnimateJoints);

		for (s32 i=0; i<pVehicle->GetNumDoors(); i++)
		{
			CCarDoor* pDoor = pVehicle->GetDoor(i);
			if (pDoor)
			{
				bool bIsPedUsingDoor		= pVehicle->IsPedUsingDoor( pDoor );
				bool bIsPedRemoteUsingDoor	= pDoor->GetPedRemoteUsingDoor();

				bool bDoorBroken = !pDoor->GetIsIntact(pVehicle);
				bool bDoorOpen = false;

				if (!pDoor->GetIsClosed())
				{
					bDoorOpen = true;
				}

				eCarLockState doorLockState = pDoor->m_eDoorLockState;
#if __BANK
				const char* vehicleLockString = (doorLockState < SIZEOF_NUM_DOORS) ? VehicleLockStateStrings[doorLockState] : "";
#else
				const char* vehicleLockString = "";
#endif

				if (i<STANDARD_FOUR_DOOR && pVehicle->IsNetworkClone())
				{
					bool bRemoteDoorOpen = (m_remoteDoorOpen & ( 1 << ( i ) ) ) ? true : false;
					Displayf("   door[%d][%p] broken[%d] open[%d] doorratio[%f] targetratio[%f] pedisusingdoor[%d] pedremoteusingdoor[%d] remotedooropen[%d] m_eDoorLockState[%u][%s]",i,pDoor,bDoorBroken,bDoorOpen,pDoor->GetDoorRatio(),pDoor->GetTargetDoorRatio(),bIsPedUsingDoor,bIsPedRemoteUsingDoor,bRemoteDoorOpen,doorLockState,vehicleLockString);
				}
				else
				{
					Displayf("   door[%d][%p] broken[%d] open[%d] doorratio[%f] targetratio[%f] pedisusingdoor[%d] pedremoteusingdoor[%d] m_eDoorLockState[%u][%s]",i,pDoor,bDoorBroken,bDoorOpen,pDoor->GetDoorRatio(),pDoor->GetTargetDoorRatio(),bIsPedUsingDoor,bIsPedRemoteUsingDoor,doorLockState,vehicleLockString);
				}
			}
		}

		Displayf("   lightson[%d] headlightfullbeamon[%d] overridelights[%d] convertibleroofstate[%d]"
			,pVehicle->m_nVehicleFlags.bLightsOn
			,pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn
			,pVehicle->m_OverrideLights
			,pVehicle->GetConvertibleRoofState());

		Displayf("   wrecked[%d] status[%d] health[%f] maxhealth[%f] enginehealth[%f] petroltankhealth[%f] bodyhealth[%f] numwheels[%d] aretyresdamaged[%d] suspensiondamaged[%d] isonfire[%d] isonitsside[%d] isupsidedown[%d]"
			,(pVehicle->GetStatus() == STATUS_WRECKED)
			,pVehicle->GetStatus()
			,pVehicle->GetHealth()
			,pVehicle->GetMaxHealth()
			,pVehicle->m_Transmission.GetEngineHealth()
			,pVehicle->GetVehicleDamage()->GetPetrolTankHealth()
			,pVehicle->GetVehicleDamage()->GetBodyHealth()
			,pVehicle->GetNumWheels()
			,pVehicle->AreTyresDamaged()
			,pVehicle->IsSuspensionDamaged()
			,pVehicle->IsOnFire()
			,pVehicle->IsOnItsSide()
			,pVehicle->IsUpsideDown());

		Displayf("   bGetIsVisible[%d] bIsOnScreen[%d] alpha[%d]",
			pVehicle->GetIsVisible(),
			pVehicle->GetIsOnScreen(),
			pVehicle->GetAlpha());

		const Vector3 pos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		Displayf("   pos[%f %f %f] IsInAir[%d]",pos.x,pos.y,pos.z,pVehicle->IsInAir());

		const Vector3 vel = pVehicle->GetVelocity();
		Displayf("   velocity[%f %f %f][%f]",vel.x,vel.y,vel.z,vel.Mag());
	}
#endif
}



u32 CNetObjVehicle::GetSizeOfVehicleGadgetData(eVehicleGadgetType gadgetType)
{
	u32 size = 0;

	CVehicleGadget* pGadget = GetDummyVehicleGadget(gadgetType);

	if (pGadget)
	{
		CSyncDataSizeCalculator sizeSerialiser;

		pGadget->Serialise(sizeSerialiser);

		size = sizeSerialiser.GetSize();
	}

	gnetAssertf(size<=MAX_VEHICLE_GADGET_DATA_SIZE, "Data size for net vehicle gadget (%d) exceeds max size (%d)", size, MAX_VEHICLE_GADGET_DATA_SIZE);

	return size;
}

void CNetObjVehicle::SetSteeringAngle(const CVehicleSteeringDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    CNetBlenderVehicle *netBlender = SafeCast(CNetBlenderVehicle, GetNetBlender());

	float steeringAngle = data.m_steeringAngle;
	if (IsClose(steeringAngle, 0.0f, 0.01f))
	{
		steeringAngle = 0.0f;
	}

    if(netBlender)
    {
        netBlender->SetTargetSteerAngle(steeringAngle);
    }

    if(!netBlender || !CanBlend(0))
    {
        pVehicle->m_vehControls.SetSteerAngle(steeringAngle);
    }
}

void CNetObjVehicle::SetVehicleControlData(const CVehicleControlDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);
    
	bool bNitrousStateDirty = false;
	bool bNitrousActive = pVehicle->GetNitrousActive();
	bool bShouldNitrousBeActive = data.m_bNitrousActive;
	if (bNitrousActive != bShouldNitrousBeActive)
	{
		pVehicle->SetNitrousActive(bShouldNitrousBeActive);
		bNitrousStateDirty = true;
	}

	bool bNitrousOverrideActive = pVehicle->GetOverrideNitrousLevel();
	bool bShouldNitrousOverrideBeActive = data.m_bIsNitrousOverrideActive;
	if (bNitrousOverrideActive != bShouldNitrousBeActive)
	{
		pVehicle->SetOverrideNitrousLevel(bShouldNitrousOverrideBeActive);
		bNitrousStateDirty = true;
	}

	if (bNitrousStateDirty)
	{
		if (!bShouldNitrousBeActive && !bShouldNitrousOverrideBeActive)
		{
			audVehicleAudioEntity* pAudioEntity = pVehicle->GetVehicleAudioEntity();
			if (pAudioEntity)
			{
				pAudioEntity->SetBoostActive(false);
			}
		}
	}

    pVehicle->m_vehControls.m_throttle = data.m_throttle;
    pVehicle->m_vehControls.m_brake    = data.m_brakePedal;

	pVehicle->SetKERS(data.m_kersActive);
	
	if(pVehicle->InheritsFromAutomobile())
	{
		SafeCast(CAutomobile, pVehicle)->SetInBurnout(data.m_isInBurnout);
	}

	if (data.m_isSubCar)
	{
		CSubmarineCar* pSubCar = static_cast<CSubmarineCar*>(pVehicle);
		gnetAssert(pSubCar);
		CSubmarineHandling* subHandling = pSubCar->GetSubHandling();
		gnetAssert(subHandling);

		subHandling->SetYawControl(data.m_subCarYaw);
		subHandling->SetPitchControl(data.m_SubCarPitch);
		subHandling->SetDiveControl(data.m_SubCarDive);
	}	

	// Cache lowrider wheel suspension values. Applied in CNetObjVehicle::ProcessControl (as this function isn't called every frame once we've stopped adjusting the suspension).
	m_bModifiedLowriderSuspension  = data.m_bModifiedLowriderSuspension;
	m_bAllLowriderHydraulicsRaised = data.m_bAllLowriderHydraulicsRaised;
	
	for(int i = 0; i < data.m_numWheels; i++)
	{
		CWheel *pWheel = pVehicle->GetWheel(i);
		if (pWheel && gnetVerifyf(i < NUM_VEH_CWHEELS_MAX, "Vehicle has more than the expected number of wheels!"))
		{
			m_fLowriderSuspension[i] = data.m_fLowriderSuspension[i];		
		}
	}

	if(pVehicle->InheritsFromAutomobile())
	{
		SafeCast(CAutomobile, pVehicle)->SetReducedSuspensionForce(data.m_reducedSuspensionForce);
	}	

	if(data.m_bPlayHydraulicsBounceSound)
	{
		pVehicle->TriggerHydraulicBounceSound();
	}
	if(data.m_bPlayHydraulicsActivationSound)
	{
		pVehicle->TriggerHydraulicActivationSound();
	}
	if(data.m_bPlayHydraulicsDeactivationSound)
	{
		pVehicle->TriggerHydraulicDeactivationSound();
	}

	pVehicle->SetRemoteDriverDoorClosing(data.m_bIsClosingAnyDoor);

    m_LastThrottleReceived = data.m_throttle;

    if(data.m_roadNodeAddress == 0)
    {
        if(!m_RoadNodeHistory[NODE_NEW].IsEmpty())
        {
            ResetRoadNodeHistory();
        }
    }
	else if (m_pCloneAITask)
	{
		CNodeAddress nodeAddr;
		nodeAddr.FromInt(data.m_roadNodeAddress);

        if(m_RoadNodeHistory[NODE_NEW].IsEmpty())
        {
            // this is the first node we have received, we need to guess at the old nodes based on node links
            // and vehicle orientation
            const unsigned NUM_OLD_NODES_TO_ESTIMATE = NODE_NEW;
            const CNodeAddress *estimatedNodes[NUM_OLD_NODES_TO_ESTIMATE];

            const CNodeAddress *currentNodeAddr = &nodeAddr;
            for(unsigned index = 0; index < NUM_OLD_NODES_TO_ESTIMATE && currentNodeAddr; index++)
            {
                const CNodeAddress *estimatedOldNode = EstimateOldRoadNode(*currentNodeAddr);

                estimatedNodes[NUM_OLD_NODES_TO_ESTIMATE - index - 1] = estimatedOldNode;
                currentNodeAddr = estimatedOldNode;
            }

            if(currentNodeAddr)
            {
                for(unsigned index = 0; index < NUM_OLD_NODES_TO_ESTIMATE; index++)
                {
                    const CNodeAddress *estimatedOldNode = estimatedNodes[index];

                    if(gnetVerifyf(estimatedOldNode, "Unexpectedly found NULL estimated node!"))
                    {
                        AddRoadNodeToHistory(*estimatedOldNode);
                    }
                }
            }
            else
            {
                // failed finding links to the newly received node, clear the history, so the clone task does
                // a join to the road network when next updated
                ResetRoadNodeHistory();
            }
        }

        if(m_RoadNodeHistory[NODE_NEW] != nodeAddr)
        {
            // search future nodes for the new node
            unsigned numNodesToShuffle = 0;
            const unsigned FUTURE_NODE_START = NODE_NEW + 1;
            for(unsigned index = FUTURE_NODE_START; index < ROAD_NODE_HISTORY_SIZE && (numNodesToShuffle == 0); index++)
            {
                if(m_RoadNodeHistory[index] == nodeAddr)
                {
                    numNodesToShuffle = index - FUTURE_NODE_START + 1;
                }
            }

            if(numNodesToShuffle > 0)
            {
                for(unsigned index = 0; index < ROAD_NODE_HISTORY_SIZE - numNodesToShuffle; index++)
                {
                    m_RoadNodeHistory[index] = m_RoadNodeHistory[index + numNodesToShuffle];
                }

                bool canAddFutureNode = true;
                for(unsigned index = ROAD_NODE_HISTORY_SIZE - numNodesToShuffle; index < ROAD_NODE_HISTORY_SIZE && canAddFutureNode; index++)
                {
                    const CNodeAddress *estimatedFutureNode = EstimateFutureRoadNode(m_RoadNodeHistory[index - 1], m_pCloneAITask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRUISE_NEW);

                    if(estimatedFutureNode == 0)
                    {
                        m_RoadNodeHistory[index].SetEmpty();
                        canAddFutureNode = false;
                    }
                    else
                    {
                        m_RoadNodeHistory[index] = *estimatedFutureNode;
                    }
                }
            }
            else
            {
                bool addRoadNode = true;

                const CPathNode *currentNode = ThePaths.FindNodePointerSafe(m_RoadNodeHistory[NODE_NEW]);
                const CPathNode *newNode     = ThePaths.FindNodePointerSafe(nodeAddr);

                if(currentNode == 0 || newNode == 0)
                {
                    addRoadNode = false;
                }
                else if(m_RoadNodeHistory[NODE_NEW].IsEmpty() || !ThePaths.These2NodesAreAdjacent(m_RoadNodeHistory[NODE_NEW], nodeAddr))
                {
                    addRoadNode = AddSkippedRoadNodesToHistory(nodeAddr);
                }

                if(addRoadNode)
                {
                    AddRoadNodeToHistory(nodeAddr);

                    bool canAddFutureNode = true;
                    for(unsigned futureNodeIndex = FUTURE_NODE_START; futureNodeIndex < ROAD_NODE_HISTORY_SIZE && canAddFutureNode; futureNodeIndex++)
                    {
                        const CNodeAddress *estimatedFutureNode = EstimateFutureRoadNode(m_RoadNodeHistory[futureNodeIndex - 1], m_pCloneAITask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRUISE_NEW);

                        if(estimatedFutureNode == 0)
                        {
                            canAddFutureNode = false;

                            for(unsigned index = futureNodeIndex; index < ROAD_NODE_HISTORY_SIZE; index++)
                            {
                                m_RoadNodeHistory[futureNodeIndex].SetEmpty();
                            }
                        }
                        else
                        {
                            m_RoadNodeHistory[futureNodeIndex] = *estimatedFutureNode;
                        }
                    }
                }
                else
                {
                    // failed finding links to the newly received node, clear the history, so the clone task does
                    // a join to the road network when next updated
                    ResetRoadNodeHistory();
                }
            }

            UpdateRouteForClone();
        }
	}

	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase*>(pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_SECONDARY));

	if (data.m_bringVehicleToHalt)
	{
		if (!pTask)
		{
			CTaskBringVehicleToHalt* pHaltTask = rage_new CTaskBringVehicleToHalt(data.m_BVTHStoppingDist, 1, data.m_BVTHControlVertVel);

			pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_SECONDARY, pHaltTask, VEHICLE_TASK_SECONDARY_ANIM, true);
		}
	}
	else if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_BRING_VEHICLE_TO_HALT)
	{
		if (!pTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
		{
			if (!pTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
			{	
				gnetAssertf(0, "Failed to abort CTaskBringVehicleToHalt on %s", GetLogName());
			}
		}
	}

	if(data.m_HasTopSpeedPercentage)
	{
		CVehicleFactory::ModifyVehicleTopSpeed(pVehicle, data.m_topSpeedPercent);
	}

	if (data.m_HasTargetGravityScale)
	{
		pVehicle->SetTargetGravityScale(data.m_TargetGravityScale);
		pVehicle->SetCachedStickY(data.m_StickY);
	}
	else
	{
		pVehicle->SetTargetGravityScale(0.0f);
	}
}

void CNetObjVehicle::SetVehicleAppearanceData(const CVehicleAppearanceDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	if (!pVehicle)
		return;

    pVehicle->SetBodyColour1(data.m_bodyColour1);
    pVehicle->SetBodyColour2(data.m_bodyColour2);
    pVehicle->SetBodyColour3(data.m_bodyColour3);
    pVehicle->SetBodyColour4(data.m_bodyColour4);
	pVehicle->SetBodyColour5(data.m_bodyColour5);
	pVehicle->SetBodyColour6(data.m_bodyColour6);

	pVehicle->SetBodyDirtLevel((float)data.m_bodyDirtLevel);

	CCustomShaderEffectVehicle* pShader = pVehicle->GetDrawHandlerPtr() ? static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect()) : NULL;
	if (pShader)
	{
		if (data.m_customPrimaryColor)
		{
			const Color32 col(data.m_customPrimaryR,data.m_customPrimaryG,data.m_customPrimaryB);
			pShader->SetCustomPrimaryColor(col);
		}
		else
		{
			pShader->ClearCustomPrimaryColor();
		}

		if (data.m_customSecondaryColor)
		{
			const Color32 col(data.m_customSecondaryR,data.m_customSecondaryG,data.m_customSecondaryB);
			pShader->SetCustomSecondaryColor(col);
		}
		else
		{
			pShader->ClearCustomSecondaryColor();
		}
	}

	// check for and remove any invalid extras
	u32 nAdjustedExtras = data.m_disableExtras;
	for(int i = VEH_EXTRA_1; i <= VEH_LAST_EXTRA; i++)
    {
		u32 nExtraBit = BIT(i - VEH_EXTRA_1 + 1);
		if(nAdjustedExtras & nExtraBit)
        {
			eHierarchyId nExtraID = static_cast<eHierarchyId>(i);
			if(!gnetVerifyf(pVehicle->HasComponent(nExtraID), "Invalid vehicle component! nExtraID[%d] pVehicle[%p][%s][%s]",nExtraID,pVehicle,pVehicle->GetModelName(),GetLogName()))
				nAdjustedExtras &= ~nExtraBit;
        }
    }

	// check if the disabled vehicle extras are different to what we know
    if(pVehicle->m_nDisableExtras != nAdjustedExtras) 
    {
		weaponDebugf3("CNetObjVehicle::SetVehicleAppearanceData--(pVehicle->m_nDisableExtras[0x%x] != data.m_disableExtras[0x%x])-->Fix",pVehicle->m_nDisableExtras,nAdjustedExtras);
		vehicleDebugf3("CNetObjVehicle::SetVehicleAppearanceData--(pVehicle->m_nDisableExtras[0x%x] != data.m_disableExtras[0x%x])-->Fix",pVehicle->m_nDisableExtras,nAdjustedExtras);

		pVehicle->m_nDisableExtras = nAdjustedExtras;

		pVehicle->UpdateExtras();

		//cache off these values
		s32 currentStatus = pVehicle->GetStatus();
		bool bRenderScorched = pVehicle->m_nPhysicalFlags.bRenderScorched;

		//ensure the Fix happens, override the STATUS temporarily from WRECKED so the fragments are updated - status will then be correctly updated below after the Fix
		if (currentStatus == STATUS_WRECKED)
			pVehicle->SetStatus(STATUS_PLAYER); 

		pVehicle->Fix(true,true);

		//ensure these values are set as before
		pVehicle->SetStatus(currentStatus);
		pVehicle->m_nPhysicalFlags.bRenderScorched = bRenderScorched;
	}

	if (pShader)
	{
		pShader->SetEnvEffScaleU8(data.m_envEffScale);
	}

	if (pVehicle->GetVariationInstance().GetWindowTint() != data.m_windowTint)
	{
		pVehicle->GetVariationInstance().SetWindowTint(data.m_windowTint);
	}

	if (data.m_bSmokeColor)
	{
		pVehicle->GetVariationInstance().SetSmokeColorR(data.m_smokeColorR);
		pVehicle->GetVariationInstance().SetSmokeColorG(data.m_smokeColorG);
		pVehicle->GetVariationInstance().SetSmokeColorB(data.m_smokeColorB);
	}

	if (data.m_hasLiveryID)
	    pVehicle->SetLiveryId(data.m_liveryID);

	if (data.m_hasLivery2ID)
	    pVehicle->SetLivery2Id(data.m_livery2ID);
    
	pVehicle->UpdateBodyColourRemapping();

	// convert back to correct kit index
	u16 nKitIndex = (data.m_kitIndex == 0) ? INVALID_VEHICLE_KIT_INDEX : data.m_kitIndex - 1;

	if (nKitIndex != INVALID_VEHICLE_KIT_INDEX && !pVehicle->GetVehicleModelInfo())
	{
		nKitIndex = INVALID_VEHICLE_KIT_INDEX;
	}

	if (nKitIndex != INVALID_VEHICLE_KIT_INDEX && pVehicle->GetVehicleModelInfo() && !pVehicle->GetVehicleModelInfo()->GetVehicleColours())
	{
		nKitIndex = INVALID_VEHICLE_KIT_INDEX;
	}

	if (nKitIndex != INVALID_VEHICLE_KIT_INDEX && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleColours() && !gnetVerifyf(nKitIndex < pVehicle->GetVehicleModelInfo()->GetVehicleColours()->m_Kits.GetCount(), "Invalid kit index passed into CNetObjVehicle::SetVehicleAppearanceData()"))
	{
		nKitIndex = INVALID_VEHICLE_KIT_INDEX;
	}

	// if not invalid, retrieve the real index
	if(nKitIndex != INVALID_VEHICLE_KIT_INDEX && pVehicle->GetVehicleModelInfo())
	{
		CVehicleModelInfo* pMi = pVehicle->GetVehicleModelInfo();
		if (pMi)
		{
			if (nKitIndex < pMi->GetNumModKits())
			{
				//retrieve the real index
				nKitIndex = pMi->GetModKit(nKitIndex);

				if (!pMi->GetVehicleColours() || !gnetVerifyf(nKitIndex < pMi->GetVehicleColours()->m_Kits.GetCount(), "Invalid kit index returned from GetModKit()"))
				{
					nKitIndex = INVALID_VEHICLE_KIT_INDEX;
				}
			}
			else
			{
				nKitIndex = INVALID_VEHICLE_KIT_INDEX;
			}
		}
		else
		{
			nKitIndex = INVALID_VEHICLE_KIT_INDEX;
		}
	}

	if (nKitIndex != INVALID_VEHICLE_KIT_INDEX)
	{
		// apply kit index
		pVehicle->GetVariationInstance().SetKitIndex(nKitIndex);
	}

	//apply wheel type
	if (data.m_wheelType != 255)
		pVehicle->GetVariationInstance().SetWheelType((eVehicleWheelType)data.m_wheelType);

	const CVehicleKit* pKit = pVehicle->GetVariationInstance().GetKit();
	if(pKit)
	{
		// verify that we haven't blown the limit
		int nVisibleMods = pKit->GetVisibleMods().GetCount();
		int nStatsMods = pKit->GetStatMods().GetCount();
		
		// make a note of what mods have been applied
		const int NUM_KIT_MODS = VMT_MAX;
		bool bModApplied[NUM_KIT_MODS];
		for(int i = 0; i < NUM_KIT_MODS; i++)
			bModApplied[i] = false;

		// apply kit variations
		bool bHasKitMod = false;
		for(unsigned i = 0; i < NUM_KIT_MOD_SYNC_VARIABLES; i++)
		{
			if(data.m_allKitMods[i] != 0)
			{
				bHasKitMod = true;
				break;
			}
		}

		if(bHasKitMod)
		{
			for(unsigned i = 0; i < nVisibleMods + nStatsMods; i++)
			{
				unsigned index = (unsigned)(i / SIZEOF_KIT_MODS_U32);
				
				u32 currentMask = data.m_allKitMods[index];
				u32 shiftValue = i - (index * SIZEOF_KIT_MODS_U32);
				
				if(i > (SIZEOF_KIT_MODS * SIZEOF_KIT_MODS_U32))
				{
					gnetAssertf(0,"CNetObjVehicle::SetVehicleAppearanceData--SIZEOF_KIT_MODS[%d] exceeds max serialized bits %d -- code needs to add another m_kitModsX -- inform default network code",SIZEOF_KIT_MODS,128);
				}

				if(currentMask & (1 << shiftValue))
				{
					// visible mods
					if(i < nVisibleMods)
					{
						u8 nIndex = static_cast<u8>(i);
						eVehicleModType modType = static_cast<eVehicleModType>(pKit->GetVisibleMods()[nIndex].GetType());

						if (pVehicle->GetVariationInstance().GetModIndex(modType) != nIndex)
						{
							pVehicle->GetVariationInstance().SetModIndex(modType, nIndex, pVehicle, false);							
						}
						if (gnetVerifyf(modType < NUM_KIT_MODS, "CNetObjVehicle::SetVehicleAppearanceData--i[%d] nVisibleMods[%d] modType[%d] >= NUM_KIT_MODS[%d] ignore setting bModApplied - would be out of bounds",i,nVisibleMods,modType,NUM_KIT_MODS))
						{
							bModApplied[modType] = true;
						}
					}
					// stats mods
					else if(i < nVisibleMods + nStatsMods)
					{
						u8 nIndex = static_cast<u8>(i - nVisibleMods);
						if (gnetVerifyf(nIndex < nStatsMods,"nIndex[%d] is >= max stats mod[%d] vehicle[%s]",nIndex,nStatsMods,pVehicle->GetModelName()))
						{
							eVehicleModType modType = static_cast<eVehicleModType>(pKit->GetStatMods()[nIndex].GetType());
							pVehicle->GetVariationInstance().SetModIndex(modType, nIndex, pVehicle, false);
							if (gnetVerifyf(modType < NUM_KIT_MODS, "CNetObjVehicle::SetVehicleAppearanceData--i[%d] nIndex[%d] nStatsMods[%d] modType[%d] >= NUM_KIT_MODS[%d] ignore setting bModApplied - would be out of bounds",i,nIndex,nStatsMods,modType,NUM_KIT_MODS))
							{
								bModApplied[modType] = true;
							}
						}
					}
				}
			}
		}

		// turn off any kit modifications that have been removed
		for(int i = 0; i < NUM_KIT_MODS; i++)
		{
			eVehicleModType modType = static_cast<eVehicleModType>(i);
			if(pVehicle->GetVariationInstance().GetModIndex(modType) != INVALID_MOD && !bModApplied[i])
			{
				if(modType >= VMT_TOGGLE_MODS && modType < VMT_MISC_MODS)
				{
					pVehicle->GetVariationInstance().ToggleMod(modType, false);
				}
				else
				{
					pVehicle->GetVariationInstance().SetModIndex(modType, INVALID_MOD, pVehicle, false);
				}
			}
		}

		// apply toggle variations (make sure and switch off as well)
		for(int i = 0; i < VMT_TOGGLE_NUM; i++)
		{
			bool bToggleOn = (data.m_toggleMods & (1 << i)) != 0;
			pVehicle->GetVariationInstance().ToggleMod(static_cast<eVehicleModType>(i + VMT_TOGGLE_MODS), bToggleOn);
		}

		// convert back to correct wheel mod index
		u8 nWheelMod = (data.m_wheelMod == 0) ? INVALID_MOD : data.m_wheelMod - 1;
		u8 nRearWheelMod = (data.m_rearWheelMod == 0) ? INVALID_MOD : data.m_rearWheelMod - 1;

		// apply wheel mod		
		pVehicle->GetVariationInstance().SetModIndex(VMT_WHEELS, nWheelMod, pVehicle, data.m_wheelVariation0);

		if (data.m_hasDifferentRearWheel)
		{
			pVehicle->GetVariationInstance().SetModIndex(VMT_WHEELS_REAR_OR_HYDRAULICS, nRearWheelMod, pVehicle, data.m_wheelVariation0);
		}		
	}

	CCustomShaderEffectVehicle *pShaderEffectVehicle = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	if(pShaderEffectVehicle)
	{
		pShaderEffectVehicle->SetLicensePlateTexIndex( data.m_LicencePlateTexIndex );

		if( data.m_licencePlate[0] != '\0' )
		{
			char tempPlate[SIZEOF_LICENCE_PLATE + 1];
			memcpy( tempPlate, data.m_licencePlate, SIZEOF_LICENCE_PLATE );
			tempPlate[SIZEOF_LICENCE_PLATE] = '\0';
			pShaderEffectVehicle->SetLicensePlateText( tempPlate );
		}

		gnetDebug1("SetVehicleAppearanceData - SetLicensePlateText %s done for [%d] [%s]", data.m_licencePlate[0] == 0 ? "not" : "", data.m_LicencePlateTexIndex, data.m_licencePlate);
	}
#if !__NO_OUTPUT
	else
	{
		gnetDebug1("SetVehicleAppearanceData - SetLicensePlateText not done for [%d] [%s]", data.m_LicencePlateTexIndex, data.m_licencePlate);
	}
#endif //!__NO_OUTPUT

	if (pVehicle->GetVehicleAudioEntity())
		pVehicle->GetVehicleAudioEntity()->SetHornType(data.m_horntype);

	for(int i=0; i < CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES; ++i)
	{
	 	ProcessBadging(pVehicle,data.m_VehicleBadgeDesc,data.m_bVehicleBadgeData[i],data.m_VehicleBadgeData[i],i);
	}

	//@begin neon only used in NG
	if (data.m_neonOn)
	{
		const Color32 col(data.m_neonColorR,data.m_neonColorG,data.m_neonColorB);
		pVehicle->SetNeonColour(col);

		pVehicle->SetNeonLOn(data.m_neonLOn);
		pVehicle->SetNeonROn(data.m_neonROn);
		pVehicle->SetNeonFOn(data.m_neonFOn);
		pVehicle->SetNeonBOn(data.m_neonBOn);
		pVehicle->SetNeonSuppressionState(data.m_neonSuppressed);
	}
	else
	{
		pVehicle->SetNeonLOn(false);
		pVehicle->SetNeonROn(false);
		pVehicle->SetNeonFOn(false);
		pVehicle->SetNeonBOn(false);
	}
	//@end neon
}

void CNetObjVehicle::SetVehicleDamageStatusData(const CVehicleDamageStatusDataNode& data)
{
	weaponDebugf3("CNetObjVehicle::SetVehicleDamageStatusData m_WindowsBroken[%d %d %d %d %d %d]",data.m_windowsBroken[0],data.m_windowsBroken[1],data.m_windowsBroken[2],data.m_windowsBroken[3],data.m_windowsBroken[4],data.m_windowsBroken[5]);

	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	// save deformation data - this will be applied in the update function
    m_deformationDataFL = 0;
	m_deformationDataFR = 0;
	m_deformationDataML = 0;
	m_deformationDataMR = 0;
    m_deformationDataRL = 0;
    m_deformationDataRR = 0;

	if(data.m_hasDeformationDamage)
	{
		m_deformationDataFL = data.m_frontLeftDamageLevel;
		m_deformationDataFR = data.m_frontRightDamageLevel;
		m_deformationDataML = data.m_middleLeftDamageLevel;
		m_deformationDataMR = data.m_middleRightDamageLevel;
		m_deformationDataRL = data.m_rearLeftDamageLevel;
		m_deformationDataRR = data.m_rearRightDamageLevel;
	}

	SetWeaponImpactPointInformation(data);

	if (data.m_hasBrokenBouncing)
	{
		if (data.m_frontBumperState)
		{
			m_FrontBumperDamageState = data.m_frontBumperState;
			pVehicle->GetVehicleDamage()->SetPartDamage(VEH_BUMPER_F, data.m_frontBumperState);
		}

		if (data.m_rearBumperState)
		{
			m_RearBumperDamageState = data.m_rearBumperState;
			pVehicle->GetVehicleDamage()->SetPartDamage(VEH_BUMPER_R, data.m_rearBumperState);
		}
	}

	for(u32 light = 0; light < SIZEOF_LIGHT_DAMAGE; light++)
	{
		if(data.m_lightsBroken[light] && pVehicle->GetVehicleDamage()->GetLightStateImmediate(light) != data.m_lightsBroken[light])
		{
			int boneID = pVehicle->GetBoneIndex((eHierarchyId)(VEH_HEADLIGHT_L + light));

			if( boneID != -1 )
			{
				pVehicle->GetVehicleDamage()->SetLightStateImmediate(light, data.m_lightsBroken[light]);

				Vector3 localLightPos    = pVehicle->GetLocalMtx(boneID).d;
				Vector3 localLightNormal(0.0f, 1.0f, 0.0f);
				g_vfxVehicle.TriggerPtFxLightSmash(pVehicle,VEH_HEADLIGHT_L + light,RCC_VEC3V(localLightPos), RCC_VEC3V(localLightNormal));
				pVehicle->GetVehicleAudioEntity()->TriggerHeadlightSmash(VEH_HEADLIGHT_L + light,localLightPos);
				pVehicle->GetVehicleDamage()->SetUpdateLightBones(true);
			}
		}
	}


	for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
	{
		//ensure this window if broken on the remote is repaired.
		eHierarchyId windowId = (eHierarchyId)(VEH_WINDSCREEN + window);
		int boneID = pVehicle->GetBoneIndex(windowId);
		if( boneID != -1 )
		{
			const Matrix34 &mtxLocal = pVehicle->GetLocalMtx(boneID);
			const bool windowSmashed = (mtxLocal.a.Mag2() == 0.0f);

			const int component = pVehicle->GetFragInst() ? pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneID) : -1;
			
			if (component != -1)
			{			
				if(data.m_windowsBroken[window])
				{
					bool shouldBreakWindow = !pVehicle->IsBrokenFlagSet(component);

					if (shouldBreakWindow && data.m_hasArmouredGlass)
					{
						shouldBreakWindow = data.m_armouredWindowsHealth[window] <= 0;
					}

					if(shouldBreakWindow)
					{
						if (windowId==VEH_WINDSCREEN)
						{
							m_frontWindscreenSmashed = true;
						}
						else if (windowId==VEH_WINDSCREEN_R)
						{
							m_rearWindscreenSmashed = true;
						}

						if (!pVehicle->GetIsOnScreen())
						{
							weaponDebugf3("REMOTE invoke m_windowsBroken SmashCollision");

							const Vec3V smashNormal = pVehicle->GetWindowNormalForSmash(windowId);
							g_vehicleGlassMan.SmashCollision(pVehicle, component, VEHICLEGLASSFORCE_WEAPON_IMPACT, smashNormal);
							pVehicle->SetWindowComponentDirty(windowId);
						}
					}
				}
				else if (windowSmashed)
				{
					//repair it
					pVehicle->SetBrokenFlag(component); //ensure this is set otherwise the remove will not process properly.
					g_vehicleGlassMan.RemoveComponent(pVehicle,component); //broken on remote but not on the local-authority
					pVehicle->ClearBrokenFlag(component); //clear it
					pVehicle->SetWindowComponentDirty(windowId);
				}

				if (data.m_hasArmouredGlass && data.m_armouredWindowsHealth[window] >= 0.0f && data.m_armouredWindowsHealth[window] < CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH)
				{
					// Create the glass component here if it's armoured glass that has been damaged, but not destroyed
					CVehicleGlassComponent *pGlassComponent = const_cast<CVehicleGlassComponent*>(g_vehicleGlassMan.GetVehicleGlassComponent((CPhysical*)pVehicle, component));
					if (pGlassComponent == NULL)
					{
						pGlassComponent = g_vehicleGlassMan.CreateComponent(pVehicle, component);
					}						
					// Then set health and decal count
					if (pGlassComponent)
					{
						pGlassComponent->SetArmouredGlassHealth(data.m_armouredWindowsHealth[window]);
						pGlassComponent->SetArmouredGlassPenetrationDecalsCount(data.m_armouredPenetrationDecalsCount[window]);	
					}
					else
					{
						vehicleWarningf("CNetObjVehicle::SetVehicleDamageStatusData Glass component not created when trying to set armoured glass health and decals! This is bad!");
					}
				}
			}
		}
	}

	bool anySirenBroken = false;

	if(pVehicle->UsesSiren())
	{
		u32 sirenBone = VEH_SIREN_1;
		for(u32 siren = 0; siren < SIZEOF_SIREN_DAMAGE; siren++, sirenBone++)
		{
			pVehicle->GetVehicleDamage()->SetSirenState(VEH_SIREN_1 + siren, data.m_sirensBroken[siren]);

			if(data.m_sirensBroken[siren])
			{
				anySirenBroken = true;

				const int boneGlassID = pVehicle->GetBoneIndex((eHierarchyId)(sirenBone + (VEH_SIREN_GLASS_1-VEH_SIREN_1)));
				const int component = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneGlassID);
				if( component != -1 )
				{
					Vec3V vSmashNormal(V_Z_AXIS_WZERO);
					g_vehicleGlassMan.SmashCollision(pVehicle,component, VEHICLEGLASSFORCE_SIREN_SMASH, vSmashNormal);
					pVehicle->GetVehicleAudioEntity()->SmashSiren();
				}
			}
		}
	}

	if(anySirenBroken)
	{
		pVehicle->GetVehicleDamage()->SetUpdateSirenBones(anySirenBroken);
	}
}

void CNetObjVehicle::SetVehicleAITask(const CVehicleTaskDataNode& data)
{
	if (m_pCloneAITask && (u32)m_pCloneAITask->GetTaskType() != data.m_taskType)
	{
		delete m_pCloneAITask;
		m_pCloneAITask = NULL;
	}

	if (!m_pCloneAITask)
	{
		m_pCloneAITask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(data.m_taskType));

        CVehicle* pVehicle = GetVehicle();
        gnetAssert(pVehicle);

        if(m_pCloneAITask && pVehicle)
        {
            UpdateRouteForClone();
        }
	}

	if (m_pCloneAITask && data.m_taskDataSize>0)
	{
		u32 byteSizeOfTaskData = (data.m_taskDataSize+7)>>3;
		datBitBuffer messageBuffer;
		messageBuffer.SetReadOnlyBits(data.m_taskData, byteSizeOfTaskData<<3, 0);

		m_pCloneAITask->ReadTaskData(messageBuffer);
	}
}

void CNetObjVehicle::SetVehicleGadgetData(const CVehicleGadgetDataNode& data)
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	if (pVehicle)
	{
		for(int i = 0; i < data.m_NumGadgets; i++)
		{
			CVehicleGadget *pVehicleGadget = NULL;

			for(int j = 0; j < pVehicle->GetNumberOfVehicleGadgets(); j++)
			{
				pVehicleGadget = pVehicle->GetVehicleGadget(j);

				if (pVehicleGadget && pVehicleGadget->GetType() == (s32)data.m_GadgetData[i].Type)
				{
					break;
				}
			}			

			if (!pVehicleGadget)
			{
				CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();

				//Need to invoke creation of the weapon manager if it doesn't exist yet on the remote otherwise we will not be able to de-serialise the data below and will lose state
				if (!pWeaponMgr && pVehicle->GetStatus()!=STATUS_WRECKED)
				{
					pVehicle->CreateVehicleWeaponMgr();
					pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
				}
				
				if(pWeaponMgr)
				{
					for(int j = 0; j < pWeaponMgr->GetNumVehicleWeapons(); j++)
					{
						CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(j);

						if (pWeapon && pWeapon->GetType() == (s32)data.m_GadgetData[i].Type)
						{
							pVehicleGadget = (CVehicleGadget*) pWeapon;
							break;
						}
					}
				}
			}

			if (!pVehicleGadget)
			{
				pVehicleGadget = AddVehicleGadget(pVehicle, (eVehicleGadgetType)data.m_GadgetData[i].Type);
			}

			if (pVehicleGadget)
			{
				datBitBuffer messageBuffer;
				messageBuffer.SetReadOnlyBits(data.m_GadgetData[i].Data, IVehicleNodeDataAccessor::MAX_VEHICLE_GADGET_DATA_SIZE, 0);

				CSyncDataReader serialiser(messageBuffer);

				pVehicleGadget->Serialise(serialiser);
			}
		}
	}
}

void CNetObjVehicle::SetComponentReservations(const CVehicleComponentReservationDataNode& data)
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

	CComponentReservationManager* pCRM = pVehicle->GetComponentReservationMgr();
    gnetAssert(!data.m_hasReservations || data.m_numVehicleComponents == pCRM->GetNumReserveComponents());

    for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
    {
        CComponentReservation *componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);
        netObject        *pedUsingComponent  = NULL;
		
		if (data.m_hasReservations)
		{
			pedUsingComponent = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_componentReservations[componentIndex]);
		}
	
        // if ped doesn't exist on this machine fail the sync?
        if(AssertVerify(componentReservation))
        {
            CPed *ped = NetworkUtils::GetPedFromNetworkObject(pedUsingComponent);
#if __ASSERT
            bool isRespawnPed = pedUsingComponent && pedUsingComponent->IsLocalFlagSet(LOCALFLAG_RESPAWNPED);
            gnetAssert(ped || (pedUsingComponent==0 || isRespawnPed));
#endif
            componentReservation->UpdatePedUsingComponentFromNetwork(ped);
        }
    }
}

bool CNetObjVehicle::GetIsAttachedForSync() const
{
	// Check for dummy attachments, and if they don't exist, check for physical attachments
	if(const CVehicle * pVehicle = GetVehicle())
	{
		if(const CVehicle * pDummyParent = pVehicle->GetDummyAttachmentParent())
		{
			if(NetworkUtils::GetNetworkObjectFromEntity(pDummyParent))
			{
				return true;
			}
		}
	}

	return CNetObjPhysical::GetIsAttachedForSync();
}

void CNetObjVehicle::GetPhysicalAttachmentData(	bool       &attached,
												ObjectId   &attachedObjectID,
												Vector3    &attachmentOffset,
												Quaternion &attachmentQuat,
												Vector3	   &attachmentParentOffset,
												s16        &attachmentOtherBone,
												s16        &attachmentMyBone,
												u32        &attachmentFlags,
												bool       &allowInitialSeparation,
												float      &invMassScaleA,
												float      &invMassScaleB,
												bool	   &activatePhysicsWhenDetached,
                                                bool       &isCargoVehicle) const
{
	CVehicle * pVehicle = GetVehicle();
	if(!pVehicle)
	{
		return;
	}
	VALIDATE_GAME_OBJECT(pVehicle);

	bool bDummyAttachment = false;

	// Handle dummy attachments here.  If dummy parent exists and no physical parent exits, then create attachment node.
	CVehicle *pAttachParent = pVehicle->GetDummyAttachmentParent();
	if(pAttachParent && !CNetObjPhysical::GetIsAttachedForSync())
	{
		netObject* pParentObj = NetworkUtils::GetNetworkObjectFromEntity(pAttachParent);
			
        if(!netInterface::IsShuttingDown() && gnetVerifyf(pParentObj, "%s is attached to a non-networked dummy attachment parent", GetLogName()))
		{
			attached = true;
			attachedObjectID = pParentObj->GetObjectID();
			attachmentOffset = pVehicle->GetParentTrailerEntity2Offset();
			attachmentQuat = pVehicle->GetParentTrailerEntityRotation();

			// Trailer attachments assume no offset on the parent (at least dummy doesn't do anything with it, so it'll have to be updated in both places)
			attachmentParentOffset.Zero();
			// Pick the same attachment bones as used in super-dummy attachment. TODO, unify this later.
			attachmentOtherBone = pVehicle->m_nVehicleFlags.bIsCargoVehicle ? (s16)pAttachParent->GetBoneIndex(TRAILER_ATTACH) : (s16)pAttachParent->GetBoneIndex(VEH_CHASSIS);
			attachmentMyBone = 0;

			// Setup attachment flags
			attachmentFlags = 0;
			attachmentFlags |= ATTACH_STATE_PHYSICAL;
			attachmentFlags |= ATTACH_FLAG_ROT_CONSTRAINT;
			attachmentFlags |= ATTACH_FLAG_POS_CONSTRAINT;
			attachmentFlags |= ATTACH_FLAG_INITIAL_WARP;
			attachmentFlags |= ATTACH_FLAG_COL_ON;
			allowInitialSeparation = false;

			// Using mass scales from the script attachment command
			invMassScaleA = 1.0f;
			invMassScaleB = 0.1f;

			gnetAssert(attachedObjectID != NETWORK_INVALID_OBJECT_ID);
			gnetAssertf((attachmentFlags < (1<<NETWORK_ATTACH_FLAGS_START)), "Physical attachment flags have one the network flags set. Has someone added a new physical attachment flag?");			
			bDummyAttachment = true;
		}
	}

    isCargoVehicle = false;

	// B*1897409: Prevent dummy boats that are attached to dummy trailers from not being flagged as a cargo vehicle.
	// Not being correctly marked as a cargo vehicle causes boats to be attached incorrectly when they switch to real. This in turn causes issues with constraints not acting correctly.
	// Now do the cargo vehicle check using the dummy parent if it's present, otherwise grab the standard attach parent and use that (if it exists) as before.
	if(!pAttachParent)
	{
		 if(pVehicle->GetAttachParent() && pVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
			 pAttachParent = SafeCast(CVehicle, pVehicle->GetAttachParent());
	}
   
	if(pAttachParent)
	{    
        if(pAttachParent->InheritsFromTrailer())
        {
            CTrailer *pTrailer = SafeCast(CTrailer, pAttachParent);
            isCargoVehicle = pTrailer->FindCargoVehicle(pVehicle) >= 0;
        }
    }

	if (!bDummyAttachment)
	{
		CNetObjPhysical::GetPhysicalAttachmentData(attached,attachedObjectID,attachmentOffset,attachmentQuat,attachmentParentOffset,attachmentOtherBone,attachmentMyBone,attachmentFlags,allowInitialSeparation,invMassScaleA,invMassScaleB,activatePhysicsWhenDetached, isCargoVehicle);
	}
}

void CNetObjVehicle::SetPhysicalAttachmentData(const bool        attached,
								               const ObjectId    attachedObjectID,
								               const Vector3    &attachmentOffset,
								               const Quaternion &attachmentQuat,
								               const Vector3    &attachmentParentOffset,
								               const s16         attachmentOtherBone,
								               const s16         attachmentMyBone,
								               const u32         attachmentFlags,
                                               const bool        allowInitialSeparation,
                                               const float       invMassScaleA,
                                               const float       invMassScaleB,
								               const bool		 activatePhysicsWhenDetached,
                                               const bool        isCargoVehicle)
{
    m_IsCargoVehicle = isCargoVehicle;

    CNetObjPhysical::SetPhysicalAttachmentData(attached,attachedObjectID,attachmentOffset,attachmentQuat,attachmentParentOffset,attachmentOtherBone,attachmentMyBone,attachmentFlags,allowInitialSeparation,invMassScaleA,invMassScaleB,activatePhysicsWhenDetached,isCargoVehicle);
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   playerIndex          - playerIndex of new owner
//                  migrationType - how the vehicle is migrating
// Returns      :   void
void CNetObjVehicle::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    CVehicle* vehicle = GetVehicle();

	bool bWasClone = IsClone();

	CNetObjPhysical::ChangeOwner(player, migrationType);

    if (vehicle)
    {
		// always reset the respot counter after the car has changed owner
		if(migrationType == MIGRATE_REASSIGNMENT && !IsClone() && vehicle->IsBeingRespotted())
		{
			vehicle->ResetRespotCounter(false);
		}

        if (bWasClone && !IsClone())
        {
			vehicleDebugf3("CNetObjVehicle::ChangeOwner vehicle[%p][%s][%s] was a clone, now local. owner[%s]",vehicle,vehicle->GetModelName(),GetLogName(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");

			if (vehicle->InheritsFromAmphibiousAutomobile())			
			{
				CAmphibiousAutomobile* amphibiousVehicle = static_cast<CAmphibiousAutomobile*>(vehicle);			
				if (amphibiousVehicle) 
				{
					if (m_bLockedToXY && amphibiousVehicle->GetAnchorHelper().CanAnchorHere(true))
					{
						amphibiousVehicle->GetAnchorHelper().Anchor(m_bLockedToXY);
					}
					else
					{
						m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + AMPHIBIOUS_ANCHOR_TIME_DELAY;
					}
				}
			}			
			else if (vehicle->InheritsFromPlane())
			{
				CPlane* pPlane = static_cast<CPlane*>(vehicle);
				if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
				{
					if (m_bLockedToXY && pSeaPlaneExtension->GetAnchorHelper().CanAnchorHere(true))
					{
						pSeaPlaneExtension->GetAnchorHelper().Anchor(m_bLockedToXY);
					}
					else
					{
						m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + AMPHIBIOUS_ANCHOR_TIME_DELAY;
					}					
				}
			}

            // ensure the vehicle is always using the last received value for the throttle
            vehicle->m_vehControls.m_throttle = m_LastThrottleReceived;

            // reset the damage data received from the network
			m_deformationDataFL = 0;
			m_deformationDataFR = 0;
			m_deformationDataML = 0;
			m_deformationDataMR = 0;
			m_deformationDataRL = 0;
			m_deformationDataRR = 0;

			vehicledoorDebugf2("CTaskMotionInAutomobile::ChangeOwner--now local reset SetPedRemoteUsingDoor(false); SetRemoteDriverDoorClosing(false);");
			vehicle->SetRemoteDriverDoorClosing(false);

			for( s32 i = 0; i < SC_DOOR_NUM; i++ )
			{
				CCarDoor* pDoor = vehicle->GetDoor(i);

				if( pDoor )
				{
					pDoor->SetPedRemoteUsingDoor( false );
				}
			}

			// make the cached clone task the real AI task
			if (m_pCloneAITask)
			{
				m_pCloneAITask->SetupAfterMigration(vehicle);
				//crash task has it's own special priority
				eVehiclePrimaryTaskPriorities taskPriority = m_pCloneAITask->GetTaskType() == 
											CTaskTypes::TASK_VEHICLE_CRASH ? VEHICLE_TASK_PRIORITY_CRASH : VEHICLE_TASK_PRIORITY_PRIMARY;
				vehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, m_pCloneAITask, taskPriority, true);
				m_pCloneAITask = NULL;
			}

            // ensure the vehicle has a player drive and correct status if the local player is driving it
            CPed *driver = vehicle->GetDriver();

            if(driver && driver->IsLocalPlayer())
            {
                vehicle->SetPlayerDriveTask(driver);

                if(vehicle->GetStatus() != STATUS_WRECKED)
                {
				    vehicle->SetStatus(STATUS_PLAYER);
                }
            }

            ResetRoadNodeHistory();

			// if the vehicle has still not been placed on the road correctly by the previous owner, do it now
			if (vehicle->m_nVehicleFlags.bPlaceOnRoadQueued)
			{
				if (vehicle->GetVelocity().XYMag2() <= 0.01f)
				{
#if ENABLE_NETWORK_LOGGING
					netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
					NetworkLogUtils::WriteLogEvent(log, "PLACING_ON_ROAD_AND_FIXING", GetLogName());
#endif // ENABLE_NETWORK_LOGGING
					vehicle->SetIsFixedWaitingForCollision(true);
				}
				else
				{
					vehicle->m_nVehicleFlags.bPlaceOnRoadQueued = false;
				}
			}

			//Ensure on change of ownership of vehicles - that if there is a passenger that is another player that took ownership he will then send out the VehicleGameStateData node state.
			//This is important as the previous owner might have been a driver and left the vehicle turning it off which didn't get sent across the network before the change, now the new owner
			//here will have a bEngineOn=true while the previous owner will be bEngineOn=false.  The new owner needs to send out his proper state to the other players to ensure the vehicle is
			//operating properly remotely. lavalley.
			if (vehicle->ContainsLocalPlayer() && HasSyncData())
			{
				CVehicleSyncTree *vehicleSyncTree = SafeCast(CVehicleSyncTree, GetSyncTree());
				if(vehicleSyncTree && vehicleSyncTree->GetVehicleGameStateNode() && vehicleSyncTree->GetVehicleGameStateNode()->GetCurrentSyncDataUnit())
				{
					vehicleSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *vehicleSyncTree->GetVehicleGameStateNode());
				}
			}
        }
		else if (!bWasClone && IsClone())
		{
			vehicleDebugf3("CNetObjVehicle::ChangeOwner vehicle[%p][%s][%s] was local, now a clone. owner[%s]",vehicle,vehicle->GetModelName(),GetLogName(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");

			// set up m_pCloneAITask with the data from the current vehicle AI task
			CVehicleTaskDataNode data;

			GetVehicleAITask(data);
			SetVehicleAITask(data);

			// flush primary task tree (leave secondary tasks, eg convertible roof task)
			vehicle->GetIntelligence()->GetTaskManager()->AbortTasksAtTreeIndex(VEHICLE_TASK_TREE_PRIMARY);

            ResetLastPedsInSeats();

			m_cloneGliderState = (u8) vehicle->GetGliderState();

			if(vehicle->InheritsFromTrailer() && vehicle->GetAttachParent())
			{
				Vector3 trailerPos = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
				Vector3 parentPos  = VEC3V_TO_VECTOR3(vehicle->GetAttachParent()->GetTransform().GetPosition());
				Vector3 offsetFromParentVehicle = trailerPos - parentPos;

				CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, GetNetBlender());
				if(netBlenderPhysical)
				{
					netBlenderPhysical->SetParentVehicleOffset(true, offsetFromParentVehicle);
				}
			}
		}

        vehicle->GetIntelligence()->InvalidateCachedFollowRoute();
        vehicle->GetIntelligence()->InvalidateCachedNodeList();

        // allow dummying for a frame until the new vehicle task get's a chance to run, otherwise the
        // vehicle will be converted to a real for a frame
        vehicle->m_nVehicleFlags.bTasksAllowDummying = true;

        if (!IsLocalFlagSet(LOCALFLAG_BEINGREASSIGNED)) // peds in cars are reassigned seperately
        {
            for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
            {
                CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

                if(occupant &&
                   occupant->MigratesWithVehicle() &&
                   occupant->GetPlayerOwner() != &player)
                {
                    // ensure the occupant is still in this vehicle on the owning machine
                    if(!occupant->IsClone() || occupant->GetPedsTargetVehicleId() == GetObjectID())
                    {
                        occupant->SetPendingPlayerIndex(player.GetPhysicalPlayerIndex());

                        NetworkInterface::GetObjectManager().ChangeOwner(*occupant, player, migrationType);

                        // we need to update the ownership token for peds migrating with cars here -
                        // they aren't synced so we won't get a sync update with the new token value
                        if(occupant->IsClone())
                        {
                            occupant->IncreaseOwnershipToken();
                        }
                    }
                }
            }
        }

        if(!IsClone())
        {
            UpdateLastPedsInSeats();
        }
    }

	//Reset Clone is responsible for collision
	m_CloneIsResponsibleForCollision = false;
}

bool CNetObjVehicle::CheckPlayerHasAuthorityOverObject(const netPlayer& player)
{
    if(IsClone() && &player != GetPlayerOwner() && m_PreventCloneMigrationCountdown != 0)
    {
#if ENABLE_NETWORK_LOGGING
        netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
        NetworkLogUtils::WriteLogEvent(log, "PREVENTING_CLONE_MIGRATION", GetLogName());
#endif // ENABLE_NETWORK_LOGGING
        return false;
    }
    else
    {
        return CNetObjPhysical::CheckPlayerHasAuthorityOverObject(player);
    }
}

void CNetObjVehicle::PostMigrate(eMigrationType migrationType)
{
	CNetObjPhysical::PostMigrate(migrationType);

	if (!IsClone())
	{
		CVehicle* vehicle = GetVehicle();
		if (vehicle && vehicle->GetDriver() != NULL)
		{
			// force sending of important state (the position nodes are already forced to update in CNetObjProximityMigrateable::StartSynchronising)
			CVehicleSyncTree *vehicleSyncTree = SafeCast(CVehicleSyncTree, GetSyncTree());

			if(vehicleSyncTree)
			{
				vehicleSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *vehicleSyncTree->GetVelocityNode());
				vehicleSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *vehicleSyncTree->GetVehicleControlNode());
				vehicleSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *vehicleSyncTree->GetVehicleTaskNode());

				vehicleSyncTree->ForceSendOfPositionData(*this, GetActivationFlags());
			}
		}
	}
}

void CNetObjVehicle::OnConversionToAmbientObject()
{
    CVehicle *vehicle = GetVehicle();

	bool automaticallyAttaches			= vehicle ? vehicle->m_nVehicleFlags.bAutomaticallyAttaches : false;
    bool lockDoorsOnCleanup				= vehicle ? vehicle->m_nVehicleFlags.bLockDoorsOnCleanup    : false;
	bool explodesOnHighCollisionDamage	= vehicle ? vehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage : false;
	bool bShouldFixIfNoCollision		= vehicle ? vehicle->m_nVehicleFlags.bShouldFixIfNoCollision : false;
	bool bAllowSpecialFlightMode		= vehicle ? vehicle->GetSpecialFlightModeAllowed() : false;	
	bool disableHoverModeFlight         = vehicle ? vehicle->GetDisableHoverModeFlight() : false;
	float specialFlightModeTargetRatio  = vehicle ? vehicle->GetSpecialFlightModeTargetRatio() : 0.0f;

	bool        ghost				  = m_bGhost;
    PlayerFlags playerLocks           = m_playerLocks;
    PlayerFlags teamLockOverrides     = m_teamLockOverrides;
    TeamFlags   teamLocks             = m_teamLocks;
	float       scriptDamageScale     = vehicle	? vehicle->GetVehicleDamage()->GetScriptDamageScale() : 1.0f;

	u8 m_parachuteTintIndex = 0;
	if(vehicle && vehicle->InheritsFromAutomobile())
	{
		m_parachuteTintIndex = SafeCast(CAutomobile, vehicle)->GetParachuteTintIndex();
	}

	bool submarineMode = false;
	CSubmarineCar* submarineCar = nullptr;
	if (vehicle && vehicle->InheritsFromSubmarineCar())
	{		
		vehicle->PreventSubmarineCarTransform(true);
		submarineCar = static_cast<CSubmarineCar*>(vehicle);
		if(submarineCar)
		{
			submarineMode = submarineCar->IsInSubmarineMode();
		}
	}

	CNetObjPhysical::OnConversionToAmbientObject();

	if (vehicle)
	{
		vehicle->m_nVehicleFlags.bAutomaticallyAttaches = automaticallyAttaches;
		vehicle->m_nVehicleFlags.bLockDoorsOnCleanup    = lockDoorsOnCleanup;
		vehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage = explodesOnHighCollisionDamage;
		vehicle->m_nVehicleFlags.bShouldFixIfNoCollision = bShouldFixIfNoCollision;
		vehicle->SetSpecialFlightModeAllowed(bAllowSpecialFlightMode);
	 	vehicle->SetSpecialFlightModeTargetRatio(specialFlightModeTargetRatio);
		vehicle->SetDisableHoverModeFlight(disableHoverModeFlight);
		
        m_bGhost            = ghost;
        m_playerLocks       = playerLocks;
        m_teamLockOverrides = teamLockOverrides;
        m_teamLocks         = teamLocks;
		
		vehicle->GetVehicleDamage()->SetScriptDamageScale(scriptDamageScale);

		if(vehicle->InheritsFromAutomobile())
		{
			SafeCast(CAutomobile, vehicle)->SetParachuteTintIndex(m_parachuteTintIndex);
		}

		vehicle->PreventSubmarineCarTransform(false);
		if(submarineCar)
		{			
			submarineCar->SetSubmarineMode(submarineMode, false);
		}		
	}
}

void CNetObjVehicle::SetInvisibleAfterFadeOut()
{
	CVehicle* vehicle = GetVehicle();

	// detach any players that are not passengers in the vehicle, so that they are not made invisible with the vehicle
	CEntity* pAttachChild = static_cast<CEntity*>(vehicle->GetChildAttachment());
	FindAndDetachAttachedPlayer(pAttachChild);

	CNetObjPhysical::SetInvisibleAfterFadeOut();
}

void CNetObjVehicle::FindAndDetachAttachedPlayer(CEntity* entity)
{
	if(entity)
	{
		if(entity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(entity);

			if (pPed->IsAPlayerPed() && !pPed->GetIsInVehicle())
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			}
		}
		
		FindAndDetachAttachedPlayer(static_cast<CEntity*>(entity->GetChildAttachment()));

		FindAndDetachAttachedPlayer(static_cast<CEntity*>(entity->GetSiblingAttachment()));
	}
}

void CNetObjVehicle::PlayerHasLeft(const netPlayer& player)
{
    CNetObjPhysical::PlayerHasLeft(player);

    CVehicle   *vehicle       = GetVehicle();
    CPed       *driver        = vehicle ? vehicle->GetDriver() : 0;
    CNetObjPed *networkDriver = driver  ? SafeCast(CNetObjPed, driver->GetNetworkObject()) : 0;

    // if a leaving player is the only ped in a vehicle it is removed along with the player
    if(driver && driver->IsPlayer() && gnetVerifyf(networkDriver, "Non-networked ped is in a networked vehicle!"))
    {
        PhysicalPlayerIndex vehicleIndex = GetPhysicalPlayerIndex();
        PhysicalPlayerIndex driverIndex  = networkDriver->GetPhysicalPlayerIndex();
        PhysicalPlayerIndex leaverIndex  = player.GetPhysicalPlayerIndex();

        if((vehicleIndex == driverIndex) && (vehicleIndex == leaverIndex))
        {
            if(!vehicle->PopTypeIsMission() && vehicle->GetSeatManager()->CountPedsInSeats(false) == 0)
            {
                SetLocalFlag(LOCALFLAG_NOREASSIGN, true);
            }
        }
    }
}

bool CNetObjVehicle::IsAttachedOrDummyAttached() const
{
    CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    bool isAttached = CNetObjPhysical::IsAttachedOrDummyAttached() || (pVehicle && pVehicle->GetDummyAttachmentParent());

    return isAttached;
}

CPhysical *CNetObjVehicle::GetAttachOrDummyAttachParent() const
{
    CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    if(pVehicle)
    {
        CPhysical *attachParent = pVehicle->GetDummyAttachmentParent();

        if(!attachParent)
        {
            attachParent = CNetObjPhysical::GetAttachOrDummyAttachParent();
        }

        return attachParent;
    }

    return 0;
}

//
// name:        ApplyNetworkDeformation
// description: Returns whether the vehicle is too close to the local player to apply update vehicle damage
//
void  CNetObjVehicle::ApplyNetworkDeformation(bool bCheckDistance)
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	// apply the latest deformation data once the vehicle is off screen
	u32 deformationData[6] = {	m_deformationDataFL,
								m_deformationDataFR,
								m_deformationDataML,
								m_deformationDataMR,
								m_deformationDataRL,
								m_deformationDataRR };

	u32 totalNewDamage = 0;
	u32 totalExistingDamage = 0;
	for(int i=0;i<6;i++)
	{
		totalExistingDamage += pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(i);
		totalNewDamage += deformationData[i];
	}	
	

	if((m_bForceProcessApplyNetworkDeformation && totalExistingDamage > 0) || (totalNewDamage > totalExistingDamage))
	{
		bool bOverrideCheckDistance = (m_AllowCloneOverrideCheckDistanceTime && (m_AllowCloneOverrideCheckDistanceTime > fwTimer::GetSystemTimeInMilliseconds()));

		vehicleDebugf3("CNetObjVehicle::ApplyNetworkDeformation m_bForceProcessApplyNetworkDeformation[%d] bOverrideCheckDistance[%d] bCheckDistance[%d] totalNewDamage[%u] totalExistingDamage[%u]",m_bForceProcessApplyNetworkDeformation,bOverrideCheckDistance,bCheckDistance,totalNewDamage,totalExistingDamage);

		bool bApply = true;
		if(bCheckDistance)
		{
			bApply = !IsTooCloseToApplyDeformationData();
		}
		bApply |= (m_bForceProcessApplyNetworkDeformation || bOverrideCheckDistance);

		if(bApply)
		{
			pVehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformationsFromNetwork(deformationData);
		}

		m_bForceProcessApplyNetworkDeformation = false; //reset
		m_AllowCloneOverrideCheckDistanceTime = 0; //reset
	}

	//Clear the override if we are beyond the allowable time for the override to live.
	if (m_AllowCloneOverrideCheckDistanceTime && (m_AllowCloneOverrideCheckDistanceTime < fwTimer::GetSystemTimeInMilliseconds()))
		m_AllowCloneOverrideCheckDistanceTime = 0; //reset
};

//
// name:        IsTooCloseToApplyDeformationData
// description: Returns whether the vehicle is too close to the local player to apply update vehicle damage
//
bool CNetObjVehicle::IsTooCloseToApplyDeformationData()
{
    bool tooClose = false;

    CVehicle *pVehicle = GetVehicle();

    if(pVehicle)
    {
		if(pVehicle->ContainsPlayer())
			return true;

        if(NetworkInterface::GetLocalPlayer())
        {
            static const float UPDATE_DEFORMATION_FROM_ON_SCREEN_NETWORK_DISTANCE_SQUARED = 200.0f * 200.0f;
            static const float UPDATE_DEFORMATION_FROM_OFF_SCREEN_NETWORK_DISTANCE_SQUARED = 100.0f * 100.0f;
            const float updateDistanceSquared = pVehicle->GetIsVisibleInSomeViewportThisFrame() ? UPDATE_DEFORMATION_FROM_ON_SCREEN_NETWORK_DISTANCE_SQUARED : UPDATE_DEFORMATION_FROM_OFF_SCREEN_NETWORK_DISTANCE_SQUARED;

            if(IsLessThanOrEqualAll(DistSquared(pVehicle->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(NetworkInterface::GetLocalPlayerFocusPosition())), ScalarVFromF32(updateDistanceSquared)))
            {
                tooClose = true;
            }
        }
    }

    return tooClose;
}

bool IsOtherNodeValidForFuturePrediction(const CPathNode* pOldNode, const CPathNode* pPotentialNewNode, const CPathNodeLink &link, const bool bIsWandering)
{
	gnetAssert(pOldNode);
	gnetAssert(pPotentialNewNode);

	if (!bIsWandering)
	{
		return true;
	}

	//if I'm wandering, and my current node isn't switched off,
	//I can't choose a switched off node
	if (!pOldNode->IsSwitchedOff() && pPotentialNewNode->IsSwitchedOff())
	{
		return false;
	}

	if (link.IsShortCut())
	{
		return false;
	}

	return true;
}

const CNodeAddress *FindClosestNodeToDirection(const CNodeAddress &roadNode, const Vector3 &direction, const bool bIsWandering)
{
    gnetAssertf(direction.Mag2() <= 1.001f, "Specified direction should be normalised!");

    const CNodeAddress *closestRoadNode = 0;

    // try to find the old path the closest matches the vehicle direction
    if(roadNode.GetRegion() < PATHFINDREGIONS)
    {
   const CPathNode *pathNode = ThePaths.FindNodePointerSafe(roadNode);

        if(pathNode && (pathNode->GetAddrRegion() < PATHFINDREGIONS) && ThePaths.IsRegionLoaded(pathNode->GetAddrRegion()))
    {
        Vector3 pathNodeCoords;
        pathNode->GetCoors(pathNodeCoords);

        float closestAngleDiff = FLT_MAX;

        u32 numLinks = pathNode->NumLinks();

        for(u32 index = 0; index < numLinks; index++)
        {
            const CPathNodeLink &link = ThePaths.GetNodesLink(pathNode, index);

            const CPathNode *otherPathNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);

            if(otherPathNode)
            {
                Vector3 otherPathNodeCoords;
                otherPathNode->GetCoors(otherPathNodeCoords);

                Vector3 pathNodesDirection = otherPathNodeCoords - pathNodeCoords;
                pathNodesDirection.Normalize();

                float angleDiff = 1.0f - pathNodesDirection.Dot(direction);

                if(IsOtherNodeValidForFuturePrediction(pathNode, otherPathNode, link, bIsWandering) && (angleDiff < closestAngleDiff))
                {
                    closestAngleDiff = angleDiff;
                    closestRoadNode  = &link.m_OtherNode;
                }
            }
        }
    }
    }

    return closestRoadNode;
}

const CNodeAddress *CNetObjVehicle::EstimateOldRoadNode(const CNodeAddress &roadNode)
{
    const CNodeAddress *estimatedOldRoadNode = 0;

    CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

    if(vehicle)
    {
        Vector3 reverseVehicleDirection = -m_LastReceivedMatrix.b;
        reverseVehicleDirection.Normalize();

        estimatedOldRoadNode = FindClosestNodeToDirection(roadNode, reverseVehicleDirection, false);
    }

    return estimatedOldRoadNode;
}

const CNodeAddress *CNetObjVehicle::EstimateFutureRoadNode(const CNodeAddress &roadNode, const bool bIsWandering)
{
    const CNodeAddress *estimatedFutureRoadNode = 0;

    CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

    if(vehicle)
    {
        Vector3 vehicleDirection = m_LastReceivedMatrix.b;
        vehicleDirection.Normalize();

        estimatedFutureRoadNode = FindClosestNodeToDirection(roadNode, vehicleDirection, bIsWandering);
    }

    return estimatedFutureRoadNode;
}

const unsigned NUM_NODES_LEVELS_TO_SEARCH = 3;

bool FindRouteBetweenNodes(const CNodeAddress &startNode,
                           const CNodeAddress &prevNode,
                           const CNodeAddress &endNode,
                           CNodeAddress *nodeList,
                           unsigned &numNodes,
                           unsigned  depthToSearch)
{
    bool foundRoute = false;

    if(depthToSearch > 0)
    {
        const CPathNode *pathNode = ThePaths.FindNodePointerSafe(startNode);

        if(pathNode)
        {
            u32 numLinks = pathNode->NumLinks();

            for(u32 index = 0; index < numLinks && !foundRoute; index++)
            {
                const CPathNodeLink &link = ThePaths.GetNodesLink(pathNode, index);

                if(link.m_OtherNode != prevNode)
                {
                    if(link.m_OtherNode == endNode)
                    {
                        foundRoute = true;
                    }
                    else
                    {
                        foundRoute = FindRouteBetweenNodes(link.m_OtherNode, startNode, endNode, nodeList, numNodes, depthToSearch - 1);
                    }

                    if(foundRoute)
                    {
                        if(gnetVerifyf(numNodes < NUM_NODES_LEVELS_TO_SEARCH, "Unexpected number of nodes found!"))
                        {
                            nodeList[numNodes] = startNode;
                            numNodes++;
                        }
                    }
                }
            }
        }
    }

    return foundRoute;
}

bool CNetObjVehicle::AddSkippedRoadNodesToHistory(const CNodeAddress &newNode)
{
    bool foundRoute = false;

    const CNodeAddress &oldNode = m_RoadNodeHistory[NODE_NEW];

    const CPathNode *pathNode = ThePaths.FindNodePointerSafe(oldNode);

    if(pathNode)
    {
        CNodeAddress nodeList[NUM_NODES_LEVELS_TO_SEARCH];
        unsigned numNodes = 0;

        u32 numLinks = pathNode->NumLinks();

        for(u32 index = 0; index < numLinks && !foundRoute; index++)
        {
            const CPathNodeLink &link = ThePaths.GetNodesLink(pathNode, index);

            numNodes = 0;

            if(FindRouteBetweenNodes(link.m_OtherNode, oldNode, newNode, nodeList, numNodes, NUM_NODES_LEVELS_TO_SEARCH))
            {
                foundRoute = true;
            }
        }

        if(foundRoute)
        {
            for(unsigned index = numNodes; index > 0; index--)
            {
                AddRoadNodeToHistory(nodeList[index - 1]);
            }
        }
    }

    return foundRoute;
}

void CNetObjVehicle::AddRoadNodeToHistory(const CNodeAddress &roadNode)
{
    // shuffle all elements down - new address always at position 0
    for(unsigned index = 0; index < NODE_NEW; index++)
    {
        m_RoadNodeHistory[index] = m_RoadNodeHistory[index + 1];
    }

    m_RoadNodeHistory[NODE_NEW] = roadNode;
}

void CNetObjVehicle::ResetRoadNodeHistory()
{
    for(unsigned index = 0; index < ROAD_NODE_HISTORY_SIZE; index++)
    {
        m_RoadNodeHistory[index].SetEmpty();
    }

    UpdateRouteForClone();
}

void CNetObjVehicle::UpdateRouteForClone()
{
    m_NodeList.Clear();
    m_FollowRouteHelper.ConstructFromNodeList(GetVehicle(), m_NodeList);

	//check our previous nodes are still valid
	for(unsigned index = 0; index <= NODE_NEW; index++)
	{
		if(m_RoadNodeHistory[index].IsEmpty() || !ThePaths.IsRegionLoaded(m_RoadNodeHistory[index]))
		{
			return;
		}
	}

    for(unsigned index = 0; index <= NODE_NEW; index++)
    {
        m_NodeList.SetPathNodeAddr(index, m_RoadNodeHistory[index]);
    }

    // copy any future nodes into the node list
    for(unsigned index = NODE_NEW + 1; index < ROAD_NODE_HISTORY_SIZE; index++)
    {
        if(!m_RoadNodeHistory[index].IsEmpty() && ThePaths.IsRegionLoaded(m_RoadNodeHistory[index]))
        {
            m_NodeList.SetPathNodeAddr(index, m_RoadNodeHistory[index]);
        }
    }

    m_NodeList.FindLinksWithNodes();
    m_NodeList.FindLanesWithNodes();
    m_NodeList.SetTargetNodeIndex(NODE_NEW);

    m_FollowRouteHelper.ConstructFromNodeList(GetVehicle(), m_NodeList);

    // ensure we have constructed a valid route
	gnetAssertf(m_FollowRouteHelper.GetNumPoints() > 0, "Route with no points generated!");

#if USE_NET_ASSERTS
    for(unsigned index = 0; index < m_FollowRouteHelper.GetNumPoints(); index++)
    {
		const CRoutePoint& rPoint1 = m_FollowRouteHelper.GetRoutePoints()[index];
		Vector3 node1Coors = VEC3V_TO_VECTOR3(rPoint1.GetPosition());
        gnetAssertf(node1Coors.x >= WORLDLIMITS_XMIN && node1Coors.x <= WORLDLIMITS_XMAX &&
                node1Coors.y >= WORLDLIMITS_YMIN && node1Coors.y <= WORLDLIMITS_YMAX &&
                node1Coors.z >= WORLDLIMITS_ZMIN && node1Coors.z <= WORLDLIMITS_ZMAX,
                "Route generated with invalid points!");
    }
#endif
}

void CNetObjVehicle::SetAlpha(u32 alpha)
{
	CNetObjPhysical::SetAlpha(alpha);

	// apply alpha to occupants too, but ignore the smoothAlpha, only vehicles can take it.
	CVehicle* pVehicle = GetVehicle();

	if (pVehicle)
	{
		const s32 maxSeats = pVehicle->GetSeatManager()->GetMaxSeats();

		for(s32 seatIndex = 0; seatIndex < maxSeats; seatIndex++)
		{
			CPed *currentOccupant  = pVehicle->GetSeatManager()->GetPedInSeat(seatIndex);

			if (currentOccupant && currentOccupant->GetNetworkObject())
			{
				SafeCast(CNetObjPhysical, currentOccupant->GetNetworkObject())->SetAlpha(alpha);
			}
		}
	}
}

void CNetObjVehicle::ResetAlphaActive()
{
	CNetObjPhysical::ResetAlphaActive();

	CVehicle* pVehicle = GetVehicle();

	// reset alpha on occupants too
	if (pVehicle)
	{
		const s32 maxSeats = pVehicle->GetSeatManager()->GetMaxSeats();

		for(s32 seatIndex = 0; seatIndex < maxSeats; seatIndex++)
		{
			CPed *currentOccupant  = pVehicle->GetSeatManager()->GetPedInSeat(seatIndex);

			if (currentOccupant && currentOccupant->GetNetworkObject())
			{
				SafeCast(CNetObjPhysical, currentOccupant->GetNetworkObject())->ResetAlphaActive();
			}
		}
	}
}

void CNetObjVehicle::SetHideWhenInvisible(bool bSet)
{
	CVehicle* pVehicle = GetVehicle();

	CNetObjPhysical::SetHideWhenInvisible(bSet);

	if (pVehicle)
	{
		// Iterate through all peds attached to the car and make them visible/invisible too
		CEntity* pAttachChild = static_cast<CEntity*>(pVehicle->GetChildAttachment());
		while(pAttachChild)
		{
			CPed* pPassenger = pAttachChild->GetIsTypePed() ? (CPed*)pAttachChild : NULL;

			if (pPassenger && pPassenger->GetNetworkObject())
			{
				SafeCast(CNetObjPhysical, pPassenger->GetNetworkObject())->SetHideWhenInvisible(bSet);
			}

			pAttachChild = static_cast<CEntity*>(pAttachChild->GetSiblingAttachment());
		}
	}
}

CVehicleGadgetTowArm* CNetObjVehicle::GetTowArmGadget(CPhysical* pPhysical) 
{
	CVehicleGadgetTowArm* pTowArm = NULL;

	if (pPhysical->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = SafeCast(CVehicle, pPhysical);

		if (pVehicle)
		{
			for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
			{
				CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

				if (pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
				{
					pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
					break;
				}
			}
		}
	}

	return pTowArm;
}

CVehicleGadgetPickUpRope *CNetObjVehicle::GetPickupRopeGadget(CPhysical* pPhysical)
{
    CVehicleGadgetPickUpRope *pPickUpRope = 0;

	if (pPhysical->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = SafeCast(CVehicle, pPhysical);

		if (pVehicle)
		{
			for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
			{
				CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

				if (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
				{
					pPickUpRope = static_cast<CVehicleGadgetPickUpRope *>(pVehicleGadget);
					break;
				}
			}
		}
	}

	return pPickUpRope;
}

CVehicleGadget*	CNetObjVehicle::GetDummyVehicleGadget(eVehicleGadgetType gadgetType)
{
	static CVehicleGadgetForks forksGadget(0,0,0.0f);
	static CSearchLight searchlightGadget(0, VEH_INVALID_ID);
	static CVehicleGadgetPickUpRopeWithHook pickupUpRopeGadget(0, 0);
	static CVehicleGadgetPickUpRopeWithMagnet pickupUpRopeMagnetGadget(0, 0);
	static CVehicleGadgetDiggerArm diggerArmGadget(0, 0.f);
	static CVehicleGadgetHandlerFrame handlerFrameGadget(0, 0, 0.f);
	static CVehicleGadgetBombBay bombayGadget(0, 0, 0.0f);

	CVehicleGadget* pGadget = NULL;

	switch (gadgetType)
	{
	case VGT_FORKS:
		pGadget = &forksGadget;
		break;
	case VGT_SEARCHLIGHT:
		pGadget = &searchlightGadget;
		break;
	case VGT_PICK_UP_ROPE:
		pGadget = &pickupUpRopeGadget;
		break;
	case VGT_DIGGER_ARM:
		pGadget = &diggerArmGadget;
		break;
	case VGT_HANDLER_FRAME:
		pGadget = &handlerFrameGadget;
		break;
	case VGT_PICK_UP_ROPE_MAGNET:
		pGadget = &pickupUpRopeMagnetGadget;
		break;
	case VGT_BOMBBAY:
		pGadget = &bombayGadget;
		break;
	default:
		gnetAssertf(0, "CNetObjVehicle::GetDummyVehicleGadget - Unhandled vehicle gadget type %d", gadgetType);
	}

	if (pGadget)
	{
		gnetAssert(pGadget->IsNetworked());
	}

	return pGadget;
}

CVehicleGadget*	CNetObjVehicle::AddVehicleGadget(CVehicle* pVehicle, eVehicleGadgetType gadgetType)
{
	CVehicleGadget* pGadget = NULL;

	switch (gadgetType)
	{
	case VGT_PICK_UP_ROPE:
		if (AssertVerify(pVehicle->InheritsFromHeli()))
		{
			SafeCast(CHeli, pVehicle)->SetPickupRopeType(PICKUP_HOOK);
			pGadget = SafeCast(CHeli, pVehicle)->AddPickupRope();
		}
		break;
	case VGT_PICK_UP_ROPE_MAGNET:
		if (AssertVerify(pVehicle->InheritsFromHeli()))
		{
			SafeCast(CHeli, pVehicle)->SetPickupRopeType(PICKUP_MAGNET);
			pGadget = SafeCast(CHeli, pVehicle)->AddPickupRope();
		}
		break;
	case VGT_SEARCHLIGHT:
		// we must wait until the searchlight is created for the heli. This can take a short time.
		break;
	default:
		gnetAssertf(0, "CNetObjVehicle::AddVehicleGadget - Unhandled vehicle gadget type %d", gadgetType);
	}

	if (pGadget)
	{
		gnetAssert(pGadget->IsNetworked());
	}

	return pGadget;
}

bool CNetObjVehicle::IsCloserToRemotePlayerForProximity(const CNetGamePlayer &remotePlayer, const float distToCurrentOwnerSqr, const float distToRemotePlayerSqr)
{
    bool isCloser = CNetObjPhysical::IsCloserToRemotePlayerForProximity(remotePlayer, distToCurrentOwnerSqr, distToRemotePlayerSqr);

    // try to pass control of off-screen vehicles using super-dummy physics earlier to players that have them on-screen
    // within the dummy/real physics range - this doesn't handle lod scale changes on remote machines currently, but should still improve matters
    CVehicle *vehicle = GetVehicle();

    if(vehicle)
    {
        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode == VDM_SUPERDUMMY && !isCloser)
        {
            bool visibleToRemotePlayer = NetworkInterface::IsVisibleToPlayer(&remotePlayer, VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()), vehicle->GetBoundRadius(), GetScopeDistance());

            if(!vehicle->GetIsOnScreen() && visibleToRemotePlayer)
            {
                if(distToRemotePlayerSqr < rage::square(CVehicleAILodManager::GetSuperDummyLodDistance()))
                {
                    isCloser = true;
                }
            }
        }
        else if(isCloser)
        {            
            bool visibleToRemotePlayer = NetworkInterface::IsVisibleToPlayer(&remotePlayer, VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()), vehicle->GetBoundRadius(), GetScopeDistance());

            if(vehicle->GetIsOnScreen() && !visibleToRemotePlayer)
            {
                if(distToCurrentOwnerSqr < rage::square(CVehicleAILodManager::GetSuperDummyLodDistance()))
                {
                    isCloser = false;
                }
            }
        }
    }

    return isCloser;
}

bool CNetObjVehicle::CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const
{
    CVehicle *vehicle = GetVehicle();

    if(gnetVerifyf(vehicle, "Invalid vehicle!"))
    {
        // check if we are still attached to the correct entity via the dummy attachment system
        if(vehicle->IsDummy())
        {
            CVehicle *dummyAttachParent = vehicle->GetDummyAttachmentParent();

            if(dummyAttachParent && dummyAttachParent->GetNetworkObject() && 
              (dummyAttachParent->GetNetworkObject()->GetObjectID() == m_pendingAttachmentObjectID))
            {
				if (failReason)
				{
					*failReason = CPA_VEHICLE_DUMMY;
				}
               
				*ppCurrentAttachmentEntity = dummyAttachParent;

				return false;
            }
        }
    }

	return CNetObjPhysical::CanProcessPendingAttachment(ppCurrentAttachmentEntity, failReason);
}

bool CNetObjVehicle::AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* reason)
{
	CVehicleGadgetTowArm* pTowArm = GetTowArmGadget(pEntityToAttachTo);
	
	bool bSuccess = true;

	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	if (pVehicle->IsDummy())
	{
        if(pVehicle->InheritsFromTrailer() && pVehicle->GetDummyAttachmentParent())
        {
            if(!pVehicle->GetDummyAttachmentParent()->TryToMakeFromDummy())
            {
                bSuccess = false;
#if ENABLE_NETWORK_LOGGING
				if(reason)
				{
					*reason = APA_VEHICLE_IS_DUMMY_TRAILER;
				}
#endif // ENABLE_NETWORK_LOGGING
            }
        }
        else if(!pVehicle->TryToMakeFromDummy())
        {
		    bSuccess = false;
#if ENABLE_NETWORK_LOGGING
			if(reason)
			{
				*reason = APA_VEHICLE_IS_DUMMY;
			}
#endif // ENABLE_NETWORK_LOGGING
        }
	}

	if (bSuccess && pEntityToAttachTo->GetIsTypeVehicle())
	{
		CVehicle* pVehicleToAttachTo = static_cast<CVehicle*>(pEntityToAttachTo);

		if (pVehicleToAttachTo->IsDummy())
		{
            if(pVehicleToAttachTo->InheritsFromTrailer() && !pVehicleToAttachTo->GetDummyAttachmentParent())
            {
			    bSuccess = false;
#if ENABLE_NETWORK_LOGGING
				if(reason)
				{
					*reason = APA_VEHICLE_PARENT_IS_DUMMY_TRAILER;
				}
#endif // ENABLE_NETWORK_LOGGING
            }
            else if(!pVehicleToAttachTo->TryToMakeFromDummyIncludingParents())
            {
                bSuccess = false;
#if ENABLE_NETWORK_LOGGING
				if(reason)
				{
					*reason = APA_VEHICLE_PARENT_IS_DUMMY;
				}
#endif // ENABLE_NETWORK_LOGGING
            }
		}
	}

	if (bSuccess)
	{
		// attaching to a tow truck is handled differently
		if (pTowArm)
		{
			CVehicle* pTowTruck = SafeCast(CVehicle, pEntityToAttachTo);

			pTowArm->AttachVehicleToTowArm(pTowTruck, GetVehicle(), -1, m_pendingAttachOffset, static_cast<s32>(m_pendingMyAttachBone));

			bSuccess = (pTowArm->GetAttachedVehicle() == GetVehicle());
#if ENABLE_NETWORK_LOGGING
			if(!bSuccess && reason)
			{
				*reason = APA_VEHICLE_TOWARM_ENTITY_DIFFER;
			}
#endif // ENABLE_NETWORK_LOGGING
		}
		else
		{
			Vector3 vTrailerEntityOffset = m_pendingAttachOffset;// Cache off the offset so we can reset it just in case AttemptPendingAttachment fails.

			// B*1897409: Cargo vehicles attach to the TRAILER_ATTACH bone and so the m_pendingAttachOffset is relative to that bone.
			// If the vehicle is a dummy on the owner machine, a physical attachment is used, which requires that the attachment offsets is relative to the entity origin, not the attach bone.
			// So find the position of the attach bone relative to the entity origin and add that to the offset that will be used for the attachment.
			if (m_IsCargoVehicle && (m_pendingAttachFlags & ATTACH_STATES) == ATTACH_STATE_PHYSICAL)
			{
				m_pendingAttachOffset += VEC3V_TO_VECTOR3(pEntityToAttachTo->GetSkeleton()->GetObjectMtx(m_pendingOtherAttachBone).d());
			}

		    bSuccess = CNetObjPhysical::AttemptPendingAttachment(pEntityToAttachTo, reason);

			m_pendingAttachOffset = vTrailerEntityOffset;
		}
	}

    if(bSuccess && m_IsCargoVehicle)
    {
        if(pVehicle->GetAttachParent() && pVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
        {
            CVehicle *pParentVehicle = SafeCast(CVehicle, pVehicle->GetAttachParent());

            if(pParentVehicle && pParentVehicle->InheritsFromTrailer())
            {
                CTrailer *pTrailer = SafeCast(CTrailer, pParentVehicle);

                if(pTrailer && (pTrailer->FindCargoVehicle(pVehicle) < 0))
                {
                    // we only support one cargo vehicle in MP currently
                    pTrailer->AddCargoVehicle(0, pVehicle);
                    pVehicle->SetParentTrailer(pTrailer);

					// B*1897409: Need to make sure the trailer attachment offsets are set for the cargo vehicle or else both the dummy and real attachments will be incorrect and cause collisions.
					pVehicle->SetParentTrailerEntity2Offset(m_pendingAttachOffset);
					pVehicle->SetParentTrailerEntityRotation(m_pendingAttachQuat);
                }
            }
        }
    }

	return bSuccess;
}

void CNetObjVehicle::AttemptPendingDetachment(CPhysical* pEntityToAttachTo)
{
	CVehicleGadgetTowArm* pTowArm = GetTowArmGadget(pEntityToAttachTo);

	// attaching to a tow truck is handled differently
	if (pTowArm)
	{
		vehicleDisplayf( "[TOWTRUCK ROPE DEBUG] CNetObjVehicle::AttemptPendingDetachment - Detaching entity." );
		pTowArm->DetachVehicle(GetVehicle());
	}
	else
	{
        CVehicle *pVehicle = GetVehicle();

        if(pVehicle->GetAttachParent() && pVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
        {
            CVehicle *pParentVehicle = SafeCast(CVehicle, pVehicle->GetAttachParent());

            if(pParentVehicle && pParentVehicle->InheritsFromTrailer())
            {
                CTrailer *pTrailer = SafeCast(CTrailer, pParentVehicle);

                pTrailer->RemoveCargoVehicle(pVehicle);
                pVehicle->SetParentTrailer(0);
            }
        }

 	    CNetObjPhysical::AttemptPendingDetachment(pEntityToAttachTo);
	}
}

bool CNetObjVehicle::UseBoxStreamerForFixByNetworkCheck() const
{
    CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

    bool useBoxStreamer = vehicle->PopTypeIsMission() || vehicle->m_nVehicleFlags.bIsLawEnforcementVehicle;

    if(!useBoxStreamer)
    {
        CPed *driver = vehicle->GetDriver();

        if(driver && driver->GetPedIntelligence()->GetOrder() != 0)
        {
            useBoxStreamer = true;
        }
    }

    return useBoxStreamer;
}

bool CNetObjVehicle::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	// dummy vehicles do not need the map streamed in underneath them
	if (pVehicle->IsDummy())
		return false;

	return CNetObjPhysical::FixPhysicsWhenNoMapOrInterior(npfbnReason);
}

void CNetObjVehicle::UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason)
{
    CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    if(pVehicle && !pVehicle->IsNetworkClone() && fixByNetwork && !pVehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) && !pVehicle->GetIsHeli())
    {
		m_FixIfNotDummyBeforePhysics = true;
    }
	else
	{
		CNetObjPhysical::UpdateFixedByNetwork(fixByNetwork, reason, npfbnReason);
	}
}

bool CNetObjVehicle::ShouldDoFixedByNetworkInAirCheck() const
{
    CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

    Vector3 position = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());

    float DO_IN_AIR_CHECK_THRESHOLD_SQR = rage::square(20.0f);

    if(((m_LastPositionWithWheelContacts - position).Mag2()) > DO_IN_AIR_CHECK_THRESHOLD_SQR)
    {
        return true;
    }

    return false;
}

void CNetObjVehicle::SetFixedByNetwork(bool fixObject, unsigned reason, unsigned npfbnReason)
{
    CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

    bool wasFixedByNetwork = vehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK);

    CNetObjPhysical::SetFixedByNetwork(fixObject, reason, npfbnReason);

    // move the car to it's current predicted position and target velocity
    bool isFixedByNetwork = vehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK);

    if(wasFixedByNetwork && !isFixedByNetwork && IsClone())
    {
        if(GetNetBlender())
        {
            GetNetBlender()->GoStraightToTarget();
        }
    }

    CTrailer *trailer = vehicle->GetAttachedTrailer();

    if(trailer && trailer->GetNetworkObject())
    {
        CNetObjVehicle *trailerNetObject = SafeCast(CNetObjVehicle, trailer->GetNetworkObject());
        trailerNetObject->SetFixedByNetwork(fixObject, reason, npfbnReason);
    }
}

bool CNetObjVehicle::AllowFixByNetwork() const 
{ 
	CVehicle *pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    if(!NetworkInterface::IsInMPCutscene())
    {
        if(pVehicle->GetDriver() == FindFollowPed() && !pVehicle->GetIsFixedByNetworkFlagSet())
        {
            return false;
        }

	    return !pVehicle->GetIsBeingTowed();
    }

    return CNetObjPhysical::AllowFixByNetwork();
}

bool CNetObjVehicle::GetCachedEntityCollidableState(void)
{
	bool cachedState = CNetObjPhysical::GetCachedEntityCollidableState();

	if (!IsClone())
	{
		CVehicle *pVehicle = GetVehicle();
		gnetAssert(pVehicle);

		if (pVehicle && pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
		{
			cachedState = true;
		}
	}

	return cachedState;
}

bool CNetObjVehicle::CanEnableCollision() const
{
	CVehicle *pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	bool bCanEnableCollision = CNetObjPhysical::CanEnableCollision();

	if (pVehicle)
	{
		// super dummy vehicles have their collision disabled, so cannot have it enabled while in super dummy
		if (pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
		{
			bCanEnableCollision = false;
		}

	}

	return bCanEnableCollision;
}

u8 CNetObjVehicle::GetLowestAllowedUpdateLevel() const
{
    CVehicle* pVehicle = GetVehicle();
	gnetAssert(pVehicle);

    if(pVehicle)
    {
		if(pVehicle->GetVelocity().Mag2() > 0.1f)
        {
            float brakePedal = pVehicle->m_vehControls.m_brake;
            float steerAngle = Clamp(pVehicle->m_vehControls.m_steerAngle, -1.0f, 1.0f);

            bank_float BRAKE_THRESHOLD      = 0.1f;
            bank_float STEER_THRESHOLD      = 0.35f;
            bank_float ANG_VEL_XY_THRESHOLD = 0.5f;

		    // ensure we are always at high update level when cars are steering or braking, or spinning fast on XY
            if(brakePedal >= BRAKE_THRESHOLD || (fabsf(steerAngle) >= STEER_THRESHOLD) || fabs(pVehicle->GetAngVelocity().XYMag2()) >= rage::square(ANG_VEL_XY_THRESHOLD))
            {
                return CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
            }
        }

		CTrailer *trailer = pVehicle->GetAttachedTrailer();

		if(trailer && trailer->GetNetworkObject() && trailer->HasCargoVehicles())
		{
			 return CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
		}
    }

    return CNetObjPhysical::GetLowestAllowedUpdateLevel();
}

bool CNetObjVehicle::HasCollisionLoadedUnderEntity() const
{
	CVehicle* pVehicle = GetVehicle();

	if (pVehicle)
	{
		// some vehicles are huge (eg the Titan), in this case make sure the collision is loaded under the entire BB of the vehicle
		spdAABB box;
		pVehicle->GetAABB(box);

		return g_StaticBoundsStore.GetBoxStreamer().HasLoadedWithinAABB(box, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	}

	return true;
}

float CNetObjVehicle::GetPredictedPosErrorThresholdSqr(unsigned updateLevel) const
{
    if(gnetVerifyf(updateLevel < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS, "Trying to get the predicted error threshold for an invalid update rate!"))
    {
        return ms_PredictedPosErrorThresholdSqr[updateLevel];
    }

    return 1.0f;
}

CNetObjPhysical::SnapshotData *CNetObjVehicle::GetSnapshotData(unsigned updateLevel)
{
    if(gnetVerifyf(updateLevel < CNetworkSyncDataULBase::NUM_UPDATE_LEVELS, "Trying to get the prediction error snapshot for an invalid update rate!"))
    {
        return &m_SnapshotData[updateLevel];
    }

    return 0;
}

bool CNetObjVehicle::ShouldPreventDummyModesThisFrame() const
{
    bool preventSuperDummy = false;

    CNetBlenderPhysical *netBlender = SafeCast(CNetBlenderPhysical, GetNetBlender());

    if(netBlender)
    {
        // check remote vehicle is not upside down or on it's side and stationary
        Vector3 targetVelocity = netBlender->GetLastVelocityReceived();

        if(targetVelocity.Mag2() < 0.01f)
        {
            Matrix34 targetMatrix = netBlender->GetLastMatrixReceived();

            float rightZ       = targetMatrix.a.z;
            bool  isUpsideDown = targetMatrix.c.z <= -0.9f;
            bool  isOnSide     = rightZ >= 0.8f || rightZ <= -0.8f;

            preventSuperDummy = isUpsideDown || isOnSide;
        }
    }

    return preventSuperDummy;
}

//
// name:        RegisterKillWithNetworkTracker
// description: If a vehicle has become wrecked we increment the counter with the network kill tracker....
//
void CNetObjVehicle::RegisterKillWithNetworkTracker()
{
    CVehicle* pVehicle = GetVehicle();
    gnetAssert(pVehicle);

    // check if this vehicle has been destroyed and has not been registered with the kill tracking code yet
    if(pVehicle && pVehicle->GetStatus() == STATUS_WRECKED)
    {
        if(!m_registeredWithKillTracker)
        {
			CNetGamePlayer *player = NetworkUtils::GetPlayerFromDamageEntity(pVehicle, pVehicle->GetWeaponDamageEntity());

			if( (player != NULL) && (player->IsMyPlayer()) && (pVehicle->GetWeaponDamageHash() != 0))
			{
				CNetworkKillTracker::RegisterKill(pVehicle->GetModelIndex(), pVehicle->GetWeaponDamageHash(), !pVehicle->PopTypeIsMission());
				
				m_registeredWithKillTracker = true;
			}
        }
    }
}

#if __BANK

bool CNetObjVehicle::ShouldDisplayAdditionalDebugInfo()
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if (displaySettings.m_displayTargets.m_displayVehicles && GetVehicle())
	{
		return true;
	}

	return CNetObjPhysical::ShouldDisplayAdditionalDebugInfo();
}

// Name         :   DisplayNetworkInfoForObject
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjVehicle::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
    CNetObjPhysical::DisplayNetworkInfoForObject(col, scale, screenCoords, debugTextYIncrement);

    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if(displaySettings.m_displayInformation.m_displayVehicleDamage && GetVehicle())
    {
        char str[256];

        CVehicle* pVehicle = GetVehicle();

        sprintf(str, "FL:%d FR:%d ML:%d MR:%d RL:%d RR:%d ",	pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(0),
																pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(1),
																pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(2),
																pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(3),
																pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(4),
																pVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamageLevel(5));
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;
    }
}

void CNetObjVehicle::DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const
{
    const unsigned DEBUG_STR_LEN = 200;
	char debugStr[DEBUG_STR_LEN];

    CNetObjPhysical::DisplayBlenderInfo(screenCoords, playerColour, scale, debugYIncrement);

    CNetBlenderPhysical *netBlenderPhysical = GetNetBlender() ? SafeCast(CNetBlenderPhysical, GetNetBlender()) : 0;

    if(netBlenderPhysical)
    {
        s32 bbCheckTimer          = netBlenderPhysical->GetBBCheckTimer();
        s32 disableCollisionTimer = netBlenderPhysical->GetDisableCollisionTimer();

        if(bbCheckTimer > 0)
        {
            snprintf(debugStr, DEBUG_STR_LEN, "BB Check Timer: %d", bbCheckTimer);
	        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	        screenCoords.y += debugYIncrement;
        }

        if(disableCollisionTimer)
        {
            snprintf(debugStr, DEBUG_STR_LEN, "Disable Collision Timer: %d", disableCollisionTimer);
	        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	        screenCoords.y += debugYIncrement;
        }
    }
}

void CNetObjVehicle::GetHealthDisplayString(char *buffer) const
{
    if(buffer)
    {
        if(GetVehicle())
        {
            sprintf(buffer, "Total %.2f Engine %.2f Petrol %.2f", GetVehicle()->GetHealth(), GetVehicle()->m_Transmission.GetEngineHealth(), GetVehicle()->GetVehicleDamage()->GetPetrolTankHealth());

            if(GetVehicle()->GetStatus() == STATUS_WRECKED)
            {
                strcat(buffer, " WRECKED");
            }
        }
    }
}

void CNetObjVehicle::GetExtendedFlagsDisplayString(char *buffer) const
{
    if(buffer)
    {
        if(GetVehicle())
        {
            sprintf(buffer, " IsDummy: %s", GetVehicle()->IsDummy() ? "true": "false");
        }
    }
}

#endif

void CNetObjVehicle::UpdateBlenderData()
{
	CVehicle* pVehicle = GetVehicle();
	CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

	if(netBlenderVehicle->HasStoppedPredictingPosition())
	{
		// if the blender for this vehicle has stopped predicting position (due to not receiving a
		// position update for too long) stop the vehicle from driving off
		//
		// only set the throttle to zero if the brake isn't active - the remote here might be burning out in position in which case it will have a throttle and a brake and if we terminate the throttle the burnout stops on the remote - not good.
		if (!pVehicle->m_vehControls.m_handBrake && pVehicle->m_vehControls.m_brake < 0.1f)
			pVehicle->m_vehControls.m_throttle = 0.0f;
	}
	else
	{
		// ensure the vehicle is always using the last received value for the throttle
		pVehicle->m_vehControls.m_throttle = m_LastThrottleReceived;
	}

	if (pVehicle->InheritsFromAmphibiousAutomobile())
	{
		CAmphibiousAutomobile* pAmphibiousCar = static_cast<CAmphibiousAutomobile*>(pVehicle);

		if (pAmphibiousCar->GetShouldUseBoatNetworkBlend() && !m_usingBoatBlenderData)
		{				
			netBlenderVehicle->SetBlenderData(ms_boatBlenderData);
			m_usingBoatBlenderData = true;			
		}
		else if (!pAmphibiousCar->GetShouldUseBoatNetworkBlend() && m_usingBoatBlenderData)
		{
			netBlenderVehicle->SetBlenderData(ms_vehicleBlenderData);
			m_usingBoatBlenderData = false;
		}
	}	

	if (pVehicle->InheritsFromBike())
	{		
		u8 cloneGliderState = GetCloneGliderState();

		if (cloneGliderState == CVehicle::GLIDING && !pVehicle->HasContactWheels() && !m_usingPlaneBlenderData)
		{
			netBlenderVehicle->SetBlenderData(ms_planeBlenderData);
			m_usingPlaneBlenderData = true;
		}
		else if (pVehicle->HasContactWheels() && m_usingPlaneBlenderData)
		{
			netBlenderVehicle->SetBlenderData(ms_vehicleBlenderData);
			m_usingPlaneBlenderData = false;
		}
	}
}

static ObjectId gs_VehicleToCheck = NETWORK_INVALID_OBJECT_ID;
static bool IsUnregisteringOccupant(const netObject *networkObject)
{
    if(networkObject && networkObject->GetObjectType() == NET_OBJ_TYPE_PED)
    {
        const CNetObjPed *netObjPed = SafeCast(const CNetObjPed, networkObject);
        
        if(netObjPed->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING) && netObjPed->GetPedsTargetVehicleId() == gs_VehicleToCheck)
        {
            return true;
        }
    }

    return false;
}

void CNetObjVehicle::UpdateHasUnregisteringOccupants()
{
    if(m_hasUnregisteringOccupants)
    {
        gs_VehicleToCheck = GetObjectID();
        int numUnregisteringPeds = NetworkInterface::GetObjectManager().GetTotalNumObjects(IsUnregisteringOccupant);

        if(numUnregisteringPeds == 0)
        {
            m_hasUnregisteringOccupants = false;
        }
    }
}

void CNetObjVehicle::ResetLastPedsInSeats()
{
    for(unsigned index = 0; index < MAX_VEHICLE_SEATS; index++)
    {
        m_PedLastInSeat[index] = NETWORK_INVALID_OBJECT_ID;
    }
}

void CNetObjVehicle::UpdateLastPedsInSeats()
{
    CVehicle *vehicle = GetVehicle();

    if(gnetVerifyf(vehicle, "Invalid vehicle!"))
    {
        CPedSyncTreeBase      *pedSyncTree      = SafeCast(CPedSyncTreeBase, CNetObjPed::GetStaticSyncTree());
        CPedGameStateDataNode *pedGameStateNode = pedSyncTree ? pedSyncTree->GetPedGameStateNode() : 0;
        NETWORK_QUITF(pedSyncTree,      "Invalid ped sync tree!");
        NETWORK_QUITF(pedGameStateNode, "Invalid ped game state node!");

        m_PedInSeatStateFullySynced = true;

        const s32 maxSeats = vehicle->GetSeatManager()->GetMaxSeats();

        for(s32 seatIndex = 0; seatIndex < maxSeats; seatIndex++)
        {
            // check cached values first
            CPed          *currentOccupant  = vehicle->GetSeatManager()->GetPedInSeat(seatIndex);
            CPed          *previousOccupant = 0;
            netObject     *networkObject    = 0;
            const ObjectId pedLastInSeat    = m_PedLastInSeat[seatIndex];

            if(pedLastInSeat != NETWORK_INVALID_OBJECT_ID)
            {
                networkObject = currentOccupant ? currentOccupant->GetNetworkObject() : 0;

                if(!networkObject || networkObject->GetObjectID() != pedLastInSeat)
                {
                    networkObject = NetworkInterface::GetNetworkObject(pedLastInSeat);
                }
            }

            if(networkObject == 0 || networkObject->IsClone() || networkObject->GetObjectType() != NET_OBJ_TYPE_PED)
            {
                m_PedLastInSeat[seatIndex] = NETWORK_INVALID_OBJECT_ID;
            }
            else
            {
                previousOccupant = NetworkUtils::GetPedFromNetworkObject(networkObject);

                // the vehicle data is synced in the ped game state node - check if this is fully synced
                // to other players
                if(pedGameStateNode && networkObject->GetSyncData())
                {
                    PlayerFlags unsyncedPlayers = networkObject->GetSyncData()->GetSyncDataUnit(pedGameStateNode->GetDataIndex()).GetUnsyncedPlayers();
                    m_PedInSeatStateFullySynced &= (unsyncedPlayers & networkObject->GetClonedState()) == 0;

                    // if the ped is pending cloning on a machine that knows about this vehicle the seat state can't be synced,
                    // note this only needs to be done for previous occupants as the car won't migrate while any current occupants
                    // are pending cloning/removing
                    if(networkObject->IsPendingCloning())
                    {
                        m_PedInSeatStateFullySynced = false;
                    }
                }
            }

            // check if the occupant has changed
            if(currentOccupant && currentOccupant != previousOccupant)
            {
                networkObject = currentOccupant->GetNetworkObject();

                if(networkObject && !networkObject->IsClone())
                {
                    if(pedGameStateNode && networkObject->GetSyncData())
                    {
                        PlayerFlags unsyncedPlayers = networkObject->GetSyncData()->GetSyncDataUnit(pedGameStateNode->GetDataIndex()).GetUnsyncedPlayers();
                        m_PedInSeatStateFullySynced &= (unsyncedPlayers & networkObject->GetClonedState()) == 0;
                    }

                    m_PedLastInSeat[seatIndex] = networkObject->GetObjectID();
                }
                else
                {
                    m_PedLastInSeat[seatIndex] = NETWORK_INVALID_OBJECT_ID;
                }
            }
        }
    }
}

bool CNetObjVehicle::ShouldObjectBeHiddenForTutorial()  const
{
	// always show the local vehicle
	if(!IsClone())
		return false;

	bool bShouldBeHid = false;

	if(m_bPassControlInTutorial && IsGlobalFlagSet(GLOBALFLAG_CLONEALWAYS_SCRIPT))
	{
		// if vehicle's owner is also inside this vehicle, but is in a different tutorial session, allow hiding this vehicle under the world
		CNetGamePlayer* thisPlayer = GetPlayerOwner();
		if(thisPlayer && thisPlayer->GetPlayerPed() && thisPlayer->IsInDifferentTutorialSession())
		{
			if(GetVehicle()->ContainsPed(thisPlayer->GetPlayerPed()))
			{
				bShouldBeHid = true;
			}
		}
	}
	else
	{
		bShouldBeHid = CNetObjPhysical::ShouldObjectBeHiddenForTutorial();
	}

	return bShouldBeHid;
}

void CNetObjVehicle::HideForTutorial()
{
	if(!gnetVerifyf(IsClone(), "%s HideInTutorial should only be called on remote vehicles!",GetLogName()))
		return;

	CVehicle *vehicle = GetVehicle();
	gnetAssert(vehicle);

	if( m_bPassControlInTutorial &&
		IsGlobalFlagSet(GLOBALFLAG_CLONEALWAYS_SCRIPT) ) 
	{
		// some aspects of game interact with invisible objects so park under the map
		float fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION = -190.0f;
		Vector3 vPosition = NetworkInterface::GetLastPosReceivedOverNetwork(vehicle);
		vPosition.z = fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION;
		vehicle->SetPosition(vPosition, true, true, true);
	}

	CNetObjPhysical::HideForTutorial();
}

void CNetObjVehicle::HideForCutscene()
{
	CVehicle *pVehicle = GetVehicle();
	gnetAssert(pVehicle);

	// we only need to make super dummies invisible, and not affect their collision. All vehicles capable of super dummying, will be forced to super dummy during the cutscene
	if (pVehicle && (pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy)) && !pVehicle->IsCollisionEnabled())
	{
		CNetObjEntity::HideForCutscene();
	}
	else
	{
		CNetObjPhysical::HideForCutscene();
	}
}

#if __BANK

// name:        ValidateDriverAndPassengers
// description: Ensures the driver and passengers state doesn't get out of sync with the vehicle
//
void CNetObjVehicle::ValidateDriverAndPassengers()
{
    CVehicle *vehicle = GetVehicle();

    if(AssertVerify(vehicle))
    {
		bool bValidateFail = false;

        for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
        {
            CPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex);

            if(occupant && occupant->GetNetworkObject())
            {
                gnetAssert(occupant->GetMyVehicle() == vehicle);
                gnetAssert(occupant->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));

				CNetObjPed *pPedNetObj = SafeCast(CNetObjPed, occupant->GetNetworkObject());

				netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

				if (occupant->GetMyVehicle() != vehicle)
				{
					NetworkLogUtils::WriteLogEvent(log, "VEHICLE_OCCUPANT_ERROR", GetLogName());
					log.WriteDataValue("Occupant", "%s", pPedNetObj ? pPedNetObj->GetLogName() : "??");

					if (occupant->GetMyVehicle())
					{
						log.WriteDataValue("Occupants vehicle", "%s", occupant->GetMyVehicle()->GetNetworkObject() ? occupant->GetMyVehicle()->GetNetworkObject()->GetLogName() : "??");
					}
					else
					{
						log.WriteDataValue("Occupants vehicle", "-none-");
					}
				}
				else if (!occupant->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					NetworkLogUtils::WriteLogEvent(log, "VEHICLE_OCCUPANT_ERROR", GetLogName());
					log.WriteDataValue("Occupant", "%s", pPedNetObj ? pPedNetObj->GetLogName() : "??");
					log.WriteDataValue("Occupant not in vehicle", "true");
				}
				else
				{
					if (!IsClone() && 
						!pPedNetObj->IsClone() && 
						IsScriptObject() &&
						pPedNetObj->IsScriptObject() &&
						IsGlobalFlagSet(netObject::GLOBALFLAG_CLONEALWAYS) == pPedNetObj->IsGlobalFlagSet(netObject::GLOBALFLAG_CLONEALWAYS) &&
						IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT) == pPedNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT))
					{
						PlayerFlags vehClonedState, vehCreateState, vehRemoveState;
						PlayerFlags pedClonedState, pedCreateState, pedRemoveState;

						GetClonedState(vehClonedState, vehCreateState, vehRemoveState);
						pPedNetObj->GetClonedState(pedClonedState, pedCreateState, pedRemoveState);

						vehClonedState |= vehCreateState;
						vehClonedState &= ~vehRemoveState;

						pedClonedState |= pedCreateState;
						pedClonedState &= ~pedRemoveState;

						if (vehClonedState != pedClonedState)
						{
							bValidateFail = true;

							if (m_validateFailTimer > 5000)
							{
								NetworkLogUtils::WriteLogEvent(log, "VEHICLE_OCCUPANT_ERROR", GetLogName());
								log.WriteDataValue("Occupant", "%s", pPedNetObj ? pPedNetObj->GetLogName() : "??");
								log.WriteDataValue("Vehicle clone state", "%u, %u, %u", vehClonedState, vehCreateState, vehRemoveState);
								log.WriteDataValue("Occupant clone state", "%u, %u, %u", pedClonedState, pedCreateState, pedRemoveState);

								unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
								const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

								for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
								{
									const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
									PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();

									bool bVehClonedOnPlayer = (vehClonedState & (1<<playerIndex)) != 0;
									bool bPedClonedOnPlayer = (pedClonedState & (1<<playerIndex)) != 0;

									if (bVehClonedOnPlayer != bPedClonedOnPlayer)
									{
										log.WriteDataValue("Incorrect for player", "%s", pPlayer->GetLogName());
										log.WriteDataValue("Vehicle", "Cloned: %s, In scope: %s", bVehClonedOnPlayer ? "true" : "false", IsInScope(*pPlayer) ? "true" : "false");
										log.WriteDataValue("Occupant", "Cloned: %s, In scope: %s", bPedClonedOnPlayer ? "true" : "false", pPedNetObj->IsInScope(*pPlayer) ? "true" : "false");

										if (!bPedClonedOnPlayer)
										{
											log.WriteDataValue("Occupant in scope in vehicle", "%s", pPedNetObj->IsInScopeVehicleChecks(*pPlayer) ? "true" : "false");
											log.WriteDataValue("Occupant in scope no vehicle", "%s", pPedNetObj->IsInScopeNoVehicleChecks(*pPlayer) ? "true" : "false");
										}
									}
								}
							}
						}
					}
				}
            }
        }

		if (bValidateFail)
		{
			m_validateFailTimer += fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			m_validateFailTimer = 0;
		}
    }
}

#endif
