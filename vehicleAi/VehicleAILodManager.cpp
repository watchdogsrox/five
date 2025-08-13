#include "VehicleAILodManager.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Camera/scripted/ScriptDirector.h"
#include "Control/replay/replay.h"
#include "debug/VectorMap.h"
#include "game/dispatch/Incidents.h"
#include "Peds/PedPopulation.h"
#include "Renderer/Occlusion.h"
#include "scene/lod/LodScale.h"
#include "Scene/World/GameWorld.h"
#include "Scene/EntityIterator.h"
#include "streaming/streaming.h"
#include "VehicleAI/VehicleAiLod.h"
#include "VehicleAI/VehicleIntelligence.h"
#include "VehicleAI/Task/TaskVehicleMissionBase.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehiclePopulation.h"
#include "Vehicles/virtualroad.h"
#include "ai/debug/system/AIDebugLogManager.h"

// Rage headers
#if __BANK
#include "Camera/CamInterface.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/Gameplay/GameplayDirector.h"
#endif
#include "GrCore/DebugDraw.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

PARAM(notimeslicevehicles, "Disable timesliced vehicle updates.");
PARAM(parkedsuperdummy, "Disallow parked vehicles to become super-dummy");

bank_bool CVehicleAILodManager::ms_bVehicleAILoddingActive = true;

bank_float CVehicleAILodManager::ms_fRealDistance							= 15.0f;
bank_float CVehicleAILodManager::ms_fDummyLodDistance						= 40.0f;
bank_float CVehicleAILodManager::ms_fSuperDummyLodDistance					= 75.0f;
bank_float CVehicleAILodManager::ms_fSuperDummyLodDistanceParked			= 40.0f;
bank_float CVehicleAILodManager::ms_fLodFarFromPopCenterDistance			= 135.0f;
bank_float CVehicleAILodManager::ms_fOffscreenLodDistScale					= 3.0f;
bank_bool CVehicleAILodManager::ms_bConsiderOccludedAsOffscreen				= true;
bank_float CVehicleAILodManager::ms_fHysteresisDistance						= 5.0f;
float CVehicleAILodManager::ms_fLodRangeScale								= 1.0f;
int CVehicleAILodManager::ms_iTimeBetweenConversionAttempts					= 2000;
int CVehicleAILodManager::ms_iTimeBetweenConversionToRealAttempts			= 500;
bank_bool CVehicleAILodManager::ms_bNLodActive								= true;
bank_s32 CVehicleAILodManager::ms_iNLodRealMax								= 10;
bank_s32 CVehicleAILodManager::ms_iNLodDummyMax								= 20;
bank_s32 CVehicleAILodManager::ms_iNLodNonTimeslicedMax						= 14;
bank_s32 CVehicleAILodManager::ms_iNLodRealDriversAndPassengersMax			= 24; // WAS 12 (PS3/XBox360)

bank_bool CVehicleAILodManager::ms_bUseDummyLod									= true;
bank_bool CVehicleAILodManager::ms_bUseSuperDummyLod							= true;
bool CVehicleAILodManager::ms_bUseSuperDummyLodForParked						= true;
bank_bool CVehicleAILodManager::ms_bAllSuperDummys								= false;
bank_bool CVehicleAILodManager::ms_bDeTimesliceTransmissionAndSleep				= true;
bank_bool CVehicleAILodManager::ms_bDeTimesliceWheelCollisions					= true; 
bank_bool CVehicleAILodManager::ms_bConvertExteriorVehsToDummyWhenInInterior	= true;
bank_bool CVehicleAILodManager::ms_bCollideWithOthersInSuperDummyMode			= true;
bank_bool CVehicleAILodManager::ms_bCollideWithOthersInSuperDummyInactiveMode	= true;
bank_bool CVehicleAILodManager::ms_bFreezeParkedSuperDummyWhenCollisionsNotLoaded = true;

bank_bool CVehicleAILodManager::ms_bUseRoadCamber								= true;
bank_bool CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal				= true;
bank_bool CVehicleAILodManager::ms_bAllowAltRouteInfoForRearWheels				= true;

bank_bool CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode				= true;
bool CVehicleAILodManager::ms_bDisableConstraintsForSuperDummyInactive			= true;

u32 CVehicleAILodManager::ms_preferredNumRealVehiclesAroundPlayer			= 5;

bool CVehicleAILodManager::ms_usePretendOccupantsSystem						= true;
float CVehicleAILodManager::ms_makePedsIntoPretendOccupantsRangeOnScreen	= 110.0f; // WAS 55.0f (PS3/XBox360)
float CVehicleAILodManager::ms_makePedsFromPretendOccupantsRangeOnScreen	= 90.0f; // WAS 45.0f (PS3/XBox360)
float CVehicleAILodManager::ms_makePedsIntoPretendOccupantsRangeOffScreen	= 20.0f;
float CVehicleAILodManager::ms_makePedsFromPretendOccupantsRangeOffScreen	=  5.0f;

bank_float CVehicleAILodManager::ms_fSuperDummySteerSpeed					= PI*0.75f;
bank_float CVehicleAILodManager::ms_fSuperDummyCloneHeadingToVelBlendRatio	= 0.2f;

bool CVehicleAILodManager::ms_bUseTimeslicing								= false;
u16 CVehicleAILodManager::ms_TimesliceMaxUpdatesPerFrame					= CONFIGURED_FROM_FILE;
bank_u16 CVehicleAILodManager::ms_TimesliceMinUpdatePeriodInFrames			= 4;
bank_float CVehicleAILodManager::ms_TimesliceAllowOnScreenDistSmall			= 75.0f; // lowered to match superdummy LOD distance
bank_float CVehicleAILodManager::ms_TimesliceAllowOnScreenDistLarge			= 150.0f;
u16 CVehicleAILodManager::ms_TimesliceCountdownUntilWrap					= 0;
u16 CVehicleAILodManager::ms_TimesliceNumInLowLodCounting					= 0;
u16 CVehicleAILodManager::ms_TimesliceNumInLowLodPrevious					= 0;
s16 CVehicleAILodManager::ms_TimesliceLastIndex								= 0;
s16 CVehicleAILodManager::ms_TimesliceFirstVehIndex							= -1;
s16 CVehicleAILodManager::ms_TimesliceLastVehIndex							= -1;

u32 CVehicleAILodManager::ms_dummyPathReconsiderLengthMs					= 500;
float CVehicleAILodManager::ms_dummyMaxDistFromRoadsEdgeMult				= 1.5f;
float CVehicleAILodManager::ms_dummyMaxDistFromRoadFromAboveMult			= 0.0f;
float CVehicleAILodManager::ms_dummyMaxDistFromRoadFromBelowMult			= 3.0f;
bool CVehicleAILodManager::ms_convertUseFixupCollisionWithWheelSurface		= false;
bool CVehicleAILodManager::ms_convertEnforceDummyMustBeNearPaths			= true;

bank_bool CVehicleAILodManager::ms_bAllowSkipUpdatesInTimeslicedLod			= true;
bank_bool CVehicleAILodManager::ms_bSkipTimeslicedUpdatesWhileParked		= true;
bank_bool CVehicleAILodManager::ms_bSkipTimeslicedUpdatesWhileStopped		= true;
bank_u32 CVehicleAILodManager::ms_iMaxTimeslicedUpdatesToSkipWhileStopped	= 1;
#if __BANK
bool CVehicleAILodManager::ms_bDisplaySkippedTimeslicedUpdates = false;
#endif // __BANK

bank_bool CVehicleAILodManager::ms_bDisableSuperDummyForSlowBikes			= true;
bank_float CVehicleAILodManager::ms_fDisableSuperDummyForSlowBikesSpeed		= 2.0f;

bank_s32 CVehicleAILodManager::ms_iDummyConstraintModePrivate				= CVehicleAILodManager::DCM_PhysicalDistanceAndRotation;
bank_s32 CVehicleAILodManager::ms_iSuperDummyConstraintModePrivate			= CVehicleAILodManager::DCM_PhysicalDistance;
bank_float CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance			= 1.0f;
bank_float CVehicleAILodManager::ms_fConstrainToRoadsExtraDistancePreJunction= 27.0f;
bank_float CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction= 27.0f;
bank_float CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint = 0.4f;

#if __BANK
bool CVehicleAILodManager::ms_bForceDummyCars								= false;
float CVehicleAILodManager::ms_fForceLodDistance							= -1.0f;
bool CVehicleAILodManager::ms_bForceAllTimesliced							= false;
bool CVehicleAILodManager::ms_displayDummyVehicleMarkers					= false;
bool CVehicleAILodManager::ms_bDisplayDummyWheelHits						= false;
bool CVehicleAILodManager::ms_bDisplayDummyPath								= false;
bool CVehicleAILodManager::ms_bDisplayDummyPathDetailed						= false;
bool CVehicleAILodManager::ms_bDisplayDummyPathConstraint					= false;
bool CVehicleAILodManager::ms_bDisplayVirtualJunctions						= false;
bool CVehicleAILodManager::ms_bRaiseDummyRoad								= false;
bool CVehicleAILodManager::ms_bRaiseDummyJunctions							= false;
float CVehicleAILodManager::ms_fRaiseDummyRoadHeight						= 1.0f;
bool CVehicleAILodManager::ms_bUseDummyWheelTransition						= true;
bool CVehicleAILodManager::ms_bDebug										= false;
bool CVehicleAILodManager::ms_bVMLODDebug									= false;
bool CVehicleAILodManager::ms_bDebugExtraInfo								= false;
bool CVehicleAILodManager::ms_bDebugPedsInfo								= false;
bool CVehicleAILodManager::ms_bDebugExtraWheelInfo							= false;
bool CVehicleAILodManager::ms_bDebugScoring									= false;
ENABLE_FRAG_OPTIMIZATION_ONLY(bool CVehicleAILodManager::ms_bDebugFragCacheInfo	= false;)
float CVehicleAILodManager::ms_fDebugDrawRange								= 300.0f;
bool CVehicleAILodManager::ms_displayDummyVehicleNonConvertReason			= false;
bool CVehicleAILodManager::ms_bDisplayFullUpdates							= false;
bool CVehicleAILodManager::ms_bValidateDummyConstraints						= false;
float CVehicleAILodManager::ms_fValidateDummyConstraintsBuffer				= 5.0f;
#endif

#if __DEV
bool CVehicleAILodManager::ms_focusVehBreakOnCheckAttemptPopConversion		= false;
bool CVehicleAILodManager::ms_focusVehBreakOnAttemptedPopConversion			= false;
bool CVehicleAILodManager::ms_focusVehBreakOnPopulationConversion			= false;
#endif // __DEV

atArray<CVehicleAILodManager::CVehicleAiPriority> CVehicleAILodManager::ms_PrioritizedVehicleArray; // array of vehicle priorities, sorted by distance
Vec3V CVehicleAILodManager::ms_vCachedVehiclePriorityCenter; // cached population center, to determine if we need to recalculate vehicle priority based on distance moved

#define VEHICLE_PRIORITY_RECALCULATE_DISTANCE_SQUARED 100.0f // Distance popcenter has to move to trigger an immediate recalculate
u8 CVehicleAILodManager::ms_iVehiclePriorityCountdown						= 0; // Current countdown timer
u8 CVehicleAILodManager::ms_iVehiclePriorityCountdownMax					= 3; // Max counter timer = number of frames to spread vehicle lod update across
u32 CVehicleAILodManager::ms_iVehicleCount									= 0;
float CVehicleAILodManager::ms_ExpectedWorstTimeslicedTimestep = 1.0f;
atQueue<float, CVehicleAILodManager::kFrameTimeMemorySize> CVehicleAILodManager::ms_FrameTimeMemory;

