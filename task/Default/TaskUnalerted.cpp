//
// task/Default/TaskUnalerted.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/Default/TaskUnalerted.h"
#include "task/Default/AmbientAnimationManager.h"
#include "task/Default/TaskAmbient.h"
#include "task/Default/TaskSwimmingWander.h"
#include "task/Default/TaskWander.h"
#include "task/Motion/Locomotion/TaskFishLocomotion.h"
#include "task/Response/TaskFlee.h"
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#include "task/Scenario/info/ScenarioInfoConditions.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "task/Scenario/Types/TaskVehicleScenario.h"
#include "task/Scenario/Types/TaskWanderingScenario.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "task/Weapons/TaskSwapWeapon.h"
#include "script/script_cars_and_peds.h"
#include "streaming/populationstreaming.h"
#include "Event/System/EventData.h"
#include "ai/BlockingBounds.h"
#include "fwnet/netplayer.h"
#include "Peds/ped.h"
#include "Peds/NavCapabilities.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "scene/world/gameWorld.h"
#include "system/nelem.h"
#include "vehicleAi/driverpersonality.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehicleThreePointTurn.h"	//for ::IsPointOnRoad
#include "vehicles/Heli.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/inventory/PedWeaponManager.h"

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------

#define MPH_TO_MPS (0.44704f)	// ~1609/(60*60)
static float s_NavSpeeds[] =
{
	5.0f*MPH_TO_MPS,
	10.0f*MPH_TO_MPS,
	15.0f*MPH_TO_MPS,
	25.0f*MPH_TO_MPS,
	35.0f*MPH_TO_MPS,
	45.0f*MPH_TO_MPS,
	55.0f*MPH_TO_MPS,
	65.0f*MPH_TO_MPS,
	80.0f*MPH_TO_MPS,
	100.0f*MPH_TO_MPS,
	125.0f*MPH_TO_MPS,
	150.0f*MPH_TO_MPS,
	200.0f*MPH_TO_MPS,
	15.0f*MPH_TO_MPS,	// Doesn't really apply to vehicles.
	15.0f*MPH_TO_MPS,	// Doesn't really apply to vehicles.
	15.0f*MPH_TO_MPS	// Doesn't really apply to vehicles.
};
CompileTimeAssert(NELEM(s_NavSpeeds) == CScenarioChainingEdge::kNumNavSpeeds);
#undef MPH_TO_MPS

CTaskUnalerted::Tunables CTaskUnalerted::sm_Tunables;

IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskUnalerted, 0xd58ec357);

const u32 CTaskUnalerted::sm_aSafeForResumeScenarioHashes[POST_HANGOUT_SCENARIOS_SIZE] =
	{ 
		// If the ped is in an interior, we currently hardcode a transition to the WORLD_HUMAN_STAND_MOBILE scenario 
		// in SearchingForNextScenarioInChain_OnUpdate(). If this list increases in size, the logic there will need to be updated. [8/29/2012 mdawe]
		ATSTRINGHASH("WORLD_HUMAN_STAND_MOBILE", 0x0e574929a),
		ATSTRINGHASH("WORLD_HUMAN_SMOKING", 0x0cd02f9c4)
	};

CTaskUnalerted::CTaskUnalerted(CTask* pScenarioTask, CScenarioPoint* pScenarioPoint, int scenarioTypeReal)
		: m_pScenarioTask(pScenarioTask)
		, m_LastUsedPoint(NULL)
		, m_LastUsedPointRealTypeIndex(-1)
		, m_PointToReturnTo(NULL)
		, m_PointToReturnToRealTypeIndex(-1)
		, m_PointToSearchFrom(NULL)
		, m_PointToSearchFromRealTypeIndex(-1)
		, m_fTimeSinceLastSearchForNextScenarioInChain(0.0f)
		, m_WanderScenarioToUseNext(-1)
		, m_LastUsedPointTimeMs(0)
		, m_TimeUntilCheckScenarioMs(0)
		, m_SPC_UseInfo(NULL)
		, m_PavementHelper()
		, m_fTimeUntilDriverAnimCheckS(sm_Tunables.m_TimeBeforeDriverAnimCheck)
		, m_FrameCountdownAfterAbortAttempt(0)
		, m_uRunningFlags(0)
		, m_bCreateProp(false)
		, m_bDontReturnToVehicle(false)
		, m_bHasScenarioChainUser(false)
		, m_bRoamMode(false)
		, m_bKeepMovingWhenFindingWanderPath(false)
		, m_bWanderAwayFromThePlayer(false)
		, m_bDoCowardScenarioResume(false)
		, m_bScenarioReEntry(false)
		, m_nFrameWanderDelay(0)
		, m_bScenarioCowering(false)
		, m_bNeverReturnToLastPoint(false)
		, m_bDontReturnToVehicleAtMigrate(false)
{
	m_ScenarioFinder.SetScenarioPoint(pScenarioPoint, scenarioTypeReal);

	taskAssertf(NetworkInterface::IsGameInProgress()
			|| !pScenarioPoint
			|| pScenarioPoint->IsRealScenarioTypeValidForThis(scenarioTypeReal)
			|| IsHashPostHangoutScenario(SCENARIOINFOMGR.GetHashForScenario(scenarioTypeReal))		// We could get into valid situations where we are resuming usage of a point as "post-hangout" scenario: WORLD_HUMAN_STAND_MOBILE or WORLD_HUMAN_SMOKING, don't fail the assert for that.
			, "Scenario type %d (%s) is not a real scenario type appropriate for this point (type %d, %s)."
			, scenarioTypeReal
			, SCENARIOINFOMGR.GetNameForScenario(scenarioTypeReal)
			, pScenarioPoint->GetScenarioTypeVirtualOrReal()
			, SCENARIOINFOMGR.GetNameForScenario(pScenarioPoint->GetScenarioTypeVirtualOrReal()));

	SetInternalTaskType(CTaskTypes::TASK_UNALERTED);
}


CTaskUnalerted::~CTaskUnalerted()
{
	if(m_pScenarioTask)
	{
		delete m_pScenarioTask;
	}
	m_SPC_UseInfo = NULL;
}


aiTask* CTaskUnalerted::Copy() const
{
	// I think we need to do this to be proper, create a copy of the task
	// (CTaskRoam, which is similar, doesn't seem to do it, though).
	CTask* pCopiedScenarioTask = NULL;
	if(m_pScenarioTask)
	{
		pCopiedScenarioTask = static_cast<CTask*>(m_pScenarioTask->Copy());
	}

	CTaskUnalerted* pNewTask = rage_new CTaskUnalerted(pCopiedScenarioTask, m_ScenarioFinder.GetScenarioPoint(), m_ScenarioFinder.GetRealScenarioPointType());
	pNewTask->m_bCreateProp = m_bCreateProp;
	pNewTask->m_bRoamMode = m_bRoamMode;
	pNewTask->m_bNeverReturnToLastPoint = m_bNeverReturnToLastPoint;

	return pNewTask;
}


CScenarioPoint* CTaskUnalerted::GetScenarioPoint() const
{
	// This is currently needed for the "Spawn Ped To Use Point" and
	// "Spawn Ped At Point" buttons to destroy the previous user before
	// spawning a new ped.
	// Also, it's used by CPed::GetScenarioPoint(), important for preventing
	// too many peds using the same scenario near each other.

	CScenarioPoint* pt = NULL;

	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		pt = pSubTask->GetScenarioPoint();

		// if we don't have a point and our subtask is a CTaskWanderingScenario
		if(!pt && pSubTask->GetTaskType() == CTaskTypes::TASK_WANDERING_SCENARIO)
		{
			// use our subtask's m_CurrentScenarioPoint
			pt = static_cast<const CTaskWanderingScenario*>(pSubTask)->GetCurrentScenarioPoint();
		}
	}

	if(!pt)
	{
		pt = m_ScenarioFinder.GetScenarioPoint();
	}

	return pt;
}


int CTaskUnalerted::GetScenarioPointType() const
{
	int type = -1;
	CScenarioPoint* pt = NULL;
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		pt = pSubTask->GetScenarioPoint();
		if(pt)
		{
			type = pSubTask->GetScenarioPointType();
		}

		// Basically match the behavior of GetScenarioPoint() in the case of the subtask being
		// CTaskWanderingScenario.
		if(!pt && pSubTask->GetTaskType() == CTaskTypes::TASK_WANDERING_SCENARIO)
		{
			pt = static_cast<const CTaskWanderingScenario*>(pSubTask)->GetCurrentScenarioPoint();
			if(pt)
			{
				type = static_cast<const CTaskWanderingScenario*>(pSubTask)->GetCurrentScenarioType();
			}
		}
	}

	if(!pt)
	{
		type = m_ScenarioFinder.GetRealScenarioPointType();
	}

	return type;
}

bool CTaskUnalerted::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	if (!isValid)
	{
		isValid = CTask::IsValidForMotionTask(task);
	}

	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}
	return isValid;
}


#if !__FINAL

void CTaskUnalerted::Debug() const
{
	CTask::Debug();
#if DEBUG_DRAW
	const CPed* pPed = GetPed();
	if (pPed && pPed->GetRemoveAsSoonAsPossible())
	{
		Vector3 vDebugPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vDebugPosition.z += 1.0f;
		grcDebugDraw::Sphere(vDebugPosition, 0.15f, Color_RosyBrown);
	}
#endif
}


const char * CTaskUnalerted::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Start",
		"Wandering",
		"SearchingForScenario",
		"SearchingForNextScenarioInChain",
		"UsingScenario",
		"InVehicleAsDriver",
		"InVehicleAsPassenger",
		"ResumeAfterAbort",
		"HolsteringWeapon",
		"ExitingVehicle",
		"ReturningToVehicle",
		"WaitForVehicleLand",
		"WanderingWait",
		"MoveOffToOblivion",
		"Finish"
	};
	CompileTimeAssert(NELEM(s_StateNames) == kNumStates);

	if(taskVerifyf(iState >= 0 && iState < kNumStates, "Invalid state %d.", iState))
	{
		return s_StateNames[iState];
	}
	else
	{
		return "?";
	}
}

#endif	// !__FINAL

CTask::FSM_Return CTaskUnalerted::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	// Allow certain lod modes
	if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE))
	{
		// Allow the fish to go deeper than the waves if navigating to a scenario point that is below them.
		const static float sf_DeepScenarioDelta = 1.0f;
		bool bAllowDeeper = m_ScenarioFinder.GetScenarioPoint() && m_ScenarioFinder.GetScenarioPoint() && m_ScenarioFinder.GetScenarioPoint()->GetWorldPosition().GetZf() + sf_DeepScenarioDelta < pPed->GetTransform().GetPosition().GetZf();

		CTaskFishLocomotion::CheckAndRespectSwimNearSurface(pPed, bAllowDeeper);
	}
	else
	{
		// So, to be able to stick near the surface a ped has to know if it is near the surface to begin with.
		// To find that out the ped needs to be in highlod physics to get the water height at its location.
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	}

	// Check shocking events - let use scenario decide if it should check them if it is running as a subtask.
	CTask* pSubtask = GetSubTask();
	if (!pSubtask || pSubtask->GetTaskType() != CTaskTypes::TASK_USE_SCENARIO)
	{
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	}

	u32 timestep = GetTimeStepInMilliseconds();
	if(m_TimeUntilCheckScenarioMs > timestep)
	{
		m_TimeUntilCheckScenarioMs -= timestep;
	}
	else
	{
		m_TimeUntilCheckScenarioMs = 0;
	}
	
	ProcessKeepMovingWhenFindingWanderPath();

	if(pPed->IsNetworkClone())
	{
		// Try to match the existence of m_SPC_UseInfo with the value of
		// m_bHasScenarioChainUser, i.e. keep a scenario chain user if that's
		// done by the owner.
		if(m_bHasScenarioChainUser)
		{
			if(!m_SPC_UseInfo)
			{
				CScenarioPoint* pt = GetScenarioPoint();
				if(pt && pt->IsChained())
				{
					TryToObtainChainUserInfo(*pt, false);
				}
			}
		}
		else
		{
			if(m_SPC_UseInfo)
			{
				CleanUpScenarioPointUsage();
			}
		}
	}
	else
	{
		m_bHasScenarioChainUser = (m_SPC_UseInfo != NULL);
	}


	// Peds in this task are allowed to play gestures as long as the ped isn't holding a weapon.
	// Ambient props like cell phones are OK.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager || !pWeaponManager->GetEquippedWeaponObject() || pWeaponManager->GetEquippedWeaponObject()->m_nObjectFlags.bAmbientProp)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_GestureAnimsAllowed, true);
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskUnalerted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Wandering)
			FSM_OnEnter
				Wandering_OnEnter();
			FSM_OnUpdate
				return Wandering_OnUpdate();

		FSM_State(State_SearchingForScenario)
			FSM_OnEnter
				SearchingForScenario_OnEnter();
			FSM_OnUpdate
				return SearchingForScenario_OnUpdate();
			FSM_OnExit
				SearchingForScenario_OnExit();

		FSM_State(State_SearchingForNextScenarioInChain)
			FSM_OnEnter
				SearchingForNextScenarioInChain_OnEnter();
			FSM_OnUpdate
				return SearchingForNextScenarioInChain_OnUpdate();

		FSM_State(State_UsingScenario)
			FSM_OnEnter
				UsingScenario_OnEnter();
			FSM_OnUpdate
				return UsingScenario_OnUpdate();
			FSM_OnExit
				UsingScenario_OnExit();

		FSM_State(State_InVehicleAsDriver)
			FSM_OnEnter
				InVehicleAsDriver_OnEnter();
			FSM_OnUpdate
				return InVehicleAsDriver_OnUpdate();

		FSM_State(State_InVehicleAsPassenger)
			FSM_OnEnter
				InVehicleAsPassenger_OnEnter();
			FSM_OnUpdate
				return InVehicleAsPassenger_OnUpdate();

		FSM_State(State_ResumeAfterAbort)
			FSM_OnEnter
				ResumeAfterAbort_OnEnter();
			FSM_OnUpdate
				return ResumeAfterAbort_OnUpdate();
			FSM_OnExit
				ResumeAfterAbort_OnExit();

		FSM_State(State_HolsteringWeapon)
			FSM_OnEnter
				HolsteringWeapon_OnEnter();
			FSM_OnUpdate
				return HolsteringWeapon_OnUpdate();

		FSM_State(State_ExitingVehicle)
			FSM_OnEnter
				ExitingVehicle_OnEnter();
			FSM_OnUpdate
				return ExitingVehicle_OnUpdate();

		FSM_State(State_ReturningToVehicle)
			FSM_OnEnter
				ReturningToVehicle_OnEnter();
			FSM_OnUpdate
				return ReturningToVehicle_OnUpdate();
			FSM_OnExit
				ReturningToVehicle_OnExit();

		FSM_State(State_WaitForVehicleLand)
			FSM_OnUpdate
				return WaitForVehicleLand_OnUpdate();

		FSM_State(State_WanderingWait)
			FSM_OnUpdate
				return WanderingWait_OnUpdate();

		FSM_State(State_MoveOffToOblivion)
			FSM_OnEnter
				MoveOffToOblivion_OnEnter();
			FSM_OnUpdate
				return MoveOffToOblivion_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}


void CTaskUnalerted::CleanUp()
{
	if(!m_PointToReturnTo)
	{
		CleanUpScenarioPointUsage();
	}
	if (m_pPropTransitionHelper)
	{
		m_pPropTransitionHelper->ReleasePropRefIfUnshared(GetPed());

		m_pPropTransitionHelper.free();
	}

	//clear out the scenario history
	CPed* pPed = GetPed();
	if (pPed)
	{
		CVehicle* pVeh = pPed->GetVehiclePedInside();
		if (pVeh)
		{
			pVeh->GetIntelligence()->DeleteScenarioPointHistory();
		}

		// Clear the config flag associated with owner following.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LinkMBRToOwnerOnChain, false);
	}
}


void CTaskUnalerted::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent))
{
	m_ScenarioFinder.Reset();

	if(m_pScenarioTask)
	{
		if(NetworkInterface::IsGameInProgress() && m_pScenarioTask->GetTaskType()==CTaskTypes::TASK_USE_SCENARIO)
		{
			//B*2145657 & 2144242: In network games if the referenced m_pScenarioTask is still valid when aborting here and about 
			//to be deleted, then check if it had a valid prop helper. If it does, ensure the helper reference is cleared here since it's likely  
			//this situation has arisen as a ped migrated and was deleted in the same frame leaving this newly created copy m_pScenarioTask
			//with a prop reference for a prop that is to be deleted but causes the assert in the CTaskUseScenario destructor which can be ignored in this case.
			CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(m_pScenarioTask.Get());

			if(pTaskUseScenario->GetPropHelper())
			{
#if __BANK
				aiDebugf2("%s CTaskUnalerted::DoAbort CTaskUseScenario m_pPropHelper.free, prop %s. Frame: %d", 
					(GetEntity() && GetPed())?GetPed()->GetDebugName():"NUll ped",
					pTaskUseScenario->m_pPropHelper->GetPropNameHash().GetCStr(),
					fwTimer::GetFrameCount());
#endif

				pTaskUseScenario->m_pPropHelper.free();
			}
		}

		delete m_pScenarioTask;
		m_pScenarioTask = NULL;
	}

	CleanupPavementFloodFillRequest();
}


