//
// streaming/populationstreaming.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

#include "populationstreaming.h"

#include "bank/bank.h"

#include "grcore/debugdraw.h"

#include "vfx/ptfx/ptfxmanager.h"

#include "ai/ambient/AmbientModelSetManager.h"
#include "camera/caminterface.h"
#include "core/game.h"
#include "core/gamesessionstatemachine.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "game/clock.h"
#include "game/Dispatch/DispatchServices.h"
#include "game/modelindices.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "game/zones.h"
#include "fwmaths/random.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "network/debug/networkdebug.h"
#include "network/networkinterface.h"
#include "objects/objectpopulation.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"
#include "peds/pedpopulation.h"
#include "peds/popcycle.h"
#include "peds/popzones.h"
#include "peds/rendering/PedVariationStream.h"
#include "performance/clearinghouse.h"
#include "scene/world/gameworld.h"
#include "scene/texlod.h"
#include "script/script_cars_and_peds.h"
#include "streaming/defragmentation.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "system/memory.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Scenario/ScenarioClustering.h"
#include "vehicleai/pathfind.h"
#include "vehicles/cargen.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/vehiclepopulation.h"

AI_OPTIMISATIONS()

#define MERGED_BUDGET (ONE_STREAMING_HEAP || RSG_DURANGO || RSG_ORBIS)

CPopulationStreaming gPopStreaming;

const u32 DEFAULT_MEM_FOR_VEHICLES_LEVEL = MAX_MEM_LEVELS - 1;
static u32 s_MemForVehiclesLevel				= DEFAULT_MEM_FOR_VEHICLES_LEVEL;

const u32 DEFAULT_MEM_FOR_PEDS_LEVEL = MAX_MEM_LEVELS - 1;
static u32 s_MemForPedsLevel					= DEFAULT_MEM_FOR_PEDS_LEVEL;

#define BUFFER_MEM_FOR_PED_MODELS				(1500000)
#define BUFFER_MEM_FOR_VEHICLE_MODELS			(1500000)

// SINGLEPLAYER
static u32 s_MemForPeds_SP[]						= { (0 + BUFFER_MEM_FOR_PED_MODELS),
														(27000000 + BUFFER_MEM_FOR_PED_MODELS),
														(36000000 + BUFFER_MEM_FOR_PED_MODELS),
														(46800000 + BUFFER_MEM_FOR_PED_MODELS) };

static u32 s_MemForVehicles_SP[]					= { (0 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(16000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(28000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(35000000 + BUFFER_MEM_FOR_VEHICLE_MODELS) };
// MULTIPLAYER
static u32 s_MemForPeds_MP[]						= { (0 + BUFFER_MEM_FOR_PED_MODELS),
														(27000000 + BUFFER_MEM_FOR_PED_MODELS),
														(36000000 + BUFFER_MEM_FOR_PED_MODELS),
														(46800000 + BUFFER_MEM_FOR_PED_MODELS) };

static u32 s_MemForVehicles_MP[]					= { (0 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(16000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(28000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(35000000 + BUFFER_MEM_FOR_VEHICLE_MODELS) };

#if !MERGED_BUDGET
#define BUFFER_MEM_FOR_PED_MODELS_VRAM			(1500000)
#define BUFFER_MEM_FOR_VEHICLE_MODELS_VRAM		(1500000)

static u32 s_MemForPedsVram[]						= { (0 + BUFFER_MEM_FOR_PED_MODELS),
														(27000000 + BUFFER_MEM_FOR_PED_MODELS),
														(36000000 + BUFFER_MEM_FOR_PED_MODELS),
														(46800000 + BUFFER_MEM_FOR_PED_MODELS) };

static u32 s_MemForVehiclesVram[]					= { (0 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(16000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(28000000 + BUFFER_MEM_FOR_VEHICLE_MODELS),
														(35000000 + BUFFER_MEM_FOR_VEHICLE_MODELS) };
#endif // !MERGED_BUDGET 

#define NUM_BOATS_TO_BE_STREAMED    (2)     // The number of boats we would like

#define NUMBER_STREAMED_CARS_NETWORK    (8)
#define NUMBER_STREAMED_PEDS_NETWORK    (6)
#define NUMBER_ZONE_SPECIFIC_CARS_NETWORK (4)

#define SCENARIO_PEDS_DONT_GET_REMOVED_DURATION     (15000)     // Scenario peds will not be removed for this long to give a scenario a chance to use it.

#define INVALID_GROUP_INDEX (~0u)

static const unsigned TARGET_NUM_AMBIENT_PEDS      = 4;
static const unsigned TARGET_NUM_GANG_PEDS         = 2;

CompileTimeAssert((TARGET_NUM_AMBIENT_PEDS + TARGET_NUM_GANG_PEDS) == NUMBER_STREAMED_PEDS_NETWORK);

static const unsigned MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL = 2;

CompileTimeAssert(MAX_NETWORK_MODEL_INTERVALS % 4 == 0); // this needs to be divisible by 4 because we use a quarter range for working with wrapping
CompileTimeAssert(MAX_NETWORK_MODEL_INTERVALS > 0);

static const u32 s_vehicleAgeForRemoval = 10000; // ms, how old vehicles need to be to be considered for removal because of age
static const u32 s_intervalForVehicleRemoval = 10000; // try to remove a new vehicle every 30s
static u32 s_lastVehicleAgeRemoval = 0;

static const u32 s_pedAgeForRemoval = 30000;
static const u32 s_intervalForPedRemoval = 10000;
static u32 s_lastPedAgeRemoval = 0;

static const u32 s_scenarioPedMaxTimeDisallowedAfterEnteringPopZone = 3000;	// Time in ms during which we deny new scenario ped model requests after a population zone switch.
static const int s_scenarioPedMinAppropriateToSkipDisallow = 3;				// Allow new scenario ped models to be streamed in if we have at least this many appropriate peds.

// static bool s_isInCutscene = false;

static bool s_isNetGameInProgress = false;

static s32 s_numMalePedsRequested = 0;
static s32 s_numFemalePedsRequested = 0;

s32 CPopulationStreaming::sm_TotalMemUsedByPeds     = 0;
s32 CPopulationStreaming::sm_TotalMemUsedByVehicles = 0;

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
float CPedPopulation::ms_pedMemoryBudgetMultiplierNG = 1.0f;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

XPARAM(nopeds);
XPARAM(nocars);

enum ePedStreamRestriction
{
	PSR_MALE		= BIT(0),
	PSR_DRIVER		= BIT(1),
	PSR_BIKER		= BIT(2),
	PSR_FEMALE		= BIT(3)
};

enum eVehicleStreamRestriction
{
	VSR_NO_BIKE		= BIT(0),
	VSR_COMMON_VEH	= BIT(1),
	VSR_BOAT		= BIT(2)
};

struct sStreamData
{
	sStreamData() : mi(fwModelId::MI_INVALID) {}

	u32 mi;
	u32 flags;
	u32 lastTimeUsed;
	s16 groupIndex;
	s16 randVal;
};

int StreamDataCompare(const void* _a, const void* _b)
{
	sStreamData* a = (sStreamData*)_a;
	sStreamData* b = (sStreamData*)_b;

	if (a->groupIndex == b->groupIndex)
	{
		if (a->lastTimeUsed == b->lastTimeUsed)
		{
			return (b->randVal > a->randVal) ? 1 : -1;
		}
		return (a->lastTimeUsed > b->lastTimeUsed) ? 1 : -1;
	}
	return (a->groupIndex > b->groupIndex) ? 1 : -1;
}

// Network model variations. The vehicle and ped lists used are often
// stored alphabetically, and network time is used to decide where to
// index the list, which would lead to the same models always being streamed (in alphabetical order)
// The network model variations allow different combinations of models to be streamed while remaining
// consistent across machines. The host of a session will pick a model variation set to use at start up
// and send this to players as they join the session
const unsigned NUM_SHUFFLED_INDICES = 10;

struct NetworkModelVariation
{
    u32 m_NetworkTimeOffset;
    u32 m_ShuffledIndices[NUM_SHUFFLED_INDICES];
};

NetworkModelVariation s_NetworkModelVariations[NUM_NETWORK_MODEL_VARIATIONS] =
{
	{0,  {45, 33, 63, 30, 18, 1,  23, 40, 16, 22} },
	{5,  {51, 39, 42, 24, 8,  19, 48, 55, 41, 28} },
	{10, {29, 57, 34, 31, 7,  60, 35, 21, 37, 6 } },
	{15, {61, 13, 15, 68, 59, 52, 46, 54, 65, 56} },
	{20, {9,  38, 32, 25, 49, 20, 5,  50, 26, 10} },
	{25, {36, 11, 17, 62, 69, 47, 64, 14, 2,  58} },
	{30, {0,  4,  12, 53, 43, 27, 67, 3,  66, 44} },
};

CPopulationStreaming::CPopulationStreaming() :
m_bBoatsNeeded(false)
, m_MaxScenarioPedModelsLoadedPerSlot(MAX_NUMBER_STREAMED_SCENARIO_PEDS_PER_SLOT)
, m_MaxScenarioPedModelsLoadedOverride(-1)
, m_aNumScenarioPedModelsLoaded()
, m_TotalMemoryUsedByPedModelsMain(0)
, m_TotalMemoryUsedByPedModelsVram(0)
, m_TotalMemoryUsedByVehicleModelsMain(0)
, m_TotalMemoryUsedByVehicleModelsVram(0)
, m_MemOverBudgetMainPeds(0)
, m_MemOverBudgetVramPeds(0)
, m_MemOverBudgetMainVehs(0)
, m_MemOverBudgetVramVehs(0)
, m_TotalStreamedPedMemoryMain(0)
, m_TotalStreamedPedMemoryVram(0)
, m_TotalStreamedVehicleMemoryMain(0)
, m_TotalStreamedVehicleMemoryVram(0)
, m_TotalWeaponMemoryMain(0)
, m_TotalWeaponMemoryVram(0)
, m_bReducePedModelBudget(false)
, m_bReduceVehicleModelBudget(false)
, m_LastVehicleOnHighwaySpawn(0)
, m_ScenarioPedStreamingDisabledUntilTimeMS(0)
#if __BANK
, m_vehicleOverrideIndex(fwModelId::MI_INVALID)
#endif
{
}

void CPopulationStreaming::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
        m_bBoatsNeeded                        = false;
        m_TotalMemoryUsedByPedModelsMain      = 0;
        m_TotalMemoryUsedByPedModelsVram      = 0;
        m_TotalMemoryUsedByVehicleModelsMain  = 0;
        m_TotalMemoryUsedByVehicleModelsVram  = 0;
		m_MemOverBudgetMainPeds				  = 0;
		m_MemOverBudgetVramPeds				  = 0;
		m_MemOverBudgetMainVehs				  = 0;
		m_MemOverBudgetVramVehs				  = 0;
		m_TotalStreamedPedMemoryMain		  = 0;
		m_TotalStreamedPedMemoryVram		  = 0;
		m_TotalStreamedVehicleMemoryMain	  = 0;
		m_TotalStreamedVehicleMemoryVram	  = 0;
		m_TotalWeaponMemoryMain				  = 0;
		m_TotalWeaponMemoryVram				  = 0;
        m_bReducePedModelBudget               = false;
        m_bReduceVehicleModelBudget           = false;
        m_NetworkModelVariationID             = 0;
        m_LastTimeUsedForStreamingNetPeds     = 0;
        m_LastTimeUsedForStreamingNetVehicles = 0;
        m_LastZoneUsedForStreamingNetModels   = -1;
        m_LocalAllowedPedModelStartOffset     = 0;
        m_LocalAllowedVehicleModelStartOffset = 0;
		m_ScenarioPedStreamingDisabledUntilTimeMS = 0;

        for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS; index++)
        {
            m_StreamedInModelData[index].m_ModelIndex           = fwModelId::MI_INVALID;
            m_StreamedInModelData[index].m_TimeStreamedIn       = 0;
            m_StreamedInModelData[index].m_TimeNoLongerRequired = 0;
            m_StreamedInModelData[index].m_IsWeapon				= false;
        }

		// Set the number of scenario models streamed in to be zero.
		for(unsigned i=0; i < m_aNumScenarioPedModelsLoaded.GetMaxCount(); i++)
		{
			m_aNumScenarioPedModelsLoaded[i] = 0;
		}
    }
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		for(int i=0; i<CVehicleModelInfo::GetResidentObjects().GetCount(); i++)
			AddResidentObject(CVehicleModelInfo::GetResidentObjects()[i]);

		for(int i=0; i<CPedModelInfo::GetResidentObjects().GetCount(); i++)
			AddResidentObject(CPedModelInfo::GetResidentObjects()[i]);

		strLocalIndex moduleIndex = ptfxManager::GetAssetStore().FindSlot("core");
		if (moduleIndex.IsValid())
		{
			strIndex streamingIndex = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(ptfxManager::GetAssetStore().GetStreamingModuleId())->GetStreamingIndex(moduleIndex);
			if(GetResidentObjectList().Find(streamingIndex) == -1)
			{
				AddResidentObject(streamingIndex);
			}
		}
	}
    else if(initMode == INIT_SESSION)
    {
        for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS; index++)
        {
            m_StreamedInModelData[index].m_ModelIndex           = fwModelId::MI_INVALID;
            m_StreamedInModelData[index].m_TimeStreamedIn       = 0;
            m_StreamedInModelData[index].m_TimeNoLongerRequired = 0;
            m_StreamedInModelData[index].m_IsWeapon				= false;
        }

        m_AppropriateLoadedCars.Clear();
        m_InAppropriateLoadedCars.Clear();
        m_SpecialLoadedCars.Clear();
        m_LoadedBoats.Clear();
        m_VehicleModelsRequiredForNetwork.Clear();
		m_DiscardedCars.Clear();
        m_bBoatsNeeded = false;

        m_AppropriateLoadedPeds.Clear();
        m_InAppropriateLoadedPeds.Clear();
        m_SpecialLoadedPeds.Clear();
        m_PedModelsRequiredForNetwork.Clear();
		m_DiscardedPeds.Clear();

		m_vehicleComponents.Reset();
		m_pedComponents.Reset();
		m_weaponComponents.Reset();
        
		// Reset the number of scenario models streamed in.
		for(unsigned i=0; i < m_aNumScenarioPedModelsLoaded.GetMaxCount(); i++)
		{
			m_aNumScenarioPedModelsLoaded[i] = 0;
		}

		streamDebugf1("Clearing all counted as scenario ped flags.");
        CModelInfo::ClearAllCountedAsScenarioPedFlags();

        // Get the memory usage of the ped shared texture dictionaries
        m_TotalMemoryUsedByPedModelsMain = 0;
        m_TotalMemoryUsedByPedModelsVram = 0;

        for(int i =0; i < CPedModelInfo::GetResidentObjects().GetCount(); i++)
        {
            strIndex index = CPedModelInfo::GetResidentObjects()[i];
            u32 virtualSize=0, physicalSize=0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize);
			AddTotalMemoryUsed(virtualSize, physicalSize, MTT_PED);
        }

        // Get the memory usage of the vehicle shared texture dictionaries
        m_TotalMemoryUsedByVehicleModelsMain = 0;
        m_TotalMemoryUsedByVehicleModelsVram = 0;

		m_MemOverBudgetMainPeds = 0;
		m_MemOverBudgetVramPeds = 0;
		m_MemOverBudgetMainVehs = 0;
		m_MemOverBudgetVramVehs = 0;

        for(int i =0; i < CVehicleModelInfo::GetResidentObjects().GetCount(); i++)
        {
            strIndex index = CVehicleModelInfo::GetResidentObjects()[i];
            u32 virtualSize=0, physicalSize=0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize);
			AddTotalMemoryUsed(virtualSize, physicalSize, MTT_VEHICLE);
        }

        m_NetworkModelVariationID             = 0;
        m_LastTimeUsedForStreamingNetPeds     = 0;
        m_LastTimeUsedForStreamingNetVehicles = 0;
        m_LastZoneUsedForStreamingNetModels   = -1;
        m_LocalAllowedPedModelStartOffset     = 0;
        m_LocalAllowedVehicleModelStartOffset = 0;
		m_ScenarioPedStreamingDisabledUntilTimeMS = 0;

		m_TotalStreamedPedMemoryMain		  = 0;
		m_TotalStreamedPedMemoryVram		  = 0;
		m_TotalStreamedVehicleMemoryMain	  = 0;
		m_TotalStreamedVehicleMemoryVram	  = 0;

        if (CFileMgr::IsGameInstalled())
		{
            // Set these so at least we have valid values (although they may change very quickly)
            MI_CAR_POLICE_CURRENT.Set(MI_CAR_POLICE.GetModelIndex(), MI_CAR_POLICE.GetName());
            MI_CAR_TAXI_CURRENT_1.Set(MI_CAR_CLASSIC_TAXI.GetModelIndex(), MI_CAR_CLASSIC_TAXI.GetName());
		}

        m_bReducePedModelBudget     = false;
        m_bReduceVehicleModelBudget = false;

		m_numQueuedVehicleModels = 0;

		s_lastPedAgeRemoval = 0;
		s_lastVehicleAgeRemoval = 0;
		m_LastVehicleOnHighwaySpawn = 0;
    }

	OnTunablesLoad();
}

void CPopulationStreaming::OnTunablesLoad()
{
	// Load the Peds and Vehicles limits from tunables
	s_MemForPeds_SP[1]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_SP_LEVEL_1", 0x1fd16740),	  27000000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForPeds_SP[2]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_SP_LEVEL_2", 0x0a993cd0),	  36000000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForPeds_SP[3]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_SP_LEVEL_3", 0xf45f105c),	  46800000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForPeds_MP[1]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_MP_LEVEL_1", 0x37e7ed48),	  27000000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForPeds_MP[2]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_MP_LEVEL_2", 0xb343e402),	  36000000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForPeds_MP[3]     = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_PEDS_MP_LEVEL_3", 0xada0d8bc),	  46800000) + BUFFER_MEM_FOR_PED_MODELS;
	s_MemForVehicles_SP[1] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_SP_LEVEL_1", 0x18cbc1d8), 16000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;
	s_MemForVehicles_SP[2] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_SP_LEVEL_2", 0x05a49b8a), 28000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;
	s_MemForVehicles_SP[3] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_SP_LEVEL_3", 0xf75fff01), 35000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;
	s_MemForVehicles_MP[1] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_MP_LEVEL_1", 0x4e67690f), 16000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;
	s_MemForVehicles_MP[2] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_MP_LEVEL_2", 0x5cb285a9), 28000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;
	s_MemForVehicles_MP[3] = ::Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH("MEM_FOR_VEHICLES_MP_LEVEL_3", 0x69f02024), 35000000) + BUFFER_MEM_FOR_VEHICLE_MODELS;

#if !MERGED_BUDGET
	s_MemForPedsVram[1] = s_MemForPeds_SP[1];
	s_MemForPedsVram[2] = s_MemForPeds_SP[2];
	s_MemForPedsVram[3] = s_MemForPeds_SP[3];

	s_MemForVehiclesVram[1] = s_MemForVehicles_SP[1];
	s_MemForVehiclesVram[2] = s_MemForVehicles_SP[2];
	s_MemForVehiclesVram[3] = s_MemForVehicles_SP[3];

#endif // !MERGED_BUDGET
}

#if __PS3 || __XENON

#define EFIGS_FONT_SIZE		272
#define JAPANESE_FONT_SIZE	1024
#define KOREAN_FONT_SIZE	448
#define CHINESE_FONT_SIZE	2048

s32 CPopulationStreaming::GetMemForPedsAdjustment(eMemoryType
#if !MERGED_BUDGET	
	eType
#endif // MERGED_BUDGET	
	, u32 uLevel) const
{
	s32 reduce = 0;

#if !MERGED_BUDGET	
	if (eType == MEMTYPE_RESOURCE_VIRTUAL)
#endif 
	{
		switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
		case LANGUAGE_JAPANESE:
			reduce =  (JAPANESE_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		case LANGUAGE_CHINESE:
		case LANGUAGE_CHINESE_SIMPLIFIED:
			reduce = (CHINESE_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		case LANGUAGE_KOREAN:
			reduce = (KOREAN_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		default:
			reduce = 0;
			break;
		}
	}

	return reduce * (uLevel / 4);
}



s32 CPopulationStreaming::GetMemForVehiclesAdjustment(eMemoryType
#if !MERGED_BUDGET	
	eType
#endif // MERGED_BUDGET	
	, u32 uLevel) const
{
	s32 reduce = 0;

#if !MERGED_BUDGET	
	if (eType == MEMTYPE_RESOURCE_VIRTUAL)
#endif 
	{
		s32 reduce = 0;
		switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
		case LANGUAGE_JAPANESE:
			reduce =  (JAPANESE_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		case LANGUAGE_CHINESE:
		case LANGUAGE_CHINESE_SIMPLIFIED:
			reduce = (CHINESE_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		case LANGUAGE_KOREAN:
			reduce = (KOREAN_FONT_SIZE - EFIGS_FONT_SIZE) / 2;
			break;
		default:
			reduce = 0;
			break;
		}
	}

	return reduce * (uLevel / 4);
}
#endif // __PS3 || __XENON

s32 CPopulationStreaming::GetCurrentMemForVehicles() const
{
	return GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel) + GetMemForVehicles(MEMTYPE_GAME_PHYSICAL, s_MemForVehiclesLevel);
}

s32 CPopulationStreaming::GetMemForVehicles(eMemoryType
#if (!MERGED_BUDGET) || __XENON || __PS3
		eType
#endif // (!MERGED_BUDGET) || __XENON || __PS3
		, u32 uLevel) const
{
	Assert(uLevel < MAX_MEM_LEVELS);

	s32 memForVehicles = (s32)((
#if (!MERGED_BUDGET) || __XENON || __PS3
		(eType != MEMTYPE_GAME_VIRTUAL) ? s_MemForVehiclesVram[uLevel] * (NetworkInterface::IsGameInProgress() ? 1.0f : CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG) :
#endif
		(NetworkInterface::IsGameInProgress() ? s_MemForVehicles_MP[uLevel] : 
												s_MemForVehicles_SP[uLevel] * CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG )));
#if __XENON || __PS3
	return memForVehicles - GetMemForVehiclesAdjustment(eType, uLevel);
#else
	return memForVehicles;
#endif // __XENON || __PS3
}

s32 CPopulationStreaming::GetCurrentMemForPeds() const
{
	return GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel) + GetMemForPeds(MEMTYPE_GAME_PHYSICAL, s_MemForPedsLevel);
}


s32 CPopulationStreaming::GetMemForPeds(eMemoryType
#if (!MERGED_BUDGET) || __XENON || __PS3
		eType
#endif // (!MERGED_BUDGET) || __XENON || __PS3
		, u32 uLevel) const
{
	s32 result = 0;
	Assert(uLevel < MAX_MEM_LEVELS);

	result = (s32)((
#if (!MERGED_BUDGET) || __XENON || __PS3
		(eType != MEMTYPE_GAME_VIRTUAL) ? s_MemForPedsVram[uLevel] * (NetworkInterface::IsGameInProgress() ? 1.0f : CPedPopulation::ms_pedMemoryBudgetMultiplierNG) :
#endif
		(NetworkInterface::IsGameInProgress() ? s_MemForPeds_MP[uLevel] : 
												s_MemForPeds_SP[uLevel] * CPedPopulation::ms_pedMemoryBudgetMultiplierNG)));
#if __XENON || __PS3
	return result - GetMemForPedsAdjustment(eType, uLevel);
#else
	return result;
#endif // __XENON || __PS3
}

void CPopulationStreaming::RemoveResidentPedObjects()
{
	// Get the memory usage of the shared ped texture dictionaries
	for(int i =0; i < CPedModelInfo::GetResidentObjects().GetCount(); i++)
	{
		strIndex index = CPedModelInfo::GetResidentObjects()[i];
		u32 virtualSize=0, physicalSize=0;
		strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize);
		AddTotalMemoryUsed(-(s32)virtualSize, -(s32)physicalSize, MTT_PED);
	}
}

void CPopulationStreaming::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
		SCENARIOCLUSTERING.ResetModelRefCounts();

        RemoveStreamedPedModelsOnShutdown();
        RemoveStreamedCarModelsOnShutdown();

        // Get the memory usage of the shared vehicle texture dictionaries
        for(int i =0; i < CVehicleModelInfo::GetResidentObjects().GetCount(); i++)
        {
			strIndex index = CVehicleModelInfo::GetResidentObjects()[i];
			u32 virtualSize=0, physicalSize=0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize);
            AddTotalMemoryUsed(-(s32)virtualSize, -(s32)physicalSize, MTT_VEHICLE);
        }

        RemoveResidentPedObjects();

#if DEBUG_POPULATION_MEMORY
		for (s32 i = 0; i < MAX_STREAMED_IN_MODELS; ++i)
		{
			if (m_StreamedInModelData[i].m_ModelIndex != fwModelId::MI_INVALID && CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex))))
			{
				CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex)));
				if (bmi->GetModelType() == MI_TYPE_PED)
				{
					CPedModelInfo* pmi = (CPedModelInfo*)bmi;
					Warningf("CPopulationStreaming::Shutdown: Non cleaned up model data for model '%s' found. M%dK V%dK HD M%dK V%dK", bmi->GetModelName(), m_StreamedInModelData[i].m_mainUsed>>10, m_StreamedInModelData[i].m_vramUsed>>10, m_StreamedInModelData[i].m_HDAssetSizeMain>>10, m_StreamedInModelData[i].m_HDAssetSizeVram>>10);
					if (pmi)
						Warningf("CPopulationStreaming::Shutdown: Ped %s has %d refs. %s", pmi->GetModelName(), pmi->GetNumPedModelRefs(), pmi->m_isRequestedAsDriver ? "(requested as driver)" : "");
				}
				else if(bmi->GetModelType() == MI_TYPE_VEHICLE)
				{
					CVehicleModelInfo* vmi = (CVehicleModelInfo*)bmi;
					Warningf("CPopulationStreaming::Shutdown: Non cleaned up model data for model '%s' found. M%dK V%dK HD M%dK V%dK", bmi->GetModelName(), m_StreamedInModelData[i].m_mainUsed>>10, m_StreamedInModelData[i].m_vramUsed>>10, m_StreamedInModelData[i].m_HDAssetSizeMain>>10, m_StreamedInModelData[i].m_HDAssetSizeVram>>10);
					if (vmi)
						Warningf("CPopulationStreaming::Shutdown: Veh %s has %d(%d) refs (parked).", vmi->GetModelName(), vmi->GetNumVehicleModelRefs(), vmi->GetNumVehicleModelRefsParked());
				}
				/*else if (bmi->GetModelType() == MI_TYPE_WEAPON)
				{
				Warningf("CPopulationStreaming::Shutdown: Non cleaned up model data for weapon '%s' found. M%dK V%dK HD M%dK V%dK", bmi->GetModelName(), m_StreamedInModelData[i].m_mainUsed>>10, m_StreamedInModelData[i].m_vramUsed>>10, m_StreamedInModelData[i].m_HDAssetSizeMain>>10, m_StreamedInModelData[i].m_HDAssetSizeVram>>10);
				}*/
			}
		}

		for (s32 i = 0; i < m_pedComponents.GetCount(); ++i)
		{
			if (m_pedComponents[i].refCount != 0)
			{
				Warningf("CPopulationStreaming::Shutdown: Found ped component with ref count %d, streaming index: %d!", m_pedComponents[i].refCount, m_pedComponents[i].streamIdx.Get());
			}
		}

		for (s32 i = 0; i < m_vehicleComponents.GetCount(); ++i)
		{
			if (m_vehicleComponents[i].refCount != 0)
			{
				Warningf("CPopulationStreaming::Shutdown: Found vehicle component with ref count %d, streaming index: %d!", m_vehicleComponents[i].refCount, m_vehicleComponents[i].streamIdx.Get());
			}
		}

		// can't do this for weapons, they aren't streamed out at this point
		//for (s32 i = 0; i < m_weaponComponents.GetCount(); ++i)
		//{
		//	if (m_weaponComponents[i].refCount != 0)
		//	{
		//		Warningf("CPopulationStreaming::Shutdown: Found weapon component with ref count %d, streaming index: %d!", m_weaponComponents[i].refCount, m_weaponComponents[i].streamIdx.Get());
		//	}
		//}
