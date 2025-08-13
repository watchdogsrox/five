#ifndef _COMMANDS_MISC_H_
#define _COMMANDS_MISC_H_

namespace rage
{
	struct scrVector;
}

namespace misc_commands
{
	void CommandActivateSaveMenu(bool bAllowWhilePlayerIsInAVehicle);	// this is here so that debug widget in CGenericGameStorage can call it.
	void CommandActivateNetworkSettingsMenu(); // this is here so that the frontend widget in CHud::AddBankWidgets() can call if.

	void SetupScriptCommands();
	void CommandClearArea(const scrVector & scrVecCoors, float Radius, bool DeleteProjectilesFlag, bool bLeaveCarGenCars, bool bClearLowPriorityPickupsOnly, bool bBroadcast);
	void CommandClearAreaLeaveVehicleHealth( const scrVector & scrVecCoors, float Radius, bool DeleteProjectilesFlag, bool bLeaveCarGenCars, bool bClearLowPriorityPickupsOnly, bool bBroadcast );
	void CommandClearAreaOfVehicles(const scrVector & scrVecCoors, float Radius, bool bLeaveCarGenCars, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bBroadcast, bool bIfEngineOnFire, bool bKeepScriptTrains);
	void CommandClearAreaOfObjects(const scrVector & scrVecCoors, float Radius, int controlFlags=0);
	void CommandClearAreaOfPeds(const scrVector & scrVecCoors, float Radius, bool bBroadcast);
	int CommandGetFrameCount();
}

#endif	//	_COMMANDS_MISC_H_
