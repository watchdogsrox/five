//
// name:        NetworkObjectPopulationMgr.h
// description: Manages levels of network objects of the different types in network games
// written by:  Daniel Yelland
//

#ifndef NETWORK_OBJECT_POPULATION_MGR_H
#define NETWORK_OBJECT_POPULATION_MGR_H

#include "fwnet/NetObject.h"
#include "fwscript/scriptObjInfo.h"
#include "debug/bar.h"

class CGameScriptHandler;
class CNetObjGame;
class CNetworkObjectMgr;

//PURPOSE
// The network object population manager is responsible for managing the levels of the
// different types of game object used in the network game. This includes entities created
// by the ambient populations as well as script and entities created by other areas of the game code.
// It provides functionality for scripts and game systems to reserve game objects for their use at the
// expense of the ambient population, as well as reducing the number of objects created based on factors
// such as bandwidth levels and the number of remote players in the area.
class CNetworkObjectPopulationMgr
{
    friend class CNetworkObjectMgr;
public:

    static const unsigned MAX_EXTERNAL_RESERVATIONS = 1; // Multiple external reservations are not currently supported
    static const unsigned UNUSED_RESERVATION_ID     = MAX_EXTERNAL_RESERVATIONS;

	static const unsigned MAX_EXTERNAL_RESERVED_OBJECTS  = 10;
	static const unsigned MAX_EXTERNAL_RESERVED_PEDS     = 30;
	static const unsigned MAX_EXTERNAL_RESERVED_VEHICLES = 15;

	static int ms_TotalNumVehicles;

	enum eVehicleGenerationContext
	{
		VGT_AmbientPopulation = 0,
		VGT_CarGen,
		VGT_HighPriorityCarGen,
		VGT_EmergencyVehicle,
		VGT_Dispatch,
		VGT_Script,
	};

	enum ePedGenerationContext
	{
		PGT_Default,
		PGT_AmbientDriver,
	};

#if __BANK

    //PURPOSE
    // This structure is used to override the numbers of network objects of
    // the different types that the machine can process. This is used to test
    // the game can cope in busy periods of the game involving more objects than
    // the game can handle.
    struct ObjectManagementDebugSettings
    {
        ObjectManagementDebugSettings();

        bool m_UseDebugSettings; // whether game should use these debug settings
        u32  m_MaxLocalObjects;  // maximum number of objects this machine can own
        u32  m_MaxLocalPeds;     // maximum number of peds this machine can own
        u32  m_MaxLocalVehicles; // maximum number of vehicles this machine can own
        u32  m_MaxTotalObjects;  // maximum number of objects this machine can handle in total
        u32  m_MaxTotalPeds;     // maximum number of peds this machine can handle in total
        u32  m_MaxTotalVehicles; // maximum number of vehicles this machine can handle in total
    };

    //PURPOSE
    // This structure keeps track of the number of times checks for whether an object
    // can be created is rejected by the object population manager for the different failure
    // reasons
    struct PopulationFailureReasons
    {
        PopulationFailureReasons();

        void Reset();

        unsigned m_NumCanRegisterPedFails;        
        unsigned m_NumCanRegisterLocalPedFails;
        unsigned m_NumTooManyTotalPedFails;
        unsigned m_NumTooManyLocalPedFails;
        unsigned m_NumCanRegisterVehicleFails;
        unsigned m_NumCanRegisterLocalVehicleFails;
        unsigned m_NumTooManyTotalVehicleFails;
        unsigned m_NumTooManyLocalVehicleFails;
        unsigned m_NumCanRegisterObjectsFails;
        unsigned m_NumCanRegisterObjectsPedFails;
        unsigned m_NumCanRegisterObjectsVehicleFails;
        unsigned m_NumCanRegisterObjectsObjectFails;
        unsigned m_NumNoIDFails;
        unsigned m_NumTooManyObjectsFails;
        unsigned m_NumDumpTimerFails;
    };

    static ObjectManagementDebugSettings ms_ObjectManagementDebugSettings;
    static PopulationFailureReasons      ms_LastPopulationFailureReasons;
    static PopulationFailureReasons      ms_PopulationFailureReasons;

    //PURPOSE
    // Adds debug widgets for manipulating the above debug settings
    static void AddDebugWidgets();

