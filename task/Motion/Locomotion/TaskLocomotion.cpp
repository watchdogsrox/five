#include "TaskLocomotion.h"

// Rage headers
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "cranimation/framedof.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentcreature.h"
#include "math/angmath.h"

// Game headers
#include "animation/AnimManager.h"
#include "animation/MovePed.h"
#include "debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

dev_float CTaskMotionBasicLocomotion::ms_fPedMoveAccel				= 2.0f;	//4.0f;
dev_float CTaskMotionBasicLocomotion::ms_fPedMoveDecel				= 8.0f;	// increased to allow peds to stop for way points better
dev_float CTaskMotionBasicLocomotion::ms_fPlayerMoveAccel			= 4.0f;
dev_float CTaskMotionBasicLocomotion::ms_fPlayerMoveDecel			= 2.0f;

const fwMvBooleanId CTaskMotionBasicLocomotion::ms_AefFootHeelLId("AEF_FOOT_HEEL_L",0x1C2B5199);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_AefFootHeelRId("AEF_FOOT_HEEL_R",0xD97C4C50);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_AlternateWalkFinishedId("Alternate_Walk_Finished",0x42FFEE42);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_FinishedIdleTurnId("Finished_Idle_Turn",0x7DCD0C73);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_LoopedId("AlternateWalkLooped",0x13507067);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnEnterMoveTurnId("OnEnterMoveTurn",0x86884717);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnEnterLocomotionId("OnEnterLocomotion",0x96273A49);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnExitLocomotionId("OnExitLocomotion",0x866C007C);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnExitMoveStartLId("OnExitMoveStartL",0xB5C1D9C2);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnExitMoveStartRId("OnExitMoveStartR",0x316E5119);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnExitMoveStopId("OnExitMoveStop",0x4A208FF8);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_OnExitMoveTurnId("OnExitMoveTurn",0x9D82AB12);
const fwMvBooleanId CTaskMotionBasicLocomotion::ms_ResetTransitionTimerId("ResetTransitionTimer",0xEAAA5021);
const fwMvClipId CTaskMotionBasicLocomotion::ms_ClipId("AlternateWalkClip",0x4A56CAFD);
const fwMvClipId CTaskMotionBasicLocomotion::ms_IdleId("WeaponIdleClip",0x7560D6C2);
const fwMvClipId CTaskMotionBasicLocomotion::ms_IdleTurnClip0Id("IdleTurnClip0",0xC7E79158);
const fwMvClipId CTaskMotionBasicLocomotion::ms_IdleTurnClip0LId("IdleTurnClip0L",0x8B7F8443);
const fwMvClipId CTaskMotionBasicLocomotion::ms_RunId("WeaponRunClip",0xB8A9F0BB);
const fwMvClipId CTaskMotionBasicLocomotion::ms_WalkId("WeaponWalkClip",0x211B8B7);
const fwMvFilterId CTaskMotionBasicLocomotion::ms_WeaponFilterId("WeaponAnimFilter",0x303714BA);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_AlternateWalkFlagId("UseAlternateWalk",0xDE7A9B0D);
const fwMvRequestId CTaskMotionBasicLocomotion::ms_AlternateWalkChangedId("AlternateWalkChanged",0x63AC9632);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_BlockMoveStopId("BlockMoveStop",0xD5F449C0);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_HasDesiredVelocityId("HasDesiredVelocity",0x75190293);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_LastFootRightId("LastFootRight",0x27BB5957);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_LocomotionVisibleId("LocomotionVisible",0x48BB595);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_MoveStartLeftVisibleId("MoveStartLeftVisible",0xDA991C83);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_MoveStartRightVisibleId("MoveStartRightVisible",0xC52EC626);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_MoveStopVisibleId("MoveStopVisible",0xB32FEC39);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_MoveTurnVisibleId("MoveTurnVisible",0x151D3C60);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_IsPlayerControlledId("IsPlayer",0x691C1A48);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_DesiredDirectionId("DesiredDirection",0xAC040D40);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_DesiredSpeedId("DesiredSpeed",0xCF7BA842);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_DirectionId("Direction",0x34DF9828);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_DurationId("AlternateWalkCrossBlend",0x817779C7);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_EndDirectionId("EndDirection",0xE6CFD19B);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_WalkStopRateId("WalkStopRate",0xFF5F20A9);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_StartDirectionId("StartDirection",0xE4825C7C);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_StartDirectionLeftId("StartDirectionLeft",0x9288BB4E);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_StartDirectionToDesiredDirectionId("StartDirectionToDesiredDirection",0x1A0B0FF4);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_StartDirectionToDesiredDirectionLeftId("StartDirectionToDesiredDirectionLeft",0x8BC371AD);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_TransitionTimerId("TransitionTimer",0x56959825);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_WeaponWeightId("WeaponAnimWeight",0x277F099E);
const fwMvClipId  CTaskMotionBasicLocomotion::ms_MoveStopClip("MoveStopClip",0xDA0897FD);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_MoveStopPhase("MoveStopPhase",0x3C16BB51);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_MoveRateOverrideId("MoveRateOverride",0x9968532B);
const fwMvFloatId CTaskMotionBasicLocomotion::ms_SlopeDirectionId("SlopeDirection",0xFA0F8DA2);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_StairWalkFlagId("UseStairWalk",0x3BBD3CD0);
const fwMvFlagId CTaskMotionBasicLocomotion::ms_SlopeWalkFlagId("UseSlopeWalk",0x847D94E4);

CTaskMotionBasicLocomotion::CTaskMotionBasicLocomotion(u16 initialState, const fwMvClipSetId &clipSetId, bool isCrouching) 
: m_clipSetId(clipSetId)
, m_moveDirection(0.0f)
, m_referenceDirection(0.0f)
, m_referenceDesiredDirection(0.0f)
, m_smoothedDirection(0.0f)
, m_smoothedSlopeDirection(0.0f)
, m_transitionTimer(0.0f)
, m_initialState(initialState)
, m_bShouldUpdateMoveTurn(true)
, m_previousDesiredSpeed(0.0f)
, m_weaponClipSet(u32(0))
, m_pWeaponFilter(NULL)
{
	if (isCrouching)
	{
		m_flags.SetFlag(Flag_Crouching);
	}
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);
	SetInternalTaskType(CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION);
}

CTaskMotionBasicLocomotion::~CTaskMotionBasicLocomotion()
{
	SetWeaponClipSet(CLIP_SET_ID_INVALID, u32(0), GetPed());
}

bool CTaskMotionBasicLocomotion::SupportsMotionState(CPedMotionStates::eMotionState state)
{	
	if (m_flags.IsFlagSet(Flag_Crouching))
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Crouch_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Crouch_Walk:
		case CPedMotionStates::MotionState_Crouch_Run:
			return true;
		default:
			return false;
		}
	}
	else
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
			return true;
		default:
			return false;
		}
	}
}

bool CTaskMotionBasicLocomotion::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	if (m_flags.IsFlagSet(Flag_Crouching))
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Crouch_Idle:
			return GetState()==State_Idle;
		case CPedMotionStates::MotionState_Crouch_Walk:
			return (GetState()==State_Locomotion && GetMotionData().GetCurrentMbrY()<=1.0f);
		case CPedMotionStates::MotionState_Crouch_Run:
			return (GetState()==State_Locomotion && GetMotionData().GetCurrentMbrY()>1.0f);
		default:
			return false;
		}
	}
	else
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
			return GetState()==State_Idle;
		case CPedMotionStates::MotionState_Walk:
			return (GetState()==State_Locomotion && GetMotionData().GetCurrentMbrY()<=1.0f);
		case CPedMotionStates::MotionState_Run:
			return (GetState()==State_Locomotion && GetMotionData().GetCurrentMbrY()>1.0f && GetMotionData().GetCurrentMbrY()<=2.0f);
		case CPedMotionStates::MotionState_Sprint:
			return (GetState()==State_Locomotion && GetMotionData().GetCurrentMbrY()>2.0f);
		default:
			return false;
		}
	}
}

CTask*  CTaskMotionBasicLocomotion::CreatePlayerControlTask() 
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}


bool CTaskMotionBasicLocomotion::IsInMotion(const CPed * UNUSED_PARAM(pPed)) const
{
	const bool bMoving = false;
	return bMoving;
}

void CTaskMotionBasicLocomotion::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(pSet);

	static const fwMvClipId s_walkClip("walk",0x83504C9C);
	static const fwMvClipId s_runClip("run",0x1109B569);
	static const fwMvClipId s_sprintClip("sprint",0xBC29E48);

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*pSet, speeds, clipNames);

	return;
}

