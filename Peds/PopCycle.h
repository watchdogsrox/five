/////////////////////////////////////////////////////////////////////////////////
// Title	:	PopCycle.h
// Author	:	Obbe, Adam Croston
// Started	:	5/12/03
// Purpose	:	To keep track of the current population disposition.
/////////////////////////////////////////////////////////////////////////////////
#ifndef POPCYCLE_H__
#define POPCYCLE_H__

// Rage headers
#include "atl/array.h"		// For atArray
#include "atl/string.h"		// For atString
#include "atl/bitset.h"		// For atFixedBitset
#include "atl/hashstring.h"		// For atString
#include "parser/macros.h"

// Framework Headers
#include "fwmaths/random.h"

// Game headers
#include "debug/debug.h"
#include "game/Dispatch/DispatchData.h"
#include "Peds/pedtype.h"
#include "Peds/PopZones.h"

class CPopZone;
class CAmbientModelVariations;

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopGroup
// Purpose		:	To store a limited list of peds and vehicles.
/////////////////////////////////////////////////////////////////////////////////
enum ePopGroupFlags {
	POPGROUP_IS_GANG        = BIT(0),		// popgroup is a gang population group
	POPGROUP_AMBIENT        = BIT(1),		// popgroup is used by ambient population generation
	POPGROUP_SCENARIO       = BIT(2),		// popgroup is used by scenarios
	POPGROUP_RARE           = BIT(3),		// popgroup contains rare peds/vehicles
	POPGROUP_NETWORK_COMMON = BIT(4),		// popgroup contains models to streamed regardless of the zone the player is in
	POPGROUP_AERIAL					= BIT(5),		// popgroup should spawn in the air
	POPGROUP_AQUATIC				= BIT(6),		// popgroup should spawn in the water
	POPGROUP_WILDLIFE				= BIT(7),		// popgroup should spawn on sand/grass
	POPGROUP_IN_VEHICLE				= BIT(8),		// popgroup spawns only in vehicles
};

// Struct that contains information about how a ped should be spawned.
// Most human peds set all three of the special conditions to false.
struct CPopCycleConditions
{
	CPopCycleConditions() : m_bSpawnInWater(false), m_bSpawnInAir(false), m_bSpawnAsWildlife(false) {}

	bool m_bSpawnInWater;
	bool m_bSpawnInAir;
	bool m_bSpawnAsWildlife;
};

struct CPopModelAndVariations
{
	atHashWithStringNotFinal m_Name;
	CAmbientModelVariations* m_Variations;
	
	CPopModelAndVariations() : m_Variations(NULL) {}
	~CPopModelAndVariations();

	CPopModelAndVariations(const CPopModelAndVariations& rhs) { CopyTo(rhs); }
	CPopModelAndVariations& operator = (const CPopModelAndVariations& rhs) { return CopyTo(rhs); }

	PAR_SIMPLE_PARSABLE;

private:

	CPopModelAndVariations& CopyTo(const CPopModelAndVariations& rhs);
};

class CPopulationGroup
{
	friend class CPopGroupList;
public:
	CPopulationGroup();
	void Reset();

	atHashWithStringNotFinal GetName() const {return m_Name;}
	u32 GetCount () const {return m_models.GetCount();}
	u32 GetModelIndex (u32 index) const;
	u32 GetModelHash (u32 index) const {return m_models[index].m_Name;}
	atHashWithStringNotFinal GetModelName (u32 index) const {return m_models[index].m_Name;}
	bool IsIndexMember (u32 modelIndex) const;

	CAmbientModelVariations* FindVariations(u32 modelIndex) const;

	bool IsFlagSet(u32 flag) const {return (m_flags & flag) != 0; }
	u32  GetFlags() const {return m_flags;}

	bool IsEmpty() const { return m_empty; }
	void SetIsEmpty(bool val) { m_empty = val; }

	bool operator == (const CPopulationGroup& rhs) const { return m_Name == rhs.m_Name; }

private:
	atHashWithStringNotFinal m_Name;
	atArray<CPopModelAndVariations> m_models;
	u32 m_flags;
	bool m_empty;