int	CVehicleAILodManager::ms_iNLodRealRemaining								= 0;
int	CVehicleAILodManager::ms_iNLodDummyRemaining							= 0;
s32	CVehicleAILodManager::ms_iNLodNonTimeslicedRemaining					= 0;
int	CVehicleAILodManager::ms_iNLodRealDriversAndPassengersRemaining			= 0;
s32 CVehicleAILodManager::ms_iMostDistantRealPedsPrioritizedIndex			= 0;
s32 CVehicleAILodManager::ms_iClosestPretendPedsPrioritizedIndex			= 0;
s32 CVehicleAILodManager::ms_iNumVehiclesConvertedToRealPedsThisFrame		= 0;

CVehicleAILodManager::EDummyConstraintMode CVehicleAILodManager::GetDummyConstraintMode()
{
	return static_cast<CVehicleAILodManager::EDummyConstraintMode>(ms_iDummyConstraintModePrivate);
}

CVehicleAILodManager::EDummyConstraintMode CVehicleAILodManager::GetSuperDummyConstraintMode()
{
	return static_cast<CVehicleAILodManager::EDummyConstraintMode>(ms_iSuperDummyConstraintModePrivate);
}

// Hack ahoy!
// Eventually all LOD state changing will happen via the CVehicleAILodManager::Update() function,
// at which point SetLodFlag() & GetLodFlag() function can be removed.
// But for now we still need a way for the vehicles themselves to manipulate their LOD flags from
// within CVehicle::TryToMakeIntoDummy()

void CVehicleAILodManager::SetLodFlag(CVehicle * pVehicle, const u32 iFlag)
{
	pVehicle->GetVehicleAiLod().SetLodFlag(iFlag);
}
void CVehicleAILodManager::ClearLodFlag(CVehicle * pVehicle, const u32 iFlag)
{
	pVehicle->GetVehicleAiLod().ClearLodFlag(iFlag);
}


