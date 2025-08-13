#include "TaskQuadLocomotion.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "math/angmath.h"
#include "fwmaths/random.h"

#include "Debug/DebugScene.h"
#include "animation/MovePed.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedMotionData.h"
#include "Peds/PedIntelligence.h"
#include "Peds/QueriableInterface.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/System/MotionTaskData.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Quad Locomotion
//////////////////////////////////////////////////////////////////////////

// Instance of tunable task params
CTaskQuadLocomotion::Tunables CTaskQuadLocomotion::sm_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskQuadLocomotion, 0x3756fd16);

// States
const fwMvStateId CTaskQuadLocomotion::ms_LocomotionStateId("Locomotion",0xB65CBA9D);
const fwMvStateId CTaskQuadLocomotion::ms_IdleStateId("Idles",0xF29E80A8);
const fwMvStateId CTaskQuadLocomotion::ms_TurnInPlaceStateId("TurnInPlace",0x8F54E5AD);
const fwMvStateId CTaskQuadLocomotion::ms_NoClipStateId("NoClip",0x23B4087C);

const fwMvRequestId CTaskQuadLocomotion::ms_StartTurnInPlaceId("StartTurnInPlace",0x2E87794F);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterTurnInPlaceId("OnEnterTurnInPlace",0xA7F587A6);

const fwMvRequestId CTaskQuadLocomotion::ms_startIdleId("StartIdle",0x2A8F6981);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);
const fwMvFloatId CTaskQuadLocomotion::ms_IdleRateId("IdleRate", 0xDDE7C19C);

const fwMvRequestId CTaskQuadLocomotion::ms_StartLocomotionId("StartLocomotion",0x15F666A1);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterStartLocomotionId("OnEnterStartLocomotion",0x66DBAD36);
const fwMvBooleanId CTaskQuadLocomotion::ms_StartLocomotionDoneId("StartLocomotionDone",0xCC98ECCF);
const fwMvFloatId	CTaskQuadLocomotion::ms_StartLocomotionBlendDurationId("StartLocomotionBlendDuration",0x5D2D8FC2);
const fwMvFloatId	CTaskQuadLocomotion::ms_StartLocomotionBlendOutDurationId("StartLocomotionBlendOutDuration", 0xB71A195d);

const fwMvRequestId CTaskQuadLocomotion::ms_StopLocomotionId("StopLocomotion", 0x2F4D0B52);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterStopLocomotionId("OnEnterStopLocomotion", 0x41C0D9EC);
const fwMvBooleanId CTaskQuadLocomotion::ms_StopLocomotionDoneId("StopLocomotionDone", 0x7254FD55);
const fwMvFloatId CTaskQuadLocomotion::ms_StopPhaseId("StopPhase", 0xAD52AF79);

const fwMvRequestId CTaskQuadLocomotion::ms_LocomotionId("Locomotion",0xB65CBA9D);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterLocomotionId("OnEnterLocomotion",0x96273A49);

const fwMvRequestId CTaskQuadLocomotion::ms_StartNoClipId("StartNoClip",0xBC7581E7);
const fwMvBooleanId CTaskQuadLocomotion::ms_OnEnterNoClipId("OnEnterNoClip",0x4F96D612);

const fwMvFloatId CTaskQuadLocomotion::ms_DesiredSpeedId("DesiredSpeed",0xCF7BA842);
const fwMvFloatId CTaskQuadLocomotion::ms_BlendedSpeedId("BlendedSpeed",0x7C7E4E9C);
const fwMvFloatId CTaskQuadLocomotion::ms_MoveDirectionId("Direction",0x34DF9828);
const fwMvFloatId CTaskQuadLocomotion::ms_IntroPhaseId("IntroPhase",0xE8C2B9F8);

const fwMvFloatId CTaskQuadLocomotion::ms_MoveWalkRateId("WalkRate",0xE5FE5F4C);
const fwMvFloatId CTaskQuadLocomotion::ms_MoveTrotRateId("TrotRate",0xB89DC56F);
const fwMvFloatId CTaskQuadLocomotion::ms_MoveCanterRateId("CanterRate",0x8626FE39);
const fwMvFloatId CTaskQuadLocomotion::ms_MoveGallopRateId("GallopRate",0x406C4BF1);

const fwMvFlagId CTaskQuadLocomotion::ms_HasWalkStartsId("HasWalkStarts",0x17C4F953);
const fwMvFlagId CTaskQuadLocomotion::ms_HasRunStartsId("HasRunStarts",0x70FAB2E8);
const fwMvFlagId CTaskQuadLocomotion::ms_CanTrotId("CanTrot",0x2CD3E75C);

const fwMvFlagId CTaskQuadLocomotion::ms_LastFootLeftId("LastFootLeft",0x7A2EB41B);
const fwMvBooleanId CTaskQuadLocomotion::ms_CanEarlyOutForMovement("CAN_EARLY_OUT_FOR_MOVEMENT",0x7E1C8464);
const fwMvBooleanId CTaskQuadLocomotion::ms_BlendToLocomotion("BLEND_TO_LOCOMOTION", 0xb4d0cb5f);

CTaskQuadLocomotion::CTaskQuadLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId) 
: CTaskMotionBase()
, m_QuadGetupReqHelper()
, m_initialState(initialState)
, m_initialSubState(initialSubState)
, m_blendType(blendType)
, m_ClipSetId(clipSetId)
, m_fCurrentGait(0.0f)
, m_fNextGait(0.0f)
, m_fTurnPhase(0.0f)
, m_fPreviousTurnPhaseGoal(0.0f)
, m_fTurnApproachRate(0.0f)
, m_fOldMoveHeadingValue(0.0f)
, m_fTransitionCounter(0.0f)
, m_bTurnInPlaceIsBlendingOut(false)
, m_fTurnInPlaceInitialDirection(0.0f)
, m_fIdleDesiredHeading(0.0f)
, m_fLastIdleStateDuration(0.0f)
, m_fAnimPhase(0.0f)
, m_fTimeBetweenThrowingEvents(0.0f)
, m_fLocomotionGaitBlendValue(0.0f)
, m_fGaitWhenDecidedToStop(0.0f)
, m_fDesiredHeadingWhenReverseStarted(0.0f)
, m_fTimeReversing(0.0f)
, m_bPursuitMode(false)
, m_bUsingWalkStart(false)
, m_bUsingRunStart(false)
, m_bMovingForForcedMotionState(false)
, m_bInTargetMotionState(false)
, m_bStartLocomotionIsInterruptible(false)
, m_bStartLocomotionBlendOut(false)
, m_bReverseThisFrame(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ON_FOOT_QUAD);

	m_ClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
}

//////////////////////////////////////////////////////////////////////////

