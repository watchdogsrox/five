// FILE :    PedAILod.cpp
// PURPOSE : Controls the LOD of the peds AI update.
// CREATED : 30-09-2009

// class header
#include "Peds/PedIntelligence/PedAiLodManager.h"

#include "grcore/debugdraw.h"

// network headers
#include "Network/NetworkInterface.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/scripted/ScriptDirector.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence/PedAiLod.h"
#include "Peds/PedIntelligence.h"
#include "Renderer/Occlusion.h"
#include "renderer/Renderer.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodScale.h"
#include "Scene/World/GameWorld.h"

// rage headers
#include "fwscene/stores/staticboundsstore.h"
#include "system/param.h"

AI_OPTIMISATIONS()

namespace AIStats
{
	EXT_PF_TIMER(CPedAILodManager_Update);
}
using namespace AIStats;

PARAM(notimeslicepeds, "Disable timesliced AI updates.");
PARAM(notimeslicepedanims, "Disable timesliced animation updates.");
PARAM(notimeslicepedanimfps, "Disable FPS adjustments for timesliced animation updates.");
PARAM(nopedphyssinglestep, "Don't try to do CPed::ProcessPhysics() just once per frame per ped, do it once per physics step.");
PARAM(nopedspringsinglestep, "Don't try to do CPed::ProcessPedStanding() just once per frame per ped, do it once per physics step.");

bank_bool  CPedAILodManager::ms_bPedAILoddingActive = true;
bool       CPedAILodManager::ms_bPedAITimesliceLodActive = false;
bool       CPedAILodManager::ms_bPedAnimTimesliceLodActive = false;
bool       CPedAILodManager::ms_bPedProcessPedStandingSingleStepActive = false;
bool       CPedAILodManager::ms_bPedProcessPhysicsSingleStepActive = false;
bool       CPedAILodManager::ms_bPedSimulatePhysicsForLowLodSkipPosUpdates = true;
bank_bool  CPedAILodManager::ms_bUpdateVisibility = true;
bank_bool  CPedAILodManager::ms_bUseOcclusionTests = false;		// Probably not necessary now if GetIsVisibleInSomeViewportThisFrame() includes occlusion.
bank_float CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance = 30.0f;
bank_float CPedAILodManager::ms_fMotionTaskPedLodDistance = 30.0f;
bank_float CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen = 7.5f;
bank_float CPedAILodManager::ms_fEventScanningLodDistance = 30.0f;
bank_float CPedAILodManager::ms_fRagdollInVehicleDistance = 30.0f;
bank_float CPedAILodManager::ms_fTimesliceIntelligenceDistance = 12.0f;

// Expose these to RAG
// These define the maximum number of entities that can have high lods on the following properties (if ms_bPedAINLodActive == true)
bank_bool	CPedAILodManager::ms_bPedAINLodActive			=	true;
bank_s32	CPedAILodManager::ms_hiLodPhysicsAndEntityCount	=	10;
bank_s32	CPedAILodManager::ms_hiLodMotionTaskCount		=	8;
bank_s32	CPedAILodManager::ms_hiLodEventScanningCount	=	20;
bank_s32	CPedAILodManager::ms_hiLodTimesliceIntelligenceCount = 5;
bank_float	CPedAILodManager::ms_iExtraHiLodOnScreenMaxDist	=	60.0f;
bank_u8		CPedAILodManager::ms_iExtraHiLodOnScreenCount	=	3;
bank_u8		CPedAILodManager::ms_iExtraHiLodOnScreenMaxPedsTotal = 60;
bool		CPedAILodManager::ms_iExtraHiLodCurrentlyActive	=	false;
bool		CPedAILodManager::ms_bTimesliceScenariosAggressivelyCurrentlyActive = false;
bank_u16	CPedAILodManager::ms_iTimesliceScenariosAggressivelyLimit = 70;
bank_float	CPedAILodManager::ms_NLodScoreModifierNotVisible =	4.0f;
bank_float	CPedAILodManager::ms_NLodScoreModifierUsingScenario = 4.0f;
bank_bool	CPedAILodManager::ms_bDisplayDebugStats			=	false;
#if __BANK
u8			CPedAILodManager::ms_TimesliceForcedPeriod		=	0;
bool		CPedAILodManager::ms_bDisplayNLodScores			=	false;
#endif	// __BANK
bank_bool	CPedAILodManager::ms_bDisplayFullUpdatesLines	=	false;
bank_bool	CPedAILodManager::ms_bDisplayFullUpdatesSpheres	=	false;
bank_bool	CPedAILodManager::ms_bDisplayFullAnimUpdatesLines	=	false;
bank_bool	CPedAILodManager::ms_bDisplayFullAnimUpdatesSpheres	=	false;
bank_s16	CPedAILodManager::ms_TimesliceMaxUpdatesPerFrame =	8;
bank_u8		CPedAILodManager::ms_TimesliceUpdatePeriodMultiplierForDeadPeds = 2;
bank_u8		CPedAILodManager::ms_TimesliceUpdatePeriodWithFewPeds = 4;
s16			CPedAILodManager::ms_TimesliceLastIndex			=	0;
CPed*		CPedAILodManager::ms_TimesliceFirstPed			=	NULL;
CPed*		CPedAILodManager::ms_TimesliceLastPed			=	NULL;
u8			CPedAILodManager::ms_TimesliceDeadPedPhase		=	0;
float		CPedAILodManager::ms_SimPhysForLowLodSkipPosUpdatesDist	= 120.0f;
float		CPedAILodManager::ms_AnimUpdateRebalanceFrameTimeSum	= 0.0f;
float		CPedAILodManager::ms_AnimUpdateRebalancePeriod			= 1.0f;
int			CPedAILodManager::ms_AnimUpdateRebalanceFrameCount		= 0;
float		CPedAILodManager::ms_AnimUpdateScoreModifierInScenarioOrVehicle = 2.0f;
float		CPedAILodManager::ms_AnimUpdateScoreModifierNotVisible = 10.0f;
int			CPedAILodManager::ms_AnimUpdateNumFrequencyLevels = 1;
u8			CPedAILodManager::ms_AnimUpdatePeriodsForLevels[kMaxAnimUpdateFrequencyLevels];
float		CPedAILodManager::ms_AnimUpdateDisabledFpsAdjustmentRate = 30.0f;
bool		CPedAILodManager::ms_AnimUpdateDisableFpsAdjustments = false;

float CPedAILodManager::ms_AnimUpdateFrequencyLevels[kMaxAnimUpdateFrequencyLevels] =
{
	1000.0f,		// For the first level, we basically always try to update every frame, at this point.
	14.0f,			// 14 Hz: every other frame when near 30 FPS.
	10.0f,			// 10 Hz
	6.0f			// 6 Hz: may not want to go much lower than this, to avoid problems with too large time deltas.
};

float CPedAILodManager::ms_AnimUpdateBaseDistances[kMaxAnimUpdateFrequencyLevels] =
{
	0.0f,			// 0-13 m for level 1 (update every frame).
	13.0f,			// 13-30 m for level 2.
	30.0f,			// 30-55 m for level 3.
	55.0f,			// 55+ m for level 4
};

float CPedAILodManager::ms_AnimUpdateCollapsedBaseDistances[kMaxAnimUpdateFrequencyLevels];
float CPedAILodManager::ms_AnimUpdateModifiedDistancesSquared[kMaxAnimUpdateFrequencyLevels];
float CPedAILodManager::ms_AnimUpdateDesiredMaxNumPerFrame = 45.0f;

#if __BANK
float		CPedAILodManager::ms_StatAnimUpdateAverageFrameTime = 0.033f;
float		CPedAILodManager::ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = 0.0f;
bool		CPedAILodManager::ms_bDisplayAnimUpdateScores = false;
int			CPedAILodManager::ms_DebugAnimUpdateForcedPeriod = 0;
#endif

// Whether to allow navmesh tracker update skipping
bank_bool CPedAILodManager::ms_bEnableNavMeshTrackerUpdateSkipping = true;

// Whether to allow navmesh tracker update skipping on peds doing full updates
bank_bool CPedAILodManager::ms_bAllowNavMeshTrackerUpdateSkipOnFullUpdates = false;

// How many consecutive navmesh tracker updates can be skipped for performance
bank_u8 CPedAILodManager::ms_uMaxTrackerUpdateSkips_OnScreen = 1;
bank_u8 CPedAILodManager::ms_uMaxTrackerUpdateSkips_OffScreen = 4;

atArray<CPedAILodManager::PedAiPriority> CPedAILodManager::ms_pedAiPriorityArray;
u8 CPedAILodManager::ms_pedAiPriorityBatchFrames = 5;
bool CPedAILodManager::ms_bForcePedAiPriorityUpdate = false;

bool CPedAILodManager::ms_UseActiveCameraForTimeslicingCentre = false;

void CPedAILodManager::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		ms_bPedAITimesliceLodActive = !PARAM_notimeslicepeds.Get();
		ms_bPedAnimTimesliceLodActive = !PARAM_notimeslicepedanims.Get();
		ms_bPedProcessPedStandingSingleStepActive = !PARAM_nopedspringsinglestep.Get();
		ms_bPedProcessPhysicsSingleStepActive = !PARAM_nopedphyssinglestep.Get();

		ms_AnimUpdateDisableFpsAdjustments = PARAM_notimeslicepedanimfps.Get();

		// Initialize some animation update variables, as if we had been running
		// at 30 fps when the game starts.
		ms_AnimUpdateRebalanceFrameTimeSum = ms_AnimUpdateRebalancePeriod;
		ms_AnimUpdateRebalanceFrameCount = (int)(30.0f*ms_AnimUpdateRebalancePeriod);
		ms_AnimUpdatePeriodsForLevels[0] = 1;
		ms_AnimUpdateNumFrequencyLevels = 1;
	}
	else if(initMode == INIT_CORE)
	{
		if(ms_pedAiPriorityArray.GetCount() == 0)
			ms_pedAiPriorityArray.ResizeGrow(CPed::GetPool()->GetSize());
	}
}

void CPedAILodManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		ms_pedAiPriorityArray.Reset();
	}
}

