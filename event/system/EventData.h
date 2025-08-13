#ifndef INC_EVENT_DATA_H
#define INC_EVENT_DATA_H

// Rage Headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "Parser/macros.h"

// Game headers

// Forward declarations
class CEventDataManager;
class CEventDecisionMakerResponse;

//
// IMPORTANT - When adding to or changing this file please also update the pargen declaration in EventDataMeta.psc
// IMPORTANT - When adding to or changing this file please also update event_enums.sch
//
enum eEventType
{
	// AI Events
	EVENT_ACQUAINTANCE_PED_DISLIKE,
	EVENT_ACQUAINTANCE_PED_HATE,
	EVENT_ACQUAINTANCE_PED_LIKE,
	EVENT_ACQUAINTANCE_PED_RESPECT,
	EVENT_ACQUAINTANCE_PED_WANTED,
	EVENT_ACQUAINTANCE_PED_DEAD,
	EVENT_AGITATED,
	EVENT_AGITATED_ACTION,
	EVENT_ENCROACHING_PED,
	EVENT_CALL_FOR_COVER,
	EVENT_CAR_UNDRIVEABLE,
	EVENT_CLIMB_LADDER_ON_ROUTE,
	EVENT_CLIMB_NAVMESH_ON_ROUTE,
	EVENT_COMBAT_TAUNT,
	EVENT_COMMUNICATE_EVENT,
	EVENT_COP_CAR_BEING_STOLEN,
	EVENT_CRIME_REPORTED,
	EVENT_DAMAGE,
	EVENT_DEAD_PED_FOUND,
	EVENT_DEATH,
	EVENT_DRAGGED_OUT_CAR,
	EVENT_DUMMY_CONVERSION,
	EVENT_EXPLOSION,
	EVENT_EXPLOSION_HEARD,
	EVENT_FIRE_NEARBY,
	EVENT_FLUSH_TASKS,
	EVENT_FOOT_STEP_HEARD,
	EVENT_GET_OUT_OF_WATER,
	EVENT_GIVE_PED_TASK,
	EVENT_GUN_AIMED_AT,
	EVENT_HELP_AMBIENT_FRIEND,
	EVENT_INJURED_CRY_FOR_HELP,
	EVENT_CRIME_CRY_FOR_HELP,
	EVENT_IN_AIR,
	EVENT_IN_WATER,
	EVENT_INCAPACITATED,
	EVENT_LEADER_ENTERED_CAR_AS_DRIVER,
	EVENT_LEADER_ENTERED_COVER,
	EVENT_LEADER_EXITED_CAR_AS_DRIVER,
	EVENT_LEADER_HOLSTERED_WEAPON,
	EVENT_LEADER_LEFT_COVER,
	EVENT_LEADER_UNHOLSTERED_WEAPON,
	EVENT_MELEE_ACTION,
	EVENT_MUST_LEAVE_BOAT,
	EVENT_NEW_TASK,
	EVENT_NONE,
	EVENT_OBJECT_COLLISION,
	EVENT_ON_FIRE,
	EVENT_OPEN_DOOR,
	EVENT_SHOVE_PED,
	EVENT_PED_COLLISION_WITH_PED,
	EVENT_PED_COLLISION_WITH_PLAYER,
	EVENT_PED_ENTERED_MY_VEHICLE,
	EVENT_PED_JACKING_MY_VEHICLE,
	EVENT_PED_ON_CAR_ROOF,
	EVENT_PED_TO_CHASE,
	EVENT_PED_TO_FLEE,
	EVENT_PLAYER_COLLISION_WITH_PED,
	EVENT_PLAYER_LOCK_ON_TARGET,
	EVENT_POTENTIAL_BE_WALKED_INTO,
	EVENT_POTENTIAL_BLAST,
	EVENT_POTENTIAL_GET_RUN_OVER,
	EVENT_POTENTIAL_WALK_INTO_FIRE,
	EVENT_POTENTIAL_WALK_INTO_OBJECT,
	EVENT_POTENTIAL_WALK_INTO_VEHICLE,
	EVENT_PROVIDING_COVER,
	EVENT_RADIO_TARGET_POSITION,
	EVENT_RAN_OVER_PED,
	EVENT_REACTION_COMBAT_VICTORY,
	EVENT_REACTION_ENEMY_PED,
	EVENT_REACTION_INVESTIGATE_DEAD_PED,
	EVENT_REACTION_INVESTIGATE_THREAT,
	EVENT_REQUEST_HELP_WITH_CONFRONTATION,
	EVENT_RESPONDED_TO_THREAT,
	EVENT_REVIVED,
	EVENT_SCRIPT_COMMAND,
	EVENT_SHOCKING_BROKEN_GLASS,
	EVENT_SHOCKING_CAR_ALARM,
	EVENT_SHOCKING_CAR_CHASE,
	EVENT_SHOCKING_CAR_CRASH,
	EVENT_SHOCKING_BICYCLE_CRASH,
	EVENT_SHOCKING_CAR_PILE_UP,
	EVENT_SHOCKING_CAR_ON_CAR,
	EVENT_SHOCKING_DANGEROUS_ANIMAL,
	EVENT_SHOCKING_DEAD_BODY,
	EVENT_SHOCKING_DRIVING_ON_PAVEMENT,
	EVENT_SHOCKING_BICYCLE_ON_PAVEMENT,
	EVENT_SHOCKING_ENGINE_REVVED,
	EVENT_SHOCKING_EXPLOSION,
	EVENT_SHOCKING_FIRE,
	EVENT_SHOCKING_GUN_FIGHT,
	EVENT_SHOCKING_GUNSHOT_FIRED,
	EVENT_SHOCKING_HELICOPTER_OVERHEAD,
	EVENT_SHOCKING_PARACHUTER_OVERHEAD,
	EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER,
	EVENT_SHOCKING_HORN_SOUNDED,
	EVENT_SHOCKING_IN_DANGEROUS_VEHICLE,
	EVENT_SHOCKING_INJURED_PED,
	EVENT_SHOCKING_MAD_DRIVER,
	EVENT_SHOCKING_MAD_DRIVER_EXTREME,
	EVENT_SHOCKING_MAD_DRIVER_BICYCLE,
	EVENT_SHOCKING_MUGGING,
	EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT,
	EVENT_SHOCKING_PED_RUN_OVER,
	EVENT_SHOCKING_PED_SHOT,
	EVENT_SHOCKING_PLANE_FLY_BY,
	EVENT_SHOCKING_POTENTIAL_BLAST,
	EVENT_SHOCKING_PROPERTY_DAMAGE,
	EVENT_SHOCKING_RUNNING_PED,
	EVENT_SHOCKING_RUNNING_STAMPEDE,
	EVENT_SHOCKING_SEEN_CAR_STOLEN,
	EVENT_SHOCKING_SEEN_CONFRONTATION,
	EVENT_SHOCKING_SEEN_GANG_FIGHT,
	EVENT_SHOCKING_SEEN_INSULT,
	EVENT_SHOCKING_SEEN_MELEE_ACTION,
	EVENT_SHOCKING_SEEN_NICE_CAR,
	EVENT_SHOCKING_SEEN_PED_KILLED,
	EVENT_SHOCKING_SEEN_VEHICLE_TOWED,
	EVENT_SHOCKING_SEEN_WEAPON_THREAT,
	EVENT_SHOCKING_SEEN_WEIRD_PED,
	EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING,
	EVENT_SHOCKING_SIREN,
	EVENT_SHOCKING_STUDIO_BOMB,
	EVENT_SHOCKING_VISIBLE_WEAPON,
	EVENT_SHOT_FIRED,
	EVENT_SHOT_FIRED_BULLET_IMPACT,
	EVENT_SHOT_FIRED_WHIZZED_BY,
	EVENT_FRIENDLY_AIMED_AT,
	EVENT_FRIENDLY_FIRE_NEAR_MISS,
	EVENT_SHOUT_BLOCKING_LOS,
	EVENT_SHOUT_TARGET_POSITION,
	EVENT_STATIC_COUNT_REACHED_MAX,
	EVENT_STUCK_IN_AIR,
	EVENT_SUSPICIOUS_ACTIVITY,
	EVENT_SWITCH_2_NM_TASK,
	EVENT_UNIDENTIFIED_PED,
	EVENT_VEHICLE_COLLISION,
	EVENT_VEHICLE_DAMAGE_WEAPON,
	EVENT_VEHICLE_ON_FIRE,
	EVENT_WHISTLING_HEARD,
	EVENT_DISTURBANCE,
	EVENT_ENTITY_DAMAGED,
	EVENT_ENTITY_DESTROYED,
	EVENT_WRITHE,
	EVENT_HURT_TRANSITION,
	EVENT_PLAYER_UNABLE_TO_ENTER_VEHICLE,
	EVENT_SCENARIO_FORCE_ACTION,
	EVENT_STAT_VALUE_CHANGED,
	EVENT_PLAYER_DEATH,
	EVENT_PED_SEEN_DEAD_PED,
	NUM_AI_EVENTTYPE,