CTaskQuadLocomotion::~CTaskQuadLocomotion()
{

}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskQuadLocomotion::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	float fDt = GetTimeStep();

	static eNmBlendOutSet s_NullSet("null");

	// Stream in the getup clipsets.
	if (!m_QuadGetupReqHelper.IsLoaded() && pPed->GetPedModelInfo()->GetGetupSet().GetHash()!=s_NullSet.GetHash())
	{
		m_QuadGetupReqHelper.Request(pPed->GetPedModelInfo()->GetGetupSet());
	}

	if (GetIsUsingPursuitMode())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
	}

	// For humans with AL_LodMotionTask, CTaskMotionBasicLocomotionLowLod would be running
	// and enable timeslicing (unblocking AL_LodTimesliceIntelligenceUpdate). For animals,
	// we don't use that task, so we manually turn on timeslicing here.
	if (CanBeTimesliced())
	{
		CPedAILod& lod = pPed->GetPedAiLod();
		lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}

	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		// Update foot down flags
		if(m_MoveNetworkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelLId))
		{
			m_MoveNetworkHelper.SetFlag(true, ms_LastFootLeftId);
		}
		else if(m_MoveNetworkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelRId))
		{
			m_MoveNetworkHelper.SetFlag(false, ms_LastFootLeftId);
		}
	}

	// Add shocking events.
	if (pPed->GetPedModelInfo()->GetPersonalitySettings().GetIsDangerousAnimal() && !pPed->IsInjured())
	{
		// Only throw events for this animal periodically for performance reasons.
		m_fTimeBetweenThrowingEvents += GetTimeStep();
		if (m_fTimeBetweenThrowingEvents > 2.0f) // MAGIC
		{
			m_fTimeBetweenThrowingEvents = 0.0f;
			CEventShockingDangerousAnimal ev(*pPed);
			CShockingEventsManager::Add(ev);
		}
	}

	// Lerp towards the desired move rate override set by script - use the same rate here as human peds.
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplier, 2.0, 1.0f, 20.0f, 0.5f);
	float fDeltaOverride = pPed->GetMotionData()->GetDesiredMoveRateOverride() - pPed->GetMotionData()->GetCurrentMoveRateOverride();
	fDeltaOverride *= (fTendToDesiredMoveRateOverrideMultiplier * fDt);
	fDeltaOverride += pPed->GetMotionData()->GetCurrentMoveRateOverride();
	pPed->GetMotionData()->SetCurrentMoveRateOverride(fDeltaOverride);

	float fCurrentMbrX, fCurrentMbrY;
	GetMotionData().GetCurrentMoveBlendRatio(fCurrentMbrX, fCurrentMbrY);

	float fDesiredMbrX, fDesiredMbrY;
	GetMotionData().GetGaitReducedDesiredMoveBlendRatio(fDesiredMbrX, fDesiredMbrY);

	// Interpolate the current towards the desired.
	const float fRate = GetTunables().m_MovementAcceleration;
	InterpValue(fCurrentMbrX, fDesiredMbrX, fRate, false, fDt);
	InterpValue(fCurrentMbrY, fDesiredMbrY, fRate, false, fDt);

	// Clamp the current MBR if on a slope.
	static const float sf_SlopeNormal = 0.71f;
	static const float sf_MinSlopeMBR = 2.3f;
	float fGroundNormalZ = pPed->GetGroundNormal().GetZ();
	if(fGroundNormalZ > 0.f && fGroundNormalZ < sf_SlopeNormal)
	{
		fCurrentMbrY = Min(fCurrentMbrY, sf_MinSlopeMBR);
	}

	// Process reversing out of bad situations - player only.
	if (pPed->IsAPlayerPed())
	{
		bool bWasReversing = m_bReverseThisFrame;
		bool bAnimalCanReverseManually = true;
		if (pPed->GetArchetype()->GetModelNameHash() == 0xDFB55C81) //a_c_rabbit_01
			bAnimalCanReverseManually = false;
			
		// Player can trigger reversing
		CControl* pControl = pPed->GetControlFromPlayer();
		if (pControl && pControl->GetQuadLocoReverse().IsReleased())
		{
			m_bReverseThisFrame = false;
		} // This isn't an else if intentionally, so that reverse is still enabled if ped is blocked in front

		if (bAnimalCanReverseManually && pControl && pControl->GetQuadLocoReverse().IsDown() && fCurrentMbrY <= MOVEBLENDRATIO_WALK)
		{
			m_bReverseThisFrame = true;
		}
		// If you are stuck on both sides, then reverse.
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_MoverConstrictedByOpposingCollisions) && IsBlockedInFront())
		{
			m_bReverseThisFrame = true;
		}
		else if (m_fTimeReversing > sm_Tunables.m_ReversingTimeBeforeAllowingBreakout && (fCurrentMbrY <= SMALL_FLOAT
			|| fabs(SubtractAngleShorter(pPed->GetDesiredHeading(), m_fDesiredHeadingWhenReverseStarted)) > DtoR * sm_Tunables.m_ReversingHeadingChangeDegreesForBreakout))
		{
			// We only stop reversing when the player lets go of the stick (mbr change) or moves the stick (desired heading change).
			// But only after a second straight of being stuck.
			m_bReverseThisFrame = false;
		}

		// Note the the heading when the ped started to reverse.
		if (m_bReverseThisFrame && !bWasReversing)
		{
			m_fDesiredHeadingWhenReverseStarted = pPed->GetDesiredHeading();
		}

		// Also limit the MBR to a walk as it looks really weird to see boars galloping backwards...
		if (m_bReverseThisFrame)
		{
			fCurrentMbrY = Min(fCurrentMbrY, MOVEBLENDRATIO_WALK);
			m_fTimeReversing += fDt;
		}
		else
		{
			m_fTimeReversing = 0.0f;
		}
	}


	GetMotionData().SetCurrentMoveBlendRatio(fCurrentMbrY, fCurrentMbrX);

	UpdateGaitReduction();

	// Start locomotion is always either considered fully in walk or fully in sprint.
	// Override the values set above.
	if (m_bUsingWalkStart)
	{
		m_fCurrentGait = MOVEBLENDRATIO_WALK;
		m_fNextGait = MOVEBLENDRATIO_WALK;
		GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK, 0.0f);
	}
	else if (m_bUsingRunStart)
	{
		m_fCurrentGait = MOVEBLENDRATIO_SPRINT;
		m_fNextGait = MOVEBLENDRATIO_SPRINT;
		GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT, 0.0f);
	}

	if (ShouldForceNoTimeslicing())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	return FSM_Continue;
}

// Can only be timesliced if in low lod motion.
bool CTaskQuadLocomotion::CanBeTimesliced() const
{
	// Check if an AI update needs to be forced.
	if (CPedAILodManager::IsTimeslicingEnabled())
	{
		return GetPed()->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask);
	}

	return false;
}

// Refuse to be timesliced if the heading delta is too large.
bool CTaskQuadLocomotion::ShouldForceNoTimeslicing() const
{
	// Check if an AI update needs to be forced.
	if (CPedAILodManager::IsTimeslicingEnabled())
	{
		float fTarget = CalcDesiredDirection();

		// Check how far away the quad is from facing their target.
		if (fabs(fTarget) > GetTunables().m_DisableTimeslicingHeadingThresholdD * DtoR)
		{
			return true;
		}
	}

	return false;
}

bool CTaskQuadLocomotion::IsInWaterWhileLowLod() const
{
	const CPed* pPed = GetPed();
	return pPed->GetIsSwimming() && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskQuadLocomotion::IsBlockedInFront() const
{
	const CPed* pPed = GetPed();
	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();

	if (!Verifyf(pCapsuleInfo && pCapsuleInfo->GetQuadrupedCapsuleInfo(), "No valid capsule info found for quadruped."))
	{
		return false;
	}

#if __BANK
	TUNE_GROUP_BOOL(QUAD_LOCO, bRenderBlockedFrontTests, false);
#endif

	const CQuadrupedCapsuleInfo* pQuadCapsuleInfo = pCapsuleInfo->GetQuadrupedCapsuleInfo();

	// Slightly expand the radius.
	const static float sf_CapsuleRadiusFactor = 1.1f;

	Vec3V vPosition = pPed->GetTransform().GetPosition();
	Vec3V vForward = pPed->GetTransform().GetForward();
	float fRadius = pCapsuleInfo->GetHalfWidth() * sf_CapsuleRadiusFactor;
	float fLength = pQuadCapsuleInfo->GetMainBoundLength() / 1.5f;

	//Check it's a player, and it's a large animal (Cow or Deer) then increase the capsule size for shapetests
	if (pPed->GetPedType() == PEDTYPE_ANIMAL && pPed->IsAPlayerPed() && (pPed->GetModelIndex() == MI_PED_ANIMAL_COW || pPed->GetModelIndex() == MI_PED_ANIMAL_DEER))
	{
		fLength = pQuadCapsuleInfo->GetMainBoundLength() * 1.2f;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestResults probeResults;
	capsuleDesc.SetResultsStructure(&probeResults);
	// From the middle of the ped to 1/4 the length of the capsule along the forward vector.
	// This seems to work OK in practice for rejecting false positives.
	Vec3V vEndPosition = vPosition + (vForward * ScalarV(fLength));
	capsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vPosition), VEC3V_TO_VECTOR3(vEndPosition), fRadius);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
		WorldProbe::ResultIterator it;
		for(it=probeResults.begin(); it < probeResults.last_result(); it++)
		{
			const CEntity* pHitEntity = it->GetHitEntity();
			// The hit entity was of type object, check and see if it is is small enough to be ignored.
			if (pHitEntity && pHitEntity->GetIsTypeObject()
				&& static_cast<const CObject*>(pHitEntity)->GetShouldBeIgnoredForConstrictingCollisionChecks())
			{
				continue;
			}
			else
			{
#if __BANK
				if (bRenderBlockedFrontTests)
					grcDebugDraw::Capsule(vPosition, vEndPosition, fRadius, Color_red, false, -1);
#endif

				return true;
			}
		}
	}