	PAR_SIMPLE_PARSABLE;
};

class CPopGroupList
{
public:
	CPopGroupList();
	void Reset();
	void Flush();

	void LoadPopGroupDataFile(const char* filename, bool bPeds);
	void LoadPatch(const char* file);
	void UnloadPatch(const char* file);

	//int GetPopCount() const {return m_popGroups.GetCount();}
	int GetPedCount() const {return m_pedGroups.GetCount();}
	int GetVehCount() const {return m_vehGroups.GetCount();}

	//const CPopGroup& GetPopGroup(u32 popGroupIndex) const {return m_popGroups[popGroupIndex];}
	const CPopulationGroup& GetPedGroup(u32 popGroupIndex) const {return m_pedGroups[popGroupIndex];}
	const CPopulationGroup& GetVehGroup(u32 popGroupIndex) const {return m_vehGroups[popGroupIndex];}

	bool FindPedGroupFromNameHash (u32 popGroupNameHash, u32& outPopGroupIndex) const;
	bool FindVehGroupFromNameHash (u32 popGroupNameHash, u32& outPopGroupIndex) const;

	bool IsPedIndexMember (u32 popGroupIndex, u32 pedModelIndex) const {return m_pedGroups[popGroupIndex].IsIndexMember(pedModelIndex);}
	u32 GetFlags (u32 popGroupIndex) const {return m_pedGroups[popGroupIndex].GetFlags();}
	u32 GetNumPeds (u32 popGroupIndex) const {return m_pedGroups[popGroupIndex].GetCount();}
	u32 GetPedIndex (u32 popGroupIndex, u32 pedWithinGroup) const {return m_pedGroups[popGroupIndex].GetModelIndex(pedWithinGroup);}
	u32 GetPedHash (u32 popGroupIndex, u32 pedWithinGroup) const {return m_pedGroups[popGroupIndex].GetModelHash(pedWithinGroup);}

	bool IsVehIndexMember (u32 popGroupIndex, u32 vehModelIndex) const {return popGroupIndex != ~0U ? m_vehGroups[popGroupIndex].IsIndexMember(vehModelIndex) : false;}
	u32 GetNumVehs (u32 popGroupIndex) const {return m_vehGroups[popGroupIndex].GetCount();}
	u32 GetVehIndex (u32 popGroupIndex, u32 vehWithinGroup) const {return m_vehGroups[popGroupIndex].GetModelIndex(vehWithinGroup);}
	u32 GetVehHash (u32 popGroupIndex, u32 vehWithinGroup) const {return m_vehGroups[popGroupIndex].GetModelHash(vehWithinGroup);}

	// Checks if the material is in the list of suitable materials inside of popgroups.meta
	bool IsMaterialSuitableForWildlife(const atHashString& sMaterial) const { return m_wildlifeHabitats.Find(sMaterial) != -1; }

	void PostLoad();
private:
	//atArray<CPopGroup> m_popGroups;
	atArray<CPopulationGroup>	m_pedGroups;
	atArray<CPopulationGroup>	m_vehGroups;
	atArray<atHashString>		m_wildlifeHabitats;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopAllocation
// Purpose		:	To store a description of a current population disposition.
//					Namely, their counts and percentages.
/////////////////////////////////////////////////////////////////////////////////
struct CPopGroupPercentage
{
	CPopGroupPercentage() : m_GroupName(atHashWithStringNotFinal::Null()), m_percentage(0) {}
	CPopGroupPercentage(const atHashWithStringNotFinal& name, u8 percentage) : m_GroupName(name), m_percentage(percentage) {}

	atHashWithStringNotFinal m_GroupName;
	u32 m_percentage;