bool CVehicleAILodManager::ShouldDoFullUpdate(CVehicle& veh)
{
	// Don't update vehicles in the reuse pool
	if(veh.GetIsInReusePool())
	{
		return false;
	}

	// First, if we are not in low LOD timeslicing mode, always return true.
	if(!veh.GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
	{
		return true;
	}

	// Check if we are forced to update.
	if(veh.m_nVehicleFlags.bLodForceUpdateThisTimeslice || veh.m_nVehicleFlags.bLodForceUpdateUntilNextAiUpdate)
	{
		return true;
	}

	if(veh.ShouldSkipUpdatesInTimeslicedLod())
	{
		return false;
	}

	return veh.GetVehicleAiLod().m_TimeslicedUpdateThisFrame;
}

int CVehicleAILodManager::ComputeRealDriversAndPassengersRemaining()
{
	return Max(0, ComputeRealDriversAndPassengersMax() - CPedPopulation::ms_nNumInVehAmbient - CPedPopulation::GetNumScheduledPedsInVehicles());
}

int CVehicleAILodManager::ComputeRealDriversAndPassengersMax()
{
	if(CWantedIncident::IsIncidentNearby(CWantedIncident::GetMaxDistanceForNearbyIncidents()))
	{
		static dev_s32 iNonLawInVehiclesPadCount = 5;
		return Max(ms_iNLodRealDriversAndPassengersMax, (CPedPopulation::ms_nNumInVehCop + CPedPopulation::ms_nNumInVehSwat + iNonLawInVehiclesPadCount));
	}

	static dev_s32 iNonExemptPadCount = 5;
	return Max(ms_iNLodRealDriversAndPassengersMax, ComputeRealDriversAndPassengersExceptions() + iNonExemptPadCount);
}

int CVehicleAILodManager::ComputeRealDriversAndPassengersExceptions()
{
	return (CPedPopulation::ms_nNumInVehAmbientDead + CPedPopulation::ms_nNumInVehAmbientNoPretendModel);
}

void CVehicleAILodManager::DisableTimeslicingImmediately(CVehicle& veh)
{
	CVehicleAILod& lod = veh.GetVehicleAiLod();

	// If already in timesliced mode, immediately kick out of it.
	if(lod.IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
	{
		lod.ClearLodFlag(CVehicleAILod::AL_LodTimeslicing);
		veh.SwitchAwayFromTimeslicedLod();
	}

	// Make sure we don't go back to timeslicing for at least a frame or so.
	lod.SetBlockedLodFlag(CVehicleAILod::AL_LodTimeslicing);

	// This shouldn't really be necessary, except possibly if some code explicitly
	// unblocks timeslicing again.
	veh.m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;
}

void CVehicleAILodManager::ResetOnReuseVehicle(CVehicle& veh)
{
	CVehicleAILod& lod = veh.GetVehicleAiLod();

	// TODO: Perhaps we should do more to reset the LOD state here? For now, it seems
	// safer to just turn off the timeslicing LOD, so we don't bypass any transition
	// functions that may need to execute.

	if(lod.IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
	{
		veh.GetVehicleAiLod().ClearLodFlag(CVehicleAILod::AL_LodTimeslicing);
		veh.SwitchAwayFromTimeslicedLod();
	}
}

void CVehicleAILodManager::UpdateLodRangeScale()
{
	//-----------------------------------------------------------------------------------------------
	// Scale all real/dummy conversion ranges according to depth of the pop control into interiors
	switch(CPedPopulation::GetPopulationControlPointInfo().m_locationInfo)
	{
	default:
		Assert(false);
	case LOCATION_EXTERIOR:
		ms_fLodRangeScale = 1.0f;
		break;
	case LOCATION_SHALLOW_INTERIOR:
		ms_fLodRangeScale = CVehiclePopulation::GetRangeScaleShallowInterior();
		break;
	case LOCATION_DEEP_INTERIOR:
		ms_fLodRangeScale = CVehiclePopulation::GetRangeScaleDeepInterior();
		break;
	}

	ms_fLodRangeScale = ms_fLodRangeScale * CVehiclePopulation::GetAllZoneScaler() * g_LodScale.GetGlobalScale();
}

eVehicleDummyMode CVehicleAILodManager::CalcDesiredLodForPoint(Vec3V_In vPos)
{
	eVehicleDummyMode desiredMode = VDM_REAL;

	const Vec3V vPopCenter = VECTOR3_TO_VEC3V(CPedPopulation::GetPopulationControlPointInfo().m_conversionCentre);
	float fDistSq = DistSquared(vPos,vPopCenter).Getf();

	const float fDummyLodDist = ms_fDummyLodDistance * ms_fLodRangeScale;
	const float fSuperDummyLodDist = ms_fSuperDummyLodDistance * ms_fLodRangeScale;

    if(fDistSq > square(fSuperDummyLodDist))
	{
		desiredMode = VDM_SUPERDUMMY;
	}
	else if(fDistSq > square(fDummyLodDist))
	{
		desiredMode = VDM_DUMMY;
	}

#if __BANK
	desiredMode = DevOverrideLod(desiredMode);
#endif

	return desiredMode;
}

#if __BANK
eVehicleDummyMode CVehicleAILodManager::DevOverrideLod(eVehicleDummyMode lodIn)
{
	eVehicleDummyMode lodOut = lodIn;
	// Allow us to turn off the super-dummy system
	if(!ms_bUseSuperDummyLod && lodIn == VDM_SUPERDUMMY)
	{
		lodOut = VDM_DUMMY;
	}
	// Force all real cars to dummy
	if(ms_bForceDummyCars && lodIn == VDM_REAL)
	{
		lodOut = VDM_DUMMY;
	}
	// Allow us to turn off the dummy system
	if(!ms_bUseDummyLod)
	{
		lodOut = VDM_REAL;
	}
	if(ms_bAllSuperDummys)
	{
		lodOut = VDM_SUPERDUMMY;
	}
	return lodOut;
}
#endif

void CVehicleAILodManager::InitValuesFromConfig()
{
	ms_TimesliceMaxUpdatesPerFrame = static_cast<u16>(GET_POPULATION_VALUE(VehicleTimesliceMaxUpdatesPerFrame));
}

void CVehicleAILodManager::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		ms_bUseTimeslicing = !PARAM_notimeslicevehicles.Get();

		if(PARAM_parkedsuperdummy.Get())
		{
			ms_bUseSuperDummyLodForParked = true;
		}

		InitValuesFromConfig();
	}
}

// Initialize the prioritized vehicle array to the size of the vehicle pool
void CVehicleAILodManager::InitializeVehiclePriorityArray()
{
	if(ms_PrioritizedVehicleArray.GetCount() == 0)
		ms_PrioritizedVehicleArray.ResizeGrow((u32) CVehicle::GetPool()->GetSize());
}

void CVehicleAILodManager::Update()
{
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
		return;
#endif
	PF_AUTO_PUSH_TIMEBAR("VehicleAILodMgr Update");

#if __BANK
	CVehicleAiLodTester::ValidateState();
	CVehicleAiLodTester::ApplyChanges();
#endif

	if (!CStreaming::IsPlayerPositioned())
	{
		return;
	}

	// Use local player position as default population control center
	Vector3 popCtrlCentre = CGameWorld::FindLocalPlayerCoors();

	// Check if script camera is in use
	if( camInterface::GetScriptDirector().IsRendering() )
	{
		// use camera position as population control center
		popCtrlCentre = camInterface::GetScriptDirector().GetFrame().GetPosition();
	}

#if __BANK
	if (ms_bDisplayVirtualJunctions)
	{
		CVirtualRoad::DebugDrawJunctions(RCC_VEC3V(popCtrlCentre));
	}
#endif //__BANK

	if(fwTimer::IsGamePaused())
		return;

	ms_iNumVehiclesConvertedToRealPedsThisFrame = 0;

	UpdateExpectedWorstTimeslicedTimestep();

	// Update LOD ranges
	UpdateLodRangeScale();
	const float fDummyLodDistance = ms_fDummyLodDistance*ms_fLodRangeScale;
	const float fSuperDummyLodDistance = ms_fSuperDummyLodDistance*ms_fLodRangeScale;
	const float fSuperDummyLodDistanceParked = ms_fSuperDummyLodDistanceParked*ms_fLodRangeScale;

	// Check that the pop center hasn't moved far enough to invalidate our last check
	// or if countdown has elapsed
	if(!ms_iVehiclePriorityCountdown || (DistSquared(ms_vCachedVehiclePriorityCenter, VECTOR3_TO_VEC3V(CPedPopulation::GetPopulationControlPointInfo().m_conversionCentre)).Getf() > VEHICLE_PRIORITY_RECALCULATE_DISTANCE_SQUARED))
	{		
		ms_iVehicleCount = FindVehiclesByPriority();
		ms_vCachedVehiclePriorityCenter = VECTOR3_TO_VEC3V(CPedPopulation::GetPopulationControlPointInfo().m_conversionCentre);
		ms_iVehiclePriorityCountdown = ms_iVehiclePriorityCountdownMax;

		ms_iNLodRealRemaining = ms_iNLodRealMax;
		ms_iNLodDummyRemaining = ms_iNLodDummyMax;
		ms_iNLodNonTimeslicedRemaining = ms_iNLodNonTimeslicedMax;
		ms_iNLodRealDriversAndPassengersRemaining = ComputeRealDriversAndPassengersRemaining();

		// We are starting over from the beginning of the pool, reset the count of the number of vehicles in timeslicing LOD.
		ms_TimesliceNumInLowLodCounting = 0;

		// Keep track of most distant prioritized vehicle array index with real ped passengers.
		// We will convert this vehicle to pretend occupants if we exceed the limit.
		ms_iMostDistantRealPedsPrioritizedIndex = -1;

		// Keep track of the closest prioritized vehicle array index with pretend occupants.
		// This will serve as a marker when traversing the list from back to front.
		ms_iClosestPretendPedsPrioritizedIndex = -1;
	}
	
	// batch the vehicles, closest to furthest
	int batchCount = static_cast<int>(ceil(ms_iVehicleCount/(ms_iVehiclePriorityCountdownMax*1.f)));
	int batchstart = batchCount * (ms_iVehiclePriorityCountdownMax- ms_iVehiclePriorityCountdown);
	int batchend = Min<u32>(batchstart + batchCount, ms_iVehicleCount); // don't try to read outside of the valid entries of the array
	
	// countdown to recalculate
	ms_iVehiclePriorityCountdown--;

	// Count the number of timesliced vehicles in this batch.
	int timesliceNumInLowLodCounting = ms_TimesliceNumInLowLodCounting;

	// Handle Pretend peds by batch as well
	const int kMaxVehiclesConvertedToRealPedsPerFrame = 1;
	int nNumVehiclesConvertedToRealPeds = 0;

	//----------------------------------------------------------------------------------------------------
	// Loop over vehicles by priority
	for(int i = batchstart; i<batchend; i++)
	{
		CVehicle * pVehicle = ms_PrioritizedVehicleArray[i].m_pVehicle;
		// check that the vehicle is still valid
		if(!CVehicle::GetPool()->IsValidPtr(pVehicle))
			continue;
		if(pVehicle->GetIsInReusePool())
			continue; // skip vehicles that are on hold for re-use
		const float fDistanceSq = ms_PrioritizedVehicleArray[i].m_fEffectiveDistSq;

		if(!CVehicleAILodManager::ms_bVehicleAILoddingActive)
		{
			pVehicle->GetVehicleAiLod().SetLodFlags(CVehicleAILod::AL_DefaultLodFlags);
		}
		else if(pVehicle->InheritsFromTrailer()
				|| CVehicle::IsEntityAttachedToTrailer(pVehicle) 
				|| (pVehicle->GetDummyAttachmentParent() && pVehicle->GetDummyAttachmentParent()->InheritsFromTrailer())
				|| pVehicle->GetIsBeingTowed())
		{
			// Attached trailers never process their own dummy state - they will convert with the cab
			// ... as do all vehicles attached to trailers.
			// ... as do all vehicles being towed by towtruck or cargobob
			if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
			{
				pVehicle->GetVehicleAiLod().ClearLodFlag(CVehicleAILod::AL_LodTimeslicing);

				pVehicle->SwitchAwayFromTimeslicedLod();
			}
		}
		else
		{
			//-----------------------------------------------------
			// Calc desired LOD

			const eVehicleDummyMode currentMode = pVehicle->GetVehicleAiLod().GetDummyMode();
			eVehicleDummyMode desiredMode = currentMode;

			if( currentMode == VDM_REAL )
			{
				float fSuperDummyDist = fSuperDummyLodDistance;

				// Use the parked super-dummy threshold if this vehicle is parked.
				if(ms_bUseSuperDummyLodForParked && pVehicle->ConsiderParkedForLodPurposes())
				{
					fSuperDummyDist = fSuperDummyLodDistanceParked;
				}

				const float fDummyDist = fDummyLodDistance+ms_fHysteresisDistance;

				if( (fDistanceSq > square(fSuperDummyDist)) && pVehicle->GetCanMakeIntoDummy(VDM_SUPERDUMMY) )
				{
					desiredMode = VDM_SUPERDUMMY;
				}
				else if( (fDistanceSq > square(fDummyDist)) && pVehicle->GetCanMakeIntoDummy(VDM_DUMMY) )
				{
					desiredMode = VDM_DUMMY;
				}
			}

			else if( currentMode == VDM_DUMMY )
			{
				const float fRealDist = fDummyLodDistance-ms_fHysteresisDistance;
				const float fSuperDummyDist = fSuperDummyLodDistance+ms_fHysteresisDistance;

				// can the vehicle still be a dummy?
				if (pVehicle->IsDummy() && !pVehicle->GetCanMakeIntoDummy(VDM_DUMMY))
				{
					//BANK_ONLY(aiDisplayf("Vehicle (%s) at (%0.2f,%0.2f) was dummy and failed GetCanMakeIntoDummy() because '%s'.  Task is '%s'.",pVehicle->GetModelName(),pVehicle->GetVehiclePosition().GetXf(),pVehicle->GetVehiclePosition().GetYf(),pVehicle->GetNonConversionReason(),(pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetActiveTask()) ? pVehicle->GetIntelligence()->GetActiveTask()->GetTaskName():"no task");)
					desiredMode = VDM_REAL;
				}

				if( fDistanceSq < square(fRealDist) )
				{
					desiredMode = VDM_REAL;
				}
				else if( (fDistanceSq > square(fSuperDummyDist)) && pVehicle->GetCanMakeIntoDummy(VDM_SUPERDUMMY) )
				{
					desiredMode = VDM_SUPERDUMMY;
				}
			}

			else if( currentMode == VDM_SUPERDUMMY )
			{
				const float fDummyDist = fSuperDummyLodDistance-ms_fHysteresisDistance;
				float fRealDist = fDummyLodDistance;

				// If this is a parked super-dummy vehicle, don't let the distance at which we turn it to
				// real be any less than the distance at which we would have turned it to super-dummy
				// (with some hysteresis).
				if(ms_bUseSuperDummyLodForParked && pVehicle->ConsiderParkedForLodPurposes())
				{
					fRealDist = Min(fRealDist, fSuperDummyLodDistanceParked - ms_fHysteresisDistance);
				}

				// can the vehicle still be a super dummy?
				if (pVehicle->IsDummy() && !pVehicle->GetCanMakeIntoDummy(VDM_SUPERDUMMY))
				{
					//BANK_ONLY(aiDisplayf("Vehicle (%s) at (%0.2f,%0.2f) was super-dummy and failed GetCanMakeIntoDummy() because '%s'.  Task is '%s'.",pVehicle->GetModelName(),pVehicle->GetVehiclePosition().GetXf(),pVehicle->GetVehiclePosition().GetYf(),pVehicle->GetNonConversionReason(),(pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetActiveTask()) ? pVehicle->GetIntelligence()->GetActiveTask()->GetTaskName():"no task");)
					if (pVehicle->GetCanMakeIntoDummy(VDM_DUMMY))
					{
						desiredMode = VDM_DUMMY;
					}
					else
					{
						desiredMode = VDM_REAL;
					}
				}

				if( fDistanceSq < square(fRealDist) )
				{
					desiredMode = VDM_REAL;
				}
				else if( (fDistanceSq < square(fDummyDist)) && pVehicle->GetCanMakeIntoDummy(VDM_DUMMY) )
				{
					desiredMode = VDM_DUMMY;
				}
			}

			// Clip LOD as required to satisfy N-lodding
			if(ms_bNLodActive)
			{
				if(desiredMode == VDM_REAL && ms_iNLodRealRemaining <= 0 && desiredMode != currentMode)
				{
					desiredMode = VDM_DUMMY;
				}
				if(desiredMode == VDM_DUMMY && ms_iNLodDummyRemaining <= 0 && desiredMode != currentMode)
				{
					desiredMode = VDM_SUPERDUMMY;
				}
			}

			// Allow vehicle to override desired mode
            desiredMode = pVehicle->ProcessOverriddenDummyMode(desiredMode);

			//-------------------------------------
			// Process LOD changes

			eVehicleDummyMode resultantMode = currentMode;
			if(desiredMode != currentMode)
			{
				switch(desiredMode)
				{
					case VDM_REAL:
					{
						const bool iTimerPassedOrWrapped = (fwTimer::GetTimeInMilliseconds() >= (pVehicle->m_realConvertAttemptTimeMs + CVehicleAILodManager::ms_iTimeBetweenConversionToRealAttempts)) 
							|| (fwTimer::GetTimeInMilliseconds() < pVehicle->m_realConvertAttemptTimeMs)
							|| pVehicle->IsParkedSuperDummy(); // bypass the timer check for parked supper dummy
						if (iTimerPassedOrWrapped)
						{
							const bool bSkipClearanceTest = pVehicle->ConsiderParkedForLodPurposes() 
								|| pVehicle->HasShrunkDummyConstraintsAsMuchAsPossible()
								|| pVehicle->InheritsFromBike()
								|| pVehicle->GetIsAircraft()
								|| pVehicle->InheritsFromBoat()
								|| !pVehicle->HasDummyConstraint()
								|| pVehicle->IsParkedSuperDummy();
							if(pVehicle->TryToMakeFromDummy(bSkipClearanceTest))
							{
								resultantMode = desiredMode;
							}
#if __BANK
							else
							{
								CAILogManager::GetLog().Log("DUMMY CONVERSION: %s Failed to convert to real: %s\n", 
									AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), pVehicle->GetNonConversionReason());
							}
#endif
						}
						break;
					}

					case VDM_DUMMY:
					case VDM_SUPERDUMMY:
					{
						const bool iTimerPassedOrWrapped = (fwTimer::GetTimeInMilliseconds() >= (pVehicle->m_dummyConvertAttemptTimeMs + CVehicleAILodManager::ms_iTimeBetweenConversionAttempts)) 
							|| (fwTimer::GetTimeInMilliseconds() < pVehicle->m_dummyConvertAttemptTimeMs);
						if(iTimerPassedOrWrapped)
						{
							if(pVehicle->TryToMakeIntoDummy(desiredMode))
							{
								resultantMode = desiredMode;
							}
#if __BANK
							else
							{
								CAILogManager::GetLog().Log("DUMMY CONVERSION: %s Failed to convert to dummy: %s\n", 
									AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), pVehicle->GetNonConversionReason());
							}
#endif
						}
						break;
					}
					default:
					{
						Assertf(false,"Desired LOD not handled.");
					}
				}
			}

			// Check if this vehicle should be in timesliced mode, which requires that it's
			// a superdummy, tasks have allowed it, and visibility checks have passed.
			bool useTimeslicingLod = false;
			if(ms_bUseTimeslicing)
			{
				if(pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing)
				{
					useTimeslicingLod = true;
				}
				else if((resultantMode == VDM_SUPERDUMMY && pVehicle->GetIsStatic())
						|| pVehicle->InheritsFromHeli()		// Helicopters are allowed to do timeslicing when in real mode, since they lack superdummy support.
						)
				{
					if(pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingAfterVisibilityChecks)
					{
						useTimeslicingLod = true;
					}
					else if(ms_iNLodNonTimeslicedRemaining == 0 && pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingIfTooManyUpdates)
					{
						// In this case, we wouldn't normally be far enough away on screen to use timeslicing,
						// but too many other vehicles are already not timesliced.
						useTimeslicingLod = true;
					}
				}

#if __BANK
				useTimeslicingLod |= ms_bForceAllTimesliced;
#endif
			}

			pVehicle->m_nVehicleFlags.bLodForceUpdateThisTimeslice = false;

			// Update the AL_LodTimeslicing flag and otherwise notify the vehicle in case the LOD level changed.
			bool wasInTimeslicingLod = pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing);
			if(useTimeslicingLod)
			{
				timesliceNumInLowLodCounting++;

				if(!wasInTimeslicingLod)
				{
					pVehicle->GetVehicleAiLod().SetLodFlag(CVehicleAILod::AL_LodTimeslicing);

					pVehicle->SwitchToTimeslicedLod();
				}
			}
			else
			{
				// Count down how many we have seen that are not timesliced.
				ms_iNLodNonTimeslicedRemaining = Max(ms_iNLodNonTimeslicedRemaining - 1, 0);

				if(wasInTimeslicingLod)
				{
					pVehicle->GetVehicleAiLod().ClearLodFlag(CVehicleAILod::AL_LodTimeslicing);

					pVehicle->SwitchAwayFromTimeslicedLod();
				}
			}

			//-------------------------------------
			// Update N-LOD counts

			switch(resultantMode)
			{
				case VDM_REAL:
					ms_iNLodRealRemaining--;
					break;
				case VDM_DUMMY:
					ms_iNLodDummyRemaining--;
					break;
				default:
					break;
			}
#if __ASSERT
			if(pVehicle->IsSuperDummy() && pVehicle->InheritsFromPlane() && pVehicle->GetStatus() == STATUS_WRECKED && pVehicle->IsInAir())
			{
				eVehicleDummyMode eOverriddenMode = pVehicle->ProcessOverriddenDummyMode(desiredMode);
				bool unused;
				eVehicleDummyMode eOverriddenMode2 = pVehicle->GetOverriddenDummyMode(desiredMode, unused);
				bool bAlwayBeReal = pVehicle->ShouldAlwaysTryToBeReal();

				Assertf(false, 
					"Wrecked plane switches to supper dummy in the middle of air, might stuck there for ever. vehicle 0x%p, fixedUntilCollisionFlag %x, bShouldFixIfNoCollision %x, AllowFreezeIfNoCollision %x, ShouldFixIfNoCollisionLoadedAroundPosition %x, Collision loaded %x, current mode %d, desired mode %d, overridden mode %d, overridden mode2 %d real LOD remaining %d, netClone %d, becomeSuperDummy %d, alwaysBeReal %d, nonConversion %s", 
					pVehicle, pVehicle->GetIsFixedUntilCollisionFlagSet(), 
					pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision, pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, 
					pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition(), pVehicle->IsCollisionLoadedAroundPosition(),
					currentMode, desiredMode, eOverriddenMode, eOverriddenMode2,
					ms_iNLodRealRemaining, pVehicle->IsNetworkClone(), pVehicle->m_nVehicleFlags.bMayBecomeParkedSuperDummy,
					bAlwayBeReal, pVehicle->GetNonConversionReason());
			}

			if(pVehicle->IsParkedSuperDummy() && !pVehicle->GetIsStatic() && !pVehicle->IsNetworkClone())
			{
				eVehicleDummyMode eOverriddenMode = pVehicle->ProcessOverriddenDummyMode(desiredMode);
				bool unused;
				eVehicleDummyMode eOverriddenMode2 = pVehicle->GetOverriddenDummyMode(desiredMode, unused);
				bool bAlwayBeReal = pVehicle->ShouldAlwaysTryToBeReal();

				Assertf(pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition() && !pVehicle->IsCollisionLoadedAroundPosition(), 
					"Parked supper dummy should be switching to real once being activated with ground collision loaded. vehicle 0x%p, fixedUntilCollisionFlag %x, bShouldFixIfNoCollision %x, AllowFreezeIfNoCollision %x, ShouldFixIfNoCollisionLoadedAroundPosition %x, Collision loaded %x, current mode %d, desired mode %d, overridden mode %d, overridden mode2 %d real LOD remaining %d, netClone %d, becomeSuperDummy %d, alwaysBeReal %d, nonConversion %s", 
					pVehicle, pVehicle->GetIsFixedUntilCollisionFlagSet(), 
					pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision, pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, 
					pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition(), pVehicle->IsCollisionLoadedAroundPosition(),
					currentMode, desiredMode, eOverriddenMode, eOverriddenMode2,
					ms_iNLodRealRemaining, pVehicle->IsNetworkClone(), pVehicle->m_nVehicleFlags.bMayBecomeParkedSuperDummy,
					bAlwayBeReal, pVehicle->GetNonConversionReason());
			}
#endif
		}

		// By this time we have established our actual LOD, so clear this "want to be" flag.
		pVehicle->m_nVehicleFlags.bCreatingAsInactive = false;

		//--------------------------------------------------------------------------------

		// Track occurrences of interest
		bool bVehicleNotConvertedToRealDueToMAX = false;
		int prevNumVehiclesConvertedToRealPeds = nNumVehiclesConvertedToRealPeds;

		// Convert this vehicle if appropriate from pretend occupants to real peds or vice versa.
		ManageVehiclePretendOccupants( pVehicle, popCtrlCentre, kMaxVehiclesConvertedToRealPedsPerFrame, ms_iNLodRealDriversAndPassengersRemaining, 
                                       nNumVehiclesConvertedToRealPeds, bVehicleNotConvertedToRealDueToMAX );

		// Did this vehicle get converted to real peds, or is it already using real peds?
		if( nNumVehiclesConvertedToRealPeds > prevNumVehiclesConvertedToRealPeds || (!pVehicle->IsUsingPretendOccupants() && pVehicle->CanSetUpWithPretendOccupants()) )
		{
			// Track the most distant vehicle with real peds that could be converted
			// NOTE: we have no distance comparisons here because list is sorted smallest to largest already
			ms_iMostDistantRealPedsPrioritizedIndex = i;
		}

		// Have we not set closest index yet and was this vehicle denied real passengers due to the limit on real peds?
		if( ms_iClosestPretendPedsPrioritizedIndex == -1 && bVehicleNotConvertedToRealDueToMAX )
		{
			// Track the closest vehicle that should have real ped passengers
			// NOTE: no distance comparisons because list is sorted smallest to largest
			ms_iClosestPretendPedsPrioritizedIndex = i;
		}

		// Maintain an accurate count
		ms_iNLodRealDriversAndPassengersRemaining = ComputeRealDriversAndPassengersRemaining();
	}
	
	// If the sweep over the prioritized vehicle list has just finished
	// and we have reached or exceeded the ideal real ambient ped passengers limit
	if( (u32)batchend == ms_iVehicleCount && CPedPopulation::ms_nNumInVehAmbient >= ComputeRealDriversAndPassengersMax() && ms_iMostDistantRealPedsPrioritizedIndex > 0 )
	{
		// Traverse down from farthest real peds vehicle to closest trying to convert to pretend occupants
		bool bConvertedRealToPretend = false;
		for(int listIndex = ms_iMostDistantRealPedsPrioritizedIndex; listIndex > ms_iClosestPretendPedsPrioritizedIndex && ms_iClosestPretendPedsPrioritizedIndex >= 0 && !bConvertedRealToPretend; listIndex--)
		{
			if( AssertVerify(listIndex >= 0) && AssertVerify(listIndex <= ms_PrioritizedVehicleArray.GetCount()) )
			{
				// Try to convert the most distant tracked vehicle from real peds to pretend occupants
				CVehicle* pVehicle = ms_PrioritizedVehicleArray[listIndex].m_pVehicle;
				// check that the vehicle is still valid
				if(!CVehicle::GetPool()->IsValidPtr(pVehicle))
					continue;
				if(pVehicle->GetIsInReusePool())
					continue; // skip vehicles that are on hold for re-use
				if( pVehicle && !pVehicle->IsUsingPretendOccupants() )
				{
					bConvertedRealToPretend = pVehicle->TryToMakePedsIntoPretendOccupants();
				}
			}
		}
	}

	// Update the count of timesliced vehicles we have found so far.
	Assert(timesliceNumInLowLodCounting <= 0xffff);
	ms_TimesliceNumInLowLodCounting = (u16)timesliceNumInLowLodCounting;

	// If this is the last batch, update ms_TimesliceNumInLowLodPrevious as we have
	// now gone through all the vehicles.
	if(!ms_iVehiclePriorityCountdown)
	{
		ms_TimesliceNumInLowLodPrevious = (u16)timesliceNumInLowLodCounting;
	}

	// If we have any vehicles in timesliced LOD, determine the next update window.
	// Note that we don't always have an exact count, just a number from the last iteration
	// through all the vehicles.
	if(ms_TimesliceNumInLowLodPrevious)
	{
		UpdateTimeslicing(ms_TimesliceNumInLowLodPrevious);
	}
}