#if __BANK
	if (bRenderBlockedFrontTests)
		grcDebugDraw::Capsule(vPosition, vPosition + (vForward * ScalarV(fLength)), fRadius, Color_green, false, -1);
#endif

	return false;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskQuadLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

		FSM_State(State_StartLocomotion)
			FSM_OnEnter
				StateStartLocomotion_OnEnter();
			FSM_OnUpdate
				return StateStartLocomotion_OnUpdate();
			FSM_OnExit
				StateStartLocomotion_OnExit();

		FSM_State(State_StopLocomotion)
			FSM_OnEnter
				StateStopLocomotion_OnEnter();
			FSM_OnUpdate
				return StateStopLocomotion_OnUpdate();

		FSM_State(State_Locomotion)
			FSM_OnEnter
				StateLocomotion_OnEnter(); 
			FSM_OnUpdate
				return StateLocomotion_OnUpdate(); 

		FSM_State(State_TurnInPlace)
			FSM_OnEnter
				StateTurnInPlace_OnEnter();
			FSM_OnUpdate
				return StateTurnInPlace_OnUpdate();

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

// PURPOSE: Manipulate the animated velocity to achieve more precise turning than the animation alone would allow.
bool	CTaskQuadLocomotion::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	s32 iState = GetState();
	CPed* pPed = GetPed();
	float fRotateSpeed = pPed->GetDesiredAngularVelocity().z;
	const float fDesiredDirection = CalcDesiredDirection();
	float fDesiredHeading = pPed->GetDesiredHeading();
	float fCurrentHeading = pPed->GetCurrentHeading();

	// Compute the "ideal" angular velocity - what would get us to the desired heading this frame.
	const float fDesiredAngularVelocity = fDesiredDirection / fTimeStep;

	if (iState == State_Idle && GetPreviousState() == State_TurnInPlace)
	{
		// Note if the turn is blending out.
		// The IgnoreMoverBlendRotationOnly_filter seems to be causing pops with quadrupeds, so we are hacking this in code.
		if (m_bTurnInPlaceIsBlendingOut && !pPed->GetIsAttached())
		{
			fRotateSpeed = 0.0f;
			pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));
			pPed->SetHeading(fDesiredHeading);
			return true;
		}
	}
	else if (iState == State_TurnInPlace)
	{
		float fCurrentHeadingAfterThisFrame = fwAngle::LimitRadianAngleSafe((fRotateSpeed * fTimeStep) + pPed->GetCurrentHeading());
		float fPredictedDesiredDirection = SubtractAngleShorter(fDesiredHeading, fCurrentHeadingAfterThisFrame);

		// If about to overshoot the desired heading, set the heading to be the desired heading.
		if (Sign(fDesiredDirection) != Sign(fPredictedDesiredDirection)  && fabs(fDesiredDirection - fPredictedDesiredDirection) < HALF_PI  && !pPed->GetIsAttached())
		{
			pPed->SetHeading(fDesiredHeading);
			fRotateSpeed = 0.0f;

			// Force the angular velocity to be the new value calculated above.
			pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));

			return true;
		}
	}
	else if (iState == State_Locomotion || iState == State_StartLocomotion)
	{
		// Figure out how much velocity is needed in the clip to do tighter turning.
		float fVelocityTolerance = sm_Tunables.m_InMotionAlignmentVelocityTolerance;
		
		// When sprinting and other tasks have noted that tighter turning is desired, enforce a lower velocity tolerance than normal.
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettingsForScript))
		{
			// Only when sprinting.
			if (m_fCurrentGait == MOVEBLENDRATIO_SPRINT)
			{
				fVelocityTolerance = sm_Tunables.m_InMotionTighterTurnsVelocityTolerance;
			}
		}

		// If there is enough turning already going on in the clip, then we can probably get away with adding a bit more to achieve tighter turns.
		// Or, player peds always have their velocity scaled.
		if (fabs(fRotateSpeed) > fVelocityTolerance || pPed->IsAPlayerPed())
		{
			// Check if the turn velocity should be scaled to achieve tighter turning.
			if (ShouldScaleTurnVelocity(fDesiredDirection, fRotateSpeed))
			{
				const float fScaleFactor = GetTurnVelocityScaleFactor();
				fRotateSpeed = fScaleFactor * fRotateSpeed;
			}

			// Lerp towards the desired angular velocity from the extracted angular velocity.
			float fRate = GetIsUsingPursuitMode() ? sm_Tunables.m_PursuitModeExtraHeadingRate : sm_Tunables.m_ProcessPhysicsApproachRate;
			Approach(fRotateSpeed, fDesiredAngularVelocity, fRate, fTimeStep);
		}

		float fCurrentHeadingAfterThisFrame = fwAngle::LimitRadianAngleSafe((fRotateSpeed * fTimeStep) + fCurrentHeading);
		float fPredictedDesiredDirection = SubtractAngleShorter(fDesiredHeading, fCurrentHeadingAfterThisFrame);

		// If about to overshoot the desired heading, set the heading to be the desired heading.
		// Not 100% sure why, but this calculation seems to be busted when going down hills.  The quads noticeably snap their heading.
		// Probably the safest thing to do is not to allow this heading forcing for the player.  It was meant to prevent overcorrection
		// while navigating, but while the ped is under player control that shouldn't really be an issue anyway.
		if (Sign(fDesiredDirection) != Sign(fPredictedDesiredDirection) && fabs(fDesiredDirection - fPredictedDesiredDirection) < HALF_PI  && !pPed->GetIsAttached() && !pPed->IsAPlayerPed())
		{
			pPed->SetHeading(fDesiredHeading);
			fRotateSpeed = 0.0f;
		}
		
		// Force the angular velocity to be the new value calculated above.
		pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));

		return true;
	}

	return false;
}

bool CTaskQuadLocomotion::ShouldScaleTurnVelocity(float fDesiredHeadingChange, float fAnimationRotateSpeed) const
{
	// Only scale while moving.
	if (GetState() != State_Locomotion)
	{
		return false;
	}

	// Only scale in the direction the ped needs to turn.
	return Sign(fDesiredHeadingChange) == Sign(fAnimationRotateSpeed);
}

