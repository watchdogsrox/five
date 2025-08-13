#include "Task/Vehicle/TaskPlayerOnHorse.h"

#if ENABLE_HORSE

//Game headers
#include "camera/CamInterface.h"
#include "event/eventDamage.h"
#include "math/angmath.h"
#include "Peds/Horse/horseComponent.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Pickups/Pickup.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/motion/TaskMotionBase.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/System/Task.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Vehicle/TaskPlayerHorseFollow.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "system/control.h"

AI_OPTIMISATIONS()

PARAM(useHorseRelativeControl, "Enable horse relative riding control");

bool CTaskPlayerOnHorse::m_sbStickHeadingUpdatedLastFrame = false;


//////////////////////////////////////////////////////////////////////////
//	FOLLOW TARGET HELPER

float CHorseFollowTargetHelper::sm_HorseFollowActivationTime = 1.f;
float CHorseFollowTargetHelper::sm_HorseFollowFadeoutTime = 1.f;

CHorseFollowTargetHelper::CHorseFollowTargetHelper() :
	m_HorseFollowActivationTime(0.f),
	m_HorseFollowTimeLastActive(0.f)
{
}

// Similar to CTaskPlayerOnHorse::DoJumpCheck
bool CHorseFollowTargetHelper::DoFollowTargetCheck(CPed& player)
{
	CPlayerInfo* pPlayerInfo = player.GetPlayerInfo();
	Assert(pPlayerInfo);
	if (!pPlayerInfo)
		return false;

	// Check if the original RDR horse follow is active
	CPlayerHorseSpeedMatch* pPlayerInputHorse = pPlayerInfo->GetInputHorse();
	if (pPlayerInputHorse)
	{
		CPlayerHorseFollowTargets& horseFollowTargets = pPlayerInputHorse->GetHorseFollowTargets();
		if (!horseFollowTargets.IsEmpty())
		{
			CControl* pControl = player.GetControlFromPlayer();

			bool bSpurredThisFrame = pControl->GetPedSprintIsPressed();
			bool bBrakeButtonDown = pControl->GetVehicleHandBrake().IsDown();	
			bool bMaintainSpeed = ( pControl->GetPedTargetIsDown() || pControl->GetPedSprintIsDown() ) && !bBrakeButtonDown;

			pPlayerInputHorse->UpdateNewHorseFollow(bMaintainSpeed, bSpurredThisFrame);
			return pPlayerInputHorse->IsNewHorseFollowReady();
		}
	}


	// Fallback - use the horse follow method based on formations

	TUNE_GROUP_FLOAT(PLAYER_FOLLOW, HEADING_DIFF_THRESHOLD, 30.f, 0.f, 180.f, 1.f);
	TUNE_GROUP_FLOAT(PLAYER_FOLLOW, FOV_THRESHOLD, 60.f, 0.f, 180.f, 1.f);

	CPhysical* pFollowTarget = pPlayerInfo->GetFollowTarget();
	float fMaxFollowDistance = pPlayerInfo->GetMaxFollowDistance();
	if (!pFollowTarget || !pFollowTarget->GetIsTypePed())
	{
		return false;
	}

	if ((fMaxFollowDistance < 0.f) ||
		(DistSquared(player.GetTransform().GetPosition(), pFollowTarget->GetTransform().GetPosition()).Getf() <= square(pPlayerInfo->GetMaxFollowDistance())))
	{
		float fFollowTargetHeading = pFollowTarget->GetIsTypePed() ? static_cast<CPed*>(pFollowTarget)->GetCurrentHeading() : pFollowTarget->GetTransform().GetHeading();
		bool bFollow = (Abs(SubtractAngleShorter(fFollowTargetHeading, player.GetCurrentHeading())) <= DEG2RAD(HEADING_DIFF_THRESHOLD));
		if (!bFollow)
		{
			Vec3V vLeaderLocalPos = player.GetTransform().UnTransform(pFollowTarget->GetTransform().GetPosition());
			Vec3V vLeaderLocalHeading = Normalize(vLeaderLocalPos);
			bFollow = (vLeaderLocalHeading.GetYf() >= Cosf(DEG2RAD(FOV_THRESHOLD / 2.f)));
		}

		if (bFollow)
		{
			CControl* pControl = player.GetControlFromPlayer();

			bool bSpurredThisFrame = pControl->GetPedSprintIsPressed();
			bool bBrakeButtonDown = pControl->GetVehicleHandBrake().IsDown();	
			bool bMaintainSpeed = ( pControl->GetPedTargetIsDown() || pControl->GetPedSprintIsDown()) && !bBrakeButtonDown;

			UpdateNewHorseFollow(&player, bMaintainSpeed, bSpurredThisFrame);
			return IsNewHorseFollowReady();
		}
	}

	return false;
}

//has the player met the input conditions to speed match?
bool CHorseFollowTargetHelper::IsNewHorseFollowReady() const
{
	return m_HorseFollowTimeLastActive >= TIME.GetElapsedTime() - sm_HorseFollowFadeoutTime;
}

