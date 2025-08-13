//
// name:        PlayerSyncNodes.h
// description: Network sync nodes used by CNetObjPlayerPeds
// written by:    John Gurney
//

#ifndef PLAYER_SYNC_NODES_H
#define PLAYER_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/PedSyncNodes.h"

#include "rline/rlsessioninfo.h"
#include "rline/clan/rlclancommon.h"

// framework includes
#include "fwmaths/Angle.h"
#include "fwscene/world/WorldLimits.h"

// game includes
#include "PedGroup/PedGroup.h"
#include "Peds/Rendering/PedDamage.h"
#include "Peds/rendering/PedOverlay.h"
#include "Peds/PlayerInfo.h"

class CPlayerCreationDataNode;
class CPlayerSectorPosNode;
class CPlayerCameraDataNode;
class CPlayerGameStateDataNode;
class CPlayerAppearanceDataNode;
class CPlayerPedGroupDataNode;
class CPlayerWantedAndLOSDataNode;
class CPlayerAmbientModelStreamingNode;
class CPlayerGamerDataNode;
class CPlayerExtendedGameStateNode;

const unsigned SIZEOF_LOUDNESS = 5;

///////////////////////////////////////////////////////////////////////////////////////
// IPlayerNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the player nodes.
// Any class that wishes to send/receive data contained in the player nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPlayerNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IPlayerNodeDataAccessor);

    struct PlayerGameStateFlags
    {
        bool controlsDisabledByScript;
        bool newMaxHealthArmour;
		bool hasSetJackSpeed;
        bool isSpectating;
        bool isAntagonisticToPlayer;
        bool neverTarget;
        bool useKinematicPhysics;
        bool inTutorial;
		bool pendingTutorialSessionChange;      
        bool respawning;
		bool willJackAnyPlayer;
		bool willJackWantedPlayersRatherThanStealCar;
		bool dontDragMeOutOfCar;
		bool playersDontDragMeOutOfCar;
        bool randomPedsFlee;
		bool everybodyBackOff;
		bool bHasMicrophone;
		bool bInvincible;
#if !__FINAL
		bool bDebugInvincible;
#endif
		bool bHasMaxHealth;
        bool myVehicleIsMyInteresting;
		u8 cantBeKnockedOffBike;
		bool bHelmetHasBeenShot;
        bool notDamagedByBullets;
        bool notDamagedByFlames;
        bool ignoresExplosions;
        bool notDamagedByCollisions;
        bool notDamagedByMelee;
        bool notDamagedBySmoke;
        bool notDamagedBySteam;
		bool playerIsWeird;
		bool noCriticalHits;
		bool disableHomingMissileLockForVehiclePedInside;
		bool ignoreMeleeFistWeaponDamageMult;
		bool lawPedsCanFleeFromNonWantedPlayer;
		bool hasHelmet;
		bool isSwitchingHelmetVisor;
		bool dontTakeOffHelmet;
		bool forceHelmetVisorSwitch;
		bool isPerformingVehicleMelee;
		bool useOverrideFootstepPtFx;
		bool disableVehicleCombat;
		bool treatFriendlyTargettingAndDamage;
		bool useKinematicModeWhenStationary;
		bool dontActivateRagdollFromExplosions;
		bool allowBikeAlternateAnimations;
		bool useLockpickVehicleEntryAnimations;
		bool m_PlayerPreferFrontSeat;
		bool ignoreInteriorCheckForSprinting;
		bool dontActivateRagdollFromVehicleImpact;
		bool swatHeliSpawnWithinLastSpottedLocation;
		bool disableStartEngine;
		bool lawOnlyAttackIfPlayerIsWanted;
		bool disableHelmetArmor;
		bool pedIsArresting;
		bool isScuba;

		// PRF
		bool PRF_BlockRemotePlayerRecording;
		bool PRF_UseScriptedWeaponFirePosition;
    };

    // getter functions
    virtual void GetPlayerCreateData(CPlayerCreationDataNode& data) const = 0;

    virtual void GetPlayerSectorPosData(CPlayerSectorPosNode& data) const = 0;

    virtual void GetPlayerGameStateData(CPlayerGameStateDataNode& data) = 0;

    virtual void GetPlayerAppearanceData(CPlayerAppearanceDataNode& data) = 0;
    virtual void GetCameraData(CPlayerCameraDataNode& data) = 0;
    virtual void GetPlayerPedGroupData(CPlayerPedGroupDataNode& data) = 0;
    virtual void GetPlayerWantedAndLOSData(CPlayerWantedAndLOSDataNode& data) = 0;
    virtual void GetAmbientModelStreamingData(CPlayerAmbientModelStreamingNode& data) = 0;
	virtual void GetPlayerGamerData(CPlayerGamerDataNode& data) = 0;
	virtual void GetPlayerExtendedGameStateData(CPlayerExtendedGameStateNode& data) = 0;

    // setter functions
    virtual void SetPlayerCreateData(const CPlayerCreationDataNode& data) = 0;

    virtual void SetPlayerSectorPosData(const CPlayerSectorPosNode& data) = 0;

    virtual void SetPlayerGameStateData(const CPlayerGameStateDataNode& data) = 0;

    virtual void SetPlayerAppearanceData(CPlayerAppearanceDataNode& data) = 0;
    virtual void SetCameraData(const CPlayerCameraDataNode& data) = 0;
    virtual void SetPlayerPedGroupData(CPlayerPedGroupDataNode& data) = 0;
    virtual void SetPlayerWantedAndLOSData(const CPlayerWantedAndLOSDataNode& data) = 0;
    virtual void SetAmbientModelStreamingData(const CPlayerAmbientModelStreamingNode& data) = 0;
	virtual void SetPlayerGamerData(const CPlayerGamerDataNode& data) = 0;
	virtual void SetPlayerExtendedGameStateData(const CPlayerExtendedGameStateNode& data) = 0;

