
// Rage headers
#include "script/wrapper.h"
#include "script/script_channel.h"

// Game headers
#include "network/NetworkInterface.h"
#include "Network/players/NetworkPlayerMgr.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "scene/world/GameWorld.h"
#include "script/Script.h"

SCRIPT_OPTIMISATIONS()

PARAM(netAutoMultiplayerMenu, "[network] If present the game will automatically launch into the multiplayer menu.");
PARAM(StraightIntoCreator, "[network] If present the game will automatically launch into the multiplayer game in creator.");
NOSTRIP_PC_PARAM(StraightIntoFreemode, "[network] If present the game will automatically launch into the multiplayer game in freemode.");

namespace lobby_commands
{

RAGE_DEFINE_SUBCHANNEL(script, lobby, DIAG_SEVERITY_DEBUG3)
#undef __script_channel
#define __script_channel script_lobby

	bool CommandLobbyAutoMultiplayerMenu()
	{
		return PARAM_netAutoMultiplayerMenu.Get();
	}

	bool CommandLobbyAutoMultiplayerFreemode()
	{
		return CNetwork::GetGoStraightToMultiplayer() || PARAM_StraightIntoFreemode.Get() ORBIS_ONLY(|| g_SysService.HasParam("-StraightIntoFreemode"));
	}

	void CommandLobbySetAutoMultiplayer(bool bIsAuto)
	{
		scriptDebugf1("CommandLobbySetAutoMultiplayer :: %s", bIsAuto ? "True" : "False");

#if RSG_PC
		if (PARAM_StraightIntoFreemode.Get() && !bIsAuto)
		{
			scriptDebugf1("CommandLobbySetAutoMultiplayer :: Clearing PARAM_StraightIntoFreemode");
			PARAM_StraightIntoFreemode.Set(NULL);
		}
#endif

		CNetwork::SetGoStraightToMultiplayer(bIsAuto);
	}

    bool CommandLobbyAutoMultiplayerEvent()
    {
        return CNetwork::GetGoStraightToMPEvent();
    }

    void CommandLobbySetAutoMpEvent(bool bIsAuto)
    {
		scriptDebugf1("CommandLobbySetAutoMpEvent :: %s", bIsAuto ? "True" : "False");
		CNetwork::SetGoStraightToMPEvent(bIsAuto);
    }

	bool CommandLobbyAutoMultiplayerRandomJob()
	{
		return CNetwork::GetGoStraightToMPRandomJob();
	}

	void CommandLobbySetAutoMpRandomJob(bool bIsAuto)
	{
		scriptDebugf1("CommandLobbySetAutoMpRandomJob :: %s", bIsAuto ? "True" : "False");
		CNetwork::SetGoStraightToMPRandomJob(bIsAuto);
	}

	void CommandShutdownSessionClearsAutoMultiplayer(bool bClearsAutoMultiplayer)
	{
		scriptDebugf1("CommandShutdownSessionClearsAutoMultiplayer :: %s", bClearsAutoMultiplayer ? "True" : "False");
		CNetwork::SetShutdownSessionClearsTheGoStraightToMultiplayerFlag(bClearsAutoMultiplayer);
	}

	bool CommandLobbyAutoMultiplayerCreator()
	{
		return PARAM_StraightIntoCreator.Get();
	}

	bool CommandDeprecated()
	{
		return false;
	}

	/*------------------------------------------------------------------------------------------------------------------------------*/

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(LOBBY_AUTO_MULTIPLAYER_MENU,0xe056c89ece707845, CommandLobbyAutoMultiplayerMenu);
		SCR_REGISTER_SECURE(LOBBY_AUTO_MULTIPLAYER_FREEMODE,0x102628dfcaf80472, CommandLobbyAutoMultiplayerFreemode);
		SCR_REGISTER_UNUSED(LOBBY_AUTO_MULTIPLAYER_CREATOR,0x367e205ffb605a5c, CommandLobbyAutoMultiplayerCreator);
		SCR_REGISTER_SECURE(LOBBY_SET_AUTO_MULTIPLAYER,0x4d2d8cf744877a20, CommandLobbySetAutoMultiplayer);
        SCR_REGISTER_SECURE(LOBBY_AUTO_MULTIPLAYER_EVENT,0xaed44b40a58aadd1, CommandLobbyAutoMultiplayerEvent);
        SCR_REGISTER_SECURE(LOBBY_SET_AUTO_MULTIPLAYER_EVENT,0x53029271aa306405, CommandLobbySetAutoMpEvent);

		SCR_REGISTER_SECURE(LOBBY_AUTO_MULTIPLAYER_RANDOM_JOB,0xd02495957f745dd0, CommandLobbyAutoMultiplayerRandomJob);
		SCR_REGISTER_SECURE(LOBBY_SET_AUTO_MP_RANDOM_JOB,0x63aed124526aa103, CommandLobbySetAutoMpRandomJob);

		SCR_REGISTER_SECURE(SHUTDOWN_SESSION_CLEARS_AUTO_MULTIPLAYER,0x5c21733602754c43, CommandShutdownSessionClearsAutoMultiplayer);

		SCR_REGISTER_UNUSED(LOBBY_LEAVE_MULTIPLAYER,0xad79adead5142693, CommandDeprecated);
		SCR_REGISTER_UNUSED(LOBBY_AUTO_MULTIPLAYER_CNC,0x924dd713f6e1e39a, CommandDeprecated);
	}

#undef __script_channel
#define __script_channel script
}
