#include "TaskFall.h"

#include "animation/Move.h"
#include "animation/MovePed.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Weapons/Inventory/PedInventory.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/System/MotionTaskData.h"
#include "Task/Default/TaskPlayer.h"
#include "modelinfo/PedModelInfo.h"
#include "event/EventReactions.h"
#include "event/EventDamage.h"
#include "event/EventShocking.h"
#include "Event/EventSound.h"
#include "event/ShockingEvents.h"
#include "System/control.h"
#include "Frontend/MiniMap.h"
#include "Physics/physics.h"
#include "Camera/CamInterface.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskInWater.h"

#define	FOOTFALL_TAG_SYNC

//////////////////////////////////////////////////////////////////////////
AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()
//////////////////////////////////////////////////////////////////////////

// Tunable parameters. ///////////////////////////////////////////////////
CTaskFall::Tunables CTaskFall::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskFall, 0x916102cc);
//////////////////////////////////////////////////////////////////////////

// Move signals/events
const fwMvFlagId	CTaskFall::ms_bOnGround("bOnGround",0x5B203C70);
const fwMvFlagId	CTaskFall::ms_bStandingLand("bStandingLand",0xFAA107A3);
const fwMvFlagId	CTaskFall::ms_bWalkLand("bWalkLand",0xFE3F0965);
const fwMvFlagId	CTaskFall::ms_bPlummeting("bPlummeting",0xDEA75DF2);
const fwMvFlagId	CTaskFall::ms_bLandRoll("bLandRoll",0xcdffae2c);
const fwMvFlagId	CTaskFall::ms_bJumpFallLeft("bJumpFallLeft",0xC6C1083A);
const fwMvFlagId	CTaskFall::ms_bJumpFallRight("bJumpFallRight",0x6684AD01);
const fwMvFlagId	CTaskFall::ms_bDive("bDive",0xBE3E0668);
const fwMvFlagId	CTaskFall::ms_bVaultFall("bVaultFall",0x77957836);

const fwMvFloatId	CTaskFall::ms_fMaxFallDistance("fMaxFallDistance",0x77F3187D);
const fwMvFloatId	CTaskFall::ms_fFallDistanceRemaining("fFallDistanceRemaining",0x7D46523D);
const fwMvFloatId	CTaskFall::ms_fDiveControl("fDiveControl",0xad1fea60);
const fwMvFloatId	CTaskFall::ms_MoveSpeedId("Speed",0xF997622B);

const fwMvBooleanId CTaskFall::ms_InterruptibleId("Interruptible",0x550A14CF);
const fwMvBooleanId	CTaskFall::ms_bLandComplete("bLandComplete",0x5CFBE6F1);
const fwMvBooleanId CTaskFall::ms_AimInterruptId("AimInterrupt",0x12398EF2);

const fwMvClipSetVarId	CTaskFall::ms_UpperBodyClipSetId("UpperBodyClipSet",0x49BB9318);

static const float MAX_FALL_ACCEL_LIMIT = 100.0f;
static const float MAX_FALL_Z_SPEED = -120.0f; //! terminal velocity (ish)

static const float s_fInvalidHeight = -9999.0f;

//////////////////////////////////////////////////////////////////////////

CTaskFall::CTaskFall(fwFlags32 iFlags)
:m_bOffGround(true)
,m_fFallTime(0.0f)
,m_fMaxFallDistance(0.0f)
,m_fWaterDepth(0.0f)
,m_fFallDistanceRemaining(0.0f)
,m_iFlags(iFlags)
,m_fInitialMBR(-1.0f)
,m_vMaxFallSpeed(VEC3_ZERO)
,m_vInitialFallVelocity(VEC3_ZERO)
,m_vStickVelocity(VEC3_ZERO)
,m_bForceRunOnExit(false)
,m_bForceWalkOnExit(false)
,m_bStandingLand(false)
,m_bLandRoll(false)
, m_bRemoteJumpButtonDown(false)
,m_bWalkLand(false)
,m_bProcessInterrupt(false)
,m_bRestartIdle(false)
,m_fInitialPedZ(s_fInvalidHeight)
,m_fCachedDiveWaterHeight(s_fInvalidHeight)
,m_fDiveControlWeighting(0.0f)
,m_fExtraDistanceForDiveControl(0.0f)
,m_fLastPedZ(0.0f)
,m_fStuckTimer(0.0f)
,m_bSetInitialVelocity(false)
,m_bCloneVaultForceLand(false)
,m_fHighFallNMDelay(0.0f)
,m_fIntroBlendTime(0.0f)
,m_bLandComplete(false)
,m_bMovementTaskBreakout(false)
,m_bUseLeftFoot(false)
,m_bWantsToEnterVehicleOrCover(false)
,m_bAllowLocalCloneLand(false)
,m_UpperBodyClipSetID(CLIP_SET_ID_INVALID)
,m_fFallTestAngle(0.0f)
,m_fCloneWantsToLandTime(0.0f)
,m_fAirControlStickXNormalised(0.0f)
,m_fAirControlStickYNormalised(0.0f)
,m_bDeferHighFall(false)
,m_bDisableTagSync(false)
,m_nDistanceFallenIndex(0)
,m_bCanUseDistanceSamples(false)
,m_pedChangingOwnership(false)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL);

	for(int i = 0; i < s_nNumDistSamples; ++i)
	{
		m_aDistanceFallen[i].fZHeight=999.0f; //init to something large, so that we aren't 'stuck' by default.
		m_aDistanceFallen[i].nTime = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::~CTaskFall()
{
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFall::ProcessPreFSM()
{	
	CPed *pPed = GetPed();
	if(pPed->GetIsSwimming() && (GetState() != State_Parachute) && !pPed->IsNetworkClone())
	{
		if(GetSubTask())
		{
			if(GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
			{
				return FSM_Quit;
			}
		}
		// If we don't yet have a sub-task but we're going to try forcing an NM high-fall then
		// don't allow us to abort immediately when swimming
		else if(!m_iFlags.IsFlagSet(FF_ForceHighFallNM))
		{
			return FSM_Quit;
		}
	}

	if (GetState() == State_Fall || GetState() == State_Landing)
	{
		if (pPed->IsLocalPlayer())
		{
			const CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl)
			{
				const bool bWantsToEnterVehicle = pControl->GetPedEnter().IsPressed();
				const bool bWantsToEnterCover = pControl->GetPedCover().IsPressed();

				if (bWantsToEnterVehicle || bWantsToEnterCover)
				{
					CTaskPlayerOnFoot* pPlayerTask = static_cast<CTaskPlayerOnFoot*>(pPed->GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
					if (pPlayerTask)
					{
						if (bWantsToEnterVehicle)
						{
							m_bWantsToEnterVehicleOrCover = true;
							pPlayerTask->SetLastTimeEnterVehiclePressed(fwTimer::GetTimeInMilliseconds());
							pPlayerTask->SetLastTimeEnterCoverPressed(0);
						}
						else if (bWantsToEnterCover)
						{
							m_bWantsToEnterVehicleOrCover = true;
							pPlayerTask->SetLastTimeEnterCoverPressed(fwTimer::GetTimeInMilliseconds());
							pPlayerTask->SetLastTimeEnterVehiclePressed(0);
						}
					}
				}
			}
		}
	}

	switch(GetState())
	{
	case State_Dive:	
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_IsDiving, true);
			if (!pPed->GetIsSwimming() && 
				!pPed->GetUsingRagdoll())
			{
				if (pPed->GetMotionData()->GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Diving_Idle)
					pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
			}
		}
		// fall through.
	case State_Initial:
	case State_Fall:
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
		}
		//! Fall through.
	case State_HighFall:
		{			
			bool bIsQuadruped = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped();

			if (!pPed->GetIsSwimming() &&  GetState()!=State_Dive &&
				GetTimeRunning() > m_fIntroBlendTime && 
				(pPed->GetMovePed().IsTaskNetworkFullyBlended()) && 
				!pPed->GetUsingRagdoll() && 
				FindSubTaskOfType(CTaskTypes::TASK_GET_UP) == NULL && 
				!bIsQuadruped)
			{
				if (pPed->GetMotionData()->GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Diving_Idle)
					pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);
			}

			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsFalling, true);

			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);
		}
		break;
	case State_Landing:
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsLanding, true );

			//! Force ped to process steep slope calculation.
			pPed->GetMotionData()->SetForceSteepSlopeTestThisFrame(true);
		}
		break;
	default:
		break;
	}

	switch(GetState())
	{
	case State_HighFall:
	case State_Parachute:
		break;
	default:
		break;
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);

	bool bIsQuadruped = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped();
	if(bIsQuadruped)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeHorseMbr, true);
	}

	if(m_fInitialMBR >= 0.0f)
	{
		if(pPed->GetMotionData())
		{
			// when getting up after a fall, we need the desired move blend ratio to tell
			// us whether to early out of our getup animations.
			if (!(GetState()==State_HighFall))
			{
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_fInitialMBR);
			}
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(m_fInitialMBR);
		}
	}

#if __BANK
	TUNE_GROUP_BOOL(PED_FALLING, bShowFallingPedDebug, false);
	if(bShowFallingPedDebug)
	{
		Vector3 vWorldPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		char debugText[20];

		sprintf(debugText, "%p", pPed);

		Color32 colour = CRGBA(255,255,255,255);

		grcDebugDraw::Text( vWorldPos, colour, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}
#endif

	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);

	//! Necessary to do proper IK fixup with ground.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPedStandingUpdateEachSimStep, true);

	TUNE_GROUP_FLOAT(PED_FALLING,fFallLegIKBlend,0.1f, 0.0f, 5.0f, 0.01f)
	pPed->GetIkManager().SetOverrideLegIKBlendTime(fFallLegIKBlend);

	if(m_iFlags.IsFlagSet(FF_InstantBlend))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	}

	return FSM_Continue;
}

bool CTaskFall::ProcessPostCamera()
{
	if(m_iFlags.IsFlagSet(FF_InstantBlend))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, false);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskFall::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed();

	// Make sure that when landing in first person the animation won't
	// take control over our movement