float CTaskMotionBasicLocomotion::GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* UNUSED_PARAM(bOutIsClipActive))
{
	float stoppingDistance = 0.0f;

	//early out if we're in the idle state
	if (GetState()==State_Idle || GetState()==State_Initial)
	{
		return 0.0f;
	}

	// if we're moving, add the speed we'll (probably) move in the frame it takes to send the stop
	// signal to the MoVE network (Not sure how well this will work during widely varying frame rate...)
	if (GetState()==State_Locomotion)
	{
		CMoveBlendRatioSpeeds speeds;
		GetMoveSpeeds(speeds);
		stoppingDistance = speeds.GetMoveSpeed(GetMotionData().GetCurrentMbrY()) * fwTimer::GetTimeStep();
	}

	// First check to see if we're passing the move stop clip and phase out of the network
	// we can use these to calculate the actual distance left in the move stop clip

	const crClip* pMoveStopClip = m_moveNetworkHelper.GetClip(ms_MoveStopClip);

	if (pMoveStopClip)
	{
		// start by extracting the mover track from the clip
		// TODO - we should do this with metadata long term for performance reasons

		float moveStopPhase = m_moveNetworkHelper.GetFloat(ms_MoveStopPhase);

		Vector3 remainingOffset = fwAnimHelpers::GetMoverTrackTranslationDiffRotated(*pMoveStopClip, moveStopPhase, 1.0f);

		stoppingDistance += remainingOffset.XYMag();
		return stoppingDistance;
	}


	// If not, get the distance values from the complete clips

	enum stoppingClips
	{
		L_FOOT_WALK,
		L_FOOT_RUN,
		R_FOOT_WALK,
		R_FOOT_RUN,
		STOPPING_CLIPS_MAX
	};	

	static const u32 clipHashes[STOPPING_CLIPS_MAX] =
	{
		ATSTRINGHASH("wstop_l_0", 0x0ed71b76c),
		ATSTRINGHASH("rstop_l", 0x0e53db8e9),
		ATSTRINGHASH("wstop_r_0", 0x0757b7872),
		ATSTRINGHASH("rstop_r", 0x03fc66df9),
	};

	float stoppingDists[STOPPING_CLIPS_MAX];

	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);

	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(pSet);

	//load or init the stopping distances in the clip clipSetId metadata
	for (int i=0; i<STOPPING_CLIPS_MAX ;i++)
	{
		float* distance = pSet->GetProperties().Access(clipHashes[i]);

		if (distance)
		{
			stoppingDists[i] = *distance;
		}
		else
		{
			const crClip* pClip = pSet->GetClip(fwMvClipId(clipHashes[i]));
			taskAssert(pClip);
			if(pClip)
			{
				stoppingDists[i] = fwAnimHelpers::GetMoverTrackTranslationDiffRotated(*pClip, 0.0f, 1.0f).y;
			}
			else
			{
				stoppingDists[i] = 0.f;
			}
			pSet->GetProperties().Insert(clipHashes[i], stoppingDists[i]);
		}
	}

	// now calculate the remaining stopping distance based on the current speed
	// GSALES - 19/07/2011
	// Due to some changes in the motion system, we no longer need to perform this step. We now transition
	// immediately into the stopping states when the desired speed drops to zero, using tag synchronization
	// to make the feet match. This makes the length of the stopping clips a reasonable approximation of
	// the overall stopping distance. That being said it may tend to overestimate slightly, since the 
	// synchronizer can bring those stop clips in at a later phase.

	// find the current clip and distance remaining from the locomotion state
	//float remainingMoveDistance = CalculateRemainingMovementDistance();

	//static float s_extraStoppingDistanceMult = 1.0f;
	static fwMvBooleanId ms_useLeftFoot("USE_LEFT_FOOT_TRANSITION",0x8F7E474);

	bool bUseLeftFoot = m_moveNetworkHelper.GetBoolean(ms_useLeftFoot);

	u16 walkStopClip = (u16)(bUseLeftFoot ? L_FOOT_WALK : R_FOOT_WALK);
	u16 runStopClip = (u16)(bUseLeftFoot ? L_FOOT_RUN : R_FOOT_RUN);

	if (GetMotionData().GetCurrentMbrY()>MOVEBLENDRATIO_WALK)
	{
		return stoppingDistance + stoppingDists[runStopClip];
	}
	else
	{
		return stoppingDistance + stoppingDists[walkStopClip];
	}
}

void CTaskMotionBasicLocomotion::CalcRatios(const float fMBR, float & fWalkRatio, float & fRunRatio, float & fSprintRatio)
{
	if(fMBR < MOVEBLENDRATIO_WALK)
	{
		fWalkRatio = 1.0f;
		fRunRatio = fSprintRatio = 0.0f;
	}
	else if(fMBR >= MOVEBLENDRATIO_WALK && fMBR <= MOVEBLENDRATIO_RUN)
	{
		fRunRatio = fMBR - MOVEBLENDRATIO_WALK;
		fWalkRatio = 1.0f - fRunRatio;
		fSprintRatio = 0.0f;
	}
	else if(fMBR >= MOVEBLENDRATIO_RUN && fMBR <= MOVEBLENDRATIO_SPRINT)
	{
		fWalkRatio = 0.0f;
		fSprintRatio = fMBR - MOVEBLENDRATIO_RUN;
		fRunRatio = 1.0f - fSprintRatio;

	}
	else
	{
		fWalkRatio = fRunRatio = fSprintRatio = 0.0f;
	}
}

float CTaskMotionBasicLocomotion::InterpValue(float& current, float target, float interpRate, bool angular /*= false*/)
{

#if USE_LINEAR_INTERPOLATION
	float initialCurrent = current;

	if (angular)
	{
		float amount = SubtractAngleShorter(target, current);
		target = current+amount;
	}

	if (target<current)
	{
		current-=(interpRate*fwTimer::GetTimeStep());
		current = Max(current, target);
	}
	else
	{
		current+=(interpRate*fwTimer::GetTimeStep());
		current = Min(current, target);
	}

	return (current-initialCurrent);
#else //USE_LINEAR_INTERPOLATION
	float amount;
	if (angular)
	{
		amount = SubtractAngleShorter(target, current);
	}
	else
	{
		amount = target-current;
	}
	amount = (amount * fwTimer::GetTimeStep() * interpRate);
	current+=amount;
	return amount;
#endif //USE_LINEAR_INTERPOLATION
}

void CTaskMotionBasicLocomotion::SendParams()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		CPed * pPed = GetPed();
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();

		float fDirectionVar;
		bool bIsPlayer = GetMotionData().GetPlayerHasControlOfPedThisFrame();

		TUNE_GROUP_BOOL(PED_MOVEMENT, bForcePlayerSpecificMovementOnAllPeds, false);

		if (bForcePlayerSpecificMovementOnAllPeds)
		{
			m_moveNetworkHelper.SetFlag(true, ms_IsPlayerControlledId);
		}
		else
		{
			m_moveNetworkHelper.SetFlag(bIsPlayer, ms_IsPlayerControlledId);
		}

		TUNE_GROUP_BOOL(PED_MOVEMENT, bFixHeadingDuringMoveTurns, false);
		
		if (GetState()==State_MoveTurn && bFixHeadingDuringMoveTurns)
		{
			// when executing the 180 move turn, we don't want the ped to turn whilst blending out into the 180 turn
			// (causes a pop). This hack could be removed if we had the option to deactivate a control parameter when a transition
			// occurs.
			fDirectionVar = 0.0f;
		}
		else
		{
			fDirectionVar = CalcDesiredDirection();
		}

		// Use the desired direction in the MoVE network to trigger transitions, etc (more responsive than using the smoothed one) 
		m_moveNetworkHelper.SetFloat(ms_DesiredDirectionId, ConvertRadianToSignal(fDirectionVar));

		if (GetMotionData().GetPlayerHasControlOfPedThisFrame())
		{
			// Smooth the direction parameter we'll be using to clipSetId blend weights in the network
			TUNE_GROUP_FLOAT ( PED_MOVEMENT, fLocomotionDirectionSmoothing, 6.0f, 0.0f, 20.0f, 0.001f );

			InterpValue(m_smoothedDirection, fDirectionVar, fLocomotionDirectionSmoothing);

			m_smoothedDirection = fwAngle::LimitRadianAngleSafe( m_smoothedDirection );
		}
		else
		{
			// Smooth the direction parameter we'll be using to clipSetId blend weights in the network
			TUNE_GROUP_FLOAT ( PED_MOVEMENT, fLocomotionDirectionSmoothingAI, 6.0f, 0.0f, 20.0f, 0.001f );

			InterpValue(m_smoothedDirection, fDirectionVar, fLocomotionDirectionSmoothingAI);

			m_smoothedDirection = fwAngle::LimitRadianAngleSafe( m_smoothedDirection );
		}

		m_moveNetworkHelper.SetFloat(ms_DirectionId, ConvertRadianToSignal(m_smoothedDirection));

		// Set the overridden rate param for run/sprint
		m_moveNetworkHelper.SetFloat( ms_MoveRateOverrideId, pPed->GetMotionData()->GetCurrentMoveRateOverride() );

		// we send a special flag to the network to tell it whether the ped wants to move
		// at this stage. This means we can early out of states based on the ped's desired behaviour,
		// whilst still smoothing the speed parameter.
		// For player controlled characters we don't want to send this signal on the first frame that 
		// the speed zeroes out, as it causes unwanted transitions when moving the stick across its dead zone
		bool bHasDesiredVelocity = false;
		if (bIsPlayer)
		{
			if (vDesiredMBR.y >0.0f || m_previousDesiredSpeed>0.0f)
				bHasDesiredVelocity = true;
		}
		else if (vDesiredMBR.y >0.0f)
		{
			bHasDesiredVelocity = true;
		}
		
		m_moveNetworkHelper.SetFlag(bHasDesiredVelocity, ms_HasDesiredVelocityId);
		

		float fSpeed = vCurrentMBR.y / MOVEBLENDRATIO_SPRINT; // normalize the speed values between 0.0f and 1.0f

		m_moveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);
		m_moveNetworkHelper.SetFloat(ms_DesiredSpeedId, vDesiredMBR.y / MOVEBLENDRATIO_SPRINT);

		m_previousDesiredSpeed = vDesiredMBR.y;

		// TEMPORARY:	We currently don't support multiple clip sets on a single network.
		//				Until we do, we'll have to pass our weapon clip clipSetId and alternate walk clips here.
		SendWeaponSignals();

		SendAlternateWalkSignals();

#if __DEV
		TUNE_GROUP_BOOL (PED_MOVEMENT, bRenderMoveSignals, false);

		if (bRenderMoveSignals)
		{
			char debugText[128];
			sprintf(debugText, "Direction:%.6f, DesiredDirection:%.6f, TransitionTimer: %.6f ", ConvertRadianToSignal(m_smoothedDirection), ConvertRadianToSignal(fDirectionVar), m_transitionTimer);
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			grcDebugDraw::Text(vPedPosition, Color_yellow, debugText);
			sprintf(debugText, "Speed:%.6f, DesiredSpeed: %.6f, HasDesiredVel:%d", fSpeed, vDesiredMBR.y / MOVEBLENDRATIO_SPRINT, (int)vDesiredMBR.y>0.0f);
			grcDebugDraw::Text(vPedPosition, Color_yellow, 0, 12, debugText);
		}
#endif //__DEV
	}
}

void CTaskMotionBasicLocomotion::SetWeaponClipSet(const fwMvClipSetId &clipSetId, atHashWithStringNotFinal filter, const CPed* pPed)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if (clipSetId.GetHash()!=0)
	{
		if (m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}

		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(filter.GetHash(), pPed);
		
		if(m_pWeaponFilter)	
		{
			m_pWeaponFilter->AddRef();
		}
	}
	else
	{
		if (m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}
	}

	m_weaponClipSet = clipSetId;
}

