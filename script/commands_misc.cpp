
#if __XENON
#include "system/xtl.h"
#endif

// Rage headers
#include "script/wrapper.h"
#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "input/virtualkeyboard.h"
#include "spatialdata/aabb.h"
#include "spatialdata/sphere.h"
#include "string/unicode.h"
#include "vector/quaternion.h"
#include "diag/channel.h"
#include "physics/collisionoverlaptest.h"

// Game headers
#include "control/gamelogic.h"
#include "control/restart.h"
#include "control/stuntjump.h"
#include "core/game.h"
#include "frontend/Credits.h"
#include "frontend/FrontendStatsMgr.h"
#include "frontend/ProfileSettings.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#if RSG_PC
#include "frontend/TextInputBox.h"
#endif // RSG_PC
#include "game/Clock.h"
#include "game/Dispatch/DispatchData.h"
#include "game/Dispatch/DispatchEnums.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/DispatchServices.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/incidents.h"
#include "Game/localisation.h"
#include "game/ModelIndices.h"
#include "game/Performance.h"
#include "game/Riots.h"
#include "game/weather.h"
#include "game/cheat.h"
#include "game/zones.h"
#include "task/Movement/Climbing/ClimbDebug.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/General/NetworkPendingProjectiles.h"
#include "objects/objectpopulation.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "Peds/PedIntelligence/PedAILodManager.h"
#include "Peds/PedIntelligence.h"
#include "peds/peddebugvisualiser.h"
#include "peds/pedpopulation.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_queue.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "scene/entity.h"
#include "scene/extracontent.h"
#include "scene/world/GameWorld.h"
#include "scene/InstancePriority.h"
#include "fwscene/search/SearchVolumes.h"
#include "script/handlers/GameScriptResources.h"
#include "script/script_brains.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "script/ScriptMetadata.h"
#include "script/ScriptMetadataManager.h"
#include "streaming/streaming.h"
#include "system/FileMgr.h"
#include "system/appcontent.h"
#include "system/service.h"
#include "system/StreamingInstall.winrt.h"
#include "fwsys/timer.h"
#include "task/Combat/TacticalAnalysis.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "task/Motion/Locomotion/TaskMotionTennis.h"
#include "task/Scenario/ScenarioManager.h"
#include "text/TextConversion.h"
#include "text/messages.h"
#include "vehicles/vehiclepopulation.h"
#include "weapons/Bullet.h"
#include "weapons/projectiles/Projectile.h"
#include "weapons/Projectiles/ProjectileManager.h"
#include "streaming/populationstreaming.h"
#include "vehicles/VehicleFactory.h"
#include "vehicleAi/pathFind.h"
#include "Vfx/Misc/Scrollbars.h"
#include "Vfx/Clouds/Clouds.h"
#include "Vfx/Clouds/CloudHat.h"

#include "game/popmultiplierareas.h"

#include "network/NetworkInterface.h"
#include "network/events/NetworkEventTypes.h"
#include "system/EndUserBenchmark.h"
#include "diag/channel.h"
#include "SaveMigration/SaveMigration.h"
#if GEN9_STANDALONE_ENABLED
#include "control/ScriptRouterContext.h"
#include "control/ScriptRouterLink.h"

#endif

#if RSG_ORBIS
// used by activity feed scripts
#include "fwlocalisation/languagePack.h"
#include "fwnet/netplayer.h"
#endif

#if DR_ENABLED
#include "physics/debugPlayback.h"
#include "physics/debugevents.h"
#include "commands_misc.h"
#endif

SCRIPT_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

#if RSG_SCE
// For the PS, we need to be able to launch content when launched from the a LiveItem.
PARAM(LiveAreaLoadContent, "[LiveArea] The content id of the content to be loaded.");
#endif // RSG_SCE

#if !GEN9_STANDALONE_ENABLED
// I don't know if this is required.
// Maybe the two natives below that I've added this for could take any type of parameter since it doesn't get used.
struct DummyScriptRouterContextData {
	scrValue m_ScriptRouterSource;		// stored in .Int
	scrValue m_ScriptRouterMode;		// stored in .Int
	scrValue m_ScriptRouterArgType;		// stored in .Int
	scrTextLabel63 m_ScriptRouterArg;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(DummyScriptRouterContextData);

#endif // !GEN9_STANDALONE_ENABLED

namespace misc_commands
{

int CommandAddHospitalRestart(const scrVector & scrVecCoors, float Heading, int WhenToUse)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	// We're quite forgiving on script passing in a number in a good range
	Assertf(Heading >= -360.0f && Heading <= 360.0f, "ADD_HOSPITAL_RESTART: Invalid desired heading: %f",Heading);
	float fHeadingRad = fwAngle::LimitRadianAngleSafe(Heading * DtoR);

	if(SCRIPT_VERIFY(((WhenToUse >=0) && (WhenToUse<=3)), "ADD_HOSPITAL_RESTART - Invalid Level"))
	{
		if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
		{
			VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
		}
		
		return CRestart::AddHospitalRestartPoint(VecPos, fHeadingRad, WhenToUse);
	}

	return -1;
}

void CommandDisableHospitalRestart(int index, bool bDisable)
{
	CRestart::DisableHospitalRestartPoint(index, bDisable);
}


int CommandAddPoliceRestart(const scrVector & scrVecCoors, float Heading, int WhenToUse)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	// We're quite forgiving on script passing in a number in a good range
	Assertf(Heading >= -360.0f && Heading <= 360.0f, "ADD_POLICE_RESTART: Invalid desired heading: %f",Heading);
	float fHeadingRad = fwAngle::LimitRadianAngleSafe(Heading * DtoR);

	if(SCRIPT_VERIFY(((WhenToUse >=0) && (WhenToUse<=3)), "ADD_POLICE_RESTART - Invalid Level"))
	{	
		if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
		{
			VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
		}

		return CRestart::AddPoliceRestartPoint(VecPos, fHeadingRad, WhenToUse);
	}

	return -1;
}

void CommandDisablePoliceRestart(int index, bool bDisable)
{
	CRestart::DisablePoliceRestartPoint(index, bDisable);
}


void CommandOverrideNextRestart(const scrVector & scrVecCoors, float Heading)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
	}

	// We're quite forgiving on script passing in a number in a good range
	Assertf(Heading >= -360.0f && Heading <= 360.0f, "OVERRIDE_NEXT_RESTART: Invalid desired heading: %f",Heading);
	float fHeadingRad = fwAngle::LimitRadianAngleSafe(Heading * DtoR);

	CRestart::OverrideNextRestart(VecPos, fHeadingRad);
}

void CommandCancelOverrideRestart()
{
	CRestart::CancelOverrideRestart();
}

void CommandPauseDeathArrestRestart(bool bPaused)
{
	CRestart::bPausePlayerRespawnAfterDeathArrest = bPaused;
}

void CommandIgnoreNextRestart(bool bIgnore)
{
	CRestart::SetIgnoreNextRestart(bIgnore);
}

void CommandSetFadeOutAfterDeath(bool bSuppressFade)
{
	bSuppressFade = !bSuppressFade;
	CRestart::bSuppressFadeOutAfterDeath = bSuppressFade;
}

void CommandSetFadeOutAfterArrest(bool bSuppressFade)
{
	bSuppressFade = !bSuppressFade;
	CRestart::bSuppressFadeOutAfterArrest = bSuppressFade;
}

void CommandSetFadeInAfterDeathArrest(bool bSuppressFade)
{
	bSuppressFade = !bSuppressFade;

	CRestart::bSuppressFadeInAfterDeathArrest = bSuppressFade;
}

void CommandSetFadeInAfterLoad(bool bFadeIn)
{
	if (bFadeIn)
	{
		CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad = true;
	}
	else
	{
		CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad = false;
	}
}


int CommmandRegisterSaveHouse(const scrVector & scrVecCoors, float Heading, const char *pRoomName, int MapAreaName, s32 playerModelNameHash)	//	bronx, brooklyn, manhat, nj
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	if(SCRIPT_VERIFY(((MapAreaName >=0) && (MapAreaName<=3)), "REGISTER_SAVE_HOUSE - Invalid Level"))
	{
		return CRestart::AddSaveHouse(VecPos, Heading, pRoomName, MapAreaName, (u32) playerModelNameHash);
	}
	return -1;
}

void CommandEnableSaveHouse(int SaveHouseIndex, bool bNewEnabledFlag, bool bAvailableForAutosaves)
{
	CRestart::SetSaveHouseEnabled(SaveHouseIndex, bNewEnabledFlag, bAvailableForAutosaves);
}

bool CommandOverrideSaveHouse(bool bOverride, const scrVector & scrVecCoors, float fHeading, bool bIsAnAutosave, Vector3 &vecReturnCoords, float &fReturnHeading)
{
	Vector3 VecPos = Vector3(scrVecCoors);
	CRestart::SetSavehouseOverride(bOverride, VecPos, fHeading);

	return CGenericGameStorage::FindClosestSaveHouse(bIsAnAutosave, vecReturnCoords, fReturnHeading);
}

//	Script BOOLs use 4 bytes so BOOL& on the script side becomes s32& on the code side
bool CommandGetSaveHouseDetailsAfterSuccessfulLoad(Vector3 &vecReturnCoords, float &fReturnHeading, s32 &bReturnFadeIn, s32 &bReturnSnapToGround)
{
	vecReturnCoords = CGenericGameStorage::ms_vCoordsOfClosestSaveHouse;
	fReturnHeading = CGenericGameStorage::ms_fHeadingOfClosestSaveHouse;
	
	if (CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad)
	{
		bReturnFadeIn = 1;
	}
	else
	{
		bReturnFadeIn = 0;
	}

	if (CGenericGameStorage::ms_bPlayerShouldSnapToGroundOnSpawn)
	{
		bReturnSnapToGround = 1;
	}
	else
	{
		bReturnSnapToGround = 0;
	}

	return CGenericGameStorage::ms_bClosestSaveHouseDataIsValid;
}


bool IsCurrentPlayerModelValid()
{
	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	CPed *pPlayerPed = pPlayerInfo ? pPlayerInfo->GetPlayerPed() : NULL;

	if (Verifyf(pPlayerPed, "IsCurrentPlayerModelValid - failed to find the player ped"))
	{
		CPedModelInfo* mi = pPlayerPed->GetPedModelInfo();
		if(Verifyf(mi, "IsCurrentPlayerModelValid - failed to find model info for player ped"))
		{
			u32 modelNameHash = mi->GetModelNameHash();
			if ( (modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName())
					|| (modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName())
					|| (modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName()) )
			{
				return true;
			}

			Assertf(0, "IsCurrentPlayerModelValid - checking player model before autosaving but it's %s so the save won't be made", mi->GetModelName());
		}
	}

	return false;
}

void CommandDoAutoSave()
{
	//	check that the player is signed in
	//	check that the device is still available

	//	could ask Derek if there's a good place to call frontend type code that needs to display while the game is running
	//	will have to bring up a message saying don't remove your memory card, switch off etc.
#if __BANK
	if (CScriptDebug::GetAutoSaveEnabled() == false)
	{
		return;
	}
#endif

	if (CPauseMenu::GetMenuPreference(PREF_AUTOSAVE) == false)
	{	//	autosave has been disabled so return immediately without queueing an operation
		return;
	}

	if (CNetwork::IsNetworkOpen() || NetworkInterface::IsAnySessionActive())
	{
		scriptAssertf(0, "DO_AUTO_SAVE - can't queue a single player save while any network session is active");
		return;
	}

	if (!IsCurrentPlayerModelValid())
	{
		return;
	}
	
	CGenericGameStorage::QueueAutosave();
}

bool CommandGetIsAutoSaveOff()
{
	if (CPauseMenu::GetMenuPreference(PREF_AUTOSAVE) == false)
	{
		return true;
	}

	return false;
}

bool CommandGetIsDisplayingSaveMessage()
{
	return CSavingMessage::IsDisplayingSavingMessage();
}

bool CommandIsAutoSaveInProgress()
{
	if ( (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)	//	Return true if autosave is at the head of the savegame queue
		|| CSavegameQueue::ContainsAnAutosave())		//	or is anywhere in the queue. Maybe I only really need to do this second check.
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CommandHasCodeRequestedAutosave()
{
	return CTheScripts::GetCodeRequestedAutoSave();
}

void CommandClearCodeRequestedAutosave()
{
	CTheScripts::SetCodeRequestedAutoSave(false);
}

//	When a Replay Mission is passed
void CommandBeginReplayStats(s32 missionId, s32 missionType)
{
	CTheScripts::GetMissionReplayStats().BeginReplayStatsStructure(missionId, missionType);
}

void CommandAddReplayStatValue(s32 valueOfStat)
{
	CTheScripts::GetMissionReplayStats().AddStatValueToReplayStatsStructure(valueOfStat);
}

void CommandEndReplayStats()
{
	CTheScripts::GetMissionReplayStats().EndReplayStatsStructure();
}
//	End of "When a Replay Mission is passed"


//	When a Load happens and the script detects that it’s a Replay save file
bool CommandHaveReplayStatsBeenStored()
{
	return CTheScripts::GetMissionReplayStats().HaveReplayStatsBeenStored();
}

s32 CommandGetReplayStatMissionId()
{
	return CTheScripts::GetMissionReplayStats().GetReplayStatMissionId();
}

s32 CommandGetReplayStatMissionType()
{
	return CTheScripts::GetMissionReplayStats().GetReplayStatMissionType();
}

s32 CommandGetReplayStatCount()
{
	return CTheScripts::GetMissionReplayStats().GetNumberOfMissionStats();
}

s32 CommandGetReplayStatAtIndex(s32 arrayIndex)
{
	return CTheScripts::GetMissionReplayStats().GetStatValueAtIndex(arrayIndex);
}

// So that HAVE_REPLAY_STATS_BEEN_STORED() doesn't return TRUE on the next normal load
void CommandClearReplayStats()
{
	CTheScripts::GetMissionReplayStats().ClearReplayStatsStructure();
}
//	End of "When a Load happens and the script detects that it's a Replay save file"



bool CommandQueueMissionRepeatLoad()
{
	return CGenericGameStorage::QueueMissionRepeatLoad();
}

s32 CommandGetStatusOfMissionRepeatLoad()
{
	return CGenericGameStorage::GetMissionRepeatLoadStatus();
}

void CommandClearStatusOfMissionRepeatLoad()
{
	CGenericGameStorage::ClearMissionRepeatLoadStatus();
}

bool CommandQueueMissionRepeatSave()
{
	return CGenericGameStorage::QueueMissionRepeatSave(false);
}

bool CommandQueueMissionRepeatSaveForBenchmarkTest()
{
	return CGenericGameStorage::QueueMissionRepeatSave(true);
}

s32 CommandGetStatusOfMissionRepeatSave()
{
	return CGenericGameStorage::GetMissionRepeatSaveStatus();
}

void CommandClearStatusOfMissionRepeatSave()
{
	CGenericGameStorage::ClearMissionRepeatSaveStatus();
}


bool CommandCanStartMissionPassedTune()
{
	//	Script can play tune if autosave is off
	//	Or the spinning disc message is displayed (which should mean that all warning screens have been done)
	//	The script will probably need to check Keith's flag about do autosave when possible
	//	If this flag is false then it's safe to play the tune
	//	If this flag is true then check CAN_START_MISSION_PASSED_TUNE to decide
	if (CPauseMenu::GetMenuPreference(PREF_AUTOSAVE) == false)
	{
		return true;
	}

	if (CSavingMessage::IsDisplayingSavingMessage())
	{
		return true;
	}

	return false;
}

bool CommandIsMemoryCardInUse()
{
	if (CGenericGameStorage::IsStorageDeviceBeingAccessed())
	{
		return true;
	}

	return false;
}

void CommandSetRandomSeed(int NewSeed)
{
	CRandomScripts::SetRandomSeed(NewSeed);
}

void CommandSetRandomMWCSeed(int /*NewSeed*/)
{
	//CRandomMWCScripts::SetRandomSeed(NewSeed);
}

void CommandSetTimeScale(float fNewTimeScale)
{
#if __BANK
	GtaThread* pThread = CTheScripts::GetCurrentGtaScriptThread();
	if(pThread)
	{
		scriptDisplayf("SET_TIME_SCALE(%f): Called from Script:- %s", fNewTimeScale, pThread->GetScriptName() );
	}
#endif	//__BANK

	// disable any active special abilities, someone them have timewarps of their own and this will mess with them
	//   CPed* pPlayer = CGameWorld::FindLocalPlayer();
	//   if (pPlayer && pPlayer->GetSpecialAbility())
	//{
	//       pPlayer->GetSpecialAbility()->Deactivate();
	//	
	//	//! Force special ability time scale to reset. This mimics the previous behaviour before we split
	//	//! into seperate timers.
	//	pPlayer->GetSpecialAbility()->SetTimeWarp(1.0f);
	//}

	fwTimer::SetTimeWarpScript(fNewTimeScale);
}

void CommandSetMissionFlag(bool MissionFlagValue)
{
	if (MissionFlagValue)
	{
		if(SCRIPT_VERIFY(CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript == false, "SET_MISSION_FLAG - Flag for this script is already TRUE"))
		{
			if(SCRIPT_VERIFY(CTheScripts::GetPlayerIsOnAMission() == false, "SET_MISSION_FLAG - Flag is already TRUE (probably set by another script)"))
			{
//				if(SCRIPT_VERIFY(CTheScripts::GetPlayerIsOnARandomEvent() == false, "SET_MISSION_FLAG - player is already on a random event so can't start a mission"))
				{
					CTheScripts::SetPlayerIsOnAMission(true);
					CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript = true;

					CDebug::SetCurrentMissionTag(CTheScripts::GetCurrentScriptName());

					Displayf("SET_MISSION_FLAG called by %s\n", CTheScripts::GetCurrentScriptName());

#if __BANK
					// Reset max ragdoll counters used for the level
					if (fragInstNMGta::ms_bLogUsedRagdolls)
					{
						fragInstNMGta::ms_MaxNMAgentsUsedCurrentLevel = 0;
						fragInstNMGta::ms_MaxRageRagdollsUsedCurrentLevel = 0;
						fragInstNMGta::ms_MaxNMAgentsUsedInComboCurrentLevel = 0;
						fragInstNMGta::ms_MaxRageRagdollsUsedInComboCurrentLevel = 0;
						fragInstNMGta::ms_MaxTotalRagdollsUsedCurrentLevel = 0;
						fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel = 0;
					}
#endif
				}
			}
		}
	}
	else
	{
		scriptAssertf(0, "%s:SET_MISSION_FLAG - should never need to call this with FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSetMissionNameDisplay(const char * DEV_ONLY(pMissionName))
{
#if __DEV
	// tell the debug system what the current mission is - used for bugstar (if active) and for the release info bar
	CDebug::SetCurrentMissionName(pMissionName);
#endif
}

void CommandInformCodeOfContentIDOfCurrentUGCMission(const char *pContentIdString)
{
	CTheScripts::SetContentIdOfCurrentUGCMission(pContentIdString);
}

bool CommandGetMissionFlag()
{
	return (CTheScripts::GetPlayerIsOnAMission());
}

void CommandSetRandomEventFlag(bool bThisScriptIsARandomEvent)
{
	if (bThisScriptIsARandomEvent)
	{
		if(SCRIPT_VERIFY(CTheScripts::GetCurrentGtaScriptThread()->GetIsARandomEventScript() == false, "SET_RANDOM_EVENT_FLAG - Flag for this script is already TRUE"))
		{
			if(SCRIPT_VERIFY(CTheScripts::GetPlayerIsOnARandomEvent() == false, "SET_RANDOM_EVENT_FLAG - Flag is already TRUE (probably set by another script)"))
			{
				if(SCRIPT_VERIFY(CTheScripts::GetPlayerIsOnAMission() == false, "SET_RANDOM_EVENT_FLAG - player is already on a mission so can't start a random event"))
				{
					CTheScripts::SetPlayerIsOnARandomEvent(true);
					CTheScripts::GetCurrentGtaScriptThread()->SetIsARandomEventScript(true);

					CDebug::SetCurrentMissionTag(CTheScripts::GetCurrentScriptName());

					Displayf("SET_RANDOM_EVENT_FLAG called by %s\n", CTheScripts::GetCurrentScriptName());
				}
			}
		}
	}
	else
	{
		scriptAssertf(0, "%s:SET_RANDOM_EVENT_FLAG - should never need to call this with FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

bool CommandGetRandomEventFlag()
{
	return (CTheScripts::GetPlayerIsOnARandomEvent());
}


bool CommandGetBaseElementLocationFromMetadata(int& outVLocation, int& outVRotation, int element, bool bHighEndApt)
{
	//request struct
	scriptDebugf3("GetBaseElementLocationFromMetadata %d %s", element, bHighEndApt? "high end apartment" : "");

	CBaseElementLocation* elementFound = NULL;
	if(!CScriptMetadataManager::GetBaseElementLocation(&elementFound, element, bHighEndApt))
	{
		Assertf(0, "Base Element Location for item %d %s cannot be found", element, bHighEndApt? "high end apartment" : "");
		return false;
	}

	//buffer to write back to
	u8* pOutLocationBuffer = reinterpret_cast<u8*>(&outVLocation);
	memcpy(pOutLocationBuffer, &elementFound->m_location.x, sizeof(float));
	pOutLocationBuffer += sizeof(scrValue);
	memcpy(pOutLocationBuffer, &elementFound->m_location.y, sizeof(float));
	pOutLocationBuffer += sizeof(scrValue);
	memcpy(pOutLocationBuffer, &elementFound->m_location.z, sizeof(float));

	u8* pOutRotationBuffer = reinterpret_cast<u8*>(&outVRotation);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.x, sizeof(float));
	pOutRotationBuffer += sizeof(scrValue);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.y, sizeof(float));
	pOutRotationBuffer += sizeof(scrValue);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.z, sizeof(float));

	return true;
}

bool CommandGetBaseElementLocationFromMetadataBlock(int& outVLocation, int& outVRotation, int element, int dataBlock)
{
	//request struct
	scriptDebugf3("GetBaseElementLocationFromMetadata %d data block %d", element, dataBlock);

	CBaseElementLocation* elementFound = NULL;
	if(!CScriptMetadataManager::GetBaseElementLocationFromBlock(&elementFound, element, dataBlock))
	{
		Assertf(0, "Base Element Location for item %d data block %d cannot be found", element, dataBlock);
		return false;
	}

	//buffer to write back to
	u8* pOutLocationBuffer = reinterpret_cast<u8*>(&outVLocation);
	memcpy(pOutLocationBuffer, &elementFound->m_location.x, sizeof(float));
	pOutLocationBuffer += sizeof(scrValue);
	memcpy(pOutLocationBuffer, &elementFound->m_location.y, sizeof(float));
	pOutLocationBuffer += sizeof(scrValue);
	memcpy(pOutLocationBuffer, &elementFound->m_location.z, sizeof(float));

	u8* pOutRotationBuffer = reinterpret_cast<u8*>(&outVRotation);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.x, sizeof(float));
	pOutRotationBuffer += sizeof(scrValue);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.y, sizeof(float));
	pOutRotationBuffer += sizeof(scrValue);
	memcpy(pOutRotationBuffer, &elementFound->m_rotation.z, sizeof(float));

	return true;
}

const char* CommandGetContentToLoad()
{
#if RSG_SCE
	static char sysServiceArg[255] = {0};

	const char* paramArg = NULL;
	if(PARAM_LiveAreaLoadContent.Get(paramArg))
	{
		return paramArg;
	}
	else if(g_SysService.Args().GetParamValue("-LiveAreaLoadContent", sysServiceArg))
	{
		return sysServiceArg;
	}
	else
#endif // RSG_ORBIS
	{
		return "";
	}
}

void CommandActivityFeedCreate(const char* captionString, const char* condensedCaptionString)
{
#if RSG_ORBIS
	int key = atStringHash(condensedCaptionString);
	g_rlNp.GetNpActivityFeed().Start(key);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char caption[64] = {0};
		formatf( caption, sizeof(caption),"%s_%c%c", captionString, std::toupper(iso[0]), std::toupper(iso[1]));

		char condensed[64] = {0};
		formatf( condensed, sizeof(condensed),"%s_%c%c", condensedCaptionString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostCaptions((sysLanguage)language, TheText.Get(caption), TheText.Get(condensed));
	}

	//g_rlNp.GetNpActivityFeed().PostThumbnailImageURL("https://media.rockstargames.com/socialclub/activity/ps4/V/gtav_68x68.png"); // title image is the same for all posts
#else
	UNUSED_VAR(captionString);
	UNUSED_VAR(condensedCaptionString);
#endif
}

void CommandActivityFeedAddSubStringToCaption(const char* subString)
{
#if RSG_ORBIS
	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char subStringWithIso[64] = {0};
		formatf( subStringWithIso, sizeof(subStringWithIso),"%s_%c%c", subString, std::toupper(iso[0]), std::toupper(iso[1]));

		scriptAssertf(TheText.DoesTextLabelExist(subStringWithIso), "Activity Feed AddSubstring - String has not been translated. Probably not been added to activityfeed gametext - %s", subStringWithIso);

		const char* stringToPrint = TheText.Get(subStringWithIso);
		g_rlNp.GetNpActivityFeed().AddSubStringToCaption((sysLanguage)language, stringToPrint);
	}

	g_rlNp.GetNpActivityFeed().ConfirmCaptionSubStringComplete();
#else
	UNUSED_VAR(subString);
#endif
}

void CommandActivityFeedAddLiteralSubStringToCaption(const char* subString)
{
#if RSG_ORBIS
	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		g_rlNp.GetNpActivityFeed().AddSubStringToCaption((sysLanguage)language, subString);
	}

	g_rlNp.GetNpActivityFeed().ConfirmCaptionSubStringComplete();
#else
	UNUSED_VAR(subString);
#endif
}

void CommandActivityFeedAddFloatToCaption(float subValue, int decimalPoint)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().AddSubValueToCaptionFloat(subValue, decimalPoint);
#else
	UNUSED_VAR(subValue);
	UNUSED_VAR(decimalPoint);
#endif
}

void CommandActivityFeedAddIntToCaption(int subValue)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().AddSubValueToCaptionInt(subValue);
#else
	UNUSED_VAR(subValue);
#endif
}

void CommandActivityFeedSmallImageURL(const char* urlString, const char* aspectRatio)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostSmallImageURL(urlString, aspectRatio);
#else
UNUSED_VAR(urlString);
UNUSED_VAR(aspectRatio);
#endif
}

void CommandActivityFeedLargeImageURL(const char* urlString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostLargeImageURL(urlString);
#else
UNUSED_VAR(urlString);
#endif
}

void CommandActivityFeedVideoURL(const char* urlString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostVideoURL(urlString);
#else
UNUSED_VAR(urlString);
#endif
}

void CommandActivityFeedPostCustomStory(const char* caption)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostCustomCaption(caption);
#else
	UNUSED_VAR(caption);
#endif
}

void CommandActivityFeedActionURL(const char* urlString, const char* labelString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionURL(urlString);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));
	
		g_rlNp.GetNpActivityFeed().PostTagForActionURL((sysLanguage)language, TheText.Get(label));
	}
#else
UNUSED_VAR(urlString);
UNUSED_VAR(labelString);
#endif
}

void CommandActivityFeedActionURLWithThumbnail(const char* urlString, const char* labelString, const char* thumbnail)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionURL(urlString);
	g_rlNp.GetNpActivityFeed().PostThumbnailForActionURL(thumbnail);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionURL((sysLanguage)language, TheText.Get(label));
	}
#else
	UNUSED_VAR(urlString);
	UNUSED_VAR(labelString);
	UNUSED_VAR(thumbnail);
#endif
}

void CommandActivityFeedActionURLAdd(const char* urlString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().AppendActionURL(urlString);
#else
	UNUSED_VAR(urlString);
#endif
}

void CommandActivityFeedActionStart(const char* commandLineString, const char* labelString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionStart(commandLineString);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionStart((sysLanguage)language, TheText.Get(label));
	}
#else
UNUSED_VAR(commandLineString);
UNUSED_VAR(labelString);
#endif
}

void CommandActivityFeedActionStartWithThumbnail(const char* commandLineString, const char* labelString, const char* thumbnail)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionStart(commandLineString);
	g_rlNp.GetNpActivityFeed().PostThumbnailForActionStart(thumbnail);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionStart((sysLanguage)language, TheText.Get(label));
	}
#else
	UNUSED_VAR(commandLineString);
	UNUSED_VAR(labelString);
	UNUSED_VAR(thumbnail);
#endif
}

void CommandActivityFeedActionStartAdd(const char* commandLineString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().AppendActionStart(commandLineString);
#else
	UNUSED_VAR(commandLineString);
#endif
}

void CommandActivityFeedActionStore(const char* productCodeString, const char* labelString)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionStore(productCodeString);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionStore((sysLanguage)language, TheText.Get(label));
	}
#else
UNUSED_VAR(productCodeString);
UNUSED_VAR(labelString);
#endif
}

void CommandActivityFeedActionStoreWithThumbnail(const char* productCodeString, const char* labelString, const char* thumbnail)
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostActionStore(productCodeString);
	g_rlNp.GetNpActivityFeed().PostThumbnailForActionStore(thumbnail);

	for (int language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( (sysLanguage)language );

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionStore((sysLanguage)language, TheText.Get(label));
	}
#else
	UNUSED_VAR(productCodeString);
	UNUSED_VAR(labelString);
	UNUSED_VAR(thumbnail);
#endif
}

void CommandActivityFeedPost()
{
#if RSG_ORBIS
	g_rlNp.GetNpActivityFeed().PostCurrentMessage();
#endif // RSG_ORBIS
}

void CommandActivityFeedOnlinePlayedWithPost(const char* gameMode)
{
#if RSG_ORBIS
	// weirdly, can only post in one language, unlike the other activity feed post
	g_rlNp.GetNpActivityFeed().StartOnlinePlayedWith(TheText.Get(gameMode));

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(u8 index = 0; index < numRemotePhysicalPlayers; ++index)
	{
		const netPlayer *remotePlayer = remotePhysicalPlayers[index];
		if (remotePlayer)
		{
			PhysicalPlayerIndex playerIndex = remotePlayer->GetPhysicalPlayerIndex();

			if(playerIndex != INVALID_PLAYER_INDEX)
			{
				g_rlNp.GetNpActivityFeed().AddOnlinePlayedWith( remotePlayer->GetGamerInfo().GetGamerHandle() );
			}
		}
	}

 	g_rlNp.GetNpActivityFeed().PostOnlinePlayedWith();

#else
	UNUSED_VAR(gameMode);
#endif // RSG_ORBIS
}

bool CommandHasResumedFromSuspend()
{
	return g_SysService.HasResumedFromSuspend();
}

void CommandSetScriptHighPrio(bool highPrio)
{
	CTheScripts::GetCurrentGtaScriptThread()->m_bIsHighPrio = highPrio;
	Displayf("SET_SCRIPT_HIGH_PRIO (%s) called by %s\n", highPrio ? "true" : "false", CTheScripts::GetCurrentScriptName());
}

void CommandSetThisIsATriggerScript(bool bThisScriptIsATriggerScript)
{
	CTheScripts::GetCurrentGtaScriptThread()->SetIsATriggerScript(bThisScriptIsATriggerScript);

	scriptDisplayf("SET_THIS_IS_A_TRIGGER_SCRIPT(%s) called by %s", bThisScriptIsATriggerScript?"true":"false", CTheScripts::GetCurrentScriptName());
}

u32 CommandGetPrevWeatherTypeHashName()
{
	u32 prevTypeIndex = g_weather.GetPrevTypeIndex();
	u32 prevTypeHashName = g_weather.GetTypeHashName(prevTypeIndex);
	return (*(reinterpret_cast<s32*>(&prevTypeHashName)));
}