//#if FPS_MODE_SUPPORTED
//	if( GetState() == State_Landing && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
//	{
//		CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
//		pMotionTask->SetVelocityOverride( Vector3(0.0f, 0.0f, 0.0f) );
//	}
//#endif

	bool bSetVelocity = false;
	if(!m_bSetInitialVelocity && GetState()!=State_HighFall)
	{
		//set once, even in land state, so that we carry forward momentum from previous state.
		bSetVelocity = true;
	}
	else
	{
		switch(GetState())
		{
		case State_Initial:
		case State_Fall:
		case State_Dive:
			{
				bSetVelocity = true;
			}
			break;
		case State_Landing:
			bSetVelocity = !pPed->GetMovePed().IsTaskNetworkFullyBlended() && !m_bStandingLand;
			break;
		default:
			break;
		}
	}

	//! Add new height sample to determine if we are stuck.
	float fPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;
	m_aDistanceFallen[m_nDistanceFallenIndex].fZHeight = fPedZ;
	m_aDistanceFallen[m_nDistanceFallenIndex].nTime = fwTimer::GetTimeInMilliseconds();
	
	++m_nDistanceFallenIndex;
	if(m_nDistanceFallenIndex >= s_nNumDistSamples)
	{
		m_nDistanceFallenIndex = 0;
		m_bCanUseDistanceSamples = GetState()==State_Fall; //filled sample buffer - it's now ok to use.
	}

	//! Note: It'd be good to move this to a Fall Motion Task at some point.
	if(bSetVelocity)
	{
		float fPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;
		static dev_float s_fLastZThreshold = 0.025f;
		static dev_float s_fMinAvgVelocityThreshold = 0.25f;

		if(!m_iFlags.IsFlagSet(FF_HasAirControl))
		{
			//! Don't check standing until we have fully blended in task (just in case capsule is still catching an edge).
			bool bStanding = false;
			if(pPed->GetMovePed().IsTaskNetworkFullyBlended())
			{
				bStanding = pPed->GetIsStanding() && pPed->GetWasStanding();
			}

			bool bFalling = (GetState()!=State_Fall) || ((m_vInitialFallVelocity.z < 0.0f) && (m_fLastPedZ > fPedZ) && (abs(fPedZ - m_fLastPedZ) > s_fLastZThreshold)); 

			//! Get total distance fallen using previous height samples.
			if(bFalling && m_bCanUseDistanceSamples)
			{
				float fMinHeight = m_aDistanceFallen[0].fZHeight;
				float fMaxHeight = m_aDistanceFallen[0].fZHeight;
				u32 nMinTime = m_aDistanceFallen[0].nTime;
				u32 nMaxTime = m_aDistanceFallen[0].nTime;
				for(int i = 1; i < s_nNumDistSamples; i++)
				{
					if(m_aDistanceFallen[i].nTime < nMinTime)
						nMinTime = m_aDistanceFallen[i].nTime;
					if(m_aDistanceFallen[i].nTime > nMaxTime)
						nMaxTime = m_aDistanceFallen[i].nTime;

					if(m_aDistanceFallen[i].nTime < fMinHeight)
						fMinHeight = m_aDistanceFallen[i].fZHeight;
					if(m_aDistanceFallen[i].nTime > fMaxHeight)
						fMaxHeight = m_aDistanceFallen[i].fZHeight;
				}

				float fTime = (float)(nMaxTime-nMinTime) * 0.001f;
				float fSampledDistanceFallen = fMaxHeight-fMinHeight;
				float fAvgSampledVelocity = abs(fSampledDistanceFallen / fTime);

				if(fAvgSampledVelocity < s_fMinAvgVelocityThreshold)
				{
					bFalling = false;
				}
			}

			if((GetTimeRunning() > 0.0f) && (bStanding || !bFalling))
			{
				m_fStuckTimer += fTimeStep;
			}
			else
			{
				m_fStuckTimer = 0.0f;
				m_fLastPedZ = fPedZ;
			}
		}
		else
		{
			m_fStuckTimer = 0.0f;
		}

		if(m_fStuckTimer <= 0.0f || (GetTimeInState() <= m_fIntroBlendTime) )
		{
			Vector3 vDesired(m_vInitialFallVelocity.x, m_vInitialFallVelocity.y, m_vInitialFallVelocity.z);
			pPed->SetDesiredVelocityClamped(vDesired, MAX_FALL_ACCEL_LIMIT);
			m_vInitialFallVelocity = vDesired;

			if(GetState() == State_Dive)
			{
				static dev_float s_fVelocityFalloff = 0.8f;
				static dev_float s_fGravityModifier = 1.0f;
				float fXYFallOff = m_iFlags.IsFlagSet(FF_DivingOutOfVehicle) ? 0.0f : s_fVelocityFalloff;
				m_vInitialFallVelocity.x *= (1.0f-(fXYFallOff*fTimeStep));
				m_vInitialFallVelocity.y *= (1.0f-(fXYFallOff*fTimeStep));
				m_vInitialFallVelocity.z += (GRAVITY*s_fGravityModifier)*fTimeStep;
			}
			else
			{
				if(!m_iFlags.IsFlagSet(FF_HasAirControl)) //no fall off in air control.
				{
					bool bStartFallOff = fwTimer::GetTimeInMilliseconds() > (pPed->GetLastValidGroundPhysicalTime() + 1000);
					if(bStartFallOff)
					{
						static dev_float s_fVelocityFalloff = 0.5f;
						float fXYFallOff = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_JustLeftVehicleNeedsReset) ? 0.0f : s_fVelocityFalloff;
						m_vInitialFallVelocity.x *= (1.0f-(fXYFallOff*fTimeStep));
						m_vInitialFallVelocity.y *= (1.0f-(fXYFallOff*fTimeStep));	
					}
				}

				if(m_iFlags.IsFlagSet(FF_DampZVelocity) && pPed->IsPlayer() && m_vInitialFallVelocity.z > 0.0f)
				{
                    bool jumpButtonDown = pPed->IsNetworkClone() ? m_bRemoteJumpButtonDown : pPed->GetControlFromPlayer() && pPed->GetControlFromPlayer()->GetPedJump().IsDown();

					if(!jumpButtonDown)
					{
						static dev_float s_fInAirZFalloff = 2.0f;
						m_vInitialFallVelocity.z *= (1.0f-(s_fInAirZFalloff*fTimeStep));
					}
				}
	
				m_vInitialFallVelocity.z += GRAVITY*fTimeStep;
			}

			m_vInitialFallVelocity.z = Max(MAX_FALL_Z_SPEED, m_vInitialFallVelocity.z);
		
			if(m_iFlags.IsFlagSet(FF_HasAirControl))
			{
				if (pPed->IsPlayer() || m_iFlags.IsFlagSet(FF_BeastFall))
				{
					Vector3 vStickInput;

					if(pPed->IsPlayer())
					{
						if(pPed->IsNetworkClone())
						{
							vStickInput = Vector3(m_fAirControlStickXNormalised, m_fAirControlStickYNormalised, 0.0f);
						}
						else if(pPed->GetControlFromPlayer())
						{
							vStickInput = Vector3(pPed->GetControlFromPlayer()->GetPedWalkLeftRight().GetNorm(), -pPed->GetControlFromPlayer()->GetPedWalkUpDown().GetNorm(), 0.0f);

							float fCamOrient = camInterface::GetHeading();
							vStickInput.RotateZ(fCamOrient);

							m_fAirControlStickXNormalised = Clamp(vStickInput.x, -1.0f, 1.0f);
							m_fAirControlStickYNormalised = Clamp(vStickInput.y, -1.0f, 1.0f);
						}
					}
					else if (m_iFlags.IsFlagSet(FF_BeastFall))
					{
						TUNE_GROUP_FLOAT(PED_JUMPING, fAISuperJumpStickInputY, 1.0f, -1.0f, 1.0f, 0.1f);	
						vStickInput = Vector3(0.0f,fAISuperJumpStickInputY,0.0f);
					}
					    
					float fStickMagSq = vStickInput.Mag2();
					Vector3 vDesiredStickVel(VEC3_ZERO);

                    if(fStickMagSq > 0.0f)
					{
						vDesiredStickVel = (vStickInput * sm_Tunables.m_InAirMovementRate);
					}

					Vector3 vDesired = pPed->GetDesiredVelocity();

					m_vStickVelocity.ApproachStraight(vDesiredStickVel, sm_Tunables.m_InAirMovementApproachRate, fwTimer::GetTimeStep());

					vDesired += m_vStickVelocity;

					pPed->SetDesiredVelocityClamped(vDesired, MAX_FALL_ACCEL_LIMIT);
                }
			}
		}

		m_bSetInitialVelocity = true;
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFall::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed();

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
			FSM_OnExit
				return StateInitial_OnExit(pPed);
		FSM_State(State_Fall)
			FSM_OnEnter
				return StateFall_OnEnter(pPed);
			FSM_OnUpdate
				return StateFall_OnUpdate(pPed);
		FSM_State(State_Dive)
			FSM_OnEnter
				return StateDive_OnEnter(pPed);
			FSM_OnUpdate
				return StateDive_OnUpdate(pPed);
		FSM_State(State_CrashLanding)
			FSM_OnEnter
				return StateCrashLanding_OnEnter(pPed);
			FSM_OnUpdate
				return StateCrashLanding_OnUpdate(pPed);
		FSM_State(State_HighFall)
			FSM_OnEnter
				return StateHighFall_OnEnter(pPed);
			FSM_OnUpdate
				return StateHighFall_OnUpdate(pPed);
		FSM_State(State_Parachute)
			FSM_OnEnter
				return StateParachute_OnEnter(pPed);
			FSM_OnUpdate
				return StateParachute_OnUpdate(pPed);
		FSM_State(State_Landing)
			FSM_OnEnter
				return StateLanding_OnEnter(pPed);
			FSM_OnUpdate
				return StateLanding_OnUpdate(pPed);
			FSM_OnExit
				return StateLanding_OnExit(pPed);
		FSM_State(State_GetUp)
			FSM_OnEnter
				return StateGetUp_OnEnter(pPed);
			FSM_OnUpdate
				return StateGetUp_OnUpdate(pPed);

		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFall::ProcessPostFSM()
{
	CPed *pPed = GetPed();

	if(FindSubTaskOfType(CTaskTypes::TASK_GET_UP) == NULL)
	{
		if( (m_fFallDistanceRemaining > 2.0f) && m_bOffGround && GetState() != State_Landing && !IsLandingWithParachute() )
		{
			// Turn off the leg Ik
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);
		}
	}

	// Send Move signals
	UpdateMoVE();

	//Clear the 'disable skydive transition' flag -- it is only valid for the first frame.
	m_iFlags.ClearFlag(FF_DisableSkydiveTransition);

	switch(GetState())
	{
	case State_Dive:	
	case State_Initial:
	case State_Fall:
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

			if(m_iFlags.IsFlagSet(FF_HasAirControl) || pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				CTaskFall::UpdateHeading(pPed, pPed->GetControlFromPlayer(), sm_Tunables.m_InAirHeadingRate);
			}
		}
		break;
	case State_Landing:
		{
			CTaskFall::UpdateHeading(pPed, pPed->GetControlFromPlayer(), m_bStandingLand ? sm_Tunables.m_StandingLandHeadingModifier : sm_Tunables.m_LandHeadingModifier);
		}
	default:
		break;
	}

	//! Reset fall test angle back to 0.0f.
	if( (m_fFallTestAngle > 0.0f) && (sm_Tunables.m_FallTestAngleBlendOutTime > 0.0f) )
	{
		m_fFallTestAngle -= ((fwTimer::GetTimeStep() * (GetInitialFallTestAngle()/sm_Tunables.m_FallTestAngleBlendOutTime)));
		m_fFallTestAngle = Max(0.0f, m_fFallTestAngle);
	}

	return FSM_Continue;
}

bool CTaskFall::ProcessMoveSignals()
{
	CPed *pPed = GetPed();

	switch(GetState())
	{
	case(State_Landing):
		{
			if(m_networkHelper.IsNetworkActive())
			{
				if( m_networkHelper.GetBoolean( ms_bLandComplete ) )
				{
					m_bLandComplete = true;
				}

				if(m_networkHelper.GetBoolean( ms_InterruptibleId ) ||
					(m_networkHelper.GetBoolean( ms_AimInterruptId ) && 
					pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && (pPed->GetPlayerInfo()->IsHardAiming() || pPed->GetPlayerInfo()->IsFiring()))) 
				{
					m_bProcessInterrupt = true;
				}

				//! Note: The aim interrupt is the earliest we allow breakout, so just use this.
				if(m_networkHelper.GetBoolean( ms_AimInterruptId ))
				{
					m_bMovementTaskBreakout = true;
				}


				if(m_networkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelLId))
				{
					m_bUseLeftFoot = false;
				}
				else if(m_networkHelper.GetBoolean(CTaskHumanLocomotion::ms_AefFootHeelRId))
				{
					m_bUseLeftFoot = true;
				}
			}
		}
		return true;
	default:
		break;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskFall::CleanUp()
{
	CPed* pPed = GetPed();

	if( m_networkHelper.GetNetworkPlayer() )
	{
		float fBlendTime;
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			static dev_float s_fStealthTime = 0.5f;
			fBlendTime = s_fStealthTime;
		}
		else
		{
			static dev_float s_fDiveBlend = FAST_BLEND_DURATION;
			static dev_float s_fFallBlendIntoWater = 0.33f;

			if(pPed->GetIsSwimming())
			{
				fBlendTime = GetState() == State_Dive || m_iFlags.IsFlagSet(FF_ForceDive) ? s_fDiveBlend : s_fFallBlendIntoWater;
			}
			else
			{
				fBlendTime = SLOW_BLEND_DURATION;
			}
		}

#ifdef	FOOTFALL_TAG_SYNC
		bool bTagSync = !m_bStandingLand && !pPed->GetIsInWater() && !m_bDisableTagSync;
		pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, fBlendTime, bTagSync ? CMovePed::Task_TagSyncTransition : 0);
#else
		pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, fBlendTime);
