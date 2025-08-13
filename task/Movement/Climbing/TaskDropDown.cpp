//
// Task/TaskDropDown.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Movement/Climbing/TaskDropDown.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/PedCapsule.h"
#include "Animation/MovePed.h"
#include "Event/Event.h"
#include "Event/EventDamage.h"
#include "Task/Movement/TaskFall.h"
#include "System/ControlMgr.h"
#include "math/angmath.h"
#include "Task/Movement/Climbing/TaskVault.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

PARAM(usedropdowns, "[taskdropdown] Enable drop downs by default");

// Statics 
bank_bool CTaskDropDown::ms_bUseDropDowns = true;

const fwMvClipSetId CTaskDropDown::ms_DropDownClipSet("move_drop_down",0xD83A0ED6);
const fwMvFlagId CTaskDropDown::ms_RunDropId("RunDrop",0x579D6E21);
const fwMvFlagId CTaskDropDown::ms_LeftHandedDropId("LeftHandedDrop",0xABABE018);
const fwMvFlagId CTaskDropDown::ms_SmallDropId("SmallDrop",0x7A3A9D7F);
const fwMvFlagId CTaskDropDown::ms_LeftHandDropId("LeftHandDrop",0xDCFF230E);
const fwMvFloatId CTaskDropDown::ms_DropHeightId("DropHeight",0xEB5DD1C1);
const fwMvBooleanId CTaskDropDown::ms_EnterDropDownId("EnterDropDown",0x3AC40B36);
const fwMvBooleanId CTaskDropDown::ms_ExitDropDownId("ExitDropDown",0x46E43860);
const fwMvClipId CTaskDropDown::ms_DropDownClipId("DropDownClip",0xABD2DE4F);
const fwMvFloatId CTaskDropDown::ms_DropDownPhaseId("DropDownPhase",0xC3B01484);

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::CTaskDropDown(const CDropDown &dropDown, float fBlendInTime)
: m_DropDown(dropDown)
, m_vCloneStartPosition(VEC3_ZERO)
, m_vTargetDist(VEC3_ZERO)
, m_fInitialMBR(0.0f)
, m_fClipAdjustStartPhase(0.0f)
, m_fClipAdjustEndPhase(1.0f)
, m_fClipLastPhase(0.0f)
, m_bNetworkBlendedIn(false)
, m_eDropDownSpeed(DropDownSpeed_Stand)
, m_vHandIKPosition(VEC3_ZERO)
, m_bHandIKSet(false)
, m_bHandIKFinished(false)
, m_bCloneTaskNoLongerRunningOnOwner(false)
, m_fBlendInTime(fBlendInTime)
{
	SetInternalTaskType(CTaskTypes::TASK_DROP_DOWN);
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::~CTaskDropDown()
{
}

#if !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::Debug() const
{
	CTask::Debug();
}

////////////////////////////////////////////////////////////////////////////////

const char * CTaskDropDown::GetStaticStateName( s32 iState )
{
	static const char* stateNames[] = 
	{
		"Init",
		"DropDown",
		"Fall",
		"Quit",
	};

	return stateNames[iState];
}

////////////////////////////////////////////////////////////////////////////////

#endif // !__FINAL

bool CTaskDropDown::GetIsAttachedToPhysical() const
{
	const CPed *pPed = GetPed();

	const fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();

	// Handle if we are attached
	if(extension && extension->GetIsAttached() && extension->GetAttachParent() && (m_DropDown.m_pPhysical == extension->GetAttachParent()))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::ShowWeapon(bool show)
{
	CPed *pPed = GetPed();
	taskAssert(pPed);

	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;

	if (pWeapon)
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, show, true);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	// Scale the desired velocity to reach the target
	bool bScaledVelocity = false;
	switch(GetState())
	{
	case State_DropDown:
		bScaledVelocity = ScaleDesiredVelocity();
		break;
	default:
		break;
	}

	// Handle if we are attached
	if(GetIsAttachedToPhysical())
	{
		CPed *pPed = GetPed();

		fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();
		taskAssert(extension);

		// Get the animated velocity of the ped

		Vector3 vAttachOffset;
		if(bScaledVelocity)
		{
			vAttachOffset = pPed->GetDesiredVelocity();
			vAttachOffset = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAttachOffset)));
		}
		else
		{
			vAttachOffset = pPed->GetAnimatedVelocity();
		}

		Vec3V vPositionalDelta = VECTOR3_TO_VEC3V(vAttachOffset * fTimeStep);

		Vec3V vPosAttachOffset = VECTOR3_TO_VEC3V(extension->GetAttachOffset());
		QuatV qRotAttachOffset = QUATERNION_TO_QUATV(extension->GetAttachQuat());

		vPosAttachOffset += Transform(qRotAttachOffset, vPositionalDelta);

		// Set the attach offset
		extension->SetAttachOffset(RCC_VECTOR3(vPosAttachOffset));

		TUNE_GROUP_FLOAT(DROPDOWN,fTargetAttachSlerpSpeed,5.0f, 0.0f, 10.0f, 0.01f);

		Matrix34 other = MAT34V_TO_MATRIX34(m_DropDown.m_pPhysical->GetMatrix());

		// Current Rotation offset
		Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		m.Dot3x3Transpose(other);
		Quaternion q;
		m.ToQuaternion(q);

		float t = Min(fTargetAttachSlerpSpeed*fwTimer::GetTimeStep(), 1.0f);
		q.SlerpNear(t, m_TargetAttachOrientation);

		// Set the attach quat
		extension->SetAttachQuat(q);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::ProcessPreFSM()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Reset flags
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true );

	// State specific
	switch(GetState())
	{
	case State_Init:
    case State_DropDown:
		{
			if(GetState() == State_DropDown)
			{
				if(m_moveNetworkHelper.IsNetworkAttached() && GetTimeInState() > m_fBlendInTime)
				{
					pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
				}

				pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );

				//! Reset sprint values. If we have stick held, we force motion anyway. This just makes sure we don't immediately
				//! sprint from a stop whilst the sprint values dissipate.
				if(pPed->GetPlayerInfo())
				{
					pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
				}
			}

			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableProcessProbes, true );

			if(m_bNetworkBlendedIn && (m_fClipLastPhase > m_fClipAdjustEndPhase) )
			{
				pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
			}

			if(!m_bNetworkBlendedIn)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false);
			}

			if(GetState()==State_DropDown)
			{
				const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDelta) * Min(Abs(fHeadingDelta), pPed->GetHeadingChangeRate() * fwTimer::GetTimeStep()));
			}

			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsVaulting, true );

