#include "ai\debug\types\VehicleTaskDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "ai\debug\types\TaskDebugInfo.h"
#include "vehicles\vehicle.h"
#include "Task/System/TaskManager.h"
#include "Task/System/Task.h"
#include "vehicleAi\VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/VehMission.h"
#include "camera/CamInterface.h"
#include "Task/System/TaskTree.h"

CVehicleTaskDebugInfo::CVehicleTaskDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams printParams, s32 iNumberOfLines)
: CVehicleDebugInfo(pVeh, printParams, iNumberOfLines)
{

}

void CVehicleTaskDebugInfo::PrintTask(CTask* pTask, bool bPrintAll)
{
	CTaskVehicleMissionBase *pCarTask = dynamic_cast<CTaskVehicleMissionBase*>(pTask);
	if(!pCarTask)
		return;

	if( CVehicleIntelligence::m_bDisplayCarFinalDestinations || bPrintAll)
	{
		const Vector3& TargetPos = pCarTask->GetParams().GetTargetPosition();
		ColorPrintLn(Color_blue, "Name: %s (0x%p): %.2f: (%.2f, %.2f, %.2f)", (const char*) pTask->GetName(), pCarTask, pCarTask->GetCruiseSpeed(), TargetPos.x, TargetPos.y, TargetPos.z);
	}
	else
	{		
		ColorPrintLn(Color_blue, "Name: %s (0x%p): %.2f", (const char*) pTask->GetName(), pCarTask, pCarTask->GetCruiseSpeed());
	}

	const CVehicleIntelligence* pIntelligence = m_Vehicle->GetIntelligence();
	if(pIntelligence && ( CVehicleIntelligence::m_bDisplayCarAiTaskDetails || bPrintAll))
	{
		char debugText[50];
		sprintf(debugText, "%s", "Non-default flags:");
		// First render the default type if set
		if( pCarTask->GetDrivingFlags() == DMode_StopForCars )
			sprintf(debugText, "%s", "-  DMode_StopForCars:");
		else if( pCarTask->GetDrivingFlags() == DMode_StopForCars_Strict )
			sprintf(debugText, "%s", "-  DMode_StopForCars_Strict:");
		else if( pCarTask->GetDrivingFlags() == DMode_AvoidCars )
			sprintf(debugText, "%s", "-  DMode_AvoidCars:");
		else if ( pCarTask->GetDrivingFlags() == DMode_AvoidCars_Reckless)
			sprintf(debugText, "%s", "- DMode_AvoidCars_Reckless");
		else if( pCarTask->GetDrivingFlags() == DMode_PloughThrough )
			sprintf(debugText, "%s", "-  DMode_PloughThrough:");
		else if( pCarTask->GetDrivingFlags() == DMode_StopForCars_IgnoreLights )
			sprintf(debugText, "%s", "-  DMode_StopForCars_IgnoreLights:");
		else if( pCarTask->GetDrivingFlags() == DMode_AvoidCars_ObeyLights )
			sprintf(debugText, "%s", "-  DMode_AvoidCars_ObeyLights:");
		else if( pCarTask->GetDrivingFlags() == DMode_AvoidCars_StopForPeds_ObeyLights )
			sprintf(debugText, "%s", "-  DMode_AvoidCars_StopForPeds_ObeyLights:");

		ColorPrintLn(Color_blue, debugText );

		for( u32 iFlag = DF_StopForCars; iFlag > 0; iFlag = iFlag << 1)
		{
			if( pCarTask->IsDrivingFlagSet((s32)iFlag) )
			{
				ColorPrintLn(Color_green, "  -  %s", pIntelligence->GetDrivingFlagName((s32)iFlag) );
			}

			if(iFlag == DF_LastFlag)
			{
				break;
			}
		}
	}
}

void CVehicleTaskDebugInfo::PrintTaskTree(bool bPrintAll)
{
	CTask* pActiveTask = m_Vehicle->GetIntelligence()->GetActiveTask();
	if(pActiveTask)
	{
		WriteHeader("VEHICLE TASK TREE");
		PushIndent();

		CTask* pTaskToPrint = pActiveTask;
		while(pTaskToPrint)
		{
			PrintTask(pTaskToPrint, bPrintAll);
			pTaskToPrint = pTaskToPrint->GetSubTask();
		}

		PopIndent();
	}
}

void CVehicleTaskDebugInfo::PrintTaskHistory(bool bPrintAll)
{
	CTaskManager* pTaskManager = m_Vehicle->GetIntelligence()->GetTaskManager();
	if(pTaskManager)
	{
		if( CVehicleIntelligence::m_bDisplayCarAiTaskHistory || bPrintAll)
		{
			WriteHeader("TASK HISTORY");
			PushIndent();

			for( s32 i = 0; i < CTaskTree::MAX_TASK_HISTORY; i++ )
			{
				s32 iTaskID = pTaskManager->GetHistory(VEHICLE_TASK_TREE_PRIMARY, i);
				if( iTaskID > 0 && iTaskID != CTaskTypes::TASK_NONE )
				{
					ColorPrintLn( Color_green, TASKCLASSINFOMGR.GetTaskName(iTaskID));
				}
			}
			PopIndent();

			WriteHeader("SECONDARY TASK HISTORY");
			PushIndent();

			for( s32 i = 0; i < CTaskTree::MAX_TASK_HISTORY; i++ )
			{
				s32 iTaskID = pTaskManager->GetHistory(VEHICLE_TASK_TREE_SECONDARY, i);
				if( iTaskID > 0 && iTaskID != CTaskTypes::TASK_NONE )
				{
					ColorPrintLn( Color_green, TASKCLASSINFOMGR.GetTaskName(iTaskID));
				}
			}
			PopIndent();
		}
	}
}

void CVehicleTaskDebugInfo::Visualise()
{
#if __BANK//def CAM_DEBUG
	if(!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	const CVehicleIntelligence* pIntelligence = m_Vehicle->GetIntelligence();
	CTask *pTask = pIntelligence->GetActiveTask();
	while(pTask)
	{
		if( CVehicleIntelligence::m_bDisplayCarAiTaskInfo )
		{
			PrintTask(pTask, false);

			if(pTask->GetIsFlagSet(aiTaskFlags::HasBegun))
			{
				pTask->Debug();
			}
		}

		if( CVehicleIntelligence::m_bDisplayCarFinalDestinations )
		{
			CTaskVehicleMissionBase *pCarTask = dynamic_cast<CTaskVehicleMissionBase*>(pTask);
			const Vector3& TargetPos = pCarTask->GetParams().GetTargetPosition();
			if(!TargetPos.IsEqual(ORIGIN))
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), TargetPos, Color_blue);
			}			
		}

		pTask = pTask->GetSubTask();
	}

	PrintTaskHistory(false);

#endif // #if __BANK
}

void CVehicleTaskDebugInfo::PrintRenderedSelection(bool bPrintAll)
{
	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	WriteLogEvent("VEHICLE TASKS");
	PushIndent();
	PushIndent();

	PrintTaskTree(bPrintAll);
	PrintTaskHistory(bPrintAll);

	PopIndent();
	PopIndent();
}

#endif