void CPedAILodManager::Update()
{
	PF_START_TIMEBAR("PedAILodMgr Update");
	PF_FUNC(CPedAILodManager_Update);

	if(fwTimer::IsGamePaused())
		return;

	int hiLodPhysicsAndEntityCount = ms_hiLodPhysicsAndEntityCount;
	int hiLodMotionTaskCount = ms_hiLodMotionTaskCount;
	int hiLodEventScanningCount = ms_hiLodEventScanningCount;
	int hiLodTimesliceIntelligenceCount = ms_hiLodTimesliceIntelligenceCount;

	// We will count how many peds we've found on screen, on foot, up to a certain number.
	int numOnScreenFoundOnFoot = 0;

	int numInLowTimesliceLOD = 0;

	static u32 s_lastPedPriorityCalculation = 0;
	
	if(ms_bForcePedAiPriorityUpdate || s_lastPedPriorityCalculation + ms_pedAiPriorityBatchFrames <= fwTimer::GetFrameCount())
	{		
		ms_bForcePedAiPriorityUpdate = false;
		s_lastPedPriorityCalculation = fwTimer::GetFrameCount();

		ms_pedAiPriorityArray.ResetCount();
		FindPedsByPriority();

		// Check if we got any peds, otherwise we can't do the std::sort() below since
		// element 0 doesn't exist.
		const int cnt = ms_pedAiPriorityArray.GetCount();
		if(cnt)
		{
			// Sort the array, based on the AI score.
			std::sort(&ms_pedAiPriorityArray[0], &ms_pedAiPriorityArray[0] + cnt, PedAiPriority::AiScoreLessThan);
		}
	}

#if __BANK
	DebugDrawFindPedsByPriority();
#endif	// __BANK

	// Do some animation timeslicing update. This doesn't change the peds themselves,
	// but may adapt distance thresholds, etc.
	if(ms_bPedAnimTimesliceLodActive)
	{
		UpdateAnimTimeslicing();
	}

	// Determine if we should activate the feature to keep the closest on screen on foot peds
	// in high LOD beyond the normal distance threshold. It gets deactivated if the total ped count
	// gets above a threshold, and reactivates if it drops down by a few from there (so it doesn't
	// toggle on and off too much).
	bool extraHiLodCurrentlyActive = ms_iExtraHiLodCurrentlyActive;
	if(extraHiLodCurrentlyActive)
	{
		if(ms_pedAiPriorityArray.GetCount() > ms_iExtraHiLodOnScreenMaxPedsTotal)
		{
			extraHiLodCurrentlyActive = false;
		}
	}
	else
	{
		if(ms_pedAiPriorityArray.GetCount() <= ms_iExtraHiLodOnScreenMaxPedsTotal - 4)
		{
			extraHiLodCurrentlyActive = true;
		}
	}

	// If this feature is enabled, set the count of how many peds we will allow.
	int extraHiLodOnScreen = 0;
	if(extraHiLodCurrentlyActive)
	{
		extraHiLodOnScreen = ms_iExtraHiLodOnScreenCount;
	}

	// Remember if it's on or off, for hysteresis on the next update.
	ms_iExtraHiLodCurrentlyActive = extraHiLodCurrentlyActive;

	int limit = ms_iTimesliceScenariosAggressivelyLimit;
	if(!ms_bTimesliceScenariosAggressivelyCurrentlyActive)
	{
		limit += 5;
	}
	ms_bTimesliceScenariosAggressivelyCurrentlyActive = (ms_pedAiPriorityArray.GetCount() >= limit);

	// Peds that are visible will get their squared distance multiplied by this factor,
	// for the purpose of comparing against distance thresholds. The effect is that when zooming
	// in with a sniper rifle, the peds that are visible should get a bump in LOD level.
	const float fInvLodDistSqScale = square(1.0f/Max(1.0f, g_LodScale.GetGlobalScale()));

	// Square the distance threshold for skipping position updates.
	const float simPhysForLowLodSkipPosUpdatesDistSq = square(ms_SimPhysForLowLodSkipPosUpdatesDist);

	for(int i = 0; i < ms_pedAiPriorityArray.GetCount(); i++)
	{
		CPed* pPed = ms_pedAiPriorityArray[i].m_pPed;
		if(!CPed::GetPool()->IsValidPtr(pPed))
			continue;

		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
			continue;

		CPedAILod& lod = pPed->GetPedAiLod();

		// Store the order of this ped. Matched to kMaxPeds. Will need to increase the storage size if kMaxPeds is bumped.
		Assert(i >= 0 && i < 256);
		lod.m_AiScoreOrder = (u8)i;

		// Make sure the animation update parameters have reasonable values to begin with.
		Assert(lod.m_AnimUpdatePeriod > 0);
		Assert(lod.m_AnimUpdatePhase < lod.m_AnimUpdatePeriod);

		// For the ped's given animation update period, update the phase
		// and time step variables. If the phase is 0, it means we did a full
		// update on the previous frame.
		if(lod.m_AnimUpdatePhase == 0)
		{
			// In this case, we have to wait for a whole period before we can
			// update again - though of course, if the period is 1, it's already time.
			lod.m_AnimUpdatePhase = lod.m_AnimUpdatePeriod - 1;
			lod.m_fTimeSinceLastAnimUpdate = fwTimer::GetTimeStep();
		}
		else
		{
			// Didn't update last frame, so decrease the phase counter and add
			// to the animation update time step for this ped. After this,
			// if m_AnimUpdatePhase is 0, it's time to update again on this frame.
			lod.m_AnimUpdatePhase--;
			lod.m_fTimeSinceLastAnimUpdate += fwTimer::GetTimeStep();
		}

		bool bIsOnScreenOnFoot = false;

		// If we haven't yet found enough on foot peds on screen, see if this one qualifies,
		// and count it.
		// Note that we need to check the timesliced peds too, so this needs to happpen before
		// the ShouldDoFullUpdateCheck() below.
		if(numOnScreenFoundOnFoot < extraHiLodOnScreen)
		{
			// The ped needs to be visible, not in a vehicle, not using a scenario, and not
			// be the local player (if we counted the player, you might get an extra hi LOD
			// AI ped when the player enters a vehicle, which wouldn't make sense).
			if(pPed->GetIsVisibleInSomeViewportThisFrame()
					&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)
					&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingScenario)
					&& !pPed->IsLocalPlayer())
			{
				// Count it and set the bool for use further down.
				numOnScreenFoundOnFoot++;
				bIsOnScreenOnFoot = true;
			}
		}

		// Check if this ped was timesliced and didn't get to update on the last frame.
		// If so, its blocking LOD flags wouldn't be set properly yet, so we couldn't use those
		// flags to determine the next update LOD (in practice, if this goes wrong, the high LOD
		// motion task tends to start again, causing terrible performance).
		if(!ShouldDoFullUpdate(*pPed) && !pPed->GetPedAiLod().GetForceNoTimesliceIntelligenceUpdate() && !pPed->GetPedAiLod().GetTaskSetNeedIntelligenceUpdate() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptForceNoTimesliceIntelligenceUpdate)
#if __BANK
				&& !ms_TimesliceForcedPeriod