//if bMaintainSpeed is true coming into this function,
//reset the timeout
//if it is false, test against all the other things that can cause us to maintain
//speed:  lookstick, holding b, etc.  if the timer hasn't expired yet, then reset it
void CHorseFollowTargetHelper::UpdateNewHorseFollow(CPed* pPlayerPed, bool& bMaintainSpeedOut, bool bSpurredThisFrame)
{
	//if we aren't holding the button, reset the activation timer
	if (!bMaintainSpeedOut)
	{
		m_HorseFollowActivationTime = 0.f;
	}
	else
	{
		//otherwise, accumulate the time this button is held down
		m_HorseFollowActivationTime += TIME.GetSeconds();
	}

	//if we've held the button long enough, activate the follow behavior
	if (m_HorseFollowActivationTime > sm_HorseFollowActivationTime)
	{
		m_HorseFollowTimeLastActive = TIME.GetElapsedTime();
	}

	//if we're already active, and the user does something like 
	//pan the camera, we want to maintain activation of the behavior
	//the difference is these things can't activate the behavior if the
	//player wasn't already following
	if (IsNewHorseFollowReady())
	{
		CControl* pControl = pPlayerPed->GetControlFromPlayer();
		//bool bIsSteering = !(IsNearZero(pControl->GetPedWalkLeftRight().GetNorm()) || IsNearZero(pControl->GetPedWalkUpDown().GetNorm()));

		//if the player spurs, break him out of the behavior immediately
		//same for player steering
		if (bSpurredThisFrame)
		{
			m_HorseFollowActivationTime = 0.f;
			m_HorseFollowTimeLastActive = 0.f;
			bMaintainSpeedOut = false;
			return;
		}

		if (pControl->GetVehicleHandBrake().IsDown() || pControl->GetSelectWeaponSpecial().IsDown())
		{
			m_HorseFollowTimeLastActive = TIME.GetElapsedTime();
		}

		//moved this outside of the above block so that maintain speed out is always
		//true if within the fadeout time
		bMaintainSpeedOut = true;
	}
}

//	FOLLOW TARGET HELPER
//////////////////////////////////////////////////////////////////////////



CTaskPlayerOnHorse::CTaskPlayerOnHorse() 
: m_bDriveByClipSetsLoaded(false)
, m_bHorseJumping(false)
, m_bAutoJump(false)
, m_iDismountTargetEntryPoint(-1)
, m_bDoAutoJumpTest(true)
{	
	SetInternalTaskType(CTaskTypes::TASK_PLAYER_ON_HORSE);
}

CTaskPlayerOnHorse::~CTaskPlayerOnHorse()
{
}


aiTask* CTaskPlayerOnHorse::Copy() const
{
	CTaskPlayerOnHorse* pTask = rage_new CTaskPlayerOnHorse();	
	return pTask;
}

CTask::FSM_Return CTaskPlayerOnHorse::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	CPed *pMount = pPed->GetMyMount();
	if (!pMount) //horse gone
		return FSM_Quit;

	// Check if the horse is owned locally.
	if(!pMount->IsNetworkClone())
	{
		if (pMount->GetPedIntelligence() && pMount->GetPedIntelligence()->GetTaskManager())
		{
			// See if the mount is tasked properly
			aiTask *pCurrentTask = pMount->GetPedIntelligence()->GetTaskManager()->GetTask(PED_TASK_TREE_MOVEMENT, PED_TASK_MOVEMENT_DEFAULT);
			if (pCurrentTask && pCurrentTask->GetTaskType() != CTaskTypes::TASK_MOVE_MOUNTED)
			{
				// If it's not, retask
				CTaskMoveMounted* pTaskMoveMounted = rage_new CTaskMoveMounted;
				pTaskMoveMounted->SetDesiredHeading(pMount->GetDesiredHeading());
				pMount->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOVEMENT, pTaskMoveMounted, PED_TASK_MOVEMENT_DEFAULT);
			}
		}
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_PlayerOnHorse, true);

	// Work out the best clip group to be used for drivebys
	PreStreamDrivebyClipSets(pPed);

	CheckForWeaponAutoHolster();

	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerOnHorse::ProcessPostFSM()
{
	return FSM_Continue;
}

void CTaskPlayerOnHorse::PreStreamDrivebyClipSets( CPed* pPed )
{
	weaponAssert(pPed->GetWeaponManager());

	const CWeaponInfoBlob::SlotList& slotList = CWeaponInfoManager::GetSlotListNavigate(pPed);

	for (s32 i=0; i<slotList.GetCount(); i++)
	{
		const CVehicleDriveByAnimInfo* pDriveByClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeaponGroup(pPed, slotList[i].GetHash());
		if (pDriveByClipInfo && !m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetClipSet()))
		{
			m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetClipSet());
		}
	}

	// Maybe should stream each clipset for all valid weapons ped has
	if (m_MultipleClipSetRequestHelper.RequestAllClipSets())
	{
		m_bDriveByClipSetsLoaded = true;
	}
	else
	{
		m_bDriveByClipSetsLoaded  = false;
	}
}

#if !__FINAL
const char * CTaskPlayerOnHorse::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] =
	{
		"State_Initial",
		"State_Player_Idles",
		"State_Aim_Use_Gun",
		"State_Ride_Use_Gun",
		"State_Use_ThrowingWeapon",
		"State_Jump",
		"State_Follow_Target",
		"State_Dismount",
		"State_Invalid"
	};

	return sStateNames[iState];
}
#endif

