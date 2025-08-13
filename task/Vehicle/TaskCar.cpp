#include "TaskCar.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/Random.h"
#include "fwmaths/Vector.h"

// Game headers
#include "ai/Ambient/ConditionalAnimManager.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "audio/caraudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/scannermanager.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "debug/DebugScene.h"
#include "Event/EventReactions.h"
#include "vehicles/VehicleGadgets.h"
#include "Control/GameLogic.h"
#include "Control/Route.h"
#include "Control/Record.h"
#include "Event/EventGroup.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "frontend/MiniMap.h"
#include "frontend/MobilePhone.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "frontend/MultiplayerChat.h"

#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Scene/Entity.h"
#include "Script/Script.h"
#include "script/script_cars_and_peds.h"
#include "streaming/populationstreaming.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/General/TaskBasic.h"
#include "Task/Response/TaskFlee.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Train.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Vehiclepopulation.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vfx/Misc/Fire.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleCruise.h"
#include "vehicleAi/Task/TaskVehicleGoto.h"
#include "vehicleAi/Task/TaskVehicleGotoAutomobile.h"
#include "vehicleAi/Task/TaskVehicleTempAction.h"
#include "vehicleAi/Task/TaskVehiclePlayer.h"
#include "VehicleAi/Task/TaskVehicleFlying.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

// network includes
#include "network/NetworkInterface.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//std headers
#include <limits>

// grrr. This macro conflicts under visual studio.
#if defined (MSVC_COMPILER)
#ifdef max
#undef max
#endif //max
#endif //MSVC_COMPILER

#define MAX_ACCEL (255.0f)

#define NICE_VEHICLE_SLOW_SPEED_SQ_THRESHOLD	(700.0f)
#define NICE_VEHICLE_SLOW_SPEED_EVENT_TIMER		(3000)
#define VEHICLE_INAPPROPRIATE_AREA_EVENT_TIMER	(15000)
#define NICE_VEHICLE_EVENT_TIMER_MIN			(30000)
#define NICE_VEHICLE_EVENT_TIMER_MAX			(60000)

extern audScannerManager g_AudioScannerManager;

u32	CTaskInVehicleBasic::sm_LastVehicleFlippedSpeechTime = 0u;
u32	CTaskInVehicleBasic::sm_LastVehicleFireSpeechTime = 0u;
u32 CTaskInVehicleBasic::sm_LastVehicleJumpSpeechTime = 0u;

//////////////////////////////////////////////////////////////////////////

CTaskLeaveAnyCar::CTaskLeaveAnyCar(s32 iDelayTime, VehicleEnterExitFlags iRunningFlags)
: m_iDelayTime(iDelayTime)
, m_iRunningFlags(iRunningFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_LEAVE_ANY_CAR);
#if __BANK
	AI_LOG_WITH_ARGS("[VehicleExit] - TASK_LEAVE_ANY_VEHICLE constructed at 0x%p, see console log for callstack\n", this);
	aiDisplayf("Frame %i, TASK_LEAVE_ANY_VEHICLE constructed at 0x%p", fwTimer::GetFrameCount(), this);
	sysStack::PrintStackTrace();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskLeaveAnyCar::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				return ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskLeaveAnyCar::Start_OnUpdate()
{
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "Ped %s starting CTaskLeaveAnyCar - 0x%p\n", AILogging::GetDynamicEntityNameSafe(GetPed()), this);

#if __BANK
	if (GetPed()->IsLocalPlayer())
	{
		GetPed()->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
	}
#endif // __BANK

	// If the ped is in a vehicle, exit it otherwise quit
	if (GetPed()->GetIsInVehicle())
	{	
		SetState(State_ExitVehicle); 
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskLeaveAnyCar::ExitVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	taskAssertf(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ),"Cannot exit vehicle: Ped is not in a vehicle");
	taskAssertf(pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed) >= 0, "Ped doesn't have a valid seat index");

	if (m_iDelayTime > 0.0f)
	{
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	}

	fwMvClipSetId streamedExitClipSetId = CLIP_SET_ID_INVALID;

	//Check if the ped is a player.

	const CConditionalClipSet* pClipSet = NULL;
	if(pPed->IsPlayer())
	{
		const CVehicleClipRequestHelper* pClipRequestHelper = CTaskVehicleFSM::GetVehicleClipRequestHelper(pPed);
		pClipSet = pClipRequestHelper ? pClipRequestHelper->GetConditionalClipSet() : NULL;
		
		if (pClipSet)
		{
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::IsStreamedExit);
			streamedExitClipSetId = pClipSet->GetClipSetId();
		}
	}

	//Create the task.
	CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(), m_iRunningFlags, (float)m_iDelayTime/1000.0f);
	pTask->SetStreamedExitClipsetId(streamedExitClipSetId);

	//Start the task.
	SetNewTask(pTask);	

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskLeaveAnyCar::ExitVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
#if __ASSERT
		if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE)
		{
			if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::WarpIfDoorBlocked))
			{
				taskAssertf(!GetPed()->GetIsInVehicle(), "Ped %s failed to exit vehicle correctly, infinite loop likely to follow, subtask ? %s, subtask previous state %s", GetPed()->GetDebugName(), GetSubTask() ? "TRUE" : "FALSE", CTaskExitVehicle::GetStaticStateName(GetSubTask()->GetPreviousState()));
			}
		}
#endif // __ASSERT
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo *CTaskLeaveAnyCar::CreateQueriableState() const
{
    return rage_new CClonedLeaveAnyCarInfo(m_iDelayTime, m_iRunningFlags);
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskLeaveAnyCar::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:		return "State_Start";
		case State_ExitVehicle: return "State_ExitVehicle";
		case State_Finish:		return "State_Finish";
	}
	return "State_Invalid";
}
#endif //!__FINAL

////////////////////////////////////////////////////////////////////////////////

CClonedLeaveAnyCarInfo::CClonedLeaveAnyCarInfo(s32 delayTime, VehicleEnterExitFlags runningFlags) :
m_iDelayTime(delayTime)
, m_iRunningFlags(runningFlags)
{
}

CClonedLeaveAnyCarInfo::CClonedLeaveAnyCarInfo() :
m_iDelayTime(0)
{
    m_iRunningFlags.BitSet().Reset();
}

CTask *CClonedLeaveAnyCarInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    return rage_new CTaskLeaveAnyCar(m_iDelayTime, m_iRunningFlags);
}

/////////////////
//DRIVE
/////////////////

////////////////////////////////////////////////////////////////////////////////
CTaskInVehicleBasic::Tunables CTaskInVehicleBasic::sm_Tunables;
IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskInVehicleBasic, 0xaa21baa3);

