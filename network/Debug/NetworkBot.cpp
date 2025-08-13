//
// name:        NetworkBot.cpp
// description: Network bots are used for debug purposes to fill network sessions with a limited number
//              of physical machines. They are essentially fake players in a network game.
// written by:  Daniel Yelland
//
NETWORK_OPTIMISATIONS()

#include "NetworkBot.h"

#if ENABLE_NETWORK_BOTS

#include "grcore/debugdraw.h"
#include "net/nethardware.h"
#include "string/stringhash.h"
#include "fwnet/netchannel.h"
#include "Network/network.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Roaming/RoamingBubbleMgr.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkSessionMessages.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/Relationships.h"
#include "Scene/World/GameWorld.h"
#include "streaming/streaming.h"			// For CStreaming::HasObjectLoaded(), etc.
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "script/script.h"
#include "script/gta_thread.h"
#include "script/streamedscripts.h"
#include "Weapons/inventory/PedInventoryLoadOut.h"

RAGE_DEFINE_SUBCHANNEL(net, bots, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_bots

unsigned int CNetworkBot::ms_numActiveBots = 0;
CNetworkBot *CNetworkBot::ms_networkBots[CNetworkBot::MAX_NETWORK_BOTS];

#if __BANK
static bool s_displayBotsInfo = false;

static int s_CurrentBotScript = 0;
static int s_CurrentScript = 0;
static int s_CurrentBot  = 0;
static int s_CurrentTask = 0;
static const unsigned NUM_BOT_TASKS = 3;
static unsigned s_NumBots = 0;

static Vector3  s_DebugBotWarpTarget(VEC3_ZERO);
static float    s_DebugBotWarpTargetHeading = 0.0f;
static bool     sDebugBotInvincible         = true;
static unsigned s_DebugBotWantedLevel       = 0;

enum BOT_TASKS
{
	E_BOT_TASK_WANDER,
	E_BOT_TASK_PATROL,
	E_BOT_TASK_COMBAT
};

static const char *s_TaskNames[NUM_BOT_TASKS] =
{
	"Wander",
	"Patrol",
	"Combat"
};

static const char* s_BotScriptNames[] =
{
	 "net_bot_simplebrain"
};
static const unsigned NUM_BOT_SCRIPTS = sizeof(s_BotScriptNames) / sizeof(s_BotScriptNames[0]);

static const char *s_BotNames[CNetworkBot::MAX_NETWORK_BOTS];

static bkCombo *s_BotCombo = 0;
static bkCombo *s_ScriptCombo = 0;

#endif // __BANK

static const char *NETWORK_BOT_MODEL_NAME = "mp_m_freemode_01";
static bool s_AsyncHeadBlend = false;

CNetworkBot::CNetworkBot(unsigned botIndex, const rlGamerInfo *gamerInfo) :
netBot(botIndex, gamerInfo)
{
	gnetAssert(botIndex < MAX_NETWORK_BOTS);

    m_TimeOfDeath       = 0;
	m_PendingBubbleID   = INVALID_BUBBLE_ID;
	m_CombatTarget      = 0;
	m_RelationshipGroup = CRelationshipManager::AddRelationshipGroup(atHashString(GetGamerInfo().GetName()), RT_mission);
	m_NetworkScriptId   = THREAD_INVALID;
	gnetAssert(m_RelationshipGroup);
}

CNetworkBot::~CNetworkBot()
{
	StopLocalScript();

	if(m_RelationshipGroup)
	{
		CRelationshipManager::RemoveRelationshipGroup(m_RelationshipGroup);
	}
}

void CNetworkBot::Init()
{
	for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		ms_networkBots[index] = 0;
	}

	ms_numActiveBots = 0;
}

void CNetworkBot::Shutdown()
{
	for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index])
		{
			{
				sysMemAutoUseDebugMemory memory;
				delete ms_networkBots[index];
			}

			ms_networkBots[index] = 0;
		}
	}

	ms_numActiveBots = 0;
}

void CNetworkBot::UpdateAllBots()
{
	for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index])
		{
			ms_networkBots[index]->Update();
		}
	}
}

void CNetworkBot::OnPlayerHasJoined(const netPlayer &player)
{
    for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index])
		{
			ms_networkBots[index]->PlayerHasJoined(player);
		}
	}
}

void CNetworkBot::OnPlayerHasLeft(const netPlayer &player)
{
    for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index])
		{
			ms_networkBots[index]->PlayerHasLeft(player);
		}
	}
}

int CNetworkBot::AddLocalNetworkBot()
{
	int botIndex = -1;

	if(ms_numActiveBots < MAX_NETWORK_BOTS)
	{
		for(unsigned int index = 0; index < MAX_NETWORK_BOTS && (botIndex == -1); index++)
		{
			if(ms_networkBots[index] == 0)
			{
				{
					sysMemAutoUseDebugMemory memory;
					ms_networkBots[index] = rage_new CNetworkBot(index);
				}				
				botIndex = index;

				ms_networkBots[index]->JoinMatch(CNetwork::GetNetworkSession().GetSnSession().GetSessionInfo(), RL_SLOT_PUBLIC);
			}
		}

		gnetAssert(botIndex != -1);
		ms_numActiveBots++;
	}

#if __BANK
	UpdateCombos();
#endif // __BANK

	return botIndex;
}