CTask::FSM_Return CTaskPlayerOnHorse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	Assert(pPed->IsAPlayerPed());
	CPed* pPlayerPed = pPed;


FSM_Begin
	FSM_State(STATE_INITIAL)
		FSM_OnUpdate
			return Initial_OnUpdate(pPlayerPed);

	FSM_State(STATE_PLAYER_IDLES)
		FSM_OnEnter
			PlayerIdles_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return PlayerIdles_OnUpdate(pPlayerPed);

	FSM_State(STATE_AIM_USE_GUN)
		FSM_OnEnter
			UseGunAiming_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return UseGunAiming_OnUpdate(pPlayerPed);

	FSM_State(STATE_RIDE_USE_GUN)
		FSM_OnEnter
			UseGunRiding_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return UseGunRiding_OnUpdate(pPlayerPed);

	FSM_State(STATE_USE_THROWING_WEAPON)
		FSM_OnEnter
			UseThrowingWeapon_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return UseThrowingWeapon_OnUpdate(pPlayerPed);

	FSM_State(STATE_JUMP)
		FSM_OnEnter
			Jump_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Jump_OnUpdate(pPlayerPed);

	FSM_State(STATE_FOLLOW_TARGET)
		FSM_OnEnter
			FollowTarget_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return FollowTarget_OnUpdate(pPlayerPed);

	FSM_State(STATE_DISMOUNT)
		FSM_OnEnter
			Dismount_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Dismount_OnUpdate(pPlayerPed);

	FSM_State(STATE_SWAP_WEAPON)
		FSM_OnEnter
			SwapWeapon_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return SwapWeapon_OnUpdate(pPlayerPed);

FSM_End
}

CTask::FSM_Return CTaskPlayerOnHorse::Initial_OnUpdate(CPed* pPlayerPed)
{	
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if (!pControl->GetVehicleExit().IsDown()) //wait for player to release the mount button
		SetState(STATE_PLAYER_IDLES);


	CPed* myMount = pPlayerPed->GetMyMount();
	Assert( myMount );

	CHorseComponent* horseComponent = myMount->GetHorseComponent();
	Assert( horseComponent );
	
	CReins* pReins = horseComponent->GetReins();
	if( pReins )
	{
		pReins->SetHasRider( true );
		pReins->AttachOneHand( pPlayerPed );
	}

	return FSM_Continue;
}

void CTaskPlayerOnHorse::PlayerIdles_OnEnter(CPed* pPlayerPed)
{	
	//set initial heading
	CPed* myMount = pPlayerPed->GetMyMount();
	Assertf(myMount, "PLAYER using CTaskPlayerOnHorse, without a mount!");
	if (myMount) 
	{
		Assert(myMount->GetPedIntelligence());
		CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();

		if (horseMoveTask && horseMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED) {
			CTaskMoveMounted* mountedTask = static_cast<CTaskMoveMounted*>(horseMoveTask);			
			mountedTask->SetDesiredHeading(myMount->GetCurrentHeading());
		}
	}

	//! Hide gun.
	weaponAssert(pPlayerPed->GetWeaponManager());
	CObject* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeaponObject();
	if(pWeapon && pWeapon->GetIsVisible())
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
	}
}

