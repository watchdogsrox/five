// FILE :    PedAILodManager.h
// PURPOSE : Sets the LOD level of all peds in the world
// CREATED : 30-09-2009

#ifndef PED_AILOD_MANAGER_H
#define PED_AILOD_MANAGER_H


// rage headers
#include "atl/array.h"
#include "system/bit.h"
#include "math/amath.h"				// PositiveFloatLessThan()
#include "vectormath/classes.h"		// Vec4V

class CPed;

// CPedAILod controls the allocation
class CPedAILodManager
{
protected:

	// PURPOSE:	Used to determine the size of some arrays on the stack. Should be cheap
	//			to bump up if we need more peds than this.
	static const int kMaxPeds = 256;

	// PURPOSE:	Entry used for scoring peds for N-lodding.
	struct PedAiPriority
	{
	public:
		// PURPOSE:	Comparison function for sorting based on AI score.
		static bool AiScoreLessThan(const PedAiPriority& a, const PedAiPriority& b)
		{
			return PositiveFloatLessThan_Unsafe(a.m_fAiScoreDistSq, b.m_fAiScoreDistSq);
		}
		// PURPOSE:	Comparison function for sorting based on animation update score.
		static bool AnimScoreLessThan(const PedAiPriority& a, const PedAiPriority& b)
		{
			return PositiveFloatLessThan_Unsafe(a.m_fAnimScoreDistSq, b.m_fAnimScoreDistSq);
		}

		CPed* m_pPed;
		float m_fDistSq;
		float m_fAiScoreDistSq;		// Score used for timeslicing AI updates.
		float m_fAnimScoreDistSq;	// Score used for timeslicing animation updates.
	};

public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();

	// PURPOSE:	Check if a given ped should do a full update of tasks and other AI this frame.
	static bool ShouldDoFullUpdate(CPed& ped);

	// PURPOSE: Check if a given ped should do a full navmesh tracker update this frame.
	static bool ShouldDoNavMeshTrackerUpdate(CPed& ped);

	// PURPOSE: Tell timeslicing to use the active camera for the timeslicing centre rather than population centre.  Will be reset each update, so call every frame.
	static void SetUseActiveCameraForTimeslicingCentre();

	// PURPOSE:	Check if timeslicing of peds is enabled at a global level.
	static bool IsTimeslicingEnabled()
	{	return ms_bPedAITimesliceLodActive;	}

	// PURPOSE:	Check if we should do CPed::ProcessPedStanding() only on the first physics update step if possible.
	static bool IsPedProcessPedStandingSingleStepActive()
	{	return ms_bPedProcessPedStandingSingleStepActive;	}

	// PURPOSE:	Check if we should do CPed::ProcessPhysics() only on the first physics update step if possible.
	static bool IsPedProcessPhysicsSingleStepActive()
	{	return ms_bPedProcessPhysicsSingleStepActive;	}

	// PURPOSE:	True if we are currently allowed to timeslice scenario peds more aggressively because we have lots of peds.
	static bool IsTimesliceScenariosAggressivelyCurrentlyActive()
	{	return ms_bTimesliceScenariosAggressivelyCurrentlyActive;	}

	// PURPOSE:	True if CPed::SimulatePhysicsForLowLod() should be allowed to skip position updates in some cases.
	static bool MayPedSimulatePhysicsForLowLodSkipPosUpdates()
	{	return ms_bPedSimulatePhysicsForLowLodSkipPosUpdates;	}

#if __BANK
	static void AddDebugWidgets();
	static void DrawFullUpdate(CPed& ped);
	static void DrawFullAnimUpdate(CPed& ped);
#endif

private:

	static bool	ClipToHiLodCount( bool inValue, int &count );

	// PURPOSE:	Decide which timesliced peds get to update next.
	static void UpdateTimeslicing(int numInLowTimesliceLOD);

	// PURPOSE:	Observe the frame rate and ped distribution, and update the animation timeslicing thresholds and periods accordingly.
	// PARAMS:	priArray		- Array of scores for peds.
	//			numPeds			- The number of peds in priArray[].
	// NOTES:	This function may change the order of priArray[].
	static void UpdateAnimTimeslicing();