#endif // DEBUG_POPULATION_MEMORY

		Assertf(m_TotalMemoryUsedByPedModelsMain == 0, "Main ped memory not what expected: %d", m_TotalMemoryUsedByPedModelsMain);
        Assertf(m_TotalMemoryUsedByPedModelsVram == 0, "Physical ped memory not what expected: %d", m_TotalMemoryUsedByPedModelsVram);
        Assertf(m_TotalMemoryUsedByVehicleModelsMain == m_TotalWeaponMemoryMain, "Main car memory not what expected: %d - %d", m_TotalMemoryUsedByVehicleModelsMain, m_TotalWeaponMemoryMain);
        Assertf(m_TotalMemoryUsedByVehicleModelsVram == m_TotalWeaponMemoryVram, "Physical car memory not what expected: %d - %d", m_TotalMemoryUsedByVehicleModelsVram, m_TotalWeaponMemoryVram);

		Assertf(m_TotalStreamedPedMemoryMain == 0, "Main streamed ped memory not what expected: %d", m_TotalStreamedPedMemoryMain);
		Assertf(m_TotalStreamedPedMemoryVram == 0, "Physics streamed ped memory not what expected: %d", m_TotalStreamedPedMemoryVram);
		Assertf(m_TotalStreamedVehicleMemoryMain == 0, "Main streamed car memory not what expected: %d", m_TotalStreamedVehicleMemoryMain);
		Assertf(m_TotalStreamedVehicleMemoryVram == 0, "Physical streamed car memory not what expected: %d", m_TotalStreamedVehicleMemoryVram);

        // can't have these asserts, weapon modelinfos aren't streamed out at this point
		//Assertf(m_TotalWeaponMemoryMain == 0, "Main weapon memory not what expected: %d", m_TotalWeaponMemoryMain);
		//Assertf(m_TotalWeaponMemoryVram == 0, "Physical weapons memory not what expected: %d", m_TotalWeaponMemoryVram);

		for (int i = 0; i < m_residentObjectsToRemove.GetCount(); ++i)
		{
			strIndex index = m_residentObjectsToRemove[i];
			m_residentObjects.Delete(m_residentObjects.Find(index));
		}
		m_residentObjectsToRemove.Reset();

	}
	else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		m_residentObjects.ResetCount();
	}
}

void CPopulationStreaming::Update()
{
#if !__FINAL
    if(CStreaming::IsStreamingPaused())
        return;
#endif

    EmergencyRemoveVehicleStructures();

	if (!CStreaming::IsPlayerPositioned())
		return;

    if (!CFileMgr::IsGameInstalled())
        return;

	if (CGameSessionStateMachine::GetGameSessionState() == CGameSessionStateMachine::START_NEW_GAME)
        return;

//    if(strStreamingEngine::GetIsLoadingPriorityObjects())
//        return;

	if (!s_isNetGameInProgress && NetworkInterface::IsGameInProgress())
	{
		// flag any discarded vehicles we have as not used anymore, since we don't deal with them in MP games
		// they might end up sticking around in memory for a while
		s32 numDiscardedCars = m_DiscardedCars.CountMembers();
		for (s32 i = 0; i < numDiscardedCars; ++i)
		{
			fwModelId modelId((strLocalIndex(m_DiscardedCars.GetMember(i))));
			if (!modelId.IsValid())
				continue;

			CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
			MarkModelNoLongerRequired(modelId.GetModelIndex());
		}

		ReclassifyLoadedVehiclesForNetwork();
	}
	s_isNetGameInProgress = NetworkInterface::IsGameInProgress();

#if 0
    // Don't load new models when the cutscenemanager is running
    if(CutSceneManager::GetInstance())
    {
		if (CutSceneManager::GetInstance()->IsRunning())
		{
			s_isInCutscene = true;
		}
		else
		{
			if (s_isInCutscene)
			{
				s_isInCutscene = false;
				if (m_AppropriateLoadedCars.CountMembers() < 3 && s_MemForVehiclesLevel > 0)
				{
					StreamOneNewCar();
					StreamOneNewCar();
					StreamOneNewCar();
				}
			}
			StreamModelsIn();
		}
    }
	else
#endif
	{
#if GTA_REPLAY
		if(!CReplayCoordinator::ShouldDisablePopStreamer())
#endif // GTA_REPLAY
			StreamModelsIn();
	}

    StreamModelsOut();
    RemoveUnusedModels();
	UpdateHDCosts();

	perfClearingHouse::SetValue(perfClearingHouse::PED_STREAMING_OVERAGE, m_MemOverBudgetMainPeds + m_MemOverBudgetVramPeds);
	perfClearingHouse::SetValue(perfClearingHouse::VEHICLE_STREAMING_OVERAGE, m_MemOverBudgetMainVehs + m_MemOverBudgetVramVehs);

	// If we recently switched to a different population zone, we may have disallowed
	// new scenario ped model requests. The code below makes sure we break out of that
	// mode if we already have enough appropriate peds - hopefully that will prevent any
	// scenario ped streaming interruption if we are just switching between population zones
	// where roughly the same ped models are appropriate.
	if(fwTimer::GetTimeInMilliseconds() < m_ScenarioPedStreamingDisabledUntilTimeMS)
	{
		streamDebugf2("Streaming of new scenario peds disabled after pop zone switch, %d ms remaining.",
				(int)(m_ScenarioPedStreamingDisabledUntilTimeMS - fwTimer::GetTimeInMilliseconds()));
		const int numAppropriate = m_AppropriateLoadedPeds.CountUsableOnes();
		if(numAppropriate >= s_scenarioPedMinAppropriateToSkipDisallow)
		{
			streamDebugf1("Streaming of new scenario peds re-enabled after pop zone switch, %d appropriate peds loaded.", numAppropriate);
			m_ScenarioPedStreamingDisabledUntilTimeMS = 0;
		}
	}

	sm_TotalMemUsedByPeds     = GetTotalMemUsedByPeds();
	sm_TotalMemUsedByVehicles = GetTotalMemUsedByVehicles();
}

void CPopulationStreaming::SetNetworkModelVariationID(u32 networkModelVariationID)
{
    if(Verifyf(networkModelVariationID < NUM_NETWORK_MODEL_VARIATIONS, "Trying to set an invalid network model variation!"))
    {
        m_NetworkModelVariationID             = networkModelVariationID;
        m_LastTimeUsedForStreamingNetPeds     = GetCurrentNetworkTimeInterval();
        m_LastTimeUsedForStreamingNetVehicles = GetCurrentNetworkTimeInterval();
    }
}

void CPopulationStreaming::SetReduceAmbientPedModelBudget(bool value)
{
    m_bReducePedModelBudget = value;
}

void CPopulationStreaming::SetReduceAmbientVehicleModelBudget(bool value)
{
    m_bReduceVehicleModelBudget = value;
}

void CPopulationStreaming::HandlePopCycleInfoChange(bool enteredNewZone)
{
	ReclassifyLoadedPeds();
	TellStreamingAboutDeletablePeds();

    ReclassifyLoadedCars();
    TellStreamingAboutDeletableVehicles();

	if(enteredNewZone)
	{
		// Prevent scenario peds from streaming in for a while, so we get a chance to catch
		// up with generic models for the new population zone.
		DisallowScenarioPedStreamingForDuration(s_scenarioPedMaxTimeDisallowedAfterEnteringPopZone);
	}
}

void CPopulationStreaming::StreamOneNewCar()
{
#if __BANK
	if (PARAM_nocars.Get())
		return;
#endif // __BANK

	if (CVehicleStructure::m_pInfoPool->GetNoOfFreeSpaces() <= 0)
	{
		// this should only happen when we leak vehicles or in the future when we afford 70+ vehicle models streamed in at the same time
		Assertf(false, "CPopulationStreaming::StreamOneNewCar: Trying to stream new car while we have no more space in the CVehicleStruct pool!");
		return;
	}

    strLocalIndex modelIndex = strLocalIndex(fwModelId::MI_INVALID);

    if(NetworkInterface::IsGameInProgress())
    {
        modelIndex = ChooseNewVehicleModelForMP();
    }
    else
    {
        modelIndex = ChooseNewVehicleModelForSP();
    }

    if(CModelInfo::IsValidModelInfo(modelIndex.Get()))
    {
		STRVIS_SET_CONTEXT(strStreamingVisualize::POPSTREAMER);

		fwModelId modelId(modelIndex);
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);

		RequestVehicleDriver(modelIndex.Get());

		if (m_DiscardedCars.IsMemberInGroup(modelIndex.Get()))
			ReinstateDiscardedCar(modelIndex.Get());

        // see if we should request a trailer too
        u32 curTime = fwTimer::GetTimeInMilliseconds();
        if ((curTime - m_LastVehicleOnHighwaySpawn) < 5000)
        {
            CVehicleModelInfo* vmi = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
            if (vmi)
            {
                u32 trailersLoaded = 0;
				CVehicleModelInfo* trailerVmi = NULL;
                const u32 trailerCount = vmi->GetTrailerCount();
                for (s32 i = 0; i < trailerCount; ++i)
                {
                    if(CModelInfo::HaveAssetsLoaded(fwModelId(vmi->GetTrailer(i, trailerVmi))))
                    {
                        trailersLoaded++;
                        break;
                    }
                }

                if (trailerCount && trailersLoaded == 0)
                {
                    fwModelId trailerId = vmi->GetTrailer(fwRandom::GetRandomNumberInRange(0, trailerCount), trailerVmi);
                    CModelInfo::RequestAssets(trailerId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
                    m_numQueuedVehicleModels++;
                }
            }
        }

		m_numQueuedVehicleModels++;
		STRVIS_SET_CONTEXT(strStreamingVisualize::NONE);
    }
}

u32 CPopulationStreaming::GetNumCommonVehicles()
{
    u32 numComonVehs = 0;

    s32 numVehs = m_AppropriateLoadedCars.CountMembers();
    u32 maxAmbientVehs = (u32)(CVehiclePopulation::GetDesiredNumberAmbientVehicles() + 0.5f);
    for (s32 i = 0; i < numVehs; ++i)
    {
        strLocalIndex mi = strLocalIndex(m_AppropriateLoadedCars.GetMember((u32)i));
        if (CPopCycle::GetPopGroups().IsVehIndexMember(CPopCycle::GetCommonVehicleGroup(), mi.Get()))
        {
            CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(mi));
            if (vmi->GetVehicleMaxNumber() >= maxAmbientVehs)
            {
                numComonVehs++;
            }
        }
    }

    return numComonVehs;
}

u32 CPopulationStreaming::ChooseNewVehicleModelForMP()
{
#if __BANK
	if (m_vehicleOverrideIndex != fwModelId::MI_INVALID)
		return m_vehicleOverrideIndex;
#endif

    u32 chosenModelIndex = fwModelId::MI_INVALID;

    unsigned numVehicleModelsRequired = m_VehicleModelsRequiredForNetwork.CountMembers();

    for(unsigned index = 0; index < numVehicleModelsRequired && !CModelInfo::IsValidModelInfo(chosenModelIndex); index++)
    {
        strLocalIndex modelIndex = strLocalIndex(m_VehicleModelsRequiredForNetwork.GetMember(index));

		fwModelId modelId(modelIndex);
        if(modelId.IsValid())
        {
            if(CModelInfo::HaveAssetsLoaded(modelId))
            {
                if(!IsModelDataStreamedIn(modelIndex.Get()))
                {
                    chosenModelIndex = modelIndex.Get();
                }
            }
            else
            {
                chosenModelIndex = modelIndex.Get();
            }
        }
    }

    return chosenModelIndex;
}

u32 CPopulationStreaming::ChooseNewVehicleModelForSP()
{
    u32 modelIndex = fwModelId::MI_INVALID;

#if __BANK
	if (m_vehicleOverrideIndex != fwModelId::MI_INVALID)
		return m_vehicleOverrideIndex;
#endif

	u32 strRestrictions = m_bBoatsNeeded ? VSR_BOAT : 0;

	// make sure we always have a common vehicle in the appropriate list
	// a common vehicle is a vehicle in the VEH_MID group with no maxNumber restriction
	if (GetNumCommonVehicles() <= 1)
		strRestrictions |= VSR_COMMON_VEH;

	// see if we have any peds that can ride bikes streamed in
	if (!m_AppropriateLoadedPeds.IsBikeDriverInPedGroup())
		strRestrictions |= VSR_NO_BIKE;

	CLoadedModelGroup appropriateVehs;
	appropriateVehs.Merge(&m_AppropriateLoadedCars, &m_LoadedBoats);

    // if we're in the "default" zone wait, or we end up streaming in vehicles we don't need
    static const u32 defaultHash = atStringHash("DEFAULT");
    if (!CPopCycle::GetCurrentActiveZone() || CPopCycle::GetCurrentActiveZone()->m_id.GetHash() != defaultHash)
	{
		s32 *pGroupArray;
		int NumAvailableGroups = CPopCycle::PickMostWantingGroupOfModels(appropriateVehs, false, &pGroupArray);
		modelIndex = ChooseCarModelToLoad(pGroupArray, NumAvailableGroups, strRestrictions);
	}

    return modelIndex;
}

void CPopulationStreaming::StreamOneNewPed(bool bNeedDriver)
{
#if __BANK
	if (PARAM_nopeds.Get())
		return;
#endif // __BANK

	strLocalIndex modelIndex = strLocalIndex(fwModelId::MI_INVALID);

    if(NetworkInterface::IsGameInProgress())
    {
        modelIndex = ChooseNewPedModelForMP(bNeedDriver);
    }
    else
    {
        modelIndex = ChooseNewPedModelForSP(bNeedDriver);
    }

    if(CModelInfo::IsValidModelInfo(modelIndex.Get()))
    {
		fwModelId modelId(modelIndex);
		if(CModelInfo::HaveAssetsLoaded(modelId))
        {
            if(!IsModelDataStreamedIn(modelIndex.Get()))
            {
                PedHasBeenStreamedIn(modelIndex.Get());
				CModelInfo::SetAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
            }
			else
			{
				ReinstateDiscardedPed(modelIndex.Get());
			}
        }
        else
        {
            CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);

			CPedModelInfo* pModelInfo = static_cast<CPedModelInfo *>(CModelInfo::GetBaseModelInfo(modelId));
			if (pModelInfo->GetPersonalitySettings().GetIsMale())
				s_numMalePedsRequested++;
			else
				s_numFemalePedsRequested++;
        }
    }
}

u32 CPopulationStreaming::ChooseNewPedModelForSP(bool bNeedDriver)
{
    u32 modelIndex = fwModelId::MI_INVALID;

	u32 strRestrictions = bNeedDriver ? PSR_DRIVER : 0;

	if (!m_AppropriateLoadedPeds.IsMaleInPedGroup() && s_numMalePedsRequested == 0)
		strRestrictions |= PSR_MALE;
	else if (!m_AppropriateLoadedPeds.IsFemaleInPedGroup() && s_numFemalePedsRequested == 0)
		strRestrictions |= PSR_FEMALE;

	if (!m_AppropriateLoadedPeds.IsBikeDriverInPedGroup() && CPopCycle::VehicleBikeGroupActive())
		strRestrictions |= PSR_BIKER;

    // if current zone index is 0 we're in the "default" zone before the popzone data has been loaded.
    // wait, or we end up streaming in vehicles we don't need
    if (CPopCycle::GetCurrentZoneIndex() > 0)
	{
		s32 *pGroupArray;
		int NumAvailableGroups = CPopCycle::PickMostWantingGroupOfModels(m_AppropriateLoadedPeds, true, &pGroupArray);
		modelIndex = ChoosePedModelToLoad(pGroupArray, NumAvailableGroups, strRestrictions);
	}

    return modelIndex;
}

u32 CPopulationStreaming::ChooseNewPedModelForMP(bool bNeedDriver)
{
	u32 chosenModelIndex = fwModelId::MI_INVALID;
	u32 chosenNonDriver = fwModelId::MI_INVALID;

    unsigned numPedModelsRequired = m_PedModelsRequiredForNetwork.CountMembers();

    for(unsigned index = 0; index < numPedModelsRequired && !CModelInfo::IsValidModelInfo(chosenModelIndex); index++)
    {
        strLocalIndex modelIndex = strLocalIndex(m_PedModelsRequiredForNetwork.GetMember(index));

		fwModelId modelId(modelIndex);
        if(modelId.IsValid())
        {
            if(CModelInfo::HaveAssetsLoaded(modelId))
            {
                if(!IsModelDataStreamedIn(modelIndex.Get()))
                {
					if (CanPedModelDrive(modelIndex.Get()) || !bNeedDriver)
	                    chosenModelIndex = modelIndex.Get();
					else if (chosenNonDriver != fwModelId::MI_INVALID)
						chosenNonDriver = modelIndex.Get();
                }
            }
            else
            {
                chosenModelIndex = modelIndex.Get();
            }
        }
    }

	if (chosenModelIndex == fwModelId::MI_INVALID)
		return chosenNonDriver;

    return chosenModelIndex;
}

void CPopulationStreaming::StreamModelsIn()
{
	bool streamNewModels = !(fwTimer::GetSystemFrameCount() & 63) || (!CStreaming::GetNumberObjectsRequested() && IsThereAnyMemoryLeft(MTT_VEHICLE));
    if (streamNewModels || (CPopCycle::GetPreferredGroupForVehs() != -1 && m_numQueuedVehicleModels == 0))
    {
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::POPSTREAMER);

		bool bNeedDriver = true;
		CLoadedModelGroup fallbackPedDrivers;
		GetFallbackPedGroup(fallbackPedDrivers, true);
		if (fallbackPedDrivers.CountMembers() > 0)
		{
			bNeedDriver = false;
		}

		if (streamNewModels)
		{
            if (IsThereAnyMemoryLeft(MTT_PED) || (m_AppropriateLoadedPeds.CountMembers() < MIN_NUMBER_STREAMED_PEDS && s_MemForPedsLevel == MAX_MEM_LEVELS - 1) || bNeedDriver)
            {
                NOTFINAL_ONLY(if(gbAllowAmbientPedGeneration || gbAllowVehGenerationOrRemoval))
                {
                    StreamOneNewPed(bNeedDriver);
                }
            }
		}

		if (IsThereAnyMemoryLeft(MTT_VEHICLE) || (m_AppropriateLoadedCars.CountMembers() < MIN_NUMBER_STREAMED_CARS && s_MemForVehiclesLevel == MAX_MEM_LEVELS - 1))
        {
            NOTFINAL_ONLY(if(gbAllowVehGenerationOrRemoval))
            {
                StreamOneNewCar();
            }
        }

        if(!(fwTimer::GetSystemFrameCount() & 127))
        {
            bool bOldBoatsNeeded = m_bBoatsNeeded;

            // we don't support random boats in network games, if the players are split across the map,
            // it's wasteful to stream in a vehicle model that not every player can use
			bool bAllowRandomBoats = NetworkInterface::IsGameInProgress() ? CVehiclePopulation::ms_bAllowRandomBoatsMP : CVehiclePopulation::ms_bAllowRandomBoats;
            if (bAllowRandomBoats)
            {
                m_bBoatsNeeded = ThePaths.IsWaterNodeNearby(camInterface::GetPos(), 80.0f);
            }
            else
            {
                m_bBoatsNeeded = false;
            }
            if (bOldBoatsNeeded != m_bBoatsNeeded)
            {
                TellStreamingAboutDeletableVehicles();

				// If boats are needed now (and not before), make sure that any model already
				// in m_LoadedBoats gets a driver requested.
				if(m_bBoatsNeeded)
				{
					// Note: potentially we could restrict this to the members of the VEH_BOATS set.
					// Or, maybe only VEH_BOATS members should go into the m_LoadedBoats set to begin with?

					const int numLoadedBoats = m_LoadedBoats.CountMembers();
					for(int i = 0; i < numLoadedBoats; i++)
					{
						const strLocalIndex modelIndex = strLocalIndex(m_LoadedBoats.GetMember(i));
						fwModelId modelId(modelIndex);
						if(modelId.IsValid())
						{
							RequestVehicleDriver(modelIndex.Get());
						}
					}
				}
            }
        }

        // update which ped models are required by the network game
        if(streamNewModels && NetworkInterface::IsGameInProgress())
        {
            UpdateModelsRequiredForNetwork();

			ReclassifyLoadedPedsForNetwork();
			ReclassifyLoadedVehiclesForNetwork();
        }
    }
}

void CPopulationStreaming::StreamModelsOut()
{
	bool forceStreamModelOut = (CPopCycle::GetPreferredGroupForVehs() != -1) && !NetworkInterface::IsGameInProgress();
    if ((fwTimer::GetSystemFrameCount() & 31) && !forceStreamModelOut)
    {
        return;
    }

    // reclassify any network models based on whether they are current required
    // by the network game
    if(NetworkInterface::IsGameInProgress())
    {
        ReclassifyLoadedVehiclesForNetwork();
        ReclassifyLoadedPedsForNetwork();
    }

	u32 removedPeds = 0;

    // Go through the inappropriate peds and have them removed.
	s32 numMembers = m_InAppropriateLoadedPeds.CountMembers();
    for (int i = 0; i < numMembers; i++)
    {
        strLocalIndex ModelIndex = strLocalIndex(m_InAppropriateLoadedPeds.GetMember(i));

		fwModelId modelId(ModelIndex);
		if (!modelId.IsValid())
			break;

        CPedModelInfo *pPedModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
		if (pPedModelInfo->m_isRequestedAsDriver)
			continue;

        if ( (!pPedModelInfo->GetCountedAsScenarioPed()) ||
            (fwTimer::GetTimeInMilliseconds() > GetTimeModelStreamedIn(ModelIndex.Get()) + SCENARIO_PEDS_DONT_GET_REMOVED_DURATION &&
            !(pPedModelInfo->GetNumPedModelRefs() > 0)) )
        {
            Assert(CModelInfo::HaveAssetsLoaded(modelId));
            CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
            MarkModelNoLongerRequired(ModelIndex.Get());
			if (!NetworkInterface::IsGameInProgress())
			{
				AddDiscardedPed(ModelIndex.Get());
				--i;
			}
			++removedPeds;
        }
    }

    // Scenario peds can also end up in the special peds (if they aren't in pedgrp.dat)
    // Also go through the special peds and remove the scenario peds that have been there for a while.
	numMembers = m_SpecialLoadedPeds.CountMembers();
    for (int i = 0; i < numMembers; i++)
    {
        strLocalIndex ModelIndex = strLocalIndex(m_SpecialLoadedPeds.GetMember(i));

		fwModelId modelId(ModelIndex);
		if (!modelId.IsValid())
			break;

        CPedModelInfo *pPedModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
		bool canDeleteScenarioPed = pPedModelInfo->GetCountedAsScenarioPed() && !pPedModelInfo->m_isRequestedAsDriver && fwTimer::GetTimeInMilliseconds() > (GetTimeModelStreamedIn(ModelIndex.Get()) + SCENARIO_PEDS_DONT_GET_REMOVED_DURATION);
		bool canDeleteOtherPed = !pPedModelInfo->GetCountedAsScenarioPed() && !pPedModelInfo->m_isRequestedAsDriver;

        if ((canDeleteScenarioPed || canDeleteOtherPed) && pPedModelInfo->GetNumPedModelRefs() <= 0)
		{
            Assert(CModelInfo::HaveAssetsLoaded(modelId));
            CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
            MarkModelNoLongerRequired(ModelIndex.Get());
            // add to discarded list if this ped is appropriate for this zone. scenario/special peds may or may not be useable in
            // ambient pop. if they are, they need to end up in the discarded list here or else they won't be rotated out
            if (IsPedModelNeededCurrently(ModelIndex.Get()))
            {
                AddDiscardedPed(ModelIndex.Get());
                --i;
            }
			++removedPeds;
        }
    }

	// try to remove old models so we get a nice rotation going
	if (removedPeds == 0 && m_DiscardedPeds.CountMembers() <= 1 && m_AppropriateLoadedPeds.CountMembers() > 1)
	{
		strLocalIndex modelToDelete = FindOldestModelToStreamOut(true, true);
		if (modelToDelete != fwModelId::MI_INVALID)
		{
			fwModelId modelId(modelToDelete);
			Assert(CModelInfo::HaveAssetsLoaded(modelId));

			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
			if (!pPedModelInfo->m_isRequestedAsDriver)
			{
				CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
				MarkModelNoLongerRequired(modelToDelete.Get());
				if (!NetworkInterface::IsGameInProgress())
					AddDiscardedPed(modelToDelete.Get());
			}
		}
	}

    if(NetworkInterface::IsGameInProgress())
    {
        CLoadedModelGroup modelsReadyForRemoval;
        CLoadedModelGroup modelsReadyForRemovalWithNoRefs;
        unsigned          modelsPendingRemoval = 0;

        // In network games we stream out any cars that not in the current required list
        int numMembers = m_InAppropriateLoadedCars.CountMembers();

        for(int index = 0; index < numMembers; index++)
        {
            u32 modelIndex = m_InAppropriateLoadedCars.GetMember(index);

            fwModelId modelIdToBeRemoved((strLocalIndex(modelIndex)));

            if(modelIdToBeRemoved.IsValid())
            {
				CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelIdToBeRemoved);

                bool missionRequired = (CModelInfo::GetAssetStreamFlags(modelIdToBeRemoved) & STRFLAG_MISSION_REQUIRED) != 0;
				missionRequired |= pModelInfo->HasAnyMissionInstances();

                if(!missionRequired)
                {
                    if(!CModelInfo::GetAssetRequiredFlag(modelIdToBeRemoved))
                    {
                        modelsPendingRemoval++;
                    }
                    else
                    {
						// don't try to remove the player important vehicle, it won't be removed since the player is in it
						if (CVehiclePopulation::ms_pInterestingVehicle)
							if (CVehiclePopulation::ms_pInterestingVehicle->GetModelIndex() == modelIndex)
								continue;

						if (pModelInfo->GetNumRefs() <= 0)
							modelsReadyForRemovalWithNoRefs.AddMember(modelIndex);
						else
							modelsReadyForRemoval.AddMember(modelIndex);
                    }
                }
            }
        }

        if(modelsPendingRemoval < MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL)
        {
			// first try ejecting models with no refs, higher chance of them getting removed quickly
			for(int index = 0; index < modelsReadyForRemovalWithNoRefs.CountMembers() && (modelsPendingRemoval < MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL); index++)
			{
				strLocalIndex modelIndex = strLocalIndex(modelsReadyForRemovalWithNoRefs.GetMember(index));

				fwModelId modelIdToBeRemoved(modelIndex);

				if(modelIdToBeRemoved.IsValid())
				{
					Assert(CModelInfo::HaveAssetsLoaded(modelIdToBeRemoved));

					CModelInfo::ClearAssetRequiredFlag(modelIdToBeRemoved, STRFLAG_DONTDELETE);
					MarkModelNoLongerRequired(modelIndex.Get());

					modelsPendingRemoval++;
				}
			}

            for(int index = 0; index < modelsReadyForRemoval.CountMembers() && (modelsPendingRemoval < MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL); index++)
            {
                strLocalIndex modelIndex = strLocalIndex(modelsReadyForRemoval.GetMember(index));

                fwModelId modelIdToBeRemoved(modelIndex);

                if(modelIdToBeRemoved.IsValid())
                {
                    Assert(CModelInfo::HaveAssetsLoaded(modelIdToBeRemoved));

			        CModelInfo::ClearAssetRequiredFlag(modelIdToBeRemoved, STRFLAG_DONTDELETE);
			        MarkModelNoLongerRequired(modelIndex.Get());

                    modelsPendingRemoval++;
                }
            }
        }
    }
    else
    {
        // TODO - Some code has been removed from here that was limiting the number of vehicles that
        //        can be streamed out concurrently - unless we were at the airport. This need checking
        //        to see whether it is still necessary. The existing code didn't work though, so I have removed it
        // END TODO

        bool canStreamCommonOut = GetNumCommonVehicles() > 1;

        CLoadedModelGroup candidatesForRemoval;
        GetCandidateVehiclesToStreamOutForSP(candidatesForRemoval, canStreamCommonOut);

        // Pick the car that has been used most times so far. (More chance that the player has seen it)
        // Take the vehicles frequency into account so that rare vehicle also will get streamed out.
        s32 MinTimesUsed = -1;
		strLocalIndex ModelIndexToBeRemoved = strLocalIndex(fwModelId::MI_INVALID);
		s32 numMembers = candidatesForRemoval.CountMembers();
        for (int k = 0; k < numMembers; k++)
        {
			const u32 uModelIndex = candidatesForRemoval.GetMember(k);
            const strLocalIndex ModelIndex = strLocalIndex(uModelIndex);

			const fwModelId modelId(ModelIndex);

			// don't try to remove the player important vehicle, it won't be removed since the player is in it
			const bool interestingVehicle = CVehiclePopulation::ms_pInterestingVehicle && CVehiclePopulation::ms_pInterestingVehicle->GetModelIndex() == uModelIndex;
			// don't try and stream out mission required vehicles - they won't get removed
			const bool missionRequired = (CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_MISSION_REQUIRED) != 0;

			if(!interestingVehicle && !missionRequired)
			{
				if (CModelInfo::GetAssetRequiredFlag(modelId))	// If the model hasn't been flagged for removal elsewhere
				{
					CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

					s32 scaledTimesUsed = pModelInfo->GetNumTimesUsed() * 100 / pModelInfo->GetVehicleFreq();
					if ( scaledTimesUsed > MinTimesUsed )
					{
						ModelIndexToBeRemoved = ModelIndex;
						MinTimesUsed = scaledTimesUsed;
					}
				}
				else if(!NetworkInterface::IsGameInProgress() 
						&& !m_DiscardedCars.IsMemberInGroup(uModelIndex) 
						&& !m_SpecialLoadedCars.IsMemberInGroup(uModelIndex) 
						&& !m_LoadedBoats.IsMemberInGroup(uModelIndex))
				{
					// If the model has been flagged for removal, but hasn't been discarded
					// This tends to happen for vehicles with streaming handled elsewhere, like debug and script created ones.
					AddDiscardedCar(uModelIndex);
				}
			}
        }

		if (ModelIndexToBeRemoved == fwModelId::MI_INVALID && m_DiscardedCars.CountMembers() <= 1)
			ModelIndexToBeRemoved = FindOldestModelToStreamOut(false, canStreamCommonOut);

		fwModelId modelIdToBeRemoved(ModelIndexToBeRemoved);
        if(modelIdToBeRemoved.IsValid())
        {
            Assert(CModelInfo::HaveAssetsLoaded(modelIdToBeRemoved));

			CModelInfo::ClearAssetRequiredFlag(modelIdToBeRemoved, STRFLAG_DONTDELETE);
			MarkModelNoLongerRequired(ModelIndexToBeRemoved.Get());
			if(!NetworkInterface::IsGameInProgress() && !m_SpecialLoadedCars.IsMemberInGroup(ModelIndexToBeRemoved.Get()) && !m_LoadedBoats.IsMemberInGroup(ModelIndexToBeRemoved.Get()))
				AddDiscardedCar(ModelIndexToBeRemoved.Get());
        }
    }
}