float CTaskQuadLocomotion::GetTurnVelocityScaleFactor() const
{
	float fMBRY = GetMotionData().GetCurrentMbrY();
	float fWalkScale = 1.0f;
	float fRunScale = 1.0f;
	float fSprintScale = 1.0f;
	
	const CPed* pPed = GetPed();
	
	// Aquire the scaling factor from the motion set.
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (Verifyf(pMotionStruct, "Invalid on foot motion task data set!"))
		{
			fWalkScale = pPed->IsAPlayerPed() ? pMotionStruct->m_QuadWalkTurnScaleFactorPlayer : pMotionStruct->m_QuadWalkTurnScaleFactor;
			fRunScale = pPed->IsAPlayerPed() ? pMotionStruct->m_QuadRunTurnScaleFactorPlayer : pMotionStruct->m_QuadRunTurnScaleFactor;
			fSprintScale = pPed->IsAPlayerPed() ? pMotionStruct->m_QuadSprintTurnScaleFactorPlayer : pMotionStruct->m_QuadSprintTurnScaleFactor;
		}
	}

	// Check which move blend ratio is in use and use that scaling factor.
	if (fMBRY <= MOVEBLENDRATIO_WALK)
	{
		return fWalkScale;
	}
	else if (fMBRY <= MOVEBLENDRATIO_RUN)
	{
		return fRunScale;
	}
	else
	{
		return fSprintScale;
	}
}

bool CTaskQuadLocomotion::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_Initial:
		{
			return true;
		}
		case State_Idle:
		{
			StateIdle_OnProcessMoveSignals();
			return true;
		}
		case State_Locomotion:
		{
			StateLocomotion_OnProcessMoveSignals();
			return true;
		}
		case State_StartLocomotion:
		{
			StateStartLocomotion_OnProcessMoveSignals();
			return true;
		}
		case State_StopLocomotion:
		{
			StateStopLocomotion_OnProcessMoveSignals();
			return true;
		}
		case State_TurnInPlace:
		{
			StateTurnInPlace_OnProcessMoveSignals();
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
CTask * CTaskQuadLocomotion::CreatePlayerControlTask()
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::CleanUp()
{
	if (m_MoveNetworkHelper.GetNetworkPlayer())
	{
		m_MoveNetworkHelper.ReleaseNetworkPlayer();
	}

	if (m_QuadGetupReqHelper.IsLoaded())
	{
		m_QuadGetupReqHelper.Release();
	}

	ResetGaitReduction();
}

//////////////////////////////////////////////////////////////////////////

void CTaskQuadLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds)
{
	taskAssert(m_ClipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	if (m_ClipSet)
	{
		static const fwMvClipId s_walkClip("walk",0x83504C9C);
		static const fwMvClipId s_runClip("canter",0x1501A19F);
		static const fwMvClipId s_sprintClip("gallop",0xD58AD8A6);

		const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

		RetrieveMoveSpeeds(*m_ClipSet, speeds, clipNames);
	}
}

// TODO - make this dependent on the stop locomotion clip.
float CTaskQuadLocomotion::GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* bOutIsClipActive)
{
	if (HasQuickStops())
	{
		if (bOutIsClipActive)
		{
			*bOutIsClipActive = GetState() == State_StopLocomotion;
		}

		float fGait = m_fCurrentGait;
		if (fGait <= MOVEBLENDRATIO_WALK)
		{
			return GetTunables().m_StoppingDistanceWalkMBR;
		}
		else if (fGait <= MOVEBLENDRATIO_RUN)
		{
			return GetTunables().m_StoppingDistanceRunMBR;
		}
		else
		{
			return GetTunables().m_StoppingDistanceGallopMBR;
		}
	}
	else
	{
		if (bOutIsClipActive)
		{
			*bOutIsClipActive = false;
		}
		return 0.1f; // HACK - without a stop clip the quad just blends straight to idle.
	}
}

bool CTaskQuadLocomotion::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	switch(state)
	{
		case CPedMotionStates::MotionState_Idle:
			return GetState() == State_Idle;
		case CPedMotionStates::MotionState_Walk:
			return (GetState() == State_Locomotion || GetState() == State_StartLocomotion) && GetMotionData().GetIsWalking();
		case CPedMotionStates::MotionState_Run:
			return (GetState() == State_Locomotion || GetState() == State_StartLocomotion) && GetMotionData().GetIsRunning();
		case CPedMotionStates::MotionState_Sprint:
			return (GetState() == State_Locomotion || GetState() == State_StartLocomotion) && GetMotionData().GetIsSprinting();
		case CPedMotionStates::MotionState_Dead:
			return GetState() == State_Dead;
		case CPedMotionStates::MotionState_DoNothing:
			return GetState() == State_DoNothing;
		default:
			return false;
	}
}

void CTaskQuadLocomotion::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
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

void CTaskQuadLocomotion::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	static bank_float sf_PitchLimitTuning = 1.0f;
	fMinOut = -sf_PitchLimitTuning;
	fMaxOut =  sf_PitchLimitTuning;
}

bool CTaskQuadLocomotion::IsInMotion(const CPed* UNUSED_PARAM(pPed)) const
{
	switch(GetState())
	{
		case State_TurnInPlace:
		case State_Locomotion:
		case State_StartLocomotion:
		case State_StopLocomotion:
			return true;
		default:
			return false;
	}
}

bool CTaskQuadLocomotion::ShouldDisableSlowingDownForCornering() const
{
	return GetState()== CTaskQuadLocomotion::State_Initial 
	|| GetState() == CTaskQuadLocomotion::State_Idle 
	|| GetState() == CTaskQuadLocomotion::State_StartLocomotion
	|| GetState() == CTaskQuadLocomotion::State_TurnInPlace;
}

// Interpolate towards the desired direction.
void CTaskQuadLocomotion::ProcessTurnPhase()
{
	const float fDT = GetTimeStep();
	float fTarget = CalcDesiredDirection();

	const float fMBR = GetMotionData().GetCurrentMbrY();
	const float fMinTurnApproachRate = GetMinTurnRate(fMBR);
	const float fIdealTurnApproach = GetIdealTurnRate(fMBR);
	const float fTurnAcceleration = GetTurnAcceleration(fMBR);
	
	// If a direction change occurs, go back to the min approach rate.
	if (fabs(SubtractAngleShorter(m_fPreviousTurnPhaseGoal, fTarget)) > sm_Tunables.m_TurnResetThresholdD * DtoR)
	{
		if (Sign(m_fPreviousTurnPhaseGoal) != Sign(fTarget))
		{
			m_fTurnApproachRate = fMinTurnApproachRate;
		}
	}

	// Reset the previous turn target.
	m_fPreviousTurnPhaseGoal = fTarget;
		
	// Lerp towards the target heading approach rate.
	Approach(m_fTurnApproachRate, fIdealTurnApproach, fTurnAcceleration, fDT);

	// Lerp towards the target heading.
	InterpValue(m_fTurnPhase, fTarget, m_fTurnApproachRate, false, fDT);

	// Limit it from -pi to pi
	m_fTurnPhase = Clamp(m_fTurnPhase, -PI, PI);
}

float CTaskQuadLocomotion::GetMinTurnRate(float fMBR) const
{
	if (fMBR < sm_Tunables.m_TurnSpeedMBRThreshold)
	{
		return sm_Tunables.m_SlowMinTurnApproachRate;
	}
	else
	{
		return sm_Tunables.m_FastMinTurnApproachRate;
	}
}

float CTaskQuadLocomotion::GetIdealTurnRate(float fMBR) const
{
	if (fMBR < sm_Tunables.m_TurnSpeedMBRThreshold)
	{
		return sm_Tunables.m_SlowTurnApproachRate;
	}
	else
	{
		return sm_Tunables.m_FastTurnApproachRate;
	}
}

float CTaskQuadLocomotion::GetTurnAcceleration(float fMBR) const
{
	if (fMBR < sm_Tunables.m_TurnSpeedMBRThreshold)
	{
		return sm_Tunables.m_SlowTurnAcceleration;
	}
	else
	{
		return sm_Tunables.m_FastTurnAcceleration;
	}
}