bool CTaskInVehicleBasic::StreamTaskAssets(const CVehicle* pVehicle, s32 iSeatIndex)
{
	return CTaskVehicleFSM::RequestClipSetFromSeat(pVehicle, iSeatIndex);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInVehicleBasic::CTaskInVehicleBasic(CVehicle* pTargetVehicle, const bool bDriveAnyCar, const CConditionalAnimsGroup* pAnims, fwFlags8 uFlags)
: m_DriveBy()
, m_Siren()
, m_pAnims(pAnims)
, m_pTargetVehicle(pTargetVehicle)
, m_broadcastStolenCarTimer()
, m_broadcaseNiceCarTimer()
, m_nUpsideDownCounter(0)
, m_fTimeOffTheGround(0.0f)
, m_uInVehicleBasicFlags(uFlags)
, m_bDriveAnyCar(bDriveAnyCar)
, m_bCanAttachToVehicle(false)
, m_bSetPedOutOfVehicle(false)
, m_pPassengerPlayerIsLookingAt(NULL)
, m_uPassengerLookingEndTime(0)
, m_bWasVehicleUpsideDown(false)
, m_bWasVehicleOnFire(false)
{
	SetInternalTaskType(CTaskTypes::TASK_IN_VEHICLE_BASIC);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInVehicleBasic::~CTaskInVehicleBasic()
{
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::ProcessPreFSM()
{   
	CPed* pPed = GetPed();
	//If ped is in a vehicle that isn't the vehicle for the task then set the task 
	//vehicle to be the ped vehicle. 
	if(m_bDriveAnyCar && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()!=m_pTargetVehicle)
	{
		m_pTargetVehicle=pPed->GetMyVehicle();		
	}
	
	//Check if driving task is still valid.
    if(!m_pTargetVehicle || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
    {
        taskAssertf(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetMyVehicle(), "Ped in vehicle flag set with no vehicle pointer!");
		AbortIK(pPed);
	    return FSM_Quit;
    }

#if __BANK
	const bool bPreviousValueSetPedOutOfVehicle = m_bSetPedOutOfVehicle;
#endif // __BANK

	//If the ped can not drive this vehicle then set the ped to exit the vehicle.
	m_bSetPedOutOfVehicle = NetworkInterface::IsInvalidVehicleForDriving(pPed, m_pTargetVehicle);

#if __BANK
	if (NetworkInterface::IsGameInProgress() && m_bSetPedOutOfVehicle && m_bSetPedOutOfVehicle != bPreviousValueSetPedOutOfVehicle)
	{
		const CNetObjPed*     pedNetworkObj = static_cast<const CNetObjPed*>(pPed->GetNetworkObject());
		const CNetObjVehicle* vehNetworkObj = static_cast<const CNetObjVehicle*>(m_pTargetVehicle->GetNetworkObject());

		if (pedNetworkObj && pedNetworkObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_RESPAWNPED))
		{
			AI_LOG_WITH_ARGS("[ExitVehicle] - Ped %s is being forced out of vehicle because CNetObjGame::LOCALFLAG_RESPAWNPED was set", AILogging::GetDynamicEntityNameSafe(pPed));
		}
		else if (vehNetworkObj && vehNetworkObj->IsLockedForAnyPlayer())
		{
			AI_LOG_WITH_ARGS("[ExitVehicle] - Ped %s is being forced out of vehicle because vehicle %s was locked for any player", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(m_pTargetVehicle));
		}
	}
#endif // __BANK

    //Check if vehicle is upside down
	//Or in water
	//Or undrivable
	CheckForUndrivableVehicle(pPed);
    
	// Only generate stolen cop car events if the player isn't using a cop model
	const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pModelInfo);
	Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
	Assertf(pModelInfo->GetDrawable() || pModelInfo->GetFragType(), "%s:Ped model is not loaded", pModelInfo->GetModelName());
	const ePedType pedType = pModelInfo->GetDefaultPedType();

    if(pPed->IsPlayer() && m_pTargetVehicle->IsLawEnforcementVehicle() && pedType != PEDTYPE_COP)
	{ 	           				
		if(!m_broadcastStolenCarTimer.IsSet() || m_broadcastStolenCarTimer.IsOutOfTime())
		{
			m_broadcastStolenCarTimer.Set(fwTimer::GetTimeInMilliseconds(),2000);
			CEventCopCarBeingStolen event(pPed,m_pTargetVehicle);
			GetEventGlobalGroup()->Add(event);
		}
	}

	UpdateCommentOnVehicle(pPed);

	//CEventDisturbance eventDisturbance(NULL, pPed, CEventDisturbance::ePedInNearbyCar);
	//GetEventGlobalGroup()->Add(eventDisturbance);
		
	if( !m_pTargetVehicle->IsInAir() )
	{
		m_fTimeOffTheGround = 0.0f;
		m_uInVehicleBasicFlags.ClearFlag(IVF_PlayedJumpAudio);
	}
	else
	{
		m_fTimeOffTheGround += GetTimeStep();
		if (m_pTargetVehicle->GetVehicleType() == VEHICLE_TYPE_CAR && !m_uInVehicleBasicFlags.IsFlagSet(IVF_PlayedJumpAudio) && m_fTimeOffTheGround > CTaskInVehicleBasic::sm_Tunables.m_fSecondsInAirBeforePassengerComment)
		{
			m_uInVehicleBasicFlags.SetFlag(IVF_PlayedJumpAudio);
			if (pPed != m_pTargetVehicle->GetDriver() && pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->DoesContextExistForThisPed("VEHICLE_JUMP"))
			{
				// Add a bit of a timeout to this to prevent multiple passengers speaking over the top of each other or if we jump multiple times in quick succession
				if (fwTimer::GetFrameCount() - sm_LastVehicleJumpSpeechTime > 2000)
				{
					// If > 1 passenger, make this a randomized say so that we vary the person who comments
					if (pPed->RandomlySay("VEHICLE_JUMP", m_pTargetVehicle->GetNumberOfPassenger() > 1 ? 0.5f : 1.0f))
					{
						sm_LastVehicleJumpSpeechTime = fwTimer::GetTimeInMilliseconds();
					}
				}
			}
		}
	}

	static float LONG_TIME_OFF_GROUND = 3.0f;
	if(m_fTimeOffTheGround > LONG_TIME_OFF_GROUND && !m_pTargetVehicle->IsDriver(pPed) && !m_pTargetVehicle->GetIsRotaryAircraft() )
	{
		pPed->RandomlySay("CAR_JUMP", 0.5f);
	}

	//Add mad driver event if the vehicle is off the ground
	static float MAD_DRIVER_IN_AIR = 0.5f;
	if (m_fTimeOffTheGround > 0)
	{
		m_pTargetVehicle->SetDrivenDangerouslyThisUpdate(true);

		if (m_fTimeOffTheGround > MAD_DRIVER_IN_AIR)
		{
			m_pTargetVehicle->SetDrivenDangerouslyExtremeThisUpdate();
		}
	}
	else
	{
		// Add mad driver event if the vehicle is skidding
		f32 fwdSlipAngle = 0.f;
		const f32 numTyresScalar = 1.f / (f32)m_pTargetVehicle->GetNumWheels();
		bool bBurnOut = false;
		bool bOffRoad = false;

		naCErrorf(m_pTargetVehicle->GetNumWheels()<=NUM_VEH_CWHEELS_MAX, "Vehicle has too many wheels");
		for(s32 i = 0 ; i < m_pTargetVehicle->GetNumWheels(); i++)
		{
			if(m_pTargetVehicle->GetWheel(i)->GetIsTouching())
			{
				const f32 healthScalar = m_pTargetVehicle->GetWheel(i)->GetTyreHealth() * TYRE_HEALTH_DEFAULT_INV;
				fwdSlipAngle += m_pTargetVehicle->GetWheel(i)->GetEffectiveSlipAngle() * healthScalar * numTyresScalar;
				if(m_pTargetVehicle->GetWheel(i)->GetDynamicFlags().IsFlagSet(WF_BURNOUT))
				{
					bBurnOut = true;
				}
				if (m_pTargetVehicle->GetWheel(i)->GetMaterialGrip() < 0.99f)
				{
					bOffRoad = true;
				}
			}
		}
		if (!bBurnOut && !bOffRoad && fwdSlipAngle > 2.5f && (pPed->IsPlayer() || (!m_pTargetVehicle->GetBrake() && !m_pTargetVehicle->GetHandBrake())) )
		{
			m_pTargetVehicle->SetDrivenDangerouslyThisUpdate(true);
		}
	}

	//Process the siren.
	ProcessSiren();

	//if the vehicle is superdummy, allow occupants to update
	if (m_pTargetVehicle->GetVehicleAiLod().GetDummyMode() == VDM_DUMMY
		|| m_pTargetVehicle->GetVehicleAiLod().GetDummyMode() == VDM_SUPERDUMMY
		|| m_pTargetVehicle->ShouldFixIfNoCollisionLoadedAroundPosition())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded, true);
	}

	//Check if the ped is not a player.
	if(!pPed->IsPlayer())
	{
		//Check shocking events.
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	}

	//Cache motion task
	if (m_pTargetVehicle->InheritsFromAutomobile())
	{
		if (!m_pMotionInAutomobileTask)
		{
			m_pMotionInAutomobileTask = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE);
		}
	}

	const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	if (pSeatClipInfo && pDrivebyInfo)
	{
		bool bIsWeaponInLeftHand = pSeatClipInfo->GetWeaponAttachedToLeftHand();
		if (pDrivebyInfo->GetWeaponAttachedToLeftHand1HOnly())
		{
			CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
			const CWeaponInfo* pWeaponInfo = pWeaponMgr ? pWeaponMgr->GetEquippedWeaponInfo() : NULL;
			bIsWeaponInLeftHand = pWeaponInfo ? !pWeaponInfo->GetIsGun2Handed() : false;
		}
		else
		{
			bIsWeaponInLeftHand = bIsWeaponInLeftHand FPS_MODE_SUPPORTED_ONLY(|| ((pPed->IsInFirstPersonVehicleCamera() || pPed->IsFirstPersonShooterModeEnabledForPlayer(false)) && pDrivebyInfo->GetLeftHandedFirstPersonAnims()));
		}

		bool bIsProp = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHash() == OBJECTTYPE_OBJECT;
		CPedEquippedWeapon::eAttachPoint attachPoint = bIsWeaponInLeftHand && !bIsProp ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand;
		
		if (pPed->GetWeaponManager() && ((pSeatClipInfo->GetWeaponRemainsVisible() && (pPed->GetWeaponManager()->GetRequiresWeaponSwitch() || pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() != attachPoint))))
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject(attachPoint);
		}
	}

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::CleanUp()
{
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_AttachedToVehicle, false);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_StreamAssets)
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_AttachedToVehicle)
			FSM_OnEnter
				AttachedToVehicle_OnEnter();
			FSM_OnUpdate
				return AttachedToVehicle_OnUpdate();
			FSM_OnExit
				return AttachedToVehicle_OnExit();

		FSM_State(State_PlayingAmbients)
			FSM_OnEnter
				PlayingAmbients_OnEnter();
			FSM_OnUpdate
				return PlayingAmbients_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnUpdate
				return Idle_OnUpdate();
			
		FSM_State(State_UseWeapon)		// Handled by vehicle driveby?
			FSM_OnEnter
				UseWeapon_OnEnter();
			FSM_OnUpdate
				return UseWeapon_OnUpdate();
				
		FSM_State(State_DriveBy)
			FSM_OnEnter
				DriveBy_OnEnter();
			FSM_OnUpdate
				return DriveBy_OnUpdate();
			FSM_OnExit
				DriveBy_OnExit();
				
		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::Start_OnUpdate()
{
	// Setup the conditional anims group we'll use for any AmbientClips subtasks
	SetConditionalAnimsGroup();

	CPed* pPed = GetPed();

	//This ped can not drive this vehicle.
	if (m_bSetPedOutOfVehicle)
	{
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	s32 iSeat = m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	aiAssertf(iSeat != -1, "%s ped %s wasn't in a vehicle, parent task type / state %s / %s", pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName(), GetParent() ? GetParent()->GetTaskName() : "NULL", GetParent() ? GetParent()->GetStateName(GetParent()->GetState()) : "NULL");
	aiAssert(m_pTargetVehicle->GetVehicleModelInfo());
	const CVehicleSeatInfo* pSeatInfo = m_pTargetVehicle->GetVehicleModelInfo()->GetSeatInfo(iSeat);
	if (!pPed->IsAPlayerPed() && taskVerifyf(pSeatInfo, "NULL seat info for vehicle %s", m_pTargetVehicle->GetVehicleModelInfo()->GetGameName()) && pSeatInfo->GetDefaultCarTask() == CVehicleSeatInfo::TASK_HANGING_ON_VEHICLE)
	{ //no players! They can't use weapons and hang without detaching, if this becomes necessary this task will need a rework
		m_bCanAttachToVehicle = true;
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_GRAB))
		{
			SetState(State_StreamAssets);
			return FSM_Continue;
		}
	}

	if (m_pAnims && !m_uInVehicleBasicFlags.IsFlagSet(IVF_DisableAmbients))
	{
		SetState(State_PlayingAmbients);
	}
	else
	{
		SetState(State_Idle);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::StreamAssets_OnUpdate()
{
	if (m_pTargetVehicle)
	{
		// Insure the clipset is streamed in
		const s32 iSeatIndex = m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
		if (StreamTaskAssets(m_pTargetVehicle, iSeatIndex))
		{
			SetState(State_AttachedToVehicle);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::AttachedToVehicle_OnEnter()
{
	s32 iSeat = m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	if (taskVerifyf(iSeat != -1, "Invalid seat index"))
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pTargetVehicle->GetSeatAnimationInfo(iSeat);
		if (taskVerifyf(pSeatClipInfo && (pSeatClipInfo->GetSeatClipSetId().GetHash() != 0), "Expected non null seat clip info or clip set"))
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pSeatClipInfo->GetSeatClipSetId());
			if (pClipSet)
			{
				const s32 clipDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash()).Get();
				const s32 iSeatBoneIndex = m_pTargetVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeat);
				const u32 sIdleHash = ATSTRINGHASH("sit", 0x05577ab18);
				u32 iIdleHash = sIdleHash;
				SetNewTask(rage_new CTaskAnimatedAttach(m_pTargetVehicle, iSeatBoneIndex, pSeatClipInfo->GetNMConstraintFlags(), clipDictIndex, iIdleHash));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::AttachedToVehicle_OnUpdate()
{
	// Switch back to animated idles
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_pAnims)
		{
			SetState(State_PlayingAmbients);
		}
		else
		{
			SetState(State_Idle);
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::AttachedToVehicle_OnExit()
{
	CPed* pPed = GetPed();

	nmTaskDebugf(this, "CTaskInVehicleBasic::AttachedToVehicle_OnExit switching to animated");
	pPed->SwitchToAnimated();

	if (!GetIsFlagSet(aiTaskFlags::InMakeAbortable))
	{
		// Reattach the ped back to the vehicle
		if (m_pTargetVehicle)
		{
			s32 iSeat = m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (iSeat != -1)
			{
				// If we're not already attached, we must be warping the ped into the vehicle
				pPed->AttachPedToEnterCar(m_pTargetVehicle, ATTACH_STATE_PED_IN_CAR, iSeat, m_pTargetVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, m_pTargetVehicle));
			}
		}
	}
	else
	{
		//pPed->SetPedOutOfVehicle(); need to be able to use drivebys from this state, then fall back to attached state.  SetPedOutOfVehicle happens when leaving the vehicle.
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::PlayingAmbients_OnEnter()
{
	if (aiVerify(m_pAnims))
	{
		// Don't play base clip as this overrides the in vehicle anims (base anims probably should be removed)
		u32 ambientClipsFlags = CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PickPermanentProp;
		if (m_pTargetVehicle->GetDriver() != GetPed())
		{
			ambientClipsFlags |= CTaskAmbientClips::Flag_PlayBaseClip;
		}
		SetNewTask(rage_new CTaskAmbientClips(ambientClipsFlags, m_pAnims));
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::PlayingAmbients_OnUpdate()
{
	CPed* pPed = GetPed();
	// Switch to nm driven idle if allowed
	if (m_bCanAttachToVehicle && !pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodRagdollInVehicle))
	{
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_GRAB) && pPed->GetIsVisibleInSomeViewportThisFrame())
		{
			SetState(State_StreamAssets);
			return FSM_Continue;
		}
	}

	if (!CheckForPlayingAmbients())
	{
		SetState(State_Idle);
		return FSM_Continue;
	}

	//This ped can not drive this vehicle.
	if (m_bSetPedOutOfVehicle)
	{
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	if (CheckForTrainExit())
	{
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	// Verify the vehicle's mounted weapon task is valid
	if(CTaskVehicleMountedWeapon::IsTaskValid(pPed))
	{
		SetState(State_UseWeapon);
		return FSM_Continue;
	}
	
	//Check if we should drive by.
	if(ShouldDriveBy())
	{
		SetState(State_DriveBy);
		return FSM_Continue;
	}

	AllowTimeslicing(pPed);

	if (m_pTargetVehicle && !m_pTargetVehicle->IsDriver(pPed) && m_pTargetVehicle->GetDriver() && m_pTargetVehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BUSTED))
	{
		// If the driver is being arrested, try to look at him.
		const u32 uHashKey = 0;
		const u32 uLookAtFlags = LF_WHILE_NOT_IN_FOV;
		const s32 iInfiniteTime = -1;
		const s32 iBlendInTime	= 500;
		const s32 iBlendOutTime = 500;
		const Vector3* vOffset = NULL;
		pPed->GetIkManager().LookAt(uHashKey, m_pTargetVehicle->GetDriver(), iInfiniteTime, BONETAG_HEAD, vOffset, uLookAtFlags, iBlendInTime, iBlendOutTime, CIkManager::IK_LOOKAT_MEDIUM);
	}
	
	// Restart our child AmbientClipsTask if it bailed and we have a valid conditional anims group
	if (aiVerify(m_pAnims))
	{
		CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (!pAmbientTask)
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::Idle_OnUpdate()
{
	if (CheckForPlayingAmbients() && GetTimeInState() > 2.0f)
	{
		SetState(State_PlayingAmbients);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Verify the vehicle's mounted weapon task is valid
	if(CTaskVehicleMountedWeapon::IsTaskValid(pPed))
	{
		SetState(State_UseWeapon);
		return FSM_Continue;
	}

	AllowTimeslicing(pPed);

#if FPS_MODE_SUPPORTED
	// The passenger will return the look when you aim the 1st person camera at them.
	// It uses a timer so it does not turn into an awkward staring contest. We tuned it so they will look back at you for 5 to 7 seconds then look away for a duration between 2.5 and 4 seconds.
	// We also reset the timer if you look away and look back before they have returned to neutral (to stop yo-yoing).
	// This logic only runs when the conversation look-ats are not active as it looked too erratic with the conflicting priorities. 
	TUNE_GROUP_BOOL(IN_CAR_LOOK_AT, bEnableLookBackAtPlayer, true);
	if(bEnableLookBackAtPlayer && pPed->IsLocalPlayer() && pPed->GetMyVehicle()->GetNumberOfPassenger() > 0 && camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera())
	{
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookBackAtPlayerHoldTimeMin, 5000, 0, 10000, 10);
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookBackAtPlayerHoldTimeMax, 7000, 0, 10000, 10);
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookBackAtPlayeyWaitTimeMin, 2500, 0, 10000, 10);
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookBackAtPlayerWaitTimeMax, 4000, 0, 10000, 10);
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, sBlendInTime, 750, 0, 10000, 10);
		TUNE_GROUP_INT(IN_CAR_LOOK_AT, sBlendOutTime, 500, 0, 10000, 10);
		static const u32 uLookAtHash = ATSTRINGHASH("LookBackAtPlayerInVehicle", 0xAEEECB25);

		const camCinematicMountedCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
		if(pCamera)
		{
			Vector3 vCameraFront = pCamera->GetFrame().GetFront();
			Vec3V vHeadPos;
			pPed->GetBonePositionVec3V(vHeadPos, BONETAG_HEAD);
			for(int iSeat=0; iSeat<pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); iSeat++)
			{
				CPed* pPassenger = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(iSeat);
				if(pPassenger && pPassenger != pPed && CTaskVehicleFSM::CanPassengerLookAtPlayer(pPassenger)
					&& pPed->GetSpeakerListenedTo() != pPassenger && pPed->GetSpeakerListenedToSecondary() != pPassenger && !pPassenger->GetSpeakerListenedTo() && !pPassenger->GetSpeakerListenedToSecondary())
				{
					Vec3V vPassengerHead;
					pPassenger->GetBonePositionVec3V(vPassengerHead, BONETAG_HEAD);
					Vec3V vCameraToPassengerHead = vPassengerHead - VECTOR3_TO_VEC3V(pCamera->GetFrame().GetPosition());
					vCameraToPassengerHead = Normalize(vCameraToPassengerHead);

					TUNE_GROUP_FLOAT(IN_CAR_LOOK_AT, fLookAtAngleInDegree, 13.0f, 0.0f, 90.0f, 1.0f);
					ScalarV lookAtAngle(rage::Cosf(DtoR* fLookAtAngleInDegree));
					if(IsGreaterThanOrEqualAll(Dot(VECTOR3_TO_VEC3V(vCameraFront), vCameraToPassengerHead), lookAtAngle))
					{
						if(m_pPassengerPlayerIsLookingAt != pPassenger)
						{
							m_uPassengerLookingEndTime = 0;
							m_pPassengerPlayerIsLookingAt = pPassenger;
						}

						// wait the player enter car look at finished before looking back again. Fix eyes shifting if this look at happens right after the enter car look at is finished.
						static const u32 uLookAtEnterExitVehicleHash = ATSTRINGHASH("LookAtPlayerEnterOrExitVehicle", 0xAC803D8);
						if(pPassenger->GetIkManager().IsLooking(uLookAtEnterExitVehicleHash))
						{
							m_uPassengerLookingEndTime = fwTimer::GetTimeInMilliseconds() + sBlendOutTime;
						}

						if(fwTimer::GetTimeInMilliseconds() > m_uPassengerLookingEndTime && (!pPassenger->GetIkManager().IsLooking(uLookAtHash) || m_uPassengerLookingEndTime == 0))
						{
							u32 lookAtFlags = LF_FROM_SCRIPT | LF_WHILE_NOT_IN_FOV;
							lookAtFlags |= LF_WIDEST_YAW_LIMIT;
							if(fwRandom::GetRandomTrueFalse())
							{
								// Random between fast and normal
								if(fwRandom::GetRandomTrueFalse())
								{
									lookAtFlags |= LF_FAST_TURN_RATE;
								}
							}
							else
							{
								// Random between slow and normal
								if(fwRandom::GetRandomTrueFalse())
								{
									lookAtFlags |= LF_SLOW_TURN_RATE;
								}
							}

							// Apply camera offset for first person camera.
							Matrix34 matCamera;
							pCamera->GetObjectSpaceCameraMatrix(pPed, matCamera);
							const Vector3 vCameraOffset = matCamera.d;
							s32 uHoldTime = fwRandom::GetRandomNumberInRange(uLookBackAtPlayerHoldTimeMin, uLookBackAtPlayerHoldTimeMax);
							pPassenger->GetIkManager().LookAt(uLookAtHash, pPed, uHoldTime, BONETAG_INVALID, &vCameraOffset, lookAtFlags, sBlendInTime, sBlendOutTime, CIkManager::IK_LOOKAT_LOW);
							s32 uWaitTime = fwRandom::GetRandomNumberInRange(uLookBackAtPlayeyWaitTimeMin, uLookBackAtPlayerWaitTimeMax);
							m_uPassengerLookingEndTime = fwTimer::GetTimeInMilliseconds() + uHoldTime + uWaitTime + sBlendOutTime;
						}
					}
					else if(m_pPassengerPlayerIsLookingAt == pPassenger)
					{
						m_uPassengerLookingEndTime = 0;
						m_pPassengerPlayerIsLookingAt = NULL;
					}
				}
			}
		}


	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::UseWeapon_OnEnter()
{
	CPed* pPed = GetPed();
	CTaskVehicleMountedWeapon::eTaskMode mode = pPed->IsLocalPlayer() ? CTaskVehicleMountedWeapon::Mode_Player : CTaskVehicleMountedWeapon::Mode_Idle;
	SetNewTask(rage_new CTaskVehicleMountedWeapon(mode));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::UseWeapon_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_pAnims)
		{
			SetState(State_PlayingAmbients);
		}
		else
		{
			SetState(State_Idle);
		}
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	const bool bSeatHasWeapons = m_pTargetVehicle->GetSeatHasWeaponsOrTurrets(m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(pPed));
	if(!bSeatHasWeapons && CheckForPlayingAmbients())
	{
		SetState(State_PlayingAmbients);
		return FSM_Continue;
	}

	if(!pPed->IsLocalPlayer())
	{
		// Only do this for certain vehicle types 
		if(m_pTargetVehicle && m_pTargetVehicle->InheritsFromHeli())
		{
			CPedAILod& lod = pPed->GetPedAiLod();
			lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
			lod.SetUnconditionalTimesliceIntelligenceUpdate(true);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::DriveBy_OnEnter()
{
	//Assert that the drive-by was requested.
	taskAssertf(m_DriveBy.m_bRequested, "The drive-by was not requested.");
	
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleGun((CTaskVehicleGun::eTaskMode)m_DriveBy.m_iMode, 0, &m_DriveBy.m_Target);
	
	//Start the task.
	SetNewTask(pTask);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::DriveBy_OnUpdate()
{
	//Check if the time has expired.
	if(GetTimeInState() > m_DriveBy.m_fTime)
	{
		//Check if the sub-task is valid.
		CTask* pTask = GetSubTask();
		if(pTask)
		{
			//Assert that the task supports termination requests.
			taskAssert(pTask->SupportsTerminationRequest());

			//Request termination.
			pTask->RequestTermination();
		}
	}
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move back to the ambients state.
		if (m_pAnims)
		{
			SetState(State_PlayingAmbients);
		}
		else
		{
			SetState(State_Idle);
		}
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::DriveBy_OnExit()
{
	//Clear the requested flag.
	m_DriveBy.m_bRequested = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::ExitVehicle_OnEnter()
{
	SetNewTask(rage_new CTaskExitVehicle(m_pTargetVehicle, m_iVehicleEnterExitFlags));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleBasic::ExitVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::TriggerIK(CPed* UNUSED_PARAM(pPed))
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::AbortIK(CPed* pPed)
{
	if (pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().AbortLookAt(250);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::RequestDriveBy(float fTime, int iMode, const CAITarget& rTarget)
{
	m_DriveBy.m_bRequested = true;
	m_DriveBy.m_fTime = fTime;
	m_DriveBy.m_iMode = iMode;
	m_DriveBy.m_Target = rTarget;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::RequestSiren(float fTime)
{
	m_Siren.m_bRequested = true;
	m_Siren.m_fTime = fTime;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleBasic::CheckForPlayingAmbients()
{
	if (!m_pAnims)
	{
		return false;
	}

	if(m_uInVehicleBasicFlags.IsFlagSet(IVF_DisableAmbients))
	{
		return false;
	}

	if (!m_pMotionInAutomobileTask)
		return true;
	
	const s32 iMotionTaskState = m_pMotionInAutomobileTask->GetState();
	if (iMotionTaskState == CTaskMotionInAutomobile::State_CloseDoor)
		return false;

	if(m_pMotionInAutomobileTask->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
	{
		const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(m_pMotionInAutomobileTask.Get());
		if(pAutoMobileTask->IsUsingFirstPersonSteeringAnims())
		{
			CTaskAmbientClips* pAmbientClipTask = static_cast<CTaskAmbientClips *>(GetPed()->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AMBIENT_CLIPS ) );
			if(pAmbientClipTask)
			{
				pAmbientClipTask->SetCleanupBlendOutDelta( INSTANT_BLEND_IN_DELTA );
			}

			return false;
		}
	}

	const CPed& rPed = *GetPed();
	return !CTaskMotionInVehicle::CheckForClosingDoor(*m_pTargetVehicle, rPed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleBasic::CheckForTrainExit()
{
	CPed* pPed = GetPed();

	// Get off train at stations
	if( m_pTargetVehicle->InheritsFromTrain()  )
	{
		// Block weapon switching while driving a train
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );

		CTrain* pTrain = static_cast<CTrain*>(m_pTargetVehicle.Get());
		if( pPed->PopTypeIsRandom() && !pTrain->IsDriver(pPed) && pTrain->m_nTrainFlags.bAtStation )
		{
			if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_JustGotOnTrain ) )
			{
				if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
				{
					if (pTrain->m_nTrainFlags.iStationPlatformSides & CTrainTrack::Right)
						m_iVehicleEnterExitFlags.BitSet().Set(CVehicleEnterExitFlags::PreferRightEntry);
					else
						m_iVehicleEnterExitFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_JustGotOffTrain, true );
					return true;
				}
			}
		}
		else
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_JustGotOnTrain, false );
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::ProcessSiren()
{
	//Check if the siren has been requested.
	if(m_Siren.m_bRequested)
	{
		//Clear the flag.
		m_Siren.m_bRequested = false;

		//Turn on the siren.
		m_pTargetVehicle->TurnSirenOn(true, false);
	}
	//Check if the time is valid.
	else if(m_Siren.m_fTime > 0.0f)
	{
		//Decrease the time.
		m_Siren.m_fTime -= GetTimeStep();
		if(m_Siren.m_fTime <= 0.0f)
		{
			m_pTargetVehicle->TurnSirenOn(false, false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleBasic::ShouldDriveBy() const
{
	return m_DriveBy.m_bRequested;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleBasic::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//CPed *pPed = GetPed(); //Get the ped ptr.

	if( ABORT_PRIORITY_IMMEDIATE==iPriority && ( !pEvent || ((CEvent*)pEvent)->GetEventType() != EVENT_NEW_TASK ) )
	{
		return true;
	}

	// Don't abort for impact reaction tasks until finished being bumped
// 	if( pEvent )
// 	{
// 		if( pPed->GetPedIntelligence()->GetEventResponseTask() && 
// 			pPed->GetPedIntelligence()->GetEventResponseTask()->GetTaskType() == CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION &&
// 			pPed->GetPedIntelligence()->GetPhysicalEventResponseTask() == NULL )
// 		{
// 			if( GetState() == State_BeingBumped )
// 			{
// 				return false;
// 			}
// 		}
// 	}

	if(!GetSubTask() || GetSubTask()->MakeAbortable( iPriority, pEvent) )
	{
		StopClip();
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if( ABORT_PRIORITY_IMMEDIATE==iPriority && ( !pEvent || ((CEvent*)pEvent)->GetEventType() != EVENT_NEW_TASK ) )
	{
		//That should be everything tidied up.
		AbortIK(pPed); 

		StopClip();
	}
	else
	{
		StopClip();
	}

	if (GetState() == State_AttachedToVehicle && pEvent && static_cast<const CEvent*>(pEvent)->GetEventType() == EVENT_DAMAGE)
	{
		pPed->SetPedOutOfVehicle(CPed::PVF_DontResetDefaultTasks);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleBasic::CheckForUndrivableVehicle( CPed* pPed )
{
	CPedGroup* pPedGroup=pPed->GetPedsGroup();
	bool bCarUpsideDown = CUpsideDownCarCheck::IsCarUpsideDown(m_pTargetVehicle.Get());
	bool bIsDrowning = m_pTargetVehicle->m_nVehicleFlags.bIsDrowning;

	CPed* pDriver = m_pTargetVehicle->GetDriver();
	if(!pDriver)
	{
		pDriver = m_pTargetVehicle->GetSeatManager()->GetLastPedInSeat(Seat_driver);
	}

	bool bDriverIsInVehicle = pDriver && pDriver->GetIsInVehicle();

	bool bDriverDead = pDriver && pDriver->IsInjured();
	bool bDriverDeadRandomChar = pPed->PopTypeIsRandom() && bDriverDead;

	float fMaxDriverDistanceSq = (pDriver && (pDriver->IsLawEnforcementPed() || pDriver->GetPedType() == PEDTYPE_FIRE || pDriver->GetPedType() == PEDTYPE_MEDIC)) ? square(35.0f) : square(15.0f);
	bool bDriverFar = !bDriverDead && !bDriverIsInVehicle && pDriver &&
		(IsGreaterThanAll(DistSquared(pPed->GetTransform().GetPosition(), pDriver->GetTransform().GetPosition()), ScalarVFromF32(fMaxDriverDistanceSq)));
	bool bDriverFarRandomChar = pPed->PopTypeIsRandom() && bDriverFar;

	const bool bOnFullVehicleRecording = CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_pTargetVehicle) && !CVehicleRecordingMgr::IsPlaybackSwitchedToAiForCar(m_pTargetVehicle);
	if( !bOnFullVehicleRecording
		&&
		(bDriverDeadRandomChar ||
		bDriverFarRandomChar ||
		bCarUpsideDown ||
		bIsDrowning ||
		(!m_pTargetVehicle->m_nVehicleFlags.bIsThisADriveableCar )) )

	{
		m_nUpsideDownCounter += (u32)GetTimeStepInMilliseconds();
		const u32 TimeUntilEvent = bIsDrowning ? 100 : 2000; // If we are drowning, get the fuck out quickly!
		if(m_nUpsideDownCounter > TimeUntilEvent)
		{
			m_nUpsideDownCounter = 0;

			// We are not stupid, we don't want to drown even if the leader is a retard and stays in the vehicle that is drowning
			// We will also exit the vehicle if the leader is no longer in it (might happen when ped is not "following" the leader)
			if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle ) &&
				(!pPedGroup || bIsDrowning || pPedGroup->GetGroupMembership()->IsLeader(pPed) || !m_pTargetVehicle->ContainsPed(pPedGroup->GetGroupMembership()->GetLeader())) )
			{
				CEventCarUndriveable event(m_pTargetVehicle);
				pPed->GetPedIntelligence()->AddEvent(event);
			}		
		}
	}
	else
	{
		m_nUpsideDownCounter = 0;
	}

	// Check for beached boat case
	if (!bOnFullVehicleRecording && CBoatIsBeachedCheck::IsBoatBeached(*m_pTargetVehicle))
	{
		CEventCarUndriveable event(m_pTargetVehicle);
		pPed->GetPedIntelligence()->AddEvent(event);	
	}
	// otherwise, check if we are not playing back a recording and will get out of undriveable vehicles
	else if( !bOnFullVehicleRecording && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle ) )
	{
		// If vehicle is stuck and we should flee
		if( CEventCarUndriveable::ShouldPedUseFleeResponseDueToStuck(pPed, m_pTargetVehicle) )
		{
			CEventCarUndriveable event(m_pTargetVehicle);
			pPed->GetPedIntelligence()->AddEvent(event);
		}
	}

	Assert(pPed->GetMyVehicle()->GetVehicleDamage());
	// Also generate some audio if the car is on fire
	float fEngineHealth = pPed->GetMyVehicle()->GetVehicleDamage()->GetEngineHealth();
	float fPetrolTankHealth = pPed->GetMyVehicle()->GetVehicleDamage()->GetPetrolTankHealth();
	bool bIsVehicleOnFire = false;
	if( m_pTargetVehicle->GetVehicleType() != VEHICLE_TYPE_HELI && m_pTargetVehicle->GetVehicleType() != VEHICLE_TYPE_PLANE &&
				!m_pTargetVehicle->IsDriver(pPed) )
	{
		if ((fEngineHealth < ENGINE_DAMAGE_ONFIRE && fEngineHealth > ENGINE_DAMAGE_FINSHED) ||
			(fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_FINISHED && fPetrolTankHealth < VEH_DAMAGE_HEALTH_PTANK_ONFIRE))
		{
			bIsVehicleOnFire = true;
		}

		if(bIsVehicleOnFire)
		{
			if (!m_bWasVehicleOnFire)
			{
				// This context is wrong, but we shipped with the code like this and it seems risky to change it 4 years later,
				// so restricting the fix to multiplayer only!
				//pPed->RandomlySay("CAR_ON_FIRE", 0.5f);
				
				if (CNetwork::IsGameInProgress())
				{
					// Add a bit of a timeout to this to prevent all passengers potentially saying the line simultaneously
					if (fwTimer::GetTimeInMilliseconds() - sm_LastVehicleFireSpeechTime > 2000)
					{
						if (pPed->RandomlySay("VEHICLE_ON_FIRE", 0.5f))
						{
							sm_LastVehicleFireSpeechTime = fwTimer::GetTimeInMilliseconds();
						}
					}
				}
			}
		}
		else if( bCarUpsideDown && !m_bWasVehicleUpsideDown && m_pTargetVehicle->GetTransform().GetC().GetZf() < 0.0f )
		{
			// This context is wrong, but we shipped with the code like this and it seems risky to change it 4 years later,
			// so restricting the fix to multiplayer only!
			//pPed->RandomlySay("CAR_FLIPPED", 0.5f);

			// Add a bit of a timeout to this to prevent all passengers potentially saying the line simultaneously
			if (CNetwork::IsGameInProgress() && fwTimer::GetTimeInMilliseconds() - sm_LastVehicleFlippedSpeechTime > 2000)
			{
				if (pPed->RandomlySay("VEHICLE_FLIPPED", 0.5f))
				{
					sm_LastVehicleFlippedSpeechTime = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
	}

	m_bWasVehicleOnFire = bIsVehicleOnFire;
	m_bWasVehicleUpsideDown = bCarUpsideDown;
}


void CTaskInVehicleBasic::UpdateCommentOnVehicle(CPed* pPed)
{
	//Disabled for bikes
	if( m_pTargetVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return;
	}

	//Disabled for law enforement, fire trucks aren't classed as special cars and aren't in the inappropriate list
	if ( m_pTargetVehicle->IsLawEnforcementVehicle())
	{
		return;
	}

	//Nice vehicles will always be commented on where ever they are.
	CVehicleModelInfo* pVehicleInfo = m_pTargetVehicle->GetVehicleModelInfo();
	if( pVehicleInfo && pVehicleInfo->GetVehicleSwankness() > SWANKNESS_3)
	{
		// If the vehicle is traveling slowly decrease the time down to the enable the event to be always on 
		if(pPed->GetMyVehicle()->GetVelocity().Mag2() < NICE_VEHICLE_SLOW_SPEED_SQ_THRESHOLD )
		{ 
			if (!m_broadcaseNiceCarTimer.IsSet() || (m_broadcaseNiceCarTimer.IsSet() && m_broadcaseNiceCarTimer.GetTimeLeft() > NICE_VEHICLE_SLOW_SPEED_EVENT_TIMER ) )
			{
				m_broadcaseNiceCarTimer.Set(fwTimer::GetTimeInMilliseconds(), 1);
			}
		}
		else
		{
			if (!m_broadcaseNiceCarTimer.IsSet() )
			{
				m_broadcaseNiceCarTimer.Set(fwTimer::GetTimeInMilliseconds(),fwRandom::GetRandomNumberInRange(NICE_VEHICLE_EVENT_TIMER_MIN,NICE_VEHICLE_EVENT_TIMER_MAX));
			}
		}
	}

	//Any vehicle that shouldn't be in this current area should be comment on.
	if (gPopStreaming.GetInAppropriateLoadedCars().IsMemberInGroup(m_pTargetVehicle->GetModelIndex()))
	{
		//only add the event if the vehicle's frequency is low, this should also stop area border problems.
		if(m_pTargetVehicle->GetVehicleModelInfo()->GetVehicleFreq() < 40 || (pVehicleInfo && pVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_INCREASE_PED_COMMENTS)))
		{
			if (!m_broadcaseNiceCarTimer.IsSet() || (m_broadcaseNiceCarTimer.IsSet() && m_broadcaseNiceCarTimer.GetTimeLeft() > VEHICLE_INAPPROPRIATE_AREA_EVENT_TIMER ) )
			{
				m_broadcaseNiceCarTimer.Set(fwTimer::GetTimeInMilliseconds(), VEHICLE_INAPPROPRIATE_AREA_EVENT_TIMER);
			}
		}
	}

	// Add shocking event randomly so peds can comment
	if(m_broadcaseNiceCarTimer.IsOutOfTime())
	{
		//Check the vehicle is still valid to be commented on, it may now be
		bool StillValid = false;
		CVehicleModelInfo* pVehicleInfo = m_pTargetVehicle->GetVehicleModelInfo();
		if( pVehicleInfo )
		{
			if (pVehicleInfo->GetVehicleSwankness() > SWANKNESS_3)
			{
				StillValid = true;
			}

			s32 vehicleFreq = pVehicleInfo->GetVehicleFreq();
			if ((gPopStreaming.GetInAppropriateLoadedCars().IsMemberInGroup(m_pTargetVehicle->GetModelIndex()) && vehicleFreq < 40) ||
				vehicleFreq < 15 )
			{
				StillValid = true;
			}
		}

		//Reset the timer even if the vehicle isn't valid to be commented on anylonger.
		m_broadcaseNiceCarTimer.Unset();

		if (StillValid)
		{
			//Comment
			m_pTargetVehicle->AddCommentOnVehicleEvent();
		}
	}
}

void CTaskInVehicleBasic::SetConditionalAnimsGroup()
{
	// Use the scenario specified anim group if possible, otherwise check the seat. 
	if (!m_pAnims)
	{
		atHashWithStringNotFinal ambientContext("EMPTY",0xBBD57BED);
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pTargetVehicle->GetSeatAnimationInfo(GetPed());
		if (pSeatClipInfo && pSeatClipInfo->GetSeatAmbientContext().GetHash() != 0)
		{
			ambientContext = pSeatClipInfo->GetSeatAmbientContext();
		}
		if (ambientContext.GetHash() != 0)
		{
			m_pAnims = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(ambientContext.GetHash());
		}
	}
}

void CTaskInVehicleBasic::AllowTimeslicing(CPed* pPed)
{
	if(!pPed->IsLocalPlayer())
	{
		// Enable AI timeslicing
		CPedAILod& lod = pPed->GetPedAiLod();
		lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
		if(!pPed->PopTypeIsMission())
		{
			lod.SetUnconditionalTimesliceIntelligenceUpdate(true);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////

///////////////////
//COMPLEX DRIVE 
///////////////////


CTaskCarDrive::CTaskCarDrive(CVehicle* pVehicle, const float CruiseSpeed, const s32 iDrivingFlags, const bool bUseExistingNodes)
 : m_pVehicle(pVehicle),
   m_bAsDriver(true),
   m_pVehicleTask(NULL),
   m_pActualVehicleTask(NULL),
   m_bDriveByRequested(false),
   m_nTimeLastDriveByRequested(0),
   m_bSetPedOuOfVehicle(false)
//   m_bIsCarSetUp(false)
{
	bool bNetVehicleValid = IsNetworkVehicleValid(pVehicle);
	if(bNetVehicleValid &&  ( pVehicle->InheritsFromHeli() || (pVehicle->InheritsFromPlane() && static_cast<CPlane*>(pVehicle)->GetVerticalFlightModeAvaliable()) ) )//heli's don't cruise so just hover
	{
		if ( pVehicle->GetDriver() && pVehicle->GetDriver()->PopTypeIsMission() )
		{
			m_pVehicleTask = rage_new CTaskVehicleHover();
		}
		else
		{
			sVehicleMissionParams params;
			params.m_fCruiseSpeed = Max(40.0f, pVehicle->pHandling->m_fEstimatedMaxFlatVel * 0.9f);
			params.SetTargetPosition(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
			params.m_fTargetArriveDist = -1.0f;
			params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);
			int iFlightHeight = CTaskVehicleFleeAirborne::ComputeFlightHeightAboveTerrain(*pVehicle, 50);	
			m_pVehicleTask = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iFlightHeight, false, 0);
		}
	}
	else
	{
		sVehicleMissionParams params;
		params.m_iDrivingFlags = iDrivingFlags;
		params.m_fCruiseSpeed = CruiseSpeed;
		m_pVehicleTask =CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);
	}

	m_bUseExistingNodes = bUseExistingNodes;

	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE);
}

CTaskCarDrive::CTaskCarDrive(CVehicle* pVehicle)
 : m_pVehicle(pVehicle),
   m_bAsDriver(true),
   m_pVehicleTask(NULL),
   m_bDriveByRequested(false),
   m_nTimeLastDriveByRequested(0),
   m_bSetPedOuOfVehicle(false)
//   m_bIsCarSetUp(false)
{
	bool bNetVehicleValid = IsNetworkVehicleValid(pVehicle);
	if(bNetVehicleValid &&  ( pVehicle->InheritsFromHeli() || (pVehicle->InheritsFromPlane() && static_cast<CPlane*>(pVehicle)->GetVerticalFlightModeAvaliable()) ) )//heli's don't cruise so just hover
	{
		m_pVehicleTask = rage_new CTaskVehicleHover();
	}
	else
	{
		sVehicleMissionParams params;
		params.m_fCruiseSpeed = 0.0f;
		m_pVehicleTask = CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);
	}
	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE);
}


CTaskCarDrive::CTaskCarDrive(CVehicle* pVehicle, aiTask *pVehicleTask, const bool bUseExistingNodes)
:	m_pVehicle(pVehicle),
	m_bAsDriver(true),
   m_pVehicleTask(NULL),
   m_bDriveByRequested(false),
   m_nTimeLastDriveByRequested(0),
   m_bSetPedOuOfVehicle(false)
{

	m_pVehicleTask = pVehicleTask;

	m_bUseExistingNodes = bUseExistingNodes;

	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE);
}

CTaskCarDrive::~CTaskCarDrive()
{
	if(m_pVehicle)
	{
//		if(m_bIsCarSetUp)
//		{
//			// restore original autopilot settings for this vehicle
//			m_pVehicle->GetIntelligence()->SetDrivingMode(m_origDrivingMode);
//			m_pVehicle->GetIntelligence()->SetMission(m_origMission);
//		    m_pVehicle->GetIntelligence()->SetCruiseSpeed(m_origCruiseSpeed);
//		}
	}

	if(m_pVehicleTask)
		delete m_pVehicleTask;
	//SParr we don't need to delete m_pActualVehicleTask as it's the task that's active in the task tree
	//when the task finishes the task tree will delete it for us
}

//
//
//
CTask::FSM_Return CTaskCarDrive::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	FSM_State(State_CarDrive)
		FSM_OnEnter
			return CarDrive_OnEnter(pPed);
		FSM_OnUpdate
			return CarDrive_OnUpdate(pPed);

	FSM_State(State_EnterVehicle)
		FSM_OnEnter
			return EnterVehicle_OnEnter(pPed);
		FSM_OnUpdate
			return EnterVehicle_OnUpdate(pPed);

	FSM_State(State_LeaveAnyCar)
		FSM_OnEnter
			return LeaveAnyCar_OnEnter(pPed);
		FSM_OnUpdate
			return LeaveAnyCar_OnUpdate(pPed);

	FSM_State(State_DriveBy)
		FSM_OnEnter
			return DriveBy_OnEnter(pPed);
		FSM_OnUpdate
			return DriveBy_OnUpdate(pPed);

	FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

//
//
//
void ProcessBoatHorizonTracking(CBoat * UNUSED_PARAM(pBoat), CPed * UNUSED_PARAM(pPed))
{
#if 0 // Disabled since handled by ambient LookAt system now
	// Don't do this when in reverse, as we will want to play a canned look-over-shoulder clip instead
	if(pBoat->GetThrottle() >= 0.0f)
	{
		static const float fDist = 50.0f;
		static const float fZHeight = 0.0f;
		Vector3 vTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pBoat->GetTransform().GetB()) * fDist);
		vTarget.z = fZHeight;
		pPed->GetIkManager().LookAt(0, NULL, 1000, BONETAG_INVALID, &vTarget);
	}
#endif
}

//
//
//
CTask::FSM_Return CTaskCarDrive::ProcessPostFSM	()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// For all peds in player's boat do some head-tracking towards the horizon
	if(m_pVehicle && m_pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT && m_pVehicle->GetDriver() && m_pVehicle->GetDriver()->IsPlayer())
	{
		ProcessBoatHorizonTracking((CBoat*)m_pVehicle.Get(), pPed);
	}

	return FSM_Continue;
}

void CTaskCarDrive::CleanUp()
{
// 	CPed *pPed = GetPed();
// 
// 	if(m_pVehicle && m_pVehicle->IsDriver(pPed) && !m_pVehicle->IsNetworkClone() )
// 	{
// 		aiTask* pRunningVehicleTask = m_pVehicle->GetIntelligence()->GetTaskManager()->GetTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
// 		if( pRunningVehicleTask && m_pVehicleTask && m_pVehicleTask->GetTaskType() == pRunningVehicleTask->GetTaskType() )
// 		{
// 			m_pVehicle->GetIntelligence()->GetTaskManager()->GetTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
// 		}
// 	}
}

void CTaskCarDrive::RequestDriveBy(const CAITarget& target, const float fFrequency, const float fAbortDistance, const u32 uFiringPattern, const u32 uDurationMs, const u32 uWeaponHash)
{
	m_bDriveByRequested = true;
	m_DriveByInfo.DriveByTarget = target;
	m_DriveByInfo.fFrequency = fFrequency;
	m_DriveByInfo.fAbortDistance = fAbortDistance;
	m_DriveByInfo.uFiringPattern = uFiringPattern;
	m_DriveByInfo.uDuration = uDurationMs;
	m_DriveByInfo.uWeaponHash = uWeaponHash;
	m_nTimeLastDriveByRequested = fwTimer::GetTimeInMilliseconds();
}

//
//
//
CTask::FSM_Return CTaskCarDrive::Start_OnUpdate(CPed* pPed)
{
	if(!m_pVehicle)
	{
		//No vehicle has been specified.
		//If the ped is already in a vehicle then use that one.
		if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			//Ped already in a car.
			m_pVehicle=pPed->GetMyVehicle();

			// wait for the vehicle to become local in a network game
			if (!m_pVehicle->IsNetworkClone())
			{
				//pFirstSubTask=CreateSubTask(CTaskTypes::TASK_COMPLEX_CAR_DRIVE_BASIC,pPed);
				SetState(State_CarDrive);
			}
		}
	}
	else
	{
		//A vehicle has been specified.
		if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			//Ped is in a vehicle already but is it the correct one?
			if(pPed->GetMyVehicle() == m_pVehicle)
			{
				//Ped is in the correct vehicle.		
				//pFirstSubTask=CreateSubTask(CTaskTypes::TASK_COMPLEX_CAR_DRIVE_BASIC,pPed);
				// wait for the vehicle to become local in a network game
				if (!m_pVehicle->IsNetworkClone())
				{
					SetState(State_CarDrive);
				}
			}
			else
			{
                if(!pPed->IsNetworkClone())
                {
				    //Ped isn't in the correct vehicle.
				    //pFirstSubTask=CreateSubTask(CTaskTypes::TASK_LEAVE_ANY_CAR,pPed);
				    SetState(State_LeaveAnyCar);
                }
			}
		}
		else
		{
			//Ped must get in the specified vehicle unless it is an upside-down car.
			if(m_pVehicle->InheritsFromBike())
			{
                if(!pPed->IsNetworkClone())
                {
				    //Always try to get back on bikes.
				    //pFirstSubTask=CreateSubTask(CTaskTypes::TASK_ENTER_VEHICLE,pPed);
				    SetState(State_EnterVehicle);
                }
			}
			else
			{
				//Only get back in cars if they aren't upside down.
				if(CUpsideDownCarCheck::IsCarUpsideDown(m_pVehicle.Get()) || CBoatIsBeachedCheck::IsBoatBeached(*m_pVehicle) || m_bSetPedOuOfVehicle)
				{
					//Car is upside down so quit
					//pFirstSubTask=CreateSubTask(CTaskTypes::TASK_FINISHED,pPed);
					SetState(State_Finish);
				}
				else
				{
                    if(!pPed->IsNetworkClone())
                    {
					    //Car isn't upside down so get back in.
					    //pFirstSubTask=CreateSubTask(CTaskTypes::TASK_ENTER_VEHICLE,pPed);		
					    SetState(State_EnterVehicle);
                    }
				}
			}
		}
	}

	return FSM_Continue;
}

bool CTaskCarDrive::ShouldDoDriveBy()
{
	if (!m_bDriveByRequested)
	{
		return false;
	}

	m_bDriveByRequested = false;

	return true;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::CarDrive_OnEnter(CPed* pPed)
{
	Assert(m_pVehicle);

	if(m_pVehicle->IsDriver(pPed))
	{
		SetUpCar();
	}

	SetNewTask(rage_new CTaskInVehicleBasic(m_pVehicle));

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::CarDrive_OnUpdate(CPed* pPed)
{
	//This ped can not drive this vehicle.
	if (m_bSetPedOuOfVehicle)
	{
		SetState(State_LeaveAnyCar);
		return FSM_Continue;
	}

	taskAssertf(m_pVehicle, "Vehicle pointer is NULL- how can this happen?");
	//taskAssertf(!m_pVehicle || m_pVehicle->GetIntelligence(), "Vehicle intelligence pointer is NULL- how can this happen?");
	//taskAssertf(!m_pVehicle || m_pVehicle->GetIntelligence()->GetTaskManager(), "GetTaskManager is NULL- how can this happen?");

	bool bStateSet = false;
	if (m_pVehicle && m_pVehicle->GetIntelligence() && m_pVehicle->GetIntelligence()->GetTaskManager())
	{
		CTask* pRunningTask = static_cast<CTask*>(m_pVehicle->GetIntelligence()->GetTaskManager()->GetTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY));
		if(GetIsFlagSet(aiTaskFlags::SubTaskFinished)|| (m_pVehicleTask && pRunningTask == NULL && m_pVehicle->IsDriver(pPed)) )
		{
			SetState(State_Start);
			bStateSet = true;
		}
	}

	if (!bStateSet)
	{
		if(pPed->GetMyVehicle() == NULL || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			SetState(State_Start);
	}

	if (ShouldDoDriveBy())
	{
		SetState(State_DriveBy);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarDrive::DriveBy_OnEnter(CPed* pPed)
{
	Assert(m_pVehicle);

	if (m_DriveByInfo.uWeaponHash != -1 && pPed->GetWeaponManager()->GetSelectedWeaponHash() != m_DriveByInfo.uWeaponHash)
	{
		pPed->GetWeaponManager()->EquipWeapon(m_DriveByInfo.uWeaponHash, -1, true);
	}

	CTask* pTask = rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire,m_DriveByInfo.uFiringPattern,&m_DriveByInfo.DriveByTarget, m_DriveByInfo.fFrequency, m_DriveByInfo.fAbortDistance);
	SetNewTask(pTask);

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::DriveBy_OnUpdate(CPed* pPed)
{
	taskAssertf(m_pVehicle, "Vehicle pointer is NULL- how can this happen?");
	taskAssertf(m_pVehicle->GetIntelligence(), "Vehicle intelligence pointer is NULL- how can this happen?");
	taskAssertf(m_pVehicle->GetIntelligence()->GetTaskManager(), "GetTaskManager is NULL- how can this happen?");

	CTask* pRunningTask = static_cast<CTask*>(m_pVehicle->GetIntelligence()->GetTaskManager()->GetTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY));
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished)|| (m_pVehicleTask && pRunningTask == NULL && m_pVehicle->IsDriver(pPed)) )
	{
		//the drive-by task will quit on its own if a distance threshold is passed
		SetState(State_Start);
	}
	else if (fwTimer::GetTimeInMilliseconds() > m_nTimeLastDriveByRequested + m_DriveByInfo.uDuration)
	{
		//bail out if we've been doing a drive-by for long enough
		SetState(State_Start);
	}
	else
	{
		if(pPed->GetMyVehicle() == NULL || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			SetState(State_Start);
	}

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::EnterVehicle_OnEnter(CPed* pPed)
{
	Assert(m_pVehicle);

	if( pPed->PopTypeIsRandom() && m_pVehicle->ContainsPlayer() )
	{
		return FSM_Continue;
	}
	else if( m_bAsDriver )
	{
		// Return to the vehicle
		SetNewTask(rage_new CTaskEnterVehicle(m_pVehicle, SR_Specific, 0));
	}
	else
	{
		// Return to the vehicle
		SetNewTask(rage_new CTaskEnterVehicle(m_pVehicle, SR_Specific, 1));
	}

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::EnterVehicle_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
		{
			Assert(pPed->GetMyVehicle() == m_pVehicle);

			// wait for the vehicle to become local in a network game
			if (!m_pVehicle->IsNetworkClone())
			{
				SetState(State_CarDrive);
			}
		}
		else
		{
			SetState(State_Finish);
		}
	}
	else
	{
		if( m_pVehicle && pPed->PopTypeIsRandom() && m_pVehicle->ContainsPlayer() )
		{
			if( !GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
				return FSM_Quit;
		}
	}
	
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::LeaveAnyCar_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	Assert(m_pVehicle);

	SetNewTask(rage_new CTaskLeaveAnyCar());

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskCarDrive::LeaveAnyCar_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!m_pVehicle || m_bSetPedOuOfVehicle)
		{
			//Vehicle must have been deleted.
			SetState(State_Finish);
		}
		else
		{
			SetState(State_EnterVehicle);			
		}
	}

	return FSM_Continue;
}

void CTaskCarDrive::SetUpCar()
{
	Assert(m_pVehicle);

	// JB: also ensure that the new task takes its nodes from the vehicle's existing node list
	CVehicleNodeList * pNodeList = m_pVehicle->GetIntelligence()->GetNodeList();
	const CTask* pNewTask = static_cast<const CTask*>(m_pVehicleTask.Get());
	// if the car already has a task of the same type as the new one, don't set another one
	if( pNewTask)
	{
		CTask* pRunningTask = static_cast<CTask*>(m_pVehicle->GetIntelligence()->GetTaskManager()->GetTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY));
		if( pRunningTask && 
			pNewTask->IsVehicleMissionTask() && 
			(!m_pVehicle->GetDriver() || !m_pVehicle->GetDriver()->PopTypeIsMission()) &&
			!m_pVehicle->PopTypeIsMission() && 
			pRunningTask->IsVehicleMissionTask() && 
			pRunningTask->GetTaskType() == pNewTask->GetTaskType() )
		{
			((CTaskVehicleMissionBase*)pRunningTask)->SetParams(((const CTaskVehicleMissionBase*)pNewTask)->GetParams());
			return;
		}
	}
	m_pActualVehicleTask = m_pVehicleTask ? m_pVehicleTask->Copy() : NULL;
	CTask *pVehicleTask = static_cast<CTask*>(m_pActualVehicleTask.Get());
	if(pVehicleTask && pVehicleTask->IsVehicleMissionTask())
		((CTaskVehicleMissionBase*)pVehicleTask)->CopyNodeList(pNodeList);

	Assert(!m_pVehicle->IsNetworkClone());

	m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, m_pActualVehicleTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	m_pVehicle->GetIntelligence()->UpdateJustBeenGivenNewCommand();

	// take a copy of the original autopilot settings for this vehicle
//	m_origDrivingMode = (s8)m_pVehicle->GetIntelligence()->GetDrivingMode();
//	m_origMission = (s8)m_pVehicle->GetIntelligence()->GetMission();
//  m_origCruiseSpeed = (u8)m_pVehicle->GetIntelligence()->GetCruiseSpeed();

//	m_bIsCarSetUp=true;
}

// copies current ai state of the car to the task
void CTaskCarDrive::GetCarState()
{
	CTaskCarDrive::SetUpCar();

//	m_fCruiseSpeed = (float)m_pVehicle->GetIntelligence()->GetCruiseSpeed();
//	m_iDrivingStyle = m_pVehicle->GetIntelligence()->GetDrivingMode();
 }

//
//
//
#if !__FINAL
const char * CTaskCarDrive::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_CarDrive",
		"State_EnterVehicle",
		"State_LeaveAnyCar",
		"State_DriveBy",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

////////////////
//DRIVE WANDER
////////////////
//
//
//
CTaskCarDriveWander::CTaskCarDriveWander
(CVehicle* pVehicle, const s32 iDrivingFlags, const float CruiseSpeed, const bool bUseExistingNodes)
 : CTaskCarDrive(pVehicle, CruiseSpeed, iDrivingFlags, bUseExistingNodes)
{
	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE_WANDER);
}

//
//
//
CTaskCarDriveWander::CTaskCarDriveWander(CVehicle* pVehicle, aiTask *pVehicleTask, const bool bUseExistingNodes)
: CTaskCarDrive(pVehicle, pVehicleTask, bUseExistingNodes)
{
	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE_WANDER);
}

//
//
//
CTaskCarDriveWander::~CTaskCarDriveWander()
{

}

//
//
//
CTask::FSM_Return CTaskCarDriveWander::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!IsNetworkVehicleValid(m_pVehicle))
	{
		return FSM_Quit;
	}

	if( m_pVehicle && m_pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		return FSM_Quit;
	}

	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	//Maybe decide to become an "interesting driver."
	//we'll check if the player hasn't seen one in a while,
	//and if we meet certain requirements, and if so,
	//override driver personality settings to become stereotypically
	//bad/reckless/etc
	ThinkAboutBecomingInterestingDriver();

	//If the ped can not drive this vehicle then set the ped to exit the vehicle.
	//Don't allow us to exit immediately - this can cause an infinite loop if we can't exit.
	if(GetTimeInState() > 0.0f)
	{
		m_bSetPedOuOfVehicle = NetworkInterface::IsInvalidVehicleForDriving(pPed, m_pVehicle);
	}

	return FSM_Continue;
}

void CTaskCarDriveWander::ThinkAboutBecomingInterestingDriver()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	//this crashed once due to a NULL m_pVehicle, I guess that's possible?
	if (!m_pVehicle)
	{
		return;
	}

	const bool bDriverSupportsSlowDriving = pPed->IsGangPed() || pPed->GetPedModelInfo()->GetPersonalitySettings().AllowSlowCruisingWithMusic();
	const bool bCarSupportsSlowDriving = m_pVehicle->GetVehicleModelInfo()->GetAllowSundayDriving()
		&& !m_pVehicle->InheritsFromBike()
		&& !m_pVehicle->InheritsFromQuadBike()
		&& !m_pVehicle->InheritsFromAmphibiousQuadBike();

// #if __BANK
// 	if (bCarSupportsSlowDriving)
// 	{
// 		grcDebugDraw::Line(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 4.0f), Color_green3, -1);
// 	}
// 
// 	if (bDriverSupportsSlowDriving)
// 	{
// 		grcDebugDraw::Line(pPed->GetMatrixRef().d(), pPed->GetMatrixRef().d() + Vec3V(0.0f, 0.0f, 4.0f), Color_yellow2, -1);
// 	}
// #endif //__BANK

	const bool bCanCreateSlowDriver = CRandomEventManager::GetInstance().CanCreateEventOfType(RC_DRIVER_SLOW, true);
	const bool bCanCreateRecklessDriver = CRandomEventManager::GetInstance().CanCreateEventOfType(RC_DRIVER_RECKLESS, true);
	const bool bCanCreateProDriver = CRandomEventManager::GetInstance().CanCreateEventOfType(RC_DRIVER_PRO, true);
	//replace this with something that just checks the type.
	if (bCanCreateSlowDriver
		|| bCanCreateRecklessDriver
		|| bCanCreateProDriver)
	{
		if (pPed->PopTypeGet() != POPTYPE_RANDOM_AMBIENT
			|| m_pVehicle->IsLawEnforcementVehicle()
			|| pPed->GetPedIntelligence()->GetOrder()
			|| m_pVehicle->HasMissionCharsInIt()
			|| m_pVehicle->GetAttachedTrailer())
		{
			return;
		}

		static const int s_iNumInterestingDriverEvents = 3;
		atFixedArray<eRandomEvent, s_iNumInterestingDriverEvents> possibleEvents;

		//randomly choose which one to create based on this ped's properties and the car he's in
		if (bCanCreateProDriver
			&& m_pVehicle->GetVehicleModelInfo()->GetVehicleSwankness() >= SWANKNESS_4
			&& pPed->GetPedModelInfo()->IsMale()
			&& !m_pVehicle->m_nVehicleFlags.bMadDriver
			&& !m_pVehicle->m_nVehicleFlags.bSlowChillinDriver)
		{
			possibleEvents.Push(RC_DRIVER_PRO);
		}

		if (bCanCreateSlowDriver
			&& bCarSupportsSlowDriving
			&& !m_pVehicle->m_nVehicleFlags.bMadDriver
			&& bDriverSupportsSlowDriving)
		{
			possibleEvents.Push(RC_DRIVER_SLOW);
		}
		else if (bCanCreateRecklessDriver
			&& m_pVehicle->GetVehicleModelInfo()->GetAllowJoyriding()
			&& !m_pVehicle->m_nVehicleFlags.bSlowChillinDriver)
		{
			possibleEvents.Push(RC_DRIVER_RECKLESS);
		}
		
		if (possibleEvents.GetCount() > 0)
		{
			const int iEventIndexToChoose = g_ReplayRand.GetInt() % possibleEvents.GetCount();
			const eRandomEvent eventToChoose = possibleEvents[iEventIndexToChoose];
			
			switch (eventToChoose)
			{
			case RC_DRIVER_PRO:
				pPed->GetPedIntelligence()->SetDriverAbilityOverride(1.0f);
				pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
				break;
			case RC_DRIVER_RECKLESS:
				pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);
				pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
				m_pVehicle->m_nVehicleFlags.bMadDriver = true;
				break;
			case RC_DRIVER_SLOW:
				pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);
				pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(0.0f);
				m_pVehicle->m_nVehicleFlags.bSlowChillinDriver = true;
				m_pVehicle->RolldownWindows();
				break;
			default:
				Assertf(0, "Unexpected interesting drive type %d chosen", eventToChoose);
				return;
			}

			m_pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
			CRandomEventManager::GetInstance().SetEventStarted(eventToChoose);
		}
	}
}

//
//
//
void CTaskCarDriveWander::SetUpCar()
{
	CTaskCarDrive::SetUpCar();

    m_pVehicle->SwitchEngineOn();
	m_pVehicle->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();	// So that it doesn't reverse straight away.
}

CTaskInfo *CTaskCarDriveWander::CreateQueriableState() const
{
    s32   iDrivingFlags = 0;
    float fCruiseSpeed  = 0.0f;

    const CTask *pVehicleTask = static_cast<const CTask*>(m_pVehicleTask.Get());
	// if the car already has a cruise task don't set another one
	if(pVehicleTask && 
		(pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRUISE_NEW ) )
	{
        const CTaskVehicleMissionBase *pVehicleMissionBase = SafeCast(const CTaskVehicleMissionBase, pVehicleTask);
        iDrivingFlags = pVehicleMissionBase->GetParams().m_iDrivingFlags;
        fCruiseSpeed  = pVehicleMissionBase->GetParams().m_fCruiseSpeed;
    }

    return rage_new CClonedDriveCarWanderInfo(m_pVehicle, iDrivingFlags, fCruiseSpeed, m_bUseExistingNodes);
}

CClonedDriveCarWanderInfo::CClonedDriveCarWanderInfo(CVehicle *pVehicle, const s32 iDrivingFlags, const float CruiseSpeed, const bool bUseExistingNodes) :
m_Vehicle(pVehicle)
,m_DrivingFlags(iDrivingFlags)
, m_CruiseSpeed(CruiseSpeed)
, m_UseExistingNodes(bUseExistingNodes)
{
}

CClonedDriveCarWanderInfo::CClonedDriveCarWanderInfo() :
m_Vehicle(0)
,m_DrivingFlags(0)
, m_CruiseSpeed(0)
, m_UseExistingNodes(false)
{
}

CTask *CClonedDriveCarWanderInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    if(m_Vehicle.GetVehicle())
    {
        return rage_new CTaskCarDriveWander(m_Vehicle.GetVehicle(), m_DrivingFlags, m_CruiseSpeed, m_UseExistingNodes);
    }
    else
    {
        return 0;
    }
}

/////////////////////
//DRIVE POINT ROUTE
/////////////////////
CTaskDrivePointRoute::CTaskDrivePointRoute
(CVehicle* pVehicle, const CPointRoute& route, 
 const float CruiseSpeed, 
 const int iMode, 
 const int iDesiredCarModel, 
 const float fTargetRadius,
 const s32 iDrivingFlags) 
: m_pVehicle(pVehicle),
  m_pRoute(rage_new CPointRoute(route)),
//  m_fCruiseSpeed(fCruiseSpeed),
  m_iMode(iMode),
  m_iDesiredCarModel(iDesiredCarModel),
  m_fTargetRadius(fTargetRadius),
  m_iProgress(0),
  m_pDesiredSubtask(NULL),
  m_fCruiseSpeed(CruiseSpeed),
  m_iDrivingFlags(iDrivingFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_CAR_DRIVE_POINT_ROUTE);
}

//
//
//
CTaskDrivePointRoute::~CTaskDrivePointRoute()
{
	if(m_pRoute)
	{
		delete m_pRoute;
		m_pRoute = NULL;
	}

	if(m_pDesiredSubtask)
	{
		delete m_pDesiredSubtask;
		m_pDesiredSubtask = NULL;
	}
}

//
//
//
CTask::FSM_Return CTaskDrivePointRoute::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
	FSM_State(State_DrivePointRoute)
		FSM_OnEnter
			return DrivePointRoute_OnEnter(pPed);
		FSM_OnUpdate
			return DrivePointRoute_OnUpdate(pPed);
	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

//
//
//
CTask::FSM_Return CTaskDrivePointRoute::DrivePointRoute_OnEnter(CPed* pPed)
{
	if(m_iProgress != 0)//only check if we're in the car if this has been called once already. to give the ped a chance to get into the car.
	{
		if(pPed->GetVehiclePedInside() != m_pVehicle)
		{
			// Not in the vehicle
			SetState(State_Finish);
		}
	}

	if(m_iProgress==m_pRoute->GetSize())
	{
		return FSM_Quit;
	}
	else
	{
		Vector3 vTarget = m_pRoute->Get(m_iProgress);

		CTask *pTask = NULL;

		sVehicleMissionParams params;
		params.m_fCruiseSpeed = m_fCruiseSpeed;
		params.SetTargetPosition(vTarget);
		params.m_fTargetArriveDist = m_fTargetRadius;
		params.m_iDrivingFlags = GetDrivingFlags();

		switch(m_iMode)
		{
		case MODE_NORMAL:
		case MODE_RACING:
			pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
			break;
		case MODE_REVERSING:
			params.m_iDrivingFlags = GetDrivingFlags()|DF_DriveInReverse;
			pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
			break;
		default:
			Assert(0);
			break;
		}

		SetNewTask(rage_new CTaskControlVehicle(m_pVehicle, pTask, m_pDesiredSubtask?m_pDesiredSubtask->Copy():NULL) );
	}

	m_iProgress++;

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskDrivePointRoute::DrivePointRoute_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	// If the subtask has finished reenter this state.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

//
//
//
aiTask* CTaskDrivePointRoute::Copy() const
{
	CTaskDrivePointRoute* pTask = rage_new CTaskDrivePointRoute
		(m_pVehicle,*m_pRoute, m_fCruiseSpeed, m_iMode, m_iDesiredCarModel ,m_fTargetRadius, GetDrivingFlags());

	if( m_pDesiredSubtask )
		pTask->SetDesiredSubtask(m_pDesiredSubtask->Copy());

	return pTask;
}

void CTaskDrivePointRoute::SetDesiredSubtask( aiTask* pTask )
{
	if( m_pDesiredSubtask )
		delete m_pDesiredSubtask;

	m_pDesiredSubtask = pTask;
}


#if !__FINAL
// return the name of the given task as a string
const char * CTaskDrivePointRoute::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_DrivePointRoute",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

#if 0
const float CTaskComplexCarDriveUsingNavMesh::ms_fTargetRadius = 4.0f;

CTaskComplexCarDriveUsingNavMesh::CTaskComplexCarDriveUsingNavMesh(CVehicle * pVehicle, const Vector3 & vTarget)
:	m_pVehicle(pVehicle),
	m_vTarget(vTarget),
	m_iRouteState(ROUTESTATE_STILL_WAITING)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_CAR_DRIVE_USING_NAVMESH);
}

