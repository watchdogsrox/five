//
// streaming/populationstreaming.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_POPULATIONSTREAMING_H
#define STREAMING_POPULATIONSTREAMING_H

#include "atl/array.h"
#include "streaming/streamingdefs.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfoFlags.h"

class CPedStreamRenderGfx;
class CPopZone;
class CVehicleStreamRenderGfx;

#if RSG_ORBIS || RSG_DURANGO || RSG_PC
#define MAX_HD_VEHICLES                 (64) // all models really, we won't have 64 different vehicle models in memory
#define MAX_HD_VEHICLES_IN_MP		    (64)
#define ASSET_MULTIPLIER                (3)
#else
#define MAX_HD_VEHICLES                 (2)
#define MAX_HD_VEHICLES_IN_MP_GARAGE	(5)
#define ASSET_MULTIPLIER                (1)
#endif

#define MIN_NUMBER_STREAMED_CARS    (3 * ASSET_MULTIPLIER)     // The code tries to at least have this many vehicles streamed in.
#define MIN_NUMBER_STREAMED_PEDS    (2 * ASSET_MULTIPLIER)     // The code tries to at least have this many peds streamed in.

#define MAX_NUMBER_STREAMED_SCENARIO_PEDS_PER_SLOT   (2 * ASSET_MULTIPLIER)
#define SCENARIO_DONT_GET_RELOADED_DURATION         (1000)      // Scenario peds will not be reloaded after being streamed out for this long.

#define MAX_NETWORK_MODEL_INTERVALS  (128)
#define NUM_NETWORK_MODEL_VARIATIONS (7)

#define MAX_MEM_LEVELS (4)


#define DEBUG_POPULATION_MEMORY (1 && !__FINAL)

namespace rage {
    class Vector3;
};

class CLoadedModelGroup
{
public:
    CLoadedModelGroup() {Clear();}
    void    Clear();
    bool    AddMember(u32 NewMember);
    bool    RemoveMember(u32 OldMember);
    u32		GetMember(u32 Member) const;
    int		CountMembers() const;
    bool    IsEmpty();
    int		CountUsableOnes();
    bool    IsMemberInGroup(u32 ModelIndex) const;
    bool    IsMaleInPedGroup();
    bool    IsFemaleInPedGroup();
    bool    IsBikeDriverInPedGroup();
    bool    IsBikeInVehGroup();
    strLocalIndex PickRandomCarModel(bool cargens);
	u32		PickRandomPedModel();
    void    SortBasedOnUsage();
    u32		PickLeastUsedModel(int MaxOfThem);
    u32		PickLeastUsedPedModel();
    void    Merge(CLoadedModelGroup *pList1, CLoadedModelGroup *pList2, CLoadedModelGroup *pList3 = NULL, CLoadedModelGroup *pList4 = NULL);
    void    CopyCarsThatBelongInThisZone(CLoadedModelGroup *pSource, CPopZone *pZone);
    void    Copy(CLoadedModelGroup *pSource);
    void    CopyCarsSmallWorkers(CLoadedModelGroup *pSource, bool bSmallWorkers);
	void	CopyEligibleCars(const Vector3 & vPos, CLoadedModelGroup* source, bool smallWorkers, bool removeNonNative, bool removeCopCars, bool removeOnlyOnHighway, bool removeNonOffroad, bool removeBigVehicles);
    void    RemoveCarsFromGroup(s32 popGroup);
	void	RemoveModelsNotInGroup(s32 popGroup, bool peds);
	void	RemoveModelsNotInModelSet(int/*CAmbientModelSetManager::ModelSetType)*/ modelSetType, int modelSetIndex);
    void    RemoveOnlyOnHighwayCarsFromGroup();
	void	RemoveNonOffroadCars();
    void    RemoveNonNativeCars();
    void    RemoveBoats();
    void    RemoveTaxiVehs();
    void    RemoveCopVehs();
    void    RemoveEmergencyServices();
    void    RemoveBigVehicles();
    void    RemoveDiscardedVehicles();
    void    RemoveModelsThatHaveHitMaximumNumber();
    void    RemoveRareModels();
    void    RemoveModelsThatAreUsedNearby(const Vector3 *coors);
	void	RemoveNonMotorcyclePeds();
    void    RemoveVehModelsWithFlagSet(CVehicleModelInfoFlags::Flags flag, bool flagValue = true);
    void    RemovePedModelsThatCantDriveCars(bool forDriver);
    void    RemoveSuppresedPeds();
    void    RemovePedsThatCantDriveCarType(ePedVehicleTypes carType);
	void	RemoveSuppressedAndNonAmbientCars();
	void	RemoveMaleOrFemale(bool male);
	void	RemoveNonPassengerPeds();
	void	RemoveBulkyItemPeds();
	void	SortCarsByDistance(const Vector3& pos, bool cargens);
	void	SortCarsByMemoryUse();
	void	SortCarsByName();
	void	SortPedsByDistance(const Vector3& pos);
	void	SortPedsByMemoryUse();
	void	SortPedsByName();

private:
	void	RemoveIndex(s32 index);

    u32 aMembers[MAX_NUM_IN_LOADED_MODEL_GROUP];

};

struct sPopEntry
{
	strIndex streamIdx;
	s32 refCount;
};

