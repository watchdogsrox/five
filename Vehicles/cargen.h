/****************************************
 * cargen.h                             *
 * ----------                           *
 *                                      *
 * declarations for                     *
 * car generator control for coop		*
 *                                      *
 * GSW 07/06/00                         *
 *                                      *
 * (c)2000 Rockstar North               *
 ****************************************/

#ifndef CARGEN_H
#define CARGEN_H

#include "atl/queue.h"
#include "atl/pool.h"
#include "modelInfo/vehicleModelInfo.h"
#include "physics/WorldProbe/worldprobe.h"
#include "task/Scenario/ScenarioChaining.h"
#include "vector/vector3.h"

//#define INFINITE_CAR_GENERATE (0xffff)

#define MAX_CAR_GENERATORS_CREATED_BY_SCRIPT	(80)

#define STORE_CARGEN_PLACEMENT_FILE __DEV

#define MAX_SCHEDULED_CARS_AT_ONCE (32) // can only have this many vehicles pending spawn at once


enum {
	CREATION_RULE_ALL = 0,
	CREATION_RULE_ONLY_SPORTS,
	CREATION_RULE_NO_SPORTS,
	CREATION_RULE_ONLY_BIG,
	CREATION_RULE_NO_BIG,
	CREATION_RULE_ONLY_BIKES,
	CREATION_RULE_NO_BIKES,
	CREATION_RULE_ONLY_DELIVERY,
	CREATION_RULE_NO_DELIVERY,
	CREATION_RULE_BOATS,
	CREATION_RULE_ONLY_POOR,
	CREATION_RULE_NO_POOR,
	CREATION_RULE_CAN_BE_BROKEN_DOWN	// Only cars that are valid for broken down car scenario
};

enum {
	CARGEN_SCENARIO_NONE = 0xff,
};

enum {
	CARGEN_FORCESPAWN		= BIT(0),
	CARGEN_IGNORE_DENSITY	= BIT(1),
	CARGEN_POLICE			= BIT(2),
	CARGEN_FIRETRUCK		= BIT(3),
	CARGEN_AMBULANCE		= BIT(4),
	CARGEN_DURING_DAY		= BIT(5),
	CARGEN_AT_NIGHT			= BIT(6),
	CARGEN_ALIGN_LEFT		= BIT(7),
	CARGEN_ALIGN_RIGHT		= BIT(8),
	CARGEN_SINGLE_PLAYER	= BIT(9),
	CARGEN_NETWORK_PLAYER	= BIT(10),
	CARGEN_LOWPRIORITY		= BIT(11),
	CARGEN_PREVENT_ENTRY	= BIT(12)
};

struct CargenPriorityAreas
{
	struct Area
	{
		Vector3 minPos;
		Vector3 maxPos;
		PAR_SIMPLE_PARSABLE;
	};

	atArray<Area> areas;

	PAR_SIMPLE_PARSABLE;
};

typedef struct
{
	s32	nCarIndex;
	char	szNumPlate[CUSTOM_PLATE_NUM_CHARS+1];	// the text itself

} TSpecialPlate;

class CCarGenerator;

struct ScheduledVehicle
{
	ScheduledVehicle() : carGen(NULL), asyncHandle((u32)-1), vehicle(NULL), pDestInteriorProxy(NULL), chainUser(NULL), forceSpawn(false), needsStaticBoundsStreamingTest(true), valid(false), addToGameWorld(true) {}
    ~ScheduledVehicle() { Invalidate(); }

	void Invalidate();

	Matrix34 creationMatrix;
	fwModelId carModelId;
	fwModelId trailerModelId;
	CCarGenerator* carGen;
	WorldProbe::CShapeTestSingleResult collisionTestResult;
	u32 asyncHandle;
	RegdVeh vehicle;
	CInteriorProxy* pDestInteriorProxy;		
	s32 destRoomIdx;
	CScenarioPointChainUseInfoPtr chainUser;
	bool allowedToSwitchOffCarGenIfSomethingOnTop;
	bool isAsync;
	bool preventEntryIfNotQualified;
	bool forceSpawn;
	bool needsStaticBoundsStreamingTest;
	bool valid;
	bool addToGameWorld;	// If false, keep the vehicle hidden, the user will need to add to the game world and fade in.
};

class CSpecialPlateHandler
{
public:
	enum { MAX_SPECIAL_PLATES=15 };

	void	Init();
	void	Add(s32 nCarIndex, char* szNumPlate);