CTaskComplexCarDriveUsingNavMesh::~CTaskComplexCarDriveUsingNavMesh()
{

}

#if !__FINAL
void CTaskComplexCarDriveUsingNavMesh::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
const char * CTaskComplexCarDriveUsingNavMesh::GetStaticStateName( s32 iState )
{
	static char * strStateNames[] =
	{
		"STATE_REQUEST_PATH",
		"STATE_FOLLOWING_PATH"
	};
	return strStateNames[iState];
}
#endif

CTask::FSM_Return CTaskComplexCarDriveUsingNavMesh::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || !pPed->GetMyVehicle())
		return FSM_Quit;

	FSM_Begin

		FSM_State(STATE_REQUEST_PATH)
			FSM_OnEnter
				RequestPath_OnEnter(pPed);
			FSM_OnUpdate
				return RequestPath_OnUpdate(pPed);

		FSM_State(STATE_FOLLOWING_PATH)
			FSM_OnEnter
				return FSM_Continue; // already done in RequestPath_OnUpdate when path is retrieved
			FSM_OnUpdate
				return FollowingPath_OnUpdate(pPed);

		FSM_End
}

void CTaskComplexCarDriveUsingNavMesh::RequestPath_OnEnter(CPed * pPed)
{
	Assert(!m_RouteSearchHelper.IsSearchActive());
	Assert(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle());

	m_RouteSearchHelper.SetEntityRadius(m_pVehicle->GetBoundRadius());

	u32 iFlags = PATH_FLAG_DONT_LIMIT_SEARCH_EXTENTS | PATH_FLAG_NEVER_CLIMB_OVER_STUFF | PATH_FLAG_NEVER_DROP_FROM_HEIGHT | PATH_FLAG_NEVER_USE_LADDERS;
	if(m_pVehicle->GetVehicleType()!=VEHICLE_TYPE_BOAT)
		iFlags |= PATH_FLAG_NEVER_ENTER_WATER;

	static float fMaxAngle = 35.0f;
	m_RouteSearchHelper.SetMaxNavigableAngle( ( DtoR * fMaxAngle) );

	Vector3 vStartPos = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetB()) * 0.25f);

	m_RouteSearchHelper.StartSearch(pPed, vStartPos, m_vTarget, ms_fTargetRadius, iFlags);

	m_CheckPathResultTimer.Set(fwTimer::GetTimeInMilliseconds(), 100);

	m_iRouteState = ROUTESTATE_STILL_WAITING;
}