int CVehicleAILodManager::FindVehiclesByPriority()
{
	const Vec3V vPopCenter(VECTOR3_TO_VEC3V(CPedPopulation::GetPopulationControlPointInfo().m_conversionCentre));
	const CViewport * pGameViewport = gVpMan.GetGameViewport();
	const grcViewport * pGameGrcViewport = pGameViewport ? &pGameViewport->GetGrcViewport() : NULL;
	const ScalarV fOffscreenLodDistScaleSq(square(ms_fOffscreenLodDistScale));
	const ScalarV fLodFarFromPopCenterDistSq(square(CVehicleAILodManager::ms_fLodFarFromPopCenterDistance));
	const int iVehPoolSize = (int) CVehicle::GetPool()->GetSize();
	const bool bNetworkGame = NetworkInterface::IsGameInProgress();

	static const ScalarV scOne(V_ONE);
	static const ScalarV fShortVehicleHalfLength( V_TWO );
	static const ScalarV fLongVehicleHalfLength( V_FOUR );

	// Set up a squared distance threshold for timesliced vehicles on screen.
	const ScalarV timesliceAllowOnScreenDistSmallV(ms_TimesliceAllowOnScreenDistSmall);
	const ScalarV timesliceAllowOnScreenDistLargeV(ms_TimesliceAllowOnScreenDistLarge);
	const ScalarV timeSliceLengthRangeInverse = scOne / (fLongVehicleHalfLength - fShortVehicleHalfLength);

	int iNumVehicles = 0;
	for(int i=0; i<iVehPoolSize; i++)
	{
		if(CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i))
		{
			// Calc an effective distance for use in LOD selection
			const Vec3V vVehPos(pVehicle->GetVehiclePosition());
			const ScalarV fVehRadius(pVehicle->GetBoundRadius());
			const Vec4V vVehSphereAndRadius(vVehPos,fVehRadius);
			ScalarV fDistSq = DistSquared(vPopCenter,vVehPos);

			const ScalarV fLengthScale = (fVehRadius - fShortVehicleHalfLength) * timeSliceLengthRangeInverse;
			const ScalarV fDesiredOnScreenDist = (timesliceAllowOnScreenDistLargeV * fLengthScale) 
												- (timesliceAllowOnScreenDistSmallV * fLengthScale) + timesliceAllowOnScreenDistSmallV;
			const ScalarV timesliceAllowOnScreenDistV = Clamp(fDesiredOnScreenDist, timesliceAllowOnScreenDistSmallV, timesliceAllowOnScreenDistLargeV);
			const ScalarV timesliceAllowOnScreenDistSqV = Scale(timesliceAllowOnScreenDistV, timesliceAllowOnScreenDistV);

			// If we have too many vehicles getting updates, we use this threshold instead. This is the hard limit, half of
			// the regular limit (V_QUARTER == 0.5*0.5).
			const ScalarV timesliceAllowOnScreenTooManyUpdatesDistSqV = Scale(timesliceAllowOnScreenDistSqV, ScalarV(V_QUARTER));

			bool bVehicleIsOnscreenToLocal = pVehicle->GetIsVisibleInSomeViewportThisFrame();			
			if(bVehicleIsOnscreenToLocal && pGameGrcViewport && !pGameGrcViewport->IsSphereVisible(vVehSphereAndRadius))
			{
				bVehicleIsOnscreenToLocal = false;
			}

			bool bVehicleOffscreenCloseAndVisibleRemotely = false;
			bool bWithinRealRange = IsLessThanAll(fDistSq, ScalarV(ms_fDummyLodDistance*ms_fDummyLodDistance)) != 0;
			//in network games we also want to take into account remote players
			if(bNetworkGame && !pVehicle->IsNetworkClone() && !bVehicleIsOnscreenToLocal && bWithinRealRange)
			{			
				//this flag only set if the vehicle is offscreen locally within dummy range and visible remotely within dummy range
				float fRadius = pVehicle->GetBoundRadius();
				bVehicleOffscreenCloseAndVisibleRemotely = NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), fRadius, ms_fDummyLodDistance);
			}

			bool bVehicleOnscreenToAnyPlayer = bVehicleOffscreenCloseAndVisibleRemotely || bVehicleIsOnscreenToLocal;

			//this flag is local only
			pVehicle->m_nVehicleFlags.bVehicleIsOnscreen = bVehicleIsOnscreenToLocal;
			
			// If the vehicle has a desire to use timeslicing, check if it fulfills the visibility conditions.
			// If bLodShouldUseTimeslicing is set, we don't want to respect visibility anyway, so no need
			// to check distance, etc.
			pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingAfterVisibilityChecks = pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingIfTooManyUpdates = pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing;
			if(ms_bUseTimeslicing && pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing && !pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing)
			{
				// Start off by making use of the earlier viewport/occlusion check.
				bool bVehicleIsVisibleForTimeslicing = bVehicleOnscreenToAnyPlayer;
				bool bVehicleIsVisibleForTimeslicingTooManyUpdates = bVehicleOnscreenToAnyPlayer;
				if(bVehicleIsVisibleForTimeslicing)
				{
					// We are potentially visible, but check if we are far enough away to use timeslicing anyway.
					if(IsGreaterThanAll(fDistSq, timesliceAllowOnScreenDistSqV))
					{
						// Yeah, we are not too visible for timeslicing.
						bVehicleIsVisibleForTimeslicingTooManyUpdates = bVehicleIsVisibleForTimeslicing = false;
					}
					else if(IsGreaterThanAll(fDistSq, timesliceAllowOnScreenTooManyUpdatesDistSqV))
					{
						// In a pinch, this is far enough away to allow timeslicing when on screen.
						bVehicleIsVisibleForTimeslicingTooManyUpdates = false;
					}
				}
				// Clear bLodCanUseTimeslicing again if we were visible. This would generally get
				// set back to true during the next task update.
				pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingAfterVisibilityChecks = !bVehicleIsVisibleForTimeslicing;
				pVehicle->m_nVehicleFlags.bLodCanUseTimeslicingIfTooManyUpdates = !bVehicleIsVisibleForTimeslicingTooManyUpdates;
			}

			//don't muck with distance if vehicle is within 15 meters of us or within real distance to remote players
			if(!bVehicleOnscreenToAnyPlayer && IsGreaterThanAll(fDistSq, ScalarV(ms_fRealDistance*ms_fRealDistance)))
			{
				fDistSq *= fOffscreenLodDistScaleSq;
			}