protected:

    virtual ~IPlayerNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerCreationDataNode
//
// Handles the serialization of a player's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Player Creation", CPlayerCreationDataNode, IPlayerNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.GetPlayerCreateData(*this);
    }

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.SetPlayerCreateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned MAX_BLOOD_MARK_SYNCED = 6;
	static const unsigned MAX_SCARS_SYNCED = 5;

    u32						m_ModelHash;
	unsigned				m_NumBloodMarks;
	CPedBloodNetworkData	m_BloodMarkData[MAX_BLOOD_MARK_SYNCED];
    unsigned				m_NumScars;
    CPedScarNetworkData		m_ScarData[MAX_SCARS_SYNCED];
	bool					m_wearingAHelmet; //only want to apply this once and then it's derived locally...
	bool					m_hasCommunicationPrivileges;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerSectorPosNode
//
// Handles the serialization of a player's sector position. Also support syncing position
// relative to a network object the player is standing upon
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerSectorPosNode : public CSyncDataNodeFrequent
{
protected:

	NODE_COMMON_IMPL("Player Sector Pos", CPlayerSectorPosNode, IPlayerNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &nodeData);

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &nodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

#if __DEV
	static RecorderId ms_bandwidthRecorderId;
#endif

	static const float MAX_STEALTH_NOISE; 

	enum
	{
		STEALTH_NOISE_NONE,
		STEALTH_NOISE_LOW,
		STEALTH_NOISE_MED,
		STEALTH_NOISE_HIGH,
		NUM_STEALTH_NOISES
	};

	static const unsigned SIZEOF_STEALTH_NOISE    = datBitsNeeded<NUM_STEALTH_NOISES-1>::COUNT;

	static float ms_aStealthNoiseThresholds[NUM_STEALTH_NOISES];

	float    m_SectorPosX;                  // X position of this object within the current sector
    float    m_SectorPosY;                  // Y position of this object within the current sector
    float    m_SectorPosZ;                  // Z position of this object within the current sector
    bool     m_IsStandingOnNetworkObject;   // is this player currently standing on another network object?
    bool     m_IsRagdolling;                // is this player ragdolling?
    bool     m_IsOnStairs;                  // is this player standing on stairs?
    ObjectId m_StandingOnNetworkObjectID;   // ID of the object this player is standing on
    Vector3  m_LocalOffset;                 // Offset from the center of the object
	float	 m_StealthNoise;			    // the players current stealth noise
	u32		 m_PackedStealthNoise;		    // the serialised players current stealth noise 
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerCameraDataNode
//
// Handles the serialization of an player's camera.
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerCameraDataNode : public CSyncDataNodeFrequent
{
public:

    CPlayerCameraDataNode();

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

    NODE_COMMON_IMPL("Player Camera", CPlayerCameraDataNode, IPlayerNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.GetCameraData(*this);
    }

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.SetCameraData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

	static const float MAX_WEAPON_RANGE_LONG_RANGE;
	static const float MAX_WEAPON_RANGE_SHORT_RANGE;
	static const float MAX_LOCKON_TARGET_OFFSET; 

	Vector3  m_Position;  			        // the position offset of the camera if aiming - or absolute position if using a free camera
	Vector3  m_playerToTargetAimOffset;		// position we're aiming at (used to compute pitch and yaw on the clone).
	Vector3  m_lockOnTargetOffset;			// if locked onto a target, offset from target position to actual lock on pos.
	Vector3  m_LookAtPosition;
	float    m_eulersX;         			// camera matrix euler angles
	float    m_eulersZ;         			// camera matrix euler angles
	ObjectId m_targetId;					// if we're aiming at a target we pass that info instead of pitch and yaw.
	bool     m_UsingFreeCamera;             // if set, this player is controlling a free camera
	bool     m_UsingCinematicVehCamera;     // if set, this player is using the cinematic vehicle camera
    bool     m_morePrecision;   			// if set, more precise camera data is used
    bool     m_largeOffset;     			// if set, the camera is far away from the player
	bool	 m_aiming;						// if the player is currently aiming a weapon
	bool	 m_longRange;					// if the player is aiming a long range weapon (sniper rifle - 1500m range) or short range (<150m)
	bool	 m_freeAimLockedOnTarget;		// if the player is free aim locked onto a target...
	bool	 m_canOwnerMoveWhileAiming;		// can the owner move while aiming (changes based on aiming from hip / scope / weapon)
    bool     m_UsingLeftTriggerAimMode;     // is this player using the left trigger aim camera mode
	bool 	 m_isLooking;
	bool	 m_bAimTargetEntity;
#if FPS_MODE_SUPPORTED
	bool     m_usingFirstPersonCamera;
	bool	 m_usingFirstPersonVehicleCamera;
	bool	 m_inFirstPersonIdle;
	bool     m_stickWithinStrafeAngle;
	bool     m_usingSwimMotionTask;
	bool     m_onSlope;
#endif
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerGameStateDataNode
//
// Handles the serialization of a player's game state
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Player Game State", CPlayerGameStateDataNode, IPlayerNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData);

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned int MAX_PLAYERINFO_DECOR_ENTRIES = 3;
	static const unsigned SIZEOF_LOCKON_STATE = 2;

    u32                                           m_PlayerState;               // the current player state
    IPlayerNodeDataAccessor::PlayerGameStateFlags m_GameStateFlags;            // game state flags
    u32                                           m_MobileRingState;           // mobile phone ring state for the player
    s32                                           m_PlayerTeam;                // current player team
    float                                         m_AirDragMult;               // air drag multiplier
    u32                                           m_MaxHealth;                 // max health for the player
    u32                                           m_MaxArmour;                 // max armour for the player
	u32											  m_JackSpeed;				   // jack speed percentage for the player
    TeamFlags			                          m_IsTargettableByTeam;       // flags indicating whether the ped is targettable by each team
    PlayerFlags                                   m_OverrideReceiveChat;       // Override Receive Chat
	PlayerFlags                                   m_OverrideSendChat;          // Override Send Chat
	bool                                          m_bOverrideTutorialChat;     // Override Tutorial Chat
    bool                                          m_bOverrideTransitionChat;   // Override Transition Chat
	bool										  m_bIsSCTVSpectating;		   // true when player is SCTV spectator
	ObjectId                                      m_SpectatorId;               // Network Object of the ped we are spectating
    PhysicalPlayerIndex                           m_AntagonisticPlayerIndex;   // Antagonistic player index
    u8                                            m_TutorialIndex;             // Tutorial session index - used to split players into fake sessions including only team-mates
    u8                                            m_TutorialInstanceID;        // Current tutorial instance ID (only used for gang sessions)
    float										  m_fVoiceLoudness;			   // Loudness of player voice through microphone
	int											  m_nVoiceChannel;			   // Voice channel this player is in
    bool                                          m_bHasVoiceProximityOverride;// If we have a voice proximity override
    Vector3                                       m_vVoiceProximityOverride;   // Proximity override
	u32											  m_sizeOfNetArrayData;		   // the total size of all the network array handler data arbitrated by this player
	s32											  m_vehicleweaponindex;		   // Vehicle Weapon Index: Missiles, Gatling, etc...
	bool										  m_bvehicleweaponindex;	   // Indicator whether the VWI is sent
	u32  										  m_decoratorListCount;		   // count of decorator extensions ( the scripted ones )
	IDynamicEntityNodeDataAccessor::decEnt 		  m_decoratorList[MAX_PLAYERINFO_DECOR_ENTRIES];  // array of decorator extensions ( the scripted ones )
	bool										  m_bIsFriendlyFireAllowed;
	bool										  m_bIsPassiveMode;
    u8                                            m_GarageInstanceIndex;
	u8											  m_nPropertyID;
	u8											  m_nMentalState;
	WIN32PC_ONLY(u8								  m_nPedDensity;)
	bool										  m_bBattleAware;
    bool                                          m_VehicleJumpDown;
	float										  m_WeaponDefenseModifier;
	float										  m_WeaponMinigunDefenseModifier;
	bool									      m_bUseExtendedPopulationRange;
	Vector3										  m_vExtendedPopulationRangeCenter;
    u16                                           m_nCharacterRank;
	bool                                          m_FadeOut;
	bool                                          m_bDisableLeavePedBehind; //Disable Leave ped behind when the remote player leaves the session.
	bool										  m_bCollisionsDisabledByScript; // used for spectating players, that have other collision flags set
	bool										  m_bInCutscene; // player is in a mocap cutscene
	bool										  m_bGhost;
	bool										  m_bIsSuperJump;
	bool                                          m_EnableCrewEmblem;       // Does the ped use the crew emblem?
	bool										  m_ConcealedOnOwner;	// true when local player concealed themselves

	ObjectId									  m_LockOnTargetID;  //for when players use homing launchers
	unsigned									  m_LockOnState;
	float										  m_VehicleShareMultiplier;
	float										  m_WeaponDamageModifier;
	float										  m_MeleeDamageModifier;
	float										  m_MeleeUnarmedDamageModifier;
	bool										  m_bHasScriptedWeaponFirePos;
	Vector3										  m_ScriptedWeaponFirePos;

	// Arcade information
	u8 m_arcadeTeamInt;
	u8 m_arcadeRoleInt;
	bool m_arcadeCNCVOffender;
	u8 m_arcadePassiveAbilityFlags;
	
	bool										  m_bIsShockedFromDOTEffect;
	bool										  m_bIsChokingFromDOTEffect;
};

//For syncing player action anims in CPedAppearanceDataNode
class CSyncedPlayerSecondaryPartialAnim
{
public:
	CSyncedPlayerSecondaryPartialAnim()
	{
		Reset();
	}

	void Reset();

	void Serialise(CSyncDataBase& serialiser);

	static const unsigned int SIZEOF_ANIM = 32;
	static const unsigned int SIZEOF_DICT = 32;
	static const unsigned int SIZEOF_FLAGS = 32;
	static const unsigned int SIZEOF_BONEMASKHASH = 32;

	static const unsigned int SIZEOF_FLOATHASH = 32;
	static const unsigned int SIZEOF_STATEPARTIALHASH = 32;
	static const unsigned int SIZEOF_MOVENETDEFHASH = 32;
	static const unsigned int SIZEOF_MOVEFLOAT	= 9;

	u32		m_FloatHash0;
	u32		m_FloatHash1;
	u32		m_FloatHash2;
	u32		m_StatePartialHash;
	u32		m_MoveNetDefHash;
	float	m_Float0;
	float	m_Float1;
	float	m_Float2;

	u32		m_ClipHash;
	u32		m_DictHash;
	u32		m_Flags;
	u32		m_BoneMaskHash;
	u32     m_taskSequenceId;
	bool	m_Active;
	bool	m_SlowBlendDuration;
	bool	m_MoVEScripted; //Defines is this syncing CTaskScriptedAnimation or CTaskMoVEScripted
	bool	m_Phone;
	bool	m_PhoneSecondary;
	bool	m_IsBlocked;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerAppearanceDataNode
//
// Handles the serialization of a player's appearance
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerAppearanceDataNode : public CSyncDataNodeInfrequent
{
public:

	CPlayerAppearanceDataNode();

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

    NODE_COMMON_IMPL("Player Appearance", CPlayerAppearanceDataNode, IPlayerNodeDataAccessor);

	bool IsAlwaysSentWithCreate() const { return true; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData);

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

	u32               m_PackedDecorations[NUM_DECORATION_BITFIELDS]; // texture preset hashes (looked up from collection)
	CSyncedPedVarData m_VariationData;								 // ped variation data
	CPackedPedProps   m_PedProps;                                    // ped props data
	CPedHeadBlendData m_HeadBlendData;                               // custom head blend data
	CSyncedPlayerSecondaryPartialAnim m_secondaryPartialAnim;
	u32				  m_networkedDamagePack;
    u32               m_NewModelHash;                                // model index for player
    u32               m_VoiceHash;                                   // voice hash code
	u32				  m_phoneMode;
	u32				  m_crewEmblemVariation;						 //
	u8				  m_parachuteTintIndex;							 // Colour of the players' parachute
	u8				  m_parachutePackTintIndex;						 // Colour of the players' parachute pack
    ObjectId          m_RespawnNetObjId;                             // ID of the ped used for Team Swapping
    bool              m_HasHeadBlendData;                            // does this player have custom head data?
    bool              m_HasDecorations;                              // number of decorations (medals/tattoos)
	bool              m_HasRespawnObjId;                             // has a valid respawn object id
	fwMvClipSetId	  m_facialClipSetId;							// the facial clipset used by the player
	u32				  m_facialIdleAnimOverrideClipNameHash;			// the facial clip used by the player
	u32				  m_facialIdleAnimOverrideClipDictNameHash;		// the dictionary used by the player
	bool			  m_isAttachingHelmet;							// are we attaching a helmet via TaskMotionInAutomobile::State_PutOnHelmet
	bool			  m_isRemovingHelmet;							 // are we playing secondary priority removing helmet anim?
	bool			  m_isWearingHelmet;							// are we wearing a helmet (needed for when we aborting putting one on)
	u8				  m_helmetTextureId;							// which helmet are we about to put on?
	u16				  m_helmetProp;									// which helmet prop are we using?
	u16				  m_visorPropUp;									// which helmet prop are we using?
	u16				  m_visorPropDown;									// which helmet prop are we using?
	bool			  m_visorIsUp;
	bool			  m_supportsVisor;
	bool			  m_isVisorSwitching;
	u8				  m_targetVisorState;
	u32               m_crewLogoTxdHash;
	u32            	  m_crewLogoTexHash;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerPedGroupDataNode
//
// Handles the serialization of a player's ped group
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerPedGroupDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Player Ped Group", CPlayerPedGroupDataNode, IPlayerNodeDataAccessor);

private:

    virtual bool GetIsManualDirty() const { return true; }

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.GetPlayerPedGroupData(*this);
    }

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.SetPlayerPedGroupData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

    CPedGroup m_pedGroup;
};


///////////////////////////////////////////////////////////////////////////////////////
// CPlayerWantedAndLOSDataNode
//
// Handles the serialization of a player's wanted level data and LOS flags to other players
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerWantedAndLOSDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Player Wanted & LOS", CPlayerWantedAndLOSDataNode, IPlayerNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.GetPlayerWantedAndLOSData(*this);
    }

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.SetPlayerWantedAndLOSData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

    virtual bool CanHaveDefaultState() const { return true; }
    virtual bool HasDefaultState() const;
    virtual void SetDefaultState();