bool CTaskMotionBasicLocomotion::ProcessPostMovement()
{
	// update the time-in-state timer here.	
	// we do this here because we can be sure that the motion tree update is complete, and that 
	// any signals we send will be available on the next motion tree update.

	const CPed* pPed = GetPed();

	//we don't want to push in any signals if the network is inactive or the entity is not clipating
	if (m_moveNetworkHelper.IsNetworkActive() && pPed && !pPed->m_nDEflags.bFrozen)
	{
		m_transitionTimer += fwTimer::GetTimeStep();

		if(m_moveNetworkHelper.GetBoolean(ms_ResetTransitionTimerId))
		{
			m_transitionTimer = 0.0f;
		}

		m_moveNetworkHelper.SetFloat(ms_TransitionTimerId, m_transitionTimer);

		if ( m_moveNetworkHelper.GetFootstep(ms_AefFootHeelLId) )
		{
			m_lastFootRight = false;
			m_moveNetworkHelper.SetFlag(false, ms_LastFootRightId);
		}
		else if ( m_moveNetworkHelper.GetFootstep(ms_AefFootHeelRId) )
		{
			m_lastFootRight = true;
			m_moveNetworkHelper.SetFlag(true, ms_LastFootRightId);
		}

		// TEMPORARY -Update some flag signals that tell the network information about its visible state
		// This whole section should be removed when we have support for checking the current
		// visibility of a target state in the move runtime. The timer check is there as a backup, since we've had some issues
		// with the move start flags being left on when we're in the idle, resulting in the ped getting stuck in his idle pose.

		if (m_moveNetworkHelper.GetBoolean(ms_OnExitMoveStopId) || (GetState()!=State_MoveStop && GetTimeInState()>1.5f))
		{
			m_moveNetworkHelper.SetFlag(false, ms_MoveStopVisibleId);
		}
		if (m_moveNetworkHelper.GetBoolean(ms_OnExitLocomotionId) || (GetState()!=State_Locomotion && GetTimeInState()>1.5f))
		{
			m_moveNetworkHelper.SetFlag(false, ms_LocomotionVisibleId);
		}
		if (m_moveNetworkHelper.GetBoolean(ms_OnExitMoveStartRId) || (GetState()!=State_MoveStartRight && GetTimeInState()>1.5f))
		{
			m_moveNetworkHelper.SetFlag(false, ms_MoveStartRightVisibleId);
		}
		if (m_moveNetworkHelper.GetBoolean(ms_OnExitMoveStartLId) || (GetState()!=State_MoveStartLeft && GetTimeInState()>1.5f))
		{
			m_moveNetworkHelper.SetFlag(false, ms_MoveStartLeftVisibleId);
		}
		if (m_moveNetworkHelper.GetBoolean(ms_OnEnterMoveTurnId))
		{
			m_moveNetworkHelper.SetFlag(true, ms_MoveTurnVisibleId);
		}
		if (m_moveNetworkHelper.GetBoolean(ms_OnExitMoveTurnId))
		{
			m_moveNetworkHelper.SetFlag(false, ms_MoveTurnVisibleId);
		}

		if (m_moveNetworkHelper.GetBoolean(ms_AlternateWalkFinishedId ))
		{
			if (GetParent()&& GetParent()->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
			{
				CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(GetParent());
				pTask->StopAlternateClip(ACT_Walk, m_alternateClips[ACT_Walk].fBlendDuration);
			}
			else
			{
				EndAlternateClip(ACT_Walk, m_alternateClips[ACT_Walk].fBlendDuration);
			}
		}
	}

	return false;
}

bool CTaskMotionBasicLocomotion::SyncToMoveState()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		enum {
			onEnterIdle,
			onEnterLocomotion,
			onEnterMoveStartL,
			onEnterMoveStartR,
			onEnterMoveStopL,
			onEnterMoveStopR,
			onEnterChangeDirection,
			eventCount
		};

		static const fwMvId s_eventIds[eventCount] = {
				ms_OnEnterIdleId,
				ms_OnEnterLocomotionId,
				fwMvId("OnEnterMoveStartL",0x587009E9),
				fwMvId("OnEnterMoveStartR",0xEA362D7B),
				fwMvId("OnEnterMoveStopL",0x5031106B),
				fwMvId("OnEnterMoveStopR",0x2AB8C57B),
				fwMvId("OnEnterChangeDirection",0xBE689531)
		};

		CompileTimeAssert(NELEM(s_eventIds) == eventCount);

		s32 iState = -1;

		fwMvId lastStateEventId = m_moveNetworkHelper.GetNetworkPlayer()->GetLastEventThisFrame(s_eventIds, eventCount);

		if( lastStateEventId == s_eventIds[onEnterIdle] )
			iState = State_Idle; 
		else if( lastStateEventId == s_eventIds[onEnterLocomotion] )
			iState = State_Locomotion;
		else if( lastStateEventId == s_eventIds[onEnterMoveStartL] )
			iState = State_MoveStartLeft;
		else if( lastStateEventId == s_eventIds[onEnterMoveStartR] )
			iState = State_MoveStartRight;
		else if( lastStateEventId == s_eventIds[onEnterMoveStopL] )
			iState = State_MoveStop;
		else if( lastStateEventId ==s_eventIds[onEnterMoveStopR] )
			iState = State_MoveStop;
		else if( lastStateEventId == s_eventIds[onEnterChangeDirection] )
			iState = State_ChangeDirection;
		else if( m_moveNetworkHelper.GetBoolean(ms_ResetTransitionTimerId) && GetState()==State_MoveTurn )
			iState = State_Locomotion;


		SendParams();

		if(iState != -1 && iState != GetState())
		{
			SetState(iState);
			return true;
		}
	}
	return false;
}

CTask::FSM_Return CTaskMotionBasicLocomotion::ProcessPreFSM()
{
	ProcessMovement(GetPed(), GetTimeStep());

 	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);
// 
// 	static dev_u32 TURN_OFF_IK_TIME = 1000;
// 	if(pPed->GetLastSlopeDetectedTime() + TURN_OFF_IK_TIME > fwTimer::GetTimeInMilliseconds() && !pPed->GetStairsDetected())
// 	{
// 		pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
// 	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionBasicLocomotion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnUpdate)
	{
		UpdateGaitReduction();
		if(SyncToMoveState())
			return FSM_Continue;
	}

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				return StateIdle_OnEnter();
			FSM_OnUpdate
				return StateIdle_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateIdle_OnExit();
				return iRet;
			}

		FSM_State(State_MoveStartRight)
				FSM_OnEnter
				return StateMoveStartRight_OnEnter();
			FSM_OnUpdate
				return StateMoveStartRight_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateMoveStartRight_OnExit();
				return iRet;
			}

		FSM_State(State_MoveStartLeft)
				FSM_OnEnter
				return StateMoveStartLeft_OnEnter();
			FSM_OnUpdate
				return StateMoveStartLeft_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateMoveStartLeft_OnExit();
				return iRet;
			}

		FSM_State(State_ChangeDirection)
				FSM_OnEnter
				return FSM_Continue;
			FSM_OnUpdate
				return StateChangeDirection_OnUpdate();
			FSM_OnExit
			{
				return FSM_Continue;
			}

		FSM_State(State_Locomotion)
			FSM_OnEnter
				return StateLocomotion_OnEnter();
			FSM_OnUpdate
				return StateLocomotion_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateLocomotion_OnExit();
				return iRet;
			}

		FSM_State(State_MoveTurn)
				FSM_OnEnter
				return StateMoveTurn_OnEnter();
			FSM_OnUpdate
				return StateMoveTurn_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateMoveTurn_OnExit();
				return iRet;
			}

		FSM_State(State_MoveStop)
				FSM_OnEnter
				return StateMoveStop_OnEnter();
			FSM_OnUpdate
				return StateMoveStop_OnUpdate();
			FSM_OnExit
			{
				FSM_Return iRet = StateMoveStop_OnExit();
				return iRet;
			}

		FSM_State(State_Finish)
			FSM_OnEnter
				return StateFinish_OnEnter();
			FSM_OnUpdate
				return StateFinish_OnUpdate();

	FSM_End
}

void CTaskMotionBasicLocomotion::CleanUp()
{
	SetWeaponClipSet(CLIP_SET_ID_INVALID, u32(0), GetPed());

	ResetGaitReduction();
}

#if !__FINAL
const char * CTaskMotionBasicLocomotion::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Initial: return "Initial";
		case State_Idle: return "Idle";
		case State_IdleTurn: return "IdleTurn";
		case State_MoveStartRight: return "MoveStartRight";
		case State_MoveStartLeft: return "MoveStartLeft";
		case State_ChangeDirection: return "ChangeDirection";
		case State_Locomotion: return "Locomotion";
		case State_MoveTurn: return "MoveTurn";
		case State_MoveStop: return "MoveStop";
		case State_Finish: return "Finish";
		default: { Assert(false); return "Unknown"; }
	}
}

#endif

#if __BANK

void CTaskMotionBasicLocomotion::DebugRenderHeading(float angle, Color32 color , bool pedRelative) const
{
	const CPed* pPed = GetPed();
	Vector3 temp(0.0f, 1.0f, 0.0f);
	temp.RotateZ(angle);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if (pedRelative)
	{
		temp = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform(VECTOR3_TO_VEC3V(temp)));
	}
	else
	{
		temp+= vPedPosition;
	}

	grcDebugDraw::Line( vPedPosition, temp, color );
}

#endif //__BANK

#if !__FINAL

void CTaskMotionBasicLocomotion::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();

#if __BANK
	//draw the smoothed direction...
	DebugRenderHeading(m_smoothedDirection, Color_green, true);
	
	switch(GetState())
	{
	case State_MoveStartLeft:
	case State_MoveStartRight:
	case State_MoveStop:
	case State_MoveTurn:
		{
			DebugRenderHeading(m_referenceDirection, Color_purple, false);
			DebugRenderHeading(m_moveDirection+m_referenceDirection, Color_orange, false);
		}
		break;
	default:

		break;
	}
#endif //__BANK

}

#endif //!__FINAL