CTask::FSM_Return CTaskPlayerOnHorse::PlayerIdles_OnUpdate(CPed* pPlayerPed)
{
	// Check (and set the state) if we should dismount
	if(CheckForDismount(pPlayerPed))
	{
		return FSM_Continue;
	}

	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	DriveHorse(pPlayerPed);

	// Check to see if we should swap weapons
	if(!pControl || !pControl->GetPedEnter().IsPressed())
	{
		if(CheckForSwapWeapon(pPlayerPed))
		{
			SetState(STATE_SWAP_WEAPON);
			return FSM_Continue;
		}
	}

	// Check for driveby controls		
	weaponAssert(pPlayerPed->GetWeaponManager());

	const CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo();

	bool bOutOfAmmo = true;
	if(pWeaponInfo)
	{
		bOutOfAmmo = pPlayerPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo);
	}
	bool bAimDown = pControl->GetPedTargetIsDown();
	const bool bHasDrivebyWeapons = pWeaponInfo && /*pWeaponInfo->CanBeUsedAsDriveBy(pPlayerPed) TODO: CanWeaponBeUsedOnHorse &&*/ !bOutOfAmmo;	
	bool bCanWeaponReload = pWeapon ? pWeapon->GetCanReload() : false;

	bool bWantsToReload = (pControl->GetPedReload().IsPressed() || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload)) && bCanWeaponReload;

	if(bWantsToReload)
	{
		SetState(STATE_RIDE_USE_GUN);
	}

	if( bAimDown && bHasDrivebyWeapons )
	{
		// Check to see if we've got driveby clips		
		bool bCanDoDriveby = CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPlayerPed);
		// deliberately disabling lasso on horse for now when not using the stun gun impl
		// this will need to be addressed later...
		if(bCanDoDriveby && pWeaponInfo && m_bDriveByClipSetsLoaded)
		{
			if (pWeaponInfo->GetIsThrownWeapon())
			{
				// Check for throwing grenades
				if(!bOutOfAmmo )
				{
					SetState(STATE_USE_THROWING_WEAPON);
				}
			}
			else
			{
				SetState(STATE_AIM_USE_GUN);
			}
		}
	}

	//! Do jump detection.
	if (DoJumpCheck(pPlayerPed))
	{
		SetState(STATE_JUMP);
	}
	else
	{
		if (m_FollowTargetHelper.DoFollowTargetCheck(*pPlayerPed))
		{
			SetState(STATE_FOLLOW_TARGET);
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnHorse::DriveHorse(CPed* pPlayerPed, bool bHeadingOnly)
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	Vector2 vecStickInput;
	vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
	// y stick results are positive for down (pulling back) and negative for up, so reverse to match world y direction
	vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();	

	CPed* myMount = pPlayerPed->GetMyMount();
	Assertf(myMount, "PLAYER using CTaskPlayerOnHorse, without a mount!");

	float fLocalHorseHeading = 0;
	Vector2 vecHorseRelStickInput = vecStickInput;
	// get the orientation of the follow camera, for player controls
	float fControlHeading;
	TUNE_GROUP_BOOL(HORSE_MOVEMENT, bForceHorseRelativeControls, false);
	if (PARAM_useHorseRelativeControl.Get() || bForceHorseRelativeControls)
	{
		fControlHeading = myMount ? myMount->GetCurrentHeading() : pPlayerPed->GetCurrentHeading(); 
	}
	else
	{
		fControlHeading = camInterface::GetPlayerControlCamHeading(myMount ? *myMount : *pPlayerPed);
		fLocalHorseHeading = myMount ? myMount->GetCurrentHeading() : pPlayerPed->GetCurrentHeading();
		fLocalHorseHeading = fwAngle::LimitRadianAngle(fLocalHorseHeading - fControlHeading);
		vecHorseRelStickInput.Rotate(-fLocalHorseHeading);
	}

	float desiredHeading = 0;
	float fHorseRelDesiredHeading = 0;
	bool bIsAiming = pControl->GetPedTargetIsDown();
	bool updateHeading = true;
	bool bStickInput = !vecStickInput.IsZero();	
	if (!bStickInput && !m_sbStickHeadingUpdatedLastFrame ) 
		updateHeading = false;

	if (bStickInput)
		m_sbStickHeadingUpdatedLastFrame = true;
	else if (m_sbStickHeadingUpdatedLastFrame)
		m_sbStickHeadingUpdatedLastFrame = false;


	if(bStickInput)
	{
		float fStickMag = Clamp(vecStickInput.Mag(), 0.0f, 1.0f);
		fHorseRelDesiredHeading = rage::Atan2f(-vecHorseRelStickInput.x, vecHorseRelStickInput.y) * fStickMag;					
	}

	desiredHeading = fHorseRelDesiredHeading + fLocalHorseHeading + fControlHeading;
	desiredHeading = fwAngle::LimitRadianAngle(desiredHeading);

	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	Assert(pPlayerInfo);	
	if(pPlayerInfo->GetSimulateGaitInputOn())	// if we want to simulate player's movement
	{
		float fDuration = pPlayerInfo->GetSimulateGaitDuration();
		float fTimerCount = pPlayerInfo->GetSimulateGaitTimerCount();
		if (!pPlayerInfo->GetSimulateGaitNoInputInterruption() && bStickInput) // reset simulated gait.
		{

			pPlayerInfo->ResetSimulateGaitInput();
		}
		else if(fDuration < 0.0f || fDuration >= fTimerCount )
		{
			vecStickInput.x = 0.0f;
			vecStickInput.y = 1.0f;

			desiredHeading = pPlayerInfo->GetSimulateGaitHeading();
			updateHeading = true;
			if(pPlayerInfo->GetSimulateGaitUseRelativeHeading())
			{
				desiredHeading = fwAngle::LimitRadianAngle(desiredHeading + pPlayerInfo->GetSimulateGaitStartHeading());				
			}
			else
			{
				desiredHeading = fwAngle::LimitRadianAngle(desiredHeading);				
			}

			fTimerCount += fwTimer::GetTimeStep();
			pPlayerInfo->SetSimulateGaitTimerCount(fTimerCount);
		}
		else
		{
			pPlayerInfo->ResetSimulateGaitInput();
		}

		if (myMount)
		{
			float moveBlendRatio = pPlayerInfo->GetSimulateGaitMoveBlendRatio();
			myMount->GetHorseComponent()->GetSpeedCtrl().SetTgtSpeed(moveBlendRatio / MOVEBLENDRATIO_SPRINT);
		}
	}

	// Set the input for the horse task
		
	bool spur = pControl->GetPedSprintIsPressed();
	bool bBrakeButtonDown = pControl->GetVehicleHandBrake().IsDown();	
	bool bHardBrake = false; //pControl->GetVehicleHandBrake().IsDown();
	static dev_float sf_MinSlideBrake = 2.0f;
	bool bIsDownHill = myMount->GetTransform().GetForward().GetZf() < -0.25f;
	float fSoftBrake = ((!bHardBrake && bBrakeButtonDown) ? 1.0f : 0.0f) || (!bIsDownHill && myMount->GetSlideSpeed() > sf_MinSlideBrake);
	bool bMaintainSpeed = ( pControl->GetPedTargetIsDown() || pControl->GetPedSprintIsDown()) && !bBrakeButtonDown;
	bIsAiming = pControl->GetPedTargetIsDown();
	float outputMatchSpeed = -1.0f;

	if (myMount) 
	{
		Assert(myMount->GetPedIntelligence());

		CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();
		if (horseMoveTask && horseMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED) {
			CTaskMoveMounted* mountedTask = static_cast<CTaskMoveMounted*>(horseMoveTask);
			if (!updateHeading)
				desiredHeading = myMount->GetCurrentHeading();
			else 
			{
				float x,y;
				myMount->GetMotionData()->GetDesiredMoveBlendRatio(x,y);				
				if (y==MOVEBLENDRATIO_REVERSE) //reverse controls if reversing
					desiredHeading = fwAngle::LimitRadianAngleSafe( desiredHeading+PI); 
			}

			if (bHeadingOnly)
			{
				float fStickMag = Clamp(vecStickInput.Mag(), 0.0f, 1.0f);
				mountedTask->SetDesiredHeading(desiredHeading);
				//TMS: The above task seems to be being created and destroyed at high speed
				myMount->GetHorseComponent()->SetHeadingOverride(desiredHeading, fStickMag);			
			}
			else
			{
				mountedTask->ApplyInput(desiredHeading, spur, bHardBrake, fSoftBrake, bMaintainSpeed, bIsAiming, vecStickInput, outputMatchSpeed);			
				if (bHardBrake) //may need refined?
				{
					mountedTask->RequestReverse();
				}
			}
		}
	}
}

void CTaskPlayerOnHorse::UseGunAiming_OnEnter(CPed* UNUSED_PARAM(pPed))
{	
	aiTask* pNewTask = rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Player);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskPlayerOnHorse::UseGunAiming_OnUpdate(CPed* pPlayerPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		SetState(STATE_PLAYER_IDLES);
		return FSM_Continue;
	}

	// Check (and set the state) if we should dismount
	if(CheckForDismount(pPlayerPed))
	{
		return FSM_Continue;
	}

	DriveHorse(pPlayerPed);

	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	bool bAimDown = pControl->GetPedTargetIsDown();				
	if(!bAimDown)
	{			
		// Try to make the gun task abort
		if(taskVerifyf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN,"NULL or unexpected subtask running"))
		{
			// Request termination - the vehicle gun task will terminate soon on this request
			GetSubTask()->RequestTermination();				
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnHorse::UseGunRiding_OnEnter(CPed* UNUSED_PARAM(pPed))
{	
	aiTask* pNewTask = rage_new CTaskGun(CWeaponController::WCT_Player, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskPlayerOnHorse::UseGunRiding_OnUpdate(CPed* pPlayerPed)
{

	// Check (and set the state) if we should dismount
	if(CheckForDismount(pPlayerPed))
	{
		return FSM_Continue;
	}

	DriveHorse(pPlayerPed);


	// The only use case for this so far is reloading; check if the reload animation is done.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished)) 
	{
		SetState(STATE_PLAYER_IDLES);
	}

	return FSM_Continue;
}

void CTaskPlayerOnHorse::UseThrowingWeapon_OnEnter(CPed* UNUSED_PARAM(pPed))
{	
	aiTask* pNewTask = rage_new CTaskMountThrowProjectile(CWeaponController::WCT_Player, 0, CWeaponTarget()); 
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskPlayerOnHorse::UseThrowingWeapon_OnUpdate(CPed* pPlayerPed)
{
 	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished)) 
 	{
 		// Go back to basic drive task
 		SetState(STATE_PLAYER_IDLES);
 		return FSM_Continue;
 	}

	// Check (and set the state) if we should dismount
	if(CheckForDismount(pPlayerPed))
	{
		return FSM_Continue;
	}

	DriveHorse(pPlayerPed);

	return FSM_Continue;
}

void CTaskPlayerOnHorse::Jump_OnEnter(CPed* pPed)
{
	m_bHorseJumping=false;
	CPed* myMount = pPed->GetMyMount();
	Assertf(myMount, "PLAYER using CTaskPlayerOnHorse, without a mount!");
	if (myMount) 
	{
		Assert(myMount->GetPedIntelligence());
		CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();
		if (horseMoveTask && horseMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED) {
			CTaskMoveMounted* mountedTask = static_cast<CTaskMoveMounted*>(horseMoveTask);
			mountedTask->RequestJump(m_bAutoJump ? CTaskMoveMounted::eJR_Auto : CTaskMoveMounted::eJR_Manual );			
		}
	}	
}

CTask::FSM_Return CTaskPlayerOnHorse::Jump_OnUpdate(CPed* pPlayerPed)
{
	//TODO rider jump
	CPed* myMount = pPlayerPed->GetMyMount();
	Assertf(myMount, "PLAYER using CTaskPlayerOnHorse, without a mount!");
	if (!m_bHorseJumping)
	{
		if (myMount && myMount->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))
			m_bHorseJumping=true;
		else
		{
			Assert(myMount->GetPedIntelligence());
			CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();
			if (horseMoveTask && horseMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED) 
			{
				CTaskMoveMounted* mountedTask = static_cast<CTaskMoveMounted*>(horseMoveTask);
				mountedTask->RequestJump(m_bAutoJump ? CTaskMoveMounted::eJR_Auto : CTaskMoveMounted::eJR_Manual );
			}
		}
	}
	if (!myMount || (m_bHorseJumping && !myMount->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))) 
	{		
		m_bAutoJump = false;
		SetState(STATE_PLAYER_IDLES);		
	}
	return FSM_Continue;
}

void CTaskPlayerOnHorse::FollowTarget_OnEnter(CPed* pPed)
{
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	Assert(pPlayerInfo);
	if (!pPlayerInfo)
		return;

	// Check if the original RDR horse follow is active
	CPlayerHorseSpeedMatch* pPlayerInputHorse = pPlayerInfo->GetInputHorse();
	if (pPlayerInputHorse)
	{
		CPlayerHorseFollowTargets& horseFollowTargets = pPlayerInputHorse->GetHorseFollowTargets();
		if (!horseFollowTargets.IsEmpty())
		{
			CTaskPlayerHorseFollow* pSubTask = rage_new CTaskPlayerHorseFollow;
			pSubTask->SetOnlyMatchSpeed(true);
			SetNewTask(pSubTask);
			return;
		}
	}


	// Fallback - use the horse follow method based on formations

	CPhysical* pPedFollowTarget = pPlayerInfo->GetFollowTarget();
	float fMaxDistance = pPlayerInfo->GetMaxFollowDistance();
	if (pedVerifyf(pPedFollowTarget->GetIsTypePed(), "Follow Target must be a ped! Not peds not supported yet!"))
	{
		SetNewTask(rage_new CTaskFollowLeaderInFormation(NULL, static_cast<CPed*>(pPedFollowTarget), fMaxDistance));
	}
}

CTask::FSM_Return CTaskPlayerOnHorse::FollowTarget_OnUpdate(CPed* pPlayerPed)
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !m_FollowTargetHelper.DoFollowTargetCheck(*pPlayerPed))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	else
	{
		Assert(pPlayerPed);
		Assert(pPlayerPed->GetMyMount());

		CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();

		CTask* pSubTask = GetSubTask();
		if (pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_PLAYER_HORSE_FOLLOW))
		{
			CTaskPlayerHorseFollow* pTPHF = static_cast<CTaskPlayerHorseFollow*>(pSubTask);
			pPlayerInfo->GetInputHorse()->SetCurrentSpeedMatchSpeed(pTPHF->GetCurrentSpeedMatchSpeedNormalized());
		}
		else
		{
			Assert(pPlayerPed->GetMyMount()->GetPedIntelligence());
			CTask* pTask = pPlayerPed->GetMyMount()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION);
			if (pTask)
			{
				CTaskMoveBeInFormation* pTMBIF = static_cast<CTaskMoveBeInFormation*>(pTask);
				pTMBIF->SetNonGroupWalkAlongsideLeader(true);
				//pTMBIF->SetTargetLocalOffset(pPlayerInfo->GetFollowTargetOffset());
				//pTMBIF->SetMayFollowNonGroupLeaderGroupMembers(pPlayerInfo->GetMayFollowTargetGroupMembers());
				pTMBIF->SetNonGroupSpacing(pPlayerInfo->GetFollowTargetRadius());
			}
		}

		CControl *pControl = pPlayerPed->GetControlFromPlayer();
		if ((!IsNearZero(pControl->GetPedWalkLeftRight().GetNorm()) || !IsNearZero(pControl->GetPedWalkUpDown().GetNorm())))
		{
			DR_ONLY(debugPlayback::RecordTaggedFloatValue(*pPlayerPed->GetMyMount()->GetCurrentPhysicsInst(), 1.0f, "DisableMoveTaskHeadingAdjustments"));
			pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments, true);
			DriveHorse(pPlayerPed, true);
			DR_ONLY(debugPlayback::RecordTaggedFloatValue(*pPlayerPed->GetMyMount()->GetCurrentPhysicsInst(), 0.0f, "DisableMoveTaskHeadingAdjustments"));
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnHorse::Dismount_OnEnter(CPed* pPlayerPed)
{		
	CTask* pTask = rage_new CTaskDismountAnimal(GetDefaultDismountFlags(), 0.0f, NULL, m_iDismountTargetEntryPoint);
	SetNewTask( pTask );

	//clear old horse heading changes, prevents old turn in place parameters from re-engaging after dismount	
	CPed* myMount = pPlayerPed->GetMyMount();
	taskAssertf(myMount, "PLAYER using CTaskPlayerOnHorse, without a mount!");	
	if (myMount) 
	{
		Assert(myMount->GetPedIntelligence());
		CTask* horseMoveTask = myMount->GetPedIntelligence()->GetDefaultMovementTask();
		if (horseMoveTask && horseMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED) {
			CTaskMoveMounted* mountedTask = static_cast<CTaskMoveMounted*>(horseMoveTask);	
			mountedTask->SetDesiredHeading(myMount->GetCurrentHeading());			
		}
	}
}

