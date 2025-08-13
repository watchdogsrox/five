#include "ai/debug/types/TaskDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "event/decision/EventDecisionMaker.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"

const char* GetShortActiveTaskTreeName(const CPed& rPed, const CTask* pActiveTask)
{
	if (pActiveTask == rPed.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY)) return "Script";
	if (pActiveTask == rPed.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP)) return "NonTemp";
	if (pActiveTask == rPed.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP)) return "Temp";
	if (pActiveTask == rPed.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PHYSICAL_RESPONSE)) return "Physical";
	return "Default";
}

const char* GetPopTypeString(ePopType popType)
{
	switch (popType)
	{
		case POPTYPE_UNKNOWN: return "POPTYPE_UNKNOWN";
		case POPTYPE_RANDOM_PERMANENT: return "POPTYPE_RANDOM_PERMANENT";
		case POPTYPE_RANDOM_PARKED: return "POPTYPE_RANDOM_PARKED";
		case POPTYPE_RANDOM_PATROL: return "POPTYPE_RANDOM_PATROL";
		case POPTYPE_RANDOM_SCENARIO: return "POPTYPE_RANDOM_SCENARIO";
		case POPTYPE_RANDOM_AMBIENT: return "POPTYPE_RANDOM_AMBIENT";
		case POPTYPE_PERMANENT: return "POPTYPE_PERMANENT";
		case POPTYPE_MISSION: return "POPTYPE_MISSION";
		case POPTYPE_REPLAY: return "POPTYPE_REPLAY";
		case POPTYPE_CACHE: return "POPTYPE_CACHE";
		case POPTYPE_TOOL: return "POPTYPE_TOOL";
		default: break;
	}
	return "UNKNOWN";
}

const char* GetAlertnessString(CPedIntelligence::AlertnessState alertState)
{
	switch (alertState)
	{
		case CPedIntelligence::AS_NotAlert: return "AS_NotAlert";
		case CPedIntelligence::AS_Alert: return "AS_Alert";
		case CPedIntelligence::AS_VeryAlert: return "AS_VeryAlert";
		case CPedIntelligence::AS_MustGoToCombat: return "AS_MustGoToCombat";
	}
	return "AS_Unknown";
}

CTasksFullDebugInfo::CTasksFullDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CTasksFullDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("TASKS FULL");
		PushIndent();
			PushIndent();
				PushIndent();
					ColorPrintLn(Color_orange, "%s ped %s, owner id (%s) (0x%p)", AILogging::GetDynamicEntityIsCloneStringSafe(m_Ped), AILogging::GetDynamicEntityNameSafe(m_Ped), AILogging::GetDynamicEntityOwnerGamerNameSafe(m_Ped), m_Ped.Get());
					ColorPrintLn(Color_red, "Ped Alertness State: %s", GetAlertnessString(m_Ped->GetPedIntelligence()->GetAlertnessState()));	
					PrintPlayerControlStatus();
					PrintPedVehicleInfo();
					PrintLn("Population type: %s", GetPopTypeString(m_Ped->PopTypeGet()));
					PrintHealth();
					PrintPrimaryTasks();
					PrintMovementTasks();
					PrintMotionTasks();
					PrintSecondaryTasks();
	
					if (!m_Ped->IsNetworkClone())
					{
						PrintCurrentEvent();
						PrintStaticCounter();
					}

					const char* szDecisionMakerName = m_Ped->GetPedIntelligence()->GetDecisionMakerName();
					ColorPrintLn(Color_green, "Decision Maker: %s", szDecisionMakerName);
					ColorPrintLn(Color_green, "Ped Type: %s", CPedType::GetPedTypeNameFromId(m_Ped->GetPedType()));
					ColorPrintLn(Color_green, "Ped Preferred Passenger Seat Index: %i", m_Ped->m_PedConfigFlags.GetPassengerIndexToUseInAGroup());

					PrintEnterVehicleDebug();	// TODO: Move to task debug?
					PrintNavMeshDebug();		// TODO: Move to task debug?
					PrintAudioDebug();
					PrintInVehicleContextDebug();
					PrintCoverDebug();
}