u32 CommandGetNextWeatherTypeHashName()
{
	u32 nextTypeIndex = g_weather.GetNextTypeIndex();
	u32 nextTypeHashName = g_weather.GetTypeHashName(nextTypeIndex);
	return (*(reinterpret_cast<s32*>(&nextTypeHashName)));
}

bool CommandIsPrevWeatherType(const char* typeName)
{
	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		return typeIndex==(s32)g_weather.GetPrevTypeIndex();
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in IS_PREV_WEATHER_TYPE", typeName);
		return false;
	}
}

bool CommandIsNextWeatherType(const char* typeName)
{
	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		return typeIndex==(s32)g_weather.GetNextTypeIndex();
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in IS_NEXT_WEATHER_TYPE", typeName);
		return false;
	}
}

void CommandSetWeatherTypePersist(const char* typeName)
{
	// multiplayer has a global time
	if(!scriptVerifyf(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather(), "SET_WEATHER_TYPE_PERSIST :: Not supported in network games! Use SET_OVERRIDE_WEATHER / SET_WEATHER_TYPE_NOW_PERSIST"))
		return; 

	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		g_weather.ForceType(typeIndex);
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in SET_WEATHER_TYPE_PERSIST", typeName);
	}
}

void CommandSetWeatherTypeNowPersist(const char* typeName)
{
	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		g_weather.ForceTypeNow(typeIndex, CWeather::FT_None);
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in SET_WEATHER_TYPE_NOW_PERSIST", typeName);
	}
}

void CommandSetWeatherTypeNow(const char* typeName)
{
	// multiplayer has a global time
	if(!scriptVerifyf(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather(), "SET_WEATHER_TYPE_NOW :: Not supported in network games! Use SET_OVERRIDE_WEATHER / SET_WEATHER_TYPE_NOW_PERSIST"))
		return; 

	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		g_weather.SetTypeNow(typeIndex);
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in SET_WEATHER_TYPE_NOW", typeName);
	}
}

void CommandSetWeatherTypeOvertimePersist(const char* typeName, float time)
{
	scriptDebugf1("%s : SET_WEATHER_TYPE_OVERTIME_PERSIST called. Weather Type :\"%s\", Time: %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), typeName, time);

	s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (typeIndex>=0 && typeIndex<(s32)g_weather.GetNumTypes())
	{
		g_weather.ForceWeatherTypeOverTime(typeIndex,time);
	}
	else
	{
		scriptAssertf(0, "Invalid Weather Type (%s) found in SET_WEATHER_TYPE_OVERTIME_PERSIST", typeName);
	}
}

void CommandSetRandomWeatherType()
{
    // multiplayer has a global time 
    if(!scriptVerifyf(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather(), "SET_RANDOM_WEATHER_TYPE :: Not supported in network games! Use SET_OVERRIDE_WEATHER."))
        return; 

	g_weather.RandomiseType();
}

void CommandClearWeatherTypePersist()
{
	g_weather.ForceTypeClear();
}

void CommandGetWeatherState(s32& prevTypeHashName, s32& nextTypeHashName, float& interpVal)
{
	u32 prevTypeIndex = g_weather.GetPrevTypeIndex();
	u32 prevTypeHashNameTemp = g_weather.GetTypeHashName(prevTypeIndex);
	prevTypeHashName = (*(reinterpret_cast<s32*>(&prevTypeHashNameTemp)));

	u32 nextTypeIndex = g_weather.GetNextTypeIndex();
	u32 nextTypeHashNameTemp = g_weather.GetTypeHashName(nextTypeIndex);
	nextTypeHashName = (*(reinterpret_cast<s32*>(&nextTypeHashNameTemp)));
	
	interpVal = g_weather.GetInterp();
}

void CommandSetWeatherState(s32 prevTypeHashName, s32 nextTypeHashName, float interpVal)
{
    // multiplayer has a global time 
    if(!scriptVerifyf(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather(), "SET_CURR_WEATHER_STATE :: Not supported in network games! Use SET_OVERRIDE_WEATHER."))
        return; 

	const s32 prevTypeIndex = g_weather.GetTypeIndex((u32)prevTypeHashName);
	g_weather.SetPrevTypeIndex(prevTypeIndex);

	const s32 nextTypeIndex = g_weather.GetTypeIndex((u32)nextTypeHashName);
	g_weather.SetNextTypeIndex(nextTypeIndex);

	g_weather.SetInterp(interpVal);
}

void CommandSetOverrideWeatherEx(const char* typeName, bool resetWetness)
{
	const s32 typeIndex = g_weather.GetTypeIndex(typeName);

	if (0 <= typeIndex && typeIndex < g_weather.GetNumTypes())
	{
		scriptDebugf1("%s : SET_OVERRIDE_WEATHER called. Weather Type :\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), typeName);
		g_weather.OverrideType(typeIndex, resetWetness);
	}
	else
	{
		scriptAssertf(0, "%s : SET_OVERRIDE_WEATHER - Invalid Weather Type :\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), typeName);
	}
}

void CommandSetOverrideWeather(const char* typeName)
{
	CommandSetOverrideWeatherEx(typeName, false);
}

void CommandClearOverrideWeather()
{
	scriptDebugf1("%s : CLEAR_OVERRIDE_WEATHER called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	g_weather.StopOverriding();
}

void CommandSetOverrideShoreWaveAmplitude(float value)
{
	g_weather.SetOverrideShoreWaveAmplitude(value);
}

void CommandSetOverrideShoreWaveMinAmplitude(float value)
{
	g_weather.SetOverrideShoreWaveMinAmplitude(value);
}
	
void CommandSetOverrideShoreWaveMaxAmplitude(float value)
{
	g_weather.SetOverrideShoreWaveMaxAmplitude(value);
}

void CommandSetOverrideOceanNoiseMinAmplitude(float value)
{
	g_weather.SetOverrideOceanNoiseMinAmplitude(value);
}

void CommandSetOverrideOceanWaveAmplitude(float value)
{
	g_weather.SetOverrideOceanWaveAmplitude(value);
}

void CommandSetOverrideOceanWaveMinAmplitude(float value)
{
	g_weather.SetOverrideOceanWaveMinAmplitude(value);
}

void CommandSetOverrideOceanWaveMaxAmplitude(float value)
{
	g_weather.SetOverrideOceanWaveMaxAmplitude(value);
}
		
void CommandSetOverrideRippleBumpiness(float value)
{
	g_weather.SetOverrideRippleBumpiness(value);
}

void CommandSetOverrideRippleMinBumpiness(float value)
{
	g_weather.SetOverrideRippleMinBumpiness(value);
}
			
void CommandSetOverrideRippleMaxBumpiness(float value)
{
	g_weather.SetOverrideRippleMaxBumpiness(value);
}

void CommandSetOverrideRippleDisturb(float value)
{
	g_weather.SetOverrideRippleDisturb(value);
}

void CommandSetWaterOverrideStrength(float value)
{
	g_weather.SetWaterOverrideStrength(value);
}

void CommandFadeInWaterOverrideOverTime(float time)
{
	g_weather.FadeInWaterOverrideOverTime(time);
}

void CommandFadeOutWaterOverrideOverTime(float time)
{
	g_weather.FadeOutWaterOverrideOverTime(time);
}

void CommandClearWeatherTypeNowPersistNetwork(int transitionTime)
{
	if(CNetwork::IsApplyingMultiplayerGlobalClockAndWeather() && transitionTime > 0)
		CNetwork::StartWeatherTransitionToGlobal(transitionTime); 
	else
		g_weather.ForceTypeNow(-1, CWeather::FT_None);
}

void CommandSetWind(float wind)
{
	g_weather.SetWindOverride(wind);
}

void CommandSetWindSpeed(float windSpeed)
{
	float windSpeedMax = g_weather.GetWindSpeedMax();
	scriptAssertf(windSpeed<windSpeedMax, "trying to set a wind speed greater than the maximum allowed (%.2f) - clamping to max", windSpeedMax);
	g_weather.SetWindOverride(MIN(windSpeed/windSpeedMax, 1.0f));
}

float CommandGetWindSpeed()
{
	return g_weather.GetWindSpeed();
}

float CommandGetWindSpeedMax()
{
	return g_weather.GetWindSpeedMax();
}

void CommandSetWindDirection(float windDir)
{
	g_weather.SetWindDirectionOverride(windDir);
}

scrVector CommandGetWindDirection()
{
	return VEC3V_TO_VECTOR3(g_weather.GetWindDirection());
}

void CommandSetRain(float rain)
{
	g_weather.SetRainOverride(rain);
}

float CommandGetRainLevel()
{
	return g_weather.GetRain();
}

void CommandSetSnow(float snow)
{
	g_weather.SetSnowOverride(snow);
}

float CommandGetSnowLevel()
{
	return g_weather.GetSnow();
}

void CommandForceLightningFlash()
{
	g_weather.ForceLightningFlash();
}

int CommandAddWindThermal(const scrVector & scrVecCoors, float radius, float height, float maxStrength, float deltaStrength)
{
	return g_weather.AddWindThermal(VECTOR3_TO_VEC3V((Vector3)scrVecCoors), radius, height, maxStrength, deltaStrength);
}

void CommandRemoveWindThermal(int index)
{
	g_weather.RemoveWindThermal(index);
}

void CommandCloudSettingsOverride(const char *pSettingsName)
{
	if (pSettingsName && (!stricmp(pSettingsName,"NULL")||!stricmp(pSettingsName,"NONE")))
		CClouds::OverrideCloudSettingName(NULL);
	else
		CClouds::OverrideCloudSettingName(pSettingsName);
}

void CommandPreloadCloudHat(const char *pCloudHatName)
{
	CloudHatManager::GetInstance()->PreloadCloudHatByName(pCloudHatName);
}

void CommandReleasePreloadCloudHat(const char *pCloudHatName)
{
	CloudHatManager::GetInstance()->ReleasePreloadCloudHatByName(pCloudHatName);
}

void CommandLoadCloudHat(const char *pCloudHatName, float transitionTime=0.0f)
{
	CloudHatManager::GetInstance()->LoadCloudHatByName(pCloudHatName, transitionTime);
}

void CommandUnloadCloudHat(const char *pCloudHatName, float transitionTime=0.0f)
{
	CloudHatManager::GetInstance()->UnloadCloudHatByName(pCloudHatName, transitionTime);
}

void CommandUnloadAllCloudHats()
{
	CloudHatManager::GetInstance()->UnloadAllCloudHats();
}

void CommandSetCloudHatAlpha(float alpha)
{
	scriptAssertf(alpha >= 0.0f && alpha <= 1.0f, "%s:SET_CLOUDS_ALPHA - Alpha of %f is out of range", CTheScripts::GetCurrentScriptNameAndProgramCounter(), alpha);
	CClouds::SetCloudAlpha(alpha);
}

void CommandResetCloudHatAlpha()
{
	CClouds::SetCloudAlpha(1.0f);
}

float CommandGetCloudHatAlpha()
{
	return CClouds::GetCloudAlpha();
}

int CommandGetGameTimer()
{
	return ((int) fwTimer::GetTimeInMilliseconds());
}

int CommandGetNumberOfMicrosecondsSinceLastCall()
{
	static utimer_t lastTick = 0;

	utimer_t thisTick = sysTimer::GetTicks();
	utimer_t diffTick = thisTick - lastTick;
	float fDiffTick = static_cast<float>(diffTick);
	fDiffTick *= sysTimer::GetTicksToMicroseconds();
	s32 ReturnTick = static_cast<s32>(fDiffTick);

	lastTick = thisTick;

	return ReturnTick;
}

float CommandGetScriptTimeWithinFrameInMicroseconds()
{
	return CTheScripts::GetScriptTimer().GetUsTime();
}

void CommandResetScriptTimeWithinFrame()
{
	CTheScripts::GetScriptTimer().Reset();
}

float CommandGetFrameTime()
{
	return fwTimer::GetTimeStep();
}

float CommandGetSystemTimeStep()
{
	return fwTimer::GetSystemTimeStep();
}

int CommandGetFrameCount()
{
	return ((int) fwTimer::GetFrameCount());
}

#if GEN9_STANDALONE_ENABLED
bool CommandGetScriptRouterContextData(ScriptRouterContextData * contextData)
{
	if (SCRIPT_VERIFY(contextData, "GET_SCRIPT_ROUTER_CONTEXT_DATA: Failed. contextData is null"))
	{
		if (SCRIPT_VERIFY(ScriptRouterLink::HasPendingScriptRouterLink(), "GET_SCRIPT_ROUTER_CONTEXT_DATA: Attempting to get empty script router context"))
		{
			return ScriptRouterLink::ParseScriptRouterLink(*contextData);
		}
	}
	return false;
}
#else // GEN9_STANDALONE_ENABLED
bool CommandGetScriptRouterContextData(DummyScriptRouterContextData* UNUSED_PARAM(contextData))
{
	return false;
}
#endif // GEN9_STANDALONE_ENABLED

#if GEN9_STANDALONE_ENABLED
bool CommandSetScriptRouterLink(ScriptRouterContextData* contextData)
{
	if (SCRIPT_VERIFY(contextData, "CREATE_SCRIPT_ROUTER_LINK: Failed. contextData is null"))
	{
		return ScriptRouterLink::SetScriptRouterLink(*contextData);
	}
	return false;
}
#else // GEN9_STANDALONE_ENABLED
bool CommandSetScriptRouterLink(DummyScriptRouterContextData* UNUSED_PARAM(contextData))
{
	return false;
}
#endif // GEN9_STANDALONE_ENABLED

int CommandGetScriptRouterLinkHash()
{
#if GEN9_STANDALONE_ENABLED
	return ScriptRouterLink::GetScriptRouterLinkHash();
#else // GEN9_STANDALONE_ENABLED
	return 0;
#endif // GEN9_STANDALONE_ENABLED
}

bool CommandHasPendingScriptRouterLink()
{
#if GEN9_STANDALONE_ENABLED
	return ScriptRouterLink::HasPendingScriptRouterLink();
#else // GEN9_STANDALONE_ENABLED
	return false;
#endif // GEN9_STANDALONE_ENABLED
}

void CommandClearScriptRouterLink()
{
#if GEN9_STANDALONE_ENABLED
	Displayf("CLEAR_SCRIPT_ROUTER_LINK");
	ScriptRouterLink::ClearScriptRouterLink();
#endif // GEN9_STANDALONE_ENABLED
}

void CommandReportInvalidScriptRouterArgument(const char* argument)
{
#if GEN9_STANDALONE_ENABLED && RSG_OUTPUT
	Errorf("REPORT_INVALID_SCRIPT_ROUTER_ARGUMENT: %s", argument);
#else
	UNUSED_VAR(argument);
#endif // GEN9_STANDALONE_ENABLED
}

#if GEN9_STANDALONE_ENABLED && RSG_PROSPERO
void CommandSetActivityScriptRoutingEnabled(bool bEnabled)
{
	Displayf("SET_ACTIVITY_SCRIPT_ROUTING_ENABLED(%s)", bEnabled ? "TRUE" : "FALSE");
	ScriptRouterContext::SetActivityScriptRoutingEnabled(bEnabled);
}
#else // GEN9_STANDALONE_ENABLED && RSG_PROSPERO 
void CommandSetActivityScriptRoutingEnabled(bool UNUSED_PARAM(bEnabled))
{

}
#endif // GEN9_STANDALONE_ENABLED && RSG_PROSPERO 

void CommandSetFakeWantedLevel(int iWantedLevel)
{
	if(CScriptHud::iFakeWantedLevel != iWantedLevel)
	{
		Displayf("SET_FAKE_WANTED_LEVEL. :%d\n", iWantedLevel);
		CScriptHud::iFakeWantedLevel = iWantedLevel;
	}

	CScriptHud::vFakeWantedLevelPos = CGameWorld::FindLocalPlayerCentreOfWorld();  // store current player pos

	CGameWorld::FindLocalPlayerWanted()->m_LastTimeWantedLevelChanged = fwTimer::GetTimeInMilliseconds();
}


int CommandGetFakeWantedLevel()
{
	return CScriptHud::iFakeWantedLevel;
}


float CommandGenerateRandomFloatInRange(float MinFloat, float MaxFloat)
{
	return ( CRandomScripts::GetRandomNumberInRange( MinFloat, MaxFloat ) );
}

int CommandGenerateRandomIntInRange(int MinInt, int MaxInt)
{
	return ( CRandomScripts::GetRandomNumberInRange(MinInt, MaxInt) );
}

int CommandGenerateRandomMWCIntInRange(int MinInt, int MaxInt)
{
	bool revertBehaviour = false;
	revertBehaviour = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("REVERT_MWC_TO_OLD", 0xc5debb40), false);

	if (revertBehaviour)
	{
		return ( CRandomScripts::GetRandomNumberInRange(MinInt, MaxInt) );
	} else
	{
		return ( CRandomMWCScripts::GetRandomNumberInRange(MinInt, MaxInt) );
	}
}

bool CommandGetGroundZFor3dCoord(const scrVector & scrVecCoors, float& ReturnZ, bool waterAsGround = false, bool ignoreDistToWaterLevelCheck = false)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	bool FoundGround = false;

	if(SCRIPT_VERIFY(VecPos.FiniteElements(), "GET_GROUND_Z_FOR_3D_COORD - 3D coords were bad (not finite)."))
	{
		ReturnZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, VecPos.x, VecPos.y, VecPos.z, &FoundGround);
	}

	scriptDebugf1("CommandGetGroundZFor3dCoord: %f %f %f   ReturnZ: %f  Found ground: %s ", VecPos.x, VecPos.y, VecPos.z, ReturnZ, FoundGround ? "TRUE": "FALSE");

	if(waterAsGround)
	{
		float waterZ = 0.0f;
		bool bFoundWater = false;

		if (ignoreDistToWaterLevelCheck)
		{
			bFoundWater = CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VecPos, &waterZ, false, FLT_MAX);
		}
		else
		{
			bFoundWater = CVfxHelper::GetWaterZ(VECTOR3_TO_VEC3V(VecPos), waterZ)!=WATERTEST_TYPE_NONE;
		}

		if (bFoundWater)
		{
			// If no ground was found but there's water, we're too far off shore
			if(!FoundGround)
			{
				ReturnZ = waterZ;
				FoundGround = true;
			}
			// If the water level is above the ground
			if(FoundGround && waterZ>ReturnZ)
			{
				ReturnZ = waterZ;
			}
		}

		scriptDebugf1("CommandGetGroundZFor3dCoord: IncludeWater: TRUE  Found water: %s   Water Z: %f ", bFoundWater ? "TRUE" : "FALSE", waterZ);
	}

	return (FoundGround);
}

bool CommandGetGroundZAndNormalFor3dCoord(const scrVector & scrVecCoors, float& ReturnZ, Vector3& ReturnNormal)
{
	Vector3 VecPos = Vector3(scrVecCoors);
	Vector3 VecNormal;
	bool bFoundGround = false;

	if(SCRIPT_VERIFY(VecPos.FiniteElements(), "GET_GROUND_Z_AND_NORMAL_FOR_3D_COORD - 3D coords were bad (not finite)."))
	{
	ReturnZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, VecPos.x, VecPos.y, VecPos.z, &bFoundGround, &VecNormal);

	ReturnNormal.x = VecNormal.x;
	ReturnNormal.y = VecNormal.y;
	ReturnNormal.z = VecNormal.z;
	}

	scriptDebugf1("CommandGetGroundZAndNormalFor3dCoord:  Pos: %f %f %f  Found ground: %s  VecNormal: %f %f %f  ReturnZ: %f ", VecPos.x, VecPos.y, VecPos.z, bFoundGround ? "TRUE" : "FALSE", VecNormal.x, VecNormal.y, VecNormal.z, ReturnZ);

	return bFoundGround;
}

bool CommandGetGroundZExcludeObjectsFor3dCoord(const scrVector & scrVecCoors, float& ReturnZ, bool includeWater = false, bool ignoreDistToWaterLevelCheck = false)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	bool FoundGround = false;

	if(SCRIPT_VERIFY(VecPos.FiniteElements(), "GET_GROUND_Z_EXCLUDING_OBJECTS_FOR_3D_COORD - 3D coords were bad (not finite)."))
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResult(probeIsect);
		probeDesc.SetStartAndEnd(VecPos, Vector3(scrVecCoors.x, scrVecCoors.y, -1000.0f));
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		probeDesc.SetExcludeTypeFlags( ArchetypeFlags::GTA_OBJECT_TYPE );
		probeDesc.SetContext(WorldProbe::LOS_Unspecified);

		FoundGround = WorldProbe::GetShapeTestManager()->SubmitTest( probeDesc );

		if( FoundGround )
		{
			ReturnZ = probeResult[0].GetHitPosition().z;
		}
	}

	scriptDebugf1("CommandGetGroundZExcludeObjectsFor3dCoord:  Pos: %f %f %f  Found ground: %s  ReturnZ: %f ", VecPos.x, VecPos.y, VecPos.z, FoundGround ? "TRUE" : "FALSE", ReturnZ);

	if(includeWater)
	{
		float waterZ = 0.0f;
		bool bFoundWater = false;

		if (ignoreDistToWaterLevelCheck)
		{
			bFoundWater = CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VecPos, &waterZ, false, FLT_MAX);
		}
		else
		{
			bFoundWater = CVfxHelper::GetWaterZ(VECTOR3_TO_VEC3V(VecPos), waterZ)!=WATERTEST_TYPE_NONE;
		}

		if(bFoundWater)
		{
			// If no ground was found but there's water, we're too far off shore
			if(!FoundGround)
			{
				ReturnZ = waterZ;
				FoundGround = true;
			}
			// If the water level is above the ground
			if(FoundGround && waterZ>ReturnZ)
			{
				ReturnZ = waterZ;
			}
		}

		scriptDebugf1("CommandGetGroundZExcludeObjectsFor3dCoord:  IncludeWater: TRUE   Found water: %s   Water Z: %f", bFoundWater ? "TRUE" : "FALSE", waterZ );
	}

	return (FoundGround);
}

bool CommandIsBulletInAngledArea(const scrVector & scrVecCoors1, const scrVector & scrVecCoors2, float AreaWidth, bool bIsPlayer)
{
	Vector3 pos1 = Vector3(scrVecCoors1);
	Vector3 pos2 = Vector3(scrVecCoors2);
	
	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "IS_BULLET_IN_AREA - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "IS_BULLET_IN_AREA - Can't Get Player Info"))
			return false;
	}

	bool rv = CBulletManager::ComputeIsBulletInAngledArea(pos1, pos2, AreaWidth, pPlayer);
	return rv;
}

bool CommandIsBulletInArea(const scrVector & scrVecCoors, float Radius, bool bIsPlayer)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	
	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "IS_BULLET_IN_AREA - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "IS_BULLET_IN_AREA - Can't Get Player Info"))
			return false;
	}

	bool rv = CBulletManager::ComputeIsBulletInSphere(VecPos, Radius, pPlayer);
	return rv;
}

bool CommandHasBulletImpactedInArea(const scrVector & scrVecCoors, float Radius, bool bIsPlayer, bool bEntryOnly)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "HAS_BULLET_IMPACTED_IN_AREA - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "HAS_BULLET_IMPACTED_IN_AREA - Can't Get Player Info"))
			return false;
	}

	bool rv = CBulletManager::ComputeHasBulletImpactedInSphere(VecPos, Radius, pPlayer, bEntryOnly);
	return rv;
}

int CommandGetNumBulletsImpactedInArea(const scrVector & scrVecCoors, float Radius, bool bIsPlayer, bool bEntryOnly)
{
	Vector3 VecPos = Vector3 (scrVecCoors);

	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "GET_NUM_BULLETS_IMPACTED_IN_AREA - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "GET_NUM_BULLETS_IMPACTED_IN_AREA - Can't Get Player Info"))
			return false;
	}

	int rv = CBulletManager::ComputeNumBulletsImpactedInSphere(VecPos, Radius, pPlayer, bEntryOnly);
	return rv;
}

// Arranges the values of the two vectors so that the volume of the locate box is calculated correctly
void RefactorValuesForAreaCheck (Vector3 &VecMin, Vector3 &VecMax)
{
	float temp_float;

	if (VecMin.x > VecMax.x)
	{
		temp_float = VecMin.x;
		VecMin.x = VecMax.x;
		VecMax.x = temp_float;
	}

	if (VecMin.y > VecMax.y)
	{
		temp_float = VecMin.y;
		VecMin.y = VecMax.y;
		VecMax.y = temp_float;
	}

	if (VecMin.z > VecMax.z)
	{
		temp_float = VecMin.z;
		VecMin.z = VecMax.z;
		VecMax.z = temp_float;
	}
}

bool CommandIsBulletInBox(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bIsPlayer)
{
	Vector3 VecPosMin = Vector3(scrVecMinCoors);
	Vector3 VecPosMax = Vector3(scrVecMaxCoors);
	
	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "IS_BULLET_IN_BOX - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "IS_BULLET_IN_BOX - Can't Get Player Info"))
			return false;
	}

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	bool rv = CBulletManager::ComputeIsBulletInBox(VecPosMin, VecPosMax, pPlayer);
	return rv;
}

bool CommandHasBulletImpactedInBox(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bIsPlayer, bool bEntryOnly)
{
	Vector3 VecPosMin = Vector3(scrVecMinCoors);
	Vector3 VecPosMax = Vector3(scrVecMaxCoors);
	
	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "HAS_BULLET_IMPACTED_IN_BOX - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "HAS_BULLET_IMPACTED_IN_BOX - Can't Get Player Info"))
			return false;
	}

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	bool rv = CBulletManager::ComputeHasBulletImpactedInBox(VecPosMin, VecPosMax, pPlayer, bEntryOnly);
	return rv;
}

bool CommandHasBulletImpactedBehindPlane(const scrVector & scrVecPlanePoint, const scrVector & scrVecPlaneNormal, bool bEntryOnly )
{
	Vector3 vPlanePoint = Vector3(scrVecPlanePoint);
	Vector3 vPlaneNormal = Vector3(scrVecPlaneNormal);
	bool rv = CBulletManager::ComputeHasBulletImpactedBehindPlane( vPlanePoint, vPlaneNormal, bEntryOnly );
	return rv;
}

int CommandGetNumBulletsImpactedInBox(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bIsPlayer, bool bEntryOnly)
{
	Vector3 VecPosMin = Vector3(scrVecMinCoors);
	Vector3 VecPosMax = Vector3(scrVecMaxCoors);

	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "GET_NUM_BULLETS_IMPACTED_IN_BOX - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "GET_NUM_BULLETS_IMPACTED_IN_BOX - Can't Get Player Info"))
			return false;
	}

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	int rv = CBulletManager::ComputeNumBulletsImpactedInBox(VecPosMin, VecPosMax, pPlayer, bEntryOnly);
	return rv;
}

float CommandASin( float x )
{
	if(SCRIPT_VERIFY( x >= -1.0f && x <= 1.0f, "X for ASIN out of range!" ))
	{
		return rage::Asinf( x ) * RtoD;
	}
	return 0.0f;
}

float CommandACos( float x )
{
	if(SCRIPT_VERIFY( x >= -1.0f && x <= 1.0f, "X for ACOS out of range!" ))
	{
		return rage::Acosf( x ) * RtoD;
	}
	return 0.0f;
}

float CommandTan( float x )
{
	return rage::Tanf( x * DtoR );
}

float CommandATan( float x )
{
	return rage::Atan2f( x,1 ) * RtoD;
}

float CommandATan2( float y, float x )
{
	return rage::Atan2f( y, x ) * RtoD;
}

bool ComamndLineIntersectsPlane(const scrVector & vscrPntA, const scrVector & vscrPntB, const scrVector & vscrPntOnPlane, const scrVector & vscrNormal, float &fTValue)
{
	//Convert to friendlier vector form
	Vector3 origin(vscrPntA.x, vscrPntA.y, vscrPntA.z);
	Vector3 ray(vscrPntB.x-vscrPntA.x, vscrPntB.y-vscrPntA.y, vscrPntB.z-vscrPntA.z);
	Vector3 vNormal(vscrNormal.x, vscrNormal.y, vscrNormal.z);
	Vector3 vPntOnPlane(vscrPntOnPlane.x, vscrPntOnPlane.y, vscrPntOnPlane.z);

	//Create plane equation
	Vector4 plane(vscrNormal.x, vscrNormal.y, vscrNormal.z, vNormal.Dot(vPntOnPlane));
	bool bReturn = geomSegments::CollideRayPlane(origin, ray, plane, &fTValue);
	return bReturn;
}

bool CommandPointAreaOverlap(const scrVector & A1, const scrVector & A2, float AWidth, const scrVector & B1, const scrVector & B2, float BWidth)
{
	Vector3 vA1(A1.x, A1.y, A1.z);
	Vector3 vA2(A2.x, A2.y, A2.z);
	Vector3 vB1(B1.x, B1.y, B1.z);
	Vector3 vB2(B2.x, B2.y, B2.z);

	if (vA1.IsClose(vA2, SMALL_FLOAT))
	{
		scriptAssertf(0, "%s:GET_POINT_AREA_OVERLAP: Box A has no length A1(%.2f,%.2f,%.2f) A2(%.2f,%.2f,%.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vA1.x, vA1.y, vA1.z, vA2.x, vA2.y, vA2.z);
		return false;
	}
	else if (vB1.IsClose(vB2, SMALL_FLOAT))
	{
		scriptAssertf(0, "%s:GET_POINT_AREA_OVERLAP: Box B has no length B1(%.2f,%.2f,%.2f) B2(%.2f,%.2f,%.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vB1.x, vB1.y, vB1.z, vB2.x, vB2.y, vB2.z);
		return false;
	}

	Vector3 vAForward = vA1 - vA2;
	vAForward.z = 0.0f;
	vAForward.Normalize();

	Vector3 vBForward = vB1 - vB2;
	vBForward.z = 0.0f;
	vBForward.Normalize();

	float fAHeight = abs(vA1.z - vA2.z);
	float fBHeight = abs(vB1.z - vB2.z);

	Mat34V MatA;
	Mat34VFromAxisAngle(MatA, VECTOR3_TO_VEC3V(vAForward), ScalarVFromF32(0.0f), VECTOR3_TO_VEC3V((vA1 + vA2)*0.5f));
	Mat34V MatB;
	Mat34VFromAxisAngle(MatB, VECTOR3_TO_VEC3V(vBForward), ScalarVFromF32(0.0f), VECTOR3_TO_VEC3V((vB1 + vB2)*0.5f));

	Mat34V RelativeMatrix;
	UnTransformOrtho(RelativeMatrix, MatA, MatB);
	
	Vec3V BoxA_hw(0.5f * AWidth, 0.5f * (vA2 - vA1).Mag(), 0.5f * fAHeight);
	Vec3V BoxB_hw(0.5f * BWidth, 0.5f * (vB2 - vB1).Mag(), 0.5f * fBHeight);

	return COT_TestBoxToBoxOBB(BoxA_hw, BoxB_hw, RelativeMatrix);
}

float CommandClosestTValueOnLine(const scrVector & vPointToTest, const scrVector & vA, const scrVector & vB, bool bClampToLine)
{
	//Convert to friendlier vector form
	Vec3V vecA(vA.x, vA.y, vA.z);
	Vec3V vecAtoB(vB.x - vA.x, vB.y - vA.y, vB.z - vA.z);
	Vec3V vecPnt(vPointToTest.x, vPointToTest.y, vPointToTest.z);

	//Get closest point
	//Calculate a TValue
	ScalarV vTValue;
	if (bClampToLine)
	{
		vTValue = geomTValues::FindTValueSegToPoint(vecA, vecAtoB, vecPnt);
	}
	else
	{
		vTValue = geomTValues::FindTValueOpenSegToPointV(vecA, vecAtoB, vecPnt);
	}
	return vTValue.Getf();
}

scrVector CommandClosestPointOnLine(const scrVector & vPointToTest, const scrVector & vA, const scrVector & vB, bool bClampToLine)
{
	//Convert to friendlier vector form
	Vec3V vecA(vA.x, vA.y, vA.z);
	Vec3V vecAtoB(vB.x - vA.x, vB.y - vA.y, vB.z - vA.z);
	Vec3V vecPnt(vPointToTest.x, vPointToTest.y, vPointToTest.z);

	//Calculate a TValue
	ScalarV vTValue;
	if (bClampToLine)
	{
		vTValue = geomTValues::FindTValueSegToPoint(vecA, vecAtoB, vecPnt);
	}
	else
	{
		vTValue = geomTValues::FindTValueOpenSegToPointV(vecA, vecAtoB, vecPnt);
	}

	//Re-project along line
	vecPnt = vecA + (vecAtoB * vTValue);

	//Convert and return
	return scrVector(vecPnt.GetXf(), vecPnt.GetYf(), vecPnt.GetZf());
}


#if DR_ENABLED

void DebugRecordStart()
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		debugPlayback::DebugRecorder::GetInstance()->StartRecording();
	}
}

