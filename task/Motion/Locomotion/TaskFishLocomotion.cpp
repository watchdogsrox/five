#include "TaskFishLocomotion.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"


#include "Debug/DebugScene.h"
#include "animation/MovePed.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Peds/PedIntelligence.h"
#include "Physics/WaterTestHelper.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/System/TaskHelpers.h"
#include "Camera/Gameplay/GameplayDirector.h"


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	FishLocomotion
//////////////////////////////////////////////////////////////////////////

// Instance of tunable task params
CTaskFishLocomotion::Tunables CTaskFishLocomotion::sm_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskFishLocomotion, 0xbb80fa72);

// States
const fwMvStateId CTaskFishLocomotion::ms_IdleStateId("Idle",0x71C21326);
const fwMvStateId CTaskFishLocomotion::ms_NoClipStateId("NoClip",0x23B4087C);

const fwMvRequestId CTaskFishLocomotion::ms_startIdleId("StartIdle",0x2A8F6981);
const fwMvBooleanId CTaskFishLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);

const fwMvRequestId CTaskFishLocomotion::ms_startTurnInPlaceId("StartTurnInPlace",0x2E87794F);
const fwMvBooleanId CTaskFishLocomotion::ms_OnEnterTurnInPlaceId("OnEnterTurnInPlace",0xA7F587A6);

const fwMvRequestId CTaskFishLocomotion::ms_startSwimId("StartSwim",0x4DBEBDB7);
const fwMvBooleanId CTaskFishLocomotion::ms_OnEnterSwimId("OnEnterSwim",0x9FF7616E);

const fwMvFloatId CTaskFishLocomotion::ms_DirectionId("Direction",0x34DF9828);	
const fwMvFloatId CTaskFishLocomotion::ms_MoveDesiredSpeedId("DesiredSpeed",0xCF7BA842);
const fwMvFloatId CTaskFishLocomotion::ms_MoveRateId("MoveRate", 0x24535f0e);

const fwMvRequestId CTaskFishLocomotion::ms_StartNoClipId("StartNoClip", 0xBC7581E7);
const fwMvBooleanId CTaskFishLocomotion::ms_OnEnterNoClipId("OnEnterNoClip", 0x4F96D612);

const fwMvFloatId CTaskFishLocomotion::ms_StateTransitionDurationId("StateTransitionDuration", 0xBF6C59AC);

dev_float CTaskFishLocomotion::ms_fBigFishMaxPitch = EIGHTH_PI;
dev_float CTaskFishLocomotion::ms_fMaxPitch = QUARTER_PI;
dev_float CTaskFishLocomotion::ms_fFishAccelRate = 2.0f;
dev_float CTaskFishLocomotion::ms_fBigFishAccelRate = 0.5f;
dev_float CTaskFishLocomotion::ms_fBigFishDecelRate = 0.25f;

BANK_ONLY(bool CTaskFishLocomotion::sm_bRenderSurfaceChecks = false;)

CTaskFishLocomotion::CTaskFishLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
, m_fTurnPhase(0.0f)
, m_fPreviousTurnPhaseGoal(0.0f)
, m_fTurnApproachRate(0.0f)
, m_fPitchRate(0.0f)
, m_fStoredDesiredPitch(0.0f)
, m_fTimeSpentOutOfWater(0.0f)
, m_bOutOfWater(false)
, m_bSwimNearSurface(false)
, m_bSwimNearSurfaceAllowDeeperThanWaves(false)
, m_bSurviveBeingOutOfWater(false)
, m_bForceFastPitching(false)
, m_bMoveInTargetState(false)
, m_bUseGaitlessRateBoost(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_FISH);
}

//////////////////////////////////////////////////////////////////////////

CTaskFishLocomotion::~CTaskFishLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

