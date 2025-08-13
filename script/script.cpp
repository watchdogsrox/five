
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    script.cpp
// PURPOSE : Scripting and stuff.
// AUTHOR :  Graeme & Obbe.
//			 anyway)
// CREATED : 12-11-99
//
/////////////////////////////////////////////////////////////////////////////////

#include "script/script.h"

// Rage headers
#include "audioengine/engine.h"
#include "system/param.h"
#include "vector/quaternion.h"

// Framework headers
#include "fwnet/nettypes.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscript/scriptInterface.h"

// Security headers
#include "security/ragesecwinapi.h"

// Game headers
#include "animation/AnimScene/AnimSceneManager.h"
#include "audio/scriptaudioentity.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/BugstarIntegration.h"
#include "event/EventErrors.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/landing_page/LandingPageConfig.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "peds/ped.h"
#include "peds/Ped.h"
#include "scene/MapChange.h"
#include "scene/ExtraContentDefs.h"
#include "scene/worldpoints.h"
#include "scene/world/GameWorld.h"
#include "script/commands_apps.h"
#include "script/commands_audio.h"
#include "script/commands_brains.h"
#include "script/commands_camera.h"
#include "script/commands_cutscene.h"
#include "script/commands_datafile.h"
#include "script/commands_debug.h"
#include "script/commands_decorator.h"
#include "script/commands_extrametadata.h"
#include "script/commands_entity.h"
#include "script/commands_event.h"
#include "script/commands_gamestream.h"
#include "script/commands_gang.h"
#include "script/commands_hud.h"
#include "script/commands_interiors.h"
#include "script/commands_itemsets.h"
#include "script/commands_landingpage.h"
#include "script/commands_law.h"
#include "script/commands_localization.h"
#include "script/commands_misc.h"
#include "script/commands_money.h"
#include "script/commands_object.h"
#include "script/commands_pad.h"
#include "script/commands_path.h"
#include "script/commands_ped.h"
#include "script/commands_player.h"
#include "script/commands_physics.h"
#include "script/commands_script.h"
#include "script/commands_security.h"
#include "script/commands_shapetest.h"
#include "script/commands_socialclub.h"
#include "script/commands_stats.h"
#include "script/commands_streaming.h"
#include "script/commands_task.h"
#include "script/commands_vehicle.h"
#include "script/commands_volume.h"
#include "script/commands_weapon.h"
#include "script/commands_fire.h"
#include "script/commands_zone.h"
#include "script/commands_graphics.h"
#include "script/commands_clock.h"
#include "script/commands_network.h"
#include "script/commands_lobby.h"
#include "script/commands_inventory.h"
#include "script/commands_water.h"
#include "script/commands_xml.h"
#include "script/commands_ny.h"
#include "script/commands_dlc.h"
#include "script/commands_recording.h"
#include "script/commands_replay.h"
#include "script/commands_netshopping.h"
#include "script/commands_savemigration.h"
#include "script/script_areas.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_hud.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script_helper.h"
#include "script/streamedscripts.h"
#include "script/script_channel.h"
#include "script/ScriptMetadataManager.h"
#include "security/plugins/apicheckplugin.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "system/exception.h"
#include "system/FileMgr.h"
#include "system/hangdetect.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "vfx/misc/MovieMeshManager.h"
#include "control/replay/replay.h"

SCRIPT_OPTIMISATIONS ()
NETWORK_OPTIMISATIONS ()

PARAM(startupscript, "[script] load and run this script in place of startup.sc");

PARAM(scriptInstructionLimit, "[script] set the maximum number of instructions per frame to something other than the default of 1,000,000");

PARAM(scriptProtectGlobals, "[script] protects scripts global memory outide of the script update process");

//Define a script assert channel
RAGE_DEFINE_CHANNEL(script)
RAGE_DEFINE_SUBCHANNEL(script,hud)
RAGE_DEFINE_SUBCHANNEL(script,camera)

CScriptsForBrains CTheScripts::ms_ScriptsForBrains;
CScriptBrainDispatcher CTheScripts::ms_ScriptBrainDispatcher;

bool CTheScripts::ms_bPlayerIsOnAMission = false;
bool CTheScripts::ms_bPlayerIsOnARandomEvent = false;
u32 CTheScripts::ms_Frack = 0xEBEFC45E;
parTree* CTheScripts::ms_pXmlTree = NULL;
parTreeNode* CTheScripts::ms_pCurrentXmlTreeNode = NULL;
bool CTheScripts::ms_bUpdatingScriptThreads = false;
bool CTheScripts::ms_bPlayerIsRepeatingAMission = false;
bool CTheScripts::ms_bPlayerIsInAnimalForm = false;
bool CTheScripts::ms_bIsInDirectorMode = false;
bool CTheScripts::ms_directorModeAvailable = true;
s32 CTheScripts::ms_customMPHudColor = HUD_COLOUR_INVALID;

CScriptPatrol CTheScripts::ms_ScriptPatrol;

CMissionReplayStats CTheScripts::ms_MissionReplayStats;

CHiddenObjects CTheScripts::ms_HiddenObjects;

atBinaryMap<CDLCScript*,u32> CDLCScript::ms_scripts;


#if __DEV
static bool bDisplayUsedScriptResources = false;
#endif

int CTheScripts::ms_NumberOfMiniGamesInProgress = 0;

#if !__PPU
bool CTheScripts::ms_bSinglePlayerRestoreHasJustOccurred = false;
#endif

#if __BANK
f32 CTheScripts::sm_fTimeFadedOut = 0.0f;
#endif	//	__BANK

#if !__FINAL
bool CTheScripts::ms_bScriptLaunched = false;
#endif //!__FINAL

sysTimer CTheScripts::sm_ScriptTimer;

char CTheScripts::ms_ContentIdOfCurrentUGCMission[RLUGC_MAX_CONTENTID_CHARS];

bool CTheScripts::ms_bAllowGameToPauseForStreaming = true;
bool CTheScripts::ms_bScenarioPedsCanBeGrabbedByScript = false;
bool CTheScripts::ms_bCodeRequestedAutosave = false;
bool CTheScripts::ms_bProcessingTheScriptsDuringGameInit = false;
CGameScriptHandlerMgr CTheScripts::ms_ScriptHandlerMgr(&CNetwork::GetBandwidthManager());
bool CTheScripts::ms_bKillingAllScriptThreads = false;
CScriptOpList CTheScripts::ms_scriptOpList;

static int s_InstructionLimit = 0;

GEN9_STANDALONE_ONLY(CTheScripts::InitState CTheScripts::ms_initState = CTheScripts::InitState::Uninitialized);

bool CTheScripts::GetClosestObjectCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	ClosestObjectDataStruct* pClosestObjectData = reinterpret_cast<ClosestObjectDataStruct*>(data);

	if (pClosestObjectData->bCheckModelIndex && (pEntity->GetModelIndex() != pClosestObjectData->ModelIndexToCheck) )
	{
		return true;
	}

	if (pClosestObjectData->bCheckStealableFlag)
	{
		scriptAssertf(pEntity->GetIsTypeObject(), "%s:GetClosestObjectCB - Object should be a real object (not a dummy) if you're checking if it's stealable. This is a code problem (probably not a script problem)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (pEntity->GetIsTypeObject())
		{
			CObject *pObj = (CObject *) pEntity;
			if (pObj->m_nObjectFlags.bIsStealable == false)
			{
				return true;
			}
		}
	}

	if(!((1 << pEntity->GetOwnedBy()) & pClosestObjectData->nOwnedByMask))
	{
		return true;
	}

	Vector3 DiffVector = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - pClosestObjectData->CoordToCalculateDistanceFrom;
	float ObjDistanceSquared = DiffVector.Mag2();
	if (ObjDistanceSquared < pClosestObjectData->fClosestDistanceSquared)
	{
		pClosestObjectData->pClosestObject = pEntity;
		pClosestObjectData->fClosestDistanceSquared = ObjDistanceSquared;
	}

	return true;
}


bool CTheScripts::CountMissionEntitiesInAreaCB(CEntity* pEntity, void* data)
{
	int* pNumberOfMissionEntities = reinterpret_cast<int*>(data);

	if (IsMissionEntity(pEntity))
		(*pNumberOfMissionEntities)++;

	return(true);
}

bool CTheScripts::IsMissionEntity(CEntity * pEntity)
{
	Assert(pEntity);
	switch (pEntity->GetType())
	{
	case ENTITY_TYPE_VEHICLE :
		if (((CVehicle *)pEntity)->PopTypeIsMission())
		{
			return true;
		}
		break;

	case ENTITY_TYPE_PED :
		if (((CPed *)pEntity)->PopTypeIsMission())
		{
			return true;
		}
		break;

	case ENTITY_TYPE_OBJECT :
		if (((CObject *)pEntity)->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)	//	|| (((CObject *)pEntity)->GetOwnedBy() == MISSION_BRAIN_OBJECT) )
		{
			return true;
		}
		break;

	default :
		break;
	}
	return false;
}

extern scrCommandHash<scrCmd> s_CommandHash;
typedef void (*regFunction) ();

void CTheScripts::ScrambleAndRegister()
{
	atArray<regFunction> sourceArray;
	sourceArray.Reserve(100);

	sourceArray.Push(netshopping_commands::SetupScriptCommands);
	sourceArray.Push(script_commands::SetupScriptCommands);
	sourceArray.Push(object_commands::SetupScriptCommands);
	sourceArray.Push(xml_commands::SetupScriptCommands);
	sourceArray.Push(volume_commands::SetupScriptCommands);
	sourceArray.Push(brain_commands::SetupScriptCommands);
	sourceArray.Push(replay_commands::SetupScriptCommands);
	sourceArray.Push(weapon_commands::SetupScriptCommands);
	sourceArray.Push(apps_commands::SetupScriptCommands);
	sourceArray.Push(ped_commands::SetupScriptCommands);
	sourceArray.Push(fire_commands::SetupScriptCommands);
	sourceArray.Push(clock_commands::SetupScriptCommands);
	sourceArray.Push(interior_commands::SetupScriptCommands);
	sourceArray.Push(pad_commands::SetupScriptCommands);
	sourceArray.Push(graphics_commands::SetupScriptCommands);
	sourceArray.Push(inventory_commands::SetupScriptCommands);
	sourceArray.Push(localization_commands::SetupScriptCommands);
	sourceArray.Push(player_commands::SetupScriptCommands);
	sourceArray.Push(physics_commands::SetupScriptCommands);
	sourceArray.Push(entity_commands::SetupScriptCommands);
	sourceArray.Push(task_commands::SetupScriptCommands);
	sourceArray.Push(zone_commands::SetupScriptCommands);
	sourceArray.Push(misc_commands::SetupScriptCommands);
	sourceArray.Push(streaming_commands::SetupScriptCommands);
	sourceArray.Push(socialclub_commands::SetupScriptCommands);
	sourceArray.Push(law_commands::SetupScriptCommands);
	sourceArray.Push(audio_commands::SetupScriptCommands);
	sourceArray.Push(gta_commands::SetupScriptCommands);
	sourceArray.Push(path_commands::SetupScriptCommands);
	sourceArray.Push(itemsets_commands::SetupScriptCommands);
	sourceArray.Push(datafile_commands::SetupScriptCommands);
	sourceArray.Push(dlc_commands::SetupScriptCommands);
	sourceArray.Push(cutscene_commands::SetupScriptCommands);
	sourceArray.Push(stats_commands::SetupScriptCommands);
	sourceArray.Push(shapetest_commands::SetupScriptCommands);
	sourceArray.Push(recording_commands::SetupScriptCommands);
	sourceArray.Push(extrametadata_commands::SetupScriptCommands);
	sourceArray.Push(camera_commands::SetupScriptCommands);
	sourceArray.Push(vehicle_commands::SetupScriptCommands);
	sourceArray.Push(lobby_commands::SetupScriptCommands);
	sourceArray.Push(event_commands::SetupScriptCommands);
	sourceArray.Push(water_commands::SetupScriptCommands);
	sourceArray.Push(money_commands::SetupScriptCommands);
	sourceArray.Push(network_commands::SetupScriptCommands);
	sourceArray.Push(debug_commands::SetupScriptCommands);
	sourceArray.Push(gang_commands::SetupScriptCommands);
	sourceArray.Push(decorator_commands::SetupScriptCommands);
	sourceArray.Push(hud_commands::SetupScriptCommands);
	sourceArray.Push(savemigration_commands::SetupScriptCommands);
	sourceArray.Push(landingpage_commands::SetupScriptCommands);
	sourceArray.Push(security_commands::SetupScriptCommands);

	// scramble the function array
	u32 size = sourceArray.GetCount();
	for(u32 scrambleCount = 0; scrambleCount < 1000; scrambleCount++)
	{
		u32 randomIndex = fwRandom::GetRandomNumberInRange(0, size);

		regFunction temp = sourceArray[randomIndex];
		sourceArray.DeleteFast(randomIndex);
		sourceArray.Push(temp);
	}

	// execute all the functions in the scambled array
	for(u32 i = 0; i < sourceArray.GetCount(); i++)
	{
		sourceArray[i]();
	}
	ApiCheckPlugin_StopNativeHash();

	s_CommandHash.RegistrationComplete(true);
}