#endif
				)
		{
			// We can't change this ped's LOD right now, but we should probably still include it
			// in the N-lod counts so that we set other peds' LODs correctly.
			ClipToHiLodCount(!pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics), hiLodPhysicsAndEntityCount);
			ClipToHiLodCount(!pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask), hiLodMotionTaskCount);
			ClipToHiLodCount(!pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning), hiLodEventScanningCount);

			numInLowTimesliceLOD++;
			continue;
		}

		if( !CPedAILodManager::ms_bPedAILoddingActive )
		{
			pPed->GetPedAiLod().SetLodFlags(CPedAILod::AL_DefaultLodFlags);
			pPed->GetPedAiLod().m_bMaySkipVisiblePositionUpdatesInLowLod = false;
		}
		else
		{
			const float fUnscaledDistanceSq = ms_pedAiPriorityArray[i].m_fDistSq;

			float fDistanceSq = fUnscaledDistanceSq;
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				fDistanceSq *= fInvLodDistSqScale;
			}

			// Determine if this ped should be allowed to skip position updates, if in low LOD and visible.
			pPed->GetPedAiLod().m_bMaySkipVisiblePositionUpdatesInLowLod = (fDistanceSq > simPhysForLowLodSkipPosUpdatesDistSq);

			// Physics lodding
			bool bBlockingPhysicsSet = pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodPhysics);

			//if the ped is a mission ped, and bounds aren't loaded, let them try to go to low lod anyway
			if (pPed->GetPedResetFlag( CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded ) && !g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pPed->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
			{
				bBlockingPhysicsSet = false;
			}

			// Never allow the local player to go into low physics LOD (url:bugstar:1570006)
			if(pPed == CGameWorld::FindLocalPlayer())
			{
				bBlockingPhysicsSet = true;
			}

			// Prevent peds in water from entering low physics LOD, unless they are fish
			if(!pPed->GetCapsuleInfo()->IsFish())
			{
				if(pPed->GetIsInWater() || pPed->GetIsSwimming() || pPed->m_Buoyancy.GetStatus()!=NOT_IN_WATER || (pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bIsWater) )
				{
					bBlockingPhysicsSet = true;
				}
			}

			// If this ped is on foot and on screen, and we haven't found too many of those already, determine
			// an alternative distance threshold for motion task and physics LOD computations.
			float extraHiOnScreenThresholdDist = 0.0f;
			if(bIsOnScreenOnFoot)
			{
				extraHiOnScreenThresholdDist = ms_iExtraHiLodOnScreenMaxDist;
			}

			const bool bOutSideHiLodPhysicsDistance = fDistanceSq < rage::square(Max(CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance, extraHiOnScreenThresholdDist));
			bool bHiLod = bBlockingPhysicsSet || bOutSideHiLodPhysicsDistance;

			if(NetworkInterface::IsGameInProgress())
			{
				// If we think we want to be in high LOD, and it's not because of bBlockingPhysicsSet,
				// and we are on foot on screen, we might be in high LOD because we are one of the closest
				// few peds, further away than the normal threshold. If so, we may not want to allow that
				// if the collision isn't streamed in, otherwise we may get some glitches due to how the
				// networking code (CNetObjPhysical::Update()) freezes/hides peds
				// (see B* 1630851: "When viewed from distance through a Sniper Scope, peds are seen flashing and stuttering").
				if(bHiLod && !bBlockingPhysicsSet && bIsOnScreenOnFoot)
				{
					// If we are closer than ms_fPhysicsAndEntityPedLodDistance, then we are not in high LOD
					// because of the special rule about the closest peds.
					if(fDistanceSq >= rage::square(CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance))
					{
						// If the bounds are not loaded around the ped, don't let it go to high LOD for this reason.
						if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pPed->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
						{
							bHiLod = false;
						}
					}
				}

				if( CGameWorld::FindLocalPlayer()	!= pPed &&
					CGameWorld::FindFollowPlayer()	== pPed )
				{
					bHiLod = true;
				}
			}

			// Only perform the NLodding if the only determining factor is distance
			if( !bBlockingPhysicsSet )
			{
				bHiLod = ClipToHiLodCount( bHiLod, hiLodPhysicsAndEntityCount );
			}

            // Don't switch peds to low LOD physics when standing on top of other physical objects,
            // this will place them inside the objects and they won't be able to swap back to high-LOD
            if(pPed->GetGroundPhysical())
            {
                bHiLod = true;
            }

			if(pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodEntityScanning))
			{
				pPed->GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(*pPed, true);
			}

			bool bScannedForEntitiesInRange = false;
			if( bHiLod )
			{
				// If we are coming from a state in which entity scanning was disabled, then ensure we
				// have up-to-date nearby object/vehicles.  We need these to test whether it's safe
				// to turn on full lod physics.
				if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
				{
					pPed->GetPedIntelligence()->GetObjectScanner()->ScanForEntitiesInRange(*pPed, true);
					pPed->GetPedIntelligence()->GetVehicleScanner()->ScanForEntitiesInRange(*pPed, true);
					bScannedForEntitiesInRange = true;
				}
				pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodEntityScanning);
			}
			else
			{
				CPedAILod& lod = pPed->GetPedAiLod();
				if(!lod.IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
				{
					CPedIntelligence& intel = *pPed->GetPedIntelligence();
					intel.GetVehicleScanner()->Clear();
					intel.GetDoorScanner()->Clear();
					//intel.GetPedScanner()->Clear();
					intel.GetObjectScanner()->Clear();
					lod.SetLodFlag(CPedAILod::AL_LodEntityScanning);
				}
			}

			if( bHiLod )
			{
				// If we are wishing to switch from low->real physics, we must
				// first ensure that we are not penetrating any objects/vehicles
				if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
				{
					// 
					if (!bScannedForEntitiesInRange && pPed->GetPedIntelligence()->GetWillForceScanOnSwitchToHighPhysLod())
					{
						pPed->GetPedIntelligence()->GetObjectScanner()->ScanForEntitiesInRange(*pPed, true);
						pPed->GetPedIntelligence()->GetVehicleScanner()->ScanForEntitiesInRange(*pPed, true);
						pPed->GetPedIntelligence()->SetWillForceScanOnSwitchToHighPhysLod(false);
					}

					if (bScannedForEntitiesInRange)
					{
						pPed->GetPedIntelligence()->SetWillForceScanOnSwitchToHighPhysLod(false);
					}

					if(pPed->GetPedAiLod().IsSafeToSwitchToFullPhysics())
					{
						pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodPhysics);
						pPed->GetPedIntelligence()->SetWillForceScanOnSwitchToHighPhysLod(true);
					}
				}
			}
			else
			{
				pPed->GetPedAiLod().SetLodFlag(CPedAILod::AL_LodPhysics);
			}

			// Motion task lodding
			// Decrement the lod change timer
			pPed->GetPedAiLod().SetBlockMotionLodChangeTimer(pPed->GetPedAiLod().GetBlockMotionLodChangeTimer()-fwTimer::GetTimeStep());

			// If timer is up, check to change lod
			if(pPed->GetPedAiLod().GetBlockMotionLodChangeTimer() <= 0.f)
			{
				bool bBlockingMotionTaskSet = (pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodMotionTask) REPLAY_ONLY(|| CReplayMgr::ShouldIncreaseFidelity())) || pPed->GetIsInVehicle();
				float fLowLodDistance = (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) REPLAY_ONLY(|| CReplayMgr::ShouldIncreaseFidelity()) ? 
														CPedAILodManager::ms_fMotionTaskPedLodDistance : 
														CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen);

				// Bump up the motion task LOD distance threshold if this was one of few on screen on foot peds.
				fLowLodDistance = Max(fLowLodDistance, extraHiOnScreenThresholdDist);

				// Square it and compare it.
				const float fLowLodDistanceSq = rage::square(fLowLodDistance);
				bHiLod = bBlockingMotionTaskSet || fDistanceSq < fLowLodDistanceSq;
				
				// Only perform the NLodding if the only determining factor is distance
				if(!bBlockingMotionTaskSet)
				{
					bHiLod = ClipToHiLodCount( bHiLod, hiLodMotionTaskCount );
				}

				if( bHiLod )
				{
					pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodMotionTask);
				}
				else
				{
					pPed->GetPedAiLod().SetLodFlag(CPedAILod::AL_LodMotionTask);
				}
			}
			
			// Eventscanning lodding
			bool bBlockingEventScanningSet = pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodEventScanning);
			bHiLod = bBlockingEventScanningSet || fDistanceSq < rage::square(CPedAILodManager::ms_fEventScanningLodDistance);

			if(!bBlockingEventScanningSet)
			{
				bHiLod = ClipToHiLodCount( bHiLod, hiLodEventScanningCount );
			}

			if( bHiLod )
			{
				pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodEventScanning);
			}
			else
			{
				pPed->GetPedAiLod().SetLodFlag(CPedAILod::AL_LodEventScanning);
			}

			bHiLod = pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodRagdollInVehicle)
				|| fDistanceSq < rage::square(CPedAILodManager::ms_fRagdollInVehicleDistance);

			if( bHiLod )
			{
				pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodRagdollInVehicle);
			}
			else
			{
				pPed->GetPedAiLod().SetLodFlag(CPedAILod::AL_LodRagdollInVehicle);
			}

			// AI timeslicing LOD
			bHiLod = !ms_bPedAITimesliceLodActive
					|| pPed->GetPedAiLod().GetForceNoTimesliceIntelligenceUpdate()
					|| pPed->GetPedAiLod().GetTaskSetNeedIntelligenceUpdate()
					|| pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate)
					|| pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptForceNoTimesliceIntelligenceUpdate)
					|| pPed->IsLocalPlayer();	// Probably safest to protect against the player ever using timeslicing.

			// If we have not already determined that we should be in high LOD, we may still want to go there
			// if we are close enough and we haven't already found enough high LOD peds.
			if(!bHiLod)
			{
				if(!lod.GetUnconditionalTimesliceIntelligenceUpdate() && ms_pedAiPriorityArray[i].m_fAiScoreDistSq < square(ms_fTimesliceIntelligenceDistance))
				{
					bHiLod = ClipToHiLodCount(true, hiLodTimesliceIntelligenceCount);
				}
			}
			else
			{
				// Note: this effectively just decreases hiLodTimesliceIntelligenceCount,
				// we intentionally don't use the return value.
				ClipToHiLodCount(true, hiLodTimesliceIntelligenceCount);
			}

#if __BANK
			if(ms_TimesliceForcedPeriod > 0)
			{
				bHiLod = false;
			}
#endif	// __BANK

			if( bHiLod )
			{
				pPed->GetPedAiLod().ClearLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
			}
			else
			{
				pPed->GetPedAiLod().SetLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
				numInLowTimesliceLOD++;
			}

			// Anim timeslicing LOD

			// Next, we will determine the update period this ped should use.
			// Use every frame as a starting point:
			int animUpdatePeriod = 1;

			// If animation timeslicing is allowed, and the ped doesn't have this type of LOD blocked,
			// check the score.
			if(ms_bPedAnimTimesliceLodActive && !lod.IsBlockedFlagSet(CPedAILod::AL_LodTimesliceAnimUpdate))
			{
				const float scoreSq = ms_pedAiPriorityArray[i].m_fAnimScoreDistSq;

				// Loop over the levels we have determined and see which one this ped's score falls within.
				int level = ms_AnimUpdateNumFrequencyLevels - 1;	// If nothing else matches, we'll use the last one.
				const int numLevels = ms_AnimUpdateNumFrequencyLevels;
				for(int i = 0; i < numLevels - 1; i++)
				{
					// Compare the (squared) score against the (squared) threshold. These are positive numbers
					// so we can use integer comparisons for speed.
					if(PositiveFloatLessThan_Unsafe(scoreSq, ms_AnimUpdateModifiedDistancesSquared[i + 1]))
					{
						level = i;
						break;
					}
				}

				// Get the period at this level.
				Assert(level >= 0 && level < ms_AnimUpdateNumFrequencyLevels);
				animUpdatePeriod = ms_AnimUpdatePeriodsForLevels[level];

#if __BANK
				if(ms_DebugAnimUpdateForcedPeriod)
				{
					animUpdatePeriod = ms_DebugAnimUpdateForcedPeriod;
				}
#endif	// __BANK
			}

			// If the period is the same as before, we don't have to do anything.
			if(lod.m_AnimUpdatePeriod != animUpdatePeriod)
			{
				// Update the AL_LodTimesliceAnimUpdate flag, so that it's set ("low LOD")
				// unless we're going to be updating on every frame. This isn't currently
				// really needed for anything, but is handy for debugging, or could be used by the
				// peds if they need to do something differently in that case.
				if(animUpdatePeriod == 1)
				{
					lod.ClearLodFlag(CPedAILod::AL_LodTimesliceAnimUpdate);
				}
				else
				{
					lod.SetLodFlag(CPedAILod::AL_LodTimesliceAnimUpdate);
				}

				// Store the new period.
				animUpdatePeriod = Clamp(animUpdatePeriod, 0, 63);
				lod.m_AnimUpdatePeriod = (u8)animUpdatePeriod;

				// Determine a phase within the period.
				// TODO: Maybe do something here to pick the least used phase within each period.
				// For now we just mod with the index in the array, which at least helps to avoid
				// everybody updating in phase.
				const int phase = (i % animUpdatePeriod);
				Assert(phase <= 0xff);
				lod.m_AnimUpdatePhase = (u8)phase;
			}

			// Reset the default blocking flags.
			if( pPed->PopTypeIsMission() )
			{
				pPed->GetPedAiLod().SetBlockedLodFlags(CPedAILod::AL_DefaultMissionBlockedLodFlags);
			}
			else if( pPed->IsLawEnforcementPed() )
			{
				pPed->GetPedAiLod().SetBlockedLodFlags(CPedAILod::AL_DefaultActiveLawBlockedLodFlags);				
			}
			else
			{
				pPed->GetPedAiLod().SetBlockedLodFlags(CPedAILod::AL_DefaultRandomBlockedLodFlags);
			}

			//reset the forced lod flags
			lod.SetForcedLodFlags(0);
			lod.m_bForceNoTimesliceIntelligenceUpdate = false;
			lod.m_bUnconditionalTimesliceIntelligenceUpdate = false;
			lod.m_bTaskSetNeedIntelligenceUpdate = false;
			lod.m_NavmeshTrackerUpdate_OnScreenSkipCount = 0;
			lod.m_NavmeshTrackerUpdate_OffScreenSkipCount = 0;
		}
	}

	if(numInLowTimesliceLOD)
	{
		UpdateTimeslicing(numInLowTimesliceLOD);
	}