bool CTasksFullDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CTasksFullDebugInfo::PrintPedVehicleInfo()
{

	if (CTaskEnterVehicle *pVehEnterTask = (CTaskEnterVehicle*)m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE))
	{
		ColorPrintLn(Color_green, "TargetEnterExitPoint: %i, TargetSeat: %i", pVehEnterTask->GetTargetEntryPoint(), pVehEnterTask->GetTargetSeat());
	}

	else if (CTaskExitVehicle *pVehExitTask = (CTaskExitVehicle*)m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		ColorPrintLn(Color_green, "TargetEnterExitPoint: %i", pVehExitTask->GetTargetEntryPoint());
	}

	else if (m_Ped->GetIsInVehicle())
	{
		const CVehicle* pVehicle = m_Ped->GetMyVehicle();
		ColorPrintLn(Color_green, "Vehicle Model Name: %s, Seat Index: %i", pVehicle->GetDebugName(), pVehicle->GetSeatManager()->GetPedsSeatIndex(m_Ped));
	}
}

void CTasksFullDebugInfo::PrintPlayerControlStatus()
{
	if (!m_Ped->IsLocalPlayer())
		return;

	const CPlayerInfo* pPlayerInfo = m_Ped->GetPlayerInfo();
	if (!pPlayerInfo)
		return;

	char tmp[256];	
	if (!pPlayerInfo->AreControlsDisabled())
	{
		ColorPrintLn(Color_green, "Player Control: Enabled");	
	}
	else 
	{
		sprintf(tmp, "Player Control: Disabled (%s%s%s%s%s%s%s%s)", 
			pPlayerInfo->IsControlsCameraDisabled() ? "Camera " : "", 
			pPlayerInfo->IsControlsDebugDisabled() ? "Debug " : "", 
			pPlayerInfo->IsControlsGaragesDisabled() ? "Garages " : "", 
			pPlayerInfo->IsControlsScriptDisabled() ? "Script(Mission) " : "", 
			pPlayerInfo->IsControlsScriptAmbientDisabled() ? "Script(Ambient) " : "", 
			pPlayerInfo->IsControlsCutscenesDisabled() ? "Cutscene " : "", 
			pPlayerInfo->IsControlsLoadingScreenDisabled() ? "LoadingScreen " : "", 
			pPlayerInfo->IsControlsFrontendDisabled() ? "Frontend " : "" );
		ColorPrintLn(Color_red, tmp);	
	}
}

void CTasksFullDebugInfo::PrintHealth()
{
	if (m_Ped->IsDead())
	{
		ColorPrintLn(Color_red, "Health: DEAD %.2f", m_Ped->GetHealth());	
	}
	else if (m_Ped->IsFatallyInjured())
	{
		ColorPrintLn(Color_DarkOrange, "Health: Fatally Injured %.2f", m_Ped->GetHealth());
	}
	else if (m_Ped->IsInjured())
	{
		ColorPrintLn(Color_orange, "Health: Injured %.2f", m_Ped->GetHealth());
	}
	else if (m_Ped->HasHurtStarted())
	{
		ColorPrintLn(Color_orange, "Health: Hurt %.2f", m_Ped->GetHealth());
	}
	else if (m_Ped->IsFatigued())
	{
		ColorPrintLn(Color_yellow2, "Health: Fatigued %.2f", m_Ped->GetHealth());
	}
	else
	{
		char deathState[32];
		if (m_Ped->GetDeathState() == DeathState_Dead )
			sprintf(deathState, "Dead");
		else if (m_Ped->GetDeathState() == DeathState_Dying )
			sprintf(deathState, "Dying");
		else
			sprintf(deathState, "Alive");

		char arrestState[32];
		if (m_Ped->GetArrestState() == ArrestState_Arrested )
			sprintf(arrestState, "Arrested");
		else
			sprintf(arrestState, "Not");

		ColorPrintLn(Color_green, "Health: %0.2f/%0.2f, DS(%s), AS(%s)", m_Ped->GetHealth(), m_Ped->GetMaxHealth(), deathState, arrestState);
	}
}