	s32	Find(s32 nIndex, char* str);
	bool	IsFull() { return (m_nSpecialPlates == MAX_SPECIAL_PLATES); }
	bool	IsEmpty() { return (m_nSpecialPlates == 0); }
	void 	Remove(s32 nIndex);


//protected:
//private:
	TSpecialPlate	m_aSpecialPlates[MAX_SPECIAL_PLATES];
	s32			m_nSpecialPlates;
};

//static CSpecialPlateHandler g_SpecialPlateHandler;

class CTheCarGenerators;
class CPlayerInfo;

class CCarGenerator
{
	friend class CTheCarGenerators;
public :
	enum {
		INFINITE_CAR_GENERATE = 101
	};

	CCarGenerator() : m_bScheduled(false), m_bTempHighPrio(false), m_bPreAssignedModel(false), m_bIgnoreTooClose(false), m_bSkipStaticBoundsTest(false) {}

	void	Setup(	const float centreX, const float centreY, const float centreZ,
					const float vec1X, const float vec1Y,
					const float lengthVec2,
					const u32 visible_model_hash_key, 
					const s32 remap1_for_car, const s32 remap2_for_car, const s32 remap3_for_car, const s32 remap4_for_car, const s32 remap5_for_car, const s32 remap6_for_car,
					u32 flags,
					const u32 ipl,
					const u32 creationRule, const u32 scenario, const bool bOwnedByScript,
                    atHashString popGroup, s8 livery, s8 livery2, bool unlocked, bool canBeStolen);
		
	void	SwitchOff(void);
	void	SwitchOn(void);
	bool	IsSwitchedOff(void) const;

	void	GenerateMultipleVehs(u32 number_of_cars_to_generate) { SwitchOn(); GenerateCount = (u8)(MIN((u8)number_of_cars_to_generate, INFINITE_CAR_GENERATE)); }

	void	BuildCreationMatrix(Matrix34 *pCreationMatrix, u32 MI) const;
	bool	PlaceOnRoadProperly(CVehicle* pVehicle, Matrix34 *pCreationMatrix, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, u32& asyncHandle, bool forceSynchronous);
	bool	Process(bool bWakeUpScenarioPoints=false);
	void	InstantProcess();

	bool	CheckIfReadyForSchedule(bool bIgnorePopulation, bool& carModelShouldBeRequested, fwModelId& carToUse, fwModelId& trailerToUse);
	bool	ScheduleVehsForGenerationIfInRange(bool bIgnorePopulation, bool bVeryClose, bool& bCarModelPicked);

	//////////////////////////////////////////////////////////////////////////
	// Functions that used by ScheduleVehsForGenerationIfInRange to schedule a cargen attempt
	ScheduledVehicle* InsertVehForGen(const fwModelId& carToUse, const fwModelId& trailerToUse, const Matrix34& CreationMatrix, const bool bAllowedToSwitchOffCarGenIfSomethingOnTop, const bool forceSpawn = false);
	bool	DoSyncProbeForBlockingObjects(const fwModelId &carToUse, const Matrix34& creationMatrix, const CVehicle* excludeVeh);
	bool	CheckHasSpaceToSchedule();
	bool	CheckNetworkObjectLimits(const fwModelId &carToUse);
	bool	CheckScenarioConditionsA();
	bool	CheckScenarioConditionsB(const fwModelId& carToUse, const fwModelId& trailerToUse);
	bool	PickValidModels(fwModelId& car_Out, fwModelId& trailer_Out, bool& carModelShouldBeRequested_Out);
	bool	CheckIsActiveForGen();
	bool	CheckPopulationHasSpace();
	//////////////////////////////////////////////////////////////////////////

	bool	GetCarGenCullRange(float &fOutRange, bool bUseReducedNetworkScopeDist) const;
	bool	CheckIfWithinRangeOfAnyPlayers(bool &bTooClose, bool bUseReducedNetworkScopeDist = false);

	void	SetPlayerHasAlreadyOwnedCar(bool bSet) { m_bPlayerHasAlreadyOwnedCar = bSet; }

	bool	IsUsed(u16 ipl) const;
	bool	IsUsed() const;

	void	SwitchOffIfWithinSwitchArea();

    float   GetGenerationRange() const;

	int		GetScenarioType() const { Assert(IsScenarioCarGen()); return m_Scenario; }
	bool	IsScenarioCarGen() const { return m_Scenario != CARGEN_SCENARIO_NONE; }

	// Check if this is a special kind of cargen allowed to place on the ground without collision being loaded.
	bool	IsDistantGroundScenarioCarGen() const;

