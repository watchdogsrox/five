#include "TaskVehiclePlayer.h"

// Framework headers
#include "fwmaths/vector.h"
#include "fwmaths/angle.h"
#include "math/vecMath.h"


// Game headers
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "frontend/PauseMenu.h"
#include "frontend/MultiplayerChat.h"
#include "system/ControlMgr.h"
#include "network/NetworkInterface.h"
#include "network/Events/NetworkEventTypes.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/Ped.h"	
#include "Peds/PlayerInfo.h"
#include "peds/PlayerSpecialAbility.h"
#include "audio/planeaudioentity.h"
#include "control/record.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Heli.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/VehicleFactory.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "scene/world/GameWorld.h"
#include "Physics/PhysicsHelpers.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "debug/DebugScene.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Game/ModelIndices.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

BANK_ONLY( PARAM( steerToCamera, "steertocamera" ) );

#if RSG_ORBIS
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_STOPPED = 12.0f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_ATSPEED = 6.0f;
#else
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_STOPPED = 10.0f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_ATSPEED = 5.0f;
#endif

bank_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_STOPPED_SPECIAL = 20.0f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_STOPPED_QUAD = 5.0f;


bank_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_SPECIAL = 15.0f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_QUAD = 4.0f;

dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_MAXSPEED = 30.0f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEERING_CURVE_POW = 1.5f;
dev_float CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_STATIONARY_AUTOCENTRE_MAX = (DtoR * 10.0f);

dev_float CTaskVehiclePlayerDriveBike::ms_fBIKE_STEER_SMOOTH_RATE_MAXSPEED = 25.0f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBIKE_STD_STEER_SMOOTH_RATE = 7.0f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBIKE_STOPPED_STEER_SMOOTH_RATE = 5.5f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBIKE_STD_LEAN_SMOOTH_RATE = 15.0f;

dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_STEER_SMOOTH_RATE_MAXSPEED = 15.0f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_STD_STEER_SMOOTH_RATE = 4.5f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_STOPPED_STEER_SMOOTH_RATE = 6.0f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_STD_LEAN_SMOOTH_RATE = 12.0f;

dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_MIN_FORWARDNESS_SLOW = 0.35f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_MAX_FORWARDNESS_SLOW = 0.40f;

dev_float CTaskVehiclePlayerDriveBike::ms_fMOTION_STEER_SMOOTH_RATE_MULT = 3.0f;
dev_float CTaskVehiclePlayerDriveBike::ms_fMOTION_STEER_DEAD_ZONE = 0.06f;
dev_float CTaskVehiclePlayerDriveBike::ms_fPAD_STEER_REUCTION_PAD_END = 0.9f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBICYCLE_STEER_REDUCTION_UNDER_LIMIT_MULT = 0.9f;
dev_float CTaskVehiclePlayerDriveBike::ms_fBIKE_STEER_REDUCTION_UNDER_LIMIT_MULT = 0.6f;

dev_float CTaskVehiclePlayerDriveBoat::ms_fBOAT_STEER_SMOOTH_RATE = 8.0f;
dev_float CTaskVehiclePlayerDriveBoat::ms_fBOAT_PITCH_FWD_SMOOTH_RATE = 10.0f;

dev_float CTaskVehiclePlayerDriveRotaryWingAircraft::ms_fHELI_THROTTLE_CONTROL_DAMPING = 0.1f;
dev_float CTaskVehiclePlayerDriveRotaryWingAircraft::ms_fHELI_AUTO_THROTTLE_FALLOFF = 0.2f;
dev_float CTaskVehiclePlayerDriveRotaryWingAircraft::ms_fHELI_STEERING_CURVE_POW_MOUSE = 1.5f;

dev_float CTaskVehiclePlayerDrivePlane::ms_fPLANE_STEERING_CURVE_POW_MOUSE = 2.0f;

float CTaskVehiclePlayerDriveAutomobile::ms_fAUTOMOBILE_MAXSPEEDNOW =	4.0f;
float CTaskVehiclePlayerDriveAutomobile::ms_fAUTOMOBILE_MAXSPEEDCHANGE =	12.0f;
float CTaskVehiclePlayerDrivePlane::ms_fPLANE_MAXSPEEDNOW =	14.0f;
float CTaskVehiclePlayerDriveBike::ms_fBIKE_MAXSPEEDNOW =	4.0f;
float CTaskVehiclePlayerDriveBike::ms_fBIKE_MAXSPEEDCHANGE =	12.0f;

const u32 g_HeliHoverCameraHash = ATSTRINGHASH("FOLLOW_HELI_HOVER_CAMERA", 0xD7020D1F);

bank_float CTaskVehiclePlayerDrive::sfSpecialAbilitySteeringTimeStepMult = 1.0f;

#if RSG_PC
const float c_fMouseAdjustmentScale = 4.0f;	// undo scaling done in mapper.cpp, can be increased if too slow.
dev_float c_fMouseTollerance = 0.1f;
#endif

////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDrive::CTaskVehiclePlayerDrive() :
CTaskVehicleMissionBase(),
m_fTimeSincePlayingRecording(999.0f),
m_bExitButtonUp(false),
m_bDoorControlButtonUp(false),
m_uTimeHydraulicsControlPressed(0)
#if RSG_PC
, m_fLowriderUpDownInput(0.0f)
#endif
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE);
}


////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDrive::~CTaskVehiclePlayerDrive()
{

}


/////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehiclePlayerDrive::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		//
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pVehicle);
			FSM_OnExit
				Drive_OnExit(pVehicle);
			FSM_OnUpdate
				return Drive_OnUpdate(pVehicle);
		//
		FSM_State(State_NoDriver)
			FSM_OnEnter
				NoDriver_OnEnter(pVehicle);
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////
//State_Drive
/////////////////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDrive::Drive_OnEnter(CVehicle* pVehicle)
{
    // Setup a convertible roof control task if we have an animation clip dictionary    
    if( pVehicle->DoesVehicleHaveAConvertibleRoofAnimation() )
    {
		aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetActiveTask();

		if(!pTask || pTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_CONVERTIBLE_ROOF)
		{
			CTaskVehicleConvertibleRoof *convertibleRoof= rage_new CTaskVehicleConvertibleRoof(false,pVehicle->m_nVehicleFlags.bRoofLowered);
			pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(convertibleRoof, VEHICLE_TASK_SECONDARY_ANIM);
		}
    }
	if( pVehicle->DoesVehicleHaveATransformAnimation() )
	{
		aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree( VEHICLE_TASK_TREE_SECONDARY )->GetActiveTask();
		if( !pTask || pTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE )
		{
			CTaskVehicleTransformToSubmarine *transformToSubmarine= rage_new CTaskVehicleTransformToSubmarine();
			pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask( transformToSubmarine, VEHICLE_TASK_SECONDARY_ANIM );
		}
	}

	if( pVehicle->GetClearWaitingOnCollisionOnPlayerEnter() )
	{
		pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision = false;
	}

	ProcessDriverInputsForPlayerOnEnter(pVehicle);
}

/////////////////////////////////////////////////////////////////////////////////
//State_Drive
/////////////////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDrive::Drive_OnExit(CVehicle* pVehicle)
{
	ProcessDriverInputsForPlayerOnExit(pVehicle);

	// if the vehicle has KERS make sure we disable it when the player exits
	if( pVehicle->pHandling->hFlags & HF_HAS_KERS )
	{
		pVehicle->SetKERS( false );
		pVehicle->m_Transmission.DisableKERSEffect();
	}
}

/////////////////////////////////////////////////////////////////////////////////
//State_Drive
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return	CTaskVehiclePlayerDrive::Drive_OnUpdate	(CVehicle* pVehicle)
{
	const bool bHasDriver = pVehicle->GetDriver() != NULL;
	const bool bDriverInjured = pVehicle->GetDriver() && pVehicle->GetDriver()->IsInjured();
	const bool bDriverArrested = pVehicle->GetDriver() && pVehicle->GetDriver()->GetIsArrested();
	const bool bDriverJumpingOut = false;//pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE_EXIT_SEAT) == CTaskExitVehicleExitSeat::State_JumpOutOfSeat ? true : false;

	m_fTimeSincePlayingRecording = rage::Min(999.0f, m_fTimeSincePlayingRecording + fwTimer::GetTimeStep());
	if( pVehicle && pVehicle->IsRunningCarRecording() )
	{
		m_fTimeSincePlayingRecording = 0.0f;
	}
	
	if(bHasDriver && !bDriverArrested && !bDriverInjured && !bDriverJumpingOut && taskVerifyf(pVehicle->GetDriver()->GetPlayerInfo(), "Invalid player info!"))
	{
		Assert( pVehicle->GetDriver() );
		Assert(pVehicle->GetDriver()->IsPlayer());
		CPed* pPlayerPed = pVehicle->GetDriver();

		CControl *pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl==NULL)
			return FSM_Continue;

		ProcessDriverInputsForPlayerOnUpdate(pPlayerPed, pControl, pVehicle);
		
		// Dont override the velocity unless the player hasn't been running a recording for at least a second
		if(IsThePlayerControlInactive(pControl) && m_fTimeSincePlayingRecording > 0.1f)
		{
			ProcessDriverInputsForPlayerInactiveOnUpdate(pControl, pVehicle);
		}

		if( CGameWorld::GetMainPlayerInfo()->AreControlsDisabled() || CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_EXIT) WIN32PC_ONLY( || SMultiplayerChat::GetInstance().ShouldDisableInputSinceChatIsTyping()))
		{
			m_bExitButtonUp = false;
		}
		else if( !m_bExitButtonUp && !pControl->GetVehicleExit().IsDown())
		{
			m_bExitButtonUp = true;
		}

		// Check if we need to re-add convertible roof animation task (it might get blatted by, e.g. the bring vehicle to halt task).
		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetActiveTask();
			if(!pTask)
			{
				CTaskVehicleConvertibleRoof *convertibleRoof= rage_new CTaskVehicleConvertibleRoof(false,pVehicle->m_nVehicleFlags.bRoofLowered);
				pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(convertibleRoof, VEHICLE_TASK_SECONDARY_ANIM);
			}
		}
		if( pVehicle->DoesVehicleHaveATransformAnimation() )
		{
			aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree( VEHICLE_TASK_TREE_SECONDARY )->GetActiveTask();
			if( !pTask )
			{
				CTaskVehicleTransformToSubmarine *transformToSubmarine= rage_new CTaskVehicleTransformToSubmarine();
				pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask( transformToSubmarine, VEHICLE_TASK_SECONDARY_ANIM );
			}
		}
	}

	if(bHasDriver == false)//if theres no driver goto the no driver state
		SetState(State_NoDriver);

	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////

bool CTaskVehiclePlayerDrive::IsThePlayerControlInactive(CControl *pControl)
{
	TUNE_GROUP_BOOL( VEHICLE_PLAYER_DRIVE, playerDrive_forceDisabledControl, false);//used for testing disabled controls

	// If the player has no control (cut scene for instance) we will hit the brakes.
	if ((!CPauseMenu::IsActive()&&!CLiveManager::IsSystemUiShowing()) || !NetworkInterface::IsGameInProgress())
	{
		if (CControlMgr::IsDisabledControl(pControl) || playerDrive_forceDisabledControl)
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePlayerDrive::NoDriver_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleNoDriver());
}


/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePlayerDrive::NoDriver_OnUpdate				(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_NO_DRIVER))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////
// shared for cars and motorbikes
////////////////////////////////////////////////////////////////////////
void CTaskVehiclePlayerDrive::ProcessPlayerControlInputsForBrakeAndGasPedal(CControl* pControl, bool bForceBrake, CVehicle *pVehicle)
{
	//get the vehicles reference frame velocity, so if they are driving on a plane we car work out the relative velocity rather then just using the absolute

	// Forwardness between -1 and 1
	float fSpeed = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	if(CPauseMenu::sm_uLastResumeTime + CPauseMenu::sm_uDisableInputDuration >= fwTimer::GetTimeInMilliseconds())
	{
		options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);
	}
	float fAccelButton = pControl->GetVehicleAccelerate().GetNorm01(options);
	float fBrakeButton = pControl->GetVehicleBrake().GetNorm01(options);
	
	if( pVehicle->m_bInvertControls )
	{
		float temp = fAccelButton;
		fAccelButton = fBrakeButton;
		fBrakeButton = temp;
	}

	float fForwardness = fAccelButton - fBrakeButton;

	if( pVehicle->pHandling->hFlags & HF_HAS_KERS )
	{
		bool bKERS = pControl->GetVehicleKERS().IsPressed();
		if( bKERS )
		{
            static u32 sMinTimeToToggleKERS = 500;
            if( !pVehicle->pHandling->GetCarHandlingData() ||
                !( pVehicle->pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS ) ||
                pVehicle->GetKERSActive() ||
                fwTimer::GetTimeInMilliseconds() > ( pVehicle->m_kersToggledTime + sMinTimeToToggleKERS )  )
            {
			    pVehicle->ToggleKERS();
                pVehicle->m_kersToggledTime = fwTimer::GetTimeInMilliseconds();
            }
		}
	}

	bool bReverseBrakeAndThrottleForSecondEngine = false;

	if( pVehicle->InheritsFromAmphibiousQuadBike() &&
		CPhysics::ms_bInStuntMode && 
		pVehicle->IsInAir() )
	{
		if( fForwardness > 0.0f &&
			pVehicle->GetThrottle() > 0.0f )
		{
			fSpeed = Abs( fSpeed );
		}
	}

	if( pVehicle->GetAreOutriggersBeingDeployed() )
	{
		bForceBrake = true;
	}

	if(bForceBrake)
	{
		pVehicle->SetBrake(1.0f);
		pVehicle->SetThrottle(0.0f);
	}
	else if(!pVehicle->m_nVehicleFlags.bEngineOn && pVehicle->m_nVehicleFlags.bIsDrowning)
	{
		pVehicle->SetBrake(0.0f);
		pVehicle->SetThrottle(0.0f);
	}
	else if(pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet)
		/*pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PUT_ON_HELMET)*/)
	{
		pVehicle->SetBrake(0.0f);
		pVehicle->SetThrottle(0.0f);
	}
	else if( pVehicle->GetSpecialFlightModeRatio() == 1.0f && 
			 !pVehicle->HasGlider() )
	{
		pVehicle->SetThrottle( fForwardness );
		pVehicle->SetBrake( 0.0f );
	}
	// We've stopped. Go where we want to go.
	else if (rage::Abs(fSpeed) < 0.5f)
	{
		// if both brake and throttle are held down, do a burn out
		if(fAccelButton > 0.6f && fBrakeButton > 0.6f)
		{
			pVehicle->SetThrottle(fAccelButton);
			pVehicle->SetBrake(fBrakeButton);
		}
		else
		{
			pVehicle->SetThrottle(fForwardness);
			pVehicle->SetBrake(0.0f);
		}
	}
	else
	{	
		if( pVehicle->GetSpeedUpBoostDuration() > 0.0f &&
			( !pVehicle->InheritsFromAmphibiousQuadBike() ||
			  static_cast< CAmphibiousQuadBike* >( pVehicle )->IsWheelsFullyOut() ||
			  static_cast< CAmphibiousQuadBike* >( pVehicle )->IsPropellerSubmerged() ) )
		{
			fForwardness = 1.0f;
		}

		// if both brake and throttle are held down, do a burn out
		if(fAccelButton > 0.6f && fBrakeButton > 0.6f)
		{
			pVehicle->SetThrottle(fAccelButton);
			pVehicle->SetBrake(fBrakeButton);
		}
		// going forwards
		else if (fSpeed >= 0.0f)
		{	
			// we want to go forwards so go faster
			if (fForwardness >= 0.0f)
			{
				if( pVehicle->GetSlowDownBoostDuration() <= 0.0f )
				{
					pVehicle->SetThrottle(fForwardness);
				}
				else
				{
					pVehicle->SetThrottle(0.0f);
				}

				pVehicle->SetBrake(0.0f);
			}
			// we don't want to go forwards - so brake
			else
			{
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(-fForwardness);
			}
		}
		// going backwards
		else
		{
			// want to go forwards
			if (fForwardness >= 0.0f)
			{
				// let us sit on the gas pedal if we're trying to get up a slope but sliding back
				if(pVehicle->GetThrottle() > 0.5f && fSpeed > -10.0f)
				{
					pVehicle->SetThrottle(fForwardness);
					pVehicle->SetBrake(0.0f);
				}
				// oh dear we're sliding back too fast, go for the brakes
				else
				{
					bReverseBrakeAndThrottleForSecondEngine = true;

					pVehicle->SetThrottle(0.0f);
					pVehicle->SetBrake(fForwardness);
				}
			}
			// we want to go backwards, so apply the gas
			else
			{
				pVehicle->SetThrottle(fForwardness);
				pVehicle->SetBrake(0.0f);
			}
		}
	}

	if( pVehicle->InheritsFromAmphibiousAutomobile() )
	{
		static_cast<CAmphibiousAutomobile*>(pVehicle)->SetReverseBrakeAndThrottle(bReverseBrakeAndThrottleForSecondEngine);
	}
}


////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePlayerDrive::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Drive&&iState<=State_NoDriver);
	static const char* aStateNames[] = 
	{
		"State_Drive",
		"State_NoDriver"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveAutomobile
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveAutomobile::CTaskVehiclePlayerDriveAutomobile() :
CTaskVehiclePlayerDrive(),
m_JustEnteredDiggerMode(false),
m_JustLeftDiggerMode(false)
#if RSG_PC
,m_MouseSteeringInput(false)
#endif
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOMOBILE);
}


//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveAutomobile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		//
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pVehicle);
			FSM_OnUpdate
				return Drive_OnUpdate(pVehicle);
		//
		FSM_State(State_NoDriver)
			FSM_OnEnter
				NoDriver_OnEnter(pVehicle);
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
		//
		FSM_State(State_DiggerMode)
			FSM_OnEnter
				DiggerMode_OnEnter(pVehicle);
			FSM_OnUpdate
				return DiggerMode_OnUpdate(pVehicle);
	FSM_End
}

u32 ms_uTimeToHoldTruckDetachButtonDown = 250;

//////////////////////////////////////////////////////////////////////
void CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	Assert(pPlayerPed && (pPlayerPed->IsControlledByLocalPlayer()));

	if(pControl==NULL)
		return;

    // Process whether we should enable Franklin's special ability.
	CPlayerSpecialAbilityManager::ProcessControl(pPlayerPed);

	bool bForceBrake = false;
	if( pControl->GetVehicleExit().IsDown() && m_bExitButtonUp )
	{
		if( !pVehicle->CanPedJumpOutCar(pPlayerPed) )
		{
			pVehicle->SetHandBrake(true);
			bForceBrake = true;
			if (pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < (0.2f*0.2f))		// If we're already static we don't want the brake light to light up. (causes ugly glitch)
			{
				pVehicle->m_nVehicleFlags.bSuppressBrakeLight = true;
			}
		}
	}
	else
	{
		pVehicle->SetHandBrake(pVehicle->GetIsHandBrakePressed(pControl));
	}

	if (!pVehicle->m_nVehicleFlags.bEngineOn && !pVehicle->m_nVehicleFlags.bEngineStarting && pVehicle->m_Transmission.GetEngineHealth() > 0.0f
		&& pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < 3.0f*3.0f)
	{
		bForceBrake = true;
	}

	float fFwdSpeedFrac = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()), pVehicle->GetVelocityIncludingReferenceFrame()) / ms_fCAR_STEER_SMOOTH_RATE_MAXSPEED;
	float fStoppedSteerSmoothRate = pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ? ms_fCAR_STEER_SMOOTH_RATE_STOPPED_QUAD : ms_fCAR_STEER_SMOOTH_RATE_STOPPED;
	float fMovingSteerSmoothRate = pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ? ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_QUAD : ms_fCAR_STEER_SMOOTH_RATE_ATSPEED;

	float timeStep = fwTimer::GetTimeStep();
	// If we are using Franklin's special ability we shouldn't slow down the steering.
	if( pPlayerPed->GetSpecialAbility() && pPlayerPed->GetSpecialAbility()->GetType() == SAT_CAR_SLOWDOWN )
	{
		if(pPlayerPed->GetSpecialAbility()->ShouldApplyFx())
		{
			float fFxStrength = pPlayerPed->GetSpecialAbility()->GetFxStrength();
			timeStep = Lerp(fFxStrength, timeStep, fwTimer::GetNonPausableCamTimeStep() * sfSpecialAbilitySteeringTimeStepMult);// We want to use a time step for the steering which can't be modified by scaling time
			
			fMovingSteerSmoothRate = Lerp(fFxStrength, fMovingSteerSmoothRate, ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_SPECIAL);
			fStoppedSteerSmoothRate = Lerp(fFxStrength, fStoppedSteerSmoothRate, ms_fCAR_STEER_SMOOTH_RATE_STOPPED_SPECIAL);
		}
	}

	float fSmoothFrac = fStoppedSteerSmoothRate;
	if(fFwdSpeedFrac > 1.0f)
		fSmoothFrac = fMovingSteerSmoothRate;
	else if(fFwdSpeedFrac > 0.0f)
		fSmoothFrac = fFwdSpeedFrac*fMovingSteerSmoothRate + (1.0f - fFwdSpeedFrac)*fStoppedSteerSmoothRate;
	
	//Slow down steering on rear wheel steer vehicles.
	if(pVehicle->pHandling->hFlags &HF_STEER_REARWHEELS)
	{
		static dev_float sfRearWheelSteerMult = 0.2f;
		fSmoothFrac *= sfRearWheelSteerMult; 
	}

	if (NetworkInterface::IsInCopsAndCrooks())
	{
		// for C&C, the Nitro special ability affects steering while active
		if (pPlayerPed->GetSpecialAbilityType(PSAS_SECONDARY) == SAT_NITRO_COP ||
			pPlayerPed->GetSpecialAbilityType(PSAS_SECONDARY) == SAT_NITRO_CROOK)
		{
			const CPlayerSpecialAbility* pPlayerSpecialAbility = pPlayerPed->GetSpecialAbility(PSAS_SECONDARY);
			if (pPlayerSpecialAbility && pPlayerSpecialAbility->IsActive())
			{
				// get the modifier from the abilities and apply it here
				fSmoothFrac *= pPlayerSpecialAbility->GetSteeringMultiplier();
			}
		}
	}

	bool bCheckSteeringExclusive = true;