void CTheScripts::RegisterScriptCommands()
{
#if  1
	ScrambleAndRegister();
#else  //
	//@@: location CTHESCRIPTS_REGISTER_SCRIPTCOMMANDS
	netshopping_commands::SetupScriptCommands();
	script_commands::SetupScriptCommands();
	object_commands::SetupScriptCommands();
	xml_commands::SetupScriptCommands();
	volume_commands::SetupScriptCommands();
	brain_commands::SetupScriptCommands();
	replay_commands::SetupScriptCommands();
	weapon_commands::SetupScriptCommands();
	apps_commands::SetupScriptCommands();
	ped_commands::SetupScriptCommands();
	fire_commands::SetupScriptCommands();
	clock_commands::SetupScriptCommands();
	interior_commands::SetupScriptCommands();
	pad_commands::SetupScriptCommands();
	graphics_commands::SetupScriptCommands();
	inventory_commands::SetupScriptCommands();
	localization_commands::SetupScriptCommands();
	player_commands::SetupScriptCommands();
	physics_commands::SetupScriptCommands();
	entity_commands::SetupScriptCommands();
	task_commands::SetupScriptCommands();
	zone_commands::SetupScriptCommands();
	misc_commands::SetupScriptCommands();
	streaming_commands::SetupScriptCommands();
	socialclub_commands::SetupScriptCommands();
	law_commands::SetupScriptCommands();
	audio_commands::SetupScriptCommands();
	gta_commands::SetupScriptCommands();
	path_commands::SetupScriptCommands();
	itemsets_commands::SetupScriptCommands();
	datafile_commands::SetupScriptCommands();
	dlc_commands::SetupScriptCommands();
	cutscene_commands::SetupScriptCommands();
	stats_commands::SetupScriptCommands();
	shapetest_commands::SetupScriptCommands();
	recording_commands::SetupScriptCommands();
	extrametadata_commands::SetupScriptCommands();
	camera_commands::SetupScriptCommands();
	vehicle_commands::SetupScriptCommands();
	lobby_commands::SetupScriptCommands();
	event_commands::SetupScriptCommands();
	water_commands::SetupScriptCommands();
	money_commands::SetupScriptCommands();
	network_commands::SetupScriptCommands();
	debug_commands::SetupScriptCommands();
	gang_commands::SetupScriptCommands();
	decorator_commands::SetupScriptCommands();
	hud_commands::SetupScriptCommands();
	savemigration_commands::SetupScriptCommands();
	landingpage_commands::SetupScriptCommands();
	security_commands::SetupScriptCommands();
#endif // 1
}


void CTheScripts::GtaFaultHandler(const char *ASSERT_ONLY(CauseOfFault), scrThread*, scrThreadFaultID faultType)
{
	if (GetCurrentGtaScriptThread())
	{
		sErrorScriptID scriptID;
		scriptID.m_scriptNameHash =	GetCurrentGtaScriptThread()->m_HashOfScriptName;

		switch(faultType)
		{
		case THREAD_ARRAY_OVERFLOW:

			GetEventScriptErrorsGroup()->Add(CEventErrorArrayOverflow(scriptID));
			break;
		case THREAD_STACK_OVERFLOW:
			GetEventScriptErrorsGroup()->Add(CEventErrorStackOverflow(scriptID));
			break;
		case THREAD_INSTRUCTION_COUNT_OVERFLOW:
			GetEventScriptErrorsGroup()->Add(CEventErrorInstructionLimit(scriptID));
			break;
		case THREAD_UNKNOWN_ERROR_TYPE:
		default:
			GetEventScriptErrorsGroup()->Add(CEventErrorUnknownError(scriptID));
			break;
		}

#if __ASSERT
		//	display the program counter of the current script here
		char ErrorString[1024];
		sprintf(ErrorString, "%s : Program Counter = %d - Check the .scd file\n", GetCurrentScriptName(), GetCurrentGtaScriptThread()->GetThreadPC());
		scriptAssertf(0, "%s:%s", ErrorString, CauseOfFault);
#endif
	}
	else
	{
		scriptAssertf(0, "%s:%s", "Unknown script name", CauseOfFault);
	}
}


// KS - WR 156 - added stacksize argument
scrThreadId CTheScripts::GtaStartNewThreadOverride(const char *progname,const void *argStruct,int argStructSize, int stackSize)
{
	if (!progname || (strlen(progname) == 0) )
	{
		if (GetCurrentGtaScriptThread())
		{
			scriptAssertf(0, "%s:CTheScripts::GtaStartNewThreadOverride - trying to start a script without passing in a script name", GetCurrentScriptNameAndProgramCounter());
		}
		else
		{
			scriptAssertf(0, "CTheScripts::GtaStartNewThreadOverride - trying to start a script without passing in a script name");
		}
	}
	scrProgramId pid = scrProgram::IsCompiledAndResident(progname);
	if (!pid)
	{
		strLocalIndex id = g_StreamedScripts.FindSlot(progname);
		scriptAssertf(id != -1, "%s:Script doesn't exist", progname);
		if (id == -1)
			return THREAD_INVALID;

		scriptAssertf(g_StreamedScripts.Get(id), "%s:Script is not in memory", progname);

		scriptDisplayf("About to start script thread %s", progname);
		pid = g_StreamedScripts.GetProgramId(id.Get());
	}

	return GtaStartNewThreadWithProgramId(pid, argStruct,argStructSize, stackSize, progname);
}

scrThreadId CTheScripts::GtaStartNewThreadWithNameHashOverride(atHashValue hashOfProgName,const void *argStruct,int argStructSize, int stackSize)
{
	if (hashOfProgName.GetHash() == 0)
	{
		if (GetCurrentGtaScriptThread())
		{
			scriptAssertf(0, "%s:CTheScripts::GtaStartNewThreadWithNameHashOverride - the hash of the script to be started is 0", GetCurrentScriptNameAndProgramCounter());
		}
		else
		{
			scriptAssertf(0, "CTheScripts::GtaStartNewThreadWithNameHashOverride - the hash of the script to be started is 0");
		}
	}

//	I'm not sure what to do with IsCompiledAndResident. It seems to be using a different hash function - scrComputeHash
	scrProgramId pid = srcpidNONE;	//	 = scrProgram::IsCompiledAndResident(progname);
//	if (!pid)
	{
		strLocalIndex id = g_StreamedScripts.FindSlotFromHashKey(hashOfProgName.GetHash());
		scriptAssertf(id != -1, "Script with hash %u doesn't exist", hashOfProgName.GetHash());
		if (id == -1)
			return THREAD_INVALID;

		scriptAssertf(g_StreamedScripts.Get(id), "Script with hash %u is not in memory", hashOfProgName.GetHash());

		scriptDisplayf("About to start script thread with hash %u", hashOfProgName.GetHash());
		pid = g_StreamedScripts.GetProgramId(id.Get());
	}

	char debugStringContainingHash[32];
	formatf(debugStringContainingHash, "0x%x", hashOfProgName.GetHash());
	return GtaStartNewThreadWithProgramId(pid, argStruct,argStructSize, stackSize, debugStringContainingHash);
}

scrThreadId CTheScripts::GtaStartNewThreadWithProgramId(scrProgramId progId,const void *argStruct,int argStructSize,int stackSize, const char* OUTPUT_ONLY(progname) )
{
	scrThreadId threadId = scrThread::CreateThread(progId, argStruct, argStructSize, stackSize);

#if !__NO_OUTPUT
	if (threadId == THREAD_INVALID)
	{
#if __BANK
		GtaThread::DisplayAllRunningScripts(false);
#endif	//	__BANK

#if __ASSERT
		scriptAssertf(0, "%s:Failed to create a new thread (stack %d)", progname, stackSize);
#else
		scriptErrorf("%s:Failed to create a new thread (stack %d)", progname, stackSize);
#endif	//	__ASSERT
	}
#endif	//	!__NO_OUTPUT

	GtaThread *pNewOne = static_cast<GtaThread *> (scrThread::GetThread(threadId));
	if (pNewOne == NULL)
	{
#if __ASSERT
		scriptAssertf(0, "CTheScripts::GtaStartNewThreadWithProgramId - Could not get a pointer to the new thread %s", progname);
#else
		scriptErrorf("CTheScripts::GtaStartNewThreadWithProgramId - Could not get a pointer to the new thread %s", progname);
#endif	//	__ASSERT

		return THREAD_INVALID;
	}
	pNewOne->Initialise();

	ms_ScriptHandlerMgr.RegisterScript(*pNewOne);

#if __BANK
	if (CScriptDebug::ShouldThisScriptBeDebugged(progname))
	{
		GtaThread::AllocateDebuggerForThread(threadId);
	}

	CScriptDebug::SetComboBoxOfThreadNamesNeedsUpdated(true);
#endif

#if BACKTRACE_ENABLED
	sysException::BacktraceNotifyScriptLaunched((u32)progId, fwTimer::GetSystemTimeInMilliseconds());
#endif // BACKTRACE_ENABLED

    scriptDisplayf("Started new script thread %s(%d) with stack size %d", progname, threadId, stackSize);
	return threadId;
}

#if !__PPU
void CTheScripts::SetSinglePlayerRestoreHasJustOccurred(bool bRestoreJustOccurred)
{
	ms_bSinglePlayerRestoreHasJustOccurred = bRestoreJustOccurred;
}

bool CTheScripts::GetSinglePlayerRestoreHasJustOccurred()
{
	return ms_bSinglePlayerRestoreHasJustOccurred;
}
#endif	//	!__PPU


void CScriptPatrol::Init()
{
    bPatrolRouteOpen = false;
    ScriptPatrolNodeIndex = 0;
    ScriptPatrolLinkIndex = 0;
    ScriptPatrolRouteHash = 0;

//	fwIplPatrolNode ScriptPatrolNodes[MAX_NUMBER_OF_PATROL_NODES];
//	fwIplPatrolLink ScriptPatrolLinks[MAX_NUMBER_OF_PATROL_LINKS];
//	int ScriptPatrolNodeIDs[MAX_NUMBER_OF_PATROL_NODES];
}

void CScriptPatrol::OpenPatrolRoute(const char* RouteName)
{
	if (SCRIPT_VERIFY((strnicmp ("miss_", RouteName,5) == 0), "CScriptPatrol::OpenPatrolRoute - Script patrol routes name must start with miss_"))
	{
		if (SCRIPT_VERIFY(bPatrolRouteOpen == false, "CScriptPatrol::OpenPatrolRoute - Patrol route must be closed before being opened"))
		{
			ScriptPatrolRouteHash = atStringHash(RouteName);
			bPatrolRouteOpen  = true;
			ScriptPatrolNodeIndex = 0;
			ScriptPatrolLinkIndex = 0;
		}
	}
}

void CScriptPatrol::ClosePatrolRoute()
{
	bPatrolRouteOpen = false;
}

void CScriptPatrol::AddPatrolNode(int NodeId, const char* NodeType, Vector3 &vNodePos, Vector3 &vNodeHeading, int Duration)
{
	if (!vNodeHeading.IsZero())
	{
		vNodeHeading = vNodeHeading - vNodePos;
	}
	
	int iDuration = (int) Duration / 1000;

	// Check if the node type references a valid scenario right away, rather than waiting for
	// a more cryptic assert from the tasks.
	u32 nodeTypeHash = atStringHash(NodeType);
	scriptAssertf(!nodeTypeHash || SCENARIOINFOMGR.GetScenarioIndex(nodeTypeHash, false) >= 0,
			"%s:ADD_PATROL_NODE - failed for find scenario '%s'.",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), NodeType);

	if (SCRIPT_VERIFY(bPatrolRouteOpen == true, "CScriptPatrol::AddPatrolNode- Patrol route must be open before adding nodes"))
	{
		if (SCRIPT_VERIFY((ScriptPatrolNodeIndex <= MAX_NUMBER_OF_PATROL_NODES - 1) , "CScriptPatrol::AddPatrolNode - Patrol route can have no more than 16 nodes"))
		{
			ScriptPatrolNodes[ScriptPatrolNodeIndex].x = vNodePos.x;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].y = vNodePos.y;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].z = vNodePos.z;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].headingX = vNodeHeading.x;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].headingY = vNodeHeading.y;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].headingZ = vNodeHeading.z;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].nodeTypeHash = nodeTypeHash;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].routeNameHash = ScriptPatrolRouteHash;
			ScriptPatrolNodes[ScriptPatrolNodeIndex].duration = iDuration;
			ScriptPatrolNodeIDs[ScriptPatrolNodeIndex] = NodeId;
			ScriptPatrolNodeIndex++;
		}
	}
}

void CScriptPatrol::AddPatrolLink(int NodeID1, int NodeID2)
{
	if (SCRIPT_VERIFY(bPatrolRouteOpen == true, "CScriptPatrol::AddPatrolLink - Patrol route must be open before adding nodes"))
	{
		if (SCRIPT_VERIFY(ScriptPatrolLinkIndex <= MAX_NUMBER_OF_PATROL_LINKS -1, "CScriptPatrol::AddPatrolLink - Patrol links must be less than 64"))
		{
			ScriptPatrolLinks[ScriptPatrolLinkIndex].node1 = NodeID1;
			ScriptPatrolLinks[ScriptPatrolLinkIndex].node2 = NodeID2;
			ScriptPatrolLinkIndex++;
		}
	}
}

void CScriptPatrol::CreatePatrolRoute()
{
	if (SCRIPT_VERIFY(bPatrolRouteOpen == false, "CScriptPatrol::CreatePatrolRoute - Must close the nodes before adding the route"))	
	{		
		CScriptResource_PatrolRoute patrolRoute(ScriptPatrolRouteHash, ScriptPatrolNodes, ScriptPatrolNodeIDs, ScriptPatrolNodeIndex, ScriptPatrolLinks, ScriptPatrolLinkIndex, 0);
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(patrolRoute);
	}
}

