
// Rage headers
#include "Diag/output.h"
#include "parser/manager.h"
#include "parser/psofile.h"
//	#include "parser/psoparserbuilder.h"
#include "system/memory.h"

// Game headers
#include "camera/helpers/ControlHelper.h"
#include "control/stuntjump.h"
#include "data/aes.h"
#include "frontend/minimap.h"
#if __D3D11
#include "frontend/MiniMapRenderThread.h"
#endif //__D3D11
#include "game/cheat.h"
#include "game/config.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "network/Live/livemanager.h"
#include "Peds/ped.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/rendering/PedVariationStream.h"
#include "Peds/rendering/PedVariationPack.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"
#include "SaveLoad/savegame_data_parser.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "scene/ExtraContent.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "network/Network.h"
#include "Network/Live/NetworkTelemetry.h"
#include "stats/stats_channel.h"
#include "stats/StatsInterface.h"
#include "streaming/streaming.h"
#include "task/Motion/TaskMotionBase.h"
#include "weapons/Weapon.h"
#include "Objects/Door.h"
#include "scene/ExtraMetadataMgr.h"
#include "system/memmanager.h"
#include "system/TamperActions.h"
#include "network/Live/NetworkProfileStats.h"
#include "rline/profilestats/rlprofilestats.h"
#include "zlib/lz4.h"
#include "system/endian.h" 

#if __ASSERT
#include "SaveLoad/savegame_cloud.h"
#endif // __ASSERT

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

SAVEGAME_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(stat, savegame, DIAG_SEVERITY_DEBUG3)
#undef __stat_channel
#define __stat_channel stat_savegame


// extern RadioStationTrackList *g_WeatherReportTrackList;
// extern RadioStationTrackList *g_NewsReportTrackList;
// extern RadioStationTrackList *g_GenericAdvertTrackList;






// ****************************** CSaveGameData ****************************************************

//	u32			CSaveGameData::ms_WorkBufferPos = 0;
//	bool		CSaveGameData::ms_bFailed = false;
//	bool		CSaveGameData::ms_bCalculatingBufferSize = false;
//	u32		CSaveGameData::ms_CurrentBlockSize = 0;


//	const char *g_pNameOfDLCScriptTreeNode	= "DLCScriptSaveData";
//	const char *g_pNameOfMainScriptTreeNode	= "ScriptSaveData";

void CSaveGameData::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		INIT_PARSER;

//		ms_WorkBufferPos = 0;
//		ms_bFailed = false;
//		ms_bCalculatingBufferSize = false;
//		ms_CurrentBlockSize = 0;
	}

	CSaveGameMappingEnumsToHashStrings::Init(initMode);
	CScriptSaveData::Init(initMode);
}

void CSaveGameData::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		SHUTDOWN_PARSER;
	}

	CScriptSaveData::Shutdown(shutdownMode);
}

void CSaveGameData::Update()
{
	CScriptSaveData::Update();
}




// ****************************** CSaveGameMappingEnumsToHashStrings ****************************************************

const atHashString CSaveGameMappingEnumsToHashStrings::sm_ArrayOfHashStringsForPedTypeEnum[] = 
{
	atHashString("PEDTYPE_PLAYER_0",0xF2AAD816),
	atHashString("PEDTYPE_PLAYER_1",0x4F47CA9),
	atHashString("PEDTYPE_NETWORK_PLAYER",0x62ABF15E),
	atHashString("PEDTYPE_PLAYER_2",0x867EFFC0),
	atHashString("PEDTYPE_CIVMALE",0x3D3967FE),
	atHashString("PEDTYPE_CIVFEMALE",0x2C92602D),
	atHashString("PEDTYPE_COP",0xE7990F7A),
	atHashString("PEDTYPE_GANG_ALBANIAN",0x93592C0F),
	atHashString("PEDTYPE_GANG_BIKER_1",0x26FCF81B),
	atHashString("PEDTYPE_GANG_BIKER_2",0xF54694AF),
	atHashString("PEDTYPE_GANG_ITALIAN",0xF1720CA0),
	atHashString("PEDTYPE_GANG_RUSSIAN",0x3AD8F53F),
	atHashString("PEDTYPE_GANG_RUSSIAN_2",0xDB87028A),
	atHashString("PEDTYPE_GANG_IRISH",0xE1D11800),
	atHashString("PEDTYPE_GANG_JAMAICAN",0x927BCC51),
	atHashString("PEDTYPE_GANG_AFRICAN_AMERICAN",0xAA14C040),
	atHashString("PEDTYPE_GANG_KOREAN",0xBF5B6069),
	atHashString("PEDTYPE_GANG_CHINESE_JAPANESE",0x3DA306A7),
	atHashString("PEDTYPE_GANG_PUERTO_RICAN",0xB17741AB),
	atHashString("PEDTYPE_DEALER",0x5B9633F1),
	atHashString("PEDTYPE_MEDIC",0xBF33757C),
	atHashString("PEDTYPE_FIRE",0x4965ABBE),
	atHashString("PEDTYPE_CRIMINAL",0xB0590E08),
	atHashString("PEDTYPE_BUM",0x1337B6E8),
	atHashString("PEDTYPE_PROSTITUTE",0x1618DBB5),
	atHashString("PEDTYPE_SPECIAL",0xA5EEF470),
	atHashString("PEDTYPE_MISSION",0x5B0343FE),
	atHashString("PEDTYPE_SWAT",0x1E4BCBB6),
	atHashString("PEDTYPE_ANIMAL",0x4BF5CCE5),
	atHashString("PEDTYPE_ARMY",0x5ABF2FF1),
};