void CTasksFullDebugInfo::PrintPrimaryTasks()
{
	const CTask* pActiveTask = m_Ped->GetPedIntelligence()->GetTaskActive();
	if (!pActiveTask)
		return;

	char szTree[32] = "Default";
	formatf(szTree, GetShortActiveTaskTreeName(*m_Ped, pActiveTask));

	const CTask* pTaskToPrint = pActiveTask;
	while (pTaskToPrint)
	{
		bool outOfScope = false;

		if (m_Ped->IsNetworkClone() && pTaskToPrint->IsClonedFSMTask())
		{
			const CTaskFSMClone *taskFSMClone = SafeCast(const CTaskFSMClone, pTaskToPrint);
			outOfScope = !const_cast<CTaskFSMClone*>(taskFSMClone)->IsInScope(m_Ped);	// IsInScope should probably be const
		}

		aiAssertf(pTaskToPrint->GetName(), "Found a task without a name!");

		if (pTaskToPrint->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE)
		{
			ColorPrintLn(Color_yellow, "%s:(Seq)%s %.2f/%.2f,..", outOfScope ? "Out Of Scope" : szTree, pTaskToPrint->GetName().c_str(), pTaskToPrint->GetTimeInState(), pTaskToPrint->GetTimeRunning());
		}
		else
		{
			ColorPrintLn(Color_yellow, "%s:%s %.2f/%.2f", outOfScope ? "Out Of Scope" : szTree, pTaskToPrint->GetName().c_str(), pTaskToPrint->GetTimeInState(), pTaskToPrint->GetTimeRunning());
		}

		if (CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayPrimaryTaskStateHistory)
		{
			PrintStateHistoryForTask(*pTaskToPrint);
		}
		pTaskToPrint=pTaskToPrint->GetSubTask();
	}
}

void CTasksFullDebugInfo::PrintMovementTasks()
{
	if (m_Ped->IsNetworkClone())
		return;

	const CTask* pActiveTask = m_Ped->GetPedIntelligence()->GetActiveMovementTask();
	if (!pActiveTask)
		return;

	const CTask* pTaskToPrint = pActiveTask;
	while (pTaskToPrint)
	{
		aiAssertf(pTaskToPrint->GetName(), "Found a task without a name!");

		const float fMbr = pTaskToPrint->GetMoveInterface() ? pTaskToPrint->GetMoveInterface()->GetMoveBlendRatio() : 0.0f;
		ColorPrintLn(Color_green, "Move:%s %.2f/%.2f (%.1f MBR)", (const char*) pTaskToPrint->GetName(), pTaskToPrint->GetTimeInState(), pTaskToPrint->GetTimeRunning(), fMbr);

		if (CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayMovementTaskStateHistory)
		{
			PrintStateHistoryForTask(*pTaskToPrint);
		}

		pTaskToPrint = pTaskToPrint->GetSubTask();
	}
}

void CTasksFullDebugInfo::PrintMotionTasks()
{
	const CTask* pActiveTask = static_cast<const CTask*>(m_Ped->GetPedIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_MOTION));
	if (!pActiveTask)
		return;

	const CTask* pTaskToPrint = pActiveTask;
	while(pTaskToPrint)
	{
		aiAssertf(pTaskToPrint->GetName(), "Found a task without a name!");

		ColorPrintLn(Color_black, "Motion:%s %.2f/%.2f", pTaskToPrint->GetName().c_str(), pTaskToPrint->GetTimeInState(), pTaskToPrint->GetTimeRunning());

		if (CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayMotionTaskStateHistory)
		{
			PrintStateHistoryForTask(*pTaskToPrint);
		}

		pTaskToPrint = pTaskToPrint->GetSubTask();
	}
}

void CTasksFullDebugInfo::PrintSecondaryTasks()
{
	const CTask* pActiveTask = m_Ped->GetPedIntelligence()->GetTaskSecondaryActive();

	if (!pActiveTask)
		return;
	
	const CTask* pTaskToPrint = pActiveTask;
	while(pTaskToPrint)
	{
		aiAssertf(pTaskToPrint->GetName(), "Found a task without a name!");

		ColorPrintLn(Color_brown, "Secondary:%s %.2f/%.2f", pTaskToPrint->GetName().c_str(), pTaskToPrint->GetTimeInState(), pTaskToPrint->GetTimeRunning());

		if (CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplaySecondaryTaskStateHistory)
		{
			PrintStateHistoryForTask(*pTaskToPrint);
		}

		pTaskToPrint = pTaskToPrint->GetSubTask();
	}
}