void CTheScripts::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_SCRIPT);
    //@@: location CTHESCRIPTS_INIT_INIT_ENTRY_POINT

	GtaThread::StaticInit(initMode);

    if(initMode == INIT_CORE)
    {
		GEN9_STANDALONE_ONLY(Assertf(ms_initState == InitState::Uninitialized, "Unexpected state %d, should be %d", ms_initState, InitState::Uninitialized));
		GEN9_STANDALONE_ONLY(ms_initState = InitState::Initialized);

        RAGE_TRACK(CTheScripts);
		
#if ENABLE_SCRIPT_GLOBALS_HEAP
		const int globalsHeapSize = fwConfigManager::GetInstance().GetSizeOfPool(atHashString("scrGlobals", 0xC780FC5), CONFIGURED_FROM_FILE);
		sysMemStartPool();
		u8* heapMem = rage_new u8[globalsHeapSize];
		sysMemEndPool();
		scrProgram::InitGlobalsHeap(heapMem, globalsHeapSize);
#endif // ENABLE_SCRIPT_GLOBALS_HEAP

	    scrProgram::InitClass();
		scrThread::InitClass();
	    GtaThread::AllocateThreads();
		ms_ScriptHandlerMgr.Init();
		scriptInterface::Init(ms_ScriptHandlerMgr);

		CGameScriptResource::Init(initMode);
		//@@: location CTHESCRIPTS_INIT_INIT_SCRIPTSMETADATAMANAGER
		CScriptMetadataManager::Init();

	    scrThread::AppFaultHandler = GtaFaultHandler;
	    scrThread::StartNewThreadOverride = GtaStartNewThreadOverride;
		scrThread::StartNewThreadWithNameHashOverride = GtaStartNewThreadWithNameHashOverride;

#if !__PPU
	    ms_bSinglePlayerRestoreHasJustOccurred = false;
#endif

	    ms_bProcessingTheScriptsDuringGameInit = false;

	    // register commands

		//@@: location CTHESCRIPTS_INIT_REGISTER_BUILTIN_COMMANDS
	    scrThread::RegisterBuiltinCommands();

		RegisterScriptCommands();

#if SCRIPT_DEBUGGING
		// Process the -nativetrace command line argument to update actions for specified natives
	    scrThread::ParseNativeTraceCommandLine();
#endif

	    ms_pXmlTree = NULL;
	    ms_pCurrentXmlTreeNode = NULL;
	    ms_bUpdatingScriptThreads = false;

		s_InstructionLimit = 0;
#if !__FINAL
		if (PARAM_scriptInstructionLimit.Get())
		{
			PARAM_scriptInstructionLimit.Get(s_InstructionLimit);
		}
#endif	//	!__FINAL

#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
		// Add the listener
		CScriptGlobalTamperChecker::Initialize();
#endif
    }

#if GEN9_STANDALONE_ENABLED
	// We want to run this exactly once during INIT_CORE for first launch. If the game session is
	//   subsequently shut down, we want to run this again when it is rebuilt during INIT_SESSION.
    if(ms_initState == InitState::Initialized)
#else
	if (initMode == INIT_SESSION)
#endif // GEN9_STANDALONE_ENABLED
    {
		GEN9_STANDALONE_ONLY(ms_initState = InitState::Running);

#if __DEV
	    g_StreamedScripts.ValidateAllScripts();
#endif

	    scriptAssertf (ms_pXmlTree== NULL, "CTheScripts::Init - XmlNodeTree is not Null");
	    ms_pXmlTree = NULL;
	    ms_pCurrentXmlTreeNode = NULL;
	    ms_bUpdatingScriptThreads = false;

	    //patrol routes
		ms_ScriptPatrol.Init();

	    ms_ScriptsForBrains.Init();
	    ms_ScriptBrainDispatcher.Init();

#if __DEV
	    bDisplayUsedScriptResources = false;
#endif

	    ms_bPlayerIsOnAMission = false;
		ms_bPlayerIsOnARandomEvent = false;
		ms_bPlayerIsRepeatingAMission = false;

	    ms_NumberOfMiniGamesInProgress = 0;

		ms_HiddenObjects.Init();

	    ms_bAllowGameToPauseForStreaming = true;

	    ms_bScenarioPedsCanBeGrabbedByScript = false;

			ms_bCodeRequestedAutosave = false;

		ms_bKillingAllScriptThreads = false;

	    ms_bProcessingTheScriptsDuringGameInit = false;

#if __BANK
		sm_fTimeFadedOut = 0.0f;
#endif	//	__BANK

#if !__FINAL
		ms_bScriptLaunched = false;
#endif //!__FINAL

		sm_ScriptTimer.Reset();

		SetContentIdOfCurrentUGCMission("");

		static const int kDefaultItemSetBufferSize = 128;		// May want to override this to be much larger through the configuration file, RDR2 needed 2048. Memory per unit should be 4 byets.
		const int itemSetBufferSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ItemSetBuffer", 0x04e2adf9b), kDefaultItemSetBufferSize);
		CItemSetManager::InitClass(itemSetBufferSize);
		
		LaunchStartupScript();
    }
	//@@: location CTHESCRIPTS_INIT_INITIALIZE_SCRIPT_SHAPE_TEST_MANAGER
	CScriptShapeTestManager::Init(initMode);
	//@@: location CTHESCRIPTS_INIT_INITIALIZE_MISSION_REPLAY_STATS_INIT
	ms_MissionReplayStats.Init(initMode);
}

void CTheScripts::LaunchStartupScript()
{
//	if (!bLoadingAPreviouslySavedGame)
    {
	    strLocalIndex mainScriptId = strLocalIndex(-1);

		// Gen9 Landing Page: There is now a pre-startup script that runs before the game session exists.
		//                    The -startupscript commandline param is delayed until the game session is created,
		//                    then it is loaded in script by landing_pre_startup.
#if GEN9_LANDING_PAGE_ENABLED
		const char *pScriptName = "landing_pre_startup";
#else // GEN9_LANDING_PAGE_ENABLED
		const char *pScriptName = CFileMgr::IsGameInstalled() ? "startup" : "startup_install";
		NOTFINAL_ONLY(PARAM_startupscript.Get(pScriptName));
#endif // GEN9_LANDING_PAGE_ENABLED

#if !__FINAL
		ms_bScriptLaunched = true;
#endif //!__FINAL

		mainScriptId = g_StreamedScripts.FindSlot(pScriptName);

        if(mainScriptId != -1)
		{
		    g_StreamedScripts.StreamingRequest(mainScriptId, STRFLAG_DONTDELETE);
		    CStreaming::LoadAllRequestedObjects();

			if (g_StreamedScripts.HasObjectLoaded(mainScriptId))
			{
#if EXECUTABLE_SCRIPTS
				// Load all of the precompiled controller scripts now.  (multiple scripts per PRX/DLL now)
				// We need to do this after we've registered all script commands, and also after all globals
				// have been registered as well.
				// See x:\gta5\script\dev\singleplayer\make_controllers.bat and build_controllers.bat.
				scrProgram::RegisterBinary("controllers");
#endif	// EXECUTABLE_SCRIPTS
				scrThreadId NewScrThreadId = scrThread::CreateThread(g_StreamedScripts.GetProgramId(mainScriptId.Get()), NULL, 0, GtaThread::GetDefaultStackSize());

				scrThread *pNewScriptThread = scrThread::GetThread(NewScrThreadId);
				if (pNewScriptThread)
				{
					GtaThread *pNewGtaThread = (GtaThread*) pNewScriptThread;

					pNewGtaThread->Initialise();

					ms_ScriptHandlerMgr.RegisterScript(*pNewScriptThread);

#if __BANK
					if (CScriptDebug::ShouldThisScriptBeDebugged(pNewScriptThread->GetScriptName()))
					{
						GtaThread::AllocateDebuggerForThread(NewScrThreadId);
					}
#endif
				}

				g_StreamedScripts.ClearRequiredFlag(mainScriptId.Get(), STRFLAG_DONTDELETE);
			}
			else
			{
				Quitf(ERR_SCR_LAUNCH_1,"CTheScripts::LaunchStartupScript - the %s script has not loaded despite LoadAllRequestedObjects() being called", pScriptName);
			}
	    }
		else
		{
			Quitf(ERR_SCR_LAUNCH_2,"CTheScripts::LaunchStartupScript - failed to find a script with the name %s in the script.rpf", pScriptName);
		}
    }
}

//
// name:		CTheScripts::Shutdown
// description:	
void CTheScripts::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		GEN9_STANDALONE_ONLY(ms_initState = InitState::Uninitialized);

		// shutdown script event queues
		GetEventScriptErrorsGroup()->Shutdown();
		GetEventScriptAIGroup()->Shutdown();
		GetEventScriptNetworkGroup()->Shutdown();

		g_ScriptAudioEntity.StopStream();
		ms_ScriptHandlerMgr.Shutdown();
        g_StreamedScripts.Shutdown();
		CScriptMetadataManager::Shutdown();
		GtaThread::DeallocateThreads();
	    scrProgram::ShutdownClass();
		scriptInterface::Shutdown();
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		// Inform the script engine that it will need to relaunch scripts if the game session is initialised
		GEN9_STANDALONE_ONLY(ms_initState = InitState::Initialized);

		ms_bKillingAllScriptThreads = true;
		scrThread::KillAllThreads();
		ms_bKillingAllScriptThreads = false;

		CItemSetManager::ShutdownClass();
		g_ScriptAudioEntity.StopStream();
		g_StreamedScripts.ShutdownLevel();
		ms_HiddenObjects.UndoHiddenObjects();

		CScriptHud::Shutdown(shutdownMode);
		
		ms_scriptOpList.Flush();

		CGameScriptResource::Shutdown(shutdownMode);

#if ENABLE_SCRIPT_GLOBALS_HEAP
		ASSERT_ONLY(scrProgram::AssertGlobalsHeapEmpty();)
#endif // ENABLE_SCRIPT_GLOBALS_HEAP
    }

	CScriptShapeTestManager::Shutdown(shutdownMode);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Called by the main update loop.
/////////////////////////////////////////////////////////////////////////////////
void 
CTheScripts::Update()
{
}

bool CTheScripts::ShouldBeProcessed()
{
	if ( (fwTimer::IsGamePaused()) && (fwTimer::ShouldScriptsBePaused()) )
	{
		return false;
	}

//	Graeme - 14.06.10 - I had to comment this out to allow the scripts to run for one frame during initialisation
//	if(CGame::IsChangingSessionState())
//	{	// If the network connection is lost then a message is displayed telling the player this and waiting
//	for A to be pressed. It's probably a bad idea to continue processing the scripts while a session restart is pending.
//	This return will stop any scripts running until the session has ended and a new one has started.
//		return false;
//	}

	// scripts are only processed when the game is performing a full update/render or HTML is being displayed
	// or the player is on the player settings screen in a network game
	// or the scripts are processing for one frame during the initialisation phase
	// or the Gen9 landing page is displayed before the game session is initialized
	if(!CGame::ShouldDoFullUpdateRender() 
		&& !NetworkInterface::IsGameInProgress()
		&& !GetProcessingTheScriptsDuringGameInit()
		GEN9_LANDING_PAGE_ONLY(&& (!CLandingPageArbiter::IsLandingPageActive() || CGame::IsSessionInitialized()))
		)
	{
		return false;
	}

	ReFrack();

	// Don't process the scripts with a replay going on.
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive() && !CReplayMgr::ShouldUpdateScripts())
	{
		return false;
	}
#endif

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Process
// PURPOSE :  Wrapper for InternalProcess so we can protect script globals from 
//			  Tampering on PC MP
/////////////////////////////////////////////////////////////////////////////////
void CTheScripts::Process(void)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	//////////////////////////////////////////////////////////////////////////
	// PC only - check for memory-tampering in script globals
	static bool s_bReportedPlayer = false;	
#if !__FINAL
	static bool s_bProtectMemory = true;
	if(PARAM_scriptProtectGlobals.Get())
	{
		s_bProtectMemory = true;
	}
#endif

	int page = -1;
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() && NetworkInterface::IsGameInProgress();
	if (
#if !__FINAL
		s_bProtectMemory && 
#endif
		!CPauseMenu::IsActive() && CScriptGlobalTamperChecker::CurrentGlobalsDifferFromPreviousFrame(page))
	{
		if ( !s_bReportedPlayer && isMultiplayer )
		{
			s_bReportedPlayer = true;
			RageSecPluginGameReactionObject obj(REACT_GAMESERVER, GENERIC_GAME_ID, 0x20C8C138, 0x611E42B9, 0x23346609);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}
	}


	{   //unchecked scope on PC only
		CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;	
#endif	//RSG_PC

		InternalProcess();

#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER		
	} // end unchecked scope on PC
	CScriptGlobalTamperChecker::Update();
#if !__FINAL
	if(s_bProtectMemory)
#endif
	{
#if		__BANK
		if(CScriptDebug::GetFakeTampering())
		{
			CScriptGlobalTamperChecker::FakeTamper(0);
		}
#endif //__BANK
	}
#endif

}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Process
// PURPOSE :  Processes the scripts.
/////////////////////////////////////////////////////////////////////////////////