strLocalIndex CPopulationStreaming::FindOldestModelToStreamOut(bool isPed, bool canStreamCommonOut)
{
	strLocalIndex modelId = strLocalIndex(fwModelId::MI_INVALID);

	// remove a car/ped from memory so we can get a new one in and rotate through the whole group for current pop zone
	// do it only if no other car/ped has been flagged for removal, at least s_intervalForAgeRemoval ms have passed since
	// last removal and there's no more memory to stream in new models
	// Don't do this is network games as MP games have their own rules for when vehicles are streamed out/in, independently
	// of the pop zone data files.
	if(!NetworkInterface::IsGameInProgress())
	{
		u32 nextRemovalTime = isPed ? (s_lastPedAgeRemoval + s_intervalForPedRemoval) : (s_lastVehicleAgeRemoval + s_intervalForVehicleRemoval);
		//s32 totalMemUsedMain = isPed ? m_TotalMemoryUsedByPedModelsMain : m_TotalMemoryUsedByVehicleModelsMain;
		//s32 totalMemUsedVram = isPed ? m_TotalMemoryUsedByPedModelsVram : m_TotalMemoryUsedByVehicleModelsVram;
		u32 ageForRemoval = isPed ? s_pedAgeForRemoval : s_vehicleAgeForRemoval;

		u32 curTimeMs = fwTimer::GetTimeInMilliseconds();
		if ((nextRemovalTime < curTimeMs)/* && ((totalMemUsedMain >= memThresholdMain) || totalMemUsedVram >= memThresholdVram)*/)
		{
			s32 numInappropriate = isPed ? m_InAppropriateLoadedPeds.CountMembers() : m_InAppropriateLoadedCars.CountMembers();

			u32 potentialModelIdx = fwModelId::MI_INVALID;
			u32 oldestTime = ~0U;
			for (u32 i = 0; i < MAX_STREAMED_IN_MODELS; ++i)
			{
				if (!CModelInfo::IsValidModelInfo(m_StreamedInModelData[i].m_ModelIndex))
					continue;

				if ((isPed && CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex)))->GetModelType() != MI_TYPE_PED) ||
					(!isPed && CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex)))->GetModelType() != MI_TYPE_VEHICLE))
					continue;

				// ignore mission entities
				if ((CModelInfo::GetAssetStreamFlags(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex))) & STRFLAG_MISSION_REQUIRED) != 0)
					continue;

				// ignore discarded entries since they're already flagged as removed
				if ((isPed && m_DiscardedPeds.IsMemberInGroup(m_StreamedInModelData[i].m_ModelIndex)) || (!isPed && m_DiscardedCars.IsMemberInGroup(m_StreamedInModelData[i].m_ModelIndex)))
					continue;

				// ignore special peds
				if (isPed && m_SpecialLoadedPeds.IsMemberInGroup(m_StreamedInModelData[i].m_ModelIndex))
					continue;

#if __BANK
				if (m_StreamedInModelData[i].m_ModelIndex == m_vehicleOverrideIndex)
					continue;
#endif

				// don't try to remove the player important vehicle, it won't be removed since the player is in it
				if (CVehiclePopulation::ms_pInterestingVehicle)
					if (CVehiclePopulation::ms_pInterestingVehicle->GetModelIndex() == m_StreamedInModelData[i].m_ModelIndex)
						continue;

                // don't remove common vehicle if we don't have other ones in memory
                if (!canStreamCommonOut)
                    if (CPopCycle::GetPopGroups().IsVehIndexMember(CPopCycle::GetCommonVehicleGroup(), m_StreamedInModelData[i].m_ModelIndex))
                        continue;

                if (isPed)
                {
                    CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[i].m_ModelIndex)));
                    if (pmi->m_isRequestedAsDriver)
					{
                        continue;
					}
					else if (!pmi->CanStreamOutAsOldest())
					{
						// Models can be selected as never to be streamed out because of age.
						continue;
					}

					if (numInappropriate > 0 && !m_InAppropriateLoadedPeds.IsMemberInGroup(m_StreamedInModelData[i].m_ModelIndex))
						continue;
                }

				if (m_StreamedInModelData[i].m_TimeStreamedIn < oldestTime)
				{
					oldestTime = m_StreamedInModelData[i].m_TimeStreamedIn;
					potentialModelIdx = m_StreamedInModelData[i].m_ModelIndex;
				}
			}

			u32 age = curTimeMs - oldestTime;
			if (age > ageForRemoval)
			{
				modelId = potentialModelIdx;
			}

			if (isPed)
				s_lastPedAgeRemoval = curTimeMs;
			else
				s_lastVehicleAgeRemoval = curTimeMs;
		}
	}
	
	return modelId;
}

void CPopulationStreaming::GetCandidateVehiclesToStreamOutForSP(CLoadedModelGroup &candidatesToRemove, bool canStreamCommonVehicleOut)
{
    candidatesToRemove.Clear();

    if(!CPopCycle::HasValidCurrentPopAllocation())
		return;

    // If there are any vehicles in the appropriate list that don't belong in this zone we
    // earmark it for deletion.
    CLoadedModelGroup   tempList;
    tempList.Merge(&m_InAppropriateLoadedCars, &m_SpecialLoadedCars, &m_AppropriateLoadedCars);

	u32 curTime = fwTimer::GetTimeInMilliseconds();

	s32 numMembers = tempList.CountMembers();
    for (int i = 0; i < numMembers; i++)
    {
        u32 mi = tempList.GetMember(i);
		fwModelId modelId((strLocalIndex(mi)));

#if __BANK
		if (m_vehicleOverrideIndex != fwModelId::MI_INVALID)
		{
			if (mi != m_vehicleOverrideIndex)
			{
				candidatesToRemove.AddMember(mi);
			}
			continue;
		}
#endif

		u32 timeStreamedIn = 0;
		for (unsigned index = 0; index < MAX_STREAMED_IN_MODELS; index++)
		{
			if (m_StreamedInModelData[index].m_ModelIndex == mi)
			{
				timeStreamedIn = m_StreamedInModelData[index].m_TimeStreamedIn;
				break;
			}
		}

		if ((curTime - timeStreamedIn) < s_vehicleAgeForRemoval)
			continue; 

        if (!canStreamCommonVehicleOut)
            if (CPopCycle::GetPopGroups().IsVehIndexMember(CPopCycle::GetCommonVehicleGroup(), mi))
                continue;

		// candidates for removal are vehicle not belonging in popzone or all of them if budget level is 0 (except if they're law enforcement)
		// note that later we skip script vehicles so no need to skip them here
		if (!DoesCarModelBelongInCurrentPopulationZone(mi) || s_MemForVehiclesLevel == 0)
		{
			candidatesToRemove.AddMember(mi);
		}
    }
}

bool CPopulationStreaming::StreamOneCarOut(bool bForce)
{
    s32 MinTimesUsed = 8;       // Only remove model if used a reasonable number of times.
    strLocalIndex ModelIndexToBeRemoved = strLocalIndex(fwModelId::MI_INVALID);

    if(bForce)
        MinTimesUsed = -1;

    // Count the number of cars we're trying to remove.
    // Only do one at a time.
    s32 numCurrentlyBeingPhasedOut = 0;

	const CLoadedModelGroup* pGroupContainingPhasedOutCars = NetworkInterface::IsGameInProgress() ? &m_AppropriateLoadedCars : &m_DiscardedCars;

	const s32 numCarsInGroupContainingPhasedOutCars = pGroupContainingPhasedOutCars->CountMembers();

    for (int C = 0; C < numCarsInGroupContainingPhasedOutCars; C++)
    {
        const strLocalIndex mi = strLocalIndex(pGroupContainingPhasedOutCars->GetMember(C));
		fwModelId modelId(mi);
        if (!CModelInfo::GetAssetRequiredFlag(modelId))
        {
            numCurrentlyBeingPhasedOut++;
        }
    }
    if (numCurrentlyBeingPhasedOut > 0)
    {
        return false;
    }

	const s32 numAppropriateLoadedCars = m_AppropriateLoadedCars.CountMembers();

    for (int C = 0; C < numAppropriateLoadedCars; C++)
    {
        u32 ModelIndex = m_AppropriateLoadedCars.GetMember(C);

#if __BANK
			if (ModelIndex == m_vehicleOverrideIndex)
				continue;
#endif

        CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)));

        // Taxis won't be streamed out if they are required
        //if ((!CPopCycle::AreTaxisRequired()) || !CVehicle::IsTaxiModelId(fwModelId(ModelIndex)))
        {
            // Pick the car that has been used most times so far. (More chance that the player has seen it)
            // Take the vehicles frequency into account so that rare vehicle also will get streamed out.
            s32 scaledTimesUsed = pModelInfo->GetNumTimesUsed() * 100 / pModelInfo->GetVehicleFreq();
            if (scaledTimesUsed > MinTimesUsed)
            {
                ModelIndexToBeRemoved = ModelIndex;
                MinTimesUsed = scaledTimesUsed;
            }
        }
    }

	fwModelId modelIdToBeRemoved(ModelIndexToBeRemoved);
    if(modelIdToBeRemoved.IsValid())
    {
		Assert(CModelInfo::HaveAssetsLoaded(modelIdToBeRemoved));
        CModelInfo::ClearAssetRequiredFlag(modelIdToBeRemoved, STRFLAG_DONTDELETE);
		if(!NetworkInterface::IsGameInProgress())
			AddDiscardedCar(ModelIndexToBeRemoved.Get());

        if(!CModelInfo::GetAssetRequiredFlag(modelIdToBeRemoved))
            return true;
    }
    return false;
}

bool CPopulationStreaming::StreamOnePedOut(bool bForce)
{
    s32 MinTimesUsed = 8;       // Only remove model if used a reasonable number of times.
    strLocalIndex ModelIndexToBeRemoved = strLocalIndex(fwModelId::MI_INVALID);

    if(bForce)
        MinTimesUsed = -1;

    // Count the number of peds we're trying to remove.
    // Only do one at a time.
    s32 numCurrentlyBeingPhasedOut = 0;
	s32 numMembers = m_AppropriateLoadedPeds.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        strLocalIndex mi = strLocalIndex(m_AppropriateLoadedPeds.GetMember(C));
		fwModelId modelId(mi);
        if (!CModelInfo::GetAssetRequiredFlag(modelId))
        {
            numCurrentlyBeingPhasedOut++;
        }
    }
    if (numCurrentlyBeingPhasedOut > 0)
    {
        return false;
    }

    for (int C = 0; C < numMembers; C++)
    {
        strLocalIndex    ModelIndex = strLocalIndex(m_AppropriateLoadedPeds.GetMember(C));
        CPedModelInfo   *pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)));

		if (pModelInfo->m_isRequestedAsDriver)
			continue;

        // Pick the car that has been used most times so far. (More chance that the player has seen it)
        if (pModelInfo->GetNumTimesUsed() > MinTimesUsed)
        {
            ModelIndexToBeRemoved = ModelIndex;
            MinTimesUsed = pModelInfo->GetNumTimesUsed();
        }
    }

	fwModelId modelIdToBeRemoved(ModelIndexToBeRemoved);
    if(modelIdToBeRemoved.IsValid())
    {
        Assert(CModelInfo::HaveAssetsLoaded(modelIdToBeRemoved));
        CModelInfo::ClearAssetRequiredFlag(modelIdToBeRemoved, STRFLAG_DONTDELETE);
		if (!NetworkInterface::IsGameInProgress())
			AddDiscardedPed(ModelIndexToBeRemoved.Get());

        if(!CModelInfo::GetAssetRequiredFlag(modelIdToBeRemoved))
            return true;
    }
    return false;
}

void CPopulationStreaming::RemoveUnusedModels()
{
    // Vehicles and peds that are earmarked to be removed and that aren't used anymore
    // should be removed.
	
	bool bIsNetworkInProgress = NetworkInterface::IsGameInProgress();
    if ( !(fwTimer::GetSystemFrameCount() & 31) || ((AreWeOverBudget(MTT_PED) || AreWeOverBudget(MTT_VEHICLE)) && !bIsNetworkInProgress) )
    {
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::POPSTREAMER);
		
        CLoadedModelGroup allVehs;
        allVehs.Merge(&m_DiscardedCars, &m_InAppropriateLoadedCars, &m_SpecialLoadedCars, &m_AppropriateLoadedCars);
        CLoadedModelGroup tempList;
		tempList.Merge(&allVehs, &m_LoadedBoats);

		s32 numMembers = tempList.CountMembers();
        for (int i = 0; i < numMembers; i++)
        {
            strLocalIndex ModelIndex = strLocalIndex(tempList.GetMember(i));

			fwModelId modelId(ModelIndex);
			if (!modelId.IsValid())
				break;

			CVehicleModelInfo* pVMI = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

            // Handle ones that are to be streamed out.
            if (!CModelInfo::GetAssetRequiredFlag(modelId) &&
                pVMI->GetNumRefs() == 0)
            {
                Assertf(pVMI->GetDrawable(), "Model %s expected to be in memory but is not loaded", pVMI->GetModelName());
                CModelInfo::RemoveAssets(modelId);
            }
        }

        tempList.Merge(&m_DiscardedPeds, &m_InAppropriateLoadedPeds, &m_SpecialLoadedPeds, &m_AppropriateLoadedPeds);
		numMembers = tempList.CountMembers();
        for (int i = 0; i < numMembers; i++)
        {
            strLocalIndex ModelIndex = strLocalIndex(tempList.GetMember(i));

			fwModelId modelId(ModelIndex);
			if (!modelId.IsValid())
				break;

			CPedModelInfo *pPedModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);

            // Don't take into account the ones that are to be streamed out.
            if (!CModelInfo::GetAssetRequiredFlag(modelId) &&
                pPedModelInfo->GetNumRefs() == 0 && !pPedModelInfo->m_isRequestedAsDriver)
            {
                Assert(CModelInfo::GetBaseModelInfo(modelId)->GetDrawable());        // Make sure it is loaded in at this point
                CModelInfo::RemoveAssets(modelId);
            }
        }
    }
}

void CPopulationStreaming::UpdateHDCosts()
{
	if ((fwTimer::GetSystemFrameCount() & 15))
	{
		return;
	}

	// vehicles
	CLoadedModelGroup cars;
	cars.Merge(&m_AppropriateLoadedCars, &m_InAppropriateLoadedCars, &m_SpecialLoadedCars, &m_LoadedBoats);
	CLoadedModelGroup allLoadedCars;
	allLoadedCars.Merge(&cars, &m_DiscardedCars);

	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);

	s32 numCars = allLoadedCars.CountMembers();
	for (s32 i = 0; i < numCars; ++i)
	{
		u32 modelIndex = allLoadedCars.GetMember(i);
		if (!CModelInfo::IsValidModelInfo(modelIndex) || CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelType() != MI_TYPE_VEHICLE)
			continue;

		CVehicleModelInfo* modelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		// find streamed data
		unsigned index = 0;
		bool found = false;
		for(index = 0; index < MAX_STREAMED_IN_MODELS; index++)
		{
			if (m_StreamedInModelData[index].m_ModelIndex == modelIndex)
			{
				found = true;
				break;
			}
		}
		if (!found)
			continue;
		
		if (modelInfo->GetAreHDFilesLoaded() && !m_StreamedInModelData[index].m_IsHD)
		{
			deps.ResetCount();
			fwModelId modelId((strLocalIndex(modelIndex)));
			CModelInfo::GetObjectAndDependencies(modelId, deps, NULL, 0);

			strIndex hdTxdIndex;
			if (modelInfo->GetHDTxdIndex() > -1)
			{
				hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));
			}
			strIndex hdFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDFragmentIndex()));

			// if hd assets aren't dependencies, add them to the memory. some vehicles don't have hd assets so these
			// indices are the regular txd/frag indices, don't want to count them twice
			m_StreamedInModelData[index].m_HDAssetSizeMain = 0;
			m_StreamedInModelData[index].m_HDAssetSizeVram = 0;

 			if (deps.Find(hdTxdIndex) == -1)
 			{
				if (modelInfo->GetHDTxdIndex() > -1 && CStreaming::HasObjectLoaded(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId()) )
				{
	 				m_StreamedInModelData[index].m_HDAssetSizeMain = CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
					m_StreamedInModelData[index].m_HDAssetSizeVram = CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				}
			}

			if (deps.Find(hdFragIndex) == -1)
			{
				m_StreamedInModelData[index].m_HDAssetSizeMain += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
				m_StreamedInModelData[index].m_HDAssetSizeVram += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
			}

			AddTotalMemoryUsed(m_StreamedInModelData[index].m_HDAssetSizeMain, m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_VEHICLE);

			m_StreamedInModelData[index].m_IsHD = true;
			streamDebugf1("UpdateHDCosts: Found HD assets for vehicle '%s', added M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
		}	
		else if (!modelInfo->GetAreHDFilesLoaded() && m_StreamedInModelData[index].m_IsHD)
		{
			streamDebugf1("UpdateHDCosts: Removing HD assets for vehicle '%s', removed M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
			AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_VEHICLE);
			m_StreamedInModelData[index].m_IsHD = false;
			m_StreamedInModelData[index].m_HDAssetSizeMain = 0;
			m_StreamedInModelData[index].m_HDAssetSizeVram = 0;
		}
	}

	// peds
	CLoadedModelGroup allLoadedPeds;
	allLoadedPeds.Merge(&m_AppropriateLoadedPeds, &m_InAppropriateLoadedPeds, &m_SpecialLoadedPeds, &m_DiscardedPeds);

	s32 numPeds = allLoadedPeds.CountMembers();
	for (s32 i = 0; i < numPeds; ++i)
	{
		u32 modelIndex = allLoadedPeds.GetMember(i);
		if (!CModelInfo::IsValidModelInfo(modelIndex) || CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelType() != MI_TYPE_PED)
			continue;

		CPedModelInfo* modelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		// find streamed data
		unsigned index = 0;
		bool found = false;
		for(index = 0; index < MAX_STREAMED_IN_MODELS; index++)
		{
			if (m_StreamedInModelData[index].m_ModelIndex == modelIndex)
			{
				found = true;
				break;
			}
		}
		if (!found)
			continue;

		if (modelInfo->GetAreHDFilesLoaded() && !m_StreamedInModelData[index].m_IsHD)
		{
			deps.ResetCount();
			fwModelId modelId((strLocalIndex(modelIndex)));
			CModelInfo::GetObjectAndDependencies(modelId, deps, NULL, 0);

			strIndex hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

			// if hd assets aren't dependencies, add them to the memory. some peds don't have hd assets so these
			// indices are the regular txd indices, don't want to count them twice
			if (deps.Find(hdTxdIndex) == -1)
			{
				m_StreamedInModelData[index].m_HDAssetSizeMain = CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				m_StreamedInModelData[index].m_HDAssetSizeVram = CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
			}

			AddTotalMemoryUsed(m_StreamedInModelData[index].m_HDAssetSizeMain, m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_PED);

			m_StreamedInModelData[index].m_IsHD = true;
			streamDebugf1("UpdateHDCosts: Found HD assets for ped '%s', added M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
		}	
		else if (!modelInfo->GetAreHDFilesLoaded() && m_StreamedInModelData[index].m_IsHD)
		{
			streamDebugf1("UpdateHDCosts: Removing HD assets for ped '%s', removed M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
			AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_PED);
			m_StreamedInModelData[index].m_IsHD = false;
			m_StreamedInModelData[index].m_HDAssetSizeMain = 0;
			m_StreamedInModelData[index].m_HDAssetSizeVram = 0;
		}
	}

	// weapons
	for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS; index++)
	{
		if (m_StreamedInModelData[index].m_IsWeapon)
		{
			fwModelId modelId(strLocalIndex(m_StreamedInModelData[index].m_ModelIndex));
			Assert(CModelInfo::IsValidModelInfo(m_StreamedInModelData[index].m_ModelIndex));
			Assert(CModelInfo::GetBaseModelInfo(modelId)->GetModelType() == MI_TYPE_WEAPON);

			CWeaponModelInfo* modelInfo = (CWeaponModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

			if (modelInfo->GetAreHDFilesLoaded() && !m_StreamedInModelData[index].m_IsHD)
			{
				deps.ResetCount();
				CModelInfo::GetObjectAndDependencies(modelId, deps, NULL, 0);

				strIndex hdTxdIndex; 
				if (modelInfo->GetHDTxdIndex() > -1)
				{
					hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));
				}

				// if hd assets aren't dependencies, add them to the memory. some peds don't have hd assets so these
				// indices are the regular txd indices, don't want to count them twice
				if (deps.Find(hdTxdIndex) == -1)
				{
					if (modelInfo->GetHDTxdIndex() > -1)
					{
						m_StreamedInModelData[index].m_HDAssetSizeMain = CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
						m_StreamedInModelData[index].m_HDAssetSizeVram = CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
					}
				}

				AddTotalMemoryUsed(m_StreamedInModelData[index].m_HDAssetSizeMain, m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_WEAPON);

				m_StreamedInModelData[index].m_IsHD = true;
				streamDebugf1("UpdateHDCosts: Found HD assets for weapon '%s', added M%dK V%dK", CModelInfo::GetBaseModelInfo(modelId)->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
			}	
			else if (!modelInfo->GetAreHDFilesLoaded() && m_StreamedInModelData[index].m_IsHD)
			{
				streamDebugf1("UpdateHDCosts: Removing HD assets for weapon '%s', removed M%dK V%dK", CModelInfo::GetBaseModelInfo(modelId)->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
				AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_WEAPON);
				m_StreamedInModelData[index].m_IsHD = false;
				m_StreamedInModelData[index].m_HDAssetSizeMain = 0;
				m_StreamedInModelData[index].m_HDAssetSizeVram = 0;
			}
		}
	}
}

u32 CPopulationStreaming::ChooseCarModelToLoad(s32* groupArray, s32 numGroups, u32 restrictions)
{
	const u32 MAX_NUM_VEH_MODELS = 164;
	sStreamData vehs[MAX_NUM_VEH_MODELS];
	u32 numVehs = 0;

    bool bDuringOfficeHours = ((CClock::GetHour() > 7) && (CClock::GetHour() < 5));
	u32 maxAmbientVehs = (u32)(CVehiclePopulation::GetDesiredNumberAmbientVehicles() + 0.5f);

	for (s32 g = 0; g < numGroups; ++g)
	{
		const u32 numVehsInGroup = CPopCycle::GetPopGroups().GetNumVehs(groupArray[g]);

		for (u32 vehIndex = 0; vehIndex < numVehsInGroup; vehIndex++)
		{
			u32 modelIndex = CPopCycle::GetPopGroups().GetVehIndex(groupArray[g], vehIndex);
			fwModelId modelId((strLocalIndex(modelIndex)));
			if (!CModelInfo::HaveAssetsLoaded(modelId) || !IsModelDataStreamedIn(modelIndex) || m_DiscardedCars.IsMemberInGroup(modelIndex))
			{
				if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(modelIndex))
					continue;

				if (IsIncompatibleWithAlreadyLoadedCar(modelIndex))
					continue;

				CVehicleModelInfo *pModelInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetBaseModelInfo(modelId));
				Assert( pModelInfo );
				if (!bDuringOfficeHours && (!pModelInfo || pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ONLY_DURING_OFFICE_HOURS)) )
					continue;

				u32 vehFlags = 0;
				if (pModelInfo->GetIsBoat())
					vehFlags |= VSR_BOAT;
				if (!pModelInfo->GetIsBike())
					vehFlags |= VSR_NO_BIKE;
				if (CPopCycle::GetPopGroups().IsVehIndexMember(CPopCycle::GetCommonVehicleGroup(), modelIndex) && pModelInfo->GetVehicleMaxNumber() >= maxAmbientVehs)
					vehFlags |= VSR_COMMON_VEH;

				if (!Verifyf(numVehs < MAX_NUM_VEH_MODELS, "There's more than %d different vehicle models in this pop zone, bump the array size please!", MAX_NUM_VEH_MODELS))
					break;

				sStreamData& newVeh = vehs[numVehs++];
				newVeh.flags = vehFlags;
				newVeh.lastTimeUsed = pModelInfo->GetLastTimeUsed();
				newVeh.mi = modelIndex;
				newVeh.groupIndex = (s16)g;
				newVeh.randVal = (s16)fwRandom::GetRandomNumber();

				// if the preferred group is set, change its index to -1 to rank the highest when sorting
				// just to raise the chance of getting a vehicle from this group
				if (CPopCycle::GetPreferredGroupForVehs() != -1 && CPopCycle::GetPreferredGroupForVehs() == groupArray[g])
					newVeh.groupIndex = -1;
			}
		}
	}

	// sort first by group index then by last time used and return the first one (the one not seen for the longest amount of time)
	// that matches the restrictions passed in
	// we sort by group first to prioritize vehicles in the earlier groups, as these are the ones most needing a new vehicle based on the percentages
	qsort(vehs, numVehs, sizeof(sStreamData), StreamDataCompare);
	for (u32 i = 0; i < numVehs; ++i)
	{
		if (vehs[i].mi == fwModelId::MI_INVALID)
			continue;

		if ((vehs[i].flags & restrictions) == restrictions)
		{
			if (vehs[i].groupIndex == -1)
				CPopCycle::SetPreferredGroupForVehs(-1);
			return vehs[i].mi;
		}
	}

	if (vehs[0].groupIndex == (s16)CPopCycle::GetPreferredGroupForVehs())
		CPopCycle::SetPreferredGroupForVehs(-1);

	return vehs[0].mi;
}