#if __BANK
			if(ms_fForceLodDistance >= 0.0f)
			{
				fDistSq = ScalarV(square(ms_fForceLodDistance));
			}
			ms_PrioritizedVehicleArray[iNumVehicles].bVisibleLocally = bVehicleIsOnscreenToLocal;
			ms_PrioritizedVehicleArray[iNumVehicles].bWithinLocalRealDist = bWithinRealRange;
			ms_PrioritizedVehicleArray[iNumVehicles].bVisibleRemotely = bVehicleOffscreenCloseAndVisibleRemotely;
#endif

			pVehicle->m_nVehicleFlags.bLodFarFromPopCenter = IsGreaterThanAll(fDistSq,fLodFarFromPopCenterDistSq) ? true : false;

			StoreScalar32FromScalarV(ms_PrioritizedVehicleArray[iNumVehicles].m_fEffectiveDistSq,fDistSq);
			ms_PrioritizedVehicleArray[iNumVehicles].m_pVehicle = pVehicle;
			iNumVehicles++;
		}
	}

	std::sort(ms_PrioritizedVehicleArray.GetElements(), ms_PrioritizedVehicleArray.GetElements() + iNumVehicles, CVehicleAiPriority::LessThan);

	return iNumVehicles;
}

void CVehicleAILodManager::UpdateExpectedWorstTimeslicedTimestep()
{
	// MAGIC! In practice, these values seem to give us an overestimate in
	// at least ~98% of the cases.
	static bank_u32 s_NumExtraVehicles = 10;
	static bank_float s_ExtraTimeFactor = 1.5f;

	// Push in the current time step into the memory.
	float currentTimestep = fwTimer::GetTimeStep();
	if(ms_FrameTimeMemory.IsFull())
	{
		ms_FrameTimeMemory.Drop();
	}
	ms_FrameTimeMemory.Push(currentTimestep);

	// Find the worst frame time in recent memory.
	const int numInMemory = ms_FrameTimeMemory.GetCount();
	float longestFrameTime = 0.0f;
	for(int i = 0; i < numInMemory; i++)
	{
		longestFrameTime = Max(longestFrameTime, ms_FrameTimeMemory[i]);
	}

	// Get the number of timesliced vehicles, plus some to account for that we may be getting more.
	const int numTimesliced = ms_TimesliceNumInLowLodPrevious + s_NumExtraVehicles;

	// Compute the time between updates, by dividing the number of timesliced vehicles by the number
	// we update per frame (rounded up).
	int framesBetweenUpdates = (numTimesliced + ms_TimesliceMaxUpdatesPerFrame - 1)/ms_TimesliceMaxUpdatesPerFrame;

	// Take into account that we have a minimum limit to the update time.
	framesBetweenUpdates = Max(framesBetweenUpdates, (int)ms_TimesliceMinUpdatePeriodInFrames);

	// Compute the time between updates from the frame time and the update period, and apply
	// a fudge factor since the frame rate may be varying, or the update order could possibly change
	// a bit, etc.
	const float timeBetweenUpdates = longestFrameTime*(float)framesBetweenUpdates*s_ExtraTimeFactor;
	ms_ExpectedWorstTimeslicedTimestep = timeBetweenUpdates;
}

void CVehicleAILodManager::ManageVehiclePretendOccupants(CVehicle* pVehicle, const Vector3& popCtrlCentre, const int kMaxVehiclesConvertedToRealPedsPerFrame, const int kMaxRealPeds, int& inout_NumVehiclesConvertedToRealPeds, bool& out_bVehicleNotConvertedToRealDueToMAX)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif // GTA_REPLAY

	// Compute distance to population control center
	float fDistToPopCenterSq = (popCtrlCentre - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())).Mag2();
	
	// Initialize effective distance using population control center by default
	float fEffectiveDistSq = fDistToPopCenterSq;

	// If a multiplayer game is running
    bool bRemotePlayerIsCloser = false;

	if(NetworkInterface::IsGameInProgress())
	{
		// Get the vehicle multiplayer object
		CNetObjEntity *netObjEntity = static_cast<CNetObjEntity *>(pVehicle->GetNetworkObject());
		if(netObjEntity)
		{
			// Compute the distance squared to nearest multiplayer player
			float netPlayerDistSq = netObjEntity->GetDistanceToNearestPlayerSquared();
			if( netPlayerDistSq < fDistToPopCenterSq )
			{
				// use closest multiplayer player distance if closer
				fEffectiveDistSq      = netPlayerDistSq;
                bRemotePlayerIsCloser = true;
			}
		}
	}

	// Handle on-screen / off-screen with different ranges
	// The goal is to not visibly notice passengers appearing/disappearing, and not have missing drivers
	// --
	// Initialize with off screen ranges unless replay is recording
	bool bIsOnScreen = pVehicle->m_nVehicleFlags.bVehicleIsOnscreen;
#if GTA_REPLAY
	bIsOnScreen |= CReplayMgr::IsRecording();
#endif

	float fEffectivePedToPretendRange = ms_makePedsIntoPretendOccupantsRangeOffScreen;
	float fEffectivePretendToPedRange = ms_makePedsFromPretendOccupantsRangeOffScreen;
	if(bIsOnScreen || bRemotePlayerIsCloser)
	{
		// Get pretend to real range multiplier from vehicle model info
		const float fVehiclePretendOccupantsScale = pVehicle->GetVehicleModelInfo()->GetPretendOccupantsScale();

		// Check if the vehicle should get an additional visibility scale factor
		float driverVisibilityScale = 1.0f;
		// first check if the vehicle driver can be seen far away
		const bool bDriverCanBeSeenFarAway = !pVehicle->CarHasRoof() || (pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT) || (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN);
		if( bDriverCanBeSeenFarAway )
		{
			driverVisibilityScale = 2.0f;
		}
		else
		{
			// check if the vehicle is fleeing
			const bool bVehicleFleeing = pVehicle->GetIntelligence()->GetActiveTask() && pVehicle->GetIntelligence()->GetActiveTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_FLEE;
			if( bVehicleFleeing )
			{
				driverVisibilityScale = 2.0f;
			}
		}

		// Compute scaled visible ranges as the effective ranges
		fEffectivePedToPretendRange = ms_makePedsIntoPretendOccupantsRangeOnScreen * ms_fLodRangeScale * driverVisibilityScale * fVehiclePretendOccupantsScale;
		fEffectivePretendToPedRange = ms_makePedsFromPretendOccupantsRangeOnScreen * ms_fLodRangeScale * driverVisibilityScale * fVehiclePretendOccupantsScale;
	}

	// Check if we should try to convert real to pretend occupants
	if( !pVehicle->IsUsingPretendOccupants() )
	{
		// if vehicle is far enough away
		if( fEffectiveDistSq > (fEffectivePedToPretendRange * fEffectivePedToPretendRange) )
		{
			bool bProceed = true;

			if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleOrCloseToAnyPlayer(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), pVehicle->GetBoundRadius(), 100.f, 10.f))
			{
				bProceed = false;
			}
			
			if (bProceed)
			{
				// NOTE: special cases will be handled inside TryToMakePedsIntoPretendOccupants so we don't cull real peds that should persist.
				pVehicle->TryToMakePedsIntoPretendOccupants();
			}
		}
	}
	// Check if we should try to convert pretend to real
	else // pVehicle->IsUsingPretendOccupants()
	{
		bool bProceed = false;

		// if vehicle is close enough
		if( fEffectiveDistSq < (fEffectivePretendToPedRange * fEffectivePretendToPedRange) )
		{
			bProceed = true;
		}
		else
		{
			//Special consideration for MP - when other players come near a remotely owned vehicle with fake peds
			if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleOrCloseToAnyPlayer(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), pVehicle->GetBoundRadius(), 100.f, 10.f))
			{
				bProceed = true;
			}
		}

		if (bProceed)
		{
			// if number of vehicles converted this frame has not reached the limit
			if(inout_NumVehiclesConvertedToRealPeds < kMaxVehiclesConvertedToRealPedsPerFrame)
			{
				// if number of real peds has not reached the limit
				if( kMaxRealPeds > 0 )
				{
					const bool bOnlyAddDriver = false;
					if(pVehicle->TryToMakePedsFromPretendOccupants(bOnlyAddDriver, &kMaxRealPeds))
					{
						inout_NumVehiclesConvertedToRealPeds++;
						ms_iNumVehiclesConvertedToRealPedsThisFrame++;
					}
				}
				else // kMaxRealPeds <= 0
				{
					out_bVehicleNotConvertedToRealDueToMAX = true;
				}
			}
		}
	}
}