void CTheScripts::InternalProcess(void)
{

	AUTO_HANG_DETECT_CRASH_ZONE;
	ms_ScriptHandlerMgr.Update();

#if __BANK
	if (camInterface::IsFadedOut())
	{
		sm_fTimeFadedOut += fwTimer::GetTimeStep();
	}
	else
	{
		sm_fTimeFadedOut = 0.0f;
	}

	if (DisplayScriptProcessMessagesWhileScreenIsFaded())
	{
		scriptDisplayf("CTheScripts::Process called while screen is faded");
	}
#endif	//	__BANK

	CControlMgr::UpdateVirtualKeyboard();

	if (!ShouldBeProcessed())
	{
		return;
	}

#if GTA_REPLAY
	if(!CReplayMgr::IsReplayDisabled() && !CReplayMgr::IsRecordingEnabled() && !CReplayMgr::ShouldUpdateScripts())
	{
		// update movie mesh manager
		g_movieMeshMgr.Update();
		return;
	}
#endif	//GTA_REPLAY

	PF_START_TIMEBAR_DETAIL("Script update");

	NetworkInterface::UpdateBeforeScripts();

#if RSG_PC
	NtDll::VerifyProtection();
	Kernel32::VerifyProtection();
#endif

	sm_ScriptTimer.Reset();

	CScriptAreas::Process();
	CScriptCars::Process();
	CScriptPeds::Process();
	CScriptDebug::Process();
	CScriptHud::Process();

#if __DEV
	if (bDisplayUsedScriptResources)
	{
		ms_ScriptHandlerMgr.DisplayObjectAndResourceInfo();
	}
#endif

#if __BANK
	ms_ScriptHandlerMgr.DisplayScriptHandlerInfo();
#endif

	if (CGameWorld::GetMainPlayerInfo() REPLAY_ONLY(&& CReplayMgr::IsReplayInControlOfWorld() == false))
	{
		CWorldPoints::ScanForScriptTriggerWorldPoints(CGameWorld::FindLocalPlayerCentreOfWorld());
	}

	ms_ScriptBrainDispatcher.Process();

#if __BANK
	if (CScriptDebug::GetOutputScriptDisplayTextCommands())
	{
		scripthudDisplayf("CTheScripts::Process - Start of processing script threads");
	}
#endif	//	__BANK

	scrThread::SetTimeStep(fwTimer::GetTimeStep());
	ms_bUpdatingScriptThreads = true;
	scrThread::UpdateAll(s_InstructionLimit);
	ms_bUpdatingScriptThreads = false;

#if __BANK
	if (CScriptDebug::GetOutputScriptDisplayTextCommands())
	{
		scripthudDisplayf("CTheScripts::Process - End of processing script threads");
	}
#endif	//	__BANK

	//Doing this immediately after threads update to minimise time spent with shapetest requests enqueued
	CScriptShapeTestManager::Update();

	//	To free .sco files faster so that they can be used within the Preview folder
	GtaThread::KillAllAbortedScriptThreads();

	// flush script event queues
	GetEventScriptErrorsGroup()->FlushAllAndMovePending();
	GetEventScriptAIGroup()->FlushAllAndMovePending();
	GetEventScriptNetworkGroup()->FlushAllAndMovePending();

	// update movie mesh manager
	g_movieMeshMgr.Update();

	CutSceneManager::GetInstance()->RenderBinkMovieAndUpdateRenderTargets();

#if __BANK
	if (DisplayScriptProcessMessagesWhileScreenIsFaded())
	{
		sm_fTimeFadedOut -= 4.0f;
	}
#endif	//	__BANK

	NetworkInterface::UpdateAfterScripts();

	CAnimSceneManager::Update(kAnimSceneUpdatePostScript);
}


void CTheScripts::RegisterEntity(CPhysical* pEntity, bool bScriptHostObject, bool bNetworked, bool bNewEntity)
{
	if (AssertVerify(pEntity) &&
		scriptVerifyf(!pEntity->GetExtension<CScriptEntityExtension>(), "CTheScripts::RegisterEntity - The entity with model %s and PopType %s is already registered as a mission entity", pEntity->GetModelName(), CTheScripts::GetPopTypeName(pEntity->PopTypeGet()) ))
	{
		CScriptEntityExtension* pNewExtension = rage_new CScriptEntityExtension(*pEntity);
		Assert(pEntity->GetExtension<CScriptEntityExtension>());

		scriptHandler* pScriptHandler = GetCurrentGtaScriptThread()->m_Handler;

		if (bScriptHostObject && NetworkInterface::IsGameInProgress())
		{
			if (!bNetworked)
			{
				sysStack::PrintStackTrace();
				Displayf("CTheScripts::RegisterEntity - trying to register a non-networked script host entity. Forcing networking.");
				Assertf(0, "CTheScripts::RegisterEntity - trying to register a non-networked script host entity. Forcing networking.");
				bNetworked = true;
			}

			if (bNetworked && !pEntity->GetNetworkObject())
			{
				sysStack::PrintStackTrace();
				Displayf("CTheScripts::RegisterEntity - trying to register a script host entity with no network object. Forcing client object.");
				Assertf(0, "CTheScripts::RegisterEntity - trying to register a script host entity with no network object. Forcing client object.");
				bScriptHostObject = false;
				bNetworked = false;
			}
		}

		// cache the networked state so that we know whether to register the object with the network code when a network game starts
		pNewExtension->SetIsNetworked(bNetworked);

		// all non-networked entities must be client ones as no other machine will know about them
		if (!bNetworked)
		{
			 bScriptHostObject = false;
		}

		if (AssertVerify(pScriptHandler))
		{
			pScriptHandler->RegisterNewScriptObject(static_cast<scriptHandlerObject&>(*pNewExtension), bScriptHostObject, bNetworked);

			if (pEntity->GetNetworkObject() && pScriptHandler->GetNetworkComponent())
			{
				if (bScriptHostObject)
				{
					static_cast<CGameScriptHandlerNetwork*>(pScriptHandler)->ValidateRegistrationRequest(pEntity, bNewEntity);

					// host objects should always be cloned on all machines running the script
					pEntity->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT, true);

					if (pEntity->GetNetworkObject())
					{
						// update the scope state immediately so that the clone creates will go out a.s.a.p
						NetworkInterface::GetObjectManager().UpdateAllInScopeStateImmediately(pEntity->GetNetworkObject());
					}
				}
			}
		}

		pEntity->ProtectStreamedArchetype();
	}
}

void CTheScripts::UnregisterEntity(CPhysical* pEntity, bool bCleanupScriptState)
{
	// unregister the ped from its associated script handler. 
	CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();
	scriptHandler* pScriptHandler = pExtension ? pExtension->GetScriptHandler() : NULL;

	if (pExtension && pScriptHandler)
	{
		// don't cleanup the script state of the target object if the object is about to be destroyed
		pExtension->SetCleanupTargetEntity(bCleanupScriptState);

		// set the no longer needed flag here so that the script handler knows that it needs to clean up the script info for the entity
		pExtension->SetNoLongerNeeded(true);

		pScriptHandler->UnregisterScriptObject(*pExtension);

		pEntity->UnprotectStreamedArchetype();
	}

// Graeme - the correct thing to do would be to delete the script guid for this entity here so that the entity can no longer be referenced by script
//	A few scripts are relying on being able to make a temp copy of an entityId, mark the entity as no longer needed and then call a few extra commands through the copied entityId
//	This wouldn't be possible if the line below was uncommented
//	fwScriptGuid::DeleteGuid(*pEntity);
}

CEntity** CTheScripts::GetAllScriptEntities(u32& numEntities)
{
	const u32 maxNumberOfScriptEntities = 700;
	static CEntity* entities[maxNumberOfScriptEntities];

	DEV_ONLY(memset(entities, 0, sizeof(entities)));

	scriptAssertf(maxNumberOfScriptEntities == (u32) CScriptEntityExtension::GetPool()->GetSize(), "CTheScripts::GetAllScriptEntities - maxNumberOfScriptEntities is %u. Change it to match the size of the CScriptEntityExtension pool which is defined in gameconfig.xml to be %d", 
		maxNumberOfScriptEntities, CScriptEntityExtension::GetPool()->GetSize());

	numEntities = ms_ScriptHandlerMgr.GetAllScriptHandlerEntities(entities, maxNumberOfScriptEntities);

	return entities;
}

bool CTheScripts::GetPlayerIsOnAMission(void)
{
	if (ms_bPlayerIsOnAMission)
	{
		return true;
	}

	return false;
}

