// File header
#include "Task/Vehicle/TaskCarCollisionResponse.h"

// Framework headers
#include "grcore/debugdraw.h" 
#include "fwmaths/Angle.h"
#include "phbound/boundcomposite.h"

// Game Headers
#include "audio/caraudioentity.h"
#include "Game/Wanted.h"
#include "Network/NetworkInterface.h"
#include "Peds/CombatBehaviour.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Scene/world/GameWorld.h"
#include "Script/Script.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskNavmesh.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Response/TaskAgitated.h"
#include "task/Response/TaskFlee.h"
#include "Task/System/TaskClassInfo.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/Weapon.h"
#include "vehicleAi\vehicleintelligence.h"
#include "vehicleAi\Task\TaskVehicleGoTo.h"
#include "vehicleAi\Task\TaskVehicleBlock.h"
#include "vehicleAi\Task\TaskVehicleCruise.h"
#include "vehicleAi\Task\TaskVehicleFleeBoat.h"

AI_OPTIMISATIONS()

// Constants
const float QUIT_COMBAT_DISTANCE_SQ      = 30.0f * 30.0f;
const float HEALTH_AFTER_LARGE_COLLISION = 90.0f;

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

bool IsCombatReactionAvailable(const CPed* pPed)
{
	return !CTheScripts::GetPlayerIsOnAMission() && pPed->CheckBraveryFlags(BF_REACT_ON_COMBAT);
}

