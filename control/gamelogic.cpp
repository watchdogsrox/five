/////////////////////////////////////////////////////////////////////////////////
// FILE :    gamelogic.cpp
// PURPOSE : Stuff having to do with the player dying/respawning etc
// AUTHOR :  Obbe.
// CREATED : 4/8/2000
/////////////////////////////////////////////////////////////////////////////////

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwscene/world/WorldLimits.h"

// Game headers
#include "animation/animBones.h"
#include "audio/audioZones.h"
#include "audio/northaudioengine.h"
#include "audio/policescanner.h"
#include "audio/radioaudioentity.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/restart.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "event/events.h"
#include "event/EventNetwork.h"
#include "event/ShockingEvents.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "game/cheat.h"
#include "game/clock.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/OrderManager.h"
#include "game/Dispatch/RoadBlock.h"
#include "game/modelIndices.h"
#include "Stats/StatsMgr.h"
#include "game/weather.h"
#include "Network/Live/NetworkTelemetry.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/players/NetworkPlayerMgr.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/Debug/NetworkDebug.h"
#include "peds/ped_channel.h"
#include "peds/pedFactory.h"
#include "peds/pedIntelligence.h"
#include "peds/pedpopulation.h"
#include "physics/gtaArchetype.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "peds/ped.h"
#include "renderer/renderer.h"
#include "renderer/water.h"
#include "pedgroup/PedGroup.h"
#include "peds/Ped.h"
#include "peds/pedpopulation.h"
#include "peds/PlayerInfo.h"
#include "scene/LoadScene.h"
#include "scene/scene.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "streaming/populationstreaming.h"
#include "system/timer.h"
#include "task/General/TaskBasic.h"
#include "task/Motion/Locomotion/TaskInWater.h"
#include "task/Movement/TaskParachute.h"
#include "Task/System/Task.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task\System\TaskTypes.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "text/messages.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/cargen.h"
#include "vehicles/heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "Vfx\Misc\Fire.h"
#include "vfx/particles/PtFxManager.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/projectiles/projectilemanager.h"
#include "Stats/StatsInterface.h"
#include "pickups/PickupManager.h"

#include "task/Combat/TaskDamageDeath.h"

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

NETWORK_OPTIMISATIONS()
AI_OPTIMISATIONS()

PARAM(level, "[gamelogic] level to start game in (alpine, underwater, desert, space, moon, codetest, testbed)");
PARAM(leaveDeadPed, "Leave dead ped behind when resurecting the local player");

#define	MAX_COST_OF_DYING  (10000)

int  CGameLogic::sm_CurrentLevel      = 1;
int  CGameLogic::sm_RequestedLevel    = 1;
bool CGameLogic::sm_bSuppressArrestStateTransitionThisFrame = false;

//bool CGameLogic::ms_bAttractModeActive = false;
//s32 CGameLogic::ms_iAttractModeTimeStep = 0;

CGameLogic::GameState  CGameLogic::sm_GameState;
u32		CGameLogic::sm_TimeOfLastEvent; // When did the player die/get arrested etc

u32		CGameLogic::sm_TimeOfLastRespawnAfterArrest;
u32		CGameLogic::sm_TimeOfLastRespawnAfterDeath;
u32		CGameLogic::sm_TimeOfLastTeleport;
bool	CGameLogic::sm_bTeleportActive = false;


u32		CGameLogic::sm_TimeToAbortSceneLoadWait;

// This is the stuff dealing with shortcuts after death/arrest
Vector3		CGameLogic::ShortCutDropOffForMission;
float		CGameLogic::ShortCutDropOffOrientationForMission;
bool		CGameLogic::MissionDropOffReadyToBeUsed;

Vector3     CGameLogic::sm_lastArrestOrDeathPos;
bool        CGameLogic::sm_hasLastArrestOrDeathPos = false;

#if !__FINAL
bool		CGameLogic::bDisable2ndPadForDebug = false;
#endif

#if __BANK
bool		CGameLogic::keepVehicleOnTeleport = true;
u32			CGameLogic::maxTeleportWaitTimeMs = 10000;
s32			CGameLogic::teleportLocationIndex = -1;
float		CGameLogic::teleportX = 0.f;
float		CGameLogic::teleportY = 0.f;
float		CGameLogic::teleportZ = 0.f;

bool		CGameLogic::teleportHasFinished = true;
#endif

void CGameLogic::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		sm_CurrentLevel = 1;
		sm_RequestedLevel = 1;

		PARAM_level.Get(sm_RequestedLevel);
#if !__FINAL
		CScene::GetLevelsData().Init();
		if(sm_RequestedLevel <= 0 || sm_RequestedLevel > CScene::GetLevelsData().GetCount())
		{
			const char* pLevelName = NULL;
			PARAM_level.Get(pLevelName);
			sm_RequestedLevel = CScene::GetLevelsData().GetLevelIndexFromFriendlyName(pLevelName);
		}
#endif // !__FINAL
	}
	if(initMode == INIT_SESSION)
	{
		sm_GameState = GAMESTATE_PLAYING;
		sm_TimeOfLastEvent = 0;
		sm_TimeOfLastRespawnAfterArrest = 0;
		sm_TimeOfLastRespawnAfterDeath = 0;

		sm_CurrentLevel = sm_RequestedLevel;

		COrderManager::GetInstance().InitSession();
		CDispatchManager::GetInstance().InitSession();
		CIncidentManager::GetInstance().InitSession();
	}
}