void CTaskComplexCarDriveUsingNavMesh::GiveCarPointPathToFollow(CVehicle* pVeh, Vector3 *pPoints, int numPoints)
{
	aiTask *pCarTask = rage_new CTaskVehicleFollowPointPath(NetworkInterface::GetSyncedTimeInMilliseconds() + 60000);

	numPoints = MIN(numPoints, CVehPointPath::MAXNUMPOINTS);
	pVeh->GetIntelligence()->m_pointPath.m_numPoints = numPoints;

	for(int i = 0; i < numPoints; i++)
	{
		pVeh->GetIntelligence()->m_pointPath.m_points[i].x = pPoints[i].x;
		pVeh->GetIntelligence()->m_pointPath.m_points[i].y = pPoints[i].y;
	}

	Assert(!pVeh->IsNetworkClone());
	pVeh->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
}

CTask::FSM_Return CTaskComplexCarDriveUsingNavMesh::RequestPath_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	Assert(m_RouteSearchHelper.IsSearchActive());
	Assert(m_CheckPathResultTimer.IsSet());

	m_iRouteState = ROUTESTATE_STILL_WAITING;

	if(m_CheckPathResultTimer.IsSet() && m_CheckPathResultTimer.IsOutOfTime())
	{
		SearchStatus searchStatus;
		float fDistance;
		Vector3 vLastPoint;
		Vector3 pvPoints[MAX_NUM_PATH_POINTS];
		s32 iMaxNumPts = MAX_NUM_PATH_POINTS;

		if(m_RouteSearchHelper.GetSearchResults(searchStatus, fDistance, vLastPoint, &pvPoints[0], &iMaxNumPts))
		{
			if(searchStatus == SS_SearchSuccessful)
			{
				GiveCarPointPathToFollow(m_pVehicle, pvPoints, MIN(iMaxNumPts, 16));
				// Need to set this flag if we want to enter a new state & task here (which we do, since we
				// don't want to be storing off loads of temporary data unnecessarily).
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

				SetNewTask(NULL);
				SetState(STATE_FOLLOWING_PATH);

				m_iRouteState = ROUTESTATE_FOUND;
			}
			else if(searchStatus == SS_SearchFailed)
			{
				m_iRouteState = ROUTESTATE_NOT_FOUND;

				return FSM_Quit;
			}
		}
		else
		{
			m_iRouteState = ROUTESTATE_STILL_WAITING;

			m_CheckPathResultTimer.Set(fwTimer::GetTimeInMilliseconds(), 100);
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskComplexCarDriveUsingNavMesh::FollowingPath_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	//Assertf(CCarIntelligence::FindUberMissionForCar(m_pVehicle)==&m_CarMission, "There must be another active task for this ped with a car CVehMission - this will cause conflicts.");
	//Assertf(m_pVehicle->GetIntelligence()->FindUberMissionForCar()==&m_CarMission, "There must be another active task for this ped with a car CVehMission - this will cause conflicts.");

	m_iRouteState = ROUTESTATE_FOUND;

	// Have we reached the target yet?  If not then request another path.
	const Vector3 vToTarget = m_vTarget - VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
	if(vToTarget.Mag() < ms_fTargetRadius)
	{
		return FSM_Quit;
	}

	aiTask *pCarTask = m_pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_FOLLOW_POINT_PATH, VEHICLE_TASK_PRIORITY_PRIMARY);
	if(pCarTask == NULL)
	{
		m_iRouteState = ROUTESTATE_STILL_WAITING;
		SetState(STATE_REQUEST_PATH);
		return FSM_Continue;
	}

	return FSM_Continue;
}

#endif //0

///////////////////////
//SET CAR TEMP ACTION
///////////////////////
CTaskCarSetTempAction::CTaskCarSetTempAction(CVehicle* pVehicle, const int iAction, const int iTime)
	: m_pTargetVehicle(pVehicle),
	m_iAction(iAction),
	m_iTime(iTime)
	, m_uConfigFlagsForInVehicleBasic(0)
	, m_uConfigFlagsForAction(0)
{
	Assertf(!pVehicle || !pVehicle->IsNetworkClone(), "Can't do temp action on a clone vehicle");
	Assertf(m_iAction!=TEMPACT_NONE, "Can't do temp action none");
	SetInternalTaskType(CTaskTypes::TASK_CAR_SET_TEMP_ACTION);
}
	
CTaskCarSetTempAction::~CTaskCarSetTempAction()
{
}

aiTask* CTaskCarSetTempAction::Copy() const
{
	CTaskCarSetTempAction* pTask = rage_new CTaskCarSetTempAction(m_pTargetVehicle, m_iAction, m_iTime);
	pTask->m_uConfigFlagsForInVehicleBasic = m_uConfigFlagsForInVehicleBasic;
	pTask->m_uConfigFlagsForAction = m_uConfigFlagsForAction;

	return pTask;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskCarSetTempAction::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_CarTempAction",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

//update the local state
CTask::FSM_Return CTaskCarSetTempAction::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		//Task start
	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pPed);

	FSM_State(State_CarTempAction)
		FSM_OnEnter
		return CarSetTempAction_OnEnter(pPed);
		FSM_OnUpdate
		return CarSetTempAction_OnUpdate(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskCarSetTempAction::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_CarTempAction);

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarSetTempAction::CarSetTempAction_OnEnter(CPed* pPed)
{
	SetNewTask( rage_new CTaskInVehicleBasic(pPed->GetMyVehicle(), false, NULL, m_uConfigFlagsForInVehicleBasic) );

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarSetTempAction::CarSetTempAction_OnUpdate(CPed* pPed)
{
	if( !m_pTargetVehicle )
	{
		m_pTargetVehicle=pPed->GetMyVehicle();
	}

	if( !m_pTargetVehicle || m_pTargetVehicle->IsNetworkClone() )
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	Assertf(m_pTargetVehicle, "m_pTargetVehicle - cannot be NULL here (1)");

	CTaskVehicleMissionBase *pCarTask = m_pTargetVehicle->GetIntelligence()->GetActiveTask();
	if (m_iAction==TEMPACT_NONE && !pCarTask)
	{		// If we have already set the tempaction and the vehicle ai code is done with it; our task is finished as well.
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(m_pTargetVehicle && m_iAction!=TEMPACT_NONE)
	{
		aiTask *pTempTask = CVehicleIntelligence::GetTempActionTaskFromTempActionType(m_iAction, m_iTime, m_uConfigFlagsForAction);

		Assertf(m_pTargetVehicle, "m_pTargetVehicle - cannot be NULL here (2)");

		m_iAction=TEMPACT_NONE;
		m_pTargetVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTempTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}


	return FSM_Continue;
}


static u32 TIME_BUTTON_HELD_TO_EXIT_VEHICLE_WITH_ENGINE_OFF = 400;
static u32 TIME_BUTTON_HELD_TO_EXIT_VEHICLE_REGARDLESS_OF_SPEED = 1000;

dev_float BUDDY_SLOW_SPEED_MIN = 3.0f;
dev_float GIRLFRIEND_BUDDY_SLOW_SPEED_MAX	= 9.0f;
dev_float BUDDY_SLOW_SPEED_MAX	= 12.0f;
dev_float BUDDY_FAST_SPEED_MIN	= 20.0f;
dev_float BUDDY_SAY_SLOW_TIME	= 12.0f;
dev_float BUDDY_SAY_QUICK_TIME	= 12.0f;

//////////////////////////////////////////////////////////////////////////
// class CTaskPlayerDrive

CTaskPlayerDrive::Tunables CTaskPlayerDrive::sm_Tunables;
IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskPlayerDrive, 0x800c831f);

CTaskPlayerDrive::CTaskPlayerDrive() :
m_bExitButtonUp(false),
m_bSmashedWindow(false),
m_chosenAmbientSetId(CLIP_SET_ID_INVALID),
m_bClipGroupLoaded(false),
m_bDriveByClipSetsLoaded(false),
m_fTimePlayerDrivingSlowly(0.0f),
m_fTimePlayerDrivingQuickly(0.0f),
m_fDangerousVehicleCounter(0.0f),
m_uTimeSinceStealthNoise(0),
m_bExitCarWhenGunTaskFinishes(false),
m_bForcedExit(false),
m_bVehicleDriveTaskAddedOrNotNeeded(false),
m_iTargetShuffleSeatIndex(-1), 
m_bRestarting(false),
m_bInFirstPersonVehicleCamera(false),
m_bTryingToInterruptVehMelee(false),
m_fTimeToDisableFP(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_PLAYER_DRIVE);
}

void  CTaskPlayerDrive::CleanUp()
{
    // Clear the vehicle task running on the vehicle task manager
    CPed* pPed = GetPed(); //Get the ped ptr.
    if( pPed )
    {
        CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			if( !pVehicle->GetIsUsingScriptAutoPilot() && pVehicle->IsDriver(pPed) && !pVehicle->IsNetworkClone() )
			{
				pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
			}
		}
    }

#if KEYBOARD_MOUSE_SUPPORT
	if(CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())
	{
		CControlMgr::GetMainPlayerControl().SetDriveCameraToggleOn(false);
		CControlMgr::ReInitControls();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

}

bool CTaskPlayerDrive::ProcessPostPreRenderAfterAttachments()
{
	CPed& rPed = *GetPed();
	if (ShouldProcessPostPreRenderAfterAttachments(&rPed, rPed.GetMyVehicle()))
	{
		rPed.UpdateFpsCameraRelativeMatrix();
		return true;
	}

	return false;
}

REGISTER_TUNE_GROUP_FLOAT( DISABLE_FP_TIMER, 1.0f );
REGISTER_TUNE_GROUP_BOOL( DISABLE_FP, true );
CTask::FSM_Return CTaskPlayerDrive::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	m_bIsExitingGunState = false;

	INSTANTIATE_TUNE_GROUP_FLOAT( VEHICLE_MELEE, DISABLE_FP_TIMER, 0.0f, 100.0f, 0.1f );
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee)) // Keep timer full while melee is going on
		m_fTimeToDisableFP = DISABLE_FP_TIMER;

	if (m_fTimeToDisableFP > 0.0f)
	{
		INSTANTIATE_TUNE_GROUP_BOOL( VEHICLE_MELEE, DISABLE_FP );
		if (DISABLE_FP)
			camInterface::GetCinematicDirector().DisableFirstPersonInVehicleThisUpdate();

		m_fTimeToDisableFP -= fwTimer::GetTimeStep();
	}

	CVehicle* pVehicle = pPed->GetMyVehicle();
	// If we aren't in a vehicle anymore then try to quit
	if( pVehicle == NULL || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		if( GetState() != State_ExitVehicle )
			return FSM_Quit;
		else
			return FSM_Continue;
	}

	if (ShouldProcessPostPreRenderAfterAttachments(pPed, pVehicle))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRenderAfterAttachments, true);
	}	

	// Make buddies comment on the speed of the car if the player is driving too slowly, or too quickly.
	ProcessBuddySpeedSpeech(pPed);

	// work out the best clip group to use for being busted
	StreamBustedClipGroup(pPed);

	// Work out the best clip group to be used for drivebys
	if (GetState() != State_ExitVehicle)
	{
		PreStreamDrivebyClipSets(pPed);
	}

	taskFatalAssertf(pPed->IsAPlayerPed(),"pPed must be a player");
	CControl *pControl = pPed->GetControlFromPlayer();

	if( CGameWorld::GetMainPlayerInfo()->AreControlsDisabled() WIN32PC_ONLY(|| SMultiplayerChat::GetInstance().ShouldDisableInputSinceChatIsTyping()))
	{
		m_bExitButtonUp = false;
	}
	else if( !m_bExitButtonUp && !pControl->GetVehicleExit().IsDown())
	{
		m_bExitButtonUp = true;
		m_uTimeSinceLastButtonPress = fwTimer::GetTimeInMilliseconds();
	}

	if(pVehicle && pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT && pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer())
	{
		ProcessBoatHorizonTracking((CBoat*)pVehicle, pPed);
	}

	// If there are any randoms still in the car,make them leave
	if( pVehicle->IsDriver(pPed) &&  pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN )
	{
		for(int i=0;i<pVehicle->GetSeatManager()->GetMaxSeats();i++)
		{
			CPed* pPassenger=pVehicle->GetSeatManager()->GetPedInSeat(i);
			if(pPassenger && pPassenger!=pPed && !pPassenger->GetPedConfigFlag( CPED_CONFIG_FLAG_StayInCarOnJack ) && pPassenger->PopTypeIsRandom())
			{
				CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(*pVehicle, NULL, pPed);
				break;
			}
		}
	}

#if KEYBOARD_MOUSE_SUPPORT

	eMenuPref eMouseSteeringPref = PREF_MOUSE_DRIVE;

	if (pPed->GetMyVehicle()->GetIsAircraft())
	{
		eMouseSteeringPref = PREF_MOUSE_FLY;
	}
	else if (pPed->GetMyVehicle()->InheritsFromSubmarine() || (pPed->GetMyVehicle()->InheritsFromSubmarineCar() && pPed->GetMyVehicle()->IsInSubmarineMode()))
	{
		eMouseSteeringPref = PREF_MOUSE_SUB;
	}

	// Handle driving/camera toggle.
	bool bDriveCameraToggleOn = false;
	if (pControl->WasKeyboardMouseLastKnownSource() && pPed->GetIsDrivingVehicle() && (CControl::GetMouseSteeringMode(eMouseSteeringPref) != CControl::eMSM_Off) )
	{
		// Use right mouse mouse for rhino tank.
		bool bSecondaryToggleDriveCamera = false;
		if(pPed->GetMyVehicle()->GetModelIndex() == MI_TANK_RHINO || (MI_TANK_KHANJALI.IsValid() && pPed->GetMyVehicle()->GetModelIndex() == MI_TANK_KHANJALI))
		{
			bSecondaryToggleDriveCamera = true;
		}

		const bool bSecondaryAttackButton = pPed->GetMyVehicle()->GetIsAircraft() || pPed->GetMyVehicle()->InheritsFromSubmarine() || (pPed->GetMyVehicle()->InheritsFromSubmarineCar() && pPed->GetMyVehicle()->IsInSubmarineMode());

		bool bVehicleToggleDriveCameraControlDown = pControl->GetVehicleMouseControlOverride().IsDown();
		
		if (bSecondaryAttackButton)
		{
			if (pPed->GetMyVehicle()->GetIsAircraft())
			{
				bVehicleToggleDriveCameraControlDown = pControl->GetVehicleFlyMouseControlOverride().IsDown();
			}
			else if (pPed->GetMyVehicle()->InheritsFromSubmarine() || (pPed->GetMyVehicle()->InheritsFromSubmarineCar() && pPed->GetMyVehicle()->IsInSubmarineMode()))
			{
				bVehicleToggleDriveCameraControlDown = pControl->GetVehicleSubMouseControlOverride().IsDown();
			}
		}

		if(bVehicleToggleDriveCameraControlDown && (bSecondaryToggleDriveCamera || bSecondaryAttackButton || (pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->IsAiming())))
		{
			bDriveCameraToggleOn = true;
		}
	}

	if(bDriveCameraToggleOn )
	{
		if(!CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())
		{
			CControlMgr::GetMainPlayerControl().SetDriveCameraToggleOn(true);
			CControlMgr::GetMainPlayerControl().SetUseSecondaryDriveCameraToggle(true);
			CControlMgr::ReInitControls();
		}
	}
	else if(CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())
	{
		CControlMgr::GetMainPlayerControl().SetDriveCameraToggleOn(false);
		CControlMgr::GetMainPlayerControl().SetUseSecondaryDriveCameraToggle(false);
		CControlMgr::ReInitControls();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	// Reset state transition variables
	m_iExitVehicleRunningFlags.BitSet().Reset();

	// Disable player lockon while driving - will re-evaluate this given more time // Tez enabled - B*32831
	TUNE_GROUP_BOOL(VEHICLE_LOCK_ON_TEST, TURN_ON_LOCKON_IN_VEHICLE, true);
	if (!TURN_ON_LOCKON_IN_VEHICLE)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerLockon, true );

	// Check if any peds are about to be run over by the player's vehicle, and if so,
	// notify them so they can evade or brace for impact.
	// Note: it's possible that we could move this into some of the states rather
	// than doing it here, but if there is any chance that the vehicle is still moving,
	// it's probably best to do it.
	ScanForPedDanger();	

	if (((pVehicle->m_nVehicleFlags.GetIsSirenOn() && pVehicle->GetModelIndex() != MI_CAR_TOWTRUCK 
		&& pVehicle->GetModelIndex() != MI_CAR_TOWTRUCK_2 && pVehicle->GetModelIndex() != MI_CAR_PBUS2)
		|| pVehicle->m_nVehicleFlags.bActAsIfHasSirenOn)
		&& pVehicle->GetIntelligence()->GetSirenReactionDistributer().IsRegistered() 
		&& pVehicle->GetIntelligence()->GetSirenReactionDistributer().ShouldBeProcessedThisFrame()
		&& pVehicle->UsesSiren())
	{
		//static fn call
		CTaskVehicleGoToPointWithAvoidanceAutomobile::MakeWayForCarWithSiren(pVehicle);	
	}

	if (pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() || pVehicle->IsTank()
		|| (pVehicle->m_nVehicleFlags.GetIsSirenOn() && pVehicle->m_nVehicleFlags.bIsLawEnforcementVehicle))
	{
		pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}

	//Update the searchlight turret if we still want to be able to do drivebys
	ControlMountedSearchLight(pPed, pVehicle);

	// Add shocking events for the player being in certain vehicles (tanks for example).
	if (pVehicle->GetIsConsideredDangerousForShockingEvents())
	{
		m_fDangerousVehicleCounter += GetTimeStep();
		if (m_fDangerousVehicleCounter > sm_Tunables.m_TimeBetweenAddingDangerousVehicleEvents)
		{
			m_fDangerousVehicleCounter = 0.0f;

			CEventShockingInDangerousVehicle event(*pVehicle);
			CShockingEventsManager::Add(event);
		}
	}

	//B*1762316: Disable weapon switching if local player ped has phone out while driving in multiplayer
	if (pPed->IsLocalPlayer() && NetworkInterface::IsGameInProgress() && CPhoneMgr::IsDisplayed())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
	}

	if (pPed->IsLocalPlayer() && pVehicle->GetIsLandVehicle() && pVehicle->IsDriver(pPed))
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile::FindPlayerNearMissTargets(pVehicle, pPed->GetPlayerInfo(), true);
	}


	// Force exit if ped is in a hanging seat and in water.
	const s32 iSeatIndex = pVehicle->GetSeatManager() ? pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) : -1;
	const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
	if(pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
	{
		CInWaterEventScanner& rWaterScanner = pPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
		if (rWaterScanner.IsDrowningInVehicle(*pPed))
		{
			pPed->KnockPedOffVehicle(false, 0.0f, 1000);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerDrive::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Init)
		FSM_OnUpdate
			return Init_OnUpdate(pPed);

	FSM_State(State_DriveBasic)
		FSM_OnEnter
			DriveBasic_OnEnter(pPed);
		FSM_OnUpdate
			return DriveBasic_OnUpdate(pPed);

	FSM_State(State_Gun)
		FSM_OnEnter
			Gun_OnEnter(pPed);
		FSM_OnUpdate
			return Gun_OnUpdate(pPed);

	FSM_State(State_MountedWeapon)
		FSM_OnEnter
			MountedWeapon_OnEnter(pPed);
		FSM_OnUpdate
			return MountedWeapon_OnUpdate(pPed);

	FSM_State(State_Projectile)
		FSM_OnEnter
			Projectile_OnEnter(pPed);
		FSM_OnUpdate
			return Projectile_OnUpdate(pPed);

	FSM_State(State_ExitVehicle)
		FSM_OnEnter
			ExitVehicle_OnEnter(pPed);
		FSM_OnUpdate
			return ExitVehicle_OnUpdate(pPed);

	FSM_State(State_ShuffleSeats)
		FSM_OnEnter
			ShuffleSeats_OnEnter(pPed);
		FSM_OnUpdate
			return ShuffleSeats_OnUpdate(pPed);
		FSM_OnExit
			ShuffleSeats_OnExit();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return Finish_OnUpdate(pPed);
FSM_End
}

CTask::FSM_Return CTaskPlayerDrive::Init_OnUpdate(CPed* pPed)
{
	// Player will always get out standing...
	pPed->SetIsCrouching(false);

	//clear out any old route it might have
	if (pPed->GetMyVehicle())
	{
		pPed->GetMyVehicle()->GetIntelligence()->m_BackupFollowRoute.Invalidate();
	}

	// in a network game we must wait for the vehicle to migrate to our machine before it can be given a vehicle AI task
	if (aiVerify(pPed->GetMyVehicle()))
	{
		AI_LOG_IF_PLAYER(pPed, "CTaskPlayerDrive::Init_OnUpdate - Setting state to DriveBasic for ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetState(State_DriveBasic);
	}
	else if (pPed->IsLocalPlayer())
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}
void CTaskPlayerDrive::DriveBasic_OnEnter(CPed* pPed)
{
	if (!pPed->GetMyVehicle())
	{
		return;
	}

	if (pPed->GetMyVehicle()->m_nVehicleFlags.bBlownUp && pPed->GetMyVehicle()->GetVehicleModelInfo() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES))
	{
		SetForceExit(true);
	}

	// Don't create a drive task if the vehicle has no driver
	if (taskVerifyf(pPed->GetMyVehicle()->GetLayoutInfo(), "NULL layout info") && pPed->GetMyVehicle()->GetLayoutInfo()->GetHasDriver())
	{
		aiTask* pNewTask = rage_new CTaskInVehicleBasic(pPed->GetMyVehicle());
		SetNewTask(pNewTask);

        m_bVehicleDriveTaskAddedOrNotNeeded = false;
	}

	m_uTimeSinceStealthNoise = 0;
}

CTask::FSM_Return  CTaskPlayerDrive::DriveBasic_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Not expecting subtask to quit
		// If it does then lets just quit straight out
		return FSM_Quit;
	}

	CTaskMobilePhone::PhoneMode phoneMode = CPhoneMgr::CamGetState() ? CTaskMobilePhone::Mode_ToCamera : CTaskMobilePhone::Mode_ToText; 
	if (CTaskMobilePhone::CanUseMobilePhoneTask(*pPed, phoneMode ) )
	{
		if ( CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed) )
		{
			CTaskMobilePhone::CreateOrResumeMobilePhoneTask(*GetPed());
		}
	}

	//Grab the vehicle.
	CVehicle* pVehicle = pPed->GetMyVehicle();

	if (pVehicle->GetIsUsingScriptAutoPilot())
	{
		m_bVehicleDriveTaskAddedOrNotNeeded = true;
	}

	//Check if the vehicle drive task has been added.
    if(m_bVehicleDriveTaskAddedOrNotNeeded)
    {
		//Ensure the vehicle is valid.
		if(pVehicle)
		{
			//Ensure the vehicle has a task.
			if(!pVehicle->GetIntelligence()->GetActiveTask())
			{
				m_bVehicleDriveTaskAddedOrNotNeeded = false;
			}
		}
    }
    
    if(!m_bVehicleDriveTaskAddedOrNotNeeded)
    {
		//set the player drive task on the vehicle		
		if (pVehicle && taskVerifyf(pVehicle->GetLayoutInfo(), "NULL layout info") && pVehicle->GetLayoutInfo()->GetHasDriver())
		{
			if(pVehicle->IsDriver(pPed) && pPed->IsPlayer() && !pVehicle->IsNetworkClone())//only create a vehicle driving task if the ped is a player and the driver.
			{
				pVehicle->SetPlayerDriveTask(pPed);
				m_bVehicleDriveTaskAddedOrNotNeeded = true;
			}
		}
		else 
		{
			m_bVehicleDriveTaskAddedOrNotNeeded = true;
		}
    }
	
	//Check for reload
	weaponAssert(pPed->GetWeaponManager());	
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	dev_float TIME_BEFORE_FREE_RELOAD = 2.0f;
	if(pWeapon && GetTimeInState() > TIME_BEFORE_FREE_RELOAD)
	{
		if (pWeapon->GetCanReload())
			pWeapon->DoReload();		
	}


	ProcessStealthNoise();

	const bool bIsDriver = pVehicle && pVehicle->IsDriver(pPed);
	// See if we can shuffle jack the ambient driver if we didn't manage to do it as part of the entry task B*1874330
	if (NetworkInterface::IsGameInProgress() && pVehicle)
	{
		if (!bIsDriver)
		{
			CPed* pDriver = pVehicle->GetDriver();
			if (pDriver && !pDriver->IsAPlayerPed() && !pDriver->PopTypeIsMission() && CTaskVehicleFSM::CanJackPed(pPed, pDriver) )
			{
				const s32 iCurrentSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				const s32 iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iCurrentSeatIndex, pVehicle);
				if (pVehicle->IsEntryIndexValid(iEntryPointIndex))
				{

					// Make sure the shuffle seat is the drivers seat!
					s32 iShuffleSeat = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, iCurrentSeatIndex);
					if (pVehicle->IsSeatIndexValid(iShuffleSeat))
					{
						const CVehicleSeatAnimInfo* pShuffleSeatClipInfo = pVehicle->GetSeatAnimationInfo(iShuffleSeat);
						bool bPreventShuffleJack = pShuffleSeatClipInfo && pShuffleSeatClipInfo->GetPreventShuffleJack();

						if (iShuffleSeat == pVehicle->GetDriverSeat() && !bPreventShuffleJack)
						{
							m_iTargetShuffleSeatIndex = iShuffleSeat;
							SetState(State_ShuffleSeats);
							return FSM_Continue;
						}
					}
				}
			}
		}
	}

	// See if we want to change state. 
	// Allow the player to exit at any time, but we must wait in this state until the vehicle ai task has been assigned (if the player is the driver)
	s32 iDesiredState = GetDesiredState(pPed);
	if (iDesiredState != GetState() && (m_bVehicleDriveTaskAddedOrNotNeeded || (pVehicle && pVehicle->GetIsAttached()) || !bIsDriver || iDesiredState == State_ExitVehicle)) //vehicles being towed need to still be allowed to do things
	{
		SetState(iDesiredState);
	}

	return FSM_Continue;
}