const atHashString CSaveGameMappingEnumsToHashStrings::sm_ArrayOfHashStringsForPedVarComp[] =
{
	atHashString("PV_COMP_HEAD",0x9E44B3BC),
	atHashString("PV_COMP_BERD",0xF1822755),
	atHashString("PV_COMP_HAIR",0x81821C89),
	atHashString("PV_COMP_UPPR",0x14AAAB7A),
	atHashString("PV_COMP_LOWR",0xDFE7EC9),
	atHashString("PV_COMP_HAND",0xF460079B),
	atHashString("PV_COMP_FEET",0x87AAB7C6),
	atHashString("PV_COMP_TEEF",0x4DE37CAB),
	atHashString("PV_COMP_ACCS",0x36CDCA),
	atHashString("PV_COMP_TASK",0x4541AB6D),
	atHashString("PV_COMP_DECL",0x61C4FE16),
	atHashString("PV_COMP_JBIB",0xE03BC491),
};

const atHashString CSaveGameMappingEnumsToHashStrings::sm_ArrayOfHashStringsForVehicleModType[] =
{
	atHashString("VMT_SPOILER",0x48DA4E22),
	atHashString("VMT_BUMPER_F",0x27253650),
	atHashString("VMT_BUMPER_R",0xF0E849DB),
	atHashString("VMT_SKIRT",0x99CEB500),
	atHashString("VMT_EXHAUST",0x4D685D86),
	atHashString("VMT_CHASSIS",0xAB4C3114),
	atHashString("VMT_GRILL",0x6A664D0),
	atHashString("VMT_BONNET",0x997875E),
	atHashString("VMT_WING_L",0x515AA159),
	atHashString("VMT_WING_R",0xC3B5060C),
	atHashString("VMT_ROOF",0x1090EEBA),
	atHashString("VMT_PLTHOLDER", 0xE05F1EF2),
	atHashString("VMT_PLTVANITY", 0x5E90D6F),
	atHashString("VMT_INTERIOR1", 0x82EA1AAD),
	atHashString("VMT_INTERIOR2", 0xB497FE08),
	atHashString("VMT_INTERIOR3", 0xDF6553A6),
	atHashString("VMT_INTERIOR4", 0x914D3773),
	atHashString("VMT_INTERIOR5", 0xD2ECBAB5),
	atHashString("VMT_SEATS", 0x443170F2),
	atHashString("VMT_STEERING", 0xEC2A15F9),
	atHashString("VMT_KNOB", 0x36C1751B),
	atHashString("VMT_PLAQUE", 0xD4328562),
	atHashString("VMT_ICE", 0x3C0B2D98),
	atHashString("VMT_TRUNK", 0x51528A24),
	atHashString("VMT_HYDRO", 0x4566602F),
	atHashString("VMT_ENGINEBAY1", 0x165FB44E),
	atHashString("VMT_ENGINEBAY2", 0x59C12C3),
	atHashString("VMT_ENGINEBAY3", 0x1BF93F7D),
	atHashString("VMT_CHASSIS2", 0x98BB6EE0),
	atHashString("VMT_CHASSIS3", 0x66648A33),
	atHashString("VMT_CHASSIS4", 0xB52327AF),
	atHashString("VMT_CHASSIS5", 0x82CF4308),
	atHashString("VMT_DOOR_L", 0xC91732C8),
	atHashString("VMT_DOOR_R", 0xB2570530),
	atHashString("VMT_LIGHTBAR", 0xE4E4A7A7),
	atHashString("VMT_LIVERY_MOD", 0x71c9eff4),
	atHashString("VMT_ENGINE",0xCB321888),
	atHashString("VMT_BRAKES",0xD721BFA5),
	atHashString("VMT_GEARBOX",0x542D449F),
	atHashString("VMT_HORN",0xB098A240),
	atHashString("VMT_SUSPENSION",0x102B8C00),
	atHashString("VMT_ARMOUR",0xEF1D4DB5),
	atHashString("VMT_NITROUS",0x3B6A8139),
	atHashString("VMT_TURBO",0xB189EF1F),
	atHashString("VMT_SUBWOOFER",0xEBC21EC0),
	atHashString("VMT_TYRE_SMOKE",0x698CBB32),
	atHashString("VMT_HYDRAULICS",0xEE286548),
	atHashString("VMT_XENON_LIGHTS",0x844AEC28),
	atHashString("VMT_WHEELS",0xD4613F38),
	atHashString("VMT_WHEELS_REAR",0xCBAD030A), // Keep the VMT_WHEELS_REAR as is to avoid game saving problem
};