void DebugRecordEnd()
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		debugPlayback::DebugRecorder::GetInstance()->StopRecording();
	}
}

void DebugRecordSave(const char *pFileName)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		debugPlayback::DebugRecorder::GetInstance()->Save(pFileName);
	}
}

void DebugRecordThisInst(int objectIndex)
{
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(objectIndex);
	debugPlayback::SetTrackAllEntities(false);
	if (pEntity)
	{
		debugPlayback::SetCurrentSelectedEntity(pEntity->GetCurrentPhysicsInst(), true);
		debugPlayback::SetTrackMode(debugPlayback::eTrackOne);
	}
}

void DebugRecordClearInsts()
{
	debugPlayback::ClearSelectedEntities();
}

void DebugRecordInst(int objectIndex)
{
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(objectIndex);
	if (Verifyf(pEntity && pEntity->GetCurrentPhysicsInst(), "Passed in invalid inst to add to list"))
	{
		debugPlayback::AddSelectedEntity(*pEntity->GetCurrentPhysicsInst());
	}
}

void DebugRecordAllInsts()
{
	debugPlayback::SetTrackAllEntities(true);
}

void DebugRecordRecordString(const char * pID, const char * pValue)	//VA_ARGS from script?
{
	debugPlayback::SetUseScriptCallstack(true);
	debugPlayback::AddSimpleLabel(pID, pValue);
	debugPlayback::SetUseScriptCallstack(false);
}


void DebugRecordSimpleLine(const char *pLabel, const scrVector & scrVecA, const scrVector & scrVecB, const scrVector & scrVecColorA, const scrVector & scrVecColorB)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		Vec3V vA(scrVecA.x, scrVecA.y, scrVecA.z);
		Vec3V vB(scrVecB.x, scrVecB.y, scrVecB.z);
		Color32 colorA(scrVecColorA.x, scrVecColorA.y, scrVecColorA.z, 1.0f);
		Color32 colorB(scrVecColorB.x, scrVecColorB.y, scrVecColorB.z, 1.0f);
		debugPlayback::SetUseScriptCallstack(true);
		debugPlayback::AddSimpleLine(pLabel, vA, vB, colorA, colorB);
		debugPlayback::SetUseScriptCallstack(false);
	}
}

void DebugRecordInstLine(int entityIndex, const char *pLabel, const scrVector & scrVecA, const scrVector & scrVecB, const scrVector & scrVecColorA, const scrVector & scrVecColorB)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			Vec3V vA(scrVecA.x, scrVecA.y, scrVecA.z);
			Vec3V vB(scrVecB.x, scrVecB.y, scrVecB.z);
			Color32 colorA(scrVecColorA.x, scrVecColorA.y, scrVecColorA.z, 1.0f);
			Color32 colorB(scrVecColorB.x, scrVecColorB.y, scrVecColorB.z, 1.0f);
			debugPlayback::SetUseScriptCallstack(true);
			debugPlayback::RecordInstLine(*pEntity->GetCurrentPhysicsInst(), pLabel, vA, vB, colorA, colorB);
			debugPlayback::SetUseScriptCallstack(false);
		}
	}
}

void DebugRecordSimpleSphere(const char *pLabel, const scrVector & scrVecA, float fRadius, const scrVector & scrVecColorA)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		Vec3V vA(scrVecA.x, scrVecA.y, scrVecA.z);
		Color32 colorA(scrVecColorA.x, scrVecColorA.y, scrVecColorA.z, 1.0f);
		debugPlayback::SetUseScriptCallstack(true);
		debugPlayback::AddSimpleSphere(pLabel, vA, fRadius, colorA);
		debugPlayback::SetUseScriptCallstack(false);
	}
}

void DebugRecordInstSphere(int entityIndex, const char *pLabel, const scrVector & scrVecA, float fRadius, const scrVector & scrVecColorA)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			Vec3V vA(scrVecA.x, scrVecA.y, scrVecA.z);
			Color32 colorA(scrVecColorA.x, scrVecColorA.y, scrVecColorA.z, 1.0f);
			debugPlayback::SetUseScriptCallstack(true);
			debugPlayback::RecordInstSphere(*pEntity->GetCurrentPhysicsInst(), pLabel, vA, fRadius, colorA);
			debugPlayback::SetUseScriptCallstack(false);
		}
	}
}

void DebugRecordRecordInstString(int objectIndex, const char *pID, const char * pValue)	//VA_ARGS from script?
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(objectIndex);
		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			debugPlayback::SetUseScriptCallstack(true);
			debugPlayback::RecordInstLabel(*pEntity->GetCurrentPhysicsInst(), pID, pValue);
			debugPlayback::SetUseScriptCallstack(false);
		}
	}
}

void DebugRecordRecordInstFloat(int objectIndex, const char * pID, float fValue)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(objectIndex);
		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			debugPlayback::SetUseScriptCallstack(true);
			debugPlayback::RecordTaggedFloatValue(*pEntity->GetCurrentPhysicsInst(), fValue, pID);
			debugPlayback::SetUseScriptCallstack(false);
		}
	}
}

void DebugRecordRecordInstVector(int objectIndex, const char *pID, const scrVector & vValue, bool bIsPosition)
{
	if (Verifyf(debugPlayback::DebugRecorder::GetInstance(), "DR_ENABLED but no recorder instance"))
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(objectIndex);
		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			Vec3V vecValue(vValue.x, vValue.y, vValue.z);
			debugPlayback::SetUseScriptCallstack(true);
			debugPlayback::RecordTaggedVectorValue(*pEntity->GetCurrentPhysicsInst(), vecValue, pID, bIsPosition ? debugPlayback::eVectorTypePosition : debugPlayback::eVectorTypeVelocity);
			debugPlayback::SetUseScriptCallstack(false);
		}
	}
}
#else
void DebugRecordStart(){};
void DebugRecordEnd(){};
void DebugRecordSave(const char *){};
void DebugRecordThisInst(int /*objectIndex*/){};
void DebugRecordInst(int /*objectIndex*/){};
void DebugRecordAllInsts(){};
void DebugRecordClearInsts(){};
void DebugRecordSimpleLine(const char *, const scrVector &, const scrVector &, const scrVector &, const scrVector &){};
void DebugRecordInstLine(int , const char *, const scrVector &, const scrVector &, const scrVector & , const scrVector &){};
void DebugRecordSimpleSphere(const char *, const scrVector &, float, const scrVector &){};
void DebugRecordInstSphere(int , const char *, const scrVector &, float , const scrVector &){};
void DebugRecordRecordString(const char * /*pID*/, const char * /*pValue*/){}
void DebugRecordRecordInstString(int /*objectIndex*/, const char * /*pID*/, const char * /*pValue*/){};
void DebugRecordRecordInstFloat(int /*objectIndex*/, const char * /*pID*/, float /*fValue*/){};
void DebugRecordRecordInstVector(int /*objectIndex*/, const char * /*pID*/, const scrVector & /*vValue*/){};
#endif

struct AreaOccupiedDataStruct
{
	bool FoundEntity;
	bool bCheckIsAlive;
	const CEntity * pExcludeEntity;
};


bool AnyEntitiesInAreaCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	AreaOccupiedDataStruct* pAreaOccupiedData = reinterpret_cast<AreaOccupiedDataStruct*>(data);

	if (pAreaOccupiedData->FoundEntity == false && pEntity != pAreaOccupiedData->pExcludeEntity)
	{
		if (pAreaOccupiedData->bCheckIsAlive && pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->IsDead())
			return true;
		pAreaOccupiedData->FoundEntity = true;
		return false;	// done with the callbacks, no need to get any more results
	}

	return(true);
}


bool AnyMissionEntitiesInAreaCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	AreaOccupiedDataStruct* pAreaOccupiedData = reinterpret_cast<AreaOccupiedDataStruct*>(data);

	if (pAreaOccupiedData->FoundEntity == false && pEntity != pAreaOccupiedData->pExcludeEntity)
	{
		if (CTheScripts::IsMissionEntity(pEntity))
		{
			pAreaOccupiedData->FoundEntity = true;
			return false;	// done with the callbacks, no need to get any more results
		}
	}

	return(true);
}


bool IsSearchVolumeOccupied(fwSearchVolume & searchVolume, bool bBuildingFlag, bool bVehicleFlag, bool bPedFlag, bool bObjectFlag, bool bDummyFlag, bool bCheckAlive, int excludeGuid)
{
	AreaOccupiedDataStruct AreaOccupiedData;
	AreaOccupiedData.FoundEntity = false;
	AreaOccupiedData.pExcludeEntity = NULL;
	AreaOccupiedData.bCheckIsAlive = bCheckAlive;

	if (NULL_IN_SCRIPTING_LANGUAGE != excludeGuid)
	{
		AreaOccupiedData.pExcludeEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(excludeGuid);
	}

	s32 typeFlags = 0;
	if (bBuildingFlag)
	{
		typeFlags |= ENTITY_TYPE_MASK_BUILDING;
	}

	if (bVehicleFlag)
	{
		typeFlags |= ENTITY_TYPE_MASK_VEHICLE;
	}

	if (bPedFlag)
	{
		typeFlags |= ENTITY_TYPE_MASK_PED;
	}

	if (bObjectFlag)
	{
		typeFlags |= ENTITY_TYPE_MASK_OBJECT;
	}

	if (bDummyFlag)
	{
		typeFlags |= ENTITY_TYPE_MASK_DUMMY_OBJECT;
	}

	SEARCH_OPTION_FLAGS SearchFlags = SEARCH_OPTION_DYNAMICS_ONLY;
	if (bBuildingFlag || bDummyFlag)
	{
		SearchFlags = SEARCH_OPTION_NONE;
	}

	CGameWorld::ForAllEntitiesIntersecting(&searchVolume, AnyEntitiesInAreaCB, static_cast<void*>(&AreaOccupiedData),
		typeFlags, SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SearchFlags, WORLDREP_SEARCHMODULE_SCRIPT);

	if (AreaOccupiedData.FoundEntity)
	{
		return true;
	}

	return false;
}

bool CommandIsAreaOccupied(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bBuildingFlag, bool bVehicleFlag, bool bPedFlag, bool bObjectFlag, bool bDummyFlag, int excludeGuid, bool bCheckAlive)
{
	Vector3 VecPosMin = Vector3 (scrVecMinCoors);
	Vector3 VecPosMax = Vector3 (scrVecMaxCoors);

	RefactorValuesForAreaCheck(VecPosMin, VecPosMax);

	spdAABB testBox;
	testBox.Set(RCC_VEC3V(VecPosMin), RCC_VEC3V(VecPosMax));
	fwIsBoxIntersectingApprox searchBox(testBox);
	return IsSearchVolumeOccupied(searchBox, bBuildingFlag, bVehicleFlag, bPedFlag, bObjectFlag, bDummyFlag, bCheckAlive, excludeGuid);
}

bool CommandIsAreaOccupiedAccurate(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bBuildingFlag, bool bVehicleFlag, bool bPedFlag, bool bObjectFlag, bool bDummyFlag, int excludeGuid, bool bCheckAlive)
{
	Vector3 VecPosMin = Vector3(scrVecMinCoors);
	Vector3 VecPosMax = Vector3(scrVecMaxCoors);

	RefactorValuesForAreaCheck(VecPosMin, VecPosMax);

	spdAABB testBox;
	testBox.Set(RCC_VEC3V(VecPosMin), RCC_VEC3V(VecPosMax));
	fwIsBoxIntersectingBB searchBox(testBox);
	return IsSearchVolumeOccupied(searchBox, bBuildingFlag, bVehicleFlag, bPedFlag, bObjectFlag, bDummyFlag, bCheckAlive, excludeGuid);
}

bool CommandIsPositionOccupied(const scrVector & scrVecCenterCoords, float radius, bool bBuildingFlag, bool bVehicleFlag, bool bPedFlag, bool bObjectFlag, bool bDummyFlag, int excludeGuid,bool  bCheckAlive)
{
	const Vector3 centre = Vector3(scrVecCenterCoords);
	fwIsSphereIntersecting searchSphere(spdSphere(RCC_VEC3V(centre), ScalarV(radius)));

	return IsSearchVolumeOccupied(searchSphere, bBuildingFlag, bVehicleFlag, bPedFlag, bObjectFlag, bDummyFlag, bCheckAlive, excludeGuid);
}


bool CommandIsPointObscuredByAMissionEntity(const scrVector & scrVecCoors, const scrVector & scrVecDimension, int excludeGuid)
{
	AreaOccupiedDataStruct AreaOccupiedData;
	AreaOccupiedData.FoundEntity = false;
	AreaOccupiedData.pExcludeEntity = NULL;

	if (NULL_IN_SCRIPTING_LANGUAGE != excludeGuid)
	{
		AreaOccupiedData.pExcludeEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(excludeGuid);
	}

	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecDimension = Vector3 (scrVecDimension);

	Vector3 vBoxMin;
	Vector3 vBoxMax;

	vBoxMin.x = VecPos.x - VecDimension.x; 
	vBoxMax.x = VecPos.x + VecDimension.x;
	vBoxMin.y = VecPos.y - VecDimension.y; 
	vBoxMax.y = VecPos.y + VecDimension.y;
	vBoxMin.z = VecPos.z - VecDimension.z;
	vBoxMax.z = VecPos.z + VecDimension.z;

	RefactorValuesForAreaCheck(vBoxMin, vBoxMax);

	spdAABB testBox;
	testBox.Set(RCC_VEC3V(vBoxMin), RCC_VEC3V(vBoxMax));

	fwIsBoxIntersectingApprox searchBox(testBox);
	CGameWorld::ForAllEntitiesIntersecting(&searchBox, AnyMissionEntitiesInAreaCB, static_cast<void*>(&AreaOccupiedData),
		ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_DYNAMICS_ONLY, WORLDREP_SEARCHMODULE_SCRIPT);

	if (AreaOccupiedData.FoundEntity)
	{
		return true;
	}

	return false;
}

void CommandClearAreaOfProjectiles(const scrVector & vPos, float fRadius, bool bBroadcast)
{
	spdAABB spdBounds = spdAABB( spdSphere((Vec3V)vPos, ScalarV(fRadius)) );
	CProjectileManager::RemoveAllProjectilesInBounds(spdBounds);
	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vPos, fRadius, CClearAreaEvent::CLEAR_AREA_FLAG_PROJECTILES);
	}
}

void CommandClearArea(const scrVector & scrVecCoors, float Radius, bool DeleteProjectilesFlag, bool bLeaveCarGenCars, bool bClearLowPriorityPickupsOnly, bool bBroadcast)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}

#if !__NO_OUTPUT
	//if(CVehicleFactory::ms_bLogDestroyedVehicles)
	{
		Displayf("******************************************************************************************************************************");
		Displayf("COMMAND_CLEAR_AREA from \"%s\" Position (%.1f,%.1f,%.1f) Radius:%.1f",
			CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
			VecCoors.x, VecCoors.y, VecCoors.z, Radius);
		Displayf("    DeleteProjectilesFlag:%s, bLeaveCarGenCars:%s, bClearLowPriorityPickupsOnly:%s, bBroadcast:%s",
			DeleteProjectilesFlag ? "true":"false",
			bLeaveCarGenCars ? "true":"false",
			bClearLowPriorityPickupsOnly ? "true":"false",
			bBroadcast ? "true":"false");
		Displayf("******************************************************************************************************************************");
	}
#endif

	CGameWorld::ClearExcitingStuffFromArea(VecCoors, Radius, DeleteProjectilesFlag, false, false, bLeaveCarGenCars, NULL, bClearLowPriorityPickupsOnly, true);

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		// inform other machines that this area is being cleared
		u32 clearFlags = CClearAreaEvent::CLEAR_AREA_FLAG_ALL;

		if (DeleteProjectilesFlag)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_PROJECTILES;

		if (!bLeaveCarGenCars)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_CARGENS;

		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, clearFlags);
	}
}


void CommandClearAreaLeaveVehicleHealth( const scrVector & scrVecCoors, float Radius, bool DeleteProjectilesFlag, bool bLeaveCarGenCars, bool bClearLowPriorityPickupsOnly, bool bBroadcast )
{
#if __BANK
	if(CVehicleFactory::ms_bLogDestroyedVehicles)
	{
		Displayf("******************************************************************************************************************************");
		Displayf("CLEAR_AREA_LEAVE_VEHICLE_HEALTH from \"%s\" Position (%.1f,%.1f,%.1f) Radius:%.1f",
			CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
			scrVecCoors.x, scrVecCoors.y, scrVecCoors.z, Radius);
		Displayf("    DeleteProjectilesFlag:%s, bLeaveCarGenCars:%s, bClearLowPriorityPickupsOnly:%s, bBroadcast:%s",
			DeleteProjectilesFlag ? "true":"false",
			bLeaveCarGenCars ? "true":"false",
			bClearLowPriorityPickupsOnly ? "true":"false",
			bBroadcast ? "true":"false");
		Displayf("******************************************************************************************************************************");
	}
#endif

	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}
	CGameWorld::ClearExcitingStuffFromArea(VecCoors, Radius, DeleteProjectilesFlag, false, false, bLeaveCarGenCars, NULL, bClearLowPriorityPickupsOnly, true, false );

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		// inform other machines that this area is being cleared
		u32 clearFlags = CClearAreaEvent::CLEAR_AREA_FLAG_ALL;

		if (DeleteProjectilesFlag)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_PROJECTILES;

		if (!bLeaveCarGenCars)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_CARGENS;

		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, clearFlags);
	}
}


void CommandClearAreaOfVehicles(const scrVector & scrVecCoors, float Radius, bool bLeaveCarGenCars, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bBroadcast, bool bIfEngineOnFire, bool bKeepScriptTrains)
{
#if __BANK
	Displayf("******************************************************************************************************************************");
	Displayf("CLEAR_AREA_OF_VEHICLES from \"%s\" Position (%.1f,%.1f,%.1f) Radius:%.1f",
		CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
		scrVecCoors.x, scrVecCoors.y, scrVecCoors.z, Radius);
	Displayf("    bLeaveCarGenCars:%s, bCheckViewFrustum:%s, bIfWrecked:%s, bIfAbandoned:%s, bBroadcast:%s, bIfEngineOnFire:%s, bKeepScriptTrains:%s",
		bLeaveCarGenCars ? "true":"false",
		bCheckViewFrustum ? "true":"false",
		bIfWrecked ? "true":"false",
		bIfAbandoned ? "true":"false",
		bBroadcast ? "true":"false",
		bIfEngineOnFire ? "true":"false",
		bKeepScriptTrains ? "true":"false");
	Displayf("******************************************************************************************************************************");
#endif

	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}

	CGameWorld::ClearCarsFromArea(VecCoors, Radius, false, bLeaveCarGenCars, bCheckViewFrustum, bIfWrecked, bIfAbandoned, true, bIfEngineOnFire, bKeepScriptTrains);

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		u32 clearFlags = CClearAreaEvent::CLEAR_AREA_FLAG_VEHICLES;

		if (!bLeaveCarGenCars)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_CARGENS;

		if(bCheckViewFrustum)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_CHECK_ALL_PLAYERS_FRUSTUMS;

		if(bIfWrecked)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_WRECKED_STATUS;

		if(bIfAbandoned)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_ABANDONED_STATUS;

		if (bIfEngineOnFire)
			clearFlags |= CClearAreaEvent::CLEAR_AREA_FLAG_ENGINE_ON_FIRE;

		// inform other machines that this area is being cleared
		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, clearFlags);
	}
}

void CommandClearAngledAreaOfVehicles(const scrVector & scrVecAngledAreaPoint1, const scrVector & scrVecAngledAreaPoint2, float DistanceOfOppositeFace, bool bLeaveCarGenCars, bool bBroadcast, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bIfEngineOnFire, bool bKeepScriptTrains)
{
#if __BANK
	if(CVehicleFactory::ms_bLogDestroyedVehicles)
	{
		Displayf("******************************************************************************************************************************");
		Displayf("CLEAR_ANGLED_AREA_OF_VEHICLES from \"%s\" Pt1 (%.1f,%.1f,%.1f) Pt2 (%.1f,%.1f,%.1f) Dist:%.1f",
			CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
			scrVecAngledAreaPoint1.x, scrVecAngledAreaPoint1.y, scrVecAngledAreaPoint1.z,
			scrVecAngledAreaPoint2.x, scrVecAngledAreaPoint2.y, scrVecAngledAreaPoint2.z,
			DistanceOfOppositeFace);
		Displayf("bLeaveCarGenCars:%s, bCheckViewFrustum:%s, bIfWrecked:%s, bIfAbandoned:%s, bBroadcast:%s, bIfEngineOnFire:%s, bKeepScriptTrains:%s",
			bLeaveCarGenCars ? "true":"false",
			bCheckViewFrustum ? "true":"false",
			bIfWrecked ? "true":"false",
			bIfAbandoned ? "true":"false",
			bBroadcast ? "true":"false",
			bIfEngineOnFire ? "true":"false",
			bKeepScriptTrains ? "true":"false");
		Displayf("******************************************************************************************************************************");
	}
#endif

	Vector3 vecAngledAreaPoint1 = Vector3(scrVecAngledAreaPoint1);
	Vector3 vecAngledAreaPoint2 = Vector3(scrVecAngledAreaPoint2);

	CGameWorld::ClearCarsFromAngledArea(vecAngledAreaPoint1, vecAngledAreaPoint2, DistanceOfOppositeFace, false, bLeaveCarGenCars, bCheckViewFrustum, bIfWrecked, bIfAbandoned, true, bIfEngineOnFire, bKeepScriptTrains);

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		scriptAssertf(0, "%s:CLEAR_ANGLED_AREA_OF_VEHICLES - this is not supported in multi-player. See JohnG", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void ClearAreaOfObjects(const scrVector & scrVecCoors, float Radius, s32 controlFlags=0)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}

#if !__NO_OUTPUT
	Displayf("******************************************************************************************************************************");
	Displayf("CLEAR_AREA_OF_OBJECTS from \"%s\" Position (%.1f,%.1f,%.1f) Radius:%.1f controlFlags:%d",
		CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
		VecCoors.x, VecCoors.y, VecCoors.z, Radius, controlFlags);
	Displayf("******************************************************************************************************************************");
#endif

	const bool bFlagForce			= (controlFlags & CLEAROBJ_FLAG_FORCE)!=0;
	const bool bFlagIncludeDoors	= (controlFlags & CLEAROBJ_FLAG_INCLUDE_DOORS)!=0;
	const bool bFlagObjsWithBrains	= (controlFlags & CLEAROBJ_FLAG_INCLUDE_OBJWITHBRAINS)!=0;
	const bool bFlagBroadcast		= (controlFlags & CLEAROBJ_FLAG_BROADCAST)!=0;
	const bool bFlagExcludeLadders	= (controlFlags & CLEAROBJ_FLAG_EXCLUDE_LADDERS)!=0;
	
	CGameWorld::ClearObjectsFromArea(VecCoors, Radius, bFlagObjsWithBrains, NULL, bFlagForce, bFlagIncludeDoors, bFlagExcludeLadders);

	if (bFlagBroadcast && NetworkInterface::IsGameInProgress())
	{
		// inform other machines that this area is being cleared
		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, CClearAreaEvent::CLEAR_AREA_FLAG_OBJECTS);
	}
}

void CommandClearAreaOfObjects(const scrVector & scrVecCoors, float Radius, s32 controlFlags/*=0*/)
{
#if __BANK
	Displayf("******************************************************************************************************************************");
	Displayf("CLEAR_AREA_OF_OBJECTS from \"%s\" Position (%.1f,%.1f,%.1f) Radius:%.1f",
		CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName(),
		scrVecCoors.x, scrVecCoors.y, scrVecCoors.z, Radius);
	Displayf("    controlFlags:%x, ",
		controlFlags);
	Displayf("******************************************************************************************************************************");
#endif
	ClearAreaOfObjects(scrVecCoors, Radius, controlFlags);
}

void CommandClearAreaOfPeds(const scrVector & scrVecCoors, float Radius, bool bBroadcast)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}
	CGameWorld::ClearPedsFromArea(VecCoors, Radius);

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		// inform other machines that this area is being cleared
		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, CClearAreaEvent::CLEAR_AREA_FLAG_PEDS);
	}
}

void CommandClearAreaOfCops(const scrVector & scrVecCoors, float Radius, bool bBroadcast)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	if (VecCoors.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecCoors.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecCoors.x, VecCoors.y);
	}
	CGameWorld::ClearCopsFromArea(VecCoors, Radius);

	if (bBroadcast && NetworkInterface::IsGameInProgress())
	{
		// inform other machines that this area is being cleared
		CClearAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecCoors, Radius, CClearAreaEvent::CLEAR_AREA_FLAG_COPS);
	}
}

void CommandClearScenarioSpawnHistory()
{
	CScenarioManager::GetSpawnHistory().ClearHistory();
}

void CommandActivateSaveMenu(bool bAllowWhilePlayerIsInAVehicle)
{
	if (CGameWorld::FindLocalPlayer())
	{
		if (bAllowWhilePlayerIsInAVehicle || (CGameWorld::FindLocalPlayer()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) == false) )
		{	
			// stop the player and request the frontend opens up in savegame mode:
			CGameWorld::FindLocalPlayer()->SetVelocity(Vector3(0,0,0));
			CGameWorld::FindLocalPlayer()->SetAngVelocity(Vector3(0,0,0));
			CSavegameSave::SetSaveHasJustOccurred(false);

			CPauseMenu::QueueOpen(FE_MENU_VERSION_SAVEGAME);
		}
	}
}

int CommandGetStatusOfManualSave()
{
	return CPauseMenu::sm_ManualSaveStructure.GetStatus();
}

bool CommandDidSaveCompleteSuccessfully()
{
	if (CSavegameSave::GetSaveHasJustOccurred())
	{
		return true;
	}
	else
	{
		return false;
	}
}
/*
void CommandActivateNetworkSettingsMenu()
{
	if (CGameWorld::FindLocalPlayer())
	{
		//if (CGameWorld::FindLocalPlayer()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) == false) // Removed - See BUG 65128
		{	
			// stop the player and request the frontend opens up in savegame mode:
			CGameWorld::FindLocalPlayer()->SetVelocity(Vector3(0,0,0));
			CGameWorld::FindLocalPlayer()->SetAngVelocity(Vector3(0,0,0));
			CFrontEnd::RequestState(FRONTEND_STATE_OPEN_PLAYERSETTINGS);
		}
	}
}*/

void CommandSetNetworkMenu (bool UNUSED_PARAM(bActive))
{
	scriptAssertf(0, "SET_NETWORK_SETTINGS_MENU - depreciated command!  shouldnt be used!");

/*	if (bActive)
	{
		CommandActivateNetworkSettingsMenu();
	}
	else
	{
		if (CFrontEnd::IsInPlayerSettingsMenus())
		{	
			CFrontEnd::RequestState(FRONTEND_STATE_CLOSE);
		}
	}*/
}


bool CommandIsGermanVersion()
{
	return CLocalisation::GermanGame();
}

bool CommandIsAussieVersion()
{
	return CLocalisation::AussieGame();
}

bool CommandIsJapaneseVersion()
{
	return sysAppContent::IsJapaneseBuild();
}

void CommandSetCreditsActive (bool bActive )
{
	if (bActive)
	{
		CCredits::Init(INIT_AFTER_MAP_LOADED);
	}
	else
	{
		CCredits::Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	}
}

void CommandSetCreditsFadeOutWithScreen(bool bFade)
{
	CCredits::SetToRenderBeforeFade(bFade);
}

bool CommandHaveCreditsReachedEnd()
{
	return (CCredits::HaveReachedEnd());
}

bool CommandAreCreditsRunning()
{
	return (CCredits::IsRunning());
}

void CommandTerminateAllScriptsWithThisName(const char *pName)
{
	if(SCRIPT_VERIFY((stricmp(CTheScripts::GetCurrentScriptName(), pName) != 0),"TERMINATE_ALL_SCRIPTS_WITH_THIS_NAME - Script can't terminate itself using this command"))
	{	
		GtaThread::KillAllThreadsWithThisName(pName);
	}
}

//	void CommandTerminateAllScriptsForNetworkGame()
//	{
//		GtaThread::KillAllThreadsForNetworkGame();
//		CTheScripts::GetScriptBrainDispatcher().PurgeWaitingItems();
//	}


// Check with Keith about removing this command and relying on the HAS_FORCE_CLEANUP_OCCURRED flags instead.
//	Any script that is currently calling NETWORK_SET_SCRIPT_IS_SAFE_FOR_NETWORK_GAME probably shouldn't be checking FORCE_CLEANUP_FLAG_SP_TO_MP.
void CommandThisScriptIsSafeForNetworkGame()
{
	CTheScripts::GetScriptHandlerMgr().SetScriptAsSafeToRunInMP(*CTheScripts::GetCurrentGtaScriptThread());
}

bool CommandGetIsScriptSafeForNetworkGame()
{
	return CTheScripts::GetCurrentGtaScriptThread()->bSafeForNetworkGame;
}

void CommandSetPlayerControlOnInMissionCleanup(bool bSetPlayerControlOnInCleanup)
{
	//	default setting is true
	if(SCRIPT_VERIFY(CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript, "SET_PLAYER_CONTROL_ON_IN_MISSION_CLEANUP - can only call this in a mission script"))
	{
		if (bSetPlayerControlOnInCleanup)
		{
			CTheScripts::GetCurrentGtaScriptThread()->bSetPlayerControlOnInMissionCleanup = true;
		}
		else
		{
			CTheScripts::GetCurrentGtaScriptThread()->bSetPlayerControlOnInMissionCleanup = false;
		}
	}
}


float CommandGetDistanceBetweenCoords(const scrVector & scrVecFirstCoors, const scrVector & scrVecSecondCoors, bool bCheck3d)
{
	Vector3 vecDiff;
	float ReturnDistance;
	
	Vector3 vecFirst = Vector3(scrVecFirstCoors);
	Vector3 vecSecond = Vector3(scrVecSecondCoors);

	if (!bCheck3d)
	{
		vecFirst.z = 0.0f;
		vecSecond.z = 0.0f;
	}
	
	vecDiff = vecFirst - vecSecond;
	ReturnDistance = vecDiff.Mag();

	return ReturnDistance;

}