//////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotion::StartMoving()
{

	//send the start locomotion signals here
	CPed* pPed = GetPed();

	// clipSetId a fixed direction that we're going to hold throughout the turn
	m_moveDirection = CalcDesiredDirection();
	m_referenceDirection = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
	m_referenceDesiredDirection = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());

	//	Since these are authored walk start clips, there's no need to lerp the speed signal.
	//	Jump the speed up to the desired immediately so that the tag
	//	synchronization time warps the clips based on the new weights.

	//Vector2 vCurrentMBR = pPed->GetMoveBlender()->GetDesiredMoveBlendRatio();

	//float fSpeed = vCurrentMBR.y / MOVEBLENDRATIO_SPRINT; // normalize the speed value between 0.0f and 1.0f
	//m_moveNetworkHelper.SetBoolean(pPed, "Speed", fSpeed);
	//pPed->GetMoveBlender()->m_vCurrentMoveBlendRatio = vCurrentMBR;

	// Decide which foot to start on - if we're transitioning from a move start state state, use the other one,
	// otherwise, decide based on the direction of movement.

	bool bRightFoot = false;

	static const fwMvRequestId swapToRightFootId("SwapToRightFoot",0x78789A92);
	static const fwMvRequestId swapToLeftFootId("SwapToLeftFoot",0x2B451915);
	static const fwMvRequestId startLocomotionId("StartLocomotion",0x15F666A1);

	switch(GetState())
	{
	case State_MoveStartLeft:
		{
			m_moveNetworkHelper.SendRequest(swapToRightFootId);
			bRightFoot = true;
		}
		break;
	case State_MoveStartRight:
		{
			m_moveNetworkHelper.SendRequest(swapToLeftFootId);
			bRightFoot = false;
		}
		break;
	default:
		{
			m_moveNetworkHelper.SendRequest(startLocomotionId);
			bRightFoot = m_moveDirection<0.0f ? true: false;
		}
		break;
	}

	float fMoveDirectionSignal = ConvertRadianToSignal(m_moveDirection);
	if (bRightFoot)
	{
		m_moveNetworkHelper.SetFloat(ms_StartDirectionId, fMoveDirectionSignal);
		m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionId, 0.0f);
	}
	else
	{
		m_moveNetworkHelper.SetFloat(ms_StartDirectionLeftId, fMoveDirectionSignal);
		m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionLeftId, 0.0f);
	}
}

//////////////////////////////////////////////////////////////////////////

#define NUM_TURN_CLIPS 5

bool CTaskMotionBasicLocomotion::AdjustTurnForDesiredDirection( float& turnAmount )
{

	CPed* pPed = GetPed();

	// only do this for players, or if a task really needs it. It's expensive
	if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceImprovedIdleTurns))
	{		
		if (!pPed->IsAPlayerPed() || pPed->GetMovePed().GetTaskNetwork()!=NULL)
			return true;
	}

	const crClip* turnClips[NUM_TURN_CLIPS];
	float clipWeights[NUM_TURN_CLIPS];
	float clipPhases[NUM_TURN_CLIPS];
	float clipTurnLeft[NUM_TURN_CLIPS];
	char paramId[24];

	bool bCanMakeTurn = true;
	
	// load the necessary params from the MoVE network
	for (u16 i=0; i< NUM_TURN_CLIPS; i++)
	{
		if (GetState()==State_MoveStartLeft)
		{
			sprintf(paramId, "IdleTurnClip%dL", i);
			turnClips[i] = m_moveNetworkHelper.GetClip(fwMvClipId(paramId));
			sprintf(paramId, "IdleTurnPhase%dL", i);
			clipPhases[i] = m_moveNetworkHelper.GetFloat(fwMvFloatId(paramId));
			sprintf(paramId, "IdleTurnWeight%dL", i);
			clipWeights[i] = m_moveNetworkHelper.GetFloat(fwMvFloatId(paramId));
		}
		else
		{
			sprintf(paramId, "IdleTurnClip%d", i);
			turnClips[i] = m_moveNetworkHelper.GetClip(fwMvClipId(paramId));
			sprintf(paramId, "IdleTurnPhase%d", i);
			clipPhases[i] = m_moveNetworkHelper.GetFloat(fwMvFloatId(paramId));
			sprintf(paramId, "IdleTurnWeight%d", i);
			clipWeights[i] = m_moveNetworkHelper.GetFloat(fwMvFloatId(paramId));
		}

		// work out the remaining turn in this clip
		if (turnClips[i])
		{
			Quaternion r(Quaternion::sm_I);

			crFrameDataSingleDof frameData(kTrackMoverRotation, 0, kFormatTypeQuaternion);
			crFrameSingleDof frame(frameData);
			frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());
			turnClips[i]->Composite(frame, turnClips[i]->ConvertPhaseToTime(clipPhases[i]));
			if(frame.Valid())
			{
				Vector3 eulerRotation;

				frame.GetValue<QuatV>(RC_QUATV(r));
				r.ToEulers(eulerRotation);
				clipTurnLeft[i] = eulerRotation.z;

				turnClips[i]->Composite(frame, turnClips[i]->ConvertPhaseToTime(1.0f));

				frame.GetValue<QuatV>(RC_QUATV(r));
				r.ToEulers(eulerRotation);
				clipTurnLeft[i] = SubtractAngleShorter(eulerRotation.z, clipTurnLeft[i]);
			}
		}
		else
		{
			return true;
		}
	}

	//180 degree rotations in clips can sometimes come out with the wrong sign...
	if (clipTurnLeft[0]<0.0f)
		clipTurnLeft[0]*=-1.0f;

	if (clipTurnLeft[NUM_TURN_CLIPS-1]>0.0f)
		clipTurnLeft[NUM_TURN_CLIPS-1]*=-1.0f;

	// calculate the remaining turn in the current blend

	float remainingTurn = 0.0f;
	float totalTurn = 0.0f;
	float desiredTurn = CalcDesiredDirection();

	// What angle do we need to pass in to make the turn?
	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMinTurnAngleForHeadingAdjust, 0.085f, 0.0f, PI, 0.0001f);

	for (u16 i=0; i<NUM_TURN_CLIPS; i++)
	{
		remainingTurn+=(clipTurnLeft[i]*clipWeights[i]);
	}

	//this would need to be adjusted for different turning clip arrangements
	totalTurn = 
		(clipWeights[0]*PI)
		+ (clipWeights[1]*HALF_PI)
		+ (clipWeights[3]*-HALF_PI)
		+ (clipWeights[4]*-PI);

	// The turn in the blend is very low. This may be because the turns are canceling each other out
	// Try calculating using the absolute value of what's remaining to get us moving again
	if (abs(remainingTurn)<fMinTurnAngleForHeadingAdjust || abs(totalTurn)<fMinTurnAngleForHeadingAdjust)
	{
		for (u16 i=0; i<NUM_TURN_CLIPS; i++)
		{
			remainingTurn+=abs(clipTurnLeft[i]);
		}

		//this should be the sum of the absolute values of the turn in the full clipSetId of turning clips
		totalTurn = PI*3.0f;
	}
	
	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMaxDesiredTurnForHeadingLerp, 0.1f, 0.0f, PI, 0.0001f);
	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMoveTurnDiscrepancyTurnRate, 7.5f, 0.0f, 10.0f, 0.0001f);
	if (abs(desiredTurn)<fMaxDesiredTurnForHeadingLerp)
	{
		// if the discrepancy is small, add a little extra turn
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(desiredTurn*fwTimer::GetTimeStep()*fMoveTurnDiscrepancyTurnRate);
	}
	else if (abs(totalTurn)>fMinTurnAngleForHeadingAdjust && abs(remainingTurn)>fMinTurnAngleForHeadingAdjust)
	{
		float remainingMovementRatio = remainingTurn / totalTurn;
		float headingTarget = desiredTurn/remainingMovementRatio;

		if (desiredTurn>clipTurnLeft[0] || desiredTurn<clipTurnLeft[NUM_TURN_CLIPS-1])
		{
			// There's not enough turn left in the clip to make it
			float turnDiscrepancy = 0.0f;
			if (desiredTurn<0.0f)
			{
				turnDiscrepancy = desiredTurn-clipTurnLeft[NUM_TURN_CLIPS-1];
			}
			else
			{
				turnDiscrepancy = desiredTurn-clipTurnLeft[0];
			}

			TUNE_GROUP_FLOAT (PED_MOVEMENT, fMaxTurnDiscrepancyForHeadingLerp, 0.4f, 0.0f, PI, 0.0001f);
			if (abs(turnDiscrepancy) < fMaxTurnDiscrepancyForHeadingLerp)
			{
				// if the discrepancy is small, add a little extra turn
				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(turnDiscrepancy*fwTimer::GetTimeStep()*fMoveTurnDiscrepancyTurnRate);
			}
			else
			{
				// Otherwise, we need to restart our idle turn
				bCanMakeTurn = false;
			}
		}
		
		TUNE_GROUP_FLOAT (PED_MOVEMENT, fMoveTurnHeadingAdjustLerpRate, 12.0f, 0.0f, 20.0f, 0.0001f);
		TUNE_GROUP_BOOL (PED_MOVEMENT, bMoveTurnAdjustLerpAngular, false);
		InterpValue(turnAmount, Clamp(headingTarget, -PI, PI), fMoveTurnHeadingAdjustLerpRate, bMoveTurnAdjustLerpAngular);
	}
	else if (abs(remainingTurn)<fMinTurnAngleForHeadingAdjust)
	{
		// the remaining turn amount in the clip is tiny (less than our threshold)
		// if we still have a desired heading change at this point, we need to initiate a new turn
		bCanMakeTurn = false;
	}

	return bCanMakeTurn;
}


#define NUM_MOVE_CLIPS 3

dev_float remainDistScale = 0.5f;