int CNetworkBot::AddRemoteNetworkBot(const rlGamerInfo &gamerInfo)
{
	int botIndex = -1;

	if(ms_numActiveBots < MAX_NETWORK_BOTS)
	{
		for(unsigned int index = 0; index < MAX_NETWORK_BOTS && (botIndex == -1); index++)
		{
			if(ms_networkBots[index] == 0)
			{
				{
					sysMemAutoUseDebugMemory memory;
					ms_networkBots[index] = rage_new CNetworkBot(index, &gamerInfo);
				}				
				botIndex = index;
			}
		}

		gnetAssert(botIndex != -1);
		ms_numActiveBots++;
	}

#if __BANK
	UpdateCombos();
#endif // __BANK

	return botIndex;
}

void CNetworkBot::RemoveNetworkBot(CNetworkBot *networkBot)
{
	if(networkBot)
	{
		bool removed = false;

		for(unsigned int index = 0; index < MAX_NETWORK_BOTS && !removed; index++)
		{
			if(ms_networkBots[index] == networkBot)
			{
				ms_networkBots[index] = 0;
				removed = true;
			}
		}

		networkBot->StopLocalScript();

		{
			sysMemAutoUseDebugMemory memory;
			delete networkBot;
		}		

		if(gnetVerify(ms_numActiveBots > 0))
		{
			ms_numActiveBots--;
		}
	}

#if __BANK
	UpdateCombos();
#endif // __BANK
}

CNetworkBot *CNetworkBot::GetNetworkBot(int botIndex)
{
	gnetAssert(botIndex >= 0);
	gnetAssert(botIndex <  MAX_NETWORK_BOTS);

	CNetworkBot *networkBot = 0;

	if(botIndex >= 0 && botIndex < MAX_NETWORK_BOTS)
	{
		networkBot = ms_networkBots[botIndex];
	}

	return networkBot;
}

CNetworkBot *CNetworkBot::GetNetworkBot(const rlGamerInfo &gamerInfo)
{
	for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index] && ms_networkBots[index]->GetGamerInfo() == gamerInfo)
		{
			return ms_networkBots[index];
		}
	}

	return 0;
}

CNetworkBot *CNetworkBot::GetNetworkBot(const CNetGamePlayer &netGamePlayer, unsigned localIndex)
{
	CNetworkBot *networkBot = 0;

	unsigned botLocalIndex = 1; // the local player will be at local index 0, bots start at local index 1

	for(unsigned int index = 0; index < MAX_NETWORK_BOTS && !networkBot; index++)
	{
		if(ms_networkBots[index] && ms_networkBots[index]->GetGamerInfo().GetPeerInfo() == netGamePlayer.GetGamerInfo().GetPeerInfo())
		{
			if(botLocalIndex == localIndex)
			{
				networkBot = ms_networkBots[index];
			}

			botLocalIndex++;
		}
	}

	return networkBot;
}

int CNetworkBot::GetBotIndex(const scrThreadId threadId)
{
	for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		scrThreadId threadIdTmp = THREAD_INVALID;
		if(ms_networkBots[index] && ms_networkBots[index]->GetScriptId(threadIdTmp) && threadIdTmp == threadId)
		{
			return index;
		}
	}

	return -1;
}


void CNetworkBot::SetNetPlayer(netPlayer *netGamePlayer)
{
	gnetAssertf(!netGamePlayer || dynamic_cast<CNetGamePlayer *>(netGamePlayer), "Trying to set a player of an invalid type for a network bot!");

	netBot::SetNetPlayer(netGamePlayer);

    if(netGamePlayer)
    {
        if(m_PendingBubbleID != INVALID_BUBBLE_ID)
        {
		    CRoamingBubbleMemberInfo memberInfo(m_PendingBubbleID, INVALID_BUBBLE_PLAYER_ID);

            CNetwork::GetRoamingBubbleMgr().AddPlayerToBubble(GetNetPlayer(), memberInfo);

            m_PendingBubbleID = INVALID_BUBBLE_ID;
        }

        if(netGamePlayer->IsLocal())
        {
            SetActive(true);
        }
    }
}

bool CNetworkBot::GetLocalIndex(unsigned &localIndex) const
{
	bool found = false;

	unsigned botLocalIndex = 1; // the local player will be at local index 0, bots start at local index 1

	for(unsigned int index = 0; index < MAX_NETWORK_BOTS && !found; index++)
	{
		if(ms_networkBots[index] && ms_networkBots[index]->GetGamerInfo().GetPeerInfo() == GetGamerInfo().GetPeerInfo())
		{
			if(ms_networkBots[index] == this)
			{
				localIndex = botLocalIndex;
				found      = true;
			}

			botLocalIndex++;
		}
	}

	return found;
}

bool CNetworkBot::JoinMatch(const rlSessionInfo &sessionInfo,
							const rlSlotType     slotType)
{
	bool success = false;

	// build player data
	CNetGamePlayerDataMsg playerData;
	playerData.Reset(
		CNetwork::GetVersion(),
		netHardware::GetNatType(),
		CNetGamePlayer::NETWORK_BOT,
		MM_GROUP_FREEMODER,
		0,
		CNetwork::GetAssetVerifier(),
		NetworkGameConfig::GetMatchmakingDataHash(),
		POOL_NORMAL,
		CNetwork::GetTimeoutTime(),
		-1,
		NetworkGameConfig::GetAimPreference(),
		RL_INVALID_CLAN_ID,
		0,
		0,
		NetworkGameConfig::GetMatchmakingRegion());

	// build join request
	CMsgJoinRequest joinRqst;
	joinRqst.Reset(JoinFlags::JF_Default, playerData);

	u8 playerData[256];
	unsigned playerDataSize = 0;
	success = gnetVerify(joinRqst.Export(playerData, sizeof(playerData), &playerDataSize));

	if(success)
	{
		if(CNetwork::GetNetworkSession().GetSnSession().IsHost())
		{
			success = CNetwork::GetNetworkSession().GetSnSession().JoinLocalBot(GetGamerInfo(),
				sessionInfo,
				NetworkInterface::GetNetworkMode(),
				slotType,
				playerData,
				playerDataSize,
				&GetStatus());
		}
		else
		{
			if(success)
			{
				success = CNetwork::GetNetworkSession().GetSnSession().JoinRemoteBot(GetGamerInfo(),
					sessionInfo,
					NetworkInterface::GetNetworkMode(),
					slotType,
					playerData,
					playerDataSize,
					&GetStatus());
			}
		}
	}

	SetState(BOT_STATE_JOINING);

	return success;
}