enum eMemoryTrackType
{
	MTT_VEHICLE,
	MTT_PED,
	MTT_WEAPON,

	MTT_MAX
};

class CPopulationStreaming
{
    friend class CLoadedModelGroup;

public:

    //PURPOSE
    // Class Constructor
    CPopulationStreaming();

    //PURPOSE
    // Initialises the population streaming system
    //PARAMS
    // initMode - The type of initialisation to perform
    void Init(unsigned initMode);

	//PURPOSE
	// Loads population streaming values from Tunables (peds and vehicle population limits)
	void OnTunablesLoad();

    //PURPOSE
    // Shuts down the population streaming system
    //PARAMS
    // shutdownMode - The type of shutdown to perform
    void Shutdown(unsigned shutdownMode);

    //PURPOSE
    // Updates the population streaming system
    void Update();

    //PURPOSE
    // Returns the network model variation ID to use when determining which models to stream during network games
    u32 GetNetworkModelVariationID() { return m_NetworkModelVariationID; }

    //PURPOSE
    // Sets the network model variation ID to use when determining which models to stream during network games
    //PARAMS
    // networkModelVariationID - The ID to use
    void SetNetworkModelVariationID(u32 networkModelVariationID);

    //PURPOSE
    // Sets the reduce ambient ped model budget flag. If this is set
    // a reduced number of ambient ped models will be streamed in for
    // use by the ambient population. This is useful for scripts that
    // stream in a large number of ped models.
    //PARAMS
    // value - The value to set the flag to
    void SetReduceAmbientPedModelBudget(bool value);

    //PURPOSE
    // Sets the reduce ambient vehicle model budget flag. If this is set
    // a reduced number of ambient vehicle models will be streamed in for
    // use by the ambient population. This is useful for scripts that
    // stream in a large number of vehicle models.
    //PARAMS
    // value - The value to set the flag to
    void SetReduceAmbientVehicleModelBudget(bool value);

    //PURPOSE
    // This function is called when the population cycle information changes.
    // This affects which cars and peds should be streamed in and updates the
    // system accordingly
    void HandlePopCycleInfoChange(bool enteredNewZone);

    //PURPOSE
    // Attempts to stream one of the currently loaded vehicle models out. Only
    // vehicles that have been used a minimum number of times will be streamed
    // out unless the force parameter is set to true. This function only marks
    // the model as available for removal by the streaming system, so it will
    // not be streamed out until there are no more instances of the model on the map
    //PARAMS
    // force - Indicates whether a model should be streamed out regardless of
    //         how many times it has been used
    bool StreamOneCarOut(bool force = false);

    //PURPOSE
    // Attempts to stream one of the currently loaded ped models out. Only
    // peds that have been used a minimum number of times will be streamed
    // out unless the force parameter is set to true. This function only marks
    // the model as available for removal by the streaming system, so it will
    // not be streamed out until there are no more instances of the model on the map
    //PARAMS
    // force - Indicates whether a model should be streamed out regardless of
    //         how many times it has been used
    bool StreamOnePedOut(bool force = false);

    //PURPOSE
    // Returns whether the ped is available for spawn
    bool IsPedAvailableForSpawn(u32 modelIndex);

	//PURPOSE
	// Returns whether the given vehicle type has a driver loaded in memory or not
	bool IsDriverAvailable(u32 modelIndex);

    //PURPOSE
    // This function is called by the streaming system once a model has
    // finished streaming into memory
    //PARAMS
    // modelIndex - The model index of the model that has streamed in
    void ModelHasBeenStreamedIn(u32 modelIndex);

    //PURPOSE
    // This function is called by the streaming system once a model has
    // finished streaming out of memory
    //PARAMS
    // modelIndex - The model index of the model that has streamed out
    void ModelHasBeenStreamedOut(u32 modelIndex);

    //PURPOSE
    // Returns a model index of a fallback ped. This is an average
    // looking ped that can used when a specific ped model requested is not available
    u32 GetFallbackPedModelIndex(bool bDriversOnly = true);

    //PURPOSE
    // Returns whether there is a fallback ped available. This is an average
    // looking ped that can used when a specific ped model requested is not available
    bool IsFallbackPedAvailable(bool bikesOnly = false, bool bDriversOnly = true);

	//PURPOSE
	// Returns a list of peds that can be used as fallback drivers
	void GetFallbackPedGroup(CLoadedModelGroup& fallbackGroup, bool carDrivers);

    //PURPOSE
    // Scenario code makes requests for peds through this function.
    bool StreamScenarioPeds(fwModelId PedModel, bool highPri, bool startupMode, const char* debugSourceName);

    //PURPOSE
    // Returns a list of streamed in peds available for use in the current location
    CLoadedModelGroup& GetAppropriateLoadedPeds() {return m_AppropriateLoadedPeds;}

    //PURPOSE
    // Returns a list of streamed in peds not available for use in the current location
    CLoadedModelGroup& GetInAppropriateLoadedPeds() {return m_InAppropriateLoadedPeds;}

    //PURPOSE
    // Returns a list of streamed in peds not available in any pop zone
    CLoadedModelGroup& GetSpecialLoadedPeds() {return m_SpecialLoadedPeds;}