bool CTaskUnalerted::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	if(priority != ABORT_PRIORITY_IMMEDIATE)
	{
		bool bReturnToOldPoint = false;

		CPed* pPed = GetPed();

		// Return the old point for cowering.
		bReturnToOldPoint |= pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee);

		if (pEvent)
		{
			// Check for certain task responses.
			CTaskTypes::eTaskType taskType = ((CEvent*)pEvent)->GetResponseTaskType();
			switch(taskType)
			{
				case CTaskTypes::TASK_COMBAT:
				case CTaskTypes::TASK_THREAT_RESPONSE:
				case CTaskTypes::TASK_NM_BALANCE:
				case CTaskTypes::TASK_NM_BRACE:
				//case CTaskTypes::TASK_NM_STUMBLE:
				case CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE:
				case CTaskTypes::TASK_SHOCKING_EVENT_REACT:
				case CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY: // Hurry away will make a decision about whether the ped will leave internally.
				case CTaskTypes::TASK_SHOCKING_EVENT_WATCH:
				case CTaskTypes::TASK_COMPLEX_EVASIVE_STEP:
				case CTaskTypes::TASK_AGITATED:
				case CTaskTypes::TASK_AGITATED_ACTION:
				case CTaskTypes::TASK_FALL: // B* 1360533
				{
					bReturnToOldPoint = true;
					break;
				}
				default:
				{
					break;
				}
			}

			// Check explicitly for certain events.
			if (!bReturnToOldPoint)
			{
				switch(pEvent->GetEventType())
				{
					case EVENT_AGITATED_ACTION:
					case EVENT_POTENTIAL_BE_WALKED_INTO:
					case EVENT_POTENTIAL_GET_RUN_OVER:
					{
						bReturnToOldPoint = true;
						break;
					}
					default:
					{
						break;
					}
				}
			}

			// If we are responding to a CEventCarUndriveable, set m_bDontReturnToVehicle so
			// we don't try to return to it when the task returns.
			const int eventType = pEvent->GetEventType();
			if(eventType == EVENT_CAR_UNDRIVEABLE)
			{
				m_bDontReturnToVehicle = true;
			}

			// Never return to your old point when you see a dead body.
			// It doesn't look good to pretend like nothing bad is going on when there is a dead guy next to you.
			if (eventType == EVENT_SHOCKING_DEAD_BODY)
			{
				bReturnToOldPoint = false;
				m_bNeverReturnToLastPoint = true;
			}
		}
		
		if (bReturnToOldPoint)
		{
			const int state = GetState();
			if(state == State_UsingScenario || state == State_SearchingForNextScenarioInChain)
			{
				CTask* pSubTask = GetSubTask();
				if(pSubTask)
				{
					if(!pPed->GetIsInVehicle())	// Skip vehicles for now, to reduce risk of mismatch of ped/vehicle 
					{
						const int subTaskType = pSubTask->GetTaskType();

						// CTaskEnterVehicle is apparently possible here, and rightfully doesn't support
						// GetScenarioPointType(). We exclude it to avoid an assert, and then we may as well
						// not do the rest either.
						if(subTaskType != CTaskTypes::TASK_ENTER_VEHICLE)
						{
							// Wandering scenario points return to their currently used point.
							if (subTaskType == CTaskTypes::TASK_WANDERING_SCENARIO)
							{
								m_PointToReturnTo = static_cast<CTaskWanderingScenario*>(pSubTask)->GetCurrentScenarioPoint();
							}
							else
							{
								m_PointToReturnTo = pSubTask->GetScenarioPoint();
								if (subTaskType == CTaskTypes::TASK_USE_SCENARIO)
								{
									m_bDoCowardScenarioResume = static_cast<CTaskUseScenario*>(pSubTask)->ShouldUseCowardEnter();
								}
							}
							m_PointToReturnToRealTypeIndex = pSubTask->GetScenarioPointType();
						}
						else
						{
							m_PointToReturnTo = NULL;
							m_PointToReturnToRealTypeIndex = -1;
						}
					}
				}
			}
			else if (state == State_Wandering || state == State_WanderingWait)
			{
				if (m_LastUsedPoint)
				{
					m_PointToReturnTo = m_LastUsedPoint;
					m_PointToReturnToRealTypeIndex = m_LastUsedPointRealTypeIndex;
				}
			}

			// Prevent switching away from SearchingForNextScenarioInChain to Wandering,
			// right as the subtask finally gets aborted. Probably one frame would be enough.
			m_FrameCountdownAfterAbortAttempt = 3;
		}
	}

	return CTask::ShouldAbort(priority, pEvent);
}


s32 CTaskUnalerted::GetDefaultStateAfterAbort() const
{
	const CPed* pPed = GetPed();
	if(pPed && pPed->GetIsInVehicle())
	{
		return State_Finish;
	}

	return State_ResumeAfterAbort;
}


bool CTaskUnalerted::ShouldAlwaysDeleteSubTaskOnAbort() const
{
	// This task can be resumed, but when doing so, it should be re-creating
	// the subtasks, so we don't need to keep those around.
	return true;
}


CTask::FSM_Return CTaskUnalerted::Start_OnUpdate()
{
	CPed& ped = *GetPed();

	// In this state, make sure we don't have any left-over resume scenario, or the
	// stationary reaction flag left on.
	m_PointToReturnTo = NULL;
	m_PointToReturnToRealTypeIndex = -1;
	CTaskUseScenario::SetStationaryReactionsEnabledForPed(ped, false);

	if (ped.GetIsInVehicle())
	{
		PickVehicleState();
		return FSM_Continue;
	}

	//Check if we should holster our weapon.
	if(ShouldHolsterWeapon())
	{
		SetState(State_HolsteringWeapon);
	}
	// if we were supplied with a first scenario task to use then jump straight to the use state
	// otherwise go into the find state so that we can find a scenario around our location
	else if(m_pScenarioTask || (m_ScenarioFinder.GetScenarioPoint() && !CScenarioManager::IsVehicleScenarioType(m_ScenarioFinder.GetRealScenarioPointType())))
	{
		SetState(State_UsingScenario);
	}
	else if(m_bRoamMode || ( (GetPreviousState() == State_ResumeAfterAbort) && !m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn)))
	{
		bool bUseAquaticRoamMode = ped.GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);
		
		if (bUseAquaticRoamMode)
		{
			SetState(State_Wandering);
		}
		else if (MaySearchForScenarios())
		{
			// If in roam mode, go straight to finding a scenario, without wandering.
			SetState(State_SearchingForScenario);
		}
	}
	//Check if we should return to our last used vehicle.
	else if(ShouldReturnToLastUsedVehicle())
	{
		SetState(State_ReturningToVehicle);
	}
	else
	{
		PickWanderState();
		SetTimeUntilCheckScenario(sm_Tunables.m_ScenarioDelayInitialMin, sm_Tunables.m_ScenarioDelayInitialMax);
	}

	return FSM_Continue;
}

void CTaskUnalerted::Wandering_OnEnter()
{
	//Reset the chainuseinfo for the scenario point
	CleanUpScenarioPointUsage();

	CPed* pPed = GetPed();

	// if we have a prop cached for transition between two TaskUseScenario points
	if (m_pPropTransitionHelper)
	{
		// remove our reference to it and clean it up
		m_pPropTransitionHelper->ReleasePropRefIfUnshared(GetPed());

		m_pPropTransitionHelper.free();
	}

	// In this state, make sure we don't have any left-over resume scenario, or the
	// stationary reaction flag left on.
	m_PointToReturnTo = NULL;
	m_PointToReturnToRealTypeIndex = -1;
	CTaskUseScenario::SetStationaryReactionsEnabledForPed(*pPed, false);
	
	// Now that the ped is wandering, count them as being in the RANDOM_AMBIENT pop group.
	// Only do so if they have previously been a random scenario ped.
	if (pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO)
	{
		pPed->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
	}

	const CTask* pSubTask = GetSubTask();

	bool bUseAquaticRoamMode = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

	if (bUseAquaticRoamMode)
	{
		// Aquatic peds run swimming wander instead of normal wander.
		if (!pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_SWIMMING_WANDER)
		{
			CTaskSwimmingWander* pWanderTask = rage_new CTaskSwimmingWander();
			SetNewTask(pWanderTask);
		}
	}
	else
	{
		taskAssert(!m_bRoamMode);		// Not currently expected to get here in this case.

		if(!pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_WANDER)
		{
			float fHeading = pPed->GetCurrentHeading();
			//Check if we should relax the constraints.
			if(m_uRunningFlags.IsFlagSet(RF_WanderAwayFromSpecifiedTarget) && (m_timeTargetSet < (fwTimer::GetTimeInMilliseconds() + 5000)))
			{
				Vector3 vToTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_TargetToInitiallyWanderAwayFrom;
				fHeading = fwAngle::GetRadianAngleBetweenPoints(vToTarget.x, vToTarget.y, 0.0f, 0.0f);
				fHeading = fwAngle::LimitRadianAngleSafe( fHeading );
			}
			else if (m_bWanderAwayFromThePlayer)
			{
				CPed* pPlayer = CGameWorld::FindLocalPlayer();
				// Look for the local player.
				if (pPlayer)
				{
					// Calculate a heading away from the player's position.
					Vec3V vPlayer = pPlayer->GetTransform().GetPosition();
					Vec3V vPed = pPed->GetTransform().GetPosition();
					fHeading = fwAngle::GetRadianAngleBetweenPoints(vPed.GetXf(), vPed.GetYf(), vPlayer.GetXf(), vPlayer.GetYf());
				}
			}
			m_uRunningFlags.ChangeFlag(RF_WanderAwayFromSpecifiedTarget, false);

			// Reset the wander away from player bool.
			m_bWanderAwayFromThePlayer = false;

			CTaskWander* pWanderTask = rage_new CTaskWander(MOVEBLENDRATIO_WALK, fHeading);
			if (m_bKeepMovingWhenFindingWanderPath)
			{
				pWanderTask->KeepMovingWhilstWaitingForFirstPath(pPed);
				m_bKeepMovingWhenFindingWanderPath = false;
			}
			if(m_bCreateProp)
			{
				pWanderTask->CreateProp();
				m_bCreateProp = false;
			}
			SetNewTask(pWanderTask);
		}
	}
}