void CTaskQuadLocomotion::ProcessGaits()
{
	const float fDT = GetTimeStep();
	const float fCurrentMBR = GetMotionData().GetCurrentMbrY();	

	m_fLocomotionGaitBlendValue = m_fCurrentGait;

	if (fabs(m_fTransitionCounter) <= SMALL_FLOAT)
	{
		m_fNextGait = Clamp((float)rage::round(fCurrentMBR), m_fCurrentGait - 1.0f, m_fCurrentGait + 1.0f);
	}

	// Transitioning to the next gait.
	if (m_fNextGait != m_fCurrentGait) 
	{
		// Decide how long the transition is going to take.
		float fTimeBetweenGaits = rage::Max(VERY_SMALL_FLOAT, GetTimeBetweenGaits(m_fCurrentGait, m_fNextGait));

		// Update transition counter.
		m_fTransitionCounter = Min(fTimeBetweenGaits, m_fTransitionCounter + fDT);

		// Blended speed is difference in the gaits multiplied by the ratio of how far along we are in the transition.
		m_fLocomotionGaitBlendValue += ((m_fNextGait - m_fCurrentGait) * (m_fTransitionCounter / fTimeBetweenGaits));

		// Done with transitioning, swap to the next gait.
		if (m_fTransitionCounter >= fTimeBetweenGaits)
		{
			m_fCurrentGait = m_fNextGait;
			m_fTransitionCounter = 0.0f;
		}
	}
	else
	{
		m_fTransitionCounter = 0.0f;
	}
}

void CTaskQuadLocomotion::SendLocomotionMoveSignals()
{
	CPed* pPed = GetPed();
	const float fCurrentMBR = GetMotionData().GetCurrentMbrY();	

	float fBlendedSpeed = m_fLocomotionGaitBlendValue;

	// Respect the script override.
	float fScriptMoveOverride = pPed->GetMotionData()->GetCurrentMoveRateOverride();
	float fWalkRate = fScriptMoveOverride;
	float fCanterRate = fScriptMoveOverride;
	float fGallopRate = fScriptMoveOverride;

	// Check if the quad is chasing down a target and needs to get there fast.
	if (GetIsUsingPursuitMode() && m_fCurrentGait == MOVEBLENDRATIO_SPRINT)
	{
		fGallopRate *= GetTunables().m_PursuitModeGallopRateFactor;
	}

	// Rate params for the different discrete MBRs to smooth out the transition.
	// Clamped to not adjust more than 30%.
	fWalkRate *= Clamp(fCurrentMBR, 0.7f, 1.3f);
	fCanterRate *= Clamp(fCurrentMBR - 1.0f, 0.7f, 1.3f);
	fGallopRate *= Clamp(fCurrentMBR - 2.0f, 0.7f, 1.0f);


	// Passing in negative rates will make the animation play in reverse.
	if (pPed->IsAPlayerPed() && m_bReverseThisFrame && GetState() != State_StartLocomotion)
	{
		fWalkRate = -1.0f;
		fCanterRate = -1.0f;
		fGallopRate = -1.0f;
	}

	// Send the blended speed to MoVE (expects it as a float from 0-1).
	fBlendedSpeed /= MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_BlendedSpeedId, fBlendedSpeed);

	// Send the rate signals to MoVE.
	m_MoveNetworkHelper.SetFloat(ms_MoveWalkRateId, fWalkRate);
	m_MoveNetworkHelper.SetFloat(ms_MoveCanterRateId, fCanterRate);
	m_MoveNetworkHelper.SetFloat(ms_MoveGallopRateId, fGallopRate);

	m_MoveNetworkHelper.SetFloat(ms_MoveDirectionId, m_fOldMoveHeadingValue);
}

void CTaskQuadLocomotion::SendStopLocomotionMoveSignals()
{
	float fBlendedSpeed = m_fGaitWhenDecidedToStop;
	fBlendedSpeed /= MOVEBLENDRATIO_SPRINT;

	m_MoveNetworkHelper.SetFloat(ms_BlendedSpeedId, fBlendedSpeed);
}

void CTaskQuadLocomotion::SendTurnSignals(float fTurnPhase)
{
	// Clamp it to a valid range.
	float fMoveHeadingValue = Clamp(fTurnPhase, -PI, PI);

	// Convert it from [0,1] for MoVE.
	fMoveHeadingValue = (fMoveHeadingValue/PI + 1.0f)/2.0f;

	// Send the signal.
	m_MoveNetworkHelper.SetFloat(ms_MoveDirectionId, fMoveHeadingValue); 
}

// Apply extra steering when in low lod locomotion
void CTaskQuadLocomotion::ApplyExtraHeadingAdjustments()
{
	CPed* pPed = GetPed();

	// Process extra heading adjustments.
	float fDesiredHeading = pPed->GetDesiredHeading();
	float fCurrentHeading = pPed->GetCurrentHeading();
	float fExtraHeadingChange = InterpValue(fCurrentHeading, fDesiredHeading, GetTunables().m_LowLodExtraHeadingAdjustmentRate, true, GetTimeStep());
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fExtraHeadingChange);
}


//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::StateInitial_OnEnter()
{
	// request the move network
	m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootQuad);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskQuadLocomotion::ShouldBeMoving()
{
	if (m_bMovingForForcedMotionState)
	{
		return true;
	}

	// Kind of a hack, but when this flag is being used we generally want to leave the idle state as soon as possible.
	// Otherwise the animal won't respond quickly.
	// There's no danger of flip-flopping between idle and turn in place in this case, because idle turns are 
	// explicitly prohibited.
	bool bIgnoreTimeInStateCheck = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_BlockQuadLocomotionIdleTurns);

	// Commit to being in a motionless state for at least a while before resuming movement.
	if (!bIgnoreTimeInStateCheck && GetTimeInState() < 0.5f)
	{
		return false;
	}

	return GetMotionData().GetGaitReducedDesiredMbrY() > 0.0f;
}