    //PURPOSE
    // Returns a list of discarded peds
    CLoadedModelGroup& GetDiscardedPeds() {return m_DiscardedPeds;}

    //PURPOSE
    // Returns a list of streamed in vehicles available for use in the current location
    CLoadedModelGroup& GetAppropriateLoadedCars() {return m_AppropriateLoadedCars;}

	//PURPOSE
	// Returns a list of streamed in vehicles not available for use in the current location
	CLoadedModelGroup& GetInAppropriateLoadedCars() {return m_InAppropriateLoadedCars;}

	//PURPOSE
	// Returns a list of discarded vehicles
	CLoadedModelGroup& GetDiscardedCars() {return m_DiscardedCars;}

	//PURPOSE
	// Returns a list of special vehicles
	CLoadedModelGroup& GetSpecialCars() {return m_SpecialLoadedCars;}

    //PURPOSE
    // Returns a list of streamed in boats available for use in the current location
    CLoadedModelGroup& GetLoadedBoats() {return m_LoadedBoats;}

    //PURPOSE
    // Returns true if the given car model index is in the discarded list
	bool IsCarDiscarded(u32 modelIndex);

    //PURPOSE
    // Displays debug text describing the currently streamed in ped models
    void PrintDebugForPeds();

    //PURPOSE
    // Displays debug text describing the currently streamed in vehicle models
    void PrintDebugForVehicles();

	//PURPOSE
	// Request several vehicles to be streamed in, should be used at startup to populate the appropriate loaded cars array.
	void InitialVehicleRequest(u32 numRequests);

	//PURPOSE
	// Resets the level of the vehicle memory budget to its starting value (called when a mission script terminates)
	void ResetVehMemoryBudgetLevel();

	//PURPOSE
	// Sets the level of the vehicle memory budget
	void SetVehMemoryBudgetLevel(u32 level);

	//PURPOSE
	// Resets the level of the ped memory budget to its starting value (called when a mission script terminates)
	void ResetPedMemoryBudgetLevel();

	//PURPOSE
	// Sets the level of the ped memory budget
	void SetPedMemoryBudgetLevel(u32 level);

	//PURPOSE
	// Add object to list of objects that are considered resident and shouldnt be considered in the budgets
	void AddResidentObject(strIndex streamingIndex) {m_residentObjects.PushAndGrow(streamingIndex);}

	//PURPOSE
	// Remove object from the list of objects that are considered resident and shouldnt be considered in the budgets
	void RemoveResidentObject(strIndex streamingIndex) {m_residentObjectsToRemove.PushAndGrow(streamingIndex);}

	//PURPOSE
	// Return list of objects considered resident and shouldnt be considered in the budgets
	const atArray<strIndex>& GetResidentObjectList() const {return m_residentObjects;}

	//PURPOSE
	// Callback to remove ped objects as they are out to be deleted.
	void RemoveResidentPedObjects();

#if __BANK
	//PURPOSE
	// Overrides all vehicle archetypes with the one specified
	void SetVehicleOverrideIndex(u32 index) { m_vehicleOverrideIndex = index; }
	u32 GetVehicleOverrideIndex() const { return m_vehicleOverrideIndex; }
	void AllocateVehicleAndPedResourceMem();
	void FreeVehicleAndPedResourceMem();

	//PURPOSE
	// Dumps detailed vehicle memory to tty
	void DumpVehicleMemoryInfo();
#endif

	//PURPOSE
	// Checks if a vehicle model is in the allowed network array, if a net game is on
	bool IsVehicleInAllowedNetworkArray(u32 modelIndex) const;

	//PURPOSE
	// Flushes all vehicle models out from memory
	static void FlushAllVehicleModelsHard();

	//PURPOSE
	// Flushes all vehicle models out from memory, ignores streaming requests so doesn't guarantee 100% everything will be ejected
	static void FlushAllVehicleModelsSoft();

    //PURPOSE
    // Returns the currently allowed ped model start offset into the list of peds streamed
    // during network games. This is based off the current network time and dependent on how
    // many unwanted ped models are currently in the process of streaming out
    u32 GetAllowedNetworkPedModelStartOffset() { return m_LocalAllowedPedModelStartOffset; }

    //PURPOSE
    // Returns the currently allowed vehicle model start offset into the list of vehicles streamed
    // during network games. This is based off the current network time and dependent on how
    // many unwanted vehicle models are currently in the process of streaming out
    u32 GetAllowedNetworkVehicleModelStartOffset() { return m_LocalAllowedVehicleModelStartOffset; }

    //PURPOSE
    // In network games the list of models available for streaming is stored in a list
    // consistent across machines, and the current network time is used to generate an
    // offset into this list to cycle the models streamed over time. The model offset
    // can wrap if the network session is running long enough, so when comparing two
    // model offsets to find which is the lower we need to take wrapping into account.
    // This function provides a method of comparing two model offsets based on this considerations
    //PARAMS
    // firstModelOffset  - Model offset to check whether lower than the second
    // secondModelOffset - Model offset to compare first model offset against
    bool IsNetworkModelOffsetLower(u32 firstModelOffset, u32 secondModelOffset);

