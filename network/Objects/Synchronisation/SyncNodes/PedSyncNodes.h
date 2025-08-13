//
// name:        PedSyncNodes.h
// description: Network sync nodes used by CNetObjPeds
// written by:    John Gurney
//

#ifndef PED_SYNC_NODES_H
#define PED_SYNC_NODES_H

#include "fwmaths/Angle.h"

#include "network/general/NetworkUtil.h"
#include "network/objects/synchronisation/SyncNodes/PhysicalSyncNodes.h"
#include "network/objects/synchronisation/SyncNodes/ProximityMigrateableSyncNodes.h"
#include "Peds/CombatData.h"
#include "Peds/PedIntelligence/PedPerception.h"
#include "peds/PedMoveBlend/PedMoveBlendBase.h"
#include "peds/PedMotionData.h"
#include "peds/PedTaskRecord.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/QueriableInterface.h"
#include "script/handlers/GameScriptMgr.h"
#include "script/handlers/GameScriptIds.h"
#include "Vehicles/componentReserve.h"
#include "weapons/inventory/AmmoItemRepository.h"
#include "weapons/inventory/PedInventory.h"
#include "weapons/WeaponTypes.h"
#include "Peds/CombatData.h"
#include "ik/IkManager.h"

class CPedCreationDataNode;
class CPedScriptCreationDataNode;
class CPedGameStateDataNode;
class CPedSectorPosMapNode;
class CPedSectorPosNavMeshNode;
class CPedComponentReservationDataNode;
class CPedScriptGameStateDataNode;
class CPedOrientationDataNode;
class CPedHealthDataNode;
class CPedAttachDataNode;
class CPedMovementGroupDataNode;
class CPedMovementDataNode;
class CPedAIDataNode;
class CPedTaskTreeDataNode;
class CPedTaskSpecificDataNode;
class CPedInventoryDataNode;
class CPedTaskSequenceDataNode;

static const float MAX_SYNCED_SHOOT_RATE = 10.0f; 