	// Network events
	EVENT_NETWORK_PLAYER_JOIN_SESSION,
	EVENT_NETWORK_PLAYER_LEFT_SESSION,
	EVENT_NETWORK_PLAYER_JOIN_SCRIPT,
	EVENT_NETWORK_PLAYER_LEFT_SCRIPT,
    EVENT_NETWORK_STORE_PLAYER_LEFT,
	EVENT_NETWORK_SESSION_START,
	EVENT_NETWORK_SESSION_END,
	EVENT_NETWORK_START_MATCH,
	EVENT_NETWORK_END_MATCH,
	EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_STALL,
	EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_COMPLAINTS,
	EVENT_NETWORK_CONNECTION_TIMEOUT,
	EVENT_NETWORK_PED_DROPPED_WEAPON,
	EVENT_NETWORK_PLAYER_SPAWN,
	EVENT_NETWORK_PLAYER_COLLECTED_PICKUP,
	EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP,
	EVENT_NETWORK_PLAYER_COLLECTED_PORTABLE_PICKUP,
	EVENT_NETWORK_PLAYER_DROPPED_PORTABLE_PICKUP,
	EVENT_NETWORK_INVITE_ARRIVED,
	EVENT_NETWORK_INVITE_ACCEPTED,
	EVENT_NETWORK_INVITE_CONFIRMED,
	EVENT_NETWORK_INVITE_REJECTED,
	EVENT_NETWORK_SUMMON,
	EVENT_NETWORK_SCRIPT_EVENT,
	EVENT_NETWORK_PLAYER_SIGNED_OFFLINE,
	EVENT_NETWORK_SIGN_IN_STATE_CHANGED,
	EVENT_NETWORK_SIGN_IN_CHANGE_ACTIONED,
	EVENT_NETWORK_NETWORK_ROS_CHANGED,
	EVENT_NETWORK_NETWORK_BAIL,
	EVENT_NETWORK_HOST_MIGRATION,
	EVENT_NETWORK_FIND_SESSION,
	EVENT_NETWORK_HOST_SESSION,
	EVENT_NETWORK_JOIN_SESSION,
	EVENT_NETWORK_JOIN_SESSION_RESPONSE,
	EVENT_NETWORK_CHEAT_TRIGGERED,
	EVENT_NETWORK_DAMAGE_ENTITY,
	EVENT_NETWORK_PLAYER_ARREST,
	EVENT_NETWORK_TIMED_EXPLOSION,
	EVENT_NETWORK_PRIMARY_CLAN_CHANGED,
	EVENT_NETWORK_CLAN_JOINED,
	EVENT_NETWORK_CLAN_LEFT,
	EVENT_NETWORK_CLAN_INVITE_RECEIVED,
	EVENT_VOICE_SESSION_STARTED,
	EVENT_VOICE_SESSION_ENDED,
	EVENT_VOICE_CONNECTION_REQUESTED,
	EVENT_VOICE_CONNECTION_RESPONSE,
	EVENT_VOICE_CONNECTION_TERMINATED,
    EVENT_TEXT_MESSAGE_RECEIVED,
    EVENT_CLOUD_FILE_RESPONSE,
	EVENT_NETWORK_PICKUP_RESPAWNED,
	EVENT_NETWORK_PRESENCE_STAT_UPDATE,
	EVENT_NETWORK_PED_LEFT_BEHIND,
	EVENT_NETWORK_INBOX_MSGS_RCVD,
	EVENT_NETWORK_ATTEMPT_HOST_MIGRATION,
	EVENT_NETWORK_INCREMENT_STAT,
	EVENT_NETWORK_SESSION_EVENT,
	EVENT_NETWORK_TRANSITION_STARTED,
	EVENT_NETWORK_TRANSITION_EVENT,
	EVENT_NETWORK_TRANSITION_MEMBER_JOINED,
	EVENT_NETWORK_TRANSITION_MEMBER_LEFT,
	EVENT_NETWORK_TRANSITION_PARAMETER_CHANGED,
	EVENT_NETWORK_CLAN_KICKED,
	EVENT_NETWORK_TRANSITION_STRING_CHANGED,
	EVENT_NETWORK_TRANSITION_GAMER_INSTRUCTION,
	EVENT_NETWORK_PRESENCE_INVITE,
	EVENT_NETWORK_PRESENCE_INVITE_REMOVED,
    EVENT_NETWORK_PRESENCE_INVITE_REPLY,
    EVENT_NETWORK_CASH_TRANSACTION_LOG,
	EVENT_NETWORK_CLAN_RANK_CHANGE,
	EVENT_NETWORK_VEHICLE_UNDRIVABLE,
	EVENT_NETWORK_PRESENCE_TRIGGER,
	EVENT_NETWORK_EMAIL_RECEIVED,
    EVENT_NETWORK_FOLLOW_INVITE_RECEIVED,
    EVENT_NETWORK_ADMIN_INVITED,
    EVENT_NETWORK_SPECTATE_LOCAL,
    EVENT_NETWORK_CLOUD_EVENT,
    EVENT_NETWORK_SHOP_TRANSACTION,
	EVENT_NETWORK_PERMISSION_CHECK_RESULT,
	EVENT_NETWORK_APP_LAUNCHED,
	EVENT_NETWORK_ONLINE_PERMISSIONS_UPDATED,
	EVENT_NETWORK_SYSTEM_SERVICE_EVENT,
	EVENT_NETWORK_REQUEST_DELAY,
	EVENT_NETWORK_SOCIAL_CLUB_ACCOUNT_LINKED,
	EVENT_NETWORK_SCADMIN_PLAYER_UPDATED,
	EVENT_NETWORK_SCADMIN_RECEIVED_CASH,
	EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED,
    EVENT_NETWORK_MARKETING_EMAIL_RECEIVED,
    EVENT_NETWORK_STUNT_PERFORMED,
	EVENT_NETWORK_FIRED_DUMMY_PROJECTILE,
	EVENT_NETWORK_PLAYER_ENTERED_VEHICLE,
	EVENT_NETWORK_PLAYER_ACTIVATED_SPECIAL_ABILITY,
	EVENT_NETWORK_PLAYER_DEACTIVATED_SPECIAL_ABILITY,
	EVENT_NETWORK_PLAYER_SPECIAL_ABILITY_FAILED_ACTIVATION,
	EVENT_NETWORK_FIRED_VEHICLE_PROJECTILE,
	EVENT_NETWORK_SC_MEMBERSHIP_STATUS,
	EVENT_NETWORK_SC_BENEFITS_STATUS,
	NUM_NETWORK_EVENTTYPE,