float CommandGetAngleBetween2dVectors(float X1, float Y1, float X2, float Y2)
{
	float ResultAngle;
	ResultAngle = ((X1 * X2) + (Y1 * Y2));
	ResultAngle /= (   rage::SqrtfSafe((X1 * X1) + (Y1 * Y1))  *  rage::SqrtfSafe((X2 * X2) + (Y2 * Y2)) );
	ResultAngle = rage::Max(-1.0f, ResultAngle);		// Rounding errors can cause the value to be outside (-1.0 .. 1.0). This messes up the acos.
	ResultAngle = rage::Min(1.0f, ResultAngle);
	ResultAngle = rage::AcosfSafe(ResultAngle);
	ResultAngle = ( RtoD * ResultAngle);
	
	return ResultAngle;
}

float CommandGetHeadingFromVector2d(float VecX, float VecY)
{
	float temp_float;

	temp_float = fwAngle::GetATanOfXY( VecX, VecY );
	temp_float = ( RtoD * temp_float);
	temp_float -= 90.0f;
	while (temp_float < 0.0f)
	{
		temp_float += 360.0f;
	}

	return temp_float;
}

u8 m_fakeVehicleSequence = 0;
u8 AllocateVehicleFakeSequence()
{
	u8 result = m_fakeVehicleSequence;
	if((m_fakeVehicleSequence + 1) >= CNetworkPendingProjectiles::MAX_PENDING_PROJECTILES)
	{
		m_fakeVehicleSequence = CNetworkPendingProjectiles::MAX_PENDING_PROJECTILES_PER_TYPE;
	}
	else
	{
		m_fakeVehicleSequence++;
	}

	return result;
}

void CommandFireSingleBulletIgnoreEntityNew(const scrVector & scrVecStartCoors, const scrVector & scrVecEndCoors, int DamageCaused, bool bSetPerfectAccuracy, s32 WeaponType, s32 PedIndex, bool bCreateTraceVfx, bool bAllowRumble, float InitialVelocity, int IgnoreEntity, bool bForceCreateNewProjectileObject, bool bDisablePlayerCoverStartAdjustment, s32 TargetEntity, bool bDoDeadCheck, bool bFreezeProjectileWaitingOnCollision, bool bSetIgnoreCollisionEnitity, bool bIgnoreCollisionResetNoBB = false)
{
	const Vector3 vStartTemp = scrVecStartCoors;
	const Vector3 vEndTemp = scrVecEndCoors;

	Displayf("SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY_NEW - Command Called");
#if __ASSERT
	
	scriptAssertf(!vStartTemp.IsClose(vEndTemp, SMALL_FLOAT), "Start and end coords are the same start : (%.2f,%.2f,%.2f), end : (%.2f,%.2f,%.2f)", scrVecStartCoors.x, scrVecStartCoors.y, scrVecStartCoors.z, scrVecEndCoors.x, scrVecEndCoors.y, scrVecEndCoors.z);
#endif // __ASSERT

	if(SCRIPT_VERIFY(DamageCaused >= 0, "SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY_NEW - damage must have a positive value"))
	{
		CPed* pPed = NULL;

		if(PedIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
			if(!bDoDeadCheck)
			{
				assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK;
			}

			pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, assertFlags);
		}

		CWeapon weapon(WeaponType, CWeapon::INFINITE_AMMO, NULL, true);

		Matrix34 weaponMatrix = weapon.BuildViewMatrix(vStartTemp, vEndTemp - vStartTemp);

		CEntity* pEntity = pPed;
		if(pPed && pPed->GetIsInVehicle() && weapon.GetWeaponInfo()->GetIsVehicleWeapon())
		{
			pEntity = pPed->GetMyVehicle();
		}

		CWeapon::sFireParams params(pEntity, weaponMatrix, &vStartTemp, &vEndTemp);
		params.iFireFlags = bSetPerfectAccuracy ? CWeapon::FF_SetPerfectAccuracy : 0;
		if (bCreateTraceVfx)
		{
			params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
		}
		else
		{
			params.iFireFlags.SetFlag( CWeapon::FF_ForceNoBulletTrace );
		}
		params.fApplyDamage = (float)DamageCaused;
		
		params.bCommandFireSingleBullet = true;
		params.fakeStickyBombSequenceId = AllocateVehicleFakeSequence();

		params.fInitialVelocity = InitialVelocity;
		params.bAllowRumble = bAllowRumble;
		params.bCreateNewProjectileObject = bForceCreateNewProjectileObject;	// Ensures we don't use the firing ped's projectile ordnance if one is equipped
		params.bDisablePlayerCoverStartAdjustment = bDisablePlayerCoverStartAdjustment;
		params.bProjectileCreatedByScriptWithNoOwner = !pEntity;
		params.bFreezeProjectileWaitingOnCollision = bFreezeProjectileWaitingOnCollision;

		if(IgnoreEntity != NULL_IN_SCRIPTING_LANGUAGE)
		{
			CEntity *pIgnoreEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(IgnoreEntity, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			params.pIgnoreDamageEntity = pIgnoreEntity;
			if (bSetIgnoreCollisionEnitity || bIgnoreCollisionResetNoBB)
			{
				params.pIgnoreCollisionEntity = pIgnoreEntity;
				params.bIgnoreCollisionResetNoBB = bIgnoreCollisionResetNoBB;
			}
		}

		if(TargetEntity != NULL_IN_SCRIPTING_LANGUAGE)
		{
			CEntity *pTargetEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(TargetEntity, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			params.pTargetEntity = pTargetEntity;
		}

#if __BANK
		Displayf("SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY_NEW - Weapon Fired Between (%.2f,%.2f,%.2f) and (%.2f,%.2f,%.2f); damage - %i; weaponType - %i; firing entity - %s (%p), targetEntity - %s (%p), ignoreEntity - %s (%p), initialVelocity - %.2f, bSetPerfectAccuracy - %s, bCreateTraceVfx - %s, bAllowRumble - %s, bForceCreateNewProjectileObject - %s, bDisablePlayerCoverStartAdjustment - %s, bDoDeadCheck - %s, bFreezeProjectileWaitingOnCollision - %s, bSetIgnoreCollisionEnitity - %s",
			scrVecStartCoors.x, scrVecStartCoors.y, scrVecStartCoors.z, scrVecEndCoors.x, scrVecEndCoors.y, scrVecEndCoors.z,
			DamageCaused, WeaponType,
			pEntity && pEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pEntity)) : (pEntity ? pEntity->GetModelName() : "null"), pEntity,
			params.pTargetEntity && params.pTargetEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(params.pTargetEntity.Get())) : (params.pTargetEntity ? params.pTargetEntity->GetModelName() : "null"), params.pTargetEntity.Get(),
			params.pIgnoreDamageEntity && params.pIgnoreDamageEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(params.pIgnoreDamageEntity.Get())) : (params.pIgnoreDamageEntity ? params.pIgnoreDamageEntity->GetModelName() : "null"), params.pIgnoreDamageEntity.Get(),
			InitialVelocity, AILogging::GetBooleanAsString(bSetPerfectAccuracy), AILogging::GetBooleanAsString(bCreateTraceVfx), AILogging::GetBooleanAsString(bAllowRumble), AILogging::GetBooleanAsString(bForceCreateNewProjectileObject),
			AILogging::GetBooleanAsString(bDisablePlayerCoverStartAdjustment), AILogging::GetBooleanAsString(bDoDeadCheck), AILogging::GetBooleanAsString(bFreezeProjectileWaitingOnCollision), AILogging::GetBooleanAsString(bSetIgnoreCollisionEnitity));
#endif

		weapon.Fire(params);
	}
}

void CommandFireSingleBulletIgnoreEntity(const scrVector & scrVecStartCoors, const scrVector & scrVecEndCoors, int DamageCaused, bool bSetPerfectAccuracy, s32 WeaponType, s32 PedIndex, bool bCreateTraceVfx, bool bAllowRumble, float InitialVelocity, int IgnoreEntity, s32 TargetEntity)
{
	CommandFireSingleBulletIgnoreEntityNew(scrVecStartCoors, scrVecEndCoors, DamageCaused, bSetPerfectAccuracy, WeaponType, PedIndex, bCreateTraceVfx, bAllowRumble, InitialVelocity, IgnoreEntity, false, false, TargetEntity, true, false, false);
}

void CommandFireSingleBullet(const scrVector & scrVecStartCoors, const scrVector & scrVecEndCoors, int DamageCaused, bool bSetPerfectAccuracy, s32 WeaponType, s32 PedIndex, bool bCreateTraceVfx, bool bAllowRumble, float InitialVelocity)
{
	CommandFireSingleBulletIgnoreEntity(scrVecStartCoors, scrVecEndCoors, DamageCaused, bSetPerfectAccuracy, WeaponType, PedIndex, bCreateTraceVfx, bAllowRumble, InitialVelocity, 0, 0);
}

void CommandGetModelDimensions(int VehicleModelHashKey, Vector3& returnMin, Vector3& returnMax)
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if(SCRIPT_VERIFY(pModelInfo, "GET_MODEL_DIMENSIONS - model doesn't exist"))
	{
		returnMin = pModelInfo->GetBoundingBoxMin();
		returnMax = pModelInfo->GetBoundingBoxMax();

		// GTAV - B*1755458 - The zentorno is deleted at the start of the mission because
		// it is too wide.
		// this is because the bounds for the doors have a rotation and stick out further than they should
		// it should really be fixed when the asset is created but that could break other things
		// this hack will just fix the bug and won't break anything else.
		if( pModelInfo->GetModelNameHash() == MI_CAR_ZENTORNO.GetName().GetHash() )
		{
			returnMin.x += 0.2f;
			returnMax.x -= 0.2f;
		}
#if RSG_GEN9
		if (pModelInfo->GetModelNameHash() == MI_CAR_CYCLONE2.GetName().GetHash())
		{
			static float shrinkX = 0.3f;
			returnMin.x += shrinkX;
			returnMax.x -= shrinkX;
		}
#endif // RSG_GEN9
	}
}

bool CommandIsBitSet(int Variable, int BitIndex)
{
	if(SCRIPT_VERIFY( ((BitIndex >=0) && (BitIndex <=31)), "IS_BIT_SET - bit must be between 0 to 31"))
	{
		return (Variable & (1 << BitIndex)) != 0;
	}
	return false;
}

void CommandSetBit(int &Variable, int BitIndex)
{
	if(SCRIPT_VERIFY( ((BitIndex >=0) && (BitIndex <=31)), "SET_BIT - bit must be between 0 to 31"))
	{
		Variable |= (1 << BitIndex);
	}
}


void CommandClearBit(int &Variable, int BitIndex)
{
	if(SCRIPT_VERIFY( ((BitIndex >=0) && (BitIndex <=31)), "CLEAR_BIT - bit must be between 0 to 31"))
	{
		Variable &= ~(1 << BitIndex);
	}
}


int CommandGetHashKey(const char *pString)
{
	u32 HashKey = 0;
	HashKey = atStringHash(pString);
	return ( *(reinterpret_cast<s32*>(&HashKey)) );
}

void CommandSlerpNearQuaternion(float Phase, float StartX, float StartY, float StartZ, float StartW, float TargetX, float TargetY, float TargetZ, float TargetW, float& ResultX, float& ResultY, float& ResultZ, float& ResultW )
{
	Quaternion Start(StartX,StartY, StartZ, StartW); 
	Quaternion Target(TargetX,TargetY, TargetZ, TargetW); 
	Quaternion Current; 

	Start.Normalize(); 
	Target.Normalize(); 

	Current.SlerpNear(Phase, Start, Target);

	ResultX = Current.x;
	ResultY = Current.y;
	ResultZ = Current.z;
	ResultW = Current.w;
}

void CommandUsingMissionCreator(bool bNewState)
{
	if(bNewState != CScriptHud::bUsingMissionCreator)
	{
#if GTA_REPLAY
		if(bNewState)
			CReplayMgr::OnWorldNotRecordable();
		else
			CReplayMgr::OnWorldBecomesRecordable();
#endif // GTA_REPLAY
		
		scriptDebugf1("CommandUsingMissionCreator :: %s -> %s", CScriptHud::bUsingMissionCreator ? "True" : "False", bNewState ? "True" : "False");
		CScriptHud::bUsingMissionCreator = bNewState;
	}
}

void CommandAllowMissionCreatorWarp(bool bAllow)
{
	if (CScriptHud::bAllowMissionCreatorWarp != bAllow)
	{
		CScriptHud::bAllowMissionCreatorWarp = bAllow;

		if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent())
		{
			CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
		}
	}
}

void CommandSetMinigameInProgress(bool bNewState)
{
	if (bNewState)
	{
#if __DEV
		Displayf("SET_MINIGAME_IN_PROGRESS(TRUE) called by %s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

		// Make sure to clear any old input data history.
		CControlMgr::GetMainPlayerControl().InitEmptyControls();

		if (CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript == false)
		{
			CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript = true;
			CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages = false;

			CTheScripts::SetNumberOfMiniGamesInProgress(CTheScripts::GetNumberOfMiniGamesInProgress() + 1);
			scriptAssertf(CTheScripts::GetNumberOfMiniGamesInProgress() < 3, "%s:SET_MINIGAME_IN_PROGRESS - didn't expect to have more than 2 minigames running at the same time", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		else
		{
			scriptAssertf(0, "%s:SET_MINIGAME_IN_PROGRESS(TRUE) - this command is being called more than once for the same script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else
	{
#if __DEV
		Displayf("SET_MINIGAME_IN_PROGRESS(FALSE) called by %s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

		CTheScripts::GetCurrentGtaScriptThread()->TidyUpMiniGameFlag();

		// Make sure to clear any old input data history.
		CControlMgr::GetMainPlayerControl().InitEmptyControls();
	}
}

bool CommandIsMinigameInProgress()
{
	return (CTheScripts::GetNumberOfMiniGamesInProgress() > 0);
}

bool CommandIsThisAMinigameScript()
{
	if (CTheScripts::GetCurrentGtaScriptThread() && CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CommandIsSniperInverted()
{
	//NOTE: This command is deprecated.
	return false;
}

bool CommandShouldUseMetricMeasurements()
{
	return CFrontendStatsMgr::ShouldUseMetric();
}

int CommandGetProfileSetting(s32 settingId)
{
	return CProfileSettings::GetInstance().GetInt( (CProfileSettings::ProfileSettingId) settingId);
}

float CommandGetCityDensity(void)
{
#if RSG_PC
	return CSettingsManager::GetInstance().GetSettings().m_graphics.m_CityDensity;
#else
	return 1.0f;
#endif 
}


bool CommandAreStringsEqual(const char *pStr1, const char *pStr2)
{
	if (!pStr1)
	{
		scriptAssertf(0, "%s:ARE_STRINGS_EQUAL - first string is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	if (!pStr2)
	{
		scriptAssertf(0, "%s:ARE_STRINGS_EQUAL - second string is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	if( !stricmp( pStr1, pStr2 ) )
	{
		return true;
	}
	return false;
}


int CommandCompareStrings(const char *pStr1, const char *pStr2, bool bCaseSensitive, int numberOfCharactersToCompare)
{
	if (numberOfCharactersToCompare <= 0)
	{
		if (bCaseSensitive)
		{
			return strcmp(pStr1, pStr2);
		}
	}
	else
	{
		if (bCaseSensitive)
		{
			return strncmp(pStr1, pStr2, numberOfCharactersToCompare);
		}
		else
		{
			return strnicmp(pStr1, pStr2, numberOfCharactersToCompare);
		}
	}

	return stricmp(pStr1, pStr2);
}


int AbsInt( int n )
{
	if( n < 0 ) { n = -n; }
	return n;
}

float AbsFloat( float fn )
{
	return rage::Abs( fn );
}

bool CommandIsSniperBulletInArea( const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors )
{
	Vector3 VecPosMin = Vector3 (scrVecMinCoors);
	Vector3 VecPosMax = Vector3 (scrVecMaxCoors);

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	bool LatestCmpFlagResult = FALSE;

	if (CBulletManager::ComputeIsBulletInBox(VecPosMin, VecPosMax))
	{
		LatestCmpFlagResult = TRUE;
	}

	return LatestCmpFlagResult;
}

bool CommandIsProjectileInArea( const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bIsPlayer )
{
	Vector3 VecPosMin = Vector3 (scrVecMinCoors);
	Vector3 VecPosMax = Vector3 (scrVecMaxCoors);

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	CPed* pPlayer = NULL;
	if (bIsPlayer)
	{
		pPlayer = CGameWorld::FindLocalPlayer();
		if(!SCRIPT_VERIFY(pPlayer, "IS_PROJECTILE_IN_AREA - Can't Find Player Ped"))
			return false;

		if(!SCRIPT_VERIFY(pPlayer->GetPlayerInfo(), "IS_PROJECTILE_IN_AREA - Can't Get Player Info"))
			return false;
	}

	bool LatestCmpFlagResult = FALSE;

	if (CProjectileManager::GetIsAnyProjectileInBounds(spdAABB(RCC_VEC3V(VecPosMin), RCC_VEC3V(VecPosMax)), 0, pPlayer))
	{
		LatestCmpFlagResult = TRUE;
	}

	return LatestCmpFlagResult;
}

bool CommandGetCoordsOfProjectileTypeInArea( const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, int WeaponType, Vector3 & returnPosition, bool bIsPlayer )
{
	Vector3 VecPosMin = Vector3 (scrVecMinCoors);
	Vector3 VecPosMax = Vector3 (scrVecMaxCoors);

	RefactorValuesForAreaCheck (VecPosMin, VecPosMax);

	bool LatestCmpFlagResult = FALSE;

	// Get the projectile type from the WeaponType
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
	if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
	{
		if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
			if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
			{
				if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
				{
					if(const CEntity * pProjectile = CProjectileManager::GetProjectileInBounds(spdAABB(RCC_VEC3V(VecPosMin), RCC_VEC3V(VecPosMax)), pAmmoInfo->GetHash()))
					{
						//by default, check this guy if we don't care about the owner
						bool bCheckThisProjectile = !bIsPlayer;

						if (bIsPlayer)
						{
							Assert(pProjectile->GetIsTypeObject());
							const CProjectile* pAsProjectile = (static_cast<const CObject*>(pProjectile))->GetAsProjectile();
							if (pAsProjectile && pAsProjectile->GetOwner() && pAsProjectile->GetOwner() == CGameWorld::FindLocalPlayer())
							{
								bCheckThisProjectile = true;
							}
						}

						if (bCheckThisProjectile)
						{
							returnPosition = VEC3V_TO_VECTOR3(pProjectile->GetTransform().GetPosition());
							LatestCmpFlagResult = TRUE;
						}
					}
				}
			}		
		}
	}

	return LatestCmpFlagResult;
}

bool CommandGetCoordsOfProjectileTypeInAngledArea( const scrVector & scrVecAngledAreaPoint1, const scrVector & scrVecAngledAreaPoint2, float DistanceOfOppositeFace, int WeaponType, Vector3 & returnPosition, bool bIsPlayer )
{
	bool LatestCmpFlagResult = FALSE;

	// Get the projectile type from the WeaponType
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
	if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
	{
		if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
			if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
			{
				if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
				{
					if(const CEntity * pProjectile = CProjectileManager::GetProjectileInAngledArea(Vector3(scrVecAngledAreaPoint1), Vector3(scrVecAngledAreaPoint2), DistanceOfOppositeFace, pAmmoInfo->GetHash()))
					{
						//by default, check this guy if we don't care about the owner
						bool bCheckThisProjectile = !bIsPlayer;

						if (bIsPlayer)
						{
							Assert(pProjectile->GetIsTypeObject());
							const CProjectile* pAsProjectile = (static_cast<const CObject*>(pProjectile))->GetAsProjectile();
							if (pAsProjectile && pAsProjectile->GetOwner() && pAsProjectile->GetOwner() == CGameWorld::FindLocalPlayer())
							{
								bCheckThisProjectile = true;
							}
						}

						if (bCheckThisProjectile)
						{
							returnPosition = VEC3V_TO_VECTOR3(pProjectile->GetTransform().GetPosition());
							LatestCmpFlagResult = TRUE;
						}
						
					}
				}
			}		
		}
	}

	return LatestCmpFlagResult;
}

bool CommandIsProjectileTypeInArea( const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, int WeaponType, bool bIsPlayer )
{
	Vector3 positionUnused;
	return CommandGetCoordsOfProjectileTypeInArea(scrVecMinCoors, scrVecMaxCoors, WeaponType, positionUnused, bIsPlayer);
}

bool CommandIsProjectileTypeInAngledArea( const scrVector & scrVecAngledAreaPoint1, const scrVector & scrVecAngledAreaPoint2, float DistanceOfOppositeFace, int WeaponType, bool bIsPlayer )
{
	Vector3 positionUnused;
	return CommandGetCoordsOfProjectileTypeInAngledArea( scrVecAngledAreaPoint1, scrVecAngledAreaPoint2, DistanceOfOppositeFace, WeaponType, positionUnused, bIsPlayer );
}

bool CommandIsProjectileTypeWithinDistance( const scrVector & scrVecCoors, int WeaponType, float distance, bool bIsPlayer)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	bool LatestCmpFlagResult = false;
	// Get the projectile type from the WeaponType
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
	if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
	{
		if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
			if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
			{
				if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
				{
					if(CEntity * pProjectile = CProjectileManager::GetProjectileWithinDistance( VecPos, pAmmoInfo->GetHash(), distance) )
					{
						//by default, check this guy if we don't care about the owner
						bool bCheckThisProjectile = !bIsPlayer;

						if (bIsPlayer)
						{
							Assert(pProjectile->GetIsTypeObject());
							const CProjectile* pAsProjectile = (static_cast<const CObject*>(pProjectile))->GetAsProjectile();
							if (pAsProjectile && pAsProjectile->GetOwner() && pAsProjectile->GetOwner() == CGameWorld::FindLocalPlayer())
							{
								bCheckThisProjectile = true;
							}
						}

						if (bCheckThisProjectile)
						{
							LatestCmpFlagResult = true;
						}
					}
				}
			}		
		}
	}
	return LatestCmpFlagResult;
}

bool CommandGetProjectileOfProjectileTypeWithinDistance( s32 pedIndex, int WeaponType, float distance, Vector3 & returnPosition, int& iProjectileEntity, bool needsToBeStationary )
{
	bool LatestCmpFlagResult = false;
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		// Get the projectile type from the WeaponType
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
		if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
		{
			if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
			{
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
				{
					if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
					{
						if(CEntity * pProjectile = CProjectileManager::GetNewestProjectileWithinDistance( pPed, pAmmoInfo->GetHash(), distance, needsToBeStationary ))
						{
#if DEBUG_DRAW
							TUNE_GROUP_BOOL(COMMANDS_MISC, bRenderGetProjectileOfProjectileTypeWithinDistance, false);
							if(bRenderGetProjectileOfProjectileTypeWithinDistance)
							{
								grcDebugDraw::Sphere(pProjectile->GetTransform().GetPosition(), 0.25f, Color_blue, true, -1);
							}
#endif
							iProjectileEntity = CTheScripts::GetGUIDFromEntity(*pProjectile);
							returnPosition = VEC3V_TO_VECTOR3(pProjectile->GetTransform().GetPosition());
							LatestCmpFlagResult = true;
							scriptDisplayf("GET_PROJECTILE_OF_PROJECTILE_TYPE_WITHIN_DISTANCE: Found projectile of type %s at location <<%.2f, %.2f, %.2f>> owned by ped %s", pAmmoInfo->GetName(), returnPosition.x, returnPosition.y, returnPosition.z, pPed->GetDebugName());
						}
						else
						{
							scriptDisplayf("GET_PROJECTILE_OF_PROJECTILE_TYPE_WITHIN_DISTANCE: Could not find a projectile of type %s owned by ped %s within the given radius %.2f", pAmmoInfo->GetName(), pPed->GetDebugName(), distance);
						}
					}
				}		
			}
		}
	}
	return LatestCmpFlagResult;
}

bool CommandGetCoordsOfProjectileTypeWithinDistance( s32 pedIndex, int WeaponType, float distance, Vector3 & returnPosition, bool needsToBeStationary )
{
	int iProjectileEntity;
	return CommandGetProjectileOfProjectileTypeWithinDistance(pedIndex, WeaponType, distance, returnPosition, iProjectileEntity, needsToBeStationary);
}

void LimitAngle( float fAngleIn, float &fAngleOut )
{
	fAngleOut = fAngleIn;

	while( fAngleOut < 0.0f )
	{
		fAngleOut += 360.0f;
	}

	while( fAngleOut > 360.0f )
	{
		fAngleOut -= 360.0f;
	}
}


int CommandGetUniqueMapVersion()
{
	return EXTRACONTENT.GetMapChangesCRC();
}

bool CommandIsPCVersion()
{
#if RSG_PC
	return true;
#else
	return false;
#endif
}

bool CommandIsPS3Version()
{
#if RSG_PS3
	return true;
#else
	return false;
#endif
}

bool CommandIsOrbisVersion()
{
#if RSG_ORBIS
	return true;
#elif RSG_PROSPERO && GTA_SCRIPT_LEGACY_PLATFORM_VERSION
	return true;
#else
	return false;
#endif
}

bool CommandIsProsperoVersion()
{
#if RSG_PROSPERO
	return true;
#else
	return false;
#endif
}

bool CommandIsScePlatform()
{
#if RSG_ORBIS
	return true;
#elif RSG_PROSPERO
	return true;
#else
	return false;
#endif
}

bool CommandIsXbox360Version()
{
#if RSG_XENON
	return true;
#else
	return false;
#endif
}

bool CommandIsDurangoVersion()
{
#if RSG_DURANGO
	return true;
#elif RSG_SCARLETT && GTA_SCRIPT_LEGACY_PLATFORM_VERSION
	return true;
#else
	return false;
#endif
}

bool CommandIsScarlettVersion()
{
#if RSG_SCARLETT
	return true;
#else
	return false;
#endif
}

bool CommandIsXboxPlatform()
{
#if RSG_SCARLETT
	return true;
#elif RSG_DURANGO
	return true;
#else
	return false;
#endif
}

bool CommandIsSteamVersion()
{
#if RSG_PC && __STEAM_BUILD
	return true;
#else
	return false;
#endif
}

bool CommandHasStorageDeviceBeenSelected()
{
	if (CSavegameUsers::GetSignedInUser())
	{
		if (CSavegameDevices::IsDeviceValid())
		{
			return true;
		}
	}

	return false;
}

bool CommandIsStringNull(const char *pStringToTest)
{
	if (pStringToTest == NULL)
	{
		return true;
	}

	return false;
}

bool CommandIsStringNullOrEmpty(const char *pStringToTest)
{
	if (pStringToTest == NULL)
	{
		return true;
	}

	if (pStringToTest[0] == 0)
	{
		return true;
	}

	return false;	
}

bool CommandStringToInt(const char *pStringToConvert, int &ReturnInteger)
{
	int Index = 0;
	ReturnInteger = -999;

	if (!pStringToConvert)
	{
		scriptAssertf(0, "%s:STRING_TO_INT - this command is being called with a NULL pointer", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	int string_length = istrlen(pStringToConvert);

	if (string_length == 0)
		return false;

	while (pStringToConvert[Index] != 0)
	{
		if ( (pStringToConvert[Index] < '0' || pStringToConvert[Index] > '9') )
		{
			if ( (pStringToConvert[Index] == '-') && (Index == 0) && (string_length > 1) )
			{

			}
			else
			{
				return false;
			}
		}

		Index++;
	}

	ReturnInteger = atoi(pStringToConvert);
	return true;
}


// DP - i may move this once i've re-written the Hud code
void CommandSetPhoneHudItem(int iType, const char *pTextLabel, int iNumber)
{
	CScriptHud::RadarMessage.bActivate = true;  // set item to activate

	CScriptHud::RadarMessage.iType = iType;

	if (iType == 0)
	{
		CScriptHud::RadarMessage.iNumber = 0;
		CScriptHud::RadarMessage.cMessage[0] = '\0';
	}
	else
	{
		CScriptHud::RadarMessage.iNumber = iNumber;
		safecpy(CScriptHud::RadarMessage.cMessage, pTextLabel, TEXT_KEY_SIZE);
	}
}

void CommandSetOverrideNoSprintingOnPhoneInMultiplayer(const bool b)
{
	CTaskMovePlayer::ms_bScriptOverrideNoRunningOnPhone = b;
}

void CommandSetMessagesWaiting(bool bWaiting)
{
	CScriptHud::RadarMessage.bMessagesWaiting = bWaiting;
}

void CommandSetSleepModeActive(bool bActive)
{
	CScriptHud::RadarMessage.bSleepModeActive = bActive;
}


void CommandSetBitsInRange(int &VariableToChange, int StartBit, int EndBit, int NewValue)
{
	if(SCRIPT_VERIFY( NewValue >= 0, "SET_BITS_IN_RANGE - NewValue must be >= 0"))
	{
		if(SCRIPT_VERIFY( (StartBit <= EndBit), "SET_BITS_IN_RANGE - StartBit should be less than or equal to EndBit"))
		{
			if(SCRIPT_VERIFY( (StartBit >=0) && (StartBit <= 31), "SET_BITS_IN_RANGE - Start Bit must be between 0 to 31"))
			{
				if(SCRIPT_VERIFY( (EndBit >=0) && (EndBit <= 31), "SET_BITS_IN_RANGE - End Bit must be between 0 to 31"))
				{
					int BitRange = (EndBit - StartBit) + 1;
					int MaxValueThatCanBeStored = (1 << BitRange);
					MaxValueThatCanBeStored -= 1;

				#if __ASSERT
					char ErrorString[256];

					if (NewValue > MaxValueThatCanBeStored)
					{
						sprintf(ErrorString, "SET_BITS_IN_RANGE - Number is too large to store in this number of bits - Start = %d, End = %d, Value = %d", StartBit, EndBit, NewValue);
						scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ErrorString);
					}
				#endif

				//	int Mask = 0x0ffffffff;
					int Temp = (MaxValueThatCanBeStored << StartBit);

				//	clear bits between StartBit and EndBit
					VariableToChange &= ~Temp;
				//	Mask &= ~Temp;

				//	set bits 
					Temp = (NewValue << StartBit);
				//	Mask |= (NewValue << StartBit);
					VariableToChange |= Temp;
				}
			}
		}
	}
}

int CommandGetBitsInRange(int VariableToRead, int StartBit, int EndBit)
{
	if(SCRIPT_VERIFY((StartBit <= EndBit), "GET_BITS_IN_RANGE - StartBit should be less than or equal to EndBit"))
	{
		if(SCRIPT_VERIFY((StartBit >=0) && (StartBit <= 31), "GET_BITS_IN_RANGE - Start Bit must be between 0 to 31"))
		{
			if(SCRIPT_VERIFY((EndBit >=0) && (EndBit <= 31), "GET_BITS_IN_RANGE - End Bit must be between 0 to 31"))
			{
				int BitRange = (EndBit - StartBit) + 1;
				int MaxValueThatCanBeStored = (1 << BitRange);
				MaxValueThatCanBeStored -= 1;

				int Temp = (VariableToRead >> StartBit);
				Temp &= MaxValueThatCanBeStored;

				return Temp;
			}
		}
	}
	return 0;
}

int CommandAddStuntJump(const scrVector & startMin, const scrVector & startMax, const scrVector & endMin, const scrVector & endMax, const scrVector & cameraPoint, int reward, int level, bool camOptional)
{
	int retVal = 0;

	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "ADD_STUNT_JUMP is called before the stunt jump manager is initialized."))
	{
		Vec3V vStartMin = Vec3V(startMin);
		Vec3V vStartMax = Vec3V(startMax);
		Vec3V vEndMin = Vec3V(endMin);
		Vec3V vEndMax = Vec3V(endMax);
		Vec3V vCameraPoint = Vec3V(cameraPoint);

		retVal = SStuntJumpManager::GetInstance().AddOne( vStartMin, vStartMax, vEndMin, vEndMax, vCameraPoint, reward, level, camOptional);
	}

	return retVal;
}

int CommandAddStuntJumpAngled(const scrVector & startMin, const scrVector & startMax, float startWidth, const scrVector & endMin, const scrVector & endMax, float endWidth, const scrVector & cameraPoint, int reward, int level, bool camOptional)
{
	int retVal = 0;

	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "ADD_STUNT_JUMP_ANGLED is called before the stunt jump manager is initialized."))
	{
		Vec3V vStartMin = Vec3V(startMin);
		Vec3V vStartMax = Vec3V(startMax);
		Vec3V vEndMin = Vec3V(endMin);
		Vec3V vEndMax = Vec3V(endMax);
		Vec3V vCameraPoint = Vec3V(cameraPoint);

		retVal = SStuntJumpManager::GetInstance().AddOneAngled( vStartMin, vStartMax, startWidth, vEndMin, vEndMax, endWidth, vCameraPoint, reward, level, camOptional);
	}

	return retVal;
}

void CommandToggleShowOptionalStuntJumpCamera(bool bShow)
{
	SStuntJumpManager::GetInstance().ToggleShowOptionalStuntCameras(bShow);
}


void CommandDeleteStuntJump(int id)
{
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "DELETE_STUNT_JUMP is called before the stunt jump manager is initialized."))
	{
		SStuntJumpManager::GetInstance().DeleteOne(id);
	}
}

void CommandEnableStuntJumpSet(int id)
{
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "ENABLE_STUNT_JUMP_SET is called before the stunt jump manager is initialized."))
	{
		SStuntJumpManager::GetInstance().EnableSet(id);
	}
}

void CommandDisableStuntJumpSet(int id)
{
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "DISABLE_STUNT_JUMP_SET is called before the stunt jump manager is initialized."))
	{
		SStuntJumpManager::GetInstance().DisableSet(id);
	}
}