void CGameLogic::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_SESSION)
	{
		COrderManager::GetInstance().ShutdownSession();
		CDispatchManager::GetInstance().ShutdownSession();
		CIncidentManager::GetInstance().ShutdownSession();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Update game logic. If player has been arrested/killed we act upon it.
/////////////////////////////////////////////////////////////////////////////////
#define WAITTIME_DEATH (5000)
#define WAITTIME_RESPAWN_NETWORK (2000)
#define WAITTIME_DEATH_MAX (10000)		     // Never wait more than 20 seconds. Even if player is still moving
#define WAITTIME_ARRESTED (2000)

// keep these in sync with enum PLAYER_SPAWN_LOCATION in mp_globals_spawning_definitions.sch
static int SPAWN_LOCATION_NEAREST_HOSPITAL = 23;
static int SPAWN_LOCATION_NEAREST_POLICE_STATION = 24;
// keep this in sync with enum SPAWN_REASON in net_spawn.sch
static int SPAWN_REASON_DEATH = 0;

void CGameLogic::Update(void)
{
#if __BANK
	UpdateWidgets();
#endif // __BANK

	// don't process gamelogic when we are about to restart the session
	if(CGame::IsChangingSessionState())
	{
		return;
	}

	// Don't process gamelogic if Replay is in control of the world as this could potentially
	// alter the world state (e.g. delete entities) without Replay knowing about it
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		return;
	}
#endif // GTA_REPLAY

	if (NetworkInterface::IsGameInProgress())
	{
		UpdateNetwork();
		return;
	}

	CPlayerInfo	*pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	u32		TimeSinceEvent;
	static Vector3 ressurrectPosition;
	static float resurrectHeading;

	if (pPlayerInfo && pPlayerInfo->GetPlayerPed())
	{
		if (pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_PLAYING)
		{
			//Test whether player has DIED perhaps.
			if( pPlayerInfo->GetPlayerPed()->GetIsDeadOrDying() )
			{
				pPlayerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_HASDIED);
				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_DEATH;
				CGameLogic::sm_TimeOfLastEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped();	

				audNorthAudioEngine::NotifyLocalPlayerDied();
			}

			//Test whether player has been ARRESTED.
			if(pPlayerInfo->GetPlayerPed()->GetIsArrested() && !sm_bSuppressArrestStateTransitionThisFrame)
			{
				if (CPlayerInfo::PLAYERSTATE_HASBEENARRESTED != pPlayerInfo->GetPlayerState())
				{
					CStatsMgr::PlayerArrested();
				}

				pPlayerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_HASBEENARRESTED);
				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_ARREST;
				CGameLogic::sm_TimeOfLastEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

				audNorthAudioEngine::NotifyLocalPlayerArrested();
			}
		}
	}
	sm_bSuppressArrestStateTransitionThisFrame=false;

	switch (CGameLogic::sm_GameState)
	{
	case CGameLogic::GAMESTATE_RESPAWN:

		if (pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_RESPAWN)
		{
#if RSG_DURANGO
			events_durango::WriteEvent_PlayerRespawnedSP();
#endif

			pPlayerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
		}
		CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
		break;

	case CGameLogic::GAMESTATE_PLAYING:
		break;

	case CGameLogic::GAMESTATE_DEATH:
		{
/*#if NA_RADIO_ENABLED
			CRadioHud::DeActivateRadioHud(true);

			if(g_RadioAudioEntity.IsMobilePhoneRadioActive())
			{
				g_RadioAudioEntity.SetMobilePhoneRadioState(false);
			}
#endif // NA_RADIO_ENABLED*/

#if NA_POLICESCANNER_ENABLED
			// check if player has wanted level
			CWanted *pWanted = pPlayerInfo ? pPlayerInfo->GetPlayerPed()->GetPlayerWanted() : NULL;
			if(pWanted && pWanted->GetWantedLevel() > WANTED_CLEAN)
			{
				g_PoliceScanner.ReportSuspectDown();
			}
#endif // NA_POLICESCANNER_ENABLED

			// ----------------------------------------------------------------

            sm_lastArrestOrDeathPos = VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition());
            sm_hasLastArrestOrDeathPos = true;

			// fade out camera
			u32 WaitTime = NetworkInterface::IsGameInProgress() ? WAITTIME_RESPAWN_NETWORK : WAITTIME_DEATH;

			TimeSinceEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped() - CGameLogic::sm_TimeOfLastEvent;

			if (TimeSinceEvent > WaitTime)
			{		
				// Start a fade but only if the player has come to a rest or presses the 'A' button.
				Vector3	MoveSpeed = CGameWorld::FindLocalPlayerSpeed();

				if (MoveSpeed.Mag() < 0.1f || (CControlMgr::GetPlayerPad() && CControlMgr::GetPlayerPad()->ButtonCrossJustDown()) || TimeSinceEvent > WAITTIME_DEATH_MAX || pPlayerInfo->GetPlayerPed()->GetVehiclePedInside())
				{
					if(!CRestart::bSuppressFadeOutAfterDeath)
					{
						camInterface::FadeOut(2000);
					}
					else
					{
						CRestart::bSuppressFadeOutAfterDeath = false;
					}

					CGameLogic::sm_GameState = CGameLogic::GAMESTATE_DEATH_FADE;
				}
			}
		}
		break;
	case CGameLogic::GAMESTATE_DEATH_FADE:
		{
			if (!CRestart::bPausePlayerRespawnAfterDeathArrest)
			{
				static bool teleportedPlayer = false;
				if (pPlayerInfo && !camInterface::IsFading() && !teleportedPlayer)
				{
                    bool teleport = CRestart::FindClosestHospitalRestartPoint(VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition()), pPlayerInfo->GetPlayerPed()->GetTransform().GetHeading(), &ressurrectPosition, &resurrectHeading);
                    if (teleport)
                        TeleportPlayer(pPlayerInfo->GetPlayerPed(), ressurrectPosition, resurrectHeading, false, true);

					// In single player games, disable shocking events from being created until the player is resurrected.
					if (!NetworkInterface::IsGameInProgress())
					{
						CShockingEventsManager::SetDisableShockingEvents(true);
						CShockingEventsManager::ClearAllEvents();

						// Fast forward time 10 hours but don't end between 22:00 and 7:00
						CClock::PassTime(10 * 60, true);
					}
					else
					{
						// In multiplayer if we die when we fade we should clear any shocking events with ourselves as the source entity
						CShockingEventsManager::ClearAllEventsWithSourceEntity(pPlayerInfo->GetPlayerPed());
					}

					// Note: in MP we don't clear all the shocking events. It's probably somewhat safe
					// to call this function anyway, because the shocking events that survived should re-add
					// the blocking areas, so should at worst be a brief interruption in blocking area coverage.
					CShockingEventsManager::ClearAllBlockingAreas();

					teleportedPlayer = true;
				}
				else if (pPlayerInfo && !camInterface::IsFading())
				{
					if (!HasTeleportPlayerFinished())
						break;

					Assert(pPlayerInfo->GetPlayerPed());
					pedAssertf(pPlayerInfo->GetPlayerPed(), "Player with no modelinfo!");

					// ---------- do death respawn game state changes ----------------
					sm_TimeOfLastRespawnAfterDeath = fwTimer::GetTimeInMilliseconds();

					// pPlayerInfo->GetPlayerPed()->GetWeaponMgr()->RemoveAllWeapons();	In IV you don't lose weapons when dying.

					// Clean up triggered projectiles (sticky bombs, etc.)
					CProjectileManager::RemoveAllTriggeredPedProjectiles(pPlayerInfo->GetPlayerPed());

					CPedWeaponManager *pWeapMgr = pPlayerInfo->GetPlayerPed()->GetWeaponManager();
					if (pWeapMgr)
					{
						const CWeaponInfo* pWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
						if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed() )
						{
							pWeapMgr->DropWeapon(pWeaponInfo->GetHash(), false);	// Make sure we don't carry a gun coming out of hospital though
						}
						pWeapMgr->EquipWeapon(pPlayerInfo->GetPlayerPed()->GetDefaultUnarmedWeaponHash(), -1, true);
					}
					// ------------------------------------------------------------

					// resurrect player
	//				CRestart::FindClosestHospitalRestartPoint(VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition()), &ressurrectPosition, &resurrectHeading);

					ResurrectLocalPlayer(ressurrectPosition, resurrectHeading, true, false, true, SPAWN_LOCATION_NEAREST_HOSPITAL, SPAWN_REASON_DEATH);

					// fade in camera
					if (!CRestart::bSuppressFadeInAfterDeathArrest)
					{
						camInterface::FadeIn(3000);
					}
					else
					{
						CRestart::bSuppressFadeInAfterDeathArrest = false;
					}

					CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
					teleportedPlayer = false;

					StopTeleport();
				}
			}
		}
		break;
	case CGameLogic::GAMESTATE_ARREST:
		{
			// ---------- do arrest game state changes ----------------

//			NA_RADIO_ENABLED_ONLY(CRadioHud::DeActivateRadioHud(true));
			
			CShockingEventsManager::ClearAllEvents();
			CShockingEventsManager::ClearAllBlockingAreas();

			// ------------------------------------------------------------

            sm_lastArrestOrDeathPos = VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition());
            sm_hasLastArrestOrDeathPos = true;

			// fade out camera
			TimeSinceEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped() - CGameLogic::sm_TimeOfLastEvent;

			if (TimeSinceEvent > WAITTIME_ARRESTED)
			{
				if (!CRestart::bSuppressFadeOutAfterArrest)
				{
					camInterface::FadeOut(2000);
				}
				else
				{
					CRestart::bSuppressFadeOutAfterArrest = false;
				}

				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_ARREST_FADE;
			}
		}
		break;
	case CGameLogic::GAMESTATE_ARREST_FADE:
		{
			if (!CRestart::bPausePlayerRespawnAfterDeathArrest)
			{
				static bool teleportedArrestedPlayer = false;
				if (pPlayerInfo && !camInterface::IsFading() && !teleportedArrestedPlayer)
				{
					CRestart::FindClosestPoliceRestartPoint(VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition()), pPlayerInfo->GetPlayerPed()->GetTransform().GetHeading(), &ressurrectPosition, &resurrectHeading);
					TeleportPlayer(pPlayerInfo->GetPlayerPed(), ressurrectPosition, resurrectHeading, false, true);
					teleportedArrestedPlayer = true;

					if(!NetworkInterface::IsGameInProgress())
					{
						// Fast forward time 10 hours but don't end between 22:00 and 7:00
						CClock::PassTime(10 * 60, true);
					}
				}
				else if (!camInterface::IsFading())
				{
					if (pPlayerInfo && !HasTeleportPlayerFinished())
						break;

					// ---------- do arrest respawn game state changes ----------------
					sm_TimeOfLastRespawnAfterArrest = fwTimer::GetTimeInMilliseconds();

					// B* 1427491: Don't reset weapons
//					CPedInventoryLoadOutManager::SetDefaultLoadOut(pPlayerInfo->GetPlayerPed());
					// ------------------------------------------------------------

					// resurrect player
		//				CRestart::FindClosestPoliceRestartPoint(VEC3V_TO_VECTOR3(pPlayerInfo->GetPlayerPed()->GetTransform().GetPosition()), &ressurrectPosition, &resurrectHeading);

					ResurrectLocalPlayer(ressurrectPosition, resurrectHeading, true, false, true, SPAWN_LOCATION_NEAREST_POLICE_STATION, SPAWN_REASON_DEATH);

					// fade in camera
					if (!CRestart::bSuppressFadeInAfterDeathArrest)
					{
						camInterface::FadeIn(3000);
					}
					else
					{
						CRestart::bSuppressFadeInAfterDeathArrest = false;
					}

					CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
					teleportedArrestedPlayer = false;

					StopTeleport();
				}
			}
		}
		break;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateNetwork
// PURPOSE :  Update game logic in case we're in a network game
/////////////////////////////////////////////////////////////////////////////////
void CGameLogic::UpdateNetwork(void)
{
	Assert(NetworkInterface::IsGameInProgress());

	CPed* player = FindPlayerPed();
	if(!player || !player->GetPlayerInfo())
		return;

	Assertf(!player->GetIsArrested(), "In the network game the player can not be arrested");

	CPlayerInfo* playerInfo = player->GetPlayerInfo();

	const CPlayerInfo::State playerState = playerInfo->GetPlayerState();

	//Update Game State.
	switch (CGameLogic::sm_GameState)
	{
	case CGameLogic::GAMESTATE_DEATH:
		{
			bool forceFade = false;

			if ((fwTimer::GetTimeInMilliseconds_NonScaledClipped() - CGameLogic::sm_TimeOfLastEvent) > WAITTIME_RESPAWN_NETWORK)
			{
				forceFade = true;
			}

			if (playerState == CPlayerInfo::PLAYERSTATE_PLAYING || forceFade)
			{
				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_DEATH_FADE;

				// Clean up triggered projectiles (sticky bombs, etc.)
				CProjectileManager::RemoveAllTriggeredPedProjectiles(playerInfo->GetPlayerPed());
			}
		}
		break;

	//Game does not really fade in multiplayer
	case CGameLogic::GAMESTATE_DEATH_FADE:
		{
			//Teleport hasnt finished
			if (!HasTeleportPlayerFinished())
				break;

			//Death Fade is done
			if (playerState == CPlayerInfo::PLAYERSTATE_PLAYING || playerState == CPlayerInfo::PLAYERSTATE_RESPAWN)
			{
				if (playerState == CPlayerInfo::PLAYERSTATE_RESPAWN)
				{
					playerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
				}

				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
			}
		}
		break;

	case CGameLogic::GAMESTATE_RESPAWN:
		{
			//Teleport hasnt finished
			if (!HasTeleportPlayerFinished())
				break;

			bool forcePlaying = false;
			if ((fwTimer::GetTimeInMilliseconds_NonScaledClipped() - CGameLogic::sm_TimeOfLastEvent) > WAITTIME_RESPAWN_NETWORK)
			{
				forcePlaying = true;
			}

			//Respawn is done
			if (playerState == CPlayerInfo::PLAYERSTATE_PLAYING || 
				playerState == CPlayerInfo::PLAYERSTATE_RESPAWN || 
				playerState == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE || // the player can be teleported (respawned) during a cutscene
				forcePlaying)
			{
				if (playerState == CPlayerInfo::PLAYERSTATE_RESPAWN)
				{
					playerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
				}

				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
			}
		}
		break;

	case CGameLogic::GAMESTATE_PLAYING:
		break;

	case CGameLogic::GAMESTATE_ARREST_FADE:
	case CGameLogic::GAMESTATE_ARREST:
		{
			gnetAssertf(0, "Invalid game state - %d", CGameLogic::sm_GameState);
			CGameLogic::sm_GameState = CGameLogic::GAMESTATE_PLAYING;
		}
		break;
	}

	//Deal with player states
	switch (playerState)
	{
		//Update PlayerState and TimeOfPlayerStateChange
		//as the script may inquire about those. Set the death game state.
		case CPlayerInfo::PLAYERSTATE_PLAYING:
		{
			//Test whether player has DIED perhaps.
			if(player->GetIsDeadOrDying())
			{
				playerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_HASDIED);
				CGameLogic::sm_GameState = CGameLogic::GAMESTATE_DEATH;
				CGameLogic::sm_TimeOfLastEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped();	
			}
		}
		break;

		case CPlayerInfo::PLAYERSTATE_RESPAWN:
		case CPlayerInfo::PLAYERSTATE_HASDIED:
		case CPlayerInfo::PLAYERSTATE_HASBEENARRESTED:
		case CPlayerInfo::PLAYERSTATE_FAILEDMISSION:
		case CPlayerInfo::PLAYERSTATE_LEFTGAME:
		case CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE:
		default:
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ResurrectLocalPlayer
// PURPOSE :  Does stuff to the player that needs to be done for dying, getting
//			  arrested and failing critical mission.
/////////////////////////////////////////////////////////////////////////////////

void CGameLogic::ResurrectLocalPlayer(const Vector3& NewCoors, float fHeadingRad, const bool leaveDeadPed, bool advanceTime, bool unpauseRenderPhases, int spawnLocation, int spawnReason)
{
	CPed * pPlayerPed = FindPlayerPed();
	if (!AssertVerify(pPlayerPed))
		return;

	CModelInfo::ValidateCurrentMPPedMapping();

	bool triggerNetworkRespawnEvent = false;
	//This needs to be done before Resurrect of the Player - Leaves a dead ped behind.
	if ((leaveDeadPed || PARAM_leaveDeadPed.Get()) && NetworkInterface::IsGameInProgress() && gnetVerify(pPlayerPed->GetNetworkObject()))
	{
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer*>(pPlayerPed->GetNetworkObject());

		netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WritePlayerText(networkLog, NetworkInterface::GetLocalPhysicalPlayerIndex(), "TEAM_SWAP_LOCAL_RESSURRECT", netObjPlayer->GetLogName());

		if (pPlayerPed->IsInjured())
		{
			if (netObjPlayer->CanRespawnLocalPlayerPed() BANK_ONLY(&& !NetworkDebug::FailRespawnNetworkEvent()))
			{
				triggerNetworkRespawnEvent = netObjPlayer->RespawnPlayerPed(NewCoors + Vector3(0.0f, 0.0f, pPlayerPed->GetCapsuleInfo()->GetGroundToRootOffset()), false, NETWORK_INVALID_OBJECT_ID);
				pPlayerPed = netObjPlayer->GetPed();
			}
		}
		else
		{
			networkLog.WriteDataValue("Team Swap", "%s: (%s)", "Failed", "Player is not dead.");
			gnetDebug1("ResurrectLocalPlayer - Not calling RespawnPlayerPed.");
		}
	}

	if(pPlayerPed->IsFatallyInjured())
	{
		CStatsMgr::UpdateStatsOnRespawn();
	}

	// cleanup the area where the player died. Can't just remove stuff in a network game as it exists on other machines.
	if (!NetworkInterface::IsGameInProgress())
	{
		CHeli::RemovePoliceHelisUponDeathArrest();
		CPickupManager::RemoveAllPickups(false);
	}

	// now resurrect the player
	pPlayerPed->Resurrect(NewCoors, fHeadingRad, triggerNetworkRespawnEvent);

	pPlayerPed->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_SHORT);
	pPlayerPed->GetPortalTracker()->RequestRescanNextUpdate();
	pPlayerPed->GetPortalTracker()->Update(NewCoors);

	// update various systems
	CAudioZones::Update(true, NewCoors);
	camInterface::ResurrectPlayer();  
	CControlMgr::StopPlayerPadShaking();
	//CGameWorld::FindLocalPlayer()->GetPlayerInfo()->SetPlayerMoodPissedOff(150);		//2.5 minutes of being pissed off.
	CGarages::PlayerArrestedOrDied();			// Tell garages about dead player
	CHudTools::SetAsRestarted(true);  // so hud knows that we have ressurected
	CMessages::ClearMessages();
	CHelpMessage::ClearAll();
	CNetworkTelemetry::PlayerSpawn(spawnLocation, spawnReason);
	CRoadBlock::OnRespawn(*pPlayerPed);
	CNewHud::GetReticule().Reset();
	
	if (!NetworkInterface::IsGameInProgress())
	{
		//Check if the parachute pack variation is active.
		if(CTaskParachute::IsParachutePackVariationActiveForPed(*pPlayerPed))
		{
			//Clear the parachute pack variation.
			CTaskParachute::ClearParachutePackVariationForPed(*pPlayerPed);
		}

		//Check if the scuba gear variation is active.
		if(CTaskMotionSwimming::IsScubaGearVariationActiveForPed(*pPlayerPed))
		{
			//Clear the scuba gear variation.
			CTaskMotionSwimming::ClearScubaGearVariationForPed(*pPlayerPed);
		}
	}

	StatId stat = StatsInterface::GetStatsModelHashId("KILLS_SINCE_LAST_CHECKPOINT");
	if(StatsInterface::IsKeyValid(stat))
	{
		StatsInterface::SetStatData(stat, 0);
	}

	// Unpause the renderphase pause.
	if(unpauseRenderPhases)
	{
		CPauseMenu::TogglePauseRenderPhases(true, OWNER_OVERRIDE, __FUNCTION__ );
	}
	
	if(NetworkInterface::IsGameInProgress())
	{
		// Clear us as the culprit in any fires
		g_fireMan.ClearCulpritFromAllFires(pPlayerPed);
		CGameWorld::ClearCulpritFromAllCarFires(pPlayerPed);
		CShockingEventsManager::ClearAllEventsWithSourceEntity(pPlayerPed);
		CExplosionManager::DisableCrimeReportingForExplosions(pPlayerPed);
		CProjectileManager::DisableCrimeReportingForExplosions(pPlayerPed);

		CIncidentManager::GetInstance().ClearAllIncidentsOfType(CIncident::IT_Wanted);

		// clear any pending delayed kills
		CWeaponDamageEvent::ClearDelayedKill();

		//// Give game some time to fill in the world (without regard for visibility or
		//// distance from the player) before processing the population normally.
		////CVehiclePopulation::ForcePopulationInit();
		//CVehiclePopulation::InstantFillPopulation();
		//CPedPopulation::ForcePopulationInit();

		// un-network any active rockets - we let them run on all machines as they give better results
		// during respawning
#if 0 // CS - Will need to be re-implemented in new projectile system
		for(s32 index = 0; index < PROJ_MAX_PROJECTILES; index++)
		{
			CProjectileData *projData = CProjectileInfo::GetProjectile(index);

			if(projData && (projData->m_pEntProjectileOwner == pPlayerPed) && (projData->m_eProjectileType == PROJECTILE_TYPE_ROCKET))
			{
				projData->m_bNetworkClone        = false;
				projData->m_bLocalProjectileOnly = true;
			}
		}
#endif // 0
	}
	else
	{
		CIncidentManager::GetInstance().ClearAllIncidents();
		COrderManager::GetInstance().ClearAllOrders();
		CShockingEventsManager::SetDisableShockingEvents(false);

		// can't just remove stuff in a network game as it exists on other machines
		CScriptEntities::ClearSpaceForMissionEntity(pPlayerPed);

        static const float populationCreationRadius = rage::Max(CPedPopulation::GetCreationDistance(), CVehiclePopulation::GetCreationDistance());
        static const float minDistForRemoval = 2000.f + populationCreationRadius;
        static const float minDistForRemovalSqr = minDistForRemoval * minDistForRemoval;

		// clear peds and vehs only if warping far enough away, otherwise we jsut end up removing everythign we created during initial population fill
		// wasting dynamic allocs and time
		bool clearPedsAnVehs = false;
        if (sm_hasLastArrestOrDeathPos && NewCoors.Dist2(sm_lastArrestOrDeathPos) > minDistForRemovalSqr)
			clearPedsAnVehs = true;

		CGameWorld::ClearExcitingStuffFromArea(sm_lastArrestOrDeathPos, minDistForRemoval - populationCreationRadius, TRUE, true, false, false, NULL, true, clearPedsAnVehs);
        sm_hasLastArrestOrDeathPos = false;

		if(advanceTime)
		{
			// Fast forward time 10 hours but don't end between 22:00 and 7:00
			CClock::PassTime(10 * 60, true);
		}
	}

	//Set this state on player so that we can control the removal of network objects
	if (NetworkInterface::IsGameInProgress() && pPlayerPed->IsLocalPlayer() && pPlayerPed->GetPlayerInfo())
	{
		// Ensure the new player has unarmed wepaon equipped, because if they died in a vehicle and the ped got left behind
		// they may still have the vehicle weapon selected because they never got set out, but the selected weapon got copied across B*1600331
		if (pPlayerPed->GetWeaponManager())
		{
			pPlayerPed->GetWeaponManager()->GetWeaponSelector()->SetSelectedWeapon(pPlayerPed, WEAPONTYPE_UNARMED);
		}

		//Attempt fix for T-Posing after respawn in multiplayer game
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );

		CGameLogic::sm_GameState = GAMESTATE_RESPAWN;
		CGameLogic::sm_TimeOfLastEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		pPlayerPed->GetPlayerInfo()->SetPlayerState(CPlayerInfo::PLAYERSTATE_RESPAWN);

		//Apply current player scar data.
		StatsInterface::ApplyPedScarData(pPlayerPed, StatsInterface::GetCurrentMultiplayerCharaterSlot());
	}
}