void CTasksFullDebugInfo::PrintStateHistoryForTask(const CTask& ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG_ONLY(rTask))
{
#if ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG
	if (!aiTaskStateHistoryManager::GetInstance())
		return;

	const aiTaskStateHistory* pTaskStateHistory = aiTaskStateHistoryManager::GetInstance()->GetTaskStateHistoryForTask(&rTask);
	if (!pTaskStateHistory)
		return;

	ColorPrintLn(Color_red, "Frame Changed | Previous States");

	s32 iCurrentState = pTaskStateHistory->GetFreeSlotStateIndex();
	if (iCurrentState == 0 || iCurrentState == 1)
	{
		iCurrentState = aiTaskStateHistory::MAX_TASK_STATES + (iCurrentState - 2);
	}
	else
	{
		iCurrentState -= 2;
	}

	const s32 iStartState = iCurrentState;

	while (pTaskStateHistory->GetStateAtIndex(iCurrentState) != (s32)aiTaskStateHistory::TASK_STATE_INVALID)
	{
		if (iCurrentState == 0)
		{
			iCurrentState = aiTaskStateHistory::MAX_TASK_STATES - 1;
		}
		else
		{
			--iCurrentState;
		}

		if (iCurrentState == iStartState)
		{
			break;
		}

		// Only draw text if we haven't already displayed it for this state
		ColorPrintLn(Color_red, "%u | %s", pTaskStateHistory->GetStateFrameCountAtIndex(iCurrentState), TASKCLASSINFOMGR.GetTaskStateName(rTask.GetTaskType(), pTaskStateHistory->GetStateAtIndex(iCurrentState)));
	}	
#endif // ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG
}

void CTasksFullDebugInfo::PrintCurrentEvent()
{
    const CEvent* pCurrentEvent = m_Ped->GetPedIntelligence()->GetCurrentEvent();
	if (pCurrentEvent)
	{
		ColorPrintLn(Color_red, "Current Event: %s, Priority: %d %s", pCurrentEvent->GetName().c_str(), pCurrentEvent->GetEventPriority(), m_Ped->GetBlockingOfNonTemporaryEvents() ? " (Blocking active!)" : " ");
	}
	else 
	{
		ColorPrintLn(Color_green, "Current Event: None %s", m_Ped->GetBlockingOfNonTemporaryEvents() ? " (Blocking active!)" : " ");
	}
}

void CTasksFullDebugInfo::PrintStaticCounter()
{
	Color32 col;
	u8 val1 = 128 + (u8)m_Ped->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter();
	u8 val2 = 255;
	col = CRGBA(Min(val1,val2),0,0);
	ColorPrintLn(col, "static:%d", m_Ped->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter());
}

void CTasksFullDebugInfo::PrintEnterVehicleDebug()
{
	const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
	if (!pEnterVehicleTask)
		return;

	const bool bWillWarpAfterTime = pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime);
	const bool bIsInAir = pEnterVehicleTask->GetVehicle() ? pEnterVehicleTask->GetVehicle()->IsInAir() : false;
	const bool bWarpTimeNotUp = pEnterVehicleTask->GetTimeBeforeWarp() > 0.0f && bWillWarpAfterTime;
	ColorPrintLn(bWarpTimeNotUp ? Color_green : Color_red, "Will Warp After Time ? %s, Time Before Warp : %.2f, m_pVehicle->IsInAir() ? : %s", AILogging::GetBooleanAsString(bWillWarpAfterTime), pEnterVehicleTask->GetTimeBeforeWarp(), AILogging::GetBooleanAsString(bIsInAir));
}