bool CNetworkBot::LeaveMatch()
{
	bool success = false;

	if(IsInMatch() && GetGamerInfo().IsLocal())
	{
		CNetGamePlayer* gamer = GetNetPlayer();
		if (gamer && AssertVerify(gamer->GetPlayerPed()))
		{
			CPedFactory::GetFactory()->DestroyPlayerPed(gamer->GetPlayerPed());
		}

		success = CNetwork::GetNetworkSession().GetSnSession().LeaveLocalBot(GetGamerInfo().GetGamerHandle(), &GetStatus());

		SetState(BOT_STATE_LEAVING);
	}

	return success;
}

void CNetworkBot::ToggleRelationshipGroup()
{
    CNetGamePlayer *gamer     = GetNetPlayer();
    CPed           *playerPed = gamer ? gamer->GetPlayerPed() : 0;

    if(playerPed)
    {
        if(m_RelationshipGroup)
        {
            if(playerPed->GetPedIntelligence()->GetRelationshipGroup() != m_RelationshipGroup)
            {
                playerPed->GetPedIntelligence()->SetRelationshipGroup(m_RelationshipGroup);
            }
            else
            {
                CPed *localPlayerPed = FindPlayerPed();

                if(localPlayerPed)
                {
                    playerPed->GetPedIntelligence()->SetRelationshipGroup(localPlayerPed->GetPedIntelligence()->GetRelationshipGroup());
                }
            }
        }
    }
}

void CNetworkBot::StartCombatMode()
{
	if(gnetVerify(IsInMatch()))
	{
		SetState(BOT_STATE_IN_COMBAT);
	}
}

void CNetworkBot::StopCombatMode()
{
	if(gnetVerify(IsInMatch()))
	{
		SetState(BOT_STATE_IN_MATCH);
	}
}

