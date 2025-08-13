// Framework headers
#include "fwanimation/animmanager.h" 
#include "fwmaths/angle.h"
#include "fwmaths/Random.h"
#include "fwmaths/Vector.h"

//Game headers
#include "animation/AnimDefines.h"

#include "camera/CamInterface.h"
#include "Game/ModelIndices.h"
#include "Event/Events.h"
#include "Objects/Object.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Streaming/Streaming.h"
#include "Task/Animation/TaskAnims.h"
#include "Task\General\TaskBasic.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task\Default\TaskPlayer.h"
#include "Task\General\TaskSecondary.h"
#include "Task/System/TaskTypes.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "vehicles/vehicle.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()

CTaskAffectSecondaryBehaviour::CTaskAffectSecondaryBehaviour(const bool bAdd, const int iType, aiTask* pTask)
: m_bAdd(bAdd),
  m_iType(iType),
  m_pTask(pTask)
{
	SetInternalTaskType(CTaskTypes::TASK_AFFECT_SECONDARY_BEHAVIOUR);
}


CTask::FSM_Return CTaskAffectSecondaryBehaviour::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_start)
			FSM_OnUpdate
				Start_OnUpdate(pPed);

		FSM_State(State_addBehaviour)
			FSM_OnUpdate
				return AddBehaviour_OnUpdate(pPed);

		FSM_State(State_deleteBehaviour)
			FSM_OnUpdate
				return DeleteBehaviour_OnUpdate(pPed);

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskAffectSecondaryBehaviour::Start_OnUpdate(CPed* pPed)
{

	taskAssertf(pPed,"Attempting to affect secondary behaviour on an invalid ped");

	if (!pPed)
	{
		SetState(State_finish);
		return FSM_Continue;
	}

	//determine if we're trying to add or remove a secondary behaviour
	if (m_bAdd)
	{
		SetState(State_addBehaviour);
	}
	else
	{
		SetState(State_deleteBehaviour);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskAffectSecondaryBehaviour::AddBehaviour_OnUpdate(CPed* pPed)
{
	aiTask* pTask=pPed->GetPedIntelligence()->GetTaskSecondary(m_iType);
	if((0==pTask) || (pTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0)))
	{
		aiTask* pTaskClone = m_pTask->Copy();
		taskAssert( pTaskClone->GetTaskType() == m_pTask->GetTaskType() );
		pPed->GetPedIntelligence()->AddTaskSecondary(pTaskClone,m_iType);
		SetState(State_finish);
	}
	return FSM_Continue;

}

CTask::FSM_Return CTaskAffectSecondaryBehaviour::DeleteBehaviour_OnUpdate(CPed* pPed)
{
	aiTask* pTask=pPed->GetPedIntelligence()->GetTaskSecondary(m_iType);
	if(0==pTask)
	{
		SetState(State_finish);
	}
	else
	{
		//pTask->AbortAtLeisure(pPed);
		if( pTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0) )
		{
			pPed->GetPedIntelligence()->AddTaskSecondary(NULL, PED_TASK_SECONDARY_PARTIAL_ANIM);
			SetState(State_finish);
		}
	}
	return FSM_Continue;
}


CTaskAffectSecondaryBehaviour::~CTaskAffectSecondaryBehaviour()
{
	if(m_pTask) delete m_pTask;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskAffectSecondaryBehaviour::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start", 
		"State_addBehaviour",
		"State_deleteBehaviour",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

CTask* CClonedCrouchInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return rage_new CTaskCrouch((s32)m_iDuration);
}

const u16 CTaskCrouch::ms_nLengthOfCrouch=20000;

CTaskCrouch::CTaskCrouch(s32 nLengthOfCrouch, s32 nUseShotsWhizzingEvents)
{
	m_nLengthOfCrouch = nLengthOfCrouch;
	m_nShotWhizzingCounter = nUseShotsWhizzingEvents;

	SetInternalTaskType(CTaskTypes::TASK_CROUCH);
}

CTaskCrouch::~CTaskCrouch()
{
}

#define SHOTS_WHIZZING_HIDE_TIME_MIN	1000
#define SHOTS_WHIZZING_HIDE_TIME_MAX	2500

void CTaskCrouch::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent * UNUSED_PARAM(pEvent))
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	pPed->SetIsCrouching(false);
	
	// this is being used to stop aiming / firing after ShotsWhizzedBy events
	if(m_nShotWhizzingCounter > 0)
	{	
		m_nShotWhizzingCounter = MAX(0, static_cast<s16>(m_nLengthOfCrouch - fwTimer::GetTimeStepInMilliseconds()));
	}
}