#if FPS_MODE_SUPPORTED
			if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
			{
				pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
			}
#endif // FPS_MODE_SUPPORTED
		}
	case State_Fall:
		break;
	default:
		break;
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableGaitReduction, true );

	// Move blender
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true );

	// Don't enlarge capsule.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::ProcessPostMovement()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		m_fClipLastPhase = Clamp(m_moveNetworkHelper.GetFloat(ms_DropDownPhaseId), 0.0f, 1.0f);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnEnter
				return StateInit_OnEnter();
			FSM_OnUpdate
				return StateInit_OnUpdate();
		FSM_State(State_DropDown)
			FSM_OnEnter
				return StateDropDown_OnEnter();
			FSM_OnUpdate
				return StateDropDown_OnUpdate();
		FSM_State(State_Fall)
			FSM_OnEnter
				return StateFall_OnEnter();
			FSM_OnUpdate
				return StateFall_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::ProcessPostFSM()
{
	// Move signals
	UpdateMoVE();

	// Update IK to attack hand to dropdown point.
	ProcessHandIK();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::CleanUp()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Enable collision
	pPed->EnableCollision();

	// Detach if we are attached
	if(GetIsAttachedToPhysical())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	CTaskVault::ResetRagdollOnCollision(pPed);

	// Enable gravity
	pPed->SetUseExtractedZ(false);

	// Blend out the move network
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);

	m_ClipSetRequestHelper.Release();

	pPed->GetMotionData()->SetGaitReductionBlockTime(); 

	// No longer dropping down
	// Restore the previous heading change rate
	pPed->RestoreHeadingChangeRate();

	ShowWeapon(true);

	CTask::CleanUp();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(priority == ABORT_PRIORITY_IMMEDIATE || pPed->IsFatallyInjured())
	{
		return CTask::ShouldAbort(priority, pEvent);
	}
	else if(pEvent)
	{
		const CEvent* pGameEvent = static_cast<const CEvent*>(pEvent);

		if(pGameEvent->RequiresAbortForRagdoll() ||
			pGameEvent->GetEventPriority() >= E_PRIORITY_SCRIPT_COMMAND_SP ||
			pGameEvent->GetEventType() == EVENT_ON_FIRE)
		{
			return CTask::ShouldAbort(priority, pEvent);
		}
		else if(pGameEvent->GetEventType() == EVENT_DAMAGE)
		{
			const CEventDamage* pDamageTask = static_cast<const CEventDamage*>(pGameEvent);
			if(pDamageTask->HasKilledPed() || pDamageTask->HasInjuredPed())
			{
				return CTask::ShouldAbort(priority, pEvent);
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CTaskDropDown::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Use Drop Down",          &ms_bUseDropDowns);
}

void CTaskDropDown::DumpToLog()
{
	taskDisplayf("--------------------------------------------------------");
	taskDisplayf("INVALID DROP DOWN DETECTED!!!");

	taskDisplayf("m_fInitialMBR = %f", m_fInitialMBR);
	taskDisplayf("m_vTargetDist = %f:%f:%f", m_vTargetDist.x, m_vTargetDist.y, m_vTargetDist.z);

	taskDisplayf("m_DropDown.m_vDropDownStartPoint = %f:%f:%f", m_DropDown.GetDropDownStartPoint().x, m_DropDown.GetDropDownStartPoint().y, m_DropDown.GetDropDownStartPoint().z);
	taskDisplayf("m_DropDown.m_vDropDownEdge = %f:%f:%f", m_DropDown.GetDropDownEdge().x, m_DropDown.GetDropDownEdge().y, m_DropDown.GetDropDownEdge().z);
	taskDisplayf("m_DropDown.m_vDropDownLand = %f:%f:%f", m_DropDown.GetDropDownLand().x, m_DropDown.GetDropDownLand().y, m_DropDown.GetDropDownLand().z);
	taskDisplayf("m_DropDown.m_vDropDownHeading = %f:%f:%f", m_DropDown.GetDropDownHeading().x, m_DropDown.GetDropDownHeading().y, m_DropDown.GetDropDownHeading().z);

	taskDisplayf("m_DropDown.m_eDropType = %d", m_DropDown.m_eDropType);

	taskDisplayf("Ped is a clone = %s", GetPed()->IsNetworkClone() ? "true" : "false");

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	taskDisplayf("Ped position = %f:%f:%f", vPedPosition.x, vPedPosition.y, vPedPosition.z);
	taskDisplayf("m_vCloneStartPosition = %f:%f:%f", m_vCloneStartPosition.x, m_vCloneStartPosition.y, m_vCloneStartPosition.z);
	
	taskDisplayf("--------------------------------------------------------");
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateInit_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Disable gravity
	pPed->SetUseExtractedZ(true);

	// Store the initial speed
	m_fInitialMBR = pPed->GetMotionData()->GetCurrentMbrY();

	if(pPed->IsNetworkClone())
	{
		TUNE_GROUP_FLOAT(DROPDOWN, fClonePedDropDownPositionTolerance, 0.5f, 0.0f, 5.0f, 0.1f);
		TUNE_GROUP_FLOAT(DROPDOWN, fClonePedDropDownOrientationTolerance, 0.0f, -1.0f, 1.0f, 0.1f);

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		taskDisplayf("CTaskDropDown::StateInit_OnEnter() - m_vCloneStartPosition = %f:%f:%f", m_vCloneStartPosition.x, m_vCloneStartPosition.y, m_vCloneStartPosition.z);
		taskDisplayf("CTaskDropDown::StateInit_OnEnter() - Ped position before warp = %f:%f:%f", vPedPosition.x, vPedPosition.y, vPedPosition.z);

		if(m_DropDown.m_pPhysical)
		{
			Vector3 vPhysicalPos = VEC3V_TO_VECTOR3(m_DropDown.m_pPhysical->GetTransform().GetPosition());
			Vector3 vPedPos		= vPhysicalPos + m_vCloneStartPosition;
			m_vCloneStartPosition = vPedPos;
		}

		//! Warp position?
		if(!pPed->GetUsingRagdoll() && 
			!m_vCloneStartPosition.IsClose(vPedPosition, fClonePedDropDownPositionTolerance) )
		{
			taskDisplayf("CTaskDropDown::StateInit_OnEnter() - Ped Warped!");
			pPed->SetPosition(m_vCloneStartPosition);
		}

		//! Warp heading?
		Vector3 vecPlayerForward(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
		Vector3 vHeadingDirection = -GetDropDown().GetDropDownHeading();
		float fNewHeading = rage::Atan2f(vHeadingDirection.x, -vHeadingDirection.y);
		fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);

		float fDot = GetDropDown().GetDropDownHeading().Dot(vecPlayerForward);
		if(fDot < fClonePedDropDownOrientationTolerance)
		{
			pPed->SetHeading(fNewHeading);
		}
	}
	else if (pPed->GetNetworkObject())
	{
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

		//Since motion state is updated via "InFrequent" make sure that the remote gets latest as the task state changes
		pPedObj->ForceResendOfAllTaskData();
		pPedObj->ForceResendAllData();

		m_vCloneStartPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}

	//! Enable heading interpolation.
	Vector3 vDirection = -GetDropDown().GetDropDownHeading();
	vDirection.z = 0.0f;
	vDirection.NormalizeFast();
	float fNewHeading = rage::Atan2f(vDirection.x, -vDirection.y);
	fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);
	pPed->SetDesiredHeading(fNewHeading);

	static dev_float HEADING_RATE = 5.0f;
	pPed->SetHeadingChangeRate(HEADING_RATE);

	if(m_DropDown.m_pPhysical)
	{
		// Position offset
		Vector3 vAttachOffset = VEC3V_TO_VECTOR3(m_DropDown.m_pPhysical->GetTransform().UnTransform(pPed->GetTransform().GetPosition()));

		Matrix34 other = MAT34V_TO_MATRIX34(m_DropDown.m_pPhysical->GetMatrix());

		// Target Rotation offset
		Matrix34 m;
		m.MakeRotateZ(fNewHeading);
		m.Dot3x3Transpose(other);
		m.ToQuaternion(m_TargetAttachOrientation);

		// Current Rotation offset
		Matrix34 mPed = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		mPed.Dot3x3Transpose(other);
		Quaternion q;
		mPed.ToQuaternion(q);

		// Attach
		u32 flags = ATTACH_STATE_BASIC | 
			ATTACH_FLAG_INITIAL_WARP | 
			ATTACH_FLAG_DONT_NETWORK_SYNC | 
			ATTACH_FLAG_COL_ON | 
			ATTACH_FLAG_AUTODETACH_ON_DEATH | 
			ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
		pPed->AttachToPhysicalBasic(m_DropDown.m_pPhysical, -1, flags, &vAttachOffset, &q);

		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
		{
			CTaskVault::SetUpRagdollOnCollision(pPed, m_DropDown.m_pPhysical);
		}

		// Test attach code by moving vehicle.
		/*static dev_float s_fTestVelMag = 20.0f;
		Vector3 vVel = VEC3V_TO_VECTOR3(m_DropDown.m_pPhysical->GetTransform().GetB())*s_fTestVelMag;
		m_DropDown.m_pPhysical->SetVelocity(vVel);*/
	}
	else
	{
		//! Turn off collision.
		pPed->DisableCollision();
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateInit_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(!m_moveNetworkHelper.IsNetworkActive())
	{
		m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskDropDown);
		m_ClipSetRequestHelper.Request(ms_DropDownClipSet);

		if(m_ClipSetRequestHelper.IsLoaded() && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskDropDown))
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskDropDown);
			m_moveNetworkHelper.SetClipSet(ms_DropDownClipSet);

			pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), m_fBlendInTime);
		}
	}

	if(m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(ms_EnterDropDownId))
	{
		SetState(State_DropDown);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateDropDown_OnEnter()
{
	// Get the ped
	CPed* pPed = GetPed();

	// Notify the ped we are off ground for a bit.
	pPed->SetIsStanding(false);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableProcessProbes, true );

	InitVelocityScaling();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateDropDown_OnUpdate()
{
	// Get the ped
	CPed* pPed = GetPed();

	if(!m_bNetworkBlendedIn)
	{
		if(m_moveNetworkHelper.IsNetworkAttached() && (GetTimeInState() > m_fBlendInTime) )
		{
			m_bNetworkBlendedIn = true;
			if(pPed->GetMotionData())
			{
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f);
				pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f);
			}

			ShowWeapon(false);
		}
	}

	// Check if we can move to fall state.
	if(m_moveNetworkHelper.GetBoolean(ms_ExitDropDownId))
	{
		SetState(State_Fall);
	}

	//! HACK. Deal with external code warping the ped without clearing the tasks :(
	if(!pPed->IsLocalPlayer())
	{
		TUNE_GROUP_FLOAT(DROPDOWN, fWarpTolerance, 10.0f, 0.0f, 50.0f, 0.1f);
		Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if(!m_DropDown.GetDropDownStartPoint().IsClose(vPedPosition, fWarpTolerance) )
		{
#if __BANK
			taskDisplayf("Ped has been warped during drop down task. Not good, so bail!");
			DumpToLog();
#endif
			SetState(State_Quit);
		}
	}

	//! Keep ragdoll on collision going if something splats it. Note: Could do with some kind of system
	//! to prevent this happening.
	if(m_DropDown.m_pPhysical && !GetPed()->GetActivateRagdollOnCollision())
	{
		CTaskVault::SetUpRagdollOnCollision(pPed, m_DropDown.m_pPhysical);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateFall_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Enable collision
	pPed->EnableCollision();

	// Detach if we are attached
	if(GetIsAttachedToPhysical())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		pPed->SetVelocity(pPed->GetDesiredVelocity());
		CTaskVault::ResetRagdollOnCollision(pPed);
	}

	// Enable gravity
	pPed->SetUseExtractedZ(false);

	if(pPed->IsNetworkClone() && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		//! Task has been created/cloned already.
	}
	else
	{
		CTaskFall* pNewTask = rage_new CTaskFall(FF_VaultFall);
		SetNewTask(pNewTask);
		pNewTask->SetHighFallNMDelay(0.1f);
		if(pPed->IsNetworkClone())
		{
			pNewTask->SetRunningLocally(true);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskDropDown::FSM_Return CTaskDropDown::StateFall_OnUpdate()
{
	// If we're falling into water then quit the task, because otherwise we'll get stuck.
	CPed *pPed = GetPed();
	taskAssert(pPed);

	//! DMKH. Hack. We don't replicate MBR quick enough when we come out of drop down, so the clone ped won't know whether to go into
	//! run or not. Just pass it the replicated running flag in this case.
	if(GetSubTask())
	{
		taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
		CTaskFall *pTaskFall = static_cast<CTaskFall*>(GetSubTask());
		pTaskFall->SetForceRunOnExit(GetDropDownSpeed() == DropDownSpeed_Run);
		pTaskFall->SetForceWalkOnExit(GetDropDownSpeed() == DropDownSpeed_Walk);
		
		if(m_bCloneTaskNoLongerRunningOnOwner && pTaskFall->GetState()==CTaskFall::State_Fall)
		{
			taskAssertf(pPed->IsNetworkClone(),"Only expect m_bCloneTaskNoLongerRunningOnOwner to be true on clones");
			pTaskFall->SetCloneVaultForceLand();
		}

		// Show weapon.
		TUNE_GROUP_FLOAT(DROPDOWN, fFallingTimeTillUnHideWeapon, 0.25f, 0.0f, 5.0f, 0.01f);
		if( (GetTimeInState() >= fFallingTimeTillUnHideWeapon) || (pTaskFall->GetState() == CTaskFall::State_Landing) )
		{
			ShowWeapon(true);
		}
	}

	if (pPed && pPed->GetIsSwimming() )
	{
		if (!GetSubTask() || GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL) )
		{
			return FSM_Quit;
		}
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Fall handles it's own land.
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::UpdateMoVE()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		m_moveNetworkHelper.SetFlag(m_DropDown.m_eDropType == eRunningDrop, ms_RunDropId);
		m_moveNetworkHelper.SetFlag(false, ms_LeftHandedDropId);
		m_moveNetworkHelper.SetFlag(m_DropDown.m_bSmallDrop, ms_SmallDropId);
		m_moveNetworkHelper.SetFlag(m_DropDown.m_bLeftHandDrop, ms_LeftHandDropId);
		m_moveNetworkHelper.SetFloat(ms_DropHeightId, m_DropDown.m_fDropHeight);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::InitVelocityScaling()
{
	CPed* pPed = GetPed();

	float fPhase = Clamp(m_moveNetworkHelper.GetFloat(ms_DropDownPhaseId), 0.0f, 1.0f);

	const crClip* pClip = m_moveNetworkHelper.GetClip(ms_DropDownClipId);
	if(taskVerifyf(pClip, "pClip is NULL"))
	{
		const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::AdjustAnimVel);
		if(pTag)
		{
			SetDesiredVelocityAdjustmentRange(fPhase, fPhase + pTag->GetEnd());
		}
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 vDropDownPoint = GetDropDown().GetDropDownStartPoint();
	vDropDownPoint.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

	m_vTargetDist   = vDropDownPoint - vPedPosition;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::ScaleDesiredVelocity()
{
	// Cache the ped
	CPed* pPed = GetPed();

	const crClip* pClip = m_moveNetworkHelper.GetClip(ms_DropDownClipId);
	if(pClip)
	{
		float fPhase = Clamp(m_moveNetworkHelper.GetFloat(ms_DropDownPhaseId), 0.0f, 1.0f);

		if(fPhase >= m_fClipAdjustStartPhase && (fPhase <= m_fClipAdjustEndPhase || m_fClipLastPhase <= m_fClipAdjustEndPhase))
		{
			// Adjust the velocity based on the current phase and the start/end phases
			float fFrameAdjust = Min(fPhase, m_fClipAdjustEndPhase) - Max(m_fClipLastPhase, m_fClipAdjustStartPhase);

			// Get the distance the animation moves over its duration
			Vector3 vAnimTotalDist(fwAnimHelpers::GetMoverTrackDisplacement(*pClip, m_fClipAdjustStartPhase, m_fClipAdjustEndPhase));
			vAnimTotalDist = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAnimTotalDist)));

			Vector3 vAnimVelocity;
			if(fPhase > m_fClipLastPhase)
			{
				vAnimVelocity = (fwAnimHelpers::GetMoverTrackDisplacement(*pClip, m_fClipLastPhase, fPhase));
				vAnimVelocity = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAnimVelocity)));
				vAnimVelocity = vAnimVelocity / fwTimer::GetTimeStep();
			}
			else
			{
				vAnimVelocity.Zero();	
			}

			// Add to the desired velocity
			Vector3 vDesiredVel = vAnimVelocity;

			Vector3 vDistanceDiff(m_vTargetDist - vAnimTotalDist);
			float fClipDuration = pClip->GetDuration();
			bool bDoXYScaling = true;
			if(fFrameAdjust > 0.0f) 
			{

				fFrameAdjust *= fClipDuration;
				fFrameAdjust /= fwTimer::GetTimeStep();

				fFrameAdjust *= 1.0f/(m_fClipAdjustEndPhase - m_fClipAdjustStartPhase);

				static dev_bool s_bDoBackwardsCheck = false;
				if(s_bDoBackwardsCheck)
				{
					Vector3 vDistDiffLocal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDistanceDiff)));
					if(vDistDiffLocal.y < 0.0f)
					{
						bDoXYScaling = false;
					}
				}

				if(bDoXYScaling)
				{
					vDesiredVel[0] += (vDistanceDiff[0] / fClipDuration) * fFrameAdjust;
					vDesiredVel[1] += (vDistanceDiff[1] / fClipDuration) * fFrameAdjust;
				}
				vDesiredVel[2] += (vDistanceDiff[2] / fClipDuration) * fFrameAdjust;
			}

