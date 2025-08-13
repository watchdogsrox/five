//
// name:        NetworkEventTypes.h
// description: Different NetworkEvent types
// written by:  John Gurney
//

#ifndef NETWORK_EVENTTYPES_H
#define NETWORK_EVENTTYPES_H

// framework includes
#include "fwnet/netgameevent.h"
#include "fwnet/neteventid.h"
#include "fwnet/neteventtypes.h"

// game includes
#include "audio/radiostation.h"
#include "control/garages.h"
#include "event/EventDamage.h"
#include "event/eventnetwork.h"
#include "game/clock.h"
#include "game/crime.h"
#include "game/Dispatch/DispatchEnums.h"
#include "ik/IkManager.h"
#include "network/arrays/NetworkArrayHandlerTypes.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "peds/RelationshipEnums.h" 
#include "script/Handlers/GameScriptMgr.h"
#include "vehicleAi/pathfind.h"
#include "vfx/misc/fire.h"
#include "weapons/Explosion.h"

namespace rage
{
    class netEventMgr;
    class netObject;
}

class CComponentReservationManager;
class CPlayerInfo;
class CNetworkRequester;
class CNetObjPickup;
class CNetworkWorldStateData;
class CPickupPlacement;
class CTaskSequenceList;
class CNetObjPlayer;

enum NetworkGameEventTypes
{
	SCRIPT_ARRAY_DATA_VERIFY_EVENT = USER_EVENT_START
	,REQUEST_CONTROL_EVENT
	,GIVE_CONTROL_EVENT
	,WEAPON_DAMAGE_EVENT
	,REQUEST_PICKUP_EVENT
	,REQUEST_MAP_PICKUP_EVENT
	,CLOCK_EVENT
	,WEATHER_EVENT
	,RESPAWN_PLAYER_PED_EVENT
	,GIVE_WEAPON_EVENT
	,REMOVE_WEAPON_EVENT
	,REMOVE_ALL_WEAPONS_EVENT
	,VEHICLE_COMPONENT_CONTROL_EVENT
	,FIRE_EVENT
	,EXPLOSION_EVENT
	,START_PROJECTILE_EVENT
	,UPDATE_PROJECTILE_TARGET_EVENT
	,REMOVE_PROJECTILE_ENTITY_EVENT
	,BREAK_PROJECTILE_TARGET_LOCK_EVENT
	,ALTER_WANTED_LEVEL_EVENT
	,CHANGE_RADIO_STATION_EVENT
	,RAGDOLL_REQUEST_EVENT
	,PLAYER_TAUNT_EVENT
	,PLAYER_CARD_STAT_EVENT
	,DOOR_BREAK_EVENT
	,SCRIPTED_GAME_EVENT
	,REMOTE_SCRIPT_INFO_EVENT
	,REMOTE_SCRIPT_LEAVE_EVENT
	,MARK_AS_NO_LONGER_NEEDED_EVENT
	,CONVERT_TO_SCRIPT_ENTITY_EVENT
	,SCRIPT_WORLD_STATE_EVENT
	,CLEAR_AREA_EVENT
	,CLEAR_RECTANGLE_AREA_EVENT
	,NETWORK_REQUEST_SYNCED_SCENE_EVENT
	,NETWORK_START_SYNCED_SCENE_EVENT
	,NETWORK_STOP_SYNCED_SCENE_EVENT
	,NETWORK_UPDATE_SYNCED_SCENE_EVENT
	,INCIDENT_ENTITY_EVENT
	,NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT
	,NETWORK_GIVE_PED_SEQUENCE_TASK_EVENT
	,NETWORK_CLEAR_PED_TASKS_EVENT
	,NETWORK_START_PED_ARREST_EVENT
	,NETWORK_START_PED_UNCUFF_EVENT
	,NETWORK_CAR_HORN_EVENT
	,NETWORK_ENTITY_AREA_STATUS_EVENT
	,NETWORK_GARAGE_OCCUPIED_STATUS_EVENT
	,PED_CONVERSATION_LINE_EVENT
	,SCRIPT_ENTITY_STATE_CHANGE_EVENT
	,NETWORK_PLAY_SOUND_EVENT
	,NETWORK_STOP_SOUND_EVENT
	,NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT
	,NETWORK_BANK_REQUEST_EVENT
	,NETWORK_AUDIO_BARK_EVENT
	,REQUEST_DOOR_EVENT
	,NETWORK_TRAIN_REPORT_EVENT
	,NETWORK_TRAIN_REQUEST_EVENT
	,NETWORK_INCREMENT_STAT_EVENT
	,MODIFY_VEHICLE_LOCK_WORD_STATE_DATA
	,MODIFY_PTFX_WORD_STATE_DATA_SCRIPTED_EVOLVE_EVENT
	,NETWORK_REQUEST_PHONE_EXPLOSION_EVENT
	,NETWORK_REQUEST_DETACHMENT_EVENT
	,NETWORK_KICK_VOTES_EVENT
	,NETWORK_GIVE_PICKUP_REWARDS_EVENT
	,NETWORK_CRC_HASH_CHECK_EVENT
	,BLOW_UP_VEHICLE_EVENT
	,NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON
	,NETWORK_RESPONDED_TO_THREAT_EVENT
	,NETWORK_SHOUT_TARGET_POSITION_EVENT
	,VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT
	,PICKUP_DESTROYED_EVENT
	,UPDATE_PLAYER_SCARS_EVENT
	,NETWORK_CHECK_EXE_SIZE_EVENT
	,NETWORK_PTFX_EVENT
	,NETWORK_PED_SEEN_DEAD_PED_EVENT
	,REMOVE_STICKY_BOMB_EVENT
	,NETWORK_CHECK_CODE_CRCS_EVENT
	,INFORM_SILENCED_GUNSHOT_EVENT
	,PED_PLAY_PAIN_EVENT
	,CACHE_PLAYER_HEAD_BLEND_DATA_EVENT
	,REMOVE_PED_FROM_PEDGROUP_EVENT
	,REPORT_MYSELF_EVENT
	,REPORT_CASH_SPAWN_EVENT
	,ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT
	,BLOCK_WEAPON_SELECTION
#if RSG_PC
	,NETWORK_CHECK_CATALOG_CRC
#endif //RSG_PC
};

// ================================================================================================================
// CScriptArrayDataVerifyEvent - triggered when the host of a script changes - all clients send out a checksum of the
//                             host broadcast data to the new host to make sure that they are up to date with it.
// ================================================================================================================

class CScriptArrayDataVerifyEvent : public arrayDataVerifyEvent
{
public:

    CScriptArrayDataVerifyEvent();
    CScriptArrayDataVerifyEvent(const netBroadcastDataArrayIdentifier<CGameScriptId>& identifier, u32 arrayChecksum );

    const char* GetEventName() const { return "SCRIPT_DATA_VERIFY_EVENT"; }