bool IsWithinCombatDistance(const CPed* pPed1, const CPed* pPed2)
{
	if(pPed1 && pPed2)
	{
//		Vector3 vDistToOtherDriver(pPed2->GetPosition());
//		vDistToOtherDriver -= pPed1->GetPosition();

//		if(vDistToOtherDriver.Mag2() < QUIT_COMBAT_DISTANCE_SQ)
		if (IsLessThanAll(DistSquared(pPed1->GetTransform().GetPosition(), pPed2->GetTransform().GetPosition()), ScalarVFromF32(QUIT_COMBAT_DISTANCE_SQ)))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// CTaskComplexCarReactToVehicleCollision
//////////////////////////////////////////////////////////////////////////

CTaskCarReactToVehicleCollision::Tunables CTaskCarReactToVehicleCollision::sm_Tunables;
IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskCarReactToVehicleCollision, 0xcd6661fa);

CTaskCarReactToVehicleCollision::CTaskCarReactToVehicleCollision(CVehicle* pMyVehicle, CVehicle* pOtherVehicle, float fDamageDone, const Vector3& vDamagePos, const Vector3& vDamageDir)
: m_pMyVehicle(pMyVehicle)
, m_pOtherVehicle(pOtherVehicle)
, m_pOtherDriver(NULL)
, m_fDamageDone(fDamageDone)
, m_fCachedMaxCruiseSpeed(0.0f)
, m_fSpeedOfContinuousCollider(0.0f)
, m_bHasReacted(false)
, m_bIsContinuouslyColliding(false)
, m_bGotOutOfVehicle(false)
, m_bUseThreatResponse(false)
, m_vDamagePos(vDamagePos)
, m_vDamageDir(vDamageDir)
, m_bPreviousDisablePretendOccupantsCached(false)
{
	if(m_pOtherVehicle)
	{
		m_pOtherDriver = pOtherVehicle->GetDriver();
	}
	SetInternalTaskType(CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION);
}

CTaskCarReactToVehicleCollision::~CTaskCarReactToVehicleCollision()
{

}

bool CTaskCarReactToVehicleCollision::AreAbnormalExitsDisabledForSeat() const
{
	const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(GetPed());
	return (pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits());
}

bool CTaskCarReactToVehicleCollision::AreBothVehiclesStationary() const
{
	//Ensure my vehicle is valid.
	if(!m_pMyVehicle)
	{
		return false;
	}

	//Ensure the other vehicle is valid.
	if(!m_pOtherVehicle)
	{
		return false;
	}

	//Ensure my vehicle is stationary.
	if(!IsVehicleStationary(*m_pMyVehicle))
	{
		return false;
	}

	//Ensure the other vehicle is stationary.
	if(!IsVehicleStationary(*m_pOtherVehicle))
	{
		return false;
	}

	return true;
}

bool CTaskCarReactToVehicleCollision::CanUseThreatResponse() const
{
	return (!CTheScripts::GetPlayerIsOnAMission() && !CTheScripts::GetPlayerIsOnARandomEvent() &&
		(m_pOtherDriver && m_pOtherDriver->IsPlayer() && (m_pOtherDriver->GetPlayerWanted()->GetWantedLevel()== WANTED_CLEAN)) &&
		!AreAbnormalExitsDisabledForSeat() && !IsBusDriver());
}

bool CTaskCarReactToVehicleCollision::IsBusDriver() const
{
	return (GetPed()->GetIsDrivingVehicle() && CVehicle::IsBusModelId(GetPed()->GetVehiclePedInside()->GetModelId()));
}

bool CTaskCarReactToVehicleCollision::IsCollisionSignificant() const
{

	//Check if the car is rich.
	if(m_pMyVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_RICH_CAR) && !m_pMyVehicle->InheritsFromBoat())
	{
		return true;
	}

	//Check if the damage is significant.
	if(m_fDamageDone > sm_Tunables.m_MaxDamageToIgnore)
	{
		return true;
	}

	// Check if this is a boat experiencing a continuous collision by something fast or on top.
	if (m_pMyVehicle->InheritsFromBoat() && m_bIsContinuouslyColliding)
	{
		CVehicle* pOtherVehicle = m_pOtherVehicle;
		if (pOtherVehicle)
		{
			// Check for collisions moving somewhat fast.
			if (m_fSpeedOfContinuousCollider >= 3.0f)
			{
				return true;
			}

			// Check for being on top of the other boat.
			if (m_vDamageDir.z < -0.7f)
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskCarReactToVehicleCollision::IsFlippingOff() const
{
	return (CTaskAimGunVehicleDriveBy::IsFlippingSomeoneOff(*GetPed()));
}

bool CTaskCarReactToVehicleCollision::IsTalking() const
{
	const audSpeechAudioEntity* pSpeechAudioEntity = GetPed()->GetSpeechAudioEntity();
	return (pSpeechAudioEntity && pSpeechAudioEntity->IsAmbientSpeechPlaying());
}

bool CTaskCarReactToVehicleCollision::IsVehicleStationary(const CVehicle& rVehicle) const
{
	//Ensure the speed is valid.
	float fSpeedSq = rVehicle.GetVelocity().XYMag2();
	static dev_float s_fMaxSpeed = 2.0f;
	float fMaxSpeedSq = square(s_fMaxSpeed);
	if(fSpeedSq > fMaxSpeedSq)
	{
		return false;
	}

	return true;
}

void CTaskCarReactToVehicleCollision::ProcessGestures()
{
	//Ensure we are not flipping off.
	if(IsFlippingOff())
	{
		return;
	}

	//Allow gestures.
	GetPed()->SetGestureAnimsAllowed(true);
}

void CTaskCarReactToVehicleCollision::ProcessThreatResponse()
{
	//Ensure the flag is not set.
	if(m_bUseThreatResponse)
	{
		return;
	}

	//Ensure we have a vehicle - the rest of these checks start to get dodgy if we don't.
	if(!m_pMyVehicle)
	{
		return;
	}

	//Ensure we can use threat response.
	if(!CanUseThreatResponse())
	{
		return;
	}

	//Check for getting out of the car to fight.
	static dev_float s_fChances = 0.5f;
	if(GetPed()->CheckBraveryFlags(BF_PURSUE_WHEN_HIT_BY_CAR) &&
		(GetPed()->GetRandomNumberInRangeFromSeed(0.0f, 1.0f) < s_fChances) && AreBothVehiclesStationary())
	{
		m_bUseThreatResponse = true;
	}

	//Check if we have been continuously colliding for a while.
	static dev_u32 s_uMinTimeContinuouslyCollidingToConsiderThreat = 2500;
	u32 uTimeWeStartedCollidingWithPlayerVehicle = m_pMyVehicle->GetIntelligence()->GetTimeWeStartedCollidingWithPlayerVehicle();
	if((uTimeWeStartedCollidingWithPlayerVehicle > 0) &&
		((uTimeWeStartedCollidingWithPlayerVehicle + s_uMinTimeContinuouslyCollidingToConsiderThreat) < fwTimer::GetTimeInMilliseconds()))
	{
		m_bUseThreatResponse = true;
	}
}

s32 CTaskCarReactToVehicleCollision::GetDefaultStateAfterAbort() const
{
	//Check the state.
	s32 iState = GetState();
	switch(iState)
	{
		case State_flee:
		case State_fight:
		case State_threatResponseInVehicle:
		{
			return iState;
		}
		default:
		{
			return State_finish;
		}
	}
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::ProcessPreFSM()
{
	//Ensure the vehicle is valid - unless we are in threat response, in which case it is possible
	//for our vehicle to be deleted while we are running away.
	if(!m_pMyVehicle && GetState() != State_threatResponseInVehicle)
	{
		return FSM_Quit;
	}

	//If we should be dead we really shouldn't be doing anything
	const CPed& rPed = *GetPed();
	if(rPed.ShouldBeDead() && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		return FSM_Quit;
	}

	//Process the gestures.
	ProcessGestures();

	//Process threat response.
	ProcessThreatResponse();

	return FSM_Continue;
}

bool CTaskCarReactToVehicleCollision::ProcessMoveSignals()
{
	if (GetState() == State_threatResponseInVehicle
		&& m_pMyVehicle)
	{
		m_pMyVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
		return true;
	}
	else
	{
		return false;
	}
}

//local state machine
CTask::FSM_Return CTaskCarReactToVehicleCollision::UpdateFSM( s32 iState, CTask::FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_fight)
			FSM_OnEnter
				return Fight_OnEnter();
			FSM_OnUpdate
				return Fight_OnUpdate(pPed);

		FSM_State(State_taunt)
			FSM_OnEnter
			return TauntInVehicle_OnEnter();
		FSM_OnUpdate
			return TauntInVehicle_OnUpdate();

		FSM_State(State_gesture)
			FSM_OnUpdate
				return Gesture_OnUpdate();

		FSM_State(State_getOut)
			FSM_OnEnter
				return GetOut_OnEnter(pPed);
			FSM_OnUpdate
				return GetOut_OnUpdate();

		FSM_State(State_getBackInVehicle)
			FSM_OnEnter
				return GetBackInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return GetBackInVehicle_OnUpdate();

		FSM_State(State_flee)
			FSM_OnEnter
				return Flee_OnEnter(pPed);
			FSM_OnUpdate
				return Flee_OnUpdate();

		FSM_State(State_threatResponseInVehicle)
			FSM_OnEnter
				return ThreatResponseInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ThreatResponseInVehicle_OnUpdate();
			FSM_OnExit
				ThreatResponseInVehicle_OnExit();

		FSM_State(State_slowDown)
			FSM_OnUpdate
				return SlowDown_OnUpdate();
			FSM_OnExit
				SlowDown_OnExit();

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}


CTask::FSM_Return CTaskCarReactToVehicleCollision::Start_OnUpdate(CPed *pPed)
{
	//Check if the collision is significant.
	if(IsCollisionSignificant())
	{
		//Increase our anger and fear.
		//@@: location CTASKCARREACTTOVEHICLECOLLISION_START_ONUPDATE_INCREASE_MOTIVATIONS
		static dev_float s_fWithoutOrder	= 0.33f;
		static dev_float s_fWithOrder		= 0.1f;
		float fAmount = (!GetPed()->GetPedIntelligence()->GetOrder()) ?
s_fWithoutOrder : s_fWithOrder;
		//@@: location CTASKCARREACTTOVEHICLECOLLISION_START_ONUPDATE_SET_ANGRY_STATE
		GetPed()->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::ANGRY_STATE,	fAmount);
		//@@: location CTASKCARREACTTOVEHICLECOLLISION_START_ONUPDATE_SET_FEAR_STATE
		GetPed()->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::FEAR_STATE,	fAmount);
	}

	//Look at the other driver.
	if(m_pOtherDriver)
	{
		GetPed()->GetIkManager().LookAt(0, m_pOtherDriver,
			5000, BONETAG_HEAD, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
	}

	//initially decide which action to take:
	//1. Enter combat with the other ped
	//2. Get out of the vehicle and perform some further action (see CTaskCarReactToVehicleCollisionGetOut)
	//3. Drive away
	//4. Say some audio
	//5. Taunt
	//6. Do nothing

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || !m_pMyVehicle || !m_pOtherVehicle || m_pOtherVehicle == m_pMyVehicle || pPed->IsPlayer())
	{
		SetState(State_finish);
		return FSM_Continue;
	}

	// Process new action
	m_nAction = GetNewAction(pPed);
	switch(m_nAction)
	{
	case ACTION_STAY_IN_CAR_ANGRY:
		{
			if(m_pOtherDriver)
			{
				SetState(State_fight);
			}
			else
			{
				SetState(State_finish);
			}
			
		}
		break;

	case ACTION_GET_OUT:
	case ACTION_GET_OUT_ON_FIRE:
	case ACTION_GET_OUT_ANGRY:
	case ACTION_GET_OUT_INSPECT_DMG:
	case ACTION_GET_OUT_INSPECT_DMG_ANGRY:
		{
			SetState(State_getOut);
		}
		break;

	case ACTION_GET_OUT_STUMBLE:
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled, true);
			// CTaskDyingDead has a special state to deal with getting dead peds to fall out of a vehicle
			pPed->ForceDeath(true, m_pOtherVehicle, WEAPONTYPE_RAMMEDBYVEHICLE);
			if (pPed->GetMyVehicle())
			{
				pPed->GetMyVehicle()->KillPedsInVehicle(m_pOtherVehicle, WEAPONTYPE_RAMMEDBYVEHICLE);
			}
		}
		break;

	case ACTION_FLEE:
		{
			SetState(State_flee);
		}
		break;

	case ACTION_THREAT_RESPONSE_IN_VEHICLE:
		{
			SetState(State_threatResponseInVehicle);
		}
		break;
	
	case ACTION_TAUNT_IN_CAR:
		{
			SetState(State_taunt);
		}
		break;
	case ACTION_SLOW_DOWN:
		{
			SetState(State_slowDown);
		}
		break;

	case ACTION_STAY_IN_CAR:
	default:
		{
			//Check if we are talking.
			if(IsTalking())
			{
				SetState(State_gesture);
			}
			else
			{
				SetState(State_finish);
			}
		}
	}

	return FSM_Continue;
}



//*********************************
//Start a fight with the other ped
//*********************************
CTask::FSM_Return CTaskCarReactToVehicleCollision::Fight_OnEnter()
{

	taskAssert(m_pMyVehicle);
	taskAssert(m_pOtherDriver);

	if (m_pOtherDriver)
	{
		SetNewTask(rage_new CTaskThreatResponse(m_pOtherDriver));
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::Fight_OnUpdate(CPed *pPed)
{

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) ||!IsWithinCombatDistance(pPed, m_pOtherDriver))
	{	
		if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_getBackInVehicle);
		}
		else
		{
			SetState(State_finish);
		}
	}

	return FSM_Continue;
}