	//PURPOSE
	// Gets the number of ped models that are loaded due to scenarios.
	// PARAMS
	// eSlotType - which type of ped is being streamed in (generally normal or small for things like birds)
	int GetNumScenarioPedModelsLoaded(eScenarioPopStreamingSlot eSlotType) const { return m_aNumScenarioPedModelsLoaded[eSlotType]; }

#if !__FINAL
	//PURPOSE
	// Gets a string name of the slot for debugging.
	static const char* GetScenarioSlotName(eScenarioPopStreamingSlot eSlotType);
#endif

    //PURPOSE
    // This function is called when a streamed ped gets new assets
    //PARAMS
    // gfx - The streamed render gfx objects which keeps track of all current assets
	void AddStreamedPedVariation(const CPedStreamRenderGfx* gfx);

	//PURPOSE
	// This function is called when a mod variation is set on a vehicle
	//PARAMS
	// gfx - The streamed render gfx objects which keeps track of all current assets
	void AddStreamedVehicleVariation(const CVehicleStreamRenderGfx* gfx);

    //PURPOSE
    // This function is called when a streamed ped has its assets removed
    //PARAMS
    // gfx - The streamed render gfx objects which keeps track of all current assets
	void RemoveStreamedPedVariation(const CPedStreamRenderGfx* gfx);

    //PURPOSE
    // This function is called when a mod variation is removed from a vehicle
    //PARAMS
    // gfx - The streamed render gfx objects which keeps track of all current assets
	void RemoveStreamedVehicleVariation(const CVehicleStreamRenderGfx* gfx);

	//PURPOSE
	// Get the current limit for how many scenario peds can be streamed in
	int GetMaxScenarioPedModelsLoadedPerSlot() const { return m_MaxScenarioPedModelsLoadedPerSlot; }

	//PURPOSE
	// Set the current limit for how many scenario peds can be streamed in
	//PARAMS
	// num - The new limit
	void SetMaxScenarioPedModelsLoadedPerSlot(int num) { m_MaxScenarioPedModelsLoadedPerSlot = num; }

	//PURPOSE
	// Set the script override of how many scenario peds can be streamed in
	//PARAMS
	// num - The new override, or -1 for no override
	void SetMaxScenarioPedModelsLoadedOverride(int num) { m_MaxScenarioPedModelsLoadedOverride = num; }

	//PURPOSE
	// Get the script override of how many scenario peds can be streamed in
	int GetMaxScenarioPedModelsLoadedOverride() const { return m_MaxScenarioPedModelsLoadedOverride; }

	//PURPOSE
	// Temporarily prevent new scenario ped models from being streamed in
	//PARAMS
	// timeInMS - Time in milli-seconds for how long from now to prevent new scenario models
	void DisallowScenarioPedStreamingForDuration(u32 timeInMS);

    //PURPOSE
    // Returns whether the vehicle with the specified model index is part of the
    // current population zone
    //PARAMS
    // modelIndex - The model index of the vehicle to check
    bool DoesCarModelBelongInCurrentPopulationZone(u32 modelIndex);

	//PURPOSE
	// Keeps track on when we spawned a vehicle on highway last, to know if we need to stream anything specific in
    void SpawnedVehicleOnHighway();

    //PURPOSE
	// Returns available memory for vehicles and peds
#if __PS3 || __XENON
	s32 GetMemForVehiclesAdjustment(eMemoryType eType, u32 uLevel) const;
	s32 GetMemForPedsAdjustment(eMemoryType eType, u32 uLevel) const;
#endif // __PS3 || __XENON
	s32 GetMemForVehicles(eMemoryType eType, u32 uLevel) const;
	s32 GetMemForPeds(eMemoryType eType, u32 uLevel) const;
	s32 GetCurrentMemForPeds() const;
	s32 GetCurrentMemForVehicles() const;

	//PURPOSE
	static s32 GetCachedTotalMemUsedByPeds() { return sm_TotalMemUsedByPeds; }
    // Returns total memory used by peds
	s32 GetTotalMemUsedByPeds() const { return m_TotalMemoryUsedByPedModelsMain + m_TotalMemoryUsedByPedModelsVram + m_TotalStreamedPedMemoryMain + m_TotalStreamedPedMemoryVram; }
	s32 GetTotalMainUsedByPeds() const { return m_TotalMemoryUsedByPedModelsMain + m_TotalStreamedPedMemoryMain; }
	s32 GetTotalVramUsedByPeds() const { return m_TotalMemoryUsedByPedModelsVram + m_TotalStreamedPedMemoryVram; }
    s32 GetTotalStreamedMainUsedByPeds() const { return m_TotalStreamedPedMemoryMain; }
    s32 GetTotalStreamedVramUsedByPeds() const { return m_TotalStreamedPedMemoryVram; }

    //PURPOSE
	static s32 GetCachedTotalMemUsedByVehicles() { return sm_TotalMemUsedByVehicles; }
    // Returns total memory used by vehicles
	s32 GetTotalMemUsedByVehicles() const { return m_TotalMemoryUsedByVehicleModelsMain + m_TotalMemoryUsedByVehicleModelsVram + m_TotalStreamedVehicleMemoryMain + m_TotalStreamedVehicleMemoryVram; }
	s32 GetTotalMainUsedByVehicles() const { return m_TotalMemoryUsedByVehicleModelsMain + m_TotalStreamedVehicleMemoryMain; }
	s32 GetTotalVramUsedByVehicles() const { return m_TotalMemoryUsedByVehicleModelsVram + m_TotalStreamedVehicleMemoryVram; }
    s32 GetTotalStreamedMainUsedByVehicles() const { return m_TotalStreamedVehicleMemoryMain; }
    s32 GetTotalStreamedVramUsedByVehicles() const { return m_TotalStreamedVehicleMemoryVram; }

