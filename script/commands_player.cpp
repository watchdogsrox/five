// Rage headers
#include "math/amath.h"
#include "script/wrapper.h"

// framework headers
#include "fwnet/nettypes.h"
#include "fwscript/scriptinterface.h"
#include "rline/rlsystemui.h"

// Game headers
#include "audio/policescanner.h"
#include "control/GameLogic.h"
#include "control/remote.h"
#include "control/garages.h"
#include "control/replay/ReplayRecording.h"
#include "control/stuntjump.h"
#include "frontend/NewHud.h"
#include "game/config.h"
#include "Game/Dispatch/DispatchData.h"
#include "game/Zones.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "modelinfo/modelinfo.h"
#include "peds/peddebugvisualiser.h"
#include "peds/pedintelligence.h"
#include "peds/pedfactory.h"
#include "peds/pedtype.h"
#include "peds/Ped.h"
#include "peds/PlayerSpecialAbility.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/pedcapsule.h"
#include "peds/PedHelmetComponent.h"
#include "peds/pedpopulation.h"
#include "peds/PlayerInfo.h"
#include "Peds/PedFlagsMeta.h"
#include "physics/WorldProbe/worldprobe.h"
#include "rline/rlnp.h"
#include "scene/LoadScene.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/gameWorld.h"
#include "script/handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "security/plugins/scripthook.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streaminginfo.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/vehicle.h"
#include "Task/Movement/TaskParachute.h"
#include "task/Default/TaskIncapacitated.h"

// network includes
#include "network/NetworkInterface.h"
#include "network/Live/livemanager.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetworkPlayerMgr.h"
#include "Network/Stats/NetworkLeaderboardCommon.h"

#include "Peds/PedWeapons/PedTargetEvaluator.h"

SCRIPT_OPTIMISATIONS ()

namespace player_commands
{
	CPed* FindPlayerPedByIndex(int playerIndex)
	{
		if (playerIndex != -1)
		{
			return CTheScripts::FindNetworkPlayerPed(playerIndex);
		}
		else
		{
			return CGameWorld::FindLocalPlayer();
		}
	}

	void CommandChangePlayerModel(int PlayerIndex, int PlayerModelHashKey)
	{
		// we want to do 'things' with the player. Better flush the draw list...
		gRenderThreadInterface.Flush();

		fwModelId playermodelId =  CModelInfo::GetModelIdFromName("player_zero");
		if (PlayerModelHashKey != DUMMY_ENTRY_IN_MODEL_ENUM_FOR_SCRIPTING_LANGUAGE)	//	use dummy enum entry to mean the standard player model
		{
			CModelInfo::GetBaseModelInfoFromHashKey((u32) PlayerModelHashKey, &playermodelId);		//	ignores return value
			if(!SCRIPT_VERIFY(playermodelId.IsValid(), "SET_PLAYER_MODEL - this is not a valid model index"))
				return;
		}

		CPed * pLocalPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

		if(pLocalPlayer)
		{
			// Update the player with the specified model
			CPed *pNewPlayerPed = CGameWorld::ChangePedModel(*pLocalPlayer, playermodelId);
			pNewPlayerPed->GetPortalTracker()->RequestRescanNextUpdate();
			pNewPlayerPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pNewPlayerPed->GetTransform().GetPosition()));
			pNewPlayerPed->GetPedIntelligence()->AddTaskDefault(pNewPlayerPed->GetCurrentMotionTask()->CreatePlayerControlTask());

	#if __DEV
			pNewPlayerPed->m_nDEflags.bCheckedForDead = true;
	#endif
		}
	}

    void CommandChangePlayerPed(int PlayerIndex, int NewPedIndex, bool KeepScriptedTasks, bool ClearPedDamage)
    {

		scriptDebugf1("%s : CHANGE_PLAYER_PED - called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        CPed *pLocalPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
        CPed *pNewPlayerPed  = CTheScripts::GetEntityToModifyFromGUID<CPed>(NewPedIndex, CTheScripts::GUID_ASSERT_FLAGS_ALL);

        if(pLocalPlayer && pNewPlayerPed)
        {

#if !__FINAL && !__NO_OUTPUT && __BANK
			if (g_PlayerSwitch.IsActive())
			{
				const Vec3V vPos = pNewPlayerPed->GetTransform().GetPosition();
				Displayf("[PlayerSwitch] Script %s called CHANGE_PLAYER_PED during player switch - new ped is %s at (%4.2f, %4.2f, %4.2f) during player switch. Call stack follows", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pNewPlayerPed->GetModelName(), vPos.GetXf(), vPos.GetYf(), vPos.GetZf());
				scrThread::PrePrintStackTrace();
			}
#endif

			CNewHud::GetReticule().Reset();

			//If the player is inside a Networked vehicle and is changing 
			//  player ped with a non networked ped then set the player out of the vehicle.
			if (NetworkInterface::IsGameInProgress())
			{
				if (pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
				{
					scriptDebugf1("%s:CHANGE_PLAYER_PED - Player inside a vehicle.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}

				if (pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && !pNewPlayerPed->GetNetworkObject())
				{
					if (pLocalPlayer->GetMyVehicle() && pLocalPlayer->GetMyVehicle()->GetNetworkObject())
					{
						scriptDebugf1("%s:CHANGE_PLAYER_PED - Player removed from vehicle.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

						pLocalPlayer->SetPedOutOfVehicle(CPed::PVF_Warp);
						pLocalPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle, false);
						pLocalPlayer->SetMyVehicle(NULL);
					}
					else if (!pLocalPlayer->GetMyVehicle())
					{
						scriptErrorf("%s:CHANGE_PLAYER_PED - Player vehicle is missing.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					}
					else if (!pLocalPlayer->GetMyVehicle()->GetNetworkObject())
					{
						scriptErrorf("%s:CHANGE_PLAYER_PED - Player vehicle is not networked.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					}
				}
				else if (pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pNewPlayerPed->GetNetworkObject())
				{
					scriptDebugf1("%s:CHANGE_PLAYER_PED - Ped swapwed with the player has a network object.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}

            // Update the player with the specified model
            CGameWorld::ChangePlayerPed(*pLocalPlayer, *pNewPlayerPed, KeepScriptedTasks, ClearPedDamage);

			//Add more debug output.
			if (NetworkInterface::IsGameInProgress())
			{
				if (pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
				{
					scriptErrorf("%s:CHANGE_PLAYER_PED - Old Player not removed from vehicle.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
        }
    }

	int CommandGetPlayerPed(int PlayerIndex)
	{
		CPed *pPlayerPed = FindPlayerPedByIndex(PlayerIndex);
		return pPlayerPed ? CTheScripts::GetGUIDFromEntity(*pPlayerPed) : 0;
 	}

	// Gets the script guid for a player
	int CommandGetPlayerScriptGuid(int PlayerIndex)
	{
		CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		if(pPlayer)
		{
			return fwScriptGuid::CreateGuid(*pPlayer);
		}
		return 0;
	}

	bool CommandPlayerHasPed(int PlayerIndex)
	{
		CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);
		
		if (pPlayer)
		{
#if __DEV
			//	Chris Rothwell needed to be able to call some ped commands for a player before a network game starts even if the player was actually dead.
			//	Seems a bit dodgy setting this flag for a dead/arrested player - no decision has been made yet concerning if it's possible for a player to 
			//	join a network game while he is dead/arrested
			pPlayer->m_nDEflags.bCheckedForDead = true;
#endif
			return true;
		}

		return false;
	}

    void CommandGetPlayerRGBColour(int playerIndex, int &Red, int &Green, int &Blue)
    {
		if (SCRIPT_VERIFY_PLAYER_INDEX("GET_PLAYER_RGB_COLOUR", playerIndex))
		{
			Color32 playerColour = NetworkColours::GetNetworkColour(NetworkColours::GetDefaultPlayerColour(static_cast<PhysicalPlayerIndex>(playerIndex)));

		    Red   = playerColour.GetRed();
			Green = playerColour.GetGreen();
			Blue = playerColour.GetBlue();
        }
    }

	int CommandGetPlayerHUDColour(int playerIndex)
	{
		if (SCRIPT_VERIFY_PLAYER_INDEX("GET_NATIVE_PLAYER_HUD_COLOUR", playerIndex))
		{
			return NetworkColours::MapNetworkColourToHudColour(NetworkColours::GetDefaultPlayerColour(static_cast<PhysicalPlayerIndex>(playerIndex)));
		}

		return HUD_COLOUR_GREY;
	}

    s32 CommandGetNumberOfPlayers()
    {
		if (NetworkInterface::IsGameInProgress())
		{
			SCRIPT_CHECK_CALLING_FUNCTION
			return NetworkInterface::GetPlayerMgr().GetNumPhysicalPlayers();
		}
		
		return 1;
    }

	s32 CommandGetPlayerTeam(int PlayerIndex)
	{
		CNetGamePlayer *player = CTheScripts::FindNetworkPlayer(PlayerIndex);
		
		if (player)
		{
			return (player->GetTeam());
		}

		return -1;
	}

	void CommandSetPlayerTeam(int PlayerIndex, int Team)
	{
		if(SCRIPT_VERIFY(Team>=-1&&Team<MAX_NUM_TEAMS, "SET_PLAYER_TEAM - Invalid team number"))
		{
			if (NetworkInterface::IsGameInProgress())
			{
				CNetGamePlayer *player = CTheScripts::FindNetworkPlayer(PlayerIndex);

				if (player)
				{
					player->SetTeam(Team);

					if (player->IsLocal())
					{
						CGameWorld::GetMainPlayerInfo()->Team = Team;
					}
				}
			}
			else
			{
				if (scriptVerifyf(PlayerIndex == 0, "%s:SET_PLAYER_TEAM - Player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					CGameWorld::GetMainPlayerInfo()->Team = Team;
				}
			}
		}
	}

    int CommandGetNumPlayersInTeam(int Team)
    {
        int numPlayers = 0;

		if (NetworkInterface::IsGameInProgress())
		{
			unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

				if (player->GetTeam() == Team)
				{
					numPlayers++; 
				}
			}
		}
		else
		{
			if (CGameWorld::GetMainPlayerInfo()->Team == Team)
			{
				numPlayers++; 
			}
		}

        return numPlayers;
    }

	const char* GetPlayerName(int PlayerIndex, const char* ASSERT_ONLY(commandName))
	{
		if (NetworkInterface::IsGameInProgress())
		{
			netPlayer* player = CTheScripts::FindNetworkPlayer(PlayerIndex, false);

			if (player)
			{
				return ( player->GetGamerInfo().GetDisplayName() );
			}
		}
		else if(CGameWorld::GetMainPlayerInfo())
		{
			if (scriptVerifyf(PlayerIndex == 0, "%s:%s - Player does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName))
			{
				return CGameWorld::GetMainPlayerInfo()->m_GamerInfo.GetDisplayName();
			}
		}

		return "**Invalid**";
	}

	const char* CommandGetPlayerName(int PlayerIndex)
	{
		return GetPlayerName(PlayerIndex, "GET_PLAYER_NAME");
	}

	float CommandGetWantedLevelRadius(int iWantedLevel )
	{
		SCRIPT_ASSERT( (iWantedLevel >= WANTED_CLEAN && iWantedLevel < WANTED_LEVEL_LAST), "GET_WANTED_LEVEL_THRESHOLD - Wanted level is beyond range of existing levels");	
		return CDispatchData::GetWantedZoneRadius(iWantedLevel);
	}

	scrVector CommandGetWantedRadiusCentre(int PlayerIndex)
	{
		scrVector result(VEC3_ZERO);

		CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		if(SCRIPT_VERIFY(pPlayer, "GET_PLAYER_WANTED_CENTRE_POSITION - Player character doesn't exit"))
		{
			if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "GET_PLAYER_WANTED_CENTRE_POSITION - Player character doesn't have a wanted class"))
			{
				result = pPlayer->GetPlayerWanted()->m_LastSpottedByPolice;
			}
		}
		return result;
	}

	void CommandSetWantedRadiusCentre(int PlayerIndex, const scrVector & scrVecCoors)
	{
		CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
		if(SCRIPT_VERIFY(pPlayer, "SET_PLAYER_WANTED_CENTRE_POSITION - Player character doesn't exit"))
		{
			if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_PLAYER_WANTED_CENTRE_POSITION - Player character doesn't have a wanted class"))
			{
				pPlayer->GetPlayerWanted()->m_LastSpottedByPolice = scrVecCoors;
				pPlayer->GetPlayerWanted()->m_CurrentSearchAreaCentre = scrVecCoors;
			}
		}
	}

	int CommandGetWantedLevelThreshold(int wantedLevel)
	{
		SCRIPT_ASSERT( (wantedLevel >= WANTED_CLEAN && wantedLevel < WANTED_LEVEL_LAST), "GET_WANTED_LEVEL_THRESHOLD - Wanted level is beyond range of existing levels");	
		return CDispatchData::GetWantedLevelThreshold(wantedLevel);
	}

void CommandAlterWantedLevel( int PlayerIndex, int iNewLevel, bool bDelayLawResponse )
{
#if !__FINAL
	scrThread::PrePrintStackTrace();
#endif
	scriptDebugf1("%s [Frame=%d] ALTER_WANTED_LEVEL :%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), iNewLevel);
	wantedDebugf3("%s [Frame=%d] ALTER_WANTED_LEVEL CommandAlterWantedLevel :%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), iNewLevel);

	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "ALTER_WANTED_LEVEL - Check player character is alive this frame");

	pPlayer->GetPlayerWanted()->SetShouldDelayLawResponse(bDelayLawResponse);
	pPlayer->GetPlayerWanted()->SetLastTimeWantedSetByScript(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));

	const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	if (iNewLevel < pPlayer->GetPlayerWanted()->GetWantedLevel())
	{
		pPlayer->GetPlayerWanted()->SetWantedLevel(vPlayerPosition, iNewLevel, 0, WL_FROM_SCRIPT);
		NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.CancelAnyPlayingReportsNextFade());
	}
	else
	{
		pPlayer->GetPlayerWanted()->SetWantedLevel(vPlayerPosition, iNewLevel, WANTED_CHANGE_DELAY_DEFAULT, WL_FROM_SCRIPT);
	}

#if __BANK
	safecpy(pPlayer->GetPlayerWanted()->m_WantedLevelLastSetFromScriptName, CTheScripts::GetCurrentScriptName());
#endif
}

void CommandAlterWantedLevelNoDrop( int PlayerIndex, int iNewLevel, bool bDelayLawResponse )
{
#if !__FINAL
	scrThread::PrePrintStackTrace();
#endif
	scriptDebugf1("%s [Frame=%d] SET_PLAYER_WANTED_LEVEL_NO_DROP. :%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), iNewLevel);
	wantedDebugf3("%s [Frame=%d] SET_PLAYER_WANTED_LEVEL_NO_DROP CommandAlterWantedLevelNoDrop. :%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), iNewLevel);

	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	pPlayer->GetPlayerWanted()->SetShouldDelayLawResponse(bDelayLawResponse);
	pPlayer->GetPlayerWanted()->SetLastTimeWantedSetByScript(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	pPlayer->GetPlayerWanted()->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), iNewLevel, WANTED_CHANGE_DELAY_DEFAULT, WL_FROM_SCRIPT);

#if __BANK
	safecpy(pPlayer->GetPlayerWanted()->m_WantedLevelLastSetFromScriptName, CTheScripts::GetCurrentScriptName());
#endif
}


void CommandApplyWantedLevelChangeNow( int PlayerIndex, bool bDelayLawResponse )
{
#if !__FINAL
	scrThread::PrePrintStackTrace();
#endif
	scriptDebugf1("%s [Frame=%d] SET_PLAYER_WANTED_LEVEL_NOW", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount());
	wantedDebugf3("SET_PLAYER_WANTED_LEVEL_NOW CommandApplyWantedLevelChangeNow");

	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_WANTED_LEVEL_NOW - Check player character is alive this frame");

	pPlayer->GetPlayerWanted()->m_TimeWhenNewWantedLevelTakesEffect = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetSyncedTimeInMilliseconds() - 1 : fwTimer::GetTimeInMilliseconds() - 1;
	pPlayer->GetPlayerWanted()->SetShouldDelayLawResponse(bDelayLawResponse);
	pPlayer->GetPlayerWanted()->SetLastTimeWantedSetByScript(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	pPlayer->GetPlayerWanted()->Update(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), NULL);
}

bool CommandPlayerHasFlashingStarsAboutToDrop( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed( PlayerIndex );

	if( !pPlayer )
		return false;

	SCRIPT_ASSERT( pPlayer->m_nDEflags.bCheckedForDead == TRUE, "ARE_PLAYER_FLASHING_STARS_ABOUT_TO_DROP - Check player character is alive this frame" );

	const CWanted& rWanted = *pPlayer->GetPlayerWanted();
	return rWanted.m_WantedLevel >= WANTED_LEVEL1 && (rWanted.m_WantedLevelBeforeParole != WANTED_CLEAN || rWanted.m_bIsOutsideCircle);
}

bool CommandPlayerHasGreyedOutStars( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "ARE_PLAYER_STARS_GREYED_OUT - Check player character is alive this frame");

	CWanted *pWanted = pPlayer->GetPlayerWanted();
	return (pWanted->m_WantedLevel >= WANTED_LEVEL1 && pWanted->CopsAreSearching());
}

bool IsWantedAndHasBeenSeenByCops( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "ARE_PLAYER_STARS_GREYED_OUT - Check player character is alive this frame");

	CWanted *pWanted = pPlayer->GetPlayerWanted();
	return (pWanted->m_WantedLevel >= WANTED_LEVEL1 && !pWanted->CopsAreSearching() && pWanted->m_iTimeFirstSpotted>0);
}

void CommandSetDispatchCopsForPlayer( int PlayerIndex, bool flag )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

//	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_DISPATCH_COPS_FOR_PLAYER - Check player character is alive this frame");

	flag = !flag;
	CWanted *pWanted = pPlayer->GetPlayerWanted();
	pWanted->m_DontDispatchCopsForThisPlayer = flag;
}

bool CommandIsWantedLevelGreater(int PlayerIndex, int iLevel)
{
	CPed *pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if (pPlayer)
	{
//		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_WANTED_LEVEL_GREATER - Check player character is alive this frame");
		if (pPlayer->GetPlayerWanted())
		{
			return (pPlayer->GetPlayerWanted()->GetWantedLevel() > iLevel );
		}
	}

	return false;
}

void CommandClearWantedLevel(int PlayerIndex)
{
	wantedDebugf3("CommandClearWantedLevel");

	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "CLEAR_PLAYER_WANTED_LEVEL - Check player character is alive this frame");

	pPlayer->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_FROM_SCRIPT);
	NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.CancelAnyPlayingReportsNextFade());

#if __BANK
	safecpy(pPlayer->GetPlayerWanted()->m_WantedLevelLastSetFromScriptName, CTheScripts::GetCurrentScriptName());
#endif
}

int CommandGetIndexOfPlayerThatCausedWL()
{
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	CWanted* wanted = pLocalPlayer->GetPlayerWanted();

	if (wanted)
	{
		int index = wanted->GetPhysicalIndexOfPlayerThatCausedCurrentWL();
		if (index == INVALID_PLAYER_INDEX)
		{
			return -1;
		}
		else
		{
			return index;
		}
	}
	return -1;
}

bool CommandLocalPlayerHasPendingCrimes()
{
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if (scriptVerifyf(pLocalPlayer, "LOCAL_PLAYER_HAS_PENDING_CRIMES - No local player found!"))
	{
		CWanted* pWanted = pLocalPlayer->GetPlayerWanted();
		if (scriptVerifyf(pWanted, "LOCAL_PLAYER_HAS_PENDING_CRIMES - No wanted info found for local player!"))
		{
			return pWanted->HasPendingCrimesQueued();
		}
	}
	
	return false;
}

bool CommandIsPlayerDead( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	return (pPlayer && pPlayer->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED);
}

bool CommandIsPlayerPressingHorn( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_PRESSING_HORN - Check player character is alive this frame");

	return pPlayer->GetPlayerInfo()->IsPlayerPressingHorn();
}

void CommandSetPlayerClothPinFrames(int iPedIndex, int pinFrames)
{
	CPed * pPed = CTheScripts::FindLocalPlayerPed(iPedIndex);
	if (pPed)
	{
		pPed->SetClothForcePin( (u8)pinFrames );
	}
}

void CommandSetPlayerClothPackageIndex(int packageIndex)
{
	CPed * pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		pPed->SetClothPackageIndex( (u8)packageIndex, 0 );
	}
}

void CommandSetPlayerClothLockCounter(int lockCounter)
{
	Assert( lockCounter >= 0 && lockCounter <= 255 );

	CPed * pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{		
		pPed->SetClothLockCounter( (u8)lockCounter );
	}
}

void CommandPlayerDetachVirtualBound()
{
	CPed * pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		for( int i = 0; i < PV_MAX_COMP; ++i )
		{
			if( pPed->m_CClothController[i] )
			{
				phVerletCloth* verletCloth = pPed->m_CClothController[i]->GetCloth(0);
				Assert( verletCloth );
				
				verletCloth->DetachVirtualBound();

				verletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, false);
				verletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, false);
			}
		}
	}
}

Mat34V identMat(V_IDENTITY);
void CommandPlayerAttachVirtualBound( const scrVector & pos, const scrVector & rot, float fCapsuleLen, float fCapsuleRad)
{
	CPed * pPed = CGameWorld::FindLocalPlayer();
	if( pPed )
	{
		for( int i = 0; i < PV_MAX_COMP; ++i )
		{
			if( pPed->m_CClothController[i] )
			{
				phVerletCloth* verletCloth = pPed->m_CClothController[i]->GetCloth(0);
				Assert( verletCloth );

				Mat34V boundMat;
				Mat34VFromEulersXYZ( boundMat, VECTOR3_TO_VEC3V(Vector3(rot)) );
				boundMat.SetCol3( VECTOR3_TO_VEC3V(Vector3(pos)) );

				verletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
				verletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, true);

				Assert( !verletCloth->m_VirtualBound );
				verletCloth->CreateVirtualBound( 1, &identMat );
				verletCloth->AttachVirtualBoundCapsule( fCapsuleRad, fCapsuleLen, boundMat, 0 );
			}
		}
	}
}

void CommandSetPlayerControl(int iPlayerIndex, bool bSetControlOn, int iFlags)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (!pPlayer)
		return;

	if(NetworkInterface::IsGameInProgress())
	{
		iFlags &= ~SPC_AmbientScript;
		iFlags &= ~SPC_RemoveProjectiles;
	}

	if (bSetControlOn)
	{	
		// Raise a flag to lock the limits of the camera when in fps mode
		pPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasControlOfPlayer, false);

		iFlags &= ~SPC_DisableControls;
		pPlayer->GetPlayerInfo()->SetPlayerControl(iFlags, 10.0f);
		const bool bReEnableControlOnDeath = ((iFlags & SPC_ReEnableControlOnDeath)!=0);
		pPlayer->GetPlayerInfo()->bEnableControlOnDeath = bReEnableControlOnDeath;
	}
	else
	{	
		// Get rid of the flag that limits the camera
		pPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasControlOfPlayer, true);

		iFlags |= SPC_DisableControls;
		pPlayer->GetPlayerInfo()->SetPlayerControl(iFlags, 10.0f);
		pPlayer->GetPlayerInfo()->bEnableControlOnDeath = true;

		if (!NetworkInterface::IsInCopsAndCrooks())
		{
			if (pPlayer->GetSpecialAbility())
				pPlayer->GetSpecialAbility()->Deactivate();
		}
	}

#if __BANK
	if (NetworkInterface::IsGameInProgress())
	{
		scriptDebugf1("%s : SET_PLAYER_CONTROL (%s) - flags=\"%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\"\n"
							,CTheScripts::GetCurrentScriptNameAndProgramCounter()
							,bSetControlOn ? "TRUE" : "FALSE"
							,((iFlags & SPC_AmbientScript)                 !=0) ? "AmbientScript" : ""
							,((iFlags & SPC_ClearTasks)                    !=0) ? "ClearTasks" : ""
							,((iFlags & SPC_RemoveFires)                   !=0) ? "RemoveFires" : ""
							,((iFlags & SPC_RemoveExplosions)              !=0) ? "RemoveExplosions" : ""
							,((iFlags & SPC_RemoveProjectiles)             !=0) ? "RemoveProjectiles" : ""
							,((iFlags & SPC_DeactivateGadgets)             !=0) ? "DeactivateGadgets" : ""
							,((iFlags & SPC_ReEnableControlOnDeath)        !=0) ? "ReEnableControlOnDeath" : ""
							,((iFlags & SPC_LeaveCameraControlsOn)         !=0) ? "LeaveCameraControlsOn" : ""
							,((iFlags & SPC_AllowPlayerToBeDamaged)        !=0) ? "AllowPlayerToBeDamaged" : ""
							,((iFlags & SPC_DontStopOtherCarsAroundPlayer) !=0) ? "DontStopOtherCarsAroundPlayer" : ""
							,((iFlags & SPC_AllowPadShake)				   !=0) ? "AllowPadShake" : "");
	}
	else
	{
		scriptDebugf1("SET_PLAYER_CONTROL (%s) called by %s", bSetControlOn ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif // __BANK
}

int CommandGetPlayerWantedLevel( int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerWanted()->GetWantedLevel() : 0);
}


void CommandSetMaxWantedLevel( int NewMaxLevel )
{
	scriptDebugf1("SET_MAX_WANTED_LEVEL - NewMaxLevel %d", NewMaxLevel);
	NewMaxLevel = Min(NewMaxLevel, WANTED_LEVEL_LAST - 1);
	CWanted::SetMaximumWantedLevel(NewMaxLevel);
}

void CommandSetPoliceRadarBlips( bool bBlips )
{
	CWanted::GetWantedBlips().EnablePoliceRadarBlips(bBlips);
}

void CommandSetPoliceIgnorePlayer( int PlayerIndex, bool bIgnorePlayer )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	if (bIgnorePlayer)
	{
		pPlayer->GetPlayerWanted()->m_PoliceBackOff = TRUE;
		CGameWorld::StopAllLawEnforcersInTheirTracks();	
	}
	else
	{
		pPlayer->GetPlayerWanted()->m_PoliceBackOff = FALSE;
	}
}