//******************************************
//Get out of the car and perform some action
//******************************************
CTask::FSM_Return CTaskCarReactToVehicleCollision::GetOut_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	taskAssert(m_pMyVehicle);
	taskAssert(m_pOtherVehicle);

	u8 nFlags = 0;
	switch(m_nAction)
	{
	case ACTION_GET_OUT_ON_FIRE:
		{
			nFlags |= CTaskCarReactToVehicleCollisionGetOut::FLAG_ON_FIRE;
		}
		break;

	case ACTION_GET_OUT_STUMBLE:
		{
			nFlags |= CTaskCarReactToVehicleCollisionGetOut::FLAG_COLLAPSE;
		}
		break;

	case ACTION_GET_OUT_ANGRY:
		{
			nFlags |= CTaskCarReactToVehicleCollisionGetOut::FLAG_IS_ANGRY;
		}
		break;

	case ACTION_GET_OUT_INSPECT_DMG:
		{
			nFlags |= CTaskCarReactToVehicleCollisionGetOut::FLAG_INSPECT_DMG;
		}
		break;

	case ACTION_GET_OUT_INSPECT_DMG_ANGRY:
		{
			nFlags |= CTaskCarReactToVehicleCollisionGetOut::FLAG_INSPECT_DMG | CTaskCarReactToVehicleCollisionGetOut::FLAG_IS_ANGRY;
		}
		break;

	default:
		{
		}
		break;
	}

	SetNewTask(rage_new CTaskCarReactToVehicleCollisionGetOut(m_pMyVehicle, m_pOtherVehicle, m_vDamagePos, m_fDamageDone, nFlags));

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::GetOut_OnUpdate()
{
	m_bGotOutOfVehicle |= !GetPed()->GetVehiclePedInside();

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we failed to get out of the vehicle.
		if(!m_bGotOutOfVehicle)
		{
			SetState(State_taunt);
		}
		else
		{
			SetState(State_finish);
		}
	}

	return FSM_Continue;
}

//************************
//Unarmed Driveby
//************************
CTask::FSM_Return CTaskCarReactToVehicleCollision::TauntInVehicle_OnEnter()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::TauntInVehicle_OnUpdate()
{
	//Check if we have not yet reacted.
	const float fTimeToReact = m_pMyVehicle->GetRandomNumberInRangeFromSeed(sm_Tunables.m_SlowDown.m_MinTimeToReact, sm_Tunables.m_SlowDown.m_MaxTimeToReact);
	if(!m_bHasReacted)
	{
		//Check if we have exceeded the time to react.
		if(GetTimeInState() > fTimeToReact)
		{
			//Set the flag.
			m_bHasReacted = true;

			if (!m_pMyVehicle)
			{
				return FSM_Continue;
			}

			if(m_pMyVehicle->GetVehicleModelInfo())
			{
				//remove the window to the side of the ped as the taunt will go through the window
				const CVehicleEntryPointInfo* pEntryPointInfo = m_pMyVehicle->GetVehicleModelInfo()->GetEntryPointInfo(0);

				if(pEntryPointInfo)
				{
					eHierarchyId window = pEntryPointInfo->GetWindowId();
					if (window != VEH_INVALID_ID)
					{
						m_pMyVehicle->RolldownWindow(window);
					}
				}
			}

			//Equip the unarmed weapon.
			if(GetPed()->GetWeaponManager())
			{
				GetPed()->GetWeaponManager()->EquipWeapon(GetPed()->GetDefaultUnarmedWeaponHash());
			}

			const CEntity* pTarget = m_pOtherVehicle;
			if(!pTarget)
			{
				pTarget = m_pOtherDriver;
				if(!pTarget)
				{
					return FSM_Continue;
				}
			}

			CAITarget target(pTarget);
			SetNewTask(rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Aim,0,&target));
			return FSM_Continue;
		}
		else
		{
			if (m_pMyVehicle)
			{
				m_pMyVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
			}
			return FSM_Continue;
		}
	}

	//Check if the task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_finish);
	}
	//Check if the task has timed out.
	else if(GetTimeInState() > 5.0f + fTimeToReact)
	{
		SetState(State_finish);
	}
	//Check if we should use threat response.
	else if(m_bUseThreatResponse)
	{
		SetState(State_threatResponseInVehicle);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::Gesture_OnUpdate()
{
	//Check if the ped is finished talking.
	if(!IsTalking())
	{
		SetState(State_finish);
	}
	//Check if we should use threat response.
	else if(m_bUseThreatResponse)
	{
		SetState(State_threatResponseInVehicle);
	}

	return FSM_Continue;
}

//**********************
//Flee in the vehicle
//**********************
CTask::FSM_Return CTaskCarReactToVehicleCollision::Flee_OnEnter(CPed *pPed)
{
	taskAssert(m_pMyVehicle);
	taskAssert(m_pOtherVehicle);

	sVehicleMissionParams params;
	params.SetTargetEntity(m_pOtherVehicle);
	params.m_iDrivingFlags = DMode_AvoidCars|DF_AvoidTargetCoors|DF_SteerAroundPeds;
	params.m_fCruiseSpeed = 40.0f;

	aiTask* pCarTask = NULL;
	if ( m_pMyVehicle->InheritsFromBoat() )
	{
		pCarTask = rage_new CTaskVehicleFleeBoat(params);
	}
	else
	{
		pCarTask = rage_new CTaskVehicleFlee(params);
	}
		
	SetNewTask(rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pCarTask ));

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::Flee_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		SetState(State_finish);
	}

	return FSM_Continue;
}




//*************************
//Get back into the vehicle
//*************************
CTask::FSM_Return CTaskCarReactToVehicleCollision::GetBackInVehicle_OnEnter(CPed * UNUSED_PARAM(pPed))
{

	taskAssert(m_pMyVehicle);

	// Return to the vehicle
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
	SetNewTask(rage_new CTaskEnterVehicle(m_pMyVehicle, SR_Specific, 0, vehicleFlags));

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::GetBackInVehicle_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		SetState(State_finish);
	}

	return FSM_Continue;
}