public:

	Vector3 m_lastSpottedLocation;
	Vector3 m_searchAreaCentre;
    u32     m_wantedLevel;
	u32	    m_WantedLevelBeforeParole;
    u32     m_timeLastSpotted;
    u32     m_visiblePlayers;
	u32     m_timeFirstSpotted;
	u8		m_fakeWantedLevel;
	bool	m_HasLeftInitialSearchArea;
	bool	m_copsAreSearching;
	bool    m_causedByThisPlayer;
	bool    m_bIsOutsideCircle;
	PhysicalPlayerIndex m_causedByPlayerPhysicalIndex;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPlayerAmbientModelStreamingNode
//
// Handles the serialization of a player's wanted level data and LOS flags to other players
///////////////////////////////////////////////////////////////////////////////////////
class CPlayerAmbientModelStreamingNode : public CSyncDataNodeInfrequent
{
public:

    CPlayerAmbientModelStreamingNode();

protected:

    NODE_COMMON_IMPL("Player Ambient Model Streaming", CPlayerAmbientModelStreamingNode, IPlayerNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.GetAmbientModelStreamingData(*this);
    }

    void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
    {
        playerNodeData.SetAmbientModelStreamingData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

    u32      m_AllowedPedModelStartOffset;
    u32      m_AllowedVehicleModelStartOffset;
    s32      m_TargetVehicleEntryPoint;
	ObjectId m_TargetVehicleForAnimStreaming;
};

class CPlayerGamerDataNode : public CSyncDataNodeInfrequent
{ 
protected:
	NODE_COMMON_IMPL("Player Gamer Data", CPlayerGamerDataNode, IPlayerNodeDataAccessor );