void CommandAllowStuntJumpsToTrigger(bool bAllow)
{
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "SET_STUNT_JUMPS_CAN_TRIGGER is called before the stunt jump manager is initialized."))
	{
		SStuntJumpManager::GetInstance().SetActive(bAllow);
	}
}

bool CommandIsStuntJumpInProgress()
{
	bool retVal = false;
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "IS_STUNT_JUMP_IN_PROGRESS is called before the stunt jump manager is initialized."))
	{
		retVal = SStuntJumpManager::GetInstance().IsAStuntjumpInProgress();
	}

	return retVal;
}


bool CommandIsStuntJumpMessageShowing()
{
	bool retVal = false;
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "IS_STUNT_JUMP_MESSAGE_SHOWING is called before the stunt jump manager is initialized."))
	{
		retVal = SStuntJumpManager::GetInstance().IsStuntjumpMessageShowing();
	}

	return retVal;
}


int CommandGetNumSuccessfulStuntJumps()
{
	int retVal = 0;
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "GET_NUM_SUCCESSFUL_STUNT_JUMPS is called before the stunt jump manager is initialized."))
	{
		retVal = SStuntJumpManager::GetInstance().GetStuntJumpCompletedStat();
	}

	return retVal;
}

int CommandGetTotalSuccessfulStuntJumps()
{
	int retVal = 0;
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "GET_TOTAL_SUCCESSFUL_STUNT_JUMPS is called before the stunt jump manager is initialized."))
	{
		retVal = SStuntJumpManager::GetInstance().GetTotalStuntJumpCompletedStat();
	}

	return retVal;
}

void CommandCancelStuntJump()
{
	if(scriptVerifyf(SStuntJumpManager::IsInstantiated(), "CANCLE_STUNT_JUMP is called before the stunt jump manager is initialized."))
	{
		SStuntJumpManager::GetInstance().AbortStuntJumpInProgress();
	}
}

void CommandSetGamePaused (bool bActive)
{
	if (bActive)
	{
		fwTimer::StartScriptPause();
		GtaThread::PauseAllThreadsBarThisOne(CTheScripts::GetCurrentGtaScriptThread());
	}
	else
	{
		fwTimer::EndScriptPause();
		GtaThread::UnpauseAllThreads();
	}

}


void CommandAllowThisScriptToBePaused(bool bScriptCanBePaused)
{
	CTheScripts::GetCurrentGtaScriptThread()->bThisScriptCanBePaused = bScriptCanBePaused;
}

void CommandSetThisScriptCanRemoveBlipsCreatedByAnyScript(bool bCanRemoveBlipsCreatedByOtherScripts)
{
	scriptDisplayf("%s:SET_THIS_SCRIPT_CAN_REMOVE_BLIPS_CREATED_BY_ANY_SCRIPT %s called", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bCanRemoveBlipsCreatedByOtherScripts?"TRUE":"FALSE");
	CTheScripts::GetCurrentGtaScriptThread()->bThisScriptCanRemoveBlipsCreatedByAnyScript = bCanRemoveBlipsCreatedByOtherScripts;
}

void CommandClearNewsScrollbar()
{
	g_scrollbars.ClearNews();
}

void CommandAddStringToNewsScrollbar(const char *pName)
{
	g_scrollbars.AddNews(pName);
}

void CommandActivateCheat(int cheat)
{
	CCheat::ScriptActivatedCheat(cheat);
}

bool CommandHasCheatBeenActivated(const char* cheatCode)
{
	return CCheat::HasCheatActivated(cheatCode);
}

bool CommandHasCheatWithHashBeenActivated(s32 hashOfCheatString, s32 lengthOfCheatString)
{
	return CCheat::HasCheatActivated( (u32) hashOfCheatString, (u32) lengthOfCheatString);
}

bool CommandHasPcCheatWithHashBeeonActivated(s32 WIN32PC_ONLY(hashOfCheatString))
{
#if RSG_PC
	return CCheat::HasPcCheatActivated( (u32) hashOfCheatString );
#else
	return false;
#endif // RSG_PC
}

// Force freezing of all exterior physics
void CommandOverrideFreezeFlags(bool bOverride)
{
	CGameWorld::OverrideFreezeFlags(bOverride);
}

void CommandSetGlobalInstancePriority(int )
{
	//DEPRECATED
}

void CommandSetDefaultGlobalInstancePriority()
{
	//DEPRECATED
}

void CommandEnableXboxScreenSaver(bool XENON_ONLY(bEnable))
{
#if __XENON
	XEnableScreenSaver(bEnable);
#endif
}

bool CommandIsFrontendFading()
{
	return false;
}

void CommandPopulateNow()
{
 	CVehiclePopulation::ForcePopulationInit();
 	CPortalTracker::TriggerForceAllActiveInteriorCapture();
}

int CommandGetIndexOfCurrentLevel()
{
	if (CTheScripts::GetProcessingTheScriptsDuringGameInit())
	{
		return CGameLogic::GetRequestedLevelIndex();
	}
	else
	{
		return CGameLogic::GetCurrentLevelIndex();
	}
}

void CommandSwitchToLevel(int level)
{
	if (SCRIPT_VERIFY(level >= CGameLogic::DEFAULT_LEVEL_INDEX && level <= CGameLogic::MAX_LEVEL_INDEX, "SWITCH_TO_LEVEL - Level Index should be between 1 and 5 inclusive"))
	{
        CGame::ChangeLevel(level);
	}
}

//	REGISTER_PERSISTENT_GLOBAL_VARIABLES(Ss, SIZE_OF(Ss))
/*
void CommandRegisterPersistentGlobalVariables(int &AddressTemp, int Size)
{
	int *Address = &AddressTemp;

	if (SCRIPT_VERIFY(Size > 0, "REGISTER_PERSISTENT_GLOBAL_VARIABLES - Size of structure should be greater than 0"))
	{
		Size *= sizeof(scrValue);			// Script returns size in script words (4 bytes)
		CTheScripts::PersistentScriptGlobals.RegisterPersistentGlobalVariables(Address, Size);
	}
}
*/

void CommandSetGravityLevel(int iGravityLevel)
{
	if(SCRIPT_VERIFY(iGravityLevel >= 0 && iGravityLevel < CPhysics::NUM_GRAVITY_LEVELS, "Invalid gravity level"))
	{
		CPhysics::SetGravityLevel((CPhysics::eGravityLevel)iGravityLevel);
	}
}

int CommandGetCurrentStackSize()
{
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		return CTheScripts::GetCurrentGtaScriptThread()->GetThreadSP();
	}
	else
	{
		Assertf(0, "GET_CURRENT_STACK_SIZE - Failed to get a pointer to the current script thread");
		return 0;
	}
}

int GetAllocatedStackSize()
{
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		return CTheScripts::GetCurrentGtaScriptThread()->GetAllocatedStackSize();
	}
	else
	{
		Assertf(0, "GET_ALLOCATED_STACK_SIZE - Failed to get a pointer to the current script thread");
		return 0;
	}
}

int CommandGetNumberOfFreeStacksOfThisSize(int stackSize)
{
	return scrThread::CheckStackAvailability(stackSize);
}

// Script returns size in script words (4 bytes)
void CommandStartSaveData(s32 &StartOfStruct, s32 NumberOfScrValuesInStruct, bool bSinglePlayer)
{
	eTypeOfSavegame typeOfSavegame = SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA;
	if (bSinglePlayer)
	{
		typeOfSavegame = SAVEGAME_SINGLE_PLAYER;
	}

	CScriptSaveData::OpenScriptSaveData(typeOfSavegame, &StartOfStruct, NumberOfScrValuesInStruct, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandStopSaveData()
{
//	search the array of parTrees for the name of this rtstruct and if the tree exists, load the data from it
	CScriptSaveData::CloseScriptStruct(CTheScripts::GetCurrentGtaScriptThread());
}

s32 CommandGetSizeOfSaveData(bool bSinglePlayer)
{
	eTypeOfSavegame typeOfSavegame = SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA;
	if (bSinglePlayer)
	{
		typeOfSavegame = SAVEGAME_SINGLE_PLAYER;
	}

	return CScriptSaveData::GetSizeOfSavedScriptData(typeOfSavegame, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterIntToSave(int &IntToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_Int, &IntToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterInt64ToSave(int &IntToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_Int64, &IntToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterEnumToSave(int &EnumToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_Int, &EnumToSave, CTheScripts::GetCurrentGtaScriptThread());
}


void CommandRegisterFloatToSave(float &FloatToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_Float, &FloatToSave, CTheScripts::GetCurrentGtaScriptThread());
}

//	David Etherton in Rage Changelist 230215, wrote - 
//	"If you have a native function that accepts bool&, just change the C++ side to be int& and all will be well."
void CommandRegisterBoolToSave(int &BoolToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_Int, &BoolToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabelToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel15, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel3ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel3, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel7ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel7, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel15ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel15, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel23ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel23, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel31ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel31, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandRegisterTextLabel63ToSave(const char *pTextLabelToSave, const char *pLabel)
{
	CScriptSaveData::AddMemberToCurrentStruct(pLabel, Script_Save_Data_Type_TextLabel63, pTextLabelToSave, CTheScripts::GetCurrentGtaScriptThread());
}


void CommandStartSaveStructWithSize(int &StartOfStruct, s32 NumberOfScrValuesInStruct, const char *pNameOfStructInstance)
{
	CScriptSaveData::OpenScriptStruct(pNameOfStructInstance, &StartOfStruct, NumberOfScrValuesInStruct, false, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandStopSaveStruct()
{
	CScriptSaveData::CloseScriptStruct(CTheScripts::GetCurrentGtaScriptThread());
}

void CommandStartSaveArrayWithSize(int &StartOfArray, s32 NumberOfScrValuesInStruct, const char *pNameOfArrayInstance)
{
	CScriptSaveData::OpenScriptStruct(pNameOfArrayInstance, &StartOfArray, NumberOfScrValuesInStruct, true, CTheScripts::GetCurrentGtaScriptThread());
}

void CommandStopSaveArray()
{
	CScriptSaveData::CloseScriptStruct(CTheScripts::GetCurrentGtaScriptThread());
}

// bool CommandGetPedLastWeaponImpactCoord (int PedIndex), const scrVector & &ImpactCoord)
// {
// 	bool bValidImpact = false;
// 
// 
// 
// 	return bValidImpact;
// }


void CommandCopyScriptStruct(int &DestStruct, int &SourceStruct, int Size)
{
	void *scrDestStruct = reinterpret_cast<void *>(&DestStruct);
	const void *scrSourceStruct = reinterpret_cast<const void *>(&SourceStruct);

	// convert to bytes
	Size *= sizeof(scrValue);

	sysMemCpy(scrDestStruct, scrSourceStruct, Size);
}


void CommandEnableDispatchService(const int dispatchEnum, const bool bEnable)
{
	if( aiVerifyf(dispatchEnum >= 0 && dispatchEnum < DT_Max, "ENABLE_DISPATCH_SERVICE: dispatch enum out of range!") )
	{
		CDispatchManager::GetInstance().EnableDispatch(static_cast<DispatchType>(dispatchEnum), bEnable);
	}
}

void CommandBlockDispatchServiceResourceCreation(const int dispatchEnum, const bool bBlock)
{
	if( aiVerifyf(dispatchEnum >= 0 && dispatchEnum < DT_Max, "BLOCK_DISPATCH_SERVICE_RESOURCE_CREATION: dispatch enum out of range!") )
	{
		CDispatchManager::GetInstance().BlockDispatchResourceCreation(static_cast<DispatchType>(dispatchEnum), bBlock);
	}
}

int CommandGetNumberResourcesAllocatedToWantedLevel(const int dispatchEnum)
{
	if( aiVerifyf(dispatchEnum >= 0 && dispatchEnum < DT_Max, "GET_NUMBER_RESOURCES_ALLOCATED_TO_WANTED_LEVEL: dispatch enum out of range!") )
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();
		if( pPed && pPed->GetPlayerWanted() )
		{
			CIncident* pWantedIncident = pPed->GetPlayerWanted()->m_wantedLevelIncident;
			if( pWantedIncident )
			{
				return pWantedIncident->GetAssignedResources(dispatchEnum);
			}
		}
	}
	return 0;
}

bool AddScriptIncident(CScriptIncident& incident, int& incidentIndex)
{
	int incidentSlot = -1;
	if (CIncidentManager::GetInstance().AddIncident(incident, true, &incidentSlot))
	{
		CIncident* pNewIncident = CIncidentManager::GetInstance().GetIncident(incidentSlot);

		if (aiVerify(pNewIncident))
		{
			incidentIndex = CIncident::GetPool()->GetIndex(pNewIncident);
			return true;
		}
	}

	incidentIndex = NULL_IN_SCRIPTING_LANGUAGE;
	return false;
}

bool CommandCreateIncident(const int dispatchEnum, const scrVector & svLocation, const s32 iNumUnits, float fTime, int& incidentIndex, s32 iOverrideRelGroupHash, const int assassinsLevel)
{
	incidentIndex = NULL_IN_SCRIPTING_LANGUAGE;

	Vector3 vLocation(svLocation);
	bool bAddIncident = true;

	// Don't add incidents if the firetruck or ambulance won't be able to reach it
	if(dispatchEnum == DT_FireDepartment || dispatchEnum == DT_AmbulanceDepartment)
	{
		float fDistanceToOnNode = (CMapAreas::GetAreaIndexFromPosition(vLocation) == 0 ) ? 90.0f : 40.0f;

		// don't add incidents if the firetruck/ambulance won't be able to reach it
		CNodeAddress CurrentNode;
		CurrentNode = ThePaths.FindNodeClosestToCoors(vLocation, fDistanceToOnNode,  CPathFind::IgnoreSwitchedOffNodes);

		if (CurrentNode.IsEmpty())
		{
			bAddIncident = false;
		}
	}

	// Update the spawn timers for fire and ambulance
	if(bAddIncident)
	{
		if(dispatchEnum == DT_FireDepartment)
		{
			CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
			if(pFireDispatchService)
			{
				if(!pFireDispatchService->GetTimerTicking())
				{
					pFireDispatchService->ResetSpawnTimer();
				}
			}
		}

		if(dispatchEnum == DT_AmbulanceDepartment)
		{
			CDispatchService* pAmbulanceDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_AmbulanceDepartment);
			if(pAmbulanceDispatchService)
			{
				if(!pAmbulanceDispatchService->GetTimerTicking())
				{
					pAmbulanceDispatchService->ResetSpawnTimer();
				}
			}
		}
	}
		

	if( bAddIncident && aiVerifyf(iNumUnits > 0, "CREATE_INCIDENT: Must request at least 1 unit.") &&
		aiVerifyf(dispatchEnum >= 0 && dispatchEnum < DT_Max, "CREATE_INCIDENT: dispatch enum out of range!") )
	{
		CScriptIncident scriptIncident(NULL, vLocation, fTime);
		if(dispatchEnum == DT_AmbulanceDepartment || dispatchEnum == DT_FireDepartment)
		{
			scriptIncident.SetUseExistingPedsOnFoot(false);
		}

		scriptIncident.SetOverrideRelationshipGroupHash((u32)iOverrideRelGroupHash);

		if (assassinsLevel != AL_Invalid && scriptVerifyf(assassinsLevel > AL_Invalid && assassinsLevel < AL_Max, "CREATE_INCIDENT: Passed in assassinsLevel (%i) is not supported! Max allowed is %i", assassinsLevel, AL_Max))
		{
			if (scriptVerifyf(dispatchEnum == DT_Assassins,"CREATE_INCIDENT: assassin level can't be used with %i dispatch type (it should only be used with DT_ASSASSINS)",dispatchEnum))
			{
				scriptIncident.SetAssassinDispatchLevel(assassinsLevel);
			}
		}

		scriptIncident.SetRequestedResources(dispatchEnum, iNumUnits);
		return AddScriptIncident(scriptIncident, incidentIndex);

	}
	return false;
}

bool CommandCreateIncidentWithEntity(const int dispatchEnum, int EntityIndex, const s32 iNumUnits, float fTime, int& incidentIndex, s32 iOverrideRelGroupHash, const int assassinsLevel)
{
	incidentIndex = NULL_IN_SCRIPTING_LANGUAGE;

	if( aiVerifyf(iNumUnits > 0, "CREATE_INCIDENT_WITH_ENTITY: Must request at least 1 unit.") &&
		aiVerifyf(dispatchEnum >= 0 && dispatchEnum < DT_Max, "CREATE_INCIDENT_WITH_ENTITY: dispatch enum out of range!") )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{
			Vector3 vLocation(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
			CScriptIncident scriptIncident(pEntity, vLocation, fTime);
			if(dispatchEnum == DT_AmbulanceDepartment || dispatchEnum == DT_FireDepartment)
			{
				scriptIncident.SetUseExistingPedsOnFoot(false);
			}

			scriptIncident.SetOverrideRelationshipGroupHash((u32)iOverrideRelGroupHash);

			if (assassinsLevel != AL_Invalid && scriptVerifyf(assassinsLevel > AL_Invalid && assassinsLevel < AL_Max, "CREATE_INCIDENT_WITH_ENTITY: Passed in assassinsLevel (%i) is not supported! Max allowed is %i", assassinsLevel, AL_Max))
			{
				if (scriptVerifyf(dispatchEnum == DT_Assassins,"CREATE_INCIDENT_WITH_ENTITY: assassin level can't be used with %i dispatch type (it should only be used with DT_ASSASSINS)",dispatchEnum))
				{
					scriptIncident.SetAssassinDispatchLevel(assassinsLevel);
				}
			}

			scriptIncident.SetRequestedResources(dispatchEnum, iNumUnits);
			return AddScriptIncident(scriptIncident, incidentIndex);
		}
	}
	return false;
}

void CommandSetIncidentRequestedUnits(int incidentIndex, const int dispatchEnum, const s32 iNumUnits)
{
	CIncident* pIncident = CIncident::GetPool()->GetAt(incidentIndex);
	if( aiVerifyf(pIncident, "SET_INCIDENT_REQUESTED_UNITS: Incident doesn't exist!") && 
		aiVerifyf(pIncident->GetType() == CIncident::IT_Scripted, "Incident of incorrect type!") )
	{
		CScriptIncident* pScriptedIncident = static_cast<CScriptIncident*>(pIncident);
		pScriptedIncident->SetRequestedResources(dispatchEnum, iNumUnits);
	}
}

void CommandSetIdealSpawnDistanceForIncident(int incidentIndex, float distance)
{
	CIncident* pIncident = CIncident::GetPool()->GetAt(incidentIndex);
	if( aiVerifyf(pIncident, "SET_IDEAL_SPAWN_DISTANCE_FOR_INCIDENT: Incident doesn't exist!") && 
		aiVerifyf(pIncident->GetType() == CIncident::IT_Scripted, "Incident of incorrect type!") )
	{
		CScriptIncident* pScriptedIncident = static_cast<CScriptIncident*>(pIncident);
		pScriptedIncident->SetIdealSpawnDistance(distance);
	}
}

void CommandDeleteIncident(int incidentIndex)
{
	CIncident* pIncident = CIncident::GetPool()->GetAt(incidentIndex);
	if( pIncident )
	{
		CIncidentManager::GetInstance().ClearIncident(pIncident);
	}
}

bool CommandIsIncidentValid(int incidentIndex)
{
	CIncident* pIncident = CIncident::GetPool()->GetAt(incidentIndex);
	if( pIncident )
	{
		return true;
	}
	return false;
}

bool CommandFindSpawnPointInDirection(const scrVector & vPosition, const scrVector &vDirection, float fIdealSpawnDistance, Vector3& vSpawnPoint)
{
	return (CDispatchSpawnHelper::FindSpawnPointInDirection((Vec3V)vPosition, (Vec3V)vDirection, fIdealSpawnDistance,
		CDispatchSpawnHelper::FSPIDF_CanFollowOutgoingLinks, RC_VEC3V(vSpawnPoint)));
}

void AddPopMultiplierAreaOverNetwork(u32 index)
{
	// Add over network....is a network game in progress and are we the script host?
	if(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork())
	{
		PopMultiplierArea const*const area = CThePopMultiplierAreas::GetActiveArea(index);
		if (!area)
			return;

#if !__FINAL
        if(area->m_isSphere)
        {
            scriptDebugf1("%s: Adding population multiplier area sphere over network: Centre: (%.2f, %.2f, %.2f) Radius:%.2f, PD:%.2f, VD:%.2f, CAM:%s",
                CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
                area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
                area->m_maxWS.x,
                area->m_pedDensityMultiplier,
                area->m_trafficDensityMultiplier,
			    area->m_bCameraGlobalMultiplier ? "(cam-global mult)" : "(zone-local mult)");
        }
        else
        {
            scriptDebugf1("%s: Adding population multiplier area box over network: Box Min (%.2f, %.2f, %.2f)-> Box Max (%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, CAM:%s",
                CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
                area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
                area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
                area->m_pedDensityMultiplier,
                area->m_trafficDensityMultiplier,
			    area->m_bCameraGlobalMultiplier ? "(cam-global mult)" : "(zone-local mult)");
        }
        scrThread::PrePrintStackTrace();
#endif // !__FINAL

		NetworkInterface::AddPopMultiplierAreaOverNetwork
		(
			CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
			area->m_minWS,
			area->m_maxWS,
			area->m_pedDensityMultiplier,
			area->m_trafficDensityMultiplier,
			area->m_isSphere,
			area->m_bCameraGlobalMultiplier
		);
	}
}

u32 CommandAddPopMultiplierArea(const scrVector & _minWS, const scrVector & _maxWS, float _pedDensityMultiplier, float _trafficDensityMultiplier, bool localOnly, bool bCameraGlobalMultiplier)
{
	u32 index = CThePopMultiplierAreas::CreatePopMultiplierArea(_minWS, _maxWS, _pedDensityMultiplier, _trafficDensityMultiplier, bCameraGlobalMultiplier);

	if(!localOnly)
	{
		scriptDebugf1("ADD_POP_MULTIPLIER_AREA (CommandAddPopMultiplierArea) was called with localOnly set to false. Forwarding to AddPopMultiplierAreaOverNetwork");
		AddPopMultiplierAreaOverNetwork(index);
	}
	else
	{
		scriptDebugf1("%s: Adding population multiplier area: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, Local Only: %s, CAM: %s",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
			_minWS.x, _minWS.y, _minWS.z,
			_maxWS.x, _maxWS.y, _maxWS.z,
			_pedDensityMultiplier,
			_trafficDensityMultiplier,
			localOnly ? "True" : "False",
			bCameraGlobalMultiplier ? "(cam-global mult)" : "(zone-local mult)");
	}

	return index;
}

u32 CommandAddPopMultiplierSphere(const scrVector & _center, float _radius, float _pedDensityMultiplier, float _trafficDensityMultiplier, bool localOnly, bool bCameraGlobalMultiplier)
{
	u32 index = CThePopMultiplierAreas::CreatePopMultiplierArea(_center, _radius, _pedDensityMultiplier, _trafficDensityMultiplier, bCameraGlobalMultiplier);

	if(!localOnly)
	{
		scriptDebugf1("ADD_POP_MULTIPLIER_SPHERE (CommandAddPopMultiplierArea) was called with localOnly set to false. Forwarding to AddPopMultiplierAreaOverNetwork");
		AddPopMultiplierAreaOverNetwork(index);
	}
	else
	{
		scriptDebugf1("%s: Adding sphere population multiplier area: Center: (%.2f, %.2f, %.2f) with radius of %.2f, PD:%.2f, VD:%.2f, Local Only: %s, CAM: %s",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
			_center.x, _center.y, _center.z,
			_radius,
			_pedDensityMultiplier,
			_trafficDensityMultiplier,
			localOnly ? "True" : "False",
			bCameraGlobalMultiplier ? "(cam-global mult)" : "(zone-local mult)");
	}

	return index;
}

bool CommandDoesPopMultiplierAreaExist(int index)
{
	if (scriptVerifyf( (index >= 0) && (index < MAX_POP_MULTIPLIER_AREAS), "%s : invalid index %d - must be >= 0 and less than MAX_POP_MULTIPLIER_AREAS (%d)", __FUNCTION__, index, MAX_POP_MULTIPLIER_AREAS))
	{
		PopMultiplierArea const * const area = CThePopMultiplierAreas::GetActiveArea(index);

		if(area && area->m_init)
		{
			return true;
		}
	}

	return false;
}

bool CommandDoesPopMultiplierSphereExist(int sphereIndex)
{
	return CommandDoesPopMultiplierAreaExist(sphereIndex);
}

void RemovePopMultiplierAreaOverNetwork(u32 index)
{
	if(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork())
	{
		PopMultiplierArea const*const area = CThePopMultiplierAreas::GetActiveArea(index);
		if (!area)
			return;

#if !__FINAL
        scriptDebugf1("%s: Removing population multiplier area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f%s",
            CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
            area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
            area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
            area->m_pedDensityMultiplier,
            area->m_trafficDensityMultiplier,
            area->m_isSphere ? " (Sphere)" : "");
        scrThread::PrePrintStackTrace();
#endif // !__FINAL

		NetworkInterface::RemovePopMultiplierAreaOverNetwork
        (
            CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
            area->m_minWS,
            area->m_maxWS,
			area->m_pedDensityMultiplier,
			area->m_trafficDensityMultiplier,
            area->m_isSphere,
			area->m_bCameraGlobalMultiplier
        );
	}
}

void CommandRemovePopDensityMultiplierArea(int _areaIndex, bool localOnly)
{
	Assertf(_areaIndex < MAX_POP_MULTIPLIER_AREAS, "%s : invalid index %d - must be less than MAX_POP_MULTIPLIER_AREAS (%d) : areaIndex = %d : localOnly = %d", __FUNCTION__, _areaIndex, MAX_POP_MULTIPLIER_AREAS, _areaIndex, localOnly);	

	PopMultiplierArea const * const area = CThePopMultiplierAreas::GetActiveArea(_areaIndex);

	if(!area || !area->m_init)
	{
		Assertf(false, "%s : No area registered for index %d - either not registered or already freed up? areaIndex = %d : localOnly = %d", __FUNCTION__, _areaIndex, _areaIndex, localOnly);	

		return;
	}

	if(CTheScripts::GetCurrentGtaScriptHandler())
	{
		if(!localOnly)
		{
			if(NetworkInterface::FindPopMultiplierAreaOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->IsSphere(), area->m_bCameraGlobalMultiplier))
			{
				RemovePopMultiplierAreaOverNetwork(_areaIndex);

				CThePopMultiplierAreas::RemovePopMultiplierArea(area->m_minWS, area->m_maxWS, area->m_isSphere, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->m_bCameraGlobalMultiplier);

				return;
			}
			else
			{
				scriptDebugf1("CommandRemovePopDensityMultiplierArea Failed to FindPopMultiplierAreaOverNetwork! (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, CGM:%s",
                    area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
                    area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
                    area->m_pedDensityMultiplier,
                    area->m_trafficDensityMultiplier,
                    area->m_bCameraGlobalMultiplier ? "TRUE" : "FALSE");
			}
		}
		else
		{
			if(!NetworkInterface::FindPopMultiplierAreaOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->IsSphere(), area->m_bCameraGlobalMultiplier))
			{
				scriptDebugf1("%s: Removing population multiplier area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, CGM:%s",
					CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
					area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
					area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
					area->m_pedDensityMultiplier,
					area->m_trafficDensityMultiplier,
                    area->m_bCameraGlobalMultiplier ? "TRUE" : "FALSE");

				CThePopMultiplierAreas::RemovePopMultiplierArea(area->m_minWS, area->m_maxWS, area->m_isSphere, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->m_bCameraGlobalMultiplier);

				return;
			}
			else
			{
				scriptDebugf1("CommandRemovePopDensityMultiplierArea Attempting to locally delete a multiplayer pop area! (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, CGM:%s",
                    area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
                    area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
                    area->m_pedDensityMultiplier,
                    area->m_trafficDensityMultiplier,
                    area->m_bCameraGlobalMultiplier ? "TRUE" : "FALSE");
			}
		}
	}

	Assertf(false, "ERROR : CommandRemovePopDensityMultiplierArea : Script handler == %p _areaIndex %d: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), PD:%.2f, VD:%.2f, CGM:%s",
        CTheScripts::GetCurrentGtaScriptHandler(),
        _areaIndex,
        area->m_minWS.x, area->m_minWS.y, area->m_minWS.z,
        area->m_maxWS.x, area->m_maxWS.y, area->m_maxWS.z,
        area->m_pedDensityMultiplier,
        area->m_trafficDensityMultiplier,
        area->m_bCameraGlobalMultiplier ? "TRUE" : "FALSE");
}

bool CommandIsPopDensityMultiplierAreaNetworked(int _areaIndex)
{
	PopMultiplierArea const * const area = CThePopMultiplierAreas::GetActiveArea(_areaIndex);

	if(!area || !area->m_init)
	{
		scriptDebugf1("%s : No area registered for index %d - either not registered or already freed up? areaIndex = %d", __FUNCTION__, _areaIndex, _areaIndex);	
		return false;
	}

	if(CTheScripts::GetCurrentGtaScriptHandler())
	{
		return NetworkInterface::FindPopMultiplierAreaOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), area->m_minWS, area->m_maxWS, area->m_pedDensityMultiplier, area->m_trafficDensityMultiplier, area->IsSphere(), area->m_bCameraGlobalMultiplier);
	}	
	return false;
}

void CommandRemovePopDensityMultiplierSphere(int sphereIndex, bool localOnly)
{
	CommandRemovePopDensityMultiplierArea(sphereIndex, localOnly);
}

void CommandEnableTennisMode(s32 pedIndex, bool enable, bool bUseFemaleClipSet) 
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "ENABLE_TENNIS_MODE - Check ped is alive this frame");
		if (enable)
		{
			CTaskMotionPed* pCurrentMotionTask = static_cast<CTaskMotionPed*>(pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED));
			if(pCurrentMotionTask)
			{
				pCurrentMotionTask->SetCleanupMotionTaskNetworkOnQuit(false);
			}

			CTaskMotionTennis* pTask = rage_new CTaskMotionTennis(bUseFemaleClipSet); 
			pPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOTION, pTask, PED_TASK_MOTION_DEFAULT, true);
		} 
		else 
		{
			pPed->StartPrimaryMotionTask();
		}
	}
}