#if KEYBOARD_MOUSE_SUPPORT
	if(pControl->WasKeyboardMouseLastKnownSource() && (pVehicle->GetModelIndex() == MI_TANK_RHINO || (MI_TANK_KHANJALI.IsValid() && pVehicle->GetModelIndex() == MI_TANK_KHANJALI)))
	{
		bCheckSteeringExclusive = false;
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if (bCheckSteeringExclusive)
	{
		pControl->SetVehicleSteeringExclusive();
	}
	float fDesiredSteerInput = -pControl->GetVehicleSteeringLeftRight().GetNorm();

	if( pVehicle->m_bInvertControls )
	{
		fDesiredSteerInput = -fDesiredSteerInput;
	}

#if RSG_PC
	float fLowriderDesiredUpDownInput = pControl->GetVehicleSteeringUpDown().GetNorm();	// For lowrider up/down bounce input.

	if(pControl->WasKeyboardMouseLastKnownSource())
	{
		if(pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
		{
			// Undo fixup applied for scaled axis, without affecting scripted minigame inputs.
			fDesiredSteerInput *= c_fMouseAdjustmentScale;
			fLowriderDesiredUpDownInput *= c_fMouseAdjustmentScale;
			m_MouseSteeringInput = true;
		}
		else if(pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MKB_AXIS)// Keyboard steering input
		{
			m_MouseSteeringInput = false;
		}
	}
	else
	{
		m_MouseSteeringInput = false;
	}

	bool bMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Vehicle;
	bool bCameraMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Camera;

	if(m_MouseSteeringInput && ((bMouseSteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseSteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())))
	{
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, CAR_STEERING_DEADZONE_CENTERING_SPEED, 2.0f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, CAR_STEERING_MULTIPLIER, 3.0f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, CAR_STEERING_CENTER_ALLOWANCE, 0.001f, 0.0f, 1.0f, 0.0001f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, CAR_STEERING_INPUT_DEADZONE, FLT_EPSILON, 0.0f, 1.0f, 0.0001f);

		float fAutoCenterMult = CPauseMenu::GetMenuPreference(PREF_MOUSE_AUTOCENTER_CAR)/10.0f;

		if (fabs(fDesiredSteerInput) <= CAR_STEERING_INPUT_DEADZONE)
		{
			if(pVehicle->m_fSteerInput > 0.0f)
			{
				fDesiredSteerInput = Clamp(pVehicle->m_fSteerInput - (CAR_STEERING_DEADZONE_CENTERING_SPEED * fAutoCenterMult) * timeStep, 0.0f, 1.0f);
			}
			else
			{
				fDesiredSteerInput = Clamp(pVehicle->m_fSteerInput + (CAR_STEERING_DEADZONE_CENTERING_SPEED * fAutoCenterMult) * timeStep,-1.0f, 0.0f);
			}
		}
		else if( Sign(fDesiredSteerInput) == Sign(pVehicle->m_fSteerInput) || rage::Abs(pVehicle->m_fSteerInput) <= CAR_STEERING_CENTER_ALLOWANCE)
		{
			fDesiredSteerInput = pVehicle->m_fSteerInput + (fDesiredSteerInput * CAR_STEERING_MULTIPLIER);
		}

		if( Sign(fLowriderDesiredUpDownInput) == Sign(m_fLowriderUpDownInput))
		{
			fLowriderDesiredUpDownInput = m_fLowriderUpDownInput + (fLowriderDesiredUpDownInput * CAR_STEERING_MULTIPLIER);
		}
	}

#else
	float fCarSteeringCurvePow = ms_fCAR_STEERING_CURVE_POW;
	fDesiredSteerInput = Sign(fDesiredSteerInput) * Powf(Abs(fDesiredSteerInput),fCarSteeringCurvePow);
#endif



// ORBIS has more fidelity in stick input now, so adjust the inputs to be more aggressive.
#if RSG_ORBIS
	TUNE_FLOAT(STEER_ORBIS_MULT, 1.0f, 0.0f, 2.0f, 0.01f);

	fDesiredSteerInput *= STEER_ORBIS_MULT;

	fDesiredSteerInput = Clamp( fDesiredSteerInput, -1.0f, 1.0f);
#endif

#if RSG_PC
	bool isWheelDevice = pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_JOYSTICK_AXIS &&
		ioJoystick::GetStick(pControl->GetVehicleSteeringLeftRight().GetSource().m_DeviceIndex).IsWheel();

	// If its a steering wheel the use the raw value as this gives us the angle of the steering wheel.
	if(isWheelDevice)
	{
		fDesiredSteerInput = -pControl->GetVehicleSteeringLeftRight().GetNorm(ioValue::NO_DEAD_ZONE);
	}
#endif // RSG_PC

	// The player can't steer whilst hotwiring the car
	CTaskMotionInAutomobile* pTask = static_cast<CTaskMotionInAutomobile*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE) );
	if(((pTask && pTask->IsHotwiringVehicle()) || pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired) && !pVehicle->m_nVehicleFlags.bEngineOn )
	{
		bForceBrake = true;
		fDesiredSteerInput = 0.0f;
#if RSG_PC
		fLowriderDesiredUpDownInput = 0.0f;
#endif
	}

	// Or when putting on helmet
	if(pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
	{
		fDesiredSteerInput = 0.0f;
#if RSG_PC
		fLowriderDesiredUpDownInput = 0.0f;
#endif
	}

	// Or while being affected by an EMP
	if (pVehicle->GetExplosionEffectEMP())
	{
		TUNE_GROUP_BOOL(VEHICLE_EXPLOSION_EFFECTS, EMP_EFFECT_SHOULD_APPLY_BRAKE, false);
		if (EMP_EFFECT_SHOULD_APPLY_BRAKE)
		{
			bForceBrake = true;
		}
		fDesiredSteerInput = 0.0f;
#if RSG_PC
		fLowriderDesiredUpDownInput = 0.0f;
#endif
	}

#if RSG_PC
	if(isWheelDevice)
	{
		// If its a steering wheel device we set the value directly as the device is already dt based (like a mouse) and it reduces steering lag which makes
		// wheel more responsive.
		pVehicle->m_fSteerInput = fDesiredSteerInput;
	}
	else
#endif // RSG_PC
	{
		pVehicle->m_fSteerInput = pVehicle->m_fSteerInput + (fDesiredSteerInput - pVehicle->m_fSteerInput) * fSmoothFrac * timeStep;
	}

	pVehicle->m_fSteerInput += pVehicle->m_fSteerInputBias;
	pVehicle->m_fSteerInput = Clamp(pVehicle->m_fSteerInput, -1.0f, 1.0f);
	float fSteerAngle = pVehicle->m_fSteerInput * pVehicle->pHandling->m_fSteeringLock;

#if __BANK
    TUNE_GROUP_BOOL( VEHICLE_DEBUG_STEER, ENABLE_STEER_TO_CAMERA, PARAM_steerToCamera.Get() );
    TUNE_GROUP_FLOAT( VEHICLE_DEBUG_STEER, STEER_RATE_SCALE, 2.0f, 0.1f, 100.0f, 0.1f );

    if( ENABLE_STEER_TO_CAMERA )
    {
        float angleToCamForward = -camInterface::GetFront().Dot( VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetRight() ) );

        if( pVehicle->GetVelocity().Dot( VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetForward() ) ) < 0.0f )
        {
            angleToCamForward *= -1.0f;
        }

        fSteerAngle = angleToCamForward * STEER_RATE_SCALE;
        fSteerAngle = Clamp( fSteerAngle, -pVehicle->pHandling->m_fSteeringLock, pVehicle->pHandling->m_fSteeringLock );

        camFollowVehicleCamera::ms_ShouldDisablePullAround = true;
    }
#endif // #if __BANK

	// moved this code down to vehicle because it's shared with motorbikes
	ProcessPlayerControlInputsForBrakeAndGasPedal(pControl, bForceBrake, pVehicle);

#if __DEV
	static dev_bool USE_THROTTLE_LIMITING_FOR_PLAYER = false;
	if( USE_THROTTLE_LIMITING_FOR_PLAYER )
	{
		static dev_float ENGINE_INCREASE = 1.0f;
		pVehicle->SetCheatPowerIncrease(ENGINE_INCREASE);
		float fMaxThrottle = CTaskVehicleGoToPointAutomobile::CalculateMaximumThrottleBasedOnTraction(pVehicle);
		pVehicle->SetThrottle(rage::Min(pVehicle->GetThrottle(), fMaxThrottle));

		// If we're slipping at all, remove any power increase
		if( pVehicle->GetCheatPowerIncrease() > 1.0f )
		{
			if( fMaxThrottle < 1.0f )
			{
				pVehicle->SetCheatPowerIncrease(Lerp(fMaxThrottle, 1.0f, pVehicle->GetCheatPowerIncrease()));
			}
		}

	}