	bool	HasPreassignedModel() const { return m_bPreAssignedModel; }
	bool	IsOwnedByScript() const { return m_bOwnedByScript; }

	bool	IsOwnedByMapData() const { return !IsScenarioCarGen() && !IsOwnedByScript(); }

	int		GetIpl() const { return m_ipl; }
	u32		GetVisibleModel() const { return VisibleModel; }
	bool	IsExtendedRange() const { return m_bExtendedRange; }
	bool	IsHighPri() const { return m_bHighPriority; }
	bool	IsScheduled()const { return m_bScheduled; }
	bool	IsLowPrio() const { return m_bLowPriority; }

	Vec3V_Out GetCentre() const { return Vec3V(m_centreX, m_centreY, m_centreZ); }

    u32	PickSuitableCar(fwModelId& trailer) const;

	s8		GetLivery() const { return m_livery; }
	s8		GetLivery2() const { return m_livery2; }

#if __BANK
	void RegisterSpawnFailure(int/*FailureType*/ failureType) const;
#endif

private:	

#if !__FINAL
	void DebugRender(bool forVectorMap);
	static const char * GetCreationRuleDesc(int i);
#endif

	float	m_centreX, m_centreY, m_centreZ;
	float	m_vec1X, m_vec1Y;
	float	m_vec2X, m_vec2Y;

	atHashString m_popGroup;

	u32	VisibleModel;
	RegdVeh m_pLatestCar;
	u16	m_ipl;

	u8	car_remap1, car_remap2, car_remap3, car_remap4, car_remap5, car_remap6;

	u8	m_CreationRule				: 4;
	u16 m_Scenario					: 9;
	
	u8	m_bStoredVisibleModel		: 1;
	u8	m_bReadyToGenerateCar		: 1;
	u8	m_bHighPriority				: 1;
	u8	m_bPlayerHasAlreadyOwnedCar	: 1;

	u8	m_bCarCreatedBefore			: 1;
	u8	m_bScheduled				: 1;		// Set if inserted in m_scheduledVehicles.
	u8	m_bDestroyAfterSchedule		: 1; 		// Set if the cargen should be removed, but was scheduled - will be destroyed later
	u8	m_bReservedForPoliceCar		: 1;
	u8	m_bReservedForFiretruck		: 1;
	u8	m_bReservedForAmbulance		: 1;
	u8	m_bActiveDuringDay			: 1;
	u8	m_bActiveAtNight			: 1;

	u8	m_bAlignedToLeft			: 1;
	u8	m_bAlignedToRight			: 1;
	u8	m_bOwnedByScript			: 1;
	u8	m_bExtendedRange			: 1;
	u8	m_bPreventEntryIfNotQualified:1;
	u8	m_bTempHighPrio				: 1;		// was temporarily set as high prio for being in an area of interest
	u8	m_bPreAssignedModel			: 1;
	u8	m_bIgnoreTooClose			: 1;

	u8 m_bHasGeneratedACar			: 1;
	u8 m_bCreateCarUnlocked			: 1;
	u8 m_bCanBeStolen				: 1;
	u8 m_bInPrioArea				: 1;
	u8 m_bLowPriority				: 1; 
	u8 m_bSkipStaticBoundsTest		: 1;

	u8 GenerateCount;
	s8 m_livery;
	s8 m_livery2;

#if __BANK
	enum
	{ 
		FC_NONE,
		FC_SCHEDULED,
		FC_NO_VALID_MODEL,
		FC_CARGEN_RESTRICTIVE_CHECKS_FAILED,
		FC_CARGEN_PROBE_FAILED,
		FC_CARGEN_PROBE_2_FAILED,
		FC_PLACE_ON_ROAD_FAILED,
		FC_CARGEN_VEH_STREAMED_OUT,
		FC_CARGEN_FACTORY_FAIL,
		FC_CARGEN_PROBE_SCHEDULE_FAILED,
		FC_VEHICLE_REMOVED,
		FC_SCENARIO_POP_FAILED,
		FC_COLLISIONS_NOT_LOADED,
		FC_NOT_ACTIVE_DURING_DAY,
		FC_NOT_ACTIVE_DURING_NIGHT,
		FC_SCRIPT_DISABLED,
		FC_TOO_MANY_CARS,
		FC_TOO_MANY_PARKED_CARS,
		FC_TOO_MANY_LOWPRIO_PARKED_CARS,
		FC_REJECTED_BY_SCENARIO,
		FC_IDENTICAL_MODEL_CLOSE,
		FC_NOT_IN_RANGE,
		FC_TOO_CLOSE_AT_GEN_TIME,
		FC_DONT_SPAWN_COPS,
		FC_CANT_REQUEST_NEW_MODEL,
		FC_CARGEN_BLOCKED_SCENARIO_CHAIN,
		FC_CARGEN_NOT_ALLOWED_IN_MP,
        FC_CARGEN_DESTROYED,
		FC_CARGEN_LOW_LOCAL_POP_LIMIT
	};
	u8	m_failCode;
	Vector3 m_failPos;
	RegdEnt m_hitEntities[4];
	float m_hitPosZ[5];
	u32 placementFail;