#if __BANK
			if(vDesiredVel.Mag2() >= (DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT) )
			{
				DumpToLog();
				Assertf(0, "TaskDropDown generated a crazy velocity (%f:%f:%f).", vDesiredVel.x, vDesiredVel.y, vDesiredVel.z);
				Assertf(0, "vAnimVelocity (%f:%f:%f)  fFrameAdjust: %f  m_fClipLastPhase: %f  fPhase: %f", vAnimVelocity.x, vAnimVelocity.y, vAnimVelocity.z, fFrameAdjust, m_fClipLastPhase, fPhase);
				Assertf(0, "vDistanceDiff (%f:%f:%f)  fClipDuration: %f  m_fClipAdjustEndPhase: %f  m_fClipAdjustStartPhase: %f  bDoXYScaling: %s", vDistanceDiff.x, vDistanceDiff.y, vDistanceDiff.z, fClipDuration, m_fClipAdjustEndPhase, m_fClipAdjustStartPhase, bDoXYScaling ? "TRUE": "FALSE");
			}
#endif

			// Apply
			pPed->SetDesiredVelocity(vDesiredVel);

			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::SetDesiredVelocityAdjustmentRange(float fStart, float fEnd)
{
	m_fClipAdjustStartPhase = fStart;
	m_fClipAdjustEndPhase   = fEnd;

	TUNE_GROUP_FLOAT(DROPDOWN, fClipAdjustStartPhaseOverride, -1.0f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fClipAdjustEndPhaseOverride, -1.0f, -1.0f, 1.0f, 0.1f);
	if(fClipAdjustStartPhaseOverride >= 0.0f)
		m_fClipAdjustStartPhase = fClipAdjustStartPhaseOverride;
	if(fClipAdjustEndPhaseOverride >= 0.0f)
		m_fClipAdjustEndPhase = fClipAdjustEndPhaseOverride;

	// Start always has to be before end
#if AI_OPTIMISATIONS_OFF
	taskAssertf(m_fClipAdjustEndPhase >= m_fClipAdjustStartPhase, "End [%.2f] has to come after start [%.2f]", m_fClipAdjustEndPhase, m_fClipAdjustStartPhase);
	taskAssertf(m_fClipAdjustEndPhase >  m_fClipAdjustStartPhase, "Will no longer perform movement scaling");
#endif // AI_OPTIMISATIONS_OFF
}