bool CommandIsPlayerPlaying( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if (pPlayer && CTheScripts::IsPlayerPlaying(pPlayer))
	{
#if __DEV
		pPlayer->m_nDEflags.bCheckedForDead = TRUE;
#endif
		return true;
	}

	return false;
}

void CommandSetEveryoneIgnorePlayer( int PlayerIndex, bool bIgnore )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_EVERYONE_IGNORE_PLAYER - Player character doesn't have a wanted class"))
	{	
		if (bIgnore)
		{
			pPlayer->GetPlayerWanted()->m_EverybodyBackOff = TRUE;
		
			if(!NetworkInterface::IsGameInProgress())
			{
				CGameWorld::StopAllLawEnforcersInTheirTracks();	
			}
		}
		else
		{
			pPlayer->GetPlayerWanted()->m_EverybodyBackOff = FALSE;
		}	
	}
}

//  DEPRECATED
void CommandSetAllRandomPedsFlee( int PlayerIndex, bool bFlee )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_ALL_RANDOM_PEDS_FLEE - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			if (bFlee)
			{
				pPlayer->GetPlayerWanted()->m_AllRandomsFlee = TRUE;
			}
			else
			{
				pPlayer->GetPlayerWanted()->m_AllRandomsFlee = FALSE;
			}	
		}
	}
}

void CommandSetAllRandomPedsFleeThisFrame( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
	{
		return;
	}

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_ALL_RANDOM_PEDS_FLEE_THIS_FRAME - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			pPlayer->GetPlayerWanted()->m_AllRandomsFlee = true;
		}
	}
}

//  DEPRECATED
void CommandSetAllNeutralRandomPedsFlee( int PlayerIndex, bool bFlee )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_ALL_NEUTRAL_RANDOM_PEDS_FLEE - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			pPlayer->GetPlayerWanted()->m_AllNeutralRandomsFlee = bFlee;
		}
	}
}


void CommandSetAllNeutralRandomPedsFleeThisFrame( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
	{
		return;
	}

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_ALL_NEUTRAL_RANDOM_PEDS_FLEE_THIS_FRAME - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			pPlayer->GetPlayerWanted()->m_AllNeutralRandomsFlee = true;
		}
	}
}


//  DEPRECATED
void CommandSetRandomPedsOnlyAttackWithGuns( int PlayerIndex, bool bVal )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_RANDOM_PEDS_ONLY_ATTACK_WITH_GUNS - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			pPlayer->GetPlayerWanted()->m_AllRandomsOnlyAttackWithGuns = bVal;
		}
	}
}
void CommandSetRandomPedsOnlyAttackWithGunsThisFrame( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
	{
		return;
	}

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_RANDOM_PEDS_ONLY_ATTACK_WITH_GUNS_THIS_FRAME - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			pPlayer->GetPlayerWanted()->m_AllRandomsOnlyAttackWithGuns = true;
		}
	}
}

void CommandSetLawPedsCanAttackNonWantedPlayerThisFrame(int PlayerIndex)
{
	CPed *pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_LAW_PEDS_CAN_ATTACK_NON_WANTED_PLAYER_THIS_FRAME - Player character doesn't have a wanted class"))
		{
			pPlayer->GetPlayerWanted()->m_LawPedCanAttackNonWantedPlayer = true;
		}
	}
}

void CommandSetIgnoreLowPriorityShockingEvents( int PlayerIndex, bool bFlee )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(!NetworkInterface::IsGameInProgress(), "SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS - Currently not supported in network game");

	if(SCRIPT_VERIFY(pPlayer->GetPlayerWanted(), "SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS - Player character doesn't have a wanted class"))
	{
		if (pPlayer->GetPlayerWanted())
		{
			if (bFlee)
			{
				pPlayer->GetPlayerWanted()->m_IgnoreLowPriorityShockingEvents = TRUE;
			}
			else
			{
				pPlayer->GetPlayerWanted()->m_IgnoreLowPriorityShockingEvents = FALSE;
			}	
		}
	}
}


void CommandSetWantedMultiplier( float fMultiplier )
{
#if __DEV
	scriptDebugf1("%s script is setting wanted multiplier to %f ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fMultiplier);
#endif
	if( scriptVerifyf( CGameWorld::FindLocalPlayerWanted(), "SET_WANTED_MULTIPLIER failed: Local player doesn't exist!" ) )
	{
		CGameWorld::FindLocalPlayerWanted()->m_fMultiplier = fMultiplier;
	}
}

void CommandSetWantedLevelDifficulty(int iPlayerIndex, float fDifficulty)
{
	//Ensure the player is valid.
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);
	if(!SCRIPT_VERIFY(pPlayer, "SET_WANTED_LEVEL_DIFFICULTY - The player is invalid."))
	{
		return;
	}
	
	//Ensure the wanted is valid.
	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if(!SCRIPT_VERIFY(pWanted, "SET_WANTED_LEVEL_DIFFICULTY - The wanted is invalid."))
	{
		return;
	}
	
	//Set the difficulty.
	SCRIPT_CHECK_CALLING_FUNCTION
	pWanted->m_Overrides.SetDifficulty(fDifficulty);
}

void CommandResetWantedLevelDifficulty(int iPlayerIndex)
{
	//Ensure the player is valid.
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);
	if(!SCRIPT_VERIFY(pPlayer, "RESET_WANTED_LEVEL_DIFFICULTY - The player is invalid."))
	{
		return;
	}
	
	//Ensure the wanted is valid.
	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if(!SCRIPT_VERIFY(pWanted, "RESET_WANTED_LEVEL_DIFFICULTY - The wanted is invalid."))
	{
		return;
	}
	
	//Reset the difficulty.
	pWanted->m_Overrides.ResetDifficulty();
}

int CommandGetWantedLevelTimeToEscape()
{
	return CDispatchData::GetParoleDuration();
}

void CommandSetWantedLevelHiddenEscapeTime(int iPlayerIndex, int iWantedLevel, int iTimeToEscape)
{
	//Ensure the player is valid.
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);
	if (!SCRIPT_VERIFY(pPlayer, "SET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME - The player is invalid."))
	{
		return;
	}

	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if (!SCRIPT_VERIFY(pWanted, "SET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME - The wanted is invalid."))
	{
		return;
	}

#if __BANK
	scriptDebugf3("Wanted level %i, hidden evasion time set to %i", iWantedLevel, iTimeToEscape);
#endif

	pWanted->m_Overrides.SetMultiplayerHiddenEvasionTimesOverride((s32)iWantedLevel, (u32)iTimeToEscape);
}

void CommandResetWantedLevelHiddenEscapeTime(int iPlayerIndex)
{
	//Ensure the player is valid.
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);
	if (!SCRIPT_VERIFY(pPlayer, "RESET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME - The player is invalid."))
	{
		return;
	}

	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if (!SCRIPT_VERIFY(pWanted, "RESET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME - The wanted is invalid."))
	{
		return;
	}

#if __BANK
	scriptDebugf3("Wanted level hidden evasion times reset");
#endif

	pWanted->m_Overrides.ResetEvasionTimeArray();
}

void CommandSetArcadeCNCVOffender(bool bVOffender)
{
	SCRIPT_ASSERT(!NetworkInterface::IsInCopsAndCrooks(), "SET_ARCADE_CNC_V_OFFENDER - Called outside of Cops & Crooks");
	CPed* pPlayer = CGameWorld::FindLocalPlayerSafe();
	SCRIPT_ASSERT(pPlayer, "SET_ARCADE_CNC_V_OFFENDER - The local player is invalid.");
	SCRIPT_ASSERT(pPlayer->GetPlayerInfo(), "SET_ARCADE_CNC_V_OFFENDER - The local player info is invalid");
	
	CPlayerArcadeInformation& playerArcadeInfo = pPlayer->GetPlayerInfo()->AccessArcadeInformation();
	scriptDebugf3("SET_ARCADE_CNC_V_OFFENDER - setting from %s to %s", playerArcadeInfo.GetCNCVOffender() ? "TRUE" : "FALSE", bVOffender ? "TRUE" : "FALSE");
	playerArcadeInfo.SetCNCVOffender(bVOffender);
}

bool CommandGetArcadeCNCVOffender(int PlayerIndex)
{
	SCRIPT_ASSERT(!NetworkInterface::IsInCopsAndCrooks(), "SET_ARCADE_CNC_V_OFFENDER - Called outside of Cops & Crooks");
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);
	SCRIPT_ASSERT(pPlayer, "SET_ARCADE_CNC_V_OFFENDER - The player index is invalid.");
	SCRIPT_ASSERT(pPlayer->GetPlayerInfo(), "SET_ARCADE_CNC_V_OFFENDER - The player info is invalid");

	return pPlayer->GetPlayerInfo()->AccessArcadeInformation().GetCNCVOffender();
}

void CommandStartFiringAmnesty( int iDuration )
{
	SCRIPT_ASSERT(!NetworkInterface::IsGameInProgress(), "START_FIRING_AMNESTY - Currently not supported in network game");
	SCRIPT_ASSERT(iDuration >= 0, "START_FIRING_AMNESTY - duration needs to be greater than or equal to zero");

	CGameWorld::FindLocalPlayerWanted()->SetTimeAmnestyEnds(fwTimer::GetTimeInMilliseconds() + iDuration);
	CGameWorld::FindLocalPlayerWanted()->ClearQdCrimes();
	CGameWorld::FindLocalPlayerWanted()->m_TimeWhenNewWantedLevelTakesEffect = 0;
	CGameWorld::FindLocalPlayerWanted()->m_nNewWantedLevel = CGameWorld::FindLocalPlayerWanted()->m_nWantedLevel;
}

void CommandReportCrime( int PlayerIndex, int crimeToReport, int crimeValue )
{
	scriptDebugf3("%s [Frame=%d] : REPORT_CRIME: Crime Type: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), crimeToReport);

	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if (!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	switch (CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		scriptDebugf3("%s [Frame=%d] : REPORT_CRIME (Default)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount());
		pPlayer->GetPlayerWanted()->ReportCrimeNow((eCrimeType)crimeToReport, VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), false, true, crimeValue);
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC:
		// do not care about victim for CNC so this is null
		scriptDebugf3("%s [Frame=%d] : REPORT_CRIME (Arcade Mode: CNC)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount());
		CCrime::ReportCrime((eCrimeType)crimeToReport, nullptr, pPlayer);
		break;
	default:
		break;
	}

}

void CommandSuppressCrimeThisFrame( int PlayerIndex, int CrimeToSuppress )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if (!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	pPlayer->GetPlayerWanted()->SuppressCrimeThisFrame((eCrimeType)CrimeToSuppress);
}

void CommandUpdateWantedPositionThisFrame( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	pPlayer->GetPlayerWanted()->m_CenterWantedPosition = true;
}

void CommandSuppressLosingWantedLevelIfHiddenThisFrame( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

#if __BANK
	wantedDebugf3("SUPPRESS_LOSING_WANTED_LEVEL_IF_HIDDEN_THIS_FRAME - setting m_SuppressLosingWantedLevelIfHiddenThisFrame to True for %s", AILogging::GetDynamicEntityNameSafe(pPlayer));
#endif

	pPlayer->GetPlayerWanted()->m_SuppressLosingWantedLevelIfHiddenThisFrame = true;
}

void CommandmAllowEvasionHUDIfDisablingHiddenEvasionThisFrame( int PlayerIndex, float fTimeBeforeAllowReport )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	pPlayer->GetPlayerWanted()->m_AllowEvasionHUDIfDisablingHiddenEvasionThisFrame = true;

	// B*2343618: Allow script to prevent player from being reported for a certain amount of time when first calling this function. 
	// SPARR changed this so it now prevents newly spawned cops from reporting playing within this time
	s32 iTimeInMilli = (s32)(fTimeBeforeAllowReport*1000.0f);
	pPlayer->GetPlayerWanted()->m_iTimeBeforeAllowReporting = iTimeInMilli;
}

void CommandForceStartHiddenEvasion( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	pPlayer->GetPlayerWanted()->ForceStartHiddenEvasion();
}

void CommandSuppressWitnessesCallingPoliceThisFrame( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer || !pPlayer->GetPlayerWanted())
	{
		return;
	}

	pPlayer->GetPlayerWanted()->m_DisableCallPoliceThisFrame = true;
}

void CommandEnableCnCCrimeReporting(bool bEnable)
{
	if (bEnable)
	{
		CCrime::sm_eReportCrimeMethod =  CCrime::REPORT_CRIME_ARCADE_CNC;
		CWanted::sm_bEnableHiddenEvasion = false;
	}
	else if (CCrime::sm_eReportCrimeMethod ==  CCrime::REPORT_CRIME_ARCADE_CNC)
	{
		CCrime::sm_eReportCrimeMethod =  CCrime::REPORT_CRIME_DEFAULT;
		CWanted::sm_bEnableHiddenEvasion = true;
	}
}

void CommandSetLawResponseDelayOverride( float fLawResponseDelay )
{
	CDispatchManager::SetLawResponseDelayOverride(fLawResponseDelay);
}

void CommandResetLawResponseDelayOverride()
{
	CDispatchManager::SetLawResponseDelayOverride(-1.0f);
}

void CommandReportPoliceSpottedPlayer( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer)
	{
		return;
	}

	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if(pWanted)
	{
		wantedDebugf3("CommandReportPoliceSpottedPlayer ReportPoliceSpottingPlayer"); 
		Vec3V vPlayerPosition = pPlayer->GetTransform().GetPosition();
		pWanted->ReportPoliceSpottingPlayer(NULL, VEC3V_TO_VECTOR3(vPlayerPosition), pWanted->m_WantedLevel, true, true);
	}
}

void CommandDisableWantedRelationshipGroupReset( int PlayerIndex, int relationshipGroupHash )
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);

	if(!pPlayer)
	{
		return;
	}

	CWanted* pWanted = pPlayer->GetPlayerWanted();
	if(pWanted)
	{
		pWanted->DisableRelationshipGroupResetThisFrame(relationshipGroupHash);
	}
}

bool CommandIsPlayerSpectatedVehicleRadioOverride()
{
	return NetworkInterface::IsOverrideSpectatedVehicleRadioSet();
}

void CommandSetPlayerSpectatedVehicleRadioOverride( bool bValue )
{
	if (NetworkInterface::GetPlayerMgr().IsInitialized())
	{
		NetworkInterface::SetOverrideSpectatedVehicleRadio( bValue );
	}
	else
	{
		SCRIPT_ASSERT(0, "SET_PLAYER_SPECTATED_VEHICLE_RADIO_OVERRIDE called when MP game is no longer is progress - please check MP state before calling this method");
	}
}

bool CommandCanPlayerStartMission( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "CAN_PLAYER_START_MISSION - Check player character is alive this frame");

	return (pPlayer->GetPlayerInfo()->CanPlayerStartMission());
}

bool CommandCanPlayerDoStealthKill( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "CAN_PLAYER_DO_STEALTH_KILL - Check player character is alive this frame");

	// We can only surrender if someone is trying to capture us and we are not already surrendering
	CTask* pTask = pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
	if (pTask && static_cast<CTaskPlayerOnFoot*>(pTask)->GetCanStealthKill())
	{
		return true;
	}

	return false;
}

bool CommandIsPlayerReadyForCutscene(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_READY_FOR_CUTSCENE - Check player character is alive this frame");
	
	// Never safe whilst ragdolling
	if( pPlayer->GetUsingRagdoll() )
	{
		return false;
	}
	//temporary fix: The mod shop script is using this command to check that the mod shop can run 
	//but this will fail if inside a garage. Multi-player has added mod shop locations as garages, so they can leverage locate commands 
	//IS_OBJECT_ENTIRELY_INSIDE_GARAGE. Removing the garage check from this command, not relevant anymore. 
	//if (CGarages::PlayerIsInteractingWithGarage())
	//{
	//	return false;
	//}

	if (CStuntJumpManager::IsAStuntjumpInProgress())
	{
		return false;
	}

	CQueriableInterface* pQueriableInterface = pPlayer->GetPedIntelligence()->GetQueriableInterface();
	if (pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER) || pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER))
	{
		return false;
	}

	// Safe in a vehicle as long as the player isn't entering or exiting
	if( pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		if ( (!pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE)) && 
			(!pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) ) )
		{
			return true;
		}
	}
	// Always safe on foot as long as the player isn't ragdolling (checked above) or getting in a vehicle
	else
	{
		return !pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
			!pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE);
	}
	return false;
}

void CommandMakePlayerSafeForCutscene( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

#if __DEV
	scriptDebugf1("MAKE_PLAYER_SAFE_FOR_CUTSCENE - SET_PLAYER_CONTROL (TRUE) called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "MAKE_PLAYER_SAFE_FOR_CUTSCENE - Check player character is alive this frame");

	pPlayer->GetPlayerInfo()->DisableControlsCutscenes();
	pPlayer->GetPlayerInfo()->SetPlayerControl(false, TRUE);
}


bool CommandIsPlayerTargettingEntity( int PlayerIndex, int iTargetEntity )
{
	bool bResult = FALSE;

	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(iTargetEntity);

	if (pPlayer && pEntity)
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_TARGETTING_ENTITY - Check player character is alive this frame");

		if (pPlayer->IsLocalPlayer())
		{
			if (pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget() == pEntity)
			{
				bResult = true;
			}
		}
		else
		{
			CNetObjPlayer* pPlayerObj = static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject());

			if (pPlayerObj && pPlayerObj->GetAimingTarget() == pEntity)
			{
				bResult = true;	
			}
		}
	}

	return(bResult);
}

bool CommandGetPlayerTargetEntity( int PlayerIndex, int& iTargetEntity )
{
	bool bResult = false;

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == true, "GET_PLAYER_TARGET_ENTITY - Check player character is alive this frame");

		CEntity *pEntity = pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
		if (pEntity && pEntity->GetIsPhysical())
		{
			bResult = true;
			iTargetEntity = CTheScripts::GetGUIDFromEntity(*(static_cast<CPhysical*>(pEntity)));
		}
	}

	return bResult;
}

bool CommandIsPlayerFreeAiming( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_FREE_AIMING - Check player character is alive this frame");

	return pPlayer->GetPlayerInfo()->GetPlayerDataFreeAiming() != 0;		
}


bool CommandIsPlayerFreeAimingAtEntity( int PlayerIndex, int iTargetEntity )
{
	//--

	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	Assert(pPlayer);

	if (!pPlayer)
		return false;		

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_FREE_AIMING_AT_ENTITY - Check player character is alive this frame");

	//--

	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(iTargetEntity);

	if(!pEntity)
		return false;

	if(!NetworkInterface::IsGameInProgress() || !pPlayer->IsNetworkClone())
	{
		const CPlayerPedTargeting & rPlayerTargeting = pPlayer->GetPlayerInfo()->GetTargeting();
		return (rPlayerTargeting.GetFreeAimTarget() == pEntity) || (rPlayerTargeting.GetFreeAimTargetRagdoll() == pEntity);
	}
	else
	{
		CNetObjPlayer* netObjPlayer = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());
		return ( netObjPlayer->IsFreeAimingLockedOnTarget() && pEntity == netObjPlayer->GetAimingTarget() );
	}

	//--
}

bool CommandGetEntityPlayerIsFreeAimingAt( int PlayerIndex, int& iTargetEntity )
{
	bool bResult = false;

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "GET_ENTITY_PLAYER_IS_FREE_AIMING_AT - Check player character is alive this frame");
		CPlayerPedTargeting & rPlayerTargeting = pPlayer->GetPlayerInfo()->GetTargeting();

		//favor non-ragdoll targets
		if (rPlayerTargeting.GetFreeAimTarget() && rPlayerTargeting.GetFreeAimTarget()->GetIsPhysical())
		{
			iTargetEntity = CTheScripts::GetGUIDFromEntity(*(static_cast<CPhysical*>(rPlayerTargeting.GetFreeAimTarget())));
			bResult = true;
		}
		else if (rPlayerTargeting.GetFreeAimTargetRagdoll() && rPlayerTargeting.GetFreeAimTargetRagdoll()->GetIsPhysical())
		{
			iTargetEntity = CTheScripts::GetGUIDFromEntity(*(static_cast<CPhysical*>(rPlayerTargeting.GetFreeAimTargetRagdoll())));
			bResult = true;
		}
	}

	return bResult;		
}

void CommandSetPlayerLockOnRangeOverride( int PlayerIndex, float fRange )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	pPlayer->GetPlayerInfo()->GetTargeting().SetLockOnRangeOverride(fRange);
}


void CommandSetPlayerCanDoDriveBy( int PlayerIndex, bool bCanDoDriveBy )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	pPlayer->GetPlayerInfo()->bCanDoDriveBy = bCanDoDriveBy;
}

void CommandSetPlayerCanBeHassledByGangs(int PlayerIndex, bool bCanBeHassled)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	pPlayer->GetPlayerInfo()->bCanBeHassledByGangs = bCanBeHassled;
}

void CommandSetPlayerCanUseCover( int PlayerIndex, bool bCanUseCover )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_CAN_USE_COVER - Check player character is alive this frame");

	pPlayer->GetPlayerInfo()->bCanUseCover = bCanUseCover;
}

int CommandGetMaxWantedLevel()
{
	return CWanted::MaximumWantedLevel;
}

bool CommandIsPlayerTargettingAnything( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_TARGETTING_ANYTHING - Check player character is alive this frame");
	Assert(pPlayer->GetPlayerInfo());

	return (pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget() != NULL);
}

void CommandSetPlayerSprint( int PlayerIndex, bool bEnableSprint )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_SPRINT - Check player character is alive this frame");

	pPlayer->GetPlayerInfo()->SetDisablePlayerSprint(!bEnableSprint);
}


void CommandPlayerResetSprintStamina( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "RESET_PLAYER_STAMINA - Check player character is alive this frame");

	pPlayer->GetPlayerInfo()->ResetSprintEnergy();
}

void CommandPlayerRestoreSprintStamina( int PlayerIndex, float fPercent )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "RESTORE_PLAYER_STAMINA - Check player character is alive this frame");

	if ( SCRIPT_VERIFY(fPercent >= 0.f,   "RESTORE_PLAYER_STAMINA - fPercent must be >= 0") &&
		   SCRIPT_VERIFY(fPercent <= 100.f, "RESTORE_PLAYER_STAMINA - fPercent must be <= 100") )
	{
		pPlayer->GetPlayerInfo()->RestoreSprintEnergy(fPercent);
	}
}

float CommandGetPlayerSprintStaminaRemaining( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (SCRIPT_VERIFY(pPlayer != NULL, "GET_PLAYER_SPRINT_STAMINA_REMAINING - Player couldn't be found"))
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "GET_PLAYER_SPRINT_STAMINA_REMAINING - Check player character is alive this frame");
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if( SCRIPT_VERIFY(pPlayerInfo != NULL, "GET_PLAYER_SPRINT_STAMINA_REMAINING - Player has no PlayerInfo") )
		{
			return pPlayerInfo->GetPlayerDataMaxSprintEnergy() - pPlayerInfo->GetPlayerDataSprintEnergy();
		}
	}
	return 0.0f;
}

float CommandGetPlayerSprintTimeRemaining( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (SCRIPT_VERIFY(pPlayer != NULL, "GET_PLAYER_SPRINT_TIME_REMAINING - Player couldn't be found"))
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "GET_PLAYER_SPRINT_TIME_REMAINING - Check player character is alive this frame");
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if( SCRIPT_VERIFY(pPlayerInfo != NULL, "GET_PLAYER_SPRINT_TIME_REMAINING - Player has no PlayerInfo") )
		{
			float maxSprint = pPlayerInfo->GetPlayerDataMaxSprintEnergy();
			float sprint = pPlayerInfo->GetPlayerDataSprintEnergy();
			float depletion = pPlayerInfo->ComputeSprintDepletionRateForPed(*pPlayer, maxSprint, CPlayerInfo::SPRINT_ON_FOOT);
			return (sprint / depletion);
		}
	}
	return 0.0f;
}

