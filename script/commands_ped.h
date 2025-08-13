#ifndef _COMMANDS_PED_H_
#define _COMMANDS_PED_H_

// Forward declarations
class CEntity;			//	declared in scene\CEntity.h
class CPed;				//	declared in Peds\ped.h

namespace ped_commands
{
	void DeleteScriptPed(CPed *pPed);

	bool CanPedBeGrabbedByScript(const CEntity *pPedEntity, bool bReturnRandomPeds, bool bReturnMissionPeds, bool bCheckIfThePedIsInAGroup, bool bCheckIfThePedIsInAVehicle, bool bReturnPlayerPeds, bool bReturnDeadOrDyingPeds, bool bReturnPedsWithScriptedTasks, bool bReturnPedsWithAnyNonDefaultTask, int iExclusionPedType);

	void SetPedCanBeKnockedOffVehicle(CPed *pPed, int CanBeKnockedOffFlag);

	void SetupScriptCommands();

	void SetRelationshipGroupBlipForPed(CPed *pPed, bool bAddBlip);
}

#endif	//	_COMMANDS_PED_H_