CTask::FSM_Return CTaskUnalerted::Wandering_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	if(m_TimeUntilCheckScenarioMs == 0)
	{
		if(MaySearchForScenarios())
		{
			bool bUseAquaticRoamMode = GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

			// Just need some minimal delay here to prevent an endless task state-change loop.
			if (!bUseAquaticRoamMode || GetTimeInState() > 0.3f)
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_SearchingForScenario);
			}
		}
		else
		{
			SetTimeUntilCheckScenario(sm_Tunables.m_ScenarioDelayAfterNotAbleToSearch);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskUnalerted::SearchingForScenario_OnEnter()
{
	// Local handle to the ped
	CPed* pPed = GetPed();

	// Clear the scenario finder flag
	m_uRunningFlags.ClearFlag(RF_HasStartedScenarioFinder);

	// Clear the pavement flag
	m_uRunningFlags.ClearFlag(RF_HasNearbyPavementToWanderOn);

	// Make sure the pavement helper is idle
	m_PavementHelper.CleanupPavementFloodFillRequest();

	// Check if the ped's navmesh tracker knows it is on pavement
	if( pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().IsOnPavement() )
	{
		// mark that we have nearby pavement
		m_uRunningFlags.SetFlag(RF_HasNearbyPavementToWanderOn);

		// go ahead and start the scenario finder
		StartScenarioFinderSearch();
	}
	else // we'll need to start a flood fill test using our helper
	{
		m_PavementHelper.StartPavementFloodFillRequest(pPed);
	}
}

CTask::FSM_Return CTaskUnalerted::SearchingForScenario_OnUpdate()
{
	// If we have a pavement search active
	if( m_PavementHelper.IsSearchActive() )
	{
		MakeScenarioTaskNotLeave();

		m_PavementHelper.UpdatePavementFloodFillRequest(GetPed());

		// wait for pavement helper to finish
		return FSM_Continue;
	}

	// If nearby pavement flag is not set
	if( !m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn) )
	{
		// If the pavement helper search found nearby pavement
		if( m_PavementHelper.HasPavement() )
		{
			// set the flag
			m_uRunningFlags.SetFlag(RF_HasNearbyPavementToWanderOn);
		}
	}

	if( !m_uRunningFlags.IsFlagSet(RF_HasStartedScenarioFinder) )
	{
		StartScenarioFinderSearch();
	}

	m_ScenarioFinder.Update();

	if(m_ScenarioFinder.IsActive())
	{
		// If the scenario finder is busy searching, we probably shouldn't let
		// our scenario subtask end due to timing out.
		MakeScenarioTaskNotLeave();
	}
	else
	{
		// If we didn't find a new point and don't have pavement to wander off to, revert back to the
		// previously-used scenario point if we have one. [10/4/2012 mdawe]
		if (!m_ScenarioFinder.GetScenarioPoint() && !m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn))
		{
			if (m_LastUsedPoint && !CPedPopulation::IsEffectInUse(*m_LastUsedPoint))
			{
				// default to using last
				bool useLast = true;

				// don't use the LastUsedPoint if the point is now obstructed
				if (useLast && m_LastUsedPoint->IsObstructedAndReset())
				{
					useLast = false;
				}

				// if last point was in a chain
				if (useLast && m_LastUsedPoint->IsChained())
				{
					// For chained points we need to make sure that the last used point we are attempting to use
					//   is not in a chain that is now being used.
					useLast = !m_LastUsedPoint->IsUsedChainFull();
				}

				// Don't allow the ped to return to this point if the conditions of using it aren't met. [2/6/2013 mdawe]
				/// We may want to call CheckCanScenarioBeUsed() here in the future?
				if( useLast )
				{
					const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(m_LastUsedPointRealTypeIndex);
					if (pScenarioInfo)
					{
						CScenarioCondition::sScenarioConditionData conditionData;
						conditionData.pPed = GetPed();

						useLast = CTaskUnalerted::CanScenarioBeReused(*m_LastUsedPoint, conditionData, m_LastUsedPointRealTypeIndex, m_SPC_UseInfo);
					}
				}

				// If using last point, set member values as the found values
				if (useLast)
				{
					m_ScenarioFinder.SetScenarioPoint(m_LastUsedPoint, m_LastUsedPointRealTypeIndex);
				}
			}
		}

		if(m_ScenarioFinder.GetScenarioPoint())
		{
			CTask* pSubTask = GetSubTask();
			if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
			{
				// In this case, we've got a new scenario point but we're still
				// running a CTaskUseScenario. The primary case where this comes up
				// is if m_bRoamMode is set, and we stayed in the SearchingForNextScenarioInChain
				// until CTaskUseScenario's timer expired, and then switched to this state
				// and found a new point to go to - in that case, we want to wait until
				// the Outro animation has finished before moving to the next scenario.
			}
			else
			{
				// If we found a scenario we can start moving to it
				PickUseScenarioState();

				// pScenarioPoint could have changed inside PickUseScenarioState() [2/4/2013 mdawe]
				CScenarioPoint* pScenarioPoint = m_ScenarioFinder.GetScenarioPoint();

				//The assumption here is that once the ped is in a chain it wont jump to a different chain as part of the normal
				// ScenarioFinder functionality. So by that if we have UserInfo for this chain already then we dont need to keep grabbing it.
				if (pScenarioPoint && pScenarioPoint->IsChained())
				{
					if (!m_SPC_UseInfo)
					{
						TryToObtainChainUserInfo(*pScenarioPoint, true);

						//Vec3V pos = pScenarioPoint->GetPosition();
						//Displayf("Register Chain User {%x} P %s {%x} <<%.5f, %.5f, %.5f>> {%x}", m_SPC_UseInfo, pPed->GetModelName(), pPed, pos.GetXf(), pos.GetYf(), pos.GetZf(), pScenarioPoint);
					}
				}
			}
			return FSM_Continue;
		}
		else if(m_bRoamMode)
		{
			CPed* pPed = GetPed();
			bool bUseAquaticRoamMode = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

			// If you are an aquatic ped, then go back to swimming around (even in roam mode).
			if (bUseAquaticRoamMode)
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_Wandering);
			}
			else
			{
				// If we are in this state with m_bRoamMode, i.e. we didn't find any
				// other scenario to go to, we want to just continue playing the scenario
				// animations. See B* 344169: "When scenario peds using TASK_ROAM fail to
				// find a point to move towards they dont play any scenario anims".
				MakeScenarioTaskNotLeave();

				// After some time, we try to find nearby scenarios again, but we keep the task.
				// There is probably little need to do this frequently, because we're not moving
				// so the conditions only change slowly (time of day, appearance of new scenarios, etc).
				if(GetTimeInState() > sm_Tunables.m_ScenarioDelayAfterFailureWhenStationary)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
					SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				}
			}
			return FSM_Continue;
		}
		else
		{
			// Go back to wandering, but keep the task.
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			PickWanderState();
			SetTimeUntilCheckScenario(sm_Tunables.m_ScenarioDelayAfterFailureMin, sm_Tunables.m_ScenarioDelayAfterFailureMax);

			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskUnalerted::SearchingForScenario_OnExit()
{
	m_uRunningFlags.ClearFlag(RF_SearchForAnyNonChainedScenarios);
}

void CTaskUnalerted::SearchingForNextScenarioInChain_OnEnter()
{
	//Cache the point we are searching from.
	m_PointToSearchFrom = m_ScenarioFinder.GetScenarioPoint();
	
	//Cache the real type index for the point.
	m_PointToSearchFromRealTypeIndex = m_ScenarioFinder.GetRealScenarioPointType();
	
	//Start a search for the next scenario in the chain.
	StartSearchForNextScenarioInChain();
}


CTask::FSM_Return CTaskUnalerted::SearchingForNextScenarioInChain_OnUpdate()
{
	CTask* pSubTask = GetSubTask();
	CPed* pPed = GetPed();

	// Only start a new hangout transition if the ped is close to the original point.
	if (pPed->GetPedIntelligence()->IsStartNewHangoutScenario() && CloseEnoughForHangoutTransition())
	{
		s32 iNewScenarioType = GetRandomScenarioTypeSafeForResume();

		const CConditionalAnimsGroup* pChosenGroup = CScenarioManager::GetConditionalAnimsGroupForScenario(iNewScenarioType);

		CScenarioCondition::sScenarioConditionData conditions;
		conditions.pPed = pPed;
		conditions.iScenarioFlags = SF_IgnoreHasPropCheck; // ped is going to get a prop, no need to check it here.

		if (pChosenGroup && pChosenGroup->CheckForMatchingConditionalAnims(conditions))
		{
			// Only kill the subtask and start a new scenario if you can use one of the flourish scenarios.

			// Kill your subtask if you have one; we're about to replace it [8/10/2012 mdawe]
			if (pSubTask)
			{
				pSubTask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL);
			}

			// Create the new TaskUseScenario [8/10/2012 mdawe]
			s32 iFlags = CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CheckShockingEvents;
			CTaskUseScenario* pNextScenario = rage_new CTaskUseScenario(iNewScenarioType, m_LastUsedPoint, iFlags);
			m_pScenarioTask = pNextScenario;
			SetState(State_UsingScenario);
		}
		else
		{
			// If the ped wanted to play a flourish scenario but didn't pick a valid one, then just leave.
			PickWanderState();
		}

		// Since we are switching away from this state, where we may have had the scenario finder actively
		// searching for something, we may need to call Reset() on it so that we don't find the scenario finder
		// unexpectedly in an active state the next time we try to use it.
		m_ScenarioFinder.Reset();

		// Clear this flag [8/10/2012 mdawe]
		pPed->GetPedIntelligence()->SetStartNewHangoutScenario(false);

		return FSM_Continue;
	}
	else
	{
		pPed->GetPedIntelligence()->SetStartNewHangoutScenario(false);
	}

	m_ScenarioFinder.Update();

	bool doneWithScenarioInRoamMode = false;

	bool bSearchActive = m_ScenarioFinder.IsActive();

	CScenarioPoint* pPointFound = m_ScenarioFinder.GetScenarioPoint();

	// If we have found a new point, and we haven't yet tested it, test it against
	// blocking areas. We basically want to detect this as early as possible, to
	// give us a lot of time to despawn gracefully.
	if(!bSearchActive && pPointFound && !m_NextPointTestedForBlockingAreas)
	{
		m_NextPointTestedForBlockingAreas = true;
		if(CScenarioManager::IsScenarioPointInBlockingArea(*pPointFound))
		{
			// Register that we'd like to despawn gracefully if possible.
			RegisterDistress();

			// At least for now, vehicles are handled separately. If we are on
			// foot, clear out the scenario point in the scenario finder, as if
			// there had been no next point on the chain. This should make us either
			// stay on the chain, if not allowed to wander, or wander off.
			if(!pPed->GetIsInVehicle())
			{
				pPointFound = NULL;
				m_ScenarioFinder.SetScenarioPoint(NULL, -1);
			}
		}
	}

	if(bSearchActive)
	{
		// We're still looking to find the next scenario, best to not leave the current scenario
		// until we're done with that.
		MakeScenarioTaskNotLeave();
	}
	else if(!m_ScenarioFinder.GetScenarioPoint())
	{
		//At this point, we have failed to find a scenario point.
		
		//Can we start a new search?
		bool bRelaxNextScenarioInChainConstraints = m_uRunningFlags.IsFlagSet(RF_RelaxNextScenarioInChainConstraints);
		bool bTimeoutExceeded = (m_fTimeSinceLastSearchForNextScenarioInChain > sm_Tunables.m_TimeBetweenSearchesForNextScenarioInChain);
		bool bCanStartNewSearch = !bRelaxNextScenarioInChainConstraints || bTimeoutExceeded;
		if(bCanStartNewSearch)
		{
			//Set the relax flag.
			bool bShouldRelaxNextScenarioInChainConstraints = !bTimeoutExceeded;
			m_uRunningFlags.ChangeFlag(RF_RelaxNextScenarioInChainConstraints, bShouldRelaxNextScenarioInChainConstraints);
			
			//Set the scenario point again -- everything has been reset.
			m_ScenarioFinder.SetScenarioPoint(m_PointToSearchFrom, m_PointToSearchFromRealTypeIndex);
			
			//Start a search for the next scenario in the chain.
			StartSearchForNextScenarioInChain();
			
			//We are still looking for the next scenario, don't leave the current one.
			MakeScenarioTaskNotLeave();
			
			//Don't do anything else.
			return FSM_Continue;
		}
		
		if(m_bRoamMode)
		{
			if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
			{
				// In this case, we are in roam mode, and we failed to find a scenario. We should probably
				// stay in this state until the CTaskUseScenario subtask times out, but we don't want it
				// to actually play exit animations - we may still need to stay in that state for longer
				// if we don't find any other point to go to.

				MakeScenarioTaskNotLeave();

				const CTaskUseScenario* pUseScenarioTask = static_cast<const CTaskUseScenario*>(pSubTask);
				if(pUseScenarioTask->GetWouldHaveLeftLastFrame())
				{
					// Check for the "FlyOffToOblivion" flag.  If it's there, then go to the special state.
					const CScenarioPoint* pScenarioPoint = pUseScenarioTask->GetScenarioPoint();
					if (pScenarioPoint && pScenarioPoint->IsFlagSet(CScenarioPointFlags::FlyOffToOblivion))
					{
						SetState(State_MoveOffToOblivion);
						return FSM_Continue;
					}

					// CTaskUseScenario's use timer has expired.
					doneWithScenarioInRoamMode = true;
				}
			}
		}
	}
	else
	{
		// The scenario finder has found the next point to use. Let the UseTaskScenario leave even if it hasn't found pavement
		if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			CTaskUseScenario* pUseScenarioTask = static_cast<CTaskUseScenario*>(pSubTask);
			pUseScenarioTask->SetCanExitToChainedScenario();
		}
	}

	if (!bSearchActive)
	{
		if (ShouldEndScenarioBecauseOfNearbyPlayer())
		{
			EndActiveScenario();
		}
	}

	if(pSubTask && !GetIsFlagSet(aiTaskFlags::SubTaskFinished) && !doneWithScenarioInRoamMode)
	{
		//Check if we should wait while searching for the next scenario.
		bool bWaitWhileSearchingForNextScenarioInChain = ShouldWaitWhileSearchingForNextScenarioInChain();
		if(bWaitWhileSearchingForNextScenarioInChain)
		{
			//Ensure the scenario does not leave.
			MakeScenarioTaskNotLeave();

			//Increase the time since the last search.
			m_fTimeSinceLastSearchForNextScenarioInChain += GetTimeStep();
		}
		
		const int subTaskType = pSubTask->GetTaskType();

		// if our subtask is a CTaskUseScenario
		if (subTaskType == CTaskTypes::TASK_USE_SCENARIO)
		{
			// Cache the prop from the existing UseScenario task
			CTaskUseScenario* pUseScenarioSubTask = static_cast<CTaskUseScenario*>(pSubTask);
			m_pPropTransitionHelper = pUseScenarioSubTask->GetPropManagementHelper();
		}
		else if(subTaskType == CTaskTypes::TASK_USE_VEHICLE_SCENARIO)
		{
			CTaskUseVehicleScenario* pUseVehicleScenarioTask = static_cast<CTaskUseVehicleScenario*>(pSubTask);

			// Check if the vehicle scenario failed. Initially, this can happen if a parking
			// spot we planned to use was blocked.
			if(pUseVehicleScenarioTask->GetState() == CTaskUseVehicleScenario::State_Failed)
			{
				m_ScenarioFinder.Reset();
				SetState(State_Start);

				// We had to abort our chain. This means that the vehicle may not be in a very
				// good spot to resume normal driving behavior. If possible, we try to get rid
				// of it when the player isn't looking, to reduce the risk of the player seeing
				// something bad.
				RegisterDistress();

				return FSM_Continue;
			}

			if(!m_ScenarioFinder.IsActive())
			{
				if(m_ScenarioFinder.GetScenarioPoint())
				{
					CScenarioChainingEdge::eNavSpeed navSpeed = (CScenarioChainingEdge::eNavSpeed)m_ScenarioFinder.GetNavSpeed();
					Assert(navSpeed >= 0 && navSpeed < CScenarioChainingEdge::kNumNavSpeeds);
					const float driveSpeed = s_NavSpeeds[navSpeed];

					const int nextType = m_ScenarioFinder.GetRealScenarioPointType();
					if(CScenarioManager::IsVehicleScenarioType(nextType))
					{
						pUseVehicleScenarioTask->SetScenarioPointNxt(m_ScenarioFinder.GetScenarioPoint(), driveSpeed);
					}
					else
					{
						pUseVehicleScenarioTask->SetScenarioPointNxt(NULL, 0.0f);
					}
					pUseVehicleScenarioTask->SetWillContinueDriving(true);
				}
				else
				{	
					pUseVehicleScenarioTask->SetScenarioPointNxt(NULL, 0.0f);

#if __ASSERT
					//if we get here, it means the current scenario point is the last one on the chain
					//add some asserts to ensure it's on the road network
					const CScenarioPoint* pScenarioPointCurrent = pUseVehicleScenarioTask->GetScenarioPoint();
					const bool bGoodScenarioPoint = !pScenarioPointCurrent || pScenarioPointCurrent->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint)
						|| pScenarioPointCurrent->IsFlagSet(CScenarioPointFlags::FlyOffToOblivion)
						|| pScenarioPointCurrent->IsFlagSet(CScenarioPointFlags::InWater)
						|| (pScenarioPointCurrent->GetTimeTillPedLeaves() < 0.0f)
						|| !SCENARIOPOINTMGR.IsChainEndPoint(*pScenarioPointCurrent)
						|| CTaskVehicleThreePointTurn::IsPointOnRoad(VEC3V_TO_VECTOR3(pScenarioPointCurrent->GetPosition()), pPed->GetVehiclePedInside(), true);
					Assertf(bGoodScenarioPoint
						, "Vehicle scenario point at (%.2f, %.2f, %.2f) is not within road boundaries, this will cause a pop!"
						, pScenarioPointCurrent->GetPosition().GetXf()
						, pScenarioPointCurrent->GetPosition().GetYf()
						, pScenarioPointCurrent->GetPosition().GetZf());

#endif //__ASSERT
				}
			}
		}
		else if(subTaskType == CTaskTypes::TASK_WANDERING_SCENARIO)
		{
			//Grab the task.
			CTaskWanderingScenario* pWanderScenarioTask = static_cast<CTaskWanderingScenario *>(pSubTask);

			// If the wander task itself detected that any of the points it looked at
			// are in blocking areas, consider this a distress situation.
			if(pWanderScenarioTask->IsBlockedByArea())
			{
				RegisterDistress();
			}


			if(!m_ScenarioFinder.IsActive())
			{
				// If we've got a next point for our wandering scenario, switch to the
				// UsingScenario state until the subtask reports it's done with the current
				// point. If we don't have a next point, we let the subtask quit.
				// We also let the subtask quit if the next point is a vehicle scenario point,
				// so that on the next update, we'll try to get into a vehicle and drive to it.
				const CScenarioPoint* nextPoint = m_ScenarioFinder.GetScenarioPoint();
				const int nextType = m_ScenarioFinder.GetRealScenarioPointType();
				if(nextPoint && !CScenarioManager::IsVehicleScenarioType(nextType))
				{
					SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					// Not sure, should we use PickUseScenarioState() here?
					SetState(State_UsingScenario);
				}
				else
				{
					// We didn't find anywhere else for our CTaskWanderingScenario to go,
					// and we probably don't want to let that subtask wander around aimlessly.
					
					// If we don't have a vehicle yet, claim the next vehicle on our chain
					if (CScenarioManager::IsVehicleScenarioType(nextType) && !pPed->GetMyVehicle() && m_SPC_UseInfo && m_SPC_UseInfo->IsIndexInfoValid())
					{
						CVehicle* pFoundVehicle = FindVehicleUsingMyChain();
						if (pFoundVehicle)
						{
							pPed->SetMyVehicle(pFoundVehicle);
						}
					}

					//Check if we should wait while searching for the next scenario.
					if(bWaitWhileSearchingForNextScenarioInChain)
					{
						//Wait for the next scenario when we are not using the scenario.
						pWanderScenarioTask->SetWaitForNextScenarioWhenNotUsingScenario();
					}
					else
					{
						//Quit when we are not using the scenario.
						pWanderScenarioTask->SetQuitWhenNotUsingScenario();
					}
				}

				return FSM_Continue;
			}
		}
		
		// Still busy using the previous scenario, update the time last used so we begin measuring
		// from when we stopped using it, in terms of avoiding choosing the same scenario again.
		m_LastUsedPointTimeMs = fwTimer::GetTimeInMilliseconds();

		return FSM_Continue;
	}

	// Chain to a vehicle scenario. 
	// Check if we have a subtask, and it is finished, and it is not done with the roaming scenario
	if(pSubTask && GetIsFlagSet(aiTaskFlags::SubTaskFinished) && !doneWithScenarioInRoamMode)
	{
		//taskDisplayf("Ped: %p - Frame: %d Cluster - CTaskUnalerted::SearchingForNextScenarioInChain_OnUpdate - Looking to go to driving", GetPed(), fwTimer::GetFrameCount());

		// if our subtask is a CTaskUseScenario and our ScenarioFinder is not active
		const int subTaskType = pSubTask->GetTaskType();
		if (subTaskType == CTaskTypes::TASK_USE_SCENARIO && !m_ScenarioFinder.IsActive() && m_ScenarioFinder.GetScenarioPoint())
		{
			// If the next scenario point is a vehicle scenario
			const int nextType = m_ScenarioFinder.GetRealScenarioPointType();
			if (CScenarioManager::IsVehicleScenarioType(nextType))
			{
				Assert(m_ScenarioFinder.GetScenarioPoint()->IsChained());

				// if we have our chain useinfo and it is valid
				if (m_SPC_UseInfo && m_SPC_UseInfo->IsIndexInfoValid())
				{
					//This needs to :
					// A. get the vehicle that is in the chain.
					CVehicle* pFoundVehicle = FindVehicleUsingMyChain();

					if (pFoundVehicle)
					{
						SeatRequestType seatRequestType = SR_Any;
						int seat = -1;
						VehicleEnterExitFlags flags;
						flags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);

						bool canUseVehicle = false;
						if(m_ScenarioFinder.GetAction() == CScenarioChainingEdge::kMoveIntoVehicleAsPassenger)
						{
							flags.BitSet().Set(CVehicleEnterExitFlags::DontUseDriverSeat);
							seat = Seat_frontPassenger;
							seatRequestType = SR_Prefer;

							canUseVehicle = true;
						}
						else
						{
							// B. make sure there is no driver in the vehicle
							// Note: we used to check for passengers here too, but now when scenario guys can enter
							// as passengers, I don't think that makes sense.
							if(!pFoundVehicle->GetDriver() /* && !pFoundVehicle->GetNumberOfPassenger() */ )
							{
								seatRequestType = SR_Specific;
								seat = pFoundVehicle->GetDriverSeat();

								canUseVehicle = true;
							}
						}

						if(canUseVehicle)
						{
							//Check to make sure vehicle is valid for use in scenario ...
							//taskDisplayf("Ped: %p - Frame: %d Cluster - CTaskUnalerted::SearchingForNextScenarioInChain_OnUpdate - Starting CTaskEnterVehicle", GetPed(), fwTimer::GetFrameCount());

							const float mbr = MOVEBLENDRATIO_WALK;

							// C. Kick off the task to get into the vehicle.

							SetNewTask(rage_new CTaskEnterVehicle(pFoundVehicle, seatRequestType, seat, flags, 0.0f, mbr));
							return FSM_Continue;
						}
					}
				}
			}
		}
	}

	// This is here because otherwise it seems like CTaskUseScenario may finish its abortion
	// procedure, and then CTaskUnalerted runs and immediately switches to the Wandering
	// state for just one frame, which would clear m_PointToReturnTo and prevent resuming
	// successfully at the desired scenario point. Not a very clean solution, but should work.
	// In most cases we should only hit this once and wouldn't even need the counter, but
	// it's there to avoid getting stuck here forever in case we ended up not actually getting aborted
	// after we aborted the subtask.
	if(m_PointToReturnTo && m_FrameCountdownAfterAbortAttempt > 0)
	{
		m_FrameCountdownAfterAbortAttempt--;
		return FSM_Continue;
	}

	if(!m_ScenarioFinder.IsActive())
	{
		if(ShouldReturnToLastUsedScenarioPointDueToAgitated())
		{
			m_ScenarioFinder.SetScenarioPoint(GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint(),
				GetPed()->GetPedIntelligence()->GetLastUsedScenarioPointType());

			//Clear the flag, in case we can't re-enter the scenario point.
			GetPed()->GetPedIntelligence()->GetLastUsedScenarioFlags().ClearFlag(CPedIntelligence::LUSF_ExitedDueToAgitated);
		}

		if(GetPed()->GetIsInVehicle())
		{
			PickVehicleState();
			return FSM_Continue;
		}
		else if(m_ScenarioFinder.GetScenarioPoint())
		{
			// If we found a scenario we can start moving to it
			PickUseScenarioState();
			return FSM_Continue;
		}
		else if(m_bRoamMode)
		{
			// After using a wandering point, we need to check if the FlyOffToOblivion flag is set and transition accordingly.
			if (m_LastUsedPoint && m_LastUsedPoint->IsFlagSet(CScenarioPointFlags::FlyOffToOblivion))
			{
				SetState(State_MoveOffToOblivion);
			}
			else
			{
				// Go to the SearchingForScenario state to look for non-chained points to go
				// to next, but keep running the subtask to use the current scenario.
				SetState(State_SearchingForScenario);
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			}

			return FSM_Continue;
		}
		else
		{
			PickWanderState();
			SetTimeUntilCheckScenario(sm_Tunables.m_ScenarioDelayAfterSuccessMin, sm_Tunables.m_ScenarioDelayAfterSuccessMax);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskUnalerted::SearchingForNextScenarioInChain_OnExit()
{
	//Clear the point to search from.
	m_PointToSearchFrom = NULL;
	
	//Clear the real type index.
	m_PointToSearchFromRealTypeIndex = -1;
	
	//Clear the relax constraints flag.
	m_uRunningFlags.ClearFlag(RF_RelaxNextScenarioInChainConstraints);
}

void CTaskUnalerted::UsingScenario_OnEnter()
{
	static const float s_MoveBlendRatios[] =
	{
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		-1.0f,
		MOVEBLENDRATIO_WALK,
		MOVEBLENDRATIO_RUN,
		MOVEBLENDRATIO_SPRINT
	};
	CompileTimeAssert(NELEM(s_MoveBlendRatios) == CScenarioChainingEdge::kNumNavSpeeds);

	aiTask* pTask = GetSubTask();
	if(pTask && !GetIsFlagSet(aiTaskFlags::KeepCurrentSubtaskAcrossTransition))
	{
		// This is a bit funky, but it looks like we get the OnEnter call before
		// we clear the old task, which is undesirable in this case.
		pTask = NULL;
	}

	CPed* pPed = GetPed();
	// Now that the ped is about to use a scenario, consider them a scenario ped for purposes of population counts.
	// Only do so if they have previously been a random ambient ped.
	if (pPed->PopTypeGet() == POPTYPE_RANDOM_AMBIENT)
	{
		pPed->PopTypeSet(POPTYPE_RANDOM_SCENARIO);
	}

	// If we don't already have a task, and the previous scenario we use specified a wander scenario
	// through the m_WanderScenarioToUseAfter parameter, create a wander scenario task.
	if(!pTask && m_WanderScenarioToUseNext >= 0)
	{
		CTaskWanderingScenario* pWanderScenarioTask = rage_new CTaskWanderingScenario(m_WanderScenarioToUseNext);
		SetNewTask(pWanderScenarioTask);
		pTask = pWanderScenarioTask;
	}
	m_WanderScenarioToUseNext = -1;

	CScenarioPoint* pScenarioPoint = m_ScenarioFinder.GetScenarioPoint();
	bool bLinkMBRToOwner = m_ScenarioFinder.GetAction() == CScenarioChainingEdge::kMoveFollowMaster;
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LinkMBRToOwnerOnChain, bLinkMBRToOwner);

	float mbr = -1.0f;
	if(pScenarioPoint)
	{
		CScenarioChainingEdge::eNavSpeed navSpeed = (CScenarioChainingEdge::eNavSpeed)m_ScenarioFinder.GetNavSpeed();
		Assert(navSpeed >= 0 && navSpeed < CScenarioChainingEdge::kNumNavSpeeds);

		mbr = s_MoveBlendRatios[navSpeed];

		//The assumption here is that once the ped is in a chain it wont jump to a different chain as part of the normal
		// ScenarioFinder functionality. So by that if we have UserInfo for this chain already then we dont need to keep grabbing it.
		if (pScenarioPoint->IsChained() && !m_SPC_UseInfo)
		{
			TryToObtainChainUserInfo(*pScenarioPoint, true);

			//Vec3V pos = pScenarioPoint->GetPosition();
			//Displayf("Register Chain User {%x} P %s {%x} <<%.5f, %.5f, %.5f>> {%x}", m_SPC_UseInfo, pPed->GetModelName(), pPed, pos.GetXf(), pos.GetYf(), pos.GetZf(), pt);
		}
	}

	if(!pTask)
	{
		// If we were supplied with a task then use that one otherwise create the proper task and set it as our new task
		if(m_pScenarioTask)
		{		
			// Note: it's probably possible for m_ScenarioFinder.Get2dEffect() to be NULL
			// in this case, may be valid for the user to just pass in a task. This is
			// probably happening for CTaskParkedVehicleScenario given through
			// CScenarioManager::GiveScenarioToParkedCar().

			pTask = m_pScenarioTask;
			m_pScenarioTask = NULL;
		}
		else
		{
			taskAssert(pScenarioPoint);
			// Create the task for unalerted use of the scenario (check blocking areas and other conditions).
			pTask = CScenarioManager::CreateTask(pScenarioPoint, m_ScenarioFinder.GetRealScenarioPointType(), true, mbr);

			// Consider creating a CTaskAmbientClips to play the right animations and hold on to props, while
			// approaching the scenario.
			if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
			{
				//Grab the task.
				CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario *>(pTask);

				if (m_bDoCowardScenarioResume)
				{
					pTaskUseScenario->SetDoCowardEnter();
					m_bDoCowardScenarioResume = false;
				}

				if (m_bScenarioReEntry)
				{
					pTaskUseScenario->SetIsDoingResume();
				}
				
				const CTaskDataInfo& taskData = GetPed()->GetTaskData();
				u32 hash = taskData.GetTaskWanderConditionalAnimsGroupHash();
				const CConditionalAnimsGroup* pGrp = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(hash);
#if __BANK
				taskAssertf(pGrp, "Failed to find conditional anims %s, for CTaskUnalerted.", taskData.GetTaskWanderConditionalAnimsGroup());
#endif
				if(pGrp)
				{
					CTaskAmbientClips* pAmbientClips = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip, pGrp);
					pTaskUseScenario->SetAdditionalTaskDuringApproach(pAmbientClips);
				}
				
				//Check if we are ignoring time.
				if(m_ScenarioFinder.GetFindOptions().m_bIgnoreTime)
				{
					//Ignore the time condition on the scenario.
					//This will allow us to stay in scenarios that have been found with a relaxed time restriction.
					pTaskUseScenario->SetIgnoreTimeCondition();
				}

				//If we have a cached propHelper, set it on the new TaskUseScenario
				if (m_pPropTransitionHelper)
				{
					pTaskUseScenario->SetPropManagmentHelper(m_pPropTransitionHelper);
				}
			}
		}

		if(pScenarioPoint)
		{
			m_WanderScenarioToUseNext = CTaskWanderingScenario::GetWanderScenarioToUseNext(m_ScenarioFinder.GetRealScenarioPointType(), GetPed());
		}

		SetNewTask(pTask);
	}
	else
	{
		Assert(!m_pScenarioTask);
		Assert(pTask->GetTaskType() == CTaskTypes::TASK_WANDERING_SCENARIO);
	}

	// Remember this scenario point as the last one we used.
	m_LastUsedPoint = pScenarioPoint;
	// Set the last used point index
	m_LastUsedPointRealTypeIndex = m_ScenarioFinder.GetRealScenarioPointType();
	m_LastUsedPointTimeMs = fwTimer::GetTimeInMilliseconds();

	if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_WANDERING_SCENARIO)
	{
		CTaskWanderingScenario* pWanderScenarioTask = static_cast<CTaskWanderingScenario*>(pTask);
		pWanderScenarioTask->SetNextScenario(pScenarioPoint, m_ScenarioFinder.GetRealScenarioPointType(), mbr);
	}

	// Clear this flag now that the subtask has been created.
	m_bScenarioReEntry = false;
}