void CGameLogic::TeleportPlayer(CPed* ped, const Vector3& coords, float headingInRadians, bool teleportVehicle, bool snapToGround, u32 maxWaitTimeMs, bool fadePlayerOut)
{
#if __ASSERT
    if (!HasTeleportPlayerFinished())
        Warningf("CGameLogic::TeleportPlayer: Starting new player teleport before previous one finished!");
#endif

	//Network Game In Progress
	const bool netGameInProgress = NetworkInterface::IsGameInProgress();

	if (netGameInProgress)
	{
		gnetAssert(ped->IsLocalPlayer());
		gnetDebug1("CGameLogic::TeleportPlayer - Start");
		NetworkInterface::WriteDataPopulationLog("TELEPORT_PLAYER", "CGameLogic::TeleportPlayer", "Start", "");
	}
	else
	{
		CPedPopulation::RemoveAllPedsHardNotMission();
		CVehiclePopulation::RemoveAllVehsSoft();
	}

	CVehicle* pVehicle = NULL;
	if (teleportVehicle)
	{
		pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);
	}

	//Set Local Flag for persistent owner in vehicle during player teleport so that the ownership does NOT change.
	if (netGameInProgress && ped->IsLocalPlayer())
	{
		//Set Flag in Local Player
		if (ped->GetNetworkObject())
		{
			static_cast< CNetObjPlayer* >(ped->GetNetworkObject())->SetLocalFlag(CNetObjGame::LOCALFLAG_TELEPORT, true);
		}

		//Set Flag in is vehicle
		if (pVehicle && pVehicle->GetNetworkObject())
		{
			static_cast< CNetObjVehicle* >(pVehicle->GetNetworkObject())->SetLocalFlag(CNetObjGame::LOCALFLAG_TELEPORT, true);
		}
	}

	// New warp manager functionality
	if (pVehicle)
	{
		CVehiclePopulation::SetCoordsOfScriptCar(pVehicle, coords.x, coords.y, coords.z, false, true);
	}


	Vector3 velocity(0,0,0);
	CWarpManager::SetWarp( coords, velocity, headingInRadians, teleportVehicle, snapToGround );
	CWarpManager::FadeOutPlayer(fadePlayerOut);
	CWarpManager::FadeOutAtStart(false);
	CWarpManager::FadeInWhenComplete(false);
	CWarpManager::InstantFillPopulation(true);

	sm_TimeToAbortSceneLoadWait = fwTimer::GetTimeInMilliseconds() + maxWaitTimeMs;
	sm_TimeOfLastTeleport = fwTimer::GetTimeInMilliseconds();


	//Set this state on player so that we can control the removal of network objects
	if (netGameInProgress && ped->IsLocalPlayer() && ped->GetPlayerInfo())
	{
		CGameLogic::sm_GameState = GAMESTATE_RESPAWN;
		CGameLogic::sm_TimeOfLastEvent = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

		// aargh, can't set the respawn state on the player if he is teleported during a cutscene! The game logic respawn state will handle this. Shit I know...
		if (ped->GetPlayerInfo()->GetPlayerState() != CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		{
			ped->GetPlayerInfo()->SetPlayerState(CPlayerInfo::PLAYERSTATE_RESPAWN);

			CNetObjPlayer* playerNetObj = SafeCast(CNetObjPlayer, ped->GetNetworkObject());

			//Force all the states altered by the teleport to be sent together in the next update to other players.
			// this makes sure the state change to PLAYERSTATE_RESPAWN is sent together with the fixed physics to TRUE
			// and the new postion.
			if (playerNetObj && playerNetObj->GetSyncData())
			{
				CPlayerSyncTree* playerSyncTree = SafeCast(CPlayerSyncTree, playerNetObj->GetSyncTree());
				playerSyncTree->ForceSendOfGameStateDataNode(*playerNetObj, playerNetObj->GetActivationFlags());
			}
		}
	}

	CVehiclePopulation::ClearInterestingVehicles(false);

}

