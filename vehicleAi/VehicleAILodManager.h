// FILE :    VehicleAILodManager.h
// PURPOSE : Sets the AI/physics LOD level of all vehicles in the world
// CREATED : 16-03-2011

#ifndef VEHICLE_AILOD_MANAGER_H
#define VEHICLE_AILOD_MANAGER_H

#include "fragment/type.h"
#include "Vector/Vector3.h"
#include "VectorMath/Vec3V.h"
#include "Vehicles/VehicleDefines.h"

#include "atl/array.h"
#include "atl/queue.h"

class CVehicle;

///////////////////////////////////////////////////////////////////////////////
class CVehicleAILodManager
{
public:
	class CVehicleAiPriority
	{
	public:
		static bool LessThan(const CVehicleAiPriority & a, const CVehicleAiPriority & b) { return (a.m_fEffectiveDistSq < b.m_fEffectiveDistSq); }
		CVehicle * m_pVehicle;
		float m_fEffectiveDistSq;
#if __BANK
		bool bVisibleLocally;
		bool bWithinLocalRealDist;
		bool bVisibleRemotely;
#endif
	};

public:
	static void InitValuesFromConfig();
	static void Init(unsigned initMode);

	static void Update();
	static eVehicleDummyMode CalcDesiredLodForPoint(Vec3V_In vPos);

	// Hack! These accessors will be removed once all dummy transitions are handled in CVehicleAILodManager::Update()
	static void SetLodFlag(CVehicle * pVehicle, const u32 iFlag);
	static void ClearLodFlag(CVehicle * pVehicle, const u32 iFlag);

	// PURPOSE:	Check if a given vehicle should do a full update of tasks and other AI this frame.
	static bool ShouldDoFullUpdate(CVehicle& veh);

    static float GetSuperDummyLodDistance() { return ms_fSuperDummyLodDistance; }

	static float GetLodRangeScale() {return ms_fLodRangeScale;}

	// PURPOSE:	Get an estimate for how long a timesliced update time step can be at most, at the current time.
	//			It's not guaranteed, but should be an overestimate in most cases.
	static float GetExpectedWorstTimeslicedTimestep() { return ms_ExpectedWorstTimeslicedTimestep; }
	
	// PURPOSE: Determine the permitted number of pretend occupants that can be converted to real peds in vehicles,
	// taking into account scheduled real ped spawns that have yet to be executed.
	static int ComputeRealDriversAndPassengersRemaining();

	// PURPOSE: Determine the desired number of total real peds in vehicles,
	// taking into account any special circumstances.
	static int ComputeRealDriversAndPassengersMax();

	// PURPOSE: Determine the number of ambient real peds in vehicles that should not influence the dynamic limit.
	static int ComputeRealDriversAndPassengersExceptions();

	// PURPOSE:	Prevent a vehicle from being timesliced for a frame, immediately
	//			making it receive full updates.
	static void DisableTimeslicingImmediately(CVehicle& veh);

	// PURPOSE:	Called when we are about to reuse a vehicle from the reuse pool.
	static void ResetOnReuseVehicle(CVehicle& veh);

	static bool GetAllowSkipUpdatesInTimeslicedLod() { return ms_bAllowSkipUpdatesInTimeslicedLod; }
	static bool GetSkipTimeslicedUpdatesWhileParked() { return ms_bAllowSkipUpdatesInTimeslicedLod && ms_bSkipTimeslicedUpdatesWhileParked; }
	static bool GetSkipTimeslicedUpdatesWhileStopped() { return ms_bAllowSkipUpdatesInTimeslicedLod && ms_bSkipTimeslicedUpdatesWhileStopped; }
	static u32 GetMaxTimeslicedUpdatesToSkipWhileStopped() { return ms_iMaxTimeslicedUpdatesToSkipWhileStopped; }
#if __BANK
	static bool GetDisplaySkippedTimeslicedUpdates() { return ms_bDisplaySkippedTimeslicedUpdates; }
#endif // __BANK