#if DEBUG_DRAW
	PrintStats();
#endif

	// Reset flags
	ms_UseActiveCameraForTimeslicingCentre = false;
}


bool	CPedAILodManager::ShouldDoFullUpdate(CPed& ped)
{
#if __BANK
	if(ms_TimesliceForcedPeriod > 0)
	{
		return (fwTimer::GetFrameCount() % ms_TimesliceForcedPeriod) == 0;
	}
#endif

	// First, if we are not in low LOD timeslicing mode, always return true.
	if(!ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
	{
		return true;
	}

	// If this ped is waiting for path, and it is ready - then do a full update
	if(ped.GetPedResetFlag(CPED_RESET_FLAG_WaitingForCompletedPathRequest))
	{
		return true;
	}

	// If we're forcing an instant ai update, always return true.
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate) ||
		ped.GetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate))
	{
		return true;
	}

	// Get the pointers to the first and last (inclusive) peds in the pool that should be updated.
	CPed* pFirst = ms_TimesliceFirstPed;
	CPed* pLast = ms_TimesliceLastPed;

	bool shouldUpdate = false;
	int deadPedPhase = ms_TimesliceDeadPedPhase;

	// Normally, pFirst would be less than pLast, but this may not be the case
	// if we are wrapping around.
	if(pFirst <= pLast)
	{
		// Check if between the first and the last, i.e. within the window of peds
		// that should be updated.
		if(&ped >= pFirst && &ped <= pLast)
		{
			shouldUpdate = true;
		}
	}
	else
	{
		// Check if this ped is after pFirst in the pool, or before pLast -
		// the window of peds that should be updated is wrapping around.
		if(&ped >= pFirst)
		{
			shouldUpdate = true;

			// In this case, we are actually at the end of the pool and have
			// technically not wrapped around yet, so subtract one from the phase.
			deadPedPhase--;
		}
		else if(&ped <= pLast)
		{
			shouldUpdate = true;
		}
	}

	// Give dead peds a chance to not update.
	if(shouldUpdate && ped.IsDead())
	{
		if(deadPedPhase < 0)	// Could happen since we subtracted one above in some cases.
		{
			deadPedPhase += ms_TimesliceUpdatePeriodMultiplierForDeadPeds;
		}

		// For dead peds, get the index from the pool, and see if that has the same parity as
		// the current timeslicing sweep.
		const int pedPhase = (CPed::GetPool()->GetJustIndexFast(&ped) % ms_TimesliceUpdatePeriodMultiplierForDeadPeds);
		if(pedPhase != deadPedPhase)
		{
			shouldUpdate = false;
		}
	}

	return shouldUpdate;
}


bool CPedAILodManager::ShouldDoNavMeshTrackerUpdate(CPed& ped)
{
	// If this feature is disabled
	if( !ms_bEnableNavMeshTrackerUpdateSkipping )
	{
		// update navmesh tracker
		return true;
	}

	// If the ped's reset flag is set to prevent skipping
	if( ped.GetPedResetFlag(CPED_RESET_FLAG_DoNotSkipNavMeshTrackerUpdate) )
	{
		// update navmesh tracker
		return true;
	}

	// Peds carrying portable pickups must always track the nav mesh as this is needed to keep track of whether the pickup is in an accessible location or not
	if( ped.GetPedConfigFlag(CPED_CONFIG_FLAG_HasPortablePickupAttached) )
	{
		return true;
	}

	// If we do not allow navmesh tracker update skip on full update,
	// and the ped is doing a full update
	if( !ms_bAllowNavMeshTrackerUpdateSkipOnFullUpdates && ShouldDoFullUpdate(ped) )
	{
		// update navmesh tracker
		return true;
	}

	// Otherwise, consider skipping a navmesh tracker update:

	// Get the ped's AI level of detail
	// (need non-const to update tracking variables)
	CPedAILod& aiLOD = ped.GetPedAiLod();

	// If ped is on-screen
	if( ped.GetIsVisibleInSomeViewportThisFrame() )
	{
		// Set the off-screen skip counter to its limit
		// This way we ensure a switch to being off-screen will update before skipping at all
		aiLOD.SetNavmeshTrackerUpdate_OffScreenSkipCount(ms_uMaxTrackerUpdateSkips_OffScreen);

		// If we have not yet reached our on-screen skip limit
		const u8 currentSkipCount = aiLOD.GetNavmeshTrackerUpdate_OnScreenSkipCount();
		if( currentSkipCount < ms_uMaxTrackerUpdateSkips_OnScreen )
		{
			// Increment the on-screen skip counter
			aiLOD.SetNavmeshTrackerUpdate_OnScreenSkipCount(currentSkipCount + 1);

			// skip this update
			return false;
		}
		// otherwise limit reached
		{
			// Reset the on-screen skip counter
			aiLOD.SetNavmeshTrackerUpdate_OnScreenSkipCount(0);

			// proceed with a normal update this frame.
			// intentionally fall through
		}
	}
	// If ped is off-screen
	else
	{
		// Set the on-screen skip counter to its limit
		// This way we ensure a switch to being on-screen will update before skipping at all
		aiLOD.SetNavmeshTrackerUpdate_OnScreenSkipCount(ms_uMaxTrackerUpdateSkips_OnScreen);

		// If we have not yet reached our off-screen skip limit
		const u8 currentSkipCount = aiLOD.GetNavmeshTrackerUpdate_OffScreenSkipCount();
		if( currentSkipCount < ms_uMaxTrackerUpdateSkips_OffScreen )
		{
			// Increment the off-screen skip counter
			aiLOD.SetNavmeshTrackerUpdate_OffScreenSkipCount(currentSkipCount + 1);

			// skip this update
			return false;
		}
		// otherwise limit reached
		{
			// Reset the off-screen skip counter
			aiLOD.SetNavmeshTrackerUpdate_OffScreenSkipCount(0);

			// proceed with a normal update this frame.
			// intentionally fall through
		}
	}

	// update navmesh tracker by default
	return true;
}


void CPedAILodManager::SetUseActiveCameraForTimeslicingCentre()
{
	ms_UseActiveCameraForTimeslicingCentre = true;
}


bool	CPedAILodManager::ClipToHiLodCount( bool inValue, int &count )
{
	if( ms_bPedAINLodActive )
	{
		// If we're Hi Lod
		if(inValue == true)
		{
			// Check the count of allowed HiLods
			if( count > 0 )
			{
				count--;
			}
			else
			{
				inValue = false;
			}
		}
	}
	return inValue;
}