void CTasksFullDebugInfo::PrintNavMeshDebug()
{
	const CTaskMoveFollowNavMesh * pNavMeshTask = static_cast<const CTaskMoveFollowNavMesh*>(m_Ped->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if (pNavMeshTask && pNavMeshTask->GetWarpTimer() >= 0)
	{
		ColorPrintLn(Color_green, "Navmesh task will warp ped in : %i ms", pNavMeshTask->GetWarpTimer());
	}
}

void CTasksFullDebugInfo::PrintAudioDebug()
{
	// Print out the last thing the ped has said for a few seconds.
	const float SAYDEBUGDURATION = (255 * 20);
	if (m_Ped->m_SayWhat >= 0) // && fwTimer::GetTimeInMilliseconds() >= ped.m_StartTimeSay)
	{
		s32	HowLongAgo = fwTimer::GetTimeInMilliseconds() - m_Ped->m_StartTimeSay;
		u8	Colour = u8(MAX(64, ((SAYDEBUGDURATION) - HowLongAgo) / ((SAYDEBUGDURATION) / 255)));
		ColorPrintLn(CRGBA(Colour,Colour,0,200), "Said: %d", m_Ped->m_SayWhat);
	}
}

void CTasksFullDebugInfo::PrintInVehicleContextDebug()
{
	if (!m_Ped->GetIsInVehicle())
		return;

	const CVehicleSeatAnimInfo* pSeatClipInfo = m_Ped->GetMyVehicle()->GetSeatAnimationInfo(m_Ped);
	if (!pSeatClipInfo)
		return;

	const CTaskMotionBase* pTask = m_Ped->GetPrimaryMotionTask();
	const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
	const CInVehicleOverrideInfo* pOverrideInfo = uOverrideInVehicleContext != 0 ? CVehicleMetadataMgr::GetInVehicleOverrideInfo(uOverrideInVehicleContext) : NULL;
	if (uOverrideInVehicleContext != 0 && pOverrideInfo && pSeatClipInfo)
	{	
		const CSeatOverrideAnimInfo* pSeatOverrideAnimInfo = pOverrideInfo->FindSeatOverrideAnimInfoFromSeatAnimInfo(pSeatClipInfo);
		ColorPrintLn(Color_blue, "In Vehicle Context Override : %s", pSeatOverrideAnimInfo ? pSeatOverrideAnimInfo->GetName().GetCStr() : "NULL");
		if (pSeatOverrideAnimInfo)
		{
			const bool bLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(pSeatOverrideAnimInfo->GetSeatClipSet());
			PushIndent();
			ColorPrintLn(bLoaded ? Color_green : Color_red, "Clipset : %s, Loaded ? %s", pSeatOverrideAnimInfo->GetSeatClipSet().GetCStr(), AILogging::GetBooleanAsString(bLoaded));
			PopIndent();
		}
	}
	else
	{
		ColorPrintLn(Color_green, "In Vehicle Context Override : None");
	}
}

void CTasksFullDebugInfo::PrintCoverDebug()
{
	if (!m_Ped->IsLocalPlayer())
		return;

	const CEntity* pEntity = CPlayerInfo::ms_DynamicCoverHelper.GetCoverEntryEntity();
	if (!pEntity)
		return;

	ColorPrintLn(Color_blue, "Cover Entry Entity = %s", pEntity->GetModelName() ? pEntity->GetModelName() : "no name");
}

#undef DEFINE_VEHICLE_DEBUG_STRING_ARRAY
#define DEFINE_VEHICLE_DEBUG_STRING_ARRAY(LIST) static const char* szStrings[] = { LIST(DEFINE_TASK_STRING) };\
												s32 uArraySize = (s32)sizeof(szStrings)/(sizeof(szStrings)[0]);\
												if (i >= 0 && i < uArraySize) { return szStrings[i]; } \
												return "UNKNOWN STRING";

const char* TaskEnterVehicleGetChangeEntryPointReasonString(s32 i)
{
	DEFINE_VEHICLE_DEBUG_STRING_ARRAY(TARGET_ENTRY_POINT_CHANGE_REASONS)
}

const char* TaskEnterVehicleGetChangeSeatReasonString(s32 i)
{
	DEFINE_VEHICLE_DEBUG_STRING_ARRAY(TARGET_SEAT_CHANGE_REASONS)
}

const char* TaskEnterVehicleGetFinishReasonString(s32 i)
{
	DEFINE_VEHICLE_DEBUG_STRING_ARRAY(TASK_FINISH_REASONS)
}

const char* TaskEnterVehicleGetChangeSeatRequestTypeReasonString(s32 i)
{
	DEFINE_VEHICLE_DEBUG_STRING_ARRAY(SEAT_REQUEST_TYPE_CHANGE_REASONS)
}

CTaskEnterVehicleDebugInfo::CTaskEnterVehicleDebugInfo(const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Task(pEnterVehicleTask)
	, m_DebugFlags(iDebugFlags)
{

}

bool CTaskEnterVehicleDebugInfo::ValidateInput()
{
	if (!m_Task)
		return false;

	return true;
}
void CTaskEnterVehicleDebugInfo::PrintCommonDebug()
{
	char szEventName[256];
	formatf(szEventName, "TASK_ENTER_VEHICLE - %s", GetTypeName());

	WriteLogEvent(szEventName);
		PushIndent();
			PushIndent();
				PushIndent();

	const CTaskEnterVehicle* pEnterVehTask = static_cast<const CTaskEnterVehicle*>(m_Task.Get());
	const CPed* pTaskPed = pEnterVehTask->GetPed();
	const CVehicle* pTargetVehicle = pEnterVehTask->GetVehicle();

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_THIS_PED))
	{
		WriteDataValue("This Ped", AILogging::GetDynamicEntityNameSafe(pTaskPed));
	}

	if (m_DebugFlags & VEHICLE_DEBUG_PRINT_TARGET_VEHICLE)
	{
		WriteDataValue("Target vehicle", AILogging::GetDynamicEntityNameSafe(pTargetVehicle));
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_PED_IN_DRIVER_SEAT))
	{
		if (pTargetVehicle)
		{
			WriteDataValue("Driver Ped", AILogging::GetDynamicEntityNameSafe(pTargetVehicle->GetSeatManager()->GetPedInSeat(pTargetVehicle->GetDriverSeat())));
		}
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_PED_IN_DIRECT_SEAT))
	{
		WriteDataValue("Direct Seat Ped", AILogging::GetDynamicEntityNameSafe(pEnterVehTask->GetPedOccupyingDirectAccessSeat()));
	}

	const s32 iTargetSeatIndex = pEnterVehTask->GetTargetSeat();
	const s32 iEntryPointIndex = pEnterVehTask->GetTargetEntryPoint();
	if (IsFlagSet(VEHICLE_DEBUG_PRINT_TARGET_SEAT_AND_ENTRYPOINT))
	{
		WriteDataValue("Target Seat", "%i", iTargetSeatIndex);
		WriteDataValue("Target Entry Point", "%i", iEntryPointIndex);
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_JACKED_PED))
	{
		WriteDataValue("Jacked Ped", AILogging::GetDynamicEntityNameSafe(pEnterVehTask->GetJackedPed()));
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_DOOR_RESERVATION))
	{
		if (pTargetVehicle && pTargetVehicle->IsEntryIndexValid(iEntryPointIndex))
		{
			const CComponentReservation* pComponentReservation = const_cast<CVehicle*>(pTargetVehicle)->GetComponentReservationMgr()->GetDoorReservation(iEntryPointIndex);
			if (pComponentReservation)
			{
				WriteDataValue("Ped Using Door", AILogging::GetDynamicEntityNameSafe(pComponentReservation->GetPedUsingComponent()));
			}
		}
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_CURRENT_STATE))
	{
		WriteDataValue("Current State", CTaskEnterVehicle::GetStaticStateName(pEnterVehTask->GetState()));
	}

	if (IsFlagSet(VEHICLE_DEBUG_PRINT_SEAT_RESERVATION))
	{
		if (pTargetVehicle && pTargetVehicle->IsEntryIndexValid(iEntryPointIndex) && pTargetVehicle->IsSeatIndexValid(iTargetSeatIndex))
		{
			const CComponentReservation* pComponentReservation = const_cast<CVehicle*>(pTargetVehicle)->GetComponentReservationMgr()->GetSeatReservation(iEntryPointIndex, SA_directAccessSeat, iTargetSeatIndex);
			if (pComponentReservation)
			{
				WriteDataValue("Ped Using Seat", AILogging::GetDynamicEntityNameSafe(pComponentReservation->GetPedUsingComponent()));
			}
		}
	}
	
	if (IsFlagSet(VEHICLE_DEBUG_RENDER_RELATIVE_VELOCITY))
	{
		if (pTargetVehicle)
		{
			WriteDataValue("Relative Vehicle Velocity", "%.2f", pTargetVehicle->GetRelativeVelocity(*pTaskPed).Mag());
		}
	}
}