//**********************
//Combat in vehicle
//**********************
CTask::FSM_Return CTaskCarReactToVehicleCollision::ThreatResponseInVehicle_OnEnter(CPed *pPed)
{
	if (m_pMyVehicle)
	{
		CPed* pInflictor = m_pOtherDriver;
		if(pInflictor && !pPed->GetPedIntelligence()->IsFriendlyWith(*pInflictor))
		{
			RequestProcessMoveSignalCalls();

			m_bPreviousDisablePretendOccupantsCached = m_pMyVehicle->m_nVehicleFlags.bDisablePretendOccupants;

			//Disable pretend occupants.
			m_pMyVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;

			//Register the threat.
			pPed->GetPedIntelligence()->RegisterThreat(pInflictor, true);

			//Create the task.
			CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pInflictor);

			//Decide whether to fight or flee.
			if(GetPed()->CheckBraveryFlags(BF_PURSUE_WHEN_HIT_BY_CAR))
			{
				pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
			}
			else if(GetPed()->CheckBraveryFlags(BF_CAN_FLEE_WHEN_HIT_BY_CAR))
			{
				pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);
			}

			//Configure vehicle pursuit.
			if(AreBothVehiclesStationary())
			{
				pTask->GetConfigFlagsForVehiclePursuit().SetFlag(CTaskVehiclePersuit::CF_ExitImmediately);
			}

			//Configure the flee.
			pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_CanCallPolice);
			pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_DisableExitVehicle);
			pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_OnlyCallPoliceIfChased);

			//Start the task.
			SetNewTask(pTask);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::ThreatResponseInVehicle_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		SetState(State_finish);
		return FSM_Continue;
	}

	if ( m_pOtherDriver )
	{
		if (m_pOtherDriver->IsPlayer() && m_pOtherDriver->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN)
		{
			SetState(State_finish);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskCarReactToVehicleCollision::ThreatResponseInVehicle_OnExit()
{
	//Check if the vehicle is valid.
	if(m_pMyVehicle)
	{
		//Do not disable pretend occupants.
		m_pMyVehicle->m_nVehicleFlags.bDisablePretendOccupants = m_bPreviousDisablePretendOccupantsCached;
	}
}

CTask::FSM_Return CTaskCarReactToVehicleCollision::SlowDown_OnUpdate()
{
	//Check if we have not yet reacted.
	if(!m_bHasReacted)
	{
		//Check if we have exceeded the time to react.
		float fTimeToReact = m_pMyVehicle->GetRandomNumberInRangeFromSeed(sm_Tunables.m_SlowDown.m_MinTimeToReact, sm_Tunables.m_SlowDown.m_MaxTimeToReact);
		if(GetTimeInState() > fTimeToReact)
		{
			//Set the flag.
			m_bHasReacted = true;

			//Slow down the vehicle.
			CTaskVehicleMissionBase* pTask = m_pMyVehicle->GetIntelligence()->GetActiveTask();
			if(pTask)
			{
				m_fCachedMaxCruiseSpeed = pTask->GetMaxCruiseSpeed();
				pTask->SetMaxCruiseSpeed(sm_Tunables.m_SlowDown.m_MaxCruiseSpeed);
			}

			//Check if we should honk.
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom < sm_Tunables.m_SlowDown.m_ChancesToHonk)
			{
				//Generate the horn type hash.
				fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				const u32 uHornTypeHash = (fRandom < sm_Tunables.m_SlowDown.m_ChancesToHonkHeldDown) 
					? ATSTRINGHASH("HELDDOWN", 0x0839504cb) 
					: ATSTRINGHASH("AGGRESSIVE",0xC91D8B07);

				//Play the horn.
				m_pMyVehicle->PlayCarHorn(true, uHornTypeHash);
			}

			//Check if we should flip off.
			fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom < sm_Tunables.m_SlowDown.m_ChancesToFlipOff)
			{
				//Check if the weapon manager is valid.
				CPed* pPed = GetPed();
				if(pPed->GetWeaponManager() && m_pOtherVehicle)
				{
					//Check if the model info is valid.
					if(m_pMyVehicle->GetVehicleModelInfo())
					{
						//Check if the entry point info is valid.
						const CVehicleEntryPointInfo* pEntryPointInfo = m_pMyVehicle->GetVehicleModelInfo()->GetEntryPointInfo(0);
						if(pEntryPointInfo)
						{
							//Roll down the window.
							eHierarchyId window = pEntryPointInfo->GetWindowId();
							if (window != VEH_INVALID_ID)
							{
								m_pMyVehicle->RolldownWindow(window);
							}
						}
					}

					//Equip the unarmed weapon.
					pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());

					//Create the task.
					CAITarget target(m_pOtherVehicle);
					CTask* pTask = rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Aim, 0, &target);

					//Start the task.
					SetNewTask(pTask);
				}
			}
		}
	}

	//Check if we have exceeded the time.
	float fTime = m_pMyVehicle->GetRandomNumberInRangeFromSeed(sm_Tunables.m_SlowDown.m_MinTime, sm_Tunables.m_SlowDown.m_MaxTime);
	if(GetTimeInState() > fTime)
	{
		//Finish the task.
		SetState(State_finish);
	}
	//Check if we should use threat response.
	else if(m_bUseThreatResponse)
	{
		SetState(State_threatResponseInVehicle);
	}

	return FSM_Continue;
}

void CTaskCarReactToVehicleCollision::SlowDown_OnExit()
{
	//Check if the vehicle is valid.
	if(m_pMyVehicle)
	{
		//Check if we have reacted.
		if(m_bHasReacted)
		{
			//Restore the max cruise speed.
			CTaskVehicleMissionBase* pTask = m_pMyVehicle->GetIntelligence()->GetActiveTask();
			if(pTask)
			{
				pTask->SetMaxCruiseSpeed(m_fCachedMaxCruiseSpeed);
			}
		}
	}
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskCarReactToVehicleCollision::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_getOut",
		"State_flee",
		"State_taunt",
		"State_gesture",
		"State_fight",
		"State_getBackInVehicle",
		"State_threatResponseInVehicle",
		"State_slowDown",
		"State_finish",
	};

	return aStateNames[iState];
}
#endif //!__FINAL

CTaskCarReactToVehicleCollision::eActionToTake CTaskCarReactToVehicleCollision::GetNewAction(CPed* pPed)
{
#if __BANK
	TUNE_GROUP_BOOL(TASK_CAR_REACT_TO_VEHICLE_COLLISION, bForceThreatResponseInVehicle, false);
	if(bForceThreatResponseInVehicle)
	{
		return ACTION_THREAT_RESPONSE_IN_VEHICLE;
	}
#endif

	//Ensure the ped is in the vehicle.
	if(!pPed->GetIsInVehicle())
	{
		return ACTION_STAY_IN_CAR;
	}

	//Check if the collision is not significant.
	if(!IsCollisionSignificant())
	{
		// For boats, at least say something.
		if (m_bIsContinuouslyColliding && m_pMyVehicle && m_pMyVehicle->InheritsFromBoat())
		{
			pPed->NewSay("GENERIC_SHOCKED_MED");
		}

		//Check if we are continuously colliding, and driving.
		if(m_bIsContinuouslyColliding && pPed->GetIsDrivingVehicle())
		{
			return ACTION_SLOW_DOWN;
		}
		else
		{
			return ACTION_STAY_IN_CAR;
		}
	}

	//Check if we can use threat response.
	if(CanUseThreatResponse())
	{
		//Check if we are driving the vehicle, and the other driver is a player.
		if(pPed->GetIsDrivingVehicle() && m_pOtherDriver && m_pOtherDriver->IsPlayer())
		{
			//Check if we are angry or fearful.
			static dev_float s_fThreshold = 0.75f;
			if((pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE) >= s_fThreshold) ||
				(pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE) >= s_fThreshold))
			{
				return ACTION_THREAT_RESPONSE_IN_VEHICLE;
			}
		}
	}

	// Check the ped's door is able to open, we don't want to be kicking anyone out
	s32 nSeatRequest = m_pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	Assert(nSeatRequest != -1);

	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);		
	if (!m_pMyVehicle->InheritsFromBoat())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);
	}
	else
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	}

	//Check if we can exit the vehicle.
	bool bCanExitVehicle = (m_pOtherDriver && m_pOtherDriver->IsPlayer()) && (CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pMyVehicle, SR_Prefer, nSeatRequest, 0, false, vehicleFlags) > 0);

	static dev_float GET_OUT_STUMBLE_THRESHOLD = 60.0f;
	static dev_float GET_OUT_STUMBLE_CHANCE    = 0.067f;

	if(bCanExitVehicle && ( m_fDamageDone > GET_OUT_STUMBLE_THRESHOLD && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < GET_OUT_STUMBLE_CHANCE) )
	{
		bool bDisableFireReaction = false;
		if(NetworkInterface::IsGameInProgress())
		{
			bDisableFireReaction = true;
		}

		static dev_float GET_OUT_ON_FIRE_THRESHOLD			= 120.0f;
		static dev_float GET_OUT_ON_FIRE_CHANCE				= 0.5f;
		static dev_float GET_OUT_ON_FIRE_MIN_VEHICLE_SPEED	= rage::square(30.0f);
		static dev_float GET_OUT_ON_FIRE_REL_VELOCITY		= rage::square(15.0f);

		if(!bDisableFireReaction && 
			m_fDamageDone > GET_OUT_ON_FIRE_THRESHOLD && 
			fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < GET_OUT_ON_FIRE_CHANCE &&
			m_pOtherVehicle && m_pOtherVehicle->GetVelocity().Mag2() > GET_OUT_ON_FIRE_MIN_VEHICLE_SPEED && 
			(m_pMyVehicle->GetVelocity() - m_pOtherVehicle->GetVelocity()).Mag2() > GET_OUT_ON_FIRE_REL_VELOCITY)
		{
			return ACTION_GET_OUT_ON_FIRE;
		}
		else
		{
			return ACTION_GET_OUT_STUMBLE;
		}
	}
	if (bCanExitVehicle && (m_pMyVehicle->InheritsFromBoat() && !(m_pMyVehicle->IsDriver(pPed))) && !m_pMyVehicle->GetDriver())
	{
		return ACTION_GET_OUT;
	}

	if(m_pMyVehicle->IsDriver(pPed) && m_pOtherVehicle && m_pOtherVehicle->ContainsPlayer())
	{
		//If we are on the highway, or the car is rich, always slow down.
		if(m_pMyVehicle->GetIntelligence()->IsOnHighway() ||
			m_pMyVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_RICH_CAR))
		{
			return ACTION_SLOW_DOWN;
		}
		else 
		{
			//Check if we are talking.
			if(IsTalking())
			{
				//Check if the chances are valid.
				static dev_float s_fChances = 0.9f;
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChances)
				{
					return ACTION_STAY_IN_CAR;
				}
			}

			return ACTION_TAUNT_IN_CAR;
		}
	}

	return ACTION_STAY_IN_CAR;
}