//
// Helper function to determine how much distance is remaining to be moved in the current walk cycle
float CTaskMotionBasicLocomotion::CalculateRemainingMovementDistance() const
{
	static fwMvClipId moveClipNames[NUM_MOVE_CLIPS] = { fwMvClipId("walk",0x83504C9C), fwMvClipId("run",0x1109B569), fwMvClipId("sprint",0xBC29E48) };

	if (GetState()!=State_Locomotion)
		return 0.0f;

	const float moveBlendRatio = GetMotionData().GetCurrentMbrY();
	int clipIdx = 0;
	if (moveBlendRatio > MOVEBLENDRATIO_RUN)
		clipIdx = 2;
	if (moveBlendRatio > MOVEBLENDRATIO_WALK)
		clipIdx = 1;

	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	const crClip* moveClip = pSet->GetClip(moveClipNames[clipIdx]);

#if 0
	// Grab the walk phase and determine the remaining stride distance to the next footfall
	// The issue with this approach is that as soon as the clip crosses the footfall ratio the value jumps
	// and so anything waiting to stop the ped when their remaining distance gets close to the clip distance
	// remaining will then overshoot.  May need revisiting - for now using a fixed proportion of the stride length
	// seems to work better!
	static const char * moveSignalNames[NUM_MOVE_CLIPS] = { "Walk", "Run", "Sprint" };
	char paramId[24];

	sprintf(paramId, "%sPhase", moveSignalNames[clipIdx]);
	float clipPhase = m_moveNetworkHelper.GetFloat( fwMvFloatId(paramId) );

	float clipWeight = 1.0f;
	//sprintf(paramId, "%sWeight%d", moveSignalNames[i]);
	//clipWeights[i] = m_moveNetworkHelper.GetSignal(paramId);

	const float cRightFootDownPhase = 0.426f;
	const float cLeftFootDownPhase = 0.892f;

	float remainingDist = 0.0f;
	if (clipPhase >= 0.0f)
	{
		if (clipPhase > cLeftFootDownPhase)	// After foot heel L
			remainingDist = fwAnimDirector::GetMoverTrackPositionChange(moveClip, clipPhase, 1.0f).y + fwAnimDirector::GetMoverTrackPositionChange(moveClip, 0.0f, cRightFootDownPhase).y;
		else if (clipPhase < cRightFootDownPhase)
			remainingDist = fwAnimDirector::GetMoverTrackPositionChange(moveClip, clipPhase, cRightFootDownPhase).y * clipWeight;
		else
			remainingDist = fwAnimDirector::GetMoverTrackPositionChange(moveClip, clipPhase, cLeftFootDownPhase).y * clipWeight;
	}
//Printf("remainingDist %f clipPhase %f (%s)\n", remainingDist, clipPhase, paramId);
#else
	// Figure the stride length (between footfalls) as the worse case distance until we can run the stop clip
	
	// GSALES - 19/07/2011. Unfortunately we can't use this approach here, as the numbers of cycles in walk clips vary widely.

	float remainingDist = 0.f;
	if(moveClip)
	{
		remainingDist = fwAnimHelpers::GetMoverTrackTranslationDiffRotated(*moveClip, 0.0f, 1.0f).y * remainDistScale;	// just quarter the clip translation change - assume the walk cycle is somewhat regular.  Assume we can stop on either feet, so the average stop distance is half one foot move distance
	}
#endif

	return remainingDist;
}