#endif

	//Stationary auto-centre damping
	if(IsClose(fDesiredSteerInput, 0.0f, FLT_EPSILON) && rage::Abs(fFwdSpeedFrac) < 0.01f)
	{
		if(bForceBrake && pVehicle->GetStatus()!=STATUS_WRECKED)
		{
			fSteerAngle = pVehicle->GetSteerAngle();
		}
		float fDesiredChange = rage::Clamp(fSteerAngle - pVehicle->GetSteerAngle(), -ms_fCAR_STEER_STATIONARY_AUTOCENTRE_MAX, ms_fCAR_STEER_STATIONARY_AUTOCENTRE_MAX);
		fSteerAngle = fDesiredChange + pVehicle->GetSteerAngle();
	}

	pVehicle->SetSteerAngle(fSteerAngle);

	if(pVehicle->pHandling->hFlags &HF_HANDBRAKE_REARWHEELSTEER)
	{
		float fDesired2ndSteerAngle = 0.0f;

		if(pVehicle->GetHandBrake())
			fDesired2ndSteerAngle = pVehicle->GetSteerAngle();

		// Progress towards the steer angle
		float fSteerDiff = rage::Clamp(fDesired2ndSteerAngle - pVehicle->GetSecondSteerAngle(),-fSmoothFrac*fwTimer::GetTimeStep(),fSmoothFrac*fwTimer::GetTimeStep());
		pVehicle->SetSecondSteerAngle(pVehicle->GetSecondSteerAngle() + fSteerDiff);
	}

	//Update hydraulic suspension control
	pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics = false;
	if(pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->HasHydraulicSuspension() &&
		pVehicle->m_nVehicleFlags.bEngineOn )
	{
		// Don't allow hydraulic modification if doing a driveby or somebody is entering/exiting/jacking the vehicle.
		if (!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && !pVehicle->m_bBlockDueToEnterExit)
		{
			float fXAxis = ((-pControl->GetVehicleHydraulicsControlLeft().GetNorm()) + pControl->GetVehicleHydraulicsControlRight().GetNorm());
			float fYAxis = ((-pControl->GetVehicleHydraulicsControlUp().GetNorm()) + pControl->GetVehicleHydraulicsControlDown().GetNorm());
			float fStickX = -fXAxis;
			float fStickY = fYAxis;

#if RSG_PC
			m_fLowriderUpDownInput = m_fLowriderUpDownInput + (fLowriderDesiredUpDownInput - m_fLowriderUpDownInput) * fSmoothFrac * timeStep;
			m_fLowriderUpDownInput += pVehicle->m_fSteerInputBias;
			m_fLowriderUpDownInput = Clamp(m_fLowriderUpDownInput, -1.0f, 1.0f);

			// PC mouse/keyboard: Use mouse input to control lowrider if using mouse steering
			if (pControl->WasKeyboardMouseLastKnownSource())
			{
				if (m_MouseSteeringInput && (CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle || CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera))
				{
					// Set stick X/Y based on cached input values. 
					// Using raw mouse input wasn't working as it was returning to 0.0f when not moving it, meaning we couldn't lock the suspension properly.
					fStickX += pVehicle->m_fSteerInput;
					fStickY += m_fLowriderUpDownInput;
				}
			}

			fStickX = Clamp (fStickX, -1.0f, 1.0f);
			fStickY = Clamp (fStickY, -1.0f, 1.0f);
#endif

			ProcessHydraulics( pControl, pVehicle, fStickX, fStickY );
		}
	}

    if( pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) &&
        pVehicle->m_nVehicleFlags.bEngineOn )
    {
        ProcessSuspensionDrop( pVehicle );
    }

	for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

		// Make handBrake2 (A on 360, X on ps3) toggle detaching the trailer
		if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowedToDetachTrailer))
		{
			if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
			{
				CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);

				if(pControl->GetVehicleHeadlight().IsDown() && pControl->GetVehicleHeadlight().HistoryHeldDown(ms_uTimeToHoldTruckDetachButtonDown))
				{
					pTrailerAttachPoint->DetachTrailer(pVehicle);
				}
			}
		}

		float fDesiredSteerInputUpDown = pControl->GetVehicleSteeringUpDown().GetNorm();
	#if RSG_PC
		if(pControl->GetVehicleSteeringUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
		{
			fDesiredSteerInputUpDown *= c_fMouseAdjustmentScale;
		}
	#endif

		if(pVehicleGadget->GetType() == VGT_DIGGER_ARM && pVehicle->m_nVehicleFlags.bEngineOn)
		{
			CVehicleGadgetDiggerArm *pDiggerArm = static_cast<CVehicleGadgetDiggerArm*>(pVehicleGadget);
			pDiggerArm->SetDesiredAcceleration( -fDesiredSteerInputUpDown );
		}

		if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM && pVehicle->m_nVehicleFlags.bEngineOn)
		{
			CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
			pTowArm->SetDesiredAcceleration(pVehicle, fDesiredSteerInputUpDown );

            //check whether the player wants to detach the vehicle
            if(pControl->GetVehicleHeadlight().IsDown() && pControl->GetVehicleHeadlight().HistoryHeldDown(ms_uTimeToHoldTruckDetachButtonDown))
            {
				const CVehicle *pAttachedVehicle = pTowArm->GetAttachedVehicle();

				if(pAttachedVehicle && pAttachedVehicle->IsNetworkClone())
				{
					// send an event to the owner of the attached entity requesting they detach
					CRequestDetachmentEvent::Trigger(*pAttachedVehicle);
				}
				else
				{
					vehicleDisplayf( "[TOWTRUCK ROPE DEBUG] CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate - Detaching entity." );
					pTowArm->DetachVehicle( );
				}
            }
		}

        //do we have a forklift
        if(pVehicleGadget->GetType() == VGT_FORKS && pVehicle->m_nVehicleFlags.bEngineOn)
        {
            CVehicleGadgetForks *pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);

			// Allow the forks to be jammed during processing of the forks gadget.
			if(!pForks->CanMoveForksDown() || pForks->AreForksLocked())
				fDesiredSteerInputUpDown = rage::Min(fDesiredSteerInputUpDown, 0.0f);
			if(!pForks->CanMoveForksUp() || pForks->AreForksLocked())
				fDesiredSteerInputUpDown = rage::Max(fDesiredSteerInputUpDown, 0.0f);

            pForks->SetDesiredAcceleration( -fDesiredSteerInputUpDown );

			// We require script to call the command to lock the forks every frame. Reset the flag here.
			pForks->UnlockForks();
        }

		if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
		{
			CVehicleGadgetHandlerFrame* pFrame= static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);

			BANK_ONLY(pFrame->DebugFrameMotion(pVehicle));

			// Allow the frame to be jammed during processing of the frame gadget.
			if(fDesiredSteerInputUpDown > 0.0f)
				pFrame->SetTryingToMoveFrameDown(true);
			else
				pFrame->SetTryingToMoveFrameDown(false);
			if(!pFrame->CanMoveFrameDown() || pFrame->IsFrameLocked())
				fDesiredSteerInputUpDown = rage::Min(fDesiredSteerInputUpDown, 0.0f);
			if(!pFrame->CanMoveFrameUp() || pFrame->IsFrameLocked())
				fDesiredSteerInputUpDown = rage::Max(fDesiredSteerInputUpDown, 0.0f);

			pFrame->SetDesiredAcceleration( -fDesiredSteerInputUpDown );

			// Check whether the player wants to detach the container (INPUT_CONTEXT is used for attaching/detatching, straingly attatching is done in script).
			if(pControl->GetPedContextAction().IsPressed() && pFrame->IsContainerAttached())
			{
				pFrame->DetachContainerFromFrame();
			}

			// We require script to call the command to lock the frame every frame. Reset the flag here.
			pFrame->UnlockFrame();
		}

		if(pVehicleGadget->GetType() == VGT_ARTICULATED_DIGGER_ARM && pVehicle->m_nVehicleFlags.bEngineOn)
		{
			CVehicleGadgetArticulatedDiggerArm* pDiggerArm = static_cast<CVehicleGadgetArticulatedDiggerArm*>(pVehicleGadget);

			switch(pDiggerArm->GetNumberOfSetupDiggerJoint())
			{
			case 0:
				break;
			case 1:
			case 2:
				{
                    if(pDiggerArm->GetJointSetup(DIGGER_JOINT_STICK))
                    {
					    pDiggerArm->SetDesiredAcceleration(DIGGER_JOINT_STICK, fDesiredSteerInputUpDown );
                    }
                    else if(pDiggerArm->GetJointSetup(DIGGER_JOINT_BOOM))
                    {
                        pDiggerArm->SetDesiredAcceleration(DIGGER_JOINT_BOOM, fDesiredSteerInputUpDown );
                    }
					break;
				}
			case 3:
			case 4:
				{
					const ioValue &ioVal = pControl->GetVehicleHorn();

					// if stick was clicked then enter "digger" mode
					if(ioVal.IsReleased() && !m_JustLeftDiggerMode)
					{
                        m_JustEnteredDiggerMode = true;
						SetState(State_DiggerMode);
					}
                    else
                    {
                        m_JustLeftDiggerMode = false;
                    }

				}
			default:
				break;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *pVehicle)
{
	if (pVehicle->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive )
	{
		if(pVehicle->HasContactWheels() || pVehicle->GetTransform().GetC().GetZf() < 0.0f)
		{
			pVehicle->SetBrake(1.0f);
			pVehicle->SetHandBrake(false);
			pVehicle->SetThrottle(0.0f);
			pVehicle->GetDriver()->GetPlayerInfo()->KeepAreaAroundPlayerClear();
			// Limit the velocity
			float Speed = pVehicle->GetVelocity().Mag();

			if (Speed > ms_fAUTOMOBILE_MAXSPEEDNOW && pVehicle->m_nVehicleFlags.bStopInstantlyWhenPlayerInactive)
			{
				float	NewSpeed = rage::Max(ms_fAUTOMOBILE_MAXSPEEDNOW, Speed - ms_fAUTOMOBILE_MAXSPEEDCHANGE);
				pVehicle->SetVelocity( pVehicle->GetVelocity() * (NewSpeed / Speed) );
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////
//State_DiggerMode
//////////////////////////////////////////////////////////////////////
void CTaskVehiclePlayerDriveAutomobile::DiggerMode_OnEnter	(CVehicle* pVehicle)
{
	for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

		if(pVehicleGadget->GetType() == VGT_ARTICULATED_DIGGER_ARM)
		{
			static s32 DIGGERARM_JOINTS_MINIMUM = 3;
			CVehicleGadgetArticulatedDiggerArm* pDiggerArm = static_cast<CVehicleGadgetArticulatedDiggerArm*>(pVehicleGadget);
			if(pDiggerArm->GetNumberOfSetupDiggerJoint() >= DIGGERARM_JOINTS_MINIMUM)//if it has less then 3 digger joints then it doesn't need to go into digger mode.
			{
				SetNewTask(rage_new CTaskVehiclePlayerDriveDiggerArm( pDiggerArm ) );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveAutomobile::DiggerMode_OnUpdate	(CVehicle* pVehicle)
{
	const bool bHasDriver = pVehicle->GetDriver() != NULL;
	const bool bDriverInjured = pVehicle->GetDriver() && pVehicle->GetDriver()->IsInjured();
	const bool bDriverArrested = pVehicle->GetDriver() && pVehicle->GetDriver()->GetIsArrested();
	const bool bDriverJumpingOut = false; //pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE_EXIT_SEAT) == CTaskExitVehicleExitSeat::State_JumpOutOfSeat ? true : false;
	CControl *pControl = NULL;

	if(bHasDriver && !bDriverArrested && !bDriverInjured && !bDriverJumpingOut)
	{
		Assert( pVehicle->GetDriver() );
		Assert(pVehicle->GetDriver()->IsPlayer());
		CPed* pPlayerPed = pVehicle->GetDriver();

		pControl = pPlayerPed->GetControlFromPlayer();
	}


	if(pControl)//if we have control check whether we should leave "digger" mode
	{
		const ioValue &ioVal = pControl->GetVehicleHorn();
		// if stick was clicked then quit "digger" mode
		if(ioVal.IsReleased() && !m_JustEnteredDiggerMode)
		{
            m_JustLeftDiggerMode = true;
			SetState(State_Drive);
		}
        else
        {
            m_JustEnteredDiggerMode = false;
        }
	}
	else
	{
		SetState(State_Drive);//if anythings failed then just go back to the driving state.
	}

	return FSM_Continue;
}

static bool sb_invertBounceStick = false;

void CTaskVehiclePlayerDriveAutomobile::ProcessHydraulics( CControl* pControl, CVehicle* pVehicle, float fStickX, float fStickY )
{
	int numWheels = pVehicle->GetNumWheels();

	if( !ProcessHydraulicsTilt( pControl, pVehicle, fStickX, fStickY ) )
	{
		ProcessHydraulicsStateChange( pControl, pVehicle );
	}

	CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
	bool suspensionUpdated	= false;
	bool steeringReduced	= false;
	bool donkSuspension		= pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS );

	for(int i = 0; i < numWheels; i++)
	{
		if( CWheel *pWheel = pVehicle->GetWheel( i ) )
		{
			eWheelHydraulicState wheelState			= pWheel->GetSuspensionHydraulicState(); 
			eWheelHydraulicState wheelTargetState	= pWheel->GetSuspensionHydraulicTargetState();

			// Flag if the player has modified the suspension of any of the wheels.
			// Needed for triggering lowrider animations and suspension syncing.
			if (!pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics && (wheelState != WHS_INVALID) && pWheel->GetSuspensionRaiseAmount() != 0.0f)
			{
				pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics = true;
			}

			TUNE_GROUP_FLOAT( HYDRAULICS_TUNE, steeringLockReduction, 0.8f, 0.0f, 1.0f, 0.01f);

			if( !steeringReduced &&
				pWheel->GetConfigFlags().IsFlagSet(WCF_STEER) &&
				pWheel->GetSuspensionRaiseAmount() < 0.0f )
			{
				float currentLoweredAmount = pWheel->GetSuspensionRaiseAmount();
				currentLoweredAmount /= pAutomobile->m_fHydraulicsLowerBound * pWheel->GetSuspensionLength();
				currentLoweredAmount *= steeringLockReduction;

				pVehicle->SetSteerAngle( pVehicle->GetSteerAngle() * steeringLockReduction );

				steeringReduced = true;
			}

			if( wheelState == WHS_FREE ||
				wheelState == WHS_BOUNCE_ACTIVE ||
				wheelState == WHS_BOUNCE_LANDING ||
				wheelState == WHS_BOUNCE_LANDED ||
				( wheelState !=
				  wheelTargetState ) )
			{
				TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, offsetScale, 1.0f, 0.0f, 5.0f, 0.1f );
				static float lockOffsetScale = 0.75f;
				float targetRaiseAmount = 0.0f;
				float raiseRate = 1.0f;

				if( donkSuspension )
				{
					TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, donkSuspensionRate, 0.1f, 0.0f, 1.0f, 0.01f );
					raiseRate = donkSuspensionRate;
				}

				Vector3 vOffset = CWheel::GetWheelOffset(pAutomobile->GetVehicleModelInfo(), pWheel->GetHierarchyId());
				vOffset.z = 0.0f;
				vOffset.NormalizeSafe(vOffset);

				switch( wheelTargetState )
				{
					case WHS_IDLE:
					{
						break;
					}
					case WHS_LOCK:
					{
						targetRaiseAmount = pWheel->GetSuspensionTargetRaiseAmount();
						break;
					}
					case WHS_LOCK_UP:
					{
						// scale this so left/right goes slower than up/down
						float rateScale = 0.75f;

						if( Abs( vOffset.y ) > Abs( vOffset.x ) &&
							Abs( fStickY ) > Abs( fStickX ) )
						{
							rateScale += lockOffsetScale * Abs( vOffset.y );
						}
						else if( Abs( vOffset.x ) > Abs( vOffset.y ) &&
								 Abs( fStickX ) > Abs( fStickY ) )
						{
							rateScale += lockOffsetScale * Abs( vOffset.x );
						}
						raiseRate *= rateScale;

						// just fall through to use the same raise amount as lock up all
					}
					case WHS_LOCK_UP_ALL:
					case WHS_BOUNCE_ALL_ACTIVE:
					case WHS_BOUNCE_ALL_LANDING:
					{
						targetRaiseAmount = pAutomobile->m_fHydraulicsUpperBound;
						break;
					}
					case WHS_LOCK_DOWN_ALL:
					case WHS_LOCK_DOWN:
					{
						targetRaiseAmount = pAutomobile->m_fHydraulicsLowerBound;
						break;
					}
					case WHS_BOUNCE:
					case WHS_BOUNCE_LEFT_RIGHT:
					{
						targetRaiseAmount	= pAutomobile->m_fHydraulicsUpperBound;
						break;
					}
					case WHS_BOUNCE_FRONT_BACK:
					{
						float rateScale = 1.0f;

						// scale this so left/right goes slower than up/down otherwise we get too much bounce
						if( Abs( vOffset.y ) > Abs( vOffset.x ) )
						{
							rateScale += offsetScale * Abs( vOffset.y );
						}

						targetRaiseAmount	= pAutomobile->m_fHydraulicsUpperBound;
						raiseRate			= rateScale;
						break;
					}
					case WHS_BOUNCE_ALL:
					{
						targetRaiseAmount	= pAutomobile->m_fHydraulicsUpperBound;
						break;
					}
					case WHS_FREE:
					case WHS_BOUNCE_ACTIVE:
					case WHS_BOUNCE_LANDING:
					case WHS_BOUNCE_LANDED:
					{
						float t = ( ( vOffset.x * fStickX ) / Abs( vOffset.x ) ) + 
								  ( ( vOffset.y * fStickY ) / Abs( vOffset.y ) );

						float m_fHydraulicsT = t >= 0.0f ? (pAutomobile->m_fHydraulicsUpperBound * t) :  (pAutomobile->m_fHydraulicsLowerBound * (-t));
						targetRaiseAmount = m_fHydraulicsT;

						break;
					}
					default:
						break;
				}

				suspensionUpdated = true;
				pWheel->SetSuspensionTargetRaiseAmount( targetRaiseAmount, raiseRate );
			}
		}
	}

	if( suspensionUpdated )
	{
		pVehicle->ActivatePhysics();
				
		pAutomobile->SetTimeHyrdaulicsModified( fwTimer::GetTimeInMilliseconds() );
		pVehicle->GetVehicleAudioEntity()->SetHydraulicSuspensionInputAxis( fStickX, fStickY );
	}
	else// if( targetStateReached )
	{
		pVehicle->GetVehicleAudioEntity()->SetHydraulicSuspensionInputAxis(0.f, 0.f);
	}	

	if( pControl->GetVehicleHydraulicsControlToggle().IsDown() )
	{
		pVehicle->ActivatePhysics();
		pVehicle->SetSteerAngle( 0.0f );

		if( Abs( pVehicle->GetThrottle() ) < 0.1f &&
			pVehicle->GetVelocity().Mag2() < 5.0f )
		{
			pVehicle->SetHandBrake(true);
			pVehicle->SetBrake( 0.1f );
		}
	}

#if __BANK
	TUNE_GROUP_BOOL( HYDRAULICS_TUNE, sb_DrawHydraulicWheelStates, false );

	if( sb_DrawHydraulicWheelStates )
	{
		char vehDebugString[ 1024 ];

		static int StartX = 10;
		static int StartY = 27;

		int y = StartY;

		sprintf(vehDebugString,"All Raised: %d", (int)pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised );
		grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
		y++;

		const char* s_StateNames[] = { "Invalid",
										"Idle",
										"Lock",
										"Lock_Up_All",
										"Lock_Up",
										"Lock_Down_All",
										"Lock_Down",
										"Bounce Left Right",
										"Bounce Front Back",
										"Bounce",
										"Bounce_All",
										"Bounce_Active",
										"Bounce_Landing",
										"Bounce_Landed",
										"Bounce_All_Active",
										"Bounce_All_Landing",
										"Free" };

		for( int i = 0; i < numWheels; i++ )
		{
			CWheel* pWheel = pVehicle->GetWheel( i );

			eWheelHydraulicState wheelState			= pWheel->GetSuspensionHydraulicState(); 
			eWheelHydraulicState wheelTargetState	= pWheel->GetSuspensionHydraulicTargetState();

			sprintf(vehDebugString,"Wheel State: %s", s_StateNames[ (int)wheelState + 1 ] );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;

			sprintf(vehDebugString,"Wheel Target State: %s", s_StateNames[ (int)wheelTargetState + 1 ] );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;

			sprintf(vehDebugString,"Wheel Raise Amount: %.3f", pWheel->GetSuspensionRaiseAmount() );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;

			sprintf(vehDebugString,"Wheel Target Raise Amount: %.3f", pWheel->GetSuspensionTargetRaiseAmount() );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;

			sprintf(vehDebugString,"Wheel Suspension Force Modified: %d", (int)( pWheel->m_bSuspensionForceModified ) );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;
			
			//sprintf(vehDebugString,"Wheel Suspension Success Count: %d", (int)( pWheel->m_nHydraulicsSuccessfulBounceCount ) );
			//grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			//y++;

			sprintf(vehDebugString,"Wheel Raise Rate: %.3f", pWheel->GetSuspensionRaiseRate() );
			grcDebugDraw::PrintToScreenCoors( vehDebugString, StartX, y );
			y++;
			
		}
	}
#endif // #if __BANK
}

void CTaskVehiclePlayerDriveAutomobile::ProcessHydraulicsStateChange( CControl* pControl, CVehicle* pVehicle )
{
	eWheelHydraulicState result = WHS_INVALID;
	u32 uTimeHydraulicsControlPressed = 0;
	u32 uResetTimeThreshold = 50;
	u32 uPressedTimeThreshold = 200;

#if __BANK
	uResetTimeThreshold		= Max( uResetTimeThreshold, 17 + fwTimer::GetTimeStepInMilliseconds() );
	uPressedTimeThreshold	= Max( uPressedTimeThreshold, 167 + fwTimer::GetTimeStepInMilliseconds() );
#endif


	// toggle the suspension state if the button is up
	// a quick tap resets the suspension.
	bool buttonUp				= pControl->GetVehicleHydraulicsControlToggle().IsUp();
	bool buttonReleasedRecently	= pControl->GetVehicleHydraulicsControlToggle().HistoryReleased( uResetTimeThreshold, &uTimeHydraulicsControlPressed );
	bool buttonPressedRecently	= pControl->GetVehicleHydraulicsControlToggle().HistoryPressed( uPressedTimeThreshold, &uTimeHydraulicsControlPressed );
	bool resetSuspension		= buttonUp && buttonPressedRecently;
	bool donkSuspension			= pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS );

	bool toggleState = false;
	static bool stateChanged = false;

	int numWheels = pVehicle->GetNumWheels();

	if( resetSuspension )
	{
		toggleState		= true;
		stateChanged	= false;
	}
	else if( buttonUp ) // the button is up 
	{
		stateChanged = false; 

		if( buttonReleasedRecently )// the button has been released recently and all the suspension is locked down
		{
			stateChanged = true;

			// only do this if the wheels are all locked down
			bool bounceAll		= !donkSuspension;
			bool lockPosition	= true;

			for( int i = 0; i < numWheels; i++ )
			{
				CWheel* pWheel = pVehicle->GetWheel( i );

				if( pWheel )
				{
					eWheelHydraulicState wheelState = pWheel->GetSuspensionHydraulicState();

					if( wheelState != WHS_LOCK_DOWN_ALL )
					{
						bounceAll = false;
					}

#if RSG_PC
					if( wheelState == WHS_LOCK_UP_ALL || 
						wheelState == WHS_LOCK_DOWN_ALL ||
						wheelState == WHS_IDLE ||
						( wheelState >= WHS_BOUNCE_START && 
						  wheelState < WHS_BOUNCE_LAST_ACTIVE ) )
#else
					if( wheelState == WHS_LOCK_UP_ALL || 
						wheelState == WHS_IDLE )
#endif
					{
						lockPosition = false;
						break;
					}
				}
			}

			if( bounceAll )
			{
				result = WHS_BOUNCE_ALL;
			}
			else if( lockPosition )
			{
				result = WHS_LOCK;
			}
		}
	}
	else if( !buttonPressedRecently )// the button is still held and was pressed recently
	{
		toggleState = true;
	}

	if( toggleState &&
		!stateChanged )
	{
		stateChanged = true;

		if( resetSuspension )
		{
			result = WHS_IDLE;
		}
		else if( !pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised )
		{
			result = WHS_LOCK_UP_ALL;
		}
		else if( !donkSuspension )
		{
			result = WHS_LOCK_DOWN_ALL;
		}
	}

	bool anyWheelStateChanged = false;

	if( result != WHS_INVALID )
	{
		for( int i = 0; i < numWheels; i++ )
		{
			CWheel* pWheel = pVehicle->GetWheel( i );

			if( pWheel && result != pWheel->GetSuspensionHydraulicTargetState() )
			{
				pWheel->SetSuspensionHydraulicTargetState( result );
				anyWheelStateChanged = true;
			}
		}
	}

	if(anyWheelStateChanged)
	{
		switch(result)
		{
		case WHS_BOUNCE_ALL:
			if(numWheels > 0)
			{
				pVehicle->TriggerHydraulicBounceSound();
			}
			break;

		case WHS_LOCK_UP_ALL:
			{
				u32 numRaisedWheels = 0;

				if(donkSuspension)
				{
					CAutomobile* pAutomobile = static_cast< CAutomobile* >( pVehicle );

					for( int i = 0; i < numWheels; i++ )
					{
						CWheel* pWheel = pVehicle->GetWheel( i );

						if( pWheel && 
							pWheel->GetSuspensionTargetRaiseAmount() == pWheel->GetSuspensionRaiseAmount() &&
							pWheel->GetSuspensionRaiseAmount() == pAutomobile->m_fHydraulicsUpperBound )
						{
							numRaisedWheels++;
						}
					}
				}

				if(numRaisedWheels < numWheels)
				{
					pVehicle->TriggerHydraulicActivationSound();
				}
			}
			break;

		case WHS_LOCK_DOWN_ALL:
		case WHS_IDLE:
			if(donkSuspension)
			{
				bool allWheelsAtTarget = true;

				for( int i = 0; i < numWheels; i++ )
				{
					CWheel* pWheel = pVehicle->GetWheel( i );

					if( pWheel && pWheel->GetSuspensionTargetRaiseAmount() != pWheel->GetSuspensionRaiseAmount() )
					{
						allWheelsAtTarget = false;
						break;
					}
				}

				if(allWheelsAtTarget)
				{
					pVehicle->TriggerHydraulicDeactivationSound();
				}
			}
			else
			{
				pVehicle->TriggerHydraulicDeactivationSound();
			}
			break;

		default:
			break;
		}
	}
}

bool CTaskVehiclePlayerDriveAutomobile::ProcessHydraulicsTilt( CControl* pControl, CVehicle* pVehicle, float fStickX, float fStickY )
{
	bool result = false;

	static dev_u32 uPressedTimeThreshold = 200;
	u32 uTimeHydraulicsControlPressed = 0;

	bool buttonHeld				= pControl->GetVehicleHydraulicsControlToggle().IsDown();
	bool buttonPressedRecently	= pControl->GetVehicleHydraulicsControlToggle().HistoryPressed( uPressedTimeThreshold, &uTimeHydraulicsControlPressed );
	bool donkSuspension			= pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS );

	static float fPrevStickX = 0.0f;
	static float fPrevStickY = 0.0f;
	static float sf_deadZone = 0.05f;

	if( Abs( fStickX ) < sf_deadZone )
	{
		fStickX = 0.0f;
	}
	if(Abs( fStickY ) < sf_deadZone )
	{
		fStickY = 0.0f;
	}

	static float sfHeldDuration = 0.0f;
	static float sfLockDuration = 0.0f;
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, lockDuration, 0.5f, 0.0f, 5.0f, 0.1f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, unlockDuration, 0.0f, 0.0f, 5.0f, 0.1f );
	float timeStep		= fwTimer::GetTimeStep();
	float invTimeStep	= fwTimer::GetInvTimeStep();
	u32 numNewWheelsBouncing = 0;
	u32 numInitialWheelsLockedDown = 0;
	u32 numWheelsInTiltLockUpState = 0;
	u32 numWheelsInTiltLockDownState = 0;

	if( buttonHeld &&
		!buttonPressedRecently )
	{
		CAutomobile* pAutomobile = static_cast< CAutomobile* >( pVehicle );
		float stickDeltaX = ( Abs( fStickX ) - Abs( fPrevStickX ) ) * invTimeStep;
		float stickDeltaY = ( Abs( fStickY ) - Abs( fPrevStickY ) ) * invTimeStep;
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, bounceStickDelta, 13.5f, 0.0f, 100.0, 0.5f );
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, resetTollerance, 0.2f, 0.0f, 1.0f, 0.01f );
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, donkResetTollerance, 0.8f, 0.0f, 1.0f, 0.01f );
		bool bounce = false;
		float fBounceStickX = -fStickX;
		float fBounceStickY = -fStickY;

		if( !donkSuspension &&
			( stickDeltaX > bounceStickDelta ||
			  stickDeltaY > bounceStickDelta ) )
		{
			bounce = true;

			if( sb_invertBounceStick )
			{
				fBounceStickY *= -1.0f;
				fBounceStickX *= -1.0f;
			}
		}

		int numWheels		= pVehicle->GetNumWheels();
		int halfNumWheels	= numWheels / 2;
		const bool isUpsideDownOrOnSide = (pVehicle->IsOnItsSide() || pVehicle->IsUpsideDown());

		for( int i = 0; i < numWheels; i++ )
		{
			CWheel *pWheel = pVehicle->GetWheel( i );

			if( pWheel )
			{
				eWheelHydraulicState currentState	= pWheel->GetSuspensionHydraulicState();
				eWheelHydraulicState targetState	= pWheel->GetSuspensionHydraulicTargetState();

				if(donkSuspension)
				{
					if(currentState == targetState && currentState == WHS_IDLE)
					{
						numInitialWheelsLockedDown++;
					}
				}
				else
				{
					if(currentState == targetState)
					{
						if(currentState == WHS_LOCK_DOWN)
						{
							numWheelsInTiltLockDownState++;
						}
						else if(currentState == WHS_LOCK_UP)
						{
							numWheelsInTiltLockUpState++;
						}
					}					

					if(currentState == WHS_LOCK_DOWN_ALL || currentState == WHS_LOCK_DOWN || currentState == WHS_IDLE)
					{
						numInitialWheelsLockedDown++;
					}
				}				

				if( ( currentState < WHS_BOUNCE_START ||
					  currentState > WHS_BOUNCE_END ) && 
					( targetState < WHS_BOUNCE_START ||
					  targetState > WHS_BOUNCE_END ) )
				{
					Vector3 vOffset = CWheel::GetWheelOffset(pAutomobile->GetVehicleModelInfo(), pWheel->GetHierarchyId());
					vOffset.z = 0.0f;
					vOffset.NormalizeSafe(vOffset);

					if( donkSuspension )
					{
						if( vOffset.x != 0.0f )
						{
							vOffset.x = vOffset.x > 0.0f ? 1.0f : -1.0f;
						}
						if( vOffset.y != 0.0f )
						{
							vOffset.y = vOffset.y > 0.0f ? 1.0f : -1.0f;
						}
					}
				
					if( fStickX != 0.0f ||
						fStickY != 0.0f ||
						bounce )
					{
						if( bounce )
						{
							float t = ( vOffset.x * fBounceStickX ) + 
									  ( vOffset.y * fBounceStickY );

							eWheelHydraulicState bounceState = WHS_BOUNCE_FRONT_BACK;

							int oppositeWheelIndex = i;
							if( Abs( fBounceStickX ) > Abs( fBounceStickY ) )
							{
								bounceState = WHS_BOUNCE_LEFT_RIGHT;

								if( i % 2 == 1 )
								{
									oppositeWheelIndex = i - 1;
								}
								else
								{
									oppositeWheelIndex = i + 1;
								}
							}
							else
							{
								if( i < halfNumWheels )
								{
									oppositeWheelIndex = i + halfNumWheels;
								}
								else
								{
									oppositeWheelIndex = i - halfNumWheels;
								}
							}

							CWheel* pOppositeWheel = pVehicle->GetWheel( oppositeWheelIndex );

							if( t > 0.0f )
							{
								if( pWheel->GetSuspensionRaiseAmount() <= 0.0f &&
									( !pOppositeWheel ||
									  pOppositeWheel->GetSuspensionHydraulicState() == WHS_LOCK_UP ) ) // only bounce if both this wheel and the opposite one are in the correct position
								{
									if( pWheel->GetSuspensionHydraulicTargetState() != bounceState )
									{
										pWheel->SetSuspensionHydraulicTargetState( bounceState );

										if(isUpsideDownOrOnSide || pWheel->GetIsTouching())
										{
											numNewWheelsBouncing++;
										}
									}
								}
								else
								{
									pWheel->SetSuspensionHydraulicTargetState( WHS_IDLE );
								}
							}
							else if( t < 0.0f )
							{
								if( pWheel->GetSuspensionHydraulicTargetState() != WHS_LOCK_UP )
								{
									pWheel->SetSuspensionHydraulicTargetState( WHS_LOCK_UP );

									if(isUpsideDownOrOnSide || pWheel->GetIsTouching())
									{
										numNewWheelsBouncing++;
									}									
								}
							}

							if( !sb_invertBounceStick )
							{
								fStickX = -fBounceStickX;
								fStickY = -fBounceStickY;
							}
						}
						else
						{
							result = true;

							// if we're not bouncing
							// if the wheel is free we need to check to see if we should lock this wheel in position
							bool wheelLocked = ( currentState >= WHS_LOCK_START &&
												 currentState <= WHS_LOCK_END ) ||
											   ( targetState >= WHS_LOCK_START &&
												 targetState <= WHS_LOCK_END );

							float t = ( vOffset.x * fStickX ) + 
									  ( vOffset.y * fStickY );

							if( !wheelLocked )
							{
								if( sfLockDuration > lockDuration ) 
								{
									if( t > 0.0f )
									{
										pWheel->SetSuspensionHydraulicTargetState( WHS_LOCK_UP );
									}
									else if( t < -0.0f )
									{
										pWheel->SetSuspensionHydraulicTargetState( WHS_LOCK_DOWN );
									}
								}
								else if( currentState != WHS_BOUNCE_ACTIVE &&
										 currentState != WHS_BOUNCE_LANDING &&
										 currentState != WHS_BOUNCE_LANDED )
								{
									pWheel->SetSuspensionHydraulicTargetState( WHS_FREE );
								}
							}
							else if( wheelLocked &&
									 sfLockDuration > unlockDuration ) // if the wheel is locked then we need to check to see if we unlock it
							{
								if( ( t > resetTollerance || 
									  ( donkSuspension &&
									    t < -donkResetTollerance ) ) &&
									( currentState == WHS_LOCK_DOWN || 
									  currentState == WHS_LOCK_DOWN_ALL ) ) 
								{
									pWheel->SetSuspensionHydraulicTargetState( WHS_FREE );
								}
								else if( ( t < -resetTollerance || 
									      ( donkSuspension &&
									        t > donkResetTollerance  ) ) &&
										 ( currentState == WHS_LOCK_UP || 
										   currentState == WHS_LOCK_UP_ALL ) )
								{
									pWheel->SetSuspensionHydraulicTargetState( WHS_FREE );
								}
							}
						}
					}
				}
			}
		}
		
		// If we go from fully idle to > 1 wheel being locked, trigger the activate sound
		if((s32)numInitialWheelsLockedDown == numWheels)
		{
			for( int i = 0; i < numWheels; i++ )
			{
				CWheel *pWheel = pVehicle->GetWheel( i );
								
				if(pWheel && pWheel->GetIsTouching())
				{
					if(pWheel->GetSuspensionHydraulicTargetState() == WHS_LOCK_UP || pWheel->GetSuspensionHydraulicTargetState() == WHS_LOCK_UP_ALL || pWheel->GetSuspensionHydraulicTargetState() == WHS_FREE)
					{
						pVehicle->TriggerHydraulicActivationSound();
						break;
					}
				}
			}
		}		
	}
	else
	{
		// if the button is no longer pressed lock any wheels that were free
		for( int i = 0; i < pVehicle->GetNumWheels(); i++ )
		{
			if( CWheel *pWheel = pVehicle->GetWheel( i ) )
			{
				eWheelHydraulicState currentState = pWheel->GetSuspensionHydraulicState();

				if( currentState == WHS_FREE )
				{
					pWheel->SetSuspensionHydraulicTargetState( WHS_LOCK );
				}
			}
		}
	}

	// Treat going from tilted (ie 2 wheels up, 2 wheels down) to not as a bounce, so that we play a bounce hiss sound
	if(numNewWheelsBouncing == 0 && numWheelsInTiltLockDownState == 2 && numWheelsInTiltLockUpState == 2)
	{
		numWheelsInTiltLockUpState = 0;
		numWheelsInTiltLockDownState = 0;

		for( int i = 0; i < pVehicle->GetNumWheels(); i++ )
		{
			if( CWheel *pWheel = pVehicle->GetWheel( i ) )
			{
				eWheelHydraulicState wheelTargetState = pWheel->GetSuspensionHydraulicTargetState();

				if( wheelTargetState == WHS_LOCK_DOWN )
				{
					numWheelsInTiltLockDownState++;
				}
				else if( wheelTargetState == WHS_LOCK_UP )
				{
					numWheelsInTiltLockUpState++;
				}
			}
		}

		if(numWheelsInTiltLockUpState != 2 || numWheelsInTiltLockDownState != 2)
		{
			numNewWheelsBouncing++;
		}
	}
	
	// If one or more wheels entered the bounce state this frame, then trigger
	// some hydraulic bounce audio
	if(numNewWheelsBouncing > 0)
	{
		pVehicle->TriggerHydraulicBounceSound();
	}

	// if the stick position hasn't changed then we increment the lock timer
	if( ( fPrevStickX != 0.0f ||
		  fPrevStickY != 0.0f ) &&
		fPrevStickX == fStickX &&
		fPrevStickY == fStickY )
	{
		sfLockDuration += timeStep;
	}
	else
	{
		fPrevStickX = fStickX;
		fPrevStickY = fStickY;
		sfLockDuration = 0.0f;
	}

	if( result )
	{
		sfHeldDuration += timeStep;
	}
	else
	{
		sfHeldDuration = 0.0f;
	}

	return result;
}