float CommandGetPlayerUnderwaterTimeRemaining( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (SCRIPT_VERIFY(pPlayer != NULL, "GET_PLAYER_UNDERWATER_TIME_REMAINING - Player couldn't be found"))
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "GET_PLAYER_UNDERWATER_TIME_REMAINING - Check player character is alive this frame");

		const CInWaterEventScanner& waterScanner = pPlayer->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
		float fTimeUnderWater = waterScanner.GetTimeSubmerged();
		float fMaxTimeUnderWater = pPlayer->GetMaxTimeUnderwater();

		return fMaxTimeUnderWater - fTimeUnderWater;
	}
	return 0.0f;
}

void CommandPlayerSetUnderwaterBreathingPercentRemaining( int PlayerIndex, float fPercent )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_UNDERWATER_BREATH_PERCENT_REMAINING - Check player character is alive this frame");

	if ( SCRIPT_VERIFY(fPercent >= 0.f,   "SET_PLAYER_UNDERWATER_BREATH_PERCENT_REMAINING - fPercent must be >= 0") &&
		SCRIPT_VERIFY(fPercent <= 100.f, "SET_PLAYER_UNDERWATER_BREATH_PERCENT_REMAINING - fPercent must be <= 100") )
	{
		CInWaterEventScanner& waterScanner = pPlayer->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
		float fMaxTimeUnderwater = pPlayer->GetMaxTimeUnderwater();
		float fTimeSubmerged = fMaxTimeUnderwater - (fPercent / fMaxTimeUnderwater);
		waterScanner.SetTimeSubmerged(fTimeSubmerged);
	}
}

int CommandGetPlayerGroup( int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	s32 playerGroup = 0;

	if (pPlayer)
		playerGroup = pPlayer->GetPlayerInfo()->GetPlayerDataPlayerGroup();

	// we reference ped groups via an id assigned to a group when it is created by the script. Player groups are a special case in that they already
	// exist. Therefore we still need to register them with the resource management system so we have an id we can use. The player's group will not
	// be cleaned up because it is permanent.
	scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PEDGROUP, playerGroup);

	if (!pResource)
	{
		CScriptResource_PedGroup pedGroup((u32)playerGroup);

		return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(pedGroup);
	}
	else
	{
		return pResource->GetId();
	}
}

int CommandGetPlayerMaxArmour( int PlayerIndex )
{
	CPed* player = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	return (player && player->GetPlayerInfo()) ? player->GetPlayerInfo()->MaxArmour : 0;
}

int CommandGetPlayerJackSpeed( int PlayerIndex )
{
	CPed* player = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	return (player && player->GetPlayerInfo()) ? player->GetPlayerInfo()->JackSpeed : 0;
}

bool CommandIsPlayerControlOn( int PlayerIndex  )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	if (NetworkInterface::IsGameInProgress())
	{	
		//	In multiplayer games, the scripts continue to run while the pause menu is up so ignore the fact that control has been turned off by the pause menu
		return (!pPlayer->GetPlayerInfo()->AreAnyControlsOtherThanFrontendDisabled());
	}
	else
	{
		return (!pPlayer->GetPlayerInfo()->AreControlsDisabled());
	}
}

bool CommandGetAreCameraControlsDisabled()
{
	// get our local player
	CPed* pPlayer = CGameWorld::FindLocalPlayerSafe();

	// if we found our player
	if (pPlayer)
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_SCRIPT_CONTROL_ON - Check player character is alive this frame");

		// get our local player info
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();

		// if found 
		if (pPlayerInfo)
		{
			// return if the controls are disabled
			return pPlayerInfo->IsControlsCameraDisabled();
		}
	}

	// if no player or player info, return true (disabled)
	return true;
}

bool CommandIsPlayerScriptControlOn( int PlayerIndex  )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "IS_PLAYER_SCRIPT_CONTROL_ON - Check player character is alive this frame");

	if (pPlayer->GetPlayerInfo()->IsControlsScriptDisabled() || pPlayer->GetPlayerInfo()->IsControlsScriptAmbientDisabled())
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CommandIsPlayerClimbing( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	return pPlayer->GetPedIntelligence()->IsPedClimbing();
}

bool CommandIsPlayerSurrendering( int UNUSED_PARAM(PlayerIndex) )
{
	scriptAssertf(0, "IS_PLAYER_SURRENDERING is deprecated");
	return false;
}

bool CommandIsPlayerBeingArrested( int PlayerIndex, bool bCheckBustedTask )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	// Look for the surrender task
	if(bCheckBustedTask)
	{
		CTask* pTask = pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BUSTED);
		if (pTask)
		{
			return true;
		}
	}

	// Backup check to see if player is arrested
	if (pPlayer->GetIsArrested())
	{
		return true;
	}

	return false;
}

void CommandSetPlayerArrestedState( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->SetArrestState(ArrestState_Arrested);
	}
}

void CommandResetPlayerArrestState( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->SetArrestState(ArrestState_None);
	}
}

bool CommandCanPlayerPerformArrest( int PlayerIndex )
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		CTaskPlayerOnFoot* taskPlayerOnFoot = (CTaskPlayerOnFoot*)pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
		if (taskPlayerOnFoot)
		{
			return taskPlayerOnFoot->CheckShouldPerformArrest(true);
		}
	}

	return false;
}

// Should be kept in sync with INCAPACITATION_STATE in command_players.sch
enum ePlayerIncapacitationState
{
	State_Invalid = -1,
	State_Falling = 0,
	State_Incapacitated,
	State_Escaping,
	State_Arrested
};

ePlayerIncapacitationState CommandGetPlayerIncapacitationState(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed * pPed = FindPlayerPedByIndex(PlayerIndex);
	if (pPed)
	{
		if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_INCAPACITATED))
		{
			int iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_INCAPACITATED);
			switch (iState)
			{
			case CTaskIncapacitated::State_Start:
			case CTaskIncapacitated::State_DownedRagdoll:
				return ePlayerIncapacitationState::State_Falling;
			case CTaskIncapacitated::State_Incapacitated:
				return ePlayerIncapacitationState::State_Incapacitated;
			case CTaskIncapacitated::State_Escape:
				return ePlayerIncapacitationState::State_Escaping;
			case CTaskIncapacitated::State_Arrested:
				return ePlayerIncapacitationState::State_Arrested;
			default:
				return ePlayerIncapacitationState::State_Invalid;
			}
		}
	}
#endif

	return ePlayerIncapacitationState::State_Invalid;
}

bool CommandIsPlayerArrestableFromIncapacitated(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed * pPlayerPed = FindPlayerPedByIndex(PlayerIndex);
	if (!pPlayerPed)
	{
		return false;
	}

	CTaskIncapacitated* pTask = (CTaskIncapacitated*)pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
	if (!pTask)
	{
		return false;
	}
	return pTask->IsArrestable();
#endif

	return false;
}

bool CommandCanLocalPlayerPerformArrestOnPlayerFromIncapacitated(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed* pLocalPed = FindPlayerPed();
	if (!pLocalPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest))
	{
		return false;
	}
	CPed * pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if (!pPlayerPed) 
	{
		return false;
	}

	CTaskIncapacitated* pTask = (CTaskIncapacitated*)pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
	if (!pTask) 
	{
		return false;
	}

	return pTask->IsArrestableByPed(pLocalPed);;
#endif

	return false;
}

bool CommandHasPlayerBeenArrestedFromIncapacitated(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed* pPlayer = FindPlayerPedByIndex(PlayerIndex);

	if (pPlayer)
	{
		CTaskIncapacitated* taskIncapacitated = (CTaskIncapacitated*)pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (taskIncapacitated)
		{
			return taskIncapacitated->GetIsArrested();
		}
	}
#endif
	return false;
}

bool CommandIsIncapacitated(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed* pPlayer = FindPlayerPedByIndex(PlayerIndex);

	if (pPlayer)
	{
		CTaskIncapacitated* taskIncapacitated = (CTaskIncapacitated*)pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (taskIncapacitated)
		{
			return taskIncapacitated->GetState() != CTaskIncapacitated::State_Arrested;
		}
	}
#endif

	return false;
}

int CommandGetPlayerArrester(int CNC_MODE_ENABLED_ONLY(PlayerIndex))
{
#if CNC_MODE_ENABLED
	CPed* pPlayer = FindPlayerPedByIndex(PlayerIndex);

	if (pPlayer)
	{
		CTaskIncapacitated* taskIncapacitated = (CTaskIncapacitated*)pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (taskIncapacitated)
		{
			CPed* pPedsArrester = taskIncapacitated->GetPedArrester();
			if (pPedsArrester)
			{
				CNetObjGame* pNetOject = pPedsArrester->GetNetworkObject();
				if (pNetOject)
				{
					CNetGamePlayer* pNetPlayer = pNetOject->GetPlayerOwner();
					if (pNetPlayer)
					{
						PhysicalPlayerIndex iPlayerIdx = pNetPlayer->GetPhysicalPlayerIndex();

						return iPlayerIdx;
					}
				}
			}
		}
	}
#endif
	return 0;
}

bool CommandIsTapToGetUpFasterFromIncapacitatedAvailable()
{
#if CNC_MODE_ENABLED
	CPed* pPed = FindPlayerPed();
	if (pPed)
	{
		CTaskIncapacitated* taskIncapacitated = (CTaskIncapacitated*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (taskIncapacitated)
		{
			if (taskIncapacitated->GetState() == CTaskIncapacitated::State_Incapacitated || taskIncapacitated->GetState() == CTaskIncapacitated::State_DownedRagdoll)
			{
				if (taskIncapacitated->GetFastGetupOffset() < CTaskIncapacitated::sm_Tunables.m_GetUpFasterLimit)
				{
					return true;
				}
			}
		}
	}
#endif
	return false;
}

int CommandGetLocalPlayerTimeTilIncapacitationRecoveryMs()
{
#if CNC_MODE_ENABLED
	CPed* pPed = FindPlayerPed();
	if (pPed)
	{
		CTaskIncapacitated* taskIncapacitated = (CTaskIncapacitated*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (taskIncapacitated) 
		{
			if (taskIncapacitated->GetState() == CTaskIncapacitated::State_Incapacitated
				|| taskIncapacitated->GetState() == CTaskIncapacitated::State_DownedRagdoll)
			{
				return taskIncapacitated->CalculateTimeTilRecovery();
			}
		}
	}
#endif
	return 0;
}


int CommandGetLocalPlayerMaxTimeTilIncapacitationRecoveryMs()
{
#if CNC_MODE_ENABLED
	return CTaskIncapacitated::sm_Tunables.m_IncapacitateTime;
#endif
	return 0;
}

bool CommandIsPlayerFreeForAmbientTask( int PlayerIndex )
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return false;

	CQueriableInterface * pQI = pPlayer->GetPedIntelligence()->GetQueriableInterface();
	if(SCRIPT_VERIFY(pQI, "IS_PLAYER_FREE_FOR_AMBIENT_TASK - Cannot find quirable interface"))
	{
		bool bIsOnLadder = pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER) || pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER);
		if(bIsOnLadder)
			return false;

		// If the query is about the local player and the local player is on a weapon pickup that could be picked up with 'LB' we don't want to trigger any script action.
#if 0 // JG
		if (pPlayer->IsLocalPlayer() && CPickups::PlayerOnWeaponPickup)
		{
			return false;
		}
#endif

#if FPS_MODE_SUPPORTED
		if(pPlayer->IsFirstPersonShooterModeEnabledForPlayer())
		{
			if(pPlayer->GetMotionData()->GetIsFPSIdle())
			{
				return true;
			}
		}
#endif // FPS_MODE_SUPPORTED

		return pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_IDLES, true );
	}
	return false;
}

int CommandPlayerId()
{
	if (NetworkInterface::IsGameInProgress())
	{
		PhysicalPlayerIndex playerIndex = NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex();

		scriptAssertf(playerIndex< MAX_NUM_PHYSICAL_PLAYERS,"Player index %d greater than max range MAX_NUM_PHYSICAL_PLAYERS %d",playerIndex,MAX_NUM_PHYSICAL_PLAYERS );

		return playerIndex;
	}

	return 0;
}

int CommandPlayerPedId()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	return pPlayerPed ? CTheScripts::GetGUIDFromEntity(*pPlayerPed) : 0;
}

int CommandGetPlayersLastVehicleNoSave()
{
	int ReturnVehicleIndex = 0;
	
	if (CVehiclePopulation::ms_pInterestingVehicle)
	{
		ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*CVehiclePopulation::ms_pInterestingVehicle);
	}
	else
	{
		ReturnVehicleIndex = NULL_IN_SCRIPTING_LANGUAGE;
	}

	return ReturnVehicleIndex;
}

s32 CommandGetPlayerIndex()
{
	if (NetworkInterface::IsGameInProgress())
	{
		return NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex();
	}
		
	return 0;
}

int CommandConvertIntToPlayerIndex(int Arg)
{
	return Arg;
}

int CommandConvertIntToParticipantIndex(int Arg)
{
	return Arg;
}

int CommandGetTimeSincePlayerHitVehicle(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerHitCar() : 0);
}

int CommandGetTimeSincePlayerHitPed(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerHitPed() : 0);
}

int CommandGetTimeSincePlayerHitBuilding(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerHitBuilding() : 0);
}

int CommandGetTimeSincePlayerHitObject(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerHitObject() : 0);
}

int CommandGetTimeSincePlayerDroveOnPavement(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerDroveOnPavement() : 0);
}

int CommandGetTimeSincePlayerRanLight(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerRanLight() : 0);
}

int CommandGetTimeSincePlayerDroveAgainstTraffic(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer ? pPlayer->GetPlayerInfo()->GetTimeSincePlayerDroveAgainstTraffic() : 0);
}

bool CommandHasForceCleanupOccurred(int ForceCleanupBitField)
{
	GtaThread *pCurrentThread = CTheScripts::GetCurrentGtaScriptThread();
	if(SCRIPT_VERIFY(pCurrentThread, "HAS_FORCE_CLEANUP_OCCURRED - failed to get current thread"))
	{
		pCurrentThread->SetForceCleanupReturnPoint(static_cast<u32>(ForceCleanupBitField));
	}
	return false;
}

void CommandForceCleanup(int ForceCleanupBitField)
{
#if __BANK
	CDebugTextureViewerInterface::Close(true);
#endif // __BANK

	u8 PreviousValueOfTriggered = CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered;
	GtaThread::ForceCleanupForAllMatchingThreads(static_cast<u32>(ForceCleanupBitField), GtaThread::MATCH_ALL, "", static_cast<scrThreadId>(-1));
	if ( (CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered > 0) && (PreviousValueOfTriggered == 0) )
	{	//	I'm not sure if this is really a problem but I'll Assert for now
		scriptAssertf(0, "%d %s has triggered its own ForceCleanup code by calling FORCE_CLEANUP", fwTimer::GetSystemFrameCount(), CTheScripts::GetCurrentScriptName());
	}
	else
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif
		scriptDebugf1("%s [Frame=%d] : FORCE_CLEANUP has been called with flags %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), ForceCleanupBitField);
	}
}

void CommandForceCleanupForAllThreadsWithThisName(const char *pName, int ForceCleanupBitField)
{
	u8 PreviousValueOfTriggered = CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered;
	GtaThread::ForceCleanupForAllMatchingThreads(static_cast<u32>(ForceCleanupBitField), GtaThread::MATCH_SCRIPT_NAME, pName, static_cast<scrThreadId>(-1));
	if ( (CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered > 0) && (PreviousValueOfTriggered == 0) )
	{	//	I'm not sure if this is really a problem but I'll Assert for now
		scriptAssertf(0, "%d %s has triggered its own ForceCleanup code by calling FORCE_CLEANUP_FOR_ALL_THREADS_WITH_THIS_NAME", fwTimer::GetSystemFrameCount(), CTheScripts::GetCurrentScriptName());
	}
	else
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif
		scriptDebugf1("%s [Frame=%d] : FORCE_CLEANUP_FOR_ALL_THREADS_WITH_THIS_NAME has been called with flags %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), ForceCleanupBitField);
	}
}

void CommandForceCleanupForThreadWithThisID(s32 threadId, int ForceCleanupBitField)
{
	u8 PreviousValueOfTriggered = CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered;
	GtaThread::ForceCleanupForAllMatchingThreads(static_cast<u32>(ForceCleanupBitField), GtaThread::MATCH_THREAD_ID, "", static_cast<scrThreadId>(threadId));
	if ( (CTheScripts::GetCurrentGtaScriptThread()->m_ForceCleanupTriggered > 0) && (PreviousValueOfTriggered == 0) )
	{	//	I'm not sure if this is really a problem but I'll Assert for now
		scriptAssertf(0, "%d %s has triggered its own ForceCleanup code by calling FORCE_CLEANUP_FOR_THREAD_WITH_THIS_ID", fwTimer::GetSystemFrameCount(), CTheScripts::GetCurrentScriptName());
	}
	else
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif
		scriptDebugf1("%s [Frame=%d] : FORCE_CLEANUP_FOR_THREAD_WITH_THIS_ID has been called with flags %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetSystemFrameCount(), ForceCleanupBitField);
	}
}

int CommandGetCauseOfMostRecentForceCleanup()
{
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "GET_CAUSE_OF_MOST_RECENT_FORCE_CLEANUP - failed to get pointer to current script thread"))
	{
		return CTheScripts::GetCurrentGtaScriptThread()->m_CauseOfMostRecentForceCleanup;
	}

	return 0;
}

void CommandSetPlayerMoodNormal(int UNUSED_PARAM(PlayerIndex))
{
	scriptAssertf(0, "%d CommandSetPlayerMoodNormal has been deprecated, called by %s", fwTimer::GetSystemFrameCount(), CTheScripts::GetCurrentScriptName());
}

void CommandSetPlayerNotAllowedEnterAnyVehicle(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	
	if (pPlayer)
	{	
		if (SCRIPT_VERIFY(!(pPlayer->GetPlayerInfo()->GetVehiclePlayerIsRestrictedToEnter()),
			"The player has been set to enter a specific vehicle, make sure this is unset before calling this command") )
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR);
		}
	}
}

void CommandSetPlayerMayOnlyEnterThisVehicle(int PlayerIndex, int iVehicleID)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		if (SCRIPT_VERIFY(!(pPlayer->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR)), 
			"The command PLAYER_MAY_NOT_ENTER_ANY_VEHICLE has been called this frame, you cant do both")) 
		{
		
			if(iVehicleID==0)
		{
			pPlayer->GetPlayerInfo()->SetMayOnlyEnterThisVehicle(NULL);
		}
		else
		{
			const CVehicle * pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleID);

			if (pVehicle)
			{
				pPlayer->GetPlayerInfo()->SetMayOnlyEnterThisVehicle(const_cast<CVehicle*>(pVehicle));
				CTheScripts::GetCurrentGtaScriptThread()->SetHasUsedPedPathCommand(true);
			}
		}
	}
}
}

bool CommandAwardAchievement(int achievementId)
{
	return CLiveManager::GetAchMgr().AwardAchievement(achievementId);
}

bool CommandSetAchievementProgress(int achievementId, int progress)
{
	return CLiveManager::GetAchMgr().SetAchievementProgress(achievementId, progress);
}

int CommandGetAchievementProgress(int achievementId)
{
	return CLiveManager::GetAchMgr().GetAchievementProgress(achievementId);
}

bool CommandHasAchievementBeenPassed(int achievementId)
{
	return CLiveManager::GetAchMgr().HasAchievementBeenPassed(achievementId);
}

bool CommandIsPlayerOnline()
{
	return NetworkInterface::IsLocalPlayerOnline();
}

float CommandGetPlayerNoiseMultiplier(int playerIndex)
{
	const CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (pPlayer)
	{
		const CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pInfo, "Expected CPlayerInfo."))
		{
			return pInfo->GetStealthMultiplier();
		}
	}
	return 0.0f;
}

void CommandSetPlayerNoiseMultiplier(int playerIndex, float multiplier)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pInfo, "Expected CPlayerInfo"))
		{
			pInfo->SetNormalStealthMultiplier(multiplier);
		}
	}
}

void CommandSetPlayerSneakingNoiseMultiplier(int playerIndex, float multiplier)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pInfo, "Expected CPlayerInfo"))
		{
			pInfo->SetSneakingStealthMultiplier(multiplier);
		}
	}
}

bool CommandCanPedHearPlayer(int playerIndex, int pedIndex)
{
	const CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if (pPlayer && pPed)
	{
		const CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if (Verifyf(pInfo, "Expected CPlayerInfo"))
		{
			Vector3 vDistance = VEC3V_TO_VECTOR3(pPlayer->GetTransformPtr()->GetPosition() - pPed->GetTransformPtr()->GetPosition());
			float fDistance2 = vDistance.Mag2();
			if (rage::square(pInfo->GetStealthNoise()) < fDistance2) {
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

#if __PPU
namespace rage
{
extern rlNp g_rlNp;
}
#endif  //__PPU

bool CommandIsPlayerLoggingInNp()
{
#if __PPU
    return !g_rlNp.IsOnline(0) && g_rlNp.IsLoggedIn(0);
#else
    return false;
#endif  //__PPU
}

bool CommandIsPlayerOnlineNp()
{
#if __PPU
    return g_rlNp.IsOnline(0);
#else
    return false;
#endif  //__PPU
}

bool CommandIsPlayerOnlineGamespy()
{
	gnetAssertf(0, "%s : IS_PLAYER_ONLINE_GAMESPY - Command is deprecated - remove call to command!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    return false;
}

void CommandShowSigninUi(bool bForce)
{
#if RSG_ORBIS
	(void)bForce;
	gnetError("CommandShowSigninUi does not work on PS4!");
#else
#if !__NO_OUTPUT
	gnetDisplay("%s - DISPLAY_SYSTEM_SIGNIN_UI - Showing signin UI from script.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scrThread::PrePrintStackTrace();
#endif
    CLiveManager::ShowSigninUi(bForce ? rlSystemUi::SIGNIN_FORCE : 0);
#endif
}

bool CommandIsSytemUiShowing()
{
    return CLiveManager::IsSystemUiShowing();
}

void CommandSetPlayerInvincible(int PlayerIndex, bool bInvincible)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (CTheScripts::Frack() && pPlayer)
	{
#if __BANK
		const bool bCurrentlyInvincibile = pPlayer->m_nPhysicalFlags.bNotDamagedByAnything == true;
		if (bCurrentlyInvincibile != bInvincible)
		{
			scriptDebugf1("%s - SET_PLAYER_INVINCIBLE changed to %s on %s, was previously %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bInvincible ? "TRUE" : "FALSE", pPlayer->GetDebugName(), pPlayer->m_nPhysicalFlags.bNotDamagedByAnything ? "TRUE" : "FALSE");
			scrThread::PrePrintStackTrace();
		}
#endif

		pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = bInvincible;
	}
}

bool CommandGetPlayerInvincible(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		return pPlayer->m_nPhysicalFlags.bNotDamagedByAnything;
	}

	return false;
}

bool CommandGetPlayerDebugInvincible(int NOTFINAL_ONLY(PlayerIndex))
{
#if !__FINAL
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		return pPlayer->IsDebugInvincible();
	}
#endif

	return false;
}

void CommandSetPlayerInvincibleButHasReactions(int PlayerIndex, bool bInvincible)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
#if __BANK
		const bool bCurrentlyInvincibile = pPlayer->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions == true;
		if (bCurrentlyInvincibile != bInvincible)
		{
			scriptDebugf1("%s - SET_PLAYER_INVINCIBLE_BUT_HAS_REACTIONS changed to %s on %s, was previously %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bInvincible ? "TRUE" : "FALSE", pPlayer->GetDebugName(), pPlayer->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions ? "TRUE" : "FALSE");
			scrThread::PrePrintStackTrace();
		}
#endif

		pPlayer->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions = bInvincible;
	}
}

void CommandAllowPlayerToCarryNonMissionObjects(int PlayerIndex, bool bAllowedToCarry)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerDataAllowedToPickUpNonMissionObjects( bAllowedToCarry );
	}
}

void CommandSetPlayerCanCollectDroppedMoney(int PlayerIndex, bool bCanCollect)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerDataCanCollectMoneyPickups( bCanCollect );
	}

}

void CommandSetPlayerCanUseVehicleSearchLight(int PlayerIndex, bool bCanUseSearchLight)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetCanUseSearchLight( bCanUseSearchLight );
	}
}

void CommandRemovePlayerHelmet(int PlayerIndex, bool ForceRemove)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		// setting this flag false should mean the task CTaskSimpleTakeOffHelmet will remove the helmet
		pPlayer->SetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet, false );

		if(ForceRemove && pPlayer->GetHelmetComponent())
		{
			pPlayer->GetHelmetComponent()->DisableHelmet();
		}
	}
}

void CommandGivePlayerRagdollControl(int PlayerIndex, bool bGiveControl)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		if(SCRIPT_VERIFY(pPlayer->GetUsingRagdoll(), "GIVE_PLAYER_RAGDOLL_CONTROL - Player ped is not ragdolling"))
		{
			pPlayer->GetPlayerInfo()->SetPlayerDataHasControlOfRagdoll( bGiveControl );
		}
	}
}