#endif

		m_networkHelper.ReleaseNetworkPlayer();

		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
		{
			CTaskHumanLocomotion *pHumanLocomotion = static_cast<CTaskHumanLocomotion*>(pTask);
			pHumanLocomotion->SetUseLeftFootTransition(m_bUseLeftFoot);
		}
	}

	// Turn off in air flag (necessary, as we break out before landing).
	if (m_iFlags.IsFlagSet(FF_DontClearInAirFlagOnCleanup))
	{
		// make sure this is set, the NMHighFall task will also clear it when aborting
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, true );
	}
	else
	{	
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );
	}

	// Restore the heading change rate
	pPed->RestoreHeadingChangeRate();

	bool bStealthMode = pPed->GetMotionData()->GetUsingStealth();
	
	bool bIsQuadruped = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped();
	if(!bIsQuadruped)
	{
		//! Clones can skip landing. In vault mode, enforce run if owner says so.
		if(pPed->IsNetworkClone() && IsVaultFall() && !m_bStandingLand)
		{
			if(m_bForceRunOnExit)
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Run : CPedMotionStates::MotionState_Run); 
			}
			else if(m_bForceWalkOnExit)
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Walk : CPedMotionStates::MotionState_Walk); 
			}
		}
		
		if(m_bRestartIdle)
		{
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
			}
			else
#endif
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Idle : CPedMotionStates::MotionState_Idle, true);
			}
		}
	}

	pPed->GetIkManager().ResetOverrideLegIKBlendTime();

	// Show weapon.
	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if (pWeapon)
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
	}

	if (!m_iFlags.IsFlagSet(FF_DontAbortRagdollOnCleanup))
	{	
		// the fall task handles ragdoll, so we need to make sure
		// we don't leave it running when exiting.
		CTaskNMControl::CleanupUnhandledRagdoll(pPed);
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_SkipFPSUnHolsterTransition, true);
	}