	static s32 GetNumVehiclesConvertedToRealPedsThisFrame() { return ms_iNumVehiclesConvertedToRealPedsThisFrame; }

	enum EDummyConstraintMode
	{
		DCM_None,						// vehicle is not constrained at all
		DCM_PhysicalDistance,			// constrained physically to the road extents
		DCM_PhysicalDistanceAndRotation	// constrained physically to road extents, and prevented from turning over
	};

#if __BANK
	static void InitWidgets();
	static float ms_fForceLodDistance;
	static bool ms_bForceAllTimesliced;
	static bool ms_bForceDummyCars;
	static bool ms_displayDummyVehicleMarkers;
	static bool	ms_bDisplayDummyWheelHits;
	static bool ms_bDisplayDummyPath;
	static bool ms_bDisplayDummyPathDetailed;
	static bool ms_bDisplayDummyPathConstraint;
	static bool ms_bDisplayVirtualJunctions;
	static bool ms_bRaiseDummyRoad;
	static bool ms_bRaiseDummyJunctions;
	static float ms_fRaiseDummyRoadHeight;
	static bool ms_bUseDummyWheelTransition;
	static bool ms_bDebug;
	static bool ms_bVMLODDebug;
	static bool ms_bDebugExtraInfo;
	static bool ms_bDebugPedsInfo;
	static bool ms_bDebugExtraWheelInfo;
	static bool ms_bDebugScoring;
	ENABLE_FRAG_OPTIMIZATION_ONLY(static bool ms_bDebugFragCacheInfo;)
	static float ms_fDebugDrawRange;
	static bool ms_displayDummyVehicleNonConvertReason;
	static bool ms_bDisplayFullUpdates;
	static bool ms_bValidateDummyConstraints;
	static float ms_fValidateDummyConstraintsBuffer;
	static void Debug();
	static void	UpdateDebugLogDisplay();

	static eVehicleDummyMode DevOverrideLod(eVehicleDummyMode lodIn);
#endif // __BANK

#if __DEV
	static bool ms_focusVehBreakOnCheckAttemptPopConversion;
	static bool ms_focusVehBreakOnAttemptedPopConversion;
	static bool ms_focusVehBreakOnPopulationConversion;
#endif // __DEV

	static bank_bool ms_bUseDummyLod;
	static bank_bool ms_bUseSuperDummyLod;
	static bool ms_bUseSuperDummyLodForParked;
	static bank_bool ms_bAllSuperDummys;
	static bank_bool ms_bDeTimesliceTransmissionAndSleep;
	static bank_bool ms_bDeTimesliceWheelCollisions;
	static bank_bool ms_bConvertExteriorVehsToDummyWhenInInterior;
	static int ms_iTimeBetweenConversionAttempts;
	static int ms_iTimeBetweenConversionToRealAttempts;

	static bank_bool ms_bDisablePhysicsInSuperDummyMode;
	static bank_bool ms_bCollideWithOthersInSuperDummyMode;
	static bank_bool ms_bCollideWithOthersInSuperDummyInactiveMode;
	static bank_bool ms_bFreezeParkedSuperDummyWhenCollisionsNotLoaded;
	static bool ms_bDisableConstraintsForSuperDummyInactive;
	static bank_bool ms_bUseRoadCamber;
	static bank_bool ms_bAllowTrailersToConvertFromReal;
	static bank_bool ms_bAllowAltRouteInfoForRearWheels;

	static u32 ms_dummyPathReconsiderLengthMs;
	static float ms_dummyMaxDistFromRoadsEdgeMult;
	static float ms_dummyMaxDistFromRoadFromAboveMult;
	static float ms_dummyMaxDistFromRoadFromBelowMult;
	static bool ms_convertUseFixupCollisionWithWheelSurface;
	static bool ms_convertEnforceDummyMustBeNearPaths;

private:
	static bank_bool ms_bAllowSkipUpdatesInTimeslicedLod;
	static bank_bool ms_bSkipTimeslicedUpdatesWhileParked;
	static bank_bool ms_bSkipTimeslicedUpdatesWhileStopped;
	static bank_u32 ms_iMaxTimeslicedUpdatesToSkipWhileStopped;
#if __BANK
	static bool ms_bDisplaySkippedTimeslicedUpdates;
#endif // __BANK

public:
	// PURPOSE:	If true, prevent bikes from being superdummy if moving slowly.
	static bank_bool ms_bDisableSuperDummyForSlowBikes;