void CTaskPlayerDrive::Gun_OnEnter(CPed* pPed)
{
	m_bExitCarWhenGunTaskFinishes = false;

	u32 uFiringPatternHash = 0;

#if __DEV
	static dev_u32 FIRING_PATTERN_OVERRIDE = 0;
	if(FIRING_PATTERN_OVERRIDE != 0)
	{
		uFiringPatternHash = FIRING_PATTERN_OVERRIDE;
	}
#endif // __DEV

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);

	aiTask* pNewTask = rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Player, uFiringPatternHash);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskPlayerDrive::Gun_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		AI_LOG_IF_PLAYER(pPed, "CTaskPlayerDrive::Gun_OnUpdate - Setting DriveBasic for ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetState(State_DriveBasic);
		return FSM_Continue;
	}

	// See if we want to change state
	s32 iDesiredState = GetDesiredState(pPed);
	if(iDesiredState != GetState())
	{
		m_bIsExitingGunState = true;

		if (iDesiredState == State_DriveBasic)
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
			if (pTask && static_cast<CTaskAimGunVehicleDriveBy*>(pTask)->GetIsPlayerAimMode(pPed))
				return FSM_Continue;
		}

		if (iDesiredState == State_ExitVehicle)
			m_bExitCarWhenGunTaskFinishes = true;

		// Try to make the drive task abort
		if(taskVerifyf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN,"NULL or unexpected subtask running"))
		{
			// Request termination - the vehicle gun task will terminate soon on this request
			GetSubTask()->RequestTermination();
		}
	}

	return FSM_Continue;
}

void CTaskPlayerDrive::MountedWeapon_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	aiTask* pNewTask = rage_new CTaskVehicleMountedWeapon(CTaskVehicleMountedWeapon::Mode_Player, NULL, PI);
	SetNewTask(pNewTask);

	// B*4244756 - Make sure we set a valid default firing pattern - This will allow vehicle weapons using aliased patterns to be set correctly (HUNTER, AKULA, etc.)
	GetPed()->GetPedIntelligence()->GetFiringPattern().SetFiringPattern(FIRING_PATTERN_FULL_AUTO);
}

CTask::FSM_Return CTaskPlayerDrive::MountedWeapon_OnUpdate(CPed* pPed)
{
	//! Try creating phone in 1st person only - seems weird that we can't pull it out.
	if(pPed->IsInFirstPersonVehicleCamera())
	{
		CTaskMobilePhone::PhoneMode phoneMode = CPhoneMgr::CamGetState() ? CTaskMobilePhone::Mode_ToCamera : CTaskMobilePhone::Mode_ToText; 
		if (CTaskMobilePhone::CanUseMobilePhoneTask(*pPed, phoneMode ) )
		{
			if ( CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed) )
			{
				CTaskMobilePhone::CreateOrResumeMobilePhoneTask(*GetPed());
			}
		}
	}

#if ENABLE_MOTION_IN_TURRET_TASK	
	// Force exit if the weapon becomes invalid
	// ... except the Mogul. We can't drive the turret to the correct angle (it's destroyed) to align jump out animations properly, so prevent an automatic exit and instead warp when we manually try to exit.
	const CVehicle* pVeh = pPed->GetMyVehicle();
	const bool bIsMogul = pVeh && MI_PLANE_MOGUL.IsValid() && pVeh->GetModelIndex() == MI_PLANE_MOGUL;
	const bool bPreventExitVehicleDueToInvalidWeapon = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventVehExitDueToInvalidWeapon);
	if(!bPreventExitVehicleDueToInvalidWeapon && pVeh && pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE) && !bIsMogul)
	{
		if (pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET))
		{
			TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_KNOCK_PED_OFF_VEHICLE_FOR_STANDING_SEAT, false);
			CInWaterEventScanner& rWaterScanner = pPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
			const bool bDrowningInVehicle = rWaterScanner.IsDrowningInVehicle(*pPed);
			const bool bIsAATrailer = MI_TRAILER_TRAILERSMALL2.IsValid() && pPed->GetMyVehicle()->GetModelIndex() == MI_TRAILER_TRAILERSMALL2;

			const CVehicleSeatAnimInfo* pSeatAimInfo = pVeh->GetSeatAnimationInfo(pPed);
			const bool bDrowning = bDrowningInVehicle || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning);
			bool bCanExitDueToDrowning = true;
			// If there's a driver (and it's not us) first wait for driver to bail due to drowning checks, then we can.
			if (bDrowning && pVeh->IsNetworkClone() && pVeh->InheritsFromPlane() && pVeh->GetDriver() && pVeh->GetDriver() != pPed && !pVeh->GetDriver()->IsFatallyInjured())
			{
				bCanExitDueToDrowning = false;
			}

			if (bCanExitDueToDrowning)
			{
				if (!DISABLE_KNOCK_PED_OFF_VEHICLE_FOR_STANDING_SEAT && bDrowningInVehicle && ((pSeatAimInfo && pSeatAimInfo->IsStandingTurretSeat()) || bIsAATrailer))
				{
					pPed->KnockPedOffVehicle(false, 0.0f, 1000);
				}

				if (!CTaskMotionInTurret::HasValidWeapon(*pPed, *pVeh) || bDrowning)
				{
					AI_LOG_WITH_ARGS("[%s] Going to State_ExitVehicle from MountedWeapon_OnUpdate - Invalid Weapon Exit - %s Vehicle %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed->GetMyVehicle()), AILogging::GetDynamicEntityNameSafe(pPed->GetMyVehicle()));
					SetState(State_ExitVehicle);
					return FSM_Continue;
				}
			}
		}
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK	

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		AI_LOG_IF_PLAYER(pPed, "CTaskPlayerDrive::MountedWeapon_OnUpdate - Setting DriveBasic for ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetState(State_DriveBasic);
		return FSM_Continue;
	}

	// See if we want to change state
	s32 iDesiredState = GetDesiredState(pPed);
	if(iDesiredState != GetState())
	{
		SetState(iDesiredState);
	}

	return FSM_Continue;
}

void CTaskPlayerDrive::Projectile_OnEnter(CPed* pPed)
{
#if FPS_MODE_SUPPORTED
	m_bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));
#endif

	CVehicle* myVehicle =  pPed->GetMyVehicle();
	if(myVehicle != NULL)
	{				
		//! If we are in 1st person, then always use CTaskMountThrowProjectile, as our clipsets are only set up for that network.
		bool bHasFirstPersonClips = false;
		if(m_bInFirstPersonVehicleCamera)
		{
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(pWeaponInfo)
			{
				const CVehicleDriveByAnimInfo* pDrivebyClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
				if(pDrivebyClipInfo && (pDrivebyClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID))
				{
					bHasFirstPersonClips=true;
				}
			}
		}

		const CVehicleSeatAnimInfo* pSeatClipInfo = myVehicle->GetSeatAnimationInfo(pPed);	
		if (bHasFirstPersonClips || (pSeatClipInfo->GetDriveByInfo() && pSeatClipInfo->GetDriveByInfo()->GetUseMountedProjectileTask())) 
		{
			bool bRightSide = pPed->GetPlayerInfo() ? pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() : false;
			bool bVehMelee = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);
			aiTask* pNewTask = rage_new CTaskMountThrowProjectile(CWeaponController::WCT_Player, 0, CWeaponTarget(), bRightSide, bVehMelee); 
			SetNewTask(pNewTask);
			return;
		}
	}
		
	aiTask* pNewTask = rage_new CTaskVehicleProjectile(CTaskVehicleProjectile::Mode_Player);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskPlayerDrive::Projectile_OnUpdate(CPed* pPed)
{
	bool bVehMelee = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) && !bVehMelee)
	{
		// Go back to basic drive task
		AI_LOG_IF_PLAYER(pPed, "CTaskPlayerDrive::Projectile_OnUpdate - Setting DriveBasic for ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetState(State_DriveBasic);
		return FSM_Continue;
	}

#if FPS_MODE_SUPPORTED
	bool bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));
	bool bCameraChanged = (bInFirstPersonVehicleCamera != m_bInFirstPersonVehicleCamera);
	bool bCamNotifiedChange = pPed->GetPlayerInfo() && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);

	// Restart the projectile state if camera mode changes between TPS and FPS
	if (!m_bRestarting && (bCameraChanged || bCamNotifiedChange))
	{
		if (bVehMelee && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOUNT_THROW_PROJECTILE)
		{
			CTaskMountThrowProjectile* pSubTask = static_cast<CTaskMountThrowProjectile*>(GetSubTask());
			pSubTask->SetVehMeleeRestarting(true);
		}

		SetFlag(aiTaskFlags::RestartCurrentState);
		m_bRestarting = true;
		return FSM_Continue;
	}

	m_bRestarting = false;

	m_bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));
#endif	//FPS_MODE_SUPPORTED

	return FSM_Continue;
}

void CTaskPlayerDrive::ExitVehicle_OnEnter(CPed* pPed)
{
	m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::PlayerControlledVehicleEntry);

	fwMvClipSetId streamedExitClipSetId = CLIP_SET_ID_INVALID;
	//fwMvClipId streamedExitClipId = CLIP_ID_INVALID;
	
	const CVehicleClipRequestHelper* pClipRequestHelper = CTaskVehicleFSM::GetVehicleClipRequestHelper(pPed);
	const CConditionalClipSet* pClipSet = pClipRequestHelper ? pClipRequestHelper->GetConditionalClipSet() : NULL;

	if (pClipSet)
	{
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::IsStreamedExit);
		streamedExitClipSetId = pClipSet->GetClipSetId();
	}

	bool bJumpOutUnderWater = false;
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(pVehicle->InheritsFromSubmarine() && (pVehicle->m_nVehicleFlags.bIsDrowning || pVehicle->GetWasInWater()))
	{
		Vector3 vNewTargetPosition(Vector3::ZeroType);
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
	
		//! Find get in.
		if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, pVehicle->GetDirectEntryPointIndexForPed(GetPed()), CExtraVehiclePoint::GET_IN), "Couldn't compute climb up offset")
		{
			static dev_float s_AllowedWaterOverlap = 6.0f;
			static dev_float s_TestDistance = 20.0f;
			float fWaterHeight = 0.0f;
			Vector3 vStartPos(vNewTargetPosition);
			vStartPos.z += s_AllowedWaterOverlap;
			if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(vStartPos, &fWaterHeight, true, POOL_DEPTH, s_TestDistance, NULL))
			{
				if( (vNewTargetPosition.z + s_AllowedWaterOverlap) <= fWaterHeight)
				{
					bJumpOutUnderWater = true;
				}
			}
		}
	}

	//clear out any old route it might have
	if (pVehicle)
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();

#if ENABLE_MOTION_IN_TURRET_TASK
		// Force jump out if ped can no longer use the turret
		const CVehicle& rVeh = *pVehicle;
		const s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(pPed);
		if (rVeh.IsTurretSeat(iSeatIndex))
		{
			CInWaterEventScanner& rWaterScanner = pPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
			const bool bDrowningInVehicle = rWaterScanner.IsDrowningInVehicle(*pPed);

			if (!CTaskMotionInTurret::HasValidWeapon(*pPed, rVeh) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning) || bDrowningInVehicle)
			{
				m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
				m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);

				const bool bIsMogul = MI_PLANE_MOGUL.IsValid() && rVeh.GetModelIndex() == MI_PLANE_MOGUL;
				if (bIsMogul)
				{
					m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
				}
			}
		}	
#endif // ENABLE_MOTION_IN_TURRET_TASK
	}
	
	if (bJumpOutUnderWater)
	{
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::ExitUnderWater);
	}

	m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::WaitForCarToSettle);
	m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked);

	float fDelayTime = 0.0f;

	// Delay exit vehicle for short amount of time if using a lowrider alt clipset (arm on window). 
	// Ensures we play the transition back to the normal steering pose before exiting.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans))
	{
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAltClipsetExitVehicleDelayTime, 0.5f, 0.0f, 1.0f, 0.01f);
		fDelayTime = fAltClipsetExitVehicleDelayTime;
	}

	CTaskExitVehicle* pExitTask = rage_new CTaskExitVehicle(pVehicle, m_iExitVehicleRunningFlags, fDelayTime);
	pExitTask->SetStreamedExitClipsetId(streamedExitClipSetId);

	SetNewTask(pExitTask);

	bool bUsingAutoPilot = pVehicle ? pVehicle->GetIsUsingScriptAutoPilot() : false;

	//set a no driver task
	if (!bUsingAutoPilot && pVehicle->IsDriver(pPed) && !pVehicle->IsNetworkClone())
	{
		aiTask *pTask = rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_PLAYER_DISABLED);
		pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}

	if(pPed->IsLocalPlayer())
	{
		g_AudioScannerManager.SetTimeToPlayCDIVehicleLine();
	}
}

CTask::FSM_Return CTaskPlayerDrive::ExitVehicle_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		SetState(State_Finish);		
	}

	return FSM_Continue;
}

void CTaskPlayerDrive::ShuffleSeats_OnEnter(CPed* pPed)
{
	if (pPed->GetMyVehicle())
	{
		SetNewTask(rage_new CTaskInVehicleSeatShuffle(pPed->GetMyVehicle(), NULL, false, m_iTargetShuffleSeatIndex));
	}

	// Set a no driver task if shuffling away from driver seat
	if (pPed->GetMyVehicle()->IsDriver(pPed) && !pPed->GetMyVehicle()->IsNetworkClone())
	{
		aiTask *pTask = rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_PLAYER_DISABLED);
		pPed->GetMyVehicle()->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
}

CTask::FSM_Return CTaskPlayerDrive::ShuffleSeats_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !pPed->GetMyVehicle())
	{
		// Go back to basic drive task
		AI_LOG_IF_PLAYER(pPed, "CTaskPlayerDrive::ShuffleSeats_OnUpdate - Setting DriveBasic for ped [%s] as SubTaskFinished flag set to: %s or IsNotPedsVehicle is:%s \n", AILogging::GetDynamicEntityNameSafe(pPed), BOOL_TO_STRING(GetIsFlagSet(aiTaskFlags::SubTaskFinished)), BOOL_TO_STRING(!pPed->GetMyVehicle()));
		SetState(State_DriveBasic);	
	}

	return FSM_Continue;
}

void CTaskPlayerDrive::ShuffleSeats_OnExit()
{
	m_iTargetShuffleSeatIndex = -1;
}

CTask::FSM_Return CTaskPlayerDrive::Finish_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Quit;
}

s32 CTaskPlayerDrive::GetDesiredState(CPed* pPed)
{
	if (m_bForcedExit)
	{
		m_iExitVehicleRunningFlags.BitSet().Reset();
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
		m_iExitVehicleRunningFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
		AI_LOG_WITH_ARGS("[%s] Going to State_ExitVehicle from GetDesiredState - m_bForcedExit - %s Vehicle %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed->GetMyVehicle()), AILogging::GetDynamicEntityNameSafe(pPed->GetMyVehicle()));
		return State_ExitVehicle;
	}

	taskFatalAssertf(pPed->IsAPlayerPed(), "CTaskPlayerDrive being given to a non-playerped!");
	CControl *pControl = pPed->GetControlFromPlayer();

	CVehicle* pVehicle = pPed->GetMyVehicle();

	if( CGameWorld::GetMainPlayerInfo()->AreControlsDisabled() || CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_EXIT) WIN32PC_ONLY( || SMultiplayerChat::GetInstance().ShouldDisableInputSinceChatIsTyping()))
	{
		m_bExitButtonUp = false;
	}
	else if( !m_bExitButtonUp && !pControl->GetVehicleExit().IsDown())
	{
		m_bExitButtonUp = true;
		m_uTimeSinceLastButtonPress = fwTimer::GetTimeInMilliseconds();
	}

	if (!CGameWorld::GetMainPlayerInfo()->AreControlsDisabled())
	{
		bool bInputPressed = false;
		if (pControl)
		{
			// Hold to shuffle from a driver seat so we don't conflict with headlight switching
			if (pPed->GetIsDrivingVehicle() || (pVehicle && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HOLD_TO_SHUFFLE)))
			{
				bInputPressed = pControl->GetVehicleShuffle().HistoryPressed(CVehicle::ms_uTimeToIgnoreButtonHeldDown) && pControl->GetVehicleShuffle().HistoryHeldDown(CVehicle::ms_uTimeToIgnoreButtonHeldDown);
			}
			else
			{
				bInputPressed = pControl->GetVehicleShuffle().IsPressed();
			}
		}

		if (bInputPressed && (CTaskVehicleFSM::CanPedShuffleToOrFromTurretSeat(pVehicle, pPed, m_iTargetShuffleSeatIndex) || CTaskVehicleFSM::CanPedShuffleToOrFromExtraSeat(pVehicle, pPed, m_iTargetShuffleSeatIndex)))
		{
			return State_ShuffleSeats;
		}
	}

	// Force the player out if a hated player is driving the car
	bool bForcePlayerOut = false;
	//removed so cops and robbers can inhabit the same car.
/*	CVehicle* pVehicle = pPed->GetMyVehicle();
	if( pVehicle && !pVehicle->IsDriver(pPed) && pVehicle->GetDriver() && 
		pVehicle->GetDriver()->IsAPlayerPed() && pPed->GetPedIntelligence()->IsThreatenedBy(*pVehicle->GetDriver()) )
	{
		bForcePlayerOut = true;
	}
*/

	//In MP, it is possible for a ped to end up in a wrecked car if
	//they are getting in while their teammate wrecks it.  It is possible that
	//this safety check should be active for SP as well, but until that is 
	//verified, might as well leave it out for now.
	// Removed for B*1342780, shouldn't be neeed as they should still take damage and die instead/ragdoll outside?