CTask::FSM_Return CTaskCrouch::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_Crouching)
			FSM_OnEnter
				return StateCrouching_OnEnter(pPed);
			FSM_OnUpdate
				return StateCrouching_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskCrouch::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	m_nStartTime = fwTimer::GetTimeInMilliseconds();

	return FSM_Continue;
}
CTask::FSM_Return CTaskCrouch::StateInitial_OnUpdate(CPed * pPed)
{
	if(ShouldQuit(pPed))
	{
		pPed->SetIsCrouching(false);
		return FSM_Quit;
	}
	SetState(State_Crouching);
	return FSM_Continue;
}
CTask::FSM_Return CTaskCrouch::StateCrouching_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskCrouch::StateCrouching_OnUpdate(CPed * pPed)
{
	if(ShouldQuit(pPed))
	{
		pPed->SetIsCrouching(false);
		return FSM_Quit;
	}

	pPed->SetIsCrouching(true);

	// JB : extreme legacy code?
	// this is being used to stop aiming / firing after ShotsWhizzedBy events
	if(m_nShotWhizzingCounter > 0)
	{	
		m_nShotWhizzingCounter = MAX(0, static_cast<s16>(m_nLengthOfCrouch - fwTimer::GetTimeStepInMilliseconds()));
	}

	return FSM_Continue;
}

bool CTaskCrouch::ShouldQuit(CPed * pPed)
{
	const u32 iCurrTime = fwTimer::GetTimeInMilliseconds();

	// Sheck if crouch timer has run out
	// A zero crouch length is interpreted as infinite
	// So never finish if the task is requested to last infinitely
	if(m_nLengthOfCrouch > 0 && iCurrTime > m_nStartTime + m_nLengthOfCrouch)
	{
		return true;
	}
	if(pPed->GetHealth() < 1.0f)
	{
		return true;
	}

	return false;
}

void CTaskCrouch::SetCrouchTimer(u16 nLengthOfCrouch)
{
	m_nStartTime = fwTimer::GetTimeInMilliseconds();
	m_nLengthOfCrouch = nLengthOfCrouch;
}


//////////////////////////////////////////////////////////////////////////
// Handle putting on a helmet when getting into a vehicle
//////////////////////////////////////////////////////////////////////////

CTaskPutOnHelmet::CTaskPutOnHelmet()
{
	SetInternalTaskType(CTaskTypes::TASK_PUT_ON_HELMET);
}

CTaskPutOnHelmet::~CTaskPutOnHelmet()
{
}

//pre FSM procedure
CTask::FSM_Return CTaskPutOnHelmet::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pPed->IsLocalPlayer() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		// Is the player input interrupting this task?
		CControl* pControl     = pPed->GetControlFromPlayer();
		if(pControl && (pControl->GetPedEnter().IsPressed() || pControl->GetVehicleAccelerate().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) || pControl->GetVehicleBrake().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD)))
		{
			ProcessFinished(pPed);
			return FSM_Quit;
		}
	}
	
	return FSM_Continue;
}


//local state machine
CTask::FSM_Return CTaskPutOnHelmet::UpdateFSM( s32 iState, CTask::FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);
		FSM_State(State_pickUpHelmet)
			FSM_OnEnter
				return PickUpHelmet_OnEnter(pPed);
			FSM_OnUpdate
				return PickUpHelmet_OnUpdate(pPed);
		FSM_State(State_putOnHelmet)
			FSM_OnUpdate
				return PutOnHelmet_OnUpdate(pPed);
		FSM_State(State_waitForClip)
			FSM_OnUpdate
				return WaitForClip_OnUpdate();
		FSM_State(State_finish)
			FSM_OnUpdate
				ProcessFinished(pPed);
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskPutOnHelmet::Start_OnUpdate(CPed* pPed)
{
	taskAssert(pPed);

	if (!pPed || !pPed->GetHelmetComponent())
	{
		//ped is invalid, nothing more to do here
		SetState(State_finish);
	}

	if(pPed->GetHelmetComponent()->IsHelmetEnabled() || !pPed->GetHelmetComponent()->HasHelmetOfType())
	{
		// Already attached to peds head or we have no helmet prop - nothing to do
		SetState(State_finish);
	}
	else
	{
		//start the clip
		SetState(State_pickUpHelmet);
	}

	return FSM_Continue;
}

//get the correct clip ID and clip set ID and start the clip
CTask::FSM_Return CTaskPutOnHelmet::PickUpHelmet_OnEnter(CPed* pPed)
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;	

	// Disabled due to vehicle metadata refactor
// 	if(pPed->GetMyVehicle())
// 	{
// 		const CVehicleSeatAnimationInfo* pSeatClipInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
// 		Assert(pSeatClipInfo);
// 
// 		pSeatClipInfo->FindClip(VehicleSeatClip::HELMET_ON,clipSetId,clipId);
// 	}	

	if(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID)
	{
		// Play clip
		StartClipBySetAndClip(pPed, clipSetId, clipId, NORMAL_BLEND_IN_DELTA);
	}
	return FSM_Continue;
}

// wait for the create object flag specified in the clip, 
// then move to the put on helmet stage
CTask::FSM_Return CTaskPutOnHelmet::PickUpHelmet_OnUpdate(CPed* pPed)
{

	if(GetClipHelper())
	{
		if(!CClipEventTags::HasEvent<crPropertyAttributeBool>(GetClipHelper()->GetClip(), CClipEventTags::Object, CClipEventTags::Create, true) 
			|| GetClipHelper()->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Create, true))
		{
			pPed->GetHelmetComponent()->EnableHelmet(ANCHOR_PH_L_HAND);

			// Next state
			SetState(State_putOnHelmet);
		}
	}
	else
	{
		// No clip playing - quit
		SetState(State_finish);
	}
	return FSM_Continue;
}