#endif

	// Base class
	CTask::CleanUp();
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::InitFallStateData(CPed *pPed)
{
	if(!pPed->IsNetworkClone())
	{
		// Store the initial move blend ratio
		if(pPed->GetMotionData())
		{
			CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
			if(pMotionTask && (pMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION))
			{
				m_fInitialMBR = pPed->GetMotionData()->GetCurrentMbrY();
			}
			else
			{
				//! Not in normal locomotion task. I.e. vaulting. Just use desired MBR in this case.
				m_fInitialMBR = pPed->GetMotionData()->GetDesiredMbrY();
			}
		}
	}

	m_fLastPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;
	
	if(m_fLastPedZ > m_fInitialPedZ)
	{
		m_fInitialPedZ = m_fLastPedZ;
	}

	// Notify that we are in the air
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, true );

	pPed->SetUseExtractedZ(false);

	m_bMovementTaskBreakout = false;
	
	if(!pPed->IsNetworkClone())
	{  
		//Clones get these updated over network
		m_bStandingLand = false;
		m_bWalkLand = false;
		m_bLandRoll = false;
        m_bRemoteJumpButtonDown = false;
	}

	m_bProcessInterrupt = false;

	m_fFallTime = 0.0f;
	m_vMaxFallSpeed = VEC3_ZERO;

	m_fCloneWantsToLandTime = 0.0f;
	m_bAllowLocalCloneLand = false;

	m_bDeferHighFall = false;
	m_bDisableTagSync = false;

	m_nDistanceFallenIndex = 0;
	m_bCanUseDistanceSamples = false;
	m_fStuckTimer = 0.0f;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateInitial_OnEnter(CPed *pPed)
{
	//! Don't allow fall if we are just going to exit immediately unless using ragdoll since we'd then get a ragdoll with no valid task
	if( !pPed->IsNetworkClone() && m_iFlags.IsFlagSet(FF_CheckForGroundOnEntry) && CTaskJump::IsPedNearGround(pPed, GetFallLandThreshold()) && !pPed->GetUsingRagdoll() )
	{
		return FSM_Quit;
	}

	// Get the motion task data set to get our clipset for this ped type
	CPedModelInfo *pModelInfo = pPed->GetPedModelInfo();
	const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pModelInfo->GetMotionTaskDataSetHash().GetHash());
	m_clipSetId	= fwMvClipSetId(pMotionTaskDataSet->GetOnFootData()->m_FallClipSetHash.GetHash());

	if(m_iFlags.IsFlagSet(FF_BeastFall))
	{
		const fwMvClipSetId BeastJumpFallSetId("MOVE_FALL@BeastJump",0xe4476b18);
		m_clipSetId	= BeastJumpFallSetId;
	}

	m_vInitialFallVelocity = pPed->GetVelocity();

	//If we aren't moving in ped direction, disable land roll as it looks pretty unnatural.
	Vector3 vVelTemp = m_vInitialFallVelocity;
	if(!IsVaultFall() && !IsJumpFall() && vVelTemp.XYMag2() > 0.1f)
	{
		vVelTemp.z = 0.0f;
		vVelTemp.Normalize();
		if(Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), vVelTemp) < 0.0f)
		{
			m_iFlags.SetFlag(FF_DisableLandRoll);
		}
	}

	//don't inherit positive z velocities.
	if(!m_iFlags.IsFlagSet(FF_DisableNMFall) && m_vInitialFallVelocity.z > 0.0f)
		m_vInitialFallVelocity.z = 0.0f;

	CalculateFallDistance();

	if(pPed->GetMotionData()->GetOverrideFallUpperBodyClipSet() != CLIP_SET_ID_INVALID)
	{
		m_UpperBodyClipSetID = pPed->GetMotionData()->GetOverrideFallUpperBodyClipSet();
	}
	else
	{
		const CWeapon * pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if(pWeapon && pWeapon->GetWeaponInfo())
		{
			m_UpperBodyClipSetID = pWeapon->GetWeaponInfo()->GetFallUpperbodyClipSetId();
		}
	}

	if(m_UpperBodyClipSetID != CLIP_SET_ID_INVALID)
	{
		m_UpperBodyClipSetRequestHelper.Request(m_UpperBodyClipSetID);
	}

	if(m_iFlags.IsFlagSet(FF_InstantBlend))
	{
		SetHighFallNMDelay(0.1f);
	}

	//! Init test angle.
	m_fFallTestAngle = GetInitialFallTestAngle();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateInitial_OnUpdate(CPed *pPed)
{
	CalculateLocomotionExitParams(pPed);

	//! Note: if we don't have any fall anims, go to fall state anyway ( but skip move gubbins ).
	if( !m_clipSetId )
	{
		SetState(State_Fall);
		return FSM_Continue;
	}

	bool bForcingIntoState = m_iFlags.IsFlagSet(FF_ForceSkyDive) || m_iFlags.IsFlagSet(FF_ForceDive) || m_iFlags.IsFlagSet(FF_ForceHighFallNM);

	if(pPed->GetUsingRagdoll())
	{
		m_iFlags.SetFlag(FF_ForceHighFallNM);
	}

	bool bCreateMoveNetwork = !pPed->GetUsingRagdoll();

	bool bCloneLocalDisableNMFall = false;

	//! Handle going straight to parachute. Do not pass (and subsequently cross blend through Fall).
	if(pPed->IsNetworkClone())
	{
		bCloneLocalDisableNMFall = (IsRunningLocally() && m_iFlags.IsFlagSet(FF_DisableNMFall));

		if(GetStateFromNetwork() == State_Parachute)
		{
			SetState(State_Parachute);
			return FSM_Continue;
		}
		else if( (GetStateFromNetwork() == State_HighFall) && !m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback) && !bCloneLocalDisableNMFall)
		{
			SetState(State_HighFall);
			return FSM_Continue;
		}
	}
	else
	{
		if(!m_iFlags.IsFlagSet(FF_ForceDive) && 
			(m_iFlags.IsFlagSet(FF_ForceSkyDive) || (!bForcingIntoState && CanPedParachute(pPed))))
		{
			SetState(State_Parachute);
			return FSM_Continue;
		}
		else if ( m_iFlags.IsFlagSet(FF_ForceHighFallNM) && ProcessHighFall(pPed))
		{
			return FSM_Continue;
		}
	}

	if(!m_networkHelper.IsNetworkActive() && bCreateMoveNetwork)
	{
		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssertf(pSet, "Fall clip set is invalid! %s", m_clipSetId.TryGetCStr());

		fwMvNetworkDefId fallDefId = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped() ? CClipNetworkMoveInfo::ms_NetworkTaskFallHorse : CClipNetworkMoveInfo::ms_NetworkTaskFall;

		m_networkHelper.RequestNetworkDef(fallDefId);

		if (pSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(fallDefId))
		{
			m_networkHelper.CreateNetworkPlayer(pPed, fallDefId);
			m_networkHelper.SetClipSet(m_clipSetId);

			if(m_UpperBodyClipSetID != CLIP_SET_ID_INVALID && m_UpperBodyClipSetRequestHelper.Request(m_UpperBodyClipSetID))
			{
				m_networkHelper.SetClipSet(m_UpperBodyClipSetID, ms_UpperBodyClipSetId);
			}

			// Attach it to an empty precedence slot in fwMove
			Assert(pPed->GetAnimDirector());

			TUNE_GROUP_FLOAT(PED_FALLING,fIntroBlendTime,0.25f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fForceDiveBlendTime,0.25f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fForceDiveFromVehicleBlendTime,0.5f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fDiveTransitionBlendTime,0.4f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fFromSmallBlendTime,0.25f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fFromHighBlendTime,0.35f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fFromSmallHeight,1.25f, 0.00f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_FALLING,fTagSyncHeight,1.0f, 0.00f, 5.0f, 0.01f);

			if(m_iFlags.IsFlagSet(FF_InstantBlend))
			{
				m_fIntroBlendTime = 0.0f;
			}
			else if(m_iFlags.IsFlagSet(FF_ForceDive))
			{
				m_fIntroBlendTime = m_iFlags.IsFlagSet(FF_DivingOutOfVehicle) ? fForceDiveFromVehicleBlendTime : fForceDiveBlendTime;
			}
			else if(m_iFlags.IsFlagSet(FF_CheckForDiveOrRagdoll))
			{
				m_fIntroBlendTime = fDiveTransitionBlendTime;
			}
			else if(m_iFlags.IsFlagSet(FF_LongBlendOnHighFalls))
			{
				m_fIntroBlendTime = m_fMaxFallDistance < fFromSmallHeight ? fFromSmallBlendTime : fFromHighBlendTime;
			}
			else
			{
				m_fIntroBlendTime = fIntroBlendTime;
			}

			pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), m_fIntroBlendTime );
		}
	}

	if(m_networkHelper.IsNetworkActive())
	{
		if(pPed->IsNetworkClone())
		{
			//! If we can skip ahead without going though fall, do it. Otherwise, just go straight to fall.
			if( (GetStateFromNetwork() > State_Initial) && (GetStateFromNetwork() < State_Quit) )
			{
				if ( (m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback) || bCloneLocalDisableNMFall ) && GetStateFromNetwork()==State_HighFall)
				{
					SetState(State_Fall);
				}
				else
				{
					SetState(GetStateFromNetwork());
				}
			}
			else
			{
				if(m_iFlags.IsFlagSet(FF_SkipToLand))
				{
					SetState(State_Landing);
				}
				else
				{
					SetState(State_Fall);
				}
			}
		}
		else
		{
			if(m_iFlags.IsFlagSet(FF_ForceDive) || 
				(m_iFlags.IsFlagSet(FF_CheckForDiveOrRagdoll) && CTaskMotionSwimming::CanDiveFromPosition(*pPed, pPed->GetTransform().GetPosition(), false)))
			{
				SetState(State_Dive);
				return FSM_Continue;
			}
			else if((m_iFlags.IsFlagSet(FF_ForceHighFallNM) || m_iFlags.IsFlagSet(FF_CheckForDiveOrRagdoll)) && ProcessHighFall(pPed) )
			{
				return FSM_Continue;
			}
			else
			{
				if(m_iFlags.IsFlagSet(FF_SkipToLand))
				{
					SetState(State_Landing);
				}
				else
				{
					SetState(State_Fall);
				}
			}
		}
	}

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateInitial_OnExit(CPed *pPed)
{
	InitFallStateData(pPed);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateFall_OnEnter(CPed *pPed)
{
	m_bOffGround = true;

	//! Initial block test. If we hit map, defer doing high fall for a wee bit as we'll probably bang our head on geometry, which can look a little daft.
	if(pPed->IsLocalPlayer() && !IsJumpFall() && !IsVaultFall() && !m_iFlags.IsFlagSet(FF_ForceHighFallNM) )
	{
		const Vector3 vProbePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vIBStart = vProbePos;
		Vector3 vIBEnd = GetTestEndUsingAngle(pPed, vIBStart, sm_Tunables.m_DeferFallBlockTestAngle, sm_Tunables.m_DeferFallBlockTestDistance);
		WorldProbe::CShapeTestHitPoint hitPoint;
		WorldProbe::CShapeTestResults probeResults(hitPoint);
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetCapsule(vIBStart, vIBEnd, sm_Tunables.m_DeferFallBlockTestRadius);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		capsuleDesc.SetExcludeEntity(pPed);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			m_bDeferHighFall = true;
		}

#if __DEV
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vIBStart), VECTOR3_TO_VEC3V(vIBEnd), sm_Tunables.m_DeferFallBlockTestRadius, m_bDeferHighFall ? Color_red : Color_blue, 2500, 0, false);	
#endif
	}

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateFall_OnUpdate(CPed *pPed)
{
	CalculateLocomotionExitParams(pPed);

	if(pPed->IsNetworkClone())
	{
		//! Go straight to landing if owner has cancelled task. Note: This might look funny in places, but it'll be look
		//! smoother in most cases I'd imagine.
		if( m_bCloneVaultForceLand || 
			(!IsRunningLocally() && GetStateFromNetwork() == -1) ||
			( IsRunningLocally() && CTaskJump::IsPedNearGround(pPed, GetFallLandThreshold()) ))
		{
			m_bAllowLocalCloneLand = true;
			SetState(State_Landing);
		}

		//! If clone has been near ground for x secs, then force a land. It's going to look odd if we don't get a response from 
		//! the host otherwise.
		if(CTaskJump::IsPedNearGround(pPed, GetFallLandThreshold()))
		{
			m_fCloneWantsToLandTime+=fwTimer::GetTimeStep();

			static dev_float s_fCloneLandTime = 0.0f;
			if(m_fCloneWantsToLandTime > s_fCloneLandTime)
			{
				m_bAllowLocalCloneLand = true;
				SetState(State_Landing);
			}
		}
		else
		{
			m_fCloneWantsToLandTime = 0.0f;
		}

		// If we're running an animated fallback, but the network wants us in high fall
		// keep trying to get a ragdoll
		if (m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback) 
			&& GetStateFromNetwork()==State_HighFall 
			&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NETWORK)
			)
		{
			m_iFlags.ClearFlag(FF_CloneUsingAnimatedFallback);
			SetState(State_HighFall);
			return FSM_Continue;
		}
	}
	else
	{
		TUNE_GROUP_FLOAT(PED_FALLING,fFallDistanceForShockingEvent, 3.0f, 0.00f, 15.0f, 0.01f);	
		//! Keep checking for parachute.
		if(CanPedParachute(pPed))
		{
			SetState(State_Parachute);
			return FSM_Continue;
		}
		else if (m_fMaxFallDistance > fFallDistanceForShockingEvent)
		{
			// Add an event for parachuting - a bit odd perhaps, but it is a good reaction for being in the air.
			CEventShockingParachuterOverhead ev(*pPed);
			// Make the range shorter than usual as its a bit less extreme.
			ev.SetAudioReactionRangeOverride(10.0f);
			ev.SetVisualReactionRangeOverride(10.0f);
			CShockingEventsManager::Add(ev);
		}

		TUNE_GROUP_FLOAT(PED_FALLING,fStuckInFallOnGroundLandTime,0.25f, 0.00f, 5.0f, 0.01f);	
		bool bStuckOnGround = (m_fStuckTimer > fStuckInFallOnGroundLandTime) &&
			(pPed->IsOnGround() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_MoverConstrictedByOpposingCollisions));

		if( CTaskJump::IsPedNearGround(pPed, GetFallLandThreshold()) || bStuckOnGround)		// Check if we're near the ground
		{
			if(bStuckOnGround)
			{
				m_iFlags.SetFlag(FF_PedIsStuck);
			}

			// Spawn the land task.
			SetState(State_Landing);
			return FSM_Continue;
		}
		else if(ProcessHighFall(pPed))
		{
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateDive_OnEnter(CPed *pPed)
{
	if(!pPed->IsNetworkClone())
	{
		// Store the initial move blend ratio
		if(pPed->GetMotionData())
		{
			CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
			if(pMotionTask && (pMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION))
			{
				m_fInitialMBR = pPed->GetMotionData()->GetCurrentMbrY();
			}
			else
			{
				//! Not in normal locomotion task. I.e. vaulting. Just use desired MBR in this case.
				m_fInitialMBR = pPed->GetMotionData()->GetDesiredMbrY();
			}
		}
	}

	//load up the dive_end animation
	fwMvClipSetId divingClipSetId;
	CTaskMotionBase* pedTask = GetPed()->GetPrimaryMotionTask();
	if (pedTask)
	{ 
		divingClipSetId = pedTask->GetDefaultDivingClipSet(); 																				
		m_SwimClipSetRequestHelper.Request(divingClipSetId);
		
		//tell pedMotion task I am diving
		if (pedTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pedTask);
			pTask->SetDivingFromFall();
		}
	}

	m_fLastPedZ = m_fLastPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;

	if(m_fLastPedZ > m_fInitialPedZ)
	{
		m_fInitialPedZ = m_fLastPedZ;
	}

	// Notify that we are in the air
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, true );

	pPed->SetUseExtractedZ(false);

	// If jumped out of vehicle, make dive a little more out of control.
	if(m_iFlags.IsFlagSet(FF_DivingOutOfVehicle))
	{
		m_fExtraDistanceForDiveControl = sm_Tunables.m_DiveControlExtraDistanceForDiveFromVehicle;
	}

	// Hide weapon.
	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if (pWeapon)
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
	}

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateDive_OnUpdate(CPed* pPed)
{
	float fPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;

	CalculateFallDistance();

	if(m_fCachedDiveWaterHeight <= s_fInvalidHeight )
	{
		m_fCachedDiveWaterHeight = fPedZ - m_fFallDistanceRemaining + sm_Tunables.m_DiveWaterOffsetToHitFullyInControlWeight;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding)) //hit ground!
	{
		SetState(State_CrashLanding);
		return FSM_Continue;
	}

	float fMaxDiveHeight = CPlayerInfo::GetDiveHeightForPed(*pPed);

    if(!pPed->IsNetworkClone())
    {
		float fDistanceFallen = (m_fInitialPedZ - fPedZ);

	    //! If we have fallen further that our dive height, then NM.
	    if( (fDistanceFallen > fMaxDiveHeight) && 
			((m_fInitialPedZ - m_fCachedDiveWaterHeight) > fMaxDiveHeight) && 
			!pPed->GetIsInWater() )
	    {
		    m_iFlags.SetFlag(FF_ForceHighFallNM);
		    if(ProcessHighFall(pPed))
		    {
			    return FSM_Continue;
		    }
	    }
    }

	if( (m_fInitialPedZ - m_fCachedDiveWaterHeight) > fMaxDiveHeight)
	{
		m_fDiveControlWeighting = 1.0f; //0.0f = in control. 1.0f. out of control.
	}
	else
	{
		float fDistanceRemainingToWater = Max(0.0f, fPedZ - m_fCachedDiveWaterHeight);

		//! Decrease extra dist - we still want to be in control from very high dives.
		m_fExtraDistanceForDiveControl -= fwTimer::GetTimeStep() * sm_Tunables.m_DiveControlExtraDistanceBlendOutSpeed;
		m_fExtraDistanceForDiveControl = Max(0.0f, m_fExtraDistanceForDiveControl);

		float fDiveControlWeighting = (fDistanceRemainingToWater + m_fExtraDistanceForDiveControl) / sm_Tunables.m_DiveControlMaxFallDistance;
		m_fDiveControlWeighting = Min(fDiveControlWeighting, 1.0f);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFall::StateCrashLanding_OnEnter(CPed* pPed)
{
	//Check if the ped is not a network clone.
	if(!pPed->IsNetworkClone())
	{
		//Create the task.
		CTask* pTask;
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
		{
			pTask =  rage_new CTaskNMControl(500, 5000, rage_new CTaskNMHighFall(500), CTaskNMControl::DO_BLEND_FROM_NM, 10.0f);
		}
		else
		{
			pTask = rage_new CTaskFallAndGetUp(CEntityBoundAI::FRONT, 1.0f);
		}

		//Start the task.
		SetNewTask(pTask);
	}

	return FSM_Continue; 
}

CTask::FSM_Return CTaskFall::StateCrashLanding_OnUpdate(CPed* pPed)
{
	if(!pPed->IsNetworkClone())
	{
		if (GetIsSubtaskFinished(CTaskTypes::TASK_NM_CONTROL))
		{
			// Let parent deal with it
			return FSM_Quit;
		}
	}
	else
		return FSM_Quit;

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateHighFall_OnEnter(CPed* pPed)
{
	// Send a disturbance event
	CEventDisturbance eventDisturbance(NULL, pPed, CEventDisturbance::ePedHeardFalling);
	GetEventGlobalGroup()->Add(eventDisturbance);

	if (pPed->IsNetworkClone() && !CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NETWORK))
	{
		m_iFlags.SetFlag(FF_CloneUsingAnimatedFallback);
		m_iFlags.SetFlag(FF_Plummeting);
	}
	else
	{	
		u32 uDurationNM = 1000;
		if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
		{
			uDurationNM = (u32)((float)uDurationNM * CTaskNMControl::GetCNCRagdollDurationModifier());
		}

		CTaskNMHighFall* pNMTask = rage_new CTaskNMHighFall(uDurationNM);
		pNMTask->SetEntryType(CTaskNMHighFall::HIGHFALL_IN_AIR);

		if (IsVaultFall())
		{
			pNMTask->SetEntryType(CTaskNMHighFall::HIGHFALL_VAULT);
			pNMTask->SetPitchDirectionOverride(pPed->GetVelocity());
		}

		SetNewTask(rage_new CTaskNMControl(2000, 10000, pNMTask, CTaskNMControl::DO_BLEND_FROM_NM));
	}

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateHighFall_OnUpdate(CPed* pPed)
{
	if (pPed->IsNetworkClone() && m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback))
	{
		// back to initial to reinit the move network.
		// we'll get moved on to fall from there
		SetState(State_Initial);
		return FSM_Continue;
	}

	//Check if the task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL)
	{
		m_bOffGround = false;

		// Quit after finishing NM
		return FSM_Quit;
	}
	//Check if the ped can parachute.
	else if(!m_iFlags.IsFlagSet(FF_ForceDive) && CanPedParachute(pPed))
	{
		//Parachute.
		SetState(State_Parachute);
	}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Request to update the bounds in case a ragdoll activation occurs
	pPed->RequestRagdollBoundsUpdate();
#endif

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateParachute_OnEnter(CPed* pPed)
{
	//Create the parachute task.

	s32 nParachuteFlags = 0;

	if(m_iFlags.IsFlagSet(FF_InstantBlend))
	{
		nParachuteFlags |= CTaskParachute::PF_InstantBlend;
	}
	else if(m_iFlags.IsFlagSet(FF_LongerParachuteBlend))
	{
		nParachuteFlags |= CTaskParachute::PF_UseLongBlend;
	}

	CTaskParachute* pTask = rage_new CTaskParachute(nParachuteFlags);

	if (!m_pedChangingOwnership)
	{
	//Choose the skydive transition.
	CTaskParachute::SkydiveTransition nTransition = CTaskParachute::ST_None;
	if(!m_iFlags.IsFlagSet(FF_ForceSkyDive) && !m_iFlags.IsFlagSet(FF_DisableSkydiveTransition))
	{
		if(pPed->GetUsingRagdoll())
		{
			nTransition = CTaskParachute::ST_None;
		}
		else if(m_iFlags.IsFlagSet(FF_ForceJumpFallLeft))
		{
			nTransition = CTaskParachute::ST_JumpInAirL;
		}
		else if(m_iFlags.IsFlagSet(FF_ForceJumpFallRight))
		{
			nTransition = CTaskParachute::ST_JumpInAirR;
		}
		else
		{
			nTransition = CTaskParachute::ST_FallGlide;
		}
	}

	//Clear the force skydive flag.
	m_iFlags.ClearFlag(FF_ForceSkyDive);

	//Set the skydive transition.
	pTask->SetSkydiveTransition(nTransition);
	}
	else
	{
		pTask->SetPedChangingOwnership(true);
		pTask->CacheParachuteState(m_cachedParachuteState, m_cachedParachuteObjectId);
	}
	//Start the task.
	SetNewTask(pTask);

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateParachute_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Quit the task.
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateLanding_OnEnter(CPed* pPed)
{
	//! If we go straight to land, ensure we have updated our exit params.
	if(m_iFlags.IsFlagSet(FF_SkipToLand))
	{
		CalculateLocomotionExitParams(pPed);
	}

	// Request move signals.
	RequestProcessMoveSignalCalls();

	// Notify that we are no longer in the air
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );

	if(pPed->GetPlayerInfo())
	{
		pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter( 0.0f );
	}

	bool bCanSnap = false;
	u32 legIkMode = pPed->m_PedConfigFlags.GetPedLegIkMode();
	if((legIkMode != LEG_IK_MODE_FULL) && (legIkMode != LEG_IK_MODE_FULL_MELEE))
	{
		if(IsJumpFall())
		{
			TUNE_GROUP_FLOAT(PED_FALLING,fSnapToGroundFallThreshold,0.1f, 0.00f, 5.0f, 0.01f);	
			float fPedZ = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;
			bCanSnap = ((m_fInitialPedZ - fPedZ) > fSnapToGroundFallThreshold);
		}
		else
		{
			bCanSnap = true;
		}

		if(bCanSnap)
		{
			static dev_u8 s_NumWarpZFrames = 25;
			pPed->m_PedResetFlags.SetEntityZFromGround( s_NumWarpZFrames );
		}
	}

	pPed->SetIsStanding(true);

	// Set the OnGround MoVE flag, this will allow MoVE to move into the land anim state
	m_bOffGround = false;

	/*Displayf( "[%d] m_bStandingLand = %d, m_bWalkLand = %d, IsRunningLocally() = %d, m_bAllowLocalCloneLand = %d", 
		fwTimer::GetFrameCount(),
		m_bStandingLand,
		m_bWalkLand,
		IsRunningLocally(),
		m_bAllowLocalCloneLand);*/

	// Is this needed or will the logic know he's fallen really far?
	if(m_iFlags.IsFlagSet(FF_Plummeting))
	{
		// Apply any fall damage
		if(!pPed->IsNetworkClone())
		{
			ApplyFallDamage(pPed);
		}

		pPed->ProcessFallCollision();

		// Add sound event when just landed
		CEventFootStepHeard eventFootstepNoise(pPed, CMiniMap::sm_Tunables.Sonar.fSoundRange_LandFromFall);
		GetEventGlobalGroup()->Add(eventFootstepNoise);
	}
	else
	{
		GenerateLandingNoise(*pPed);
	}

	DoLandingPadShake(pPed, m_fInitialPedZ, m_iFlags);

	float fCurrentZ = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()).z;

	bool bSlopeOk = GetPed()->IsOnGround() ? (pPed->GetGroundNormal().z > sm_Tunables.m_LandRollSlopeThreshold) : false;

	if(!m_iFlags.IsFlagSet(FF_DisableLandRoll) && 
		!pPed->GetHasJetpackEquipped() &&
		(!pPed->IsNetworkClone() || m_bAllowLocalCloneLand) && 
		!m_bStandingLand && 
		((m_fInitialPedZ - fCurrentZ) > GetLandRollHeight() ) &&
		bSlopeOk)
	{
		m_bLandRoll = true;
	}

	TUNE_GROUP_FLOAT(PED_FALLING, fVelZToClampTo, -3.0f, -100.0f, 100.0f, 0.1f);

	Vector3 vCurrentVel = pPed->GetVelocity();
	if(vCurrentVel.z <= fVelZToClampTo )
	{
		vCurrentVel.z = fVelZToClampTo;
		pPed->SetDesiredVelocity(vCurrentVel);
		pPed->SetVelocity(vCurrentVel);
		m_vInitialFallVelocity.z = vCurrentVel.z;
	}

#if __BANK
	TUNE_GROUP_BOOL(PED_JUMPING, bRenderFallLandingSpot, false);
	if(bRenderFallLandingSpot)
	{
		// Draw a green sphere at out actual landing spot
		Vector3	vLandingSpot = pPed->GetGroundPos();
		grcDebugDraw::Sphere( vLandingSpot, 0.13f, Color32(0, 255, 0, 255), true, 240);
	}
#endif	//__BANK

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateLanding_OnUpdate(CPed* pPed)
{
	//! If land rolling, allow ped to keep processing exit.
	if(m_bLandRoll)
	{
		CalculateLocomotionExitParams(pPed);
	}

	if(pPed->IsNetworkClone())
	{
		//! Allow owner to take ownership of clone land.
		if(m_bAllowLocalCloneLand && GetStateFromNetwork() == State_Landing)
		{
			m_bAllowLocalCloneLand = false;
		}
	}

	if(!m_networkHelper.IsNetworkActive())
	{
		SetState(State_Quit);
	}
	else if(m_iFlags.IsFlagSet(FF_Plummeting))
	{
		// Await the land anim's completion
		if(m_bLandComplete)
		{
			if( pPed->ShouldBeDead() == false )
			{
				SetState(State_GetUp);
			}
			else
			{
				// send a death event that will put the ped directly into the static frame.
				// the landing anim should have already posed him on the ground.
				CEventDeath deathEvent(false, false);
				deathEvent.SetUseRagdollFrame(true);

				pPed->GetPedIntelligence()->AddEvent(deathEvent);
				return FSM_Continue;
			}
		}
	}
	else
	{
		bool bCanInterruptFall;
		if(m_bLandRoll)
		{
			bCanInterruptFall = m_bMovementTaskBreakout;
		}
		else
		{
			bCanInterruptFall = true;
		}

		//! Re-enter fall state if we aren't on ground during land.
		if(!m_iFlags.IsFlagSet(FF_PedIsStuck) && (bCanInterruptFall && GetTimeInState() > 0.0f && !CTaskJump::IsPedNearGround(pPed, sm_Tunables.m_ReenterFallLandThreshold) ) )
		{
			if(!pPed->IsNetworkClone() || m_bAllowLocalCloneLand)
			{
				//! If we locally predicted fall, but now have no geometry under us, go back to fall.
				m_vInitialFallVelocity = pPed->GetVelocity();
				m_fMaxFallDistance = 0.0f;
				m_iFlags.ClearFlag(FF_UseVaultFall);
				
				InitFallStateData(pPed);
				SetState(State_Fall);
				return FSM_Continue;
			}
			else
			{
				m_bDisableTagSync = true;
				return FSM_Quit;
			}
		}

		bool bTaskFullyBlended = GetTimeRunning() > m_fIntroBlendTime;

		bool bCanInterrupt = false;
		if(GetHasInterruptedClip(pPed) && !pPed->IsNetworkClone())
		{
			if(IsJumpFall() || IsVaultFall())
			{
				bCanInterrupt = bTaskFullyBlended;
			}
			else
			{
				bCanInterrupt = true;
			}
		}

		// Await the land anim's completion
		if( m_bLandComplete || bCanInterrupt )
		{
			SetState(State_Quit);
		}
		else if(bTaskFullyBlended)
		{
			m_bProcessInterrupt = false;
		}
	}
	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateLanding_OnExit(CPed *pPed)
{
	bool bIsQuadruped = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped();
	if(!bIsQuadruped)
	{
		bool bStealthMode = pPed->GetMotionData()->GetUsingStealth();

		if(pPed->IsNetworkClone())
		{
			if(m_bStandingLand)
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Idle : CPedMotionStates::MotionState_Idle);
			}
			return FSM_Continue;
		}

		bool bRun = !m_bStandingLand && !m_bWalkLand;

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		}
#endif
		else
		{
			if( (m_bStandingLand && IsTryingToMove(pPed) && GetHasInterruptedClip(pPed) ) || m_bWalkLand)
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Walk : CPedMotionStates::MotionState_Walk);
			}
			else if(bRun)
			{
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Run : CPedMotionStates::MotionState_Run); 
			}
			else
			{
				//! Move to idle animation from a standing land, this ensures we get rid of any locomotion
				//! states underneath.
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Idle : CPedMotionStates::MotionState_Idle);

				//! Force restart idle if it's already playing (so we see intro blend).
				m_bRestartIdle = true;
			}
		}
	}

	if (pPed->GetNetworkObject() && !pPed->IsNetworkClone())
	{
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

		//Since motion state is InFrequent make sure that the remote gets set at the same time as the task state
		pPedObj->ForceResendAllData();
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateGetUp_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskGetUp* pNewTask = rage_new CTaskGetUp();
	SetNewTask(pNewTask);
	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateGetUp_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Quit);
	}
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////