	PAR_SIMPLE_PARSABLE;
};

class CPopAllocation
{
	friend class CPopScheduleList;
public:
	void Reset();
	void NormalizePercentages();
	atHashWithStringNotFinal PickRandomPedGroup() const {return PickRandomGroup(m_pedGroupPercentages);}
	atHashWithStringNotFinal PickRandomVehGroup() const {return PickRandomGroup(m_vehGroupPercentages);}

#if __BANK
	u32 GetMaxNumAmbientPeds() const {return (sm_forceMaxPeds ? sm_forceMaxPeds : (u32)(m_nMaxNumAmbientPeds*sm_maxPedsMultiplier));}
	u32 GetPercentageOfMaxCars() const {return (sm_forceMaxCars ? sm_forceMaxCars : (u32)(m_nPercentageOfMaxCars*sm_maxCarsMultiplier));}
#else
	u32 GetMaxNumAmbientPeds() const {return m_nMaxNumAmbientPeds;}
	u32 GetPercentageOfMaxCars() const {return m_nPercentageOfMaxCars;}
#endif // __BANK
	u32 GetMaxNumScenarioPeds() const {return m_nMaxNumScenarioPeds;}
	u32 GetMaxNumParkedCars() const {return m_nMaxNumParkedCars;}
	u32 GetMaxNumLowPrioParkedCars() const {return m_nMaxNumLowPrioParkedCars;}
	u32 GetPercentageCopsCars() const {return m_nPercCopsCars;}
	u32 GetPercentageCopsPeds() const {return m_nPercCopsPeds;}
	u32 GetMaxScenarioPedModels() const {return m_nMaxScenarioPedModels;}
	u32 GetMaxScenarioVehicleModels() const {return m_nMaxScenarioVehicleModels;}
	u32 GetMaxPreassignedParkedCars() const {return m_nMaxPreassignedParkedCars;}
	const CPopGroupPercentage& GetPedGroupPercentage(int group) const {return m_pedGroupPercentages[group];}
	u32 GetNumberOfPedGroups() const {return m_pedGroupPercentages.GetCount();}
	const CPopGroupPercentage& GetVehGroupPercentage(int group) const {return m_vehGroupPercentages[group];}
	u32 GetNumberOfVehGroups() const {return m_vehGroupPercentages.GetCount();}
#if __BANK
	static void SetMaxPedsMultiplier(float multi) {sm_maxPedsMultiplier = multi;}
	static void SetMaxCarsMultiplier(float multi) {sm_maxCarsMultiplier = multi;}
	static void SetForceMaxPeds(u32 max) {sm_forceMaxPeds = (u8)max;}
	static void SetForceMaxCars(u32 max) {sm_forceMaxCars = (u8)max;}
#endif

private:
	void NormalizePercentages(atArray<CPopGroupPercentage>& array);
	atHashWithStringNotFinal PickRandomGroup(const atArray<CPopGroupPercentage>& array) const;

	u8			m_nMaxNumAmbientPeds;
	u8			m_nMaxNumScenarioPeds;
	u8			m_nPercentageOfMaxCars;
	u8			m_nMaxNumParkedCars;
	u8			m_nMaxNumLowPrioParkedCars;
	u8			m_nPercCopsCars;
	u8			m_nPercCopsPeds;
	u8			m_nMaxScenarioPedModels;
	u8			m_nMaxScenarioVehicleModels;
	u8			m_nMaxPreassignedParkedCars;
	atArray<CPopGroupPercentage>	m_pedGroupPercentages;
	atArray<CPopGroupPercentage>	m_vehGroupPercentages;

#if __BANK
	static float sm_maxPedsMultiplier;
	static float sm_maxCarsMultiplier;
	static u8 sm_forceMaxPeds;
	static u8 sm_forceMaxCars;
#endif

	PAR_SIMPLE_PARSABLE;
};


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopSchedule
// Purpose		:	To store a series of a population dispositions over time.
/////////////////////////////////////////////////////////////////////////////////
class CPopSchedule
{
	friend class CPopScheduleList;
public:
	// The number of samples during the day (once every 2 hours->12)
	enum{	POPCYCLE_DAYSUBDIVISIONS  = 12};

	// The weekday types.
	enum{	POPCYCLE_WEEKDAY = 0,
		POPCYCLE_WEEKSUBDIVISIONS};