void CVehicleAILodManager::UpdateTimeslicing(int numInLowTimesliceLOD)
{
	// Unset the m_TimeslicedUpdateThisFrame flag on the vehicles we updated on the previous frame.
	SetTimeslicedUpdateFlagForVehiclesInRange(ms_TimesliceFirstVehIndex, ms_TimesliceLastVehIndex, false);

	// First, we get the index we stored off on the previous frame,
	// and keep a copy of it.
	int vehIndex = ms_TimesliceLastIndex;

	// Get the pool and its size, and set up some other variables we will use.
	CVehicle::Pool *pPool = CVehicle::GetPool();
	const int numSlots = (int) pPool->GetSize();
	int numToUpdate = 0;
	int numConsidered = 0;

	const int numToUpdateBasedOnCount = (numInLowTimesliceLOD + ms_TimesliceMinUpdatePeriodInFrames - 1)/ms_TimesliceMinUpdatePeriodInFrames;
	const int maxToUpdate = Clamp(numToUpdateBasedOnCount, (int)1, (int)ms_TimesliceMaxUpdatesPerFrame);

	int firstIndex = -1;
	int lastIndex = -1;

	bool wrapped = false;

	// Loop over vehicles until we've found the desired amount, or wrapped around.
	while(1)
	{
		// Advance to the next slot, wrapping around if we reached the end of the pool.
		vehIndex++;
		if(vehIndex >= numSlots)
		{
			wrapped = true;
			vehIndex = 0;
		}

		// Get the vehicle if it exists, and check if it should be timesliced. If not, we just move on to the next.
		CVehicle* pVeh = pPool->GetSlot(vehIndex);
		if(pVeh && pVeh->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
		{
			// Store the pointer to the first we found on this frame.
			if(firstIndex < 0)
			{
				firstIndex = vehIndex;
			}

			// Store the pointer to the last we have found so far.
			lastIndex = vehIndex;

			pVeh->UpdateFlagsForTimeslicedLod();

			if(!pVeh->ShouldSkipUpdatesInTimeslicedLod())
			{
				// Count them, and stop if we have found enough.
				numToUpdate++;
				if(numToUpdate >= maxToUpdate)
				{
					break;
				}
			}
		}

		// Keep track of how many slots we have checked, and break if all slots have been
		// checked at least once, so we are back where we started.
		if(++numConsidered >= numSlots)
		{
			break;
		}
	}

	if(ms_TimesliceCountdownUntilWrap > 0)
	{
		ms_TimesliceCountdownUntilWrap--;
	}
	if(wrapped)
	{
		if(ms_TimesliceCountdownUntilWrap > 0)
		{
			ms_TimesliceFirstVehIndex = ms_TimesliceLastVehIndex = -1;
			return;
		}
		ms_TimesliceCountdownUntilWrap = ms_TimesliceMinUpdatePeriodInFrames;
	}

	// Store the index we are at.
	Assert(vehIndex >= 0 && vehIndex < 0x8000);
	ms_TimesliceLastIndex = (s16)vehIndex;

	// Store the indices of the range of peds to update. We don't use these directly from ShouldDoFullUpdate(),
	// but they are useful for clearing the  m_TimeslicedUpdateThisFrame flag on the next frame.
	Assert(firstIndex >= -0x8000 && firstIndex < 0x8000);
	Assert(lastIndex >= -0x8000 && lastIndex < 0x8000);
	ms_TimesliceFirstVehIndex = (s16)firstIndex;
	ms_TimesliceLastVehIndex = (s16)lastIndex;

	// Set the m_TimeslicedUpdateThisFrame to true within the range we want to update.
	SetTimeslicedUpdateFlagForVehiclesInRange(firstIndex, lastIndex, true);
}


void CVehicleAILodManager::SetTimeslicedUpdateFlagForVehiclesInRange(int poolStartIndex, int poolEndIndex, bool value)
{
	CVehicle::Pool *pPool = CVehicle::GetPool();
	const int numSlots = (int) pPool->GetSize();

	if(poolStartIndex >= 0)
	{
		Assert(poolEndIndex >= 0);

		int index = poolStartIndex;
		while(1)
		{
			Assert(index >= 0 && index < numSlots);
			CVehicle* pVehicle = pPool->GetSlot(index);
			if(pVehicle)
			{
				// If setting the value to true, make sure it's not already true - the way
				// we do this now, that would be an indication of something not working properly.
				Assert(!value || !pVehicle->GetVehicleAiLod().m_TimeslicedUpdateThisFrame);

				if(value)
				{
					if(pVehicle->ShouldSkipUpdatesInTimeslicedLod())
					{
						// We're definitely getting a timesliced update to skip this frame -- increment skip counter
						pVehicle->IncrementNumTimeslicedUpdatesSkipped();
					}
					else
					{
						pVehicle->ResetNumTimeslicedUpdatesSkipped();
					}
				}

				pVehicle->GetVehicleAiLod().m_TimeslicedUpdateThisFrame = value;
			}

			// Check if we reached the end. Note that this is inclusive.
			if(index == poolEndIndex)
			{
				break;
			}

			// Move to the next slot, wrapping around if necessary.
			if(++index == numSlots)
			{
				index = 0;
			}
		}
	}
	else
	{
		// We expect both of the indices to be valid, or both to be invalid if there is no range.
		Assert(poolEndIndex < 0);
	}
}

#if __BANK
void CVehicleAILodManager::Debug()
{
	if(!ms_bVMLODDebug && !ms_bDebug && !ms_bDebugExtraInfo && !ms_bDebugPedsInfo && !ms_bDisplayFullUpdates && !ms_bDebugScoring)
		return;

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	char tmp[256];
	CVehicle::Pool * pVehiclePool = CVehicle::GetPool();
	s32 iPoolSize = (s32) pVehiclePool->GetSize();
	while(iPoolSize--)
	{
		CVehicle * pVehicle = pVehiclePool->GetSlot(iPoolSize);
		if(!pVehicle || pVehicle->GetIsInReusePool())
			continue;

		//not yet added to world
		if(!pVehicle->GetIsRetainedByInteriorProxy() && pVehicle->GetOwnerEntityContainer() == NULL)
			continue;

		if(ms_bVMLODDebug && !pVehicle->GetIsAircraft() && pVehicle->InheritsFromAutomobile())
		{
			Color32 col = pVehicle->IsDummy() ? (pVehicle->IsSuperDummy() ? Color_red : Color_yellow) : Color_green;
			static float fVehicleScale = 1.0f;
			CVectorMap::DrawVehicleLodInfo(pVehicle, col, fVehicleScale);
		}

		// If ms_bDisplayFullUpdates is set, draw a sphere around vehicles that update on this frame. Note that
		// we do this before checking the range, since we are generally interested in seeing this happen in the distance too.
		if(ms_bDisplayFullUpdates)
		{
			if(ShouldDoFullUpdate(*pVehicle))
			{
				grcDebugDraw::Sphere(pVehicle->GetTransform().GetPosition(), 2.5f, Color_white, false);
			}
			if(!ms_bDebug && !ms_bDebugExtraInfo && !ms_bDebugPedsInfo)
			{
				// No other debug drawing was enabled.
				continue;
			}
		}

		const Vector3 vPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		const float fRange = (vOrigin - vPos).Mag();
		if(fRange > ms_fDebugDrawRange)
			continue;

		const float fFullAlphaFraction = 0.5f;
		const float fAlpha = 1.0f - Max(0.0f,fRange-ms_fDebugDrawRange*fFullAlphaFraction) / ((1.0f-fFullAlphaFraction) * ms_fDebugDrawRange);
		Color32 col;

		if(pVehicle)
		{
			if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
			{
				Assert(!pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodDummy));
				if(ms_bDebugExtraInfo)
				{
					sprintf(tmp, "SuperDummy (%p)",pVehicle);
				}
				else
				{
					sprintf(tmp, "SuperDummy");
				}
				if(pVehicle->GetIsStatic())
				{
					if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
					{
						col = Color_violet;
					}
					else
					{
						col = Color_purple;
					}
				}
				else
				{
					col = Color_red;
				}
			}
			else if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodDummy))
			{
				if(ms_bDebugExtraInfo)
				{
					sprintf(tmp, "Dummy (%p)",pVehicle);
				}
				else
				{
					sprintf(tmp, "Dummy");
				}
				col = Color_orange;
			}
			else // Real
			{
				if(ms_bDebugExtraInfo)
				{
					sprintf(tmp, "Real (%p)",pVehicle);
				}
				else
				{
					sprintf(tmp, "Real");
				}
				if(!pVehicle->GetCollider())
				{
					col = Color_white;
				}
				else if(pVehicle->GetCollider()->IsArticulated())
				{
					col = Color_cyan;
				}
				else
				{
					col = Color_green;
				}
			}

			col.SetAlpha((int)(fAlpha * 255.0f));
			int yOffset = -grcDebugDraw::GetScreenSpaceTextHeight()*4;

			if( ms_bDebug || ms_bDebugExtraInfo )
			{
				grcDebugDraw::Text(vPos, col, 0, yOffset, tmp, false);
				yOffset += grcDebugDraw::GetScreenSpaceTextHeight();

				if(ms_bDebugExtraInfo)
				{
					sprintf(tmp, "Collider : %p",pVehicle->GetCollider());
				}
				else
				{
					sprintf(tmp, "Collider : %s",pVehicle->GetCollider() ? "true" : "false");
				}
				grcDebugDraw::Text(vPos, col, 0, yOffset, tmp, false);
				yOffset += grcDebugDraw::GetScreenSpaceTextHeight();

				if(ms_bDebugExtraInfo && !pVehicle->HasCollisionLoadedAroundVehicle())
				{
					sprintf(tmp, "No collision");
					grcDebugDraw::Text(vPos, Color_red, 0, yOffset, tmp, false);
					yOffset += grcDebugDraw::GetScreenSpaceTextHeight();
				}
			}

			if(ms_bDebugExtraInfo)
			{
				// Extra information about attachments (trying to display in a concise form)
				const fwEntity * pParentPhys = pVehicle->GetAttachParent();
				const fwEntity * pParentDummy = pVehicle->GetDummyAttachmentParent();
				const fwEntity * pParent = pParentPhys ? pParentPhys : pParentDummy;
				const char * sParentType = pParentPhys ? "phys" : (pParentDummy ? "dummy" : "none");
				if(pParentPhys && pParentDummy)
				{
					Assertf(pParentPhys==pParentDummy,"Vehicle with physical and dummy parents that are different.");
					col = Color_yellow;
					sParentType = "both";
				}
				bool bChildrenVehiclePhys = false;
				const fwEntity * pChildPhys = pVehicle->GetChildAttachment();
				while(pChildPhys && !bChildrenVehiclePhys)
				{
					if(static_cast<const CPhysical*>(pChildPhys)->GetIsTypeVehicle())
					{
						bChildrenVehiclePhys = true;
					}
					pChildPhys = pChildPhys->GetSiblingAttachment();
				}
				bool bChildrenDummy = pVehicle->HasDummyAttachmentChildren();
				const char * sChildrenType = bChildrenVehiclePhys ? "phys" : (bChildrenDummy ? "dummy" : "none");
				if(bChildrenVehiclePhys && bChildrenDummy)
				{
					col = Color_yellow;
					sChildrenType = "both";
				}

				if(pParent)
				{
					sprintf(tmp, "Parent: %s(%p), Children: %s",sParentType,pParent,sChildrenType);
				}
				else
				{
					sprintf(tmp, "Parent: %s, Children: %s",sParentType,sChildrenType);
				}
						  
				if(NetworkInterface::IsGameInProgress())
				{
					if(pVehicle->IsNetworkClone())
					{
						col = Color_white;
					}
				}

				grcDebugDraw::Text(vPos, col, 0, yOffset, tmp, false);
				yOffset += grcDebugDraw::GetScreenSpaceTextHeight();

				static bool bDisplaySpeeds = false;
				if(bDisplaySpeeds)
				{
					float fSpeedCollider = pVehicle->CPhysical::GetVelocity().Mag();
					float fSpeedSuperDummy = pVehicle->GetSuperDummyVelocity().Mag();
					sprintf(tmp,"Collider speed = %0.2f, SD speed = %0.2f",fSpeedCollider,fSpeedSuperDummy);
					grcDebugDraw::Text(vPos, col, 0, yOffset, tmp, false);
					yOffset = grcDebugDraw::GetScreenSpaceTextHeight();
				}
			}

			if(ms_bDebugPedsInfo)
			{
				int numCPedOccupants = pVehicle->GetNumberOfPassenger();
				if( pVehicle->GetDriver() ) 
				{ 
					numCPedOccupants += 1;

					static dev_float fVertOffsetForHead = 0.60f;
					static dev_float fHeadIndicatorRadius = 0.55f;
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVehicle->GetDriver()->GetTransform().GetPosition()) + Vector3(0.0f,0.0f,fVertOffsetForHead), fHeadIndicatorRadius, Color_white, false);
				}
				if( numCPedOccupants > 0 )
				{
					formatf(tmp, "Real Peds : %d", numCPedOccupants);
					grcDebugDraw::Text(vPos, Color_white, 0, yOffset, tmp, false);
					yOffset += grcDebugDraw::GetScreenSpaceTextHeight();

					if( !pVehicle->GetVehicleModelInfo()->GetAllowPretendOccupants() )
					{
						formatf(tmp, "!AllowPretendOccupants");
						grcDebugDraw::Text(vPos, Color_white, 0, yOffset, tmp, false);
						yOffset += grcDebugDraw::GetScreenSpaceTextHeight();
					}
				}
				else // no occupants
				{
					if( pVehicle->m_nVehicleFlags.bUsingPretendOccupants )
					{
						formatf(tmp, "PretendOccupants");
						grcDebugDraw::Text(vPos, Color_blue, 0, yOffset, tmp, false);
						yOffset += grcDebugDraw::GetScreenSpaceTextHeight();

						if( pVehicle->m_nVehicleFlags.bFailedToResetPretendOccupants )
						{
							switch(pVehicle->m_RealPedFailReason)
							{
								case CVehicle::RPFR_SUCCEEDED:
									formatf(tmp, "SUCCEEDED");// this shouldn't happen in practice
									break;
								case CVehicle::RPFR_IS_NET_CLONE:
									formatf(tmp, "IS_NET_CLONE");
									break;
								case CVehicle::RPFR_IS_NOT_NET_OBJECT:
									formatf(tmp, "IS_NOT_NET_OBJECT");
									break;
								case CVehicle::RPFR_NET_OBJ_REGISTER_FAILED:
									formatf(tmp, "NET_OBJ_REGISTER_FAILED");
									break;
								case CVehicle::RPFR_PED_POOL_FULL:
									formatf(tmp, "PED_POOL_FULL");
									break;
								case CVehicle::RPFR_NO_FALLBACK_PED:
									formatf(tmp, "NO_FALLBACK_PED");
									break;
								case CVehicle::RPFR_ADD_POLICE_FAILED:
									formatf(tmp, "ADD_POLICE_FAILED");
									break;
								case CVehicle::RPFR_NET_REQUEST_DRIVER_MODEL:
									formatf(tmp, "NET_REQUEST_DRIVER_MODEL");
									break;
								case CVehicle::RPFR_NET_SET_UP_DRIVER:
									formatf(tmp, "NET_SET_UP_DRIVER");
									break;
								case CVehicle::RPFR_FILTERED_SETUP_DRIVER:
									formatf(tmp, "FILTERED_SETUP_DRIVER");
									break;
								case CVehicle::RPFR_UNFILTERED_SETUP_DRIVER:
									formatf(tmp, "UNFILTERED_SETUP_DRIVER");
									break;
								case CVehicle::RPFR_ADD_FOR_TRAIN:
									formatf(tmp, "ADD_FOR_TRAIN");
									break;
								case CVehicle::RPFR_ADD_FOR_VEH:
									formatf(tmp, "ADD_FOR_VEH");
									break;
								default:
									formatf(tmp, "Unknown Real Ped Failure");
							}
							grcDebugDraw::Text(vPos, Color_red, 0, yOffset, tmp, false);
							yOffset += grcDebugDraw::GetScreenSpaceTextHeight();
						}
					}
					else // !pVehicle->m_nVehicleFlags.bUsingPretendOccupants
					{
						formatf(tmp, "NoPretendOccupants");
						grcDebugDraw::Text(vPos, Color_black, 0, yOffset, tmp, false);
						yOffset += grcDebugDraw::GetScreenSpaceTextHeight();
					}
					
					if( pVehicle->m_nVehicleFlags.bDisablePretendOccupants )
					{
						Color32 speedColor = Color_black;
						if(pVehicle->GetVelocity().Mag() > 0.2f)
						{
							Color_red;
						}
						formatf(tmp, "DisablePretendOccupants");
						grcDebugDraw::Text(vPos, speedColor, 0, yOffset, tmp, false);
						yOffset += grcDebugDraw::GetScreenSpaceTextHeight();
					}
				}
			}

			if(ms_bDebugExtraWheelInfo)
			{
				// Show wheel compression and hit info for the first 6 wheels (sprintf below assumes 6)
				const int kMaxWheelsToDisplay = 6;
				float fCompression[kMaxWheelsToDisplay];
				bool bHit[kMaxWheelsToDisplay];
				CWheel const * const * ppWheels = pVehicle->GetWheels();
				for(int i=0; i<kMaxWheelsToDisplay; i++)
				{
					fCompression[i] = -1.0f;
					bHit[i] = false;
					if(i<pVehicle->GetNumWheels())
					{
						fCompression[i] = ppWheels[i]->GetCompression();
						bHit[i] = ppWheels[i]->GetDynamicFlags().IsFlagSet(WF_HIT);
					}
				}
				sprintf(tmp,"Compressions = %0.3f(%d), %0.3f(%d), %0.3f(%d), %0.3f(%d), %0.3f(%d), %0.3f(%d)",
					fCompression[0],bHit[0],fCompression[1],bHit[1],fCompression[2],bHit[2],fCompression[3],bHit[3],fCompression[4],bHit[4],fCompression[5],bHit[5]);
				grcDebugDraw::Text(vPos, col, 0, yOffset, tmp, false);
				yOffset = grcDebugDraw::GetScreenSpaceTextHeight();
			}
		}
	}

	if(ms_bVMLODDebug)
	{	
		CVectorMap::DrawCircle(CGameWorld::FindLocalPlayerCoors(), 2.0f, Color_purple, true);

		const float fFOV = camInterface::GetGameplayDirector().GetFrame().GetFov() * CVectorMap::m_fLastTanFOV;
		const float fFOVStart = DtoR*(-fFOV-90.0f);
		const float fFOVEnd = DtoR*(fFOV-90.0f);

		const Vector3 vConversionCentre = CPedPopulation::GetPopulationControlPointInfo().m_conversionCentre;	
		CVectorMap::DrawArc(vConversionCentre, ms_fDummyLodDistance, fFOVStart, fFOVEnd, Color_yellow, false);
		CVectorMap::DrawArc(vConversionCentre, ms_fSuperDummyLodDistance, fFOVStart, fFOVEnd, Color_red, false);
		CVectorMap::DrawArc(vConversionCentre, rage::Max(ms_fRealDistance, ms_fDummyLodDistance / ms_fOffscreenLodDistScale), fFOVEnd, fFOVStart, Color_yellow, false);
		CVectorMap::DrawArc(vConversionCentre, ms_fSuperDummyLodDistance / ms_fOffscreenLodDistScale,fFOVEnd, fFOVStart, Color_red, false);
	}

	if(ms_bDebugScoring)
	{
		char tmp[256];
		Vec2V pos(0.01f, 0.01f);
		float yOffset = 0.01f;
		sprintf(tmp, "%s","Index, Addr, Network, DistSqr, Vis Local, <Local Real Dist, Vis Remote (Color = Current Mode)");
		grcDebugDraw::Text(pos, DD_ePCS_NormalisedZeroToOne, Color_blue, tmp);
		float fNextLineBreakDistance = ms_fDummyLodDistance*ms_fDummyLodDistance;
		for(int i = 0; i < ms_iVehicleCount; ++i)
		{
			CVehicleAiPriority& pPriority = ms_PrioritizedVehicleArray[i];
			if(pPriority.m_fEffectiveDistSq > fNextLineBreakDistance)
			{
				pos.SetYf(yOffset += 0.01f);
				fNextLineBreakDistance = fNextLineBreakDistance == ms_fDummyLodDistance*ms_fDummyLodDistance ? ms_fSuperDummyLodDistance*ms_fSuperDummyLodDistance : FLT_MAX;
			}		
			if(CVehicle::GetPool()->IsValidPtr(pPriority.m_pVehicle) && !pPriority.m_pVehicle->GetIsAircraft() && pPriority.m_pVehicle->InheritsFromAutomobile())
			{
				if(!pPriority.m_pVehicle->GetIsRetainedByInteriorProxy() && pPriority.m_pVehicle->GetOwnerEntityContainer() == NULL)
					continue;

				sprintf(tmp, "%d 0x%p (%s)= %.0f %s %s %s", i, pPriority.m_pVehicle,pPriority.m_pVehicle->IsNetworkClone() ? "C" : "L",	
				pPriority.m_fEffectiveDistSq, pPriority.bVisibleLocally?"T":"F", pPriority.bWithinLocalRealDist?"T":"F", pPriority.bVisibleRemotely?"T":"F");

				Color32 col = pPriority.m_pVehicle->IsDummy() ? (pPriority.m_pVehicle->IsSuperDummy() ? Color_red : Color_yellow) : Color_green;
				pos.SetYf(yOffset += 0.01f);
				grcDebugDraw::Text(pos, DD_ePCS_NormalisedZeroToOne, col, tmp);		
			}
		}
	}
}
#endif