void CommandSetPlayerLockon(int PlayerIndex, bool bEnable)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		scriptDebugf1("SET_PLAYER_LOCKON - Flag DisablePlayerLockon=%s - %s", !bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		pPlayer->SetPedConfigFlag( CPED_CONFIG_FLAG_DisablePlayerLockon, !bEnable );
	}
}


void CommandAllowLockonToFriendlyPlayers(int PlayerIndex, bool Allow)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers, Allow );
	}
}

void CommandSetPlayerTargetingMode(int mode)
{
    Assertf(mode >= 0 && mode < CPedTargetEvaluator::MAX_TARGETING_OPTIONS, "Invalid targeting mode assigned from script command! %d", mode);
    if(mode>=0 && mode < CPedTargetEvaluator::MAX_TARGETING_OPTIONS)
    {
        CPauseMenu::SetMenuPreference(PREF_TARGET_CONFIG, mode);

        CProfileSettings& settings = CProfileSettings::GetInstance();
        settings.Set(CProfileSettings::TARGETING_MODE, CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG));
    }
}

int CommandGetPlayerTargetingMode()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	int iTargetingMode = settings.GetInt(CProfileSettings::TARGETING_MODE);
	return iTargetingMode;
}

void CommandSetPlayerTargetLevel(int level)
{
	CPlayerPedTargeting::SetPlayerTargetLevel(static_cast<CPlayerPedTargeting::ePlayerTargetLevel>(level));
}

bool CommandIsUsingFPSThirdPersonCover()
{
	return CPauseMenu::GetMenuPreference(PREF_FPS_THIRD_PERSON_COVER) ? true : false;
}

bool CommandIsUsingHoodCamera()
{
	return CPauseMenu::GetMenuPreference(PREF_HOOD_CAMERA) ? true : false;
}

int CommandWhatWillPlayerPickup(int UNUSED_PARAM(PlayerIndex))
{
#if 0 // CS
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		CWeapon* pPlayerWeapon = pPlayer->GetWeaponMgr()->GetWeaponFromObject();
		if(pPlayerWeapon && pPlayerWeapon->GetWeaponType()!=WEAPONTYPE_OBJECT && !pPlayer->HasPhoneEquipped() && !pPlayer->GetWeaponMgr()->GetWeaponObjectBackup())
		{
			CObject* pPickupObject = CTaskPlayerOnFoot::CheckForObjectToPickup(pPlayer);
			if(pPickupObject)
			{
//				return CObject::GetPool()->GetIndex(pPickupObject);
				return CTheScripts::GetGUIDFromEntity(*pPickupObject);
			}
		}
	}
#endif // 0
	return NULL_IN_SCRIPTING_LANGUAGE;
}

void CommandClearPlayerHasDamagedAtLeastOnePed(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->bHasDamagedAtLeastOnePed = false;
	}
}

bool CommandHasPlayerDamagedAtLeastOnePed(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer && pPlayer->GetPlayerInfo()->bHasDamagedAtLeastOnePed);
}

void CommandClearPlayerHasDamagedAtLeastOneNonAnimalPed(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->bHasDamagedAtLeastOneNonAnimalPed = false;
	}
}

bool CommandHasPlayerDamagedAtLeastOneNonAnimalPed(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	return (pPlayer && pPlayer->GetPlayerInfo()->bHasDamagedAtLeastOneNonAnimalPed);
}

void CommandForceAirDragMultForPlayersVehicle(int PlayerIndex, float fDragMult)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		if(SCRIPT_VERIFY(fDragMult >= 1.0, "SET_AIR_DRAG_MULTIPLIER_FOR_PLAYERS_VEHICLE - Mult must be 1.0 or greater"))
		{
			if(SCRIPT_VERIFY(fDragMult <= 50.0, "SET_AIR_DRAG_MULTIPLIER_FOR_PLAYERS_VEHICLE - Mult must be less than 50.0"))
			{
				pPlayer->GetPlayerInfo()->m_fForceAirDragMult = fDragMult;
			}
		}
	}
}

void CommandSetSwimMultiplierForPlayer(int PlayerIndex, float fMultiplier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		if (SCRIPT_VERIFY(fMultiplier >= 1.0, "SET_SWIM_MULTIPLIER_FOR_PLAYER - Multiplier must be 1.0 or greater"))
		{
			if (SCRIPT_VERIFY(fMultiplier < 1.5, "SET_SWIM_MULTIPLIER_FOR_PLAYER - Multiplier must be less than 1.5"))
			{
				pPlayer->GetPlayerInfo()->m_fSwimSpeedMultiplier = fMultiplier;
			}
		}
	}
}

void CommandSetRunSprintMultiplierForPlayer(int PlayerIndex, float fMultiplier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		if (SCRIPT_VERIFY(fMultiplier >= 1.0, "SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER - Multiplier must be 1.0 or greater"))
		{
			if (SCRIPT_VERIFY(fMultiplier < 1.5, "SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER - Multiplier must be less than 1.5"))
			{
				pPlayer->GetPlayerInfo()->SetRunSprintSpeedMultiplier(fMultiplier);
			}
		}
	}
}

int CommandGetTimeSinceLastArrest()
{
	if (CGameLogic::GetTimeOfLastRespawnAfterArrest() == 0)
	{
		return -1;
	}

	return fwTimer::GetTimeInMilliseconds() - CGameLogic::GetTimeOfLastRespawnAfterArrest();
}

int CommandGetTimeSinceLastDeath()
{
	if (CGameLogic::GetTimeOfLastRespawnAfterDeath() == 0)
	{
		return -1;
	}
	return fwTimer::GetTimeInMilliseconds() - CGameLogic::GetTimeOfLastRespawnAfterDeath();
}

int CommandGetTrainPlayerWouldEnter(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	int ReturnVehicleIndex = NULL_IN_SCRIPTING_LANGUAGE;

	if (pPlayer)
	{
		SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "GET_TRAIN_PLAYER_WOULD_ENTER - Check player character is alive this frame");

		CVehicle *pVeh = pPlayer->GetPlayerInfo()->ScanForTrainToEnter(pPlayer, VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetB()), false );

		if (pVeh)
		{
			ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*pVeh);
		}
	}

	return ReturnVehicleIndex;
}


void CommandSetDrawPlayerComponent(int componentIdx, bool bEnable){

	if (componentIdx >= 0){
		CPedVariationStream::SetDrawPlayerComp(componentIdx, bEnable);
	}
}

bool CommandDoesMainPlayerExist()
{
	return CGameWorld::GetMainPlayerInfo() != NULL;
}

//****************************************************************
// CommandAssistedMovementOpenRoute - these are zero-based from
// 0..CAssistedMovementRouteStore::MAX_SCRIPTED_ROUTES
// Opening a route allows editing

void CommandAssistedMovementOpenRoute(const int iRouteIndex)
{
	if(SCRIPT_VERIFY(iRouteIndex >= 0 && iRouteIndex < CAssistedMovementRouteStore::MAX_SCRIPTED_ROUTES, "ASSISTED_MOVEMENT_OPEN_ROUTE - route index out of range"))
	{
		if(SCRIPT_VERIFY(CAssistedMovementRouteStore::GetScriptRouteEditIndex()==-1, "ASSISTED_MOVEMENT_OPEN_ROUTE - a route is already being edited"))
		{
			CAssistedMovementRouteStore::SetScriptRouteEditIndex(iRouteIndex);
		}
	}
}

//********************************************************************
// CommandAssistedMovementCloseRoute - closes the route being edited

void CommandAssistedMovementCloseRoute()
{
	int iScriptRouteIndex = CAssistedMovementRouteStore::GetScriptRouteEditIndex();

	if(SCRIPT_VERIFY(iScriptRouteIndex!=-1, "ASSISTED_MOVEMENT_CLOSE_ROUTE - no route is being edited"))
	{
		// When closing the route, calculate the route hash.  Yes, these don't change since the index
		// is fixed - but doing it here is a precaution, because the hash is cleared when the route is.

		const int iActualIndex = iScriptRouteIndex + CAssistedMovementRouteStore::FIRST_SCRIPTED_ROUTE;
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(iActualIndex);
		const u32 iRouteHash = CAssistedMovementRouteStore::CalcScriptedRouteHash(iScriptRouteIndex);
		pRoute->SetPathStreetNameHash(iRouteHash);

		CAssistedMovementRouteStore::SetScriptRouteEditIndex(-1);
	}
}

//*********************************************************************
// CommandAssistedMovementCloseRoute - clears the route being edited

void CommandAssistedMovementFlushRoute()
{
	if(SCRIPT_VERIFY(CAssistedMovementRouteStore::GetScriptRouteEditIndex()!=-1, "ASSISTED_MOVEMENT_FLUSH_ROUTE - no route is being edited"))
	{
		const int iIndex = CAssistedMovementRouteStore::GetScriptRouteEditIndex() + CAssistedMovementRouteStore::FIRST_SCRIPTED_ROUTE;
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(iIndex);
		pRoute->Clear();
	}
}

//****************************************************************************
// CommandAssistedMovementCloseRoute - adds a point to the route being edited
// x,y,z : the position
// fWidth : path radius
// fTension : 0.0=weak, 0.5=default, 1.0=strong
// bNonLeave : no way of leaving or turning on this route

void CommandAssistedMovementAddPoint(const float x, const float y, const float z, const float fWidth, float fTension, const s32 iFlags)
{
	if(SCRIPT_VERIFY(CAssistedMovementRouteStore::GetScriptRouteEditIndex()!=-1, "ASSISTED_MOVEMENT_ADD_POINT - no route is being edited"))
	{
		const int iIndex = CAssistedMovementRouteStore::GetScriptRouteEditIndex() + CAssistedMovementRouteStore::FIRST_SCRIPTED_ROUTE;
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(iIndex);

		SCRIPT_ASSERT(fTension >= 0.0f && fTension <= 1.0f, "ASSISTED_MOVEMENT_ADD_POINT - fTension param must be within range 0.0 to 1.0");
		fTension = Clamp(fTension, 0.0f, 1.0f);

		if(SCRIPT_VERIFY(pRoute->GetSize() < CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS, "ASSISTED_MOVEMENT_ADD_POINT - max num points exceeded"))
		{
			pRoute->AddPoint(Vector3(x,y,z), fWidth, iFlags, fTension*2.0f);
		}
	}
}

//*******************************************************************************
// CommandAssistedMovementCloseRoute - sets the width of the route being edited

void CommandAssistedMovementSetWidth(const float UNUSED_PARAM(fWidth))
{
	Assertf(false, "ASSISTED_MOVEMENT_SET_WIDTH has been removed.  This value is now set in the ASSISTED_MOVEMENT_ADD_POINT command.");
}

//*******************************************************************************************
// CommandAssistedMovementIsOnAnyRoute - returns whether the player is attached to any route

bool CommandAssistedMovementIsOnAnyRoute()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		CTaskMovePlayer * pPlayerMove = (CTaskMovePlayer*)pPlayer->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_PLAYER);
		if(pPlayerMove)
		{
			CPlayerAssistedMovementControl * pControl = pPlayerMove->GetAssistedMovementControl();
			if(pControl)
			{
				const CAssistedMovementRoute & route = pControl->GetRoute();
				if(route.GetSize() && pControl->GetIsUsingRoute())
				{
					return true;
				}
			}
		}
	}
	return false;
}

//*******************************************************************************************
// CommandAssistedMovementIsOnScriptedRoute - returns whether the player is attached to a
// scripted route.  If so it will return the scripted route index (0..MAX_SCRIPTED_ROUTES),
// otherwise it will return -1

int CommandAssistedMovementIsOnScriptedRoute()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		CTaskMovePlayer * pPlayerMove = (CTaskMovePlayer*)pPlayer->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_PLAYER);
		if(pPlayerMove)
		{
			CPlayerAssistedMovementControl * pControl = pPlayerMove->GetAssistedMovementControl();
			if(pControl)
			{
				const CAssistedMovementRoute & route = pControl->GetRoute();
				if(route.GetSize() && pControl->GetIsUsingRoute())
				{
					for(int r=0; r<CAssistedMovementRouteStore::MAX_SCRIPTED_ROUTES; r++)
					{
						const int iRouteIndex = r + CAssistedMovementRouteStore::FIRST_SCRIPTED_ROUTE;
						CAssistedMovementRoute * pStoredRoute = CAssistedMovementRouteStore::GetRoute(iRouteIndex);
						if(route.GetPathStreetNameHash() && route.GetPathStreetNameHash()==pStoredRoute->GetPathStreetNameHash())
						{
							return r;
						}
					}
				}
			}
		}
	}

	return -1;
}

void CommandSetPlayerForcedAim(int PlayerIndex, bool ForcedAim)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_FORCED_AIM - Check player character is alive this frame");

	pPlayer->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcedAim, ForcedAim );
}

void CommandSetPlayerForcedZoom(int PlayerIndex, bool ForcedZoom)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer || !pPlayer->GetPlayerInfo())
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_FORCED_ZOOM - Check player character is alive this frame");

	if (ForcedZoom)
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_FORCED_ZOOM);
	else
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().UnsetFlag(CPlayerResetFlags::PRF_FORCED_ZOOM);
}

void CommandSetPlayerForceSkipAimIntro(int PlayerIndex, bool SkipIntro)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (!pPlayer || !pPlayer->GetPlayerInfo())
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_FORCE_SKIP_AIM_INTRO - Check player character is alive this frame");

	if (SkipIntro)
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_FORCE_SKIP_AIM_INTRO);
	else
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().UnsetFlag(CPlayerResetFlags::PRF_FORCE_SKIP_AIM_INTRO);
}

void CommandSetDisableAmbientMeleeMove(int PlayerIndex, bool bDisable)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (!pPlayer)
		return;

	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_DISABLE_AMBIENT_MELEE_MOVE - Check player character is alive this frame");

	pPlayer->GetPlayerInfo()->SetPlayerDataAmbientMeleeMoveDisabled( bDisable );
}

void CommandSetPlayerMaxMoveBlendRatio(int PlayerIndex, float fMaxMoveBlendRatio)
{
	scriptAssertf(0, "%s: (SET_PLAYER_MAX_MOVE_BLEND_RATIO - this is being deprecated. Use SET_PED_MAX_MOVE_BLEND_RATIO instead", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
 
 	if (!SCRIPT_VERIFY(pPlayer,"Could not find player"))
 		return;
 
 	SCRIPT_ASSERT(pPlayer->m_nDEflags.bCheckedForDead == TRUE, "SET_PLAYER_MAX_MOVE_BLEND_RATIO - Check player character is alive this frame");
 
	pPlayer->GetMotionData()->SetScriptedMaxMoveBlendRatio(fMaxMoveBlendRatio);
}

void CommandSetInvertPlayerWalk(bool invert)
{
    CPed * pPlayer = CGameWorld::FindLocalPlayer();

	if(pPlayer)
    {
	    CControl *pControl = pPlayer->GetControlFromPlayer();

        if(pControl)
        {
            pControl->SetPedWalkInverted(invert);
        }
    }
}

void CommandSetInvertPlayerLook(bool invert)
{
    CPed * pPlayer = CGameWorld::FindLocalPlayer();

	if(pPlayer)
    {
	    CControl *pControl = pPlayer->GetControlFromPlayer();

        if(pControl)
        {
            pControl->SetPedLookInverted(invert);
        }
    }
}

void CommandDisablePlayerFiring(int UNUSED_PARAM(PlayerIndex), bool UNUSED_PARAM(bDisable))
{
	CControlMgr::GetMainPlayerControl().DisableInput(INPUT_ATTACK);
	CControlMgr::GetMainPlayerControl().DisableInput(INPUT_ATTACK2);
	CControlMgr::GetMainPlayerControl().DisableInput(INPUT_VEH_ATTACK);
	CControlMgr::GetMainPlayerControl().DisableInput(INPUT_VEH_ATTACK2);
}

void CommandDisablePlayerThrowingGrenadeWhileUsingGun()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		pPlayer->SetPedResetFlag(CPED_RESET_FLAG_DisableProjectileThrowsWhileAimingGun, true);
	}
}


void CommandSetPlayerJackSpeed(int PlayerIndex, int JackSpeed)
{
	scriptAssertf(0, "SET_PLAYER_JACK_SPEED is deprecated, please speak to an AI coder if you need this functionality");
	if (!SCRIPT_VERIFY(JackSpeed >= 75, "Trying to set jack speed to less than 75% which would look odd"))
		return;

	if (!SCRIPT_VERIFY(JackSpeed <= 125, "Trying to set jack speed to more than 125% which would look odd"))
		return;

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->JackSpeed = static_cast<u16>(JackSpeed);
	}
}

void CommandSetPlayerStealthSpeed(int PlayerIndex, int StealthSpeed)
{
	static const int MIN_STEALTH_SPEED = 80;
	static const int MAX_STEALTH_SPEED = 120;
	scriptAssertf(StealthSpeed >= MIN_STEALTH_SPEED, "%s: Trying to set stealth speed to less than %d which would look odd", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MIN_STEALTH_SPEED);
	scriptAssertf(StealthSpeed <= MAX_STEALTH_SPEED, "%s: Trying to set stealth speed to more than %d which would look odd", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_STEALTH_SPEED);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		StealthSpeed = Clamp(StealthSpeed, MIN_STEALTH_SPEED, MAX_STEALTH_SPEED);
		pPlayer->GetPlayerInfo()->StealthRate = static_cast<float>(StealthSpeed) / 100.f;
	}
}

void CommandSetPlayerMaxArmour(int PlayerIndex, int MaxArmour)
{
    if (!SCRIPT_VERIFY(MaxArmour >= 0, "Trying to set max armour to less than zero!"))
		return;

    CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
        pPlayer->GetPlayerInfo()->MaxArmour = static_cast<u16>(MaxArmour);
    }
}

int CommandGetPlayerSelectedAbilitySlot(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (scriptVerifyf(pPlayer, "GET_PLAYER_SELECTED_ABILITY_SLOT - No local player found with index %d!", PlayerIndex))
	{		
		return (int)pPlayer->GetSelectedAbilitySlot();
	}
	else
	{
		return -1;
	}
}

int CommandGetLocalPlayerSpecialAbilityMP(int abilitySlot)
{
	SCRIPT_ASSERT(NetworkInterface::IsGameInProgress(), "GET_LOCAL_PLAYER_SPECIAL_ABILITY_MP - Called on a SP game!");
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	const CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
	const bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayerInfo, "GET_LOCAL_PLAYER_SPECIAL_ABILITY_MP - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "GET_LOCAL_PLAYER_SPECIAL_ABILITY_MP - invalid ability slot: %d", abilitySlot))
	{
		return pPlayerInfo->GetMPSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
	}

	return -1;
}

void CommandSetSpecialAbilityMP(int PlayerIndex, int abilityType, int abilitySlot = 0)
{
	SCRIPT_ASSERT(NetworkInterface::IsGameInProgress(), "SET_SPECIAL_ABILITY_MP - Called on a SP game!");
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(scriptVerifyf(pPlayer, "SET_SPECIAL_ABILITY_MP - No local player found with index %d!", PlayerIndex))
	{
		CPlayerInfo *pPlayerInfo = pPlayer->GetPlayerInfo();
		bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
		if (scriptVerifyf(pPlayerInfo, "SET_SPECIAL_ABILITY_MP - Player with no player info!")
			&& scriptVerifyf(abilityType >= SAT_NONE && abilityType < SAT_MAX, "SET_SPECIAL_ABILITY_MP - Passed wrong ability input: %d", abilityType)
			&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SET_SPECIAL_ABILITY_MP - invalid ability slot: %d", abilitySlot))
		{
			pPlayerInfo->SetMPSpecialAbility( static_cast<SpecialAbilityType>(abilityType), (ePlayerSpecialAbilitySlot)abilitySlot );
#if __ASSERT
			fwTimer::SetTimescaleIsBeingModifiedByScript(true);

			scriptDebugf1("Special ability %d set on player %s for ability slot %d", abilityType, pPlayer->GetLogName(), abilitySlot);
#endif			
		}
	}
}

bool CommandCanSpecialAbilityBeActivated(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "CAN_SPECIAL_ABILITY_BE_ACTIVATED - No local player found with index %d!", playerIndex)		
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "CAN_SPECIAL_ABILITY_BE_ACTIVATED - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			return ability->CanBeActivated();
		}
	}
	return false;
}

bool CommandCanSpecialAbilityBeSelectedInArcadeMode(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "CAN_SPECIAL_ABILITY_BE_SELECTED_IN_ARCADE_MODE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "CAN_SPECIAL_ABILITY_BE_SELECTED_IN_ARCADE_MODE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			return ability->CanBeSelectedInArcadeMode();
		}
	}
	return false;
}

void CommandSpecialAbilityActivate(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_ACTIVATE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_ACTIVATE - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_ACTIVATE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
#if !__NO_OUTPUT
			bool activated = ability->Activate();
			scriptDebugf1("Special ability activated: %s (on player: %s)", activated ? "TRUE" : "FALSE", pPlayer->GetLogName());
#else
			ability->Activate();
#endif
		}
	}
	
}

void CommandSpecialAbilityDeactivate(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_DEACTIVATE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_DEACTIVATE - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_DEACTIVATE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			ability->Deactivate();
		}
	}
}

void CommandSpecialAbilityDeactivateFast(int playerIndex, int abilitySlot = 0)
{
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		return;
	}

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_DEACTIVATE_FAST - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_DEACTIVATE_FAST - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_DEACTIVATE_FAST - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			ability->DeactivateNoFadeOut();
		}
	}
}

void CommandSpecialAbilityDeactivateMP(int playerIndex, int abilitySlot = 0)
{
#if __ASSERT
	fwTimer::SetTimescaleIsBeingModifiedByScript(false);
#endif	
	CommandSpecialAbilityDeactivateFast(playerIndex);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_DEACTIVATE_MP - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_DEACTIVATE_MP - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_DEACTIVATE_MP - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			ability->StopFx();
			scriptDebugf1("Special ability deactivated on player %s", pPlayer->GetLogName());
		}
	}
}

void CommandSpecialAbilityReset(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_RESET - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_RESET - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_RESET - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* ability = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (ability)
		{
			ability->Reset();
		}
	}
}

void CommandSpecialAbilityChargeOnMissionFailed(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_ON_MISSION_FAILED - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_ON_MISSION_FAILED - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_ON_MISSION_FAILED - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_MISSION_FAILED, pPlayer);
	}
}

void CommandSpecialAbilityChargeSmall(int playerIndex, bool increment, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_SMALL - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_SMALL - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_SMALL - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_SMALL called on player with no special ability in slot %d!", abilitySlot))
		{
			if (increment)
			{
				specialAbility->AddSmallCharge(ignoreActive);
			}
			else
			{
				specialAbility->RemoveSmallCharge(ignoreActive);
			}
		}
	}
}

void CommandSpecialAbilityChargeMedium(int playerIndex, bool increment, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_MEDIUM - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_MEDIUM - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_MEDIUM - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_MEDIUM called on player with no special ability in slot %d!", abilitySlot))
		{
			if (increment)
			{
				specialAbility->AddMediumCharge(ignoreActive);
			}
			else
			{
				specialAbility->RemoveMediumCharge(ignoreActive);
			}
		}
	}
}

void CommandSpecialAbilityChargeLarge(int playerIndex, bool increment, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_LARGE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_LARGE - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_LARGE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility();
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_LARGE called on player with no special ability in slot %d!", abilitySlot))
		{
			if (increment)
			{
				specialAbility->AddLargeCharge(ignoreActive);
			}
			else
			{
				specialAbility->RemoveLargeCharge(ignoreActive);
			}
		}
	}
}

void CommandSpecialAbilityChargeContinuous(int playerIndex, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_CONTINUOUS - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_CONTINUOUS - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_CONTINUOUS - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_CONTINUOUS called on player with no special ability in slot %d!", abilitySlot))
		{
			specialAbility->AddToMeterContinuous(ignoreActive, 1.f);
		}
	}
}

void CommandSpecialAbilityChargeAbsolute(int playerIndex, int charge, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_CONTINUOUS - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_CONTINUOUS - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_CONTINUOUS - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_ABSOLUTE called on player with no special ability in slot %d!", abilitySlot))
		{
			specialAbility->AddToMeter(charge, ignoreActive);
		}
	}
}

void CommandSpecialAbilityChargeNormalized(int playerIndex, float charge, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_CHARGE_NORMALIZED - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_CHARGE_NORMALIZED - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_CHARGE_NORMALIZED - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_CHARGE_NORMALIZED called on player with no special ability in slot %d!", abilitySlot))
		{
			specialAbility->AddToMeterNormalized(charge, ignoreActive, false);
		}
	}
}

void CommandSpecialAbilityFillMeter(int playerIndex, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_FILL_METER - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_FILL_METER - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_FILL_METER - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_FILL_METER called on player with no special ability in slot %d!", abilitySlot))
		{
			specialAbility->FillMeter(ignoreActive);
		}
	}
}

void CommandSpecialAbilityDepleteMeter(int playerIndex, bool ignoreActive, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "SPECIAL_ABILITY_DEPLETE_METER - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "SPECIAL_ABILITY_DEPLETE_METER - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "SPECIAL_ABILITY_DEPLETE_METER - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "SPECIAL_ABILITY_DEPLETE_METER called on player with no special ability in slot! %d", abilitySlot))
		{
			specialAbility->DepleteMeter(ignoreActive);
		}
	}
}