//////////////////////////////////////////////////////////////////////////
// CTaskComplexCarReactToVehicleCollisionGetOut
//////////////////////////////////////////////////////////////////////////

CTaskCarReactToVehicleCollisionGetOut::CTaskCarReactToVehicleCollisionGetOut(CVehicle* pMyVehicle, CVehicle* pOtherVehicle, const Vector3& vDamagePos, float fDamageDone, u8 nFlags)
: m_pMyVehicle(pMyVehicle)
, m_pOtherVehicle(pOtherVehicle)
, m_pOtherDriver(NULL)
, m_vDamagePos(vDamagePos)
, m_fDamageDone(fDamageDone)
, m_nFlags(nFlags)
{
	if(m_pOtherVehicle)
	{
		m_pOtherDriver = pOtherVehicle->GetDriver();
	}
	SetInternalTaskType(CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION_GET_OUT);
}

CTaskCarReactToVehicleCollisionGetOut::~CTaskCarReactToVehicleCollisionGetOut()
{

}

//local state machine
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::UpdateFSM( s32 iState, CTask::FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	//If we should be dead we really shouldn't be doing anything
	if (pPed->ShouldBeDead() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		return FSM_Quit;
	}

	FSM_Begin
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_waitBeforeExitingVehicle)
			FSM_OnUpdate
				return WaitBeforeExitingVehicle_OnUpdate(pPed);
		
		FSM_State(State_exitVehicle)
			FSM_OnEnter
				return ExitVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ExitVehicle_OnUpdate(pPed);

		FSM_State(State_onFire)
			FSM_OnEnter
				return OnFire_OnEnter(pPed);
			FSM_OnUpdate
				return OnFire_OnUpdate();

		FSM_State(State_collapse)
			FSM_OnEnter
				return Collapse_OnEnter(pPed);
			FSM_OnUpdate
				return Collapse_OnUpdate();

		FSM_State(State_agitated)
			FSM_OnEnter
				return Agitated_OnEnter();
			FSM_OnUpdate
				return Agitated_OnUpdate();

		FSM_State(State_moveToImpactPoint)
			FSM_OnEnter
				return MoveToImpactPoint_OnEnter();
			FSM_OnUpdate
				return MoveToImpactPoint_OnUpdate(pPed);

		FSM_State(State_lookAtImpactPoint)
			FSM_OnEnter
				return LookAtImpactPoint_OnEnter(pPed);
			FSM_OnUpdate
				return LookAtImpactPoint_OnUpdate();

		FSM_State(State_enterVehicle)
			FSM_OnEnter
				return EnterVehicle_OnEnter();
			FSM_OnUpdate
				return EnterVehicle_OnUpdate();

		FSM_State(State_fight)
			FSM_OnEnter
				return Fight_OnEnter();
			FSM_OnUpdate
				return Fight_OnUpdate(pPed);

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