bool CTaskQuadLocomotion::ShouldStopMoving()
{
	if (m_bMovingForForcedMotionState)
	{
		return false;
	}

	if (GetMotionData().GetGaitReducedDesiredMbrY() < 0.1f)
	{
		const CTaskMoveGoToPointAndStandStill* pGototask = CTaskHumanLocomotion::GetGotoPointAndStandStillTask(GetPed());
		if (pGototask)
		{
			const float fStopDistance = GetTunables().m_StoppingGotoPointRemainingDist;
			const float fDistanceRemaining = pGototask->GetDistanceRemaining();
			if (fDistanceRemaining < fStopDistance)
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskQuadLocomotion::StateInitial_OnUpdate()
{
	taskAssert(m_ClipSet); 

	if (m_ClipSet && m_ClipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootQuad))
	{	

		CPed* pPed = GetPed();

		if (!m_MoveNetworkHelper.IsNetworkActive())
		{
			// Create the network player
			Assert(pPed->GetAnimDirector());
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootQuad);
			m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
		}

		if (m_MoveNetworkHelper.IsNetworkActive())
		{
			CMovePed& move = pPed->GetMovePed();
			move.SetMotionTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f);

			m_MoveNetworkHelper.SetFlag(HasWalkStarts(), ms_HasWalkStartsId);
			m_MoveNetworkHelper.SetFlag(HasRunStarts(), ms_HasRunStartsId);
			m_MoveNetworkHelper.SetFlag(CanTrot(), ms_CanTrotId);

			RequestProcessMoveSignalCalls();

			switch(GetMotionData().GetForcedMotionStateThisFrame())
			{
			case CPedMotionStates::MotionState_Idle:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId);
					GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
					SetState(State_Idle);
					break;
				}
			case CPedMotionStates::MotionState_Walk:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_LocomotionStateId);
					GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_WALK);
					m_fCurrentGait = MOVEBLENDRATIO_WALK;
					m_bMovingForForcedMotionState = true;
					SetState(State_Locomotion);
					break;
				}
			case CPedMotionStates::MotionState_Run:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_LocomotionStateId);
					GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_RUN);
					m_fCurrentGait = MOVEBLENDRATIO_RUN;
					m_bMovingForForcedMotionState = true;
					SetState(State_Locomotion);
					break;
				}
			case CPedMotionStates::MotionState_Sprint:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_LocomotionStateId);
					GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
					GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
					m_fCurrentGait = MOVEBLENDRATIO_SPRINT;
					m_bMovingForForcedMotionState = true;
					SetState(State_Locomotion);
					break;
				}
			case CPedMotionStates::MotionState_Dead:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_NoClipStateId);
					SetState(State_Dead);
					break;
				}
			case CPedMotionStates::MotionState_DoNothing:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_NoClipStateId);
					SetState(State_DoNothing);
					break;
				}
			default:
				{
					m_MoveNetworkHelper.ForceStateChange(ms_IdleStateId);
					SetState(State_Idle);
					break;
				}
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::StateStartLocomotion_OnEnter()
{
	CPed* pPed = GetPed();

	// Cache off the desired direction, so we will know if it deviates too much and this state needs to blend out into normal movement.
	m_fLocomotionStartInitialDirection = pPed->GetDesiredHeading();

	// Compute the heading blend in value for the walkstart.
	float fMoveHeadingValue = CalcDesiredDirection();

	// Clamp it to a valid range.
	fMoveHeadingValue = Clamp(fMoveHeadingValue, -PI, PI);
	
	// Convert it from [0,1] for MoVE.
	fMoveHeadingValue = (fMoveHeadingValue/PI + 1.0f)/2.0f;

	m_fOldMoveHeadingValue = fMoveHeadingValue;
	
	// Send the signal.
	m_MoveNetworkHelper.SetFloat(ms_MoveDirectionId, fMoveHeadingValue);

	// Determine what gait the ped will be in when finishing this walk/run start.
	float fDesiredMBR = GetMotionData().GetDesiredMbrY();

	// The gait of the quadruped is either walking or running after beginning the locomotion start animation.
	if (fDesiredMBR >= sm_Tunables.m_StartLocomotionWalkRunBoundary)
	{
		m_bUsingRunStart = true;
		fDesiredMBR = MOVEBLENDRATIO_SPRINT;
	}
	else
	{
		m_bUsingWalkStart = true;
		fDesiredMBR = MOVEBLENDRATIO_WALK;
	}

	// Send the desired speed to MoVE as a signal [0,1]
	fDesiredMBR /= MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_DesiredSpeedId, fDesiredMBR);

	// Transition to the next MoVE state.
	m_MoveNetworkHelper.SendRequest(ms_StartLocomotionId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterStartLocomotionId);

	static dev_float sf_MinTimeInIdleForShortBlend = 0.5f;

	float fBlendDuration =  m_fLastIdleStateDuration > sf_MinTimeInIdleForShortBlend ? sm_Tunables.m_StartLocomotionDefaultBlendDuration : sf_MinTimeInIdleForShortBlend;

	m_MoveNetworkHelper.SetFloat(ms_StartLocomotionBlendDurationId, fBlendDuration);

	// Reset the interruptible tag found in ProcessMoveSignals for this state.
	m_bStartLocomotionIsInterruptible = false;

	// Reset the blendout tag found in ProcessMoveSignals for this state.
	m_bStartLocomotionBlendOut = false;

	// Reset cached motion state values.
	m_bInTargetMotionState = false;
	m_fAnimPhase = 0.0f;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskQuadLocomotion::StateStartLocomotion_OnUpdate()
{
	// Calculate the values to send to MoVE.
	ProcessGaits();
	ProcessTurnPhase();

	// Send animation blend information to MoVE.
	SendLocomotionMoveSignals();

	// Wait on MoVE before possibly transitioning to a new state.
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if (pPed->IsAPlayerPed() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_MoverConstrictedByOpposingCollisions))
	{
		// When we realize we are stuck we need to get out of the startlocomotion state.
		// You can't play the walkstarts in reverse.  The blend may not be ideal, but hopefully you will detect
		// that you are stuck early on in the walkcycle.
		SetState(State_Locomotion);
		return FSM_Continue;
	}

	// Check if either the BLEND_TO_LOCOMOTION tag was hit or the phase crossed the default threshold.
	bool bShouldBlendOut = m_bStartLocomotionBlendOut || m_fAnimPhase >= GetTunables().m_StartLocomotionBlendoutThreshold;

	// Check if the we can interrupt start locomotion due to significant heading (all) or speed changes (player-controlled animals only)	
	bool bShouldInterrupt = false;	
	if (m_bStartLocomotionIsInterruptible)
	{
		bool bDirectionChanged = fabs(SubtractAngleShorter(pPed->GetDesiredHeading(), m_fLocomotionStartInitialDirection)) >= GetTunables().m_StartLocomotionHeadingDeltaBlendoutThreshold;
		bool bSpeedIncreased = pPed->IsAPlayerPed() && GetMotionData().GetIsWalking(GetMotionData().GetCurrentMbrY()) && !GetMotionData().GetIsWalking(GetMotionData().GetGaitReducedDesiredMbrY());
		bShouldInterrupt = bDirectionChanged || bSpeedIncreased;
	}

	if (ShouldStopMoving())
	{
		if (HasQuickStops() && m_fAnimPhase > GetTunables().m_StartToIdleDirectlyPhaseThreshold)
		{
			SetState(State_StopLocomotion);
		}
		else
		{
			SetState(State_Idle);
		}
	}

	// Change to to regular locomotion if we're interrupting (due to significant heading or speed changes) or if the transition anims are done playing.
	if (bShouldBlendOut || bShouldInterrupt)
	{
		if (bShouldInterrupt)
		{
			m_MoveNetworkHelper.SetFloat(ms_StartLocomotionBlendOutDurationId, sm_Tunables.m_StartLocomotionEarlyOutBlendOutDuration);
		}
		else
		{
			m_MoveNetworkHelper.SetFloat(ms_StartLocomotionBlendOutDurationId, sm_Tunables.m_StartLocomotionDefaultBlendOutDuration);
		}
		SetState(State_Locomotion);
	}

	if (IsInWaterWhileLowLod())
	{
		// Block the low physics flag so the qped can drown, etc.
		GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
	}

	return FSM_Continue;
}

void	CTaskQuadLocomotion::StateStartLocomotion_OnExit()
{
	if (m_bUsingRunStart)
	{
		m_fCurrentGait = MOVEBLENDRATIO_SPRINT;
		GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
	}
	else if (m_bUsingWalkStart)
	{
		m_fCurrentGait = MOVEBLENDRATIO_WALK;
		GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
	}

	m_bUsingWalkStart = false;
	m_bUsingRunStart = false;
}

void CTaskQuadLocomotion::StateStartLocomotion_OnProcessMoveSignals()
{
	// Tell the task to call ProcessPhysics so the animated velocity can be manipulated.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
	m_MoveNetworkHelper.GetFloat(ms_IntroPhaseId, m_fAnimPhase);

	// Grab the signal for whether or not the start locomotion animation is interruptible.
	bool bStartLocomotionIsInterruptible = false;
	m_MoveNetworkHelper.GetBoolean(ms_CanEarlyOutForMovement, bStartLocomotionIsInterruptible);
	m_bStartLocomotionIsInterruptible |= bStartLocomotionIsInterruptible;

	// Grab the signal for whether or not the start locomotion can blend into ordinary locomotion.
	bool bStartLocomotionStartBlend = false;
	m_MoveNetworkHelper.GetBoolean(ms_BlendToLocomotion, bStartLocomotionStartBlend);
	m_bStartLocomotionBlendOut |= bStartLocomotionStartBlend;
}