	// PURPOSE:	If ms_bDisableSuperDummyForSlowBikes is set, this is the speed threshold
	//			for when to switch to dummy.
	static bank_float ms_fDisableSuperDummyForSlowBikesSpeed;

	// taken from EDummyConstraintMode
	static bank_float ms_fConstrainToRoadsExtraDistance;
	static bank_float ms_fConstrainToRoadsExtraDistanceAtJunction;
	static bank_float ms_fConstrainToRoadsExtraDistancePreJunction;
	static bank_float ms_fPercentOfJunctionLengthToAddToConstraint;

	static u32 ms_preferredNumRealVehiclesAroundPlayer;

	static bool ms_usePretendOccupantsSystem;
	static float ms_makePedsIntoPretendOccupantsRangeOnScreen;
	static float ms_makePedsFromPretendOccupantsRangeOnScreen;
	static float ms_makePedsIntoPretendOccupantsRangeOffScreen;
	static float ms_makePedsFromPretendOccupantsRangeOffScreen;

	static bank_float ms_fSuperDummySteerSpeed; // (hacky - we should derive this per-car from their handling data)
    
    // PURPOSE: The rate at which clone superdummy vehicles blend there heading to match their direction of velocity
    static bank_float ms_fSuperDummyCloneHeadingToVelBlendRatio;

	// PURPOSE:	If set to true, vehicles are allowed to use timesliced AI updates when possible.
	static bool ms_bUseTimeslicing;

	// PURPOSE:	If timeslicing of vehicle AI is enabled, this is the max number of vehicles that get to update any given frame.
	static u16 ms_TimesliceMaxUpdatesPerFrame;

	// PURPOSE:	The minimum update period of timesliced vehicles, in frames. It's probably not desirable to get updates on every
	//			frame when a vehicle is considered timesliced, even if there are only a few vehicles, mostly because timesliced
	//			updates may bypass some expensive processes that are otherwise internally timesliced.
	static bank_u16 ms_TimesliceMinUpdatePeriodInFrames;

	// PURPOSE:	Beyond this distance, vehicle AI timeslicing may be used even for vehicles that are on screen and not occluded.
	static bank_float ms_TimesliceAllowOnScreenDistSmall;
	static bank_float ms_TimesliceAllowOnScreenDistLarge;

	// PURPOSE:	Used with ms_TimesliceMinUpdatePeriodInFrames to implement a minimum update period.
	static u16 ms_TimesliceCountdownUntilWrap;

	// PURPOSE:	Counter for how many vehicles use timeslicing. Since the manager update itself is timesliced,
	//			this only counts how many we've found so far, ms_TimesliceNumInLowLodPrevious would be more accurate
	//			if you need to know the current number.
	static u16 ms_TimesliceNumInLowLodCounting;

	// PURPOSE:	Number of vehicles in timeslicing LOD mode during the last completed pass over all vehicles, copied
	//			from ms_TimesliceNumInLowLodCounting.
	static u16 ms_TimesliceNumInLowLodPrevious;

	// PURPOSE:	Vehicle index used to keeps track of where the next window of timesliced updates will start.
	static s16 ms_TimesliceLastIndex;

	// PURPOSE:	Index of the first vehicle that may get a full update this frame.
	static s16 ms_TimesliceFirstVehIndex;

	// PURPOSE:	Index of the last vehicle that may get a full update this frame.
	static s16 ms_TimesliceLastVehIndex;

    static EDummyConstraintMode GetDummyConstraintMode();
    static EDummyConstraintMode GetSuperDummyConstraintMode();

	static void InitializeVehiclePriorityArray();

private:

	// PURPOSE: Makes conversion between real and dummy occupants for the given vehicle
	static void ManageVehiclePretendOccupants(CVehicle* pVehicle, const Vector3& popCtrlCentre, const int kMaxVehiclesConvertedToRealPedsPerFrame, const int kMaxRealPeds, int& inout_NumVehiclesConvertedToRealPeds, bool& out_bVehicleNotConvertedToRealDueToMAX);

	// PURPOSE:	Decide which timesliced vehicles get to update next.
	static void UpdateTimeslicing(int numInLowTimesliceLOD);

	// PURPOSE:	Set m_TimeslicedUpdateThisFrame on all vehicles within a range (with wraparound) to a specific value.
	static void SetTimeslicedUpdateFlagForVehiclesInRange(int poolStartIndex, int poolEndIndex, bool value);

	// LOD functions
	static void UpdateLodRangeScale();
	static int FindVehiclesByPriority();

	// PURPOSE:	Recompute ms_ExpectedWorstTimeslicedTimestep.
	static void UpdateExpectedWorstTimeslicedTimestep();

    static bank_s32 ms_iDummyConstraintModePrivate;
	static bank_s32 ms_iSuperDummyConstraintModePrivate;

	static bank_bool  ms_bVehicleAILoddingActive;
	static bank_float ms_fRealDistance;
	static bank_float ms_fDummyLodDistance;
	static bank_float ms_fSuperDummyLodDistance;
	static bank_float ms_fSuperDummyLodDistanceParked;
	static bank_float ms_fHysteresisDistance;
	static bank_float ms_fOffscreenLodDistScale;
	static bank_bool ms_bConsiderOccludedAsOffscreen;
	static bank_float ms_fLodFarFromPopCenterDistance;
	static float ms_fLodRangeScale;
	static bank_bool ms_bNLodActive;
	static bank_s32 ms_iNLodRealMax;
	static bank_s32 ms_iNLodDummyMax;
	static bank_s32 ms_iNLodNonTimeslicedMax;
	static bank_s32 ms_iNLodRealDriversAndPassengersMax;

	static atArray<CVehicleAiPriority> ms_PrioritizedVehicleArray;
	static Vec3V ms_vCachedVehiclePriorityCenter;
	static u8 ms_iVehiclePriorityCountdownMax;
	static u8 ms_iVehiclePriorityCountdown;
	static u32 ms_iVehicleCount;

	// PURPOSE:	Current expected worst case for timesliced time steps.
	static float ms_ExpectedWorstTimeslicedTimestep;

	// PURPOSE:	Number of elements in ms_FrameTimeMemory.
	static const int kFrameTimeMemorySize = 16;

	// PURPOSE:	Memory of the update time step for the last number of frames,
	//			used to estimate the probable worst case timesliced time step.
	static atQueue<float, kFrameTimeMemorySize> ms_FrameTimeMemory;
	
	// N-lod tracking
	static s32	ms_iNLodRealRemaining;
	static s32	ms_iNLodDummyRemaining;
	static s32	ms_iNLodNonTimeslicedRemaining;
	static s32	ms_iNLodRealDriversAndPassengersRemaining;
	static s32  ms_iMostDistantRealPedsPrioritizedIndex;
	static s32  ms_iClosestPretendPedsPrioritizedIndex;
	static s32	ms_iNumVehiclesConvertedToRealPedsThisFrame;
};

///////////////////////////////////////////////////////////////////////////////
// CVehicleAiLodTester
// Used to manipulate validate vehicle state and manipulate them to test LOD-related behaviors

#if __BANK
class CVehicleAiLodTester
{
public:
	static void InitWidgets(bkBank & bank);
	static void ValidateState();
	static void ApplyChanges();

protected:
	static bool ms_bTestForAboveRoad;
	static float ms_fTestForAboveRoadThreshold;
	static bool ms_bTestForBelowRoad;
	static float ms_fTestForBelowRoadThreshold;
	static bool ms_bTeleportVehicles;
	static Vector3 ms_vTeleportOffset;
	static Vector3 ms_vTeleportEulers;
};
#endif

#endif // VEHICLE_AILOD_MANAGER_H