CTask::FSM_Return CTaskPlayerOnHorse::Dismount_OnUpdate(CPed* pPlayerPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) )
		{
			// Ped got off mount, quit the ride task
			return FSM_Quit;
		}
		else
		{
			SetState(STATE_PLAYER_IDLES);
			return FSM_Continue;
		}	
	}
	return FSM_Continue;
}

void CTaskPlayerOnHorse::SwapWeapon_OnEnter(CPed* pPlayerPed)
{
	s32 iFlags = 0;
	const CPedWeaponManager* pWeaponMgr = pPlayerPed->GetWeaponManager();
	taskAssert( pWeaponMgr );

	// Check the currently equipped weapon to determine if we should holster before drawing
	const CWeaponInfo* pEquippedInfo = pWeaponMgr->GetEquippedWeaponInfo();
	if( pEquippedInfo && pEquippedInfo->GetIsCarriedInHand() && pWeaponMgr->GetEquippedWeaponObject() != NULL )
		iFlags = SWAP_HOLSTER;

	// Should we draw the next weapon?
	const CWeaponInfo* pPendingInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeaponMgr->GetEquippedWeaponHash());
	if( pPendingInfo && pPendingInfo->GetIsCarriedInHand() )
		iFlags |= SWAP_DRAW;

	// If nothing has been set swap instantly
	if( iFlags == 0 )
		iFlags = SWAP_INSTANTLY;

	// Since we are riding a horse simply run the swap weapon task
	SetNewTask( rage_new CTaskSwapWeapon( iFlags ) );
}