	CPopSchedule();
	void Reset(void);
	atHashWithStringNotFinal GetName() const {return m_Name;}
	CPopAllocation& GetAllocation(int weekSubdivision, int daySubdivision)
	{
		Assert(weekSubdivision < POPCYCLE_WEEKSUBDIVISIONS);
		return m_popAllocation[(weekSubdivision*POPCYCLE_DAYSUBDIVISIONS)+daySubdivision];
	}
	const CPopAllocation& GetAllocation(int weekSubdivision, int daySubdivision) const
	{
		Assert(weekSubdivision < POPCYCLE_WEEKSUBDIVISIONS);
		return m_popAllocation[(weekSubdivision*POPCYCLE_DAYSUBDIVISIONS)+daySubdivision];
	}

	void SetOverridePedGroup(u32 overridePedGroup, u32 overridePercentage);
	u32 GetOverridePedGroup() const {return m_OverridePedGroup;}
	u32 GetOverridePedPercentage() const {return m_overridePedPercentage;}

	void SetOverrideVehGroup(u32 overrideVehGroup, u32 overridePercentage);
	u32 GetOverrideVehGroup() const {return m_OverrideVehGroup;}
	u32 GetOverrideVehPercentage() const {return m_overrideVehPercentage;}

	void SetOverrideVehicleModel(u32 modelIndex) { m_overrideVehModel = modelIndex; }
	u32 GetOverrideVehicleModel() const { return m_overrideVehModel.Get(); }

private:
	atHashWithStringNotFinal m_Name;
	// The allotments according to each day time and daytime type.
	CPopAllocation	m_popAllocation[POPCYCLE_WEEKSUBDIVISIONS*POPCYCLE_DAYSUBDIVISIONS];

	// override values that can be set by script
	u32 m_OverridePedGroup;
	u32 m_overridePedPercentage;
	u32 m_OverrideVehGroup;
	u32 m_overrideVehPercentage;
	strLocalIndex m_overrideVehModel;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopScheduleList
// Purpose		:	Container object for all population schedules
/////////////////////////////////////////////////////////////////////////////////
class CPopScheduleList
{
public:
	enum {INVALID_SCHEDULE = -1};

	CPopScheduleList();
	void Reset();

	// Raw pop schedule data.
#if !__FINAL
	const char* GetNameFromIndex (s32 popScheduleIndex) const;
	void Save(const char* filename);
#endif // !__FINAL
	bool GetIndexFromNameHash (u32 popScheduleNameHash, s32& outPopScheduleIndex) const;
	void Load(const char* fileName);
	void PostLoad();

	int GetCount() const {return m_schedules.GetCount();}
	const CPopSchedule& GetSchedule(int schedule) const { Assert(schedule != INVALID_SCHEDULE); return m_schedules[schedule];}
	CPopSchedule& GetSchedule(int schedule) { Assert(schedule != INVALID_SCHEDULE); return m_schedules[schedule];}

private:

	atArray<CPopSchedule> m_schedules;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZoneData
// Purpose		:	Helper class to serialize pop zone data
/////////////////////////////////////////////////////////////////////////////////
class CPopZoneData
{
public:

	void OnPostSetCallback(parTreeNode* pTreeNode, parMember* pMember, void* pInstance);

	struct sZone
	{
		ConstString zoneName;

		atHashString spName;
		atHashString mpName;
		atHashString vfxRegion;

		u8 plantsMgrTxdIdx;

		eZoneScumminess scumminessLevel;
		eLawResponseTime lawResponseTime; // LAW_RESPONSE_DELAY_MEDIUM
		eZoneVehicleResponseType lawResponseType; // VEHICLE_RESPONSE_DEFAULT
		eZoneSpecialAttributeType specialZoneAttribute; // SPECIAL_NONE

		float vehDirtMin;
		float vehDirtMax;
		float vehDirtGrowScale;

		float pedDirtMin;
		float pedDirtMax;

		float popRangeScaleStart;
		float popRangeScaleEnd;
		float popRangeMultiplier;
		float popBaseRangeScale;