bool  CPopulationStreaming::IsIncompatibleWithAlreadyLoadedCar(u32 model)
{
	CVehicleModelInfo* pVehModelInfo = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(model))));
	Assert( pVehModelInfo );
	if( !pVehModelInfo )
		return false;

    // prevent too many bikes from being streamed in
    if (pVehModelInfo && pVehModelInfo->GetIsBike())
    {
        if (m_AppropriateLoadedCars.CountMembers() <= 2)
        {
            return true;
        }

		s32 numMembers = m_AppropriateLoadedCars.CountMembers();
        for (int i = 0; i < numMembers; i++)
        {
            strLocalIndex ModelIndex = strLocalIndex(m_AppropriateLoadedCars.GetMember(i));
            if (((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetIsBike())
            {
                return true;
            }
        }
    }

    // prevent too many boring vehicles being streamed in
    const u32 boringVehiclesGroupNameHash = ATSTRINGHASH("VEH_BORING", 0x047cb2f40);
    u32 popGroupIndexOfBoringVehicles = 0;
    bool boringVehiclePopGroupExists = CPopCycle::GetPopGroups().FindVehGroupFromNameHash(boringVehiclesGroupNameHash, popGroupIndexOfBoringVehicles);
    if(!boringVehiclePopGroupExists)
    {
        return false;
    }

    if (CPopCycle::GetPopGroups().IsVehIndexMember(popGroupIndexOfBoringVehicles, model))
    {
        int numLoaded = 0;
        // Count the number of boring saloons already loaded.
        for (u32 i = 0; i < CPopCycle::GetPopGroups().GetNumVehs(popGroupIndexOfBoringVehicles); i++)
        {
            u32 mi = CPopCycle::GetPopGroups().GetVehIndex(popGroupIndexOfBoringVehicles, i);

			fwModelId modelId((strLocalIndex(mi)));
            if (CModelInfo::HaveAssetsLoaded(modelId) && CModelInfo::GetAssetRequiredFlag(modelId) )
            {
                numLoaded++;
            }
        }
        if (numLoaded >= 1)
        {
            return true;
        }
    }

    return false;
}

u32 CPopulationStreaming::ChoosePedModelToLoad(s32* groupArray, s32 numGroups, u32 restrictions)
{
	const u32 MAX_NUM_PED_MODELS = 128;
	sStreamData peds[MAX_NUM_PED_MODELS];
	u32 numPeds = 0;

	for (s32 g = 0; g < numGroups; ++g)
	{
		const u32 numPedsInGroup = CPopCycle::GetPopGroups().GetNumPeds(groupArray[g]);

		for (u32 pedIndex = 0; pedIndex < numPedsInGroup; pedIndex++)
		{
			u32 modelIndex = CPopCycle::GetPopGroups().GetPedIndex(groupArray[g], pedIndex);
			fwModelId modelId((strLocalIndex(modelIndex)));
			if (!CModelInfo::HaveAssetsLoaded(modelId) || !IsModelDataStreamedIn(modelIndex) || m_DiscardedPeds.IsMemberInGroup(modelIndex))
			{
				if (!CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(modelIndex))
				{
					u32 pedFlags = 0;
					CPedModelInfo *pModelInfo = static_cast<CPedModelInfo *>(CModelInfo::GetBaseModelInfo(modelId));
					if (pModelInfo->GetPersonalitySettings().GetIsMale())
						pedFlags |= PSR_MALE;
					else
						pedFlags |= PSR_FEMALE;
					if (CanPedModelDrive(modelIndex))
						pedFlags |= PSR_DRIVER;
					if (CanPedModelRideBike(modelIndex))
						pedFlags |= PSR_BIKER;

					if (!Verifyf(numPeds < MAX_NUM_PED_MODELS, "There's more than %d different ped models in this pop zone, bump the array size please!", MAX_NUM_PED_MODELS))
						break;

					sStreamData& newPed = peds[numPeds++];
					newPed.flags = pedFlags;
					newPed.lastTimeUsed = pModelInfo->GetLastTimeUsed();
					newPed.mi = modelIndex;
					newPed.groupIndex = (s16)g;
					newPed.randVal = (s16)fwRandom::GetRandomNumber();
				}
			}
		}
	}

	// sort peds first by group index then by last time used and return the first one (the one not seen for the longest amount of time)
	// that matches the restrictions passed in
	// we sort by group first to prioritize peds in the earlier groups, as these are the ones most needing a new ped based on the percentages
	qsort(peds, numPeds, sizeof(sStreamData), StreamDataCompare);
	for (u32 i = 0; i < numPeds; ++i)
	{
		if (peds[i].mi == fwModelId::MI_INVALID)
			continue;

		if ((peds[i].flags & restrictions) == restrictions)
			return peds[i].mi;
	}

	return peds[0].mi;
}

bool CPopulationStreaming::CanPedModelDrive(u32 mi)
{
	if (CModelInfo::IsValidModelInfo(mi))
	{
		CPedModelInfo *pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
		if ( (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_POOR_CAR)) && (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_AVERAGE_CAR)) && (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_RICH_CAR)) &&
			(!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_BIG_CAR)) )
		{
			return false;
		}

		return true;
	}

	return false;
}

bool CPopulationStreaming::CanPedModelRideBike(u32 mi)
{
	if (CModelInfo::IsValidModelInfo(mi))
	{
		CPedModelInfo *pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
		if (pmi->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_MOTORCYCLE) && (pmi->HasHelmetProp() || pmi->GetCanRideBikeWithNoHelmet()))
		{
			return true;
		}
	}
	return false;
}

void CPopulationStreaming::UpdateModelsRequiredForNetwork()
{
    u32 networkTimeInterval = GetCurrentNetworkTimeInterval();

    bool popScheduleChanged  = CPopCycle::GetCurrentZoneIndex() != m_LastZoneUsedForStreamingNetModels;
    bool timeAdvanced        = networkTimeInterval              != m_LastTimeUsedForStreamingNetPeds;
    
    if(m_PedModelsRequiredForNetwork.IsEmpty() || timeAdvanced || popScheduleChanged)
    {
        u32 adjustedModelStartOffset = GetAdjustedModelStartOffset(PED_MODEL_OFFSET, networkTimeInterval, popScheduleChanged);

        if((adjustedModelStartOffset != m_LastTimeUsedForStreamingNetPeds) || popScheduleChanged)
        {
            UpdatePedModelsRequiredForNetwork(adjustedModelStartOffset);

            m_LastTimeUsedForStreamingNetPeds = adjustedModelStartOffset;
        }
    }

    timeAdvanced = networkTimeInterval != m_LastTimeUsedForStreamingNetVehicles;

    if(m_VehicleModelsRequiredForNetwork.IsEmpty() || timeAdvanced || popScheduleChanged)
    {
        u32 adjustedModelStartOffset = GetAdjustedModelStartOffset(VEHICLE_MODEL_OFFSET, networkTimeInterval, popScheduleChanged);

        if((adjustedModelStartOffset != m_LastTimeUsedForStreamingNetVehicles) || popScheduleChanged)
        {
            UpdateVehicleModelsRequiredForNetwork(adjustedModelStartOffset);

            m_LastTimeUsedForStreamingNetVehicles = adjustedModelStartOffset;
        }
    }

    m_LastZoneUsedForStreamingNetModels = CPopCycle::GetCurrentZoneIndex();
}

u32 CPopulationStreaming::GetAdjustedModelStartOffset(ModelOffsetType offsetType, u32 desiredModelStartOffset, bool /*popScheduleChanged*/)
{
    u32                adjustedModelStartOffset = desiredModelStartOffset;
    u32                lowestModelStartOffset   = adjustedModelStartOffset;
    CLoadedModelGroup *inAppropriateModels      = 0;
    u32                lastTimeUsedForStreaming = 0;

    if(offsetType == PED_MODEL_OFFSET)
    {
        u32 lowestRemoteOffset = 0;

        if(NetworkInterface::GetLowestPedModelStartOffset(lowestRemoteOffset))
        {
            lowestModelStartOffset = lowestRemoteOffset;
        }

        inAppropriateModels      = &m_InAppropriateLoadedPeds;
        lastTimeUsedForStreaming = m_LastTimeUsedForStreamingNetPeds;
    }
    else
    {
        Assertf(offsetType == VEHICLE_MODEL_OFFSET, "Unexpected model offset type!");

        u32 lowestRemoteOffset = 0;

        if(NetworkInterface::GetLowestVehicleModelStartOffset(lowestRemoteOffset))
        {
            lowestModelStartOffset = lowestRemoteOffset;
        }

        inAppropriateModels      = &m_InAppropriateLoadedCars;
        lastTimeUsedForStreaming = m_LastTimeUsedForStreamingNetVehicles;
    }

    int      numInappropriateModels            = inAppropriateModels->CountMembers();
    unsigned numModelsAwaitingStreamingRemoval = 0;

    for(int index = 0; index < numInappropriateModels; index++)
    {
        strLocalIndex modelIndex = strLocalIndex(inAppropriateModels->GetMember(index));

        fwModelId modelId(modelIndex);

        if(modelId.IsValid())
        {
            bool requiredByScript = ((CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_MISSION_REQUIRED) != 0);

            if (!requiredByScript)
            {
                numModelsAwaitingStreamingRemoval++;
            }
        }
    }

    if(numModelsAwaitingStreamingRemoval >= MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL)
    {
        adjustedModelStartOffset = lastTimeUsedForStreaming;
    }
    else
    {
        u32 difference = desiredModelStartOffset - lastTimeUsedForStreaming;
        difference     = MIN(difference, MAX_NETWORK_MODELS_OF_TYPE_PENDING_STREAMING_REMOVAL);
        difference    -= numModelsAwaitingStreamingRemoval;

        adjustedModelStartOffset = lastTimeUsedForStreaming + difference;
        adjustedModelStartOffset %= MAX_NETWORK_MODEL_INTERVALS;
    }

    if(offsetType == PED_MODEL_OFFSET)
    {
        m_LocalAllowedPedModelStartOffset = adjustedModelStartOffset;
    }
    else
    {
        m_LocalAllowedVehicleModelStartOffset = adjustedModelStartOffset;
    }

    if(IsNetworkModelOffsetLower(lowestModelStartOffset, adjustedModelStartOffset))
    {
        adjustedModelStartOffset = lowestModelStartOffset;
    }

    return adjustedModelStartOffset;
}

#if __BANK

void CPopulationStreaming::LogModelsPendingRemovalInfo(ModelOffsetType offsetType, u32 desiredModelStartOffset, u32 adjustedModelStartOffset)
{
    if(desiredModelStartOffset != adjustedModelStartOffset)
    {
        if(offsetType == VEHICLE_MODEL_OFFSET)
        {
            NetworkDebug::LogVehiclesUsingInappropriateModels();
        }
        else if(offsetType == PED_MODEL_OFFSET)
        {
            NetworkDebug::LogPedsUsingInappropriateModels();
        }
    }
}

#endif // __BANK

u32 CPopulationStreaming::GetCurrentNetworkTimeInterval()
{
    const unsigned TIME_INTERVAL_TO_CHANGE_MODELS = 3 * 60 * 1000; // update the streamed models list every 3 minutes

    u32 networkTimeInterval = NetworkInterface::GetSyncedTimeInMilliseconds() / TIME_INTERVAL_TO_CHANGE_MODELS;
    networkTimeInterval    += s_NetworkModelVariations[m_NetworkModelVariationID].m_NetworkTimeOffset;
    networkTimeInterval    %= MAX_NETWORK_MODEL_INTERVALS;

    return networkTimeInterval;
}

void CPopulationStreaming::UpdatePedModelsRequiredForNetwork(u32 modelStartOffset)
{
    m_PedModelsRequiredForNetwork.Clear();

    CLoadedModelGroup ambientPedCandidates;
    CLoadedModelGroup gangPedCandidates;
    CPopCycle::GetNetworkPedModelsForCurrentZone(ambientPedCandidates, gangPedCandidates);

    const unsigned numAmbientPedModels  = ambientPedCandidates.CountMembers();
    const unsigned numGangPedModels     = gangPedCandidates.CountMembers();

    const unsigned targetNumGangPeds    = MIN(numGangPedModels, TARGET_NUM_GANG_PEDS);
    const unsigned targetNumAmbientPeds = MIN(numAmbientPedModels, (TARGET_NUM_AMBIENT_PEDS + (TARGET_NUM_GANG_PEDS - targetNumGangPeds)));

    if(numGangPedModels > 0)
    {
        unsigned gangPedStart = modelStartOffset % numGangPedModels;
        unsigned gangPedEnd   = gangPedStart + targetNumGangPeds;

        for(unsigned index = gangPedStart; index < gangPedEnd; index++)
        {
            unsigned pedIndex = index % numGangPedModels;
            m_PedModelsRequiredForNetwork.AddMember(gangPedCandidates.GetMember(pedIndex));
        }
    }
    
	bool hasDriver = false;
    if(numAmbientPedModels > 0)
    {
        unsigned ambientPedStart = modelStartOffset % numAmbientPedModels;
        unsigned ambientPedEnd   = ambientPedStart + targetNumAmbientPeds;

        for(unsigned index = ambientPedStart; index < ambientPedEnd; index++)
        {
            unsigned pedIndex = index % numAmbientPedModels;
            m_PedModelsRequiredForNetwork.AddMember(ambientPedCandidates.GetMember(pedIndex));
			hasDriver |= CanPedModelDrive(ambientPedCandidates.GetMember(pedIndex));
        }
    }

	if (!hasDriver)
	{
		for (s32 i = 0; i < numAmbientPedModels; ++i)
		{
			if (CanPedModelDrive(ambientPedCandidates.GetMember(i)))
			{
				m_PedModelsRequiredForNetwork.AddMember(ambientPedCandidates.GetMember(i));
				break;
			}
		}
	}
}

bool CPopulationStreaming::CanSpawnInMp(u32 modelIndex)
{
	if (modelIndex == MI_CAR_TAXI_CURRENT_1.GetModelIndex())
        return false;

	if (modelIndex == MI_CAR_TOWTRUCK.GetModelIndex())
        return false;

	if (modelIndex == MI_CAR_TOWTRUCK_2.GetModelIndex())
        return false;

	if (modelIndex == MI_CAR_FORKLIFT.GetModelIndex())
        return false;

    return true;
}

void CPopulationStreaming::UpdateVehicleModelsRequiredForNetwork(u32 modelStartOffset)
{
    m_VehicleModelsRequiredForNetwork.Clear();

    CLoadedModelGroup commonVehicles;
    CLoadedModelGroup zoneVehicles;
    CPopCycle::GetNetworkVehicleModelsForCurrentZone(commonVehicles, zoneVehicles);

	float multiplier = 1.f;
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS) && 0 // NO multiplier on networking
	multiplier = CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)		

    const unsigned numCommonVehicleModels = commonVehicles.CountMembers();
    const unsigned numZoneVehicleModels   = zoneVehicles.CountMembers();
    const unsigned targetNumCommonModels  = MIN(numCommonVehicleModels, (unsigned)((NUMBER_STREAMED_CARS_NETWORK - NUMBER_ZONE_SPECIFIC_CARS_NETWORK) * multiplier));
    unsigned targetNumZoneModels		  = MIN(numZoneVehicleModels,   (unsigned)(NUMBER_ZONE_SPECIFIC_CARS_NETWORK * multiplier));

	if (numCommonVehicleModels == 0)
		targetNumZoneModels = MIN(numZoneVehicleModels, (unsigned)(NUMBER_STREAMED_CARS_NETWORK * multiplier));

    if(numCommonVehicleModels > 0)
    {
        unsigned commonVehicleStart   = modelStartOffset % numCommonVehicleModels;
        unsigned numCommonModelsAdded = 0;

        for(unsigned index = 0; index < numCommonVehicleModels && numCommonModelsAdded < targetNumCommonModels; index++)
        {
            // adjust the vehicle to stream based off the number of shuffled indices we have and the number of possible models.
            // In the case where the number of models is greater than the number of indices, we split the list into blocks of models
            // that have the shuffled indices applied to them
            unsigned blockIndex        = (commonVehicleStart+index) % NUM_SHUFFLED_INDICES;
            unsigned shuffleBlockValue = (commonVehicleStart+index) - blockIndex;
            unsigned shuffledIndex     = shuffleBlockValue + s_NetworkModelVariations[m_NetworkModelVariationID].m_ShuffledIndices[blockIndex];
            unsigned vehicleIndex      = shuffledIndex % numCommonVehicleModels;

            u32 modelIndex = commonVehicles.GetMember(vehicleIndex);

            if (!CanSpawnInMp(modelIndex))
                continue;

            if(!m_VehicleModelsRequiredForNetwork.IsMemberInGroup(modelIndex))
            {
                m_VehicleModelsRequiredForNetwork.AddMember(modelIndex);
                numCommonModelsAdded++;
            }
        }
    }

    if(numZoneVehicleModels > 0 && targetNumZoneModels > 0)
    {
        unsigned zoneVehicleStart   = modelStartOffset % numZoneVehicleModels;
        unsigned numZoneModelsAdded = 0;

        for(unsigned index = 0; index < numZoneVehicleModels && numZoneModelsAdded < targetNumZoneModels; index++)
        {
            // adjust the vehicle to stream based off the number of shuffled indices we have and the number of possible models.
            // In the case where the number of models is greater than the number of indices, we split the list into blocks of models
            // that have the shuffled indices applied to them
            unsigned blockIndex        = (zoneVehicleStart+index) % NUM_SHUFFLED_INDICES;
            unsigned shuffleBlockValue = (zoneVehicleStart+index) - blockIndex;
            unsigned shuffledIndex     = shuffleBlockValue + s_NetworkModelVariations[m_NetworkModelVariationID].m_ShuffledIndices[blockIndex];
            unsigned vehicleIndex      = shuffledIndex % numZoneVehicleModels;

            u32 modelIndex = zoneVehicles.GetMember(vehicleIndex);

			if (!CanSpawnInMp(modelIndex))
				continue;

            if(!m_VehicleModelsRequiredForNetwork.IsMemberInGroup(modelIndex))
            {
                m_VehicleModelsRequiredForNetwork.AddMember(modelIndex);
                numZoneModelsAdded++;
            }
        }
    }
}

bool CPopulationStreaming::IsPedAvailableForSpawn(u32 modelIndex)
{
	if (modelIndex == fwModelId::MI_INVALID)
		return false;

	if (m_AppropriateLoadedPeds.IsMemberInGroup(modelIndex))
		return true;

	if (m_SpecialLoadedPeds.IsMemberInGroup(modelIndex))
		return true;

	if (m_InAppropriateLoadedPeds.IsMemberInGroup(modelIndex))
		return true;

	if (m_DiscardedPeds.IsMemberInGroup(modelIndex))
		return true;

	return false;
}

bool CPopulationStreaming::IsDriverAvailable(u32 modelIndex)
{
	if (modelIndex == fwModelId::MI_INVALID)
		return false;

	CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
	Assert(mi);

	// if no drivers are specified we don't care
	if (mi->GetDriverCount() == 0)
		return true;

	return IsPedAvailableForSpawn(CPedPopulation::FindSpecificDriverModelForCarToUse(fwModelId(strLocalIndex(modelIndex))).GetModelIndex());
}

void CPopulationStreaming::ReclassifyLoadedCars()
{
    CLoadedModelGroup   Temp1 = m_AppropriateLoadedCars;
    CLoadedModelGroup   Temp2 = m_InAppropriateLoadedCars;

    m_AppropriateLoadedCars.Clear();
    m_InAppropriateLoadedCars.Clear();

	s32 numMembers = Temp1.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = Temp1.GetMember(C);
            //m_AppropriateLoadedCars.AddMember(ModelIndex);

		if (IsCarModelNeededCurrently(ModelIndex))
		{
			m_AppropriateLoadedCars.AddMember(ModelIndex);
		}
		else
		{
			m_InAppropriateLoadedCars.AddMember(ModelIndex);
		}
    }

	numMembers = Temp2.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = Temp2.GetMember(C);
            //m_AppropriateLoadedCars.AddMember(ModelIndex);

		if (IsCarModelNeededCurrently(ModelIndex))
		{
			m_AppropriateLoadedCars.AddMember(ModelIndex);

			// Make sure any appropriate loaded car has a driver requested.
			RequestVehicleDriver(ModelIndex);
		}
		else
		{
			m_InAppropriateLoadedCars.AddMember(ModelIndex);
		}
    }
	CVehicleFactory::GetFactory()->RemoveVehGroupFromCache(m_InAppropriateLoadedCars);
}

void CPopulationStreaming::ReclassifyLoadedVehiclesForNetwork()
{
    CLoadedModelGroup tempList;
    tempList.Merge(&m_AppropriateLoadedCars, &m_InAppropriateLoadedCars);
    m_AppropriateLoadedCars.Clear();
    m_InAppropriateLoadedCars.Clear();

    int numCars = tempList.CountMembers();

    for(int index = 0; index < numCars; index++)
    {
        u32 modelIndex = tempList.GetMember(index);

        if(CModelInfo::IsValidModelInfo(modelIndex))
        {
            if(m_VehicleModelsRequiredForNetwork.IsMemberInGroup(modelIndex))
            {
                m_AppropriateLoadedCars.AddMember(modelIndex);

				// Make sure any appropriate loaded car has a driver requested.
				RequestVehicleDriver(modelIndex);
            }
            else
            {
                m_InAppropriateLoadedCars.AddMember(modelIndex);
            }
        }
    }
}

void CPopulationStreaming::ReclassifyLoadedPeds()
{
    CLoadedModelGroup   Temp1 = m_AppropriateLoadedPeds;
    CLoadedModelGroup   Temp2 = m_InAppropriateLoadedPeds;

    m_AppropriateLoadedPeds.Clear();
    m_InAppropriateLoadedPeds.Clear();

	s32 numMembers = Temp1.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = Temp1.GetMember(C);
        if(!CPed::IsSuperLodFromIndex(ModelIndex))
        {
            if (IsPedModelNeededCurrently(ModelIndex))
            {
                m_AppropriateLoadedPeds.AddMember(ModelIndex);
            }
            else
            {
                m_InAppropriateLoadedPeds.AddMember(ModelIndex);
            }
        }
		else
		{
			Assertf(false, "Found a superlod ped in one of the population lists!");
		}
    }

	numMembers = Temp2.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = Temp2.GetMember(C);
        if(!CPed::IsSuperLodFromIndex(ModelIndex))
        {
            if (IsPedModelNeededCurrently(ModelIndex))
            {
                m_AppropriateLoadedPeds.AddMember(ModelIndex);
            }
            else
            {
                m_InAppropriateLoadedPeds.AddMember(ModelIndex);
            }
        }
		else
		{
			Assertf(false, "Found a superlod ped in one of the population lists!");
		}
    }

	CPedFactory::GetFactory()->RemovePedGroupFromCache(m_InAppropriateLoadedPeds);
}

void CPopulationStreaming::ReclassifyLoadedPedsForNetwork()
{
    CLoadedModelGroup tempList;
    tempList.Merge(&m_AppropriateLoadedPeds, &m_InAppropriateLoadedPeds);
    m_AppropriateLoadedPeds.Clear();
    m_InAppropriateLoadedPeds.Clear();

    int numPeds = tempList.CountMembers();

    for(int index = 0; index < numPeds; index++)
    {
        u32 modelIndex = tempList.GetMember(index);

        if(CModelInfo::IsValidModelInfo(modelIndex))
        {
            if(m_PedModelsRequiredForNetwork.IsMemberInGroup(modelIndex))
            {
                m_AppropriateLoadedPeds.AddMember(modelIndex);
            }
            else
            {
                m_InAppropriateLoadedPeds.AddMember(modelIndex);
            }
        }
    }
}

u32 CPopulationStreaming::GetTotalNumberOfCarsStreamedIn() const
{
	const u32 discardedCars = m_DiscardedCars.CountMembers();
	const u32 appropriateCars = m_AppropriateLoadedCars.CountMembers();
	const u32 inappropriateCars = m_InAppropriateLoadedCars.CountMembers();
	const u32 specialCars = m_SpecialLoadedCars.CountMembers();
	const u32 boats = m_LoadedBoats.CountMembers();

	return discardedCars + appropriateCars + inappropriateCars + specialCars + boats;
}

void CPopulationStreaming::AddDiscardedCar(u32 modelIndex)
{
	m_DiscardedCars.AddMember(modelIndex);

	m_AppropriateLoadedCars.RemoveMember(modelIndex);
	m_InAppropriateLoadedCars.RemoveMember(modelIndex);

	Assert(!m_SpecialLoadedCars.IsMemberInGroup(modelIndex));
	m_SpecialLoadedCars.RemoveMember(modelIndex);
	Assert(!m_LoadedBoats.IsMemberInGroup(modelIndex));
	m_LoadedBoats.RemoveMember(modelIndex);
}

void CPopulationStreaming::ReinstateDiscardedCar(u32 modelIndex)
{
	m_DiscardedCars.RemoveMember(modelIndex);

	bool bIsBoat = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))))->GetVehicleType()==VEHICLE_TYPE_BOAT;
	bool bIsTrain = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))))->GetVehicleType()==VEHICLE_TYPE_TRAIN;
	bool bIsHeli = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))))->GetVehicleType()==VEHICLE_TYPE_HELI;
	bool bIsAmbulance = (modelIndex == MI_CAR_AMBULANCE);
	bool bIsFiretruck = (modelIndex == MI_CAR_FIRETRUCK);

	bool requestDriver = true;
	if (bIsHeli || bIsTrain || bIsAmbulance || bIsFiretruck)
	{
		m_SpecialLoadedCars.AddMember(modelIndex);

		// Don't request a driver for the members of m_SpecialLoadedCars, has to be done elsewhere
		// by users that need it.
		requestDriver = false;
	}
	else if (bIsBoat)
	{
		m_LoadedBoats.AddMember(modelIndex);

		// Only request drivers for boats if we actually want boats in the general population.
		// Otherwise, they are probably just needed for car generators or vehicle scenarios (which
		// will request drivers if needed).
		requestDriver = m_bBoatsNeeded;
	}
	else if (IsCarModelNeededCurrently(modelIndex))
	{
		m_AppropriateLoadedCars.AddMember(modelIndex);
	}
	else
	{
		m_InAppropriateLoadedCars.AddMember(modelIndex);

		// Car generator and scenario vehicles often end up here. We don't need drivers to be
		// requested for those, at this level of the code.
		requestDriver = false;
	}

	if(requestDriver)
	{
		RequestVehicleDriver(modelIndex);
	}
}

void CPopulationStreaming::AddDiscardedPed(u32 modelIndex)
{
	m_DiscardedPeds.AddMember(modelIndex);

	m_AppropriateLoadedPeds.RemoveMember(modelIndex);
	m_InAppropriateLoadedPeds.RemoveMember(modelIndex);
	Assert(!m_SpecialLoadedPeds.IsMemberInGroup(modelIndex) || IsPedModelNeededCurrently(modelIndex));
	m_SpecialLoadedPeds.RemoveMember(modelIndex);  
}

void CPopulationStreaming::ReinstateDiscardedPed(u32 modelIndex)
{
	m_DiscardedPeds.RemoveMember(modelIndex);
	if (AddPedModelToCorrectList(modelIndex))
	{
		// only add this flag to appropriate or inappropriate peds. any other types might not have the flag removed by us
		CModelInfo::SetAssetRequiredFlag(fwModelId(strLocalIndex(modelIndex)), STRFLAG_DONTDELETE);
	}
	Assertf(m_AppropriateLoadedPeds.IsMemberInGroup(modelIndex) ||
		m_InAppropriateLoadedPeds.IsMemberInGroup(modelIndex) ||
		m_SpecialLoadedPeds.IsMemberInGroup(modelIndex), "Ped has been reinstated but isn't in any of the lists!");
}

s32 CPopulationStreaming::AddStreamedInModelData(u32 modelIndex)
{
    bool added = false;

    Assertf(!IsModelDataStreamedIn(modelIndex), "Model data already tracked is being added again!");

	s32 ret = -1;
    for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && !added; index++)
    {
        if(!CModelInfo::IsValidModelInfo(m_StreamedInModelData[index].m_ModelIndex))
        {
            m_StreamedInModelData[index].m_ModelIndex           = modelIndex;
            m_StreamedInModelData[index].m_TimeStreamedIn       = fwTimer::GetTimeInMilliseconds();
            m_StreamedInModelData[index].m_TimeNoLongerRequired = 0;
			m_StreamedInModelData[index].m_HDAssetSizeMain		= 0;
			m_StreamedInModelData[index].m_HDAssetSizeVram		= 0;
			m_StreamedInModelData[index].m_IsHD					= false;
            added = true;
			ret = (s32)index;

			if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[index].m_ModelIndex)))->GetModelType() == MI_TYPE_WEAPON)
				m_StreamedInModelData[index].m_IsWeapon = true;
        }
    }

#if __BANK
    if (!Verifyf(added, "Failed to add streamed in model data!"))
	{
		for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && !added; index++)
		{
			if(CModelInfo::IsValidModelInfo(m_StreamedInModelData[index].m_ModelIndex))
			{
				Displayf("%s: %d\n", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_StreamedInModelData[index].m_ModelIndex)))->GetModelName(),
					m_StreamedInModelData[index].m_ModelIndex);
			}
			else
			{
				Displayf("INVALID: %d\n", m_StreamedInModelData[index].m_ModelIndex);
			}
		}
	}
#endif // __BANK

	return ret;
}