//////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotion::StartMoveTurn()
{
	//send the start locomotion signals here

	// manually shift to the move turn state so we don't have to wait for the round trip from the MoVE network
	SetState(State_MoveTurn);

	// send the request to start our 180 turn
	static const fwMvRequestId start180TurnId("Start180Turn",0x9C47F232);
	m_moveNetworkHelper.SendRequest(start180TurnId);

	// Remove any extra turn that has already been added in SendParams() this frame
	m_smoothedDirection = 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool  CTaskMotionBasicLocomotion::StartMoveNetwork()
{
	CPed * pPed = GetPed();

	if (!m_moveNetworkHelper.IsNetworkActive())
	{		
		m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFoot);

		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssert(pSet);

		if (pSet->Request_DEPRECATED() && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFoot))
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFoot);
			m_moveNetworkHelper.SetClipSet(m_clipSetId);
		}
	}

	static const fwMvStateId idlesId("Idles",0xF29E80A8);
	static const fwMvStateId locomotionId("Locomotion",0xB65CBA9D);

	if (m_moveNetworkHelper.IsNetworkActive() && !m_moveNetworkHelper.IsNetworkAttached())
	{
		m_moveNetworkHelper.DeferredInsert();
		SendParams();

		if (m_flags.IsFlagSet(Flag_Crouching))
		{
			switch (GetMotionData().GetForcedMotionStateThisFrame())
			{		
			case CPedMotionStates::MotionState_Crouch_Idle:
				m_moveNetworkHelper.ForceStateChange(idlesId);
				GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
				break;
			case CPedMotionStates::MotionState_Crouch_Walk:
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
				break;
			case CPedMotionStates::MotionState_Crouch_Run:
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
				break;
			default:
				if (m_initialState==State_Locomotion)
				{
					m_moveNetworkHelper.ForceStateChange(locomotionId);
				}
				break;
			}
		}
		else
		{
			switch (GetMotionData().GetForcedMotionStateThisFrame())
			{
			case CPedMotionStates::MotionState_Idle:
				m_moveNetworkHelper.ForceStateChange(idlesId);
				GetMotionData().SetCurrentMoveBlendRatio(0.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(0.0f,0.0f);
				break;
			case CPedMotionStates::MotionState_Walk:
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				GetMotionData().SetCurrentMoveBlendRatio(1.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(1.0f,0.0f);
				break;
			case CPedMotionStates::MotionState_Run:
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				GetMotionData().SetCurrentMoveBlendRatio(2.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(2.0f,0.0f);
				break;
			case CPedMotionStates::MotionState_Sprint:
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				GetMotionData().SetCurrentMoveBlendRatio(3.0f,0.0f);
				GetMotionData().SetDesiredMoveBlendRatio(3.0f,0.0f);
				break;
			default:
				if (m_initialState==State_Locomotion)
				{
					m_moveNetworkHelper.ForceStateChange(locomotionId);
				}
				break;
			}
		}
	}
	else
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateInitial_OnEnter()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateInitial_OnUpdate()
{
	StartMoveNetwork();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateIdle_OnEnter()
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateIdle_OnUpdate()
{
	CPed * pPed = GetPed();

	// Give the animators a bit of a buffer when trying to hit headings. If in the idle state and close to the target heading
	// add some extra rotation in order to hit it.

	const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());

	TUNE_GROUP_FLOAT (PED_MOVEMENT, ms_fIdleHeadingMinDeltaToLerp, 0.300f,0.0f,PI,0.001f);
	TUNE_GROUP_FLOAT (PED_MOVEMENT, ms_fIdleHeadingLerpRate, 7.5f,0.0f,20.0f,0.001f);

	bool bWantsToStartMoving = false;

	if( Abs(fHeadingDelta) < ms_fIdleHeadingMinDeltaToLerp )
	{
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingDelta*fwTimer::GetTimeStep()*ms_fIdleHeadingLerpRate*((pPed->GetMovePed().GetTaskNetwork() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS)) ? 0.0f : 1.0f));
	}
	else
	{
		bWantsToStartMoving = true;
		if (IsNearZero(pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2()))
		{
			// in this case we want to zero the current velocity and has desired velocity flag, otherwise we can wind up doing a walk start by accident
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f);
			m_moveNetworkHelper.SetFlag(false, ms_HasDesiredVelocityId);
			m_moveNetworkHelper.SetFloat(ms_SpeedId, 0.0f);
		}
	}

	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMinSpeedToStartMoving, 0.1f,0.0f,1.0f,0.001f);

	if (pPed->GetMotionData()->GetDesiredMoveBlendRatio().y>fMinSpeedToStartMoving)
	{
		bWantsToStartMoving = true;
	}

	if (bWantsToStartMoving)
	{
		TUNE_GROUP_INT (PED_MOVEMENT, iNumWaitFramesForMoveStart, 0, 0, 15, 1);

		if (++m_moveStartWaitCounter > iNumWaitFramesForMoveStart)
		{
			StartMoving();
			m_moveStartWaitCounter=0;
		}
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateIdle_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartRight_OnEnter()
{
	m_moveNetworkHelper.SetFlag(true, ms_MoveStartRightVisibleId);
	m_bShouldUpdateMoveTurn = true;
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartRight_OnUpdate()
{	
	CPed* pPed = GetPed();

	if (m_bShouldUpdateMoveTurn)
	{
		bool bStartNewTurn = false;

		// there are two reasons we may kick off a new move start here:

		// 1. If there's not enough turn in the clip to make the desired direction by the end of it.
		if (AdjustTurnForDesiredDirection(m_moveDirection))
		{
			m_moveNetworkHelper.SetFloat(ms_StartDirectionId, ConvertRadianToSignal(m_moveDirection));
			m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionId, Abs(SubtractAngleShorter(m_referenceDesiredDirection, pPed->GetDesiredHeading())));
		}
		else
		{
			bStartNewTurn = true;
		}

		// 2. If the ped is trying to walk and we're in an idle turn system
		if (!bStartNewTurn)
		{
			if (m_moveNetworkHelper.GetClip(ms_IdleTurnClip0Id) && pPed->GetMotionData()->GetCurrentMoveBlendRatio().y > 0.3f )
			{
				bStartNewTurn = true;
			}
		}

		if (bStartNewTurn)
		{
			//If it looks like we're not going to hit the full turn, kick off a direction change here
			if (m_moveNetworkHelper.GetFootstep(ms_AefFootHeelLId) || m_moveNetworkHelper.GetFootstep(ms_AefFootHeelRId) || m_moveNetworkHelper.GetBoolean(ms_FinishedIdleTurnId))
			{
				m_bShouldUpdateMoveTurn = false;
				StartMoving();
			}
			else
			{
				m_moveNetworkHelper.SetFloat(ms_StartDirectionId, ConvertRadianToSignal(m_moveDirection));
				m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionId, Abs(SubtractAngleShorter(m_referenceDesiredDirection, pPed->GetDesiredHeading())));
			}
		}
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartRight_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartLeft_OnEnter()
{
	m_moveNetworkHelper.SetFlag(true, ms_MoveStartLeftVisibleId);
	m_bShouldUpdateMoveTurn = true;
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartLeft_OnUpdate()
{	
	CPed* pPed = GetPed();

	if (m_bShouldUpdateMoveTurn)
	{
		bool bStartNewTurn = false;

		// there are two reasons we may kick off a new move start here:

		// 1. If there's not enough turn in the clip to make the desired direction by the end of it.

		if (AdjustTurnForDesiredDirection(m_moveDirection))
		{
			m_moveNetworkHelper.SetFloat(ms_StartDirectionLeftId, ConvertRadianToSignal(m_moveDirection));
			m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionLeftId, Abs(SubtractAngleShorter(m_referenceDesiredDirection, pPed->GetDesiredHeading())));
		}
		else
		{
			bStartNewTurn = true;
		}

		// 2. If the ped is trying to walk and we're in an idle turn system
		if (!bStartNewTurn)
		{
			if (m_moveNetworkHelper.GetClip(ms_IdleTurnClip0LId) && pPed->GetMotionData()->GetCurrentMoveBlendRatio().y > 0.3f)
			{
				bStartNewTurn = true;
			}
		}

		if (bStartNewTurn)
		{
			//If it looks like we're not going to hit the full turn and we have an opportunity to do so, kick off a direction change here
			if (m_moveNetworkHelper.GetFootstep(ms_AefFootHeelLId) || m_moveNetworkHelper.GetFootstep(ms_AefFootHeelRId) || m_moveNetworkHelper.GetBoolean(ms_FinishedIdleTurnId))
			{
				m_bShouldUpdateMoveTurn = false;
				StartMoving();
			}
			else
			{
				m_moveNetworkHelper.SetFloat(ms_StartDirectionLeftId, ConvertRadianToSignal(m_moveDirection));
				m_moveNetworkHelper.SetFloat(ms_StartDirectionToDesiredDirectionLeftId, Abs(SubtractAngleShorter(m_referenceDesiredDirection, pPed->GetDesiredHeading())));
			}
		}
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStartLeft_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateChangeDirection_OnUpdate()
{	
	//trigger the move to the state we're interested in
	StartMoving();
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateLocomotion_OnEnter()
{
	m_referenceDirection = CalcDesiredDirection();
	m_moveNetworkHelper.SetFlag(true, ms_LocomotionVisibleId);
	m_smoothedSlopeDirection = 0.f;
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateLocomotion_OnUpdate()
{
	CPed* pPed = GetPed();

	float desiredDirection = CalcDesiredDirection();

	TUNE_GROUP_FLOAT ( PED_MOVEMENT, fMoveTurnActivationAngle, 0.4f, 0.0f, PI, 0.001f );

	if ( !IsNearZero(pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2()) && abs( SubtractAngleShorter( desiredDirection, m_referenceDirection ) ) > (PI - fMoveTurnActivationAngle) && (abs(m_referenceDirection) < fMoveTurnActivationAngle) )
	{
		StartMoveTurn();
	}

	m_moveNetworkHelper.SetFloat(ms_EndDirectionId, ConvertRadianToSignal(desiredDirection));

// 	if(!m_flags.IsFlagSet(Flag_Crouching) && pPed->GetSlopeDetected())
// 	{
// 		Vector3 vNormal = pPed->GetSlopeNormal();
// 		float fSlopePitch = rage::Atan2f(vNormal.XYMag(), vNormal.z);
// 		vNormal.z = 0.f;
// 		if(vNormal.Mag2() > 0.f)
// 		{
// 			vNormal.NormalizeFast();
// 		}
// 		float fSlopeHeadingDot = -vNormal.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
// 		fSlopePitch *= fSlopeHeadingDot;
// 
// 		TUNE_GROUP_FLOAT ( PED_MOVEMENT, fLocomotionSlopeDirectionSmoothing, 6.0f, 0.0f, 20.0f, 0.001f );
// 
// 		float fHeadingDiff = SubtractAngleShorter(fSlopePitch, m_smoothedSlopeDirection);
// 		float fAbsHeadingDiff = abs(fHeadingDiff);
// 		float fFrameChange = Clamp(fLocomotionSlopeDirectionSmoothing*fwTimer::GetTimeStep()*Sign(fHeadingDiff), -fAbsHeadingDiff, fAbsHeadingDiff);
// 		m_smoothedSlopeDirection += fFrameChange;
// 		m_smoothedSlopeDirection = fwAngle::LimitRadianAngleSafe( m_smoothedSlopeDirection );
// 
// 		m_moveNetworkHelper.SetFloat(ms_SlopeDirectionId, ConvertRadianToSignal(m_smoothedSlopeDirection));
// 		if(pPed->GetStairsDetected())
// 		{
// 			m_moveNetworkHelper.SetFlag(true, ms_StairWalkFlagId);
// 		}
// 		else
// 		{
// 			TUNE_GROUP_BOOL ( PED_MOVEMENT, bUseSlopeAnims, true );
// 			if(bUseSlopeAnims)
// 			{
// 				m_moveNetworkHelper.SetFlag(true, ms_SlopeWalkFlagId);
// 			}
// 		}
// 	}
// 	else
// 	{
// 		m_smoothedSlopeDirection = 0.f;
// 		m_moveNetworkHelper.SetFlag(false, ms_StairWalkFlagId);
// 		m_moveNetworkHelper.SetFlag(false, ms_SlopeWalkFlagId);
// 	}

	m_referenceDirection = desiredDirection;

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateLocomotion_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveTurn_OnEnter()
{
	//store the target direction of our turn so that we can figure out if we need to turn more to hit it
	m_referenceDirection = fwAngle::LimitRadianAngleSafe(GetPed()->GetCurrentHeading()+PI);
	m_moveNetworkHelper.SetFlag(true, ms_BlockMoveStopId);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveTurn_OnUpdate()
{
	// Apply some extra steering here
	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMoveTurnExtraHeadingChangeMultiplier, 0.36f, 0.0f, 1.0f, 0.0001f );

	CPed* pPed = GetPed();

	float fExtraTurn = InterpValue(m_referenceDirection, fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading()), fMoveTurnExtraHeadingChangeMultiplier, true);

	float fExtraHeadingChangeThisFrame = GetPed()->GetMotionData()->GetExtraHeadingChangeThisFrame() + fExtraTurn*((pPed->GetMovePed().GetTaskNetwork() ? 0.0f : 1.0f));
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fExtraHeadingChangeThisFrame);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveTurn_OnExit()
{
	m_moveNetworkHelper.SetFlag(false, ms_BlockMoveStopId);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStop_OnEnter()
{
	CPed * pPed = GetPed();

	// calculate and pass in a direction to hold throughout the turn
	m_moveDirection = CalcDesiredDirection();
	m_referenceDirection = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());

	m_moveNetworkHelper.SetFloat(ms_EndDirectionId, ConvertRadianToSignal(m_moveDirection));
	m_moveNetworkHelper.SetFlag(true, ms_MoveStopVisibleId);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStop_OnUpdate()
{
	// try gradually interpolating the turn direction towards the current direction.
	// should help to close direction gaps on turns
	CPed* pPed = GetPed();

	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMoveStopHeadingLerpRate, 0.4f, 0.0f, 50.0f, 0.001f);

	float targetAngle = SubtractAngleShorter(
		fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading()),
		m_referenceDirection
		);

	LerpAngleNoWrap(m_moveDirection, targetAngle, fMoveStopHeadingLerpRate);
	m_moveNetworkHelper.SetFloat(ms_EndDirectionId, ConvertRadianToSignal(m_moveDirection));

	TUNE_GROUP_FLOAT (PED_MOVEMENT, fMoveStopHurryRateMultiplyer, 2.0f, 0.0f, 50.0f, 0.001f);

	if (GetMotionData().GetDesiredMoveBlendRatio().Mag2()>0.0f)
	{
		m_moveNetworkHelper.SetFloat(ms_WalkStopRateId, fMoveStopHurryRateMultiplyer);
	}
	else
	{
		m_moveNetworkHelper.SetFloat(ms_WalkStopRateId, 1.0f);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateMoveStop_OnExit()
{
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionBasicLocomotion::StateFinish_OnEnter()
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskMotionBasicLocomotion::StateFinish_OnUpdate()
{
	return FSM_Continue;
}

void CTaskMotionBasicLocomotion::ProcessMovement(CPed* pPed, float fTimeStep)
{
	//******************************************************************************************
	// This is the work which was done inside CPedMoveBlendOnFoot to model player/ped movement

	const bool bIsPlayer = pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame();

	// Interpolate the current moveblendratio towards the desired
	const float fAccel = bIsPlayer ? ms_fPlayerMoveAccel : ms_fPedMoveAccel;
	const float fDecel = bIsPlayer ? ms_fPlayerMoveDecel : ms_fPedMoveDecel;
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
	
	vCurrentMBR.x = 0.0f;

	if(vDesiredMBR.y > vCurrentMBR.y)
	{
		vCurrentMBR.y += fAccel * fTimeStep;
		vCurrentMBR.y = MIN(vCurrentMBR.y, MIN(vDesiredMBR.y, MOVEBLENDRATIO_SPRINT));
	}
	else if(vDesiredMBR.y < vCurrentMBR.y)
	{
		vCurrentMBR.y -= fDecel * fTimeStep;
		vCurrentMBR.y = MAX(vCurrentMBR.y, MAX(vDesiredMBR.y, MOVEBLENDRATIO_STILL));
	}
	else
	{
		vCurrentMBR.y = vDesiredMBR.y;
	}

// 	if(pPed->GetSlopeDetected())
// 	{
// 		// Clamp to run on slopes/stairs
// 		vCurrentMBR.y = MIN(vCurrentMBR.y, MOVEBLENDRATIO_RUN);
// 	}

	// give the animated velocity change as the current turn velocity
	float fCurrentTurnVelocity = pPed->GetAnimatedAngularVelocity();


	// Copy variables back into moveblender.  This is a temporary measure.
	pPed->GetMotionData()->SetCurrentMoveBlendRatio(vCurrentMBR.y, vCurrentMBR.x);
	pPed->GetMotionData()->SetCurrentTurnVelocity(fCurrentTurnVelocity);
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	// Speed override lerp
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplier, 2.0, 1.0f, 20.0f, 0.5f);
	float	deltaOverride = pPed->GetMotionData()->GetDesiredMoveRateOverride() - pPed->GetMotionData()->GetCurrentMoveRateOverride();
	deltaOverride *= (fTendToDesiredMoveRateOverrideMultiplier * fTimeStep);
	deltaOverride += pPed->GetMotionData()->GetCurrentMoveRateOverride();
	pPed->GetMotionData()->SetCurrentMoveRateOverride(deltaOverride);
}

void CTaskMotionBasicLocomotion::SendWeaponSignals()
{
	static fwMvClipId s_idleClip("idle",0x71C21326);
	static fwMvClipId s_walkClip("walk",0x83504C9C);
	static fwMvClipId s_runClip("run",0x1109B569);


	float weaponWeight = 0.0f;
	const crClip* idleClip = NULL;
	const crClip* walkClip = NULL;
	const crClip* runClip = NULL;

	// deal with parameters for the partial weapon clip sets
	if (m_weaponClipSet != CLIP_SET_ID_INVALID && fwClipSetManager::Request_DEPRECATED(m_weaponClipSet))
	{
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_weaponClipSet);

		idleClip = clipSet->GetClip(s_idleClip);
		walkClip = clipSet->GetClip(s_walkClip);
		runClip = clipSet->GetClip(s_runClip);

		weaponWeight = 1.0f;
	}

	// send the signals to MoVE
	m_moveNetworkHelper.SetClip( idleClip, ms_IdleId );
	m_moveNetworkHelper.SetClip( walkClip, ms_WalkId );
	m_moveNetworkHelper.SetClip( runClip, ms_RunId );
	m_moveNetworkHelper.SetFloat( ms_WeaponWeightId, weaponWeight );

	// TODO: Replace this with a hash loaded from weapon metadata once the filters are no longer demand loaded
	m_moveNetworkHelper.SetFilter(m_pWeaponFilter, ms_WeaponFilterId);
}

void CTaskMotionBasicLocomotion::SendAlternateWalkSignals()
{
	if (m_alternateClips[ACT_Walk].IsValid())
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_alternateClips[ACT_Walk].iDictionaryIndex, m_alternateClips[ACT_Walk].uClipHash.GetHash());

		m_moveNetworkHelper.SetClip(pClip, ms_ClipId);
		m_moveNetworkHelper.SetBoolean(ms_LoopedId, GetAlternateClipLooping(ACT_Walk));
		m_moveNetworkHelper.SetFloat(ms_DurationId, m_alternateClips[ACT_Walk].fBlendDuration);
		m_moveNetworkHelper.SetFlag(true, ms_AlternateWalkFlagId);
	}
	else
	{
		m_moveNetworkHelper.SetClip(NULL, ms_ClipId);
		m_moveNetworkHelper.SetFlag(false, ms_AlternateWalkFlagId);
	}
	
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotion::StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping)
{
	if (data.IsValid())
	{
		m_alternateClips[type] = data;
		SetAlternateClipLooping(type, bLooping);

		if (m_moveNetworkHelper.IsNetworkActive())
		{
			if (type == ACT_Walk)
			{
				m_moveNetworkHelper.SendRequest(ms_AlternateWalkChangedId);
				SendAlternateWalkSignals();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotion::EndAlternateClip(AlternateClipType type, float fBlendDuration)
{
	StopAlternateClip(type, fBlendDuration);

	if (m_moveNetworkHelper.IsNetworkActive())
	{
		if (type == ACT_Walk)
		{
			SendAlternateWalkSignals();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

// Instance of tunable task params
CTaskMotionBasicLocomotionLowLod::Tunables CTaskMotionBasicLocomotionLowLod::ms_Tunables;	
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskMotionBasicLocomotionLowLod, 0x8d4a40fa);

const fwMvFloatId CTaskMotionBasicLocomotionLowLod::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskMotionBasicLocomotionLowLod::ms_MovementRateId("Movement_Rate",0x6D2F67D0);
const fwMvFloatId CTaskMotionBasicLocomotionLowLod::ms_MovementInitialPhaseId("Movement_InitialPhase",0x4AAD353B);
const fwMvClipId CTaskMotionBasicLocomotionLowLod::ms_WeaponIdleId("WeaponIdleClip",0x7560D6C2);
const fwMvClipId CTaskMotionBasicLocomotionLowLod::ms_WeaponRunId("WeaponRunClip",0xB8A9F0BB);
const fwMvClipId CTaskMotionBasicLocomotionLowLod::ms_WeaponWalkId("WeaponWalkClip",0x211B8B7);
const fwMvFloatId CTaskMotionBasicLocomotionLowLod::ms_WeaponAnimWeightId("WeaponAnimWeight",0x277F099E);
const fwMvFilterId CTaskMotionBasicLocomotionLowLod::ms_WeaponAnimFilterId("WeaponAnimFilter",0x303714BA);
dev_float CTaskMotionBasicLocomotionLowLod::ms_fHeadingMoveThreshold = PI*0.5f;
///////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::CTaskMotionBasicLocomotionLowLod(State initialState, const fwMvClipSetId& clipSetId)
: m_clipSetId(clipSetId)
, m_clipSetIdForCachedSpeeds(CLIP_SET_ID_INVALID)
, m_initialState(initialState)
, m_fExtraHeadingRate(0.f)
, m_bMovementClipSetChanged(false)
, m_bMovementInitialPhaseSet(false)
, m_weaponClipSet(u32(0))
, m_pWeaponFilter(NULL)
{
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID);
	SetInternalTaskType(CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION_LOW_LOD);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::~CTaskMotionBasicLocomotionLowLod()
{
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskMotionBasicLocomotionLowLod::Debug() const
{
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskMotionBasicLocomotionLowLod::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial:    return "Initial";
	case State_Idle:       return "Idle";
	case State_Locomotion: return "Locomotion";
	default:               return "Unknown";
	}
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionBasicLocomotionLowLod::SupportsMotionState(CPedMotionStates::eMotionState state)
{
	switch(state)
	{
	case CPedMotionStates::MotionState_Idle: //intentional fall through
	case CPedMotionStates::MotionState_Walk:
	case CPedMotionStates::MotionState_Run:
	case CPedMotionStates::MotionState_Sprint:
		return true;
	default:
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionBasicLocomotionLowLod::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	switch(state)
	{
	case CPedMotionStates::MotionState_Idle:
		return GetState()==State_Idle;
	case CPedMotionStates::MotionState_Walk:
		return (GetState()==State_Locomotion && GetMotionData().GetIsWalking());
	case CPedMotionStates::MotionState_Run:
		return (GetState()==State_Locomotion && GetMotionData().GetIsRunning());
	case CPedMotionStates::MotionState_Sprint:
		return (GetState()==State_Locomotion && GetMotionData().GetIsSprinting());
	default:
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds)
{
	// If this function was called for the same clip set ID last time, we can use
	// the speeds we cached off, as an optimization.
	if(m_clipSetIdForCachedSpeeds == m_clipSetId)
	{
		speeds = m_cachedSpeeds;
		return;
	}

	speeds.Zero();

	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskAssert(pSet);

	static const fwMvClipId s_walkClip("walk",0x83504C9C);
	static const fwMvClipId s_runClip("run",0x1109B569);
	static const fwMvClipId s_sprintClip("sprint",0xBC29E48);

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*pSet, speeds, clipNames);

	// Store the speeds, and the associated clip set ID, so we can reuse them the next time
	// this function is called, if the clip set hasn't changed.
	m_cachedSpeeds = speeds;
	m_clipSetIdForCachedSpeeds = m_clipSetId;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionBasicLocomotionLowLod::IsInMotion(const CPed* UNUSED_PARAM(pPed)) const
{
	return GetState() == State_Locomotion;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	outClip = CLIP_IDLE;
	outClipSet = m_clipSetId;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CTaskMotionBasicLocomotionLowLod::CreatePlayerControlTask()
{
	return rage_new CTaskPlayerOnFoot();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::SetMovementClipSet(const fwMvClipSetId& clipSetId)
{
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	if(m_clipSetId != clipSetId)
	{
		// Store the new clip set
		m_clipSetId = clipSetId;

		bool bClipSetStreamed = false;
		fwClipSet* pMovementClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
		if(taskVerifyf(pMovementClipSet, "Movement clip set [%s] doesn't exist", m_clipSetId.GetCStr()))
		{
			if(pMovementClipSet->Request_DEPRECATED())
			{
				// Clip set streamed
				bClipSetStreamed = true;
			}
		}

		if(m_moveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendMovementClipSet();
		}
		else
		{
			ASSERT_ONLY(VerifyLowLodMovementClipSet(m_clipSetId, GetEntity() ? GetPed() : NULL));
			// Flag the change
			m_bMovementClipSetChanged = true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::ProcessPreFSM()
{
	CTaskMotionBasicLocomotion::ProcessMovement(GetPed(), GetTimeStep());
	UpdateHeading();

	// When we run this low LOD motion task, also allow time slicing of AI tasks.
	CPedAILod& lod = GetPed()->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	lod.SetUnconditionalTimesliceIntelligenceUpdate(true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Idle)
			FSM_OnEnter
				return StateIdle_OnEnter();
			FSM_OnUpdate
				return StateIdle_OnUpdate();
			FSM_OnExit
				return StateIdle_OnExit();
		FSM_State(State_Locomotion)
			FSM_OnEnter
				return StateLocomotion_OnEnter();
			FSM_OnUpdate
				return StateLocomotion_OnUpdate();
			FSM_OnExit
				return StateLocomotion_OnExit();
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::ProcessPostFSM()
{
	SendParams();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::CleanUp()
{
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateInitial_OnUpdate()
{
	CPed* pPed = GetPed();

	if(!m_moveNetworkHelper.IsNetworkActive())
	{
		m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHumanLowLod);

		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssertf(pSet, "Movement clip set [%s] does not exist", m_clipSetId.GetCStr());
		if(taskVerifyf(pSet->Request_DEPRECATED(), "Movement clip set [%s] not streamed in when starting low lod motion task, will cause a t-pose PS(%s), TR(%.2f), TIS(%.2f)", m_clipSetId.GetCStr(), GetStaticStateName(GetPreviousState()), GetTimeRunning(), GetTimeInState()) && 
			taskVerifyf(m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskOnFootHuman), "ms_NetworkTaskOnFootHumanLowLod not streamed"))
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootHumanLowLod);
			m_moveNetworkHelper.SetClipSet(m_clipSetId);
		}
	}

	static const fwMvStateId idleId("Idle",0x71C21326);
	static const fwMvStateId locomotionId("Locomotion",0xB65CBA9D);

	if(m_moveNetworkHelper.IsNetworkActive())
	{
		if(!m_moveNetworkHelper.IsNetworkAttached())
		{
			m_moveNetworkHelper.DeferredInsert();
		}
		switch(GetMotionData().GetForcedMotionStateThisFrame())
		{
		case CPedMotionStates::MotionState_Idle:
			m_moveNetworkHelper.ForceStateChange(idleId);
			GetMotionData().SetCurrentMoveBlendRatio(0.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(0.0f, 0.0f);
			SetState(State_Idle);
			break;

		case CPedMotionStates::MotionState_Walk:
			m_moveNetworkHelper.ForceStateChange(locomotionId);
			GetMotionData().SetCurrentMoveBlendRatio(1.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(1.0f, 0.0f);
			SetState(State_Locomotion);
			break;

		case CPedMotionStates::MotionState_Run:
			m_moveNetworkHelper.ForceStateChange(locomotionId);
			GetMotionData().SetCurrentMoveBlendRatio(2.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(2.0f, 0.0f);
			SetState(State_Locomotion);
			break;

		case CPedMotionStates::MotionState_Sprint:
			m_moveNetworkHelper.ForceStateChange(locomotionId);
			GetMotionData().SetCurrentMoveBlendRatio(3.0f, 0.0f);
			GetMotionData().SetDesiredMoveBlendRatio(3.0f, 0.0f);
			SetState(State_Locomotion);
			break;

		default:
			if(m_initialState == State_Locomotion)
			{
				m_moveNetworkHelper.ForceStateChange(locomotionId);
				SetState(State_Locomotion);
			}
			else
			{
				m_moveNetworkHelper.ForceStateChange(idleId);
				SetState(State_Idle);
			}
			break;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateIdle_OnEnter()
{
	static const fwMvBooleanId idleOnEnterId("State_Idle",0xDD0BC665);
	static const fwMvRequestId startIdleId("StartIdle",0x2A8F6981);

	m_moveNetworkHelper.WaitForTargetState(idleOnEnterId);
	m_moveNetworkHelper.SendRequest(startIdleId);

	// Zero out the mbr
	GetMotionData().SetCurrentMoveBlendRatio(0.f);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateIdle_OnUpdate()
{
	if(!m_moveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	static dev_float MOVING_SPEED = 0.01f;
	if(pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2() > square(MOVING_SPEED))
	{
		SetState(State_Locomotion);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateIdle_OnExit()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateLocomotion_OnEnter()
{
	static const fwMvBooleanId locomotionOnEnterId("State_Locomotion",0x15885610);
	static const fwMvRequestId startLocomotionId("StartLocomotion",0x15F666A1);

	m_moveNetworkHelper.WaitForTargetState(locomotionOnEnterId);
	m_moveNetworkHelper.SendRequest(startLocomotionId);

	// Allow initial phase to be set
	m_bMovementInitialPhaseSet = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateLocomotion_OnUpdate()
{
	CPed* pPed = GetPed();

	bool bInTargetState = m_moveNetworkHelper.IsInTargetState();

	if(!m_bMovementInitialPhaseSet && !bInTargetState)
	{
		m_moveNetworkHelper.SetFloat(ms_MovementInitialPhaseId, (float)(pPed->GetRandomSeed()/(float)RAND_MAX_16));
	}
	else
	{
		// Flag as set once network is in correct state.
		m_bMovementInitialPhaseSet = true;
	}

	if(!bInTargetState)
	{
		return FSM_Continue;
	}

	static dev_float MOVING_SPEED = 0.01f;
	if(pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2() <= square(MOVING_SPEED))
	{
		SetState(State_Idle);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionBasicLocomotionLowLod::FSM_Return CTaskMotionBasicLocomotionLowLod::StateLocomotion_OnExit()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::UpdateHeading()
{
	CPed* pPed = GetPed();
	if(pPed && !pPed->GetUsingRagdoll() && !pPed->IsDead())
	{
		float fExtraHeading;
		// Get the desired direction
		float fDesiredDirection = CalcDesiredDirection();

		if(Sign(fDesiredDirection) != Sign(m_fExtraHeadingRate))
		{
			// Reset
			m_fExtraHeadingRate = 0.f;
		}

		CPed* pPed = GetPed();

		float timeStep = GetTimeStep();

		// If time slicing is enabled, consider preventing time sliced updates if we are not facing
		// the direction we want to be.
		if(CPedAILodManager::IsTimeslicingEnabled())
		{
			// Check if we are on screen, otherwise this is probably not worth doing.
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				// Get the absolute heading delta.
				const float absHeadingDelta = fabsf(fDesiredDirection);

				// Determine the threshold, with hysteresis based on if we are already timesliced or not.
				float threshold;
				if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
				{
					threshold = ms_Tunables.m_ForceUpdatesWhenTurningStartThresholdRadians;
				}
				else
				{
					threshold = ms_Tunables.m_ForceUpdatesWhenTurningStopThresholdRadians;
				}

				// Check if we exceeded the threshold.
				if(absHeadingDelta > threshold)
				{
					// Shrink the time step used in this function, so we don't do a large heading
					// change - that's what we are trying to avoid.
					timeStep = fwTimer::GetTimeStep();

					// Make sure we stay out of timesliced update mode until we are facing the desired direction again.
					pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				}
			}
		}

		// Move towards target rate
		Approach(m_fExtraHeadingRate, ms_Tunables.MovingExtraHeadingChangeRate * Sign(fDesiredDirection), ms_Tunables.MovingExtraHeadingChangeRateAcceleration, timeStep);
		if(fDesiredDirection < 0.f)
		{
			fExtraHeading = Max(m_fExtraHeadingRate * timeStep, fDesiredDirection);
		}
		else
		{
			fExtraHeading = Min(m_fExtraHeadingRate * timeStep, fDesiredDirection);
		}

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fExtraHeading);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::SendParams()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		CPed* pPed = GetPed();
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();

		// Speed
		const float ONE_OVER_SPINT = 1.0f / MOVEBLENDRATIO_SPRINT;
		float fSpeed = vCurrentMBR.y * ONE_OVER_SPINT; // normalize the speed values between 0.0f and 1.0f

		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
		{
			if( Abs( SubtractAngleShorter(pPed->GetMotionData()->GetCurrentHeading(), pPed->GetMotionData()->GetDesiredHeading())) >= ms_fHeadingMoveThreshold )
			{
				fSpeed = 0.0f;
			}
		}

		m_moveNetworkHelper.SetFloat(ms_SpeedId, fSpeed);

		// Movement rate
		m_moveNetworkHelper.SetFloat(ms_MovementRateId, pPed->GetMotionData()->GetCurrentMoveRateOverride());

		// Movement clip set
		if(m_bMovementClipSetChanged)
		{
			fwClipSet* pMovementClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
			if(pMovementClipSet)
			{
				if(pMovementClipSet->Request_DEPRECATED())
				{
					SendMovementClipSet();
				}
			}
		}

		SendWeaponSignals();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::SendMovementClipSet()
{
	m_moveNetworkHelper.SetClipSet(m_clipSetId);

	// Don't do this again
	m_bMovementClipSetChanged = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::SetWeaponClipSet(const fwMvClipSetId &clipSetId, atHashWithStringNotFinal filter, const CPed* pPed)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if (clipSetId.GetHash()!=0)
	{
		if (m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}

		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(filter.GetHash(), pPed);

		if(m_pWeaponFilter)	
		{
			m_pWeaponFilter->AddRef();
		}
	}
	else
	{
		if (m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}
	}

	m_weaponClipSet = clipSetId;
}

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskMotionBasicLocomotionLowLod::VerifyLowLodMovementClipSet(const fwMvClipSetId &clipSetId, const CPed *pPed)
{
	if(clipSetId!= CLIP_SET_ID_INVALID)
	{
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", clipSetId.TryGetCStr()))
		{
			atArray<CTaskHumanLocomotion::sVerifyClip> clips;
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("idle",0x71c21326),	CTaskHumanLocomotion::VCF_ZeroDuration));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("walk",0x83504C9C),	CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("run",0x1109b569),		CTaskHumanLocomotion::VCF_MovementCycle));
			clips.PushAndGrow(CTaskHumanLocomotion::sVerifyClip(fwMvClipId("sprint",0x0bc29e48),	CTaskHumanLocomotion::VCF_MovementCycle));

			CTaskHumanLocomotion::VerifyClips(clipSetId, clips, pPed);
		}
	}
}
#endif	//__ASSERT

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionBasicLocomotionLowLod::SendWeaponSignals()
{
	static fwMvClipId s_idleClip("idle",0x71C21326);
	static fwMvClipId s_walkClip("walk",0x83504C9C);
	static fwMvClipId s_runClip("run",0x1109B569);


	float weaponWeight = 0.0f;
	const crClip* idleClip = NULL;
	const crClip* walkClip = NULL;
	const crClip* runClip = NULL;

	// deal with parameters for the partial weapon clip sets
	if (m_weaponClipSet != CLIP_SET_ID_INVALID && fwClipSetManager::Request_DEPRECATED(m_weaponClipSet))
	{
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(m_weaponClipSet);

		idleClip = clipSet->GetClip(s_idleClip);
		walkClip = clipSet->GetClip(s_walkClip);
		runClip = clipSet->GetClip(s_runClip);

		weaponWeight = 1.0f;
	}

	// send the signals to MoVE
	m_moveNetworkHelper.SetClip( idleClip, ms_WeaponIdleId );
	m_moveNetworkHelper.SetClip( walkClip, ms_WeaponWalkId );
	m_moveNetworkHelper.SetClip( runClip, ms_WeaponRunId );
	m_moveNetworkHelper.SetFloat( ms_WeaponAnimWeightId, weaponWeight );

	// TODO: Replace this with a hash loaded from weapon metadata once the filters are no longer demand loaded
	m_moveNetworkHelper.SetFilter(m_pWeaponFilter, ms_WeaponAnimFilterId);
}