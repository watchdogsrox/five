#ifndef _COMMANDS_WATER_H_
#define _COMMANDS_WATER_H_

namespace water_commands
{
	//Make sure this is insync with commands_water.sch
	enum
	{
		SCRIPT_WATER_TEST_RESULT_NONE = 0,	// The query hit nothing
		SCRIPT_WATER_TEST_RESULT_WATER,		// The query hit water
		SCRIPT_WATER_TEST_RESULT_BLOCKED	// The query hit something before hitting water
	};
	void SetupScriptCommands();
}

#endif	//	_COMMANDS_WATER_H_