bool CTaskCarReactToVehicleCollisionGetOut::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent)
	{
		// Allow us to be damaged if we've not started to exit
		if (((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE && GetState() != State_exitVehicle)
		{
			return true;
		}

		if (((CEvent*)pEvent)->GetEventType() == EVENT_ON_FIRE && (m_nFlags & FLAG_ON_FIRE) && iPriority != ABORT_PRIORITY_IMMEDIATE && !((CEvent*)pEvent)->RequiresAbortForRagdoll())
		{
			return false;
		}	
	}

	return true;
}

bool CTaskCarReactToVehicleCollisionGetOut::ShouldWaitBeforeExitingVehicle() const
{
	//Ensure the other driver is valid.
	if(!m_pOtherDriver)
	{
		return false;
	}

	//Ensure the other vehicle is valid.
	if(!m_pOtherVehicle)
	{
		return false;
	}

	//Ensure the other driver is driving the other vehicle.
	if(!m_pOtherVehicle->IsDriver(m_pOtherDriver.Get()))
	{
		return false;
	}

	//Check if the other vehicle is moving towards us.
	if(m_pOtherVehicle->GetVelocity().XYMag2() > 1.0f)
	{
		Vector3 vToUs = (VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_pOtherVehicle->GetTransform().GetPosition()));
		if(m_pOtherVehicle->GetVelocity().Dot(vToUs) >= 0.0f)
		{
			return true;
		}
	}

	return false;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Start_OnUpdate(CPed* pPed)
{
	if(!m_pMyVehicle || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		SetState(State_finish);
	}
	else if(ShouldWaitBeforeExitingVehicle())
	{
		SetState(State_waitBeforeExitingVehicle);
	}
	else
	{
		SetState(State_exitVehicle);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::WaitBeforeExitingVehicle_OnUpdate(CPed* pPed)
{
	if(!m_pMyVehicle || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		SetState(State_finish);
	}
	else if(!ShouldWaitBeforeExitingVehicle())
	{
		SetState(State_exitVehicle);
	}

	return FSM_Continue;
}

//*****************************
// Start by exiting the vehicle
//*****************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::ExitVehicle_OnEnter(CPed* pPed)
{
	// Cache whether ped is the driver or not, will be used later to decide what VO to say
	// - cache it now, as once we have exited the car we will not know anymore
	if( m_pMyVehicle == NULL )
	{
		return FSM_Continue;
	}
	if(m_pMyVehicle->IsDriver(pPed))
	{
		m_nFlags |= FLAG_IS_DRIVER;
	}

	// Do we need to set the vehicle on fire?
	if((m_nFlags & FLAG_ON_FIRE) && !g_fireMan.IsEntityOnFire(m_pMyVehicle))
	{
		Vector3 vForward(0.0f, 1.0f, 0.0f);
		float fDot = vForward.Dot(m_vDamagePos);

		Vector3 vFirePos(VEC3V_TO_VECTOR3(m_pMyVehicle->GetTransform().GetPosition()));

		if(fDot > 0.0f || !m_pMyVehicle->GetVehicleDamage()->GetPetrolTankPosWld(&vFirePos))
		{
			// Get engine bone
			int nFireBoneIndex = m_pMyVehicle->GetBoneIndex(VEH_ENGINE);
			if(nFireBoneIndex > -1)
			{
				Matrix34 mFireBone;
				m_pMyVehicle->GetGlobalMtx(nFireBoneIndex, mFireBone);

				vFirePos = mFireBone.d;
			}
		}

		const float fBurnTime     = fwRandom::GetRandomNumberInRange(20.0f, 35.0f);
		const float fBurnStrength = fwRandom::GetRandomNumberInRange(0.75f, 1.0f);
		const float fPeakTime	  = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);

		// Start vehicle fire
		Vec3V vPosLcl = CVfxHelper::GetLocalEntityPos(*m_pMyVehicle, RCC_VEC3V(vFirePos));
		Vec3V vNormLcl = CVfxHelper::GetLocalEntityDir(*m_pMyVehicle, Vec3V(V_Z_AXIS_WZERO));

		g_fireMan.StartVehicleFire(m_pMyVehicle, -1, vPosLcl, vNormLcl, pPed, fBurnTime, fBurnStrength, fPeakTime, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);

		// Start ped fire
		g_fireMan.StartPedFire(pPed, pPed, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, 0);
	}

	VehicleEnterExitFlags nFlags;
	nFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	nFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);

	if(m_nFlags & FLAG_ON_FIRE)
	{
		nFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
		nFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToWalk);
	}
	else if(m_nFlags & FLAG_COLLAPSE)
	{
		nFlags.BitSet().Set(CVehicleEnterExitFlags::DeadFallOut);
	}

	if (m_pMyVehicle->InheritsFromBoat())
	{
		nFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	}

	SetNewTask(rage_new CTaskExitVehicle(m_pMyVehicle, nFlags));

	//static int SAY_DELAY = 1000;
	//pPed->NewSay(GetSayPhrase(m_pMyVehicle, (m_nFlags & FLAG_IS_DRIVER)), 0, false, false, SAY_DELAY);

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::ExitVehicle_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{	
		if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if((m_nFlags & FLAG_ON_FIRE) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
			{
				SetState(State_onFire);
			}
			else if((m_nFlags & FLAG_COLLAPSE) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
			{
				SetState(State_collapse);
			}
			else if(m_nFlags & FLAG_INSPECT_DMG)
			{
				SetState(State_moveToImpactPoint);
			}
			else if(m_pOtherDriver)
			{
				SetState(State_agitated);
			}
			else
			{
				SetState(State_finish);
			}
		}
		else
		{
			SetState(State_finish);
		}
	}
	return FSM_Continue;
}

//*****************************
//	 Set the ped on fire!
//*****************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::OnFire_OnEnter(CPed* pPed)
{
	SetNewTask(rage_new CTaskComplexOnFire(WEAPONTYPE_FIRE));

	g_fireMan.StartPedFire(pPed, pPed, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, 0);

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::OnFire_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_finish);
	}

	return FSM_Continue;
}
//*****************************
//	 Make the ped collapse
//*****************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Collapse_OnEnter(CPed* pPed)
{
	const int MIN_TIME = 1000;
	const int MAX_TIME = 10000;

	Assert(pPed->GetRagdollInst());
	phBoundComposite *bounds = pPed->GetRagdollInst()->GetTypePhysics()->GetCompositeBounds();
	Assert(bounds);
	int nComponent = fwRandom::GetRandomNumberInRange(1, bounds->GetNumBounds()-1);
 
	Matrix34 matComponent = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	// get component position directly from the ragdoll bounds
	pPed->GetRagdollComponentMatrix(matComponent, nComponent);

	const float TEST_SHOT_HIT_FORCE_MULTIPLIER = -30.0f; 

	Vector3 vHitDir   = -VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());
	Vector3 vHitForce = vHitDir * TEST_SHOT_HIT_FORCE_MULTIPLIER;

	nmEntityDebugf(pPed, "CTaskCarReactToVehicleCollisionGetOut::Collapse - starting nm shot task");
	CTaskNMShot* pTaskNM = rage_new CTaskNMShot(pPed, MIN_TIME, MAX_TIME, NULL, WEAPONTYPE_PISTOL, nComponent, matComponent.d, vHitForce, vHitDir);

	SetNewTask(rage_new CTaskNMControl(2000, 30000, pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM));

	// Reduce the peds health to make them collapse
	pPed->SetHealth(Min(pPed->GetHealth(), HEALTH_AFTER_LARGE_COLLISION));

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Collapse_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		if(m_pMyVehicle && !GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_enterVehicle);
		}
		else
		{
			SetState(State_finish);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Agitated_OnEnter()
{
	//Assert that we are not agitated with ourselves.
	aiAssert(GetPed() != m_pOtherDriver);

	//Create the task.
	CTaskAgitated* pTask = rage_new CTaskAgitated(m_pOtherDriver);
	
	//Apply the agitation.
	pTask->ApplyAgitation(AT_BumpedInVehicle);
	
	//Start the task.
	SetNewTask(pTask);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Agitated_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Enter the vehicle.
		SetState(State_enterVehicle);
	}
	
	return FSM_Continue;
}

//****************************
// Move to the point of impact
//****************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::MoveToImpactPoint_OnEnter()
{
	 //Convert m_vDamagePos to world space
	Vector3 vWorldDamagePos(m_vDamagePos);
	vWorldDamagePos = m_pMyVehicle->TransformIntoWorldSpace(vWorldDamagePos);

	Vector3 vTargetPos = vWorldDamagePos;
	vTargetPos -= VEC3V_TO_VECTOR3(m_pMyVehicle->GetTransform().GetPosition());
	vTargetPos.NormalizeFast();
	vTargetPos += vWorldDamagePos;

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, vTargetPos, 2.0f);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask/*, rage_new CTaskSimplePlayRandomAmbients()*/));

	return FSM_Continue;
}
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::MoveToImpactPoint_OnUpdate(CPed* pPed)
{
	//check if the route is too long
	aiTask* pNewSubTask = GetSubTask();
	bool bRouteIsTooLong = false;

	if(pNewSubTask && pNewSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pMoveTask = static_cast<CTaskComplexControlMovement*>(pNewSubTask);
		if(pMoveTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask->GetRunningMovementTask(pPed));
			if(pNavMeshTask)
			{
				if(pNavMeshTask->IsFollowingNavMeshRoute())
				{
					static dev_float MAX_ROUTE_LENGTH = 5.0f;
					if(pNavMeshTask->GetTotalRouteLength() > MAX_ROUTE_LENGTH)
					{
						// Quit the navmesh task and move to the next state
						bRouteIsTooLong=true;
					}
				}
			}
		}
	}

	//check if the move is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bRouteIsTooLong)
	{	
		if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_lookAtImpactPoint);
		}
		else
		{
			SetState(State_finish);
		}
	}

	

	return FSM_Continue;
}
//****************************************
//Have the ped look at the point of impact
//****************************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::LookAtImpactPoint_OnEnter(CPed* pPed)
{
	// Convert m_vDamagePos to world space
	Vector3 vWorldDamagePos(m_vDamagePos);
	vWorldDamagePos = m_pMyVehicle->TransformIntoWorldSpace(vWorldDamagePos);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(vWorldDamagePos.x, vWorldDamagePos.y, vPedPosition.x, vPedPosition.y);

	CTaskMoveAchieveHeading* pMoveTask = rage_new CTaskMoveAchieveHeading(fTargetHeading);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask/*, rage_new CTaskSimplePlayRandomAmbients()*/));

	return FSM_Continue;
}
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::LookAtImpactPoint_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		if(m_pMyVehicle && !GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_enterVehicle);
		}
		else
		{
			SetState(State_finish);
		}
	}
	return FSM_Continue;
}

//****************************************
//Re-enter the vehicle
//****************************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::EnterVehicle_OnEnter()
{
	// Return to the vehicle
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
	SetNewTask(rage_new CTaskEnterVehicle(m_pMyVehicle, SR_Specific, 0, vehicleFlags));

	return FSM_Continue;
}

CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::EnterVehicle_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{	
		SetState(State_finish);
	}
	return FSM_Continue;
}
//****************************************
//Fight the offending ped
//****************************************
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Fight_OnEnter()
{
	SetNewTask(rage_new CTaskThreatResponse(m_pOtherDriver));

	return FSM_Continue;
}
CTask::FSM_Return CTaskCarReactToVehicleCollisionGetOut::Fight_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished)|| !IsWithinCombatDistance(pPed, m_pOtherDriver))
	{	
		if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_enterVehicle);
		}
		else
		{
			SetState(State_finish);
		}
	}
	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskCarReactToVehicleCollisionGetOut::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_waitBeforeExitingVehicle",
		"State_exitVehicle",
		"State_onFire",
		"State_collapse",
		"State_agitated",
		"State_moveToImpactPoint",
		"State_lookAtImpactPoint",
		"State_fight",
		"State_enterVehicle",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL
//
//#if !__FINAL
//void CTaskComplexCarReactToVehicleCollisionGetOut::Debug(const CPed* pPed) const
//{
//#if DEBUG_DRAW
//	if((m_nFlags & FLAG_INSPECT_DMG) && m_pMyVehicle)
//	{
//		// Convert m_vDamagePos to world space
//		Vector3 vWorldDamagePos(m_vDamagePos);
//		vWorldDamagePos = m_pMyVehicle->TransformIntoWorldSpace(vWorldDamagePos);
//
//		grcDebugDraw::Sphere(vWorldDamagePos, 0.2f, Color_red1, false);
//
//		Vector3 vTargetPos = vWorldDamagePos;
//		vTargetPos -= m_pMyVehicle->GetPosition();
//		vTargetPos.NormalizeFast();
//		vTargetPos += vWorldDamagePos;
//
//		grcDebugDraw::Sphere(vTargetPos, 0.2f, Color_yellow, false);
//	}
//#endif
//
//	if(GetSubTask())
//	{
//		GetSubTask()->Debug();
//	}
//}
//#endif
//
//aiTask* CTaskComplexCarReactToVehicleCollisionGetOut::CreateNextSubTask(CPed* pPed)
//{
//	aiTask* pNewSubTask = NULL;
//
//	switch(m_state)
//	{
//	case STATE_EXIT_VEHICLE:
//		{
//			if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
//			{
//				if((m_nFlags & FLAG_ON_FIRE) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
//				{
//					pNewSubTask = CreateSubTask(CTaskTypes::TASK_NM_ONFIRE, pPed);
//
//					m_state = STATE_ON_FIRE;
//				}
//				else if((m_nFlags & FLAG_COLLAPSE) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
//				{
//					pNewSubTask = CreateSubTask(CTaskTypes::TASK_NM_SHOT, pPed);
//
//					m_state = STATE_COLLAPSE;
//				}
//				else if(m_nFlags & FLAG_INSPECT_DMG)
//				{
//					// Convert m_vDamagePos to world space
//					Vector3 vWorldDamagePos(m_vDamagePos);
//					vWorldDamagePos = m_pMyVehicle->TransformIntoWorldSpace(vWorldDamagePos);
//
//					Vector3 vTargetPos = vWorldDamagePos;
//					vTargetPos -= m_pMyVehicle->GetPosition();
//					vTargetPos.NormalizeFast();
//					vTargetPos += vWorldDamagePos;
//
//					pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed, vTargetPos);
//
//					m_state = STATE_MOVE_TO_IMPACT_POINT;
//				}
//				else if(m_pOtherDriver)
//				{
//					if(m_nFlags & FLAG_IS_ANGRY)
//					{
//						// Go straight into combat
//						pNewSubTask = CreateSubTask(CTaskTypes::TASK_THREAT_RESPONSE, pPed);
//
//						m_state = STATE_COMBAT;
//					}
//					else
//					{
//						pNewSubTask	= CreateSubTask(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING, pPed, m_pOtherDriver->GetPosition());
//
//						m_state = STATE_ORIENTATE;
//					}
//				}
//			}
//
//			if(!pNewSubTask)
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//				// We shouldn't still be in a vehicle at this point - task must have failed, so quit
//				m_state = STATE_FINISHED;
//			}
//		}
//		break;
//
//	case STATE_ON_FIRE:
//	case STATE_COLLAPSE:
//	case STATE_ORIENTATE:
//		{
//			pNewSubTask = CreateSubTask(CTaskTypes::TASK_SAY_AUDIO, pPed);
//
//			m_state = STATE_SAY_AUDIO;
//		}
//		break;
//
//	case STATE_MOVE_TO_IMPACT_POINT:
//		{
//			if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
//			{
//				// Convert m_vDamagePos to world space
//				Vector3 vWorldDamagePos(m_vDamagePos);
//				vWorldDamagePos = m_pMyVehicle->TransformIntoWorldSpace(vWorldDamagePos);
//
//				pNewSubTask	= CreateSubTask(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING, pPed, vWorldDamagePos);
//
//				m_state = STATE_LOOK_AT_IMPACT_POINT;
//			}
//			else
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//				m_state = STATE_FINISHED;
//			}
//		}
//		break;
//
//	case STATE_LOOK_AT_IMPACT_POINT:
//		{
//			pNewSubTask = CreateSubTask(CTaskTypes::TASK_SAY_AUDIO, pPed);
//
//			m_state = STATE_SAY_AUDIO;
//		}
//		break;
//
//	case STATE_SAY_AUDIO:
//		{
//			if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_ENTER_VEHICLE, pPed);
//
//				m_state = STATE_ENTER_VEHICLE;
//			}
//			else
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//				m_state = STATE_FINISHED;
//			}
//		}
//		break;
//			
//	case STATE_COMBAT:
//		{
//			if(m_pMyVehicle && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_ENTER_VEHICLE, pPed);
//
//				m_state = STATE_ENTER_VEHICLE;
//			}
//			else
//			{
//				pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//				m_state = STATE_FINISHED;
//			}
//		}
//		break;
//			
//	case STATE_ENTER_VEHICLE:
//		{
//			pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//			m_state = STATE_FINISHED;
//		}
//		break;
//
//	default:
//		{
//			taskAssertf(false, "Unhandled state [%d]", m_state);
//		}
//		break;
//	}
//
//	return pNewSubTask;
//}
//
//aiTask* CTaskComplexCarReactToVehicleCollisionGetOut::CreateFirstSubTask(CPed* pPed)
//{
//	aiTask* pNewSubTask = NULL;
//
//	if(m_pMyVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
//	{
//		// Cache whether ped is the driver or not, will be used later to decide what VO to say
//		// - cache it now, as once we have exited the car we will not know anymore
//		if(m_pMyVehicle->IsDriver(pPed))
//		{
//			m_nFlags |= FLAG_IS_DRIVER;
//		}
//
//		// Do we need to set the vehicle on fire?
//		if((m_nFlags & FLAG_ON_FIRE) && !g_fireMan.IsEntityOnFire(m_pMyVehicle))
//		{
//			Vector3 vForward(0.0f, 1.0f, 0.0f);
//			float fDot = vForward.Dot(m_vDamagePos);
//			
//			Vector3 vFirePos(m_pMyVehicle->GetPosition());
//
//			if(fDot > 0.0f || !m_pMyVehicle->GetVehicleDamage()->GetPetrolTankPosWld(vFirePos))
//			{
//				// Get engine bone
//				int nFireBoneIndex = m_pMyVehicle->GetBoneIndex(VEH_ENGINE);
//				if(nFireBoneIndex > -1)
//				{
//					vFirePos = m_pMyVehicle->GetGlobalMtx(nFireBoneIndex).d;
//				}
//			}
//
//			const float fBurnTime     = fwRandom::GetRandomNumberInRange(20.0f, 35.0f);
//			const float fBurnStrength = fwRandom::GetRandomNumberInRange(0.75f, 1.0f);
//
//			// Start vehicle fire
//			g_fireMan.StartEntityFire(m_pMyVehicle, RCC_VEC3F(vFirePos), Vec3f(0.0f, 0.0f, 1.0f), PGTAMATERIALMGR->g_idCarMetal, NULL, FIRETYPE_GENERIC, false, 2, fBurnTime, fBurnStrength);
//
//			// Start ped fire
//			g_fireMan.StartEntityFire(pPed, RCC_VEC3F(pPed->GetPosition()), Vec3f(0.0f, 0.0f, 1.0f), PGTAMATERIALMGR->g_idButtocks, m_pMyVehicle);
//		}
//
//		pNewSubTask = CreateSubTask(CTaskTypes::TASK_EXIT_VEHICLE, pPed);
//
//		m_state = STATE_EXIT_VEHICLE;
//	}
//	else
//	{
//		pNewSubTask = CreateSubTask(CTaskTypes::TASK_FINISHED, pPed);
//
//		m_state = STATE_FINISHED;
//	}
//
//	return pNewSubTask;
//}
//
//aiTask* CTaskComplexCarReactToVehicleCollisionGetOut::ControlSubTask(CPed* pPed)
//{
//	aiTask* pNewSubTask = GetSubTask();
//
//	switch(m_state)
//	{
//	case STATE_MOVE_TO_IMPACT_POINT:
//		{
//			if(pNewSubTask && pNewSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
//			{
//				CTaskComplexControlMovement* pMoveTask = static_cast<CTaskComplexControlMovement*>(pNewSubTask);
//				if(pMoveTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
//				{
//					CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask->GetRunningMovementTask(pPed));
//					if(pNavMeshTask)
//					{
//						if(pNavMeshTask->IsFollowingNavMeshRoute())
//						{
//							static dev_float MAX_ROUTE_LENGTH = 5.0f;
//							if(pNavMeshTask->GetTotalRouteLength() > MAX_ROUTE_LENGTH)
//							{
//								// Quit the navmesh task and move to the next state
//								pNewSubTask = CreateNextSubTask(pPed);
//							}
//						}
//					}
//				}
//			}
//		}
//		break;
//
//	case STATE_COMBAT:
//		{
//			if(!IsWithinCombatDistance(pPed, m_pOtherDriver))
//			{
//				pNewSubTask = CreateNextSubTask(pPed);
//			}
//		}
//		break;
//
//	default:
//		{
//		}
//		break;
//	}
//
//	return pNewSubTask;
//}
//
//aiTask* CTaskComplexCarReactToVehicleCollisionGetOut::CreateSubTask(const int iTaskType, CPed* pPed)
//{
//	aiTask* pNewSubTask = NULL;
//
//	switch(iTaskType)
//	{
//	case CTaskTypes::TASK_EXIT_VEHICLE:
//		{
//			int nFlags = RF_DontCloseDoor | RF_QuitIfAllDoorsBlocked;
//			if(m_nFlags & FLAG_IS_ANGRY)
//			{
//				nFlags &= ~RF_DontCloseDoor;
//				//nFlags |=  RF_CloseDoorAngrily;
//			}
//			else if(m_nFlags & FLAG_INSPECT_DMG)
//			{
//				nFlags &= ~RF_DontCloseDoor;
//			}
//			else if(m_nFlags & FLAG_ON_FIRE)
//			{
//				nFlags |= RF_DontWaitForCarToStop;
//			}
//
//			pNewSubTask = rage_new CTaskExitVehicle(m_pMyVehicle, nFlags);
//
//			//static int SAY_DELAY = 1000;
//			//pPed->NewSay(GetSayPhrase(m_pMyVehicle, (m_nFlags & FLAG_IS_DRIVER)), 0, false, false, SAY_DELAY);
//		}
//		break;
//
//	case CTaskTypes::TASK_ENTER_VEHICLE:
//		{
//			pNewSubTask = rage_new CTaskEnterVehicle(m_pMyVehicle, Seat_AsDriver, CTaskEnterVehicle::State_Finish, RF_WarpAfterTime);
//		}
//		break;
//
//	case CTaskTypes::TASK_SAY_AUDIO:
//		{
//			pNewSubTask = rage_new CTaskSayAudio(GetSayPhrase(m_pMyVehicle, (m_nFlags & FLAG_IS_DRIVER)), 0.0f, 1.0f, m_pOtherDriver);
//		}
//		break;
//
//	case CTaskTypes::TASK_THREAT_RESPONSE:
//		{
//			pNewSubTask=CTaskThreatResponse::CreateCombatTask(m_pOtherDriver, false, false);
//			static int SAY_DELAY = 500;
//			pPed->NewSay(GetSayPhrase(m_pMyVehicle, (m_nFlags & FLAG_IS_DRIVER)), 0, true, false, SAY_DELAY);
//		}
//		break;
//
//	case CTaskTypes::TASK_NM_ONFIRE:
//		{
//			CTaskNMOnFire* pTaskNM = rage_new CTaskNMOnFire(1000, 30000, 0.3f);
//
//			pNewSubTask = rage_new CTaskNMControl(2000, 30000, pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM);
//
//			g_fireMan.StartEntityFire(pPed, RCC_VEC3F(pPed->GetPosition()), Vec3f(0.0f, 0.0f, 1.0f), PGTAMATERIALMGR->g_idButtocks, m_pMyVehicle);
//		}
//		break;
//
//	case CTaskTypes::TASK_NM_SHOT:
//		{
//			const int MIN_TIME = 1000;
//			const int MAX_TIME = 10000;
//
//			Assert(pPed->GetRagdollInst());
			//phBoundComposite *bounds = pPed->GetRagdollInst()->GetTypePhysics()->GetCompositeBounds();
			//Assert(bounds);
			//int nComponent = fwRandom::GetRandomNumberInRange(1, bounds->GetNumBounds()-1);