bool CPopulationStreaming::RemoveStreamedInModelData(u32 modelIndex)
{
    bool removed = false;

    for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && !removed; index++)
    {
        if(m_StreamedInModelData[index].m_ModelIndex == modelIndex)
        {
			if (m_StreamedInModelData[index].m_IsHD && (m_StreamedInModelData[index].m_HDAssetSizeMain > 0 || m_StreamedInModelData[index].m_HDAssetSizeVram))
			{
				streamDebugf1("RemoveStreamedInModelData: Removing HD assets for '%s'! Removed M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName(), m_StreamedInModelData[index].m_HDAssetSizeMain/1024, m_StreamedInModelData[index].m_HDAssetSizeVram/1024);
				if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelType() == MI_TYPE_VEHICLE)
				{
					AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_VEHICLE);
				}
				else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelType() == MI_TYPE_PED)
				{
					AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_PED);
				}
				else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelType() == MI_TYPE_WEAPON)
				{
					AddTotalMemoryUsed(-(s32)m_StreamedInModelData[index].m_HDAssetSizeMain, -(s32)m_StreamedInModelData[index].m_HDAssetSizeVram, MTT_WEAPON);
				}
				else
				{
					Assertf(false, "Unexpected model type with HD assets (%s)", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetModelName());
				}
			}

			streamDebugf1("RemoveStreamedInModelData: Removing data streamed in at %d (%d ms ago)", m_StreamedInModelData[index].m_TimeStreamedIn, fwTimer::GetTimeInMilliseconds() - m_StreamedInModelData[index].m_TimeStreamedIn);

            m_StreamedInModelData[index].m_ModelIndex           = fwModelId::MI_INVALID;
            m_StreamedInModelData[index].m_TimeStreamedIn       = 0;
            m_StreamedInModelData[index].m_TimeNoLongerRequired = 0;
			m_StreamedInModelData[index].m_HDAssetSizeMain		= 0;
			m_StreamedInModelData[index].m_HDAssetSizeVram		= 0;
			m_StreamedInModelData[index].m_IsHD					= false;
            m_StreamedInModelData[index].m_IsWeapon				= false;
#if DEBUG_POPULATION_MEMORY
			m_StreamedInModelData[index].m_mainUsed				= 0;
			m_StreamedInModelData[index].m_vramUsed				= 0;
#endif // DEBUG_POPULATION_MEMORY
            removed = true;
        }
    }

    Assertf(!IsModelDataStreamedIn(modelIndex), "Model data still tracked after removal!");

	return removed;
}

bool CPopulationStreaming::IsModelDataStreamedIn(u32 modelIndex)
{
    bool found = false;

    for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && !found; index++)
    {
        if(m_StreamedInModelData[index].m_ModelIndex == modelIndex)
        {
            found = true;
        }
    }

	return found;
}

void CPopulationStreaming::AddStreamedInComponent(u32 modelIndex, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type)
{
	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);

	fwModelId modelId((strLocalIndex(modelIndex)));
	CModelInfo::GetObjectAndDependencies(modelId, deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());

	AddStreamedInDependencies(deps, componentList, totalVirtual, totalPhysical, type);
}

void CPopulationStreaming::AddStreamedInDependencies(const atArray<strIndex>& deps, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type)
{
	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		streamDebugf1("AddStreamedInDependencies: dep %d: %s", i, strStreamingEngine::GetObjectName(deps[i]));

		s32 entrySlot = -1;
		s32 firstEmptySlot = -1;
		for (s32 f = 0; f < componentList.GetCount(); ++f)
		{
			if (firstEmptySlot == -1 && componentList[f].refCount == 0)
			{
				firstEmptySlot = f;
			}

			if (componentList[f].streamIdx == deps[i])
			{
				entrySlot = f;
				break;
			}
		}

		if (entrySlot == -1)
		{
			// the current strIndex wasn't found in our list, add it and increment memory usage
			if (firstEmptySlot != -1)
			{
				// reuse empty slot to avoid memory allocations when possible
				componentList[firstEmptySlot].streamIdx = deps[i];
				componentList[firstEmptySlot].refCount = 1;
			}
			else
			{
				sPopEntry newEntry = { deps[i], 1 };
				componentList.PushAndGrow(newEntry);
			}

			u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
			u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
			AddTotalMemoryUsed(virtualMemory, physicalMemory, type);
			streamDebugf1("[actual] AddStreamedInDependencies: dep %d: %s %d/%d", i, strStreamingEngine::GetObjectName(deps[i]), virtualMemory, physicalMemory);

			totalVirtual += virtualMemory;
			totalPhysical += physicalMemory;
		}
		else
		{
			Assert(deps[i] == componentList[entrySlot].streamIdx);

			// if we found the index in our list add its memory cost only if this is the first reference
			if (componentList[entrySlot].refCount == 0)
			{
				u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
				u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
				AddTotalMemoryUsed(virtualMemory, physicalMemory, type);

				totalVirtual += virtualMemory;
				totalPhysical += physicalMemory;
			}

			componentList[entrySlot].refCount++;
		}
	}
}

void CPopulationStreaming::RemoveStreamedInComponent(u32 modelIndex, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type)
{
	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);

	fwModelId modelId((strLocalIndex(modelIndex)));
	CModelInfo::GetObjectAndDependencies(modelId, deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());

	RemoveStreamedInDependencies(deps, componentList, totalVirtual, totalPhysical, type);

	switch (type)
	{
	case MTT_PED:
		{
			Assert(m_TotalMemoryUsedByPedModelsMain >= 0);
			m_TotalMemoryUsedByPedModelsMain = MAX(0, m_TotalMemoryUsedByPedModelsMain);
			Assert(m_TotalMemoryUsedByPedModelsVram >= 0);
			m_TotalMemoryUsedByPedModelsVram = MAX(0, m_TotalMemoryUsedByPedModelsVram);
		}
		break;
	case MTT_VEHICLE:
	case MTT_WEAPON:
		{
			Assert(m_TotalMemoryUsedByVehicleModelsMain >= 0);
			m_TotalMemoryUsedByVehicleModelsMain = MAX(0, m_TotalMemoryUsedByVehicleModelsMain);
			Assert(m_TotalMemoryUsedByVehicleModelsVram >= 0);
			m_TotalMemoryUsedByVehicleModelsVram = MAX(0, m_TotalMemoryUsedByVehicleModelsVram);
		}
		break;
	default:
		break;
	}
}

void CPopulationStreaming::RemoveStreamedInDependencies(const atArray<strIndex>& deps, atArray<sPopEntry>& componentList, u32& totalVirtual, u32& totalPhysical, eMemoryTrackType type)
{
	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		streamDebugf1("RemoveStreamedInDependencies: dep %d: %s", i, strStreamingEngine::GetObjectName(deps[i]));

		s32 entrySlot = -1;
		for (s32 f = 0; f < componentList.GetCount(); ++f)
		{
			if (componentList[f].streamIdx == deps[i])
			{
				entrySlot = f;
				break;
			}
		}

		if (entrySlot == -1 || componentList[entrySlot].refCount == 0)
		{
			continue;
		}

		Assert(deps[i] == componentList[entrySlot].streamIdx);
		if (componentList[entrySlot].refCount == 1)
		{
			u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
			u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
			AddTotalMemoryUsed(-(s32)virtualMemory, -(s32)physicalMemory, type);
			streamDebugf1("[actual] RemoveStreamedInDependencies: dep %d: %s %d/%d", i, strStreamingEngine::GetObjectName(deps[i]), virtualMemory, physicalMemory);

			totalVirtual += virtualMemory;
			totalPhysical += physicalMemory;
		}

		Assert(componentList[entrySlot].refCount > 0);
		componentList[entrySlot].refCount = MAX(0, componentList[entrySlot].refCount - 1);
	}
}

s32 CPopulationStreaming::GetTimeModelStreamedIn(u32 modelIndex)
{
    s32 time = 0;

    for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && time==0; index++)
    {
        if(m_StreamedInModelData[index].m_ModelIndex == modelIndex)
        {
            time = m_StreamedInModelData[index].m_TimeStreamedIn;
        }
    }

    return time;
}

void CPopulationStreaming::SetTimeModelStreamedIn(fwModelId PedModelId, s32 time)
{
	for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS; index++)
	{
		if(m_StreamedInModelData[index].m_ModelIndex == PedModelId.GetModelIndex())
		{
			m_StreamedInModelData[index].m_TimeStreamedIn = time;
			return;
		}
	}
}


void CPopulationStreaming::MarkModelNoLongerRequired(u32 modelIndex)
{
    bool found = false;

    for(unsigned index = 0; index < MAX_STREAMED_IN_MODELS && !found; index++)
    {
        if(m_StreamedInModelData[index].m_ModelIndex == modelIndex)
        {
            m_StreamedInModelData[index].m_TimeNoLongerRequired = fwTimer::GetTimeInMilliseconds();
            found                                               = true;
        }
    }
}

bool CPopulationStreaming::IsCarModelNeededCurrently(u32 ModelIndex)
{
	//Boats are now allowed in SP and MP - process first.
	if ( ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetIsBoat() )
	{
		return m_bBoatsNeeded;
	}

    if (NetworkInterface::IsGameInProgress())
    {
        if(m_VehicleModelsRequiredForNetwork.IsMemberInGroup(ModelIndex))
        {
            return true;
        }

        return false;
    }

    return DoesCarModelBelongInCurrentPopulationZone(ModelIndex);
}

void CPopulationStreaming::DisallowScenarioPedStreamingForDuration(u32 timeInMS)
{
	streamDebugf1("Streaming of new scenario peds disabled for %d ms.", (int)timeInMS);

	m_ScenarioPedStreamingDisabledUntilTimeMS = Max(
			m_ScenarioPedStreamingDisabledUntilTimeMS, 
			fwTimer::GetTimeInMilliseconds() + timeInMS);
}

bool CPopulationStreaming::DoesCarModelBelongInCurrentPopulationZone(u32 ModelIndex)
{
	if(CPopCycle::HasValidCurrentPopAllocation())
	{
		const u32 popGroupCount = CPopCycle::GetPopGroups().GetVehCount();
		for (u32 popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
		{
			if (CPopCycle::GetCurrentVehGroupPercentage(popGroupIndex) > 0)
			{   // This group is needed. Now test whether the car is a member of this group.
				if (CPopCycle::GetPopGroups().IsVehIndexMember(popGroupIndex, ModelIndex))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CPopulationStreaming::SpawnedVehicleOnHighway()
{
    m_LastVehicleOnHighwaySpawn = fwTimer::GetTimeInMilliseconds();
}

bool CPopulationStreaming::IsPedModelNeededCurrently(u32 ModelIndex)
{
    if (NetworkInterface::IsGameInProgress())
    {
        if(m_PedModelsRequiredForNetwork.IsMemberInGroup(ModelIndex))
        {
            return true;
        }

        return false;
    }

	if(!CPopCycle::HasValidCurrentPopAllocation())
		return false;

	const u32 popGroupCount = CPopCycle::GetPopGroups().GetPedCount();
    for (u32 popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
    {
		if (CPopCycle::GetCurrentPedGroupPercentage(popGroupIndex) > 0)
        {   // This group is needed. Now test whether the car is a member of this group.
			if (CPopCycle::GetPopGroups().IsPedIndexMember(popGroupIndex, ModelIndex))
			{
				return true;
			}
		}
	}

    return false;
}

bool CPopulationStreaming::IsPedModelNeededInAnyZone(u32 ModelIndex)
{
	const u32 popGroupCount = CPopCycle::GetPopGroups().GetPedCount();
    for (u32 popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
    {
        if (CPopCycle::GetPopGroups().IsPedIndexMember(popGroupIndex, ModelIndex))
        {
            return true;
        }
    }

    return false;
}

void CPopulationStreaming::TellStreamingAboutDeletableVehicles()
{
    // Vehicles that are not native to the current zone and that are in the ONLY_IN_NATIVE_ZONE list should be streamed out.
    const u32 nativeZoneOnlysGroupNameHash = ATSTRINGHASH("ONLY_IN_NATIVE_ZONE", 0x068c476fc);
    u32 popGroupIndexOfNativeZoneOnlys = 0;
    bool nativeZoneOnlyPopGroupExists = CPopCycle::GetPopGroups().FindVehGroupFromNameHash(nativeZoneOnlysGroupNameHash, popGroupIndexOfNativeZoneOnlys);

	s32 numMembers = m_InAppropriateLoadedCars.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = m_InAppropriateLoadedCars.GetMember(C);

		fwModelId modelId((strLocalIndex(ModelIndex)));
		if (!modelId.IsValid())
			break;

        Assert(CModelInfo::HaveAssetsLoaded(modelId));

#if __BANK
			if (ModelIndex == m_vehicleOverrideIndex)
				continue;
#endif

        if (nativeZoneOnlyPopGroupExists && CPopCycle::GetPopGroups().IsVehIndexMember(popGroupIndexOfNativeZoneOnlys, ModelIndex) && !DoesCarModelBelongInCurrentPopulationZone(ModelIndex))
        {
            CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
			MarkModelNoLongerRequired(ModelIndex);
			if(!NetworkInterface::IsGameInProgress())
			{
				AddDiscardedCar(ModelIndex);
				--C;
			}
        }
    }


    // Boats can be removed if we don't need them.
	numMembers = m_LoadedBoats.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = m_LoadedBoats.GetMember(C);

		fwModelId modelId((strLocalIndex(ModelIndex)));
		if (!modelId.IsValid())
			break;

        Assert(CModelInfo::HaveAssetsLoaded(modelId));

#if __BANK
			if (ModelIndex == m_vehicleOverrideIndex)
				continue;
#endif

        if (m_bBoatsNeeded)
        {
            CModelInfo::SetAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
        }
        else
        {
            CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
			MarkModelNoLongerRequired(ModelIndex);
        }
    }

    // If the player drives a car that is appropriate we make sure it is not marked for removal.
    // We may as well continue to use it.
    if (CGameWorld::FindLocalPlayerVehicle())
    {
        u32 playerCarMI = CGameWorld::FindLocalPlayerVehicle()->GetModelIndex();

        if ( m_AppropriateLoadedCars.IsMemberInGroup(playerCarMI) )
        {
            if ( !nativeZoneOnlyPopGroupExists || !CPopCycle::GetPopGroups().IsVehIndexMember(popGroupIndexOfNativeZoneOnlys, playerCarMI) || DoesCarModelBelongInCurrentPopulationZone(playerCarMI))
            {
				fwModelId modelId((strLocalIndex(playerCarMI)));
                CModelInfo::SetAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
            }
        }
    }
}

void CPopulationStreaming::TellStreamingAboutDeletablePeds()
{
	s32 numMembers = m_InAppropriateLoadedPeds.CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        u32 ModelIndex = m_InAppropriateLoadedPeds.GetMember(C);

		fwModelId modelId((strLocalIndex(ModelIndex)));
		if (!modelId.IsValid())
			break;

        Assert(CModelInfo::HaveAssetsLoaded(modelId));
		CPedModelInfo *pPedModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
		if (pPedModelInfo->m_isRequestedAsDriver)
			continue;

        CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
		MarkModelNoLongerRequired(ModelIndex);
		if (!NetworkInterface::IsGameInProgress())
		{
			AddDiscardedPed(ModelIndex);
			--C;
		}
    }
}

void CPopulationStreaming::ModelHasBeenStreamedIn(u32 ModelIndex)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::POPSTREAMER);

    if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_PED)
    {
        PedHasBeenStreamedIn(ModelIndex);
    }
    else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_VEHICLE)
    {
        VehicleHasBeenStreamedIn(ModelIndex);
    }
	else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_WEAPON)
	{
		WeaponHasBeenStreamedIn(ModelIndex);
	}
}

void CPopulationStreaming::ModelHasBeenStreamedOut(u32 ModelIndex)
{
    if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_PED)
    {
        PedHasBeenStreamedOut(ModelIndex);
    }
    else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_VEHICLE)
    {
        VehicleHasBeenStreamedOut(ModelIndex);
    }
    else if (CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelType() == MI_TYPE_WEAPON)
    {
        WeaponHasBeenStreamedOut(ModelIndex);
    }
}

void CPopulationStreaming::PedHasBeenStreamedIn(u32 ModelIndex)
{
    Assert( (!m_AppropriateLoadedPeds.IsMemberInGroup(ModelIndex)) && (!m_InAppropriateLoadedPeds.IsMemberInGroup(ModelIndex)) );

    CPedModelInfo* pPMI = ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))));
    Assert(pPMI);
    pPMI->ResetNumTimesUsed();

	if (pPMI->GetPersonalitySettings().GetIsMale())
	{
		s_numMalePedsRequested--;
		if (s_numMalePedsRequested < 0)
			s_numMalePedsRequested = 0;
	}
	else
	{
		s_numFemalePedsRequested--;
		if (s_numFemalePedsRequested < 0)
			s_numFemalePedsRequested = 0;
	}

    // don't add player models to any of the population lists - this is handled in CPedVariationMgr
    if (!pPMI->GetIsPlayerModel() && (!CPed::IsSuperLodFromIndex(ModelIndex)))
    {
		ReinstateDiscardedPed(ModelIndex);

#if DEBUG_POPULATION_MEMORY
		s32 indexAdded = 
#endif // DEBUG_POPULATION_MEMORY
						  AddStreamedInModelData(ModelIndex);

        u32 totalVirtual = 0;
		u32 totalPhysical = 0;
		AddStreamedInComponent(ModelIndex, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);

#if DEBUG_POPULATION_MEMORY
		m_StreamedInModelData[indexAdded].m_mainUsed = totalVirtual;
		m_StreamedInModelData[indexAdded].m_vramUsed = totalPhysical;
#endif // DEBUG_POPULATION_MEMORY

		streamDebugf1("PedHasBeenStreamedIn: %s uses %dK/%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
    }
}

void CPopulationStreaming::AddStreamedPedVariation(const CPedStreamRenderGfx* gfx)
{
	if (!gfx)
		return;

	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	for (s32 i = 0; i < PV_MAX_COMP; ++i)
	{
		if (gfx->m_dwdIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_dwdIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			AddStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_txdIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_txdIdx[i]), g_TxdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			AddStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_cldIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_cldIdx[i]), g_ClothStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			AddStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_headBlendIdx[i] != -1)
		{
			deps.ResetCount();
			if (i < HBS_HEAD_DRAWABLES)
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			else if (i < HBS_MICRO_MORPH_SLOTS)
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_TxdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			else
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_DrawableStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			AddStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_fpAltIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_fpAltIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			AddStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}
	}

	streamDebugf1("AddStreamedPedVariation: %dK/%dK", totalVirtual/1024, totalPhysical/1024);

	m_TotalStreamedPedMemoryMain += totalVirtual;
	m_TotalStreamedPedMemoryVram += totalPhysical;
}

void CPopulationStreaming::AddStreamedVehicleVariation(const CVehicleStreamRenderGfx* gfx)
{
	if (!gfx)
		return;

	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		if (gfx->GetFragIndex(i) != -1)
		{
			streamDebugf1("AddStreamedVehicleVariation: Adding mod %d", i);
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(gfx->GetFragIndex(i), g_FragmentStore.GetStreamingModuleId(), deps, NULL, 0);
			AddStreamedInDependencies(deps, m_vehicleComponents, totalVirtual, totalPhysical, MTT_VEHICLE);
		}
	}

	streamDebugf1("AddStreamedVehicleVariation: %dK/%dK", totalVirtual/1024, totalPhysical/1024);

	m_TotalStreamedVehicleMemoryMain += totalVirtual;
	m_TotalStreamedVehicleMemoryVram += totalPhysical;
}

void CPopulationStreaming::RemoveStreamedPedVariation(const CPedStreamRenderGfx* gfx)
{
	if (!gfx)
		return;

	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	for (s32 i = 0; i < PV_MAX_COMP; ++i)
	{
		if (gfx->m_dwdIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_dwdIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			RemoveStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_txdIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_txdIdx[i]), g_TxdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			RemoveStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_cldIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_cldIdx[i]), g_ClothStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			RemoveStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_headBlendIdx[i] != -1)
		{
			deps.ResetCount();
			if (i < HBS_HEAD_DRAWABLES)
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			else if (i < HBS_MICRO_MORPH_SLOTS)
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_TxdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			else
				CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_headBlendIdx[i]), g_DrawableStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			RemoveStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}

		if (gfx->m_fpAltIdx[i] != -1)
		{
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(strLocalIndex(gfx->m_fpAltIdx[i]), g_DwdStore.GetStreamingModuleId(), deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());
			RemoveStreamedInDependencies(deps, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);
		}
	}

	streamDebugf1("RemoveStreamedPedVariation: %dK/%dK", totalVirtual/1024, totalPhysical/1024);

	m_TotalStreamedPedMemoryMain -= totalVirtual;
	m_TotalStreamedPedMemoryVram -= totalPhysical;
}

void CPopulationStreaming::RemoveStreamedVehicleVariation(const CVehicleStreamRenderGfx* gfx)
{
	if (!gfx)
		return;
	
	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		if (gfx->GetFragIndex(i) != -1)
		{
			streamDebugf1("RemoveStreamedVehicleVariation: Adding mod %d", i);
			deps.ResetCount();
			CStreaming::GetObjectAndDependencies(gfx->GetFragIndex(i), g_FragmentStore.GetStreamingModuleId(), deps, NULL, 0);
			RemoveStreamedInDependencies(deps, m_vehicleComponents, totalVirtual, totalPhysical, MTT_VEHICLE);
		}
	}

	streamDebugf1("RemoveStreamedVehicleVariation: %dK/%dK", totalVirtual/1024, totalPhysical/1024);

	m_TotalStreamedVehicleMemoryMain -= totalVirtual;
	m_TotalStreamedVehicleMemoryVram -= totalPhysical;
}

void CPopulationStreaming::PedHasBeenStreamedOut(u32 ModelIndex)
{
    m_AppropriateLoadedPeds.RemoveMember(ModelIndex);
    m_InAppropriateLoadedPeds.RemoveMember(ModelIndex);
    m_SpecialLoadedPeds.RemoveMember(ModelIndex);                   // Mission requested peds will end up in the SpecialPeds and need to be removed also.
	m_DiscardedPeds.RemoveMember(ModelIndex);

    // Set the last time used to the current time so that the other vehicles in the same group will get loaded in before this one.
    CPedModelInfo *pPedModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)));

    // don't add player models to any of the population lists - this is handled in CPedVariationMgr
    if (!pPedModelInfo->GetIsPlayerModel() && (!CPed::IsSuperLodFromIndex(ModelIndex)))
    {
        if (!RemoveStreamedInModelData(ModelIndex))
			return;

        pPedModelInfo->SetLastTimeUsed(fwTimer::GetTimeInMilliseconds());

        // If this ped was counted as a scenario ped we reduce the count of those.
        if (pPedModelInfo->GetCountedAsScenarioPed())
        {
			streamDebugf1("%s no longer counted as scenario ped.", pPedModelInfo->GetModelName());
            pPedModelInfo->SetCountedAsScenarioPed(false);
			//Decrement the number of models streamed in for the given slot.
			int scenarioPedStreamingSlot = pPedModelInfo->GetScenarioPedStreamingSlot();
            m_aNumScenarioPedModelsLoaded[scenarioPedStreamingSlot]--;
            Assert(m_aNumScenarioPedModelsLoaded[scenarioPedStreamingSlot] >= 0);
        }

		u32 totalVirtual = 0;
		u32 totalPhysical = 0;
		RemoveStreamedInComponent(ModelIndex, m_pedComponents, totalVirtual, totalPhysical, MTT_PED);

		streamDebugf1("PedHasBeenStreamedOut: %s uses %dK/%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
    }

	streamAssertf(!pPedModelInfo->GetCountedAsScenarioPed(),
			"Ped model %s just streamed out, but is still marked as a scenario ped.",
			pPedModelInfo->GetModelName());

}

void CPopulationStreaming::RequestVehicleDriver(u32 vehicleModelIndex)
{
	fwModelId modelId((strLocalIndex(vehicleModelIndex)));
	if (!modelId.IsValid())
		return;

	CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(modelId);
	if (!bmi || bmi->GetModelType() != MI_TYPE_VEHICLE)
		return;

	if (!CModelInfo::GetAssetRequiredFlag(modelId))
		return;

	CVehicleModelInfo* vmi = (CVehicleModelInfo*)bmi;
	if (!vmi->HasRequestedDrivers() && !CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(vehicleModelIndex))
		CPedPopulation::LoadSpecificDriverModelsForCar(modelId);
}

void CPopulationStreaming::UnrequestVehicleDriver(u32 vehicleModelIndex)
{
	fwModelId modelId((strLocalIndex(vehicleModelIndex)));
	if (!modelId.IsValid())
		return;

	CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(modelId);
	if (!bmi || bmi->GetModelType() != MI_TYPE_VEHICLE)
		return;

	CPedPopulation::RemoveSpecificDriverModelsForCar(modelId);
}

void CPopulationStreaming::VehicleHasBeenStreamedIn(u32 ModelIndex)
{
    Assert( (!m_AppropriateLoadedCars.IsMemberInGroup(ModelIndex)) && (!m_InAppropriateLoadedCars.IsMemberInGroup(ModelIndex)) );

	CVehicleModelInfo* vmi = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)));
	vmi->ResetNumTimesUsed();
    vmi->SetLastTimeUsed(fwTimer::GetTimeInMilliseconds());

    bool bIsBoat = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetVehicleType()==VEHICLE_TYPE_BOAT;
    bool bIsTrain = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetVehicleType()==VEHICLE_TYPE_TRAIN;
    bool bIsHeli = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetVehicleType()==VEHICLE_TYPE_HELI;
    bool bIsAmbulance = (ModelIndex == MI_CAR_AMBULANCE);
    bool bIsFiretruck = (ModelIndex == MI_CAR_FIRETRUCK);
	REPLAY_ONLY(bool bIsReplayInCharge = CReplayMgr::IsReplayInControlOfWorld();)

	bool requestDriver = true;
    if (bIsHeli || bIsTrain || bIsAmbulance || bIsFiretruck REPLAY_ONLY(|| bIsReplayInCharge))
    {
		// AF: Vehicles are added to the special list for some unknown reason. I am assuming this list is for vehicles whose streaming
		// is managed somewhere else
        m_SpecialLoadedCars.AddMember(ModelIndex);

		// Don't request a driver for the members of m_SpecialLoadedCars, has to be done elsewhere
		// by users that need it.
		requestDriver = false;
    }
    else if (bIsBoat)
    {
        m_LoadedBoats.AddMember(ModelIndex);

		// Only request drivers for boats if we actually want boats in the general population.
		// Otherwise, they are probably just needed for car generators or vehicle scenarios (which
		// will request drivers if needed).
		requestDriver = m_bBoatsNeeded;
    }
	else if (IsCarModelNeededCurrently(ModelIndex))
	{
		m_AppropriateLoadedCars.AddMember(ModelIndex);
	}
	else
	{
		m_InAppropriateLoadedCars.AddMember(ModelIndex);

		// Car generator and scenario vehicles often end up here. We don't need drivers to be
		// requested for those, at this level of the code.
		requestDriver = false;
	}

	if(requestDriver)
	{
		RequestVehicleDriver(ModelIndex);
	}

	m_DiscardedCars.RemoveMember(ModelIndex);

#if DEBUG_POPULATION_MEMORY
    s32 indexAdded =
#endif // DEBUG_POPULATION_MEMORY
					 AddStreamedInModelData(ModelIndex);

	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	AddStreamedInComponent(ModelIndex, m_vehicleComponents, totalVirtual, totalPhysical, MTT_VEHICLE);