// Cache off the water depth
CTask::FSM_Return	CTaskFishLocomotion::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE))
	{
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodMotionTask);
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodNavigation);
	}
	else
	{
		if (!m_bSwimNearSurface && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask))
		{
			AllowTimeslicing();
		}
	}

	if (GetState() < State_Flop)
	{
		// Always check if the fish is in water, even if it has no navmesh tracker or is in low lod.
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceLowLodWaterCheck, true);

		// Don't let fish get pushed around by water forces when swimming.
		pPed->m_Buoyancy.m_fForceMult = 0.0f;

		bool bTurnOnExtractedZ = true;
		if (pPed->m_Buoyancy.GetSubmergedLevel() <= 0.99f)
		{
			bTurnOnExtractedZ = false;
		}

		if (bTurnOnExtractedZ)
		{
			// LOD Changes turn use extracted Z back off, fish need this on to stay swimming around.
			if (!pPed->GetUseExtractedZ())
			{
				pPed->SetUseExtractedZ(true);
			}
		}
		else
		{
			pPed->SetUseExtractedZ(false);
		}
	}
	else
	{
		// Turn back on gravity once dead so the fish will sink.
		if (pPed->GetUseExtractedZ())
		{
			pPed->SetUseExtractedZ(false);
		}

		// Once flopping, go ahead and let them get pushed around by water forces.
		pPed->m_Buoyancy.m_fForceMult = 1.0f;
	}

	float fDesiredMoveRateOverride = pPed->GetMotionData()->GetDesiredMoveRateOverride();

	if (GetUseGaitlessRateBoostThisFrame())
	{
		fDesiredMoveRateOverride += sm_Tunables.m_GaitlessRateBoost;
	}

	// Lerp towards the desired move rate override set by script - use the same rate here as human peds.
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplier, 2.0, 1.0f, 20.0f, 0.5f);
	float fDeltaOverride = fDesiredMoveRateOverride - pPed->GetMotionData()->GetCurrentMoveRateOverride();
	fDeltaOverride *= (fTendToDesiredMoveRateOverrideMultiplier * GetTimeStep());
	fDeltaOverride += pPed->GetMotionData()->GetCurrentMoveRateOverride();
	pPed->GetMotionData()->SetCurrentMoveRateOverride(fDeltaOverride);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFishLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				StateIdle_OnEnter(); 
			FSM_OnUpdate
				return StateIdle_OnUpdate();

		FSM_State(State_TurnInPlace)
			FSM_OnEnter
				StateTurnInPlace_OnEnter();
			FSM_OnUpdate
				return StateTurnInPlace_OnUpdate();

		FSM_State(State_Swim)
			FSM_OnEnter
				StateSwim_OnEnter(); 
			FSM_OnUpdate
				return StateSwim_OnUpdate(); 

		FSM_State(State_Flop)
			FSM_OnEnter
				StateFlop_OnEnter();
			FSM_OnUpdate
				return StateFlop_OnUpdate();

		FSM_State(State_Dead)
			FSM_OnEnter
				StateDead_OnEnter();
			FSM_OnUpdate
				return StateDead_OnUpdate();

		FSM_State(State_DoNothing)
			FSM_OnEnter
				StateDoNothing_OnEnter();
			FSM_OnUpdate
				return StateDoNothing_OnUpdate();

	FSM_End
}

bool CTaskFishLocomotion::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_Idle:
		{
			StateSwim_OnProcessMoveSignals();
			return true;
		}
		case State_TurnInPlace:
		{
			StateTurnInPlace_OnProcessMoveSignals();
			return true;
		}
		case State_Swim:
		{
			StateSwim_OnProcessMoveSignals();
			return true;
		}
		case State_Dead:
		{
			StateDead_OnProcessMoveSignals();
			return true;
		}
		case State_DoNothing:
		{
			StateDoNothing_OnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFishLocomotion::ProcessPostMovement()
{
	// This is run after the physics update - so it is safe to check for the fish being out of water.
	// Need to check IsSwimming (low lod case) and IsInWater (high lod case).
	CPed* pPed = GetPed();

	if (m_bSwimNearSurface)
	{
		// Never call the fish as out of water while all of these special conditions are playing out below.
		m_bOutOfWater = false;

		const Vec3V vFishPos = pPed->GetTransform().GetPosition();
		const Vec3V vFishForward = pPed->GetTransform().GetForward();
		float fWaterHead = 0.0f;
		float fWaterMid = 0.0f;
		float fWaterTail = 0.0f;
		Vec3V vProbe = vFishPos;
		
		// Probe at head.
		vProbe += vFishForward * ScalarV(sm_Tunables.m_SurfaceProbeHead);
		if (!Verifyf(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(vProbe), &fWaterHead, true), "Fish not in water at head."))
		{
			fWaterHead = vFishPos.GetZf();
		}
		vProbe.SetZf(fWaterHead);
#if __BANK
		if (sm_bRenderSurfaceChecks)
		{
			grcDebugDraw::Sphere(vProbe, 0.5f, Color_green, false, -1);
		}
#endif

		// Probe at midsection.	
		vProbe = vFishPos;
		if (!Verifyf(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(vProbe), &fWaterMid, true), "Fish not in water at mid."))
		{
			fWaterMid = vFishPos.GetZf();
		}
		vProbe.SetZf(fWaterMid);
#if __BANK
		if (sm_bRenderSurfaceChecks)
		{
			grcDebugDraw::Sphere(vProbe, 0.5f, Color_yellow, false, -1);
		}
#endif

		// Probe at tail.
		vProbe = vFishPos - (vFishForward * ScalarV(sm_Tunables.m_SurfaceProbeTail));
		if (!Verifyf(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(vProbe), &fWaterTail, true), "Fish not in water at tail."))
		{
			fWaterTail = vFishPos.GetZf();
		}
		vProbe.SetZf(fWaterTail);