void CommandCloneOverrideTennisMode(s32 pedIndex, bool bAllowOverrideCloneUpdate)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "ENABLE_CLONE_OVERRIDE_TENNIS_MODE - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->SetAllowOverrideCloneUpdate(bAllowOverrideCloneUpdate);
		}
	}
}

bool CommandIsInTennisMode(s32 pedIndex) 
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "IS_TENNIS_MODE - Check ped is alive this frame");
		CTaskMotionTennis* pTaskMoVE = (CTaskMotionTennis*)pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS);
		if(pTaskMoVE && pTaskMoVE->GetMoveNetworkHelper()->IsNetworkActive())
		{
			return true;
		}
	}
	return false;
}

void CommandPlayTennisSwingAnim(s32 pedIndex, const char *pAnimDictName, const char *pAnimName, float startPhase, float playRate, bool bSlowBlend)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "PLAY_TENNIS_SWING_ANIM - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->PlayTennisSwingAnim(pAnimDictName, pAnimName, startPhase, playRate, bSlowBlend);
		}
	}
}

void CommandPlayTennisDiveAnim(s32 pedIndex, int iDirection, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "PLAY_TENNIS_DIVE_ANIM - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->PlayDiveAnim((CTaskMotionTennis::eDiveDirection)iDirection, fDiveHorizontal, fDiveVertical, fPlayRate, bSlowBlend);
		}
	}
}

void CommandSetTennisSwingAnimPlayRate(s32 pedIndex, float playRate)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "SET_TENNIS_SWING_ANIM_PLAY_RATE - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->SetSwingAnimPlayRate(playRate);
		}
	}
}

bool CommandGetTennisSwingAnimComplete(s32 pedIndex)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "GET_TENNIS_SWING_ANIM_COMPLETE - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			return pTask->GetTennisSwingAnimComplete();
		}
	}

	return false;
}

bool CommandTennisSwingAnimCanBeInterrupted(s32 pedIndex)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "GET_TENNIS_SWING_ANIM_CAN_BE_INTERRUPTED - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			return pTask->GetTennisSwingAnimCanBeInterrupted();
		}
	}

	return false;
}

bool CommandTennisSwingAnimSwung(s32 pedIndex)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "GET_TENNIS_SWING_ANIM_SWUNG - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			return pTask->GetTennisSwingAnimSwung();
		}
	}

	return false;
}

void CommandSetTennisAllowDampenStrafeDirection(s32 pedIndex, bool bAllow)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if(pPed)
	{
		SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "SET_TENNIS_ALLOW_DAMPEN_STRAFE_DIRECTION - Check ped is alive this frame");
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->SetAllowDampenStrafeDirection(bAllow);
		}
	}
}

void CommandSetTennisMoveNetworkSignalFloat(int PedIndex, const char* szSignal, float fSignal)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pPed && pPed->GetPedIntelligence())
	{
		CTaskMotionTennis* pTaskMoVE = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if (SCRIPT_VERIFY (pTaskMoVE, "SET_TENNIS_MOVE_NETWORK_SIGNAL_FLOAT - task CTaskMotionTennis is not running!"))
		{
			if( SCRIPT_VERIFY (pTaskMoVE->GetMoveNetworkHelper()->IsNetworkActive(), "SET_TENNIS_MOVE_NETWORK_SIGNAL_FLOAT - Move network not active!") )
			{
				if( SCRIPT_VERIFY ((!pPed->IsNetworkClone() || pTaskMoVE->GetAllowOverrideCloneUpdate()),
					"SET_TENNIS_MOVE_NETWORK_SIGNAL_FLOAT - Can't set float on cloned ped if its task is not being overridden!" ) )
				{
					pTaskMoVE->SetSignalFloat(szSignal, fSignal);
				}
			}
		}
	}
}

void CommandResetDispatchSpawnLocation()
{
	//Reset the spawn location.
	CDispatchSpawnProperties::GetInstance().ResetLocation();
}

void CommandSetDispatchSpawnLocation(const scrVector & vLocation)
{
	//Set the spawn location.
	CDispatchSpawnProperties::GetInstance().SetLocation((Vec3V)vLocation);
}

void CommandResetDispatchIdealSpawnDistance()
{
	//Reset the ideal spawn distance.
	CDispatchSpawnProperties::GetInstance().ResetIdealSpawnDistance();
}

void CommandSetDispatchIdealSpawnDistance(const float fIdealSpawnDistance)
{
	//Set the ideal spawn distance.
	CDispatchSpawnProperties::GetInstance().SetIdealSpawnDistance(fIdealSpawnDistance);
}

void CommandResetDispatchTimeBetweenSpawnAttempts(const int dispatchEnum)
{
	//Check if the resource is active.
	scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS, dispatchEnum);
	if(pResource)
	{
		//Remove the resource.
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(pResource->GetId(), false, true, CGameScriptResource::SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS);
	}
}

void CommandSetDispatchTimeBetweenSpawnAttempts(const int dispatchEnum, const float fTimeBetweenSpawnAttempts)
{
	//Remove the resource.
	CommandResetDispatchTimeBetweenSpawnAttempts(dispatchEnum);
	
	//Register the resource.
	CScriptResource_DispatchTimeBetweenSpawnAttempts timeBetweenSpawnAttempts(dispatchEnum, fTimeBetweenSpawnAttempts);
	CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(timeBetweenSpawnAttempts);
}

void CommandResetDispatchTimeBetweenSpawnAttemptsMultiplier(const int dispatchEnum)
{
	//Check if the resource is active.
	scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER, dispatchEnum);
	if(pResource)
	{
		//Remove the resource.
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(pResource->GetId(), false, true, CGameScriptResource::SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER);
	}
}

void CommandSetDispatchTimeBetweenSpawnAttemptsMultiplier(const int dispatchEnum, const float fTimeBetweenSpawnAttemptsMultiplier)
{
	//Remove the resource.
	CommandResetDispatchTimeBetweenSpawnAttemptsMultiplier(dispatchEnum);
	
	//Register the resource.
	CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier timeBetweenSpawnAttemptsMultiplier(dispatchEnum, fTimeBetweenSpawnAttemptsMultiplier);
	CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(timeBetweenSpawnAttemptsMultiplier);
}

int CommandAddDispatchSpawnAngledBlockingArea(const scrVector & vStart, const scrVector & vEnd, float fWidth)
{
	//Add the blocking area.
	int iIndex = CDispatchSpawnProperties::GetInstance().AddAngledBlockingArea((Vec3V)vStart, (Vec3V)vEnd, fWidth);
	if(SCRIPT_VERIFY(iIndex >= 0, "ADD_DISPATCH_SPAWN_ANGLED_BLOCKING_AREA -- Failed to add blocking area."))
	{
		return iIndex;
	}
	else
	{
		return -1;
	}
}

int CommandAddDispatchSpawnSphereBlockingArea(const scrVector & vCenter, float fRadius)
{
	//Add the blocking area.
	int iIndex = CDispatchSpawnProperties::GetInstance().AddSphereBlockingArea((Vec3V)vCenter, fRadius);
	if(SCRIPT_VERIFY(iIndex >= 0, "ADD_DISPATCH_SPAWN_SPHERE_BLOCKING_AREA -- Failed to add blocking area."))
	{
		return iIndex;
	}
	else
	{
		return -1;
	}
}

void CommandRemoveDispatchSpawnBlockingArea(int iIndex)
{
	//Remove the blocking area.
	CDispatchSpawnProperties::GetInstance().RemoveBlockingArea(iIndex);
}

void CommandResetDispatchSpawnBlockingAreas()
{
	//Reset the blocking areas.
	CDispatchSpawnProperties::GetInstance().ResetBlockingAreas();
}

void CommandResetWantedResponseNumPedsToSpawn()
{
	//Reset the number of peds to spawn.
	CWantedResponseOverrides::GetInstance().ResetNumPedsToSpawn();
}

void CommandSetWantedResponseNumPedsToSpawn(int iDispatchType, int iNumPedsToSpawn)
{
	//Set the number of peds to spawn.
	CWantedResponseOverrides::GetInstance().SetNumPedsToSpawn((DispatchType)iDispatchType, (s8)iNumPedsToSpawn);
}

void CommandAddTacticalNavMeshPoint(const scrVector & vPosition)
{
	CTacticalAnalysisNavMeshPoints::AddPointFromScript((Vec3V)vPosition);
}

void CommandClearTacticalNavMeshPoints()
{
	CTacticalAnalysisNavMeshPoints::ClearPointsFromScript();
}

void CommandSetRiotModeEnabled(bool bEnabled)
{
	CRiots::GetInstance().SetEnabled(bEnabled);
}

/* Virtual Keyboard usage:

DISPLAY_ONSCREEN_KEYBOARD("Enter Some Text", "initial value")

INT oskStatus = OSK_PENDING
WHILE (oskStatus == OSK_PENDING) DO
	oskStatus = UPDATE_ONSCREEN_KEYBOARD()
	WAIT(0)
END

IF oskStatus == OSK_SUCCESS THEN
    STRING result = GET_ONSCREEN_KEYBOARD_RESULT()
END

*/


enum eKeyboardType
{
	ONSCREEN_KEYBOARD_ENGLISH,
	ONSCREEN_KEYBOARD_LOCALISED,
	ONSCREEN_KEYBOARD_PASSWORD,
	ONSCREEN_KEYBOARD_GAMERTAG,
	ONSCREEN_KEYBOARD_EMAIL,
	ONSCREEN_KEYBOARD_BASIC_ENGLISH,
	ONSCREEN_KEYBOARD_FILENAME,
};


void CommandDisplayOnScreenKeyboardWithLongerInitialString(s32 keyboardTypeFlag, const char* title, const char* XENON_ONLY(pDescription) DURANGO_ONLY(pDescription), const char* pInitialValue1, const char* pInitialValue2, const char* pInitialValue3, const char* pInitialValue4, const char* pInitialValue5, const char* pInitialValue6, const char* pInitialValue7, const char* pInitialValue8, s32 maxLength)
{
	static char16 g_VirtualKeyboardTitle[64];
#if __XENON || RSG_DURANGO
	static char16 g_VirtualKeyboardDescription[256];
#endif	//	__XENON || RSG_DURANGO

	if (maxLength < 2)
	{
		scriptAssertf(0, "%s : DISPLAY_ONSCREEN_KEYBOARD - MaxLength (%d) is less than 2 so setting it to 2", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxLength);
		maxLength = 2;
	}

	if (maxLength > ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD)
	{
		scriptAssertf(0, "%s : DISPLAY_ONSCREEN_KEYBOARD - MaxLength (%d) is greater than %d so setting it to %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), maxLength, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
		maxLength = ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD;
	}

	if (title == NULL)
	{
		scriptAssertf(0, "DISPLAY_ONSCREEN_KEYBOARD - prompt string is NULL so setting it to an empty string");
		title = "";
	}

	char longInitialString[ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD];

	safecpy(longInitialString, "", ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	if (scriptVerifyf(pInitialValue1, "DISPLAY_ONSCREEN_KEYBOARD - initialValue string is NULL so setting it to an empty string"))
	{
		safecat(longInitialString, pInitialValue1, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	if (pInitialValue2)
	{
		safecat(longInitialString, pInitialValue2, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	if (pInitialValue3)
	{
		safecat(longInitialString, pInitialValue3, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	if (pInitialValue4)
	{
		safecat(longInitialString, pInitialValue4, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	if (pInitialValue5)
	{
		safecat(longInitialString, pInitialValue5, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	if (pInitialValue6)
	{
		safecat(longInitialString, pInitialValue6, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}
	if (pInitialValue7)
	{
		safecat(longInitialString, pInitialValue7, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}
	if (pInitialValue8)
	{
		safecat(longInitialString, pInitialValue8, ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
	}

	USES_CONVERSION;
	ioVirtualKeyboard::Params params;

	params.m_KeyboardType = ioVirtualKeyboard::kTextType_ALPHABET;
	params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_ENGLISH;

	switch (keyboardTypeFlag)
	{
	case ONSCREEN_KEYBOARD_ENGLISH :
		break;

	case ONSCREEN_KEYBOARD_LOCALISED :
		switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
		case LANGUAGE_RUSSIAN :
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CYRILLIC;
			break;

		case LANGUAGE_KOREAN :
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_KOREAN;
			break;

		case LANGUAGE_CHINESE_TRADITIONAL :
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CHINESE;
			break;

		case LANGUAGE_CHINESE_SIMPLIFIED:
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_CHINESE_SIMPLIFIED;
			break;

		case LANGUAGE_JAPANESE :
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_JAPANESE;
			break;

		case LANGUAGE_POLISH :
			params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_POLISH;
			break;
		}
		break;

	case ONSCREEN_KEYBOARD_PASSWORD :
		params.m_KeyboardType = ioVirtualKeyboard::kTextType_PASSWORD;
		break;

	case ONSCREEN_KEYBOARD_GAMERTAG :
		params.m_KeyboardType = ioVirtualKeyboard::kTextType_GAMERTAG;
		break;

	case ONSCREEN_KEYBOARD_EMAIL :
		params.m_KeyboardType = ioVirtualKeyboard::kTextType_EMAIL;
		break;

	case ONSCREEN_KEYBOARD_BASIC_ENGLISH :
		params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_BASIC_ENGLISH;
		break;

	case ONSCREEN_KEYBOARD_FILENAME :
		params.m_KeyboardType = ioVirtualKeyboard::kTextType_FILENAME;
		break;

	default :
		scriptAssertf(0, "%s : DISPLAY_ONSCREEN_KEYBOARD - Unrecognized keyboardTypeFlag %d so defaulting to English kTextType_ALPHABET", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyboardTypeFlag);
		break;
	}

	Utf8ToWide(g_VirtualKeyboardTitle, TheText.Get(title), NELEM(g_VirtualKeyboardTitle));
	params.m_Title = g_VirtualKeyboardTitle;

#if __XENON || RSG_DURANGO
	if (pDescription)
	{
		Utf8ToWide(g_VirtualKeyboardDescription, TheText.Get(pDescription), NELEM(g_VirtualKeyboardDescription));
		params.m_Description = g_VirtualKeyboardDescription;
	}
#endif	//	__XENON || RSG_DURANGO

	params.m_InitialValue = UTF8_TO_WIDE(longInitialString);
	//params.m_InitialValue = A2W(initialValue);
	params.m_PlayerIndex = CControlMgr::GetMainPlayerIndex();
	params.m_MaxLength = maxLength;

	CControlMgr::ShowVirtualKeyboard(params);
}

void CommandDisplayOnScreenKeyboard(s32 keyboardTypeFlag, const char* title, const char* pDescription, const char* pInitialValue1, const char* pInitialValue2, const char* pInitialValue3, const char* pInitialValue4, s32 maxLength)
{
	CommandDisplayOnScreenKeyboardWithLongerInitialString(keyboardTypeFlag, title, pDescription, pInitialValue1, pInitialValue2, pInitialValue3, pInitialValue4, "", "", "", "", maxLength);
}


int CommandUpdateOnScreenKeyboard()
{
	return (int)CControlMgr::UpdateVirtualKeyboard();
}

const char* CommandGetOnScreenKeyboardResult()
{
	if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
	{
		return CControlMgr::GetVirtualKeyboardResult();
	}
	return NULL;
}

void CommandCancelOnScreenKeyboard()
{
	CControlMgr::CancelVirtualKeyboard();
}


void CommandSetAllowTildeCharacterFromOnScreenKeyboard(bool NOTFINAL_ONLY(bAllowTilde))
{
#if !__FINAL
	CControlMgr::SetAllowTildeCharacterFromVirtualKeyboard(bAllowTilde);
#endif	//	!__FINAL
}

void CommandNextOnScreenKeyboardResultWillDisplayUsingTheseFonts(int fontBitField)
{
	CControlMgr::SetFontBitFieldForDisplayingOnscreenKeyboardResult(fontBitField);
}

void CommandEnableAction(int iActionToEnable, bool bEnable)
{
	ACTIONMGR.EnableAction( (u32)iActionToEnable, bEnable );
}

bool CommandIsActionEnabled( int iActionHash )
{
	return ACTIONMGR.IsActionEnabled( (u32)iActionHash );
}


int CommandGetRealWorldTime()
{
	return fwTimer::GetTimeInMilliseconds() / 1000;
}

void CommandSetRandomEventEnabled(s32 eventType, bool bEnable)
{
	CRandomEventManager::GetInstance().SetEventTypeEnabled(eventType, bEnable);
}

void CommandSetExplosiveAmmoThisFrame(int PlayerIndex)
{
	if (NetworkInterface::IsGameInProgress())
	{
		scriptAssertf(0, "SET_EXPLOSIVE_AMMO_THIS_FRAME call detected in MP, blocked!");
#if USE_RAGESEC
		RageSecPluginGameReactionObject t(REACT_TELEMETRY, GENERIC_GAME_ID, fwRandom::GetRandomNumber(), fwRandom::GetRandomNumber(), 0xD1D3C375);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
		t.type = REACT_KICK_RANDOMLY;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
		t.type = REACT_DESTROY_MATCHMAKING;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
#endif // USE_RAGESEC
	}
	else
	{
		CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
		if (pPlayer)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_EXPLOSIVE_AMMO_ON);
		}
	}
}

void CommandSetFireAmmoThisFrame(int PlayerIndex)
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		if (pPlayer)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_FIRE_AMMO_ON);
		}
	}
	else
	{
		CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
		if (pPlayer)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_FIRE_AMMO_ON);
		}
	}
}

void CommandSetExplosiveMeleeThisFrame(int PlayerIndex)
{
	if (NetworkInterface::IsGameInProgress())
	{
		scriptAssertf(0, "SET_EXPLOSIVE_MELEE_THIS_FRAME call detected in MP, blocked!");
#if USE_RAGESEC
		RageSecPluginGameReactionObject t(REACT_TELEMETRY, GENERIC_GAME_ID, fwRandom::GetRandomNumber(), fwRandom::GetRandomNumber(), 0xC9BA5BEB);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
		t.type = REACT_KICK_RANDOMLY;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
		t.type = REACT_DESTROY_MATCHMAKING;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(t);
#endif // USE_RAGESEC
	}
	else
	{
		CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
		if (pPlayer)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_EXPLOSIVE_MELEE_ON);
		}
	}
}

void CommandSetSuperJumpThisFrame(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
	}
}

bool CommandGetIsSuperJumpActive(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		return pPlayer->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
	}
	return false;
}

void CommandSetBeastJumpThisFrame(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_BEAST_JUMP_ON);
	}
}

bool CommandGetIsBeastJumpActive(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		return pPlayer->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_BEAST_JUMP_ON);
	}
	return false;
}

void CommandSetForcedJumpThisFrame(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_FORCED_JUMP);
	}
}

void CommandWaitUntilGameInstalled()
{
	SCRIPT_ASSERT(false, "WAIT_UNTIL_GAME_INSTALLED - This command isn't used anymore!");
}

bool CommandHasGameInstalledThisSession()
{
	return StreamingInstall::HasInstalledThisSession();
}


void CommandSetInstancePriorityMode(int playMode)
{
	CInstancePriority::SetMode(playMode);
}

void CommandSetInstancePriorityHint(int hint)
{
	CInstancePriority::SetScriptHint(hint);
}

void CommandSetTickerJohnMarstonIsDone()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	scriptAssertf(settings.AreSettingsValid(), "%s:SET_TICKER_JOHNMARSTON_IS_DONE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if(settings.AreSettingsValid() && !settings.Exists(CProfileSettings::TICKER_JOHN_MARSTON_IS_AVAILABLE))
	{
		settings.Set(CProfileSettings::TICKER_JOHN_MARSTON_IS_AVAILABLE, 1);
		settings.Write();
	}
}

void CommandPreventArrestStateThisFrame()
{
	CGameLogic::SuppressArrestState();
}

bool CommandAreProfileSettingsValid()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	return settings.AreSettingsValid();
}

void CommandForceStatePlaying()
{
	CGameLogic::ForceStatePlaying();

	CPlayerInfo	*pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	if( pPlayerInfo && pPlayerInfo->GetPlayerPed() )
	{
		pPlayerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
	}
}

void CommandScriptRaceInit(s32 numCheckpoints, s32 numLaps, s32 numPlayers, s32 localPlayerIndex)
{
    CRaceTimes::Init(numCheckpoints, numLaps, numPlayers, localPlayerIndex);
}

void CommandScriptRaceShutdown()
{
    CRaceTimes::Shutdown();
}

void CommandScriptRacePlayerHitCheckpoint(s32 pedIndex, s32 checkpoint, s32 lap, s32 time)
{
    CRaceTimes::CheckpointHit(pedIndex, checkpoint, lap, time);
}

bool CommandScriptRaceGetPlayerSplitTime(s32 pedIndex, s32& time, s32& position)
{
    return CRaceTimes::GetPlayerTime(pedIndex, time, position);
}


void CommandStartEndUserBenchmark()
{
#if COLLECT_END_USER_BENCHMARKS
	EndUserBenchmarks::StartBenchmark();
#endif
}

void CommandStopEndUserBenchmark()
{
#if COLLECT_END_USER_BENCHMARKS
	EndUserBenchmarks::StopBenchmark();
#endif
}