void CNetworkBot::Update()
{
	netBot::Update();

	ProcessRespawning();

	switch(GetState())
	{
	case BOT_STATE_IDLE:
		break;
	case BOT_STATE_JOINING:
		if(!GetStatus().Pending())
		{
			if(GetStatus().Succeeded())
			{
				SetState(BOT_STATE_CREATE_PLAYER);
			}
			else
			{
				SetState(BOT_STATE_IDLE);
			}
		}
		break;
	case BOT_STATE_CREATE_PLAYER:
		{
			fwModelId modelId = CModelInfo::GetModelIdFromName(NETWORK_BOT_MODEL_NAME);

			if (gnetVerify(modelId.IsValid()))
			{
				if(!CModelInfo::HaveAssetsLoaded(modelId))
				{
					CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				}
				else
				{
					if(GetNetPlayer() && GetNetPlayer()->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
					{
						CreateNetworkBotPlayerPed();
						SetState(BOT_STATE_IN_MATCH);
					}
				}
			}
			else
			{
				CNetworkBot::LeaveMatch();
			}
		}
		break;
	case BOT_STATE_LEAVING:
		if(!GetStatus().Pending())
		{
			if(GetStatus().Succeeded())
			{
				RemoveNetworkBot(this);
			}
			else
			{
				gnetAssertf(0, "Network bot failed to leave the session!");
				SetState(BOT_STATE_IDLE);
			}
		}
		break;
	case BOT_STATE_IN_MATCH:
		break;
	case BOT_STATE_IN_COMBAT:
		ProcessCombat();
		break;
	default:
		break;
	}
}

const char*  CNetworkBot::GetScriptName() const 
{
#if !__FINAL
	GtaThread* pThread = GtaThread::GetThreadWithThreadId(m_NetworkScriptId);
	if(pThread && pThread->GetScriptName())
	{
		return pThread->GetScriptName();
	}
#endif // !__FINAL

	return netBot::GetScriptName();
}

bool  CNetworkBot::LaunchScript(const char* scriptName)
{
	bool ret = false;

	if(!gnetVerifyf(GetGamerInfo().IsLocal(), "Failed to retrieve local index for network bot"))
		return ret;

	if (!gnetVerifyf(scriptName, "Invalid Script Name."))
		return ret;

#if !__FINAL

	// Make sure we stop any running scripts.
	if (gnetVerifyf(StopLocalScript(), "Network Bot Failed to Stop current script name=\"%s\" id=\"%d\".", GetScriptName(), m_NetworkScriptId))
	{
		const strLocalIndex scriptId = g_StreamedScripts.FindSlot(scriptName);
		if(gnetVerifyf(scriptId != -1, "Script doesn't exist \"%s\"", scriptName))
		{
			g_StreamedScripts.StreamingRequest(scriptId, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);

			CStreaming::LoadAllRequestedObjects();

			if (gnetVerifyf(g_StreamedScripts.HasObjectLoaded(scriptId), "Failed to stream in script \"%s\"", scriptName))
			{
				// Make sure we manage to start a new script.
				m_NetworkScriptId = CTheScripts::GtaStartNewThreadOverride(scriptName, NULL, 0, GtaThread::GetNetworkBotStackSize());
				if (gnetVerifyf(m_NetworkScriptId, "Network Bot Failed to Start script name=\"%s\"", scriptName))
				{
					gnetDebug1("Network Bot Start script name=\"%s\"", scriptName);

					ret = true;
				}
			}
		}
	}

#endif // !__FINAL

	return ret;
}


bool  CNetworkBot::StopLocalScript( )
{
	bool ret = true;

#if !__FINAL

	if (m_NetworkScriptId)
	{
		ret = false;

		if(gnetVerifyf(GetGamerInfo().IsLocal(), "Network Bot name=\"%s\" Failed to Stop script name=\"%s\" id=\"%d\" - Bot is Not Local", GetGamerInfo().GetName(), GetScriptName(), m_NetworkScriptId))
		{
			GtaThread* pThread = GtaThread::GetThreadWithThreadId(m_NetworkScriptId);
			if(pThread)
			{
				gnetDebug1("Network Bot name=\"%s\" Stop script name=\"%s\"", GetGamerInfo().GetName(), GetScriptName());

				pThread->Kill();
			}

			m_NetworkScriptId = THREAD_INVALID;

			ret = true;
		}
	}

#endif // !__FINAL

	return ret;
}

bool  CNetworkBot::PauseLocalScript( )
{
	bool ret = true;

#if !__FINAL

	if (m_NetworkScriptId)
	{
		ret = false;

		if(gnetVerifyf(GetGamerInfo().IsLocal(), "Network Bot name=\"%s\" Failed to Pause script name=\"%s\" id=\"%d\" - Bot is Not Local", GetGamerInfo().GetName(), GetScriptName(), m_NetworkScriptId))
		{
			GtaThread* pThread = GtaThread::GetThreadWithThreadId(m_NetworkScriptId);
			if(gnetVerifyf(pThread, "Network Bot name=\"%s\" Failed to Pause script name=\"%s\" id=\"%d\" - Invalid Thread ID", GetGamerInfo().GetName(), GetScriptName(), m_NetworkScriptId))
			{
				gnetDebug1("Network Bot name=\"%s\" Pause script name=\"%s\"", GetGamerInfo().GetName(), GetScriptName());

				pThread->Pause();

				ret = true;
			}
		}
	}

#endif // !__FINAL

	return ret;
}

bool  CNetworkBot::ContinueLocalScript( )
{
	bool ret = true;

#if !__FINAL

	if (m_NetworkScriptId)
	{
		ret = false;

		if(gnetVerifyf(GetGamerInfo().IsLocal(), "Network Bot name=\"%s\" Failed to Continue script name=\"%s\" id=\"%d\" - Bot is Not Local", GetGamerInfo().GetName(), GetScriptName(), m_NetworkScriptId))
		{
			GtaThread* pThread = GtaThread::GetThreadWithThreadId(m_NetworkScriptId);
			if(gnetVerifyf(pThread, "Network Bot name=\"%s\" Failed to Continue script name=\"%s\" id=\"%d\" - Invalid Thread ID", GetGamerInfo().GetName(), GetScriptName(), m_NetworkScriptId))
			{
				gnetDebug1("Network Bot name=\"%s\" Continue script name=\"%s\"", GetGamerInfo().GetName(), GetScriptName());

				pThread->Continue();

				ret = true;
			}
		}
	}

#endif // !__FINAL

	return ret;
}

#if __BANK

static CNetworkBot *GetCurrentlySelectedNetworkBot()
{
	CNetworkBot *networkBot = 0;

	int botIndex = 0;

	for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS && !networkBot; index++)
	{
		CNetworkBot *currentBot = CNetworkBot::GetNetworkBot(index);

		if(currentBot && currentBot->GetGamerInfo().IsLocal())
		{
			if(botIndex == s_CurrentBot)
			{
				networkBot = currentBot;
			}

			botIndex++;
		}
	}

	return networkBot;
}

static void NetworkBank_AddNetworkBot()
{
	gnetDebug2("Adding network bot via widgets");
    CNetworkBot::AddLocalNetworkBot();
}

static void NetworkBank_RemoveNetworkBot()
{
    gnetDebug2("Removing network bot via widgets");

	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		if(networkBot->IsInMatch())
		{
			networkBot->LeaveMatch();
		}
		else
		{
			CNetworkBot::RemoveNetworkBot(networkBot);
		}
	}
}

static void NetworkBank_ToggleNetworkBotRelationships()
{
    CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		networkBot->ToggleRelationshipGroup();
	}
}

static void NetworkBank_WarpNetworkBotToLocalPlayer()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		CNetGamePlayer *netGamePlayer  = networkBot->GetNetPlayer();
		CPed           *localPlayerPed = CGameWorld::FindLocalPlayer();

		if (localPlayerPed && !localPlayerPed->IsDead() && netGamePlayer && netGamePlayer->GetPlayerPed())
		{
			netGamePlayer->GetPlayerPed()->SetPosition(VEC3V_TO_VECTOR3(localPlayerPed->GetTransform().GetPosition()), true, true, true);
		}
	}
}

static void NetworkBank_WarpNetworkBotToPositionAndHeading()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();

		if (netGamePlayer && netGamePlayer->GetPlayerPed())
		{
			netGamePlayer->GetPlayerPed()->Teleport(s_DebugBotWarpTarget, s_DebugBotWarpTargetHeading);
		}
	}
}