		s32 mpGangTerritoryIndex; // -1

		u8 dirtRed;
		u8 dirtGreen;
		u8 dirtBlue;

		bool allowScenarioWeatherExits; // true
		bool allowHurryAwayToLeavePavement; // false

		PAR_SIMPLE_PARSABLE;
	};

	atArray<sZone> zones;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopCycle
// Purpose		:	To keep track of the current population disposition, based
//					on CPopSchedules and and CPopGroups.
/////////////////////////////////////////////////////////////////////////////////
class CLoadedModelGroup;

struct sGroupPercentage
{
	s32 groupIndex;
	float deviation;
    float requestedPercentage;
    s32 randNum;
	bool zeroInstances : 1;
};

struct sAvailableModel
{
	u32 index;
	float distanceSquared;
};

class CPopCycle
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Display();

	static void LoadPopGroupsFile(const char* file);
	static void UnloadPopGroupsFile(const char* file);

	static void LoadPopGroupPatch(const char* file);
	static void UnloadPopGroupPatch(const char* file);

	static void LoadZoneBindFile(const char* file);
	static void UnloadZoneBindFile(const char* file);

	static void LoadPopSchedFile(const char* file);
	static void UnloadPopSchedFile(const char* file);

#if __BANK
	static void InitWidgets();	
#endif

	static s32 GetCurrentMaxNumScenarioPeds() { return sm_currentMaxNumScenarioPeds;}
	static s32 GetCurrentMaxNumAmbientPeds() { return sm_currentMaxNumAmbientPeds;}
	static s32 GetCurrentMaxNumCopPeds() { return sm_currentMaxNumCopPeds;}

	static float GetCurrentPercentageOfMaxCars();
	static s32 GetCurrentMaxNumParkedCars();
	static s32 GetCurrentMaxNumLowPrioParkedCars();
	static s32 GetCurrentDesiredPercentageOfCopsCars();
	static s32 GetCurrentDesiredPercentageOfCopsPeds();

	static float CalcWeatherMultiplier();
	static float CalcHighWayPedMultiplier();
	static float CalcHighWayCarMultiplier();
	static float GetInteriorPedMultiplier();
	static float GetInteriorCarMultiplier();
	static float GetWantedPedMultiplier();
	static float GetWantedCarMultiplier();
	static float GetCombatPedMultiplier();
	static float GetCombatCarMultiplier();

	static float ScaleMultiplier_SP_to_MP(const float fMult);

#if __BANK
	static void GetAmbientPedMultipliers(float * fWeatherMultiplier, float * fHighwayMultiplier, float * fInteriorMultiplier, float * fWantedMultiplier, float * fCombatMultiplier, float * fPedDensityMultiplier);
	static void GetScenarioPedMultipliers(float * fHighwayMultiplier, float * fInteriorMultiplier, float * fWantedMultiplier, float * fCombatMultiplier, float * fPedDensityMultiplier);
#endif

	static s32 GetTemporaryMaxNumScenarioPeds();
	static s32 GetTemporaryMaxNumAmbientPeds();
	static s32 GetTemporaryMaxNumCars();

	static void GetNetworkPedModelsForCurrentZone(CLoadedModelGroup &ambientModelsList, CLoadedModelGroup &gangModelsList);
	static void GetNetworkVehicleModelsForCurrentZone(CLoadedModelGroup &commonModelsList, CLoadedModelGroup &zoneSpecificModelsList);
	static s32 PickMostWantingGroupOfModels(CLoadedModelGroup &ExistingGroup, bool bPeds, s32 **ppGroupList);
	static bool FindNewPedModelIndex(u32 *pNewPedModelIndex, bool bNoCops, bool forceCivPedCreation, u32 requiredPopGroupFlags, CPopCycleConditions& conditions, const CAmbientModelVariations*& pVariationsOut, const Vector3* pos);
	static bool FindNewCarModelIndex(u32* pNewCarModelIndex, CLoadedModelGroup& availableCars, const Vector3* pos = NULL, bool cargens = false, bool bRandomizedSelection=false);
	static bool AreTaxisRequired();