CTask::FSM_Return CTaskUnalerted::UsingScenario_OnUpdate()
{
	// if our subtask is a CTaskWanderingScenario
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_WANDERING_SCENARIO)
	{
		// if our subtask has its next scenario
		CTaskWanderingScenario* pWanderScenarioTask = static_cast<CTaskWanderingScenario*>(pSubTask);

		// If CTaskWanderingScenario detected a blocking area, consider ourselves under distress.
		if(pWanderScenarioTask->IsBlockedByArea())
		{
			RegisterDistress();
		}

		if(pWanderScenarioTask->GotNextScenario())
		{
			// check if the player is nearby and the EndScenarioIfPlayerWithinRadius is set
			if (ShouldEndScenarioBecauseOfNearbyPlayer())
			{
				// end CTaskWanderScenario's scenario subtask
				pWanderScenarioTask->EndActiveScenarioTask();
			}
			// Haven't even started to go to this position we already gave it, so don't
			// start looking for the next chained scenario just yet.
			return FSM_Continue;
		}
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	SetState(State_SearchingForNextScenarioInChain);

	return FSM_Continue;
}

void CTaskUnalerted::UsingScenario_OnExit()
{
	//We don't need the propTransitionHelper reference anymore.
	if (m_pPropTransitionHelper)
	{
		m_pPropTransitionHelper.free();
	}
}

void CTaskUnalerted::InVehicleAsDriver_OnEnter()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	taskAssert(pVeh);

	CScenarioPoint* pt = m_ScenarioFinder.GetScenarioPoint();
	if(pt)
	{
		// Deal with the possibility of the point being blocked, and possibly
		// stop using scenarios and fall back on some other behavior.
		if(HandleEndVehicleScenarioBecauseOfBlockingArea(*pt))
		{
			pt = NULL;
		}
	}

	if(pt)
	{
		//The assumption here is that once the ped is in a chain it wont jump to a different chain as part of the normal
		// ScenarioFinder functionality. So by that if we have UserInfo for this chain already then we dont need to keep grabbing it.
		if (pt->IsChained() && !m_SPC_UseInfo)
		{
			TryToObtainChainUserInfo(*pt, true);

			//Vec3V pos = pt->GetPosition();
			//Displayf("Register Chain User {%x} P %s {%x}, V %s {%x} <<%.5f, %.5f, %.5f>> {%x}", m_SPC_UseInfo, pPed->GetModelName(), pPed, pVeh->GetModelName(), pVeh, pos.GetXf(), pos.GetYf(), pos.GetZf(), pt);
		}

		u32 flags = 0;

		CScenarioChainingEdge::eNavMode navMode = (CScenarioChainingEdge::eNavMode)m_ScenarioFinder.GetNavMode();

		switch(navMode)
		{
			case CScenarioChainingEdge::kDirect:
				break;
			case CScenarioChainingEdge::kNavMesh:
				flags |= CTaskUseVehicleScenario::VSF_NavUseNavMesh;
				break;
			case CScenarioChainingEdge::kRoads:
				flags |= CTaskUseVehicleScenario::VSF_NavUseRoads;
				break;
			default:
				taskAssertf(0, "Unknown nav mode %d.", (int)navMode);
				break;
		}

		// This flag will be overwritten with the true value once we know it, depending on whether
		// we have a chained scenario to continue to or not.
		flags |= CTaskUseVehicleScenario::VSF_WillContinueDriving;

		CScenarioChainingEdge::eNavSpeed navSpeed = (CScenarioChainingEdge::eNavSpeed)m_ScenarioFinder.GetNavSpeed();
		Assert(navSpeed >= 0 && navSpeed < CScenarioChainingEdge::kNumNavSpeeds);
		const float driveSpeed = s_NavSpeeds[navSpeed];

		CScenarioPoint* pPrevPoint = m_PointToSearchFrom;

		// If we are coming from an on-foot scenario, don't pass that in as the previous point to CTaskUseVehicleScenario.
		// If it's attached to an entity, it doesn't know how to network sync it and will fail an assert, and even if it's not,
		// it has little to do with how the vehicle should behave, since it's not where the vehicle came from but where the ped came from
		// before entering the vehicle.
		if(pPrevPoint && !CScenarioManager::IsVehicleScenarioType(pPrevPoint->GetScenarioTypeVirtualOrReal()))
		{
			pPrevPoint = NULL;
		}

		const CTaskUseVehicleScenario* pOldScenarioTask = static_cast<const CTaskUseVehicleScenario*>(FindSubTaskOfType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO));
		if (pOldScenarioTask && pOldScenarioTask->GetScenarioPointPrev())
		{
			CRoutePoint prevRoutePoint;
			//point we are going to
			prevRoutePoint.m_vPositionAndSpeed = Vec4V(pOldScenarioTask->GetScenarioPointPrev()->GetPosition());
			prevRoutePoint.SetSpeed(driveSpeed);
			prevRoutePoint.ClearLaneCenterOffsetAndNodeAddrAndLane();
			Vec::Vector_4V zero_4 = Vec::V4VConstant(V_ZERO);
			prevRoutePoint.m_vParamsAsVector = zero_4;

			pVeh->GetIntelligence()->AddScenarioHistoryPoint(prevRoutePoint);
		}

		CTaskUseVehicleScenario* pUseScenarioTask = NULL;

		// Check if we can use the task we have stored.
		aiTask* pStoredTask = m_pScenarioTask;
		if(pStoredTask && pStoredTask->GetTaskType() == CTaskTypes::TASK_USE_VEHICLE_SCENARIO)
		{
			CTaskUseVehicleScenario* pVehTask = static_cast<CTaskUseVehicleScenario*>(pStoredTask);
			// Note: we may want to run with these asserts, but I'm not confident that they will never fail, and we handle it anyway:
			//	if(taskVerifyf(pVehTask->GetScenarioPoint() == pt, "Scenario point mismatch with stored task.")
			//			&& taskVerifyf(pVehTask->GetScenarioType() ==  m_ScenarioFinder.GetRealScenarioPointType(), "Scenario type mismatch with stored task."))
			if(pVehTask->GetScenarioPoint() == pt && pVehTask->GetScenarioType() == m_ScenarioFinder.GetRealScenarioPointType())
			{
				// Assume ownership of the task.
				pUseScenarioTask = pVehTask;
				m_pScenarioTask = NULL;
			}
		}

		if(!pUseScenarioTask)
		{
			pUseScenarioTask = rage_new CTaskUseVehicleScenario(m_ScenarioFinder.GetRealScenarioPointType(), pt, flags, driveSpeed, pPrevPoint);
		}

		// Don't freeze when waiting on collision to load around the vehicle if it is a heli or a plane.
		// We are relying on the scenario designer to be placing routes in a reasonable way to avoid paths through buildings.
		if (pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane())
		{
			pVeh->SetAllowFreezeWaitingOnCollision(false);
			pVeh->m_nVehicleFlags.bShouldFixIfNoCollision = false;
		}

		SetNewTask(pUseScenarioTask);
	}	
	else
	{
		//At this point we are no longer using the chain so need to reset our usage of it
		CleanUpScenarioPointUsage();

		if ( (pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane() ) && pVeh->IsInAir() )
		{
			//
			// makes heli's and airplanes just fly off into space a million miles away
			//
			sVehicleMissionParams params;
			params.m_fCruiseSpeed = Max(40.0f, pVeh->pHandling->m_fEstimatedMaxFlatVel * 0.9f);
			params.SetTargetPosition(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()));
			params.m_fTargetArriveDist = -1.0f;
			params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);

			int iFlightHeight = CTaskVehicleFleeAirborne::ComputeFlightHeightAboveTerrain(*pVeh, 100);
			aiTask* pVehicleTask = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iFlightHeight, false, 0);
			aiTask* pSubTask = rage_new CTaskInVehicleBasic(pVeh);
			SetNewTask(rage_new CTaskControlVehicle(pVeh, pVehicleTask, pSubTask));

			pVeh->SetAllowFreezeWaitingOnCollision(false);
			pVeh->m_nVehicleFlags.bShouldFixIfNoCollision = false;

			pVeh->SwitchEngineOn(true);
			pVeh->m_nVehicleFlags.bDisableHeightMapAvoidance = false;

			// Most of the time when we get into this situation of not having a chain to follow,
			// we would probably want to be removed as soon as we can without the player noticing.
			// Otherwise, we can easily end up with more vehicles in the air than what we are supposed
			// to (particularly noticeable for the blimp).
			bool registerDistress = true;
			if(!pVeh->InheritsFromBlimp())
			{
				// Exception: if the player is flying and thus can easily follow, we may
				// not want to do this.
				const CPed* pPlayer = CGameWorld::FindLocalPlayer();
				if(pPlayer)
				{
					const CVehicle* pPlayerVehicle = pPlayer->GetVehiclePedInside();
					if(pPlayerVehicle)
					{
						if(pPlayerVehicle->GetIsAircraft() && pPlayerVehicle->IsInAir())
						{
							registerDistress = false;
						}
					}
				}
			}

			if(registerDistress)
			{
				RegisterDistress();
			}
		}
		else
		{
			if(!WillDefaultTasksBeGoodForVehicle(*pVeh))
			{
				RegisterDistress();
			}

			// If this is the root-most task
			if(GetParent() == NULL)
			{
				// allow the vehicle to use pretend occupants
				pVeh->m_nVehicleFlags.bDisablePretendOccupants = false;
				// request driver as it may be a restricted list of peds
				gPopStreaming.RequestVehicleDriver(pVeh->GetModelIndex());
			}

			// Nowhere to go, use the default driving task to join traffic and drive around randomly.
			// Note: we could also probably just quit CTaskUnalerted to get this effect, but by keeping this
			// task active, we can start experimenting with finding placed scenario points and driving towards
			// them (for parking, etc).
			CTask* pTask = pPed->ComputeDefaultDrivingTask(*pPed, pVeh, false);
			SetNewTask(pTask);
		}
	}

	// At this point, if we had a scenario task stored, we would have hopefully used
	// it above, if possible. If we still have a task, get rid of it, so we don't try
	// to use it later.
	delete m_pScenarioTask;
	m_pScenarioTask = NULL;
}