static void NetworkBank_StopAllBotScript()
{
	for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
	{
		CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

		if(networkBot && networkBot->GetGamerInfo().IsLocal())
		{
			CNetGamePlayer* netGamePlayer = networkBot->GetNetPlayer();
			CPed* playerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

			if(playerPed)
			{
				playerPed->GetPedIntelligence()->ClearTasks();

				networkBot->StopCombatMode();

				networkBot->StopLocalScript();
			}
		}
	}
}

static void NetworkBank_StopBotScript()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		CNetGamePlayer* netGamePlayer = networkBot->GetNetPlayer();
		CPed* playerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

		if(playerPed)
		{
			playerPed->GetPedIntelligence()->ClearTasks();

			networkBot->StopCombatMode();

			networkBot->StopLocalScript();
		}
	}
}

static void NetworkBank_PauseBotScript()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		CNetGamePlayer* netGamePlayer = networkBot->GetNetPlayer();
		CPed* playerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

		if(playerPed)
		{
			playerPed->GetPedIntelligence()->ClearTasks();

			networkBot->StopCombatMode();

			networkBot->PauseLocalScript();
		}
	}
}

static void NetworkBank_ContinueBotScript()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		CNetGamePlayer* netGamePlayer = networkBot->GetNetPlayer();
		CPed* playerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

		if(playerPed)
		{
			playerPed->GetPedIntelligence()->ClearTasks();

			networkBot->StopCombatMode();

			networkBot->ContinueLocalScript();
		}
	}
}

static void GiveNetworkBotCurrentlySelectedScript(CNetworkBot* networkBot)
{
	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		CNetGamePlayer* netGamePlayer = networkBot->GetNetPlayer();
		CPed* playerPed = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

		if(playerPed)
		{
			playerPed->GetPedIntelligence()->ClearTasks();

			networkBot->StopCombatMode();

			networkBot->LaunchScript(s_BotScriptNames[s_CurrentScript]);
		}
	}
}

static void GiveNetworkBotCurrentlySelectedTask(CNetworkBot *networkBot)
{
	if(networkBot)
	{
		CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();
		CPed     *playerPed     = netGamePlayer ? netGamePlayer->GetPlayerPed() : 0;

		if(playerPed)
		{
			playerPed->GetPedIntelligence()->ClearTasks();

			networkBot->StopCombatMode();

			// get the currently selected task
			switch(s_CurrentTask)
			{
			case E_BOT_TASK_WANDER:
				{
					// Used to do this
					//	CTaskWander * pTaskWander = (CTaskWander*)playerPed->ComputeWanderTask(*playerPed);
					//	pTaskWander->SetDirection(playerPed->GetCurrentHeading());
					//	pTaskWander->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
					// but it's not safe to assume that we get a CTaskWander back from ComputeWanderTask().
					// In the cases where we do, ComputeWanderTask() already initializes it like we did here.
					// Note: we could also just do rage_new CTaskWander here, if that's waht we really want.
					CTask* pTask = playerPed->ComputeWanderTask(*playerPed);
					playerPed->GetPedIntelligence()->AddTaskDefault(pTask);
				}
				break;
			case E_BOT_TASK_PATROL:
				{
					Vector3 startPos(-8.0f, -2.5f, 1.5f);
					Vector3 endPos(-7.2f, -72.0f, 1.0f);
					CTaskSequenceList *sequenceTask = rage_new CTaskSequenceList();
					sequenceTask->AddTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, startPos)));
					sequenceTask->AddTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, endPos)));
					sequenceTask->SetRepeatMode(CTaskSequenceList::REPEAT_FOREVER);
					CTaskUseSequence* pTask = rage_new CTaskUseSequence(*sequenceTask);
					playerPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY, true);
				}
				break;
			case E_BOT_TASK_COMBAT:
				{
					networkBot->StartCombatMode();
				}
				break;

			default:
				break;
			};
		}
	}
}

static void NetworkBank_GiveNetworkBotScript()
{
	// get the currently selected network bot
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		GiveNetworkBotCurrentlySelectedScript(networkBot);
	}
}

static void NetworkBank_GiveAllLocalNetworkBotsScript()
{
	for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
	{
		CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

		if(networkBot && networkBot->GetGamerInfo().IsLocal())
		{
			GiveNetworkBotCurrentlySelectedScript(networkBot);
		}
	}
}

static void NetworkBank_UpdateScriptCombo()
{
	if (s_ScriptCombo)
	{
		s_ScriptCombo->UpdateCombo("Running Scripts", &s_CurrentScript, CGameScriptHandlerMgr::ms_NumHandlers, (const char **)CGameScriptHandlerMgr::ms_Handlers, NullCB);
	}
}

static void NetworkBank_AddNetworkBotToScript()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		const CGameScriptHandler* pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerFromComboName(CTheScripts::GetScriptHandlerMgr().ms_Handlers[s_CurrentScript]);

		if (pScriptHandler && networkBot->GetNetPlayer() && gnetVerifyf(pScriptHandler->GetNetworkComponent(), "Can't add bot to script - it is not networked"))
		{
			pScriptHandler->GetNetworkComponent()->AddPlayerBot(*networkBot->GetNetPlayer());
		}
	}
}