	// PURPOSE:	Called by UpdateAnimTimeslicing() to recompute ms_AnimUpdateModifiedDistances[],
	//			based on the current ped distribution and the current update period levels.
	// PARAMS:	priArray		- Array of scores for peds.
	//			numPeds			- The number of peds in priArray[].
	static void ComputeAnimTimeslicingDistances();

	static void	PrintStats();

	static bank_bool	ms_bPedAILoddingActive;

	// Controls for skipping ped navmesh tracker updates
	static bank_bool	ms_bEnableNavMeshTrackerUpdateSkipping;
	static bank_bool	ms_bAllowNavMeshTrackerUpdateSkipOnFullUpdates;

public:
	static bank_float	ms_fPhysicsAndEntityPedLodDistance;
	static bank_float	ms_fMotionTaskPedLodDistance;
	static bank_float	ms_fMotionTaskPedLodDistanceOffscreen;
	static bank_float	ms_fEventScanningLodDistance;
	static bank_float	ms_fRagdollInVehicleDistance;
	static bank_float	ms_fTimesliceIntelligenceDistance;

	// PURPOSE:	Upper limit to how far away we can use "extra hi LOD on screen" feature.
	static bank_float	ms_iExtraHiLodOnScreenMaxDist;

	// PURPOSE:	How many of the closest on-screen peds that could become hi LOD beyond the normal range.
	static bank_u8		ms_iExtraHiLodOnScreenCount;

	// PURPOSE:	How many peds in total can exist before we stop allowing extra hi LOD guys.
	static bank_u8		ms_iExtraHiLodOnScreenMaxPedsTotal;

	// PURPOSE:	True if extra hi LODs are currently allowed, based on the total ped count.
	static bool			ms_iExtraHiLodCurrentlyActive;

	// PURPOSE:	True if we are currently allowed to timeslice scenario peds more aggressively because we have lots of peds.
	static bool			ms_bTimesliceScenariosAggressivelyCurrentlyActive;

	// PURPOSE:	If set, visualize peds getting full updates, in the 3D world, using lines from the player.
	static bank_bool	ms_bDisplayFullUpdatesLines;

	// PURPOSE:	If set, visualize peds getting full updates, in the 3D world, using spheres.
	static bank_bool	ms_bDisplayFullUpdatesSpheres;

	// PURPOSE:	If set, visualize peds getting animation updates, using lines from the player.
	static bank_bool	ms_bDisplayFullAnimUpdatesLines;

	// PURPOSE:	If set, visualize peds getting animation updates, using spheres.
	static bank_bool	ms_bDisplayFullAnimUpdatesSpheres;

#if __BANK
	// PURPOSE:	If set, display N-lod scores on screen.
	static bool			ms_bDisplayNLodScores;
#endif	// __BANK
	
private:
	// PURPOSE:	Set up an array of PedAiPriority elements, sorted by a modified distance score.
	static int FindPedsByPriority();
#if __BANK
	static void GetCenterInfoString(char* buffOut, int maxSz);

	// PURPOSE:	Do debug drawing related to FindPedsByPriority().
	static void DebugDrawFindPedsByPriority();
#endif	// __BANK
	static atArray<PedAiPriority> ms_pedAiPriorityArray;
	static u8 ms_pedAiPriorityBatchFrames;
	static bool	ms_bForcePedAiPriorityUpdate;
	// PURPOSE:	Update visibility flags for all peds.
	static void UpdatePedVisibility();
public:
	// PURPOSE:	Update visibility flags for a ped.
	static void UpdatePedVisibility(CPed* pPed);
private:
	// PURPOSE:	Used by FindPedsByPriority() to compute the scores and distances for all peds.
	static void ComputePedScores(Vec4V* pDistSqArrayOut, Vec4V* pAiScoreDistSqArrayOut, Vec4V* pAnimScoreDistSqArrayOut, int maxVecs);

	// PURPOSE:	Determine the position used to compute the score of peds for AI/animation LOD purposes.
	static Vec3V_Out FindPedScoreCenter();

	static bank_bool	ms_bPedAINLodActive;

	// PURPOSE:	If set, allow timeslicing of ped AI.
	static bool			ms_bPedAITimesliceLodActive;