void CTaskVehiclePlayerDriveAutomobile::ProcessSuspensionDrop( CVehicle* pVehicle )
{
    static dev_float maxSpeedToLowerSuspension = 1.0f;

    float targetRaiseAmount = 0.0f;
    audVehicleAudioEntity* vehicleAudioEntity = pVehicle->GetVehicleAudioEntity();
    static dev_float sfMaxStuckTimeForSuspensionRaise = 2000.0f; 

    if( pVehicle->GetVelocity().Mag2() < maxSpeedToLowerSuspension &&
		( !pVehicle->InheritsFromAutomobile() || !static_cast< CAutomobile* >( pVehicle )->m_nAutomobileFlags.bWheelieModeEnabled ) &&
        ( ( pVehicle->GetVehicleDamage()->GetStuckTimer( VEH_STUCK_JAMMED ) < sfMaxStuckTimeForSuspensionRaise || pVehicle->GetThrottle() == 0.0f ) ||
            ( pVehicle->InheritsFromAutomobile() && static_cast< CAutomobile* >( pVehicle )->m_nAutomobileFlags.bInBurnout ) ) ) // raise the suspension back up if the vehicle is jammed
    {
        if(vehicleAudioEntity)
        {
            vehicleAudioEntity->TriggerStationarySuspensionDrop();
        }

        targetRaiseAmount = -pVehicle->pHandling->m_fSuspensionRaise;
    }
    else
    {
        if(vehicleAudioEntity)
        {
            vehicleAudioEntity->CancelStationarySuspensionDrop();
        }
    }

    bool suspensionUpdated = false;
    int numWheels = pVehicle->GetNumWheels();
    static_cast< CAutomobile* >( pVehicle )->SetHydraulicsRate( Max( static_cast< CAutomobile* >( pVehicle )->GetHydraulicsRate(), 0.25f ) );
    static dev_float suspensionLowerRate = 0.4f;
	bool steeringLockReduced = false;

    for(int i = 0; i < numWheels; i++)
    {
        if( CWheel *pWheel = pVehicle->GetWheel( i ) )
        {
            if( targetRaiseAmount != pWheel->GetSuspensionRaiseAmount() )
            {
                //if( targetRaiseAmount < pWheel->GetSuspensionRaiseAmount() ||
                //    pVehicle->GetThrottle() > 0.0f )
                {
                    pWheel->SetSuspensionTargetRaiseAmount( targetRaiseAmount, suspensionLowerRate );
                    suspensionUpdated = true;
                }
            }

			float currentRaiseAmount = pWheel->GetSuspensionRaiseAmount();
			if( !steeringLockReduced &&
				pWheel->GetConfigFlags().IsFlagSet( WCF_STEER ) &&
				currentRaiseAmount < 0.0f )
			{
				float currentLoweredAmount = currentRaiseAmount;
				currentLoweredAmount /= pVehicle->pHandling->m_fSuspensionRaise;
				currentLoweredAmount *= 0.8f;

				pVehicle->SetSteerAngle( pVehicle->GetSteerAngle() * ( 1.0f - Abs( currentLoweredAmount ) ) );

				steeringLockReduced = true;
			}
        }

        if( suspensionUpdated )
        {
            pVehicle->ActivatePhysics();

            for( int i = 0; i < numWheels; i++ )
            {
                if( CWheel *pWheel = pVehicle->GetWheel( i ) )
                {
                    pWheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePlayerDriveAutomobile::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Drive&&iState<=State_DiggerMode);
	static const char* aStateNames[] = 
	{
		"State_Drive",
		"State_NoDriver",
		"State_DiggerMode"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveDiggerArm
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveDiggerArm::CTaskVehiclePlayerDriveDiggerArm(CVehicleGadgetArticulatedDiggerArm *diggerGadget) :
CTaskVehiclePlayerDrive(),
m_DiggerGadget(diggerGadget)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveDiggerArm::ProcessDriverInputsForPlayerOnUpdate(CPed *UNUSED_PARAM(pPlayerPed), CControl *pControl, CVehicle *pVehicle)
{
	Assert(m_DiggerGadget);

	if(pControl==NULL)
		return;

	//Make sure the digger isn't still moving
	pVehicle->SetBrake(1.0f);
	pVehicle->SetHandBrake(true);
	pVehicle->SetThrottle(0.0f);

	pControl->SetVehicleSteeringExclusive();
	float fDesiredSteerInputUpDown = pControl->GetVehicleSteeringUpDown().GetNorm();
	float fDesiredSteerInputLeftRight = pControl->GetVehicleSteeringLeftRight().GetNorm();
	float fDesiredGunInputUpDown = pControl->GetVehicleGunUpDown().GetUnboundNorm();
	float fDesiredGunInputLeftRight = pControl->GetVehicleGunLeftRight().GetUnboundNorm();

#if RSG_PC
	if(pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
	{
		fDesiredSteerInputLeftRight *= c_fMouseAdjustmentScale;
	}
	if(pControl->GetVehicleSteeringUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
	{
		fDesiredSteerInputUpDown *= c_fMouseAdjustmentScale;
	}
	if(pControl->GetVehicleGunLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
	{
		fDesiredGunInputLeftRight *= c_fMouseAdjustmentScale;
	}
	if(pControl->GetVehicleGunUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
	{
		fDesiredGunInputUpDown *= c_fMouseAdjustmentScale;
	}
#endif

	m_DiggerGadget->SetDesiredAcceleration(DIGGER_JOINT_BASE, -fDesiredSteerInputLeftRight );
	m_DiggerGadget->SetDesiredAcceleration(DIGGER_JOINT_BOOM, fDesiredSteerInputUpDown );
	m_DiggerGadget->SetDesiredAcceleration(DIGGER_JOINT_STICK, fDesiredGunInputUpDown );
	m_DiggerGadget->SetDesiredAcceleration(DIGGER_JOINT_BUCKET, fDesiredGunInputLeftRight );
}



//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveBike
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveBike::CTaskVehiclePlayerDriveBike() :
m_uLastSprintTime(0),
m_uStartTime(0),
m_fMaxBicycleForwardnessSlowCycle(ms_fBICYCLE_MAX_FORWARDNESS_SLOW), 
m_bClearedForwardness(false),
m_bStartFinished(false),
m_fStartingThrottle(0.0f),
CTaskVehiclePlayerDrive()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BIKE);
}

//////////////////////////////////////////////////////////////////////
//Moving tuning values
static dev_float sfPlayerBikeSpeedSteerMult = 0.15f;
static dev_float sfPlayerBicycleSpeedSteerMult = 0.075f;

static dev_float sfPlayerBikeSpeedSteerFwdThreshold = 0.15f;
static dev_float sfPlayerBikeSpeedSteerSideRatioThreshold = 0.1f;
static dev_bool  sbPlayerBikeSpeedSteerAutoCentre = true;
static dev_float sfPlayerBikeSpeedSteerAutoCentreMax = ( DtoR * 10.0f);

static dev_float sfPlayerBicycleTuckSteerMult = 0.05f;

//Stationary tuning values
static dev_bool  sbPlayerBikeStationarySteerAutoCentre = true;
static dev_float sfPlayerBikeNoInputSteerAutoCentreMax = ( DtoR * 5.0f);

void CTaskVehiclePlayerDriveBike::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CBike*>(pVehicle));
	CBike *pBike = static_cast<CBike*>(pVehicle);
	float timeStep = fwTimer::GetTimeStep();

	Assert(pPlayerPed);
	if (!pPlayerPed->IsControlledByLocalPlayer())
		return;

	if(pControl==NULL)
		return;
/* //Disable this on bikes for the time being
    // Process whether we should enable Franklin's special ability.
    if( pPlayerPed->GetSpecialAbility() && pPlayerPed->GetSpecialAbility()->GetType() == SAT_CAR_SLOWDOWN )
    {
        if(pControl->GetVehicleHandBrakeAlt().IsDown())
        {
            pPlayerPed->GetSpecialAbility()->Activate();
        }
        else
        {
            pPlayerPed->GetSpecialAbility()->Deactivate();
        }
    }
*/
	if( pControl->GetVehicleExit().IsDown() && m_bExitButtonUp &&
		!CControlMgr::GetMainPlayerControl().IsInputDisabled( INPUT_VEH_EXIT ) )// we want to get out of the car, so we need to brake
	{
		pBike->SetHandBrake( true );
	}
	else if (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) 
	{
		pBike->SetHandBrake(false);//no handbrake on bicycles
	}
	else
	{
		pBike->SetHandBrake(pBike->GetIsHandBrakePressed(pControl));
	}


	float fDesiredSteerInputUpDown = 0.0f;
	float fDesiredSteerInputLeftRight = 0.0f;

	float fMovingSteerSmoothRate = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? ms_fBICYCLE_STD_STEER_SMOOTH_RATE : ms_fBIKE_STD_STEER_SMOOTH_RATE;
	float fStoppedSteerSmoothRate = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? ms_fBICYCLE_STOPPED_STEER_SMOOTH_RATE : ms_fBIKE_STOPPED_STEER_SMOOTH_RATE;
	float fLeanSmoothRate = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? ms_fBICYCLE_STD_LEAN_SMOOTH_RATE : ms_fBIKE_STD_LEAN_SMOOTH_RATE;
	
	float fSteerSmoothRate = fStoppedSteerSmoothRate;
	float fFwdSpeedFrac = DotProduct(VEC3V_TO_VECTOR3(pBike->GetTransform().GetB()), pBike->GetVelocityIncludingReferenceFrame());
	fFwdSpeedFrac /= (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ?  ms_fBICYCLE_STEER_SMOOTH_RATE_MAXSPEED : ms_fBIKE_STEER_SMOOTH_RATE_MAXSPEED;

	// Or when putting on helmet
	if(!pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
	{
		pBike->GetBikeLeanInputs(fDesiredSteerInputLeftRight, fDesiredSteerInputUpDown);

 		if( abs(fDesiredSteerInputLeftRight) < ms_fMOTION_STEER_DEAD_ZONE )
 		{
 			fDesiredSteerInputLeftRight = 0.0f;
 		}

		fMovingSteerSmoothRate *= ms_fMOTION_STEER_SMOOTH_RATE_MULT;
		fStoppedSteerSmoothRate *= ms_fMOTION_STEER_SMOOTH_RATE_MULT;
	}


	if(fFwdSpeedFrac > 1.0f)
		fSteerSmoothRate = fMovingSteerSmoothRate;
	else if(fFwdSpeedFrac > 0.0f)
		fSteerSmoothRate = fFwdSpeedFrac*fMovingSteerSmoothRate + (1.0f - fFwdSpeedFrac)*fStoppedSteerSmoothRate;

    // If we are using Franklin's special ability we shouldn't slow down the steering.
    if( pPlayerPed->GetSpecialAbility() && pPlayerPed->GetSpecialAbility()->GetType() == SAT_CAR_SLOWDOWN )
    {
        if( pPlayerPed->GetSpecialAbility()->IsActive() )
        {
            timeStep = fwTimer::GetNonPausableCamTimeStep() * sfSpecialAbilitySteeringTimeStepMult;// We want to use a time step for the steering which can't be modified by scaling time
        }
    }

	//reduce steering beneath the limits
	if(rage::Abs(fDesiredSteerInputLeftRight) < ms_fPAD_STEER_REUCTION_PAD_END)
	{
		fDesiredSteerInputLeftRight *= (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ?  ms_fBICYCLE_STEER_REDUCTION_UNDER_LIMIT_MULT : ms_fBIKE_STEER_REDUCTION_UNDER_LIMIT_MULT;;
	}

	// progressive steering
	float fSteeringChange = (fDesiredSteerInputLeftRight - pBike->m_fSteerInput) * rage::Min(1.0f,fSteerSmoothRate * timeStep);

	pBike->m_fSteerInput = pBike->m_fSteerInput + fSteeringChange;

	pBike->m_fSteerInput = Clamp(pBike->m_fSteerInput, -1.0f, 1.0f);

	// progressive leaning
	fDesiredSteerInputUpDown = Clamp(fDesiredSteerInputUpDown, -1.0f, 1.0f);

	if( pVehicle->m_bInvertControls )
	{
		fDesiredSteerInputUpDown = -fDesiredSteerInputUpDown;
	}

	pBike->m_fAnimLeanFwd = pBike->m_fAnimLeanFwd + (fDesiredSteerInputUpDown - pBike->m_fAnimLeanFwd) * rage::Min(1.0f,fLeanSmoothRate * timeStep);
	pBike->m_fAnimLeanFwd = Clamp(pBike->m_fAnimLeanFwd, -1.0f, 1.0f);

	if (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
		bool bForceBrake = false;
		if (pControl->GetVehicleExit().IsDown())
		{
			if (!pVehicle->CanPedJumpOutCar(pPlayerPed))
			{
				pVehicle->SetHandBrake(true);
				bForceBrake = true;
			}
		}

		ProcessPlayerControlInputsForBrakeAndGasPedalBicycle(pControl, bForceBrake, pBike, pPlayerPed);
	}
	else
	{
		ProcessPlayerControlInputsForBrakeAndGasPedal(pControl, false, pVehicle);
	}

	// Jumping Bike code
	if(pBike->pHandling && pBike->pHandling->GetBikeHandlingData() && pBike->pHandling->GetBikeHandlingData()->m_fJumpForce > 0.0f)
	{
		//Have to work out whether the button is pressed by using the history as special abilities use the same button and clears the input, this logic clones that of GetPedDuck
		static dev_u32 HISTORY_MS = 300;
		u32 timePressed = 0;
		if(pControl->GetVehicleJump().HistoryPressed(HISTORY_MS, &timePressed))
		{
			if(timePressed > pBike->m_uJumpLastPressedTime)
			{
				pBike->m_uJumpLastPressedTime = timePressed;
				pBike->m_nBikeFlags.bJumpPressed = true;
				pBike->m_nBikeFlags.bJumpReleased = false;
			}
		}
		else if(pBike->m_uJumpLastPressedTime > 0 && pBike->m_uJumpLastReleasedTime == 0 && pControl->GetVehicleJump().HistoryReleased(HISTORY_MS, &timePressed))
		{
			pBike->m_nBikeFlags.bJumpReleased = true;
			pBike->m_nBikeFlags.bJumpPressed = false;
		}
	}


	// script influence on steering input
	if( pVehicle->m_bInvertControls )
	{
		pVehicle->m_fSteerInput = -pVehicle->m_fSteerInput;
	}
	pBike->m_fSteerInput += 0.5f*pBike->m_fSteerInputBias;
	pBike->m_fSteerInput = Clamp(pBike->m_fSteerInput, -1.0f, 1.0f);

	if (pBike->GetIntelligence()->GetRecordingNumber() < 0 || CVehicleRecordingMgr::GetUseCarAI(pBike->GetIntelligence()->GetRecordingNumber()))		// Only overwrite the steer angle if this bike doesn't have a recording played on it. (This can happen for the players' bike)
	{
        float fDesiredSteerAngle = pBike->m_fSteerInput * pBike->pHandling->m_fSteeringLock;

        if(pBike->GetDriver() && pBike->GetDriver()->IsLocalPlayer())// For the local player, do some extra steering correction
        {
            Vector3 vBikeVelocity = pBike->GetVelocityIncludingReferenceFrame();

            float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(pBike->GetTransform().GetB()), vBikeVelocity);
            float fSideSpeed = DotProduct(VEC3V_TO_VECTOR3(pBike->GetTransform().GetA()), vBikeVelocity);
			
			float fPlayerBikeSpeedSteerMult = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? sfPlayerBicycleSpeedSteerMult : sfPlayerBikeSpeedSteerMult;

            float fPlayerBikeSpeedSteerFwdThreshold = sfPlayerBikeSpeedSteerFwdThreshold * pBike->pHandling->m_fEstimatedMaxFlatVel;
            if(fFwdSpeed > fPlayerBikeSpeedSteerFwdThreshold && 
                !(fSideSpeed * fDesiredSteerAngle < -sfPlayerBikeSpeedSteerSideRatioThreshold*rage::Abs(fFwdSpeed)) )
            {
                fDesiredSteerAngle /= 1.0f + fPlayerBikeSpeedSteerMult * (fFwdSpeed - fPlayerBikeSpeedSteerFwdThreshold);
            }

			float fSteeringLockReduction = 1.0f;
			if(pBike->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsTuckedOnBicycleThisFrame))
			{
				fSteeringLockReduction = sfPlayerBicycleTuckSteerMult;
			}

			//Steer reduction rates
			float fReductionConst = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? 0.75f : 0.55f;
			float fStationaryReductionConst = (pBike->GetVehicleType() == VEHICLE_TYPE_BICYCLE) ? 0.80f : 0.70f;

			bool bSteerReduction = false;
			float fSteerReductionConst = fReductionConst;
            if(fFwdSpeed > fPlayerBikeSpeedSteerFwdThreshold && sbPlayerBikeSpeedSteerAutoCentre)
            {
				TUNE_GROUP_FLOAT(FIRST_PERSON_IN_VEHICLE_TUNE, BICYCLE_STEER_AUTO_CENTRE_MAX, 2.5f, 0.0f, 20.0f, 0.01f);
				float fSteerAutoCentreMax = CTaskMotionInAutomobile::IsFirstPersonDrivingBicycle(*pBike->GetDriver(), *pBike) ? BICYCLE_STEER_AUTO_CENTRE_MAX * DtoR : sfPlayerBikeSpeedSteerAutoCentreMax;
				float fAutoCentreSteerAngle = 0.0f;
				static dev_float sfMinLeanAngleToAutoCentreSteer =  0.01f;
				if(rage::Abs(pBike->GetLeanAngle()) > sfMinLeanAngleToAutoCentreSteer && !pBike->IsInAir())
				{
					fAutoCentreSteerAngle = rage::Atan2f(-fSideSpeed, fFwdSpeed);
					fAutoCentreSteerAngle = rage::Clamp(fAutoCentreSteerAngle, -fSteerAutoCentreMax, fSteerAutoCentreMax);
				}

                fDesiredSteerAngle = rage::Clamp(fDesiredSteerAngle + fAutoCentreSteerAngle, -pBike->pHandling->m_fSteeringLock*fSteeringLockReduction, pBike->pHandling->m_fSteeringLock*fSteeringLockReduction);
				bSteerReduction = rage::Abs(fDesiredSteerAngle) > rage::Abs(pBike->GetSteerAngle()) && !pBike->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsTuckedOnBicycleThisFrame);
			}
		    else if(sbPlayerBikeStationarySteerAutoCentre)
			{
				if(IsClose(fDesiredSteerInputLeftRight, 0.0f, FLT_EPSILON))
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_IN_VEHICLE_TUNE, BICYCLE_NO_INPUT_STEER_AUTO_CENTRE_MAX, 2.5f, 0.0f, 20.0f, 0.01f);
					float fNoInputSteerAutoCentreMax =  CTaskMotionInAutomobile::IsFirstPersonDrivingBicycle(*pBike->GetDriver(), *pBike) ? BICYCLE_NO_INPUT_STEER_AUTO_CENTRE_MAX * DtoR : sfPlayerBikeNoInputSteerAutoCentreMax;
					float fDesiredChange = rage::Clamp(fDesiredSteerAngle - pBike->GetSteerAngle(), -fNoInputSteerAutoCentreMax, fNoInputSteerAutoCentreMax);
					fDesiredSteerAngle = fDesiredChange + pBike->GetSteerAngle();
				}
				else
				{
					bSteerReduction = true;
				}
				
				fDesiredSteerAngle = rage::Clamp(fDesiredSteerAngle, -pBike->pHandling->m_fSteeringLock*fSteeringLockReduction, pBike->pHandling->m_fSteeringLock*fSteeringLockReduction);
				fSteerReductionConst = fStationaryReductionConst;
			}

			float fOldSteerAngle = pBike->GetSteerAngle();
			float fSteerDelta = fDesiredSteerAngle-fOldSteerAngle;

			float fSteerReduction = 0.0f;

			if(bSteerReduction)
			{
				fSteerReduction = fSteerReductionConst * fSteerDelta;
			}
			else if(pBike->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsTuckedOnBicycleThisFrame))// If we're past the steering limits we want to slowly bring the steering back under the limits when in the tucked mode.
			{
				if(rage::Abs(pBike->GetSteerAngle()) > pBike->pHandling->m_fSteeringLock*fSteeringLockReduction)
				{
					fSteerReduction = fSteerReductionConst * fSteerDelta;
				}
			}

			float fSteerChange = (fSteerDelta - fSteerReduction);

			if(Sign(fSteerDelta) != Sign(fSteerChange))
			{
				fSteerChange = 0.0f;
			}

			fDesiredSteerAngle = fOldSteerAngle + fSteerChange;
		}

		pBike->SetSteerAngle(fDesiredSteerAngle); 
	}

}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveBike::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *pVehicle)
{
	if (pVehicle->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive)
	{
		// If the player has no control (cut scene for instance) we will hit the brakes.
		if(pVehicle->HasContactWheels())
		{
			pVehicle->SetBrake(1.0f);
			pVehicle->SetHandBrake(false);
			pVehicle->SetThrottle(0.0f);
			FindPlayerPed()->GetPlayerInfo()->KeepAreaAroundPlayerClear();
			// Limit the velocity
			float Speed = pVehicle->GetVelocity().Mag();

			if (Speed > ms_fBIKE_MAXSPEEDNOW && pVehicle->m_nVehicleFlags.bStopInstantlyWhenPlayerInactive)
			{
				float	NewSpeed = rage::Max(ms_fBIKE_MAXSPEEDNOW, Speed - ms_fBIKE_MAXSPEEDCHANGE);
				pVehicle->SetVelocity( pVehicle->GetVelocity() * (NewSpeed / Speed) );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveBike::ProcessPlayerControlInputsForBrakeAndGasPedalBicycle(CControl* pControl, bool bForceBrake, CBike *pBike, CPed* pPlayerPed)
{
	CVehicleModelInfo* pVehicleModelInfo = pBike->GetVehicleModelInfo();
	CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
	//get the vehicles reference frame velocity, so if they are driving on a plane we car work out the relative velocity rather then just using the absolute
	Vector3 vRelativeVelocity = pBike->GetVelocityIncludingReferenceFrame();
	const float fCurrentSpeed = vRelativeVelocity.Mag();
	const float fMaxSpeed = pHandlingData->m_fEstimatedMaxFlatVel;

	float fSpeedRatio = fCurrentSpeed / fMaxSpeed;

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, BICYCLE_THROTTLE_RECOVERY_RATE, 0.04f, 0.0f, 1.0f, 0.01f);
	if (m_fMaxBicycleForwardnessSlowCycle < ms_fBICYCLE_MAX_FORWARDNESS_SLOW)
	{
		m_fMaxBicycleForwardnessSlowCycle += fwTimer::GetTimeStep() * BICYCLE_THROTTLE_RECOVERY_RATE;
		m_fMaxBicycleForwardnessSlowCycle = rage::Clamp(m_fMaxBicycleForwardnessSlowCycle, 0.0f, ms_fBICYCLE_MAX_FORWARDNESS_SLOW);
	}

	CTaskMotionOnBicycle* pBicycleTask = static_cast<CTaskMotionOnBicycle*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE));
	static dev_float MIN_SPEED_FOR_MOVE_START = 0.2f;
	if (fSpeedRatio < MIN_SPEED_FOR_MOVE_START && pBicycleTask && pBicycleTask->GetState() == CTaskMotionOnBicycle::State_Still)
	{
		m_uStartTime = 0;
		m_bStartFinished = false;
	}
	else
	{
		m_uStartTime += fwTimer::GetTimeStepInMilliseconds();
	}

	// Forwardness between -1 and 1
	float fSpeed = DotProduct(vRelativeVelocity, VEC3V_TO_VECTOR3(pBike->GetTransform().GetB()));
	float fAccelButton = 0.0f;

	float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_BICYCLE, true);
	//Displayf("Sprint Result: %.2f", fSprintResult);

	bool bIsTrackStanding = false;
	bool bIsTransitioning = false;
	const CBicycleInfo* pBicycleInfo = pBike->GetLayoutInfo()->GetBicycleInfo();
	const bool bIsFixie = pBicycleInfo && pBicycleInfo->GetIsFixieBike();
	TUNE_GROUP_BOOL(BICYCLE_TUNE, ALLOW_FORCED_BRAKE, true);
	if (ALLOW_FORCED_BRAKE && pBicycleTask && pBicycleInfo)
	{
		if (pBicycleTask->GetState() == CTaskMotionOnBicycle::State_StillToSit && pBicycleTask->GetTimeInState() < pBicycleInfo->GetMinForcedInitialBrakeTime())
		{
			if (vRelativeVelocity.Mag2() <= rage::square(CTaskMotionOnBicycle::ms_Tunables.m_MaxSpeedForStill))
			{
				bForceBrake = true;
			}
		}
		else if (pBicycleTask->GetSubTask() && pBicycleInfo->GetCanTrackStand())
		{
			const s32 iBicycleControllerState =  pBicycleTask->GetSubTask()->GetState();
			bIsTrackStanding = iBicycleControllerState == CTaskMotionOnBicycleController::State_TrackStand;
			const bool bIsFixieSkidding = iBicycleControllerState == CTaskMotionOnBicycleController::State_FixieSkid;
			bIsTransitioning = iBicycleControllerState == CTaskMotionOnBicycleController::State_ToTrackStandTransition;
			if (bIsFixie)
			{
				if ((bIsTrackStanding || bIsFixieSkidding || bIsTransitioning))
				{
					bForceBrake = true;
				}
			}
			else if (bIsTrackStanding && fSprintResult == 0.0f)
			{
				bForceBrake = true;
			}
		}
	}

	// Script-specified disabling of sprinting gives a max movespeed of 1.0f (running)
	if(pPlayerPed->GetPlayerInfo()->GetPlayerDataPlayerSprintDisabled())
		fSprintResult = Min(fSprintResult, 1.0f);

	bool bStartingToRide = false;

	const bool bIgnoreInAirCheck = false;
	const bool bPreparingToBunnyHop = CTaskMotionOnBicycle::WantsToJump(*pPlayerPed, *pBike, bIgnoreInAirCheck);
	bool bCyclingPreventedDueToDriveby = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);
	if (bCyclingPreventedDueToDriveby)
	{
		const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPlayerPed);
		if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive())
		{
			bCyclingPreventedDueToDriveby = false;
		}
	}
	float fFrontBrakeButton = (!bPreparingToBunnyHop && bCyclingPreventedDueToDriveby) ? 1.0f : pControl->GetVehiclePushbikeFrontBrake().GetNorm01();
	float fRearBrakeButton = (!bPreparingToBunnyHop && bCyclingPreventedDueToDriveby) ? 1.0f : pControl->GetVehiclePushbikeRearBrake().GetNorm01();// TODO: Might want to make this a new button rear brake button

	// We make the fixie only apply the front brake, so when we do the fixie skid we do the rear too
	bool bAbleToReverse = true;
	if (bIsFixie)
	{
		if (pControl->GetVehiclePushbikeFrontBrake().IsDown() && pControl->GetVehicleBrake().IsDown())
		{
			fFrontBrakeButton = 1.0f;
			fRearBrakeButton = 1.0f;
			bAbleToReverse = false;
		}
		else
		{
			if (fRearBrakeButton > fFrontBrakeButton)
			{
				bAbleToReverse = false;
				fFrontBrakeButton = fRearBrakeButton;
			}
			fRearBrakeButton = 0.0f;
		}
	}
	else if (bIsTrackStanding || bIsTransitioning)
	{
		bAbleToReverse = false;
	}

	float fBrakeButton = fFrontBrakeButton;

	if (fSprintResult >= 1.0f && fBrakeButton == 0.0f)
	{
		if (bStartingToRide)
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_SPRINT_RESULT, 1.0f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_SPRINT_RESULT, 10.0f, 0.0f, 10.0f, 0.01f);
			fAccelButton = rage::Clamp((fSprintResult - MIN_SPRINT_RESULT) / (MAX_SPRINT_RESULT - MIN_SPRINT_RESULT), 0.0f, 1.0f);
		}
		else
		{
			fAccelButton = 1.0f;
		}
	}

	//Only go into reverse if both brakes are pressed.
	TUNE_BOOL(ENABLE_DUAL_BRAKE_REVERSE, FALSE);

	float fReverse = pControl->GetVehicleBrake().GetNorm01();
	float fBackwardness = fRearBrakeButton;
	if(fFrontBrakeButton > fRearBrakeButton)
	{
		fBackwardness = fFrontBrakeButton;
	}

	if (fSprintResult >= 1.0f && fRearBrakeButton == 1.0f)
	{
		fSprintResult = 0.0f;
		fAccelButton = 0.0f;
	}

	if(fReverse == 0.0f || bPreparingToBunnyHop)// if reverse is not pressed or we are preparing to bunny hop don't reverse.
	{
		bAbleToReverse = false;
	}

	float fForwardness = fAccelButton - fBackwardness;

	if (bCyclingPreventedDueToDriveby || (bPreparingToBunnyHop && fForwardness > 0.0f))
	{
		fForwardness = 0.0f;
		fSprintResult = 0.0f;
		fAccelButton = 0.0f;
	}

	if (fForwardness > 0.0f && pBicycleTask && pBicycleTask->GetState() == CTaskMotionOnBicycle::State_Still)
	{
		bStartingToRide = true;
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, PUSH_START_STARTING_THROTTLE, 0.30f, 0.0f, 1.0f, 0.01f);
		m_fStartingThrottle = PUSH_START_STARTING_THROTTLE;
	}

	if (!m_bStartFinished)
	{
		static dev_u32 VEHICLE_START_TIME = 2000;
		if (m_uStartTime >= VEHICLE_START_TIME || fForwardness < 0.0f)
		{
			m_bStartFinished = true;
		}
	}

	if (NetworkInterface::IsGameInProgress() && fSprintResult > 0.0f && fForwardness == 0.0f)
	{
		fForwardness = fSprintResult;
	}

	if (fSprintResult > 1.0f)
	{
		m_bClearedForwardness = false;
		m_uLastSprintTime = fwTimer::GetTimeInMilliseconds();

		if (!bStartingToRide)
		{
			const float fSprintNormalisedResult = fSprintResult / pPlayerPed->GetPlayerInfo()->GetButtonSprintResults(CPlayerInfo::SPRINT_ON_BICYCLE);
			//Displayf("fSprintNormalisedResult: %.2f", fSprintNormalisedResult);
			fForwardness = Sign(fForwardness) * rage::Clamp(fSprintNormalisedResult, 0.0f, 1.0f);
		}
	}
	else if (fForwardness > 0.0f)
	{
		const u32 uLastSprintDiff = fwTimer::GetTimeInMilliseconds() - m_uLastSprintTime;
		static dev_u32 uIgnoreSlownessResetTime = 500;

		bool bClampForwardness = false;

		if (uLastSprintDiff >= uIgnoreSlownessResetTime)
		{
			if (fSprintResult > 0.0f)
			{
				if (m_bStartFinished)
				{
					bClampForwardness = true;
				}
					
				if (!bStartingToRide)
				{
					const float fSprintNormalisedResult = fSprintResult / pPlayerPed->GetPlayerInfo()->GetButtonSprintResults(CPlayerInfo::SPRINT_ON_BICYCLE);
					//Displayf("fSprintNormalisedResult: %.2f", fSprintNormalisedResult);
					fForwardness = Sign(fForwardness) * rage::Clamp(fSprintNormalisedResult, 0.0f, 1.0f);
				}
			}
			else if (m_bStartFinished)
			{
				bClampForwardness = true;
			}
		}
		else
		{
			if (!bStartingToRide)
			{
				if (pBicycleTask && pBicycleTask->GetState() == CTaskMotionOnBicycle::State_StillToSit)
				{
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, STILL_TO_SIT_FORWARD, 0.2f, 0.0f, 1.0f, 0.01f);
					fForwardness = STILL_TO_SIT_FORWARD;
				}
				else
				{
					fForwardness = 1.0f;
				}
			}
		}

		if (bClampForwardness)
		{
			float fSpeed = pBike->GetVelocityIncludingReferenceFrame().Mag();
			fSpeed /=fMaxSpeed;

			//fSpeed *= SPEED_MULT;
			//static dev_float MAX_SPEED = 0.6f;
			//if (fSpeed > MAX_SPEED || (!m_bClearedForwardness && m_fMaxBicycleForwardnessSlowCycle >= ms_fBICYCLE_MAX_FORWARDNESS_SLOW))

			if ((!m_bClearedForwardness && m_fMaxBicycleForwardnessSlowCycle >= ms_fBICYCLE_MAX_FORWARDNESS_SLOW))
			{
				m_fMaxBicycleForwardnessSlowCycle = ms_fBICYCLE_MAX_FORWARDNESS_SLOW;
				m_bClearedForwardness = true;
			}

			fForwardness = Sign(fForwardness) * rage::Clamp(Abs(fForwardness), 0.0f, ms_fBICYCLE_MAX_FORWARDNESS_SLOW);

			if (fForwardness >= 0.0f)
			{
				fForwardness = Min(m_fMaxBicycleForwardnessSlowCycle, fForwardness);
			}
		}
	}

	//Displayf("m_fMaxBicycleForwardnessSlowCycle: %.2f, fForwardness: %.2f", m_fMaxBicycleForwardnessSlowCycle, fForwardness);

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, PUSH_START_THROTTLE_APPROACH_RATE, 0.6f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_THROTTLE_PUSH_START, 0.85f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_FORWARDNESS, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_FORWARDNESS_SETTING_OFF, 0.2f, 0.0f, 1.0f, 0.01f);
	bool bSettingOff = (pBicycleTask && ((pBicycleTask->GetState() == CTaskMotionOnBicycle::State_Still && fForwardness > 0.0f) || pBicycleTask->GetState() == CTaskMotionOnBicycle::State_StillToSit)) ? true : false;
	if (m_bStartFinished)
	{
		bSettingOff = false;
	}

	if(bForceBrake)
	{
		pBike->SetBrake(1.0f);
		pBike->SetThrottle(0.0f);
		pBike->SetRearBrake(1.0f);
	}
	else if(!pBike->m_nVehicleFlags.bEngineOn && pBike->m_nVehicleFlags.bIsDrowning)
	{
		pBike->SetBrake(0.0f);
		pBike->SetThrottle(0.0f);
		pBike->SetRearBrake(0.0f);
	}
	else if(pBike->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet)
		/*pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PUT_ON_HELMET)*/)
	{
		pBike->SetBrake(0.0f);
		pBike->SetThrottle(0.0f);
		pBike->SetRearBrake(0.0f);
	}
	// We've stopped. Go where we want to go.
	else if (rage::Abs(fSpeed) < 0.5f)
	{
		// if both brake and throttle are held down, do a burn out
		if(fAccelButton > 0.6f && fBrakeButton > 0.6f && pBike->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			pBike->SetThrottle(fAccelButton);
			pBike->SetBrake(fBrakeButton);
			pBike->SetRearBrake(0.0f);
		}
		else
		{
			if (bSettingOff || fAccelButton > 0.0f)
			{
				if (bSettingOff)
				{
					rage::Approach(m_fStartingThrottle, MAX_THROTTLE_PUSH_START, PUSH_START_THROTTLE_APPROACH_RATE, fwTimer::GetTimeStep());
					fForwardness = m_fStartingThrottle;
				}
				else
				{
					fForwardness = rage::Clamp(fForwardness, MIN_FORWARDNESS, 1.0f);
				}
			}

			if(!bAbleToReverse && fForwardness <= 0.0f)
			{
				fForwardness = 0.0f;// prevent the player reversing if they are not supposed to.
			}

			pBike->SetThrottle(fForwardness);

			if(fForwardness != 0.0f)
			{
				pBike->SetBrake(0.0f);
				pBike->SetRearBrake(0.0f);
			}
			else//make sure we don't roll backwards.
			{
				pBike->SetBrake(1.0f);
				pBike->SetRearBrake(0.0f);
			}
		}
	}
	else
	{	
		// if both brake and throttle are held down, do a burn out
		if(fAccelButton > 0.6f && fBrakeButton > 0.6f && pBike->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			pBike->SetThrottle(fAccelButton);
			pBike->SetBrake(fBrakeButton);
			pBike->SetRearBrake(0.0f);
		}
		// going forwards
		else if (fSpeed >= 0.0f)
		{	
			// we want to go forwards so go faster
			if (fForwardness >= 0.0f)
			{
				if (bSettingOff || fAccelButton > 0.0f || (!m_bStartFinished && fForwardness > 0.0f))
				{
					if (bSettingOff || !m_bStartFinished)
					{
						rage::Approach(m_fStartingThrottle, MAX_THROTTLE_PUSH_START, PUSH_START_THROTTLE_APPROACH_RATE, fwTimer::GetTimeStep());
						fForwardness = m_fStartingThrottle;
					}
					else
					{
						fForwardness = rage::Clamp(fForwardness, MIN_FORWARDNESS, 1.0f);
					}
				}
				pBike->SetThrottle(fForwardness);
				pBike->SetBrake(0.0f);
				pBike->SetRearBrake(0.0f);
			}
			// we don't want to go forwards - so brake
			else
			{
				pBike->SetThrottle(0.0f);
				if(fFrontBrakeButton > 0.0f)
				{
					pBike->SetBrake(-fForwardness);
					pBike->SetRearBrake(fRearBrakeButton);
				}
				else
				{
					pBike->SetBrake(0.0f);
					pBike->SetRearBrake(fRearBrakeButton);
				}

			}
		}
		// going backwards
		else
		{
			// want to go forwards
			static dev_float sfMaxReverseSpeed = -3.0f;

			if(fSpeed < sfMaxReverseSpeed)
			{
				pBike->SetThrottle(0.0f);
				pBike->SetBrake(1.0f);
				pBike->SetRearBrake(1.0f);
			}
			else if (fForwardness >= 0.0f)
			{
				// let us sit on the gas pedal if we're trying to get up a slope but sliding back
				if(pBike->GetThrottle() > 0.5f && fSpeed > -10.0f)
				{
					pBike->SetThrottle(fForwardness);
					pBike->SetBrake(0.0f);
					pBike->SetRearBrake(0.0f);
				}
				// oh dear we're sliding back too fast, go for the brakes
				else
				{
					pBike->SetThrottle(0.0f);

					if(fFrontBrakeButton > 0.0f)
					{
						pBike->SetBrake(fForwardness);
						pBike->SetRearBrake(fRearBrakeButton);
					}
					else
					{
						pBike->SetBrake(1.0f);
						pBike->SetRearBrake(fRearBrakeButton);
					}
				}
			}
			// we want to go backwards, so apply the gas
			else if(bAbleToReverse)
			{
				pBike->SetThrottle(fForwardness);
				pBike->SetBrake(0.0f);
				pBike->SetRearBrake(0.0f);
			}
			else
			{
				pBike->SetThrottle(0.0f);
				if(fFrontBrakeButton > 0.0f)
				{
					pBike->SetBrake(-fForwardness);
					pBike->SetRearBrake(fRearBrakeButton);
				}
				else
				{
					pBike->SetBrake(0.0f);
					pBike->SetRearBrake(fRearBrakeButton);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveBoat
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveBoat::CTaskVehiclePlayerDriveBoat() :
CTaskVehiclePlayerDrive()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BOAT);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveBoat::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	//Assert(dynamic_cast<CBoat*>(pVehicle));
	//CBoat *pBoat = static_cast<CBoat*>(pVehicle);
	CBoatHandling* pHandling = pVehicle->GetBoatHandling();

	Assert(pPlayerPed && (pPlayerPed->IsControlledByLocalPlayer()));

	CAnchorHelper* anchorHelper = NULL;
	if( pVehicle->InheritsFromBoat() )
	{
		anchorHelper = &static_cast< CBoat* >( pVehicle )->GetAnchorHelper();
	}
	else if( pVehicle->InheritsFromAmphibiousAutomobile() )
	{
		anchorHelper = &static_cast< CAmphibiousAutomobile* >( pVehicle )->GetAnchorHelper();
	}


	// make sure anchor is cleared in this state
	if( anchorHelper->IsAnchored() && !anchorHelper->ShouldForcePlayerBoatToRemainAnchored())
		anchorHelper->Anchor(false);

	if( pPlayerPed->GetIsArrested() || pPlayerPed->IsInjured() )
	{
		pVehicle->SetThrottle(0.0f);
		pVehicle->SetBrake(0.0f);
		return;
	}

	float fForwardness = pControl->GetVehicleAccelerate().GetNorm01() - pControl->GetVehicleBrake().GetNorm01();
	if( !pVehicle->m_nVehicleFlags.bEngineOn && pVehicle->m_nVehicleFlags.bIsDrowning )
	{
		fForwardness = 0.0f;
	}

	pVehicle->SetThrottle( fForwardness );
	pVehicle->SetBrake( 0.0f );

	float fSteerMult = 1.0f;
	// handBrake tries to steer twice as tightly
	if( pVehicle->GetIsHandBrakePressed( pControl ) )
	{
		fSteerMult = 1.3f;
	}

	// progressive steering
	pControl->SetVehicleSteeringExclusive();

	float fDesiredInput			= -pControl->GetVehicleSteeringLeftRight().GetNorm() * fSteerMult;
	float fDesiredPitchInput	= pControl->GetVehicleSteeringUpDown().GetNorm();
	float fSteerSmoothRate		= ms_fBOAT_STEER_SMOOTH_RATE;
	float fPitchSmoothRate		= ms_fBOAT_PITCH_FWD_SMOOTH_RATE;
#if RSG_PC
		const float c_fMouseAdjustmentScaleForBoats = c_fMouseAdjustmentScale * 3.0f;	// boats steering is unresponsive, so turn it up.
		if( pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS )
		{
			fDesiredInput *= c_fMouseAdjustmentScaleForBoats;
		}
		if( pControl->GetVehicleSteeringUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS )
		{
			fDesiredPitchInput *= c_fMouseAdjustmentScaleForBoats;
		}
#endif
#if USE_SIXAXIS_GESTURES
	if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_BOAT))
	{
		CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
		if(gesture)
		{
			if(fDesiredInput == 0.0f && pControl->GetVehicleSteeringLeftRight().IsEnabled())
			{
				fDesiredInput = -gesture->GetPadRollInRange(CBoat::MOTION_CONTROL_ROLL_MIN,CBoat::MOTION_CONTROL_ROLL_MAX,true) * fSteerMult;
				fSteerSmoothRate = CVehicle::ms_fDefaultMotionContolSteerSmoothRate;

			}
			if(fDesiredPitchInput == 0.0f && pControl->GetVehicleSteeringUpDown().IsEnabled())
			{
				fDesiredPitchInput = gesture->GetPadPitchInRange(CBoat::MOTION_CONTROL_PITCH_MIN,CBoat::MOTION_CONTROL_PITCH_MAX,true,true);
				fPitchSmoothRate = CVehicle::ms_fDefaultMotionContolSteerSmoothRate * fPitchSmoothRate/fSteerSmoothRate;
			}
		}
	}
#endif
	pHandling->SetPitchFwdInput( pHandling->GetPitchFwdInput() + 
								( fDesiredPitchInput - pHandling->GetPitchFwdInput()) * rage::Min( 1.0f, fPitchSmoothRate * fwTimer::GetTimeStep() ) );

	pVehicle->m_fSteerInput += ( fDesiredInput - pVehicle->m_fSteerInput) * rage::Min( 1.0f, fSteerSmoothRate *fwTimer::GetTimeStep() );
	pVehicle->m_fSteerInput = Clamp( pVehicle->m_fSteerInput, -fSteerMult, fSteerMult );

	// use non-linear steering - squared
	float fVal = 0.0f;
	if( pVehicle->m_fSteerInput >= 0 )
	{
		fVal = pVehicle->m_fSteerInput * pVehicle->m_fSteerInput;
	}
	else
	{
		fVal = -pVehicle->m_fSteerInput * pVehicle->m_fSteerInput;
	}

	pVehicle->SetSteerAngle( fVal * pVehicle->pHandling->m_fSteeringLock );

	//Check if heli has light and if it needs turning on/off
	if( pControl->GetVehicleHeadlight().IsPressed() && pPlayerPed->GetPlayerInfo()->GetCanUseSearchLight() )
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if(pWeaponMgr)
		{
			for( int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++ )
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon( i );

				if( pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT )
				{
					CSearchLight* pSearchlight = static_cast<CSearchLight*>( pWeapon );
					pSearchlight->Fire( pPlayerPed, pVehicle, VEC3_ZERO );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveBoat::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveBoat::ProcessDriverInputsForPlayerOnExit( CVehicle* pVehicle )
{
	Assert(pVehicle);

	// GTAV - B*1784768 - After the player driving the boat died in the mission 'Dry Docking', the boat continued to drive with no way of stopping it.
	// reset the input when the player stops driving the vehicle.
	CBoat *pBoat = static_cast<CBoat*>(pVehicle);

	pBoat->SetThrottle( 0.0f );
	pBoat->SetBrake( 0.25f );

}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveTrain
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveTrain::CTaskVehiclePlayerDriveTrain() :
CTaskVehiclePlayerDrive()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_TRAIN);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveTrain::ProcessDriverInputsForPlayerOnUpdate(CPed* /*pPlayerPed*/, CControl* /*pControl*/, CVehicle* /*pVehicle*/)
{
	
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveTrain::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}


//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveSubmarine
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveSubmarine::CTaskVehiclePlayerDriveSubmarine() :
CTaskVehiclePlayerDrive()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINE);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarine::ProcessDriverInputsForPlayerOnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{

}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarine::ProcessDriverInputsForPlayerOnExit				(CVehicle* pVehicle)
{
	Assert(pVehicle);

	CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

	// GTAV B*1815137 - crash when choosing to load a quick save while in a submarine
	// The crash is caused by this being called because CVehicle destructor calls CPedIntelligence::FlushImmediately 
	// which aborts all the tasks and calls this.
	// This is not the best way to fix this.
	if( pSubHandling )
	{
		//Reset inputs
		pSubHandling->SetPitchControl(0.0f);
		pSubHandling->SetYawControl(0.0f);
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarine::ProcessDriverInputsForPlayerOnUpdate(CPed *UNUSED_PARAM(pPlayerPed), CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

	physicsAssertf(pControl,"Unexpected NULL control");

	// Make sure any anchor is cleared in this state unless specifically requested.
	if(pSubHandling->GetAnchorHelper().IsAnchored() && !pSubHandling->GetAnchorHelper().ShouldForcePlayerBoatToRemainAnchored())
	{
		pSubHandling->GetAnchorHelper().Anchor(false);
	}

	Vector2 vPlayerInput;

	pControl->SetVehicleSubSteeringExclusive();
	vPlayerInput.y = pControl->GetVehicleSubPitchUpDown().GetNorm();
	vPlayerInput.x = -pControl->GetVehicleSubTurnLeftRight().GetNorm();
#if RSG_PC
		if(pControl->GetVehicleSubTurnLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
		{
			vPlayerInput.x *= c_fMouseAdjustmentScale;
		}
		if(pControl->GetVehicleSubPitchUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
		{
			vPlayerInput.y *= c_fMouseAdjustmentScale;
		}
#endif

	CFlightModelHelper::MakeCircularInputSquare(vPlayerInput);

	vPlayerInput.x += (pControl->GetVehicleSubTurnHardLeft().GetNorm01() - pControl->GetVehicleSubTurnHardRight().GetNorm01());

	pSubHandling->SetPitchControl(vPlayerInput.y);
	pSubHandling->SetYawControl(vPlayerInput.x);

	float fThrottle = pControl->GetVehicleSubThrottleUp().GetNorm01() - pControl->GetVehicleSubThrottleDown().GetNorm01();

	fThrottle = rage::Clamp(fThrottle,-1.0f,1.0f);

	pVehicle->SetThrottle(fThrottle);

	// Don't want to control ascending or descending anymore
	pSubHandling->SetDiveControl(pControl->GetVehicleSubAscend().GetNorm01() - pControl->GetVehicleSubDescend().GetNorm01());
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarine::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}



//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveSubmarineCar
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveSubmarineCar::CTaskVehiclePlayerDriveSubmarineCar() :
CTaskVehiclePlayerDriveAutomobile()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINECAR);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarineCar::ProcessDriverInputsForPlayerOnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{

}

//////////////////////////////////////////////////////////////////////
aiTask::FSM_Return CTaskVehiclePlayerDriveSubmarineCar::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		//
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pVehicle);
			FSM_OnUpdate
				return Drive_OnUpdate(pVehicle);
		//
		FSM_State(State_NoDriver)
			FSM_OnEnter
				NoDriver_OnEnter(pVehicle);
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
		//
		FSM_State(State_SubmarineMode)
			FSM_OnEnter
				SubmarineMode_OnEnter();
			FSM_OnUpdate
				return SubmarineMode_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarineCar::SubmarineMode_OnEnter	()
{
	SetNewTask( rage_new CTaskVehiclePlayerDriveSubmarine() );
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveSubmarineCar::SubmarineMode_OnUpdate( CVehicle* pVehicle )
{
	CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );

	if( !submarineCar ||
		!submarineCar->IsInSubmarineMode() )
	{
		SetState( State_Drive );
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarineCar::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
// Car stuff
	CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );

	if( !submarineCar->IsInSubmarineMode() )
	{
		CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate( pPlayerPed, pControl, pVehicle );
	}
	else
	{
		SetState( State_SubmarineMode );
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveSubmarineCar::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}


//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveAmphibiousAutomobile
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveAmphibiousAutomobile::CTaskVehiclePlayerDriveAmphibiousAutomobile() :
	CTaskVehiclePlayerDriveAutomobile()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AMPHIBIOUS_AUTOMOBILE);
}


CTaskVehiclePlayerDriveSubmarineCar::~CTaskVehiclePlayerDriveSubmarineCar()
{

}


//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAmphibiousAutomobile::ProcessDriverInputsForPlayerOnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{

}

//////////////////////////////////////////////////////////////////////
aiTask::FSM_Return CTaskVehiclePlayerDriveAmphibiousAutomobile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		//
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pVehicle);
			FSM_OnUpdate
				return Drive_OnUpdate(pVehicle);
	//
		FSM_State(State_NoDriver)
			FSM_OnEnter
				NoDriver_OnEnter(pVehicle);
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
	//
		FSM_State(State_BoatMode)
			FSM_OnEnter
				BoatMode_OnEnter();
			FSM_OnUpdate
				return BoatMode_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAmphibiousAutomobile::BoatMode_OnEnter()
{
	SetNewTask( rage_new CTaskVehiclePlayerDriveBoat() );
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveAmphibiousAutomobile::BoatMode_OnUpdate( CVehicle* pVehicle )
{
	CAmphibiousAutomobile* amphibiousVehicle = static_cast< CAmphibiousAutomobile* >( pVehicle );

	if( !amphibiousVehicle )
	{
		SetState( State_Drive );
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAmphibiousAutomobile::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	// Car stuff
	CAmphibiousAutomobile* amphibiousVehicle = static_cast< CAmphibiousAutomobile* >( pVehicle );
	CAnchorHelper& anchorHelper = amphibiousVehicle->GetAnchorHelper();

	// make sure anchor is cleared in this state
	if( anchorHelper.IsAnchored() && !anchorHelper.ShouldForcePlayerBoatToRemainAnchored())
	{
		anchorHelper.Anchor(false);
	}

	CTaskVehiclePlayerDriveAutomobile::ProcessDriverInputsForPlayerOnUpdate( pPlayerPed, pControl, pVehicle );
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAmphibiousAutomobile::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}


//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDrivePlane
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDrivePlane::CTaskVehiclePlayerDrivePlane() :
CTaskVehiclePlayerDrive()
#if RSG_PC
,m_MouseSteeringInput(false)
#endif
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_PLANE);
	m_TimeOfLastLandingGearToggle	= 0;
	m_ThrottleControlHasBeenActive	= false;
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDrivePlane::Drive_OnEnter(CVehicle* pVehicle)
{
    CTaskVehiclePlayerDrive::Drive_OnEnter(pVehicle);
	m_ThrottleControlHasBeenActive = false;
}

//////////////////////////////////////////////////////////////////////
u32 ms_uTimeToHoldDoorControlButtonDown = 250;
#if USE_SIXAXIS_GESTURES
static dev_float sfZeroMotionThreshold = 0.1f;
#endif	// USE_SIXAXIS_GESTURES
void CTaskVehiclePlayerDrivePlane::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CPlane*>(pVehicle));
	CPlane* pPlane = static_cast<CPlane*>(pVehicle);

	// If this is a seaplane it could be anchored. Make sure anchor is cleared in this state.
	if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
	{
		if(pSeaPlaneExtension->GetAnchorHelper().IsAnchored() && !pSeaPlaneExtension->GetAnchorHelper().ShouldForcePlayerBoatToRemainAnchored())
		{
			pSeaPlaneExtension->GetAnchorHelper().Anchor(false);
		}
	}

	float fSpeed = Abs(DotProduct(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()), pVehicle->GetVelocityIncludingReferenceFrame()));

	Assert(pPlayerPed && (pPlayerPed->IsControlledByLocalPlayer()));

	float fDesiredThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();

	// GTAV - B*1754978 - If we're in flight we don't want the afterburner effect to cut out
	// for a frame so if the throttle control has never been active set it to the previous stored value
	if( !m_ThrottleControlHasBeenActive )
	{
		m_ThrottleControlHasBeenActive = pControl->GetVehicleFlyThrottleUp().IsEnabled();

		if( !m_ThrottleControlHasBeenActive )
		{
			fDesiredThrottleControl = pVehicle->GetThrottle();
		}
	}

	pPlane->SetThrottleControl(fDesiredThrottleControl);

	TUNE_FLOAT(TEST_PLANE_PERC, 0.5f, 0.0f, 1.0f, 0.01f);
	
	float fMotionPitchControl = 0.0f, fMotionRollControl = 0.0f, fMotionYawControl = 0.0f;