// 	if(pVehicle && (pVehicle->GetStatus() == STATUS_WRECKED) && NetworkInterface::IsGameInProgress() && !pPed->ShouldBeDead())
// 	{
// 		bForcePlayerOut = true;
// 	}

	if( m_bExitButtonUp || bForcePlayerOut )
	{
		u32 iTimePeriod = MIN((fwTimer::GetTimeInMilliseconds() - m_uTimeSinceLastButtonPress),TIME_BUTTON_HELD_TO_EXIT_VEHICLE_WITH_ENGINE_OFF);

		// If we've just blocked the exit due to a recent warp, reduce the cache history on the input so it ignores button presses made inside the block.
		TUNE_GROUP_INT(VEHICLE_EXIT_TUNE, TIME_AFTER_WARP_TO_BLOCK_EXIT, 400, 0, 1000, 1);
		CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if (pPlayerInfo)
		{
			s32 iTimeSinceBlockWarpFinished = fwTimer::GetTimeInMilliseconds() - (pPlayerInfo->GetLastTimeWarpedIntoVehicle() + TIME_AFTER_WARP_TO_BLOCK_EXIT);
			if (iTimeSinceBlockWarpFinished >= 0 && iTimeSinceBlockWarpFinished < (s32)iTimePeriod)
			{
				iTimePeriod = iTimeSinceBlockWarpFinished;
			}
		}

		bool bPlayerEnterExitHeldThenReleased = false;
		bool bForceEngineOff = pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BOAT;
		bool bSwitchOffEngine = bForceEngineOff;

		const bool bOnlyExitOnRelease = pPed->GetPedResetFlag(CPED_RESET_FLAG_OnlyExitVehicleOnButtonRelease);
		if( !bOnlyExitOnRelease && pControl->GetVehicleExit().HistoryHeldDown(iTimePeriod) )
		{
			bPlayerEnterExitHeldThenReleased = true;
			bSwitchOffEngine = true;
		}
		else if( pControl->GetVehicleExit().HistoryReleased(iTimePeriod) )
		{
			bPlayerEnterExitHeldThenReleased = true;
		}

		//-------------------------------------------------------------------------
		// CHECK FOR EXITING CAR
		//-------------------------------------------------------------------------
		if( !pPed->GetIsArrested() &&
			pPed->GetMyVehicle()->GetCarDoorLocks() != CARLOCK_LOCKED_PLAYER_INSIDE &&
			pPed->GetMyVehicle()->CanPedExitCar())
		{
			VehicleEnterExitFlags iCarFlags;
			bool bCanExit = false;
			const bool bAlwaysExitOnPress = NetworkInterface::IsGameInProgress() && !bOnlyExitOnRelease;
			bool bPlayerExitRegardlessOfSpeed = bAlwaysExitOnPress || (bOnlyExitOnRelease && bPlayerEnterExitHeldThenReleased) || pControl->GetVehicleExit().HistoryHeldDown(TIME_BUTTON_HELD_TO_EXIT_VEHICLE_REGARDLESS_OF_SPEED) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyRequireOnePressToExitVehicle);
			
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyRequireOnePressToExitVehicle))
			{
				if (!pPed->GetMyVehicle()->CanPedStepOutCar(pPed))
				{
					iCarFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
				}
			}

			if (m_bExitCarWhenGunTaskFinishes)
			{	
				bPlayerEnterExitHeldThenReleased = true;
				bPlayerExitRegardlessOfSpeed = true;
			}

			const bool bCanExitFromButtonPress = pControl->GetVehicleExit().IsPressed() && (!bOnlyExitOnRelease || pControl->GetVehicleExit().IsReleased());

			if( bPlayerEnterExitHeldThenReleased && pPed->GetMyVehicle()->CanPedStepOutCar(pPed) )
			{
				bCanExit = true;
				AI_LOG_WITH_ARGS("[%s] GetDesiredState - %s Vehicle %s: exiting due to bPlayerEnterExitHeldThenReleased and CanPedStepOutCar()\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
			}
			if( bPlayerEnterExitHeldThenReleased && bPlayerExitRegardlessOfSpeed )
			{
				bCanExit = true;
				const bool bCanStepOutCar = pPed->GetMyVehicle()->CanPedStepOutCar(pPed);
				const bool bCanJumpOutCar = pPed->GetMyVehicle()->CanPedJumpOutCar(pPed);
				if (!bCanStepOutCar && bCanJumpOutCar)
				{
					iCarFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
				}
				if (!bAlwaysExitOnPress || bCanJumpOutCar || !pPed->GetMyVehicle()->IsDriver(pPed))
				{
					iCarFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				}
				AI_LOG_WITH_ARGS("[%s] GetDesiredState - %s Vehicle %s: exiting due to bPlayerEnterExitHeldThenReleased and bPlayerExitRegardlessOfSpeed\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
			}
			else if( bCanExitFromButtonPress &&
				!pPed->GetMyVehicle()->CanPedStepOutCar(pPed) &&
				pPed->GetMyVehicle()->CanPedJumpOutCar(pPed))
			{
				bCanExit = true;
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				AI_LOG_WITH_ARGS("[%s] GetDesiredState - %s Vehicle %s: exiting due to button press and !CanPedStepOutCar() and CanPedJumpOutCar()\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
			}
			else if( !pPed->GetMyVehicle()->IsDriver(pPed) && bCanExitFromButtonPress )
			{
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				bCanExit = true;
				AI_LOG_WITH_ARGS("[%s] GetDesiredState - %s Vehicle %s: passenger exiting due to button press\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
			}

			// Cant get out of trains whilst moving
			if( pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_TRAIN )
			{
				CTrain* pTrain = static_cast<CTrain*>(pPed->GetMyVehicle());
				if( !pTrain->m_nTrainFlags.bAtStation && pTrain->GetVelocity().XYMag() > 0.1f)
				{
					bCanExit = false;
				}
				else
				{
					if (pTrain->m_nTrainFlags.iStationPlatformSides & CTrainTrack::Right)
						iCarFlags.BitSet().Set(CVehicleEnterExitFlags::PreferRightEntry);
					else
						iCarFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);
				}
			}
			else if ( pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BIKE || pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE || pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
			{
				if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet))
				{
					//Disable getting off the bike if currently in the process of putting on a helmet which is in hand. 
					CPedHelmetComponent* pHelmet = pPed->GetHelmetComponent();
					if(pHelmet && pHelmet->IsHelmetInHand())
					{	
						bCanExit = false;
					}
				}
			}
			else if ( pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE )
			{
				// Don't let ped get off in certain states because we need to pedals to be reset properly
				const CTaskMotionOnBicycleController* pMotionOnBicycleControllerTask = static_cast<const CTaskMotionOnBicycleController*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER));
				if (pMotionOnBicycleControllerTask)
				{
					const s32 iBicycleControllerState = pMotionOnBicycleControllerTask->GetState();
					if (iBicycleControllerState == CTaskMotionOnBicycleController::State_PreparingToLaunch || 
						iBicycleControllerState == CTaskMotionOnBicycleController::State_Launching || 
						iBicycleControllerState == CTaskMotionOnBicycleController::State_FixieSkid || 
						iBicycleControllerState == CTaskMotionOnBicycleController::State_TrackStand || 
						iBicycleControllerState == CTaskMotionOnBicycleController::State_ToTrackStandTransition)
					{
						bCanExit = false;
					}
				}
			}
			else if( bSwitchOffEngine )
			{
				// Nothing checked this flag before, and it has been recycled.
				//	iCarFlags |= RF_PlayerTurnEngineOff;
			}

			// Check to see if player has warped into vehicle recently and block exit (button mash prevention)
			if(pPlayerInfo && pPlayerInfo->GetLastTimeWarpedIntoVehicle() + TIME_AFTER_WARP_TO_BLOCK_EXIT > fwTimer::GetTimeInMilliseconds())
			{
				bCanExit = false;
			}

			if (pVehicle && pVehicle->m_bSpecialEnterExit && pPed->IsGoonPed())
			{
				TUNE_GROUP_INT(EXIT_VEHICLE_TUNE, TIME_SINCE_LAST_PRESS_FOR_GOON, 500, 0, 2000, 100);
				const bool bHeldToExit = !(pControl->GetVehicleExit().HistoryPressed(TIME_SINCE_LAST_PRESS_FOR_GOON) && pControl->GetVehicleExit().IsReleased());
				if (bHeldToExit)
				{
					bCanExit = false;
					AI_LOG_WITH_ARGS("[BossGoonVehicle] Preventing Goon player %s[%p] from exiting vehicle %s[%p] due to holding enter/exit button\n", 
						AILogging::GetDynamicEntityNameSafe(pPed), pPed, pVehicle->GetModelName(), pVehicle);
				}
				else
				{
					AI_LOG_WITH_ARGS("[BossGoonVehicle] Goon player %s[%p] will exit vehicle %s[%p] because he tapped enter/exit button\n",
						AILogging::GetDynamicEntityNameSafe(pPed), pPed, pVehicle->GetModelName(), pVehicle);
				}
			}

			if( bForcePlayerOut )
			{
				bCanExit = true;
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				iCarFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);
				AI_LOG_WITH_ARGS("[%s] GetDesiredState - %s Vehicle %s: bForcePlayerOut = true\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
			}

			if( bCanExit )
			{
				// Want to exit vehicle
				m_iExitVehicleRunningFlags = iCarFlags;
				AI_LOG_WITH_ARGS("[%s] Going to State_ExitVehicle from GetDesiredState - %s Vehicle %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle));
				return State_ExitVehicle;
			}
		}
	}

	// If player drivebys are disabled we can only exit the vehicle
	if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanDoDrivebys) || (pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->bCanDoDriveBy))
	{
		return State_DriveBasic;
	}

	// Check in case player wants to driveby
	weaponAssert(pPed->GetWeaponManager());
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();

	bool bOutOfAmmo = true;
	if(pWeaponInfo)
	{
		bOutOfAmmo = pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo);
	}

	const bool bHasWeaponsAtSeat = pVehicle->GetSeatHasWeaponsOrTurrets(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed));
	bool bDriverNeedsToStartEngine = pVehicle->GetDriver() == pPed && 
		(!pVehicle->m_nVehicleFlags.bEngineOn || pVehicle->m_nVehicleFlags.bEngineStarting) && pVehicle->GetVehicleDamage()->GetEngineHealth() > 0.0f &&
		pVehicle->GetVehicleDamage()->GetPetrolTankLevel() > 0.0f && !pVehicle->GetIsAttached();

	if(bHasWeaponsAtSeat && pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != -1 && !bDriverNeedsToStartEngine && !AllowDrivebyWithSearchLight(pPed, pVehicle))
	{
		return State_MountedWeapon;
	}


	// Check for driveby controls
	bool bDrivebyControlsDown = false;
	bool bAimDown = false;
	bool bCheckForSeatShuffle = false;

	taskAssert(pPed->GetMyVehicle()->GetLayoutInfo());
	const bool bVehicleHasDriver = pPed->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

	if (!bVehicleHasDriver && pPed->GetMyVehicle()->GetVehicleType() != VEHICLE_TYPE_TRAIN )
	{
		bDrivebyControlsDown = false;
		bAimDown = pControl->GetPedTargetIsDown();
		bDriverNeedsToStartEngine = false;
		bCheckForSeatShuffle = true;
	}
	else
	{
		if (pPed->GetIsDrivingVehicle())
		{
			bDrivebyControlsDown = (pControl->GetVehicleAim().IsDown() && !pPed->GetMyVehicle()->m_nVehicleFlags.bCarNeedsToBeHotwired) ? true : false;
		}
		else
		{
			bDrivebyControlsDown = pPed->GetPlayerInfo()->IsFiring() || pControl->GetVehicleAim().IsDown();
		}
		//B*1823615: Enable driver aiming in PC builds
		bool bAllowDriverAimOnPCBuild = false;
#if RSG_PC
		bAllowDriverAimOnPCBuild = pWeaponInfo && !pWeaponInfo->GetIsUnarmed();
#endif
		bool bPedIsDriver = pPed->GetMyVehicle()->GetDriver() == pPed;
		bAimDown = ((!bPedIsDriver || bAllowDriverAimOnPCBuild) && ((pControl->GetVehiclePassengerAimDownCheckToggleAim() && !bPedIsDriver) || CPlayerInfo::IsForcedAiming() || (bPedIsDriver && bAllowDriverAimOnPCBuild && CPlayerInfo::IsHardAiming())));
	}

	bool bWantsToDoNormalDriveby = (bDrivebyControlsDown || bAimDown);

	bool bWantsToDoVehicleMelee = false;
	if (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->CanPlayerPerformVehicleMelee())
	{
		if ( pControl->GetVehMeleeHold().IsDown() && 
			(pControl->GetVehMeleeLeft().IsPressed() || pControl->GetVehMeleeRight().IsPressed()) )
		{
			bWantsToDoVehicleMelee = true;
		}
	}

	const bool bHasDrivebyWeapons = pWeaponInfo && pWeaponInfo->CanBeUsedAsDriveBy(pPed) && (!bOutOfAmmo || GetState()==State_Gun);	//if out of ammo but already shooting let taskdriveby end on its own
	if( ( bWantsToDoNormalDriveby || bWantsToDoVehicleMelee) && bHasDrivebyWeapons ) // removed need to start engine check, as gunplay should always take precedence over most any other action [2/14/2012 musson]
	{
		// Check to see if we've got driveby clips
		bool bCanDoDriveby = CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed);

		//if we're a pilot and flying then don't driveby, or if we are a driver in a firetruck
		if(pPed->GetMyVehicle()->GetDriver() == pPed)
		{
            CVehicleModelInfo* vmi = pPed->GetMyVehicle()->GetVehicleModelInfo();
            Assert(vmi);
            if (vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DRIVER_NO_DRIVE_BY))
            {
                bCanDoDriveby = false;
            }

			if( pPed->GetMyVehicle()->IsShuntModifierActive() )
			{
				bCanDoDriveby = false;
			}

			//don't do drive by if putting on helmet
			CTaskMotionInAutomobile* pMotionTask = static_cast<CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
			if (pMotionTask)
			{
				if(pMotionTask->GetState() == CTaskMotionInAutomobile::State_PutOnHelmet)
				{
					bCanDoDriveby = false;
				}
			
#if FPS_MODE_SUPPORTED
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || pPed->IsInFirstPersonVehicleCamera())
				{
					if(pMotionTask->GetState() == CTaskMotionInAutomobile::State_StartEngine 
						|| pMotionTask->GetState() == CTaskMotionInAutomobile::State_Hotwiring
						|| pMotionTask->GetState() == CTaskMotionInAutomobile::State_Start
						|| pMotionTask->GetState() == CTaskMotionInAutomobile::State_StreamAssets)
					{
						bCanDoDriveby = false;
					}

					if(pMotionTask->GetState() == CTaskMotionInAutomobile::State_FirstPersonSitIdle && GetTimeRunning() <= 0.1f)
					{
						bCanDoDriveby = false;
					}
				}
#endif
			}
		}

		//or using the horn
		if( (pVehicle->GetDriver() == pPed) && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsPlayerPressingHorn() && 
			!(pPed->GetMyVehicle()->InheritsFromQuadBike() || pPed->GetMyVehicle()->InheritsFromBike() || pPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike()))
		{
			bCanDoDriveby = false;
		}
		
		//or talking on phone
		const bool isPedHoldingPhone =	pPed->GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_MOBILE_PHONE) != NULL; 	
		if (isPedHoldingPhone)
		{
			bCanDoDriveby = false;
		}

#if FPS_MODE_SUPPORTED		
		// b*2155901
		const bool isPedInHeli = pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI;
		const bool bIsScopedWeapon = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
#endif // FPS_MODE_SUPPORTED

		//1st person driveby doesn't play mobile phone task, so check UI.
		const camThirdPersonCamera* thirdPersonCamera = camInterface::FindFollowPed() == pPed ? camInterface::GetGameplayDirector().GetThirdPersonCamera() : NULL;

		const bool bIsFirstPersonViewModeActive = thirdPersonCamera &&
													thirdPersonCamera->GetControlHelper() &&
													thirdPersonCamera->GetControlHelper()->GetViewMode() == camControlHelperMetadataViewMode::FIRST_PERSON;
		
		const bool bIsInFirstPersonModeConsideringVehicle = pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->GetIsAircraft() ? 
																bIsFirstPersonViewModeActive : 
																pPed->IsInFirstPersonVehicleCamera();

		bool bUpsideDown = bWantsToDoVehicleMelee ? pPed->GetMyVehicle()->IsUpsideDown() : pPed->GetMyVehicle()->GetTransform().GetC().GetZf() < 0.707f; 
		if(pPed->IsLocalPlayer() && bIsInFirstPersonModeConsideringVehicle && CPhoneMgr::IsDisplayed() )
		{
			bCanDoDriveby = false;
		}
		//or car is upside down/tilted
		else if(!pPed->IsInFirstPersonVehicleCamera() && bUpsideDown && 
			!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim) )
		{
			bCanDoDriveby = false FPS_MODE_SUPPORTED_ONLY(|| (isPedInHeli && bIsScopedWeapon)); // b*2155901
		}

		// Or underwater
		else if (pPed->GetMyVehicle()->m_Buoyancy.GetSubmergedLevel() >= 0.9f)
		{
			bCanDoDriveby = false;
		}

		// Or not in motion task correctly yet. This is to ensure ped is fully in PoV camera before selecting clips.
		else if (pPed->IsLocalPlayer())
		{
			const CTaskMotionInVehicle* pMotionTask = static_cast<const CTaskMotionInVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_VEHICLE));
			if(!pMotionTask)
			{
				bCanDoDriveby = false;
			}
		}

		// Don't allow driveby if the selected weapon is invalid (disabled on the selector)
		if (pPed->GetWeaponManager() && !pPed->GetWeaponManager()->GetWeaponSelector()->GetIsWeaponValid(pPed, pWeaponInfo, true, true))
		{
			bCanDoDriveby = false;
		}

		// Dont do normal drivebys with vehicle melee weapons
		if (!bWantsToDoVehicleMelee && bWantsToDoNormalDriveby && pWeaponInfo && pWeaponInfo->GetCanBeUsedInVehicleMelee())
		{
			bCanDoDriveby = false;
		}

		if(bCanDoDriveby && pWeaponInfo && m_bDriveByClipSetsLoaded)
		{
			if ( pWeaponInfo->GetIsThrownWeapon() ||
				(bWantsToDoVehicleMelee || (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee) && !m_bTryingToInterruptVehMelee)) )
			{
				// Check for throwing grenades
				if(!bOutOfAmmo) 
				{
					if (bWantsToDoVehicleMelee)
					{
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee, true);
						pPed->GetPlayerInfo()->SetVehMeleePerformingRightHit(pControl->GetVehMeleeRight().IsDown());

						INSTANTIATE_TUNE_GROUP_FLOAT( VEHICLE_MELEE, DISABLE_FP_TIMER, 0.0f, 100.0f, 0.1f );
						m_fTimeToDisableFP = DISABLE_FP_TIMER;

						INSTANTIATE_TUNE_GROUP_BOOL( VEHICLE_MELEE, DISABLE_FP );
						if (DISABLE_FP)
							camInterface::GetCinematicDirector().DisableFirstPersonInVehicleThisUpdate();
					}

					return State_Projectile;
				}
			}
			else
			{
				return State_Gun;
			}
		}
	}

	return State_DriveBasic;

}

void CTaskPlayerDrive::SetState(s32 iState)
{
	BANK_ONLY(taskDebugf2("Frame : %u - %s%s : %p : changed state from %s:%s:%p to %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, GetStateName(iState)));
	aiTask::SetState(iState);
}

void CTaskPlayerDrive::StreamBustedClipGroup( CPed* UNUSED_PARAM(pPed) )
{
	fwMvClipSetId desiredBustedSetId = CLIP_SET_ID_INVALID;
// 	if( pPed->GetMyVehicle() && pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN )
// 	{
// 		// Look up busted clips for this group
// 
// 		const CVehicleSeatAnimationInfo* pSeatAnimationInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
// 		while(pSeatAnimationInfo && desiredBustedSetId == CLIP_SET_ID_INVALID)
// 		{
// 			desiredBustedSetId = pSeatAnimationInfo->GetBustedAnimationGroup();
// 
// 			if(desiredBustedSetId == CLIP_SET_ID_INVALID)
// 			{
// 				pSeatAnimationInfo = pSeatAnimationInfo->GetFallbackAnimationInfo();
// 			}
// 		}
// 	}

	// Stream in the requested clip, or release
	if(desiredBustedSetId == CLIP_SET_ID_INVALID)
	{
		m_chosenAmbientSetId = CLIP_SET_ID_INVALID;
		m_ClipSetRequestHelper.Release();
		m_bClipGroupLoaded = false;
	}
	else
	{
		m_chosenAmbientSetId = desiredBustedSetId;
		m_bClipGroupLoaded = m_ClipSetRequestHelper.Request(m_chosenAmbientSetId);
	}
}

void CTaskPlayerDrive::ScanForPedDanger()
{
	CPed *pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!pVehicle)
	{
		return;
	}

	// This value appears to be what was used in GTAIV (AVOIDBLOCKSIZE_PD * 4.0f, for the player):
	float fAvoidBlockSize = 44.0f;	// MAGIC!

	// Identify scan area. (Block around car)
	const Vec3V pos = pVehicle->GetTransform().GetPosition();
	const float posX = pos.GetXf();
	const float posY = pos.GetYf();
	const float minX = posX - fAvoidBlockSize;
	const float maxX = posX + fAvoidBlockSize;
	const float minY = posY - fAvoidBlockSize;
	const float maxY = posY + fAvoidBlockSize;

	// Unfortunately, the code for making peds evade is a part of CTaskVehicleGoToPointWithAvoidanceAutomobile.
	// Would be good to move, but would require a fair amount of refactoring.
	CTaskVehicleGoToPointWithAvoidanceAutomobile::ScanForPedDanger(pVehicle, minX, minY, maxX, maxY);
}

void CTaskPlayerDrive::PreStreamDrivebyClipSets( CPed* pPed )
{
	weaponAssert(pPed->GetInventory());

	// stream each clipset for all valid weapons ped has
	const CWeaponItemRepository& WeaponRepository = pPed->GetInventory()->GetWeaponRepository();
	for(int i = 0; i < WeaponRepository.GetItemCount(); i++)
	{
		const CWeaponItem* pWeaponItem = WeaponRepository.GetItemByIndex(i);
		if(pWeaponItem)
		{
			const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
			const CVehicleDriveByAnimInfo* pDriveByClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
			if (pDriveByClipInfo)
			{
				// Don't prestream driveby clipsets in SP if weapon is flagged as MP driveby only
				bool bBlockClipSetRequest = (!NetworkInterface::IsGameInProgress() && pWeaponInfo && pWeaponInfo->GetIsDriveByMPOnly());
				if (!bBlockClipSetRequest)
				{
					if(pDriveByClipInfo->GetClipSet() != CLIP_SET_ID_INVALID && 
						!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetClipSet()))
					{
						m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetClipSet());
					}

					if(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID && 
						!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash())))
					{
						m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()));
					}

					if (pDriveByClipInfo->GetFirstPersonVehicleMeleeClipSet() != CLIP_SET_ID_INVALID &&
						!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetFirstPersonVehicleMeleeClipSet()))
					{
						m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetFirstPersonVehicleMeleeClipSet());
					}

					if(pDriveByClipInfo->GetVehicleMeleeClipSet() != CLIP_SET_ID_INVALID && 
						!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetVehicleMeleeClipSet()))
					{
						m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetVehicleMeleeClipSet());
					}
				}
			}
		}
	}

	if (m_MultipleClipSetRequestHelper.RequestAllClipSets())
	{
		m_bDriveByClipSetsLoaded = true;
	}
	else
	{
		m_bDriveByClipSetsLoaded  = false;
	}
}

void CTaskPlayerDrive::ProcessBuddySpeedSpeech( CPed* pPed )
{
	const bool bDisableSpeedComments = CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_CLEAN;
	// Make buddies comment on the speed of the car if the player is driving too slowly, or too quickly.
	bool bResetTimers = true;
	if( !bDisableSpeedComments &&
		pPed->GetMyVehicle() &&
		pPed->GetMyVehicle()->IsDriver(pPed) && 
		pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR &&
		!CGameWorld::GetMainPlayerInfo()->AreControlsDisabled() )
	{
		bool bSaySlow = false;
		bool bSayFast = false;
		float fMoveSpeedSq = pPed->GetMyVehicle()->GetVelocity().Mag2();
		float fBuddyMaxSlowSpeed = BUDDY_SLOW_SPEED_MAX;
		// Work out the speed we consider to be slow, based on who our passenger is
		for( s32 iSeat = 0; iSeat < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pBuddyPed = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(iSeat);
			if( pBuddyPed && pBuddyPed != pPed )
			{
				if( !pBuddyPed->IsMale() )
					fBuddyMaxSlowSpeed = GIRLFRIEND_BUDDY_SLOW_SPEED_MAX;
			}
		}
		if( fMoveSpeedSq > rage::square(BUDDY_FAST_SPEED_MIN) )
		{
			bResetTimers = false;
			m_fTimePlayerDrivingQuickly += fwTimer::GetTimeStep();
			if( m_fTimePlayerDrivingQuickly > BUDDY_SAY_QUICK_TIME )
				bSayFast = true;
		}
		else if( ( fMoveSpeedSq > rage::square(BUDDY_SLOW_SPEED_MIN) ) && ( fMoveSpeedSq < rage::square(fBuddyMaxSlowSpeed) ) )
		{
			bResetTimers = false;
			m_fTimePlayerDrivingSlowly += fwTimer::GetTimeStep();
			if( m_fTimePlayerDrivingSlowly > BUDDY_SAY_SLOW_TIME )
				bSaySlow = true;
		}
		
		bool bSayRadioJustRetuned = false;
#if NA_RADIO_ENABLED
		// See if we've just retuned the radio
		const audRadioStation* station = g_RadioAudioEntity.GetPlayerRadioStation();
		RadioGenre genre = RADIO_GENRE_OFF;
		if (g_RadioAudioEntity.WasVehicleRadioJustRetuned() && station)
		{
			bSayRadioJustRetuned = true;
			genre = station->GetGenre();			
		}
#endif

		if( bSayFast || bSaySlow || bSayRadioJustRetuned)
		{
			for( s32 iSeat = 0; iSeat < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); iSeat++ )
			{
				CPed* pBuddyPed = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(iSeat);
				if( pBuddyPed && pBuddyPed != pPed )
				{
					const bool bBuddySpokenRecently = (pBuddyPed->GetSpeechAudioEntity()->GetTimeLastInScriptedConversation() + 10000) > fwTimer::GetTimeInMilliseconds();
					// Dont say any speed audio if the ped has just spoken a line of dialogue
					if( !bBuddySpokenRecently )
					{
						if( bSayFast && pBuddyPed->RandomlySay("CAR_FAST") )
						{
							bResetTimers = true;
							break;
						}
						else if( bSaySlow && pBuddyPed->RandomlySay("CAR_SLOW") )
						{
							bResetTimers = true;
							break;
						}
#if NA_RADIO_ENABLED
						else if( bSayRadioJustRetuned)
						{
							s32 ig1 = -1;
							s32 ig2 = -1;
							pBuddyPed->GetPedRadioCategory(ig1, ig2);
							if(ig1 != -1 && ig1 == (s32)genre)
							{
								bResetTimers = true;
								pBuddyPed->NewSay("RADIO_LIKE_TRACK");
								break;
							}
						}
#endif
					}
				}
			}
		}
	}

	if( bResetTimers )
	{
		m_fTimePlayerDrivingSlowly = 0.0f;
		m_fTimePlayerDrivingQuickly = 0.0f;
	}
}


void CTaskPlayerDrive::ProcessStealthNoise()
{
	if(m_uTimeSinceStealthNoise < sm_Tunables.m_StealthNoisePeriodMS)
	{
		m_uTimeSinceStealthNoise += fwTimer::GetTimeStepInMilliseconds();
		return;
	}

	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(pVehicle && pVehicle->GetDriver() == pPed)
	{
		VehicleType vt = pVehicle->GetVehicleType();
		if(pVehicle->IsEngineOn() || vt == VEHICLE_TYPE_BICYCLE)
		{
			const float spd = pVehicle->GetVelocityIncludingReferenceFrame().Mag();
			const float spdT = (spd - sm_Tunables.m_StealthSpeedThresholdLow)/Max(sm_Tunables.m_StealthSpeedThresholdHigh - sm_Tunables.m_StealthSpeedThresholdLow, SMALL_FLOAT);
			float noiseRange = 0.f;
			if (spdT > 0.0f)
			{
				const float spdTClamped = Clamp(spdT, 0.0f, 1.0f);

				noiseRange = Lerp(spdTClamped, CMiniMap::sm_Tunables.Sonar.fSoundRange_CarLowSpeed, CMiniMap::sm_Tunables.Sonar.fSoundRange_CarHighSpeed);

				if(vt == VEHICLE_TYPE_BICYCLE)
				{
					noiseRange *= sm_Tunables.m_StealthVehicleTypeFactorBicycles;
				}
			}

			// Removed Sonar Blips for engine speed, too muddy [4/1/2013 mdawe]
			// but still reportStealthNoise if the player is involved
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if (pPlayerInfo && noiseRange > 0.f)
			{
				pPlayerInfo->ReportStealthNoise(noiseRange);
			}

			if (pVehicle->IsHornOn())
			{
				//Don't call CreateSonarBlipAndReportStealthNoise for the CarHorn, as it reduces the noise range based on player stealth
				if(pPlayerInfo)
				{
					pPlayerInfo->ReportStealthNoise(CMiniMap::sm_Tunables.Sonar.fSoundRange_CarHorn);
				}
				if(pPed->IsLocalPlayer())
				{
					CMiniMap::CreateSonarBlip( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), CMiniMap::sm_Tunables.Sonar.fSoundRange_CarHorn, HUD_COLOUR_BLUEDARK, pPed );
				}
			}
		}
	}

	m_uTimeSinceStealthNoise = 0;
}

bool CTaskPlayerDrive::AllowDrivebyWithSearchLight(CPed* pPed, CVehicle* pVehicle)
{
	if(pPed->GetWeaponManager() && pVehicle->GetVehicleWeaponMgr())
	{
		CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		if(pVehicleWeapon && pVehicleWeapon->GetType() == VGT_SEARCHLIGHT &&
			(pVehicle->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed)) == 1 || 
			(pPed->IsPlayer() && !pPed->GetPlayerInfo()->GetCanUseSearchLight())))
		{
			return true;
		}
	}
	return false;
}

void CTaskPlayerDrive::ControlMountedSearchLight(CPed* pPed, CVehicle* pVehicle)
{
	//Update searchlight if it's the only vehicle weapon on the boat to allow us to do drivebys
	if(pVehicle->GetVehicleWeaponMgr() && (!pPed->IsPlayer() || pPed->GetPlayerInfo()->GetCanUseSearchLight()))
	{
		atArray<const CVehicleWeapon*> weapon;
		pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), weapon);
		if(weapon.GetCount() == 1 && weapon[0]->GetType() == VGT_SEARCHLIGHT)
		{
			Vector3 vTargetPosition;
			const CEntity* pTarget = NULL;
			if(!GetTargetInfo(pPed,vTargetPosition,pTarget))
			{
				// Just point straight forward from the ped
				vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetB());
			}

			// Get all weapons and turrets associated with our seat
			atArray<CVehicleWeapon*> weaponArray;
			atArray<CTurret*> turretArray;
			CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();
			pVehWeaponMgr->GetWeaponsAndTurretsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed),weaponArray,turretArray);

			// See how close we are to target
			int numTurrets = turretArray.size(); 
			float fSmallestAngleToTarget = numTurrets==0 ? 0 : PI;
			bool bIsWithinFiringAngularThreshold = true;
			for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
			{
				turretArray[iTurretIndex]->AimAtWorldPos(vTargetPosition,pPed->GetMyVehicle(), true, true);		

				// Calc angle to target
				// If it is too big we wont fire
				float fAngleToTarget;
				Matrix34 matTurret;
				turretArray[iTurretIndex]->GetTurretMatrixWorld(matTurret,pPed->GetMyVehicle());
				Vector3 vToTarget = vTargetPosition - matTurret.d;

				fAngleToTarget = matTurret.b.Angle(vToTarget);
				if(fAngleToTarget < fSmallestAngleToTarget)
					fSmallestAngleToTarget = fAngleToTarget;

				static dev_float sfAngularThreshold = 14.9f * DtoR;
				bIsWithinFiringAngularThreshold &= turretArray[iTurretIndex]->IsWithinFiringAngularThreshold( sfAngularThreshold );
			}
		}
	}
}

