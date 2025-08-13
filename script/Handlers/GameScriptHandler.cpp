//
// name:		NetGameScriptHandler.cpp
// description:	Project specific network script handler
// written by:	John Gurney
//


#include "script/handlers/GameScriptHandler.h"

// game includes
#include "ai/blockingbounds.h"
#include "audio/scriptaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "control/gps.h"
#include "control/record.h"
#include "control/stuntjump.h"
#include "control/WaypointRecording.h"
#include "event/EventWeapon.h"
#include "event/Events.h"
#include "frontend/MiniMap.h"
#include "game/Dispatch/DispatchData.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/DispatchServices.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/Incidents.h"
#include "game/user.h"
#include "game/weather.h"
#include "network/Debug/NetworkDebug.h"
#include "pathserver/PathServer.h"
#include "peds/AssistedMovementStore.h"
#include "peds/Ped.h"
#include "peds/pedpopulation.h"		// CPedPopulation.
#include "peds/PopCycle.h"
#include "physics/gtaInst.h"
#include "pickups/PickupManager.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "scene/InstancePriority.h"
#include "scene/LoadScene.h"
#include "scene/MapChange.h"
#include "Scene/Physical.h"
#include "scene/streamer/StreamVolume.h"
#include "Scene/world/GameWorld.h"
#include "Script/handlers/GameScriptEntity.h"
#include "Script/handlers/GameScriptResources.h"
#include "Script/gta_thread.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "script/script_itemsets.h"
#include "streaming/streaming.h"		// For CStreaming::EnableStreaming().
#include "streaming/populationstreaming.h"
#include "task/Default/Patrol/PatrolRoutes.h"
#include "task/Scenario/info/ScenarioInfo.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "timecycle/TimeCycle.h"
#include "vehicleAi/junctions.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "Vehicles/cargen.h"
#include "Vehicles/train.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxRegionInfo.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/Systems/VfxWheel.h"
#include "vfx/misc/DistantLights.h"

// framework includes
#include "fwnet/netObjectMgrBase.h"
#include "fwnet/netTypes.h"
#include "fwscene/stores/imapgroup.h"
#include "fwscript/scripthandlermgr.h"
#include "fwscript/scriptinterface.h"
#include "vfx/ptfx/ptfxmanager.h"

SCRIPT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// all handlers are network ones at the moment
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CGameScriptHandler, CGameScriptHandler::MAX_NUM_SCRIPT_HANDLERS, 0.26f, atHashString("CGameScriptHandler",0x1182232c));

CGameScriptHandler::CGameScriptHandler(scrThread& scriptThread)
: scriptHandler(scriptThread)
, m_ScriptId(CGameScriptId(scriptThread))
{
	if (scriptVerify(m_Thread))
	{
		GtaThread* pGtaThread = static_cast<GtaThread*>(m_Thread);

		pGtaThread->m_Handler = this;

#if __DEV || __BANK
		m_ScriptName = pGtaThread->GetScriptName();
#endif
	}

	if (NetworkInterface::IsGameInProgress())
	{
		CGameScriptHandlerMgr::CRemoteScriptInfo* pInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(m_ScriptId, true);

		if (pInfo)
		{
			pInfo->SetRunningLocally(true);
		}
	}
}

CGameScriptHandler::~CGameScriptHandler()
{
	if (m_Thread)
	{
		static_cast<GtaThread*>(m_Thread)->m_Handler = NULL;
	}
}

CObject* CGameScriptHandler::GetScriptCObject(ScriptObjectId objectId)
{
    scriptHandlerObject* pObj = GetScriptObject(objectId);

    if (pObj)
    {
        return static_cast<CObject*>(pObj->GetEntity());
    }

    return NULL;
}

CPhysical* CGameScriptHandler::GetScriptEntity(ScriptObjectId objectId)
{
	scriptHandlerObject* pObj = GetScriptObject(objectId);

	if (pObj)
	{
		return static_cast<CPhysical*>(pObj->GetEntity());
	}

	return NULL;
}

bool CGameScriptHandler::Update()
{
#if __DEV

	// try and assign the script name (this can stream in a little later than the thread being created)
	if (!GetScriptName())
	{
		FindScriptName();
	}
#endif

	if (m_NetComponent)
	{
		static_cast<CGameScriptHandlerNetComponent*>(m_NetComponent)->TriggerJoinEvents();
	}

	return scriptHandler::Update();
}

void CGameScriptHandler::Shutdown()
{
	CTheScripts::GetScriptHandlerMgr().GetLog();

	if (m_ObjectList.GetHead() == NULL)
	{
		scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "No objects in object list to cleanup");		
	}

	scriptHandler::Shutdown();
}