CSetSeatRequestTypeDebugInfo::CSetSeatRequestTypeDebugInfo(SeatRequestType newSeatRequestType, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams)
: CTaskEnterVehicleDebugInfo(pEnterVehicleTask, iDebugFlags, printParams)
	, m_SeatRequestType(newSeatRequestType)
	, m_ChangeReason(iChangeReason)
{

}

void CSetSeatRequestTypeDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	PrintCommonDebug();
		WriteDataValue("New Seat Request Type", CSeatAccessor::GetSeatRequestTypeString(m_SeatRequestType));
		WriteDataValue("Change Reason", TaskEnterVehicleGetChangeSeatRequestTypeReasonString(m_ChangeReason));
}

CSetTargetEntryPointDebugInfo::CSetTargetEntryPointDebugInfo(s32 iNewEntryPoint, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams)
	: CTaskEnterVehicleDebugInfo(pEnterVehicleTask, iDebugFlags, printParams)
	, m_NewEntryPoint(iNewEntryPoint)
	, m_ChangeReason(iChangeReason)
{

}

void CSetTargetEntryPointDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	PrintCommonDebug();
		WriteDataValue("New Target Entry Point", "%i", m_NewEntryPoint);
		WriteDataValue("Change Reason", TaskEnterVehicleGetChangeEntryPointReasonString(m_ChangeReason));
}

