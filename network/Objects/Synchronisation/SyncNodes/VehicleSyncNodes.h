//
// name:        VehicleSyncNodes.h
// description: Network sync nodes used by CNetObjVehicles
// written by:    John Gurney
//

#ifndef VEHICLE_SYNC_NODES_H
#define VEHICLE_SYNC_NODES_H

#include "network/objects/synchronisation/SyncNodes/PhysicalSyncNodes.h"

#include "network/General/NetworkUtil.h"
#include "Script/Handlers/GameScriptIds.h"
#include "vehicles/vehicle.h"
#include "Vehicles/VehicleGadgets.h"
#include "vfx/decals/DecalCallbacks.h"

class CVehicleCreationDataNode;
class CVehicleAngVelocityDataNode;
class CVehicleGameStateDataNode;
class CVehicleScriptGameStateDataNode;
class CVehicleHealthDataNode;
class CVehicleSteeringDataNode;
class CVehicleControlDataNode;
class CVehicleAppearanceDataNode;
class CVehicleDamageStatusDataNode;
class CVehicleTaskDataNode;
class CVehicleComponentReservationDataNode;
class CVehicleProximityMigrationDataNode;
class CVehicleGadgetDataNode;

static const unsigned int SIZEOF_LIGHT_DAMAGE			= VEHICLE_LIGHT_COUNT;
static const unsigned int SIZEOF_WINDOW_DAMAGE			= VEH_LAST_WINDOW - VEH_WINDSCREEN + 1;
static const unsigned int SIZEOF_SIREN_DAMAGE			= VEHICLE_SIREN_COUNT;
static const unsigned int SIZEOF_LICENCE_PLATE			= 8;
static const unsigned int SIZEOF_KIT_MODS_U8			= 8;
static const unsigned int SIZEOF_KIT_MODS_U32			= 32;
static const unsigned int NUM_KIT_MOD_SYNC_VARIABLES	= 13;
static const unsigned int SIZEOF_KIT_MODS				= SIZEOF_KIT_MODS_U32 * NUM_KIT_MOD_SYNC_VARIABLES;
static const unsigned int NUM_NETWORK_DAMAGE_DIRECTIONS = 6;
static const unsigned int SIZEOF_NUM_DOORS				= 7;
static const unsigned int SIZEOF_ALTERED_COUNT			= 4;
static const unsigned int MAX_ALTERED_VALUE				= (1<<SIZEOF_ALTERED_COUNT)-1;
static const u8 XENON_LIGHT_DEFAULT_COLOR = 0xff;

///////////////////////////////////////////////////////////////////////////////////////
// IVehicleNodeDataAccessor
//
// Interface class for setting/getting data to serialise for the vehicle nodes.
// Any class that wishes to send/receive data contained in the vehicle nodes must
// implement this interface.
///////////////////////////////////////////////////////////////////////////////////////
class IVehicleNodeDataAccessor : public netINodeDataAccessor
{
public:

    DATA_ACCESSOR_ID_DECL(IVehicleNodeDataAccessor);

    struct ScriptVehicleFlags
    {
        bool takeLessDamage;
        bool vehicleCanBeTargetted;
        bool partOfConvoy;
        bool isDrowning;
        bool canBeVisiblyDamaged;
        bool freebies;
		bool lockedForNonScriptPlayers;
		bool respectLocksWhenHasDriver;
		bool lockDoorsOnCleanup;
		bool shouldFixIfNoCollision;
		bool automaticallyAttaches;
		bool scansWithNonPlayerDriver;
        bool disableExplodeOnContact;
        bool canEngineDegrade;
        bool canPlayerAircraftEngineDegrade;
        bool nonAmbientVehicleMayBeUsedByGoToPointAnyMeans;
        bool canBeUsedByFleeingPeds;
        bool allowNoPassengersLockOn;
		bool allowHomingMissleLockOn;
        bool disablePetrolTankDamage;
        bool isStolen;
        bool explodesOnHighExplosionDamage;
		bool rearDoorsHaveBeenBlownOffByStickybomb;
		bool isInCarModShop;
		bool specialEnterExit;
		bool onlyStartVehicleEngineOnThrottle;
		bool dontOpenRearDoorsOnExplosion;
		bool disableTowing;
		bool dontProcessVehicleGlass;
		bool useDesiredZCruiseSpeedForLanding;
		bool dontResetTurretHeadingInScriptedCamera;
		bool disableWantedConesResponse;
    };

