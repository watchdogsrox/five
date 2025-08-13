
// Rage headers
#include "script\wrapper.h"
// Game headers
#include "peds\gangs.h"

namespace gang_commands
{

void CommandSetCreateRandomGangMembers(bool UNUSED_PARAM(bOn))
{
//	CPopulation::m_bDontCreateRandomGangMembers = !bOn;
}

void CommandSetOnlyCreateRandomGangMembers(bool UNUSED_PARAM(bOn))
{
//	CPopulation::m_bOnlyCreateRandomGangMembers = bOn;
}

void CommandSetGangWeapons(int GangID, int FirstWeapon, int SecondWeapon, int ThirdWeapon)
{
	Assertf(0, "SET_GANG_WEAPONS: No longer available" );

	((void)GangID);
	((void)FirstWeapon);
	((void)SecondWeapon);
	((void)ThirdWeapon);
	//CGangs::SetGangWeapons(GangID, FirstWeapon, SecondWeapon, ThirdWeapon);	
}


	void SetupScriptCommands()
	{
		SCR_REGISTER_UNUSED(SET_CREATE_RANDOM_GANG_MEMBERS, 0x3adc6d2d, CommandSetCreateRandomGangMembers);
		SCR_REGISTER_UNUSED(SET_ONLY_CREATE_RANDOM_GANG_MEMBERS, 0xfcce1d7, CommandSetOnlyCreateRandomGangMembers);
		SCR_REGISTER_UNUSED(SET_GANG_WEAPONS, 0x53bfa346, CommandSetGangWeapons);
	}
}	//	end of namespace gang_commands