#if USE_SIXAXIS_GESTURES
	if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_AIRCRAFT))
	{
		// Static values here can be tweaked in widgets

		CPadGesture* pGesture = CControlMgr::GetPlayerPad()->GetPadGesture();
		if(pGesture)
		{
			// Rescale the raw pitch input to custom range
			// Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyPitchUpDown().IsEnabled())
			{
				fMotionPitchControl = pGesture->GetPadPitchInRange(CPlane::MOTION_CONTROL_PITCH_MIN,CPlane::MOTION_CONTROL_PITCH_MAX,false,true);
				if(fMotionPitchControl > -sfZeroMotionThreshold && fMotionPitchControl < sfZeroMotionThreshold)
				{
					fMotionPitchControl = 0.0f;
				}
			}
			 
			// Rescale the raw roll too
			// Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyRollLeftRight().IsEnabled())
			{
				fMotionRollControl = pGesture->GetPadRollInRange(CPlane::MOTION_CONTROL_ROLL_MIN,CPlane::MOTION_CONTROL_ROLL_MAX,true);
				if(fMotionRollControl > -sfZeroMotionThreshold && fMotionRollControl < sfZeroMotionThreshold)
				{
					fMotionRollControl = 0.0f;
				}
			}

			// Not currently used unless Extrapolate yaw is turned on in RAG
			if(pControl->GetVehicleFlyYawRight().IsEnabled() || pControl->GetVehicleFlyYawLeft().IsEnabled())
			{
				fMotionYawControl = pGesture->GetPadYaw() * CPlane::MOTION_CONTROL_YAW_MULT;;
			}
		}
	}
	//else