bool CGameScriptHandler::Terminate()
{
	// the following code was copied from the old mission_cleanup.cpp file. Not sure if it is all still required. Someone should review it.
	GtaThread* pGtaThread = SafeCast(GtaThread, m_Thread);

	if (AssertVerify(pGtaThread))
	{
		// Remove all ped wandering/generations areas which may have been switched on/off by this script
		if (pGtaThread->GetHasUsedPedPathCommand())
		{
			CPathServer::ClearPedSwitchAreasForScript(pGtaThread->GetThreadId());
		}
		// Remove script-controlled restrictions over what vehicle players may enter
		if (pGtaThread->GetHasOverriddenVehicleEntering() && CGameWorld::FindLocalPlayer())
		{
			CGameWorld::FindLocalPlayer()->GetPlayerInfo()->SetMayOnlyEnterThisVehicle(NULL);
		}
		// Remove script requested path node loads
		if (pGtaThread->GetThreadId() == GtaThread::GetHasLoadedAllPathNodesScriptId())
		{
			GtaThread::ClearHasLoadedAllPathNodes( ASSERT_ONLY( pGtaThread->GetThreadId()) );
			ThePaths.bLoadAllRegions = false;
		}
		// If the navmesh required slot for script was used by this thread, set it as no longer active
		if(CPathServer::GetNavmeshMeshRegionScriptID() == pGtaThread->GetThreadId())
		{
			CPathServer::RemoveNavMeshRegion( NMR_Script, pGtaThread->GetThreadId());
		}
		// Remove all blocking objects which may have been set up by this script
		CPathServerGta::RemoveAllBlockingObjectsForScript(pGtaThread->GetThreadId());

		g_LoadScene.DeregisterForScript(pGtaThread->GetThreadId());
		CStreamVolumeMgr::DeregisterForScript(pGtaThread->GetThreadId());
		g_MapChangeMgr.RemoveAllOwnedByScript(pGtaThread->GetThreadId());

		g_MapDataStore.GetGroupSwapper().AbandonOwnedByScript(pGtaThread->GetThreadId());

		// clear active script hints for entity instance priority
		CInstancePriority::SetScriptHint(CInstancePriority::SCRIPT_HINT_NONE);

		// Remove all the loaded waypoint recordings which were streamed by this script
		CWaypointRecording::RemoveAllRecordingsRequestedByScript(pGtaThread->GetThreadId());

		ThePaths.TidyUpNodeSwitchesAfterMission(pGtaThread->GetThreadId());

		g_ScriptAudioEntity.RemoveScript(pGtaThread->GetThreadId());

		g_vfxBlood.RemoveScript(pGtaThread->GetThreadId());
		g_vfxEntity.RemoveScript(pGtaThread->GetThreadId());
		g_vfxMaterial.RemoveScript(pGtaThread->GetThreadId());
		g_vfxPed.RemoveScript(pGtaThread->GetThreadId());
		g_vfxVehicle.RemoveScript(pGtaThread->GetThreadId());
		g_vfxWater.RemoveScript(pGtaThread->GetThreadId());
		g_vfxWeapon.RemoveScript(pGtaThread->GetThreadId());
		g_vfxWheel.RemoveScript(pGtaThread->GetThreadId());
		g_decalMan.RemoveScript(pGtaThread->GetThreadId());
		g_fireMan.RemoveScript(pGtaThread->GetThreadId());
		g_ptFxManager.RemoveScript(pGtaThread->GetThreadId());
		ptfxManager::RemoveScript(pGtaThread->GetThreadId());
		g_vfxRegionInfoMgr.RemoveScript(pGtaThread->GetThreadId());

		CPedGroups::ResetAllFormationsModifiedByThisScript(pGtaThread->GetThreadId());

		CDispatchManager::SetLawResponseDelayOverride(-1.0f);

		//	Should I be calling CTheScripts::GetHiddenObjects().UndoHiddenObjects() to clear all hidden objects belonging
		//	to this script? Probably best to leave it up to each individual CObject to handle its own

        if( camInterface::IsInitialized() )
		{
            camInterface::GetScriptDirector().OnScriptTermination(pGtaThread);
        }

		if (pGtaThread->IsThisAMissionScript || pGtaThread->GetIsARandomEventScript())
		{
			CVehiclePopulation::ms_overrideNumberOfParkedCars = -1;

			CPedPopulation::PedNonRemovalAreaClear();
			CPedPopulation::SetScenarioPedDensityMultipliers(1.0f, 1.0f);
			CVehiclePopulation::ms_bAllowRandomBoats = true;

			ThePaths.bIgnoreNoGpsFlag = false;
			ThePaths.bIgnoreNoGpsFlagUntilFirstNode = false;

			//Ensure the cinematic button is not disabled beyond the end of a mission.
			camInterface::GetCinematicDirector().SetCinematicButtonDisabledByScriptState(false);
			camInterface::GetGameplayDirector().SetPersistentHighAltitudeFovScalingState(true);

			g_weather.SetWindOverride(-0.01f);

			CTrain::DisableRandomTrains(false);
			CTrain::ReleaseMissionTrains(pGtaThread->GetThreadId());
			CTrain::RestoreScriptModificationsToTracks(pGtaThread->GetThreadId());
			CWanted::GetWantedBlips().EnablePoliceRadarBlips(true);
			CWanted::GetWantedBlips().ResetBlipSpriteToUseForCopPeds();
			CWanted::SetMaximumWantedLevel(WANTED_LEVEL_LAST - 1);

			CTheCarGenerators::SetAllCarGeneratorsBackToActive();
			CVehiclePopulation::ms_bAllowGarbageTrucks = true;

			CPedPopulation::SetAmbientPedDensityMultiplier(1.0f);
			CVehiclePopulation::SetParkedCarDensityMultiplier( 1.0f );
			CVehiclePopulation::SetRandomVehDensityMultiplier( 1.0f );
			CVehiclePopulation::SetScriptedRangeMultiplier( 1.0f );
			CVehiclePopulation::ms_bAllowEmergencyServicesToBeCreated = true;

			CPedPopulation::SetForcedNewAmbientPedType(-1);

			gPopStreaming.SetReduceAmbientPedModelBudget(false);
			gPopStreaming.SetReduceAmbientVehicleModelBudget(false);

			gPopStreaming.ResetPedMemoryBudgetLevel();
			gPopStreaming.ResetVehMemoryBudgetLevel();

			CPedPopulation::SetAllowCreateRandomCops(true);
			CPedPopulation::SetAllowCreateRandomCopsOnScenarios(true);
			CPedPopulation::SetAllowDummyConversions(true);
			CTheCarGenerators::bScriptDisabledCarGenerators = false;
			CGps::RenderRaceTrack(false);
			CGps::ClearGpsFlagsOnScriptExit(pGtaThread->GetThreadId());
			CGps::GetSlot(GPS_SLOT_USED_FOR_WAYPOINTS_GPS).Clear(GPS_SLOT_USED_FOR_WAYPOINTS_GPS, true);
			CGps::GetSlot(GPS_SLOT_USED_FOR_CUSTOM_ROUTE).Clear(GPS_SLOT_USED_FOR_CUSTOM_ROUTE, true);
			CGps::GetSlot(GPS_SLOT_USED_FOR_DISCRETE_ROUTE).Clear(GPS_SLOT_USED_FOR_DISCRETE_ROUTE, true);
			CPed::ms_MoneyCarriedByAllNewPeds = -1;

			if(pGtaThread->IsThisAMissionScript && SStuntJumpManager::IsInstantiated())
			{
				SStuntJumpManager::GetInstance().AbortStuntJumpInProgress();	// Do this as there was a bug with missions failing while a stuntjump was active. The slowmo would never end.
				SStuntJumpManager::GetInstance().SetActive(true);				// The script may have disabled the stunt jumps.
			}
			
			CScriptHud::fFakeMinimapAltimeterHeight = 0.0f;
			CScriptHud::ms_bColourMinimapAltimeter = false;
			CScriptHud::iFakeWantedLevel = 0;
			CScriptHud::vFakeWantedLevelPos = Vector3(0,0,0);

			ThePaths.ReleaseRequestedNodes(NODE_REQUEST_SCRIPT);

			// Call James' function to remove all navmesh required regions
			CPathServer::RemoveNavMeshRegion(NMR_Script, pGtaThread->GetThreadId());

			// Remove all assisted movement routes
			CAssistedMovementRouteStore::GetRouteToggles().ResetAll(pGtaThread->GetThreadId());

			//Removing scripted patrol nodes
			CPatrolRoutes::ClearScriptedPatrolRoutes();

			g_weather.ForceTypeClear();

			g_timeCycle.SetRegionOverride(-1);			

			g_distantLights.SetDisableVehicleLights(false);
			
			PostFX::SetNoiseOverride(false);
			PostFX::SetNoisinessOverride(0.0f); 

			RenderPhaseSeeThrough::ResetHeatScale();
			
			g_vfxWeapon.ResetIntSmokeOverrides();

			// Disable infra red vision
			RenderPhaseSeeThrough::SetState(false);

			CScriptCars::GetSuppressedCarModels().ClearAllSuppressedModels();
			CScriptPeds::GetSuppressedPedModels().ClearAllSuppressedModels();
			CScriptPeds::GetRestrictedPedModels().ClearAllRestrictedModels();

			CStreaming::EnableStreaming();
			CTheScripts::SetAllowGameToPauseForStreaming(true);

			CMiniMap::ClearSonarBlips();
			CMiniMap::AllowSonarBlips(true);

			CScriptHud::ms_fRadarZoomDistanceThisFrame = 0.0f;
			CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;
			CScriptHud::ms_iRadarZoomValue = 0;
			CScriptHud::bDisplayHud = true;
			CScriptHud::bDisplayRadar = true;
//			CTheScripts::fCameraHeadingWhenPlayerIsAttached = 0.0f;
//			CTheScripts::fCameraHeadingStepWhenPlayerIsAttached = 0.0f;
			CUserDisplay::OnscnTimer.FreezeTimers = FALSE;

//			CScriptHud::bUseMessageFormatting = false;
//			CScriptHud::MessageCentre = 0;
//			CScriptHud::MessageWidth = 0;

			// player might not exist if we are returning to the single player script
			// from a network script
			if (CGameWorld::FindLocalPlayer())
			{
				CGameWorld::FindLocalPlayer()->CleanupMissionState();

				if (CTheScripts::GetNumberOfMiniGamesInProgress() == 0 && pGtaThread->bSetPlayerControlOnInMissionCleanup)
				{
					CPlayerInfo* pPlayerInfo = CGameWorld::FindLocalPlayer()->GetPlayerInfo();

					if (scriptVerifyf(pPlayerInfo, "CGameScriptHandler::Terminate - pPlayerInfo is NULL for the local player"))
					{
						pPlayerInfo->GetWanted().m_AllRandomsFlee = FALSE;
						pPlayerInfo->GetWanted().m_AllNeutralRandomsFlee = FALSE;
						pPlayerInfo->GetWanted().m_AllRandomsOnlyAttackWithGuns = FALSE;
						pPlayerInfo->GetWanted().m_PoliceBackOff = FALSE;
						pPlayerInfo->GetWanted().m_EverybodyBackOff = FALSE;
						pPlayerInfo->GetWanted().m_IgnoreLowPriorityShockingEvents = FALSE;
						pPlayerInfo->SetPlayerControl(false, false);
					}
				}
			}

			// Clear the array used for checking for upside-down cars
			CScriptCars::GetUpsideDownCars().Init();

			// Clear the array used for checking for stuck cars
			CScriptCars::GetStuckCars().Init();

			// Reset the GunShot Range
			CEventGunShot::SetGunShotSenseRangeForRiot2(-1.0f);
			//CScriptedPriorities::Clear();

			if (CTheScripts::GetNumberOfMiniGamesInProgress() == 0 && pGtaThread->bClearHelpInMissionCleanup)
			{
				CHelpMessage::ClearAll();
			}

			// Some relationship groups we need to make sure we reset the feelings between said group and the player rel group
			if( CRelationshipManager::s_pPlayerGroup )
			{
				if( CRelationshipManager::s_pCopGroup )
				{
					CRelationshipManager::s_pPlayerGroup->ClearRelationship(CRelationshipManager::s_pCopGroup);
					CRelationshipManager::s_pCopGroup->ClearRelationship(CRelationshipManager::s_pPlayerGroup);
				}

				if( CRelationshipManager::s_pArmyGroup )
				{
					CRelationshipManager::s_pPlayerGroup->ClearRelationship(CRelationshipManager::s_pArmyGroup);
					CRelationshipManager::s_pArmyGroup->ClearRelationship(CRelationshipManager::s_pPlayerGroup);
				}

				if( CRelationshipManager::s_pSecurityGuardGroup )
				{
					CRelationshipManager::s_pPlayerGroup->ClearRelationship(CRelationshipManager::s_pSecurityGuardGroup);
					CRelationshipManager::s_pSecurityGuardGroup->ClearRelationship(CRelationshipManager::s_pPlayerGroup);
				}
			}

			// Remove any cover blocking areas added during a mission
			CCover::FlushCoverBlockingAreas();

			// Remove any ped non-creation or non-removal zones.
			CPedPopulation::PedNonCreationAreaClear();
			CPedPopulation::PedNonRemovalAreaClear();

			CDispatchManager::GetInstance().EnableAllDispatch(true);
			CDispatchManager::GetInstance().BlockAllDispatchResourceCreation(false);
			CIncidentManager::GetInstance().ClearAllIncidentsOfType(CIncident::IT_Scripted);
			
			//Reset the dispatch spawn properties.
			CDispatchSpawnProperties::GetInstance().Reset();

			//Reset the wanted response overrides.
			CWantedResponseOverrides::GetInstance().Reset();

			//Restore points that had their priority toggled.
			CScenarioPointPriorityManager::GetInstance().RestoreGroupsToOriginalPriorities();

			CJunctions::OnScriptTerminate(pGtaThread->GetThreadId());

			CDebug::DumpCollisionStatsToNetwork(pGtaThread->GetScriptName());

			CVehicle::SetLightsCutoffDistanceTweak(0.0f);
			CVehicle::ForceActualVehicleLightsOff(false);

			CPed::SetAllowHurtForAllMissionPeds(true);
		}	//	end of if (pGtaThread->IsThisAMissionScript || pGtaThread->GetIsARandomEventScript())

		if (pGtaThread->IsThisAMissionScript)
		{
			pGtaThread->IsThisAMissionScript = false;
			scriptAssertf(CTheScripts::GetPlayerIsOnAMission() == true, "%s:CMissionCleanup::Process - Mission Flag is already FALSE", pGtaThread->GetScriptName());
			CTheScripts::SetPlayerIsOnAMission(false);

			scriptDisplayf("MISSION_FLAG cleared by mission cleanup of %s\n", pGtaThread->GetScriptName());

#if __BANK
			if (fragInstNMGta::ms_bLogUsedRagdolls)
			{
				// Display max ragdolls used for the level
				Printf("\n\n");
				Displayf("[RAGDOLL WATERMARK] Mission %s ended.  A maximum of %d NM AGENTS and %d RAGE RAGDOLLS were used individually this mission.  %d fallback animations were used.", pGtaThread->GetScriptName(),
					fragInstNMGta::ms_MaxNMAgentsUsedCurrentLevel, fragInstNMGta::ms_MaxRageRagdollsUsedCurrentLevel, fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel);
				Displayf("[RAGDOLL WATERMARK] At peak usage this mission, a total of %d ragdolls were used simultaneously.  %d of those were NM agents and %d were rage ragdolls.", 
					fragInstNMGta::ms_MaxTotalRagdollsUsedCurrentLevel, fragInstNMGta::ms_MaxNMAgentsUsedInComboCurrentLevel, fragInstNMGta::ms_MaxRageRagdollsUsedInComboCurrentLevel);
				Printf("\n\n");

				// Clear counters
				fragInstNMGta::ms_MaxNMAgentsUsedCurrentLevel = 0;
				fragInstNMGta::ms_MaxRageRagdollsUsedCurrentLevel = 0;
				fragInstNMGta::ms_MaxNMAgentsUsedInComboCurrentLevel = 0;
				fragInstNMGta::ms_MaxRageRagdollsUsedInComboCurrentLevel = 0;
				fragInstNMGta::ms_MaxTotalRagdollsUsedCurrentLevel = 0;
				fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel = 0;
			}
#endif
		}

		if (pGtaThread->GetIsARandomEventScript())
		{
			pGtaThread->SetIsARandomEventScript(false);
			scriptAssertf(CTheScripts::GetPlayerIsOnARandomEvent() == true, "%s: CMissionCleanup::Process - ms_bPlayerIsOnARandomEvent is already FALSE", pGtaThread->GetScriptName());
			CTheScripts::SetPlayerIsOnARandomEvent(false);

			scriptDisplayf("Random Event flag cleared by mission cleanup of %s\n", pGtaThread->GetScriptName());
		}

		if (pGtaThread->bIsThisAMiniGameScript)
		{
			pGtaThread->TidyUpMiniGameFlag();
		}
	}

	return scriptHandler::Terminate();
}

