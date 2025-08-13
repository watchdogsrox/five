// File header
#include "Task/System/TaskTypes.h"

// Game headers
#include "Task/System/Task.h"

AI_OPTIMISATIONS()

namespace CTaskTypes
{

CompileTimeAssert(TASK_HANDS_UP == 0 );	// "Task types out of sync with commands_task.sch"
CompileTimeAssert(TASK_CLIMB_LADDER == 1 );	// "Task types out of sync with commands_task.sch"
CompileTimeAssert(TASK_EXIT_VEHICLE == 2 );	// "Task types out of sync with commands_task.sch"
CompileTimeAssert(TASK_COMBAT_ROLL == 3 );	// "Task types out of sync with commands_task.sch"
CompileTimeAssert(TASK_AIM_GUN_ON_FOOT == 4 );	// "Task types out of sync with commands_task.sch"

bool IsRagdollTask(int taskType)
{
	if(taskType >= TASK_NM_RELAX && taskType < TASK_RAGDOLL_LAST)
	{
		return true;
	}

	return false;
}

bool IsCombatTask(int taskType)
{
	if( taskType == TASK_COMBAT || taskType == TASK_THREAT_RESPONSE || 
		taskType == TASK_REACT_TO_GUN_AIMED_AT || 
		taskType == TASK_SWAT_WANTED_RESPONSE || taskType == TASK_POLICE_WANTED_RESPONSE )
	{
		return true;
	}

	return false;
}

bool IsPlayerDriveTask(int taskType)
{
	if (taskType == TASK_VEHICLE_PLAYER_DRIVE
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_AUTOMOBILE
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_BIKE
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_BOAT
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_SUBMARINE
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_PLANE
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_HELI
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_AUTOGYRO
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_TRAIN
		|| taskType == TASK_VEHICLE_PLAYER_DRIVE_AMPHIBIOUS_AUTOMOBILE
        || taskType == TASK_VEHICLE_PLAYER_DRIVE_SUBMARINECAR)
	{
		return true;
	}

	return false;
}

}
