#ifndef _COMMANDS_TASK_H_
#define _COMMANDS_TASK_H_

//	Rage headers
#include "script/thread.h"	//	required for scrVector

class CPed;		//	declared in Peds\ped.h
class CTask;	//	declared in task\System\Task.h

namespace task_commands
{
	void SetupScriptCommands();
	CTask* CommonUseNearestScenarioToPos( CPed* pPed, const scrVector & svScenarioPlace, float fMaxRange, bool bWarp, bool bCalledFromTask, int iTimeToLeave, bool bMustBeTrain=false );
	CTask* CommonUseNearestScenarioChainToPos( CPed* pPed, const scrVector & svScenarioPlace, float fMaxRange, bool bWarp, bool bCalledFromTask, int iTimeToLeave );
	void UpdatePedRagdollBoundsForScriptActivation(CPed* pPed);
}
#endif	//	_COMMANDS_TASK_H_

