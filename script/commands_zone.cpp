
// Rage headers
#include "script\wrapper.h"
#include "script\script_channel.h"
// Game headers
#include "game\zones.h"
#include "game\MapZones.h"
#include "script\script.h"
#include "script/script_helper.h"
#include "peds\popcycle.h"
#include "peds/popzones.h"
#include "streaming/populationstreaming.h"
#include "network/networkinterface.h"
#include "script/Handlers/GameScriptResources.h"

namespace zone_commands
{


int CommandGetZoneAtCoords(const scrVector & coords)
{
	Vector3 TempVec = Vector3(coords);
	CPopZone* pZone = CPopZones::FindSmallestForPosition(&TempVec, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
	return CPopZones::GetPopZoneId(pZone);
}

int CommandGetZoneFromNameId(const char* pNameId)
{
	CPopZone* pZone = CPopZones::GetPopZone(pNameId);
	if(scriptVerifyf(pZone, "Population zone %s does not exist", pNameId))
	{
		return CPopZones::GetPopZoneId(pZone);
	}
	return CPopZones::INVALID_ZONEID;
}

int CommandGetZonePopSchedule(int zoneId)
{
	CPopZone* pZone = CPopZones::GetPopZone(zoneId);
	int scheduleId = -1;
	if(scriptVerifyf(pZone != NULL, "SET_ZONE_SCUMMINESS: Invalid zone id passed"))
	{
		if(NetworkInterface::IsGameInProgress())
			scheduleId = CPopZones::GetPopZonePopScheduleIndexForMP(pZone);
		else
			scheduleId = CPopZones::GetPopZonePopScheduleIndexForSP(pZone);
	}
	return scheduleId;
}

int CommandGetMPGangTerritoryIndex(s32 zoneId)
{
	CPopZone* pZone = CPopZones::GetPopZone(zoneId);
	if(SCRIPT_VERIFY(pZone, "GET_MP_GANG_TERRITORY_INDEX - Invalid zone id passed"))
	{
		return pZone->m_MPGangTerritoryIndex;
	}

	return -1;
}

void CommandSetZoneCopsActive(int zoneId, bool AllowCops)
{
	if(scriptVerifyf(zoneId != CPopZones::INVALID_ZONEID, "SET_ZONE_COPS_ACTIVE: Invalid zone id passed"))
		CPopZones::SetAllowCops(zoneId, AllowCops);
}

const char *CommandGetNameOfZone(const scrVector & scrVecCoors)
{
	CPopZone *pZone;
	static char RetString[16];

	Vector3 TempVec = Vector3(scrVecCoors);
	pZone = CPopZones::FindSmallestForPosition(&TempVec, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
    if(pZone)
    {
		return pZone->m_associatedTextId.TryGetCStr();
    }
    else
    {
        strcpy(RetString, "");
    }
	return (const char *)&RetString;
}

const char *CommandGetNameOfInfoZone(const scrVector & scrVecCoors)
{
	CPopZone *pZone;
	static char RetString[16];

	Vector3 TempVec = Vector3(scrVecCoors);
	pZone = CPopZones::FindSmallestForPosition(&TempVec, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
    if(pZone)
    {
        return pZone->m_id.TryGetCStr();
    }
    else
    {
        strcpy(RetString, "");
    }
	return (const char *)&RetString;
}


void CommandSetZoneEnabled(int zoneId, bool bEnabled)
{
	if(scriptVerifyf(zoneId != CPopZones::INVALID_ZONEID, "SET_ZONE_ENABLED: Invalid zone id passed"))
		CPopZones::SetEnabled(zoneId, bEnabled);
}

bool CommandGetZoneEnabled(int zoneId)
{
	bool	bEnabled = false;
	if(scriptVerifyf(zoneId != CPopZones::INVALID_ZONEID, "GET_ZONE_ENABLED: Invalid zone id passed"))
	{
		bEnabled = CPopZones::GetEnabled(zoneId);
	}
	return  bEnabled;
}

int CommandGetZoneScumminess(int zoneId)
{
	int scumminess = SCUMMINESS_ABOVE_AVERAGE;
	if (scriptVerifyf(zoneId != CPopZones::INVALID_ZONEID, "GET_ZONE_SCUMMINESS: Invalid zone id passed"))
	{
		scumminess = CPopZones::GetScumminess(zoneId);
	}
	return scumminess;
}

void CommandOverridePopScheduleGroups(int popSchedule, const char* pGroupName, int percentage)
{
	if(scriptVerifyf(popSchedule != CPopScheduleList::INVALID_SCHEDULE, "OVERRIDE_POPSCHEDULE_GROUPS: Invalid pop schedule"))
	{
		u32 popGroup;
		if(Verifyf(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(atHashValue(pGroupName), popGroup), "Ped popgroup %s does not exist", pGroupName))
		{
			CScriptResource_PopScheduleOverride override(popSchedule, popGroup, percentage);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(override);

            if(NetworkInterface::IsGameInProgress())
            {
#if !__FINAL
                scriptDebugf1("%s: Overriding pop group percentage over network: Schedule %d, Group %s, Percentage %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), popSchedule, pGroupName, percentage);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
                NetworkInterface::OverridePopGroupPercentageOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), popSchedule, popGroup, static_cast<u32>(percentage));
            }
		}
	}
}

void CommandClearPopScheduleOverride(int popScheduleID)
{
	if(scriptVerifyf(popScheduleID != CPopScheduleList::INVALID_SCHEDULE, "CLEAR_POPSCHEDULE_OVERRIDE: Invalid pop schedule"))
	{
        CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(popScheduleID);

        const u32 popGroupHash = popSchedule.GetOverridePedGroup();
        const int percentage   = popSchedule.GetOverridePedPercentage();

		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE, popScheduleID);

        if(NetworkInterface::IsGameInProgress())
        {
#if !__FINAL
            scriptDebugf1("%s: Clearing overridden pop group percentage over network: Schedule %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), popScheduleID);
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            NetworkInterface::RemovePopGroupPercentageOverrideOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), popScheduleID, popGroupHash, percentage);
        }
	}
}

void CommandOverridePopScheduleVehicleModel(int popSchedule, int ModelHashKey)
{
	if(scriptVerifyf(popSchedule != CPopScheduleList::INVALID_SCHEDULE, "OVERRIDE_POPSCHEDULE_VEHICLE_MODEL: Invalid pop schedule") &&
       scriptVerifyf(NetworkInterface::IsGameInProgress(), "OVERRIDE_POPSCHEDULE_VEHICLE_MODEL is only supported in network games!"))
	{
        fwModelId ModelId;
        CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);

        if (scriptVerifyf(pModelInfo, "%s: OVERRIDE_POPSCHEDULE_VEHICLE_MODEL - model with hash %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey))
        {
	        if (scriptVerifyf(ModelId.IsValid(), "%s: OVERRIDE_POPSCHEDULE_VEHICLE_MODEL - model with hash %d exists but its model index is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey))
	        {
		        CScriptResource_PopScheduleVehicleModelOverride override(popSchedule, ModelId.GetModelIndex());
                CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(override);
            }
        }
	}
}

void CommandClearPopScheduleVehicleModelOverride(int popScheduleID)
{
	if(scriptVerifyf(popScheduleID != CPopScheduleList::INVALID_SCHEDULE, "CLEAR_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL: Invalid pop schedule") &&
       scriptVerifyf(NetworkInterface::IsGameInProgress(), "CLEAR_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL is only supported in network games!"))
	{
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL, popScheduleID);
	}
}

int CommandGetMapAreaFromCoords(const scrVector & scrVecInCoords)
{
	Vector3 vecInCoords = Vector3(scrVecInCoords);

	return CMapAreas::GetAreaIndexFromPosition(vecInCoords);
}

s32 CommandGetHashOfMapAreaAtCoords(const scrVector & scrVecInCoords)
{
	Vector3 vecInCoords = Vector3(scrVecInCoords);

	return ( (s32) CMapAreas::GetAreaNameHashFromPosition(vecInCoords));
}

void CommandOverrideMaxScenarioPedModels(int maxPedModels)
{
	gPopStreaming.SetMaxScenarioPedModelsLoadedOverride(maxPedModels);
}

void CommandClearMaxScenarioPedModelsOverride()
{
	gPopStreaming.SetMaxScenarioPedModelsLoadedOverride(-1);
}


s32 CommandGetMapZoneAtCoords(const scrVector & coords)
{
	Vector3 TempVec = Vector3(coords);
	return (s32)CMapZoneManager::GetZoneAtCoords(TempVec);
}


void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(GET_ZONE_AT_COORDS,0xb3aaef02e08ea464, CommandGetZoneAtCoords);
	SCR_REGISTER_SECURE(GET_ZONE_FROM_NAME_ID,0xbfcaed06c67f8431, CommandGetZoneFromNameId);
	SCR_REGISTER_SECURE(GET_ZONE_POPSCHEDULE,0x12fecc81be9413ba, CommandGetZonePopSchedule);
	SCR_REGISTER_UNUSED(GET_MP_GANG_TERRITORY_INDEX,0xf95d159de2e42944, CommandGetMPGangTerritoryIndex);
	SCR_REGISTER_UNUSED(SET_ZONE_COPS_ACTIVE,0x6344d7f88e052f02, CommandSetZoneCopsActive);
	SCR_REGISTER_SECURE(GET_NAME_OF_ZONE,0x717dd0abf4a97737, CommandGetNameOfZone);
	SCR_REGISTER_UNUSED(GET_NAME_OF_INFO_ZONE,0xe57312689aadf2d1, CommandGetNameOfInfoZone);
	SCR_REGISTER_SECURE(SET_ZONE_ENABLED,0x1524eea9eac1f06b, CommandSetZoneEnabled);
	SCR_REGISTER_UNUSED(GET_ZONE_ENABLED,0x953d4ed6832c90df, CommandGetZoneEnabled);
	SCR_REGISTER_SECURE(GET_ZONE_SCUMMINESS,0x0f2e6657dc3695b8, CommandGetZoneScumminess);

	SCR_REGISTER_UNUSED(OVERRIDE_POPSCHEDULE_GROUPS,0x4f82f932d29836a2, CommandOverridePopScheduleGroups);
	SCR_REGISTER_UNUSED(CLEAR_POPSCHEDULE_OVERRIDE,0xf0fa86d937eb69e9, CommandClearPopScheduleOverride);
    SCR_REGISTER_SECURE(OVERRIDE_POPSCHEDULE_VEHICLE_MODEL,0xe81f6467ad1c34fc, CommandOverridePopScheduleVehicleModel);
    SCR_REGISTER_SECURE(CLEAR_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL,0x70fca19a938a5df3, CommandClearPopScheduleVehicleModelOverride);

	SCR_REGISTER_UNUSED(GET_MAP_AREA_FROM_COORDS,0x68a9d79c339625c7, CommandGetMapAreaFromCoords);
	SCR_REGISTER_SECURE(GET_HASH_OF_MAP_AREA_AT_COORDS,0x5e43fce03b0cd999, CommandGetHashOfMapAreaAtCoords);

	SCR_REGISTER_UNUSED(OVERRIDE_MAX_SCENARIO_PED_MODELS,0xa98cd79ad6e91097, CommandOverrideMaxScenarioPedModels);
	SCR_REGISTER_UNUSED(CLEAR_MAX_SCENARIO_PED_MODELS_OVERRIDE,0xdc1b7fb785d72e92, CommandClearMaxScenarioPedModelsOverride);

	SCR_REGISTER_UNUSED(GET_MAP_ZONE_AT_COORDS,0xa7570ade3c568784,	 CommandGetMapZoneAtCoords);
}
}