void CommandResetEndUserBenchmarks()
{
#if COLLECT_END_USER_BENCHMARKS
	scriptDebugf1("%s : RESET_END_USER_BENCHMARK", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	EndUserBenchmarks::ResetBenchmarks();
#endif
}

void CommandSaveEndUserBenchmarks()
{
#if COLLECT_END_USER_BENCHMARKS
	EndUserBenchmarks::OutputBenchmarkResults();
#endif
}

bool CommandDidUIStartEndUserBenchmarks()
{
#if COLLECT_END_USER_BENCHMARKS
	return EndUserBenchmarks::GetWasStartedFromPauseMenu(FE_MENU_VERSION_SP_PAUSE);
#else
	return false;
#endif
}

bool CommandDidLandingScreenStartEndUserBenchmarks()
{
#if COLLECT_END_USER_BENCHMARKS
	return EndUserBenchmarks::GetWasStartedFromPauseMenu(FE_MENU_VERSION_LANDING_MENU);
#else
	return false;
#endif
}

bool CommandEndUserBenchmarkCommandline()
{
#if COLLECT_END_USER_BENCHMARKS
	return EndUserBenchmarks::GetWasBenchmarkOnCommandline();
#else
	return false;
#endif
}



s32 CommandGetBenchmarkIterationCount()
{
#if COLLECT_END_USER_BENCHMARKS
	return EndUserBenchmarks::GetIterationCount();
#else
	return -1;
#endif
}

s32 CommandGetBenchmarkPassCount()
{
#if COLLECT_END_USER_BENCHMARKS
	return EndUserBenchmarks::GetSinglePassInfo();
#else
	return -1;
#endif
}



void CommandQuitGame()
{
#if RSG_PC
	CPauseMenu::SetGameWantsToExitToWindows(true);
	NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
#endif
}

void CommandRestartGame()
{
#if RSG_PC
	CPauseMenu::SetGameWantsToRestart(true); 
	NetworkInterface::GetNetworkExitFlow().StartShutdownTasks(); 
#endif
}

bool CommandHasAsyncInstallFinished()
{
    return CFileMgr::HasAsyncInstallFinished();
}

void CommandCleanupAsyncInstall()
{
    CFileMgr::CleanupAsyncInstall();
}

bool CommandPlmIsInConstrainedMode()
{
	return g_SysService.IsConstrained();
}

u32 CommandPlmGetConstrainedDurationMs()
{
	return g_SysService.GetConstrainedDurationMs();
}

void CommandSetPlayerIsInAnimalForm(bool bInAnimalForm)
{
	scriptDisplayf("SET_PLAYER_IS_IN_ANIMAL_FORM(%s) called by %s", bInAnimalForm?"true":"false", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CTheScripts::SetPlayerIsInAnimalForm(bInAnimalForm);
}

bool CommandGetPlayerIsInAnimalForm()
{
	return CTheScripts::GetPlayerIsInAnimalForm();
}

void CommandSetPlayerIsRepeatingAMission(bool bIsRepeatingAMission)
{
	CTheScripts::SetPlayerIsRepeatingAMission(bIsRepeatingAMission);
}

void CommandDisableScreenDimmingThisFrame()
{
	g_SysService.DisableScreenDimmingThisFrame();
}

int CommandGetLogFileNumber()
{
#if !__NO_OUTPUT
	return diagChannel::GetLogFileNumber();
#else
	return -1;
#endif
}

void CommandUseActiveCameraForTimeslicingCentre()
{
	CPedAILodManager::SetUseActiveCameraForTimeslicingCentre();
}

bool CommandIsSessionInitialized()
{
	return CGame::IsSessionInitialized();
}

#if GEN9_STANDALONE_ENABLED
int CommandGetChosenCriminalCareer()
{
	if (!uiVerifyf(SCriminalCareerService::IsInstantiated(), "Criminal Career has not been instantiated, returning default value"))
	{
		return CriminalCareerDefs::CareerIdentifier_None;
	}
	else
	{
		const CriminalCareerDefs::CareerIdentifier c_careerIdentifier = SCriminalCareerService::GetInstance().GetChosenCareerIdentifier();
		return c_careerIdentifier;
	}	
}

bool CommandHasFinalizedChosenCriminalCareer()
{
	if (!uiVerifyf(SCriminalCareerService::IsInstantiated(), "Criminal Career has not been instantiated"))
	{
		return false;
	}
	else
	{
		const bool c_isFinalized = SCriminalCareerService::GetInstance().IsShoppingCartFinalized();
		return c_isFinalized;
	}
}

int CommandGetChosenMpCharacterSlot()
{
	if (uiVerifyf(SCriminalCareerService::IsInstantiated(), "Criminal Career has not been instantiated, returning default value"))
	{
		const int c_chosenMpCharacterSlot = SCriminalCareerService::GetInstance().GetChosenMpCharacterSlot();
		return c_chosenMpCharacterSlot;	
	}
	else
	{
		return -1;
	}
}

void CommandResetChosenMpCharacterSlot()
{	
	if (uiVerifyf(SCriminalCareerService::IsInstantiated(), "Criminal Career has not been instantiated, cannot reset"))
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif
		SCriminalCareerService::GetInstance().ResetChosenMpCharacterSlot();
	}
}

#endif // GEN9_STANDALONE_ENABLED

#if RSG_GEN9
static atHashMap<int, int> s_ContentIdIndices;

void CommandSetContentIdIndex(int hash, int index)
{
	s_ContentIdIndices.Insert(hash, index);
}

int CommandGetContentIdIndex(int hash)
{
	if (atHashMap<int, int>::Pair* found = s_ContentIdIndices.FindPair(hash))
	{
		return found->second;
	}

	return -1;
}
#else
static atMap<int, int> s_ContentIdIndices;

void CommandSetContentIdIndex(int hash, int index)
{
	s_ContentIdIndices.Insert(hash, index);
}

int CommandGetContentIdIndex(int hash)
{
	const int* contentIdIndex = s_ContentIdIndices.Access(hash);
	if (contentIdIndex)
	{
		return *contentIdIndex;
	}

	return -1;
}
#endif // RSG_GEN9

void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(GET_CURRENT_STACK_SIZE,0x9d05ca7eb03931e4, CommandGetCurrentStackSize );
	SCR_REGISTER_SECURE(GET_ALLOCATED_STACK_SIZE,0x02bff29a859c559b, GetAllocatedStackSize);
	SCR_REGISTER_SECURE(GET_NUMBER_OF_FREE_STACKS_OF_THIS_SIZE,0x1bb8523a8f665d3c, CommandGetNumberOfFreeStacksOfThisSize);

	SCR_REGISTER_SECURE(SET_RANDOM_SEED,0xab6792da769477ef, CommandSetRandomSeed);
	SCR_REGISTER_UNUSED(SET_RANDOM_MWC_SEED,0xb3ba759687828e96, CommandSetRandomMWCSeed);

	SCR_REGISTER_SECURE(SET_TIME_SCALE,0x434e3ae126ddc5b5, CommandSetTimeScale);
	SCR_REGISTER_SECURE(SET_MISSION_FLAG,0xbd1f51fb3fa2c6e4, CommandSetMissionFlag);
	SCR_REGISTER_SECURE(GET_MISSION_FLAG,0x969475fa6ab2673a, CommandGetMissionFlag);
	SCR_REGISTER_SECURE(SET_RANDOM_EVENT_FLAG,0xa560174a439e78f1, CommandSetRandomEventFlag);
	SCR_REGISTER_SECURE(GET_RANDOM_EVENT_FLAG,0xe3bbdb1c36703410, CommandGetRandomEventFlag);

	SCR_REGISTER_SECURE(GET_CONTENT_TO_LOAD,0x64ae4acd8b8abba0, CommandGetContentToLoad);

	SCR_REGISTER_SECURE(ACTIVITY_FEED_CREATE,0xf3f7ab8cb18f8672, CommandActivityFeedCreate);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ADD_SUBSTRING_TO_CAPTION,0x23790f9720c61a64, CommandActivityFeedAddSubStringToCaption);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ADD_LITERAL_SUBSTRING_TO_CAPTION,0x83016c761162c9b3, CommandActivityFeedAddLiteralSubStringToCaption);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ADD_FLOAT_TO_CAPTION,0xcfb26410637d79ca, CommandActivityFeedAddFloatToCaption);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ADD_INT_TO_CAPTION,0x382b4843eeac4bee, CommandActivityFeedAddIntToCaption);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_SMALL_IMAGE_URL,0x4c2138ed18b7ddaa, CommandActivityFeedSmallImageURL);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_LARGE_IMAGE_URL,0x2cab098edb97dc42, CommandActivityFeedLargeImageURL);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_VIDEO_URL,0xa4bfbde5d14a04b5, CommandActivityFeedVideoURL);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_POST_CUSTOM_STORY,0xc4d9429bf312e605, CommandActivityFeedPostCustomStory);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_URL,0x033f22cad386d2d9, CommandActivityFeedActionURL);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_URL_WITH_THUMBNAIL,0xae452dbe980adcfb, CommandActivityFeedActionURLWithThumbnail);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_URL_ADD,0x4a8874357028cab0, CommandActivityFeedActionURLAdd);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ACTION_START_WITH_COMMAND_LINE,0x56740f9b3cf5c107, CommandActivityFeedActionStart);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_START_WITH_COMMAND_LINE_WITH_THUMBNAIL,0xe15061aad60143bc, CommandActivityFeedActionStartWithThumbnail);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ACTION_START_WITH_COMMAND_LINE_ADD,0x66a8a07bdb4374ec, CommandActivityFeedActionStartAdd);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_STORE,0xde7297e83b3c3198, CommandActivityFeedActionStore);
	SCR_REGISTER_UNUSED(ACTIVITY_FEED_ACTION_STORE_WITH_THUMBNAIL,0xebc2e94e7854050e, CommandActivityFeedActionStoreWithThumbnail);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_POST,0xdc9fc08ce33c7a1c, CommandActivityFeedPost);
	SCR_REGISTER_SECURE(ACTIVITY_FEED_ONLINE_PLAYED_WITH_POST,0x8ae5e7c487444291, CommandActivityFeedOnlinePlayedWithPost);

	SCR_REGISTER_SECURE(HAS_RESUMED_FROM_SUSPEND,0xf4c7da2b4577a1a8, CommandHasResumedFromSuspend);

	SCR_REGISTER_SECURE(SET_SCRIPT_HIGH_PRIO,0xeeefcc23227a8148, CommandSetScriptHighPrio);

	SCR_REGISTER_SECURE(SET_THIS_IS_A_TRIGGER_SCRIPT,0x3e397aa8c35a7536, CommandSetThisIsATriggerScript);

	SCR_REGISTER_UNUSED(SET_MISSION_NAME_DISPLAY,0x75712baa1c868552, CommandSetMissionNameDisplay		);

	SCR_REGISTER_SECURE(INFORM_CODE_OF_CONTENT_ID_OF_CURRENT_UGC_MISSION,0x5878944a227f31f3, CommandInformCodeOfContentIDOfCurrentUGCMission);

	SCR_REGISTER_UNUSED(GET_BASE_ELEMENT_LOCATION_FROM_METADATA,0xa435716d942bc584, CommandGetBaseElementLocationFromMetadata);
	SCR_REGISTER_SECURE(GET_BASE_ELEMENT_LOCATION_FROM_METADATA_BLOCK,0x8fbc59b07727a89e, CommandGetBaseElementLocationFromMetadataBlock);

	// new weather commands
	SCR_REGISTER_SECURE(GET_PREV_WEATHER_TYPE_HASH_NAME,0x0831c76f5bf9595e, CommandGetPrevWeatherTypeHashName);
	SCR_REGISTER_SECURE(GET_NEXT_WEATHER_TYPE_HASH_NAME,0xb1d36fa656f76888, CommandGetNextWeatherTypeHashName);
	SCR_REGISTER_SECURE(IS_PREV_WEATHER_TYPE,0x410dfb50be272f58, CommandIsPrevWeatherType);
	SCR_REGISTER_SECURE(IS_NEXT_WEATHER_TYPE,0x6dcfe88f29c7556e, CommandIsNextWeatherType);
	SCR_REGISTER_SECURE(SET_WEATHER_TYPE_PERSIST,0xa32f818f9701571c, CommandSetWeatherTypePersist);
	SCR_REGISTER_SECURE(SET_WEATHER_TYPE_NOW_PERSIST,0x9cd86fac92dbe038, CommandSetWeatherTypeNowPersist);
	SCR_REGISTER_SECURE(SET_WEATHER_TYPE_NOW,0xfd366591424e7542, CommandSetWeatherTypeNow);
	SCR_REGISTER_SECURE(SET_WEATHER_TYPE_OVERTIME_PERSIST,0x95fc0d9ffa87229e, CommandSetWeatherTypeOvertimePersist);
	SCR_REGISTER_SECURE(SET_RANDOM_WEATHER_TYPE,0xe715ced9607d2ece, CommandSetRandomWeatherType);
	SCR_REGISTER_SECURE(CLEAR_WEATHER_TYPE_PERSIST,0xc4075f869bc19306, CommandClearWeatherTypePersist);
	SCR_REGISTER_SECURE(CLEAR_WEATHER_TYPE_NOW_PERSIST_NETWORK,0x18b8b239b6c992fe, CommandClearWeatherTypeNowPersistNetwork);
	SCR_REGISTER_SECURE(GET_CURR_WEATHER_STATE,0x234904ad6416665b, CommandGetWeatherState);
	SCR_REGISTER_SECURE(SET_CURR_WEATHER_STATE,0x325fe91915483aae, CommandSetWeatherState);


	// used to override the global weather setting for network games
	SCR_REGISTER_SECURE(SET_OVERRIDE_WEATHER,0x6964c8d9877ab910, CommandSetOverrideWeather);
	SCR_REGISTER_SECURE(SET_OVERRIDE_WEATHEREX,0xdb125694c9cda16b, CommandSetOverrideWeatherEx);
	SCR_REGISTER_SECURE(CLEAR_OVERRIDE_WEATHER,0x92af77f76753447a, CommandClearOverrideWeather);

	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_SHOREWAVEAMPLITUDE,0xb252b5efbe5eac46,		CommandSetOverrideShoreWaveAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_SHOREWAVEMINAMPLITUDE,0x72fcb447b822ff47,	CommandSetOverrideShoreWaveMinAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_SHOREWAVEMAXAMPLITUDE,0x86ede07c99406ba7,	CommandSetOverrideShoreWaveMaxAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_OCEANNOISEMINAMPLITUDE,0xfea2c505215523d7,	CommandSetOverrideOceanNoiseMinAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_OCEANWAVEAMPLITUDE,0x77048cb9a5fb0916,		CommandSetOverrideOceanWaveAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_OCEANWAVEMINAMPLITUDE,0x09282630f7e20396,	CommandSetOverrideOceanWaveMinAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_OCEANWAVEMAXAMPLITUDE,0x84cafc0a42fd76a0,	CommandSetOverrideOceanWaveMaxAmplitude);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_RIPPLEBUMPINESS,0x5aa6d432576df799,			CommandSetOverrideRippleBumpiness);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_RIPPLEMINBUMPINESS,0x1a821c0c65ec99af,		CommandSetOverrideRippleMinBumpiness);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_RIPPLEMAXBUMPINESS,0x8786d241002bc8d3,		CommandSetOverrideRippleMaxBumpiness);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_RIPPLEDISTURB,0x81a6481340f8990d,			CommandSetOverrideRippleDisturb);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_SET_STRENGTH,0xd25cad13a0bd46c9,				CommandSetWaterOverrideStrength);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_FADE_IN,0x5d5d57e564e3d07b,						CommandFadeInWaterOverrideOverTime);
	SCR_REGISTER_SECURE(WATER_OVERRIDE_FADE_OUT,0x871782f14f0d7b2d,					CommandFadeOutWaterOverrideOverTime);

	SCR_REGISTER_SECURE(SET_WIND,0xab0c65a9ad2e3cff, CommandSetWind);
	SCR_REGISTER_SECURE(SET_WIND_SPEED,0xc66ad89039f856e2, CommandSetWindSpeed);
	SCR_REGISTER_SECURE(GET_WIND_SPEED,0xeb79f8939f3e6872, CommandGetWindSpeed);
	SCR_REGISTER_UNUSED(GET_WIND_SPEED_MAX,0xe161612575d0216d, CommandGetWindSpeedMax);
	SCR_REGISTER_SECURE(SET_WIND_DIRECTION,0xfa33a321c7b93ced, CommandSetWindDirection);
	SCR_REGISTER_SECURE(GET_WIND_DIRECTION,0x140a8934a5733d96, CommandGetWindDirection);
	SCR_REGISTER_SECURE(SET_RAIN,0xbf9611f192a7c3eb, CommandSetRain);
	SCR_REGISTER_SECURE(GET_RAIN_LEVEL,0x63b191e910106864, CommandGetRainLevel);
	SCR_REGISTER_SECURE(SET_SNOW,0xe22e7b8fa0c9916a, CommandSetSnow);
	SCR_REGISTER_SECURE(GET_SNOW_LEVEL,0xb5e8a19e38498780, CommandGetSnowLevel);
	SCR_REGISTER_SECURE(FORCE_LIGHTNING_FLASH,0xd7c2019ec11e9a90, CommandForceLightningFlash);

	SCR_REGISTER_UNUSED(ADD_WIND_THERMAL,0x37201cd57a9b677f, CommandAddWindThermal);
	SCR_REGISTER_UNUSED(REMOVE_WIND_THERMAL,0x480dc99192605686, CommandRemoveWindThermal);

	SCR_REGISTER_SECURE(SET_CLOUD_SETTINGS_OVERRIDE,0xd6013c019f519e30,CommandCloudSettingsOverride);
	SCR_REGISTER_SECURE(PRELOAD_CLOUD_HAT,0x13f742409efb338e, CommandPreloadCloudHat);
	SCR_REGISTER_UNUSED(RELEASE_PRELOAD_CLOUD_HAT,0x30008e3f5eef01cf, CommandReleasePreloadCloudHat);
	SCR_REGISTER_SECURE(LOAD_CLOUD_HAT,0x52e423fd38f601b9, CommandLoadCloudHat);
	SCR_REGISTER_SECURE(UNLOAD_CLOUD_HAT,0xc6d4125eb3ef156d, CommandUnloadCloudHat);
	SCR_REGISTER_SECURE(UNLOAD_ALL_CLOUD_HATS,0x967d9549339d0a5b, CommandUnloadAllCloudHats);
	
	SCR_REGISTER_SECURE(SET_CLOUDS_ALPHA,0xee1c00f3473ac995, CommandSetCloudHatAlpha);
	SCR_REGISTER_UNUSED(RESET_CLOUDS_ALPHA,0x8aec426542aa2bd9, CommandResetCloudHatAlpha);
	SCR_REGISTER_SECURE(GET_CLOUDS_ALPHA,0xd69743921ab3f0f1, CommandGetCloudHatAlpha);

	SCR_REGISTER_SECURE(GET_GAME_TIMER,0x320d1840b6daa1cc, CommandGetGameTimer);
	SCR_REGISTER_UNUSED(GET_NUMBER_OF_MICROSECONDS_SINCE_LAST_CALL,0x2250f0db2ed78dee, CommandGetNumberOfMicrosecondsSinceLastCall);
	SCR_REGISTER_UNUSED(GET_SCRIPT_TIME_WITHIN_FRAME_IN_MICROSECONDS,0x165e83a234816d0b, CommandGetScriptTimeWithinFrameInMicroseconds);
	SCR_REGISTER_UNUSED(RESET_SCRIPT_TIME_WITHIN_FRAME,0x47649a42d455d975, CommandResetScriptTimeWithinFrame);
	SCR_REGISTER_SECURE(GET_FRAME_TIME,0x51109c28352ec9a3, CommandGetFrameTime);
	SCR_REGISTER_SECURE(GET_SYSTEM_TIME_STEP,0x74aa3c129f86fc2d, CommandGetSystemTimeStep);

	SCR_REGISTER_SECURE(GET_FRAME_COUNT,0x8142a6539ddc180f, CommandGetFrameCount);
	
	//Maths
	SCR_REGISTER_SECURE(GET_RANDOM_FLOAT_IN_RANGE,0xc4eab25a108c2429, CommandGenerateRandomFloatInRange);
	SCR_REGISTER_SECURE(GET_RANDOM_INT_IN_RANGE,0x5853b15f78e1a265, CommandGenerateRandomIntInRange);
	SCR_REGISTER_SECURE(GET_RANDOM_MWC_INT_IN_RANGE,0x95f3c8dae5a0339c, CommandGenerateRandomMWCIntInRange);

	SCR_REGISTER_SECURE(GET_GROUND_Z_FOR_3D_COORD,0x9cd4cbf2bbe10f00, CommandGetGroundZFor3dCoord);
	SCR_REGISTER_SECURE(GET_GROUND_Z_AND_NORMAL_FOR_3D_COORD,0x45d982dae0be3255, CommandGetGroundZAndNormalFor3dCoord);
	SCR_REGISTER_SECURE(GET_GROUND_Z_EXCLUDING_OBJECTS_FOR_3D_COORD,0x6d2d1393fa9794d3, CommandGetGroundZExcludeObjectsFor3dCoord);
	SCR_REGISTER_SECURE(ASIN,0xcd3c356ca8d8298c, CommandASin);
	SCR_REGISTER_SECURE(ACOS,0x12528dc2778cb217, CommandACos);
	SCR_REGISTER_SECURE(TAN,0x06a2016215b8e171, CommandTan);
	SCR_REGISTER_SECURE(ATAN,0xe391a81befb0f974, CommandATan);
	SCR_REGISTER_SECURE(ATAN2,0xc529d13129c03cf4, CommandATan2);
	SCR_REGISTER_SECURE(GET_DISTANCE_BETWEEN_COORDS,0x987a5f1e1a67e3c0, CommandGetDistanceBetweenCoords);
	SCR_REGISTER_SECURE(GET_ANGLE_BETWEEN_2D_VECTORS,0x952f3fa2e7880565, CommandGetAngleBetween2dVectors);
	SCR_REGISTER_SECURE(GET_HEADING_FROM_VECTOR_2D,0xd12efcab87804bed, CommandGetHeadingFromVector2d);
	SCR_REGISTER_SECURE(GET_RATIO_OF_CLOSEST_POINT_ON_LINE,0xd7500bc8646dab0d, CommandClosestTValueOnLine);
	SCR_REGISTER_SECURE(GET_CLOSEST_POINT_ON_LINE,0x291018e753a9a0ad, CommandClosestPointOnLine);
	SCR_REGISTER_SECURE(GET_LINE_PLANE_INTERSECTION,0xdfaf13813077792a, ComamndLineIntersectsPlane);
	SCR_REGISTER_SECURE(GET_POINT_AREA_OVERLAP,0xc25227ffc64097ff, CommandPointAreaOverlap);
	SCR_REGISTER_SECURE(SET_BIT,0xced9e32812d6c7c7, CommandSetBit);
	SCR_REGISTER_SECURE(CLEAR_BIT,0xb0550bc91b0159d6, CommandClearBit);
	SCR_REGISTER_SECURE(GET_HASH_KEY,0x2e87280918b16506, CommandGetHashKey);
	SCR_REGISTER_SECURE(SLERP_NEAR_QUATERNION,0x6d60832059befca5, CommandSlerpNearQuaternion); 

	//DEBUG event recording
	SCR_REGISTER_UNUSED(DEBUG_RECORD_START,0xa90d7a9390bd8f3d, DebugRecordStart);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_END,0xabfed591ce971f84, DebugRecordEnd);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_SAVE,0x8ff49609d8384b9b, DebugRecordSave);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_THIS_ENTITY,0x391c5280f4245775, DebugRecordThisInst);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY,0x9a4ce63767c521ff, DebugRecordInst);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_CLEAR_ENTITIES,0x23535bc7b0368aa6, DebugRecordClearInsts);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ALL_ENTITIES,0x897fa206c9849bbe, DebugRecordAllInsts);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_STRING,0x541bf377d861ed23, DebugRecordRecordString);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY_STRING,0x26ec0a96a7d7fe0e, DebugRecordRecordInstString);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY_FLOAT,0x1060750a8d39db07, DebugRecordRecordInstFloat);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY_VECTOR,0x64fb1a2f3ad76350, DebugRecordRecordInstVector);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_LINE,0x1a100e58fe8dabe2, DebugRecordSimpleLine);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_SPHERE,0x1f41e39434616104, DebugRecordSimpleSphere);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY_LINE,0xccc1adde87802277, DebugRecordInstLine);
	SCR_REGISTER_UNUSED(DEBUG_RECORD_ENTITY_SPHERE,0x1f47c1a96eae8796, DebugRecordInstSphere);

	//Area
	SCR_REGISTER_SECURE(IS_AREA_OCCUPIED,0x15c2370f75b1b388, CommandIsAreaOccupied);
	SCR_REGISTER_SECURE(IS_AREA_OCCUPIED_SLOW,0x6ea66e1eac07ca8c, CommandIsAreaOccupiedAccurate);
	SCR_REGISTER_SECURE(IS_POSITION_OCCUPIED,0x90bd7eb173e1cfd6, CommandIsPositionOccupied);
	SCR_REGISTER_SECURE(IS_POINT_OBSCURED_BY_A_MISSION_ENTITY,0x488e34a772939b4c, CommandIsPointObscuredByAMissionEntity);
	SCR_REGISTER_SECURE(CLEAR_AREA,0x7a997a0a971d305f, CommandClearArea);
	SCR_REGISTER_SECURE(CLEAR_AREA_LEAVE_VEHICLE_HEALTH,0xd414399d29212438, CommandClearAreaLeaveVehicleHealth );
	SCR_REGISTER_SECURE(CLEAR_AREA_OF_VEHICLES,0x02b222eadc9dc566, CommandClearAreaOfVehicles);
	SCR_REGISTER_SECURE(CLEAR_ANGLED_AREA_OF_VEHICLES,0x17b104183e0f2c46, CommandClearAngledAreaOfVehicles);
	SCR_REGISTER_SECURE(CLEAR_AREA_OF_OBJECTS,0xdc226c748c3c868d, CommandClearAreaOfObjects);
	SCR_REGISTER_SECURE(CLEAR_AREA_OF_PEDS,0xf96094a43d443c41, CommandClearAreaOfPeds);
	SCR_REGISTER_SECURE(CLEAR_AREA_OF_COPS,0xcb583354ea6e5e4a, CommandClearAreaOfCops);
	SCR_REGISTER_SECURE(CLEAR_AREA_OF_PROJECTILES,0x917f631782a9cb32, CommandClearAreaOfProjectiles);
	SCR_REGISTER_SECURE(CLEAR_SCENARIO_SPAWN_HISTORY,0x49bd8a3aec4c4cf4, CommandClearScenarioSpawnHistory);
	SCR_REGISTER_SECURE(SET_SAVE_MENU_ACTIVE,0xe708f785c0b6cb98, CommandActivateSaveMenu);
	SCR_REGISTER_SECURE(GET_STATUS_OF_MANUAL_SAVE,0x889492759cde5b07, CommandGetStatusOfManualSave);
	SCR_REGISTER_UNUSED(DID_SAVE_COMPLETE_SUCCESSFULLY,0x05706e34f1c87047, CommandDidSaveCompleteSuccessfully);
	SCR_REGISTER_UNUSED(SET_NETWORK_SETTINGS_MENU,0x257b7aefb66d58d2, CommandSetNetworkMenu);
	SCR_REGISTER_SECURE(SET_CREDITS_ACTIVE,0x291b578efc5bc622, CommandSetCreditsActive);
	SCR_REGISTER_SECURE(SET_CREDITS_FADE_OUT_WITH_SCREEN,0x4b980fe990fd29a0, CommandSetCreditsFadeOutWithScreen);
	SCR_REGISTER_SECURE(HAVE_CREDITS_REACHED_END,0xca215a1d7ffd35cc, CommandHaveCreditsReachedEnd);
	SCR_REGISTER_SECURE(ARE_CREDITS_RUNNING,0xa1cec49d70a8bf2c,CommandAreCreditsRunning);
	
	SCR_REGISTER_SECURE(TERMINATE_ALL_SCRIPTS_WITH_THIS_NAME,0xcb67cdee283e73dc, CommandTerminateAllScriptsWithThisName);
//	SCR_REGISTER_UNUSED(TERMINATE_ALL_SCRIPTS_FOR_NETWORK_GAME, 0x51c902bd, CommandTerminateAllScriptsForNetworkGame);
	SCR_REGISTER_SECURE(NETWORK_SET_SCRIPT_IS_SAFE_FOR_NETWORK_GAME,0x3f57bedbc381cbc7, CommandThisScriptIsSafeForNetworkGame);
	SCR_REGISTER_UNUSED(NETWORK_GET_IS_SCRIPT_SAFE_FOR_NETWORK_GAME,0xfadba653c265e78a, CommandGetIsScriptSafeForNetworkGame);
	SCR_REGISTER_UNUSED(SET_PLAYER_CONTROL_ON_IN_MISSION_CLEANUP,0x451a0065621843ce, CommandSetPlayerControlOnInMissionCleanup);

	SCR_REGISTER_SECURE(ADD_HOSPITAL_RESTART,0x6104d87e51533cf0, CommandAddHospitalRestart);
	SCR_REGISTER_SECURE(DISABLE_HOSPITAL_RESTART,0x582d074c5cdb512a, CommandDisableHospitalRestart);

	SCR_REGISTER_SECURE(ADD_POLICE_RESTART,0xd5c8a1f537cc13cd, CommandAddPoliceRestart);
	SCR_REGISTER_SECURE(DISABLE_POLICE_RESTART,0xcb7dbda526904496, CommandDisablePoliceRestart);

	SCR_REGISTER_SECURE(SET_RESTART_COORD_OVERRIDE,0x275e58a39347a249, CommandOverrideNextRestart);
	SCR_REGISTER_SECURE(CLEAR_RESTART_COORD_OVERRIDE,0x1b9c2beba596b762, CommandCancelOverrideRestart);
	SCR_REGISTER_SECURE(PAUSE_DEATH_ARREST_RESTART,0x3780e26e475691ed, CommandPauseDeathArrestRestart);
	SCR_REGISTER_SECURE(IGNORE_NEXT_RESTART,0x631794154765bf95, CommandIgnoreNextRestart);

	SCR_REGISTER_SECURE(SET_FADE_OUT_AFTER_DEATH,0xb607c92d60b318dd, CommandSetFadeOutAfterDeath);
	SCR_REGISTER_SECURE(SET_FADE_OUT_AFTER_ARREST,0x07fd15da380c8512, CommandSetFadeOutAfterArrest);
	SCR_REGISTER_SECURE(SET_FADE_IN_AFTER_DEATH_ARREST,0x3e49ef84c421e367, CommandSetFadeInAfterDeathArrest);
	SCR_REGISTER_SECURE(SET_FADE_IN_AFTER_LOAD,0x8eaab4c63946014a, CommandSetFadeInAfterLoad);
	SCR_REGISTER_SECURE(REGISTER_SAVE_HOUSE,0x1409c3f0480b1ae1, CommmandRegisterSaveHouse);
	SCR_REGISTER_SECURE(SET_SAVE_HOUSE,0x71c0ad6b706b0460, CommandEnableSaveHouse);
	SCR_REGISTER_SECURE(OVERRIDE_SAVE_HOUSE,0x6c3901dad6a66e9b, CommandOverrideSaveHouse);
	SCR_REGISTER_SECURE(GET_SAVE_HOUSE_DETAILS_AFTER_SUCCESSFUL_LOAD,0xbb3adc7c09e1a3fe, CommandGetSaveHouseDetailsAfterSuccessfulLoad);
	SCR_REGISTER_SECURE(DO_AUTO_SAVE,0x090f3b7471315852, CommandDoAutoSave);
	SCR_REGISTER_SECURE(GET_IS_AUTO_SAVE_OFF,0x1a02fc35b2cea0b1, CommandGetIsAutoSaveOff);
	SCR_REGISTER_UNUSED(GET_IS_DISPLAYING_SAVE_MESSAGE,0xb4b43085c1b27f3f, CommandGetIsDisplayingSaveMessage);
	SCR_REGISTER_SECURE(IS_AUTO_SAVE_IN_PROGRESS,0x07ad9e67f357c655, CommandIsAutoSaveInProgress);
	SCR_REGISTER_SECURE(HAS_CODE_REQUESTED_AUTOSAVE,0x07e4b614127ef0b9, CommandHasCodeRequestedAutosave);
	SCR_REGISTER_SECURE(CLEAR_CODE_REQUESTED_AUTOSAVE,0xc2d330547506b393, CommandClearCodeRequestedAutosave);

	SCR_REGISTER_SECURE(BEGIN_REPLAY_STATS,0x3e9c72be85529fca, CommandBeginReplayStats);
	SCR_REGISTER_SECURE(ADD_REPLAY_STAT_VALUE,0x0bc5939c2b796f37, CommandAddReplayStatValue);
	SCR_REGISTER_SECURE(END_REPLAY_STATS,0x7c6521e71e7516ff, CommandEndReplayStats);

	SCR_REGISTER_SECURE(HAVE_REPLAY_STATS_BEEN_STORED,0x816bb3ea7eff7ae0, CommandHaveReplayStatsBeenStored);
	SCR_REGISTER_SECURE(GET_REPLAY_STAT_MISSION_ID,0x9452928d5e202744, CommandGetReplayStatMissionId);
	SCR_REGISTER_SECURE(GET_REPLAY_STAT_MISSION_TYPE,0x239a5a4365d45418, CommandGetReplayStatMissionType);
	SCR_REGISTER_SECURE(GET_REPLAY_STAT_COUNT,0x40b3ae143c0ad363, CommandGetReplayStatCount);
	SCR_REGISTER_SECURE(GET_REPLAY_STAT_AT_INDEX,0x8ee1969ab2d0232f, CommandGetReplayStatAtIndex);
	SCR_REGISTER_SECURE(CLEAR_REPLAY_STATS,0x4a38a0835fed135b, CommandClearReplayStats);



	SCR_REGISTER_SECURE(QUEUE_MISSION_REPEAT_LOAD,0x70a108100ab45cd7, CommandQueueMissionRepeatLoad);
	SCR_REGISTER_UNUSED(GET_STATUS_OF_MISSION_REPEAT_LOAD,0xab63a5455a68d6c1, CommandGetStatusOfMissionRepeatLoad);
	SCR_REGISTER_UNUSED(CLEAR_STATUS_OF_MISSION_REPEAT_LOAD,0x0756649919c96d14, CommandClearStatusOfMissionRepeatLoad);

	SCR_REGISTER_SECURE(QUEUE_MISSION_REPEAT_SAVE,0x18e2479a36402e8d, CommandQueueMissionRepeatSave);
	SCR_REGISTER_SECURE(QUEUE_MISSION_REPEAT_SAVE_FOR_BENCHMARK_TEST,0xe799476ecf831fe3, CommandQueueMissionRepeatSaveForBenchmarkTest);
	SCR_REGISTER_SECURE(GET_STATUS_OF_MISSION_REPEAT_SAVE,0x0c76dfddcbb67a06, CommandGetStatusOfMissionRepeatSave);
	SCR_REGISTER_UNUSED(CLEAR_STATUS_OF_MISSION_REPEAT_SAVE,0xeb0b7f57c096cfde, CommandClearStatusOfMissionRepeatSave);
	
	SCR_REGISTER_UNUSED(CAN_START_MISSION_PASSED_TUNE,0xd4ca97b2ea1d6e7b, CommandCanStartMissionPassedTune);
	SCR_REGISTER_SECURE(IS_MEMORY_CARD_IN_USE,0x92b900558e4a1e49, CommandIsMemoryCardInUse);
	SCR_REGISTER_SECURE(SHOOT_SINGLE_BULLET_BETWEEN_COORDS,0xdb6ad61ce00e8a46, CommandFireSingleBullet);
	SCR_REGISTER_SECURE(SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY,0x251885feebe139f2, CommandFireSingleBulletIgnoreEntity);
	SCR_REGISTER_SECURE(SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY_NEW,0x633f41b5cea31cc2, CommandFireSingleBulletIgnoreEntityNew);
	SCR_REGISTER_SECURE(GET_MODEL_DIMENSIONS,0xb38ef75835faf667, CommandGetModelDimensions);
	SCR_REGISTER_SECURE(SET_FAKE_WANTED_LEVEL,0x30266329281c3a4f, CommandSetFakeWantedLevel);
	SCR_REGISTER_SECURE(GET_FAKE_WANTED_LEVEL,0xc1267d7e16e74496, CommandGetFakeWantedLevel);
	SCR_REGISTER_UNUSED(IS_BIT_SET,0xe2d0c323a1ae5d85, CommandIsBitSet);

	SCR_REGISTER_SECURE(USING_MISSION_CREATOR,0xed32721223814a05, CommandUsingMissionCreator);
	SCR_REGISTER_SECURE(ALLOW_MISSION_CREATOR_WARP,0x49e24542c726ebac, CommandAllowMissionCreatorWarp);
	SCR_REGISTER_SECURE(SET_MINIGAME_IN_PROGRESS,0x0d0b74b18bf93317, CommandSetMinigameInProgress);
	SCR_REGISTER_SECURE(IS_MINIGAME_IN_PROGRESS,0x410d46b709324b0f, CommandIsMinigameInProgress);
	SCR_REGISTER_SECURE(IS_THIS_A_MINIGAME_SCRIPT,0x4308664be93cb669, CommandIsThisAMinigameScript);
	SCR_REGISTER_SECURE(IS_SNIPER_INVERTED,0x8a6d972d3a2e44d7, CommandIsSniperInverted);
	SCR_REGISTER_SECURE(SHOULD_USE_METRIC_MEASUREMENTS,0x70350e94345b6f7b, CommandShouldUseMetricMeasurements);
	SCR_REGISTER_SECURE(GET_PROFILE_SETTING,0x5d851a9195129ce9, CommandGetProfileSetting);
	SCR_REGISTER_SECURE(ARE_STRINGS_EQUAL,0x3c57c2f07fee34d2, CommandAreStringsEqual);
	SCR_REGISTER_SECURE(COMPARE_STRINGS,0x1c1342f0adfea3b3, CommandCompareStrings);
	SCR_REGISTER_SECURE(ABSI,0x07141611fd632b59, AbsInt);
	SCR_REGISTER_SECURE(ABSF,0x48053974ed6f63ce, AbsFloat);
	SCR_REGISTER_SECURE(IS_SNIPER_BULLET_IN_AREA,0x42b9e261d7c3ebbf, CommandIsSniperBulletInArea);
	SCR_REGISTER_SECURE(IS_PROJECTILE_IN_AREA,0x863026e0c5c2e52b, CommandIsProjectileInArea);
	SCR_REGISTER_SECURE(IS_PROJECTILE_TYPE_IN_AREA,0xa1c6623fc1047dad, CommandIsProjectileTypeInArea);
	SCR_REGISTER_SECURE(IS_PROJECTILE_TYPE_IN_ANGLED_AREA,0x2767353d989432a3, CommandIsProjectileTypeInAngledArea);
	SCR_REGISTER_SECURE(IS_PROJECTILE_TYPE_WITHIN_DISTANCE,0xedf30c9168b336d8, CommandIsProjectileTypeWithinDistance);
	SCR_REGISTER_SECURE(GET_COORDS_OF_PROJECTILE_TYPE_IN_AREA,0x42b0867e406ec2ae, CommandGetCoordsOfProjectileTypeInArea);
	SCR_REGISTER_UNUSED(GET_COORDS_OF_PROJECTILE_TYPE_IN_ANGLED_AREA,0x0338ae308befb34d, CommandGetCoordsOfProjectileTypeInAngledArea);
	SCR_REGISTER_SECURE(GET_COORDS_OF_PROJECTILE_TYPE_WITHIN_DISTANCE,0xe3d651a33d0fad23, CommandGetCoordsOfProjectileTypeWithinDistance);
	SCR_REGISTER_SECURE(GET_PROJECTILE_OF_PROJECTILE_TYPE_WITHIN_DISTANCE,0x367885498c2037bd, CommandGetProjectileOfProjectileTypeWithinDistance);
	SCR_REGISTER_SECURE(IS_BULLET_IN_ANGLED_AREA,0x42d790a6634dc8dc, CommandIsBulletInAngledArea);
	SCR_REGISTER_SECURE(IS_BULLET_IN_AREA,0x9ac74c7ef847a074, CommandIsBulletInArea);
	SCR_REGISTER_SECURE(IS_BULLET_IN_BOX,0x4cb6acee3ea17cc9, CommandIsBulletInBox);
	SCR_REGISTER_SECURE(HAS_BULLET_IMPACTED_IN_AREA,0x81c70e8a536aac9e, CommandHasBulletImpactedInArea);
	SCR_REGISTER_SECURE(HAS_BULLET_IMPACTED_IN_BOX,0xc73d3966feed8c9c, CommandHasBulletImpactedInBox);
	SCR_REGISTER_UNUSED(HAS_BULLET_IMPACTED_BEHIND_PLANE,0xed02ab40f9b6feb6, CommandHasBulletImpactedBehindPlane);
	SCR_REGISTER_UNUSED(GET_NUM_BULLETS_IMPACTED_IN_BOX,0x2c800874540f4f3b, CommandGetNumBulletsImpactedInBox);
	SCR_REGISTER_UNUSED(GET_NUM_BULLETS_IMPACTED_IN_AREA,0x91d2f1868e0793c4, CommandGetNumBulletsImpactedInArea);

	SCR_REGISTER_UNUSED(GET_UNIQUE_MAP_VERSION,0x87c65863a6cb6f41, CommandGetUniqueMapVersion);
	SCR_REGISTER_SECURE(IS_ORBIS_VERSION,0xa7384dad7cf469da, CommandIsOrbisVersion);
	SCR_REGISTER_SECURE(IS_DURANGO_VERSION,0x3ebd3af4e5d7a08c, CommandIsDurangoVersion);
	SCR_REGISTER_SECURE(IS_XBOX360_VERSION,0xdfc67688f9088b45, CommandIsXbox360Version);
	SCR_REGISTER_SECURE(IS_PS3_VERSION,0x527312c0df85960a, CommandIsPS3Version);
	SCR_REGISTER_SECURE(IS_PC_VERSION,0x8fe9914d4872d601, CommandIsPCVersion);
	SCR_REGISTER_SECURE(IS_STEAM_VERSION,0x0a27b2b6282f7169, CommandIsSteamVersion);
	SCR_REGISTER_UNUSED(IS_GERMAN_VERSION,0x687c805ba078733c, CommandIsGermanVersion);
	SCR_REGISTER_SECURE(IS_AUSSIE_VERSION,0x86790e25cce58a5b, CommandIsAussieVersion);
	SCR_REGISTER_SECURE(IS_JAPANESE_VERSION,0xb8c0bb75d8a77db3, CommandIsJapaneseVersion);
	SCR_REGISTER_SECURE(IS_XBOX_PLATFORM, 0x138679ca01e21f53, CommandIsXboxPlatform);
	SCR_REGISTER_SECURE(IS_SCARLETT_VERSION, 0xc545ab1cf97abb34, CommandIsScarlettVersion);
	SCR_REGISTER_SECURE(IS_SCE_PLATFORM, 0xf911e695c1eb8518, CommandIsScePlatform);
	SCR_REGISTER_SECURE(IS_PROSPERO_VERSION, 0x807abe1ab65c24d2, CommandIsProsperoVersion);

	SCR_REGISTER_UNUSED(HAS_STORAGE_DEVICE_BEEN_SELECTED,0x26e19cb0038ed95f, CommandHasStorageDeviceBeenSelected);

	SCR_REGISTER_SECURE(IS_STRING_NULL,0xe8f6c1f695b25b91,	CommandIsStringNull);
	SCR_REGISTER_SECURE(IS_STRING_NULL_OR_EMPTY,0xacc32b78196fbc2a, CommandIsStringNullOrEmpty);
	SCR_REGISTER_SECURE(STRING_TO_INT,0xa8581fb9086c10c4,		CommandStringToInt);
	SCR_REGISTER_UNUSED(SET_PHONE_HUD_ITEM,0xcfcd18ca8fff8d7c, CommandSetPhoneHudItem);
	SCR_REGISTER_UNUSED(SET_OVERRIDE_NO_SPRINTING_ON_PHONE_IN_MULTIPLAYER,0xb6fab419baa3b5fa, CommandSetOverrideNoSprintingOnPhoneInMultiplayer);
	SCR_REGISTER_UNUSED(SET_MESSAGES_WAITING,0xe5b2167603650634, CommandSetMessagesWaiting);
	SCR_REGISTER_UNUSED(SET_SLEEP_MODE_ACTIVE,0x236d7f6c1691ef9b, CommandSetSleepModeActive);
	SCR_REGISTER_SECURE(SET_BITS_IN_RANGE,0x66daaa2eabb846b9, CommandSetBitsInRange);
	SCR_REGISTER_SECURE(GET_BITS_IN_RANGE,0xf6cf013e72c680b4, CommandGetBitsInRange);
	SCR_REGISTER_SECURE(ADD_STUNT_JUMP,0xe3e21255829bfa40, CommandAddStuntJump);
	SCR_REGISTER_SECURE(ADD_STUNT_JUMP_ANGLED,0x55b30d3eba45a1fa, CommandAddStuntJumpAngled);
	SCR_REGISTER_SECURE(TOGGLE_SHOW_OPTIONAL_STUNT_JUMP_CAMERA,0xc14f6902ae1a5e2a, CommandToggleShowOptionalStuntJumpCamera);
	SCR_REGISTER_SECURE(DELETE_STUNT_JUMP,0x2e1fd4291978441d, CommandDeleteStuntJump);
	SCR_REGISTER_SECURE(ENABLE_STUNT_JUMP_SET,0xe26992076b0111b4, CommandEnableStuntJumpSet);
	SCR_REGISTER_SECURE(DISABLE_STUNT_JUMP_SET,0x3a19907cc9850026, CommandDisableStuntJumpSet);
	SCR_REGISTER_SECURE(SET_STUNT_JUMPS_CAN_TRIGGER,0x97df6de511b1cc98, CommandAllowStuntJumpsToTrigger);
	SCR_REGISTER_SECURE(IS_STUNT_JUMP_IN_PROGRESS,0xbf6119a4ad89ef09, CommandIsStuntJumpInProgress);
	SCR_REGISTER_SECURE(IS_STUNT_JUMP_MESSAGE_SHOWING,0x70843a833fda0155, CommandIsStuntJumpMessageShowing);
	SCR_REGISTER_SECURE(GET_NUM_SUCCESSFUL_STUNT_JUMPS,0xc543ff0fe8bf24ac, CommandGetNumSuccessfulStuntJumps);
	SCR_REGISTER_SECURE(GET_TOTAL_SUCCESSFUL_STUNT_JUMPS,0x59de6e7fa1beff04, CommandGetTotalSuccessfulStuntJumps);
	SCR_REGISTER_SECURE(CANCEL_STUNT_JUMP,0x8a5171986f5357d4, CommandCancelStuntJump);
	SCR_REGISTER_SECURE(SET_GAME_PAUSED,0x27f66b7fe7db7c3a, CommandSetGamePaused);
	SCR_REGISTER_SECURE(SET_THIS_SCRIPT_CAN_BE_PAUSED,0x2c07cbafac54a645, CommandAllowThisScriptToBePaused);
	SCR_REGISTER_SECURE(SET_THIS_SCRIPT_CAN_REMOVE_BLIPS_CREATED_BY_ANY_SCRIPT,0xe2b3315f89fb203d, CommandSetThisScriptCanRemoveBlipsCreatedByAnyScript);
	SCR_REGISTER_UNUSED(CLEAR_NEWS_SCROLLBAR,0xf7af2da93489cc3e, CommandClearNewsScrollbar);
	SCR_REGISTER_UNUSED(ADD_STRING_TO_NEWS_SCROLLBAR,0x83982aa807758dad, CommandAddStringToNewsScrollbar);
	SCR_REGISTER_UNUSED(SET_CHEAT_ACTIVE,0xd41dbda3fabb3c5f, CommandActivateCheat);
	SCR_REGISTER_UNUSED(HAS_CHEAT_BEEN_ACTIVATED,0x5b4dd99aedcb74c9, CommandHasCheatBeenActivated);
	SCR_REGISTER_SECURE(HAS_CHEAT_WITH_HASH_BEEN_ACTIVATED,0x549ecfc9f246e127, CommandHasCheatWithHashBeenActivated);
	SCR_REGISTER_SECURE(HAS_PC_CHEAT_WITH_HASH_BEEN_ACTIVATED,0x21d2b24a19ad8996, CommandHasPcCheatWithHashBeeonActivated);
	SCR_REGISTER_SECURE(OVERRIDE_FREEZE_FLAGS,0xa404d3b3cbec1144, CommandOverrideFreezeFlags);

	SCR_REGISTER_UNUSED(SET_GLOBAL_INSTANCE_PRIORITY,0xfde10efcb8697073, CommandSetGlobalInstancePriority);
	SCR_REGISTER_UNUSED(SET_DEFAULT_GLOBAL_INSTANCE_PRIORITY,0xa2466e3a9ed547b7, CommandSetDefaultGlobalInstancePriority);
	SCR_REGISTER_SECURE(SET_INSTANCE_PRIORITY_MODE,0x0da2849a157f1c01, CommandSetInstancePriorityMode);
	SCR_REGISTER_SECURE(SET_INSTANCE_PRIORITY_HINT,0x9b8a09aad1cfefb3, CommandSetInstancePriorityHint);

	SCR_REGISTER_UNUSED(SET_XBOX_SCREEN_SAVER,0x34a435880aac72ce, CommandEnableXboxScreenSaver);
	SCR_REGISTER_SECURE(IS_FRONTEND_FADING,0x6304243969f5a11a, CommandIsFrontendFading);
	SCR_REGISTER_SECURE(POPULATE_NOW,0x466bf1f57ab05359, CommandPopulateNow);
	SCR_REGISTER_SECURE(GET_INDEX_OF_CURRENT_LEVEL,0x33d66016fe7b1f4c, CommandGetIndexOfCurrentLevel);
	SCR_REGISTER_UNUSED(SWITCH_TO_LEVEL,0x0d736c2a8021511c, CommandSwitchToLevel);