	static const CPopGroupList& GetPopGroups() {return sm_popGroups;}
	static CPopScheduleList& GetPopSchedules() {return sm_popSchedules;}
	static bool HasValidCurrentPopAllocation();
	static const CPopSchedule& GetCurrentPopSchedule();
	static const CPopAllocation& GetCurrentPopAllocation();
	static u8 GetCurrentPedGroupPercentage(int popGroupIndex) {return sm_currentPedPercentages[popGroupIndex];}
	static u8 GetCurrentVehGroupPercentage(int popGroupIndex) {return sm_currentVehPercentages[popGroupIndex];}
	static bool VerifyHasValidCurrentPopAllocation();

	static float   GetCurrentZoneVehDirtMin();
	static float   GetCurrentZoneVehDirtMax();
	static float   GetCurrentZonePedDirtMin();
	static float   GetCurrentZonePedDirtMax();
	static float   GetCurrentZoneDirtGrowScale();
	static Color32 GetCurrentZoneDirtCol();

	static bool IsCurrentZoneAirport();

	static atHashWithStringNotFinal GetCurrentZoneVfxRegionHashName();
	static u8 GetCurrentZonePlantsMgrTxdIdx();

	static int GetCurrentZoneVehicleResponseType() { return sm_nCurrentZoneVehicleResponseType; }
	static s32 GetCurrentZoneIndex() {return sm_nCurrentZonePopScheduleIndex;}
	static void SetPopulationChanged() {sm_populationChanged = true;}

	static s32 GetPreferredGroupForVehs() { return sm_preferredSpawnGroupForVehs; }
	static void SetPreferredGroupForVehs(s32 group) { sm_preferredSpawnGroupForVehs = group; }

	static bool IsMaterialSuitableForWildlife(const atHashString& sMaterial) { return sm_popGroups.IsMaterialSuitableForWildlife(sMaterial); }

	static u32 GetCommonVehicleGroup() { return sm_commonVehicleGroup; }

	static u32 GetMaxCargenModels() { return sm_pCurrPopAllocation ? sm_pCurrPopAllocation->GetMaxPreassignedParkedCars() : 1; }
	static int GetMaxScenarioVehicleModels() { return sm_pCurrPopAllocation ? sm_pCurrPopAllocation->GetMaxScenarioVehicleModels() : 2; }

	static bool VehicleBikeGroupActive();

	static void ApplyZoneBindData(const CPopZoneData* data);

	static CPopZone *GetCurrentActiveZone();

#if !__FINAL
	static bool sm_bDisplayDebugInfo;
	static bool sm_bDisplayScenarioDebugInfo;
	static bool sm_bDisplaySimpleDebugInfo;
	static bool sm_bDisplayInVehDeadDebugInfo;
	static bool sm_bDisplayInVehNoPretendModelDebugInfo;
	static bool sm_bDisplayCarDebugInfo;
	static bool sm_bEnableVehPopDebugOutput;
	static bool sm_bDisplayCarSimpleDebugInfo;
	static bool sm_bDisplayCarPopulationFailureCounts;
	static bool sm_bDisplayPedPopulationFailureCounts;
	static bool sm_bDisplayWanderingPedPopulationFailureCountsOnVectorMap;
	static bool sm_bDisplayScenarioPedPopulationFailureCountsOnVectorMap;
	static bool sm_bDisplayScenarioVehiclePopulationFailureCountsOnVectorMap;
	static bool sm_bDisplayPedPopulationFailureCountsDebugSpew;
	static bool sm_bDisplayScenarioPedPopulationFailuresInTheWorld;
	static atFinalHashString sm_nCurrentZoneName;
#endif // !__FINAL

protected:

	static void ResetPopGroupsAndSchedules					(void);
	static void RefreshPopGroupData();

