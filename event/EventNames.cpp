// File header
#include "Event/EventNames.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Event/EventDamage.h"
#include "Event/EventLeader.h"
#include "Event/EventMovement.h"
#include "Event/EventNetwork.h"
#include "Event/Events.h"
#include "Event/EventReactions.h"
#include "Event/EventScript.h"
#include "Event/EventShocking.h"
#include "Event/EventSound.h"
#include "Event/EventWeapon.h"

#if !__NO_OUTPUT
// Static initialisation
bool       CEventNames::ms_bEventsCounted = false;
CEventName CEventNames::ms_EventNames[NUM_EVENTTYPE];

#define E2N(a,b) { ms_EventNames[a].m_pName = #a; ms_EventNames[a].m_iEventSize = b; }

void CEventNames::AddEventNames()
{
	E2N(EVENT_VEHICLE_COLLISION,						sizeof(CEventVehicleCollision))				
	E2N(EVENT_PED_COLLISION_WITH_PED,					sizeof(CEventPedCollisionWithPed))			
	E2N(EVENT_PED_COLLISION_WITH_PLAYER,				sizeof(CEventPedCollisionWithPlayer))		
	E2N(EVENT_PLAYER_COLLISION_WITH_PED,				sizeof(CEventPlayerCollisionWithPed))
	E2N(EVENT_OBJECT_COLLISION,							sizeof(CEventObjectCollision))
	E2N(EVENT_DRAGGED_OUT_CAR,							sizeof(CEventDraggedOutCar))					
	E2N(EVENT_DAMAGE,									sizeof(CEventDamage))							
	E2N(EVENT_DEATH,									sizeof(CEventDeath))		
#if CNC_MODE_ENABLED
	E2N(EVENT_INCAPACITATED,							sizeof(CEventIncapacitated))
#endif
	E2N(EVENT_POTENTIAL_GET_RUN_OVER,					sizeof(CEventPotentialGetRunOver))
	E2N(EVENT_SHOT_FIRED,								sizeof(CEventGunShot))						
	E2N(EVENT_COP_CAR_BEING_STOLEN,						sizeof(CEventCopCarBeingStolen))
	E2N(EVENT_PED_ENTERED_MY_VEHICLE,					sizeof(CEventPedEnteredMyVehicle))
	E2N(EVENT_PED_JACKING_MY_VEHICLE,					sizeof(CEventPedJackingMyVehicle))
	E2N(EVENT_REVIVED,									sizeof(CEventRevived))
	E2N(EVENT_PED_TO_CHASE,								sizeof(CEventPedToChase))
	E2N(EVENT_PED_TO_FLEE,								sizeof(CEventPedToFlee))
	E2N(EVENT_GUN_AIMED_AT,								sizeof(CEventGunAimedAt))
	E2N(EVENT_HELP_AMBIENT_FRIEND,						sizeof(CEventHelpAmbientFriend))
	E2N(EVENT_FRIENDLY_AIMED_AT,					sizeof(CEventFriendlyAimedAt))
	E2N(EVENT_FRIENDLY_FIRE_NEAR_MISS,		sizeof(CEventFriendlyFireNearMiss))
	E2N(EVENT_SCRIPT_COMMAND,							sizeof(CEventScriptCommand))
	E2N(EVENT_IN_AIR,									sizeof(CEventInAir))
	E2N(EVENT_ACQUAINTANCE_PED_DEAD,					sizeof(CEventAcquaintancePedDead))	
	E2N(EVENT_ACQUAINTANCE_PED_HATE,					sizeof(CEventAcquaintancePedHate))			
	E2N(EVENT_ACQUAINTANCE_PED_DISLIKE,					sizeof(CEventAcquaintancePedDislike))			
	E2N(EVENT_ACQUAINTANCE_PED_LIKE,					sizeof(CEventAcquaintancePedLike))			
	E2N(EVENT_ACQUAINTANCE_PED_RESPECT,					sizeof(CEventAcquaintancePedRespect))				
	E2N(EVENT_ENCROACHING_PED,							sizeof(CEventEncroachingPed))			
	E2N(EVENT_VEHICLE_DAMAGE_WEAPON,					sizeof(CEventVehicleDamageWeapon))		
	E2N(EVENT_POTENTIAL_WALK_INTO_OBJECT,				sizeof(CEventPotentialWalkIntoObject))							
	E2N(EVENT_CAR_UNDRIVEABLE,							sizeof(CEventCarUndriveable))	
	E2N(EVENT_CRIME_REPORTED,							sizeof(CEventCrimeReported))						
	E2N(EVENT_POTENTIAL_BLAST,							sizeof(CEventPotentialBlast))				
	E2N(EVENT_POTENTIAL_WALK_INTO_FIRE,					sizeof(CEventPotentialWalkIntoFire))				
	E2N(EVENT_SHOT_FIRED_WHIZZED_BY,					sizeof(CEventGunShotWhizzedBy))		
	E2N(EVENT_LEADER_ENTERED_CAR_AS_DRIVER,				sizeof(CEventLeaderEnteredCarAsDriver))				
	E2N(EVENT_LEADER_EXITED_CAR_AS_DRIVER,				sizeof(CEventLeaderExitedCarAsDriver))			    
	E2N(EVENT_POTENTIAL_WALK_INTO_VEHICLE,				sizeof(CEventPotentialWalkIntoVehicle)) 	
	E2N(EVENT_ON_FIRE,									sizeof(CEventOnFire))
	E2N(EVENT_OPEN_DOOR,								sizeof(CEventOpenDoor))
	E2N(EVENT_SHOVE_PED,								sizeof(CEventShovePed))
	E2N(EVENT_FIRE_NEARBY,								sizeof(CEventFireNearby))
	E2N(EVENT_IN_WATER,									sizeof(CEventInWater))		
	E2N(EVENT_GET_OUT_OF_WATER,							sizeof(CEventGetOutOfWater))
	E2N(EVENT_VEHICLE_ON_FIRE,							sizeof(CEventVehicleOnFire))
	E2N(EVENT_PED_ON_CAR_ROOF,							sizeof(CEventPedOnCarRoof))
	E2N(EVENT_STUCK_IN_AIR,								sizeof(CEventStuckInAir))
	E2N(EVENT_COMMUNICATE_EVENT,						sizeof(CEventCommunicateEvent))
	E2N(EVENT_CALL_FOR_COVER,							sizeof(CEventCallForCover))
	E2N(EVENT_SHOUT_TARGET_POSITION,					sizeof(CEventShoutTargetPosition))
	E2N(EVENT_SHOT_FIRED_BULLET_IMPACT,					sizeof(CEventGunShotBulletImpact))
	E2N(EVENT_ACQUAINTANCE_PED_WANTED,					sizeof(CEventAcquaintancePedWanted))
	E2N(EVENT_INJURED_CRY_FOR_HELP,						sizeof(CEventInjuredCryForHelp))
	E2N(EVENT_CRIME_CRY_FOR_HELP,						sizeof(CEventCrimeCryForHelp))
	E2N(EVENT_REQUEST_HELP_WITH_CONFRONTATION,			sizeof(CEventRequestHelpWithConfrontation))
	E2N(EVENT_NEW_TASK,									sizeof(CEventNewTask))
	E2N(EVENT_STATIC_COUNT_REACHED_MAX,					sizeof(CEventStaticCountReachedMax))
	E2N(EVENT_POTENTIAL_BE_WALKED_INTO,					sizeof(CEventPotentialBeWalkedInto))
	E2N(EVENT_GIVE_PED_TASK,							sizeof(CEventGivePedTask))
	E2N(EVENT_MELEE_ACTION,								sizeof(CEventMeleeAction))
	E2N(EVENT_FLUSH_TASKS,								sizeof(CEventFlushTasks))
	E2N(EVENT_CLIMB_LADDER_ON_ROUTE,					sizeof(CEventClimbLadderOnRoute))
	E2N(EVENT_CLIMB_NAVMESH_ON_ROUTE,					sizeof(CEventClimbNavMeshOnRoute))
	E2N(EVENT_SWITCH_2_NM_TASK,							sizeof(CEventSwitch2NM))
	E2N(EVENT_DUMMY_CONVERSION,							sizeof(CEventDummyConversion))
	E2N(EVENT_RAN_OVER_PED,								sizeof(CEventRanOverPed))
	E2N(EVENT_LEADER_HOLSTERED_WEAPON,					sizeof(CEventLeaderHolsteredWeapon))
	E2N(EVENT_LEADER_UNHOLSTERED_WEAPON,				sizeof(CEventLeaderUnholsteredWeapon))
	E2N(EVENT_LEADER_ENTERED_COVER,						sizeof(CEventLeaderEnteredCover))
	E2N(EVENT_LEADER_LEFT_COVER,						sizeof(CEventLeaderLeftCover))
	E2N(EVENT_EXPLOSION,								sizeof(CEventExplosion))
	E2N(EVENT_MUST_LEAVE_BOAT,							sizeof(CEventMustLeaveBoat))
	E2N(EVENT_SUSPICIOUS_ACTIVITY,						sizeof(CEventSuspiciousActivity))
	E2N(EVENT_DEAD_PED_FOUND,							sizeof(CEventDeadPedFound))
	E2N(EVENT_REACTION_ENEMY_PED,						sizeof(CEventReactionEnemyPed))
	E2N(EVENT_REACTION_INVESTIGATE_THREAT,				sizeof(CEventReactionInvestigateThreat))
	E2N(EVENT_REACTION_INVESTIGATE_DEAD_PED,			sizeof(CEventReactionInvestigateDeadPed))
	E2N(EVENT_REACTION_COMBAT_VICTORY,					sizeof(CEventReactionCombatVictory))
	E2N(EVENT_FOOT_STEP_HEARD,							sizeof(CEventFootStepHeard))
	E2N(EVENT_EXPLOSION_HEARD,							sizeof(CEventExplosionHeard))
	E2N(EVENT_UNIDENTIFIED_PED,							sizeof(CEventUnidentifiedPed))
	E2N(EVENT_RADIO_TARGET_POSITION,					sizeof(CEventRadioTargetPosition))
	E2N(EVENT_WHISTLING_HEARD,							sizeof(CEventWhistlingHeard))
	E2N(EVENT_DISTURBANCE,								sizeof(CEventDisturbance))
	E2N(EVENT_ENTITY_DAMAGED,							sizeof(CEventEntityDamaged))
	E2N(EVENT_ENTITY_DESTROYED,							sizeof(CEventEntityDestroyed))
	E2N(EVENT_AGITATED,									sizeof(CEventAgitated))
	E2N(EVENT_AGITATED_ACTION,							sizeof(CEventAgitatedAction))
	E2N(EVENT_COMBAT_TAUNT,								sizeof(CEventCombatTaunt))
	E2N(EVENT_RESPONDED_TO_THREAT,						sizeof(CEventRespondedToThreat))
	E2N(EVENT_PLAYER_DEATH,								sizeof(CEventPlayerDeath))
	E2N(EVENT_SHOCKING_BROKEN_GLASS,					sizeof(CEventShockingBrokenGlass))
	E2N(EVENT_SHOCKING_CAR_ALARM,						sizeof(CEventShockingCarAlarm))
	E2N(EVENT_SHOCKING_CAR_CHASE,						sizeof(CEventShockingCarChase))
	E2N(EVENT_SHOCKING_CAR_CRASH,						sizeof(CEventShockingCarCrash))
	E2N(EVENT_SHOCKING_BICYCLE_CRASH,					sizeof(CEventShockingBicycleCrash))
	E2N(EVENT_SHOCKING_CAR_PILE_UP,						sizeof(CEventShockingCarPileUp))
	E2N(EVENT_SHOCKING_CAR_ON_CAR,						sizeof(CEventShockingCarOnCar))
	E2N(EVENT_SHOCKING_DANGEROUS_ANIMAL,				sizeof(CEventShockingDangerousAnimal))
	E2N(EVENT_SHOCKING_DEAD_BODY,						sizeof(CEventShockingDeadBody))
	E2N(EVENT_SHOCKING_DRIVING_ON_PAVEMENT,				sizeof(CEventShockingDrivingOnPavement))
	E2N(EVENT_SHOCKING_BICYCLE_ON_PAVEMENT,				sizeof(CEventShockingBicycleOnPavement))
	E2N(EVENT_SHOCKING_ENGINE_REVVED,					sizeof(CEventShockingEngineRevved))
	E2N(EVENT_SHOCKING_EXPLOSION,						sizeof(CEventShockingExplosion))
	E2N(EVENT_SHOCKING_FIRE,							sizeof(CEventShockingFire))
	E2N(EVENT_SHOCKING_GUN_FIGHT,						sizeof(CEventShockingGunFight))
	E2N(EVENT_SHOCKING_GUNSHOT_FIRED,					sizeof(CEventShockingGunshotFired))
	E2N(EVENT_SHOCKING_HELICOPTER_OVERHEAD,				sizeof(CEventShockingHelicopterOverhead))
	E2N(EVENT_SHOCKING_PARACHUTER_OVERHEAD,				sizeof(CEventShockingParachuterOverhead))
	E2N(EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER,		sizeof(CEventShockingPedKnockedIntoByPlayer))
	E2N(EVENT_SHOCKING_HORN_SOUNDED,					sizeof(CEventShockingHornSounded))
	E2N(EVENT_SHOCKING_IN_DANGEROUS_VEHICLE,			sizeof(CEventShockingInDangerousVehicle))
	E2N(EVENT_SHOCKING_INJURED_PED,						sizeof(CEventShockingInjuredPed))
	E2N(EVENT_SHOCKING_MAD_DRIVER,						sizeof(CEventShockingMadDriver))
	E2N(EVENT_SHOCKING_MAD_DRIVER_EXTREME,				sizeof(CEventShockingMadDriverExtreme))
	E2N(EVENT_SHOCKING_MAD_DRIVER_BICYCLE,				sizeof(CEventShockingMadDriverBicycle))
	E2N(EVENT_SHOCKING_MUGGING,							sizeof(CEventShockingMugging))
	E2N(EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT,		sizeof(CEventShockingNonViolentWeaponAimedAt))
	E2N(EVENT_SHOCKING_PED_RUN_OVER,					sizeof(CEventShockingPedRunOver))
	E2N(EVENT_SHOCKING_PED_SHOT,						sizeof(CEventShockingPedShot))
	E2N(EVENT_SHOCKING_PLANE_FLY_BY,					sizeof(CEventShockingPlaneFlyby))
	E2N(EVENT_SHOCKING_POTENTIAL_BLAST,					sizeof(CEventShockingPotentialBlast))
	E2N(EVENT_SHOCKING_PROPERTY_DAMAGE,					sizeof(CEventShockingPropertyDamage))
	E2N(EVENT_SHOCKING_SEEN_CAR_STOLEN,					sizeof(CEventShockingSeenCarStolen))
	E2N(EVENT_SHOCKING_SEEN_CONFRONTATION,				sizeof(CEventShockingSeenConfrontation))
	E2N(EVENT_SHOCKING_SEEN_GANG_FIGHT,					sizeof(CEventShockingSeenGangFight))
	E2N(EVENT_SHOCKING_SEEN_INSULT,						sizeof(CEventShockingSeenInsult))
	E2N(EVENT_SHOCKING_SEEN_MELEE_ACTION,				sizeof(CEventShockingSeenMeleeAction))
	E2N(EVENT_SHOCKING_SEEN_NICE_CAR,					sizeof(CEventShockingSeenNiceCar))
	E2N(EVENT_SHOCKING_SEEN_PED_KILLED,					sizeof(CEventShockingSeenPedKilled))
	E2N(EVENT_SHOCKING_SEEN_VEHICLE_TOWED,				sizeof(CEventShockingVehicleTowed))
	E2N(EVENT_SHOCKING_SEEN_WEAPON_THREAT,				sizeof(CEventShockingWeaponThreat))
	E2N(EVENT_SHOCKING_SEEN_WEIRD_PED,					sizeof(CEventShockingWeirdPed))
	E2N(EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING,		sizeof(CEventShockingWeirdPedApproaching))
	E2N(EVENT_SHOCKING_SIREN,							sizeof(CEventShockingSiren))
	E2N(EVENT_SHOCKING_STUDIO_BOMB,						sizeof(CEventShockingStudioBomb))
	E2N(EVENT_SHOCKING_RUNNING_PED,						sizeof(CEventShockingRunningPed))
	E2N(EVENT_SHOCKING_RUNNING_STAMPEDE,				sizeof(CEventShockingRunningStampede))
	E2N(EVENT_SHOCKING_VISIBLE_WEAPON,					sizeof(CEventShockingVisibleWeapon))
	E2N(EVENT_WRITHE,									sizeof(CEventWrithe))	
	E2N(EVENT_HURT_TRANSITION,							sizeof(CEventHurtTransition))
	E2N(EVENT_PLAYER_UNABLE_TO_ENTER_VEHICLE,			sizeof(CEventPlayerUnableToEnterVehicle))
	E2N(EVENT_SCENARIO_FORCE_ACTION,					sizeof(CEventScenarioForceAction))	
	E2N(EVENT_STAT_VALUE_CHANGED,					    sizeof(CEventStatChangedValue))
	E2N(EVENT_PED_SEEN_DEAD_PED,					    sizeof(CEventPedSeenDeadPed))

	// network events
	E2N(EVENT_NETWORK_PLAYER_JOIN_SESSION,				sizeof(CEventNetworkPlayerJoinSession))
	E2N(EVENT_NETWORK_PLAYER_LEFT_SESSION,				sizeof(CEventNetworkPlayerLeftSession))		
	E2N(EVENT_NETWORK_PLAYER_JOIN_SCRIPT,				sizeof(CEventNetworkPlayerJoinScript))
	E2N(EVENT_NETWORK_PLAYER_LEFT_SCRIPT,				sizeof(CEventNetworkPlayerLeftScript))
    E2N(EVENT_NETWORK_STORE_PLAYER_LEFT,				sizeof(CEventNetworkStorePlayerLeft))
    E2N(EVENT_NETWORK_SESSION_START,					sizeof(CEventNetworkStartSession))
	E2N(EVENT_NETWORK_SESSION_END,						sizeof(CEventNetworkEndSession))
	E2N(EVENT_NETWORK_START_MATCH,						sizeof(CEventNetworkStartMatch))
	E2N(EVENT_NETWORK_END_MATCH,						sizeof(CEventNetworkEndMatch))
	E2N(EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_STALL,sizeof(CEventNetworkRemovedFromSessionDueToStall))
	E2N(EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_COMPLAINTS,sizeof(CEventNetworkRemovedFromSessionDueToComplaints))
	E2N(EVENT_NETWORK_CONNECTION_TIMEOUT,				sizeof(CEventNetworkConnectionTimeout))
	E2N(EVENT_NETWORK_PED_DROPPED_WEAPON,				sizeof(CEventNetworkPedDroppedWeapon))
	E2N(EVENT_NETWORK_PLAYER_SPAWN,						sizeof(CEventNetworkPlayerSpawn))
	E2N(EVENT_NETWORK_PLAYER_COLLECTED_PICKUP,			sizeof(CEventNetworkPlayerCollectedPickup))
	E2N(EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP,	sizeof(CEventNetworkPlayerCollectedAmbientPickup))
	E2N(EVENT_NETWORK_PLAYER_COLLECTED_PORTABLE_PICKUP,	sizeof(CEventNetworkPlayerCollectedPortablePickup))
	E2N(EVENT_NETWORK_PLAYER_DROPPED_PORTABLE_PICKUP,	sizeof(CEventNetworkPlayerDroppedPortablePickup))
	E2N(EVENT_NETWORK_INVITE_ARRIVED,					sizeof(CEventNetworkInviteArrived))
	E2N(EVENT_NETWORK_INVITE_ACCEPTED,					sizeof(CEventNetworkInviteAccepted))
	E2N(EVENT_NETWORK_INVITE_CONFIRMED,					sizeof(CEventNetworkInviteConfirmed))
	E2N(EVENT_NETWORK_INVITE_REJECTED,					sizeof(CEventNetworkInviteRejected))
	E2N(EVENT_NETWORK_SUMMON,							sizeof(CEventNetworkSummon))
	E2N(EVENT_NETWORK_SCRIPT_EVENT,						sizeof(CEventNetworkScriptEvent))
	E2N(EVENT_NETWORK_PLAYER_SIGNED_OFFLINE,			sizeof(CEventNetworkPlayerSignedOffline))
	E2N(EVENT_NETWORK_SIGN_IN_STATE_CHANGED,			sizeof(CEventNetworkSignInStateChanged))
	E2N(EVENT_NETWORK_SIGN_IN_CHANGE_ACTIONED,			sizeof(CEventNetworkSignInChangeActioned))
	E2N(EVENT_NETWORK_NETWORK_ROS_CHANGED,				sizeof(CEventNetworkRosChanged))
	E2N(EVENT_NETWORK_NETWORK_BAIL,						sizeof(CEventNetworkBail))
	E2N(EVENT_NETWORK_HOST_MIGRATION,					sizeof(CEventNetworkHostMigration))
	E2N(EVENT_NETWORK_FIND_SESSION,						sizeof(CEventNetworkFindSession))
	E2N(EVENT_NETWORK_HOST_SESSION,						sizeof(CEventNetworkHostSession))
	E2N(EVENT_NETWORK_JOIN_SESSION,						sizeof(CEventNetworkJoinSession))
	E2N(EVENT_NETWORK_JOIN_SESSION_RESPONSE,			sizeof(CEventNetworkJoinSessionResponse))
	E2N(EVENT_NETWORK_CHEAT_TRIGGERED,					sizeof(CEventNetworkCheatTriggered))
	E2N(EVENT_NETWORK_DAMAGE_ENTITY,					sizeof(CEventNetworkEntityDamage))
	E2N(EVENT_NETWORK_PLAYER_ARREST,					sizeof(CEventNetworkPlayerArrest))
	E2N(EVENT_NETWORK_TIMED_EXPLOSION,					sizeof(CEventNetworkTimedExplosion))
	E2N(EVENT_NETWORK_PRIMARY_CLAN_CHANGED,				sizeof(CEventNetworkPrimaryClanChanged))
	E2N(EVENT_NETWORK_CLAN_JOINED,						sizeof(CEventNetworkClanJoined))
	E2N(EVENT_NETWORK_CLAN_LEFT,						sizeof(CEventNetworkClanLeft))
	E2N(EVENT_NETWORK_CLAN_INVITE_RECEIVED,				sizeof(CEventNetworkClanInviteReceived))
	E2N(EVENT_VOICE_SESSION_STARTED,					sizeof(CEventNetworkVoiceSessionStarted))
	E2N(EVENT_VOICE_SESSION_ENDED,						sizeof(CEventNetworkVoiceSessionEnded))
	E2N(EVENT_VOICE_CONNECTION_REQUESTED,				sizeof(CEventNetworkVoiceConnectionRequested))
	E2N(EVENT_VOICE_CONNECTION_RESPONSE,				sizeof(CEventNetworkVoiceConnectionResponse))
	E2N(EVENT_VOICE_CONNECTION_TERMINATED,				sizeof(CEventNetworkVoiceConnectionTerminated))
    E2N(EVENT_TEXT_MESSAGE_RECEIVED,				    sizeof(CEventNetworkTextMessageReceived))
    E2N(EVENT_CLOUD_FILE_RESPONSE,						sizeof(CEventNetworkCloudFileResponse))
	E2N(EVENT_NETWORK_PICKUP_RESPAWNED,					sizeof(CEventNetworkPickupRespawned))
	E2N(EVENT_NETWORK_PRESENCE_STAT_UPDATE,				sizeof(CEventNetworkPresence_StatUpdate))
	E2N(EVENT_NETWORK_PED_LEFT_BEHIND,					sizeof(CEventNetworkPedLeftBehind))
	E2N(EVENT_NETWORK_INBOX_MSGS_RCVD,					sizeof(CEventNetwork_InboxMsgReceived))
	E2N(EVENT_NETWORK_ATTEMPT_HOST_MIGRATION,			sizeof(CEventNetworkAttemptHostMigration))
	E2N(EVENT_NETWORK_INCREMENT_STAT,					sizeof(CEventNetworkIncrementStat))
	E2N(EVENT_NETWORK_SESSION_EVENT,					sizeof(CEventNetworkSessionEvent))
	E2N(EVENT_NETWORK_TRANSITION_STARTED,				sizeof(CEventNetworkTransitionStarted))
	E2N(EVENT_NETWORK_TRANSITION_EVENT,					sizeof(CEventNetworkTransitionEvent))
	E2N(EVENT_NETWORK_TRANSITION_MEMBER_JOINED,			sizeof(CEventNetworkTransitionMemberJoined))
	E2N(EVENT_NETWORK_TRANSITION_MEMBER_LEFT,			sizeof(CEventNetworkTransitionMemberLeft))
	E2N(EVENT_NETWORK_TRANSITION_PARAMETER_CHANGED,		sizeof(CEventNetworkTransitionParameterChanged))
	E2N(EVENT_NETWORK_TRANSITION_STRING_CHANGED,		sizeof(CEventNetworkTransitionStringChanged))
	E2N(EVENT_NETWORK_TRANSITION_GAMER_INSTRUCTION,		sizeof(CEventNetworkTransitionGamerInstruction))
	E2N(EVENT_NETWORK_PRESENCE_INVITE,					sizeof(CEventNetworkPresenceInvite))
	E2N(EVENT_NETWORK_PRESENCE_INVITE_REMOVED,			sizeof(CEventNetworkPresenceInviteRemoved))
    E2N(EVENT_NETWORK_PRESENCE_INVITE_REPLY,			sizeof(CEventNetworkPresenceInviteReply))
    E2N(EVENT_NETWORK_CASH_TRANSACTION_LOG,			    sizeof(CEventNetworkCashTransactionLog))
    E2N(EVENT_NETWORK_PRESENCE_TRIGGER,			        sizeof(CEventNetworkPresenceTriggerEvent))
    E2N(EVENT_NETWORK_FOLLOW_INVITE_RECEIVED,			sizeof(CEventNetworkFollowInviteReceived))
    E2N(EVENT_NETWORK_ADMIN_INVITED,			        sizeof(CEventNetworkAdminInvited))
    E2N(EVENT_NETWORK_SPECTATE_LOCAL,			        sizeof(CEventNetworkSpectateLocal))
    E2N(EVENT_NETWORK_CLOUD_EVENT,			            sizeof(CEventNetworkCloudEvent))
    E2N(EVENT_NETWORK_SHOP_TRANSACTION,			        sizeof(CEventNetworkShopTransaction))
	E2N(EVENT_NETWORK_PERMISSION_CHECK_RESULT,			sizeof(CEventNetworkPermissionCheckResult))
	E2N(EVENT_NETWORK_APP_LAUNCHED,						sizeof(CEventNetworkAppLaunched))
	E2N(EVENT_NETWORK_SYSTEM_SERVICE_EVENT,				sizeof(CEventNetworkSystemServiceEvent))
	E2N(EVENT_NETWORK_SCADMIN_PLAYER_UPDATED,			sizeof(CEventNetworkScAdminPlayerUpdated))
	E2N(EVENT_NETWORK_SCADMIN_RECEIVED_CASH,			sizeof(CEventNetworkScAdminReceivedCash))
	E2N(EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED,		sizeof(CEventNetworkClanInviteRequestReceived))
	E2N(EVENT_NETWORK_VEHICLE_UNDRIVABLE,				sizeof(CEventNetworkVehicleUndrivable))
	E2N(EVENT_NETWORK_PLAYER_ENTERED_VEHICLE,			sizeof(CEventNetworkPlayerEnteredVehicle))
	E2N(EVENT_NETWORK_PLAYER_ACTIVATED_SPECIAL_ABILITY, sizeof(CEventNetworkPlayerActivatedSpecialAbility))
	E2N(EVENT_NETWORK_PLAYER_DEACTIVATED_SPECIAL_ABILITY, sizeof(CEventNetworkPlayerDeactivatedSpecialAbility))
	E2N(EVENT_NETWORK_PLAYER_SPECIAL_ABILITY_FAILED_ACTIVATION, sizeof(CEventNetworkPlayerSpecialAbilityFailedActivation))
	E2N(EVENT_NETWORK_SC_MEMBERSHIP_STATUS,				sizeof(CEventNetworkScMembershipStatus))		
	E2N(EVENT_NETWORK_SC_BENEFITS_STATUS,				sizeof(CEventNetworkScMembershipStatus))

		
};