#if DEBUG_POPULATION_MEMORY
	m_StreamedInModelData[indexAdded].m_mainUsed = totalVirtual;
	m_StreamedInModelData[indexAdded].m_vramUsed = totalPhysical;
	streamDebugf1("VehicleHasBeenStreamedIn (%d, %d ms ago, index: %d): %s uses %dK/%dK additional memory", m_StreamedInModelData[indexAdded].m_TimeStreamedIn, fwTimer::GetTimeInMilliseconds() - m_StreamedInModelData[indexAdded].m_TimeStreamedIn, indexAdded, CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
#else
	streamDebugf1("VehicleHasBeenStreamedIn: %s uses %dK/%dK additional memory", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
#endif // DEBUG_POPULATION_MEMORY

	if(NetworkInterface::IsGameInProgress())
	{
		ReclassifyLoadedVehiclesForNetwork();
	}

	m_numQueuedVehicleModels = (s8)rage::Max(0, m_numQueuedVehicleModels - 1);

    EmergencyRemoveVehicleStructures();
}

void CPopulationStreaming::VehicleHasBeenStreamedOut(u32 ModelIndex)
{
	m_AppropriateLoadedCars.RemoveMember(ModelIndex);
	m_InAppropriateLoadedCars.RemoveMember(ModelIndex);
	m_SpecialLoadedCars.RemoveMember(ModelIndex);
	m_LoadedBoats.RemoveMember(ModelIndex);
	m_DiscardedCars.RemoveMember(ModelIndex);

	UnrequestVehicleDriver(ModelIndex);

	CTheCarGenerators::VehicleHasBeenStreamedOut(ModelIndex);

	if(NetworkInterface::IsGameInProgress())
	{
		ReclassifyLoadedVehiclesForNetwork();
	}

	if (!RemoveStreamedInModelData(ModelIndex))
		return;

    // Set the last time used to the current time so that the other vehicles in the same group will get loaded in before this one.
    ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->SetLastTimeUsed(fwTimer::GetTimeInMilliseconds());

	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	RemoveStreamedInComponent(ModelIndex, m_vehicleComponents, totalVirtual, totalPhysical, MTT_VEHICLE);

	streamDebugf1("VehicleHasBeenStreamedOut: %s freed %dK/%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
}

void CPopulationStreaming::WeaponHasBeenStreamedIn(u32 ModelIndex)
{

#if DEBUG_POPULATION_MEMORY
	s32 indexAdded =
#endif // DEBUG_POPULATION_MEMORY
		AddStreamedInModelData(ModelIndex);

	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	AddStreamedInComponent(ModelIndex, m_weaponComponents, totalVirtual, totalPhysical, MTT_WEAPON);

#if DEBUG_POPULATION_MEMORY
	m_StreamedInModelData[indexAdded].m_mainUsed = totalVirtual;
	m_StreamedInModelData[indexAdded].m_vramUsed = totalPhysical;
	streamDebugf1("WeaponHasBeenStreamedIn (%d, %d ms ago, index: %d): %s uses %dK/%dK additional memory", m_StreamedInModelData[indexAdded].m_TimeStreamedIn, fwTimer::GetTimeInMilliseconds() - m_StreamedInModelData[indexAdded].m_TimeStreamedIn, indexAdded, CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
#else
	streamDebugf1("WeaponHasBeenStreamedIn: %s uses %dK/%dK additional memory", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
#endif // DEBUG_POPULATION_MEMORY
}

void CPopulationStreaming::WeaponHasBeenStreamedOut(u32 ModelIndex)
{
	if (!RemoveStreamedInModelData(ModelIndex))
		return;

	u32 totalVirtual = 0;
	u32 totalPhysical = 0;
	RemoveStreamedInComponent(ModelIndex, m_weaponComponents, totalVirtual, totalPhysical, MTT_WEAPON);

	streamDebugf1("WeaponHasBeenStreamedOut: %s freed %dK/%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)))->GetModelName(), totalVirtual/1024, totalPhysical/1024);
}

u32 CPopulationStreaming::GetFallbackPedModelIndex(bool bDriversOnly)
{
  float   lowestWeirdness = 1000.0f;
  u32 MostAveragePedModelIndex = fwModelId::MI_INVALID;

	CLoadedModelGroup fallbackPeds;
	GetFallbackPedGroup(fallbackPeds, bDriversOnly);

	// if we have appropriate peds use only those to choose an index, otherwise use all fallback peds
	CLoadedModelGroup appropriateCarPeds = m_AppropriateLoadedPeds;
	appropriateCarPeds.RemovePedModelsThatCantDriveCars(true);

	s32 numMembers = appropriateCarPeds.CountMembers();
	if (numMembers == 0)
		numMembers = fallbackPeds.CountMembers();

    for (int p = 0; p < numMembers; p++)
    {
			float   Weirdness = 0.0f;
			strLocalIndex TestMI = strLocalIndex(fallbackPeds.GetMember(p));

			CPedModelInfo *pModelInfo = ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(TestMI)));
			if (!pModelInfo->IsMale())
			{
				Weirdness += 0.1f;
			}
			if (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_POOR_CAR) && !pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_AVERAGE_CAR) && !pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_RICH_CAR))
			{
				Weirdness += 0.2f;
			}
			if (Weirdness < lowestWeirdness)
			{
				lowestWeirdness = Weirdness;
				MostAveragePedModelIndex = TestMI.Get();
			}
    }

    Assert(CModelInfo::IsValidModelInfo(MostAveragePedModelIndex));      // If there wasn't a fallback ped the calling code should have called IsFallbackPedAvailable first.

    return MostAveragePedModelIndex;
}

bool CPopulationStreaming::IsFallbackPedAvailable(bool bikesOnly, bool bDriversOnly)
{
	CLoadedModelGroup group;
	GetFallbackPedGroup(group, bDriversOnly);
	if (bikesOnly)
		group.RemoveNonMotorcyclePeds();

	return !group.IsEmpty();
}

void CPopulationStreaming::GetFallbackPedGroup(CLoadedModelGroup& fallbackGroup, bool carDrivers)
{
	fallbackGroup.Clear();
	fallbackGroup.Merge(&m_AppropriateLoadedPeds, &m_InAppropriateLoadedPeds, &m_DiscardedPeds);
	fallbackGroup.RemoveSuppresedPeds();

	if (carDrivers)
		fallbackGroup.RemovePedModelsThatCantDriveCars(true);
}

bool CPopulationStreaming::StreamScenarioPeds(fwModelId PedModelId, bool highPri, bool startupMode, const char* ASSERT_ONLY(debugSourceName))
{
	if (Verifyf( PedModelId.IsValid(), "We need a valid model specified" ))
	{
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::POPSTREAMER);

		CPedModelInfo *pPedModelInfo = (CPedModelInfo *) CModelInfo::GetBaseModelInfo(PedModelId);
		Assertf(pPedModelInfo, "Valid ped model ID, but invalid CPedModelInfo.");
		eScenarioPopStreamingSlot eStreamSlot = pPedModelInfo->GetScenarioPedStreamingSlot();

		if (!CModelInfo::HaveAssetsLoaded(PedModelId))
		{
			// We're not going to stream a scenario ped back in straight away.
			// - Update: we do now, if it's for a high priority scenario.
			u32 lastTimeUsed = pPedModelInfo->GetLastTimeUsed();
			if ( ( (fwTimer::GetTimeInMilliseconds() - lastTimeUsed) < SCENARIO_DONT_GET_RELOADED_DURATION) && lastTimeUsed != 0
					&& !highPri && !startupMode)
			{
				return false;
			}

			if (!CModelInfo::AreAssetsRequested(PedModelId))
			{
				if ( !CModelInfo::AreAssetsLoading(PedModelId) )
				{
					streamAssertf( !pPedModelInfo->GetCountedAsScenarioPed(), "%s already counted as a scenario ped, which was not expected.", pPedModelInfo->GetModelName() );
					if (!pPedModelInfo->GetCountedAsScenarioPed())
					{
						if(!IsAllowedToRequestScenarioPedModel(highPri, startupMode, eStreamSlot))
						{
							streamDebugf1("Not allowed to stream in %s, too many scenario peds (%d/%d/%d).",
									pPedModelInfo->GetModelName(),
									m_aNumScenarioPedModelsLoaded[eStreamSlot], m_MaxScenarioPedModelsLoadedPerSlot, m_MaxScenarioPedModelsLoadedOverride);
							return false;
						}


						if(streamVerifyf(CModelInfo::RequestAssets(PedModelId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE),
								"Failed to request model %s for scenario ped, may be missing from streaming archives (requested from %s).", pPedModelInfo->GetModelName(), debugSourceName))
						{
							pPedModelInfo->SetCountedAsScenarioPed(true);
							streamDebugf1("%s is now counted as scenario ped.", pPedModelInfo->GetModelName());
							//Increment the number of scenario peds streamed in for a given slot.
							m_aNumScenarioPedModelsLoaded[eStreamSlot]++;
						}
					}
				}

				return false;
			}
		}
		else
		{
			// If the model is loaded but in the process of being removed grab it allow it to be used again.
			// This is temporary whilst we properly apply scenario limits
			// We also need to bump the streamed in timer to prevent the ped population from immediately pushing it back out again.
			if (!CModelInfo::GetAssetRequiredFlag(PedModelId))
			{
				if(!IsAllowedToRequestScenarioPedModel(highPri, startupMode, eStreamSlot))
				{
					streamDebugf1("Not allowed to stream in %s, too many scenario peds (%d/%d/%d).",
							pPedModelInfo->GetModelName(),
							m_aNumScenarioPedModelsLoaded[eStreamSlot], m_MaxScenarioPedModelsLoadedPerSlot, m_MaxScenarioPedModelsLoadedOverride);

					return false;
				}

				ReinstateDiscardedPed(PedModelId.GetModelIndex());
				SetTimeModelStreamedIn(PedModelId, fwTimer::GetTimeInMilliseconds());

				// ReinstateDiscaredPed() doesn't necessarily set STRFLAG_DONTDELETE for scenario peds, but
				// if we get here the ped is loaded and counted as a scenario ped, so it needs the flag to
				// prevent it from getting streamed out.
				// Note: I think this should be done here, but not sure that I want to change it right now
				// since it was probably unrelated to the bug I looked at:
				//	CModelInfo::SetAssetRequiredFlag(PedModelId, STRFLAG_DONTDELETE);

				if( !pPedModelInfo->GetCountedAsScenarioPed() )
				{
					pPedModelInfo->SetCountedAsScenarioPed(true);
					streamDebugf1("%s is now counted as scenario ped (was about to be removed).", pPedModelInfo->GetModelName());
					//Increment the number of scenario peds loaded in for a given slot.
					m_aNumScenarioPedModelsLoaded[eStreamSlot]++;
				}
				return true;
			}
		}

		return true;
	}

	return false;
}

void CPopulationStreaming::RemoveStreamedPedModelsOnShutdown()
{
	CPedFactory::GetFactory()->ClearDestroyedPedCache();

    s32 k, num = m_AppropriateLoadedPeds.CountMembers();
    u32 mi = fwModelId::MI_INVALID;
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_AppropriateLoadedPeds.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedPedModelsOnShutdown: Appropriate ped: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
        Assertf(((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs() == 0, "m_AppropriateLoadedPeds - %s NumPedModelRefs %d", pModelInfo->GetModelName(), ((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
        CModelInfo::RemoveAssets(modelId);
    }

    num = m_InAppropriateLoadedPeds.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_InAppropriateLoadedPeds.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedPedModelsOnShutdown: InAppropriate ped: %s\n", pModelInfo->GetModelName());
#endif

		Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
		Assertf(((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs() == 0, "m_InAppropriateLoadedPeds - %s NumPedModelRefs %d", pModelInfo->GetModelName(), ((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
        CModelInfo::RemoveAssets(modelId);
    }

    num = m_SpecialLoadedPeds.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_SpecialLoadedPeds.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedPedModelsOnShutdown: Special ped: %s\n", pModelInfo->GetModelName());
#endif

        Assert(CModelInfo::IsValidModelInfo(mi));
		Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
        Assertf(((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs() == 0, "m_SpecialLoadedPeds - %s NumPedModelRefs %d", pModelInfo->GetModelName(), ((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }

    num = m_DiscardedPeds.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_DiscardedPeds.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedPedModelsOnShutdown: Discarded ped: %s\n", pModelInfo->GetModelName());
#endif

        Assert(CModelInfo::IsValidModelInfo(mi));
		Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
        Assertf(((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs() == 0, "m_SpecialLoadedPeds - %s NumPedModelRefs %d", pModelInfo->GetModelName(), ((CPedModelInfo*)pModelInfo)->GetNumPedModelRefs());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }
}

void CPopulationStreaming::RemoveStreamedCarModelsOnShutdown()
{
	CTheCarGenerators::ClearScheduledQueue();
	CVehicleFactory::GetFactory()->ClearDestroyedVehCache();

    s32 k, num = m_AppropriateLoadedCars.CountMembers();
    u32 mi = fwModelId::MI_INVALID;
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_AppropriateLoadedCars.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedCarModelsOnShutdown: Appropriate car: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs() == 0, "m_AppropriateLoadedCars - %s NumRefs %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs());
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked() == 0, "m_AppropriateLoadedCars - %s NumRefsParked %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }

    num = m_InAppropriateLoadedCars.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_InAppropriateLoadedCars.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedCarModelsOnShutdown: InAppropriate car: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs() == 0, "m_InAppropriateLoadedCars - %s NumRefs %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs());
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked() == 0, "m_InAppropriateLoadedCars - %s NumRefsParked %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }

    num = m_SpecialLoadedCars.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_SpecialLoadedCars.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedCarModelsOnShutdown: Special car: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
		Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs() == 0, "m_SpecialLoadedCars - %s NumRefs %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs());
		Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked() == 0, "m_SpecialLoadedCars - %s NumRefsParked %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }

    num = m_LoadedBoats.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_LoadedBoats.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedCarModelsOnShutdown: Boat: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs() == 0, "m_LoadedBoats - %s NumRefs %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs());
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked() == 0, "m_LoadedBoats - %s NumRefsParked %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }

    num = m_DiscardedCars.CountMembers();
    for (k = (num-1); k >= 0; k--)
    {
        mi = m_DiscardedCars.GetMember(k);
		fwModelId modelId((strLocalIndex(mi)));
		ASSERT_ONLY(CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId); )

#if DEBUG_POPULATION_MEMORY && __ASSERT
		Warningf("CPopulationStreaming::RemoveStreamedCarModelsOnShutdown: Discarded car: %s\n", pModelInfo->GetModelName());
#endif

        Assert(pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs() == 0, "m_LoadedBoats - %s NumRefs %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefs());
        Assertf(((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked() == 0, "m_LoadedBoats - %s NumRefsParked %d", pModelInfo->GetModelName(), ((CVehicleModelInfo*)pModelInfo)->GetNumVehicleModelRefsParked());
        Assert(pModelInfo->GetDrawable());

		CModelInfo::ClearAssetRequiredFlag(modelId, STR_DONTDELETE_MASK);
		CModelInfo::RemoveAssets(modelId);
    }
}

void CPopulationStreaming::EmergencyRemoveVehicleStructures()
{
    const unsigned REMOVAL_THRESHOLD = 20;
    int numToDitch = REMOVAL_THRESHOLD - CVehicleStructure::m_pInfoPool->GetNoOfFreeSpaces();

    if (numToDitch > 0)
    {
        streamDebugf2("Emergency vehicle removal");

        CLoadedModelGroup tempList;
        tempList.Merge(&m_InAppropriateLoadedCars, &m_DiscardedCars, &m_AppropriateLoadedCars);

        int numVehicles = tempList.CountMembers();

        for(int index = (numVehicles-1); (index >= 0) && (numToDitch > 0); index--)
        {
            strLocalIndex modelIndex = strLocalIndex(tempList.GetMember(index));
			fwModelId modelId(modelIndex);
            if (CModelInfo::GetBaseModelInfo(modelId)->GetNumRefs() == 0 &&
                !CModelInfo::GetAssetRequiredFlag(modelId))
            {
                CModelInfo::RemoveAssets(modelId);
                numToDitch--;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////
// CLoadedModelGroup

void CLoadedModelGroup::Clear()
{
    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        aMembers[C] = fwModelId::MI_INVALID;
    }
}

bool CLoadedModelGroup::AddMember(u32 NewMember)
{
	if(IsMemberInGroup(NewMember))
	{
		return false;
	}

    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        if (!CModelInfo::IsValidModelInfo(aMembers[C]))
        {
            aMembers[C] = NewMember;
            return true;
        }
    }

    Assert(0);  // List is full?

	return false;
}

bool CLoadedModelGroup::RemoveMember(u32 OldMember)
{
    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        if (aMembers[C] == OldMember)
        {
            // Found the one to remove. Now shift the rest up a bit.
            for (int K = C; K < MAX_NUM_IN_LOADED_MODEL_GROUP-1; K++)
            {
                aMembers[K] = aMembers[K + 1];
            }
            aMembers[MAX_NUM_IN_LOADED_MODEL_GROUP-1] = fwModelId::MI_INVALID;
            return true;
        }
    }

	return false;
}

u32 CLoadedModelGroup::GetMember(u32 Member) const
{
    //Assert(aMembers[Member] >= 0);
    return(aMembers[Member]);
}

int CLoadedModelGroup::CountMembers() const
{
    int Result = 0;

    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        if (CModelInfo::IsValidModelInfo(aMembers[C]))
        {
            Result++;
        }
        else if (!CModelInfo::IsValidModelInfo(aMembers[C]))
        {
            return Result;
        }
    }
    return Result;
}

bool CLoadedModelGroup::IsEmpty()
{
    return !CModelInfo::IsValidModelInfo(aMembers[0]);
}

int CLoadedModelGroup::CountUsableOnes()
{
    int Result = 0;

    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        // Only consider models that aren't about to be streamed out.
		fwModelId modelId((strLocalIndex(aMembers[C])));
        if (modelId.IsValid() && CModelInfo::GetAssetRequiredFlag(modelId))
        {
            Result++;
        }
        else if (!(modelId.IsValid()))
        {
            return Result;
        }
    }
    return Result;
}

bool CLoadedModelGroup::IsMemberInGroup(u32 ModelIndex) const
{
    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        if (aMembers[C] == ModelIndex)
        {
            return true;
        }
    }
    return false;
}

bool CLoadedModelGroup::IsMaleInPedGroup()
{
    for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
    {
        if (aMembers[C] == fwModelId::MI_INVALID)
        {
            return false;
        }
        if ( ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C]))))->IsMale()  )
        {
            return true;
        }
    }
    return false;
}

bool CLoadedModelGroup::IsFemaleInPedGroup()
{
	for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
	{
		if (aMembers[C] == fwModelId::MI_INVALID)
		{
			return false;
		}
		if ( !((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C]))))->IsMale()  )
		{
			return true;
		}
	}
	return false;
}

bool CLoadedModelGroup::IsBikeDriverInPedGroup()
{
	for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
	{
		if (aMembers[C] == fwModelId::MI_INVALID)
		{
			return false;
		}

		if (gPopStreaming.CanPedModelRideBike(aMembers[C]))
		{
			return true;
		}
	}
	return false;
}

bool CLoadedModelGroup::IsBikeInVehGroup()
{
	for (int C = 0; C < MAX_NUM_IN_LOADED_MODEL_GROUP; C++)
	{
		if (aMembers[C] == fwModelId::MI_INVALID)
		{
			return false;
		}

		CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C])));
		if (vmi->GetIsBike())
		{
			return true;
		}
	}
	return false;
}

strLocalIndex CLoadedModelGroup::PickRandomCarModel(bool cargens)
{
    int Num = CountMembers();
    if (Num == 0)
        return strLocalIndex(fwModelId::MI_INVALID);

    int C;

    u32 aApplicableMembers[MAX_NUM_IN_LOADED_MODEL_GROUP];
    float   aScaledOccurence[MAX_NUM_IN_LOADED_MODEL_GROUP];
    int NumberOfApplicableCars = 0;



    for (C = 0; C < Num; C++)
    {
        u32 ModelIndex = aMembers[C];
		fwModelId modelId((strLocalIndex(ModelIndex)));
        Assert(((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(modelId))->GetVehicleFreq() > 0);

        // Only consider models that aren't about to be streamed out.
        if (CModelInfo::GetAssetRequiredFlag(modelId))
        {

            if (!CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(ModelIndex))      // Only consider cars that haven't been suppressed by the script.
            {
                aApplicableMembers[NumberOfApplicableCars] = ModelIndex;
				CVehicleModelInfo* pVMI = (CVehicleModelInfo*)(CModelInfo::GetBaseModelInfo(modelId));
				s32 refs = cargens ? pVMI->GetNumVehicleModelRefsParked() : pVMI->GetNumVehicleModelRefs();
                aScaledOccurence[NumberOfApplicableCars] = float(refs) / pVMI->GetVehicleFreq();
                NumberOfApplicableCars++;
            }
        }
    }

    // Now pick the vehicle that is used least (relative to its frequency)
    u32 smallestMI = fwModelId::MI_INVALID;
    float   smallestVal = 999999.9f;

    for (int i = 0; i < NumberOfApplicableCars; i++)
    {
        if (aScaledOccurence[i] < smallestVal)
        {
            smallestMI = aApplicableMembers[i];
            smallestVal = aScaledOccurence[i];
        }
    }

    return strLocalIndex(smallestMI);
}

u32 CLoadedModelGroup::PickRandomPedModel()
{
	int Num = CountMembers();
	if (Num == 0)
		return fwModelId::MI_INVALID;

	int C;

	u32 aApplicableMembers[MAX_NUM_IN_LOADED_MODEL_GROUP];
	u32 aOccurence[MAX_NUM_IN_LOADED_MODEL_GROUP];
	int NumberOfApplicablePeds = 0;



	for (C = 0; C < Num; C++)
	{
		u32 ModelIndex = aMembers[C];

		// Only consider models that aren't about to be streamed out.
		fwModelId modelId((strLocalIndex(ModelIndex)));
		if (CModelInfo::GetAssetRequiredFlag(modelId) && !CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(ModelIndex))
		{
			aApplicableMembers[NumberOfApplicablePeds] = ModelIndex;
			CPedModelInfo* pMI = (CPedModelInfo*)(CModelInfo::GetBaseModelInfo(modelId));
			aOccurence[NumberOfApplicablePeds] = pMI->GetNumPedModelRefs();
			NumberOfApplicablePeds++;
		}
	}

	// Now pick the ped that is used least
	u32 smallestMI = fwModelId::MI_INVALID;
	u32 smallestVal = 999999;

	for (int i = 0; i < NumberOfApplicablePeds; i++)
	{
		if (aOccurence[i] < smallestVal)
		{
			smallestMI = aApplicableMembers[i];
			smallestVal = aOccurence[i];
		}
	}

	return smallestMI;
}

void CLoadedModelGroup::SortBasedOnUsage()
{
    int Num = CountMembers();

    bool    bChangeMade = true;
    while (bChangeMade)
    {
        bChangeMade = false;

        for (int C = 0; C < Num-1; C++)
        {
            if ( ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C]))))->GetNumTimesUsed() <
                 ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C+1]))))->GetNumTimesUsed() )
            {
                u32 Temp = aMembers[C];
                aMembers[C] = aMembers[C+1];
                aMembers[C+1] = Temp;

                bChangeMade = true;
            }
        }
    }
}

u32 CLoadedModelGroup::PickLeastUsedModel(int MaxOfThem)
{
    int Num = CountMembers();

    u32 ModelIndex = fwModelId::MI_INVALID;
    int BestRefs = 999;
    int BestUsage = 999;

    for (int C = 0; C < Num; C++)
    {
        // Pick this one if is used less than the previous best or if it is
        // used equally but hasn't been used as much in the past.
		CVehicleModelInfo* pVMI = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C])));
        if ( pVMI->GetNumVehicleModelRefs() < BestRefs ||
             (pVMI->GetNumVehicleModelRefs() == BestRefs &&
             pVMI->GetNumTimesUsed() < BestUsage))
        {
            if (!CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(aMembers[C]))     // Only consider cars that haven't been suppressed by the script.
            {
                ModelIndex = aMembers[C];
				BestRefs = ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetNumVehicleModelRefs();
                BestUsage = ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetNumTimesUsed();
            }
        }
    }
    if (BestRefs <= MaxOfThem)
    {
        return ModelIndex;
    }
    return fwModelId::MI_INVALID;
}

u32 CLoadedModelGroup::PickLeastUsedPedModel()
{
    int Num = CountMembers();

    u32 ModelIndex = fwModelId::MI_INVALID;
    int BestRefs = 999;
    int BestUsage = 999;

    for (int C = 0; C < Num; C++)
    {
        // Pick this one if is used less than the previous best or if it is
        // used equally but hasn't been used as much in the past.
		CPedModelInfo* pPMI = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(aMembers[C])));
        if ( pPMI->GetNumPedModelRefs() < BestRefs ||
            (pPMI->GetNumPedModelRefs() == BestRefs &&
            pPMI->GetNumTimesUsed() < BestUsage))
        {
            if (!CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(aMembers[C]))     // Only consider peds that haven't been suppressed by the script.
            {
				ModelIndex = aMembers[C];
				BestRefs = ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetNumPedModelRefs();
				BestUsage = ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex))))->GetNumTimesUsed();
            }
        }
    }
    return ModelIndex;
}

void CLoadedModelGroup::Merge(CLoadedModelGroup *pList1, CLoadedModelGroup *pList2, CLoadedModelGroup *pList3, CLoadedModelGroup *pList4)
{
    Assert(pList1 != this && pList2 != this && pList3 != this);

    this->Clear();

    Assert(pList1 && pList2);

	s32 numMembers = pList1->CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        this->AddMember(pList1->GetMember(C));
    }
	numMembers = pList2->CountMembers();
    for (int C = 0; C < numMembers; C++)
    {
        this->AddMember(pList2->GetMember(C));
    }
    if (pList3)
    {
		numMembers = pList3->CountMembers();
        for (int C = 0; C < numMembers; C++)
        {
            this->AddMember(pList3->GetMember(C));
        }
    }
    if (pList4)
    {
		numMembers = pList4->CountMembers();
        for (int C = 0; C < numMembers; C++)
        {
            this->AddMember(pList4->GetMember(C));
        }
    }
}

void CLoadedModelGroup::Copy(CLoadedModelGroup *pSource)
{
    Clear();

	s32 numMembers = pSource->CountMembers();
    for (int i = 0; i < numMembers; i++)
    {
        AddMember(pSource->GetMember(i));
    }
}

void CLoadedModelGroup::CopyCarsSmallWorkers(CLoadedModelGroup *pSource, bool bSmallWorkers)
{
    Clear();

	s32 numMembers = pSource->CountMembers();
    for (int i = 0; i < numMembers; i++)
    {
        u32 testModel = pSource->GetMember(i);
        Assert(CModelInfo::IsValidModelInfo(testModel));

        if (bSmallWorkers == ( ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(testModel))))->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SMALL_WORKER) ) )
        {
            AddMember(testModel);
        }
    }
}

void CLoadedModelGroup::CopyEligibleCars(const Vector3 & vPos, CLoadedModelGroup* source, bool smallWorkers, bool removeNonNative, bool removeCopCars, bool removeOnlyOnHighway, bool removeNonOffroad, bool removeBigVehicles)
{
	bool haveBikeRider = gPopStreaming.GetAppropriateLoadedPeds().IsBikeDriverInPedGroup();

	u32 numMembers = 0;
	for (s32 i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; ++i)
	{
		u32 next = source->GetMember(i);
		if (!CModelInfo::IsValidModelInfo(next))
			break; // reached end, safe to break

		CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(next)));
		Assert(vmi);

		if(CVehiclePopulation::IsModelReservedForSpecificStreet(next, vPos))
			continue;

		if (smallWorkers != vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SMALL_WORKER))
			continue;

		if (removeNonNative && !gPopStreaming.DoesCarModelBelongInCurrentPopulationZone(next))
			continue;

		if (removeCopCars && CVehicle::IsLawEnforcementVehicleModelId(fwModelId(strLocalIndex(next))))
			continue;

		if (removeOnlyOnHighway && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ONLY_ON_HIGHWAYS))
			continue;

		if (removeNonOffroad && !vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_OFFROAD_VEHICLE))
			continue;

		//not too happy about this hack--if this vehicle is bad at turning, only spawn it on roads with
		//multiple lanes
		if (removeBigVehicles && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVOID_TURNS))
			continue;

		// remove cars that have hit max number
		if ((vmi->GetNumVehicleModelRefs() + vmi->GetNumVehicleModelRefsParked()) >= vmi->GetVehicleMaxNumber())
			continue;

		if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(next))
			continue;

		if (!haveBikeRider && vmi->GetIsBike())
			continue;

		aMembers[numMembers++] = next;
	}

	// clear the rest of the entries
	for (s32 i = numMembers; i < MAX_NUM_IN_LOADED_MODEL_GROUP; ++i)
		aMembers[i] = fwModelId::MI_INVALID;
}

void CLoadedModelGroup::RemoveCarsFromGroup(s32 popGroup)
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi) && CPopCycle::GetPopGroups().IsVehIndexMember(popGroup, mi))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveModelsNotInGroup(s32 popGroup, bool peds)
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

		if (!CModelInfo::IsValidModelInfo(mi))
			continue;

		bool isMember = peds ? CPopCycle::GetPopGroups().IsPedIndexMember(popGroup, mi) : CPopCycle::GetPopGroups().IsVehIndexMember(popGroup, mi);
        if (!isMember)
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveModelsNotInModelSet(int modelSetType, int modelSetIndex)
{
	// Get the model set from the manager.
	const CAmbientModelSet& modelSet = CAmbientModelSetManager::GetInstance().GetModelSet((CAmbientModelSetManager::ModelSetType)modelSetType, modelSetIndex);

    for(int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        const u32 mi = aMembers[i];

		if(!CModelInfo::IsValidModelInfo(mi))
		{
			continue;
		}

		// Get the model info, so we can get to the model name hash.
		const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
		if(!pModelInfo)
		{
			continue;
		}

		// Check if it's a member of the model set.
		const bool isMember = modelSet.GetContainsModel(pModelInfo->GetModelNameHash());
        if(!isMember)
        {	// Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveOnlyOnHighwayCarsFromGroup()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi) && ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ONLY_ON_HIGHWAYS) )
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveNonOffroadCars()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];

		if (CModelInfo::IsValidModelInfo(mi) && !((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_OFFROAD_VEHICLE))
		{       // Remove this model from the list
			RemoveIndex(i--);
		}
	}
}

void CLoadedModelGroup::RemoveNonNativeCars()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi) && !gPopStreaming.DoesCarModelBelongInCurrentPopulationZone(mi))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveBoats()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi) && ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetIsBoat())
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveTaxiVehs()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if(CModelInfo::IsValidModelInfo(mi) && CVehicle::IsTaxiModelId(fwModelId(strLocalIndex(mi))))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveCopVehs()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if(CModelInfo::IsValidModelInfo(mi) && CVehicle::IsLawEnforcementVehicleModelId(fwModelId(strLocalIndex(mi))))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveEmergencyServices()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if ( CModelInfo::IsValidModelInfo(mi) && ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EMERGENCY_SERVICE))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveBigVehicles()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi) && ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
        {
            // Remove this model from the list
			RemoveIndex(i--);
        }
    }
}