void	CPedAILodManager::UpdateTimeslicing(int numInLowTimesliceLOD)
{
	// First, we get the index we stored off on the previous frame,
	// and keep a copy of it.
	int pedIndex = ms_TimesliceLastIndex;

	// Get the pool and its size, and set up some other variables we will use.
	CPed::Pool *pPool = CPed::GetPool();
	const int numSlots = pPool->GetSize();
	int numToUpdate = 0;
	int numConsidered = 0;
	// The >>1 here is somewhat arbitrarily dividing by two if we have only a few peds
	// in low LOD. For example, if 3 peds were in low LOD, maxToUpdate would be 1, and
	// each of them would get updates every three frames. When we have more peds in low LOD,
	// the number to update per frame will be ms_TimesliceMaxUpdatesPerFrame.
	const int maxToUpdate = Max(Min((int)ms_TimesliceMaxUpdatesPerFrame, numInLowTimesliceLOD/ms_TimesliceUpdatePeriodWithFewPeds), 1);
	CPed* pFirst = NULL;
	CPed* pLast = NULL;

	bool wrapped = false;

	// Loop over peds until we've found the desired amount, or wrapped around.
	while(1)
	{
		// Advance to the next slot, wrapping around if we reached the end of the pool.
		pedIndex++;
		if(pedIndex >= numSlots)
		{
			pedIndex = 0;
			wrapped = true;
		}

		// Get the ped if it exists, and check if it should be timesliced. If not, we just move on to the next.
		CPed* pPed = pPool->GetSlot(pedIndex);
		if(pPed && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
		{
			// Store the pointer to the first we found on this frame.
			if(!pFirst)
			{
				pFirst = pPed;
			}

			// Store the pointer to the last we have found so far.
			pLast = pPed;

			// Count them, and stop if we have found enough.
			numToUpdate++;
			if(numToUpdate >= maxToUpdate)
			{
				break;
			}
		}

		// Keep track of how many slots we have checked, and break if all slots have been
		// checked at least once, so we are back where we started.
		if(++numConsidered >= numSlots)
		{
			break;
		}
	}

	// If we wrapped around, starting a new sweep of the window of timesliced peds that get to
	// update, increase ms_TimesliceDeadPedPhase, to give other dead peds a chance to update
	// this time around.
	if(wrapped)
	{
		ms_TimesliceDeadPedPhase++;
		if(ms_TimesliceDeadPedPhase >= ms_TimesliceUpdatePeriodMultiplierForDeadPeds)
		{
			ms_TimesliceDeadPedPhase = 0;
		}
	}

	// Store the index we are at.
	Assert(pedIndex >= 0 && pedIndex < 0x8000);
	ms_TimesliceLastIndex = (s16)pedIndex;

	// Store the pointers to the peds. It would perhaps be more natural to store the indices,
	// but then we would have to go from pointer to index each time we need to check if a ped
	// should be updated, which would be likely to require a somewhat slow integer division.
	ms_TimesliceFirstPed = pFirst;
	ms_TimesliceLastPed = pLast;
}

void CPedAILodManager::UpdateAnimTimeslicing()
{
	// Accumulate the time steps of the frames, to be used for the next frame rate average.
	ms_AnimUpdateRebalanceFrameTimeSum += fwTimer::GetTimeStep();
	ms_AnimUpdateRebalanceFrameCount++;
	if(ms_AnimUpdateRebalanceFrameTimeSum < ms_AnimUpdateRebalancePeriod)
	{
		// Not enough time has passed yet for us to change anything.
		return;
	}

	// Compute the average frame time over the last accumulation period.
	float avgFrameTime = ms_AnimUpdateRebalanceFrameTimeSum/(float)ms_AnimUpdateRebalanceFrameCount;

	if(ms_AnimUpdateDisableFpsAdjustments)
	{
		avgFrameTime = 1.0f/ms_AnimUpdateDisabledFpsAdjustmentRate;
	}

#if __BANK
	ms_StatAnimUpdateAverageFrameTime = avgFrameTime;
#endif

	// Reset the frame time accumulation.
	ms_AnimUpdateRebalanceFrameTimeSum = 0.0f;
	ms_AnimUpdateRebalanceFrameCount = 0;


	// Determine the periods for the animation update levels, based on tuning and the average frame rate.
	// We also update ms_AnimUpdateCollapsedBaseDistances, which is the same as ms_AnimUpdateBaseDistances
	// except collapsed if multiple levels use the same period.
	int numLevels = 0;
	for(int i = 0; i < kMaxAnimUpdateFrequencyLevels; i++)
	{
		// Compute the period, in frames, that we would use at this frame rate, to get as close
		// to the desired update frequency (in Hz) as we can.
		const int period = Clamp((int)rage::round(1.0f/(avgFrameTime*ms_AnimUpdateFrequencyLevels[i])), 1, 0xff);

		// If this is the first level, or if the period at this level is different than the period of
		// the previous level, store the period and distance.
		if(!numLevels || ms_AnimUpdatePeriodsForLevels[numLevels - 1] != period)
		{
			ms_AnimUpdatePeriodsForLevels[numLevels] = (u8)period;
			ms_AnimUpdateCollapsedBaseDistances[numLevels] = ms_AnimUpdateBaseDistances[i];
			numLevels++;
		}
	}

	int numPeds = ms_pedAiPriorityArray.GetCount();

	// Store the number of update frequency levels we now have in ms_AnimUpdatePeriodsForLevels/ms_AnimUpdateCollapsedBaseDistances.
	ms_AnimUpdateNumFrequencyLevels = numLevels;

	// Check if the number of updates we would get if we updated everybody at level 0 is acceptable.
	// If so, we don't really have sort the array and call ComputeAnimTimeslicingDistances() to try
	// to find some distance thresholds, we can just set them all up at a single level right here.
	// Aside from doing less work, the primary reason is that we will more reliably let everybody update
	// - before, it was possible for somebody to move beyond the computed distance threshold and get
	// timesliced for a few frames before we had a chance to rebalance.
	if(numPeds <= ((int)ms_AnimUpdateDesiredMaxNumPerFrame)*ms_AnimUpdatePeriodsForLevels[0])
	{
		ms_AnimUpdateNumFrequencyLevels = 1;
#if __BANK
		Assert(ms_AnimUpdatePeriodsForLevels[0] > 0);
		ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = ((float)numPeds)/((float)ms_AnimUpdatePeriodsForLevels[0]);
#endif
		return;
	}

	// We don't need to sort here because none of the priorities have changed since the last sort
	//std::sort(&ms_pedAiPriorityArray[0], &ms_pedAiPriorityArray[0] + ms_pedAiPriorityArray.GetCount(), PedAiPriority::AiScoreLessThan);

	// Compute the score thresholds.
	ComputeAnimTimeslicingDistances();
}


void CPedAILodManager::ComputeAnimTimeslicingDistances()
{
	int numObj = ms_pedAiPriorityArray.GetCount();
	// Set up a temporary array for storing the index in the array of the first ped
	// at each frequency level.
	int indexOfFirstAtLevel[kMaxAnimUpdateFrequencyLevels];
	for(int i = 0; i < kMaxAnimUpdateFrequencyLevels; i++)
	{
		indexOfFirstAtLevel[i] = 0;
	}

	// The goal of the algorithm below is to find a volume of the "modifier" variable,
	// so that when we multiply the distance thresholds by it, we get a certain
	// number of updates per frame, on average. We will do this iteratively by increasing
	// the modifier until we would get too many updates.
	float modifier = 1.0f;
	const float modifierIncreaseFactor = 1.2f;	// How much to increase per iteration, lower would be more accurate, but more expensive to run the algorithm.
	const float desiredAvgAnimUpdatesPerFrame = ms_AnimUpdateDesiredMaxNumPerFrame;
	const int maxIter = 20;						// Mostly to guard against infinite loops if there are invalid numbers in the input. 1.2^20 = 38, so the modifier should be fairly large already at that point.

	const int numLevels = ms_AnimUpdateNumFrequencyLevels;
	if(numLevels > 1)
	{
		// Perform the iterations. We need to keep track of some values from the previous iteration:
		float prevModifier = modifier;
		float prevAvgAnimUpdatesDelta = -LARGE_FLOAT;
		for(int iter = 0; iter < maxIter; iter++)
		{
			// Update the indexOfFirstAtLevel[] array. Element 0 should always be 0, so we
			// start at 1. We just bump up the values from the previous iteration, since for
			// each iteration we can only increase the distance thresholds, so shouldn't be very
			// expensive.
			for(int level = 1; level < numLevels; level++)
			{
				// Get the current index, and the threshold distance if we were to use the current modifier.
				int index = indexOfFirstAtLevel[level];
				float threshold = square(ms_AnimUpdateCollapsedBaseDistances[level]*modifier);

				// Loop until we reach the end of the array, or until we've found a ped with
				// a worse score than the threshold for this level.
				while(index < numObj && ms_pedAiPriorityArray[index].m_fAnimScoreDistSq < threshold)
				{
					index++;
				}
				// Store it:
				indexOfFirstAtLevel[level] = index;

				// The first index for the next level will never be smaller than the first
				// index for the next level, so update the next level to reduce the work
				// we need to do.
				if(level < numLevels - 1)
				{
					const int nextLevel = level + 1;
					indexOfFirstAtLevel[nextLevel] = Max(indexOfFirstAtLevel[nextLevel], index);
				}
			}

			// Next, loop over the levels and total up the average number of updates per frame.
			float avgAnimUpdates = 0;
			for(int level = 0; level < numLevels; level++)
			{
				const int firstAtThisLevel = indexOfFirstAtLevel[level];
				const int firstAtNextLevel = (level < numLevels - 1) ? indexOfFirstAtLevel[level + 1] : numObj;
				const int numAtThisLevel = firstAtNextLevel - firstAtThisLevel;
				const float avgAnimUpdatesPerFrameAtThisLevel = (float)numAtThisLevel/(float)ms_AnimUpdatePeriodsForLevels[level];
				avgAnimUpdates += avgAnimUpdatesPerFrameAtThisLevel;
			}

			// Now, compute how far off we are relative to our target. If this value is greater than zero,
			// we can stop.
			const float avgAnimUpdatesDelta = avgAnimUpdates - desiredAvgAnimUpdatesPerFrame;
			if(avgAnimUpdatesDelta >= 0.0f)
			{
				// Compare the number of updates with this modifier, vs. the number of updates with the modifier
				// from the previous iteration of the algorithm. We will use the one that's closest to the target.
				Assert(prevAvgAnimUpdatesDelta < 0.0f);
				if(avgAnimUpdatesDelta < -prevAvgAnimUpdatesDelta)
				{
#if __BANK
					ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = avgAnimUpdates;
#endif
					// Too many updates per frame, but closer than the last we tested.
					// Keep the current modifier.
					break;
				}
				else
				{
#if __BANK
					ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = prevAvgAnimUpdatesDelta + desiredAvgAnimUpdatesPerFrame;
#endif
					// The last one we tried was a closer match.
					modifier = prevModifier;
					break;
				}
			}

			// Check if the first level is already past the end of the array. If so, there is no need to continue,
			// further increases to the modifier won't make any difference, and we are already below our target.
			Assert(numLevels >= 2);
			if(indexOfFirstAtLevel[1] >= numObj)
			{
#if __BANK
				ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = avgAnimUpdates;
#endif
				break;
			}

			// Try a larger modifier.
			prevModifier = modifier;
			modifier *= modifierIncreaseFactor;
		}
	}
	else
	{
		// In this case, there should be exactly one level, and everybody will update at the same period.
		Assert(numLevels == 1);
#if __BANK
		ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame = (float)numObj/(float)ms_AnimUpdatePeriodsForLevels[0];
#endif
	}

	// Apply the distance modifier to compute ms_AnimUpdateModifiedDistancesSquared.
	for(int level = 0; level < numLevels; level++)
	{
		Assert(level >= 0 && level < NELEM(ms_AnimUpdateModifiedDistancesSquared));
		ms_AnimUpdateModifiedDistancesSquared[level] = square(ms_AnimUpdateCollapsedBaseDistances[level]*modifier);
	}
}


void	CPedAILodManager::PrintStats()
{
#if DEBUG_DRAW
	if(ms_bDisplayDebugStats)
	{
		int	pedCount = 0;
		int	pedAILodInfoCounts[CPedAILod::kNumAiLodFlags];				// 1 for each AILod bit
		for(int i=0;i<CPedAILod::kNumAiLodFlags;i++)
			pedAILodInfoCounts[i] = 0;

		int pedAnimUpdates = 0;

		CPed::Pool& pedPool = *CPed::GetPool();
		CPed* pPed;
		int i=pedPool.GetSize();
		while(i--)
		{
			// For peds we need an array that equates to each AILod bit
			pPed = pedPool.GetSlot(i);
			if (pPed)
			{
				const CPedAILod& lod = pPed->GetPedAiLod();

				pedCount++;
				for(int count=0; count<CPedAILod::kNumAiLodFlags; count++)
				{
					if( lod.IsLodFlagSet( 1<<count ) )
						pedAILodInfoCounts[count]++;
				}

				if(lod.ShouldUpdateAnimsThisFrame())
				{
					pedAnimUpdates++;
				}
			}
		}

		grcDebugDraw::AddDebugOutput("PED N-LODDING INFO");
		grcDebugDraw::AddDebugOutput("Peds Total = %d", pedCount);
		grcDebugDraw::AddDebugOutput("PED AI LODDING");
		grcDebugDraw::AddDebugOutput("LodEventScanning Count = %d", pedAILodInfoCounts[0]);
		grcDebugDraw::AddDebugOutput("LodMotionTask Count = %d", pedAILodInfoCounts[1]);
		grcDebugDraw::AddDebugOutput("LodPhysics Count = %d", pedAILodInfoCounts[2]);
		grcDebugDraw::AddDebugOutput("LodNavigation Count = %d", pedAILodInfoCounts[3]);
		grcDebugDraw::AddDebugOutput("LodEntityScanning Count = %d", pedAILodInfoCounts[4]);
		grcDebugDraw::AddDebugOutput("LodRagdollInVehicle Count = %d", pedAILodInfoCounts[5]);
		grcDebugDraw::AddDebugOutput("LodTimesliceIntelligenceUpdate Count = %d", pedAILodInfoCounts[6]);
		grcDebugDraw::AddDebugOutput("LodTimesliceAnimUpdate Count = %d", pedAILodInfoCounts[7]);
		CompileTimeAssert(CPedAILod::kNumAiLodFlags == 8);	// Keep the stats above up to date if adding or removing flags.

		char buff[256];
		GetCenterInfoString(buff, sizeof(buff));

		grcDebugDraw::AddDebugOutput("PED ANIM LODDING");
		grcDebugDraw::AddDebugOutput("Anim Update Center: %s", buff);
		grcDebugDraw::AddDebugOutput("Anim Updates = %d", pedAnimUpdates);
		grcDebugDraw::AddDebugOutput("Current Theoretical Average Updates Per Frame = %.1f", ms_StatAnimUpdateTheoreticalCurrentAveragePerFrame);
		grcDebugDraw::AddDebugOutput("Avg Frame Rate = %.1f Hz", 1.0f/Max(ms_StatAnimUpdateAverageFrameTime, 0.0001f));
		for(int i = 0; i < ms_AnimUpdateNumFrequencyLevels; i++)
		{
			const int period = ms_AnimUpdatePeriodsForLevels[i];
			if(i < ms_AnimUpdateNumFrequencyLevels - 1)
			{
				formatf(buff, "%.1f..%.1f m distance update every %d frames.", sqrtf(ms_AnimUpdateModifiedDistancesSquared[i]), sqrtf(ms_AnimUpdateModifiedDistancesSquared[i + 1]), period);
			}
			else
			{
				formatf(buff, "%.1f+ m distance update every %d frames.", sqrtf(ms_AnimUpdateModifiedDistancesSquared[i]), period);
			}
			grcDebugDraw::AddDebugOutput("%s", buff);
		}
		grcDebugDraw::AddDebugOutput("FOV dist scale: %.1f", g_LodScale.GetGlobalScale());
	}
#endif	//DEBUG_DRAW

}



#if __BANK

void CPedAILodManager::AddDebugWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("Ped lodding");

			pBank->AddToggle("Lodding Active",					&CPedAILodManager::ms_bPedAILoddingActive);
			pBank->AddSlider("Physics distance",				&CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance, 0.0f, 9999.0f, 1.0f);
			pBank->AddSlider("Motion task distance",			&CPedAILodManager::ms_fMotionTaskPedLodDistance, 0.0f, 9999.0f, 1.0f);
			pBank->AddSlider("Motion task distance offscreen",	&CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen, 0.0f, 9999.0f, 1.0f);
			pBank->AddSlider("EventScanning distance",			&CPedAILodManager::ms_fEventScanningLodDistance, 0.0f, 9999.0f, 1.0f);
			pBank->AddSlider("Timeslice Intelligence Distance",	&CPedAILodManager::ms_fTimesliceIntelligenceDistance, 0.0f, 9999.0f, 1.0f);

			pBank->AddToggle("N-Lodding Active",				&CPedAILodManager::ms_bPedAINLodActive);
			pBank->AddSlider("N-Lod HiPhysicsAndEntityCount",	&CPedAILodManager::ms_hiLodPhysicsAndEntityCount, 0, 2000, 1);
			pBank->AddSlider("N-Lod HiMotionTaskCount",			&CPedAILodManager::ms_hiLodMotionTaskCount, 0, 2000, 1);
			pBank->AddSlider("N-Lod HiEventScanningCount",		&CPedAILodManager::ms_hiLodEventScanningCount, 0, 2000, 1);
			pBank->AddSlider("N-Lod HiTimesliceIntelligenceCount", &CPedAILodManager::ms_hiLodTimesliceIntelligenceCount, 0, 2000, 1);
			pBank->AddSlider("Extra Hi On Screen Count",		&CPedAILodManager::ms_iExtraHiLodOnScreenCount, 0, 255, 1);
			pBank->AddSlider("Extra Hi On Screen Max Dist",		&CPedAILodManager::ms_iExtraHiLodOnScreenMaxDist, 0.0f, 9999.0f, 1.0f);
			pBank->AddSlider("Extra Hi On Screen Max Peds Total", &CPedAILodManager::ms_iExtraHiLodOnScreenMaxPedsTotal, 0, 255, 1);

			pBank->AddSlider("N-Lod Not Visible Score Modifier",	&CPedAILodManager::ms_NLodScoreModifierNotVisible, 0.0f, 1000.0f, 0.1f);
			pBank->AddSlider("N-Lod Using Scenario Score Modifier",	&CPedAILodManager::ms_NLodScoreModifierUsingScenario, 0.0f, 1000.0f, 0.1f);
			pBank->AddToggle("N-Lod Display Scores",			&CPedAILodManager::ms_bDisplayNLodScores);


			pBank->AddToggle("Update Ped Visibility",			&CPedAILodManager::ms_bUpdateVisibility);
			
			pBank->AddSlider("Ped AI Lod Batch Frames" , &CPedAILodManager::ms_pedAiPriorityBatchFrames, 0, 120, 1);

#if DEBUG_DRAW
			pBank->AddToggle("Display Stats",			&CPedAILodManager::ms_bDisplayDebugStats);
#endif	// __DEBUG_DRAW

			pBank->AddToggle("CPed::ProcessPedStanding() Single Step Active", &ms_bPedProcessPedStandingSingleStepActive);
			pBank->AddToggle("CPed::ProcessPhysics() Single Step Active", &ms_bPedProcessPhysicsSingleStepActive);
			pBank->AddToggle("CPed::SimulatePhysicsForLowLod() Allow Skip Position Updates", &ms_bPedSimulatePhysicsForLowLodSkipPosUpdates);
			pBank->AddToggle("AI Timeslicing Active",				&CPedAILodManager::ms_bPedAITimesliceLodActive);
			pBank->AddSlider("AI Timeslicing Max Updates Per Frame",	&CPedAILodManager::ms_TimesliceMaxUpdatesPerFrame, 1, 100, 1);
			pBank->AddSlider("AI Timeslicing Update Period With Few Peds",	&CPedAILodManager::ms_TimesliceUpdatePeriodWithFewPeds, 1, 32, 1);
			pBank->AddSlider("AI Timeslicing Update Period Multiplier For Dead Peds", &CPedAILodManager::ms_TimesliceUpdatePeriodMultiplierForDeadPeds, 1, 32, 1);
			pBank->AddToggle("AI Timeslicing Display Updates (Lines)",		&CPedAILodManager::ms_bDisplayFullUpdatesLines);
			pBank->AddToggle("AI Timeslicing Display Updates (Spheres)",	&CPedAILodManager::ms_bDisplayFullUpdatesSpheres);
			pBank->AddSlider("AI Timeslicing Forced Period",				&CPedAILodManager::ms_TimesliceForcedPeriod, 0, 255, 1);
			pBank->AddSlider("AI Timeslicing Aggressive Scenario Ped Limit", &CPedAILodManager::ms_iTimesliceScenariosAggressivelyLimit, 0, 1000, 1);

			pBank->AddToggle("Anim Timeslicing Active",						&CPedAILodManager::ms_bPedAnimTimesliceLodActive);
			pBank->AddToggle("Anim Timeslicing Display Updates (Lines)",	&CPedAILodManager::ms_bDisplayFullAnimUpdatesLines);
			pBank->AddToggle("Anim Timeslicing Display Updates (Spheres)",	&CPedAILodManager::ms_bDisplayFullAnimUpdatesSpheres);
			pBank->AddToggle("Anim Timeslicing Display Scores",				&CPedAILodManager::ms_bDisplayAnimUpdateScores);

			pBank->AddSlider("Anim Timeslicing Force Period for Debugging", &ms_DebugAnimUpdateForcedPeriod, 0, 100, 1);
			pBank->AddToggle("Anim Timeslicing Disable FPS Adjustments",	&ms_AnimUpdateDisableFpsAdjustments);

			pBank->PushGroup("Anim Timeslicing Tuning");

			pBank->AddSlider("Modifier Not Visible",						&CPedAILodManager::ms_AnimUpdateScoreModifierNotVisible, 0.0f, 1000.0f, 0.1f);
			pBank->AddSlider("Modifier In Scenario or Using Scenario",		&CPedAILodManager::ms_AnimUpdateScoreModifierInScenarioOrVehicle, 0.0f, 1000.0f, 0.1f);

			for(int i = 0; i < kMaxAnimUpdateFrequencyLevels - 1; i++)
			{
				char buff[256];
				formatf(buff, "Distance Threshold %d-%d", i, i + 1);
				pBank->AddSlider(buff, &ms_AnimUpdateBaseDistances[i + 1], 1.0f, 10000.0f, 1.0f);
				formatf(buff, "Frequency %d", i + 1);
				pBank->AddSlider(buff, &ms_AnimUpdateFrequencyLevels[i + 1], 1.0f, 100.0f, 1.0f);
			}

			pBank->AddSlider("Anim Timeslicing Desired Max Updates Per Frame", &ms_AnimUpdateDesiredMaxNumPerFrame, 1.0f, 500.0f, 1.0f);

			pBank->AddSlider("FPS with Adjustments Disabled", &ms_AnimUpdateDisabledFpsAdjustmentRate, 1.0f, 120.0f, 1.0f);

			pBank->AddSlider("CPed::SimulatePhysicsForLowLod() Visible Skip Position Updates Dist", &ms_SimPhysForLowLodSkipPosUpdatesDist, 0.0f, 500.0f, 1.0f);

			pBank->PopGroup();	// "Anim Timeslicing Tuning"

			pBank->PushGroup("NavMeshTracker");

			pBank->AddToggle( "Enable NavMeshTracker update skipping", &ms_bEnableNavMeshTrackerUpdateSkipping);
			pBank->AddToggle( "Allow skips on full updates", &ms_bAllowNavMeshTrackerUpdateSkipOnFullUpdates);
			pBank->AddSlider( "NavmeshTracker update skips (onscreen)", &ms_uMaxTrackerUpdateSkips_OnScreen, 0, CPedAILod::kMaxSkipCount, 1 );
			pBank->AddSlider( "NavmeshTracker update skips (offscreen)", &ms_uMaxTrackerUpdateSkips_OffScreen, 0, CPedAILod::kMaxSkipCount, 1 );
			
			pBank->PopGroup(); // "NavMeshTracker"

			CPedAILod::AddDebugWidgets(*pBank);

		pBank->PopGroup();
	}
}