	EVENT_ERRORS_UNKNOWN_ERROR,
	EVENT_ERRORS_ARRAY_OVERFLOW,
	EVENT_ERRORS_INSTRUCTION_LIMIT,
	EVENT_ERRORS_STACK_OVERFLOW,
	NUM_ERRORS_EVENTTYPE,

	NUM_EVENTTYPE,
	eEVENTTYPE_MAX = NUM_EVENTTYPE,
	EVENT_INVALID = -1,
};

enum InterpretEventAs
{
	IEA_POSITION = 0,
	IEA_ENTITY,
	NUM_INTERPRETEVENTAS,
	INTERPRETEVENTAS_MAX = NUM_INTERPRETEVENTAS,
};

enum EventSourceType
{
	EVENT_SOURCE_UNDEFINED = 0,
	EVENT_SOURCE_PLAYER,
	EVENT_SOURCE_FRIEND,
	EVENT_SOURCE_THREAT,
	NUM_EVENTSOURCETYPE,
	EVENTSOURCETYPE_MAX = NUM_EVENTSOURCETYPE,
};

class CEventDataResponseTask
{
public:
	CEventDataResponseTask() {}
	virtual ~CEventDataResponseTask() {}

	const rage::atHashString & GetName() const	{ return m_Name; }

protected:
	rage::atHashString	m_Name;