CTaskFall::FSM_Return CTaskFall::StateGetUp_OnEnterClone(CPed* pPed)
{
	if (m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback))
	{
		CTaskGetUp* pNewTask = rage_new CTaskGetUp();
		pPed->SetIsStanding(false);
		SetNewTask(pNewTask);
	}

	return FSM_Continue;
}

CTaskFall::FSM_Return CTaskFall::StateGetUp_OnUpdateClone(CPed* UNUSED_PARAM(pPed))
{
	if(m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback) && GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Quit);
	}
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////

bool CTaskFall::IsTryingToMove(const CPed* pPed) const
{
	if(m_bForceRunOnExit || m_bForceWalkOnExit)
		return true;

	// Interrupt only if the player/ped is moving?
	if(pPed->IsLocalPlayer())
	{
		const CControl* pControl = pPed->GetControlFromPlayer();

		if(pControl && (pControl->GetPedWalkLeftRight().GetNorm() != 0.0f || pControl->GetPedWalkUpDown().GetNorm() != 0.0f))
		{
			return true;
		}

#if FPS_MODE_SUPPORTED
		// Interrupt in 1st person on camera movement.
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pControl && (pControl->GetPedLookLeftRight().GetNorm() != 0.0f || pControl->GetPedLookUpDown().GetNorm() != 0.0f))
		{
			return true;
		}
#endif
	}
	else
	{
		//! If a clone, just use last synced vars. Only not trying to move if we are attempting to stand land.
		if(pPed->IsNetworkClone())
		{
			return !m_bStandingLand;
		}

		Vector2 vDesMbr;
		pPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesMbr);
		if(vDesMbr.Mag2() > 0.01f)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::IsLandingWithParachute() const
{
	//Ensure the state is valid.
	if(GetState() != State_Parachute)
	{
		return false;
	}

	//Ensure the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Ensure we are landing.
	taskAssert(pSubTask->GetTaskType() == CTaskTypes::TASK_PARACHUTE);
	const CTaskParachute* pTaskParachute = static_cast<const CTaskParachute *>(pSubTask);
	if(!pTaskParachute->IsLanding())
	{
		return false;
	}

	//Ensure we are not crash landing.
	if(pTaskParachute->IsCrashLanding())
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

float CTaskFall::GetLandRollHeight() const
{
	if(IsVaultFall())
	{
		return sm_Tunables.m_LandRollHeightFromVault;
	}
	else if(IsJumpFall())
	{
		return sm_Tunables.m_LandRollHeightFromJump;
	}

	return sm_Tunables.m_LandRollHeight;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::GetHasInterruptedClip(const CPed* pPed) const
{
	if(m_bMovementTaskBreakout)
	{
		// Interrupt if forced
		if(pPed->GetPedIntelligence()->GetCurrentEventType() == EVENT_IN_AIR)
		{
			CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
			if (pEvent && static_cast<CEventInAir*>(pEvent)->GetForceInterrupt())
			{
				return true;
			}
		}

		// Check for climb interrupt if fall isn't controlled at a higher level by on foot task.
		if(pPed->IsLocalPlayer())
		{
			if(m_bWantsToEnterVehicleOrCover)
			{
				return true;
			}

			if(!IsVaultFall() && !IsJumpFall())
			{
				const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				const bool bHeavyWeapon = pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy();

				sPedTestResults VaultTestResults;
				VaultTestResults.Reset();
				if(CTaskPlayerOnFoot::CanJumpOrVault(const_cast<CPed*>(pPed), VaultTestResults, bHeavyWeapon, CTaskPlayerOnFoot::STATE_INVALID))
				{
					return true;
				}
			}

			if(pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) > 1.0f)
			{
				return true;
			}
		}

		// Break out for stealth
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			return true;
		}
	}

	if(m_bProcessInterrupt)
	{
		if(m_bStandingLand)
		{
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				return true;
			}
#endif

			// Interrupt only if the player/ped is moving?
			if(IsTryingToMove(pPed))
			{
				return true;
			}

			// players & AI following routes can interrupt
			if(pPed->GetPedResetFlag( CPED_RESET_FLAG_FollowingRoute ))
			{
				return true;
			}

			// break out on slopes.
			if(!pPed->GetSteepSlopePos().IsZero())
			{
				return true;
			}

			// Interrupt if the player is aiming a weapon
			if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && (pPed->GetPlayerInfo()->IsHardAiming() || pPed->GetPlayerInfo()->IsFiring()) )
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

// Send any signals from the task to the MoVE network.
void	CTaskFall::UpdateMoVE()
{
	if(m_networkHelper.IsNetworkActive())
	{	
		// Send the signals to MoVE
		m_networkHelper.SetFloat(ms_fMaxFallDistance, m_fMaxFallDistance);
		m_networkHelper.SetFloat(ms_fFallDistanceRemaining, m_fFallDistanceRemaining);
		m_networkHelper.SetFloat(ms_MoveSpeedId, m_fInitialMBR);
		m_networkHelper.SetFloat(ms_fDiveControl, m_fDiveControlWeighting);
		m_networkHelper.SetFlag(!m_bOffGround, ms_bOnGround);
		m_networkHelper.SetFlag(m_bStandingLand, ms_bStandingLand);
		m_networkHelper.SetFlag(m_bWalkLand, ms_bWalkLand);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(FF_Plummeting), ms_bPlummeting);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(FF_ForceJumpFallLeft), ms_bJumpFallLeft);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(FF_ForceJumpFallRight), ms_bJumpFallRight);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(FF_ForceDive) || GetState()==State_Dive, ms_bDive);
		m_networkHelper.SetFlag(m_iFlags.IsFlagSet(FF_UseVaultFall), ms_bVaultFall);
		m_networkHelper.SetFlag(m_bLandRoll, ms_bLandRoll);

		if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsMeleeFist())
		{
			crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(m_UpperBodyClipSetID);
			if (pFilter)
			{
				m_networkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::ProcessHighFall(CPed* pPed)
{
	if(m_iFlags.IsFlagSet(FF_DisableNMFall))
		return false;

	// Record the fastest falling speed
	const Vector3& vMoveSpeed = pPed->GetVelocity();
	if(vMoveSpeed.z < m_vMaxFallSpeed.z)
	{
		m_vMaxFallSpeed = vMoveSpeed;
	}

	CalculateFallDistance();

	bool bGoToHighFall;

	if(m_iFlags.IsFlagSet(FF_ForceHighFallNM))
	{
		//! Note: We assume the forcing code has done the necessary height checks!
		bGoToHighFall = true;
		m_fFallTime += GetTimeStep();
	}
	else
	{
		bGoToHighFall = ((m_fHighFallNMDelay <= 0.0f || GetTimeRunning() > m_fHighFallNMDelay) && 
			GetIsHighFall(pPed, IsJumpFall() || m_fInitialMBR >= MOVEBLENDRATIO_RUN, m_fFallTime, m_fFallTestAngle, m_fWaterDepth, GetTimeStep()));
	}

	//! If we are deferring a high fall, allow us to quit out here. In this way, we can cache the fact that we have already detected a high fall and 
	//! process it as soon as we allow.
	if(bGoToHighFall && m_bDeferHighFall)
	{
		//! Force this flag, so that we always pass GetIsHighFall from now on.
		m_iFlags.SetFlag(FF_ForceHighFallNM);

		if(m_fFallTime < sm_Tunables.m_DeferHighFallTime)
		{
			bGoToHighFall = false;
		}
	}

	if(bGoToHighFall)
	{
		bool bHasJetpack = pPed->GetHasJetpackEquipped(); 

		//! Kill fall task if we have a jetpack on. The in air event will generate the jetpack task. Note: we don't create the jetpack as a subtask as it 
		//! can generate a fall task as a child task, which would get confusing.
		if(bHasJetpack && GetParent() && (GetParent()->GetTaskType() != CTaskTypes::TASK_JETPACK))
		{
			CEventInAir eventInAir(pPed);
			pPed->GetPedIntelligence()->AddEvent(eventInAir);
			SetState(State_Quit);
			return true;
		}
		else
		{
			if( pPed->GetSpeechAudioEntity() )
			{
				// Perform VO
				audDamageStats damageStats;
				damageStats.RawDamage = 0.0f;
				damageStats.DamageReason = AUD_DAMAGE_REASON_FALLING;
				pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
			}

			TUNE_GROUP_BOOL(PED_FALLING, bForcePlummeting, false)

			// Only switch to high fall if we can use a rag-doll
			if(!bForcePlummeting && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
			{
				SetState(State_HighFall);
				return true;
			}
			else
			{
				m_iFlags.SetFlag(FF_Plummeting);
			}
		}
	}

	if(m_iFlags.IsFlagSet(FF_Plummeting) && !m_iFlags.IsFlagSet(FF_DoneScream) && pPed->GetVelocity().z <= -20.0f)
	{
		if (pPed->GetSpeechAudioEntity())
		{
			audDamageStats damageStats;
			damageStats.Fatal = false;
			damageStats.RawDamage = 20.0f;
			damageStats.DamageReason = AUD_DAMAGE_REASON_FALLING;
			damageStats.PedWasAlreadyDead = pPed->ShouldBeDead();
			pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
		}
		m_iFlags.SetFlag(FF_DoneScream);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::GetIsHighFall(CPed* pPed, bool bForceHighFallVelocity, float& fFallTime, float fFallAngle, float fWaterDepth, float fTimeStep)
{
	// Cache the ped move speed
	const Vector3& vMoveSpeed = pPed->GetVelocity();

	// Position that we will perform capsule tests from, to see if we are high enough above the ground.
	// Previously the probe would start at a position 0.5m in front of the ped, but this doesn't work if you're facing a wall
	// (it will probe on the other side of the wall and there could be a large drop there, for example).  If you want to probe in anticipation
	// of the peds motion arch I would modify the end (downward) probe position, and the start position should always be from the current player 
	// position.  But for the time being I don't know of any cases that require velocity-influenced probing. 
	const Vector3 vProbePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	fFallTime += fTimeStep;

	float fHighFallProbeLength = GetMaxPedFallHeight(pPed);

	//Check for water beneath the ped.
	float fWaterHeight = 0.0f;
	bool bFoundWater = false;
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRiversNoWaves(vProbePos, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
	{
		bFoundWater = true;
		float fHeightAboveWater = vProbePos.z-fWaterHeight;	
		if(fHeightAboveWater < fHighFallProbeLength)
		{
			return false;
		}
	}

	//! If we are falling into water, add on extra fall height to prevent ragdoll.
	if(fWaterDepth > sm_Tunables.m_HighFallWaterDepthMin)
	{
		float fWaterDepthScale = Clamp(((fWaterDepth - sm_Tunables.m_HighFallWaterDepthMin) / (sm_Tunables.m_HighFallWaterDepthMax - sm_Tunables.m_HighFallWaterDepthMin)), 0.0f, 1.0f);
		fHighFallProbeLength += rage::Lerp(fWaterDepthScale, sm_Tunables.m_HighFallExtraHeightWaterDepthMin, sm_Tunables.m_HighFallExtraHeightWaterDepthMax);
	}

	float fProbeRadius = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetProbeRadius() : 0.1f;

	Vector3 vStart = vProbePos;
	Vector3 vEnd = GetTestEndUsingAngle(pPed, vStart, fFallAngle, fHighFallProbeLength);

	bool bTestEndHitWater = bFoundWater && vEnd.z < fWaterHeight;
	
	float fHighFallVel = pPed->IsPlayer() ? sm_Tunables.m_ImmediateHighFallSpeedPlayer : sm_Tunables.m_ImmediateHighFallSpeedAi;
	float fContinuousGapHighFallTime = sm_Tunables.m_ContinuousGapHighFallTime;
	float fContinuousGapHighFallSpeed = sm_Tunables.m_ContinuousGapHighFallSpeed;

	bool bHighFall = false;

	if(bTestEndHitWater)
	{
		//! do nothing, this is ok.
	}
	else if( (vMoveSpeed.z < fHighFallVel) || bForceHighFallVelocity)
	{
#if __DEV
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), fProbeRadius, Color_blue, 2500, 0, false);	
#endif

		// Require a gap underneath player to kick in high fall NM behaviour
		WorldProbe::CShapeTestHitPoint hitPoint;
		WorldProbe::CShapeTestResults probeResults(hitPoint);
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetCapsule(vStart, vEnd, fProbeRadius);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		capsuleDesc.SetExcludeEntity(pPed);
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			bHighFall = true;
		}
	}
	else if(pPed->IsPlayer() && vMoveSpeed.z < fContinuousGapHighFallSpeed)
	{
#if __DEV
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), fProbeRadius, Color_blue, 2500, 0, false);	
#endif
		// Require a gap underneath player for a time period to kick in high fall NM behaviour
		WorldProbe::CShapeTestHitPoint hitPoint;
		WorldProbe::CShapeTestResults probeResults(hitPoint);
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetCapsule(vStart, vEnd, fProbeRadius);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		capsuleDesc.SetExcludeEntity(pPed);
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			if(fFallTime > fContinuousGapHighFallTime)
			{
				bHighFall = true;
			}
		}
		else
		{
			// Reset timer, no longer falling from massive height
			fFallTime = 0.0f;
		}
	}
	else
	{
		// Reset timer, no longer falling from massive height
		fFallTime = 0.0f;
	}

	return bHighFall;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::ApplyFallDamage(CPed* pPed)
{
	float fDamage = 0.0f;

	float fKillFallHeight = CCollisionEventScanner::KILL_FALL_HEIGHT;
	if(pPed->IsAPlayerPed())
	{
		fKillFallHeight = CCollisionEventScanner::PLAYER_KILL_FALL_HEIGHT;
	}

	float fFallHeight = pPed->GetFallingHeight() - pPed->GetTransform().GetPosition().GetZf();
	if(fFallHeight > fKillFallHeight)
	{
		fDamage = 100.0f * pPed->GetHealth();
		if(fDamage > 0.0f)
		{
			g_vfxBlood.TriggerPtFxFallToDeath(pPed);
		}
	}
	else
	{
		const float PED_PHYSICAL_DAMAGE_MULT = 4.0f;
		fDamage = fFallHeight * PED_PHYSICAL_DAMAGE_MULT;
	}

	CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_FALL);
	CPedDamageCalculator damageCalculator(NULL, fDamage, WEAPONTYPE_FALL, 0, false);
	damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::IsJumpFall() const
{
	return m_iFlags.IsFlagSet(FF_ForceJumpFallLeft) || m_iFlags.IsFlagSet(FF_ForceJumpFallRight);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::IsSkydiveFall() const
{
	return m_iFlags.IsFlagSet(FF_ForceSkyDive);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::IsVaultFall() const
{
	return m_iFlags.IsFlagSet(FF_VaultFall);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::GetIsAiming() const
{
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN)
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::GenerateLandingNoise(CPed& ped)
{
	float fNoise = CMiniMap::sm_Tunables.Sonar.fSoundRange_LandFromFall;
	fNoise = CMiniMap::CreateSonarBlipAndReportStealthNoise(&ped, ped.GetTransform().GetPosition(), fNoise, HUD_COLOUR_BLUEDARK);

	//! DMKH. Don't add if no noise.
	if(fNoise > 0.0f)
	{
		// Add sound event when just landed
		CEventFootStepHeard eventFootstepNoise(&ped, fNoise);
		GetEventGlobalGroup()->Add(eventFootstepNoise);
	}
}

float CTaskFall::GetFallLandThreshold()
{
	return sm_Tunables.m_FallLandThreshold;
}

void CTaskFall::CalculateFallDistance()
{
	CPed *pPed = GetPed();

	static dev_float s_fInitFallDist = 20.0f; 
	static dev_float s_fInitFallDistDive = 100.0f;

	float fProbeRadius = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetProbeRadius() : 0.1f;

	// Drop a probe to determine how far off the floor we are (discount any forward motion)
	// This obviously won't work very well if we are moving forward
	float fFallDistance = GetState() == State_Dive ? s_fInitFallDistDive : s_fInitFallDist;

	Vector3	probeStart, probeEnd;
	probeStart = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	probeEnd = probeStart - (ZAXIS * fFallDistance);

	WorldProbe::CShapeTestHitPoint probeIsect;
	WorldProbe::CShapeTestResults probeResults(probeIsect);
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetCapsule(probeStart, probeEnd, fProbeRadius);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	capsuleDesc.SetExcludeEntity(GetPed());
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
		// Didn't hit anything within our probe distance, a long fall
		fFallDistance = -(probeResults[0].GetHitPosition().z - probeStart.z);
	}

	m_fWaterDepth = 0.0f;

	//! Check for water beneath the ped.
	float fWaterHeight = 0.0f;
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRiversNoWaves(probeStart, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
	{
		float fHeightAboveWater = probeStart.z-fWaterHeight;	
		if(fHeightAboveWater < fFallDistance)
		{
			m_fWaterDepth = fFallDistance - fHeightAboveWater;
			fFallDistance = fHeightAboveWater;
		}
	}

	//! Only allow fall distance to grow. This allows us to swap out to high fall anims as we detect a larger fall.
	if(fFallDistance > m_fMaxFallDistance)
	{
		m_fMaxFallDistance = fFallDistance;
	}

	m_fFallDistanceRemaining = Max(fFallDistance, 0.0f);
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::UpdateHeading(CPed *pPed, const CControl *pControl, float fHeadingModifier)
{
	// No heading update if following a path or entering a vehicle.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) || pPed->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute))
	{
		return;
	}

	if(pPed->IsNetworkClone())
	{
		if(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_JUMP))
		{
			CTaskJump* task = static_cast<CTaskJump*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_JUMP));
			if(task->GetFlags()->IsFlagSet(JF_SuperJump))
			{
				pPed->SetDesiredHeading(task->GetSyncedHeading());
			}
		}
		return;
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))
	{
		TUNE_GROUP_FLOAT(PED_JUMPING, fFirstPersonFallHeadingModifier, 5.0f, 0.0f, 10.0f, 0.1f);
		fHeadingModifier = fFirstPersonFallHeadingModifier;

		if(fHeadingModifier > 0.0f)
		{
			float aimHeading = camInterface::GetPlayerControlCamHeading(*pPed);

			const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			if(pFpsCam)
			{
				aimHeading += pFpsCam->GetRelativeHeading();
			}

			aimHeading = fwAngle::LimitRadianAngle(aimHeading);
			pPed->SetDesiredHeading(aimHeading);

			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			float desiredHeading = pTask ? pTask->CalcDesiredDirection(): 0.0f;	
			static dev_float s_MaxHeadingChange = 0.5f;
			float fHeadingChange = Clamp(desiredHeading * fHeadingModifier * fwTimer::GetTimeStep(), -s_MaxHeadingChange, s_MaxHeadingChange); 
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingChange);
		}
		return;
	}
#endif

	if(fHeadingModifier > 0.0f)
	{
		static dev_float s_Epsilon = 0.01f;
		float fMag = 0.0f;
		if(pControl)
		{
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

			fMag = vStickInput.Mag();
			if(fMag > s_Epsilon)
			{
				float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);
				float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);
				fStickAngle = fStickAngle + fCamOrient;
				fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
				pPed->SetDesiredHeading(fStickAngle);
			}
		}
		else if(pPed->IsNetworkClone())
		{
			//! TO DO - sync if necessary?
			fMag = 1.0f;
		}

