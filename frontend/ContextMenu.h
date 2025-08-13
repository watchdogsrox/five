#ifndef __CONTEXT_MENU_H__
#define __CONTEXT_MENU_H__

//rage
#include "atl/array.h"
#include "atl/singleton.h"
#include "fwnet/nettypes.h"
#include "net/status.h"
#include "parser/macros.h"
#include "rline/rlgamerinfo.h"
#include "rline/clan/rlclancommon.h"
#include "data/base.h"
#include "data/callback.h"

//game
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/UIContexts.h"
#include "Frontend/CMenuBase.h"
#include "Stats/StatsTypes.h"

namespace rage
{
	class parTreeNode;
};

class ClanInviteManager
{
	//Cache off the last X # of requested handles to send crew invites to. This will help prevent spamming network requests, but will be transparent to the user.
	class CachedInvite
	{
	public:
		typedef rlGamerHandle value_type;
		CachedInvite() : mLastTimeMS(sysTimer::GetSystemMsTime()) {}
		CachedInvite(const rlGamerHandle &gamer) : mLastTimeMS(sysTimer::GetSystemMsTime()), mHandle(gamer) {}

		bool operator ==(const CachedInvite &ci) const		{ return mHandle == ci.mHandle; }
		bool operator ==(const value_type &handle) const	{ return mHandle == handle; }

		u32 GetTimeElapsedSinceLastUpdate() const			{ return sysTimer::GetSystemMsTime() - mLastTimeMS; }
		bool IsThrottled(u32 throttleDurationMS) const		{ return GetTimeElapsedSinceLastUpdate() < throttleDurationMS; }

		const value_type &GetHandle() const					{ return mHandle; }
		u32 GetLastTimeMS() const							{ return mLastTimeMS; }

		void SetHandle(const value_type &handle)			{ mHandle = handle; }
		void SetLastTimeMS(u32 timeMS)						{ mLastTimeMS = timeMS; }
		void UpdateLastTimeMS()								{ mLastTimeMS = sysTimer::GetSystemMsTime(); }

	private:
		u32 mLastTimeMS;
		value_type mHandle;
	};

public:
	ClanInviteManager();

	void Update();

	void InviteGamer(const rlGamerHandle &gamer);

private:
	void UpdateInviteCache();

	bool IsIdle();
	bool ShouldDisplayMessage();
	void ResetTimer() { lastUpdateElapsedTime = ShouldDisplayMessage() ? sBlendInTimeInSeconds : 0.0f; }

	bool WasLastSuccessful() { return lastSuccess; }
	const rlGamerHandle &GetLastGamerHandle() {	return lastGamer; }

	bool IsInviteeThrottled(const rlGamerHandle &gamer);
	void AddGamerToInviteCache(const rlGamerHandle &gamer);



	//Control Values
	static BankBool sDBGAlwaysShow;
	static BankBool sDelayIdle;
	static BankUInt32 sThrottleDuration;
	static BankFloat sBlendInTimeInSeconds;
	static BankFloat sBlendOutTimeInSeconds;
	static BankFloat sTimeToDisplayMsgInSeconds;

	atQueue<CachedInvite, 8> inviteCache;
	rlClanId currInviteCachedClan;

	//Cached result info
	netStatus m_netStatus;
	int lastResultCode;
	rlGamerHandle lastGamer;
	bool lastSuccess;

	//Time values
	float lastUpdateElapsedTime;
};

typedef atSingleton<ClanInviteManager> SClanInviteManager;

class CContextMenuOption
{
public:
	atHashWithStringBank text;
	atHashWithStringBank contextOption;
	UIContextList	displayConditions;
	bool bIsSelectable;

	const char* GetString() const;
	const char* GetSubstring() const;
	const int GetType() const;
	const bool GetIsSelectable() const {return bIsSelectable;}

	bool CanShow() const;
	void AddSlot(int depth, int menuId, int index) const;

	void preLoad(parTreeNode* pTreeNode);

	PAR_SIMPLE_PARSABLE;

#if __BANK
public:
	CContextMenuOption()
	{
		++sm_Count;
		sm_Allocs += sizeof(CContextMenuOption);
	}

	~CContextMenuOption()
	{
		--sm_Count;
		sm_Allocs -= sizeof(CContextMenuOption);
	}

	static int sm_Count;
	static int sm_Allocs;
#endif
};

class CContextMenu : public datBase
{
public:
	CContextMenu() : m_bIsOpen(false), m_MenuCB(NULL)
	{
#if __BANK
		++sm_Count;
		sm_Allocs += sizeof(CContextMenu);
	};