bool CTheScripts::IsPlayerPlaying(CPed *pPlayerPed)
{
	if (scriptVerifyf(pPlayerPed, "CTheScripts::IsPlayerPlaying - ped pointer is NULL"))
	{
		CPlayerInfo *pPlayerInfo = pPlayerPed->GetPlayerInfo();
		if (scriptVerifyf(pPlayerInfo, "CTheScripts::IsPlayerPlaying - ped doesn't have PlayerInfo"))
		{
			if (pPlayerInfo->IsRestartingAfterDeath() || pPlayerPed->IsInjured()
				|| pPlayerInfo->IsRestartingAfterArrest())
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

#if __DEV
void CTheScripts::ClearAllEntityCheckedForDeadFlags(void)
{
	CPed::Pool *pedpool = CPed::GetPool();
	CPed *pPed;
	CVehicle::Pool *vehpool = CVehicle::GetPool();
	CVehicle *pVehicle;
	s32 i;

	i = (s32) pedpool->GetSize();

	while (i--)
	{
		pPed = pedpool->GetSlot(i);

		if (pPed)	//	 && !pPed->IsPlayer())
		{
			pPed->m_nDEflags.bCheckedForDead = FALSE;
		}
	}

	i = (s32) vehpool->GetSize();

	while (i--)
	{
		pVehicle = vehpool->GetSlot(i);

		if (pVehicle)
		{
			pVehicle->m_nDEflags.bCheckedForDead = FALSE;
		}
	}
}


#endif	//	__DEV


GtaThread *CTheScripts::GetCurrentGtaScriptThread(void)
{
	return static_cast<GtaThread *>(scrThread::GetActiveThread());
}

CGameScriptHandler *CTheScripts::GetCurrentGtaScriptHandler(void)
{
	return (scrThread::GetActiveThread() ? static_cast<GtaThread *>(scrThread::GetActiveThread())->m_Handler : NULL);
}

CGameScriptHandlerNetwork *CTheScripts::GetCurrentGtaScriptHandlerNetwork(void)
{
	CGameScriptHandler *pHandler = GetCurrentGtaScriptHandler();

	if (pHandler &&	pHandler->RequiresANetworkComponent())
	{
		return static_cast<CGameScriptHandlerNetwork *>(pHandler);
	}

	return NULL;
}

void CTheScripts::PrioritizeRequestsFromMissionScript(s32 &streamingFlags)
{
	if (!GetCurrentGtaScriptThread() || !GetCurrentGtaScriptThread()->m_bIsHighPrio)
		return;

	if (GetCurrentGtaScriptThread()->IsThisAMissionScript || GetCurrentGtaScriptThread()->GetIsARandomEventScript() || GetCurrentGtaScriptThread()->GetIsATriggerScript())
	{
		streamingFlags |= STRFLAG_PRIORITY_LOAD;
	}
}

u32 CTheScripts::GetCurrentScriptHash(void)
{
	const u32 noScript = 0;
	if (ms_bUpdatingScriptThreads)
	{
		return scrThread::GetActiveThread()->GetScriptNameHash();
	}
	else
	{
		return noScript;
	}
}

const char *CTheScripts::GetCurrentScriptName(void)
{
	const char* noScript = "";
	if (ms_bUpdatingScriptThreads)
	{
		return scrThread::GetActiveThread()->GetScriptName();
	}
	else
	{
		return noScript;
	}
}

#if !__NO_OUTPUT
const char *CTheScripts::GetCurrentScriptNameAndProgramCounter(void)
{
	static char text[256];

	if (ms_bUpdatingScriptThreads)
	{
		sprintf(text, "SCRIPT: Script Name = %s : Program Counter = %d", GetCurrentScriptName(), GetCurrentGtaScriptThread()->GetThreadPC() );
	}
	else
	{
		sprintf(text, " ");
	}

	return text;
}
#endif // !__NO_OUTPUT


void CTheScripts::SetContentIdOfCurrentUGCMission(const char *pContentIdString)
{
	if (pContentIdString == NULL)
	{
		scriptAssertf(0, "CTheScripts::SetContentIdOfCurrentUGCMission called with a NULL string so setting the string to empty");
		pContentIdString = "";
	}

	safecpy(ms_ContentIdOfCurrentUGCMission, pContentIdString, NELEM(ms_ContentIdOfCurrentUGCMission));
}

// ********************* CMissionReplayStats **************************
void CMissionReplayStats::Init(unsigned initMode)
{
	//	Don't clear the structure in INIT_SESSION
	if(initMode == INIT_CORE)
	{
		ClearReplayStatsStructure();
	}
}

//	When a Replay Mission is passed
void CMissionReplayStats::BeginReplayStatsStructure(s32 missionId, s32 missionType)
{
	if (scriptVerifyf(m_bStatsHaveBeenStored == false, "CMissionReplayStats::BeginReplayStatsStructure - stats have already been stored"))
	{
		if (scriptVerifyf(m_bConstructingStructureOfStats == false, "CMissionReplayStats::BeginReplayStatsStructure - structure of stats is already being constructed"))
		{
			m_bConstructingStructureOfStats = true;
			m_nMissionId = missionId;
			m_nMissionType = missionType;

			m_ArrayOfMissionStats.Reset();
		}
	}
}

void CMissionReplayStats::AddStatValueToReplayStatsStructure(s32 valueOfStat)
{
	if (scriptVerifyf(m_bConstructingStructureOfStats, "CMissionReplayStats::AddStatValueToReplayStatsStructure - structure of stats is not being constructed. Check that BEGIN_REPLAY_STATS has been called first"))
	{
		if (scriptVerifyf(m_bStatsHaveBeenStored == false, "CMissionReplayStats::AddStatValueToReplayStatsStructure - stats have already been stored"))
		{
			m_ArrayOfMissionStats.PushAndGrow(valueOfStat);
		}
	}	
}

void CMissionReplayStats::EndReplayStatsStructure()
{
	if (scriptVerifyf(m_bConstructingStructureOfStats, "CMissionReplayStats::EndReplayStatsStructure - structure of stats is not being constructed. Check that BEGIN_REPLAY_STATS has been called first"))
	{
		if (scriptVerifyf(m_bStatsHaveBeenStored == false, "CMissionReplayStats::EndReplayStatsStructure - stats have already been stored"))
		{
			m_bStatsHaveBeenStored = true;
			m_bConstructingStructureOfStats = false;
		}
	}
}
//	End of "When a Replay Mission is passed"

//	When a Load happens and the script detects that it’s a Replay save file
s32 CMissionReplayStats::GetNumberOfMissionStats()
{
	if (scriptVerifyf(m_bStatsHaveBeenStored, "CMissionReplayStats::GetNumberOfMissionStats - stats haven't been stored"))
	{
		return m_ArrayOfMissionStats.GetCount();
	}

	return 0;
}

s32 CMissionReplayStats::GetStatValueAtIndex(s32 arrayIndex)
{
	if (scriptVerifyf(m_bStatsHaveBeenStored, "CMissionReplayStats::GetStatValueAtIndex - stats haven't been stored"))
	{
		if (scriptVerifyf( (arrayIndex >= 0) && (arrayIndex < m_ArrayOfMissionStats.GetCount()), "CMissionReplayStats::GetStatValueAtIndex - arrayIndex %d is out of range 0 to %d", arrayIndex, m_ArrayOfMissionStats.GetCount()))
		{
			return m_ArrayOfMissionStats[arrayIndex];
		}
	}

	return 0;
}

void CMissionReplayStats::ClearReplayStatsStructure()
{

	m_bStatsHaveBeenStored = false;
	m_bConstructingStructureOfStats = false;
	m_nMissionId = 0;
	m_nMissionType = 0;

	m_ArrayOfMissionStats.Reset();
}
//	End of "When a Load happens and the script detects that it’s a Replay save file"



// ********************* CHiddenObjects **************************
void CHiddenObjects::Init()
{
	m_MapOfHiddenObjects.Reset();
}


#if !__NO_OUTPUT
void CHiddenObjects::DisplayInteriorDetails(fwInteriorLocation &interiorLoc)
{
	if (interiorLoc.IsValid())
	{
		scriptDisplayf("CHiddenObjects::DisplayInteriorDetails - interiorLoc is valid");

		scriptDisplayf("CHiddenObjects::DisplayInteriorDetails - interior proxy index is %d", interiorLoc.GetInteriorProxyIndex());

		if (interiorLoc.IsAttachedToRoom())
		{
			scriptDisplayf("CHiddenObjects::DisplayInteriorDetails - interiorLoc is attached to room. Room index is %d", interiorLoc.GetRoomIndex());
		}

		if (interiorLoc.IsAttachedToPortal())
		{
			scriptDisplayf("CHiddenObjects::DisplayInteriorDetails - interiorLoc is attached to portal. Portal index is %d. PortalToExterior=%s", interiorLoc.GetPortalIndex(), interiorLoc.IsPortalToExterior()?"true":"false");
		}
	}
	else
	{
		scriptDisplayf("CHiddenObjects::DisplayInteriorDetails - interiorLoc is not valid");
	}
}
#endif	//	!__NO_OUTPUT

void CHiddenObjects::AddToHiddenObjectMap(s32 ScriptGUIDOfEntity, spdSphere &SearchVolume, s32 ModelHash, 
										  CDummyObject *pDummyObj, fwInteriorLocation DummyInteriorLocation, s32 IplIndex)
{
	if (m_MapOfHiddenObjects.Access(ScriptGUIDOfEntity) == NULL)
	{
		hidden_object_struct HiddenObjStruct;
		HiddenObjStruct.m_SearchVolume = SearchVolume;
		HiddenObjStruct.m_ModelHash = ModelHash;

#if !__NO_OUTPUT
		fwModelId modelId;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(static_cast< u32 >( ModelHash ), &modelId);

		if (scriptVerifyf(modelId.IsValid(), "CHiddenObjects::AddToHiddenObjectMap - %d (signed) %u (unsigned) is not a valid model hash", ModelHash, (u32) ModelHash))
		{
			Vec3V SearchPos = SearchVolume.GetCenter();
			scriptDisplayf("CHiddenObjects::AddToHiddenObjectMap - called for %s at %f %f %f", pModelInfo->GetModelName(), SearchPos.GetXf(), SearchPos.GetYf(), SearchPos.GetZf());
		}
#endif	//	!__NO_OUTPUT

		HiddenObjStruct.m_pRelatedDummyForObjectGrabbedFromTheWorld = pDummyObj;
		HiddenObjStruct.m_InteriorLocationOfRelatedDummyForObjectGrabbedFromTheWorld = DummyInteriorLocation;
		HiddenObjStruct.m_IplIndexOfObjectGrabbedFromTheWorld = IplIndex;

#if !__NO_OUTPUT
		scriptDisplayf("CHiddenObjects::AddToHiddenObjectMap - interior location details are");
		DisplayInteriorDetails(DummyInteriorLocation);
#endif	//	!__NO_OUTPUT

		m_MapOfHiddenObjects.Insert(ScriptGUIDOfEntity, HiddenObjStruct);
	}

	g_MapChangeMgr.Add((u32) ModelHash, (u32) ModelHash, SearchVolume, true, CMapChange::TYPE_HIDE, true, true);
}


void CHiddenObjects::ReregisterInHiddenObjectMap(s32 oldScriptGUIDOfEntity, s32 newScriptGUIDOfEntity)
{
	if (oldScriptGUIDOfEntity != newScriptGUIDOfEntity)
	{
//	Get entry for old GUID
		hidden_object_struct *pHideDataForThisEntity = m_MapOfHiddenObjects.Access(oldScriptGUIDOfEntity);
		if (pHideDataForThisEntity)
		{
			hidden_object_struct HiddenObjStruct = *pHideDataForThisEntity;

//	Remove entry for old GUID
			m_MapOfHiddenObjects.Delete(oldScriptGUIDOfEntity);

			if (scriptVerifyf(m_MapOfHiddenObjects.Access(newScriptGUIDOfEntity) == NULL, 
				"CHiddenObjects::ReregisterInHiddenObjectMap - MapOfHiddenObjects contains data for old GUID %d, but we didn't expect it to also contain data for the new GUID %d", oldScriptGUIDOfEntity, newScriptGUIDOfEntity))
			{
#if !__NO_OUTPUT
				fwModelId modelId;
				CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(static_cast< u32 >( HiddenObjStruct.m_ModelHash ), &modelId);

				if (scriptVerifyf(modelId.IsValid(), "CHiddenObjects::ReregisterInHiddenObjectMap - %d (signed) %u (unsigned) is not a valid model hash", HiddenObjStruct.m_ModelHash, (u32) HiddenObjStruct.m_ModelHash))
				{
					Vec3V SearchPos = HiddenObjStruct.m_SearchVolume.GetCenter();
					scriptDisplayf("CHiddenObjects::ReregisterInHiddenObjectMap - called for %s at %f %f %f (Old GUID=%d, New GUID=%d)", 
						pModelInfo->GetModelName(), SearchPos.GetXf(), SearchPos.GetYf(), SearchPos.GetZf(), 
						oldScriptGUIDOfEntity, newScriptGUIDOfEntity);
				}

				scriptDisplayf("CHiddenObjects::ReregisterInHiddenObjectMap - interior location details are");
				DisplayInteriorDetails(HiddenObjStruct.m_InteriorLocationOfRelatedDummyForObjectGrabbedFromTheWorld);
#endif	//	!__NO_OUTPUT

//	Add entry for new GUID
				m_MapOfHiddenObjects.Insert(newScriptGUIDOfEntity, HiddenObjStruct);
			}
		}
	}
}

bool CHiddenObjects::RemoveFromHiddenObjectMap(s32 ScriptGUIDOfEntity, bool bHandleRelatedDummy)
{
	bool bReturnValue = false;

	hidden_object_struct *pHideDataForThisEntity = m_MapOfHiddenObjects.Access(ScriptGUIDOfEntity);
	if (pHideDataForThisEntity)
	{
		g_MapChangeMgr.Remove(pHideDataForThisEntity->m_ModelHash, pHideDataForThisEntity->m_ModelHash, 
			pHideDataForThisEntity->m_SearchVolume, CMapChange::TYPE_HIDE, false);

		CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ScriptGUIDOfEntity, 0);
		if (bHandleRelatedDummy && pObject)
		{
#if !__NO_OUTPUT
			Vec3V SearchPos = pHideDataForThisEntity->m_SearchVolume.GetCenter();
			scriptDisplayf("CHiddenObjects::RemoveFromHiddenObjectMap has been called for %s at %f %f %f", pObject->GetModelName(), SearchPos.GetXf(), SearchPos.GetYf(), SearchPos.GetZf());
#endif	//	!__NO_OUTPUT

			if (!pObject->GetIsRetainedByInteriorProxy())
			{	//	If the object is on a retained list then that means the interior is shut down so skip the following code
				//	and just make the object ENTITY_OWNEDBY_TEMP
				CDummyObject *pRelatedDummy = pHideDataForThisEntity->m_pRelatedDummyForObjectGrabbedFromTheWorld;
				fwInteriorLocation RelatedDummyLocation = pHideDataForThisEntity->m_InteriorLocationOfRelatedDummyForObjectGrabbedFromTheWorld;
				s32 IplIndex = pHideDataForThisEntity->m_IplIndexOfObjectGrabbedFromTheWorld;

				if (!pRelatedDummy)
				{
					scriptDisplayf("CHiddenObjects::RemoveFromHiddenObjectMap - pointer to related dummy for object %s has been lost so start a world search", pObject->GetModelName());
					pRelatedDummy = FindClosestMatchingDummy(pHideDataForThisEntity, pObject);
				}

				if (pRelatedDummy)
				{
					pObject->SetIplIndex(IplIndex);

					pObject->SetRelatedDummy(pRelatedDummy, RelatedDummyLocation);
					//	Can I just call CDummyObject::Disable() and let CObjectPopulation::ManageObject decide whether the object
					//	should be swapped to the dummy?
					pRelatedDummy->Disable();

					scriptDisplayf("CHiddenObjects::RemoveFromHiddenObjectMap - related dummy for object %s has been set up", pObject->GetModelName());

					bReturnValue = true;
				}
			}
			else
			{
				scriptDisplayf("CHiddenObjects::RemoveFromHiddenObjectMap - object %s is on a retained list so skipping the code to re-establish a connection with the related dummy", pObject->GetModelName());
			}
		}

		m_MapOfHiddenObjects.Delete(ScriptGUIDOfEntity);
	}

	return bReturnValue;
}

void CHiddenObjects::UndoHiddenObjects()
{
	scriptAssertf(m_MapOfHiddenObjects.GetNumUsed() == 0, "CHiddenObjects::UndoHiddenObjects - expected all hidden objects to have been dealt with already during Shutdown Session");

//	If any objects haven't already been unhidden then do that now
	atMap<s32, hidden_object_struct>::Iterator mapIterator = m_MapOfHiddenObjects.CreateIterator();
	while (!mapIterator.AtEnd())
	{
		s32 ScriptGUID = mapIterator.GetKey();
//	Can I assume that all CObjects will be deleted in Shutdown Session
//	and that UndoHiddenObjects() will only be called during Shutdown Session
		RemoveFromHiddenObjectMap(ScriptGUID, false);
		mapIterator.Start();
	}
}


struct ClosestDummyObjectStruct
{
	CEntity *m_pClosestDummyObject;

	u32 m_ModelHash;
	fwInteriorLocation m_InteriorLocation;
	s32 m_IplIndex;

	float m_fClosestDistanceSquared;
	Vec3V m_vCoordToCalculateDistanceFrom;
};


bool GetClosestDummyObjectCB(CEntity* pEntity, void* data)
{
	if (scriptVerifyf(pEntity, "GetClosestDummyObjectCB - entity pointer is NULL"))
	{
		if (scriptVerifyf(data, "GetClosestDummyObjectCB - data pointer is NULL"))
		{
			ClosestDummyObjectStruct* pClosestDummyObjectData = reinterpret_cast<ClosestDummyObjectStruct*>(data);

			if(pEntity->GetIsTypeDummyObject())
			{
				if (pEntity->GetBaseModelInfo()->GetModelNameHash() == pClosestDummyObjectData->m_ModelHash)
				{
					if (pEntity->GetIplIndex() == pClosestDummyObjectData->m_IplIndex)
					{
						if (pEntity->GetInteriorLocation().IsSameLocation(pClosestDummyObjectData->m_InteriorLocation))
						{
							//Get the distance between the centre of the locate and the entities position
							Vec3V DiffVector = pEntity->GetTransform().GetPosition() - pClosestDummyObjectData->m_vCoordToCalculateDistanceFrom;

							const float ObjDistanceSquared = MagSquared(DiffVector).Getf();

							if (ObjDistanceSquared < pClosestDummyObjectData->m_fClosestDistanceSquared)
							{
								pClosestDummyObjectData->m_pClosestDummyObject = pEntity;
								pClosestDummyObjectData->m_fClosestDistanceSquared = ObjDistanceSquared;
							}
						}
					}
				}
			}
		}
	}

	return true;
}


//	Debug function to print reason for failure of FindClosestMatchingDummy
void CHiddenObjects::DisplayFailReason(const char* ASSERT_ONLY(pFailReason), const hidden_object_struct* ASSERT_ONLY(pHideData), const CObject* ASSERT_ONLY(pObject))
{
#if __ASSERT
	Vector3 vObjectCoors(0.0f, 0.0f, 0.0f);
	const char *pObjectName = "Unknown Object";
	if (pObject)
	{
		vObjectCoors = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
		pObjectName = pObject->GetModelName();
	}
	Vec3V SearchPos = pHideData->m_SearchVolume.GetCenter();

	scriptAssertf(0,
		"%s Object Name = %s, Object Pos = %f %f %f, Original Search Pos = %f %f %f",
		pFailReason, pObjectName, vObjectCoors.GetX(), vObjectCoors.GetY(), vObjectCoors.GetZ(), 
		SearchPos.GetXf(), SearchPos.GetYf(), SearchPos.GetZf());
#endif	//	__ASSERT
}

CDummyObject *CHiddenObjects::FindClosestMatchingDummy(hidden_object_struct *pHideData, const CObject* pObject)
{
	CDummyObject *pDummyObject = NULL;

	fwInteriorLocation InteriorLoc = pHideData->m_InteriorLocationOfRelatedDummyForObjectGrabbedFromTheWorld;

	bool bInteriorIsLoaded = false;
	bool bExteriorIsLoaded = false;
	if (InteriorLoc.IsValid())
	{
#if !__NO_OUTPUT
		scriptDisplayf("CHiddenObjects::FindClosestMatchingDummy - interior location details are");
		DisplayInteriorDetails(InteriorLoc);
#endif	//	!__NO_OUTPUT

		CInteriorProxy *pInteriorProxy = CInteriorProxy::GetFromLocation(InteriorLoc);
		if (pInteriorProxy)
		{
			if (pInteriorProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
			{
				bInteriorIsLoaded = true;
				scriptDisplayf("CHiddenObjects::FindClosestMatchingDummy - interior %d is instanced", InteriorLoc.GetInteriorProxyIndex());
			}
			else
			{
				scriptDisplayf("CHiddenObjects::FindClosestMatchingDummy - current state of interior proxy is %u", pInteriorProxy->GetCurrentState());

				scriptDisplayf("---");
				scriptDisplayf("interior & object dump...\n");
				scriptDisplayf("interior : %s, room : %d\n",pInteriorProxy->GetModelName(),InteriorLoc.GetRoomIndex());
				CDebug::DumpEntity(pObject);

				// this assert isn't a real problem - object may have been pushed into exterior
				//DisplayFailReason("CHiddenObjects::FindClosestMatchingDummy - expected interior to be instanced at this stage. If this was not the case, we should have earlied-out due to the CObject being retained.", pHideData, pObject);
			}
		}
		else
		{
			DisplayFailReason("CHiddenObjects::FindClosestMatchingDummy - pInteriorProxy is NULL.", pHideData, pObject);
		}
	}
	else
	{
// 		fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(pHideData->m_IplIndexOfObjectGrabbedFromTheWorld);
// 		if (pDef && pDef->IsLoaded())
		if ( fwMapDataStore::GetStore().HasObjectLoaded(strLocalIndex(pHideData->m_IplIndexOfObjectGrabbedFromTheWorld)) )
		{
			bExteriorIsLoaded = true;
			scriptDisplayf("CHiddenObjects::FindClosestMatchingDummy - ipl %d is loaded", pHideData->m_IplIndexOfObjectGrabbedFromTheWorld);
		}
	}

	if (bInteriorIsLoaded || bExteriorIsLoaded)
	{
		ClosestDummyObjectStruct searchData;
		searchData.m_pClosestDummyObject = NULL;

		searchData.m_ModelHash = (u32) pHideData->m_ModelHash;
		searchData.m_InteriorLocation = InteriorLoc;
		searchData.m_IplIndex = pHideData->m_IplIndexOfObjectGrabbedFromTheWorld;
		
		searchData.m_fClosestDistanceSquared = pHideData->m_SearchVolume.GetRadiusSquaredf() * 4.0f;	//	set this to larger than the scan range (remember it's squared distance)
		searchData.m_vCoordToCalculateDistanceFrom = pHideData->m_SearchVolume.GetCenter();

		SEARCH_LOCATION_FLAGS SearchFlags = SEARCH_LOCATION_EXTERIORS;	//	Search exteriors only
		if (bInteriorIsLoaded)
		{
			SearchFlags = SEARCH_LOCATION_INTERIORS;	//	Search interiors only
		}

		fwIsSphereIntersecting searchSphere(pHideData->m_SearchVolume);
		CGameWorld::ForAllEntitiesIntersecting(&searchSphere, GetClosestDummyObjectCB, (void*) &searchData, 
			ENTITY_TYPE_MASK_DUMMY_OBJECT, SearchFlags,
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);

		if (searchData.m_pClosestDummyObject)
		{
			if (scriptVerifyf(searchData.m_pClosestDummyObject->GetIsTypeDummyObject(), "CHiddenObjects::FindClosestMatchingDummy - expected entity to be a dummy object"))
			{
				pDummyObject = static_cast<CDummyObject*>(searchData.m_pClosestDummyObject);
			}
		}
	}

	return pDummyObject;
}
// ********************* End of CHiddenObjects **************************

#if __ASSERT
bool CTheScripts::IsValidGlobalVariable(int *AddressToTest)
{
	for (int i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		scrValue *pLow_addr = scrProgram::GetGlobals(i);
		scrValue *pHigh_addr = scrProgram::GetGlobals(i) + (scrProgram::GetGlobalSize(i));	//	4 *
		if ( (pLow_addr <= (scrValue *) AddressToTest) && ((scrValue *) AddressToTest < pHigh_addr) )
		{
			return true;
		}
	}

	return false;
}


void CTheScripts::DoEntityModifyChecks(const fwEntity* pEntity, unsigned assertFlags)
{
	if (scrThread::GetCurrentCmdName())
	{
		if (assertFlags & GUID_ASSERT_FLAG_ENTITY_EXISTS)
		{
			scriptAssertf(pEntity, "%s: %s - Entity doesn't exist", GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName());
		}

		if (pEntity && (pEntity->GetType() == ENTITY_TYPE_VEHICLE || pEntity->GetType() == ENTITY_TYPE_PED || pEntity->GetType() == ENTITY_TYPE_OBJECT))
		{
			const CPhysical* pPhysical = static_cast<const CPhysical*>(pEntity);

			if (assertFlags & GUID_ASSERT_FLAG_DEAD_CHECK && pEntity->GetType() != ENTITY_TYPE_OBJECT)
			{
				scriptAssertf (pPhysical->m_nDEflags.bCheckedForDead == TRUE, "%s: %s - Check entity is alive this frame", GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName());
			}

			if (assertFlags & GUID_ASSERT_FLAG_NOT_CLONE)
			{
				if (pPhysical->IsNetworkClone() || (pPhysical->GetNetworkObject() && pPhysical->GetNetworkObject()->IsPendingOwnerChange()))
				{
					scriptAssertf (0, "%s: %s - Cannot call this command on an entity (%s) owned by another machine! (Clone: %s, migrating: %s)", GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName(), pPhysical->GetNetworkObject()->GetLogName(), pPhysical->IsNetworkClone() ? "true" : "false", pPhysical->GetNetworkObject()->IsPendingOwnerChange() ? "true" : "false");
				}
			}
		}
	}
}

#endif // __ASSERT


#if __BANK

const char *CTheScripts::GetPopTypeName(ePopType PopType)
{
	switch (PopType)
	{
		case POPTYPE_UNKNOWN :
			return "POPTYPE_UNKNOWN";
		case POPTYPE_RANDOM_PERMANENT :
			return "POPTYPE_RANDOM_PERMANENT";
		case POPTYPE_RANDOM_PARKED :
			return "POPTYPE_RANDOM_PARKED";
		case POPTYPE_RANDOM_PATROL :
			return "POPTYPE_RANDOM_PATROL";
		case POPTYPE_RANDOM_SCENARIO :
			return "POPTYPE_RANDOM_SCENARIO";
		case POPTYPE_RANDOM_AMBIENT :
			return "POPTYPE_RANDOM_AMBIENT";
		case POPTYPE_PERMANENT :
			return "POPTYPE_PERMANENT";
		case POPTYPE_MISSION :
			return "POPTYPE_MISSION";
		case POPTYPE_REPLAY :
			return "POPTYPE_REPLAY";
		case POPTYPE_TOOL :
			return "POPTYPE_TOOL";
		case POPTYPE_CACHE :
			return "POPTYPE_CACHE";
		case NUM_POPTYPES :
			return "NUM_POPTYPES";
	}

	return "Unknown PopType";
}

#endif // __BANK

fwExtensibleBase* CTheScripts::GetExtensibleBaseToModifyFromGUID(int guid, const char* ASSERT_ONLY(commandName), unsigned ASSERT_ONLY(assertFlags))
{
	fwExtensibleBase* pBase = fwScriptGuid::FromGuid<fwExtensibleBase>(guid);

#if __ASSERT
	if (commandName)
	{
		if (assertFlags & GUID_ASSERT_FLAG_ENTITY_EXISTS)
		{
			scriptAssertf(pBase, "%s: %s - Object doesn't exist", GetCurrentScriptNameAndProgramCounter(), commandName);
		}
	}
#endif // __ASSERT

	return pBase;
}

CItemSet* CTheScripts::GetItemSetToModifyFromGUID(int guid, const char* ASSERT_ONLY(commandName), unsigned ASSERT_ONLY(assertFlags))
{
	CItemSet* pItemSet = fwScriptGuid::FromGuid<CItemSet>(guid);

#if __ASSERT
	if (commandName)
	{
		if (assertFlags & GUID_ASSERT_FLAG_ENTITY_EXISTS)
		{
			scriptAssertf(pItemSet, "%s: %s - Item set doesn't exist", GetCurrentScriptNameAndProgramCounter(), commandName);
		}
	}
#endif // __ASSERT

	return pItemSet;
}

CScriptedCoverPoint* CTheScripts::GetScriptedCoverPointToModifyFromGUID(int guid, const char* ASSERT_ONLY(commandName), unsigned ASSERT_ONLY(assertFlags))
{
	CScriptedCoverPoint* pScriptedCoverPoint = fwScriptGuid::FromGuid<CScriptedCoverPoint>(guid);

#if __ASSERT
	if (commandName)
	{
		if (assertFlags & GUID_ASSERT_FLAG_ENTITY_EXISTS)
		{
			scriptAssertf(pScriptedCoverPoint, "%s: %s - Scripted coverpoint doesn't exist", GetCurrentScriptNameAndProgramCounter(), commandName);
		}
	}
#endif // __ASSERT

	return pScriptedCoverPoint;
}

const fwExtensibleBase* CTheScripts::GetExtensibleBaseToQueryFromGUID(int guid, const char* commandName, unsigned assertFlags)
{
	assertFlags &= (~GUID_ASSERT_FLAG_NOT_CLONE);
	return GetExtensibleBaseToModifyFromGUID(guid, commandName, assertFlags);
}

const CItemSet* CTheScripts::GetItemSetToQueryFromGUID(int guid, const char* commandName, unsigned assertFlags)
{
	assertFlags &= (~GUID_ASSERT_FLAG_NOT_CLONE);
	return GetItemSetToModifyFromGUID(guid, commandName, assertFlags);
}

const CScriptedCoverPoint* CTheScripts::GetScriptedCoverPointToQueryFromGUID(int guid, const char* commandName, unsigned assertFlags)
{
	return GetScriptedCoverPointToModifyFromGUID(guid, commandName, assertFlags);
}

int CTheScripts::GetGUIDFromEntity(fwEntity& entity)
{
	return fwScriptGuid::CreateGuid(entity);
}

int CTheScripts::GetGUIDFromExtensibleBase(fwExtensibleBase& base)
{
	return fwScriptGuid::GetGuidFromBase(base);
}

int	CTheScripts::GetGUIDFromPreferredCoverPoint(CScriptedCoverPoint& preferredCoverPoint)
{
	return fwScriptGuid::CreateGuid(preferredCoverPoint);
}

#if !__FINAL
EXTERN_PARSER_ENUM(ScriptTaskTypes);

const char* CTheScripts::GetScriptTaskName( s32 iCommandIndex )
{
	return PARSER_ENUM(ScriptTaskTypes).NameFromValue(iCommandIndex);
}


const char* CTheScripts::GetScriptStatusName( u8 iScriptStatus )
{
	switch( iScriptStatus )
	{
	case CPedScriptedTaskRecordData::EVENT_STAGE: return "WAITING_TO_START_TASK";
	case CPedScriptedTaskRecordData::ACTIVE_TASK_STAGE: return "PERFORMING_TASK";
	case CPedScriptedTaskRecordData::DORMANT_TASK_STAGE: return "DORMANT_TASK";
	case CPedScriptedTaskRecordData::VACANT_STAGE: return "VACANT_STAGE";
	case CPedScriptedTaskRecordData::GROUP_TASK_STAGE: return "GROUP_TASK_STAGE";
	case CPedScriptedTaskRecordData::UNUSED_ATTRACTOR_SCRIPT_TASK_STAGE: return "ATTRACTOR_SCRIPT_TASK_STAGE";
	case CPedScriptedTaskRecordData::SECONDARY_TASK_STAGE: return "SECONDARY_TASK_STAGE";
	default: return "FINISHED_TASK";
	}
}
#endif //!__FINAL

CPed* CTheScripts::FindLocalPlayerPed( int playerIndex, bool ASSERT_ONLY(bAssert) )
{
	CPed* pPlayerPed = NULL;

	if (NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* pNetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));

		if (scriptVerifyf(pNetPlayer && pNetPlayer->IsMyPlayer(), "%s : %s - Can only be called on the local player", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName()))
		{
			pPlayerPed = pNetPlayer->GetPlayerPed();
		}
	}
	else if (scriptVerifyf(playerIndex==0, "%s : %s - Can only be called on the local player", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName()))
	{	
		pPlayerPed = CGameWorld::FindLocalPlayer();
	}

#if __ASSERT
	if (bAssert)
	{
		scriptAssertf(pPlayerPed, "%s : %s - Player ped doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName());
	}
#endif

	return pPlayerPed;
}

CPed* CTheScripts::FindNetworkPlayerPed( int playerIndex, bool ASSERT_ONLY(bAssert) )
{
	CPed* pPlayerPed = NULL;
	CNetGamePlayer* pNetPlayer = NULL;

	if (NetworkInterface::IsGameInProgress())
	{
		if (SCRIPT_VERIFY_PLAYER_INDEX(scrThread::GetCurrentCmdName(), playerIndex))
		{
			pNetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));

			if (pNetPlayer)
			{
				pPlayerPed = pNetPlayer->GetPlayerPed();
			}
		}
	}
	else if (scriptVerifyf(playerIndex==0, "%s : %s - Can only be called on the local player", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName()))
	{
		pPlayerPed = CGameWorld::FindLocalPlayer();
	}

#if __ASSERT
	if (bAssert)
	{
		scriptAssertf(!pPlayerPed || pPlayerPed->GetPlayerInfo(), "%s : %s - Player %d (%s) has no player info", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName(), playerIndex, pNetPlayer ? pNetPlayer->GetLogName() : "-none-");
	}
#endif

	return pPlayerPed;
}