// wait for the flag in the clip to release the helmet, 
// then move to the wait for clip stage 
CTask::FSM_Return CTaskPutOnHelmet::PutOnHelmet_OnUpdate(CPed* pPed)
{
	if(GetClipHelper())
	{
		pPed->GetHelmetComponent()->EnableHelmet(ANCHOR_PH_L_HAND);

		if(GetClipHelper()->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Release, true) || 
			(!CClipEventTags::HasEvent<crPropertyAttributeBool>(GetClipHelper()->GetClip(), CClipEventTags::Object, CClipEventTags::Release, true) && 
			GetClipHelper()->GetPhase() > 0.66f))
		{
			pPed->GetHelmetComponent()->EnableHelmet();

			// Next state
			SetState(State_waitForClip);
		}
	}
	else
	{
		// No clip playing - quit
		SetState(State_finish);
	}
	return FSM_Continue;
}

//wait until the clip completes before going to the finish state
CTask::FSM_Return CTaskPutOnHelmet::WaitForClip_OnUpdate()
{
	if(!GetClipHelper())
	{
		// go to the finish state
		SetState(State_finish);
	}
	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskPutOnHelmet::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start", 
		"State_waitForStream",
		"State_pickUpHelmet",
		"State_putOnHelmet",
		"State_waitForAnim",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

void CTaskPutOnHelmet::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	ProcessFinished(pPed);
}

void CTaskPutOnHelmet::ProcessFinished(CPed* pPed)
{
	Assert(pPed->GetHelmetComponent());
	if(GetClipHelper())
	{
		StopClip(NORMAL_BLEND_OUT_DELTA);
	}

	if(pPed->GetHelmetComponent()->IsHelmetEnabled())
	{
		// Ensure helmet is put on head (not hand)
		pPed->GetHelmetComponent()->EnableHelmet();
	}
}

//////////////////////////////////////////////////////////////////////////
// Handle taking off a helmet
//////////////////////////////////////////////////////////////////////////

CTaskTakeOffHelmet::CTaskTakeOffHelmet()
{
	SetInternalTaskType(CTaskTypes::TASK_TAKE_OFF_HELMET);
	m_bLoadedRestoreProp = false;
	m_bRecreateWeapon = false;
#if FPS_MODE_SUPPORTED
	m_bEnableTakeOffAnimsFPS = false;
	m_pFirstPersonIkHelper = NULL;
#endif	//FPS_MODE_SUPPORTED
	m_bEnabledHelmet = false;
	m_bEnabledHelmetFx = false;
	m_bDeferEnableHelmet = false;
	m_bVisorSwitch = false;
	m_nVisorTargetState = -1;
}

CTaskTakeOffHelmet::~CTaskTakeOffHelmet()
{
}

//pre fsm processing
CTask::FSM_Return CTaskTakeOffHelmet::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// B*3081949: The flag is needed to be true so that CPedHelmetComponent::UseOverrideInMP
	// returns true so that CPedHelmetComponent::GetHelmetPropIndex returns the cached index
	if( m_bDeferEnableHelmet )
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsRemovingHelmet, true);
	
	if (pPed->GetUsingRagdoll())
	{
		ProcessFinished(pPed);
		return FSM_Quit; 
	}

	if(pPed->IsLocalPlayer())
	{
		// If we are trying to enter a vehicle - quit task
		CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl)
		{
			//abort the task if the ped needs to aim or fire
			const bool bFireOrAimPressed = pControl->GetPedAttack().IsDown() || pPed->GetPlayerInfo()->IsAiming() || pPed->GetPlayerInfo()->IsFiring();
			if(bFireOrAimPressed)
			{
				// Don't quit out right away, wait for that frame required to restore old had properly
				if (!m_bVisorSwitch && NetworkInterface::IsGameInProgress())
				{
					pPed->GetHelmetComponent()->RestoreOldHat(true);
					if (pPed->GetHelmetComponent()->GetStoredHatPropIndex() == -1 &&
						pPed->GetHelmetComponent()->GetStoredHatTexIndex() == -1)
					{
						ProcessFinished(pPed);
						return FSM_Quit;
					}
				}
				else
				{
					ProcessFinished(pPed);
					return FSM_Quit;
				}
			}
			else
			{
				Vector3 vCarSearchDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
				bool bUsingStickInput = false;
				
				//abort the task if the ped is entering a vehicle
				if( FPS_MODE_SUPPORTED_ONLY(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) &&) 
					pControl->GetPedEnter().IsPressed() && 
					CPlayerInfo::ScanForVehicleToEnter(pPed, vCarSearchDirection, bUsingStickInput) != NULL )
				{
					// Don't quit out right away, wait for that frame required to restore old had properly
					if (!m_bVisorSwitch && NetworkInterface::IsGameInProgress())
					{
						pPed->GetHelmetComponent()->RestoreOldHat(true);
						if (pPed->GetHelmetComponent()->GetStoredHatPropIndex() == -1 &&
							pPed->GetHelmetComponent()->GetStoredHatTexIndex() == -1)
						{
							ProcessFinished(pPed);
							return FSM_Quit;
						}
					}
					else
					{
						ProcessFinished(pPed);
						return FSM_Quit;
					}
				}
			}
		}
	}
	else if (pPed->IsNetworkClone() && (pPed->GetIsEnteringVehicle() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || pPed->GetPedResetFlag(CPED_RESET_FLAG_FiringWeapon)))
	{
		ProcessFinished(pPed);
		return FSM_Quit;
	}

	TUNE_GROUP_BOOL(FIRST_PERSON_HELMET_ANIMS, bEnableTakeOffAnims, true);
	m_bEnableTakeOffAnimsFPS = bEnableTakeOffAnims;