    //PURPOSE
    // Displays debug text for the object population manager
    static void DisplayDebugText();

#endif // __BANK

    //PURPOSE
    // Initialises the object population manager at the start of the network session.
    static void Init();

    //PURPOSE
    // Shuts down the object population manager at the start of the network session.
    static void Shutdown();

    //PURPOSE
    // Performs any per frame processing required by the object population manager. Called once per frame
    static void Update();

    //PURPOSE
    // Called to notify the population manager when a new network object has been created
    //PARAMS
    // objectType      - The type of object that has been created
    // isMissionObject - Is the object that has been created a mission object?
    static void OnNetworkObjectCreated(netObject& object);

    //PURPOSE
    // Called to notify the population manager when a network object has been removed
    //PARAMS
    // objectType      - The type of object that has been removed
    // isMissionObject - Is the object that has been removed a mission object?
    static void OnNetworkObjectRemoved(netObject& object);

    //PURPOSE
    // Creates an object reservation with a specified ID. This can be used by
    // an external system to reserve game entities for its own use. The system
    // creating the reservation must set the active reservation index before
    // attempting to register a new network object via the SetCurrentExternalReservationIndex() function
    //PARAMS
    // numPedsToReserve				- The number of peds to reserve
    // numVehiclesToReserve			- The number of vehicles to reserve
	// numObjectsToReserve			- The number of objects to reserve
	// bAllowExcessAllocations		- If set the reservation is allowed to create more than it has reserved
   //RETURNS
    // A handle to the reservation created. UNUSED_RESERVATION_ID is returned if the
    // a reservation could not be created (the game will also assert)
    static unsigned CreateExternalReservation(u32 numPedsToReserve, u32 numVehiclesToReserve, u32 numObjectsToReserve, bool bAllowExcessAllocations = false);

	//PURPOSE
	// Updates the current reservations for an existing external reservation
	//PARAMS
	// reservationID - The ID of the external reservation to update
	// numPedsToReserve     - The number of peds to reserve
	// numVehiclesToReserve - The number of vehicles to reserve
	// numObjectsToReserve  - The number of objects to reserve
	static void UpdateExternalReservation(unsigned reservationID, u32 numPedsToReserve, u32 numVehiclesToReserve, u32 numObjectsToReserve);

	//PURPOSE
	// Finds any existing entities belonging to the current external reservation
	static void FindAllocatedExternalReservations(unsigned reservationID);

	//PURPOSE
	// Returns true if a reservation is in use
	//PARAMS
	// reservationID - The ID of the external reservation to release
	static bool IsExternalReservationInUse(unsigned reservationID);

	//PURPOSE
    // Releases a previously created external reservation
    //PARAMS
    // reservationID - The ID of the external reservation to release
    static void ReleaseExternalReservation(unsigned reservationID);

    //PURPOSE
    // Sets the current reservation index, used to inform the population manager
    // that any subsequent object creations will use previously reserved objects
    //PARAMS
    // reservationID - The ID of the external reservation to set
    static void SetCurrentExternalReservationIndex(unsigned reservationID);

    //PURPOSE
    // Clears the current reservation index, used to inform the population manager
    // that any subsequent object creations will use the general management code
    static void ClearCurrentReservationIndex();

	//PURPOSE
	// Gets the current number of reserved objects for this reservation
	static void GetNumReservedObjects(unsigned reservationID, u32& numPeds, u32& numVehicles, u32& numObjects);

	//PURPOSE
	// Gets the current number of reserved objects for this reservation
	static void GetNumCreatedObjects(unsigned reservationID, u32& numPeds, u32& numVehicles, u32& numObjects);

	//PURPOSE
	// Sets the current number of created objects for this reservation
	static void SetNumCreatedObjects(unsigned reservationID, u32 numPeds, u32 numVehicles, u32 numObjects);

    //PURPOSE
    // Gets number of network objects of the given type controlled by this machine
    //PARAMS
    // objectType - The type of objects to return the number of
    static int GetNumLocalObjects(NetworkObjectType objectType);

    //PURPOSE
    // Gets number of network vehicles controlled by this machine
    static int GetNumLocalVehicles();