void CPedAILodManager::DrawFullUpdate(CPed& ped)
{
	if(ms_bDisplayFullUpdatesLines)
	{
		const Vec3V playerCoordsV = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
		grcDebugDraw::Line(playerCoordsV, ped.GetTransform().GetPosition(), Color_white, -1);
	}
	if(ms_bDisplayFullUpdatesSpheres)
	{
		grcDebugDraw::Sphere(ped.GetTransform().GetPosition(), 1.5f, Color_white, false, -1);
	}
}

void CPedAILodManager::DrawFullAnimUpdate(CPed& ped)
{
	if(ms_bDisplayFullAnimUpdatesLines)
	{
		const Vec3V playerCoordsV = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
		grcDebugDraw::Line(playerCoordsV, ped.GetTransform().GetPosition(), Color_purple, -1);
	}
	if(ms_bDisplayFullAnimUpdatesSpheres)
	{
		grcDebugDraw::Sphere(ped.GetTransform().GetPosition(), 1.4f, Color_purple, false, -1);
	}
}

#endif	// __BANK

int CPedAILodManager::FindPedsByPriority()
{
	CompileTimeAssert((kMaxPeds & 3) == 0);
	FastAssert(kMaxPeds >= CPed::GetPool()->GetSize());


#if __XENON
	// Workaround for what appears to be a 360 compiler bug - use one large array and a pointer
	// halfway into it, rather than two smaller arrays. Otherwise, apparently the two arrays
	// end up at the same address on the stack frame and the modified distances (scores) get
	// used as the actual distances too.
	Vec4V distSqArray[kMaxPeds/4*3];
	Vec4V* aiScoreArray = &distSqArray[kMaxPeds/4];
	Vec4V* animScoreArray = &distSqArray[kMaxPeds/4*2];

	//
	// Note: the compiler error seems to be present even if FindPedsByPriority() is simplified into
	// a small standalone function using float arrays:
	//	int FindPedsByPriority(CPedAILodManager::PedAiPriority*)
	//	{
	//		float distSqArray[256];
	//		float scoreArray[256];
	//		CPedAILodManager::ComputePedScores((Vec4V*)distSqArray, (Vec4V*)scoreArray, 64);
	//		return 0;
	//	}
	// However, it appears that the contents of ComputePedScores() is relevant, because if I just
	// comment out the definition of that function further below, the compiled output seems
	// to be correct again.
#else
	// Set up temporary arrays to hold the squared distances and scores of all peds.
	Vec4V distSqArray[kMaxPeds/4];
	Vec4V aiScoreArray[kMaxPeds/4];
	Vec4V animScoreArray[kMaxPeds/4];
#endif

	// Compute the scores and squared distances of all peds, using the spatial array. Scores
	// will be computed both for use with AI timeslicing and animation timeslicing.
	ComputePedScores(distSqArray, aiScoreArray, animScoreArray, kMaxPeds/4);

	// Loop over all the peds in the spatial array, and build the PedAiPriority array.
	const CSpatialArray& spatialArray = *CPed::ms_spatialArray;
	const int numObj = spatialArray.GetNumObjects();
	const float* pDistSqArrayFloat = (const float*)distSqArray;
	const float* pAiScoreDistSqArrayFloat = (const float*)aiScoreArray;
	const float* pAnimScoreDistSqArrayFloat = (const float*)animScoreArray;

	// direct array access here to reduce atArray overhead
	ms_pedAiPriorityArray.Resize(numObj);
	PedAiPriority* pIntoArray = ms_pedAiPriorityArray.GetElements();
	for(int i = 0; i < numObj; i++)
	{
		
		pIntoArray[i].m_fDistSq = pDistSqArrayFloat[i];
		pIntoArray[i].m_fAiScoreDistSq = pAiScoreDistSqArrayFloat[i];
		pIntoArray[i].m_fAnimScoreDistSq = pAnimScoreDistSqArrayFloat[i];

		CPed* pPed = CPed::GetPedFromSpatialArrayNode(spatialArray.GetNodePointer(i));
		pIntoArray[i].m_pPed = pPed;
	}

	return numObj;
}