CTaskDropDown::eDropDownSpeed CTaskDropDown::GetDropDownSpeed() const
{
	const CPed* pPed = GetPed();

	if(pPed->IsNetworkClone())
	{
		return m_eDropDownSpeed;
	}
	else
	{
		bool bTryingToMove = false;
		if(pPed->IsLocalPlayer())
		{
			const CControl* pControl = pPed->GetControlFromPlayer();
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
			float fStickMagSq = vStickInput.Mag2();
			if(fStickMagSq > 0.0f)
			{
				bTryingToMove = true;
			}
		}
		else
		{
			bTryingToMove = true;
		}

		bool bCanRun = true;
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			bCanRun = false;
		}
		else if(pPed->GetPlayerInfo())
		{
			bCanRun = pPed->IsBeingForcedToRun() || pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) >= 1.0f || (m_fInitialMBR > MOVEBLENDRATIO_RUN);
		}
		else if( (m_fInitialMBR <= MOVEBLENDRATIO_WALK)) 
		{
			bCanRun = false;
		}

		if(bTryingToMove)
		{
			if(bCanRun)
			{
				return DropDownSpeed_Run;
			}
			else
			{
				return DropDownSpeed_Walk;
			}
		}
	}

	return DropDownSpeed_Stand;
}

void CTaskDropDown::ProcessHandIK()
{
	if(!m_bHandIKSet)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DisableArmSolver, true);

	}
	if(m_DropDown.m_bLeftHandDrop)
	{
		UpdateHandIK(HandIK_Left, BONETAG_L_HAND, BONETAG_L_IK_HAND);
	}
	else
	{
		UpdateHandIK(HandIK_Right, BONETAG_R_HAND, BONETAG_R_IK_HAND);
	}
}