CTask::FSM_Return CTaskUnalerted::InVehicleAsDriver_OnUpdate()
{
	const CPed* pPed = GetPed();
	if(!pPed->GetIsInVehicle())
	{
		// Make sure we don't try to get back in. Some lower level task
		// probably kicked us out because some condition wasn't fulfilled,
		// and it is likely to happen again if we try to go back.
		m_bDontReturnToVehicle = true;

		m_ScenarioFinder.Reset();
		SetState(State_Start);
		return FSM_Continue;
	}

	const CScenarioPoint* pt = m_ScenarioFinder.GetScenarioPoint();
	if(pt)
	{
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_SearchingForNextScenarioInChain);
	}

	return FSM_Continue;
}

// PURPOSE:  Helper function for creating an in vehicle task for a passenger of a vehicle.
CTask* CTaskUnalerted::CreateTaskForPassenger(CVehicle& rVehicle)
{
	const CConditionalAnimsGroup* pGroup = GetDriverConditionalAnimsGroup(rVehicle);
	return rage_new CTaskInVehicleBasic(&rVehicle, false, pGroup);
}

CTask * CTaskUnalerted::SetupDrivingTaskForEmergencyVehicle(const CPed &rPedDriver, CVehicle& rVehicle, const bool bIsFiretruck, const bool bIsAmbulance)
{
	taskAssert(rPedDriver.GetMyVehicle() == &rVehicle);
	taskAssertf(!(bIsFiretruck && bIsAmbulance), "A vehicle can't be an ambulance and a firetruck can it?");
	DrivingFlags dm = DMode_StopForCars;
	const CScenarioPoint * pScenarioPt = rPedDriver.GetScenarioPoint(rPedDriver, true);
	bool bIsOnDuty = false;
	if (pScenarioPt)
	{
		bool bSirenOn = pScenarioPt->IsFlagSet(CScenarioPointFlags::ActivateVehicleSiren);
		rVehicle.TurnSirenOn(bSirenOn);
		bIsOnDuty = bSirenOn;
		if (pScenarioPt->IsFlagSet(CScenarioPointFlags::AggressiveVehicleDriving))
		{	
			dm = DMode_AvoidCars;
		}
	}
	else
	{
		rVehicle.TurnSirenOn(false);
	}

	if (bIsFiretruck)
	{
		rVehicle.m_nVehicleFlags.bIsFireTruckOnDuty = bIsOnDuty;
	}
	if (bIsAmbulance)
	{
		rVehicle.m_nVehicleFlags.bIsAmbulanceOnDuty = bIsOnDuty;
	}

	return rage_new CTaskCarDriveWander(&rVehicle, dm, CDriverPersonality::FindMaxCruiseSpeed(&rPedDriver, 
		&rVehicle, rVehicle.GetVehicleType() == VEHICLE_TYPE_BICYCLE), false);
}

// PURPOSE:  Helper function for grabbing the driver's conditional anims group (if any).
const CConditionalAnimsGroup* CTaskUnalerted::GetDriverConditionalAnimsGroup(const CVehicle& rVehicle)
{
	CPed* pDriver = rVehicle.GetDriver();
	if (pDriver)
	{
		CTaskUseVehicleScenario* pDriverTask = static_cast<CTaskUseVehicleScenario*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO));
		if (pDriverTask)
		{
			return pDriverTask->GetScenarioConditionalAnimsGroup();
		}
	}
	return NULL;
}

// PURPOSE: Helper function for checking if a scenario point can be reused.
// First the ped will try to reuse the real type passed in, if that fails it will try the remaining virtual types.
// If a new real type is selected then scenarioTypeReal will be updated accordingly.
bool CTaskUnalerted::CanScenarioBeReused(CScenarioPoint& rPoint, CScenarioCondition::sScenarioConditionData& conditionData, s32& scenarioTypeReal, CScenarioPointChainUseInfo* pExistingUserInfo)
{
	// First, check if the scenario can be reused with the existing real type.
	if (CanScenarioBeReusedWithCurrentRealType(rPoint, conditionData, scenarioTypeReal, pExistingUserInfo))
	{
		return true;
	}
	else
	{
		// The existing real type did not work out, but there could be another virtual type on that point that will.
		return CanScenarioBeReusedWithNewRealType(rPoint, conditionData, scenarioTypeReal, pExistingUserInfo);
	}
}

// PURPOSE: Helper function for checking if a scenario point and scenario real type can be reused
bool CTaskUnalerted::CanScenarioBeReusedWithCurrentRealType(CScenarioPoint& rPoint,
		CScenarioCondition::sScenarioConditionData& conditionData,
		s32 scenarioTypeReal,
		CScenarioPointChainUseInfo* pExistingUserInfo)
{
	// If the point is chained, and the chain is full, then we can't return to it.
	// If we did, there would be an assert about too many users on the chain.
	// There are probably a few possibilities here:
	// A. Somebody could have spawned on the chain after we left it, and now there isn't enough
	//    space to return to it.
	// B. We could possibly have a CScenarioPointChainUseInfo object and actually make the
	//    chain seem full ourselves.
	// C. The chain may still be considered full from our previous use of it.
	// Case A seems to be what happens most commonly in practice. Case B is protected against
	// by the !m_SPC_UseInfo condition below - that doesn't mean that it's necessarily completely
	// handled, but at least the new line below won't change anything.
	// Case C can probably happen if we arrived in a car and left the car parked in the chain.
	// To deal with that, we'll probably need to look through the chain users and see if we can
	// make use of one (that doesn't have another ped, and if it has a vehicle, it's "our" vehicle).
	// But even as is, it should still in that case prevent the assert, we just won't return to the
	// chain.
	if(!pExistingUserInfo && rPoint.IsChained() && rPoint.IsUsedChainFull())
	{
		return false;
	}

	const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioTypeReal);
	// Reject types without scenario infos - should we assert here?
	if (!pScenarioInfo)
	{
		return false;
	}

	if (rPoint.HasTimeOverride())
	{
		// Make sure the time is valid for the ped to use the point.
		// CheckScenarioTimesAndProbability already checks the primary condition of the scenario, so there is no need to check it twice here.
		if (!CScenarioManager::CheckScenarioTimesAndProbability(&rPoint, scenarioTypeReal, false, false))
		{
			return false;
		}
	}
	else
	{
		// Respect the primary condition of the scenario.
		if (pScenarioInfo->GetCondition() && !pScenarioInfo->GetCondition()->Check(conditionData))
		{
			return false;
		}
	}

	// Reject all vehicle scenario points.
	if (CScenarioManager::IsVehicleScenarioType(*pScenarioInfo))
	{
		return false;
	}

	// Explcitly block certain types of scenarios from being reused.
	if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::CantBeReused))
	{
		return false;
	}

	// Reject points that have been moved - they might have been tipped over.
	if (CScenarioManager::IsScenarioPointAttachedEntityUprootedOrBroken(rPoint))
	{
		return false;
	}

	Vec3V effectPosV = rPoint.GetPosition();
	const Vector3 effectPos = VEC3V_TO_VECTOR3(effectPosV);

	// Check if the point is inside a scenario blocking area.
	if (CScenarioBlockingAreas::IsPointInsideBlockingAreas(effectPos, true, false))
	{
		return false;
	}

	return true;
}

// PURPOSE: Check the point's other virtual types and see if any are valid for the ped to use.
// If there is, then set scenarioTypeReal to the new value.
bool CTaskUnalerted::CanScenarioBeReusedWithNewRealType(CScenarioPoint& rPoint, CScenarioCondition::sScenarioConditionData& conditionData,
														s32& scenarioTypeReal, CScenarioPointChainUseInfo* pExistingUserInfo)
{
	const CPed* pPed = conditionData.pPed;

	// Validate the condition struct's ped.
	if (!Verifyf(pPed, "Expected a valid ped in the condition data when selecting an alternative real type for reuse."))
	{
		return false;
	}

	const int scenarioTypeVirtualOrReal = rPoint.GetScenarioTypeVirtualOrReal();
	const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(scenarioTypeVirtualOrReal);
	taskAssert(numRealTypes <= CScenarioInfoManager::kMaxRealScenarioTypesPerPoint);
	int realTypesToPickFrom[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
	float probToPick[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
	int numRealTypesToPickFrom = 0;
	float probSum = 0.0f;

	// Iterate over the real types adding any that can be used to the types to pick from.
	for(int j = 0; j < numRealTypes; j++)
	{
		if(!CanScenarioBeReusedWithCurrentRealType(rPoint, conditionData, SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, j), pExistingUserInfo))
		{
			continue;
		}

		const float prob = SCENARIOINFOMGR.GetRealScenarioTypeProbability(scenarioTypeVirtualOrReal, j);
		realTypesToPickFrom[numRealTypesToPickFrom] = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, j);
		probToPick[numRealTypesToPickFrom] = prob;
		probSum += prob;
		numRealTypesToPickFrom++;
	}

	int chosenRealTypeIndex = CScenarioFinder::PickRealScenarioTypeFromGroup(realTypesToPickFrom, probToPick, numRealTypesToPickFrom, probSum);

	if (chosenRealTypeIndex < 0)
	{
		return false;
	}
	
	// Set the new real type.
	scenarioTypeReal = chosenRealTypeIndex;
	return true;
}

CScenarioPointChainUseInfo* CTaskUnalerted::GetScenarioChainUserData() const 
{ 
	return m_SPC_UseInfo; 
}

void CTaskUnalerted::SetScenarioChainUserInfo(CScenarioPointChainUseInfo* _user)
{
	Assertf(!m_SPC_UseInfo || !_user, "CScenarioPointChainUseInfo is already set even though we are trying to override it");
	m_SPC_UseInfo = _user;
	//Displayf("Transfer Chain User {%x}", m_SPC_UseInfo);
}

void CTaskUnalerted::InVehicleAsPassenger_OnEnter()
{
	const CPed* pPed = GetPed();
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	if(taskVerifyf(pVeh, "Expected vehicle."))
	{
		SetNewTask(CreateTaskForPassenger(*pVeh));
	}

	// Not sure if we should do this, but for now it seems best to let the driver
	// handle it all.
	m_ScenarioFinder.SetScenarioPoint(NULL, -1);
}


CTask::FSM_Return CTaskUnalerted::InVehicleAsPassenger_OnUpdate()
{
	CPed* pPed = GetPed();
	if(!pPed->GetIsInVehicle())
	{
		SetState(State_Start);
		return FSM_Continue;
	}
	else
	{
		// Periodically check if the driver has a different conditional anims group than the ped.
		m_fTimeUntilDriverAnimCheckS -= GetTimeStep();
		if (m_fTimeUntilDriverAnimCheckS < 0.0f)
		{
			CTaskInVehicleBasic* pSubtask = static_cast<CTaskInVehicleBasic*>(FindSubTaskOfType(CTaskTypes::TASK_IN_VEHICLE_BASIC));
			CVehicle* pVehicle = pPed->GetMyVehicle();
			const CConditionalAnimsGroup* pDriverAnimGroup = GetDriverConditionalAnimsGroup(*pVehicle);
			if (pDriverAnimGroup != NULL && pSubtask->GetConditionalAnimGroup() != pDriverAnimGroup)
			{
				// If so, then restart the current state to switch to the driver's conditional anim group.
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			// Reset the timer.
			m_fTimeUntilDriverAnimCheckS = sm_Tunables.m_TimeBeforeDriverAnimCheck;
		}
	}

	return FSM_Continue;
}

void CTaskUnalerted::ResumeAfterAbort_OnEnter()
{
	if (!IsPedInVehicle() && !GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::IgnorePavementCheckWhenLeavingScenarios))
	{
		StartPavementFloodFillRequest();
	}
}

CTask::FSM_Return CTaskUnalerted::ResumeAfterAbort_OnUpdate()
{
	CPed& ped = *GetPed();

	//Check if we should resume crossing a road
	if( ped.GetPedIntelligence() && ped.GetPedIntelligence()->ShouldResumeCrossingRoad() )
	{
		SetState(State_Wandering);
		return FSM_Continue;
	}

	// Wait until we know if there's pavement around before deciding to do anything. [10/9/2012 mdawe]
	bool bDoPavementCheck = !IsPedInVehicle() && !ped.GetTaskData().GetIsFlagSet(CTaskFlags::IgnorePavementCheckWhenLeavingScenarios);
	if (bDoPavementCheck && !ProcessPavementFloodFillRequest())
	{
		return FSM_Continue;
	}

	bool bReturningToOldScenario = false;

	//Update our point to return to.
	UpdatePointToReturnToAfterResume();
	
	//Check if we should holster our weapon.
	if(ShouldHolsterWeapon())
	{
		SetState(State_HolsteringWeapon);
	}
	//Check if we have a point to return to, we are not in a vehicle and our point to return to is not in use
	else if(m_PointToReturnTo && !ped.GetIsInVehicle() && !CPedPopulation::IsEffectInUse(*m_PointToReturnTo))
	{
		// initialize with default condition data
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = GetPed();

		// if we have enter anims and can return to the cached point
		if (CanScenarioBeReused(*m_PointToReturnTo, conditionData, m_PointToReturnToRealTypeIndex, m_SPC_UseInfo))
		{
			//Check to see if this scenario changes type on resume
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_PointToReturnToRealTypeIndex);
			if (pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ChangeToSafeTypeOnResume))
			{
				m_PointToReturnToRealTypeIndex = GetRandomScenarioTypeSafeForResume();
			}

			// set the scenario finder to use the PointToReturnTo info
			m_ScenarioFinder.SetScenarioPoint(m_PointToReturnTo, m_PointToReturnToRealTypeIndex);

			// clear our PointToReturnTo info
			m_PointToReturnTo = NULL;
			m_PointToReturnToRealTypeIndex = -1;

			// flag that we are returning to our old scenario
			bReturningToOldScenario = true;

			m_bScenarioReEntry = true;

			// start using the scenario
			SetState(State_UsingScenario);
		}
		else // The ped can't reuse their old scenario point (or doesn't have one)
		{
			if (m_bRoamMode)
			{
				bool bUseAquaticRoamMode = ped.GetTaskData().GetIsFlagSet(CTaskFlags::UseAquaticRoamMode);

				if (bUseAquaticRoamMode)
				{
					// This sort of breaks what we mean by "roam mode", but most of the time a fish that cannot resume 
					// its scenario after wandering is going to fail to find a new point immediately.
					// In this case, it's probably better to swim around in the wander state for a few frames so the
					// shark doesn't come to an abrupt halt.
					SetState(State_Wandering);		
				}
				else
				{
					// Roam peds cannot wander
					SetState(State_Start);
				}
			}
			else
			{
				// Aggressively delete this ped if they end up wandering without nearby pavements. [5/28/2013 mdawe]
				if (!m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn))
				{
					ped.SetRemoveAsSoonAsPossible(true);
				}

				// Set RF_SearchForAnyNonChainedScenarios to aggressively search for new scenario points to use.
				m_uRunningFlags.SetFlag(RF_SearchForAnyNonChainedScenarios);
				SetState(State_SearchingForScenario);
			}
		}
	}
	// The ped is not in a vehicle now, but they were.  Have them try to go back in it.
	// We'll probably be using the default tasks once we get there though, so don't get back
	// into a vehicle where that wouldn't work well.
	// Note: we could perhaps just go to State_Start here, since it already does some similar
	// checks to see if we should return to the vehicle.
	else if (!ped.GetIsInVehicle() && ped.GetMyVehicle()
			&& !m_bDontReturnToVehicle
			&& CheckCommonReturnToVehicleConditions(*ped.GetMyVehicle())
			&& WillDefaultTasksBeGoodForVehicle(*ped.GetMyVehicle()))
	{
		SetState(State_ReturningToVehicle);
	}
	else
	{
		SetState(State_Start);
	}

	// Don't turn off that the ped should use stationary reactions in the case that the ped is returning to their old point.
	// They likely got this flag turned on from that point, so if they get an event in between now and the time when they are back in their point
	// they will try and run when we don't want them to.
	if (!bReturningToOldScenario)
	{
		CTaskUseScenario::SetStationaryReactionsEnabledForPed(ped, false);
		m_bDoCowardScenarioResume = false;
	}

	return FSM_Continue;
}