//////////////////////////////////////////////////////////////////////////

void CTaskQuadLocomotion::StateStopLocomotion_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(ms_StopLocomotionId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterStopLocomotionId);

	// Note what the gait was when this qped decided to stop moving.
	m_fGaitWhenDecidedToStop = m_fCurrentGait;

	// Reset cached motion state values.
	m_bInTargetMotionState = false;
	m_fAnimPhase = 0.0f;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskQuadLocomotion::StateStopLocomotion_OnUpdate()
{
	// Calculate the values to send to MoVE.
	ProcessGaits();
	ProcessTurnPhase();

	// Send animation blend information to MoVE.
	SendStopLocomotionMoveSignals();
	SendTurnSignals(m_fTurnPhase);

	// Wait on MoVE before possibly transitioning state.
	if (!m_bInTargetMotionState)
	{	
		return FSM_Continue;
	}

	static const float sf_HackedSlidePhase = 0.5f;
	static const float sf_HackedSlideRate = 3.0f;
	static const float sf_HackedSlideTolerance = 30.0f * DtoR;

	if (m_fAnimPhase >= GetTunables().m_StopPhaseThreshold)
	{
		SetState(State_Idle);
	}
	else if (ShouldBeMoving())
	{
		if (m_fAnimPhase > GetTunables().m_MinStopPhaseToResumeMovement && m_fAnimPhase < GetTunables().m_MaxStopPhaseToResumeMovement)
		{
			Assert(HasWalkStarts() && HasRunStarts());
			SetState(State_StartLocomotion);
		}
	}
	else if (m_fAnimPhase < sf_HackedSlidePhase && fabs(CalcDesiredDirection()) < sf_HackedSlideTolerance)
	{
		// Force some sliding while the stop animation is playing so the ped can be a bit closer to their desired heading when it is done playing.
		// Otherwise you may see a more "robotic" sequence of walk -> stop -> turn in place when we could have just nailed the heading here.
		// Only do this during the first part of the anim to avoid it looking too slidey.
		CPed* pPed = GetPed();
		float fDesiredHeading = pPed->GetDesiredHeading();
		float fCurrentHeading = pPed->GetCurrentHeading();
		float fDelta = InterpValue(fCurrentHeading, fDesiredHeading, sf_HackedSlideRate, true, GetTimeStep());
		GetMotionData().SetExtraHeadingChangeThisFrame(fDelta);
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateStopLocomotion_OnProcessMoveSignals()
{
	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
	m_MoveNetworkHelper.GetFloat(ms_StopPhaseId, m_fAnimPhase);
}

//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::StateLocomotion_OnEnter()
{
	float fDesiredMBR = GetMotionData().GetDesiredMbrY();
	// Send the desired speed to MoVE.
	fDesiredMBR /= MOVEBLENDRATIO_SPRINT;
	m_MoveNetworkHelper.SetFloat(ms_DesiredSpeedId, fDesiredMBR);

	m_fTurnApproachRate = GetMinTurnRate(GetMotionData().GetCurrentMbrY());

	// Transition to the next MoVE state.
	m_MoveNetworkHelper.SendRequest(ms_LocomotionId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterLocomotionId);

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskQuadLocomotion::StateLocomotion_OnUpdate()
{
	// Calculate the values to send to MoVE.
	ProcessGaits();
	ProcessTurnPhase();

	// Send animation blending signals to MoVE.
	SendLocomotionMoveSignals();
	SendTurnSignals(m_fTurnPhase);

	// Wait on MoVE before possibly transitioning to a new state.
	if (!m_bInTargetMotionState)
	{	
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Apply extra heading changes while in low LoD.
	bool bIsLowLodMotion = pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask);
	if (bIsLowLodMotion)
	{
		ApplyExtraHeadingAdjustments();
	}

	// If the quad is now supposed to be stopped, switch to the idle state.
	if (ShouldStopMoving() && (HasQuickStops() || GetMotionData().GetCurrentMbrY() <= GetTunables().m_MinMBRToStop))
	{	
		if (HasQuickStops())
		{
			SetState(State_StopLocomotion);
		}
		else
		{
			SetState(State_Idle);
		}
	}

	// Done with forcing the motion state now that the qped is in locomotion.
	m_bMovingForForcedMotionState = false;

	if (IsInWaterWhileLowLod())
	{
		// Block the low physics flag so the qped can drown, etc.
		GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateLocomotion_OnProcessMoveSignals()
{
	// Tell the task to call ProcessPhysics so the animated velocity can be manipulated.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

float	CTaskQuadLocomotion::GetTimeBetweenGaits(float fCurrentGait, float fNextGait) const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		if (fCurrentGait == MOVEBLENDRATIO_WALK && fNextGait == MOVEBLENDRATIO_RUN)
		{
			return pMotionStruct->m_QuadWalkRunTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_RUN && fNextGait == MOVEBLENDRATIO_WALK)
		{
			return pMotionStruct->m_QuadRunWalkTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_RUN && fNextGait == MOVEBLENDRATIO_SPRINT)
		{
			return pMotionStruct->m_QuadRunSprintTransitionTime;
		}
		else if (fCurrentGait == MOVEBLENDRATIO_SPRINT && fNextGait == MOVEBLENDRATIO_RUN)
		{
			return pMotionStruct->m_QuadSprintRunTransitionTime;
		}
	}
	return 0.25f;
}

bool	CTaskQuadLocomotion::HasWalkStarts() const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		return pMotionStruct->m_HasWalkStarts;
	}
	return false;
}

bool	CTaskQuadLocomotion::HasRunStarts() const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		return pMotionStruct->m_HasRunStarts;
	}
	return false;
}

bool	CTaskQuadLocomotion::HasQuickStops() const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		return pMotionStruct->m_HasQuickStops;
	}
	return false;
}