void CGameScriptHandler::CreateNetworkComponent()
{
	Assertf(0 , "Trying to create a network component for a game script handler");
}

scriptResource* CGameScriptHandler::RegisterScriptResource(scriptResource& resource, bool *pbResourceHasJustBeenAddedToList)
{
	scriptResource* pNewResource = NULL;

#if __DEV
	if (!GetScriptName())
	{
		FindScriptName();
	}
#endif

	if (pbResourceHasJustBeenAddedToList)
	{
		*pbResourceHasJustBeenAddedToList = false;
	}

	pNewResource = GetScriptResource(resource.GetType(), resource.GetReference());

	// Argh! Can't assert here - lots of the scripts using the old mission cleanup stuff were registering resources twice, so this has to fail 
	// silently at the moment!
//	scriptAssertf(!pNewResource, "%s: Script resource %s with reference %d already exists", CTheScripts::GetCurrentScriptNameAndProgramCounter(), resource.GetResourceName(), resource.GetReference()))

	if (!pNewResource)
	{
		if (CGameScriptResource::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			pNewResource = scriptHandler::RegisterScriptResource(resource, pbResourceHasJustBeenAddedToList);
		}
#if !__FINAL
		else
		{
#if __DEV
			scriptDisplayf("CGameScriptResource is full. Here is a list of all entities and resources belonging to all scripts");
			CTheScripts::GetScriptHandlerMgr().SpewObjectAndResourceInfo();
#endif	//	__DEV

			const s32 pool_size = CGameScriptResource::GetPool()->GetSize();

			scriptDisplayf("CGameScriptResource is full. Size = %d, Number of Used Spaces = %d", pool_size, CGameScriptResource::GetPool()->GetNoOfUsedSpaces());

			s32 pool_array_index = 0;
			while (pool_array_index < pool_size)
			{
				CGameScriptResource *pCurrentResource = CGameScriptResource::GetPool()->GetSlot(pool_array_index);
				if (pCurrentResource)
				{
					scriptDisplayf("Pool index %d: Reference %d Type %s", pool_array_index, pCurrentResource->GetReference(), pCurrentResource->GetResourceName());
				}

				pool_array_index++;
			}

#if __BANK
			scriptDisplayf("%s : Ran out of script resources, probably due to a bug in the script", GetScriptName());
#endif	//	__BANK
			scriptAssertf(0, "%s : Ran out of script resources, probably due to a bug in the script", GetScriptName());
		}
#endif	//	!__FINAL

#if __DEV
		if (pNewResource)
		{
			scriptDisplayf("%s : Register new resource %s(%d) (id: %d, ref: %d)", GetScriptName(), pNewResource->GetResourceName(), pNewResource->GetType(), pNewResource->GetId(), pNewResource->GetReference());
		}
		else
		{
			scriptDisplayf("%s : Register new resource %s(%d) **failed**", GetScriptName(), resource.GetResourceName(), resource.GetType());
		}
#endif
	}

	return pNewResource;
}