//	SCR_REGISTER_UNUSED(REGISTER_PERSISTENT_GLOBAL_VARIABLES, 0x42830d95, CommandRegisterPersistentGlobalVariables);
	SCR_REGISTER_SECURE(SET_GRAVITY_LEVEL,0x29f437c0738a11d6, CommandSetGravityLevel);

	SCR_REGISTER_SECURE(START_SAVE_DATA,0x6a3cf241a8e6bb10, CommandStartSaveData);
	SCR_REGISTER_SECURE(STOP_SAVE_DATA,0x6be88f905e2d4a46, CommandStopSaveData);
	SCR_REGISTER_SECURE(GET_SIZE_OF_SAVE_DATA,0xc42fc514879fbd37, CommandGetSizeOfSaveData);
	SCR_REGISTER_SECURE(REGISTER_INT_TO_SAVE,0x761051927661f94b, CommandRegisterIntToSave);
	SCR_REGISTER_SECURE(REGISTER_INT64_TO_SAVE,0x8ee599308657b2a9, CommandRegisterInt64ToSave);
	SCR_REGISTER_SECURE(REGISTER_ENUM_TO_SAVE,0x9702f50cca480d02, CommandRegisterEnumToSave);
	SCR_REGISTER_SECURE(REGISTER_FLOAT_TO_SAVE,0xd9ec39a4185f1a0f, CommandRegisterFloatToSave);
	SCR_REGISTER_SECURE(REGISTER_BOOL_TO_SAVE,0xf0d5ec209bfbceb4, CommandRegisterBoolToSave);
	SCR_REGISTER_SECURE(REGISTER_TEXT_LABEL_TO_SAVE,0x3f5eb2069155d1b2, CommandRegisterTextLabelToSave);
	SCR_REGISTER_UNUSED(REGISTER_TEXT_LABEL_3_TO_SAVE,0x3f0600d8ea048c7e, CommandRegisterTextLabel3ToSave);
	SCR_REGISTER_UNUSED(REGISTER_TEXT_LABEL_7_TO_SAVE,0x425c2a67015424cd, CommandRegisterTextLabel7ToSave);
	SCR_REGISTER_SECURE(REGISTER_TEXT_LABEL_15_TO_SAVE,0x4cf17ce9e4568a82, CommandRegisterTextLabel15ToSave);
	SCR_REGISTER_SECURE(REGISTER_TEXT_LABEL_23_TO_SAVE,0xd8129bfbc7574726, CommandRegisterTextLabel23ToSave);
	SCR_REGISTER_SECURE(REGISTER_TEXT_LABEL_31_TO_SAVE,0xb55ff6a2f8dcf841, CommandRegisterTextLabel31ToSave);
	SCR_REGISTER_SECURE(REGISTER_TEXT_LABEL_63_TO_SAVE,0x76c51166efaffcf8, CommandRegisterTextLabel63ToSave);
	SCR_REGISTER_SECURE(START_SAVE_STRUCT_WITH_SIZE,0x35ef5a3254a1ae34, CommandStartSaveStructWithSize);
	SCR_REGISTER_SECURE(STOP_SAVE_STRUCT,0xa78477896ddce3c5, CommandStopSaveStruct);
	SCR_REGISTER_SECURE(START_SAVE_ARRAY_WITH_SIZE,0xc6882a6353b4adb3, CommandStartSaveArrayWithSize);
	SCR_REGISTER_SECURE(STOP_SAVE_ARRAY,0xbda66a8b530a60ab, CommandStopSaveArray);
	
	SCR_REGISTER_SECURE(COPY_SCRIPT_STRUCT,0x049557638ade4df8, CommandCopyScriptStruct);

	SCR_REGISTER_SECURE(ENABLE_DISPATCH_SERVICE,0x07cecf421d895f3d, CommandEnableDispatchService);
	SCR_REGISTER_SECURE(BLOCK_DISPATCH_SERVICE_RESOURCE_CREATION,0x5258fce0ec176e04, CommandBlockDispatchServiceResourceCreation);
	SCR_REGISTER_SECURE(GET_NUMBER_RESOURCES_ALLOCATED_TO_WANTED_LEVEL,0x75eff7187a7d4dc4, CommandGetNumberResourcesAllocatedToWantedLevel);
	
	
	SCR_REGISTER_SECURE(CREATE_INCIDENT,0x302d843e3c8f40e2, CommandCreateIncident);
	SCR_REGISTER_SECURE(CREATE_INCIDENT_WITH_ENTITY,0x5d5732191056751c, CommandCreateIncidentWithEntity);
	SCR_REGISTER_SECURE(DELETE_INCIDENT,0xa4d968c5e2a5d0a0, CommandDeleteIncident);
	SCR_REGISTER_SECURE(IS_INCIDENT_VALID,0x99b99da99fb0d4ec, CommandIsIncidentValid);
	SCR_REGISTER_SECURE(SET_INCIDENT_REQUESTED_UNITS,0x337e4f61a2792efe, CommandSetIncidentRequestedUnits);
	SCR_REGISTER_SECURE(SET_IDEAL_SPAWN_DISTANCE_FOR_INCIDENT,0xb4ee171ee2b772bf, CommandSetIdealSpawnDistanceForIncident);

	SCR_REGISTER_SECURE(FIND_SPAWN_POINT_IN_DIRECTION,0xed142878c9f94e2f, CommandFindSpawnPointInDirection);

	SCR_REGISTER_SECURE(ADD_POP_MULTIPLIER_AREA,0x7cc9abaffea56277, CommandAddPopMultiplierArea);
	SCR_REGISTER_SECURE(DOES_POP_MULTIPLIER_AREA_EXIST,0x4b4bc360c6d8d2a4, CommandDoesPopMultiplierAreaExist);
	SCR_REGISTER_SECURE(REMOVE_POP_MULTIPLIER_AREA,0x79d94867e24afb8a, CommandRemovePopDensityMultiplierArea);
	SCR_REGISTER_SECURE(IS_POP_MULTIPLIER_AREA_NETWORKED,0x2ccebb043649a456, CommandIsPopDensityMultiplierAreaNetworked);
	

	SCR_REGISTER_SECURE(ADD_POP_MULTIPLIER_SPHERE,0x9c5e0d0887ecd76d, CommandAddPopMultiplierSphere);
	SCR_REGISTER_SECURE(DOES_POP_MULTIPLIER_SPHERE_EXIST,0x9638d57f9a8decf9, CommandDoesPopMultiplierSphereExist);
	SCR_REGISTER_SECURE(REMOVE_POP_MULTIPLIER_SPHERE,0xf63317d708095ce2, CommandRemovePopDensityMultiplierSphere);


	//Mini game support
	SCR_REGISTER_SECURE(ENABLE_TENNIS_MODE,0x6535edbe6f8dbb11, CommandEnableTennisMode);
	SCR_REGISTER_UNUSED(ENABLE_CLONE_OVERRIDE_TENNIS_MODE,0x88c77cbe7aeff1c0, CommandCloneOverrideTennisMode);
	SCR_REGISTER_SECURE(IS_TENNIS_MODE,0x4b383368dc8ccf38, CommandIsInTennisMode);	
	SCR_REGISTER_SECURE(PLAY_TENNIS_SWING_ANIM,0xa427179dc0581389, CommandPlayTennisSwingAnim);	
	SCR_REGISTER_UNUSED(SET_TENNIS_SWING_ANIM_PLAY_RATE,0xf654a9796bd73ae2, CommandSetTennisSwingAnimPlayRate);	
	SCR_REGISTER_SECURE(GET_TENNIS_SWING_ANIM_COMPLETE,0x75c0723cf45e484f, CommandGetTennisSwingAnimComplete);	
	SCR_REGISTER_SECURE(GET_TENNIS_SWING_ANIM_CAN_BE_INTERRUPTED,0x6460d686d9ea3ea9, CommandTennisSwingAnimCanBeInterrupted);	
	SCR_REGISTER_SECURE(GET_TENNIS_SWING_ANIM_SWUNG,0x061730bd8d533982, CommandTennisSwingAnimSwung);	
	SCR_REGISTER_UNUSED(SET_TENNIS_ALLOW_DAMPEN_STRAFE_DIRECTION,0x6903eb81d900e828, CommandSetTennisAllowDampenStrafeDirection);	
	SCR_REGISTER_SECURE(PLAY_TENNIS_DIVE_ANIM,0x08c0f779422456f1, CommandPlayTennisDiveAnim);	
	SCR_REGISTER_SECURE(SET_TENNIS_MOVE_NETWORK_SIGNAL_FLOAT,0x5f64484476f8387b, CommandSetTennisMoveNetworkSignalFloat);	
	
	//Dispatch
	SCR_REGISTER_SECURE(RESET_DISPATCH_SPAWN_LOCATION,0x9979f63cd1e2927a, CommandResetDispatchSpawnLocation);
	SCR_REGISTER_SECURE(SET_DISPATCH_SPAWN_LOCATION,0x1eea2d366d6c2406, CommandSetDispatchSpawnLocation);
	SCR_REGISTER_SECURE(RESET_DISPATCH_IDEAL_SPAWN_DISTANCE,0x34a2da203a9abc58, CommandResetDispatchIdealSpawnDistance);
	SCR_REGISTER_SECURE(SET_DISPATCH_IDEAL_SPAWN_DISTANCE,0xb0520b53aa49f54f, CommandSetDispatchIdealSpawnDistance);
	SCR_REGISTER_SECURE(RESET_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS,0x82ec6fa179cc6599, CommandResetDispatchTimeBetweenSpawnAttempts);
	SCR_REGISTER_SECURE(SET_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS,0xd1f5efbaf2ec7b47, CommandSetDispatchTimeBetweenSpawnAttempts);
	SCR_REGISTER_UNUSED(RESET_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER,0xe6ab6f0e5e7e8f3d, CommandResetDispatchTimeBetweenSpawnAttemptsMultiplier);
	SCR_REGISTER_SECURE(SET_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER,0x8df9d06d24867ef7, CommandSetDispatchTimeBetweenSpawnAttemptsMultiplier);
	SCR_REGISTER_SECURE(ADD_DISPATCH_SPAWN_ANGLED_BLOCKING_AREA,0x40b95e12f9a36a28, CommandAddDispatchSpawnAngledBlockingArea);
	SCR_REGISTER_SECURE(ADD_DISPATCH_SPAWN_SPHERE_BLOCKING_AREA,0x903a26ef102dab53, CommandAddDispatchSpawnSphereBlockingArea);
	SCR_REGISTER_SECURE(REMOVE_DISPATCH_SPAWN_BLOCKING_AREA,0xd41a5dca42ba8bdd, CommandRemoveDispatchSpawnBlockingArea);
	SCR_REGISTER_SECURE(RESET_DISPATCH_SPAWN_BLOCKING_AREAS,0x77be4f11ef7fc8c7, CommandResetDispatchSpawnBlockingAreas);

	//Wanted Response
	SCR_REGISTER_SECURE(RESET_WANTED_RESPONSE_NUM_PEDS_TO_SPAWN,0x777e9192665d1ca2, CommandResetWantedResponseNumPedsToSpawn);
	SCR_REGISTER_SECURE(SET_WANTED_RESPONSE_NUM_PEDS_TO_SPAWN,0x4f0c3dda5af5a24f, CommandSetWantedResponseNumPedsToSpawn);

	//Tactical Analysis
	SCR_REGISTER_SECURE(ADD_TACTICAL_NAV_MESH_POINT,0xac5d4c07ad99da31, CommandAddTacticalNavMeshPoint);
	SCR_REGISTER_SECURE(CLEAR_TACTICAL_NAV_MESH_POINTS,0x3cde5c5113b2a756, CommandClearTacticalNavMeshPoints);

	//Riot Mode
	SCR_REGISTER_SECURE(SET_RIOT_MODE_ENABLED,0xf542970145bc96b5, CommandSetRiotModeEnabled);

	//On-screen Keyboard Support
	SCR_REGISTER_SECURE(DISPLAY_ONSCREEN_KEYBOARD_WITH_LONGER_INITIAL_STRING,0x9f75296be5b9e406, CommandDisplayOnScreenKeyboardWithLongerInitialString);
	SCR_REGISTER_SECURE(DISPLAY_ONSCREEN_KEYBOARD,0xd36afc4a86165b46, CommandDisplayOnScreenKeyboard);
	SCR_REGISTER_SECURE(UPDATE_ONSCREEN_KEYBOARD,0x9417f1137725b4b3, CommandUpdateOnScreenKeyboard);
	SCR_REGISTER_SECURE(GET_ONSCREEN_KEYBOARD_RESULT,0xd0cd6f19a5cd39ec, CommandGetOnScreenKeyboardResult);
	SCR_REGISTER_SECURE(CANCEL_ONSCREEN_KEYBOARD,0xa4d40100016b1da1, CommandCancelOnScreenKeyboard);
	SCR_REGISTER_UNUSED(SET_ALLOW_TILDE_CHARACTER_FROM_ONSCREEN_KEYBOARD,0x27a5cdc282d09650, CommandSetAllowTildeCharacterFromOnScreenKeyboard);

	SCR_REGISTER_SECURE(NEXT_ONSCREEN_KEYBOARD_RESULT_WILL_DISPLAY_USING_THESE_FONTS,0xaebb4e3408e9c382, CommandNextOnScreenKeyboardResultWillDisplayUsingTheseFonts);

	// Action Manager
	SCR_REGISTER_SECURE(ACTION_MANAGER_ENABLE_ACTION,0x43efe7c11330165e, CommandEnableAction);
	SCR_REGISTER_UNUSED(ACTION_MANAGER_IS_ACTION_ENABLED,0xd12fee6181945623, CommandIsActionEnabled);


	SCR_REGISTER_SECURE(GET_REAL_WORLD_TIME,0x3f60413f5df65748, CommandGetRealWorldTime );

	//Random event
	SCR_REGISTER_SECURE(SUPRESS_RANDOM_EVENT_THIS_FRAME,0xbd9c95510c697303, CommandSetRandomEventEnabled );

	//Cheats
	SCR_REGISTER_SECURE(SET_EXPLOSIVE_AMMO_THIS_FRAME,0x582ef587cc7597ae, CommandSetExplosiveAmmoThisFrame );
	SCR_REGISTER_SECURE(SET_FIRE_AMMO_THIS_FRAME,0x7fe655ce221e741b, CommandSetFireAmmoThisFrame );
	SCR_REGISTER_SECURE(SET_EXPLOSIVE_MELEE_THIS_FRAME,0xc3562ba50c82388b, CommandSetExplosiveMeleeThisFrame );
	SCR_REGISTER_SECURE(SET_SUPER_JUMP_THIS_FRAME,0xa39a115964cb8d41, CommandSetSuperJumpThisFrame );
	SCR_REGISTER_UNUSED(GET_IS_SUPER_JUMP_ACTIVE,0x4859abb912d14d99, CommandGetIsSuperJumpActive );
	SCR_REGISTER_SECURE(SET_BEAST_JUMP_THIS_FRAME,0xf85eb82eef0fad32, CommandSetBeastJumpThisFrame );
	SCR_REGISTER_UNUSED(GET_IS_BEAST_JUMP_ACTIVE,0x172607fb83e84f65, CommandGetIsBeastJumpActive );

	SCR_REGISTER_SECURE(SET_FORCED_JUMP_THIS_FRAME,0xd097da9bfe7a2016, CommandSetForcedJumpThisFrame );

	SCR_REGISTER_UNUSED(WAIT_UNTIL_GAME_INSTALLED,0x7f60cd94039a85ea, CommandWaitUntilGameInstalled);
	SCR_REGISTER_SECURE(HAS_GAME_INSTALLED_THIS_SESSION,0x05c8821cd3e101ba, CommandHasGameInstalledThisSession);


	SCR_REGISTER_SECURE(SET_TICKER_JOHNMARSTON_IS_DONE,0x48d794795c819e09, CommandSetTickerJohnMarstonIsDone);
	SCR_REGISTER_SECURE(ARE_PROFILE_SETTINGS_VALID,0xb8f4926b803bfd19, CommandAreProfileSettingsValid);

	SCR_REGISTER_SECURE(PREVENT_ARREST_STATE_THIS_FRAME,0x1a4209996802ef21, CommandPreventArrestStateThisFrame);
	SCR_REGISTER_SECURE(FORCE_GAME_STATE_PLAYING,0x956030e0596c0f3e, CommandForceStatePlaying);

	SCR_REGISTER_SECURE(SCRIPT_RACE_INIT,0xb209dde44ec31285, CommandScriptRaceInit);
	SCR_REGISTER_SECURE(SCRIPT_RACE_SHUTDOWN,0x21177c558350f942, CommandScriptRaceShutdown);
	SCR_REGISTER_SECURE(SCRIPT_RACE_PLAYER_HIT_CHECKPOINT,0x8b131ad9ca84bce8, CommandScriptRacePlayerHitCheckpoint);
	SCR_REGISTER_SECURE(SCRIPT_RACE_GET_PLAYER_SPLIT_TIME,0xeb390afeb597210e, CommandScriptRaceGetPlayerSplitTime);

	// End User Benchmarks
	SCR_REGISTER_SECURE(START_END_USER_BENCHMARK,0xfdb4b0657fc92719, CommandStartEndUserBenchmark);
	SCR_REGISTER_SECURE(STOP_END_USER_BENCHMARK,0x125d159abd01a279, CommandStopEndUserBenchmark);
	SCR_REGISTER_SECURE(RESET_END_USER_BENCHMARK,0x16c1baaa79abea83, CommandResetEndUserBenchmarks);
	SCR_REGISTER_SECURE(SAVE_END_USER_BENCHMARK,0xd6cf1348fbd2f71a, CommandSaveEndUserBenchmarks);
	SCR_REGISTER_SECURE(UI_STARTED_END_USER_BENCHMARK,0xe76e60fda68de799, CommandDidUIStartEndUserBenchmarks);
	SCR_REGISTER_SECURE(LANDING_SCREEN_STARTED_END_USER_BENCHMARK,0xed419b5d18c4aa5a, CommandDidLandingScreenStartEndUserBenchmarks);
	SCR_REGISTER_SECURE(IS_COMMANDLINE_END_USER_BENCHMARK,0x22f20e950399daed, CommandEndUserBenchmarkCommandline);
	SCR_REGISTER_SECURE(GET_BENCHMARK_ITERATIONS,0xbeae7b00e881ae3e, CommandGetBenchmarkIterationCount);
	SCR_REGISTER_SECURE(GET_BENCHMARK_PASS,0xf6d0f47e80cff135, CommandGetBenchmarkPassCount);

	SCR_REGISTER_SECURE(RESTART_GAME,0xeba933b1a2380569, CommandRestartGame);
	SCR_REGISTER_SECURE(QUIT_GAME,0x315551089dac8c73, CommandQuitGame);

    SCR_REGISTER_SECURE(HAS_ASYNC_INSTALL_FINISHED,0xda2d2333ed6bb2c1, CommandHasAsyncInstallFinished);
    SCR_REGISTER_SECURE(CLEANUP_ASYNC_INSTALL,0xeac0d007daaa67a6, CommandCleanupAsyncInstall);

	// PLM (Process Lifetime Management)
	SCR_REGISTER_SECURE(PLM_IS_IN_CONSTRAINED_MODE,0xaf62a965ebf9946a, CommandPlmIsInConstrainedMode);
	SCR_REGISTER_SECURE(PLM_GET_CONSTRAINED_DURATION_MS,0xfe3098cc7933c733, CommandPlmGetConstrainedDurationMs);

	SCR_REGISTER_SECURE(SET_PLAYER_IS_IN_ANIMAL_FORM,0x608a0ddf19c87adf, CommandSetPlayerIsInAnimalForm);
	SCR_REGISTER_SECURE(GET_IS_PLAYER_IN_ANIMAL_FORM,0x80f6e0a3b422f9fd, CommandGetPlayerIsInAnimalForm);

	SCR_REGISTER_SECURE(SET_PLAYER_IS_REPEATING_A_MISSION,0x004d0da56b6ff87a, CommandSetPlayerIsRepeatingAMission);

	// Stop screen dimming.
	SCR_REGISTER_SECURE(DISABLE_SCREEN_DIMMING_THIS_FRAME,0x21c4591c6022e9f3, CommandDisableScreenDimmingThisFrame);

	// Log file
	SCR_REGISTER_UNUSED(GET_LOG_FILE_NUMBER,0x64eb00f8e968882d, CommandGetLogFileNumber);

	//Get The Density of the city
	SCR_REGISTER_SECURE(GET_CITY_DENSITY,0x5cc952a51a751c4c, CommandGetCityDensity);

	//Set anim timeslicing score centre to use the camera, rather than population centre
	SCR_REGISTER_SECURE(USE_ACTIVE_CAMERA_FOR_TIMESLICING_CENTRE,0x6a9ec5834979e98a, CommandUseActiveCameraForTimeslicingCentre);

	// script router navigation
#if GEN9_STANDALONE_ENABLED
	SCR_REGISTER_UNUSED(GET_SCRIPT_ROUTER_CONTEXT,0xd3b3a6d194fb6d3b, CommandGetScriptRouterContextData);
	SCR_REGISTER_UNUSED(SET_SCRIPT_ROUTER_LINK,0x5d22dae27ef205b3, CommandSetScriptRouterLink);
	SCR_REGISTER_UNUSED(GET_SCRIPT_ROUTER_LINK_HASH,0x8267ec27aa1bb696, CommandGetScriptRouterLinkHash);
	SCR_REGISTER_UNUSED(HAS_PENDING_SCRIPT_ROUTER_LINK,0x15aad97a998e81ba, CommandHasPendingScriptRouterLink);
	SCR_REGISTER_UNUSED(CLEAR_SCRIPT_ROUTER_LINK,0x63d53e11c1c1cce3, CommandClearScriptRouterLink);
	SCR_REGISTER_UNUSED(REPORT_INVALID_SCRIPT_ROUTER_ARGUMENT, 0x87acffe5c7610714, CommandReportInvalidScriptRouterArgument);

	SCR_REGISTER_UNUSED(SET_ACTIVITY_SCRIPT_ROUTING_ENABLED, 0xa4523cd8e76ff12d, CommandSetActivityScriptRoutingEnabled);
	SCR_REGISTER_UNUSED(IS_SESSION_INITIALIZED,0xd4073af8bf125a14, CommandIsSessionInitialized);
#endif

#if GEN9_STANDALONE_ENABLED
	SCR_REGISTER_UNUSED(GET_CHOSEN_CRIMINAL_CAREER,0x088de8e8e8f522dd, CommandGetChosenCriminalCareer);
	SCR_REGISTER_UNUSED(HAS_FINALIZED_CHOSEN_CRIMINAL_CAREER, 0x543cb61641ac456e, CommandHasFinalizedChosenCriminalCareer);
	SCR_REGISTER_UNUSED(GET_CHOSEN_MP_CHARACTER_SLOT,0xf7d89f7e3ff81e21, CommandGetChosenMpCharacterSlot);
	SCR_REGISTER_UNUSED(RESET_CHOSEN_MP_CHARACTER_SLOT, 0xff018aa297325503, CommandResetChosenMpCharacterSlot);
#endif // GEN9_STANDALONE_ENABLED

	SCR_REGISTER_SECURE(SET_CONTENT_ID_INDEX, 0x4b82fa6f2d624634, CommandSetContentIdIndex);
	SCR_REGISTER_SECURE(GET_CONTENT_ID_INDEX, 0xecf041186c5a94dc, CommandGetContentIdIndex);
}

}	//	end of namespace misc_commands