CTask::FSM_Return CTaskPlayerOnHorse::SwapWeapon_OnUpdate(CPed* pPlayerPed)
{
	// Has the swap weapon task finished?
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || !GetSubTask() )
	{
		SetState( STATE_PLAYER_IDLES );
		return FSM_Continue;
	}

	// Continue to allow the player to drive the horse
	DriveHorse( pPlayerPed );

	// Should we still allow a jump?
	if( DoJumpCheck( pPlayerPed ) )
	{
		SetState(STATE_JUMP);
	}

	return FSM_Continue;
}

bool CTaskPlayerOnHorse::CheckForDismount( CPed* pPlayerPed )
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	bool bExitPressed = pControl->GetVehicleExit().IsPressed();
	CPed* myMount = pPlayerPed->GetMyMount();
	if (!myMount)
		return true;

	if (bExitPressed || myMount->GetUsingRagdoll() || myMount->IsDead())
	{
		m_iDismountTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPlayerPed, myMount, SR_Any, myMount->GetSeatManager()->GetPedsSeatIndex(pPlayerPed), false, GetDefaultDismountFlags());

		if(m_iDismountTargetEntryPoint >= 0)
		{
			SetState(STATE_DISMOUNT);
			return true;
		}
	}

	return false;
}

bool CTaskPlayerOnHorse::CheckForSwapWeapon( CPed* pPlayerPed )
{
	taskAssert( pPlayerPed );
	const CPedWeaponManager* pWeaponMgr = pPlayerPed->GetWeaponManager();
	taskAssert( pWeaponMgr );

	if( pWeaponMgr->GetRequiresWeaponSwitch() )
	{
		const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeaponMgr->GetEquippedWeaponHash());
		if(pInfo)
		{
			// Ensure weapon is streamed in
			if( pPlayerPed->GetInventory()->GetIsStreamedIn( pInfo->GetHash() ) )
			{
				SetState( STATE_SWAP_WEAPON );
				return true;
			}				
		}
	}

	return false;
}