bool CGameScriptHandler::RemoveScriptResource(ScriptResourceType type, const ScriptResourceRef ref, bool bDetach, bool UNUSED_PARAM(bAssert))
{
	if (!scriptHandler::RemoveScriptResource(type, ref, bDetach, false))
	{
		// Argh! Can't assert here - lots of the scripts using the old mission cleanup stuff were removing resources twice, so this has to fail 
		// silently at the moment!
		//scriptAssertf(!bAssert, "%s: Resource with type %d and ref %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type, ref);
		return false;
	}

	return true;
}

bool CGameScriptHandler::RemoveScriptResource(ScriptResourceId resourceId, bool bDetach, bool UNUSED_PARAM(bAssert), ScriptResourceType resourceType)
{
	if (!scriptHandler::RemoveScriptResource(resourceId, bDetach, false, resourceType))
	{
		// Argh! Can't assert here - lots of the scripts using the old mission cleanup stuff were removing resources twice, so this has to fail 
		// silently at the moment!
		//scriptVerifyf(!bAssert, "%s: Resource with id %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), resourceId);
		return false;
	}

	return true;

}

ScriptResourceRef CGameScriptHandler::RegisterScriptResourceAndGetRef(scriptResource& resource)
{
	scriptResource* pNewResource = RegisterScriptResource(resource);

	if (pNewResource)
	{
		return pNewResource->GetReference();
	}
	else
	{
		return resource.GetInvalidReference();
	}
}