bool CTaskPlayerDrive::GetTargetInfo(CPed* pPed, Vector3& vTargetPosOut,const CEntity*& pTargetOut)
{
	pTargetOut = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
	if( pTargetOut )
	{
		vTargetPosOut = VEC3V_TO_VECTOR3(pTargetOut->GetTransform().GetPosition());
	}
	else
	{
		weaponAssert(pPed->GetWeaponManager());
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if(pWeapon && pWeaponObject)
		{
			Vector3 vecStart;
			Vector3 vecEnd;
			const Matrix34& weaponMatrix = RCC_MATRIX34(pWeaponObject->GetMatrixRef());

			//NOTE: The aim camera should be attached to this ped's vehicle.
			CVehicle* pVehicle = pPed->GetMyVehicle();
			pWeapon->CalcFireVecFromAimCamera(pVehicle, weaponMatrix, vecStart, vecEnd);
			vTargetPosOut = vecEnd;
		}
		else
		{
			if(camInterface::GetScriptDirector().IsRendering() )
			{
				vTargetPosOut = camInterface::GetScriptDirector().GetFrame().GetPosition() + (camInterface::GetScriptDirector().GetFrame().GetFront() * 30.0f);
			}
			else
			{
				camBaseCamera* pVehCam = (camBaseCamera*)camInterface::GetGameplayDirector().GetFollowVehicleCamera();				

				if( pVehCam )
				{
					vTargetPosOut = pVehCam->GetFrame().GetPosition() + (pVehCam->GetFrame().GetFront() * 30.0f);
				}
				else
				{
					vTargetPosOut = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (camInterface::GetFront() * 30.0f);
				}
			}
		}
	}

	Assert(rage::FPIsFinite(vTargetPosOut.x));
	Assert(rage::FPIsFinite(vTargetPosOut.y));
	Assert(rage::FPIsFinite(vTargetPosOut.z));

	return true;
}

bool CTaskPlayerDrive::ShouldProcessPostPreRenderAfterAttachments(CPed* pPed, CVehicle* pVehicle)
{
	return pPed->IsLocalPlayer() && pPed->IsInFirstPersonVehicleCamera() && pVehicle && pVehicle->InheritsFromBicycle();
}

#if !__FINAL
void CTaskPlayerDrive::Debug() const
{
	// Base class
	CTask::Debug();
}

const char * CTaskPlayerDrive::GetStaticStateName( s32 iState )
{
	const char* sStateNames[] = 
	{
		"Init",
		"DriveBasic",	
		"Gun",			
		"MountedWeapon",
		"Projectile",	
		"ExitVehicle",
		"ShuffleSeats",
		"SetPedInVehicle",
		"Finish"
	};
	return sStateNames[iState];
}
#endif

///////////////////////////
// React to running over Ped with vehicle
///////////////////////////

const char* RAN_OVER_PED_CONCERNED_VO = "RUN_OVER_PLAYER";
const char* RAN_OVER_PED_REPLY = "PLAYER_RUN_OVER_RESPONSE";

CTaskReactToRanPedOver::CTaskReactToRanPedOver(CVehicle* pMyVehicle, CPed* pPedRanOver, float fDamageCaused, eActionToTake action)
: m_pMyVehicle(pMyVehicle),
m_pPedRanOver(pPedRanOver),
m_nAction(action),
m_fDamageCaused(fDamageCaused)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_RUNNING_PED_OVER);
}

CTaskReactToRanPedOver::~CTaskReactToRanPedOver()
{
}

CTask::FSM_Return CTaskReactToRanPedOver::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (GetSubTask())
	{
		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			// If ped has been running into an object for some time, then just
			// quit the task we are probably unable to get to the position.
			if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > CStaticMovementScanner::ms_iStaticCounterStuckLimit)
			{
				pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().ResetStaticCounter();
				return FSM_Quit; //ditch here cause we can't finish
			}
		}
		else if(GetSubTask()->GetTaskType()!=CTaskTypes::TASK_ENTER_VEHICLE)
		{
			if(m_pPedRanOver)
			{
				//look at the ran over ped
				pPed->GetIkManager().LookAt(0, m_pPedRanOver, 100, BONETAG_HEAD, NULL );
			}
		}
	}

	return FSM_Continue;
}

//local state machine
CTask::FSM_Return CTaskReactToRanPedOver::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_getOutOfCar)
			FSM_OnEnter
				return GetOutOfCar_OnEnter(pPed);
			FSM_OnUpdate
				return GetOutOfCar_OnUpdate(pPed);

		FSM_State(State_watchPed)
			FSM_OnEnter
				return WatchPed_OnEnter(pPed);
			FSM_OnUpdate
				return WatchPed_OnUpdate();

		FSM_State(State_sayAudio)
			FSM_OnEnter
				return SayAudio_OnEnter();
			FSM_OnUpdate
				return SayAudio_OnUpdate(pPed);

		FSM_State(State_wait)
			FSM_OnEnter
				return Wait_OnEnter();
			FSM_OnUpdate
				return Wait_OnUpdate();

		FSM_State(State_getBackInCar)
			FSM_OnEnter
				return GetBackInCar_OnEnter();
			FSM_OnUpdate
				return GetBackInCar_OnUpdate(pPed);

		FSM_State(State_fight)
			FSM_OnEnter
				return Fight_OnEnter(pPed);
			FSM_OnUpdate
				return Fight_OnUpdate();

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//state functions
CTask::FSM_Return CTaskReactToRanPedOver::Start_OnUpdate(CPed* pPed)
{

	//In some cases we won't have to react at all...
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) 
		|| !m_pPedRanOver 
		|| pPed->IsPlayer() 
		|| pPed->GetPedType() == PEDTYPE_MEDIC 
		|| pPed->GetPedType() == PEDTYPE_COP
		|| pPed->GetPedType() == PEDTYPE_FIRE)
	{
		SetState(State_finish);
		return FSM_Continue;
	}

#if __DEV
	//use this to force a particular reaction in a dev build
	static eActionToTake FORCE_ACTION = ACTION_INVALID;
	if(FORCE_ACTION != ACTION_INVALID)
	{
		m_nAction = FORCE_ACTION;
	}
#endif

	// Only calculate a random action if not already defined
	if(m_nAction == ACTION_INVALID)
	{
		aiTask* pGetInVehicleTask = m_pPedRanOver->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_ENTER_VEHICLE);
		if(pGetInVehicleTask)
		{
			// Is the ran over ped trying to get into our vehicle?  If so, just say audio - don't get out
			if(static_cast<CTaskEnterVehicle*>(pGetInVehicleTask)->GetVehicle() == m_pMyVehicle)
			{
				m_nAction = ACTION_SAY_ANGRY;
				SetState(State_sayAudio);
			}
		}

		if(m_nAction == ACTION_INVALID)
		{
			bool bHasConcernedVO = pPed->GetSpeechAudioEntity()->DoesContextExistForThisPed(RAN_OVER_PED_CONCERNED_VO);

			// initial reaction choice: either drive away fast, or get out of car to check ped
			float fRandomReaction = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

			static float MIN_DAMAGE_TO_REACT = 0.5f;
			const bool bFriendlyPed = pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPedRanOver);

			if(m_fDamageCaused > MIN_DAMAGE_TO_REACT)
			{
				if(fRandomReaction < 0.5f || bFriendlyPed)
				{
					//get out of car (if possible) and check on ped

					//check the drivers door is able to open, we don't want to be kicking any passengers out
					int iSeatRequest = m_pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
					Assert(iSeatRequest != -1);
					int iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pMyVehicle, SR_Specific, iSeatRequest, 0, true);

					if(!bFriendlyPed && (fRandomReaction < 0.15f || !bHasConcernedVO))
					{
						if(iTargetEntryPoint > 0)
							m_nAction = ACTION_GET_OUT_ANGRY;
						else
							m_nAction = ACTION_SAY_ANGRY;
					}
					else
					{
						if(iTargetEntryPoint > 0)
							m_nAction = ACTION_GET_OUT_CONCERNED;
						else
							m_nAction = ACTION_SAY_CONCERNED;
					}
				}
				else if(fRandomReaction < 0.75f && bHasConcernedVO)
				{
					m_nAction = ACTION_SAY_CONCERNED;
				}
				else
				{
					m_nAction = ACTION_SAY_ANGRY;
				}
			}
			else
			{
				if(!bFriendlyPed && (fRandomReaction < 0.5f || !bHasConcernedVO))
				{
					m_nAction = ACTION_SAY_ANGRY;
				}
				else
				{
					m_nAction = ACTION_SAY_CONCERNED;
				}
			}
		}
	}
	switch(m_nAction)
	{
	case ACTION_GET_OUT_ANGRY:
	case ACTION_GET_OUT_CONCERNED:
		SetState(State_getOutOfCar);
		break;
	case ACTION_SAY_ANGRY:
	case ACTION_SAY_CONCERNED:
		SetState(State_sayAudio);
		break;
	case ACTION_FIGHT:
		SetState(State_fight);
		break;
	default:
		taskAssertf(0, "Ped has chosen an undefined reaction after running over another ped.");
	};
	return FSM_Continue;
}


CTask::FSM_Return CTaskReactToRanPedOver::GetOutOfCar_OnEnter(CPed* pPed)
{
	//Sound shocked
	pPed->NewSay("SHIT");

	//start the get out of car task
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);
	if(m_nAction == ACTION_GET_OUT_ANGRY)
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	}

	SetNewTask(rage_new CTaskExitVehicle(m_pMyVehicle, vehicleFlags));

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::GetOutOfCar_OnUpdate(CPed* pPed)
{
	//if the get out of car sub task is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if (m_nAction==ACTION_GET_OUT_CONCERNED || m_nAction==ACTION_GET_OUT_ANGRY)
			{	//stand still and shout at the ped or be concerned
				SetState(State_watchPed);
			}
			else
			{	//something's gone wrong here, this shouldn't have happened
				taskAssertf(0, "This ped action is invalid in this context.");
				SetState(State_finish);
			}
		}
		else
		{	//the ped has failed to exit the vehicle - just bail here
			SetState(State_finish);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::WatchPed_OnEnter(CPed* pPed)
{	//start the watch player task here

	float fDesiredHeading;
	if(m_pPedRanOver)
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vPedRanOverPosition = VEC3V_TO_VECTOR3(m_pPedRanOver->GetTransform().GetPosition());
		fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vPedRanOverPosition.x, vPedRanOverPosition.y, vPedPosition.x, vPedPosition.y);
	}
	else
	{
		fDesiredHeading = pPed->GetTransform().GetHeading();
	}

	CTaskMoveAchieveHeading*		pMoveSubTask	= rage_new CTaskMoveAchieveHeading(fDesiredHeading);
	CTaskAmbientClips*				pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("REACT_TO_RAN_OVER"));
	CTaskComplexControlMovement*	pNewSubTask		= rage_new CTaskComplexControlMovement(pMoveSubTask, pAmbientTask);

	SetNewTask(pNewSubTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::WatchPed_OnUpdate()
{
	//if the get out of car sub task is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		//say some audio
		SetState(State_sayAudio);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::SayAudio_OnEnter()
{
	const char* audioName;
	u32 audioNameReplyHash = 0;

	//determine the appropriate audio to say
	switch(m_nAction)
	{
	case ACTION_SAY_CONCERNED:
	case ACTION_GET_OUT_CONCERNED:
		{
			audioName = RAN_OVER_PED_CONCERNED_VO;
			audioNameReplyHash = atStringHash(RAN_OVER_PED_REPLY);
		}
		break;
	case ACTION_SAY_ANGRY:
	case ACTION_GET_OUT_ANGRY:
	case ACTION_FIGHT:
	default:
		{
			if(m_pMyVehicle.Get()==NULL)
			{
				audioName = "CRASH_GENERIC";
			}
			else
			{
				switch(m_pMyVehicle->GetVehicleType())
				{
				case VEHICLE_TYPE_CAR:
					{
						if(m_pMyVehicle->GetVehicleAudioEntity() && m_pMyVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
						{
							if(static_cast<audCarAudioEntity*>(m_pMyVehicle->GetVehicleAudioEntity())->CanBeDescribedAsACar())
								audioName = "CRASH_CAR";
							else
								audioName = "CRASH_GENERIC";
							break;
						}
					}

				case VEHICLE_TYPE_BIKE:
					{
						audioName = "CRASH_BIKE";
						break;
					}

				default:
					{
						audioName = "CRASH_GENERIC";
						break;
					}
				}
			}
		}
	}

	aiTask* pNewSubTask = NULL;

	pNewSubTask = rage_new CTaskSayAudio(audioName, 1.0f, 1.0f, m_pPedRanOver,audioNameReplyHash);

	SetNewTask(pNewSubTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::SayAudio_OnUpdate(CPed* pPed)
{
	//if the get out of car sub task is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		//is the ped out of his car?
		if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			//wait for a while before getting back in the car
			SetState(State_wait);
		}
		else
		{
			//we're done, exit the task
			SetState(State_finish);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::Wait_OnEnter()
{	
	int iDuration; 

	if(m_nAction==ACTION_GET_OUT_ANGRY)
	{
		iDuration = fwRandom::GetRandomNumberInRange(2000, 4000);
	}
	else
	{
		iDuration = fwRandom::GetRandomNumberInRange(3000, 7500);
	}

	CTaskMoveStandStill*			pMoveSubTask	= rage_new CTaskMoveStandStill(iDuration);
	CTaskAmbientClips*				pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("REACT_TO_RAN_OVER"));
	CTaskComplexControlMovement*	pNewSubTask		= rage_new CTaskComplexControlMovement( pMoveSubTask, pAmbientTask );

	SetNewTask(pNewSubTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::Wait_OnUpdate()
{
	//if the get out of car sub task is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		//tell the ped to get back in his car
		SetState(State_getBackInCar);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::GetBackInCar_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Set the aggressiveness high.
	pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);

	//Set the ability low.
	pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);

	//Set the mad driver flag.
	if(m_pMyVehicle)
	{
		m_pMyVehicle->m_nVehicleFlags.bMadDriver = true;
	}

	// Return to the vehicle
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
	SetNewTask(rage_new CTaskEnterVehicle(m_pMyVehicle, SR_Specific, 0, vehicleFlags));

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::GetBackInCar_OnUpdate(CPed* pPed)
{
	//if the enter vehicle subtask is now finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{	//ped's back in his vehicle. We're done here
			SetState(State_finish);
		}
		else
		{
			CPed* pOtherDriver = NULL;

			if(m_pMyVehicle)
			{
				pOtherDriver=m_pMyVehicle->GetDriver();
			}
			else
			{
				//the ped's vehicle has mysteriously disappeared. Quit
				SetState(State_finish);
			}

			if(pOtherDriver)
			{
				//some other git has pinched the ped's vehicle. Quit and assume it's been jacked
				SetState(State_finish);
			}
			else
			{
				//haven't managed to make it back in yet. Try waiting for a while before getting back in
				SetState(State_wait);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::Fight_OnEnter(CPed* pPed)
{	
	const char* audioName;


	if(m_pMyVehicle.Get()==NULL)
	{
		audioName = "CRASH_GENERIC";
	}
	else
	{
		switch(m_pMyVehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_CAR:
			{
				if(m_pMyVehicle->GetVehicleAudioEntity() && m_pMyVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
				{
					if(static_cast<audCarAudioEntity*>(m_pMyVehicle->GetVehicleAudioEntity())->CanBeDescribedAsACar())
						audioName = "CRASH_CAR";
					else
						audioName = "CRASH_GENERIC";
					break;
				}
			}

		case VEHICLE_TYPE_BIKE:
			{
				audioName = "CRASH_BIKE";
				break;
			}

		default:
			{
				audioName = "CRASH_GENERIC";
				break;
			}
		}
	}

	pPed->NewSay(audioName);

	SetNewTask(rage_new CTaskThreatResponse(m_pPedRanOver));

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToRanPedOver::Fight_OnUpdate()
{
	//if the combat subtask is now finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//we're done here - exit
		SetState(State_finish);
	}

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskReactToRanPedOver::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_getOutOfCar",
		"State_watchPed",
		"State_getBackInCar",
		"State_sayAudio",
		"State_wait",
		"State_fight",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

void CTaskReactToRanPedOver::CalcTargetPosWithOffset(CPed * pPed, CPed * pInjuredPed)
{
	Assertf(pInjuredPed, "Injured ped null ptr");

	const Vector3 vInjuredPedPosition = VEC3V_TO_VECTOR3(pInjuredPed->GetTransform().GetPosition());
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	m_vTarget = (vPedPosition - vInjuredPedPosition);
	m_vTarget.NormalizeFast();
	m_vTarget = m_vTarget + vInjuredPedPosition;

	// If this position is inside the vehicle, then push out.
	// This can happen if the vehicle takes a little while to come to rest.
	// If this is not done then the ped may end up walking into the vehicle.
	if(m_pMyVehicle)
	{
		CEntityBoundAI bound(*m_pMyVehicle, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
		if(bound.LiesInside(m_vTarget))
		{
			Vector3 vTmp = m_vTarget;
			bound.MoveOutside(vTmp, m_vTarget);
		}
	}
}



aiTask* CTaskReactToRanPedOver::Copy() const
{
	CTaskReactToRanPedOver* pTask = rage_new CTaskReactToRanPedOver(m_pMyVehicle, m_pPedRanOver, m_fDamageCaused, m_nAction);
	return pTask;
}



const int CTaskComplexGetOffBoat::ms_iNumJumpAttempts = 3;

static dev_bool s_bUseNavMeshToGetOffBoat = false;

CTaskComplexGetOffBoat::CTaskComplexGetOffBoat(const int iTimer)
	: CTaskComplex(),
	m_pBoat(NULL),
	m_iNumJumpAttempts(ms_iNumJumpAttempts),
	m_bTriggerAutoVault(false),
	m_bFallBackToNavMeshTask(false),
	m_uTimeLastOnBoat(0)
{
	if(iTimer != -1)
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), iTimer);
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_GET_OFF_BOAT);
}

CTaskComplexGetOffBoat::~CTaskComplexGetOffBoat()
{
}

aiTask * CTaskComplexGetOffBoat::Copy() const
{
	return rage_new CTaskComplexGetOffBoat();
}

aiTask * CTaskComplexGetOffBoat::CreateNextSubTask(CPed * pPed)
{
	switch(GetSubTask()->GetTaskType())
	{
		case CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT:
		{
			CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
			switch(pCtrlMove->GetMoveTaskType())
			{
				case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE:
				{
					if(m_bFallBackToNavMeshTask)
					{
						return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
					}
				}
				//intentional fall through.
				case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
				{
					CBoat * pBoat = ProbeForBoat(pPed);
					if(!pBoat)
						return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);

					if(m_bTriggerAutoVault)
					{
						return CreateSubTask(CTaskTypes::TASK_VAULT, pPed);
					}
					else
					{
						return CreateSubTask(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING, pPed);
					}
				}
				case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
				{
					CBoat * pBoat = ProbeForBoat(pPed);
					if(!pBoat)
						return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);

					return CreateSubTask(CTaskTypes::TASK_JUMPVAULT, pPed);
				}
				case CTaskTypes::TASK_MOVE_GO_TO_POINT:
				{
					// NB: We could potentially restart the task from the beginning here?
					return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
				}
			}
			break;
		}
		case CTaskTypes::TASK_JUMPVAULT:
		{
			CBoat * pBoat = ProbeForBoat(pPed);
			if(!pBoat)
				return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);

			m_iNumJumpAttempts--;
			if(m_iNumJumpAttempts > 0)
			{
				return CreateSubTask(CTaskTypes::TASK_JUMPVAULT, pPed);
			}
			else
			{
				// If we've tried jumping, but have still not left the boat then try just walking forwards for a bit
				return CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
			}
		}
		case CTaskTypes::TASK_VAULT:
		{
			CBoat * pBoat = ProbeForBoat(pPed);
			if(!pBoat)
				return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);

			if(m_bTriggerAutoVault)
			{
				m_bTriggerAutoVault = false;
				//! Ok, still on boat. Fire off another nav mesh task.
				CBoardingPoint bp;
				Vector3 vTargetPos;
				if(static_cast<CBoat*>(m_pBoat.Get())->GetClosestBoardingPoint(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), bp))
				{
					m_vGetOffLocalPos = bp.GetPosition();
					m_fGetOffLocalHeading = bp.GetHeading();
					m_fGetOffLocalHeading = fwAngle::LimitRadianAngle(m_fGetOffLocalHeading);

					if(s_bUseNavMeshToGetOffBoat)
					{
						return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
					}
					else
					{
						return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE, pPed);
					}
				}
			}
			else
			{
				m_iNumJumpAttempts--;
				if(m_iNumJumpAttempts > 0)
				{
					return CreateSubTask(CTaskTypes::TASK_JUMPVAULT, pPed);
				}
				else
				{
					// If we've tried jumping, but have still not left the boat then try just walking forwards for a bit
					return CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
				}
			}
		}
	}
	Assertf(false, "TaskType %i not handled!", GetSubTask()->GetTaskType());
	return NULL;
}

//***************************************************************************
// ProbeForBoat
// This function fires a ray (or bunch of them) downwards under the ped to
// see if we're standing on a boat, and returns pointer to it if we are.
// I originally wanted to use the m_pGroundPhysical pointer, but this can
// sometimes be NULL if the ped happens to get airborne.
// Addendum: I'm now ALSO checking the m_pGroundPhysical pointer, for the
// cases where the ped is standing right on the edge of a boat & the line-
// probe misses it!  :-0

CBoat * CTaskComplexGetOffBoat::ProbeForBoat(CPed * pPed)
{
	if(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT)
		return static_cast<CBoat*>(pPed->GetGroundPhysical());

	const Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fProbeHeight = 5.0f;
	const u32 iFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vStart, vStart - Vector3(0.0f, 0.0f, fProbeHeight));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(iFlags);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
	if(!bHit || !probeResult[0].GetHitInst() || !probeResult[0].GetHitInst()->GetUserData())
		return NULL;

	CEntity* pEntity = (CEntity*)probeResult[0].GetHitInst()->GetUserData();
	if(pEntity->GetIsTypeVehicle() && ((CVehicle*)pEntity)->GetVehicleType()==VEHICLE_TYPE_BOAT)
		return static_cast<CBoat*>(pEntity);

	return NULL;
}

bool CTaskComplexGetOffBoat::CheckForOnBoat(CPed * pPed) 
{
	if(pPed->GetGroundPhysical() == m_pBoat)
	{
		m_uTimeLastOnBoat = fwTimer::GetTimeInMilliseconds();
		return true;
	}
	else
	{
		if(pPed->IsOnGround() && !ProbeForBoat(pPed))
		{
			return false;
		}
	}

	static dev_float s_uTimeOut = 500;
	if(fwTimer::GetTimeInMilliseconds() > (m_uTimeLastOnBoat + s_uTimeOut) && !ProbeForBoat(pPed) )
	{
		return false;
	}

	return true;
}

void CTaskComplexGetOffBoat::WarpPedOffBoat(CPed * pPed)
{
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vPos = vPedPos;
	const u32 iFlags = (GetClosestPos_ClearOfObjects | GetClosestPos_ExtraThorough);
	bool bFoundPos = (CPathServer::GetClosestPositionForPed(vPedPos, vPos, 150.0f, iFlags) != ENoPositionFound);
	Assertf(bFoundPos, "CTaskComplexGetOffBoat::WarpPedOffBoat couldn't find a safe position");

	if(!bFoundPos)
	{
		CNodeAddress addr = ThePaths.FindNodeClosestToCoors(vPedPos);
		if(!addr.IsEmpty())
		{
			const CPathNode * pNode = ThePaths.FindNodePointer(addr);
			if(pNode)
			{
				pNode->GetCoors(vPos);
				Vector3 vDiff = vPos - vPedPos;
				if(vDiff.Mag() < 20.0f)
				{
					bFoundPos = true;
				}
			}
		}
	}

	if(!bFoundPos)
	{
		vPos = vPedPos;
		vPos.x += 10.0f;
		vPos.y += 10.0f;
		Assertf(false, "CTaskComplexGetOffBoat::WarpPedOffBoat couldn't find a good position.");
	}

	vPos.z += 2.0f;
	pPed->Teleport(vPos, pPed->GetCurrentHeading());
}

aiTask * CTaskComplexGetOffBoat::CreateFirstSubTask(CPed * pPed)
{
	m_pBoat = static_cast<CVehicle*>(ProbeForBoat(pPed));
	if(!m_pBoat)
		return NULL;

	CBoardingPoint bp;
	Vector3 vTargetPos;
	if(static_cast<CBoat*>(m_pBoat.Get())->GetClosestBoardingPoint(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), bp))
	{
		m_vGetOffLocalPos = bp.GetPosition();
		m_fGetOffLocalHeading = bp.GetHeading();
		m_fGetOffLocalHeading = fwAngle::LimitRadianAngle(m_fGetOffLocalHeading);

		if(s_bUseNavMeshToGetOffBoat)
		{
			return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
		}
		else
		{
			return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE, pPed);
		}
	}
	else
	{
		// Boarding points aren't going to work on a vehicle like the TUG with no navmesh, warp off
		if(MI_BOAT_TUG.IsValid() && m_pBoat->GetModelIndex() == MI_BOAT_TUG)
		{
			WarpPedOffBoat(pPed);
			return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
		}
		else
		{
			Assertf(false, "Boat %s doesn't have any boarding points, so peds can't get on & off it..", CModelInfo::GetBaseModelInfoName(m_pBoat->GetModelId()));
			return NULL;
		}
	}
}