bool CTaskPlayerOnHorse::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	
	if (task.IsOnFoot())
		isValid = true;

	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}

	return isValid;
}

void CTaskPlayerOnHorse::CleanUp()
{
}

bool CTaskPlayerOnHorse::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}

bool CTaskPlayerOnHorse::DoJumpCheck(CPed* pPlayerPed)
{
	CControl* pControl = pPlayerPed->GetControlFromPlayer();
	bool bJumpPressed = pControl && pControl->GetPedJump().IsPressed();

	//! Hack. Perform every 2nd tick. 
	m_bDoAutoJumpTest = !m_bDoAutoJumpTest;

	m_bAutoJump = false;

	taskAssert(pPlayerPed->GetMyMount());

	CPed *pMount = pPlayerPed->GetMyMount();

	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseAutoJumpParallelDot, 0.4f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseAutoJumpForwardDot, 0.75f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseAutoJumpStickDot, 0.75f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultDistMin, 3.0f, 0.0f, 15.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultDistMax, 8.0f, 0.0f, 15.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultDistCutoff, 3.0f, 0.0f, 15.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultHeightThresholdMin, 0.65f, 0.01f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultHeightThresholdMax, 2.00f, 0.01f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultDepthThreshold, 1.75f, 0.01f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpVaultMBRThreshold, 0.1f, 0.01f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpMinHorizontalClearance, 0.75f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseJumpZClearanceAdjustment, 0.5f, 0.0f, 100.0f, 0.1f);

	//! Get results immediately if we press button.
	if(bJumpPressed)
	{
		pMount->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
	}
	else
	{
		if(!m_bDoAutoJumpTest)
			return false;

		CTaskMotionBase* pTask = pMount->GetCurrentMotionTask();
		bool bReducingGait = pTask && pTask->IsGaitReduced();

		bool bAutoVaultEnabled = !pMount->GetPedResetFlag(CPED_RESET_FLAG_DisablePlayerAutoVaulting) && 
			!pMount->GetPedResetFlag(CPED_RESET_FLAG_IsOnAssistedMovementRoute) && 
			!pMount->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute);

		if( bAutoVaultEnabled && ((pMount->GetMotionData()->GetCurrentMbrY() >= fHorseJumpVaultMBRThreshold) || bReducingGait) )
		{
			pMount->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
			pMount->SetPedResetFlag(CPED_RESET_FLAG_SearchingForAutoVaultClimb, true);
		}
	}

	sPedTestParams pedTestParams;
	pedTestParams.fParallelDot = fHorseAutoJumpParallelDot;
	pedTestParams.fForwardDot = fHorseAutoJumpForwardDot;
	pedTestParams.fStickDot = fHorseAutoJumpStickDot;
	pedTestParams.fAutoJumpVaultDistCutoff = fHorseJumpVaultDistCutoff;
	pedTestParams.fJumpVaultMBRThreshold = fHorseJumpVaultMBRThreshold;
	pedTestParams.fVaultDistStanding = -1.0f;
	pedTestParams.fVaultDistStandingVehicle = -1.0f;
	pedTestParams.fJumpVaultHeightThresholdMin = fHorseJumpVaultHeightThresholdMin;
	pedTestParams.fJumpVaultHeightThresholdMax = fHorseJumpVaultHeightThresholdMax;
	pedTestParams.fJumpVaultDepthThresholdMin = 0.0f;
	pedTestParams.fJumpVaultDepthThresholdMax = fHorseJumpVaultDepthThreshold;
	pedTestParams.bDepthThresholdTestInsideRange = true;
	pedTestParams.fMaxVaultDistance = 0.0f;
	pedTestParams.bDoJumpVault = true;
	pedTestParams.bOnlyDoJumpVault = true;
	pedTestParams.bShowStickIntent = false;
	pedTestParams.fCurrentMBR = pMount->GetMotionData()->GetCurrentMbrY();
	pedTestParams.fJumpVaultMinHorizontalClearance = fHorseJumpMinHorizontalClearance;
	pedTestParams.fOverrideMaxClimbHeight = fHorseJumpVaultHeightThresholdMax;
	pedTestParams.fUseTestOrientationHeight = fHorseJumpVaultHeightThresholdMax;
	pedTestParams.fAutoJumpVaultDistMin = fHorseJumpVaultDistMin;
	pedTestParams.fAutoJumpVaultDistMax = fHorseJumpVaultDistMax;
	pedTestParams.fZClearanceAdjustment = fHorseJumpZClearanceAdjustment;

	//! Run climb detector from horse's perspective.
	sPedTestResults pedTestResults;
	pMount->GetPedIntelligence()->GetClimbDetector().Process(NULL, &pedTestParams, &pedTestResults);

	CClimbHandHoldDetected handHold;
	if(pMount->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold))
	{
		if(pedTestResults.GetCanAutoJumpVault() || (pedTestResults.GetCanJumpVault() && bJumpPressed) )
		{
			m_bAutoJump = true;
			return true;
		}
	}

	if(bJumpPressed)
	{
		//! Don't allow manual jumping when we have a potential auto-jump lined up. We'll end up jumping into it :)
		static dev_u32 s_uDetectedHandholdCooldown = 200; 
		bool bDetectingAutoJump = fwTimer::GetTimeInMilliseconds() < 
			(pMount->GetPedIntelligence()->GetClimbDetector().GetLastDetectedHandHoldTime()+s_uDetectedHandholdCooldown);

		if(!bDetectingAutoJump)
		{
			return true;
		}
	}

	return false;
}

VehicleEnterExitFlags CTaskPlayerOnHorse::GetDefaultDismountFlags() const
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);

	return vehicleFlags;
}

void CTaskPlayerOnHorse::CheckForWeaponAutoHolster()
{
	//CPed* pPed = GetPed();
	//CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();

	//if(taskVerifyf(pPlayerInfo, "Player without a PlayerInfo"))
	//{
	//	bool bBlockAutoHolster(false);

	//	switch(GetState())
	//	{
	//		case STATE_PLAYER_IDLES:
	//			// auto holster allowed
	//		break;
	//		default: 
	//			bBlockAutoHolster = true;
	//		break;
	//	}

	//	if(pPlayerInfo->UpdateAutoHolsterWeaponInfo(bBlockAutoHolster))
	//	{
	//		pPed->GetWeaponManager()->BackupEquippedWeapon();
	//		pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
	//	}
	//}
}

#if !__FINAL
void CTaskPlayerOnHorse::Debug() const
{
	// Base class
	CTask::Debug();
}
#endif

#endif // ENABLE_HORSE

// eof