void CLoadedModelGroup::RemoveDiscardedVehicles()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];

		if (CModelInfo::IsValidModelInfo(mi) && gPopStreaming.GetDiscardedCars().IsMemberInGroup(mi))
		{
			// Remove this model from the list
			RemoveIndex(i--);
		}
	}
}

void CLoadedModelGroup::RemoveModelsThatHaveHitMaximumNumber()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];
        if (!CModelInfo::IsValidModelInfo(mi))
            continue;

        CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
        if (((vmi->GetNumVehicleModelRefs() + vmi->GetNumVehicleModelRefsParked()) >= vmi->GetVehicleMaxNumber() || !vmi->HasAnyColorsToSpawn()))
        {       // Remove this model from the list
			RemoveIndex(i--);
        }
    }

	RemoveRareModels();
}

void CLoadedModelGroup::RemoveRareModels()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];

		// check for rare vehicles, maximum number is 1 for those
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
			if (vmi->GetNumVehicleModelRefs() + vmi->GetNumVehicleModelRefsParked() > 0)
			{
				for (s32 f = 0; f < CPopCycle::GetPopGroups().GetVehCount(); ++f)
				{
					if (CPopCycle::GetPopGroups().GetVehGroup(f).IsFlagSet(POPGROUP_RARE) && CPopCycle::GetPopGroups().IsVehIndexMember(f, mi))
					{
						// remove this model
						RemoveIndex(i--);
						break;
					}
				}
			}
		}
	}
}

#define CUTOFF_DIST (10.0f)

void CLoadedModelGroup::RemoveModelsThatAreUsedNearby(const Vector3 *coors)
{
	const Vec3V posV = VECTOR3_TO_VEC3V(*coors);
	ScalarV radiusV(CUTOFF_DIST);

	CSpatialArrayNode* results[64];

	const int numFound = CPed::GetSpatialArray().FindInCylinderXY(posV.GetXY(), radiusV, results, NELEM(results));
	for(int i = 0; i < numFound; i++)
	{
		CPed* pPed = CPed::GetPedFromSpatialArrayNode(results[i]);
		RemoveMember(pPed->GetModelIndex());
	}

	// check for models that are queued to spawn in the ped population system
	CLoadedModelGroup tempGroup;
	tempGroup.Copy(this);
	for(int i = 0; i < tempGroup.CountMembers(); i++)
	{		
		u32 modelIndex = tempGroup.GetMember(i);
		float scheduledDistSqr = CPedPopulation::GetDistanceSqrToClosestScheduledPedWithModelId(*coors, modelIndex);
		if(scheduledDistSqr < (CUTOFF_DIST*CUTOFF_DIST))
		{
			RemoveMember(modelIndex);			
		}
	}
}

void CLoadedModelGroup::RemoveNonMotorcyclePeds()
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi))
        {
            if (!gPopStreaming.CanPedModelRideBike(mi) || CModelInfo::GetAssetsAreDeletable(fwModelId(strLocalIndex(mi))))
            {       // Remove this model from the list
				RemoveIndex(i--);
            }
        }
    }
}

void CLoadedModelGroup::RemoveVehModelsWithFlagSet(CVehicleModelInfoFlags::Flags flag, bool flagValue)
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi))
        {
			bool isFlagSet = ((CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi))))->GetVehicleFlag(flag);
            if (isFlagSet == flagValue)
            {       // Remove this model from the list
				RemoveIndex(i--);
            }
        }
    }
}

void CLoadedModelGroup::RemovePedModelsThatCantDriveCars(bool forDriver)
{
    for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
    {
        u32 mi = aMembers[i];

        if (CModelInfo::IsValidModelInfo(mi))
        {
            CPedModelInfo *pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
            if (((!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_POOR_CAR)) && (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_AVERAGE_CAR)) && (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_RICH_CAR)) &&
                (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(PED_DRIVES_BIG_CAR))) || CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(mi) || pModelInfo->GetIsCop() || (!forDriver && !pModelInfo->GetCanSpawnInCar())
				|| pModelInfo->GetOnlyBulkyItemVariations()	// Peds with only bulky item variations can't be in cars [6/26/2013 mdawe]
				)
            {       // Remove this model from the list
				RemoveIndex(i--);
            }
        }
    }
}

void CLoadedModelGroup::RemoveSuppresedPeds()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];

		if (CModelInfo::IsValidModelInfo(mi))
		{
			if (CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(mi))
			{       // Remove this model from the list
				RemoveIndex(i--);
			}
		}
	}
}

void CLoadedModelGroup::RemovePedsThatCantDriveCarType(ePedVehicleTypes carType)
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];

		if (CModelInfo::IsValidModelInfo(mi))
		{
			CPedModelInfo *pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
			if (!pModelInfo->GetPersonalitySettings().DrivesVehicleType(carType))
			{       // Remove this model from the list
				RemoveIndex(i--);
			}
		}
	}
}

void CLoadedModelGroup::RemoveSuppressedAndNonAmbientCars()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));

            if (vmi->GetVehicleType() == VEHICLE_TYPE_BOAT)
                continue;

			// remove cars that shouldn't spawn ambiently on roads
			if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(mi) || vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_SPAWN_AS_AMBIENT))
			{
				RemoveIndex(i--);
			}	
		}
	}
}

void CLoadedModelGroup::RemoveMaleOrFemale(bool male)
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));

            // remove either all males or female, as specified by the parameter
			if (male != pmi->IsMale())
			{
				RemoveIndex(i--);
			}	
		}
	}
}

void CLoadedModelGroup::RemoveNonPassengerPeds()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
			if (!pmi->GetCanSpawnInCar())
			{
				RemoveIndex(i--);
			}	
		}
	}
}

void CLoadedModelGroup::RemoveBulkyItemPeds()
{
	for (int i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; i++)
	{
		u32 mi = aMembers[i];
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));
			if (pmi->GetOnlyBulkyItemVariations()) // Peds with only bulky item variations can't be in cars [6/26/2013 mdawe]
			{
				RemoveIndex(i--);
			}	
		}
	}
}

void CLoadedModelGroup::RemoveIndex(s32 index)
{
	for (s32 i = index; i < MAX_NUM_IN_LOADED_MODEL_GROUP - 1; ++i)
	{
		aMembers[i] = aMembers[i + 1];
	}

	aMembers[MAX_NUM_IN_LOADED_MODEL_GROUP - 1] = fwModelId::MI_INVALID;
}

bool includeScheduledCargens = false;
const Vector3* comparePos = NULL;
int CompareCarDistance(const void* lhs, const void* rhs)
{
	Assert(comparePos);

	const u32 a = *(const u32*)lhs;
	const u32 b = *(const u32*)rhs;

    if (a == fwModelId::MI_INVALID)
        return 1;
    if (b == fwModelId::MI_INVALID)
        return -1;

	CVehicleModelInfo* aVmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(a)));
	CVehicleModelInfo* bVmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(b)));

	float aDist = aVmi ? aVmi->GetDistanceSqrToClosestInstance(*comparePos, includeScheduledCargens) : 0.f;
	float bDist = bVmi ? bVmi->GetDistanceSqrToClosestInstance(*comparePos, includeScheduledCargens) : 0.f;

	return (aDist > bDist) ? -1 : 1;
}

void PopulationStreamingDependencyCost(strLocalIndex assetIndex, s32 streamingModuleId, atArray<strIndex>& allDeps, u32& totalMain, u32& totalVram)
{
	strIndex backingNewDepsStore[STREAMING_MAX_DEPENDENCIES*2];
	atUserArray<strIndex> newDeps(backingNewDepsStore, STREAMING_MAX_DEPENDENCIES*2);
	CStreaming::GetObjectAndDependencies(assetIndex, streamingModuleId, newDeps, NULL, 0);

	for (s32 i = 0; i < newDeps.GetCount(); ++i)
	{
		if (allDeps.Find(newDeps[i]) == -1)
		{
			totalMain += strStreamingEngine::GetInfo().GetObjectVirtualSize(newDeps[i]);
			totalVram += strStreamingEngine::GetInfo().GetObjectPhysicalSize(newDeps[i]);

			if( Verifyf( allDeps.GetCapacity() > allDeps.GetCount(), "PopulationStreamingDependencyCost: Passed deps array has insufficient space. Skipping!" ) )
			{
				allDeps.Append() = newDeps[i];
			}
		}
	}
}

u32 GetCarMemoryUse(const u32 carIdx)
{
	strLocalIndex modelIndex = strLocalIndex(carIdx);
	fwModelId modelId(modelIndex);
	CVehicleModelInfo* modelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

	strIndex backingDepStore[STREAMING_MAX_DEPENDENCIES*4];
	atUserArray<strIndex> deps(backingDepStore, STREAMING_MAX_DEPENDENCIES*4);

	strIndex backingHDDepsStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> hdDeps(backingHDDepsStore, STREAMING_MAX_DEPENDENCIES);

	deps.ResetCount();

	// find out HD status
	u32 virtualSizeHd  = 0;
	u32 physicalSizeHd = 0;
	if (modelInfo->GetAreHDFilesLoaded())
	{
		fwModelId modelId(modelIndex);
		hdDeps.ResetCount();
		CModelInfo::GetObjectAndDependencies(modelId, hdDeps, NULL, 0);


		// if hd assets aren't dependencies, add them to the memory. some vehicles don't have hd assets so these
		// indices are the regular txd/frag indices, don't want to count them twice
		if (modelInfo->GetHDTxdIndex() != -1)
		{
			strIndex hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

			if (hdDeps.Find(hdTxdIndex) == -1)
			{
				deps.Append() = hdTxdIndex;
				virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
			}
		}

		if (modelInfo->GetHDFragmentIndex() != -1)
		{
			strIndex hdFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDFragmentIndex()));

			if (hdDeps.Find(hdFragIndex) == -1)
			{
				deps.Append() = hdFragIndex;
				virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
				physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
			}
		}
	}

	// get potential mods
	u32 totalVirtualMods = 0;
	u32 totalPhysicalMods = 0;
	for (s32 f = 0; f < modelInfo->GetNumVehicleInstances(); ++f)
	{
		if (modelInfo->GetVehicleInstance(f))
		{
			CVehicleStreamRenderGfx* gfx = modelInfo->GetVehicleInstance(f)->GetVehicleDrawHandler().GetVariationInstance().GetVehicleRenderGfx();
			if (gfx)
			{
				for (u8 g = 0; g < VMT_RENDERABLE + MAX_LINKED_MODS; ++g)
				{
					if (gfx->GetFragIndex(g) != -1)
						PopulationStreamingDependencyCost(gfx->GetFragIndex(g), g_FragmentStore.GetStreamingModuleId(), deps, totalVirtualMods, totalPhysicalMods);
				}
			}
		}
	}

	// Find size of the vehicle model
	u32 virtualSize  = 0;
	u32 physicalSize = 0;
	CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount(), true);

	virtualSize += virtualSizeHd + totalVirtualMods;
	physicalSize += physicalSizeHd + totalPhysicalMods;

	return virtualSize + physicalSize;
}

u32 GetPedMemoryUse(const u32 pedIdx)
{
	strLocalIndex modelIndex = strLocalIndex(pedIdx);
	fwModelId modelId(modelIndex);
	CPedModelInfo* modelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

	strIndex backingDepsStore[STREAMING_MAX_DEPENDENCIES*6];
	atUserArray<strIndex> deps(backingDepsStore, STREAMING_MAX_DEPENDENCIES*6);

	strIndex backingHDDepsStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> hdDeps(backingHDDepsStore, STREAMING_MAX_DEPENDENCIES);

	deps.ResetCount();

	// find out HD status
	u32 virtualSizeHd  = 0;
	u32 physicalSizeHd = 0;
	if (modelInfo->GetAreHDFilesLoaded())
	{
		hdDeps.ResetCount();
		fwModelId modelId(modelIndex);
		CModelInfo::GetObjectAndDependencies(modelId, hdDeps, NULL, 0);

		if (modelInfo->GetHDTxdIndex() != -1)
		{
			strIndex hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

			// if hd assets aren't dependencies, add them to the memory. some peds don't have hd assets so these
			// indices are the regular txd indices, don't want to count them twice
			if (hdDeps.Find(hdTxdIndex) == -1)
			{
				deps.Append() = hdTxdIndex;
				virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
			}
		}
	}

	// get streamed assets
	u32 totalVirtualStreamed = 0;
	u32 totalPhysicalStreamed = 0;
	for (s32 f = 0; f < modelInfo->GetNumPedInstances(); ++f)
	{
		if (modelInfo->GetPedInstance(f))
		{
			CPedStreamRenderGfx* gfx = modelInfo->GetPedInstance(f)->GetPedDrawHandler().GetPedRenderGfx();
			if (gfx)
			{
				for (s32 g = 0; g < PV_MAX_COMP; ++g)
				{
					// drawables
					if (gfx->m_dwdIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_dwdIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}
					if (gfx->m_hdDwdIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_hdDwdIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}

					// textures
					if (gfx->m_txdIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_txdIdx[g]), g_TxdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}
					if (gfx->m_hdTxdIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_hdTxdIdx[g]), g_TxdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}

					// cloth
					if (gfx->m_cldIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_cldIdx[g]), g_ClothStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}

					// first person alternate drawables
					if (gfx->m_fpAltIdx[g] != -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_fpAltIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}
				}

				// head blend data
				for (u32 g = 0; g < HBS_MAX; ++g)
				{
					if (gfx->m_headBlendIdx[g] > -1)
					{
						PopulationStreamingDependencyCost(strLocalIndex(gfx->m_headBlendIdx[g]), (g < HBS_HEAD_DRAWABLES) ? g_DwdStore.GetStreamingModuleId() : (g < HBS_MICRO_MORPH_SLOTS ? g_TxdStore.GetStreamingModuleId() : g_DrawableStore.GetStreamingModuleId()), deps, totalVirtualStreamed, totalPhysicalStreamed);
					}
				}
			}
		}
	}

	// Find size of the ped model
	u32 virtualSize  = 0;
	u32 physicalSize = 0;
	CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount(), true);

	virtualSize += virtualSizeHd + totalVirtualStreamed;
	physicalSize += physicalSizeHd + totalPhysicalStreamed;

	return virtualSize + physicalSize;
}


int CompareCarMemoryUse(const void* lhs, const void* rhs)
{
	const u32 a = *(const u32*)lhs;
	const u32 b = *(const u32*)rhs;

	if (a == fwModelId::MI_INVALID)
		return 1;
	if (b == fwModelId::MI_INVALID)
		return -1;

	u32 aMem = GetCarMemoryUse(a);
	u32 bMem = GetCarMemoryUse(b);

	return (aMem > bMem) ? -1 : 1;
}

int CompareName(const void* lhs, const void* rhs)
{
	const u32 a = *(const u32*)lhs;
	const u32 b = *(const u32*)rhs;

	if (a == fwModelId::MI_INVALID)
		return 1;
	if (b == fwModelId::MI_INVALID)
		return -1;

	strLocalIndex modelIndex_a = strLocalIndex(a);
	strLocalIndex modelIndex_b = strLocalIndex(b);
	fwModelId amID(modelIndex_a);
	fwModelId bmID(modelIndex_b);
#if !__NO_OUTPUT
	return strcmp(CModelInfo::GetBaseModelInfoName(amID), CModelInfo::GetBaseModelInfoName(bmID));
#else
	return 0;
#endif
}

int ComparePedDistance(const void* lhs, const void* rhs)
{
	Assert(comparePos);

	const u32 a = *(const u32*)lhs;
	const u32 b = *(const u32*)rhs;

	if (a == fwModelId::MI_INVALID)
		return 1;
	if (b == fwModelId::MI_INVALID)
		return -1;

	CPedModelInfo* aPmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(a)));
	CPedModelInfo* bPmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(b)));

	float aDist = aPmi ? aPmi->GetDistanceSqrToClosestInstance(*comparePos) : 0.f;
	float bDist = bPmi ? bPmi->GetDistanceSqrToClosestInstance(*comparePos) : 0.f;

	return (aDist > bDist) ? -1 : 1;
}

int ComparePedMemoryUse(const void* lhs, const void* rhs)
{
	const u32 a = *(const u32*)lhs;
	const u32 b = *(const u32*)rhs;

	if (a == fwModelId::MI_INVALID)
		return 1;
	if (b == fwModelId::MI_INVALID)
		return -1;

	u32 aMem = GetPedMemoryUse(a);
	u32 bMem = GetPedMemoryUse(b);

	return (aMem > bMem) ? -1 : 1;
}

void CLoadedModelGroup::SortCarsByDistance(const Vector3& pos, bool cargens)
{
	comparePos = &pos;
	includeScheduledCargens = cargens;
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), CompareCarDistance);
}

void CLoadedModelGroup::SortCarsByMemoryUse()
{
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), CompareCarMemoryUse);
}

void CLoadedModelGroup::SortCarsByName()
{
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), CompareName);
}

void CLoadedModelGroup::SortPedsByDistance(const Vector3& pos)
{
	comparePos = &pos;
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), ComparePedDistance);
}

void CLoadedModelGroup::SortPedsByMemoryUse()
{
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), ComparePedMemoryUse);
}

void CLoadedModelGroup::SortPedsByName()
{
	const u32 numMembers = CountMembers();
	qsort(aMembers, numMembers, sizeof(u32), CompareName);
}

static void AppendModelDescription(char *description, unsigned length, u32 modelID)
{
#if !__FINAL
    snprintf(description, length, "%s %s", description, CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(modelID))));
#else
    snprintf(description, length, "%s %d", description, modelID);
#endif
}

bool CPopulationStreaming::IsCarDiscarded(u32 modelIndex)
{
	s32 numCars = m_DiscardedCars.CountMembers();
	for (s32 i = 0; i < numCars; ++i)
	{
		if (m_DiscardedCars.GetMember(i) == modelIndex)
			return true;
	}

	return false;
}

void CPopulationStreaming::PrintDebugForPeds()
{
#if DEBUG_DRAW

    const unsigned DEBUG_TEXT_LEN = 255;
    char debugText[DEBUG_TEXT_LEN];

    // display the car groups
    const unsigned NUM_PED_GROUPS = 4;
    const char *groupNames[] =
    {
        "Appropriate peds",
        "InAppropriate peds",
        "Special peds",
		"Discarded peds"
    };

    const CLoadedModelGroup *groups[] =
    {
        &m_AppropriateLoadedPeds,
        &m_InAppropriateLoadedPeds,
        &m_SpecialLoadedPeds,
		&m_DiscardedPeds,
    };

    for(unsigned group = 0; group < NUM_PED_GROUPS; group++)
    {
        snprintf(debugText, DEBUG_TEXT_LEN, "%s:", groupNames[group]);

        unsigned numCars = groups[group]->CountMembers();

        for(int index = 0; index < numCars; index++)
        {
            AppendModelDescription(debugText, DEBUG_TEXT_LEN, groups[group]->GetMember(index));
        }

        grcDebugDraw::AddDebugOutput(debugText);
    }

    if (NetworkInterface::IsGameInProgress())
    {
        snprintf(debugText, DEBUG_TEXT_LEN, "Network streamed peds:");

        unsigned numRequiredPeds = m_PedModelsRequiredForNetwork.CountMembers();

        for(unsigned index = 0; index < numRequiredPeds; index++)
        {
            u32 modelIndex = m_PedModelsRequiredForNetwork.GetMember(index);

			if(CModelInfo::IsValidModelInfo(modelIndex))
            {
                AppendModelDescription(debugText, DEBUG_TEXT_LEN, modelIndex);
            }
        }

        grcDebugDraw::AddDebugOutput(debugText);
    }

    // Print the suppressed and restricted ped models. (specific ped models can be suppressed by script)
    CScriptPeds::GetSuppressedPedModels().DisplaySuppressedModels();
	CScriptPeds::GetRestrictedPedModels().DisplayRestrictedModels();

    if(NetworkInterface::IsGameInProgress())
    {
        u32         currentTimeInterval = GetCurrentNetworkTimeInterval();
        u32         lowestRemoteOffset  = currentTimeInterval;
        const char *playerName          = "None";

        NetworkInterface::GetLowestPedModelStartOffset(lowestRemoteOffset, &playerName);

        snprintf(debugText, DEBUG_TEXT_LEN, "Ped Model Offset(Current:%d Locally Allowed:%d Remotely Allowed:%d (Lowest remote player is %s))", currentTimeInterval, m_LocalAllowedPedModelStartOffset, lowestRemoteOffset, playerName);

        grcDebugDraw::AddDebugOutput(debugText);
    }

    // Print some details regarding the peds in mem.
    CLoadedModelGroup allLoadedPeds;
    allLoadedPeds.Merge(&m_AppropriateLoadedPeds, &m_InAppropriateLoadedPeds, &m_SpecialLoadedPeds, &m_DiscardedPeds);

    const unsigned numLoadedPeds = allLoadedPeds.CountMembers();

    for (int index = 0; index < numLoadedPeds; index++)
    {
        u32 modelIndex = allLoadedPeds.GetMember(index);
		fwModelId modelId((strLocalIndex(modelIndex)));
        char infoString[128] = "";

        if (!CModelInfo::GetAssetRequiredFlag(modelId))
        {
            snprintf(infoString, DEBUG_TEXT_LEN, "Can be removed by streaming");
        }
        if (CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_MISSION_REQUIRED)
        {
            snprintf(infoString, DEBUG_TEXT_LEN, "%s  Required by script", infoString);
        }
        if (((CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId))->GetCountedAsScenarioPed())
        {
            snprintf(infoString, DEBUG_TEXT_LEN, "%s (Scenario)", infoString);
        }

        // Find size of the ped model

        u32 virtualSize  = 0;
        u32 physicalSize = 0;
        CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, m_residentObjects.GetElements(), m_residentObjects.GetCount(), true);

        char pedDescription[128];
        snprintf(pedDescription, 128, "%d", modelIndex);
        AppendModelDescription(pedDescription, 128, modelIndex);

        CPedModelInfo *pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

        if(pedModelInfo)
        {
            snprintf(debugText, DEBUG_TEXT_LEN, "%s TimeStreamed:%d Refs:%d OnFoot:%d InCar:%d Used:%d Size:%d %s %s",
                     pedDescription,
                     fwTimer::GetTimeInMilliseconds() - GetTimeModelStreamedIn(modelIndex),
                     pedModelInfo->GetNumPedModelRefs(),
                     pedModelInfo->GetNumTimesOnFoot(),
                     pedModelInfo->GetNumTimesInCar(),
                     pedModelInfo->GetNumTimesUsed(),
                     virtualSize+physicalSize,
                     infoString,
					 pedModelInfo->m_isRequestedAsDriver ? "driver" : "");

			if (m_DiscardedPeds.IsMemberInGroup(modelIndex))
			{
		        grcDebugDraw::AddDebugOutput(Color32(200, 0, 0), debugText);
				safecpy(debugText, "Instances: ");
				char addr[16];

				CPed::Pool* pool = CPed::GetPool();
				for (s32 i = 0; i < pool->GetSize(); ++i)
				{
					CPed* ped = pool->GetSlot(i);
					if (ped && ped->GetModelIndex() == modelIndex)
					{
						snprintf(addr, DEBUG_TEXT_LEN, " 0x%8p ", ped);
						safecat(debugText, addr);
					}
				}
				
		        grcDebugDraw::AddDebugOutput(Color32(200, 0, 0), debugText);
			}
			else
			{
				grcDebugDraw::AddDebugOutput(debugText);
			}
        }
    }

    snprintf(debugText, DEBUG_TEXT_LEN, "Memory used by peds: %d / %d (over %d)", m_TotalMemoryUsedByPedModelsMain, GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel), m_MemOverBudgetMainPeds + m_MemOverBudgetVramPeds);
    grcDebugDraw::AddDebugOutput(debugText);

#if !MERGED_BUDGET 
    snprintf(debugText, DEBUG_TEXT_LEN, "Physical memory used by peds: %d / %d (over %d)", m_TotalMemoryUsedByPedModelsVram, GetMemForPeds(MEMTYPE_GAME_PHYSICAL, s_MemForPedsLevel), m_MemOverBudgetVramPeds);
    grcDebugDraw::AddDebugOutput(debugText);
#endif // !MERGED_BUDGET 
	
	snprintf(debugText, DEBUG_TEXT_LEN, "Streamed memory used by peds: %d", m_TotalStreamedPedMemoryMain + m_TotalStreamedPedMemoryVram);
	grcDebugDraw::AddDebugOutput(debugText);
#endif // DEBUG_DRAW
}

void CPopulationStreaming::InitialVehicleRequest(u32 numRequests)
{
    NOTFINAL_ONLY(if(gbAllowVehGenerationOrRemoval))
    {
	    for (u32 i = 0; i < numRequests; ++i)
	    {
			if (IsThereAnyMemoryLeft(MTT_VEHICLE))
		    {
			    StreamOneNewCar();
		    }
	    }
    }
}

void CPopulationStreaming::ResetVehMemoryBudgetLevel()
{
	SetVehMemoryBudgetLevel(DEFAULT_MEM_FOR_VEHICLES_LEVEL);
}

void CPopulationStreaming::SetVehMemoryBudgetLevel(u32 level)
{
	Assert(level < MAX_MEM_LEVELS);
	if (level >= MAX_MEM_LEVELS)
		return;

	s_MemForVehiclesLevel = level;
	
	CVehiclePopulation::ms_vehicleMemoryBudgetMultiplier = (float)level / (MAX_MEM_LEVELS - 1);
}


void CPopulationStreaming::ResetPedMemoryBudgetLevel()
{
	SetPedMemoryBudgetLevel(DEFAULT_MEM_FOR_PEDS_LEVEL);
}

void CPopulationStreaming::SetPedMemoryBudgetLevel(u32 level)
{
	Assert(level < MAX_MEM_LEVELS);
	if (level >= MAX_MEM_LEVELS)
		return;

	s_MemForPedsLevel = level;

	CPedPopulation::ms_pedMemoryBudgetMultiplier = (float)level / (MAX_MEM_LEVELS - 1);
}

bool CPopulationStreaming::IsVehicleInAllowedNetworkArray(u32 modelIndex) const
{
    if (!NetworkInterface::IsGameInProgress() || modelIndex == fwModelId::MI_INVALID)
		return true;

    if(m_VehicleModelsRequiredForNetwork.IsMemberInGroup(modelIndex))
        return true;

    return false;
}

