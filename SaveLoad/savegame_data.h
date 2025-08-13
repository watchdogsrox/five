

#ifndef SAVEGAME_DATA_H
#define SAVEGAME_DATA_H


// Rage headers
#include "atl/hashstring.h"
#include "parser/psobuilder.h"
#include "parser/psodata.h"
#include "parser/rtstructure.h"
#include "script/value.h"
#include "spatialdata/aabb.h"
#include "vector/vector3.h"

// Game headers
#include "audio/audiodefines.h"
#include "audio/radiostation.h"
#include "control/garages.h"
#include "frontend/ProfileSettings.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "Peds/PedType.h"
#include "Peds/rendering/PedVariationDS.h"
#include "Peds/rendering/PedDamage.h"
#include "SaveLoad/savegame_defines.h"
#include "scene/ExtraContent.h"
//	#include "scene/ExtraMetadataMgr.h"		//	for __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
#include "stats/StatsDataMgr.h"
#include "Vehicles/vehiclestorage.h"
#include "game/Wanted.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "stats/StatsMgr.h"



class CSaveGameMappingEnumsToHashStrings
{
public:
//	Public functions
	static void Init(unsigned initMode);

	static s32 FindIndexFromHashString_PedType(const atHashString &HashStringToSearchFor);
	static bool GetHashString_PedType(s32 Index, atHashString &ReturnedHashString);

	static s32 FindIndexFromHashString_PedVarComp(const atHashString &HashStringToSearchFor);
	static bool GetHashString_PedVarComp(s32 Index, atHashString &ReturnedHashString);

	static s32 FindIndexFromHashString_VehicleModType(const atHashString &HashStringToSearchFor);
	static bool GetHashString_VehicleModType(s32 Index, atHashString &ReturnedHashString);

private:
//	Private functions
	static s32 FindIndexFromHashString(const atHashString *ArrayOfHashStrings, u32 numHashStrings, const atHashString &HashStringToSearchFor);

//	Private data
	static const atHashString sm_ArrayOfHashStringsForPedTypeEnum[];	//	PEDTYPE_LAST_PEDTYPE];
	static const atHashString sm_ArrayOfHashStringsForPedVarComp[];	//	PV_MAX_COMP];
	static const atHashString sm_ArrayOfHashStringsForVehicleModType[];	//		VMT_MAX];
};

class CSaveGameData
{
private:

public:
//	Public functions
	static void		Init(unsigned initMode);
	static void		Shutdown(unsigned shutdownMode);

	static void		Update();

	static const char *GetNameOfDLCScriptTreeNode() { return "DLCScriptSaveData"; }
	static const char *GetNameOfMainScriptTreeNode() { return "ScriptSaveData"; }
};

#endif	//	SAVEGAME_DATA_H