//
//			Matrix34 matComponent = pPed->GetMatrix();
//			// get component position directly from the ragdoll bounds
//			pPed->GetRagdollComponentMatrix(matComponent, nComponent);
//
//			const float TEST_SHOT_HIT_FORCE_MULTIPLIER = -30.0f; 
//
//			Vector3 vHitDir   = -pPed->GetA();
//			Vector3 vHitForce = vHitDir * TEST_SHOT_HIT_FORCE_MULTIPLIER;
//
//			CTaskNMShot* pTaskNM = rage_new CTaskNMShot(pPed, MIN_TIME, MAX_TIME, NULL, WEAPONTYPE_PISTOL, nComponent, matComponent.d, vHitForce, vHitDir);
//
//			pNewSubTask = rage_new CTaskNMControl(2000, 30000, pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM);
//
//			// Reduce the peds health to make them collapse
//			pPed->SetHealth(Min(pPed->GetHealth(), HEALTH_AFTER_LARGE_COLLISION));
//		}
//		break;
//
//	case CTaskTypes::TASK_FINISHED:
//		{
//			pNewSubTask = NULL;
//		}
//		break;
//
//	default:
//		{
//			taskAssertf(false, "Invalid task type [%s]", CTaskNames::GetTaskName(iTaskType));
//		}
//		break;
//	}
//
//	return pNewSubTask;
//}
//
//aiTask* CTaskComplexCarReactToVehicleCollisionGetOut::CreateSubTask(const int iTaskType, CPed* pPed, const Vector3& vTargetPos)
//{
//	aiTask* pNewSubTask = NULL;
//
//	switch(iTaskType)
//	{
//	case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
//		{
//			float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, pPed->GetPosition().x, pPed->GetPosition().y);
//
//			aiTask* pMoveTask = rage_new CTaskMoveAchieveHeading(fTargetHeading);
//			pNewSubTask	= rage_new CTaskComplexControlMovement(pMoveTask/*, rage_new CTaskSimplePlayRandomAmbients()*/);
//		}
//		break;
//
//	case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
//		{
//			aiTask* pMoveTask = rage_new CTaskMoveFollowNavMesh(PEDMOVE_RUN, vTargetPos, 2.0f);
//			pNewSubTask = rage_new CTaskComplexControlMovement(pMoveTask/*, rage_new CTaskSimplePlayRandomAmbients()*/);
//		}
//		break;
//
//	default:
//		{
//			taskAssertf(false, "Invalid task type [%s]", CTaskNames::GetTaskName(iTaskType));
//		}
//		break;
//	}
//
//	return pNewSubTask;
//}