	// PURPOSE:	If set, allow timeslicing of ped animations.
	static bool			ms_bPedAnimTimesliceLodActive;

	// PURPOSE:	If set, try to do CPed::ProcessPedStanding() just once per frame per ped, rather than once per physics step.
	static bool			ms_bPedProcessPedStandingSingleStepActive;

	// PURPOSE:	If set, try to do CPed::ProcessPhysics() just once per frame per ped, rather than once per physics step.
	static bool			ms_bPedProcessPhysicsSingleStepActive;

	// PURPOSE:	If set, make CPed::SimulatePhysicsForLowLod() skip position updates on some frames when possible.
	static bool			ms_bPedSimulatePhysicsForLowLodSkipPosUpdates;

	// PURPOSE:	Normally true, can be set to false to disable what UpdatePedVisibility() does.
	static bank_bool	ms_bUpdateVisibility;

	// PURPOSE:	If true, use COcclusion to determine ped visibility.
	static bank_bool	ms_bUseOcclusionTests;

	static bank_s32		ms_hiLodPhysicsAndEntityCount;
	static bank_s32		ms_hiLodMotionTaskCount;
	static bank_s32		ms_hiLodEventScanningCount;
	static bank_s32		ms_hiLodTimesliceIntelligenceCount;

	// PURPOSE:	Factor applied to the distance score for peds that are not visible.
	static bank_float	ms_NLodScoreModifierNotVisible;

	// PURPOSE:	Factor applied to the distance score for peds that are using a scenario.
	static bank_float	ms_NLodScoreModifierUsingScenario;

	static bank_bool	ms_bDisplayDebugStats;

#if __BANK
	static u8			ms_TimesliceForcedPeriod;
#endif

	// PURPOSE:	If timeslicing of ped AI is enabled, this is the max number of peds that get to update any given frame.
	static bank_s16		ms_TimesliceMaxUpdatesPerFrame;

	// PURPOSE:	Multiplier on the time period between timesliced updates for dead peds.
	static bank_u8		ms_TimesliceUpdatePeriodMultiplierForDeadPeds;

	// PURPOSE:	If we don't hit the ms_TimesliceMaxUpdatesPerFrame limit (i.e. when few peds are around), roughly how
	//			often does a timesliced ped get to update (in frames)?
	static bank_u8		ms_TimesliceUpdatePeriodWithFewPeds;

	// PURPOSE:	Minimum number of peds that we can have to activate the more aggressive timeslicing for scenarios.
	static bank_u16		ms_iTimesliceScenariosAggressivelyLimit;

	// PURPOSE:	Ped index used to keeps track of where the next window of timesliced updates will start.
	static s16			ms_TimesliceLastIndex;

	// PURPOSE:	Pointer to the first ped that may get a full update this frame.
	// NOTES:	Intentionally not a RegdPed, this is meant for pointer comparisons and is not safe to dereference.
	static CPed*		ms_TimesliceFirstPed;

	// PURPOSE:	Pointer to the last ped that may get a full update this frame.
	// NOTES:	Intentionally not a RegdPed, this is meant for pointer comparisons and is not safe to dereference.
	static CPed*		ms_TimesliceLastPed;

	// PURPOSE:	This counts up each time we finish a sweep over timesliced peds, up to
	//			ms_TimesliceUpdatePeriodMultiplierForDeadPeds-1 (and wraps around), to determine
	//			which dead peds get to update during this sweep.
	static u8			ms_TimesliceDeadPedPhase;

	// PURPOSE:	In order for visible low LOD peds to skip position updates, this is the minimum distance they have to be.
	static float		ms_SimPhysForLowLodSkipPosUpdatesDist;

	// PURPOSE:	The number of frames we have currently accumulated in ms_AnimUpdateRebalanceFrameTimeSum.
	static int			ms_AnimUpdateRebalanceFrameCount;

	// PURPOSE:	Accumulation of frame times, used to compute an average frame rate.
	static float		ms_AnimUpdateRebalanceFrameTimeSum;

	// PURPOSE:	Amount of time over which we are averaging the frame rate before adapting the thresholds.
	static float		ms_AnimUpdateRebalancePeriod;