	PAR_PARSABLE;
};

class CEventDataResponsePoliceTaskWanted : public CEventDataResponseTask
{
public:
	CEventDataResponsePoliceTaskWanted() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseSwatTaskWanted : public CEventDataResponseTask
{
public:
	CEventDataResponseSwatTaskWanted() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskCombat : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskCombat() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskThreat : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskThreat() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskCower : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskCower() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskHandsUp : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskHandsUp() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskGunAimedAt : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskGunAimedAt() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskFlee : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskFlee() : CEventDataResponseTask(), Flee(IEA_ENTITY) {}

	rage::u8 GetFlee() const { return Flee; }

protected:
	rage::u8 Flee;

	PAR_PARSABLE;
};

class CEventDataResponseTaskTurnToFace : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskTurnToFace() : CEventDataResponseTask(), Face(IEA_ENTITY) {}

	rage::u8 GetFace() const { return Face; }

protected:
	rage::u8 Face;

	PAR_PARSABLE;
};

class CEventDataResponseTaskCrouch : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskCrouch() : CEventDataResponseTask(), TimeToCrouch(3.0f) {}

	float GetTimeToCrouch() const { return TimeToCrouch; }

protected:
	float TimeToCrouch;

	PAR_PARSABLE;
};

class CEventDataResponseTaskLeaveCarAndFlee : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskLeaveCarAndFlee() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskWalkRoundFire : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskWalkRoundFire() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskScenarioFlee : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskScenarioFlee() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskFlyAway : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskFlyAway() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskWalkRoundEntity : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskWalkRoundEntity() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskHeadTrack : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskHeadTrack() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventGoto : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventGoto() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventHurryAway : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventHurryAway() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventWatch : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventWatch() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingPoliceInvestigate : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingPoliceInvestigate() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;

};