ScriptResourceId CGameScriptHandler::RegisterScriptResourceAndGetId(scriptResource& resource)
{
	scriptResource* pNewResource = RegisterScriptResource(resource);

	if (pNewResource)
	{
		return pNewResource->GetId();
	}
	else
	{
		return scriptResource::INVALID_RESOURCE_ID;
	}
}

u32 CGameScriptHandler::GetNumRegisteredObjects() const 
{
	u32 numObjects = 0;

	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		numObjects++;
		node = node->GetNext();
	}

	return numObjects;
}

u32 CGameScriptHandler::GetNumRegisteredNetworkedObjects() const 
{
	u32 numObjects = 0;

	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		if (!node->Data->IsUnnetworkedObject())
		{
			numObjects++;
		}
		node = node->GetNext();
	}

	return numObjects;
}

u32 CGameScriptHandler::GetAllEntities(CEntity* entityArray[], u32 arrayLen)
{
	atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	u32 numEntities = 0;

	while (node && AssertVerify(numEntities<arrayLen))
	{
		if (node->Data->GetEntity())
		{
			entityArray[numEntities++] = static_cast<CEntity*>(node->Data->GetEntity());
		}

		node = node->GetNext();
	}

	return numEntities;
}

void CGameScriptHandler::GetStreamingIndices(atArray<strIndex>& streamingIndices, u32 skipFlag) const
{
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		if (node->Data->GetEntity())
		{
			strIndex index = static_cast<CScriptEntityExtension*>(node->Data)->GetStreamingIndex();
			if(!(strStreamingEngine::GetInfo().GetObjectFlags(index) & skipFlag))
				streamingIndices.PushAndGrow(index);
		}

		node = node->GetNext();
	}

	ResourceList::const_iterator curr = m_ResourceList.begin();
	ResourceList::const_iterator end = m_ResourceList.end();

	for(; curr != end; ++curr)
	{
		const CGameScriptResource* resource = static_cast<const CGameScriptResource*>(*curr);

		strIndex streamingIndex = resource->GetStreamingIndex();

		if (streamingIndex != strIndex(strIndex::INVALID_INDEX) && !(strStreamingEngine::GetInfo().GetObjectFlags(streamingIndex) & skipFlag))
		{
			streamingIndices.PushAndGrow(streamingIndex);
		}
	}
}