void CGameLogic::StopTeleport()
{
	CWarpManager::StopWarp();
}

bool CGameLogic::IsTeleportActive()
{
	return CWarpManager::IsActive(); 
}


bool CGameLogic::HasTeleportPlayerFinished()
{
bool abortSceneLoadWait = false;

	if (!CWarpManager::IsActive())
		return true;

	abortSceneLoadWait = !CWarpManager::IsActive();

	//Network Game In Progress
	const bool netGameInProgress = NetworkInterface::IsGameInProgress();

	// fail safety in case we're out of memory
	if (fwTimer::GetTimeInMilliseconds() > sm_TimeToAbortSceneLoadWait)
	{
		if (netGameInProgress)
		{
			gnetDebug1("CGameLogic::HasTeleportPlayerFinished - End: Forced");
			NetworkInterface::WriteDataPopulationLog("TELEPORT_PLAYER", "CGameLogic::HasTeleportPlayerFinished", "End:", "Forced");
		}
		abortSceneLoadWait = true;
	}

	if (!abortSceneLoadWait)
	{

		// wait for map to stream in
		if ( CWarpManager::IsActive() )
		{
			if (netGameInProgress) gnetDebug1("HasTeleportPlayerFinished() return FALSE because of new load scene.");
			return false;
		}

		//Network game in progress - Peds/Cars pending removal or owner change
		if (CNetworkObjectPopulationMgr::PendingTransitionalObjects())
		{
			if (netGameInProgress) gnetDebug1("HasTeleportPlayerFinished() return FALSE because of PendingTransitionalObjects.");
			return false;
		}

		if (!netGameInProgress)
		{
			// wait for new peds/vehicles to stream in
			if (gPopStreaming.GetAppropriateLoadedCars().CountMembers() < 2)
				return false;

			if (gPopStreaming.GetAppropriateLoadedPeds().CountMembers() < 1)
				return false;

			// wait for peds/vehicles to spawn
			if (CVehiclePopulation::GetTotalVehsOnMap() < 10)
				return false;

			if (CPedPopulation::ms_nNumOnFootAmbient < 10)
				return false;
		}

		if (!CPedPopulation::HasForcePopulationInitFinished() || !CVehiclePopulation::HasInstantFillPopulationFinished())
			return false;
	}

	//Set some network logging
	if (netGameInProgress)
	{
		gnetDebug1("CGameLogic::HasTeleportPlayerFinished End: Normal");
		NetworkInterface::WriteDataPopulationLog("TELEPORT_PLAYER", "CGameLogic::HasTeleportPlayerFinished", "End:", "Normal");
	}

	StopTeleport();

	return true;
}