    //PURPOSE
    // Returns total memory used by weapons
	s32 GetTotalMainUsedByWeapons() const { return m_TotalWeaponMemoryMain; }
	s32 GetTotalVramUsedByWeapons() const { return m_TotalWeaponMemoryVram; }

    //PURPOSE
    // This function decides if we should request a driver to the given vehicle and if so adds the request
    //PARAMS
    // modelIndex - The model index of the vehicle that (maybe) needs a driver
	void RequestVehicleDriver(u32 vehicleModelIndex);

	//PURPOSE
	// This function decides if we should undo a driver request
	//PARAMS
	// modelIndex - The model index of the vehicle that doesn't need a driver anymore
	void UnrequestVehicleDriver(u32 vehicleModelIndex);

    bool CanSpawnInMp(u32 modelIndex);

#if __BANK
	float	RenderPedMemoryUse(float x, float y);
	float	RenderVehicleMemoryUse(float x, float y);

#endif	//__BANK

private:

    //PURPOSE
    // Stores information about a currently streamed in model (vehicle or ped)
    struct StreamedInModelData
    {
        u32 m_ModelIndex;           // model index of a streamed in model
        s32 m_TimeStreamedIn;       // time the model was streamed in
        s32 m_TimeNoLongerRequired; // time the model was marked available for streaming out
		u32 m_HDAssetSizeMain;		// main mem size of hd assets, if any are loaded
		u32 m_HDAssetSizeVram;		// vram mem size of hd assets, if any are loaded
#if DEBUG_POPULATION_MEMORY
		u32 m_mainUsed;
		u32 m_vramUsed;
#endif // DEBUG_POPULATION_MEMORY
		bool m_IsHD;				// keeps track if the current entry has its hd assets counted in the budget or not
		bool m_IsWeapon;
    };

    //PURPOSE
    // Used by GetAdjustedModelStartOffset() to determine whether to calculate an offset for
    // peds or vehicles
    enum ModelOffsetType
    {
        PED_MODEL_OFFSET,     // ped model offset
        VEHICLE_MODEL_OFFSET, // vehicle model offset
    };

    static const unsigned MAX_STREAMED_IN_MODELS = (128 + 32) * ASSET_MULTIPLIER;

	//PURPOSE
	// Get the total number of cars streamed in
	u32 GetTotalNumberOfCarsStreamedIn() const;

	//PURPOSE
	// Adds a car to the discarded list and removes it from the other lists for book keeping
	//PARAMS
	// modelIndex - The index of the model to add
	void AddDiscardedCar(u32 modelIndex);

	//PURPOSE
	// Re-instates a discarded car back into the correct lists
	//PARAMS
	// modelIndex - The index of the model to add
	void ReinstateDiscardedCar(u32 modelIndex);

	//PURPOSE
	// Adds a ped to the discarded list and removes it from the other lists for book keeping
	//PARAMS
	// modelIndex - The index of the model to add
	void AddDiscardedPed(u32 modelIndex);

	//PURPOSE
	// Re-instates a discarded ped back into the correct lists
	//PARAMS
	// modelIndex - The index of the model to add
	void ReinstateDiscardedPed(u32 modelIndex);

	//PURPOSE
	// Addes a streamed or re-instated model to the correct appropriate list
	//PARAMS
	// modelIndex - The index of the model to add
	bool AddPedModelToCorrectList( u32 modelIndex );

    //PURPOSE
    // Stores information about a newly streamed in model
    //PARAMS
    // modelIndex - The index of the model to add
    s32 AddStreamedInModelData(u32 modelIndex);

    //PURPOSE
    // Removes information about a newly streamed out model
    //PARAMS
    // modelIndex - The index of the model to remove
    bool RemoveStreamedInModelData(u32 modelIndex);

    //PURPOSE
    // Returns whether the specified model data is currently streamed in
    //PARAMS
    // modelIndex - The index of the model to check
    bool IsModelDataStreamedIn(u32 modelIndex);

	//PURPOSE
	// Stores information about a newly used component if it's not already in the list
	//PARAMS
	// modelIndex - The index of the parent model
	// componentList - The component list for this model type
	// totalAccum - total memory accumulator for this model type
	// totalVirtual - total new virtual memory used in the budget by this model
	// totalPhysical - total new physical memory used in the budget by this model
	void AddStreamedInComponent(u32 modelIndex, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type);
	void AddStreamedInDependencies(const atArray<strIndex>& deps, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type);

	//PURPOSE
	// Removes information about a newly discarded component. Subtracts memory from budget if refcount is 0
	//PARAMS
	// modelIndex - The index of the parent model
	// componentList - The component list for this model type
	// totalAccum - total memory accumulator for this model type
	// totalVirtual - total virtual memory that was removed from the budget by this model
	// totalPhysical - total physics memory that was removed from the budget by this model
	void RemoveStreamedInComponent(u32 modelIndex, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type);
	void RemoveStreamedInDependencies(const atArray<strIndex>& deps, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type);