#if __BANK

void CGameScriptHandler::DisplayScriptHandlerInfo() const
{
	grcDebugDraw::AddDebugOutput("");
	grcDebugDraw::AddDebugOutput("%s", GetScriptName());
	grcDebugDraw::AddDebugOutput("");
	grcDebugDraw::AddDebugOutput("Num registered objects: %d", GetNumRegisteredObjects());
	grcDebugDraw::AddDebugOutput("");

	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		if (node->Data->GetEntity())
		{
			netObject* pNetObj = static_cast<CPhysical*>(node->Data->GetEntity())->GetNetworkObject();

			if (pNetObj)
			{
				grcDebugDraw::AddDebugOutput("%s", pNetObj->GetLogName());
			}
		}

		node = node->GetNext();
	}

	grcDebugDraw::AddDebugOutput("");
}

void CGameScriptHandler::DisplayObjectAndResourceInfo() const
{
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		if (node->Data->GetEntity())
		{
			static_cast<CScriptEntityExtension*>(node->Data)->DisplayDebugInfo(GetScriptName());
		}

		node = node->GetNext();
	}

	ResourceList::const_iterator curr = m_ResourceList.begin();
	ResourceList::const_iterator end = m_ResourceList.end();

	for(; curr != end; ++curr)
	{
		const CGameScriptResource* resource = static_cast<const CGameScriptResource*>(*curr);

		resource->DisplayDebugInfo(GetScriptName());
	}
}

void CGameScriptHandler::SpewObjectAndResourceInfo() const
{
	u32 count = 0;
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		node->Data->SpewDebugInfo(GetScriptName());
		node = node->GetNext();
		count++;
	}

	if (count > 0)
	{
		scriptDisplayf("CGameScriptHandler::SpewObjectAndResourceInfo - %u entities registered with %s", count, GetScriptName());
	}

	count = 0;

	ResourceList::const_iterator curr = m_ResourceList.begin();
	ResourceList::const_iterator end = m_ResourceList.end();

	for(; curr != end; ++curr)
	{
		const CGameScriptResource* resource = static_cast<const CGameScriptResource*>(*curr);

		resource->SpewDebugInfo(GetScriptName());
		count++;
	}

	if (count > 0)
	{
		scriptDisplayf("CGameScriptHandler::SpewObjectAndResourceInfo - %u resources registered with %s", count, GetScriptName());
	}
}

