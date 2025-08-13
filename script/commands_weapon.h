//
// filename:	commands_weapon.h
// description:	
//

#ifndef INC_COMMANDS_WEAPON_H_
#define INC_COMMANDS_WEAPON_H_

// --- Include Files ------------------------------------------------------------


// --- Globals ------------------------------------------------------------------

namespace weapon_commands
{
	void CommandPedDropsInventoryWeapon(int PedIndex, int TypeOfWeapon, const scrVector &offset, int ammoAmount);
	void SetupScriptCommands();
}


#endif // !INC_COMMANDS_WEAPON_H_