#if FPS_MODE_SUPPORTED
	// Stay in strafing if in FPS mode.  Have to force aiming to prevent strafe task from turning to camera direction.  Fixes using animated camera when taking off helmet.
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsAiming, true);
		pPed->SetIsStrafing(true);
	}
#endif // FPS_MODE_SUPPORTED

	// Block camera changing if in FPS mode (as we're playing either the TPS or FPS helmet anim).
	if (pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

	return FSM_Continue;

}

//local state machine
CTask::FSM_Return CTaskTakeOffHelmet::UpdateFSM( s32 iState, CTask::FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
	
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);
		FSM_State(State_waitForStream)
			FSM_OnUpdate
				return WaitForStream_OnUpdate(pPed);
		FSM_State(State_waitForVisorStream)
			FSM_OnUpdate
				return WaitForVisorStream_OnUpdate(pPed);
		FSM_State(State_takeOffHelmet)
			FSM_OnEnter
				return TakeOffHelmet_OnEnter(pPed);
			FSM_OnUpdate
				return TakeOffHelmet_OnUpdate(pPed);
		FSM_State(State_switchHelmetVisor)
			FSM_OnEnter
				return SwitchHelmetVisor_OnEnter(pPed);
		FSM_OnUpdate
				return SwitchHelmetVisor_OnUpdate(pPed);
		FSM_State(State_putDownHelmet)
			FSM_OnUpdate
				return PutDownHelmet_OnUpdate(pPed);
		FSM_State(State_waitForClip)
			FSM_OnUpdate
				return WaitForClip_OnUpdate(pPed);
		FSM_State(State_finish)
			FSM_OnUpdate
				ProcessFinished(pPed);
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskTakeOffHelmet::Start_OnUpdate(CPed* pPed)
{
	taskAssertf(pPed, "Trying to remove a helmet from an invalid ped!");
	
	if (!pPed || !pPed->GetHelmetComponent())
	{
		//ped is invalid, nothing more to do here
		SetState(State_finish);
		return FSM_Continue;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
	{
		m_bVisorSwitch = true;

		if (!pPed->IsNetworkClone())
		{
			int nTargetVisorState = pPed->GetHelmetComponent()->GetIsVisorUp() ? 0 : 1; // This will be sent to the clones
			SetVisorTargetState(nTargetVisorState);
		}
	}

	// B*1902760: Hide weapons while taking off helmet.
	if (!m_bVisorSwitch && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject()!=NULL)
	{
		CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon->GetIsVisible())
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, true);
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
			m_bRecreateWeapon = true;
		}
	}

	if(pPed->GetHelmetComponent()->IsHelmetEnabled()/* && fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId)*/)
	{
		if (m_bVisorSwitch)
		{
			SetState(State_waitForVisorStream);
		}
		else
		{
			//start playing the clip
			SetState(State_waitForStream);
		}
	}
	else
	{
		// No helmet - nothing to take off
		SetState(State_finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskTakeOffHelmet::WaitForStream_OnUpdate(CPed* pPed)
{
	fwMvClipSetId clipSetId = CLIP_SET_TAKE_OFF_HELMET;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if (m_bEnableTakeOffAnimsFPS)
		{
			clipSetId = CLIP_SET_PUT_ON_TAKE_OFF_HELMET_FPS;
		}
	}
#endif	//FPS_MODE_SUPPORTED

	//If the ped is getting out or off a vehicle then wait. 
	if ( pPed->IsExitingVehicle() )
	{
		return FSM_Continue;
	}

	if(!m_bLoadedRestoreProp)
	{
		//Wait for an original head prop to be streamed in
		bool bFinishedPreLoading = pPed->GetHelmetComponent()->HasPreLoadedProp();
		if(	!pPed->GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
		{
			//Load the helmet
			if(pPed->GetHelmetComponent()->RequestPreLoadProp(true))
			{
				return FSM_Continue;
			}
			else
			{
				m_bLoadedRestoreProp = true;
			}
		}

		if(bFinishedPreLoading)
		{	
			m_bLoadedRestoreProp = true;
		}
	}

	// Wait for it to be streamed in, just in case
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet && pClipSet->Request_DEPRECATED())
	{	
		// And make sure our clip actually exist
		if (!fwAnimManager::GetClipIfExistsBySetId(clipSetId, CLIP_TAKE_HELMET_OFF))
			SetState(State_finish);
		else
		{
			if(!m_bLoadedRestoreProp)
			{
				return FSM_Continue;
			}

			SetState(State_takeOffHelmet);	
			return FSM_Continue;
		}
	}

	if (!pClipSet)
		SetState(State_finish);

	return FSM_Continue;
}

CTask::FSM_Return CTaskTakeOffHelmet::WaitForVisorStream_OnUpdate(CPed* pPed)
{
	CPed& rPed = *pPed;

	bool bIsVisorUpNow = rPed.GetHelmetComponent()->GetIsVisorUp();

	if(!m_bLoadedRestoreProp) // reuse same variable, shouldn't be able to take off helmet and switch visor at the same time anyway
	{
		bool bFinishedPreLoading = rPed.GetHelmetComponent()->HasPreLoadedProp();

		if(!rPed.GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
		{
			int nVisorVersionToPreLoad = bIsVisorUpNow ? rPed.GetHelmetComponent()->GetHelmetVisorDownPropIndex() : rPed.GetHelmetComponent()->GetHelmetVisorUpPropIndex();

			if (nVisorVersionToPreLoad == -1)
			{
				SetState(State_finish);
				return FSM_Continue;
			}

			rPed.GetHelmetComponent()->SetPropFlag(PV_FLAG_DEFAULT_HELMET);
			rPed.GetHelmetComponent()->SetHelmetPropIndex(nVisorVersionToPreLoad);
			rPed.GetHelmetComponent()->RequestPreLoadProp();
			return FSM_Continue;
		}

		if (bFinishedPreLoading)
		{
			m_bLoadedRestoreProp = true;
		}
	}

	fwMvClipSetId clipSetId = CLIP_SET_SWITCH_VISOR_ON_FOOT;
	fwMvClipId clipId = CTaskTakeOffHelmet::GetVisorClipId(rPed, bIsVisorUpNow, true);

#if FPS_MODE_SUPPORTED
	if (pPed->IsLocalPlayer() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		clipId = bIsVisorUpNow ? CLIP_SWITCH_VISOR_POV_DOWN_ON_FOOT : CLIP_SWITCH_VISOR_POV_UP_ON_FOOT;
	}
#endif	//FPS_MODE_SUPPORTED

	// Wait for it to be streamed in, just in case
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet && pClipSet->Request_DEPRECATED())
	{	
		// And make sure our clip actually exist
		if (!fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId))
			SetState(State_finish);
		else
		{
			if(!m_bLoadedRestoreProp)
			{
				return FSM_Continue;
			}

			SetState(State_switchHelmetVisor);	
			return FSM_Continue;
		}
	}

	if (!pClipSet)
		SetState(State_finish);

	return FSM_Continue;
}