///////////////////////////////////////////////////////////////////////////////////////
// IPedNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the ped nodes.
// Any class that wishes to send/receive data contained in the ped nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IPedNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IPedNodeDataAccessor);

    struct TaskSlotData
    {
        u32     m_taskType;
        u32     m_taskPriority;
        u32     m_taskTreeDepth;
		u32     m_taskSequenceId;
		bool    m_taskActive;
    };

	//IMPORTANT NOTE: If a new flag is added here, please update IsScriptPedConfigFlagSynchronized method below to include the flag so it won't assert as not being synchronized.
    struct ScriptPedFlags
    {
		unsigned int cantBeKnockedOffBike;
        bool neverTargetThisPed;
        bool blockNonTempEvents;
        bool noCriticalHits;
        bool isPriorityTarget;
        bool doesntDropWeaponsWhenDead;
		bool dontDragMeOutOfCar;
		bool playersDontDragMeOutOfCar;
		bool drownsInWater;
		bool drownsInSinkingVehicle;
        bool forceDieIfInjured;
		bool fallOutOfVehicleWhenKilled;
        bool diesInstantlyInWater;
		bool justGetsPulledOutWhenElectrocuted;
		bool dontRagdollFromPlayerImpact;
		bool lockAnim;
		bool preventPedFromReactingToBeingJacked;
		bool dontInfluenceWantedLevel;
		bool getOutUndriveableVehicle;
		bool moneyHasBeenGivenByScript;
		bool shoutToGroupOnPlayerMelee;
		bool canDrivePlayerAsRearPassenger;
		bool keepRelationshipGroupAfterCleanup;
		bool disableLockonToRandomPeds;
		bool willNotHotwireLawEnforcementVehicle;
		bool willCommandeerRatherThanJack;
		bool useKinematicModeWhenStationary;
		bool forcedToUseSpecificGroupSeatIndex;
		bool playerCanJackFriendlyPlayers;
		bool willFlyThroughWindscreen;
		bool getOutBurningVehicle;
		bool disableLadderClimbing;
        bool forceDirectEntry;
		bool phoneDisableCameraAnimations;
		bool notAllowedToJackAnyPlayers;
		bool runFromFiresAndExplosions;
		bool ignoreBeingOnFire;
		bool disableWantedHelicopterSpawning;
		bool ignoreNetSessionFriendlyFireCheckForAllowDamage;
		bool checklockedbeforewarp;
		bool dontShuffleInVehicleToMakeRoom;
		bool disableForcedEntryForOpenVehicles;
		bool giveWeaponOnGetup;
		bool dontHitVehicleWithProjectiles;
		bool dontLeaveCombatIfTargetPlayerIsAttackedByPolice;
		bool useTargetPerceptionForCreatingAimedAtEvents;
		bool disableHomingMissileLockon;
		bool forceIgnoreMaxMeleeActiveSupportCombatants;
		bool stayInDefensiveAreaWhenInVehicle;
		bool dontShoutTargetPosition;
		bool preventExitVehicleDueToInvalidWeapon;
		bool disableHelmetArmor;
		bool upperBodyDamageAnimsOnly;
		bool listenToSoundEvents;
		bool openDoorArmIK;
		bool dontBlipCop;
		bool forceSkinCharacterCloth;
		bool phoneDisableTalkingAnimations;
		bool lowerPriorityOfWarpSeats;
		bool dontBehaveLikeLaw;
		bool phoneDisableTextingAnimations;
		bool dontActivateRagdollFromBulletImpact;
		bool checkLOSForSoundEvents;
		bool disablePanicInVehicle;
		bool forceRagdollUponDeath;
		bool pedIgnoresAnimInterruptEvents;
		bool dontBlip;
		bool preventAutoShuffleToDriverSeat;
		bool canBeAgitated;
        bool AIDriverAllowFriendlyPassengerSeatEntry;
		bool onlyWritheFromWeaponDamage;
		bool shouldChargeNow;
        bool disableJumpingFromVehiclesAfterLeader;
		bool preventUsingLowerPrioritySeats;
		bool preventDraggedOutOfCarThreatResponse;
		bool canAttackNonWantedPlayerAsLaw;
		bool disableMelee;
		bool preventAutoShuffleToTurretSeat;
		bool disableEventInteriorStatusCheck;
		bool onlyUpdateTargetWantedIfSeen;
		bool treatDislikeAsHateWhenInCombat;
		bool treatNonFriendlyAsHateWhenInCombat;
        bool preventReactingToCloneBullets;
		bool disableAutoEquipHelmetsInBikes;
		bool disableAutoEquipHelmetsInAircraft;
		bool stopWeaponFiringOnImpact;
		bool keepWeaponHolsteredUnlessFired;
		bool disableExplosionReactions;
		bool dontActivateRagdollFromExplosions;
		bool avoidTearGas;
		bool disableHornAudioWhenDead;
		bool ignoredByAutoOpenDoors;
		bool activateRagdollWhenVehicleUpsideDown;
		bool allowNearbyCoverUsage;
		bool cowerInsteadOfFlee;
		bool disableGoToWritheWhenInjured;
		bool blockDroppingHealthSnacksOnDeath;
		bool willTakeDamageWhenVehicleCrashes;
		bool dontRespondToRandomPedsDamage;
		bool dontTakeOffHelmet;
		bool allowContinuousThreatResponseWantedLevelUpdates;
		bool keepTargetLossResponseOnCleanup;
		bool forcedToStayInCover;
		bool broadcastRepondedToThreatWhenGoingToPointShooting;
		bool blockPedFromTurningInCover;
		bool teleportToLeaderVehicle;
		bool dontLeaveVehicleIfLeaderNotInVehicle;
		bool forcePedToFaceLeftInCover;
		bool forcePedToFaceRightInCover;
		bool useNormalExplosionDamageWhenBlownUpInVehicle;
		bool dontClearLocalPassengersWantedLevel;
		bool useGoToPointForScenarioNavigation;
		bool hasBareFeet;
		bool blockAutoSwapOnWeaponPickups;
        bool isPriorityTargetForAI;
		bool hasHelmet;
		bool isSwitchingHelmetVisor;
		bool forceHelmetVisorSwitch;
		bool isPerformingVehMelee;
		bool useOverrideFootstepPtFx;
		bool dontAttackPlayerWithoutWantedLevel;
        bool disableShockingEvents;
		bool treatAsFriendlyForTargetingAndDamage;
		bool allowBikeAlternateAnimations;
		bool useLockpickVehicleEntryAnimations;
		bool ignoreInteriorCheckForSprinting;
		bool dontActivateRagdollFromVehicleImpact;
        bool avoidanceIgnoreAll;
		bool pedsJackingMeDontGetIn;
		bool disableTurretOrRearSeatPreference;
		bool disableHurt;
		bool allowTargettingInVehicle;
		bool firesDummyRockets;
		bool decoyPed;
		bool hasEstablishedDecoy;
		bool disableInjuredCryForHelpEvents;
		bool dontCryForHelpOnStun;
		bool blockDispatchedHelicoptersFromLanding;
		bool dontChangeTargetFromMelee;
		bool disableBloodPoolCreation;
		bool ragdollFloatsIndefinitely;
		bool blockElectricWeaponDamage;

		//PRF
		bool PRF_disableInVehicleActions;
		bool PRF_IgnoreCombatManager;
	};

	struct ScriptPedPerception
	{
		u32 m_HearingRange;
		u32 m_SeeingRange;
		u32 m_SeeingRangePeripheral;
		float m_CentreOfGazeMaxAngle;
		float m_VisualFieldMinAzimuthAngle;
		float m_VisualFieldMaxAzimuthAngle;
		bool m_bIsHighlyPerceptive;

		ScriptPedPerception() 
			: m_HearingRange(static_cast< u32 >(CPedPerception::ms_fSenseRange))
			, m_SeeingRange(static_cast< u32 >(CPedPerception::ms_fSenseRange))
			, m_SeeingRangePeripheral(static_cast< u32 >(CPedPerception::ms_fSenseRangePeripheral))
			, m_CentreOfGazeMaxAngle(CPedPerception::ms_fCentreOfGazeMaxAngle)
			, m_VisualFieldMinAzimuthAngle(CPedPerception::ms_fVisualFieldMinAzimuthAngle)
			, m_VisualFieldMaxAzimuthAngle(CPedPerception::ms_fVisualFieldMaxAzimuthAngle)
			, m_bIsHighlyPerceptive(false)
		{
		}
	};

	struct PedArrestFlags
	{
		bool	 m_isHandcuffed;
		bool	 m_canPerformArrest;
		bool	 m_canPerformUncuff;
		bool	 m_canBeArrested;
		bool	 m_isInCustody;

		PedArrestFlags() 
		: m_isHandcuffed(false)
		, m_canPerformArrest(false)
		, m_canPerformUncuff(false)
		, m_canBeArrested(false)
		, m_isInCustody(false)
		{
		}
	};

	typedef void (*fnTaskDataLogger)(netLoggingInterface *log, u32 taskType, const u8 *taskData, const int numBits);

	class CTaskData
	{
	public:

		u32 m_TaskType;
		u32 m_TaskDataSize;
		u8  m_TaskData[CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE];

		template <class Serialiser>
		void Serialise(Serialiser &serialiser, fnTaskDataLogger taskDataLogFunction, bool bSerialiseTaskType)
		{
			static const unsigned SIZEOF_TASK_DATA_SIZE = datBitsNeeded<CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE>::COUNT;

			if (bSerialiseTaskType || taskDataLogFunction)
			{
				SERIALISE_TASKTYPE(serialiser, m_TaskType, "Task Type");
			}

			if(serialiser.GetIsMaximumSizeSerialiser())
			{
				m_TaskDataSize = CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE;
			}

			SERIALISE_UNSIGNED(serialiser, m_TaskDataSize, SIZEOF_TASK_DATA_SIZE, "m_TaskDataSize");

			if (m_TaskDataSize > 0)
			{
				gnetAssertf(m_TaskDataSize <= CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE, "Task %s is larger than MAX_SPECIFIC_TASK_INFO_SIZE, increase to %d", TASKCLASSINFOMGR.GetTaskName(m_TaskType), m_TaskDataSize);

				SERIALISE_DATABLOCK(serialiser, m_TaskData, m_TaskDataSize, "m_TaskData");

				if (taskDataLogFunction)
				{
					(*taskDataLogFunction)(serialiser.GetLog(), m_TaskType, m_TaskData, m_TaskDataSize);
				}
			}
		}
	};

	static const unsigned int MAX_SYNCED_WEAPON_COMPONENTS = 11;  //MAX_WEAPON_COMPONENTS
    static const unsigned int MAX_SYNCED_GADGETS = 3;
    static const unsigned int NUM_TASK_SLOTS     = 8;

	static const unsigned int MAX_SYNCED_SEQUENCE_TASKS = 10;  

    // getter functions
    virtual void GetPedCreateData(CPedCreationDataNode& data) = 0;

    virtual void GetScriptPedCreateData(CPedScriptCreationDataNode& data) = 0;

    virtual void GetSectorPosMapNode(CPedSectorPosMapNode& data) = 0;

    virtual void GetSectorNavmeshPosition(CPedSectorPosNavMeshNode& data) = 0;

    virtual void GetPedHeadings(CPedOrientationDataNode& data) = 0;

    virtual void GetPedGameStateData(CPedGameStateDataNode& data) = 0;

    virtual void GetPedScriptGameStateData(CPedScriptGameStateDataNode& data) = 0;

    virtual void GetPedHealthData(CPedHealthDataNode& data) = 0;

    virtual void GetAttachmentData(CPedAttachDataNode& data) = 0;

    virtual void GetMotionGroup(CPedMovementGroupDataNode& data) = 0;

    virtual void GetPedMovementData(CPedMovementDataNode& data) = 0;

    virtual void GetPedAppearanceData(CPackedPedProps &pedProps, CSyncedPedVarData& variationData, u32 &phoneMode, u8& parachuteTintIndex, u8& parachutePackTintIndex, fwMvClipSetId& facialClipSetId, u32 &facialIdleAnimOverrideClipNameHash, u32 &facialIdleAnimOverrideClipDictHash, bool& isAttachingHelmet, bool& isRemovingHelmet, bool& isWearingHelmet, u8& helmetTextureId, u16& helmetProp, u16& visorUpProp, u16& visorDownProp, bool& visorIsUp, bool& supportsVisor, bool& isVisorSwitching, u8& targetVisorState) = 0;

    virtual void GetAIData(CPedAIDataNode& data) = 0;

	virtual void GetTaskTreeData(CPedTaskTreeDataNode& data) = 0;

	virtual void GetTaskSpecificData(CPedTaskSpecificDataNode& data) = 0;

	virtual void GetPedInventoryData(CPedInventoryDataNode& data) = 0;
                                     
    virtual void GetPedComponentReservations(CPedComponentReservationDataNode& data) = 0;

	virtual void GetTaskSequenceData(bool& hasSequence, u32& sequenceResourceId, u32& numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32& repeatMode) = 0;

    // setter functions
    virtual void SetPedCreateData(const CPedCreationDataNode& data) = 0;

    virtual void SetScriptPedCreateData(const CPedScriptCreationDataNode& data) = 0;

    virtual void SetSectorPosMapNode(const CPedSectorPosMapNode& data) = 0;

    virtual void SetSectorNavmeshPosition(const CPedSectorPosNavMeshNode& data) = 0;

    virtual void SetPedHeadings(const CPedOrientationDataNode& data) = 0;

    virtual void SetPedGameStateData(const CPedGameStateDataNode& data) = 0;

    virtual void SetPedScriptGameStateData(const CPedScriptGameStateDataNode& data) = 0;

	virtual void SetPedHealthData(const CPedHealthDataNode& data) = 0;

    virtual void SetAttachmentData(const CPedAttachDataNode& data) = 0;

    virtual void SetMotionGroup(const CPedMovementGroupDataNode& data) = 0;

    virtual void SetPedMovementData(const CPedMovementDataNode& data) = 0;

    virtual void SetPedAppearanceData(const CPackedPedProps pedProps, const CSyncedPedVarData& variationData, const u32 phoneMode, const u8 parachuteTintIndex, const u8 parachutePackTintIndex, const fwMvClipSetId& facialClipSetId, const u32 facialIdleAnimOverrideClipHash, const u32 facialIdleAnimOverrideClipDictHash, const bool isAttachingHelmet, const bool isRemovingHelmet, const bool isWearingHelmet, const u8 helmetTextureId, const u16 helmetProp, const u16 visorUpProp, const u16 visorDownProp, const bool isSwitchingVisorDown, const bool isSwitchingVisorUp, const bool isVisorSwitching, const u8 targetVisorState, bool forceApplyAppearance = false) = 0;

    virtual void SetAIData(const CPedAIDataNode& data) = 0;

	virtual void SetTaskTreeData(const CPedTaskTreeDataNode& data) = 0;

	virtual void SetTaskSpecificData(const CPedTaskSpecificDataNode& data) = 0;

    virtual void SetPedInventoryData(const CPedInventoryDataNode& data) = 0;
                                     
    virtual void SetPedComponentReservations(const CPedComponentReservationDataNode& data) = 0;

	virtual void SetTaskSequenceData(u32 numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32 repeatMode) = 0;

	virtual void PostTasksUpdate() = 0;
	virtual void UpdateTaskSpecificData() = 0;