bool	CTaskQuadLocomotion::CanTrot() const
{
	const CPed* pPed = GetPed();
	const CMotionTaskDataSet* pSet = pPed->GetMotionTaskDataSet();
	if (Verifyf(pSet && pSet->GetOnFootData(), "Invalid motion task data set!"))
	{
		const sMotionTaskData* pMotionStruct = pSet->GetOnFootData();
		return pMotionStruct->m_CanTrot;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::StateTurnInPlace_OnEnter()
{
	// Initially enter this state at 0 MBR and facing forwards.
	m_fCurrentGait = 0.0f;
	m_fNextGait = 0.0f;
	m_fTransitionCounter = 0.0f;
	m_MoveNetworkHelper.SendRequest(ms_StartTurnInPlaceId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterTurnInPlaceId);

	m_fTurnApproachRate = GetMinTurnRate(GetMotionData().GetCurrentMbrY());

	// Store off the quad's initial direction. 
	m_fTurnInPlaceInitialDirection = CalcDesiredDirection();
	m_bTurnInPlaceIsBlendingOut = false;

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskQuadLocomotion::StateTurnInPlace_OnUpdate()
{
	// Calculate the values to send to MoVE.
	ProcessGaits();

	// Send blend information to MoVE.
	SendTurnSignals(m_fTurnInPlaceInitialDirection);

	// Wait on MoVE before possibly transitioning to a new state.
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (ShouldBeMoving())
	{
		// Movement has been requested.
		if (!m_bReverseThisFrame && GetMotionData().GetDesiredMbrY() >= sm_Tunables.m_StartLocomotionWalkRunBoundary && HasRunStarts())
		{
			SetState(State_StartLocomotion);
		}
		else if (!m_bReverseThisFrame && GetMotionData().GetDesiredMbrY() < sm_Tunables.m_StartLocomotionWalkRunBoundary && HasWalkStarts())
		{
			SetState(State_StartLocomotion);
		}
		else
		{
			SetState(State_Locomotion);
		}
	}
	else
	{
		CPed* pPed = GetPed();
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockQuadLocomotionIdleTurns))
		{
			SetState(State_Idle);
			return FSM_Continue;
		}

		if (GetTimeInState() > GetTunables().m_TurnToIdleTransitionDelay)
		{
			// Check to see if while turning the ped has achieved their target or decided to go in a different direction.
			float fHeadingDelta = CalcDesiredDirection();
			float fTolerance = DtoR * GetTunables().m_StopAnimatedTurnsD;
			const float fPitch = GetPed()->GetCurrentPitch();

			// Don't be so precise on steep slopes - qpeds have spring code which can slightly change their current heading when we didn't expect it.
			// The changes are typically small, so simply widening the turn tolerances should be enough to prevent drifting and awkward looking animations.
			float fSteepSlopeTolerance = DtoR * GetTunables().m_SteepSlopeStopAnimatedTurnsD;
			if (fabs(fPitch) >= DtoR * GetTunables().m_SteepSlopeThresholdD)
			{
				fTolerance = fSteepSlopeTolerance;
			}

			if (fabs(fHeadingDelta) < fTolerance)
			{
				// Transition to idle because we have achieved the desired heading.
				m_bTurnInPlaceIsBlendingOut = true;
				SetState(State_Idle);
			} 
			else if (Sign(fHeadingDelta) != Sign(m_fTurnInPlaceInitialDirection))
			{
				// Transition to idle to force turning to start over again in 0.35 seconds or so.
				SetState(State_Idle);
			}
		}
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateTurnInPlace_OnProcessMoveSignals()
{
	// Tell the task to call ProcessPhysics so the animated velocity can be manipulated.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);

	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

void	CTaskQuadLocomotion::StateIdle_OnEnter()
{
	m_fCurrentGait = 0.0f;
	m_fNextGait = 0.0f;
	m_fTransitionCounter = 0.0f;
	m_fIdleDesiredHeading = GetPed()->GetDesiredHeading();

	// Vary the idle rates to prevent synchronization if two animals are standing right next to each other.
	float fRate = fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
	m_MoveNetworkHelper.SetFloat(ms_IdleRateId, fRate);

	m_MoveNetworkHelper.SendRequest(ms_startIdleId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterIdleId);

	// Reset the counter for tracking how long the ped was in the idle state.
	m_fLastIdleStateDuration = 0.0f;

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskQuadLocomotion::StateIdle_OnUpdate()
{
	// Wait on MoVE before possibly transitioning to a new state.
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	s32 iNextState = State_Idle;

	if (ShouldBeMoving())
	{
		// Movement has been requested.
		if (!m_bReverseThisFrame && GetMotionData().GetDesiredMbrY() >= sm_Tunables.m_StartLocomotionWalkRunBoundary && HasRunStarts())
		{
			iNextState = State_StartLocomotion;
		}
		else if (!m_bReverseThisFrame && GetMotionData().GetDesiredMbrY() < sm_Tunables.m_StartLocomotionWalkRunBoundary && HasWalkStarts())
		{
			iNextState = State_StartLocomotion;
		}
		else
		{
			iNextState = State_Locomotion;
		}
	}
	else
	{
		CPed* pPed = GetPed();

		if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockQuadLocomotionIdleTurns))
		{
			float fTurnTolerance = GetTunables().m_StartAnimatedTurnsD * DtoR;
			const float fHeadingDelta = CalcDesiredDirection();
			const float fDesiredHeading = pPed->GetDesiredHeading();
			const float fPitch = pPed->GetCurrentPitch();

			// Don't be so precise on steep slopes - qpeds have spring code which can slightly change their current heading when we didn't expect it.
			// The changes are typically small, so simply widening the turn tolerances should be enough to prevent drifting and awkward looking animations.
			float fSteepSlopeTolerance = DtoR * GetTunables().m_SteepSlopeStartAnimatedTurnsD;
			if (fabs(fPitch) >= DtoR * GetTunables().m_SteepSlopeThresholdD)
			{
				fTurnTolerance = fSteepSlopeTolerance;
			}

			// The desired heading when the state was started does not equal the desired heading now, it is no longer safe to zero out the angular velocity.
			if (fabs(SubtractAngleShorter(fDesiredHeading, m_fIdleDesiredHeading)) > fTurnTolerance)
			{
				m_bTurnInPlaceIsBlendingOut = false;
			}

			if (GetTimeInState() > GetTunables().m_TurnTransitionDelay || GetPreviousState() != State_TurnInPlace)
			{
				// Stop zeroing out the angular velocity on the turns if the ped has been in this state for long enough, or if 
				// the previous state was not State_TurnInPlace.
				m_bTurnInPlaceIsBlendingOut = false;

				// If the ped wants to turn to achieve a new angle, transition to turn in place.
				if (fabs(fHeadingDelta) > fTurnTolerance)
				{
					iNextState = State_TurnInPlace;
				}
			}		
		}
	}

	// Turn phase is forward.
	m_fTurnPhase = 0.0f;
	m_fPreviousTurnPhaseGoal = 0.0f;

	if (iNextState != State_Idle)
	{
		m_fLastIdleStateDuration = GetTimeInState();
		SetState(iNextState);
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateIdle_OnProcessMoveSignals()
{
	if (m_bTurnInPlaceIsBlendingOut)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);
	}

	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskQuadLocomotion::StateDead_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(ms_StartNoClipId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterNoClipId);

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

// Stay dead unless it stops being forced.
CTask::FSM_Return CTaskQuadLocomotion::StateDead_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Dead)
	{
		SetState(State_Initial);
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateDead_OnProcessMoveSignals()
{
	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskQuadLocomotion::StateDoNothing_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(ms_StartNoClipId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterNoClipId);

	// Reset cached motion state values.
	m_bInTargetMotionState = false;

	// Tell MoVE to send signals to this task.
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskQuadLocomotion::StateDoNothing_OnUpdate()
{
	if (!m_bInTargetMotionState)
	{
		return FSM_Continue;
	}

	if (GetMotionData().GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_DoNothing)
	{
		SetState(State_Initial);
	}

	return FSM_Continue;
}

void CTaskQuadLocomotion::StateDoNothing_OnProcessMoveSignals()
{
	// Cache values sent by MoVE.
	m_bInTargetMotionState |= m_MoveNetworkHelper.IsInTargetState();
}

//////////////////////////////////////////////////////////////////////////

float CTaskQuadLocomotion::CalcDesiredDirection() const
{
	const CPed * pPed = GetPed();
	float fUnnormalizedCurrentHeading = pPed->GetCurrentHeading();
	float fUnnormalizedDesiredHeading = pPed->GetDesiredHeading();
	if (Verifyf(fUnnormalizedCurrentHeading == fUnnormalizedCurrentHeading, "NAN current heading."))
	{
		if (Verifyf(fUnnormalizedDesiredHeading == fUnnormalizedDesiredHeading, "NAN desired heading."))
		{
			const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(fUnnormalizedCurrentHeading);
			const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(fUnnormalizedDesiredHeading);

			const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

			return fHeadingDelta;
		}

	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskQuadLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Idle: return "Idle";
	case State_StartLocomotion: return "Start Locomotion";
	case State_StopLocomotion: return "Stop Locomotion";
	case State_Locomotion: return "Locomotion";
	case State_TurnInPlace: return "TurnInPlace";
	case State_Dead: return "Dead";
	case State_DoNothing: return "DoNothing";
	default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskQuadLocomotion::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