	virtual bool GetIsManualDirty() const { return true; }
	virtual bool IsAlwaysSentWithCreate() const { return true; }

private:
	void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData);
	void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData);
	void Serialise(CSyncDataBase& serialiser);

public:

	//Crew (clan) info
	rlClanMembershipData m_clanMembership;
	bool m_bNeedToSerialiseRankSystemFlags;
	bool m_bNeedToSerialiseCrewRankTitle;
	char m_crewRankTitle[RL_CLAN_NAME_MAX_CHARS];

	bool m_bHasStartedTransition;
	bool m_bHasTransitionInfo;
	char m_aInfoBuffer[rlSessionInfo::TO_STRING_BUFFER_SIZE];

	bool m_bIsRockstarDev;
	bool m_bIsRockstarQA;
	bool m_bIsCheater;

    unsigned m_nMatchMakingGroup;

	//Muting data (for doing auto-mute stuff)
	bool m_bNeedToSerialiseMuteData;
	u32 m_muteCount;
	u32 m_muteTotalTalkersCount;

	PlayerFlags m_kickVotes;

    bool m_hasCommunicationPrivileges;

	PlayerAccountId m_playerAccountId;
};


class CPlayerExtendedGameStateNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Extended Player Game State", CPlayerExtendedGameStateNode, IPlayerNodeDataAccessor);

private:

	void ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData);
	void ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData);
	void Serialise(CSyncDataBase& serialiser);

public:

	float m_fxWaypoint;
	float m_fyWaypoint;
	ObjectId m_WaypointObjectId;
	bool m_bHasActiveWaypoint;
	bool m_bOwnsWaypoint;
	unsigned m_WaypointLocalDirtyTimestamp;
    float m_CityDensity;
	float m_fovRatio;        			// camera fov ratio
	float m_aspectRatio;					// camera aspect ratio
	float m_MaxExplosionDamage;
	PlayerFlags m_ghostPlayers;				// set when the player is to only be ghosted with specific players
};

#endif  // PLAYER_SYNC_NODES_H