#if __BANK

void CPedAILodManager::GetCenterInfoString(char* buffOut, int maxSz)
{
	const Vec3V popCenterV = FindPedScoreCenter();

	// Come up with a string describing where the position came from, basically needs to match
	// the logic in FindPedScoreCenter() + CFocusEntityMgr::GetPopPos() to be accurate.
	const char* pNameOfPosSource = "?";
	const CFocusEntityMgr& mgr = CFocusEntityMgr::GetMgr();
	if(mgr.HasOverridenPop())
	{
		pNameOfPosSource = "FocusEntityMgr overriden pop";
	}
	else
	{
		switch(mgr.GetFocusState())
		{
			case CFocusEntityMgr::FOCUS_DEFAULT_PLAYERPED:
				pNameOfPosSource = "FocusEntityMgr DEFAULT_PLAYERPED";
				break;
			case CFocusEntityMgr::FOCUS_OVERRIDE_ENTITY:
				pNameOfPosSource = "FocusEntityMgr FOCUS_OVERRIDE_ENTITY";
				break;
			case CFocusEntityMgr::FOCUS_OVERRIDE_POS:
				pNameOfPosSource = "FocusEntityMgr FOCUS_OVERRIDE_POS";
				break;
			default:
				pNameOfPosSource = "FocusEntityMgr ???";
				break;
		}
	}
	if(camInterface::GetScriptDirector().IsRendering())
	{
		pNameOfPosSource = "ScriptDirector";
	}
	if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode())
	{
		pNameOfPosSource = "Spectator";
	}

	formatf(buffOut, maxSz, "%.1f, %.1f, %.1f (%s)", popCenterV.GetXf(), popCenterV.GetYf(), popCenterV.GetZf(), pNameOfPosSource);
}


void CPedAILodManager::DebugDrawFindPedsByPriority()
{
#if DEBUG_DRAW
	const int numObj = ms_pedAiPriorityArray.GetCount();

	char popCenterInfo[256];
	GetCenterInfoString(popCenterInfo, sizeof(popCenterInfo));

	const bool displayCenterInfo = !(grcDebugDraw::GetDisplayDebugText() && ms_bDisplayDebugStats);

	if(ms_bDisplayNLodScores)
	{
		// Display the position and its source at a fixed position on screen.
		if(displayCenterInfo)
		{
			grcDebugDraw::Text(Vec2V(0.1f, 0.15f), Color_pink, popCenterInfo);
		}

		// Draw the scores for all the peds.
		for(int i = 0; i < numObj; i++)
		{
			char buff[256];
			const CPed* pPed = ms_pedAiPriorityArray[i].m_pPed;
			if(pPed && CPed::GetPool()->IsValidPtr(const_cast<CPed*>(pPed)))
			{
				formatf(buff, "%.1f / %.1f", sqrtf(ms_pedAiPriorityArray[i].m_fAiScoreDistSq), sqrtf(ms_pedAiPriorityArray[i].m_fDistSq));
				grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_pink, 0, 0, buff);
			}
		}
	}

	if(ms_bDisplayAnimUpdateScores)
	{
		// Display the position and its source at a fixed position on screen.
		if(displayCenterInfo)
		{
			grcDebugDraw::Text(Vec2V(0.1f, 0.1f), Color_purple, popCenterInfo);
		}

		// Draw the scores for all the peds.
		for(int i = 0; i < numObj; i++)
		{
			const CPed* pPed = ms_pedAiPriorityArray[i].m_pPed;
			if(pPed && CPed::GetPool()->IsValidPtr(const_cast<CPed*>(pPed)))
			{
				char buff[256];
				formatf(buff, "%.1f / %.1f", sqrtf(ms_pedAiPriorityArray[i].m_fAnimScoreDistSq), sqrtf(ms_pedAiPriorityArray[i].m_fDistSq));
				grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_purple, 0, 0, buff);
			}
		}
	}

#endif	// DEBUG_DRAW
}
#endif	// __BANK

void CPedAILodManager::UpdatePedVisibility()
{
	if(!ms_bUpdateVisibility)
	{
		return;
	}

	// Loop over all the peds in the pool.
	CPed::Pool* pPool = CPed::GetPool();
	const int maxPeds = pPool->GetSize();
	for(int i = 0; i < maxPeds; i++)
	{
		CPed* pPed = pPool->GetSlot(i);	
		UpdatePedVisibility(pPed);
	}
}

void CPedAILodManager::UpdatePedVisibility(CPed* pPed)
{	
	if(pPed && ms_bUpdateVisibility)
	{
		bool visible = false;

		// GetIsVisibleInSomeViewportThisFrame() handles checking for occlusions, so it's the only check we need to do
		if(pPed->GetIsVisibleInSomeViewportThisFrame())
		{
			visible = true;
		}

		// Check to see if the visibility value actually changed. If so, update
		// the config flag and the spatial array flag.
		bool wasVisible = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen);
		if(wasVisible != visible)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen, visible);
			pPed->UpdateSpatialArrayTypeFlags();

			if(visible)
				ms_bForcePedAiPriorityUpdate = true; // ped just became visible, force an update this frame
		}
	}
}