	static void LoadPopGroups								(void);
	static void LoadPopSchedules							(void);
	//static void LoadPopSchedulesDataFile					(const char* fileName);
	static void LoadPopZoneToPopScheduleBindings();
#if __BANK
	static void ReloadPopZoneToPopScheduleBindings();
#endif
	static void LoadPopZoneToPopScheduleBindingsDataFile_PSO	(const char* fileName);
	static void LoadPopZoneToPopScheduleBindingsDataFile_META	(const char* fileName);
#if 0
	static void LoadPopZoneToPopScheduleBindingsDataFile2	(const char* fileName);
#endif

	static void UpdateCurrZone								(void);
	static void UpdateCurrZoneFromCoors						(const Vector3 &playerPos);
	static void UpdatePercentages							(void);

	static u32 CountNumPedsInCombat();

	static bool sm_populationChanged;
	static s32 sm_nCurrentZonePopScheduleIndex;
	static int sm_nCurrentZoneVehicleResponseType;
	static s32 sm_nCurrentDaySubdivision;
	static s32 sm_nCurrentWeekSubdivision;

	static CPopZone* sm_pCurrZone;
	static const CPopAllocation* sm_pCurrPopAllocation;
	static const CPopSchedule* sm_pCurrPopSchedule;
	static atArray<u8> sm_currentPedPercentages;
	static atArray<u8> sm_currentVehPercentages;

	static s32 sm_currentMaxNumCopPeds, sm_currentMaxNumAmbientPeds, sm_currentMaxNumScenarioPeds;
	static u32 sm_popSchedulesMaxNumCars;

	static u32 sm_currentNumPedsInCombat;

	static bank_bool sm_bUseCombatMultiplier;
	static bank_u32 sm_iMinPedsInCombatForPopMultiplier;
	static bank_u32 sm_iMaxPedsInCombatForPopMultiplier;
	static bank_float sm_fPedsInCombatMinPopMultiplier;

	static bank_float sm_fMinWantedMultiplierMP;

	static CPopGroupList sm_popGroups;
	static atArray<atHashString> sm_popGroupPatches;
	static CPopScheduleList sm_popSchedules;

	// Number of times to use the cached population group for FindNewPedIndex
	static u8 sm_PopulationRecalculateCountdown;
	static u64 sm_CurrentPopZoneId;
	static u32 sm_CachedMostWantedPopGroup;

	static s32 sm_preferredSpawnGroupForVehs;

	static float sm_distanceDeltaForUpdateSqr;
	static u32 sm_nextTimeForUpdate;
	static Vector3 sm_lastUpdatePos;
	static u32 sm_commonVehicleGroup;
	static u32 sm_vehicleBikeGroup;
	static u32 sm_lastTimePedsInCombatCounted;
#if __BANK
	static bool sm_enableOverridePedDirtScale;
	static float sm_overridePedDirtScale;
	static bool sm_enableOverrideVehDirtScale;
	static float sm_overridevehDirtScale;
	static bool sm_enableOverrideDirtColor;
	static Color32 sm_overrideDirtColor;
#endif	
};


inline const CPopAllocation& CPopCycle::GetCurrentPopAllocation()
{
	Assertf(HasValidCurrentPopAllocation(), "Current pop schedule index  at position %f,%f,%f is invalid", sm_lastUpdatePos.x, sm_lastUpdatePos.y, sm_lastUpdatePos.z);
	return *sm_pCurrPopAllocation;//
}

inline const CPopSchedule& CPopCycle::GetCurrentPopSchedule()
{
	Assertf(HasValidCurrentPopAllocation(), "Current pop schedule index  at position %f,%f,%f is invalid", sm_lastUpdatePos.x, sm_lastUpdatePos.y, sm_lastUpdatePos.z);
	return *sm_pCurrPopSchedule;//
}

inline bool CPopCycle::HasValidCurrentPopAllocation()
{
	return (sm_pCurrPopAllocation != NULL);
}

inline bool CPopCycle::VerifyHasValidCurrentPopAllocation()
{
	return Verifyf(HasValidCurrentPopAllocation(), "Current pop schedule index  at position %f,%f,%f is invalid", sm_lastUpdatePos.x, sm_lastUpdatePos.y, sm_lastUpdatePos.z);
}


#endif // POPCYCLE_H__