void CGameLogic::ResurrectClonePlayer(CPed *pPlayerPed, const Vector3& NewCoors, const u32 timeStamp, bool bLeaveTemporaryPedCorpse)
{
	respawnDebugf3("CGameLogic::ResurrectClonePlayer NewCoors[%f %f %f] timeStamp[%u] bLeaveTemporaryPedCorpse[%d]",NewCoors.x,NewCoors.y,NewCoors.z,timeStamp,bLeaveTemporaryPedCorpse);

	if(!AssertVerify(pPlayerPed) || !AssertVerify(pPlayerPed->IsNetworkClone()))
	{
		return;
	}

	if (NetworkInterface::IsGameInProgress() && bLeaveTemporaryPedCorpse)
	{
		if (NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_PED, false) && pPlayerPed->GetIsVisible())
		{
			CPed* clonePed = CPedFactory::GetFactory()->ClonePed(pPlayerPed,true,true,true);
			if (clonePed)
			{
				respawnDebugf3("CGameLogic::ResurrectClonePlayer--CLONED--temporary ped left behind--beaming effect enabled");

				u32 defaultRelationshipGroupHash = pPlayerPed->GetPedIntelligence()->GetRelationshipGroupDefault()->GetName().GetHash();
				u32 relationshipGroupHash = pPlayerPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash();

				//Ensure the clone ped has all the original ped config flags - as this will be the remote invocation of a player in a second
				clonePed->m_PedConfigFlags = pPlayerPed->m_PedConfigFlags;
				
				//This can distort the relationship groups with the call to SetDefaultRelationshipGroup - use the above cached values and set them for the ped after ChangePlayerPed is invoked.
				CGameWorld::ChangePlayerPed(*pPlayerPed, *clonePed);

				//Reset relationship groups are properly re-established
				CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(defaultRelationshipGroupHash);
				if( pGroup )
				{
					pPlayerPed->GetPedIntelligence()->SetRelationshipGroupDefault(pGroup);
					clonePed->GetPedIntelligence()->SetRelationshipGroupDefault(pGroup);
				}

				pGroup = CRelationshipManager::FindRelationshipGroup(relationshipGroupHash);
				if( pGroup )
				{
					pPlayerPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
					clonePed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
				}

				//Ensure that the previous ped - now corpse is visible
				pPlayerPed->SetIsVisibleForAllModules();

				//Set one ped to beam out
				pPlayerPed->SetSpecialNetworkLeave();

				//Ensure that even though the previous ped is a clone we don't continue replicate it
				if (pPlayerPed->GetNetworkObject())
				{
					pPlayerPed->GetNetworkObject()->SetLocalFlag(netObject::LOCALFLAG_NOCLONE, true);
					pPlayerPed->GetNetworkObject()->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, true);
				}

				if (clonePed->GetNetworkObject())
				{
					clonePed->GetNetworkObject()->SetLocalFlag(netObject::LOCALFLAG_NOCLONE, true);
					clonePed->GetNetworkObject()->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, true);
				}

				//Continue processing for the respawning player ped, but need to set the pointer correctly for correct processing below
				pPlayerPed = clonePed;
			}
		}
		else
		{
			Warningf("CGameLogic::ResurrectClonePlayer--bLeaveTemporaryPedCorpse requested--but couldn't CanRegisterObject--skip trying to leave a temporary ped behind for beam out effect");
		}
	}

	pPlayerPed->SetDeathState(DeathState_Alive);
	pPlayerPed->Resurrect(NewCoors, pPlayerPed->GetCurrentHeading());

	CNetObjPlayer* pPlayerObj = pPlayerPed->GetNetworkObject() ? static_cast<CNetObjPlayer*>(pPlayerPed->GetNetworkObject()) : NULL;
	Assert(pPlayerObj);

	if (pPlayerObj)
	{
        // we need to abort all clone tasks here - calling resurrect on a ped clears it's move network
        // and the tasks can get stuck in a bad state
        pPlayerPed->GetPedIntelligence()->FlushImmediately(false, false);
        pPlayerObj->AllowTaskSetting();
        pPlayerPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
        pPlayerObj->DisallowTaskSetting();

		CNetBlenderPed* pPlayerBlender = static_cast<CNetBlenderPed*>(pPlayerObj->GetNetBlender());

		respawnDebugf3("[%d] CGameLogic::ResurrectClonePlayer -- Updating net blender %.2f:%.2f:%.2f timeStamp=%d", fwTimer::GetFrameCount(), NewCoors.x, NewCoors.y, NewCoors.z, timeStamp);

		pPlayerBlender->UpdatePosition(NewCoors, timeStamp); 
		pPlayerBlender->GoStraightToTarget();
		pPlayerBlender->SetLastSyncMessageTime(timeStamp);

		respawnDebugf3("[%d] CGameLogic::ResurrectClonePlayer -- Net Blender Present %.2f:%.2f:%.2f timeStamp=%d", fwTimer::GetFrameCount(), pPlayerBlender->GetCurrentPredictedPosition().x, pPlayerBlender->GetCurrentPredictedPosition().y, pPlayerBlender->GetCurrentPredictedPosition().z, timeStamp);
		respawnDebugf3("[%d] CGameLogic::ResurrectClonePlayer -- Ped position after blender update %.2f:%.2f:%.2f timeStamp=%d", fwTimer::GetFrameCount(), pPlayerPed->GetTransform().GetPosition().GetXf(), pPlayerPed->GetTransform().GetPosition().GetYf(), pPlayerPed->GetTransform().GetPosition().GetZf(), timeStamp);

		// make sure the ped has a motion task, or he may end up T-posing
		pPlayerPed->StartPrimaryMotionTask();
	}

	// un-network any active rockets - we let them run on all machines as they give better results
	// during respawning