#if FPS_MODE_SUPPORTED
		// If we are in first person mode, switch to aiming straight away
		if( !pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
		{
#endif
			if(fMag > s_Epsilon)
			{
				float fMagScale = Min(1.0f, fMag);

				CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
				float desiredHeading = pTask ? pTask->CalcDesiredDirection(): 0.0f;	
				static dev_float s_MaxHeadingChange = 0.5f;
				float fHeadingChange = Clamp(desiredHeading * fHeadingModifier * fwTimer::GetTimeStep() * fMagScale, -s_MaxHeadingChange, s_MaxHeadingChange); 
				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingChange);
			}
#if FPS_MODE_SUPPORTED
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::CanBreakoutToMovementTask() const
{
	return m_bMovementTaskBreakout;
}

//////////////////////////////////////////////////////////////////////////

Vector3 CTaskFall::GetTestEndUsingAngle(const CPed *pPed, const Vector3 &vStart, float fAngle, float fTestLength)
{
	//Calculate the direction.
	Vector3 vDirection(-ZAXIS);
	if(fAngle != 0.0f)
	{
		//Calculate the ped direction.
		Vec3V vPedVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
		Vec3V vPedVelocityXY = vPedVelocity;
		vPedVelocityXY.SetZ(ScalarV(V_ZERO));
		Vec3V vPedForward = pPed->GetTransform().GetForward();
		Vec3V vPedForwardXY = vPedForward;
		vPedForwardXY.SetZ(ScalarV(V_ZERO));
		Vec3V vPedDirection = NormalizeFastSafe(vPedVelocityXY, vPedForwardXY);

		//Calculate the axis of rotation.
		Vec3V vUp(V_UP_AXIS_WZERO);
		Vec3V vAxisOfRotation = Cross(vPedDirection, vUp);

		//Rotate the direction about the axis.
		QuatV qRotation = QuatVFromAxisAngle(vAxisOfRotation, ScalarVFromF32(fAngle * DtoR));
		vDirection = VEC3V_TO_VECTOR3(Transform(qRotation, VECTOR3_TO_VEC3V(vDirection)));
	}

	//! Calculate the end position.
	Vector3 vEnd = vStart + (vDirection * fTestLength);
	return vEnd;
}

//////////////////////////////////////////////////////////////////////////

float CTaskFall::GetInitialFallTestAngle() const
{
	if(IsVaultFall())
	{
		return sm_Tunables.m_VaultFallTestAngle;
	}
	else if(IsJumpFall())
	{
		return sm_Tunables.m_JumpFallTestAngle;
	}
	else if (m_iFlags.IsFlagSet(FF_CheckForGroundOnEntry))
	{
		// Calculate fall test angle if we're doing a natural fall.
		float fAngle = 0.0f;
		float fMaxFallAngle = sm_Tunables.m_JumpFallTestAngle;

		// Scale angle based on peds XY velocity.
		Vector3 vVelocityXY = GetPed()->GetVelocity(); vVelocityXY.z = 0.0f;
		float fPedVelocity = vVelocityXY.Mag();
		TUNE_GROUP_FLOAT(PED_FALLING, fMaxVelocityThreshold, 4.0f, 0.0f, 7.05f, 0.01f);
		fPedVelocity = rage::Clamp(fPedVelocity, 0.0f, fMaxVelocityThreshold);

		float fActualVelocityDelta = fMaxVelocityThreshold - fPedVelocity;
		float fAngleMult = 1 - (fActualVelocityDelta / fMaxVelocityThreshold);
		fAngle = fMaxFallAngle * fAngleMult;

		return fAngle;
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::CanPedParachute(const CPed *pPed) const
{
	if (m_iFlags.IsFlagSet(FF_DisableParachuteTask))
		return false;

	return CTaskParachute::CanPedParachute(*pPed, m_fFallTestAngle);
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::DoLandingPadShake(CPed *pPed, float fInitialFallHeight, const fwFlags32 &iFlags)
{
	//! Do pad shake. 
	if(pPed->IsLocalPlayer())
	{
		float fFallHeight = fInitialFallHeight - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;

		if(fFallHeight >= sm_Tunables.m_PadShakeMinHeight)
		{
			float fScale = rage::Min(1.0f, (fFallHeight - sm_Tunables.m_PadShakeMinHeight) / (sm_Tunables.m_PadShakeMaxHeight - sm_Tunables.m_PadShakeMinHeight));
			float fIntensity = sm_Tunables.m_PadShakeMinIntensity + ((sm_Tunables.m_PadShakeMaxIntensity-sm_Tunables.m_PadShakeMinIntensity) *fScale);
			fIntensity *= iFlags.IsFlagSet(FF_BeastFall) ? sm_Tunables.m_SuperJumpIntensityMult : 1.0f;
			s32 nDuration = (s32)((float)(sm_Tunables.m_PadShakeMinDuration + ((sm_Tunables.m_PadShakeMaxDuration-sm_Tunables.m_PadShakeMinDuration) * fScale)));
			nDuration *= (s32)(iFlags.IsFlagSet(FF_BeastFall) ? sm_Tunables.m_SuperJumpDurationMult : 1.0f);
			CControlMgr::StartPlayerPadShakeByIntensity(nDuration,fIntensity);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

float CTaskFall::GetMaxPedFallHeight(const CPed *pPed)
{
	if(pPed->IsLocalPlayer())
	{
		return CPlayerInfo::GetFallHeightForPed(*pPed);
	}
	
	return sm_Tunables.m_HighFallProbeLength;
}

//////////////////////////////////////////////////////////////////////////

void CTaskFall::CalculateLocomotionExitParams(const CPed *pPed)
{
	if(!pPed->IsNetworkClone())
	{
		// Setup the signal to determine which land anim to play
		if(IsTryingToMove(pPed))
		{
			if(IsVaultFall())
			{
				m_bWalkLand = m_bForceWalkOnExit;
			}
			else
			{
				if(pPed->GetPlayerInfo())
				{
					//! Allow local player to trigger run/walk based on stick input for more control.
					bool bCanRun = pPed->IsBeingForcedToRun() || pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) >= 1.0f;
					m_bWalkLand = !bCanRun;
				}
				else
				{
					m_bWalkLand = m_fInitialMBR <= MOVEBLENDRATIO_WALK;
				}
			}

			m_bStandingLand = false;
		}
		else
		{
			m_bStandingLand = true;
			m_bWalkLand = false;
		}
	}
}

#if !__FINAL

const char * CTaskFall::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Fall",
		"HighFall",
		"Dive",
		"Parachute",
		"Landing",
		"CrashLanding",
		"GetUp",
		"Quit"
	};
	return aStateNames[iState];
}

void CTaskFall::Debug() const
{
	//Call the base class version.
	CTaskFSMClone::Debug();
}

#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////

bool CTaskFall::OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) 
{
	return false;
}

void CTaskFall::OnCloneTaskNoLongerRunningOnOwner()
{
	m_bProcessInterrupt = true;
	SetStateFromNetwork(State_Quit);
}

bool CTaskFall::IsInScope(const CPed* pPed)
{
	if((GetStateFromNetwork() == State_Parachute) || (GetState() == State_Parachute))
	{
		return true;
	}

	// while in beast jump never be out of scope
	if(pPed->IsPlayer() && pPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON))
	{
		return true;
	}

	//! If we haven't started task yet, make sure we have no ground underneath us before we do. This prevents ped falling
	//! when it appears inappropriate to do so.
	if( !IsJumpFall() && 
		!IsVaultFall() && 
		(GetStateFromNetwork()==State_Initial || GetStateFromNetwork()==State_Fall) && 
		GetState()==State_Initial )
	{
		if(CTaskJump::IsPedNearGround(pPed, GetFallLandThreshold()))
		{
			return false;
		}
	}

	return CTaskFSMClone::IsInScope(pPed);
}

CTaskInfo* CTaskFall::CreateQueriableState() const
{
	u16 flags = static_cast<u16>(m_iFlags.GetAllFlags());

    const CPed *pPed = GetPed();
    bool  bJumpButtonDown = false;

    if(pPed && pPed->GetControlFromPlayer())
    {
        bJumpButtonDown = pPed->GetControlFromPlayer()->GetPedJump().IsDown();
    }

	return rage_new CClonedFallInfo( GetState(), flags, m_fInitialMBR, m_bWalkLand, m_bStandingLand, m_bLandRoll, bJumpButtonDown, m_fAirControlStickXNormalised, m_fAirControlStickYNormalised);
}

void CTaskFall::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_FALL );
	CClonedFallInfo* info = static_cast<CClonedFallInfo*>(pTaskInfo);

	bool bCloneLocalDisableNMFall = (IsRunningLocally() && m_iFlags.IsFlagSet(FF_DisableNMFall));
	if(!bCloneLocalDisableNMFall)
	{
		m_iFlags.SetAllFlags( info->GetFlags() );
	}

	m_fInitialMBR = info->GetInitialMBR();

	bool bWalkLand		= info->GetWalkLand();
	bool bStandingLand = info->GetStandingLand();
	bool bLandRoll     = info->GetLandRoll();

	//! If we landed locally, but got land state wrong, need to bail early to correct.
	//! Note: Doesn;t work so well in practice, best to just let clone continue and fixup
	//! in blender.
	/*if(GetState() == State_Landing && m_bAllowLocalCloneLand)
	{
		if(m_bStandingLand && bStandingLand != m_bStandingLand)
		{
			m_bLandComplete = true;
		}
		if(bWalkLand != m_bWalkLand)
		{
			m_bLandComplete = true;
		}
	}*/

	m_bWalkLand = bWalkLand;
	m_bStandingLand = bStandingLand;
	m_bLandRoll = bLandRoll;
    m_bRemoteJumpButtonDown = info->GetJumpButtonDown();

    m_fAirControlStickXNormalised = info->GetAirControlStickXNormalised();
    m_fAirControlStickYNormalised = info->GetAirControlStickYNormalised();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskFall::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
	case State_HighFall:
		expectedTaskType = CTaskTypes::TASK_NM_CONTROL; 
		break;
	case State_Parachute:
		expectedTaskType = CTaskTypes::TASK_PARACHUTE;
		break;
	default:
		return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

CTaskFall::FSM_Return CTaskFall::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(iState != State_Quit)
	{
		if(iEvent == OnUpdate)
		{
			int stateFromNetwork = GetStateFromNetwork();

			bool bIgnoreNetSync = m_iFlags.IsFlagSet(FF_CloneUsingAnimatedFallback) && (stateFromNetwork==State_HighFall || stateFromNetwork==State_GetUp);

			bIgnoreNetSync |=(stateFromNetwork==State_HighFall && IsRunningLocally() && m_iFlags.IsFlagSet(FF_DisableNMFall));

			if (!bIgnoreNetSync)
			{
				// if we've been synced to a state other than high fall by the remote machine,
				// we're no longer doing the animated fallback
				m_iFlags.ClearFlag(FF_CloneUsingAnimatedFallback);

				// Stop owner resetting us back to start state if we've went past it, 
				// Stop state from being set if owner hasn't started yet
				if( ( GetState() != State_Initial ) &&
					( GetState() != State_HighFall ) && 
					( stateFromNetwork > GetState() ) && 
					( stateFromNetwork != State_Initial ) && 
					( stateFromNetwork != -1 ))
				{
					SetState(stateFromNetwork);
					return FSM_Continue;
				}

				if(GetState() == State_Landing)
				{
					switch(stateFromNetwork)
					{
						case(State_HighFall):
						case(State_Dive):
						case(State_Parachute):
							// If we landed, and owner task is important, roll back.
							SetState(stateFromNetwork);
							return FSM_Continue;
						case(State_Fall):
							// If we haven't landed locally, allow owner to put us back in fall.
							if(!m_bAllowLocalCloneLand)
							{
								SetState(State_Fall);
								return FSM_Continue;
							}
							break;
						default:
							break;
					}
				}

				// While we are in a high fall locally the remote may have quit, 
				// leaving the task info invalid, so check remote State_Quit state here.
				if(stateFromNetwork!=State_Quit)
				{
					Assert(GetState() == iState);
					UpdateClonedSubTasks(pPed, GetState());
				}
				else if(!IsRunningLocally())
				{
					if(GetState() == State_Initial)
					{
						SetState(State_Quit);
						return FSM_Continue;
					}
					else if(GetState() == State_Fall)
					{
						SetState(State_Landing);
						return FSM_Continue;
					}
				}

			}
		}
	}

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
			FSM_OnExit
				return StateInitial_OnExit(pPed);
	
		FSM_State(State_Fall)
			FSM_OnEnter
				return StateFall_OnEnter(pPed); 
			FSM_OnUpdate
				return StateFall_OnUpdate(pPed); 

		FSM_State(State_Dive)
			FSM_OnEnter
				return StateDive_OnEnter(pPed); 
			FSM_OnUpdate
				return StateDive_OnUpdate(pPed); 			
		
		FSM_State(State_CrashLanding)
			FSM_OnEnter
				return StateCrashLanding_OnEnter(pPed);
			FSM_OnUpdate
				return StateCrashLanding_OnUpdate(pPed);

		FSM_State(State_Landing)
			FSM_OnEnter
				return StateLanding_OnEnter(pPed);
			FSM_OnUpdate
				return StateLanding_OnUpdate(pPed);
			FSM_OnExit
				return StateLanding_OnExit(pPed);

		FSM_State(State_HighFall)
			FSM_OnEnter
				return StateHighFall_OnEnter(pPed);
			FSM_OnUpdate
				return StateHighFall_OnUpdate(pPed);

		FSM_State(State_GetUp)
			FSM_OnEnter
				return StateGetUp_OnEnterClone(pPed);
			FSM_OnUpdate
				return StateGetUp_OnUpdateClone(pPed);
		
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_Parachute)
			FSM_OnEnter
			return StateParachute_OnEnter(pPed);
		FSM_OnUpdate
			return StateParachute_OnUpdate(pPed);

	FSM_End
}

