//
// name:        NetworkEventTypes.cpp
// description: Different NetworkEvent types
// written by:  John Gurney
//
#include "network/events/NetworkEventTypes.h"

// Rage headers
#include "fragmentnm/messageparams.h"

// Framework header
#include "ai/aichannel.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "fwnet/netblender.h"
#include "fwnet/neteventmgr.h"
#include "fwnet/netlogutils.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscript/scriptinterface.h"
#include "fwmaths/angle.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxscript.h"
#include "vfx/systems/VfxScript.h"

// Game headers
#include "animation/FacialData.h"
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "control/gamelogic.h"
#include "control/route.h"
#include "event/events.h"
#include "event/eventgroup.h"
#include "event/eventnetwork.h"
#include "event/EventDamage.h"
#include "event/EventScript.h"
#include "Frontend/MiniMap.h"
#include "Frontend/ReportMenu.h"
#include "Frontend/GameStreamMgr.h"
#include "game/clock.h"
#include "game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Network.h"
#include "network/Debug/NetworkDebug.h"
#include "network/general/NetworkDamageTracker.h"
#include "Network/General/NetworkHasherUtil.h"
#include "network/general/NetworkPendingProjectiles.h"
#include "network/General/NetworkSynchronisedScene.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"
#include "network/general/scriptworldstate/WorldStates/NetworkEntityAreaWorldState.h"
#include "network/general/scriptworldstate/WorldStates/NetworkScenarioBlockingAreaWorldState.h"
#include "Network/General/ScriptWorldState/WorldStates/NetworkPtFXWorldState.h"
#include "Network/Live/NetworkRemoteCheaterDetector.h"
#include "Network/Live/NetworkTelemetry.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/objects/entities/NetObjAutomobile.h"
#include "network/objects/entities/NetObjEntity.h"
#include "network/objects/entities/NetObjPickup.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/SyncNodes/PlayerSyncNodes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/players/NetGamePlayer.h"
#include "network/players/NetworkPlayerMgr.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "pedgroup/PedGroup.h"
#include "peds/ped.h"
#include "peds/PedHelmetComponent.h"
#include "peds/pedintelligence.h"
#include "peds/ped.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "scene/Entity.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"			// For CStreaming::HasObjectLoaded(), etc.
#include "fwscene/search/SearchVolumes.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "system/controlMgr.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/System/TaskTypes.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "task/Default/TaskIncapacitated.h"
#include "weapons/explosion.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/projectiles/projectile.h"
#include "weapons/projectiles/projectilemanager.h"
#include "weapons/projectiles/projectilerocket.h"
#include "weapons/weapon.h"
#include "vehicles/automobile.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/planes.h"
#include "vehicleAi/task/TaskVehicleGoToPlane.h"
#include "vehicles/cargen.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "script/commands_ped.h"
#include "script/script_channel.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "network/General/NetworkVehicleModelDoorLockTable.h"
#include "stats/StatsInterface.h"
#include "stats/StatsSavesMgr.h"
#include "Diag/output.h"
#include "system/InfoState.h"
#include "system/appcontent.h"

#include "control/replay/effects/ParticleMiscFxPacket.h"

#if RSG_PC
#include "Network/Shop/Catalog.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#endif //RSG_PC

#if __BANK
#include "system/stack.h"
#endif //__BANK

XPARAM(invincibleMigratingPeds);
PARAM(dontCrashOnScriptValidation, "Don't crash game if invalid script is trying to trigger events");

NETWORK_OPTIMISATIONS()

CompileTimeAssert(sizeof(CScriptArrayDataVerifyEvent)   <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestControlEvent)          <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CGiveControlEvent)             <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CWeaponDamageEvent)            <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestPickupEvent)           <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestMapPickupEvent)        <= LARGEST_NETWORKEVENT_CLASS);
#if !__FINAL
CompileTimeAssert(sizeof(CDebugGameClockEvent)			<= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CDebugGameWeatherEvent)		<= LARGEST_NETWORKEVENT_CLASS);
#endif
CompileTimeAssert(sizeof(CRespawnPlayerPedEvent)        <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CGiveWeaponEvent)              <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoveWeaponEvent)            <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoveAllWeaponsEvent)        <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CVehicleComponentControlEvent) <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CFireEvent)					  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CExplosionEvent)				  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CStartProjectileEvent)           <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CAlterWantedLevelEvent)          <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CChangeRadioStationEvent)        <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRagdollRequestEvent)            <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPlayerTauntEvent)               <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPlayerCardStatEvent)            <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CDoorBreakEvent)                 <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CScriptedGameEvent)              <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoteScriptInfoEvent)          <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoteScriptLeaveEvent)         <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CMarkAsNoLongerNeededEvent)      <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CConvertToScriptEntityEvent)     <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CScriptWorldStateEvent)          <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CIncidentEntityEvent)            <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CClearAreaEvent)                 <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestNetworkSyncedSceneEvent) <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CStartNetworkSyncedSceneEvent)   <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CStartNetworkPedArrestEvent)     <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CStartNetworkPedUncuffEvent)     <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CCarHornEvent)					  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CEntityAreaStatusEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CGarageOccupiedStatusEvent)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPedConversationLineEvent)	      <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPlaySoundEvent)				  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CStopSoundEvent)				  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPlayAirDefenseFireEvent)        <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CAudioBankRequestEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CAudioBarkingEvent)			  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestDoorEvent)				  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkTrainRequestEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkTrainReportEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkIncrementStatEvent)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CModifyVehicleLockWorldStateDataEvent)	<= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CModifyPtFXWorldStateDataScriptedEvolveEvent) <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestPhoneExplosionEvent)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRequestDetachmentEvent)         <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CSendKickVotesEvent)             <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CGivePickupRewardsEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CBlowUpVehicleEvent)			  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkRespondedToThreatEvent)  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPickupDestroyedEvent)			  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CUpdatePlayerScarsEvent)    	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkPtFXEvent)				  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CNetworkPedSeenDeadPedEvent)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoveStickyBombEvent)		  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CInformSilencedGunShotEvent)     <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CPedPlayPainEvent)               <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CReportMyselfEvent)              <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CReportCashSpawnEvent)           <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CClearRectangleAreaEvent)        <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CActivateVehicleSpecialAbilityEvent) <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CUpdateProjectileTargetEntity)	  <= LARGEST_NETWORKEVENT_CLASS);
CompileTimeAssert(sizeof(CRemoveProjectileEntity)		  <= LARGEST_NETWORKEVENT_CLASS);

#if RSG_PC
CompileTimeAssert(sizeof(CNetworkCheckCatalogCrc)         <= LARGEST_NETWORKEVENT_CLASS);
#endif //RSG_PC

CompileTimeAssert((LARGEST_NETWORKEVENT_CLASS%16)==0);

RAGE_DEFINE_SUBCHANNEL(net, eventtypes, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_eventtypes

// ===========================================================================================================
// SCRIPT ARRAY DATA VERIFY EVENT
// ===========================================================================================================

CScriptArrayDataVerifyEvent::CScriptArrayDataVerifyEvent()
#if ENABLE_NETWORK_LOGGING
: m_bScriptNotRunning(false)
, m_bPlayerNotAParticipant(false)
#endif // ENABLE_NETWORK_LOGGING
{
}

CScriptArrayDataVerifyEvent::CScriptArrayDataVerifyEvent(const netBroadcastDataArrayIdentifier<CGameScriptId>& identifier, u32 arrayChecksum)
: arrayDataVerifyEvent(HOST_BROADCAST_DATA_ARRAY_HANDLER, arrayChecksum)
, m_identifier(identifier.GetScriptId(), NULL, identifier.GetInstanceId(), identifier.GetDataId() BANK_ONLY(, identifier.GetDataDebugArrayName()))
#if ENABLE_NETWORK_LOGGING
, m_bScriptNotRunning(false)
, m_bPlayerNotAParticipant(false)
#endif // ENABLE_NETWORK_LOGGING
{
	m_EventType = SCRIPT_ARRAY_DATA_VERIFY_EVENT;
}

void CScriptArrayDataVerifyEvent::EventHandler(datBitBuffer &msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CScriptArrayDataVerifyEvent networkEvent;
    netInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CScriptArrayDataVerifyEvent::Trigger(const netBroadcastDataArrayIdentifier<CGameScriptId>& identifier, u32 arrayChecksum)
{
    netInterface::GetEventManager().CheckForSpaceInPool();
    CScriptArrayDataVerifyEvent *pEvent = rage_new CScriptArrayDataVerifyEvent(identifier, arrayChecksum);
    netInterface::GetEventManager().PrepareEvent(pEvent);
}

void CScriptArrayDataVerifyEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer& toPlayer )
{
	arrayDataVerifyEvent::Prepare(messageBuffer, toPlayer);
	m_identifier.Write(messageBuffer);
}

void CScriptArrayDataVerifyEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	arrayDataVerifyEvent::Handle(messageBuffer, fromPlayer, toPlayer);
	m_identifier.Read(messageBuffer);
}

bool CScriptArrayDataVerifyEvent::Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_identifier.GetScriptId());

	if (pHandler && pHandler->GetNetworkComponent())
	{
		if (pHandler->GetNetworkComponent()->IsPlayerAParticipant(fromPlayer))
		{
			arrayDataVerifyEvent::Decide(fromPlayer, toPlayer);
		}
		else
		{
			LOGGING_ONLY(m_bPlayerNotAParticipant = true;)
		}
	}
	else
	{
		LOGGING_ONLY(m_bScriptNotRunning = true;)
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CScriptArrayDataVerifyEvent::WriteEventToLogFile(bool LOGGING_ONLY(wasSent), bool LOGGING_ONLY(eventLogOnly), datBitBuffer *LOGGING_ONLY(messageBuffer)) const
{
	netLoggingInterface &log = eventLogOnly ? netInterface::GetEventManager().GetLog() : netInterface::GetEventManager().GetLogSplitter();

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Identifier", "%s", m_identifier.GetLogName());

	arrayDataVerifyEvent::WriteEventToLogFile(wasSent, eventLogOnly, messageBuffer);

	if (m_bScriptNotRunning)
	{
		log.WriteDataValue("IGNORED", "Script not running");
	}
	else if (m_bPlayerNotAParticipant)
	{
		log.WriteDataValue("IGNORED", "Player is not a participant");
	}
}

#endif //ENABLE_NETWORK_LOGGING

const netArrayHandlerBase* CScriptArrayDataVerifyEvent::GetArrayHandler() const
{
	const netArrayHandlerBase* pArray = netInterface::GetArrayManager().GetArrayHandler(m_ArrayType, &m_identifier);

	return pArray;
}

netArrayHandlerBase* CScriptArrayDataVerifyEvent::GetArrayHandler()
{
	netArrayHandlerBase* pArray = netInterface::GetArrayManager().GetArrayHandler(m_ArrayType, &m_identifier);

	return pArray;
}

// ===========================================================================================================
// REQUEST CONTROL EVENT
// ===========================================================================================================

CRequestControlEvent::CRequestControlEvent() :
netGameEvent( REQUEST_CONTROL_EVENT, false ),
m_objectID( NETWORK_INVALID_OBJECT_ID ),
m_objectType(NUM_NET_OBJ_TYPES),
m_TimeStamp(0),
m_CPCResultCode(CPC_SUCCESS)
{
}

CRequestControlEvent::CRequestControlEvent( netObject *pObject ) :
netGameEvent( REQUEST_CONTROL_EVENT, false ),
m_objectID( NETWORK_INVALID_OBJECT_ID ),
m_objectType(NUM_NET_OBJ_TYPES),
m_TimeStamp(0),
m_CPCResultCode(CPC_SUCCESS)
{
    gnetAssert(pObject);

    m_objectID = pObject->GetObjectID();
}

void CRequestControlEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRequestControlEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestControlEvent::Trigger(netObject* pObject NOTFINAL_ONLY(, const char* reason))
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "REQUESTING_CONTROL", "%s", pObject ? pObject->GetLogName() : "?");
	NOTFINAL_ONLY( NetworkInterface::GetEventManager().GetLog().WriteDataValue("Reason", reason); )

    gnetAssert(NetworkInterface::IsGameInProgress());

    NetworkInterface::GetEventManager().CheckForSpaceInPool();

    CRequestControlEvent *pEvent = rage_new CRequestControlEvent(pObject);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRequestControlEvent::IsInScope( const netPlayer& player ) const
{
	if (player.IsRemote())
	{
		netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_objectID, player);

		// sent to player that owns the object
		return (pObject && (&player == pObject->GetPlayerOwner()));
	}

	return false;
}

template <class Serialiser> void CRequestControlEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_objectID, "Local ID");
}

void CRequestControlEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestControlEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestControlEvent::Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
    netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_objectID, toPlayer);

	if (pObject)
	{
		if (pObject->CanPassControl(fromPlayer, MIGRATE_SCRIPT, &m_CPCResultCode))
		{
			if (!gnetVerifyf(pObject->GetSyncData() || pObject->CanPassControlWithNoGameObject(), "Trying to migrate %s with no sync data", pObject->GetLogName()))
			{
				m_CPCResultCode = CPC_FAIL_NO_SYNC_DATA;
			}
			else if(pObject->IsPendingOwnerChange())
			{
				m_CPCResultCode = CPC_FAIL_IS_MIGRATING;
			}
			else
			{
				CGiveControlEvent::Trigger(fromPlayer, pObject, MIGRATE_SCRIPT);

				// if a script vehicle is being requested, migrate any script ped occupants as well
				if (static_cast<CNetObjGame*>(pObject)->IsScriptObject()) 
				{
					CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pObject);

					if (vehicle)
					{
						for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
						{
							CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

							if (occupant && !occupant->IsClone() && occupant->IsScriptObject()) 
							{
								CGiveControlEvent::Trigger(fromPlayer, occupant, MIGRATE_SCRIPT);
							}
						}
					}
				}
			}
		}
		else
		{
			gnetAssert(m_CPCResultCode != CPC_SUCCESS);
		}
	}

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CRequestControlEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	netObject* obj = NetworkInterface::GetObjectManager().GetNetworkObject(m_objectID);
	log.WriteDataValue("Object", "%s", obj ? obj->GetLogName() : "does not exist");
}

void CRequestControlEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* obj = NetworkInterface::GetObjectManager().GetNetworkObject(m_objectID);

	if (!obj)
	{
		log.WriteDataValue("Rejected", "Object does not exist");
	}
	else if (m_CPCResultCode != CPC_SUCCESS)
	{
		log.WriteDataValue("Rejected", "%s", NetworkUtils::GetCanPassControlErrorString(m_CPCResultCode));
		if(m_CPCResultCode == CPC_FAIL_PED_TASKS && obj->GetObjectType() == NET_OBJ_TYPE_PED)
		{
			CNetObjPed* netPed = static_cast<CNetObjPed*>(obj);
			if(netPed && netPed->GetPed())
			{
				CPedIntelligence* pedIntelligence = netPed->GetPed()->GetPedIntelligence();
				if(pedIntelligence)
				{
					log.WriteDataValue("Task", "%s", pedIntelligence->GetLastMigrationFailTaskReason());
				}
			}
		}
	}
}

#endif // ENABLE_NETWORK_LOGGING

bool CRequestControlEvent::operator==( const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == REQUEST_CONTROL_EVENT)
    {
		const CRequestControlEvent* requestControlEvent = SafeCast(const CRequestControlEvent, pEvent);

        return (requestControlEvent->m_objectID == m_objectID);
    }

    return false;
}

// ===========================================================================================================
// GIVE CONTROL EVENT
// ===========================================================================================================

CGiveControlEvent::CGiveControlEvent() :
netGameEvent( GIVE_CONTROL_EVENT, true ),
m_pPlayer(NULL),
m_PhysicalPlayersBitmask(0),
m_TimeStamp(0),
m_bTooManyObjects(false),
m_bLocalPlayerPendingTutorialChange(false),
m_bAllObjectsMigrateTogether(false),
m_bPhysicalPlayerBitmaskDiffers(false),
m_numControlData(0),
m_numCriticalMigrations(0),
m_numScriptMigrations(0),
m_bLocallyTriggered(false)
{
}

CGiveControlEvent::~CGiveControlEvent()
{
	if (m_bLocallyTriggered)
	{
		// make sure the pending player index is cleared if the event is getting destroyed prematurely (ie if the event pool is full)
		for(u32 index = 0; index < m_numControlData; index++)
		{
			GiveControlData &giveControlData = m_giveControlData[index];

			netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID, true);

			if (pObject && pObject->IsPendingOwnerChange() && !giveControlData.m_bControlTaken)
			{
				pObject->ClearPendingPlayerIndex();

				if (pObject->GetEntity())
				{
					// stop entity migrating again for a wee while
					static_cast<CNetObjEntity*>(pObject)->SetProximityMigrationTimer(fwTimer::GetSystemTimeInMilliseconds() + 2000);

					if(giveControlData.m_migrationType == MIGRATE_SCRIPT)
					{
						static_cast<CNetObjEntity*>(pObject)->SetScriptMigrationTimer(fwTimer::GetSystemTimeInMilliseconds() + 2000);
					}
				}

				// if the object is a vehicle, then we need to clear the pending player ids on the occupants
				CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pObject);

				if (vehicle)
				{
					for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
					{
						CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

						if (occupant && 
							occupant->MigratesWithVehicle() && 
							(!m_pPlayer || occupant->GetPendingPlayerIndex() == m_pPlayer->GetPhysicalPlayerIndex()))
						{
							occupant->ClearPendingPlayerIndex();
						}
					}
				}
			}
		}
	}
}

bool CGiveControlEvent::CanAddGiveControlData(const netPlayer& player, netObject &object, eMigrationType migrationType)
{
    bool canAddGiveControlData = false;
	bool scriptObject = static_cast<CNetObjGame&>(object).IsScriptObject();

    if(m_numControlData < MAX_OBJECTS_PER_EVENT)
    {
		// critical migrations can have a lot of extra data, so less objects can be passed in a message. Critical migrations have to
		// occupy their own give control event, we can't mix proximity & critical give control events
		if (scriptObject)
		{
			if (m_numControlData == m_numScriptMigrations && 
				m_numScriptMigrations < MAX_NUM_SCRIPT_MIGRATIONS)
			{
				canAddGiveControlData = true;
			}
		}
		else if (netObject::IsCriticalMigration(migrationType))
		{
			if (m_numControlData == m_numCriticalMigrations && 
				m_numCriticalMigrations < MAX_NUM_NON_PROXIMITY_MIGRATIONS)
			{
				canAddGiveControlData = true;
			}
		}
		else if (m_numCriticalMigrations == 0 && m_numScriptMigrations == 0)
		{
			canAddGiveControlData = true;
		}

        if (canAddGiveControlData && m_numControlData > 0)
        {
            if(m_pPlayer != &player || (object.GetPlayerOwner() != GetOwnerPlayer()))
            {
                canAddGiveControlData = false;
            }
        }
    }

    return canAddGiveControlData;
}

bool CGiveControlEvent::ContainsGiveControlData(const netPlayer& player,
                                                netObject       &object,
                                                eMigrationType  NET_ASSERTS_ONLY(migrationType))
{
    for(u32 index = 0; index < m_numControlData; index++)
    {
        GiveControlData &giveControlData = m_giveControlData[index];

        if(&player                    == m_pPlayer &&
           giveControlData.m_objectID == object.GetObjectID())
        {
            gnetAssert(giveControlData.m_migrationType == migrationType);
            return true;
        }
    }

    return false;
}

void CGiveControlEvent::AddGiveControlData(const netPlayer &player,
                                           netObject       &object,
                                           eMigrationType  migrationType)
{
    bool canAddGiveControlData = m_bAllObjectsMigrateTogether || CanAddGiveControlData(player, object, migrationType);

	gnetAssertf(canAddGiveControlData, "Failed to add give control data for %s to event %s (migration type: %d, numControlData = %d, numCriticalMigrations = %d, numScriptMigrations = %d)", object.GetLogName(), GetEventName(), migrationType, m_numControlData, m_numCriticalMigrations, m_numScriptMigrations);

    if(canAddGiveControlData)
    {
        SetOwnerPlayer(object.GetPlayerOwner());

        GiveControlData &giveControlData = m_giveControlData[m_numControlData];

        gnetAssert(object.CanPassControl(player, migrationType));
        gnetAssert(!object.IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING));
		gnetAssert(object.HasGameObject() || object.CanPassControlWithNoGameObject());

        m_pPlayer								= &player;
        giveControlData.m_objectID				= object.GetObjectID();
        giveControlData.m_objectType			= object.GetObjectType();
        giveControlData.m_bControlTaken			= true; // need to set this so logging of extra data works
        giveControlData.m_migrationType			= migrationType;
		giveControlData.m_bCannotAccept			= false;
		giveControlData.m_bCannotApplyExtraData = false;
		giveControlData.m_bLocalPlayerIsDead	= false;
        giveControlData.m_CACResultCode			= CAC_SUCCESS;

        // tell the object that its owner is about to change
        object.SetPendingPlayerIndex(m_pPlayer->GetPhysicalPlayerIndex());

        // if the object is a vehicle, then  we need to set the pending peed ids on the occupants
        CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(&object);

        if (vehicle)
        {
            for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
            {
                CNetObjPed *occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex) ? static_cast<CNetObjPed*>(vehicle->GetSeatManager()->GetPedInSeat(seatIndex)->GetNetworkObject()) : 0;

				if (occupant && !occupant->IsClone() && occupant->GetPendingPlayerIndex() == INVALID_PLAYER_INDEX && occupant->MigratesWithVehicle())
                {
                    occupant->SetPendingPlayerIndex(m_pPlayer->GetPhysicalPlayerIndex());
                }
            }
         }

        m_numControlData++;

		if (static_cast<CNetObjGame&>(object).IsScriptObject())
		{
			m_numScriptMigrations++;
		}
		else if (netObject::IsCriticalMigration(migrationType))
		{
			m_numCriticalMigrations++;
		}
    }
}

void CGiveControlEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CGiveControlEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGiveControlEvent::Trigger(const netPlayer& player, netObject* pObject, eMigrationType migrationType)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    // if we are migrating a ped check whether it has been frozen,
    // if so we change the migration type as the migration is more critical, this
    // is overridden here to remove the need for all calling code to check this
    if(pObject && pObject->GetObjectType() == NET_OBJ_TYPE_PED)
    {
        CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pObject);

        if(pPed && pPed->m_nDEflags.bFrozen)
        {
            migrationType = MIGRATE_FROZEN_PED;
        }
    }

	bool bCriticalMigration = netObject::IsCriticalMigration(migrationType);

    gnetAssert(NetworkInterface::IsGameInProgress());
    gnetAssert(pObject);
	gnetAssert(bCriticalMigration || !pObject->IsGlobalFlagSet(netObject::GLOBALFLAG_PERSISTENTOWNER));
	gnetAssert(!pObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_TELEPORT));
    gnetAssert(!pObject->IsClone());
    gnetAssert(!player.IsMyPlayer() || (pObject->GetPlayerOwner() && pObject->GetPlayerOwner()->IsLocal() && pObject->GetPlayerOwner()->IsBot()));
    gnetAssert(!pObject->IsPendingOwnerChange() || pObject->GetPendingPlayerOwner() == &player);

	if (!gnetVerifyf(pObject->GetSyncData() || static_cast<CNetObjGame*>(pObject)->CanPassControlWithNoSyncData(), "Trying to pass control of %s with no sync data", pObject->GetLogName()))
	{
		return;
	}
	 
	// dont send event again if an event has already been triggered
    // (the object has a pending player id the same as the one of pPlayer)
    if (!pObject->IsPendingOwnerChange())
    {
        if (pObject->CanPassControl(player, migrationType))
        {
            // check all of the events in the pool to ensure this event is not already on
            // the queue, and try to find an empty event if it isn't
            bool               eventExists = false;
            CGiveControlEvent *eventToUse  = 0;

            atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

            while (node)
            {
                netGameEvent *networkEvent = node->Data;

                if (networkEvent && networkEvent->GetEventType() == GIVE_CONTROL_EVENT && !networkEvent->IsFlaggedForRemoval())
                {
                    CGiveControlEvent *giveControlEvent = static_cast<CGiveControlEvent *>(networkEvent);

                    if (giveControlEvent->ContainsGiveControlData(player, *pObject, migrationType))
                    {
                        eventExists = true;
						break;
                    }
                    else if (giveControlEvent->CanAddGiveControlData(player, *pObject, migrationType) && 
							!giveControlEvent->HasBeenSent() && 
							!giveControlEvent->m_bAllObjectsMigrateTogether)
                    {
                        eventToUse = giveControlEvent;
						break;
                    }
                }

                node = node->GetNext();
            }

            if(!eventExists)
            {
				if (!CheckForVehicleAndScriptPassengers(player, pObject, migrationType))
				{
					// have to check CanAddGiveControlData() again here, as CheckForVehicleAndScriptPassengers() can trigger events for script passengers
					if (eventToUse && !eventToUse->CanAddGiveControlData(player, *pObject, migrationType))
					{
						eventToUse = 0;
					}

					if(eventToUse)
					{
						eventToUse->AddGiveControlData(player, *pObject, migrationType);
					}
					else
					{
						if (NetworkInterface::GetEventManager().CheckForSpaceInPool(bCriticalMigration))
						{
							eventToUse = rage_new CGiveControlEvent();
							eventToUse->m_bLocallyTriggered = true;

							eventToUse->AddGiveControlData(player, *pObject, migrationType);

							if (gnetVerify(pObject->IsPendingOwnerChange()))
							{
								NetworkInterface::GetEventManager().PrepareEvent(eventToUse);
							}
							else
							{
								delete eventToUse;
							}
						}
						else
						{
							gnetAssertf(0, "Warning - network event pool full, dumping give control event");
							return;
						}
					}
				}
            }
        }
    }
}

bool CGiveControlEvent::IsInScope( const netPlayer& player ) const
{
    bool inScope = false;

    if (m_numControlData > 0)
    {
        // sent to player being given control of the objects
        inScope = (&player == m_pPlayer);
    }

    return inScope;
}

bool CGiveControlEvent::CheckForVehicleAndScriptPassengers(const netPlayer& player, netObject* pObject, eMigrationType migrationType)
{
	CGiveControlEvent *newEvent  = 0;

	bool bFoundScriptPassengers = false;

	// if the object is a vehicle and being driven by a script ped, then the script ped has to migrate with it. 
	// They are added separately so that any critical migration data is sent with them. This would get lost otherwise.
	CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pObject);

	if (vehicle)
	{
		if (vehicle->GetDriver() && !NetworkUtils::IsNetworkCloneOrMigrating(vehicle->GetDriver()))
		{
			CNetObjPed *pDriverObj = static_cast<CNetObjPed*>(vehicle->GetDriver()->GetNetworkObject());

			if (pDriverObj && pDriverObj->IsScriptObject())
			{
				bFoundScriptPassengers = true;

				// if we can't pass control of the driver, the event is dumped
				if (!pDriverObj->CanPassControl(player, migrationType))
				{
					return true;
				}

				// create a new event for the driver + vehicle
				if (NetworkInterface::GetEventManager().CheckForSpaceInPool(netObject::IsCriticalMigration(migrationType)))
				{
					newEvent = rage_new CGiveControlEvent();
					newEvent->m_bLocallyTriggered = true;
					newEvent->m_bAllObjectsMigrateTogether = true;
					newEvent->AddGiveControlData(player, *pDriverObj, migrationType);
					newEvent->AddGiveControlData(player, *pObject, migrationType);
				}
				else
				{
					gnetAssertf(0, "Warning - network event pool full, dumping give control event");
				}
			}
		}

		// migrate any remaining script passengers
		for(s32 seatIndex = 0; seatIndex < vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
		{
			CPed* occupant = vehicle->GetSeatManager()->GetPedInSeat(seatIndex);

			if (occupant && occupant != vehicle->GetDriver())
			{
				CNetObjPed *occupantObj = static_cast<CNetObjPed*>(occupant->GetNetworkObject());

				if (occupantObj && 
					!occupantObj->IsClone() && 
					!occupantObj->IsPendingOwnerChange() && 
					occupantObj->IsScriptObject() && 
					occupantObj->CanPassControl(player, migrationType))
				{
					CGiveControlEvent::Trigger(player, occupantObj, migrationType);
				}
			}
		}
	}

	if (newEvent)
	{
		NetworkInterface::GetEventManager().PrepareEvent(newEvent);
	}

	return bFoundScriptPassengers;
}

void CGiveControlEvent::CheckForAllObjectsMigratingTogether()
{
	bool bRejectAll = false;

	if (m_bAllObjectsMigrateTogether)
	{
		for(u32 index = 0; index < m_numControlData; index++)
		{
			GiveControlData &giveControlData = m_giveControlData[index];

			if (!giveControlData.m_bControlTaken)
			{
				bRejectAll = true;
				break;
			}
		}
	}

	if (bRejectAll)
	{
		for(u32 index = 0; index < m_numControlData; index++)
		{
			GiveControlData &giveControlData = m_giveControlData[index];

			if (giveControlData.m_bControlTaken)
			{
				giveControlData.m_bControlTaken = false;
				giveControlData.m_bCannotAccept = true;
			}
		}
	}
}

u32 CGiveControlEvent::GetPlayersInSessionBitMask() const
{
    u32 playersInSessionBitmask = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

    PhysicalPlayerIndex localPhysicalPlayerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();

    if(gnetVerifyf(localPhysicalPlayerIndex != INVALID_PLAYER_INDEX, "Invalid local physical player index!"))
    {
        playersInSessionBitmask |= (1<<localPhysicalPlayerIndex);
    }

    return playersInSessionBitmask;
}

template <class Serialiser> void CGiveControlEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    // write the bitmask of the physical players in the session
    SERIALISE_BITFIELD(serialiser, m_PhysicalPlayersBitmask, MAX_NUM_PHYSICAL_PLAYERS, "Physical Players In Session");

    // write the number of objects being passed control of
	SERIALISE_UNSIGNED(serialiser, m_numControlData, SIZEOF_NUM_EVENTS, "Number of control data");
	SERIALISE_BOOL(serialiser, m_bAllObjectsMigrateTogether, "All objects migrate together");

	m_numControlData = MIN(m_numControlData, MAX_OBJECTS_PER_EVENT);
    for(u32 index = 0; index < m_numControlData; index++)
    {
		SERIALISE_OBJECTID(serialiser, m_giveControlData[index].m_objectID, "Local ID");
		SERIALISE_UNSIGNED(serialiser, m_giveControlData[index].m_objectType, SIZEOF_OBJECTTYPE, "Object Type");
		SERIALISE_MIGRATION_TYPE(serialiser, m_giveControlData[index].m_migrationType, "Migration Type");
    }
}

template <class Serialiser> void CGiveControlEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    for(u32 index = 0; index < m_numControlData; index++)
    {
        SERIALISE_BOOL(serialiser, m_giveControlData[index].m_bControlTaken, "Is Control Taken");

        if(!m_giveControlData[index].m_bControlTaken)
        {
            m_giveControlData[index].m_bObjectExists = (NetworkInterface::GetObjectManager().GetNetworkObject(m_giveControlData[index].m_objectID) != NULL);
            SERIALISE_BOOL(serialiser, m_giveControlData[index].m_bObjectExists, "Object Exists");
        }
    }

    SERIALISE_BOOL(serialiser, m_bTooManyObjects, "Too many objects");
}

void CGiveControlEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    m_PhysicalPlayersBitmask = GetPlayersInSessionBitMask();

    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CGiveControlEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);

    for(u32 index = 0; index < m_numControlData; index++)
    {
        GiveControlData &giveControlData = m_giveControlData[index];

        giveControlData.m_bControlTaken			= false;
		giveControlData.m_bCannotAccept			= false;
		giveControlData.m_bCannotApplyExtraData = false;
        giveControlData.m_bLocalPlayerIsDead	= false;
        giveControlData.m_bTypesDiffer          = false;
    }

    m_pPlayer                       = &fromPlayer;
    m_bTooManyObjects               = false;
    m_bPhysicalPlayerBitmaskDiffers = false;
}


bool CGiveControlEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    if (NetworkDebug::ShouldIgnoreGiveControlEvents())
    {
        return false;
    }

    if(m_PhysicalPlayersBitmask != GetPlayersInSessionBitMask())
    {
        m_bPhysicalPlayerBitmaskDiffers = true;
    }

    for(u32 index = 0; index < m_numControlData; index++)
    {
        GiveControlData &giveControlData = m_giveControlData[index];

        netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(giveControlData.m_objectID, fromPlayer);

        if (pObject)
        {
			NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_TRY_TO_MIGRATE, *pObject);

            if(m_bPhysicalPlayerBitmaskDiffers)
            {
                giveControlData.m_bControlTaken = false;
            }
            else if (giveControlData.m_objectType != pObject->GetObjectType())
            {
                gnetAssertf(0, "%s has a different type on the sender's machine! Local:%d, Sender:%d", pObject->GetLogName(), static_cast<int>(pObject->GetObjectType()), static_cast<int>(giveControlData.m_objectType));

                giveControlData.m_bTypesDiffer  = true;
                giveControlData.m_bControlTaken = false;
            }
			else if (pObject->CanAcceptControl(fromPlayer, giveControlData.m_migrationType, &giveControlData.m_CACResultCode))
            {
                if(fromPlayer.IsLocal())
                {
                    giveControlData.m_bControlTaken = true;
                }
                else
                {
                    // check if we already own the maximum number of objects of this type
                    bool canTakeOwnershipOfType = CNetworkObjectPopulationMgr::CanTakeOwnershipOfObjectOfType(pObject->GetObjectType(), static_cast<CNetObjGame*>(pObject)->IsScriptObject());
					
					const CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();
					const CNetObjPlayer *localNetObjPlayer = NULL;
					const CPed* localPlayerPed = localPlayer ? localPlayer->GetPlayerPed() : NULL;

					if (localPlayerPed)
					{
						localNetObjPlayer = SafeCast(const CNetObjPlayer, localPlayerPed->GetNetworkObject());
					}

					if (!canTakeOwnershipOfType && localPlayerPed)
					{
						TUNE_FLOAT(PROXIMITY_MIGRATION_ACCEPT_DIST, 30.0f, 10.0f, 100.0f, 1.0f);

						// allow vehicles to migrate if they are very close to the local player
						if (pObject->GetEntity() && pObject->GetEntity()->GetIsTypeVehicle())
						{
							Vector3 entityPos = VEC3V_TO_VECTOR3(pObject->GetEntity()->GetTransform().GetPosition());
							Vector3 localPlayerPos = VEC3V_TO_VECTOR3(localPlayerPed->GetTransform().GetPosition());
							Vector3 diff = entityPos - localPlayerPos;

							if (diff.Mag2() <= PROXIMITY_MIGRATION_ACCEPT_DIST*PROXIMITY_MIGRATION_ACCEPT_DIST)
							{
								canTakeOwnershipOfType = true;
							}
						}
					}

					bool bProximityMigration = netObject::IsProximityMigration(giveControlData.m_migrationType);
                    bool bOutOfScope         = giveControlData.m_migrationType == MIGRATE_OUT_OF_SCOPE;

					if(bProximityMigration && !bOutOfScope && (!canTakeOwnershipOfType || NetworkInterface::GetObjectManager().IsOwningTooManyObjects()))
					{
						m_bTooManyObjects = true;
					}
					else if (bProximityMigration && !bOutOfScope && !(localPlayer && localPlayer->CanAcceptMigratingObjects()))
					{
						// can't accept control of objects when we are about to respawn, or we are in a MP cutscene
						giveControlData.m_bLocalPlayerIsDead = true;
					}
					else if (localNetObjPlayer && localNetObjPlayer->IsTutorialSessionChangePending())
					{
						// can't accept control of objects when we are about to change tutorial session
						m_bLocalPlayerPendingTutorialChange = true;
					}
					else
					{
						giveControlData.m_bControlTaken = true;
					}
                }
            }
            else
            {
                giveControlData.m_bCannotAccept = true;
                giveControlData.m_bControlTaken = false;
            }
        }
        else
        {
            giveControlData.m_bControlTaken = false;
        }
    }

	CheckForAllObjectsMigratingTogether();

    return true;
}

void CGiveControlEvent::PrepareReply ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CGiveControlEvent::HandleReply ( datBitBuffer& messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEventReply<CSyncDataReader>(messageBuffer);

    for(u32 index = 0; index < m_numControlData; index++)
    {
        GiveControlData &giveControlData = m_giveControlData[index];

        netObject* pObject = NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID, true);

        // object may have been deleted since event was triggered, or it may have migrated to another machine if a lot of packets were dropped
        if (pObject && pObject->IsPendingOwnerChange())
        {
            gnetAssertf(pObject->GetPendingPlayerOwner() == &fromPlayer, "Pending owner is incorrect! Expected %s, Actual %s", fromPlayer.GetLogName(), pObject->GetPendingPlayerOwner() ? pObject->GetPendingPlayerOwner()->GetLogName() : "Unknown");

            if (!giveControlData.m_bControlTaken)
            {				
				if(!giveControlData.m_bObjectExists)
                {
                    bool bHasBeenCloned  = pObject->HasBeenCloned(fromPlayer);
                    bool bPendingCloning = pObject->IsPendingCloning(fromPlayer);
                    bool bPendingRemoval = pObject->IsPendingRemoval(fromPlayer);

                    if(bHasBeenCloned && !bPendingCloning && !bPendingRemoval)
                    {
                        pObject->SetBeenCloned(fromPlayer, false);

                        if (pObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
                        {
                            if(!pObject->HasBeenCloned())
                            {
                                NetworkInterface::GetObjectManager().ForceRemovalOfUnregisteringObject(pObject);
                            }
                        }
                    }
                }
            }
            else if (pObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                // the other machine owns the object now, so we can clean up here
                NetworkInterface::GetObjectManager().ForceRemovalOfUnregisteringObject(pObject);
            }
        }
    }

    if(m_bTooManyObjects)
    {
        NetworkInterface::GetObjectManager().TooManyObjectsOwnedACKReceived(fromPlayer);
    }
}

void CGiveControlEvent::PrepareExtraData( datBitBuffer &messageBuffer, bool bReply, const netPlayer &player)
{
    if (!bReply)
    {
        m_TimeStamp = netInterface::GetTimestampForPositionUpdates();
        messageBuffer.WriteUns(m_TimeStamp, SIZEOF_TIMESTAMP);

        for(u32 index = 0; index < m_numControlData; index++)
        {
            GiveControlData &giveControlData = m_giveControlData[index];

			// include unregistering objects, this can happen when migrating ambient props 
            CNetObjGame* pObject = SafeCast(CNetObjGame, NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID, true));
            gnetAssert(m_pPlayer == &player);

			if (pObject && !pObject->HasGameObject() && !pObject->CanPassControlWithNoGameObject())
			{
				gnetAssertf(0, "Trying to pass control of %s, which has no game object", pObject->GetLogName());
				pObject = NULL;
			}

			if (pObject && pObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
			{
				Warningf("Trying to pass control of %s, which is now being reassigned", pObject->GetLogName());
				pObject = NULL;
			}

            // write the migration data for the object
            if(pObject && !pObject->IsClone())
            {
                messageBuffer.WriteBool(true);

                ActivationFlags actFlags = pObject->GetActivationFlags();

				if (netObject::IsProximityMigration(giveControlData.m_migrationType))
				{
					actFlags |= ACTIVATIONFLAG_PROXIMITY_MIGRATE;
				}

				// sanity check migration is ok at this point
				ASSERT_ONLY(pObject->ValidateMigration());

				unsigned syncTime = netInterface::GetSynchronisationTime();

				// update the sync tree first to make sure that we are sending the current state of the object
				if (pObject->GetSyncData())
				{
					pObject->GetSyncTree()->Update(pObject, pObject->GetActivationFlags(), syncTime);
				}

				u8 tempMessageBuffer[MAX_MESSAGE_PAYLOAD_BYTES];

				datBitBuffer tempBitBuffer;
				tempBitBuffer.SetReadWriteBits(tempMessageBuffer, MAX_MESSAGE_PAYLOAD_BITS, 0);

                pObject->GetSyncTree()->Write(SERIALISEMODE_MIGRATE,
                                              actFlags,
                                              pObject,
                                              tempBitBuffer,
                                              syncTime,
											  &NetworkInterface::GetMessageLog(),
                                              player.GetPhysicalPlayerIndex());

				u32 numBitsFree = messageBuffer.GetMaxBits() - messageBuffer.GetNumBitsWritten();

				if (tempBitBuffer.GetNumBitsWritten() <= numBitsFree)
				{
					tempBitBuffer.SetCursorPos(0);
					messageBuffer.WriteBits(tempBitBuffer, tempBitBuffer.GetNumBitsWritten());
				}
				else
				{
#if ENABLE_NETWORK_LOGGING
					WriteExtraDataToLogFile(messageBuffer, false);
#endif // ENABLE_NETWORK_LOGGING
					NETWORK_QUITF(0, "CGiveControlEvent::PrepareExtraData - ran out of space in the event to write the data for %s (index %d, numBitsFree = %d, numBitsWritten = %d)", pObject->GetLogName(), index, numBitsFree, tempBitBuffer.GetNumBitsWritten());
				}
            }
            else
            {
                messageBuffer.WriteBool(false);
            }
        }
    }
}

void CGiveControlEvent::HandleExtraData( datBitBuffer &messageBuffer, bool bReply, const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
    if (!bReply)
    {
        messageBuffer.ReadUns(m_TimeStamp, SIZEOF_TIMESTAMP);

		netObject* objectsToMigrate[MAX_OBJECTS_PER_EVENT];

		bool bRejectAll = false;

        for(u32 index = 0; index < m_numControlData; index++)
        {
            GiveControlData &giveControlData = m_giveControlData[index];

            netObject* pObject = m_pPlayer ? NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(giveControlData.m_objectID, *m_pPlayer) : 0;

			objectsToMigrate[index] = pObject;

            bool bObject = false;
            messageBuffer.ReadBool(bObject);

            if (bObject)
            {
                bool typeValid = (pObject == 0) || (pObject->GetObjectType() == giveControlData.m_objectType);
                gnetAssert(typeValid || !giveControlData.m_bControlTaken);

				ActivationFlags actFlags = netObject::IsProximityMigration(giveControlData.m_migrationType) ? ACTIVATIONFLAG_PROXIMITY_MIGRATE : 0;
				
#if ENABLE_NETWORK_LOGGING
				if (pObject)
				{
					NetworkInterface::GetMessageLog().Log("\tEXTRA DATA FOR %s:\r\n", pObject->GetLogName());
				}
#endif // ENABLE_NETWORK_LOGGING

				NetworkInterface::GetObjectManager().GetSyncTree(giveControlData.m_objectType)->Read(SERIALISEMODE_MIGRATE,
                                                                                             actFlags,
                                                                                             messageBuffer,
                                                                                             &NetworkInterface::GetMessageLog());

				if (pObject)
				{
					if ( pObject->IsClone() && giveControlData.m_bControlTaken && typeValid && !bRejectAll)
					{
						// we don't need to apply the migration data when passing control between local players
						if (!fromPlayer.IsLocal())
						{
							if (!pObject->GetSyncTree()->CanApplyNodeData(pObject))
							{
								giveControlData.m_bCannotApplyExtraData = true;
								giveControlData.m_bControlTaken = false;

								if (m_bAllObjectsMigrateTogether)
								{
									bRejectAll = true;
								}
							}
							else 
							{
								// update the network blender last update timestamp
								if(pObject->GetNetBlender())
								{
									pObject->GetNetBlender()->SetLastSyncMessageTime(m_TimeStamp);
								}

								// apply the migration data
								pObject->PreSync();
								pObject->GetSyncTree()->ApplyNodeData(pObject);
								pObject->PostSync();

								// run the network blender to ensure the object is popped to it's new target location if too
								// far out of sync
								if(pObject->GetNetBlender() && pObject->CanBlend())
								{
									pObject->GetNetBlender()->Update();
								}
							}
						}
					}
				}
            }
        }

		CheckForAllObjectsMigratingTogether();

		for(u32 index = 0; index < m_numControlData; index++)
		{
			GiveControlData &giveControlData = m_giveControlData[index];

			netObject* pObject = objectsToMigrate[index];

			if (giveControlData.m_bControlTaken && pObject)
			{
				//DAN TEMP - taking control of a ped in a vehicle we are already controlling,
				//			 we need to synchronise the ped so the ownership token is sent
				CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pObject);
				if(pPed && pPed->GetMyVehicle() && !pPed->GetMyVehicle()->IsNetworkClone())
				{
					pObject->SetLocalFlag(CNetObjGame::LOCALFLAG_FORCE_SYNCHRONISE, true);
				}

				NetworkInterface::GetObjectManager().ChangeOwner(*pObject, toPlayer, giveControlData.m_migrationType);
				gnetAssert(!pObject->IsClone());
			}
		}
    }
}

#if ENABLE_NETWORK_LOGGING

void CGiveControlEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	if (m_bAllObjectsMigrateTogether)
	{
		log.WriteDataValue("All objects migrate together", "true");
	}

    for(u32 index = 0; index < m_numControlData; index++)
    {
        const GiveControlData &giveControlData = m_giveControlData[index];

		netObject* pObject = NULL;		

        if (gnetVerify(giveControlData.m_objectID != NETWORK_INVALID_OBJECT_ID))
        {
			pObject = NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID, true);

            if (pObject)
            {
                log.WriteDataValue("Object", "%s", pObject->GetLogName());
            }
            else
            {
                log.WriteDataValue("Object id", "%d", giveControlData.m_objectID);
            }
        }

		LogMigrationType(log, giveControlData.m_migrationType, "Migration type");
	}

	log.WriteDataValue("PlayerBitmask", "%d", m_PhysicalPlayersBitmask);
}

void CGiveControlEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    for(u32 index = 0; index < m_numControlData; index++)
    {
        const GiveControlData& giveControlData = m_giveControlData[index];

        if (giveControlData.m_bControlTaken)
        {
            netObject* pObject = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(giveControlData.m_objectID, *NetworkInterface::GetLocalPlayer()) : 0;

            if(gnetVerifyf(pObject, "We are accepting control of an object we don't know about! How is this possible?"))
            {
                log.WriteDataValue("Reply", "Accept - %s", pObject->GetLogName());
            }
            else
            {
                log.WriteDataValue("Reply", "Accept - ?_%d", giveControlData.m_objectID);
            }
        }
        else
        {
            netObject* pObject = (NETWORK_INVALID_OBJECT_ID != giveControlData.m_objectID) && m_pPlayer ? NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(giveControlData.m_objectID, *m_pPlayer) : NULL;

            if (!pObject)
            {
                log.WriteDataValue("Reply", "Reject - Object doesn't exist");
            }
            else if (giveControlData.m_bTypesDiffer)
            {
                log.WriteDataValue("Reply", "Reject - The object types differ on the sender and this machine");
            }
            else if (m_bPhysicalPlayerBitmaskDiffers)
            {
                log.WriteDataValue("Reply", "Reject - The players in the session differ between the sender and this machine - likely a player is in the process of joining/leaving");
				log.WriteDataValue("Synced PlayerBitmask", "%d", m_PhysicalPlayersBitmask);
				log.WriteDataValue("Local PlayerBitmask", "%d", GetPlayersInSessionBitMask());
			}
            else if (giveControlData.m_bCannotAccept)
            {
                log.WriteDataValue("Reply", "Reject - The object cannot accept ownership");
                log.WriteDataValue("Reason", NetworkUtils::GetCanAcceptControlErrorString(giveControlData.m_CACResultCode));
            }
			else if (giveControlData.m_bCannotApplyExtraData)
			{
				log.WriteDataValue("Reply", "Reject - The object cannot apply the extra data");
				log.WriteDataValue("Reason", NetworkUtils::GetCanAcceptControlErrorString(giveControlData.m_CACResultCode));
			}
            else if(m_bTooManyObjects)
            {
                log.WriteDataValue("Reply", "Reject - Too many objects");
            }
            else if (giveControlData.m_bLocalPlayerIsDead)
            {
                log.WriteDataValue("Reply", "Reject - Local player is dead and about to respawn elsewhere");
            }
			else if (m_bLocalPlayerPendingTutorialChange)
			{
				log.WriteDataValue("Reply", "Reject - Local player is pending tutorial session change");
			}
            else
            {
                log.WriteDataValue("Reply", "Reject - Object cannot change owner");
            }
        }
    }
}

void CGiveControlEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    for(u32 index = 0; index < m_numControlData; index++)
    {
        const GiveControlData &giveControlData = m_giveControlData[index];

		netObject* pObject = (NETWORK_INVALID_OBJECT_ID != giveControlData.m_objectID) ? NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID) : NULL;
        
        if(!pObject)
        {
            log.WriteDataValue("ControlTaken", "Reject - ?_%d", giveControlData.m_objectID);
        }
        else if (!giveControlData.m_bControlTaken)
        {
            log.WriteDataValue("ControlTaken", "Reject - %s", pObject->GetLogName());
        }
        else
        {
            log.WriteDataValue("ControlTaken", "Accept - %s", pObject->GetLogName());
        }
    }
}

void CGiveControlEvent::WriteExtraDataToLogFile( datBitBuffer &UNUSED_PARAM(messageBuffer), bool LOGGING_ONLY(bReply)) const
{
    if(!NetworkInterface::GetMessageLog().IsEnabled())
    {
        return;
    }

    if(!bReply)
    {
	    for(u32 index = 0; index < m_numControlData; index++)
	    {
		    const GiveControlData &giveControlData = m_giveControlData[index];

		    netObject* pObject = NULL;		

		    if (gnetVerify(giveControlData.m_objectID != NETWORK_INVALID_OBJECT_ID))
		    {
			    pObject = NetworkInterface::GetObjectManager().GetNetworkObject(giveControlData.m_objectID, true);

			    if (pObject)
			    {
				    NetworkInterface::GetMessageLog().Log("\tEXTRA DATA FOR %s:\r\n", pObject->GetLogName());
				    pObject->GetSyncTree()->LogData(NetworkInterface::GetMessageLog());
			    }
		    }
	    }
    }
}

#endif // ENABLE_NETWORK_LOGGING

bool CGiveControlEvent::MustPersist() const
{
    bool mustPersist = false;

    for(u32 index = 0; index < m_numControlData; index++)
    {
        const GiveControlData &giveControlData = m_giveControlData[index];

		mustPersist |= netObject::IsCriticalMigration(giveControlData.m_migrationType);
    }

    return mustPersist;
}

// ===========================================================================================================
// WEAPON DAMAGE EVENT
// ===========================================================================================================

float CWeaponDamageEvent::LOCAL_HIT_POSITION_MAG = 90.0f;
float CWeaponDamageEvent::LOCAL_ENTITY_WEAPON_HIT_POSITION_MAG = 1.0f;
const float CWeaponDamageEvent::MELEE_RANGE_SQ = 100.0f * 100.0f;

CWeaponDamageEvent::PlayerDamageTime        CWeaponDamageEvent::ms_aLastPlayerKillTimes[MAX_NUM_PHYSICAL_PLAYERS];
CWeaponDamageEvent::PlayerDelayedKillData   CWeaponDamageEvent::ms_delayedKillData;

CWeaponDamageEvent::CWeaponDamageEvent() :
netGameEvent( WEAPON_DAMAGE_EVENT, false ),
m_damageType(DAMAGE_OBJECT),
m_parentID(NETWORK_INVALID_OBJECT_ID),
m_hitObjectID(NETWORK_INVALID_OBJECT_ID),
m_hitPosition(Vector3(0.0f, 0.0f, 0.0f)),
m_component(-1),
m_bOverride(false),
m_weaponHash(0),
m_weaponDamage(0),
m_weaponDamageAggregationCount(0),
m_tyreIndex(-1),
m_suspensionIndex(-1),
m_damageTime(0),
m_damageFlags(0),
m_actionResultId(0),
m_meleeId(0),
m_nForcedReactionDefinitionID(0),
m_hitEntityWeapon(false),
m_hitWeaponAmmoAttachment(false),
m_silenced(false),
m_willKillPlayer(false),
m_SystemTimeAdded(sysTimer::GetSystemMsTime()),
m_playerNMAborted(false),
m_rejected(false),
m_playerHealth(-1),
m_playerHealthTimestamp(0),
m_locallyTriggered(false),
m_killedPlayerIndex(INVALID_PLAYER_INDEX),
m_firstBullet(false),
m_bOriginalHitObjectIsClone(false),
m_bVictimPlayer(false),
m_playerDistance(0),
m_hitDirection(VEC3_ZERO),
m_hasHitDirection(false)
{
#if ENABLE_NETWORK_LOGGING
    m_bNoParentObject    = false;
    m_bNoHitObject       = false;
    m_bHitObjectIsClone  = false;
	m_bPlayerResurrected = false;
	m_bPlayerTooFarAway	 = false;
#endif // ENABLE_NETWORK_LOGGING
}

CWeaponDamageEvent::CWeaponDamageEvent( NetworkEventType eventType ) :
netGameEvent( eventType, false ),
m_damageType(DAMAGE_OBJECT),
m_parentID(NETWORK_INVALID_OBJECT_ID),
m_hitObjectID(NETWORK_INVALID_OBJECT_ID),
m_hitPosition(Vector3(0.0f, 0.0f, 0.0f)),
m_component(-1),
m_bOverride(false),
m_weaponHash(0),
m_weaponDamage(0),
m_weaponDamageAggregationCount(0),
m_tyreIndex(-1),
m_suspensionIndex(-1),
m_damageTime(0),
m_damageFlags(0),
m_actionResultId(0),
m_meleeId(0),
m_nForcedReactionDefinitionID(0),
m_hitEntityWeapon(false),
m_hitWeaponAmmoAttachment(false),
m_silenced(false),
m_willKillPlayer(false),
m_SystemTimeAdded(sysTimer::GetSystemMsTime()),
m_playerNMAborted(false),
m_rejected(false),
m_playerHealth(-1),
m_playerHealthTimestamp(0),
m_locallyTriggered(false),
m_killedPlayerIndex(INVALID_PLAYER_INDEX),
m_firstBullet(false),
m_bOriginalHitObjectIsClone(false),
m_bVictimPlayer(false),
m_playerDistance(0),
m_hitDirection(VEC3_ZERO),
m_hasHitDirection(false)
{
#if ENABLE_NETWORK_LOGGING
    m_bNoParentObject    = false;
    m_bNoHitObject       = false;
    m_bHitObjectIsClone  = false;
	m_bPlayerResurrected = false;
	m_bPlayerTooFarAway  = false;
#endif // ENABLE_NETWORK_LOGGING
}

CWeaponDamageEvent::CWeaponDamageEvent(CEntity *pParentEntity,
                                       CEntity *pHitEntity,
                                       const Vector3& localHitPosition,
                                       const s32 hitComponent,
                                       const bool bOverride,
                                       const u32 weaponHash,
                                       const u32 weaponDamage,
                                       const s32 tyreIndex,
                                       const s32 suspensionIndex,
									   const u32 damageFlags,
									   const u32 actionResultId,
									   const u16 meleeId,
									   const u32 nForcedReactionDefinitionID,
									   const bool hitEntityWeapon,
									   const bool hitWeaponAmmoAttachment,
									   const bool silenced,
									   const bool firstBullet,
									   const Vector3* localHitDirection /* = 0 */) :
netGameEvent( WEAPON_DAMAGE_EVENT, false, false ),
m_damageType(DAMAGE_OBJECT),
m_parentID  (NetworkUtils::GetNetworkObjectFromEntity(pParentEntity)->GetObjectID()),
m_hitObjectID     (NetworkUtils::GetNetworkObjectFromEntity(pHitEntity)->GetObjectID()),
m_hitPosition(localHitPosition),
m_component       (hitComponent),
m_bOverride       (bOverride),
m_damageFlags	  (damageFlags),
m_actionResultId  (actionResultId),
m_meleeId		  (meleeId),
m_nForcedReactionDefinitionID (nForcedReactionDefinitionID),
m_weaponHash      (weaponHash),
m_weaponDamage    (weaponDamage),
m_weaponDamageAggregationCount(0),
m_tyreIndex       (static_cast<s8>(tyreIndex)),
m_suspensionIndex (static_cast<s8>(suspensionIndex)),
m_hitEntityWeapon (hitEntityWeapon),
m_hitWeaponAmmoAttachment(hitWeaponAmmoAttachment),
m_silenced        (silenced),
m_willKillPlayer  (false),
m_damageTime      (0),
m_SystemTimeAdded(sysTimer::GetSystemMsTime()),
m_playerNMAborted(false),
m_playerHealth	  (-1),
m_playerHealthTimestamp(0),
m_rejected		  (false),
m_locallyTriggered(true),
m_killedPlayerIndex(INVALID_PLAYER_INDEX),
m_firstBullet(firstBullet),
m_bOriginalHitObjectIsClone(false),
m_playerDistance(0),
m_hitDirection(VEC3_ZERO),
m_hasHitDirection(false)
{
	weaponDebugf3("CWeaponDamageEvent::CWeaponDamageEvent");

	if(localHitDirection)
	{
		m_hitDirection    = *localHitDirection;
		m_hasHitDirection = !localHitDirection->IsZero();
	}

	const CWeaponInfo* pWeaponInfo = NULL;

	if (m_weaponHash != 0)
	{
		pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
		gnetAssertf(pWeaponInfo, "A CWeaponDamageEvent has been triggered with an invalid weapon hash");
	}
	else
	{
		// we are allowed no weapon hash if the override flag is set
		gnetAssertf(bOverride, "A CWeaponDamageEvent has been triggered with a weapon hash of 0");
	}

	m_bVictimPlayer = false;

	switch(pHitEntity->GetType())
    {
    case ENTITY_TYPE_OBJECT:
        m_damageType = DAMAGE_OBJECT;
        break;
    case ENTITY_TYPE_VEHICLE:
        m_damageType = DAMAGE_VEHICLE;
        break;
    case ENTITY_TYPE_PED:
		m_bVictimPlayer = static_cast<CPed*>(pHitEntity)->IsPlayer();

		if (pParentEntity->GetIsTypePed() && static_cast<CPed*>(pParentEntity)->IsPlayer() &&
            static_cast<CPed*>(pHitEntity)->IsPlayer())
        {
            m_damageType = DAMAGE_PLAYER;

			m_damageTime = NetworkInterface::GetSyncedTimeInMilliseconds();

			if (m_damageTime == 0)
				m_damageTime = 1;

			Vector3 diff = VEC3V_TO_VECTOR3(pParentEntity->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pHitEntity->GetTransform().GetPosition());

			m_playerDistance = static_cast<u16>(diff.XYMag());

            if(pWeaponInfo && pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsProjectile())
            {
				CPed* pLocalPlayerInflictor = static_cast<CPed*>(pParentEntity);
                CPed* pHitPlayer = static_cast<CPed*>(pHitEntity);

				if (pLocalPlayerInflictor && pHitPlayer)
				{
					weaponDebugf3("[%d] ## %s created for %s ##", fwTimer::GetFrameCount(), GetEventName(), pHitPlayer->GetNetworkObject() ? pHitPlayer->GetNetworkObject()->GetLogName() : "Player ?");

					m_willKillPlayer = ComputeWillKillPlayer(*pLocalPlayerInflictor, *pHitPlayer);
				}
			}

			gnetAssertf(m_weaponDamage < (1<<SIZEOF_WEAPONDAMAGE), "Weapon damage (%d) exceeds maximum (%d) for player weapon damage event, this may screw up shot reactions", m_weaponDamage, (1<<SIZEOF_WEAPONDAMAGE)-1);
		}
		else
		{
            m_damageType = DAMAGE_PED;

			//If the inflictor is a Player we need to set the 
			// ped damage info - This will be used to detect headshots in UpdateDamageTracker()
			if (pParentEntity->GetIsTypePed() && static_cast<CPed*>(pParentEntity)->IsPlayer())
			{
				if (pWeaponInfo && pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsProjectile())
				{
					CPed* pedHit = static_cast<CPed*>(pHitEntity);
					if (!pedHit->IsDead())
					{
						bool bMeleeDamage = ((m_damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0) || ((m_damageFlags & CPedDamageCalculator::DF_VehicleMeleeHit) != 0);
						pedHit->SetWeaponDamageInfo(pParentEntity, weaponHash, fwTimer::GetTimeInMilliseconds(), bMeleeDamage);
						pedHit->SetWeaponDamageComponent(hitComponent);
					}
				}
			}
        }

        break;
    default:
        gnetAssert(0);
    }

	if (pHitEntity && pHitEntity->GetIsDynamic())
	{
		CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pHitEntity);
		if (pDynamicEntity && pDynamicEntity->IsNetworkClone())
			m_bOriginalHitObjectIsClone = true;
	}

    if(m_weaponHash != WEAPONTYPE_RAMMEDBYVEHICLE)
    {
        const u32 maxWeaponDamage = (1<<SIZEOF_WEAPONDAMAGE)-1;
        if(m_weaponDamage > maxWeaponDamage)
        {
            m_weaponDamage = maxWeaponDamage;
        }
    }
    else
    {
        const u32 maxWeaponDamage = (1<<SIZEOF_RAMMEDDAMAGE)-1;
        if(m_weaponDamage > maxWeaponDamage)
        {
            m_weaponDamage = maxWeaponDamage;
        }
    }

#if ENABLE_NETWORK_LOGGING
    m_bNoParentObject    = false;
    m_bNoHitObject       = false;
    m_bHitObjectIsClone  = false;
	m_bPlayerResurrected = false;
	m_bPlayerTooFarAway  = false;
#endif // ENABLE_NETWORK_LOGGING
}

CWeaponDamageEvent::~CWeaponDamageEvent()
{
	if (m_locallyTriggered)
	{
		if (m_killedPlayerIndex != INVALID_PLAYER_INDEX)
		{
			if (AssertVerify(m_killedPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS))
			{
				ms_aLastPlayerKillTimes[m_killedPlayerIndex].time = 0;
			}
			ms_delayedKillData.Reset();
		}
		
		if (m_bOriginalHitObjectIsClone && NetworkInterface::IsGameInProgress())
		{
			netObject* pHitObject = NULL;

			if (m_hitObjectID != NETWORK_INVALID_OBJECT_ID)
			{
				pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
			}

			if (pHitObject && !pHitObject->IsClone() && !pHitObject->IsPendingOwnerChange())
			{
				// the object is local again, so apply the damage
				netObject* pParentObject = NULL;

				if(m_parentID != NETWORK_INVALID_OBJECT_ID)
				{
					pParentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);
				}

				if (pParentObject && pParentObject->GetEntity())
				{
					weaponDebugf3("CWeaponDamageEvent::~CWeaponDamageEvent -- m_bOriginalHitObjectIsClone && !pHitObject->IsClone() && !pHitObject->IsPendingOwnerChange() --> invoke ProcessDamage");
					ProcessDamage(pParentObject, pHitObject); 
				}
			}
		}
	}
}

void CWeaponDamageEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CWeaponDamageEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CWeaponDamageEvent::Trigger(CEntity* pParentEntity, CEntity* pHitEntity, const Vector3& worldHitPosition, const s32 hitComponent, const bool bOverride, const u32 weaponHash, const float weaponDamage, const s32 tyreIndex, const s32 suspensionIndex, const u32 damageFlags, const u32 actionResultId, const u16 meleeId, const u32 nForcedReactionDefinitionID, const bool hitEntityWeapon /* = false */, const bool hitWeaponAmmoAttachment /* = false */, const bool silenced /* = false */, const bool firstBullet /* = false */,  const Vector3* hitDirection /* = 0 */)
{
	weaponDebugf3("CWeaponDamageEvent::Trigger pParentEntity[%p][%s] pHitEntity[%p][%s] worldHitPosition[%f %f %f] hitComponent[%d] bOverride[%d] weaponHash[%u] weaponDamage[%f] tyreIndex[%d] suspensionIndex[%d] damageFlags[0x%x] actionResultId[%u] meleeId[%u] nForcedReactionDefinitionID[%u] hitEntityWeapon[%d] hitWeaponAmmoAttachment[%d] silenced[%d] firstBullet[%d]",
		pParentEntity,pParentEntity ? pParentEntity->GetModelName() : "",
		pHitEntity,pHitEntity ? pHitEntity->GetModelName() : "",
		worldHitPosition.x,worldHitPosition.y,worldHitPosition.z,
		hitComponent,bOverride,weaponHash,weaponDamage,tyreIndex,suspensionIndex,
		damageFlags,actionResultId,meleeId,nForcedReactionDefinitionID,hitEntityWeapon,hitWeaponAmmoAttachment,silenced,firstBullet);

    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    gnetAssert(pParentEntity && NetworkUtils::GetNetworkObjectFromEntity(pParentEntity));
    gnetAssert(pHitEntity && NetworkUtils::GetNetworkObjectFromEntity(pHitEntity));

	if (!pParentEntity || !pHitEntity)
		return;

	// don't send damage events when we are in an MP cutscene
	if (NetworkInterface::IsInMPCutscene())
		return;

    // don't send damage events involving non-networked entities
    if(!NetworkUtils::GetNetworkObjectFromEntity(pParentEntity) || !NetworkUtils::GetNetworkObjectFromEntity(pHitEntity))
    {
        return;
    }

    Vector3 hitPos = Vector3(0.0f, 0.0f, 0.0f);

	int finalHitComponent = hitComponent;

	// if we're not going to be a ped weapon hit event...
	if(!hitEntityWeapon)
	{
		if (pHitEntity->GetIsTypePed())
		{
			gnetAssert(hitComponent >= 0);

			// don't bother if a ped being hit is dead
			if (static_cast<CPed*>(pHitEntity)->IsDead())
				return;

			// remap any hit component to the high LOD level and it will get remapped back on the other side....
			CPed const* ped = (CPed*)pHitEntity;

			// Don't do this if we've really hit a ped's attached weapon and are sending over the ped as a dummy...
			if(ped && ped->GetRagdollInst() && ped->GetRagdollInst()->GetCached())
			{
				// Maintaining the original behavior of using the passed in component if it can not be mapped
				// Would bailing out made more sense though?
				int ragdollComponent = fragInstNM::MapRagdollLODComponentCurrentToHigh(hitComponent, ped->GetRagdollInst()->GetCurrentPhysicsLOD());
				if (gnetVerifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent))
				{
					finalHitComponent = ragdollComponent;
				}
			} 
		}

		// calculate the position local to the hit object
		Vector3 diff = worldHitPosition - VEC3V_TO_VECTOR3(pHitEntity->GetTransform().GetPosition());

		hitPos = VEC3V_TO_VECTOR3(pHitEntity->GetTransform().UnTransform3x3(RCC_VEC3V(diff)));
		weaponDebugf3("worldHitPosition[%f %f %f] diff[%f %f %f] hitPos[%f %f %f]",worldHitPosition.x,worldHitPosition.y,worldHitPosition.z,diff.x,diff.y,diff.z,hitPos.x,hitPos.y,hitPos.z);
	}
	else
	{
		// Get the local space position of where we hit the gun...
		if (!GetHitEntityWeaponLocalSpacePos(pHitEntity, worldHitPosition, hitPos, hitWeaponAmmoAttachment))
		{
			return;
		}
	}

    if (NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
    {
#if __BANK
		if (pParentEntity 
			&& pParentEntity->GetIsTypePed()
			&& static_cast<CPed*>(pParentEntity)->IsPlayer()
			&& pHitEntity 
			&& pHitEntity->GetIsTypeVehicle()
			&& NetworkDebug::DebugWeaponDamageEvent())
		{
			netObject* damager = NetworkUtils::GetNetworkObjectFromEntity(pParentEntity);
			netObject* victim = NetworkUtils::GetNetworkObjectFromEntity(pHitEntity);
			if (damager && victim)
			{
				gnetWarning("[DebugWeaponDamageEvent] Trigger Weapon Damage Event, player=%s, vehicle=%s", damager->GetLogName(), victim->GetLogName());
				sysStack::PrintStackTrace();
			}
		}
#endif // __BANK

		u32 damage = (u32) weaponDamage;
		if (weaponDamage > 0.0f && weaponDamage < 1.0f)
		{
			damage = 1;
		}

		weaponDebugf3("CWeaponDamageEvent::Trigger hitPos[%f %f %f] invoke CWeaponDamageEvent",hitPos.x,hitPos.y,hitPos.z);

		CWeaponDamageEvent* pEvent = rage_new CWeaponDamageEvent(
                                                            pParentEntity,
                                                            pHitEntity,
                                                            hitPos,
                                                            finalHitComponent,
                                                            bOverride,
                                                            weaponHash,
                                                            damage,
                                                            tyreIndex,
                                                            suspensionIndex,
															damageFlags,
															actionResultId,
															meleeId,
															nForcedReactionDefinitionID,
															hitEntityWeapon,
															hitWeaponAmmoAttachment,
															silenced,
															firstBullet,
															hitDirection);

        NetworkInterface::GetEventManager().PrepareEvent(pEvent);

    }
    else
    {
        gnetAssertf(0, "Warning - network event pool full, dumping weapon damage vehicle event");
    }
}

bool CWeaponDamageEvent::IsInScope( const netPlayer& player ) const
{
    netObject *parentObject = 0;
    netObject *hitObject    = 0;

    if(m_parentID != NETWORK_INVALID_OBJECT_ID)
    {
        parentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);
    }

    if(m_hitObjectID != NETWORK_INVALID_OBJECT_ID)
    {
        hitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
    }

    // can't send event if the weapon's parent object does not exist on this player
    if (!parentObject || !parentObject->HasBeenCloned(player))
        return false;

    // sent to player that owns the object, or the player the object is migrating to 
	if (hitObject)
	{
		bool bMeleeDamage = ((m_damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);
		if (bMeleeDamage)
		{
			CEntity* pParentEntity = parentObject->GetEntity();
			if (pParentEntity)
			{
				Vector3 vCameraPosition = NetworkInterface::GetPlayerFocusPosition(player);
				Vector3 vParentPosition = VEC3V_TO_VECTOR3(pParentEntity->GetTransform().GetPosition());
				float fDistanceMag2 = (vParentPosition - vCameraPosition).Mag2();
				if (fDistanceMag2 < MELEE_RANGE_SQ)
					return (!player.IsMyPlayer());	// Don't send if this is the local player			
			}
		}

		if (hitObject->IsClone())
		{
			return (&player == hitObject->GetPlayerOwner());
		}
		else if (hitObject->GetPendingPlayerOwner())
		{
			return (&player == hitObject->GetPendingPlayerOwner());
		}
    }

    return false;
}

bool CWeaponDamageEvent::HasTimedOut() const
{
	const unsigned WEAPON_DAMAGE_EVENT_TIMEOUT = 1000;

	u32 currentTime = sysTimer::GetSystemMsTime();

	// basic wrapping handling (this doesn't have to be precise, and the system time should never wrap)
	if(currentTime < m_SystemTimeAdded)
	{
		m_SystemTimeAdded = currentTime;
	}

	u32 timeElapsed = currentTime - m_SystemTimeAdded;

	bool bTimedOut = timeElapsed > WEAPON_DAMAGE_EVENT_TIMEOUT;

	if (bTimedOut && m_damageType == DAMAGE_PLAYER)
	{
		// we must always wait for a reply when killing a player
		if (m_willKillPlayer)
		{
			bTimedOut = false;
		}
		else
		{
			weaponDebugf3("[%d] ## %s_%d timed out ##", fwTimer::GetFrameCount(), GetEventName(), GetEventId().GetID());

			netObject* pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
			CPed *hitPlayerPed = NetworkUtils::GetPlayerPedFromNetworkObject(pHitObject);
	
			if (hitPlayerPed && AssertVerify(hitPlayerPed->IsAPlayerPed()))
			{
				CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, hitPlayerPed->GetNetworkObject());

				if (pPlayerObj)
				{
					pPlayerObj->DeductPendingDamage(m_weaponDamage);

					weaponDebugf3("Deduct %d pending damage, now = %d", m_weaponDamage, pPlayerObj->GetPendingDamage());
				}
			}
		}
	}

	return bTimedOut;

}

template <class Serialiser> void CWeaponDamageEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_UNSIGNED(serialiser, m_damageType, SIZEOF_DAMAGETYPE, "Damage Type");
    SERIALISE_UNSIGNED(serialiser, m_weaponHash, SIZEOF_WEAPONTYPE, "Weapon Type");

    // write whether the damage differs from the default damage for this weapon type
    SERIALISE_BOOL(serialiser, m_bOverride, "Override Default Damage");
	
	// write whether we really hit the weapon the entity (ped) is holding
	SERIALISE_BOOL(serialiser, m_hitEntityWeapon, "Hit Entity Weapon");
	SERIALISE_BOOL(serialiser, m_hitWeaponAmmoAttachment, "Hit Weapon Ammo Attachment");
	SERIALISE_BOOL(serialiser, m_silenced, "Silenced");

	SERIALISE_UNSIGNED(serialiser, m_damageFlags, SIZEOF_DAMAGEFLAGS, "Damage flags");

	// this is the proper mechanism to determine if we have melee damage or not
	bool bMeleeDamage = ((m_damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);

	SERIALISE_BOOL(serialiser, bMeleeDamage, "Has Melee Damage");
	if(bMeleeDamage)
	{
		SERIALISE_UNSIGNED(serialiser, m_actionResultId, SIZEOF_ACTIONRESULTID, "Action Result ID");
		SERIALISE_UNSIGNED(serialiser, m_meleeId, SIZEOF_MELEEID, "Melee ID");
		SERIALISE_UNSIGNED(serialiser, m_nForcedReactionDefinitionID, SIZEOF_MELEEREACTIONID, "melee Reaction ID");
	}

    if (m_bOverride)
    {
        // DAN TEMP - need to hook up rammed weapon damage later
        //// write the weapon damage
        //if(m_weaponHash != WEAPONTYPE_RAMMEDBYVEHICLE)
        {
            SERIALISE_UNSIGNED(serialiser, m_weaponDamage, SIZEOF_WEAPONDAMAGE, "Weapon Damage");
        }
        //else
        //{
        //    SERIALISE_UNSIGNED(serialiser, m_weaponDamage, SIZEOF_RAMMEDDAMAGE, "Rammed by car Damage");
        //}
    }

	bool bIsAggregated = m_weaponDamageAggregationCount > 0;
	SERIALISE_BOOL(serialiser, bIsAggregated, "Is Aggregated");
	if (bIsAggregated)
	{
		SERIALISE_UNSIGNED(serialiser, m_weaponDamageAggregationCount, SIZEOF_WEAPONDAMAGE_AGGREGATION, "Weapon Damage Aggregation Count");
	}
	else
	{
		m_weaponDamageAggregationCount = 0;
	}

	SERIALISE_BOOL(serialiser, m_bVictimPlayer, "m_bVictimPlayer");
	if (m_bVictimPlayer)
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.x, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position X");
		SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.y, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position Y");
		SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.z, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position Z");
	}

    if (m_damageType == DAMAGE_PLAYER)
    {
        SERIALISE_UNSIGNED(serialiser, m_damageTime, 32, "Damage Time");
		SERIALISE_BOOL(serialiser, m_willKillPlayer, "Will Kill");

		//Always serialize the hit it for melee damage as we send it to other players
		if (bMeleeDamage)
		{
			SERIALISE_OBJECTID(serialiser, m_hitObjectID,    "Hit Global ID");
		}

		bool useLargeDistance = m_playerDistance >= (1<<SIZEOF_PLAYER_DISTANCE);
		SERIALISE_BOOL(serialiser, useLargeDistance, "Use Large Distance");
		if(useLargeDistance)
		{
			SERIALISE_UNSIGNED(serialiser, m_playerDistance, SIZEOF_PLAYER_DISTANCE_LARGE, "Player distance Large");
		}
		else
		{
			SERIALISE_UNSIGNED(serialiser, m_playerDistance, SIZEOF_PLAYER_DISTANCE, "Player distance");
		}
    }
    else
    {
        SERIALISE_OBJECTID(serialiser, m_parentID, "Parent Global ID");
        SERIALISE_OBJECTID(serialiser, m_hitObjectID,    "Hit Global ID");
    }

    if (m_damageType == DAMAGE_OBJECT || m_damageType == DAMAGE_VEHICLE)
    {
        SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.x, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position X");
        SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.y, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position Y");
        SERIALISE_PACKED_FLOAT(serialiser, m_hitPosition.z, LOCAL_HIT_POSITION_MAG, SIZEOF_POSITION, "Local Hit Position Z");

        if (m_damageType == DAMAGE_VEHICLE)
        {
            bool hasVehicleData = (m_tyreIndex != -1 || m_suspensionIndex != -1);

            SERIALISE_BOOL(serialiser, hasVehicleData, "Has Vehicle Data");

            if(hasVehicleData)
            {
                m_tyreIndex = 1 + m_tyreIndex;
                m_suspensionIndex = 1 + m_suspensionIndex;

                SERIALISE_UNSIGNED(serialiser, m_tyreIndex, SIZEOF_TYRE_INDEX, "Tyre Index");
                SERIALISE_UNSIGNED(serialiser, m_suspensionIndex, SIZEOF_SUSPENSION_INDEX, "Suspension Index");

                m_tyreIndex = m_tyreIndex - 1;
                m_suspensionIndex = m_suspensionIndex - 1;
            }
        }
    }
    else
    {
        SERIALISE_UNSIGNED(serialiser, m_component, SIZEOF_COMPONENT, "Hit Component");
    }

	SERIALISE_BOOL(serialiser, m_firstBullet, "First Bullet");

	SERIALISE_BOOL(serialiser, m_hasHitDirection, "Has m_hitDirection");
	if(m_hasHitDirection)
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_hitDirection.x, TWO_PI, SIZEOF_POSITION, "Hit Direction X");
		SERIALISE_PACKED_FLOAT(serialiser, m_hitDirection.y, TWO_PI, SIZEOF_POSITION, "Hit Direction Y");
		SERIALISE_PACKED_FLOAT(serialiser, m_hitDirection.z, TWO_PI, SIZEOF_POSITION, "Hit Direction Z");
	}
}

template <class Serialiser> void CWeaponDamageEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

	if (m_damageType == DAMAGE_PLAYER)
	{
		u32 playerHealth = m_playerHealth >= 0 ? (u32) m_playerHealth : 200;

		SERIALISE_UNSIGNED(serialiser, playerHealth, SIZEOF_HEALTH, "Player Health");
		SERIALISE_UNSIGNED(serialiser, m_playerHealthTimestamp, SIZEOF_HEALTH_TIMESTAMP, "Player Health Timestamp");
		SERIALISE_BOOL(serialiser, m_rejected, "Rejected");

		m_playerHealth = (s16) playerHealth;
	}
}

void CWeaponDamageEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CWeaponDamageEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CWeaponDamageEvent::Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	weaponDebugf3("CWeaponDamageEvent::Decide -- [%d] ### %s received from player %s ### ", fwTimer::GetFrameCount(), GetEventName(), fromPlayer.GetLogName());

    // do the basic rejection checks - decides whether we should apply damage
    netObject *parentObject = 0;
    netObject *hitObject    = 0;

	bool bReply = true;

	m_rejected = false;

	if (m_damageType == DAMAGE_PLAYER)
	{
	  m_RequiresReply = true;
	}

    if(DoBasicRejectionChecks(static_cast<const CNetGamePlayer &>(fromPlayer).GetPlayerPed(),
                              static_cast<const CNetGamePlayer &>(toPlayer).GetPlayerPed(),
                              parentObject, hitObject))
    {
		bool bMeleeDamage = ((m_damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);
		if (bMeleeDamage)
		{
			//special processing for melee damage when the hitobject is a clone - invoke ProcessDamage than return - skipping other processing
			if (hitObject && hitObject->IsClone())
			{
				m_RequiresReply = false;

				if (m_damageType == DAMAGE_PLAYER)
					m_damageType = DAMAGE_PED;
	
				ProcessDamage(parentObject, hitObject, &bReply);

				return true;
			}
		}
		else
		{
			// ignore the event if the entity is not local, the event will get sent to the new owner
			if (hitObject && (hitObject->IsClone() || hitObject->IsPendingOwnerChange()))
			{
				bReply = false;
			}
		}

		m_rejected = true;
	}
	/*
	//-----
	//received a fire counter, ensure that the weapon the firer fired has fired already, if not actuate it now.
	//this might still be delayed because firing waits until the render - possibly if not the right # - return false here
	//this will then invoke a delay and then the receive fire message will come in next and this should work appropriately.
	//might need to send seed in order to get shotgun blasts to work properly under this funky condition
	if (m_firstBullet)
	{
		weaponDebugf3("CWeaponDamageEvent::Decide--m_firstBullet m_damageTime[%u] getsyncedtimeinmilliseconds[%u] deltatime[%u]",m_damageTime,NetworkInterface::GetSyncedTimeInMilliseconds(),NetworkInterface::GetSyncedTimeInMilliseconds()-m_damageTime);

		CEntity *parentEntity = parentObject ? parentObject->GetEntity() : 0;
		if (parentEntity && parentEntity->GetIsTypePed() && parentEntity->GetIsOnScreen())
		{
			CPed* firingPed = static_cast<CPed*>(parentEntity);
			if (firingPed && firingPed->IsPlayer())
			{
				weaponDebugf3("CWeaponDamageEvent::Decide--IsPlayer && IsOnScreen");

				//Only allow this short-circuit if the delta between current time and damaged time is less than a threshold - otherwise the damage might be neglected causing other syncing issues.
				static const u32 deltaTimeThreshold = 500; 
				if ((NetworkInterface::GetSyncedTimeInMilliseconds() - m_damageTime) < deltaTimeThreshold)
				{
					CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(firingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(firingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
					}

					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(firingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
					}

					if(pAimGunTask)
					{
						u8 getFireCounter = pAimGunTask->GetFireCounter();
						weaponDebugf3("CWeaponDamageEvent::Decide--pAimGunTask--getFireCounter[%u]",getFireCounter);
						if (getFireCounter == 0)
						{
							// catch the case where the ped has fired but the fire state was missed by the network update, and return false so the decide will be processed again... if the SetForcedFire is processed by an a processing virtual...
							weaponDebugf3("CWeaponDamageEvent::Decide--m_firstBullet && (getFireCounter==0)-->return false");
							return false;
						}
					}
					else
					{
						weaponDebugf3("CWeaponDamageEvent::Decide--!pAimGunTask-->return false");
						return false;
					}
				}
			}
		}
	}
	//-----*/

	if (!m_rejected && !ProcessDamage(parentObject, hitObject, &bReply))
	{
		m_rejected = true;
	}

	if (m_damageType == DAMAGE_PLAYER)
	{
		const CPed *pToPlayerPed = static_cast<const CNetGamePlayer &>(toPlayer).GetPlayerPed();
		if(pToPlayerPed)
		{
			bool bInvincible = pToPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything;

			if (bInvincible)
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Player is invincible"));
			}
#if !__FINAL
			if (pToPlayerPed->IsDebugInvincible())
				bInvincible = true;
#endif
						
			CNetObjPlayer* netObjPlayer = pToPlayerPed->IsPlayer() ? static_cast<CNetObjPlayer *>(pToPlayerPed->GetNetworkObject()) : NULL;

			if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer()> 0)
			{
				bInvincible = true;
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Player has invincibility timer set"));
			}

			if (bInvincible)
			{
				m_rejected = true;
			}
		}
	}

	if (m_damageType == DAMAGE_PED && parentObject && parentObject->GetObjectType() == NET_OBJ_TYPE_PLAYER && hitObject)
	{
		HandlePlayerShootingPed(fromPlayer, hitObject, m_rejected);
	}

	if (m_damageType == DAMAGE_PLAYER && hitObject && AssertVerify(hitObject->GetObjectType() == NET_OBJ_TYPE_PLAYER))
	{
		CPed *hitPed = static_cast<CNetObjPlayer*>(hitObject)->GetPed();

		// cache the players health after the damage has been applied for replying to the event
		if (AssertVerify(hitPed->IsLocalPlayer()))
		{
			float playerHealth = hitPed->GetHealth();

			if (playerHealth <= 0.0f)
			{
				m_playerHealth = 0;
			}
			else
			{
				m_playerHealth = static_cast<s16>(playerHealth);
			}

			m_playerHealthTimestamp = netInterface::GetNetworkTime();
		}
	}

	if (hitObject && (hitObject->GetObjectType() == NET_OBJ_TYPE_PLAYER))
	{
		//ensure that if the local player is hit that the firer is blipped on the radar
		//set the CPED_RESET_FLAG_FiringWeapon
		//this is used in CScriptPedAIBlips::AIBlip::IsPedNoticableToPlayer to determine if the ped should be blipped 
		//setting this will force the blip
		CPed *hitPed = static_cast<CNetObjPlayer*>(hitObject)->GetPed();
		if (hitPed && hitPed->IsLocalPlayer())
		{
			CPed* parentPed = NULL;

			if (m_damageType == DAMAGE_PLAYER)
			{
				parentPed = static_cast<const CNetGamePlayer &>(fromPlayer).GetPlayerPed();
			}
			else
			{
				netObject* pParentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);
				parentPed = NetworkUtils::GetPedFromNetworkObject(pParentObject);
			}

			if (parentPed)
			{
				parentPed->SetPedResetFlag( CPED_RESET_FLAG_FiringWeapon, true );
			}
		}
	}

	return bReply;
}

bool CWeaponDamageEvent::ProcessDamage(netObject *parentObject, netObject *hitObject, bool* bReply)
{
	weaponDebugf3("CWeaponDamageEvent::ProcessDamage parentObject[%p] hitObject[%p]",parentObject,hitObject);

    CEntity *parentEntity = parentObject ? parentObject->GetEntity() : 0;
    CPed    *hitPed       = NetworkUtils::GetPedFromNetworkObject(hitObject);

	// handle damage dealt being run over
    if((m_weaponHash == WEAPONTYPE_RAMMEDBYVEHICLE || m_weaponHash == WEAPONTYPE_EXPLOSION) && hitPed && parentEntity)
    {
		weaponDebugf3("((m_weaponHash == WEAPONTYPE_RAMMEDBYVEHICLE || m_weaponHash == WEAPONTYPE_EXPLOSION) && hitPed && parentEntity)");

        CEventDamage tempDamageEvent(parentEntity, fwTimer::GetTimeInMilliseconds(), m_weaponHash);
        CPedDamageCalculator damageCalculator(parentEntity, static_cast<float>(m_weaponDamage), m_weaponHash, 0, false);
        damageCalculator.ApplyDamageAndComputeResponse(hitPed, tempDamageEvent.GetDamageResponseData(), m_damageFlags);

        if(tempDamageEvent.AffectsPed(hitPed))
        {
            hitPed->GetPedIntelligence()->AddEvent(tempDamageEvent);
        }

        return true;
    }

	// modify the damage based on the weapon used - if this fails we can't apply the damage (player-on-player damage includes any modifications)
    if(m_damageType != DAMAGE_PLAYER && !ModifyDamageBasedOnWeapon(parentObject, bReply))
    {
		LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Models not loaded"));
		return false;
    }

 	weaponDebugf3("call CheckForDelayedKill");
	if(CheckForDelayedKill(parentObject, hitObject))
	{
		LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Delayed kill"));
		return false;
	}

	float previousPlayerHealth = 0.0f;

	if (m_damageType == DAMAGE_PLAYER && hitPed)
	{
		weaponDebugf3("## PLAYER DAMAGED (pending damage: %d, init health = %f) ##", m_weaponDamage, hitPed->GetHealth());

		if (AssertVerify(hitObject->GetObjectType() == NET_OBJ_TYPE_PLAYER))
		{
			// store the pending damage so that it can be used later when processing the shot
			static_cast<CNetObjPlayer*>(hitObject)->SetPendingDamage(m_weaponDamage);
			m_damageFlags |= CPedDamageCalculator::DF_UsePlayerPendingDamage;
		}

		// if the shooting player thinks that this is a kill and we are nearly dead, allow the kill
		if (m_willKillPlayer)
		{
			m_damageFlags |= CPedDamageCalculator::DF_ExpectedPlayerKill;
		}

		previousPlayerHealth = hitPed->GetHealth();
	}

	// Get the entity and the world space position...
    CEntity *hitEntity = hitObject ? hitObject->GetEntity() : 0;
	Vector3 worldHitPosition(VEC3_ZERO);

	//---

	// handle shooting a ped's weapon by switching the hit object to the weapon the ped is carrying...
	if(IsHitEntityWeaponEvent())
	{
		// Is the ped we're about to apply this to in a suitable state (i.e carrying a weapon)
		if(!IsHitEntityWeaponEntitySuitable(hitEntity))
		{
			LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Ped not carrying weapon"));
			return false;
		}

		GetHitEntityWeaponWorldSpacePos(hitEntity, m_hitPosition, worldHitPosition, HitEntityWeaponAmmoAttachment());

		if(hitEntity)
		{
			CPed* weaponHolder = (CPed*)hitEntity;
			if(weaponHolder && weaponHolder->GetWeaponManager())
			{
				hitEntity = weaponHolder->GetWeaponManager()->GetEquippedWeaponObject();

				if(HitEntityWeaponAmmoAttachment())
				{
					hitEntity = (CEntity*)(hitEntity->GetChildAttachment());
				}

				if(hitEntity)
				{
					// nullify the hit object / ped as it's not the one we hit...
					hitObject	= NULL;
					hitPed		= NULL;
				}
			}
		}

		gnetAssert(!hitObject && !hitPed && hitEntity->GetIsTypeObject());
	}
	else
	{
		if (gnetVerifyf(hitEntity, "Hit object does not have an entity!"))
		{
			// shouldn't be remapping anything if we've been sending the ped through as a substitute for it's weapon....
			gnetAssert(!m_hitEntityWeapon && !m_hitWeaponAmmoAttachment);

			worldHitPosition = hitEntity->TransformIntoWorldSpace(m_hitPosition);
		}
	}
	// weapon damage and effects
	weaponDebugf3("CWeaponDamageEvent::ProcessDamage-->invoke CWeapon::ReceiveFireMessage");
    if (!CWeapon::ReceiveFireMessage(parentEntity,
									hitEntity,
									m_weaponHash,
									static_cast<float>(m_weaponDamage),
									worldHitPosition,
									m_component,
									m_tyreIndex,
									m_suspensionIndex,
									m_damageFlags,
									m_actionResultId, 
									m_meleeId,
									m_nForcedReactionDefinitionID,
									NULL,
									m_silenced,
									m_hasHitDirection ? &m_hitDirection : 0,
									m_weaponDamageAggregationCount))
	{
		LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "ReceiveFireMessage failed"));
		return false;
	}

	if (hitPed)
	{
		weaponDebugf3("## PLAYER HEALTH NOW = %f ##", hitPed->GetHealth());

		// local player has died, force an immediate health node update to avoid double kills and aborted shot reactions
		if (m_damageType == DAMAGE_PLAYER && previousPlayerHealth > 0.0f && hitPed->GetHealth() == 0.0f && hitPed->IsLocalPlayer())
		{
			netObject* pPlayerObj = hitPed->GetNetworkObject();
		
			if ( pPlayerObj && pPlayerObj->GetSyncData() && pPlayerObj->GetSyncTree())
			{
				weaponDebugf3("## FORCE HEALTH UPDATE at %f ##", hitPed->GetHealth());

				pPlayerObj->GetSyncTree()->Update(pPlayerObj, pPlayerObj->GetActivationFlags(), netInterface::GetSynchronisationTime());
				pPlayerObj->GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, pPlayerObj->GetActivationFlags(), pPlayerObj, *SafeCast(CPedSyncTreeBase, pPlayerObj->GetSyncTree())->GetPedHealthNode());
			}
		}
	}

	return true;
}


bool CWeaponDamageEvent::ComputeWillKillPlayer(const CPed& inflictor, CPed& victim)
{
	weaponDebugf3("## COMPUTE WILL KILL PLAYER ##");

	// Calculate the distance to the ped we have hit
	float fDist = FLT_MAX;

	Vector3 vStart(VEC3V_TO_VECTOR3(inflictor.GetTransform().GetPosition()));
	
	const CObject* pWeaponObject = inflictor.GetWeaponManager()->GetEquippedWeaponObject();
	
	if (pWeaponObject)
	{
		const CWeapon* pWeapon = inflictor.GetWeaponManager()->GetEquippedWeapon();
		const Matrix34& weaponMatrix = RCC_MATRIX34(pWeaponObject->GetMatrixRef());
		Vector3 vMuzzlePosition(Vector3::ZeroType);
		pWeapon->GetMuzzlePosition(weaponMatrix, vStart);
	}

	//Use the victim position instead of the m_hitPosition as that is local space coord and needs conversion - just keep this simple and use the ped position for the check
	Vector3 vHitPedPosition = VEC3V_TO_VECTOR3(victim.GetTransform().GetPosition());
	fDist = vStart.Dist(vHitPedPosition);
	weaponDebugf3("vStart[%f %f %f] pHitPed[%f %f %f] fDist[%f]",vStart.x,vStart.y,vStart.z,vHitPedPosition.x,vHitPedPosition.y,vHitPedPosition.z,fDist);
	
	weaponDebugf3("Victim health[%f]. Initial Damage[%d]", victim.GetHealth(), m_weaponDamage);

	CNetObjPlayer* pVictimObj = SafeCast(CNetObjPlayer, victim.GetNetworkObject());

	CEventDamage tempDamageEvent((CEntity*)&inflictor, fwTimer::GetTimeInMilliseconds(), m_weaponHash);
	CPedDamageCalculator damageCalculator(&inflictor, (float)m_weaponDamage, m_weaponHash, m_component, false);
	tempDamageEvent.GetDamageResponseData().m_bClonePedKillCheck = true;
	weaponDebugf3("-->invoke ApplyDamageAndComputeResponse hitComponent[%d]",m_component);
	damageCalculator.ApplyDamageAndComputeResponse(&victim, tempDamageEvent.GetDamageResponseData(), m_damageFlags, m_actionResultId, fDist);

	// adjust our damage based on the damage calculation (includes headshots, etc)
	float damage = damageCalculator.GetRawDamage();
	m_weaponDamage = (u32)damageCalculator.GetRawDamage();
	if(damage > 0.0f && damage < 1.0f)
	{
		m_weaponDamage = 1;
	}
	m_bOverride = true;

	weaponDebugf3("New modified damage[%d]. Head shot[%d]", m_weaponDamage, tempDamageEvent.GetDamageResponseData().m_bHeadShot);

	bool bKilled = tempDamageEvent.GetDamageResponseData().m_bKilled;

	if (pVictimObj && pVictimObj->IsClone())
	{
		weaponDebugf3("Victim pending damage[%d]", pVictimObj->GetPendingDamage());

		if ((float)pVictimObj->GetLastReceivedHealth() - damageCalculator.GetRawDamage() - (float)pVictimObj->GetPendingDamage() < victim.GetDyingHealthThreshold())
		{
			weaponDebugf3("Damage + pending damage will kill player");
			bKilled = true;
		}
		else if (bKilled)
		{
			weaponDebugf3("Will kill player");
		}

		pVictimObj->AddToPendingDamage(m_weaponDamage, bKilled);
	}

	if (inflictor.IsLocalPlayer())
	{
		m_killedPlayerIndex = victim.GetNetworkObject()->GetPlayerOwner() ? victim.GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;

		if (m_killedPlayerIndex != INVALID_PLAYER_INDEX)
		{
			if (bKilled)
			{
				if (ms_aLastPlayerKillTimes[m_killedPlayerIndex].time == 0)
				{
					weaponDebugf3("possible kill of remote player -- set ms_aLastPlayerDamageTimes[%d].time[%u]",m_killedPlayerIndex,m_damageTime);
					ms_aLastPlayerKillTimes[m_killedPlayerIndex].time = m_damageTime;
				}
				else
				{
					weaponDebugf3("possible kill of remote player -- leave previous ms_aLastPlayerDamageTimes[%d].time[%u]",m_killedPlayerIndex,ms_aLastPlayerKillTimes[m_killedPlayerIndex].time);
					m_killedPlayerIndex = INVALID_PLAYER_INDEX;
				}
			}
			else
			{
				weaponDebugf3("local player or remote player not likely killed set ms_aLastPlayerDamageTimes[%d].time[0]",m_killedPlayerIndex);
				ms_aLastPlayerKillTimes[m_killedPlayerIndex].time = 0;
			}
		}
	}

	return bKilled;
}

bool CWeaponDamageEvent::IsHitEntityWeaponEntitySuitable(CEntity const* pedEntity)
{
	if(!pedEntity)
	{
		return false;
	}

	if(pedEntity && !pedEntity->GetIsTypePed())
	{
		return false;
	}

	CPed* ped = (CPed*)pedEntity;
	if(!ped->GetWeaponManager() && !ped->GetWeaponManager()->GetEquippedWeapon())
	{
		return false;
	}

	CEntity const* weaponEntity = ped->GetWeaponManager()->GetEquippedWeaponObject();
	if(!weaponEntity)
	{
		return false;
	}

	if(HitEntityWeaponAmmoAttachment())
	{
		if(!weaponEntity->GetChildAttachment())
		{
			return false;
		}
	}

	return true;	
}

bool CWeaponDamageEvent::IsHitEntityWeaponEvent()
{
	return m_hitEntityWeapon;
}

bool CWeaponDamageEvent::HitEntityWeaponAmmoAttachment()
{
	return m_hitWeaponAmmoAttachment;
}

/*[static]*/ bool CWeaponDamageEvent::GetHitEntityWeaponLocalSpacePos(CEntity const* pedEntity, Vector3 const& hitPosWS, Vector3& posLS, bool const hitWeaponAmmoAttachment)
{
	bool bSuccess = false;

	// Check the entity is a ped and is carrying a weapon...
	if (gnetVerify(pedEntity && pedEntity->GetIsTypePed()))
	{
		CPed* pPed = (CPed*)pedEntity;

		if (gnetVerify(pPed->GetWeaponManager()))
		{
			CEntity const* weaponEntity = pPed->GetWeaponManager()->GetEquippedWeaponObject();

			if (gnetVerifyf(weaponEntity, "CWeaponDamageEvent::GetHitEntityWeaponLocalSpacePos - %s has no equipped weapon object", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "??"))
			{
				if(hitWeaponAmmoAttachment)
				{
					gnetAssert(weaponEntity->GetChildAttachment());
					weaponEntity = (CEntity*)(weaponEntity->GetChildAttachment());
				}

				if (gnetVerifyf(weaponEntity, "CWeaponDamageEvent::GetHitEntityWeaponLocalSpacePos - %s has no equipped weapon child attachment", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "??"))
				{
					Vector3 diff = hitPosWS - VEC3V_TO_VECTOR3(weaponEntity->GetTransform().GetPosition());

					posLS = VEC3V_TO_VECTOR3(weaponEntity->GetTransform().UnTransform3x3(RCC_VEC3V(diff)));	

					bSuccess = true;
				}
			}
		}
	}

	return bSuccess;
}

/*[static]*/ void CWeaponDamageEvent::GetHitEntityWeaponWorldSpacePos(CEntity const* pedEntity, Vector3 const& hitPosLS, Vector3& posWS, bool const hitWeaponAmmoAttachment)
{
	// Check the entity is a ped and is carrying a weapon...
	gnetAssert(pedEntity && pedEntity->GetIsTypePed());
	gnetAssert(((CPed*)pedEntity)->GetWeaponManager() && ((CPed*)pedEntity)->GetWeaponManager()->GetEquippedWeapon());

	CEntity const* weaponEntity = ((CPed*)pedEntity)->GetWeaponManager()->GetEquippedWeaponObject();
	gnetAssert(weaponEntity);

	if(hitWeaponAmmoAttachment)
	{
		gnetAssert(weaponEntity->GetChildAttachment());
		weaponEntity = (CEntity*)(weaponEntity->GetChildAttachment());
	}

	posWS = weaponEntity->TransformIntoWorldSpace(hitPosLS);
}

void CWeaponDamageEvent::PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	if (m_damageType == DAMAGE_PLAYER && !m_rejected)
	{
		gnetAssertf(m_playerHealth != -1, "CWeaponDamageEvent - m_playerHealth is not set");
		gnetAssertf(m_playerHealthTimestamp != 0, "CWeaponDamageEvent - m_playerHealthTimestamp is not set");
	}

    SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CWeaponDamageEvent::HandleReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	weaponDebugf3("[%d] ## %s_%d reply received ##", fwTimer::GetFrameCount(), GetEventName(), GetEventId().GetID());

    SerialiseEventReply<CSyncDataReader>(messageBuffer);

	if (m_damageType == DAMAGE_PLAYER)
	{
		netObject* pParentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);
		netObject* pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
		CPed *hitPlayerPed = NetworkUtils::GetPlayerPedFromNetworkObject(pHitObject);
		CPed *parentPlayerPed = NetworkUtils::GetPlayerPedFromNetworkObject(pParentObject);

		if (hitPlayerPed && parentPlayerPed)
		{
			weaponDebugf3("(hitPlayerPed && parentPlayerPed)");

			//If this player shot us recently and we haven't killed him, then apply his shot now. This is to stop double kills.
			PhysicalPlayerIndex playerIndex = INVALID_PLAYER_INDEX;

			if (AssertVerify(hitPlayerPed->GetNetworkObject()) && hitPlayerPed->GetNetworkObject()->GetPlayerOwner())
				playerIndex = hitPlayerPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();

			weaponDebugf3("playerIndex[%d]",playerIndex);
			weaponDebugf3("m_playerHealth[%d]",m_playerHealth);
			weaponDebugf3("m_playerHealthTimestamp[%d]",m_playerHealthTimestamp);
			weaponDebugf3("m_rejected[%d]",m_rejected);

			if (playerIndex != INVALID_PLAYER_INDEX)
			{
				weaponDebugf3("IsDelayedKillPending[%d]--m_playerHealth[%u]",IsDelayedKillPending(playerIndex),m_playerHealth);

				if (IsDelayedKillPending(playerIndex))
				{
					weaponDebugf3("IsDelayedKillPending m_playerHealth[%d]",m_playerHealth);
					if (m_playerHealth > 0)
					{
						ApplyDelayedKill();
					}
					else
					{
						ClearDelayedKill();
					}
				}

				if (m_killedPlayerIndex != INVALID_PLAYER_INDEX &&
					ms_aLastPlayerKillTimes[playerIndex].time != 0 &&
					ms_aLastPlayerKillTimes[playerIndex].time <= m_damageTime)
				{
					gnetAssert(m_killedPlayerIndex == playerIndex);
					weaponDebugf3("ms_aLastPlayerDamageTimes[%d].time = 0",playerIndex);
					ms_aLastPlayerKillTimes[playerIndex].time = 0;
					m_killedPlayerIndex = INVALID_PLAYER_INDEX;
				}
			}

			CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, hitPlayerPed->GetNetworkObject());

			if (m_playerHealthTimestamp != 0 && m_playerHealth >= 0)
			{
				pPlayerObj->SetLastReceivedHealth((u32)m_playerHealth, m_playerHealthTimestamp);
			}

			const CWeaponInfo *pWeaponInfo = NULL;

			if (m_weaponHash != 0)
			{
				pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
				gnetAssertf(pWeaponInfo, "Received a CWeaponDamageEvent reply with an unrecognised weapon hash");
			}

			if (m_weaponHash == WEAPONTYPE_EXPLOSION)
			{
				if (m_rejected)
				{
					CTaskNMControl* pNMControlTask = SafeCast(CTaskNMControl, hitPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));

					if (pNMControlTask && pNMControlTask->IsRunningLocally())
					{
						if (!pNMControlTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
						{
							pNMControlTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL);
						}

						hitPlayerPed->GetPedIntelligence()->RecalculateCloneTasks();
					}
				}
			}
			else if (pWeaponInfo && pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsProjectile())
			{
				float weaponDamage = m_bOverride ? m_weaponDamage : pWeaponInfo->GetDamage();

				pPlayerObj->DeductPendingDamage((u32)weaponDamage);

				weaponDebugf3("Deduct %f pending damage, now = %d", weaponDamage, pPlayerObj->GetPendingDamage());

				if (m_playerHealth > 0)
				{
					if (m_rejected)
					{
						weaponDebugf3("Abort NM shot reaction");
						// call this here to also reset any pending damage
						pPlayerObj->AbortNMShotReaction();
						m_playerNMAborted = true;
					}
					else if (m_willKillPlayer)
					{
						float pendingDamage = (float)pPlayerObj->GetPendingDamage();

						u32 playerHealth = pPlayerObj->GetLastReceivedHealth();

						if (playerHealth > 0.0f && playerHealth < hitPlayerPed->GetDyingHealthThreshold())
						{
							gnetAssertf(0, "Player health is below dying health threshold but non-0");
						}
						if (playerHealth > hitPlayerPed->GetDyingHealthThreshold() && (playerHealth - hitPlayerPed->GetDyingHealthThreshold() > pendingDamage))
						{
							weaponDebugf3("Abort NM shot reaction");
							pPlayerObj->AbortNMShotReaction();
							m_playerNMAborted = true;
						}
					}
				}
			}
		}
	}
	else if (AssertVerify(m_damageType == DAMAGE_PED))
	{
		netObject* pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
		CPed *hitPed = NetworkUtils::GetPedFromNetworkObject(pHitObject);
	
		// the weapon damage event failed so abort any local shot reactions
		if (hitPed && hitPed->IsNetworkClone() && !hitPed->GetIsDeadOrDying() && hitPed->GetUsingRagdoll())
		{
			nmEntityDebugf(hitPed, "CWeaponDamageEvent::HandleReply switching to animated");
			hitPed->SwitchToAnimated(true, true, true, false, false, true, true);
		}
	}
}

#if ENABLE_NETWORK_LOGGING

void CWeaponDamageEvent::WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

    WriteDataToLogFile(bEventLogOnly);

	if (m_damageType == DAMAGE_PLAYER)
	{
		netObject* objHit = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID, true);
		if (objHit && objHit->GetObjectType() == NET_OBJ_TYPE_PLAYER)
		{
			CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, objHit);

			if (pPlayerObj->GetPed())
			{
				if (wasSent)
				{
					log.WriteDataValue("Victim last received health", "%u", pPlayerObj->GetLastReceivedHealth());
					log.WriteDataValue("Victim LRH timestamp", "%u", pPlayerObj->GetLastReceivedHealthTimestamp());
					log.WriteDataValue("Victim pending damage", "%u", pPlayerObj->GetPendingDamage());
				}
				else
				{
					log.WriteDataValue("Victim health", "%f", pPlayerObj->GetPed()->GetHealth());
				}

				log.WriteDataValue("Victim dying threshold", "%f", pPlayerObj->GetPed()->GetDyingHealthThreshold());
			}
		}
	}
}

void CWeaponDamageEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	netObject* pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
	CPed *hitPlayerPed = NetworkUtils::GetPlayerPedFromNetworkObject(pHitObject);
	CNetObjPlayer* pPlayerObj = hitPlayerPed ? SafeCast(CNetObjPlayer, hitPlayerPed->GetNetworkObject()) : NULL;

	logSplitter.WriteDataValue("Player health", "%d", m_playerHealth);
	logSplitter.WriteDataValue("Player health timestamp", "%d", m_playerHealthTimestamp);
	logSplitter.WriteDataValue("Rejected", "%s", m_rejected ? "true" : "false");

	if (pPlayerObj)
	{
		logSplitter.WriteDataValue("Victim", "%s", pPlayerObj->GetLogName());
		logSplitter.WriteDataValue("Victim last received health", "%u", pPlayerObj->GetLastReceivedHealth());
		logSplitter.WriteDataValue("Victim LRH timestamp", "%u", pPlayerObj->GetLastReceivedHealthTimestamp());
		logSplitter.WriteDataValue("Victim pending damage", "%d", pPlayerObj->GetPendingDamage());
		logSplitter.WriteDataValue("Victim dying threshold", "%f", hitPlayerPed->GetDyingHealthThreshold());
		logSplitter.WriteDataValue("Victim NM aborted", "%s", m_playerNMAborted ? "true" : "false");
	}
}

void CWeaponDamageEvent::WriteDataToLogFile(bool LOGGING_ONLY(bEventLogOnly)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	bool bIgnored = m_bNoParentObject || m_bNoHitObject || m_bHitObjectIsClone || m_bPlayerResurrected || m_bPlayerTooFarAway;
    if(!bIgnored)
    {
        log.WriteDataValue("Event", "Accepted");
    }
    else
    {
        const char *ignoreReason = "";
        ignoreReason = m_bNoParentObject    ? "Parent object not cloned on this machine"		: ignoreReason;
        ignoreReason = m_bNoHitObject       ? "Hit object not cloned on this machine"			: ignoreReason;
        ignoreReason = m_bHitObjectIsClone  ? "Hit object not owned by this machine"			: ignoreReason;
		ignoreReason = m_bPlayerResurrected ? "Triggered before local player was resurrected"	: ignoreReason;
		ignoreReason = m_bPlayerTooFarAway  ? "Local player too far away from hit position"	    : ignoreReason;

        log.WriteDataValue("Event", "Failed (%s)", ignoreReason);
    }

    if(NETWORK_INVALID_OBJECT_ID != m_parentID)
    {
        netObject* parent = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID, true);
        if (parent)
        {
            log.WriteDataValue("Inflictor", "%s", parent->GetLogName());
        }
    }

    if(NETWORK_INVALID_OBJECT_ID != m_hitObjectID)
    {
        netObject* objHit = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID, true);
        if (objHit)
        {
            log.WriteDataValue("Victim", "%s", objHit->GetLogName());
        }
    }

    log.WriteDataValue("Weapon type", "%d", m_weaponHash);

    if (m_bOverride)
    {
        log.WriteDataValue("Damage override", "%d", m_weaponDamage);
    }

	log.WriteDataValue("Damage flags", "%d", m_damageFlags);

	
	log.WriteDataValue("Melee Action ID", "%d", m_actionResultId);

	log.WriteDataValue("Melee ID", "%d", m_meleeId);

	log.WriteDataValue("Melee Reaction ID", "%d", m_nForcedReactionDefinitionID);

    if (m_component != -1)
    {
        log.WriteDataValue("Component", "%d",m_component);
    }
    else
    {
        log.WriteDataValue("Local impact pos", "%f, %f, %f", m_hitPosition.x, m_hitPosition.y, m_hitPosition.z);
    }

    if (m_damageType == DAMAGE_PLAYER)
    {
        log.WriteDataValue("Damage time", "%d", m_damageTime);
		log.WriteDataValue("Will kill", "%s", m_willKillPlayer ? "true" : "false");
        log.WriteDataValue("Delayed kill", "%s", IsDelayedKillPending() ? "true" : "false");
		log.WriteDataValue("Local offset hit position", "%f, %f, %f", m_hitPosition.x, m_hitPosition.y, m_hitPosition.z);
		log.WriteDataValue("Player distance", "%d", m_playerDistance);
    }

	log.WriteDataValue("Hit Entity Weapon", "%d", m_hitEntityWeapon);
	log.WriteDataValue("Hit Weapon Ammo Attachment", "%d", m_hitWeaponAmmoAttachment);
	log.WriteDataValue("Silenced", "%d", m_silenced);

	log.WriteDataValue("Has Hit Direction", "%d", m_hasHitDirection);
	if(m_hasHitDirection)
	{
		log.WriteDataValue("Hit Direction X", "%f", m_hitDirection.x);
		log.WriteDataValue("Hit Direction Y", "%f", m_hitDirection.y);
		log.WriteDataValue("Hit Direction Z", "%f", m_hitDirection.z);
	}
}

#endif // ENABLE_NETWORK_LOGGING

bool CWeaponDamageEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == WEAPON_DAMAGE_EVENT)
    {
        const CWeaponDamageEvent *pWeaponDamageEvent = SafeCast(const CWeaponDamageEvent, pEvent);

        // if a similar weapon damage event exists on the queue and has not yet been sent, then just add its weapon
        // damage value to ours and remove it
        if (pWeaponDamageEvent->m_damageType == m_damageType &&
            pWeaponDamageEvent->m_parentID == m_parentID &&
            pWeaponDamageEvent->m_hitObjectID == m_hitObjectID &&
            pWeaponDamageEvent->m_weaponHash == m_weaponHash &&
			pWeaponDamageEvent->m_hitEntityWeapon == m_hitEntityWeapon && 
			pWeaponDamageEvent->m_hitWeaponAmmoAttachment == m_hitWeaponAmmoAttachment && 
			pWeaponDamageEvent->m_silenced == m_silenced)
        {
			const CWeaponInfo* pWeaponInfo = NULL;

			if (m_weaponHash != 0)
			{
				pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
				gnetAssert(pWeaponInfo);
			}

            if (HasBeenSent())
            {
                if (pWeaponInfo && pWeaponInfo->GetIsAutomatic())
                {
                    // mark the new event to be sent in 200 millisecs, this means any more weapon damage events can be appended to this one before
                    // it is sent. This means that a continuous burst of machine gun fire will sent weapon damage events 5 times a second
                    netObject* pParentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);
                    netObject* pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
                    CPed *parentPlayerPed = NetworkUtils::GetPlayerPedFromNetworkObject(pParentObject);
                    CPed *hitPlayerPed    = NetworkUtils::GetPlayerPedFromNetworkObject(pHitObject);

                    // player on player events need to be sent more frequently
                    if (parentPlayerPed || hitPlayerPed)
                        ((netGameEvent*)pEvent)->DelaySending(100);
                    else
                        ((netGameEvent*)pEvent)->DelaySending(200);
                }
            }
            else
            {
                u32 newDamage = (u32)(m_weaponDamage + pWeaponDamageEvent->m_weaponDamage);

                if (newDamage < (1<<SIZEOF_WEAPONDAMAGE) && m_weaponDamageAggregationCount < (1 << SIZEOF_WEAPONDAMAGE_AGGREGATION))
                {
					CWeaponDamageEvent* thisEvent = ((CWeaponDamageEvent*)this);

                    thisEvent->m_weaponDamage = newDamage;
                    thisEvent->m_bOverride = true;
					thisEvent->m_weaponDamageAggregationCount++;

					if (pWeaponDamageEvent->m_willKillPlayer)
						thisEvent->m_willKillPlayer = true;

					pWeaponDamageEvent->m_locallyTriggered = false;

                    return true;
                }
            }
        }
    }

    return false;
}

bool CWeaponDamageEvent::DoBasicRejectionChecks(const CPed *fromPlayerPed, const CPed *toPlayerPed, netObject *&pParentObject, netObject *&pHitObject)
{
    if (m_damageType == DAMAGE_PLAYER)
    {
        // setup the parent and hit network objects from the players involved in this event
        pParentObject = fromPlayerPed ? fromPlayerPed->GetNetworkObject() : 0;
        m_parentID    = pParentObject ? pParentObject->GetObjectID()      : NETWORK_INVALID_OBJECT_ID;

		bool bMeleeDamage = ((m_damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);
		if (bMeleeDamage)
		{
			gnetAssert(m_hitObjectID != NETWORK_INVALID_OBJECT_ID);

			pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);
		}
		else
		{
			pHitObject    = toPlayerPed   ? toPlayerPed->GetNetworkObject()   : 0;
			m_hitObjectID = pHitObject    ? pHitObject->GetObjectID()         : NETWORK_INVALID_OBJECT_ID;
		}

        if (!pHitObject || !pHitObject->GetEntity())
		{
			LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "No hit entity"));
			LOGGING_ONLY(m_bNoHitObject = true);
			weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--No hit entity-->return true");
			return true;
		}

		// ignore weapon damage events sent just before our player is resurrected
        const unsigned TIME_TO_IGNORE_DAMAGE_AFTER_RESURRECTION = 1000;

        u32 resurrectionTime = pHitObject ? static_cast<CNetObjPlayer*>(pHitObject)->GetLastResurrectionTime() : 0;

		if (AssertVerify(m_damageTime != 0) && resurrectionTime != 0 && m_damageTime < (resurrectionTime - TIME_TO_IGNORE_DAMAGE_AFTER_RESURRECTION))
		{
			LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Player resurrecting"));
			LOGGING_ONLY(m_bPlayerResurrected = true);
			weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Player resurrecting-->return true");
			return true;
		}

		// ignore shots from dead players, this avoids double kills
		if (fromPlayerPed && fromPlayerPed->GetNetworkObject())
		{
			if (SafeCast(CNetObjPlayer, fromPlayerPed->GetNetworkObject())->GetLastReceivedHealth() == 0)
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Attacking player is dead"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Attacking player is dead-->return true");
				return true;
			}
		}

		if(toPlayerPed)
        {
			if(!NetworkInterface::IsFriendlyFireAllowed(toPlayerPed, fromPlayerPed))
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Friendly fire not allowed"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Friendly fire not allowed-->return true");
				return true;
			}

			// ignore weapon damage events if the player is already being killed by Stealth - Delayed Kill
			if (toPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth))
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Player killed by stealth"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Player killed by stealth-->return true");
				return true;
			}

			// ignore weapon damage events if the player is already being killed by 1 punch kill - Delayed Kill
			if (toPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown))
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Player killed by takedown"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Player killed by takedown-->return true");
				return true;
			}
			
			if (fromPlayerPed)
			{
				// ignore weapon damage events that are too far away from our current player position
				Vector3 diff = VEC3V_TO_VECTOR3(toPlayerPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(fromPlayerPed->GetTransform().GetPosition());

				const int ignoreDist = static_cast<u16>(toPlayerPed->GetIsInVehicle() ? PLAYER_DAMAGE_IGNORE_RANGE_IN_VEHICLE : PLAYER_DAMAGE_IGNORE_RANGE_ON_FOOT);

				int flatDist = static_cast<int>(diff.XYMag());
				
				// If the position isn't too far away and we've not hit a weapon (as the position will be in local space).
				if((Abs(flatDist - static_cast<int>(m_playerDistance)) > ignoreDist) && (!m_hitEntityWeapon))
				{
					LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Shot too far away (player dist: %d, local dist: %d)", m_playerDistance, flatDist));
					LOGGING_ONLY(m_bPlayerTooFarAway = true);
					weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Shot too far away-->return true");
					return true;
				}
			}
        }
    }
    else
    {
        if(m_parentID != NETWORK_INVALID_OBJECT_ID)
        {
            pParentObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_parentID);

            if (!pParentObject || !pParentObject->GetEntity())
            {
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "No parent entity"));
                LOGGING_ONLY(m_bNoParentObject = true);
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--No parent entity-->return true");
                return true;
            }
        }

        gnetAssert(m_hitObjectID != NETWORK_INVALID_OBJECT_ID);

        pHitObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_hitObjectID);

        if (!pHitObject || !pHitObject->GetEntity())
        {
			LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "No hit entity %d"));
            LOGGING_ONLY(m_bNoHitObject = true);
			weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--No hit entity-->return true");
            return true;
        }
    }

	if(pHitObject->IsClone() || pHitObject->IsPendingOwnerChange())
    {
		LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Hit entity is clone or migrating"));
        LOGGING_ONLY(m_bHitObjectIsClone = true);
		weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Hit entity is clone or migrating-->return true");
        return true;
    }

	if (m_damageType == DAMAGE_PED)
	{
		CEntity* hitEntity = pHitObject->GetEntity();

		if (hitEntity && hitEntity->GetIsTypePed())
		{
			CPed* ped = static_cast<CPed*>(hitEntity);

			// ignore weapon damage events if the player is already being killed by Stealth - Delayed Kill
			if (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth))
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Ped killed by stealth"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Ped killed by stealth-->return true");
				return true;
			}

			// ignore weapon damage events if the player is already being killed by 1 punch kill - Delayed Kill
			if (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown))
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Ped killed by takedown"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Ped killed by takedown-->return true");
				return true;
			}
		}

		// ignore shots from dead peds, this avoids double kills
		CEntity* parentEntity = pParentObject->GetEntity();

		if (parentEntity && parentEntity->GetIsTypePed())
		{
			CPed* ped = static_cast<CPed*>(parentEntity);

			if (ped->GetIsDeadOrDying())
			{
				LOGGING_ONLY(NetworkInterface::GetEventManager().GetLog().WriteDataValue("NOT PROCESSED", "Attacking ped is dead"));
				weaponDebugf3("CWeaponDamageEvent::DoBasicRejectionChecks--NOT PROCESSED--Attacking ped is dead-->return true");
				return true;
			}
		}

	}

    return false;
}

bool CWeaponDamageEvent::ModifyDamageBasedOnWeapon(const netObject *parentObject, bool *shouldReply)
{
	const CWeaponInfo *weaponInfo = 0;

	if (m_weaponHash != 0)
	{
		weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
		gnetAssertf(weaponInfo, "Received a CWeaponDamageEvent with an unrecognised weapon hash");
	}

    // check if we have the weapon dealing the damage loaded in memory yet, if not
    // we can't apply the damage effects locally (such as bullets firing etc...)
    if(weaponInfo)
	{
        bool modelsLoaded = true;
        const strLocalIndex modelIndex = strLocalIndex(weaponInfo->GetModelIndex());
		fwModelId modelId(modelIndex);
		if(CModelInfo::IsValidModelInfo(modelIndex.Get()) && !CModelInfo::HaveAssetsLoaded(modelId))
        {
           CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
            modelsLoaded = false;
        }

        if(!modelsLoaded)
        {
			if (shouldReply)
	            *shouldReply = false;
    
			return false;
        }

		if (!m_bOverride)
		{
			const CPed     *parentPed     = NetworkUtils::GetPedFromNetworkObject(parentObject);
			const CVehicle *parentVehicle = NetworkUtils::GetVehicleFromNetworkObject(parentObject);

			// get the default weapon damage for the given weapon type
			if((parentPed || (parentVehicle && parentVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)) && weaponInfo)
			{
				if(weaponInfo->GetIsProjectile() && parentPed)
				{
					const CAmmoInfo *ammoInfo = weaponInfo->GetAmmoInfo();

					if(gnetVerify(ammoInfo) && gnetVerify(ammoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId())))
					{
						const CAmmoProjectileInfo *projectileInfo = static_cast<const CAmmoProjectileInfo *>(ammoInfo);

						m_weaponDamage = static_cast<u32>(projectileInfo->GetDamage());
					}
				}
				else
				{
					m_weaponDamage = static_cast<u32>(weaponInfo->GetDamage());
				}

				if(parentPed && m_weaponDamage == 0)
				{
					//NetworkInterface::GetEventManager().GetLog().WriteErrorHeader("Weapon damage value is NULL! Chosen weapon type is %d!", parentPed->GetWeaponManager().GetSelectedWeaponHash());
				}
			}
			else
			{
				gnetAssertf(0, "Received a weapon damage event that is not overriding the damage value for a case not handled!");
				if (shouldReply)
	                *shouldReply = true;
	
				return false;
			}
		}
	}

    return true;
}

void CWeaponDamageEvent::HandlePlayerShootingPed(const netPlayer &shootingPlayer, netObject *hitObject, bool damageFailed)
{
	if (damageFailed)
	{
		weaponDebugf3("ProcessDamage failed");

		// send a reply to the player informing him that the damage failed and to abort any damage reactions locally running on the ped
		m_RequiresReply = true;
	}
	else
	{
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, hitObject);

		// migrate the ped being hit to the player firing at him, so the reactions look better on his machine
		if (pPedObj && !pPedObj->IsClone() && !pPedObj->IsPendingOwnerChange())
		{
			CPed* pPed = pPedObj->GetPed();

			if (pPed && !pPed->GetIsInVehicle() && !pPed->GetIsDeadOrDying() && 
				static_cast<CNetObjProximityMigrateable*>(pPedObj)->IsProximityMigrationPermitted() && 
                !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SYNCHRONIZED_SCENE) &&
				pPedObj->GetSyncData() &&
				pPedObj->CanPassControl(shootingPlayer, MIGRATE_FORCED))
			{
				CGiveControlEvent::Trigger(shootingPlayer, hitObject, MIGRATE_FORCED);
			}
		}
	}
}

bool CWeaponDamageEvent::CheckForDelayedKill(const netObject *parentObject, const netObject *hitObject)
{
	weaponDebugf3("CWeaponDamageEvent::CheckForDelayedKill m_damageType[%u] m_damageTime[%u]",m_damageType,m_damageTime);

    if (m_damageType == DAMAGE_PLAYER && AssertVerify(m_damageTime != 0))
    {
		weaponDebugf3("DAMAGE_PLAYER m_damageTime[%u]",m_damageTime);

		CPed *myPlayer = NULL;
		if(hitObject)
		{
			myPlayer = static_cast<CPed*>(hitObject->GetEntity());
		}

        if (myPlayer)
        {
			weaponDebugf3("myPlayer[%p]",myPlayer);

            const CPed *parentPlayerPed   = NetworkUtils::GetPlayerPedFromNetworkObject(parentObject);
            CNetObjGame  *parentNetObject = parentPlayerPed ? parentPlayerPed->GetNetworkObject() : 0;

            CNetGamePlayer *playerOwner = parentNetObject ? parentNetObject->GetPlayerOwner() : 0;

            if(playerOwner)
            {
                PhysicalPlayerIndex playerIndex = playerOwner->GetPhysicalPlayerIndex();

			    bool bIsDelayedKillPending = IsDelayedKillPending();

			    weaponDebugf3("IsDelayedKillPending[%d] GetEntity[%p] ms_aLastPlayerDamageTimes[%d].time[%u] m_damageTime[%u]",bIsDelayedKillPending,parentObject->GetEntity(),playerIndex,ms_aLastPlayerKillTimes[playerIndex].time,m_damageTime);

                // if this going to kill my player? If so and we fired a shot at the other player first we need to wait until we
                // see if that shot killed the other player before we are killed
                if (!bIsDelayedKillPending && parentPlayerPed && ms_aLastPlayerKillTimes[playerIndex].time != 0)
                {
				    bool bKilled = ComputeWillKillPlayer(*parentPlayerPed, *myPlayer);

				    if (ms_aLastPlayerKillTimes[playerIndex].time < m_damageTime &&
					    m_damageTime - ms_aLastPlayerKillTimes[playerIndex].time < 1000)
				    {
					    weaponDebugf3("ms_aLastPlayerKillTimes[playerIndex].time[%u] < m_damageTime[%u]", ms_aLastPlayerKillTimes[playerIndex].time, m_damageTime);
					    weaponDebugf3("bKilled[%d]",bKilled);

					    if (bKilled)
					    {
							    // delay the kill to see if we killed him first
						    RegisterDelayedKill(playerIndex,
											    m_weaponHash,
											    static_cast<float>(m_weaponDamage),
											    m_component);

						    weaponDebugf3("return true");
						    return true;
					    }
				    }
				    else 
				    {
					    ms_aLastPlayerKillTimes[playerIndex].time = 0;

					    if (bKilled)
					    {
						    weaponDebugf3("Abort NM shot reaction");
						    // he killed us first, abort any local shot reactions on him
						    static_cast<CNetObjPlayer*>(parentPlayerPed->GetNetworkObject())->AbortNMShotReaction();
					    }
				    }
                }
            }
        }
    }

	weaponDebugf3("return false");
    return false;
}

// name:        RegisterDelayedKill
// description: Used to delay kills and stop double kills when two players shoot each other at roughly the same time
void CWeaponDamageEvent::RegisterDelayedKill(PhysicalPlayerIndex enemyPlayerIndex, u32 weaponHash, float fDamage, int hitComponent)
{
	weaponDebugf3("CWeaponDamageEvent::RegisterDelayedKill enemyPlayerIndex[%d] fDamage[%f] hitComponent[%d]",enemyPlayerIndex,fDamage,hitComponent);

    ms_delayedKillData.m_enemyPlayerIndex = enemyPlayerIndex;
    ms_delayedKillData.m_weaponHash = weaponHash;
    ms_delayedKillData.m_fDamage = fDamage;
    ms_delayedKillData.m_hitComponent = hitComponent;
    ms_delayedKillData.m_time = NetworkInterface::GetSyncedTimeInMilliseconds();

	weaponDebugf3("-->IsDelayedKillPending(playerindex=%d)[%d]",enemyPlayerIndex,IsDelayedKillPending(enemyPlayerIndex));
}

// name:        RegisterDelayedKill
// description: Returns true if we have a delayed kill waiting
// params:      killingPlayerIndex - this
bool CWeaponDamageEvent::IsDelayedKillPending(PhysicalPlayerIndex killingPlayerIndex)
{
    return ms_delayedKillData.IsSet(killingPlayerIndex);
}

// name:        RegisterDelayedKill
// description: Apply the delayed kill shot data to our player
void CWeaponDamageEvent::ApplyDelayedKill()
{
	weaponDebugf3("CWeaponDamageEvent::ApplyDelayedKill");

    if (ms_delayedKillData.IsSet())
    {
        CNetGamePlayer* KillingPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(ms_delayedKillData.m_enemyPlayerIndex);

        if (KillingPlayer && KillingPlayer->GetPlayerPed())
        {
            if ((fwTimer::GetSystemTimeInMilliseconds() - ms_delayedKillData.m_time) < 500)
            {
                Vector3 worldPos = Vector3(0.0f, 0.0f, 0.0f);

                // weapon damage and effects
                CWeapon::ReceiveFireMessage(KillingPlayer->GetPlayerPed(),
                                            NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetPlayerInfo()->GetPlayerPed(),
                                            ms_delayedKillData.m_weaponHash,
                                            static_cast<float>(ms_delayedKillData.m_fDamage),
                                            worldPos,
                                            ms_delayedKillData.m_hitComponent);

				if (KillingPlayer->GetPlayerPed() && KillingPlayer->GetPlayerPed()->GetNetworkObject())
				{
					weaponDebugf3("Abort NM shot reaction");
					static_cast<CNetObjPlayer*>(KillingPlayer->GetPlayerPed()->GetNetworkObject())->AbortNMShotReaction();
				}

            }
        }

        ms_delayedKillData.Reset();
    }
}

// name:        ClearDelayedKill
// description: Resets delayed kill data
void CWeaponDamageEvent::ClearDelayedKill()
{
	weaponDebugf3("CWeaponDamageEvent::ClearDelayedKill");
    ms_delayedKillData.Reset();
}

// ===========================================================================================================
// REQUEST PICKUP EVENT
// ===========================================================================================================

CRequestPickupEvent::CRequestPickupEvent() :
netGameEvent( REQUEST_PICKUP_EVENT, true ),
m_pedId(NETWORK_INVALID_OBJECT_ID),
m_pickupId(NETWORK_INVALID_OBJECT_ID),
m_failureCode(0),
m_timer(0),
m_bGranted(false),
m_bPlacement(false),
m_locallyTriggered(false)
{
}

CRequestPickupEvent::CRequestPickupEvent( const CPed* pPed, const CPickup* pPickup) :
netGameEvent( REQUEST_PICKUP_EVENT, true  ),
m_pedId(pPed->GetNetworkObject()->GetObjectID()),
m_pickupId(pPickup->GetNetworkObject()->GetObjectID()),
m_failureCode(0),
m_timer(fwTimer::GetSystemTimeInMilliseconds()),
m_bGranted(false),
m_bPlacement(false),
m_locallyTriggered(true)
{
}

CRequestPickupEvent::CRequestPickupEvent( const CPed* pPed, const CPickupPlacement* pPickupPlacement) :
netGameEvent( REQUEST_PICKUP_EVENT, true  ),
m_pedId(pPed->GetNetworkObject()->GetObjectID()),
m_pickupId(pPickupPlacement->GetNetworkObject()->GetObjectID()),
m_failureCode(0),
m_timer(fwTimer::GetSystemTimeInMilliseconds()),
m_bGranted(false),
m_bPlacement(true),
m_locallyTriggered(true)
{
}

CRequestPickupEvent::~CRequestPickupEvent()
{
	if (m_locallyTriggered)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_pickupId);
		CPickup* pPickup = pObj ? NetworkUtils::GetPickupFromNetworkObject(pObj) : NULL;

		// inform pickup not to wait for this event anymore
		if (pPickup)
			pPickup->ProcessCollectionResponse(NULL, false);
	}
}

void CRequestPickupEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRequestPickupEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestPickupEvent::Trigger(const CPed* pPed, const CPickup* pPickup)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    if (AssertVerify(NetworkInterface::IsGameInProgress()) && AssertVerify(pPickup->GetNetworkObject()) && AssertVerify(pPed->GetNetworkObject()))
    {
        NetworkInterface::GetEventManager().CheckForSpaceInPool();
        CRequestPickupEvent *pEvent = rage_new CRequestPickupEvent(pPed, pPickup);
        NetworkInterface::GetEventManager().PrepareEvent(pEvent);
    }
}

void CRequestPickupEvent::Trigger(const CPed* pPed, const CPickupPlacement* pPickupPlacement)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (AssertVerify(NetworkInterface::IsGameInProgress()) && AssertVerify(pPickupPlacement->GetNetworkObject()) && AssertVerify(pPed->GetNetworkObject()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CRequestPickupEvent *pEvent = rage_new CRequestPickupEvent(pPed, pPickupPlacement);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CRequestPickupEvent::IsInScope( const netPlayer& player ) const
{
    netObject* pPickupObj = NetworkInterface::GetNetworkObject(m_pickupId);

	netPlayer* pPlayerOwner = pPickupObj ? pPickupObj->GetPlayerOwner() : NULL;

	if (pPlayerOwner)
	{
		if (pPlayerOwner->IsLocal())
		{
			if (pPickupObj->GetPendingPlayerOwner() && pPickupObj->GetPendingPlayerOwner() == &player)
			{
				return true;
			}
		}
		else if (pPickupObj->GetPlayerOwner() == &player)
		{
			return true;
		} 
	}

    return false;
}

bool CRequestPickupEvent::HasTimedOut() const
{
	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

	// basic wrapping handling (this doesn't have to be precise, and the system time should never wrap)
	if(currentTime < m_timer)
	{
		m_timer = currentTime;
	}

	u32 timeElapsed = currentTime - m_timer;

	return timeElapsed > EVENT_TIMEOUT;
}

template <class Serialiser> void CRequestPickupEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_pedId, "Ped id");
	SERIALISE_OBJECTID(serialiser, m_pickupId, "Pickup/Placement id");
	SERIALISE_BOOL(serialiser, m_bPlacement, "Placement");
}

template <class Serialiser> void CRequestPickupEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
	static const unsigned SIZEOF_FAILURE_CODE = datBitsNeeded<CPickup::PCC_NUM_FAILURE_REASONS-1>::COUNT;

	Serialiser serialiser(messageBuffer);

    SERIALISE_BOOL(serialiser, m_bGranted, "Granted");

	if (!m_bGranted)
	{
		SERIALISE_UNSIGNED(serialiser, m_failureCode, SIZEOF_FAILURE_CODE, "Failure code");
	}
}

void CRequestPickupEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestPickupEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestPickupEvent::Decide (const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    m_bGranted = false;

    netObject* pPedObj = NetworkInterface::GetNetworkObject(m_pedId);
    netObject* pPickupObj = NetworkInterface::GetNetworkObject(m_pickupId);
    CPed* pPed = pPedObj ? NetworkUtils::GetPedFromNetworkObject(pPedObj) : NULL;

	if (m_bPlacement)
	{
		CPickupPlacement* pPickupPlacement = pPickupObj ? NetworkUtils::GetPickupPlacementFromNetworkObject(pPickupObj) : NULL;

		if (pPickupPlacement && pPed && !pPickupPlacement->GetIsCollected())
		{
			if (!pPickupObj->IsClone() && !pPickupObj->IsPendingOwnerChange())
			{
				if (pPickupPlacement->GetPickup())
				{
					// the collection type is not used here
					if (pPickupPlacement->GetPickup()->CanCollect(pPed, CPickup::COLLECT_CLONE, &m_failureCode))
					{
						pPickupPlacement->GetPickup()->Collect(pPed);
						m_bGranted = true;
					}
				}
				else
				{
					CPickupManager::RegisterPickupCollection(pPickupPlacement, pPed);
					m_bGranted = true;
				}
			}
			else if (pPickupPlacement->GetPickup())
			{
				pPickupPlacement->GetPickup()->SetRemotePendingCollection(*pPed);
			}
		}
	}
	else
	{
		CPickup* pPickup = pPickupObj ? NetworkUtils::GetPickupFromNetworkObject(pPickupObj) : NULL;

		if (pPickup && pPed && !pPickup->IsFlagSet(CPickup::PF_Collected))
		{
			if (!pPickupObj->IsClone() && !pPickupObj->IsPendingOwnerChange())
			{
				// the collection type is not used here
				if (pPickup->CanCollect(pPed, CPickup::COLLECT_INVALID, &m_failureCode))
				{
					pPickup->Collect(pPed);
					m_bGranted = true;
				}
			}
			else
			{
				pPickup->SetRemotePendingCollection(*pPed);
			}
		}
	}
		

    return true;
}

void CRequestPickupEvent::PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CRequestPickupEvent::HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEventReply<CSyncDataReader>(messageBuffer);

    netObject* pObj = NetworkInterface::GetNetworkObject(m_pickupId);

	if (m_bPlacement)
	{
		CPickupPlacement* pPickupPlacement = pObj ? NetworkUtils::GetPickupPlacementFromNetworkObject(pObj) : NULL;

		if (pPickupPlacement && pPickupPlacement->GetPickup())
		{
			pPickupPlacement->GetPickup()->ProcessCollectionResponse(&fromPlayer, m_bGranted);
		}
	}
	else
	{
		CPickup* pPickup = pObj ? NetworkUtils::GetPickupFromNetworkObject(pObj) : NULL;

		// inform pickup to stop waiting for collection response
		if (pPickup)
		{
			pPickup->ProcessCollectionResponse(&fromPlayer, m_bGranted);
		}
	}
}

#if ENABLE_NETWORK_LOGGING

void CRequestPickupEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

    netObject* pPickupObj = NetworkInterface::GetNetworkObject(m_pickupId);
    logSplitter.WriteDataValue("Pickup", " %s", pPickupObj ? pPickupObj->GetLogName() : "Doesn't exist");
}

void CRequestPickupEvent::WriteDecisionToLogFile() const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Granted", "%s", m_bGranted ? "true" : "false");

	if (!m_bGranted && m_failureCode != 0)
	{
		logSplitter.WriteDataValue("Failure reason", "%s", CPickup::GetCanCollectFailureString(m_failureCode));
	}
}

void CRequestPickupEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

    logSplitter.WriteDataValue("Granted", " %s", m_bGranted ? "true" : "false");

	if (!m_bGranted && m_failureCode != 0)
	{
		logSplitter.WriteDataValue("Failure reason", "%s", CPickup::GetCanCollectFailureString(m_failureCode));
	}
}

#endif // ENABLE_NETWORK_LOGGING

bool CRequestPickupEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == REQUEST_PICKUP_EVENT)
    {
        const CRequestPickupEvent *pRequestPickupEvent = SafeCast(const CRequestPickupEvent, pEvent);

        if (pRequestPickupEvent->m_pickupId== m_pickupId)
        {
            // another request for the same pickup, restart the timer
            ((CRequestPickupEvent*)this)->m_timer = fwTimer::GetSystemTimeInMilliseconds();
			pRequestPickupEvent->m_locallyTriggered = false;
            return true;
        }
    }

    return false;
}

// ===========================================================================================================
// REQUEST MAP PICKUP EVENT
// ===========================================================================================================

CRequestMapPickupEvent::CRequestMapPickupEvent(const CPickupPlacement* pPlacement) 
: netGameEvent( REQUEST_MAP_PICKUP_EVENT, true  )
, m_timer(fwTimer::GetSystemTimeInMilliseconds())
, m_bGranted(false)
, m_bGrantedByAllPlayers(true)
, m_bLocallyTriggered(false)
#if ENABLE_NETWORK_LOGGING
, m_rejectReason(REJECT_NONE)
#endif
{
	if (pPlacement)
	{
		m_placementInfo = *pPlacement->GetScriptInfo();
		m_bLocallyTriggered = true;
	}
}

CRequestMapPickupEvent::~CRequestMapPickupEvent()
{
	if (m_bLocallyTriggered)
	{
		CPickupPlacement* pPickupPlacement = GetPlacement();

		if (pPickupPlacement && !pPickupPlacement->GetIsBeingDestroyed() && pPickupPlacement->GetPickup())
		{
			pPickupPlacement->GetPickup()->ProcessCollectionResponse(NetworkInterface::GetLocalPlayer(), m_bGrantedByAllPlayers);
		}
	}
}

void CRequestMapPickupEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRequestMapPickupEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestMapPickupEvent::Trigger(const CPickupPlacement* pPlacement)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssertf(!pPlacement->GetLocalOnly(), "CRequestMapPickupEvent being triggered on a local only pickup");

	if (AssertVerify(NetworkInterface::IsGameInProgress()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CRequestMapPickupEvent *pEvent = rage_new CRequestMapPickupEvent(pPlacement);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CRequestMapPickupEvent::IsInScope( const netPlayer& player ) const
{
	const float SCOPE_RANGE_ON_FOOT = 10.0f;	// the range at which this event is in scope with another player if the pickup is collectable on foot
	const float SCOPE_RANGE_IN_VEHICLE = 20.0f;	// the range at which this event is in scope with another player if the pickup is collectable in vehicle

	const CNetGamePlayer& gamePlayer = static_cast<const CNetGamePlayer&>(player);

	if (gamePlayer.GetPlayerPed() && !player.IsLocal())
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_placementInfo.GetScriptId());

		if (gamePlayer.GetPlayerPed() && pHandler && pHandler->GetNetworkComponent())
		{
			if (pHandler->GetNetworkComponent()->IsPlayerAParticipant(player))
			{
				// find the map placement for this pickup
				scriptHandlerObject* pScriptObj = pHandler->GetScriptObject(m_placementInfo.GetObjectId());

				if (pScriptObj && pScriptObj->GetType() != SCRIPT_HANDLER_OBJECT_TYPE_PICKUP)
				{
					gnetAssertf(0, "CRequestMapPickupEvent::IsInScope: (%s) the script object (%d) is not a pickup", pHandler->GetLogName(), m_placementInfo.GetObjectId());
				}
				else
				{
					CPickupPlacement* pPlacement = SafeCast(CPickupPlacement, pScriptObj);

					if (pPlacement && pPlacement->GetPickup())
					{
						// some map pickups may be set to only be networked on a subset of machines, check for that here
						/*if (pPlacement->GetPlayersAvailability() != 0 && (pPlacement->GetPlayersAvailability() & (1 << player.GetPhysicalPlayerIndex())) == 0)
						{
							return false;
						}*/

						netObject* pPlacementObj = pPlacement->GetNetworkObject();
						netPlayer* pPlayerOwner = pPlacementObj ? pPlacementObj->GetPlayerOwner() : NULL;

						if (pPlayerOwner)
						{
							if (pPlayerOwner->IsLocal())
							{
								if (pPlacementObj->GetPendingPlayerOwner() && pPlacementObj->GetPendingPlayerOwner() == &player)
								{
									return true;
								}
							}
							else if (pPlacementObj->GetPlayerOwner() == &player)
							{
								return true;
							} 
						}

						Vector3 placementPos = pPlacement->GetPickupPosition();
						Vector3 playerPos	 = VEC3V_TO_VECTOR3(gamePlayer.GetPlayerPed()->GetTransform().GetPosition());

						Vector3 diff = placementPos - playerPos;

						float scopeRange = 0.0f;

						if (pPlacement->GetPickup()->GetAllowCollectionInVehicle() && gamePlayer.GetPlayerPed()->GetIsInVehicle())
						{	
							scopeRange = SCOPE_RANGE_IN_VEHICLE*SCOPE_RANGE_IN_VEHICLE;
						}
						else
						{
							scopeRange = SCOPE_RANGE_ON_FOOT*SCOPE_RANGE_ON_FOOT;
						}

						if (diff.Mag2() < scopeRange)
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool CRequestMapPickupEvent::HasTimedOut() const
{
	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

	// basic wrapping handling (this doesn't have to be precise, and the system time should never wrap)
	if(currentTime < m_timer)
	{
		m_timer = currentTime;
	}

	u32 timeElapsed = currentTime - m_timer;

	return timeElapsed > EVENT_TIMEOUT;
}

template <class Serialiser> void CRequestMapPickupEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	m_placementInfo.Serialise(serialiser);
}

template <class Serialiser> void CRequestMapPickupEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_BOOL(serialiser, m_bGranted, "Granted");
}

CPickupPlacement* CRequestMapPickupEvent::GetPlacement() const
{
	CPickupPlacement* pPlacement = NULL;

	scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_placementInfo.GetScriptId());

	if (pHandler)
	{
		// find the map placement for this pickup
		scriptHandlerObject* pScriptObj = pHandler->GetScriptObject(m_placementInfo.GetObjectId());

		if (pScriptObj && pScriptObj->GetType() != SCRIPT_HANDLER_OBJECT_TYPE_PICKUP)
		{
			gnetAssertf(0, "CRequestMapPickupEvent::GetPlacement: (%s) the script object (%d) is not a pickup", pHandler->GetLogName(), m_placementInfo.GetObjectId());
		}
		else
		{
			pPlacement = SafeCast(CPickupPlacement, pScriptObj);
		}
	}

	return pPlacement;
}

void CRequestMapPickupEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestMapPickupEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestMapPickupEvent::Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	m_bGranted = true;

	CPickupPlacement* pPickupPlacement = GetPlacement();

	if (pPickupPlacement)
	{
		if (pPickupPlacement->GetNetworkObject())
		{
			LOGGING_ONLY(m_rejectReason = REJECT_PLACEMENT_NETWORKED);
			m_bGranted = false;
		}
		else if (pPickupPlacement->GetPickup() && 
				 pPickupPlacement->GetPickup()->GetIsPendingCollection() &&
				 fromPlayer.GetRlPeerId() < toPlayer.GetRlPeerId())
		{
			LOGGING_ONLY(m_rejectReason = REJECT_PENDING_COLLECTION);
			m_bGranted = false;
		}

		if (m_bGranted && pPickupPlacement->GetPickup())
		{
			CPed* pPlayerPed = static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed();

			if (pPlayerPed)
			{
				pPickupPlacement->GetPickup()->SetRemotePendingCollection(*pPlayerPed);
			}
		}
	}

	return true;
}

void CRequestMapPickupEvent::PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CRequestMapPickupEvent::HandleReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEventReply<CSyncDataReader>(messageBuffer);

	if (!m_bGranted)
	{
		m_bGrantedByAllPlayers = false;
	}
}

#if ENABLE_NETWORK_LOGGING

void CRequestMapPickupEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{

	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	char logName[50];
	m_placementInfo.GetLogName(logName, 50);

	logSplitter.WriteDataValue("Placement", logName);
}

void CRequestMapPickupEvent::WriteDecisionToLogFile() const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Granted", " %s", m_bGranted ? "true" : "false");

	if (!m_bGranted && m_rejectReason != REJECT_NONE)
	{
		switch (m_rejectReason)
		{
		case REJECT_PLACEMENT_NETWORKED:
			logSplitter.WriteDataValue("Reason", "Placement is already networked (collected)");
			 break;
		case REJECT_PENDING_COLLECTION:
			logSplitter.WriteDataValue("Granted", "Pickup is pending collection");
			 break;
		default:
			 gnetAssertf(0, "CRequestMapPickupEvent: Unrecognised reject reason %d", m_rejectReason);
		}
	}
}

void CRequestMapPickupEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Granted", " %s", m_bGranted ? "true" : "false");
}

#endif // ENABLE_NETWORK_LOGGING

bool CRequestMapPickupEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == REQUEST_MAP_PICKUP_EVENT)
	{
		const CRequestMapPickupEvent *pRequestPickupEvent = SafeCast(const CRequestMapPickupEvent, pEvent);

		if (pRequestPickupEvent->m_placementInfo == m_placementInfo)
		{
			// another request for the same pickup, restart the timer
			((CRequestMapPickupEvent*)this)->m_timer = fwTimer::GetSystemTimeInMilliseconds() + EVENT_TIMEOUT;
			pRequestPickupEvent->m_bLocallyTriggered = false;
			return true;
		}
	}

	return false;
}

// ===========================================================================================================
// GAME CLOCK EVENT
// ===========================================================================================================

#if !__FINAL
CDebugGameClockEvent::CDebugGameClockEvent() 
	: netGameEvent(CLOCK_EVENT, false, true)
	, m_pPlayer(NULL)
	, m_day((u32)CClock::GetDay())
	, m_month((u32)CClock::GetMonth())
	, m_year((u32)CClock::GetYear())
	, m_hours((u32)CClock::GetHour())
	, m_minutes((u32)CClock::GetMinute())
	, m_seconds((u32)CClock::GetSecond())
	, m_nFlags(FLAGS_NONE)
	, m_nSendTime(0)
{
    // day starts at 1 - see Date::IsValid
    gnetAssert(m_day > 0);
    gnetAssert(m_day <= 31);
	gnetAssert(m_month < 12);
	gnetAssert(m_hours < 24);
	gnetAssert(m_minutes < 60);
	gnetAssert(m_seconds < 60);
}

CDebugGameClockEvent::CDebugGameClockEvent(unsigned nFlags, const netPlayer* pPlayer)
	: netGameEvent(CLOCK_EVENT, false, true)
	, m_pPlayer(pPlayer)
	, m_nFlags(nFlags)
	, m_nSendTime(0)
{
    if(((m_nFlags & FLAG_IS_OVERRIDE) != 0) && !gnetVerify(CNetwork::IsClockTimeOverridden()))
    {
        gnetError("CDebugGameClockEvent :: Sending an override but time is not overridden!");
    }

	m_day = static_cast<u32>(CClock::GetDay());
	m_month = static_cast<u32>(CClock::GetMonth());
	m_year = static_cast<u32>(CClock::GetYear());

    if(((m_nFlags & FLAG_IS_OVERRIDE) == 0) && CNetwork::IsClockTimeOverridden())
    {
        int nHour, nMinute, nSecond, nLastMinAdded = 0;
        CNetwork::GetClockTimeStoredForOverride(nHour, nMinute, nSecond, nLastMinAdded);
        m_hours = static_cast<u32>(nHour);
        m_minutes = static_cast<u32>(nMinute);
        m_seconds = static_cast<u32>(nSecond);
    }
    else
    {
        m_hours = static_cast<u32>(CClock::GetHour());
        m_minutes = static_cast<u32>(CClock::GetMinute());
        m_seconds = static_cast<u32>(CClock::GetSecond());
    }
	gnetAssert(m_hours < 24);
	gnetAssert(m_minutes <= 60);
	gnetAssert(m_seconds <= 60);
}

void CDebugGameClockEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CDebugGameClockEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CDebugGameClockEvent::Trigger(unsigned nFlags, const netPlayer* pPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsNetworkOpen());

	if (NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
	{
		CDebugGameClockEvent *pEvent = rage_new CDebugGameClockEvent(nFlags, pPlayer);

		// if the clock has changed and there is another one on the queue we need to
		// destroy it as the old events are out of date
		netGameEvent *existingEvent = NetworkInterface::GetEventManager().GetExistingEventFromEventList(CLOCK_EVENT);

		if(existingEvent && (*existingEvent != pEvent))
		{
			NetworkInterface::GetEventManager().DestroyAllEventsOfType(CLOCK_EVENT);
		}

#if !__NO_OUTPUT
		if (CNetwork::IsClockOverrideLocked())
		{
			gnetError("Attempting to trigger clock override event with lock in place");
#if !__FINAL
			diagDefaultPrintStackTrace();
#endif
		}
#endif

		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
	else
	{
		gnetAssertf(0, "Warning - network event pool full, dumping game clock event");
	}
}

bool CDebugGameClockEvent::IsInScope( const netPlayer& player) const
{
	// if m_pPlayer is set then this event is only being sent to one player,
	// otherwise it is sent to all
	if (m_pPlayer)
	{
		return (&player == m_pPlayer);
	}

	return !player.IsMyPlayer();
}

template <class Serialiser> void CDebugGameClockEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_day, SIZEOF_DAY, "Day");
	SERIALISE_UNSIGNED(serialiser, m_month, SIZEOF_MONTH, "Month");	
	SERIALISE_UNSIGNED(serialiser, m_year, SIZEOF_YEAR, "Year");

	SERIALISE_UNSIGNED(serialiser, m_hours, SIZEOF_HOURS, "Hours");
	SERIALISE_UNSIGNED(serialiser, m_minutes, SIZEOF_MINUTES, "Minutes");
	SERIALISE_UNSIGNED(serialiser, m_seconds, SIZEOF_SECONDS, "Seconds");
    SERIALISE_UNSIGNED(serialiser, m_nFlags, NUM_FLAGS, "Flags");

    m_nSendTime = NetworkInterface::GetNetworkTime(); 
    SERIALISE_UNSIGNED(serialiser, m_nSendTime, 32, "Send Time");
}

void CDebugGameClockEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CDebugGameClockEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &OUTPUT_ONLY(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

    gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: From: %s, Flags: %d. Time: %02d:%02d:%02d. Is Overridden: %s. SendTime: %d, Current: %d", fromPlayer.GetLogName(), m_nFlags, m_hours, m_minutes, m_seconds, CNetwork::IsClockTimeOverridden() ? "True" : "False", m_nSendTime, NetworkInterface::GetNetworkTime());

#if !__NO_OUTPUT
	if (CNetwork::IsClockOverrideLocked())
	{
		gnetError("NetClock :: CDebugGameClockEvent::Handle :: Attempting to handle clock override event with lock in place");
#if !__FINAL
		diagDefaultPrintStackTrace();
#endif
	}
#endif

    // reset the modified clock flag
    if(((m_nFlags & FLAG_IS_DEBUG_RESET) != 0))
    {
		if(CClock::HasDebugModifiedInNetworkGame())
		{
			gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: Resetting Modified in Network Game");
			CClock::ResetDebugModifiedInNetworkGame();
		}
    }
    else
    {
	    CClock::SetDate(m_day, static_cast<Date::Month>(m_month), m_year);

		if(((m_nFlags & FLAG_IS_DEBUG_CHANGE) != 0) && CNetwork::IsClockTimeOverridden())
		{
			gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: Debug Change, Clearing Override");
			CNetwork::ClearClockTimeOverride();
		}

        // if override, just update the override directly
		if(((m_nFlags & FLAG_IS_OVERRIDE) != 0))
		{
			gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: Overriding clock time");
			CNetwork::OverrideClockTime(static_cast<int>(m_hours), static_cast<int>(m_minutes), static_cast<int>(m_seconds), false);
		}
        // do not override a local clock override with a maintenance debug update
        else if(!(CNetwork::IsClockTimeOverridden() && ((m_nFlags & FLAG_IS_DEBUG_MAINTAIN) != 0)))   
        {
            // work out how many seconds have passed since the send
            u32 nTimeSinceSend = (NetworkInterface::GetNetworkTime() > m_nSendTime) ? (NetworkInterface::GetNetworkTime() - m_nSendTime) : 0;
            u32 nClockSeconds = static_cast<u32>(static_cast<float>(nTimeSinceSend / (static_cast<float>(CClock::GetMsPerGameMinute()) / 60.0f)));

            // add to a date time structure
            DateTime tDateTime(Date(m_day, static_cast<Date::Month>(m_month), m_year), Time(m_hours, m_minutes, m_seconds));
            tDateTime.AddTime(0, 0, nClockSeconds);

            // threshold at which we always take the debug time
            static const int nChangeThreshold = 300; 

            // only switch when moving forward (prevents jumping) or when it drifts too far
            int nSecondsDiff = tDateTime.GetSecondsSince1900() - static_cast<int>(CClock::GetSecondsSince1900());
			if(nSecondsDiff > 0 || Abs(nSecondsDiff) > nChangeThreshold)
			{
				gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: Applying Clock: %02d:%02d:%02d", tDateTime.GetTime().GetHours(), tDateTime.GetTime().GetMinutes(), tDateTime.GetTime().GetSeconds());
				CClock::SyncDebugNetworkTime(tDateTime.GetTime().GetHours(), tDateTime.GetTime().GetMinutes(), tDateTime.GetTime().GetSeconds());
			}
            else
            {
				gnetDebug1("NetClock :: CDebugGameClockEvent::Handle :: Not Applying Clock - Diff: %us, Threshold: %us", nSecondsDiff, nChangeThreshold);
			}
        }
    }
}

#if ENABLE_NETWORK_LOGGING

void CDebugGameClockEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	WriteDataToLogFile(bEventLogOnly);
}

void CDebugGameClockEvent::WriteDataToLogFile(bool UNUSED_PARAM(bEventLogOnly)) const
{
	netLoggingInterface &log = NetworkInterface::GetEventManager().GetLog();

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Day", "%d", m_day);
	log.WriteDataValue("Month", "%d", m_month);
	log.WriteDataValue("Year", "%d", m_year);

	log.WriteDataValue("Hours", "%d", m_hours);
	log.WriteDataValue("Minutes", "%d", m_minutes);
	log.WriteDataValue("Seconds", "%d", m_seconds);
    log.WriteDataValue("Flags", "%d", m_nFlags);
}

void CDebugGameClockEvent::WriteDecisionToLogFile() const
{
	netLoggingInterface &log = NetworkInterface::GetEventManager().GetLog();

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Day", "%d", m_day);
	log.WriteDataValue("Month", "%d", m_month);
	log.WriteDataValue("Year", "%d", m_year);

	log.WriteDataValue("Hours", "%d", m_hours);
	log.WriteDataValue("Minutes", "%d", m_minutes);
	log.WriteDataValue("Seconds", "%d", m_seconds);
    log.WriteDataValue("Flags", "%d", m_nFlags);
}

#endif // ENABLE_NETWORK_LOGGING

bool CDebugGameClockEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == CLOCK_EVENT)
	{
		const CDebugGameClockEvent *pGameClockEvent = SafeCast(const CDebugGameClockEvent, pEvent);

		if (pGameClockEvent->m_day                == m_day     &&
			pGameClockEvent->m_month              == m_month   &&
			pGameClockEvent->m_year               == m_year    &&
			pGameClockEvent->m_hours              == m_hours   &&
			pGameClockEvent->m_minutes            == m_minutes &&
			pGameClockEvent->m_seconds            == m_seconds)
		{
			return true;
		}
	}

	return false;
}
#endif

// ===========================================================================================================
// CDebugGameWeatherEvent - sent by a host informing players of the current weather setup
// ===========================================================================================================
#if !__FINAL
CDebugGameWeatherEvent::CDebugGameWeatherEvent()
: netGameEvent(WEATHER_EVENT, false, true)
, m_bIsForced(false)
, m_nPrevIndex(0)
, m_nCycleIndex(0)
, m_pPlayer(NULL)
{

}

CDebugGameWeatherEvent::CDebugGameWeatherEvent(bool bForced, u32 nPrevIndex, u32 nCycleIndex, const netPlayer* pPlayer)
: netGameEvent(WEATHER_EVENT, false, true)
, m_bIsForced(bForced)
, m_nPrevIndex(nPrevIndex)
, m_nCycleIndex(nCycleIndex)
, m_pPlayer(pPlayer)
{

}

void CDebugGameWeatherEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CDebugGameWeatherEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CDebugGameWeatherEvent::Trigger(bool bForced, u32 nPrevIndex, u32 nCycleIndex, const netPlayer* pPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsNetworkOpen());

	if(NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
	{
		CDebugGameWeatherEvent* pEvent = rage_new CDebugGameWeatherEvent(bForced, nPrevIndex, nCycleIndex, pPlayer);

		// punt old events - we consider these out of date
		netGameEvent* pExistingEvent = NetworkInterface::GetEventManager().GetExistingEventFromEventList(WEATHER_EVENT);
		if(pExistingEvent && (*pExistingEvent != pEvent))
			NetworkInterface::GetEventManager().DestroyAllEventsOfType(WEATHER_EVENT);

		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
	else
		gnetAssertf(0, "Warning - network event pool full, dumping game clock event");
}

bool CDebugGameWeatherEvent::IsInScope(const netPlayer& player) const
{
	// if m_pPlayer is set then this event is only being sent to one player,
	// otherwise it is sent to all
	if (m_pPlayer)
	{
		return (&player == m_pPlayer);
	}

	return !player.IsMyPlayer();
}

template <class Serialiser> void CDebugGameWeatherEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const unsigned SIZEOF_WEATHERTYPE = datBitsNeeded<WEATHER_MAX_TYPES>::COUNT;
	static const unsigned SIZEOF_WEATHERLIST = datBitsNeeded<WEATHER_MAX_CYCLE_ENTRIES>::COUNT;

	Serialiser serialiser(messageBuffer);

	SERIALISE_BOOL(serialiser, m_bIsForced, "Weather Forced");
	SERIALISE_UNSIGNED(serialiser, m_nPrevIndex, SIZEOF_WEATHERTYPE, "Weather Type");

	if(!m_bIsForced)
		SERIALISE_UNSIGNED(serialiser, m_nCycleIndex, SIZEOF_WEATHERLIST, "Weather Cycle Index");
}

void CDebugGameWeatherEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CDebugGameWeatherEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

    gnetDebug1("Weather :: Applying from weather event");
	g_weather.SyncDebugNetworkWeather(m_bIsForced, m_nPrevIndex, m_nCycleIndex, 0);
}

#if ENABLE_NETWORK_LOGGING

void CDebugGameWeatherEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	WriteDataToLogFile(bEventLogOnly);
}

void CDebugGameWeatherEvent::WriteDataToLogFile(bool UNUSED_PARAM(bEventLogOnly)) const
{
	netLoggingInterface &log = NetworkInterface::GetEventManager().GetLog();

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Weather Forced", "%s", m_bIsForced ? "True" : "False");
	log.WriteDataValue("Weather Prev Index", "%d", m_nPrevIndex);
	log.WriteDataValue("Weather Cycle Index", "%d", m_nCycleIndex);
}

void CDebugGameWeatherEvent::WriteDecisionToLogFile() const
{
	netLoggingInterface &log = NetworkInterface::GetEventManager().GetLog();

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Weather Forced", "%s", m_bIsForced ? "True" : "False");
	log.WriteDataValue("Weather Prev Index", "%d", m_nPrevIndex);
	log.WriteDataValue("Weather Cycle Index", "%d", m_nCycleIndex);
}

#endif // ENABLE_NETWORK_LOGGING

bool CDebugGameWeatherEvent::operator==(const netGameEvent* pEvent) const
{
	if(pEvent->GetEventType() == WEATHER_EVENT)
	{
		const CDebugGameWeatherEvent* pGameEvent = SafeCast(const CDebugGameWeatherEvent, pEvent);
		if(pGameEvent->m_bIsForced == m_bIsForced && pGameEvent->m_nPrevIndex == m_nPrevIndex && pGameEvent->m_nCycleIndex == m_nCycleIndex)
			return true;
	}

	return false;
}
#endif
// ================================================================================================================
// CRespawnPlayerPedEvent - sent from a client to the host when they have resurrected themselves
// ================================================================================================================
CRespawnPlayerPedEvent::CRespawnPlayerPedEvent() :
netGameEvent( RESPAWN_PLAYER_PED_EVENT, true ),
m_bShouldReply(false),
m_spawnPoint(Vector3(0.0f, 0.0f, 0.0f)),
m_timeStamp(0),
m_RespawnNetObjId(NETWORK_INVALID_OBJECT_ID),
m_timeRespawnNetObj(0),
m_LastFrameScopeFlagsUpdated(0),
m_RespawnScopeFlags(0),
m_ResurrectPlayer(false),
m_EnteringMPCutscene(false),
m_RespawnFailed(false),
m_HasMoney(true),
m_HasScarData(false)
{
}

CRespawnPlayerPedEvent::CRespawnPlayerPedEvent(const Vector3 &spawnPoint, u32 timeStamp, const ObjectId respawnNetObjId, u32 timeRespawnNetObj, bool resurrectPlayer, bool enteringMPCutscene, bool hasMoney, bool hasScarData, float u, float v, float scale) :
netGameEvent( RESPAWN_PLAYER_PED_EVENT, true ),
m_bShouldReply(false),
m_spawnPoint(spawnPoint),
m_timeStamp(timeStamp),
m_RespawnNetObjId(respawnNetObjId),
m_timeRespawnNetObj(timeRespawnNetObj),
m_LastFrameScopeFlagsUpdated(0),
m_RespawnScopeFlags(0),
m_ResurrectPlayer(resurrectPlayer),
m_EnteringMPCutscene(enteringMPCutscene),
m_RespawnFailed(false),
m_HasMoney(hasMoney),
m_HasScarData(hasScarData),
m_scarU(u),
m_scarV(v),
m_scarScale(scale)
{
}

void CRespawnPlayerPedEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRespawnPlayerPedEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRespawnPlayerPedEvent::Trigger(const Vector3 &spawnPoint, const ObjectId respawnNetObjId, u32 timeRespawnNetObj, bool resurrectPlayer, bool enteringMPCutscene, bool hasMoney, bool hasScarData, float u, float v, float scale)
{
	respawnDebugf3("CRespawnPlayerPedEvent::Trigger spawnPoint[%f %f %f] respawnNetObjId[%u] timeRespawnNetObj[%u] resurrectPlayer[%d] enteringMPCutscene[%d] hasMoney[%d] hasScarData[%d] u[%f] v[%f] scale[%f]",spawnPoint.x,spawnPoint.y,spawnPoint.z,respawnNetObjId,timeRespawnNetObj,resurrectPlayer,enteringMPCutscene,hasMoney,hasScarData,u,v,scale);

    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CRespawnPlayerPedEvent *pEvent = rage_new CRespawnPlayerPedEvent(spawnPoint, NetworkInterface::GetNetworkTime(), respawnNetObjId, timeRespawnNetObj, resurrectPlayer, enteringMPCutscene, hasMoney, hasScarData, u, v, scale);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRespawnPlayerPedEvent::IsInScope( const netPlayer& player ) const
{
	if (player.IsLocal())
		return false;

	return true;
}

template <class Serialiser> void CRespawnPlayerPedEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const unsigned SIZEOF_RESPAWN_SCOPE_FLAGS = MAX_NUM_PHYSICAL_PLAYERS;
	static const float    MAXSIZE_SCARDATA_FLOAT = 100.0f;
	static const unsigned SIZEOF_SCARDATA_FLOAT = 32;

	Serialiser serialiser(messageBuffer);

	SERIALISE_POSITION(serialiser, m_spawnPoint, "Spawn Point");
	SERIALISE_UNSIGNED(serialiser, m_timeStamp, SIZEOF_TIMESTAMP, "Timestamp");
	SERIALISE_OBJECTID(serialiser, m_RespawnNetObjId, "RespawnNetObj Id");
	SERIALISE_UNSIGNED(serialiser, m_timeRespawnNetObj, SIZEOF_TIMESTAMP, "Respawn ped timer");
	SERIALISE_BITFIELD(serialiser, m_RespawnScopeFlags, SIZEOF_RESPAWN_SCOPE_FLAGS, "Respawn scope flags");
	SERIALISE_BOOL(serialiser, m_ResurrectPlayer, "Resurrect Player");
	SERIALISE_BOOL(serialiser, m_EnteringMPCutscene, "Entering MP cutscene");
	SERIALISE_BOOL(serialiser, m_HasMoney, "Has Money");

	SERIALISE_BOOL(serialiser, m_HasScarData, "Has Scar Data");
	if (m_HasScarData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser,m_scarU,MAXSIZE_SCARDATA_FLOAT,SIZEOF_SCARDATA_FLOAT,"Scar Data U");
		SERIALISE_PACKED_FLOAT(serialiser,m_scarV,MAXSIZE_SCARDATA_FLOAT,SIZEOF_SCARDATA_FLOAT,"Scar Data V");
		SERIALISE_PACKED_FLOAT(serialiser,m_scarScale,MAXSIZE_SCARDATA_FLOAT,SIZEOF_SCARDATA_FLOAT,"Scar Data Scale");
	}
	else
	{
		m_scarU = 0.f;
		m_scarV = 0.f;
		m_scarScale = 0.f;
	}
}

template <class Serialiser> void CRespawnPlayerPedEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_BOOL(serialiser, m_RespawnFailed, "Respawn Failed result");
}

void CRespawnPlayerPedEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    UpdateRespawnScopeFlags();

    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRespawnPlayerPedEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);

    gnetAssert(dynamic_cast<const CNetGamePlayer *>(&fromPlayer));
    const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(fromPlayer);

    CPlayerInfo* pPlayerInfo = player.GetPlayerPed() ? player.GetPlayerPed()->GetPlayerInfo() : 0;

	respawnDebugf3("CRespawnPlayerPedEvent::Handle fromPlayer.GetLogName[%s] pPlayerInfo[%p] GetPlayerState[%d] m_ResurrectPlayer[%d]",fromPlayer.GetLogName(),pPlayerInfo,pPlayerInfo ? pPlayerInfo->GetPlayerState() : -1,m_ResurrectPlayer);

    if (pPlayerInfo && pPlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_LEFTGAME)
    {
		m_bShouldReply = true;

        // check if the respawn ped is in scope of the local player - otherwise there is no point
        // waiting for the network object to be cloned on this machine
        if((m_RespawnScopeFlags & (1<<NetworkInterface::GetLocalPhysicalPlayerIndex())) == 0)
        {
            // respawn ped out of scope don't create a respawn ped
			respawnDebugf3("CRespawnPlayerPedEvent::Handle--((m_RespawnScopeFlags & (1<<NetworkInterface::GetLocalPhysicalPlayerIndex())) == 0)-->m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID");
            m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID;
        }
        else
        {
		    if (NETWORK_INVALID_OBJECT_ID != m_RespawnNetObjId)
		    {
			    netObject* networkObject = CNetwork::GetObjectManager().GetNetworkObject(m_RespawnNetObjId, true);
			    if (!networkObject)
			    {
				    //Interval that we can wait for the respawns object id to be created locally,
				    //  after that we give up.
				    if ((NetworkInterface::GetNetworkTime() - m_timeRespawnNetObj) < RESPAWN_PED_CREATION_INTERVAL)
				    {
						respawnDebugf3("CRespawnPlayerPedEvent::Handle--!networkObject--((NetworkInterface::GetNetworkTime()[%u] - m_timeRespawnNetObj[%u])[%u] < RESPAWN_PED_CREATION_INTERVAL[%u])-->m_bShouldReply = false",NetworkInterface::GetNetworkTime(),m_timeRespawnNetObj,(NetworkInterface::GetNetworkTime() - m_timeRespawnNetObj),RESPAWN_PED_CREATION_INTERVAL);
					    m_bShouldReply = false;
				    }
                    else
                    {
                        // timed-out don't create a respawn ped
						respawnDebugf3("CRespawnPlayerPedEvent::Handle--!networkObject--((NetworkInterface::GetNetworkTime()[%u] - m_timeRespawnNetObj[%u])[%u] >= RESPAWN_PED_CREATION_INTERVAL[%u])-->m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID",NetworkInterface::GetNetworkTime(),m_timeRespawnNetObj,(NetworkInterface::GetNetworkTime() - m_timeRespawnNetObj),RESPAWN_PED_CREATION_INTERVAL);
						m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID;
                    }
			    }
		    }
        }

		respawnDebugf3("CRespawnPlayerPedEvent::Handle--m_bShouldReply[%d]",m_bShouldReply);

		if (m_bShouldReply)
		{
			bool respawnSucceeded = false;

            //This needs to be done before Resurrect of the Clone.
            CNetObjPlayer *netObjPlayer = player.GetPlayerPed() ? static_cast<CNetObjPlayer*>(player.GetPlayerPed()->GetNetworkObject()) : 0;

			respawnDebugf3("CRespawnPlayerPedEvent::Handle--netObjPlayer[%p]",netObjPlayer);

			if(netObjPlayer)
	        {
				NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), NetworkInterface::GetLocalPhysicalPlayerIndex(), "TRYING_TO_CREATE_RESPAWN_PED", netObjPlayer->GetLogName());

				if (netObjPlayer->CanRespawnClonePlayerPed(m_RespawnNetObjId) BANK_ONLY(&& !NetworkDebug::FailCloneRespawnNetworkEvent()))
				{
					respawnSucceeded = netObjPlayer->RespawnPlayerPed(m_spawnPoint, false, m_RespawnNetObjId); 

					if(m_EnteringMPCutscene && gnetVerifyf(player.GetPlayerPed()->GetPlayerInfo(), "Remote player has no player info!"))
					{
						player.GetPlayerPed()->GetPlayerInfo()->SetPlayerState(CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE);
#if __FINAL
						netObjPlayer->SetIsVisible(false, "net");
#else
						netObjPlayer->SetIsVisible(false, "CRespawnPlayerPedEvent");
#endif // __FINAL
					}
				}

				respawnDebugf3("CRespawnPlayerPedEvent::Handle--SetIsVisibleForModule(SETISVISIBLE_MODULE_RESPAWN,false,true)");
				player.GetPlayerPed()->SetIsVisibleForModule(SETISVISIBLE_MODULE_RESPAWN,false,true);

				respawnDebugf3("CRespawnPlayerPedEvent::Handle--SetRespawnInvincibilityTimer(1000)--this will help prevent the SETISVISIBLE_MODULE_RESPAWN from being set to true incorrectly in the CNetObjPlayer::Update");
				netObjPlayer->SetRespawnInvincibilityTimer(1000);
				netObjPlayer->SetCloneRespawnInvincibilityTimerCountdown(true);

				netObjPlayer->SetRemoteHasMoney(m_HasMoney);
	        }

			m_RespawnFailed = !respawnSucceeded;

			respawnDebugf3("CRespawnPlayerPedEvent::Handle--m_ResurrectPlayer[%d]",m_ResurrectPlayer);
            if(m_ResurrectPlayer)
            {
				if (netObjPlayer)
					netObjPlayer->SetRemoteScarData(m_HasScarData,m_scarU,m_scarV,m_scarScale);

				CGameLogic::ResurrectClonePlayer(player.GetPlayerPed(), m_spawnPoint, m_timeStamp, m_RespawnFailed);
            }
		}
    }
}

bool CRespawnPlayerPedEvent::Decide (const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    return m_bShouldReply;
}

void CRespawnPlayerPedEvent::PrepareReply (datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CRespawnPlayerPedEvent::HandleReply (datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEventReply<CSyncDataReader>(messageBuffer);

	if (NETWORK_INVALID_OBJECT_ID != m_RespawnNetObjId)
	{
		netObject* netObjPlayer = CNetwork::GetObjectManager().GetNetworkObject(m_RespawnNetObjId, true);

		if (gnetVerify(netObjPlayer))
		{
			static_cast< CNetObjPed* >(netObjPlayer)->SetPlayerRespawnFlags(m_RespawnFailed, fromPlayer);
		}
	}
}

void CRespawnPlayerPedEvent::UpdateRespawnScopeFlags()
{
    if(fwTimer::GetSystemFrameCount() != m_LastFrameScopeFlagsUpdated)
    {
        if(m_RespawnNetObjId != NETWORK_INVALID_OBJECT_ID)
		{
            m_RespawnScopeFlags = 0;

			netObject *networkObject = CNetwork::GetObjectManager().GetNetworkObject(m_RespawnNetObjId, true);

			if(networkObject)
			{
                unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	            for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
                {
		            const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

                    if(networkObject->IsInScope(*remotePlayer))
                    {
                        if(gnetVerifyf(remotePlayer->GetPhysicalPlayerIndex() < MAX_NUM_PHYSICAL_PLAYERS, "Invalid physical player index!"))
                        {
                            m_RespawnScopeFlags |= (1<<remotePlayer->GetPhysicalPlayerIndex());
                        }
                    }
                }
            }
        }

        m_LastFrameScopeFlagsUpdated = fwTimer::GetSystemFrameCount();
    }
}

#if ENABLE_NETWORK_LOGGING

void CRespawnPlayerPedEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("Spawn point", "%f, %f, %f", m_spawnPoint.x, m_spawnPoint.y, m_spawnPoint.z);
    log.WriteDataValue("Timestamp", "%d", m_timeStamp);
	log.WriteDataValue("RespawnNetObj Id", "%d", m_RespawnNetObjId);
	log.WriteDataValue("Time Respawn NetObj", "%d", m_timeRespawnNetObj);
    log.WriteDataValue("Respawn scope flags", "%d", m_RespawnScopeFlags);
    log.WriteDataValue("Resurrect Player", "%s", m_ResurrectPlayer ? "TRUE" : "FALSE");
    log.WriteDataValue("Entering MP cutscene", "%s", m_EnteringMPCutscene ? "TRUE" : "FALSE");
	log.WriteDataValue("Has Money", "%s", m_HasMoney ? "TRUE" : "FALSE");
}

void CRespawnPlayerPedEvent::WriteDecisionToLogFile() const
{
	netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Reply", "%s", m_bShouldReply ? "Accept" : "Reject - Respawn Ped does not exist");

	if (!m_bShouldReply)
	{
		log.WriteDataValue("Spawn point", "%f, %f, %f", m_spawnPoint.x, m_spawnPoint.y, m_spawnPoint.z);
		log.WriteDataValue("Timestamp", "%d", m_timeStamp);
		log.WriteDataValue("RespawnNetObj Id", "%d", m_RespawnNetObjId);
		log.WriteDataValue("Time Respawn NetObj", "%d", m_timeRespawnNetObj);
	}
	else
	{
		log.WriteDataValue("Respawn Ped Failed", "PED_%d:%s", m_RespawnNetObjId, m_RespawnFailed ? "TRUE" : "FALSE");
	}
}


void CRespawnPlayerPedEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
	netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Reply", "%s", m_bShouldReply ? "Accept" : "Reject - Respawn Ped does not exist");
	log.WriteDataValue("Respawn Ped Failed", "PED_%d:%s", m_RespawnNetObjId, m_RespawnFailed ? "TRUE" : "FALSE");
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// GIVE WEAPON EVENT
// ===========================================================================================================

CGiveWeaponEvent::CGiveWeaponEvent()
: netGameEvent( GIVE_WEAPON_EVENT, false )
 ,m_pedID(NETWORK_INVALID_OBJECT_ID)
 ,m_weaponHash(0)
 ,m_ammo(0)
 ,m_bAsPickup(false)
{
}

CGiveWeaponEvent::CGiveWeaponEvent( CPed* pPed, u32 weaponHash, int ammo, bool bAsPickup) :
netGameEvent( GIVE_WEAPON_EVENT, false ),
m_pedID(pPed->GetNetworkObject()->GetObjectID()),
m_weaponHash(weaponHash),
m_ammo(ammo),
m_bAsPickup(bAsPickup)
{
}

void CGiveWeaponEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CGiveWeaponEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGiveWeaponEvent::Trigger(CPed* pPed, u32 weaponHash, int ammo, bool bAsPickup)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    gnetAssert(pPed);

	gnetAssertf(ammo < ((u64)1<<CGiveWeaponEvent::SIZEOF_AMMO), "Invalid ammo amount %d, max size is %" SIZETFMT "u", ammo, ((u64)1<<CGiveWeaponEvent::SIZEOF_AMMO));

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CGiveWeaponEvent *pEvent = rage_new CGiveWeaponEvent(pPed, weaponHash, ammo, bAsPickup);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CGiveWeaponEvent::IsInScope( const netPlayer& player) const
{
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    // send to the owner of the ped
    return (&player == pPedObj->GetPlayerOwner());
}

template <class Serialiser> void CGiveWeaponEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_pedID, "Ped Global ID");
    SERIALISE_UNSIGNED(serialiser, m_weaponHash, SIZEOF_WEAPON, "Weapon Type");
    SERIALISE_INTEGER(serialiser, m_ammo, SIZEOF_AMMO, "Ammo");
    SERIALISE_BOOL(serialiser, m_bAsPickup, "Given as a pickup");
}

void CGiveWeaponEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CGiveWeaponEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);

    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pPedObj);

    if (pPedObj && gnetVerifyf(pPed, "%s: Cannot handle CGiveWeaponEvent on non-ped objects.", pPedObj->GetLogName()))
    {
        if (pPedObj->IsClone())
        {
            // ped is owned by a different machine - pass on the message
            CGiveWeaponEvent::Trigger(pPed, m_weaponHash, m_ammo);
        }
        else
        {
			const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
			gnetAssertf(wi, "A CGiveWeaponEvent has been triggered with an invalid weapon hash");

			if (wi)
			{
				s32 weaponSlot = wi->GetSlot();
				if(pPed->GetInventory() && pPed->GetInventory()->GetWeaponBySlot(weaponSlot) == 0)
				{
					// we only want to give the weapon if they don't have it, and not extract the ammo
					if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
					{
						pPed->GetInventory()->AddWeaponAndAmmo(m_weaponHash, m_ammo);

						if(pPed->GetWeaponManager())
						{
							pPed->GetWeaponManager()->EquipBestWeapon();
							pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
						}
					}
					else
					{
						pPed->GetInventory()->AddWeaponAndAmmo(m_weaponHash, m_ammo);
					}
				}
			}
        }
	}
}

bool CGiveWeaponEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == GIVE_WEAPON_EVENT)
	{
		 const CGiveWeaponEvent *pWeaponDamageEvent = SafeCast(const CGiveWeaponEvent, pEvent);
		 if(pWeaponDamageEvent->m_pedID == m_pedID &&
			pWeaponDamageEvent->m_weaponHash == m_weaponHash &&
			pWeaponDamageEvent->m_bAsPickup == m_bAsPickup)
		 {
			 CGiveWeaponEvent* thisEvent = ((CGiveWeaponEvent*)this);
			 thisEvent->m_ammo += pWeaponDamageEvent->m_ammo;

			 return true;
		 }
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CGiveWeaponEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (pedObj)
    {
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
    }

    log.WriteDataValue("Weapon", "%d", m_weaponHash);
    log.WriteDataValue("Ammo", "%d", m_ammo);
    log.WriteDataValue("AsPickup", "%d", m_bAsPickup ? "True" : "False");
}

void CGiveWeaponEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (!pedObj)
    {
        log.WriteDataValue("Reply", "Reject - Ped does not exist");
    }
    else if (!pedObj->IsClone())
    {
        log.WriteDataValue("Reply", "Reject - Ped is not locally owned");
    }
    else
    {
        log.WriteDataValue("Reply", "Accept");
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
        log.WriteDataValue("Weapon", "%d", m_weaponHash);
        log.WriteDataValue("Ammo", "%d", m_ammo);
        log.WriteDataValue("AsPickup", "%d", m_bAsPickup ? "True" : "False");
    }
}

void CGiveWeaponEvent::WriteDataToLogFile(bool UNUSED_PARAM(bEventLogOnly)) const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (pedObj)
    {
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
        log.WriteDataValue("Weapon", "%d", m_weaponHash);
        log.WriteDataValue("Ammo", "%d", m_ammo);
        log.WriteDataValue("AsPickup", "%d", m_bAsPickup ? "True" : "False");
    }
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// REMOVE WEAPON EVENT
// ===========================================================================================================

CRemoveWeaponEvent::CRemoveWeaponEvent()
: netGameEvent( REMOVE_WEAPON_EVENT, false )
 ,m_pedID(NETWORK_INVALID_OBJECT_ID)
 ,m_weaponHash(0)
{
}

CRemoveWeaponEvent::CRemoveWeaponEvent( CPed* pPed, u32 weaponHash) :
netGameEvent( REMOVE_WEAPON_EVENT, false ),
m_pedID(pPed->GetNetworkObject()->GetObjectID()),
m_weaponHash(weaponHash)
{
}

void CRemoveWeaponEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRemoveWeaponEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemoveWeaponEvent::Trigger(CPed* pPed, u32 weaponHash)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    gnetAssert(pPed);

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CRemoveWeaponEvent *pEvent = rage_new CRemoveWeaponEvent(pPed, weaponHash);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRemoveWeaponEvent::IsInScope( const netPlayer& player) const
{
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    // send to the owner of the ped
    return (&player == pPedObj->GetPlayerOwner());
}

template <class Serialiser> void CRemoveWeaponEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_pedID, "Ped Global ID");
    SERIALISE_UNSIGNED(serialiser, m_weaponHash, SIZEOF_WEAPON, "Weapon Type");
}

void CRemoveWeaponEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoveWeaponEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);

    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pPedObj);

    if (pPedObj && gnetVerifyf(pPed, "%s: Cannot handle CRemoveWeaponEvent on non-ped objects.", pPedObj->GetLogName()))
    {
        if (pPedObj->IsClone())
        {
            // ped is owned by a different machine - pass on the message
            CRemoveWeaponEvent::Trigger(pPed, m_weaponHash);
        }
        else if(pPed->GetInventory())
        {
            pPed->GetInventory()->RemoveWeapon(m_weaponHash);
        }
    }
}

#if ENABLE_NETWORK_LOGGING

void CRemoveWeaponEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (pedObj)
    {
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
    }

    log.WriteDataValue("Weapon", "%d", m_weaponHash);
}

void CRemoveWeaponEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (!pedObj)
    {
        log.WriteDataValue("Reply", "Reject - Ped does not exist");
    }
    else if (!pedObj->IsClone())
    {
        log.WriteDataValue("Reply", "Reject - Ped is not locally owned");
    }
    else
    {
        log.WriteDataValue("Reply", "Accept");
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
        log.WriteDataValue("Weapon", "%d", m_weaponHash);
    }
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// REMOVE ALL WEAPONS EVENT
// ===========================================================================================================

CRemoveAllWeaponsEvent::CRemoveAllWeaponsEvent() :
netGameEvent( REMOVE_ALL_WEAPONS_EVENT, false )
, m_pedID(NETWORK_INVALID_OBJECT_ID)
{
}

CRemoveAllWeaponsEvent::CRemoveAllWeaponsEvent( CPed* pPed) :
netGameEvent( REMOVE_ALL_WEAPONS_EVENT, false ),
m_pedID(pPed->GetNetworkObject()->GetObjectID())
{
}

void CRemoveAllWeaponsEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRemoveAllWeaponsEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemoveAllWeaponsEvent::Trigger(CPed* pPed)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    gnetAssert(pPed);

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CRemoveAllWeaponsEvent *pEvent = rage_new CRemoveAllWeaponsEvent(pPed);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRemoveAllWeaponsEvent::IsInScope( const netPlayer& player) const
{
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    // send to the owner of the ped
    return (&player == pPedObj->GetPlayerOwner());
}

template <class Serialiser> void CRemoveAllWeaponsEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_pedID, "Ped Global ID");
}

void CRemoveAllWeaponsEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoveAllWeaponsEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &NET_ASSERTS_ONLY(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);

    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    CPed *pPed = NetworkUtils::GetPedFromNetworkObject(pPedObj);
    gnetAssert(pPed);

    if(pPed)
    {
        if (pPedObj->IsClone())
        {
            if(gnetVerifyf(!pPed->IsPlayer(), "Received remove all weapons request from %s for remote player %s! This is not allowed for anti-cheat reasons!", fromPlayer.GetLogName(), pPedObj->GetPlayerOwner() ? pPedObj->GetPlayerOwner()->GetLogName() : "Unknown"))
            {
                // ped is owned by a different machine - pass on the message
                CRemoveAllWeaponsEvent::Trigger(pPed);
            }
        }
        else if(pPed->GetInventory() && gnetVerifyf(!pPed->IsLocalPlayer(), "Received remove all weapons request from %s for the local player! This is not allowed for anti-cheat reasons!", fromPlayer.GetLogName()))
        {
		    //Load the ped with the default loadout.
            CPedInventoryLoadOutManager::SetDefaultLoadOut(pPed);
        }
    }
}

#if ENABLE_NETWORK_LOGGING

void CRemoveAllWeaponsEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (pedObj)
    {
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
    }
}

void CRemoveAllWeaponsEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (!pedObj)
    {
        log.WriteDataValue("Reply", "Reject - Ped does not exist");
    }
    else if (!pedObj->IsClone())
    {
        log.WriteDataValue("Reply", "Reject - Ped is not locally owned");
    }
    else
    {
        log.WriteDataValue("Reply", "Accept");
        log.WriteDataValue("Ped", "%s", pedObj->GetLogName());
    }
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// VEHICLE COMPONENT CONTROL EVENT
// ===========================================================================================================
CVehicleComponentControlEvent::CVehicleComponentControlEvent() :
netGameEvent( VEHICLE_COMPONENT_CONTROL_EVENT, true, true ),
m_vehicleID( NETWORK_INVALID_OBJECT_ID ),
m_pedID( NETWORK_INVALID_OBJECT_ID ),
m_pedInSeatID ( NETWORK_INVALID_OBJECT_ID ),
m_componentIndex( 0 ),
m_pOwnerPlayer(NULL),
m_bRequest(false),
m_bGranted(false),
m_bComponentIsASeat(false),
m_bReservationFailed(false),
m_bPedOwnerWrong(false),
m_bPedInSeatWrong(false),
m_bPedInSeatNotCloned(false),
m_bPedStillLeaving(false),
m_bPedStillVaulting(false)
{
}

CVehicleComponentControlEvent::CVehicleComponentControlEvent(   const ObjectId vehicleObjectID,
                                                                const ObjectId pedObjectID,
                                                                s32            componentIndex,
                                                                bool           bRequest,
                                                                const netPlayer *pOwnerPlayer) :
netGameEvent( VEHICLE_COMPONENT_CONTROL_EVENT, true, true ),
m_vehicleID( vehicleObjectID ),
m_pedID( pedObjectID ),
m_pedInSeatID ( NETWORK_INVALID_OBJECT_ID ),
m_bGranted(false),
m_bRequest(bRequest),
m_componentIndex((u8) componentIndex),
m_pOwnerPlayer(pOwnerPlayer),
m_bComponentIsASeat(false),
m_bReservationFailed(false),
m_bPedOwnerWrong(false),
m_bPedInSeatWrong(false),
m_bPedInSeatNotCloned(false),
m_bPedStillLeaving(false),
m_bPedStillVaulting(false)

{
    netObject *pedObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    if(gnetVerifyf(pedObject || !bRequest, "Creating a vehicle component control event request for a ped without a network object!"))
    {
        if(pedObject)
        {
            SetOwnerPlayer(pedObject->GetPlayerOwner());
        }
    }
    
    //Get the reservation mgr.
    CComponentReservationManager* pReservationMgr = GetComponentReservationMgr();
    if(!pReservationMgr)
		return;

	//Get the reservation.
    CComponentReservation *pReservation = pReservationMgr->FindComponentReservationByArrayIndex(componentIndex);
    gnetAssert(pReservation);

	//Find the seat index.
    int iSeatIndex = GetModelSeatInfo()->GetSeatFromBoneIndex(pReservation->GetBoneIndex());
    if (iSeatIndex > -1 && !m_bRequest)
    {
        m_bComponentIsASeat = true;
        CPed* pOccupier = GetSeatManager()->GetPedInSeat(iSeatIndex);

        if (pOccupier)
        {
            gnetAssert(pOccupier->GetNetworkObject());
            m_pedInSeatID = pOccupier->GetNetworkObject()->GetObjectID();
        }
    }
}

void CVehicleComponentControlEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CVehicleComponentControlEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CVehicleComponentControlEvent::Trigger(const ObjectId vehicleId, const ObjectId pedId, s32 componentIndex, bool bRequest, const netPlayer* pOwnerPlayer)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    gnetAssert(NetworkInterface::GetObjectManager().GetNetworkObject(vehicleId) || pOwnerPlayer);

    if (NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
    {
        CVehicleComponentControlEvent *pEvent = rage_new CVehicleComponentControlEvent(vehicleId, pedId, componentIndex, bRequest, pOwnerPlayer);
        NetworkInterface::GetEventManager().PrepareEvent(pEvent);
    }
    else
    {
        gnetAssertf(0, "Warning - network event pool full, dumping vehicle component control event");
    }
}

bool CVehicleComponentControlEvent::IsInScope( const netPlayer& player ) const
{
    // never want to send this event to local players
    if(player.IsLocal())
    {
        return false;
    }

    netObject* pVehicle = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);

    if (pVehicle && pVehicle->GetPlayerOwner())
    {
        // sent to player owns the vehicle
        return (&player == pVehicle->GetPlayerOwner());
    }
    else
    {
        // m_ownerPlayer is set when we are freeing a component for a non-existant vehicle
        return (&player == m_pOwnerPlayer);
    }
}

template <class Serialiser> void CVehicleComponentControlEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle Global ID");
    SERIALISE_OBJECTID(serialiser, m_pedID, "Ped Global ID");
    SERIALISE_UNSIGNED(serialiser, m_componentIndex, SIZEOF_COMPONENT, "Component Index");
    SERIALISE_BOOL(serialiser, m_bRequest, "Request");
    SERIALISE_BOOL(serialiser, m_bComponentIsASeat, "Component is a seat");

    if (!m_bRequest && m_bComponentIsASeat)
    {
        // if we are freeing a seat we need to send back who is sitting in it
        SERIALISE_OBJECTID(serialiser, m_pedInSeatID, "Ped in seat Global ID");
    }
}

template <class Serialiser> void CVehicleComponentControlEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_BOOL(serialiser, m_bGranted, "Is Granted");

    if (m_bGranted)
    {
        SERIALISE_BOOL(serialiser, m_bComponentIsASeat, "Component is a seat");

        if (m_bComponentIsASeat)
        {
            SERIALISE_OBJECTID(serialiser, m_pedInSeatID, "Ped in seat Global ID");
        }
    }
}

void CVehicleComponentControlEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CVehicleComponentControlEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CVehicleComponentControlEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	//Grab the network objects.
    netObject* pVehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
    netObject* pPedObj     = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    
    //Ensure the network objects are valid.
    if(!pVehicleObj || !pPedObj)
		return true;
		
	//Ensure the vehicle is not a clone or pending an ownership change.
	if(pVehicleObj->IsClone() || pVehicleObj->IsPendingOwnerChange())
		return true;
    
    //Grab the ped.
    CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pPedObj);
    
    //Ensure the ped is valid.
    if(!pPed)
		return true;

	if (pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT))
	{
		m_bPedStillVaulting = true;
		return true;
	}
		
	//Grab the reservation mgr.
	CComponentReservationManager* pReservationMgr = GetComponentReservationMgr();
	if(!pReservationMgr)
		return true;
    
    CPed* pLocalOccupier = NULL;

    m_bGranted = false;

    CComponentReservation *pReservation = pReservationMgr->FindComponentReservationByArrayIndex(m_componentIndex);
    
    if (!pPedObj->GetPlayerOwner() || pPedObj->GetPlayerOwner() != &fromPlayer)
    {
        m_bPedOwnerWrong = true;
        return true;
    }

    if(AssertVerify(pReservation))
    {
        int iSeatIndex = GetModelSeatInfo()->GetSeatFromBoneIndex(pReservation->GetBoneIndex());
        if (iSeatIndex > -1)
        {
            pLocalOccupier = GetSeatManager()->GetPedInSeat(iSeatIndex);
        }

        bool bIsASeat = iSeatIndex > -1;

        if(m_bRequest)
        {
            if (bIsASeat)
            {
				//Grab the vehicle.
				CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pVehicleObj);
                if (pVehicle && pVehicle->m_nVehicleFlags.bUsingPretendOccupants)
                {
                    // the ped cannot enter a vehicle with pretend occupants
                    return true;
                }

                if (pLocalOccupier && !pLocalOccupier->IsNetworkClone() && !(pLocalOccupier->GetNetworkObject() && pLocalOccupier->GetNetworkObject()->HasBeenCloned(fromPlayer)))
                {
                    // the ped cannot enter a vehicle if the occupier of the seat has not been cloned on his machine yet
                    m_bPedInSeatNotCloned = true;
                    return true;
                }
            }

            // a component is being requested
            if (!pReservation->GetPedUsingComponent() || pReservation->GetPedUsingComponent() == pPed)
            {
                if(!pReservation->GetPedUsingComponent())
                {
                    if (!pReservation->ReserveUseOfComponent(pPed))
					{
						m_bReservationFailed = true;
						return true;
					}
                }

                m_bGranted = true;

                if (bIsASeat)
                {
                    // if a seat is granted we need to send back who is sitting in it, as the other machine may not be aware of this
                    if (pLocalOccupier)
                    {
                        if(pLocalOccupier->IsNetworkClone())
                        {
                            // if the local occupier is a clone the player requesting the component might be more
                            // up-to-date with the ped's seat state than we are - if this is the case tell them
                            // there is no-one in the seat
                            CTaskInVehicleSeatShuffle *pInVehicleShuffleTask = (CTaskInVehicleSeatShuffle *) pLocalOccupier->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);

                            if(pInVehicleShuffleTask && pInVehicleShuffleTask->GetTargetSeatIndex() != iSeatIndex)
                            {
                                pLocalOccupier = 0;
                            }
                        }

                        if(pLocalOccupier)
                        {                       
                            m_pedInSeatID = pLocalOccupier->GetNetworkObject()->GetObjectID();
                        }
                    }
                }
            }
        }
        else
        {
            // a component is being freed
            if (bIsASeat)
            {
                // a seat is being freed - check that the ped in the seat on our machine corresponds with the other machine
                CPed* pRemoteOccupier = NULL;

                if (m_pedInSeatID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);

                    if (!pNetObj)
                    {
                        // the ped in the seat on the other machine has not been cloned on this machine
                        m_bPedInSeatWrong = true;
                        return true;
                    }

                    pRemoteOccupier = NetworkUtils::GetPedFromNetworkObject(pNetObj);

                    if (!pRemoteOccupier)
                    {
                        gnetAssertf(0, "VehicleComponentControlEvent : %s is not a ped!", pNetObj->GetLogName());
                    }
                }

                if (pLocalOccupier != pRemoteOccupier)
                {
                    if (pLocalOccupier && pRemoteOccupier && pRemoteOccupier == pReservation->GetPedUsingComponent())
                    {
                        if (pLocalOccupier->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
                        {
                            m_bPedStillLeaving = true;
                            return true;
                        }

                        // force our ped out as the owner of the seat is now in the car
                        pLocalOccupier->SetPedOutOfVehicle(CPed::PVF_Warp);
                    }
                    else
                    {
                        // our ped in the seat differs from the other machine, so do not grant the free request until this is rectified
                        m_bPedInSeatWrong = true;
                        return true;
                    }
                }
            }

            if (pReservation->GetPedUsingComponent() == pPed)
            {
                if (!pReservation->UnreserveUseOfComponent(pPed))
				{
					m_bReservationFailed = true;
					return true;
				}
            }
			else
			{
				m_bReservationFailed = true;
			}
	
	        m_bGranted = true;
        }
    }

    return true;
}

void CVehicleComponentControlEvent::PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    if (m_bGranted)
    {
		//Grab the reservation mgr.
		CComponentReservationManager* pReservationMgr = GetComponentReservationMgr();
		if(pReservationMgr)
		{
			CComponentReservation *pReservation = pReservationMgr->FindComponentReservationByArrayIndex(m_componentIndex);
			gnetAssert(pReservation);

			int iSeatIndex = GetModelSeatInfo()->GetSeatFromBoneIndex(pReservation->GetBoneIndex());
			m_bComponentIsASeat = iSeatIndex > -1;
        }
    }

    SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CVehicleComponentControlEvent::HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	//Grab the reservation mgr.
	CComponentReservationManager* pReservationMgr = GetComponentReservationMgr();
	
	//Grab the ped.
    netObject *pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    CPed      *pPed    = NetworkUtils::GetPedFromNetworkObject(pPedObj);

    m_pedInSeatID = NETWORK_INVALID_OBJECT_ID;

    SerialiseEventReply<CSyncDataReader>(messageBuffer);

    if(m_bRequest && m_bGranted && (!pReservationMgr || !pPed))
    {
        // if the ped or vehicle have been deleted, free up the component again
        CVehicleComponentControlEvent::Trigger(m_pedID, m_vehicleID, m_componentIndex, false, &fromPlayer);
    }
    else
    {
        CComponentReservation *pReservation = NULL;

        if(pReservationMgr)
        {
            pReservation = pReservationMgr->FindComponentReservationByArrayIndex(m_componentIndex);
            gnetAssert(pReservation);
        }

        if(pReservation)
        {
            if (pPed)
                pReservation->ReceiveRequestResponse(pPed, m_bRequest, m_bGranted);

            int iSeatIndex = GetModelSeatInfo()->GetSeatFromBoneIndex(pReservation->GetBoneIndex());
            bool bComponentIsASeat = iSeatIndex > -1;

            gnetAssert(!m_bGranted || m_bComponentIsASeat == bComponentIsASeat);

            if (m_bRequest && m_bGranted && bComponentIsASeat)
            {
                // check that the ped in the seat on our machine corresponds with the other machine
                CPed* pLocalPedInSeat = GetSeatManager()->GetPedInSeat(iSeatIndex);
                CPed* pRemotePedInSeat = NULL;

                if (m_pedInSeatID != NETWORK_INVALID_OBJECT_ID)
                {
                    netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);

                    if (!pNetObj)
                    {
                        m_bPedInSeatWrong = true;
                    }
                    else
                    {
                        CPed *pedInSeat = NetworkUtils::GetPedFromNetworkObject(pNetObj);

                        if (!pedInSeat && pNetObj->GetEntity())
                        {
                            gnetAssertf(0, "VehicleComponentControlEvent reply: %s is not a ped!", pNetObj->GetLogName());
                        }

                        pRemotePedInSeat = pedInSeat;
                    }
                }

                if (pLocalPedInSeat != pRemotePedInSeat)
                {
                    m_bPedInSeatWrong = true;

                    // tell the ped that requested the component about this: if the ped is entering a vehicle it needs to wait until the occupier
                    // of the seat is correct. Otherwise it may try and jack a ped who is not really in the seat, etc.
                    CTaskEnterVehicle         *pEnterVehicleTask     = (CTaskEnterVehicle*) pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_ENTER_VEHICLE);
                    CTaskInVehicleSeatShuffle *pInVehicleShuffleTask = (CTaskInVehicleSeatShuffle *) pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);

                    if (pEnterVehicleTask || pInVehicleShuffleTask)
                    {
                        bool makeTaskWait = true;
                        // if we think there is no-one in the seat and the ped requesting the seat is
                        // running an enter vehicle task, and the remote machine thinks the ped is already in the seat,
                        // we don't need the task to wait
                        if(pLocalPedInSeat == 0 && pRemotePedInSeat == pPed)
                        {
                            makeTaskWait = false;
                        }

                        if(makeTaskWait)
                        {
                            if (pRemotePedInSeat)
                            {
                                if(pEnterVehicleTask)
                                {
                                    pEnterVehicleTask->SetWaitForSeatToBeOccupied(true);
                                }

                                if(pInVehicleShuffleTask)
                                {
                                    pInVehicleShuffleTask->SetWaitForSeatToBeOccupied(true);
                                }
                            }
                            else
                            {
                                if(pEnterVehicleTask)
                                {
                                    pEnterVehicleTask->SetWaitForSeatToBeEmpty(true);
                                }

                                if(pInVehicleShuffleTask)
                                {
                                    pInVehicleShuffleTask->SetWaitForSeatToBeEmpty(true);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

#if ENABLE_NETWORK_LOGGING

void CVehicleComponentControlEvent::WriteEventToLogFile(bool LOGGING_ONLY(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    // The vehicle whose door we are changing control of
    netObject* vehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
    // Vehicle Name
    if (vehicleObj)
    {
        log.WriteDataValue("Vehicle", "%s", vehicleObj->GetLogName());
    }
    else
    {
        log.WriteDataValue("Vehicle", "?_%d", m_vehicleID);
    }

	log.WriteDataValue("Component", "%d", m_componentIndex);

   // The ped trying to enter the vehicle
    const netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);
    if (pPedObj)
    {
        log.WriteDataValue("Ped trying to enter", "%s", pPedObj->GetLogName());
    }
    else
    {
        log.WriteDataValue("Ped trying to enter", "?_%d", m_pedID);
    }

	// The component on the vehicle
    CVehicle * vehicle = NetworkUtils::GetVehicleFromNetworkObject(vehicleObj);
    CComponentReservation* reservation = vehicle ? vehicle->GetComponentReservationMgr()->FindComponentReservationByArrayIndex(m_componentIndex) : 0;
    // The ped sitting in a seat that is granted
    if (wasSent && reservation && reservation->IsComponentInUse())
    {
        netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);
        if (NETWORK_INVALID_OBJECT_ID == m_pedInSeatID)
            log.WriteDataValue("Seat occupier", "None");
        else if (pNetObj)
            log.WriteDataValue("Seat occupier", "%s", pNetObj->GetLogName());
        else
        {
            log.WriteDataValue("Seat occupier", "?_%d", m_pedInSeatID);
        }
    }

    // The player which owns the vehicle
    CNetGamePlayer* player = m_pOwnerPlayer ? NetworkInterface::GetPhysicalPlayerFromIndex(m_pOwnerPlayer->GetPhysicalPlayerIndex()) : NULL;
    if (player)
        log.WriteDataValue("Vehicle Owner", "%s", player->GetLogName());

    // If true control is requested, if false it is being released
    log.WriteDataValue("m_bRequest", "%s", m_bRequest ? "True" : "False");

    // Set if the component is a seat
    log.WriteDataValue("m_bComponentIsASeat", "%s", m_bComponentIsASeat ? "True" : "False");
}

void CVehicleComponentControlEvent::WriteDecisionToLogFile() const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    // The vehicle whose door we are changing control of
    netObject* vehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
    // The component on the vehicle
    CVehicle * vehicle = NetworkUtils::GetVehicleFromNetworkObject(vehicleObj);
    CComponentReservation* reservation = vehicle ? vehicle->GetComponentReservationMgr()->FindComponentReservationByArrayIndex(m_componentIndex) : 0;
    // The ped trying to enter the vehicle
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    if (m_bGranted)
    {
        // Control of the component is granted
        log.WriteDataValue("m_bGranted", "True");
        // Control of the seat is also granted if this request is for a door (this isn't actually used anywhere besides logging so it's gone.
        // log.WriteDataValue("m_bSeatAlsoGranted", "%s", m_bSeatAlsoGranted ? "True" : "False");
    }
    else
    {
        // Control of the component is granted
        log.WriteDataValue("m_bGranted", "False");

        if (!vehicleObj && !pPedObj)
        {
            log.WriteDataValue("Reason", "Vehicle and ped do not exist.");
        }
        else if (!vehicleObj)
        {
            log.WriteDataValue("Reason", "Vehicle does not exist.");
        }
        else if (!vehicle)
        {
            log.WriteDataValue("Reason", "There is no automobile game object.");
        }
        else if (vehicleObj->IsClone())
        {
            log.WriteDataValue("Reason", "Vehicle is not locally owned.");
        }
        else if (vehicleObj->IsPendingOwnerChange())
        {
            log.WriteDataValue("Reason", "Vehicle is pending ownership change.");
        }
        else if (!pPedObj)
        {
            log.WriteDataValue("Reason", "Ped does not exist.");
        }
        else if (!pPedObj->GetEntity())
        {
            log.WriteDataValue("Reason", "There is no ped game object.");
        }
        else if (vehicle->m_nVehicleFlags.bUsingPretendOccupants)
        {
            log.WriteDataValue("Reason", "The vehicle has pretend occupants.");
        }
        // set if we think ped is owned by a different machine
        else if (m_bPedOwnerWrong)
        {
            if (pPedObj->GetPlayerOwner())
                log.WriteDataValue("Reason", "Ped is owned by %s.", pPedObj->GetPlayerOwner()->GetLogName());
            else
                log.WriteDataValue("Reason", "Ped is being reassigned.");
        }
        // set if the ped in the seat has not been cloned on the machine requesting the component
        else if (m_bPedInSeatNotCloned)
        {
            log.WriteDataValue("Reason", "The occupier of the seat has not been cloned on the requester's machine.");
        }
        // set if the ped we think is in a seat differs from the owner machine
        else if (m_bPedInSeatWrong)
        {
            CPed* pLocalPedInSeat = NULL;
            CPed* pRemotePedInSeat = NULL;
            bool  bNotCloned = false;

            // Seat occupier
            if(reservation && reservation->IsComponentInUse() && vehicle)
            {
                int iSeatIndex = vehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetSeatFromBoneIndex(reservation->GetBoneIndex());
                if (iSeatIndex > -1)
                {
                    pLocalPedInSeat = vehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);
                }
            }

            if(m_pedInSeatID != NETWORK_INVALID_OBJECT_ID)
            {
                netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);
                if (!pNetObj)
                {
                    log.WriteDataValue("Reason", "?_%d in the seat on the remote machine has not been cloned on this machine.", m_pedInSeatID);
                    bNotCloned = true;
                }
                else
                {
                    pRemotePedInSeat = NetworkUtils::GetPedFromNetworkObject(pNetObj);
                }
            }

            if (!bNotCloned)
            {
                if (pLocalPedInSeat && !pRemotePedInSeat)
                {
                    log.WriteDataValue("Reason", "The seat should be empty but we still have %s in it", pLocalPedInSeat->GetNetworkObject()  ? pLocalPedInSeat->GetNetworkObject()->GetLogName()  : "a non-networked ped");
                }
                else if (pRemotePedInSeat && !pLocalPedInSeat)
                {
                    log.WriteDataValue("Reason", "The seat should have %s in it but we think it is empty", pRemotePedInSeat->GetNetworkObject() ? pRemotePedInSeat->GetNetworkObject()->GetLogName() : "a non-networked ped");
                }
                else if(pLocalPedInSeat && pRemotePedInSeat)
                {
                    const char* localPedName  = pLocalPedInSeat->GetNetworkObject()  ? pLocalPedInSeat->GetNetworkObject()->GetLogName()  : "a non-networked ped";
                    const char* remotePedName = pRemotePedInSeat->GetNetworkObject() ? pRemotePedInSeat->GetNetworkObject()->GetLogName() : "a non-networked ped";
                    log.WriteDataValue("Reason", "The occupier of the seat differs (our ped: %s, remote ped: %s)", localPedName, remotePedName);
                }
            }
        }
        // set if the ped in the seat is still leaving
        else if (m_bPedStillLeaving)
        {
            log.WriteDataValue("Reason", "The occupant of the seat is still leaving");
        }
		else if (m_bPedStillVaulting)
		{
			log.WriteDataValue("Reason", "The ped is still vaulting");
		}
		else if (m_bReservationFailed)
		{
			log.WriteDataValue("Reason", "The reservation/unreservation failed");
		}
        else
        {
            CPed* pedUsingComponent = reservation ? reservation->GetPedUsingComponent() : 0;
            gnetAssert(pedUsingComponent);
            netObject* pPedObj = pedUsingComponent ? pedUsingComponent->GetNetworkObject() : 0;
            log.WriteDataValue("Reason", "%s is already in control of component", pPedObj ? pPedObj->GetLogName() : "a non-networked ped");
        }
    }
}

void CVehicleComponentControlEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
    netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

    // The vehicle whose door we are changing control of
    netObject* vehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
    // The ped trying to enter the vehicle
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedID);

    // If true control is requested, if false it is being released
    //log.WriteDataValue("m_bRequest", "%s", m_bRequest ? "True" : "False");
    // Control of the component is granted
    log.WriteDataValue("m_bGranted", "%s", m_bGranted ? "True" : "False");

    if (vehicleObj)
        log.WriteDataValue("Vehicle", "%s", vehicleObj->GetLogName());

    // The component on the vehicle
    CVehicle * vehicle = NetworkUtils::GetVehicleFromNetworkObject(vehicleObj);
    CComponentReservation* reservation = vehicle ? vehicle->GetComponentReservationMgr()->FindComponentReservationByArrayIndex(m_componentIndex) : 0;
    // The ped sitting in a seat that is granted
    if (m_bGranted && NETWORK_INVALID_OBJECT_ID != m_pedInSeatID)
    {
        netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);
        if (pNetObj)
        {
            log.WriteDataValue("Seat occupier", "%s", pNetObj->GetLogName());
        }
        else
        {
            log.WriteDataValue("Seat occupier", "?_%d", m_pedInSeatID);
        }
    }

    if (m_bRequest && m_bGranted)
    {
        if (!vehicle)
            log.WriteDataValue("Error", "Vehicle does not exist - component freed again");

        CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pPedObj);
        if (!pPed)
            log.WriteDataValue("Error", "Ped does not exist - component freed again");

        if (vehicle && m_bPedInSeatWrong)
        {
            log.WriteDataValue("WARNING", "");
            log.WriteDataValue("m_bPedInSeatWrong", "True");

            // Local Seat occupier
            CPed* pLocalPedInSeat = NULL;
            if(reservation && reservation->IsComponentInUse() && vehicle)
            {
                int iSeatIndex = vehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetSeatFromBoneIndex(reservation->GetBoneIndex());
                if (iSeatIndex > -1)
                {
                    pLocalPedInSeat = vehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);
                }
            }

            // Remote Seat occupier
            CPed* pRemotePedInSeat = NULL;
            if (m_pedInSeatID != NETWORK_INVALID_OBJECT_ID)
            {
                netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedInSeatID);
                if (pNetObj)
                {
                    pRemotePedInSeat = NetworkUtils::GetPedFromNetworkObject(pNetObj);
                }
                else
                {
                    log.WriteDataValue("Seat occupier", "?_%d in the seat has not been cloned on this machine", m_pedInSeatID);
                }
            }

			if (pRemotePedInSeat && pLocalPedInSeat)
			{
				const char* localPedName  = pLocalPedInSeat->GetNetworkObject()  ? pLocalPedInSeat->GetNetworkObject()->GetLogName()  : "a non-networked ped";
				const char* remotePedName = pRemotePedInSeat->GetNetworkObject() ? pRemotePedInSeat->GetNetworkObject()->GetLogName() : "a non-networked ped";
				log.WriteDataValue("Reason", "The occupier of the seat differs (our ped: %s, remote ped: %s)", localPedName, remotePedName);
			}
			else if (pLocalPedInSeat)
            {
                const char* localPedName  = pLocalPedInSeat->GetNetworkObject()  ? pLocalPedInSeat->GetNetworkObject()->GetLogName()  : "a non-networked ped";
                log.WriteDataValue("Reason", "The seat should be empty but we still have %s in it", localPedName);
            }
            else if (pRemotePedInSeat)
            {
                const char* remotePedName = pRemotePedInSeat->GetNetworkObject() ? pRemotePedInSeat->GetNetworkObject()->GetLogName() : "a non-networked ped";
                log.WriteDataValue("Reason", "The seat should have %s in it but we think it is empty", remotePedName);
            }
        }
    }
}

#endif // ENABLE_NETWORK_LOGGING

bool CVehicleComponentControlEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == VEHICLE_COMPONENT_CONTROL_EVENT)
    {
        const CVehicleComponentControlEvent *pVehicleComponentRequestEvent = SafeCast(const CVehicleComponentControlEvent, pEvent);

        if (pVehicleComponentRequestEvent->m_pedID == m_pedID &&
            pVehicleComponentRequestEvent->m_vehicleID == m_vehicleID &&
            pVehicleComponentRequestEvent->m_componentIndex == m_componentIndex &&
            pVehicleComponentRequestEvent->m_bComponentIsASeat == m_bComponentIsASeat)
        {
            return true;
        }
    }

    return false;
}

CComponentReservationManager* CVehicleComponentControlEvent::GetComponentReservationMgr() const
{
	//Grab the net object.
	netObject* pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
	if(!pNetObject)
		return NULL;
		
	//Grab the vehicle.
	CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);
	if(pVehicle != NULL)
		return pVehicle->GetComponentReservationMgr();
		
	//Grab the ped.
	CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pNetObject);
	if(pPed != NULL)
		return pPed->GetComponentReservationMgr();
		
	return NULL;
}

const CModelSeatInfo* CVehicleComponentControlEvent::GetModelSeatInfo() const
{
	//Grab the net object.
	netObject* pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
	if(!pNetObject)
		return NULL;

	//Grab the vehicle.
	CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);
	if(pVehicle != NULL)
		return pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();

	//Grab the ped.
	CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pNetObject);
	if(pPed != NULL)
		return pPed->GetPedModelInfo()->GetModelSeatInfo();

	return NULL;
}

const CSeatManager* CVehicleComponentControlEvent::GetSeatManager() const
{
	//Grab the net object.
	netObject* pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
	if(!pNetObject)
		return NULL;

	//Grab the vehicle.
	CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);
	if(pVehicle != NULL)
		return pVehicle->GetSeatManager();

	//Grab the ped.
	CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pNetObject);
	if(pPed != NULL)
		return pPed->GetSeatManager();

	return NULL;
}

// ================================================================================================================
// FIRE EVENT
// ================================================================================================================
const float CFireEvent::FIRE_CLONE_RANGE_SQ = 200.0f * 200.0f;
const float CFireEvent::MAX_BURN_STRENGTH = 25.0f;
const float CFireEvent::MAX_BURN_TIME     = 90.0f; // current maximum burn time is WRECKED_SMOKE_DURATION_MAX_CAR * 2.0f (from the fires code)

static bool FireTypeRequiresBurningEntity(FireType_e fireType)
{
	switch(fireType)
	{
	case FIRETYPE_TIMED_MAP:
	case FIRETYPE_TIMED_PETROL_TRAIL:
	case FIRETYPE_TIMED_PETROL_POOL:
		return false;
		break;
	default:
		return true;
	}
}

CFireEvent::CFireEvent() :
netGameEvent(FIRE_EVENT, false, false),
m_numFires(0),
m_playersInScope(0),
m_fireOwner(false),
m_TimedOut(false)
{
}

void CFireEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CFireEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CFireEvent::Trigger(CFire& fire)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
	{
		const ObjectId burningEntityID = fire.GetEntity()  ? NetworkUtils::GetObjectIDFromGameObject(fire.GetEntity())  : NETWORK_INVALID_OBJECT_ID;
		const ObjectId ownerID         = fire.GetCulprit() ? NetworkUtils::GetObjectIDFromGameObject(fire.GetCulprit()) : NETWORK_INVALID_OBJECT_ID;

		Vec3V vParentPos = fire.GetParentPositionWorld();

		Vec3V vFirePos = (burningEntityID != NETWORK_INVALID_OBJECT_ID) ? fire.GetPositionLocal() : fire.GetPositionWorld();
		sFireData fireData(RCC_VECTOR3(vFirePos),
			RCC_VECTOR3(vParentPos),
			fire.GetFireType(),
			burningEntityID,
			ownerID,
			fire.GetFlag(FIRE_FLAG_IS_SCRIPTED),
			fire.GetNumGenerations(),
			fire.GetBurnTime(),
			fire.GetBurnStrength(),
			fire.GetNetworkIdentifier(),
			fire.GetWeaponHash());

		gnetAssertf(!FireTypeRequiresBurningEntity(fire.GetFireType()) ||
			burningEntityID    != NETWORK_INVALID_OBJECT_ID,
			"Starting an unexpected fire type without a burning entity! (Fire type: %d)", fire.GetFireType());

		gnetAssertf(fire.GetNumGenerations() >= 0, "A fire is being created with a negative number of generations! (%d)", fire.GetNumGenerations());
		gnetAssertf(fire.GetNumGenerations() <= FIRE_DEFAULT_NUM_GENERATIONS, "A fire is being created with too many generations! (%d)", fire.GetNumGenerations());

		// check all of the events in the pool to find any existing fire events this can be packed into
		CFireEvent *eventToUse = 0;

		atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

		while (node)
		{
			netGameEvent *networkEvent = node->Data;

			if(networkEvent && networkEvent->GetEventType() == FIRE_EVENT  && !networkEvent->IsFlaggedForRemoval())
			{
				CFireEvent *fireEvent = static_cast<CFireEvent *>(networkEvent);

				if(fireEvent->CanAddFireData(fireData) && !fireEvent->HasBeenSent())
				{
					eventToUse = fireEvent;
				}
			}

			node = node->GetNext();
		}

		if(eventToUse)
		{
			eventToUse->AddFireData(fireData);
		}
		else
		{
			CFireEvent *pEvent = rage_new CFireEvent();
			pEvent->AddFireData(fireData);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
	else
	{
		gnetAssertf(0, "Warning - network event pool full, dumping request fire event");
	}
}

bool CFireEvent::CanAddFireData(const sFireData &fireData)
{
	bool canAddFireData = false;

	if(m_numFires < MAX_NUM_FIRE_DATA)
	{
		// we can only pack fire events together if the players in scope of the fire are the same
		if(m_numFires == 0 || (GetPlayersInScopeForFireData(fireData) == m_playersInScope))
		{
			canAddFireData = true;
		}
	}

	return canAddFireData;
}

void CFireEvent::AddFireData(const sFireData &fireData)
{
	gnetAssert(CanAddFireData(fireData));
	gnetAssert(fireData.m_burnTime     <= MAX_BURN_TIME);
	gnetAssert(fireData.m_burnStrength <= MAX_BURN_STRENGTH);

#if __ASSERT
	for (int i=0; i<m_numFires; i++)
	{
		if (m_fireData[i].m_netIdentifier == fireData.m_netIdentifier)
		{
			gnetAssertf(0, "Trying to add a fire to a request fire event when it already exists in the event's fire array");
			return;
		}
	}
#endif // __ASSERT

	if(m_numFires < MAX_NUM_FIRE_DATA)
	{
		m_fireData[m_numFires] = fireData;

		if(m_numFires == 0)
		{
			m_playersInScope = GetPlayersInScopeForFireData(fireData);
		}

		m_numFires++;
	}

	m_fireOwner = true;
}

bool CFireEvent::IsInScope( const netPlayer& player ) const
{
	return (m_playersInScope & (1<<player.GetPhysicalPlayerIndex())) != 0;
}
bool CFireEvent::HasTimedOut() const
{
	return m_TimedOut;
}

template <class Serialiser> void CFireEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	// write the number of fires in this event
	SERIALISE_UNSIGNED(serialiser, m_numFires, SIZEOF_NUM_FIRE_DATA, "Number of fires");

    gnetAssertf(m_numFires <= MAX_NUM_FIRE_DATA, "Received a fire event with too much fire data!");
    m_numFires = MIN(m_numFires, MAX_NUM_FIRE_DATA);

	for(u32 index = 0; index < m_numFires; index++)
	{
		// write the fire type and entity or position first
		bool hasBurningEntity = m_fireData[index].m_burningEntityID != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_fireData[index].m_fireType), SIZEOF_FIRE_TYPE, "Fire Type");

		SERIALISE_BOOL(serialiser, hasBurningEntity);

		if(hasBurningEntity)
		{
			SERIALISE_OBJECTID(serialiser, m_fireData[index].m_burningEntityID, "Burning Entity ID");
		}

		bool bParentPosition = m_fireData[index].m_parentPosition.IsNonZero();
		SERIALISE_BOOL(serialiser, bParentPosition);
		if (bParentPosition)
		{
			SERIALISE_POSITION(serialiser, m_fireData[index].m_parentPosition, "Parent Position");
		}
		else
		{
			m_fireData[index].m_parentPosition = VEC3_ZERO;
		}
		
		SERIALISE_POSITION(serialiser, m_fireData[index].m_firePosition, "Fire Position");
		SERIALISE_OBJECTID(serialiser, m_fireData[index].m_ownerID, "Owner ID");
		SERIALISE_BOOL(serialiser, m_fireData[index].m_isScripted, "Is Scripted");
		SERIALISE_UNSIGNED(serialiser, m_fireData[index].m_maxGenerations, SIZEOF_MAX_GENERATIONS, "Max Generations");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fireData[index].m_burnTime,     MAX_BURN_TIME,     16, "Burn Time");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fireData[index].m_burnStrength, MAX_BURN_STRENGTH, 16, "Burn Strength");

		bool hasWeaponHash = m_fireData[index].m_weaponHash != 0;
		SERIALISE_BOOL(serialiser, hasWeaponHash);
		if(hasWeaponHash)
		{
			SERIALISE_UNSIGNED(serialiser, m_fireData[index].m_weaponHash, 32, "Weapon Hash");
		}
		else
		{
			m_fireData[index].m_weaponHash = 0;
		}

		m_fireData[index].m_netIdentifier.Serialise<Serialiser>(messageBuffer);
	}
}

void CFireEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CFireEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CFireEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	for(u32 index = 0; index < m_numFires; index++)
	{
		// ignore fires in cutscenes or from players in other tutorial sessions
		if (!NetworkInterface::IsInMPCutscene() && !SafeCast(const CNetGamePlayer, &fromPlayer)->IsInDifferentTutorialSession())
		{
			// get the network objects from the read object IDs
			netObject *pNetObjBurningEntity = 0;
			netObject *pNetObjCulprit       = 0;

			if(m_fireData[index].m_burningEntityID != NETWORK_INVALID_OBJECT_ID)
			{
				pNetObjBurningEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_fireData[index].m_burningEntityID);
			}

			if(m_fireData[index].m_ownerID != NETWORK_INVALID_OBJECT_ID)
			{
				pNetObjCulprit = NetworkInterface::GetObjectManager().GetNetworkObject(m_fireData[index].m_ownerID);
			}

			// get the entities from the network objects
			CEntity *pBurningEntity = pNetObjBurningEntity ? pNetObjBurningEntity->GetEntity() : 0;
			CPed *pCulprit          = NetworkUtils::GetPedFromNetworkObject(pNetObjCulprit);

			m_fireData[index].m_netIdentifier.SetPlayerOwner(fromPlayer.GetPhysicalPlayerIndex());

			CFireSettings fireSettings;
			fireSettings.pCulprit = pCulprit;
			fireSettings.fireType = m_fireData[index].m_fireType;
			fireSettings.parentFireType = m_fireData[index].m_fireType; //just say parent is the same as the current fire rather than leaving it blank - fixes issues of orientation for remote petrol fires. See CFire::GetMatrixOffset. lavalley
			fireSettings.burnTime = m_fireData[index].m_burnTime;
			fireSettings.burnStrength = m_fireData[index].m_burnStrength;
			fireSettings.peakTime = 0.0f;
			fireSettings.numGenerations = m_fireData[index].m_maxGenerations;
			fireSettings.pNetIdentifier = &m_fireData[index].m_netIdentifier;
			fireSettings.weaponHash = m_fireData[index].m_weaponHash;
			fireSettings.pEntity = 0;

			bool bStartFire = false;

			if(m_fireData[index].m_burningEntityID != NETWORK_INVALID_OBJECT_ID)
			{
				if (pBurningEntity)
				{
					fireSettings.pEntity = pBurningEntity;
					bStartFire = true;
				}
			}
			else if (gnetVerifyf(!FireTypeRequiresBurningEntity(fireSettings.fireType), "Trying to start a fire without a burning entity when it is required!"))
			{
				bStartFire = true;
			}

			fireSettings.vPosLcl = RCC_VEC3V(m_fireData[index].m_firePosition);

			fireSettings.vParentPos = RCC_VEC3V(m_fireData[index].m_parentPosition);

			if (bStartFire)
			{
				CFire* pFire = g_fireMan.StartFire(fireSettings);

				if (pFire)
				{
					// possibly hook up the fire with an existing fire incident
					CIncidentManager::GetInstance().FindFireIncidentForFire(pFire);
				}
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CFireEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Local Fire", "%s", m_fireOwner ? "true" : "false");
	log.LineBreak();

	for(u32 index = 0; index < m_numFires; index++)
	{
		WriteDataToLogFile(index, bEventLogOnly);
	}
}

void CFireEvent::WriteDataToLogFile(u32 LOGGING_ONLY(fireIndex), bool LOGGING_ONLY(bEventLogOnly)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Fire", "%d", fireIndex);
	log.WriteDataValue("Identifier", "%d:%d", m_fireData[fireIndex].m_netIdentifier.GetPlayerOwner(), m_fireData[fireIndex].m_netIdentifier.GetFXId());

	if(m_fireData[fireIndex].m_ownerID != NETWORK_INVALID_OBJECT_ID)
	{
		netObject* fireOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_fireData[fireIndex].m_ownerID, true);
		if (fireOwner)
		{
			log.WriteDataValue("Owner", "%s", fireOwner->GetLogName());
		}
	}

	if(m_fireData[fireIndex].m_burningEntityID != NETWORK_INVALID_OBJECT_ID)
	{
		netObject* burningEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_fireData[fireIndex].m_burningEntityID, true);
		if (burningEntity)
		{
			log.WriteDataValue("Burning entity", "%s", burningEntity->GetLogName());
		}
	}
	else
	{
		log.WriteDataValue("At position", "%f, %f, %f"
			,m_fireData[fireIndex].m_firePosition.x
			,m_fireData[fireIndex].m_firePosition.y
			,m_fireData[fireIndex].m_firePosition.z);
	}

	log.WriteDataValue("Is scripted", "%s", m_fireData[fireIndex].m_isScripted ? "true" : "false");
	log.WriteDataValue("Type", "%d", m_fireData[fireIndex].m_fireType);
	log.WriteDataValue("Max generations", "%d", m_fireData[fireIndex].m_maxGenerations);
	log.WriteDataValue("Burn Time", "%f", m_fireData[fireIndex].m_burnTime);
	log.WriteDataValue("Burn Strength", "%f", m_fireData[fireIndex].m_burnStrength);
	log.WriteDataValue("Weapon Hash", "%d", m_fireData[fireIndex].m_weaponHash);
	log.LineBreak();
}

#endif // ENABLE_NETWORK_LOGGING

PlayerFlags CFireEvent::GetPlayersInScopeForFireData(const sFireData &fireData)
{
	PlayerFlags playerFlags = 0;

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		if(pPlayer->GetPlayerPed())
		{
			// send to all players
			bool bInScope = true;

			// don't send to players in a cutscene
			if (pPlayer->GetPlayerInfo() && pPlayer->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
			{
				bInScope = false;
			}
			else if(fireData.m_burningEntityID != NETWORK_INVALID_OBJECT_ID)
			{
				// don't send to players that don't know about the burning entity yet
				netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(fireData.m_burningEntityID);

				if(networkObject && !networkObject->IsClone() && !networkObject->HasBeenCloned(*pPlayer))
				{
					bInScope = false;
				}
			}
			else
			{
				if(fireData.m_fireType == FIRETYPE_TIMED_OBJ_GENERIC)
				{
					// don't request a network fire for objects that have not been registered with the network
					bInScope = false;
				}
				else
				{
					// don't send to players far away from the fire
					const Vec3V vCameraPosition = VECTOR3_TO_VEC3V(NetworkInterface::GetPlayerFocusPosition(*pPlayer));
					float distanceFromFireSq = DistSquared(vCameraPosition, RCC_VEC3V(fireData.m_firePosition)).Getf();

					bInScope = (distanceFromFireSq <= FIRE_CLONE_RANGE_SQ);
				}
			}

			if(bInScope)
			{
				playerFlags |= (1<<pPlayer->GetPhysicalPlayerIndex());
			}
		}
	}

	return playerFlags;
}

// ===========================================================================================================
// EXPLOSION EVENT
// ===========================================================================================================

const float CExplosionEvent::EXPLOSION_CLONE_RANGE_SQ					= 350.0f * 350.0f;
const float CExplosionEvent::EXPLOSION_CLONE_RANGE_ENTITY_SQ			= 700.0f * 700.0f;
const float CExplosionEvent::EXPLOSION_CLONE_RANGE_SNIPER_SQ			= 700.0f * 700.0f;
const float CExplosionEvent::EXPLOSION_CLONE_RANGE_SNIPER_ENTITY_SQ     = 1500.0f * 1500.0f;

#if REGREF_VALIDATE_CDCDCDCD
namespace rage
{
	XPARAM(RegrefValidation);
}
#endif

CExplosionEvent::CExplosionEvent() :
netGameEvent( EXPLOSION_EVENT, true ),
m_explosionArgs(EXP_TAG_DONTCARE, Vector3(0.0f, 0.0f, 0.0f)),
m_explodingEntityID  (NETWORK_INVALID_OBJECT_ID),
m_entExplosionOwnerID(NETWORK_INVALID_OBJECT_ID),
m_entIgnoreDamageID  (NETWORK_INVALID_OBJECT_ID),
m_attachEntityID     (NETWORK_INVALID_OBJECT_ID),
m_dummyPosition		 (VECTOR3_ZERO),
m_bHasProjectile     (false),
m_hasRelatedDummy    (false),
m_bEntityExploded    (false),
m_success            (false),
m_shouldAttach	 (false),
m_SystemTimeAdded(sysTimer::GetSystemMsTime())
{
	// For debugging url:bugstar:1837138
#if REGREF_VALIDATE_CDCDCDCD
	if(PARAM_RegrefValidation.Get())
	{
		Displayf("CExplosionEvent %p default ctor\n", this);
	}
#endif
}

CExplosionEvent::CExplosionEvent(CExplosionManager::CExplosionArgs& explosionArgs, CProjectile* pProjectile) :
netGameEvent(EXPLOSION_EVENT, false),
m_explosionArgs(explosionArgs),
m_explodingEntityID  (NetworkUtils::GetObjectIDFromGameObject(explosionArgs.m_pExplodingEntity.Get())),
m_entExplosionOwnerID(NetworkUtils::GetObjectIDFromGameObject(explosionArgs.m_pEntExplosionOwner.Get())),
m_entIgnoreDamageID  (NetworkUtils::GetObjectIDFromGameObject(explosionArgs.m_pEntIgnoreDamage.Get())),
m_attachEntityID     (NetworkUtils::GetObjectIDFromGameObject(explosionArgs.m_pAttachEntity.Get())),
m_shouldAttach		 (explosionArgs.m_pAttachEntity.Get() != nullptr),
m_dummyPosition		 (VECTOR3_ZERO),
m_bHasProjectile     (pProjectile != NULL),
m_hasRelatedDummy    (explosionArgs.m_pRelatedDummyForNetwork != NULL),
m_bEntityExploded    (explosionArgs.m_pExplodingEntity.Get() ? explosionArgs.m_pExplodingEntity.Get()->m_nFlags.bHasExploded : 0),
m_success            (false),
m_SystemTimeAdded(sysTimer::GetSystemMsTime())
{
	// For debugging url:bugstar:1837138
#if REGREF_VALIDATE_CDCDCDCD
	if(PARAM_RegrefValidation.Get())
	{
		Displayf("CExplosionEvent %p constructed with args %p (%p)\n", this, &m_explosionArgs, &explosionArgs);
	}
#endif

	gnetAssert(m_explosionArgs.m_networkIdentifier.IsValid());
 
    if (pProjectile)
    {
        m_projectileIdentifier = pProjectile->GetNetworkIdentifier();
    }

	if (m_hasRelatedDummy)
	{
		m_dummyPosition = VEC3V_TO_VECTOR3(explosionArgs.m_pRelatedDummyForNetwork->GetTransform().GetPosition());
	}
	
    if(explosionArgs.m_pAttachEntity && NetworkUtils::GetNetworkObjectFromEntity(explosionArgs.m_pAttachEntity))
    {
        // if the explosion is attached we send a position relative to the attached object
        m_explosionArgs.m_explosionPosition -= VEC3V_TO_VECTOR3(explosionArgs.m_pAttachEntity->GetTransform().GetPosition());
    }
	else
	{
		gnetAssertf(Abs(explosionArgs.m_explosionPosition.x) > 10.0f || Abs(explosionArgs.m_explosionPosition.y) > 10.0f || Abs(explosionArgs.m_explosionPosition.z) > 10.0f, "CExplosionEvent: The explosion position is an offset");
	}
}

CExplosionEvent::~CExplosionEvent()
{
	// For debugging url:bugstar:1837138
#if REGREF_VALIDATE_CDCDCDCD
	if(PARAM_RegrefValidation.Get())
	{
		Displayf("CExplosionEvent %p dtor\n", this);
	}
#endif
}

#define EXPLOSION_VEHICLE_COMPENSATE_FRAME_DIST_TRIGGER_VELOCITY (25.0f)
#define EXPLOSION_VEHICLE_COMPENSATE_MULTIPLE (1.25f)    //Adjustment value guarantees damage to Lazer when at full speed and a sticky bomb detonated attached to its rearmost point 

void  CExplosionEvent::CompensateForVehicleVelocity(CEntity* pAttachEntity)
{
	if (pAttachEntity && pAttachEntity->GetIsTypeVehicle())
	{
		CVehicle *vehicle = static_cast<CVehicle*>(pAttachEntity);

		Vector3 vCentre = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
		Vector3 vHitPointToCentreDiff = m_explosionArgs.m_explosionPosition - vCentre;
		Vector3 vehicleVel = vehicle->GetVelocity();
		float fVelScalar = vehicleVel.Mag();

		vehicleVel.Normalize();
		vHitPointToCentreDiff.Normalize();

		float fDotHitPointToCentre = vHitPointToCentreDiff.Dot(vehicleVel); //negative if hit is behind the centre

		if( fDotHitPointToCentre < 0.0f &&
			fVelScalar > EXPLOSION_VEHICLE_COMPENSATE_FRAME_DIST_TRIGGER_VELOCITY)
		{
			//If vehicle is moving at a comparable rate to the explosion speed, 
			//and we know it was hit on remote machine (hence attachment being passed),   
			//then move the explosion position along the moving direction about a frames worth 
			//of moved distance to ensure that damage will be caused and vehicle does not
			//outrun explosion locally.	
			float distTravelledLastFrame = vehicle->GetDistanceTravelled();
			m_explosionArgs.m_explosionPosition += vehicleVel*distTravelledLastFrame*EXPLOSION_VEHICLE_COMPENSATE_MULTIPLE;
		}
	}
}

void CExplosionEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CExplosionEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CExplosionEvent::Trigger(CExplosionManager::CExplosionArgs& explosionArgs, CProjectile* pProjectile)
{
	weaponDebugf3("CExplosionEvent::Trigger");

    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    // we only support camera shakes of up to 1.0f over the network
    if(explosionArgs.m_fCamShake > 1.0f)
    {
        explosionArgs.m_fCamShake = 1.0f;
    }

	if(explosionArgs.m_vDirection.Mag() > 1.01f)
	{
		gnetAssertf(0,"explosionArgs.m_vDirection is too big for a normal x %f, y %f, z %f.",explosionArgs.m_vDirection.x, explosionArgs.m_vDirection.y, explosionArgs.m_vDirection.z);
		explosionArgs.m_vDirection.Normalize();
	}

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CExplosionEvent *pEvent = rage_new CExplosionEvent(explosionArgs, pProjectile);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CExplosionEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    m_explosionArgs.m_networkIdentifier.Serialise<Serialiser>(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_explodingEntityID, "Exploding Entity ID");
    SERIALISE_OBJECTID(serialiser, m_entExplosionOwnerID, "Explosion Owner ID");
    SERIALISE_OBJECTID(serialiser, m_entIgnoreDamageID, "Ignore Damage Entity ID");
    SERIALISE_INTEGER(serialiser, reinterpret_cast<s32&>(m_explosionArgs.m_explosionTag), datBitsNeeded<EEXPLOSIONTAG_MAX>::COUNT + 1, "Explosion Tag");
    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_explosionArgs.m_sizeScale, 1.0f, SIZEOF_SIZE_SCALE, "Explosion Scale");
    SERIALISE_POSITION_SIZE(serialiser, m_explosionArgs.m_explosionPosition, "Explosion Position", SIZEOF_EXPLOSION_POSITION);
	SERIALISE_BOOL(serialiser, m_hasRelatedDummy, "Has Related Dummy");
    SERIALISE_UNSIGNED(serialiser, m_explosionArgs.m_activationDelay,     SIZEOF_ACTIVATION_DELAY, "Activation Delay");
    SERIALISE_PACKED_FLOAT(serialiser, m_explosionArgs.m_fCamShake, 1.0f,     SIZEOF_CAM_SHAKE, "Cam Shake");
	SERIALISE_BOOL(serialiser, m_explosionArgs.m_bMakeSound, "Make Sound");
    SERIALISE_BOOL(serialiser, m_explosionArgs.m_bNoDamage, "No Damage");
    SERIALISE_BOOL(serialiser, m_explosionArgs.m_bNoFx, "No FX");
	SERIALISE_BOOL(serialiser, m_explosionArgs.m_bInAir, "In air");
	SERIALISE_BOOL(serialiser, m_bEntityExploded, "Exploding entity exploded");
	SERIALISE_BOOL(serialiser, m_shouldAttach, "Should attached to object");
	SERIALISE_OBJECTID(serialiser, m_attachEntityID, "Attach Entity ID");
	SERIALISE_VECTOR(serialiser, m_explosionArgs.m_vDirection, 1.1f, SIZEOF_EXPLOSION_DIRECTION, "Explosion direction");
	SERIALISE_BOOL(serialiser, m_explosionArgs.m_bAttachedToVehicle, "Attached to vehicle");
	SERIALISE_BOOL(serialiser, m_explosionArgs.m_bDetonatingOtherPlayersExplosive, "Detonating other players bomb");
	SERIALISE_UNSIGNED(serialiser, m_explosionArgs.m_weaponHash, sizeof(u32)<<3, "Weapon Hash");
    u32 interiorLoc = m_explosionArgs.m_interiorLocation.GetAsUint32();
    SERIALISE_UNSIGNED(serialiser, interiorLoc, SIZEOF_INTERIOR_LOC, "Interior location");
    m_explosionArgs.m_interiorLocation.SetAsUint32(interiorLoc);

	if (m_hasRelatedDummy)
	{
		static const unsigned int SIZEOF_DUMMY_POSITION = 31;
		SERIALISE_POSITION_SIZE(serialiser, m_dummyPosition, "Dummy Position", SIZEOF_DUMMY_POSITION);
	}

	SERIALISE_BOOL(serialiser, m_bHasProjectile);
		
	if (m_bHasProjectile)
	{
		m_projectileIdentifier.Serialise<Serialiser>(messageBuffer);

		if (m_explosionArgs.m_bDetonatingOtherPlayersExplosive)
		{
			PhysicalPlayerIndex otherProjectileOwnerPlayerIndex = m_projectileIdentifier.GetPlayerOwner();
			SERIALISE_UNSIGNED(serialiser, otherProjectileOwnerPlayerIndex, sizeof(PhysicalPlayerIndex)<<3, "Other player projectile owner index");
			m_projectileIdentifier.SetPlayerOwner(otherProjectileOwnerPlayerIndex);
			gnetAssert(otherProjectileOwnerPlayerIndex!= INVALID_PLAYER_INDEX);
		}
	}
}

bool CExplosionEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if(player.IsLocal())
	{
		return false;
	}

    if ((m_explodingEntityID != NETWORK_INVALID_OBJECT_ID && !m_explosionArgs.m_pExplodingEntity) ||
        (m_entExplosionOwnerID != NETWORK_INVALID_OBJECT_ID && !m_explosionArgs.m_pEntExplosionOwner) ||
		(m_entIgnoreDamageID != NETWORK_INVALID_OBJECT_ID && !m_explosionArgs.m_pEntIgnoreDamage) ||
        (m_attachEntityID != NETWORK_INVALID_OBJECT_ID && !m_explosionArgs.m_pAttachEntity))
    {
        // if any of the explosion entities have been deleted then we have to dump the event
        return false;
    }
	
	if (player.GetPlayerInfo() && player.GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
	{
		// ignore players in cutscenes
		return false;
	}
	
	if (SafeCast(const CNetGamePlayer, &playerToCheck)->IsInDifferentTutorialSession())
	{
		// ignore players in different tutorial sessions
		return false;
	}
	
	float explosionCloneRange = EXPLOSION_CLONE_RANGE_SQ;

	bool bHasExplodingEntity = false;

	if (GetExplodingEntityID() != NETWORK_INVALID_OBJECT_ID)
	{
		netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(GetExplodingEntityID());

		if(networkObject)
		{
			if (!networkObject->IsClone() && !m_hasRelatedDummy)
			{
				if (networkObject->HasBeenCloned(playerToCheck))
				{
					explosionCloneRange = EXPLOSION_CLONE_RANGE_ENTITY_SQ;

					bHasExplodingEntity = true;
				}
				else
				{
					return false;
				}
			}
			else if (networkObject->GetPlayerOwner() == &playerToCheck)
			{
				return true;
			}
		}
	}
    
	// don't send to players far away from the explosion
    if (player.GetPlayerPed())
    {
		CNetObjPlayer* playerNetObj = SafeCast(CNetObjPlayer, player.GetPlayerPed()->GetNetworkObject());
		CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		CNetObjPlayer* myPlayerNetObj = pLocalPlayer ? SafeCast(CNetObjPlayer, pLocalPlayer->GetNetworkObject()) : NULL;

		// if we are being spectated by this player, create all explosions on his machine
		if (playerNetObj && playerNetObj->IsSpectating() && playerNetObj->GetSpectatorObjectId() == myPlayerNetObj->GetObjectID())
		{
			return true;
		}

		Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(playerToCheck);

		gnetAssertf(!playerFocusPosition.IsClose(VEC3_ZERO, 0.1f),"Expect a valid focus position for player %s",playerToCheck.GetLogName());

		Vector3 explosionPosition = m_explosionArgs.m_explosionPosition;

        if(m_explosionArgs.m_pAttachEntity && NetworkUtils::GetNetworkObjectFromEntity(m_explosionArgs.m_pAttachEntity))
        {
            explosionPosition += VEC3V_TO_VECTOR3(m_explosionArgs.m_pAttachEntity->GetTransform().GetPosition());
        }

		float distanceFromExplosionSq = (playerFocusPosition - explosionPosition).Mag2();

		grcViewport *viewport = NetworkUtils::GetNetworkPlayerViewPort(static_cast<const CNetGamePlayer&>(playerToCheck));

		if (viewport && viewport->GetFOVY() < 100.f && explosionCloneRange < EXPLOSION_CLONE_RANGE_SNIPER_SQ)
		{
			if (bHasExplodingEntity)
			{
				explosionCloneRange = EXPLOSION_CLONE_RANGE_SNIPER_ENTITY_SQ;
			}
			else
			{
				explosionCloneRange = EXPLOSION_CLONE_RANGE_SNIPER_SQ;
			}
		}

		if (distanceFromExplosionSq <= explosionCloneRange)
		{
			return true;
		}
    }

    // if the explosion is attached to an entity, we must make sure the owner of the entity knows about the explosion. If the entity is failing to 
    // migrate and its owner is far away the vehicle will not be destroyed otherwise.
    if (m_explosionArgs.m_pAttachEntity)
    {
        netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(m_explosionArgs.m_pAttachEntity);

        if (pNetObj && pNetObj->IsClone() && pNetObj->GetPlayerOwner() == &playerToCheck)
        {
            return true;
        }
    }

    return false;
}

bool CExplosionEvent::HasTimedOut() const
{
	const unsigned EXPLOSION_EVENT_TIMEOUT = 500;

	u32 currentTime = sysTimer::GetSystemMsTime();

	// basic wrapping handling (this doesn't have to be precise, and the system time should never wrap)
	if(currentTime < m_SystemTimeAdded)
	{
		m_SystemTimeAdded = currentTime;
	}

	u32 timeElapsed = currentTime - m_SystemTimeAdded;

	return timeElapsed > EXPLOSION_EVENT_TIMEOUT;
}

void CExplosionEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CExplosionEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CExplosionEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	weaponDebugf3("CExplosionEvent::Decide");

    m_explosionArgs.m_networkIdentifier.SetPlayerOwner(fromPlayer.GetPhysicalPlayerIndex());

	const CNetGamePlayer* pGamePlayer = static_cast<const CNetGamePlayer*>(&fromPlayer);

	// ignore the explosions if we are in a cutscene or different tutorial session
	if (!NetworkInterface::IsInMPCutscene() && !pGamePlayer->IsInDifferentTutorialSession())
	{
		if (m_bHasProjectile)
		{
			if (!m_explosionArgs.m_bDetonatingOtherPlayersExplosive)
			{
				m_projectileIdentifier.SetPlayerOwner(fromPlayer.GetPhysicalPlayerIndex());
			}

			// remove the projectile which created this explosion
			CProjectileManager::RemoveProjectile(m_projectileIdentifier);

			CNetworkPendingProjectiles::RemoveProjectile(pGamePlayer->GetPlayerPed(), m_projectileIdentifier);
		}

		//the projectile doesn't exist so assume it has been exploded
		if (!(m_explosionArgs.m_bDetonatingOtherPlayersExplosive && !CProjectileManager::GetProjectile(m_projectileIdentifier)))
		{	
			m_success = CreateExplosion();

			if (!m_success && m_explosionArgs.m_pExplodingEntity && !m_explosionArgs.m_pExplodingEntity->GetCurrentPhysicsInst())
			{
				// reject the event if the exploding entity is waiting to stream in its phys inst
				return false;
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CExplosionEvent::WriteEventToLogFile(bool UNUSED_PARAM(bSet), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("Identifier", "%d:%d", m_explosionArgs.m_networkIdentifier.GetPlayerOwner(), m_explosionArgs.m_networkIdentifier.GetFXId());

    if(NETWORK_INVALID_OBJECT_ID != m_explodingEntityID)
    {
        netObject* explodingEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_explodingEntityID, true);
        if (explodingEntity)
        {
            log.WriteDataValue("Exploding entity", "%s", explodingEntity->GetLogName());
        }
    }

    if(NETWORK_INVALID_OBJECT_ID != m_entExplosionOwnerID)
    {
        netObject* explosionOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_entExplosionOwnerID, true);
        if (explosionOwner)
        {
            log.WriteDataValue("Owner", "%s", explosionOwner->GetLogName());
        }
    }

	if(NETWORK_INVALID_OBJECT_ID != m_entIgnoreDamageID)
	{
		netObject* ignoreDamageEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_entIgnoreDamageID, true);
		if (ignoreDamageEntity)
		{
			log.WriteDataValue("Ignore Damage Entity", "%s", ignoreDamageEntity->GetLogName());
		}
	}

    log.WriteDataValue("Explosion type", "%d", m_explosionArgs.m_explosionTag);
    log.WriteDataValue("Size Scale", "%f",  m_explosionArgs.m_sizeScale);

    Vector3 explosionPosition = m_explosionArgs.m_explosionPosition;
    log.WriteDataValue("Explosion position", "%f, %f, %f", explosionPosition.x, explosionPosition.y, explosionPosition.z);

	log.WriteDataValue("Is dummy object explosion", "%s",  m_hasRelatedDummy ? "true" : "false");
    log.WriteDataValue("Activation delay", "%d", m_explosionArgs.m_activationDelay);
    log.WriteDataValue("Make sound", "%s",  m_explosionArgs.m_bMakeSound ? "true" : "false");
    log.WriteDataValue("No damage", "%s",  m_explosionArgs.m_bNoDamage ? "true" : "false");
	log.WriteDataValue("No fx", "%s",  m_explosionArgs.m_bNoFx ? "true" : "false");
	log.WriteDataValue("Entity exploded", "%s",  m_bEntityExploded ? "true" : "false");
    log.WriteDataValue("Cam shake", "%f",  m_explosionArgs.m_fCamShake);
    log.WriteDataValue("Direction", "%f, %f, %f",  m_explosionArgs.m_vDirection.x, m_explosionArgs.m_vDirection.y, m_explosionArgs.m_vDirection.z);
    log.WriteDataValue("Attached to vehicle", "%s",  m_explosionArgs.m_bAttachedToVehicle ? "true" : "false");
    log.WriteDataValue("Detonating other players bomb", "%s",  m_explosionArgs.m_bDetonatingOtherPlayersExplosive ? "true" : "false");

	log.WriteDataValue("Weapon Hash", "%d",  m_explosionArgs.m_weaponHash);
    log.WriteDataValue("Interior location", "%u", m_explosionArgs.m_interiorLocation.GetAsUint32());

	log.WriteDataValue("Should Attach to Object", "%s", m_shouldAttach ? "TRUE" : "FALSE");
    if(NETWORK_INVALID_OBJECT_ID != m_attachEntityID)
    {
        netObject* attachEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_attachEntityID, true);
        if (attachEntity)
        {
            log.WriteDataValue("Attached to", "%s", attachEntity->GetLogName());
        }
    }

	if (m_hasRelatedDummy)
	{
		log.WriteDataValue("Dummy position" , "%f, %f, %f", m_dummyPosition.x, m_dummyPosition.y, m_dummyPosition.z);
	}

	log.WriteDataValue("Has projectile", "%s",  m_bHasProjectile ? "true" : "false");

	if (m_explosionArgs.m_bDetonatingOtherPlayersExplosive)
	{
		PhysicalPlayerIndex otherProjectileOwnerPlayerIndex  = m_projectileIdentifier.GetPlayerOwner();
		NetworkFXId			fxId							 = m_projectileIdentifier.GetFXId();
		gnetAssert(otherProjectileOwnerPlayerIndex!= INVALID_PLAYER_INDEX);
		gnetAssert(fxId != INVALID_NETWORK_FX_ID);
		log.WriteDataValue("Other Player Explosion Projectile Identifier", "%d:%d", otherProjectileOwnerPlayerIndex, fxId);
	}
	else if(m_bHasProjectile)
	{
		log.WriteDataValue("Explosion Projectile Identifier", "%d:%d", m_projectileIdentifier.GetPlayerOwner(), m_projectileIdentifier.GetFXId());
	}
}

void CExplosionEvent::WriteDecisionToLogFile() const
{
	netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Successfully created", "%s", m_success ? "true" : "false");
}

#endif // ENABLE_NETWORK_LOGGING

bool CExplosionEvent::CreateExplosion()
{
    // get the network objects from the read object IDs
    netObject *pNetObjExplodingEntity		= 0;
    netObject *pNetObjExplosionOwner		= 0;
    netObject *pNetObjIgnoreDamageEntity	= 0;
    netObject *pNetObjAttachEntity			= 0;

    bool bSuccess = true;

    if(m_explodingEntityID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjExplodingEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_explodingEntityID);
    }

    if(m_entExplosionOwnerID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjExplosionOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_entExplosionOwnerID);
    }

	if(m_entIgnoreDamageID != NETWORK_INVALID_OBJECT_ID)
	{
		pNetObjIgnoreDamageEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_entIgnoreDamageID);
	}

    if(m_attachEntityID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjAttachEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_attachEntityID);
    }

	if (m_bHasProjectile)
	{
		// remove the projectile which created this explosion
		CProjectileManager::RemoveProjectile(m_projectileIdentifier);
	}

    m_explosionArgs.m_vDirection.Normalize();

    // get the entities from the network objects
    m_explosionArgs.m_pExplodingEntity   = pNetObjExplodingEntity	 ? pNetObjExplodingEntity->GetEntity()	  : 0;
    m_explosionArgs.m_pEntExplosionOwner = pNetObjExplosionOwner	 ? pNetObjExplosionOwner->GetEntity()	  : 0;
    m_explosionArgs.m_pEntIgnoreDamage	 = pNetObjIgnoreDamageEntity ? pNetObjIgnoreDamageEntity->GetEntity() : 0;
    m_explosionArgs.m_pAttachEntity      = pNetObjAttachEntity		 ? pNetObjAttachEntity->GetEntity()		  : 0;
 
	if (m_hasRelatedDummy && !m_explosionArgs.m_pExplodingEntity)
	{
		// try to find the real object from the position of the specified dummy object
		m_explosionArgs.m_pExplodingEntity = GetObjectFromExplodingDummyPosition();
		
		// if explosion should attach but no attach object was synced, attach to the dummy object
		if(m_shouldAttach && m_attachEntityID == NETWORK_INVALID_OBJECT_ID)
		{
			m_explosionArgs.m_pAttachEntity = m_explosionArgs.m_pExplodingEntity;
		}
	}

	if (m_explosionArgs.m_pExplodingEntity && !m_explosionArgs.m_pExplodingEntity->GetCurrentPhysicsInst())
	{
		// can't explode entities with no phys inst
		bSuccess = false;
	}
	else if (m_bEntityExploded)
	{
		if (!m_explosionArgs.m_pExplodingEntity)
		{
			bSuccess = false;
		}
		else if (m_explosionArgs.m_pExplodingEntity->m_nFlags.bHasExploded)
		{
			bSuccess = true;
		}
		else
		{
			bSuccess = m_explosionArgs.m_pExplodingEntity->TryToExplode(m_explosionArgs.m_pEntExplosionOwner, false);
		}
	}
	else 
	{
        if(m_attachEntityID != NETWORK_INVALID_OBJECT_ID)
        {
            if(m_explosionArgs.m_pAttachEntity == 0)
            {
                bSuccess = false;
            }
            else
            {
                m_explosionArgs.m_explosionPosition += VEC3V_TO_VECTOR3(m_explosionArgs.m_pAttachEntity->GetTransform().GetPosition());

                CompensateForVehicleVelocity(m_explosionArgs.m_pAttachEntity);
            }
        }

		if (bSuccess)
		{
			bSuccess = CExplosionManager::AddExplosion(m_explosionArgs);
		}
	}

    return bSuccess;
}

CObject *CExplosionEvent::GetObjectFromExplodingDummyPosition()
{
	CObject *objectToExplode = 0;

	gnetAssertf(m_dummyPosition.Mag2() > 1.0f, "CExplosionEvent::GetObjectFromExplodingDummyPosition() - dummy position not set");

	objectToExplode = CNetObjObject::FindAssociatedAmbientObject(m_dummyPosition, -1);

	if(objectToExplode == 0)
	{
		fwPtrListSingleLink dummyList;
		fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(m_dummyPosition), ScalarV(50.0f)));    // NB please review this 50.0f!
		CGameWorld::ForAllEntitiesIntersecting(&testSphere, gWorldMgr.AppendToLinkedListCB, (void*) &dummyList,
			ENTITY_TYPE_MASK_DUMMY_OBJECT, SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS);

		fwPtrNode *pDummyNode = dummyList.GetHeadPtr();

		while (pDummyNode)
		{
			CDummyObject *pDummy = (CDummyObject*)pDummyNode->GetPtr();

			if (CNetObjObject::IsSameDummyPos(m_dummyPosition, VEC3V_TO_VECTOR3(pDummy->GetTransform().GetPosition())))
			{
				objectToExplode = (CObject*) CObjectPopulation::ConvertToRealObject(pDummy);
				break;
			}

			pDummyNode = pDummyNode->GetNextPtr();
		}
	}

	gnetAssertf(objectToExplode, "CExplosionEvent - Failed to find dummy object at %f, %f, %f", m_dummyPosition.x, m_dummyPosition.y, m_dummyPosition.z);

	return objectToExplode;
}
// ===========================================================================================================
// START PROJECTILE EVENT
// ===========================================================================================================
const float CStartProjectileEvent::PROJECTILE_CLONE_RANGE_SQ = 500.0f * 500.0f;
const float CStartProjectileEvent::PROJECTILE_CLONE_RANGE_SQ_EXTENDED = 750.0f * 750.0f;

CStartProjectileEvent::CStartProjectileEvent() :
netGameEvent(START_PROJECTILE_EVENT, false),
m_projectileOwnerID  (NETWORK_INVALID_OBJECT_ID),
m_projectileHash     (0),
m_weaponFiredFromHash(0),
m_initialPosition    (Vector3(0.0f, 0.0f, 0.0f)),
m_fireDirection      (Vector3(0.0f, 0.0f, 0.0f)),
m_effectGroup        (NUM_EWEAPONEFFECTGROUP),
m_targetEntityID     (NETWORK_INVALID_OBJECT_ID),
m_projectileID       (NETWORK_INVALID_OBJECT_ID),
m_ignoreDamageEntId  (NETWORK_INVALID_OBJECT_ID),
m_noCollisionEntId   (NETWORK_INVALID_OBJECT_ID),
m_coordinatedWithTask(false),
m_coordinateWithTaskSequence(0),
m_bCommandFireSingleBullet(false),
m_bAllowDamping(true),
m_bVehicleRelativeOffset(false),
m_vVehicleRelativeOffset(VEC3_ZERO),
m_fLaunchSpeedOverride(-1.0f),
m_bWasLockedOnWhenFired(false),
m_tintIndex(0),
m_vehicleFakeSequenceId((u8)~0),
m_bSyncOrientation(false),
m_projectileWeaponMatrix(M34_IDENTITY)
{
    // Empty
}

CStartProjectileEvent::CStartProjectileEvent(const ObjectId          projectileOwnerID,
                                             u32                     projectileHash,
                                             u32                     weaponFiredFromHash,
                                             const Vector3          &initialPosition,
                                             const Vector3          &fireDirection,
                                             eWeaponEffectGroup      effectGroup,
                                             const ObjectId          targetEntityID,
                                             const ObjectId          projectileID,
											 const ObjectId			 ignoreDamageEntID,
											 const ObjectId			 noCollisionEntID,
                                             const CNetFXIdentifier &identifier,
											 const bool bCommandFireSingleBullet,
											 bool		 bAllowDamping,
											 float		 fLaunchSpeedOverride,
											 bool		 bRocket,
											 bool		 bWasLockedOnWhenFired,
											 u8			 tintIndex,
											 u8			 fakeStickyBombSequence,
											 bool        syncOrientation,
											 Matrix34    projectileWeaponMatrix) :
netGameEvent(START_PROJECTILE_EVENT, false),
m_projectileOwnerID  (projectileOwnerID),
m_projectileHash     (projectileHash),
m_weaponFiredFromHash(weaponFiredFromHash),
m_initialPosition    (initialPosition),
m_fireDirection      (fireDirection),
m_effectGroup        (effectGroup),
m_targetEntityID     (targetEntityID),
m_projectileID       (projectileID),
m_ignoreDamageEntId  (ignoreDamageEntID),
m_noCollisionEntId   (noCollisionEntID),
m_coordinatedWithTask(false),
m_coordinateWithTaskSequence(0),
m_netIdentifier(identifier),
m_bCommandFireSingleBullet(bCommandFireSingleBullet),
m_bAllowDamping(bAllowDamping),
m_bVehicleRelativeOffset(false),
m_vVehicleRelativeOffset(VEC3_ZERO),
m_fLaunchSpeedOverride(fLaunchSpeedOverride),
m_bWasLockedOnWhenFired(bWasLockedOnWhenFired),
m_tintIndex(tintIndex),
m_vehicleFakeSequenceId(fakeStickyBombSequence),
m_bSyncOrientation(syncOrientation)
{
    if(m_projectileOwnerID != NETWORK_INVALID_OBJECT_ID)
    {
        netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID);
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

        if(ped && !m_bCommandFireSingleBullet)
        {
            if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THROW_PROJECTILE))
            {
                m_coordinateWithTaskSequence = ped->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_THROW_PROJECTILE);
                m_coordinatedWithTask = true;
            }
			else if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE))
			{
				m_coordinateWithTaskSequence = ped->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
				m_coordinatedWithTask = true;
			}
			else if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE))
			{
				m_coordinateWithTaskSequence = ped->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE);
				m_coordinatedWithTask = true;
			}
        }

		if (bRocket)
		{
			m_bVehicleRelativeOffset = CalculateMovingVehicleRelativeFirePoint(networkObject);

			if(ped && ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
			{   //Coordinate with TASK_AIM_GUN_ON_FOOT and sync task sequence when firing rockets B*1972374
				m_coordinateWithTaskSequence = ped->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_AIM_GUN_ON_FOOT);
				m_coordinatedWithTask = true;
			}
		}	

		if (m_bSyncOrientation)
		{
			m_projectileWeaponMatrix = projectileWeaponMatrix;
		}
	}
}

template <class Serialiser> void CStartProjectileEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const unsigned int SIZEOF_TINT_INDEX = 8;

    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_projectileOwnerID, "Projectile Owner ID");
    SERIALISE_UNSIGNED(serialiser, m_projectileHash, sizeof(m_projectileHash)<<3, "Projectile Hash");
    SERIALISE_UNSIGNED(serialiser, m_weaponFiredFromHash, sizeof(m_weaponFiredFromHash)<<3, "Weapon Hash");
	SERIALISE_VECTOR(serialiser, m_initialPosition, WORLDLIMITS_XMAX, 32, "Initial Position");
    SERIALISE_OBJECTID(serialiser, m_targetEntityID, "Target Entity ID");
	SERIALISE_VECTOR(serialiser, m_fireDirection, 1.1f, SIZEOF_FIRE_DIRECTION, "Fire direction");
    SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_effectGroup), SIZEOF_EFFECT_GROUP, "Effect group");
	SERIALISE_UNSIGNED(serialiser, m_tintIndex, SIZEOF_TINT_INDEX, "Tint Index");
	SERIALISE_BOOL(serialiser, m_bAllowDamping, "Allow Damping");
	SERIALISE_BOOL(serialiser, m_bVehicleRelativeOffset, "Vehicle Relative Offset");

	SERIALISE_BOOL(serialiser, m_bCommandFireSingleBullet, "Command Fire Single Bullet");
	
	bool hasValidFakeSequence = m_vehicleFakeSequenceId != INVALID_FAKE_SEQUENCE_ID;
	SERIALISE_BOOL(serialiser, hasValidFakeSequence, "Has Valid Fake Sequence");
	if(hasValidFakeSequence)
	{
		static const unsigned SIZEOF_FAKE_SEQUENCE_ID = datBitsNeeded<CNetworkPendingProjectiles::MAX_PENDING_PROJECTILES-1>::COUNT;
		SERIALISE_UNSIGNED(serialiser, m_vehicleFakeSequenceId, SIZEOF_FAKE_SEQUENCE_ID, "Fake Sequence Id for command sticky bomb");
	}
	else
	{
		m_vehicleFakeSequenceId = INVALID_FAKE_SEQUENCE_ID;
	}

	if(m_bVehicleRelativeOffset)
	{
		const float MAX_VEHICLE_RELATIVE_OFFSET = 400.0f;

		if (fabs(m_vVehicleRelativeOffset.x) > MAX_VEHICLE_RELATIVE_OFFSET || fabs(m_vVehicleRelativeOffset.y) > MAX_VEHICLE_RELATIVE_OFFSET || fabs(m_vVehicleRelativeOffset.z) > MAX_VEHICLE_RELATIVE_OFFSET)
		{
#if __ASSERT
			netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID);
			const CVehicle *vehicleProjectileWasFiredFrom = GetLaunchVehicle(networkObject);

			// Calling CPhysical::GetDebugName() as CVehicle::GetDebugName() only returns the model name
			gnetAssertf(false, "Exceeded expected vehicle relative offset limits for vehicle %s", vehicleProjectileWasFiredFrom ? vehicleProjectileWasFiredFrom->CPhysical::GetDebugName() : "No Vehicle Data!");
#endif // __ASSERT
			
			m_vVehicleRelativeOffset.x = Clamp(m_vVehicleRelativeOffset.x, -MAX_VEHICLE_RELATIVE_OFFSET, MAX_VEHICLE_RELATIVE_OFFSET);
			m_vVehicleRelativeOffset.y = Clamp(m_vVehicleRelativeOffset.y, -MAX_VEHICLE_RELATIVE_OFFSET, MAX_VEHICLE_RELATIVE_OFFSET);
			m_vVehicleRelativeOffset.z = Clamp(m_vVehicleRelativeOffset.z, -MAX_VEHICLE_RELATIVE_OFFSET, MAX_VEHICLE_RELATIVE_OFFSET);			
		}

		SERIALISE_VECTOR(serialiser, m_vVehicleRelativeOffset, MAX_VEHICLE_RELATIVE_OFFSET, SIZEOF_VEHICLE_OFFSET_DIRECTION, "Relative vehicle offset");
	}
	else
	{
		m_vVehicleRelativeOffset = VEC3_ZERO;
	}

    // write whether this projectile has come from a throwing task
    SERIALISE_BOOL(serialiser, m_coordinatedWithTask, "Coordinated with task");

	bool bLaunchSpeedOverride = false;
	if(m_fLaunchSpeedOverride >= 0.0f)
	{
		bLaunchSpeedOverride = true;
	}
	
	SERIALISE_BOOL(serialiser, bLaunchSpeedOverride);
	if(bLaunchSpeedOverride)
	{
		static const float MAX_WEAPON_LAUNCH_SPEED = 8000.0f; // Max value comes from script SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY which currently only uses car speed (m/s) squared + 100 so increase this to 8000 to accommodate about 200 mph
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fLaunchSpeedOverride,MAX_WEAPON_LAUNCH_SPEED,SIZEOF_THROW_LAUNCH_SPEED,"Override Launch Speed");
	}
	else
	{
		m_fLaunchSpeedOverride = -1.0f; //set to default when not overriding
	}

    if(m_coordinatedWithTask)
    {
        SERIALISE_UNSIGNED(serialiser, m_coordinateWithTaskSequence, 32, "Coordinate with Task Sequence");
        // thrown world objects not supported anymore
        //SERIALISE_OBJECTID(serialiser, m_projectileID, "Projectile ID");
    }
	SERIALISE_BOOL(serialiser, m_bWasLockedOnWhenFired, "Was Locked On When Fired");  //only expected with rockets on foot which always are coordinated with task

	SERIALISE_OBJECTID(serialiser, m_ignoreDamageEntId, "Ignore Damage Entity ID");
	SERIALISE_OBJECTID(serialiser, m_noCollisionEntId, "No Collision Entity ID");

	SERIALISE_BOOL(serialiser, m_bSyncOrientation, "Sync the orientation for this projectile");
	if (m_bSyncOrientation)
	{
		SERIALISE_ORIENTATION(serialiser, m_projectileWeaponMatrix, "Projectile Orientation");
	}

    m_netIdentifier.Serialise<Serialiser>(messageBuffer);
}

void CStartProjectileEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CStartProjectileEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

void CStartProjectileEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CStartProjectileEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStartProjectileEvent::Trigger(CProjectile* pProjectile, const Vector3& fireDirection, bool bCommandFireSingleBullet, bool bAllowDamping, float fLaunchSpeedOverride)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

	bool bRocket = false;
	bool bWasLockedOnWhenFired = false;
    if (pProjectile->GetOwner() && !static_cast<const CDynamicEntity*>(pProjectile->GetOwner())->GetNetworkObject())
    {
        //NetworkInterface::GetEventManager().GetLog().WriteErrorHeader("Attempting to start a projectile event for an owner not registered with the network!");
    }
    else
    {
        const CEntity* pTargetEntity = NULL;

        CProjectileRocket* pRocket = pProjectile->GetAsProjectileRocket();
        if (pRocket)
        {
			bRocket = true;
            pTargetEntity = pRocket->GetTarget();
			bWasLockedOnWhenFired = pRocket->GetWasLockedOnWhenFired();
        }

		const CEntity* pIgnoreDamageEnt = pProjectile->GetIgnoreDamageEntity();
		const CEntity* pNoCollisionEnt = (const CEntity*)(pProjectile->GetNoCollisionEntity());

        const ObjectId ownerId = pProjectile->GetOwner() ? static_cast<const CDynamicEntity*>(pProjectile->GetOwner())->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
        const ObjectId projectileId = pProjectile->GetNetworkObject() ? pProjectile->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
        const ObjectId targetId = (pTargetEntity && static_cast<const CDynamicEntity*>(pTargetEntity)->GetNetworkObject()) ? static_cast<const CDynamicEntity*>(pTargetEntity)->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;	
		const ObjectId ignoreDamageEntId = pIgnoreDamageEnt && static_cast<const CDynamicEntity*>(pIgnoreDamageEnt)->GetNetworkObject() ? static_cast<const CDynamicEntity*>(pIgnoreDamageEnt)->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
		const ObjectId noCollisionEntId = pNoCollisionEnt && static_cast<const CDynamicEntity*>(pNoCollisionEnt)->GetNetworkObject() ? static_cast<const CDynamicEntity*>(pNoCollisionEnt)->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
		const u8 tintIndex = pProjectile->GetWeaponTintIndex();

		bool syncOrientation = false;
		Matrix34 projectileWeaponMatrix;
		u32 weaponFiredFromHash = pProjectile->GetWeaponFiredFromHash();
		if (weaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_BOMB", 0x9af0b90c) ||
			weaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_BOMB_CLUSTER", 0x0d28bca3) ||
			weaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_BOMB_GAS", 0x5540a91e) ||
			weaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_BOMB_INCENDIARY", 0x6af7a717))
		{
			syncOrientation = true;
			projectileWeaponMatrix = MAT34V_TO_MATRIX34(pProjectile->GetMatrix());
		}

        NetworkInterface::GetEventManager().CheckForSpaceInPool();
        CStartProjectileEvent *pEvent = rage_new CStartProjectileEvent(ownerId, pProjectile->GetHash(), pProjectile->GetWeaponFiredFromHash(), VEC3V_TO_VECTOR3(pProjectile->GetTransform().GetPosition()), fireDirection, pProjectile->GetEffectGroup(), targetId, projectileId, ignoreDamageEntId, noCollisionEntId, pProjectile->GetNetworkIdentifier(),bCommandFireSingleBullet, bAllowDamping, fLaunchSpeedOverride, bRocket, bWasLockedOnWhenFired, tintIndex, (u8)pProjectile->GetTaskSequenceId(), syncOrientation, projectileWeaponMatrix);
        NetworkInterface::GetEventManager().PrepareEvent(pEvent);	
    }
}

bool CStartProjectileEvent::IsInScope( const netPlayer& playerToCheck ) const
{
    bool bInScope = playerToCheck.IsRemote();

	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if(bInScope)
    {
		// don't send to players in a cutscene
		if (player.GetPlayerInfo() && player.GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		{
			bInScope = false;
		}
        else if(m_targetEntityID != NETWORK_INVALID_OBJECT_ID)
        {
            // don't send to players that don't know about the projectile target yet
            netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntityID);

            if(networkObject == 0 || (!networkObject->IsClone() && !networkObject->HasBeenCloned(playerToCheck)))
            {
                bInScope = false;
            }
        }
        else
        {
			if(player.GetPlayerPed())
            {
				Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(playerToCheck);

				gnetAssertf(!playerFocusPosition.IsClose(VEC3_ZERO, 0.1f),"Expect a valid focus position for player %s",playerToCheck.GetLogName());

                // don't send to players far away from the projectile start position
				ScalarV svDistanceFromProjectileSq = DistSquared(RCC_VEC3V(playerFocusPosition), RCC_VEC3V(m_initialPosition));
				if(m_weaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_CHERNO_MISSILE", 0xa247d03e))
				{
					bInScope = (IsLessThanOrEqualAll(svDistanceFromProjectileSq, ScalarVFromF32(PROJECTILE_CLONE_RANGE_SQ_EXTENDED))) != 0;
				}
				else
				{
					bInScope = (IsLessThanOrEqualAll(svDistanceFromProjectileSq, ScalarVFromF32(PROJECTILE_CLONE_RANGE_SQ))) != 0;
				}
			}
        }

		//If firing relative to vehicle check it has been cloned on player
		if(m_bVehicleRelativeOffset)
		{
			netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID);
			const CVehicle *pVeh = GetLaunchVehicle(networkObject);

			if(taskVerifyf(pVeh,"%s should have a valid vehicle!",networkObject?networkObject->GetLogName():"Null net object"))
			{
				netObject* vehicleNetObject = pVeh->GetNetworkObject();
				if(vehicleNetObject == 0 || (!vehicleNetObject->IsClone() && !vehicleNetObject->HasBeenCloned(playerToCheck)))
				{
					bInScope = false;
				}
			}
		}
    }

    return bInScope;
}

bool CStartProjectileEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    // get the network objects from the read object IDs
    netObject *pNetObjProjectileOwner = 0;
    netObject *pNetObjTargetEntity    = 0;
    netObject *pNetObjProjectile      = 0;
	netObject *pNetObjIgnoreDamageEntity = 0;
	netObject *pNetObjNoCollisionEntity = 0;

    if(m_projectileOwnerID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjProjectileOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID);
    }

    if(m_targetEntityID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjTargetEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntityID);
    }

    if(m_projectileID != NETWORK_INVALID_OBJECT_ID)
    {
        pNetObjProjectile = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileID);
    }

	if(m_ignoreDamageEntId != NETWORK_INVALID_OBJECT_ID)
	{
		pNetObjIgnoreDamageEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_ignoreDamageEntId);
	}

	if(m_noCollisionEntId != NETWORK_INVALID_OBJECT_ID)
	{
		pNetObjNoCollisionEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_noCollisionEntId);
	}

    m_netIdentifier.SetPlayerOwner(fromPlayer.GetPhysicalPlayerIndex());

    // get the entities from the network objects
    CEntity *pProjectileOwner = pNetObjProjectileOwner ? pNetObjProjectileOwner->GetEntity() : 0;
    CEntity *pTargetEntity    = pNetObjTargetEntity    ? pNetObjTargetEntity->GetEntity()    : 0;
    CEntity *pProjectile      = pNetObjProjectile      ? pNetObjProjectile->GetEntity()      : 0;
	CEntity *pIgnoreDamageEntity = pNetObjIgnoreDamageEntity ? pNetObjIgnoreDamageEntity->GetEntity() : 0;
	CEntity *pNoCollisionEntity = pNetObjNoCollisionEntity ? pNetObjNoCollisionEntity->GetEntity() : 0;

    // check if the projectile is loaded, if not request it and don't acknowledge the event
    bool ackResult           = true;
    bool projectileAvailable = true;
	bool isRocket			 = false;

	// ignore projectile events in cutscenes
	if (!NetworkInterface::IsInMPCutscene() && !SafeCast(const CNetGamePlayer, &fromPlayer)->IsInDifferentTutorialSession())
	{
		if (!pProjectile)
		{
			const CItemInfo* info = CWeaponInfoManager::GetInfo(m_projectileHash);
			gnetAssertf(info, "A CStartProjectileEvent has been triggered with an invalid weapon hash");

			if (info)
			{
				isRocket = info->GetClassId() == CAmmoRocketInfo::GetStaticClassId();

				u32 modelIndex = info->GetModelIndex();
				fwModelId modelId((strLocalIndex(modelIndex)));
				gnetAssert(modelIndex != -1);

				if (!CModelInfo::HaveAssetsLoaded(modelId))
				{
					CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
					projectileAvailable = false;
				}
			}
		}

		if(!projectileAvailable)
		{
			ackResult = false;
		}
		else
		{
			Matrix34 tempWeaponMatrix;
			if(pProjectileOwner)
			{
				if(m_bVehicleRelativeOffset  && taskVerifyf(!m_coordinatedWithTask,"m_bVehicleRelativeOffset Only compensates straight line powered projectiles not coordinated task projectiles"))
				{
					//If projectile being fired from a fast moving vehicle, move the start point along the fire direction here 
					ModifyInitialPosForVehicleRelativeFirePoint(pNetObjProjectileOwner); 
				}

				tempWeaponMatrix = MAT34V_TO_MATRIX34(pProjectileOwner->GetMatrix());

				if(pProjectileOwner->GetIsTypePed())
				{
					CPed *ped = static_cast<CPed *>(pProjectileOwner);

					CPedWeaponManager* pWeaponMgr = ped->GetWeaponManager();
					CWeapon* pWeapon = pWeaponMgr ? pWeaponMgr->GetEquippedWeapon() : NULL;
					const CObject* pWeaponObject = pWeaponMgr ? pWeaponMgr->GetEquippedWeaponObject() : NULL;

					if(m_bCommandFireSingleBullet)
					{
						MakeWeaponMatrix(tempWeaponMatrix,true);
					}
					else if(pWeapon && pWeaponObject)
					{
						if(pWeapon->GetWeaponInfo()->GetIsProjectile())
						{
							if(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_PROJECTILE) != -1)
							{			
								MakeWeaponMatrix(tempWeaponMatrix,true);
							}
							else if(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE) != -1)
							{
								MakeWeaponMatrix(tempWeaponMatrix,false);
							}
							else
							{
								pWeaponObject->GetMatrixCopy(tempWeaponMatrix);
							}
						}
						else
						{
							pWeaponObject->GetMatrixCopy(tempWeaponMatrix);
						}
					}
					// ped passenger firing from turret
					else
					{
						CVehicle* vehicle = ped->GetMyVehicle();
						if(vehicle && vehicle->GetVehicleWeaponMgr())
						{
							int pedSeat = vehicle->GetSeatManager()->GetPedsSeatIndex(ped);
							
							CTurret* turret = vehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(pedSeat);
							if(turret)
							{
								// Set projectile's direction to synced direction vector
								tempWeaponMatrix.b = m_fireDirection;
								tempWeaponMatrix.b.Normalize();

								tempWeaponMatrix.a.Cross(tempWeaponMatrix.b, Vector3(0.0f, 0.0f, 1.0f));
								tempWeaponMatrix.a.Normalize();

								tempWeaponMatrix.c.Cross(tempWeaponMatrix.a, tempWeaponMatrix.b);
								tempWeaponMatrix.c.Normalize();
							}
						}
					}
				}
				else
				{   //Single turret vehicle is firing not using m_bCommandFireSingleBullet so use the turret matrix for the projectile instead of the vehicle matrix alone
					if(pProjectileOwner->GetIsTypeVehicle())
					{
						CVehicle *pVehicle = static_cast<CVehicle *>(pProjectileOwner);

						if(pVehicle->GetIsHeli())
						{  //The heli has a searchlight turret in weapon manager so dont use to calc projectile matrix use fire direction to get matrix
							MakeWeaponMatrix(tempWeaponMatrix,true);
						}
						else if(pVehicle->GetVehicleWeaponMgr() && pVehicle->GetVehicleWeaponMgr()->GetNumTurrets()==1)
						{
							CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret(0);
							pTurret->GetTurretMatrixWorld(tempWeaponMatrix,pVehicle);						
						}
					}
				}
			}
			else
			{
				tempWeaponMatrix.Identity();
			}

			if(m_bSyncOrientation)
			{
				tempWeaponMatrix = m_projectileWeaponMatrix;
			}

			tempWeaponMatrix.d = m_initialPosition;
			
			gnetAssert(tempWeaponMatrix.IsOrthonormal());
			//tempWeaponMatrix.Print();

			bool addProjectile = (m_projectileOwnerID == NETWORK_INVALID_OBJECT_ID) || (pProjectileOwner != 0); //if either we actively didn't send an owner, or we actively sent an owner (lavalley)

			if(m_coordinatedWithTask)
			{
				addProjectile = false;

				if(isRocket)
				{ 
					//If syncing a rocket check if the peds TASK_AIM_GUN_ON_FOOT coordinated task 
					//is actually running. If it isn't, then we have to create the projectile and fire it immediately here otherwise it never gets created,
					//we set addProjectile true in order to ignore CNetworkPendingProjectiles create the rocket and call its Fire function below.
					CPed* pFiringPed = static_cast<CPed*>(pProjectileOwner);
					if (pFiringPed)
					{
						CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));

						if(!pAimGunTask || !pAimGunTask->IsInScope(pFiringPed))
						{
							addProjectile = true;
						}
					}
				}

				if(!addProjectile)
				{
					CNetworkPendingProjectiles::AddProjectile(pProjectileOwner, m_coordinateWithTaskSequence, m_projectileHash, m_weaponFiredFromHash, tempWeaponMatrix, m_effectGroup, pTargetEntity, 0, m_fireDirection, m_netIdentifier, m_bAllowDamping, m_bWasLockedOnWhenFired, m_fLaunchSpeedOverride);
				}
			}

			if(addProjectile && !pProjectile)
			{
				CProjectile *pProjectile = CProjectileManager::CreateProjectile(m_projectileHash, m_weaponFiredFromHash, pProjectileOwner, tempWeaponMatrix, 0.0f, DAMAGE_TYPE_NONE, m_effectGroup, pTargetEntity, &m_netIdentifier, (u32) m_tintIndex);

                if(pProjectile == 0)
                {
                    gnetDebug2("Failed to create networked projectile!");
                }
                else
                {
					if (pIgnoreDamageEntity)
						pProjectile->SetIgnoreDamageEntity(pIgnoreDamageEntity);

					if (pNoCollisionEntity)
						pProjectile->SetNoCollisionEntity(pNoCollisionEntity);

					if((m_bCommandFireSingleBullet || m_weaponFiredFromHash == ATSTRINGHASH("WEAPON_FLAREGUN", 0x47757124)) && m_vehicleFakeSequenceId != INVALID_FAKE_SEQUENCE_ID)
					{
						pProjectile->SetTaskSequenceId(m_vehicleFakeSequenceId);
					}

					bool bAllowToSetOwnerAsNoCollision = pNoCollisionEntity ? false : true;
				    pProjectile->Fire(m_fireDirection, /*fLifeTime*/ -1.0f, m_fLaunchSpeedOverride, m_bAllowDamping,false,false,false,NULL,false,bAllowToSetOwnerAsNoCollision);
					CProjectileRocket* pRocket = pProjectile->GetAsProjectileRocket();
					if (pRocket)
					{
						pRocket->SetWasLockedOnWhenFired(m_bWasLockedOnWhenFired);
					}
                }
			}	
		}
	}

    return ackResult;
}

void  CStartProjectileEvent::MakeWeaponMatrix(Matrix34 &tempWeaponMatrix, bool bProjectile)
{
	// The missile and grenade models have different orientations in max
	// (which the bones in the weapons take into consideration)
	// so we have to alter the matrix for the two models.
	// In max	- missile points fwd = y axis
	//			- grenade point fwd = x axis

	Vector3 upAxis(0.0f, 0.0f, 1.0f);
	Vector3 fwdDir		= m_fireDirection;
	fwdDir.Normalize();

	const float AIM_VEC_NEAR_EPSILON = 0.0001f;

	if( (1.0f - fabs(fwdDir.z)) < AIM_VEC_NEAR_EPSILON)
	{
		//if aiming directly up or down then make upAxis be orthogonal to this instead
		upAxis = Vector3(0.0f, 1.0f, 0.0f);
	}

	Vector3 rightDir	= m_fireDirection;

	if(rightDir.IsClose(VEC3_ZERO, 0.1f))
	{
		//Turns out zero fire directions are valid input
		//parameters to CProjectile::Fire and are effectively
		//giving the projectile zero velocity when the weapon
		//has fired near an obstruction and blows up nearby. See B* 1222364 & B*1208407
		//So just return a valid matrix here since the projectile is going nowhere.

		tempWeaponMatrix.Identity();
		return;
	}

	rightDir.Cross(upAxis); 
	rightDir.Normalize();

	Vector3 upDir		= rightDir;
	upDir.Cross(fwdDir);
	upDir.Normalize();

	if(bProjectile)
	{			
		tempWeaponMatrix.a	= rightDir;
		tempWeaponMatrix.b	= fwdDir;
		tempWeaponMatrix.c	= upDir;
	}
	else
	{
		tempWeaponMatrix.a	= fwdDir;
		tempWeaponMatrix.b	= upDir;
		tempWeaponMatrix.c	= rightDir;
	}
}

const CVehicle* CStartProjectileEvent::GetLaunchVehicle(const netObject *networkObject) const
{
	const CVehicle *pVeh = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

	if(!pVeh)
	{
		const CPed *pPed = NetworkUtils::GetPedFromNetworkObject(networkObject);

		if(pPed && pPed->GetMyVehicle() && pPed->GetIsInVehicle() && pPed->GetMyVehicle()->GetNetworkObject())
		{
			pVeh = pPed->GetMyVehicle();
		}
	}

	return pVeh;
}

bool CStartProjectileEvent::CalculateMovingVehicleRelativeFirePoint(const netObject *networkObject)
{
	m_vVehicleRelativeOffset = VEC3_ZERO;
	const CVehicle *pVeh = GetLaunchVehicle(networkObject);

	static const float VEH_SPEED_THRESHOLD = 10.0f;
	static const float VEH_SPEED_THRESHOLD_SQ = VEH_SPEED_THRESHOLD*VEH_SPEED_THRESHOLD;

	if(pVeh && pVeh->GetVelocity().Mag2() > VEH_SPEED_THRESHOLD_SQ)
	{
		Matrix34 mMatrix = MAT34V_TO_MATRIX34(pVeh->GetMatrix());
		m_vVehicleRelativeOffset = m_initialPosition;
		mMatrix.UnTransform(m_vVehicleRelativeOffset);
		
		gnetAssertf(!m_vVehicleRelativeOffset.IsClose(VEC3_ZERO, 0.05f),"m_vVehicleRelativeOffset is zero size locally!");

		// Relative offset can't be bigger than 50 meters (unless new models are added with a bigger distance between centre and gun muzzles) 
		if (m_vVehicleRelativeOffset.Mag() > 50.0f)
		{
			return false;
		}

		return true;
	}

	return false;
}


void CStartProjectileEvent::ModifyInitialPosForVehicleRelativeFirePoint(const netObject *networkObject)
{
	const CVehicle *pVeh = GetLaunchVehicle(networkObject);

	gnetAssertf(!m_vVehicleRelativeOffset.IsClose(VEC3_ZERO, 0.05f),"m_vVehicleRelativeOffset is zero size!");

	if( taskVerifyf(pVeh,"No vehicle associated with %s firing projectile type 0x%x",networkObject?networkObject->GetLogName():"Null net object",m_projectileHash) )
	{
		Vector3 newInitialPosition = m_vVehicleRelativeOffset;

		Matrix34 mMatrix = MAT34V_TO_MATRIX34(pVeh->GetMatrix());
		mMatrix.Transform(newInitialPosition);

		newInitialPosition += pVeh->GetVelocity() * fwTimer::GetTimeStep();

		m_initialPosition = newInitialPosition;
	}
}

#if ENABLE_NETWORK_LOGGING

void CStartProjectileEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    WriteDataToLogFile(bEventLogOnly);
}

void CStartProjectileEvent::WriteDataToLogFile(bool LOGGING_ONLY(bEventLogOnly)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    if(NETWORK_INVALID_OBJECT_ID != m_projectileOwnerID)
    {
        netObject* projectileOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID, true);
        if (projectileOwner)
        {
            log.WriteDataValue("Owner", "%s", projectileOwner->GetLogName());
        }
    }

    log.WriteDataValue("Projectile type", "%u", m_projectileHash);
    log.WriteDataValue("Initial position", "%f, %f, %f", m_initialPosition.x, m_initialPosition.y, m_initialPosition.z);
    log.WriteDataValue("Fire direction", "%f, %f, %f", m_fireDirection.x, m_fireDirection.y, m_fireDirection.z);
	log.WriteDataValue("Command fire single bullet", "%s", m_bCommandFireSingleBullet ? "true" : "false");
	log.WriteDataValue("Command fake sequence id", "%d", m_vehicleFakeSequenceId);

    if(NETWORK_INVALID_OBJECT_ID != m_targetEntityID)
    {
        netObject* targetEntity = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntityID, true);
        if (targetEntity)
        {
            log.WriteDataValue("Target", "%s", targetEntity->GetLogName());
        }
    }

	log.WriteDataValue("Coordinated with task", "%s", m_coordinatedWithTask ? "true" : "false");

	if(m_coordinatedWithTask)
	{
		log.WriteDataValue("Coordinated task sequence", "%d", m_coordinateWithTaskSequence);
		log.WriteDataValue("Was Locked On When Fired", "%s", m_bWasLockedOnWhenFired ? "true" : "false");
	}

    if (m_fLaunchSpeedOverride >=0.0f)
    {
		log.WriteDataValue("Allow damping", "%s", m_bAllowDamping ? "true" : "false");
		log.WriteDataValue("Launch speed override", "%f", m_fLaunchSpeedOverride);
    }
    else
    {
        log.WriteDataValue("Launch speed override", "none");
    }

	log.WriteDataValue("Using vehicle relative offset", "%s", m_bVehicleRelativeOffset ? "true" : "false");

	if(m_bVehicleRelativeOffset)
	{
		log.WriteDataValue("Vehicle Relative Offset", "%f, %f, %f", m_vVehicleRelativeOffset.x, m_vVehicleRelativeOffset.y, m_vVehicleRelativeOffset.z);

		netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID);
		const CVehicle *vehicleProjectileWasFiredFrom = GetLaunchVehicle(networkObject);

		// Calling CPhysical::GetDebugName() as CVehicle::GetDebugName() only returns the model name
		log.WriteDataValue("Vehicle Relative ID", "%s", vehicleProjectileWasFiredFrom ? vehicleProjectileWasFiredFrom->CPhysical::GetDebugName() : "No Vehicle Data!");
	}

	if(NETWORK_INVALID_OBJECT_ID != m_projectileID)
	{
		netObject* projectileID = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileID, true);
		if (projectileID)
		{
			log.WriteDataValue("Projectile Object", "%s", projectileID->GetLogName());
		}
	}

	if(NETWORK_INVALID_OBJECT_ID != m_ignoreDamageEntId)
	{
		netObject* ignoreDamageEntId = NetworkInterface::GetObjectManager().GetNetworkObject(m_ignoreDamageEntId, true);
		if (ignoreDamageEntId)
		{
			log.WriteDataValue("Projectile Object", "%s", ignoreDamageEntId->GetLogName());
		}
	}

	if(NETWORK_INVALID_OBJECT_ID != m_noCollisionEntId)
	{
		netObject* noCollisionEntId = NetworkInterface::GetObjectManager().GetNetworkObject(m_noCollisionEntId, true);
		if (noCollisionEntId)
		{
			log.WriteDataValue("Projectile Object", "%s", noCollisionEntId->GetLogName());
		}
	}

	log.WriteDataValue("Projectile Identifier", "%d:%d", m_netIdentifier.GetPlayerOwner(), m_netIdentifier.GetFXId());

	log.WriteDataValue("Sync orientation", "%s", m_bSyncOrientation ? "true" : "false");
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// UPDATE PROJECTILE TARGET EVENT
// ===========================================================================================================
CUpdateProjectileTargetEntity::CUpdateProjectileTargetEntity() :
	netGameEvent(UPDATE_PROJECTILE_TARGET_EVENT, false),
	m_projectileOwnerID    (NETWORK_INVALID_OBJECT_ID),
	m_projectileSequenceId (-1),
	m_targetFlareGunSequenceId       (-1)
{
	// Empty
}

CUpdateProjectileTargetEntity::CUpdateProjectileTargetEntity(const ObjectId     projectileOwnerID,
															 u32                projectilSequenceId,
															 s32				targetFlareGunSequenceId) :
netGameEvent(UPDATE_PROJECTILE_TARGET_EVENT, false),
	m_projectileOwnerID				(projectileOwnerID),
	m_projectileSequenceId			(projectilSequenceId),
	m_targetFlareGunSequenceId		(targetFlareGunSequenceId)
{
}

bool CUpdateProjectileTargetEntity::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == UPDATE_PROJECTILE_TARGET_EVENT)
	{
		const CUpdateProjectileTargetEntity* pScriptEvent = SafeCast(const CUpdateProjectileTargetEntity, pEvent);

		if (pScriptEvent)
		{
			if(pScriptEvent->m_projectileOwnerID == m_projectileOwnerID &&
			   pScriptEvent->m_projectileSequenceId == m_projectileSequenceId &&
			   pScriptEvent->m_targetFlareGunSequenceId == m_targetFlareGunSequenceId)
			{
				return true;
			}
		}
	}

	return false;
}

template <class Serialiser> void CUpdateProjectileTargetEntity::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const int NUM_BITS_SEQUENCE_ID = 32;
	static const int NUM_BITS_FLARE_SEQUENCE_ID = datBitsNeeded<CProjectileManager::MAX_FLARE_GUN_PROJECTILES_PER_PED_MP>::COUNT + 1;

	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_projectileOwnerID, "Projectile Owner ID");
	SERIALISE_INTEGER(serialiser, m_projectileSequenceId, NUM_BITS_SEQUENCE_ID, "Projectile Sequence ID");
	SERIALISE_INTEGER(serialiser, m_targetFlareGunSequenceId, NUM_BITS_FLARE_SEQUENCE_ID, "Target Flare Gun Sequence ID");
}

void CUpdateProjectileTargetEntity::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CUpdateProjectileTargetEntity::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

void CUpdateProjectileTargetEntity::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CUpdateProjectileTargetEntity networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CUpdateProjectileTargetEntity::Trigger(CProjectile* pProjectile, s32 targetFlareGunSequenceId)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (pProjectile->GetOwner())
	{
		netObject* projectileNetOwner = static_cast<const CDynamicEntity*>(pProjectile->GetOwner())->GetNetworkObject();
		if(projectileNetOwner)
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CUpdateProjectileTargetEntity *pEvent = rage_new CUpdateProjectileTargetEntity(projectileNetOwner->GetObjectID(), pProjectile->GetTaskSequenceId(), targetFlareGunSequenceId);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
}

bool CUpdateProjectileTargetEntity::IsInScope( const netPlayer& playerToCheck) const
{
	if (playerToCheck.IsLocal())
		return false;

	return true;
}

bool CUpdateProjectileTargetEntity::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	if(gnetVerifyf(static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed(), "No ped present for sender player") && gnetVerifyf(static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed()->GetNetworkObject(), "Sender player ped doesn't have network object"))
	{
		PendingRocketTargetUpdate newUpdateEvent;
		newUpdateEvent.m_projectileOwnerId = m_projectileOwnerID;
		newUpdateEvent.m_projectileSequenceId = m_projectileSequenceId;
		newUpdateEvent.m_flareOwnerId = static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed()->GetNetworkObject()->GetObjectID();
		newUpdateEvent.m_targetFlareGunSequenceId = m_targetFlareGunSequenceId;
		newUpdateEvent.m_eventAddedTime = fwTimer::GetTimeInMilliseconds();
		CProjectileManager::AddPendingProjectileTargetUpdateEvent(newUpdateEvent);
	}
	return true;
}

#if ENABLE_NETWORK_LOGGING
void CUpdateProjectileTargetEntity::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	WriteDataToLogFile(bEventLogOnly);
}

void CUpdateProjectileTargetEntity::WriteDataToLogFile(bool LOGGING_ONLY(bEventLogOnly)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject* projectileOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID, true);
	if(projectileOwner)
	{
		log.WriteDataValue("Owner", "%s", projectileOwner->GetLogName());
	}

	log.WriteDataValue("Projectile Sequence ID", "%d", m_projectileSequenceId);
	log.WriteDataValue("Target Flare Gun Sequence ID", "%d", m_targetFlareGunSequenceId);
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// BREAK PROJECTILE TARGET LOCK EVENT
// ===========================================================================================================
CBreakProjectileTargetLock::CBreakProjectileTargetLock() :
	netGameEvent(BREAK_PROJECTILE_TARGET_LOCK_EVENT, false)
{
}

CBreakProjectileTargetLock::CBreakProjectileTargetLock(CNetFXIdentifier   netIdentifier) :
netGameEvent(BREAK_PROJECTILE_TARGET_LOCK_EVENT, false),
	m_netIdentifier(netIdentifier)
{
}

bool CBreakProjectileTargetLock::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == BREAK_PROJECTILE_TARGET_LOCK_EVENT)
	{
		const CBreakProjectileTargetLock* pScriptEvent = SafeCast(const CBreakProjectileTargetLock, pEvent);

		if (pScriptEvent)
		{
			if(pScriptEvent->m_netIdentifier.GetPlayerOwner() == m_netIdentifier.GetPlayerOwner() &&
				pScriptEvent->m_netIdentifier.GetFXId() == m_netIdentifier.GetFXId())
			{
				return true;
			}
		}
	}

	return false;
}

template <class Serialiser> void CBreakProjectileTargetLock::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	
	m_netIdentifier.Serialise<Serialiser>(messageBuffer);
}

void CBreakProjectileTargetLock::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CBreakProjectileTargetLock::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

void CBreakProjectileTargetLock::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CBreakProjectileTargetLock networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CBreakProjectileTargetLock::Trigger(CNetFXIdentifier netIdentifier)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CBreakProjectileTargetLock *pEvent = rage_new CBreakProjectileTargetLock(netIdentifier);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CBreakProjectileTargetLock::IsInScope( const netPlayer& playerToCheck) const
{
	if (playerToCheck.IsLocal())
		return false;

	return true;
}

bool CBreakProjectileTargetLock::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	if(gnetVerifyf(static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed(), "No ped present for sender player") && gnetVerifyf(static_cast<const CNetGamePlayer&>(fromPlayer).GetPlayerPed()->GetNetworkObject(), "Sender player ped doesn't have network object"))
	{
		m_netIdentifier.SetPlayerOwner(fromPlayer.GetPhysicalPlayerIndex());
		CProjectileManager::BreakProjectileHoming(m_netIdentifier);
	}
	return true;
}

#if ENABLE_NETWORK_LOGGING
void CBreakProjectileTargetLock::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	WriteDataToLogFile(bEventLogOnly);
}

void CBreakProjectileTargetLock::WriteDataToLogFile(bool LOGGING_ONLY(bEventLogOnly)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Projectile Identifier", "%d:%d", m_netIdentifier.GetPlayerOwner(), m_netIdentifier.GetFXId());
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// UPDATE PROJECTILE TARGET EVENT
// ===========================================================================================================
CRemoveProjectileEntity::CRemoveProjectileEntity() :
	netGameEvent(REMOVE_PROJECTILE_ENTITY_EVENT, false),
	m_projectileOwnerID    (NETWORK_INVALID_OBJECT_ID),
	m_projectileSequenceId (-1)
{
	// Empty
}

CRemoveProjectileEntity::CRemoveProjectileEntity(const ObjectId     projectileOwnerID,
												 u32                projectilSequenceId) :
netGameEvent(REMOVE_PROJECTILE_ENTITY_EVENT, false),
	m_projectileOwnerID				(projectileOwnerID),
	m_projectileSequenceId			(projectilSequenceId)
{
}

bool CRemoveProjectileEntity::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == REMOVE_PROJECTILE_ENTITY_EVENT)
	{
		const CRemoveProjectileEntity* pScriptEvent = SafeCast(const CRemoveProjectileEntity, pEvent);

		if (pScriptEvent)
		{
			if(pScriptEvent->m_projectileOwnerID == m_projectileOwnerID &&
				pScriptEvent->m_projectileSequenceId == m_projectileSequenceId)
			{
				return true;
			}
		}
	}

	return false;
}

template <class Serialiser> void CRemoveProjectileEntity::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const int NUM_BITS_SEQUENCE_ID = 32;

	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_projectileOwnerID, "Projectile Owner ID");
	SERIALISE_INTEGER(serialiser, m_projectileSequenceId, NUM_BITS_SEQUENCE_ID, "Projectile Sequence ID");
}

void CRemoveProjectileEntity::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoveProjectileEntity::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

void CRemoveProjectileEntity::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRemoveProjectileEntity networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemoveProjectileEntity::Trigger(ObjectId pProjectileId, u32 taskSequence)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CRemoveProjectileEntity *pEvent = rage_new CRemoveProjectileEntity(pProjectileId, taskSequence);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRemoveProjectileEntity::IsInScope( const netPlayer& playerToCheck) const
{
	if (playerToCheck.IsLocal())
		return false;

	return true;
}

bool CRemoveProjectileEntity::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* projectileOwner = NetworkInterface::GetNetworkObject(m_projectileOwnerID);
	if(projectileOwner)
	{
		atArray<RegdProjectile> rocketProjectiles;
		CProjectileManager::GetProjectilesForOwner(projectileOwner->GetEntity(), rocketProjectiles);
		for(int r = 0; r < rocketProjectiles.GetCount(); r++)
		{
			CProjectile* rocket = rocketProjectiles[r]->GetAsProjectile();
			if(rocket && rocket->GetTaskSequenceId() == m_projectileSequenceId)
			{
				rocket->Destroy();
			}
		}
	}
	return true;
}

#if ENABLE_NETWORK_LOGGING
void CRemoveProjectileEntity::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool bEventLogOnly, datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	WriteDataToLogFile(bEventLogOnly);
}

void CRemoveProjectileEntity::WriteDataToLogFile(bool LOGGING_ONLY(bEventLogOnly)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject* projectileOwner = NetworkInterface::GetObjectManager().GetNetworkObject(m_projectileOwnerID, true);
	if(projectileOwner)
	{
		log.WriteDataValue("Owner", "%s", projectileOwner->GetLogName());
	}

	log.WriteDataValue("Projectile Sequence ID", "%d", m_projectileSequenceId);
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// ALTER WANTED LEVEL EVENT
// ===========================================================================================================

CAlterWantedLevelEvent::CAlterWantedLevelEvent() :
netGameEvent( ALTER_WANTED_LEVEL_EVENT, false, true ),
m_pPlayer(NULL),
m_bSetWantedLevel(false),
m_crimeType(CRIME_NONE),
m_newWantedLevel(0),
m_bPoliceDontCare(false),
m_delay(0),
m_causedByPlayerPhysicalIndex(INVALID_PLAYER_INDEX)
{
}

CAlterWantedLevelEvent::CAlterWantedLevelEvent( const netPlayer& player,
                                                u32 newWantedLevel,
												u32 delay,
												PhysicalPlayerIndex causedByPlayerPhysicalIndex) :
netGameEvent( ALTER_WANTED_LEVEL_EVENT, false, true ),
m_pPlayer(&player),
m_bSetWantedLevel(true),
m_crimeType(CRIME_NONE),
m_newWantedLevel(newWantedLevel),
m_bPoliceDontCare(false),
m_delay(delay),
m_causedByPlayerPhysicalIndex(causedByPlayerPhysicalIndex)
{
}

CAlterWantedLevelEvent::CAlterWantedLevelEvent(const netPlayer& player,
                                               eCrimeType crimeType,
                                               bool bPoliceDontCare,
											   PhysicalPlayerIndex causedByPlayerPhysicalIndex) :
netGameEvent( ALTER_WANTED_LEVEL_EVENT, false, true ),
m_pPlayer(&player),
m_bSetWantedLevel(false),
m_crimeType(static_cast<u32>(crimeType)),
m_newWantedLevel(0),
m_bPoliceDontCare(bPoliceDontCare),
m_delay(0),
m_causedByPlayerPhysicalIndex(causedByPlayerPhysicalIndex)
{
}

bool CAlterWantedLevelEvent::IsEqual(const netPlayer& player, eCrimeType crimeType, bool bPoliceDontCare)
{
    if(&player                                   == m_pPlayer  &&
       static_cast<u32>(crimeType)               == m_crimeType &&
       bPoliceDontCare                           == m_bPoliceDontCare &&
       0                                         == m_newWantedLevel)
    {
        return true;
    }

    return false;
}

bool CAlterWantedLevelEvent::IsEqual(const netPlayer& player, u32 newWantedLevel)
{
    if(&player                                   == m_pPlayer  &&
       static_cast<u32>(CRIME_NONE)              == m_crimeType &&
       false                                     == m_bPoliceDontCare &&
       newWantedLevel                            == m_newWantedLevel)
    {
        return true;
    }

    return false;
}

void CAlterWantedLevelEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CAlterWantedLevelEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CAlterWantedLevelEvent::Trigger(const netPlayer& player, u32 newWantedLevel, s32 delay, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    NetworkInterface::GetEventManager().CheckForSpaceInPool();

    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        atDNode<netGameEvent*, datBase> *nextNode = node->GetNext();

        netGameEvent *currentEvent = node->Data;

        if(currentEvent && currentEvent->GetEventType() == ALTER_WANTED_LEVEL_EVENT  && !currentEvent->IsFlaggedForRemoval())
        {
            CAlterWantedLevelEvent *alterWantedLevelEvent = static_cast<CAlterWantedLevelEvent *>(currentEvent);

            if(alterWantedLevelEvent->IsEqual(player, newWantedLevel))
            {
                return;
            }
            else if(alterWantedLevelEvent->GetPlayer() == &player)
            {
                if(!alterWantedLevelEvent->IsCrimeCommittedEvent())
                {
                    // destroy the event
                    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "DESTROYING_EVENT", "%s_%d\r\n", NetworkInterface::GetEventManager().GetEventNameFromType(alterWantedLevelEvent->GetEventType()), alterWantedLevelEvent->GetEventId().GetID());
                    alterWantedLevelEvent->GetEventId().ClearPlayersInScope();
                }
            }
        }

        node = nextNode;
    }

	wantedDebugf3("CAlterWantedLevelEvent::Trigger toPlayer[%s] toPlayerPed[%p] newWantedLevel[%d] delay[%d]",static_cast<const CNetGamePlayer &>(player).GetGamerInfo().GetName(),static_cast<const CNetGamePlayer &>(player).GetPlayerPed(),newWantedLevel,delay);
	u32 uDelay = (delay >= 0) ? (u32) delay : 0;
    CAlterWantedLevelEvent *pEvent = rage_new CAlterWantedLevelEvent(player, newWantedLevel, uDelay, causedByPlayerPhysicalIndex);

    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CAlterWantedLevelEvent::Trigger(const netPlayer& player, eCrimeType crimeType, bool bPoliceDontReallyCare, PhysicalPlayerIndex causedByPlayerPhysicalIndex)
{
    gnetAssert(NetworkInterface::IsGameInProgress());
    NetworkInterface::GetEventManager().CheckForSpaceInPool();

    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        atDNode<netGameEvent*, datBase> *nextNode = node->GetNext();

        netGameEvent *currentEvent = node->Data;

        if(currentEvent && currentEvent->GetEventType() == ALTER_WANTED_LEVEL_EVENT && !currentEvent->IsFlaggedForRemoval())
        {
            CAlterWantedLevelEvent *alterWantedLevelEvent = static_cast<CAlterWantedLevelEvent *>(currentEvent);

            if(alterWantedLevelEvent->IsEqual(player, crimeType, bPoliceDontReallyCare))
            {
                return;
            }
            else if(alterWantedLevelEvent->GetPlayer() == &player)
            {
                if(alterWantedLevelEvent->IsCrimeCommittedEvent())
                {
                    // destroy the event
                    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "DESTROYING_EVENT", "%s_%d\r\n", NetworkInterface::GetEventManager().GetEventNameFromType(alterWantedLevelEvent->GetEventType()), alterWantedLevelEvent->GetEventId().GetID());
                    alterWantedLevelEvent->GetEventId().ClearPlayersInScope();
                }
            }
        }

        node = nextNode;
    }

	wantedDebugf3("CAlterWantedLevelEvent::Trigger toPlayer[%s] toPlayerPed[%p] crimeType[%d] bPoliceDontReallyCare[%d]",static_cast<const CNetGamePlayer &>(player).GetGamerInfo().GetName(),static_cast<const CNetGamePlayer &>(player).GetPlayerPed(),crimeType,bPoliceDontReallyCare);
    CAlterWantedLevelEvent *pEvent = rage_new CAlterWantedLevelEvent(player, crimeType, bPoliceDontReallyCare, causedByPlayerPhysicalIndex);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CAlterWantedLevelEvent::IsInScope( const netPlayer& player ) const
{
    return (&player == m_pPlayer);
}

template <class Serialiser> void CAlterWantedLevelEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	static const unsigned int SIZEOF_DELAY = 16;

    Serialiser serialiser(messageBuffer);

    SERIALISE_BOOL(serialiser, m_bSetWantedLevel);

    if (m_bSetWantedLevel)
    {
       SERIALISE_UNSIGNED(serialiser, m_newWantedLevel, SIZEOF_WANTEDLEVEL, "New Wanted Level");

	   SERIALISE_UNSIGNED(serialiser, m_causedByPlayerPhysicalIndex, 8, "WL Caused By");	   
	   bool bHasDelay = (m_delay != 0);
	   SERIALISE_BOOL(serialiser, bHasDelay, "Has Delay");
	   if (bHasDelay)
	   {
		   SERIALISE_UNSIGNED(serialiser,m_delay,SIZEOF_DELAY,"Delay");
	   }
	   else
	   {
		   m_delay = 0;
	   }
    }
    else
    {
       SERIALISE_UNSIGNED(serialiser, m_crimeType, SIZEOF_CRIMETYPE, "Crime Type");
       SERIALISE_BOOL(serialiser, m_bPoliceDontCare, "Police Don't Care");
    }
}

void CAlterWantedLevelEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CAlterWantedLevelEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CAlterWantedLevelEvent::Decide(const netPlayer &OUTPUT_ONLY(fromPlayer), const netPlayer &toPlayer)
{
	CPed* pPlayerPed = static_cast<const CNetGamePlayer&>(toPlayer).GetPlayerPed();
    CPlayerInfo* pPlayer = pPlayerPed ? pPlayerPed->GetPlayerInfo() : NULL;

    if ( pPlayer && (pPlayer->GetPlayerState() != CPlayerInfo::PLAYERSTATE_LEFTGAME) )
    {
		// Don't allow wanted level set from event if temporarily invincible after being re-spawned (lavalley)
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(pPlayerPed->GetNetworkObject());
		if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
		{
			wantedDebugf3("CAlterWantedLevelEvent::Decide fromPlayer[%s] --> m_bSetWantedLevel[%d] m_newWantedLevel[%d] -- disregard player just respawned GetRespawnInvicibilityTimer[%d] --> return true",static_cast<const CNetGamePlayer &>(fromPlayer).GetGamerInfo().GetName(),m_bSetWantedLevel,m_newWantedLevel,netObjPlayer->GetRespawnInvincibilityTimer());
			return true;
		}

		const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
        if (m_bSetWantedLevel)
        {
			wantedDebugf3("CAlterWantedLevelEvent::Decide fromPlayer[%s] --> invoke SetWantedLevelNoDrop m_newWantedLevel[%d] m_delay[%u] WL_FROM_NETWORK",static_cast<const CNetGamePlayer &>(fromPlayer).GetGamerInfo().GetName(),m_newWantedLevel,m_delay);
            pPlayerPed->GetPlayerWanted()->SetWantedLevelNoDrop(vPlayerPosition, m_newWantedLevel, (s32) m_delay, WL_FROM_NETWORK, true, m_causedByPlayerPhysicalIndex);
        }
        else
        {
			wantedDebugf3("CAlterWantedLevelEvent::Decide fromPlayer[%s] --> invoke ReportCrimeNow m_crimeType[%d] m_bPoliceDontCare[%d]",static_cast<const CNetGamePlayer &>(fromPlayer).GetGamerInfo().GetName(),m_crimeType,m_bPoliceDontCare);
            pPlayerPed->GetPlayerWanted()->ReportCrimeNow(static_cast<eCrimeType>(m_crimeType), vPlayerPosition, m_bPoliceDontCare, false, 0, NULL, m_causedByPlayerPhysicalIndex);
        }
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CAlterWantedLevelEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    if (m_bSetWantedLevel)
    {
        log.WriteDataValue("New wanted level", "%d", m_newWantedLevel);
    }
    else
    {
        log.WriteDataValue("Crime", "%d", m_crimeType);
    }
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// CHANGE RADIO STATION EVENT
// ===========================================================================================================

CChangeRadioStationEvent::CChangeRadioStationEvent() :
netGameEvent( CHANGE_RADIO_STATION_EVENT, false, false ),
m_vehicleId(0),
m_stationId(0)
{
}

CChangeRadioStationEvent::CChangeRadioStationEvent( const ObjectId vehicleId, const u8 stationIndex ) :
netGameEvent( CHANGE_RADIO_STATION_EVENT, false, false ),
m_vehicleId(vehicleId),
m_stationId(stationIndex)
{
}

void CChangeRadioStationEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CChangeRadioStationEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CChangeRadioStationEvent::Trigger(const CVehicle* pVehicle, const u8 stationIndex)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CChangeRadioStationEvent *pEvent = rage_new CChangeRadioStationEvent(pVehicle->GetNetworkObject()->GetObjectID(),stationIndex);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CChangeRadioStationEvent::IsInScope( const netPlayer& player ) const
{ 
	if (player.IsLocal())
		return false;

	netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);

	CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);

	// send to owner of the vehicle
	if (vehicle)
	{
		return (&player == pNetObj->GetPlayerOwner());
	}

	return false;
}

template <class Serialiser> void CChangeRadioStationEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_vehicleId, "Vehicle ID");
	SERIALISE_UNSIGNED(serialiser, m_stationId, 8, "Station ID");
}

void CChangeRadioStationEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CChangeRadioStationEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CChangeRadioStationEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);
	CVehicle *pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);

	if (pVehicle && pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->SetRadioStationFromNetwork(m_stationId, static_cast<const CNetGamePlayer &>(fromPlayer).GetPlayerPed() == pVehicle->GetDriver());
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CChangeRadioStationEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);
	CVehicle* vehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);
	if (vehicle)
	{
		log.WriteDataValue("Vehicle", "%s", pNetObj->GetLogName());
	}
	else
	{
		log.WriteDataValue("Vehicle", "?_%d", m_vehicleId);
	}

	log.WriteDataValue("Station ID", "%d", m_stationId);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// RAGDOLL REQUEST EVENT
// ===========================================================================================================

CRagdollRequestEvent::CRagdollRequestEvent() :
netGameEvent( RAGDOLL_REQUEST_EVENT, false, false )
, m_pedId(NETWORK_INVALID_OBJECT_ID)
, m_bSwitchedToRagdoll(false)
{
}

CRagdollRequestEvent::CRagdollRequestEvent( const ObjectId pedId ) :
netGameEvent( RAGDOLL_REQUEST_EVENT, false, false ),
m_pedId(pedId),
m_bSwitchedToRagdoll(false)
{
}

void CRagdollRequestEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRagdollRequestEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRagdollRequestEvent::Trigger(const ObjectId pedID)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CRagdollRequestEvent *pEvent = rage_new CRagdollRequestEvent(pedID);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRagdollRequestEvent::IsInScope( const netPlayer& player ) const
{
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedId);
    CPed *ped = NetworkUtils::GetPedFromNetworkObject(pPedObj);

    return (ped && player.GetPhysicalPlayerIndex() == pPedObj->GetPhysicalPlayerIndex());
}

template <class Serialiser> void CRagdollRequestEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_pedId, "Ped ID");
}

void CRagdollRequestEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRagdollRequestEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRagdollRequestEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedId);
    CPed *ped = NetworkUtils::GetPedFromNetworkObject(pPedObj);

    if (ped && !pPedObj->IsClone() && !pPedObj->IsPendingOwnerChange() && ped->GetHealth() > 0.0f)
    {
        if (ped->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(ped, RAGDOLL_TRIGGER_NETWORK))
        {
			CTaskNMControl* pNMTask = static_cast<CTaskNMControl*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
			CTask* pSubTask = pNMTask ? pNMTask->GetSubTask() : NULL;

			// If we're running an NM control task with a valid sub-task then we don't need to force a switch to ragdoll
			// as it means we're running an NM behavior task, a rage ragdoll task or a blend-from-NM task
			if (pSubTask && CTaskNMControl::IsValidNMControlSubTask(pSubTask))
			{
				if (pSubTask->IsNMBehaviourTask())
				{
					static_cast<CTaskNMBehaviour*>(pSubTask)->ForceFallOver();
				}
			}
			else
			{
				CTask* pTaskNM = rage_new CTaskNMRelax(1000, 2000);
				CEventSwitch2NM event(3000, pTaskNM);

				if(ped->SwitchToRagdoll(event))
				{
					m_bSwitchedToRagdoll = true;
				}
			}
        }
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CRagdollRequestEvent::WriteEventToLogFile(bool LOGGING_ONLY(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pPedObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pedId);
    CPed* ped = NetworkUtils::GetPedFromNetworkObject(pPedObj);
    if (pPedObj)
        log.WriteDataValue("Ped", "%s", pPedObj->GetLogName());
    else
    {
        log.WriteDataValue("Ped", "?_%d", m_pedId);
    }

    if (!wasSent && ped)
    {
      if (m_bSwitchedToRagdoll)
          log.WriteDataValue("Action", "Success - ped given relax event");
      else if (pPedObj->IsClone())
          log.WriteDataValue("Action", "Failed - ped is a clone");
      else if (pPedObj->IsPendingOwnerChange())
          log.WriteDataValue("Action", "Failed - ped migrating");
      else if (ped->GetHealth() <= 0.0f)
          log.WriteDataValue("Action", "Failed - ped is dead");
      else if (!ped->GetUsingRagdoll())
          log.WriteDataValue("Action", "Failed - ped can't ragdoll");
      else
          log.WriteDataValue("Action", "Failed - ped couldn't process event");
    }
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// PLAYER TAUNT EVENT
// ===========================================================================================================

CPlayerTauntEvent::CPlayerTauntEvent() :
netGameEvent( PLAYER_TAUNT_EVENT, false, false ),
m_tauntVariationNumber(0)
{
    m_context[0]='\0';
}

CPlayerTauntEvent::CPlayerTauntEvent( const char* context, s32 tauntVariationNumber) :
netGameEvent( PLAYER_TAUNT_EVENT, false, false ),
m_tauntVariationNumber(tauntVariationNumber)
{
    gnetAssert(context && strlen(context) < MAX_CONTEXT_LENGTH);

    m_context[0]='\0';

    if(context && strlen(context) < MAX_CONTEXT_LENGTH)
    {
        safecpy(m_context, context, MAX_CONTEXT_LENGTH);
    }
}

void CPlayerTauntEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CPlayerTauntEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPlayerTauntEvent::Trigger(const char* context, s32 tauntVariationNumber)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CPlayerTauntEvent *pEvent = rage_new CPlayerTauntEvent(context, tauntVariationNumber);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CPlayerTauntEvent::IsInScope( const netPlayer& playerToCheck ) const
{
    static const float TAUNT_RANGE_SQ = 200.0f * 200.0f;

    bool inScope = false;

    if(playerToCheck.IsRemote() && !playerToCheck.IsBot())
    {
        // don't send to players far away from the taunt
        if (FindPlayerPed())
        {
            gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
            const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

            if(player.GetPlayerPed())
            {
                float distanceFromTauntSq = DistSquared(player.GetPlayerPed()->GetTransform().GetPosition(), FindPlayerPed()->GetTransform().GetPosition()).Getf();

                inScope = (distanceFromTauntSq <= TAUNT_RANGE_SQ);
            }
        }
    }

    return inScope;
}

template <class Serialiser> void CPlayerTauntEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_STRING(serialiser, m_context, MAX_CONTEXT_LENGTH, "Context");
    SERIALISE_UNSIGNED(serialiser, m_tauntVariationNumber, SIZEOF_TAUNT_VARIATION, "Taunt Variation");
}

void CPlayerTauntEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPlayerTauntEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPlayerTauntEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    gnetAssert(dynamic_cast<const CNetGamePlayer *>(&fromPlayer));
    const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(fromPlayer);

    if (player.GetPlayerPed()->GetSpeechAudioEntity())
    {
		//player.GetPlayerPed()->GetSpeechAudioEntity()->Say(const_cast<char*>(m_context), true, true, 0, -1, 0, 0, 1.0f, 0, m_tauntVariationNumber);
		audSpeechInitParams speechParams;
		speechParams.forcePlay = true;
		speechParams.allowRecentRepeat = true;
		speechParams.requestedVolume = 0;
		player.GetPlayerPed()->GetSpeechAudioEntity()->Say(const_cast<char*>(m_context), speechParams, 0, -1, 0, 0, -1, 1.0f, &m_tauntVariationNumber);
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CPlayerTauntEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("Context", "%s", m_context);
    log.WriteDataValue("Variation", "%d", m_tauntVariationNumber);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// PLAYER STAT EVENT
// ===========================================================================================================
CPlayerCardStatEvent::CPlayerCardStatEvent():
netGameEvent( PLAYER_CARD_STAT_EVENT, false, true)
{
	memset(m_stats, 0, sizeof(m_stats));
}

CPlayerCardStatEvent::CPlayerCardStatEvent(const float data[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT]):
netGameEvent( PLAYER_CARD_STAT_EVENT, false, true)
{
	memcpy(m_stats, data, sizeof(m_stats));
}

void CPlayerCardStatEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CPlayerCardStatEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPlayerCardStatEvent::Trigger(const float data[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT])
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPlayerCardStatEvent *pEvent = rage_new CPlayerCardStatEvent(data);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser>
void CPlayerCardStatEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	//Needs the number of bits for this data array.
	SERIALISE_DATABLOCK(serialiser, (u8*)m_stats, sizeof(m_stats)*8, "Stats");
}

void CPlayerCardStatEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer& UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPlayerCardStatEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer& UNUSED_PARAM(fromPlayer), const netPlayer& UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPlayerCardStatEvent::Decide (const netPlayer& fromPlayer, const netPlayer& UNUSED_PARAM(toPlayer))
{
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(fromPlayer);
	CNetObjPlayer *netObjPlayer = player.GetPlayerPed() ? static_cast<CNetObjPlayer*>(player.GetPlayerPed()->GetNetworkObject()) : NULL;
	if(netObjPlayer)
	{
		netObjPlayer->ReceivedStatEvent(m_stats);
	}

	return true;
}

// ===========================================================================================================
// PED CONVERSATION LINE EVENT
// ===========================================================================================================

CPedConversationLineEvent::CPedConversationLineEvent() :
netGameEvent( PED_CONVERSATION_LINE_EVENT, false, false ),
m_TargetPedId( NETWORK_INVALID_OBJECT_ID ),
m_voiceNameHash(0),
m_variationNumber(0),
m_playscriptedspeech(false),
m_speechParamsString(false)
{
	m_context[0]='\0';
	m_speechParams[0]='\0';
}

CPedConversationLineEvent::CPedConversationLineEvent( CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, bool playscriptedspeech) :
netGameEvent( PED_CONVERSATION_LINE_EVENT, false, false ),
m_voiceNameHash(voiceNameHash),
m_variationNumber(variationNumber),
m_playscriptedspeech(playscriptedspeech),
m_speechParamsString(false)
{
	m_TargetPedId = (ped && ped->GetNetworkObject()) ? ped->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
	gnetAssert(NETWORK_INVALID_OBJECT_ID != m_TargetPedId);

    gnetAssertf(context && strlen(context) < MAX_CONTEXT_LENGTH, "context %s strlen %" SIZETFMT "d > MAX_CONTEXT_LENGTH %d", context ? context : "NULL", context ? strlen(context) : 0, MAX_CONTEXT_LENGTH);

    m_context[0]='\0';
	m_speechParams[0]='\0';

    if(context && strlen(context) < MAX_CONTEXT_LENGTH)
    {
        safecpy(m_context, context, MAX_CONTEXT_LENGTH);
    }
}

CPedConversationLineEvent::CPedConversationLineEvent(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, const char* speechParams) :
netGameEvent( PED_CONVERSATION_LINE_EVENT, false, false ),
m_voiceNameHash(voiceNameHash),
m_variationNumber(variationNumber),
m_playscriptedspeech(false),
m_speechParamsString(true)
{
	m_TargetPedId = (ped && ped->GetNetworkObject()) ? ped->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
	gnetAssert(NETWORK_INVALID_OBJECT_ID != m_TargetPedId);

	gnetAssertf(context && strlen(context) < MAX_CONTEXT_LENGTH, "context %s strlen %" SIZETFMT "d > MAX_CONTEXT_LENGTH %d", context ? context : "NULL", context ? strlen(context) : 0, MAX_CONTEXT_LENGTH);
	gnetAssertf(speechParams && strlen(speechParams) < MAX_SPEECH_PARAMS_LENGTH, "speechParams %s strlen %" SIZETFMT "d > MAX_SPEECH_PARAMS_LENGTH %d", speechParams ? speechParams : "NULL", speechParams ? strlen(speechParams) : 0, MAX_SPEECH_PARAMS_LENGTH);

	m_context[0]='\0';
	m_speechParams[0]='\0';

	if(context && strlen(context) < MAX_CONTEXT_LENGTH)
	{
		safecpy(m_context, context, MAX_CONTEXT_LENGTH);
	}

	if(speechParams && strlen(speechParams) < MAX_SPEECH_PARAMS_LENGTH)
	{
		safecpy(m_speechParams, speechParams, MAX_SPEECH_PARAMS_LENGTH);
	}
}

void CPedConversationLineEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CPedConversationLineEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPedConversationLineEvent::Trigger(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, bool playscriptedspeech)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());
    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CPedConversationLineEvent *pEvent = rage_new CPedConversationLineEvent(ped, voiceNameHash, context, variationNumber, playscriptedspeech);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CPedConversationLineEvent::Trigger(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, const char* speechParams)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPedConversationLineEvent *pEvent = rage_new CPedConversationLineEvent(ped, voiceNameHash, context, variationNumber, speechParams);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CPedConversationLineEvent::IsInScope( const netPlayer& playerToCheck ) const
{
    static const float RANGE_SQ = 20.0f * 20.0f;
	static const float RANGE_SQ_EXTENDED = 50.0f * 50.0f;	

    bool inScope = false;

    if(playerToCheck.IsRemote() && !playerToCheck.IsBot())
    {
		if(NETWORK_INVALID_OBJECT_ID != m_TargetPedId)
		{
			CPed* ped = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_TargetPedId));
			if(ped)
			{
				// don't send to players far away from the taunt
				const CNetGamePlayer& gamePlayer = static_cast<const CNetGamePlayer&>(playerToCheck);
				if (gamePlayer.GetPlayerPed())
				{
					//Check if we are spectating
					CNetObjPlayer* playerNetObj = SafeCast(CNetObjPlayer, gamePlayer.GetPlayerPed()->GetNetworkObject());
					CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
					CNetObjPlayer* myPlayerNetObj = pLocalPlayer ? SafeCast(CNetObjPlayer, pLocalPlayer->GetNetworkObject()) : NULL;

					// if we are being spectated by this player, always allow speech to be heard on his machine
					if (playerNetObj && playerNetObj->IsSpectating() && playerNetObj->GetSpectatorObjectId() == myPlayerNetObj->GetObjectID())
					{
						return true;
					}

					float distanceSq = DistSquared(ped->GetTransform().GetPosition(), gamePlayer.GetPlayerPed()->GetTransform().GetPosition()).Getf();

					if(distanceSq <= RANGE_SQ_EXTENDED)
					{
						SpeechParams* pSpeechParams = audNorthAudioEngine::GetObject<SpeechParams>(m_speechParams);
						SpeechRequestedVolumeType requestedVolume = pSpeechParams ? (SpeechRequestedVolumeType)pSpeechParams->RequestedVolume : SPEECH_VOLUME_UNSPECIFIED;

						if(requestedVolume == SPEECH_VOLUME_FRONTEND || requestedVolume == SPEECH_VOLUME_MEGAPHONE)
						{
							inScope = true;
						}
						else
						{
							inScope = distanceSq <= RANGE_SQ;
						}
					}

					audDisplayf("CPedConversationLineEvent context: %s voice: %u params %s - player %s in scope? %s (%.02fm away)", m_context, m_voiceNameHash, m_speechParams, gamePlayer.GetLogName(), inScope ? "true" : "false", SqrtfSafe(distanceSq));
				}
			}
		}
	}

    return inScope;
}

template <class Serialiser> void CPedConversationLineEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_TargetPedId, "Target Ped ID");
    SERIALISE_STRING(serialiser, m_context, MAX_CONTEXT_LENGTH, "Context");
	SERIALISE_UNSIGNED(serialiser, m_voiceNameHash,	SIZEOF_VOICE_NAME_HASH, "Voice Name Hash");
	SERIALISE_UNSIGNED(serialiser, m_variationNumber, SIZEOF_LINE_VARIATION, "Line Variation");
	SERIALISE_BOOL(serialiser, m_speechParamsString);

	if (m_speechParamsString)
	{
		SERIALISE_STRING(serialiser, m_speechParams, MAX_SPEECH_PARAMS_LENGTH, "Speech Params");
	}
	else
	{
		SERIALISE_BOOL(serialiser, m_playscriptedspeech, "Scripted Speech");
	}
}

void CPedConversationLineEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPedConversationLineEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPedConversationLineEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	if(NETWORK_INVALID_OBJECT_ID != m_TargetPedId)
	{
		CPed* ped = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_TargetPedId));
		if(ped)
		{
			if (m_playscriptedspeech)
			{
				if(ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying())
				{
					ped->GetSpeechAudioEntity()->StopPlayingScriptedSpeech();
				}

				ped->GetSpeechAudioEntity()->PlayScriptedSpeech("", 
					const_cast<char*>(m_context), 
					m_variationNumber,
					NULL,
					AUD_SPEECH_NORMAL,
					true,
					0,
					ORIGIN,
					ped,
					m_voiceNameHash
					);
			}
			else if (m_speechParamsString)
			{
				if (m_voiceNameHash == ATSTRINGHASH("Mask_SFX", 0x36179520))
				{
					ped->GetSpeechAudioEntity()->Say(const_cast<char*>(m_context), const_cast<char*>(m_speechParams), m_voiceNameHash, -1, NULL, 0, -1, 1.0f, true, &m_variationNumber);
				}
				else
				{
					ped->GetSpeechAudioEntity()->SetAmbientVoiceName(m_voiceNameHash, true);
					ped->GetSpeechAudioEntity()->Say(const_cast<char*>(m_context), const_cast<char*>(m_speechParams), 0, -1, NULL, 0, -1, 1.0f, true, &m_variationNumber);
				}
			}
			else
			{
				audSpeechInitParams speechParams;
				speechParams.forcePlay = true;
				speechParams.allowRecentRepeat = true;
				speechParams.requestedVolume = 0;
				ped->GetSpeechAudioEntity()->Say(const_cast<char*>(m_context), speechParams, m_voiceNameHash, -1, 0, 0, -1, 1.0f, &m_variationNumber);
			}
		}
	}

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CPedConversationLineEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Target Ped ID", "%d", m_TargetPedId);
	log.WriteDataValue("Voice Name Hash", "%d", m_voiceNameHash);
	log.WriteDataValue("Context", "%s", m_context);
    log.WriteDataValue("Variation", "%d", m_variationNumber);
	log.WriteDataValue("Play Scripted Speech", "%d", m_playscriptedspeech);
}

#endif //ENABLE_NETWORK_LOGGING


// ===========================================================================================================
// DOOR BREAK EVENT
// ===========================================================================================================

CDoorBreakEvent::CDoorBreakEvent() :
netGameEvent( DOOR_BREAK_EVENT, false, false ),
m_vehicleId(NETWORK_INVALID_OBJECT_ID),
m_door(0)
{
}

CDoorBreakEvent::CDoorBreakEvent( const ObjectId vehicleId, u32 door) :
netGameEvent( DOOR_BREAK_EVENT, false, false ),
m_vehicleId(vehicleId),
m_door(static_cast<u8>(door))
{

}

void CDoorBreakEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CDoorBreakEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CDoorBreakEvent::Trigger(const CVehicle* pVehicle, const CCarDoor* pDoor)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    if (AssertVerify(pVehicle && pVehicle->GetNetworkObject()))
    {
        u32 door = 0;
        int i=0;

        for (i=0; i<pVehicle->GetNumDoors(); i++)
        {
            if (pVehicle->GetDoor(i) == pDoor)
            {
                door = i;
                break;
            }
        }

        if (AssertVerify(i < pVehicle->GetNumDoors()))
        {
            NetworkInterface::GetEventManager().CheckForSpaceInPool();
            CDoorBreakEvent *pEvent = rage_new CDoorBreakEvent(pVehicle->GetNetworkObject()->GetObjectID(), door);
            NetworkInterface::GetEventManager().PrepareEvent(pEvent);
        }
    }
}

bool CDoorBreakEvent::IsInScope( const netPlayer& player ) const
{
    netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);

    CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);

    // send to owner of the vehicle
    if (vehicle)
    {
        return (&player == pNetObj->GetPlayerOwner());
    }

    return false;
}

template <class Serialiser> void CDoorBreakEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_vehicleId, "Vehicle ID");
    SERIALISE_UNSIGNED(serialiser, m_door, SIZEOF_DOOR, "Door");
}

void CDoorBreakEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CDoorBreakEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CDoorBreakEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);
    CVehicle *pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);

    // break off the door if not in use
    if (pVehicle && pVehicle->GetDoor(m_door) && pVehicle->GetDoor(m_door)->GetIsIntact(pVehicle))
    {
        CComponentReservation* pComponent = pVehicle->GetComponentReservationMgr()->FindComponentReservation(pVehicle->GetDoor(m_door)->GetHierarchyId(), false);

        if (!pComponent || !pComponent->IsComponentInUse())
        {
            pVehicle->GetDoor(m_door)->BreakOff(pVehicle);
        }
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CDoorBreakEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleId);
    CVehicle* vehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);
    if (vehicle)
      log.WriteDataValue("Vehicle", "%s", pNetObj->GetLogName());
    else
    {
        log.WriteDataValue("Vehicle", "?_%d", m_vehicleId);
    }

    log.WriteDataValue("Door", "%d", m_door);
}

#endif //ENABLE_NETWORK_LOGGING


// ===========================================================================================================
// SCRIPTED GAME EVENT
// ===========================================================================================================

CScriptedGameEvent::sEventHistoryEntry CScriptedGameEvent::ms_eventHistory[CScriptedGameEvent::HISTORY_QUEUE_SIZE];
u32 CScriptedGameEvent::ms_numEventsInHistory = 0;

u32 CScriptedGameEvent::ms_validCallingScriptsHashes[] = 
{
	ATSTRINGHASH("act_cinema", 0x8961d933),
	ATSTRINGHASH("apparcadebusiness", 0x4BA16A46),
	ATSTRINGHASH("appfixersecurity", 0x0484D815),
	ATSTRINGHASH("arcadeendscreen", 0xD55288C2),
	ATSTRINGHASH("arcadelobby", 0xC6077DE9),
	ATSTRINGHASH("arcadelobbyui", 0xD8297C4E),
	ATSTRINGHASH("arcade_lobby_preview", 0xDCED49A7),
	ATSTRINGHASH("arcade_seating", 0xA0B1239B),
	ATSTRINGHASH("blackjack", 0xD378365E),
	ATSTRINGHASH("carwash1", 0x9f4e8127),
	ATSTRINGHASH("carwash2", 0xb618aebb),
	ATSTRINGHASH("casino_exterior_seating", 0x38C0DADF),
	ATSTRINGHASH("casino_slots", 0x5f1459d7),
	ATSTRINGHASH("casinoroulette", 0x05e86d0d),
	ATSTRINGHASH("am_agency_suv", 0x12117cf0),
	ATSTRINGHASH("am_airstrike", 0x060cd733),
	ATSTRINGHASH("am_ammo_drop", 0xa7c6571d),
	ATSTRINGHASH("am_armwrestling", 0x3c77f4aa),
	ATSTRINGHASH("am_armwrestling_apartment", 0x69bdb2d8),
	ATSTRINGHASH("am_backup_heli", 0xddb3aada),
	ATSTRINGHASH("am_boat_taxi", 0xd8133249),
	ATSTRINGHASH("am_bru_box", 0x48600c01),
	ATSTRINGHASH("am_casino_limo", 0x3A5BBA05),
	ATSTRINGHASH("am_casino_luxury_car", 0xb93135a6),
	ATSTRINGHASH("am_casino_peds", 0x1C0F96E7),
	ATSTRINGHASH("am_challenges", 0x59de0357),
	ATSTRINGHASH("am_contact_requests", 0xb3920707),
	ATSTRINGHASH("am_cp_collection", 0x0544b90b),
	ATSTRINGHASH("am_cr_securityvan", 0xb121a990),
	ATSTRINGHASH("am_crate_drop", 0x240f2e7b),
	ATSTRINGHASH("am_criminal_damage", 0xc62526f5),
	ATSTRINGHASH("am_darts", 0x2a10cf0c),
	ATSTRINGHASH("am_darts_apartment", 0xab14f1f2),
	ATSTRINGHASH("am_dead_drop", 0x9497de5d),
	ATSTRINGHASH("am_destroy_veh", 0xeab28126),
	ATSTRINGHASH("am_distract_cops", 0x81dc676e),
	ATSTRINGHASH("am_ferriswheel", 0xe4e0cbce),
	ATSTRINGHASH("am_ga_pickups", 0xa77e5305),
	ATSTRINGHASH("am_gang_call", 0x0266ce78),
	ATSTRINGHASH("am_heist_int", 0xc74cc971),
	ATSTRINGHASH("am_heli_taxi", 0x4840fef0),
	ATSTRINGHASH("am_hold_up", 0x0c6dac46),
	ATSTRINGHASH("am_hot_property", 0x95335193),
	ATSTRINGHASH("am_hot_target", 0x3c04751e),
	ATSTRINGHASH("am_hunt_the_beast", 0x7e76f859),
	ATSTRINGHASH("am_imp_exp", 0x1961b859),
	ATSTRINGHASH("am_joyrider", 0xe1bc7b99),
	ATSTRINGHASH("am_kill_list", 0xa8d5e84c),
	ATSTRINGHASH("am_king_of_the_castle", 0x7a9a3349),
	ATSTRINGHASH("am_launcher", 0x57c919e1),
	ATSTRINGHASH("am_luxury_showroom", 0xf4ab6fb5),
	ATSTRINGHASH("am_mission_launch", 0x8fa27332),
	ATSTRINGHASH("am_mp_arcade", 0x500b732f),	
	ATSTRINGHASH("am_mp_arcade_claw_crane", 0xA54F9693),
	ATSTRINGHASH("am_mp_arcade_fortune_teller", 0xF4C6D398),
	ATSTRINGHASH("am_mp_arcade_love_meter", 0x39245013),
	ATSTRINGHASH("am_mp_arcade_peds", 0x5e02e823),	 
	ATSTRINGHASH("am_mp_arcade_strength_test", 0x7488FE33),
	ATSTRINGHASH("am_mp_arc_cab_manager", 0x4EE41F75),
	ATSTRINGHASH("am_mp_arena_box", 0x8C9C5FB8),
	ATSTRINGHASH("am_mp_arena_garage", 0x26EBCE20), 
	ATSTRINGHASH("am_mp_armory_aircraft", 0x9c6f5716),
	ATSTRINGHASH("am_mp_armory_truck", 0x5b9b0272),
	ATSTRINGHASH("am_mp_auto_shop", 0xba18b4c),
	ATSTRINGHASH("am_mp_biker_warehouse", 0x504ebb57),
	ATSTRINGHASH("am_mp_bunker", 0x89d486f8),
	ATSTRINGHASH("am_mp_business_hub", 0xC85B5BEF),
	ATSTRINGHASH("am_mp_car_meet_property", 0x2385bdc4),
	ATSTRINGHASH("am_mp_car_meet_sandbox", 0x5158A9E4),
	ATSTRINGHASH("am_mp_casino", 0x7166BD4A),
	ATSTRINGHASH("am_mp_casino_apartment", 0xD76D873D),
	ATSTRINGHASH("am_mp_casino_nightclub", 0x700869D),
	ATSTRINGHASH("am_mp_casino_valet_garage", 0x6CC29C20),
	ATSTRINGHASH("am_mp_creator_trailer", 0xcbd05bfb),
	ATSTRINGHASH("am_mp_defunct_base", 0xcab8a943),
	ATSTRINGHASH("AM_MP_DRONE", 0x230D1E0D),
	ATSTRINGHASH("am_mp_fixer_hq", 0x995f4ea5),
	ATSTRINGHASH("am_mp_garage_control", 0xfd4b7439),
	ATSTRINGHASH("AM_MP_HACKER_TRUCK", 0x349B7463),
	ATSTRINGHASH("AM_MP_HANGAR", 0xefc0d4ec),
	ATSTRINGHASH("am_mp_ie_warehouse", 0xb7a8f8db),
	ATSTRINGHASH("am_mp_island", 0x1C2A2AF8),
	ATSTRINGHASH("AM_MP_JUGGALO_HIDEOUT", 0xedfd243e),
	ATSTRINGHASH("AM_MP_MULTISTOREY_GARAGE", 0x679a06a1),
	ATSTRINGHASH("am_mp_music_studio", 0xfd3397ae),
	ATSTRINGHASH("AM_MP_NIGHTCLUB", 0x8C56B7FE),
	ATSTRINGHASH("am_mp_peds", 0xBC350B6D),
	ATSTRINGHASH("am_mp_property_ext", 0x72ee09f2),
	ATSTRINGHASH("am_mp_property_int", 0x95b61b3c),
    ATSTRINGHASH("am_mp_rc_vehicle", 0xd1894172),
	ATSTRINGHASH("am_mp_shooting_range", 0xaa3368fc),
	ATSTRINGHASH("am_mp_simeon_showroom", 0x1ed5a03d),
	ATSTRINGHASH("am_mp_smpl_interior_ext", 0xe0a7b7ad),
	ATSTRINGHASH("am_mp_smpl_interior_int", 0x05973129),
	ATSTRINGHASH("am_mp_solomon_office", 0x220A2BF3),
	ATSTRINGHASH("am_mp_submarine", 0xE16957F2),
	ATSTRINGHASH("am_mp_vehicle_reward", 0x692F8E30),
	ATSTRINGHASH("am_mp_warehouse", 0x2262ddf0),
	ATSTRINGHASH("am_mp_yacht", 0xc930c939),
	ATSTRINGHASH("am_npc_invites", 0x529d545f),
	ATSTRINGHASH("am_pass_the_parcel", 0x8dd957aa),
	ATSTRINGHASH("am_patrick", 0xb70b6f22),
	ATSTRINGHASH("am_penned_in", 0x9fd87720),
	ATSTRINGHASH("am_pi_menu", 0x06f27211),
	ATSTRINGHASH("am_plane_takedown", 0x990a9196),
	ATSTRINGHASH("am_prostitute", 0x62773681),
	ATSTRINGHASH("am_rollercoaster", 0xa8f75ae1),
	ATSTRINGHASH("am_rontrevor_cut", 0xe895e13e),
	ATSTRINGHASH("am_taxi", 0x455fe4de),
	ATSTRINGHASH("am_treasurehunt", 0xff5699a6),
	ATSTRINGHASH("am_vehicle_spawn", 0xb726fc82),
	ATSTRINGHASH("apartment_minigame_launcher", 0xc4382362),
	ATSTRINGHASH("appbikerbusiness", 0x57fad435),
	ATSTRINGHASH("appbroadcast", 0x1b67ddef),
	ATSTRINGHASH("appbusinesshub", 0xD1BE4A95),
	ATSTRINGHASH("appbunkerbusiness", 0x068ba1e1),
	ATSTRINGHASH("appcontacts", 0xb7387687),
	ATSTRINGHASH("apphackertruck", 0x57A3EC48),
	ATSTRINGHASH("appimportexport", 0x114e0bf4),
	ATSTRINGHASH("appinternet", 0x6a172273),
	ATSTRINGHASH("appjipmp", 0xae47d94e),
	ATSTRINGHASH("appmpbossagency", 0x4834763f),
	ATSTRINGHASH("appmpjoblistnew", 0xcb883653),
	ATSTRINGHASH("appsecuroserv", 0x25b97645),
	ATSTRINGHASH("appSmuggler", 0xbda16e7d),
	ATSTRINGHASH("arena_carmod", 0xdafc0911),
	ATSTRINGHASH("armory_aircraft_carmod", 0x78ddb08f),
	ATSTRINGHASH("base_carmod", 0x00e813dd),
	ATSTRINGHASH("business_battles", 0xA6526FA9),
	ATSTRINGHASH("BUSINESS_BATTLES_DEFEND", 0x6FA239A5),
	ATSTRINGHASH("business_battles_sell", 0xF3F871DA),
	ATSTRINGHASH("business_hub_carmod", 0x6E6F052),
	ATSTRINGHASH("camhedz_arcade", 0xE81212C6),
	ATSTRINGHASH("car_meet_carmod", 0xBB3D7E9D),
	ATSTRINGHASH("carmod_shop", 0x1dc6b680),
	ATSTRINGHASH("casino_interior_seating", 0xC2C12122),
	ATSTRINGHASH("casino_lucky_wheel", 0xBDF0CCFF),
	ATSTRINGHASH("celebrations", 0xf8bef8f8),
	ATSTRINGHASH("clothes_shop_mp", 0xc0f0bbc3),
	ATSTRINGHASH("clothes_shop_sp", 0xf8a3166b),
	ATSTRINGHASH("copscrooks", 0xa43c6afe),
	ATSTRINGHASH("copscrookswrapper", 0xa7d5c6a0),
	ATSTRINGHASH("copscrooks_holdup_instance", 0x70871f89),
	ATSTRINGHASH("copscrooks_job_instance", 0xF28DD9D9),
	ATSTRINGHASH("debug", 0x0addd94c),
	ATSTRINGHASH("degenatron_games", 0x5A550D31),
	ATSTRINGHASH("dont_cross_the_line", 0x85db1b5a),
	ATSTRINGHASH("fixer_hq_carmod", 0x41f74a04),
	ATSTRINGHASH("fm_bj_race_controler", 0xbe3e7d03),
	ATSTRINGHASH("fm_content_acid_lab_sell", 0x92772b1f),
	ATSTRINGHASH("fm_content_acid_lab_setup", 0x61ff0c31),
	ATSTRINGHASH("fm_content_acid_lab_source", 0x701beffb),
	ATSTRINGHASH("fm_content_ammunation", 0x4537E4CA),
	ATSTRINGHASH("fm_content_auto_shop_delivery", 0xfc4970dc),
	ATSTRINGHASH("fm_content_bank_shootout", 0x3f21df40),
	ATSTRINGHASH("fm_content_bar_resupply", 0x2363D09D),
	ATSTRINGHASH("fm_content_business_battles", 0x62EA78D0),
	ATSTRINGHASH("fm_content_cargo", 0x035d0edc),
	ATSTRINGHASH("fm_content_cerberus", 0xe05f49f1),
	ATSTRINGHASH("fm_content_club_management", 0xa14222b3),
	ATSTRINGHASH("fm_content_club_odd_jobs", 0xdf2e4655),
	ATSTRINGHASH("fm_content_club_source", 0x2B88E6A1),
	ATSTRINGHASH("fm_content_clubhouse_contracts", 0x5a98b2be),
	ATSTRINGHASH("fm_content_convoy", 0x4b293c1f),
	ATSTRINGHASH("fm_content_crime_scene", 0x5CA3449B),
	ATSTRINGHASH("fm_content_bike_shop_delivery", 0x3ff69732),
	ATSTRINGHASH("fm_content_dispatch", 0xBC0EB82E),
	ATSTRINGHASH("fm_content_drug_lab_work", 0x0bf710af),
	ATSTRINGHASH("fm_content_drug_vehicle", 0x5B3F117),
	ATSTRINGHASH("fm_content_export_cargo", 0xee7c5974),
	ATSTRINGHASH("fm_content_golden_gun", 0xA7CBD213),
	ATSTRINGHASH("fm_content_gunrunning", 0x1a160b48),
	ATSTRINGHASH("fm_content_hsw_time_trial", 0x9C273413),
	ATSTRINGHASH("fm_content_island_dj", 0xBE590E09),
	ATSTRINGHASH("fm_content_island_heist", 0xF2A65E92),
	ATSTRINGHASH("fm_content_metal_detector", 0xEEDDF72E),
	ATSTRINGHASH("fm_content_movie_props", 0x9BF88E0E),
	ATSTRINGHASH("fm_content_parachuter", 0x746aa9c6),
	ATSTRINGHASH("fm_content_payphone_hit", 0xF7F8D71A),
	ATSTRINGHASH("fm_content_payphone_intro", 0x2F916684),
	ATSTRINGHASH("fm_content_phantom_car", 0xF699F9CD),
	ATSTRINGHASH("fm_content_robbery", 0x298258d1),
	ATSTRINGHASH("fm_content_security_contract", 0x26EA9F66),
	ATSTRINGHASH("fm_content_serve_and_protect", 0xFF78189D),
	ATSTRINGHASH("fm_content_sightseeing", 0x975e5d41),
	ATSTRINGHASH("fm_content_skydive", 0x3d946731),
	ATSTRINGHASH("fm_content_slasher", 0xD45729DB),
	ATSTRINGHASH("fm_content_smuggler_plane", 0xb8140067),
	ATSTRINGHASH("fm_content_smuggler_trail", 0x277b17b6),
	ATSTRINGHASH("fm_content_source_research", 0xe3e84b48),
	ATSTRINGHASH("fm_content_stash_house", 0x29513dec),
	ATSTRINGHASH("fm_content_taxi_driver", 0xb7782ad4),
	ATSTRINGHASH("fm_content_tuner_robbery", 0xB5278C95),
	ATSTRINGHASH("fm_content_vehicle_list", 0x9E4931D3),
	ATSTRINGHASH("fm_content_vip_contract_1", 0x75C7E0D3),
	ATSTRINGHASH("fm_content_xmas_mugger", 0x6cfc0084),
	ATSTRINGHASH("fm_deathmatch_controler", 0xe62128cb),
	ATSTRINGHASH("fm_deathmatch_creator", 0xa57e7489),
	ATSTRINGHASH("fm_hideout_controler", 0x055ede8c),
	ATSTRINGHASH("fm_hold_up_tut", 0x75804856),
	ATSTRINGHASH("fm_horde_controler", 0x514a6d17),
	ATSTRINGHASH("fm_impromptu_dm_controler", 0xad453307),
	ATSTRINGHASH("fm_intro", 0xefe7c380),
	ATSTRINGHASH("fm_maintain_transition_players", 0xf11bb0f7),
	ATSTRINGHASH("fm_mission_controller", 0x6c2ce225),
	ATSTRINGHASH("fm_mission_controller_2020", 0xBF461AD0),
	ATSTRINGHASH("fm_race_controler", 0xeb004e0e),
	ATSTRINGHASH("fm_race_creator", 0x4410302a),
	ATSTRINGHASH("fm_survival_controller", 0x1D477306),
	ATSTRINGHASH("fm_survival_creator", 0xBD2AAC0F),
	ATSTRINGHASH("fmmc_launcher", 0xa3fb8a5e),
	ATSTRINGHASH("fmmc_playlist_controller", 0x56e10d97),
	ATSTRINGHASH("freemode", 0xc875557d),
	ATSTRINGHASH("freemodearcade", 0xff3cad99),
	ATSTRINGHASH("freemode_initArcade", 0xA2B04ABA),
	ATSTRINGHASH("freemodeLite", 0x1CA0CF36),
	ATSTRINGHASH("gb_airfreight", 0x59e32892),
	ATSTRINGHASH("gb_amphibious_assault", 0x31dc55bb),
	ATSTRINGHASH("gb_assault", 0xc3961a05),
	ATSTRINGHASH("GB_BANK_JOB", 0x929CEFED),
	ATSTRINGHASH("gb_bellybeast", 0xd4d41110),
	ATSTRINGHASH("gb_biker_bad_deal", 0x2334f600),
	ATSTRINGHASH("gb_biker_burn_assets", 0xd9ee6178),
	ATSTRINGHASH("gb_biker_contraband_defend", 0x56a1412c),
	ATSTRINGHASH("gb_biker_contraband_sell", 0xdffc4863),
	ATSTRINGHASH("gb_biker_contract_killing", 0x1c8463b3),
	ATSTRINGHASH("gb_biker_criminal_mischief", 0x3e927a10),
	ATSTRINGHASH("gb_biker_destroy_vans", 0x6a5fc4cc),
	ATSTRINGHASH("gb_biker_driveby_assassin", 0x2898b9e6),
	ATSTRINGHASH("gb_biker_free_prisoner", 0x966afe8f),
	ATSTRINGHASH("gb_biker_joust", 0xa9165795),
	ATSTRINGHASH("gb_biker_last_respects", 0x63389db5),
	ATSTRINGHASH("gb_biker_race_p2p", 0xd4caa546),
	ATSTRINGHASH("gb_biker_rescue_contact", 0x8403149b),
	ATSTRINGHASH("gb_biker_rippin_it_up", 0x64ad4306),
	ATSTRINGHASH("gb_biker_safecracker", 0xfc5f9481),
	ATSTRINGHASH("gb_biker_search_and_destroy", 0x6d5e1016),
	ATSTRINGHASH("gb_biker_shuttle", 0xd41e6264),
	ATSTRINGHASH("gb_biker_stand_your_ground", 0x9e4e7734),
	ATSTRINGHASH("gb_biker_steal_bikes", 0xdd56dc62),
	ATSTRINGHASH("gb_biker_unload_weapons", 0xb31a0796),
	ATSTRINGHASH("gb_biker_wheelie_rider", 0xb3e837ff),
	ATSTRINGHASH("gb_carjacking", 0xa29e8148),
	ATSTRINGHASH("gb_cashing_out", 0xaa79b0eb),
	ATSTRINGHASH("gb_casino", 0x13645DC3),
    ATSTRINGHASH("gb_casino_heist", 0x8ef7e740),
	ATSTRINGHASH("gb_casino_heist_planning", 0x8F28DC74),
	ATSTRINGHASH("gb_collect_money", 0x206c6c4b),
	ATSTRINGHASH("gb_contraband_buy", 0xeb04de05),
	ATSTRINGHASH("gb_contraband_defend", 0x2055a1a4),
	ATSTRINGHASH("gb_contraband_sell", 0x7b3e31d2),
	ATSTRINGHASH("gb_data_hack", 0x45154B11),
	ATSTRINGHASH("gb_deathmatch", 0x9f42b34b),
	ATSTRINGHASH("gb_delivery", 0x6e9955cb),
	ATSTRINGHASH("GB_EYE_IN_THE_SKY", 0x3DA8C1A),
	ATSTRINGHASH("gb_finderskeepers", 0xa6320c4d),
	ATSTRINGHASH("gb_fivestar", 0x8dc43a99),
	ATSTRINGHASH("gb_fortified", 0x51925081),
	ATSTRINGHASH("gb_fragile_goods", 0x2382af03),
	ATSTRINGHASH("gb_fully_loaded", 0xda508d57),
	ATSTRINGHASH("gb_gangops", 0xfb30320a),
	ATSTRINGHASH("gb_gang_ops_planning", 0x6d20caab),
	ATSTRINGHASH("gb_gunrunning", 0x8cd0bff0),
	ATSTRINGHASH("gb_gunrunning_defend", 0x14b421aa),
	ATSTRINGHASH("gb_gunrunning_delivery", 0x29a28ed8),
	ATSTRINGHASH("gb_headhunter", 0x71c13b55),
	ATSTRINGHASH("gb_hunt_the_boss", 0x52d21ba4),
	ATSTRINGHASH("gb_ie_delivery_cutscene", 0x9114379b),
	ATSTRINGHASH("gb_illicit_goods_resupply", 0x9d9d13ec),
	ATSTRINGHASH("GB_INFILTRATION", 0x610BE6A0),
	ATSTRINGHASH("GB_JEWEL_STORE_GRAB", 0xB0BCBA60),
	ATSTRINGHASH("gb_ploughed", 0x14611265),
	ATSTRINGHASH("gb_point_to_point", 0x45bd5447),
	ATSTRINGHASH("gb_ramped_up", 0x506cc782),
	ATSTRINGHASH("gb_rob_shop", 0xb4901ab2),
	ATSTRINGHASH("gb_salvage", 0x4eff43a1),
	ATSTRINGHASH("GB_SECURITY_VAN", 0xE8DA8C5F),
	ATSTRINGHASH("gb_sightseer", 0x724eba87),
	ATSTRINGHASH("gb_smuggler", 0xabd3de17),
	ATSTRINGHASH("gb_stockpiling", 0xf60a885b),
	ATSTRINGHASH("GB_TARGET_PURSUIT", 0x5D94ED7B),
	ATSTRINGHASH("gb_terminate", 0xf2095bfd),
	ATSTRINGHASH("gb_transporter", 0x0b87dc63),
	ATSTRINGHASH("gb_vehicle_export", 0xd9751e61),
	ATSTRINGHASH("gb_velocity", 0x5f6c2123),
	ATSTRINGHASH("gb_yacht_rob", 0x7fa4662e),
    ATSTRINGHASH("ggsm_arcade", 0x2aedcdfb),
	ATSTRINGHASH("grid_arcade_cabinet", 0xD5597100),
	ATSTRINGHASH("golf", 0x299717f2),
	ATSTRINGHASH("golf_mp", 0xcf654b49),
    ATSTRINGHASH("gunclub_shop", 0x1beb0bf6),
	ATSTRINGHASH("gunslinger_arcade", 0xDB23F3DC),
    ATSTRINGHASH("hacker_truck_carmod", 0xCE27944E),
	ATSTRINGHASH("hairdo_shop_mp", 0xb57685c9),
	ATSTRINGHASH("hairdo_shop_sp", 0xb7895f57),
	ATSTRINGHASH("hangar_carmod", 0x34141a78),
	ATSTRINGHASH("heist_island_planning", 0x55EC87F4), 
	ATSTRINGHASH("ingamehud", 0xc45650f0),
	ATSTRINGHASH("laptop_trigger", 0x6834eeb9),
	ATSTRINGHASH("lobby", 0x5A8CB6B1),
	ATSTRINGHASH("lobby_preview", 0x5EC02F5F),
	ATSTRINGHASH("main", 0x27eb33d7),
	ATSTRINGHASH("main_persistent", 0x5700179c),
	ATSTRINGHASH("maintransition", 0x5ff2841f),
	ATSTRINGHASH("mg_race_to_point", 0x804da739),
	ATSTRINGHASH("mp_menuped", 0x99ef65a4),
	ATSTRINGHASH("mptestbed", 0x62c09cc6),
	ATSTRINGHASH("net_shop_gameTransactions", 0x1DA70FC7),
	ATSTRINGHASH("net_test_drive", 0x4eb46ff0),
	ATSTRINGHASH("nightclubpeds", 0x9a821400),
	ATSTRINGHASH("ob_bong", 0x57439d64),
	ATSTRINGHASH("ob_cashregister", 0x0e1819bf),
	ATSTRINGHASH("ob_drinking_shots", 0x000269eb),
	ATSTRINGHASH("ob_franklin_beer", 0xb1b4d0ed),
	ATSTRINGHASH("ob_franklin_wine", 0x00eb926e),
	ATSTRINGHASH("ob_jukebox", 0xD044BED0),
	ATSTRINGHASH("ob_mp_shower_med", 0x96c72262),
	ATSTRINGHASH("ob_mp_stripper", 0x5a6f3a5f),
	ATSTRINGHASH("ob_telescope", 0x5202cf8c),
	ATSTRINGHASH("ob_vend1", 0xa39c1f72),
	ATSTRINGHASH("ob_vend2", 0x11c77bc7),
	ATSTRINGHASH("pausemenu_multiplayer", 0x52f86d6a),
	ATSTRINGHASH("pb_prostitute", 0x6a96798a),
	ATSTRINGHASH("personal_carmod_shop", 0xdef103d2),
	ATSTRINGHASH("pilot_school", 0xb7f44cef),
	ATSTRINGHASH("pilot_school_mp", 0xc5a00edd),
	ATSTRINGHASH("puzzle", 0x881750F6),
	ATSTRINGHASH("range_modern_mp", 0x1f538c61),
	ATSTRINGHASH("road_arcade", 0x7ae86e42),	 
	ATSTRINGHASH("script_metrics", 0x9cf45f72),
	ATSTRINGHASH("scroll_arcade_cabinet", 0x738209C),
	ATSTRINGHASH("sctv", 0xf0f8c24d),
	ATSTRINGHASH("shop_controller", 0x39da738b),
	ATSTRINGHASH("spawn_activities", 0xB4C17BD2),
	ATSTRINGHASH("stripclub", 0x095b7c6a),
	ATSTRINGHASH("stripclub_drinking", 0xede89eb0),
	ATSTRINGHASH("stripclub_mp", 0x97cd8e78),
	ATSTRINGHASH("tattoo_shop", 0xc0d26565),
	ATSTRINGHASH("tennis", 0x3bf97fcd),
	ATSTRINGHASH("tennis_network_mp", 0xe3940b1c),
	ATSTRINGHASH("Three_Card_Poker", 0xc8608f32),
	ATSTRINGHASH("tuner_planning", 0x1be25eb4),
	ATSTRINGHASH("tuner_property_carmod", 0x70b10f84),
	ATSTRINGHASH("tuner_sandbox_activity", 0x8758ef91),
	ATSTRINGHASH("xml_menus", 0x164e8dee),
	ATSTRINGHASH("valentineRpReward2", 0xd1f9d9c5),
	ATSTRINGHASH("wizard_arcade", 0x25C5FCF3),
	ATSTRINGHASH("ZOMBIE_SURVIVAL", 0x5E84FEF8)	
};
unsigned CScriptedGameEvent::NUM_VALID_CALLING_SCRIPTS = (sizeof(CScriptedGameEvent::ms_validCallingScriptsHashes) / sizeof(u32));

CScriptedGameEvent::CScriptedGameEvent()
: netGameEvent( SCRIPTED_GAME_EVENT, true )
,m_PlayerFlags(0)
,m_SizeOfData(0)
{
	sysMemSet(m_Data, 0, SIZEOF_DATA);

	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

CScriptedGameEvent::CScriptedGameEvent(CGameScriptId& scriptId, void* data, const unsigned sizeOfData, const u32 playerFlags)
: netGameEvent( SCRIPTED_GAME_EVENT, true )
, m_scriptId(scriptId)
, m_PlayerFlags(playerFlags)
, m_SizeOfData(sizeOfData)
{
	sysMemSet(m_Data, 0, SIZEOF_DATA);

	if (AssertVerify(0 < m_SizeOfData) && AssertVerify(m_SizeOfData <= SIZEOF_DATA))
	{
		sysMemCpy(m_Data, data, sizeOfData);
	}
	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

template <class Serialiser> void CScriptedGameEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_SizeOfData, 8*sizeof(m_SizeOfData), "Scripted Data");
	if (AssertVerify(0 < m_SizeOfData) && AssertVerify(m_SizeOfData <= SIZEOF_DATA))
	{
		SERIALISE_DATABLOCK(serialiser, m_Data, GetSizeInBits());
	}
}

void
CScriptedGameEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CScriptedGameEvent networkEvent;
	netInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void
CScriptedGameEvent::Trigger(CGameScriptId& scriptId, void* data, const unsigned sizeOfData, const u32 playerFlags)
{
	u32 scriptHash = 0;

	for (scriptHash=0; scriptHash<NUM_VALID_CALLING_SCRIPTS; scriptHash++)
	{
		if (ms_validCallingScriptsHashes[scriptHash] == scriptId.GetScriptNameHash())
			break;
	}

	bool bAllowEvent = scriptHash < NUM_VALID_CALLING_SCRIPTS;

#if !__FINAL
	if(PARAM_dontCrashOnScriptValidation.Get())
	{
		bAllowEvent = true;
	}
	else
	{
		gnetFatalAssertf(bAllowEvent, "Scripted game event being called from invalid script %s", scriptId.GetScriptName());
	}
#endif // !__FINAL

	if (bAllowEvent)
	{
		netInterface::GetEventManager().CheckForSpaceInPool();
		CScriptedGameEvent* pEvent = rage_new CScriptedGameEvent(scriptId, data, sizeOfData, playerFlags);
		netInterface::GetEventManager().PrepareEvent(pEvent);

		u32 numTimesTriggered = AddHistoryItem(scriptId, data, sizeOfData);

		bool sendTelemetry = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SCRIPT_EVENT_SPAM_ENABLED", 0xc14d227b), true);
		if (sendTelemetry)
		{
			int telemetryThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SCRIPT_EVENT_SPAM_THRESHOLD", 0x112d95a6), TELEMETRY_THRESHOLD);	

			if (numTimesTriggered > telemetryThreshold)
			{
				scrValue* dataNew = (scrValue*)data;
				const int eventType = dataNew[0].Int;
				SendScriptSpamTelemetry(eventType);
			}
		}
	}
}

bool
CScriptedGameEvent::IsInScope(const netPlayer& player) const
{
	if (!AssertVerify(0 < m_SizeOfData) || !AssertVerify(m_SizeOfData <= SIZEOF_DATA))
	{
		gnetError("Event SCRIPTED_GAME_EVENT not sent.");
		return false;
	}

	gnetAssertf(player.GetPhysicalPlayerIndex() <= 31, "bit must be between 0 to 31");

	return (!player.IsLocal() && SendToPlayer(player.GetPhysicalPlayerIndex()));
}

void
CScriptedGameEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent< CSyncDataWriter >(messageBuffer);
}

void CScriptedGameEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent< CSyncDataReader >(messageBuffer);
}

bool CScriptedGameEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	if (AssertVerify(0 < m_SizeOfData) && AssertVerify(m_SizeOfData <= SIZEOF_DATA))
	{
		// ignore the event if the there was no space for the script event on the queue
		CEventNetworkScriptEvent scriptEvent((void*)m_Data, m_SizeOfData, &fromPlayer);
		return GetEventScriptNetworkGroup()->AddRemote(scriptEvent) != NULL;
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void
CScriptedGameEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Script", "%s", m_scriptId.GetLogName());
	log.WriteDataValue("SizeOfData", " %u", m_SizeOfData);
}

void
CScriptedGameEvent::WriteDecisionToLogFile() const
{
}

void
CScriptedGameEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("SizeOfData", " %u", m_SizeOfData);
}

#endif // ENABLE_NETWORK_LOGGING

bool CScriptedGameEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == SCRIPTED_GAME_EVENT)
	{
		const CScriptedGameEvent* pScriptEvent = SafeCast(const CScriptedGameEvent, pEvent);

		if (pScriptEvent)
		{
			if (m_SizeOfData == pScriptEvent->m_SizeOfData &&
				memcmp(m_Data, pScriptEvent->m_Data, m_SizeOfData) == 0)
			{
				bool bReusing = false;

				for (PhysicalPlayerIndex i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
				{
					if (((pScriptEvent->m_PlayerFlags & (1<<i)) != 0) && ((m_PlayerFlags & (1<<i)) == 0))
					{
						m_PlayerFlags  |= (1<<i);

						// we need to set the player ID on the event ID to handle the case where the event has already been sent
						GetEventId().SetPlayerInScope(i);

#if ENABLE_NETWORK_LOGGING
						if (!bReusing)
						{
							LOGGING_ONLY(NetworkLogUtils::WriteLogEvent(netInterface::GetEventManager().GetLog(), "REUSING_EVENT", "%s_%d", GetEventName(), GetEventId().GetID()));
						}
						const netPlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(i);
						if (pPlayer)
						{
							LOGGING_ONLY(netInterface::GetEventManager().GetLog().WriteDataValue("Adding player", "%s", pPlayer->GetLogName()));
						}
						else
						{
							LOGGING_ONLY(netInterface::GetEventManager().GetLog().WriteDataValue("Adding player", "%d", i));
						}
#endif // ENABLE_NETWORK_LOGGING

						bReusing = true;
					}
				}

				return true;
			}
		}
	}

	return false;
}

void CScriptedGameEvent::SendScriptSpamTelemetry(const int eventType)
{
	gnetAssertf(0, "Script Event Spam detected, this should not happen in DEV - eventType = %d", eventType);
	static const int MAX_METRIC_FREQUENCY = 3 * 60; // 3 minutes
	static u64 s_lastMetricSentTimestamp = 0;
	static int s_ignoredEventCount = 0;

	u64 ts = rlGetPosixTime();
	if(ts - s_lastMetricSentTimestamp > MAX_METRIC_FREQUENCY)
	{
		MetricScriptEventSpam m(eventType, s_ignoredEventCount);
		APPEND_METRIC(m);
		s_lastMetricSentTimestamp = ts;
		s_ignoredEventCount = 0;
	}
	else
	{
		s_ignoredEventCount++;
	}
}

u32 CScriptedGameEvent::AddHistoryItem(CGameScriptId& scriptId, void* data, const unsigned sizeOfData)
{
	u32 numTimesTriggered = 1;

	sEventHistoryEntry newItem = sEventHistoryEntry(scriptId.GetScriptIdHash().GetHash(), CalculateDataHash(reinterpret_cast<u8*>(data), sizeOfData), sizeOfData);

	sEventHistoryEntry* pDuplicateItem = nullptr;

	for (u32 i=0; i<ms_numEventsInHistory; i++)
	{
		if (ms_eventHistory[i] == newItem)
		{
			pDuplicateItem = &ms_eventHistory[i];
			break;
		}
	}

	if (!pDuplicateItem)
	{
		if (ms_numEventsInHistory == HISTORY_QUEUE_SIZE)
		{
			ms_numEventsInHistory--;
		}

		ms_eventHistory[ms_numEventsInHistory++] = newItem;
	}
	else
	{
		numTimesTriggered = ++pDuplicateItem->m_numTimesTriggered;

		// sort history items based on the number of events triggered for that entry. This means we will dump the least frequent entries.
		qsort(&ms_eventHistory[0], ms_numEventsInHistory, sizeof(sEventHistoryEntry), &SortEventHistory);
	}

	return numTimesTriggered;
}

int CScriptedGameEvent::SortEventHistory(const void *paramA, const void *paramB)
{
	const sEventHistoryEntry* pEntry1 = reinterpret_cast<const sEventHistoryEntry*>(paramA);
	const sEventHistoryEntry* pEntry2 = reinterpret_cast<const sEventHistoryEntry*>(paramB);

	if (pEntry1->m_numTimesTriggered < pEntry2->m_numTimesTriggered)
	{
		return 1;
	}
	else if (pEntry1->m_numTimesTriggered > pEntry2->m_numTimesTriggered)
	{
		return -1;
	}

	return 0;
}

u32 CScriptedGameEvent::CalculateDataHash(u8* data, unsigned sizeOfData)
{
	// Computes a hash of the given data buffer.
	// Hash function suggested by http://www.cs.yorku.ca/~oz/hash.html
	u32 hash = 5381;

	while (sizeOfData > 0)
	{
		sizeOfData--;
		hash = ((hash << 5) + hash) ^ data[sizeOfData];
	}

	return hash;
}

// ===========================================================================================================
// REMOTE SCRIPT INFO EVENT
// ===========================================================================================================

CRemoteScriptInfoEvent::CRemoteScriptInfoEvent() :
netGameEvent( REMOTE_SCRIPT_INFO_EVENT, false, false ),
m_players(0),
m_ignored(false)
{
}

CRemoteScriptInfoEvent::CRemoteScriptInfoEvent(const CGameScriptHandlerMgr::CRemoteScriptInfo& scriptInfo, PlayerFlags players) :
netGameEvent( REMOTE_SCRIPT_INFO_EVENT, false, false ),
m_scriptInfo(scriptInfo),
m_players(players),
m_ignored(false)
{

}

void CRemoteScriptInfoEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRemoteScriptInfoEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

bool CRemoteScriptInfoEvent::Trigger(const CGameScriptHandlerMgr::CRemoteScriptInfo& scriptInfo, PlayerFlags players)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

	// check to see if there is already a remote script info event being sent to another player, in this case we can reuse this event
	atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

	CRemoteScriptInfoEvent *existingEvent = NULL;

	while (node)
	{
		netGameEvent *networkEvent = node->Data;

		if(networkEvent && networkEvent->GetEventType() == REMOTE_SCRIPT_INFO_EVENT && !networkEvent->IsFlaggedForRemoval())
		{
			CRemoteScriptInfoEvent* pRemoteScriptInfoEvent = static_cast<CRemoteScriptInfoEvent *>(networkEvent);

			if (pRemoteScriptInfoEvent->m_scriptInfo.GetScriptId() == scriptInfo.GetScriptId())
			{
				if (pRemoteScriptInfoEvent->m_scriptInfo == scriptInfo)
				{
					existingEvent = pRemoteScriptInfoEvent;
				}
				else
				{
					// make sure the player still to receive the last event, get this new one, with the updated script info
					players |= pRemoteScriptInfoEvent->GetEventId().GetUnackedPlayers();

					// destroy any previous events for this script
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "DESTROYING_EVENT", "%s_%d\r\n", NetworkInterface::GetEventManager().GetEventNameFromType(pRemoteScriptInfoEvent->GetEventType()), pRemoteScriptInfoEvent->GetEventId().GetID());
					pRemoteScriptInfoEvent->GetEventId().ClearPlayersInScope();
				}
			}
		}

		node = node->GetNext();
	}

	if(existingEvent)
	{
		existingEvent->AddTargetPlayers(players);
	}
	else
	{
		if (!NetworkInterface::GetEventManager().CheckForSpaceInPool())
		{
			return false;
		}

		CRemoteScriptInfoEvent *pEvent = rage_new CRemoteScriptInfoEvent(scriptInfo, players);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}

	return true;
}

bool CRemoteScriptInfoEvent::IsInScope( const netPlayer& player ) const
{
    // never send remote script info events to local players
    if(player.IsLocal())
        return false;

    return (m_players & (1<<player.GetPhysicalPlayerIndex())) != 0;
}

template <class Serialiser> void CRemoteScriptInfoEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    m_scriptInfo.Serialise(serialiser);
}

void CRemoteScriptInfoEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoteScriptInfoEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRemoteScriptInfoEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	// ignore this event if we are not in a match yet (we may not know the physical indices of all the other players in the game at this point)
	if (NetworkInterface::IsGameInProgress())
	{
		scriptHandler* pHandler = scriptInterface::GetScriptManager().GetScriptHandler(m_scriptInfo.GetScriptId());

		// ignore the event if we are hosting the script
		if (pHandler && pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->IsHostLocal())
		{
			m_ignored = true;
		}
		else
		{
			CTheScripts::GetScriptHandlerMgr().AddRemoteScriptInfo(m_scriptInfo, &fromPlayer);
		}

		return true;
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CRemoteScriptInfoEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("Script", "%s", m_scriptInfo.GetScriptId().GetLogName());
	log.WriteDataValue("Participants", "%d", m_scriptInfo.GetActiveParticipants());
    log.WriteDataValue("Host Token", "%d", m_scriptInfo.GetHostToken());
	log.WriteDataValue("Players to send to", "%u", m_players);

	if (m_ignored)
	{
		log.WriteDataValue("Ignored", "We are hosting");
	}
}

#endif //ENABLE_NETWORK_LOGGING

void CRemoteScriptInfoEvent::AddTargetPlayers( PlayerFlags players )
{
	if (m_players != players)
	{
		for (PhysicalPlayerIndex i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
		{
			if (((players & (1<<i)) != 0) && ((m_players & (1<<i)) == 0))
			{
				m_players |= (1<<i);

				// we need to set the player ID on the event ID to handle the case where the event has already been sent
				GetEventId().SetPlayerInScope(i);
			}
		}
	}
}

// ===========================================================================================================
// REMOTE SCRIPT LEAVE EVENT
// ===========================================================================================================

CRemoteScriptLeaveEvent::CRemoteScriptLeaveEvent() :
netGameEvent( REMOTE_SCRIPT_LEAVE_EVENT, false, false ),
m_players(0)
BANK_ONLY(,m_bSuccess(false))
DEV_ONLY(,m_aliveTime(0))
{
}

CRemoteScriptLeaveEvent::CRemoteScriptLeaveEvent(const CGameScriptId& scrId, PlayerFlags players) :
netGameEvent( REMOTE_SCRIPT_LEAVE_EVENT, false, false ),
m_scriptId(scrId),
m_players(players)
BANK_ONLY(,m_bSuccess(false))
{
#if __DEV
    m_aliveTime = fwTimer::GetSystemTimeInMilliseconds();
#endif // __DEV
}

void CRemoteScriptLeaveEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CRemoteScriptLeaveEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemoteScriptLeaveEvent::Trigger(const CGameScriptId& scrId, PlayerFlags players)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    NetworkInterface::GetEventManager().CheckForSpaceInPool();
    CRemoteScriptLeaveEvent *pEvent = rage_new CRemoteScriptLeaveEvent(scrId, players);
    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRemoteScriptLeaveEvent::IsInScope( const netPlayer& player ) const
{
#if __DEV
    gnetAssertf(m_aliveTime - fwTimer::GetSystemTimeInMilliseconds() < 60000, "Script leave event for script %s is not being processed", m_scriptId.GetLogName());
#endif
	if (!player.IsBot() && !player.IsLocal())
	{
		return (m_players & (1<<player.GetPhysicalPlayerIndex())) != 0;
	}

	return false;
}

template <class Serialiser> void CRemoteScriptLeaveEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    m_scriptId.Serialise(serialiser);
}

void CRemoteScriptLeaveEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoteScriptLeaveEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRemoteScriptLeaveEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	// ignore this event if we are not in a match yet (we may not know the physical indicies of all the other players in the game at this point)
	if (NetworkInterface::IsGameInProgress())
	{
		CTheScripts::GetScriptHandlerMgr().RemovePlayerFromRemoteScript(m_scriptId, fromPlayer, false);
	    return true;
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CRemoteScriptLeaveEvent::WriteEventToLogFile(bool LOGGING_ONLY(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("Script", "%s", m_scriptId.GetLogName());

    if (!wasSent)
    {
        log.WriteDataValue("Success", "%s", m_bSuccess ? "true" : "false");
    }
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// MARK AS NO LONGER NEEDED EVENT
// ===========================================================================================================

CMarkAsNoLongerNeededEvent::CMarkAsNoLongerNeededEvent() :
netGameEvent( MARK_AS_NO_LONGER_NEEDED_EVENT, false, false ),
m_numScriptObjects(0),
m_objectsForDeletion(false)
{
	for(u32 index = 0; index < MAX_OBJECTS_PER_EVENT; index++)
	{
		m_scriptObjects[index].objectId = INVALID_SCRIPT_OBJECT_ID;
		m_scriptObjects[index].toBeDeleted = false;
	}
}

bool CMarkAsNoLongerNeededEvent::AddObjectID(const ObjectId &objectID, bool toBeDeleted)
{
	bool objectIDAdded = false;

    // first ensure we don't already have this object in the data for this event
    for(u32 index = 0; index < m_numScriptObjects; index++)
    {
        if(m_scriptObjects[index].objectId == objectID && m_scriptObjects[index].toBeDeleted == toBeDeleted)
        {
            objectIDAdded = true;
        }
    }

    if(!objectIDAdded)
    {
	    if(m_numScriptObjects < MAX_OBJECTS_PER_EVENT)
	    {
		    m_scriptObjects[m_numScriptObjects].objectId = objectID;
		    m_scriptObjects[m_numScriptObjects].toBeDeleted = toBeDeleted;
		    m_numScriptObjects++;
		    objectIDAdded = true;

		    if (toBeDeleted)
		    {
			    m_objectsForDeletion = true;
		    }
	    }
    }

	return objectIDAdded;
}

void CMarkAsNoLongerNeededEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CMarkAsNoLongerNeededEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CMarkAsNoLongerNeededEvent::Trigger(CNetObjGame& object, bool toBeDeleted)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	gnetAssert(object.GetScriptObjInfo());

	bool addedObjectID = false;

	if (AssertVerify(object.IsScriptObject()))
	{
		object.SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);
	}

	if (toBeDeleted && object.GetEntity())
	{
		SafeCast(CNetObjEntity, &object)->FlagForDeletion();
	}

	// search for any existing mark as no longer needed events on the queue and try
	// to append to the end of it
	atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

	while (node)
	{
		netGameEvent *networkEvent = node->Data;

		if(networkEvent && !networkEvent->HasBeenSent() && networkEvent->GetEventType() == MARK_AS_NO_LONGER_NEEDED_EVENT && !networkEvent->IsFlaggedForRemoval())
		{
			CMarkAsNoLongerNeededEvent *markAsNoLongerNeededEvent = static_cast<CMarkAsNoLongerNeededEvent *>(networkEvent);

			if(markAsNoLongerNeededEvent->AddObjectID(object.GetObjectID(), toBeDeleted))
			{
				addedObjectID = true;
			}
		}

		node = node->GetNext();
	}

	if(!addedObjectID)
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CMarkAsNoLongerNeededEvent *pEvent = rage_new CMarkAsNoLongerNeededEvent();
		addedObjectID = pEvent->AddObjectID(object.GetObjectID(), toBeDeleted);
		gnetAssert(addedObjectID);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CMarkAsNoLongerNeededEvent::IsInScope( const netPlayer& player ) const
{
	return (!player.IsLocal());
}

template <class Serialiser> void CMarkAsNoLongerNeededEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_numScriptObjects, SIZEOF_NUM_OBJECTS, "Num Objects");

	gnetAssert(m_numScriptObjects <= MAX_OBJECTS_PER_EVENT);
	m_numScriptObjects = MIN(m_numScriptObjects, MAX_OBJECTS_PER_EVENT);

	SERIALISE_BOOL(serialiser, m_objectsForDeletion);

	for(u32 index = 0; index < m_numScriptObjects; index++)
	{
		SERIALISE_OBJECTID(serialiser, m_scriptObjects[index].objectId, "Object ID");

		if (m_objectsForDeletion)
		{
			SERIALISE_BOOL(serialiser, m_scriptObjects[index].toBeDeleted, "To Be Deleted");
		}
		else
		{
			m_scriptObjects[index].toBeDeleted = false;
		}
	}
}

void CMarkAsNoLongerNeededEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	gnetAssert(m_numScriptObjects <= MAX_OBJECTS_PER_EVENT);
	m_numScriptObjects = MIN(m_numScriptObjects, MAX_OBJECTS_PER_EVENT);

	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CMarkAsNoLongerNeededEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CMarkAsNoLongerNeededEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	for(u32 index = 0; index < m_numScriptObjects; index++)
	{
		netObject *networkObject = netInterface::GetNetworkObject(m_scriptObjects[index].objectId);

		if(networkObject)
		{
			CNetObjGame *netObjGame = SafeCast(CNetObjGame, networkObject);

			if (netObjGame->GetEntity())
			{
				if (netObjGame->IsScriptObject())
				{
					// unregister with the script immediately, and clean up later
					scriptHandler* pScriptHandler = netObjGame->GetScriptHandlerObject() ? netObjGame->GetScriptHandlerObject()->GetScriptHandler() : NULL;

					if (pScriptHandler)
					{
						CScriptEntityExtension* pExtension = netObjGame->GetEntity()->GetExtension<CScriptEntityExtension>();

						if (pExtension)
							pExtension->SetNoLongerNeeded(true);

						pScriptHandler->UnregisterScriptObject(*netObjGame->GetScriptHandlerObject());
					}

					netObjGame->SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);
				}

				if (m_scriptObjects[index].toBeDeleted)
				{
					gnetAssert(m_objectsForDeletion);
					SafeCast(CNetObjEntity, netObjGame)->FlagForDeletion();
				}
			}
		}
	}

	return true;
}

bool CMarkAsNoLongerNeededEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == MARK_AS_NO_LONGER_NEEDED_EVENT)
    {
        const CMarkAsNoLongerNeededEvent *markAsNoLongerNeededEvent = SafeCast(const CMarkAsNoLongerNeededEvent, pEvent);

        if(markAsNoLongerNeededEvent)
        {
            for(u32 index = 0; index < m_numScriptObjects; index++)
            {
                gnetAssertf(markAsNoLongerNeededEvent->m_numScriptObjects == 1, "New mark as no longer needed event should always have exactly one object in it!");

                if (m_scriptObjects[index].objectId == markAsNoLongerNeededEvent->m_scriptObjects[0].objectId)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

#if ENABLE_NETWORK_LOGGING

void CMarkAsNoLongerNeededEvent::WriteEventToLogFile(bool UNUSED_PARAM(bSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	for(u32 index = 0; index < m_numScriptObjects; index++)
	{
		netObject *networkObject = netInterface::GetNetworkObject(m_scriptObjects[index].objectId);

		if (networkObject)
		{
			log.WriteDataValue("Object", "%s", networkObject->GetLogName());
		}
		else
		{
			log.WriteDataValue("Object id", "%d", m_scriptObjects[index].objectId);
		}

		if (m_objectsForDeletion)
		{
			log.WriteDataValue("To Be Deleted", "%s", m_scriptObjects[index].toBeDeleted ? "true" : "false");
		}
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// CONVERT TO SCRIPT ENTITY EVENT
// ===========================================================================================================

CConvertToScriptEntityEvent::CConvertToScriptEntityEvent() :
netGameEvent( CONVERT_TO_SCRIPT_ENTITY_EVENT, false, false ),
m_entityId(NETWORK_INVALID_OBJECT_ID)
{
}

CConvertToScriptEntityEvent::CConvertToScriptEntityEvent(const CNetObjPhysical& object, const CGameScriptObjInfo& scriptInfo) :
netGameEvent( CONVERT_TO_SCRIPT_ENTITY_EVENT, false, false ),
m_entityId(object.GetObjectID()),
m_scriptObjInfo(scriptInfo)
{
}

CConvertToScriptEntityEvent::~CConvertToScriptEntityEvent() 
{
	ConvertToScriptEntity();
}

void CConvertToScriptEntityEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CConvertToScriptEntityEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CConvertToScriptEntityEvent::Trigger(const CNetObjPhysical& object)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	gnetAssert(!object.IsScriptObject());
	
	if (AssertVerify(object.GetPendingScriptObjInfo()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CConvertToScriptEntityEvent *pEvent = rage_new CConvertToScriptEntityEvent(object, *object.GetPendingScriptObjInfo());
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CConvertToScriptEntityEvent::IsInScope( const netPlayer& player ) const
{
	if (!player.IsMyPlayer())
	{
		netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId);

		if (networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsPhysical())
		{
			const CGameScriptObjInfo* pInfo = static_cast<CNetObjPhysical*>(networkObject)->GetPendingScriptObjInfo();

			if (pInfo)
			{
				scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(pInfo->GetScriptId());

				if (pHandler &&	pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
				{
					if (networkObject->IsClone())
					{
						return (&player == networkObject->GetPlayerOwner());
					}
					else if (networkObject->GetPendingPlayerOwner())
					{
						return (&player == networkObject->GetPendingPlayerOwner());
					}
				}
			}
		}
	}

	return false;
}

template <class Serialiser> void CConvertToScriptEntityEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_entityId, "Entity ID");
	m_scriptObjInfo.Serialise(serialiser);
}

void CConvertToScriptEntityEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CConvertToScriptEntityEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CConvertToScriptEntityEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	return ConvertToScriptEntity();
}

#if ENABLE_NETWORK_LOGGING

void CConvertToScriptEntityEvent::WriteEventToLogFile(bool UNUSED_PARAM(bSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	CNetObjGame *networkObject = static_cast<CNetObjGame*>(netInterface::GetNetworkObject(m_entityId));

	log.WriteDataValue("Entity", "%s", networkObject ? networkObject->GetLogName() : "??");
	log.WriteDataValue("Script", "%s", m_scriptObjInfo.GetScriptId().GetLogName());
}

#endif //ENABLE_NETWORK_LOGGING

bool CConvertToScriptEntityEvent::operator==( const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == CONVERT_TO_SCRIPT_ENTITY_EVENT)
	{
		const CConvertToScriptEntityEvent* pConvertEvent = SafeCast(const CConvertToScriptEntityEvent, pEvent);

		if (pConvertEvent)
		{
			if (pConvertEvent->m_entityId == m_entityId)
			{
				return true;
			}
		}
	}

	return false;
}

bool CConvertToScriptEntityEvent::ConvertToScriptEntity()
{
	CNetObjGame *networkObject = static_cast<CNetObjGame*>(netInterface::GetNetworkObject(m_entityId));

	if (networkObject && !networkObject->IsClone() && !networkObject->IsPendingOwnerChange())
	{
		gnetAssert(dynamic_cast<CPhysical*>(networkObject->GetEntity()));
		CPhysical* pEntity = static_cast<CPhysical*>(networkObject->GetEntity());

		if (networkObject->IsScriptObject())
		{
#if __ASSERT
			CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			gnetAssertf(pExtension, "CConvertToScriptEntityEvent: %s has no extension", networkObject->GetLogName());
			gnetAssertf(!pExtension || pExtension->GetScriptInfo()->GetScriptId() == m_scriptObjInfo.GetScriptId(), "CConvertToScriptEntityEvent: %s is registered with script %s", networkObject->GetLogName(), pExtension->GetScriptInfo()->GetScriptId().GetLogName());
#endif // __ASSERT
			return true;
		}		
		else
		{
            if(!netVerifyf(pEntity, "CConvertToScriptEntityEvent::Decide - NULL entity"))
            {
                return true;
            }

			if (netVerifyf(!pEntity->GetExtension<CScriptEntityExtension>(), "CConvertToScriptEntityEvent::Decide - %s already has a script extension", networkObject->GetLogName()))
			{
				static_cast<CNetObjPhysical*>(networkObject)->SetPendingScriptObjInfo(m_scriptObjInfo);
				static_cast<CNetObjPhysical*>(networkObject)->ApplyPendingScriptObjInfo();
				return true;
			}
		}
	}

	return false;
}

// ================================================================================================================
// SCRIPT WORLD STATE EVENT
// ================================================================================================================
CScriptWorldStateEvent::CScriptWorldStateEvent() :
netGameEvent( SCRIPT_WORLD_STATE_EVENT, false )
, m_WorldStateData(0)
, m_ChangeState(false)
, m_PlayersInScope(0)
{
}

CScriptWorldStateEvent::CScriptWorldStateEvent(const CNetworkWorldStateData &worldStateData,
                                               bool                          changeState,
                                               const netPlayer              *targetPlayer) :
netGameEvent( SCRIPT_WORLD_STATE_EVENT, false )
, m_WorldStateData(worldStateData.Clone())
, m_ChangeState(changeState)
, m_PlayersInScope(0)
{
    if(targetPlayer == 0)
    {
        m_PlayersInScope = ~0U;
    }
    else
    {
        PhysicalPlayerIndex playerIndex = targetPlayer->GetPhysicalPlayerIndex();

        if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Sending a world state event to a player with an invalid physical player index!"))
        {
            m_PlayersInScope |= (1<<playerIndex);
        }
    }
}

CScriptWorldStateEvent::~CScriptWorldStateEvent()
{
    delete m_WorldStateData;
    m_WorldStateData = 0;
}

void CScriptWorldStateEvent::AddTargetPlayer(const netPlayer &targetPlayer)
{
    PhysicalPlayerIndex playerIndex = targetPlayer.GetPhysicalPlayerIndex();

    if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Sending a world state event to a player with an invalid physical player index!"))
    {
        if((m_PlayersInScope & (1<<playerIndex)) == 0)
        {
            m_PlayersInScope |= (1<<playerIndex);

			// we need to set the player ID on the event ID to handle the case where the event has already been sent
			if(!GetEventId().MustSendToPlayer(playerIndex))
			{
				GetEventId().SetPlayerInScope(playerIndex);
			}	
        }
    }
}

void CScriptWorldStateEvent::EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CScriptWorldStateEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, bitBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CScriptWorldStateEvent::Trigger(const CNetworkWorldStateData &worldStateData,
                                     bool                          changeState,
                                     const netPlayer              *targetPlayer)
{
    CScriptWorldStateEvent *eventToUse = 0;

    // if a target player has been specified check if there is already an event describing
    // this world state being sent to another player, in this case we can reuse this event
    if(targetPlayer)
    {
        atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

        while (node)
        {
            netGameEvent *networkEvent = node->Data;

            if(networkEvent && !networkEvent->HasBeenSent() && networkEvent->GetEventType() == SCRIPT_WORLD_STATE_EVENT && !networkEvent->IsFlaggedForRemoval())
            {
                CScriptWorldStateEvent *existingEvent = static_cast<CScriptWorldStateEvent *>(networkEvent);

                if(existingEvent->m_WorldStateData &&
                   existingEvent->m_WorldStateData->IsEqualTo(worldStateData))
                {
                    eventToUse = existingEvent;
                }
            }

            node = node->GetNext();
        }
    }

    if(eventToUse)
    {
#if ENABLE_NETWORK_LOGGING
		// log the event data
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "UPDATING_EVENT", "%s_%d", NetworkInterface::GetEventManager().GetEventNameFromType(eventToUse->GetEventType()), eventToUse->GetEventId().GetID());
		NetworkInterface::GetEventManager().GetLog().WriteDataValue("Adding player", "%s", targetPlayer->GetLogName());
		NetworkInterface::GetEventManager().GetLog().WriteDataValue("Player index", "%d", targetPlayer->GetPhysicalPlayerIndex());
#endif //ENABLE_NETWORK_LOGGING
		eventToUse->AddTargetPlayer(*targetPlayer);
    }
    else
    {
		if (CNetworkScenarioBlockingAreaWorldStateData::GetPool()->GetNoOfFreeSpaces())
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CScriptWorldStateEvent *newEvent = rage_new CScriptWorldStateEvent(worldStateData, changeState, targetPlayer);
			NetworkInterface::GetEventManager().PrepareEvent(newEvent);
		}
		else
		{
#if __ASSERT
			gnetAssertf(0,"CScriptWorldStateEvent::Trigger Ran out of CNetworkScenarioBlockingAreaWorldStateData for use in CScriptWorldStateEvent see MAX_SCENARIO_BLOCKING_DATA");
#else
			Warningf("CScriptWorldStateEvent::Trigger Ran out of CNetworkScenarioBlockingAreaWorldStateData for use in CScriptWorldStateEvent MAX_SCENARIO_BLOCKING_DATA");
#endif
		}
    }
}

bool CScriptWorldStateEvent::IsInScope(const netPlayer &player) const
{
    bool inScope = false;

    if(!player.IsLocal())
    {
        inScope = ((1<<player.GetPhysicalPlayerIndex()) & m_PlayersInScope)!=0;
    }

    return inScope;
}

template <class Serialiser> void CScriptWorldStateEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    const unsigned SIZEOF_WORLD_STATE_TYPE = datBitsNeeded<NET_NUM_WORLD_STATES>::COUNT;

    Serialiser serialiser(messageBuffer);

    unsigned type = m_WorldStateData ? m_WorldStateData->GetType() : 0;
    SERIALISE_UNSIGNED(serialiser, type, SIZEOF_WORLD_STATE_TYPE);
    SERIALISE_BOOL(serialiser, m_ChangeState);

    if(m_WorldStateData == 0)
    {
        m_WorldStateData = NetworkScriptWorldStateTypes::CreateWorldState(static_cast<NetScriptWorldStateTypes>(type));
    }

    if(gnetVerifyf(m_WorldStateData, "Invalid world state data, cannot serialise event!"))
    {
        m_WorldStateData->Serialise(serialiser);
        m_WorldStateData->PostSerialise();
    }
}

void CScriptWorldStateEvent::Prepare(datBitBuffer &bitBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataWriter>(bitBuffer);
}

void CScriptWorldStateEvent::Handle(datBitBuffer &bitBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(bitBuffer);

    if(gnetVerifyf(m_WorldStateData, "Invalid world state data, cannot apply event data!"))
    {
        if(m_ChangeState)
        {
            CNetworkScriptWorldStateMgr::ChangeWorldState(*m_WorldStateData, true);
        }
        else
        {
            CNetworkScriptWorldStateMgr::RevertWorldState(*m_WorldStateData, true);
        }
    }
}

bool CScriptWorldStateEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    // the world state data is applied when it is read (see Handle())
    return true;
}

#if ENABLE_NETWORK_LOGGING

void CScriptWorldStateEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(eventLogOnly), datBitBuffer *UNUSED_PARAM(bitBuffer)) const
{
    netLoggingInterface &log = eventLogOnly ? netInterface::GetEventManager().GetLog() : netInterface::GetEventManager().GetLogSplitter();

    if(!log.IsEnabled())
    {
        return;
    }

    log.WriteDataValue("World State", "%s_%s", m_ChangeState ? "CHANGE" : "REVERT", m_WorldStateData ? m_WorldStateData->GetName() : "?");

    if(m_WorldStateData)
    {
        m_WorldStateData->LogState(log);
    }
}

#endif //ENABLE_NETWORK_LOGGING


// ===========================================================================================================
// ORDER ENTITY EVENT
// ===========================================================================================================

CIncidentEntityEvent::CIncidentEntityEvent() :
netGameEvent( INCIDENT_ENTITY_EVENT, false, false ),
m_incidentID(0),
m_dispatchType(DT_Max),
m_entityAction(NUM_ENTITY_ACTIONS),
m_hasBeenAddressed(false)
{
}

CIncidentEntityEvent::CIncidentEntityEvent(unsigned incidentID, unsigned dispatchType, eEntityAction entityAction) :
netGameEvent( INCIDENT_ENTITY_EVENT, false, false ),
m_incidentID(incidentID),
m_dispatchType(dispatchType),
m_entityAction(entityAction),
m_hasBeenAddressed(false)
{
}

CIncidentEntityEvent::CIncidentEntityEvent(unsigned incidentID, bool m_hasBeenAddressed) :
netGameEvent( INCIDENT_ENTITY_EVENT, false, false ),
m_incidentID(incidentID),
m_dispatchType(DT_Max),
m_entityAction(NUM_ENTITY_ACTIONS),
m_hasBeenAddressed(m_hasBeenAddressed)
{
}

void CIncidentEntityEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CIncidentEntityEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CIncidentEntityEvent::Trigger(unsigned incidentIndex, DispatchType dispatchType, eEntityAction entityAction)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CIncidentEntityEvent *pEvent = rage_new CIncidentEntityEvent(incidentIndex, (u32)dispatchType, entityAction);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CIncidentEntityEvent::Trigger(unsigned incidentIndex, bool hasBeenAddressed)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CIncidentEntityEvent *pEvent = rage_new CIncidentEntityEvent(incidentIndex, hasBeenAddressed);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CIncidentEntityEvent::IsInScope( const netPlayer& player ) const
{
	if (CNetwork::GetArrayManager().GetIncidentsArrayHandler())
	{
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncidentFromIncidentId(m_incidentID);

		if (pIncident)
		{
			const netPlayer* pArbitrator = CNetwork::GetArrayManager().GetIncidentsArrayHandler()->GetElementArbitration(pIncident->GetIncidentIndex());

			if (pArbitrator)
			{
				if (pArbitrator->IsLocal())
				{
					// the incident is now local
					TriggerEntityAction(pIncident);
				}
				else 
				{
					return pArbitrator == &player;
				}
			}
		}
	}

	return false;
}

template <class Serialiser> void CIncidentEntityEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_incidentID, CIncidentsArrayHandler::GetSizeOfIncidentID());

	bool bDispatch = m_dispatchType != DT_Max;

	SERIALISE_BOOL(serialiser, bDispatch);

	if (bDispatch)
	{
		SERIALISE_UNSIGNED(serialiser, m_dispatchType, SIZEOF_DISPATCH_TYPE);
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_entityAction), SIZEOF_ENTITY_ACTION);
	}
	else
	{
		SERIALISE_BOOL(serialiser, m_hasBeenAddressed);
	}
}

void CIncidentEntityEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CIncidentEntityEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CIncidentEntityEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	if (CNetwork::GetArrayManager().GetOrdersArrayHandler())
	{
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncidentFromIncidentId(m_incidentID);

		if (pIncident && pIncident->IsLocallyControlled())
		{
			if (!pIncident->GetNetworkObject() || (!pIncident->GetNetworkObject()->IsClone() && !pIncident->GetNetworkObject()->IsPendingOwnerChange()))
			{
				TriggerEntityAction(pIncident);
				return true;
			}	
		}
	}

	return false;
}

bool CIncidentEntityEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == INCIDENT_ENTITY_EVENT)
	{
		const CIncidentEntityEvent *incidentEntityEvent = SafeCast(const CIncidentEntityEvent, pEvent);
		return (incidentEntityEvent->m_incidentID == m_incidentID &&
				incidentEntityEvent->m_dispatchType == m_dispatchType &&
				incidentEntityEvent->m_entityAction == m_entityAction &&
				incidentEntityEvent->m_hasBeenAddressed == m_hasBeenAddressed);
	}

	return false;
}

bool CIncidentEntityEvent::MustPersistWhenOutOfScope() const
{
	// if there is no arbitrator for the incident, the event must be kept until there is one
	if (CNetwork::GetArrayManager().GetIncidentsArrayHandler())
	{
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncidentFromIncidentId(m_incidentID);

		if (pIncident)
		{
			const netPlayer* pArbitrator = CNetwork::GetArrayManager().GetIncidentsArrayHandler()->GetElementArbitration(pIncident->GetIncidentIndex());

			if (!pArbitrator)
			{
				return true;
			}
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CIncidentEntityEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Incident id", "%d", m_incidentID);

	if (m_dispatchType != DT_Max)
	{
		switch (m_dispatchType)
		{
		case DT_PoliceAutomobile:
			log.WriteDataValue("Dispatch Type", "Police Auto");
			break;
		case DT_PoliceHelicopter:
			log.WriteDataValue("Dispatch Type", "Police Heli");
			break;
		case DT_FireDepartment:
			log.WriteDataValue("Dispatch Type", "Fire Dept");
			break;
		case DT_SwatAutomobile:
			log.WriteDataValue("Dispatch Type", "Swat Auto");
			break;
		case DT_AmbulanceDepartment:
			log.WriteDataValue("Dispatch Type", "Ambulance Dept");
			break;
		case DT_PoliceRiders:
			log.WriteDataValue("Dispatch Type", "Police Riders");
			break;
		case DT_PoliceVehicleRequest:
			log.WriteDataValue("Dispatch Type", "Police Vehicle Request");
			break;
		case DT_PoliceRoadBlock:
			log.WriteDataValue("Dispatch Type", "Police Road Block");
			break;
		case DT_Gangs:
			log.WriteDataValue("Dispatch Type", "Gangs");
			break;
		case DT_SwatHelicopter:
			log.WriteDataValue("Dispatch Type", "Swat Heli");
			break;
		case DT_PoliceBoat:
			log.WriteDataValue("Dispatch Type", "Police Boat");
			break;
		case DT_ArmyVehicle:
			log.WriteDataValue("Dispatch Type", "Army Vehicle");
			break;
		case DT_BikerBackup:
			log.WriteDataValue("Dispatch Type", "Biker Backup");
			break;
		case DT_Assassins:
			log.WriteDataValue("Dispatch Type", "Assassins");
			break;
		default: 
			gnetAssertf(0, "Urecognised dispatch type!");
		}

		switch (m_entityAction)
		{
		case ENTITY_ACTION_ARRIVED:
			log.WriteDataValue("Entity action", "Arrived");
			break;
		case ENTITY_ACTION_FINISHED:
			log.WriteDataValue("Entity action", "Finished");
			break;
		case NUM_ENTITY_ACTIONS:
			break;
		}
	}
	else
	{
		log.WriteDataValue("Has Been Addressed", "%s", m_hasBeenAddressed ? "True" : "False");
	}
}

#endif //ENABLE_NETWORK_LOGGING

void CIncidentEntityEvent::TriggerEntityAction(CIncident* pIncident) const
{
	// use the incident index to make sure that this is the correct order (it may have been removed and another created in its place)
	if (pIncident)
	{
		if (m_dispatchType != DT_Max)
		{
			switch (m_entityAction)
			{
			case ENTITY_ACTION_ARRIVED:
				pIncident->SetArrivedResources(m_dispatchType, pIncident->GetArrivedResources(m_dispatchType)+1);
				break;
			case ENTITY_ACTION_FINISHED:
				pIncident->SetFinishedResources(m_dispatchType, pIncident->GetFinishedResources(m_dispatchType)+1);
				break;
			case NUM_ENTITY_ACTIONS:
				break;
			}
		}
		else
		{
			pIncident->SetHasBeenAddressed(m_hasBeenAddressed);
		}
	}
}


// ===========================================================================================================
// CLEAR AREA EVENT
// ===========================================================================================================

CClearAreaEvent::CClearAreaEvent() :
netGameEvent( CLEAR_AREA_EVENT, false, false ),
m_areaCentre(VEC3_ZERO),
m_areaRadius(0.0f),
m_flags(0)
{
	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

CClearAreaEvent::CClearAreaEvent(const CGameScriptId& scriptId, const Vector3& areaCentre, float areaRadius, u32 flags) :
netGameEvent( CLEAR_AREA_EVENT, false, false ),
m_scriptId(scriptId),
m_areaCentre(areaCentre),
m_areaRadius(areaRadius),
m_flags(flags)
{
	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

void CClearAreaEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CClearAreaEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CClearAreaEvent::Trigger(const CGameScriptId& scriptId, const Vector3& areaCentre, float areaRadius, u32 flags)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CClearAreaEvent *pEvent = rage_new CClearAreaEvent(scriptId, areaCentre, areaRadius, flags);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CClearAreaEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

template <class Serialiser> void CClearAreaEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_POSITION(serialiser, m_areaCentre, "Center");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_areaRadius, static_cast<float>(MAX_AREA_RADIUS), SIZEOF_AREA_RADIUS, "Radius");
	SERIALISE_UNSIGNED(serialiser, m_flags, CLEAR_AREA_NUM_FLAGS, "Flags");
}

void CClearAreaEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CClearAreaEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CClearAreaEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	bool bLeaveCarGens = (m_flags & CLEAR_AREA_FLAG_CARGENS) == 0;

	if (m_flags & CLEAR_AREA_FLAG_ALL)
	{
		CGameWorld::ClearExcitingStuffFromArea(m_areaCentre, m_areaRadius, (m_flags & CLEAR_AREA_FLAG_PROJECTILES) != 0, false, false, bLeaveCarGens, NULL, true, true);
	}
	else
	{
		if (m_flags & CLEAR_AREA_FLAG_PEDS)
		{
			CGameWorld::ClearPedsFromArea(m_areaCentre, m_areaRadius);
		}

		if (m_flags & CLEAR_AREA_FLAG_VEHICLES)
		{
			bool bCheckFrustum = ((m_flags & CLEAR_AREA_FLAG_CHECK_ALL_PLAYERS_FRUSTUMS)!=0);
			bool bIfWrecked = ((m_flags & CLEAR_AREA_FLAG_WRECKED_STATUS)!=0);
			bool bIfAbandoned = ((m_flags & CLEAR_AREA_FLAG_ABANDONED_STATUS)!=0);
			bool bIfEngineOnFire = ((m_flags & CLEAR_AREA_FLAG_ENGINE_ON_FIRE)!=0);

			CGameWorld::ClearCarsFromArea(m_areaCentre, m_areaRadius, false, bLeaveCarGens, bCheckFrustum, bIfWrecked, bIfAbandoned,true,bIfEngineOnFire);
		}

		if (m_flags & CLEAR_AREA_FLAG_OBJECTS)
		{
			CGameWorld::ClearObjectsFromArea(m_areaCentre, m_areaRadius);
		}

		if (m_flags & CLEAR_AREA_FLAG_COPS)
		{
			CGameWorld::ClearCopsFromArea(m_areaCentre, m_areaRadius);
		}

		if (m_flags & CLEAR_AREA_FLAG_PROJECTILES)
		{
			CGameWorld::ClearProjectilesFromArea(m_areaCentre, m_areaRadius);
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CClearAreaEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Script", "%s", m_scriptId.GetLogName());
	log.WriteDataValue("Centre", "%f, %f, %f", m_areaCentre.x, m_areaCentre.y, m_areaCentre.z);
	log.WriteDataValue("Radius", "%f", m_areaRadius);
	log.WriteDataValue("Flags", "%d", m_flags);
}

#endif //ENABLE_NETWORK_LOGGING

bool CClearAreaEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == CLEAR_AREA_EVENT)
	{
		const CClearAreaEvent* pClearAreaEvent = SafeCast(const CClearAreaEvent, pEvent);

		if (pClearAreaEvent)
		{
			if (m_areaCentre == pClearAreaEvent->m_areaCentre &&
				m_areaRadius == pClearAreaEvent->m_areaRadius &&
				m_flags == pClearAreaEvent->m_flags)
			{
#if __DEV
				if (m_timeCreated - pClearAreaEvent->m_timeCreated < 5000)
				{
					if (m_scriptId == pClearAreaEvent->m_scriptId)
					{
						gnetAssertf(0, "A script clear area event is being triggered multiple times by script %s. This is bad. Centre: %.2f %.2f %.2f, Radius: %.2f, Flags: %d.", 
							m_scriptId.GetLogName(), m_areaCentre.x, m_areaCentre.y, m_areaCentre.z, m_areaRadius, pClearAreaEvent->m_flags);
					}
					else
					{
						gnetAssertf(0, "A script clear area event is being triggered multiple times by scripts %s and %s. This is bad. Centre: %.2f %.2f %.2f, Radius: %.2f, Flags: %d.", 
							m_scriptId.GetLogName(), pClearAreaEvent->m_scriptId.GetLogName(), m_areaCentre.x, m_areaCentre.y, m_areaCentre.z, m_areaRadius, pClearAreaEvent->m_flags);
					}
				}
#endif
				return true;
			}
		}
	}

	return false;
}

// ===========================================================================================================
// CLEAR RECTANGLE AREA EVENT
// ===========================================================================================================

CClearRectangleAreaEvent::CClearRectangleAreaEvent() :
	netGameEvent( CLEAR_RECTANGLE_AREA_EVENT, false, false ),
	m_minPosition(0.f, 0.f, 0.f), m_maxPosition(0.f, 0.f, 0.f)
{
	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

CClearRectangleAreaEvent::CClearRectangleAreaEvent(const CGameScriptId& scriptId, Vector3 minPos, Vector3 maxPos) :
	netGameEvent( CLEAR_RECTANGLE_AREA_EVENT, false, false ),
	m_scriptId(scriptId),
	m_minPosition(minPos), m_maxPosition(maxPos) 
{
	DEV_ONLY(m_timeCreated = fwTimer::GetTimeInMilliseconds());
}

void CClearRectangleAreaEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CClearRectangleAreaEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CClearRectangleAreaEvent::Trigger(const CGameScriptId& scriptId, const Vector3* minPos, const Vector3* maxPos)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CClearRectangleAreaEvent *pEvent = rage_new CClearRectangleAreaEvent(scriptId, *minPos, *maxPos);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CClearRectangleAreaEvent::IsInScope(const netPlayer& player) const
{
	return !player.IsLocal();
}

template <class Serialiser> void CClearRectangleAreaEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	
	SERIALISE_POSITION(serialiser, m_minPosition, "Min Position");
	SERIALISE_POSITION(serialiser, m_maxPosition, "Max Position");
}

void CClearRectangleAreaEvent::Prepare( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CClearRectangleAreaEvent::Handle( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CClearRectangleAreaEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	CTheCarGenerators::RemoveCarsFromGeneratorsInArea( m_minPosition.x, m_minPosition.y, m_minPosition.z,
													   m_maxPosition.x, m_maxPosition.y, m_maxPosition.z);
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CClearRectangleAreaEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Script", "%s", m_scriptId.GetLogName());
	log.WriteDataValue("Min", "X:%f, Y:%f, Z:%f", m_minPosition.x, m_minPosition.y, m_minPosition.z);
	log.WriteDataValue("Max", "X:%f, Y:%f, Z:%f", m_maxPosition.x, m_maxPosition.y, m_maxPosition.z);
}

#endif //ENABLE_NETWORK_LOGGING

bool CClearRectangleAreaEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == CLEAR_RECTANGLE_AREA_EVENT)
	{
		const CClearRectangleAreaEvent* pClearRectangleAreaEvent = SafeCast(const CClearRectangleAreaEvent, pEvent);

		if (pClearRectangleAreaEvent)
		{
			if (m_minPosition.x == pClearRectangleAreaEvent->m_minPosition.x &&
				m_minPosition.y == pClearRectangleAreaEvent->m_minPosition.y &&
				m_minPosition.z == pClearRectangleAreaEvent->m_minPosition.z &&
				m_maxPosition.x == pClearRectangleAreaEvent->m_maxPosition.x &&
				m_maxPosition.y == pClearRectangleAreaEvent->m_maxPosition.y &&
				m_maxPosition.z == pClearRectangleAreaEvent->m_maxPosition.z)
			{
#if __DEV
				if (m_timeCreated - pClearRectangleAreaEvent->m_timeCreated < 5000)
				{
					if (m_scriptId == pClearRectangleAreaEvent->m_scriptId)
					{
						gnetAssertf(0, "A script clear rectangle area event is being triggered multiple times by script %s. This is bad.", m_scriptId.GetLogName());
					}
					else
					{
						gnetAssertf(0, "A script clear rectangle area event is being triggered multiple times by scripts %s and %s. This is bad.", m_scriptId.GetLogName(), pClearRectangleAreaEvent->m_scriptId.GetLogName());
					}
				}
#endif
				return true;
			}
		}
	}

	return false;
}

// ===========================================================================================================
// REQUEST NETWORK SYNCED SCENE EVENT
// ===========================================================================================================

CRequestNetworkSyncedSceneEvent::CRequestNetworkSyncedSceneEvent() :
netGameEvent( NETWORK_REQUEST_SYNCED_SCENE_EVENT, false, false )
, m_SceneID(-1)
, m_TargetPlayerID(INVALID_PLAYER_INDEX)
{
}

CRequestNetworkSyncedSceneEvent::CRequestNetworkSyncedSceneEvent(int sceneID, netPlayer &targetPlayer) :
netGameEvent( NETWORK_REQUEST_SYNCED_SCENE_EVENT, false, false )
, m_SceneID(sceneID)
, m_TargetPlayerID(targetPlayer.GetPhysicalPlayerIndex())
{
}

void CRequestNetworkSyncedSceneEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRequestNetworkSyncedSceneEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestNetworkSyncedSceneEvent::Trigger(int sceneID, netPlayer &targetPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CRequestNetworkSyncedSceneEvent *pEvent = rage_new CRequestNetworkSyncedSceneEvent(sceneID, targetPlayer);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRequestNetworkSyncedSceneEvent::IsInScope( const netPlayer& player ) const
{
	return player.GetPhysicalPlayerIndex() == m_TargetPlayerID;
}

template <class Serialiser> void CRequestNetworkSyncedSceneEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

    SERIALISE_INTEGER(serialiser, m_SceneID, CNetworkSynchronisedScenes::SIZEOF_SCENE_ID);
}

void CRequestNetworkSyncedSceneEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestNetworkSyncedSceneEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestNetworkSyncedSceneEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    if(CNetworkSynchronisedScenes::IsSceneActive(m_SceneID))
    {
        if(!CStartNetworkSyncedSceneEvent::IsSceneStartEventPending(m_SceneID, &fromPlayer))
        {
            CStartNetworkSyncedSceneEvent::Trigger(m_SceneID, CNetworkSynchronisedScenes::GetTimeSceneStarted(m_SceneID), &fromPlayer);
        }
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CRequestNetworkSyncedSceneEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("REQUESTING_NETWORK_SYNCHRONISED_SCENE", "%d", m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

bool CRequestNetworkSyncedSceneEvent::operator==(const netGameEvent* pEvent) const
{
    if(pEvent && pEvent->GetEventType() == NETWORK_REQUEST_SYNCED_SCENE_EVENT)
    {
		const CRequestNetworkSyncedSceneEvent* syncedSceneEvent = SafeCast(const CRequestNetworkSyncedSceneEvent, pEvent);

		if(m_SceneID == syncedSceneEvent->m_SceneID)
		{
			return true;
		}
	}

	return false;
}

// ===========================================================================================================
// START NETWORK SYNCED SCENE EVENT
// ===========================================================================================================

CStartNetworkSyncedSceneEvent::CStartNetworkSyncedSceneEvent() :
netGameEvent( NETWORK_START_SYNCED_SCENE_EVENT, false, false )
, m_SceneID(-1)
, m_NetworkTimeStarted(0)
, m_ReleaseSceneOnCleanup(false)
, m_PlayersInScope(0)
{
}

CStartNetworkSyncedSceneEvent::CStartNetworkSyncedSceneEvent(int sceneID, unsigned networkTimeStarted, const netPlayer *targetPlayer) :
netGameEvent( NETWORK_START_SYNCED_SCENE_EVENT, false, false )
, m_SceneID(sceneID)
, m_NetworkTimeStarted(networkTimeStarted)
, m_ReleaseSceneOnCleanup(false)
, m_PlayersInScope(0)
{
	if(targetPlayer == 0)
	{
		m_PlayersInScope = ~0U;
	}
	else
	{
		PhysicalPlayerIndex playerIndex = targetPlayer->GetPhysicalPlayerIndex();

		if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Sending a world state event to a player with an invalid physical player index!"))
		{
			m_PlayersInScope |= (1<<playerIndex);
		}
	}
}

CStartNetworkSyncedSceneEvent::~CStartNetworkSyncedSceneEvent()
{
    if(m_ReleaseSceneOnCleanup)
    {
        CNetworkSynchronisedScenes::ReleaseScene(m_SceneID);
    }
}

void CStartNetworkSyncedSceneEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CStartNetworkSyncedSceneEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStartNetworkSyncedSceneEvent::Trigger(int sceneID, unsigned networkTimeStarted, const netPlayer *targetPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CStartNetworkSyncedSceneEvent *pEvent = rage_new CStartNetworkSyncedSceneEvent(sceneID, networkTimeStarted, targetPlayer);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CStartNetworkSyncedSceneEvent::IsInScope( const netPlayer& player ) const
{
	bool inScope = false;

	if(!player.IsLocal())
	{
		inScope = ((1<<player.GetPhysicalPlayerIndex()) & m_PlayersInScope)!=0;
		if(inScope)
		{
			inScope = CNetworkSynchronisedScenes::IsInScope(player, m_SceneID);
		}
	}
	
	return inScope;
}

bool CStartNetworkSyncedSceneEvent::HasTimedOut() const
{
    return !CNetworkSynchronisedScenes::IsSceneActive(m_SceneID);
}

template <class Serialiser> void CStartNetworkSyncedSceneEvent::SerialiseEvent(datBitBuffer &messageBuffer, bool stopOldScenes)
{
	Serialiser serialiser(messageBuffer);

    const unsigned SIZEOF_NETWORK_TIME_STARTED = 32;

    SERIALISE_INTEGER(serialiser, m_SceneID, CNetworkSynchronisedScenes::SIZEOF_SCENE_ID);
    SERIALISE_UNSIGNED(serialiser, m_NetworkTimeStarted, SIZEOF_NETWORK_TIME_STARTED);

    if(stopOldScenes)
    {
        if(CNetworkSynchronisedScenes::IsSceneActive(m_SceneID) && CNetworkSynchronisedScenes::GetTimeSceneStarted(m_SceneID) != m_NetworkTimeStarted)
        {
            CNetworkSynchronisedScenes::StopSceneRunning(m_SceneID LOGGING_ONLY(, "CStartNetworkSyncedSceneEvent::SerialiseEvent"));
        }
    }

    CNetworkSynchronisedScenes::Serialise(serialiser, m_SceneID);
}

void CStartNetworkSyncedSceneEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer, false);
}

void CStartNetworkSyncedSceneEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer, true);

    CNetworkSynchronisedScenes::PostSceneUpdateFromNetwork(m_SceneID, m_NetworkTimeStarted);
}

bool CStartNetworkSyncedSceneEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	if(CNetworkSynchronisedScenes::IsCloneBlockedFromPlayingScene(m_SceneID))
	{
		return true;
	}

    if(CNetworkSynchronisedScenes::TryToStartSceneRunning(m_SceneID))
    {
	    return true;
    }
    else
    {
        // if we failed to start this scene we need to release the scene data as it was allocated during the serialisation
        m_ReleaseSceneOnCleanup = true;
    }

    return false;
}

#if ENABLE_NETWORK_LOGGING

void CStartNetworkSyncedSceneEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	CNetworkSynchronisedScenes::LogSceneDescription(log, m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

bool CStartNetworkSyncedSceneEvent::IsSceneStartEventPending(int sceneID, const netPlayer *toPlayer)
{
    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        netGameEvent *networkEvent = node->Data;

        if (networkEvent && networkEvent->GetEventType() == NETWORK_START_SYNCED_SCENE_EVENT && !networkEvent->IsFlaggedForRemoval())
        {
            CStartNetworkSyncedSceneEvent *startSyncedSceneEvent = static_cast<CStartNetworkSyncedSceneEvent *>(networkEvent);

            if(startSyncedSceneEvent && startSyncedSceneEvent->m_SceneID == sceneID)
            {
                PhysicalPlayerIndex playerIndex = toPlayer ? toPlayer->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;

                if(playerIndex == INVALID_PLAYER_INDEX || startSyncedSceneEvent->GetEventId().MustSendToPlayer(playerIndex))
                {
                    return true;
                }
            }
        }

        node = node->GetNext();
    }

    return false;
}

bool CStartNetworkSyncedSceneEvent::operator==(const netGameEvent* pEvent) const
{
	if(pEvent && pEvent->GetEventType() == NETWORK_START_SYNCED_SCENE_EVENT)
	{
		const CStartNetworkSyncedSceneEvent* syncedSceneEvent = SafeCast(const CStartNetworkSyncedSceneEvent, pEvent);

		if(m_SceneID == syncedSceneEvent->m_SceneID && m_PlayersInScope == syncedSceneEvent->m_PlayersInScope)
		{
			return true;
		}
	}

	return false;
}

// ===========================================================================================================
// STOP NETWORK SYNCED SCENE EVENT
// ===========================================================================================================

CStopNetworkSyncedSceneEvent::CStopNetworkSyncedSceneEvent() :
netGameEvent( NETWORK_STOP_SYNCED_SCENE_EVENT, false, false ),
m_SceneID(-1)
{
}

CStopNetworkSyncedSceneEvent::CStopNetworkSyncedSceneEvent(int sceneID) :
netGameEvent( NETWORK_STOP_SYNCED_SCENE_EVENT, false, false ),
m_SceneID(sceneID)
{
}

void CStopNetworkSyncedSceneEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CStopNetworkSyncedSceneEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStopNetworkSyncedSceneEvent::Trigger(int sceneID)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CStopNetworkSyncedSceneEvent *pEvent = rage_new CStopNetworkSyncedSceneEvent(sceneID);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CStopNetworkSyncedSceneEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

bool CStopNetworkSyncedSceneEvent::HasTimedOut() const
{
    // when this event is triggered the local scene should already have been stopped, if it becomes active again another
    // scene is running with the same network scene ID, so we stop sending the event to prevent cancelling this new scene
    return CNetworkSynchronisedScenes::IsSceneActive(m_SceneID);
}

template <class Serialiser> void CStopNetworkSyncedSceneEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

    SERIALISE_INTEGER(serialiser, m_SceneID, CNetworkSynchronisedScenes::SIZEOF_SCENE_ID);
}

void CStopNetworkSyncedSceneEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CStopNetworkSyncedSceneEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CStopNetworkSyncedSceneEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	CNetworkSynchronisedScenes::StopSceneRunning(m_SceneID LOGGING_ONLY(, "CStopNetworkSyncedSceneEvent::Decide"));

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CStopNetworkSyncedSceneEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("STOPPING_NETWORK_SYNCHRONISED_SCENE", "%d", m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// UPDATE NETWORK SYNCED SCENE EVENT
// ===========================================================================================================

CUpdateNetworkSyncedSceneEvent::CUpdateNetworkSyncedSceneEvent() :
netGameEvent( NETWORK_UPDATE_SYNCED_SCENE_EVENT, false, false ),
m_SceneID(-1),
m_Rate(1.0f)
{
}

CUpdateNetworkSyncedSceneEvent::CUpdateNetworkSyncedSceneEvent(int sceneID, float const rate) :
netGameEvent( NETWORK_UPDATE_SYNCED_SCENE_EVENT, false, false ),
m_SceneID(sceneID),
m_Rate(rate)
{
}

CUpdateNetworkSyncedSceneEvent::~CUpdateNetworkSyncedSceneEvent()
{}

void CUpdateNetworkSyncedSceneEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CUpdateNetworkSyncedSceneEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CUpdateNetworkSyncedSceneEvent::Trigger(int sceneID, float const rate)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CUpdateNetworkSyncedSceneEvent *pEvent = rage_new CUpdateNetworkSyncedSceneEvent(sceneID, rate);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CUpdateNetworkSyncedSceneEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

bool CUpdateNetworkSyncedSceneEvent::HasTimedOut() const
{
    return !CNetworkSynchronisedScenes::IsSceneActive(m_SceneID);
}

template <class Serialiser> void CUpdateNetworkSyncedSceneEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

    SERIALISE_INTEGER(serialiser, m_SceneID, CNetworkSynchronisedScenes::SIZEOF_SCENE_ID);
	
	//---
    
	const unsigned SIZEOF_RATE = 8;
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Rate, 2.0f, SIZEOF_RATE);

	//---

	// to sync entire scene description if we need to sync more members - remove m_Rate from class and just use line below
	// CNetworkSynchronisedScenes::Serialise(serialiser, m_SceneID);

	//---
}

void CUpdateNetworkSyncedSceneEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CUpdateNetworkSyncedSceneEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CUpdateNetworkSyncedSceneEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	// The scene may have been turned off locally by the CNetworkSyncronisedScene code 
	// (because it in turn has been turned off by the fwAnimDirectorComponentSyncedScene code) 
	// so this check to see if the scene is active before setting....
	if(CNetworkSynchronisedScenes::IsSceneActive(m_SceneID))
	{
		CNetworkSynchronisedScenes::SetSceneRate(m_SceneID, m_Rate);

		int localSceneID = CNetworkSynchronisedScenes::GetLocalSceneID(m_SceneID);

		if(-1 != localSceneID)
		{
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneRate((fwSyncedSceneId)localSceneID, m_Rate);
			return true;
		}
	}

	// scene doesn't exist locally..
	return false;
}

bool CUpdateNetworkSyncedSceneEvent::operator==(const netGameEvent* pEvent) const
{
	if(pEvent && pEvent->GetEventType() == NETWORK_UPDATE_SYNCED_SCENE_EVENT)
    {
		const CUpdateNetworkSyncedSceneEvent* syncedSceneEvent = SafeCast(const CUpdateNetworkSyncedSceneEvent, pEvent);

		if(m_SceneID == syncedSceneEvent->m_SceneID)
		{
			float rhsRate = syncedSceneEvent->m_Rate;
			
			if(Abs(m_Rate - rhsRate) < 0.001f)
			{
				return true;
			}
			else if (!syncedSceneEvent->HasBeenSent())
			{
				m_Rate = syncedSceneEvent->m_Rate;
				return true;
			}
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CUpdateNetworkSyncedSceneEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Scene ID", "%d", m_SceneID);
	log.WriteDataValue("Rate", "%.2f",	 m_Rate);

	//CNetworkSynchronisedScenes::LogSceneDescription(log, m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// GIVE PED SCRIPTED TASK EVENT
// ===========================================================================================================

CGivePedScriptedTaskEvent::CGivePedScriptedTaskEvent(const NetworkEventType eventType) :
netGameEvent( eventType, false, false )
, m_PedID(NETWORK_INVALID_OBJECT_ID)
, m_TaskInfo(0)
, m_TaskType(CTaskTypes::MAX_NUM_TASK_TYPES)
, m_PedNotLocal(false)
, m_TaskGiven(false)
{
}

CGivePedScriptedTaskEvent::CGivePedScriptedTaskEvent(CPed *ped, CTaskInfo *taskInfo, const NetworkEventType eventType) :
netGameEvent( eventType, false, false )
, m_PedID(NETWORK_INVALID_OBJECT_ID)
, m_TaskInfo(taskInfo)
, m_TaskType(CTaskTypes::MAX_NUM_TASK_TYPES)
, m_PedNotLocal(false)
, m_TaskGiven(false)
{
    if(gnetVerifyf(ped, "Trying to give a scripted task network event to a NULL ped!") &&
       gnetVerifyf(ped->GetNetworkObject(), "Trying to give a scripted task network event to a non-networked ped!"))
    {
        m_PedID = ped->GetNetworkObject()->GetObjectID();
    }

    if(gnetVerifyf(m_TaskInfo, "Trying to give a scripted task network event with a NULL task info!"))
    {
        m_TaskType = m_TaskInfo->GetTaskType();
    }
}

CGivePedScriptedTaskEvent::~CGivePedScriptedTaskEvent()
{
    if(m_TaskInfo)
    {
		GivePedScriptedTask();

        delete m_TaskInfo;
        m_TaskInfo = 0;
    }
}

void CGivePedScriptedTaskEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CGivePedScriptedTaskEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGivePedScriptedTaskEvent::Trigger(CPed *ped, CTaskInfo *taskInfo)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

    // the latest script task given to a ped takes precedence so we dump any old events relating
    // to the ped we are giving the task to
    RemoveExistingEvents(ped);

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CGivePedScriptedTaskEvent *pEvent = rage_new CGivePedScriptedTaskEvent(ped, taskInfo);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CGivePedScriptedTaskEvent::AreEventsPending(ObjectId pedID)
{
    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        netGameEvent *currentEvent = node->Data;

        if(currentEvent && 
		  (currentEvent->GetEventType() == NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT || currentEvent->GetEventType() == NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT ) && !currentEvent->IsFlaggedForRemoval())
        {
            CGivePedScriptedTaskEvent *givePedScriptedTaskEvent = static_cast<CGivePedScriptedTaskEvent *>(currentEvent);

            if(givePedScriptedTaskEvent->m_PedID == pedID)
            {
                return true;
            }
        }

        node = node->GetNext();
    }

    return false;
}

bool CGivePedScriptedTaskEvent::IsInScope( const netPlayer& player ) const
{
	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

	if (networkObject)
	{
		if (networkObject->IsClone())
		{
			return (&player == networkObject->GetPlayerOwner());
		}
		else if (networkObject->GetPendingPlayerOwner())
		{
			return (&player == networkObject->GetPendingPlayerOwner());
		}
	}

	return false;
}

bool CGivePedScriptedTaskEvent::HasTimedOut() const
{
    netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

    return networkObject == 0;
}

template <class Serialiser> void CGivePedScriptedTaskEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

    const unsigned SIZEOF_TASK_TYPE = datBitsNeeded<CTaskTypes::MAX_NUM_TASK_TYPES>::COUNT;

    SERIALISE_OBJECTID(serialiser, m_PedID, "Ped ID");
    SERIALISE_UNSIGNED(serialiser, m_TaskType, SIZEOF_TASK_TYPE, "Task Type");
}

void CGivePedScriptedTaskEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);

    if(gnetVerifyf(m_TaskInfo, "Trying to serialise an scripted task network event without a task info!"))
    {
        m_TaskInfo->WriteSpecificTaskInfo(messageBuffer);
	}
}

void CGivePedScriptedTaskEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

    gnetAssertf(!m_TaskInfo, "Handling a scripted task network event with an already set task info!");

	m_TaskInfo = CQueriableInterface::CreateTaskInformationForNetwork(m_TaskType, true, PED_TASK_PRIORITY_PRIMARY, INVALID_TASK_SEQUENCE_ID);
	
	if (gnetVerifyf(m_TaskInfo, "Failed to create task info for scripted task network event!"))
	{
		m_TaskInfo->ReadSpecificTaskInfo(messageBuffer);
	}
}

bool CGivePedScriptedTaskEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	return GivePedScriptedTask();
}

#if ENABLE_NETWORK_LOGGING

void CGivePedScriptedTaskEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

    log.WriteDataValue("Ped Given Task",   networkObject ? networkObject->GetLogName() : "None");
    log.WriteDataValue("Task Type Given",  TASKCLASSINFOMGR.GetTaskName(m_TaskType));
    if(m_TaskInfo)
    {
        m_TaskInfo->LogSpecificTaskInfo(log);
 	}
}

void CGivePedScriptedTaskEvent::WriteDecisionToLogFile() const
{
	netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

	if (m_PedNotLocal)
	{
		log.WriteDataValue("Ignored", "Ped not local");
	}
}

#endif // ENABLE_NETWORK_LOGGING

void CGivePedScriptedTaskEvent::RemoveExistingEvents(CPed *ped)
{
    if(gnetVerifyf(ped, "Invalid ped specified!") &&
       gnetVerifyf(ped->GetNetworkObject(), "Specified ped has no network object!"))
    {
        ObjectId pedID = ped->GetNetworkObject()->GetObjectID();

        atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

        while (node)
        {
            atDNode<netGameEvent*, datBase> *nextNode = node->GetNext();

            netGameEvent *currentEvent = node->Data;

            if(currentEvent &&
			  (currentEvent->GetEventType() == NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT || currentEvent->GetEventType() == NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT ) && !currentEvent->IsFlaggedForRemoval())
			{
                CGivePedScriptedTaskEvent *givePedScriptedTaskEvent = SafeCast(CGivePedScriptedTaskEvent, currentEvent);

                if(givePedScriptedTaskEvent->m_PedID == pedID)
                {
                    // destroy the event
                    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(),
                                                   "DESTROYING_EVENT", "%s_%d\r\n",
                                                   NetworkInterface::GetEventManager().GetEventNameFromType(givePedScriptedTaskEvent->GetEventType()),
                                                   givePedScriptedTaskEvent->GetEventId().GetID());

                    givePedScriptedTaskEvent->GetEventId().ClearPlayersInScope();
                }
            }

            node = nextNode;
        }
    }
}

bool CGivePedScriptedTaskEvent::GivePedScriptedTask()
{
	if (m_TaskGiven)
	{
		return false;
	}

	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);
	CPed      *ped           = NetworkUtils::GetPedFromNetworkObject(networkObject);

	gnetAssertf(!networkObject || (networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) ||
		networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT)), "Giving a scripted task to an ambient ped!");

	if(ped && !ped->IsLocalPlayer())
	{
		// just fail silently if the ped is injured or dead, the ped may have been killed between the time
		// the remote script gave the task and when this event was received
		if(!ped->IsInjured())
		{
			CNetObjPed *netObjPed = SafeCast(CNetObjPed, networkObject);

			// if we aren't in control ignore the event, it will get resent to the new owner
			if (netObjPed->IsClone() || netObjPed->IsPendingOwnerChange())
			{
				m_PedNotLocal = true;

				return false;
			}

			int    scriptedTaskType = m_TaskInfo->GetScriptedTaskType();
			CTask *task             = m_TaskInfo->CreateLocalTask(ped);

			if(task)
			{
				netObjPed->GivePedScriptedTask(task, scriptedTaskType);
			}
		}
	}

	m_TaskGiven = true;

	return true;
}

// ===========================================================================================================
// GIVE PED SEQUENCE TASK EVENT
// ===========================================================================================================

u32										CGivePedSequenceTaskEvent::ms_numSequenceTasks = 0;    
IPedNodeDataAccessor::CTaskData			CGivePedSequenceTaskEvent::ms_sequenceTaskData[IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS];
u32										CGivePedSequenceTaskEvent::ms_sequenceRepeatMode = 0;

CGivePedSequenceTaskEvent::CGivePedSequenceTaskEvent() :
CGivePedScriptedTaskEvent( NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT )
, m_HasSequenceTaskData(false)
, m_SequenceDoesntExist(false)
{
}

CGivePedSequenceTaskEvent::CGivePedSequenceTaskEvent(CPed *ped, CTaskInfo *taskInfo) :
CGivePedScriptedTaskEvent( ped, taskInfo, NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT )
, m_HasSequenceTaskData(false)
, m_SequenceDoesntExist(false)
{
}

void CGivePedSequenceTaskEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CGivePedSequenceTaskEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGivePedSequenceTaskEvent::Trigger(CPed *ped, CTaskInfo *taskInfo)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (!gnetVerifyf(taskInfo && taskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SEQUENCE, "CGivePedSequenceTaskEvent - the task info is not for a sequence task"))
	{
		return;
	}

	// the latest script task given to a ped takes precedence so we dump any old events relating
	// to the ped we are giving the task to
	CGivePedScriptedTaskEvent::RemoveExistingEvents(ped);

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CGivePedSequenceTaskEvent *pEvent = rage_new CGivePedSequenceTaskEvent(ped, taskInfo);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CGivePedSequenceTaskEvent::HasTimedOut() const
{
	return CGivePedScriptedTaskEvent::HasTimedOut() || !GetTaskSequenceList();
}

void CGivePedSequenceTaskEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer )
{
	CGivePedScriptedTaskEvent::Prepare(messageBuffer, toPlayer);

	scriptHandler* pScriptHandler = GetScriptHandler();

	// don't send the task sequence data to another player who is running the script that triggered this event, as he will have the sequence
	if (AssertVerify(pScriptHandler && pScriptHandler->GetNetworkComponent()) &&
		!pScriptHandler->GetNetworkComponent()->IsPlayerAParticipant(toPlayer))
	{
		CTaskSequenceList* pTaskSequenceList = GetTaskSequenceList();

		if (AssertVerify(pTaskSequenceList))
		{
			CNetObjPed::ExtractTaskSequenceData(m_PedID, *pTaskSequenceList, ms_numSequenceTasks, ms_sequenceTaskData, ms_sequenceRepeatMode);

			m_HasSequenceTaskData = true;
		}
	}

	messageBuffer.WriteBool(m_HasSequenceTaskData);

	if (m_HasSequenceTaskData)
	{
		CSyncDataWriter serialiser(messageBuffer);

		SerialiseTaskSequenceData(serialiser);
	}
}

void CGivePedSequenceTaskEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	CGivePedScriptedTaskEvent::Handle(messageBuffer, fromPlayer, toPlayer);

	messageBuffer.ReadBool(m_HasSequenceTaskData);

	if (m_HasSequenceTaskData)
	{
		CSyncDataReader serialiser(messageBuffer);

		SerialiseTaskSequenceData(serialiser);
	}
}

bool CGivePedSequenceTaskEvent::Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

	if (networkObject)
	{
		if (m_HasSequenceTaskData)
		{
			CNetObjPed *netObjPed = SafeCast(CNetObjPed, networkObject);
			netObjPed->SetTaskSequenceData(ms_numSequenceTasks, ms_sequenceTaskData, ms_sequenceRepeatMode);
		}
		else if (!GetTaskSequenceList())
		{
			// if the task sequence data has not being sent and the task sequence does not exist, ignore the event. This will be either because the
			// script has not created the sequence yet, or our local player has stopped running it. The sender will resend the event with the task
			// data when he detects that our player has left the script.
			m_SequenceDoesntExist = true;

			// set this so that GivePedScriptedTask() is not called when the event is destroyed
			m_TaskGiven = true;

			return false;
		}
	}

	return CGivePedScriptedTaskEvent::Decide(fromPlayer, toPlayer);
}

#if ENABLE_NETWORK_LOGGING

void CGivePedSequenceTaskEvent::WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const
{
	CGivePedScriptedTaskEvent::WriteEventToLogFile(wasSent, bEventLogOnly, pMessageBuffer);

	if (m_HasSequenceTaskData && NetworkInterface::GetEventManager().GetLog().IsEnabled())
	{
		CSyncDataLogger serialiser(&NetworkInterface::GetEventManager().GetLog());

		SerialiseTaskSequenceData(serialiser);
	}
}

void CGivePedSequenceTaskEvent::WriteDecisionToLogFile() const
{
	netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!log.IsEnabled())
    {
        return;
    }

	if (m_SequenceDoesntExist)
	{
		log.WriteDataValue("Ignored", "Sequence doesn't exist");
	}
	else
	{
		CGivePedScriptedTaskEvent::WriteDecisionToLogFile();
	}
}

#endif // ENABLE_NETWORK_LOGGING

void CGivePedSequenceTaskEvent::SerialiseTaskSequenceData(CSyncDataBase &serialiser) const
{
	static const unsigned SIZE_OF_NUM_TASKS = datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS>::COUNT;
	static const unsigned SIZE_OF_REPEAT_MODE = 1;

	SERIALISE_UNSIGNED(serialiser, ms_numSequenceTasks, SIZE_OF_NUM_TASKS);

	for (u32 i=0; i<ms_numSequenceTasks; i++)
	{
#if ENABLE_NETWORK_LOGGING
		ms_sequenceTaskData[i].Serialise(serialiser, CQueriableInterface::LogTaskInfo, true);
#else
		ms_sequenceTaskData[i].Serialise(serialiser, NULL, true);
#endif			
	}

	SERIALISE_UNSIGNED(serialiser, ms_sequenceRepeatMode, SIZE_OF_REPEAT_MODE);
}

scriptHandler* CGivePedSequenceTaskEvent::GetScriptHandler() const	
{
	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);
	CPed      *ped           = NetworkUtils::GetPedFromNetworkObject(networkObject);

	CScriptEntityExtension* pScriptExtension = ped ? ped->GetExtension<CScriptEntityExtension>() : NULL;

	if (pScriptExtension)
	{
		return pScriptExtension->GetScriptHandler();
	}

	return NULL;
}

CTaskSequenceList* CGivePedSequenceTaskEvent::GetTaskSequenceList() const
{
	CTaskSequenceList* taskSequenceList = NULL;

	if (m_TaskInfo && m_TaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SEQUENCE)
	{
		CClonedTaskSequenceInfo* pSequenceTaskInfo = SafeCast(CClonedTaskSequenceInfo, m_TaskInfo);

		scriptHandler* pScriptHandler = GetScriptHandler();

		if (pScriptHandler)
		{
			scriptResource* pScriptResource = pScriptHandler->GetScriptResource(pSequenceTaskInfo->GetSequenceResourceId());

			if (pScriptResource && gnetVerifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_SEQUENCE_TASK, "Script sequence resource not found for sequence resource id %d", pSequenceTaskInfo->GetSequenceResourceId()))
			{
				s32 sequenceId = pScriptResource->GetReference();

				if (AssertVerify(sequenceId >= 0 && sequenceId < CTaskSequences::MAX_NUM_SEQUENCE_TASKS))
				{
					taskSequenceList = &CTaskSequences::ms_TaskSequenceLists[sequenceId];

					if (taskSequenceList->IsEmpty())
					{
						taskSequenceList = NULL;
					}
				}
			}
		}
	}

	return taskSequenceList;
}

// ===========================================================================================================
// CLEAR PED TASKS EVENT
// ===========================================================================================================

CClearPedTasksEvent::CClearPedTasksEvent() :
netGameEvent( NETWORK_CLEAR_PED_TASKS_EVENT, false, false )
, m_PedID(NETWORK_INVALID_OBJECT_ID)
, m_ClearTasksImmediately(false)
{
}

CClearPedTasksEvent::CClearPedTasksEvent(CPed *ped, bool clearTasksImmediately) :
netGameEvent( NETWORK_CLEAR_PED_TASKS_EVENT, false, false )
, m_PedID(NETWORK_INVALID_OBJECT_ID)
, m_ClearTasksImmediately(clearTasksImmediately)
{
    if(gnetVerifyf(ped, "Trying to give a clear ped tasks network event to a NULL ped!") &&
       gnetVerifyf(ped->GetNetworkObject(), "Trying to give a clear ped tasks network event to a non-networked ped!"))
    {
        m_PedID = ped->GetNetworkObject()->GetObjectID();
    }
}

void CClearPedTasksEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CClearPedTasksEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CClearPedTasksEvent::Trigger(CPed *ped, bool clearTasksImmediately)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CClearPedTasksEvent *pEvent = rage_new CClearPedTasksEvent(ped, clearTasksImmediately);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CClearPedTasksEvent::IsInScope( const netPlayer& player ) const
{
	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

    return (networkObject && (&player == networkObject->GetPlayerOwner()));
}

bool CClearPedTasksEvent::HasTimedOut() const
{
    netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

    return networkObject == 0 || !networkObject->IsClone();
}

template <class Serialiser> void CClearPedTasksEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_PedID, "Ped ID");
    SERIALISE_BOOL(serialiser, m_ClearTasksImmediately, "Clear Tasks Immediately");
}

void CClearPedTasksEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CClearPedTasksEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CClearPedTasksEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);
    CPed      *ped           = NetworkUtils::GetPedFromNetworkObject(networkObject);

    if(ped && gnetVerifyf(networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) ||
                          networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT), "Clearing ped tasks on an ambient ped!"))
    {
        if(networkObject->IsClone())
        {
            CClearPedTasksEvent::Trigger(ped, m_ClearTasksImmediately);
        }
        else
        {
            // just fail silently if the ped is injured or dead, the ped may have been killed between the time
            // the remote script gave the task and when this event was received
            if(!ped->IsInjured())
            {
                if(m_ClearTasksImmediately)
                {
                    ped->GetPedIntelligence()->FlushImmediately(true);
			        ped->SetIsCrouching(false);
			        ped->StopAllMotion(true);
			        if (ped->GetHelmetComponent())
				        ped->GetHelmetComponent()->DisableHelmet();
			        ped->SetPedResetFlag( CPED_RESET_FLAG_IsDrowning, false );
                }
                else
                {
                    ped->GetPedIntelligence()->ClearTasks();
                }
            }
        }
    }

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CClearPedTasksEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_PedID);

    log.WriteDataValue("Ped Cleared Tasks", networkObject ? networkObject->GetLogName() : "None");
    log.WriteDataValue("Cleared Tasks Immediately", m_ClearTasksImmediately ? "TRUE" : "FALSE");
}

#endif //ENABLE_NETWORK_LOGGING

bool CClearPedTasksEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == NETWORK_CLEAR_PED_TASKS_EVENT)
    {
        const CClearPedTasksEvent *clearPedTasksEvent = SafeCast(const CClearPedTasksEvent, pEvent);
        return (clearPedTasksEvent->m_PedID == m_PedID &&
                clearPedTasksEvent->m_ClearTasksImmediately == m_ClearTasksImmediately);
    }

    return false;
}

// ===========================================================================================================
// START NETWORK PED ARREST EVENT
// ===========================================================================================================

CStartNetworkPedArrestEvent::CStartNetworkPedArrestEvent() :
netGameEvent( NETWORK_START_PED_ARREST_EVENT, false, false ),
m_ArresterPedId(NETWORK_INVALID_OBJECT_ID),
m_ArresteePedId(NETWORK_INVALID_OBJECT_ID),
m_iFlags(0)
{
}

CStartNetworkPedArrestEvent::CStartNetworkPedArrestEvent(CPed *pArresterPed, CPed *pArresteePed, fwFlags8 iFlags) :
netGameEvent( NETWORK_START_PED_ARREST_EVENT, false, false ),
m_ArresterPedId(NetworkUtils::GetObjectIDFromGameObject(pArresterPed)),
m_ArresteePedId(NetworkUtils::GetObjectIDFromGameObject(pArresteePed)),
m_iFlags(iFlags)
{
}

void CStartNetworkPedArrestEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CStartNetworkPedArrestEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStartNetworkPedArrestEvent::Trigger(CPed *pArresterPed, CPed *pArresteePed, fwFlags8 iFlags)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CStartNetworkPedArrestEvent *pEvent = rage_new CStartNetworkPedArrestEvent(pArresterPed, pArresteePed, iFlags);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CStartNetworkPedArrestEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

bool CStartNetworkPedArrestEvent::HasTimedOut() const
{
	//!!TODO!! return !CNetworkSynchronisedScenes::IsSceneActive(m_SceneID);
	return false;
}

template <class Serialiser> void CStartNetworkPedArrestEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_ArresterPedId, "Arrester Ped Id");
	SERIALISE_OBJECTID(serialiser, m_ArresteePedId, "Arrestee Ped Id");
	SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Arrest Flags");
}

void CStartNetworkPedArrestEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CStartNetworkPedArrestEvent::Handle ( datBitBuffer &CNC_MODE_ENABLED_ONLY(messageBuffer), const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
#if CNC_MODE_ENABLED
	if(AssertVerify(NetworkInterface::IsInCopsAndCrooks()))
	{
		SerialiseEvent<CSyncDataReader>(messageBuffer);

		netObject *pArresteeNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_ArresteePedId);
		CPed *pArresteePed = NetworkUtils::GetPedFromNetworkObject(pArresteeNetObject);

		netObject *pArrestingrNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_ArresterPedId);
		CPed *pArrestingPed = NetworkUtils::GetPedFromNetworkObject(pArrestingrNetObject);

		//Check we are being arrested by a cop
		if(pArrestingPed && pArrestingPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest))
		{
			// Check I am the one being arrested
			if (pArresteePed && !pArresteePed->IsNetworkClone())
			{
				if (pArresteePed->IsPlayer() && pArresteePed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
				{
					return;
				}

				//Check the arreting is in range
				TUNE_GROUP_FLOAT(CNC_ARREST, fAllowedEventArrestDistance, 100.0f, 0.0f, 1000.0f, 0.01f);
				Vector3 vArresteePos = VEC3V_TO_VECTOR3(pArresteePed->GetTransform().GetPosition());
				Vector3 vArrestingPos = VEC3V_TO_VECTOR3(pArrestingPed->GetTransform().GetPosition());
				if(vArresteePos.Dist2(vArrestingPos) > rage::square(fAllowedEventArrestDistance))
				{
					return;
				}

				CTaskIncapacitated* incapTask = (CTaskIncapacitated*)pArresteePed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
				if (incapTask)
				{
					incapTask->SetIsArrested(true);
					incapTask->SetPedArrester(pArrestingPed);
				}
			}
		}
	}
#endif //CNC_MODE_ENABLED
}

bool CStartNetworkPedArrestEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	//!!TODO!!CNetworkSynchronisedScenes::StartSceneRunning(m_SceneID);

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CStartNetworkPedArrestEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	//netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	//netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	//CNetworkSynchronisedScenes::LogSceneDescription(log, m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// START NETWORK PED UNCUFF EVENT
// ===========================================================================================================

CStartNetworkPedUncuffEvent::CStartNetworkPedUncuffEvent()
: netGameEvent(NETWORK_START_PED_UNCUFF_EVENT, false, false)
, m_UncufferPedId(NETWORK_INVALID_OBJECT_ID)
, m_CuffedPedId(NETWORK_INVALID_OBJECT_ID)
{
}

CStartNetworkPedUncuffEvent::CStartNetworkPedUncuffEvent(CPed* pUncufferPed, CPed* pCuffedPed) :
netGameEvent( NETWORK_START_PED_UNCUFF_EVENT, false, false ),
m_UncufferPedId(NetworkUtils::GetObjectIDFromGameObject(pUncufferPed)),
m_CuffedPedId(NetworkUtils::GetObjectIDFromGameObject(pCuffedPed))
{
}

void CStartNetworkPedUncuffEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CStartNetworkPedUncuffEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStartNetworkPedUncuffEvent::Trigger(CPed* pUncufferPed, CPed* pCuffedPed)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CStartNetworkPedUncuffEvent *pEvent = rage_new CStartNetworkPedUncuffEvent(pUncufferPed, pCuffedPed);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CStartNetworkPedUncuffEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

bool CStartNetworkPedUncuffEvent::HasTimedOut() const
{
	//!!TODO!! return !CNetworkSynchronisedScenes::IsSceneActive(m_SceneID);
	return false;
}

template <class Serialiser> void CStartNetworkPedUncuffEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_UncufferPedId, "Uncuffer Ped Id");
	SERIALISE_OBJECTID(serialiser, m_CuffedPedId, "Cuffed Ped Id");
}

void CStartNetworkPedUncuffEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CStartNetworkPedUncuffEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

	//netObject *pUncufferNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_UncufferPedId);
	//CPed *pUncufferPed = NetworkUtils::GetPedFromNetworkObject(pUncufferNetObject);

	netObject *pCuffedNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_CuffedPedId);
	CPed *pCuffedPed = NetworkUtils::GetPedFromNetworkObject(pCuffedNetObject);

	if (pCuffedPed && !pCuffedPed->IsNetworkClone())
	{
		// TODO: Start task to uncuff with animations
		// For now, just disable the cuffed flag to kick the ped out of the cuffed task
		pCuffedPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed, false);
	}
}

bool CStartNetworkPedUncuffEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	//!!TODO!!CNetworkSynchronisedScenes::StartSceneRunning(m_SceneID);

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CStartNetworkPedUncuffEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	//netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	//netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	//CNetworkSynchronisedScenes::LogSceneDescription(log, m_SceneID);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// CAR HORN EVENT
// ===========================================================================================================

CCarHornEvent::CCarHornEvent()
	: netGameEvent(NETWORK_CAR_HORN_EVENT, false, false)
	, m_bIsHornOn(false)
	, m_vehicleID(NETWORK_INVALID_OBJECT_ID)
	, m_hornModIndex(0)
{

}

CCarHornEvent::CCarHornEvent(const bool bIsHornOn, const ObjectId vehicleID, u8 hornModIndex)
	: netGameEvent(NETWORK_CAR_HORN_EVENT, false, false)
	, m_bIsHornOn(bIsHornOn)
	, m_vehicleID(vehicleID)
	, m_hornModIndex(hornModIndex)
{

}

void CCarHornEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CCarHornEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CCarHornEvent::Trigger(const bool bIsHornOn, const ObjectId vehicleID, const u8 hornModIndex)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	gnetAssert(NetworkInterface::GetNetworkObject(vehicleID));

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CCarHornEvent* pEvent = rage_new CCarHornEvent(bIsHornOn, vehicleID, hornModIndex);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CCarHornEvent::IsInScope(const netPlayer& player) const
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	if(!pObject)
	{
		gnetAssertf(0, "CCarHornEvent - Invalid vehicle ID supplied!");
		return false;
	}
	
	bool bInScope = true; 
	bInScope &= !player.IsLocal();
	bInScope &= (pObject->IsPendingCloning(player) || pObject->HasBeenCloned(player));
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();

	return bInScope;
}

template <class Serialiser> void CCarHornEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_BOOL(serialiser, m_bIsHornOn, "Horn");
	SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle ID");
	
	bool bHornModIndex = (m_hornModIndex != 0);
	SERIALISE_BOOL(serialiser, bHornModIndex, "bHornModIndex");
	if (bHornModIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_hornModIndex, 8, "m_hornModIndex");
	}
	else
	{
		m_hornModIndex = 0;
	}
}

void CCarHornEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CCarHornEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CCarHornEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	if(pObject)
	{
		CNetObjVehicle* pNetObject = SafeCast(CNetObjVehicle, pObject);
		CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);
		if(pVehicle)
		{
			if(m_bIsHornOn && pVehicle->GetVehicleAudioEntity())
			{
				if (m_hornModIndex != 0)
				{
					pVehicle->GetVariationInstance().SetModIndex(VMT_HORN, m_hornModIndex, pVehicle, false);
				}

				// Fail-safe value for when the horn plays and the host disconnects without sending the CCarHornEvent 'OFF' event
				const f32 HORN_FALLBACK_TIME = 10.f; 

				pVehicle->GetVehicleAudioEntity()->PlayVehicleHorn(HORN_FALLBACK_TIME);
				pNetObject->SetHornAppliedThisFrame();
				pNetObject->SetHornOnFromNetwork(true);
			}
			else if (pVehicle->GetVehicleAudioEntity())
			{
				// this catches when both events arrive in one frame
				if(pNetObject->WasHornAppliedThisFrame())
				{
					pNetObject->SetHornOffPending();
				}
				else
				{
					// turn horn off
					pVehicle->GetVehicleAudioEntity()->StopHorn();
					pNetObject->SetHornOnFromNetwork(false);
				}
			}
		}
	}

	// always acknowledge
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CCarHornEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	netObject* pObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	if(pObject)
	{
		CNetObjVehicle* pNetObject = SafeCast(CNetObjVehicle, pObject);
		CVehicle* pVehicle = pNetObject->GetVehicle();
		if(pVehicle)
			log.WriteDataValue("Vehicle", "%s", pNetObject->GetLogName());
		else
			log.WriteDataValue("Vehicle", "?_%d", m_vehicleID);
	}
	log.WriteDataValue("Horn", "%s", m_bIsHornOn ? "On" : "Off");
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// VOICE DRIVEN MOUTH MOVEMENT FINISHED EVENT
// ===========================================================================================================

CVoiceDrivenMouthMovementFinishedEvent::CVoiceDrivenMouthMovementFinishedEvent()
	: netGameEvent(VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT, false, false)
{

}

void CVoiceDrivenMouthMovementFinishedEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CVoiceDrivenMouthMovementFinishedEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CVoiceDrivenMouthMovementFinishedEvent::Trigger()
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CVoiceDrivenMouthMovementFinishedEvent* pEvent = rage_new CVoiceDrivenMouthMovementFinishedEvent();
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CVoiceDrivenMouthMovementFinishedEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true; 
	bInScope &= !player.IsLocal();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();

	return bInScope;
}

template <class Serialiser> void CVoiceDrivenMouthMovementFinishedEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
}

void CVoiceDrivenMouthMovementFinishedEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CVoiceDrivenMouthMovementFinishedEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CVoiceDrivenMouthMovementFinishedEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	const CNetGamePlayer &player = static_cast< const CNetGamePlayer & >(fromPlayer);
	CPed *pPlayerPed = player.GetPlayerPed();
	if(pPlayerPed)
	{
		gnetAssert(!pPlayerPed->IsLocalPlayer());

		pPlayerPed->StopVoiceDrivenMouthMovement();
	}

	// always acknowledge
	return true;
}

bool CVoiceDrivenMouthMovementFinishedEvent::operator==( const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT)
	{
		return true;
	}

	return false;
}
#if ENABLE_NETWORK_LOGGING

void CVoiceDrivenMouthMovementFinishedEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// ENTITY AREA STATUS EVENT
// ===========================================================================================================

CEntityAreaStatusEvent::CEntityAreaStatusEvent()
: netGameEvent(NETWORK_ENTITY_AREA_STATUS_EVENT, false, false)
, m_nNetworkID(-1)
, m_bIsOccupied(false)
, m_PlayersInScope(0)
{

}

CEntityAreaStatusEvent::CEntityAreaStatusEvent(const CGameScriptId &scriptID, const int nNetworkID, const bool bIsOccupied, const netPlayer* pTargetPlayer)
: netGameEvent(NETWORK_ENTITY_AREA_STATUS_EVENT, false, false)
, m_ScriptID(scriptID)
, m_nNetworkID(nNetworkID)
, m_bIsOccupied(bIsOccupied)
, m_PlayersInScope(0)
{
	if(pTargetPlayer == 0)
		m_PlayersInScope = ~0U;
	else
	{
		PhysicalPlayerIndex playerIndex = pTargetPlayer->GetPhysicalPlayerIndex();
		if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Sending an entity area status event to a player with an invalid physical player index!"))
			m_PlayersInScope |= (1 << playerIndex);
	}
}

void CEntityAreaStatusEvent::AddTargetPlayer(const netPlayer& targetPlayer)
{
	PhysicalPlayerIndex playerIndex = targetPlayer.GetPhysicalPlayerIndex();
	if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Sending a world state event to a player with an invalid physical player index!"))
	{
		if((m_PlayersInScope & (1 << playerIndex)) == 0)
		{
			m_PlayersInScope |= (1<<playerIndex);

			// we need to set the player ID on the event ID to handle the case where the event has already been sent
			GetEventId().SetPlayerInScope(playerIndex);
		}
	}
}

void CEntityAreaStatusEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CEntityAreaStatusEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CEntityAreaStatusEvent::Trigger(const CGameScriptId &scriptID, const int nNetworkID, const bool bIsOccupied, const netPlayer* pTargetPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	gnetAssert(nNetworkID >= 0 && nNetworkID < CNetworkEntityAreaWorldStateData::MAX_NUM_NETWORK_IDS);

	// if a target player has been specified check if there is already an event describing
	// this world state being sent to another player, in this case we can reuse this event
	CEntityAreaStatusEvent* pEventToUse = NULL;
	
	atDNode<netGameEvent*, datBase>* pNode = NetworkInterface::GetEventManager().GetEventListHead();
	while(pNode)
	{
		netGameEvent* pEvent = pNode->Data;
		if(pEvent && pEvent->GetEventType() == NETWORK_ENTITY_AREA_STATUS_EVENT && !pEvent->IsFlaggedForRemoval())
		{
			CEntityAreaStatusEvent* pStatusEvent = static_cast<CEntityAreaStatusEvent*>(pEvent);
			if(pStatusEvent->m_ScriptID == scriptID && pStatusEvent->m_nNetworkID == nNetworkID)
			{
				if(pStatusEvent->m_bIsOccupied != bIsOccupied)
				{
					// destroy the event
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "DESTROYING_EVENT", "%s_%d\r\n", NetworkInterface::GetEventManager().GetEventNameFromType(pStatusEvent->GetEventType()), pStatusEvent->GetEventId().GetID());
					pStatusEvent->GetEventId().ClearPlayersInScope();
				}
				else if(pTargetPlayer && ((1 << pTargetPlayer->GetPhysicalPlayerIndex()) & pStatusEvent->m_PlayersInScope) == 0)
				{
					pEventToUse = pStatusEvent;
					break;
				}
			}
		}
		pNode = pNode->GetNext();
	}

	// apply target player to existing event if we found one
	if(pEventToUse)
		pEventToUse->AddTargetPlayer(*pTargetPlayer);
	else
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CEntityAreaStatusEvent* pEvent = rage_new CEntityAreaStatusEvent(scriptID, nNetworkID, bIsOccupied, pTargetPlayer);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CEntityAreaStatusEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true; 

	bInScope &= !player.IsLocal();
	bInScope &= ((1 << player.GetPhysicalPlayerIndex()) & m_PlayersInScope) != 0;

	return bInScope;
}

template <class Serialiser> void CEntityAreaStatusEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	const unsigned SIZEOF_NETWORK_ID = datBitsNeeded<CNetworkEntityAreaWorldStateData::MAX_NUM_NETWORK_IDS>::COUNT + 1;
	
	Serialiser serialiser(messageBuffer);
	m_ScriptID.Serialise(serialiser);
	SERIALISE_INTEGER(serialiser, m_nNetworkID, SIZEOF_NETWORK_ID, "Network ID");
	SERIALISE_BOOL(serialiser, m_bIsOccupied, "Is Occupied");
}

void CEntityAreaStatusEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CEntityAreaStatusEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CEntityAreaStatusEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	return CNetworkEntityAreaWorldStateData::UpdateOccupiedState(m_ScriptID, m_nNetworkID, fromPlayer.GetPhysicalPlayerIndex(), m_bIsOccupied);
}

bool CEntityAreaStatusEvent::operator==( const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_ENTITY_AREA_STATUS_EVENT)
	{
		const CEntityAreaStatusEvent* pEntityAreaEvent = static_cast<const CEntityAreaStatusEvent*>(pEvent);
		return (pEntityAreaEvent->m_ScriptID == m_ScriptID &&
			    pEntityAreaEvent->m_nNetworkID == m_nNetworkID &&
				pEntityAreaEvent->m_bIsOccupied == m_bIsOccupied &&
				pEntityAreaEvent->m_PlayersInScope == m_PlayersInScope);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CEntityAreaStatusEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Occupied", "%s", m_bIsOccupied ? "true" : "false");
	
	CNetworkEntityAreaWorldStateData* pState = CNetworkEntityAreaWorldStateData::GetWorldState(m_ScriptID, m_nNetworkID);
	if(pState)
		pState->LogState(log);
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// GARAGE OCCUPIED STATUS EVENT
// ===========================================================================================================

CGarageOccupiedStatusEvent::CGarageOccupiedStatusEvent()
: netGameEvent(NETWORK_GARAGE_OCCUPIED_STATUS_EVENT, false, false)
, m_GarageIndex(-1)
, m_boxesOccupied(0)
, m_specificPlayerToSendTo(INVALID_PLAYER_INDEX)
{

}

CGarageOccupiedStatusEvent::CGarageOccupiedStatusEvent(const int nGarageIndex, const BoxFlags boxesOccupied, PhysicalPlayerIndex playerToSendTo)
: netGameEvent(NETWORK_GARAGE_OCCUPIED_STATUS_EVENT, false, false)
, m_GarageIndex(nGarageIndex)
, m_boxesOccupied(boxesOccupied)
, m_specificPlayerToSendTo(playerToSendTo)
{

}

void CGarageOccupiedStatusEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CGarageOccupiedStatusEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGarageOccupiedStatusEvent::Trigger(const int nGarageIndex, const BoxFlags boxesOccupied, PhysicalPlayerIndex playerToSendTo)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsNetworkOpen());
	
	if (AssertVerify(nGarageIndex >= 0 && nGarageIndex < CGarages::GetNumGarages()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CGarageOccupiedStatusEvent* pEvent = rage_new CGarageOccupiedStatusEvent(nGarageIndex, boxesOccupied, playerToSendTo);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CGarageOccupiedStatusEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true; 

	bInScope &= !player.IsLocal();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();

	if (m_specificPlayerToSendTo != INVALID_PLAYER_INDEX && player.GetPhysicalPlayerIndex() != m_specificPlayerToSendTo)
	{
		bInScope = false;
	}
	
	return bInScope;
}

template <class Serialiser> void CGarageOccupiedStatusEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	const unsigned SIZEOF_GARAGE_INDEX = 8;

	Serialiser serialiser(messageBuffer);
	SERIALISE_INTEGER(serialiser, m_GarageIndex, SIZEOF_GARAGE_INDEX, "Garage Index");
	SERIALISE_BITFIELD(serialiser, m_boxesOccupied, sizeof(BoxFlags)<<3, "Boxes Occupied");
}

void CGarageOccupiedStatusEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CGarageOccupiedStatusEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CGarageOccupiedStatusEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	if (AssertVerify(m_GarageIndex >= 0 && m_GarageIndex < CGarages::GetNumGarages()))
	{
		CGarages::aGarages[m_GarageIndex].ReceiveOccupiedStateFromPlayer(fromPlayer.GetPhysicalPlayerIndex(), m_boxesOccupied);
	}
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CGarageOccupiedStatusEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	if (m_GarageIndex >= 0 && m_GarageIndex < CGarages::GetNumGarages())
	{
		log.WriteDataValue("Garage", "%s", CGarages::aGarages[m_GarageIndex].name.GetCStr());
	}
	else
	{
		log.WriteDataValue("Garage", "**INVALID INDEX**");
	}
	log.WriteDataValue("Garage Index", "%d", m_GarageIndex);
	log.WriteDataValue("Boxes Occupied", "%d", m_boxesOccupied);
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// SCRIPT ENTITY STATE CHANGE EVENT
// ===========================================================================================================

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

bool CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters *blockNonTempEventParams = static_cast<CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters *>(otherParameters);

        if(blockNonTempEventParams->m_BlockNonTemporaryEvents == m_BlockNonTemporaryEvents)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters::ApplyParameters(netObject *networkObject)
{
    if(gnetVerifyf(networkObject, "Trying to apply blocking of non temporary events parameters to an invalid network object!"))
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

        if(ped)
        {
            ped->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents, m_BlockNonTemporaryEvents);
        }
    }
}

void CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters::LogParameters(netLoggingInterface &log)
{
    log.WriteDataValue("Block Non Temporary Events", "%s", m_BlockNonTemporaryEvents ? "TRUE" : "FALSE");
}

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

bool CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters *relGroupHashParams = static_cast<CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters *>(otherParameters);

        if(relGroupHashParams->m_relationshipGroupHash == m_relationshipGroupHash)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters::ApplyParameters(netObject *networkObject)
{
    if(gnetVerifyf(networkObject, "Trying to apply blocking of non temporary events parameters to an invalid network object!"))
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

        if(ped)
        {
			CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(m_relationshipGroupHash);

			if (gnetVerifyf(pGroup, "pGroup invalid m_relationshipGroupHash[%u]",m_relationshipGroupHash))
			{
#if __DEV
				CPedTargetting* pPedTargetting = ped->GetPedIntelligence()->GetTargetting(false);
				bool bWasFriendlyWithAnyTarget = pPedTargetting && pPedTargetting->IsFriendlyWithAnyTarget();
#endif
				ped->GetPedIntelligence()->SetRelationshipGroup(pGroup);

				if (pGroup->GetShouldBlipPeds())
				{
					ped_commands::SetRelationshipGroupBlipForPed(ped, true);
				}
				else
				{
					ped_commands::SetRelationshipGroupBlipForPed(ped, false);
				}

#if __DEV
				bool bNowFriendlyWithAnyTarget = pPedTargetting && pPedTargetting->IsFriendlyWithAnyTarget();
				scriptAssertf( bWasFriendlyWithAnyTarget || !bNowFriendlyWithAnyTarget, "%s: SET_RELATIONSHIP_GROUP: setting a char to like chars its already fighting, please use clear char tasks!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
			}
        }
    }
}

void CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters::LogParameters(netLoggingInterface &log)
{
	LogRelationshipGroup(log, m_relationshipGroupHash, "Relationship Group");
}

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

bool CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters *params = static_cast<CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters *>(otherParameters);

        if(params->m_ragdollBlockFlags == m_ragdollBlockFlags)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters::ApplyParameters(netObject *networkObject)
{
    if(gnetVerifyf(networkObject, "Trying to apply blocking of non temporary events parameters to an invalid network object!"))
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

        if(ped)
        {
			ped->ApplyRagdollBlockingFlags(reinterpret_cast<eRagdollBlockingFlagsBitSet&>(m_ragdollBlockFlags));
        }
    }
}

void CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters::LogParameters(netLoggingInterface &log)
{
	LogRelationshipGroup(log, m_ragdollBlockFlags, "Ragdoll blocking flags");
}

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

bool CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed *cruiseSpeedParams = static_cast<CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed *>(otherParameters);

        if(cruiseSpeedParams->m_cruiseSpeed == m_cruiseSpeed)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed::ApplyParameters(netObject *networkObject)
{
    if(gnetVerifyf(networkObject, "Trying to apply cruise speed to an invalid network object!"))
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

        if(ped)
        {
			if(gnetVerifyf(ped->GetMyVehicle() && ped->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "SET_DRIVE_TASK_CRUISE_SPEED - Ped is not in car"))
			{
				if(gnetVerifyf(ped->GetMyVehicle()->GetDriver() == ped, "SET_DRIVE_TASK_CRUISE_SPEED - Ped is not driver of car"))
				{
					CTaskVehicleMissionBase *pCarTask = ped->GetMyVehicle()->GetIntelligence()->GetActiveTask();
					if(pCarTask)
					{
						pCarTask->SetCruiseSpeed(m_cruiseSpeed);
					}
				}
			}
        }
    }
}

void CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed::LogParameters(netLoggingInterface &log)
{
	log.WriteDataValue("Cruise Speed", "%f", m_cruiseSpeed);
}

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CSettingOfLookAtEntity) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::CSettingOfLookAtEntity(const ObjectId objectID, const u32 nFlags, const int nTime) 
{
	Init(objectID, nFlags, nTime);
}

CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::CSettingOfLookAtEntity(netObject* pNetObj, const u32 nFlags, const int nTime) 
{
	Init(pNetObj ? pNetObj->GetObjectID() : NETWORK_INVALID_OBJECT_ID, nFlags, nTime);
}

void CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::Init(const ObjectId objectID, const u32 nFlags, const int nTime)
{
	m_ObjectID = objectID;
	m_nFlags = nFlags;
	m_nTime = nTime;

	// time is an int (defaulting to -1 implying 'until told otherwise') measured in ms
	// round to 1/10th a second accuracy
	m_nTimeCapped = static_cast<u32>((m_nTime / 100) + 1);
	gnetAssertf(m_nTimeCapped < (1u << SIZEOF_TIME), "CSettingOfLookAtEntity :: SIZEOF_TIME needs increased!");
}

CompileTimeAssert(sizeof(CScriptEntityStateChangeEvent::CSettingOfDriveTaskCruiseSpeed) < CScriptEntityStateChangeEvent::MAX_PARAMETER_STORAGE_SIZE);

bool CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSettingOfLookAtEntity *lookAtEntityParams = static_cast<CScriptEntityStateChangeEvent::CSettingOfLookAtEntity *>(otherParameters);

        if(lookAtEntityParams->m_ObjectID    == m_ObjectID &&
           lookAtEntityParams->m_nFlags      == m_nFlags   &&
           lookAtEntityParams->m_nTime       == m_nTime    &&
           lookAtEntityParams->m_nTimeCapped == m_nTimeCapped)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply look at parameters to an invalid network object!"))
	{
		CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

		if(ped)
		{
			netObject* pNetObject = NetworkInterface::GetNetworkObject(m_ObjectID);
			if(pNetObject)
			{
				CEntity* pEntity = pNetObject->GetEntity();

                if(pEntity)
                {
				    int nTime = static_cast<int>(m_nTimeCapped * 100) - 1;
				    eAnimBoneTag boneTag = pEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID;
				    ped->GetIkManager().LookAt(0, pEntity, nTime, boneTag, NULL, m_nFlags, 500, 500, CIkManager::IK_LOOKAT_MEDIUM);
                }
			}
		}
	}
}

void CScriptEntityStateChangeEvent::CSettingOfLookAtEntity::LogParameters(netLoggingInterface &log)
{
	netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_ObjectID);
	log.WriteDataValue("Object", "%s", pObj ? pObj->GetLogName() : "Does Not Exist");
	log.WriteDataValue("Flags", "%d", m_nFlags);
	log.WriteDataValue("Time", "%d", m_nTime);
	log.WriteDataValue("Time Capped", "%d", m_nTimeCapped);
}

bool CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters *aboveTerrainParams = static_cast<CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters *>(otherParameters);

        if(aboveTerrainParams->m_minHeightAboveTerrain == m_minHeightAboveTerrain)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply blocking of non temporary events parameters to an invalid network object!"))
	{
		CPlane *plane = NetworkUtils::GetPlaneFromNetworkObject(networkObject);

		if(plane)
		{
			CTaskVehicleMissionBase* pVehicleTask = plane->GetIntelligence()->GetActiveTask();
			if(pVehicleTask)
			{
				if(pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_PLANE)
				{
					CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane *>(pVehicleTask);
					pTask->SetMinHeightAboveTerrain(m_minHeightAboveTerrain);
				}
			}
		}
	}
}

void CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters::LogParameters(netLoggingInterface &log)
{
	LogRelationshipGroup(log, m_minHeightAboveTerrain, "Min Height Above Terrain");
}

CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::CSettingOfTaskVehicleTempAction(const ObjectId objectID, const int iAction, const int iTime) 
{
	Init(objectID, iAction, iTime);
}

CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::CSettingOfTaskVehicleTempAction(netObject* pNetObj, const int iAction, const int iTime) 
{
	Init(pNetObj ? pNetObj->GetObjectID() : NETWORK_INVALID_OBJECT_ID, iAction, iTime);
}

void CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::Init(const ObjectId objectID, const int iAction, const int iTime)
{
	gnetAssertf(iAction < 255, "CSettingOfTaskVehicleTempAction::Init iAction > 255 [%d] --> will not serialise properly",iAction);
	gnetAssertf(iTime <= 65536, "CSettingOfTaskVehicleTempAction::Init iTime > 65536 [%d] --> will not serialise properly",iTime);

	m_ObjectID = objectID;
	m_iAction = iAction;
	m_bTime = (iTime >= 0);
	m_iTime = iTime;
}

bool CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
	if(otherParameters && otherParameters->GetType() == GetType())
	{
		CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction *taskvehicletempactionparams = static_cast<CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction *>(otherParameters);

		if(taskvehicletempactionparams->m_ObjectID    == m_ObjectID &&
			taskvehicletempactionparams->m_iAction    == m_iAction  &&
			taskvehicletempactionparams->m_iTime      == m_iTime)
		{
			return true;
		}
	}

	return false;
}

void CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply task vehicle temp action parameters to an invalid network object!"))
	{
		CPed* pPed = NetworkUtils::GetPedFromNetworkObject(networkObject);

		if(pPed && !pPed->IsNetworkClone())
		{
			netObject* pNetObject = NetworkInterface::GetNetworkObject(m_ObjectID);
			if(pNetObject && pNetObject->GetEntity() && pNetObject->GetEntity()->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = (CVehicle*) pNetObject->GetEntity();
				if (pPed)
				{
					CTask* pTask=rage_new CTaskCarSetTempAction(pVehicle,m_iAction,m_iTime);
					CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask); //still forever
					pPed->GetPedIntelligence()->AddEvent(event);
				}
			}
		}
	}
}

void CScriptEntityStateChangeEvent::CSettingOfTaskVehicleTempAction::LogParameters(netLoggingInterface &log)
{
	netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_ObjectID);
	log.WriteDataValue("Object", "%s", pObj ? pObj->GetLogName() : "Does Not Exist");
	log.WriteDataValue("Action", "%d", m_iAction);
	log.WriteDataValue("Time", "%d", m_iTime);
}

bool CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride *params = static_cast<CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride *>(otherParameters);

		if(params)
		{
			if((params->m_clear == m_clear) && (params->m_idleClipNameHash == m_idleClipNameHash) && (params->m_idleClipDictNameHash == m_idleClipDictNameHash))
			{
				return true;
			}
		}
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply blocking of non temporary events parameters to an invalid network object!"))
	{
		CPed* ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

		if(ped && ped->GetFacialData())
		{
			if(!m_clear)
			{
				ped->GetFacialData()->SetFacialIdleAnimOverride(ped, m_idleClipNameHash, m_idleClipDictNameHash);
			}
			else
			{
				ped->GetFacialData()->ClearFacialIdleAnimOverride(ped);
			}
		}
	}
}

void CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride::LogParameters(netLoggingInterface &log)
{
    if(m_clear)
    {
	    log.WriteDataValue("Clear", "TRUE");
    }
    else
    {
	    log.WriteDataValue("Clip Name Hash", "%d", m_idleClipNameHash);
	    log.WriteDataValue("Clip Dict Name Hash", "%d", m_idleClipDictNameHash);
    }
}

bool CScriptEntityStateChangeEvent::CSetVehicleLockState::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSetVehicleLockState *params = static_cast<CScriptEntityStateChangeEvent::CSetVehicleLockState*>(otherParameters);

		if(params)
		{
			if((params->m_PlayerLocks                     == m_PlayerLocks) &&
               (params->m_DontTryToEnterIfLockedForPlayer == m_DontTryToEnterIfLockedForPlayer) &&
               (params->m_Flags                           == m_Flags))
			{
				return true;
			}
		}
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSetVehicleLockState::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply vehicle lock state parameters to an invalid network object!"))
	{
		CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

		if(vehicle)
		{
            if((m_Flags & LOCK_STATE) != 0)
            {
                CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, networkObject);
                netObjVehicle->SetPlayerLocks(m_PlayerLocks);
            }

            if((m_Flags & PLAYER_FLAG) != 0)
            {
                vehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = m_DontTryToEnterIfLockedForPlayer;
            }
		}
	}
}

void CScriptEntityStateChangeEvent::CSetVehicleLockState::LogParameters(netLoggingInterface &log)
{
    if((m_Flags & PLAYER_FLAG) != 0)
    {
        log.WriteDataValue("Don't Try To Enter If Locked For Player", "%s", m_DontTryToEnterIfLockedForPlayer ? "TRUE" : "FALSE");
    }

    if((m_Flags & LOCK_STATE) != 0)
    {
	    log.WriteDataValue("Player Locks", "%d", m_PlayerLocks);
    }
}

bool CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver::IsEqual(IScriptEntityStateParametersBase *otherParameters) const
{
    if(otherParameters && otherParameters->GetType() == GetType())
    {
        CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver *setExclusiveDriverParams = static_cast<CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver *>(otherParameters);

        if(setExclusiveDriverParams->m_ExclusiveDriverID == m_ExclusiveDriverID && setExclusiveDriverParams->m_ExclusiveDriverIndex == m_ExclusiveDriverIndex)
        {
            return true;
        }
    }

    return false;
}

void CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver::ApplyParameters(netObject *networkObject)
{
	if(gnetVerifyf(networkObject, "Trying to apply exclusive driver parameters to an invalid network object!"))
	{
		CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

		if(vehicle)
		{
			CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, networkObject);
            netObjVehicle->SetPendingExclusiveDriverID(m_ExclusiveDriverID, m_ExclusiveDriverIndex);
			AI_LOG_WITH_ARGS("[ExclusiveDriver] - Machine with local player %s is setting pending exclusive driver ID on vehicle %s (%p), exclusive driver id %i, index %i\n", AILogging::GetDynamicEntityNameSafe(CGameWorld::FindLocalPlayer()), AILogging::GetDynamicEntityNameSafe(vehicle), vehicle, m_ExclusiveDriverID, m_ExclusiveDriverIndex);
		}
	}
}

void CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver::LogParameters(netLoggingInterface &log)
{
	log.WriteDataValue("Exclusive Driver ID", "%d", m_ExclusiveDriverID);
	log.WriteDataValue("Exclusive Driver Index", "%u", m_ExclusiveDriverIndex);
}

CScriptEntityStateChangeEvent::CScriptEntityStateChangeEvent()
: netGameEvent(SCRIPT_ENTITY_STATE_CHANGE_EVENT, false, false)
, m_ScriptEntityID(NETWORK_INVALID_OBJECT_ID)
, m_StateParameters(0)
, m_NetworkTimeOfChange(0)
{
}

CScriptEntityStateChangeEvent::CScriptEntityStateChangeEvent(netObject *networkObject, const IScriptEntityStateParametersBase &parameters)
: netGameEvent(SCRIPT_ENTITY_STATE_CHANGE_EVENT, false, false)
, m_ScriptEntityID(NETWORK_INVALID_OBJECT_ID)
, m_StateParameters(parameters.Copy(m_ParameterClassStorage))
, m_NetworkTimeOfChange(NetworkInterface::GetNetworkTime())
{
    if(networkObject)
    {
        m_ScriptEntityID = networkObject->GetObjectID();
    }
}

CScriptEntityStateChangeEvent::~CScriptEntityStateChangeEvent()
{
	netObject *networkObject = NetworkInterface::GetNetworkObject(m_ScriptEntityID);

	// the object may have migrated back to us, in which case apply the data
	if(networkObject && !networkObject->IsClone() && !networkObject->IsPendingOwnerChange())
	{
		m_StateParameters->ApplyParameters(networkObject);
	}
}

void CScriptEntityStateChangeEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CScriptEntityStateChangeEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CScriptEntityStateChangeEvent::Trigger(netObject *networkObject, const IScriptEntityStateParametersBase &parameters)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

    if(gnetVerifyf(networkObject, "Trying to change the remote state of an invalid network object!"))
    {
	    NetworkInterface::GetEventManager().CheckForSpaceInPool();
	    CScriptEntityStateChangeEvent* pEvent = rage_new CScriptEntityStateChangeEvent(networkObject, parameters);
	    NetworkInterface::GetEventManager().PrepareEvent(pEvent);
    }
}

bool CScriptEntityStateChangeEvent::IsInScope(const netPlayer& player) const
{
    netObject *networkObject = NetworkInterface::GetNetworkObject(m_ScriptEntityID);

	if (networkObject)
	{
		if (networkObject->IsClone())
		{
			return (&player == networkObject->GetPlayerOwner());
		}
		else if (networkObject->GetPendingPlayerOwner())
		{
			return (&player == networkObject->GetPendingPlayerOwner());
		}
	}

	return false;
}

template <class Serialiser> void CScriptEntityStateChangeEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    const unsigned SIZEOF_STATE_CHANGE_TIME = 32;

	Serialiser serialiser(messageBuffer);

    StateChangeType stateChangeType = m_StateParameters ? m_StateParameters->GetType() : NUM_STATE_CHANGE_TYPES;

    SERIALISE_OBJECTID(serialiser, m_ScriptEntityID);
    SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(stateChangeType), SIZEOF_STATE_CHANGE_TYPE);

    if(m_StateParameters == 0)
    {
        m_StateParameters = CreateParameterClass(stateChangeType);
    }

    SERIALISE_UNSIGNED(serialiser, m_NetworkTimeOfChange, SIZEOF_STATE_CHANGE_TIME);
}

void CScriptEntityStateChangeEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);

    LOGGING_ONLY(NETWORK_QUITF(m_StateParameters, "Trying to serialise a state change event with no parameters!"););
	if(m_StateParameters)
	{
		m_StateParameters->WriteParameters(messageBuffer);
	}
}

void CScriptEntityStateChangeEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

    LOGGING_ONLY(NETWORK_QUITF(m_StateParameters, "Trying to serialise a state change event with no parameters!"););
	if(m_StateParameters)
	{
		m_StateParameters->ReadParameters(messageBuffer);
	}
}

bool CScriptEntityStateChangeEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject *networkObject = NetworkInterface::GetNetworkObject(m_ScriptEntityID);

    if(networkObject && !networkObject->IsClone() && !networkObject->IsPendingOwnerChange())
    {
        LOGGING_ONLY(NETWORK_QUITF(m_StateParameters, "Trying to apply a state change event with no parameters!"););
		if(m_StateParameters)
		{
			m_StateParameters->ApplyParameters(networkObject);
		}
		return true;
    }

    return false;
}

bool CScriptEntityStateChangeEvent::operator==( const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == SCRIPT_ENTITY_STATE_CHANGE_EVENT)
    {
        const CScriptEntityStateChangeEvent *stateChangeEvent = static_cast<const CScriptEntityStateChangeEvent *>(pEvent);
        return (stateChangeEvent->m_ScriptEntityID == m_ScriptEntityID &&
                stateChangeEvent->m_StateParameters &&
                m_StateParameters &&
                stateChangeEvent->m_StateParameters->IsEqual(m_StateParameters));
    }

    return false;
}

#if ENABLE_NETWORK_LOGGING

void CScriptEntityStateChangeEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject *networkObject = NetworkInterface::GetNetworkObject(m_ScriptEntityID);

    if(networkObject)
    {
        log.WriteDataValue("Script Entity", "%s", networkObject->GetLogName());
    }
    else
    {
        log.WriteDataValue("Script Entity", "??_%d", m_ScriptEntityID);
    }

    log.WriteDataValue("State Change Time", "%d", m_NetworkTimeOfChange);

    if(m_StateParameters)
    {
        log.WriteDataValue("State Change Type", "%s", m_StateParameters->GetName());
        m_StateParameters->LogParameters(log);
    }
}

#endif // ENABLE_NETWORK_LOGGING

CScriptEntityStateChangeEvent::IScriptEntityStateParametersBase *CScriptEntityStateChangeEvent::CreateParameterClass(const StateChangeType &stateChangeType)
{
#if !__FINAL
    static const CScriptEntityStateChangeEvent::StateChangeType supportedTypes[] =
    {
        SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,
		SET_PED_RELATIONSHIP_GROUP_HASH,
		SET_DRIVE_TASK_CRUISE_SPEED,
		SET_LOOK_AT_ENTITY,
		SET_PLANE_MIN_HEIGHT_ABOVE_TERRAIN,
		SET_PED_RAGDOLL_BLOCKING_FLAGS,
		SET_TASK_VEHICLE_TEMP_ACTION,
        SET_PED_FACIAL_IDLE_ANIM_OVERRIDE,
        SET_VEHICLE_LOCK_STATE,
        SET_EXCLUSIVE_DRIVER
    };

    // This catches the commonly made mistake of not adding a case for a newly added state change event to the switch
    // statement below - this leads to a fatal crash when the code runs
    CompileTimeAssert((sizeof(supportedTypes) / sizeof(CScriptEntityStateChangeEvent::StateChangeType)) == NUM_STATE_CHANGE_TYPES);
#endif // !__FINAL

    switch(stateChangeType)
    {
    case SET_BLOCKING_OF_NON_TEMPORARY_EVENTS:
        return rage_placement_new(m_ParameterClassStorage) CBlockingOfNonTemporaryEventsParameters();
	case SET_PED_RELATIONSHIP_GROUP_HASH:
		return rage_placement_new(m_ParameterClassStorage) CSettingOfPedRelationshipGroupHashParameters();
	case SET_DRIVE_TASK_CRUISE_SPEED:
		return rage_placement_new(m_ParameterClassStorage) CSettingOfDriveTaskCruiseSpeed();
	case SET_LOOK_AT_ENTITY:
		return rage_placement_new(m_ParameterClassStorage) CSettingOfLookAtEntity();
    case SET_PLANE_MIN_HEIGHT_ABOVE_TERRAIN:
        return rage_placement_new(m_ParameterClassStorage) CSettingOfPlaneMinHeightAboveTerrainParameters();
	case SET_PED_RAGDOLL_BLOCKING_FLAGS:
		return rage_placement_new(m_ParameterClassStorage) CSetPedRagdollBlockFlagParameters();
    case SET_TASK_VEHICLE_TEMP_ACTION:
        return rage_placement_new(m_ParameterClassStorage) CSettingOfTaskVehicleTempAction();
    case SET_PED_FACIAL_IDLE_ANIM_OVERRIDE:
        return rage_placement_new(m_ParameterClassStorage) CSetPedFacialIdleAnimOverride();
    case SET_VEHICLE_LOCK_STATE:
        return rage_placement_new(m_ParameterClassStorage) CSetVehicleLockState();
    case SET_EXCLUSIVE_DRIVER:
        return rage_placement_new(m_ParameterClassStorage) CSetVehicleExclusiveDriver();

	default:
        gnetAssertf(0, "Trying to create a state change parameter class of an unhandled type!");
        return 0;
    };
}

// ================================================================================================================
// PLAY SOUND EVENT
// ================================================================================================================
CPlaySoundEvent::CPlaySoundEvent() 
: netGameEvent(NETWORK_PLAY_SOUND_EVENT, false, false)
, m_bUseEntity(false)
, m_EntityID(NETWORK_INVALID_OBJECT_ID)
, m_Position(VEC3_ZERO)
, m_setNameHash(0)
, m_soundNameHash(0)
, m_SoundID(0)
, m_nRange(0)
{
	m_ScriptId.Invalidate();
}

CPlaySoundEvent::CPlaySoundEvent(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, int nRange)
: netGameEvent(NETWORK_PLAY_SOUND_EVENT, false, false)
, m_bUseEntity(true)
, m_EntityID(NETWORK_INVALID_OBJECT_ID)
, m_setNameHash(setNameHash)
, m_soundNameHash(soundNameHash)
, m_nRange(nRange)
{
	m_ScriptId.Invalidate();

	// retrieve the network object
	if(gnetVerifyf(pEntity != NULL, "CPlaySoundEvent :: NULL entity supplied!"))
	{
		netObject* pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
		if(gnetVerifyf(pNetObject != NULL, "CPlaySoundEvent :: Entity does not have a network object!"))
			m_EntityID = pNetObject->GetObjectID();
	}
}

CPlaySoundEvent::CPlaySoundEvent(const Vector3& vPosition, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange)
: netGameEvent(NETWORK_PLAY_SOUND_EVENT, false, false)
, m_bUseEntity(false)
, m_EntityID(NETWORK_INVALID_OBJECT_ID)
, m_Position(vPosition)
, m_setNameHash(setNameHash)
, m_soundNameHash(soundNameHash)
, m_SoundID(soundID)
, m_ScriptId(scriptId)
, m_nRange(nRange)
{

}

CPlaySoundEvent::CPlaySoundEvent(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange)
: netGameEvent(NETWORK_PLAY_SOUND_EVENT, false, false)
, m_bUseEntity(true)
, m_EntityID(NETWORK_INVALID_OBJECT_ID)
, m_Position(VEC3_ZERO)
, m_setNameHash(setNameHash)
, m_soundNameHash(soundNameHash)
, m_SoundID(soundID)
, m_ScriptId(scriptId)
, m_nRange(nRange)
{
	// retrieve the network object
	if(gnetVerifyf(pEntity != NULL, "CPlaySoundEvent :: NULL entity supplied!"))
	{
		netObject* pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
		if(gnetVerifyf(pNetObject != NULL, "CPlaySoundEvent :: Entity does not have a network object!"))
			m_EntityID = pNetObject->GetObjectID();
	}	
}

void CPlaySoundEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CPlaySoundEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPlaySoundEvent::Trigger(const Vector3& vPosition, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange)
{
	networkAudioDebugf3("CPlaySoundEvent::Trigger vPosition[%f %f %f] setNameHash[%u] soundNameHash[%u] soundID[%d] nRange[%d]",vPosition.x,vPosition.y,vPosition.z,setNameHash,soundNameHash,soundID,nRange);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPlaySoundEvent *pEvent = rage_new CPlaySoundEvent(vPosition, setNameHash, soundNameHash, soundID, scriptId, nRange);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CPlaySoundEvent::Trigger(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange)
{
	networkAudioDebugf3("CPlaySoundEvent::Trigger pEntity[%p][%s] setNameHash[%u] soundNameHash[%u] soundID[%d] nRange[%d]",pEntity,pEntity ? pEntity->GetModelName() : "",setNameHash,soundNameHash,soundID,nRange);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPlaySoundEvent *pEvent = rage_new CPlaySoundEvent(pEntity, setNameHash, soundNameHash, soundID, scriptId, nRange);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CPlaySoundEvent::Trigger(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, int nRange)
{
	networkAudioDebugf3("CPlaySoundEvent::Trigger pEntity[%p][%s] setNameHash[%u] soundNameHash[%u] nRange[%d]",pEntity,pEntity ? pEntity->GetModelName() : "",setNameHash,soundNameHash,nRange);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPlaySoundEvent *pEvent = rage_new CPlaySoundEvent(pEntity, setNameHash, soundNameHash, nRange);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CPlaySoundEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true;
	bInScope &= !player.IsLocal();
	bInScope &= !player.IsBot();

	const CNetGamePlayer* netGamePlayer = SafeCast(const CNetGamePlayer, &player);
	bInScope &= !netGamePlayer->IsInDifferentTutorialSession();

	const CPed* netPed = netGamePlayer->GetPlayerPed();
	bInScope &= (netPed && !NetworkInterface::IsEntityConcealed(*netPed));

#if !__NO_OUTPUT
	bool bInitialInScope = bInScope;
#endif

	if(bInScope && m_nRange != 0)
	{
		Vec3V vPosition(V_ZERO);
		if(m_bUseEntity)
		{
			netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityID);
			if(pNetObj)
			{
				if(!pNetObj->IsClone())
				{
					bInScope &= pNetObj->HasBeenCloned(player) || pNetObj->IsPendingCloning(player);
				}

				if(pNetObj->GetEntity())
				{
					vPosition = pNetObj->GetEntity()->GetTransform().GetPosition();
				}
			}
			else
				bInScope = false;
		}
		else
			vPosition = VECTOR3_TO_VEC3V(m_Position);

		// if we're still in scope
		if(bInScope)
		{
			gnetAssertf(VEC3V_TO_VECTOR3(vPosition) != VEC3_ZERO, "CPlaySoundEvent::IsInScope Position is 0!");
			// don't send to players far away from the sound position
			Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(player);
			float distanceSq = DistSquared(VECTOR3_TO_VEC3V(playerFocusPosition), vPosition).Getf();
			bInScope &= (distanceSq <= (m_nRange * m_nRange));
		}
	}

#if !__NO_OUTPUT
	networkAudioDebugf3("CPlaySoundEvent::IsInScope player[%s] bInitialInScope[%d] bInScope[%d] m_nRange[%d]",player.GetLogName(),bInitialInScope,bInScope,m_nRange);
#endif

	return bInScope;
}

template <class Serialiser> void CPlaySoundEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_BOOL(serialiser, m_bUseEntity, "Using Entity");
	if(!m_bUseEntity)
	{
		SERIALISE_POSITION(serialiser, m_Position, "Audio Position");
	}
	else
	{
		SERIALISE_OBJECTID(serialiser, m_EntityID, "Audio Entity");
	}
	
	bool bSetNameValid = (m_setNameHash != 0);
	SERIALISE_BOOL(serialiser, bSetNameValid, "Set Name Hash Valid");
	if (bSetNameValid)
	{
		SERIALISE_UNSIGNED(serialiser, m_setNameHash, SIZEOF_AUDIO_HASH, "Audio Meta Hash");
	}
	else
	{
		m_setNameHash = 0;
	}

	SERIALISE_UNSIGNED(serialiser, m_soundNameHash, SIZEOF_AUDIO_HASH, "Audio Meta Hash");

	SERIALISE_UNSIGNED(serialiser, m_SoundID, SIZEOF_SOUND_ID, "Audio Sound ID");

	bool bSetScriptValid = m_ScriptId.IsValid();
	SERIALISE_BOOL(serialiser, bSetScriptValid, "Set Script Valid");
	if (bSetScriptValid)
	{
		m_ScriptId.Serialise(serialiser);
	}
	else
	{
		m_ScriptId.Invalidate();
	}
}

void CPlaySoundEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPlaySoundEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPlaySoundEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	networkAudioDebugf3("CPlaySoundEvent::Decide m_setNameHash[%u] m_soundNameHash[%u] soundID[%d]",m_setNameHash,m_soundNameHash,m_SoundID);

	// this is an ID that we can use to find this sound should we need to stop it
	u32 nNetworkID = (fromPlayer.GetPhysicalPlayerIndex() << 16) | m_SoundID;

	// entity or position?
	if(m_bUseEntity)
	{
		if(m_EntityID != NETWORK_INVALID_OBJECT_ID)
		{
			netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityID);
			if(pNetObj)
			{
				CEntity* pEntity = pNetObj->GetEntity();
				if(pEntity)
				{
					if(m_ScriptId.IsValid())
					{
						g_ScriptAudioEntity.PlaySoundFromNetwork(nNetworkID, m_ScriptId, pEntity, m_setNameHash, m_soundNameHash);
					}
					else
					{
						audDisplayf("CPlaySoundEvent::Decide, triggering sound %u, soundset %u from audio entity", m_setNameHash, m_soundNameHash);

						naAudioEntity* audioEntity = pEntity->GetAudioEntity();

						if(audioEntity)
						{
							audSoundInitParams initParams;
							initParams.TrackEntityPosition = true;
							initParams.EnvironmentGroup = (naEnvironmentGroup*)pEntity->GetAudioEnvironmentGroup();
							initParams.Position = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

							audSoundSet soundSet;
							
							if(m_setNameHash)
							{
								if(soundSet.Init(m_setNameHash))
								{
									audMetadataRef soundRef = soundSet.Find(m_soundNameHash);

									if(soundRef.IsValid())
									{
										if(audioEntity->CreateAndPlaySound(soundRef, &initParams))
										{
											REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_setNameHash, m_soundNameHash, &initParams, pEntity));
										}
									}
								}
							}
							else
							{
								if(audioEntity->CreateAndPlaySound(m_soundNameHash, &initParams))
								{
									REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_soundNameHash, &initParams, pEntity));
								}
							}
						}
						else
						{
							audWarningf("CPlaySoundEvent::Decide, failed to find audioEntity - sound %u, soundset %u", m_setNameHash, m_soundNameHash);
						}
					}
				}
				else
				{
					audWarningf("CPlaySoundEvent::Decide, failed to find CEntity - sound %u, soundset %u", m_setNameHash, m_soundNameHash);
				}
			}
			else
			{
				audWarningf("CPlaySoundEvent::Decide, failed to find netObject - sound %u, soundset %u", m_setNameHash, m_soundNameHash);
			}
		}
		else
		{
			audWarningf("CPlaySoundEvent::Decide, invalid m_EntityID - sound %u, soundset %u", m_setNameHash, m_soundNameHash);
		}
	}
	else
		g_ScriptAudioEntity.PlaySoundFromNetwork(nNetworkID, m_ScriptId, &m_Position, m_setNameHash, m_soundNameHash);

	return true;
}

bool CPlaySoundEvent::operator==( const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_PLAY_SOUND_EVENT)
	{
		const CPlaySoundEvent *playSoundEvent = static_cast<const CPlaySoundEvent *>(pEvent);

		if (playSoundEvent->m_soundNameHash == m_soundNameHash &&
			playSoundEvent->m_SoundID == m_SoundID)
		{
			if (playSoundEvent->m_bUseEntity)
			{
				if (m_bUseEntity &&	playSoundEvent->m_EntityID == m_EntityID)
					return true;
			}
			else
			{
				if (!m_bUseEntity && playSoundEvent->m_Position.IsClose(m_Position, 0.1f))
				{
					return true;
				}
			}
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CPlaySoundEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	if(m_bUseEntity)
	{
		netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityID);
		if(pNetObj)
			log.WriteDataValue("Audio Entity", "%s", pNetObj->GetLogName());
	}
	else
		log.WriteDataValue("Audio Position", "%f, %f, %f", m_Position.x, m_Position.y, m_Position.z);

	if (m_ScriptId.IsValid())
	{
		log.WriteDataValue("Script id", "%s", m_ScriptId.GetLogName());
	}

	log.WriteDataValue("Set Name Hash", "%u", m_setNameHash);
	log.WriteDataValue("Sound Name Hash", "%u", m_soundNameHash);
	log.WriteDataValue("Audio Sound ID", "%d", m_SoundID);
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// STOP SOUND EVENT
// ================================================================================================================
CStopSoundEvent::CStopSoundEvent() 
: netGameEvent(NETWORK_STOP_SOUND_EVENT, false, false)
, m_SoundID(0)
{

}

CStopSoundEvent::CStopSoundEvent(u8 soundID)
: netGameEvent(NETWORK_STOP_SOUND_EVENT, false, false)
, m_SoundID(soundID)
{
	
}

void CStopSoundEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CStopSoundEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CStopSoundEvent::Trigger(u8 soundID)
{
	networkAudioDisplayf("CStopSoundEvent::Trigger soundID[%d]",soundID);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CStopSoundEvent *pEvent = rage_new CStopSoundEvent(soundID);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CStopSoundEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true;
	bInScope &= !player.IsLocal();
	bInScope &= !player.IsBot();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();
	return bInScope;
}

template <class Serialiser> void CStopSoundEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_SoundID, SIZEOF_SOUND_ID, "Audio Sound ID");
}

void CStopSoundEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CStopSoundEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CStopSoundEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	// build network ID to stop sound
	u32 nNetworkID = (fromPlayer.GetPhysicalPlayerIndex() << 16) | m_SoundID;

	networkAudioDisplayf("CStopSoundEvent::Decide nNetworkID[%u] m_SoundID[%u]",nNetworkID,m_SoundID);

	g_ScriptAudioEntity.StopSoundFromNetwork(nNetworkID);

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CStopSoundEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Audio Sound ID", "%d", m_SoundID);
}

#endif //ENABLE_NETWORK_LOGGING


// ================================================================================================================
// PLAY AIR DEFENSE FIRE EVENT
// ================================================================================================================

CPlayAirDefenseFireEvent::CPlayAirDefenseFireEvent() 
	: netGameEvent(NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT, false, false)
	, m_SoundHash(0)
	, m_positionOfAirDefence(VECTOR3_ZERO)
	, m_SystemTimeAdded(sysTimer::GetSystemMsTime())
	, m_sphereRadius(0)
{

}

CPlayAirDefenseFireEvent::CPlayAirDefenseFireEvent(Vector3 position, u32 soundHash, float sphereRadius) 
	: netGameEvent(NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT, false, false)
	, m_positionOfAirDefence(position)
	, m_SoundHash(soundHash)
	, m_SystemTimeAdded(sysTimer::GetSystemMsTime())
	, m_sphereRadius(sphereRadius)
{

}

void CPlayAirDefenseFireEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CPlayAirDefenseFireEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPlayAirDefenseFireEvent::Trigger(Vector3 position, u32 soundHash, float sphereRadius)
{
	networkAudioDebugf3("CPlayAirDefenseFireEvent::Trigger soundHash[%u] sphereRadius[%f] position[%f %f %f]",
		soundHash, sphereRadius, position.x, position.y, position.z);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();

	static u32 LAST_AIR_DEFENSE_FIRE = 0;
	const u32 MAX_DELTA_BETWEEN_AIRDEFENSE_FIRE_EVENTS = 1000;
	bool shouldAddEvent = true;

	u32 currTime = sysTimer::GetSystemMsTime();

	if (currTime - LAST_AIR_DEFENSE_FIRE < MAX_DELTA_BETWEEN_AIRDEFENSE_FIRE_EVENTS)
	{
		shouldAddEvent = false;
	}

	if (shouldAddEvent)
	{
		// Send it across to other players
		CPlayAirDefenseFireEvent *pEvent = rage_new CPlayAirDefenseFireEvent(position, soundHash, sphereRadius);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);

		LAST_AIR_DEFENSE_FIRE = currTime;
	}
}

bool CPlayAirDefenseFireEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true;
	bInScope &= !player.IsLocal();
	bInScope &= !player.IsBot();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();

	if(bInScope && m_sphereRadius != 0)
	{		
		Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(player);
		gnetAssertf(m_positionOfAirDefence.IsNonZero(), "CPlayAirDefenseFireEvent::IsInScope Air Defense Gun Position is 0! Not possible!");
			
		float distanceSq = DistSquared(VECTOR3_TO_VEC3V(playerFocusPosition), VECTOR3_TO_VEC3V(m_positionOfAirDefence)).Getf();
		// don't send to players far away from the gun position (double the AA guns' range)
		float soundRange = m_sphereRadius * 2;

		bInScope &= (distanceSq <= (soundRange * soundRange));		
	}

#if !__NO_OUTPUT
	networkAudioDebugf3("CPlayAirDefenseFireEvent::IsInScope player[%s] bInScope[%d] m_sphereRadius[%f] m_positionOfAirDefence[%f %f %f]", 
		player.GetLogName(), bInScope, m_sphereRadius, m_positionOfAirDefence.x, m_positionOfAirDefence.y, m_positionOfAirDefence.z);
#endif

	return bInScope;
}

template <class Serialiser> void CPlayAirDefenseFireEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	const unsigned int SIZEOF_SOUND_HASH = 32;    
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_SoundHash, SIZEOF_SOUND_HASH, "Audio Sound Hash");
	SERIALISE_POSITION(serialiser, m_positionOfAirDefence, "Position of Air Defense Gun");
}

void CPlayAirDefenseFireEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPlayAirDefenseFireEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPlayAirDefenseFireEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	audSoundInitParams initParams;
	initParams.Position = m_positionOfAirDefence;
	g_WeaponAudioEntity.CreateDeferredSound(m_SoundHash, NULL, &initParams);

	networkAudioDebugf3("CPlayAirDefenseFireEvent::Decide m_SoundHash[%u]  m_positionOfAirDefence[%f %f %f]", 
		m_SoundHash, m_positionOfAirDefence.x, m_positionOfAirDefence.y, m_positionOfAirDefence.z);

	return true;
}

bool CPlayAirDefenseFireEvent::HasTimedOut() const
{
	const unsigned AIRDEFENSEFIRE_EVENT_TIMEOUT = 1000;

	u32 currentTime = sysTimer::GetSystemMsTime();

	if(currentTime < m_SystemTimeAdded)
	{
		m_SystemTimeAdded = currentTime;
	}

	u32 timeElapsed = currentTime - m_SystemTimeAdded;

	return timeElapsed > AIRDEFENSEFIRE_EVENT_TIMEOUT;
}

#if ENABLE_NETWORK_LOGGING

void CPlayAirDefenseFireEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Audio Sound Hash", "%u", m_SoundHash);
	log.WriteDataValue("Air Defense Position", "%f, %f, %f", m_positionOfAirDefence.x, m_positionOfAirDefence.y, m_positionOfAirDefence.z);
}

#endif //ENABLE_NETWORK_LOGGING


bool CPlayAirDefenseFireEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_PLAY_SOUND_EVENT)
	{
		const CPlayAirDefenseFireEvent *playAirDefenseFireEvent = static_cast<const CPlayAirDefenseFireEvent *>(pEvent);
		if (playAirDefenseFireEvent->m_SoundHash == m_SoundHash &&
			playAirDefenseFireEvent->m_SystemTimeAdded == m_SystemTimeAdded)
		{
			return true;	
		}
	}
	return false;
}


// ================================================================================================================
// BANK REQUEST EVENT
// ================================================================================================================
CAudioBankRequestEvent::CAudioBankRequestEvent() 
: netGameEvent(NETWORK_BANK_REQUEST_EVENT, false, false)
, m_BankHash(audScriptBankSlot::INVALID_BANK_ID)
, m_bIsRequest(true)
, m_PlayerBits(0)
{
	m_ScriptID.Invalidate();
}

CAudioBankRequestEvent::CAudioBankRequestEvent(u32 bankHash, CGameScriptId& scriptId, bool bIsRequest, unsigned playerBits)
: netGameEvent(NETWORK_BANK_REQUEST_EVENT, false, false)
, m_BankHash(bankHash)
, m_ScriptID(scriptId)
, m_bIsRequest(bIsRequest)
, m_PlayerBits(playerBits)
{
		
}

void CAudioBankRequestEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CAudioBankRequestEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CAudioBankRequestEvent::Trigger(u32 bankHash, CGameScriptId& scriptId, bool bIsRequest, unsigned playerBits)
{
	networkAudioDebugf3("CAudioBankRequestEvent::Trigger bankHash[%u] bIsRequest[%d]",bankHash,bIsRequest);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CAudioBankRequestEvent *pEvent = rage_new CAudioBankRequestEvent(bankHash, scriptId, bIsRequest, playerBits);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CAudioBankRequestEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true;
	bInScope &= !player.IsLocal();
	bInScope &= !player.IsBot();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();
	bInScope &= (m_PlayerBits & (1 << player.GetPhysicalPlayerIndex())) != 0;
	return bInScope;
}

template <class Serialiser> void CAudioBankRequestEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_BankHash, SIZEOF_BANK_ID, "Audio Bank ID");
	SERIALISE_BOOL(serialiser, m_bIsRequest, "Is Load Request");
	m_ScriptID.Serialise(serialiser);
}

void CAudioBankRequestEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CAudioBankRequestEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CAudioBankRequestEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	networkAudioDebugf3("CAudioBankRequestEvent::Decide m_BankHash[%u] m_bIsRequest[%d]",m_BankHash,m_bIsRequest);

	if(m_bIsRequest)
		g_ScriptAudioEntity.RequestScriptBankFromNetwork(m_BankHash, m_ScriptID);
	else
		g_ScriptAudioEntity.ReleaseScriptBankFromNetwork(m_BankHash, m_ScriptID);

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CAudioBankRequestEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	log.WriteDataValue("Script", "%s", m_ScriptID.GetLogName());
	log.WriteDataValue("Audio Bank ID", "%d", m_BankHash);
	log.WriteDataValue("Is Load Request", "%s", m_bIsRequest ? "True" : "False");
}

#endif //ENABLE_NETWORK_LOGGINGsc

// ================================================================================================================
// CAudioBarkingEvent - 
// ================================================================================================================
CAudioBarkingEvent::CAudioBarkingEvent()
	: netGameEvent(NETWORK_AUDIO_BARK_EVENT, false, false)
	, m_PedId(NETWORK_INVALID_OBJECT_ID)
	, m_ContextHash(0)
	, m_BarkPosition(VEC3_ZERO)
{
}

CAudioBarkingEvent::CAudioBarkingEvent(ObjectId pedId, u32 contextHash, Vector3 barkPosition)
	: netGameEvent(NETWORK_AUDIO_BARK_EVENT, false, false)
	, m_PedId(pedId)
	, m_ContextHash(contextHash)
	, m_BarkPosition(barkPosition)
{
}

void CAudioBarkingEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CAudioBarkingEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CAudioBarkingEvent::Trigger(ObjectId pedId, u32 contextHash, Vector3 barkPosition)
{
	static const int MAX_BARKING_EVENTS = 5;

	networkAudioDebugf3("CAudioBarkingEvent::Trigger pedId[%d] contextHash[%u] barkPosition[x:%.2f,y:%.2f,z:%.2f]", pedId, contextHash, barkPosition.GetX(), barkPosition.GetY(), barkPosition.GetZ());

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

	int backEventCounter = 0;
	while (node)
	{
		netGameEvent *networkEvent = node->Data;

		if (networkEvent && networkEvent->GetEventType() == NETWORK_AUDIO_BARK_EVENT && !networkEvent->IsFlaggedForRemoval())
		{
			backEventCounter++;
			if (backEventCounter > MAX_BARKING_EVENTS)
			{
				gnetDebug3("Dropping CAudioBarkingEvent because maximum has been reached(%d)", MAX_BARKING_EVENTS);
				return;
			}
		}
		node = node->GetNext();
	}

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CAudioBarkingEvent *pEvent = rage_new CAudioBarkingEvent(pedId, contextHash, barkPosition);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CAudioBarkingEvent::IsInScope(const netPlayer& player) const
{
	static const int BARK_MAX_DISTANCE = 100;

	if (player.IsLocal())
		return false;
	
	if (SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession())
		return false;

	gnetAssertf(!m_BarkPosition.IsClose(VEC3_ZERO, 0.1f), "CAudioBarkingEvent expects a valid bark position");

	Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(player);
	float distanceSq = DistSquared(VECTOR3_TO_VEC3V(playerFocusPosition), VECTOR3_TO_VEC3V(m_BarkPosition)).Getf();
	
	return (distanceSq <= (BARK_MAX_DISTANCE * BARK_MAX_DISTANCE));
}

template <class Serialiser> void CAudioBarkingEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_PedId, "Ped Id");
	SERIALISE_HASH(serialiser, m_ContextHash, "Context Hash");
}

void CAudioBarkingEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CAudioBarkingEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CAudioBarkingEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	networkAudioDebugf3("CAudioBarkingEvent::Decide m_PedId[%d] m_ContextHash[%u]", m_PedId, m_ContextHash);

	netObject* networkObject = NetworkInterface::GetNetworkObject(m_PedId);
	if (networkObject && (networkObject->GetObjectType() == NET_OBJ_TYPE_PED || networkObject->GetObjectType() == NET_OBJ_TYPE_PLAYER))
	{
		CNetObjPed* netPed = SafeCast(CNetObjPed, networkObject);
		
		CPed* ped = netPed->GetPed();
		if (ped && ped->GetSpeechAudioEntity())
		{
			ped->GetSpeechAudioEntity()->PlayAnimalVocalization(m_ContextHash);
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CAudioBarkingEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if (!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Ped Id", "%d", m_PedId);
	log.WriteDataValue("Context Hash", "%u", m_ContextHash);
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// REQUEST DOOR EVENT
// ===========================================================================================================

CRequestDoorEvent::CRequestDoorEvent() 
: netGameEvent( REQUEST_DOOR_EVENT, true )
, m_doorEnumHash(0)
, m_doorState(0)
, m_bGranted(false)
, m_bGrantedByAllPlayers(false)
, m_bLocallyTriggered(false)
{
}

CRequestDoorEvent::CRequestDoorEvent(int doorEnumHash, int doorState, const CGameScriptId& scriptId) 
: netGameEvent( REQUEST_DOOR_EVENT, true  )
, m_doorEnumHash(doorEnumHash)
, m_doorState(doorState)
, m_scriptId(scriptId)
, m_bGranted(false)
, m_bGrantedByAllPlayers(true)
, m_bLocallyTriggered(true)
{
}

CRequestDoorEvent::~CRequestDoorEvent()
{
	if (m_bLocallyTriggered)
	{
		CDoorSystemData* pDoor = CDoorSystem::GetDoorData(m_doorEnumHash);

		if (pDoor && !pDoor->GetNetworkObject())
		{
			pDoor->SetPendingNetworking(false);

			if (m_bGrantedByAllPlayers)
			{
				if (CNetObjDoor::GetPool()->GetNoOfFreeSpaces() == 0)
				{
					scriptAssertf(0, "%s: CRequestDoorEvent - ran out of networked doors (max: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetObjDoor::GetPool()->GetSize());
				}
				else
				{
					NetworkInterface::RegisterScriptDoor(*pDoor, true);
					pDoor->SetPendingState((DoorScriptState)m_doorState);
				}
			}
		}
	}
}

void CRequestDoorEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRequestDoorEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestDoorEvent::Trigger(CDoorSystemData& door, int doorState)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	if (AssertVerify(NetworkInterface::IsGameInProgress()) && AssertVerify(door.GetScriptInfo()))
	{
		door.SetPendingNetworking(true);

		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CRequestDoorEvent *pEvent = rage_new CRequestDoorEvent(door.GetEnumHash(), doorState, static_cast<CGameScriptId&>(door.GetScriptInfo()->GetScriptId()));
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CRequestDoorEvent::IsInScope( const netPlayer& player ) const
{
	if (!player.IsLocal())
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_scriptId);

		if (pHandler && pHandler->GetNetworkComponent() && pHandler->GetNetworkComponent()->IsPlayerAParticipant(player))
		{
			return true;
		}
	}

	return false;
}

template <class Serialiser> void CRequestDoorEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_doorEnumHash, 32, "Door enum hash");
}

template <class Serialiser> void CRequestDoorEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_BOOL(serialiser, m_bGranted, "Granted");
}

void CRequestDoorEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestDoorEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestDoorEvent::Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	// we can't process door events until we are into the match (the single player doors are flushed when the match starts)
	if (!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	CDoorSystemData* pDoorData = CDoorSystem::GetDoorData(m_doorEnumHash);

	if (pDoorData)
	{
		if (!pDoorData->GetNetworkObject())
		{
			gnetAssert(pDoorData->GetPersistsWithoutNetworkObject());

			if (pDoorData->GetPendingNetworking())
			{
				// if two players are trying to network a door at the same time, only allow the player with the highest peer id
				if (fromPlayer.GetRlPeerId() > toPlayer.GetRlPeerId())
				{
					m_bGranted = true;
				}
			}
			else
			{
				m_bGranted = true;
			}
		}
	}
	else
	{
		m_bGranted = true;
	}

	return true;
}

void CRequestDoorEvent::PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CRequestDoorEvent::HandleReply ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEventReply<CSyncDataReader>(messageBuffer);

	if (!m_bGranted)
	{
		m_bGrantedByAllPlayers = false;
	}
}

#if ENABLE_NETWORK_LOGGING

void CRequestDoorEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Door enum hash", "%u", m_doorEnumHash);
	logSplitter.WriteDataValue("Door state", "%s", CDoorSystemData::GetDoorStateName((DoorScriptState)m_doorState));
}

void CRequestDoorEvent::WriteDecisionToLogFile() const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Granted", " %s", m_bGranted ? "true" : "false");
}

void CRequestDoorEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Granted", " %s", m_bGranted ? "true" : "false");
}

#endif // ENABLE_NETWORK_LOGGING

bool CRequestDoorEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == REQUEST_DOOR_EVENT)
	{
		const CRequestDoorEvent *pRequestDoorEvent = SafeCast(const CRequestDoorEvent, pEvent);

		if (pRequestDoorEvent->m_doorEnumHash == m_doorEnumHash)
		{
			// copy any new door state to the old event so that it will be applied when the event is destroyed
			m_doorState = pRequestDoorEvent->m_doorState;
			pRequestDoorEvent->m_bLocallyTriggered = false;
			return true;
		}
	}

	return false;
}

// ===========================================================================================================
// NETWORK TRAIN REQUEST EVENT
// ===========================================================================================================

CNetworkTrainRequestEvent::CNetworkTrainRequestEvent() 
	: netGameEvent( NETWORK_TRAIN_REQUEST_EVENT, true, true )
	, m_traintrack(0)
	, m_trainapproved(false)
{
}

CNetworkTrainRequestEvent::CNetworkTrainRequestEvent(int traintrack) 
	: netGameEvent( NETWORK_TRAIN_REQUEST_EVENT, true, true  )
	, m_traintrack(traintrack)
	, m_trainapproved(false)
{
}

CNetworkTrainRequestEvent::~CNetworkTrainRequestEvent()
{

}

void CNetworkTrainRequestEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkTrainRequestEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkTrainRequestEvent::Trigger(int traintrack)
{
	trainDebugf2("CNetworkTrainRequestEvent::Trigger traintrack[%d] IsHost[%d]",traintrack,NetworkInterface::IsHost());

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	if (AssertVerify(NetworkInterface::IsGameInProgress()))
	{
		if (NetworkInterface::IsHost())
		{
			bool bApproval = CTrain::DetermineHostApproval(traintrack);
			CTrain::ReceivedHostApprovalData(traintrack,bApproval);
		}
		else
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CNetworkTrainRequestEvent *pEvent = rage_new CNetworkTrainRequestEvent(traintrack);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
}

void CNetworkTrainRequestEvent::PrepareReply ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEventReply<CSyncDataWriter>(messageBuffer);
}

void CNetworkTrainRequestEvent::HandleReply ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEventReply<CSyncDataReader>(messageBuffer);

	CTrain::ReceivedHostApprovalData(m_traintrack,m_trainapproved);
}

bool CNetworkTrainRequestEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal() && player.IsHost();
}

template <class Serialiser> void CNetworkTrainRequestEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_traintrack, 4, "m_traintrack");
}

template <class Serialiser> void CNetworkTrainRequestEvent::SerialiseEventReply(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_traintrack, 4, "m_traintrack");
	SERIALISE_BOOL(serialiser, m_trainapproved,"m_trainapproved");

	trainDebugf2("CNetworkTrainRequestEvent::SerialiseEventReply traintrack[%d] trainapproved[%d]",m_traintrack,m_trainapproved);
}

void CNetworkTrainRequestEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkTrainRequestEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkTrainRequestEvent::Decide (const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	trainDebugf2("CNetworkTrainRequestEvent::Decide traintrack[%d]",m_traintrack);

	m_trainapproved = CTrain::DetermineHostApproval(m_traintrack);
		
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkTrainRequestEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("m_traintrack", "%u", m_traintrack);
}

void CNetworkTrainRequestEvent::WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("m_traintrack", "%u", m_traintrack);
	logSplitter.WriteDataValue("m_trainapproved", "%u", m_trainapproved);
}

#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// NETWORK TRAIN REPORT EVENT
// ===========================================================================================================

CNetworkTrainReportEvent::CNetworkTrainReportEvent() 
	: netGameEvent( NETWORK_TRAIN_REPORT_EVENT, false, true )
	, m_traintrack(0)
	, m_trainactive(false)
	, m_playerindex(255)
{
}

CNetworkTrainReportEvent::CNetworkTrainReportEvent(int traintrack, bool trainactive, u8 playerindex) 
	: netGameEvent( NETWORK_TRAIN_REPORT_EVENT, false, true  )
	, m_traintrack(traintrack)
	, m_trainactive(trainactive)
	, m_playerindex(playerindex)
{
}

CNetworkTrainReportEvent::~CNetworkTrainReportEvent()
{

}

void CNetworkTrainReportEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkTrainReportEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkTrainReportEvent::Trigger(int traintrack, bool trainactive, u8 playerindex)
{
	trainDebugf2("CNetworkTrainReportEvent::Trigger trackindex[%d] trainactive[%d] playerindex[%d]",traintrack,trainactive,playerindex);

	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	if (AssertVerify(NetworkInterface::IsGameInProgress()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CNetworkTrainReportEvent *pEvent = rage_new CNetworkTrainReportEvent(traintrack,trainactive,playerindex);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CNetworkTrainReportEvent::IsInScope( const netPlayer& player ) const
{
	if (player.IsLocal())
		return false;

	if (m_playerindex != 255 && (player.GetPhysicalPlayerIndex() != m_playerindex))
		return false;

	//This is true if we are sending to all other players, or to one specified player
	return true;
}

template <class Serialiser> void CNetworkTrainReportEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_traintrack, 4, "m_traintrack");
	SERIALISE_BOOL(serialiser, m_trainactive, "m_trainactive");
}

void CNetworkTrainReportEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkTrainReportEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkTrainReportEvent::Decide (const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	trainDebugf2("CNetworkTrainReportEvent::Decide");
	CTrain::SetTrackActive(m_traintrack,m_trainactive);
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkTrainReportEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("m_traintrack", "%u", m_traintrack);
	logSplitter.WriteDataValue("m_trainactive", "%u", m_trainactive);
}

#endif // ENABLE_NETWORK_LOGGING

// ================================================================================================================
// CModifyVehicleLockWorldStateDataEvent 
// - works in conjunction with CNetworkVehiclePlayerLockingWorldState 
// - sent when an existing vehicle instance lock is changes state
// ================================================================================================================

CModifyVehicleLockWorldStateDataEvent::CModifyVehicleLockWorldStateDataEvent()
: 
	netGameEvent( MODIFY_VEHICLE_LOCK_WORD_STATE_DATA, false ),
	m_playerIndex(INVALID_PLAYER_INDEX),
	m_vehicleIndex(-1),
	m_modelId(fwModelId::MI_INVALID),
	m_lock(false)
{}

CModifyVehicleLockWorldStateDataEvent::CModifyVehicleLockWorldStateDataEvent(u32 const playerIndex, s32 const vehicleIndex, u32 const modelId, bool const lock)
: 
	netGameEvent( MODIFY_VEHICLE_LOCK_WORD_STATE_DATA, false ),
	m_playerIndex(playerIndex),
	m_vehicleIndex(vehicleIndex),
	m_modelId(modelId),
	m_lock(lock)
{}

void CModifyVehicleLockWorldStateDataEvent::EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CModifyVehicleLockWorldStateDataEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, bitBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CModifyVehicleLockWorldStateDataEvent::Trigger(u32 const playerIndex, s32 const vehicleIndex, u32 const modelId, bool const lock)
{
	bool exists = false;

	if(vehicleIndex != -1)
	{
		gnetAssert(fwModelId::MI_INVALID == modelId);

		int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(playerIndex, vehicleIndex);
		if(gnetVerifyf(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX, "Trying to modify an entry that doesn't exist?!"))
		{
			exists = true;
		}
	}
	else if(modelId != fwModelId::MI_INVALID)
	{
		gnetAssert(-1 == vehicleIndex);

		int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(playerIndex, modelId);
		if(gnetVerifyf(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX, "Trying to modify an entry that doesn't exist?!"))
		{
			exists = true;
		}		
	}
	else
	{
		gnetAssertf(false, "CModifyVehicleLockWorldStateDataEvent::Trigger - entry does not exist?");
		return;
	}

	if(exists)
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		gnetAssert(NetworkInterface::IsGameInProgress());
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CModifyVehicleLockWorldStateDataEvent *pEvent = rage_new CModifyVehicleLockWorldStateDataEvent(playerIndex, vehicleIndex, modelId, lock);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CModifyVehicleLockWorldStateDataEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = player.IsRemote();

	return bInScope;
}

void CModifyVehicleLockWorldStateDataEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer & UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CModifyVehicleLockWorldStateDataEvent::Handle( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

template <class Serialiser> void CModifyVehicleLockWorldStateDataEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

	const u32 SIZEOF_PLAYER_INDEX	= 8;
	const u32 SIZEOF_VEHICLE_INDEX	= 32;
	const u32 SIZEOF_MODEL_ID		= 32;

    SERIALISE_UNSIGNED(serialiser, m_playerIndex,  SIZEOF_PLAYER_INDEX,	"Player Index");	
	SERIALISE_UNSIGNED(serialiser, m_vehicleIndex, SIZEOF_VEHICLE_INDEX,	"Vehicle Index");	
	SERIALISE_UNSIGNED(serialiser, m_modelId,		 SIZEOF_MODEL_ID,		"Model Id");	
	SERIALISE_BOOL(serialiser, m_lock, "Lock");

	gnetAssert((m_vehicleIndex == -1 && m_modelId != fwModelId::MI_INVALID) || (m_vehicleIndex != -1 && m_modelId == fwModelId::MI_INVALID));
}

bool CModifyVehicleLockWorldStateDataEvent::Decide(const netPlayer & UNUSED_PARAM(fromPlayer), const netPlayer & UNUSED_PARAM(toPlayer))
{
	if(m_vehicleIndex != -1)
	{
		gnetAssert(fwModelId::MI_INVALID == m_modelId);

		int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(m_playerIndex, m_vehicleIndex);
		if(gnetVerifyf(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX, "Trying to modify an entry that doesn't exist?!"))
		{
			CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(m_playerIndex, m_vehicleIndex, m_lock);
		}
	}
	else if(m_modelId != fwModelId::MI_INVALID)
	{
		gnetAssert(-1 == m_vehicleIndex);

		int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(m_playerIndex, m_modelId);
		if(gnetVerifyf(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX, "Trying to modify an entry that doesn't exist?!"))
		{
			CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(m_playerIndex, m_modelId, m_lock);
		}		
	}
	else
	{
		gnetAssertf(false, "CModifyVehicleLockWorldStateDataEvent::Decide - entry does not exist?");
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CModifyVehicleLockWorldStateDataEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Player Index",	"%d", m_playerIndex);
	logSplitter.WriteDataValue("Vehicle Index", "%d", m_vehicleIndex);
	logSplitter.WriteDataValue("Model Id",		"%d", m_modelId);
	logSplitter.WriteDataValue("Lock",			"%d", m_lock);
}

#endif // ENABLE_NETWORK_LOGGING

bool CModifyVehicleLockWorldStateDataEvent::operator==(const netGameEvent* pEvent) const
{
    if (pEvent->GetEventType() == MODIFY_VEHICLE_LOCK_WORD_STATE_DATA)
    {
		const CModifyVehicleLockWorldStateDataEvent* modifyEvent = SafeCast(const CModifyVehicleLockWorldStateDataEvent, pEvent);

        return ((modifyEvent->m_playerIndex == m_playerIndex) && 
					(modifyEvent->m_vehicleIndex == m_vehicleIndex) && 
						(modifyEvent->m_modelId == m_modelId) &&
							(modifyEvent->m_lock == m_lock));
    }

    return false;	
}

// ================================================================================================================
// CModifyPtFXWorldStateDataScriptedEvolveEvent 
// - works in conjunction with CNetworkPtFXWorldStateData 
// - sent when an FX evolves
// ================================================================================================================

CModifyPtFXWorldStateDataScriptedEvolveEvent::CModifyPtFXWorldStateDataScriptedEvolveEvent()
	: netGameEvent(MODIFY_PTFX_WORD_STATE_DATA_SCRIPTED_EVOLVE_EVENT, false)
	, m_EvoHash(0)
	, m_EvoVal(0.0f)
	, m_UniqueID(0)
{}

CModifyPtFXWorldStateDataScriptedEvolveEvent::CModifyPtFXWorldStateDataScriptedEvolveEvent(u32 evoHash, float evoVal, int uniqueID)
	: netGameEvent(MODIFY_PTFX_WORD_STATE_DATA_SCRIPTED_EVOLVE_EVENT, false)
	, m_EvoHash(evoHash)
	, m_EvoVal(evoVal)
	, m_UniqueID(uniqueID)
{}

void CModifyPtFXWorldStateDataScriptedEvolveEvent::EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CModifyPtFXWorldStateDataScriptedEvolveEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, bitBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CModifyPtFXWorldStateDataScriptedEvolveEvent::Trigger(u32 evoHash, float evoVal, int uniqueId)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CModifyPtFXWorldStateDataScriptedEvolveEvent *pEvent = rage_new CModifyPtFXWorldStateDataScriptedEvolveEvent(evoHash, evoVal, uniqueId);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CModifyPtFXWorldStateDataScriptedEvolveEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = player.IsRemote();
	return bInScope;
}

void CModifyPtFXWorldStateDataScriptedEvolveEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer & UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CModifyPtFXWorldStateDataScriptedEvolveEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

template <class Serialiser> void CModifyPtFXWorldStateDataScriptedEvolveEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	const u32 SIZEOF_EVO_HASH = 32;
	
	const u32 SIZEOF_EVO_VAL = 32;
	const float MAX_EVO_VAL = 1000.0f;

	const u32 SIZEOF_UNIQUE_ID = 32;

	SERIALISE_UNSIGNED(serialiser, m_EvoHash, SIZEOF_EVO_HASH, "Evo Hash");
	SERIALISE_PACKED_FLOAT(serialiser, m_EvoVal, MAX_EVO_VAL, SIZEOF_EVO_VAL, "Evo Val");
	SERIALISE_INTEGER(serialiser, m_UniqueID, SIZEOF_UNIQUE_ID, "World State Unique ID");
}

bool CModifyPtFXWorldStateDataScriptedEvolveEvent::Decide(const netPlayer & UNUSED_PARAM(fromPlayer), const netPlayer & UNUSED_PARAM(toPlayer))
{
	CNetworkPtFXWorldStateData::ApplyScriptedEvolvePtFXFromNetwork(m_UniqueID, m_EvoHash, m_EvoVal);
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CModifyPtFXWorldStateDataScriptedEvolveEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

	if (!logSplitter.IsEnabled())
	{
		return;
	}

	logSplitter.WriteDataValue("Evo Hash", "%u", m_EvoHash);
	logSplitter.WriteDataValue("Evo val", "%f", m_EvoVal);
	logSplitter.WriteDataValue("Unique ID", "%d", m_UniqueID);
}

#endif // ENABLE_NETWORK_LOGGING

bool CModifyPtFXWorldStateDataScriptedEvolveEvent::operator==(const netGameEvent* UNUSED_PARAM(pEvent)) const
{
    // Always send the latest event. 
    // Otherwise we can have the following situation
    //          1) Add event with val 0.0f
    //          2) Add event with val 1.0f
    //          3) Add event with val 0.0f --> if in very close frames, will be deleted because 1) already exists in the buffer
    //          This will result with latest event sent being 1.0f instead of the desired 0.0f
    return false;
}

//-----------------------------------------------------------------------
// USE THIS EVENT VERY SPARINGLY. This is very expensive on the network so it should only be used once every few minutes.
//-----------------------------------------------------------------------

u32 CNetworkIncrementStatEvent::ms_whitelistedEventStatsHashes[CNetworkIncrementStatEvent::NUM_WHITELISTED_EVENT_STATS] =
{
		ATSTRINGHASH("MPPLY_GRIEFING", 0x9C6A0C42),
		ATSTRINGHASH("MPPLY_OFFENSIVE_LANGUAGE", 0x3CDB43E2),
		ATSTRINGHASH("MPPLY_OFFENSIVE_TAGPLATE", 0xE8FB6DD5),
		ATSTRINGHASH("MPPLY_OFFENSIVE_UGC", 0xF3DE4879),
		ATSTRINGHASH("MPPLY_GAME_EXPLOITS", 0xCBFD04A4),
		ATSTRINGHASH("MPPLY_EXPLOITS", 0x9F79BA0B),
		ATSTRINGHASH("MPPLY_VC_ANNOYINGME", 0x62EB8C5A),
		ATSTRINGHASH("MPPLY_VC_HATE", 0xE7072CD),
		ATSTRINGHASH("MPPLY_TC_ANNOYINGME", 0x762F9994),
		ATSTRINGHASH("MPPLY_TC_HATE", 0xB722D6C0),
		ATSTRINGHASH("MPPLY_FRIENDLY", 0xDAFB10F9),
		ATSTRINGHASH("MPPLY_HELPFUL", 0x893E1390),
		ATSTRINGHASH("AWD_CAR_BOMBS_ENEMY_KILLS", 0x6D7F0859)
};

CNetworkIncrementStatEvent::CNetworkIncrementStatEvent()
: 
netGameEvent( NETWORK_INCREMENT_STAT_EVENT, false ),
m_uStatHash(0),
m_iStrength(0),
m_pPlayer(NULL)
{}

CNetworkIncrementStatEvent::CNetworkIncrementStatEvent(u32 uStatHash, int iStrength, netPlayer* pPlayer)
: 
netGameEvent( NETWORK_INCREMENT_STAT_EVENT, false ),
m_uStatHash(uStatHash),
m_iStrength(iStrength),
m_pPlayer(pPlayer)
{}

CNetworkIncrementStatEvent::~CNetworkIncrementStatEvent()
{}

void CNetworkIncrementStatEvent::EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkIncrementStatEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, bitBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkIncrementStatEvent::Trigger(u32 uStatHash, int iStrength, netPlayer* pPlayer)
{
	if (!IsStatWhitelisted(uStatHash))
	{
		return;
	}

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CNetworkIncrementStatEvent *pEvent = rage_new CNetworkIncrementStatEvent(uStatHash, iStrength, pPlayer);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CNetworkIncrementStatEvent::IsInScope(const netPlayer& player) const
{
	if (player.IsLocal())
		return false;

	return (&player == m_pPlayer);
}

void CNetworkIncrementStatEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer & UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkIncrementStatEvent::Handle( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

template <class Serialiser> void CNetworkIncrementStatEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	const u32 SIZEOF_STAT_HASH = 32;
	const u32 SIZEOF_INT_STR = 32;

	SERIALISE_UNSIGNED(serialiser, m_uStatHash,  SIZEOF_STAT_HASH,	"Stat Hash");		
	SERIALISE_INTEGER(serialiser, m_iStrength, SIZEOF_INT_STR, "Message Strength");
}

bool CNetworkIncrementStatEvent::IsStatWhitelisted(u32 statHash)
{
	bool found = false;
	for (int i = 0; i < NUM_WHITELISTED_EVENT_STATS && !found; i++)
	{
		if (statHash == ms_whitelistedEventStatsHashes[i])
		{
			found = true;
		}
	}

	gnetAssertf(found, "CNetworkIncrementStatEvent - Cannot be used with stat %d", statHash);
	if (!found)
	{
		gnetError("CNetworkIncrementStatEvent - Cannot be used with stat %d", statHash);
	}
	return found;
}

bool CNetworkIncrementStatEvent::Decide(const netPlayer &fromPlayer, const netPlayer & UNUSED_PARAM(toPlayer))
{
	u32 statHash = m_uStatHash;

	if (!IsStatWhitelisted(statHash))
	{
		return true;
	}

	//Deal with any exceptions for character stats - Should increment the value depending 
	//  the current selected character slot.
	if (m_uStatHash != 0 && ATSTRINGHASH("AWD_CAR_BOMBS_ENEMY_KILLS", 0x6D7F0859) == statHash)
	{
		statHash = STAT_AWD_CAR_BOMBS_ENEMY_KILLS.GetHash();
	}

	// Deal with MP report player stats.  Possibly required to push a feed warning.
	if (m_uStatHash != 0 &&  ( ATSTRINGHASH("MPPLY_EXPLOITS", 0x9F79BA0B) == statHash || ATSTRINGHASH("MPPLY_GAME_EXPLOITS", 0xCBFD04A4) == statHash ))
	{
		int EXPLOIT_THRESHOLD_DEFAULT = 60;
		u32 uCurrentExploitValue = 0;

		int iExploitThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EXPLOIT_WARNING_THRESHOLD", 0x4e4d9902), EXPLOIT_THRESHOLD_DEFAULT);	
		uCurrentExploitValue = StatsInterface::GetIntStat(statHash);

		if (uCurrentExploitValue < iExploitThreshold && (uCurrentExploitValue + m_iStrength) >= iExploitThreshold)
		{
			//Feed me.
			CGameStreamMgr::GetGameStream()->PostTicker( TheText.Get("RP_EXPLOIT_FEED"), false );
		}
	}

	if(statHash != 0 && StatsInterface::IsKeyValid(statHash))
	{
		const int DELAY_REPORT_SAVE = 20000;
		StatsInterface::IncrementStat(statHash, (Float)m_iStrength);
		GetEventScriptNetworkGroup()->Add(CEventNetworkIncrementStat());
		StatsInterface::TryMultiplayerSave(STAT_SAVETYPE_EXPLOITS, DELAY_REPORT_SAVE);
	}
	else
	{
		gnetAssertf(false, "CNetworkIncrementStatEvent::Decide - invalid stat hash");
	}

	eReportType eType;
	int iStat = 0;
	int iThreshold = 0;
	bool bTestForUGCReport = false;

	if (m_uStatHash != 0 && ATSTRINGHASH("MPPLY_VC_HATE", 0xE7072CD) == statHash)
	{
		eType = ReportType_VoiceChat_Hate;
		bTestForUGCReport = true;
        static const int VOICE_HATE_THRESHOLD_DEFAULT = 32;
		iThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PCL_VOICE_HATE_THRESHOLD", 0xb0003301), VOICE_HATE_THRESHOLD_DEFAULT);
	}
	else if (m_uStatHash != 0 && ATSTRINGHASH("MPPLY_EXPLOITS", 0x9F79BA0B) == statHash)
	{
		eType = ReportType_Exploit;
		bTestForUGCReport = true;
        static const int PCL_EXPLOITS_THRESHOLD_DEFAULT = 32;
		iThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PCL_EXPLOITS_THRESHOLD", 0x10ec0bae), PCL_EXPLOITS_THRESHOLD_DEFAULT);
	}
	else if (m_uStatHash != 0 && ATSTRINGHASH("MPPLY_TC_ANNOYINGME", 0x762F9994) == statHash)
	{
		eType = ReportType_TextChat_Annoying;
		bTestForUGCReport = true;
        static const int PCL_TEXT_CHAT_ANNOYING_THRESHOLD_DEFAULT = 32;
		iThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PCL_TEXT_CHAT_ANNOYING_THRESHOLD", 0x7AC96396), PCL_TEXT_CHAT_ANNOYING_THRESHOLD_DEFAULT);
	}
	else if (m_uStatHash != 0 && ATSTRINGHASH("MPPLY_TC_HATE", 0xB722D6C0) == statHash)
	{
		eType = ReportType_TextChat_Hate;
		bTestForUGCReport = true;
        static const int PCL_TEXT_CHAT_HATE_THRESHOLD_DEFAULT = 32;
		iThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PCL_TEXT_CHAT_HATE_THRESHOLD", 0xFB485045), PCL_TEXT_CHAT_HATE_THRESHOLD_DEFAULT);
	}
	else
	{
		eType = ReportType_GameExploit;
		bTestForUGCReport = true;
        static const int GAME_BUGS_THRESHOLD_DEFAULT = 32;
		iThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PCL_GAME_BUGS_THRESHOLD", 0x7ef4aef6), GAME_BUGS_THRESHOLD_DEFAULT);
	}

	if (bTestForUGCReport)
	{
		iStat = StatsInterface::GetIntStat(m_uStatHash);

		if (iStat >= iThreshold)
		{
			CReportMenu::GenerateMPPlayerReport(fromPlayer, eType);
		}
		
	}
	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkIncrementStatEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

    if(!logSplitter.IsEnabled())
    {
        return;
    }

	logSplitter.WriteDataValue("Stat Hash",	"%d", m_uStatHash);
}

#endif // ENABLE_NETWORK_LOGGING

// ================================================================================================================
// REQUEST PHONE EXPLOSION EVENT
// ================================================================================================================
CRequestPhoneExplosionEvent::CRequestPhoneExplosionEvent() 
: netGameEvent(NETWORK_REQUEST_PHONE_EXPLOSION_EVENT, false, false)
, m_nVehicleID(NETWORK_INVALID_OBJECT_ID)
{

}

CRequestPhoneExplosionEvent::CRequestPhoneExplosionEvent(const ObjectId nVehicleID) 
: netGameEvent(NETWORK_REQUEST_PHONE_EXPLOSION_EVENT, false, false)
, m_nVehicleID(nVehicleID)
{

}

void CRequestPhoneExplosionEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRequestPhoneExplosionEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestPhoneExplosionEvent::Trigger(const ObjectId nVehicleID)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CRequestPhoneExplosionEvent *pEvent = rage_new CRequestPhoneExplosionEvent(nVehicleID);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRequestPhoneExplosionEvent::IsInScope(const netPlayer& player) const
{
	bool bInScope = true;
	bInScope &= !player.IsLocal();
	bInScope &= !player.IsBot();
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();
	return bInScope;
}

template <class Serialiser> void CRequestPhoneExplosionEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_OBJECTID(serialiser, m_nVehicleID, "Vehicle Object ID");
}

void CRequestPhoneExplosionEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestPhoneExplosionEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestPhoneExplosionEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_nVehicleID);
	if(pObject && IsVehicleObjectType(pObject->GetObjectType()))
	{
		CNetObjVehicle* pNetVehicle = SafeCast(CNetObjVehicle, pObject);
		if(pNetVehicle)
		{
			CPed* pCulprit = static_cast<const CNetGamePlayer &>(fromPlayer).GetPlayerPed();
			
			//! Do not blow up the vehicle if it has been marked as not damaged by anything.
			bool bInvincible = false;
			if(pNetVehicle->GetVehicle())
			{
				bInvincible = pNetVehicle->GetVehicle()->m_nPhysicalFlags.bNotDamagedByAnything;
			}

			if(!bInvincible)
			{
				pNetVehicle->DetonatePhoneExplosive(pCulprit ? pCulprit->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID);
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CRequestPhoneExplosionEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

	netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_nVehicleID);
	log.WriteDataValue("Vehicle Entity", "%s", pNetObj ? pNetObj->GetLogName() : "INVALID");
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// REQUEST DETACHMENT EVENT
// ================================================================================================================
CRequestDetachmentEvent::CRequestDetachmentEvent() 
: netGameEvent(NETWORK_REQUEST_DETACHMENT_EVENT, false)
, m_PhysicalToDetachID(NETWORK_INVALID_OBJECT_ID)
{

}

CRequestDetachmentEvent::CRequestDetachmentEvent(const CPhysical &physicalToDetach) 
: netGameEvent(NETWORK_REQUEST_DETACHMENT_EVENT, false)
, m_PhysicalToDetachID(NETWORK_INVALID_OBJECT_ID)
{
	if(gnetVerifyf(physicalToDetach.GetNetworkObject(), "Passing non-networked physical to request detachment event!"))
	{
		m_PhysicalToDetachID = physicalToDetach.GetNetworkObject()->GetObjectID();
	}
}

void CRequestDetachmentEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRequestDetachmentEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRequestDetachmentEvent::Trigger(const CPhysical &physicalToDetach)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	if(gnetVerifyf(physicalToDetach.GetNetworkObject(), "Requesting to detach a physical object that is not networked!") &&
		gnetVerifyf(physicalToDetach.GetAttachParent() && static_cast<CPhysical *>(physicalToDetach.GetAttachParent())->GetIsTypeVehicle(), "This event only supports detaching entities from vehicles currently!"))
	{
		gnetDebug2("Requesting detachment event for %s with attached parent: %s", physicalToDetach.GetNetworkObject()->GetLogName(), static_cast<CPhysical*>(physicalToDetach.GetAttachParent())->GetNetworkObject()->GetLogName());
		sysStack::PrintStackTrace();

		gnetAssert(NetworkInterface::IsGameInProgress());
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CRequestDetachmentEvent *pEvent = rage_new CRequestDetachmentEvent(physicalToDetach);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CRequestDetachmentEvent::IsInScope(const netPlayer& player) const
{
	bool inScope = false;

	if(!player.IsLocal() && NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_PhysicalToDetachID, player))
	{
		inScope = true;
	}

	return inScope;
}

template <class Serialiser> void CRequestDetachmentEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_OBJECTID(serialiser, m_PhysicalToDetachID, "Physical To Detach ID");
}

void CRequestDetachmentEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRequestDetachmentEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRequestDetachmentEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &toPlayer)
{
	netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_PhysicalToDetachID, toPlayer);

	if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsPhysical())
	{
		CPhysical *physical = static_cast<CPhysical *>(networkObject->GetEntity());

		if(physical && physical->GetAttachParent() && static_cast<CPhysical *>(physical->GetAttachParent())->GetIsTypeVehicle())
		{
			CVehicle *vehicle = static_cast<CVehicle *>(physical->GetAttachParent());

			for(int index = 0; index < vehicle->GetNumberOfVehicleGadgets(); index++)
			{
				CVehicleGadget *vehicleGadget = vehicle->GetVehicleGadget(index);

				if(vehicleGadget)
				{
					switch(vehicleGadget->GetType())
					{
					case VGT_TRAILER_ATTACH_POINT:
						{
							CVehicleTrailerAttachPoint* attachPoint = static_cast<CVehicleTrailerAttachPoint*>(vehicleGadget);
							if(attachPoint && physical->GetAttachParent() == vehicle)
							{
								vehicleDisplayf( "[TOWTRUCK ROPE DEBUG] CRequestDetachmentEvent::Decide - Detaching trailer." );
								attachPoint->DetachTrailer(vehicle);
							}
						}
						break;
					case VGT_TOW_TRUCK_ARM:
						{
							CVehicleGadgetTowArm *towArm = static_cast<CVehicleGadgetTowArm*>(vehicleGadget);

							if(towArm && towArm->GetAttachedVehicle() == physical)
							{
								vehicleDisplayf( "[TOWTRUCK ROPE DEBUG] CRequestDetachmentEvent::Decide - Detaching entity." );
								towArm->DetachVehicle();
							}
						}
						break;
					case VGT_PICK_UP_ROPE:
					case VGT_PICK_UP_ROPE_MAGNET:
						{
							CVehicleGadgetPickUpRope *pickUpRope = static_cast<CVehicleGadgetPickUpRope*>(vehicleGadget);

							if(pickUpRope && pickUpRope->GetAttachedEntity() == physical)
							{
								pickUpRope->DetachEntity();
							}
						}
						break;
					default:
						break;
					}
				}
			}
		}
	}

	return true;
}

bool CRequestDetachmentEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_REQUEST_DETACHMENT_EVENT)
	{
		const CRequestDetachmentEvent *requestDetachEvent = SafeCast(const CRequestDetachmentEvent, pEvent);

		return (requestDetachEvent->m_PhysicalToDetachID == m_PhysicalToDetachID);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CRequestDetachmentEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_PhysicalToDetachID);
	log.WriteDataValue("Physical Object To Detach", "%s", pNetObj ? pNetObj->GetLogName() : "INVALID");
}

#endif //ENABLE_NETWORK_LOGGING


// ================================================================================================================
// SEND KICK VOTES EVENT
// ================================================================================================================
CSendKickVotesEvent::CSendKickVotesEvent() 
: netGameEvent(NETWORK_KICK_VOTES_EVENT, false, true)
, m_KickVotes(0)
, m_PlayersInScope(0)
{

}

CSendKickVotesEvent::CSendKickVotesEvent(const PlayerFlags kickVotes, const PlayerFlags playersInScope) 
: netGameEvent(NETWORK_KICK_VOTES_EVENT, false, true)
, m_KickVotes(kickVotes)
, m_PlayersInScope(playersInScope)
{
}

void CSendKickVotesEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CSendKickVotesEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CSendKickVotesEvent::Trigger(const PlayerFlags kickVotes, const PlayerFlags playersInScope)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CSendKickVotesEvent *pEvent = rage_new CSendKickVotesEvent(kickVotes, playersInScope);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CSendKickVotesEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	const unsigned SIZEOF_KICK_VOTES_FLAGS = MAX_NUM_PHYSICAL_PLAYERS;
	SERIALISE_BITFIELD(serialiser, m_KickVotes, SIZEOF_KICK_VOTES_FLAGS, "Kick Votes flags");
}

void CSendKickVotesEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CSendKickVotesEvent::IsInScope(const netPlayer& player) const 
{
	return ((m_PlayersInScope & (1<<player.GetPhysicalPlayerIndex())) != 0);
}

void CSendKickVotesEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);

	for (PhysicalPlayerIndex p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
	{
		netPlayer* pKickingPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(p);
		if (pKickingPlayer && pKickingPlayer->GetPhysicalPlayerIndex() != fromPlayer.GetPhysicalPlayerIndex())
		{
			if (m_KickVotes & (1<<p))
			{
				pKickingPlayer->AddKickVote(fromPlayer);
			}
			else
			{
				pKickingPlayer->RemoveKickVote(fromPlayer);
			}
		}
	}
}

bool CSendKickVotesEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_REQUEST_DETACHMENT_EVENT)
	{
		const CSendKickVotesEvent *requestDetachEvent = SafeCast(const CSendKickVotesEvent, pEvent);

		return (requestDetachEvent->m_KickVotes == m_KickVotes);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CSendKickVotesEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Kick vote flags", "%d", m_KickVotes);
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// GIVE PICKUP REWARDS EVENT
// ================================================================================================================
CGivePickupRewardsEvent::CGivePickupRewardsEvent() 
: netGameEvent(NETWORK_GIVE_PICKUP_REWARDS_EVENT, false)
, m_ReceivingPlayers(0)
, m_NumRewards(0)
{
	for (u32 i=0; i<MAX_REWARDS; i++)
	{
		m_Rewards[i] = 0;
	}
}

CGivePickupRewardsEvent::CGivePickupRewardsEvent(PlayerFlags receivingPlayers, u32 rewardHash) 
: netGameEvent(NETWORK_GIVE_PICKUP_REWARDS_EVENT, false, true)
, m_ReceivingPlayers(receivingPlayers)
, m_NumRewards(0)
{
	for (u32 i=0; i<MAX_REWARDS; i++)
	{
		m_Rewards[i] = 0;
	}

	m_Rewards[0] = rewardHash;
	m_NumRewards = 1;
}

void CGivePickupRewardsEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CGivePickupRewardsEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CGivePickupRewardsEvent::Trigger(PlayerFlags receivingPlayers, u32 rewardHash)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CGivePickupRewardsEvent *pEvent = rage_new CGivePickupRewardsEvent(receivingPlayers, rewardHash);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CGivePickupRewardsEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	static const unsigned SIZEOF_REWARD = 32;
	static const unsigned SIZEOF_NUM_REWARDS = datBitsNeeded<MAX_REWARDS>::COUNT;

	SERIALISE_UNSIGNED(serialiser, m_NumRewards, SIZEOF_NUM_REWARDS);
	gnetAssertf(m_NumRewards <= MAX_REWARDS, "Received a reward event with too much reward data!");
	m_NumRewards = MIN(m_NumRewards, MAX_REWARDS);

	for (u32 i=0; i < m_NumRewards; i++)
	{
		SERIALISE_UNSIGNED(serialiser, m_Rewards[i], SIZEOF_REWARD, "Reward Hash");
	}

	gnetAssert(m_NumRewards > 0);
}

void CGivePickupRewardsEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CGivePickupRewardsEvent::IsInScope(const netPlayer& player) const 
{
	return ((m_ReceivingPlayers & (1<<player.GetPhysicalPlayerIndex())) != 0);
}

void CGivePickupRewardsEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CGivePickupRewardsEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	for (u32 i=0; i<m_NumRewards; i++)
	{
		atHashWithStringNotFinal hash(m_Rewards[i]);

		const CPickupRewardData* pPickupReward = CPickupDataManager::GetPickupRewardData(hash);

		if (gnetVerifyf(pPickupReward, "CGivePickupRewardsEvent: Couldn't find pickup reward with hash %u", m_Rewards[i]))
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

			if (gnetVerify(pLocalPlayer) &&
				!CPickupManager::IsSuppressionFlagSet(pPickupReward->GetType()) &&
				pPickupReward->CanGive(NULL, pLocalPlayer))
			{
				pPickupReward->Give(NULL, pLocalPlayer);
			}
		}
	}

	return true;
}

bool CGivePickupRewardsEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_GIVE_PICKUP_REWARDS_EVENT)
	{
		const CGivePickupRewardsEvent *pickupRewardsEvent = SafeCast(const CGivePickupRewardsEvent, pEvent);

		if (pickupRewardsEvent->m_ReceivingPlayers == m_ReceivingPlayers)
		{
			// if the rewards are for the same players, add them to this event and dump the other
			if (m_NumRewards + pickupRewardsEvent->m_NumRewards <= MAX_REWARDS)
			{
				for (u32 i=0; i<pickupRewardsEvent->m_NumRewards; i++)
				{
					if (AssertVerify(m_NumRewards < MAX_REWARDS))
					{
						m_Rewards[m_NumRewards++] = pickupRewardsEvent->m_Rewards[i];
					}
				}

				return true;
			}
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CGivePickupRewardsEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

#if !__FINAL
	for (u32 i=0; i<m_NumRewards; i++)
	{
		atHashWithStringNotFinal hash(m_Rewards[i]);

		const CPickupRewardData* pPickupReward = CPickupDataManager::GetPickupRewardData(hash);

		log.WriteDataValue("Reward", "%s", pPickupReward->GetName());
	}
#endif
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// NETWORK CRC HASH CHECK EVENT
// ================================================================================================================
CNetworkCrcHashCheckEvent::CNetworkCrcHashCheckEvent() 
: netGameEvent(NETWORK_CRC_HASH_CHECK_EVENT, false)
, m_requestId(0)
, m_ReceivingPlayer(INVALID_PLAYER_INDEX)
, m_eSystemToCheck(NetworkHasherUtil::INVALID_HASHABLE)
, m_uSpecificCheck(0)
, m_bIsReply(false)
, m_uChecksumResult(0)
{
	m_fileNameToHash[0]='\0';
}

CNetworkCrcHashCheckEvent::CNetworkCrcHashCheckEvent(u8 thisRequestId, bool bIsReply, PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck, const char* fileNameToHash, u32 resultChecksum)
: netGameEvent(NETWORK_CRC_HASH_CHECK_EVENT, false)
, m_requestId(thisRequestId)
, m_ReceivingPlayer(receivingPlayer)
, m_eSystemToCheck(systemToCheck)
, m_uSpecificCheck(specificCheck)
, m_bIsReply(bIsReply)
, m_uChecksumResult(resultChecksum)
{
	m_fileNameToHash[0]='\0';

	// Only requests bother to sync strings. Replies can match due to m_requestId
	if(!m_bIsReply)
	{
		if(fileNameToHash)
		{
			gnetAssertf(systemToCheck == NetworkHasherUtil::CRC_GENERIC_FILE_CONTENTS, "Why are you passing a file name to hash to a not CRC_GENERIC_FILE_CONTENTS crc? (%s)", fileNameToHash);
			gnetAssertf((u32)strlen(fileNameToHash) < MAX_FILENAME_LENGTH,"CNetworkCrcHashCheckEvent is trying to handle a string bigger than %d chars: (%s)", MAX_FILENAME_LENGTH, fileNameToHash);

			safecpy(m_fileNameToHash, fileNameToHash, MAX_FILENAME_LENGTH);
		}
		else 
		{
			gnetAssertf(systemToCheck != NetworkHasherUtil::CRC_GENERIC_FILE_CONTENTS, "A CRC_GENERIC_FILE_CONTENTS crc check was triggered without a valid specific file name?");
		}
	}
}

void CNetworkCrcHashCheckEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkCrcHashCheckEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkCrcHashCheckEvent::TriggerRequest(PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck, const char* fileNameToHash)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	const CNetGamePlayer* pRemotePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(receivingPlayer);
	if( AssertVerify(pLocalPlayer && pRemotePlayer))
	{
		if(!pRemotePlayer->IsCheatAlreadyNotified(NetworkRemoteCheaterDetector::CRC_COMPROMISED))
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			const u8 thisRequestId = NetworkHasherUtil::GetNextRequestId();
			hasherDebugf3("Triggering a network crc hash request: id[%d]", thisRequestId);
			CNetworkCrcHashCheckEvent *pEvent = rage_new CNetworkCrcHashCheckEvent(thisRequestId, false, receivingPlayer, systemToCheck, specificCheck, fileNameToHash, 0);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);

			// On a request, start our own hash calculation, so we can compare it with its posterior reply
			NetworkHasherUtil::PushNewHashRequest(thisRequestId, pLocalPlayer->GetRlGamerId(), pRemotePlayer->GetRlGamerId(), (NetworkHasherUtil::eHASHABLE_SYSTEM)systemToCheck, specificCheck, fileNameToHash, false);
		}
#if !__FINAL
		else
		{
			hasherDebugf3("Skip CRC check for player already flagged as hacker");
		}
#endif
	}
}

void CNetworkCrcHashCheckEvent::TriggerReply(u8 repliedRequestId, PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck, u32 resultChecksum)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	hasherDebugf3("Replying a network crc hash request: id[%d]", repliedRequestId);
	CNetworkCrcHashCheckEvent *pEvent = rage_new CNetworkCrcHashCheckEvent(repliedRequestId, true, receivingPlayer, systemToCheck, specificCheck, NULL, resultChecksum);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CNetworkCrcHashCheckEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	static const unsigned SIZEOF_HASHABLE_SYSTEM = datBitsNeeded<NetworkHasherUtil::INVALID_HASHABLE>::COUNT;
	static const unsigned SIZEOF_REQUEST_ID = 8;
	static const unsigned SIZEOF_SPECIFIC_CHECK = 32;
	static const unsigned SIZEOF_CHECKSUM_RESULT = 32;

	SERIALISE_UNSIGNED(serialiser, m_requestId, SIZEOF_REQUEST_ID, "ReqId");

	SERIALISE_BOOL(serialiser, m_bIsReply, "IsReply");
	if(m_bIsReply)
	{
		SERIALISE_UNSIGNED(serialiser, m_uChecksumResult, SIZEOF_CHECKSUM_RESULT, "CRC");
	}
	else
	{
		m_uChecksumResult = 0;
	}

	SERIALISE_UNSIGNED(serialiser, m_eSystemToCheck, SIZEOF_HASHABLE_SYSTEM, "SystemCheck");

	// Sync hashes if this is a reply or a non GenericFile request.
	if(m_eSystemToCheck!=NetworkHasherUtil::CRC_GENERIC_FILE_CONTENTS || m_bIsReply)
	{
		m_fileNameToHash[0]='\0';
		SERIALISE_UNSIGNED(serialiser, m_uSpecificCheck, SIZEOF_SPECIFIC_CHECK, "SpecificCheck");
	}
	else
	{
		SERIALISE_STRING(serialiser, m_fileNameToHash, MAX_FILENAME_LENGTH, "FileToHash");

		// at the end of the day m_uSpecificCheck is what's send as extraDesc to R* crc cheater telemetry event. 
		// Set it as the filename hash. Already has that value in the sender.
		// This allows us to have the hash in the remote machine without syncing it, so we can send it back in the correspondent reply
		m_uSpecificCheck = atStringHash(m_fileNameToHash);
	}
}

void CNetworkCrcHashCheckEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CNetworkCrcHashCheckEvent::IsInScope(const netPlayer& player) const 
{
	return m_ReceivingPlayer == player.GetPhysicalPlayerIndex();
}

void CNetworkCrcHashCheckEvent::Handle(datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkCrcHashCheckEvent::Decide(const netPlayer& fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	hasherDebugf3("Received a network crc hash %s: id[%d]", m_bIsReply? "REPLY": "REQUEST", m_requestId);
	const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if( AssertVerify(pLocalPlayer) )
	{
		const rlGamerId& localPlayerId = pLocalPlayer->GetRlGamerId();
		const rlGamerId& fromPlayerId = fromPlayer.GetRlGamerId();

		if( AssertVerify(localPlayerId != fromPlayerId) )
		{
			if(m_bIsReply)
			{
				NetworkHasherUtil::PushNewHashResult(m_requestId, localPlayerId, fromPlayerId, (NetworkHasherUtil::eHASHABLE_SYSTEM)m_eSystemToCheck, m_uSpecificCheck, m_uChecksumResult);
			}
			else
			{
				NetworkHasherUtil::PushNewHashRequest(m_requestId, fromPlayerId, localPlayerId, (NetworkHasherUtil::eHASHABLE_SYSTEM)m_eSystemToCheck, m_uSpecificCheck, m_fileNameToHash, true);
			}
		}
	}

	return true;
}

bool CNetworkCrcHashCheckEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_CRC_HASH_CHECK_EVENT)
	{
		const CNetworkCrcHashCheckEvent *pCrcHashEvent = SafeCast(const CNetworkCrcHashCheckEvent, pEvent);

		return pCrcHashEvent->m_ReceivingPlayer == m_ReceivingPlayer
			&& pCrcHashEvent->m_eSystemToCheck == m_eSystemToCheck
			&& pCrcHashEvent->m_uSpecificCheck == m_uSpecificCheck
			&& pCrcHashEvent->m_requestId == m_requestId
			&& pCrcHashEvent->m_bIsReply == m_bIsReply
			&& pCrcHashEvent->m_uChecksumResult == m_uChecksumResult
			&& !strncmp(pCrcHashEvent->m_fileNameToHash, m_fileNameToHash, MAX_FILENAME_LENGTH);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkCrcHashCheckEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool UNUSED_PARAM(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());

	if(!logSplitter.IsEnabled())
	{
		return;
	}

	logSplitter.WriteDataValue("Req ID", "%d", m_requestId);
	logSplitter.WriteDataValue("IsReply", "%d", m_bIsReply);
	logSplitter.WriteDataValue("System Asked", "%d", m_eSystemToCheck);
	logSplitter.WriteDataValue("System Details", "%d", m_uSpecificCheck);
	logSplitter.WriteDataValue("FileToHash", "%s", m_fileNameToHash);
	logSplitter.WriteDataValue("CRC", "%d", m_uChecksumResult);
}

#endif // ENABLE_NETWORK_LOGGING


// ================================================================================================================
// BLOW UP VEHICLE EVENT
// ================================================================================================================
CBlowUpVehicleEvent::CBlowUpVehicleEvent() 
: netGameEvent(BLOW_UP_VEHICLE_EVENT, false, false)
, m_vehicleID(NETWORK_INVALID_OBJECT_ID)
, m_culpritID(NETWORK_INVALID_OBJECT_ID)
, m_weaponHash(0)
, m_bAddExplosion(false)
, m_bDelayedExplosion(false)
, m_timeAdded(sysTimer::GetSystemMsTime())
{

}

CBlowUpVehicleEvent::CBlowUpVehicleEvent(ObjectId vehicleId, ObjectId culpritId, bool bAddExplosion, u32 weaponHash, bool bDelayedExplosion) 
: netGameEvent(BLOW_UP_VEHICLE_EVENT, false, false)
, m_vehicleID(vehicleId)
, m_culpritID(culpritId)
, m_weaponHash(weaponHash)
, m_bAddExplosion(bAddExplosion)
, m_bDelayedExplosion(bDelayedExplosion)
, m_timeAdded(sysTimer::GetSystemMsTime())
{
}

CBlowUpVehicleEvent::~CBlowUpVehicleEvent()
{
	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);

	if (vehicleObject && !vehicleObject->IsClone() && !vehicleObject->IsPendingOwnerChange())
	{
		// the migration failed and the vehicle is local again, blow it up
		CEntity* pEntity = vehicleObject->GetEntity();

		if (pEntity && AssertVerify(pEntity->GetIsTypeVehicle()))
		{
			netObject *culpritObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_culpritID);

			static_cast<CVehicle*>(pEntity)->BlowUpCar(culpritObject ? culpritObject->GetEntity() : NULL, false, m_bAddExplosion, false, m_weaponHash, m_bDelayedExplosion);
		}	
	}
}

void CBlowUpVehicleEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CBlowUpVehicleEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CBlowUpVehicleEvent::Trigger(CVehicle& vehicle, CEntity *pCulprit, bool bAddExplosion, u32 weaponHash, bool bDelayedExplosion)
{
	if (AssertVerify(vehicle.GetNetworkObject()))
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		netObject* pCulpritObj = NetworkUtils::GetNetworkObjectFromEntity(pCulprit);

		gnetAssert(NetworkInterface::IsGameInProgress());
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CBlowUpVehicleEvent *pEvent = rage_new CBlowUpVehicleEvent(vehicle.GetNetworkObject()->GetObjectID(), pCulpritObj ? pCulpritObj->GetObjectID() : NETWORK_INVALID_OBJECT_ID,  bAddExplosion, weaponHash, bDelayedExplosion);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

template <class Serialiser> void CBlowUpVehicleEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle");
	SERIALISE_OBJECTID(serialiser, m_culpritID, "Culprit");
	SERIALISE_UNSIGNED(serialiser, m_weaponHash, 32, "Weapon Hash");
	SERIALISE_BOOL(serialiser, m_bAddExplosion, "Add Explosion");
	SERIALISE_BOOL(serialiser, m_bDelayedExplosion, "Delayed Explosion");
}

void CBlowUpVehicleEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CBlowUpVehicleEvent::IsInScope(const netPlayer& player) const 
{
	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);

	// sent to player that owns the object, or the player the vehicle is migrating to 
	if (vehicleObject && !player.IsLocal())
	{
		if (vehicleObject->IsClone())
		{
			return (&player == vehicleObject->GetPlayerOwner());
		}
		else if (vehicleObject->GetPendingPlayerOwner())
		{
			return (&player == vehicleObject->GetPendingPlayerOwner());
		}
	}

	return false;
}

void CBlowUpVehicleEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CBlowUpVehicleEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);

	// ignore the event if the entity is not local, the event will get sent to the new owner
	if (!vehicleObject || vehicleObject->IsClone() || vehicleObject->IsPendingOwnerChange())
	{
		return false;
	}

	CEntity* pEntity = vehicleObject->GetEntity();

	if (pEntity && AssertVerify(pEntity->GetIsTypeVehicle()))
	{
		netObject *culpritObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_culpritID);

		static_cast<CVehicle*>(pEntity)->BlowUpCar(culpritObject ? culpritObject->GetEntity() : NULL, false, m_bAddExplosion, false, m_weaponHash, m_bDelayedExplosion);
	}

	return true;
}

bool CBlowUpVehicleEvent::HasTimedOut() const
{
	const unsigned BLOWUP_EVENT_TIMEOUT = 2000;

	u32 currentTime = sysTimer::GetSystemMsTime();

	// basic wrapping handling (this doesn't have to be precise, and the system time should never wrap)
	if(currentTime < m_timeAdded)
	{
		m_timeAdded = currentTime;
	}

	u32 timeElapsed = currentTime - m_timeAdded;

	return timeElapsed > BLOWUP_EVENT_TIMEOUT;
}

bool CBlowUpVehicleEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() ==BLOW_UP_VEHICLE_EVENT)
	{
		const CBlowUpVehicleEvent *blowUpVehicleEvent = SafeCast(const CBlowUpVehicleEvent, pEvent);

		return (blowUpVehicleEvent->m_vehicleID == m_vehicleID);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CBlowUpVehicleEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);
	netObject *culpritObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_culpritID);

	if (vehicleObject)
	{
		log.WriteDataValue("Vehicle", vehicleObject->GetLogName());
		log.WriteDataValue("Culprit", culpritObject ? culpritObject->GetLogName() : "-none");
		log.WriteDataValue("Weapon Hash", "%u", m_weaponHash);
		log.WriteDataValue("Add explosion", "%s", m_bAddExplosion ? "true" : "false");
		log.WriteDataValue("Delayed explosion", "%s", m_bDelayedExplosion ? "true" : "false");
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// ACTIVATE VEHICLE SPECIAL ABILITY EVENT
// ================================================================================================================
CActivateVehicleSpecialAbilityEvent::CActivateVehicleSpecialAbilityEvent() 
	: netGameEvent(ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT, false, false)
	, m_vehicleID(NETWORK_INVALID_OBJECT_ID)
	, m_abilityType(NUM_ABILITY_TYPES)
	, m_timeAdded(sysTimer::GetSystemMsTime())
{

}

CActivateVehicleSpecialAbilityEvent::CActivateVehicleSpecialAbilityEvent(ObjectId vehicleId, eAbilityType abilityType) 
	: netGameEvent(ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT, false, false)
	, m_vehicleID(vehicleId)
	, m_abilityType(abilityType)
	, m_timeAdded(sysTimer::GetSystemMsTime())
{
}

CActivateVehicleSpecialAbilityEvent::~CActivateVehicleSpecialAbilityEvent()
{	
}

void CActivateVehicleSpecialAbilityEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CActivateVehicleSpecialAbilityEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CActivateVehicleSpecialAbilityEvent::Trigger(CVehicle& vehicle, eAbilityType abilityType)
{
	if (AssertVerify(vehicle.GetNetworkObject()))
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);
		
		gnetAssert(NetworkInterface::IsGameInProgress());
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CActivateVehicleSpecialAbilityEvent *pEvent = rage_new CActivateVehicleSpecialAbilityEvent(vehicle.GetNetworkObject()->GetObjectID(), abilityType);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

template <class Serialiser> void CActivateVehicleSpecialAbilityEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle");
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_abilityType), SIZEOF_ABILITY_TYPE, "Ability Type");
}

void CActivateVehicleSpecialAbilityEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CActivateVehicleSpecialAbilityEvent::IsInScope(const netPlayer& player) const 
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	if(!pObject)
	{
		gnetAssertf(0, "CActivateVehicleSpecialAbility - Invalid vehicle ID supplied!");
		return false;
	}

	bool bInScope = true;
	bInScope &= (pObject->IsPendingCloning(player) || pObject->HasBeenCloned(player));
	bInScope &= !SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession();

	return bInScope;
}

void CActivateVehicleSpecialAbilityEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CActivateVehicleSpecialAbilityEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	if(pObject)
	{
		CNetObjVehicle* pNetObject = SafeCast(CNetObjVehicle, pObject);
		CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);

		if (pVehicle)
		{
			CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);

			if (pCar)
			{
				if (m_abilityType == JUMP)
				{
					pVehicle->SetIsDoingJump(true);
				}				
				else if (m_abilityType == ROCKET_BOOST)
				{
					pVehicle->SetRocketBoostedFromNetwork();
				}
				else if (m_abilityType == ROCKET_BOOST_STOP)
				{
					pVehicle->SetRocketBoostedStoppedFromNetwork();
				}
				else if (m_abilityType == SHUNT_L)
				{
					pVehicle->SetShuntLeftFromNetwork();
				}
				else if (m_abilityType == SHUNT_R)
				{
					pVehicle->SetShuntRightFromNetwork();
				}
			}
		}
	}
	return true;
}

bool CActivateVehicleSpecialAbilityEvent::HasTimedOut() const
{
	const unsigned SPECIAL_ABILITY_TIMEOUT = 250;
	u32 currentTime = sysTimer::GetSystemMsTime();

	if(currentTime < m_timeAdded)
	{
		m_timeAdded = currentTime;
	}

	u32 timeElapsed = currentTime - m_timeAdded;

	return timeElapsed > SPECIAL_ABILITY_TIMEOUT;
}

bool CActivateVehicleSpecialAbilityEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT)
	{
		const CActivateVehicleSpecialAbilityEvent *activateVehicleSpecialEvent = SafeCast(const CActivateVehicleSpecialAbilityEvent, pEvent);

		return (activateVehicleSpecialEvent->m_vehicleID == m_vehicleID && activateVehicleSpecialEvent->m_abilityType == m_abilityType);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CActivateVehicleSpecialAbilityEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_vehicleID);

	if (vehicleObject)
	{
		log.WriteDataValue("Vehicle", vehicleObject->GetLogName());
		log.WriteDataValue("Ability Type", "%d", m_abilityType);
	}
}

#endif //ENABLE_NETWORK_LOGGING


// ================================================================================================================
// NETWORK SPECIAL FIRE WEAPON
// ================================================================================================================
CNetworkSpecialFireEquippedWeaponEvent::CNetworkSpecialFireEquippedWeaponEvent() 
	: netGameEvent(NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON, false, false)
	, m_entityID(NETWORK_INVALID_OBJECT_ID)
	, m_vecTargetIsZero(true)
	, m_vecStart(VEC3_ZERO)
	, m_vecTarget(VEC3_ZERO)
	, m_setPerfectAccuracy(false)
{

}

CNetworkSpecialFireEquippedWeaponEvent::CNetworkSpecialFireEquippedWeaponEvent(const ObjectId entityID, const bool bvecTargetIsZero, const Vector3& vecStart, const Vector3& vecTarget, const bool bSetPerfectAccuracy) 
	: netGameEvent(NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON, false, false)
	, m_entityID(entityID)
	, m_vecTargetIsZero(bvecTargetIsZero)
	, m_vecStart(vecStart)
	, m_vecTarget(vecTarget)
	, m_setPerfectAccuracy(bSetPerfectAccuracy)
{
}

CNetworkSpecialFireEquippedWeaponEvent::~CNetworkSpecialFireEquippedWeaponEvent()
{
}

void CNetworkSpecialFireEquippedWeaponEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkSpecialFireEquippedWeaponEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkSpecialFireEquippedWeaponEvent::Trigger(const CEntity* pEntity, const bool bvecTargetIsZero, const Vector3& vecStart, const Vector3& vecTarget, const bool bSetPerfectAccuracy)
{
	if (AssertVerify(pEntity))
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		netObject* pNetObjEntity = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
		if (pNetObjEntity)
		{
			gnetAssert(NetworkInterface::IsGameInProgress());
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CNetworkSpecialFireEquippedWeaponEvent *pEvent = rage_new CNetworkSpecialFireEquippedWeaponEvent(pNetObjEntity->GetObjectID(), bvecTargetIsZero, vecStart, vecTarget, bSetPerfectAccuracy);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
}

template <class Serialiser> void CNetworkSpecialFireEquippedWeaponEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_entityID, "Entity");
	SERIALISE_BOOL(serialiser, m_vecTargetIsZero, "Vec Target Is Zero");
	SERIALISE_BOOL(serialiser, m_setPerfectAccuracy, "Set Perfect Accuracy");
	if (!m_vecTargetIsZero)
	{
		SERIALISE_POSITION(serialiser, m_vecStart, "Vec Start");
		SERIALISE_POSITION(serialiser, m_vecTarget, "Vec Target");
	}
	else
	{
		m_vecStart.Zero();
		m_vecTarget.Zero();
	}
}

void CNetworkSpecialFireEquippedWeaponEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CNetworkSpecialFireEquippedWeaponEvent::IsInScope(const netPlayer& player) const 
{
	netObject *pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_entityID);

	// sent to player that owns the object, or the player the vehicle is migrating to 
	if (pNetObject && !player.IsLocal())
	{
		return pNetObject->IsInScope(player);
	}

	return false;
}

void CNetworkSpecialFireEquippedWeaponEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkSpecialFireEquippedWeaponEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject *pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_entityID);
	if (!pNetObject)
		return false;

	CEntity* pEntity = pNetObject->GetEntity();

	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pEntity);
			if (pPed && pPed->GetWeaponManager())
			{
				CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if( pEquippedWeapon && pWeaponObject )
				{
					Matrix34 matWeapon;
					if(pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ) != -1)
					{
						pWeaponObject->GetGlobalMtx( pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ), matWeapon );
					}
					else
					{
						pWeaponObject->GetMatrixCopy( matWeapon );
					}

					if (m_vecTargetIsZero)
					{
						CWeapon::sFireParams params( pPed, matWeapon, NULL, NULL );
						params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
						if(m_setPerfectAccuracy)
							params.iFireFlags.SetFlag( CWeapon::FF_SetPerfectAccuracy );
						pEquippedWeapon->Fire( params );

					}
					else
					{
						CWeapon::sFireParams params( pPed, matWeapon, &m_vecStart, &m_vecTarget );
						params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
						if(m_setPerfectAccuracy)
							params.iFireFlags.SetFlag( CWeapon::FF_SetPerfectAccuracy );
						pEquippedWeapon->Fire( params );
					}
				}
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkSpecialFireEquippedWeaponEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject *pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_entityID);

	if (pNetObject)
	{
		log.WriteDataValue("Entity", pNetObject->GetLogName());
		log.WriteDataValue("m_vecTargetIsZero", "%s", m_vecTargetIsZero ? "true" : "false");
		log.WriteDataValue("m_setPerfectAccuracy", "%s", m_setPerfectAccuracy ? "true" : "false");
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// NETWORK_RESPONDED_TO_THREAT_EVENT
// ================================================================================================================
CNetworkRespondedToThreatEvent::CNetworkRespondedToThreatEvent()
	: netGameEvent(NETWORK_RESPONDED_TO_THREAT_EVENT, false, false)
	, m_numRecipients(0)
	, m_responderEntityID(NETWORK_INVALID_OBJECT_ID)
	, m_threatEntityID(NETWORK_INVALID_OBJECT_ID)
{
	for(u32 i=0; i < MAX_RECIPIENTS_PER_EVENT; i++)
	{
		m_recipientsEntityID[i] = NETWORK_INVALID_OBJECT_ID;
	}
}

CNetworkRespondedToThreatEvent::CNetworkRespondedToThreatEvent(const ObjectId responderEntityID, const ObjectId threateningEntityID)
	: netGameEvent(NETWORK_RESPONDED_TO_THREAT_EVENT, false, false)
	, m_numRecipients(0)
	, m_responderEntityID(responderEntityID)
	, m_threatEntityID(threateningEntityID)
{
	for(u32 i=0; i < MAX_RECIPIENTS_PER_EVENT; i++)
	{
		m_recipientsEntityID[i] = NETWORK_INVALID_OBJECT_ID;
	}
}

CNetworkRespondedToThreatEvent::~CNetworkRespondedToThreatEvent()
{
}

bool CNetworkRespondedToThreatEvent::AddRecipientObjectID(const ObjectId& objectID)
{
	// First check if this recipient is already listed
	for(u32 i=0; i < m_numRecipients; i++)
	{
		if( m_recipientsEntityID[i] == objectID )
		{
			return true;
		}
	}

	if( m_numRecipients < MAX_RECIPIENTS_PER_EVENT )
	{
		m_recipientsEntityID[m_numRecipients] = objectID;
		m_numRecipients++;
		return true;
	}

	return false;
}

void CNetworkRespondedToThreatEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkRespondedToThreatEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkRespondedToThreatEvent::Trigger(const CEntity* pRecipientEntity, const CEntity* pResponderEntity, const CEntity* pThreatEntity)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);
	
	gnetAssert(NetworkInterface::IsGameInProgress());

	bool bAddedRecipientObjectID = false;

	if( AssertVerify(pRecipientEntity) && AssertVerify(pResponderEntity) && AssertVerify(pThreatEntity) )
	{
		netObject* pRecipientNetObjEntity = NetworkUtils::GetNetworkObjectFromEntity(pRecipientEntity);
		netObject* pResponderNetObjEntity = NetworkUtils::GetNetworkObjectFromEntity(pResponderEntity);
		netObject* pThreatNetObjEntity = NetworkUtils::GetNetworkObjectFromEntity(pThreatEntity);

		if( pRecipientNetObjEntity && pResponderNetObjEntity && pThreatNetObjEntity )
		{
			const ObjectId& recipientObjectID = pRecipientNetObjEntity->GetObjectID();
			const ObjectId& responderObjectID = pResponderNetObjEntity->GetObjectID();
			const ObjectId& threatObjectID = pThreatNetObjEntity->GetObjectID();

			// Search for any existing events on the queue and try to append recipient
			atDNode<netGameEvent*, datBase>* pNode = NetworkInterface::GetEventManager().GetEventListHead();
			while( pNode )
			{
				netGameEvent* pNetworkEvent = pNode->Data;
				if( pNetworkEvent && pNetworkEvent->GetEventType() == NETWORK_RESPONDED_TO_THREAT_EVENT && !pNetworkEvent->IsFlaggedForRemoval())
				{
					CNetworkRespondedToThreatEvent* pRespondedToThreatEvent = static_cast<CNetworkRespondedToThreatEvent*>(pNetworkEvent);
					// existing event threat and responder match
					if( pRespondedToThreatEvent->m_responderEntityID == responderObjectID && pRespondedToThreatEvent->m_threatEntityID == threatObjectID )
					{
						if( pRespondedToThreatEvent->AddRecipientObjectID(recipientObjectID) )
						{
							bAddedRecipientObjectID = true;
							break;
						}
					}
				}
				pNode = pNode->GetNext();
			}

			if( !bAddedRecipientObjectID )
			{
				NetworkInterface::GetEventManager().CheckForSpaceInPool();
				CNetworkRespondedToThreatEvent* pEvent = rage_new CNetworkRespondedToThreatEvent(responderObjectID, threatObjectID);
				pEvent->AddRecipientObjectID(recipientObjectID);
				NetworkInterface::GetEventManager().PrepareEvent(pEvent);
			}
		}
	}
}

template <class Serialiser> void CNetworkRespondedToThreatEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_numRecipients, SIZEOF_NUM_RECIPIENTS, "Num Recipients");
	SERIALISE_OBJECTID(serialiser, m_responderEntityID, "Responder Entity");
	SERIALISE_OBJECTID(serialiser, m_threatEntityID, "Threat Entity");

    gnetAssertf(m_numRecipients <= MAX_RECIPIENTS_PER_EVENT, "Received a responded to threat event with too many recipients!");
    m_numRecipients = MIN(m_numRecipients, MAX_RECIPIENTS_PER_EVENT);

	for(u32 i=0; i < m_numRecipients; i++)
	{
		SERIALISE_OBJECTID(serialiser, m_recipientsEntityID[i], "Recipient ID");
	}
}

void CNetworkRespondedToThreatEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CNetworkRespondedToThreatEvent::IsInScope(const netPlayer& player) const 
{
	// check recipients
	for(u32 i=0; i < m_numRecipients; i++)
	{
		netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_recipientsEntityID[i]);

		// send if given player owns any recipient entity
		if (pRecipientNetObject)
		{
			if (pRecipientNetObject->IsClone() && pRecipientNetObject->GetPlayerOwner() == &player)
			{
				return true;
			}
			else if (pRecipientNetObject->GetPendingPlayerOwner() && pRecipientNetObject->GetPendingPlayerOwner() == &player)
			{
				return true;
			}
		}
	}

	return false;
}

void CNetworkRespondedToThreatEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkRespondedToThreatEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pResponderNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_responderEntityID);
	netObject* pThreatNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_threatEntityID);

	CEntity* pResponderEntity = pResponderNetObject ? pResponderNetObject->GetEntity() : NULL;
	CEntity* pThreatEntity = pThreatNetObject ? pThreatNetObject->GetEntity() : NULL;

	if( pResponderEntity && pThreatEntity )
	{
		if( pResponderEntity->GetIsTypePed() && pThreatEntity->GetIsTypePed() )
		{
			CPed* pResponderPed = static_cast<CPed*>(pResponderEntity);
			CPed* pThreatPed = static_cast<CPed*>(pThreatEntity);

			//Create the event.
			CEventRespondedToThreat respondedToThreatEvent(pResponderPed, pThreatPed);

			// Traverse recipients
			for(u32 i=0; i < m_numRecipients; i++)
			{
				// If the recipient object is locally controlled
				netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_recipientsEntityID[i]);
				if( pRecipientNetObject && !pRecipientNetObject->IsClone() )
				{
					CEntity* pRecipientEntity = pRecipientNetObject->GetEntity();
					if(pRecipientEntity && pRecipientEntity->GetIsTypePed())
					{
						CPed* pRecipientPed = static_cast<CPed*>(pRecipientEntity);

						// Add to ped intelligence of the ped
						pRecipientPed->GetPedIntelligence()->AddEvent(respondedToThreatEvent);
					}
				}
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkRespondedToThreatEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject* pResponderNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_responderEntityID);
	if (pResponderNetObject)
	{
		log.WriteDataValue("Responder Entity", pResponderNetObject->GetLogName());
	}

	netObject* pThreatNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_threatEntityID);
	if (pThreatNetObject)
	{
		log.WriteDataValue("Threat Entity", pThreatNetObject->GetLogName());
	}

	for(u32 i=0; i < m_numRecipients; i++)
	{
		netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_recipientsEntityID[i]);
		if (pRecipientNetObject)
		{
			log.WriteDataValue("Recipient Entity", pRecipientNetObject->GetLogName());
		}
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// NETWORK_SHOUT_TARGET_POSITION_EVENT
// ================================================================================================================
CNetworkShoutTargetPositionEvent::CNetworkShoutTargetPositionEvent()
	: netGameEvent(NETWORK_SHOUT_TARGET_POSITION_EVENT, false, false)
	, m_shoutingEntityID(NETWORK_INVALID_OBJECT_ID)
	, m_targetEntityID(NETWORK_INVALID_OBJECT_ID)
	, m_numRecipients(0)
{
	for(u32 i=0; i < MAX_RECIPIENTS_PER_EVENT; i++)
	{
		m_recipientsEntityID[i] = NETWORK_INVALID_OBJECT_ID;
	}
}

CNetworkShoutTargetPositionEvent::CNetworkShoutTargetPositionEvent(const ObjectId shoutingEntityID, const ObjectId targetEntityID)
	: netGameEvent(NETWORK_SHOUT_TARGET_POSITION_EVENT, false, false)
	, m_shoutingEntityID(shoutingEntityID)
	, m_targetEntityID(targetEntityID)
	, m_numRecipients(0)
{
	for(u32 i=0; i < MAX_RECIPIENTS_PER_EVENT; i++)
	{
		m_recipientsEntityID[i] = NETWORK_INVALID_OBJECT_ID;
	}
}

CNetworkShoutTargetPositionEvent::~CNetworkShoutTargetPositionEvent()
{
}

bool CNetworkShoutTargetPositionEvent::AddRecipientObjectID(const ObjectId& objectID)
{
	for(u32 i=0; i < m_numRecipients; i++)
	{
		// there should be no duplicates being added
		if(m_recipientsEntityID[i] == objectID)
		{
			return true;
		}
	}

	if( m_numRecipients < MAX_RECIPIENTS_PER_EVENT )
	{
		m_recipientsEntityID[m_numRecipients] = objectID;
		m_numRecipients++;
		return true;
	}

	return false;
}

void CNetworkShoutTargetPositionEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkShoutTargetPositionEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkShoutTargetPositionEvent::Trigger(const CEntity* pRecipientEntity, const CEntity* pShoutingEntity, const CEntity* pTargetEntity)
{	
	gnetAssert(NetworkInterface::IsGameInProgress());

	bool bAddedRecipientObjectID = false;

	if( AssertVerify(pRecipientEntity) && AssertVerify(pShoutingEntity) && AssertVerify(pTargetEntity) )
	{
		netObject* pRecipientNetObjEntity	= NetworkUtils::GetNetworkObjectFromEntity(pRecipientEntity);
		netObject* pShoutingNetObjEntity	= NetworkUtils::GetNetworkObjectFromEntity(pShoutingEntity);
		netObject* pTargetNetObjEntity		= NetworkUtils::GetNetworkObjectFromEntity(pTargetEntity);

		if( pRecipientNetObjEntity && pShoutingNetObjEntity && pTargetNetObjEntity )
		{
			const ObjectId& recipientObjectID	= pRecipientNetObjEntity->GetObjectID();
			const ObjectId& shoutingObjectID	= pShoutingNetObjEntity->GetObjectID();
			const ObjectId& targetObjectID		= pTargetNetObjEntity->GetObjectID();

			// Search for any existing events on the queue and try to append recipient
			atDNode<netGameEvent*, datBase>* pNode = NetworkInterface::GetEventManager().GetEventListHead();
			while( pNode )
			{
				netGameEvent* pNetworkEvent = pNode->Data;
				if( pNetworkEvent && pNetworkEvent->GetEventType() == NETWORK_SHOUT_TARGET_POSITION_EVENT && !pNetworkEvent->IsFlaggedForRemoval())
				{
					CNetworkShoutTargetPositionEvent* pShoutTargetPositionEvent = static_cast<CNetworkShoutTargetPositionEvent*>(pNetworkEvent);
					// existing event threat and responder match
					if( pShoutTargetPositionEvent->m_shoutingEntityID == shoutingObjectID && pShoutTargetPositionEvent->m_targetEntityID == targetObjectID )
					{
						if( pShoutTargetPositionEvent->AddRecipientObjectID(recipientObjectID) )
						{
							bAddedRecipientObjectID = true;
							break;
						}
					}
				}
				pNode = pNode->GetNext();
			}

			if( !bAddedRecipientObjectID )
			{
				NetworkInterface::GetEventManager().CheckForSpaceInPool();
				CNetworkShoutTargetPositionEvent* pEvent = rage_new CNetworkShoutTargetPositionEvent(shoutingObjectID, targetObjectID);
				pEvent->AddRecipientObjectID(recipientObjectID);
				NetworkInterface::GetEventManager().PrepareEvent(pEvent);
			}
		}
	}
}

template <class Serialiser> void CNetworkShoutTargetPositionEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_numRecipients, SIZEOF_NUM_RECIPIENTS, "Num Recipients");
	SERIALISE_OBJECTID(serialiser, m_shoutingEntityID,	"Shouting Entity");
	SERIALISE_OBJECTID(serialiser, m_targetEntityID,		"Target	Entity");

    gnetAssertf(m_numRecipients <= MAX_RECIPIENTS_PER_EVENT, "Received a shout target position event with too many recipients!");
    m_numRecipients = MIN(m_numRecipients, MAX_RECIPIENTS_PER_EVENT);

	for(u32 i=0; i < m_numRecipients; i++)
	{
		SERIALISE_OBJECTID(serialiser, m_recipientsEntityID[i], "Recipient ID");
	}	
}

void CNetworkShoutTargetPositionEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CNetworkShoutTargetPositionEvent::IsInScope(const netPlayer& player) const 
{
	if (!player.IsLocal())
	{
		// check recipients
		for(u32 i=0; i < m_numRecipients; i++)
		{
			netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_recipientsEntityID[i]);

			// send if given player owns any recipient entity
			if (pRecipientNetObject)
			{
				if (pRecipientNetObject->IsClone() && pRecipientNetObject->GetPlayerOwner() == &player)
				{
					return true;
				}
				else if (pRecipientNetObject->GetPendingPlayerOwner() && pRecipientNetObject->GetPendingPlayerOwner() == &player)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void CNetworkShoutTargetPositionEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkShoutTargetPositionEvent::Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer)
{
	netObject* pShoutingNetObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_shoutingEntityID, fromPlayer);

    // generally the shouting entity will be owned by the player who sent us this event, but check the global list if
    // we can't find it in case it migrated after the event was sent. This should be a rare occurance
    if(!pShoutingNetObject)
    {
        pShoutingNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_shoutingEntityID);
    }

	netObject* pTargetNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntityID);

	CEntity* pShoutingEntity = pShoutingNetObject ? pShoutingNetObject->GetEntity() : NULL;
	CEntity* pTargetEntity = pTargetNetObject ? pTargetNetObject->GetEntity() : NULL;

	if( pShoutingEntity && pTargetEntity )
	{
		if( pShoutingEntity->GetIsTypePed() && pTargetEntity->GetIsTypePed() )
		{
			CPed* pShoutingPed = static_cast<CPed*>(pShoutingEntity);
			CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);

			//Create the event.
			CEventShoutTargetPosition shoutTargetPositionEvent(pShoutingPed, pTargetPed);

			// Traverse recipients
			for(u32 i=0; i < m_numRecipients; i++)
			{
				// If the recipient object is locally controlled
				netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObjectFromPlayer(m_recipientsEntityID[i], toPlayer);
				if( pRecipientNetObject && !pRecipientNetObject->IsClone() )
				{
					CEntity* pRecipientEntity = pRecipientNetObject->GetEntity();
					if(pRecipientEntity && pRecipientEntity->GetIsTypePed())
					{
						CPed* pRecipientPed = static_cast<CPed*>(pRecipientEntity);

						// Add to ped intelligence of the ped
						pRecipientPed->GetPedIntelligence()->AddEvent(shoutTargetPositionEvent);
					}
				}
			}
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkShoutTargetPositionEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject* pShoutingNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_shoutingEntityID);
	if (pShoutingNetObject)
	{
		log.WriteDataValue("Shouting Entity", pShoutingNetObject->GetLogName());
	}

	netObject* pTargetNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetEntityID);
	if (pTargetNetObject)
	{
		log.WriteDataValue("Target Entity", pTargetNetObject->GetLogName());
	}

	for(u32 i=0; i < m_numRecipients; i++)
	{
		netObject* pRecipientNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_recipientsEntityID[i]);
		if (pRecipientNetObject)
		{
			log.WriteDataValue("Recipient Entity", pRecipientNetObject->GetLogName());
		}
	}
}

#endif //ENABLE_NETWORK_LOGGING


// ================================================================================================================
// PICKUP DESTROYED EVENT
// ================================================================================================================

CPickupDestroyedEvent::CPickupDestroyedEvent()
: netGameEvent(PICKUP_DESTROYED_EVENT, false)
, m_networkId(NETWORK_INVALID_OBJECT_ID)
, m_pickupPlacement(false)
, m_mapPlacement(false)
{

}

CPickupDestroyedEvent::CPickupDestroyedEvent(CPickupPlacement& placement)
: netGameEvent(PICKUP_DESTROYED_EVENT, false)
, m_networkId(NETWORK_INVALID_OBJECT_ID)
, m_pickupPlacement(true)
, m_mapPlacement(placement.GetIsMapPlacement())
{
	if (m_mapPlacement)
	{
		m_placementInfo = *placement.GetScriptInfo();
	}
	else if (AssertVerify(placement.GetNetworkObject()))
	{
		m_networkId = placement.GetNetworkObject()->GetObjectID();
	}
}

CPickupDestroyedEvent::CPickupDestroyedEvent(CPickup& pickup)
: netGameEvent(PICKUP_DESTROYED_EVENT, false)
, m_networkId(NETWORK_INVALID_OBJECT_ID)
, m_pickupPlacement(false)
, m_mapPlacement(false)
{
	m_networkId = pickup.GetNetworkObject()->GetObjectID();
}

void CPickupDestroyedEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CPickupDestroyedEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPickupDestroyedEvent::Trigger(CPickupPlacement& placement)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (placement.GetIsMapPlacement())
	{
		if (!AssertVerify(placement.GetScriptInfo()))
		{
			return;
		}
	}
	else if (!AssertVerify(placement.GetNetworkObject()))
	{
		return;
	}

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CPickupDestroyedEvent *pEvent = rage_new CPickupDestroyedEvent(placement);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

void CPickupDestroyedEvent::Trigger(CPickup& pickup)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	if (AssertVerify(pickup.GetNetworkObject()))
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CPickupDestroyedEvent *pEvent = rage_new CPickupDestroyedEvent(pickup);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

template <class Serialiser> void CPickupDestroyedEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_BOOL(serialiser, m_pickupPlacement);

	if (m_pickupPlacement)
	{
		SERIALISE_BOOL(serialiser, m_mapPlacement);

		if (m_mapPlacement)
		{
			m_placementInfo.Serialise(serialiser);
		}
		else
		{
			SERIALISE_OBJECTID(serialiser, m_networkId);
		}
	}
	else
	{
		m_mapPlacement = false;
		SERIALISE_OBJECTID(serialiser, m_networkId);
	}
}

void CPickupDestroyedEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CPickupDestroyedEvent::IsInScope(const netPlayer& player) const 
{
	if (!player.IsLocal())
	{
		if (m_mapPlacement)
		{
			scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_placementInfo.GetScriptId());

			if (pHandler && pHandler->GetNetworkComponent())
			{
				return pHandler->GetNetworkComponent()->IsPlayerAParticipant(player);
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool CPickupDestroyedEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	if (m_mapPlacement)
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_placementInfo.GetScriptId());

		if (pHandler)
		{
			scriptHandlerObject* pScriptObj = pHandler->GetScriptObject(m_placementInfo.GetObjectId());

            if(pScriptObj)
            {
			    if (pScriptObj->GetType() != SCRIPT_HANDLER_OBJECT_TYPE_PICKUP)
			    {
				    gnetAssertf(0, "CPickupDestroyedEvent::Decide: (%s) the script object (%d) is not a pickup", pHandler->GetLogName(), m_placementInfo.GetObjectId());
			    }
			    else
			    {
				    CPickupPlacement* pPlacement = SafeCast(CPickupPlacement, pScriptObj);

				    pPlacement->SetPickupHasBeenDestroyed(false);
			    }
            }
		}
	}
	else
	{
		netObject* pNetObj = NetworkInterface::GetNetworkObject(m_networkId);

		if (pNetObj)
		{
			if (m_pickupPlacement)
			{
				if (gnetVerifyf(pNetObj->GetObjectType() == NET_OBJ_TYPE_PICKUP_PLACEMENT, "CPickupDestroyedEvent::Decide: (%s) is not a pickup placement", pNetObj->GetLogName()))
				{
					CPickupPlacement* pPlacement = static_cast<CNetObjPickupPlacement*>(pNetObj)->GetPickupPlacement();

					if (pPlacement)
					{
						pPlacement->SetPickupHasBeenDestroyed(false);
					}
				}
			}
			else
			{
				if (gnetVerifyf(pNetObj->GetObjectType() == NET_OBJ_TYPE_PICKUP, "CPickupDestroyedEvent::Decide: (%s) is not a pickup", pNetObj->GetLogName()))
				{
					CPickup* pPickup = static_cast<CNetObjPickup*>(pNetObj)->GetPickup();

					if (pPickup)
					{
						pPickup->AddExplosion();

						if (!pPickup->IsFlagSet(CPickup::PF_Destroyed))
						{
							NetworkInterface::UnregisterObject(pPickup, true);
						}
					}
				}
			}
		}
	}

	return true;
}

void CPickupDestroyedEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}


bool CPickupDestroyedEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == PICKUP_DESTROYED_EVENT)
	{
		const CPickupDestroyedEvent *pickupDestroyedEvent = SafeCast(const CPickupDestroyedEvent, pEvent);

		return (m_placementInfo == pickupDestroyedEvent->m_placementInfo);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CPickupDestroyedEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Script", "%s", m_placementInfo.GetScriptId().GetLogName());

	char logName[100];
	m_placementInfo.GetLogName(logName, 100);

	log.WriteDataValue("Pickup", "%s", logName);
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// UPDATE PLAYER SCARS EVENT
// ================================================================================================================
CUpdatePlayerScarsEvent::CUpdatePlayerScarsEvent()
: netGameEvent(UPDATE_PLAYER_SCARS_EVENT, false)
{
}

void CUpdatePlayerScarsEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CUpdatePlayerScarsEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CUpdatePlayerScarsEvent::Trigger()
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CUpdatePlayerScarsEvent *pEvent = rage_new CUpdatePlayerScarsEvent;
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CUpdatePlayerScarsEvent::SerialiseEvent(datBitBuffer &messageBuffer,
                                                                         unsigned &numBloodMarks, CPedBloodNetworkData *bloodMarkData,
                                                                         unsigned &numScars,      CPedScarNetworkData  *scarDataIn)
{
	Serialiser serialiser(messageBuffer);

    const unsigned SIZE_OF_NUM_BLOOD_MARKS  = 5;
    const unsigned SIZE_OF_NUM_SCARS   		= 3;
    const unsigned SIZE_OF_UV          		= 9;
    const unsigned SIZE_OF_ROTATION    		= 8;
	const unsigned SIZE_OF_AGE		   		= 8;
    const unsigned SIZE_OF_SCALE       		= 8;
    const unsigned SIZE_OF_FORCE_FRAME 		= 8;
    const unsigned SIZE_OF_SCAR_HASH   		= 32;
	const unsigned SIZE_OF_BLOOD_HASH		= 32;
    const unsigned SIZE_OF_DAMAGE_ZONE 		= 3;

	SERIALISE_UNSIGNED(serialiser, numScars, SIZE_OF_NUM_SCARS, "Num Scars");

    for(unsigned index = 0; index < CPlayerCreationDataNode::MAX_SCARS_SYNCED; index++)
    {
        CPedScarNetworkData &scarData = scarDataIn[index];

        if(index < numScars || serialiser.GetIsMaximumSizeSerialiser())
        {
            unsigned hash = scarData.scarNameHash.GetHash();
            unsigned zone = static_cast<unsigned>(scarData.zone);

            SERIALISE_PACKED_FLOAT(serialiser, scarData.uvPos.x,          1.0f,   SIZE_OF_UV,          "Scar UV X");
            SERIALISE_PACKED_FLOAT(serialiser, scarData.uvPos.y,          1.0f,   SIZE_OF_UV,          "Scar UV X");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.rotation, 360.0f, SIZE_OF_ROTATION,    "Scar Rotation");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.age,	   512.0f, SIZE_OF_AGE,			"Scar Age");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.scale,    2.0f,   SIZE_OF_SCALE,       "Scar Scale");
            SERIALISE_INTEGER(serialiser, scarData.forceFrame,                   SIZE_OF_FORCE_FRAME, "Forced Frame");
            SERIALISE_UNSIGNED(serialiser, hash,                                 SIZE_OF_SCAR_HASH,   "Scar Hash");
            SERIALISE_UNSIGNED(serialiser, zone,                                 SIZE_OF_DAMAGE_ZONE, "Damage Zone");

            scarData.scarNameHash.SetHash(hash);
            scarData.zone = static_cast<ePedDamageZones>(zone);
        }
        else
        {
            scarData.uvPos.Zero();

	        scarData.rotation     = 0.0f;
	        scarData.scale        = 1.0f;
			scarData.age		  = 0.0f;
	        scarData.forceFrame   = -1;
	        scarData.scarNameHash = 0u;
	        scarData.zone         = kDamageZoneInvalid;
        }
    }
	
	SERIALISE_UNSIGNED(serialiser, numBloodMarks, SIZE_OF_NUM_BLOOD_MARKS, "Num Blood Marks");
	
	for(int i = 0; i < CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED; ++i)
	{
		CPedBloodNetworkData & bloodData = bloodMarkData[i];

		if(i < numBloodMarks || serialiser.GetIsMaximumSizeSerialiser())
		{
			unsigned zone = static_cast<unsigned>(bloodData.zone);
			unsigned hash = bloodData.bloodNameHash.GetHash();

			SERIALISE_PACKED_FLOAT(serialiser, bloodData.uvPos.x,          1.0f,   SIZE_OF_UV,          "Blood UV X");
            SERIALISE_PACKED_FLOAT(serialiser, bloodData.uvPos.y,          1.0f,   SIZE_OF_UV,          "Blood UV X");
			SERIALISE_UNSIGNED(serialiser, zone,                                  SIZE_OF_DAMAGE_ZONE, "Damage Zone");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.rotation, 360.0f, SIZE_OF_ROTATION,    "Blood Rotation");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.age,	    512.0f, SIZE_OF_AGE,		 "Blood Age");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.scale,    2.0f,   SIZE_OF_SCALE,       "Blood Scale");
			SERIALISE_UNSIGNED(serialiser, hash,									SIZE_OF_BLOOD_HASH,	 "Blood Name Hash");

			bloodData.zone = static_cast<ePedDamageZones>(zone);
			bloodData.bloodNameHash.SetHash(hash);
		}
		else
		{
			bloodData.age			= 0.0f;
			bloodData.bloodNameHash = 0U;
			bloodData.rotation		= 0.0f;
			bloodData.scale			= 0.0f;
			bloodData.uvPos.x		= 0.0f;
			bloodData.uvPos.y		= 0.0f;
			bloodData.zone			= kDamageZoneInvalid;
		}
	}
}

void CUpdatePlayerScarsEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    unsigned              numBloodMarks = 0;
	CPedBloodNetworkData  bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED];
    unsigned              numScars = 0;
    CPedScarNetworkData	  scarData[CPlayerCreationDataNode::MAX_SCARS_SYNCED];

    CNetObjPlayer *netObjPlayer = FindPlayerPed() ? SafeCast(CNetObjPlayer, FindPlayerPed()->GetNetworkObject()) : 0;

    if(netObjPlayer)
    {
        netObjPlayer->GetScarAndBloodData(numBloodMarks, bloodMarkData, numScars, scarData);
    }

	SerialiseEvent<CSyncDataWriter>(messageBuffer, numBloodMarks, bloodMarkData, numScars, scarData);
}

bool CUpdatePlayerScarsEvent::IsInScope(const netPlayer& player) const 
{
	if(!player.IsLocal())
	{
        return true;
	}

	return false;
}

bool CUpdatePlayerScarsEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	return true;
}

void CUpdatePlayerScarsEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
    unsigned              numBloodMarks = 0;
	CPedBloodNetworkData  bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED];
    unsigned              numScars = 0;
    CPedScarNetworkData	  scarData[CPlayerCreationDataNode::MAX_SCARS_SYNCED];

	SerialiseEvent<CSyncDataReader>(messageBuffer, numBloodMarks, bloodMarkData, numScars, scarData);

    const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(fromPlayer);

	CNetObjPlayer *netObjPlayer = player.GetPlayerPed() ? static_cast<CNetObjPlayer*>(player.GetPlayerPed()->GetNetworkObject()) : NULL;

	if(netObjPlayer)
	{
        netObjPlayer->SetScarAndBloodData(numBloodMarks, bloodMarkData, numScars, scarData);
    }
}


bool CUpdatePlayerScarsEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == UPDATE_PLAYER_SCARS_EVENT)
	{
		return true;
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CUpdatePlayerScarsEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

    /*log.WriteDataValue("Num Scars", "%d", m_NumScars);

    for(unsigned index = 0; index < m_NumScars; index++)
    {
        const CPedScarNetworkData &scarData = m_ScarData[index];

        unsigned hash = scarData.scarNameHash.GetHash();
        unsigned zone = static_cast<unsigned>(scarData.zone);

        log.WriteDataValue("Scar UV", "%.2f, %.2f", scarData.uvPos.x, scarData.uvPos.y);
        log.WriteDataValue("Scar Rot", "%.2f", scarData.rotation);
        log.WriteDataValue("Scar Age", "%.2f", scarData.age);
        log.WriteDataValue("Scar Scale", "%.2f", scarData.scale);
        log.WriteDataValue("Scar Force Frame", "%d", scarData.forceFrame);
        log.WriteDataValue("Scar Hash", "%d", hash);
        log.WriteDataValue("Scar Zone", "%d", zone);
    }

    log.WriteDataValue("Num Blood Marks", "%d", m_NumBloodMarks);

    for(int i = 0; i < m_NumBloodMarks; ++i)
	{
		const CPedBloodNetworkData &bloodData = m_BloodMarkData[i];

		unsigned zone = static_cast<unsigned>(bloodData.zone);
		unsigned hash = bloodData.bloodNameHash.GetHash();

        log.WriteDataValue("Blood UV", "%.2f, %.2f", bloodData.uvPos.x, bloodData.uvPos.y);
        log.WriteDataValue("Blood Rot", "%.2f",   bloodData.rotation);
        log.WriteDataValue("Blood Age", "%.2f",   bloodData.age);
        log.WriteDataValue("Blood Scale", "%.2f", bloodData.scale);
        log.WriteDataValue("Blood Hash", "%d", hash);
        log.WriteDataValue("Blood Zone", "%d", zone);
    }*/
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// CNetworkCheckExeSizeEvent - ask a remote player for the size of his exe and report if his size does not match the correct sizes.
// ================================================================================================================

CNetworkCheckExeSizeEvent::CNetworkCheckExeSizeEvent(const PhysicalPlayerIndex toPlayerIndex, const s64 exeSize, const bool isReply)
: netGameEvent(NETWORK_CHECK_EXE_SIZE_EVENT, true)
, m_ExeSize(exeSize)
, m_ReceivingPlayer(toPlayerIndex) 
, m_IsReply(isReply)
, m_Sku(0)
{
	if(sysAppContent::IsAmericanBuild())
		m_Sku = 2;
	else if(sysAppContent::IsJapaneseBuild())
		m_Sku = 1;
	//else	// if(sysAppContent::IsEuropeanBuild())
	//	m_Sku = 0;
}

void CNetworkCheckExeSizeEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkCheckExeSizeEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkCheckExeSizeEvent::Trigger( const PhysicalPlayerIndex toPlayerIndex, const u64 exeSize, const bool isreply )
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	CNetGamePlayer* pRemotePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(toPlayerIndex);
	if( gnetVerify(pRemotePlayer) )
	{
		if(!pRemotePlayer->IsCheatAlreadyNotified(NetworkRemoteCheaterDetector::CRC_EXE_SIZE))
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CNetworkCheckExeSizeEvent *pEvent = rage_new CNetworkCheckExeSizeEvent(toPlayerIndex, exeSize, isreply);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
}

template <class Serialiser> void CNetworkCheckExeSizeEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_INTEGER(serialiser, m_ExeSize, 64, "Exe Size");
	SERIALISE_BOOL(serialiser, m_IsReply, "Is Reply");
	SERIALISE_UNSIGNED(serialiser, m_Sku, SIZEOF_SKU, "Sku");
}

void CNetworkCheckExeSizeEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkCheckExeSizeEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkCheckExeSizeEvent::Decide(const netPlayer& fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if( AssertVerify(pLocalPlayer) )
	{
		const rlGamerId& localPlayerId = pLocalPlayer->GetRlGamerId();
		const rlGamerId& fromPlayerId  = fromPlayer.GetRlGamerId();

		u32 sku = 0;
		if(sysAppContent::IsAmericanBuild())
			sku = 2;
		else if(sysAppContent::IsJapaneseBuild())
			sku = 1;

		if( gnetVerify(localPlayerId != fromPlayerId) && m_Sku == sku )
		{
			const s64 localExeSize = (s64) NetworkInterface::GetExeSize();
			if (localExeSize != m_ExeSize)
			{
				gnetWarning("Exe sizes do not match. remote( %" I64FMT "d ) != local( %" I64FMT "d )", m_ExeSize, localExeSize);

				if (!m_IsReply)
					CNetworkCheckExeSizeEvent::Trigger( fromPlayer.GetPhysicalPlayerIndex(), localExeSize, true );

				u32 leftmost32Bits  = m_ExeSize >> 32;
				u32 rightmost32Bits = m_ExeSize & 0xffffffff;
				NetworkRemoteCheaterDetector::NotifyExeSize(NetworkInterface::GetPlayerFromGamerId(fromPlayerId), leftmost32Bits, rightmost32Bits);
			}
		}
		else if (m_Sku != sku)
		{
			gnetWarning("Sku's do not match. Ignoring exe size checks. remote(%d) != local(%d)", m_Sku, sku);
			gnetWarning("Exe sizes: remote( %" I64FMT "d ) != local( %" I64FMT "d )", m_ExeSize, (s64)NetworkInterface::GetExeSize());
		}
	}

	return true;
}

bool CNetworkCheckExeSizeEvent::IsInScope( const netPlayer& player ) const
{
	return m_ReceivingPlayer == player.GetPhysicalPlayerIndex();
}

bool CNetworkCheckExeSizeEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_CHECK_EXE_SIZE_EVENT)
	{
		const CNetworkCheckExeSizeEvent* pExeSizeEvent = SafeCast(const CNetworkCheckExeSizeEvent, pEvent);

		return (pExeSizeEvent->m_ExeSize         == m_ExeSize && 
				pExeSizeEvent->m_ReceivingPlayer == m_ReceivingPlayer && 
				pExeSizeEvent->m_IsReply         == m_IsReply);
	}

	return false;
}


#if ENABLE_NETWORK_LOGGING

void CNetworkCheckExeSizeEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
		return;

	log.WriteDataValue("Receiving Player", "%d", m_ReceivingPlayer);
	log.WriteDataValue("Executable Size", "%d", m_ExeSize);
	log.WriteDataValue("Is Reply", "%d", m_IsReply);
	log.WriteDataValue("Sku", "%d", m_Sku);
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// CNetworkCodeCRCFailedEvent - ask a remote player for any failed code CRCs and report if there's a mismatch
// ================================================================================================================

CNetworkCodeCRCFailedEvent::CNetworkCodeCRCFailedEvent(const PhysicalPlayerIndex reportedPlayerIndex, const u32 crcFailVersionAndType, const u32 crcFailAddress, const u32 crcValue)
: netGameEvent(NETWORK_CHECK_CODE_CRCS_EVENT, true)
, m_FailedCRCVersionAndType(crcFailVersionAndType)
, m_FailedCRCAddress(crcFailAddress)
, m_FailedCRCValue(crcValue)
, m_ReportedPlayer(reportedPlayerIndex) 
{
}

void CNetworkCodeCRCFailedEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkCodeCRCFailedEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkCodeCRCFailedEvent::Trigger(const u32 crcFailVersionAndType, const u32 crcFailAddress, const u32 crcValue)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	CNetGamePlayer* pRemotePlayer = NetworkInterface::GetLocalPlayer();
	if( gnetVerify(pRemotePlayer) )
	{
		PhysicalPlayerIndex reportedPlayerIndex = pRemotePlayer->GetPhysicalPlayerIndex();
		gnetDebug1("CNetworkCodeCRCFailedEvent: Triggering for player %d (V&T=0x%08x, Addr=0x%08x, Val=0x%08x)",
			(u32)reportedPlayerIndex, crcFailVersionAndType, crcFailAddress, crcValue);
		if(!pRemotePlayer->IsCheatAlreadyNotified(NetworkRemoteCheaterDetector::CRC_CODE_CRCS, crcValue))
		{
			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CNetworkCodeCRCFailedEvent *pEvent = rage_new CNetworkCodeCRCFailedEvent(reportedPlayerIndex, crcFailVersionAndType, crcFailAddress, crcValue);
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
		else
		{
			gnetDebug1("CNetworkCodeCRCFailedEvent: Cheat already notified, not re-triggering.");
		}
	}
}

template <class Serialiser> void CNetworkCodeCRCFailedEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_INTEGER(serialiser, m_FailedCRCVersionAndType, 32, "VersionAndType");
	SERIALISE_INTEGER(serialiser, m_FailedCRCAddress, 32, "Address");
	SERIALISE_INTEGER(serialiser, m_FailedCRCValue, 32, "Value");
}

void CNetworkCodeCRCFailedEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkCodeCRCFailedEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkCodeCRCFailedEvent::Decide(const netPlayer& fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if( AssertVerify(pLocalPlayer) )
	{
		const rlGamerId& localPlayerId = pLocalPlayer->GetRlGamerId();
		const rlGamerId& fromPlayerId  = fromPlayer.GetRlGamerId();

		gnetDebug1("CNetworkCodeCRCFailedEvent: Got event. VersionAndType=0x%08x, Address=0x%08x, Value=0x%08x",
			m_FailedCRCVersionAndType, m_FailedCRCAddress, m_FailedCRCValue);
		if( gnetVerify(localPlayerId != fromPlayerId) )
		{
			gnetDebug1("CNetworkCodeCRCFailedEvent: NotifyCRCFail!");
			NetworkRemoteCheaterDetector::NotifyCRCFail(NetworkInterface::GetPlayerFromGamerId(fromPlayerId), m_FailedCRCVersionAndType, m_FailedCRCAddress, m_FailedCRCValue);
		}
	}

	return true;
}

bool CNetworkCodeCRCFailedEvent::IsInScope( const netPlayer& player ) const
{
	return !player.IsLocal();
}

bool CNetworkCodeCRCFailedEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_CHECK_CODE_CRCS_EVENT)
	{
		const CNetworkCodeCRCFailedEvent* pCrcFailEvent = SafeCast(const CNetworkCodeCRCFailedEvent, pEvent);

		return (pCrcFailEvent->m_FailedCRCVersionAndType == m_FailedCRCVersionAndType &&
				pCrcFailEvent->m_FailedCRCAddress == m_FailedCRCAddress &&
				pCrcFailEvent->m_FailedCRCValue == m_FailedCRCValue &&
				pCrcFailEvent->m_ReportedPlayer == m_ReportedPlayer);
	}

	return false;
}


#if ENABLE_NETWORK_LOGGING

void CNetworkCodeCRCFailedEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
		return;

	log.WriteDataValue("Reported Player", "%d", m_ReportedPlayer);
	log.WriteDataValue("Failed CRC VersionAndType:", "%d", m_FailedCRCVersionAndType);
	log.WriteDataValue("Failed CRC Address:", "%d", m_FailedCRCAddress);
	log.WriteDataValue("Failed CRC Value:", "%d", m_FailedCRCValue);
}

#endif //ENABLE_NETWORK_LOGGING

// ================================================================================================================
// CNetworkPtFXEvent
// ================================================================================================================

const float CNetworkPtFXEvent::PTFX_CLONE_RANGE_SQ			= 100.0f * 100.0f;
const float CNetworkPtFXEvent::PTFX_CLONE_RANGE_SNIPER_SQ   = 200.0f * 200.0f;

CNetworkPtFXEvent::CNetworkPtFXEvent() 
: netGameEvent(NETWORK_PTFX_EVENT, false)
, m_PtFXHash(0)
, m_PtFXAssetHash(0)
, m_bUseEntity(false)
, m_EntityId(NETWORK_INVALID_OBJECT_ID)
, m_bUseBoneIndex(false)
, m_boneIndex(-1)
, m_FxPos(Vector3::ZeroType)
, m_FxRot(Vector3::ZeroType)
, m_Scale(1.0f)
, m_InvertAxes(0)
, m_bHasColor(false)
, m_r(0)
, m_g(0)
, m_b(0)
, m_bHasAlpha(false)
, m_alpha(0.f)
, m_ignoreScopeChecks(false)
{

}

CNetworkPtFXEvent::CNetworkPtFXEvent(atHashWithStringNotFinal ptfxhash, atHashWithStringNotFinal ptfxassethash, const CEntity* pEntity, int boneIndex, const Vector3& vfxpos, const Vector3& vfxrot, const float& scale, const u8& invertAxes, const bool& bHasColor, const u8& r, const u8& g, const u8& b, const bool& bHasAlpha, const float& alpha, bool ignoreScopeChecks)
: netGameEvent(NETWORK_PTFX_EVENT, false)
, m_PtFXHash((u32)ptfxhash)
, m_PtFXAssetHash((u32)ptfxassethash)
, m_bUseEntity(false)
, m_EntityId(pEntity ? NetworkUtils::GetNetworkObjectFromEntity(pEntity)->GetObjectID() : NETWORK_INVALID_OBJECT_ID)
, m_bUseBoneIndex(false)
, m_boneIndex(boneIndex)
, m_FxPos(vfxpos)
, m_FxRot(vfxrot)
, m_Scale(scale)
, m_InvertAxes(invertAxes)
, m_bHasColor(bHasColor)
, m_r(r)
, m_g(g)
, m_b(b)
, m_bHasAlpha(bHasAlpha)
, m_alpha(alpha)
, m_ignoreScopeChecks(ignoreScopeChecks)
{
	m_bUseEntity = pEntity ? true : false;
	m_bUseBoneIndex = (boneIndex != -1) ? true : false;
}

void CNetworkPtFXEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkPtFXEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkPtFXEvent::Trigger(atHashWithStringNotFinal ptfxhash, atHashWithStringNotFinal ptfxassethash, const CEntity* pEntity, int boneIndex, const Vector3& vfxpos, const Vector3& vfxrot, const float& scale, const u8& invertAxes, const float& r, const float& g, const float& b, const float& alpha, bool ignoreScopeChanges)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	if (NetworkInterface::GetEventManager().CheckForSpaceInPool(false))
	{
		bool bHasColor = (r >= 0.f);
		bool bHasAlpha = (alpha >= 0.f);

		u8 ur = 0, ug = 0, ub = 0;
		if (bHasColor)
		{
			Color32 color(r,g,b);
			ur = color.GetRed();
			ug = color.GetGreen();
			ub = color.GetBlue();
		}

		CNetworkPtFXEvent *pEvent = rage_new CNetworkPtFXEvent(ptfxhash, ptfxassethash, pEntity, boneIndex, vfxpos, vfxrot, scale, invertAxes, bHasColor, ur, ug, ub, bHasAlpha, alpha, ignoreScopeChanges);
	
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
	else
	{
		gnetAssertf(0, "Warning - network event pool full, dumping ptfx event");
	}
}

template <class Serialiser> void CNetworkPtFXEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	const unsigned SIZEOF_PTFX_HASH         = 32;
	const unsigned SIZEOF_PTFX_ASSET_HASH   = 32;
	const unsigned SIZEOF_PTFX_SCALE        = 10;
	const unsigned SIZEOF_PTFX_COLOUR		= 8;
	const unsigned SIZEOF_PTFX_ALPHA		= 8;
	const unsigned SIZEOF_INVERT_AXES_FLAGS = 3;  // 1 bit for each axis inverted
	const unsigned SIZEOF_BONE_INDEX		= 32;
	const float    MAX_PTFX_SCALE			= 10.0f;

	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_PtFXHash,      SIZEOF_PTFX_HASH, "PTFX Hash");
	SERIALISE_UNSIGNED(serialiser, m_PtFXAssetHash, SIZEOF_PTFX_ASSET_HASH, "PTFX Asset Hash");
	SERIALISE_POSITION(serialiser, m_FxPos, "Position");
	SERIALISE_POSITION(serialiser, m_FxRot, "Rotation"); // Rotation stored in Vector3 so can be synced the same way as a position
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Scale, MAX_PTFX_SCALE, SIZEOF_PTFX_SCALE, "Scale");
	SERIALISE_UNSIGNED(serialiser, m_InvertAxes, SIZEOF_INVERT_AXES_FLAGS, "Invert Axes Flags");

	SERIALISE_BOOL(serialiser, m_bUseEntity, "Using Entity");
	if(m_bUseEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_EntityId, "Entity");
	}
	else
	{
		m_EntityId = NETWORK_INVALID_OBJECT_ID;
	}

	SERIALISE_BOOL(serialiser, m_bUseBoneIndex, "Using Bone Index");
	if(m_bUseBoneIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_boneIndex, SIZEOF_BONE_INDEX, "Bone Index");
	}
	else
	{
		m_boneIndex = -1;
	}

	SERIALISE_BOOL(serialiser, m_bHasColor, "Has Color");
	if(m_bHasColor || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_r, SIZEOF_PTFX_COLOUR, "Red");
		SERIALISE_UNSIGNED(serialiser, m_g, SIZEOF_PTFX_COLOUR, "Green");
		SERIALISE_UNSIGNED(serialiser, m_b, SIZEOF_PTFX_COLOUR, "Blue");
	}
	else
	{
		m_r = 0;
		m_g = 0;
		m_b = 0;
	}

	SERIALISE_BOOL(serialiser, m_bHasAlpha, "Has Alpha");
	if(m_bHasAlpha || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_alpha, 1.0f, SIZEOF_PTFX_ALPHA, "Alpha");
	}
	else
	{
		m_alpha = -1.0f;
	}
}

void CNetworkPtFXEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkPtFXEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkPtFXEvent::Decide(const netPlayer& UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	// Skip this if we go in a cutscene by the time we receive this (if it passed isInScope checks on the owner)
	if(NetworkInterface::IsInMPCutscene())
	{
		return true;
	}

	CEntity* pEntity = NULL;
	if (m_EntityId != NETWORK_INVALID_OBJECT_ID)
	{
		netObject* pNetObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityId);
		if (pNetObject && pNetObject->GetEntity())
		{
			pEntity = pNetObject->GetEntity();
		}
	}

	//-----------

	atHashWithStringNotFinal ptfxhash(m_PtFXHash);
	atHashWithStringNotFinal ptfxassethash(m_PtFXAssetHash);

	//-----------

	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(m_PtFXAssetHash);
	if (slot.IsValid())
	{
		if (!ptfxManager::GetAssetStore().HasObjectLoaded(slot))
		{
			if (!ptfxManager::GetAssetStore().IsObjectLoading(slot))
				ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_PRIORITY_LOAD);

			return false;
		}
	}

	//-----------

	if (m_bHasColor)
	{
		Color32 color(m_r, m_g, m_b);
		ptfxScript::SetTriggeredColourTint(color.GetRedf(),color.GetGreenf(),color.GetBluef());

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketTriggeredScriptPtFxColour>(CPacketTriggeredScriptPtFxColour(color.GetRedf(),color.GetGreenf(),color.GetBluef()));
		}
#endif
	}

	if (m_bHasAlpha)
	{
		ptfxScript::SetTriggeredAlphaTint(m_alpha);
	}

	//-----------

	ptfxScript::Trigger(m_PtFXHash, m_PtFXAssetHash, pEntity, m_boneIndex, RC_VEC3V(m_FxPos), RC_VEC3V(m_FxRot), m_Scale, m_InvertAxes);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketTriggeredScriptPtFx>(
			CPacketTriggeredScriptPtFx(m_PtFXHash, m_PtFXAssetHash, m_boneIndex, m_FxPos, m_FxRot, m_Scale, m_InvertAxes),
			pEntity,
			true);
	}
#endif
	//-----------

	return true;
}

bool CNetworkPtFXEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if(player.IsLocal())
		return false;

	if (SafeCast(const CNetGamePlayer, &player)->IsInDifferentTutorialSession())
	{
		// ignore players in different tutorial sessions
		return false;
	}
	
	CEntity* pEntity = NULL;
	if (m_bUseEntity)
	{
		netObject *networkObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityId);

		if(networkObject)
		{
			pEntity = networkObject->GetEntity();

			if (!networkObject->IsClone() && !networkObject->HasBeenCloned(player))
			{
				return false;
			}
		}
	}

	// don't send to players far away from the ptfx or that are in a cutscene - if the ptfxpos is a world position
	if ((!m_bUseEntity || pEntity) && player.GetPlayerPed())
	{
		if (player.GetPlayerInfo() && player.GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		{
			// ignore players in cutscenes
			aiDebugf2("Frame %i: failed to trigger CNetworkPtFXEvent because player %s is in an mp cutscene", fwTimer::GetFrameCount(), player.GetLogName());
			return false;
		}

		CNetObjPlayer* playerNetObj = SafeCast(CNetObjPlayer, player.GetPlayerPed()->GetNetworkObject());
		CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		CNetObjPlayer* myPlayerNetObj = pLocalPlayer ? SafeCast(CNetObjPlayer, pLocalPlayer->GetNetworkObject()) : NULL;

		// if we are being spectated by this player, create all ptfx on his machine
		if (playerNetObj && playerNetObj->IsSpectating() && playerNetObj->GetSpectatorObjectId() == myPlayerNetObj->GetObjectID())
		{
			return true;
		}

		if (m_ignoreScopeChecks)
		{
			return true;
		}

		Vector3 playerFocusPosition = NetworkInterface::GetPlayerFocusPosition(playerToCheck);

		gnetAssertf(!playerFocusPosition.IsClose(VEC3_ZERO, 0.1f),"Expect a valid focus position for player %s",playerToCheck.GetLogName());

		Vector3 ptfxPosition = m_bUseEntity && pEntity ? (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) + m_FxPos) : m_FxPos;

		float distanceFromptfxSq = (playerFocusPosition - ptfxPosition).Mag2();

		float ptfxCloneRange = PTFX_CLONE_RANGE_SQ;

		grcViewport *viewport = NetworkUtils::GetNetworkPlayerViewPort(static_cast<const CNetGamePlayer&>(playerToCheck));

		if (viewport && viewport->GetFOVY() < 100.f)
		{
			ptfxCloneRange = PTFX_CLONE_RANGE_SNIPER_SQ;
		}

		if (distanceFromptfxSq <= ptfxCloneRange)
		{
			return true;
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkPtFXEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
		return;

	log.WriteDataValue("FX Hash", "%u", m_PtFXHash);
	log.WriteDataValue("FX Asset Hash", "%u", m_PtFXAssetHash);
	log.WriteDataValue("FX Position", "%f, %f, %f", m_FxPos.x, m_FxPos.y, m_FxPos.z);
	log.WriteDataValue("FX Rotation", "%f, %f, %f", m_FxRot.x, m_FxRot.y, m_FxRot.z);
	log.WriteDataValue("Entity Id", "%d", m_EntityId);
	log.WriteDataValue("Bone Index", "%d", m_boneIndex);
	log.WriteDataValue("Scale", "%f", m_Scale);
	log.WriteDataValue("Invert Axes", "%d", m_InvertAxes);
}

#endif //ENABLE_NETWORK_LOGGING


// ===========================================================================================================
// PED SEEN DEAD PED EVENT
// ===========================================================================================================

CNetworkPedSeenDeadPedEvent::CNetworkPedSeenDeadPedEvent() :
netGameEvent( NETWORK_PED_SEEN_DEAD_PED_EVENT, false, false ),
m_numDeadSeen(0)
{

}

void CNetworkPedSeenDeadPedEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkPedSeenDeadPedEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

bool CNetworkPedSeenDeadPedEvent::AddSightingToEvent(const ObjectId& deadPedId, const ObjectId& finderPedId)
{
	// Check for duplicates first
	for(u32 i=0; i < m_numDeadSeen; i++)
	{
		if(m_allSightings[i].m_DeadPedId == deadPedId && 
		   m_allSightings[i].m_FindingPedId == finderPedId)
		{
			return true;
		}
	}

	// Add if there is space
	if(m_numDeadSeen < MAX_SYNCED_SIGHTINGS)
	{
		m_allSightings[m_numDeadSeen].m_DeadPedId = deadPedId;
		m_allSightings[m_numDeadSeen].m_FindingPedId = finderPedId;
		m_numDeadSeen++;
		return true;
	}

	return false;
}

void CNetworkPedSeenDeadPedEvent::Trigger(const CPed* pDeadPed, const CPed* pFindingPed)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());
	if (!pDeadPed->GetNetworkObject())
	{
		aiDebugf2("Frame %i: failed to trigger CEventPedSeenDeadPed because pDeadPed->GetNetworkObject() is null", fwTimer::GetFrameCount());
		return;
	}

	if (!pFindingPed->GetNetworkObject())
	{
		aiDebugf2("Frame %i: failed to trigger CEventPedSeenDeadPed because pFindingPed->GetNetworkObject() is null", fwTimer::GetFrameCount());
		return;
	}

	const ObjectId& deadPedId = pDeadPed->GetNetworkObject()->GetObjectID();
	const ObjectId& finderPedId = pFindingPed->GetNetworkObject()->GetObjectID();

	bool addedToExistingEvent = false;
	// Search for any existing events on the queue and try to append recipient
	atDNode<netGameEvent*, datBase>* pNode = NetworkInterface::GetEventManager().GetEventListHead();
	while(pNode)
	{
		netGameEvent* pNetworkEvent = pNode->Data;
		if( pNetworkEvent && pNetworkEvent->GetEventType() == NETWORK_PED_SEEN_DEAD_PED_EVENT && !pNetworkEvent->IsFlaggedForRemoval() && !pNetworkEvent->HasBeenSent())
		{
			CNetworkPedSeenDeadPedEvent* seenDeadPedEvent = static_cast<CNetworkPedSeenDeadPedEvent*>(pNetworkEvent);
			if(seenDeadPedEvent->AddSightingToEvent(deadPedId, finderPedId))
			{
				addedToExistingEvent = true;
				break;
			}
		}
		pNode = pNode->GetNext();
	}

	if(!addedToExistingEvent)
	{
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CNetworkPedSeenDeadPedEvent *pEvent = rage_new CNetworkPedSeenDeadPedEvent();
		pEvent->AddSightingToEvent(deadPedId, finderPedId);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

bool CNetworkPedSeenDeadPedEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if(player.IsLocal())
	{
		return false;
	}

	if (player.GetPlayerInfo() && player.GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
	{
		// ignore players in cutscenes
		aiDebugf2("Frame %i: failed to trigger CEventPedSeenDeadPed because player %s is in an mp cutscene", fwTimer::GetFrameCount(), player.GetLogName());
		return false;
	}

	if (player.IsInDifferentTutorialSession())
	{
		// ignore players in different tutorial sessions
		aiDebugf2("Frame %i: failed to trigger CEventPedSeenDeadPed because player %s is in a different tutorial session", fwTimer::GetFrameCount(), player.GetLogName());
		return false;
	}

	return true;
}

template <class Serialiser> void CNetworkPedSeenDeadPedEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_numDeadSeen, SIZEOF_NUM_SIGHTINGS, "Number of Dead Ped Sightings");

	static const unsigned BUFFER_SIZE = 64;
	char buf[BUFFER_SIZE];
	for(u32 i=0; i < m_numDeadSeen; i++)
	{
		formatf(buf, BUFFER_SIZE, "Dead Ped %d ID", i);
		SERIALISE_OBJECTID(serialiser, m_allSightings[i].m_DeadPedId, buf);
		formatf(buf, BUFFER_SIZE, "Finding Ped %d ID", i);
		SERIALISE_OBJECTID(serialiser, m_allSightings[i].m_FindingPedId, buf);
	}
	for(u32 i=m_numDeadSeen; i < MAX_SYNCED_SIGHTINGS; i++)
	{
		m_allSightings[i].m_DeadPedId = NETWORK_INVALID_OBJECT_ID;
		m_allSightings[i].m_FindingPedId = NETWORK_INVALID_OBJECT_ID;
	}
}

void CNetworkPedSeenDeadPedEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkPedSeenDeadPedEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkPedSeenDeadPedEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	for(u32 i = 0; i < m_numDeadSeen; i++)
	{
		netObject* pDeadPedObj = NetworkInterface::GetNetworkObject(m_allSightings[i].m_DeadPedId);
		netObject* pFindingPedObj = NetworkInterface::GetNetworkObject(m_allSightings[i].m_FindingPedId);
		CPed* pDeadPed = pDeadPedObj ? NetworkUtils::GetPedFromNetworkObject(pDeadPedObj) : NULL;
		CPed* pFindingPed = pFindingPedObj ? NetworkUtils::GetPedFromNetworkObject(pFindingPedObj) : NULL;

		if (pDeadPed && pFindingPed)
		{
			aiDebugf2("Frame %i: Adding CEventPedSeenDeadPed to script ai group ped %s has found dead ped %s", fwTimer::GetFrameCount(), pDeadPed->GetLogName(), pFindingPed->GetLogName());
			GetEventScriptAIGroup()->Add(CEventPedSeenDeadPed(*pDeadPed, *pFindingPed));
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CNetworkPedSeenDeadPedEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	static const unsigned BUFFER_SIZE = 64;
	char buf[BUFFER_SIZE];
	for(u32 i = 0; i < m_numDeadSeen; i++)
	{
		netObject* pDeadPedObj = NetworkInterface::GetNetworkObject(m_allSightings[i].m_DeadPedId);
		netObject* pFindingPedObj = NetworkInterface::GetNetworkObject(m_allSightings[i].m_FindingPedId);
		formatf(buf, BUFFER_SIZE, "Dead Ped %d", i);
		log.WriteDataValue(buf, "%s", pDeadPedObj ? pDeadPedObj->GetLogName() : "Doesn't exist");
		formatf(buf, BUFFER_SIZE, "Finding ped %d", i);
		log.WriteDataValue(buf, "%s", pFindingPedObj ? pFindingPedObj->GetLogName() : "Doesn't exist");
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// REMOVE STICKY BOMB EVENT
// ===========================================================================================================

CRemoveStickyBombEvent::CRemoveStickyBombEvent() :
netGameEvent( REMOVE_STICKY_BOMB_EVENT, false, false ),
m_numStickyBombs(0)
{
}

CRemoveStickyBombEvent::CRemoveStickyBombEvent(const CNetFXIdentifier &netIdentifier) :
netGameEvent( REMOVE_STICKY_BOMB_EVENT, false, false ),
m_numStickyBombs(0)
{
	AddStickyBomb(netIdentifier);
}

bool CRemoveStickyBombEvent::AddStickyBomb(const CNetFXIdentifier &netIdentifier)
{
	bool bAdded = false;

	if (m_numStickyBombs < MAX_STICKY_BOMBS)
	{
		if (m_numStickyBombs == 0 ||  m_netIdentifiers[0].GetPlayerOwner() == netIdentifier.GetPlayerOwner())
		{
			m_netIdentifiers[m_numStickyBombs++] = netIdentifier;
			bAdded = true;
		}
	}

	return bAdded;
}

void CRemoveStickyBombEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRemoveStickyBombEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemoveStickyBombEvent::Trigger(const CNetFXIdentifier &netIdentifier)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	gnetAssert(NetworkInterface::IsGameInProgress());

	// see if we can add this sticky bomb to an existing event
	atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

	while (node)
	{
		netGameEvent *networkEvent = node->Data;

		if (networkEvent && !networkEvent->HasBeenSent() && networkEvent->GetEventType() == REMOVE_STICKY_BOMB_EVENT && !networkEvent->IsFlaggedForRemoval())
		{
			CRemoveStickyBombEvent *removeStickyBombEvent = static_cast<CRemoveStickyBombEvent *>(networkEvent);

			if (removeStickyBombEvent->AddStickyBomb(netIdentifier))
			{
				return;
			}
		}

		node = node->GetNext();
	}

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CRemoveStickyBombEvent *pEvent = rage_new CRemoveStickyBombEvent(netIdentifier);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CRemoveStickyBombEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if(player.IsLocal())
	{
		return false;
	}

	if (AssertVerify(m_numStickyBombs > 0))
	{
		if (m_netIdentifiers[0].GetPlayerOwner() == ((const CNetGamePlayer&)playerToCheck).GetPhysicalPlayerIndex())
		{
			return true;
		}
	}

	return false;
}

template <class Serialiser> void CRemoveStickyBombEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	static const int SIZEOF_NUM_STICKYBOMBS = datBitsNeeded<MAX_STICKY_BOMBS>::COUNT;

	SERIALISE_UNSIGNED(serialiser, m_numStickyBombs, SIZEOF_NUM_STICKYBOMBS);

    gnetAssertf(m_numStickyBombs <= MAX_STICKY_BOMBS, "Received a remove sticky bomb event with too many sticky bombs!");
    m_numStickyBombs = MIN(m_numStickyBombs, MAX_STICKY_BOMBS);

	for (u32 i=0; i < m_numStickyBombs; i++)
	{
		m_netIdentifiers[i].Serialise<Serialiser>(messageBuffer);
	}
}

void CRemoveStickyBombEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemoveStickyBombEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRemoveStickyBombEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &toPlayer)
{
	for (u32 i=0; i < m_numStickyBombs; i++)
	{
		m_netIdentifiers[i].SetPlayerOwner(((const CNetGamePlayer&)toPlayer).GetPhysicalPlayerIndex());

		CProjectile* pProjectile = CProjectileManager::GetExistingNetSyncProjectile(m_netIdentifiers[i]);

		if (pProjectile && AssertVerify(!pProjectile->GetNetworkIdentifier().IsClone()))
		{
			CProjectileManager::RemoveNetSyncProjectile(pProjectile);
		}
	}

	return true;
}

#if ENABLE_NETWORK_LOGGING

void CRemoveStickyBombEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	for (u32 i=0; i < m_numStickyBombs; i++)
	{
		log.WriteDataValue("FX id", "%d", m_netIdentifiers[i].GetFXId());
	}
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// INFORM SILENCED GUNSHOT EVENT
// ===========================================================================================================

CInformSilencedGunShotEvent::CInformSilencedGunShotEvent() :
netGameEvent( INFORM_SILENCED_GUNSHOT_EVENT, false, false )
, m_TargetPlayerID(INVALID_PLAYER_INDEX)
, m_FiringPedID(NETWORK_INVALID_OBJECT_ID)
, m_ShotPos(VEC3_ZERO)
, m_ShotTarget(VEC3_ZERO)
, m_WeaponHash(0)
, m_MustBeSeenInVehicle(false)
{
}

CInformSilencedGunShotEvent::CInformSilencedGunShotEvent(CPed *targetPed, CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle) :
netGameEvent( INFORM_SILENCED_GUNSHOT_EVENT, false, false )
, m_TargetPlayerID(INVALID_PLAYER_INDEX)
, m_FiringPedID(NETWORK_INVALID_OBJECT_ID)
, m_ShotPos(shotPos)       
, m_ShotTarget(shotTarget)
, m_WeaponHash(weaponHash)
, m_MustBeSeenInVehicle(mustBeSeenInVehicle)
{
    if(targetPed && targetPed->GetNetworkObject())
    {
        m_TargetPlayerID = targetPed->GetNetworkObject()->GetPhysicalPlayerIndex();
    }

    if(firingPed && firingPed->GetNetworkObject())
    {
        m_FiringPedID = firingPed->GetNetworkObject()->GetObjectID();
    }
}

void CInformSilencedGunShotEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CInformSilencedGunShotEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CInformSilencedGunShotEvent::Trigger(CPed *targetPed, CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    gnetAssert(NetworkInterface::IsGameInProgress());

    // see if we are already informing this player about this gunshot for another ped
    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        netGameEvent *networkEvent = node->Data;

        if (networkEvent && !networkEvent->HasBeenSent() && networkEvent->GetEventType() == INFORM_SILENCED_GUNSHOT_EVENT && !networkEvent->IsFlaggedForRemoval())
        {
            CInformSilencedGunShotEvent *silencedGunShotEvent = static_cast<CInformSilencedGunShotEvent *>(networkEvent);

            if(silencedGunShotEvent)
            {
                PhysicalPlayerIndex playerIndex = (targetPed && targetPed->GetNetworkObject()) ? targetPed->GetNetworkObject()->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;
                ObjectId            firingPedID = (firingPed && firingPed->GetNetworkObject()) ? firingPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

                if(playerIndex         == silencedGunShotEvent->m_TargetPlayerID &&
                   firingPedID         == silencedGunShotEvent->m_FiringPedID    &&
                   weaponHash          == silencedGunShotEvent->m_WeaponHash     &&
                   mustBeSeenInVehicle == silencedGunShotEvent->m_MustBeSeenInVehicle)
                {
                    const float epsilon = 0.01f;

                    if(shotPos.IsClose   (silencedGunShotEvent->m_ShotPos,    epsilon)  &&
                       shotTarget.IsClose(silencedGunShotEvent->m_ShotTarget, epsilon))
                    {
                        return;
                    }
                }
            }
        }

        node = node->GetNext();
    }

    unsigned            timeLastSentToPlayer = 0;
    PhysicalPlayerIndex playerIndex          = (targetPed && targetPed->GetNetworkObject()) ? targetPed->GetNetworkObject()->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;

    if(playerIndex != INVALID_PLAYER_INDEX)
    {
        timeLastSentToPlayer = NetworkInterface::GetEventManager().GetTimeSinceEventSentToPlayer(INFORM_SILENCED_GUNSHOT_EVENT, playerIndex);
    }

    const unsigned MIN_TIME_BETWEEN_EVENTS = 500;

    if(timeLastSentToPlayer > MIN_TIME_BETWEEN_EVENTS)
    {
        NetworkInterface::GetEventManager().CheckForSpaceInPool();
        CInformSilencedGunShotEvent *pEvent = rage_new CInformSilencedGunShotEvent(targetPed, firingPed, shotPos, shotTarget, weaponHash, mustBeSeenInVehicle);
        NetworkInterface::GetEventManager().PrepareEvent(pEvent);
    }
}

bool CInformSilencedGunShotEvent::IsInScope(const netPlayer& player) const
{
    if(player.IsLocal())
    {
        return false;
    }

    return m_TargetPlayerID == player.GetPhysicalPlayerIndex();
}

template <class Serialiser> void CInformSilencedGunShotEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    const unsigned SIZEOF_WEAPON_HASH = 32;

    Serialiser serialiser(messageBuffer);

    SERIALISE_OBJECTID(serialiser, m_FiringPedID, "Firing Ped");
    SERIALISE_POSITION(serialiser, m_ShotPos,     "Shot Position");
    SERIALISE_POSITION(serialiser, m_ShotTarget,  "Shot Target");
    SERIALISE_UNSIGNED(serialiser, m_WeaponHash,  SIZEOF_WEAPON_HASH, "Weapon Hash");
    SERIALISE_BOOL    (serialiser, m_MustBeSeenInVehicle, "Must Be Seen In Vehicle");
}

void CInformSilencedGunShotEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CInformSilencedGunShotEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CInformSilencedGunShotEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	weaponDebugf3("CInformSilencedGunShotEvent::Decide");

    netObject *networkObject = NetworkInterface::GetNetworkObject(m_FiringPedID);
    CPed      *ped           = NetworkUtils::GetPedFromNetworkObject(networkObject);

    CNetObjPed::OnInformSilencedGunShot(ped, m_ShotPos, m_ShotTarget, m_WeaponHash, m_MustBeSeenInVehicle);

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CInformSilencedGunShotEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    netObject *networkObject = NetworkInterface::GetNetworkObject(m_FiringPedID);
    log.WriteDataValue("Firing Ped",  "%s", networkObject ? networkObject->GetLogName() : "Unknown");
    log.WriteDataValue("Shot Pos",    "%.2f, %.2f, %.2f", m_ShotPos.x,    m_ShotPos.y,    m_ShotPos.z);
    log.WriteDataValue("Shot Target", "%.2f, %.2f, %.2f", m_ShotTarget.x, m_ShotTarget.y, m_ShotTarget.z);
    log.WriteDataValue("Weapon Hash", "%d", m_WeaponHash);
    log.WriteDataValue("Must Be Seen In Vehicle", "%s", m_MustBeSeenInVehicle ? "TRUE" : "FALSE");
}

#endif //ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// PED PLAY PAIN EVENT
// ===========================================================================================================
CPedPlayPainEvent::CPedPlayPainEvent() :
netGameEvent(PED_PLAY_PAIN_EVENT, false, false)
, m_NumPlayPainData(0)
{
}

CPedPlayPainEvent::CPedPlayPainEvent(const CPed *ped, int damageReason, float rawDamage) :
netGameEvent( PED_PLAY_PAIN_EVENT, false, false )
, m_NumPlayPainData(0)
{
    AddPlayPainData(ped, damageReason, rawDamage);
}

void CPedPlayPainEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CPedPlayPainEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CPedPlayPainEvent::Trigger(const CPed *ped, int damageReason, float rawDamage)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    // check all of the events in the pool to find any existing events this can be packed into
    CPedPlayPainEvent *eventToUse = 0;

    atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

    while (node)
    {
        netGameEvent *networkEvent = node->Data;

        if(networkEvent && networkEvent->GetEventType() == PED_PLAY_PAIN_EVENT && !networkEvent->IsFlaggedForRemoval())
        {
            CPedPlayPainEvent *playPainEvent = static_cast<CPedPlayPainEvent *>(networkEvent);

            if(playPainEvent->m_NumPlayPainData < MAX_NUM_PLAY_PAIN_DATA && !playPainEvent->HasBeenSent())
            {
                eventToUse = playPainEvent;
            }
        }

        node = node->GetNext();
    }

    if(eventToUse)
    {
        eventToUse->AddPlayPainData(ped, damageReason, rawDamage);
    }
    else
    {
        gnetAssert(NetworkInterface::IsGameInProgress());
        NetworkInterface::GetEventManager().CheckForSpaceInPool();
        CPedPlayPainEvent *pEvent = rage_new CPedPlayPainEvent(ped, damageReason, rawDamage);
        NetworkInterface::GetEventManager().PrepareEvent(pEvent);
    }
}

bool CPedPlayPainEvent::IsInScope( const netPlayer& playerToCheck ) const
{
    static const float RANGE_SQ = rage::square(100.0f);

    bool inScope = false;

    if(playerToCheck.IsRemote() && !playerToCheck.IsBot())
    {
        // send the event to any players with at least one of the packed play pain requests in scope - this
        // keeps things simpler - sounds is attentuated by distance so remote players won't hear audio from very far away peds
        for(unsigned index = 0; index < MAX_NUM_PLAY_PAIN_DATA; index++)
        {
            CPed* ped = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_PlayPainData[index].m_TargetPedID));

            if(ped)
            {
                // don't send to players far away from the taunt
                const CNetGamePlayer& gamePlayer = static_cast<const CNetGamePlayer&>(playerToCheck);
                if (gamePlayer.GetPlayerPed())
                {
                    //Check if we are spectating
                    CNetObjPlayer* playerNetObj = SafeCast(CNetObjPlayer, gamePlayer.GetPlayerPed()->GetNetworkObject());
                    CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
                    CNetObjPlayer* myPlayerNetObj = pLocalPlayer ? SafeCast(CNetObjPlayer, pLocalPlayer->GetNetworkObject()) : NULL;

                    // if we are being spectated by this player, always allow speech to be heard on his machine
                    if (playerNetObj && playerNetObj->IsSpectating() && playerNetObj->GetSpectatorObjectId() == myPlayerNetObj->GetObjectID())
                    {
                        return true;
                    }

                    float distanceSq = DistSquared(ped->GetTransform().GetPosition(), gamePlayer.GetPlayerPed()->GetTransform().GetPosition()).Getf();

                    inScope = (distanceSq <= RANGE_SQ);
                }
            }
        }
    }

    return inScope;
}

template <class Serialiser> void CPedPlayPainEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
    Serialiser serialiser(messageBuffer);

    SERIALISE_UNSIGNED(serialiser, m_NumPlayPainData, SIZEOF_NUM_PLAY_PAIN_DATA, "Num Play Pain Data");

    gnetAssertf(m_NumPlayPainData <= MAX_NUM_PLAY_PAIN_DATA, "Received a ped play pain event with too much data!");
    m_NumPlayPainData = MIN(m_NumPlayPainData, MAX_NUM_PLAY_PAIN_DATA);

    for(unsigned index = 0; index < m_NumPlayPainData; index++)
    {
        const unsigned SIZEOF_DAMAGE_REASON = datBitsNeeded<AUD_NUM_DAMAGE_REASONS>::COUNT;
        const unsigned SIZEOF_RAW_DAMAGE    = 8;
        const float    MAX_RAW_DAMAGE       = 201.0f; // value derived from max allowable value for (g_HealthLostForHighPain + 1.0f) - actual value is lower

        SERIALISE_OBJECTID(serialiser, m_PlayPainData[index].m_TargetPedID, "Target Ped ID");
        SERIALISE_UNSIGNED(serialiser, m_PlayPainData[index].m_DamageReason, SIZEOF_DAMAGE_REASON, "Damage");
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_PlayPainData[index].m_RawDamage, MAX_RAW_DAMAGE, SIZEOF_RAW_DAMAGE, "Raw Damage");
    }
}

void CPedPlayPainEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
    SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CPedPlayPainEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CPedPlayPainEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
    for(unsigned index = 0; index < m_NumPlayPainData; index++)
    {
        CPed *ped = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_PlayPainData[index].m_TargetPedID));

        if(ped && ped->GetSpeechAudioEntity())
        {
            audDamageStats stats;
            stats.DamageReason = (audDamageReason)m_PlayPainData[index].m_DamageReason;
            stats.RawDamage    = m_PlayPainData[index].m_RawDamage;

            //don't run velocity safety check for script-triggered falling, as sometimes they have crazy falls where the ped's initial
            //velocity is actually upwards
            if(stats.DamageReason == AUD_DAMAGE_REASON_FALLING || stats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING)
            {
                ped->GetSpeechAudioEntity()->SetBlockFallingVelocityCheck();
            }

            ped->GetSpeechAudioEntity()->InflictPain(stats);
        }
    }

    return true;
}

#if ENABLE_NETWORK_LOGGING

void CPedPlayPainEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
    netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
    netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

    if(!log.IsEnabled())
    {
        return;
    }

    for(unsigned index = 0; index < m_NumPlayPainData; index++)
    {
        netObject *networkObject = NetworkInterface::GetNetworkObject(m_PlayPainData[index].m_TargetPedID);

        if(networkObject)
        {
            log.WriteDataValue("Target Ped", "%s", networkObject->GetLogName());
        }
        else
        {
            log.WriteDataValue("Target Ped ID", "%d", m_PlayPainData[index].m_TargetPedID);
        }

        log.WriteDataValue("Damage Reason", "%d",   m_PlayPainData[index].m_DamageReason);
        log.WriteDataValue("Raw Damage",    "%.2f", m_PlayPainData[index].m_RawDamage);
    }
}

#endif //ENABLE_NETWORK_LOGGING

void CPedPlayPainEvent::AddPlayPainData(const CPed *ped, int damageReason, float rawDamage)
{
    if(gnetVerifyf(m_NumPlayPainData < MAX_NUM_PLAY_PAIN_DATA, "No space to add new play pain data!"))
    {
        if(gnetVerifyf(ped,                     "Trying to add play pain data for an invalid ped!") &&
           gnetVerifyf(ped->GetNetworkObject(), "Trying to add play pain data for a non-networked ped!"))
        {
            m_PlayPainData[m_NumPlayPainData].m_TargetPedID  = ped->GetNetworkObject()->GetObjectID();
            m_PlayPainData[m_NumPlayPainData].m_DamageReason = damageReason;
            m_PlayPainData[m_NumPlayPainData].m_RawDamage    = rawDamage;

            m_NumPlayPainData++;
        }
    }
}

// ===========================================================================================================
// CACHE_PLAYER_HEAD_BLEND_DATA_EVENT
// ===========================================================================================================
CCachePlayerHeadBlendDataEvent::CCachePlayerHeadBlendDataEvent() :
netGameEvent(CACHE_PLAYER_HEAD_BLEND_DATA_EVENT, false, true)
, m_pPlayerToBroadcastTo(NULL)
{
}

CCachePlayerHeadBlendDataEvent::CCachePlayerHeadBlendDataEvent(const CPedHeadBlendData& headBlendData, const netPlayer *pPlayerToBroadcastTo) :
netGameEvent( CACHE_PLAYER_HEAD_BLEND_DATA_EVENT, false, true )
, m_headBlendData(headBlendData)
, m_pPlayerToBroadcastTo(pPlayerToBroadcastTo)
{
}

void CCachePlayerHeadBlendDataEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CCachePlayerHeadBlendDataEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CCachePlayerHeadBlendDataEvent::Trigger(const CPedHeadBlendData& headBlendData, const netPlayer *pPlayerToBroadcastTo)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	// check all of the events in the pool to find any existing events to remove
	atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

	while (node)
	{
		netGameEvent *networkEvent = node->Data;

		if(networkEvent && networkEvent->GetEventType() == CACHE_PLAYER_HEAD_BLEND_DATA_EVENT && !networkEvent->IsFlaggedForRemoval())
		{
			// destroy any previous events for this script
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetEventManager().GetLog(), "DESTROYING_EVENT", "%s_%d\r\n", NetworkInterface::GetEventManager().GetEventNameFromType(networkEvent->GetEventType()), networkEvent->GetEventId().GetID());
			networkEvent->GetEventId().ClearPlayersInScope();
		}

		node = node->GetNext();
	}

	gnetAssert(NetworkInterface::IsGameInProgress());
	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CCachePlayerHeadBlendDataEvent *pEvent = rage_new CCachePlayerHeadBlendDataEvent(headBlendData, pPlayerToBroadcastTo);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CCachePlayerHeadBlendDataEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	if (m_pPlayerToBroadcastTo)
	{
		return (m_pPlayerToBroadcastTo == &playerToCheck);
	}
	else
	{
		return (playerToCheck.IsRemote() && !playerToCheck.IsBot());
	}
}

template <class Serialiser> void CCachePlayerHeadBlendDataEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	m_headBlendData.Serialise(serialiser);
}

void CCachePlayerHeadBlendDataEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CCachePlayerHeadBlendDataEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CCachePlayerHeadBlendDataEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	if (fromPlayer.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		NetworkInterface::CachePlayerHeadBlendData(fromPlayer.GetPhysicalPlayerIndex(), m_headBlendData);
		return true;
	}

	return false;
}

// ===========================================================================================================
// REMOVE_PED_FROM_PEDGROUP_EVENT 
// ===========================================================================================================
CRemovePedFromPedGroupEvent::CRemovePedFromPedGroupEvent() :
netGameEvent(REMOVE_PED_FROM_PEDGROUP_EVENT, false, false),
m_PedId(NETWORK_INVALID_OBJECT_ID)
{
}

CRemovePedFromPedGroupEvent::CRemovePedFromPedGroupEvent(ObjectId pedId, const CGameScriptObjInfo& pedScriptInfo) :
netGameEvent( REMOVE_PED_FROM_PEDGROUP_EVENT, false, false )
, m_PedId(pedId)
, m_PedScriptInfo(pedScriptInfo)
{
}

CRemovePedFromPedGroupEvent::~CRemovePedFromPedGroupEvent()
{
	// the group might have migrated locally so make sure he is still removed
	TryToRemovePed(NULL);
}

void CRemovePedFromPedGroupEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CRemovePedFromPedGroupEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CRemovePedFromPedGroupEvent::Trigger(const CPed& ped)
{
	if (ped.GetPedsGroup() && gnetVerify(NetworkInterface::IsGameInProgress()))
	{
		const CScriptEntityExtension* pScriptExtension = ped.GetExtension<CScriptEntityExtension>();

		if (AssertVerify(pScriptExtension) && AssertVerify(pScriptExtension->GetScriptInfo()) && AssertVerify(ped.GetNetworkObject()))
		{
			USE_MEMBUCKET(MEMBUCKET_NETWORK);

			NetworkInterface::GetEventManager().CheckForSpaceInPool();
			CRemovePedFromPedGroupEvent *pEvent = rage_new CRemovePedFromPedGroupEvent(ped.GetNetworkObject()->GetObjectID(), *pScriptExtension->GetScriptInfo());
			NetworkInterface::GetEventManager().PrepareEvent(pEvent);
		}
	}
}

bool CRemovePedFromPedGroupEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	if (playerToCheck.IsRemote())
	{
		netObject* pPedObj = NetworkInterface::GetNetworkObject(m_PedId);

		if (pPedObj && pPedObj->GetEntity() && pPedObj->GetEntity()->GetIsTypePed())
		{
			CPedGroup* pPedGroup =  static_cast<CPed*>(pPedObj->GetEntity())->GetPedsGroup();

			if (pPedGroup)
			{
				if (pPedGroup->IsPlayerGroup())
				{
					return (pPedGroup->GetGroupIndex() == playerToCheck.GetPhysicalPlayerIndex());
				}
				else
				{
					CPedGroupsArrayHandler* pPedGroupsArrayHandler = NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler();

					if (pPedGroupsArrayHandler)
					{
						return &playerToCheck == pPedGroupsArrayHandler->GetElementArbitration(pPedGroup->GetNetArrayHandlerIndex());
					}
				}
			}
		}
	}

	return false;
}

template <class Serialiser> void CRemovePedFromPedGroupEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_PedId, "Ped id");
	m_PedScriptInfo.Serialise(serialiser);
}

void CRemovePedFromPedGroupEvent::Prepare ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CRemovePedFromPedGroupEvent::Handle ( datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CRemovePedFromPedGroupEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pPedObj = NetworkInterface::GetNetworkObject(m_PedId);

	if (pPedObj && pPedObj->GetEntity() && pPedObj->GetEntity()->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPedObj->GetEntity());

		const CScriptEntityExtension* pScriptExtension = pPed->GetExtension<CScriptEntityExtension>();

		if (pScriptExtension && pScriptExtension->GetScriptInfo() && *pScriptExtension->GetScriptInfo() == m_PedScriptInfo)
		{
			return TryToRemovePed(pPed);
		}
	}

	return true;
}

bool CRemovePedFromPedGroupEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == REMOVE_PED_FROM_PEDGROUP_EVENT)
	{
		const CRemovePedFromPedGroupEvent* removePedFromPedGroupEvent = SafeCast(const CRemovePedFromPedGroupEvent, pEvent);

		return (removePedFromPedGroupEvent->m_PedScriptInfo == m_PedScriptInfo);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING
void CRemovePedFromPedGroupEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	log.WriteDataValue("Ped", "PED_%d", m_PedId);
	log.WriteDataValue("Script", "%s", m_PedScriptInfo.GetScriptId().GetLogName());
}
#endif

bool CRemovePedFromPedGroupEvent::TryToRemovePed(CPed* pPed)
{
	if (!pPed)
	{
		netObject* pPedObj = NetworkInterface::GetNetworkObject(m_PedId);

		if (pPedObj && pPedObj->GetEntity() && pPedObj->GetEntity()->GetIsTypePed())
		{
			pPed = static_cast<CPed*>(pPedObj->GetEntity());
		}
	}

	if (pPed)
	{
		CPedGroup* pPedGroup = pPed->GetPedsGroup();

		if (pPedGroup)
		{
			if (pPedGroup->IsPlayerGroup())
			{
				if (pPedGroup->IsLocallyControlled())
				{
					pPedGroup->GetGroupMembership()->RemoveMember(pPed);
					pPedGroup->Process();
				}
			}
			else
			{
				CPedGroupsArrayHandler* pPedGroupsArrayHandler = NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler();

				if (pPedGroupsArrayHandler && pPedGroupsArrayHandler->IsElementLocallyArbitrated(pPedGroup->GetNetArrayHandlerIndex()))
				{
					pPedGroup->GetGroupMembership()->RemoveMember(pPed);
					pPedGroup->Process();
					return true;
				}
				else
				{
					// ignore the event if the group has migrated - the sender will find this out and send the event to the new owner
					return false;
				}
			}
		}
	}

	return true;
}

// ===========================================================================================================
// REPORT_MYSELF_EVENT
// ===========================================================================================================
CReportMyselfEvent::CReportMyselfEvent() 
	: netGameEvent(REPORT_MYSELF_EVENT, false, false)
	, m_report(0xFFFF)
	, m_reportParam(0)
{
}

CReportMyselfEvent::CReportMyselfEvent(const u32 report, const u32 reportParam) 
	: netGameEvent(REPORT_MYSELF_EVENT, false, false)
	, m_report(report)
	, m_reportParam(reportParam)
{

#if __ASSERT
	switch (m_report)
	{
	case NetworkRemoteCheaterDetector::CODE_TAMPERING:
	case NetworkRemoteCheaterDetector::TELEMETRY_BLOCK:
#if RSG_PC
	case NetworkRemoteCheaterDetector::GAME_SERVER_CASH_BANK:
	case NetworkRemoteCheaterDetector::GAME_SERVER_CASH_WALLET:
	case NetworkRemoteCheaterDetector::GAME_SERVER_INVENTORY:
	case NetworkRemoteCheaterDetector::GAME_SERVER_SERVER_INTEGRITY:
#endif // RSG_PC
		break;

	default:
		gnetAssertf(0, "Invalid report '%u' received.", m_report);
		break;
	}
#endif
}

CReportMyselfEvent::~CReportMyselfEvent()
{
}

void CReportMyselfEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
    CReportMyselfEvent networkEvent;
    NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

template <class Serialiser> void CReportMyselfEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_report, SIZEOF_REPORT, "Report");
	SERIALISE_UNSIGNED(serialiser, m_reportParam, SIZEOF_REPORT, "Report Param");
}

void CReportMyselfEvent::Trigger(const u32 report, const u32 reportParam)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CReportMyselfEvent *pEvent = rage_new CReportMyselfEvent(report, reportParam);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CReportMyselfEvent::IsInScope(const netPlayer& playerToCheck) const
{
	if (!playerToCheck.IsMyPlayer())
	{
		return true;
	}

    return false;
}

void CReportMyselfEvent::Prepare ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CReportMyselfEvent::Handle ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CReportMyselfEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	switch (m_report)
	{
	case NetworkRemoteCheaterDetector::CODE_TAMPERING:
		NetworkRemoteCheaterDetector::NotifyTampering(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)), m_reportParam);
		break;

	case NetworkRemoteCheaterDetector::TELEMETRY_BLOCK: 
		NetworkRemoteCheaterDetector::NotifyTelemetryBlock(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)));
		break;

#if RSG_PC
	case NetworkRemoteCheaterDetector::GAME_SERVER_CASH_WALLET:
		NetworkRemoteCheaterDetector::NotifyGameServerCashWallet(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)), m_reportParam);
		break;

	case NetworkRemoteCheaterDetector::GAME_SERVER_CASH_BANK:
		NetworkRemoteCheaterDetector::NotifyGameServerCashBank(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)), m_reportParam);
		break;

	case NetworkRemoteCheaterDetector::GAME_SERVER_INVENTORY:
		NetworkRemoteCheaterDetector::NotifyGameServerInventory(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)), m_reportParam);
		break;

	case NetworkRemoteCheaterDetector::GAME_SERVER_SERVER_INTEGRITY:
		NetworkRemoteCheaterDetector::NotifyGameServerIntegrity(SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer)), m_reportParam);
		break;
#endif // RSG_PC

	default:
		gnetAssertf(0, "Invalid report '%u' received.", m_report);
		break;
	}

    return true;
}

bool CReportMyselfEvent::operator==(const netGameEvent* pEvent) const
{
    // we only need one of these at a time
    if (pEvent->GetEventType() == REPORT_MYSELF_EVENT)
    {
		const CReportMyselfEvent* pPeportEvent = SafeCast(const CReportMyselfEvent, pEvent);

		if (pPeportEvent->m_report      == m_report &&
			pPeportEvent->m_reportParam == m_reportParam)
		{
			return true;
		}
    }

    return false;
}
#if ENABLE_NETWORK_LOGGING
void CReportMyselfEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool eventLogOnly, datBitBuffer* UNUSED_PARAM(messageBuffer)) const
{
	netLoggingInterface &log = eventLogOnly ? netInterface::GetEventManager().GetLog() : netInterface::GetEventManager().GetLogSplitter();
	if(!log.IsEnabled())
		return;

	const char* report = "UNKNOWN";
	switch (m_report)
	{
	case NetworkRemoteCheaterDetector::CODE_TAMPERING:  report = "CODE_TAMPERING";  break;
	case NetworkRemoteCheaterDetector::TELEMETRY_BLOCK: report = "TELEMETRY_BLOCK"; break; 
		break;
	default:
		break;
	}

	log.WriteDataValue("ReportMyself", "%s", report);
	log.WriteDataValue("Parameter", "%d", m_reportParam);
}
#endif // ENABLE_NETWORK_LOGGING

// ===========================================================================================================
// REPORT_CASH_SPAWN_EVENT
// ===========================================================================================================
CReportCashSpawnEvent::CReportCashSpawnEvent() :
	netGameEvent(REPORT_CASH_SPAWN_EVENT, false, false)
{
}

CReportCashSpawnEvent::CReportCashSpawnEvent(u64 uid, int amount, int hash, ActivePlayerIndex toPlayer) 
	: netGameEvent(REPORT_CASH_SPAWN_EVENT, false, false)
	, m_uid(uid)
	, m_amount(amount)
	, m_hash(hash)
	, m_toPlayer(toPlayer)
{

}

CReportCashSpawnEvent::~CReportCashSpawnEvent()
{
}

void CReportCashSpawnEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CReportCashSpawnEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CReportCashSpawnEvent::Trigger(u64 uid, int amount, int hash, ActivePlayerIndex toPlayer)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	NetworkInterface::GetEventManager().CheckForSpaceInPool( );
	CReportCashSpawnEvent *pEvent = rage_new CReportCashSpawnEvent(uid, amount, hash, toPlayer);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

bool CReportCashSpawnEvent::IsInScope( const netPlayer& playerToCheck ) const
{
	return (playerToCheck.GetActivePlayerIndex() == m_toPlayer);
}

void CReportCashSpawnEvent::Prepare ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer) )
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CReportCashSpawnEvent::Handle ( datBitBuffer& messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

template <class Serialiser> void CReportCashSpawnEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_UNSIGNED(serialiser, m_uid, 64, "uid");
	SERIALISE_INTEGER(serialiser, m_amount, 32, "amount");
	SERIALISE_INTEGER(serialiser, m_hash, 32, "hash");
}

bool CReportCashSpawnEvent::Decide(const netPlayer &fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	CNetGamePlayer* player = SafeCast(CNetGamePlayer, const_cast<netPlayer *>(&fromPlayer));
	if(gnetVerifyf(player, "Invalid NetGamePlayer"))
	{
		const rlGamerHandle& gamer = player->GetGamerInfo().GetGamerHandle();
		CNetworkTelemetry::AppendCashCreated(m_uid, gamer, m_amount, m_hash);
		return true;
	}
	return false;
}

// ================================================================================================================
// BLOCK WEAPON SELECTION EVENT
// ================================================================================================================
CBlockWeaponSelectionEvent::CBlockWeaponSelectionEvent() 
	: netGameEvent(BLOCK_WEAPON_SELECTION, false, false)
	, m_VehicleId(NETWORK_INVALID_OBJECT_ID)
	, m_BlockWeapon(false)
{

}

CBlockWeaponSelectionEvent::CBlockWeaponSelectionEvent(ObjectId vehicleId, bool blockWeapon) 
	: netGameEvent(BLOCK_WEAPON_SELECTION, false, false)
	, m_VehicleId(vehicleId)
	, m_BlockWeapon(blockWeapon)
{
}

CBlockWeaponSelectionEvent::~CBlockWeaponSelectionEvent()
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_VehicleId);
	if(pObject && !pObject->IsClone())
	{
		CNetObjVehicle* pNetObject = SafeCast(CNetObjVehicle, pObject);
		CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);

		if (pVehicle)
		{
			pVehicle->GetIntelligence()->BlockWeaponSelection(m_BlockWeapon);
		}
	}
}

void CBlockWeaponSelectionEvent::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CBlockWeaponSelectionEvent networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CBlockWeaponSelectionEvent::Trigger(CVehicle& vehicle, bool blockWeapon)
{
	if (AssertVerify(vehicle.GetNetworkObject()))
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		gnetAssert(NetworkInterface::IsGameInProgress());
		NetworkInterface::GetEventManager().CheckForSpaceInPool();
		CBlockWeaponSelectionEvent *pEvent = rage_new CBlockWeaponSelectionEvent(vehicle.GetNetworkObject()->GetObjectID(), blockWeapon);
		NetworkInterface::GetEventManager().PrepareEvent(pEvent);
	}
}

template <class Serialiser> void CBlockWeaponSelectionEvent::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);

	SERIALISE_OBJECTID(serialiser, m_VehicleId, "Vehicle");
	SERIALISE_BOOL(serialiser, m_BlockWeapon, "Block Weapon");
}

void CBlockWeaponSelectionEvent::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

bool CBlockWeaponSelectionEvent::IsInScope(const netPlayer& player) const 
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_VehicleId);
	if(!pObject)
	{
		gnetAssertf(0, "CBlockWeaponSelectionEvent - Invalid vehicle ID supplied!");
		return false;
	}

	return pObject->GetPlayerOwner() == &player;
}

void CBlockWeaponSelectionEvent::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CBlockWeaponSelectionEvent::Decide(const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	netObject* pObject = NetworkInterface::GetNetworkObject(m_VehicleId);
	if(pObject)
	{
		CNetObjVehicle* pNetObject = SafeCast(CNetObjVehicle, pObject);
		CVehicle* pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObject);

		if (pVehicle)
		{
				pVehicle->GetIntelligence()->BlockWeaponSelection(m_BlockWeapon);
		}
	}
	return true;
}

bool CBlockWeaponSelectionEvent::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == BLOCK_WEAPON_SELECTION)
	{
		const CBlockWeaponSelectionEvent *activateVehicleSpecialEvent = SafeCast(const CBlockWeaponSelectionEvent, pEvent);

		return (activateVehicleSpecialEvent->m_VehicleId == m_VehicleId && activateVehicleSpecialEvent->m_BlockWeapon == m_BlockWeapon);
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING

void CBlockWeaponSelectionEvent::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
	{
		return;
	}

	netObject *vehicleObject = NetworkInterface::GetObjectManager().GetNetworkObject(m_VehicleId);

	if (vehicleObject)
	{
		log.WriteDataValue("Vehicle", vehicleObject->GetLogName());
		log.WriteDataValue("Block Weapon", "%s", m_BlockWeapon ? "TRUE" : "FALSE");
	}
}

#endif //ENABLE_NETWORK_LOGGING

#if RSG_PC

// ================================================================================================================
// CNetworkCheckCatalogCrc - Check the catalog crc of another player.
// ================================================================================================================

CNetworkCheckCatalogCrc::CNetworkCheckCatalogCrc(const bool isReply)
	: netGameEvent(NETWORK_CHECK_CATALOG_CRC, true)
	, m_IsReply(isReply)
	, m_crc(netCatalog::GetInstance().GetCRC())
	, m_version(netCatalog::GetInstance().GetVersion())
{
	gnetAssert(netCatalog::GetInstance().IsLatestVersion());
}

void CNetworkCheckCatalogCrc::EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence)
{
	CNetworkCheckCatalogCrc networkEvent;
	NetworkInterface::GetEventManager().HandleEvent(&networkEvent, msgBuffer, fromPlayer, toPlayer, messageSeq, eventID, eventIDSequence);
}

void CNetworkCheckCatalogCrc::Trigger(const bool isreply)
{
	USE_MEMBUCKET(MEMBUCKET_NETWORK);

	NetworkInterface::GetEventManager().CheckForSpaceInPool();
	CNetworkCheckCatalogCrc *pEvent = rage_new CNetworkCheckCatalogCrc(isreply);
	NetworkInterface::GetEventManager().PrepareEvent(pEvent);
}

template <class Serialiser> void CNetworkCheckCatalogCrc::SerialiseEvent(datBitBuffer &messageBuffer)
{
	Serialiser serialiser(messageBuffer);
	SERIALISE_UNSIGNED(serialiser, m_crc, 32, "crc");
	SERIALISE_UNSIGNED(serialiser, m_version, SIZEOF_VERSION, "version");
	SERIALISE_BOOL(serialiser, m_IsReply, "Is Reply");
}

void CNetworkCheckCatalogCrc::Prepare(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataWriter>(messageBuffer);
}

void CNetworkCheckCatalogCrc::Handle(datBitBuffer &messageBuffer, const netPlayer &UNUSED_PARAM(fromPlayer), const netPlayer &UNUSED_PARAM(toPlayer))
{
	SerialiseEvent<CSyncDataReader>(messageBuffer);
}

bool CNetworkCheckCatalogCrc::Decide(const netPlayer& fromPlayer, const netPlayer &UNUSED_PARAM(toPlayer))
{
	const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	if( AssertVerify(pLocalPlayer) )
	{
		const rlGamerId& localPlayerId = pLocalPlayer->GetRlGamerId();
		const rlGamerId& fromPlayerId  = fromPlayer.GetRlGamerId();

		if(gnetVerify(localPlayerId != fromPlayerId))
		{
			const u32 localCrc = netCatalog::GetInstance().GetCRC();
			const u32 localVersion = netCatalog::GetInstance().GetVersion();

			if (localCrc != m_crc && localVersion == m_version)
			{
				gnetWarning("crc's do not match. remote(%u) != local(%u), version='%u'", m_crc, localCrc, m_version);

				bool flagRefresh = false;
				const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CATALOG_CRC_CHECK", 0x8bfa927d), flagRefresh);

				if (flagRefresh || !tuneExists)
				{
					if (!m_IsReply)
						CNetworkCheckCatalogCrc::Trigger(true);

					//TODO: Refresh Catalog...? What else - telemetry report?
					GameTransactionSessionMgr::Get().FlagCatalogForRefresh();
				}
			}
		}
	}

	return true;
}

bool CNetworkCheckCatalogCrc::IsInScope( const netPlayer& player ) const
{
	return (!player.IsMyPlayer());
}

bool CNetworkCheckCatalogCrc::operator==(const netGameEvent* pEvent) const
{
	if (pEvent->GetEventType() == NETWORK_CHECK_CATALOG_CRC)
	{
		const CNetworkCheckCatalogCrc* pExeSizeEvent = SafeCast(const CNetworkCheckCatalogCrc, pEvent);

		return (pExeSizeEvent->m_crc == m_crc 
				&& pExeSizeEvent->m_version == m_version 
				&& pExeSizeEvent->m_IsReply == m_IsReply);
	}

	return false;
}


#if ENABLE_NETWORK_LOGGING

void CNetworkCheckCatalogCrc::WriteEventToLogFile(bool UNUSED_PARAM(wasSent), bool LOGGING_ONLY(bEventLogOnly), datBitBuffer *UNUSED_PARAM(pMessageBuffer)) const
{
	netLogSplitter logSplitter(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
	netLoggingInterface &log = bEventLogOnly ? NetworkInterface::GetEventManager().GetLog() : logSplitter;

	if(!log.IsEnabled())
		return;

	log.WriteDataValue("CRC value", "%d", m_crc);
	log.WriteDataValue("Is Reply", "%d", m_IsReply);
}

#endif //ENABLE_NETWORK_LOGGING

#endif // RSG_PC


static int SortByNumberDescending(const CNetworkEventsDumpInfo::eventDumpDetails* lhs, const CNetworkEventsDumpInfo::eventDumpDetails* rhs)
{
	return rhs->count - lhs->count;
}

atFixedArray<CNetworkEventsDumpInfo::eventDumpDetails, netGameEvent::NETEVENTTYPE_MAXTYPES> CNetworkEventsDumpInfo::ms_eventTypesCount;
atBinaryMap<CNetworkEventsDumpInfo::eventDumpDetails, u32> CNetworkEventsDumpInfo::ms_scriptedEvents;

void CNetworkEventsDumpInfo::UpdateNetworkEventsCrashDumpData()
{
	netGameEvent::Pool *pEventPool = netGameEvent::GetPool();
	if (pEventPool)
	{
		atDNode<netGameEvent*, datBase> *node = NetworkInterface::GetEventManager().GetEventListHead();

		ms_scriptedEvents.Reset();
		ms_eventTypesCount.Resize(netGameEvent::NETEVENTTYPE_MAXTYPES);
		for(int i=0; i<netGameEvent::NETEVENTTYPE_MAXTYPES; i++)
		{
			ms_eventTypesCount[i].count = 0;
			ms_eventTypesCount[i].eventName = nullptr;
		}

		while (node)
		{
			netGameEvent* pEvent = node->Data;

			if (pEvent)
			{
				ms_eventTypesCount[pEvent->GetEventType()].count++;
				ms_eventTypesCount[pEvent->GetEventType()].eventName = pEvent->GetEventName();
				if (pEvent->GetEventType() == SCRIPTED_GAME_EVENT)
				{
					CScriptedGameEvent* pScriptEvent = SafeCast(CScriptedGameEvent, pEvent);
					if (pScriptEvent)
					{
						if (ms_scriptedEvents.Has(pScriptEvent->GetScriptId().GetScriptIdHash()))
						{
							eventDumpDetails* eventDetails = ms_scriptedEvents.SafeGet(pScriptEvent->GetScriptId().GetScriptIdHash());
							if (eventDetails)
								eventDetails->count++;
						}
						else
						{
							eventDumpDetails eventDetails;
							eventDetails.eventName = pScriptEvent->GetScriptId().GetScriptName();
							eventDetails.count = 1;
							ms_scriptedEvents.Insert(pScriptEvent->GetScriptId().GetScriptIdHash(), eventDetails);
							ms_scriptedEvents.FinishInsertion();
						}							
					}
				}
			}

			node = node->GetNext();
		}

		ms_eventTypesCount.QSort(0, -1, SortByNumberDescending);	
	}
}

//-----------------------------------------------------------------------
//
//-----------------------------------------------------------------------

namespace NetworkEventTypes
{
    void RegisterNetworkEvents()
    {
        // register the frame network events
        netInterface::RegisterNetGameEvents();

		NetworkInterface::GetEventManager().RegisterNetworkEvent(SCRIPT_ARRAY_DATA_VERIFY_EVENT,         CScriptArrayDataVerifyEvent::EventHandler,     "SCRIPT_ARRAY_DATA_VERIFY_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REQUEST_CONTROL_EVENT,                  CRequestControlEvent::EventHandler,            "REQUEST_CONTROL_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(GIVE_CONTROL_EVENT,                     CGiveControlEvent::EventHandler,               "GIVE_CONTROL_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(WEAPON_DAMAGE_EVENT,                    CWeaponDamageEvent::EventHandler,              "WEAPON_DAMAGE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REQUEST_PICKUP_EVENT,                   CRequestPickupEvent::EventHandler,             "REQUEST_PICKUP_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REQUEST_MAP_PICKUP_EVENT,               CRequestMapPickupEvent::EventHandler,          "REQUEST_MAP_PICKUP_EVENT");
#if !__FINAL
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CLOCK_EVENT,						     CDebugGameClockEvent::EventHandler,		    "GAME_CLOCK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(WEATHER_EVENT,						     CDebugGameWeatherEvent::EventHandler,			"GAME_WEATHER_EVENT");
#endif
		NetworkInterface::GetEventManager().RegisterNetworkEvent(RESPAWN_PLAYER_PED_EVENT,               CRespawnPlayerPedEvent::EventHandler,          "RESPAWN_PLAYER_PED_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(GIVE_WEAPON_EVENT,                      CGiveWeaponEvent::EventHandler,                "GIVE_WEAPON_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOVE_WEAPON_EVENT,                    CRemoveWeaponEvent::EventHandler,              "REMOVE_WEAPON_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOVE_ALL_WEAPONS_EVENT,               CRemoveAllWeaponsEvent::EventHandler,          "REMOVE_ALL_WEAPONS_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(VEHICLE_COMPONENT_CONTROL_EVENT,        CVehicleComponentControlEvent::EventHandler,   "VEHICLE_COMPONENT_CONTROL_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(FIRE_EVENT,						     CFireEvent::EventHandler,					    "FIRE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(EXPLOSION_EVENT,					     CExplosionEvent::EventHandler,				    "EXPLOSION_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(START_PROJECTILE_EVENT,                 CStartProjectileEvent::EventHandler,           "START_PROJECTILE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(UPDATE_PROJECTILE_TARGET_EVENT,		 CUpdateProjectileTargetEntity::EventHandler,	"UPDATE_PROJECTILE_TARGET_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(BREAK_PROJECTILE_TARGET_LOCK_EVENT,	 CBreakProjectileTargetLock::EventHandler,		"BREAK_PROJECTILE_TARGET_LOCK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOVE_PROJECTILE_ENTITY_EVENT,		 CRemoveProjectileEntity::EventHandler,			"REMOVE_PROJECTILE_ENTITY_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(ALTER_WANTED_LEVEL_EVENT,               CAlterWantedLevelEvent::EventHandler,          "ALTER_WANTED_LEVEL_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CHANGE_RADIO_STATION_EVENT,             CChangeRadioStationEvent::EventHandler,        "CHANGE_RADIO_STATION_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(RAGDOLL_REQUEST_EVENT,                  CRagdollRequestEvent::EventHandler,            "RAGDOLL_REQUEST_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(PLAYER_TAUNT_EVENT,                     CPlayerTauntEvent::EventHandler,               "PLAYER_TAUNT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(PLAYER_CARD_STAT_EVENT,			     CPlayerCardStatEvent::EventHandler,            "PLAYER_CARD_STAT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(DOOR_BREAK_EVENT,                       CDoorBreakEvent::EventHandler,                 "DOOR_BREAK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(SCRIPTED_GAME_EVENT,                    CScriptedGameEvent::EventHandler,              "SCRIPTED_GAME_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOTE_SCRIPT_INFO_EVENT,               CRemoteScriptInfoEvent::EventHandler,          "REMOTE_SCRIPT_INFO_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOTE_SCRIPT_LEAVE_EVENT,              CRemoteScriptLeaveEvent::EventHandler,         "REMOTE_SCRIPT_LEAVE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(MARK_AS_NO_LONGER_NEEDED_EVENT,         CMarkAsNoLongerNeededEvent::EventHandler,      "MARK_AS_NO_LONGER_NEEDED_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CONVERT_TO_SCRIPT_ENTITY_EVENT,         CConvertToScriptEntityEvent::EventHandler,     "CONVERT_TO_SCRIPT_ENTITY_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(SCRIPT_WORLD_STATE_EVENT,               CScriptWorldStateEvent::EventHandler,          "SCRIPT_WORLD_STATE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(INCIDENT_ENTITY_EVENT,			         CIncidentEntityEvent::EventHandler,		    "INCIDENT_ENTITY_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CLEAR_AREA_EVENT,				         CClearAreaEvent::EventHandler,			        "CLEAR_AREA_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CLEAR_RECTANGLE_AREA_EVENT,		     CClearRectangleAreaEvent::EventHandler,	    "CLEAR_RECTANGLE_AREA_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_REQUEST_SYNCED_SCENE_EVENT,     CRequestNetworkSyncedSceneEvent::EventHandler, "NETWORK_REQUEST_SYNCED_SCENE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_START_SYNCED_SCENE_EVENT,       CStartNetworkSyncedSceneEvent::EventHandler,   "NETWORK_START_SYNCED_SCENE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_UPDATE_SYNCED_SCENE_EVENT,      CUpdateNetworkSyncedSceneEvent::EventHandler,  "NETWORK_UPDATE_SYNCED_SCENE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_STOP_SYNCED_SCENE_EVENT,        CStopNetworkSyncedSceneEvent::EventHandler,    "NETWORK_STOP_SYNCED_SCENE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT,   CGivePedScriptedTaskEvent::EventHandler,       "GIVE_PED_SCRIPTED_TASK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT,   CGivePedSequenceTaskEvent::EventHandler,       "GIVE_PED_SEQUENCE_TASK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CLEAR_PED_TASKS_EVENT,          CClearPedTasksEvent::EventHandler,             "NETWORK_CLEAR_PED_TASKS_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_START_PED_ARREST_EVENT,         CStartNetworkPedArrestEvent::EventHandler,     "NETWORK_START_PED_ARREST_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_START_PED_UNCUFF_EVENT,         CStartNetworkPedUncuffEvent::EventHandler,     "NETWORK_START_PED_UNCUFF_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CAR_HORN_EVENT,			     CCarHornEvent::EventHandler,				    "NETWORK_SOUND_CAR_HORN_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_ENTITY_AREA_STATUS_EVENT,	     CEntityAreaStatusEvent::EventHandler,		    "NETWORK_ENTITY_AREA_STATUS_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_GARAGE_OCCUPIED_STATUS_EVENT,   CGarageOccupiedStatusEvent::EventHandler,	    "NETWORK_GARAGE_OCCUPIED_STATUS_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(PED_CONVERSATION_LINE_EVENT,            CPedConversationLineEvent::EventHandler,       "PED_CONVERSATION_LINE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(SCRIPT_ENTITY_STATE_CHANGE_EVENT,       CScriptEntityStateChangeEvent::EventHandler,   "SCRIPT_ENTITY_STATE_CHANGE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_PLAY_SOUND_EVENT,               CPlaySoundEvent::EventHandler,                 "NETWORK_PLAY_SOUND_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_STOP_SOUND_EVENT,               CStopSoundEvent::EventHandler,                 "NETWORK_STOP_SOUND_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT,     CPlayAirDefenseFireEvent::EventHandler,        "NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_BANK_REQUEST_EVENT,             CAudioBankRequestEvent::EventHandler,          "NETWORK_BANK_REQUEST_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_AUDIO_BARK_EVENT,				 CAudioBarkingEvent::EventHandler,				"NETWORK_AUDIO_BARK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REQUEST_DOOR_EVENT,				     CRequestDoorEvent::EventHandler,			    "REQUEST_DOOR_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_TRAIN_REQUEST_EVENT,		     CNetworkTrainRequestEvent::EventHandler,	    "NETWORK_TRAIN_REQUEST_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_TRAIN_REPORT_EVENT,		     CNetworkTrainReportEvent::EventHandler,	    "NETWORK_TRAIN_REPORT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_INCREMENT_STAT_EVENT,		     CNetworkIncrementStatEvent::EventHandler,      "NETWORK_INCREMENT_STAT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(MODIFY_VEHICLE_LOCK_WORD_STATE_DATA,    CModifyVehicleLockWorldStateDataEvent::EventHandler,	"MODIFY_VEHICLE_LOCK_WORD_STATE_DATA");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(MODIFY_PTFX_WORD_STATE_DATA_SCRIPTED_EVOLVE_EVENT, CModifyPtFXWorldStateDataScriptedEvolveEvent::EventHandler, "MODIFY_PTFX_WORD_STATE_DATA_SCRIPTED_EVOLVE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_REQUEST_PHONE_EXPLOSION_EVENT,  CRequestPhoneExplosionEvent::EventHandler,	    "REQUEST_PHONE_EXPLOSION_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_REQUEST_DETACHMENT_EVENT,       CRequestDetachmentEvent::EventHandler,	        "REQUEST_DETACHMENT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_KICK_VOTES_EVENT,               CSendKickVotesEvent::EventHandler,	            "KICK_VOTES_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_GIVE_PICKUP_REWARDS_EVENT,      CGivePickupRewardsEvent::EventHandler,	        "GIVE_PICKUP_REWARDS_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CRC_HASH_CHECK_EVENT,    	     CNetworkCrcHashCheckEvent::EventHandler,	    "NETWORK_CRC_HASH_CHECK_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(BLOW_UP_VEHICLE_EVENT,    			     CBlowUpVehicleEvent::EventHandler,			    "BLOW_UP_VEHICLE_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON,   CNetworkSpecialFireEquippedWeaponEvent::EventHandler, "NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_RESPONDED_TO_THREAT_EVENT,      CNetworkRespondedToThreatEvent::EventHandler,  "NETWORK_RESPONDED_TO_THREAT_EVENT");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_SHOUT_TARGET_POSITION_EVENT,    CNetworkShoutTargetPositionEvent::EventHandler,"NETWORK_SHOUT_TARGET_POSITION");
		NetworkInterface::GetEventManager().RegisterNetworkEvent(VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT, CVoiceDrivenMouthMovementFinishedEvent::EventHandler, "VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(PICKUP_DESTROYED_EVENT,                 CPickupDestroyedEvent::EventHandler,           "PICKUP_DESTROYED_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(UPDATE_PLAYER_SCARS_EVENT,              CUpdatePlayerScarsEvent::EventHandler,         "UPDATE_PLAYER_SCARS_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CHECK_EXE_SIZE_EVENT,           CNetworkCheckExeSizeEvent::EventHandler,       "NETWORK_CHECK_EXE_SIZE_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_PTFX_EVENT,				     CNetworkPtFXEvent::EventHandler,               "NETWORK_PTFX_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_PED_SEEN_DEAD_PED_EVENT,	     CNetworkPedSeenDeadPedEvent::EventHandler,     "NETWORK_PED_SEEN_DEAD_PED_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOVE_STICKY_BOMB_EVENT,			     CRemoveStickyBombEvent::EventHandler,          "REMOVE_STICKY_BOMB_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CHECK_CODE_CRCS_EVENT,          CNetworkCodeCRCFailedEvent::EventHandler,      "NETWORK_CHECK_CODE_CRCS_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(INFORM_SILENCED_GUNSHOT_EVENT, 	     CInformSilencedGunShotEvent::EventHandler,     "INFORM_SILENCED_GUNSHOT_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(PED_PLAY_PAIN_EVENT, 	                 CPedPlayPainEvent::EventHandler,               "PED_PLAY_PAIN_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(CACHE_PLAYER_HEAD_BLEND_DATA_EVENT,     CCachePlayerHeadBlendDataEvent::EventHandler,  "CACHE_PLAYER_HEAD_BLEND_DATA_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REMOVE_PED_FROM_PEDGROUP_EVENT,         CRemovePedFromPedGroupEvent::EventHandler,     "REMOVE_PED_FROM_PEDGROUP_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REPORT_MYSELF_EVENT,                    CReportMyselfEvent::EventHandler,              "REPORT_MYSELF_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(REPORT_CASH_SPAWN_EVENT,                CReportCashSpawnEvent::EventHandler,           "REPORT_CASH_SPAWN_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT, CActivateVehicleSpecialAbilityEvent::EventHandler,  "ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT" );
		NetworkInterface::GetEventManager().RegisterNetworkEvent(BLOCK_WEAPON_SELECTION,				 CBlockWeaponSelectionEvent::EventHandler,		"BLOCK_WEAPON_SELECTION");
#if RSG_PC
		NetworkInterface::GetEventManager().RegisterNetworkEvent(NETWORK_CHECK_CATALOG_CRC,            CNetworkCheckCatalogCrc::EventHandler,        "NETWORK_CHECK_CATALOG_CRC" );
#endif // RSG_PC

		// remember to increment netGameEvent::NETEVENTTYPE_MAXTYPES if adding a new event!
	}
}