	//PURPOSE
	// Gets number of network vehicles controlled by this machine
	static int GetNumCachedLocalVehicles() { return ms_CachedNumLocalVehicles; }

    //PURPOSE
    // Gets number of network objects of a vehicle type owned by other machines
    //PARAMS
    // objectType - The type of objects to return the number of
    static int GetNumRemoteObjects(NetworkObjectType objectType);

    //PURPOSE
    // Gets number of network objects of a vehicle type owned by other machines
    static int GetNumRemoteVehicles();

    //PURPOSE
    // Gets total number of network objects currently in the game
    static int GetTotalNumObjects();

    //PURPOSE
    // Returns the total number of object of each type
    //PARAMS
    // objectType - The type of object to check
    static int GetTotalNumObjectsOfType(NetworkObjectType objectType);

    //PURPOSE
    // Returns the total number of vehicles
	static int GetTotalNumVehicles();

	//PURPOSE
	// Returns the total number of vehicles
	// Note: Thread safe version for accessing this from outside
	static int GetTotalNumVehiclesSafe() { return ms_TotalNumVehicles; }

	//PURPOSE
	// Returns the current maximum number of objects of the specified type this player is allowed to own.
	// This can vary based upon the number of players in the same area
	//PARAMS
	// objectType - The type of object to check
	static u32 GetMaxObjectsAllowedToOwnOfType(NetworkObjectType objectType);

    //PURPOSE
    // Get the current population target levels of locally owned entities
    static u32 GetLocalTargetNumObjects()   { return ms_PopulationTargetLevels.m_TargetNumObjects;  }
    static u32 GetLocalTargetNumPeds()      { return ms_PopulationTargetLevels.m_TargetNumPeds;     }
    static u32 GetLocalTargetNumVehicles()  { return ms_PopulationTargetLevels.m_TargetNumVehicles; }

    //PURPOSE
    // Get the current population desired levels of locally owned entities (to balance across players in the area)
    static u32 GetLocalDesiredNumObjects()   { return ms_BalancingTargetLevels.m_TargetNumObjects;  }
    static u32 GetLocalDesiredNumPeds()      { return ms_BalancingTargetLevels.m_TargetNumPeds;     }
    static u32 GetLocalDesiredNumVehicles()  { return ms_BalancingTargetLevels.m_TargetNumVehicles; }

    //PURPOSE
    // Get the current bandwidth target levels of locally owned entities
    static u32 GetBandwidthTargetNumObjects()   { return ms_CurrentBandwidthTargetLevels.m_TargetNumObjects;  }
    static u32 GetBandwidthTargetNumPeds()      { return ms_CurrentBandwidthTargetLevels.m_TargetNumPeds;     }
    static u32 GetBandwidthTargetNumVehicles()  { return ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles; }

    //PURPOSE
    // Get the maximum bandwidth target levels of locally owned entities (based on estimated bandwidth)
    static u32 GetMaxBandwidthTargetNumObjects()   { return ms_MaxBandwidthTargetLevels.m_TargetNumObjects;  }
    static u32 GetMaxBandwidthTargetNumPeds()      { return ms_MaxBandwidthTargetLevels.m_TargetNumPeds;     }
    static u32 GetMaxBandwidthTargetNumVehicles()  { return ms_MaxBandwidthTargetLevels.m_TargetNumVehicles; }

    //PURPOSE
    // Set the current bandwidth target levels of locally owned entities
    //PARAMS
    // targetNumObjects  - The target number of objects to locally own
    // targetNumPeds     - The target number of peds to locally own
    // targetNumVehicles - The target number of vehicles to locally own
    static void SetBandwidthTargetLevels(u32 targetNumObjects, u32 targetNumPeds, u32 targetNumVehicles);

    //PURPOSE
    // Set the maximum bandwidth target for this machine for use in determining
    // how many local entities we can control
    //PARAMS
    // targetBandwidth - Bandwidth (in KBps).
    static void SetMaxBandwidthTargetLevels(u32 targetBandwidth);

    //PURPOSE
    // Returns whether an object can be registered with these parameters
    //PARAMS
    // objectType      - The type of object to register
    // isMissionObject - Is the object to register a mission object?
    static bool CanRegisterObject(NetworkObjectType objectType, bool isMissionObject);