void CTaskUnalerted::ResumeAfterAbort_OnExit()
{
	CleanupPavementFloodFillRequest();
}

void CTaskUnalerted::HolsteringWeapon_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Grab the weapon manager.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	taskAssert(pWeaponManager);

	//Equip the unarmed weapon.
	if(!pWeaponManager->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash()))
	{
		//Add a warning.
		taskWarningf("Failed to holster due to unable to equip unarmed weapon.");

		//Disable holster weapon.
		m_uRunningFlags.SetFlag(RF_DisableHolsterWeapon);

		return;
	}

	// For B* 888036: "Ignorable assert TaskUnalerted.cpp(1442): [ai_task] Error:
	// Assertf(0) FAILED: Failed to holster a weapon (WEAPON_STUNGUN), CTaskUnalerted
	// may not be working properly.", there seemed to be an issue where CTaskMotionPed::Swimming_OnExit() 
	// called RestoreBackupWeapon(), which gave the weapon back to the ped and made the ShouldHolsterWeapon()
	// assert in HolsteringWeapon_OnUpdate() fail. Now, if we are swimming, we should probably set
	// the backup weapon to unarmed as well, and may be best to not try to run CTaskSwapWeapon. Seems to
	// fix the assert. All that is a pretty obscure case happening because we are aborting the mission,
	// we are not really meant to use CTaskUnalerted for diving.
	if(pPed->GetIsSwimming()
			// Possibly should do these as well, but holding off until there is a need, so we can test it properly:
			// || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing)
			// || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)
			)
	{
		pWeaponManager->SetBackupWeapon(pPed->GetDefaultUnarmedWeaponHash());
		return;
	}

	//Create the task.
	CTask* pTask = rage_new CTaskSwapWeapon(SWAP_HOLSTER);

	//Start the task.
	SetNewTask(pTask);
}


CTask::FSM_Return CTaskUnalerted::HolsteringWeapon_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
#if __ASSERT
		// It would be bad if we failed to holster as we would probably try again
		// (and fail?) right away, so if that happens, trigger an informative assert.
		if(ShouldHolsterWeapon())
		{
			const char* pWeapName = NULL;
			const CPed* pPed = GetPed();
			const CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
			if(pWeapMgr)
			{
				atHashString str(pWeapMgr->GetEquippedWeaponHash());
				pWeapName = str.GetCStr();
			}
			taskAssertf(0, "Failed to holster a weapon (%s), CTaskUnalerted may not be working properly.", pWeapName ? pWeapName : "?");
		}
#endif

		//Check if we were resuming this task.
		if(GetPreviousState() == State_ResumeAfterAbort)
		{
			//Continue to resume the task.
			SetState(State_ResumeAfterAbort);
		}
		else
		{
			//Move to the start state.
			SetState(State_Start);
		}
		
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CTaskUnalerted::ExitingVehicle_OnEnter()
{
	CPed* pPed = GetPed();

	// If the Ped is exiting the vehicle, and the Ped is the driver.
	if (pPed->GetIsDrivingVehicle())
	{
		// get a handle to myVehicle
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			// turn off the engine since the Ped is getting out of the car
			pVehicle->SwitchEngineOff();

			// turn off lights
			pVehicle->m_nVehicleFlags.bLightsOn = false;

			// flag the vehicle to be parked
			pVehicle->m_nVehicleFlags.bIsThisAParkedCar = true;

			// set the PopType to parked
			pVehicle->PopTypeSet(POPTYPE_RANDOM_PARKED);

			//reset scenario chain history here
			pVehicle->GetIntelligence()->DeleteScenarioPointHistory();
		}
	}

	VehicleEnterExitFlags vehicleFlags;
	SetNewTask(rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags));
}


CTask::FSM_Return CTaskUnalerted::ExitingVehicle_OnUpdate()
{
	CTask* pSubTask = GetSubTask();
	if(!pSubTask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//If the Exit Vehicle subtask failed add a short delay for vehicles as the PickVehicleState function will result in 
		// selecting the ExitingVehicle state even though it just failed this frame. This delay is being done to allow 
		// some time between state switches so we dont end up looping the task states in a way that results in a possible
		// infinite loop. (ie if the doors are blocked and the ped cant get out of the vehicle)
		if(GetPed()->GetIsInVehicle())
		{
			if(GetTimeInState() <= sm_Tunables.m_WaitTimeAfterFailedVehExit)
			{
				return FSM_Continue;
			}
		}

		if(m_ScenarioFinder.GetScenarioPoint())
		{
			if(GetPed()->GetIsInVehicle())
			{
				PickVehicleState();
			}
			else
			{
				PickUseScenarioState();
			}
			return FSM_Continue;
		}
		else
		{
			SetState(State_Start);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}


void CTaskUnalerted::ReturningToVehicle_OnEnter()
{
	const CPed* pPed = GetPed();

	VehicleEnterExitFlags flags;
	flags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);

	const float mbr = MOVEBLENDRATIO_WALK;

	CTaskEnterVehicle* pTask = rage_new CTaskEnterVehicle(pPed->GetMyVehicle(), SR_Specific, pPed->GetMyVehicle()->GetDriverSeat(), flags, 0.0f, mbr);
	SetNewTask(pTask);

	// Should the ped open doors between the last used point and vehicle?
	if (m_LastUsedPoint && m_LastUsedPoint->GetOpenDoor())
	{
		// Turn on our arm IK flag
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, true);
	}
}


CTask::FSM_Return CTaskUnalerted::ReturningToVehicle_OnUpdate()
{
	CPed* pPed = GetPed();

	// Should the ped open doors between last use point and vehicle?
	if (m_LastUsedPoint && m_LastUsedPoint->GetOpenDoor())
	{
		// Update our search for doors flag
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDoors, true);
	}

	CTask* pSubTask = GetSubTask();
	if(!pSubTask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPed->GetIsInVehicle())
		{
			PickVehicleState();
			return FSM_Continue;
		}
		SetState(State_Start);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskUnalerted::ReturningToVehicle_OnExit()
{
	// Turn off our arm IK flag
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, false);
}

CTask::FSM_Return CTaskUnalerted::WaitForVehicleLand_OnUpdate()
{
	CPed* pPed = GetPed();
	aiAssert(pPed);
	CVehicle* pVeh = pPed->GetMyVehicle();
	aiAssert(pVeh);

	if (CTaskUnalerted::HasVehicleLanded(*pVeh))
	{
		SetState(State_ExitingVehicle);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskUnalerted::WanderingWait_OnUpdate()
{
	if (m_nFrameWanderDelay > 0)
	{
		--m_nFrameWanderDelay;
		return FSM_Continue;
	}

	SetState(State_Wandering);
	return FSM_Continue;
}

void CTaskUnalerted::MoveOffToOblivion_OnEnter()
{
	CPed* pPed = GetPed();

	// Flee forever away from your current position.
	CAITarget fleeTarget;
	Vector3 vPositionToFlee(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	fleeTarget.SetPosition(vPositionToFlee);

	CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(fleeTarget);
	pFleeTask->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
	
	// If the ped can enter water but doesn't prefer to avoid it, and is in water, then the path should stay in water.
	CPedNavCapabilityInfo& navCapabilities = pPed->GetPedIntelligence()->GetNavCapabilities();
	if (pPed->GetIsInWater() || (pPed->GetIsSwimming() && 
		(navCapabilities.IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_ENTER_WATER) && !CPedNavCapabilityInfo::FLAG_PREFER_TO_AVOID_WATER)))
	{
		pFleeTask->GetConfigFlags().SetFlag(CTaskSmartFlee::CF_NeverLeaveWater | CTaskSmartFlee::CF_FleeAtCurrentHeight | CTaskSmartFlee::CF_NeverLeaveDeepWater);
	}

	SetNewTask(pFleeTask);
}

CTask::FSM_Return CTaskUnalerted::MoveOffToOblivion_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_SMART_FLEE))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	return FSM_Continue;
}

void CTaskUnalerted::CleanUpScenarioPointUsage()
{
	if (m_SPC_UseInfo)
	{
		CPed* pPed = GetPed();
		CVehicle* pVehicleIn = NULL;
		if (pPed)
		{
			pVehicleIn = pPed->GetVehiclePedInside();
		}
		
		CScenarioPointChainUseInfo* pUser = m_SPC_UseInfo;
		m_SPC_UseInfo = NULL;

		//If we are in a vehicle and that vehicle is the one hour chain is tracking 
		// then reset it.
		if (pVehicleIn && pUser->GetVehicle() == pVehicleIn)
		{
			pUser->ResetVehicle();
		}
		pUser->ResetPed();

		if (pVehicleIn)
		{
			pVehicleIn->GetIntelligence()->DeleteScenarioPointHistory();
		}

		//Displayf("Clean Up Chain User {%x} P {%x}, V {%x}", m_SPC_UseInfo, pPed, pVeh);
	}
}

void CTaskUnalerted::StartPavementFloodFillRequest()
{
	m_PavementHelper.StartPavementFloodFillRequest(GetPed());
	if (m_PavementHelper.HasPavement())
	{
		m_uRunningFlags.SetFlag(RF_HasNearbyPavementToWanderOn);
	}
	else
	{
		m_uRunningFlags.ClearFlag(RF_HasNearbyPavementToWanderOn);
	}
}

bool CTaskUnalerted::ProcessPavementFloodFillRequest()
{
	m_PavementHelper.UpdatePavementFloodFillRequest(GetPed());
	if (m_PavementHelper.HasPavement())
	{
		m_uRunningFlags.SetFlag(RF_HasNearbyPavementToWanderOn);
		return true;
	}
	else if (m_PavementHelper.HasNoPavement())
	{
		m_uRunningFlags.ClearFlag(RF_HasNearbyPavementToWanderOn);
		return true; // search finished successfully with no pavement found
	}
	else
	{
		// Search still going.
		return false;
	}
}

void CTaskUnalerted::CleanupPavementFloodFillRequest()
{
	m_PavementHelper.CleanupPavementFloodFillRequest();
}

void CTaskUnalerted::StartScenarioFinderSearch()
{
	taskAssert(!m_uRunningFlags.IsFlagSet(RF_HasStartedScenarioFinder));

	CScenarioFinder::FindOptions opts;

	// To match what AttractPedsToScenarioPointList() used to do, and to give
	// scenarios a chance to only be used for spawning and not to attract existing
	// peds, we require the WillAttractPeds flag to be set.
	// Note: CTaskRoam didn't require this, so we don't do it if m_bRoamMode is set.
	if(!m_bRoamMode)
	{
		opts.m_RequiredScenarioInfoFlags.BitSet().Set(CScenarioInfoFlags::WillAttractPeds);
	}
	else
	{
		//Roam mode allows for peds to use "nospawn" points as valid locations to navigate to
		opts.m_bDontIgnoreNoSpawnPoints = true;
	}

	// We don't want to find the point we just came from.
	if(m_LastUsedPoint)
	{
		// Check the time, as if this scenario is really the only one we can use,
		// we would probably want to eventually get back to it.
		u32 timeSinceLastUsed = fwTimer::GetTimeInMilliseconds() - m_LastUsedPointTimeMs;
		// Also, if the last used point is your current point (totally possible if the ped spawned there), skip it regardless of how much time has elapsed.
		if(m_LastUsedPoint == GetScenarioPoint() || timeSinceLastUsed <= (u32)(sm_Tunables.m_TimeMinBeforeLastPoint*1000.0f))
		{
			// If there's nearby pavement, exclude the scenario we were just using.
			// Otherwise, we'd rather just start our last scenario again. [10/4/2012 mdawe]
			if (m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn))
			{
				opts.m_IgnorePoint = m_LastUsedPoint;
			}
		}
	}

	// We dont want to find the same type we just came from.
	if (m_LastUsedPointRealTypeIndex != -1)
	{
		//Check to see if the last used scenario cares if we use another of that type within the time span
		const CScenarioInfo* info = SCENARIOINFOMGR.GetScenarioInfoByIndex(m_LastUsedPointRealTypeIndex);
		if(taskVerifyf(info, "Unable to find scenario info for index %u", m_LastUsedPointRealTypeIndex))
		{
			if (info->GetIsFlagSet(CScenarioInfoFlags::UseLastTypeUsageTimer))
			{
				// Check the time, as if this scenario is really the only one we can use,
				// we would probably want to eventually get back to it.
				u32 timeSinceLastUsed = fwTimer::GetTimeInMilliseconds() - m_LastUsedPointTimeMs;
				if(timeSinceLastUsed <= (u32)(sm_Tunables.m_TimeMinBeforeLastPointType*1000.0f))
				{
					opts.m_sIgnoreScenarioType = m_LastUsedPointRealTypeIndex;
				}
			}
		}
	}

	// We probably wouldn't want to find scenarios that are a part of a chain, but
	// not at the beginning of it, and start using those.
	opts.m_NoChainingNodesWithIncomingLinks = true;

	// This prevents us from finding scenarios that need props from the environment,
	// if those props don't exist. If we don't do this, CTaskUseScenario should still
	// quit, but there would probably be a visible interruption in the task execution.
	opts.m_bFindPropsInEnvironment = true;

	// This prevents us from finding scenarios that are behind the ped resulting in the 
	// ped having walked past the scenario then turning and going back to the scenario
	// which tends to look unnatural.
	// @RSGNWE-JW if we are roaming, allow the peds to look behind them for a new place to go to.
	opts.m_bIgnorePointsBehindPed = !m_bRoamMode;

	// For being attracted to scenarios, we should probably respect the probability in
	// the scenario point/type.
	opts.m_bCheckProbability = true;

	// When navigating towards the point we potentially care about the angle of approach.
	opts.m_bCheckApproachAngle = true;

	if (m_uRunningFlags.IsFlagSet(RF_SearchForAnyNonChainedScenarios))
	{
		opts.m_RequiredScenarioInfoFlags.BitSet().Clear(CScenarioInfoFlags::WillAttractPeds);
		opts.m_bExcludeChainNodes = true;
		opts.m_bDontIgnoreNoSpawnPoints = true;
		opts.m_bIgnorePointsBehindPed = false;
		opts.m_bMustBeReusable = true;
		opts.m_bMustNotHaveStationaryReactionsEnabled = true;
	}

	m_ScenarioFinder.StartSearch(*GetPed(), opts);

	m_uRunningFlags.SetFlag(RF_HasStartedScenarioFinder);
}

bool CTaskUnalerted::IsPedInVehicle() const
{
	return GetPed()->GetIsInVehicle();
}

bool CTaskUnalerted::HasVehicleLanded(const CVehicle& vehicle)
{
	// Don't exit an aircraft, unless its landed
	if (vehicle.GetIsAircraft())
	{
		if (vehicle.InheritsFromHeli())
		{
			//Check if the heli is landed.
			const CHeli* pHeli = static_cast<const CHeli *>(&vehicle);
			if(pHeli->GetHeliIntelligence()->IsLanded())
			{
				static float s_fMinTimeLanded = 0.5f;
				if(pHeli->GetHeliIntelligence()->GetTimeSpentLanded() < s_fMinTimeLanded)
				{
					//delay leaving based on time we have been landed.
					return false;
				}
			}
		}

		if (vehicle.IsEngineOn())
		{
			//engine should be turned off ... 
			return false;
		}

		if (!vehicle.HasContactWheels())
		{
			//wheels dont touch the ground
			return false;
		}

		if (vehicle.GetVelocity().Mag2() > rage::square(1.0f))
		{
			//still moving 
			return false;
		}
	}
	return true;
}