// #if __DEV
// void PrintLoadedJunctionsWrapper()
// {
// 	CVirtualRoad::ms_VirtualJunctions.PrintSizeofAllLoadedJunctions();
// }
// #endif //__DEV

#if __BANK
void CVehicleAILodManager::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Vehicle AI and Nodes");
	if(pBank)
	{
		pBank->PushGroup("Vehicle lodding");

		pBank->AddToggle("Debug",									&ms_bDebug);
		pBank->AddToggle("Debug, extra info",						&ms_bDebugExtraInfo);
		pBank->AddToggle("Debug, peds info",						&ms_bDebugPedsInfo);
		pBank->AddToggle("Debug, extra wheel info",					&ms_bDebugExtraWheelInfo);
		pBank->AddToggle("Debug, scoring",							&ms_bDebugScoring);
		pBank->AddToggle("Debug, VM Lod",							&ms_bVMLODDebug, datCallback(UpdateDebugLogDisplay));
		ENABLE_FRAG_OPTIMIZATION_ONLY(pBank->AddToggle("Debug, frag cache info", &ms_bDebugFragCacheInfo);)
		pBank->AddSlider("Debug, draw range",						&ms_fDebugDrawRange, 0.0f, 20000.0f, 1.0f);
		pBank->AddToggle("Lodding Active",							&ms_bVehicleAILoddingActive);
		pBank->AddSlider("Lod - Dummy Distance",					&ms_fDummyLodDistance, 0.0f, 9999.0f, 1.0f);
		pBank->AddSlider("Lod - Super-Dummy Distance",				&ms_fSuperDummyLodDistance, 0.0f, 9999.0f, 1.0f);
		pBank->AddSlider("Lod - Super-Dummy Distance (Parked)",		&ms_fSuperDummyLodDistanceParked, 0.0f, 9999.0f, 1.0f);
		pBank->AddSlider("Lod - Offscreen distance scale",			&ms_fOffscreenLodDistScale, 1.0f, 10.0f, 0.1f);
		pBank->AddToggle("Lod - Occluded counts as offscreen",		&ms_bConsiderOccludedAsOffscreen);
		pBank->AddSlider("Lod - Far from pop center dist",			&ms_fLodFarFromPopCenterDistance, 0.0f, 9999.0f, 1.0f);
		pBank->AddToggle("N-Lod, Active",							&ms_bNLodActive);
		pBank->AddSlider("N-Lod, Real max",							&ms_iNLodRealMax, 0, 1000, 1);
		pBank->AddSlider("N-Lod, Dummy max",						&ms_iNLodDummyMax, 0, 1000, 1);
		pBank->AddSlider("N-Lod, Non-timesliced max",				&ms_iNLodNonTimeslicedMax, 0, 1000, 1);
		pBank->AddSlider("N-Lod, Real occupants max",				&ms_iNLodRealDriversAndPassengersMax, 0, 1000, 1);
		pBank->AddToggle("Display Dummy Vehicle Non Convert Reason",&ms_displayDummyVehicleNonConvertReason);
		pBank->AddToggle("Validate dummy constraints",				&ms_bValidateDummyConstraints);
		pBank->AddSlider("Validate dummy constraints, buffer",		&ms_fValidateDummyConstraintsBuffer, 0.0f, 100.0f, 1.0f);
		pBank->AddToggle("Timeslicing, Active",						&ms_bUseTimeslicing);
		pBank->AddSlider("Timeslicing, Num to Update",				&ms_TimesliceMaxUpdatesPerFrame, 1, 100, 1);
		pBank->AddSlider("Timeslicing, Min Update Period",			&ms_TimesliceMinUpdatePeriodInFrames, 0, 100, 1);
		pBank->AddToggle("Timeslicing, Display Full Updates",		&ms_bDisplayFullUpdates);
		pBank->AddSlider("Timeslicing, Allow On Screen Min Distance (Small)", &ms_TimesliceAllowOnScreenDistSmall, 0.0f, 10000.0f, 5.0f);
		pBank->AddSlider("Timeslicing, Allow On Screen Min Distance (Large)", &ms_TimesliceAllowOnScreenDistLarge, 0.0f, 10000.0f, 5.0f);
		pBank->AddToggle("Timeslicing, Allow skip updates in timesliced lod", &ms_bAllowSkipUpdatesInTimeslicedLod);
		pBank->AddToggle("Timeslicing, Skip timesliced updates while parked", &ms_bSkipTimeslicedUpdatesWhileParked);
		pBank->AddToggle("Timeslicing, Skip timesliced updates while stopped", &ms_bSkipTimeslicedUpdatesWhileStopped);
		pBank->AddSlider("Timeslicing, Maximum timesliced updates to skip while stopped", &ms_iMaxTimeslicedUpdatesToSkipWhileStopped, 0, 20, 1);
		pBank->AddToggle("Timeslicing, Display skipped timesliced updates", &ms_bDisplaySkippedTimeslicedUpdates);

		pBank->AddSlider("Lod Timeslicing",				&ms_iVehiclePriorityCountdownMax, 1, 10, 1);		

		pBank->AddSeparator();

#if __DEV
		pBank->AddToggle("Focus Vehicle Break On Check Should Attempt Conversion",	&ms_focusVehBreakOnCheckAttemptPopConversion);
		pBank->AddToggle("Focus Vehicle Break On Attempted Conversion",				&ms_focusVehBreakOnAttemptedPopConversion);
		pBank->AddToggle("Focus Vehicle Break On Conversion",						&ms_focusVehBreakOnPopulationConversion);
#endif // __DEV

		pBank->AddToggle("Display Dummy Wheel Hits",				&ms_bDisplayDummyWheelHits);
		pBank->AddToggle("Display Dummy Path",						&ms_bDisplayDummyPath);
		pBank->AddToggle("Display Dummy Path (detailed)",			&ms_bDisplayDummyPathDetailed);
		pBank->AddToggle("Display Dummy Path Constraint",			&ms_bDisplayDummyPathConstraint);
		pBank->AddToggle("Display Virtual Junction Heightmaps",		&ms_bDisplayVirtualJunctions);

		pBank->AddToggle("Raise dummy road",						&ms_bRaiseDummyRoad);
		pBank->AddToggle("Raise dummy junctions",					&ms_bRaiseDummyJunctions);
		pBank->AddSlider("Raise dummy road, height",				&ms_fRaiseDummyRoadHeight, -10.0f, 10.0f, 0.01f);

		pBank->AddToggle("Use dummy wheel transition",				&ms_bUseDummyWheelTransition);
		pBank->AddToggle("Use The Dummy Vehicle System",			&ms_bUseDummyLod);
		pBank->AddToggle("Enable use of Super-Dummy LOD",			&ms_bUseSuperDummyLod);
		pBank->AddToggle("Enable use of Super-Dummy LOD for parked vehicles", &ms_bUseSuperDummyLodForParked);
		pBank->AddToggle("Enable use of Virtual Junction Heightmap Sampling",	&CVirtualRoad::ms_bEnableVirtualJunctionHeightmaps);
		pBank->AddToggle("Enable constraint slack when changing paths", &CVirtualRoad::ms_bEnableConstraintSlackWhenChangingPaths);
// #if __DEV
// 		pBank->AddButton("Print Size of All Loaded Junctions", datCallback(PrintLoadedJunctionsWrapper));
// #endif //__DEV
		pBank->AddSlider("Force Lod Distance",						&ms_fForceLodDistance, -1.0f, 1000.0f, 1.0f);
		pBank->AddToggle("Force all Super-dummys",					&ms_bAllSuperDummys);
		pBank->AddToggle("Force all timesliced",					&ms_bForceAllTimesliced);
		pBank->AddToggle("De-timeslice transmission and sleep",		&ms_bDeTimesliceTransmissionAndSleep);
		pBank->AddToggle("De-timeslice wheel collisions",			&ms_bDeTimesliceWheelCollisions);

		pBank->AddSlider("Super-dummy steer speed",					     &ms_fSuperDummySteerSpeed, 0.0f, PI*4.0f, 0.05f);
        pBank->AddSlider("Super-dummy clone heading to vel blend ratio", &ms_fSuperDummyCloneHeadingToVelBlendRatio, 0.0f, 1.0f, 0.01f);

		pBank->PushGroup("Pretend Occupants", false);
		pBank->AddToggle("Use The Pretend Occupants System",		&ms_usePretendOccupantsSystem);
		pBank->AddSlider("Occupant Removal Distance (OnScreen)",				&ms_makePedsIntoPretendOccupantsRangeOnScreen,	0.0f, 1000.0f, 1.0f);
		pBank->AddSlider("Occupant Restore Distance (OnScreen)",				&ms_makePedsFromPretendOccupantsRangeOnScreen,	0.0f, 1000.0f, 1.0f);
		pBank->AddSlider("Occupant Removal Distance (OFFScreen)",				&ms_makePedsIntoPretendOccupantsRangeOffScreen,	0.0f, 1000.0f, 1.0f);
		pBank->AddSlider("Occupant Restore Distance (OFFScreen)",				&ms_makePedsFromPretendOccupantsRangeOffScreen,	0.0f, 1000.0f, 1.0f);
		pBank->PopGroup();

		const char * pConstraintModes[] =
		{
			"None",
			"Physical distance only",
			"Physical distance & rotation",
			"Non-physical distance"
		};

		pBank->AddCombo("Dummy constraint method:", &ms_iDummyConstraintModePrivate, 3, pConstraintModes);
		pBank->AddCombo("SuperDummy constraint method:", &ms_iSuperDummyConstraintModePrivate, 4, pConstraintModes);
		pBank->AddSlider("Distance constraint extra leeway", &ms_fConstrainToRoadsExtraDistance, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Distance constraint extra leeway (junction node)", &ms_fConstrainToRoadsExtraDistanceAtJunction, 0.0f, 50.0f, 0.1f);
		pBank->AddSlider("Distance constraint extra leeway (pre-junction node)", &ms_fConstrainToRoadsExtraDistancePreJunction, 0.0f, 50.0f, 0.1f);
		pBank->AddSlider("Portion of Junction Length for constraint buffer dist", &ms_fPercentOfJunctionLengthToAddToConstraint, 0.0f, 1.0f, 0.01f);

		pBank->AddSlider("Dummy Path reconsider Time",					&ms_dummyPathReconsiderLengthMs, 0, 2000, 10);
		pBank->AddSlider("Dummy Max Dist From Roads Edge Mult",			&ms_dummyMaxDistFromRoadsEdgeMult, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Dummy Max Dist From Road From Above Mult",	&ms_dummyMaxDistFromRoadFromAboveMult, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Dummy Max Dist From Road From Below Mult",	&ms_dummyMaxDistFromRoadFromBelowMult, 0.0f, 10.0f, 0.1f);
		pBank->AddToggle("Use Fixup Collision With Wheel Surface",		&ms_convertUseFixupCollisionWithWheelSurface);
		pBank->AddToggle("Enforce ConvertToDummy Must Be Near Paths",	&ms_convertEnforceDummyMustBeNearPaths);
		pBank->AddToggle("Convert All Exterior Vehicles To Dummy When In Interior", &ms_bConvertExteriorVehsToDummyWhenInInterior);
		pBank->AddToggle("Display Dummy Vehicle Markers",				&ms_displayDummyVehicleMarkers);
		pBank->AddToggle("Collide with others in super-dummy mode",		&ms_bCollideWithOthersInSuperDummyMode);
		pBank->AddToggle("Collide with others in super-dummy inactive mode",&ms_bCollideWithOthersInSuperDummyInactiveMode);
		pBank->AddToggle("Freeze Parked SuperDummy When Collisions Not Loaded", &ms_bFreezeParkedSuperDummyWhenCollisionsNotLoaded);
		pBank->AddToggle("Disable physics in super-dummy mode",			&ms_bDisablePhysicsInSuperDummyMode);
		pBank->AddToggle("Use road camber in super-dummy mode",			&ms_bUseRoadCamber);
		pBank->AddToggle("Allow trailers to convert from real",			&ms_bAllowTrailersToConvertFromReal);
		pBank->AddToggle("Allow alt RouteInfo for rear wheels",			&ms_bAllowAltRouteInfoForRearWheels);

		pBank->AddToggle("Disable SuperDummy for slow bikes",			&ms_bDisableSuperDummyForSlowBikes);
		pBank->AddSlider("Disable SuperDummy for slow bikes, speed",	&ms_fDisableSuperDummyForSlowBikesSpeed, 0.0f, 100.0f, 0.1f);

		CVehicleAiLodTester::InitWidgets(*pBank);

		pBank->PopGroup();
	}
}

