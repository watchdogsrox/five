#ifndef SCRIPT_COMMANDS_SHAPETEST_H
#define SCRIPT_COMMANDS_SHAPETEST_H

#include "script/script_shapetests.h"

//-----------------------------------------------------------------------------

namespace shapetest_commands
{
	// Make sure this is in sync with commands_shapetest.sch
	enum 
	{
		SCRIPT_INCLUDE_MOVER = 1,
		SCRIPT_INCLUDE_VEHICLE = 2,
		SCRIPT_INCLUDE_PED = 4,
		SCRIPT_INCLUDE_RAGDOLL = 8,
		SCRIPT_INCLUDE_OBJECT = 16,
		SCRIPT_INCLUDE_PICKUP = 32,
		SCRIPT_INCLUDE_GLASS = 64,
		SCRIPT_INCLUDE_RIVER = 128,
		SCRIPT_INCLUDE_FOLIAGE = 256,
		SCRIPT_INCLUDE_ALL = 511,
	};

	// Make sure this is in sync with commands_shapetest.sch
	enum
	{
		SCRIPT_SHAPETEST_OPTION_IGNORE_GLASS		= 1,
		SCRIPT_SHAPETEST_OPTION_IGNORE_SEE_THROUGH	= 2,
		SCRIPT_SHAPETEST_OPTION_IGNORE_NO_COLLISION	= 4,
		
		SCRIPT_SHAPETEST_OPTION_DEFAULT				= SCRIPT_SHAPETEST_OPTION_IGNORE_GLASS | SCRIPT_SHAPETEST_OPTION_IGNORE_SEE_THROUGH | SCRIPT_SHAPETEST_OPTION_IGNORE_NO_COLLISION,
	};

	u32 GetPhysicsFlags(s32 LOSFlags);
	u32 GetShapeTestOptionFlags(s32 nOptionFlags);
	void SetupScriptCommands();

}	// namespace shapetest_commands

//-----------------------------------------------------------------------------

#endif	// SCRIPT_COMMANDS_SHAPETEST_H