CTaskFSMClone*	CTaskFall::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskFall *newTask = rage_new CTaskFall(m_iFlags);

	m_iFlags.SetFlag(FF_DontClearInAirFlagOnCleanup);

	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		static_cast<CTaskNMControl*>(GetSubTask())->SetFlag(CTaskNMControl::CLONE_LOCAL_SWITCH);
	}

	// If we are parachuting and ownership changes, that means this task belongs to a parachuting ped.
	// In that case set the migrate flag so that we don't clean up the task entirely (i.e destroy the chute)
	if (GetState() == State_Parachute)
	{
		CTaskParachute *oldParachuteSubTask = static_cast<CTaskParachute*>(GetSubTask());

		if (oldParachuteSubTask)
		{
			oldParachuteSubTask->SetPedChangingOwnership(true);
		}	

		newTask->SetPedChangingOwnership(true);
		newTask->CacheParachuteState(oldParachuteSubTask->GetState(), oldParachuteSubTask->GetParachuteObjectId());
		
		newTask->SetState(GetState());
	}
	
	return newTask;
}

CTaskFSMClone*	CTaskFall::CreateTaskForLocalPed(CPed* pPed)
{
	m_iFlags.ClearFlag(FF_CloneUsingAnimatedFallback);

	CTaskFall* newTask = SafeCast(CTaskFall, CreateTaskForClonePed(pPed));
		
	return newTask;
}