	void RemoveRegdEntities() // Clear out m_hitEntities so that we don't try to reference this car generator anymore
	{
		m_hitEntities[0] = NULL;
		m_hitEntities[1] = NULL;
		m_hitEntities[2] = NULL;
		m_hitEntities[3] = NULL;
	}
#endif // __BANK


	// Whether range checks should be performed in 2 or 3 dimensions (typically aerial vehicles want three dimensional checks).
	bool	ShouldCheckRangesIn3D() const;

	bool	CheckIfWithinRangeOfPlayer(CPlayerInfo* pPlayer, float range);

	// Helper function to determine if this point is within range distance of all players.
	bool	CheckIfOutsideRangeOfAllPlayers(float fRange);
	
	float	GetCreationDistanceOnScreen() const;
	float   GetCreationDistanceOffScreen() const;

#if STORE_CARGEN_PLACEMENT_FILE
#define CARGEN_DEV_FILENAME_LENGTH (16)
	char m_placedFrom[CARGEN_DEV_FILENAME_LENGTH];
#endif

};

// This stuff deals with car generators being switched off.
#define MAX_NUM_CAR_GENERATOR_SWITCHES	(128)

//	class car_generator_container_class
class CTheCarGenerators
{
public:
	static void Init();
	static atIteratablePool<CCarGenerator> CarGeneratorPool;
	static u16* ms_CarGeneratorIPLs;
	static s32 NumOfCarGenerators;
	static bool  NoGenerate;
	static bool  bScriptDisabledCarGenerators;
	static bool	 bScriptDisabledCarGeneratorsWithHeli;
	static bool  bScriptForcingAllCarsLockedThisFrame;
	static s32   m_iTryToWakeUpScenarioPoints;
	static s32   m_iTryToWakeUpPriorityAreaScenarioPoints;
	static Vector3	m_CarGenSwitchedOffMin[MAX_NUM_CAR_GENERATOR_SWITCHES];
	static Vector3	m_CarGenSwitchedOffMax[MAX_NUM_CAR_GENERATOR_SWITCHES];
	static Vector3	m_CarGenAreaOfInterestPosition;
	static float m_CarGenAreaOfInterestRadius;
	static float m_CarGenAreaOfInterestPercentage;
	static float m_CarGenScriptLockedPercentage;
	static u32 m_ActivationTime;
	static dev_u32 m_TimeBetweenScenarioActivationBoosts;
	static dev_float m_MinDistanceToPlayerForScenarioActivation;
	static dev_float m_MinDistanceToPlayerForHighPriorityScenarioActivation;
	static s32	m_numCarGenSwitches;
	static u32 m_NumScheduledVehicles;
	static u16	m_scheduledVehiclesNextIndex;
	static atFixedArray<ScheduledVehicle, MAX_SCHEDULED_CARS_AT_ONCE> m_scheduledVehicles;

public :
#if !__FINAL
	static bool gs_bDisplayCarGenerators;
	static bool gs_bDisplayCarGeneratorsOnVMap;
	static bool gs_bDisplayCarGeneratorsScenariosOnly;
	static bool gs_bDisplayCarGeneratorsPreassignedOnly;
	static float gs_fDisplayCarGeneratorsDebugRange;
	static bool gs_bDisplayAreaOfInterest;
#endif

#if __BANK
	static float gs_fForceSpawnCarRadius;
	static bool gs_bCheckPhysicProbeWhenForceSpawn;
#endif // __BANK

//	static u8 divider;

	static void InitLevelWithMapLoaded(void);
	static void InitLevelWithMapUnloaded(void);

	static CCarGenerator *GetCarGenerator(s32 Index) { return (CarGeneratorPool.GetSlot(Index)); }

	static CCarGenerator *GetCarGeneratorSafe(int index)
	{
		return index < CarGeneratorPool.GetSize() ? GetCarGenerator(index) : NULL;
	}

	static int GetSize() { return (int)CarGeneratorPool.GetSize(); }