    //PURPOSE
    // Returns whether an object can be registered with these parameters for the specified number of entities
    //PARAMS
    // numPeds           - The number of peds to register
    // numVehicles       - The number of vehicles to register
    // numObjects        - The number of objects to register
	// numPickups        - The number of pickups to register
	// numDoors          - The number of doors to register
    // areMissionObjects - Are the objects to register mission objects?
    static bool CanRegisterObjects(u32 numPeds, u32 numVehicles, u32 numObjects, u32 numPickups, u32 numDoors, bool areMissionObjects);

    //PURPOSE
    // Returns whether this player is allowed to register another local network object of this type
    //PARAMS
    // objectType      - The type of local object to register
    // isMissionObject - Is the local object a mission object?
    static bool CanRegisterLocalObjectOfType(NetworkObjectType objectType, bool isMissionObject);

    //PURPOSE
    // Returns whether this player is allowed to register another local network object of this type
    //PARAMS
    // objectType        - The type of local objects to register
    // count             - The number of local objects to register
    // areMissionObjects - Are the local objects mission objects?
    static bool CanRegisterLocalObjectsOfType(NetworkObjectType objectType, u32 count, bool areMissionObjects);

    //PURPOSE
    // Returns whether this player is allowed to register another remote network object of this type
    //PARAMS
    // objectType      - The type of remote object to register
	// isMissionObject - Is the remote object a mission object?
	// isReservedObject - Is the remote object included in an external reservation
    static bool CanRegisterRemoteObjectOfType(NetworkObjectType objectType, bool isMissionObject, bool isReservedObject);

    //PURPOSE
    // Returns whether this player can take control of another existing object of this type
    //PARAMS
    // objectType      - The type of object to take control of
    // isMissionObject - Is the object to take control of a mission object?
    static bool CanTakeOwnershipOfObjectOfType(NetworkObjectType objectType, bool isMissionObject);

    //PURPOSE
    // Sets the flag for performs addition population restrictions when creating local objects
    //PARAMS
    // process - Indicates whether the system should process local population limits
    static void SetProcessLocalPopulationLimits(bool process) { ms_ProcessLocalPopulationLimits = process; }

	//PURPOSE
	// Sets the context for vehicle spawning so that we can perform any custom logic.
	//PARAMS
	static void SetVehicleGenerationContext(eVehicleGenerationContext eContext)  { ms_eVehicleGenerationContext = eContext; }

	//PURPOSE
	// Gets the context for vehicle spawning so that we can restore it from calling code.
	//PARAMS
	static eVehicleGenerationContext GetVehicleGenerationContext()  { return ms_eVehicleGenerationContext; }

	//PURPOSE
	// Sets the context for ped spawning so that we can perform any necessary custom logic.
	//PARAMS
	static void SetPedGenerationContext(ePedGenerationContext eContext)  { ms_ePedGenerationContext = eContext; }

	//PURPOSE
	// Gets the context for ped spawning so that we can restore it from calling code.
	//PARAMS
	static ePedGenerationContext GetPedGenerationContext()  { return ms_ePedGenerationContext; }

    //PURPOSE
    // Reassigns a script created object (used when the object migrates)
    //PARAMS
    // networkObject - The network object to reassign
    static void ReassignNetworkMissionObject(CNetObjGame &networkObject);

	//PURPOSE
	// Returns true when there are objects that we dont know if we need to pass control
	// or remove during respawn.
	static bool  PendingTransitionalObjects() { return (ms_NumTransitionalObjects > 0); }

#if !__NO_OUTPUT
    //PURPOSE
    // Returns a string describing the last reason a population request was rejected by the manager
    static const char *GetLastFailTypeString();
#endif // !__NO_OUTPUT

	//PURPOSE
	// Flag/UnFlag transitional network objects from the population control.
	static bool  FlagObjAsTransitional(const ObjectId &);
	static bool  UnFlagObjAsTransitional(const ObjectId &);

    //PURPOSE
    // This struct keeps track of the current percentage shares for each player
    // for the specified object types. This is used to decide how many local game objects
    // can be created
    struct PercentageShare
    {
        PercentageShare()
        {
            Reset();
        }