#if __BANK
		if (sm_bRenderSurfaceChecks)
		{
			grcDebugDraw::Sphere(vProbe, 0.5f, Color_red, false, -1);
		}
#endif

		// Build the ped matrix.
		Mat34V mMat;
		// Position = position of ped moved down along the Z axis by the fin depth.
		Vec3V vNewPosition = vFishPos;
		const float fDT = fwTimer::GetTimeStep();

		float fCurrentZ = vFishPos.GetZf();
		float fDesiredOffset = pPed->GetTaskData().GetSurfaceSwimmingDepthOffset();
		float fDesiredZ = fWaterMid - fDesiredOffset;
		float fHeightRate = 0.0f;

		if (fDesiredZ - fCurrentZ > 0.0f)
		{
			// Scale the rising rate by how much distance is remaining - that way as the shark is getting closer to the surface of the water
			// it actually will ascend slower.
			fHeightRate = sm_Tunables.m_SurfaceHeightRisingLerpRate * (fDesiredZ - fCurrentZ);
		}
		else
		{
			fHeightRate = sm_Tunables.m_SurfaceHeightFallingLerpRate;
		}

		InterpValue(fCurrentZ, fDesiredZ, fHeightRate, false, fDT);

		if (m_bSwimNearSurfaceAllowDeeperThanWaves)
		{
			fCurrentZ = Min(fCurrentZ, vFishPos.GetZf());
		}
		m_bSwimNearSurfaceAllowDeeperThanWaves = false;

		vNewPosition.SetZf(fCurrentZ);

		float fCurrentPitch = pPed->GetCurrentPitch();
		
		bool bAlignToWaves = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::AlignPitchToWavesInFishLocomotion);

		if (bAlignToWaves)
		{
			// Pitch = interpolate from the current pitch to the pitch defined by the angle between the head and tail water spots.
			float fNewPitch = Atan2f(fWaterHead - fWaterTail, sm_Tunables.m_SurfaceProbeHead + sm_Tunables.m_SurfaceProbeTail);
			InterpValue(fCurrentPitch, fNewPitch, sm_Tunables.m_SurfacePitchLerpRate, true, fDT);
		}

		// Heading = the ped's current heading.
		CPed::ComputeMatrixFromHeadingAndPitch(mMat, pPed->GetCurrentHeading(), fCurrentPitch, vNewPosition);

		// Force the new position.
		pPed->SetMatrix(MAT34V_TO_MATRIX34(mMat));

		// We did change the position, so return true.
		return true;
	}
	else
	{
		const bool bIsLowLod = pPed->IsUsingLowLodPhysics();
		
		bool bIsInWater;
		if(pPed->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame())
		{
			const float fThreshold = pPed->IsAPlayerPed() ? sm_Tunables.m_PlayerOutOfWaterThreshold : 0.0f;
			bIsInWater = pPed->m_Buoyancy.GetSubmergedLevel() > fThreshold;
		}
		else
		{
			bIsInWater = pPed->GetIsInWater();
		}

		if (!bIsLowLod && !bIsInWater)
		{
			m_fTimeSpentOutOfWater += fwTimer::GetTimeStep();

			// Players are considered out of water immediately.
			if (pPed->IsAPlayerPed())
			{
				m_fTimeSpentOutOfWater += sm_Tunables.m_FishOutOfWaterDelay;
			}
		}
		else
		{
			m_fTimeSpentOutOfWater = 0.0f;
		}
		if (m_fTimeSpentOutOfWater > sm_Tunables.m_FishOutOfWaterDelay)
		{
			m_bOutOfWater = true;
		}
		return false;
	}
}

void CTaskFishLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	bool bIsBigFish = GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

	if (bIsBigFish)
	{
		fMinOut = -ms_fBigFishMaxPitch;
		fMaxOut = ms_fBigFishMaxPitch;
	}
	else
	{
		fMinOut = -ms_fMaxPitch;
		fMaxOut = ms_fMaxPitch;
	}
}

//////////////////////////////////////////////////////////////////////////
CTask * CTaskFishLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////
void CTaskFishLocomotion::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed* pPed = GetPed();
	if (pPed->IsAPlayerPed())
	{
		// Override with control hash.
		settings.m_CameraHash = sm_Tunables.m_PlayerControlCamera;
	}
	else
	{
		// Don't override.
		settings.m_CameraHash = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::CleanUp()
{
	CPed* pPed = GetPed();

	if (pPed)
	{
		// Set back to default value as other tasks may want buoyancy
		pPed->SetUseExtractedZ(false);
		pPed->m_Buoyancy.SetShouldStickToWaterSurface(true);
	}

	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (clipSet)
	{
		static const fwMvClipId s_walkClip("swim",0x4AD48D2);
		static const fwMvClipId s_runClip("accelerate",0x3B09302A);

		const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_runClip };

		RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::TogglePedSwimNearSurface(CPed* pPed, bool bSwimNearSurface, bool bAllowDeeperThanWaves)
{
	if (pPed)
	{
		CTaskMotionBase* pMotion = pPed->GetCurrentMotionTask();
		if (Verifyf(pMotion && pMotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH, "Target must have fish locomotion to swim near surface!"))
		{
			CTaskFishLocomotion* pFish = static_cast<CTaskFishLocomotion*>(pMotion);
			pFish->SetSwimNearSurface(bSwimNearSurface, bAllowDeeperThanWaves);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::CheckAndRespectSwimNearSurface(CPed* pPed, bool bAllowDeeperThanWaves)
{
	if (pPed)
	{
		float fCurrentZ = pPed->GetTransform().GetPosition().GetZf();
		float fWaterZ = pPed->m_Buoyancy.GetAbsWaterLevel();

		CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, fabs(fCurrentZ - fWaterZ) < sm_Tunables.m_SurfaceHeightFollowingTriggerRange, bAllowDeeperThanWaves);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::ToggleFishSurvivesBeingOutOfWater(CPed* pPed, bool bSurvive)
{
	if (pPed)
	{
		CTaskMotionBase* pMotion = pPed->GetCurrentMotionTask();
		if (Verifyf(pMotion && pMotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH, "Target must have fish locomotion to toggle survive being out of water!"))
		{
			CTaskFishLocomotion* pFish = static_cast<CTaskFishLocomotion*>(pMotion);
			pFish->SetSurviveBeingOutOfWater(bSurvive);
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::InterpolateSpeed(float fDt)
{
	float fCurrentMbrX, fCurrentMbrY;
	GetMotionData().GetCurrentMoveBlendRatio(fCurrentMbrX, fCurrentMbrY);

	float fDesiredMbrX, fDesiredMbrY;
	GetMotionData().GetDesiredMoveBlendRatio(fDesiredMbrX, fDesiredMbrY);

	CPed* pPed = GetPed();
	bool bIsBigFish = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

	float fRate = ms_fFishAccelRate;

	if (fDesiredMbrY > fCurrentMbrY)
	{
		// Player peds always accelerate more quickly, even if they are big fish.
		if (bIsBigFish && !pPed->IsAPlayerPed())
		{
			fRate = ms_fBigFishAccelRate;
		}
	}
	else
	{
		// Player peds always decelerate more slowly.
		if (bIsBigFish || pPed->IsAPlayerPed())
		{
			fRate = ms_fBigFishDecelRate;
		}
	}

	InterpValue(fCurrentMbrX, fDesiredMbrX, fRate, false, fDt);
	InterpValue(fCurrentMbrY, fDesiredMbrY, fRate, false, fDt);

	GetMotionData().SetCurrentMoveBlendRatio(fCurrentMbrY, fCurrentMbrX);
}

void CTaskFishLocomotion::SetNewHeading()
{
	CPed* pPed = GetPed();
	const float fDT = fwTimer::GetTimeStep();

	if(!pPed->GetUsingRagdoll())
	{
		// Get the delta modified by the turn intensity.
		float fTarget = CalcDesiredDirection();

		if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::ForceNoTurningInFishLocomotion))
		{
			fTarget = 0.0f;
		}

		// If a direction change occurs, go back to the min approach rate.
		if (fabs(SubtractAngleShorter(m_fPreviousTurnPhaseGoal, fTarget)) > 5.0f * DtoR)
		{
			if (Sign(m_fPreviousTurnPhaseGoal) != Sign(fTarget))
			{
				m_fTurnApproachRate = sm_Tunables.m_MinTurnApproachRate;
			}
		}

		// Reset the previous turn target.
		m_fPreviousTurnPhaseGoal = fTarget;

		// Lerp towards the target heading approach rate.
		Approach(m_fTurnApproachRate, GetIdealTurnApproachRate(), GetTurnAcceleration(), fDT);

		// Lerp towards the target heading.
		InterpValue(m_fTurnPhase, fTarget, m_fTurnApproachRate, false, fDT);

		// Clamp it to a valid range.
		float fMoveHeadingValue = Clamp(m_fTurnPhase, -PI, PI);

		// Convert it from [0,1] for MoVE.
		fMoveHeadingValue = (fMoveHeadingValue/PI + 1.0f)/2.0f;

		// Send the signal.
		m_networkHelper.SetFloat(ms_DirectionId, fMoveHeadingValue);

		bool bApplyExtraHeadingChanges = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::ApplyExtraHeadingChangesInFishLocomotion);

		// Apply extra turning if required.
		if (bApplyExtraHeadingChanges && fabs(fTarget) * RtoD > sm_Tunables.m_AssistanceAngle)
		{
			float fExtra = 0.0f;
			InterpValue(fExtra, fTarget, sm_Tunables.m_ExtraHeadingRate, true, fDT);
			GetMotionData().SetExtraHeadingChangeThisFrame(fExtra);
		}
	}
}

// Currently adjusting pitch without any anims...
void CTaskFishLocomotion::SetNewPitch()
{
	CPed* pPed = GetPed();
	
	float fCurrent = pPed->GetCurrentPitch();
	const float fDesired = pPed->GetDesiredPitch();
	const float fDT = fwTimer::GetTimeStep();
	
	const static float sf_PitchDelta = 3.0f * DtoR;

	// Check if the pitch has changed.
	if (fabs(SubtractAngleShorter(m_fStoredDesiredPitch, fDesired)) > sf_PitchDelta)
	{
		// Reset the rate.
		m_fPitchRate = 0.0f;

		// Reset the stored pitch.
		m_fStoredDesiredPitch = fDesired;
	}

	// Some fish don't pitch at all under player control unless moving at at least a full walk speed.
	if (!m_bForceFastPitching && GetMotionData().GetCurrentMbrY() < MOVEBLENDRATIO_WALK 
		&& pPed->GetTaskData().GetIsFlagSet(CTaskFlags::BlockPlayerFishPitchingWhenSlow) && pPed->IsAPlayerPed())
	{
		return;
	}

	// Do to inconsistencies in calls to CalcDesiredAngularVelocity() when it low lod physics, it seems best to not update the pitch much.
	bool bLowLodPhysics = pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics);

	// Interpolate pitch towards the desired value.
	if(!pPed->GetUsingRagdoll())
	{
		float fIdealRate = bLowLodPhysics ? sm_Tunables.m_LowLodPhysicsPitchIdealApproachRate : sm_Tunables.m_HighLodPhysicsPitchIdealApproachRate;

		if (m_bForceFastPitching)
		{
			fIdealRate = sm_Tunables.m_FastPitchingApproachRate;
		}

		if (m_bForceFastPitching)
		{
			m_fPitchRate = fIdealRate;
		}
		else if (!pPed->IsAPlayerPed() && pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseSimplePitchingInFishLocomotion))
		{
			m_fPitchRate = fIdealRate;
		}
		else if (pPed->IsAPlayerPed() && GetMotionData().GetCurrentMbrY() >= MOVEBLENDRATIO_RUN)
		{
			m_fPitchRate = fIdealRate;
		}
		else
		{
			// Figure out what the pitch velocity is.
			float fPitchAccel = GetPitchAcceleration();
			Approach(m_fPitchRate, fIdealRate, fPitchAccel, fDT);
		}

		// Figure out how much to pitch this frame.
		float fDelta = InterpValue(fCurrent, fDesired, m_fPitchRate, true, fDT);

		GetMotionData().SetExtraPitchChangeThisFrame(fDelta);
	}
}

float CTaskFishLocomotion::GetPitchAcceleration() const
{
	const CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseSlowPlayerFishPitchAcceleration))
		{
			return sm_Tunables.m_PitchAcceleration;
		}
		else
		{
			return sm_Tunables.m_PlayerPitchAcceleration;
		}
	}
	else
	{
		return sm_Tunables.m_PitchAcceleration;
	}
}

float CTaskFishLocomotion::GetStateTransitionDuration() const
{
	const CPed* pPed = GetPed();
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseLongerBlendsInFishLocomotion))
	{
		return sm_Tunables.m_LongStateTransitionBlendTime;
	}
	else
	{
		return sm_Tunables.m_ShortStateTransitionBlendTime;
	}
}