void CTaskUnalerted::MakeScenarioTaskNotLeave()
{
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	if(pSubTask->GetTaskType() != CTaskTypes::TASK_USE_SCENARIO)
	{
		return;
	}

	CTaskUseScenario* pUseScenarioTask = static_cast<CTaskUseScenario*>(pSubTask);
	pUseScenarioTask->SetDontLeaveThisFrame();
}


bool CTaskUnalerted::MaySearchForScenarios() const
{
	// For now, we don't try to use scenarios if we are carrying a prop,
	// which is how CPedPopulation::AttractPedsToScenarioPointList() worked.
	const CPed* pPed = GetPed();
	const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr && pWeaponMgr->GetEquippedWeaponObject())
	{
		return false;
	}

	// If we have an active crossing subtask
	if( pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS) )
	{
		// don't interrupt it for an attractor scenario
		return false;
	}

	return true;
}


void CTaskUnalerted::SetTimeUntilCheckScenario(float delay)
{
	const u32 delayMs = (u32)(delay*1000.0f);
	m_TimeUntilCheckScenarioMs = Max(m_TimeUntilCheckScenarioMs, delayMs);
}


void CTaskUnalerted::SetTimeUntilCheckScenario(float minDelay, float maxDelay)
{
	SetTimeUntilCheckScenario(g_ReplayRand.GetRanged(minDelay, maxDelay));
}


void CTaskUnalerted::PickUseScenarioState()
{
	CScenarioPoint* pt = m_ScenarioFinder.GetScenarioPoint();
	if(pt)
	{
		const CPed* pPed = GetPed();
		if(CScenarioManager::IsVehicleScenarioType(m_ScenarioFinder.GetRealScenarioPointType()))
		{
			if(!pPed->GetIsInVehicle())
			{
				if(CanReturnToVehicle())
				{
					SetState(State_ReturningToVehicle);
					return;
				}
				else
				{
					m_ScenarioFinder.SetScenarioPoint(NULL, -1);
					SetState(State_Start);
					return;
				}
			}
		}
	}

	SetState(State_UsingScenario);
}


void CTaskUnalerted::PickVehicleState()
{
	m_bDontReturnToVehicle = false;

	const CPed* pPed = GetPed();
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	if(taskVerifyf(pVeh, "Expected vehicle"))
	{
		if(pPed->GetIsDrivingVehicle())
		{
			CScenarioPoint* pt = m_ScenarioFinder.GetScenarioPoint();
			if(pt)
			{
				if(!CScenarioManager::IsVehicleScenarioType(m_ScenarioFinder.GetRealScenarioPointType()))
				{
					if (pVeh->InheritsFromHeli())
					{
						SetState(State_WaitForVehicleLand);
					}
					else
					{
						SetState(State_ExitingVehicle);
					}
					return;
				}
			}


			SetState(State_InVehicleAsDriver);
		}
		else
		{
			SetState(State_InVehicleAsPassenger);
		}
	}
	else
	{
		if(GetState() != State_Start)
		{
			SetState(State_Start);
		}
	}
}


void CTaskUnalerted::PickWanderState()
{
	// If we have a weapon we should holster, go to the HolsteringWeapon state. From there,
	// we'll probably go back to State_Start.
	if(ShouldHolsterWeapon())
	{
		SetState(State_HolsteringWeapon);
	}
	else if (m_nFrameWanderDelay > 0)
	{
		SetState(State_WanderingWait);
	}
	else
	{
		SetState(State_Wandering);
	}
}


bool CTaskUnalerted::CheckCommonReturnToVehicleConditions(const CVehicle& veh) const
{
	//Ensure the vehicle has no driver.
	if(veh.GetDriver())
	{
		return false;
	}
	
	if(!veh.IsInDriveableState())
	{
		// The vehicle is not in usable condition, we probably shouldn't try to return to it.
		return false;
	}

	// Can't return to boats. If it was on land, we'd immediately leave because it's undriveable. 
	// If it's in water, we'd immediately get a task to get out of the water.  [6/22/2013 mdawe]
	if (veh.InheritsFromBoat())
	{
		return false;
	}

	if(!veh.CanPedEnterCar(GetPed()))
	{
		return false;
	}

	if (NetworkInterface::IsInvalidVehicleForDriving(GetPed(), &veh))
	{
		return false;
	}

	return true;
}


bool CTaskUnalerted::CanReturnToVehicle() const
{
	const CPed* pPed = GetPed();
	if(pPed->GetIsInVehicle() || pPed->GetIsEnteringVehicle())
	{
		return false;
	}

	const CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}

	if(!CheckCommonReturnToVehicleConditions(*pVehicle))
	{
		return false;
	}

	return true;
}

bool CTaskUnalerted::CloseEnoughForHangoutTransition() const
{
	// Bail out if there isn't a last used point.
	if (!m_LastUsedPoint)
	{
		return false;
	}

	const CPed* pPed = GetPed();
	Vec3V vPoint = m_LastUsedPoint->GetWorldPosition();
	Vec3V vPed = pPed->GetTransform().GetPosition();
	const CTaskUseScenario::Tunables& scenarioTunables = CTaskUseScenario::GetTunables();

	// Check to make sure the ped's heading is close enough.
	// IgnoreScenarioPointHeading and prop scenario points aren't handled here, but none of the transition scenarios have those settings.
	const float fPointHeading = m_LastUsedPoint->GetDirectionf();
	if (fabs(SubtractAngleShorter(pPed->GetCurrentHeading(), fPointHeading) * RtoD) > scenarioTunables.m_SkipGotoHeadingDeltaDegrees)
	{
		return false;
	}

	// Check to make sure the ped is close enough along the Z axis.
	if (ABS(vPoint.GetZf() - vPed.GetZf()) > scenarioTunables.m_SkipGotoZDist)
	{
		return false;
	}

	// Check to make sure the ped is close enough in the XY plane.
	vPoint.SetZ(ScalarV(V_ZERO));
	vPed.SetZ(ScalarV(V_ZERO));
	if (IsGreaterThanAll(DistSquared(vPoint, vPed), ScalarV(scenarioTunables.m_SkipGotoXYDist * scenarioTunables.m_SkipGotoXYDist)))
	{
		return false;
	}

	// Fine for a transition.
	return true;
}


bool CTaskUnalerted::ShouldHolsterWeapon() const
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DisableHolsterWeapon))
	{
		return false;
	}

	const CPed* pPed = GetPed();
	if(pPed->GetIsInVehicle())
	{
		// Don't do anything if we are in a vehicle for now, if we happen to get here.
		return false;
	}
	const CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
	if(!pWeapMgr)
	{
		return false;
	}
	
	u32 weaponHash = pWeapMgr->GetEquippedWeaponHash();
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
	{
		// if the ped has a weapon equipped from its loadout defined in CAmbientPedModelVariations
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_EquippedAmbientLoadOutWeapon))
		{
			// don't holster the weapon
			return false;
		}

		const CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
		if(pPedModelInfo)
		{
			const CPedModelInfo::PersonalityData& personalityData = pPedModelInfo->GetPersonalitySettings();
			if(CPedInventoryLoadOutManager::LoadOutShouldEquipWeapon(personalityData.GetDefaultWeaponLoadoutHash(), weaponHash))
			{
				// This is a weapon that's tagged as being equipped in the default loadout, so we
				// probably shouldn't holster it (otherwise, the feature to equip a weapon through
				// the loadout probably wouldn't be very useful).
				return false;
			}
		}

		return true;
	}

	return false;
}

bool CTaskUnalerted::ShouldReturnToLastUsedScenarioPointDueToAgitated() const
{
	//Ensure we aren't agitated.
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return false;
	}

	//Ensure we left our last used scenario point due to agitated.
	if(!GetPed()->GetPedIntelligence()->GetLastUsedScenarioFlags().IsFlagSet(CPedIntelligence::LUSF_ExitedDueToAgitated))
	{
		return false;
	}

	//Ensure the last used scenario point is valid.
	const CScenarioPoint* pLastUsedScenarioPoint = GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint();
	if(!pLastUsedScenarioPoint)
	{
		return false;
	}

	return true;
}

bool CTaskUnalerted::ShouldReturnToLastUsedVehicle() const
{
	if(m_bDontReturnToVehicle)
	{
		return false;
	}

	// Don't want to get caught in an infinite loop.
	// If needed we could try a max number of retries
	if (GetPreviousState() == State_ReturningToVehicle)
	{
		return false;
	}

	//Ensure we are on foot.
	if(!GetPed()->GetIsOnFoot())
	{
		return false;
	}

	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}

	if(!CheckCommonReturnToVehicleConditions(*pVehicle))
	{
		return false;
	}

	//Ensure the vehicle is close.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pVehicle->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceToReturnToLastUsedVehicle));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	// Don't re-enter certain types of vehicles, for which we may not have good default
	// behavior if we are not on a chain.
	if(!WillDefaultTasksBeGoodForVehicle(*pVehicle))
	{
		return false;
	}

	if(NetworkInterface::IsGameInProgress() && GetState() == State_Start && !GetPed()->IsNetworkClone() && m_bDontReturnToVehicleAtMigrate)
	{
		return false;
	}

	return true;
}

bool CTaskUnalerted::WillDefaultTasksBeGoodForVehicle(const CVehicle& veh) const
{
	// For airplanes, the default tasks are problematic, and will basically just make
	// us sit still and idle.
	if(veh.InheritsFromPlane())
	{
		return false;
	}

	return true;
}

void CTaskUnalerted::UpdatePointToReturnToAfterResume()
{
	//Ensure we don't have a point to return to.
	if(m_PointToReturnTo)
	{
		return;
	}

	// Check if something happened to the ped that we would never want to resume from.
	if(m_bNeverReturnToLastPoint)
	{
		// Now that we've made the decision, reset this flag.
		m_bNeverReturnToLastPoint= false;

		// We never want to return to this point, ever.
		if (m_PointToReturnTo)
		{
			m_PointToReturnTo->SetObstructed();
			m_PointToReturnToRealTypeIndex = Scenario_Invalid;
		}

		return;
	}

	//Check if we should return to our last used scenario point due to agitated.
	if(ShouldReturnToLastUsedScenarioPointDueToAgitated() || !m_uRunningFlags.IsFlagSet(RF_HasNearbyPavementToWanderOn))
	{
		//Set the point to return to.
		m_PointToReturnTo				= GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint();
		m_PointToReturnToRealTypeIndex	= GetPed()->GetPedIntelligence()->GetLastUsedScenarioPointType();
	}
}

void CTaskUnalerted::ProcessKeepMovingWhenFindingWanderPath()
{
	//Ensure the flag has not been set.
	if(m_bKeepMovingWhenFindingWanderPath)
	{
		return;
	}
	
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the sub-task is a scenario.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_USE_SCENARIO)
	{
		return;
	}
	
	//Grab the task.
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pSubTask);
	
	//Ensure the task is in the correct state.
	if(pTask->GetState() != CTaskUseScenario::State_Exit)
	{
		return;
	}

	//Ensure the exit clips are determined.
	if(!pTask->GetExitClipsDetermined())
	{
		return;
	}
	
	//Ensure the exit clips ends in a walk.
	if(!pTask->GetExitClipsEndsInWalk())
	{
		return;
	}
	
	//Set the flag.
	m_bKeepMovingWhenFindingWanderPath = true;
}

bool CTaskUnalerted::ShouldWaitWhileSearchingForNextScenarioInChain() const
{
	//Assert that we are in the correct state.
	taskAssertf(GetState() == State_SearchingForNextScenarioInChain, "The state is invalid: %d.", GetState());
	
	//Ensure the scenario finder is not active.
	if(m_ScenarioFinder.IsActive())
	{
		return false;
	}
	
	//Ensure the scenario finder failed.
	if(m_ScenarioFinder.GetScenarioPoint())
	{
		return false;
	}
	
	//Ensure a chained scenario may become valid shortly.
	if(!m_ScenarioFinder.GetInformationFlags().IsFlagSet(CScenarioFinder::IF_ChainedScenarioMayBecomeValidSoon))
	{
		return false;
	}
	
	return true;
}

void CTaskUnalerted::StartSearchForNextScenarioInChain()
{
	//Assert that a search is not active.
	taskAssertf(!m_ScenarioFinder.IsActive(), "The scenario finder is active.");
	
	//Assert that we are in the correct state.
	taskAssertf(GetState() == State_SearchingForNextScenarioInChain, "The state is invalid: %d.", GetState());
	
	CScenarioFinder::FindOptions opts;

	// Note: we intentionally don't require WillAttractPeds on scenarios
	// that are chained to.

	// This prevents us from finding scenarios that need props from the environment,
	// if those props don't exist. If we don't do this, CTaskUseScenario should still
	// quit, but there would probably be a visible interruption in the task execution.
	opts.m_bFindPropsInEnvironment = true;

	//Check if we should relax the constraints.
	if(m_uRunningFlags.IsFlagSet(RF_RelaxNextScenarioInChainConstraints))
	{
		//Ignore the time.
		opts.m_bIgnoreTime = true;
	}

	// always allow Peds to use nospawn points when looking for the next point
	opts.m_bDontIgnoreNoSpawnPoints = true;

	// FindScenario() is going to check for a matching conditional anim set which potentially will check if the ped is holding the right prop.
	// However, the ped is going to get the prop from using the point.
	opts.m_bIgnoreHasPropCheck = true;

	m_ScenarioFinder.StartSearchForNextInChain(*GetPed(), opts);
	
	//Clear the time since the last search.
	m_fTimeSinceLastSearchForNextScenarioInChain = 0.0f;

	m_NextPointTestedForBlockingAreas = false;
}

bool CTaskUnalerted::ShouldEndScenarioBecauseOfNearbyPlayer() const
{
	CScenarioPoint* pPoint = GetScenarioPoint();

	if (pPoint)
	{
		if (pPoint->IsFlagSet(CScenarioPointFlags::EndScenarioIfPlayerWithinRadius))
		{
			Vec3V vPoint = pPoint->GetWorldPosition();
			float fRadius = pPoint->GetRadius();
			
			if (NetworkInterface::IsGameInProgress())
			{
				const netPlayer* pClosest = NULL;
				return NetworkInterface::IsCloseToAnyPlayer(VEC3V_TO_VECTOR3(vPoint), fRadius, pClosest);
			}
			else
			{
				CPed* pPlayer = CGameWorld::FindLocalPlayer();
				if (pPlayer)
				{
					Vec3V vPlayer = pPlayer->GetTransform().GetPosition();
					if (IsLessThanAll(DistSquared(vPlayer, vPoint), ScalarV(fRadius * fRadius)))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void CTaskUnalerted::EndActiveScenario()
{
	CTask* pSubTask = GetSubTask();

	if (pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO || pSubTask->GetTaskType() == CTaskTypes::TASK_USE_VEHICLE_SCENARIO)
	{
		CTaskScenario* pTaskScenario = static_cast<CTaskScenario*>(pSubTask);
		pTaskScenario->SetTimeToLeave(0.0f);
	}
}

bool CTaskUnalerted::HandleEndVehicleScenarioBecauseOfBlockingArea(const CScenarioPoint& pt)
{
	const CPed* pPed = GetPed();
	taskAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();

	bool tryToRemove = pVeh->m_nVehicleFlags.bTryToRemoveAggressively;
	if(!tryToRemove)
	{
		Vec3V posV = pt.GetPosition();
		if(CScenarioBlockingAreas::ShouldScenarioQuitFromBlockingAreas(RCC_VECTOR3(posV), false, true))
		{
			tryToRemove = true;

			// Set the flag to try to remove this vehicle more aggressively than normal,
			// since it's supposed to go to a point that inside of a blocking area.
			RegisterDistress();
		}
	}

	if(tryToRemove)
	{
		// If it's an airborne plane or helicopter, stop using scenarios. We can help
		// to get rid of those by making them fly away. Would be good to do something
		// similar on ground, but it has to be done with care since it's not always
		// the case that a scenario vehicle can make it out safely without the guidance
		// of a chain.
		if((pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane()) && pVeh->IsInAir())
		{
			return true;
		}
	}

	return false;
}


void CTaskUnalerted::RegisterDistress()
{
	// Don't touch anything mission-created or networked.
	CPed* pPed = GetPed();
	if(pPed->PopTypeIsMission() || pPed->IsNetworkClone())
	{
		return;
	}

	CVehicle* pVehicle = pPed->GetVehiclePedInside();

	// If we are not in a vehicle, mark the ped to be removed.
	if(!pVehicle)
	{
		pPed->SetRemoveAsSoonAsPossible(true);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster, false);	// Note: questionable, but currently necessary.

		// If we arrived in a vehicle, we may still have a pointer to it. Probably
		// best to try to remove that as well, so we don't get an empty car left behind.
		pVehicle = pPed->GetMyVehicle();

		// Only do it if it doesn't have a driver, though.
		if(pVehicle && pVehicle->GetDriver())
		{
			pVehicle = NULL;
		}
	}

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bTryToRemoveAggressively = true;
		pVehicle->m_nVehicleFlags.bIsInCluster = false;					// Note: questionable, but currently necessary.
	}
}


void CTaskUnalerted::SetTargetToWanderAwayFrom( Vector3& vTarget )
{
	m_uRunningFlags.ChangeFlag(RF_WanderAwayFromSpecifiedTarget, true); 
	m_TargetToInitiallyWanderAwayFrom = vTarget;
	m_timeTargetSet = fwTimer::GetTimeInMilliseconds();
}

void CTaskUnalerted::ClearScenarioPointToReturnTo()
{
	m_PointToReturnTo = NULL;
	m_PointToReturnToRealTypeIndex = -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Clone implementation
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CTaskInfo* CTaskUnalerted::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	const CTask* pSubtask = GetSubTask();
	m_bScenarioCowering = false;

	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		const CTaskUseScenario* pTask = static_cast<const CTaskUseScenario *>(pSubtask);
		if(pTask->GetState() == CTaskUseScenario::State_WaitingOnExit)
		{
			m_bScenarioCowering = true;
		}
	}

	pInfo =  rage_new CClonedTaskUnalertedInfo(GetState(), m_bHasScenarioChainUser, m_bScenarioCowering);

	return pInfo;
}

void CTaskUnalerted::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_UNALERTED);
	CClonedTaskUnalertedInfo *pUnalertedInfo = static_cast<CClonedTaskUnalertedInfo*>(pTaskInfo);

	m_bHasScenarioChainUser = pUnalertedInfo->GetHasScenarioChainUser();
	m_bScenarioCowering		= pUnalertedInfo->GetIsCowering();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskUnalerted::UpdateClonedFSM(const s32 UNUSED_PARAM(iState), const FSM_Event iEvent)
{
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

	if(iEvent == OnUpdate)
	{
		UpdateClonedSubTasks(GetPed(), GetState());
	}

	return FSM_Continue;
}

void CTaskUnalerted::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}