//get the correct clip ID and clip set ID and start the clip
CTask::FSM_Return CTaskTakeOffHelmet::TakeOffHelmet_OnEnter(CPed* pPed)
{
	fwMvClipSetId clipSetId = CLIP_SET_TAKE_OFF_HELMET;
	fwMvClipId clipId = CLIP_TAKE_HELMET_OFF;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if (m_bEnableTakeOffAnimsFPS)
		{
			clipSetId = CLIP_SET_PUT_ON_TAKE_OFF_HELMET_FPS;
			const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
			if (!pClip)
			{
				taskAssertf(pClip, "TakeOffHelmet_OnEnter: Couldn't find clip %s from clipset %s for state %s. Falling back to third person Take_off_helmet_stand clip!", clipId.GetCStr(), clipSetId.GetCStr(), GetStaticStateName(GetState()));
				clipSetId = CLIP_SET_TAKE_OFF_HELMET;
			}

			// Hide equipped weapon to prevent clipping with helmet and IK issues (restored in cleanup)
			if (pPed->GetEquippedWeaponInfo() && !pPed->GetEquippedWeaponInfo()->GetIsUnarmed())
			{
				CObject *pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if (pWeaponObject)
				{
					pWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
				}
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

	//Clear preloaded prop data so we use the correct textrure and not the preloaded saved one.
	pPed->GetHelmetComponent()->ClearPreLoadedProp();

	// Play clip
#if __ASSERT
	const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
	taskAssertf(!pEnterVehicleTask || (pEnterVehicleTask->GetMoveNetworkHelper() && !pEnterVehicleTask->GetMoveNetworkHelper()->IsNetworkActive()), "About to replace enter vehicle move network on the task slot for ped %s, this is bad", AILogging::GetDynamicEntityNameSafe(pPed));
#endif // __ASSERT

	StartClipBySetAndClip(pPed, clipSetId, clipId, NORMAL_BLEND_IN_DELTA);
	
	return FSM_Continue;
}

// wait for the create object flag specified in the clip, 
// then move to the put on helmet stage
CTask::FSM_Return CTaskTakeOffHelmet::TakeOffHelmet_OnUpdate(CPed* pPed)
{
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsRemovingHelmet, true);

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if (!m_bEnableTakeOffAnimsFPS)
		{
			pPed->GetHelmetComponent()->DisableHelmet(false);
			SetState(State_finish);
			return FSM_Continue;
		}
	}
#endif	//FPS_MODE_SUPPORTED

	if(GetClipHelper())
	{
		if(GetClipHelper()->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Create, true) || 
			(!CClipEventTags::HasEvent<crPropertyAttributeBool>(GetClipHelper()->GetClip(), CClipEventTags::Object, CClipEventTags::Create, true) && GetClipHelper()->GetPhase() > 0.1f))
		{
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				m_bDeferEnableHelmet = true;
			}
			else
			{
				EnableHelmet();
			}

			// Next state
			SetState(State_putDownHelmet);
		}
	}
	else
	{
		// No clip playing - quit
		SetState(State_finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskTakeOffHelmet::SwitchHelmetVisor_OnEnter(CPed* pPed)
{
	bool bIsVisorUpNow = pPed->GetHelmetComponent()->GetIsVisorUp();

	fwMvClipSetId clipSetId = CLIP_SET_SWITCH_VISOR_ON_FOOT;
	fwMvClipId clipId = CTaskTakeOffHelmet::GetVisorClipId(*pPed, bIsVisorUpNow, true);
	
#if FPS_MODE_SUPPORTED
	if (pPed->IsLocalPlayer() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		clipId = bIsVisorUpNow ? CLIP_SWITCH_VISOR_POV_DOWN_ON_FOOT : CLIP_SWITCH_VISOR_POV_UP_ON_FOOT;
	}
#endif	//FPS_MODE_SUPPORTED

	//Clear preloaded prop data so we use the correct textrure and not the preloaded saved one.
	//pPed->GetHelmetComponent()->ClearPreLoadedProp();

	// Play clip
//#if __ASSERT
//	const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
//	taskAssertf(!pEnterVehicleTask || (pEnterVehicleTask->GetMoveNetworkHelper() && !pEnterVehicleTask->GetMoveNetworkHelper()->IsNetworkActive()), "About to replace enter vehicle move network on the task slot for ped %s, this is bad", AILogging::GetDynamicEntityNameSafe(pPed));
//#endif // __ASSERT

	StartClipBySetAndClip(pPed, clipSetId, clipId, NORMAL_BLEND_IN_DELTA);

	return FSM_Continue;
};

CTask::FSM_Return CTaskTakeOffHelmet::SwitchHelmetVisor_OnUpdate(CPed* pPed)
{
	CPed& rPed = *pPed;

	rPed.SetHeadIkBlocked(); //reset every frame

	if(GetClipHelper())
	{
		if (GetClipHelper()->IsEvent(CClipEventTags::MoveEvent))
		{
#if FPS_MODE_SUPPORTED
			if (rPed.IsLocalPlayer() && rPed.IsFirstPersonShooterModeEnabledForPlayer(false))
				rPed.SetPedResetFlag(CPED_RESET_FLAG_PutDownHelmetFX, true);
#endif //FPS_MODE_SUPPORTED

			rPed.GetHelmetComponent()->EnableHelmet();
			rPed.GetHelmetComponent()->ClearPreLoadedProp();
			// Reset the desired helmet flag, which was updated by EnableHelmet() internally
			rPed.GetHelmetComponent()->SetPropFlag(PV_FLAG_NONE);

			if (!pPed->IsNetworkClone())
				pPed->GetHelmetComponent()->SetIsVisorUp(!pPed->GetHelmetComponent()->GetIsVisorUp());

			SetState(State_waitForClip);
		}
	}
	else
	{
		// No clip playing - quit
		SetState(State_finish);
	}

	return FSM_Continue;
};

// wait for the flag in the clip to release the helmet, 
// then move to the wait for clip stage 
CTask::FSM_Return CTaskTakeOffHelmet::PutDownHelmet_OnUpdate(CPed* pPed)
{
	if(GetClipHelper())
	{
		//Restore the hair early for a better transition
		//Only if another prop isn't being restored
		if(GetClipHelper()->IsEvent<crPropertyAttributeBool>(CClipEventTags::MoveEvent, CClipEventTags::RestoreHair, true))
		{
			int iPropIndex = CPedPropsMgr::GetPedPropIdx(pPed, ANCHOR_HEAD);
			if((iPropIndex == -1 || CPedPropsMgr::GetPedPropActualAnchorPoint(pPed,ANCHOR_HEAD) != ANCHOR_NONE )
				&& pPed->GetHelmetComponent()->GetStoredHatPropIndex() == -1)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScaleLarger, true);
				pPed->SetTargetHairScale(0.0f);
			}
		}


		// FPS: Re-show the helmet once it's at a safe distance from the peds head to avoid clipping
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			const crClip *pClip = GetClipHelper()->GetClip();

			float fTakeOffPhase = GetClipHelper() ?  GetClipHelper()->GetPhase() : -1;

			if(!m_bEnabledHelmet)
			{
				CClipEventTags::Key DetachFPHelmet("DetachFPHelmet", 0xd4f9037e); 

				float fDetachPhase = -1.0f;
				if (pClip)
				{
					CClipEventTags::FindEventPhase(pClip, DetachFPHelmet, fDetachPhase);
				}

				if (fDetachPhase == -1.0f)
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_HELMET_ANIMS, fFirstPersonPropDetachPhase, 0.5f, 0.0f, 1.0f, 0.01f);
					fDetachPhase = fFirstPersonPropDetachPhase;
				}

				if (pPed->IsLocalPlayer() && fTakeOffPhase > fDetachPhase)
				{
					EnableHelmet();
				}
			}

			if(!m_bEnabledHelmetFx)
			{
				CClipEventTags::Key FPHelmetFX("FPHelmetFX", 0xebe90475); 

				float fFXPhase = -1.0f;
				if (pClip)
				{
					CClipEventTags::FindEventPhase(pClip, FPHelmetFX, fFXPhase);
				}

				if (fFXPhase == -1.0f)
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_HELMET_ANIMS, fFirstPersonFXPhase, 0.4f, 0.0f, 1.0f, 0.01f);
					fFXPhase = fFirstPersonFXPhase;
				}

				if (pPed->IsLocalPlayer() && fTakeOffPhase > fFXPhase)
				{
					m_bEnabledHelmetFx = true;
				}
			}

			if (m_bEnabledHelmetFx)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_PutDownHelmetFX, true);
			}

			float fCullPhase = -1.0f;
			// Use the cull tag phase if available
			if (pClip)
			{
				CClipEventTags::FindEventPhase(pClip, CClipEventTags::FirstPersonHelmetCull, fCullPhase);
			}

			// Else fall back to debug cull phase if no tag set
			if (fCullPhase == -1.0f)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_HELMET_ANIMS, fStopCullPhase_TakeOff, 0.5f, 0.0f, 1.0f, 0.01f);
				fCullPhase = fStopCullPhase_TakeOff;
			}

			if (pPed->IsLocalPlayer() && fTakeOffPhase > fCullPhase)
			{
				// Disables PV_FLAG_HIDE_IN_FIRST_PERSON flag check in CPedPropsMgr::RenderPropsInternal from culling the helmet 
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS, true);
			}
		}

		if(GetClipHelper()->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Release, true) || 
			(!CClipEventTags::HasEvent<crPropertyAttributeBool>(GetClipHelper()->GetClip(), CClipEventTags::Object, CClipEventTags::Release, true)))
		{
			const Vector3 impulse(-8.0f,6.0f,-2.5f);
			pPed->GetHelmetComponent()->DisableHelmet(true,&impulse, NetworkInterface::IsGameInProgress());

			// Next state
			SetState(State_waitForClip);
		}
	}
	else
	{
		// No clip playing - quit
		SetState(State_finish);
	}
	return FSM_Continue;
}