    //PURPOSE
    // Returns the time a model (vehicle or ped) was streamed in
    //PARAMS
    // modelIndex - The index of the model to check
    s32 GetTimeModelStreamedIn(u32 modelIndex);


	//PURPOSE
	// Sets the time a model (vehicle or ped) was streamed in (for use to prolong scenario uses)
	//PARAMS
	// modelIndex - The index of the model to change
	void SetTimeModelStreamedIn(fwModelId PedModelId, s32 time);


    //PURPOSE
    // Marks a model (vehicle or ped) as no longer requires to be streamed in
    //PARAMS
    // modelIndex - The index of the model to set as no longer needed
    void MarkModelNoLongerRequired(u32 modelIndex);

    //PURPOSE
    // Returns whether the vehicle with the specified model index should be streamed in
    //PARAMS
    // modelIndex - The model index of the vehicle to check
    bool IsCarModelNeededCurrently(u32 modelIndex);

    //PURPOSE
    // Returns whether the ped with the specified model index should be streamed in
    //PARAMS
    // modelIndex - The model index of the ped to check
    bool IsPedModelNeededCurrently(u32 modelIndex);

    //PURPOSE
    // Returns whether the ped with the specified model index is needed in any zone
    //PARAMS
    // modelIndex - The model index of the ped to check
    bool IsPedModelNeededInAnyZone(u32 modelIndex);

    //PURPOSE
    // Streams a new vehicle model for use by the ambient population
    void StreamOneNewCar();

    //PURPOSE
    // Streams a new ped model for use by the ambient population
    void StreamOneNewPed(bool bNeedDriver);

    //PURPOSE
    // Counts the number of common vehicles we have in memory
    u32 GetNumCommonVehicles();
    
    //PURPOSE
    // Chooses a new vehicle model to stream in for use by the ambient population in network games
    u32 ChooseNewVehicleModelForMP();

    //PURPOSE
    // Chooses a new vehicle model to stream in for use by the ambient population in single player games
    u32 ChooseNewVehicleModelForSP();

    //PURPOSE
    // Chooses a new ped model to stream in for use by the ambient population in single player games
    u32 ChooseNewPedModelForSP(bool bNeedDriver);

    //PURPOSE
    // Chooses a new ped model to stream in for use by the ambient population in multiplayer games
    u32 ChooseNewPedModelForMP(bool bNeedDriver);

    //PURPOSE
    // Attempts to stream new models in for use by the ambient population,
    // if necessary
    void StreamModelsIn();

    //PURPOSE
    // Attempts to stream any models out that are no longer required.
    void StreamModelsOut();

	//PURPOSE
	// Finds the oldest model in memory and returns its id
	strLocalIndex FindOldestModelToStreamOut(bool isPed, bool canStreamCommonOut);

    //PURPOSE
    // Builds a list of candidate models to stream out in single player games
    //PARAMS
    // candidatesToRemove - list of models to stream out
    void GetCandidateVehiclesToStreamOutForSP(CLoadedModelGroup &candidatesToRemove, bool canStreamCommonVehicleOut);

    //PURPOSE
    // This function is called when a ped model has finished streaming in
    //PARAMS
    // modelIndex - The model index of the ped streamed in
    void PedHasBeenStreamedIn(u32 modelIndex);

    //PURPOSE
    // This function is called when a ped model has finished streaming out
    //PARAMS
    // modelIndex - The model index of the ped streamed out
    void PedHasBeenStreamedOut(u32 modelIndex);

    //PURPOSE
    // This function is called when a vehicle model has finished streaming in
    //PARAMS
    // modelIndex - The model index of the vehicle streamed in
    void VehicleHasBeenStreamedIn(u32 modelIndex);

    //PURPOSE
    // This function is called when a vehicle model has finished streaming out
    //PARAMS
    // modelIndex - The model index of the vehicle streamed out
    void VehicleHasBeenStreamedOut(u32 modelIndex);

    //PURPOSE
    // This function is called when a weapon model has finished streaming in
    //PARAMS
    // modelIndex - The model index of the weapon streamed in
    void WeaponHasBeenStreamedIn(u32 modelIndex);

    //PURPOSE
    // This function is called when a weapon model has finished streaming out
    //PARAMS
    // modelIndex - The model index of the weapon streamed out
    void WeaponHasBeenStreamedOut(u32 modelIndex);

    //PURPOSE
    // Updates the appropriate and inappropriate vehicle lists when the pop
    // cycle changes
    void ReclassifyLoadedCars();

    //PURPOSE
    // Updates the appropriate and inappropriate ped lists during
    // network games based on the time models have been streamed in
    void ReclassifyLoadedVehiclesForNetwork();

    //PURPOSE
    // Updates the appropriate and inappropriate ped lists when the pop
    // cycle changes
    void ReclassifyLoadedPeds();

    //PURPOSE
    // Updates the appropriate and inappropriate ped lists during
    // network games based on the time models have been streamed in
    void ReclassifyLoadedPedsForNetwork();

    //PURPOSE
    // Removes an unused models from the streaming system
    void RemoveUnusedModels();