aiTask * CTaskComplexGetOffBoat::ControlSubTask(CPed * pPed)
{
	if(!m_pBoat)
	{
		return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
	}
	if(m_Timer.IsSet() && m_Timer.IsOutOfTime())
	{
		WarpPedOffBoat(pPed);
		return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
	}

	s32 nTaskType = GetSubTask()->GetTaskType();

	switch(nTaskType)
	{
		case CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT:
		{
			if(!CheckForOnBoat(pPed))
			{
				return CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
			}

			CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
			CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
			if(pMoveTask)
			{
				Assertf(pMoveTask->GetTaskType()==pCtrlMove->GetMoveTaskType(), "GetSubTask() is TASK_COMPLEX_CONTROL_MOVEMENT but GetMoveTaskType() does not match the actual task in the movement slot.");
				switch(pMoveTask->GetTaskType())
				{
					case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
					{
						CTaskMoveAchieveHeading * pAchvHdg = (CTaskMoveAchieveHeading*)pMoveTask;
						const float fHdg = fwAngle::LimitRadianAngle(m_pBoat->GetTransform().GetHeading() + m_fGetOffLocalHeading);
						pAchvHdg->SetHeading(fHdg);
						break;
					}
					case CTaskTypes::TASK_MOVE_GO_TO_POINT:
					{
						CTaskMoveGoToPoint * pGoToTask = (CTaskMoveGoToPoint*)pMoveTask;
						const float fDist = 2.0f;
						Vector3 vGoToTarget = m_pBoat->TransformIntoWorldSpace(m_vGetOffLocalPos);
						const float fHdg = fwAngle::LimitRadianAngle(m_pBoat->GetTransform().GetHeading() + m_fGetOffLocalHeading);
						vGoToTarget.x -= rage::Sinf(fHdg) * fDist;
						vGoToTarget.y += rage::Cosf(fHdg) * fDist;
						pGoToTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vGoToTarget), true);
						break;
					}
					case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE:
					{
						//! Fix up heading if we find ourselves close to target.
						static dev_float s_fSeekEntityXYTolerance = 0.35f;

						Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						vPedPos.z = 0.0f;
						Vector3 vGoToTarget = m_pBoat->TransformIntoWorldSpace(m_vGetOffLocalPos);
						vGoToTarget.z = 0.0f;
						if (vPedPos.Dist2(vGoToTarget) < rage::square(s_fSeekEntityXYTolerance))
						{
							return CreateNextSubTask(pPed);
						}

						//! This task re-calculates target. Note: If we have been in task for so long, we should kick of nav mesh task.
						if(pMoveTask->GetTimeRunning() > 10.0f)
						{
							m_bFallBackToNavMeshTask = true;
							return CreateNextSubTask(pPed);
						}
						break;
					}
					case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
					{
						// If following navmesh & we can't find a route, then skip onto the next subtask
						CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)pMoveTask;
						if(pNavTask->GetNavMeshRouteResult()==NAVMESHROUTE_ROUTE_NOT_FOUND)
						{
							if(pNavTask->MakeAbortable( ABORT_PRIORITY_IMMEDIATE, NULL))
							{
								return CreateNextSubTask(pPed);
							}
						}
					}
				}
			}

			//! Try and auto-climb
			if (pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 10)
			{
				Vector3 vGoToTarget = m_pBoat->TransformIntoWorldSpace(m_vGetOffLocalPos);

				Vector3 vToTarget = vGoToTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				vToTarget.z = 0.f;
				vToTarget.Normalize();

				if(Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), vToTarget) > 0.967f)
				{
					//! Get climb params.
					sPedTestParams pedTestParams;
					CTaskPlayerOnFoot::GetClimbDetectorParam(	CTaskPlayerOnFoot::STATE_INVALID,
						pPed, 
						pedTestParams, 
						true, 
						false, 
						false, 
						false);

					//! Ask the climb detector if it can find a handhold.
					CClimbHandHoldDetected handHoldDetected;
					pPed->GetPedIntelligence()->GetClimbDetector().ResetFlags();
					pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
					pPed->GetPedIntelligence()->GetClimbDetector().Process(NULL, &pedTestParams);
					if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
					{
						if(handHoldDetected.GetPhysical() == m_pBoat)
						{
							m_bTriggerAutoVault = true;
							return CreateNextSubTask(pPed);
						}
					}
				}
			}

			break;
		}
	}
	return GetSubTask();
}

CTask *	CTaskComplexGetOffBoat::CreateSubTask(const int iTaskType, CPed * pPed)
{
	switch(iTaskType)
	{
		case CTaskTypes::TASK_FINISHED:
		{
			return NULL;
		}
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			const Vector3 vWorldSpaceTarget = m_pBoat->TransformIntoWorldSpace(m_vGetOffLocalPos);
			CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(
				MOVEBLENDRATIO_WALK,
				vWorldSpaceTarget,
				1.0f,
				CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
				20000 // 20 secs before warping
			);

			return rage_new CTaskComplexControlMovement(pNavTask);
		}
		case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE:
		{
			Vector3 TargetOffset = m_vGetOffLocalPos;

			float fNavMBR = MOVEBLENDRATIO_WALK;

			static dev_s32 s_MaxSeekTime = 10000; 
			static dev_s32 s_ScanPeriod = 0; 
			TTaskMoveSeekEntityXYOffsetRotated* pSeekTask = rage_new TTaskMoveSeekEntityXYOffsetRotated(m_pBoat, s_MaxSeekTime, s_ScanPeriod);
			CEntitySeekPosCalculatorXYOffsetRotated seekPosCalculator(TargetOffset);
			((TTaskMoveSeekEntityXYOffsetRotated*)pSeekTask)->SetEntitySeekPosCalculator(seekPosCalculator);

			static dev_float GO_TO_CLIMB_RADIUS = 0.3f;

			((TTaskMoveSeekEntityXYOffsetRotated*)pSeekTask)->SetMoveBlendRatio(fNavMBR);
			((TTaskMoveSeekEntityXYOffsetRotated*)pSeekTask)->SetTargetRadius(GO_TO_CLIMB_RADIUS);

			static dev_float s_GoToPointDistance = 100.0f; 
			pSeekTask->SetGoToPointDistance(s_GoToPointDistance);

			return rage_new CTaskComplexControlMovement(pSeekTask);
		}
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			const float fHdg = fwAngle::LimitRadianAngle(m_pBoat->GetTransform().GetHeading() + m_fGetOffLocalHeading);
			CTaskMoveAchieveHeading * pAchvHdg = rage_new CTaskMoveAchieveHeading(fHdg, 8.0f);
			return rage_new CTaskComplexControlMovement(pAchvHdg);
		}
		case CTaskTypes::TASK_VAULT:
		{
			CClimbHandHoldDetected handHoldDetected;
			pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected);
			
			fwFlags8 iVaultFlags;
			iVaultFlags.SetFlag(VF_DontDisableVaultOver);

			return rage_new CTaskVault(handHoldDetected, iVaultFlags);
		}
		case CTaskTypes::TASK_JUMPVAULT:
		{
			if(CTaskJumpVault::WillVault(pPed, JF_ForceVault))
			{
				fwFlags8 iVaultFlags;
				iVaultFlags.SetFlag(VF_DontDisableVaultOver);
				CClimbHandHoldDetected handHoldDetected;
				pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected);
				return rage_new CTaskVault(handHoldDetected, iVaultFlags);
			}
			else
			{
				return rage_new CTaskJumpVault();
			}
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			const float fDist = 2.0f;
			Vector3 vGoToTarget = m_pBoat->TransformIntoWorldSpace(m_vGetOffLocalPos);
			const float fHdg = fwAngle::LimitRadianAngle(m_pBoat->GetTransform().GetHeading() + m_fGetOffLocalHeading);
			vGoToTarget.x -= rage::Sinf(fHdg) * fDist;
			vGoToTarget.y += rage::Cosf(fHdg) * fDist;
			CTaskMoveGoToPoint * pGoToTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, vGoToTarget);
			pGoToTask->SetTargetHeightDelta(1000.0f);	// just getting into the XY target radius will suffice
			return rage_new CTaskComplexControlMovement(pGoToTask);
		}
	}
	Assertf(false, "TaskType %i not handled!", iTaskType);
	return NULL;
}

CTaskControlVehicle::CTaskControlVehicle(CVehicle* pVehicle, aiTask* pVehicleTask, aiTask* pSubtask, fwFlags8 uConfigFlags)
: m_pVehicle(pVehicle), 
m_pDesiredSubTask(NULL),
m_bNewDesiredSubtask(false),
m_pVehicleTask(NULL),
m_pActualVehicleTask(NULL),
m_uConfigFlags(uConfigFlags)
{
	m_pVehicleTask = pVehicleTask;
	SetDesiredSubtask(pSubtask);

	SetInternalTaskType(CTaskTypes::TASK_CONTROL_VEHICLE);

#if __ASSERT
	// make sure the serialised vehicle task does not exceed the maximum task data size 
	if (pVehicleTask && NetworkInterface::IsGameInProgress())
	{
		CTaskVehicleMissionBase* pVehTask = SafeCast(CTaskVehicleMissionBase, pVehicleTask);

		if (pVehTask)
		{
			CSyncDataSizeCalculator serialiser;
			pVehTask->Serialise(serialiser);
			aiAssertf(serialiser.GetSize() <= CClonedControlVehicleInfo::MAX_VEHICLE_TASK_DATA_SIZE, "Vehicle task %s has a serialised data size (%d) that exceeds the max data size (%d)", pVehTask->GetTaskName(), serialiser.GetSize(), CClonedControlVehicleInfo::MAX_VEHICLE_TASK_DATA_SIZE);
		}
	}
#endif // __ASSERT
}


CTaskControlVehicle::~CTaskControlVehicle()
{
	if( m_pDesiredSubTask )
	{
		delete m_pDesiredSubTask;
		m_pDesiredSubTask = NULL;
	}

	if(m_pVehicleTask)
		delete m_pVehicleTask;
}

CTask::FSM_Return CTaskControlVehicle::ProcessPreFSM()
{
	// If the vehicle no longer exists - quit.
	if( !m_pVehicle )
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskControlVehicle::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// Enter the vehicle if the ped starts up outside it
		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return EnterVehicle_OnUpdate(pPed);

		// Start the vehicle engine
		FSM_State(State_StartVehicle)
			FSM_OnEnter
				StartVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StartVehicle_OnUpdate(pPed);

		// Join the vehicle with the road system
		FSM_State(State_JoinWithRoadSystem)
			FSM_OnUpdate
				return JoinWithRoadSystem_OnUpdate(pPed);

		// Main driving state
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pPed);
			FSM_OnUpdate
				return Drive_OnUpdate(pPed);
			
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

bool CTaskControlVehicle::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	//Don't allow abortions if following a recording
	if (pEvent && pEvent->GetEventType() == EVENT_SCRIPT_COMMAND
		&& m_pVehicleTask && m_pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING)
	{
		const CEventScriptCommand* pEventScriptCommand = static_cast<const CEventScriptCommand*>(pEvent);
		if (pEventScriptCommand->GetTask() 
			&& (pEventScriptCommand->GetTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT
				|| pEventScriptCommand->GetTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE)
			)
		{
			return false;
		}
	}

	return CTaskFSMClone::ShouldAbort(priority, pEvent);
}

void CTaskControlVehicle::CleanUp()
{
	CPed *pPed = GetPed();

	if(m_pVehicle && m_pVehicle->IsDriver(pPed) && !m_pVehicle->IsNetworkClone())
	{
		m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, NULL, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
}

bool CTaskControlVehicle::IsVehicleTaskUnableToGetToRoad() const
{
	if (m_pVehicle)
	{
		const CTaskVehicleGoToAutomobileNew* pTaskVehicleGoTo = SafeCast(const CTaskVehicleGoToAutomobileNew, m_pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW));
		if (pTaskVehicleGoTo)
		{
			return pTaskVehicleGoTo->IsUnableToGetToRoad();
		}
	}

	return false;
}

CTaskInfo*	CTaskControlVehicle::CreateQueriableState() const
{
	return rage_new CClonedControlVehicleInfo(GetState(), m_pVehicle, static_cast<CTaskVehicleMissionBase*>(m_pVehicleTask.Get()));
}

void CTaskControlVehicle::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_CONTROL_VEHICLE );

	CClonedControlVehicleInfo *pControlVehicleInfo = static_cast<CClonedControlVehicleInfo*>(pTaskInfo);

	if (m_pVehicleTask && pControlVehicleInfo->GetVehicleTaskType() != 0)
	{
        if(m_pVehicleTask->GetTaskType() != static_cast<s32>(pControlVehicleInfo->GetVehicleTaskType()))
        {
            delete m_pVehicleTask;
            m_pVehicleTask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(pControlVehicleInfo->GetVehicleTaskType()));
        }

        if(m_pVehicleTask)
        {
		    Assert(dynamic_cast<CTaskVehicleMissionBase*>(m_pVehicleTask.Get()));
		    pControlVehicleInfo->ApplyDataToVehicleTask(*static_cast<CTaskVehicleMissionBase*>(m_pVehicleTask.Get()));
        }
	}

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskControlVehicle::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Exit);
}

bool CTaskControlVehicle::ControlPassingAllowed(CPed* pPed, const netPlayer& player, eMigrationType UNUSED_PARAM(migrationType))
{
	// don't allow the ped to migrate if the vehicle has not been cloned on the same machine as the ped
	if (!pPed->IsNetworkClone() && m_pVehicle && m_pVehicle->GetNetworkObject() && !m_pVehicle->IsNetworkClone() && !m_pVehicle->GetNetworkObject()->HasBeenCloned(player))
		return false;

	return true;
}

CTaskFSMClone *CTaskControlVehicle::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed))
{
	CTaskControlVehicle *newTask = NULL;
	
	if (m_pVehicle)
	{
		m_pActualVehicleTask = m_pVehicleTask ? m_pVehicleTask->Copy() : NULL;
		newTask = rage_new CTaskControlVehicle(m_pVehicle, SafeCast(CTaskVehicleMissionBase, m_pActualVehicleTask.Get()));
	}

	return newTask;
}

CTaskFSMClone *CTaskControlVehicle::CreateTaskForLocalPed(CPed *UNUSED_PARAM(pPed))
{
	CTaskControlVehicle *newTask = NULL;
	
	if (m_pVehicle)
	{
		m_pActualVehicleTask = m_pVehicleTask ? m_pVehicleTask->Copy() : NULL;
		newTask = rage_new CTaskControlVehicle(m_pVehicle, SafeCast(CTaskVehicleMissionBase, m_pActualVehicleTask.Get()));
	}

	return newTask;
}

CTask::FSM_Return CTaskControlVehicle::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (GetStateFromNetwork() == State_Exit && iEvent == OnUpdate)
	{
		return FSM_Quit;
	}

	FSM_Begin
	// Initial state
	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdateClone(pPed);

	// Enter the vehicle if the ped starts up outside it
	FSM_State(State_EnterVehicle)
		FSM_OnEnter
		EnterVehicle_OnEnter(pPed);
	FSM_OnUpdate
		return EnterVehicle_OnUpdate(pPed);

	// Start the vehicle engine
	FSM_State(State_StartVehicle)
		FSM_OnEnter
		StartVehicle_OnEnter(pPed);
	FSM_OnUpdate
		return StartVehicle_OnUpdate(pPed);

	// Join the vehicle with the road system
	FSM_State(State_JoinWithRoadSystem)
		FSM_OnUpdate
		return JoinWithRoadSystem_OnUpdateClone(pPed);

	// Main driving state
	FSM_State(State_Drive)
		FSM_OnEnter
		return Drive_OnUpdateClone(pPed);
		FSM_OnUpdate
		return Drive_OnUpdateClone(pPed);

	// Quit
	FSM_State(State_Exit)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

bool CTaskControlVehicle::IsPedInTheCorrectVehicle(CPed* pPed)
{
	return pPed->GetVehiclePedInside() == m_pVehicle && pPed->GetIsDrivingVehicle();
}

CTask::FSM_Return CTaskControlVehicle::Start_OnUpdate( CPed* pPed )
{
	// If the ped is in the correct vehicle - join with the road system
	if( IsPedInTheCorrectVehicle(pPed) )
	{
		if( m_pVehicle->IsEngineOn() )
			SetState(State_JoinWithRoadSystem);
		else
			SetState(State_StartVehicle);
	}
	// Try to enter the vehicle if we're not currently in the correct one
	else
	{
		//Only get back in cars if they aren't upside down.
		if(CUpsideDownCarCheck::IsCarUpsideDown(m_pVehicle.Get()) || CBoatIsBeachedCheck::IsBoatBeached(*m_pVehicle))
		{
			SetState(State_Exit);
		}
		else
		{
			SetState(State_EnterVehicle);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskControlVehicle::Start_OnUpdateClone( CPed* pPed )
{
	if (!m_pVehicle)
	{
		if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CONTROL_VEHICLE))
		{
			return FSM_Quit;
		}

		CClonedControlVehicleInfo* pInfo = static_cast<CClonedControlVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_CONTROL_VEHICLE, PED_TASK_PRIORITY_MAX));

		if (aiVerify(pInfo))
		{
			m_pVehicle = pInfo->GetVehicle();
		}

		if (!m_pVehicle)
		{
			return FSM_Continue;
		}
	}

	if (GetStateFromNetwork() != State_Start && GetStateFromNetwork() != State_Invalid)
	{
		// we must wait for the queriable state update for the enter vehicle task before proceeding, otherwise it will abort prematurely 
		if (GetStateFromNetwork() == State_EnterVehicle && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(!CTaskTypes::TASK_ENTER_VEHICLE))
		{
			return FSM_Continue;
		}

		SetState(GetStateFromNetwork());
	}
	return FSM_Continue;
}

void CTaskControlVehicle::EnterVehicle_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	// Return to the vehicle
	SetNewTask(rage_new CTaskEnterVehicle(m_pVehicle, SR_Specific, m_pVehicle->GetDriverSeat()));
}

CTask::FSM_Return CTaskControlVehicle::EnterVehicle_OnUpdate( CPed* pPed )
{
	// If the subtask has finised
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		// If the ped has succesfully entered the car, carry out the drive task
		if( IsPedInTheCorrectVehicle(pPed) )
		{
			SetState(State_JoinWithRoadSystem);
			return FSM_Continue;
		}
		// The ped has failed to enter the vehicle, quit
		else
		{
			SetState(State_Exit);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}


CTask::FSM_Return CTaskControlVehicle::JoinWithRoadSystem_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if(!m_uConfigFlags.IsFlagSet(CF_IgnoreNewCommand))
	{
		m_pVehicle->GetIntelligence()->UpdateJustBeenGivenNewCommand();
	}

	// we must wait for the vehicle to become local before proceeding
	if (!m_pVehicle->IsNetworkClone())
	{
		SetState(State_Drive);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskControlVehicle::JoinWithRoadSystem_OnUpdateClone( CPed* UNUSED_PARAM(pPed) )
{
	if(!m_uConfigFlags.IsFlagSet(CF_IgnoreNewCommand))
	{
		m_pVehicle->GetIntelligence()->UpdateJustBeenGivenNewCommand();
	}

	if (GetStateFromNetwork() > GetState())
		SetState(GetStateFromNetwork());

	return FSM_Continue;
}

void CTaskControlVehicle::StartVehicle_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	//SetNewTask(rage_new CTaskStartCar(m_pVehicle) );
}

CTask::FSM_Return CTaskControlVehicle::StartVehicle_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	// If the subtask has finised
// 	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
// 	{
		SetState(State_JoinWithRoadSystem);
// 		return FSM_Continue;
// 	}
	return FSM_Continue;
}

void CTaskControlVehicle::Drive_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	taskFatalAssertf(m_pDesiredSubTask, "desired subtask always expected to be valid!");
	SetNewTask( m_pDesiredSubTask->Copy() );
	m_bNewDesiredSubtask = false;
	Assert(!m_pVehicle->IsNetworkClone());

    eVehiclePrimaryTaskPriorities eTaskPriority = VEHICLE_TASK_PRIORITY_PRIMARY;

    //Crash tasks should only be added to the crash task tree
    if(m_pVehicleTask && m_pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRASH)
    {
        eTaskPriority = VEHICLE_TASK_PRIORITY_CRASH;
    }

	m_pActualVehicleTask = m_pVehicleTask ? m_pVehicleTask->Copy() : NULL;
	m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, m_pActualVehicleTask, eTaskPriority, false);
}

CTask::FSM_Return CTaskControlVehicle::Drive_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if(m_pVehicle)
	{
		// If a mission has been achieved - quit
		CTaskVehicleMissionBase *pCarTask = m_pVehicle->GetIntelligence()->GetActiveTask();
		if(!pCarTask || (pCarTask->IsMissionAchieved() && !pCarTask->IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved)) )
		{
			SetState(State_Exit);
			return FSM_Continue;
		}
	}

	// If a new desired subtask has been added, change our subtask
	if( m_bNewDesiredSubtask && m_pDesiredSubTask )
	{
		SetNewTask( m_pDesiredSubTask->Copy() );
		m_bNewDesiredSubtask = false;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskControlVehicle::Drive_OnUpdateClone(CPed* pPed)
{
	CTaskFSMClone *pCloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_VEHICLE_GUN);
	if( pCloneSubTask )
	{
		SetNewTask( pCloneSubTask );
	}
	else
	{
		pCloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON);
		if( pCloneSubTask )
		{
			SetNewTask( pCloneSubTask );
		}
	}
	return FSM_Continue;
}

void CTaskControlVehicle::SetDesiredSubtask(aiTask* pTask)
{
	if( m_pDesiredSubTask )
		delete m_pDesiredSubTask;

	m_pDesiredSubTask = pTask;

	// Default to the basic drive task if the desired subtask isn't specified
	if( m_pDesiredSubTask == NULL )
		m_pDesiredSubTask = rage_new CTaskInVehicleBasic(m_pVehicle);

	m_bNewDesiredSubtask = true;
}

#if !__FINAL
const char * CTaskControlVehicle::GetStaticStateName( s32 iState  )
{
	taskAssertf(iState >= State_Start && iState <= State_Exit,
		"iState fell outside expected valid range. iState value was %d", 
		iState);
	if (iState == State_Invalid )
	{
		return "State_Invalid";
	}
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_EnterVehicle",
		"State_StartVehicle",
		"State_JoinWithRoadSystem",
		"State_Drive",
		"State_Exit"
	};
	return aStateNames[iState];
}
#endif //!__FINAL

CClonedControlVehicleInfo::fnTaskDataLogger CClonedControlVehicleInfo::m_taskDataLogFunction = 0;

CClonedControlVehicleInfo::CClonedControlVehicleInfo()
: m_targetVehicle(NULL)
, m_hasVehicleTask(false)
, m_vehicleTaskType(0)
, m_vehicleTaskDataSize(0)
, m_network(true)
{
}

CClonedControlVehicleInfo::CClonedControlVehicleInfo(u32 state, CVehicle* pVehicle, CTaskVehicleMissionBase* pVehicleTask)
: m_targetVehicle(pVehicle)
, m_hasVehicleTask(false)
, m_vehicleTaskType(0)
, m_vehicleTaskDataSize(0)
, m_network(true)
{
	SetStatusFromMainTaskState(state);

	if (pVehicleTask)
	{
		m_hasVehicleTask = true;

		m_vehicleTaskType = pVehicleTask->GetTaskType();

		if (pVehicleTask->IsIgnoredByNetwork())
		{
			m_network = false;
		}
		else
		{
			// we can only serialise the vehicle task in the network game because it may try and serialise a target position, which uses a word extents callback
			// defined in netSerialiserUtils, and only set up when the network game is initialised
			if (NetworkInterface::IsGameInProgress())
			{
				datBitBuffer messageBuffer;
				messageBuffer.SetReadWriteBits(m_vehicleTaskData, sizeof(m_vehicleTaskData), 0);

				CTaskVehicleSerialiserBase* pTaskSerialiser = pVehicleTask->GetTaskSerialiser();

				if (pTaskSerialiser)
				{
					pTaskSerialiser->WriteTaskData(pVehicleTask, messageBuffer);
					m_vehicleTaskDataSize = messageBuffer.GetCursorPos();
					delete pTaskSerialiser;
				}
			}
		}
	}
}

CClonedControlVehicleInfo::~CClonedControlVehicleInfo() 
{ 
}

int CClonedControlVehicleInfo::GetScriptedTaskType() const
{
    int type = SCRIPT_TASK_INVALID;

    if(m_hasVehicleTask)
    {
        switch(m_vehicleTaskType)
        {
		case CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW:
        case CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER:
        case CTaskTypes::TASK_VEHICLE_GOTO_PLANE:
        case CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE:
            type = SCRIPT_TASK_VEHICLE_DRIVE_TO_COORD;
			break;
        case CTaskTypes::TASK_VEHICLE_ATTACK:
        case CTaskTypes::TASK_VEHICLE_BLOCK:
        case CTaskTypes::TASK_VEHICLE_CIRCLE:
        case CTaskTypes::TASK_VEHICLE_CRASH:
        case CTaskTypes::TASK_VEHICLE_CRUISE_NEW:
        case CTaskTypes::TASK_VEHICLE_ESCORT:
		case CTaskTypes::TASK_VEHICLE_FLEE:
		case CTaskTypes::TASK_VEHICLE_FLEE_AIRBORNE:
        case CTaskTypes::TASK_VEHICLE_FOLLOW:
		case CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING:
		case CTaskTypes::TASK_VEHICLE_HELI_PROTECT:
        case CTaskTypes::TASK_VEHICLE_LAND:
        case CTaskTypes::TASK_VEHICLE_PARK_NEW:
        case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR:
        case CTaskTypes::TASK_VEHICLE_RAM:
        case CTaskTypes::TASK_VEHICLE_STOP:
        case CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN:
            type = SCRIPT_TASK_VEHICLE_MISSION;
			break;
		default:
			Assertf(0, "CClonedControlVehicleInfo::GetScriptedTaskType() - vehicle task %s must be added to the switch statement", TASKCLASSINFOMGR.GetTaskName(m_vehicleTaskType));
            break;
        }
    }

    return type;
}

CTaskFSMClone *CClonedControlVehicleInfo::CreateCloneFSMTask()
{
	CTaskVehicleMissionBase* pVehicleTask = NULL;

	if (m_hasVehicleTask)
	{
		pVehicleTask = SafeCast(CTaskVehicleMissionBase, CVehicleIntelligence::CreateVehicleTaskForNetwork(m_vehicleTaskType));
		
		if (pVehicleTask)
		{
			if (m_vehicleTaskDataSize > 0)
			{
				CTaskVehicleSerialiserBase* pTaskSerialiser = pVehicleTask->GetTaskSerialiser();

				if (aiVerifyf(pTaskSerialiser, "Vehicle task %s does not have a network serialiser", pVehicleTask->GetName().c_str()))
				{
					u32 byteSizeOfTaskData = (m_vehicleTaskDataSize+7)>>3;
					datBitBuffer messageBuffer;
					messageBuffer.SetReadOnlyBits(m_vehicleTaskData, byteSizeOfTaskData<<3, 0);

					pTaskSerialiser->ReadTaskData(pVehicleTask, messageBuffer);
					delete pTaskSerialiser;
				}
			}
		}
	}

	CTaskFSMClone *pCloneTask = rage_new CTaskControlVehicle(m_targetVehicle.GetVehicle(), pVehicleTask);

	return pCloneTask;
}

CTask *CClonedControlVehicleInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    return CreateCloneFSMTask();
}

void CClonedControlVehicleInfo::ApplyDataToVehicleTask(CTaskVehicleMissionBase& vehicleTask)
{
	CTaskVehicleSerialiserBase* pTaskSerialiser = vehicleTask.GetTaskSerialiser();

	if (aiVerifyf(pTaskSerialiser, "Vehicle task %s does not have a network serialiser", vehicleTask.GetName().c_str()))
	{
		u32 byteSizeOfTaskData = (m_vehicleTaskDataSize+7)>>3;
		datBitBuffer messageBuffer;
		messageBuffer.SetReadOnlyBits(m_vehicleTaskData, byteSizeOfTaskData<<3, 0);

		pTaskSerialiser->ReadTaskData(&vehicleTask, messageBuffer);

		delete pTaskSerialiser;
	}
}