        void Reset()
        {
            m_ObjectShare  = 0.0f;
            m_PedShare     = 0.0f;
            m_VehicleShare = 0.0f;
        }

        float m_ObjectShare;        // percentage share of objects
        float m_PedShare;           // percentage share of peds
        float m_VehicleShare;       // percentage share of vehicles
    };

private:

	static const unsigned WAIT_TIMER_FOR_POPULATION_TRANSITION = 20*1000;
	static const unsigned MAX_NUM_TRANSITIONAL_OBJECTS   = MAX_NUM_NETOBJVEHICLES+MAX_NUM_NETOBJPEDS;

#if !__NO_OUTPUT
    //PURPOSE
    // Enumeration of the different possible ways the network population manager
    // can reject object creation requests
    enum PopulationFailTypes
    {
        PFT_SUCCESS,
        PFT_TOO_MANY_TOTAL_PEDS,
        PFT_TOO_MANY_LOCAL_PEDS,
        PFT_TOO_MANY_TOTAL_VEHICLES,
        PFT_TOO_MANY_LOCAL_VEHICLES,
        PFT_NO_ID_FAILS,
        PFT_TOO_MANY_OBJECTS,
        PFT_DUMP_TIMER_ACTIVE,
		PFT_SPECTATOR_JOIN_DELAY,
    };
#endif // !__NO_OUTPUT

    //PURPOSE
    // This struct keeps track of the current target number of the different
    // object types to own locally. This is calculated based on the player percentage shares
    struct TargetLevels
    {
        TargetLevels()
        {
            Reset();
        }

        void Reset()
        {
            m_TargetNumObjects   = MAX_NUM_NETOBJOBJECTS;
            m_TargetNumPeds      = MAX_NUM_LOCAL_NETOBJPEDS;
            m_TargetNumVehicles  = MAX_NUM_LOCAL_NETOBJVEHICLES;
        }

        u32 m_TargetNumObjects;  // The target number of objects
        u32 m_TargetNumPeds;     // The target number of peds
        u32 m_TargetNumVehicles; // The target number of vehicles
    };

    //PURPOSE
    // This struct keeps track of a group of game entities reserved
    // by an external system.
    struct ExternalReservation
    {
        ExternalReservation()
        {
            Reset();
        }

        bool IsInUse()
        {
            return m_ReservationID != UNUSED_RESERVATION_ID;
        }

        void Reset()
        {
            m_ReservationID			 = UNUSED_RESERVATION_ID;
            m_ReservedObjects		 = 0;
            m_ReservedPeds			 = 0;
            m_ReservedVehicles		 = 0;
            m_AllocatedObjects		 = 0;
            m_AllocatedPeds			 = 0;
            m_AllocatedVehicles		 = 0;
			m_AllowExcessAllocations = 0;
        }

        unsigned m_ReservationID;			// ID of this reservation; UNUSED_RESERVATION_ID if unused
        u32      m_ReservedObjects;			// The number of objects reserved
        u32      m_ReservedPeds;			// The number of peds reserved
        u32      m_ReservedVehicles;		// The number of vehicles reserved
        u32      m_AllocatedObjects;		// The number of objects allocated
        u32      m_AllocatedPeds;			// The number of peds allocated
        u32      m_AllocatedVehicles;		// The number of vehicles allocated
		bool	 m_AllowExcessAllocations;	// If set, the allocated members are allowed to exceed the reserved members
    };


    CNetworkObjectPopulationMgr();
    CNetworkObjectPopulationMgr(const CNetworkObjectPopulationMgr &);
    ~CNetworkObjectPopulationMgr();

    CNetworkObjectPopulationMgr &operator=(const CNetworkObjectPopulationMgr &);

    //PURPOSE
    // Returns the maximum allowed number of objects of each type (vehicles are counted together)
    //PARAMS
    // objectType - The type of object to check
    static int GetMaxNumObjectsOfType(NetworkObjectType objectType);

    //PURPOSE
    // Returns the number of the different types of game entities reserved by external systems. Entities
    // reserved by the currently active reservation (set via a call to SetCurrentExternalReservationIndex())
    // are not returned.
    //PARAMS
    // numUnusedReservedPeds     - The number of reserved peds that have not been allocated by external systems
    // numUnusedReservedVehicles - The number of reserved vehicles that have not been allocated by external systems
	// numUnusedReservedObjects  - The number of reserved objects that have not been allocated by external systems
    static void GetNumExternallyRequiredEntities(u32 &numUnusedReservedPeds, u32 &numUnusedReservedVehicles, u32 &numUnusedReservedObjects);

