#include "TaskBirdLocomotion.h"

#include "fwmaths/random.h"
#include "fwScene/scan/Scan.h"
#include "fwScene/scan/ScanCascadeShadows.h"
#include "spatialdata/aabb.h"

#include "ai/stats.h"
#include "animation/MovePed.h"
#include "camera/gameplay/GameplayDirector.h"
#include "debug/DebugScene.h"
#include "Event/Events.h"
#include "peds/PedIntelligence.h"
#include "Peds/pedplacement.h"
#include "Peds/PedMotionData.h"
#include "Physics/CollisionRecords.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/System/MotionTaskData.h"

AI_OPTIMISATIONS()

using namespace AIStats;

//////////////////////////////////////////////////////////////////////////
//	Bird Locomotion
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskBirdLocomotion::ms_startIdleId("StartIdle",0x2A8F6981);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);

const fwMvRequestId CTaskBirdLocomotion::ms_startWalkId("StartWalk",0xD999A89E);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterWalkId("OnEnterWalk",0x229ED4B);

const fwMvRequestId CTaskBirdLocomotion::ms_startTakeOffId("StartTakeOff",0xBC98B86E);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterTakeOffId("OnEnterTakeOff",0x1AB54AEF);
const fwMvFloatId CTaskBirdLocomotion::ms_TakeOffPhaseId("TakeOffPhase", 0xe824244b);


const fwMvRequestId CTaskBirdLocomotion::ms_startDescendId("StartDescending",0xCEC30260);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterDescendId("OnEnterDescending",0x46C6BD21);

const fwMvRequestId CTaskBirdLocomotion::ms_startLandId("StartLanding",0xBD826C5A);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterLandId("OnEnterLanding",0x4DD27063);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnExitLandId("OnExitLanding",0x917C6948);

const fwMvRequestId CTaskBirdLocomotion::ms_startFlyId("StartFly",0x6F35E712);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterFlyId("OnEnterFly",0xC066196A);

const fwMvStateId CTaskBirdLocomotion::ms_FlyId("Fly",0xfd57a904);
const fwMvStateId CTaskBirdLocomotion::ms_GlideId("Glide",0xf9209d38);
const fwMvStateId CTaskBirdLocomotion::ms_IdleId("Idle",0x71c21326);
const fwMvStateId CTaskBirdLocomotion::ms_WalkId("Walk",0x83504c9c);

const fwMvRequestId CTaskBirdLocomotion::ms_startGlideId("StartGlide",0x197A79A3);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterGlideId("OnEnterGlide",0xCEA32FB3);

const fwMvRequestId CTaskBirdLocomotion::ms_startFlyUpwardsId("StartFlyUpwards",0x15BC67D);
const fwMvBooleanId CTaskBirdLocomotion::ms_OnEnterFlyUpwardsId("OnEnterFlyUpwards",0x5C3DEC39);

const fwMvFloatId CTaskBirdLocomotion::ms_DirectionId("Direction",0x34DF9828);
const fwMvFloatId CTaskBirdLocomotion::ms_TakeOffRateId("TakeOffRate",0x5DB2DCD);
const fwMvBooleanId CTaskBirdLocomotion::ms_CanEarlyOutForMovement("CAN_EARLY_OUT_FOR_MOVEMENT",0x7E1C8464);

dev_float CTaskBirdLocomotion::ms_fMaxPitch 	= HALF_PI * 0.95f;
dev_float CTaskBirdLocomotion::ms_fMaxPitchPlayer = HALF_PI * 0.75f;
u32 CTaskBirdLocomotion::ms_uPlayerFlapTimeMS = 1200;

u32 CTaskBirdLocomotion::sm_TimeForNextTakeoff = 0;

// Instance of tunable task params
CTaskBirdLocomotion::Tunables CTaskBirdLocomotion::sm_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskBirdLocomotion, 0xd9ed5d70);

CTaskBirdLocomotion::CTaskBirdLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_clipSetId(clipSetId)
, m_fTurnDiscrepancyTurnRate(0.12f)
, m_fDesiredTurnForHeadingLerp(0.001f)
, m_fLandClipXYTranslation(0.0f)
, m_fLandClipZTranslation(0.0f)
, m_fTurnPhase(0.5f)
, m_fTurnRate(0.0f)
, m_fLastTurnDirection(0.0f)
, m_fPitchRate(0.0f)
, m_fLastPitchDirection(0.0f)
, m_fTurnRateDuringTakeOff(0.0f)
, m_fTimeWithContinuousCollisions(0.0f)
, m_fTimeOffScreenWhenStuck(0.0f)
, m_bWakeUpInProcessPhysics(false)
, m_bLandWhenAble(false)
, m_fDescendClipRatio(1.0f)
, m_fAscendClipRatio(1.0f)
, m_bInTargetMotionState(false)
, m_bTakeoffAnimationCanBlendOut(false)
, m_bFlapRequested(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_BIRD);
}

//////////////////////////////////////////////////////////////////////////