	// PURPOSE:	The number of animation update frequency levels we can have.
	static const int	kMaxAnimUpdateFrequencyLevels = 4;

	// PURPOSE:	Base distance thresholds for different frequency levels. This is tuning
	//			data which doesn't change, but a modifier gets applied on top.
	// NOTES:	These "distances" are actually scores, but the scores correspond to distances
	//			for peds that are visible and wandering around.
	static float		ms_AnimUpdateBaseDistances[kMaxAnimUpdateFrequencyLevels];

	// PURPOSE:	Computed from ms_AnimUpdateBaseDistances[], and identical if all
	//			periods are different. If the update periods end up being the same at
	//			multiple levels, this array gets collapsed.
	// NOTES:	The number of actual entries in the array is ms_AnimUpdateNumFrequencyLevels.
	static float		ms_AnimUpdateCollapsedBaseDistances[kMaxAnimUpdateFrequencyLevels];

	// PURPOSE:	Same as ms_AnimUpdateCollapsedBaseDistances[] but with modifiers applied, and squared.
	// NOTES:	The number of actual entries in the array is ms_AnimUpdateNumFrequencyLevels.
	static float		ms_AnimUpdateModifiedDistancesSquared[kMaxAnimUpdateFrequencyLevels];

	// PURPOSE:	The current animation update period at different levels, in frames.
	//			(1 means update every other frame, 3 means update every three frames, etc).
	// NOTES:	The number of actual entries in the array is ms_AnimUpdateNumFrequencyLevels.
	static u8			ms_AnimUpdatePeriodsForLevels[kMaxAnimUpdateFrequencyLevels];

	// PURPOSE:	The current number of unique update period levels.
	static int			ms_AnimUpdateNumFrequencyLevels;

	// PURPOSE:	Tuning data matching ms_AnimUpdateBaseDistances[], where we specify roughly
	//			the desired frequency of updates at each level, in Hz. 
	static float		ms_AnimUpdateFrequencyLevels[kMaxAnimUpdateFrequencyLevels];

	// PURPOSE:	Tuning parameter for how many animation updates we want per frame.
	static float		ms_AnimUpdateDesiredMaxNumPerFrame;

	// PURPOSE:	Animation update score modifier for peds that are in a vehicle or are using a scenario.
	// NOTES:	A higher number means they may get fewer updates (as if they were further away).
	static float		ms_AnimUpdateScoreModifierInScenarioOrVehicle;

	// PURPOSE:	Animation update score modifier for peds that are off screen.
	// NOTES:	A higher number means they may get fewer updates (as if they were further away).
	static float		ms_AnimUpdateScoreModifierNotVisible;

	// PURPOSE:	If ms_AnimUpdateDisableFpsAdjustments is set, this is the FPS we pretend that we are running at, for anim timeslicing purposes.
	static float		ms_AnimUpdateDisabledFpsAdjustmentRate;

	// PURPOSE:	If true, do animation timeslicing computations as if the game had been running at an optimal framerate, even if it's not.
	static bool			ms_AnimUpdateDisableFpsAdjustments;

	// PURPOSE: Define the number of consecutive navmesh tracker updates to skip
	static bank_u8  ms_uMaxTrackerUpdateSkips_OnScreen;
	static bank_u8  ms_uMaxTrackerUpdateSkips_OffScreen;

#if __BANK
	// PURPOSE:	The last average frame rate we computed, for animation update purposes.
	static float		ms_StatAnimUpdateAverageFrameTime;

	// PURPOSE: Based on the average frame rate and ped distribution the last time
	//			ComputeAnimTimeslicingDistances() was called, this is the number of
	//			animation updates we should expect per frame.
	static float		ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame;

	// PURPOSE:	If true, draw animation update scores on screen.
	static bool			ms_bDisplayAnimUpdateScores;

	// PURPOSE:	If set to a non-zero value, force all peds to update animations at this period (in frames).
	static int			ms_DebugAnimUpdateForcedPeriod;
#endif	// __BANK

	// PURPOSE: If set use the active camera for the timeslicing centre, rather than population centre.  Reset each update.
	static bool			ms_UseActiveCameraForTimeslicingCentre;

};

#endif //PED_AILOD_MANAGER_H