void CommandSpecialAbilityLock(int pedModelNameHash, bool isArcadePlayer)
{
    CPlayerSpecialAbilityManager::SetAbilityStatus((u32)pedModelNameHash, false, isArcadePlayer);
}

void CommandSpecialAbilityUnlock(int pedModelNameHash, bool isArcadePlayer)
{
    CPlayerSpecialAbilityManager::SetAbilityStatus((u32)pedModelNameHash, true, isArcadePlayer);
}

float CommandGetSpecialAbilityRemainingPercentage(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "GET_SPECIAL_ABILITY_REMAINING_PERCENTAGE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "GET_SPECIAL_ABILITY_REMAINING_PERCENTAGE - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "GET_SPECIAL_ABILITY_REMAINING_PERCENTAGE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if (scriptVerifyf(specialAbility, "GET_SPECIAL_ABILITY_REMAINING_PERCENTAGE called on player with no special ability in slot! %d", abilitySlot))
		{
			return specialAbility->GetRemainingPercentageForDisplay();
		}
	}
	return 0.f;
}

bool CommandIsSpecialAbilityUnlocked(int pedModelNameHash)
{
    return CPlayerSpecialAbilityManager::IsAbilityUnlocked(pedModelNameHash);
}

bool CommandIsSpecialAbilityActive(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "IS_SPECIAL_ABILITY_ACTIVE - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "IS_SPECIAL_ABILITY_ACTIVE - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "IS_SPECIAL_ABILITY_ACTIVE - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "IS_SPECIAL_ABILITY_ACTIVE called on player with no special ability in slot %d!", abilitySlot))
		{
	        return specialAbility->IsActive();	
		}
	}
    return false;
}

bool CommandIsSpecialAbilityMeterFull(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "IS_SPECIAL_ABILITY_METER_FULL - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "IS_SPECIAL_ABILITY_METER_FULL - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "IS_SPECIAL_ABILITY_METER_FULL - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "IS_SPECIAL_ABILITY_METER_FULL called on player with no special ability in slot %d!", abilitySlot))
		{
	        return specialAbility->IsMeterFull();	
		}
	}
    return false;
}

void CommandEnableSpecialAbility(int playerIndex, bool enable, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "ENABLE_SPECIAL_ABILITY - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "ENABLE_SPECIAL_ABILITY - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "ENABLE_SPECIAL_ABILITY - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "ENABLE_SPECIAL_ABILITY called on player with no special ability in slot %d!", abilitySlot))
		{
			if (enable)
				specialAbility->EnableAbility();
			else
				specialAbility->DisableAbility();
		}
	}
}

bool CommandIsSpecialAbilityEnabled(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "ENABLE_SPECIAL_ABILITY - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "ENABLE_SPECIAL_ABILITY - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "ENABLE_SPECIAL_ABILITY - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility((ePlayerSpecialAbilitySlot)abilitySlot);
		if(scriptVerifyf(specialAbility, "IS_SPECIAL_ABILITY_ENABLED called on player with no special ability in slot %d!", abilitySlot))
		{
			return specialAbility->IsAbilityEnabled();	
		}
	}
	return false;
}

void CommandSetSpecialAbilityMultiplier(float multiplier)
{
    CPlayerSpecialAbilityManager::SetChargeMultiplier(multiplier);
}

void CommandUpdateSpecialAbilityFromStat(int playerIndex, int abilitySlot = 0)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "ENABLE_SPECIAL_ABILITY - No local player found with index %d!", playerIndex)
		&& scriptVerifyf(pPlayer->GetPlayerInfo(), "ENABLE_SPECIAL_ABILITY - Player with no player info!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "ENABLE_SPECIAL_ABILITY - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* specialAbility = pPlayer->GetSpecialAbility();
		if(scriptVerifyf(specialAbility, "UPDATE_SPECIAL_ABILITY_FROM_STAT called on player with no special ability in slot %d!", abilitySlot))
		{
			return specialAbility->UpdateFromStat();	
		}
	}
}

void CommandStartLocalPlayerSpecialAbilityFX(int abilitySlot = 0)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "START_LOCAL_PLAYER_SPECIAL_ABILITY_FX - No local player found!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "START_LOCAL_PLAYER_SPECIAL_ABILITY_FX - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* pSpecialAbility = pPlayer->GetSpecialAbility();
		if (scriptVerifyf(pSpecialAbility, "START_LOCAL_PLAYER_SPECIAL_ABILITY_FX called on player with no special ability in slot %d!", abilitySlot))
		{
			pSpecialAbility->StartFx();
		}
	}
}

void CommandStopLocalPlayerSpecialAbilityFX(int abilitySlot = 0)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	bool bIsValidSpecialAbilitySlot = CPed::IsValidSpecialAbilitySlot((ePlayerSpecialAbilitySlot)abilitySlot);
	if (scriptVerifyf(pPlayer, "STOP_LOCAL_PLAYER_SPECIAL_ABILITY_FX - No local player found!")
		&& scriptVerifyf(bIsValidSpecialAbilitySlot, "STOP_LOCAL_PLAYER_SPECIAL_ABILITY_FX - invalid ability slot: %d", abilitySlot))
	{
		CPlayerSpecialAbility* pSpecialAbility = pPlayer->GetSpecialAbility();
		if (scriptVerifyf(pSpecialAbility, "STOP_LOCAL_PLAYER_SPECIAL_ABILITY_FX called on player with no special ability in slot %d!", abilitySlot))
		{
			pSpecialAbility->StopFx();
		}
	}
}

void CommandInitialiseArcadeAbilities(int playerIndex)
{
	scriptDebugf1("%s : INITIALISE_ARCADE_ABILITIES - called for player %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeAbilityEffects().Initialise();
	}
}

void CommandActivateArcadeAbility(int playerIndex, int arcadeAbility)
{
	scriptDebugf1("%s : ACTIVATE_ARCADE_ABILITY - called for player %d, arcadeAbility: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, arcadeAbility);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeAbilityEffects().SetIsActive(eArcadeAbilityEffectType(arcadeAbility), true);
	}
}

void CommandDeactivateArcadeAbility(int playerIndex, int arcadeAbility)
{
	scriptDebugf1("%s : DEACTIVATE_ARCADE_ABILITY - called for player %d, arcadeAbility: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, arcadeAbility);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeAbilityEffects().SetIsActive(eArcadeAbilityEffectType(arcadeAbility), false);
	}
}

bool CommandIsArcadeAbilityActive(int playerIndex, int arcadeAbility)
{
	CPed* pPlayer = FindPlayerPedByIndex(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		return pPlayer->GetPlayerInfo()->GetArcadeAbilityEffects().GetIsActive(eArcadeAbilityEffectType(arcadeAbility));
	}

	return false;
}

void CommandSetArcadeAbilityModifier(int playerIndex, int arcadeAbility, float arcadeAbilityModifier)
{
	scriptDebugf1("%s : SET_ARCADE_ABILITY_MODIFIER - called for player %d, arcadeAbility: %d, mod: %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex, arcadeAbility, arcadeAbilityModifier);
	
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeAbilityEffects().SetModifier(eArcadeAbilityEffectType(arcadeAbility), arcadeAbilityModifier);
	}
}

float CommandGetArcadeAbilityModifier(int playerIndex, int arcadeAbility)
{	
	CPed* pPlayer = FindPlayerPedByIndex(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		return pPlayer->GetPlayerInfo()->GetArcadeAbilityEffects().GetModifier(eArcadeAbilityEffectType(arcadeAbility));
	}

	return 0.f;
}

void CommandCleanupArcadeAbilities(int playerIndex)
{
	scriptDebugf1("%s : CLEANUP_ARCADE_ABILITIES - called for player %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeAbilityEffects().Cleanup();
	}
}

int CommandGetArcadeTeam(int playerIndex)
{
	const CPed* pPlayer = FindPlayerPedByIndex(playerIndex);

	eArcadeTeam arcadeTeam = eArcadeTeam::AT_NONE;
	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		arcadeTeam = pPlayer->GetPlayerInfo()->GetArcadeInformation().GetTeam();
	}

	return static_cast<int>(arcadeTeam);
}

int CommandGetArcadeRole(int playerIndex)
{
	const CPed* pPlayer = FindPlayerPedByIndex(playerIndex);

	eArcadeRole arcadeRole = eArcadeRole::AR_NONE;
	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		arcadeRole = pPlayer->GetPlayerInfo()->GetArcadeInformation().GetRole();
	}

	return static_cast<int>(arcadeRole);
}

void CommandSetArcadeTeamAndRole(int playerIndex, int arcadeTeam, int arcadeRole)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		pPlayer->GetPlayerInfo()->AccessArcadeInformation().SetTeamAndRole(eArcadeTeam(arcadeTeam), eArcadeRole(arcadeRole));
	}

	CNewHud::HandleCharacterChange();
}

void CommandSetLocalPlayerArcadeActiveVehicle(int vehicleIndex)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		if (pVehicle)
		{
			pPlayer->GetPlayerInfo()->AccessArcadeInformation().SetActiveVehicle(pVehicle);
		}
	}
}

bool CommandGetIsPlayerDrivingOnHighway(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if( pPlayer && pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayer->GetMyVehicle() && pPlayer->GetMyVehicle()->IsDriver(pPlayer) )
	{
		if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
		{
			return pPlayer->GetPlayerInfo()->GetPlayerDataPlayerOnHighway() != 0;
		}
	}
	return false;
}

bool CommandGetIsPlayerDrivingWreckless(int playerIndex, int wrecklesstype)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (pPlayer && pPlayer->GetPlayerInfo())
	{
		switch ((eWrecklessType)wrecklesstype)
		{
			case DROVE_AGAINST_TRAFFIC:
			{
				return fwTimer::GetTimeInMilliseconds()-pPlayer->GetPlayerInfo()->GetPlayerDataLastTimePlayerDroveAgainstTraffic() < 2000;
				break;
			}

			case RAN_RED_LIGHT:
			{
				return fwTimer::GetTimeInMilliseconds()-pPlayer->GetPlayerInfo()->GetPlayerDataLastTimePlayerRanLight() < 2000;
				break;
			}

			case ON_PAVEMENT:
			{
				return fwTimer::GetTimeInMilliseconds()-pPlayer->GetPlayerInfo()->GetPlayerDataLastTimePlayerDroveOnPavement() < 500;
				break;
			}
		}
	}
	return false;
}

bool CommandGetIsMoppingAreaFreeInFrontOfPlayer(int playerIndex, float fMoppingRadius)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if( pPlayer && scriptVerifyf(pPlayer->GetCapsuleInfo(), "GET_IS_MOPPING_AREA_FREE_IN_FRONT_OF_PLAYER no capsule info!") )
	{
		const CBipedCapsuleInfo *pCapsuleInfo = pPlayer->GetCapsuleInfo()->GetBipedCapsuleInfo();
		if (Verifyf(pCapsuleInfo, "Mopping area requires a biped!"))
		{
			float fHeight = pCapsuleInfo->GetHeadHeight() - pCapsuleInfo->GetCapsuleZOffset();
			float fRadius = MIN(fMoppingRadius, fHeight*0.45f);
			float fPlayerRadius = pCapsuleInfo->GetRadius();
			float fCentreForwards = fPlayerRadius + fRadius - 0.01f; // So it slightly overlaps the players capsule to avoid a wall being between the two.
			Vector3 vCapsuleStart = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetB()) * fCentreForwards;
			vCapsuleStart += VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
			vCapsuleStart += ZAXIS * (pCapsuleInfo->GetCapsuleZOffset()+fRadius);
			Vector3 vCapsuleEnd = vCapsuleStart + (ZAXIS * (fHeight - (2.0f*fRadius)));

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetCapsule(vCapsuleStart, vCapsuleEnd, fRadius);
			capsuleDesc.SetDomainForTest(WorldProbe::TEST_IN_LEVEL);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetIsDirected(false);
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
			capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
			capsuleDesc.SetExcludeEntity(pPlayer);
			return !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
		}
	}
	return false;
}

int CommandGetLastNearMissVehicle(int playerIndex)
{
    int vehicleGuid = 0;
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

    if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
    {
        const CVehicle* veh = pPlayer->GetPlayerInfo()->GetLastNearMissVehicle();
        if (veh)
        {
            vehicleGuid = CTheScripts::GetGUIDFromEntity(const_cast<CVehicle&>(*veh));
        }
    }
    return vehicleGuid;
}

int CommandGetTimeSinceLastNearMiss(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
    if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
    {
        return pPlayer->GetPlayerInfo()->GetTimeSinceLastNearMiss();
    }
    return 0;
}


void CommandStartPlayerTeleport(int playerIndex, const scrVector & targetCoords, float heading, bool teleportVehicle, bool snapToGround, bool fadePlayerOut)
{
	Vector3 target = Vector3(targetCoords);

#if __DEV
	if(NetworkInterface::IsGameInProgress())
	{
		static Vector3 oldPos;
		static u32 samePosCounter = 0;
		if ((oldPos - target).Mag2() > 5.0f)
		{
			samePosCounter = 0;
		}

		if (2 < samePosCounter)
		{
			scriptWarningf("%s:START_PLAYER_TELEPORT is being called on the same position %d times.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), samePosCounter);
		}

		++samePosCounter;
		oldPos = target;
	}
#endif // __DEV

	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (pPlayer && scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		float headingInRadians = fwAngle::LimitRadianAngleSafe( DtoR * heading);
		CGameLogic::TeleportPlayer(pPlayer, target, headingInRadians, teleportVehicle, snapToGround, CWarpManager::MAX_WAIT_TIME_MS, fadePlayerOut);

		if (CWarpManager::IsActive())
		{
			CWarpManager::SetScriptThreadId(pGtaThread->GetThreadId());
		}
	}
}

bool CommandUpdatePlayerTeleport(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (!pPlayer || !scriptVerifyf(pPlayer->GetPlayerInfo(), "Player with no player info!"))
	{
		return false;
	}

	return CGameLogic::HasTeleportPlayerFinished();
}

void CommandStopPlayerTeleport()
{
	CGameLogic::StopTeleport();
}

bool CommandIsPlayerTeleportActive()
{
	return CGameLogic::IsTeleportActive();
}

float CommandGetPlayerCurrentStealthNoise(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(Verifyf(pInfo, "Expected CPlayerInfo."))
		{
			return pInfo->GetStealthNoise();
		}
	}
	return 0.0f;
}

void CommandSetPlayerHealthRechargeMultiplier(int playerIndex, float fMultiplier)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(Verifyf(pInfo, "Expected CPlayerInfo."))
		{
			pInfo->GetHealthRecharge().SetRechargeScriptMultiplier(fMultiplier);
		}
	}
}

float CommandGetPlayerHealthRechargeMaxPercent(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(scriptVerifyf(pInfo, "GET_PLAYER_HEALTH_RECHARGE_MAX_PERCENT - CPlayerInfo not found"))
		{
			return pInfo->GetHealthRecharge().GetRechargeMaxPercent();
		}
	}
	return 0.0f;
}

void CommandSetPlayerHealthRechargeMaxPercent(int playerIndex, float fPercent)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(scriptVerifyf(pInfo, "SET_PLAYER_HEALTH_RECHARGE_MAX_PERCENT - CPlayerInfo not found"))
		{
			if (scriptVerifyf(fPercent > 0.0f && fPercent <= 1.0f, "SET_PLAYER_HEALTH_RECHARGE_MAX_PERCENT - Percentage invalid, needs to be above 0.0 and less than or equal to 1.0"))
			{
				pInfo->GetHealthRecharge().SetRechargeMaxPercent(fPercent);
			}
		}
	}
}

void CommandDisablePlayerHealthRecharge(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_HEALTH_RECHARGE);
	}	
}

float CommandGetPlayerFallDistanceToTriggerRagdollOverride(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(scriptVerifyf(pInfo, "GET_PLAYER_FALL_DISTANCE_TO_TRIGGER_RAGDOLL_OVERRIDE - CPlayerInfo not found"))
		{
			return pInfo->GetFallHeightForPedOverride();
		}
	}
	return 0.0f;
}

void CommandSetPlayerFallDistanceToTriggerRagdollOverride(int playerIndex, float fDistance)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		CPlayerInfo* pInfo = pPlayer->GetPlayerInfo();
		if(scriptVerifyf(pInfo, "SET_PLAYER_FALL_DISTANCE_TO_TRIGGER_RAGDOLL_OVERRIDE - CPlayerInfo not found"))
		{
			pInfo->SetFallHeightForPedOverride(fDistance);
		}
	}
}

void CommandSetPlayerWeaponDamageModifier(int nPlayerIndex, float fWeaponDamageModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerWeaponDamageModifier(fWeaponDamageModifier);
	}
}

void CommandSetPlayerWeaponDefenseModifier(int nPlayerIndex, float fWeaponDefenseModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerWeaponDefenseModifier(fWeaponDefenseModifier);
	}
}

void CommandSetPlayerWeaponMinigunDefenseModifier(int nPlayerIndex, float fWeaponDefenseModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerWeaponMinigunDefenseModifier(fWeaponDefenseModifier);
	}
}

void CommandSetPlayerMeleeWeaponDamageModifier(int nPlayerIndex, float fMeleeWeaponDamageModifier, bool bAffectsUnarmed)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerMeleeWeaponDamageModifier(fMeleeWeaponDamageModifier);

		float fCurrentUnarmedMod = pPlayer->GetPlayerInfo()->GetPlayerMeleeUnarmedDamageModifier();
		if (fMeleeWeaponDamageModifier == 1.0f && !bAffectsUnarmed && fCurrentUnarmedMod != 1.0f)
		{
			scriptAssertf(0,"Script is resetting Melee Weapon damage modifier to 1.0f, but not previously modified Melee Unarmed damage modifier (%.2f). Is this intended?", fCurrentUnarmedMod);
#if !__FINAL
			scrThread::PrePrintStackTrace();
#endif
		}

		if (bAffectsUnarmed)
		{
			pPlayer->GetPlayerInfo()->SetPlayerMeleeUnarmedDamageModifier(fMeleeWeaponDamageModifier);
		}
	}
}

void CommandSetPlayerMeleeWeaponDefenseModifier(int nPlayerIndex, float fMeleeWeaponDefenseModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerMeleeWeaponDefenseModifier(fMeleeWeaponDefenseModifier);
	}
}

void CommandSetPlayerMeleeWeaponForceModifier(int nPlayerIndex, float fMeleeWeaponForceModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerMeleeWeaponForceModifier(fMeleeWeaponForceModifier);
	}
}

void CommandSetPlayerVehicleDamageModifier(int nPlayerIndex, float fVehicleDamageModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerVehicleDamageModifier(fVehicleDamageModifier);
	}
}

void CommandSetPlayerVehicleDefenseModifier(int nPlayerIndex, float fVehicleDefenseModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerVehicleDefenseModifier(fVehicleDefenseModifier);
	}
}

void CommandSetPlayerMaxExplosiveDamage(int nPlayerIndex, float fMaxExplosiveDamage)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerMaxExplosiveDamage(fMaxExplosiveDamage);
	}
}

void CommandSetPlayerExplosiveDamageModifier(int nPlayerIndex, float fExplosiveDamageModifier)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerExplosiveDamageModifier(fExplosiveDamageModifier);
	}
}

void CommandSetPlayerWeaponTakedownDefenseModifier(int nPlayerIndex, float fWeaponTakedownDefenseModifier)
{
	CPed * pPlayer = CTheScripts::FindNetworkPlayerPed(nPlayerIndex);
	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerWeaponTakedownDefenseModifier(fWeaponTakedownDefenseModifier);
	}
}

void CommandSetPlayerParachuteTintIndex(int playerIndex, int tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->SetTintIndexForParachute((u32)tintIndex);
	}	
}

void CommandGetPlayerParachuteTintIndex(int playerIndex, int& tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		tintIndex = (int)pPlayer->GetPedIntelligence()->GetTintIndexForParachute();
	}	
}

void CommandSetPlayerReserveParachuteTintIndex(int playerIndex, int tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->SetTintIndexForReserveParachute((u32)tintIndex);
	}	
}

void CommandGetPlayerReserveParachuteTintIndex(int playerIndex, int& tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		tintIndex = (int)pPlayer->GetPedIntelligence()->GetTintIndexForReserveParachute();
	}	
}

void CommandSetPlayerParachutePackTintIndex(int playerIndex, int tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->SetTintIndexForParachutePack((u32)tintIndex);
	}	
}

void CommandGetPlayerParachutePackTintIndex(int playerIndex, int& tintIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		tintIndex = (int)pPlayer->GetPedIntelligence()->GetTintIndexForParachutePack();
	}	
}

void CommandSetPlayerHasReserveParachute(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if(pPlayer)
	{
		//if(Verifyf(CTaskParachute::DoesPedHaveParachute(*pPlayer), "Ped should already have a parachute in inventory before adding a reserve!"))
		//{
			pPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute, true); 
		//}
	}	
}

bool CommandGetPlayerHasReserveParachute(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if(pPlayer)
	{
		return pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute);
	}	

	return false;
}

void CommandSetPlayerCanLeaveParachuteSmokeTrail(int playerIndex, bool bCanLeaveParachuteSmokeTrail)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetCanLeaveParachuteSmokeTrail(bCanLeaveParachuteSmokeTrail);
	}	
}

void CommandSetPlayerParachuteSmokeTrailColor(int playerIndex, int r, int g, int b)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetParachuteSmokeTrailColor(Color32(r, g, b));
	}	
}

void CommandGetPlayerParachuteSmokeTrailColor(int playerIndex, int& r, int& g, int& b)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		Color32 col = pPlayer->GetPlayerInfo()->GetParachuteSmokeTrailColor();
		r = col.GetRed();
		g = col.GetGreen();
		b = col.GetBlue();
	}	
}

int CommandGetPlayerPhonePaletteIdx(int playerIndex)
{
	int res = -1;
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		res = pPlayer->GetPedPhonePaletteIdx();
	}

	return res;
}

void CommandSetPlayerPhonePaletteIdx(int playerIndex, int idx)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->SetPedPhonePaletteIdx(idx);
	}
}

void CommandSetDisplayPlayerDamageInfo(bool PEDDEBUGVISUALISER_ONLY(bDisplay))
{
#if PEDDEBUGVISUALISER
	CPedDebugVisualiserMenu::SetDisplayingOnlyForPlayers(bDisplay);
	CPedDebugVisualiser::DisplayPedDamageRecords(bDisplay);
#endif // PEDDEBUGVISUALISER
}

void CommandSimulatePlayerInputGait(int iPlayerIndex, float fMoveBlendRatio, int fTimer, float fHeading, bool bUseRelativeHeading, bool bNoInputInterruption)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetSimulateGaitInput(fMoveBlendRatio, ((float)fTimer)/1000.0f, fHeading*DtoR, bUseRelativeHeading, bNoInputInterruption);
	}
}

void CommandResetPlayerInputGait(int iPlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->ResetSimulateGaitInput();
	}
}

void CommandSetAutoGiveParachuteWhenEnterPlane(int iPlayerIndex, bool bGivePlayerParachute)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetAutoGiveParachuteWhenEnterPlane(bGivePlayerParachute);
	}
}

void CommandSetAutoGiveScubaGearWhenExitVehicle(int iPlayerIndex, bool bGivePlayerScubaGear)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetAutoGiveScubaGearWhenExitVehicle(bGivePlayerScubaGear);
	}
}

void CommandSetPlayerStealthPerceptionModifier(int iPlayerIndex, float StealthPerceptionModifier)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetStealthPerceptionModifier(StealthPerceptionModifier);
	}
}

float CommandGetPlayerStealthPerceptionModifier(int iPlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		return pPlayer->GetPlayerInfo()->GetStealthPerceptionModifier();
	}

	return 1.0f;
}

void CommandSetPlayerToUseCoverThreatWeighting(int iPlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(iPlayerIndex);

	if (pPlayer)
	{
		pPlayer->SetPlayerResetFlag(CPlayerResetFlags::PRF_USE_COVER_THREAT_WEIGHTING);
	}
}

bool CommandIsRemotePlayerInNonClonedVehicle(int iPlayerIndex)
{
    CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(iPlayerIndex);

	if (pPlayerPed)
	{
        return NetworkInterface::IsRemotePlayerInNonClonedVehicle(pPlayerPed);
	}

	return false;
}

void CommandIncreasePlayerJumpSuppressionRange(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_INCREASE_JUMP_SUPPRESSION_RANGE);
	}
}

void CommandsSetPlayerResetFlagPreferRearSeats(int PlayerIndex, int VehicleIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer && pPlayer->GetPlayerInfo())
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_PREFER_REAR_SEATS);
		const CVehicle* pVehicleToPrefer = NULL;
		if (VehicleIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			pVehicleToPrefer = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);
		}
		pPlayer->GetPlayerInfo()->SetVehiclePlayerPreferRearSeat(pVehicleToPrefer);
	}
}