	~CContextMenu()
	{
		--sm_Count;
		sm_Allocs -= sizeof(CContextMenu);
#endif
	}


	bool HasOptions() const  { return !contextOptions.empty(); }
	void SetShowHideCB(CMenuBase* newCB) { m_MenuCB = newCB;}
	bool WouldShowMenu() const;
	bool ShowMenu();
	void CloseMenu();
	bool IsOpen() const;
	
	void postLoad();

	// the pane this menu appears on
	const MenuScreenId& GetTriggerMenuId() const { return m_TriggerMenuId; }
	// the id for this context menu
	const MenuScreenId& GetContextMenuId() const { return m_ContextMenuId; }

	static CScaleformMovieWrapper& GetMovie();

protected:
	void SET_MENU();
	void DISPLAY_MENU();
	void CLEAR_MENU();

	void ShowOrHideColumnCB(bool bShow);

	// parsed by XML
	atArray<atHashWithStringBank> contextOptions;
	MenuScreenId		m_TriggerMenuId;
	MenuScreenId		m_ContextMenuId;
	eDepth				m_depth;

	// local data
	MenuBaseRef			m_MenuCB;
	bool				m_bIsOpen;

	

	PAR_SIMPLE_PARSABLE;

#if __BANK
public:
	static int sm_Count;
	static int sm_Allocs;
#endif
};

// static helper class for common-enough (though not ubiquitous) menu stuff


class CContextMenuHelper
{
public:
	static void BuildPlayerContexts(const rlGamerHandle& gamerHandle, bool bSuppressCrewInvites = false, const rlClanMembershipData* pMembership = NULL);
	static bool HandlePlayerContextOptions(atHashWithStringBank context, const rlGamerHandle& gamerHandle);


	static bool IsLocalPlayer(const rlGamerHandle& gamerHandle);

	static void InitVotes();
	static void PlayerHasJoined(PhysicalPlayerIndex index);
	static void PlayerHasLeft(PhysicalPlayerIndex index);
	static bool SetKickVote(const rlGamerHandle& gamerHandle, bool vote);
	static bool SetKickVote(PhysicalPlayerIndex index, bool vote, bool castNetworkVote = true);
	static bool GetKickVote(const rlGamerHandle& gamerHandle);
	static bool GetKickVote(PhysicalPlayerIndex index) {return sm_kickVotes[index];}
	static bool InKickCooldown();
	static bool InReportCooldown();
	static bool InCommendCooldown();
	static bool CooldownHelper(unsigned tunableNameHash, unsigned disabledNameHash, const int tunableDefault, u32 lastTime);
	static void FlagPlayerContextInTournament(bool bInTournament) { m_bInTournament = bInTournament;}
    static bool IsPlayerContextInTournament() { return m_bInTournament;}

	static bool ReportUGC(const rlGamerHandle& gamerHandle);
	static bool ReportPlayer(const rlGamerHandle& gamerHandle);
	static bool CommendPlayer(const rlGamerHandle& gamerHandle);

	static void ShowCommendScreen();

	static void SetLastReportTimer(u32 uSeconds)		{ m_uLastReport = uSeconds; }

	static void clbReportCommendHelper(u32& lastTimestamp, const StatId cStatIdStrength, const StatId cStatIdPenalty, const StatId cStatIdRestore, const unsigned nTunableNameHash, const unsigned nDisableHash, const int tunableDefault, u32 eventHash);
	static void clbReportForGriefing();
	static void clbReportForOffensiveLang();
	static void clbReportForOffensiveTagPlate();
	static void clbReportForOffensiveUGC();
	static void clbReportForExploit();
	static void clbReportForGameExploit();
	static void clbReportForUGCContent();
	static void clbReportForVCAnnoying();
	static void clbReportForVCHate();
	static void clbReportForTextChatAnnoying();
	static void clbReportForTextChatHate();

	static void clbCommendForFriendly();
	static void clbCommendForHelpful();
	
#if __BANK
	static bool m_bNonStopReports;
#endif //__BANK

private:
	static void CreateWarningMessage( const char* pBody );
	static void ClearWarningScreen();

	static atRangeArray<bool, MAX_NUM_PHYSICAL_PLAYERS> sm_kickVotes;

	static u32 m_uLastReport;
	static u32 m_uLastCommend;
	static u32 m_uLastKick;
	static bool m_bInTournament;

};


#endif // __CONTEXT_MENU_H__