    //PURPOSE
    // Keeps track of what HD assets are in memory for the vehicles and peds we have loaded
	void UpdateHDCosts();

    //PURPOSE
    // Removes all streamed in ped models when the system is shutting down
    void RemoveStreamedPedModelsOnShutdown();

    //PURPOSE
    // Removes all streamed in vehicle models when the system is shutting down
    void RemoveStreamedCarModelsOnShutdown();

    //PURPOSE
    // Ensures there is enough CVehicleStructures objects for new
    // vehicle models to be streamed in. If the number of free
    // CVehicleStructures drops below a threshold this function attempts to make space.
    void EmergencyRemoveVehicleStructures();

    //PURPOSE
    // Marks vehicle models that are not currently required for removal by the streaming system
    void TellStreamingAboutDeletableVehicles();

    //PURPOSE
    // Marks ped models that are not currently required for removal by the streaming system
    void TellStreamingAboutDeletablePeds();

    //PURPOSE
    // Chooses a vehicle from the specified car group to stream in
    //PARAMS
    // vehModelGroup - The car group to load the vehicle from
    u32 ChooseCarModelToLoad(s32* groupArray, s32 numGroups, u32 restrictions);

    //PURPOSE
    // Checks whether the specified vehicle model is incompatible with
    // the other currently loaded vehicles. This is used to prevent too
    // many similar or unusual vehicles being streamed in at the same time
    //PARAMS
    // modelIndex - The index of the model to check
    bool IsIncompatibleWithAlreadyLoadedCar(u32 modelIndex);

    //PURPOSE
    // Finds the best ped to stream in next fromt he lsit of peds in the current pop zone
    u32 ChoosePedModelToLoad(s32* groupArray, s32 numGroups, u32 restrictions);

	//PURPOSE
	// Returns true if the given ped model info can drive cars
	bool CanPedModelDrive(u32 mi);

	//PURPOSE
	// Returns true if the given ped model info can ride bikes
	bool CanPedModelRideBike(u32 mi);

    //PURPOSE
    // Updates the list of currently required models for the network game
    void UpdateModelsRequiredForNetwork();

    //PURPOSE
    // Updates the list of currently required vehicle models for the network game
    //PARAMS
    // modelStartOffset - Start offset into the list of models for the current pop zone for calculating the list of models to stream
    void UpdateVehicleModelsRequiredForNetwork(u32 modelStartOffset);

    //PURPOSE
    // Updates the list of currently required ped models for the network game
    //PARAMS
    // modelStartOffset - Start offset into the list of models for the current pop zone for calculating the list of models to stream
    void UpdatePedModelsRequiredForNetwork(u32 modelStartOffset);

    //PURPOSE
    // The models streamed in network games are taken from the population zones and are cycled based on the network time.
    // A new model can only be streamed when some of the old models have been streamed out. To ensure machines in the same network
    // session always stream the same vehicles we can only stream a new model when all of the nearby players are ready. This function
    // determines the allowable model start offset based on these rules (it will return the current model start offset if it is not possible
    // to stream a new model yet)
    //PARAMS
    // offsetType              - type of model offset to check - vehicles or peds
    // desiredModelStartOffset - the target model start offset (derived from network time)
    // popScheduleChanged      - indicates whether the pop schedule has changed since the model lists were last updated
    u32 GetAdjustedModelStartOffset(ModelOffsetType offsetType, u32 desiredModelStartOffset, bool popScheduleChanged);

#if __BANK
    //PURPOSE
    // Logs information about the models pending removal that are preventing the desired model start offset from being used.
    // This is generally because instances of these models are nearby the local player and cannot be cleaned up
    //PARAMS
    // offsetType               - Indicates whether these models are for peds or vehicles
    // desiredModelStartOffset  - This is the offset into the models list that we want to use based off the current network time interval
    // adjustedModelStartOffset - This is the offset into the models list adjusted to take into account of old models pending removal
    void LogModelsPendingRemovalInfo(ModelOffsetType offsetType, u32 desiredModelStartOffset, u32 adjustedModelStartOffset);
#endif // __BANK

    //PURPOSE
    // Returns the current network time interval to use to index into the model lists
    u32 GetCurrentNetworkTimeInterval();

    //PURPOSE
    // Displays information about the different car groups currently streamed in
    // in the single player game
    void PrintCarGroups();

    //PURPOSE
    // Displays information about the different car groups currently streamed in
    // in the network game
    void PrintNetworkCarGroups();

	//PURPOSE
	// Returns if we have any free memory left for peds/vehicles
	bool IsThereAnyMemoryLeft(eMemoryTrackType type);

	//PURPOSE
	// Returns if we use more memory than specified in the budget for peds or vehicles
	bool AreWeOverBudget(eMemoryTrackType type);

	//PURPOSE
	// Add/remove memory used by peds/vehicles
	void AddTotalMemoryUsed(s32 main, s32 vram, eMemoryTrackType type);

	//PURPOSE
	// Returns whether we are allowed to stream in one more ped for scenarios or not
	//PARAMS
	// highPri - True if this is for a high priority scenario
	// startupMode - True if we are in the population system's startup mode, i.e. the screen is faded or some similar situation.
	// eSlotNumber - the scenario streaming slot the model is using
	bool IsAllowedToRequestScenarioPedModel(bool highPri, bool startupMode, eScenarioPopStreamingSlot eSlotNumber) const;