void CommandsSetPlayerResetFlagPreferFrontPassengerSeat(int PlayerIndex, int VehicleIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer && pPlayer->GetPlayerInfo())
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_PREFER_FRONT_PASSENGER_SEAT);
		const CVehicle* pVehicleToPrefer = NULL;
		if (VehicleIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			pVehicleToPrefer = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);
		}
		pPlayer->GetPlayerInfo()->SetVehiclePlayerPreferFrontPassengerSeat(pVehicleToPrefer);
	}
}

void CommandSetPlayerSimulateAiming(int iPlayerIndex, bool bSimulateAiming)
{
	CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(iPlayerIndex);

	if (pPlayerPed)
	{
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SimulatingAiming, bSimulateAiming );
	}
}

bool CommandHasPlayerBeenSpottedInStolenVehicle(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		return pPlayer->GetPlayerInfo()->HasBeenSpottedInStolenVehicle();
	}

	return false;
}

int CommandGetSpotterOfPlayerInStolenVehicle(int PlayerIndex)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		CPed* pSpotter = pPlayer->GetPlayerInfo()->GetSpotterOfStolenVehicle();

		return pSpotter ? CTheScripts::GetGUIDFromEntity(*pSpotter) : 0;
	}

	return 0;
}

bool CommandIsPlayerBattleAware(int PlayerIndex)
{
	CPed const* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	
	if (pPlayer)
	{
		if(pPlayer->IsLocalPlayer())
		{
			return pPlayer->GetPedIntelligence()->IsBattleAware();
		}
		else if(NetworkInterface::IsGameInProgress())
		{
			CNetObjPlayer const* netObjPlayer = static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject());
		
			if(netObjPlayer)
			{
				return netObjPlayer->IsBattleAware();
			}
		}
	}

	return false;
}

bool CommandPlayerReceivedBattleEventRecently(int PlayerIndex, int nTime, bool bIncludeLocalEvents)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		u32 uLastBattleEvent = pPlayer->GetPedIntelligence()->GetLastBattleEventTime(bIncludeLocalEvents);
		if(fwTimer::GetTimeInMilliseconds() < (uLastBattleEvent + (u32)nTime))
		{
			return true;
		}
	}

	return false;
}

void CommandExtendWorldBoundaryForPlayer(const scrVector & scrVecCoors)
{
	CPlayerInfo::ExtendWorldBoundary(scrVecCoors);
}

void CommandResetWorldBoundaryForPlayer()
{
	CPlayerInfo::ResetWorldBoundary();
}

bool CommandIsPlayerRidingTrain(int PlayerIndex)
{
	const CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if(pPlayer)
	{
		CTask* pTask = pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
		if(pTask && (pTask->GetState() == CTaskPlayerOnFoot::STATE_RIDE_TRAIN))
		{
			return true;
		}
	}

	return false;
}

bool CommandHasPlayerLeftTheWorld(int PlayerIndex)
{
	const CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo() && pPlayer->GetPlayerInfo()->HasPlayerLeftTheWorld())
	{
		return true;
	}

	return false;
}

void CommandDisableDispatchHeliRefueling(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DISABLE_DISPATCHED_HELI_REFUEL);
	}
}

void CommandDisableHeliDestroyedDispatchSpawnDelay(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DISABLE_DISPATCHED_HELI_DESTROYED_SPAWN_DELAY);
	}
}

void CommandSetPlayerLeavePedBehind(int PlayerIndex, bool bLeavePedBehind)
{
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->SetPlayerLeavePedBehind(bLeavePedBehind);
	}
}

void CommandSetPlayerParachuteVariationOverride( int PlayerIndex, int ComponentID, int DrawableID, int TexID, int AltDrawableID)
{
	CPed const* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{	
		if (NetworkInterface::IsGameInProgress() && DrawableID == 0)
		{
			scriptAssertf(0, "SET_PLAYER_PARACHUTE_VARIATION_OVERRIDE - DrawableID is 0, preventing. This will cause an infinite parachute take-off loop.");
		}
		else
		{
			pPlayer->GetPlayerInfo()->SetPedParachuteVariationOverride(static_cast<ePedVarComp>(ComponentID), DrawableID, TexID, AltDrawableID);
		}
	}
}

void CommandClearPlayerParachuteVariationOverride( int PlayerIndex )
{
	CPed const* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->ClearPedParachuteVariationOverride();
	}
}

void CommandSetPlayerParachuteModelOverride( int PlayerIndex, int ModelNameHash)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedParachuteModelOverride((u32)ModelNameHash);
	}
}

void CommandSetPlayerReserveParachuteModelOverride(int PlayerIndex, int ModelNameHash)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedReserveParachuteModelOverride((u32)ModelNameHash);
	}
}

u32 CommandGetPlayerParachuteModelOverride(int PlayerIndex)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		return pPlayer->GetPlayerInfo()->GetPedParachuteModelOverride();
	}

	return 0;
}

u32 CommandGetPlayerReserveParachuteModelOverride(int PlayerIndex)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer && pPlayer->GetPlayerInfo())
	{
		return pPlayer->GetPlayerInfo()->GetPedReserveParachuteModelOverride();
	}

	return 0;
}

void CommandClearPlayerParachuteModelOverride( int PlayerIndex )
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedParachuteModelOverride(0);
	}
}

void CommandClearPlayerReserveParachuteModelOverride(int PlayerIndex)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedReserveParachuteModelOverride(0);
	}
}

void CommandSetPlayerParachutePackModelOverride( int PlayerIndex, int ModelNameHash)
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedParachutePackModelOverride((u32)ModelNameHash);
	}
}

void CommandClearPlayerParachutePackModelOverride( int PlayerIndex )
{
	CPed const* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		pPlayer->GetPlayerInfo()->SetPedParachutePackModelOverride(0);
	}
}

void CommandDisablePlayerVehicleRewards(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DISABLE_VEHICLE_REWARDS);
	}
}

void CommandEnableBlueToothHeadSet(int PlayerIndex, bool b)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		pPlayer->GetPlayerInfo()->SetEnableBlueToothHeadset(b);
	}
}

bool CommandGetEnableBlueToothHeadSet(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{	
		return pPlayer->GetPlayerInfo()->GetEnableBlueToothHeadset();
	}
	
	return false;
}

void CommandDisableCameraViewModeCycle(int playerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(playerIndex);
	if(pPlayer)
	{
		pPlayer->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}	
}

int CommandGetPlayerFakeWantedLevel(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

	if (pPlayer && pPlayer->IsPlayer())
	{	
		if (NetworkInterface::IsGameInProgress())
		{
			//MP
			if (pPlayer->GetNetworkObject())
			{
				CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPlayer->GetNetworkObject());
				if (pNetObjPlayer)
				{
					return pNetObjPlayer->GetRemoteFakeWantedLevel();
				}
			}
		}
		else
		{
			//SP
			return CScriptHud::iFakeWantedLevel;
		}
	}

	return 0;
}

void CommandSetPlayerCanDamagePlayer(int PlayerIndex, int TargetPlayerIndex, bool bAllowDamageFlag)
{
	CPed *pTargetPlayerPed = CTheScripts::FindNetworkPlayerPed(TargetPlayerIndex, false);

	if (pTargetPlayerPed 
		&& SCRIPT_VERIFY(pTargetPlayerPed->GetNetworkObject(), "SET_PLAYER_CAN_DAMAGE_PLAYER - Can only be used during a network game")
		&& SCRIPT_VERIFY(PlayerIndex >= 0 && PlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "SET_PLAYER_CAN_DAMAGE_PLAYER - Invalid Player")
		&& SCRIPT_VERIFY(TargetPlayerIndex >= 0 && TargetPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "SET_PLAYER_CAN_DAMAGE_PLAYER - Invalid Other Player"))
	{
		CNetObjPed* pTargetPedObj = SafeCast(CNetObjPed, pTargetPlayerPed->GetNetworkObject());

		if (pTargetPedObj)
		{
			pTargetPedObj->SetIsDamagableByPlayer(PlayerIndex, bAllowDamageFlag);
		}
	}
}

void CommandSetApplyWaypointOfPlayer(int PlayerIndex, int color)
{
	if(PlayerIndex == -1)
	{
		SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject())->SetWaypointOverrideOwner(NETWORK_INVALID_OBJECT_ID);
	}
	else
	{
		CPed* playerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex, false);
		if(playerPed && playerPed->GetNetworkObject())
		{
			SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject())->SetWaypointOverrideColor(static_cast<eHUD_COLOURS>(color));
			SafeCast(CNetObjPlayer, NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetNetworkObject())->SetWaypointOverrideOwner(playerPed->GetNetworkObject()->GetObjectID());
		}
	}
}

bool CommandIsPlayerVehicleWeaponToggledToNonHoming(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		const CWeaponInfo* pWeaponInfo = pPlayer->GetEquippedWeaponInfo();
		if (pWeaponInfo && pWeaponInfo->GetIsVehicleWeapon() && pWeaponInfo->GetCanHomingToggle())
		{
			CPlayerPedTargeting& rPlayerTargeting = pPlayer->GetPlayerInfo()->GetTargeting();
			return !rPlayerTargeting.GetVehicleHomingEnabled();
		}
	}
	
	return false;
}

void CommandSetPlayerVehicleWeaponToNonHoming(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		const CWeaponInfo* pWeaponInfo = pPlayer->GetEquippedWeaponInfo();
		if (pWeaponInfo && pWeaponInfo->GetIsVehicleWeapon() && pWeaponInfo->GetCanHomingToggle())
		{
			CPlayerPedTargeting& rPlayerTargeting = pPlayer->GetPlayerInfo()->GetTargeting();
			rPlayerTargeting.SetVehicleHomingEnabled(false);
		}
	}
}

void CommandSetPlayerHomingDisabledForAllVehicleWeapons(int PlayerIndex, bool bValue)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);

	if (pPlayer)
	{
		pPlayer->GetPlayerInfo()->GetTargeting().SetVehicleHomingForceDisabled(bValue);

		// If our current weapon is a homing weapon, set this flag immediately. Otherwise, it'll be set correctly when we try to change vehicle weapon.
		const CWeaponInfo* pWeaponInfo = pPlayer->GetEquippedWeaponInfo();
		if (pWeaponInfo && pWeaponInfo->GetIsVehicleWeapon() && pWeaponInfo->GetIsHoming())
		{
			pPlayer->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(!bValue);
		}
	}
}

void CommandAddPlayerTargetableEntity(int PlayerIndex, int TargetEntityIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(TargetEntityIndex);
		if (pEntity && pPlayer != pEntity)
		{
			CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
			if (pPlayerInfo)
			{
				pPlayerInfo->GetTargeting().AddTargetableEntity(pEntity);
			}
		}
	}
}

void CommandRemovePlayerTargetableEntity(int PlayerIndex, int TargetEntityIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(TargetEntityIndex);
		if (pEntity)
		{
			CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
			if (pPlayerInfo)
			{
				pPlayerInfo->GetTargeting().RemoveTargetableEntity(pEntity);
			}
		}
	}
}

void CommandClearPlayerTargetableEntities(int PlayerIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if (pPlayerInfo)
		{
			pPlayerInfo->GetTargeting().ClearTargetableEntities();
		}
	}
}

bool CommandGetIsEntityInTargetableByPlayerArray(int PlayerIndex, int TargetEntityIndex)
{
	CPed* pPlayer = CTheScripts::FindLocalPlayerPed(PlayerIndex);
	if (pPlayer)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(TargetEntityIndex);
		if (pEntity)
		{
			CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
			if (pPlayerInfo && pPlayerInfo->GetTargeting().GetIsEntityAlreadyRegisteredInTargetableArray(pEntity))
			{
				return true;
			}
		}
	}

	return false;
}

void CommandSetPlayerPreviousVariationData(int PlayerIndex, int ComponentID, int DrawableID, int AltDrawableID, int TextureID, int PaletteID)
{
	CPed const* pPlayer = CTheScripts::FindNetworkPlayerPed(PlayerIndex);
	if(pPlayer && pPlayer->GetPlayerInfo())
	{	
		pPlayer->GetPlayerInfo()->SetPreviousVariationData(static_cast<ePedVarComp>(ComponentID), DrawableID, AltDrawableID, TextureID, PaletteID);
	}
}

void CommandRemoveScriptFirePosition()
{
	if (CPed* pPed = CGameWorld::FindLocalPlayer())
	{
		if (CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo())
		{
			pPlayerInfo->ClearScriptedWeaponFiringPos();
		}
	}
}
	
void CommandSetScriptFirePosition(const scrVector & targetCoords)
{
	if (CPed* pPed = CGameWorld::FindLocalPlayer())
	{
		if (CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo())
		{
			Vector3 target = Vector3(targetCoords);
			pPlayerInfo->SetScriptedWeaponFiringPos(VECTOR3_TO_VEC3V(target));
		}
	}
}

void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE_HONEYPOT(GET_PLAYER_PED,0x407e03586628e458,							CommandGetPlayerPed							);					//	NATIVE PROC GET_PLAYER_PED (PLAYERID PLAYER_IN, CHARID &CHAR_OUT)
		SCR_REGISTER_SECURE(GET_PLAYER_PED_SCRIPT_INDEX,0xe0fcabb9fcdf25ed,						CommandGetPlayerScriptGuid					);
		SCR_REGISTER_SECURE(SET_PLAYER_MODEL,0xbece2b5c03bb4f09,								CommandChangePlayerModel					);
        SCR_REGISTER_SECURE(CHANGE_PLAYER_PED,0xb10d8e27c549787d,								CommandChangePlayerPed  					);
		SCR_REGISTER_UNUSED(NETWORK_PLAYER_HAS_PED,0xa49d19d03dcc3578,							CommandPlayerHasPed							);					//	NATIVE FUNC BOOL PLAYER_HAS_CHAR (PLAYERID PLAYER_IN)
		SCR_REGISTER_SECURE(GET_PLAYER_RGB_COLOUR,0xea0963c9e116ac55,							CommandGetPlayerRGBColour                   );					//	NATIVE PROC INT GET_PLAYER_RGB_COLOUR( PLAYER_INDEX PlayerIndex, INT &Red, INT &Green, INT &Blue )
		SCR_REGISTER_UNUSED(GET_NATIVE_PLAYER_HUD_COLOUR,0x9f53470fbc61099d,					CommandGetPlayerHUDColour                   );					//	NATIVE PROC HUD_COLOURS GET_PLAYER_HUD_COLOUR( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE_HONEYPOT(GET_NUMBER_OF_PLAYERS,0x3a0de5c10c341c80,					CommandGetNumberOfPlayers                   );                  //  NATIVE FUNC INT GET_NUM_PLAYERS()
		SCR_REGISTER_SECURE(GET_PLAYER_TEAM,0x0e40f846a70ba3ec,									CommandGetPlayerTeam						);					//	NATIVE FUNC INT GET_PLAYER_TEAM( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE(SET_PLAYER_TEAM,0x78a9f1d1065c4165,									CommandSetPlayerTeam						);					//	NATIVE FUNC INT SET_PLAYER_TEAM( PLAYER_INDEX PlayerIndex, INT Team )
		SCR_REGISTER_SECURE(GET_NUMBER_OF_PLAYERS_IN_TEAM,0xe9cb4976f6bdca99,					CommandGetNumPlayersInTeam					);                  //	NATIVE FUNC INT GET_NUMBER_OF_PLAYERS_IN_TEAM( INT Team )
		SCR_REGISTER_SECURE(GET_PLAYER_NAME,0x61042ca2a97bbb69,									CommandGetPlayerName						);					//	NATIVE FUNC INT GET_PLAYER_TEAM( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE(GET_WANTED_LEVEL_RADIUS,0xe923e49e6f7d32e2,							CommandGetWantedLevelRadius					);
		SCR_REGISTER_SECURE(GET_PLAYER_WANTED_CENTRE_POSITION,0x0e218e8a8c75638d,				CommandGetWantedRadiusCentre				);
		SCR_REGISTER_SECURE(SET_PLAYER_WANTED_CENTRE_POSITION,0x1eae781eaa7f5613,				CommandSetWantedRadiusCentre				);
		SCR_REGISTER_SECURE(GET_WANTED_LEVEL_THRESHOLD,0x1ac3ecc14fa9cd77,						CommandGetWantedLevelThreshold				);
		SCR_REGISTER_SECURE_HONEYPOT(SET_PLAYER_WANTED_LEVEL,0xbcd99b4edae55be6,				CommandAlterWantedLevel						);					//	NATIVE PROC ALTER_WANTED_LEVEL( INT PlayerIndex, INT WantedLevel )
		SCR_REGISTER_SECURE(SET_PLAYER_WANTED_LEVEL_NO_DROP,0x007d28b99df625b1,					CommandAlterWantedLevelNoDrop				);					//	NATIVE PROC SET_PLAYER_WANTED_LEVEL_NO_DROP( INT PlayerIndex, INT WantedLevel )
		SCR_REGISTER_SECURE(SET_PLAYER_WANTED_LEVEL_NOW,0xb78cbe6c9550e5ef,						CommandApplyWantedLevelChangeNow			);					//	NATIVE PROC SET_PLAYER_WANTED_LEVEL_NOW( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE(ARE_PLAYER_FLASHING_STARS_ABOUT_TO_DROP,0x5e4795798d5a9125,			CommandPlayerHasFlashingStarsAboutToDrop	);
		SCR_REGISTER_SECURE(ARE_PLAYER_STARS_GREYED_OUT,0xea9f0d44b6d1c2d6,						CommandPlayerHasGreyedOutStars				);
		SCR_REGISTER_SECURE(IS_WANTED_AND_HAS_BEEN_SEEN_BY_COPS,0xe6f58896ab252846,				IsWantedAndHasBeenSeenByCops				);
		SCR_REGISTER_SECURE(SET_DISPATCH_COPS_FOR_PLAYER,0xf0ca50ac5d6cfd41,					CommandSetDispatchCopsForPlayer				);
		SCR_REGISTER_SECURE(IS_PLAYER_WANTED_LEVEL_GREATER,0x37bbf9acd752871a,					CommandIsWantedLevelGreater					);					//	NATIVE PROC IS_PLAYER_WANTED_LEVEL_GREATER( INT PlayerIndex, INT WantedLevel )
		SCR_REGISTER_SECURE(CLEAR_PLAYER_WANTED_LEVEL,0xba5d7196eacb9282,						CommandClearWantedLevel						);					//	NATIVE PROC CLEAR_PLAYER_WANTED_LEVEL( INT PlayerIndex )
		SCR_REGISTER_UNUSED(LOCAL_PLAYER_HAS_PENDING_CRIMES,0x345ecf3f6b5bda08,				CommandLocalPlayerHasPendingCrimes			);					//  NATIVE PROC LOCAL_PLAYER_HAS_PENDING_CRIMES()
		SCR_REGISTER_UNUSED(GET_INDEX_OF_PLAYER_THAT_CAUSED_WL,0xe3c9c6ab84bb6fab,				CommandGetIndexOfPlayerThatCausedWL			);					//	NATIVE FUNC INT GET_INDEX_OF_PLAYER_THAT_CAUSED_WL()
		SCR_REGISTER_SECURE(IS_PLAYER_DEAD,0xe08d4560995e7946,									CommandIsPlayerDead							);					//	NATIVE PROC IS_PLAYER_DEAD( INT PlayerIndex )
		SCR_REGISTER_SECURE(IS_PLAYER_PRESSING_HORN,0x98d590fb7ba7df44,							CommandIsPlayerPressingHorn					);					//	NATIVE PROC IS_PLAYER_PRESSING_HORN( INT PlayerIndex )
		SCR_REGISTER_SECURE(SET_PLAYER_CONTROL,0x64e21045781afbc9,								CommandSetPlayerControl						);					//	NATIVE PROC SET_PLAYER_CONTROL( INT PlayerIndex, BOOL SetControl )