#endif	// USE_SIXAXIS_GESTURES
	pControl->SetVehicleFlySteeringExclusive();

	ioValue::ReadOptions readOptions = ioValue::ALWAYS_DEAD_ZONE;

#if RSG_PC
	const bool bScaledAxisRollInput = pControl->GetVehicleFlyRollLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
	const bool bScaledAxisPitchnput = pControl->GetVehicleFlyPitchUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
	const bool bScaledAxisYawInput = pControl->GetVehicleFlyYawLeft().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS
									|| pControl->GetVehicleFlyYawRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
	if(bScaledAxisRollInput || bScaledAxisPitchnput)
	{
		readOptions = ioValue::DEFAULT_OPTIONS;
	}
#endif

	float fPitchControl = fMotionPitchControl + (pControl->GetVehicleFlyPitchUpDown().GetNorm(readOptions));
	fPitchControl += pPlane->m_fPitchInputBias;

	float fRollControl = (fMotionRollControl + pControl->GetVehicleFlyRollLeftRight().GetNorm(readOptions)*(1.0f - TEST_PLANE_PERC*rage::Abs(pPlane->GetPitchControl() ) ) );
	fRollControl += pPlane->m_fRollInputBias;

	// Allow the script to manipulate yaw control
	// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work
	// (ideally we would alter this but its safer to fix the issue this way).
	float fYawControl = fMotionYawControl + Abs(pControl->GetVehicleFlyYawRight().GetNorm()) - Abs(pControl->GetVehicleFlyYawLeft().GetNorm());
	fYawControl += pPlane->m_fSteerInputBias;

#if RSG_PC
	float timeStep = fwTimer::GetTimeStep();

	if(pControl->WasKeyboardMouseLastKnownSource())
	{
		if(bScaledAxisRollInput || bScaledAxisPitchnput || bScaledAxisYawInput)
		{
			if(bScaledAxisRollInput)
			{
				fRollControl *= c_fMouseAdjustmentScale;
			}
			if(bScaledAxisPitchnput)
			{
				fPitchControl *= c_fMouseAdjustmentScale;
			}
			if(bScaledAxisYawInput)
			{
				fYawControl *= c_fMouseAdjustmentScale;
			}
			m_MouseSteeringInput = true;
		}
		else if(pControl->GetVehicleFlyRollLeftRight().GetSource().m_Device == IOMS_MKB_AXIS || pControl->GetVehicleFlyPitchUpDown().GetSource().m_Device == IOMS_MKB_AXIS)// Keyboard steering input
		{
			m_MouseSteeringInput = false; 
		}
	}
	else
	{
		// HACK TO FIX GTAV B*2870991 - The input returns the control immediately back to joypad control when playing on mouse, even when there isn't one plugged in
		if( !m_MouseSteeringInput ||
			Abs( fRollControl ) > c_fMouseTollerance ||
			Abs( fPitchControl ) > c_fMouseTollerance ||
			Abs( fYawControl ) > c_fMouseTollerance )
		{
			m_MouseSteeringInput = false;
		}
	}

	bool bMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle;
	bool bCameraMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera;

	if(m_MouseSteeringInput && ((bMouseFlySteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseFlySteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())))
	{
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_DEADZONE_CENTERING_SPEED_ROLL, 4.0f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_DEADZONE_CENTERING_SPEED_PITCH, 0.6f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_DEADZONE_CENTERING_SPEED_YAW, 0.6f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_MULTIPLIER, 1.0f, 0.0f, 30.0f, 0.01f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_CENTER_ALLOWANCE, 0.001f, 0.0f, 1.0f, 0.0001f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_INPUT_DEADZONE, FLT_EPSILON, 0.0f, 1.0f, 0.0001f);
		TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, PLANE_STEERING_DELTA_MULT, 15.0f, 0.0f, 30.0f, 0.1f);

		float fAutoCenterMult = CPauseMenu::GetMenuPreference(PREF_MOUSE_AUTOCENTER_PLANE)/10.0f;

		//Roll
		if( bScaledAxisRollInput )
		{
			if (fabs(fRollControl) <= PLANE_STEERING_INPUT_DEADZONE)
			{
				if(pPlane->GetRollControl() > 0.0f)
				{
					fRollControl = Clamp(pPlane->GetRollControl() - PLANE_STEERING_DEADZONE_CENTERING_SPEED_ROLL * fAutoCenterMult * timeStep, 0.0f, 1.0f);
				}
				else
				{
					fRollControl = Clamp(pPlane->GetRollControl() + PLANE_STEERING_DEADZONE_CENTERING_SPEED_ROLL * fAutoCenterMult * timeStep,-1.0f, 0.0f);
				}
			}
			else if( Sign(fRollControl) == Sign(pPlane->GetRollControl()) || rage::Abs(pPlane->GetRollControl()) <= PLANE_STEERING_CENTER_ALLOWANCE)
			{
				fRollControl = pPlane->GetRollControl() + (fRollControl * PLANE_STEERING_MULTIPLIER);
			}
			else
			{
				fRollControl = pPlane->GetRollControl() + (fRollControl - pPlane->GetRollControl()) * PLANE_STEERING_DELTA_MULT * timeStep;
			}
		}

		//Pitch
		if( bScaledAxisPitchnput )
		{
			if (fabs(fPitchControl) <= PLANE_STEERING_INPUT_DEADZONE)
			{
				if(pPlane->GetPitchControl() > 0.0f)
				{
					fPitchControl = Clamp(pPlane->GetPitchControl() - PLANE_STEERING_DEADZONE_CENTERING_SPEED_PITCH * fAutoCenterMult * timeStep, 0.0f, 1.0f);
				}
				else
				{
					fPitchControl = Clamp(pPlane->GetPitchControl() + PLANE_STEERING_DEADZONE_CENTERING_SPEED_PITCH * fAutoCenterMult * timeStep,-1.0f, 0.0f);
				}
			}
			else if( Sign(fPitchControl) == Sign(pPlane->GetPitchControl()) || rage::Abs(pPlane->GetPitchControl()) <= PLANE_STEERING_CENTER_ALLOWANCE)
			{
				fPitchControl = pPlane->GetPitchControl() + (fPitchControl * PLANE_STEERING_MULTIPLIER);
			}
			else
			{
				fPitchControl = pPlane->GetPitchControl() + (fPitchControl - pPlane->GetPitchControl()) * PLANE_STEERING_DELTA_MULT * timeStep;
			}
		}

		//Yaw
		if( bScaledAxisYawInput )
		{
			if (fabs(fYawControl) <= PLANE_STEERING_INPUT_DEADZONE)
			{
				if(pPlane->GetYawControl() > 0.0f)
				{
					fYawControl = Clamp(pPlane->GetYawControl() - PLANE_STEERING_DEADZONE_CENTERING_SPEED_YAW * fAutoCenterMult * timeStep, 0.0f, 1.0f);
				}
				else
				{
					fYawControl = Clamp(pPlane->GetYawControl() + PLANE_STEERING_DEADZONE_CENTERING_SPEED_YAW * fAutoCenterMult * timeStep,-1.0f, 0.0f);
				}
			}
			else if( Sign(fYawControl) == Sign(pPlane->GetYawControl()) || rage::Abs(pPlane->GetYawControl()) <= PLANE_STEERING_CENTER_ALLOWANCE)
			{
				fYawControl = pPlane->GetYawControl() + (fYawControl * PLANE_STEERING_MULTIPLIER);
			}
			else
			{
				fYawControl = pPlane->GetYawControl() + (fYawControl - pPlane->GetYawControl()) * PLANE_STEERING_DELTA_MULT * timeStep;
			}
		}
	}
#endif

	pPlane->SetPitchControl( fPitchControl );
	pPlane->SetRollControl( fRollControl );
	pPlane->SetYawControl( fYawControl );

	dev_float fPlaneWheelSteeringSpeed = 1.0f;
	float fDesiredSteerAngle = -pPlane->GetYawControl() * pPlane->pHandling->m_fSteeringLock * 0.02f*rage::Max(0.1f, 50.0f - fSpeed);
	float fSteerAngleDiff = fDesiredSteerAngle - pPlane->GetSteerAngle();
	fSteerAngleDiff = Clamp(fSteerAngleDiff, -fPlaneWheelSteeringSpeed * fwTimer::GetTimeStep(), fPlaneWheelSteeringSpeed * fwTimer::GetTimeStep());
    pPlane->SetSteerAngle(pPlane->GetSteerAngle() + fSteerAngleDiff);

	pPlane->SetYawControl(Clamp(pPlane->GetYawControl(), -1.0f, 1.0f));
    pPlane->SetPitchControl(Clamp(pPlane->GetPitchControl(), -1.0f, 1.0f));
    pPlane->SetRollControl(Clamp(pPlane->GetRollControl(), -1.0f, 1.0f));


	// Make new undercarriage input (A on 360, X on ps3) toggle landing gear
	// Only allow toggle when in the air
	if(!pPlane->HasContactWheels())
	{
		if(pControl->GetVehicleFlyUndercarriage().IsPressed())
		{
			u32 currentTime = fwTimer::GetTimeInMilliseconds();
			static bank_u32 minTimeForGearToggle = 500;
			switch(pPlane->GetLandingGear().GetPublicState())
			{
			case CLandingGear::STATE_LOCKED_DOWN:
			case CLandingGear::STATE_DEPLOYING:
				if(currentTime > m_TimeOfLastLandingGearToggle + minTimeForGearToggle)
				{
					pPlane->GetLandingGear().ControlLandingGear(pVehicle,CLandingGear::COMMAND_RETRACT);
					m_TimeOfLastLandingGearToggle = currentTime;
				}
				break;
			case CLandingGear::STATE_LOCKED_UP:
			case CLandingGear::STATE_RETRACTING:
				if(currentTime > m_TimeOfLastLandingGearToggle + minTimeForGearToggle)
				{
					pPlane->GetLandingGear().ControlLandingGear(pVehicle,CLandingGear::COMMAND_DEPLOY);
					m_TimeOfLastLandingGearToggle = currentTime;
				}
				break;
			default:
				break;
			}
		}
	}

	bool bInputActivated = pControl->GetVehicleFlyVerticalFlight().IsPressed();
	bool bOnlyToggleWhenComplete = pPlane->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE );
	bool bTula = MI_PLANE_TULA.IsValid() && pPlane->GetModelIndex() == MI_PLANE_TULA;
	bool bAvenger = MI_PLANE_AVENGER.IsValid() && pPlane->GetModelIndex() == MI_PLANE_AVENGER;
	if (bTula || bAvenger)
	{
		static u32 uMinTimeForTapMS = 250;
		u32 timeForTapMS = u32( (float)uMinTimeForTapMS * ( (float)fwTimer::GetTimeStepInMilliseconds() / 33.0f ) );
		timeForTapMS = Max( uMinTimeForTapMS, timeForTapMS );
		bInputActivated = pControl->GetVehicleFlyVerticalFlight().IsReleased() && !pControl->GetVehicleFlyVerticalFlight().IsReleasedAfterHistoryHeldDown(timeForTapMS);
		bOnlyToggleWhenComplete = true;
	}

	if( bAvenger && !pPlane->IsInAir( false ) ) // don't allow the avenger to toggle mode when on the ground
	{
		bInputActivated = false;
	}

	if(bInputActivated)
	{
		//Process whether the player wants to go to vertical take off mode
		if(pPlane->GetVerticalFlightModeAvaliable() &&
			( !bOnlyToggleWhenComplete || 
			  pPlane->GetDesiredVerticalFlightModeRatio() == pPlane->GetVerticalFlightModeRatio() ) )
		{
			if(pPlane->GetDesiredVerticalFlightModeRatio() >= 1.0f)
			{
				pPlane->SetDesiredVerticalFlightModeRatio( 0.0f );
				((audPlaneAudioEntity*)pPlane->GetVehicleAudioEntity())->OnVerticalFlightModeChanged(0.0f);
			}
			else
			{
				pPlane->SetDesiredVerticalFlightModeRatio( 1.0f );
				((audPlaneAudioEntity*)pPlane->GetVehicleAudioEntity())->OnVerticalFlightModeChanged(1.0f);
			}
		}
	}

#if RSG_PC
	if (m_bExitButtonUp && SMultiplayerChat::GetInstance().ShouldDisableInputSinceChatIsTyping())
		m_bExitButtonUp = false;
#endif

	bool bForceBrake = false;
	if( pControl->GetVehicleExit().IsDown() && m_bExitButtonUp )
	{
		pPlane->SetHandBrake(true);
		if( !pPlane->CanPedJumpOutCar(pPlayerPed) )
		{
			bForceBrake = true;
			if (pPlane->GetVelocityIncludingReferenceFrame().Mag2() < (0.2f*0.2f))		// If we're already static we don't want the brake light to light up. (causes ugly glitch)
			{
				pPlane->m_nVehicleFlags.bSuppressBrakeLight = true;
			}
		}
	}
	else
	{
		pPlane->SetHandBrake(false);
	}

//#if 0	// Disable open/close rear door of cargo plane, titan. B* 1369017
#if __BANK

	if( CVehicleFactory::ms_rearDoorsCanOpen )
	{
		// Process the rear ramp door
		m_bDoorControlButtonUp |= pControl->GetVehicleRoof().IsUp();

		if(m_bDoorControlButtonUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldDoorControlButtonDown))
		{
			CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_BOOT);
			if(pDoor == NULL)
			{
				pDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_R); // Some planes have their rare door tagged with VEH_DOOR_DSIDE_R
			}
			if(pDoor && pDoor->GetIsIntact(pPlane))
			{
				if(pDoor->GetIsLatched(pPlane) || pDoor->GetTargetDoorRatio() < 0.01f)
				{
					pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
				}
				else if(pDoor->GetTargetDoorRatio() > 0.99f)
				{
					pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
				}
			}
			m_bDoorControlButtonUp = false;
		}
	}
#endif

	// GTAV FIX B*1773938 - Need to set the brake when in vertical flight
	// mode so that the game doesn't think we're stuck
	// This is the same as the rotary wing aircraft
	bool bInVerticalFlightMode = false;

	if( pPlane->GetVerticalFlightModeAvaliable() )
	{
		if( pPlane->GetVerticalFlightModeRatio() == 1.0f )
		{
			bInVerticalFlightMode = true;
			pPlane->SetBrake(1.0f);
			pPlane->SetThrottle(0.0f);
			pPlane->SetHandBrake(false);
		}
	}

	if( !bInVerticalFlightMode )
	{
		ProcessPlayerControlInputsForBrakeAndGasPedal(pControl,bForceBrake, pVehicle);
	}

	// process crop duster smoke vfx in MP
	if (NetworkInterface::IsGameInProgress() &&
		pVehicle->GetStatus()==STATUS_PLAYER &&
		pVehicle->GetModelIndex()==MI_PLANE_DUSTER.GetModelIndex() &&
		pControl->GetParachuteSmoke().IsDown())
	{
		g_vfxVehicle.UpdatePtFxPlaneSmoke(pVehicle, Color32(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDrivePlane::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{
	//The following code prevents the plane from crashing when player brings up the phone internet. Commented it out per Ben's suggestion - NanM

// 	if (pVehicle->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive)
// 	{
// 		// If the player has no control (cut scene for instance) we will hit the brakes.
// 		pVehicle->SetBrake(1.0f);
// 		pVehicle->SetHandBrake(true);
// 		pVehicle->SetThrottle(0.0f);
// 		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->KeepAreaAroundPlayerClear();
// 		// Limit the velocity
// 		float Speed = pVehicle->GetVelocity().Mag();
// 
// 		if (Speed > ms_fPLANE_MAXSPEEDNOW)
// 		{
// 			pVehicle->SetVelocity( pVehicle->GetVelocity() * (ms_fPLANE_MAXSPEEDNOW / Speed) );
// 		}
// 	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDrivePlane::ProcessPlayerControlInputsForBrakeAndGasPedal(CControl* pControl, bool bForceBrake, CVehicle *pVehicle)
{
    //cast the vehicle into the applicable type
    Assert(dynamic_cast<CPlane*>(pVehicle));
    CPlane *pPlane = static_cast<CPlane*>(pVehicle);

    //get the vehicles reference frame velocity, so if they are driving on a plane we car work out the relative velocity rather then just using the absolute
    // Forwardness between -1 and 1
    float fSpeed = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
    float fAccelButton = pControl->GetVehicleFlyThrottleUp().GetNorm01();

	// GTAV - B*1754978 - If we're in flight we don't want the afterburner effect to cut out
	// for a frame so if the throttle control has never been active set it to the previous stored value
	if( !m_ThrottleControlHasBeenActive )
	{
		fAccelButton = pVehicle->GetThrottle();
		fAccelButton = Clamp(fAccelButton, -1.0f, 1.0f);
	}

    float fBrakeButton = pControl->GetVehicleFlyThrottleDown().GetNorm01();
    float fForwardness = fAccelButton - fBrakeButton;
	fForwardness = Clamp(fForwardness, -1.0f, 1.0f);

    if(bForceBrake)
    {
        pVehicle->SetBrake(1.0f);
        pVehicle->SetThrottle(0.0f);
    }
    else if(!pVehicle->m_nVehicleFlags.bEngineOn && pVehicle->m_nVehicleFlags.bIsDrowning)
    {
        pVehicle->SetBrake(0.0f);
        pVehicle->SetThrottle(0.0f);
    }
    else if(pVehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet)
		/*pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PUT_ON_HELMET)*/)
    {
        pVehicle->SetBrake(0.0f);
        pVehicle->SetThrottle(0.0f);
    }
    // We've stopped. Go where we want to go.
    else if (rage::Abs(fSpeed) < 0.5f)
    {
        // if both brake and throttle are held down, do a burn out
        if(fAccelButton > 0.6f && fBrakeButton > 0.6f)
        {
            pVehicle->SetThrottle(fAccelButton);
            pVehicle->SetBrake(fBrakeButton);
        }
        else
        {
            pVehicle->SetThrottle(fForwardness);
            pVehicle->SetBrake(0.0f);
        }
    }
    else
    {	
        // if both brake and throttle are held down, do a burn out
        if(fAccelButton > 0.6f && fBrakeButton > 0.6f)
        {
            pVehicle->SetThrottle(fAccelButton);
            pVehicle->SetBrake(fBrakeButton);
        }
        // going forwards or in air
        else if (fSpeed >= 0.0f || pPlane->IsInAir())
        {	
            // we want to go forwards so go faster
            if (fForwardness >= 0.0f)
            {
                pVehicle->SetThrottle(fForwardness);
                pVehicle->SetBrake(0.0f);
            }
            // we don't want to go forwards - so brake
            else
            {
                pVehicle->SetThrottle(0.0f);
                pVehicle->SetBrake(-fForwardness);
            }
        }
        // going backwards
        else
        {
            // want to go forwards
            if (fForwardness >= 0.0f)
            {
                // let us sit on the gas pedal if we're trying to get up a slope but sliding back
                if(pVehicle->GetThrottle() > 0.5f && fSpeed > -10.0f)
                {
                    pVehicle->SetThrottle(fForwardness);
                    pVehicle->SetBrake(0.0f);
                }
                // oh dear we're sliding back too fast, go for the brakes
                else
                {
                    pVehicle->SetThrottle(0.0f);
                    pVehicle->SetBrake(fForwardness);
                }
            }
            // we want to go backwards, so apply the gas
            else
            {
                pVehicle->SetThrottle(fForwardness);
                pVehicle->SetBrake(0.0f);
            }
        }
    }
}



//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveAutogyro
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveAutogyro::CTaskVehiclePlayerDriveAutogyro() :
CTaskVehiclePlayerDriveRotaryWingAircraft()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOGYRO);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAutogyro::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CAutogyro*>(pVehicle));
	CAutogyro *pAutogyro = static_cast<CAutogyro*>(pVehicle);

	CTaskVehiclePlayerDriveRotaryWingAircraft::ProcessDriverInputsForPlayerOnUpdate(pPlayerPed, pControl, pAutogyro);

	ProcessPlayerControlInputsForBrakeAndGasPedal(pPlayerPed->GetControlFromPlayer(),false, pAutogyro);

	if(pAutogyro->HasContactWheels())
	{
		pAutogyro->SetSteerAngle(-pAutogyro->GetYawControl()*pAutogyro->pHandling->m_fSteeringLock);
	}	
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveAutogyro::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle))
{

}


//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveHeli
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveHeli::CTaskVehiclePlayerDriveHeli() :
CTaskVehiclePlayerDriveRotaryWingAircraft()
{
    m_bJustEnteredHoverMode = false;
    m_bJustLeftHoverMode = false;
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_HELI);
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveHeli::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const s32 state = GetState();
	if(state == State_Hover)
	{
		settings.m_CameraHash = g_HeliHoverCameraHash;
		//NOTE: We cannot clone the previous orientation as the hover camera needs to snap to align with the current heli orientation initially.
		settings.m_Flags.ClearFlag(Flag_ShouldClonePreviousOrientation);
		settings.m_Flags.ClearFlag(Flag_ShouldFallBackToIdealHeading);
	}
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveHeli::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		//
		FSM_State(State_Drive)
			FSM_OnEnter
				Drive_OnEnter(pVehicle);
			FSM_OnUpdate
				return Drive_OnUpdate(pVehicle);
		//
		FSM_State(State_NoDriver)
			FSM_OnEnter
				NoDriver_OnEnter(pVehicle);
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
		//
		FSM_State(State_Hover)
			FSM_OnEnter
				Hover_OnEnter(pVehicle);
			FSM_OnUpdate
				return Hover_OnUpdate(pVehicle);
            FSM_OnExit
                Hover_OnExit(pVehicle);
        //
        FSM_State(State_InactiveHover)
            FSM_OnEnter
                InactiveHover_OnEnter(pVehicle);
            FSM_OnUpdate
                return InactiveHover_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveHeli::ProcessDriverInputsForPlayerOnUpdate(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CHeli*>(pVehicle));
	CHeli *pHeli= static_cast<CHeli*>(pVehicle);

	Assert(pPlayerPed && (pPlayerPed->IsControlledByLocalPlayer()));

	// If this is a seaplane it could be anchored. Make sure anchor is cleared in this state.
	if( CSeaPlaneExtension* pSeaPlaneExtension = pHeli->GetExtension<CSeaPlaneExtension>() )
	{
		if( pSeaPlaneExtension->GetAnchorHelper().IsAnchored() && !pSeaPlaneExtension->GetAnchorHelper().ShouldForcePlayerBoatToRemainAnchored() )
		{
			pSeaPlaneExtension->GetAnchorHelper().Anchor( false );
		}
	}

	// auto throttle will keep us hovering, but won't stop us stopping from moving up, if that makes any sense?
	float fDesiredThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();

	float fAutoThrottle = 0.0f;
	{
		if( !pVehicle->HasContactWheels() &&
			( !pVehicle->pHandling->GetSeaPlaneHandlingData() || !pVehicle->m_nFlags.bPossiblyTouchesWater ) )
		{
			fAutoThrottle = 0.98f * rage::Clamp( 1.0f - pVehicle->GetVelocityIncludingReferenceFrame().z * ms_fHELI_AUTO_THROTTLE_FALLOFF, 0.0f, 1.0f );
		}

		// need to combine desired throttle and auto throttle, somehow
		if(fDesiredThrottleControl > 0.01f)
		{
			fDesiredThrottleControl += 1.0f;
		}
		else if(fDesiredThrottleControl < -0.01f)
		{
			fDesiredThrottleControl = rage::Max(-0.1f, fDesiredThrottleControl + 0.5f);
		}
		else
		{
			fDesiredThrottleControl = fAutoThrottle;
		}
	}

	float fThrottleChangeMult = rage::Powf(ms_fHELI_THROTTLE_CONTROL_DAMPING, fwTimer::GetTimeStep());

	// Store this here since it will be overridden by superclass process control inputs
	fDesiredThrottleControl = fThrottleChangeMult*pHeli->GetThrottleControl() + (1.0f - fThrottleChangeMult)*fDesiredThrottleControl;

	CTaskVehiclePlayerDriveRotaryWingAircraft::ProcessDriverInputsForPlayerOnUpdate(pPlayerPed, pControl, pHeli);

	pHeli->SetThrottleControl(fDesiredThrottleControl);

    static dev_bool sbEnableHoverMode = false;
    if(sbEnableHoverMode)
    {
        const ioValue &ioVal = pControl->GetVehicleHorn();

        // if stick was clicked then enter "hover" mode
        if(ioVal.IsReleased() && !m_bJustLeftHoverMode)
        {
            m_bJustEnteredHoverMode = true;
            SetState(State_Hover);
        }
        else
        {
            m_bJustLeftHoverMode = false;
        }
    }

	//Check if heli has light and if it needs turning on/off
	if(pControl->GetVehicleHeadlight().IsPressed() && pPlayerPed->GetPlayerInfo()->GetCanUseSearchLight())
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if(pWeaponMgr)
		{
			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT)
				{
					CSearchLight* pSearchlight = static_cast<CSearchLight*>(pWeapon);
					pSearchlight->Fire(pPlayerPed,pVehicle,VEC3_ZERO);
				}
			}
		}
	}

	// Process the rear ramp door
