// Title	:	DispatchEnums.h
// Started	:	13/05/2010

#ifndef INC_DISPATCH_ENUMS_H_
#define INC_DISPATCH_ENUMS_H_

// Keep this in sync with script enums in commands_misc.sch and dispatchdata.psc (in the XML data at the top)
enum DispatchType
{
	DT_Invalid = 0,
	DT_PoliceAutomobile,
	DT_PoliceHelicopter,
	DT_FireDepartment,
	DT_SwatAutomobile,
	DT_AmbulanceDepartment,
	DT_PoliceRiders,
	DT_PoliceVehicleRequest,
	DT_PoliceRoadBlock,
	DT_PoliceAutomobileWaitPulledOver,
	DT_PoliceAutomobileWaitCruising,
	DT_Gangs,
	DT_SwatHelicopter,
	DT_PoliceBoat,
	DT_ArmyVehicle,
	DT_BikerBackup,
	DT_Assassins,
	DT_Max
};

// Keep this in sync with script enums in commands_misc.sch and dispatchdata.psc (in the XML data at the top)
enum DispatchAssassinLevel
{
	AL_Invalid = 0,
	AL_Low,
	AL_Med,
	AL_High,
	AL_Max
};

#endif