static void NetworkBank_AddAllNetworkBotsToScript()
{
	const CGameScriptHandler* pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerFromComboName(CTheScripts::GetScriptHandlerMgr().ms_Handlers[s_CurrentScript]);

	if (pScriptHandler && gnetVerifyf(pScriptHandler->GetNetworkComponent(), "Can't add bots to script - it is not networked"))
	{
		for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
		{
			CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

			if(networkBot && networkBot->GetNetPlayer() && networkBot->GetGamerInfo().IsLocal())
			{
				pScriptHandler->GetNetworkComponent()->AddPlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
}

static void NetworkBank_RemoveNetworkBotFromScript()
{
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot && networkBot->GetGamerInfo().IsLocal())
	{
		const CGameScriptHandler* pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerFromComboName(CTheScripts::GetScriptHandlerMgr().ms_Handlers[s_CurrentScript]);

		if (pScriptHandler && pScriptHandler->GetNetworkComponent())
		{
			pScriptHandler->GetNetworkComponent()->RemovePlayerBot(*networkBot->GetNetPlayer());
		}
	}
}

static void NetworkBank_RemoveAllNetworkBotsFromScript()
{
	const CGameScriptHandler* pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerFromComboName(CTheScripts::GetScriptHandlerMgr().ms_Handlers[s_CurrentScript]);

	if (pScriptHandler && pScriptHandler->GetNetworkComponent())
	{
		for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
		{
			CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

			if(networkBot && networkBot->GetGamerInfo().IsLocal())
			{
				pScriptHandler->GetNetworkComponent()->RemovePlayerBot(*networkBot->GetNetPlayer());
			}
		}
	}
}

static void NetworkBank_ToggleNetworkBotInvincible()
{
    CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();

		if (netGamePlayer && netGamePlayer->GetPlayerPed())
		{
            netGamePlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything = !netGamePlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything;
        }
    }
}

static void NetworkBank_ToggleAllLocalNetworkBotsInvincible()
{
    for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
	{
		CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

        if(networkBot)
		{
            CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();

            if(netGamePlayer && netGamePlayer->GetPlayerPed())
            {
                netGamePlayer->GetPlayerPed()->m_nPhysicalFlags.bNotDamagedByAnything = sDebugBotInvincible;
            }
        }
    }

    sDebugBotInvincible = !sDebugBotInvincible;
}

static void NetworkBank_GiveNetworkBotWantedLevel()
{
    CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();

		if (netGamePlayer && netGamePlayer->GetPlayerPed())
		{
            CWanted &wanted = netGamePlayer->GetPlayerPed()->GetPlayerInfo()->GetWanted();

            wanted.m_WantedLevel = static_cast<eWantedLevel>(s_DebugBotWantedLevel);
        }
    }
}

static void NetworkBank_GiveAllLocalNetworkBotsWantedLevel()
{
    for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
	{
		CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

        if(networkBot)
		{
            CNetGamePlayer *netGamePlayer = networkBot->GetNetPlayer();

		    if (netGamePlayer && netGamePlayer->GetPlayerPed())
		    {
                CWanted &wanted = netGamePlayer->GetPlayerPed()->GetPlayerInfo()->GetWanted();

                wanted.m_WantedLevel = static_cast<eWantedLevel>(s_DebugBotWantedLevel);
            }
        }
    }
}

static void NetworkBank_GiveNetworkBotTask()
{
	// get the currently selected network bot
	CNetworkBot *networkBot = GetCurrentlySelectedNetworkBot();

	if(networkBot)
	{
		GiveNetworkBotCurrentlySelectedTask(networkBot);
	}
}

static void NetworkBank_GiveAllLocalNetworkBotsTask()
{
	for(unsigned index = 0; index < CNetworkBot::MAX_NETWORK_BOTS; index++)
	{
		CNetworkBot *networkBot = CNetworkBot::GetNetworkBot(index);

		if(networkBot && networkBot->GetGamerInfo().IsLocal())
		{
			GiveNetworkBotCurrentlySelectedTask(networkBot);
		}
	}
}