    // storage for data about currently streamed in models
    StreamedInModelData m_StreamedInModelData[MAX_STREAMED_IN_MODELS];

    CLoadedModelGroup   m_AppropriateLoadedCars;           // Currently streamed in vehicles that can be used in the current zone
    CLoadedModelGroup   m_InAppropriateLoadedCars;         // Currently streamed in vehicles that cannot be used in the current zone
    CLoadedModelGroup   m_SpecialLoadedCars;               // Currently streamed in vehicles outside the ambient population (police cars, taxis etc...)
    CLoadedModelGroup   m_LoadedBoats;                     // Currently streamed in boats
    CLoadedModelGroup   m_VehicleModelsRequiredForNetwork; // Currently required vehicle models for the network game
	CLoadedModelGroup	m_DiscardedCars;				   // Car types that have been discarded from the population but are still in memory
    bool                m_bBoatsNeeded;                    // Do we have to have some boats streamed in here?

    CLoadedModelGroup   m_AppropriateLoadedPeds;          // Currently streamed in peds that can be used in the current zone
    CLoadedModelGroup   m_InAppropriateLoadedPeds;        // Currently streamed in peds that cannot be used in the current zone
    CLoadedModelGroup   m_SpecialLoadedPeds;              // Currently streamed in peds that outside the ambient population (cops, firemen etc...)
    CLoadedModelGroup   m_PedModelsRequiredForNetwork;    // Currently required ped models for the network game
    CLoadedModelGroup	m_DiscardedPeds;				  // ped types discarded by population code but still in memory

	s32					m_MaxScenarioPedModelsLoadedPerSlot;     // How many ped models we are currently allowed to request for scenarios per streaming slot.
	s32					m_MaxScenarioPedModelsLoadedOverride; // Override of m_MaxScenarioPedModelsLoaded set by script, or -1
    s32                 m_TotalMemoryUsedByPedModelsMain; // The total main streaming memory currently in use by ped models
    s32                 m_TotalMemoryUsedByPedModelsVram; // The total vram streaming memory currently in use by ped models
    s32                 m_TotalMemoryUsedByVehicleModelsMain; // The total main streaming memory currently in use by vehicle models
    s32                 m_TotalMemoryUsedByVehicleModelsVram; // The total vram streaming memory currently in use by vehicle models
	s32					m_MemOverBudgetMainPeds;
	s32					m_MemOverBudgetVramPeds;
	s32					m_MemOverBudgetMainVehs;
	s32					m_MemOverBudgetVramVehs;
	s32					m_TotalStreamedPedMemoryMain;
	s32					m_TotalStreamedPedMemoryVram;
	s32					m_TotalStreamedVehicleMemoryMain;
	s32					m_TotalStreamedVehicleMemoryVram;
	s32					m_TotalWeaponMemoryMain;
	s32					m_TotalWeaponMemoryVram;
    bool                m_bReducePedModelBudget;          // Indicates whether the streaming budget for peds should be reduced
    bool                m_bReduceVehicleModelBudget;      // Indicates whether the streaming budget for vehicles should be reduced

    u32                 m_NetworkModelVariationID;             // Used to increase variety in the order models are streamed in, while remaining consistent across machines
    u32                 m_LastTimeUsedForStreamingNetPeds;     // The last network time in seconds used to decide which network ped models to stream
    u32                 m_LastTimeUsedForStreamingNetVehicles; // The last network time in seconds used to decide which network vehicle models to stream
    s32                 m_LastZoneUsedForStreamingNetModels;   // The last pop zone used to decide which network models to stream
    u32                 m_LocalAllowedPedModelStartOffset;     // The ped model start offset the local machine is able to move to (based on network time and how many models are pending streaming out)
    u32                 m_LocalAllowedVehicleModelStartOffset; // The ped model start offset the local machine is able to move to (based on network time and how many models are pending streaming out)

    u32                 m_LastVehicleOnHighwaySpawn;

	u32					m_ScenarioPedStreamingDisabledUntilTimeMS;	// Time stamp (compatible with fwTimer::GetTimeInMilliseconds()) for when we are allowed to stream in new scenario models again.

	atArray<sPopEntry> m_vehicleComponents;
	atArray<sPopEntry> m_pedComponents;
	atArray<sPopEntry> m_weaponComponents;
	atArray<strIndex> m_residentObjects;
	atArray<strIndex> m_residentObjectsToRemove;
	atRangeArray<s16, NUM_SCENARIO_POP_SLOTS> m_aNumScenarioPedModelsLoaded;     // How many ped models are streamed in (or have been requested) for scenarioss

	s8					m_numQueuedVehicleModels;

	static s32 sm_TotalMemUsedByPeds;
	static s32 sm_TotalMemUsedByVehicles;

#if __BANK
	u32 m_vehicleOverrideIndex;
#endif
};

// wrapper class needed to interface with game skeleton code
class CPopulationStreamingWrapper
{
public:

    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void Update();
};

extern CPopulationStreaming gPopStreaming;

#endif // STREAMING_POPULATIONSTREAMING_H