class CEventDataResponseTaskEvasiveStep : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskEvasiveStep() : CEventDataResponseTask() {}

protected:

	PAR_PARSABLE;
};

class CEventDataResponseTaskEscapeBlast : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskEscapeBlast() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskAgitated : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskAgitated() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskExplosion : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskExplosion() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskDuckAndCover : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskDuckAndCover() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventReactToAircraft : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventReactToAircraft() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventReact : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventReact() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventBackAway : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventBackAway() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventStopAndStare : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventStopAndStare() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskSharkAttack : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskSharkAttack() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskExhaustedFlee : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskExhaustedFlee() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskGrowlAndFlee : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskGrowlAndFlee() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskWalkAway : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskWalkAway() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseAggressiveRubberneck : public CEventDataResponseTask
{
public:
	CEventDataResponseAggressiveRubberneck() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingNiceCar : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingNiceCar() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseFriendlyNearMiss : public CEventDataResponseTask
{
public:
	CEventDataResponseFriendlyNearMiss() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseFriendlyAimedAt : public CEventDataResponseTask
{
public:
	CEventDataResponseFriendlyAimedAt() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseDeferToScenarioPointFlags : public CEventDataResponseTask
{
public:
	CEventDataResponseDeferToScenarioPointFlags() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponseTaskShockingEventThreatResponse : public CEventDataResponseTask
{
public:
	CEventDataResponseTaskShockingEventThreatResponse() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

class CEventDataResponsePlayerDeath : public CEventDataResponseTask
{
public:
	CEventDataResponsePlayerDeath() : CEventDataResponseTask() {}

	PAR_PARSABLE;
};

enum eEventResponseFleeFlags
{
	DisableCrouch,
	DisableHandsUp,
	DisableCover,
	DisableVehicleExit,
	AllowVehicleOffRoad,
	IsShortTerm
};

class CEventDataDecisionMaker
{
public:
	CEventDataDecisionMaker() { m_CachedDecisionMakerParent = NULL; }
	virtual ~CEventDataDecisionMaker() {}

	const rage::atHashString& GetName() const { return m_Name; }
	rage::u32 GetDecisionMakerParentRef() const { return DecisionMakerParentRef.GetHash(); }

	const CEventDataDecisionMaker* GetCachedDecisionMakerParent() const
	{	return m_CachedDecisionMakerParent;	}

	int GetNumEventResponses() const { return EventResponse.GetCount(); }
	const atHashString & GetEventResponse(const int index) const { return EventResponse[index]; }

	const CEventDecisionMakerResponse* GetCachedSortedEventResponse(int index) const { return m_CachedSortedEventResponse[index]; }
	eEventType GetCachedSortedEventTypeForResponse(int index) const { return (eEventType)m_CachedSortedEventTypeForResponse[index]; }
	int GetNumCachedResponses() const { return m_CachedSortedEventTypeForResponse.GetCount(); }

	int FindFirstPossibleCachedResponseForEventType(eEventType eventType) const;

	void PostLoad();

protected:
	static const rage::u8		MAX_EVENTRESPONSES = 46;

	const CEventDataDecisionMaker*											m_CachedDecisionMakerParent;

	atFixedArray<u16 /*eEventType*/, MAX_EVENTRESPONSES>					m_CachedSortedEventTypeForResponse;

	atFixedArray<const CEventDecisionMakerResponse*, MAX_EVENTRESPONSES>	m_CachedSortedEventResponse;

	rage::atHashString			m_Name;

	rage::atHashString			DecisionMakerParentRef;

	rage::atArray<rage::atHashString>			EventResponse;
	
	PAR_PARSABLE;
};

struct CEventResponseTheDecision
{
public:
	inline CEventResponseTheDecision() : m_TaskType(-1), m_FleeFlags(0), m_pResponse(NULL) {}

	inline void Reset()		{ m_TaskType = -1; m_FleeFlags = 0; }

	int GetTaskType() const				{ return m_TaskType; }
	void SetTaskType(int val)			{ Assert(val >= -0x8000 && val <= 0x7fff); m_TaskType = (s16)val; }

	bool HasDecision() const			{ return m_TaskType != -1; }

	int GetFleeFlags() const			{ return m_FleeFlags; }
	void SetFleeFlags(int val)			{ Assign(m_FleeFlags, val); }

	u32 GetSpeechHash() const			{ return m_SpeechHash;}
	void SetSpeechHash(const u32 val)	{ m_SpeechHash.SetHash(val); }

	void SetResponse(const CEventDecisionMakerResponse* pResponse) { m_pResponse = pResponse; }
	const CEventDecisionMakerResponse* GetResponse() const { return m_pResponse; }

private:
	s16	m_TaskType;
	u8  m_FleeFlags;
	atHashString m_SpeechHash;
	const CEventDecisionMakerResponse* m_pResponse;
};

#endif // INC_EVENT_DATA_H