void CVehicleAILodManager::UpdateDebugLogDisplay()
{
	CVectorMap::m_bDisplayLocalPlayerCamera = ms_bVMLODDebug;
}

#endif


///////////////////////////////////////////////////////////////////////////////
// CVehicleAiLodTester

#if __BANK

bool CVehicleAiLodTester::ms_bTestForAboveRoad = false;
float CVehicleAiLodTester::ms_fTestForAboveRoadThreshold = 1.0f;
bool CVehicleAiLodTester::ms_bTestForBelowRoad = false;
float CVehicleAiLodTester::ms_fTestForBelowRoadThreshold = 1.0f;

bool CVehicleAiLodTester::ms_bTeleportVehicles = false;
Vector3 CVehicleAiLodTester::ms_vTeleportOffset(0.0f,0.0f,5.0f);
Vector3 CVehicleAiLodTester::ms_vTeleportEulers(0.0f,0.0f,0.0f);

void CVehicleAiLodTester::InitWidgets(bkBank & bank)
{
	// Validations and modifications
	bank.PushGroup("Vehicle validation and modification");
	bank.AddToggle("Test for above road", &ms_bTestForAboveRoad);
	bank.AddSlider("Test for above road, threshold", &ms_fTestForAboveRoadThreshold, 0.0f, 100.0f, 0.1f);
	bank.AddToggle("Test for below road", &ms_bTestForBelowRoad);
	bank.AddSlider("Test for below road, threshold", &ms_fTestForBelowRoadThreshold, 0.0f, 100.0f, 0.1f);
	bank.AddToggle("Teleport vehicles", &ms_bTeleportVehicles);
	bank.AddSlider("Teleport vehicles, offset", &ms_vTeleportOffset, -100.0f, 100.0f, 0.1f);
	bank.AddSlider("Teleport vehicles, Eulers", &ms_vTeleportEulers, -10.0f, 10.0f, 0.1f);
	bank.PopGroup();
}

void CVehicleAiLodTester::ValidateState()
{
	// Player vehicle shouldn't be dummy
	CVehicle * pPlayerVehicle = FindPlayerVehicle();
	if(pPlayerVehicle)
	{
		Assertf(pPlayerVehicle->GetVehicleAiLod().GetDummyMode()==VDM_REAL, "Player's vehicle is in dummy mode %i - it should always be VDM_REAL!", pPlayerVehicle->GetVehicleAiLod().GetDummyMode());
	}

	// TODO: Check for floating or sunk vehicles
}

void CVehicleAiLodTester::ApplyChanges()
{
	CVehicle::Pool *pool = CVehicle::GetPool();

	if(ms_bTeleportVehicles)
	{
		for(int i=0; i<pool->GetSize(); i++)
		{
			if(CVehicle * pVeh = pool->GetSlot(i))
			{
				Mat34V oldMatrix(pVeh->GetMatrix());
				const Vec3V vEulers = Mat34VToEulersXYZ(oldMatrix);
				const Vec3V vNewEulers = vEulers + RCC_VEC3V(ms_vTeleportEulers);
				const Vec3V vNewPos = oldMatrix.d() + RCC_VEC3V(ms_vTeleportOffset);
				Mat34V newMatrix;
				Mat34VFromEulersXYZ(newMatrix,vNewEulers,vNewPos);
				pVeh->SetMatrix(RCC_MATRIX34(newMatrix),true,true,true);
			}
		}
		ms_bTeleportVehicles = false;
	}
}

#endif // __BANK