float CTaskFishLocomotion::GetIdealTurnApproachRate() const
{
	const CPed* pPed = GetPed();
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseLongerBlendsInFishLocomotion))
	{
		return sm_Tunables.m_IdealTurnApproachRateSlow;
	}
	else
	{
		return sm_Tunables.m_IdealTurnApproachRate;
	}
}

float CTaskFishLocomotion::GetTurnAcceleration() const
{
	const CPed* pPed = GetPed();
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseLongerBlendsInFishLocomotion))
	{
		return sm_Tunables.m_TurnAccelerationSlow;
	}
	else
	{
		return sm_Tunables.m_TurnAcceleration;
	}
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

void	CTaskFishLocomotion::StateInitial_OnEnter()
{
	CPed *pPed = GetPed();
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, false );
	pPed->m_Buoyancy.SetShouldStickToWaterSurface(false);
	pPed->SetUseExtractedZ(true);
	pPed->SetIsInWater(true);

	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskFish);
	// stream in the movement clip clipSetId

	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFishLocomotion::StateInitial_OnUpdate()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(clipSet); 

	if (clipSet && clipSet->Request_DEPRECATED())
	{	
		if (m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskFish))
		{ 
			CPed* pPed = GetPed();

			if (!m_networkHelper.IsNetworkActive())
			{
				// Create the network player
				m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskFish);
				m_networkHelper.SetClipSet(m_clipSetId); 

				// Attach it to an empty precedence slot in fwMove
				Assert(pPed->GetAnimDirector());
				CMovePed& move = pPed->GetMovePed();
				move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);
			}

			if (m_networkHelper.IsNetworkActive())
			{
				CMovePed& move = pPed->GetMovePed();
				move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);
				RequestProcessMoveSignalCalls();

				switch(GetMotionData().GetForcedMotionStateThisFrame())
				{
					case CPedMotionStates::MotionState_Idle:
					{
						m_networkHelper.ForceStateChange(ms_IdleStateId);
						SetState(State_Idle);
						break;
					}
					case CPedMotionStates::MotionState_Dead:
					{
						m_networkHelper.ForceStateChange(ms_NoClipStateId);
						SetState(State_Dead);
						break;
					}
					case CPedMotionStates::MotionState_DoNothing:
					{
						m_networkHelper.ForceStateChange(ms_NoClipStateId);
						SetState(State_DoNothing);
						break;
					}
					default:
					{
						m_networkHelper.ForceStateChange(ms_IdleStateId);
						SetState(State_Idle);
						break;
					}
				}
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFishLocomotion::StateSwim_OnEnter()
{
	m_networkHelper.SetFloat(ms_StateTransitionDurationId, GetStateTransitionDuration());
	m_networkHelper.SendRequest(ms_startSwimId);
	m_networkHelper.WaitForTargetState(ms_OnEnterSwimId);

	// Reset cached values.
	m_bMoveInTargetState = false;

	// Reset the turn approach rate.
	m_fTurnApproachRate = sm_Tunables.m_MinTurnApproachRate;

	// Request MoVE signals to be sent to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFishLocomotion::StateSwim_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	if (!m_bSurviveBeingOutOfWater && IsOutOfWater())
	{
		SetState(State_Flop);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if (pPed->GetMotionData()->GetDesiredMbrY() == MOVEBLENDRATIO_STILL)
	{	
		if (pPed->IsAPlayerPed())
		{
			// The extra check on the heading value for player peds is to avoid idle turning.
			// The extra check on the current MBR of player peds is to force them to downgrade their gait to a slow swim prior to stopping from a fast swim.
			if (fabs(CalcDesiredDirection()) < sm_Tunables.m_StartTurnThresholdDegrees * DtoR && pPed->GetMotionData()->GetCurrentMbrY() <= 0.1f)
			{
				SetState(State_Idle);
			}
		}
		else
		{
			SetState(State_Idle);
		}
	}

	return FSM_Continue;
}

void CTaskFishLocomotion::StateSwim_OnProcessMoveSignals()
{
	float fDt = fwTimer::GetTimeStep();

	InterpolateSpeed(fDt);

	SetNewHeading();
	SetNewPitch();

	// No gaits yet for fish, just send over the MBR converted to a MoVE signal.
	m_networkHelper.SetFloat(ms_MoveDesiredSpeedId, GetMotionData().GetCurrentMbrY() / MOVEBLENDRATIO_SPRINT);

	m_networkHelper.SetFloat(ms_MoveRateId, GetMotionData().GetCurrentMoveRateOverride());

	// Cache off values sent by MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}


//////////////////////////////////////////////////////////////////////////

void	CTaskFishLocomotion::StateIdle_OnEnter()
{
	m_networkHelper.SetFloat(ms_StateTransitionDurationId, GetStateTransitionDuration());
	m_networkHelper.SendRequest(ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);

	// Reset cached values.
	m_bMoveInTargetState = false;

	// Reset the turn approach rate.
	m_fTurnApproachRate = sm_Tunables.m_MinTurnApproachRate;

	// Request MoVE signals to be sent to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskFishLocomotion::StateIdle_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	if (!m_bSurviveBeingOutOfWater && IsOutOfWater())
	{
		SetState(State_Flop);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Check if motion has been requested.
	if (pPed->GetMotionData()->GetDesiredMbrY() > MOVEBLENDRATIO_STILL)
	{	
		SetState(State_Swim);
	}
	// Check if idle turns have been requested.
	else if (fabs(CalcDesiredDirection()) >= sm_Tunables.m_StartTurnThresholdDegrees * DtoR)
	{
		if (pPed->IsAPlayerPed())
		{
			// Player peds cannot turn in place.
			SetState(State_Swim);
		}
		else
		{
			SetState(State_TurnInPlace);
		}
	}

	return FSM_Continue;
}

void CTaskFishLocomotion::StateIdle_OnProcessMoveSignals()
{
	// Cache off values sent by MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();

	// Lerp towards the target heading.
	float fTarget = CalcDesiredDirection();
	Approach(m_fTurnPhase, fTarget, m_fTurnApproachRate, fwTimer::GetTimeStep());
}

void CTaskFishLocomotion::StateTurnInPlace_OnEnter()
{
	m_networkHelper.SetFloat(ms_StateTransitionDurationId, GetStateTransitionDuration());
	m_networkHelper.SendRequest(ms_startTurnInPlaceId);
	m_networkHelper.WaitForTargetState(ms_OnEnterTurnInPlaceId);
	
	// Reset cached values.
	m_bMoveInTargetState = false;

	// Reset the turn approach rate.
	m_fTurnApproachRate = sm_Tunables.m_MinTurnApproachRate;

	// Request MoVE signals to be sent to this task.
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskFishLocomotion::StateTurnInPlace_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Check if motion has been requested.
	if (pPed->GetMotionData()->GetDesiredMbrY() > MOVEBLENDRATIO_STILL)
	{
		SetState(State_Swim);
	}
	// Check if the fish should stop idle turns.
	else if (fabs(CalcDesiredDirection()) < sm_Tunables.m_StopTurnThresholdDegrees * DtoR)
	{
		SetState(State_Idle);
	}

	return FSM_Continue;
}

void CTaskFishLocomotion::StateTurnInPlace_OnProcessMoveSignals()
{
	float fDt = fwTimer::GetTimeStep();

	InterpolateSpeed(fDt);

	SetNewHeading();
	SetNewPitch();

	// No gaits yet for fish, just send over the MBR converted to a MoVE signal.
	m_networkHelper.SetFloat(ms_MoveDesiredSpeedId, GetMotionData().GetCurrentMbrY() * 0.33f);

	// Cache off values sent by MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

// Tell the ped to flop.
void CTaskFishLocomotion::StateFlop_OnEnter()
{
	// Cache the ped pointer.
	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		CTask* pTaskNM = rage_new CTaskNMRelax(3000, 6000);
		
		CEventSwitch2NM ragdollEvent(10000, pTaskNM);
		pPed->SwitchToRagdoll(ragdollEvent);
	}
	else
	{
		// Kill the fish.
		CEventDeath event(false, true);
		pPed->GetPedIntelligence()->AddEvent(event);

		// Aggressively delete this ped.
		pPed->SetRemoveAsSoonAsPossible(true);
	}

	// Make it respect gravity and stick to the ground again.
	pPed->SetUseExtractedZ(false);
}

CTask::FSM_Return CTaskFishLocomotion::StateFlop_OnUpdate()
{
	// Cache the ped pointer.
	CPed* pPed = GetPed();
	if (pPed->GetIsDeadOrDying())
	{
		SetState(State_Dead);
	}

	return FSM_Continue;
}

void CTaskFishLocomotion::StateDead_OnEnter()
{
	m_networkHelper.SendRequest(ms_StartNoClipId);
	m_networkHelper.WaitForTargetState(ms_OnEnterNoClipId);

	// Reset signals sent to MoVE.
	m_bMoveInTargetState = false;
	
	// Tell MoVE to send signals back to this task.
	RequestProcessMoveSignalCalls();
}

// Stay dead unless something tells you you are not dead.
CTask::FSM_Return CTaskFishLocomotion::StateDead_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead)
	{
		SetState(State_Initial);
	}
	return FSM_Continue;
}

void CTaskFishLocomotion::StateDead_OnProcessMoveSignals()
{
	// Cache off values sent by MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

void CTaskFishLocomotion::StateDoNothing_OnEnter()
{
	m_networkHelper.SendRequest(ms_StartNoClipId);
	m_networkHelper.WaitForTargetState(ms_OnEnterNoClipId);

	// Reset signals sent to MoVE.
	m_bMoveInTargetState = false;

	// Tell MoVE to send signals back to this task.
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskFishLocomotion::StateDoNothing_OnUpdate()
{
	if (!m_networkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead)
	{
		SetState(State_Initial);
	}
	return FSM_Continue;
}

void CTaskFishLocomotion::StateDoNothing_OnProcessMoveSignals()
{
	// Cache off values sent by MoVE.
	m_bMoveInTargetState |= m_networkHelper.IsInTargetState();
}

// Returns true if the fish is above the water level.
bool CTaskFishLocomotion::IsOutOfWater()
{
	return m_bOutOfWater;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFishLocomotion::AllowTimeslicing()
{
	CPed* pPed = GetPed();
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}

//////////////////////////////////////////////////////////////////////////

float CTaskFishLocomotion::CalcDesiredPitch()
{
	CPed* pPed = GetPed();
	const float fCurrentPitch = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentPitch());
	const float fDesiredPitch = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredPitch());

	const float fPitchDelta = SubtractAngleShorter(fDesiredPitch, fCurrentPitch);

	return fPitchDelta;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskFishLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Initial: 
			return "Initial";
		case State_Idle: 
			return "Idle";
		case State_TurnInPlace:
			return "TurnInPlace";
		case State_Swim: 
			return "Swimming";
		case State_Flop: 
			return "Flopping";
		case State_Dead:
			return "Dead";
		case State_DoNothing:
			return "DoNothing";
		default: 
		{
			taskAssertf(false, "Unknown state!"); 
			return "Unknown"; 
		}
	}
}

void CTaskFishLocomotion::Debug() const
{
}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