protected:

    virtual ~IPedNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedCreationDataNode
//
// Handles the serialization of a ped's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CPedCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Ped Creation", CPedCreationDataNode, IPedNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

    u32      m_popType;               // population type
    u32      m_modelHash;             // model index
    u32      m_randomSeed;            // random seed
	u32		 m_maxHealth;			  // max health as it can be altered
    bool     m_inVehicle;             // is this ped in a vehicle?
    ObjectId m_vehicleID;             // ID of the vehicle this ped is currently in
    u32      m_seat;                  // seat the ped is sitting in in the vehicle
    bool     m_hasProp;               // does this ped carry a prop
    u32      m_propHash;              // index of the prop the ped is carrying
    bool     m_isStanding;            // is this ped standing?
	bool     m_IsRespawnObjId;        // is a valid respawn object id
	bool     m_RespawnFlaggedForRemoval; // True if the respawn ped was flagged for removal
	bool     m_hasAttDamageToPlayer;  // True if the ped damage should be attributed to a certain player.
	PhysicalPlayerIndex m_attDamageToPlayer; // ID of the Player to attribute damage to.
	bool	 m_wearingAHelmet;		  // is the ped wearing a helmet *put on via CPedHelmetComponent*
	u32      m_voiceHash;             // voice hash code
};

class CPedScriptCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Script Ped Creation", CPedScriptCreationDataNode, IPedNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.GetScriptPedCreateData(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetScriptPedCreateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

    bool  m_StayInCarWhenJacked;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedGameStateDataNode
//
// Handles the serialization of a ped's game state
///////////////////////////////////////////////////////////////////////////////////////
class CPedGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Game State", CPedGameStateDataNode, IPedNodeDataAccessor);

    PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	u8		 m_weaponComponentsTint[IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS];
	u32      m_weaponComponents[IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS]; // hashes of weapon components equipped
	u32      m_equippedGadgets[IPedNodeDataAccessor::MAX_SYNCED_GADGETS]; // hashes of gadgets equipped

	IPedNodeDataAccessor::ScriptPedPerception  m_PedPerception;          // Ped Perception values

	u32      m_arrestState;           // ped arrest state
	u32      m_deathState;            // ped death state
	u32      m_weapon;                // currently selected weapon type
	u32      m_numWeaponComponents;            // the number of weapon components on current equipped weapon
	u32      m_numGadgets;            // the number of currently equipped gadgets
	u32      m_seat;                  // seat the ped is sitting in in the vehicle
	u32		 m_nMovementModeOverrideID;
	u32	     m_LookAtFlags;           // eHeadIkFlags 

	ObjectId m_vehicleID;             // ID of the vehicle this ped is currently in
	ObjectId m_mountID;				  // ID of the mount this ped is currently in
	ObjectId m_custodianID;			  // ID of the player that is and has taken us into custody.
	ObjectId m_LookAtObjectID;        // If looking at an object, ID of object ped is looking at

	u8       m_weaponObjectTintIndex; // weapon object tint index (coloured weapons)
	u8		 m_vehicleweaponindex;
	
	u8		 m_cleardamagecount;

	IPedNodeDataAccessor::PedArrestFlags m_arrestFlags;	  // Arrest Data.

 	bool	 m_keepTasksAfterCleanup; // ped keeps his tasks given when he was a script ped
	bool     m_weaponObjectExists;    // is there a weapon object
	bool     m_weaponObjectVisible;   // is the weapon object visible
	bool	 m_weaponObjectHasAmmo;
	bool	 m_weaponObjectAttachLeft;
	bool	 m_doingWeaponSwap;		  // The ped is running a CTaskSwapWeapon
	bool     m_inVehicle;             // is this ped in a vehicle?
	bool     m_hasVehicle;            // does this ped have a vehicle?
	bool	 m_onMount;				  // is this ped on a mount?
	bool	 m_hasCustodianOrArrestFlags; // does this ped have a custodian.
	bool	 m_bvehicleweaponindex;
	bool	 m_bActionModeEnabled;
	bool	 m_bStealthModeEnabled;
	bool	 m_flashLightOn;
	bool	 m_killedByStealth;
	bool	 m_killedByTakedown;
	bool	 m_killedByKnockdown;
	bool	 m_killedByStandardMelee;
	bool	 m_bPedPerceptionModified;
	bool	 m_isLookingAtObject;     // Is looking at an object
	bool	 m_changeToAmbientPopTypeOnMigration;
	bool	 m_isUpright;
	bool	 m_isDuckingInVehicle;
	bool	 m_isUsingLowriderLeanAnims;
	bool	 m_isUsingAlternateLowriderLeanAnims;
	bool     m_HasValidWeaponClipset;             // do we have a valid weapon clip set? Used as part of a hack fix for when pointing while holding a weapon
	bool	 m_createdByConcealedPlayer;
	bool	 m_dontBehaveLikeLaw;
	bool	 m_hitByTranqWeapon;
	bool	 m_permanentlyDisablePotentialToBeWalkedIntoResponse;
	bool	 m_dontActivateRagdollFromAnyPedImpact;
	bool	 m_hasDroppedWeapon;
	bool	 m_canBeIncapacitated;
	bool	 m_bDisableStartEngine;
	bool	 m_disableBlindFiringInShotReactions;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedSectorPosMapNode
//
// Handles the serialization of a ped's sector position on the map. This involve syncing
// the Z position at full accuracy
///////////////////////////////////////////////////////////////////////////////////////
class CPedSectorPosMapNode : public CSectorPositionDataNode
{
protected:

	NODE_COMMON_IMPL("Ped Map Sector Pos", CPedSectorPosMapNode, IPedNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &nodeData)
    {
        nodeData.GetSectorPosMapNode(*this);
		m_IsStandingOnNetworkObject = m_StandingOnNetworkObjectID != NETWORK_INVALID_OBJECT_ID;
   }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &nodeData)
    {
        nodeData.SetSectorPosMapNode(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

public:

    bool     m_IsRagdolling;                // is this ped ragdolling on top of another network object?
    bool     m_IsStandingOnNetworkObject;   // is this ped currently standing on another network object?
    ObjectId m_StandingOnNetworkObjectID;   // ID of the object this ped is standing on
    Vector3  m_LocalOffset;                 // Offset from the center of the object
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedSectorPosNavMeshNode
//
// Handles the serialization of a ped's sector position when it is on the navmesh.
// This involves syncing the Z position at a lower level of accuracy. The Z position
// can be looked up from the navmesh on the receiving side. We still need to send a
// Z position so we can distinguish which navmesh a ped is standing on in the cases where
// there are multiple height levels for a given XY position (i.e. a bridge over the top of a road).
///////////////////////////////////////////////////////////////////////////////////////
class CPedSectorPosNavMeshNode : public CSyncDataNodeFrequent
{
private:

    static const unsigned SIZEOF_NAVMESH_SECTORPOS_Z = 6;

public:

    static const float GetQuantisationError() { return WORLD_HEIGHTOFSECTOR_NETWORK / static_cast<float>(1<<SIZEOF_NAVMESH_SECTORPOS_Z); } 

protected:

	NODE_COMMON_IMPL("Ped Navmesh Sector Pos", CPedSectorPosNavMeshNode, IPedNodeDataAccessor);

	PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &nodeData)
    {
        nodeData.GetSectorNavmeshPosition(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &nodeData)
    {
        nodeData.SetSectorNavmeshPosition(*this);
    }

	void Serialise(CSyncDataBase& serialiser);

public:

#if __DEV
	static RecorderId ms_bandwidthRecorderId;
#endif

    float m_sectorPosX; // X position of this object within the current sector
    float m_sectorPosY; // Y position of this object within the current sector
    float m_sectorPosZ; // Z position of this object within the current sector
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedComponentReservationDataNode
//
// Handles the serialization of a ped's component reservation
///////////////////////////////////////////////////////////////////////////////////////
class CPedComponentReservationDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Ped Component Reservation", CPedComponentReservationDataNode, IPedNodeDataAccessor);

private:

	void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
	{
		pedNodeData.GetPedComponentReservations(*this);
	}

	void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
	{
		pedNodeData.SetPedComponentReservations(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	static const unsigned int MAX_PED_COMPONENTS = CComponentReservation::MAX_NUM_COMPONENTS;

	u32		 m_numPedComponents;
	ObjectId m_componentReservations[MAX_PED_COMPONENTS];
};


// One set of vehicle entry config for a ped and vehicle
class CSyncedPedVehicleEntryScriptConfigData
{
public:

	CSyncedPedVehicleEntryScriptConfigData() { Reset(); }
	CSyncedPedVehicleEntryScriptConfigData(u8 uFlags, CVehicle* pVeh, s32 iSeatIndex) : m_Flags(uFlags), m_SyncedVehicle(pVeh), m_SeatIndex(iSeatIndex){}

	enum eFlags
	{
		ForceUseFrontSeats = BIT0,
		ForceUseRearSeats = BIT1,
		ForceForAnyVehicle = BIT2,
		LastFlag = ForceForAnyVehicle
	};

	void Reset();
	void Serialise(CSyncDataBase& serialiser);

	bool operator==(const CSyncedPedVehicleEntryScriptConfigData& src) const
	{
		return ((src.m_SyncedVehicle.GetVehicle()==m_SyncedVehicle.GetVehicle() ||
				src.m_SyncedVehicle.GetVehicleID()==m_SyncedVehicle.GetVehicleID()) &&
				src.m_Flags==m_Flags && src.m_SeatIndex==m_SeatIndex);
	}

	bool operator!=(const CSyncedPedVehicleEntryScriptConfigData& src) const
	{
		return (!(*this==src));
	}

#if __BANK
	bool HasConflictingFlagsSet() const { return IsFlagSet(ForceUseFrontSeats) && IsFlagSet(ForceUseRearSeats); }
#endif // __BANK

	bool IsFlagSet(u8 iFlag) const { return (m_Flags & iFlag) != 0; }
	bool HasAForcingFlagSet() const { return IsFlagSet(ForceUseRearSeats) || IsFlagSet(ForceUseFrontSeats); }
	bool HasAForcingSeatIndex() const { return m_SeatIndex >= 0; }
	bool HasNonDefaultState() const { return m_Flags != 0 || HasAForcingSeatIndex(); }

	CSyncedVehicle m_SyncedVehicle;
	u8 m_Flags;
	s32 m_SeatIndex;
};

// Used to store and sync data configured by script to change aspects of the vehicle entry sequence for multiple vehicles e.g seating preferences
// new instances are allocated on the heap via the default allocator and a pointer is stored on the ped if needed, not expecting many of these
class CSyncedPedVehicleEntryScriptConfig
{
public:

	static const s32 MAX_VEHICLE_ENTRY_DATAS = 5;

#if __BANK
	bool ValidateData() const;
	void SpewDataToTTY() const;
#endif // __BANK

	void ClearForcedSeatUsage();
	void Reset();
	void Serialise(CSyncDataBase& serialiser);

	CSyncedPedVehicleEntryScriptConfig& operator=(const CSyncedPedVehicleEntryScriptConfig& src) 
	{
		for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
		{
			m_Data[i].m_Flags = src.m_Data[i].m_Flags;
			m_Data[i].m_SyncedVehicle = src.m_Data[i].m_SyncedVehicle;
			m_Data[i].m_SeatIndex = src.m_Data[i].m_SeatIndex;
		}
		return *this;
	}

	const CSyncedPedVehicleEntryScriptConfigData& GetData(s32 iSlot) const { return m_Data[iSlot]; }
	s32 GetFlags(s32 iSlot) const { return static_cast<s32>(m_Data[iSlot].m_Flags); }
	void SetFlags(s32 iSlot, u8 iFlags) { m_Data[iSlot].m_Flags = iFlags; }
	bool IsFlagSet(s32 iSlot, u8 iFlag) const { return m_Data[iSlot].IsFlagSet(iFlag); }
	const CVehicle* GetVehicle(s32 iSlot) const { return m_Data[iSlot].m_SyncedVehicle.GetVehicle(); }
	ObjectId GetVehicleId(s32 iSlot) const { return m_Data[iSlot].m_SyncedVehicle.GetVehicleID(); }
	void SetVehicle(s32 iSlot, CVehicle* pVeh) { m_Data[iSlot].m_SyncedVehicle.SetVehicle(pVeh); }
	void SetVehicleId(s32 iSlot, const ObjectId& vehicleId) { m_Data[iSlot].m_SyncedVehicle.SetVehicleID(vehicleId); }
	void SetSeat(s32 iSlot, s32 iSeatIndex) { m_Data[iSlot].m_SeatIndex = iSeatIndex; }
	const s32 GetSeatIndex(s32 iSlot) const { return m_Data[iSlot].m_SeatIndex; }
	s32 GetForcingSeatIndexForThisVehicle(const CVehicle* pVeh) const;
	
	bool IsVehicleValidForRejectionChecks(s32 iSlot, const CVehicle& rVehicle) const { return IsFlagSet(iSlot, CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle) || &rVehicle == GetVehicle(iSlot); }
	bool ShouldRejectBecauseForcedToUseFrontSeats(s32 iSlot, bool bIsFrontSeat) const { return IsFlagSet(iSlot, CSyncedPedVehicleEntryScriptConfigData::ForceUseFrontSeats) && !bIsFrontSeat; }
	bool ShouldRejectBecauseForcedToUseRearSeats(s32 iSlot, bool bIsFrontSeat) const { return IsFlagSet(iSlot,CSyncedPedVehicleEntryScriptConfigData::ForceUseRearSeats) && bIsFrontSeat; }
	bool ShouldRejectBecauseForcedToUseSeatIndex(const CVehicle& rVeh, s32 iSeat) const;
	bool HasNonDefaultState() const;
	bool HasDefaultState() const { return !HasNonDefaultState(); }
	bool HasAForcingFlagSetForThisVehicle(const CVehicle* pVeh) const;
	bool HasARearForcingFlagSetForThisVehicle(const CVehicle* pVeh) const;

private:

	atRangeArray<CSyncedPedVehicleEntryScriptConfigData, MAX_VEHICLE_ENTRY_DATAS> m_Data;

};

///////////////////////////////////////////////////////////////////////////////////////
// CPedScriptGameStateDataNode
//
// Handles the serialization of a ped's script game state
///////////////////////////////////////////////////////////////////////////////////////
class CPedScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Script Game State", CPedScriptGameStateDataNode, IPedNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

    virtual bool ResetScriptStateInternal();

public:

	static bool IsScriptPedConfigFlagSynchronized(int Flag);

	CCombatData::BehaviourFlagsBitSet		   m_combatBehaviorFlags;
	u32										   m_targetLossResponse;
	u32										   m_fleeBehaviorFlags;
	u32										   m_NavCapabilityFlags;
	u32										   m_popType;                // population type
	u32										   m_pedType;				 // ped type
    float                                      m_shootRate;              // ped shoot rate
    u32                                        m_pedCash;                // amount of cash the ped is carrying
	s32										   m_minOnGroundTimeForStun; // minimum on ground time for ped when hit by stun gun
	u32										   m_ragdollBlockingFlags;   // script flags for blocking ragdoll
	u32										   m_ammoToDrop;             // a script specified ammo amount that the ped will drop when he dies
	u32										   m_inVehicleContextHash;   // sync the in vehicle context hash
	Vector3                                    m_DefensiveAreaCentre;    // centre point of the defensive area
	float                                      m_DefensiveAreaRadius;    // defensive area radius
	Vector3                                    m_AngledDefensiveAreaV1;
	Vector3                                    m_AngledDefensiveAreaV2;
	float                                      m_AngledDefensiveAreaWidth;
	float									   m_fMaxInformFriendDistance; 
	float                                      m_fTimeBetweenAggressiveMovesDuringVehicleChase;
	float									   m_fMaxVehicleTurretFiringRange;
	float                                      m_fAccuracy;
	float									   m_fMaxShootingDistance;
	float									   m_fBurstDurationInCover;
	float									   m_fTimeBetweenBurstsInCover;
	float									   m_fTimeBetweenPeeks;
	float									   m_fBlindFireChance;
	float									   m_fStrafeWhenMovingChance;
	float									   m_fWeaponDamageModifier;

	float									   m_fHomingRocketBreakLockAngle;
	float									   m_fHomingRocketBreakLockAngleClose;
	float									   m_fHomingRocketBreakLockCloseDistance;

	s32										   m_vehicleweaponindex;	 // Vehicle Weapon Index: Missiles, Gatling, etc...
	s32										   m_SeatIndexToUseInAGroup;
	TeamFlags						           m_isTargettableByTeam;    // flags indicating whether the ped is targettable by each team
	IPedNodeDataAccessor::ScriptPedFlags       m_PedFlags;               // ped flags
	u8										   m_combatMovement;
    bool                                       m_pedHasCash;             // does the ped carry any cash?
    bool                                       m_HasDefensiveArea;       // does the ped have a defensive area
	u8                                         m_DefensiveAreaType;      // Type of defensive area
    bool                                       m_UseCentreAsGotoPos;     // use the centre point of a the defensive area as the place
	bool									   m_hasPedType;			 // used to determine if there is a ped type to send
	bool									   m_isPainAudioDisabled;	 // Is the pain audio disabled
	bool									   m_isAmbientSpeechDisabled;
	bool									   m_HasInVehicleContextHash;
	u8										   m_uMaxNumFriendsToInform;
	CSyncedPedVehicleEntryScriptConfig		   m_PedVehicleEntryScriptConfig;
	u32										   m_FiringPatternHash;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedOrientationDataNode
//
// Handles the serialization of a ped's orientation
///////////////////////////////////////////////////////////////////////////////////////
class CPedOrientationDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Ped Orientation", CPedOrientationDataNode, IPedNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		if (GetIsInCutscene(pObj))
		{
			return ~0u;
		}

        return GetIsAttachedForPlayers(pObj);
	}

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.GetPedHeadings(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetPedHeadings(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

    float m_currentHeading; // ped current heading
    float m_desiredHeading; // ped desired heading
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedHealthDataNode
//
// Handles the serialization of a ped's health
///////////////////////////////////////////////////////////////////////////////////////
class CPedHealthDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Health", CPedHealthDataNode, IPedNodeDataAccessor);

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.GetPedHealthData(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetPedHealthData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; }
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	u32		 m_scriptMaxHealth;	   // max health set by script
	u32      m_health;             // health
	u32      m_armour;             // ped armour
	u32		 m_endurance;		   // ped endurance
	u32		 m_scriptMaxEndurance;
	u32      m_weaponDamageHash;   // weapon damage Hash 
	u32	     m_hurtEndTime;		   // hurt time (used by GetUp, Writhe + Aiming + Gun to pick injured animations)
	s32		 m_weaponDamageComponent;
	ObjectId m_weaponDamageEntity; // weapon damage entity (only for script objects)
	bool	 m_hasMaxHealth;       // health is max
	bool	 m_hasDefaultArmour;   // armour is default
	bool	 m_hasMaxEndurance;	   // endurance is max
	bool     m_killedWithHeadShot; // true if the ped died from a headshot
	bool     m_killedWithMeleeDamage; // true if the ped died from a Melee damage (weapon whips)
	bool 	 m_hurtStarted;		   // CPEDCONFIGFLAG_HurtStarted
	bool	 m_maxHealthSetByScript; // Script set a max health for this ped
	bool	 m_maxEnduranceSetByScript; // Script set a max endurance for this ped
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedAttachDataNode
//
// Handles the serialization of a ped's attachment
///////////////////////////////////////////////////////////////////////////////////////
class CPedAttachDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Attach", CPedAttachDataNode, IPedNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    Vector3    m_attachOffset;       // offset from attachment position
    Quaternion m_attachQuat;         // attachment quaternion
	ObjectId   m_attachObjectID;     // ID of Object ped is attached to
    s16        m_attachBone;         // bone ped is attached to
    u32        m_attachFlags;        // attachment flags
    float      m_attachHeading;      // attachment heading
    float      m_attachHeadingLimit; // attachment heading limit
	bool       m_attached;           // is the ped attached?
	bool	   m_attachedToGround;   // whether the ped is attached to ground or not
};


//For syncing ped tennis motion hopefully just swings from CommandPlayTennisSwingAnim
class CSyncedTennisMotionData
{
public:
	CSyncedTennisMotionData() : m_bAllowOverrideCloneUpdate(false), m_bControlOutOfDeadZone(false)
	{
		Reset();
	}
	CSyncedTennisMotionData(const CSyncedTennisMotionData& src) { *this = src; }

	void Reset();

	void Serialise(CSyncDataBase& serialiser);

	static const unsigned int SIZEOF_ANIM = 32;
	static const unsigned int SIZEOF_DICT = 32;
	static const unsigned int SIZEOF_RATE	= 8;
	static const unsigned int SIZEOF_PHASE	= 8;
	static const unsigned int SIZEOF_DIVEHDIR	= 8;
	static const unsigned int SIZEOF_DIVEVDIR	= 8;

	bool operator==(const CSyncedTennisMotionData& src) const
	{
		return (src.m_DiveMode==m_DiveMode &&
				src.m_DiveDirection==m_DiveDirection &&
				src.m_ClipHash==m_ClipHash &&
				src.m_DictHash==m_DictHash &&
				src.m_bAllowOverrideCloneUpdate==m_bAllowOverrideCloneUpdate && 
				src.m_bControlOutOfDeadZone==m_bControlOutOfDeadZone ); 
	}

	bool operator!=(const CSyncedTennisMotionData& src) const
	{
		return (!(*this==src));
	}

	CSyncedTennisMotionData& operator=(const CSyncedTennisMotionData& src) 
	{
		m_Active			= src.m_Active;
		m_DiveMode			= src.m_DiveMode;
		m_DiveDirection		= src.m_DiveDirection;
		m_ClipHash			= src.m_ClipHash;
		m_DictHash			= src.m_DictHash;
		m_fStartPhase		= src.m_fStartPhase;
		m_fPlayRate			= src.m_fPlayRate;
		m_fDiveHorizontal	= src.m_fDiveHorizontal;
		m_fDiveVertical		= src.m_fDiveVertical;
		m_bSlowBlend		= src.m_bSlowBlend;
		m_bAllowOverrideCloneUpdate		= src.m_bAllowOverrideCloneUpdate;
		m_bControlOutOfDeadZone	= src.m_bControlOutOfDeadZone;

		return *this;
	}

	bool	m_Active;
	bool	m_DiveMode;
	bool	m_DiveDirection;
	u32		m_ClipHash;
	u32		m_DictHash;
	float	m_fStartPhase;
	float	m_fPlayRate;
	float	m_fDiveHorizontal;
	float	m_fDiveVertical;
	bool	m_bSlowBlend;
	bool    m_bAllowOverrideCloneUpdate;
	bool	m_bControlOutOfDeadZone;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedMovementGroupDataNode
//
// Handles the serialization of a ped's movement group
///////////////////////////////////////////////////////////////////////////////////////
class CPedMovementGroupDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Movement Group", CPedMovementGroupDataNode, IPedNodeDataAccessor);

    virtual PlayerFlags StopSend(const netSyncTreeTargetObject*, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
         pedNodeData.GetMotionGroup(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetMotionGroup(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:	

	float m_motionInVehiclePitch;
	u16 m_moveBlendType;						// the type of move blender the ped is using (compressed - 0 == ped, 1 == ped low lod, 2 == horse)
    u32  m_moveBlendState;					// the state of the move blender the ped is using
	fwMvClipSetId m_motionSetId;			// current motion set this ped is using (e.g. crouching, swimming, standard)
	fwMvClipSetId m_overriddenWeaponSetId;	// current weapon set this ped is using (e.g. being cuffed)
    fwMvClipSetId m_overriddenStrafeSetId;	// current strafe set this ped is using (e.g. move ballistic)
    bool m_isCrouching;                     // indicates whether the ped is currently crouching
    bool m_isStealthy;                      // indicates whether the ped is currently being stealthy
    bool m_isStrafing;						// indicates whether the ped is currently strafing
    bool m_isRagdolling;					// indicates whether the ped is currently ragdolling
	bool m_isRagdollConstraintAnkleActive;	// indicates if the ped is currently ankle cuffed
	bool m_isRagdollConstraintWristActive;	// indicates if the ped is currently handcuffed

	CSyncedTennisMotionData m_TennisMotionData; //For syncing swings from CommandPlayTennisSwingAnim

private:

	enum SerialisedMoveBlenderTypes
	{
		BlenderType_Ped			= 0,
		BlenderType_PedLowLod	= 1,
		BlenderType_Horse		= 2,
		BlenderType_OnFootHorse = 3,
		BlenderType_OnFootBird  = 4,
		BlenderType_Invalid		= 5
	};

	u16 PackMotionTaskType(u16 const type) const;
	void UnpackMotionTaskType(u16 const type);
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedMovementDataNode
//
// Handles the serialization of a ped's movement
///////////////////////////////////////////////////////////////////////////////////////
class CPedMovementDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Ped Movement", CPedMovementDataNode, IPedNodeDataAccessor);

    virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		if (GetIsInCutscene(pObj))
		{
			return ~0u;
		}

		return GetIsAttachedForPlayers(pObj);
	}

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    bool   m_HasDesiredMoveBlendRatioX; // indicates whether the move blend ratio for the ped in the X axis is non-zero
    bool   m_HasDesiredMoveBlendRatioY; // indicates whether the move blend ratio for the ped in the Y axis is non-zero
    bool   m_HasStopped;                // indicates the ped has stopped moving (velocity is zero)
    float  m_DesiredMoveBlendRatioX;    // desired move blend ratio in the X axis
    float  m_DesiredMoveBlendRatioY;    // desired move blend ratio in the Y axis
    float  m_DesiredPitch;              // desired pitch
	float  m_MaxMoveBlendRatio;			// script set max move blend ratio
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedAppearanceDataNode
//
// Handles the serialization of a ped's appearance
///////////////////////////////////////////////////////////////////////////////////////
class CPedAppearanceDataNode : public CSyncDataNodeInfrequent
{
public:

	CPedAppearanceDataNode();

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

    NODE_COMMON_IMPL("Ped Appearance", CPedAppearanceDataNode, IPedNodeDataAccessor);

	bool IsAlwaysSentWithCreate() const { return true; }

protected:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

    CPackedPedProps		m_pedProps;									// ped props flags
	CSyncedPedVarData	m_variationData;
	u32					m_PhoneMode;								// for secondary task phone 
	u8					m_parachuteTintIndex;						// what colour the parachute will appear on deployment...
	u8					m_parachutePackTintIndex;					// what colour the parachute pack will appear on deployment...

	fwMvClipSetId		m_facialClipSetId;							// the facial clipset used by the ped
	u32					m_facialIdleAnimOverrideClipNameHash;		// the facial clip used by the ped
	u32					m_facialIdleAnimOverrideClipDictNameHash;	// the dictionary used by the ped

	bool				m_isAttachingHelmet;						// are we attaching a helmet?
	bool				m_isRemovingHelmet;							// are we removing a helmet?
	bool				m_isWearingHelmet;							// are we wearing a helmet?
	u8					m_helmetTextureId;							// what texture are we going to use for the helmet?
	u16					m_helmetProp;								// what helmet type are we using?
	u16					m_visorUpProp;								// what helmet type are we using?
	u16					m_visorDownProp;							// what helmet type are we using?
	bool				m_visorIsUp;
	bool				m_supportsVisor;
	bool				m_isVisorSwitching;
	u8					m_targetVisorState;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedAIDataNode
//
// Handles the serialization of a ped's AI
///////////////////////////////////////////////////////////////////////////////////////
class CPedAIDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped AI", CPedAIDataNode, IPedNodeDataAccessor);

	bool IsAlwaysSentWithCreate() const { return true; }

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.GetAIData(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetAIData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

 	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    u32 m_relationshipGroup;        // ped relationship group
    u32 m_decisionMakerType;        // standard decision maker type
};


///////////////////////////////////////////////////////////////////////////////////////
// CPedTaskDataNode
//
// Handles the serialization of a ped's task tree
///////////////////////////////////////////////////////////////////////////////////////
class CPedTaskTreeDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Ped Task Tree", CPedTaskTreeDataNode, IPedNodeDataAccessor);

    virtual bool IsCreateNode() const { return true; }

	bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const;

    PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned int SIZEOF_SCRIPTTASKSTAGE;

	IPedNodeDataAccessor::TaskSlotData	m_taskTreeData[IPedNodeDataAccessor::NUM_TASK_SLOTS];
	u32									m_taskSlotsUsed; // which slots in the task tree data are used
    s32									m_scriptCommand; // current task script command
    u32									m_taskStage;     // current stage of task script command
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedTaskSpecificDataNode
//
// Handles the serialization of a specific task's data
///////////////////////////////////////////////////////////////////////////////////////
class CPedTaskSpecificDataNode : public CSyncDataNodeFrequent
{
public:

	CPedTaskSpecificDataNode() :
	m_taskIndex(-1)
	{
	}

	void SetTaskIndex(const u32 taskIndex) { m_taskIndex = taskIndex; }
	static void SetTaskDataLogFunction(IPedNodeDataAccessor::fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

protected:

	NODE_COMMON_IMPL("Ped Task", CPedTaskSpecificDataNode, IPedNodeDataAccessor);

	virtual bool IsCreateNode() const { return true; }

    PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const;

private:

	void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData);

	void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData);

	void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	// static task data logging function
	static IPedNodeDataAccessor::fnTaskDataLogger m_taskDataLogFunction;

	int  m_taskIndex;     // index of the task this node is representing
	IPedNodeDataAccessor::CTaskData	m_taskData;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedInventoryDataNode
//
// Handles the serialization of a ped's inventory
///////////////////////////////////////////////////////////////////////////////////////
class CPedInventoryDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Ped Inventory", CPedInventoryDataNode, IPedNodeDataAccessor);

	virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.GetPedInventoryData(*this);
    }

    void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
    {
        pedNodeData.SetPedInventoryData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const ;
	virtual void SetDefaultState();

public:

	u32  m_itemSlotComponent[MAX_WEAPONS][IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS];
    u32  m_itemSlots[MAX_WEAPONS];
    u32  m_numItems;
    u32  m_ammoSlots[MAX_AMMOS];
    u32  m_ammoQuantity[MAX_AMMOS];
    u32  m_numAmmos;
	u8   m_itemSlotTint[MAX_WEAPONS];
	u8   m_itemSlotNumComponents[MAX_WEAPONS];
	bool m_itemSlotComponentActive[MAX_WEAPONS][IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS];
    bool m_ammoInfinite[MAX_AMMOS];
    bool m_allAmmoInfinite;
};

///////////////////////////////////////////////////////////////////////////////////////
// CPedTaskSequenceDataNode
//
// Handles the serialization of the tasks in a sequence the ped is running
///////////////////////////////////////////////////////////////////////////////////////
class CPedTaskSequenceDataNode : public CSyncDataNodeFrequent
{
public:
	
	CPedTaskSequenceDataNode() : m_numTasks(0) {}

	static void SetTaskDataLogFunction(IPedNodeDataAccessor::fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

	virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

protected:

	NODE_COMMON_IMPL("Ped Sequence Tasks", CPedTaskSequenceDataNode, IPedNodeDataAccessor);

	bool IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const;

	virtual bool GetIsSyncUpdateNode() const { return false; }

private:

	void ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
	{
		pedNodeData.GetTaskSequenceData(m_hasSequence, m_sequenceResourceId, m_numTasks, m_taskData, m_repeatMode);
	}

	void ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
	{
		if (m_hasSequence)
		{
			pedNodeData.SetTaskSequenceData(m_numTasks, m_taskData, m_repeatMode);
		}
	}

	void Serialise(CSyncDataBase& serialiser);

public:

	// static task data logging function
	static IPedNodeDataAccessor::fnTaskDataLogger m_taskDataLogFunction;

	bool							m_hasSequence;
	u32								m_sequenceResourceId;
	u32								m_numTasks;    
	IPedNodeDataAccessor::CTaskData	m_taskData[IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS];
	u32								m_repeatMode;
};

#endif  // PED_SYNC_NODES_H