//#if 0 // Disable open/close rear door of cargobob. B* 1369017
#if __BANK
	if( CVehicleFactory::ms_rearDoorsCanOpen )
	{
		m_bDoorControlButtonUp |= pControl->GetVehicleRoof().IsUp();

		if(m_bDoorControlButtonUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldDoorControlButtonDown))
		{
			CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_R);
			if(pDoor && pDoor->GetIsIntact(pHeli))
			{
				if(pDoor->GetIsLatched(pHeli) || pDoor->GetTargetDoorRatio() < 0.01f)
				{
					pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
				}
				else if(pDoor->GetTargetDoorRatio() > 0.99f)
				{
					pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
				}
			}
			m_bDoorControlButtonUp = false;
		}
	}
#endif

	bool HasRopeAndHook = false;

	for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

		if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
		{
			HasRopeAndHook = true;
			CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);

			//check whether the player wants to detach the vehicle
			if(pControl->GetVehicleGrapplingHook().IsPressed())
			{
				pPickUpRope->Trigger();
			}
		}
	}

	if(!HasRopeAndHook)
	{
		TUNE_BOOL(OVERRIDE_CARGOBOB_PICKUP_ROPE_TYPE, false);
		TUNE_BOOL(CARGOBOB_USE_MAGNET, true);
		// ghost vehicles in non-contact races are not permitted to use the hook
		if(pControl->GetVehicleGrapplingHook().IsPressed() && !NetworkInterface::IsAGhostVehicle(*pHeli))
		{
			if(OVERRIDE_CARGOBOB_PICKUP_ROPE_TYPE)
				pHeli->SetPickupRopeType(CARGOBOB_USE_MAGNET ? PICKUP_MAGNET : PICKUP_HOOK);

			pHeli->AddPickupRope();
		}
	}
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveHeli::ProcessDriverInputsForPlayerInactiveOnUpdate( CControl *UNUSED_PARAM(pControl), CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CHeli*>(pVehicle));
	CHeli *pHeli= static_cast<CHeli*>(pVehicle);

	if (pHeli->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive)
	{
		if(pHeli->HasContactWheels())
		{ 
			pHeli->SetThrottleControl(0.0f);
			pHeli->SetYawControl(0.0f);
			pHeli->SetPitchControl(0.0f);
			pHeli->SetRollControl(0.0f);
		}
		else
		{
			SetState(State_InactiveHover);
		}
	}
}


//////////////////////////////////////////////////////////////////////
//State_InactiveHover
//////////////////////////////////////////////////////////////////////
void CTaskVehiclePlayerDriveHeli::InactiveHover_OnEnter	(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleHover() );
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveHeli::InactiveHover_OnUpdate	(CVehicle* pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CHeli*>(pVehicle));
	CHeli *pHeli= static_cast<CHeli*>(pVehicle);

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_HOVER))
	{
		// If the subtask has terminated - restart
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	if(pHeli->HasContactWheels() && pHeli->GetVelocityIncludingReferenceFrame().Mag2() < 16.0f)
	{
		SetState(State_Drive);
		return FSM_Continue;
	}

	//get the player controls
	Assert( pVehicle->GetDriver() );
	CPed* pPlayerPed = pVehicle->GetDriver();

	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl==NULL)
		return FSM_Continue;

	//the control is active again so just go back to the drive state.
	if(IsThePlayerControlInactive(pControl) == false)
	{
		SetState(State_Drive);
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////
//State_Hover
//////////////////////////////////////////////////////////////////////
void CTaskVehiclePlayerDriveHeli::Hover_OnEnter	(CVehicle* pVehicle)
{
    //cast the vehicle into the applicable type
    Assert(dynamic_cast<CHeli*>(pVehicle));
    CHeli *pHeli= static_cast<CHeli*>(pVehicle);

    pHeli->SetHoverMode(true);
    pHeli->SetThrustVectoringMode(false);

    Vector3 v = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());

    sVehicleMissionParams params;
    params.m_fCruiseSpeed = 0.0f;
    params.SetTargetPosition(v);
    params.m_fTargetArriveDist = 0.0f;
	SetNewTask(rage_new CTaskVehicleGoToHelicopter(params
		, CTaskVehicleGoToHelicopter::HF_DontModifyOrientation|CTaskVehicleGoToHelicopter::HF_DontModifyPitch
		|CTaskVehicleGoToHelicopter::HF_DontModifyThrottle|CTaskVehicleGoToHelicopter::HF_DontDoAvoidance
		, -1.0f, 0) );

}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePlayerDriveHeli::Hover_OnUpdate	(CVehicle* pVehicle)
{
    const bool bHasDriver = pVehicle->GetDriver() != NULL;
    const bool bDriverInjured = pVehicle->GetDriver() && pVehicle->GetDriver()->IsInjured();
    const bool bDriverArrested = pVehicle->GetDriver() && pVehicle->GetDriver()->GetIsArrested();
    const bool bDriverJumpingOut = false;
    CControl *pControl = NULL;


    m_fTimeSincePlayingRecording = rage::Min(999.0f, m_fTimeSincePlayingRecording + fwTimer::GetTimeStep());
    if( pVehicle && pVehicle->IsRunningCarRecording() )
    {
        m_fTimeSincePlayingRecording = 0.0f;
    }

    if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER))
    {
        // If the subtask has terminated - restart
        SetFlag(aiTaskFlags::RestartCurrentState);
    }

    if(bHasDriver && !bDriverArrested && !bDriverInjured && !bDriverJumpingOut && taskVerifyf(pVehicle->GetDriver()->GetPlayerInfo(), "Invalid player info!"))
    {
        Assert( pVehicle->GetDriver() );
        Assert(pVehicle->GetDriver()->IsPlayer());
        CPed* pPlayerPed = pVehicle->GetDriver();

        CControl *pControl = pPlayerPed->GetControlFromPlayer();
        if(pControl==NULL)
            return FSM_Continue;

        ProcessHoverDriverInputsForPlayer(pPlayerPed, pControl, pVehicle);

        // Dont override the velocity unless the player hasn't been running a recording for at least a second
        if(IsThePlayerControlInactive(pControl) && m_fTimeSincePlayingRecording > 0.1f)
        {
            ProcessDriverInputsForPlayerInactiveOnUpdate(pControl, pVehicle);
        }
    }

    if(bHasDriver && !bDriverArrested && !bDriverInjured && !bDriverJumpingOut)
    {
        Assert( pVehicle->GetDriver() );
        Assert(pVehicle->GetDriver()->IsPlayer());
        CPed* pPlayerPed = pVehicle->GetDriver();

        pControl = pPlayerPed->GetControlFromPlayer();
    }


    if(pControl)//if we have control check whether we should leave "Hover" mode
    {
        const ioValue &ioVal = pControl->GetVehicleHorn();
        // if stick was clicked then quit "Hover" mode
        if(ioVal.IsReleased() && !m_bJustEnteredHoverMode)
        {
            m_bJustLeftHoverMode = true;
            SetState(State_Drive);
        }
        else
        {
            m_bJustEnteredHoverMode = false;
        }
    }
    else
    {
        SetState(State_Drive);//if anythings failed then just go back to the driving state.
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveHeli::Hover_OnExit	(CVehicle* pVehicle)
{
    //cast the vehicle into the applicable type
    Assert(dynamic_cast<CHeli*>(pVehicle));
    CHeli *pHeli= static_cast<CHeli*>(pVehicle);

    pHeli->SetHoverMode(false);
    pHeli->SetThrustVectoringMode(true);
}

////////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveHeli::ProcessHoverDriverInputsForPlayer		(CPed *UNUSED_PARAM(pPlayerPed), CControl *pControl, CVehicle *pVehicle)
{
    //cast the vehicle into the applicable type
    Assert(dynamic_cast<CHeli*>(pVehicle));
    CHeli *pHeli= static_cast<CHeli*>(pVehicle);

    CTask* pTask = GetSubTask();
    CTaskVehicleGoToHelicopter* pGotoHeliTask = NULL;

    if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
    {
        //Vector3 v = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
        pGotoHeliTask = (CTaskVehicleGoToHelicopter*)pTask;
    }

    if(pGotoHeliTask == NULL)
    {
        return;
    }

    // auto throttle will keep us hovering, but won't stop us stopping from moving up, if that makes any sense?
    float fDesiredThrottleControl = (pControl->GetVehicleFlyThrottleUp().GetNorm01()*2.0f) - pControl->GetVehicleFlyThrottleDown().GetNorm01();

    pHeli->SetThrottleControl(fDesiredThrottleControl);


	// If the throttle isn't pressed then hover around this point
    if(fDesiredThrottleControl == 0.0f)
    {
        if(pGotoHeliTask->GetHeliFlags()&CTaskVehicleGoToHelicopter::HF_DontModifyThrottle)
        {
            Vector3 v = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
            pGotoHeliTask->SetTargetPosition(&v);
            pGotoHeliTask->UnSetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyThrottle);
        }
    }
    else
    {
        pGotoHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyThrottle);
    }

    pHeli->SetPitchControl(0.0f);
    pHeli->SetRollControl(0.0f);

#if USE_SIXAXIS_GESTURES
    if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_AIRCRAFT))
    {
        // Static values here can be tweaked in widgets

        CPadGesture* pGesture = CControlMgr::GetPlayerPad()->GetPadGesture();
        if(pGesture)
        {
            // Rescale the raw pitch input to custom range
            // Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyPitchUpDown().IsEnabled())
			{
				pHeli->SetPitchControl(pGesture->GetPadPitchInRange(CHeli::MOTION_CONTROL_PITCH_MIN,CHeli::MOTION_CONTROL_PITCH_MAX,false,true));
			}

            // Rescale the raw roll too
            // Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyRollLeftRight().IsEnabled())
			{
				pHeli->SetRollControl(-pGesture->GetPadRollInRange(CHeli::MOTION_CONTROL_ROLL_MIN,CHeli::MOTION_CONTROL_ROLL_MAX,false));
			}

        }
    }
    //else
#endif	// USE_SIXAXIS_GESTURES
	{
		pControl->SetVehicleFlySteeringExclusive();
        pHeli->SetPitchControl( pHeli->GetPitchControl() + pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE));
        pHeli->SetRollControl( pHeli->GetRollControl() + -pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE));
    }

//Work out Yaw Control
    const Vector3	target				(pHeli->GetHoverModeDesiredTarget());
    Vector3	        targetDelta			(target - VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition()));

    const float		targetDeltaAngleFlat = fwAngle::GetATanOfXY(targetDelta.x, targetDelta.y);
    const Vector3	targetDeltaFlat		 (targetDelta.x, targetDelta.y, 0.0f);

    Vector3	forwardDirFlat = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB());
    forwardDirFlat.z = 0.0f;
    forwardDirFlat.Normalize();


    // Predict our future orientation.
    static float orientationPredictionTime = 0.3f;
    Vector3 vecForward(VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB()));
    const float currOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
    const float orientationDeltaPrev = fwAngle::LimitRadianAngleSafe(currOrientation - pHeli->GetHeliIntelligence()->GetOldOrientation());
    const float predictedOrientation = currOrientation + orientationDeltaPrev * (orientationPredictionTime / fwTimer::GetTimeStep());


    // Make the heli face the target coordinates.
    const float toTargetPredictedOrienationDelta = fwAngle::LimitRadianAngleSafe(targetDeltaAngleFlat - predictedOrientation);
    const float fYawControlUnclamped = toTargetPredictedOrienationDelta * -2.0f;
    const float fYawControl = rage::Clamp(fYawControlUnclamped, -1.0f, 1.0f);

    pHeli->SetYawControl(fYawControl);


    bool bPlayerRollOrPitchInput = false;
    if(pHeli->GetRollControl() == 0.0f)
    {
        pGotoHeliTask->UnSetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyRoll);
    }
    else
    {
        bPlayerRollOrPitchInput = true;
        pGotoHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyRoll);
        pHeli->SetRollControl(Clamp(pHeli->GetRollControl(), -0.5f, 0.5f));
    }

    if(pHeli->GetPitchControl() == 0.0f)
    {
        vecForward = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB());
        targetDelta.Normalize();
        float	desiredPitch = rage::Atan2f(targetDelta.z, targetDelta.XYMag());
        desiredPitch = rage::Clamp(desiredPitch, -1.0f, 1.0f);

        // In this new code we calculate the pitch that we want first.
        //static float pitchToDesiredSpeedRatio	= -0.1f;
        static float pitchPredictionTime		= 1.5f;
        static float pitchControlMultiplier		= 2.0f;

        const float pitch = rage::Atan2f(vecForward.z, vecForward.XYMag());
        float predictedPitch = pitch + (pitch - pHeli->GetHeliIntelligence()->GetOldTilt()) * (pitchPredictionTime / fwTimer::GetTimeStep());
        float fPitchControl = (desiredPitch - predictedPitch) * pitchControlMultiplier;
        fPitchControl = rage::Clamp(fPitchControl, -1.0f, 1.0f);

        pHeli->SetPitchControl(fPitchControl);

        pHeli->SetPitchControl(Clamp(pHeli->GetPitchControl(), -1.0f, 1.0f));
    }
    else
    {        
        bPlayerRollOrPitchInput = true;
        pHeli->SetPitchControl(Clamp(pHeli->GetPitchControl(), -0.5f, 0.5f));
    }


    if(bPlayerRollOrPitchInput)
    {
        pHeli->SetHoverMode(false);
    }
    else if( !pHeli->GetHoverMode())
    {
        pHeli->SetHoverMode(true);
        const Vector3 *vOriginalTarget = pGotoHeliTask->GetTargetPosition();
        Vector3 vNewTarget = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
        vNewTarget.z = vOriginalTarget->z;//maintain height.
        pGotoHeliTask->SetTargetPosition(&vNewTarget);
        pGotoHeliTask->UnSetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyThrottle);
    }

    pHeli->SetSteerAngle(0.0f);
    pHeli->SetBrake(1.0f);
    pHeli->SetThrottle(0.0f);
    pHeli->SetHandBrake(false);
}



////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePlayerDriveHeli::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Drive&&iState<=State_InactiveHover);
	static const char* aStateNames[] = 
	{
		"State_Drive",
		"State_NoDriver",
		"State_Hover",
        "State_InactiveHover"
	};

	return aStateNames[iState];
}
#endif


//////////////////////////////////////////////////////////////////////
//
//class CTaskVehiclePlayerDriveRotaryWingAircraft
//
//////////////////////////////////////////////////////////////////////

CTaskVehiclePlayerDriveRotaryWingAircraft::CTaskVehiclePlayerDriveRotaryWingAircraft () :
CTaskVehiclePlayerDrive()
#if RSG_PC
,m_MouseSteeringInput(false)
#endif
{
	m_TimeOfLastLandingGearToggle	= 0;
}

//////////////////////////////////////////////////////////////////////

void CTaskVehiclePlayerDriveRotaryWingAircraft::ProcessDriverInputsForPlayerOnUpdate(CPed *UNUSED_PARAM(pPlayerPed), CControl *pControl, CVehicle *pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CRotaryWingAircraft*>(pVehicle));
	CRotaryWingAircraft *pRotaryWingAircraft= static_cast<CRotaryWingAircraft*>(pVehicle);

	float fDesiredThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();

	float fThrottleChangeMult = rage::Powf(ms_fHELI_THROTTLE_CONTROL_DAMPING, fwTimer::GetTimeStep());
	pRotaryWingAircraft->SetThrottleControl(fThrottleChangeMult*pRotaryWingAircraft->GetThrottleControl() + (1.0f - fThrottleChangeMult)*fDesiredThrottleControl);

#if USE_SIXAXIS_GESTURES
	if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_AIRCRAFT))
	{
		// Static values here can be tweaked in widgets

		CPadGesture* pGesture = CControlMgr::GetPlayerPad()->GetPadGesture();
		if(pGesture)
		{
			// Rescale the raw pitch input to custom range
			// Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyPitchUpDown().IsEnabled())
			{
				pRotaryWingAircraft->SetPitchControl(pGesture->GetPadPitchInRange(CHeli::MOTION_CONTROL_PITCH_MIN,CHeli::MOTION_CONTROL_PITCH_MAX,false,true));
			}
			
			// Rescale the raw roll too
			// Values will be clamped below so don't bother getting this function to do it
			if(pControl->GetVehicleFlyRollLeftRight().IsEnabled())
			{
				pRotaryWingAircraft->SetRollControl(-pGesture->GetPadRollInRange(CHeli::MOTION_CONTROL_ROLL_MIN,CHeli::MOTION_CONTROL_ROLL_MAX,false));
			}

			// Dont clamp this in case we want ridic fast yaw
			if(pControl->GetVehicleFlyYawRight().IsEnabled() || pControl->GetVehicleFlyYawLeft().IsEnabled())
			{
				pRotaryWingAircraft->SetYawControl( pGesture->GetPadYaw()*CHeli::MOTION_CONTROL_YAW_MULT );
			}
		}
	}
	//else
#endif	// USE_SIXAXIS_GESTURES
	{
		pControl->SetVehicleFlySteeringExclusive();

#if RSG_PC
		ioValue::ReadOptions readOptions = ioValue::ALWAYS_DEAD_ZONE;

		bool bScaledAxisRollInput = pControl->GetVehicleFlyRollLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
		bool bScaledAxisPitchnput = pControl->GetVehicleFlyPitchUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
		if(bScaledAxisRollInput || bScaledAxisPitchnput)
		{
			readOptions = ioValue::DEFAULT_OPTIONS;
		}

		float fPitchControl = pControl->GetVehicleFlyPitchUpDown().GetNorm(readOptions);
		float fRollControl = -pControl->GetVehicleFlyRollLeftRight().GetNorm(readOptions);
		const bool bScaledAxisYawInput = pControl->GetVehicleFlyYawLeft().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS
										|| pControl->GetVehicleFlyYawRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS;
		// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work
		// (ideally we would alter this but its safer to fix the issue this way).
		float fYawControl = Abs(pControl->GetVehicleFlyYawRight().GetNorm()) - Abs(pControl->GetVehicleFlyYawLeft().GetNorm());

		static bool sb_pitchWasScaled = false;

		if( bScaledAxisPitchnput )
		{
			sb_pitchWasScaled = true;
		}

		if( sb_pitchWasScaled )
		{
			bScaledAxisPitchnput = true;
		}


		float timeStep = fwTimer::GetTimeStep();

		if(pControl->WasKeyboardMouseLastKnownSource())
		{
			if(bScaledAxisRollInput || bScaledAxisPitchnput || bScaledAxisYawInput)
			{
				// Undo fixup applied for scaled axis, without affecting scripted minigame inputs.
				if(bScaledAxisRollInput)
				{
					fRollControl *= c_fMouseAdjustmentScale;
				}
				if(bScaledAxisPitchnput)
				{
					fPitchControl *= c_fMouseAdjustmentScale;
				}
				if(bScaledAxisYawInput)
				{
					fYawControl *= c_fMouseAdjustmentScale;
				}
				m_MouseSteeringInput = true;
			}
			else if(pControl->GetVehicleFlyRollLeftRight().GetSource().m_Device == IOMS_MKB_AXIS || pControl->GetVehicleFlyPitchUpDown().GetSource().m_Device == IOMS_MKB_AXIS)// Keyboard steering input
			{
				m_MouseSteeringInput = false;
			}
		}
		else
		{
			// HACK TO FIX GTAV B*2870991 - The input returns the control immediately back to joypad control when playing on mouse, even when there isn't one plugged in
			if( !m_MouseSteeringInput ||
				Abs( fRollControl ) > c_fMouseTollerance ||
				Abs( fPitchControl ) > c_fMouseTollerance ||
				Abs( fYawControl ) > c_fMouseTollerance )
			{
				m_MouseSteeringInput = false;
			}
		}

		bool bMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle;
		bool bCameraMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera;

		if(m_MouseSteeringInput && ((bMouseFlySteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseFlySteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())))
		{
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_DEADZONE_CENTERING_SPEED_ROLL, 4.0f, 0.0f, 30.0f, 0.01f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_DEADZONE_CENTERING_SPEED_PITCH, 0.0f, 0.0f, 30.0f, 0.01f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_DEADZONE_CENTERING_SPEED_YAW, 0.0f, 0.0f, 30.0f, 0.01f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_MULTIPLIER, 1.0f, 0.0f, 30.0f, 0.01f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_CENTER_ALLOWANCE, 0.001f, 0.0f, 1.0f, 0.0001f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_INPUT_DEADZONE, FLT_EPSILON, 0.0f, 1.0f, 0.0001f);
			TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, HELI_STEERING_DELTA_MULT, 5.0f, 0.0f, 30.0f, 0.1f);

			float fAutoCenterMult = CPauseMenu::GetMenuPreference(PREF_MOUSE_AUTOCENTER_PLANE)/10.0f;

			//Roll
			if( bScaledAxisRollInput )
			{
				if (fabs(fRollControl) <= HELI_STEERING_INPUT_DEADZONE)
				{
					if(pRotaryWingAircraft->GetRollControl() > 0.0f)
					{
						fRollControl = Clamp(pRotaryWingAircraft->GetRollControl() - (HELI_STEERING_DEADZONE_CENTERING_SPEED_ROLL * fAutoCenterMult * timeStep), 0.0f, 1.0f);
					}
					else
					{
						fRollControl = Clamp(pRotaryWingAircraft->GetRollControl() + (HELI_STEERING_DEADZONE_CENTERING_SPEED_ROLL * fAutoCenterMult * timeStep),-1.0f, 0.0f);
					}
				}
				else if( Sign(fRollControl) == Sign(pRotaryWingAircraft->GetRollControl()) || rage::Abs(pRotaryWingAircraft->GetRollControl()) <= HELI_STEERING_CENTER_ALLOWANCE)
				{
					fRollControl = pRotaryWingAircraft->GetRollControl() + (fRollControl * HELI_STEERING_MULTIPLIER);
				}
				else
				{
					fRollControl = pRotaryWingAircraft->GetRollControl() + (fRollControl - pRotaryWingAircraft->GetRollControl()) * HELI_STEERING_DELTA_MULT * timeStep;
				}
			}

			//Pitch
			if( bScaledAxisPitchnput )
			{
				if (fabs(fPitchControl) <= HELI_STEERING_INPUT_DEADZONE)
				{
					if(pRotaryWingAircraft->GetPitchControl() > 0.0f)
					{
						fPitchControl = Clamp(pRotaryWingAircraft->GetPitchControl() - (HELI_STEERING_DEADZONE_CENTERING_SPEED_PITCH * fAutoCenterMult * timeStep), 0.0f, 1.0f);
					}
					else
					{
						fPitchControl = Clamp(pRotaryWingAircraft->GetPitchControl() + (HELI_STEERING_DEADZONE_CENTERING_SPEED_PITCH * fAutoCenterMult * timeStep),-1.0f, 0.0f);
					}
				}
				else if( Sign(fPitchControl) == Sign(pRotaryWingAircraft->GetPitchControl()) || rage::Abs(pRotaryWingAircraft->GetPitchControl()) <= HELI_STEERING_CENTER_ALLOWANCE)
				{
					fPitchControl = pRotaryWingAircraft->GetPitchControl() + (fPitchControl * HELI_STEERING_MULTIPLIER);
				}
				else
				{
					fPitchControl = pRotaryWingAircraft->GetPitchControl() + (fPitchControl - pRotaryWingAircraft->GetPitchControl()) * HELI_STEERING_DELTA_MULT * timeStep;
				}
			}

			//Yaw
			if( bScaledAxisYawInput )
			{
				if (fabs(fYawControl) <= HELI_STEERING_INPUT_DEADZONE)
				{
					if(pRotaryWingAircraft->GetYawControl() > 0.0f)
					{
						fYawControl = Clamp(pRotaryWingAircraft->GetYawControl() - (HELI_STEERING_DEADZONE_CENTERING_SPEED_YAW * fAutoCenterMult * timeStep), 0.0f, 1.0f);
					}
					else
					{
						fYawControl = Clamp(pRotaryWingAircraft->GetYawControl() + (HELI_STEERING_DEADZONE_CENTERING_SPEED_YAW * fAutoCenterMult * timeStep),-1.0f, 0.0f);
					}
				}
				else if( Sign(fYawControl) == Sign(pRotaryWingAircraft->GetYawControl()) || rage::Abs(pRotaryWingAircraft->GetYawControl()) <= HELI_STEERING_CENTER_ALLOWANCE)
				{
					fYawControl = pRotaryWingAircraft->GetYawControl() + (fYawControl * HELI_STEERING_MULTIPLIER);
				}
				else
				{
					fYawControl = pRotaryWingAircraft->GetYawControl() + (fYawControl - pRotaryWingAircraft->GetYawControl()) * HELI_STEERING_DELTA_MULT * timeStep;
				}
			}
		}

		pRotaryWingAircraft->SetPitchControl( fPitchControl );
		pRotaryWingAircraft->SetRollControl( fRollControl );
		pRotaryWingAircraft->SetYawControl( fYawControl );