#if 0 // CS - Will need to be re-implemented in new projectile system
	for(s32 index = 0; index < PROJ_MAX_PROJECTILES; index++)
	{
		CProjectileData *projData = CProjectileInfo::GetProjectile(index);

		if(projData && (projData->m_pEntProjectileOwner == pPlayerPed) && (projData->m_eProjectileType == PROJECTILE_TYPE_ROCKET))
		{
			projData->m_bNetworkClone        = false;
			projData->m_bLocalProjectileOnly = true;
		}
	}
#endif // 0

	// Add Spawn network script event.
	CNetGamePlayer* pNetPlayer = NULL;
	if (NetworkInterface::IsInSession() && pPlayerPed->IsNetworkPlayer() && AssertVerify(pPlayerPed->GetPlayerInfo()) && 
		AssertVerify((pNetPlayer = NetworkInterface::GetPlayerFromGamerId(pPlayerPed->GetPlayerInfo()->m_GamerInfo.GetGamerId())) != NULL))
	{
		GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerSpawn(*pNetPlayer));
	}
}

int 
CGameLogic::GetCurrentLevelIndex() 
{ 
	return sm_CurrentLevel; 
}

void  
CGameLogic::SetCurrentLevelIndex(const int level)
{
	// Levels start on the 1st index ...
	Assert(0 < level && level <= CScene::GetLevelsData().GetCount());

	//	GSW - sm_iOldLevelIndex is not needed for now
	//	sm_iOldLevelIndex = (levelIndex != sm_CurrentLevel) ? sm_CurrentLevel : sm_iOldLevelIndex;
	sm_CurrentLevel = level;
}