void CTaskDropDown::UpdateHandIK(eHandIK handIndex, int UNUSED_PARAM(nHandBone), int nHelperBone)
{
	CPed *pPed = GetPed();

	float fDropEdgeHeight = GetDropDown().GetDropDownEdge().z;

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownArmIKBlendIn, 0.2f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownArmIKBlendOut, 0.2f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(DROPDOWN, fHandIKOffset, 0.0f, 0.0f, 1.0f, 0.01f);

	static dev_float ACCEPTABLE_HAND_DRIFT_SQ	= (0.25f * 0.25f);
	static dev_float MINIMUM_HAND_IK_DISTANCE	= (0.25f);
	static dev_float HAND_PROBE_OFFSET_UP		= (0.2f);
	static dev_float HAND_PROBE_OFFSET_DOWN		= (0.2f);
	static dev_float HAND_PROBE_RADIUS			= (0.1f);

	Vector3 vHelperBonePosition(VEC3_ZERO);
	pPed->GetBonePosition(vHelperBonePosition, (eAnimBoneTag)nHelperBone);

	if(m_bHandIKSet)
	{
		if ( m_bHandIKSet && (m_vHandIKPosition - vHelperBonePosition).Mag2() > ACCEPTABLE_HAND_DRIFT_SQ)
		{
			m_bHandIKFinished = true;
		}
	}

	if(!m_bHandIKFinished)
	{
		Vector3 vecStart = vHelperBonePosition;
		Vector3 vecEnd = vHelperBonePosition;
		vecStart.z = fDropEdgeHeight + HAND_PROBE_OFFSET_UP;
		vecEnd.z = fDropEdgeHeight - HAND_PROBE_OFFSET_DOWN;

		const CEntity* ppExceptions[1] = { NULL };
		s32 iNumExceptions = 1;
		ppExceptions[0] = pPed;
		iNumExceptions = 1;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestHitPoint testIntersection;
		WorldProbe::CShapeTestResults probeResults(testIntersection);
		capsuleDesc.SetCapsule(vecStart, vecEnd, HAND_PROBE_RADIUS);
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE);
		capsuleDesc.SetExcludeEntities(ppExceptions, iNumExceptions, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
		capsuleDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

		bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

		if(bHit)
		{
			Vector3 vIntersectionPosition = VEC3V_TO_VECTOR3(testIntersection.GetPosition() );
			float fDistanceHandToIntersection = (vIntersectionPosition - vHelperBonePosition).Mag();
			if ((fDistanceHandToIntersection < MINIMUM_HAND_IK_DISTANCE) )
			{
				Vector3 vIKHand = VEC3V_TO_VECTOR3(testIntersection.GetPosition() );
				m_vHandIKPosition = vIKHand;
				m_bHandIKSet = true;
			}
		}

		if(m_bHandIKSet)
		{
			Vector3 vIKHand = m_vHandIKPosition;
			vIKHand.z += fHandIKOffset;

			pPed->GetIkManager().SetAbsoluteArmIKTarget(handIndex == HandIK_Left ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, 
				vIKHand, 
				AIK_TARGET_WRT_IKHELPER,
				fDropDownArmIKBlendIn,
				fDropDownArmIKBlendOut);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::CanBreakoutToMovementTask() const
{
	const CPed *pPed = GetPed();

	if (pPed->IsLocalPlayer())
	{
		switch(GetState())
		{
		case(State_Fall):
			{
				if(GetSubTask())
				{
					taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
					const CTaskFall *pTaskFall = static_cast<const CTaskFall*>(GetSubTask());
					return pTaskFall->CanBreakoutToMovementTask();
				}
			}
		default:
			break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bank_bool CTaskDropDown::GetUsingDropDowns()
{
	bool bDropDown = ms_bUseDropDowns
#if __BANK
		|| PARAM_usedropdowns.Get()
#endif // __BANK
		; 

	return bDropDown;
}

////////////////////////////////////////////////////////////////////////////////

// Clone task implementation for CTaskDropDown

////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskDropDown::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	Vector3 vStartPosition = VEC3_ZERO;


	pInfo =  rage_new CClonedTaskDropDownInfo(GetState(), m_DropDown, m_vCloneStartPosition, (u8)GetDropDownSpeed());

	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////
void CTaskDropDown::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_DROP_DOWN);
	CClonedTaskDropDownInfo *dropdownInfo = static_cast<CClonedTaskDropDownInfo*>(pTaskInfo);

	m_DropDown = dropdownInfo->GetDropDown();
	m_vCloneStartPosition = dropdownInfo->GetPedStartPosition();
	m_eDropDownSpeed = static_cast<eDropDownSpeed>(dropdownInfo->GetDropDownSpeed());

	if(dropdownInfo->GetPhysicalEntity())
	{
		taskAssert(dropdownInfo->GetPhysicalEntity()->GetIsPhysical());
		m_DropDown.m_pPhysical = static_cast<CPhysical*>(dropdownInfo->GetPhysicalEntity());
	}

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskDropDown::OverridesNetworkBlender(CPed* pPed)
{
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		return ((CTaskFSMClone*)GetSubTask())->OverridesNetworkBlender(pPed);
	}

	return true; 
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDropDown::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(GetStateFromNetwork() == State_Quit)
	{
		return FSM_Quit;
	}
	else if (iEvent == OnUpdate)
	{
		UpdateClonedSubTasks(pPed, GetState());
	}

	return UpdateFSM( iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::OnCloneTaskNoLongerRunningOnOwner()
{
	CPed *pPed = GetPed();

	Assert(pPed);

	m_bCloneTaskNoLongerRunningOnOwner = true;
	
	//If jump task has started on remote ped then quit
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_JUMP))
	{
		SetStateFromNetwork(State_Quit);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDropDown::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
	case State_Fall:
		expectedTaskType = CTaskTypes::TASK_FALL; 
		break;
	default:
		return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		CTask *pCloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(expectedTaskType);
		if(pCloneSubTask)
		{
			SetNewTask(pCloneSubTask);
		}
	}
}

//-------------------------------------------------------------------------
// Task info for Task Drop Down Info
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedTaskDropDownInfo::CClonedTaskDropDownInfo(s32 state, const CDropDown &dropDown, const Vector3 &vPedStartPosition, u8 iDropDownSpeed) 
: m_DropDown(dropDown)
, m_vPedStartPosition(vPedStartPosition)
, m_iDropDownSpeed(iDropDownSpeed)
{
	SetStatusFromMainTaskState(state);
	m_pPhysicalEntity.SetEntity(m_DropDown.m_pPhysical);

	if(m_pPhysicalEntity.GetEntityID()!= NETWORK_INVALID_OBJECT_ID)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(m_DropDown.m_pPhysical->GetTransform().GetPosition());
		m_vPedStartPosition = vPedStartPosition - vPos;
	}
	else
	{
		m_DropDown.ClearPhysicalAndConvertToAbsolute(); //ensure any unserialisable object gets its positions set to absolute
	}
}

CClonedTaskDropDownInfo::CClonedTaskDropDownInfo() 
: m_vPedStartPosition(VEC3_ZERO)
, m_iDropDownSpeed(0)
, m_pPhysicalEntity(NULL)
{
}


CTaskFSMClone * CClonedTaskDropDownInfo::CreateCloneFSMTask()
{
	CTaskDropDown* pTaskDropDown = NULL;

	pTaskDropDown = rage_new CTaskDropDown(m_DropDown);

	return pTaskDropDown;
}