#else

#if USE_SIXAXIS_GESTURES
		if (CControlMgr::GetPlayerPad() && !CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_AIRCRAFT))
#endif // USE_SIXAXIS_GESTURES
		{
			pRotaryWingAircraft->SetYawControl(0.0f);
			pRotaryWingAircraft->SetPitchControl(0.0f);
			pRotaryWingAircraft->SetRollControl(0.0f);
		}

		pRotaryWingAircraft->SetPitchControl( pRotaryWingAircraft->GetPitchControl() + pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE));
		pRotaryWingAircraft->SetRollControl( pRotaryWingAircraft->GetRollControl() + -pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE));

		// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work (ideally we would alter this but its safer to fix the issue this way).
		pRotaryWingAircraft->SetYawControl( pRotaryWingAircraft->GetYawControl() + Abs(pControl->GetVehicleFlyYawRight().GetNorm()) - Abs(pControl->GetVehicleFlyYawLeft().GetNorm()));
#endif
	}

	pRotaryWingAircraft->SetStrafeMode( Abs(pControl->GetVehicleFlyYawRight().GetNorm()) > 0.0f && Abs(pControl->GetVehicleFlyYawLeft().GetNorm()) > 0.0f );

	pRotaryWingAircraft->SetPitchControl(Clamp(pRotaryWingAircraft->GetPitchControl(), -1.0f, 1.0f));
	pRotaryWingAircraft->SetRollControl(Clamp(pRotaryWingAircraft->GetRollControl(), -1.0f, 1.0f));
	pRotaryWingAircraft->SetYawControl(Clamp(pRotaryWingAircraft->GetYawControl(), -1.0f, 1.0f));

	const float fSteeringAngle = pRotaryWingAircraft->InheritsFromBlimp() ? Clamp(pRotaryWingAircraft->GetRollControl(), -1.0f, 1.0f) * pVehicle->pHandling->m_fSteeringLock : 0.0f;
	pRotaryWingAircraft->SetSteerAngle(fSteeringAngle);
	pRotaryWingAircraft->SetBrake(1.0f);
	pRotaryWingAircraft->SetThrottle(0.0f);
	pRotaryWingAircraft->SetHandBrake(false);

	// Make new undercarriage input
	// Only allow toggle when in the air
	if( pRotaryWingAircraft->InheritsFromHeli() &&
		( !pRotaryWingAircraft->HasContactWheels() ||
		  pRotaryWingAircraft->GetIsJetPack() ) )
	{
		CHeli* pHeli = static_cast< CHeli* >( pRotaryWingAircraft );

		if( pHeli->HasLandingGear() )
		{
			if(pControl->GetVehicleFlyUndercarriage().IsPressed())
			{
				CLandingGear& landingGear = pHeli->GetLandingGear();

				// dirty hack to avoid conflict with player special abilities
				// if we don't consume the input value, the input will be used twice
				// because we disable input update inside CPlayerSpecialAbilityManager::UpdateSpecialAbilityTrigger
				const_cast<ioValue&>(pControl->GetVehicleFlyUndercarriage()).SetCurrentValue(0);

				u32 currentTime = fwTimer::GetTimeInMilliseconds();
				static bank_u32 minTimeForGearToggle = 500;
				switch( landingGear.GetPublicState() )
				{
				case CLandingGear::STATE_LOCKED_DOWN:
				case CLandingGear::STATE_DEPLOYING:
					if(currentTime > m_TimeOfLastLandingGearToggle + minTimeForGearToggle)
					{
						landingGear.ControlLandingGear(pVehicle,CLandingGear::COMMAND_RETRACT);
						m_TimeOfLastLandingGearToggle = currentTime;
					}
					break;
				case CLandingGear::STATE_LOCKED_UP:
				case CLandingGear::STATE_RETRACTING:
					if(currentTime > m_TimeOfLastLandingGearToggle + minTimeForGearToggle)
					{
						landingGear.ControlLandingGear(pVehicle,CLandingGear::COMMAND_DEPLOY);
						m_TimeOfLastLandingGearToggle = currentTime;
					}
					break;
				default:
					break;
				}
			}
		}
	}

}




//////////////////////////////////////////////////////////////////////
//
//class CTaskVehicleNoDriver
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////

CTaskVehicleNoDriver::CTaskVehicleNoDriver(NoDriverType noDriverType /*= NO_DRIVER_TYPE_ABANDONED*/) :
m_eNoDriverType(noDriverType)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_NO_DRIVER);
}


//////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleNoDriver::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//player is always dirty
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

	FSM_Begin
		// State_NoDriver
		FSM_State(State_NoDriver)
			FSM_OnUpdate
				return NoDriver_OnUpdate(pVehicle);
	FSM_End
}


//////////////////////////////////////////////////////////////////////
//State_NoDriver
//////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleNoDriver::NoDriver_OnUpdate(CVehicle* pVehicle)
{
	pVehicle->GetIntelligence()->ResetPretendOccupantEventData();
	pVehicle->GetIntelligence()->FlushPretendOccupantEventGroup();

	//set the controls for different vehicle types
	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_CAR:
    case VEHICLE_TYPE_QUADBIKE:
		ProcessAutomobile( static_cast<CAutomobile*>(pVehicle));
		break;
	case VEHICLE_TYPE_BOAT:
		ProcessBoat(static_cast<CBoat*>(pVehicle));
		break;
	case VEHICLE_TYPE_SUBMARINE:
        ProcessSub(static_cast<CSubmarine*>(pVehicle));
		break;
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_AUTOGYRO:	
	case VEHICLE_TYPE_BLIMP:
		ProcessRotaryWingAirfcraft	(static_cast<CAutogyro*>(pVehicle));
		break;
	case VEHICLE_TYPE_PLANE:
		ProcessPlane(static_cast<CPlane*>(pVehicle));
		break;
	case VEHICLE_TYPE_BIKE:
	case VEHICLE_TYPE_BICYCLE:	
		ProcessBike(static_cast<CBike*>(pVehicle));	
		break;
	case VEHICLE_TYPE_TRAIN:
	case VEHICLE_TYPE_TRAILER:
		break;//nothing setup for these yet.
	case VEHICLE_TYPE_SUBMARINECAR:
		{
			CSubmarineCar* submarineCar = (static_cast<CSubmarineCar*>(pVehicle));
			if( submarineCar->IsInSubmarineMode() )
			{
				ProcessSub( static_cast<CSubmarine*>(pVehicle));
			}
			else
			{
				ProcessAutomobile( static_cast<CAutomobile*>(pVehicle));
			}
		}
		break;
	case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
	case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
		{
			ProcessAutomobile( static_cast<CAutomobile*>(pVehicle) );
		}
		break;
	default:
		Assertf(0,"Vehicle type not supported");
	}

	return FSM_Quit;//just return as the controls should not get changed until another task is put on the tree.
}

/////////////////////////////////////////////////////////////////////////////

void CTaskVehicleNoDriver::ProcessAutomobile(CVehicle* pVehicle)
{
	switch(m_eNoDriverType)
	{
	case NO_DRIVER_TYPE_ABANDONED:
	case NO_DRIVER_TYPE_INITIALLY_CREATED:
		pVehicle->m_nVehicleFlags.bSuppressBrakeLight = true;
		if((pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE && pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < 0.1f))
			pVehicle->SetBrake(1.0f);
		if(pVehicle->m_nVehicleFlags.bRestingOnPhysical)
			pVehicle->SetBrake(0.5f);
		else if(pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < 0.5f)
			pVehicle->SetBrake(0.2f);
		else
			pVehicle->SetBrake(0.0f);

		pVehicle->SetHandBrake(false);
		//pVehicle->SetSteerAngle(0.0f);
		pVehicle->SetThrottle(0.0f);
		break;

	case NO_DRIVER_TYPE_PLAYER_DISABLED:
		if(pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < 0.01f || (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer()
			&& (pVehicle->GetDriver()->GetIsArrested())))
		{
			pVehicle->SetHandBrake(true);
			pVehicle->SetBrake(1.0f);
			pVehicle->SetThrottle(0.0f);
		}
		else
		{
			pVehicle->SetBrake(0.0f);
			pVehicle->SetHandBrake(false);
		}
		pVehicle->SetSteerAngle(0.0f);
		pVehicle->SetThrottle(0.0f);
		break;

	case NO_DRIVER_TYPE_WRECKED:
		pVehicle->SetBrake(0.05f);
		pVehicle->SetHandBrake(true);
		//pVehicle->SetSteerAngle(0.0f);
		pVehicle->SetThrottle(0.0f);
		break;

    case NO_DRIVER_TYPE_TOWED:
        pVehicle->SetBrake(0.00f);
        pVehicle->SetHandBrake(false);
        pVehicle->SetSteerAngle(0.0f);
        pVehicle->SetThrottle(0.0f);
        break;

	default:
		break;
	}
	
	if(!pVehicle->IsAlarmActivated())
	{
		pVehicle->StopHorn();
	}
}

float fPlaneApplyLargeAirBrakeSpeed2 = 25.0f;
float fPlaneStillVelocityThreshold = 0.1f;

void CTaskVehicleNoDriver::ProcessPlane(CPlane* pPlane)
{
	if (pPlane->m_CarGenThatCreatedUs == -1 || pPlane->m_LastTimeWeHadADriver > 0)
	{
		pPlane->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}

	switch(m_eNoDriverType)
	{
	case NO_DRIVER_TYPE_ABANDONED:
	{
		// Don't crash the Microlight if at low altitude while abandoning (similar to helicopters)
		bool bNearGround = false;
		CVehicleModelInfo* pModelInfo = pPlane->GetVehicleModelInfo();
		if (pModelInfo && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_CRASH_ABANDONED_NEAR_GROUND))
		{
			const float fHeightThreshold = 3.0f;
			Vector3 vStart = VEC3V_TO_VECTOR3(pPlane->GetVehiclePosition());
			Vector3 vEnd = vStart;
			vEnd.z += pPlane->GetBoundingBoxMin().z - fHeightThreshold;
			WorldProbe::CShapeTestHitPoint probeHitPoint;
			WorldProbe::CShapeTestResults probeResults(probeHitPoint);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetExcludeEntity(pPlane);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
			probeDesc.SetIsDirected(true);

			bNearGround = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		}

		if(pPlane->IsInAir() && !bNearGround)
		{
			// Don't go into crash mode if another player is jacking you in VTOL mode
			bool bPreventCrash = false;
			CPed* lastDriver = 0;
			if (pPlane->GetSeatManager())
				lastDriver = pPlane->GetSeatManager()->GetLastPedInSeat(0);

			if (pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() > 0 && lastDriver && (lastDriver->GetPedResetFlag(CPED_RESET_FLAG_BeingJacked) || lastDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir)))
			{
				CComponentReservation* pComponentReservation = pPlane->GetComponentReservationMgr()->GetSeatReservation(0, SA_directAccessSeat, 0);
				if (pComponentReservation && pComponentReservation->GetPedUsingComponent())
				{
					bPreventCrash = true;
				}
			}

			// Clear hover mode so we don't land back on plane after ejecting
			if (!bPreventCrash && pPlane->GetVerticalFlightModeAvaliable())
			{
				pPlane->SetVerticalFlightModeRatio(0.0f);
			}

			if(!bPreventCrash && !pPlane->IsNetworkClone() && pPlane->GetSeatManager()->GetNumPlayers() == 0 && !pPlane->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH))
			{
				lastDriver = 0;
				if (pPlane->GetSeatManager())
					lastDriver = pPlane->GetSeatManager()->GetLastPedInSeat(0);

				// Create a crash task if one doesn't already exist
#if __BANK
				aiDebugf1("CTaskVehicleNoDriver::ProcessPlane() - Creating CTaskVehicleCrash for %s(%p) that is above ground due to NO_DRIVER_TYPE_ABANDONED", AILogging::GetDynamicEntityNameSafe(pPlane), pPlane);
#endif
				CTaskVehicleCrash *pCrashTask = rage_new CTaskVehicleCrash(lastDriver);

				pCrashTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, false);
				pCrashTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, false);
				pCrashTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, true);

				pPlane->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCrashTask, VEHICLE_TASK_PRIORITY_CRASH);
			}
		}
		else
		{
			float fPlaneVelocitySqr = pPlane->GetVelocityIncludingReferenceFrame().Mag2();

			if (fPlaneVelocitySqr > fPlaneStillVelocityThreshold*fPlaneStillVelocityThreshold)
			{
				pPlane->SetBrake(1.0f);
			}

			pPlane->SetHandBrake(bNearGround); // Otherwise the Microlight will roll away forever
			if(pPlane->GetVelocityIncludingReferenceFrame().Mag2() > fPlaneApplyLargeAirBrakeSpeed2)
			{
				pPlane->SetAirBrakeControl(5.0f);
			}
			pPlane->SetThrottle(0.0f);
		}
	}
		break;
	case NO_DRIVER_TYPE_INITIALLY_CREATED:
		pPlane->m_nVehicleFlags.bSuppressBrakeLight = true;
		if(pPlane->GetVelocityIncludingReferenceFrame().Mag2() < 0.1f)
			pPlane->SetBrake(1.0f);
		if(pPlane->m_nVehicleFlags.bRestingOnPhysical)
			pPlane->SetBrake(0.5f);
		else if(pPlane->GetVelocityIncludingReferenceFrame().Mag2() < 0.5f)
			pPlane->SetBrake(0.2f);
		else
			pPlane->SetBrake(0.0f);

		pPlane->SetHandBrake(false);
		//pVehicle->SetSteerAngle(0.0f);
		pPlane->SetThrottle(0.0f);
		break;
	case NO_DRIVER_TYPE_PLAYER_DISABLED:
		if(pPlane->GetVelocityIncludingReferenceFrame().Mag2() < 0.01f || (pPlane->GetDriver() && pPlane->GetDriver()->IsPlayer()
			&& (pPlane->GetDriver()->GetIsArrested())))
		{
			pPlane->SetHandBrake(true);
			pPlane->SetBrake(1.0f);
			pPlane->SetThrottle(0.0f);
		}
		else
		{
			pPlane->SetBrake(0.0f);
			pPlane->SetHandBrake(false);
		}
		pPlane->SetSteerAngle(0.0f);
		pPlane->SetThrottle(0.0f);
		break;

	case NO_DRIVER_TYPE_WRECKED:
		pPlane->SetBrake(0.05f);
		pPlane->SetHandBrake(true);
		//pPlane->SetSteerAngle(0.0f);
		pPlane->SetThrottle(0.0f);
		break;

    case NO_DRIVER_TYPE_TOWED:
        pPlane->SetBrake(0.00f);
        pPlane->SetHandBrake(false);
        pPlane->SetSteerAngle(0.0f);
        pPlane->SetThrottle(0.0f);
        break;

	default:
		break;
	}
	
	if(!pPlane->IsAlarmActivated())
	{
		pPlane->StopHorn();
	}
}

const float SECOND_LOD_DISTANCE		= 150.0f;

////////////////////////////////////////////////////////////////////////////
void		CTaskVehicleNoDriver::ProcessSub					(CSubmarine* pSub)
{
    switch(m_eNoDriverType)
    {
    case(NO_DRIVER_TYPE_ABANDONED):
    case(NO_DRIVER_TYPE_PLAYER_DISABLED):
    case(NO_DRIVER_TYPE_WRECKED):
    case(NO_DRIVER_TYPE_TOWED):
	case(NO_DRIVER_TYPE_INITIALLY_CREATED):
        {
			CSubmarineHandling* subHandling = pSub->GetSubHandling();
            pSub->SetSteerAngle(0.0f);
            pSub->SetHandBrake(false);
            pSub->SetBrake(1.0f);
            pSub->SetThrottle(0.0f);
            subHandling->SetDiveControl(0.1f);
			subHandling->SetPitchControl(0.0f);
			subHandling->SetYawControl(0.0f);

            Vector3 vecTemp = VEC3V_TO_VECTOR3(pSub->GetTransform().GetPosition()) - CGameWorld::FindLocalPlayerCentreOfWorld();
            if( vecTemp.Mag() > SECOND_LOD_DISTANCE )
            {
                pSub->SetVelocity(Vector3(0.0f,0.0f,0.0f));	// just incase
                pSub->SetAngVelocity(Vector3(0.0f,0.0f,0.0f));
                return;
            }
        }
        break;
    default:
        break;
    }
}

////////////////////////////////////////////////////////////////////////////
void CTaskVehicleNoDriver::ProcessBoat(CVehicle* pBoat)
{	
	switch(m_eNoDriverType)
	{
		case(NO_DRIVER_TYPE_ABANDONED):
		case(NO_DRIVER_TYPE_PLAYER_DISABLED):
		case(NO_DRIVER_TYPE_WRECKED):
        case(NO_DRIVER_TYPE_TOWED):
		case(NO_DRIVER_TYPE_INITIALLY_CREATED):
			{
				// don't want to apply expensive physics and buoyancy calculations 
				// if range from player is large, since effects will not be noticed
				pBoat->GetBoatHandling()->SetInWater( 1 );
				pBoat->GetBoatHandling()->SetEngineInWater( 1 );
				pBoat->SetIsInWater( TRUE );

				pBoat->SetSteerAngle(0.0f);
				pBoat->SetHandBrake(false);
				pBoat->SetBrake(0.25f);
				pBoat->SetThrottle(0.0f);

				Vector3 vecTemp = VEC3V_TO_VECTOR3(pBoat->GetTransform().GetPosition()) - CGameWorld::FindLocalPlayerCentreOfWorld();
				if( vecTemp.Mag() > SECOND_LOD_DISTANCE )
				{
					pBoat->SetVelocity(Vector3(0.0f,0.0f,0.0f));	// just incase
					pBoat->SetAngVelocity(Vector3(0.0f,0.0f,0.0f));
					return;
				}
			}
			break;
		default:
			break;
	}
}

////////////////////////////////////////////////////////////////////////////
extern float HELI_ABANDONED_THROTTLE; // from Heli.cpp

void CTaskVehicleNoDriver::ProcessRotaryWingAirfcraft(CRotaryWingAircraft* pAircraft)
{
	if (pAircraft->m_CarGenThatCreatedUs == -1 || pAircraft->m_LastTimeWeHadADriver > 0)
	{
		pAircraft->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}

	switch(m_eNoDriverType)
	{
	case NO_DRIVER_TYPE_ABANDONED:
	case NO_DRIVER_TYPE_INITIALLY_CREATED:

		pAircraft->SetBrake(1.0f);

		if(!pAircraft->IsInAir())
		{
			pAircraft->SetThrottleControl(0.0f);
			pAircraft->SetPitchControl(0.0f);
			pAircraft->SetYawControl(0.0f);
			pAircraft->SetRollControl(0.0f);;
		}
		else
		{
			pAircraft->SetThrottleControl(HELI_ABANDONED_THROTTLE);
			pAircraft->SetYawControl( pAircraft->GetYawControl() + (pAircraft->GetYawControl() > 0.0f ? 1.0f : -1.0f) * sfHeliAbandonedYawMult * (fwRandom::GetRandomTrueFalse() ? fwRandom::GetRandomNumberInRange(0.5f, 1.0f) : fwRandom::GetRandomNumberInRange(-0.5f, -0.25f)));
			pAircraft->SetYawControl( Clamp(pAircraft->GetYawControl(), -10.0f, 10.0f));

			pAircraft->SetPitchControl( pAircraft->GetPitchControl() +  sfHeliAbandonedPitchMult * fwRandom::GetRandomNumberInRange(0.5f, 1.0f) * (fwRandom::GetRandomTrueFalse() ? 1.0f : -1.0f));
			pAircraft->SetPitchControl( Clamp(pAircraft->GetPitchControl(), -10.0f, 10.0f) );

			pAircraft->SetRollControl( pAircraft->GetRollControl() + sfHeliAbandonedPitchMult * fwRandom::GetRandomNumberInRange(0.5f, 1.0f) * (fwRandom::GetRandomTrueFalse() ? 1.0f : -1.0f));
			pAircraft->SetRollControl( Clamp(pAircraft->GetRollControl(), -10.0f, 10.0f) );
		}
		break;
	case NO_DRIVER_TYPE_WRECKED:
	case NO_DRIVER_TYPE_PLAYER_DISABLED:
    case NO_DRIVER_TYPE_TOWED:
		{
			const bool bHasDriver = pAircraft->GetDriver() != NULL;
			const bool bDriverArrested = pAircraft->GetDriver() && pAircraft->GetDriver()->GetIsArrested();
			const bool bDriverInjured = pAircraft->GetDriver() && pAircraft->GetDriver()->IsInjured();
			if( !bHasDriver || bDriverArrested || bDriverInjured ) 
			{
				pAircraft->SetThrottle(0.0f);
				if(  bDriverInjured && pAircraft->HasContactWheels() == false )
				{
					pAircraft->SetThrottleControl(HELI_CRASH_THROTTLE);
					pAircraft->SetPitchControl(HELI_CRASH_PITCH);
					pAircraft->SetYawControl(HELI_CRASH_YAW);
					pAircraft->SetRollControl(HELI_CRASH_ROLL);
				}
				else
				{
					pAircraft->SetThrottleControl(0.0f);
					pAircraft->SetPitchControl(0.0f);
					pAircraft->SetYawControl(0.0f);
					pAircraft->SetRollControl(0.0f);
				}
			}
		}
		break;
	default:
        Assert(0);
		break;		
	}
}


////////////////////////////////////////////////////////////////////////////

void CTaskVehicleNoDriver::ProcessBike(CBike* pBike)
{
	switch(m_eNoDriverType)
	{
		case NO_DRIVER_TYPE_ABANDONED:
		case NO_DRIVER_TYPE_INITIALLY_CREATED:
			{
				pBike->SetBrake(0.0f);
				pBike->SetHandBrake(false);
				pBike->SetThrottle(0.0f);

				if(pBike->GetVelocityIncludingReferenceFrame().Mag2() < 5.0f || pBike->m_nBikeFlags.bOnSideStand)
				{
					pBike->SetBrake(HIGHEST_BRAKING_WITHOUT_LIGHT);
					if (pBike->GetLayoutInfo() && !pBike->GetLayoutInfo()->GetBikeLeansUnlessMoving())
					{
						if(pBike->m_nBikeFlags.bOnSideStand)
						{
							pBike->SetHandBrake(true);
							float standLeanAngle = pBike->pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
							if(standLeanAngle != 0.f && pBike->GetSteerAngle() == 0.0f)
							{
								const float fBikeOnStandLeanAngleInv = 1.0f / standLeanAngle;
								float fSteerAngleFromLean = Clamp(pBike->m_fBikeLeanAngle * fBikeOnStandLeanAngleInv, 0.0f, 1.0f);
								pBike->SetSteerAngle(fSteerAngleFromLean * BIKE_ON_STAND_STEER_ANGLE);
							}
						}
					}
				}
				break;
			}

		case NO_DRIVER_TYPE_PLAYER_DISABLED:
			{
				const bool bDriverArrested = pBike->GetDriver() && pBike->GetDriver()->GetIsArrested();
				if(pBike->GetVelocityIncludingReferenceFrame().Mag2() < 0.01f || bDriverArrested )
				{
					pBike->SetBrake(1.0f);
					pBike->SetHandBrake(true);
				}
				else
				{
					pBike->SetBrake(0.0f);
					pBike->SetHandBrake(false);
				}
				pBike->SetSteerAngle(0.0f);
				pBike->SetThrottle(0.0f);
				break;
			}

		case NO_DRIVER_TYPE_WRECKED:
			{
				pBike->SetBrake(0.05f);
				pBike->SetHandBrake(true);
				pBike->SetSteerAngle(0.0f);
				pBike->SetThrottle(0.0f);
				break;
			}
        case NO_DRIVER_TYPE_TOWED:
            {
                pBike->SetBrake(0.00f);
                pBike->SetHandBrake(false);
                pBike->SetSteerAngle(0.0f);
                pBike->SetThrottle(0.0f);
                break;
            }
		default:
			Assert(0);
			break;
	}
	
	pBike->StopHorn();
}

//////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleNoDriver::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_NoDriver&&iState<=State_NoDriver);
	static const char* aStateNames[] = 
	{
		"State_NoDriver",
	};

	return aStateNames[iState];
}
#endif