int 
CGameLogic::GetRequestedLevelIndex() 
{
	return sm_RequestedLevel; 
}

void 
CGameLogic::SetRequestedLevelIndex(const int level)
{
	// Levels start on the 1st index ...
	Assert(0 < level && level <= CScene::GetLevelsData().GetCount());

	sm_RequestedLevel = level;
}

bool CGameLogic::IsRunningGTA5Map() // hack to disable the world height update on testbed levels
{
#if !__FINAL
	XPARAM(level);

	int levelIdx = -1;
	const char* pLevelName = NULL;

	PARAM_level.Get(levelIdx);

	if (levelIdx <= 0 || levelIdx > CScene::GetLevelsData().GetCount())
	{
		PARAM_level.Get(pLevelName);
	}
	else
	{
		pLevelName = CScene::GetLevelsData().GetFriendlyName(levelIdx);
	}

	if (pLevelName && stricmp(pLevelName, "gta5") != 0)
	{
		return false;
	}
#endif // !__FINAL

	return true;
}

void CGameLogic::ForceStatePlaying()
{
	Displayf("Forcing CGameLogic::sm_GameState from %d to %d", sm_GameState, GAMESTATE_PLAYING);

	if (IsTeleportActive())
		StopTeleport();

	sm_GameState = GAMESTATE_PLAYING;
}

#if __BANK
bkBank* CGameLogic::GetGameLogicBank()
{
	bkBank *bank = BANKMGR.FindBank("Game Logic");
	if( !bank )
		bank = &BANKMGR.CreateBank("Game Logic");

	Assertf(bank, "Failed to create gamelogic bank!");
	return bank;
}

void CGameLogic::TeleportPlayerCb()
{
	CPlayerInfo* playerInfo = CGameWorld::GetMainPlayerInfo();
	if(playerInfo)
	{
		CPed* playerPed = playerInfo->GetPlayerPed();
		if(playerPed)
		{
			Vector3 coords(teleportX, teleportY, teleportZ);
			TeleportPlayer(playerPed, coords, playerPed->GetTransform().GetHeading(), keepVehicleOnTeleport, true, maxTeleportWaitTimeMs);
			Displayf("\n***\nGame Logic: Teleporting player...\n***\n");
			teleportHasFinished = false;
		}
	}
}

const char* teleportLocationNames[10] = { "Michael's house", "Franklin's house", "Trevor's shack", "Strip club planning location", "Sweatshop", "Garage", "Showroom", "FBI building", "Rural bank", "Original start location" };
Vector3 teleportLocations[10] = { Vector3(-841.2872f, 158.0778f, 66.1773f), Vector3(-16.7612f, -1453.1525f, 30.5500f), Vector3(1981.2532f, 3815.8582f, 31.3914f), Vector3(128.5380f, -1307.6937f, 28.1732f), Vector3(713.8944f, -1086.5442f, 21.3346f), Vector3(189.6962f, -1255.6549f, 28.3109f), Vector3(-59.7362f, -1110.6184f, 25.4353f), Vector3(94.9453f, -742.9583f, 44.7549f), Vector3(-116.0866f, 6456.1396f, 30.4904f), Vector3(-1234.70f, -1135.56f, 6.81f) };
void CGameLogic::UpdateTeleportLocationCb()
{
	if (teleportLocationIndex == -1 || teleportLocationIndex >= 10)
		return;

	const Vector3& targetLoc = teleportLocations[teleportLocationIndex];
	teleportX = targetLoc.x;
	teleportY = targetLoc.y;
	teleportZ = targetLoc.z;
}

void CGameLogic::UpdateWidgets()
{
	if (!teleportHasFinished && HasTeleportPlayerFinished())
	{
		teleportHasFinished = true;
		Displayf("\n***\nGame Logic: Teleport finished!\n***\n");
	}
}

void CGameLogic::InitWidgets()
{
	bkBank *bank = GetGameLogicBank();
	bank->PushGroup("Safe Player Teleport");
	bank->AddToggle("Keep vehicle", &keepVehicleOnTeleport);
	bank->AddCombo("Teleport locations", &teleportLocationIndex, 10, teleportLocationNames, UpdateTeleportLocationCb);
	bank->AddSlider("Target coord X", &teleportX, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.1f);
	bank->AddSlider("Target coord Y", &teleportY, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 0.1f);
	bank->AddSlider("Target coord Z", &teleportZ, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.1f);
	bank->AddButton("Teleport player", TeleportPlayerCb);
	bank->PopGroup();
}
#endif // __BANK