    //PURPOSE
    // Returns whether the specified number of objects of this type can be registered from the currently active
    // external reservation. If no reservation is currently active this will never succeed
    //PARAMS
    // objectType        - The type of objects to register
    // count             - The number of objects to register
    static bool CanAllocateFromExternalReservation(NetworkObjectType objectType, u32 count);

    //PURPOSE
    // Returns whether any more objects of this type can be registered regardless of the owner
    //PARAMS
    // objectType      - The type of object to register
	// isMissionObject - Is the object to register a mission object?
	// isReservedObject - Is the object to register a reserved object (created as part of an external reservation)?
    static bool CanRegisterObjectOfType(NetworkObjectType objectType, bool isMissionObject, bool isReservedObject);

    //PURPOSE
    // Returns whether the specified number of objects of this type can be registered regardless of the owner
    //PARAMS
    // objectType        - The type of objects to register
    // count             - The number of objects to register
	// areMissionObjects - Are the objects mission objects?
	// areReservedObjects - Are the objects reserved objects?
    static bool CanRegisterObjectsOfType(NetworkObjectType objectType, u32 count, bool areMissionObjects, bool areReservedObjects);

    //PURPOSE
    // Reduces the bandwidth target levels when the game is struggling to send data to remote players
    static void ReduceBandwidthTargetLevels();

    //PURPOSE
    // Increases the bandwidth target levels when the game is no longer struggling to send data to remote players
    // for a long enough period of time
    static void IncreaseBandwidthTargetLevels();

	//PURPOSE
	// Updates the local object target levels for the different object types
	static void UpdateObjectTargetLevels();

	//PURPOSE
	// Updates objects marked as transitional.
	static void UpdatePendingTransitionalObjects();

    // target levels of the number of local objects to own based on different criteria
    static TargetLevels ms_PopulationTargetLevels;       // Population target levels - restricts population based on network object availability
    static TargetLevels ms_CurrentBandwidthTargetLevels; // Bandwidth target levels  - restricts population based on bandwidth levels
    static TargetLevels ms_MaxBandwidthTargetLevels;     // Max Bandwidth target levels - used to cap current bandwidth target levels
    static TargetLevels ms_BalancingTargetLevels;        // Balancing target levels - used to balance object ownership across players in vicinity

    // the last time the bandwidth target levels were increased
    static u32 ms_LastTimeBandwidthLevelsIncreased;

    // when this flag is set the object population manager will restrict the creation of local
    // objects based on reservations required for local reasons - for example peds reserved for
    // pretend occupants or vehicles reserved for other remote player vehicles
    static bool ms_ProcessLocalPopulationLimits;

	// Allow calling code to specify the generation context we are using so we know how it is being spawned.
	static eVehicleGenerationContext ms_eVehicleGenerationContext;
	static ePedGenerationContext ms_ePedGenerationContext;

    // contains data about reservations made by external systems
    static unsigned ms_CurrentExternalReservation;
    static ExternalReservation ms_ExternalReservations[MAX_EXTERNAL_RESERVATIONS];

	// control object deletion and control passage during a player teleport around the map
	static ObjectId  ms_TransitionalPopulation[MAX_NUM_TRANSITIONAL_OBJECTS];
	static unsigned  ms_TransitionalPopulationTimer;
	static unsigned  ms_NumTransitionalObjects;

#if !__NO_OUTPUT
    static PopulationFailTypes ms_LastFailType;
#endif // !__NO_OUTPUT

	static int ms_CachedNumLocalVehicles;
};

// time interval to wait before allowing ambient objects to be created after a local object has been dumped
extern bank_u32 DUMPED_AMBIENT_ENTITY_TIMER;
// time interval to allow ambient objects to be created after the dumped timer has completed
extern bank_u32 DUMPED_AMBIENT_ENTITY_INCREASE_INTERVAL;

#endif  // #NETWORK_OBJECT_POPULATION_MGR_H