CTaskBirdLocomotion::~CTaskBirdLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskBirdLocomotion::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	if (ShouldForceNoTimeslicing())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}
	else
	{
		AllowTimeslicing();
	}

	// Disable anim timeslicing if a bird's shadow is visible.
	ProcessShadows();


	if (pPed->IsAPlayerPed())
	{
		bool bKillPlayer = IsBirdUnderwater();

		if (GetState() == State_Fly || GetState() == State_Glide)
		{
			// Bird collision doesn't seem to play well with ProcessPreComputeImpacts when flying into the ground, so skip it.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_TaskSkipProcessPreComputeImpacts, true);

			// Check the collision record, if a collision exists then kill the player.
			// Also check if you hit the water.
			const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
			if(pColRecord)
			{
				bKillPlayer = true;
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

		if (bKillPlayer)
		{
			CDeathSourceInfo deathInfo;
			CTask* pTaskDie = rage_new CTaskDyingDead(&deathInfo, CTaskDyingDead::Flag_ragdollDeath);
			CEventGivePedTask deathEvent(PED_TASK_PRIORITY_PRIMARY, pTaskDie);
			pPed->GetPedIntelligence()->AddEvent(deathEvent);
		}
	}


	return FSM_Continue;
}

CTask::FSM_Return CTaskBirdLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* ped = GetPed();
	if(ped && ped->IsNetworkClone())
	{
		if(iEvent == OnUpdate && iState >= State_Idle)
		{
			if (m_requestedCloneStateChange != -1 && m_requestedCloneStateChange != GetState())
			{
				AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] Animal Ped %s performing state change to %s\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(m_requestedCloneStateChange));

				switch(m_requestedCloneStateChange)
				{
				case State_Idle:
					ForceStateChange(ms_IdleId);
					break;
				case State_Walk:
					ForceStateChange(ms_WalkId);
					break;
				case State_Fly:
					ForceStateChange(ms_FlyId);
					break;
				case State_Glide:
					ForceStateChange(ms_GlideId);
					break;
				}

				SetState(m_requestedCloneStateChange);
				m_requestedCloneStateChange = -1;
				return FSM_Continue;
			}
		}
	}

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

		FSM_State(State_Walk)
			FSM_OnEnter
				StateWalk_OnEnter(); 
			FSM_OnUpdate
				return StateWalk_OnUpdate();

		FSM_State(State_WaitForTakeOff)
			FSM_OnUpdate
				return StateWaitForTakeOff_OnUpdate();

		FSM_State(State_TakeOff)
			FSM_OnEnter
				StateTakeOff_OnEnter(); 
			FSM_OnUpdate
				return StateTakeOff_OnUpdate(); 

		FSM_State(State_Descend)
			FSM_OnEnter
				StateDescend_OnEnter(); 
			FSM_OnUpdate
				return StateDescend_OnUpdate(); 

		FSM_State(State_Land)
			FSM_OnEnter
				StateLand_OnEnter(); 
			FSM_OnUpdate
				return StateLand_OnUpdate(); 

		FSM_State(State_Fly)
			FSM_OnEnter
				StateFly_OnEnter(); 
			FSM_OnUpdate
				return StateFly_OnUpdate();

		FSM_State(State_Glide)
			FSM_OnEnter
				StateGlide_OnEnter(); 
			FSM_OnUpdate
				return StateGlide_OnUpdate();

		FSM_State(State_FlyUpwards)
			FSM_OnEnter
				StateFlyUpwards_OnEnter();
			FSM_OnUpdate
				return StateFlyUpwards_OnUpdate();

		FSM_State(State_Dead)
			FSM_OnUpdate
				return StateDead_OnUpdate();

	FSM_End
}


CTask::FSM_Return CTaskBirdLocomotion::ProcessPostFSM()
{
	s32 iState = GetState();

	// Disable some performance saving tweaks if you are the player.
	if (!GetPed()->IsAPlayerPed())
	{
		if (iState == State_Fly || iState == State_Glide)
		{
			ProcessInactiveFlightMode();
		}
		else if (iState == State_Descend || iState == State_FlyUpwards)
		{
			ProcessActiveFlightMode();
		}
	}
	return FSM_Continue;
}

bool CTaskBirdLocomotion::IsInWaterWhileLowLod() const
{
	const CPed* pPed = GetPed();
	return pPed->GetIsSwimming() && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics);
}

void CTaskBirdLocomotion::ProcessActiveFlightMode()
{
	CPed* pPed = GetPed();

	//CPed::SetUsingLowLodPhysics() switches UseExtractedZ back off.  Birds need this on to stay in the air.
	if (!pPed->GetUseExtractedZ())
	{
		pPed->SetUseExtractedZ(true);
	}
}

void CTaskBirdLocomotion::ProcessInactiveFlightMode()
{
	CPed* pPed = GetPed();

	// At this point the bird is above the heightmap so it is in a safe area.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_NoCollisionMovementMode, true);

	// Prevent the ped from getting woken up by having high lod physics.
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	lod.SetForcedLodFlag(CPedAILod::AL_LodPhysics);
}