CNetGamePlayer *CTheScripts::FindNetworkPlayer( int playerIndex, bool ASSERT_ONLY(bAssert))
{
	CNetGamePlayer* pNetPlayer = NULL;

	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s : %s - A network game is not running", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName()))
	{
		if (SCRIPT_VERIFY_PLAYER_INDEX(scrThread::GetCurrentCmdName(), playerIndex))
		{
			pNetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));

#if __ASSERT
			if (bAssert)
			{
				scriptAssertf(pNetPlayer, "%s : %s - Network player doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName());
			}
#endif
		}
	}

	return pNetPlayer;
}

bool CTheScripts::ExportGamerHandle(const rlGamerHandle* phGamer, int& handleData, int sizeOfData ASSERT_ONLY(, const char* szCommand))
{
#if __NO_OUTPUT
	if(!scriptVerifyf(phGamer->IsValid(), "%s : %s - Invalid handle!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
#else
#if __ASSERT
	char szToString[RL_MAX_GAMER_HANDLE_CHARS];
#endif // __ASSERT
	if(!scriptVerifyf(phGamer->IsValid(), "%s : %s - Invalid handle! As String: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand, phGamer->ToString(szToString)))
#endif
		return false;

	// script returns size in script words
	if(!scriptVerifyf(sizeOfData == SCRIPT_GAMER_HANDLE_SIZE, "%s : %s - Invalid data size!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
		return false;

	sizeOfData *= sizeof(scrValue); 

	// buffer to write our handle into
	u8* pHandleBuffer = reinterpret_cast<u8*>(&handleData);

	// retrieve gamer handle
	unsigned nExported = 0;
#if __NO_OUTPUT
	if(!scriptVerifyf(phGamer->Export(pHandleBuffer, sizeOfData, &nExported), "%s : %s - Export failed!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
#else
	if(!scriptVerifyf(phGamer->Export(pHandleBuffer, sizeOfData, &nExported), "%s : %s - Export failed! As String: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand, phGamer->ToString(szToString)))
#endif
		return false;
	if(!scriptVerifyf(nExported < GAMER_HANDLE_SIZE, "%s : %s - Increase GAMER_HANDLE_SIZE!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
		return false;

	return true;
}

bool CTheScripts::ImportGamerHandle(rlGamerHandle* phGamer, int& handleData, int sizeOfData ASSERT_ONLY(, const char* szCommand))
{
	// script returns size in script words
	if(!scriptVerifyf(sizeOfData == SCRIPT_GAMER_HANDLE_SIZE, "%s : %s - Invalid data size!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
		return false;

	sizeOfData *= sizeof(scrValue); 

	// buffer to write our handle into
	u8* pHandleBuffer = reinterpret_cast<u8*>(&handleData);

	// retrieve gamer handle
	unsigned nImported = 0;
	if(!scriptVerifyf(phGamer->Import(pHandleBuffer, sizeOfData, &nImported), "%s : %s - Import failed!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
	{
#if !__NO_OUTPUT
		if (sizeOfData == SCRIPT_GAMER_HANDLE_SIZE)
		{
			scriptErrorf("CTheScripts::ImportGamerHandle() - FAILED - left to right - Service 1st 8bits from left:");
			int* handleDataOut = &handleData;
			for (int i=0; i<SCRIPT_GAMER_HANDLE_SIZE; i++)
				scriptErrorf(".............. handleData[%d] = '%d'", i, handleDataOut[i]);
		}
#endif //!__NO_OUTPUT

		return false;
	}

	if(!scriptVerifyf(nImported < GAMER_HANDLE_SIZE, "%s : %s - Increase GAMER_HANDLE_SIZE!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
		return false;

	return scriptVerifyf(phGamer->IsValid(), "%s : %s - Invalid handle!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand);
}

bool CTheScripts::ImportGamerHandle(rlGamerHandle* phGamer, u8* pData ASSERT_ONLY(, const char* szCommand))
{
	// retrieve gamer handle
	unsigned nImported = 0;
	if(!scriptVerifyf(phGamer->Import(pData, GAMER_HANDLE_SIZE, &nImported), "%s : %s - Import failed! Invalid gamerhandle: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand, pData))
		return false;
	if(!scriptVerifyf(nImported < GAMER_HANDLE_SIZE, "%s : %s - Increase GAMER_HANDLE_SIZE!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand))
		return false;

	return scriptVerifyf(phGamer->IsValid(), "%s : %s - Invalid handle!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szCommand);
}

bool CTheScripts::HasEntityBeenRegisteredWithCurrentThread(const CPhysical* pPhysical)
{
	const CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();

	return (pExtension && pExtension->GetScriptInfo() && pExtension->GetScriptInfo()->GetScriptId() == CGameScriptId(*GetCurrentGtaScriptThread()));
}

int CTheScripts::GetIdForEntityRegisteredWithCurrentThread(const CPhysical* pPhysical)
{
	if (AssertVerify(HasEntityBeenRegisteredWithCurrentThread(pPhysical)))
	{
		const CScriptEntityExtension* pExtension = pPhysical->GetExtension<CScriptEntityExtension>();

		return static_cast<int>(pExtension->GetScriptInfo()->GetObjectId());
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}

void CScriptEulers::MatrixFromEulers(Matrix34& matrix, const Vector3& eulers, EulerAngleOrder rotationOrder)
{
	switch(rotationOrder)
	{
	case EULER_XYZ:
		matrix.FromEulersXYZ(eulers);
		break;

	case EULER_XZY:
		matrix.FromEulersXZY(eulers);
		break;

	case EULER_YXZ:
		matrix.FromEulersYXZ(eulers);
		break;

	case EULER_YZX:
		matrix.FromEulersYZX(eulers);
		break;

	case EULER_ZXY:
		matrix.FromEulersZXY(eulers);
		break;

	case EULER_ZYX:
		matrix.FromEulersZYX(eulers);
		break;

	default:
		scriptAssertf(false, "Invalid Euler rotation order");
		matrix.Identity();
		break;
	}
}

Vector3 CScriptEulers::MatrixToEulers(const Matrix34& matrix, EulerAngleOrder rotationOrder)
{
	Vector3 eulers;

	switch(rotationOrder)
	{
	case EULER_XYZ:
		matrix.ToEulersXYZ(eulers);
		break;

	case EULER_XZY:
		matrix.ToEulersXZY(eulers);
		break;

	case EULER_YXZ:
		matrix.ToEulersYXZ(eulers);
		break;

	case EULER_YZX:
		matrix.ToEulersYZX(eulers);
		break;

	case EULER_ZXY:
		matrix.ToEulersZXY(eulers);
		break;

	case EULER_ZYX:
		matrix.ToEulersZYX(eulers);
		break;

	default:
		scriptAssertf(false, "Invalid Euler rotation order");
		eulers.Zero();
		break;
	}

	return eulers;
}

void CScriptEulers::QuaternionFromEulers(Quaternion& quat, const Vector3& eulers, EulerAngleOrder rotationOrder)
{
	switch(rotationOrder)
	{
	case EULER_XYZ:
		quat.FromEulers(eulers, eEulerOrderXYZ);
		break;

	case EULER_XZY:
		quat.FromEulers(eulers, eEulerOrderXZY);
		break;

	case EULER_YXZ:
		quat.FromEulers(eulers, eEulerOrderYXZ);
		break;

	case EULER_YZX:
		quat.FromEulers(eulers, eEulerOrderYZX);
		break;

	case EULER_ZXY:
		quat.FromEulers(eulers, eEulerOrderZXY);
		break;

	case EULER_ZYX:
		quat.FromEulers(eulers, eEulerOrderZYX);
		break;

	default:
		scriptAssertf(false, "CScriptEulers::QuaternionFromEulers - Invalid Euler rotation order");
		quat.Identity();
		break;
	}
}

Vector3	CScriptEulers::QuaternionToEulers(Quaternion& quat, EulerAngleOrder rotationOrder)
{
	Vector3 eulers;

	switch(rotationOrder)
	{
	case EULER_XYZ:
		quat.ToEulers(eulers, eEulerOrderXYZ);
		break;

	case EULER_XZY:
		quat.ToEulers(eulers, eEulerOrderXZY);
		break;

	case EULER_YXZ:
		quat.ToEulers(eulers, eEulerOrderYXZ);
		break;

	case EULER_YZX:
		quat.ToEulers(eulers, eEulerOrderYZX);
		break;

	case EULER_ZXY:
		quat.ToEulers(eulers, eEulerOrderZXY);
		break;

	case EULER_ZYX:
		quat.ToEulers(eulers, eEulerOrderZYX);
		break;

	default:
		scriptAssertf(false, "CScriptEulers::QuaternionToEulers - Invalid Euler rotation order");
		eulers.Zero();
		break;
	}

	return eulers;
}

// *************************************************************************************************

//	CPersistentScriptGlobals CTheScripts::PersistentScriptGlobals;

/*
void CPersistentScriptGlobals::Init()
{
	m_nSizeOfPersistentGlobalsBuffer = 0;
	m_pBackupOfPersistentGlobalsBuffer = NULL;
	m_nOffsetToStartOfPersistentGlobalsBuffer = -1;
}

void CPersistentScriptGlobals::Clear()
{
	m_nSizeOfPersistentGlobalsBuffer = 0;
	if (m_pBackupOfPersistentGlobalsBuffer)
	{
		delete[] m_pBackupOfPersistentGlobalsBuffer;
	}
	m_pBackupOfPersistentGlobalsBuffer = NULL;

	m_nOffsetToStartOfPersistentGlobalsBuffer = -1;
}

void CPersistentScriptGlobals::RegisterPersistentGlobalVariables(int *pStartOfPersistentGlobals, int nSizeOfBuffer)
{
	u8 *pStartOfGlobalsMemory = reinterpret_cast<u8*>(scrProgram::GetGlobals());
	u8 *pStartOfPersistents = reinterpret_cast<u8*>(pStartOfPersistentGlobals);
	int nOffsetToGlobalsInBytes = pStartOfPersistents - pStartOfGlobalsMemory;

	scriptAssertf( (nOffsetToGlobalsInBytes >= 0) && (nOffsetToGlobalsInBytes < static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize())), "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - offset to persistent globals is out of range");
	scriptAssertf( (nOffsetToGlobalsInBytes + nSizeOfBuffer) <= static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize()), "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - persistent globals structure extends past the end of the global script memory");

	scriptAssertf((scrProgram::GetGlobals() + (nOffsetToGlobalsInBytes/sizeof(scrValue)) ) == reinterpret_cast<scrValue*>(pStartOfPersistentGlobals), "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - I've made a mistake in my calculations - Graeme");

	scriptAssertf( (m_nSizeOfPersistentGlobalsBuffer == 0) || (m_nSizeOfPersistentGlobalsBuffer == nSizeOfBuffer), "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - attempting to change the size of the persistent globals structure");
	scriptAssertf( (m_nOffsetToStartOfPersistentGlobalsBuffer == -1) || (m_nOffsetToStartOfPersistentGlobalsBuffer == nOffsetToGlobalsInBytes), "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - attempting to change the offset of the persistent globals structure");

	scriptAssertf(!m_pBackupOfPersistentGlobalsBuffer, "CPersistentScriptGlobals::RegisterPersistentGlobalVariables - didn't expect the backup buffer to be allocated at this point");

	Clear();

	m_nSizeOfPersistentGlobalsBuffer = nSizeOfBuffer;
	m_nOffsetToStartOfPersistentGlobalsBuffer = nOffsetToGlobalsInBytes;
}

bool CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer()
{
	if (m_nSizeOfPersistentGlobalsBuffer == 0)
	{
		scriptAssertf(m_nOffsetToStartOfPersistentGlobalsBuffer == -1, "CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer - size of buffer is 0 but offset is not -1");
		return true;	//	returning false would probably suggest that the function had failed rather than that the buffer didn't need to be allocated
	}

	if (m_pBackupOfPersistentGlobalsBuffer)
	{
		scriptAssertf(0, "CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer - didn't expect backup buffer to already be allocated at this stage");
		delete[] m_pBackupOfPersistentGlobalsBuffer;
		m_pBackupOfPersistentGlobalsBuffer = NULL;
	}

	scriptAssertf( (m_nOffsetToStartOfPersistentGlobalsBuffer >= 0) && (m_nOffsetToStartOfPersistentGlobalsBuffer < static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize())), "CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer - offset to persistent globals is out of range");
	scriptAssertf( (m_nOffsetToStartOfPersistentGlobalsBuffer + m_nSizeOfPersistentGlobalsBuffer) <= static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize()), "CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer - persistent globals structure extends past the end of the global script memory");

//	Allocate buffer of the correct size
	m_pBackupOfPersistentGlobalsBuffer = rage_new u8[m_nSizeOfPersistentGlobalsBuffer];
	if (m_pBackupOfPersistentGlobalsBuffer)
	{	//	Copy the global data into this buffer
		const u8 *pStartOfPersistentStructureInGlobalsMemory = reinterpret_cast<const u8*>(scrProgram::GetGlobals()) + m_nOffsetToStartOfPersistentGlobalsBuffer;
		sysMemCpy(m_pBackupOfPersistentGlobalsBuffer, pStartOfPersistentStructureInGlobalsMemory, m_nSizeOfPersistentGlobalsBuffer);
	}
	else
	{
		scriptAssertf(0, "CPersistentScriptGlobals::StorePersistentGlobalsInBackupBuffer - failed to allocate memory for the backup buffer");
		return false;
	}

	return true;
}

bool CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer()
{
	bool bSucceeded = true;

	if (scrProgram::GetGlobalSize() > 0)
	{
		if (m_pBackupOfPersistentGlobalsBuffer)
		{
			scriptAssertf(m_nSizeOfPersistentGlobalsBuffer > 0, "CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer - size of buffer to restore must be greater than 0");

			scriptAssertf( (m_nOffsetToStartOfPersistentGlobalsBuffer >= 0) && (m_nOffsetToStartOfPersistentGlobalsBuffer < static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize())), "CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer - offset to persistent globals is out of range");
			scriptAssertf( (m_nOffsetToStartOfPersistentGlobalsBuffer + m_nSizeOfPersistentGlobalsBuffer) <= static_cast<int>(sizeof(scrValue) * scrProgram::GetGlobalSize()), "CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer - persistent globals structure extends past the end of the global script memory");

			//	Copy the contents of the backup buffer back into the global data
			u8 *pStartOfPersistentStructureInGlobalsMemory = reinterpret_cast<u8*>(scrProgram::GetGlobals()) + m_nOffsetToStartOfPersistentGlobalsBuffer;
			sysMemCpy(pStartOfPersistentStructureInGlobalsMemory, m_pBackupOfPersistentGlobalsBuffer, m_nSizeOfPersistentGlobalsBuffer);
		}
		else
		{
			if ( (m_nSizeOfPersistentGlobalsBuffer != 0) || (m_nOffsetToStartOfPersistentGlobalsBuffer != -1) )
			{
				scriptAssertf(0, "CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer - no backup buffer to restore from");
				bSucceeded = false;
			}
		}
	}
	else
	{
		scriptAssertf(0, "CPersistentScriptGlobals::RestorePersistentGlobalsFromBackupBuffer - no global variables have been allocated at this stage");
		bSucceeded = false;
	}

//	free the backup buffer
	if (m_pBackupOfPersistentGlobalsBuffer)
	{
		delete[] m_pBackupOfPersistentGlobalsBuffer;
	}
	m_pBackupOfPersistentGlobalsBuffer = NULL;

	return bSucceeded;
}
*/

// *************************************************************************************************


class CDLCScriptDataMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::DLC_SCRIPT_METAFILE:
			{
				CDLCScript::AddScriptData(file.m_filename);
				return true;
			}
			break;
			default:
				return false;
		}
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file)
	{
		CDLCScript::RemoveScriptData(file.m_filename);
	}
}g_dlcScriptDataMounter;

void CDLCScript::AddScriptData(const char* fileName)
{
	CDLCScript* scriptData = rage_new CDLCScript;
	if(PARSER.LoadObject(fileName,NULL,*scriptData))
	{
		if(!CDLCScript::ms_scripts.Access(scriptData->GetScriptLiteralName().GetHash()))
		{
			scriptData->InitScript();
			CDLCScript::ms_scripts.SafeInsert(scriptData->GetScriptLiteralName().GetHash(),scriptData);
			CDLCScript::ms_scripts.FinishInsertion();
		}
		return;
	}
	delete scriptData;
}

void CDLCScript::RemoveScriptData(const char* fileName)
{
	CDLCScript* scriptData = rage_new CDLCScript;
	if(PARSER.LoadObject(fileName, NULL, *scriptData))
	{
		RemoveScript(scriptData);
	}
	delete scriptData;
}


void CDLCScript::Init(unsigned initMode)
{
	switch(initMode)
	{
		case INIT_CORE:
			CDataFileMount::RegisterMountInterface(CDataFileMgr::DLC_SCRIPT_METAFILE, &g_dlcScriptDataMounter);		
		break;
		case INIT_SESSION:
			for(dlcScriptIterator it=ms_scripts.Begin(); it!= ms_scripts.End();++it)
			{
				(*it)->InitScript();
			}
		break;
	}

}

void CDLCScript::Update()
{
	for(dlcScriptIterator it=ms_scripts.Begin(); it!= ms_scripts.End();++it)
	{
		(*it)->UpdateScript();
	}
}

void CDLCScript::Shutdown(unsigned shutdownMode)
{
	switch(shutdownMode)
	{
		case SHUTDOWN_SESSION:
		{
			for(dlcScriptIterator it=ms_scripts.Begin(); it!= ms_scripts.End();++it)
			{
				(*it)->ShutdownScript();
			}
		}
		break;
		case SHUTDOWN_WITH_MAP_LOADED:
		{
			for(dlcScriptIterator it=ms_scripts.Begin(); it!= ms_scripts.End();++it)
			{
				RemoveScript((*it));	
			}
			ms_scripts.Reset();		
		}
		break;
		//TODO map reset on tom's changes
	}
}

void CDLCScript::InitScript()
{
	const strLocalIndex scrIndex = g_StreamedScripts.FindSlotFromHashKey(m_startupScript.GetHash());
	if (Verifyf(scrIndex.IsValid(), "DLC Script %s can't be found", m_startupScript.GetCStr()))
	{
		m_request.Request(scrIndex, g_StreamedScripts.GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		m_state = CDLCScript::DLC_SCR_REQUESTED;
	}
}

void CDLCScript::UpdateScript()
{
	switch (m_state)
	{
	case CDLCScript::DLC_SCR_REQUESTED:
		{
			if ( m_request.HasLoaded() )
			{
				const int objIdx = m_request.GetRequestId().Get();
				const char* progName = "";

				m_request.ClearRequiredFlags(STRFLAG_MISSION_REQUIRED);

#if !__NO_OUTPUT
				if( m_startupScript.GetCStr() != NULL ) 
					progName = m_startupScript.GetCStr();
#endif

				m_thread = CTheScripts::GtaStartNewThreadWithProgramId(g_StreamedScripts.GetProgramId(objIdx), NULL, 0, m_scriptCallstackSize, progName);
				m_request.Release();

				if (Verifyf(m_thread!= THREAD_INVALID,  "Error launching program %s", progName))
				{
					m_state = CDLCScript::DLC_SCR_LOADED;
				}
			}

		}
		break;
	default: 
		break;			
	}
}

void CDLCScript::ShutdownScript()
{
	m_request.Release();
	if (m_thread != THREAD_INVALID)
	{
		scrThread::KillThread(m_thread);
		m_thread = THREAD_INVALID;
		strLocalIndex slot = g_StreamedScripts.FindSlotFromHashKey(GetScriptLiteralName().GetHash());
		strIndex idx =g_StreamedScripts.GetStreamingIndex(slot);
		strStreamingEngine::GetInfo().RemoveObject(idx);
	}
	m_state = CDLCScript::DLC_SCR_NONE;
}

void CDLCScript::RemoveScript(CDLCScript* dlcScript)
{
	if(CDLCScript** curScriptPtr = CDLCScript::ms_scripts.SafeGet(dlcScript->GetScriptLiteralName().GetHash()))
	{
		CDLCScript* curScript = (*curScriptPtr);
		if(curScript)
		{		
			curScript->ShutdownScript();
			curScript->GetRequest().StreamingRemove();
			ms_scripts.Remove(ms_scripts.GetIndexFromDataPtr(curScriptPtr));
		}
	}
}

bool CDLCScript::ContainsScript(atLiteralHashValue nameHash)
{
	if(CDLCScript::ms_scripts.Access(nameHash.GetHash()))
		return true;
	return false;
}

#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER

atRangeArray<bool, scrProgram::MAX_GLOBAL_BLOCKS> CScriptGlobalTamperChecker::ms_abValidPage;
bool CScriptGlobalTamperChecker::ms_bTamperingDetected = false;
s32 CScriptGlobalTamperChecker::ms_cUncheckedSection = 0;
CScriptGlobalTamperCheckerTunablesListener CScriptGlobalTamperChecker::ms_pScriptGlobalTamperTunablesListener;

void CScriptGlobalTamperChecker::Initialize()
{
	// Add the listener
	Tunables::GetInstance().AddListener(&ms_pScriptGlobalTamperTunablesListener);
}
void CScriptGlobalTamperChecker::Reset()
{
	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		ms_abValidPage[i] = false;
	}
	ms_bTamperingDetected = false;
}

bool CScriptGlobalTamperChecker::CurrentGlobalsDifferFromPreviousFrame(int &page)
{
	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		if (ms_abValidPage[i] && (ms_cUncheckedSection == 0))
		{
			if (HaveGlobalsBeenWrittenToSinceReset(i))
			{
				page = i;
				return true;
			}
		}
	}
	return ms_bTamperingDetected;
}

void CScriptGlobalTamperChecker::Update()
{
	Assertf(ms_cUncheckedSection == 0, "Resetting Global Write Tracking in unchecked Section! Called Begin without End?"); 

	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		if (scrProgram::GetGlobals(i) && ResetGlobalWriteTracking(i))
		{
			ms_abValidPage[i]= true;
		}
		else
		{
			ms_abValidPage[i] = false;
		}	
	}
}

//sets the memory for the globals as write only, letting us catch any
//exceptional cases left over in code
void CScriptGlobalTamperChecker::LockGlobalsMemory()
{
#if !RSG_FINAL
	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		if (scrProgram::GetGlobals(i))
		{
			LockGlobalsMemory(i);
		}
	}
#endif
}

//unlock the globals memory for the script update
void CScriptGlobalTamperChecker::UnlockGlobalsMemory()
{
#if !RSG_FINAL
	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		if (scrProgram::GetGlobals(i))
		{
			UnlockGlobalsMemory(i);
		}
	}
#endif
}

// Changes between a call to this and a call to EndUncheckedSection
// will not register as tampering
void CScriptGlobalTamperChecker::BeginUncheckedSection()
{
	// Do tamper check now as any results will be invalidated 
	// once we end the unchecked section
	int page = -1;
	ms_bTamperingDetected = NetworkInterface::IsGameInProgress() && (ms_cUncheckedSection == 0) && CurrentGlobalsDifferFromPreviousFrame(page);		
	if(ms_cUncheckedSection == 0 && PARAM_scriptProtectGlobals.Get())
	{
		CScriptGlobalTamperChecker::UnlockGlobalsMemory();
	}	
	ms_cUncheckedSection++;
}

void CScriptGlobalTamperChecker::EndUncheckedSection()
{
	ms_cUncheckedSection--;
	if(ms_cUncheckedSection == 0)
	{
		for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
		{
			if(ms_abValidPage[i])
			{
				ResetGlobalWriteTracking(i);
			}
		}

		if(PARAM_scriptProtectGlobals.Get())
		{
			CScriptGlobalTamperChecker::LockGlobalsMemory();
		}	
	}	
}

void CScriptGlobalTamperChecker::FakeTamper(const s32 offsetIndex)
{
#if !RSG_FINAL
	for (s32 i=0; i<scrProgram::MAX_GLOBAL_BLOCKS; i++)
	{
		if (scrProgram::GetGlobals(i) && ResetGlobalWriteTracking(i))
		{
			FakeTamper(i,offsetIndex);
		}
	}
#else
	(void)offsetIndex;
#endif
}

bool CScriptGlobalTamperChecker::ResetGlobalWriteTracking(s32 pageIndex)
{
	scrValue* globals = scrProgram::GetGlobals(pageIndex);
	if (globals)
	{
		const s32 globalSize = scrProgram::GetGlobalSize(pageIndex);
		return (ResetWriteWatch(globals, globalSize*sizeof(scrValue)) ==  0);
	}

	return false;
}

void CScriptGlobalTamperCheckerTunablesListener::OnTunablesRead()
{
	m_IsDisabled = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("DISABLE_SCRIPT_STATISTICS", 0xD4329E00), false);
}

bool CScriptGlobalTamperChecker::ShouldCheckGlobalBlock(s32 pageIndex)
{
	// Should we ignore everything because of tuneables?
	if(ms_pScriptGlobalTamperTunablesListener.IsDisabled())
		return false;

	//TODO: Extend to flexibly include other buckets via a tunable arrayn
	u32 blockNameHash = scrProgram::GetGlobalNameHash(pageIndex);

	const int numIgnorableGlobalBlocks = 3;
	const u32 ignoreableGlobalBlockNameHashes[numIgnorableGlobalBlocks] = {
		ATSTRINGHASH("startup", 0x41D6F794),
		ATSTRINGHASH("ugc_global_registration", 0x19F4EE3D),
		ATSTRINGHASH("global_fmmc_registration", 0x562E9707)};
	
	for(int i = 0; i < numIgnorableGlobalBlocks; i++)
	{
		if(blockNameHash == ignoreableGlobalBlockNameHashes[i])
			return false;
	}
	return true;
}

bool CScriptGlobalTamperChecker::HaveGlobalsBeenWrittenToSinceReset(s32 pageIndex)
{
	if(!ShouldCheckGlobalBlock(pageIndex))
		return false;

	scrValue* globals = scrProgram::GetGlobals(pageIndex);
	if (globals)
	{
		const s32 globalSize = scrProgram::GetGlobalSize(pageIndex);

		void* addressArray[128];
		ULONG_PTR count = 128;
		ULONG pageSize;

		if ( GetWriteWatch(0, globals, globalSize*sizeof(scrValue), addressArray, &count, &pageSize) == 0 )
		{
			if (count)
			{
#if !RSG_FINAL
				scriptWarningf("CScriptGlobalTamperChecker - Global %p / Page %d / Size %d / Valid Page? %x", globals, pageIndex, globalSize, ms_abValidPage[pageIndex]);
				for(int i = 0; i < count && i < 128; i++)
				{
					scriptWarningf("\tAddress %d/%d - %p", i , count-1, addressArray[i]);
				}

				if(PARAM_scriptProtectGlobals.Get())
				{
					__debugbreak();
				}
#endif
				return true;
			}
		}

	}
	return false;
}

void CScriptGlobalTamperChecker::LockGlobalsMemory(s32 pageIndex)
{
	scrValue* globals = scrProgram::GetGlobals(pageIndex);
	if (globals)
	{
		const s32 globalSize = scrProgram::GetGlobalSize(pageIndex);
		sysMemLockMemory(globals, globalSize*sizeof(scrValue));
	}
}

void CScriptGlobalTamperChecker::UnlockGlobalsMemory(s32 pageIndex)
{
	scrValue* globals = scrProgram::GetGlobals(pageIndex);
	if (globals)
	{
		const s32 globalSize = scrProgram::GetGlobalSize(pageIndex);
		sysMemUnlockMemory(globals, globalSize*sizeof(scrValue));
	}
}

void CScriptGlobalTamperChecker::FakeTamper(s32 pageIndex,const s32 offsetIndex)
{
	scrValue* globals = scrProgram::GetGlobals(pageIndex);
	if (globals)
	{
		const s32 globalSize = scrProgram::GetGlobalSize(pageIndex);
		if(offsetIndex < globalSize)
		{
			globals[offsetIndex].Int = 123;
		}
	}
}

CScriptGlobalTamperChecker::UncheckedScope::UncheckedScope()
{
	CScriptGlobalTamperChecker::BeginUncheckedSection();
}

CScriptGlobalTamperChecker::UncheckedScope::~UncheckedScope()
{
	CScriptGlobalTamperChecker::EndUncheckedSection();
}


#endif	//RSG_PC