void CSaveGameMappingEnumsToHashStrings::Init(unsigned initMode)
{
	if (INIT_CORE == initMode)
	{
		CompileTimeAssert(NELEM(sm_ArrayOfHashStringsForPedTypeEnum) == PEDTYPE_LAST_PEDTYPE);
		CompileTimeAssert(NELEM(sm_ArrayOfHashStringsForPedVarComp) == PV_MAX_COMP);
		CompileTimeAssert(NELEM(sm_ArrayOfHashStringsForVehicleModType) == VMT_MAX);
	}
	else if (INIT_SESSION == initMode)
	{
	}
}

s32 CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString(const atHashString *ArrayOfHashStrings, u32 numHashStrings, const atHashString &HashStringToSearchFor)
{
	for (u32 array_index = 0; array_index < numHashStrings; array_index++)
	{
		if (ArrayOfHashStrings[array_index] == HashStringToSearchFor)
		{
			return array_index;
		}
	}

	Assertf(0, "CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString - hash string %s not found", HashStringToSearchFor.GetCStr());
	return -1;
}

s32 CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_PedType(const atHashString &HashStringToSearchFor)
{
	return FindIndexFromHashString(sm_ArrayOfHashStringsForPedTypeEnum, NELEM(sm_ArrayOfHashStringsForPedTypeEnum), HashStringToSearchFor);
}

bool CSaveGameMappingEnumsToHashStrings::GetHashString_PedType(s32 Index, atHashString &ReturnedHashString)
{
	static atHashString ErrorHashString;
	if (Verifyf( (Index >= 0) && (Index < NELEM(sm_ArrayOfHashStringsForPedTypeEnum)), "CSaveGameMappingEnumsToHashStrings::GetHashString_PedType - Index %d is out of range (0 to %d)", Index, NELEM(sm_ArrayOfHashStringsForPedTypeEnum)))
	{
		ReturnedHashString = sm_ArrayOfHashStringsForPedTypeEnum[Index];
		return true;
	}

	ReturnedHashString = ErrorHashString;
	return false;
}

s32 CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_PedVarComp(const atHashString &HashStringToSearchFor)
{
	return FindIndexFromHashString(sm_ArrayOfHashStringsForPedVarComp, NELEM(sm_ArrayOfHashStringsForPedVarComp), HashStringToSearchFor);
}

bool CSaveGameMappingEnumsToHashStrings::GetHashString_PedVarComp(s32 Index, atHashString &ReturnedHashString)
{
	static atHashString ErrorHashString;
	if (Verifyf( (Index >= 0) && (Index < NELEM(sm_ArrayOfHashStringsForPedVarComp)), "CSaveGameMappingEnumsToHashStrings::GetHashString_PedVarComp - Index %d is out of range (0 to %d)", Index, NELEM(sm_ArrayOfHashStringsForPedVarComp)))
	{
		ReturnedHashString = sm_ArrayOfHashStringsForPedVarComp[Index];
		return true;
	}

	ReturnedHashString = ErrorHashString;
	return false;
}


s32 CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_VehicleModType(const atHashString &HashStringToSearchFor)
{
	return FindIndexFromHashString(sm_ArrayOfHashStringsForVehicleModType, NELEM(sm_ArrayOfHashStringsForVehicleModType), HashStringToSearchFor);
}

bool CSaveGameMappingEnumsToHashStrings::GetHashString_VehicleModType(s32 Index, atHashString &ReturnedHashString)
{
	static atHashString ErrorHashString;
	if (Verifyf( (Index >= 0) && (Index < NELEM(sm_ArrayOfHashStringsForVehicleModType)), "CSaveGameMappingEnumsToHashStrings::GetHashString_VehicleModType - Index %d is out of range (0 to %d)", Index, NELEM(sm_ArrayOfHashStringsForVehicleModType)))
	{
		ReturnedHashString = sm_ArrayOfHashStringsForVehicleModType[Index];
		return true;
	}

	ReturnedHashString = ErrorHashString;
	return false;
}