void CGameScriptHandler::SpewAllResourcesOfType(ScriptResourceType resourceType) const
{
	ResourceList::const_iterator curr = m_ResourceList.begin();
	ResourceList::const_iterator end = m_ResourceList.end();

	u32 count = 0;
	for(; curr != end; ++curr)
	{
		const CGameScriptResource* resource = static_cast<const CGameScriptResource*>(*curr);

		if(resource->GetType() == resourceType)
		{
			resource->SpewDebugInfo(GetScriptName());
			++count;
		}
	}

	if (count > 0)
	{
		scriptDisplayf("CGameScriptHandler::SpewAllResourcesOfType - %u resources registered with %s\n", count, GetScriptName());
	}
}
#endif // __BANK


#if __SCRIPT_MEM_CALC

#if __SCRIPT_MEM_DISPLAY

typedef struct  
{
	strIndex	index;
	u32			count;
} strIndexAndCount;

void CGameScriptHandler::DisplayUniqueObjectsForScript(const atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores) const
{
	// Now tally the streaming index's
	atArray<strIndexAndCount> talliedStreamingIndices;

	for(int i=0; i<streamingIndices.size();i++)
	{
		strIndex thisIndex = streamingIndices[i];
		int foundIDX = -1;
		for(int j=0;j<talliedStreamingIndices.size();j++)
		{
			if( talliedStreamingIndices[j].index == thisIndex )
			{
				foundIDX = j;
				break;
			}
		}
		if(foundIDX == -1)
		{
			// New
			strIndexAndCount thisTally;
			thisTally.index = thisIndex;
			thisTally.count = 1;
			talliedStreamingIndices.Grow() = thisTally;
		}
		else
		{
			// Tally
			talliedStreamingIndices[foundIDX].count++;
		}
	}

	// Now print em out tallied up
	// Get modelinfo... strStreamingEngine::GetInfo().GetObjectName(strIndex) .. 
	// to get model name. get sizes from strIndex, not sure about type... 
	//	look into that
	for(int i=0;i<talliedStreamingIndices.size();i++)
	{
		u32	virtualSize = 0;
		u32	physicalSize = 0;

		strIndexAndCount entry = talliedStreamingIndices[i];

		// Need to get fwModelId from strIndex here
		u32 index = strStreamingEngine::GetInfo().GetObjectIndex(entry.index).Get();
		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(index);
		const char *pName = strStreamingEngine::GetInfo().GetObjectName(entry.index);

		bool	bDDFlagSet = false;
		if(strStreamingEngine::GetInfo().GetObjectFlags(entry.index) & STRFLAG_DONTDELETE)
		{
			bDDFlagSet = true;
		}

		strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(entry.index, virtualSize, physicalSize, ignoreList, numIgnores, true);

		strStreamingModule* pModule = CModelInfo::GetStreamingModule();
		if (Verifyf( pModule == strStreamingEngine::GetInfo().GetModule(entry.index), "not a model info"))
		{
			char typeBuffer[64];
			if(pModelInfo)
			{
				u8	modelType = pModelInfo->GetModelType();
				if( modelType == MI_TYPE_VEHICLE )
				{
					sprintf(typeBuffer, "Vehicle");
				}
				else if( modelType == MI_TYPE_PED )
				{
					sprintf(typeBuffer, "Ped");
				}
				else
				{
					sprintf(typeBuffer, "Object");
				}
			}

			grcDebugDraw::AddDebugOutput("%s: %s %s (%d)  Model %dK %dK %s(for this entity, including deps)", 
				GetScriptName(),
				typeBuffer,
				pName,
				entry.count,
				virtualSize>>10, physicalSize>>10,
				bDDFlagSet ? "(DD)":"");
		}
	}

}

#endif	//__SCRIPT_MEM_DISPLAY


//	Is the skipFlag STRFLAG_DONTDELETE still required?
void CGameScriptHandler::CalculateMemoryUsage(atArray<strIndex>& streamingIndices, 
											  const strIndex* ignoreList, s32 numIgnores, 
					 u32 &VirtualForResourcesWithoutStreamingIndex, u32 &PhysicalForResourcesWithoutStreamingIndex,
					 bool bDisplayScriptDetails, bool bFilterDetails, u32 skipFlag) const
	{
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		fwEntity* pEntity = node->Data->GetEntity();

		if (pEntity)
		{
			strIndex index = static_cast<CScriptEntityExtension*>(node->Data)->GetStreamingIndex();
			if(!(strStreamingEngine::GetInfo().GetObjectFlags(index) & skipFlag))
			{
#if !__FINAL
				// Do not count if this is a script-created personal vehicle not owned by the local player
				if (pEntity->GetType() == ENTITY_TYPE_VEHICLE)
				{
					CVehicle* pVehicle = verify_cast<CVehicle*>(pEntity);
					if (pVehicle && pVehicle->IsRemotePersonalVehicle())
					{
						//Displayf("SCRIPT MEM: Not counting remote personal vehicle: %s", pVehicle->GetDebugName());
						node = node->GetNext();
						continue;
					}
				}
#endif
				streamingIndices.Append() = index;

#if __SCRIPT_MEM_DISPLAY
				if (bDisplayScriptDetails && !bFilterDetails)
				{
					static_cast<CScriptEntityExtension*>(node->Data)->DisplayMemoryUsage(GetScriptName(), ignoreList, numIgnores, skipFlag);
				}
#endif	//	__SCRIPT_MEM_DISPLAY

			}
		}

		node = node->GetNext();
	}