// Disable anim timeslicing if a bird's shadow is visible.
void CTaskBirdLocomotion::ProcessShadows()
{
	if (GetState() == State_Fly || GetState() == State_FlyUpwards || GetState() == State_Descend || GetState() == State_Glide ||
		GetState() == State_TakeOff)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		CPed* pPed = GetPed();
		if (pPlayer)
		{
			// Get the positions of the birds and the player.
			const Vec3V playerPosV = pPlayer->GetTransform().GetPosition();
			const Vec3V pedPosV = pPed->GetMatrixRef().GetCol3();

			// Get the direction of the shadow, and compute the delta of a long segment along that direction.
			const fwScanCascadeShadowInfo& shadowInfo = fwScan::GetScanCascadeShadowInfo();
			const Vec3V shadowDirV = shadowInfo.GetShadowDir();
	
			// Check for the magnitude being small - this probably indicates that shadows have been turned off.
			if (IsLessThanAll(MagSquared(shadowDirV), ScalarV(0.01f)))
			{
				return;
			}

			// Compute the closest position to the player, on the infinite line segment starting at the
			// bird, in the direction of the shadows.
			const ScalarV closestPosTV = geomTValues::FindTValueOpenSegToPointV(pedPosV, shadowDirV, playerPosV);
			const Vec3V closestPosV = AddScaled(pedPosV, shadowDirV, closestPosTV);

			// Compute the squared distance to the closest point on the segment, and the corresponding threshold.
			const ScalarV distSqToClosestV = DistSquared(playerPosV, closestPosV);
			const ScalarV thresholdDistV = ScalarV(sm_Tunables.m_NoAnimTimeslicingShadowRange);
			const ScalarV thresholdDistSqV = Scale(thresholdDistV, thresholdDistV);

			// We only care about visible shadows if the bird's shadow could possibly be anywhere near the player.
			// IsShadowVisible() seems to be rather conservative.
			if(IsLessThanAll(distSqToClosestV, thresholdDistSqV))
			{
				spdAABB pedAABB;
				pPed->GetAABB(pedAABB);

				bool bShadowVisible = shadowInfo.IsShadowVisible(pedAABB);

				if (bShadowVisible)
				{
					// DEBUG DRAWING
					//grcDebugDraw::Line(pedPosV, AddScaled(pedPosV, shadowDirV, ScalarV(1000.0f)), Color_white, -1);
					pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceAnimUpdate);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskBirdLocomotion::ProcessMoveSignals()
{
	const int iCurrentState = GetState();

	SetNewPitch();

	switch(iCurrentState)
	{
		case State_Idle:
		{
			StateIdle_OnProcessMoveSignals();
			return true;
		}
		case State_Walk:
		{
			StateWalk_OnProcessMoveSignals();
			return true;
		}
		case State_Fly:
		{
			StateFly_OnProcessMoveSignals();
			return true;
		}
		case State_Glide:
		{
			StateGlide_OnProcessMoveSignals();
			return true;
		}
		case State_Descend:
		{
			StateDescend_OnProcessMoveSignals();
			return true;
		}
		case State_FlyUpwards:
		{
			StateFlyUpwards_OnProcessMoveSignals();
			return true;
		}
		case State_Land:
		{
			StateLand_OnProcessMoveSignals();
			return true;
		}
		case State_TakeOff:
		{
			StateTakeOff_OnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

Vec3V_Out CTaskBirdLocomotion::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
{
	CPed* pPed = GetPed();

	// Handle velocity overrides.
	// This must be done *before* CalcDesiredVelocity as that is where the velocity override is used.
	if (pPed->IsAPlayerPed())
	{
		// Look up the original animated velocity.
		Vector3 vVelocity = pPed->GetAnimatedVelocity();
		if (GetState() == State_Fly)
		{
			// Scale by the flap tunable.
			vVelocity *= GetFlapSpeedScale();
			SetVelocityOverride(vVelocity);
		}
		else if (GetState() == State_Glide)
		{
			// Scale by the glide tunable.
			vVelocity *= GetGlideSpeedScale();
			SetVelocityOverride(vVelocity);
		}
		else
		{
			// Not using an override for any other states.
			ClearVelocityOverride();
		}
	}

	Vec3V vVel = CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);

	if (pPed->IsAPlayerPed())
	{
		if (GetState() == State_Land)
		{
			// Don't sink below the ground when landing...
			if (pPed->IsOnGround())
			{
				vVel.SetZf(Max(0.0f, vVel.GetZf()));
			}
		}
		else if (GetState() == State_Fly)
		{
			// Accelerate the speed when pitched downward.
			if (pPed->GetCurrentPitch() <= sm_Tunables.m_PlayerTiltedDownToleranceDegrees * DtoR)
			{
				Vec3V vDirection = NormalizeFastSafe(vVel, Vec3V(V_ZERO));
				vDirection *= ScalarV(sm_Tunables.m_PlayerTiltedDownSpeedBoost * fTimestep);
				vVel += vDirection;

				if (IsGreaterThanAll(MagSquared(vVel), ScalarV(sm_Tunables.m_PlayerTiltedSpeedCap * sm_Tunables.m_PlayerTiltedSpeedCap)))
				{
					vVel = NormalizeFastSafe(vVel, Vec3V(V_ZERO));
					vVel *= ScalarV(sm_Tunables.m_PlayerTiltedSpeedCap);
				}
			}
		}
	}

	return vVel;
}

///////////////////////////////////////////////////////////////////////////////

bool CTaskBirdLocomotion::ProcessPhysics( float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice) )
{
	return GetPed()->IsAPlayerPed() || m_bWakeUpInProcessPhysics;
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		// Enable gravity
		pPed->SetUseExtractedZ(false);
	}

	if (m_networkHelper.GetNetworkPlayer())
	{
		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskBirdLocomotion::IsOnFoot()
{
	return ( (GetState() == State_Idle) || (GetState() == State_Walk) || (GetState() == State_WaitForTakeOff) );
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	if (clipSet)
	{
		static const fwMvClipId s_walkClip("walk_forward",0x4A45106C);

		const fwMvClipId clipNames[3] = { s_walkClip, s_walkClip, s_walkClip };

		RetrieveMoveSpeeds(*clipSet, speeds, clipNames);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskBirdLocomotion::CanFlyThroughAir()
{
	if (!GetPed()->IsAPlayerPed())
	{
		return true;
	}

	switch(GetState())
	{
		case State_Glide:
		case State_Fly:
		case State_TakeOff:
		case State_Land:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTask* CTaskBirdLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
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

bool CTaskBirdLocomotion::CanLandNow() const
{
	const CPed* pPed = GetPed();
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if (pPed->IsAPlayerPed())
	{
		// Players don't have to get quite as close to the ground to land.
		const static float sf_PlayerFudgeFactor = 1.0f;
		static const float sf_ProbeRadius = 0.05f;
		const float fProbeLength = fabs(m_fLandClipZTranslation) + sf_PlayerFudgeFactor;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestFixedResults<> probeResults;
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetCapsule(vPedPos, vPedPos + Vector3(0.0f, 0.0f, -fProbeLength), sf_ProbeRadius);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

		if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			// Don't allow landing on cars.
			if(probeResults[0].GetHitEntity()->GetIsTypeVehicle())
			{
				return false;
			}

			// Don't allow landing into water

			

			float fWaterHeightOut = 0.0f;
			float fWaterProbeLength = vPedPos.GetZ() - probeResults[0].GetHitPosition().GetZ();
			if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vPedPos, &fWaterHeightOut, false, fWaterProbeLength))
			{
				return false;
			}

			// Slope of where we are landing needs to be < 40 degrees.
			if (probeResults[0].GetNormal().GetZf() > 0.766f)
			{
				return true;
			}
		}

		return false;
	}
	else
	{
		Vector3 vPedGroundPos = vPedPos;

		if (!CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE, &vPedGroundPos, NULL, NULL, NULL, 0.5f,
			99.5f, (ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE), NULL, true))
		{
			return false;
		}

		float heightDiff = (vPedPos.z - vPedGroundPos.z);

		return heightDiff < fabs(m_fLandClipZTranslation);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::RequestPlayerLand()
{
	m_bLandWhenAble = true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::RequestFlap()
{
	m_bFlapRequested = true;
}

//////////////////////////////////////////////////////////////////////////

// Change the bird's heading to approach the desired.
void CTaskBirdLocomotion::SetNewHeading()
{
	CPed* pPed = GetPed();
	if (!pPed->GetUsingRagdoll())
	{
		// NOTE: using time since last frame as aiTask::GetTimeStep() only counts full task updates
		float fDT = fwTimer::GetTimeStep();

		// Find out how much the bird wants to turn.
		float fTarget = CalcDesiredHeading();

		if (pPed->IsAPlayerPed())
		{
			if (fabs(fTarget) < sm_Tunables.m_PlayerHeadingDeadZoneThresholdDegrees * DtoR)
			{
				fTarget = 0.0f;
			}

			// Reset the rate if the direction changes.
			if (Sign(fTarget) != Sign(m_fLastTurnDirection))
			{
				m_fTurnRate = 0.0f;
			}

			// Gliding has slower turning than flapping.
			bool bGliding = GetState() == State_Glide;

			float fTargetRate = 1.0f;
			float fTurnAcceleration = 1.0f;

			if (bGliding)
			{
				fTurnAcceleration = sm_Tunables.m_PlayerGlideTurnAcceleration;

				fTargetRate = sm_Tunables.m_PlayerGlideIdealTurnRate;
			}
			else
			{
				fTurnAcceleration = sm_Tunables.m_PlayerFlappingTurnAcceleration;

				fTargetRate = sm_Tunables.m_PlayerFlappingIdealTurnRate;
			}

			m_fLastTurnDirection = fTarget;

			// Interpolate towards the desired rate.
			Approach(m_fTurnRate, fTargetRate, fTurnAcceleration, fDT);

			// Interpolate towards the desired heading.
			Approach(m_fTurnPhase, fTarget, m_fTurnRate, fDT);
		}
		else
		{
			// Lerp towards the target heading.
			InterpValue(m_fTurnPhase, fTarget, sm_Tunables.m_AIBirdTurnApproachRate, true, fDT);
		}


		// Convert to a move signal.
		float fMoveHeadingValue = ConvertRadianToSignal(m_fTurnPhase);

		// Send the signal.
		m_networkHelper.SetFloat(ms_DirectionId, fMoveHeadingValue);
	}
}

void CTaskBirdLocomotion::SetNewPitch()
{
	CPed* pPed = GetPed();

	// NOTE: using time since last frame as aiTask::GetTimeStep() only counts full task updates
	float fDT = fwTimer::GetTimeStep();

	const float fDesired = GetMotionData().GetDesiredPitch();
	float fCurrent = GetMotionData().GetCurrentPitch();

	if (pPed->IsAPlayerPed())
	{
		float fPitchDir = CalcDesiredPitch();

		// Reset the rate if traveling in the opposite direction.
		if (Sign(fPitchDir) != Sign(m_fLastPitchDirection))
		{
			m_fPitchRate = 0.0f;
		}

		m_fLastPitchDirection = fPitchDir;

		// Approach the ideal pitch rate.
		Approach(m_fPitchRate, sm_Tunables.m_PlayerPitchIdealRate, sm_Tunables.m_PlayerPitchAcceleration, fDT);
	}
	else
	{
		// AI pitch linearly.
		m_fPitchRate = sm_Tunables.m_AIPitchTurnApproachRate;
	}

	// Adjust pitch by the rate.
	float fDelta = InterpValue(fCurrent, fDesired, m_fPitchRate, true, fDT);
	GetMotionData().SetExtraPitchChangeThisFrame(fDelta);
}

void CTaskBirdLocomotion::SetNewGroundPitchForBird(CPed& rPed, float fDT)
{
	float fCurrentPitch = rPed.GetCurrentPitch();
	float fCalculatedDesiredPitch = 0.0f;
	Vec3V vGroundNormal = VECTOR3_TO_VEC3V(rPed.GetGroundNormal());

	// No point bothering with the math below if the ped is on flat ground.  Also avoids normalizing a zero vector.
	if (vGroundNormal.GetZf() < 0.999f && !IsCloseAll(vGroundNormal, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)) && IsFiniteAll(vGroundNormal))
	{
		// Grab a copy of the ped's forward vector.
		Vec3V vForward = rPed.GetTransform().GetForward();

		// Slope is the angle rotated away from WORLD_UP.
		const float fGroundSlope = Angle(vGroundNormal, VECTOR3_TO_VEC3V(ZAXIS)).Getf();
		const ScalarV zeroV(V_ZERO);
		vForward.SetZ(zeroV);
		vGroundNormal.SetZ(zeroV);
		vForward = NormalizeFastSafe(vForward, Vec3V(V_ZERO));
		vGroundNormal = Normalize(vGroundNormal);

		// Dot it with the forward vector (ignoring z) to match up with the angle that the ped is approaching the slope.
		fCalculatedDesiredPitch = fGroundSlope * Dot(vForward, vGroundNormal).Getf();
		fCalculatedDesiredPitch = -1.0f * fCalculatedDesiredPitch;
	}

	fCalculatedDesiredPitch = Clamp(fCalculatedDesiredPitch, -ms_fMaxPitchPlayer, ms_fMaxPitchPlayer);
	
	// TaskMotionBase will need to know that this is now our desired pitch.
	rPed.SetDesiredPitch(fCalculatedDesiredPitch);

	// Figure out how much pitch to adjust this frame.
	static const float sf_PlayerGroundPitchRate = 2.0f;
	float fDelta = InterpValue(fCurrentPitch, fCalculatedDesiredPitch, sf_PlayerGroundPitchRate, true, fDT);
	Assert(rPed.GetMotionData());
	rPed.GetMotionData()->SetExtraPitchChangeThisFrame(fDelta);
}

// Return the delta to add onto the current heading to achieve the desired heading.
float CTaskBirdLocomotion::CalcDesiredHeading() const
{
	const CPed* pPed = GetPed();
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());

	const float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

	return fHeadingDelta;
}

//////////////////////////////////////////////////////////////////////////

float CTaskBirdLocomotion::CalcDesiredPitch() const
{
	const CPed* pPed = GetPed();
	const float fCurrentPitch = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentPitch());
	const float fDesiredPitch = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredPitch());

	const float fPitchDelta = SubtractAngleShorter(fDesiredPitch, fCurrentPitch);

	return fPitchDelta;
}

//////////////////////////////////////////////////////////////////////////

float CTaskBirdLocomotion::GetExtraRadiusForGotoPoint() const
{
	return m_fLandClipXYTranslation;
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateInitial_OnEnter()
{
	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootBird);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskBirdLocomotion::StateInitial_OnUpdate()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(clipSet);

	if (clipSet && clipSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootBird))
	{
		// Check for the landing animation
		const crClip *pClip = clipSet->GetClip(fwMvClipId("land",0xE355D038));
		if (pClip)
		{
			// Extract the mover translation by analyzing the animation from start to end
			Vector3 vClipDisp(fwAnimHelpers::GetMoverTrackDisplacement(*pClip, 0.0f, 1.0f));

			// compute the XY translation as the velocity in XY plane.
			m_fLandClipXYTranslation = vClipDisp.XYMag();
			m_fLandClipZTranslation = vClipDisp.z;
		}

		// Check for the descend animation.
		m_fDescendClipRatio = ComputeClipRatio(fwMvClipId("descend",0x3A770027));

		// Check for the ascend animation.
		m_fAscendClipRatio = ComputeClipRatio(fwMvClipId("ascend",0xAB60AE69));

		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskOnFootBird);

		m_networkHelper.SetClipSet( m_clipSetId); 

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();
		move.SetMotionTaskNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);

		CPedMotionData& motion = GetMotionData();

		if (motion.GetIsFlying())
		{
			SetState(State_Fly);
		}
		else if (abs(motion.GetCurrentMbrY())> MOVEBLENDRATIO_STILL)
		{
			SetState(State_Walk);
		}
		else if (motion.GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Dead)
		{
			SetState(State_Dead);
		}
		else
		{
			SetState(State_Idle);
		}
		m_networkHelper.ForceNextStateReady();
	}

	return FSM_Continue;
}

// Ratio of Z translation to XY translation in the clip.
float	CTaskBirdLocomotion::ComputeClipRatio(fwMvClipId clipId)
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(clipSet);

	if (clipSet && clipSet->Request_DEPRECATED())
	{
		// Check for the landing animation
		const crClip *pClip = clipSet->GetClip(clipId);
		if (Verifyf(pClip, "No clip found."))
		{
			// Extract the mover translation by analyzing the animation from start to end
			Vector3 vClipDisp(fwAnimHelpers::GetMoverTrackDisplacement(*pClip, 0.0f, 1.0f));

			float fXY = vClipDisp.XYMag();
			float fZ = vClipDisp.z;
			if (fXY > SMALL_FLOAT)
			{
				return fZ / fXY;
			}
		}
	}
	
	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateWalk_OnEnter()
{
	m_networkHelper.SendRequest(ms_startWalkId);
	m_networkHelper.WaitForTargetState(ms_OnEnterWalkId);

	// Clear the cached signal values.
	m_bInTargetMotionState = false;

	// Request signals to be sent from MoVE to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskBirdLocomotion::StateWalk_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	if (pPed->IsAPlayerPed())
	{
		if(pPed->GetCapsuleInfo()->GetBipedCapsuleInfo())
		{
			const CBipedCapsuleInfo *pBipedCapsuleInfo = pPed->GetCapsuleInfo()->GetBipedCapsuleInfo();
			pPed->SetDesiredMainMoverCapsuleRadius(pBipedCapsuleInfo->GetRadiusRunning(), true);
		}

		SetNewGroundPitchForBird(*pPed, GetTimeStep());
	}

	CPedMotionData& motion = GetMotionData();

	if (abs(motion.GetDesiredMbrY() ) == MOVEBLENDRATIO_STILL)
	{	
		SetState(State_Idle);
	}
	else if (motion.GetIsFlying())
	{
		SetState(State_WaitForTakeOff);
	}

	if (IsInWaterWhileLowLod())
	{
		// Block the low physics flag so the bird can drown, etc.
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
	}

	return FSM_Continue;
}

void CTaskBirdLocomotion::StateWalk_OnProcessMoveSignals()
{
	// No walking turn anims for birds, so turn manually through code.
	const float fDT = fwTimer::GetTimeStep();

	// Find out how much the bird wants to turn.
	float fCurrentHeading = GetMotionData().GetCurrentHeading();
	const float fDesiredHeading = GetMotionData().GetDesiredHeading();

	// Low lod physics seems to require a stronger heading lerp rate to reduce footsliding effects.
	const bool bLowLodPhysics = GetPed()->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics);

	// Calculate the extra heading change.
	float fExtra = InterpValue(fCurrentHeading, fDesiredHeading, bLowLodPhysics ? sm_Tunables.m_LowLodWalkHeadingLerpRate : sm_Tunables.m_HighLodWalkHeadingLerpRate, true, fDT);

	// Add on the extra heading change.
	GetMotionData().SetExtraHeadingChangeThisFrame(fExtra);

	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskBirdLocomotion::StateWaitForTakeOff_OnUpdate()
{
	if (CanBirdTakeOffYet())
	{
		// Decide when the next bird can take off.
		sm_TimeForNextTakeoff = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange((int)sm_Tunables.m_MinWaitTimeBetweenTakeOffsMS, (int)sm_Tunables.m_MaxWaitTimeBetweenTakeOffsMS);
		SetState(State_TakeOff);
	}
	return FSM_Continue;
}

bool CTaskBirdLocomotion::CanBirdTakeOffYet()
{
	// Player peds can takeoff as soon as they want to.
	if (GetPed()->IsAPlayerPed())
	{
		return true;
	}

	// Never delay for more than 1.5 seconds, it is too long to look good at that point.
	// This is possible if the timer wraps around or if there are large numbers of birds in the same spot.
	if (fwTimer::GetTimeInMilliseconds() >= sm_TimeForNextTakeoff || GetTimeInState() > 1.5f)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateTakeOff_OnEnter()
{
	m_networkHelper.SendRequest(ms_startTakeOffId);
	m_networkHelper.WaitForTargetState(ms_OnEnterTakeOffId);
	m_networkHelper.SetFloat(ms_TakeOffRateId, fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTakeOffRate, sm_Tunables.m_MaxTakeOffRate));
	m_fTurnRateDuringTakeOff = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTakeOffHeadingChangeRate, sm_Tunables.m_MaxTakeOffHeadingChangeRate);

	// Clear the cached signal values.
	m_bInTargetMotionState = false;
	m_bTakeoffAnimationCanBlendOut = false;

	// Request signals to be sent from MoVE to this task.
	RequestProcessMoveSignalCalls();

	CPed *pPed = GetPed();
	
	// Disable gravity
	pPed->SetUseExtractedZ(true);

	if(pPed->GetPedAudioEntity())
		pPed->GetPedAudioEntity()->RegisterBirdTakingOff();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateTakeOff_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (m_bTakeoffAnimationCanBlendOut)
	{
		SetState(State_FlyUpwards);
	}

	return FSM_Continue;
}