	static const unsigned int MAX_VEHICLE_GADGET_DATA_SIZE = 94;

	typedef struct 
	{
		u32 Type;
		u8 Data[MAX_VEHICLE_GADGET_DATA_SIZE];
	} GadgetData;

	// getter functions
    virtual void GetVehicleCreateData(CVehicleCreationDataNode& data) = 0;

    virtual void GetVehicleAngularVelocity(CVehicleAngVelocityDataNode & data) = 0;

    virtual void GetVehicleGameState(CVehicleGameStateDataNode& data) = 0;

    virtual void GetVehicleScriptGameState(CVehicleScriptGameStateDataNode& data) = 0;

    virtual void GetVehicleHealth(CVehicleHealthDataNode& data) = 0;

    virtual void GetSteeringAngle(CVehicleSteeringDataNode & data) = 0;
    virtual void GetVehicleControlData(CVehicleControlDataNode& data) = 0;

	virtual void GetVehicleAppearanceData(CVehicleAppearanceDataNode& data) = 0;

	virtual void GetVehicleDamageStatusData(CVehicleDamageStatusDataNode& data) = 0;

	virtual void GetVehicleAITask(CVehicleTaskDataNode& data) = 0;

	virtual void GetComponentReservations(CVehicleComponentReservationDataNode& data) = 0;

    virtual void GetVehicleMigrationData(CVehicleProximityMigrationDataNode& data) = 0;

	virtual void GetVehicleGadgetData(CVehicleGadgetDataNode& data) = 0;

	 // setter functions
    virtual void SetVehicleCreateData(const CVehicleCreationDataNode& data) = 0;

    virtual void SetVehicleAngularVelocity(CVehicleAngVelocityDataNode& data) = 0;

    virtual void SetVehicleGameState(const CVehicleGameStateDataNode& data) = 0;

    virtual void SetVehicleScriptGameState(const CVehicleScriptGameStateDataNode& data) = 0;

    virtual void SetVehicleHealth(const CVehicleHealthDataNode& data) = 0;

    virtual void SetSteeringAngle(const CVehicleSteeringDataNode& data) = 0;
    virtual void SetVehicleControlData(const CVehicleControlDataNode& data) = 0;

    virtual void SetVehicleAppearanceData(const CVehicleAppearanceDataNode& data) = 0;

	virtual void SetVehicleDamageStatusData(const CVehicleDamageStatusDataNode& data) = 0;

	virtual void SetVehicleAITask(const CVehicleTaskDataNode& data) = 0;

	virtual void SetComponentReservations(const CVehicleComponentReservationDataNode& data) = 0;

    virtual void SetVehicleMigrationData(const CVehicleProximityMigrationDataNode& data) = 0;

	virtual void SetVehicleGadgetData(const CVehicleGadgetDataNode& data) = 0;

protected:

    virtual ~IVehicleNodeDataAccessor() {}
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleCreationDataNode
//
// Handles the serialization of a vehicle's creation data
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleCreationDataNode : public CProjectBaseSyncDataNode
{
protected:

    NODE_COMMON_IMPL("Vehicle Creation", CVehicleCreationDataNode, IVehicleNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

    virtual bool CanApplyData(netSyncTreeTargetObject* pObj) const;

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetVehicleCreateData(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetVehicleCreateData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:
	
    u32  m_popType;                      // population type
	u32  m_randomSeed;		             // random seed
    u32  m_modelHash;                    // vehicle model index
    u32  m_status;                       // vehicle status flags
	u32	 m_maxHealth;					 // max health as it can be altered
    u32	 m_lastDriverTime;               // the last time this vehicle had a driver
    bool m_takeOutOfParkedCarBudget;     // should this vehicle be taken out of the parked car population budget?
    bool m_needsToBeHotwired;            // does the car need to be hotwired?
    bool m_tyresDontBurst;               // are the tyres impervious to damage?
	bool m_usesVerticalFlightMode;		 // true when it's a plane and uses special flight mode
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleAngVelocityDataNode
//
// Handles the serialization of a vehicle's angular velocity.
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleAngVelocityDataNode : public CPhysicalAngVelocityDataNode
{
protected:

    NODE_COMMON_IMPL("Vehicle AngVelocity", CVehicleAngVelocityDataNode, IVehicleNodeDataAccessor);

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.GetVehicleAngularVelocity(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &physicalNodeData)
    {
        physicalNodeData.SetVehicleAngularVelocity(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

    virtual bool CanHaveDefaultState() const { return true; } 
    virtual bool HasDefaultState() const;
    virtual void SetDefaultState();

public:
    bool m_IsSuperDummyAngVel;  // indicates this angular velocity was retrieved from a superdummy vehicle (should not be applied on remote machines)
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleGameStateDataNode
//
// Handles the serialization of a vehicle's game state
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Game State", CVehicleGameStateDataNode, IVehicleNodeDataAccessor);

	virtual bool IsCreateNode() const { return true; }

	bool IsAlwaysSentWithCreate() const { return true; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetVehicleGameState(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetVehicleGameState(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	static const int							 MAX_EXTRA_BROKEN_FLAGS = 7;
	float  m_HeadlightMultiplier;
	float  m_customPathNodeStreamingRadius;
	float  m_downforceModifierFront;
	float  m_downforceModifierRear;
    u32    m_radioStation;				                 // current radio station
	u32    m_doorLockState;				                 // door lock state
	u32    m_doorsBroken;				                 // doors broken state
	u32    m_doorsNotAllowedToBeBrokenOff;				 // if the doors are not allowed to be broken off bitmask
	u32	   m_windowsDown;				                 // windows down state
	u32	   m_timedExplosionTime;		                 // network time at which the timed explosion is to occur
	u32	   m_junctionArrivalTime;						 // Time that the vehicle arrived at its current junction
	u32    m_OverridenVehHornHash;						 // Hash of a horn sound used for overriden vehicle horn
	u32    m_fullThrottleEndTime;						 // Network time that Full Throttle will end
	ObjectId m_timedExplosionCulprit;	                 // entity responsible for the timed explosion
	ObjectId m_exclusiveDriverPedID[CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS]; // exclusive driver (only peds that can drive this vehicle).
	ObjectId m_lastDriverPedID;
	u8	   m_ExtraBrokenFlags;
	u8	   m_overridelights;
	u8	   m_doorIndividualLockedStateFilter;
	u8	   m_doorIndividualLockedState[SIZEOF_NUM_DOORS];
	u8	   m_junctionCommand;							 // Traffic flow command (stop, go)
	u8	   m_doorsOpen;					                 // doors open state
	u8	   m_doorsOpenRatio[SIZEOF_NUM_DOORS];		     // doors open ratio
	u8	   m_xenonLightColor;
	PlayerFlags m_PlayerLocks;
    bool   m_engineOn;					                 // is this vehicle's engine currently on?
	bool   m_engineStarting;			                 // is the engine starting
	bool   m_engineSkipEngineStartup;					 // if the audio for the engine startup should be skipped
	bool   m_handBrakeOn;								 //indicates whether the hand brake is on
    bool   m_scriptSetHandbrakeOn;						 // indicates whether script has specified the handbrake is on this vehicle (included in vehicle game state to ensure goes with handbrake state)
    bool   m_sirenOn;					                 // is this vehicle's siren currently on?
    bool   m_pretendOccupants;			                 // does this vehicle have pretend occupants
    bool   m_useRespotEffect;
	bool   m_runningRespotTimer;		                 // is this vehicle running the car respot timer
    bool   m_isDriveable;				                 // is this vehicle drivable?
    bool   m_alarmSet;					                 // is the alarm set?
    bool   m_alarmActivated;			                 // is the alarm activated
	bool   m_isStationary;				                 // is this a stationary car
	bool   m_isParked;									 // is this a parked car
	bool   m_hasTimedExplosion;			                 // is there a timed explosive on this vehicle
    bool   m_DontTryToEnterThisVehicleIfLockedForPlayer; // should players attempt to enter vehicle if its locked for them?
	bool   m_lightsOn;									 // lights on
	bool   m_headlightsFullBeamOn;						 // headlights full beam on
	bool   m_roofLowered;								 // set when a convertible has the roof open
	bool   m_removeWithEmptyCopOrWreckedVehs;            // consider this veh with cop/wrecked vehs for removal purposes
	bool   m_moveAwayFromPlayer;						 // should this veh move away from the player
	bool   m_forceOtherVehsToStop;						 // should other vehicles be forced to stop for this one
	bool   m_radioStationChangedByDriver;				 // driver changed current radio station
	bool   m_noDamageFromExplosionsOwnedByDriver;		 //
	bool   m_flaggedForCleanup;							 // flagged for cleanup
	bool   m_ghost;
	bool   m_hasLastDriver;
	bool   m_influenceWantedLevel;
	bool   m_hasBeenOwnedByPlayer;
	bool   m_AICanUseExclusiveSeats;					 // AI can use driver seat even if marked exclusive
	bool   m_placeOnRoadQueued;
	bool   m_planeResistToExplosion;
	bool   m_mercVeh;
	bool   m_vehicleOccupantsTakeExplosiveDamage;
	bool   m_canEjectPassengersIfLocked;
	bool   m_RemoveAggressivelyForCarjackingMission;	 // Allows the vehicle to be removed aggressively during the car jacking missions
	bool   m_OverridingVehHorn;							 // Is vehicle horn has been overriden
	bool   m_UnFreezeWhenCleaningUp;					 // Vehicle flag set by script but can't be synced in script node because it would reset
	bool   m_usePlayerLightSettings;
	bool   m_isTrailerAttachmentEnabled;                 // Script can disable trailers from attaching themselves
	bool   m_detachedTombStone;
	bool   m_fullThrottleActive;						 // Is the Full Throttle effect being applied to this vehicle
	bool   m_driftTyres;
	bool   m_disableSuperDummy;
	bool   m_checkForEnoughRoomToFitPed;
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleScriptGameStateDataNode
//
// Handles the serialization of a vehicle's game state
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleScriptGameStateDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Script Game State", CVehicleScriptGameStateDataNode, IVehicleNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetVehicleScriptGameState(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetVehicleScriptGameState(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

    virtual bool ResetScriptStateInternal();

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;

	virtual void SetDefaultState();

public:

	
    IVehicleNodeDataAccessor::ScriptVehicleFlags m_VehicleFlags;
    u32											 m_PopType;
    s32											 m_restrictedAmmoCount[MAX_NUM_VEHICLE_WEAPONS];
	TeamFlags                                    m_TeamLocks;
    PlayerFlags                                  m_TeamLockOverrides;
	float										 m_CollisionWithMapDamageScale;
	float										 m_ScriptMaxSpeed;
	float										 m_fScriptDamageScale;
	float										 m_fScriptWeaponDamageScale;
	float										 m_fRampImpulseScale;
	float										 m_fOverrideArriveDistForVehPersuitAttack;
    u8                                           m_GarageInstanceIndex;
	bool										 m_isinair;					// is the vehicle in the air
	unsigned									 m_DamageThreshold;
	bool										 m_bBlockWeaponSelection;
	bool										 m_isBeastVehicle;
	ObjectId									 m_VehicleProducingSlipstream;
	bool										 m_IsCarParachuting;
	float                                        m_parachuteStickX;
	float                                        m_parachuteStickY;
	bool                                         m_lockedToXY;				// is this amphibious locked in the XY plane (anchored)
	float                                        m_BuoyancyForceMultiplier; // shows us how much the boat wants to float back up. 0 when the boat is sinking the fastest.
	bool										 m_bBoatIgnoreLandProbes;
	float                                        m_rocketBoostRechargeRate;
	ObjectId                                     m_parachuteObjectId;
	bool                                         m_hasParachuteObject;
	bool                                         m_disableRampCarImpactDamage;
	bool										 m_tuckInWheelsForQuadBike;
	u8                                           m_vehicleParachuteTintIndex;
	bool										 m_hasHeliRopeLengthSet;
	float										 m_HeliRopeLength;
    float                                        m_ExtraBoundAttachAllowance;
	bool										 m_ScriptForceHd;
	bool										 m_CanEngineMissFire;
	bool										 m_DisableBreaking;
	u8											 m_gliderState;
	bool										 m_disableCollisionUponCreation;  // Disable collision for 1 frame upon creation
	bool										 m_disablePlayerCanStandOnTop;
	bool										 m_canPickupEntitiesThatHavePickupDisabled;
	s32											 m_BombAmmoCount;
	s32											 m_CountermeasureAmmoCount;
	bool										 m_InSubmarineMode;
	bool										 m_TransformInstantly;
	bool										 m_AllowSpecialFlightMode;
	bool										 m_SpecialFlightModeUsed;
	bool                                         m_homingCanLockOnToObjects;
	bool										 m_DisableVericalFlightModeTransition;
	bool                                         m_HasOutriggerDeployed;
	bool										 m_UsingAutoPilot;
	bool                                         m_DisableHoverModeFlight;
	bool										 m_RadioEnabledByScript;
	bool										 m_bIncreaseWheelCrushDamage;
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleHealthDataNode
//
// Handles the serialization of a vehicle's health
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleHealthDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Health", CVehicleHealthDataNode, IVehicleNodeDataAccessor);

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetVehicleHealth(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetVehicleHealth(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	float    m_suspensionHealth[NUM_VEH_CWHEELS_MAX]; // the health of the suspension for the wheels
	float    m_tyreWearRate[NUM_VEH_CWHEELS_MAX];
	bool     m_tyreDamaged[NUM_VEH_CWHEELS_MAX];      // indicates which tyres are damaged
	bool     m_tyreDestroyed[NUM_VEH_CWHEELS_MAX];    // indicates which tyres are destroyed
	bool	 m_tyreBrokenOff[NUM_VEH_CWHEELS_MAX];
	bool	 m_tyreFire[NUM_VEH_CWHEELS_MAX];

	u64		 m_lastDamagedMaterialId;				  // last material id that was damaged for vehicle
    s32      m_packedEngineHealth;                    // packed engine health value
    s32      m_packedPetrolTankHealth;                // packed petrol tank health value
    u32      m_numWheels;                             // number of wheels on this car
	bool     m_tyreHealthDefault;                     // is the tyre health for all wheels at the default
    bool     m_suspensionHealthDefault;               // is the suspension health for all wheels at the default
    bool     m_hasDamageEntity;                       // has this vehicle been damaged by another entity?
	bool	 m_hasMaxHealth;                          // health is max
	u32      m_health;                                // health
	u32		 m_bodyhealth;							  // body health
	u32      m_weaponDamageHash;                      // weapon damage Hash 
	ObjectId m_weaponDamageEntity;                    // weapon damage entity (only for script objects???????????????)
	u8		 m_fixedCount;							  // counter that is sync'd to trigger remote fix of vehicle when local fix of vehicle happens
	u8		 m_extinguishedFireCount;

	bool     m_isWrecked;                             // is this vehicle wrecked?
	bool	 m_isBlownUp;							  // was the vehicle wrecked by explosion?
	bool	 m_healthsame;							  // if the health is the same as body health
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleSteeringDataNode
//
// Handles the serialization of a vehicle's steering
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleSteeringDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Steering", CVehicleSteeringDataNode, IVehicleNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
         vehicleNodeData.GetSteeringAngle(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetSteeringAngle(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

    float m_steeringAngle; // steering angle
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleControlDataNode
//
// Handles the serialization of a vehicle's gas and brake
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleControlDataNode : public CSyncDataNodeFrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Control Node", CVehicleControlDataNode, IVehicleNodeDataAccessor);

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetVehicleControlData(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetVehicleControlData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

	static const unsigned int NUM_WHEELS = 4;

public:

	u32   m_numWheels;          // number of wheels on this car
    float m_throttle;			// the current value of the throttle
    float m_brakePedal;			// the current value of the brake pedal
	float m_topSpeedPercent;	// set to the maximum speed a vehicle can travel at
	int	  m_roadNodeAddress;	// the current road node the vehicle is driving from
	bool  m_kersActive;			// indicates if the kers system is active
	bool  m_bringVehicleToHalt; // CTaskBringVehicleToHalt is running as a secondary task
	float m_BVTHStoppingDist;	// CTaskBringVehicleToHalt stopping dist
	bool  m_BVTHControlVertVel;	// CTaskBringVehicleToHalt bControlVerticalVelocity
	bool  m_bModifiedLowriderSuspension; // CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate; player has modified suspension of lowrider
	bool  m_bAllLowriderHydraulicsRaised;// CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate; player has raised all lowrider suspension
	float m_fLowriderSuspension[NUM_VEH_CWHEELS_MAX]; // Syncs modified lowrider suspension values
	bool  m_bPlayHydraulicsBounceSound;			// Hydraulics sound effect when bouncing
	bool  m_bPlayHydraulicsActivationSound;		// Hydraulics sound effect when activated
	bool  m_bPlayHydraulicsDeactivationSound;	// Hydraulics sound effect when de-activated
	bool  m_reducedSuspensionForce;				// reduced suspension force used to "stance" tuner pack vehicles
	bool  m_bIsClosingAnyDoor;
	bool  m_HasTopSpeedPercentage;
	bool  m_HasTargetGravityScale;  // For hover vehicles
	float m_TargetGravityScale;  
	float m_StickY;

	bool  m_isInBurnout;
	bool  m_isSubCar;
	float m_subCarYaw;   //the current value of the yaw control for sub cars
	float m_SubCarPitch; //the current value of the pitch control for sub cars
	float m_SubCarDive;  //the current value of the dive control for sub cars

    bool m_bNitrousActive;
	bool m_bIsNitrousOverrideActive;	
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleAppearanceDataNode
//
// Handles the serialization of a vehicle's appearance
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleAppearanceDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Appearance", CVehicleAppearanceDataNode, IVehicleNodeDataAccessor);

	virtual bool IsCreateNode() const { return true; }

	bool IsAlwaysSentWithCreate() const { return true; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData);

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData);

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned SIZEOF_WHEEL_MOD_INDEX   = 8;
	static const unsigned MAX_WHEEL_MOD_INDEX	   = (1 << SIZEOF_WHEEL_MOD_INDEX) - 1;
	static const unsigned SIZEOF_LICENCE_PLATE     = 8;  // Licence (British English) or License (American English) <sigh>
	static const unsigned MAX_VEHICLE_BADGES	   = DECAL_NUM_VEHICLE_BADGES; // this should be in sync with the thing in decal settings
	class CVehicleBadgeData
	{
	public:
		FW_REGISTER_CLASS_POOL(CVehicleBadgeData);

		bool operator==(const CVehicleBadgeData& other) const
		{
			return ((m_offsetPos == other.m_offsetPos) && (m_size == other.m_size) && (m_badgeIndex == other.m_badgeIndex));
		}

		bool operator!=(const CVehicleBadgeData& other) const
		{
			return !((m_offsetPos == other.m_offsetPos) && (m_size == other.m_size) && (m_badgeIndex == other.m_badgeIndex));
		}

		Vector3             m_offsetPos;
		Vector3             m_dir;
		Vector3             m_side;
		s32                 m_boneIndex;
		float               m_size;
		u32					m_badgeIndex;  
		u8					m_alpha;
	};

	CVehicleBadgeData m_VehicleBadgeData[MAX_VEHICLE_BADGES];
	CVehicleBadgeDesc  m_VehicleBadgeDesc;

	u32     m_disableExtras;         // bit flags indicating which "extra" car parts are disabled
	u32     m_liveryID;              // ID of the livery for the vehicle
	u32     m_livery2ID;             // ID of the livery2 for the vehicle
	
	u32		m_allKitMods[NUM_KIT_MOD_SYNC_VARIABLES];
	u32		m_toggleMods;			 // bitfield of the toggle mods that are switched on
	s32		m_LicencePlateTexIndex;  // Licence plate texture index.

	u32		m_horntype;

	u8		m_licencePlate[SIZEOF_LICENCE_PLATE]; // Licence Plate

    u8      m_bodyColour1;           // vehicle body colour 1
    u8      m_bodyColour2;           // vehicle body colour 2
    u8      m_bodyColour3;           // vehicle body colour 3
    u8      m_bodyColour4;           // vehicle body colour 4
    u8      m_bodyColour5;           // vehicle body colour 5
    u8      m_bodyColour6;           // vehicle body colour 6
    u8      m_bodyDirtLevel;         // vehicle body dirt level
	u16		m_kitIndex;				 // the kit index that the variation data is using	 
	u8		m_wheelType;			 // wheel type value
	u8		m_wheelMod;				 // wheel mod value
	u8		m_rearWheelMod;			 // rear wheel mod value (for bikes) 
	u8		m_windowTint;			 // window tint
	u8		m_smokeColorR;			 // smoke color R
	u8		m_smokeColorG;			 // smoke color G
	u8		m_smokeColorB;			 // smoke color B
	u8		m_customPrimaryR;		 // custom secondary color R
	u8		m_customPrimaryG;		 // custom secondary color G
	u8		m_customPrimaryB;		 // custom secondary color B
	u8		m_customSecondaryR;		 // custom secondary color R
	u8		m_customSecondaryG;		 // custom secondary color G
	u8		m_customSecondaryB;		 // custom secondary color B
	u8		m_neonColorR;			 // neon color R
	u8		m_neonColorG;			 // neon color G
	u8		m_neonColorB;			 // neon color B

	u8		m_envEffScale;

	bool    m_bVehicleBadgeData[MAX_VEHICLE_BADGES];
	bool	m_bWindowTint;			 // has a window tint
	bool	m_bSmokeColor;			 // has a smoke color
	bool	m_VehicleBadge;
	bool	m_wheelVariation0;
	bool	m_wheelVariation1;
	bool	m_customPrimaryColor;
	bool	m_customSecondaryColor;
	bool	m_hasLiveryID;
	bool	m_hasLivery2ID;
	bool    m_hasDifferentRearWheel; // has a rear wheel that might have a different type (bikes)
	bool	m_neonOn;
	bool	m_neonLOn;
	bool	m_neonROn;
	bool	m_neonFOn;
	bool	m_neonBOn;
	bool    m_neonSuppressed;
};


///////////////////////////////////////////////////////////////////////////////////////
// CVehicleDamageStatusDataNode
//
// Handles the serialization of a vehicle's damage status
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleDamageStatusDataNode : public CSyncDataNodeInfrequent
{
protected:

	NODE_COMMON_IMPL("Vehicle Damage Status", CVehicleDamageStatusDataNode, IVehicleNodeDataAccessor);

	virtual bool IsCreateNode() const { return true; }

	bool IsAlwaysSentWithCreate() const { return true; }

	virtual bool GetIsManualDirty() const { return true; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

private:

	void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData);

	void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData);

	void Serialise(CSyncDataBase& serialiser);

public:

	u8   	m_weaponImpactPointLocationCounts[NUM_NETWORK_DAMAGE_DIRECTIONS];

	u8		m_frontLeftDamageLevel;
	u8		m_frontRightDamageLevel;
	u8		m_middleLeftDamageLevel;
	u8		m_middleRightDamageLevel;
	u8		m_rearLeftDamageLevel;
	u8		m_rearRightDamageLevel;

	bool    m_windowsBroken[SIZEOF_WINDOW_DAMAGE];				    // array of broken windows
	float   m_armouredWindowsHealth[SIZEOF_WINDOW_DAMAGE];	        // the health of all the windows (if bulletproof / armoured)	
	bool    m_sirensBroken[SIZEOF_SIREN_DAMAGE];			        // array of broken sirens
	bool    m_lightsBroken[SIZEOF_LIGHT_DAMAGE];			        // array of broken lights
	u8		m_frontBumperState;									    // the front bumper state
	u8		m_rearBumperState;								        // the rear bumper state
	u8      m_armouredPenetrationDecalsCount[SIZEOF_WINDOW_DAMAGE]; // number of bullet penetration decals for armoured windows    

	bool    m_hasDeformationDamage;			 // has this vehicle got deformation damage
	bool    m_hasLightsBroken;				 // true if any lights are broken
	bool    m_hasSirensBroken;				 // true if any sirens are broken
	bool    m_hasWindowsBroken;				 // true if any windows are broken
	bool	m_hasBrokenBouncing;			 // whether the front or rear bumper states are set
	bool	m_weaponImpactPointLocationSet;  // if there are any weapon impacts to set/send
	bool    m_hasArmouredGlass;              // windows are bulletproof / armoured

};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleTaskDataNode
//
// Handles the serialization of a vehicle's AI task
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleTaskDataNode : public CSyncDataNodeInfrequent
{
public:

	static const unsigned int MAX_VEHICLE_TASK_DATA_SIZE = 255;

	typedef void (*fnTaskDataLogger)(netLoggingInterface *log, u32 taskType, const u8 *taskData, const int numBits);

	static void SetTaskDataLogFunction(fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

protected:

	NODE_COMMON_IMPL("Vehicle AI Task", CVehicleTaskDataNode, IVehicleNodeDataAccessor);

private:

	void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
	{
		vehicleNodeData.GetVehicleAITask(*this);
	}

	void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
	{
		vehicleNodeData.SetVehicleAITask(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

public:

	// static task data logging function
	static fnTaskDataLogger m_taskDataLogFunction;

	u32 m_taskType;
	u32 m_taskDataSize;
	u8  m_taskData[MAX_VEHICLE_TASK_DATA_SIZE];
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleComponentReservationDataNode
//
// Handles the serialization of a vehicle's component reservation
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleComponentReservationDataNode : public CSyncDataNodeInfrequent
{
protected:

    NODE_COMMON_IMPL("Vehicle Component Reservation", CVehicleComponentReservationDataNode, IVehicleNodeDataAccessor);

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.GetComponentReservations(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
        vehicleNodeData.SetComponentReservations(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

	virtual bool CanHaveDefaultState() const { return true; } 
	virtual bool HasDefaultState() const;
	virtual void SetDefaultState();

public:

	static const unsigned int MAX_VEHICLE_COMPONENTS = CComponentReservation::MAX_NUM_COMPONENTS;

	bool m_hasReservations;
    u32 m_numVehicleComponents;
    ObjectId m_componentReservations[MAX_VEHICLE_COMPONENTS];
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleProximityMigrationDataNode
//
// Handles the serialization of a physical's migration data
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleProximityMigrationDataNode : public CProjectBaseSyncDataNode
{
public:

	static const unsigned int MAX_TASK_MIGRATION_DATA_SIZE = 100;

	typedef void (*fnTaskDataLogger)(netLoggingInterface *log, u32 taskType, const u8 *taskData, const int numBits);

	static void SetTaskDataLogFunction(fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

protected:

    NODE_COMMON_IMPL("Vehicle Proximity Migration", CVehicleProximityMigrationDataNode, IVehicleNodeDataAccessor);

    virtual bool GetIsSyncUpdateNode() const { return false; }

private:

    void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
       vehicleNodeData.GetVehicleMigrationData(*this);
    }

    void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
    {
       vehicleNodeData.SetVehicleMigrationData(*this);
    }

    void Serialise(CSyncDataBase& serialiser);

public:

	static const unsigned int SIZEOF_MAX_VEHICLE_SEATS;
	static const unsigned int SIZEOF_TASK_TYPE;
	static const unsigned int SIZEOF_STATUS;
	static const unsigned int SIZEOF_MIGRATIONDATASIZE;

	// static task data logging function
	static fnTaskDataLogger m_taskDataLogFunction;

	u32      m_maxOccupants;                   // maximum number of passengers for this vehicle
    bool     m_hasOccupant[MAX_VEHICLE_SEATS]; // does this vehicle have passengers?
    ObjectId m_occupantID[MAX_VEHICLE_SEATS];  // IDs of the passengers
	bool     m_hasPopType;                     // has a population type
	u32      m_PopType;                        // population type
    u32      m_status;                         // vehicle status flags
    u32	     m_lastDriverTime;                 // the last time this vehicle had a driver
    bool     m_isMoving;                       // is the vehicle moving (if not pos and vel not sent)
    Vector3  m_position;                       // current vehicle position
    s32      m_packedVelocityX;                // current velocity X (packed)
    s32      m_packedVelocityY;                // current velocity Y (packed)
    s32      m_packedVelocityZ;                // current velocity Z (packed)
    float    m_SpeedMultiplier;                // speed multiplier (used when calculating cruise speeds)
	u16		 m_RespotCounter;				   // remaining time from respotting
	bool	 m_hasTaskData;					   // does the vehicle have any task data to sync
	u32		 m_taskType;					   // the current AI task type
	u32		 m_taskMigrationDataSize;                           // the size of the migration data for the current AI task
	u8		 m_taskMigrationData[MAX_TASK_MIGRATION_DATA_SIZE]; // the migration data of the current AI task
};

///////////////////////////////////////////////////////////////////////////////////////
// CVehicleGadgetDataNode
//
// Handles the serialization of a vehicle's gadget
///////////////////////////////////////////////////////////////////////////////////////
class CVehicleGadgetDataNode : public CSyncDataNodeFrequent
{
public:

	static const unsigned int MAX_NUM_GADGETS = 2;

	typedef void (*fnGadgetDataLogger)(netLoggingInterface *log, IVehicleNodeDataAccessor::GadgetData& gadgetData);

	static void SetGadgetLogFunction(fnGadgetDataLogger gadgetLogFunction) { m_gadgetLogFunction = gadgetLogFunction; }

	virtual PlayerFlags StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
	{
		return GetIsInCutscene(pObj) ? ~0u : 0;
	}

protected:

	NODE_COMMON_IMPL("Vehicle Gadget Data", CVehicleGadgetDataNode, IVehicleNodeDataAccessor);

private:

	void ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
	{
		vehicleNodeData.GetVehicleGadgetData(*this);
	}

	void ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
	{
		vehicleNodeData.SetVehicleGadgetData(*this);
	}

	void Serialise(CSyncDataBase& serialiser);

public:

	static fnGadgetDataLogger m_gadgetLogFunction;

    bool    m_IsAttachedTrailer;
    Vector3 m_OffsetFromParentVehicle;
	u32     m_NumGadgets;
	IVehicleNodeDataAccessor::GadgetData m_GadgetData[MAX_NUM_GADGETS];
};

#endif  // VEHICLE_SYNC_NODES_H