//////////////////////////////////////////////////////////////////////////

CClonedFallInfo::CClonedFallInfo( s32 const _state, u32 const _flags, float const _initialMBR, bool bWalkLand, bool bStandingLand, bool bLandRoll, bool bJumpButtonDown, float fAirControlStickXNormalised, float fAirControlStickYNormalised)
: m_iFlags(_flags),
m_fInitialMBR(_initialMBR),
m_bWalkLand(bWalkLand),
m_bStandingLand(bStandingLand),
m_bLandRoll(bLandRoll),
m_bJumpButtonDown(bJumpButtonDown),
m_fAirControlStickXNormalised(fAirControlStickXNormalised),
m_fAirControlStickYNormalised(fAirControlStickYNormalised)
{
	SetStatusFromMainTaskState(_state);
}

CClonedFallInfo::CClonedFallInfo()
:
m_iFlags(0),
m_fInitialMBR(-1.0f),
m_bWalkLand(false),
m_bStandingLand(false),
m_bLandRoll(false),
m_bJumpButtonDown(false),
m_fAirControlStickXNormalised(0.0f),
m_fAirControlStickYNormalised(0.0f)
{}

CClonedFallInfo::~CClonedFallInfo()
{}

CTaskFSMClone* CClonedFallInfo::CreateCloneFSMTask()
{
	return rage_new CTaskFall( static_cast<u16>(m_iFlags) );
} 

//////////////////////////////////////////////////////////////////////////