void CTaskBirdLocomotion::StateTakeOff_OnProcessMoveSignals()
{
	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();

	// Find out how much the bird wants to turn.
	float fCurrentHeading = GetMotionData().GetCurrentHeading();
	const float fDesiredHeading = GetMotionData().GetDesiredHeading();

	const float fDT = fwTimer::GetTimeStep();

	// Calculate the extra heading change.
	float fExtra = InterpValue(fCurrentHeading, fDesiredHeading, m_fTurnRateDuringTakeOff, true, fDT);

	// Turn slightly so that birds can avoid obstacles during takeoff.
	CPedMotionData& motion = GetMotionData();
	motion.SetExtraHeadingChangeThisFrame(fExtra);

	// Check the phase of the takeoff.
	float fPhase = 0.0f;
	m_networkHelper.GetFloat(ms_TakeOffPhaseId, fPhase);
	if (fPhase > sm_Tunables.m_DefaultTakeoffBlendoutPhase)
	{
		m_bTakeoffAnimationCanBlendOut = true;
	}

	// Check the tagging override - only valid earlier not later than default phase.
	bool bTakeoffAnimationCanBlendOut = false;
	m_networkHelper.GetBoolean(ms_CanEarlyOutForMovement, bTakeoffAnimationCanBlendOut);
	m_bTakeoffAnimationCanBlendOut |= bTakeoffAnimationCanBlendOut;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateDescend_OnEnter()
{
	m_networkHelper.SendRequest(ms_startDescendId);
	m_networkHelper.WaitForTargetState(ms_OnEnterDescendId);
	m_fTurnPhase = 0.5f;

	// Clear the target motion state values.
	m_bInTargetMotionState = false;

	// Clear the collision timer.
	m_fTimeWithContinuousCollisions = 0.0f;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateDescend_OnUpdate()
{
	const float fTarget = GetMotionData().GetDesiredHeading();
	float fCurrent = GetMotionData().GetCurrentHeading();

	// Lerp towards the target heading slowly while descending - there are no descending turn animations.
	float fExtra = InterpValue(fCurrent, fTarget, 1.0f, true, GetTimeStep());

	GetMotionData().SetExtraHeadingChangeThisFrame(fExtra);

	// See if the bird is stuck when descending and deal with it accordingly.
	CheckForCollisions();

	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (!GetMotionData().GetIsFlying())
	{
		// If was descending and no longer need to be flying, try to land.
		m_bLandWhenAble = true;
	}

	if (m_bLandWhenAble)
	{
		if (CanLandNow())
		{	
			m_bLandWhenAble = false;
			SetState(State_Land);
		}
	}

	// If not landing, then the bird might need to climb.
	if (GetMotionData().GetIsFlyingForwards() || GetMotionData().GetIsFlyingUpwards())
	{
		SetState(State_Fly);
	}
	
	return FSM_Continue;
}

void CTaskBirdLocomotion::StateDescend_OnProcessMoveSignals()
{
	// Send signals to MoVE
	SetNewHeading();

	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::StateLand_OnEnter()
{
	m_networkHelper.SendRequest(ms_startLandId);
	m_networkHelper.WaitForTargetState(ms_OnExitLandId);

	CPed *pPed = GetPed();

	// Enable gravity
	pPed->SetUseExtractedZ(false);

	// No longer flying.
	pPed->GetMotionData()->SetIsNotFlying();

	// Clear the motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE we want to receive signals.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateLand_OnUpdate()
{
	CPed* pPed = GetPed();
	if (pPed->IsAPlayerPed())
	{
		SetNewGroundPitchForBird(*pPed, GetTimeStep());
	}

	// When the clip has completed we can transition straight through to idle
	if (m_bInTargetMotionState)
	{
		SetState(State_Idle);
	}

	return FSM_Continue;
}

void CTaskBirdLocomotion::StateLand_OnProcessMoveSignals()
{
	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateFly_OnEnter()
{
	// Don't send move signals when going from StateFlyDownwards -> StateFly, as there is not a corresponding MoVE state.
	m_networkHelper.SendRequest(ms_startFlyId);
	m_networkHelper.WaitForTargetState(ms_OnEnterFlyId);

	float fFlapTimeSeconds = fwRandom::GetRandomNumberInRange(sm_Tunables.m_TimeToFlapMin, sm_Tunables.m_TimeToFlapMax);

	if (GetPed()->IsAPlayerPed())
	{
		m_FlapTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_uPlayerFlapTimeMS);
	}
	else
	{
		m_FlapTimer.Set(fwTimer::GetTimeInMilliseconds(), rage::round(fFlapTimeSeconds * 1000.0f));
	}


	// Reset the cached motion variables.
	m_bInTargetMotionState = false;

	// Tell the MoVE network to send values to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateFly_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		if (m_bFlapRequested)
		{
			m_FlapTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_uPlayerFlapTimeMS);
			m_bFlapRequested = false;
		}

		if (m_bLandWhenAble)
		{
			m_bLandWhenAble = false;
			SetState(State_Land);
		}
		else if (!pPed->IsNetworkClone() && m_FlapTimer.IsOutOfTime())
		{
			SetState(State_Glide);
		}
	}
	else
	{
		CPedMotionData& motion = GetMotionData();
		if (!motion.GetIsFlying())
		{
			// Destination reached - try to land when you can.
			m_bLandWhenAble = true;
			SetState(State_Descend);
		}
		else if (motion.GetIsFlyingUpwards())
		{
			SetState(State_FlyUpwards);
		}
		else if (motion.GetIsFlyingDownwards())
		{
			// Flying downward but not landing.
			SetState(State_Descend);
		}
		else if (m_FlapTimer.IsOutOfTime())
		{
			// If we've been flapping for a minimum period, then we can start to glide for a bit.
			SetState(State_Glide);
		}
	}

	return FSM_Continue;
}

void CTaskBirdLocomotion::StateFly_OnProcessMoveSignals()
{
	// Send signals to MoVE
	SetNewHeading();

	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateGlide_OnEnter()
{
	m_networkHelper.SendRequest(ms_startGlideId);
	m_networkHelper.WaitForTargetState(ms_OnEnterGlideId);

	m_FlapTimer.Set(fwTimer::GetTimeInMilliseconds(), 6000);

	// Reset the cached motion state variables.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateGlide_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	CPedMotionData& motion = GetMotionData();
	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed())
	{
		if (m_bLandWhenAble)
		{	
			m_bLandWhenAble = false;
			SetState(State_Land);
		}
		else if (m_bFlapRequested)
		{
			m_bFlapRequested = false;
			SetState(State_Fly);
		}
	}
	else
	{
		if (motion.GetIsFlyingUpwards())
		{
			SetState(State_Fly);
		}
		else if (motion.GetIsFlyingDownwards())
		{
			SetState(State_Fly);
		}
		else if ((abs(motion.GetDesiredMbrY()) <= MOVEBLENDRATIO_STILL) ||
			(m_FlapTimer.IsOutOfTime()))
		{
			// Destination reached - transition via flapping forwards state
			// or, we've been gliding too long.
			SetState(State_Fly);
		}
	}


	return FSM_Continue;
}

void CTaskBirdLocomotion::StateGlide_OnProcessMoveSignals()
{
	// Send signals to MoVE
	SetNewHeading();

	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskBirdLocomotion::StateFlyUpwards_OnEnter()
{
	m_networkHelper.SendRequest(ms_startFlyUpwardsId);
	m_networkHelper.WaitForTargetState(ms_OnEnterFlyUpwardsId);
	m_fTurnPhase = 0.5f;

	// Clear the cached motion state values.
	m_bInTargetMotionState = false;

	// Clear the collision timer.
	m_fTimeWithContinuousCollisions = 0.0f;

	// Tell MoVE to start sending values to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskBirdLocomotion::StateFlyUpwards_OnUpdate()
{
	const float fTarget = GetMotionData().GetDesiredHeading();
	float fCurrent = GetMotionData().GetCurrentHeading();

	// Lerp towards the target heading slowly while ascending - there are no ascending turn animations.
	float fExtra = InterpValue(fCurrent, fTarget, 1.0f, true, GetTimeStep());

	GetMotionData().SetExtraHeadingChangeThisFrame(fExtra);

	// See if the bird is stuck when ascending and deal with it accordingly.
	if (!GetPed()->IsAPlayerPed())
	{
		CheckForCollisions();
	}

	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetIsFlyingForwards() || GetMotionData().GetIsFlyingDownwards() || !GetMotionData().GetIsFlying())
	{
		// Transition is through the flying state.
		SetState(State_Fly);
	}

	return FSM_Continue;
}

void CTaskBirdLocomotion::StateFlyUpwards_OnProcessMoveSignals()
{
	// Send signals to MoVE
	SetNewHeading();

	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::StateIdle_OnEnter()
{
	CPed *pPed = GetPed();
	
	// Enable gravity
	pPed->SetUseExtractedZ(false);

	m_networkHelper.SendRequest(ms_startIdleId);
	m_networkHelper.WaitForTargetState(ms_OnEnterIdleId);

	// Reset the cached motion state variables.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskBirdLocomotion::StateIdle_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	CPedMotionData& motion = GetMotionData();

	CPed* pPed = GetPed();

	// Different logic with players - they can take off without needing to be moving forwards.
	// This probably makes sense for non-player birds, but seems safer to keep the logic the same as it was now.
	if (pPed->IsAPlayerPed())
	{
		SetNewGroundPitchForBird(*pPed, GetTimeStep());

		if (motion.GetIsFlying())
		{
			SetState(State_WaitForTakeOff);
		}
		else if (abs(motion.GetDesiredMbrY()) > MOVEBLENDRATIO_STILL)
		{
			SetState(State_Walk);
		}
	}
	else
	{
		if (abs(motion.GetDesiredMbrY()) > MOVEBLENDRATIO_STILL)
		{
			if (motion.GetIsFlying() || motion.GetIsFlyingUpwards())
			{
				SetState(State_WaitForTakeOff);
			}
			else
			{
				SetState(State_Walk);
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskBirdLocomotion::StateIdle_OnProcessMoveSignals()
{
	// Store off signals from MoVE.
	m_bInTargetMotionState |= m_networkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

// Stay dead unless forced to some other state, in which case go to State_Initial to restart.
CTask::FSM_Return	CTaskBirdLocomotion::StateDead_OnUpdate()
{
	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead)
	{
		SetState(State_Initial);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskBirdLocomotion::AllowTimeslicing()
{
	CPed* pPed = GetPed();
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}


bool CTaskBirdLocomotion::ShouldForceNoTimeslicing()
{
	// Don't timeslice if you are a player.
	if (GetPed()->IsAPlayerPed())
	{
		return true;
	}

	// Don't timeslice if the heading diff is too great.
	float fDelta = CalcDesiredHeading();

	return fabs(fDelta) > sm_Tunables.m_ForceNoTimeslicingHeadingDiff * DtoR;
}

bool CTaskBirdLocomotion::IsBirdUnderwater() const
{
	const CPed* pPed = GetPed();
	return pPed->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame() && pPed->m_Buoyancy.GetSubmergedLevel() > 0.5f;
}

void CTaskBirdLocomotion::CheckForCollisions()
{
	const CCollisionRecord* pColRecord = GetPed()->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();

	if (pColRecord)
	{
		m_fTimeWithContinuousCollisions += GetTimeStep();
	}
	else
	{
		m_fTimeWithContinuousCollisions = 0.0f;
	}

	// Delete the bird if we can, it is stuck.
	if (ShouldIgnoreDueToContinuousCollisions())
	{
		PossiblyDeleteStuckBird();
	}
}

bool CTaskBirdLocomotion::ShouldIgnoreDueToContinuousCollisions()
{
	return m_fTimeWithContinuousCollisions >= sm_Tunables.m_TimeWhenStuckToIgnoreBird;
}

void CTaskBirdLocomotion::PossiblyDeleteStuckBird()
{	
	CPed* pPed = GetPed();

	// Check if the ped can be deleted
	const bool bDeletePedIfInCar = false; // prevent deletion in cars
	const bool bDoNetworkChecks = true; // do network checks (this is the default)
	if (pPed->CanBeDeleted(bDeletePedIfInCar, bDoNetworkChecks))
	{
		// Get handle to local player ped
		const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
		if (pLocalPlayerPed)
		{
			// Check distance to local player is beyond minimum threshold
			const ScalarV distToLocalPlayerSq_MIN = ScalarVFromF32(rage::square(sm_Tunables.m_MinDistanceFromPlayerToDeleteStuckBird));
			const ScalarV distToLocalPlayerSq = DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			if (IsGreaterThanAll(distToLocalPlayerSq, distToLocalPlayerSq_MIN))
			{
				// Check if ped is offscreen
				const bool bCheckNetworkPlayersScreens = true;
				if (!pPed->GetIsOnScreen(bCheckNetworkPlayersScreens))
				{
					m_fTimeOffScreenWhenStuck += GetTimeStep();

					if (m_fTimeOffScreenWhenStuck >= sm_Tunables.m_TimeUntilDeletionWhenStuckOffscreen)
					{
						pPed->FlagToDestroyWhenNextProcessed();
					}
				}
				else
				{
					// Ped is back onscreen.  Reset the timer.
					m_fTimeOffScreenWhenStuck = 0.0f;
				}
			}
			else
			{
				// Ped is too close.  Reset the timer.
				m_fTimeOffScreenWhenStuck = 0.0f;
			}
		}
	}
	else
	{
		// Ped can not be deleted.  Reset the timer.
		m_fTimeOffScreenWhenStuck = 0.0f;
	}
}

float CTaskBirdLocomotion::GetFlapSpeedScale() const
{
	const CPed* pPed = GetPed();

	// Acquire the scaling factor from the motion set.
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet, "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (Verifyf(pMotionStruct, "Invalid on foot motion task data set!"))
		{
			return pMotionStruct->m_PlayerBirdFlappingSpeedMultiplier;
		}
	}

	return 1.0f;
}

float CTaskBirdLocomotion::GetGlideSpeedScale() const
{
	const CPed* pPed = GetPed();

	// Acquire the scaling factor from the motion set.
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet, "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (Verifyf(pMotionStruct, "Invalid on foot motion task data set!"))
		{
			return pMotionStruct->m_PlayerBirdGlidingSpeedMultiplier;
		}
	}

	return 1.0f;
}


void CTaskBirdLocomotion::ForceStateChange( const fwMvStateId &targetStateId )
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), GetTaskName(), "%s ped %s forcing state change to main target state %s", AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), targetStateId.GetCStr());
	m_networkHelper.ForceStateChange(targetStateId);
} 

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskBirdLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Initial: return "Initial";
		case State_Idle: return "Idle";
		case State_Walk: return "Walk";
		case State_WaitForTakeOff: return "WaitForTakeoff";
		case State_TakeOff: return "TakeOff";
		case State_Land: return "Land";
		case State_Fly: return "Fly";
		case State_Glide: return "Glide";
		case State_FlyUpwards: return "FlyUpwards";
		case State_Descend: return "Descend";
		case State_Dead: return "Dead";
		default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskBirdLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
