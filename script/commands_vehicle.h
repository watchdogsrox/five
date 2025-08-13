#ifndef _COMMANDS_VEHICLE_H_
#define _COMMANDS_VEHICLE_H_

// Forward declarations
class CVehicle;

namespace vehicle_commands
{
	void DeleteScriptVehicle(CVehicle *pVehicle);

	void CommandSoundCarHorn(int VehicleIndex, int TimeToSoundHorn, int HornTypeHash = 0, bool IsMusicalHorn = false);

	void SetupScriptCommands();
}

#endif