CSetTargetSeatDebugInfo::CSetTargetSeatDebugInfo(s32 iNewSeat, s32 iChangeReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams)
	: CTaskEnterVehicleDebugInfo(pEnterVehicleTask, iDebugFlags, printParams)
	, m_NewSeat(iNewSeat)
	, m_ChangeReason(iChangeReason)
{

}

void CSetTargetSeatDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	PrintCommonDebug();
	WriteDataValue("New Target Seat", "%i", m_NewSeat);
	WriteDataValue("Change Reason", TaskEnterVehicleGetChangeSeatReasonString(m_ChangeReason));
}

CSetTaskFinishStateDebugInfo::CSetTaskFinishStateDebugInfo(s32 iFinishReason, const CTaskEnterVehicle* pEnterVehicleTask, s32 iDebugFlags, DebugInfoPrintParams printParams)
	: CTaskEnterVehicleDebugInfo(pEnterVehicleTask, iDebugFlags, printParams)
	, m_FinishReason(iFinishReason)
{

}

void CSetTaskFinishStateDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	PrintCommonDebug();
		WriteDataValue("Previous State", "%s", CTaskEnterVehicle::GetStaticStateName(m_Task->GetState()));
		WriteDataValue("Finish Reason", TaskEnterVehicleGetFinishReasonString(m_FinishReason));
}

CGivePedJackedEventDebugInfo::CGivePedJackedEventDebugInfo(const CPed* pJackedPed, const CTask* pTask, const char* szCallingFunction, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Task(pTask)
	, m_JackedPed(pJackedPed)
{
	formatf(m_szCallingFunction, szCallingFunction);
}

void CGivePedJackedEventDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	char szEventName[256];
	formatf(szEventName, "%s - %s", m_Task->GetTaskName(), "GIVE_PED_JACKED_EVENT");

	const char* szStateName = (m_Task->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE) ? CTaskEnterVehicle::GetStaticStateName(m_Task->GetState()) : CTaskInVehicleSeatShuffle::GetStaticStateName(m_Task->GetState());
	WriteLogEvent(szEventName);
		PushIndent();
			PushIndent();
				PushIndent();
					WriteDataValue("Function", m_szCallingFunction);
					WriteDataValue("Task State", szStateName);
					WriteDataValue("Jacked Ped", AILogging::GetDynamicEntityNameSafe(m_JackedPed));
					WriteDataValue("This Ped", AILogging::GetDynamicEntityNameSafe(m_Task->GetPed()));
}

bool CGivePedJackedEventDebugInfo::ValidateInput()
{
	if (!m_Task)
		return false;

	if (m_Task->GetTaskType() != CTaskTypes::TASK_ENTER_VEHICLE && m_Task->GetTaskType() != CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE)
		return false;

	return true;
}
#endif // AI_DEBUG_OUTPUT_ENABLED
