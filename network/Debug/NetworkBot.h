//
// name:        NetworkBot.h
// description: Network bots are used for debug purposes to fill network sessions with a limited number
//              of physical machines. They are essentially fake players in a network game.
// written by:  Daniel Yelland
//

#ifndef NETWORK_BOT_H
#define NETWORK_BOT_H

#include "fwnet/netbot.h"
#include "fwtl/pool.h"
#include "net/status.h"
#include "rline/rl.h"
#include "rline/rlgamerinfo.h"
#include "scene/RegdRefTypes.h"

#include "Network/Players/NetGamePlayer.h"

#if ENABLE_NETWORK_BOTS

namespace rage
{
	class rlSessionInfo;
}

class CNetGamePlayer;
class CPed;
class CRelationshipGroup;

class CNetworkBot : public netBot
{
public:

	static const unsigned int MAX_NETWORK_BOTS = 31;

	CNetworkBot(unsigned botIndex, const rlGamerInfo *gamerInfo = 0);
	~CNetworkBot();

	static void Init();
	static void Shutdown();
	static void UpdateAllBots();

    static void OnPlayerHasJoined(const netPlayer &player);
    static void OnPlayerHasLeft(const netPlayer &player);

	static int AddLocalNetworkBot();
	static int AddRemoteNetworkBot(const rlGamerInfo &gamerInfo);
	static void RemoveNetworkBot(CNetworkBot *networkBot);
	static CNetworkBot *GetNetworkBot(int botIndex);
	static CNetworkBot *GetNetworkBot(const rlGamerInfo &gamerInfo);
	static CNetworkBot *GetNetworkBot(const CNetGamePlayer &netGamePlayer, unsigned localIndex);

	static unsigned int GetNumberOfActiveBots() { return ms_numActiveBots; }

	static int GetBotIndex(const scrThreadId threadId);

	CNetGamePlayer *GetNetPlayer() { return static_cast<CNetGamePlayer *>(netBot::GetNetPlayer()); }
	void SetNetPlayer(netPlayer *netGamePlayer);

	unsigned GetPendingBubbleID() const { return m_PendingBubbleID; }
	void SetPendingBubbleID(unsigned bubbleID) { m_PendingBubbleID = bubbleID; }

	bool GetLocalIndex(unsigned &localIndex) const;
	bool GetScriptId(scrThreadId& threadId) const { threadId = m_NetworkScriptId; return (0 != threadId); }
	const char*  GetScriptName() const;

	bool JoinMatch(const rlSessionInfo& sessionInfo,
		const rlSlotType slotType);

	bool LeaveMatch();

    void ToggleRelationshipGroup();

	void StartCombatMode();
	void StopCombatMode();

	void Update();

	bool  LaunchScript(const char* scriptName);
	bool  StopLocalScript();
	bool  PauseLocalScript();
	bool  ContinueLocalScript();

	void  ProcessRespawning();

#if __BANK
	static void AddDebugWidgets();
	static void DisplayDebugText();
#endif

private:

	enum
	{
		BOT_STATE_IN_COMBAT = BOT_STATE_USER
	};

    virtual const char *GetStateName(unsigned state) const;

	CNetworkBot();
	CNetworkBot(const CNetworkBot &);

	CNetworkBot &operator=(const CNetworkBot &);

	void CreateNetworkBotPlayerPed();

	void  ProcessCombat();
	CPed *FindCombatTarget();

#if __BANK
	static void UpdateCombos();
#endif // __BANK

	static unsigned int ms_numActiveBots;
	static CNetworkBot *ms_networkBots[MAX_NETWORK_BOTS];

    unsigned            m_TimeOfDeath;
	unsigned            m_PendingBubbleID;
	RegdPed             m_CombatTarget;
	CRelationshipGroup *m_RelationshipGroup;
	scrThreadId         m_NetworkScriptId;
};

#endif // ENABLE_NETWORK_BOTS

#endif // NETWORK_BOT_H