#if __SCRIPT_MEM_DISPLAY
	if (bDisplayScriptDetails && bFilterDetails)
	{
		DisplayUniqueObjectsForScript(streamingIndices, ignoreList, numIgnores);
	}
#endif	//	__SCRIPT_MEM_DISPLAY

	ResourceList::const_iterator curr = m_ResourceList.begin();
	ResourceList::const_iterator end = m_ResourceList.end();

	for(; curr != end; ++curr)
	{
		const CGameScriptResource* resource = static_cast<const CGameScriptResource*>(*curr);

		u32 NonStreamingVirtualForThisResource = 0;
		u32 NonStreamingPhysicalForThisResource = 0;
		const char *pScriptName = "";
#if __SCRIPT_MEM_DISPLAY
		pScriptName = GetScriptName();
#endif	//	__SCRIPT_MEM_DISPLAY

		resource->CalculateMemoryUsage(pScriptName, streamingIndices, ignoreList, numIgnores, 
			NonStreamingVirtualForThisResource, NonStreamingPhysicalForThisResource,
			bDisplayScriptDetails, skipFlag);
		VirtualForResourcesWithoutStreamingIndex += NonStreamingVirtualForThisResource;
		PhysicalForResourcesWithoutStreamingIndex += NonStreamingPhysicalForThisResource;
	}
}
#endif // __SCRIPT_MEM_CALC

ScriptResourceId CGameScriptHandler::GetNextFreeResourceId(scriptResource& resource) 
{
	return scriptHandler::GetNextFreeResourceId(resource);
}

void CGameScriptHandler::DetachAllUnnetworkedObjects(scriptHandlerObject* objects[MAX_UNNETWORKED_OBJECTS], u32& numObjects)
{
	atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	numObjects = 0;

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	static const unsigned LOG_NAME_LEN = 128;

	char logName[LOG_NAME_LEN];

	while (node && AssertVerify(numObjects < MAX_UNNETWORKED_OBJECTS))
	{
		atDNode<scriptHandlerObject*, datBase> *nextNode = node->GetNext();
		scriptHandlerObject* pObject = node->Data;

		if (pObject->IsUnnetworkedObject())
		{
			objects[numObjects++] = pObject;
			m_ObjectList.PopNode(*node);
			delete node;

			pObject->SetScriptHandler(NULL);

			if (log)
			{
				pObject->GetScriptInfo()->GetLogName(logName, LOG_NAME_LEN);
				NetworkLogUtils::WriteLogEvent(*log, "DETACH_UNNETWORKED_OBJECT", "%s      %s", pObject->GetScriptInfo()->GetScriptId().GetLogName(), logName);
				log->WriteDataValue("Script id", "%d", pObject->GetScriptInfo()->GetObjectId());
			}
		}

		node = nextNode;
	}
}

bool CGameScriptHandler::DestroyScriptResource(scriptResource& resource)
{
	bool success = scriptHandler::DestroyScriptResource(resource);

#if __DEV
	if (success)
	{
		scriptDisplayf("%s : Destroy script resource %s(%d) (id: %d, ref: %d)", GetScriptName(), resource.GetResourceName(), resource.GetType(), resource.GetId(), resource.GetReference());
	}
	else
	{
		scriptDisplayf("%s : Script resource %s(%d) left for other scripts (id: %d, ref: %d)", GetScriptName(), resource.GetResourceName(), resource.GetType(), resource.GetId(), resource.GetReference());
	}
#endif

	return success;
}

void CGameScriptHandler::CleanupScriptObject(scriptHandlerObject &object)
{
	scriptHandler::CleanupScriptObject(object);

	fwEntity* pEntity = object.GetEntity();

	if (pEntity)
	{
		static_cast<CDynamicEntity*>(pEntity)->DestroyScriptExtension();
	}
}

#if __DEV
void CGameScriptHandler::FindScriptName()
{
	GtaThread* pGtaThread = static_cast<GtaThread*>(m_Thread);

	// try and assign the script name (this can stream in a little later than the thread being created)
	if (!GetScriptName() && pGtaThread && pGtaThread->GetScriptName())
	{
		m_ScriptName = pGtaThread->GetScriptName();

#if __BANK
		CTheScripts::GetScriptHandlerMgr().UpdateScriptHandlerCombo();
#endif
	}

}
#endif // __DEV