const char* CEventNames::GetEventName(eEventType eventType)
{
	if(eventType < 0 || eventType >= NUM_EVENTTYPE)
	{
		return "Invalid event";
	}

	if(!ms_bEventsCounted)
	{
		AddEventNames();
		ms_bEventsCounted = true;
	}

	if(ms_EventNames[eventType].m_pName)
	{
		return ms_EventNames[eventType].m_pName;
	}

	return "No event name";
}

bool CEventNames::EventNameExists(eEventType eventType)
{
	if(eventType < 0 || eventType >= NUM_EVENTTYPE)
	{
		return false;
	}

	if(!ms_bEventsCounted)
	{
		AddEventNames();
		ms_bEventsCounted = true;
	}

	if(ms_EventNames[eventType].m_pName && ms_EventNames[eventType].m_iEventSize > 0)
	{
		return true;
	}

	return false;
}

void CEventNames::PrintEventNamesAndSizes()
{
	if(!ms_bEventsCounted)
	{
		AddEventNames();
		ms_bEventsCounted = true;
	}

	for(s32 i = 0; i < NUM_EVENTTYPE; i++)
	{
		if(ms_EventNames[i].m_pName)
		{
			char string[1024];
			sprintf(string,"%s, %d\n", ms_EventNames[i].m_pName, ms_EventNames[i].m_iEventSize );
			aiDisplayf("%s", string);
		}
	}
}


#endif //!__NO_OUTPUT