void CPedAILodManager::ComputePedScores(Vec4V* pDistSqArrayOut, Vec4V* pAiScoreDistSqArrayOut, Vec4V* pAnimScoreDistSqArrayOut, int ASSERT_ONLY(maxVecs))
{
	// As you zoom in on somebody (with a sniper rifle, etc), they become more visible, and you would
	// want them to be less likely to skip animation updates. The normal limits we have (saying for example
	// that somebody needs to be at the very least 10 m away to update at ~14 Hz) essentially need to
	// be adjusted. Rather than adjusting those limits, which would also affect off-screen peds, we divide
	// the scores of all the visible peds by g_LodScale.GetGlobalScale(). This will get multiplied in
	// with animBaseScore, and we will divide the off-screen scoring factor so that it makes no difference
	// for them.
	const float scoreExtraFactorForVisible = 1.0f/Max(1.0f, g_LodScale.GetGlobalScale());

	const float aiBaseScore = scoreExtraFactorForVisible;

	// Set up scoring based on scenario usage.
	const u32 aiFlag1ToScore = CPed::kSpatialArrayTypeFlagUsingScenario;
	const u32 aiFlag1Value = CPed::kSpatialArrayTypeFlagUsingScenario;
	const float aiFlag1Modifier = ms_NLodScoreModifierUsingScenario;

	// Set up scoring based on visibility.
	const u32 aiFlag2ToScore = CPed::kSpatialArrayTypeFlagVisibleOnScreen;
	const u32 aiFlag2Value = 0;	// Apply the modifier if the flag is zero, i.e. the ped is not visible.
	const float aiFlag2Modifier = ms_NLodScoreModifierNotVisible/scoreExtraFactorForVisible;;

	// For animation updates, we want to multiply by ms_AnimUpdateScoreModifierInScenarioOrVehicle if
	// either the kSpatialArrayTypeFlagInVehicle flag is set, or kSpatialArrayTypeFlagUsingScenario.
	// That's equivalent to say that we want to divide by ms_AnimUpdateScoreModifierInScenarioOrVehicle if
	// both of those flags are zero, if we also multiply by ms_AnimUpdateScoreModifierInScenarioOrVehicle.
	const float animBaseScore = ms_AnimUpdateScoreModifierInScenarioOrVehicle*scoreExtraFactorForVisible;
	const u32 animFlag1ToScore = CPed::kSpatialArrayTypeFlagInVehicle | CPed::kSpatialArrayTypeFlagUsingScenario;
	const u32 animFlag1Value = 0;
	const float animFlag1Modifier = 1.0f/ms_AnimUpdateScoreModifierInScenarioOrVehicle;

	// Set up animation update scoring based on visibility.
	const u32 animFlag2ToScore = CPed::kSpatialArrayTypeFlagVisibleOnScreen;
	const u32 animFlag2Value = 0;	// Apply the modifier if the flag is zero, i.e. the ped is not visible.
	const float animFlag2Modifier = ms_AnimUpdateScoreModifierNotVisible/scoreExtraFactorForVisible;

	const Vec3V popCenterV = FindPedScoreCenter();

	const CSpatialArray& spatialArray = *CPed::ms_spatialArray;

	// Set up array pointers.
	const Vec4V* RESTRICT objXPtr = (const Vec4V*)spatialArray.GetPosXArray();
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)spatialArray.GetPosYArray();
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)spatialArray.GetPosZArray();
	const Vec4V* RESTRICT objTypeFlagsPtr = (const Vec4V*)spatialArray.GetTypeFlagArray();
	Vec4V* RESTRICT distSqOutPtr = pDistSqArrayOut;
	Vec4V* RESTRICT aiScoreDistSqOutPtr = pAiScoreDistSqArrayOut;
	Vec4V* RESTRICT animScoreDistSqOutPtr = pAnimScoreDistSqArrayOut;

	const int numObj = spatialArray.GetNumObjects();
	Assert(maxVecs >= (numObj + 3)/4);

	// Splat the position into three separate vectors so we can operate on SoA data.
	const Vec4V posXV(popCenterV.GetX());
	const Vec4V posYV(popCenterV.GetY());
	const Vec4V posZV(popCenterV.GetZ());

	const Vec4V oneV(V_ONE);

	// Get the base modifier, i.e. what we will multiply the distance to get a score before
	// applying the conditional modifiers.
	const Vec4V aiBaseScoreModifierV(LoadScalar32IntoScalarV(aiBaseScore));
	const Vec4V aiBaseScoreModifierSqV = Scale(aiBaseScoreModifierV, aiBaseScoreModifierV);

	// Set up vectors for scoring based on the value of flag 1.
	const Vec4V aiTypeFlag1ToScoreV(LoadScalar32IntoScalarV(aiFlag1ToScore));
	const Vec4V aiTypeFlag1ValueV(LoadScalar32IntoScalarV(aiFlag1Value));
	const Vec4V aiTypeFlag1ModifierV(LoadScalar32IntoScalarV(aiFlag1Modifier));

	// We can apply the base modifier more or less for free while we apply
	// flag 1's conditional modifier, by multiplying flag 1's modifier by the base
	// and use that if the conditions match, or select the base modifier if they don't.
	const Vec4V aiTypeFlag1ModifierTimesBaseV = Scale(aiTypeFlag1ModifierV, aiBaseScoreModifierV);
	const Vec4V aiTypeFlag1ModifierTimesBaseSqV = Scale(aiTypeFlag1ModifierTimesBaseV, aiTypeFlag1ModifierTimesBaseV);

	// Set up vectors for scoring based on the value of flag 2.
	const Vec4V aiTypeFlag2ToScoreV(LoadScalar32IntoScalarV(aiFlag2ToScore));
	const Vec4V aiTypeFlag2ValueV(LoadScalar32IntoScalarV(aiFlag2Value));
	const Vec4V aiTypeFlag2ModifierV(LoadScalar32IntoScalarV(aiFlag2Modifier));
	const Vec4V aiTypeFlag2ModifierSqV = Scale(aiTypeFlag2ModifierV, aiTypeFlag2ModifierV);

	// Do the same for animation scoring.
	const Vec4V animBaseScoreModifierV(LoadScalar32IntoScalarV(animBaseScore));
	const Vec4V animBaseScoreModifierSqV = Scale(animBaseScoreModifierV, animBaseScoreModifierV);
	const Vec4V animTypeFlag1ToScoreV(LoadScalar32IntoScalarV(animFlag1ToScore));
	const Vec4V animTypeFlag1ValueV(LoadScalar32IntoScalarV(animFlag1Value));
	const Vec4V animTypeFlag1ModifierV(LoadScalar32IntoScalarV(animFlag1Modifier));
	const Vec4V animTypeFlag1ModifierTimesBaseV = Scale(animTypeFlag1ModifierV, animBaseScoreModifierV);
	const Vec4V animTypeFlag1ModifierTimesBaseSqV = Scale(animTypeFlag1ModifierTimesBaseV, animTypeFlag1ModifierTimesBaseV);
	const Vec4V animTypeFlag2ToScoreV(LoadScalar32IntoScalarV(animFlag2ToScore));
	const Vec4V animTypeFlag2ValueV(LoadScalar32IntoScalarV(animFlag2Value));
	const Vec4V animTypeFlag2ModifierV(LoadScalar32IntoScalarV(animFlag2Modifier));
	const Vec4V animTypeFlag2ModifierSqV = Scale(animTypeFlag2ModifierV, animTypeFlag2ModifierV);

	// Loop over the spatial array data.
	for(int i = 0; i < numObj; i += 4)
	{
		// Load from the arrays to the vector registers.
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;
		const Vec4V typeFlagsV = *objTypeFlagsPtr;

		// Compute the squared distance to the point.
		const Vec4V dxV = Subtract(xxV, posXV);
		const Vec4V dyV = Subtract(yyV, posYV);
		const Vec4V dzV = Subtract(zzV, posZV);
		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V dxy2V = AddScaled(dx2V, dyV, dyV);
		const Vec4V d2V = AddScaled(dxy2V, dzV, dzV);

		// Compute the AI score modifier for the first flag (scenario usage).
		const Vec4V aiTypeFlag1SetV = And(typeFlagsV, aiTypeFlag1ToScoreV);
		const VecBoolV aiTypeFlag1ResultV = IsEqualInt(aiTypeFlag1SetV, aiTypeFlag1ValueV);
		const Vec4V aiTypeFlag1ScoreV = SelectFT(aiTypeFlag1ResultV, aiBaseScoreModifierSqV, aiTypeFlag1ModifierTimesBaseSqV);

		// Compute the AI score modifier for the second flag (visibility).
		const Vec4V aiTypeFlag2SetV = And(typeFlagsV, aiTypeFlag2ToScoreV);
		const VecBoolV aiTypeFlag2ResultV = IsEqualInt(aiTypeFlag2SetV, aiTypeFlag2ValueV);
		const Vec4V aiTypeFlag2ScoreV = SelectFT(aiTypeFlag2ResultV, oneV, aiTypeFlag2ModifierSqV);

		// Multiply the squared distances by the modifiers.
		const Vec4V aiScore1V = Scale(d2V, aiTypeFlag1ScoreV);
		const Vec4V aiScore2V = Scale(aiScore1V, aiTypeFlag2ScoreV);

		// Now, do the same for scoring for animation updates.
		const Vec4V animTypeFlag1SetV = And(typeFlagsV, animTypeFlag1ToScoreV);
		const VecBoolV animTypeFlag1ResultV = IsEqualInt(animTypeFlag1SetV, animTypeFlag1ValueV);
		const Vec4V animTypeFlag1ScoreV = SelectFT(animTypeFlag1ResultV, animBaseScoreModifierSqV, animTypeFlag1ModifierTimesBaseSqV);
		const Vec4V animTypeFlag2SetV = And(typeFlagsV, animTypeFlag2ToScoreV);
		const VecBoolV animTypeFlag2ResultV = IsEqualInt(animTypeFlag2SetV, animTypeFlag2ValueV);
		const Vec4V animTypeFlag2ScoreV = SelectFT(animTypeFlag2ResultV, oneV, animTypeFlag2ModifierSqV);
		const Vec4V animScore1V = Scale(d2V, animTypeFlag1ScoreV);
		const Vec4V animScore2V = Scale(animScore1V, animTypeFlag2ScoreV);

		// Store the results.
		*distSqOutPtr = d2V;
		*aiScoreDistSqOutPtr = aiScore2V;
		*animScoreDistSqOutPtr = animScore2V;

		// Move on in the arrays.
		objXPtr++;
		objYPtr++;
		objZPtr++;
		objTypeFlagsPtr++;
		distSqOutPtr++;
		aiScoreDistSqOutPtr++;
		animScoreDistSqOutPtr++;
	}
}


Vec3V_Out CPedAILodManager::FindPedScoreCenter()
{
	Vec3V popCenterV = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPopPos());

	// If a script camera is active, we should probably use the position of that for scoring
	// LOD. At least, that's what we would want for animation timeslicing, and we don't currently
	// have a good way to do anything different for other LOD aspects.
	// See B* 975823: "Cameraman's reaction to Steve getting shot plays very jerkily".
	if(camInterface::GetScriptDirector().IsRendering())
	{
		popCenterV = RCC_VEC3V(camInterface::GetScriptDirector().GetFrame().GetPosition());
	}

	// If spectating in MP, make sure that we use the camera position.
	// B* 1055211: "During mission 'Never Nude!', player was a spectator observing enemy remote players and character animation was choppy/poor"
	// B* 1634092: "If the player we are spectating in freemode is some distance away their animations often update at a noticeably slow rate"
	if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode())
	{
		popCenterV = RCC_VEC3V(camInterface::GetFrame().GetPosition());
	}

	// Use the active camera rather than population centre
	// B* 3650494 - Mobile Operations Centre - Players experiences stuttering walking/running animation
	if (ms_UseActiveCameraForTimeslicingCentre)
	{
		popCenterV = RCC_VEC3V(camInterface::GetFrame().GetPosition());
	}

	return popCenterV;
}