//		SCR_REGISTER_UNUSED(SET_PLAYER_CONTROL_ADVANCED, 0x4261bf11,							CommandSetPlayerControlAdvanced				);					//	NATIVE PROC SET_PLAYER_CONTROL( INT PlayerIndex, BOOL SetControl )
		SCR_REGISTER_SECURE(GET_PLAYER_WANTED_LEVEL,0xc378b7f0d175a5b0,							CommandGetPlayerWantedLevel					);					//	NATIVE PROC STORE_WANTED_LEVEL( INT PlayerIndex, INT& WantedLevel )
		SCR_REGISTER_SECURE(SET_MAX_WANTED_LEVEL,0xc0f70a3cdec87ece,							CommandSetMaxWantedLevel					);					//	NATIVE PROC SET_MAX_WANTED_LEVEL( INT PlayerIndex )
		SCR_REGISTER_SECURE(SET_POLICE_RADAR_BLIPS,0x6eac3a35d1bb9b9f,							CommandSetPoliceRadarBlips					);
		SCR_REGISTER_SECURE(SET_POLICE_IGNORE_PLAYER,0xb5b3aae149fc278d,						CommandSetPoliceIgnorePlayer				);					//	NATIVE PROC SET_POLICE_IGNORE_PLAYER( INT PlayerIndex, BOOL IgnorePlayer )
		SCR_REGISTER_SECURE(IS_PLAYER_PLAYING,0x3583a42587543334,								CommandIsPlayerPlaying						);					//	NATIVE PROC IS_PLAYER_PLAYING( INT PlayerIndex )
		SCR_REGISTER_SECURE(SET_EVERYONE_IGNORE_PLAYER,0xac58ba7d19452704,						CommandSetEveryoneIgnorePlayer				);					//	NATIVE PROC SET_EVERYONE_IGNORE_PLAYER( INT PlayerIndex, BOOL IgnorePlayer )
	
		SCR_REGISTER_SECURE(SET_ALL_RANDOM_PEDS_FLEE,0x7a8df8c7511be48a,						CommandSetAllRandomPedsFlee					);									// DEPRECTATED
		SCR_REGISTER_SECURE(SET_ALL_RANDOM_PEDS_FLEE_THIS_FRAME,0xfecbe8ed0e1259a5,				CommandSetAllRandomPedsFleeThisFrame);
		SCR_REGISTER_SECURE(SET_ALL_NEUTRAL_RANDOM_PEDS_FLEE,0x7f9fba1d5bb2ca14,				CommandSetAllNeutralRandomPedsFlee			);					// DEPRECATED
		SCR_REGISTER_SECURE(SET_ALL_NEUTRAL_RANDOM_PEDS_FLEE_THIS_FRAME,0x710e43300e497e2a,		CommandSetAllNeutralRandomPedsFleeThisFrame			);
		SCR_REGISTER_UNUSED(SET_RANDOM_PEDS_ONLY_ATTACK_WITH_GUNS,0xc466b2cc66f5e0d4,			CommandSetRandomPedsOnlyAttackWithGuns		);		// DEPRECATED
		SCR_REGISTER_UNUSED(SET_RANDOM_PEDS_ONLY_ATTACK_WITH_GUNS_THIS_FRAME,0xd971e122e85dc9eb,			CommandSetRandomPedsOnlyAttackWithGunsThisFrame		);
		SCR_REGISTER_SECURE(SET_LAW_PEDS_CAN_ATTACK_NON_WANTED_PLAYER_THIS_FRAME,0xef2c918f9b332bfd, CommandSetLawPedsCanAttackNonWantedPlayerThisFrame);

		SCR_REGISTER_SECURE(SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS,0xec26897438239334,			CommandSetIgnoreLowPriorityShockingEvents	);					//	NATIVE PROC SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS( INT PlayerIndex, BOOL IgnorePlayer )
		SCR_REGISTER_SECURE(SET_WANTED_LEVEL_MULTIPLIER,0x8576657a629c4b0a,						CommandSetWantedMultiplier					);					//	NATIVE PROC SET_WANTED_LEVEL_MULTIPLIER( FLOAT Multiplier )
		SCR_REGISTER_SECURE(SET_WANTED_LEVEL_DIFFICULTY,0x48a2ec087250e1bc,						CommandSetWantedLevelDifficulty				);
		SCR_REGISTER_SECURE(RESET_WANTED_LEVEL_DIFFICULTY,0x6e31398aa6a7c5ce,					CommandResetWantedLevelDifficulty			);
		SCR_REGISTER_SECURE(GET_WANTED_LEVEL_TIME_TO_ESCAPE,0xcb57195468bc75e4,					CommandGetWantedLevelTimeToEscape			);					//	NATIVE FUNC GET_WANTED_LEVE_TIME_TO_ESCAPE( )
		SCR_REGISTER_SECURE(SET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME,0x439fce01cc242689,			CommandSetWantedLevelHiddenEscapeTime		); 
		SCR_REGISTER_SECURE(RESET_WANTED_LEVEL_HIDDEN_ESCAPE_TIME,0x1c1af50a180090aa,			CommandResetWantedLevelHiddenEscapeTime		);
		SCR_REGISTER_UNUSED(SET_ARCADE_CNC_V_OFFENDER,0x3464edd4c4fd9610,						CommandSetArcadeCNCVOffender				);
		SCR_REGISTER_UNUSED(GET_ARCADE_CNC_V_OFFENDER,0xfb53be1acc57eb56,						CommandGetArcadeCNCVOffender				);
		SCR_REGISTER_SECURE(START_FIRING_AMNESTY,0x3b97c4414e778c45,							CommandStartFiringAmnesty					);
		SCR_REGISTER_SECURE(REPORT_CRIME,0xda63aad3559a7fb5,									CommandReportCrime							);
		SCR_REGISTER_SECURE(SUPPRESS_CRIME_THIS_FRAME,0x01e6fd194dcbeb7d,						CommandSuppressCrimeThisFrame				);
		SCR_REGISTER_SECURE(UPDATE_WANTED_POSITION_THIS_FRAME,0xb2cc2a0592c4efcf,				CommandUpdateWantedPositionThisFrame		);
		SCR_REGISTER_SECURE(SUPPRESS_LOSING_WANTED_LEVEL_IF_HIDDEN_THIS_FRAME,0xabb7d34a8925c872,		CommandSuppressLosingWantedLevelIfHiddenThisFrame		);
		SCR_REGISTER_SECURE(ALLOW_EVASION_HUD_IF_DISABLING_HIDDEN_EVASION_THIS_FRAME,0x02affbcc3b11c52a,	CommandmAllowEvasionHUDIfDisablingHiddenEvasionThisFrame );
		SCR_REGISTER_SECURE(FORCE_START_HIDDEN_EVASION,0x894e9c42b8740f5e,						CommandForceStartHiddenEvasion				);
		SCR_REGISTER_SECURE(SUPPRESS_WITNESSES_CALLING_POLICE_THIS_FRAME,0x11a68a2ddd8cd574,	CommandSuppressWitnessesCallingPoliceThisFrame		);
		SCR_REGISTER_UNUSED(ENABLE_CNC_STYLE_CRIME_REPORTING,0x443c2153a70d4895,				CommandEnableCnCCrimeReporting				);
		SCR_REGISTER_SECURE(REPORT_POLICE_SPOTTED_PLAYER,0xec1fa12f2050bd84,					CommandReportPoliceSpottedPlayer			);
		SCR_REGISTER_SECURE(SET_LAW_RESPONSE_DELAY_OVERRIDE,0xe831cbc76e6851d3,					CommandSetLawResponseDelayOverride			);
		SCR_REGISTER_SECURE(RESET_LAW_RESPONSE_DELAY_OVERRIDE,0x72555a32a7f6481e,				CommandResetLawResponseDelayOverride		);
		SCR_REGISTER_SECURE(CAN_PLAYER_START_MISSION,0x4965181a7de78866,						CommandCanPlayerStartMission				);					//	NATIVE PROC CAN_PLAYER_START_MISSION( INT PlayerIndex )
		SCR_REGISTER_UNUSED(CAN_PLAYER_DO_STEALTH_KILL,0x6a367975aa530ddb,						CommandCanPlayerDoStealthKill				);
		SCR_REGISTER_UNUSED(SET_PLAYER_SAFE_FOR_CUTSCENE,0xf7ff5edcd2c90b51,					CommandMakePlayerSafeForCutscene			);					//	NATIVE PROC SET_PLAYER_SAFE_FOR_CUTSCENE( INT PlayerIndex )
		SCR_REGISTER_SECURE(IS_PLAYER_READY_FOR_CUTSCENE,0x682283641e48e60d,					CommandIsPlayerReadyForCutscene				);					//	NATIVE FUNC BOOL IS_PLAYER_READY_FOR_CUTSCENE( INT PlayerIndex )
		SCR_REGISTER_SECURE(IS_PLAYER_TARGETTING_ENTITY,0x8bf92a6d56a911eb,						CommandIsPlayerTargettingEntity				);					//	NATIVE PROC IS_PLAYER_TARGETTING_CHAR( INT PlayerIndex, INT CharIndex )
		SCR_REGISTER_SECURE(GET_PLAYER_TARGET_ENTITY,0x269c23e72e54a04e,						CommandGetPlayerTargetEntity				);
		SCR_REGISTER_SECURE_HONEYPOT(IS_PLAYER_FREE_AIMING,0x55e78b6aafef4eb8,							CommandIsPlayerFreeAiming					);					//	NATIVE PROC IS_PLAYER_FREE_AIMING( INT PlayerIndex )
		SCR_REGISTER_SECURE(IS_PLAYER_FREE_AIMING_AT_ENTITY,0x24e6c39aac2154d0,					CommandIsPlayerFreeAimingAtEntity			);					//	NATIVE PROC IS_PLAYER_FREE_AIMING_AT_PED( INT PlayerIndex, INT CharIndex )
		SCR_REGISTER_SECURE(GET_ENTITY_PLAYER_IS_FREE_AIMING_AT,0xfd48978e4e1baa06,				CommandGetEntityPlayerIsFreeAimingAt		);
		SCR_REGISTER_SECURE(SET_PLAYER_LOCKON_RANGE_OVERRIDE,0x37f5f63782291e3b,				CommandSetPlayerLockOnRangeOverride			);					//	NATIVE PROC SET_PLAYER_LOCKON_RANGE_OVERRIDE( INT PlayerIndex, FLOAT Range )
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_DO_DRIVE_BY,0xf82507ebd42c6070,						CommandSetPlayerCanDoDriveBy				);					//	NATIVE PROC SET_PLAYER_CAN_DO_DRIVE_BY( INT PlayerIndex, BOOL CanDoDriveBy )
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_BE_HASSLED_BY_GANGS,0x9c7e01e1a0eaa797,				CommandSetPlayerCanBeHassledByGangs			);
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_USE_COVER,0x8830bfbe22c483c8,						CommandSetPlayerCanUseCover					);					//	NATIVE PROC SET_PLAYER_CAN_USE_COVER( INT PlayerIndex, BOOL CanDoDriveBy )
		SCR_REGISTER_SECURE(GET_MAX_WANTED_LEVEL,0x847ba8d143d888d1,							CommandGetMaxWantedLevel					);					//	NATIVE PROC GET_MAX_WANTED_LEVEL( INT& WantedLevel )
		SCR_REGISTER_SECURE(IS_PLAYER_TARGETTING_ANYTHING,0x20f5a2db460ab9bb,					CommandIsPlayerTargettingAnything			);					//	NATIVE PROC IS_PLAYER_TARGETTING_ANYTHING( INT PlayerIndex )
		SCR_REGISTER_SECURE(SET_PLAYER_SPRINT,0xb7baa0aa99553c94,								CommandSetPlayerSprint						);					//	NATIVE PROC DISABLE_PLAYER_SPRINT( INT PlayerIndex, bool DisableSprint )
		SCR_REGISTER_SECURE(RESET_PLAYER_STAMINA,0x511842c558980f8a,					        CommandPlayerResetSprintStamina				);
		SCR_REGISTER_SECURE(RESTORE_PLAYER_STAMINA,0x699b5067386de80c,							CommandPlayerRestoreSprintStamina			);
		SCR_REGISTER_SECURE(GET_PLAYER_SPRINT_STAMINA_REMAINING,0xfdb293b712306c65,				CommandGetPlayerSprintStaminaRemaining		);
		SCR_REGISTER_SECURE(GET_PLAYER_SPRINT_TIME_REMAINING,0x7fb7b029eb5d5a31,				CommandGetPlayerSprintTimeRemaining			);					//	NATIVE FUNC FLOAT GET_PLAYER_SPRINT_TIME_REMAINING( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE(GET_PLAYER_UNDERWATER_TIME_REMAINING,0x8d856ff63e29934b,			CommandGetPlayerUnderwaterTimeRemaining		);					//	NATIVE FUNC FLOAT GET_PLAYER_UNDERWATER_TIME_REMAINING( PLAYER_INDEX PlayerIndex )		
		SCR_REGISTER_SECURE(SET_PLAYER_UNDERWATER_BREATH_PERCENT_REMAINING,0xaa17f3802978cc19,	CommandPlayerSetUnderwaterBreathingPercentRemaining		);		//	NATIVE FUNC FLOAT SET_PLAYER_UNDERWATER_BREATH_PERCENT_REMAINING( PLAYER_INDEX PlayerIndex, FLOAT fPercent )
		SCR_REGISTER_SECURE(GET_PLAYER_GROUP,0xfd8e434495ec2f26,								CommandGetPlayerGroup						);					//	NATIVE PROC GET_PLAYER_GROUP( INT PlayerIndex, INT& GroupIndex )
		SCR_REGISTER_SECURE(GET_PLAYER_MAX_ARMOUR,0x57a3e5a15723f6ca,							CommandGetPlayerMaxArmour					);					//	NATIVE PROC GET_PLAYER_MAX_ARMOUR(INT PlayerIndex, INT& MaxArmour )
		SCR_REGISTER_UNUSED(GET_PLAYER_JACK_SPEED,0x881f0bb5bb902a7e,							CommandGetPlayerJackSpeed					);		
		SCR_REGISTER_SECURE(IS_PLAYER_CONTROL_ON,0x1c7a1a7e9e4766cf,							CommandIsPlayerControlOn					);					//	NATIVE PROC IS_PLAYER_CONTROL_ON( INT PlayerIndex )
		SCR_REGISTER_SECURE(GET_ARE_CAMERA_CONTROLS_DISABLED,0x87694b2faa0fc0c7,				CommandGetAreCameraControlsDisabled			);					//	NATIVE PROC GET_ARE_CAMERA_CONTROLS_DISABLED()
		SCR_REGISTER_SECURE(IS_PLAYER_SCRIPT_CONTROL_ON,0x9895f96718388657,						CommandIsPlayerScriptControlOn				);
		SCR_REGISTER_SECURE(IS_PLAYER_CLIMBING,0xd28b9359d0f02aca,								CommandIsPlayerClimbing						);					//	NATIVE PROC IS_PLAYER_CLIMBING( INT PlayerIndex )
		SCR_REGISTER_UNUSED(IS_PLAYER_SURRENDERING, 0x97848078,									CommandIsPlayerSurrendering					);
		SCR_REGISTER_SECURE(IS_PLAYER_BEING_ARRESTED,0x99c20d75af0fb22e,						CommandIsPlayerBeingArrested				);
		SCR_REGISTER_UNUSED(SET_PLAYER_ARRESTED,0x5d89fd0b951b3e35,								CommandSetPlayerArrestedState				);
		SCR_REGISTER_SECURE(RESET_PLAYER_ARREST_STATE,0x4ef216bdd5c4ca98,						CommandResetPlayerArrestState				);
		SCR_REGISTER_UNUSED(CAN_PLAYER_PERFORM_ARREST,0x14c0b14c939cfeab,						CommandCanPlayerPerformArrest				);
		SCR_REGISTER_SECURE(GET_PLAYERS_LAST_VEHICLE,0x9993ef7fdbcdb632,						CommandGetPlayersLastVehicleNoSave			);
		SCR_REGISTER_SECURE(GET_PLAYER_INDEX,0xb6ba8b8e3d0b41c6,								CommandGetPlayerIndex						);					//	NATIVE FUNC INT GET_PLAYER_POINTS( PLAYER_INDEX PlayerIndex )
		SCR_REGISTER_SECURE(INT_TO_PLAYERINDEX,0xa1087a6350cd9244,								CommandConvertIntToPlayerIndex				);
		SCR_REGISTER_SECURE(INT_TO_PARTICIPANTINDEX,0x54f800f95142c750,							CommandConvertIntToParticipantIndex			);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_PLAYER_HIT_VEHICLE,0xafb2d9a35cc1f82d,				CommandGetTimeSincePlayerHitVehicle			);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_PLAYER_HIT_PED,0xb2d0b5ae5881bc0e,					CommandGetTimeSincePlayerHitPed				);
		SCR_REGISTER_UNUSED(GET_TIME_SINCE_PLAYER_HIT_BUILDING,0xaec5115ef4b500ef,				CommandGetTimeSincePlayerHitBuilding		);
		SCR_REGISTER_UNUSED(GET_TIME_SINCE_PLAYER_HIT_OBJECT,0xcfaf27ac3cad2e5a,				CommandGetTimeSincePlayerHitObject			);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_PLAYER_DROVE_ON_PAVEMENT,0x70959fe6aaf74c96,			CommandGetTimeSincePlayerDroveOnPavement	);
		SCR_REGISTER_UNUSED(GET_TIME_SINCE_PLAYER_RAN_LIGHT,0x756550b7f103e66f,					CommandGetTimeSincePlayerRanLight			);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_PLAYER_DROVE_AGAINST_TRAFFIC,0x5962f6054bc15dd5,		CommandGetTimeSincePlayerDroveAgainstTraffic);
		SCR_REGISTER_SECURE(IS_PLAYER_FREE_FOR_AMBIENT_TASK,0x26ddf157f01e6504,					CommandIsPlayerFreeForAmbientTask			);
		SCR_REGISTER_SECURE(PLAYER_ID,0x9e2d6c50374fcfa9,										CommandPlayerId								);
		SCR_REGISTER_SECURE(PLAYER_PED_ID,0xe2d3d51028f0428a,									CommandPlayerPedId							);
		SCR_REGISTER_SECURE(NETWORK_PLAYER_ID_TO_INT,0xa52c4f51ecab7e02,						CommandGetPlayerIndex						);

		SCR_REGISTER_SECURE(HAS_FORCE_CLEANUP_OCCURRED,0x4b34601c5c56f3ee,						CommandHasForceCleanupOccurred);
		SCR_REGISTER_SECURE(FORCE_CLEANUP,0x2481b055c8b5ca09,									CommandForceCleanup);
		SCR_REGISTER_SECURE(FORCE_CLEANUP_FOR_ALL_THREADS_WITH_THIS_NAME,0x7dd73f7f62179990,	CommandForceCleanupForAllThreadsWithThisName);
		SCR_REGISTER_SECURE(FORCE_CLEANUP_FOR_THREAD_WITH_THIS_ID,0xd4798e4f64b31c25,			CommandForceCleanupForThreadWithThisID);
		SCR_REGISTER_SECURE(GET_CAUSE_OF_MOST_RECENT_FORCE_CLEANUP,0x27400a3ffa074f8b,			CommandGetCauseOfMostRecentForceCleanup);

		SCR_REGISTER_UNUSED(SET_PLAYER_MOOD_NORMAL,0x31b4ebc58552715b,							CommandSetPlayerMoodNormal					);
		SCR_REGISTER_SECURE(SET_PLAYER_MAY_ONLY_ENTER_THIS_VEHICLE,0x40da51fc390b094d,			CommandSetPlayerMayOnlyEnterThisVehicle		);
		SCR_REGISTER_SECURE(SET_PLAYER_MAY_NOT_ENTER_ANY_VEHICLE,0x5dd154885519bd3d,			CommandSetPlayerNotAllowedEnterAnyVehicle	);
		SCR_REGISTER_SECURE(GIVE_ACHIEVEMENT_TO_PLAYER,0xb8d77780df8e1242,						CommandAwardAchievement);
		SCR_REGISTER_SECURE(SET_ACHIEVEMENT_PROGRESS,0xee587cff91cf5eda,						CommandSetAchievementProgress);
		SCR_REGISTER_SECURE(GET_ACHIEVEMENT_PROGRESS,0xe70cd2cdee98df14,						CommandGetAchievementProgress);
		SCR_REGISTER_SECURE(HAS_ACHIEVEMENT_BEEN_PASSED,0xda4421f81df4b90d,						CommandHasAchievementBeenPassed             );
		SCR_REGISTER_SECURE(IS_PLAYER_ONLINE,0xc65b603e7942d0db,								CommandIsPlayerOnline);
		SCR_REGISTER_SECURE(IS_PLAYER_LOGGING_IN_NP,0xaa6218da66d7e3ff,							CommandIsPlayerLoggingInNp);
		SCR_REGISTER_UNUSED(IS_PLAYER_ONLINE_NP,0xc0402d65d01d8b65,								CommandIsPlayerOnlineNp);
		SCR_REGISTER_UNUSED(IS_PLAYER_ONLINE_GAMESPY,0xc90b211a7c9b4754,						CommandIsPlayerOnlineGamespy);
		SCR_REGISTER_SECURE(DISPLAY_SYSTEM_SIGNIN_UI,0x7ee945726afadacf,						CommandShowSigninUi);
		SCR_REGISTER_SECURE(IS_SYSTEM_UI_BEING_DISPLAYED,0xdfa2e5162727d6d2,					CommandIsSytemUiShowing);
		SCR_REGISTER_SECURE_HONEYPOT(SET_PLAYER_INVINCIBLE,0xc099da307dd6bc62,					CommandSetPlayerInvincible);
		SCR_REGISTER_SECURE(GET_PLAYER_INVINCIBLE,0xf81d8e7ed654a736,							CommandGetPlayerInvincible);
		SCR_REGISTER_SECURE(GET_PLAYER_DEBUG_INVINCIBLE,0x76df66470b55ae8f,					CommandGetPlayerDebugInvincible);
		SCR_REGISTER_SECURE(SET_PLAYER_INVINCIBLE_BUT_HAS_REACTIONS,0x7e9ca7389ef595e6,			CommandSetPlayerInvincibleButHasReactions);
		SCR_REGISTER_UNUSED(SET_PLAYER_CAN_CARRY_NON_MISSION_OBJECTS, 0x54b0c19b,				CommandAllowPlayerToCarryNonMissionObjects);
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_COLLECT_DROPPED_MONEY,0x01a3f029bd732372,			CommandSetPlayerCanCollectDroppedMoney);
		SCR_REGISTER_UNUSED(SET_PLAYER_CAN_USE_VEHICLE_SEARCH_LIGHT,0xd16bed9cfa9181a9,			CommandSetPlayerCanUseVehicleSearchLight);
		SCR_REGISTER_SECURE(REMOVE_PLAYER_HELMET,0x2f163a300f34f245,							CommandRemovePlayerHelmet);
		SCR_REGISTER_SECURE(GIVE_PLAYER_RAGDOLL_CONTROL,0xb5e6954fe3fb7cce,						CommandGivePlayerRagdollControl);
		SCR_REGISTER_SECURE(SET_PLAYER_LOCKON,0x05611fa30193f5b5,								CommandSetPlayerLockon);
		SCR_REGISTER_UNUSED(SET_LOCKON_TO_FRIENDLY_PLAYERS,0x074d7227ca41020a,					CommandAllowLockonToFriendlyPlayers);
		SCR_REGISTER_SECURE(SET_PLAYER_TARGETING_MODE,0x74970e4fa8f6b345,						CommandSetPlayerTargetingMode);
		SCR_REGISTER_UNUSED(GET_PLAYER_TARGETING_MODE,0x2751fbc4e04e92f4,						CommandGetPlayerTargetingMode);
		SCR_REGISTER_SECURE(SET_PLAYER_TARGET_LEVEL,0x50a5fc22a168dda2,							CommandSetPlayerTargetLevel);
		SCR_REGISTER_SECURE(GET_IS_USING_FPS_THIRD_PERSON_COVER,0x425d4ff8ff23ee78,				CommandIsUsingFPSThirdPersonCover);
		SCR_REGISTER_SECURE(GET_IS_USING_HOOD_CAMERA,0x64edc8d24bf9eec0,						CommandIsUsingHoodCamera);
		SCR_REGISTER_UNUSED(GET_OBJECT_PLAYER_WILL_PICKUP,0x0757a4b70196ab75,					CommandWhatWillPlayerPickup);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_HAS_DAMAGED_AT_LEAST_ONE_PED,0x3c2a46bb06553cee,		CommandClearPlayerHasDamagedAtLeastOnePed);
		SCR_REGISTER_SECURE(HAS_PLAYER_DAMAGED_AT_LEAST_ONE_PED,0x51223ab40e2f620e,				CommandHasPlayerDamagedAtLeastOnePed);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_HAS_DAMAGED_AT_LEAST_ONE_NON_ANIMAL_PED,0xf01ad4af166855d2,	CommandClearPlayerHasDamagedAtLeastOneNonAnimalPed);
		SCR_REGISTER_SECURE(HAS_PLAYER_DAMAGED_AT_LEAST_ONE_NON_ANIMAL_PED,0x7246ae6e7c9ca089,			CommandHasPlayerDamagedAtLeastOneNonAnimalPed);
		SCR_REGISTER_SECURE(SET_AIR_DRAG_MULTIPLIER_FOR_PLAYERS_VEHICLE,0xcf7f6e047025528c,		CommandForceAirDragMultForPlayersVehicle);
		SCR_REGISTER_SECURE(SET_SWIM_MULTIPLIER_FOR_PLAYER,0xc04f56833375054e,					CommandSetSwimMultiplierForPlayer);
		SCR_REGISTER_SECURE(SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER,0x0c6ebabc089a80be,			CommandSetRunSprintMultiplierForPlayer);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_LAST_ARREST,0x6824a90851dbf244,						CommandGetTimeSinceLastArrest);
		SCR_REGISTER_SECURE(GET_TIME_SINCE_LAST_DEATH,0x23c8a58f6f1c9b52,						CommandGetTimeSinceLastDeath);
		SCR_REGISTER_UNUSED(GET_TRAIN_PLAYER_WOULD_ENTER,0x04c13134f5cb89d1,					CommandGetTrainPlayerWouldEnter);
		SCR_REGISTER_UNUSED(DISPLAY_PLAYER_COMPONENT,0x7e6a207868f07b7d,						CommandSetDrawPlayerComponent);
		SCR_REGISTER_UNUSED(DOES_MAIN_PLAYER_EXIST,0x5a0286552d34c344,							CommandDoesMainPlayerExist);
		SCR_REGISTER_UNUSED(ASSISTED_MOVEMENT_OPEN_ROUTE,0x5b9800e127b28910,					CommandAssistedMovementOpenRoute);
		SCR_REGISTER_SECURE(ASSISTED_MOVEMENT_CLOSE_ROUTE,0x647dc34f84b77534,					CommandAssistedMovementCloseRoute);
		SCR_REGISTER_SECURE(ASSISTED_MOVEMENT_FLUSH_ROUTE,0x662e7343c29d10eb,					CommandAssistedMovementFlushRoute);
		SCR_REGISTER_UNUSED(ASSISTED_MOVEMENT_ADD_POINT,0xb2a3c724543d6261,						CommandAssistedMovementAddPoint);
		SCR_REGISTER_UNUSED(ASSISTED_MOVEMENT_SET_WIDTH, 0x76ffd005,							CommandAssistedMovementSetWidth);
		SCR_REGISTER_UNUSED(ASSISTED_MOVEMENT_IS_ON_ANY_ROUTE,0x977fc240046bc725,				CommandAssistedMovementIsOnAnyRoute);
		SCR_REGISTER_UNUSED(ASSISTED_MOVEMENT_IS_ON_SCRIPTED_ROUTE,0xc2e604bb77c0436c,			CommandAssistedMovementIsOnScriptedRoute);
		SCR_REGISTER_SECURE(SET_PLAYER_FORCED_AIM,0xf5a5b768549ee604,							CommandSetPlayerForcedAim);
		SCR_REGISTER_SECURE(SET_PLAYER_FORCED_ZOOM,0x921859563fe3c7d8,							CommandSetPlayerForcedZoom);
		SCR_REGISTER_SECURE(SET_PLAYER_FORCE_SKIP_AIM_INTRO,0x29bd7dc6acd28546,					CommandSetPlayerForceSkipAimIntro);
		SCR_REGISTER_UNUSED(SET_PLAYER_MAX_MOVE_BLEND_RATIO,0xea6ca3528cf9441b,					CommandSetPlayerMaxMoveBlendRatio);
        SCR_REGISTER_UNUSED(SET_INVERT_PLAYER_WALK,0xdc6bf5ef8e0e75e7,						    CommandSetInvertPlayerWalk);
		SCR_REGISTER_SECURE(DISABLE_PLAYER_FIRING,0xb07ec4599255161e,							CommandDisablePlayerFiring);
		SCR_REGISTER_SECURE(DISABLE_PLAYER_THROW_GRENADE_WHILE_USING_GUN,0x4ad02a972d2153c3,	CommandDisablePlayerThrowingGrenadeWhileUsingGun);
		SCR_REGISTER_SECURE(SET_DISABLE_AMBIENT_MELEE_MOVE,0x0917a1352a09c319,					CommandSetDisableAmbientMeleeMove);
		SCR_REGISTER_UNUSED(SET_PLAYER_JACK_SPEED,0xc4fa1b1238f97dda,							CommandSetPlayerJackSpeed);
		SCR_REGISTER_UNUSED(SET_PLAYER_STEALTH_SPEED,0x59f960d6f9f18040,						CommandSetPlayerStealthSpeed);
        SCR_REGISTER_SECURE(SET_PLAYER_MAX_ARMOUR,0x89a7d91fa86a73f1,			                CommandSetPlayerMaxArmour);
		
		SCR_REGISTER_UNUSED(GET_PLAYER_SELECTED_ABILITY_SLOT,0xb82d8d9ea7aba971,				CommandGetPlayerSelectedAbilitySlot);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_ACTIVATE,0xaef827744eca5aea,		                CommandSpecialAbilityActivate);
		SCR_REGISTER_UNUSED(CAN_SPECIAL_ABILITY_BE_ACTIVATED,0x515321622d11b14c,				CommandCanSpecialAbilityBeActivated);
		SCR_REGISTER_UNUSED(CAN_SPECIAL_ABILITY_BE_SELECTED_IN_ARCADE_MODE,0x4149c64e5fcc4466,	CommandCanSpecialAbilityBeSelectedInArcadeMode);
		SCR_REGISTER_SECURE(SET_SPECIAL_ABILITY_MP,0x5134e3f4c3ed2572,							CommandSetSpecialAbilityMP);
		SCR_REGISTER_UNUSED(GET_LOCAL_PLAYER_SPECIAL_ABILITY_MP,0x40df36b3febdd50c,				CommandGetLocalPlayerSpecialAbilityMP);
		SCR_REGISTER_SECURE(SPECIAL_ABILITY_DEACTIVATE_MP,0x3a6863cc483284fd,					CommandSpecialAbilityDeactivateMP);		
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_DEACTIVATE,0xc204033758e4127f,		                CommandSpecialAbilityDeactivate);
		SCR_REGISTER_SECURE(SPECIAL_ABILITY_DEACTIVATE_FAST,0x85b547885d8f8b40,					CommandSpecialAbilityDeactivateFast);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_RESET,0xc59f3672444b1ee5,			                CommandSpecialAbilityReset);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_ON_MISSION_FAILED,0xa131cc850da50cf0,		CommandSpecialAbilityChargeOnMissionFailed);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_SMALL,0x7c94b5047d73de57,					CommandSpecialAbilityChargeSmall);
	    SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_MEDIUM,0x3f2a203e395eb931,					CommandSpecialAbilityChargeMedium);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_LARGE,0x06055697ad0bc5eb,					CommandSpecialAbilityChargeLarge);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_CONTINUOUS,0x38a059a79a45e98d,				CommandSpecialAbilityChargeContinuous);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_ABSOLUTE,0x8c8a3177ffab2a2d,                 CommandSpecialAbilityChargeAbsolute);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_CHARGE_NORMALIZED,0x266e4b6e6c97e2cb,               CommandSpecialAbilityChargeNormalized);
		SCR_REGISTER_SECURE(SPECIAL_ABILITY_FILL_METER,0x6dd7ec85b970fff8,						CommandSpecialAbilityFillMeter);
		SCR_REGISTER_SECURE(SPECIAL_ABILITY_DEPLETE_METER,0x8c6a1e9558ae8255,					CommandSpecialAbilityDepleteMeter);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_LOCK,0x36f8265cd4c1dc7b,                            CommandSpecialAbilityLock);
        SCR_REGISTER_SECURE(SPECIAL_ABILITY_UNLOCK,0x9af618154f4af9d7,                          CommandSpecialAbilityUnlock);
		SCR_REGISTER_UNUSED(GET_SPECIAL_ABILITY_REMAINING_PERCENTAGE,0x9c2aba0bdeb048e7,		CommandGetSpecialAbilityRemainingPercentage);
        
		SCR_REGISTER_SECURE(IS_SPECIAL_ABILITY_UNLOCKED,0xbde74e4e16908df5,                     CommandIsSpecialAbilityUnlocked);
		SCR_REGISTER_SECURE(IS_SPECIAL_ABILITY_ACTIVE,0xa89c6f3818cc3acb,						CommandIsSpecialAbilityActive);
		SCR_REGISTER_SECURE(IS_SPECIAL_ABILITY_METER_FULL,0xc9eabec2eb7eac4a,                   CommandIsSpecialAbilityMeterFull);
		SCR_REGISTER_SECURE(ENABLE_SPECIAL_ABILITY,0x9f1fd386a42a87da,							CommandEnableSpecialAbility);
		SCR_REGISTER_SECURE(IS_SPECIAL_ABILITY_ENABLED,0x74eacb77c7688f1c,						CommandIsSpecialAbilityEnabled);
		SCR_REGISTER_SECURE(SET_SPECIAL_ABILITY_MULTIPLIER,0xe4fd06c1760ec85e,					CommandSetSpecialAbilityMultiplier);
		SCR_REGISTER_SECURE(UPDATE_SPECIAL_ABILITY_FROM_STAT,0xd7072950313d43d7,				CommandUpdateSpecialAbilityFromStat);
		
		SCR_REGISTER_UNUSED(START_LOCAL_PLAYER_SPECIAL_ABILITY_FX,0xb97d5fbf7d02b645, CommandStartLocalPlayerSpecialAbilityFX);
		SCR_REGISTER_UNUSED(STOP_LOCAL_PLAYER_SPECIAL_ABILITY_FX,0x43d20c82aa9fe610, CommandStopLocalPlayerSpecialAbilityFX);
			
		SCR_REGISTER_UNUSED(INIITIALISE_ARCADE_ABILITIES,0x66276c0759050711,					CommandInitialiseArcadeAbilities);
		SCR_REGISTER_UNUSED(ACTIVATE_ARCADE_ABILITY,0xeba93b47c5c09cbf,						CommandActivateArcadeAbility);
		SCR_REGISTER_UNUSED(DEACTIVATE_ARCADE_ABILITY,0xdd5e360eb2170111,						CommandDeactivateArcadeAbility);
		SCR_REGISTER_UNUSED(IS_ARCADE_ABILITY_ACTIVE,0x582155efa93aad59,						CommandIsArcadeAbilityActive);
		SCR_REGISTER_UNUSED(SET_ARCADE_ABILITY_MODIFIER,0x8e6fd476c8c37b82,					CommandSetArcadeAbilityModifier);
		SCR_REGISTER_UNUSED(GET_ARCADE_ABILITY_MODIFIER,0x9d33969ae27b02be,					CommandGetArcadeAbilityModifier);
		SCR_REGISTER_UNUSED(CLEANUP_ARCADE_ABILITIES,0xb4ade47c2ec2a996,						CommandCleanupArcadeAbilities);
		SCR_REGISTER_UNUSED(GET_PLAYER_ARCADE_TEAM,0xd657131873bca926,							CommandGetArcadeTeam);
		SCR_REGISTER_UNUSED(GET_PLAYER_ARCADE_ROLE,0x85f98c2154092a64,							CommandGetArcadeRole);
		SCR_REGISTER_UNUSED(SET_PLAYER_ARCADE_TEAM_AND_ROLE,0x4d0990bbcea4e345,				CommandSetArcadeTeamAndRole);
		SCR_REGISTER_UNUSED(SET_LOCAL_PLAYER_ARCADE_ACTIVE_VEHICLE,0x74ab49aab277a3cc,			CommandSetLocalPlayerArcadeActiveVehicle);

		SCR_REGISTER_SECURE(GET_IS_PLAYER_DRIVING_ON_HIGHWAY,0xe7cdc562b263ca12,				CommandGetIsPlayerDrivingOnHighway);
		SCR_REGISTER_SECURE(GET_IS_PLAYER_DRIVING_WRECKLESS,0xf2162c96615104e4,					CommandGetIsPlayerDrivingWreckless);
		SCR_REGISTER_SECURE(GET_IS_MOPPING_AREA_FREE_IN_FRONT_OF_PLAYER,0xa61d663494402396,		CommandGetIsMoppingAreaFreeInFrontOfPlayer);
		SCR_REGISTER_UNUSED(GET_LAST_NEAR_MISS_VEHICLE,0x2f6194032ec5ec20,	                    CommandGetLastNearMissVehicle);
		SCR_REGISTER_UNUSED(GET_TIME_SINCE_LAST_NEAR_MISS,0x6bdcc60e2c06d024,	                CommandGetTimeSinceLastNearMiss);
		SCR_REGISTER_SECURE(START_PLAYER_TELEPORT,0x8a87922c72f578ec,							CommandStartPlayerTeleport);
		SCR_REGISTER_SECURE(UPDATE_PLAYER_TELEPORT,0x0c752c2ccfc18307,							CommandUpdatePlayerTeleport);
		SCR_REGISTER_SECURE(STOP_PLAYER_TELEPORT,0x093f334b2f3729db,							CommandStopPlayerTeleport);
		SCR_REGISTER_SECURE(IS_PLAYER_TELEPORT_ACTIVE,0xbc4351060befb0d2,						CommandIsPlayerTeleportActive);
		SCR_REGISTER_SECURE(GET_PLAYER_CURRENT_STEALTH_NOISE,0x62dde94612b5849e,				CommandGetPlayerCurrentStealthNoise);
		SCR_REGISTER_SECURE(SET_PLAYER_HEALTH_RECHARGE_MULTIPLIER,0xc0d061dfdaf121ee,			CommandSetPlayerHealthRechargeMultiplier);
		SCR_REGISTER_SECURE(GET_PLAYER_HEALTH_RECHARGE_MAX_PERCENT,0x3c2c079ce6f46b91,			CommandGetPlayerHealthRechargeMaxPercent);
		SCR_REGISTER_SECURE(SET_PLAYER_HEALTH_RECHARGE_MAX_PERCENT,0x7effc69e5350eed6,			CommandSetPlayerHealthRechargeMaxPercent);
		SCR_REGISTER_UNUSED(DISABLE_PLAYER_HEALTH_RECHARGE,0x196e0bb6e33c96bb,					CommandDisablePlayerHealthRecharge);
		SCR_REGISTER_UNUSED(GET_PLAYER_FALL_DISTANCE_TO_TRIGGER_RAGDOLL_OVERRIDE,0xef60704068e3e7c1,	CommandGetPlayerFallDistanceToTriggerRagdollOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_FALL_DISTANCE_TO_TRIGGER_RAGDOLL_OVERRIDE,0x9d4c11eeebeddcff,	CommandSetPlayerFallDistanceToTriggerRagdollOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_WEAPON_DAMAGE_MODIFIER,0xd27f869625866850,				CommandSetPlayerWeaponDamageModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_WEAPON_DEFENSE_MODIFIER,0xefe4775047323c19,				CommandSetPlayerWeaponDefenseModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_WEAPON_MINIGUN_DEFENSE_MODIFIER,0xbf94e55ee11dabdf,		CommandSetPlayerWeaponMinigunDefenseModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_MELEE_WEAPON_DAMAGE_MODIFIER,0xabb7c36b502bf82e,			CommandSetPlayerMeleeWeaponDamageModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_MELEE_WEAPON_DEFENSE_MODIFIER,0x59c6987f8fa159ba,		CommandSetPlayerMeleeWeaponDefenseModifier);
		SCR_REGISTER_UNUSED(SET_PLAYER_MELEE_WEAPON_FORCE_MODIFIER,0x6183b4f11f0b8d09,			CommandSetPlayerMeleeWeaponForceModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_VEHICLE_DAMAGE_MODIFIER,0x12aa10f5fac06542,				CommandSetPlayerVehicleDamageModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_VEHICLE_DEFENSE_MODIFIER,0x0acd4d4123d79892,				CommandSetPlayerVehicleDefenseModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_MAX_EXPLOSIVE_DAMAGE,0xfa556584ae68db4b,					CommandSetPlayerMaxExplosiveDamage);
		SCR_REGISTER_SECURE(SET_PLAYER_EXPLOSIVE_DAMAGE_MODIFIER,0x9b8b6c9b77039457,			CommandSetPlayerExplosiveDamageModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_WEAPON_TAKEDOWN_DEFENSE_MODIFIER,0x9e5ca28f9b676421,		CommandSetPlayerWeaponTakedownDefenseModifier);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_TINT_INDEX,0x5b315b77ff0579f4,					CommandSetPlayerParachuteTintIndex);
		SCR_REGISTER_SECURE(GET_PLAYER_PARACHUTE_TINT_INDEX,0xfc2bfa7cbd781b8f,					CommandGetPlayerParachuteTintIndex);
		SCR_REGISTER_SECURE(SET_PLAYER_RESERVE_PARACHUTE_TINT_INDEX,0x4ffa0b650332a748,			CommandSetPlayerReserveParachuteTintIndex);
		SCR_REGISTER_SECURE(GET_PLAYER_RESERVE_PARACHUTE_TINT_INDEX,0x4d70bbbc048f9efe,			CommandGetPlayerReserveParachuteTintIndex);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_PACK_TINT_INDEX,0x6a9da6a45b05cbac,			CommandSetPlayerParachutePackTintIndex);
		SCR_REGISTER_SECURE(GET_PLAYER_PARACHUTE_PACK_TINT_INDEX,0x5a52ede69663ab67,			CommandGetPlayerParachutePackTintIndex);
		SCR_REGISTER_SECURE(SET_PLAYER_HAS_RESERVE_PARACHUTE,0xadaf9b9713526673,				CommandSetPlayerHasReserveParachute);
		SCR_REGISTER_SECURE(GET_PLAYER_HAS_RESERVE_PARACHUTE,0xe37296e8bded7a5c,				CommandGetPlayerHasReserveParachute);
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_LEAVE_PARACHUTE_SMOKE_TRAIL,0x0b5cb004ea93110f,		CommandSetPlayerCanLeaveParachuteSmokeTrail);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR,0x2a4c3472cd44ab3c,			CommandSetPlayerParachuteSmokeTrailColor);
		SCR_REGISTER_SECURE(GET_PLAYER_PARACHUTE_SMOKE_TRAIL_COLOR,0x593c0beb67fa713d,			CommandGetPlayerParachuteSmokeTrailColor);
		SCR_REGISTER_UNUSED(SET_PLAYER_RESET_FLAG_PREFER_REAR_SEATS,0x11d2f13707a54f0a,			CommandsSetPlayerResetFlagPreferRearSeats);
		SCR_REGISTER_UNUSED(SET_PLAYER_RESET_FLAG_PREFER_FRONT_PASSENGER_SEAT,0x5f013a06717af95a,		CommandsSetPlayerResetFlagPreferFrontPassengerSeat);
		SCR_REGISTER_SECURE(SET_PLAYER_PHONE_PALETTE_IDX,0x801bbd5e2830170e,					CommandSetPlayerPhonePaletteIdx);
		SCR_REGISTER_UNUSED(GET_PLAYER_PHONE_PALETTE_IDX,0x01cbf2e57ed4fcf1,					CommandGetPlayerPhonePaletteIdx);
		SCR_REGISTER_UNUSED(SET_DISPLAY_PLAYER_DAMAGE_INFO,0xde816b2a86175f36,					CommandSetDisplayPlayerDamageInfo);
		SCR_REGISTER_UNUSED(GET_PLAYER_NOISE_MULTIPLIER,0x0275327f7112d849,						CommandGetPlayerNoiseMultiplier);
		SCR_REGISTER_SECURE(SET_PLAYER_NOISE_MULTIPLIER,0xf3e520d4ddfe531b,						CommandSetPlayerNoiseMultiplier);
		SCR_REGISTER_SECURE(SET_PLAYER_SNEAKING_NOISE_MULTIPLIER,0xa79405544d13368b,			CommandSetPlayerSneakingNoiseMultiplier);
		SCR_REGISTER_SECURE(CAN_PED_HEAR_PLAYER,0x42a51c8e69566474,								CommandCanPedHearPlayer);
		SCR_REGISTER_SECURE(SIMULATE_PLAYER_INPUT_GAIT,0x44b88fb74a8ddc72,						CommandSimulatePlayerInputGait);
		SCR_REGISTER_SECURE(RESET_PLAYER_INPUT_GAIT,0x2fa820332400f593,							CommandResetPlayerInputGait);
		SCR_REGISTER_SECURE(SET_AUTO_GIVE_PARACHUTE_WHEN_ENTER_PLANE,0xc5504757fd12bb7b,		CommandSetAutoGiveParachuteWhenEnterPlane);
		SCR_REGISTER_SECURE(SET_AUTO_GIVE_SCUBA_GEAR_WHEN_EXIT_VEHICLE,0xc7bfb1837a4167db,		CommandSetAutoGiveScubaGearWhenExitVehicle);
		SCR_REGISTER_SECURE(SET_PLAYER_STEALTH_PERCEPTION_MODIFIER,0xa5695a8b4fdb4f5e,			CommandSetPlayerStealthPerceptionModifier);
		SCR_REGISTER_UNUSED(GET_PLAYER_STEALTH_PERCEPTION_MODIFIER,0x3288b5aa51284100,			CommandGetPlayerStealthPerceptionModifier);
        SCR_REGISTER_SECURE(IS_REMOTE_PLAYER_IN_NON_CLONED_VEHICLE,0xe1e3700d85b26c1b,          CommandIsRemotePlayerInNonClonedVehicle);
		SCR_REGISTER_SECURE(INCREASE_PLAYER_JUMP_SUPPRESSION_RANGE,0x7544277489901b13,          CommandIncreasePlayerJumpSuppressionRange);
		SCR_REGISTER_SECURE(SET_PLAYER_SIMULATE_AIMING,0xe528977784fc248a,					    CommandSetPlayerSimulateAiming);
		SCR_REGISTER_SECURE(SET_PLAYER_CLOTH_PIN_FRAMES,0x1dd4b2f07d800518,						CommandSetPlayerClothPinFrames);
		SCR_REGISTER_SECURE(SET_PLAYER_CLOTH_PACKAGE_INDEX,0x3c42bb73c8569132,					CommandSetPlayerClothPackageIndex);
		SCR_REGISTER_SECURE(SET_PLAYER_CLOTH_LOCK_COUNTER,0x148b70d5e8887d3f,					CommandSetPlayerClothLockCounter);		
		SCR_REGISTER_SECURE(PLAYER_ATTACH_VIRTUAL_BOUND,0x45ab8ba30f865cb3,						CommandPlayerAttachVirtualBound);
		SCR_REGISTER_SECURE(PLAYER_DETACH_VIRTUAL_BOUND,0x0040caada929b9a5,						CommandPlayerDetachVirtualBound);
		SCR_REGISTER_SECURE(HAS_PLAYER_BEEN_SPOTTED_IN_STOLEN_VEHICLE,0xbeeefdea6103f9d5,		CommandHasPlayerBeenSpottedInStolenVehicle);
		SCR_REGISTER_UNUSED(GET_SPOTTER_OF_PLAYER_IN_STOLEN_VEHICLE,0x3b9b5f040dbecaa3,			CommandGetSpotterOfPlayerInStolenVehicle);
		SCR_REGISTER_UNUSED(SET_PLAYER_TO_USE_COVER_THREAT_WEIGHTING,0xd7ec8c7119c8f543,		CommandSetPlayerToUseCoverThreatWeighting);
		SCR_REGISTER_SECURE(IS_PLAYER_BATTLE_AWARE,0xaf3f5649b6d00465,							CommandIsPlayerBattleAware);
		SCR_REGISTER_SECURE(GET_PLAYER_RECEIVED_BATTLE_EVENT_RECENTLY,0x97e6091aa1973956,		CommandPlayerReceivedBattleEventRecently);
		SCR_REGISTER_SECURE(EXTEND_WORLD_BOUNDARY_FOR_PLAYER,0xf9b446afd56f5445,				CommandExtendWorldBoundaryForPlayer);
		SCR_REGISTER_SECURE(RESET_WORLD_BOUNDARY_FOR_PLAYER,0xc6c76b77086d3daa,					CommandResetWorldBoundaryForPlayer);
		SCR_REGISTER_SECURE(IS_PLAYER_RIDING_TRAIN,0xe267eaf3df9f6bd1,							CommandIsPlayerRidingTrain);
		SCR_REGISTER_SECURE(HAS_PLAYER_LEFT_THE_WORLD,0x4f9fbd9d1f101839,						CommandHasPlayerLeftTheWorld);
		SCR_REGISTER_UNUSED(DISABLE_DISPATCH_HELI_REFUELING,0x881ac55434ed380a,					CommandDisableDispatchHeliRefueling);
		SCR_REGISTER_UNUSED(DISABLE_HELI_DESTROYED_DISPATCH_SPAWN_DELAY,0x139c854370f25197,		CommandDisableHeliDestroyedDispatchSpawnDelay);
		SCR_REGISTER_SECURE(SET_PLAYER_LEAVE_PED_BEHIND,0xedd6fa5743d3a359,						CommandSetPlayerLeavePedBehind);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_VARIATION_OVERRIDE,0x5663081cf61457eb,			CommandSetPlayerParachuteVariationOverride);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_PARACHUTE_VARIATION_OVERRIDE,0xdfce9946492b1ebb,		CommandClearPlayerParachuteVariationOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_MODEL_OVERRIDE,0x4b91103eae3ecfbb,				CommandSetPlayerParachuteModelOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_RESERVE_PARACHUTE_MODEL_OVERRIDE,0x97a7e5b40d58666e,	CommandSetPlayerReserveParachuteModelOverride);
		SCR_REGISTER_SECURE(GET_PLAYER_PARACHUTE_MODEL_OVERRIDE,0x64180d3cb343acf9,				CommandGetPlayerParachuteModelOverride);
		SCR_REGISTER_SECURE(GET_PLAYER_RESERVE_PARACHUTE_MODEL_OVERRIDE,0x4ace43a0376677b0,	CommandGetPlayerReserveParachuteModelOverride);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_PARACHUTE_MODEL_OVERRIDE,0x0dedd914eb7a1ce6,			CommandClearPlayerParachuteModelOverride);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_RESERVE_PARACHUTE_MODEL_OVERRIDE,0x812f5aaf0bbe170f,  CommandClearPlayerReserveParachuteModelOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_PARACHUTE_PACK_MODEL_OVERRIDE,0xd386a07b18792f21,		CommandSetPlayerParachutePackModelOverride);
		SCR_REGISTER_SECURE(CLEAR_PLAYER_PARACHUTE_PACK_MODEL_OVERRIDE,0xd07bc41548db6391,		CommandClearPlayerParachutePackModelOverride);
		SCR_REGISTER_SECURE(DISABLE_PLAYER_VEHICLE_REWARDS,0x73d32b6c724386d4,					CommandDisablePlayerVehicleRewards);
		SCR_REGISTER_UNUSED(DISABLE_WANTED_REL_GROUP_RESET_THIS_FRAME,0xa975b6508bf58958,		CommandDisableWantedRelationshipGroupReset);
		SCR_REGISTER_UNUSED(IS_PLAYER_SPECTATED_VEHICLE_RADIO_OVERRIDE,0x6dafc808e24c0ded,		CommandIsPlayerSpectatedVehicleRadioOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_SPECTATED_VEHICLE_RADIO_OVERRIDE,0x904a1a06bb7d0b76,		CommandSetPlayerSpectatedVehicleRadioOverride);
		SCR_REGISTER_SECURE(SET_PLAYER_BLUETOOTH_STATE,0x6f5a4936c4df72a0,						CommandEnableBlueToothHeadSet);
		SCR_REGISTER_SECURE(IS_PLAYER_BLUETOOTH_ENABLE,0xae84a55f7e62c04c,						CommandGetEnableBlueToothHeadSet);
		SCR_REGISTER_SECURE(DISABLE_CAMERA_VIEW_MODE_CYCLE,0x1784ea330ee3ad4e,					CommandDisableCameraViewModeCycle);
		SCR_REGISTER_SECURE(GET_PLAYER_FAKE_WANTED_LEVEL,0x161c7b2e8d844c6c,					CommandGetPlayerFakeWantedLevel);
		SCR_REGISTER_SECURE(SET_PLAYER_CAN_DAMAGE_PLAYER,0x6abb3d68b10d5597,					CommandSetPlayerCanDamagePlayer);
		SCR_REGISTER_SECURE(SET_APPLY_WAYPOINT_OF_PLAYER,0xc8004c24dae59107,					CommandSetApplyWaypointOfPlayer);
		SCR_REGISTER_SECURE(IS_PLAYER_VEHICLE_WEAPON_TOGGLED_TO_NON_HOMING,0xd21ca22014584990,	CommandIsPlayerVehicleWeaponToggledToNonHoming);
		SCR_REGISTER_SECURE(SET_PLAYER_VEHICLE_WEAPON_TO_NON_HOMING,0x86ac447a712352ff,			CommandSetPlayerVehicleWeaponToNonHoming);
		SCR_REGISTER_SECURE(SET_PLAYER_HOMING_DISABLED_FOR_ALL_VEHICLE_WEAPONS,0x15167583e169ed45,		CommandSetPlayerHomingDisabledForAllVehicleWeapons);

		SCR_REGISTER_SECURE(ADD_PLAYER_TARGETABLE_ENTITY,0x07e8eabdfe9fdf42,					CommandAddPlayerTargetableEntity);
		SCR_REGISTER_SECURE(REMOVE_PLAYER_TARGETABLE_ENTITY,0x86841c9019b7f41c,					CommandRemovePlayerTargetableEntity);
		SCR_REGISTER_UNUSED(CLEAR_PLAYER_TARGETABLE_ENTITIES,0x4afaed887f949d14,				CommandClearPlayerTargetableEntities);
		SCR_REGISTER_UNUSED(GET_IS_ENTITY_IN_TARGETABLE_BY_PLAYER_ARRAY,0x2d676bf42a0cf7fa,		CommandGetIsEntityInTargetableByPlayerArray);

		SCR_REGISTER_SECURE(SET_PLAYER_PREVIOUS_VARIATION_DATA,0x8840802df7e20b30,				CommandSetPlayerPreviousVariationData);
		SCR_REGISTER_SECURE(REMOVE_SCRIPT_FIRE_POSITION,0x483de9cec1cbfd93,					CommandRemoveScriptFirePosition);
		SCR_REGISTER_SECURE(SET_SCRIPT_FIRE_POSITION,0x23793ff4ded698b1,						CommandSetScriptFirePosition);
	}

}	//	end of namespace player_commands