void CNetworkBot::AddDebugWidgets()
{
	bkBank *bank = BANKMGR.FindBank("Network");

	if(gnetVerifyf(bank, "Unable to find network bank!"))
	{
		bank->PushGroup("Debug Network Bots", false);
			bank->AddToggle("Display bots info",                   &s_displayBotsInfo);
			bank->AddToggle("Async head blends",                   &s_AsyncHeadBlend);
			bank->AddButton("Add Network Bot",                      datCallback(NetworkBank_AddNetworkBot));
			bank->AddButton("Remove Network Bot",                   datCallback(NetworkBank_RemoveNetworkBot));
			s_BotCombo = bank->AddCombo("Bot Name", &s_CurrentBot, s_NumBots, (const char **)s_BotNames, NullCB);
			bank->AddButton("Toggle Network Bot Relationship",      datCallback(NetworkBank_ToggleNetworkBotRelationships));
			bank->AddButton("Warp Network Bot To Local Player",     datCallback(NetworkBank_WarpNetworkBotToLocalPlayer));

            bank->AddSlider("Warp Target X",       &s_DebugBotWarpTarget.x, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.01f);
            bank->AddSlider("Warp Target Y",       &s_DebugBotWarpTarget.y, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 0.01f);
            bank->AddSlider("Warp Target Z",       &s_DebugBotWarpTarget.z, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.01f);
            bank->AddSlider("Warp Target Heading", &s_DebugBotWarpTargetHeading, 0.0f, TWO_PI, 0.01f);
            bank->AddButton("Warp Network Bot To Position/Heading", datCallback(NetworkBank_WarpNetworkBotToPositionAndHeading));

			bank->PushGroup("Bot Scripts", false);
				bank->AddButton("Stop All Network Bots Scripts.",     datCallback(NetworkBank_StopAllBotScript));
				bank->AddButton("Stop current Network Bot Script.",     datCallback(NetworkBank_StopBotScript));
				bank->AddButton("Pause current Network Bot Script.",    datCallback(NetworkBank_PauseBotScript));
				bank->AddButton("Continue current Network Bot Script.", datCallback(NetworkBank_ContinueBotScript));

				bank->AddCombo("Bot Scripts", &s_CurrentBotScript, NUM_BOT_SCRIPTS, (const char **)s_BotScriptNames, NullCB);
				bank->AddButton("Give Network Bot Script",                datCallback(NetworkBank_GiveNetworkBotScript));
				bank->AddButton("Give All Local Network Bots Script",     datCallback(NetworkBank_GiveAllLocalNetworkBotsScript));
			bank->PopGroup();

			bank->PushGroup("Joining Scripts", false);
				bank->AddButton("Update Script Combo",                datCallback(NetworkBank_UpdateScriptCombo));
				s_ScriptCombo = bank->AddCombo("Running Scripts", &s_CurrentScript, CGameScriptHandlerMgr::ms_NumHandlers, (const char **)CGameScriptHandlerMgr::ms_Handlers, NullCB);
				bank->AddButton("Add Network Bot To Script",                datCallback(NetworkBank_AddNetworkBotToScript));
				bank->AddButton("Add All Network Bots To Script",           datCallback(NetworkBank_AddAllNetworkBotsToScript));
				bank->AddButton("Remove Network Bot From Script",           datCallback(NetworkBank_RemoveNetworkBotFromScript));
				bank->AddButton("Remove All Network Bots From Script",       datCallback(NetworkBank_RemoveAllNetworkBotsFromScript));
			bank->PopGroup();

            bank->PushGroup("Misc", false);
                bank->AddButton("Toggle Network Bot Invincible",             datCallback(NetworkBank_ToggleNetworkBotInvincible));
			    bank->AddButton("Toggle All Local Network Bots Invincible",  datCallback(NetworkBank_ToggleAllLocalNetworkBotsInvincible));
                bank->AddSlider("Wanted Level", &s_DebugBotWantedLevel, 0, 5, 1);
                bank->AddButton("Give Network Bot Wanted Level",            datCallback(NetworkBank_GiveNetworkBotWantedLevel));
				bank->AddButton("Give All Local Network Bots Wanted Level", datCallback(NetworkBank_GiveAllLocalNetworkBotsWantedLevel));
            bank->PopGroup();

			bank->PushGroup("Tasks", false);
				bank->AddCombo("Task", &s_CurrentTask, NUM_BOT_TASKS, (const char **)s_TaskNames, NullCB);
				bank->AddButton("Give Network Bot Task",                datCallback(NetworkBank_GiveNetworkBotTask));
				bank->AddButton("Give All Local Network Bots Task",     datCallback(NetworkBank_GiveAllLocalNetworkBotsTask));
			bank->PopGroup();
		bank->PopGroup();
	}
}

void CNetworkBot::DisplayDebugText()
{
	if(s_displayBotsInfo)
	{
		for(unsigned int index = 0; index < MAX_NETWORK_BOTS; index++)
		{
			CNetworkBot *networkBot = ms_networkBots[index];

			if(networkBot)
			{
				grcDebugDraw::AddDebugOutput("%s : (%llx) Is %sin match : %s", networkBot->GetGamerInfo().GetName(), networkBot->GetGamerInfo().GetGamerId().GetId(), networkBot->IsInMatch() ? "" : "not ", networkBot->GetScriptName());
			}
		}
	}
}

#endif

const char *CNetworkBot::GetStateName(unsigned state) const
{
    switch(state)
    {
    case BOT_STATE_IN_COMBAT:
        return "BOT_STATE_IN_COMBAT";
    default:
        return netBot::GetStateName(state);
    }
}

void CNetworkBot::CreateNetworkBotPlayerPed()
{
	CPed *localPlayer = CGameWorld::FindLocalPlayer();

	if(AssertVerify(localPlayer) && AssertVerify(GetNetPlayer()))
	{
		// create the network bot player ped close to the local player
		fwModelId modelId = CModelInfo::GetModelIdFromName(NETWORK_BOT_MODEL_NAME);

		gnetAssert(CModelInfo::HaveAssetsLoaded(modelId));

		Vector3 newPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetB()) * 2.0f);

		Matrix34 tempMat;
		tempMat.Identity();
		tempMat.d = newPos;

		const CControlledByInfo networkPlayerControl(true, true);
		CPed* playerPed = CPedFactory::GetFactory()->CreatePlayerPed(networkPlayerControl, modelId.GetModelIndex(), &tempMat, GetNetPlayer()->GetPlayerInfo());

		if (AssertVerify(playerPed))
		{
			CGameWorld::AddPlayerToWorld(playerPed, VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()));

            playerPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);

			netGameObjectWrapper<CDynamicEntity> wrapper(playerPed);

			NetworkInterface::GetObjectManager().RegisterNetworkBotGameObject(GetNetPlayer()->GetPhysicalPlayerIndex(), &wrapper, 0, netObject::GLOBALFLAG_PERSISTENTOWNER|CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|netObject::GLOBALFLAG_CLONEALWAYS);

            playerPed->PopTypeSet(POPTYPE_MISSION);
			playerPed->GetPlayerInfo()->SetupPlayerGroup();
			playerPed->GetPedIntelligence()->SetRelationshipGroup(m_RelationshipGroup);
			playerPed->SetRandomHeadBlendData(s_AsyncHeadBlend);
            playerPed->FinalizeHeadBlend();

			CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, playerPed->GetNetworkObject());

			if(netObjPlayer)
			{
				netObjPlayer->SetPlayerDisplayName(GetNetPlayer()->GetLogName());
			}
		}
	}
}