//wait until the clip completes before going to the finish state
CTask::FSM_Return CTaskTakeOffHelmet::WaitForClip_OnUpdate(CPed* pPed)
{
	if (!m_bVisorSwitch && NetworkInterface::IsGameInProgress())
		pPed->GetHelmetComponent()->RestoreOldHat(true);

	Vector2 vCurrMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMbr);
	if(!GetClipHelper())
	{
		// Next state
		SetState(State_finish);
	}
	else
	{
		TUNE_GROUP_FLOAT(HELMET_VISOR, TIME_BEFORE_INTERUPT_SECONDS, 0.2f, 0.0f, 5.0f, 0.1f);
		bool bWaitOneUpdateForVisorSwitch = m_bVisorSwitch && GetTimeInState() < TIME_BEFORE_INTERUPT_SECONDS;
		if (!bWaitOneUpdateForVisorSwitch && vCurrMbr.Mag() > MOVEBLENDRATIO_WALK)
		{
			// We can only break out if we actually restored old hat
			if (!m_bVisorSwitch && NetworkInterface::IsGameInProgress())
			{
				if (pPed->GetHelmetComponent()->GetStoredHatPropIndex() == -1 && 
					pPed->GetHelmetComponent()->GetStoredHatTexIndex() == -1)
				{
					SetState(State_finish);
				}
			}
			else
			{
				SetState(State_finish);
			}
		}
	}

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskTakeOffHelmet::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start", 
		"State_waitForStream",
		"State_waitForVisorStream",
		"State_takeOffHelmet",
		"State_switchHelmetVisor",
		"State_putDownHelmet",
		"State_waitForClip",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