	static int GetIndexForCarGenerator(const CCarGenerator* cargen) { return CarGeneratorPool.GetIndex((void*)cargen); }
		
	static s32 CreateCarGenerator(const float centreX, const float centreY, const float centreZ,
									const float vec1X, const float vec1Y,
									const float lengthVec2,
									const u32 VisModHashKey, 
									const s32 car_rmap1, const s32 car_rmap2, const s32 car_rmap3, const s32 car_rmap4, const s32 car_rmap5, const s32 car_rmap6,
									u32 flags,
									const u32 ipl, const bool CreatedByScript,
									const u32 creationRule, const u32 scenario,
                                    atHashString popGroup, s8 livery, s8 livery2, bool unlocked, bool canBeStolen);
	static void DestroyCarGeneratorByIndex(s32 index, bool shouldBeScenarioCarGen);
	static void TryToDestroyUnimportantCarGen();
	static void RemoveCarGenerators(u32 ipl);		
	static void SetCarGeneratorsActiveInArea(const Vector3 &vecMin, const Vector3 &vecMax, bool bActive);
	static void SetAllCarGeneratorsBackToActive();
	static void CreateCarsOnGeneratorsInArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ, float Percentage, s32 maxNumCarsInArea);
	static void RemoveCarsFromGeneratorsInArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ);
//	static s32 HijackCarGenerator(Vector3 &Coors, s32 ModelIndex, u8 Remap1, u8 Remap2, u8 Remap3, u8 Remap4, float bestDist = 50.0f);
//	static void  HijackCarGenerator(s32 index, s32 ModelIndex, u8 Remap1, u8 Remap2, u8 Remap3, u8 Remap4);
	static void ReleaseHijackedCarGenerator(s32 CarGenerator);

	static void	InteriorHasBeenPopulated(const spdAABB &bbox);

	static bool ShouldGenerateVehicles();
	static void Process(void);
	static void ProcessAll(void);
//	static void DoPostSetupPhase(void);
	static bool	GenerateScheduledVehiclesIfReady();

	static bool PerformFinalReadyChecks( ScheduledVehicle &veh );
	static bool GenerateScheduledVehicle( ScheduledVehicle &veh, bool ignoreLocalPopulationLimits );
	static bool FinishScheduledVehicleCreation( ScheduledVehicle &veh, bool ignoreLocalPopulationLimits );
	static bool PostCreationSetup(ScheduledVehicle& veh, bool ignoreLocalPopulationLimits);
	static void AddToGameWorld(CVehicle& veh, CVehicle* pTrailer, const fwInteriorLocation& loc);
	static bool IsClusteredScenarioCarGen( ScheduledVehicle &veh );

	static CSpecialPlateHandler	m_SpecialPlateHandler;
	static void	AddSpecialPlate(s32 carID, char* strPlate) { m_SpecialPlateHandler.Add(carID, strPlate); }

	static void StopGenerating() { NoGenerate = true; }
	static void StartGenerating() { NoGenerate = false; }

	static bool ItIsDay();

	static void ClearScheduledQueue();
    static s32 GetNumQueuedVehs() { return m_NumScheduledVehicles; }

	static CargenPriorityAreas* GetPrioAreas() { return ms_prioAreas; }
	static atArray<u32>& GetPreassignedModels() { return ms_preassignedModels; }
	static atArray<u32>& GetScenarioModels() { return ms_scenarioModels; }
	static void VehicleHasBeenStreamedOut(u32 modelIndex);
	static bool RequestVehicleModelForScenario(fwModelId modelId, bool highPri);

	static void InstantFill();

#if !__FINAL
	static void DebugRender();
	static void DumpCargenInfo();
#endif

#if __BANK
	static void InitWidgets(bkBank* bank);
	static void ForceSpawnCarsWithinRadius();
#endif // __BANK

protected:
	static void DropScheduledVehicle(ScheduledVehicle& veh, bool releaseChainUser = true);

	static CargenPriorityAreas* ms_prioAreas;

	static atArray<u32> ms_preassignedModels;
	static atArray<u32> ms_scenarioModels;

	// PURPOSE:	This is the number of car generators that we more or less guarantee will
	//			can be available for vehicle scenarios. If we go beyond that, we will no
	//			longer allow removal of art-placed car generators. 
	static u16	m_MinAllowedScenarioCarGenerators;

	// PURPOSE:	The number of car generators currently in use for vehicle scenarios.
	static u16	m_NumScenarioCarGenerators;

	static u8	m_NumScriptGeneratedCarGenerators;
};

#endif