bool CTaskUnalerted::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if( GetState() == State_HolsteringWeapon	||
		GetState() == State_ExitingVehicle		||
		GetState() == State_ReturningToVehicle	||
		GetState() == State_WaitForVehicleLand  ||
		GetState() == State_ResumeAfterAbort )
	{
		return false;
	}

	return true;
}

CTaskFSMClone* CTaskUnalerted::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTask* pSubtask = GetSubTask();

	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pSubtask);
		if(pTask->GetState() == CTaskUseScenario::State_WaitingOnExit)
		{
			pTask->SetMigratingCowerExitingTask();
		}
	}

	CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted;

	pTaskUnalerted->SetScenarioMigratedWhileCowering(m_bScenarioCowering);
	return pTaskUnalerted;
}

CTaskFSMClone* CTaskUnalerted::CreateTaskForLocalPed(CPed* pPed)
{
	CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_USE_SCENARIO, PED_TASK_PRIORITY_MAX, false);

	CTaskUseScenario* pTaskUseScenario = NULL;

	if (pTaskInfo)
	{	
		CTaskUseScenario* pExitingTaskUseScenario = NULL;
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			pExitingTaskUseScenario = static_cast<CTaskUseScenario *>(GetSubTask());
			pTaskUseScenario = SafeCast(CTaskUseScenario, pExitingTaskUseScenario->CreateTaskForLocalPed(pPed));
			pExitingTaskUseScenario->SetFlag(aiTaskFlags::HandleCloneSwapToSameTaskType);
		}
		else
		{
			pTaskUseScenario = SafeCast(CTaskUseScenario, pTaskInfo->CreateCloneFSMTask());
		}

		if(taskVerifyf(pTaskUseScenario, "Expect valid task"))
		{
			if(pPed->GetNetworkObject())
			{
				CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());
				//The ped will be running a CTaskUseScenario subtask
				//set this flag now so this net ped is known as a scenario ped immediately before
				//any scope checks that occur between now and the CTaskUseScenario update
				netObjPed->SetOverrideScopeBiggerWorldGridSquare(true);
			}

			CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted(pTaskUseScenario, pTaskUseScenario->GetScenarioPoint(),pTaskUseScenario->GetScenarioPointType());

			return pTaskUnalerted;
		}
	}

	pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO, PED_TASK_PRIORITY_MAX, false);

	CTaskUseVehicleScenario* pTaskUsevehicleScenario = NULL;

	if (pTaskInfo)
	{	
		CClonedTaskUseVehicleScenarioInfo* pClonedTaskUseVehicleScenarioInfo = static_cast<CClonedTaskUseVehicleScenarioInfo*>(pTaskInfo);

		pTaskUsevehicleScenario = SafeCast(CTaskUseVehicleScenario, pTaskInfo->CreateCloneFSMTask());
		if(taskVerifyf(pTaskUsevehicleScenario, "Expect valid vehicle task"))
		{
			CScenarioPoint* pStartSP =pTaskUsevehicleScenario->GetScenarioPoint();
			s32 scenarioType = pTaskUsevehicleScenario->GetScenarioType();

			if(pClonedTaskUseVehicleScenarioInfo && pClonedTaskUseVehicleScenarioInfo->GetNextScenarioPoint())
			{
				pStartSP = pClonedTaskUseVehicleScenarioInfo->GetNextScenarioPoint();
			}

			CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted(pTaskUsevehicleScenario, pStartSP, scenarioType );
			return pTaskUnalerted;
		}
	}

	pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_WANDERING_SCENARIO, PED_TASK_PRIORITY_MAX, false);

	CTaskWanderingScenario* pTaskWanderingScenario = NULL;

	CClonedTaskWanderingScenarioInfo* pClonedTaskWanderingScenarioInfo = static_cast<CClonedTaskWanderingScenarioInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_WANDERING_SCENARIO, PED_TASK_PRIORITY_MAX, false));
	if (pClonedTaskWanderingScenarioInfo)
	{	
		pTaskWanderingScenario = SafeCast(CTaskWanderingScenario, pClonedTaskWanderingScenarioInfo->CreateCloneFSMTask());
		if(taskVerifyf(pTaskWanderingScenario, "Expect valid wandering scenario task"))
		{
			CScenarioPoint* pStartSP =NULL;
			s32 scenarioType = pClonedTaskWanderingScenarioInfo->GetScenarioType();

			if(pClonedTaskWanderingScenarioInfo->GetNextScenarioPoint())
			{
				pStartSP = pClonedTaskWanderingScenarioInfo->GetNextScenarioPoint();
			}
			else if(pClonedTaskWanderingScenarioInfo->GetCurrScenarioPoint())
			{
				pStartSP = pClonedTaskWanderingScenarioInfo->GetCurrScenarioPoint();
			}

			CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted(pTaskWanderingScenario, pStartSP,scenarioType);
			return pTaskUnalerted;
		}
	}
	CTaskUnalerted *pTaskUnalerted = rage_new CTaskUnalerted;

	pTaskUnalerted->m_bDontReturnToVehicleAtMigrate = GetStateFromNetwork()!=State_ReturningToVehicle;

	return pTaskUnalerted;
}

void CTaskUnalerted::UpdateClonedSubTasks(CPed* pPed, int const UNUSED_PARAM(iState))
{
	if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
	{
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO))
		{
			if(CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_USE_SCENARIO, false) )
			{		
				CTaskUseScenario* pNewScenarioTask = static_cast<CTaskUseScenario *>(GetNewTask());
				if(taskVerifyf(pNewScenarioTask,"Expect a new CTaskUseScenario here "))
				{
					CNetObjPed* pPedObj = pPed ? SafeCast(CNetObjPed, pPed->GetNetworkObject()) : NULL;

					if(pPedObj)
					{   //Set net object as using scenario scope range immediately
						pPedObj->SetOverrideScopeBiggerWorldGridSquare(true);
					}

					if(m_bScenarioCowering)
					{
						pNewScenarioTask->SetMigratingCowerStartingTask();
					}
				}
			}
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_VEHICLE_SCENARIO))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_USE_VEHICLE_SCENARIO, false);
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDERING_SCENARIO))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_WANDERING_SCENARIO, false);
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_AMBIENT_CLIPS, false);
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_EXIT_VEHICLE, false);
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_ENTER_VEHICLE, false);
		}
	}
}

CVehicle* CTaskUnalerted::FindVehicleUsingMyChain() const
{
	if (aiVerify(m_SPC_UseInfo && m_SPC_UseInfo->IsIndexInfoValid()))
	{
		const atArray<CScenarioPointChainUseInfo*>& users = CScenarioChainingGraph::GetChainUserArrayContainingUser(*m_SPC_UseInfo);
		const int ucount = users.GetCount();
		for (int u = 0; u < ucount; u++)
		{
			const CScenarioPointChainUseInfo* user = users[u];
			if (m_SPC_UseInfo != user) // the one we know about is a ped user info so a vehicle should not be in there.
			{
				CVehicle* foundVehicle = const_cast<CVehicle*>(user->GetVehicle());
				if (foundVehicle && !foundVehicle->GetIsInReusePool()) // B*2930035
				{		
					return foundVehicle;
				}
			}
		}
	}
	return NULL;
}

s32 CTaskUnalerted::GetRandomScenarioTypeSafeForResume( )
{
	static int scenario_index = 0;

	if (GetPed()->GetIsInInterior())
	{
		// If the Ped is indoors, force it to mobile calls (don't pick smoking). 
		//  This logic would need to change if there's more than two scenarios to pick from in sm_aSafeForResumeScenarioHashes. [8/28/2012 mdawe]
		return CScenarioManager::GetScenarioTypeFromHashKey(ATSTRINGHASH("WORLD_HUMAN_STAND_MOBILE", 0x0e574929a));
	}
	else
	{
		// Pick the next scenario in the list. If we had more than two scenarios
		// to move into, this could be a random selection from the list,
		// but we don't want to pick the same number twice in a row lest we get 
		// two peds right next to each other both doing the same thing. 
		// That could still be an issue if there's more than two Peds hitting this at once, though. [8/10/2012 mdawe]
		scenario_index = (scenario_index + 1) % POST_HANGOUT_SCENARIOS_SIZE;
		return CScenarioManager::GetScenarioTypeFromHashKey(sm_aSafeForResumeScenarioHashes[scenario_index]);
	}
}

#if __ASSERT
bool CTaskUnalerted::IsHashPostHangoutScenario(u32 uHash)
{
	for(int i=0;i<POST_HANGOUT_SCENARIOS_SIZE;i++)
	{
		if(uHash ==sm_aSafeForResumeScenarioHashes[i])
		{
			return true;
		}
	}
	return false;
}
#endif


void CTaskUnalerted::TryToObtainChainUserInfo(CScenarioPoint& pt, bool reallyNeeded)
{
	Assert(pt.IsChained());

	// Shouldn't be needed, but if something goes wrong we are probably better off being defensive here:
	if(!aiVerifyf(!m_SPC_UseInfo, "Already got a chain user."))
	{
		return;
	}

	CPed* pPed = GetPed();

	// First, find the chain the point is in. This isn't very fast, but it's basically the same
	// as what the old CScenarioChainingGraph::RegisterPointChainUser() would do, and we don't
	// have to repeat this work once we actually register.
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	int regionIndexForPoint = -1;
	int chainIndexForPoint = -1;
	int nodeIndexForPoint = -1;
	for(int i = 0; i < numRegions; i++)
	{
		const CScenarioPointRegion* pReg = SCENARIOPOINTMGR.GetRegion(i);
		if(!pReg)
		{
			continue;
		}
		const CScenarioChainingGraph& graph = pReg->GetChainingGraph();
		int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(pt);
		if(nodeIndex >= 0)
		{
			chainIndexForPoint = graph.GetNodesChainIndex(nodeIndex);
			nodeIndexForPoint = nodeIndex;
			regionIndexForPoint = i;
			break;
		}
	}

	if(chainIndexForPoint < 0)
	{
		// Didn't find any chain in the resident regions, can't do anything.
		return;
	}

	// First off, regardless of whether the chain is full or not, we check the spawn history to see if
	// there is already an entry matching this ped and this chain. If so, we will just use that, since
	// we don't want this ped to count as more than one user of the chain.
	CScenarioPointChainUseInfo* chainUserInfo = CScenarioManager::GetSpawnHistory().FindChainUserFromHistory(*pPed, regionIndexForPoint, chainIndexForPoint);

	// If we found anything, bind it to us and return.
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	if(chainUserInfo)
	{
		chainUserInfo->SetPedAndVehicle(pPed, pVeh);
#if __BANK
		aiDebugf2("Ped %s (%p) found chaining user info from spawn history, in CTaskUnalerted %p.", pPed->GetDebugName(), (void*)pPed, chainUserInfo);
		aiDebugf2("- ped %p, vehicle %p, hist %d", (void*)chainUserInfo->GetPed(), (void*)chainUserInfo->GetVehicle(), chainUserInfo->GetNumRefsInSpawnHistory());
#endif
		m_SPC_UseInfo = chainUserInfo;
		return;
	}

	// If the chain is full and we really need to register, try to make space by
	// kicking out some history entry, or dead ped, or clone, or something like that
	// which could b e considered less important than us.
	if(pt.IsUsedChainFull() && reallyNeeded)
	{
		if(chainIndexForPoint >= 0)
		{
			CScenarioPointRegion* pRegion = SCENARIOPOINTMGR.GetRegion(regionIndexForPoint);
			if(pRegion)
			{
				pRegion->TryToMakeSpaceForChainUser(chainIndexForPoint);
			}
		}
	}

	// If the chain is not full, just register a new user now. Also, if the chain is full,
	// but we think we really need to be there, register it as well. That is something that
	// probably can happen in networking games.
	if(!pt.IsUsedChainFull() || reallyNeeded)
	{
		chainUserInfo = CScenarioChainingGraph::RegisterPointChainUserForSpecificChain(pPed, pVeh, regionIndexForPoint, chainIndexForPoint, nodeIndexForPoint);

		if(chainUserInfo)
		{
#if (!__NO_OUTPUT) && (__ASSERT || __BANK)
			aiDebugf2("Ped %s (%p) created new chaining user info in CTaskUnalerted %p.", pPed->GetDebugName(), (void*)pPed, chainUserInfo);
			aiDebugf2("- ped %p, vehicle %p, hist %d", (void*)chainUserInfo->GetPed(), (void*)chainUserInfo->GetVehicle(), chainUserInfo->GetNumRefsInSpawnHistory());
#endif	// (!__NO_OUTPUT) && (__ASSERT || __BANK)

			m_SPC_UseInfo = chainUserInfo;
			return;
		}
	}

}

//-------------------------------------------------------------------------
// Task info for Task Unalerted Info
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedTaskUnalertedInfo::CClonedTaskUnalertedInfo(s32 state, bool hasScenarioChainUser, bool bIsCowering)
		: m_bHasScenarioChainUser(hasScenarioChainUser)
		, m_bIsCowering(bIsCowering)
{
	SetStatusFromMainTaskState(state);
}

CClonedTaskUnalertedInfo::CClonedTaskUnalertedInfo() 
		: m_bHasScenarioChainUser(false)
		, m_bIsCowering(false)
{
}

CTaskFSMClone * CClonedTaskUnalertedInfo::CreateCloneFSMTask()
{
	CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted();

	pTaskUnalerted->SetScenarioMigratedWhileCowering(m_bIsCowering);

	return pTaskUnalerted;
}

CTask *CClonedTaskUnalertedInfo::CreateLocalTask(fwEntity* pEntity)
{
	if(pEntity && pEntity->GetType() == ENTITY_TYPE_PED)
	{
		CPed* pPed = (CPed*)pEntity;
		CNetObjPed* pPedObj = pPed ? SafeCast(CNetObjPed, pPed->GetNetworkObject()) : NULL;

		if(pPedObj)
		{
			CTaskUseScenario* pTaskUseScenario = NULL;
			CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_USE_SCENARIO, PED_TASK_PRIORITY_MAX, false);

			if(pTaskInfo)
			{
				CTaskUseScenario* pExitingTaskUseScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
				if (pExitingTaskUseScenario)
				{
					pTaskUseScenario = SafeCast(CTaskUseScenario, pExitingTaskUseScenario->CreateTaskForLocalPed(pPed));
					pExitingTaskUseScenario->SetFlag(aiTaskFlags::HandleCloneSwapToSameTaskType);
				}
				else
				{
					pTaskUseScenario = SafeCast(CTaskUseScenario, pTaskInfo->CreateCloneFSMTask());
				}
			}

			if(pTaskUseScenario)
			{
				pPedObj->SetOverrideScopeBiggerWorldGridSquare(true);

				CTaskUnalerted* pTaskUnalerted = rage_new CTaskUnalerted(pTaskUseScenario, pTaskUseScenario->GetScenarioPoint(),pTaskUseScenario->GetScenarioPointType());

				return pTaskUnalerted;
			}
		}
	}

	CTaskUnalerted * pTask = (CTaskUnalerted*) CreateCloneFSMTask();

	return pTask;
}



//-----------------------------------------------------------------------------

// End of file 'task/Default/TaskUnalerted.cpp'