void CTaskTakeOffHelmet::EnableHelmet()
{
	GetPed()->GetHelmetComponent()->EnableHelmet(ANCHOR_PH_L_HAND, true);
	m_bEnabledHelmet = true;
	m_bDeferEnableHelmet = false;
}

fwMvClipId CTaskTakeOffHelmet::GetVisorClipId(CPed& rPed, bool bVisorIsUpNow, bool bOnFoot)
{
	bool bNVGogglesEquipped = rPed.GetHelmetComponent()->IsWearingHelmetWithGoggles();

	fwMvClipId clipId = CLIP_ID_INVALID;
	if (bOnFoot)
	{
		if (bNVGogglesEquipped)
		{
			clipId = bVisorIsUpNow ? CLIP_SWITCH_NV_GOGGLES_DOWN_ON_FOOT : CLIP_SWITCH_NV_GOGGLES_UP_ON_FOOT;
		}
		else
		{
			clipId = bVisorIsUpNow ? CLIP_SWITCH_VISOR_DOWN_ON_FOOT : CLIP_SWITCH_VISOR_UP_ON_FOOT;
		}
	}
	else
	{
		if (bNVGogglesEquipped)
		{
			clipId = bVisorIsUpNow ? CLIP_SWITCH_NV_GOGGLES_DOWN_ON_FOOT : CLIP_SWITCH_NV_GOGGLES_UP_ON_FOOT;
		}
		else
		{
			clipId = bVisorIsUpNow ? CTaskMotionInAutomobile::ms_Tunables.m_PutHelmetVisorDownClipId : CTaskMotionInAutomobile::ms_Tunables.m_PutHelmetVisorUpClipId;
		}
	}

	return clipId;
}