    static void EventHandler(datBitBuffer &msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    static void Trigger(const netBroadcastDataArrayIdentifier<CGameScriptId>& identifier, u32 arrayChecksum);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *messageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

protected:

	virtual netArrayHandlerBase* GetArrayHandler();
	virtual const netArrayHandlerBase* GetArrayHandler() const;

private:

	netBroadcastDataArrayIdentifier<CGameScriptId>	m_identifier;	// The identifier of the script array

#if ENABLE_NETWORK_LOGGING
	bool m_bScriptNotRunning : 1;
	bool m_bPlayerNotAParticipant : 1;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CRequestControlEvent - triggered by a player that wants to take control of a network object that is being
//                        controlled by another player
// ================================================================================================================

class CRequestControlEvent : public netGameEvent
{
public:

    CRequestControlEvent();
    CRequestControlEvent( netObject* pObject );

    const char* GetEventName() const { return "REQUEST_CONTROL_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(netObject* pObject NOTFINAL_ONLY(, const char* reason));

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
    template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

    static const unsigned   SIZEOF_TIMESTAMP = 32;

    ObjectId                m_objectID;             // the object we need control of
    NetworkObjectType       m_objectType;           // the type of the object
    u32                     m_TimeStamp;            // timestamp the event was sent
    unsigned                m_CPCResultCode;        // result code from CanPassControl() - used for logging
	
    void WriteDataToLogFile(bool bEventLogOnly) const;
};

// ================================================================================================================
// CGiveControlEvent - sent by a player giving control of a network object to another player
// ================================================================================================================
class CGiveControlEvent : public netGameEvent
{
public:

    CGiveControlEvent();
	~CGiveControlEvent();

    const char* GetEventName() const { return "GIVE_CONTROL_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const netPlayer& player, netObject* pObject, eMigrationType migrationType);

    bool CanAddGiveControlData(const netPlayer &player, netObject &object, eMigrationType  migrationType);
    bool ContainsGiveControlData(const netPlayer& player,
                                 netObject      &object,
                                 eMigrationType migrationType);
    void AddGiveControlData(const netPlayer& player,
                            netObject       &object,
                            eMigrationType  migrationType);

    virtual bool IsInScope              ( const netPlayer& player) const;
    virtual void Prepare                ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle                 ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide                 (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual void PrepareReply           ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void HandleReply            ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual void PrepareExtraData       ( datBitBuffer&, bool bReply, const netPlayer& player);
    virtual void HandleExtraData        ( datBitBuffer &messageBuffer, bool bReply, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const;

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile    ( bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile () const;
    virtual void WriteReplyToLogFile    (bool wasSent) const;
    virtual void WriteExtraDataToLogFile( datBitBuffer &messageBuffer, bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    static bool CheckForVehicleAndScriptPassengers(const netPlayer& player, netObject* pObject, eMigrationType migrationType);
    void CheckForAllObjectsMigratingTogether();

    u32 GetPlayersInSessionBitMask() const;

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
    template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

    struct GiveControlData
    {
        GiveControlData() :
        m_objectID(NETWORK_INVALID_OBJECT_ID)
        , m_objectType(NUM_NET_OBJ_TYPES)
        , m_migrationType()
        , m_bControlTaken(false)
        , m_bCannotAccept(false)
        , m_bCannotApplyExtraData(false)
        , m_bLocalPlayerIsDead(false)
        , m_bObjectExists(true)
        , m_bTypesDiffer(false)
        , m_CACResultCode(CAC_SUCCESS)
        {
        }

        ObjectId           m_objectID;                  // the object to which control is being given
        NetworkObjectType  m_objectType;                // the object type
        eMigrationType	   m_migrationType;             // the type of migration
        bool               m_bControlTaken;             // true if control is transferred
        bool               m_bCannotAccept;             // the object cannot accept ownership
        bool               m_bCannotApplyExtraData;     // the object cannot apply the extra data
        bool               m_bLocalPlayerIsDead;        // true if local player is about to respawn
        bool               m_bObjectExists;             // indicates whether the object exists
        bool               m_bTypesDiffer;              // indicates the type of the object on the local machine is different from m_objectType
        unsigned           m_CACResultCode;             // result code from CanAcceptControl()
    };

    static const unsigned SIZEOF_NUM_EVENTS                = 2;
    static const unsigned MAX_OBJECTS_PER_EVENT            = ((1<<SIZEOF_NUM_EVENTS) - 1);
    static const unsigned MAX_NUM_NON_PROXIMITY_MIGRATIONS = 3; // non-proximity migrations can have a lot of extra data, so less objects can be passed in a message
    static const unsigned MAX_NUM_SCRIPT_MIGRATIONS        = 2; // script migrations have the most extra data, so even less objects can be passed in a message
    static const unsigned SIZEOF_TIMESTAMP                 = 32;

    const netPlayer*        m_pPlayer;                              // the player being given control
    u32                     m_PhysicalPlayersBitmask;               // bitmask of the players currently in the session (masked on physical player index)
    u32                     m_TimeStamp;                            // timestamp the event was sent
    bool                    m_bTooManyObjects;                      // true if this machine already controls too many objects of this type
    bool                    m_bLocalPlayerPendingTutorialChange;    // true if local player is pending a tutorial session change
    bool                    m_bAllObjectsMigrateTogether;           // if set, all objects in the event must migrate together - if one fails they all fail
    bool                    m_bPhysicalPlayerBitmaskDiffers;        // if set, the players in the session are inconsistent between the sender/receiver - likely a player is joining/leaving
	bool					m_bLocallyTriggered;
	u32                     m_numControlData;                       // number of objects being given control of
    u32                     m_numCriticalMigrations;                // number of objects being given control of via a critical migration
    u32                     m_numScriptMigrations;                  // number of script objects being given control of via a critical migration
    GiveControlData         m_giveControlData[MAX_OBJECTS_PER_EVENT];
};

// ================================================================================================================
// CWeaponDamageEvent - triggered when a weapon damages a network object
// ================================================================================================================

class CWeaponDamageEvent : public netGameEvent
{
public:

    static float LOCAL_HIT_POSITION_MAG;
	static float LOCAL_ENTITY_WEAPON_HIT_POSITION_MAG;

	static const unsigned PLAYER_DAMAGE_IGNORE_RANGE_ON_FOOT = 10;
	static const unsigned PLAYER_DAMAGE_IGNORE_RANGE_IN_VEHICLE = 30;
 
    enum eDamageType
    {
        DAMAGE_OBJECT,
        DAMAGE_VEHICLE,
        DAMAGE_PED,
        DAMAGE_PLAYER
    };

    // size of packed event elements in bits
    enum
    {
		SIZEOF_HEALTH           = 14,
        SIZEOF_DAMAGETYPE       = 2,
        SIZEOF_WEAPONTYPE       = 32,
		SIZEOF_DAMAGEFLAGS      = datBitsNeeded<CPedDamageCalculator::DF_LastDamageFlag>::COUNT,
        SIZEOF_WEAPONDAMAGE     = 17,
		SIZEOF_WEAPONDAMAGE_AGGREGATION	= 4,
        SIZEOF_RAMMEDDAMAGE     = 10,
        SIZEOF_POSITION         = 16,
        SIZEOF_COMPONENT        = 5,
        SIZEOF_TYRE_INDEX       = 4,
        SIZEOF_SUSPENSION_INDEX = 4,
		SIZEOF_ACTIONRESULTID	= 32,
		SIZEOF_MELEEID			= 16,
		SIZEOF_MELEEREACTIONID	= 32,
		SIZEOF_HEALTH_TIMESTAMP = 32,
		SIZEOF_PLAYER_DISTANCE  = 11,
		SIZEOF_PLAYER_DISTANCE_LARGE = 16,
   };

public:

    CWeaponDamageEvent( );
    CWeaponDamageEvent( NetworkEventType eventType);

    CWeaponDamageEvent( CEntity *parentEntity,
                        CEntity *hitEntity,
                        const Vector3& hitPosition,
                        const s32 component,
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
						const bool fireBullet,
						const Vector3* hitDirection = 0);

    ~CWeaponDamageEvent();

public:

    const char* GetEventName() const { return "WEAPON_DAMAGE_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CEntity* pParentEntity, CEntity* pHitEntity, const Vector3& worldHitPosition, const s32 hitComponent, const bool bOverride, const u32 weaponHash, const float weaponDamage, const s32 tyreIndex, const s32 suspensionIndex, const u32 damageFlags, const u32 actionResultId, const u16 meleeId, const u32 nForcedReactionDefinitionID, const bool hitEntityWeapon = false, const bool hitWeaponAmmoAttachment = false, const bool silenced = false, const bool firstBullet = false,  const Vector3* hitDirection = 0);

	virtual bool IsInScope( const netPlayer& player) const;
    virtual bool HasTimedOut() const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }
	virtual bool CanChangeScope() const { return true; }
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

    static void ClearDelayedKill();

private:

	bool ProcessDamage(netObject *parentObject, netObject *hitObject, bool* bReply = NULL);
	bool ComputeWillKillPlayer(const CPed& attackingPlayer, CPed& hitPlayer);
	bool IsHitEntityWeaponEvent();																					// Have I hit an entity weapon (i.e have I shot another ped's weapon and am being sent over to see if the bullet has penetrated and hit the target player)
	bool HitEntityWeaponAmmoAttachment();																			// Did i hit the ammo box attached to a weapon?
	bool IsHitEntityWeaponEntitySuitable(CEntity const* pedEntity);													// Does the ped I'm going to apply this damage hit too still have a weapon in hand for me to do it?
	static bool GetHitEntityWeaponLocalSpacePos(CEntity const* pedEntity, Vector3 const& hitPosWS, Vector3& posLS, bool const hitAmmoAttachment);	// Get the local space position of where I hit the weapon to send over.
	static void GetHitEntityWeaponWorldSpacePos(CEntity const* pedEntity, Vector3 const& hitPosLS, Vector3& posWS, bool const hitAmmoAttachment);	// Get the world space position of the hit to use in damage calculations...

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
    template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

	Vector3     m_hitPosition;				// the position of the weapon impact, can be local to the hit object or a world position depending on the damage type
	Vector3     m_hitDirection;				// used to sync the direction of melee force to allow physics to behave the same on all machines
	u32			m_damageFlags;				// Damage flags
	u32         m_weaponHash;           	// the weapon type
	u32         m_weaponDamage;         	// the weapon damage
	u8			m_weaponDamageAggregationCount; //The number of events this event represents via aggregation by operator==
	u32         m_playerHealthTimestamp;   	// the timestamp of the health of a player being hit (passed in the reply)
	u32         m_damageTime;           	// the time of the damage
	u32		    m_actionResultId;			// ID of melee action result (used to get correct hit reaction).
	u32			m_nForcedReactionDefinitionID; // ID of hit reaction to play.
    int         m_component;            	// the component of the ped being hit
	u16			m_playerDistance;			// the distance between the two players (used by DAMAGE_PLAYER events) 
	s16         m_playerHealth;         	// the health of a player being hit (passed in the reply)
	u16		    m_meleeId;					// Unique ID of melee action.
	ObjectId    m_parentID;             	// the object doing the hitting
	ObjectId    m_hitObjectID;          	// the object being hit
	s8          m_tyreIndex;            	// the index of the tyre damaged (only used for cars)
	s8          m_suspensionIndex;      	// the index of the wheel suspension damaged (only used for cars)
	u8          m_damageType;           	// object / vehicle / ped / player
	bool		m_hasHitDirection;			// used to find out if m_hitDirection should be used
	bool		m_hitEntityWeapon;			// If we hit the weapon of a ped (easiest with the sniper rifle) we still need to send the event over, even though the gun is a non-cloned local object
	bool		m_hitWeaponAmmoAttachment;	// If we hit the ammo box attached to a weapon we need to take that into account so we use the right object when setting things up over the network
	bool		m_silenced;
	bool		m_willKillPlayer;			// set when we think this damage event is going to kill the player it is being sent to
	bool		m_playerNMAborted;			// set when we have to abort the remote players local NM reaction
	bool		m_rejected;					// set when a player we are shooting rejects the event
	bool        m_bOverride;            	// if set the weapon is not the weapon held by the parent entity, and the m_weaponType and m_weaponDamage fields are used
	bool		m_firstBullet;
	bool		m_bOriginalHitObjectIsClone;
	bool		m_bVictimPlayer;

	mutable bool m_locallyTriggered;

    mutable u32 m_SystemTimeAdded;			// The system time this event was added - used to time events out
	
	PhysicalPlayerIndex m_killedPlayerIndex; // The physical player index of a player the event is killing

	struct PlayerDamageTime
    {
        PlayerDamageTime() : time(0) {}
        u32 time;
    };

    struct PlayerDelayedKillData
    {
        PlayerDelayedKillData() { Reset(); }

        bool IsSet(PhysicalPlayerIndex enemyPlayerIndex = INVALID_PLAYER_INDEX) { return (enemyPlayerIndex != INVALID_PLAYER_INDEX ? (m_enemyPlayerIndex == enemyPlayerIndex) : (m_enemyPlayerIndex != INVALID_PLAYER_INDEX)); }

        void Reset()
        {
            m_enemyPlayerIndex = INVALID_PLAYER_INDEX;
            m_weaponHash       = 0;
            m_fDamage          = 0.0f;
            m_hitComponent     = 0;
            m_time             = 0;
        }

        PhysicalPlayerIndex m_enemyPlayerIndex;
        u32					m_weaponHash;
        float				m_fDamage;
        int					m_hitComponent;
        u32					m_time;
    };

    // stores the times of the last damage events sent to other players that will kill them
    static  PlayerDamageTime        ms_aLastPlayerKillTimes[MAX_NUM_PHYSICAL_PLAYERS];

    // stores a kill pending on our player
    static  PlayerDelayedKillData   ms_delayedKillData;

    bool DoBasicRejectionChecks(const CPed *fromPlayerPed, const CPed *toPlayerPed, netObject *&pParentObject, netObject *&pHitObject);
    bool CheckForDelayedKill(const netObject *pParentObject, const netObject *pHitObject);
    bool ModifyDamageBasedOnWeapon(const netObject *parentObject, bool *shouldReply);
	void HandlePlayerShootingPed(const netPlayer &shootingPlayer, netObject *hitObject, bool damageFailed);

    // used to delay kills and stop double kills when two players shoot each other at roughly the same time
    static  void RegisterDelayedKill(PhysicalPlayerIndex enemyPlayerIndex, u32 weaponHash, float fDamage, int hitComponent);
    static  bool IsDelayedKillPending(PhysicalPlayerIndex killingPlayerIndex = INVALID_PLAYER_INDEX);
    static  void ApplyDelayedKill();

	static const float		MELEE_RANGE_SQ;

#if ENABLE_NETWORK_LOGGING
    bool        m_bNoParentObject    : 1;    // the parent object is not cloned on this machine
    bool        m_bNoHitObject       : 1;    // the hit object is not cloned on this machine
    bool        m_bHitObjectIsClone	 : 1;    // the hit object is not owned by this machine
	bool		m_bPlayerResurrected : 1;	 // the local player was resurrected before this event
	bool		m_bPlayerTooFarAway  : 1;	 // the local player is too far away from the damage position

    void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CRequestPickupEvent - sent by a non-host requesting to pick up a pickup
// ================================================================================================================
class CRequestPickupEvent : public netGameEvent
{
public:

    enum { EVENT_TIMEOUT = 250 }; // the length of time the event stays on the queue before being removed

public:
    CRequestPickupEvent( );
	CRequestPickupEvent( const CPed* pPed, const CPickup* pPickup);
	CRequestPickupEvent( const CPed* pPed, const CPickupPlacement* pPickupPlacement);
    ~CRequestPickupEvent();

    const char* GetEventName() const { return "REQUEST_PICKUP_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPed* pPed, const CPickup* pPickup);
	static void Trigger(const CPed* pPed, const CPickupPlacement* pPickupPlacement);

    virtual bool IsInScope( const netPlayer& player ) const;
	virtual bool HasTimedOut() const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;
	virtual bool MustPersistWhenOutOfScope() const { return true; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile() const;
    virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
    template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

    ObjectId		m_pedId;            // the id of the ped requesting to collect the pickup
    ObjectId		m_pickupId;         // the id of the pickup or placement being collected
	unsigned		m_failureCode;		// the failure code, set when the pickup rejects the request
    mutable int     m_timer;            // a timer used to time out the event if it has been on the queue too long
	bool			m_bPlacement;       // if true the ped is requesting to collect a pickup placement
	bool			m_bGranted;         // if true player is allowed to pick up the pickup
	mutable bool    m_locallyTriggered;
};

// ================================================================================================================
// CRequestMapPickupEvent - sent by a non-host requesting to pick up a pickup
// ================================================================================================================
class CRequestMapPickupEvent : public netGameEvent
{
public:

	enum { EVENT_TIMEOUT = 250 }; // the length of time the event stays on the queue before being removed

public:
	CRequestMapPickupEvent( const CPickupPlacement* pPlacement = NULL);
	~CRequestMapPickupEvent();

	const char* GetEventName() const { return "REQUEST_MAP_PICKUP_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPickupPlacement* pPlacement);

	virtual bool IsInScope( const netPlayer& player ) const;
	virtual bool HasTimedOut() const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
	virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	enum eRejectReasonTypes
	{
		REJECT_NONE,
		REJECT_PLACEMENT_NETWORKED,
		REJECT_PENDING_COLLECTION
	};
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

	CPickupPlacement* GetPlacement() const;

	CGameScriptObjInfo	m_placementInfo;			// the script info of the map placement being collected
	mutable int			m_timer;					// a timer used to time out the event if it has been on the queue too long
	bool				m_bGranted;					// if true a player has allowed colletion of the placement
	bool				m_bGrantedByAllPlayers;     // if true all players have allowed to pick up the placement
	mutable bool		m_bLocallyTriggered;		// if true this event was triggered locally
#if ENABLE_NETWORK_LOGGING
	u32					m_rejectReason;				// the reason why the request wasn't granted (for logging)
#endif
};

// ================================================================================================================
// CDebugGameClockEvent - sent by a host informing players of game clock
// ================================================================================================================

#if !__FINAL
class CDebugGameClockEvent : public netGameEvent
{
public:

    enum
    {
		SIZEOF_DAY      = 5,  //2^5 = 32 need 31
		SIZEOF_MONTH	= 4,  //2^4 = 16 need 12
		SIZEOF_YEAR		= 12, //2^12 = 4096 need up to year 3000
        SIZEOF_HOURS	= 6,
        SIZEOF_MINUTES	= 7,
        SIZEOF_SECONDS	= 7,
    };

    enum 
    {
        FLAGS_NONE                  = 0,              
        FLAG_IS_OVERRIDE            = BIT0,
        FLAG_IS_DEBUG_CHANGE        = BIT1,
        FLAG_IS_DEBUG_MAINTAIN      = BIT2,
        FLAG_IS_DEBUG_RESET         = BIT3,
        NUM_FLAGS                   = 4,
    };

public:
	CDebugGameClockEvent();
	CDebugGameClockEvent(unsigned nFlags, const netPlayer* pPlayer);

    const char* GetEventName() const { return "GAME_CLOCK_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(unsigned nFlags, const netPlayer* pPlayer = NULL);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    void WriteDataToLogFile(bool bEventLogOnly) const;

    const netPlayer* m_pPlayer;
	u32 m_day;
	u32 m_month;
	u32 m_year;
    u32 m_hours;
    u32 m_minutes;
    u32 m_seconds;
    unsigned m_nFlags;
    unsigned m_nSendTime;
};
#endif

// ================================================================================================================
// CDebugGameWeatherEvent - sent by a host informing players of the current weather setup
// ================================================================================================================

#if !__FINAL
class CDebugGameWeatherEvent : public netGameEvent
{

public:

	CDebugGameWeatherEvent();
	CDebugGameWeatherEvent(bool bForced, u32 nPrevIndex, u32 nCycleIndex, const netPlayer* pPlayer);

	const char* GetEventName() const { return "GAME_WEATHER_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(bool bForced, u32 nPrevIndex, u32 nCycleIndex, const netPlayer* pPlayer = NULL);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

#if ENABLE_NETWORK_LOGGING
    void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING

	const netPlayer* m_pPlayer;
	bool m_bIsForced;
	u32 m_nPrevIndex;
	u32 m_nCycleIndex;
};
#endif

// ================================================================================================================
// CRespawnPlayerPedEvent - sent from a client to the host when they have resurrected themselves
// ================================================================================================================

class CRespawnPlayerPedEvent : public netGameEvent
{
public:
	static const unsigned SIZEOF_TIMESTAMP = 32;

	//Interval that we can wait for the respawns object id to be created locally,
	//  after that we give up.
	static const unsigned RESPAWN_PED_CREATION_INTERVAL = 6000;


public:
    CRespawnPlayerPedEvent( );
    CRespawnPlayerPedEvent( const Vector3 &spawnPoint, u32 timeStamp, const ObjectId respawnNetObjId, u32 timeRespawnNetObj, bool resurrectPlayer, bool enteringMPCutscene, bool hasMoney, bool hasScarData, float u, float v, float scale);

    const char* GetEventName() const { return "RESPAWN_PLAYER_PED_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const Vector3 &spawnPoint, const ObjectId respawnNetObjId, u32 timeRespawnNetObj, bool resurrectPlayer, bool enteringMPCutscene, bool hasMoney = true, bool hasScarData = false, float u = 0.f, float v = 0.f, float scale = 0.f);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
	virtual void WriteReplyToLogFile(bool wasSent) const;
#endif //ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

    void UpdateRespawnScopeFlags();

    Vector3     m_spawnPoint;
    u32         m_timeStamp;
    bool        m_bShouldReply;
	ObjectId    m_RespawnNetObjId;
	u32         m_timeRespawnNetObj;
    u32         m_LastFrameScopeFlagsUpdated; // last frame the respawn scope flags were updated (we only want to do this once per frame)
	float		m_scarU;
	float		m_scarV;
	float		m_scarScale;
    PlayerFlags m_RespawnScopeFlags;          // indicates which players the respawn ped to be created is in scope with
    bool        m_ResurrectPlayer;            // indicates that the player should be resurrected
	bool        m_EnteringMPCutscene;         // indicates this player is moving to an MP cutscene
	bool        m_RespawnFailed;              // indicates if the respawn ped was created.
	bool		m_HasMoney;					  // If the player has money - used to determine if remote blood should be promoted to scars or not
	bool		m_HasScarData;
};

// ================================================================================================================
// CGiveWeaponEvent - sent from the host to the owner of a ped giving it a weapon
// ================================================================================================================

class CGiveWeaponEvent : public netGameEvent
{
public:

    CGiveWeaponEvent( );
    CGiveWeaponEvent( CPed* pPed, u32 weaponHash, int ammo, bool bAsPickup);

    const char* GetEventName() const { return "GIVE_WEAPON_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CPed* pPed, u32 weaponHash, int ammo, bool bAsPickup = false);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

public:
    static const unsigned int SIZEOF_WEAPON = 32;
    static const unsigned int SIZEOF_AMMO   = 16;

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_pedID;
    u32      m_weaponHash;
    int      m_ammo;
    bool     m_bAsPickup;

#if ENABLE_NETWORK_LOGGING
    void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CRemoveWeaponEvent - sent from the host to the owner of a ped removing a weapon
// ================================================================================================================

class CRemoveWeaponEvent : public netGameEvent
{
public:

    CRemoveWeaponEvent( );
    CRemoveWeaponEvent( CPed* pPed, u32 weaponHash );

    const char* GetEventName() const { return "REMOVE_WEAPON_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CPed* pPed, u32 weaponHash);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

private:

    static const unsigned int SIZEOF_WEAPON = 32;

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_pedID;
    u32      m_weaponHash;
};

// ================================================================================================================
// CRemoveAllWeaponsEvent - sent from the host to the owner of a ped all weapons
// ================================================================================================================

class CRemoveAllWeaponsEvent : public netGameEvent
{
public:

    CRemoveAllWeaponsEvent( );
    CRemoveAllWeaponsEvent( CPed* pPed);

    const char* GetEventName() const { return "REMOVE_ALL_WEAPONS_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CPed* pPed);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_pedID;
};

// ================================================================================================================
// CVehicleComponentControlEvent - used when a ped wants to request or release control of a component (a door or seat) on a remote vehicle
// ================================================================================================================

class CVehicleComponentControlEvent : public netGameEvent
{
public:

    CVehicleComponentControlEvent( );
    CVehicleComponentControlEvent(  const ObjectId   vehicleID,
                                    const ObjectId   pedID,
                                    s32              componentIndex,
                                    bool             bRequest,
                                    const netPlayer *pOwnerPlayer);

    const char* GetEventName() const { return "VEHICLE_COMPONENT_CONTROL_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const ObjectId vehicleId, const ObjectId pedId, s32 componentIndex, bool bRequest, const netPlayer* pOwnerPlayer = NULL);

    virtual bool IsInScope( const netPlayer& player ) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    virtual void WriteDecisionToLogFile() const;
    virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

private: //utilities

		  CComponentReservationManager* GetComponentReservationMgr() const;
	const CModelSeatInfo*				GetModelSeatInfo() const;
	const CSeatManager*					GetSeatManager() const;

private:

    static const unsigned SIZEOF_COMPONENT = 5;

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
    template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

    ObjectId         m_vehicleID;        // the vehicle whose door we are changing control of
    ObjectId         m_pedID;            // the ped trying to enter the vehicle
    ObjectId         m_pedInSeatID;      // the ped sitting in a seat that is granted
    u8               m_componentIndex;       // the component on the vehicle
    const netPlayer* m_pOwnerPlayer;         // the player which owns the vehicle
    bool             m_bRequest;             // if true control is requested, if false it is being released
    bool             m_bGranted;             // control of the component is granted
    bool             m_bComponentIsASeat;    // set if the component is a seat
	bool             m_bReservationFailed;   // set if the reservation fails to get applied
    bool             m_bPedOwnerWrong;       // set if we think ped is owned by a different machine
    bool             m_bPedInSeatWrong;      // set if the ped we think is in a seat differs from the owner machine
    bool             m_bPedInSeatNotCloned;  // set if the ped in the seat has not been cloned on the machine requesting the component
    bool             m_bPedStillLeaving;     // set if the ped in the seat is still leaving
	bool             m_bPedStillVaulting;     // set if the ped is still vaulting

};

// ================================================================================================================
// CFireEvent - triggered when a player wants to create a new fire
// ================================================================================================================
class CFireEvent : public netGameEvent
{
public:

	struct sFireData
	{
	public:

		sFireData() :
		  m_firePosition   (Vector3(0.0f, 0.0f, 0.0f)),
			  m_parentPosition (Vector3(0.0f, 0.0f, 0.0f)),
			  m_fireType       (FIRETYPE_TIMED_MAP),
			  m_burningEntityID(NETWORK_INVALID_OBJECT_ID),
			  m_ownerID        (NETWORK_INVALID_OBJECT_ID),
			  m_isScripted     (false),
			  m_maxGenerations (0),
			  m_burnTime       (0),
			  m_burnStrength   (0),
			  m_weaponHash	 (0)
		  {
		  }

		  sFireData(const sFireData &fireData) :
		  m_firePosition   (fireData.m_firePosition),
			  m_parentPosition (fireData.m_parentPosition),
			  m_fireType       (fireData.m_fireType),
			  m_burningEntityID(fireData.m_burningEntityID),
			  m_ownerID        (fireData.m_ownerID),
			  m_isScripted     (fireData.m_isScripted),
			  m_maxGenerations (fireData.m_maxGenerations),
			  m_burnTime       (fireData.m_burnTime),
			  m_burnStrength   (fireData.m_burnStrength),
			  m_netIdentifier  (fireData.m_netIdentifier),
			  m_weaponHash	 (fireData.m_weaponHash)
		  {
		  }

		  sFireData(const Vector3&                  firePosition,
			  const Vector3&			  parentPosition,
			  FireType_e                  fireType,
			  const ObjectId              burningEntityID,
			  const ObjectId              ownerID,
			  bool                        isScripted,
			  u32                         maxGenerations,
			  float                       burnTime,
			  float                       burnStrength,
			  const CNetFXIdentifier&     identifier,
			  u32						  weaponHash) :
		  m_firePosition   (firePosition),
			  m_parentPosition (parentPosition),
			  m_fireType       (fireType),
			  m_burningEntityID(burningEntityID),
			  m_ownerID        (ownerID),
			  m_isScripted     (isScripted),
			  m_maxGenerations (static_cast<u8>(maxGenerations)),
			  m_burnTime       (burnTime),
			  m_burnStrength   (burnStrength),
			  m_netIdentifier  (identifier),
			  m_weaponHash	 (weaponHash)
		  {
		  }

		  Vector3                m_firePosition;    // position of the fire
		  Vector3				 m_parentPosition;  // parent position
		  FireType_e             m_fireType;        // type of fire
		  ObjectId               m_burningEntityID; // entity fire is attached to (can be NULL)
		  ObjectId               m_ownerID;         // instigator of the fire
		  u8                     m_maxGenerations;  // maximum number of generations of fire to create
		  float                  m_burnTime;        // burn time for the fire
		  float                  m_burnStrength;    // burn strength for the fire
		  CNetFXIdentifier       m_netIdentifier;   // identifies the fire across the network
		  bool                   m_isScripted;      // indicates whether the fire was created by a script
		  u32					m_weaponHash;	   // the weapon started the fire
	};

	static const unsigned int MAX_NUM_FIRE_DATA    = 5;
	static const unsigned int SIZEOF_NUM_FIRE_DATA = datBitsNeeded<MAX_NUM_FIRE_DATA>::COUNT;

public:

	CFireEvent();

	const char* GetEventName() const { return "FIRE_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CFire& fire);

	bool CanAddFireData(const sFireData &fireData);
	void AddFireData(const sFireData &fireData);

	bool IsInScope( const netPlayer& player ) const;
	virtual bool HasTimedOut() const;
	void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool MustPersist() const { return false; }

	void SetTimedOut() { m_TimedOut=true; }
	bool GetFireOwner() { return m_fireOwner; }

#if ENABLE_NETWORK_LOGGING
	void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	PlayerFlags GetPlayersInScopeForFireData(const sFireData &fireData);

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const float        FIRE_CLONE_RANGE_SQ;
	static const float        MAX_BURN_STRENGTH;
	static const float        MAX_BURN_TIME;
	static const unsigned int SIZEOF_FIRE_TYPE          = datBitsNeeded<FIRETYPE_MAX>::COUNT;
	static const unsigned int SIZEOF_MAX_GENERATIONS    = datBitsNeeded<FIRE_DEFAULT_NUM_GENERATIONS>::COUNT;

	u32                 m_numFires;
	sFireData           m_fireData[MAX_NUM_FIRE_DATA];
	mutable PlayerFlags m_playersInScope;
	bool                m_fireOwner;                        // set to true if this event was generated locally
	bool				m_TimedOut;

#if ENABLE_NETWORK_LOGGING
	void WriteDataToLogFile(u32 fireIndex, bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CExplosionEvent - creates an explosion over the network
// ================================================================================================================
class CExplosionEvent : public netGameEvent
{
public:

    CExplosionEvent();
    CExplosionEvent(CExplosionManager::CExplosionArgs& explosionArgs, CProjectile* pProjectile);

	virtual ~CExplosionEvent();

    const char* GetEventName() const { return "EXPLOSION_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CExplosionManager::CExplosionArgs& explosionArgs, CProjectile* pProjectile);

	void CompensateForVehicleVelocity(CEntity* pAttachEntity);

    bool IsInScope( const netPlayer& player ) const;
	virtual bool HasTimedOut() const;
    void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    void Handle  ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
    void WriteDecisionToLogFile() const;
 #endif // ENABLE_NETWORK_LOGGING

protected:

    const ObjectId GetExplodingEntityID() const { return m_explodingEntityID; }

    bool CreateExplosion();

#if ENABLE_NETWORK_LOGGING
    void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
 
    CObject *GetObjectFromExplodingDummyPosition();

    static const unsigned int SIZEOF_EXPLOSION_TAG		= 6;     // number of bits to pack explosion tag in
	static const unsigned int SIZEOF_EXPLOSION_POSITION	= 22;    // number of bits to pack the position
	static const unsigned int SIZEOF_ACTIVATION_DELAY   = 16;    // number of bits to pack the activation delay value
    static const unsigned int SIZEOF_SIZE_SCALE         = 8;     // number of bits to pack the size scale value
    static const unsigned int SIZEOF_CAM_SHAKE          = 8;     // number of bits to pack the camera shake value
    static const unsigned int SIZEOF_INTERIOR_LOC       = 32;    // number of bits to pack the interior location value
	static const float        EXPLOSION_CLONE_RANGE_SQ;          // any other players within this distance need to have the explosion requested
	static const float        EXPLOSION_CLONE_RANGE_ENTITY_SQ;   // any other players within this distance of an exploding entity need to have the explosion requested
	static const float        EXPLOSION_CLONE_RANGE_SNIPER_SQ;   // any other players using a sniper scope within this distance need to have the explosion requested
	static const float        EXPLOSION_CLONE_RANGE_SNIPER_ENTITY_SQ;   // any other players using a sniper scope within this distance of an exploding entity need to have the explosion requested
	static const unsigned int SIZEOF_EXPLOSION_DIRECTION = 16;  //get best accuracy for this normal

    CExplosionManager::CExplosionArgs m_explosionArgs;       // all of the explosion parameters
    ObjectId                          m_explodingEntityID;   // ID of entity exploding
    ObjectId                          m_attachEntityID;      // ID of entity an explosion is attached to
    ObjectId                          m_entExplosionOwnerID; // ID of entity causing explosion
    ObjectId                          m_entIgnoreDamageID;	 // ID of entity marked to ignore damage
    CNetFXIdentifier                  m_projectileIdentifier;// the identifier of the projectile which caused this explosion
	Vector3							  m_dummyPosition;		 // the dummy position of the entity this explosion is attached to 
	bool							  m_bHasProjectile;       // if true there is a projectile associated with this explosion which must be removed
	bool							  m_bEntityExploded;      // if true the exploding entity has been set as exploded
	bool                              m_hasRelatedDummy;     // set to true if we are using the dummy position as the explosion position
	bool							  m_shouldAttach;		 // if true, use the dummy position to find object to attach to
	bool                              m_success;             // true if explosion was successfully created
 	mutable u32 m_SystemTimeAdded;      // The system time this event was added - used to time explosion events out if remote machine gets stuck before replying
};

// ================================================================================================================
// CStartProjectileEvent - sent by player to inform other players to create a new projectile
// ================================================================================================================
class CStartProjectileEvent : public netGameEvent
{
public:

    CStartProjectileEvent();

    CStartProjectileEvent(const ObjectId          projectileOwnerID,
                          u32                     projectileHash,
                          u32                     weaponFiredFromHash,
                          const Vector3          &initialPosition,
                          const Vector3          &fireDirection,
                          eWeaponEffectGroup      effectGroup,
                          const ObjectId          targetEntityID,
                          const ObjectId          projectileID,
						  const ObjectId		  ignoreDamageEntID,
						  const ObjectId		  noCollisionEntID,
                          const CNetFXIdentifier &identifier,
						  const bool bCommandFireSingleBullet,
						  bool		 bAllowDamping,
						  float		 fLaunchSpeedOverride,
						  bool		 bRocket,
						  bool		 bWasLockedOnWhenFired,
						  u8		 tintIndex,
						  u8		 fakeStickyBombSequence,
						  bool       syncOrientation,
						  Matrix34   projectileWeaponMatrix);

    virtual ~CStartProjectileEvent() {}

    const char* GetEventName() const { return "START_PROJECTILE_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(class CProjectile* pProjectile, const Vector3& fireDirection, bool bCommandFireSingleBullet = false, bool bAllowDampingfloat = true, float fLaunchSpeedOverride =-1.0f);

    bool IsInScope( const netPlayer& player ) const;
    void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    void Handle  ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    void MakeWeaponMatrix(Matrix34 &tempWeaponMatrix, bool bProjectile);
	bool MustPersist() const { return false; }

	const CVehicle* GetLaunchVehicle(const netObject *networkObject) const;
	bool CalculateMovingVehicleRelativeFirePoint(const netObject *networkObject); //returns true if vehicle exists and is moving fast enough to need compensation
	void ModifyInitialPosForVehicleRelativeFirePoint(const netObject *networkObject);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    static const float PROJECTILE_CLONE_RANGE_SQ;
	static const float PROJECTILE_CLONE_RANGE_SQ_EXTENDED;
    static const unsigned int SIZEOF_WEAPON_TYPE = 7;  // number of bits to pack explosion type in
    static const unsigned int SIZEOF_EFFECT_GROUP = datBitsNeeded<NUM_EWEAPONEFFECTGROUP>::COUNT;  // number of bits to effect group
	static const unsigned int SIZEOF_FIRE_DIRECTION = 16;  //get best accuracy for this normal
	static const unsigned int SIZEOF_THROW_LAUNCH_SPEED = 18;
	static const unsigned int SIZEOF_VEHICLE_OFFSET_DIRECTION = 16;  //

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId           m_projectileOwnerID;   // projectile owner (who fired it)
    u32                m_projectileHash;      // projectile type
    u32                m_weaponFiredFromHash;
	Vector3            m_vVehicleRelativeOffset;	// If fired from a moving vehicle this records the relative fire point of the projectile from the vehicle 
													// so the remote can compensate for movement and start the projectile at the nearest point along its direction to this point.
	Vector3            m_initialPosition;     // initial position
    Vector3            m_fireDirection;       // direction fired
    eWeaponEffectGroup m_effectGroup;         // projectile weapon effect group
    ObjectId           m_targetEntityID;      // target entity
    ObjectId           m_projectileID;        // projectile ID
	ObjectId		   m_ignoreDamageEntId;   // ignore damage entity ID
	ObjectId		   m_noCollisionEntId;    // no collision entity ID
    bool               m_coordinatedWithTask; // has this projectile come from a task that requires coordinating projectile timing
    u32                m_coordinateWithTaskSequence;   // the sequence of the coordinated task
    CNetFXIdentifier   m_netIdentifier;       // the projectile's network identifier
	u8				   m_tintIndex;			  // projectile tint index
	Matrix34           m_projectileWeaponMatrix; // weapon matrix for the projectile, to be used only for bombs

	// If sticky bomb is fired by command, it won't have sequence id so we create our own sequence
	u8				   m_vehicleFakeSequenceId;
	
	bool			   m_bCommandFireSingleBullet;	//true if started by CommandFireSingleBullet - use the matrix derived from start and direction 
													//as ped may be co-pilot and have no weapon equipped of its own
	bool			   m_bVehicleRelativeOffset;	//If fired from a vehicle platform that is moving fast enough this signifies that m_vVehicleRelativeOffset has been
													// has been calculated to allow relative vehicle compensation to move m_initialPosition along the fire direction
	bool			   m_bAllowDamping;				//For thrown projectiles
	bool			   m_bWasLockedOnWhenFired;		//When rocket fired from homing launcher need to know whether we track the passed target
	float			   m_fLaunchSpeedOverride;      //For thrown projectiles
	bool               m_bSyncOrientation;          //Custom sync-ing the projectile orientation for big ordinances (plane-dropped bombs) 
#if ENABLE_NETWORK_LOGGING
    void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CUpdateProjectileTargetEntity - sent by player to inform other players to modify a homing projectile's target entity
// ================================================================================================================
class CUpdateProjectileTargetEntity : public netGameEvent
{
public:
	CUpdateProjectileTargetEntity();
	CUpdateProjectileTargetEntity(const ObjectId projectileOwnerID,
								  u32 projectileSequenceId,
								  s32 targetFlareGunSequenceId);

	virtual ~CUpdateProjectileTargetEntity() {}
	virtual bool operator==(const netGameEvent* pEvent) const;

	const char* GetEventName() const { return "UPDATE_PROJECTILE_TARGET_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(class CProjectile* pProjectile, s32 targetFlareGunSequenceId);

	bool IsInScope( const netPlayer& player ) const;
	void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	void Handle  ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_projectileOwnerID;				// projectile owner (who fired it)
	s32      m_projectileSequenceId;			// projectile type
	s32		 m_targetFlareGunSequenceId;	// flare gun owner's projectile target sequence id
	
#if ENABLE_NETWORK_LOGGING
	void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CBreakProjectileTargetLock - sent by player to inform other players to stop homing on projectile's target entity
// ================================================================================================================
class CBreakProjectileTargetLock : public netGameEvent
{
public:
	CBreakProjectileTargetLock();
	CBreakProjectileTargetLock(CNetFXIdentifier projectileId);

	virtual ~CBreakProjectileTargetLock() {}
	virtual bool operator==(const netGameEvent* pEvent) const;

	const char* GetEventName() const { return "BREAK_PROJECTILE_TARGET_LOCK_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CNetFXIdentifier projectileId);

	bool IsInScope( const netPlayer& player ) const;
	void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	void Handle  ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CNetFXIdentifier   m_netIdentifier;       // the projectile's network identifier
#if ENABLE_NETWORK_LOGGING
	void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CRemoveProjectileEntity - sent by player to inform other players to modify a homing projectile's target entity
// ================================================================================================================
class CRemoveProjectileEntity : public netGameEvent
{
public:
	CRemoveProjectileEntity();
	CRemoveProjectileEntity(const ObjectId projectileOwnerID,
		u32 projectileSequenceId);

	virtual ~CRemoveProjectileEntity() {}
	virtual bool operator==(const netGameEvent* pEvent) const;

	const char* GetEventName() const { return "REMOVE_PROJECTILE_ENTITY_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(ObjectId pProjectileId, u32 taskSequence);

	bool IsInScope( const netPlayer& player ) const;
	void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	void Handle  ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_projectileOwnerID;				// projectile owner (who fired it)
	s32      m_projectileSequenceId;			// projectile sequence id
#if ENABLE_NETWORK_LOGGING
	void WriteDataToLogFile(bool bEventLogOnly) const;
#endif // ENABLE_NETWORK_LOGGING
};

// ================================================================================================================
// CAlterWantedLevelEvent - sent when one machine wants to alter the wanted level of a cloned player
// ================================================================================================================

class CAlterWantedLevelEvent : public netGameEvent
{
public:

    enum
    {
        SIZEOF_WANTEDLEVEL  = 14,
        SIZEOF_CRIMETYPE    = datBitsNeeded<MAX_CRIMES>::COUNT,
    };

public:
    CAlterWantedLevelEvent( );
    CAlterWantedLevelEvent( const netPlayer& player, eCrimeType crimeType, bool bPoliceDontCare, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);
    CAlterWantedLevelEvent( const netPlayer& player, u32 newWantedLevel, u32 delay, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);

    const char  *GetEventName()    const { return "ALTER_WANTED_LEVEL_EVENT"; }
    const netPlayer* GetPlayer() { return m_pPlayer; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const netPlayer& player, u32 newWantedLevel, s32 delay, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);
    static void Trigger(const netPlayer& player, eCrimeType crimeType, bool bPoliceDontReallyCare, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);

    bool IsCrimeCommittedEvent() { return m_crimeType != CRIME_NONE; }

    bool IsEqual(const netPlayer& player, eCrimeType crimeType, bool bPoliceDontCare);
    bool IsEqual(const netPlayer& player, u32 newWantedLevel);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    const netPlayer* m_pPlayer;
    bool             m_bSetWantedLevel;
    u32              m_crimeType;
    u32              m_newWantedLevel;
	u32				 m_delay;
    bool             m_bPoliceDontCare;
	PhysicalPlayerIndex m_causedByPlayerPhysicalIndex;
};

// ================================================================================================================
// CChangeRadioStationEvent - sent from the client when he wants to change the radio on the owner
// ================================================================================================================
struct audRadioStationSyncNextTrack;

class CChangeRadioStationEvent : public netGameEvent
{
public:

    CChangeRadioStationEvent( );
    CChangeRadioStationEvent( const ObjectId vehicleId, const u8 stationIndex );

    const char* GetEventName() const { return "CHANGE_RADIO_STATION_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const CVehicle* pVehicle, const u8 stationIndex);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_vehicleId;
	u8 m_stationId;
};

// ================================================================================================================
// CRagdollRequestEvent - sent when the clone ped wants to influence the main ped's radgoll behaviour
// ================================================================================================================
class CRagdollRequestEvent : public netGameEvent
{
public:

    CRagdollRequestEvent( );
    CRagdollRequestEvent( const ObjectId pedId );

    const char* GetEventName() const { return "RAGDOLL_REQUEST_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const ObjectId pedID);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_pedId;
    bool     m_bSwitchedToRagdoll;
};

// ================================================================================================================
// CPlayerTauntEvent - sent when the local player does a taunt
// ================================================================================================================
class CPlayerTauntEvent : public netGameEvent
{
public:

    CPlayerTauntEvent();
    CPlayerTauntEvent(const char* context, s32 tauntVariationNumber);

    const char* GetEventName() const { return "PLAYER_TAUNT_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const char* context, s32 tauntVariationNumber);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    static const unsigned int MAX_CONTEXT_LENGTH     = 32;
    static const unsigned int SIZEOF_TAUNT_VARIATION = 32;

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    char m_context[MAX_CONTEXT_LENGTH];
    s32 m_tauntVariationNumber;
};

// ================================================================================================================
// CPlayerCardStatEvent - sent when stats for the player card are updated.
// ================================================================================================================
class CPlayerCardStatEvent : public netGameEvent
{
public:
	CPlayerCardStatEvent();
	CPlayerCardStatEvent(const float data[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT]);// Floats * 100 casted to ints, casted back after being networked (using less bits), ints stay as ints.

	const char* GetEventName() const { return "PLAYER_CARD_STAT_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const float data[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT]);

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

	bool Decide (const netPlayer& fromPlayer, const netPlayer& toPlayer);

	virtual bool IsInScope( const netPlayer& player) const {return !player.IsLocal(); }

private:

	float m_stats[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT];
};

// ================================================================================================================
// CDoorBreakEvent - sent when a door breaks off on a clone
// ================================================================================================================
class CCarDoor;
class CDoorBreakEvent : public netGameEvent
{
public:

    enum
    {
        SIZEOF_DOOR = 3
    };

public:

    CDoorBreakEvent();
    CDoorBreakEvent(const ObjectId vehicleId, u32 door);

    const char* GetEventName() const { return "DOOR_BREAK_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const CVehicle* pVehicle, const CCarDoor* pDoor);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_vehicleId;
    u8       m_door;
};

// ================================================================================================================
// CScriptedGameEvent - Events sent by the scripters
// ================================================================================================================
class CScriptedGameEvent : public netGameEvent
{
public:
	static const unsigned SIZEOF_DATA = MAX_SCRIPTED_EVENT_DATA_SIZE;

	static const unsigned TELEMETRY_THRESHOLD = 100;
	static const unsigned HISTORY_QUEUE_SIZE = 20;
	static unsigned NUM_VALID_CALLING_SCRIPTS;

private:

	struct sEventHistoryEntry
	{
		sEventHistoryEntry() : m_scriptNameHash(0), m_dataHash(0), m_sizeOfData(0), m_numTimesTriggered(0) {}
		sEventHistoryEntry(u32 scriptNameHash, u32 dataHash, u32 size) : m_scriptNameHash(scriptNameHash), m_dataHash(dataHash), m_sizeOfData(size), m_numTimesTriggered(1) {}

		bool operator==(const sEventHistoryEntry& entry)
		{
			return (m_scriptNameHash == entry.m_scriptNameHash && m_dataHash == entry.m_dataHash && entry.m_sizeOfData == m_sizeOfData);
		}

		u32	m_scriptNameHash;
		u32	m_dataHash;
		u32	m_sizeOfData;
		u32	m_numTimesTriggered;
	};

private:

	// the id of the script that triggered the event
	CGameScriptId m_scriptId;

	// Data sent.
	u8  m_Data[SIZEOF_DATA];

	// Player flags indicating which players the event is sent to.
	mutable u32  m_PlayerFlags;

	// Indicates the size of the data.
	unsigned  m_SizeOfData;

public:

	CScriptedGameEvent();
	CScriptedGameEvent(CGameScriptId& scriptId, void* data, const unsigned sizeOfData, const u32 playerFlags);

	const char* GetEventName() const { return "SCRIPTED_GAME_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);

	static void Trigger(CGameScriptId& scriptId, void* data, const unsigned sizeOfData, const u32 playerFlags);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
	virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

	static void ClearEventHistory() { ms_numEventsInHistory = 0; }

	CGameScriptId GetScriptId() { return m_scriptId; }

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	void SetPlayer(const PhysicalPlayerIndex playerIndex)
	{
		gnetAssert(!(m_PlayerFlags &(1<<playerIndex)));
		m_PlayerFlags |= (1<<playerIndex);
	}

	bool SendToPlayer(const PhysicalPlayerIndex playerIndex) const
	{
		return (m_PlayerFlags & (1<<playerIndex)) != 0;
	}

	int  GetSizeInBits() const { return m_SizeOfData*8; }
	const char* GetScriptedEventTypeName(const unsigned eventType) const;

	// event history / event spam telemetry
	static u32 AddHistoryItem(CGameScriptId& scriptId, void* data, const unsigned sizeOfData);
	static int SortEventHistory(const void *paramA, const void *paramB);
	static u32 CalculateDataHash(u8* data, const unsigned sizeOfData);
	static void SendScriptSpamTelemetry(const int eventType);

#if __DEV
	u32 m_timeCreated;
#endif

	static sEventHistoryEntry ms_eventHistory[HISTORY_QUEUE_SIZE];
	static u32 ms_numEventsInHistory;

	static u32 ms_validCallingScriptsHashes[];
};

// ================================================================================================================
// CRemoteScriptInfoEvent - sent from the host of a script session informing non-participants about the script
// ================================================================================================================
class CRemoteScriptInfoEvent : public netGameEvent
{
public:

    CRemoteScriptInfoEvent();
    CRemoteScriptInfoEvent(const CGameScriptHandlerMgr::CRemoteScriptInfo& scriptInfo, PlayerFlags players);

    const char* GetEventName() const { return "REMOTE_SCRIPT_INFO_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static bool Trigger(const CGameScriptHandlerMgr::CRemoteScriptInfo& scriptInfo, PlayerFlags players);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	
	void AddTargetPlayers(PlayerFlags players);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    CGameScriptHandlerMgr::CRemoteScriptInfo    m_scriptInfo;
	mutable PlayerFlags                         m_players;			// the players the event has to be sent to
	bool										m_ignored;
 };

// ================================================================================================================
// CRemoteScriptLeaveEvent - sent by a participant of a script session informing non-participants that they are leaving
// ================================================================================================================
class CRemoteScriptLeaveEvent : public netGameEvent
{
public:

    CRemoteScriptLeaveEvent();
    CRemoteScriptLeaveEvent(const CGameScriptId& scriptInfo, PlayerFlags players);

    const char* GetEventName() const { return "REMOTE_SCRIPT_LEAVE_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const CGameScriptId& scriptInfo, PlayerFlags players);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    CGameScriptId   m_scriptId;
    PlayerFlags     m_players;       // the players the event has to be sent to
    BANK_ONLY(bool            m_bSuccess);      // set when we have successfully remove the player from the script
#if __DEV
    u32             m_aliveTime;     // how long this event has been on the queue. Used to sanity check that it is getting processed properly at the receiving end
#endif
};

// ================================================================================================================
// CMarkAsNoLongerNeededEvent - sent when the host marks a script object as no longer needed for an object owned by a remote player
// ================================================================================================================
class CMarkAsNoLongerNeededEvent : public netGameEvent
{
public:

	CMarkAsNoLongerNeededEvent( );

	bool AddObjectID(const ObjectId &objectID, bool toBeDeleted);

	const char* GetEventName() const { return "MARK_AS_NO_LONGER_NEEDED_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CNetObjGame& object, bool toBeDeleted = false);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool bSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	struct sObjectInfo
	{
		ObjectId objectId;
		bool toBeDeleted;
	};

private:

	static const unsigned int SIZEOF_NUM_OBJECTS     = 4;
	static const unsigned int MAX_OBJECTS_PER_EVENT = ((1<<SIZEOF_NUM_OBJECTS) - 1);

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u32			m_numScriptObjects;
	sObjectInfo m_scriptObjects[MAX_OBJECTS_PER_EVENT];
	bool		m_objectsForDeletion;
};

// ================================================================================================================
// CConvertToScriptEntityEvent - sent when the host wants to convert an ambient entity owned by a remote player into a script entity
// ================================================================================================================
class CConvertToScriptEntityEvent : public netGameEvent
{
public:

	CConvertToScriptEntityEvent();
	CConvertToScriptEntityEvent(const CNetObjPhysical& object, const CGameScriptObjInfo& scriptInfo);
	~CConvertToScriptEntityEvent();

	const char* GetEventName() const { return "CONVERT_TO_SCRIPT_ENTITY_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CNetObjPhysical& object);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return true; }
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool bSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	bool ConvertToScriptEntity();

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId			m_entityId;
	CGameScriptObjInfo	m_scriptObjInfo;
};

// ================================================================================================================
// CScriptWorldStateEvent - sent when the host of a script disables road nodes to all players in the session. This
//                       ensures even players who are not running the same script disable the specified nodes
// ================================================================================================================
class CScriptWorldStateEvent : public netGameEvent
{
public:

    CScriptWorldStateEvent();
    CScriptWorldStateEvent(const CNetworkWorldStateData &worldStateData,
                           bool                          changeState,
                           const netPlayer              *targetPlayer);

    ~CScriptWorldStateEvent();

    const char* GetEventName() const { return "SCRIPT_WORLD_STATE_EVENT"; }

    static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CNetworkWorldStateData &worldStateData,
                        bool                          changeState,
                        const netPlayer              *targetPlayer = 0);

    virtual bool IsInScope(const netPlayer &player) const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    void AddTargetPlayer(const netPlayer &targetPlayer);

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    CNetworkWorldStateData *m_WorldStateData;
    bool                    m_ChangeState;
    PlayerFlags             m_PlayersInScope;
};

// ================================================================================================================
// CIncidentEntityEvent - sent when a dispatch order entity arrives or is finished with an incident controlled by another machine
// ================================================================================================================
class CIncidentEntityEvent : public netGameEvent
{
public:

	enum eEntityAction
	{
		ENTITY_ACTION_ARRIVED,
		ENTITY_ACTION_FINISHED,
		NUM_ENTITY_ACTIONS
	};

public:

	CIncidentEntityEvent( );
	CIncidentEntityEvent( unsigned incidentID, unsigned dispatchType, eEntityAction entityAction );
	CIncidentEntityEvent( unsigned incidentID, bool hasBeenAddressed );

	const char* GetEventName() const { return "INCIDENT_ENTITY_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(unsigned incidentIndex, DispatchType dispatchType, eEntityAction entityAction);
	static void Trigger(unsigned incidentIndex, bool hasBeenAddressed);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;
	virtual bool MustPersistWhenOutOfScope() const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool bSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int SIZEOF_DISPATCH_TYPE  = datBitsNeeded<DT_Max-1>::COUNT;
	static const unsigned int SIZEOF_ENTITY_ACTION  = datBitsNeeded<NUM_ENTITY_ACTIONS-1>::COUNT;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	void TriggerEntityAction(CIncident* pIncident) const;

	u32					m_incidentID;
	u32					m_dispatchType;
	eEntityAction		m_entityAction;
	bool				m_hasBeenAddressed;
};

// ================================================================================================================
// CClearAreaEvent - sent when a player wishes to clear an area of ambient population
// ================================================================================================================
class CClearAreaEvent : public netGameEvent
{
public:

	enum 
	{
		CLEAR_AREA_FLAG_ALL = BIT(0),
		CLEAR_AREA_FLAG_PEDS = BIT(1),
		CLEAR_AREA_FLAG_VEHICLES = BIT(2),
		CLEAR_AREA_FLAG_OBJECTS = BIT(3),
		CLEAR_AREA_FLAG_COPS = BIT(4),
		CLEAR_AREA_FLAG_PROJECTILES = BIT(5),
		CLEAR_AREA_FLAG_CARGENS = BIT(6),
		CLEAR_AREA_FLAG_WRECKED_STATUS = BIT(7),
		CLEAR_AREA_FLAG_ABANDONED_STATUS = BIT(8),
		CLEAR_AREA_FLAG_CHECK_ALL_PLAYERS_FRUSTUMS = BIT(9),
		CLEAR_AREA_FLAG_ENGINE_ON_FIRE = BIT(10),
		CLEAR_AREA_NUM_FLAGS = 11
	};

public:

	CClearAreaEvent();
	CClearAreaEvent(const CGameScriptId& scriptId, const Vector3& areaCentre, float areaRadius, u32 flags);

	const char* GetEventName() const { return "CLEAR_AREA_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CGameScriptId& scriptId, const Vector3& areaCentre, float areaRadius, u32 flags);

	virtual bool IsInScope(const netPlayer &player) const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned MAX_AREA_RADIUS = 16384;
	static const unsigned SIZEOF_AREA_RADIUS = datBitsNeeded<MAX_AREA_RADIUS>::COUNT;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CGameScriptId	m_scriptId;
	Vector3			m_areaCentre;
	float			m_areaRadius;
	u32				m_flags;

#if __DEV
	u32				m_timeCreated;
#endif
};

// ================================================================================================================
// CClearRectangleAreaEvent - sent when a player wishes to clear an area of parking cars
// ================================================================================================================
class CClearRectangleAreaEvent : public netGameEvent
{
public:
	CClearRectangleAreaEvent();
	CClearRectangleAreaEvent(const CGameScriptId& scriptId, Vector3 minPos, Vector3 maxPos);

	const char* GetEventName() const { return "CLEAR_RECTANGLE_AREA_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CGameScriptId& scriptId, const Vector3* minPos, const Vector3* maxPos);

	virtual bool IsInScope(const netPlayer &player) const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING
private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CGameScriptId m_scriptId;
	Vector3		  m_minPosition;
	Vector3		  m_maxPosition;
	
#if __DEV
	u32				m_timeCreated;
#endif
};

// ================================================================================================================
// CRequestNetworkSyncedSceneEvent - sent when a player wishes to query a synchronised scene running on a remote machine
// ================================================================================================================
class CRequestNetworkSyncedSceneEvent : public netGameEvent
{
public:

	CRequestNetworkSyncedSceneEvent();
    CRequestNetworkSyncedSceneEvent(int sceneID, netPlayer &targetPlayer);

	const char* GetEventName() const { return "REQUEST_NETWORK_SYNCED_SCENE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int sceneID, netPlayer &targetPlayer);

	virtual bool IsInScope(const netPlayer &player) const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	int                 m_SceneID;
    PhysicalPlayerIndex m_TargetPlayerID;
};

// ================================================================================================================
// CStartNetworkSyncedSceneEvent - sent when a player wishes to start a synchronised scene on other players machines
// ================================================================================================================
class CStartNetworkSyncedSceneEvent : public netGameEvent
{
public:

	CStartNetworkSyncedSceneEvent();
    CStartNetworkSyncedSceneEvent(int sceneID, unsigned networkTimeStarted, const netPlayer *targetPlayer);
    ~CStartNetworkSyncedSceneEvent();

	const char* GetEventName() const { return "START_NETWORK_SYNCED_SCENE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int sceneID, unsigned networkTimeStarted, const netPlayer *targetPlayer = 0);

	virtual bool IsInScope(const netPlayer &player) const;
    virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

    static bool IsSceneStartEventPending(int sceneID, const netPlayer *toPlayer = 0);

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer, bool stopOldScenes);

	int      m_SceneID;
    unsigned m_NetworkTimeStarted;
    bool     m_ReleaseSceneOnCleanup;
	PlayerFlags m_PlayersInScope;
};

// ================================================================================================================
// CStopNetworkSyncedSceneEvent - sent when a player wishes to stop a synchronised scene on other players machines
// ================================================================================================================
class CStopNetworkSyncedSceneEvent : public netGameEvent
{
public:

	CStopNetworkSyncedSceneEvent();
    CStopNetworkSyncedSceneEvent(int sceneID);

	const char* GetEventName() const { return "STOP_NETWORK_SYNCED_SCENE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int sceneID);

	virtual bool IsInScope(const netPlayer &player) const;
    virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	int m_SceneID;
};

// ================================================================================================================
// CUpdateNetworkSyncedSceneEvent - sent when a script has updated a value in the script description....
// ================================================================================================================
class CUpdateNetworkSyncedSceneEvent : public netGameEvent
{
public:

	CUpdateNetworkSyncedSceneEvent();
    CUpdateNetworkSyncedSceneEvent(int sceneID, float const rate);
	~CUpdateNetworkSyncedSceneEvent();

	const char* GetEventName() const { return "UPDATE_NETWORK_SYNCED_SCENE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int sceneID, float const rate);

	virtual bool IsInScope(const netPlayer &player) const;
    virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

	virtual bool operator==(const netGameEvent* pEvent) const;

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	int m_SceneID;
	mutable float m_Rate;
};


// ================================================================================================================
// CGivePedScriptedTaskEvent - sent when a player wishes to give a ped owned by another machine a scripted task
// ================================================================================================================
class CGivePedScriptedTaskEvent : public netGameEvent
{
public:

	CGivePedScriptedTaskEvent(const NetworkEventType eventType = NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT);
    CGivePedScriptedTaskEvent(CPed *ped, CTaskInfo *taskInfo, const NetworkEventType eventType = NETWORK_GIVE_PED_SCRIPTED_TASK_EVENT);
    ~CGivePedScriptedTaskEvent();

	const char* GetEventName() const { return "GIVE_PED_SCRIPTED_TASK_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed *ped, CTaskInfo *taskInfo);
    static bool AreEventsPending(ObjectId pedID);

	virtual bool IsInScope(const netPlayer &player) const;
    virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return true; }

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
	virtual void WriteDecisionToLogFile() const;
#endif // ENABLE_NETWORK_LOGGING

protected:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	bool GivePedScriptedTask();

    static void RemoveExistingEvents(CPed *ped);

    ObjectId		m_PedID;		// Object ID of the ped to give the task to
    CTaskInfo		*m_TaskInfo;	// Task info describing the task to give (owned by this task)
    u32				m_TaskType;		// The scripted task type (see script_tasks.h)
	bool			m_PedNotLocal;	// Set when the event is ignored because the ped is not local
	bool			m_TaskGiven;	// Set when the task has been given to the ped
};

// ================================================================================================================
// CGivePedSequenceTaskEvent - sent when a player wishes to give a ped owned by another machine a sequence task
// ================================================================================================================
class CGivePedSequenceTaskEvent : public CGivePedScriptedTaskEvent
{
public:

	CGivePedSequenceTaskEvent();
	CGivePedSequenceTaskEvent(CPed *ped, CTaskInfo *taskInfo);

	const char* GetEventName() const { return "GIVE_PED_SEQUENCE_TASK_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed *ped, CTaskInfo *taskInfo);

	virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
	virtual void WriteDecisionToLogFile() const;
#endif //ENABLE_NETWORK_LOGGING

private:

	void SerialiseTaskSequenceData(CSyncDataBase &serialiser) const;

	scriptHandler* GetScriptHandler() const ;			// returns the script handler that triggered this event
	CTaskSequenceList* GetTaskSequenceList() const;

	static void RemoveExistingEvents(CPed *ped);

	bool											m_HasSequenceTaskData; // If true, task data for a sequence task has been sent in the event
	bool											m_SequenceDoesntExist;	// Set when the event is ignored because the task sequence does not exist

	static u32										ms_numSequenceTasks;    
	static IPedNodeDataAccessor::CTaskData			ms_sequenceTaskData[IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS];
	static u32										ms_sequenceRepeatMode;
};

// ================================================================================================================
// CClearPedTasksEvent - sent when a player wishes clear the tasks on a ped owned by a remote player
// ================================================================================================================
class CClearPedTasksEvent : public netGameEvent
{
public:

	CClearPedTasksEvent();
    CClearPedTasksEvent(CPed *ped, bool clearTasksImmediately);

	const char* GetEventName() const { return "CLEAR_PED_TASKS_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed *ped, bool clearTasksImmediately);

	virtual bool IsInScope(const netPlayer &player) const;
    virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_PedID;                 // Object ID of the ped to clear tasks
    bool     m_ClearTasksImmediately; // Flag indicating whether the tasks should be cleared immediately
};

// ================================================================================================================
// CStartNetworkPedArrestEvent - sent when a player wishes to start arresting a network ped
// ================================================================================================================
class CStartNetworkPedArrestEvent : public netGameEvent
{
public:

	CStartNetworkPedArrestEvent();
	CStartNetworkPedArrestEvent(CPed* pArresterPed, CPed* pArresteePed, fwFlags8 iFlags);

	const char* GetEventName() const { return "START_NETWORK_PED_ARREST_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed *pArresterPed, CPed *pArresteePed, fwFlags8 iFlags = 0);

	virtual bool IsInScope(const netPlayer &player) const;
	virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
 
#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_ArresterPedId;
	ObjectId m_ArresteePedId;
	static const unsigned SIZEOF_FLAGS = 8;
	u8 m_iFlags;
};

// ================================================================================================================
// CStartNetworkPedUncuffEvent - sent when a player wishes to start uncuffing a network ped
// ================================================================================================================
class CStartNetworkPedUncuffEvent : public netGameEvent
{
public:

	CStartNetworkPedUncuffEvent();
	CStartNetworkPedUncuffEvent(CPed* pUncufferPed, CPed* pCuffedPed);

	const char* GetEventName() const { return "START_NETWORK_PED_UNCUFF_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed* pUncufferPed, CPed* pCuffedPed);

	virtual bool IsInScope(const netPlayer &player) const;
	virtual bool HasTimedOut() const;
	virtual void Prepare(datBitBuffer &bitBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *bitBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_UncufferPedId;
	ObjectId m_CuffedPedId;
};

// ================================================================================================================
// CCarHornEvent - sent when the players car horn is toggled on / off
// ================================================================================================================
class CCarHornEvent : public netGameEvent
{
public:

	CCarHornEvent();
	CCarHornEvent(const bool bIsHornOn, const ObjectId vehicleID, const u8 hornModIndex);
	
	const char* GetEventName() const { return "NETWORK_CAR_HORN_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const bool bIsHornOn, const ObjectId vehicleID, u8 hornModIndex = 0);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	bool MustPersist() const { return !m_bIsHornOn; }

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_vehicleID;
	u8 m_hornModIndex;
	bool m_bIsHornOn;
};

// ================================================================================================================
// CVoiceDrivenMouthMovementFinished - sent when the player has finished speaking
// ================================================================================================================
class CVoiceDrivenMouthMovementFinishedEvent : public netGameEvent
{
public:

	CVoiceDrivenMouthMovementFinishedEvent();

	const char* GetEventName() const { return "VOICE_DRIVEN_MOUTH_MOVEMENT_FINISHED_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger();

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
};


// ================================================================================================================
// CEntityAreaStatusEvent - sent when an entity areas occupied status changes
// ================================================================================================================
class CEntityAreaStatusEvent : public netGameEvent
{
public:

	CEntityAreaStatusEvent();
	CEntityAreaStatusEvent(const CGameScriptId &scriptID, const int nNetworkID, const bool bIsOccupied, const netPlayer* pTargetPlayer);

	const char* GetEventName() const { return "NETWORK_ENTITY_AREA_STATUS_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CGameScriptId &scriptID, const int nNetworkID, const bool bIsOccupied, const netPlayer* pTargetPlayer = 0);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	void AddTargetPlayer(const netPlayer &targetPlayer);

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CGameScriptId m_ScriptID; 
	int m_nNetworkID;
	bool m_bIsOccupied; 
	PlayerFlags m_PlayersInScope;
};

// ================================================================================================================
// CModifyVehicleLockWorldStateDataEvent 
// - works in conjunction with CNetworkVehiclePlayerLockingWorldState 
// - sent when an existing vehicle instance lock is changes state
// ================================================================================================================
class CModifyVehicleLockWorldStateDataEvent : public netGameEvent
{
public:

	CModifyVehicleLockWorldStateDataEvent();
	CModifyVehicleLockWorldStateDataEvent(u32 const playerIndex, s32 const vehicleIndex, u32 const modelId, bool const lock);

	const char* GetEventName() const { return "MODIFY_VEHICLE_LOCK_WORLD_STATE_DATA_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);

	static void Trigger(u32 const playerIndex, s32 const vehicleIndex, u32 const modelId, bool const lock);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u32		m_playerIndex;
	s32		m_vehicleIndex;
	u32		m_modelId;
	bool	m_lock;
};

// ================================================================================================================
// CModifyPtFXWorldStateDataScriptedEvolveEvent 
// - works in conjunction with CNetworkPtFXWorldStateData 
// - sent when an FX evolves
// ================================================================================================================
class CModifyPtFXWorldStateDataScriptedEvolveEvent : public netGameEvent
{
public:

	CModifyPtFXWorldStateDataScriptedEvolveEvent();
	CModifyPtFXWorldStateDataScriptedEvolveEvent(u32 evoHash, float evoVal, int uniqueID);

	const char* GetEventName() const { return "MODIFY_PTFX_WORLD_STATE_DATA_SCRIPTED_EVOLVE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);

	static void Trigger(u32 evoHash, float evoVal, int uniqueId);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u32 m_EvoHash;
	float m_EvoVal;
	int m_UniqueID;
};

// ================================================================================================================
// CGarageOccupiedStatusEvent - sent when a garage occupied status changes
// ================================================================================================================
class CGarageOccupiedStatusEvent : public netGameEvent
{
public:

	CGarageOccupiedStatusEvent();
	CGarageOccupiedStatusEvent(const int nGarageIndex, const BoxFlags boxesOccupied, PhysicalPlayerIndex playerToSendTo);

	const char* GetEventName() const { return "NETWORK_GARAGE_OCCUPIED_STATUS_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const int nNetworkID, const BoxFlags boxesOccupied, PhysicalPlayerIndex playerToSendTo = INVALID_PLAYER_INDEX);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	int m_GarageIndex;
	BoxFlags m_boxesOccupied;
	PhysicalPlayerIndex m_specificPlayerToSendTo;
};

// ================================================================================================================
// CPedConversationLineEvent - send to get a ped to speak a line of conversation
// Currently used for one line, one speaker conversations - a more complex solution is required
// if we have multiple party, multiple line conversations in MP...
// ================================================================================================================
class CPedConversationLineEvent : public netGameEvent
{
public:

    CPedConversationLineEvent();
	CPedConversationLineEvent(CPed const* ped, u32 const voiceNameHash, const char* context, s32 tauntVariationNumber, bool playscriptedspeech);
	CPedConversationLineEvent(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, const char* speechParams);

    const char* GetEventName() const { return "PED_CONVERSATION_LINE_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, bool playscriptedspeech = false);
	static void Trigger(CPed const* ped, u32 const voiceNameHash, const char* context, s32 variationNumber, const char* speechParams);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int MAX_CONTEXT_LENGTH		= 40;
	static const unsigned int MAX_SPEECH_PARAMS_LENGTH	= 40;
	static const unsigned int SIZEOF_VOICE_NAME_HASH	= 32;
    static const unsigned int SIZEOF_LINE_VARIATION		= 32;

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId m_TargetPedId;
	char m_context[MAX_CONTEXT_LENGTH];
	char m_speechParams[MAX_SPEECH_PARAMS_LENGTH];
	u32 m_voiceNameHash;
    s32 m_variationNumber;
	bool m_playscriptedspeech;
	bool m_speechParamsString;
};

// ================================================================================================================
// CScriptEntityStateChangeEvent - sent to request state change on a script entity owned by another machine
// ================================================================================================================
class CScriptEntityStateChangeEvent : public netGameEvent
{
public:

    static const unsigned MAX_PARAMETER_STORAGE_SIZE = 128;

    enum StateChangeType
    {
        FIRST_STATE_CHANGE_TYPE,
        SET_BLOCKING_OF_NON_TEMPORARY_EVENTS = FIRST_STATE_CHANGE_TYPE,
		SET_PED_RELATIONSHIP_GROUP_HASH,
		SET_DRIVE_TASK_CRUISE_SPEED,
		SET_LOOK_AT_ENTITY,
		SET_PLANE_MIN_HEIGHT_ABOVE_TERRAIN,
		SET_PED_RAGDOLL_BLOCKING_FLAGS,
		SET_TASK_VEHICLE_TEMP_ACTION,
		SET_PED_FACIAL_IDLE_ANIM_OVERRIDE,
        SET_VEHICLE_LOCK_STATE,
        SET_EXCLUSIVE_DRIVER,
		NUM_STATE_CHANGE_TYPES
    };

    // Base class for script entity state parameters
    class IScriptEntityStateParametersBase
    {
    public:

        virtual ~IScriptEntityStateParametersBase() {}

        virtual StateChangeType GetType() const = 0;
        virtual const char     *GetName() const = 0;

        virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const = 0;

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const = 0;

        virtual void ReadParameters (datBitBuffer &messageBuffer) = 0;
        virtual void WriteParameters(datBitBuffer &messageBuffer) = 0;
        virtual void ApplyParameters(netObject *networkObject)    = 0;
        virtual void LogParameters  (netLoggingInterface &log)    = 0;

    protected:
        IScriptEntityStateParametersBase() {}
    };

    // parameter class for setting the blocking of non temporary events
    class CBlockingOfNonTemporaryEventsParameters : public IScriptEntityStateParametersBase
    {
    public:
        CBlockingOfNonTemporaryEventsParameters() : m_BlockNonTemporaryEvents(false) {}
        CBlockingOfNonTemporaryEventsParameters(bool blockNonTemporaryEvents) : m_BlockNonTemporaryEvents(blockNonTemporaryEvents) {}

        virtual StateChangeType GetType() const { return SET_BLOCKING_OF_NON_TEMPORARY_EVENTS; }
        virtual const char     *GetName() const { return "SET_BLOCKING_OF_NON_TEMPORARY_EVENTS"; }

        virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CBlockingOfNonTemporaryEventsParameters(m_BlockNonTemporaryEvents);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

        virtual void ReadParameters (datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataReader>(messageBuffer);
        }

        virtual void WriteParameters(datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataWriter>(messageBuffer);
        }

        virtual void ApplyParameters(netObject *networkObject);
        virtual void LogParameters  (netLoggingInterface &log);

    private:

        template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
        {
            Serialiser serialiser(messageBuffer);
            SERIALISE_BOOL(serialiser, m_BlockNonTemporaryEvents);
        }

        bool m_BlockNonTemporaryEvents;
    };

	class CSettingOfPedRelationshipGroupHashParameters : public IScriptEntityStateParametersBase
	{
	public:
        CSettingOfPedRelationshipGroupHashParameters() : m_relationshipGroupHash((u32)INVALID_RELATIONSHIP_GROUP) {}
        CSettingOfPedRelationshipGroupHashParameters(u32 const relationshipGroup) : m_relationshipGroupHash(relationshipGroup) {}

        virtual StateChangeType GetType() const { return SET_PED_RELATIONSHIP_GROUP_HASH; }
        virtual const char     *GetName() const { return "SET_PED_RELATIONSHIP_GROUP_HASH"; }

        virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSettingOfPedRelationshipGroupHashParameters(m_relationshipGroupHash);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

        virtual void ReadParameters (datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataReader>(messageBuffer);
        }

        virtual void WriteParameters(datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataWriter>(messageBuffer);
        }

        virtual void ApplyParameters(netObject *networkObject);
        virtual void LogParameters  (netLoggingInterface &log);

	private:

        template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
        {
            Serialiser serialiser(messageBuffer);
			SerialiseRelationshipGroup(serialiser, m_relationshipGroupHash, "Relationship Group");
        }

		u32 m_relationshipGroupHash;
	};

	class CSetPedRagdollBlockFlagParameters : public IScriptEntityStateParametersBase
	{
	public:
        CSetPedRagdollBlockFlagParameters() : m_ragdollBlockFlags(0) {}
        CSetPedRagdollBlockFlagParameters(u32 const ragdollBlockFlags) : m_ragdollBlockFlags(ragdollBlockFlags) {}

        virtual StateChangeType GetType() const { return SET_PED_RAGDOLL_BLOCKING_FLAGS; }
        virtual const char     *GetName() const { return "SET_PED_RAGDOLL_BLOCKING_FLAGS"; }

        virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSetPedRagdollBlockFlagParameters(m_ragdollBlockFlags);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

        virtual void ReadParameters (datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataReader>(messageBuffer);
        }

        virtual void WriteParameters(datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataWriter>(messageBuffer);
        }

        virtual void ApplyParameters(netObject *networkObject);
        virtual void LogParameters  (netLoggingInterface &log);

	private:

        template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
        {
            Serialiser serialiser(messageBuffer);
			SerialiseRelationshipGroup(serialiser, m_ragdollBlockFlags, "Ragdoll Blocking Flags");
        }

		u32 m_ragdollBlockFlags;
	};

	class CSettingOfDriveTaskCruiseSpeed : public IScriptEntityStateParametersBase
	{
	public:
        CSettingOfDriveTaskCruiseSpeed() : m_cruiseSpeed(0.0f) {}
        CSettingOfDriveTaskCruiseSpeed(float const cruiseSpeed) : m_cruiseSpeed(cruiseSpeed) {}

        virtual StateChangeType GetType() const { return SET_DRIVE_TASK_CRUISE_SPEED; }
        virtual const char     *GetName() const { return "SET_DRIVE_TASK_CRUISE_SPEED"; }

        virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSettingOfDriveTaskCruiseSpeed(m_cruiseSpeed);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

        virtual void ReadParameters (datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataReader>(messageBuffer);
        }

        virtual void WriteParameters(datBitBuffer &messageBuffer)
        {
            SerialiseParameters<CSyncDataWriter>(messageBuffer);
        }

        virtual void ApplyParameters(netObject *networkObject);
        virtual void LogParameters  (netLoggingInterface &log);

	private:

		static const u32 SIZEOF_CRUISE_SPEED = 8;

        template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
        {
            Serialiser serialiser(messageBuffer);
			SERIALISE_PACKED_FLOAT(serialiser, m_cruiseSpeed, 30.f, SIZEOF_CRUISE_SPEED, "Cruise Speed");
        }

		float m_cruiseSpeed;
	};

	class CSettingOfLookAtEntity : public IScriptEntityStateParametersBase
	{
	public:

		CSettingOfLookAtEntity() : m_ObjectID(NETWORK_INVALID_OBJECT_ID), m_nFlags(0), m_nTime(-1), m_nTimeCapped(0) {}
		CSettingOfLookAtEntity(const ObjectId objectID, const u32 nFlags, const int nTime);
		CSettingOfLookAtEntity(netObject* pNetObj, const u32 nFlags, const int nTime);

		virtual StateChangeType GetType() const { return SET_LOOK_AT_ENTITY; }
		virtual const char     *GetName() const { return "SET_LOOK_AT_ENTITY"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSettingOfLookAtEntity(m_ObjectID, m_nFlags, m_nTime);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

		static const unsigned int SIZEOF_TIME = 10;
		static const unsigned int SIZEOF_LOOK_AT_FLAGS = LF_NUM_FLAGS; 

		void Init(const ObjectId objectID, const u32 nFlags, const int nTime);
		
		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			Serialiser serialiser(messageBuffer);
			SERIALISE_OBJECTID(serialiser, m_ObjectID, "Look At Entity");
			SERIALISE_UNSIGNED(serialiser, m_nFlags, SIZEOF_LOOK_AT_FLAGS, "Look At Flags");
			SERIALISE_UNSIGNED(serialiser, m_nTimeCapped, SIZEOF_TIME, "Look at Time");
		}

		ObjectId m_ObjectID;
		u32 m_nFlags;
		int m_nTime;
		u32 m_nTimeCapped;
	};

	class CSettingOfPlaneMinHeightAboveTerrainParameters : public IScriptEntityStateParametersBase
	{
	public:
		CSettingOfPlaneMinHeightAboveTerrainParameters() : m_minHeightAboveTerrain(0) {}
		CSettingOfPlaneMinHeightAboveTerrainParameters(u32 const minHeightAboveTerrain) : m_minHeightAboveTerrain(minHeightAboveTerrain) {}

		virtual StateChangeType GetType() const { return SET_PLANE_MIN_HEIGHT_ABOVE_TERRAIN; }
		virtual const char     *GetName() const { return "SET_PLANE_MIN_HEIGHT_ABOVE_TERRAIN"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSettingOfPlaneMinHeightAboveTerrainParameters(m_minHeightAboveTerrain);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			Serialiser serialiser(messageBuffer);
			SERIALISE_UNSIGNED(serialiser, m_minHeightAboveTerrain, 32, "Min Height Above Terrain");
		}

		u32 m_minHeightAboveTerrain;
	};

	class CSettingOfTaskVehicleTempAction : public IScriptEntityStateParametersBase
	{
	public:
		CSettingOfTaskVehicleTempAction() : m_ObjectID(NETWORK_INVALID_OBJECT_ID), m_iAction(0), m_iTime(-1), m_bTime(false) {}
		CSettingOfTaskVehicleTempAction(const ObjectId objectID, const int iAction, const int iTime);
		CSettingOfTaskVehicleTempAction(netObject* pNetObj, const int iAction, const int iTime);

		virtual StateChangeType GetType() const { return SET_TASK_VEHICLE_TEMP_ACTION; }
		virtual const char     *GetName() const { return "SET_TASK_VEHICLE_TEMP_ACTION"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSettingOfTaskVehicleTempAction(m_ObjectID,m_iAction,m_iTime);}

		virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

		static const unsigned int SIZEOF_ACTION = 8; //0-255 
		static const unsigned int SIZEOF_TIME = 16; //0-65536

		void Init(const ObjectId objectID, const int iAction, const int iTime);

		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			Serialiser serialiser(messageBuffer);
			SERIALISE_OBJECTID(serialiser, m_ObjectID, "ScriptVehicle");
			SERIALISE_UNSIGNED(serialiser, m_iAction, SIZEOF_ACTION, "m_iAction");
			SERIALISE_BOOL(serialiser, m_bTime);
			if (m_bTime)
			{
				SERIALISE_UNSIGNED(serialiser, m_iTime, SIZEOF_TIME, "m_iTime");
			}
			else
			{
				m_iTime = -1;
			}

		}

		ObjectId m_ObjectID;
		int m_iAction;
		int m_iTime;
		bool m_bTime;
	};

	class CSetPedFacialIdleAnimOverride : public IScriptEntityStateParametersBase
	{
	public:
		CSetPedFacialIdleAnimOverride() : m_clear(false), m_idleClipNameHash(0), m_idleClipDictNameHash(0) {}
		CSetPedFacialIdleAnimOverride(bool clear, u32 const idleClipNameHash, u32 const idleClipDictNameHash) : m_clear(clear), m_idleClipNameHash(idleClipNameHash), m_idleClipDictNameHash(idleClipDictNameHash) {}

		virtual StateChangeType GetType() const { return SET_PED_FACIAL_IDLE_ANIM_OVERRIDE; }
		virtual const char     *GetName() const { return "SET_PED_FACIAL_IDLE_ANIM_OVERRIDE"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSetPedFacialIdleAnimOverride(m_clear, m_idleClipNameHash, m_idleClipDictNameHash);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			Serialiser serialiser(messageBuffer);
			SERIALISE_BOOL(serialiser, m_clear, "Clear");
			if(!m_clear)
			{
				SERIALISE_UNSIGNED(serialiser, m_idleClipNameHash,		32, "Facial Idle Anim Clip Hash");
				SERIALISE_UNSIGNED(serialiser, m_idleClipDictNameHash,	32, "Facial Idle Anim Clip Dict Hash");
			}
		}

		bool	m_clear;					// Are we setting or clearing?
		u32		m_idleClipNameHash;			// If we're setting, what are we setting?
		u32		m_idleClipDictNameHash;		// If we're setting, what are we setting?
	};

    class CSetVehicleLockState : public IScriptEntityStateParametersBase
	{
	public:
		CSetVehicleLockState() : m_PlayerLocks(0), m_DontTryToEnterIfLockedForPlayer(false), m_Flags(0) {}
		CSetVehicleLockState(PlayerFlags playerLocks) : m_PlayerLocks(playerLocks), m_DontTryToEnterIfLockedForPlayer(false), m_Flags(LOCK_STATE) {}
        CSetVehicleLockState(bool dontTryToEnterIfLockedForPlayer) : m_PlayerLocks(0), m_DontTryToEnterIfLockedForPlayer(dontTryToEnterIfLockedForPlayer), m_Flags(PLAYER_FLAG) {}
        CSetVehicleLockState(PlayerFlags playerLocks, bool dontTryToEnterIfLockedForPlayer) : m_PlayerLocks(playerLocks), m_DontTryToEnterIfLockedForPlayer(dontTryToEnterIfLockedForPlayer), m_Flags(LOCK_STATE | PLAYER_FLAG) {}

		virtual StateChangeType GetType() const { return SET_VEHICLE_LOCK_STATE; }
		virtual const char     *GetName() const { return "SET_VEHICLE_LOCK_STATE"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSetVehicleLockState(m_PlayerLocks, m_DontTryToEnterIfLockedForPlayer, m_Flags);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

        CSetVehicleLockState(PlayerFlags playerLocks, bool dontTryToEnterIfLockedForPlayer, u8 flags) : m_PlayerLocks(playerLocks), m_DontTryToEnterIfLockedForPlayer(dontTryToEnterIfLockedForPlayer), m_Flags(flags) {}

        enum StateChangeFlags
        {
            LOCK_STATE  = BIT(0),
            PLAYER_FLAG = BIT(1)
        };

		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			Serialiser serialiser(messageBuffer);
			
            const unsigned SIZEOF_FLAGS = 2;

            SERIALISE_BITFIELD(serialiser, m_PlayerLocks, MAX_NUM_PHYSICAL_PLAYERS,  "Player Locks");
			SERIALISE_BOOL(serialiser, m_DontTryToEnterIfLockedForPlayer, "Don't Try To Enter If Locked For Player");
            SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Flags");
		}

		PlayerFlags m_PlayerLocks;
        bool        m_DontTryToEnterIfLockedForPlayer;
        u8          m_Flags;
	};

    class CSetVehicleExclusiveDriver : public IScriptEntityStateParametersBase
	{
	public:
		CSetVehicleExclusiveDriver() : m_ExclusiveDriverID(NETWORK_INVALID_OBJECT_ID) {}
		CSetVehicleExclusiveDriver(ObjectId exclusiveDriverID, u32 ExclusiveDriverIndex) : m_ExclusiveDriverID(exclusiveDriverID), m_ExclusiveDriverIndex(ExclusiveDriverIndex) {}

		virtual StateChangeType GetType() const { return SET_EXCLUSIVE_DRIVER; }
		virtual const char     *GetName() const { return "SET_EXCLUSIVE_DRIVER"; }

		virtual IScriptEntityStateParametersBase *Copy(u8 *storage) const { return rage_placement_new(storage) CSetVehicleExclusiveDriver(m_ExclusiveDriverID, m_ExclusiveDriverIndex);}

        virtual bool IsEqual(IScriptEntityStateParametersBase *otherParameters) const;

		virtual void ReadParameters (datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataReader>(messageBuffer);
		}

		virtual void WriteParameters(datBitBuffer &messageBuffer)
		{
			SerialiseParameters<CSyncDataWriter>(messageBuffer);
		}

		virtual void ApplyParameters(netObject *networkObject);
		virtual void LogParameters  (netLoggingInterface &log);

	private:

		template <class Serialiser> void SerialiseParameters(datBitBuffer &messageBuffer)
		{
			const unsigned SIZEOF_EXCLUSIVE_DRIVER_INDEX = 2;

			Serialiser serialiser(messageBuffer);
			SERIALISE_OBJECTID(serialiser, m_ExclusiveDriverID, "Exclusive Driver");
			SERIALISE_UNSIGNED(serialiser, m_ExclusiveDriverIndex, SIZEOF_EXCLUSIVE_DRIVER_INDEX, "Exclusive Driver Index");
		}

		ObjectId m_ExclusiveDriverID;
		u32	m_ExclusiveDriverIndex;
	};

	CScriptEntityStateChangeEvent();
	CScriptEntityStateChangeEvent(netObject *networkObject, const IScriptEntityStateParametersBase &parameters);
	~CScriptEntityStateChangeEvent();

	const char* GetEventName() const { return "SCRIPT_ENTITY_STATE_CHANGE_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(netObject *networkObject, const IScriptEntityStateParametersBase &parameters);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return true; }
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned SIZEOF_STATE_CHANGE_TYPE = datBitsNeeded<NUM_STATE_CHANGE_TYPES-1>::COUNT;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    IScriptEntityStateParametersBase *CreateParameterClass(const StateChangeType &stateChangeType);

    ObjectId                          m_ScriptEntityID;
    IScriptEntityStateParametersBase *m_StateParameters;
    unsigned                          m_NetworkTimeOfChange;
    u8                                m_ParameterClassStorage[MAX_PARAMETER_STORAGE_SIZE];
};

// ================================================================================================================
// CPlaySoundEvent - 
// ================================================================================================================
class CPlaySoundEvent : public netGameEvent
{
public:

	CPlaySoundEvent();
	CPlaySoundEvent(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, int nRange);
	CPlaySoundEvent(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange);
	CPlaySoundEvent(const Vector3& vPosition, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange);

	const char* GetEventName() const { return "PLAY_SOUND_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange);
	static void Trigger(const Vector3& vPosition, u32 setNameHash, u32 soundNameHash, u8 soundID, CGameScriptId& scriptId, int nRange);
	static void Trigger(const CEntity* pEntity, u32 setNameHash, u32 soundNameHash, int nRange);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool MustPersist() const { return false; }
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int SIZEOF_AUDIO_HASH	= 32;
	static const unsigned int SIZEOF_SOUND_ID	= 8;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	bool m_bUseEntity;
	ObjectId m_EntityID;
	Vector3 m_Position;
	u32 m_setNameHash;
	u32 m_soundNameHash;
	u8 m_SoundID;
	CGameScriptId m_ScriptId;
	int m_nRange; 
};

// ================================================================================================================
// CStopSoundEvent - 
// ================================================================================================================
class CStopSoundEvent : public netGameEvent
{
public:

	CStopSoundEvent();
	CStopSoundEvent(u8 soundID);

	const char* GetEventName() const { return "STOP_SOUND_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(u8 soundID);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int SIZEOF_SOUND_ID = 8;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	u8 m_SoundID;
};


// ================================================================================================================
// CPlayAirDefenseFireEvent - 
// ================================================================================================================
class CPlayAirDefenseFireEvent : public netGameEvent
{
public:

	CPlayAirDefenseFireEvent();
	CPlayAirDefenseFireEvent(Vector3 position, u32 soundHash, float sphereRadius);

	const char* GetEventName() const { return "NETWORK_PLAY_AIRDEFENSE_FIRE_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(Vector3 position, u32 soundHash, float sphereRadius);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool MustPersist() const { return false; }
	virtual bool HasTimedOut() const;
	virtual u32 GetTimeAdded() const { return m_SystemTimeAdded;}
	virtual bool operator==(const netGameEvent* pEvent) const;
#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING
	
private:  
	
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	mutable u32 m_SystemTimeAdded;
	u32 m_SoundHash;
	Vector3 m_positionOfAirDefence;
	float m_sphereRadius;
};

// ================================================================================================================
// CAudioBankRequestEvent - 
// ================================================================================================================
class CAudioBankRequestEvent : public netGameEvent
{
public:

	CAudioBankRequestEvent();
	CAudioBankRequestEvent(u32 bankHash, CGameScriptId& scriptId, bool bIsRequest, unsigned playerBits);

	const char* GetEventName() const { return "BANK_REQUEST_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(u32 bankHash, CGameScriptId& scriptId, bool bIsRequest, unsigned playerBits);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int SIZEOF_BANK_ID = 32;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	u32 m_BankHash;
	CGameScriptId m_ScriptID;
	bool m_bIsRequest;
	unsigned m_PlayerBits; 
};

// ================================================================================================================
// CAudioBarkingEvent - 
// ================================================================================================================
class CAudioBarkingEvent : public netGameEvent
{
public:
	CAudioBarkingEvent();
	CAudioBarkingEvent(ObjectId pedId, u32 contextHash, Vector3 barkPosition);

	const char* GetEventName() const { return "AUDIO_BARKING_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(ObjectId pedId, u32 contextHash, Vector3 barkPosition);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	ObjectId m_PedId;
	u32 m_ContextHash;
	Vector3 m_BarkPosition;
};

// ================================================================================================================
// CRequestDoorEvent - sent when we want to change the state of a non-networked door and start networking it
// ================================================================================================================
class CRequestDoorEvent : public netGameEvent
{
public:
	CRequestDoorEvent( );
	CRequestDoorEvent( int doorEnumHash, int doorState, const CGameScriptId& scriptId );
	~CRequestDoorEvent();

	const char* GetEventName() const { return "REQUEST_DOOR_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CDoorSystemData& door, int doorState);

	virtual bool IsInScope( const netPlayer& player ) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual void PrepareReply ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void HandleReply ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteDecisionToLogFile() const;
	virtual void WriteReplyToLogFile(bool wasSent) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

	int				m_doorEnumHash;				// the enum hash identifying the door
	mutable int		m_doorState;				// the state to be set on the door
	CGameScriptId	m_scriptId;					// the script that owns the door
	bool			m_bGranted;					// if true player is allowed to network the door
	bool			m_bGrantedByAllPlayers;     // if true all players have permitted the door to be networked
	mutable bool	m_bLocallyTriggered;
};

// ================================================================================================================
// CNetworkTrainRequestEvent - sent when a train is created so that other systems will not try to create a train on the same tracks
// ================================================================================================================
class CNetworkTrainRequestEvent : public netGameEvent
{
public:
	CNetworkTrainRequestEvent( );
	CNetworkTrainRequestEvent( int traintrack );
	~CNetworkTrainRequestEvent();

	const char* GetEventName() const { return "NETWORK_TRAIN_REQUEST_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int traintrack);

	virtual void PrepareReply           ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void HandleReply            ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);

	virtual bool IsInScope( const netPlayer& player ) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
	virtual void WriteReplyToLogFile(bool UNUSED_PARAM(wasSent)) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	template <class Serialiser> void SerialiseEventReply(datBitBuffer &messageBuffer);

	int		m_traintrack;
	bool    m_trainapproved;
};

// ================================================================================================================
// CNetworkTrainReportEvent - sent when a train is created so that other systems will not try to create a train on the same tracks
// ================================================================================================================
class CNetworkTrainReportEvent : public netGameEvent
{
public:
	CNetworkTrainReportEvent( );
	CNetworkTrainReportEvent( int traintrack, bool trainactive, u8 playerindex );
	~CNetworkTrainReportEvent();

	const char* GetEventName() const { return "NETWORK_TRAIN_REPORT_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(int traintrack, bool trainactive, u8 playerindex = 255);

	virtual bool IsInScope( const netPlayer& player ) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	int		m_traintrack;
	bool	m_trainactive;
	u8		m_playerindex;
};

// ================================================================================================================
// CNetworkIncrementStatEvent - sent when a player is reported or commended
// ================================================================================================================
class CNetworkIncrementStatEvent : public netGameEvent
{
public:
	CNetworkIncrementStatEvent( );
	CNetworkIncrementStatEvent( u32 uStatHash, int iStrength, netPlayer* pPlayer );
	~CNetworkIncrementStatEvent();

	const char* GetEventName() const { return "NETWORK_INCREMENT_STAT_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger( u32 uStatHash, int iStrength, netPlayer* pPlayer );

	virtual bool IsInScope( const netPlayer& player ) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static bool IsStatWhitelisted(u32 statHash);

	u32 m_uStatHash;
	int m_iStrength;
	netPlayer* m_pPlayer;

	static const unsigned NUM_WHITELISTED_EVENT_STATS = 13;
	static u32 ms_whitelistedEventStatsHashes[NUM_WHITELISTED_EVENT_STATS];
};

// ================================================================================================================
// CRequestPhoneExplosionEvent - 
// ================================================================================================================
class CRequestPhoneExplosionEvent : public netGameEvent
{
public:

	CRequestPhoneExplosionEvent();
	CRequestPhoneExplosionEvent(const ObjectId nVehicleID);
	
	const char* GetEventName() const { return "REQUEST_PHONE_EXPLOSION_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const ObjectId nVehicleID);
	
	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	ObjectId m_nVehicleID;
};

// ================================================================================================================
// CRequestDetachmentEvent - sent to request an entity to detach from what it is currently attached to
// ================================================================================================================
class CRequestDetachmentEvent : public netGameEvent
{
public:

    CRequestDetachmentEvent(const CPhysical &physicalToDetach);
	CRequestDetachmentEvent();
	
	const char* GetEventName() const { return "REQUEST_DETACHMENT_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPhysical &physicalToDetach);
	
	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    ObjectId m_PhysicalToDetachID;
};

// ================================================================================================================
// CSendMyKickVotesEvent - sent to inform about my gamer kick votes.
// ================================================================================================================
class CSendKickVotesEvent : public netGameEvent
{
public:
	CSendKickVotesEvent(const PlayerFlags kickVotes, const PlayerFlags playersInScope);
	CSendKickVotesEvent();

	const char* GetEventName() const { return "KICK_VOTES_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const PlayerFlags kickVotes, const PlayerFlags playersInScope);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	PlayerFlags	 m_KickVotes;
	PlayerFlags	 m_PlayersInScope;
};

// ================================================================================================================
// CGivePickupRewardsEvent - a way of passing pickup rewards on to other players (eg the passengers in your vehicle)
// ================================================================================================================
class CGivePickupRewardsEvent : public netGameEvent
{
public:
	CGivePickupRewardsEvent(PlayerFlags receivingPlayers, u32 rewardHash);
	CGivePickupRewardsEvent();

	const char* GetEventName() const { return "NETWORK_GIVE_PICKUP_REWARDS_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(PlayerFlags receivingPlayers, u32 rewardHash);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const unsigned MAX_REWARDS = 5;

	PlayerFlags	 m_ReceivingPlayers;
	mutable u32	 m_Rewards[MAX_REWARDS];
	mutable u32	 m_NumRewards;
};

// ================================================================================================================
// CNetworkCrcHashCheckEvent - ask a remote player a certain common memory hash and check against local's. Way to detect remote player mem/file changes
// ================================================================================================================
class CNetworkCrcHashCheckEvent : public netGameEvent
{
public:
	CNetworkCrcHashCheckEvent(u8 thisRequestId, bool bIsReply, PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck, const char* fileNameToHash, u32 resultChecksum);
	CNetworkCrcHashCheckEvent();

	const char* GetEventName() const { return "NETWORK_CRC_HASH_CHECK_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void TriggerRequest(PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck , const char* fileNameToHash);
	static void TriggerReply(u8 repliedRequestId, PhysicalPlayerIndex receivingPlayer, u8 systemToCheck, u32 specificCheck , u32 resultChecksum);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	PhysicalPlayerIndex		m_ReceivingPlayer;
	bool					m_bIsReply;
	u8						m_eSystemToCheck;		//< eHASHABLE_SYSTEM; ie Weapon Info (CRC_WEAPON_INFO_DATA)
	u8						m_requestId;
	u32						m_uSpecificCheck;		//< ie melee weapon (WEAPONTYPE_UNARMED)
	u32						m_uChecksumResult;
	
	// If m_eSystemToCheck is CRC_GENERIC_FILE_CONTENTS, player needs to send the file path. Only in that case we will sync this char chunk (not serialising m_uSpecificCheck)
	static const unsigned int MAX_FILENAME_LENGTH = 128;
	char m_fileNameToHash[MAX_FILENAME_LENGTH];
};

// ================================================================================================================
// CBlowUpVehicleEvent - sent when we want to remotely blow up a vehicle
// ================================================================================================================
class CBlowUpVehicleEvent : public netGameEvent
{
public:
	CBlowUpVehicleEvent(ObjectId vehicleId, ObjectId culpritId, bool bAddExplosion, u32 weaponHash, bool bDelayedExplosion);
	CBlowUpVehicleEvent();
	~CBlowUpVehicleEvent();

	const char* GetEventName() const { return "BLOW_UP_VEHICLE_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CVehicle& vehicle, CEntity *pCulprit, bool bAddExplosion, u32 weaponHash, bool bDelayedExplosion);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool HasTimedOut() const;
	virtual bool CanChangeScope() const { return true; }
	virtual bool operator==(const netGameEvent* pEvent) const;

	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId		m_vehicleID;
	ObjectId		m_culpritID;
	u32				m_weaponHash;
	bool			m_bAddExplosion;
	bool			m_bDelayedExplosion;
	mutable u32		m_timeAdded;
};

// ================================================================================================================
// CActivateVehicleSpecialAbility - sent when we want to activate the special ability of a car
// ================================================================================================================
class CActivateVehicleSpecialAbilityEvent : public netGameEvent
{
public:
	enum eAbilityType
	{
		JUMP,
		ROCKET_BOOST,
		ROCKET_BOOST_STOP,
		SHUNT_L,
		SHUNT_R,
		NUM_ABILITY_TYPES
	};

	CActivateVehicleSpecialAbilityEvent(ObjectId vehicleId, eAbilityType abilityType);
	CActivateVehicleSpecialAbilityEvent();
	~CActivateVehicleSpecialAbilityEvent();
	
	const char* GetEventName() const { return "ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CVehicle& vehicle, eAbilityType abilityType);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool HasTimedOut() const;
	virtual bool CanChangeScope() const { return false; }
	virtual bool operator==(const netGameEvent* pEvent) const;

	virtual bool MustPersist() const {return false;}
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const unsigned int SIZEOF_ABILITY_TYPE  = datBitsNeeded<NUM_ABILITY_TYPES-1>::COUNT;

	ObjectId	 m_vehicleID;
	eAbilityType m_abilityType;  
	mutable u32	 m_timeAdded;
};

// ================================================================================================================
// CNetworkSpecialFireEquippedWeaponEvent - sent when we want a player to invoke firing outside of the task system
// ================================================================================================================
class CNetworkSpecialFireEquippedWeaponEvent : public netGameEvent
{
public:

	CNetworkSpecialFireEquippedWeaponEvent(const ObjectId entityID, const bool bvecTargetIsZero, const Vector3& vecStart, const Vector3& vecTarget, const bool bSetPerfectAccuracy);
	CNetworkSpecialFireEquippedWeaponEvent();
	~CNetworkSpecialFireEquippedWeaponEvent();

	const char* GetEventName() const { return "NETWORK_SPECIAL_FIRE_EQUIPPED_WEAPON"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CEntity* pEntity, const bool bvecTargetIsZero, const Vector3& vecStart, const Vector3& vecTarget, const bool bSetPerfectAccuracy);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return false; }

	virtual bool MustPersist() const {return false;}
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	ObjectId		m_entityID;
	Vector3			m_vecStart;
	Vector3			m_vecTarget;
	bool			m_vecTargetIsZero;
	bool			m_setPerfectAccuracy;
};

// ================================================================================================================
// CNetworkRespondedToThreatEvent - sent when a ped is informing a remote ally of responding to a threat
// ================================================================================================================
class CNetworkRespondedToThreatEvent : public netGameEvent
{
public:

	CNetworkRespondedToThreatEvent(const ObjectId responderEntityID, const ObjectId threateningEntityID);
	CNetworkRespondedToThreatEvent();
	~CNetworkRespondedToThreatEvent();

	bool AddRecipientObjectID(const ObjectId& objectID);

	const char* GetEventName() const { return "NETWORK_RESPONDED_TO_THREAT_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CEntity* pRecipientEntity, const CEntity* pResponderEntity, const CEntity* pThreatEntity);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return true; }
	virtual bool MustPersist() const {return false;}
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const unsigned int SIZEOF_NUM_RECIPIENTS = 4; // bits
	static const unsigned int MAX_RECIPIENTS_PER_EVENT = ((1<<SIZEOF_NUM_RECIPIENTS) - 1);

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u32      m_numRecipients;
	ObjectId m_responderEntityID;
	ObjectId m_threatEntityID;
	ObjectId m_recipientsEntityID[MAX_RECIPIENTS_PER_EVENT];
};

// ================================================================================================================
// CNetworkShoutTargetPositionEvent - sent when a ped is shouting a targets position to other peds in it's group (swat / police)
// ================================================================================================================
class CNetworkShoutTargetPositionEvent : public netGameEvent
{
public:

	CNetworkShoutTargetPositionEvent(const ObjectId shoutingEntityID, const ObjectId targetEntityID);
	CNetworkShoutTargetPositionEvent();
	~CNetworkShoutTargetPositionEvent();

	bool AddRecipientObjectID(const ObjectId& objectID);

	const char* GetEventName() const { return "NETWORK_SHOUT_TARGET_POSITION_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CEntity* pRecipientEntity, const CEntity* pPedShouting, const CEntity* pTargetPed);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return true; }

	virtual bool MustPersist() const {return false;}
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const unsigned int SIZEOF_NUM_RECIPIENTS = 4; // bits
	static const unsigned int MAX_RECIPIENTS_PER_EVENT = ((1<<SIZEOF_NUM_RECIPIENTS) - 1);

	u32      m_numRecipients;
	ObjectId m_shoutingEntityID;
	ObjectId m_targetEntityID;
	ObjectId m_recipientsEntityID[MAX_RECIPIENTS_PER_EVENT];
};


// ================================================================================================================
// CPickupDestroyedEvent - sent when a pickup placement pickup is destroyed
// ================================================================================================================
class CPickupDestroyedEvent : public netGameEvent
{
public:

	CPickupDestroyedEvent();
	CPickupDestroyedEvent(CPickupPlacement& placement);
	CPickupDestroyedEvent(CPickup& pickup);

	const char* GetEventName() const { return "PICKUP_DESTROYED_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CPickupPlacement& placement);
	static void Trigger(CPickup& pickup);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CGameScriptObjInfo	m_placementInfo;	// the script info of the placement being destroyed
	ObjectId			m_networkId;		// the net id of the pickup being destroyed
	bool				m_pickupPlacement;	
	bool				m_mapPlacement;	
};

// ================================================================================================================
// CUpdatePlayerScarsEvent - sent when script wants to forcibly update the local players scars to remote players
// ================================================================================================================
class CUpdatePlayerScarsEvent : public netGameEvent
{
public:

	CUpdatePlayerScarsEvent();

	const char* GetEventName() const { return "UPDATE_PLAYER_SCARS_EVENT"; }

	static void EventHandler(datBitBuffer &bitBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger();

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;
	virtual bool MustPersist() const {return false;}

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer,
                                                    unsigned &numBloodMarks, CPedBloodNetworkData *bloodMarkData,
                                                    unsigned &numScars,      CPedScarNetworkData  *scarData);
};

// ================================================================================================================
// CNetworkCheckExeSizeEvent - ask a remote player for the size of his exe and report if his size does not match the correct sizes.
// ================================================================================================================
class CNetworkCheckExeSizeEvent : public netGameEvent
{
public:
	CNetworkCheckExeSizeEvent() : netGameEvent(NETWORK_CHECK_EXE_SIZE_EVENT, false), m_ExeSize(0), m_ReceivingPlayer(INVALID_PLAYER_INDEX), m_IsReply(false), m_Sku(0) {;}
	CNetworkCheckExeSizeEvent(const PhysicalPlayerIndex toPlayerIndex, const s64 exeSize, const bool isReply);

	const char* GetEventName() const { return "NETWORK_CHECK_EXE_SIZE_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger( const PhysicalPlayerIndex toPlayerIndex, const u64 exeSize, const bool isreply = false);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool IsInScope(const netPlayer& player) const;
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	static const unsigned SIZEOF_SKU = 2;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	s64                  m_ExeSize;
	PhysicalPlayerIndex  m_ReceivingPlayer;
	bool                 m_IsReply;
	u32                  m_Sku;
};





// ================================================================================================================
// CNetworkCodeCRCFailedEvent - ask a remote player for any failed code CRCs and report if there's a mismatch
// ================================================================================================================
#define CNetworkCodeCRCFailedEvent CNetworkInfoChangeEvent
class CNetworkCodeCRCFailedEvent : public netGameEvent
{
public:
	CNetworkCodeCRCFailedEvent() : netGameEvent(NETWORK_CHECK_CODE_CRCS_EVENT, false), m_FailedCRCVersionAndType(0),
		m_FailedCRCAddress(0), m_FailedCRCValue(0), m_ReportedPlayer(INVALID_PLAYER_INDEX) {}
	CNetworkCodeCRCFailedEvent(const PhysicalPlayerIndex reportedPlayerIndex, const u32 crcFailVersionAndType, const u32 crcFailAddress, const u32 crcValue);

	const char* GetEventName() const { return "NETWORK_INFO_CHANGE_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const u32 crcFailVersionAndType, const u32 crcFailAddress, const u32 crcValue);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool IsInScope(const netPlayer& player) const;
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u32                  m_FailedCRCVersionAndType;
	u32                  m_FailedCRCAddress;
	u32                  m_FailedCRCValue;
	PhysicalPlayerIndex  m_ReportedPlayer;
};

// ================================================================================================================
// CNetworkPtFXEvent
// ================================================================================================================
class CNetworkPtFXEvent : public netGameEvent
{
public:
	CNetworkPtFXEvent();
	CNetworkPtFXEvent(atHashWithStringNotFinal ptfxhash, atHashWithStringNotFinal ptfxassethash, const CEntity* pEntity, int boneIndex, const Vector3& vfxpos, const Vector3& vfxrot, const float& scale, const u8& invertAxes, const bool& bHasColor, const u8& r, const u8& g, const u8& b, const bool& bHasAlpha, const float& alpha, bool ignoreScopeChecks = false);

	const char* GetEventName() const { return "NETWORK_PTFX_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(atHashWithStringNotFinal ptfxhash, atHashWithStringNotFinal ptfxassethash, const CEntity* pEntity, int boneIndex, const Vector3& vfxpos, const Vector3& vfxrot, const float& scale, const u8& invertAxes, const float& r, const float& g, const float& b, const float& alpha, bool ignoreScopeChanges = false);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool IsInScope(const netPlayer& player) const;
	virtual bool MustPersist() const {return false;}

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	static const float        PTFX_CLONE_RANGE_SQ;
	static const float        PTFX_CLONE_RANGE_SNIPER_SQ;

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	Vector3		m_FxPos;
	Vector3		m_FxRot;
	ObjectId	m_EntityId;
	u32			m_PtFXHash;
	u32			m_PtFXAssetHash;
	s32			m_boneIndex;
	float		m_Scale;
	float		m_alpha;
	u8			m_InvertAxes;
	u8			m_r;
	u8			m_g;
	u8			m_b;
	bool		m_bUseEntity;
	bool		m_bUseBoneIndex;
	bool		m_bHasColor;
	bool		m_bHasAlpha;
	bool        m_ignoreScopeChecks;
};

// ================================================================================================================
// CPedSeenDeadPedEvent - sent when a local ped has seen a dead ped if flagged to generate events for this
// ================================================================================================================
class CNetworkPedSeenDeadPedEvent : public netGameEvent
{
public:
	CNetworkPedSeenDeadPedEvent();

	const char* GetEventName() const { return "PED_SEEN_DEAD_PED_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPed* pDeadPed, const CPed* pFindingPed);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool MustPersist() const { return false; }

	bool AddSightingToEvent(const ObjectId& deadPedId, const ObjectId& finderPedId);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	struct SightingInfo
	{
		SightingInfo() : m_DeadPedId(NETWORK_INVALID_OBJECT_ID), m_FindingPedId(NETWORK_INVALID_OBJECT_ID) {}
		ObjectId		m_DeadPedId;            // the id of the dead ped
		ObjectId		m_FindingPedId;         // the id of the ped that saw the dead ped
	};
	
	static const unsigned int SIZEOF_NUM_SIGHTINGS = 4; // bits
	static const unsigned int MAX_SYNCED_SIGHTINGS = ((1<<SIZEOF_NUM_SIGHTINGS) - 1);
	u32				m_numDeadSeen;
	SightingInfo	m_allSightings[MAX_SYNCED_SIGHTINGS];
};

// ================================================================================================================
// CRemoveStickyBombEvent - sent to request a sticky bomb to be removed that is owned by a remote player
// ================================================================================================================
class CRemoveStickyBombEvent : public netGameEvent
{
public:

	CRemoveStickyBombEvent();
	CRemoveStickyBombEvent(const CNetFXIdentifier &netIdentifier);

	const char* GetEventName() const { return "REMOVE_STICKY_BOMB_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CNetFXIdentifier &netIdentifier);

	bool AddStickyBomb(const CNetFXIdentifier &netIdentifier);

	virtual bool IsInScope(const netPlayer& player) const;
	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const int MAX_STICKY_BOMBS = 5;

	CNetFXIdentifier m_netIdentifiers[MAX_STICKY_BOMBS];
	u32 m_numStickyBombs;
};

// ================================================================================================================
// CInformSilencedGunShotEvent - sent to inform a remote ped that is not responding to clone silenced gun shots about a local silenced gun shot
// ================================================================================================================
class CInformSilencedGunShotEvent : public netGameEvent
{
public:

    CInformSilencedGunShotEvent();
    CInformSilencedGunShotEvent(CPed *targetPed, CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle);

    const char* GetEventName() const { return "INFORM_SILENCED_GUNSHOT_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(CPed *targetPed, CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle);

    virtual bool IsInScope(const netPlayer& player) const;
    virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
    virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    PhysicalPlayerIndex m_TargetPlayerID;      // Physical player index to send this event to
    ObjectId            m_FiringPedID;         // ID of the ped firing the shot
    Vector3             m_ShotPos;             // Shot position (origin position)
    Vector3             m_ShotTarget;          // Shot target (impact position)
    u32                 m_WeaponHash;          // Hash of silenced weapon that has been fired
    bool                m_MustBeSeenInVehicle; // Indicates this gun shot must be seen if the target ped is in a vehicle
};

// ================================================================================================================
// CPedPlayPainEvent - send to get a ped to play pain response audio
// ================================================================================================================
class CPedPlayPainEvent : public netGameEvent
{
public:

    CPedPlayPainEvent();
    CPedPlayPainEvent(const CPed *ped, int damageReason, float rawDamage);

    const char* GetEventName() const { return "PED_PLAY_PAIN_EVENT"; }

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const CPed *ped, int damageReason, float rawDamage);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool MustPersist() const { return false; }

#if ENABLE_NETWORK_LOGGING
    virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

    template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

    void AddPlayPainData(const CPed *ped, int damageReason, float rawDamage);

    struct PlayPainData
    {
        PlayPainData() :
        m_TargetPedID(NETWORK_INVALID_OBJECT_ID)
        , m_DamageReason(0)
        , m_RawDamage(0.0f)
        {
        }

        ObjectId m_TargetPedID;  // Target ped
        int      m_DamageReason; // Damage reason
        float    m_RawDamage;    // Amount of damage
    };

    static const unsigned MAX_NUM_PLAY_PAIN_DATA    = 5;
    static const unsigned SIZEOF_NUM_PLAY_PAIN_DATA = datBitsNeeded<MAX_NUM_PLAY_PAIN_DATA>::COUNT;

    unsigned     m_NumPlayPainData;
    PlayPainData m_PlayPainData[MAX_NUM_PLAY_PAIN_DATA];
};

// ================================================================================================================
// CCachePlayerHeadBlendDataEvent - sends out the default head blend for the local player
// ================================================================================================================
class CCachePlayerHeadBlendDataEvent : public netGameEvent
{
public:

	CCachePlayerHeadBlendDataEvent();
	CCachePlayerHeadBlendDataEvent(const CPedHeadBlendData& headBlendData, const netPlayer *pPlayerToBroadcastTo = NULL);

	const char* GetEventName() const { return "CACHE_PLAYER_HEAD_BLEND_DATA_EVENT"; }

	void SetPedHeadBlendData(const CPedHeadBlendData& headBlendData) { m_headBlendData = headBlendData; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPedHeadBlendData& headBlendData, const netPlayer *pPlayerToBroadcastTo = NULL);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	CPedHeadBlendData m_headBlendData;
	const netPlayer* m_pPlayerToBroadcastTo;
};

// ================================================================================================================
// CRemovePedFromPedGroupEvent - sends a request to the owner of ped group to remove a ped
// ================================================================================================================
class CRemovePedFromPedGroupEvent : public netGameEvent
{
public:

	CRemovePedFromPedGroupEvent();
	CRemovePedFromPedGroupEvent(ObjectId pedId, const CGameScriptObjInfo& pedScriptInfo);
	~CRemovePedFromPedGroupEvent();

	const char* GetEventName() const { return "REMOVE_PED_FROM_PEDGROUP_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const CPed& ped);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *messageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);
	
	bool TryToRemovePed(CPed* pPed);

	ObjectId			m_PedId;
	CGameScriptObjInfo	m_PedScriptInfo;
};

// ================================================================================================================
// CReportMyselfEvent - reports ourselves to other players when we locally detect cheating/tampering
// ================================================================================================================
#define CReportMyselfEvent CUpdateFxnEvent
class CReportMyselfEvent : public netGameEvent
{
public:
	CReportMyselfEvent();
    CReportMyselfEvent(const u32 report, const u32 reportParam);

    ~CReportMyselfEvent();

#if !__FINAL
    const char* GetEventName() const { return "REPORT_MYSELF_EVENT"; }
#else
    const char* GetEventName() const { return "UPDATE_FXN_EVENT"; }
#endif // __FINAL

    static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
    static void Trigger(const u32 report, const u32 reportParam = 0);

    virtual bool IsInScope( const netPlayer& player) const;
    virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
    virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);
    virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool eventLogOnly, datBitBuffer *messageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const unsigned int SIZEOF_REPORT = 32;

	u32  m_report;
	u32 m_reportParam;
};

// ================================================================================================================
// CReportCashSpawnEvent - reports cash spawning for other players too, in case they're blocking telemetry
// ================================================================================================================
class CReportCashSpawnEvent : public netGameEvent
{
public:

	CReportCashSpawnEvent();
	CReportCashSpawnEvent(u64 uid, int amount, int hash, ActivePlayerIndex toPlayer);
	~CReportCashSpawnEvent();

	const char* GetEventName() const { return "REPORT_CASH_SPAWN_EVENT"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(u64 uid, int amount, int hash, ActivePlayerIndex toPlayer);

	virtual bool IsInScope( const netPlayer& player) const;
	virtual void Prepare ( datBitBuffer &messageBuffer, const netPlayer &toPlayer );
	virtual void Handle ( datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide (const netPlayer &fromPlayer, const netPlayer &toPlayer);

private:

	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	u64 m_uid;
	int m_amount;
	int m_hash;
	ActivePlayerIndex m_toPlayer;
};

// ================================================================================================================
// CBlockWeaponSelectionEvent - sent when script is trying to block weapon selection on clone vehicles
// ================================================================================================================
class CBlockWeaponSelectionEvent : public netGameEvent
{
public:
	CBlockWeaponSelectionEvent(ObjectId vehicleId, bool blockWeapon);
	CBlockWeaponSelectionEvent();
	~CBlockWeaponSelectionEvent();

	const char* GetEventName() const { return "BLOCK_WEAPON_SELECTION"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(CVehicle& vehicle, bool blockWeapon);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool CanChangeScope() const { return false; }
	virtual bool operator==(const netGameEvent* pEvent) const;

	virtual bool MustPersist() const {return false;}
	virtual bool IsInScope(const netPlayer& player) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	bool m_BlockWeapon;
	ObjectId m_VehicleId;
};

#if RSG_PC

// ================================================================================================================
// CNetworkCheckCatalogCrc - Check the catalog crc of another player.
// ================================================================================================================
class CNetworkCheckCatalogCrc : public netGameEvent
{
public:
	CNetworkCheckCatalogCrc() 
		: netGameEvent(NETWORK_CHECK_CATALOG_CRC, false)
		, m_crc(0)
		, m_version(0)
		, m_IsReply(false) 
	{;}

	CNetworkCheckCatalogCrc(const bool isReply);

	const char* GetEventName() const { return "NETWORK_CHECK_CATALOG_CRC"; }

	static void EventHandler(datBitBuffer& msgBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer, const netSequence messageSeq, const EventId eventID, const unsigned eventIDSequence);
	static void Trigger(const bool isreply = false);

	virtual void Prepare(datBitBuffer &messageBuffer, const netPlayer &toPlayer);
	virtual void Handle(datBitBuffer &messageBuffer, const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool Decide(const netPlayer &fromPlayer, const netPlayer &toPlayer);
	virtual bool IsInScope(const netPlayer& player) const;
	virtual bool operator==(const netGameEvent* pEvent) const;

#if ENABLE_NETWORK_LOGGING
	virtual void WriteEventToLogFile(bool wasSent, bool bEventLogOnly, datBitBuffer *pMessageBuffer) const;
#endif // ENABLE_NETWORK_LOGGING

private:
	template <class Serialiser> void SerialiseEvent(datBitBuffer &messageBuffer);

	static const unsigned int SIZEOF_VERSION = 13;

	u32    m_crc;
	u32    m_version;
	bool   m_IsReply;
};

#endif //RSG_PC


class CNetworkEventsDumpInfo
{
public:
	CNetworkEventsDumpInfo() {}

	struct eventDumpDetails
	{
		unsigned count;
		const char *eventName;
	};

	static void UpdateNetworkEventsCrashDumpData();

	static atFixedArray<eventDumpDetails, netGameEvent::NETEVENTTYPE_MAXTYPES> ms_eventTypesCount;
	static atBinaryMap<eventDumpDetails, u32> ms_scriptedEvents;
};

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// UPDATE 'LARGEST_NETWORKEVENT_CLASS' if any new classes are added that are larger than it
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

namespace NetworkEventTypes
{
    void RegisterNetworkEvents();
}

#endif  // NETWORK_EVENTTYPES_H