void CPopulationStreaming::FlushAllVehicleModelsHard()
{
	// Disable defragging - we're about to unload some files.
#if USE_DEFRAGMENTATION
	bool defragEnabled = strStreamingEngine::GetDefragmentation()->GetEnabled();
	strStreamingEngine::GetDefragmentation()->FlushAndDisable();
#endif // USE_DEFRAGMENTATION

	CRandomEventManager::GetInstance().FlushCopAsserts();
	CStreaming::LoadAllRequestedObjects();
	CDispatchManager::GetInstance().Flush();
	CVehiclePopulation::RemoveAllVehsHard();
	CVehicleFactory::GetFactory()->ClearDestroyedVehCache();
	gRenderThreadInterface.Flush();
	CObjectPopulation::ManageObjectPopulation(true, true); // parts that fell off a vehicle will be objects but use vehicle drawables, keeping references
	gPopStreaming.RemoveStreamedCarModelsOnShutdown();

	Assertf(!CVehicle::GetPool()->GetNoOfUsedSpaces(), "%d vehicle instances still present after a vehicle flush!", (int) CVehicle::GetPool()->GetNoOfUsedSpaces());

#if DEBUG_POPULATION_MEMORY
	for (s32 i = 0; i < MAX_STREAMED_IN_MODELS; ++i)
	{
		if (gPopStreaming.m_StreamedInModelData[i].m_ModelIndex != fwModelId::MI_INVALID && CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(gPopStreaming.m_StreamedInModelData[i].m_ModelIndex))))
		{
			Assertf(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(gPopStreaming.m_StreamedInModelData[i].m_ModelIndex)))->GetModelType() != MI_TYPE_VEHICLE, "CPopulationStreaming::FlushAllVehicleModelsHard: Non cleaned up model data for model '%s' found. M%dK V%dK HD M%dK V%dK", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(gPopStreaming.m_StreamedInModelData[i].m_ModelIndex)))->GetModelName(), gPopStreaming.m_StreamedInModelData[i].m_mainUsed>>10, gPopStreaming.m_StreamedInModelData[i].m_vramUsed>>10, gPopStreaming.m_StreamedInModelData[i].m_HDAssetSizeMain>>10, gPopStreaming.m_StreamedInModelData[i].m_HDAssetSizeVram>>10);
		}
	}

	for (s32 i = 0; i < gPopStreaming.m_vehicleComponents.GetCount(); ++i)
	{
		if (gPopStreaming.m_vehicleComponents[i].refCount != 0 && gPopStreaming.m_StreamedInModelData[i].m_ModelIndex != fwModelId::MI_INVALID)
		{
			Assertf(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(gPopStreaming.m_StreamedInModelData[i].m_ModelIndex)))->GetModelType() != MI_TYPE_VEHICLE, "CPopulationStreaming::FlushAllVehicleModelsHard: Found vehicle component with ref count %d, streaming index: %d!", gPopStreaming.m_vehicleComponents[i].refCount, gPopStreaming.m_vehicleComponents[i].streamIdx.Get());
		}
	}
#endif // DEBUG_POPULATION_MEMORY

#if USE_DEFRAGMENTATION
	if (defragEnabled)
		strStreamingEngine::GetDefragmentation()->Enable();
#endif // USE_DEFRAGMENTATION
}

void CPopulationStreaming::FlushAllVehicleModelsSoft()
{
//	CPlayerSwitchMgrLong::SetState() calls this. A switch could happen at any time during the game so it's not safe for us to delete any script vehicles or unload any script-requested models
	CRandomEventManager::GetInstance().FlushCopAsserts();
	CDispatchManager::GetInstance().Flush();
	CVehiclePopulation::RemoveAllVehsSoft();	//	we can't remove any script vehicles so call RemoveAllVehsSoft() instead of RemoveAllVehsHard()
	CVehicleFactory::GetFactory()->ClearDestroyedVehCache();
	gRenderThreadInterface.Flush();
	CObjectPopulation::ManageObjectPopulation(true, true); // parts that fell off a vehicle will be objects but use vehicle drawables, keeping references
//	gPopStreaming.RemoveStreamedCarModelsOnShutdown();	//	This function unloads models that have been requested by script so it's not safe to call here
}

#if !__FINAL
//PURPOSE
// Gets a string name of the slot for debugging purposes.
const char* CPopulationStreaming::GetScenarioSlotName(eScenarioPopStreamingSlot eSlotType)
{
	switch(eSlotType)
	{
	case SCENARIO_POP_STREAMING_NORMAL:
		{
			return "Normal Scenario Peds";
		}
	case SCENARIO_POP_STREAMING_SMALL:
		{
			return "Small Scenario Peds";
		}
	default:
		{
			return "Unknown Type Scenario Peds";
		}
	}
}
#endif

bool CPopulationStreaming::IsNetworkModelOffsetLower(u32 firstModelOffset, u32 secondModelOffset)
{
    bool isLower = false;

    // as the model offsets can wrap we need to take this into account when deciding whether which start
    // offset is the lower. As the model offset changes relatively infrequently, we are considering the top
    // quarter of the range to be less than the first quarter of the range. Other ranges are handled normally
    const unsigned QUARTER_RANGE = MAX_NETWORK_MODEL_INTERVALS / 4;

    if(firstModelOffset  < secondModelOffset ||
      ((firstModelOffset > (MAX_NETWORK_MODEL_INTERVALS - QUARTER_RANGE)) && (secondModelOffset < QUARTER_RANGE)))
    {
        isLower = true;
    }

    return isLower;
}

void CPopulationStreaming::PrintCarGroups()
{
    const unsigned DEBUG_TEXT_LEN = 255;
    char debugText[DEBUG_TEXT_LEN];

    // display the appropriate car groups, this
    // needs special handling as we need to check whether
    // the vehicles are appropriate for the current zone
    unsigned numCars = m_AppropriateLoadedCars.CountMembers();

    char validForZone[DEBUG_TEXT_LEN];
    char invalidForZone[DEBUG_TEXT_LEN];
    snprintf(validForZone, DEBUG_TEXT_LEN, "Appropriate cars (belonging in this zone):");
    snprintf(invalidForZone, DEBUG_TEXT_LEN, "Appropriate cars (not belonging in this zone):");

    for(int index = 0; index < numCars; index++)
    {
        int modelID = m_AppropriateLoadedCars.GetMember(index);

        if(DoesCarModelBelongInCurrentPopulationZone(modelID))
        {
            AppendModelDescription(validForZone, DEBUG_TEXT_LEN, modelID);
        }
        else
        {
            AppendModelDescription(invalidForZone, DEBUG_TEXT_LEN, modelID);
        }
    }

#if DEBUG_DRAW
    grcDebugDraw::AddDebugOutput(validForZone);
    grcDebugDraw::AddDebugOutput(invalidForZone);
#endif

    // display the other car groups
    const unsigned NUM_CAR_GROUPS = 4;
    const char *groupNames[] =
    {
		"Discarded cars",
        "InAppropriate cars",
        "Special cars",
        "Boats"
    };

    const CLoadedModelGroup *groups[] =
    {
		&m_DiscardedCars,
        &m_InAppropriateLoadedCars,
        &m_SpecialLoadedCars,
        &m_LoadedBoats
    };

    for(unsigned group = 0; group < NUM_CAR_GROUPS; group++)
    {
        snprintf(debugText, DEBUG_TEXT_LEN, "%s:", groupNames[group]);

        unsigned numCars = groups[group]->CountMembers();

        for(int index = 0; index < numCars; index++)
        {
            AppendModelDescription(debugText, DEBUG_TEXT_LEN, groups[group]->GetMember(index));
        }

#if DEBUG_DRAW
        grcDebugDraw::AddDebugOutput(debugText);
#endif
    }
}

void CPopulationStreaming::PrintNetworkCarGroups()
{
    const unsigned DEBUG_TEXT_LEN = 255;
    char debugText[DEBUG_TEXT_LEN];

    const unsigned NUM_CAR_GROUPS = 4;
    const char *groupNames[] =
    {
        "Appropriate cars",
        "InAppropriate cars",
		"Discarded cars",
        "Special cars",
        "Boats"
    };

    const CLoadedModelGroup *groups[] =
    {
        &m_AppropriateLoadedCars,
        &m_InAppropriateLoadedCars,
		&m_DiscardedCars,
        &m_SpecialLoadedCars,
        &m_LoadedBoats
    };

    for(unsigned group = 0; group < NUM_CAR_GROUPS; group++)
    {
        snprintf(debugText, DEBUG_TEXT_LEN, "%s:", groupNames[group]);

        unsigned numCars = groups[group]->CountMembers();

        for(int index = 0; index < numCars; index++)
        {
            AppendModelDescription(debugText, DEBUG_TEXT_LEN, groups[group]->GetMember(index));
        }

#if DEBUG_DRAW
        grcDebugDraw::AddDebugOutput(debugText);
#endif
    }
}

bool CPopulationStreaming::IsThereAnyMemoryLeft(eMemoryTrackType type)
{
	switch (type)
	{
	case MTT_PED:
		{
			// keep a buffer at the end of the ped budget to account for half the scenario models needed in current zone
            // plus the cost for one ped (the new ped to stream in)
			int limit = (m_MaxScenarioPedModelsLoadedOverride >= 0 ? m_MaxScenarioPedModelsLoadedOverride : m_MaxScenarioPedModelsLoadedPerSlot);
			limit = limit >> 1;
            limit++;

			const s32 pedBuffer = limit * BUFFER_MEM_FOR_PED_MODELS;

#if !MERGED_BUDGET 
			const s32 pedBufferVram = limit * BUFFER_MEM_FOR_PED_MODELS_VRAM;
#endif // !MERGED_BUDGET

			return 
#if !MERGED_BUDGET 
				(m_TotalMemoryUsedByPedModelsVram < (GetMemForPeds(MEMTYPE_GAME_PHYSICAL, s_MemForPedsLevel) - pedBufferVram)) && 
#endif // !MERGED_BUDGET
				(m_TotalMemoryUsedByPedModelsMain < (GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel) - pedBuffer));
		}
		break;
	case MTT_VEHICLE:
	case MTT_WEAPON:
		{
			return ((m_TotalMemoryUsedByVehicleModelsMain < (GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel) - BUFFER_MEM_FOR_VEHICLE_MODELS))
#if !MERGED_BUDGET 
				&& (m_TotalMemoryUsedByVehicleModelsVram < (GetMemForVehicles(MEMTYPE_GAME_PHYSICAL, s_MemForVehiclesLevel) - BUFFER_MEM_FOR_VEHICLE_MODELS_VRAM))
#endif
					);
		}
		break;
	default:
		break;
	}

	return true;
}

bool CPopulationStreaming::AreWeOverBudget(eMemoryTrackType type)
{
	switch (type)
	{
	case MTT_PED:
		return (m_MemOverBudgetMainPeds + m_MemOverBudgetVramPeds) > 0;
	case MTT_VEHICLE:
	case MTT_WEAPON:
		return (m_MemOverBudgetMainVehs + m_MemOverBudgetVramVehs) > 0;
	default:
		break;
	}

	return false;
}

#if __BANK
atArray<void*> s_allocs;
void CPopulationStreaming::AllocateVehicleAndPedResourceMem()
{	
	MEM_USE_USERDATA(MEMUSERDATA_POPULATION);

	USE_MEMBUCKET(MEMBUCKET_DEFAULT);

	strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();

	for (u32 uMemType = 0; uMemType < (MERGED_BUDGET ? 1 : 2); uMemType++)
	{
		eMemoryType eType = (uMemType == 0) ? MEMTYPE_GAME_VIRTUAL : MEMTYPE_GAME_PHYSICAL;

		size_t taken = 0;
		size_t memory = GetMemForPeds(eType, MAX_MEM_LEVELS-1) + GetMemForVehicles(eType, MAX_MEM_LEVELS-1);
		memory -= (eType == MEMTYPE_GAME_VIRTUAL) ? m_TotalMemoryUsedByPedModelsMain + m_TotalMemoryUsedByVehicleModelsMain :
													   m_TotalMemoryUsedByPedModelsVram + m_TotalMemoryUsedByVehicleModelsVram;

		size_t leafSize = (eType == MEMTYPE_GAME_VIRTUAL) ? g_rscVirtualLeafSize : g_rscPhysicalLeafSize;
		size_t chunk = leafSize << 8;
		while (memory > leafSize)
		{
			while (chunk > memory)
			{
				chunk >>= 1;
			}

			if (chunk < leafSize)
				break;
			
			void* alloc = allocator.TryAllocate(chunk, 128, eType);
			if (alloc) 
			{
				s_allocs.Grow() = alloc;
				memory -= chunk;
				taken += chunk;
			}
			else
			{
				chunk >>= 1;
			}
		}

// 		Displayf("Stolen MAIN %dKb + PEDS %dKb + VEHICLES %dKb = %dKb (remain %dKb)", 
// 			taken>>10,m_TotalMemoryUsedByPedModelsMain>>10,m_TotalMemoryUsedByVehicleModelsMain>>10,
// 			(taken+m_TotalMemoryUsedByPedModelsMain+m_TotalMemoryUsedByVehicleModelsMain)>>10,
// 			memory>>10);
	}
}

void CPopulationStreaming::FreeVehicleAndPedResourceMem()
{
	MEM_USE_USERDATA(MEMUSERDATA_POPULATION);

	strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
	for (int i = 0; i < s_allocs.GetCount();++i)
	{
		allocator.Free(s_allocs[i]);
	}
	s_allocs.Reset();
}
#endif // __BANK

extern bool g_maponlyHogEnabled;

void CPopulationStreaming::AddTotalMemoryUsed(s32 main, s32 vram, eMemoryTrackType type)
{
#if __BANK
	static int recursioncount = 0;

	if (g_maponlyHogEnabled && (main != 0 || vram != 0) && recursioncount == 0)
	{
		++recursioncount;
		FreeVehicleAndPedResourceMem();
	}
#endif // BANK

	switch (type)
	{
	case MTT_PED:
		{
#if MERGED_BUDGET 
			m_TotalMemoryUsedByPedModelsMain += main + vram;
			m_MemOverBudgetMainPeds = rage::Max(0, (s32)(m_TotalMemoryUsedByPedModelsMain - GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel)));
#else
			m_TotalMemoryUsedByPedModelsMain += main;
			m_TotalMemoryUsedByPedModelsVram += vram;
			m_MemOverBudgetMainPeds = rage::Max(0, (s32)(m_TotalMemoryUsedByPedModelsMain - GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel)));
			m_MemOverBudgetVramPeds = rage::Max(0, (s32)(m_TotalMemoryUsedByPedModelsVram - GetMemForPeds(MEMTYPE_GAME_PHYSICAL, s_MemForPedsLevel)));
#endif
		}
		break;
	case MTT_WEAPON:
#if MERGED_BUDGET 
			m_TotalWeaponMemoryMain += main + vram;
#else
			m_TotalWeaponMemoryMain += main;
			m_TotalWeaponMemoryVram += vram;
#endif
			// NOTE: no break; on purpose, we want weapons to count in the vehicle budget
	case MTT_VEHICLE:
		{
#if MERGED_BUDGET 
			m_TotalMemoryUsedByVehicleModelsMain += main + vram;
			m_MemOverBudgetMainVehs = rage::Max(0, (s32)(m_TotalMemoryUsedByVehicleModelsMain - GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel)));
#else
			m_TotalMemoryUsedByVehicleModelsMain += main;
			m_TotalMemoryUsedByVehicleModelsVram += vram;
			m_MemOverBudgetMainVehs = rage::Max(0, (s32)(m_TotalMemoryUsedByVehicleModelsMain - GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel)));
			m_MemOverBudgetVramVehs = rage::Max(0, (s32)(m_TotalMemoryUsedByVehicleModelsVram - GetMemForVehicles(MEMTYPE_GAME_PHYSICAL, s_MemForVehiclesLevel)));
#endif
		}
		break;
	default:
		break;
	}

#if __BANK
	if (g_maponlyHogEnabled && (main != 0 || vram != 0) && recursioncount == 1)
	{
		AllocateVehicleAndPedResourceMem();
		--recursioncount;
	}
#endif // __BANK
}

bool CPopulationStreaming::IsAllowedToRequestScenarioPedModel(bool highPri, bool startupMode, eScenarioPopStreamingSlot eSlotNumber) const
{
	// At least for now, since we've been running without a limit, it's probably
	// safest to not respect the limit if a new model is needed for a high priority scenario.
	if(!highPri)
	{
		// If we recently switched population zones, deny requests for new scenario models, to give some generic
		// peds a chance to stream in first. Those are important for scenarios too.
		if(!startupMode && fwTimer::GetTimeInMilliseconds() < m_ScenarioPedStreamingDisabledUntilTimeMS)
		{
			streamDebugf2("IsAllowedToRequestScenarioPedModel() returning false due to recent pop zone switch.");
			return false;
		}

		// Get the limit, respecting script overrides.
		const int limit = (m_MaxScenarioPedModelsLoadedOverride >= 0 ? m_MaxScenarioPedModelsLoadedOverride : m_MaxScenarioPedModelsLoadedPerSlot);
		if(m_aNumScenarioPedModelsLoaded[eSlotNumber] >= limit)
		{
			return false;
		}
	}
	return true;
}

void CPopulationStreaming::PrintDebugForVehicles()
{
#if DEBUG_DRAW
    const unsigned DEBUG_TEXT_LEN = 255;
    char debugText[DEBUG_TEXT_LEN];

#if MERGED_BUDGET 
    snprintf(debugText, DEBUG_TEXT_LEN, "Memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsMain, GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel), m_MemOverBudgetMainVehs + m_MemOverBudgetVramVehs);
    grcDebugDraw::AddDebugOutput(debugText);
	snprintf(debugText, DEBUG_TEXT_LEN, "Streamed memory used by vehicles: %d", m_TotalStreamedVehicleMemoryMain + m_TotalStreamedVehicleMemoryVram);
	grcDebugDraw::AddDebugOutput(debugText);
	snprintf(debugText, DEBUG_TEXT_LEN, "Memory used by weapons: %d", m_TotalWeaponMemoryMain + m_TotalWeaponMemoryVram);
	grcDebugDraw::AddDebugOutput(debugText);
#else
    snprintf(debugText, DEBUG_TEXT_LEN, "Main memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsMain, GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel), m_MemOverBudgetMainVehs);
    grcDebugDraw::AddDebugOutput(debugText);
	snprintf(debugText, DEBUG_TEXT_LEN, "Physical memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsVram, GetMemForVehicles(MEMTYPE_GAME_PHYSICAL, s_MemForVehiclesLevel), m_MemOverBudgetVramVehs);
    grcDebugDraw::AddDebugOutput(debugText);
	snprintf(debugText, DEBUG_TEXT_LEN, "Streamed memory used by vehicles: m%d - v%d", m_TotalStreamedVehicleMemoryMain, m_TotalStreamedVehicleMemoryMain);
	grcDebugDraw::AddDebugOutput(debugText);
	snprintf(debugText, DEBUG_TEXT_LEN, "Memory used by weapons: m%d - v%d", m_TotalWeaponMemoryMain, m_TotalWeaponMemoryVram);
	grcDebugDraw::AddDebugOutput(debugText);
#endif // MERGED_BUDGET 
#endif // DEBUG_DRAW

    // display the car groups
    if(NetworkInterface::IsGameInProgress())
    {
        PrintNetworkCarGroups();
    }
    else
    {
        PrintCarGroups();
    }

    // Print the suppressed vehicles. (specific car models can be suppressed by script)
    CScriptCars::GetSuppressedCarModels().DisplaySuppressedModels();

#if DEBUG_DRAW
    if(NetworkInterface::IsGameInProgress())
    {
        u32         currentTimeInterval = GetCurrentNetworkTimeInterval();
        u32         lowestRemoteOffset  = currentTimeInterval;
        const char *playerName          = "None";

        NetworkInterface::GetLowestVehicleModelStartOffset(lowestRemoteOffset, &playerName);

        snprintf(debugText, DEBUG_TEXT_LEN, "Vehicle Model Offset(Current:%d Locally Allowed:%d Remotely Allowed:%d (Lowest remote player is %s))", currentTimeInterval, m_LocalAllowedVehicleModelStartOffset, lowestRemoteOffset, playerName);

        grcDebugDraw::AddDebugOutput(debugText);

        snprintf(debugText, DEBUG_TEXT_LEN, "Model Variation Set ID: %d", m_NetworkModelVariationID);
        grcDebugDraw::AddDebugOutput(debugText);
    }

    // Print some details regarding the cars in mem.
    CLoadedModelGroup cars;
    cars.Merge(&m_AppropriateLoadedCars, &m_InAppropriateLoadedCars, &m_SpecialLoadedCars, &m_LoadedBoats);
    CLoadedModelGroup allLoadedCars;
	allLoadedCars.Merge(&cars, &m_DiscardedCars);

	s32 numMembers = allLoadedCars.CountMembers();
    for (int index = 0; index < numMembers; index++)
    {
        u32 modelIndex = allLoadedCars.GetMember(index);
		fwModelId modelId((strLocalIndex(modelIndex)));
        char infoString[128] = "";

        if (!CModelInfo::GetAssetRequiredFlag(modelId))
        {
            snprintf(infoString, DEBUG_TEXT_LEN, "Can be removed by streaming");
        }
        if (CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_MISSION_REQUIRED)
        {
            snprintf(infoString, DEBUG_TEXT_LEN, "%s  Required by script", infoString);
        }

        // Find size of the vehicle model
        u32 virtualSize  = 0;
        u32 physicalSize = 0;
        CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, m_residentObjects.GetElements(), m_residentObjects.GetCount(), true);

        char modelDescription[128];
        formatf(modelDescription, 128, "%d", modelIndex);
        AppendModelDescription(modelDescription, 128, modelIndex);
        CVehicleModelInfo* vmi = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
        if (!vmi->m_data)
            continue;
        snprintf(debugText, DEBUG_TEXT_LEN, "%s%s TimeStreamed:%d Refs(parked):%d(%d) Used:%d %s Size:%d",
                modelDescription,
                vmi->GetAreHDFilesLoaded() ? "(HD)" : "",
                fwTimer::GetTimeInMilliseconds() - GetTimeModelStreamedIn(modelIndex),
                vmi->GetNumVehicleModelRefs(),
                vmi->GetNumVehicleModelRefsParked(),
                vmi->GetNumTimesUsed(),
                infoString, virtualSize+physicalSize);

		if (m_DiscardedCars.IsMemberInGroup(modelIndex))
		{
			grcDebugDraw::AddDebugOutput(Color32(200, 0, 0), debugText);
			safecpy(debugText, "Instances: ");
			char addr[16];

			CVehicle::Pool* pool = CVehicle::GetPool();
			for (s32 i = 0; i < pool->GetSize(); ++i)
			{
				CVehicle* veh = pool->GetSlot(i);
				if (veh && veh->GetModelIndex() == modelIndex)
				{
					snprintf(addr, DEBUG_TEXT_LEN, " 0x%8p ", veh);
					safecat(debugText, addr);
				}
			}

			grcDebugDraw::AddDebugOutput(Color32(200, 0, 0), debugText);
		}
		else
		{
			grcDebugDraw::AddDebugOutput(debugText);
		}
    }
#endif // DEBUG_DRAW
}

#if __BANK
void CPopulationStreaming::DumpVehicleMemoryInfo()
{
	CLoadedModelGroup allLoadedCars;
	allLoadedCars.Merge(&m_AppropriateLoadedCars, &m_InAppropriateLoadedCars, &m_SpecialLoadedCars, &m_LoadedBoats);

	u32 totalVirtual = 0;
	u32 totalPhysical = 0;

	atArray<strIndex> allDeps;
	char buf[128] = {0};

	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);

	Displayf("************** Loaded vehicle memory info **************\n");
	s32 numMembers = allLoadedCars.CountMembers();
	for (int index = 0; index < numMembers; index++)
	{
		u32 modelIndex = allLoadedCars.GetMember(index);
		fwModelId modelId((strLocalIndex(modelIndex)));

		// Find size of the vehicle model
		u32 virtualSize  = 0;
		u32 physicalSize = 0;
		CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, m_residentObjects.GetElements(), m_residentObjects.GetCount(), true);
		
		Displayf("%s main: %d - phys: %d - total: %d\n", CModelInfo::GetBaseModelInfoName(modelId), virtualSize, physicalSize, virtualSize + physicalSize);
		
		deps.ResetCount();
		CModelInfo::GetObjectAndDependencies(modelId, deps, m_residentObjects.GetElements(), m_residentObjects.GetCount());

		Displayf("*** %d dependencies:\n", deps.GetCount());
		for (s32 i = 0; i < deps.GetCount(); ++i)
		{
			u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
			u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
			const char* name = strStreamingEngine::GetInfo().GetObjectName(deps[i], buf, sizeof(buf));

			bool dupe = false;
			if (allDeps.Find(deps[i]) != -1)
				dupe = true;
			else
			{
				totalVirtual += virtualMemory;
				totalPhysical += physicalMemory;
			}

			Displayf("*** %d (%s) - main: %d - phys: %d - total: %d %s\n", deps[i].Get(), name, virtualMemory, physicalMemory, virtualMemory + physicalMemory, dupe ? "(SHARED)" : "");

			allDeps.PushAndGrow(deps[i]);
		}
	}

	Displayf("%d resident assets:\n", CVehicleModelInfo::GetResidentObjects().GetCount());
	for (int i = 0; i < CVehicleModelInfo::GetResidentObjects().GetCount(); i++)
	{
		strIndex index = CVehicleModelInfo::GetResidentObjects()[i];
		u32 virtualSize=0, physicalSize=0;
		strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize);
		const char* name = strStreamingEngine::GetInfo().GetObjectName(index, buf, sizeof(buf));

		Displayf("*** %d (%s) - main: %d - phys: %d - total: %d\n", index.Get(), name, virtualSize, physicalSize, virtualSize + physicalSize);
		totalVirtual += virtualSize;
		totalPhysical += physicalSize;
	}

	Displayf("Total main: %d\n", totalVirtual);
	Displayf("Total phys: %d\n", totalPhysical);
	Displayf("********************************************************\n");
}
#endif // __BANK

bool CPopulationStreaming::AddPedModelToCorrectList( u32 modelIndex )
{
	CPedModelInfo* pPMI = ((CPedModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))));
	Assert(pPMI);

	if (pPMI->GetIsCop())
	{
		m_SpecialLoadedPeds.AddMember(modelIndex);
		return false;
	}
	else if (IsPedModelNeededCurrently(modelIndex))
	{
		m_AppropriateLoadedPeds.AddMember(modelIndex);
	}
	else if (IsPedModelNeededInAnyZone(modelIndex))
	{
		m_InAppropriateLoadedPeds.AddMember(modelIndex);
	}
	else
	{
		m_SpecialLoadedPeds.AddMember(modelIndex);
		return false;
	}

	return true;
}

void CPopulationStreamingWrapper::Init(unsigned initMode)
{
    gPopStreaming.Init(initMode);
}

void CPopulationStreamingWrapper::Shutdown(unsigned shutdownMode)
{
    gPopStreaming.Shutdown(shutdownMode);
}

void CPopulationStreamingWrapper::Update()
{
    gPopStreaming.Update();
}


#if __BANK

#include "debug/OverviewScreen.h"

float	CPopulationStreaming::RenderPedMemoryUse(float x, float y)
{
	char theText[256];
	Color32 textColour = Color32(255, 255, 255, 255);

#if MERGED_BUDGET 
	sprintf(theText, "Memory used by peds: %d / %d (over %d)", m_TotalMemoryUsedByPedModelsMain, GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel), m_MemOverBudgetMainPeds + m_MemOverBudgetVramPeds);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Streamed memory used by peds: %d", m_TotalStreamedPedMemoryMain + m_TotalStreamedPedMemoryVram);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
#else
	sprintf(theText, "Main memory used by peds: %d / %d (over %d)", m_TotalMemoryUsedByPedModelsMain, GetMemForPeds(MEMTYPE_GAME_VIRTUAL, s_MemForPedsLevel), m_MemOverBudgetMainPeds);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Physical memory used by peds: %d / %d (over %d)", m_TotalMemoryUsedByPedModelsVram, GetMemForPeds(MEMTYPE_GAME_PHYSICAL, s_MemForPedsLevel), m_MemOverBudgetVramPeds);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Streamed memory used by peds: m%d - v%d", m_TotalStreamedPedMemoryMain, m_TotalStreamedPedMemoryVram);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
#endif // MERGED_BUDGET 
	return y;
}

float	CPopulationStreaming::RenderVehicleMemoryUse(float x, float y)
{
	char theText[256];
	Color32 textColour = Color32(255, 255, 255, 255);

#if MERGED_BUDGET 
	sprintf(theText, "Memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsMain, GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel), m_MemOverBudgetMainVehs + m_MemOverBudgetVramVehs);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Streamed memory used by vehicles: %d", m_TotalStreamedVehicleMemoryMain + m_TotalStreamedVehicleMemoryVram);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
#else
	sprintf(theText, "Main memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsMain, GetMemForVehicles(MEMTYPE_GAME_VIRTUAL, s_MemForVehiclesLevel), m_MemOverBudgetMainVehs);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Physical memory used by vehicles: %d / %d (over %d)", m_TotalMemoryUsedByVehicleModelsVram, GetMemForVehicles(MEMTYPE_GAME_PHYSICAL, s_MemForVehiclesLevel), m_MemOverBudgetVramVehs);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Streamed memory used by vehicles: m%d - v%d", m_TotalStreamedVehicleMemoryMain, m_TotalStreamedVehicleMemoryMain);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
	sprintf(theText, "Memory used by weapons: m%d - v%d", m_TotalWeaponMemoryMain, m_TotalWeaponMemoryVram);
	grcDebugDraw::Text(GET_TEXT_SCREEN_COORDS(x, y), textColour, theText, false);
	y+=DEBUG_CHAR_HEIGHT;
#endif // MERGED_BUDGET 

	return y;
}

#endif // __BANK