void CNetworkBot::ProcessRespawning()
{
	if(IsInMatch())
	{
		CPed *myPlayerPed = GetNetPlayer() ? GetNetPlayer()->GetPlayerPed() : 0;

		if(myPlayerPed && myPlayerPed->GetIsDeadOrDying())
		{
            if(m_TimeOfDeath == 0)
            {
                m_TimeOfDeath = NetworkInterface::GetNetworkTime();
            }
            else
            {
                const unsigned RESPAWN_TIME = 5000;

                unsigned timeSinceDeath = NetworkInterface::GetNetworkTime() - m_TimeOfDeath;

                if(timeSinceDeath > RESPAWN_TIME)
                {
			        myPlayerPed->Resurrect(VEC3V_TO_VECTOR3(myPlayerPed->GetTransform().GetPosition()), myPlayerPed->GetCurrentHeading());

                    m_TimeOfDeath = 0;
                }
            }
		}
	}
}

void CNetworkBot::ProcessCombat()
{
	if(m_CombatTarget == 0 || m_CombatTarget->GetIsDeadOrDying())
	{
		m_CombatTarget = FindCombatTarget();
	}

	CPed *myPlayerPed = GetNetPlayer() ? GetNetPlayer()->GetPlayerPed() : 0;

	if(m_CombatTarget && myPlayerPed)
	{
		Vector3 deltaPos = VEC3V_TO_VECTOR3(myPlayerPed->GetTransform().GetPosition() - m_CombatTarget->GetTransform().GetPosition());

		float distSquared = deltaPos.XYMag2();

		static bank_float SEEK_DISTANCE_WHEN_IDLE_SQUARED      = 20.0f  * 20.0f;
		static bank_float SEEK_DISTANCE_WHEN_IN_COMBAT_SQUARED = 50.0f  * 50.0f;

		bool runningCombatTask = myPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT);
		bool tooFarForCombat   = distSquared > (runningCombatTask ? SEEK_DISTANCE_WHEN_IN_COMBAT_SQUARED : SEEK_DISTANCE_WHEN_IDLE_SQUARED);

		if(tooFarForCombat)
		{
			if(!myPlayerPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD))
			{
				// too far away from other player for combat, chase the player
				TTaskMoveSeekEntityStandard *moveTask = rage_new TTaskMoveSeekEntityStandard(m_CombatTarget);
				((TTaskMoveSeekEntityStandard*)moveTask)->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
				CTask *pTask = rage_new CTaskComplexControlMovement(moveTask);
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask, false, E_PRIORITY_GIVE_PED_TASK);
				myPlayerPed->GetPedIntelligence()->AddEvent(event);
			}
		}
		else
		{
			if(!runningCombatTask)
			{
				// arm the local player if necessary
				if( myPlayerPed->GetWeaponManager() && !myPlayerPed->GetWeaponManager()->GetIsArmedGun() && myPlayerPed->GetInventory() )
				{
                    static const atHashWithStringNotFinal NETWORK_BOT_LOADOUT("LOADOUT_NETWORK_BOT",0xC8923B7F);
                    CPedInventoryLoadOutManager::SetLoadOut(myPlayerPed, NETWORK_BOT_LOADOUT.GetHash());
                    myPlayerPed->GetWeaponManager()->EquipBestWeapon();
				}

				// close enough to attack other player - go for it!
				CTaskCombat *combatTask = rage_new CTaskCombat(m_CombatTarget);
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, combatTask, false, E_PRIORITY_GIVE_PED_TASK);
				myPlayerPed->GetPedIntelligence()->AddEvent(event);

				myPlayerPed->SetBlockingOfNonTemporaryEvents(true);
				myPlayerPed->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(CCombatData::TLR_NeverLoseTarget);
			}
		}
	}
}

CPed *CNetworkBot::FindCombatTarget()
{
	float closestDistSquared = FLT_MAX;
	CPed *closestPlayer      = 0;

	if(GetNetPlayer() && GetNetPlayer()->GetPlayerPed())
	{
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			if(player != GetNetPlayer() && player->GetPlayerPed())
			{
				Vector3 deltaPos = VEC3V_TO_VECTOR3(GetNetPlayer()->GetPlayerPed()->GetTransform().GetPosition() - player->GetPlayerPed()->GetTransform().GetPosition());

				float distSquared = deltaPos.XYMag2();

				if(distSquared < closestDistSquared && !player->GetPlayerPed()->GetIsDeadOrDying())
				{
					closestDistSquared = distSquared;
					closestPlayer      = player->GetPlayerPed();
				}
			}
		}
	}

	return closestPlayer;
}

#if __BANK

void CNetworkBot::UpdateCombos()
{
	// update the bots list
	s_NumBots = 0;

	for(unsigned index = 0; index < MAX_NETWORK_BOTS; index++)
	{
		if(ms_networkBots[index] && ms_networkBots[index]->GetGamerInfo().IsLocal())
		{
			s_BotNames[s_NumBots] = ms_networkBots[index]->GetGamerInfo().GetName();

			s_NumBots++;
		}
	}

	// avoid empty combo boxes (RAG can't handle them)
	if(s_NumBots == 0)
	{
		s_BotNames[0] = "--No Bots--";
		s_NumBots  = 1;
	}

	// update player combo
	if(s_BotCombo)
	{
		s_BotCombo->UpdateCombo("Bot Name", &s_CurrentBot, s_NumBots, (const char **)s_BotNames);
	}

	// update script combo
	if (s_ScriptCombo)
	{
		s_ScriptCombo->UpdateCombo("Running Scripts", &s_CurrentScript, CGameScriptHandlerMgr::ms_NumHandlers, (const char **)CGameScriptHandlerMgr::ms_Handlers, NullCB);
	}
}

#endif // __BANK

#endif // ENABLE_NETWORK_BOTS