void CTaskTakeOffHelmet::CleanUp()
{
	CPed* pPed = GetPed();

	if(!pPed)
	{
		return;		
	}

	if(!pPed->GetHelmetComponent())
	{
		return;
	}

	if (!m_bVisorSwitch)
	{
		// B*2216490: Clear helmet index chosen from the hashmap.
		pPed->GetHelmetComponent()->ClearHelmetIndexUsedFromHashMap();
	}

	// Restore Weapon
	if (!m_bVisorSwitch && pPed && m_bRecreateWeapon && pPed->GetWeaponManager())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
		pPed->GetWeaponManager()->EquipWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), -1, true);
	}

#if FPS_MODE_SUPPORTED
	// Restore equipped weapon if hidden
	if (!m_bVisorSwitch && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetEquippedWeaponInfo() && !pPed->GetEquippedWeaponInfo()->GetIsUnarmed())
	{
		CObject *pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeaponObject && !pWeaponObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
		{
			pWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}

	if (m_pFirstPersonIkHelper)
	{
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif	//FPS_MODE_SUPPORTED

	if (!m_bVisorSwitch)
	{
		// we've been aborted before we've had a chance to replace the hat correctly...
		if(pPed->GetHelmetComponent()->GetStoredHatPropIndex() != -1 || pPed->GetHelmetComponent()->GetStoredHatTexIndex() != -1)
		{
			// there should be no spare slots and the helmet prop should be flagged for a delayed delete...
			if(CPedPropsMgr::GetNumActiveProps(pPed) == MAX_PROPS_PER_PED && CPedPropsMgr::IsPedPropDelayedDelete(pPed, ANCHOR_HEAD))
			{
				if(!pPed->GetHelmetComponent()->IsPendingHatRestore())
				{
					pPed->GetHelmetComponent()->RegisterPendingHatRestore();
				}
			}
		}
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch,false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,false);
}

bool CTaskTakeOffHelmet::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
	{
		const CEventGivePedTask *pGivePedTaskEvent = static_cast<const CEventGivePedTask*>(pEvent);
		if(pGivePedTaskEvent->GetTask() && pGivePedTaskEvent->GetTask()->GetTaskType()==CTaskTypes::TASK_TAKE_OFF_HELMET)
		{
			return false;
		}
	}
	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskTakeOffHelmet::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	ProcessFinished(pPed);
}

bool CTaskTakeOffHelmet::ProcessPostCamera()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if (m_pFirstPersonIkHelper == NULL)
		{
			m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper(CFirstPersonIkHelper::FP_OnFoot);
			taskAssert(m_pFirstPersonIkHelper);

			m_pFirstPersonIkHelper->SetArm(CFirstPersonIkHelper::FP_ArmLeft);
		}

		if (m_pFirstPersonIkHelper)
		{
			m_pFirstPersonIkHelper->Process(pPed);
		}
	}
	else
	{
		if (m_pFirstPersonIkHelper)
			m_pFirstPersonIkHelper->Reset();
	}
#endif //FPS_MODE_SUPPORTED

	return true;
}

void CTaskTakeOffHelmet::ProcessFinished(CPed* pPed)
{
	if(GetClipHelper())
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
		{
			StopClip(FAST_BLEND_OUT_DELTA);
		}
		else
		{
			StopClip(REALLY_SLOW_BLEND_OUT_DELTA);
		}
	}

	if(m_bDeferEnableHelmet)
	{
		EnableHelmet();
	}

	const Vector3 impulse(-8.0f,6.0f,-2.5f);

	if (!m_bVisorSwitch)
	{
		CPedHelmetComponent* pHelmetComponent = pPed->GetHelmetComponent();
		if (pHelmetComponent)
		{
			pHelmetComponent->DisableHelmet(true,&impulse,true);	
			pHelmetComponent->ClearPreLoadedProp();
		}
	}
}
