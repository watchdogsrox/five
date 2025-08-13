//
// filename:	NetworkTransactionTelemetry.h
// description:	
//

#ifndef INC_NETWORK_TRANSACTION_TELEMETRY_H_
#define INC_NETWORK_TRANSACTION_TELEMETRY_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "diag/channel.h"
#include "rline/rlgamerinfo.h"
#include "string/stringhash.h"
// Game headers
#include "NetworkTelemetry.h"
#include "Scene/World/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "script/script.h"
#include "Network/NetworkTypes.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"

#if __NET_SHOP_ACTIVE
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Shop/NetworkShopping.h"
#endif //__NET_SHOP_ACTIVE

class MetricBaseChar
{
public:
	MetricBaseChar()
	{
		m_slot      = StatsInterface::GetCurrentMultiplayerCharaterSlot();
		m_cash      = MoneyInterface::GetVCWalletBalance();
		m_bank      = MoneyInterface::GetVCBankBalance();
		m_evc       = MoneyInterface::GetEVCBalance();
		m_pvc       = MoneyInterface::GetPVCBalance();
		m_pxr       = MoneyInterface::GetPXRValue();
		NET_SHOP_ONLY( m_nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry(); )
	}

	NET_SHOP_ONLY( s64 GetNonceValue() const { return m_nonceSeed; } )

	bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->Begin("char", NULL);
		{
			result = result && rw->WriteInt("slot", m_slot);
			result = result && rw->WriteInt64("cash", m_cash);
			result = result && rw->WriteInt64("bank", m_bank);
			result = result && rw->WriteInt64("evc", m_evc);
			result = result && rw->WriteInt64("pvc", m_pvc);
			result = result && rw->WriteFloat("pxr", m_pxr);

#if __NET_SHOP_ACTIVE
			if (!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
				result = result && rw->WriteInt64("nonce", m_nonceSeed);
#endif //__NET_SHOP_ACTIVE
		}
		result = result && rw->End();
		return result;
	}

private:
	int m_slot;
	s64 m_cash;
	s64 m_bank;
	s64 m_evc;
	s64 m_pvc;
	float m_pxr;
	NET_SHOP_ONLY( s64 m_nonceSeed; )
};

class MetricBaseInfo
{
public:
	MetricBaseInfo ( )
	{
		m_posn        = CGameWorld::FindLocalPlayer() ? CGameWorld::FindLocalPlayerCoors() : Vector3(0.0f, 0.0f, 0.0f);
		m_playtime    = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
		m_vcPosixTime = StatsInterface::GetUInt64Stat(STAT_MPPLY_STORE_PURCHASE_POSIX_TIME);
		m_vccash      = StatsInterface::GetInt64Stat(STAT_MPPLY_STORE_TOTAL_MONEY_BOUGHT);
		m_mid		  = static_cast<s64>(CNetworkTelemetry::GetMatchHistoryId());
	}

	bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->WriteArray("c", &m_posn[0], 3);
		result = result && rw->WriteUns("pt", m_playtime);
		result = result && rw->WriteUns64("vct", m_vcPosixTime);
		result = result && rw->WriteInt64("vcp", m_vccash);
		result = result && rw->WriteInt64("mid", m_mid);
		return result;
	}

private:
	Vector3 m_posn;
	u32 m_playtime;
	u64 m_vcPosixTime;
	s64 m_vccash;
	s64 m_mid;
};

class MetricBasicTransaction : public rlMetric, public MetricBaseChar, public MetricBaseInfo
{
public:
	MetricBasicTransaction ( ) : MetricBaseChar(), MetricBaseInfo() { }

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && this->MetricBaseChar::Write(rw);
		result = result && this->MetricBaseInfo::Write(rw);
		return result;
	}
};

class MetricTransaction : public MetricBasicTransaction
{
public:
	MetricTransaction(s64 amount) : MetricBasicTransaction(), m_amount(amount) { };
	virtual bool Write(RsonWriter* rw) const
	{
		bool result = rw->WriteInt64("amt", m_amount);

		result = this->MetricBasicTransaction::Write(rw);

		const char* pActionName = GetActionName();
		if(pActionName)
		{
			result = result && rw->WriteString("action", GetActionName());
			result = result && rw->WriteString("cat", GetCategoryName());
			result = result && rw->Begin("ctx", NULL);
			result = result && WriteContext(rw);
			result = result && rw->End();
		}

#if __NET_SHOP_ACTIVE
		gnetAssertf(NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() 
			|| m_amount == 0 
			|| GetNonceValue() > 0, "Invalid nonce for '%s'.", GetActionName());
#endif //__NET_SHOP_ACTIVE

		return result;
	}

	virtual const char*   GetActionName() const {return NULL;}
	virtual const char* GetCategoryName() const {return "NONE";}
	virtual bool WriteContext(RsonWriter* ) const {return true;}

private:
	s64 m_amount;
};

#if !__FINAL
class MetricClearBank : public rlMetric, public MetricBaseInfo
{
	RL_DECLARE_METRIC(CLEAR_BANK, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricClearBank() {;}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricBaseInfo::Write(rw);
	}
};
#endif // !__FINAL

// DELETE Character
class MetricDeleteCharater : public rlMetric, public MetricBaseInfo
{
	RL_DECLARE_METRIC(DELETE_CHAR, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricDeleteCharater(const int character) : m_slot(character), m_cash(MoneyInterface::GetVCBankBalance()) {;}
	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("char", NULL);
		{
			result = result && rw->WriteInt("slot", m_slot);
			result = result && rw->WriteInt64("cash", m_cash);
		}
		result = result && rw->End();

		result = result && MetricBaseInfo::Write(rw);

		return result;
	}

private:
	int  m_slot;
	s64  m_cash;
};

// DELETE Character
class MetricManualDeleteCharater : public rlMetric, public MetricBaseInfo
{
	RL_DECLARE_METRIC(MANUAL_DELETE_CHAR, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricManualDeleteCharater(const int character) : m_slot(character), m_cash(MoneyInterface::GetVCBankBalance()) {;}
	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("char", NULL);
		{
			result = result && rw->WriteInt("slot", m_slot);
			result = result && rw->WriteInt64("cash", m_cash);
		}
		result = result && rw->End();

		result = result && MetricBaseInfo::Write(rw);

		return result;
	}

private:
	int  m_slot;
	s64  m_cash;
};
// Clear all of EVC Character.
class MetricClearEVC : public rlMetric, public MetricBaseInfo
{
	RL_DECLARE_METRIC(CLEAR_EVC, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricClearEVC(const s64 amount) : m_amount(amount), m_cash(MoneyInterface::GetVCBankBalance()) {;}
	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->WriteInt64("amount", m_amount);
		result = result && rw->WriteInt64("cash", m_cash);
		result = result && MetricBaseInfo::Write(rw);
		return result;
	}

private:
	s64  m_amount;
	s64  m_cash;
};

// EARN event
class MetricEarn : public MetricTransaction
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarn(s64 amount, bool tobank = false) : MetricTransaction(amount)
		, m_tobank(tobank) 
		, m_starterPack(1)
	{
		if (CLiveManager::GetCommerceMgr().IsEntitledToStarterPack() != cCommerceManager::NONE)
		{
			m_starterPack = 2;
		}
		if (CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack() != cCommerceManager::NONE)
		{
			m_starterPack = 3;
		}
	}

	virtual bool Write(RsonWriter* rw) const 
	{
		return this->MetricTransaction::Write(rw) 
			&& rw->WriteBool("tobank", m_tobank)
			&& rw->WriteInt("sp", m_starterPack)
#if NET_ENABLE_MEMBERSHIP_METRICS
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
			&& rw->WriteInt("m", rlScMembership::GetMembershipTelemetryState(NetworkInterface::GetLocalGamerIndex()))
#else
			&& rw->WriteInt("m", NET_DUMMY_NOT_MEMBER_STATE)
#endif
#endif
			&& rw->WriteInt("g", SocialClubEventMgr::Get().GetGlobalEventHash())
#if NET_ENABLE_MEMBERSHIP_METRICS
			&& rw->WriteInt("e", SocialClubEventMgr::Get().GetMembershipEventHash())
#endif
			;
	}

private:
	bool m_tobank;
	int m_starterPack;
};

// SPEND event
class MetricSpend : public MetricTransaction
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpend(s64 amount, bool fromBank) : MetricTransaction(amount)
		, m_fromBank(fromBank)
		, m_starterPack(1)
		, m_discount(SpendMetricCommon::sm_discount)
	{
		if(CLiveManager::GetCommerceMgr().IsEntitledToStarterPack() != cCommerceManager::NONE)
		{
			m_starterPack = 2;
		}
		if(CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack() != cCommerceManager::NONE)
		{
			m_starterPack = 3;
		}
		SpendMetricCommon::SetDiscount(0);
	}

	virtual bool Write(RsonWriter* rw) const 
	{
		bool result = MetricTransaction::Write(rw);
		result &= rw->WriteBool("frombank", m_fromBank);
		result &= rw->WriteInt("sp", m_starterPack);
		result &= rw->WriteInt("p", SpendMetricCommon::sm_properties);
		result &= rw->WriteInt("p2", SpendMetricCommon::sm_properties2);
		result &= rw->WriteInt("h", SpendMetricCommon::sm_heists);
		if (m_discount != 0)
		{
			result &= rw->WriteInt("d", m_discount);
		}
#if NET_ENABLE_WINDFALL_METRICS
		result &= rw->WriteBool("w", SpendMetricCommon::sm_windfall);
#endif
#if NET_ENABLE_MEMBERSHIP_METRICS
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
		result &= rw->WriteInt("m", rlScMembership::GetMembershipTelemetryState(NetworkInterface::GetLocalGamerIndex()));
#else
		result &= rw->WriteInt("m", NET_DUMMY_NOT_MEMBER_STATE);
#endif
#endif
		result &= rw->WriteInt("g", SocialClubEventMgr::Get().GetGlobalEventHash());
#if NET_ENABLE_MEMBERSHIP_METRICS
		result &= rw->WriteInt("e", SocialClubEventMgr::Get().GetMembershipEventHash());
#endif
		return result;
	}

private:
	bool m_fromBank;
	int m_starterPack;
	int m_discount;
};

// Shared money From Last Job
class MetricGiveJobShared : public MetricBasicTransaction
{
	RL_DECLARE_METRIC(GIVE_JOB, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricGiveJobShared(int amount, const rlGamerHandle& gamerHandle) : MetricBasicTransaction(), m_amount(amount), m_gamerHandle(gamerHandle) {}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricBasicTransaction::Write(rw);

		result = result && rw->WriteInt("amt", m_amount);

		bool gameradded = false;
		if (gnetVerify(m_gamerHandle.IsValid()))
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);

			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				gameradded = true;
				result = result && rw->WriteString("to", gamerHandleStr);
			}
		}

		if (!gameradded)
		{
			result = result && rw->WriteString("to", "UNKNOWN");
		}

#if __NET_SHOP_ACTIVE
		gnetAssertf(NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() 
			|| m_amount == 0 
			|| GetNonceValue() > 0, "Invalid nonce for 'GIVE_JOB'.");
#endif //__NET_SHOP_ACTIVE

		return result;
	}

private:
	int m_amount;
	rlGamerHandle m_gamerHandle;
};

// Purchase VC event
class MetricPurchaseVC : public rlMetric, public MetricBaseInfo, public MetricBaseChar
{
	RL_DECLARE_METRIC(PURCH, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricPurchaseVC(int amount, float usde, const char* packName, bool ingameStorePurchase) 
		: MetricBaseInfo()
		, MetricBaseChar()
		, m_amount(amount)
		, m_usde(usde)
		, m_bInGamePurchase(ingameStorePurchase) 
	{
		if(gnetVerifyf(packName, "No productName"))
		{
			safecpy(m_PackName, packName);
		}
		else
		{
			safecpy(m_PackName, "");
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->WriteInt("amt", m_amount);
		result = result && rw->WriteFloat("usde", m_usde);
		result = result && rw->WriteBool("ingame", m_bInGamePurchase);
		result = result && rw->WriteString("n", m_PackName);
		result = result && this->MetricBaseInfo::Write(rw);
		result = result && this->MetricBaseChar::Write(rw);

#if __NET_SHOP_ACTIVE
		gnetAssertf(NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() 
			|| m_amount == 0 
			|| GetNonceValue() > 0, "Invalid nonce for 'PURCH'.");
#endif //__NET_SHOP_ACTIVE

		return result;
	}

private:
	int m_amount;
	float m_usde;
	bool m_bInGamePurchase;
	char m_PackName[rlCashPackUSDEInfo::MAX_PACK_NAME_CHARS];
};

class MetricIngameStoreCheckoutComplete : public rlMetric, public MetricBaseInfo, public MetricBaseChar
{
	RL_DECLARE_METRIC(CO_COMPL, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricIngameStoreCheckoutComplete(const char* displayString, const char* productId)
		: MetricBaseInfo()
		, MetricBaseChar()
	{
		
		if (gnetVerifyf(displayString, "No display price"))
		{
			safecpy(m_DisplayPrice, displayString);
		}
		else
		{
			safecpy(m_DisplayPrice, "");
		}

		if (gnetVerifyf(productId, "No product Id"))
		{
			safecpy(m_ProductId, productId);
		}
		else
		{
			safecpy(m_ProductId, "");
		}		
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->WriteString("d", m_DisplayPrice);
		result = result && rw->WriteString("n", m_ProductId);
		result = result && this->MetricBaseInfo::Write(rw);
		result = result && this->MetricBaseChar::Write(rw);

		return result;
	}

private:
	static const int MAX_DISPLAY_PRICE_LENGTH = 32;
	static const int MAX_PRODUCT_ID_LENGTH = 64;

	char m_DisplayPrice[MAX_DISPLAY_PRICE_LENGTH];
	char m_ProductId[MAX_PRODUCT_ID_LENGTH];

};

// New Purchase VC event for tracking within the ingame store
class MetricInGamePurchaseVC : public rlMetric, public MetricBaseInfo, public MetricBaseChar
{
    RL_DECLARE_METRIC(PURCH_IG, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricInGamePurchaseVC(int amount, float usde) 
		: MetricBaseInfo()
		, MetricBaseChar()
		, m_amount(amount)
		, m_usde(usde) {}

    virtual bool Write(RsonWriter* rw) const
    {
        bool result = true;
        result = result && rw->WriteInt("amt", m_amount);
        result = result && rw->WriteFloat("usde", m_usde);
        result = result && this->MetricBaseInfo::Write(rw);
		result = result && this->MetricBaseChar::Write(rw);

#if __NET_SHOP_ACTIVE
		gnetAssertf(NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() 
			|| m_amount == 0 
			|| GetNonceValue() > 0, "Invalid nonce for 'PURCH_IG'.");
#endif //__NET_SHOP_ACTIVE

		return result;
    }

private:
    int m_amount;
    float m_usde;
};

// transfer cash from bank account to character event
class MetricAllocateVC : public MetricTransaction
{
	RL_DECLARE_METRIC(ALLOT, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricAllocateVC(int amount) : MetricTransaction((s64)amount) {}
};

// transfer cash from character back to bank account event
class MetricRecoupVC : public MetricTransaction
{
	RL_DECLARE_METRIC(RECOUP, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricRecoupVC(int amount) : MetricTransaction((s64)amount) {}
};


//
// EARN metrics
//
class MetricEarnFromPickup : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromPickup(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "PICKUP";}
	virtual const char* GetCategoryName() const {return "PICKED_UP";}
};

class MetricEarnFromGangAttackPickup : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromGangAttackPickup(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "GAPICKUP";}
	virtual const char* GetCategoryName() const {return "PICKED_UP";}
};

class MetricEarnFromAssassinateTargetKilled : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromAssassinateTargetKilled(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "KILL_TARGET";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnFromArmoredRobbery : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromArmoredRobbery(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "ARM_ROB";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnSpinTheWheelCash : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnSpinTheWheelCash(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "WHEEL_CASH";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnFromCrateDrop : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromCrateDrop(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "CRATEDROP";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnMatchId : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricEarnMatchId(int amount, const char* matchId) : MetricEarn((s64)amount) 
	{
		m_matchId[0] = '\0';
		if (AssertVerify(matchId))
		{
			safecpy(m_matchId, matchId, COUNTOF(m_matchId));
		}
	}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw) 
			&& rw->WriteString("mid", m_matchId);
	}

private:
	char m_matchId[MAX_STRING_LENGTH];
};

class MetricEarnFromBetting : public MetricEarnMatchId
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricEarnFromBetting(int amount, const char* matchId) : MetricEarnMatchId(amount, matchId) {}
	virtual const char*   GetActionName() const {return "BETS";}
	virtual const char* GetCategoryName() const {return "BETTING";}
};


class MetricEarnFromJobBonuses : public MetricEarnMatchId
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromJobBonuses(int amount, const char* matchId, const char* challenge) : MetricEarnMatchId(amount, matchId) 
	{
		m_challenge[0] = '\0';
		if (AssertVerify(challenge))
		{
			safecpy(m_challenge, challenge, COUNTOF(m_challenge));
		}
	}

	virtual const char*   GetActionName() const {return "JOB_BONUS";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarnMatchId::WriteContext(rw) 
			&& rw->WriteString("ch", m_challenge);
	}

private:
	char m_challenge[MAX_STRING_LENGTH];
};

class MetricEarnFromGangOps : public MetricEarnMatchId
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromGangOps(int amount, const char* matchId, int challenge, const char* category) : MetricEarnMatchId(amount, matchId) , m_challenge(challenge)
	{
		m_category[0] = '\0';
		if (AssertVerify(category))
		{
			safecpy(m_category, category, COUNTOF(m_category));
		}
	}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return m_category;}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarnMatchId::WriteContext(rw) 
			&& rw->WriteInt("item", m_challenge);
	}

private:
	int m_challenge;
	char m_category[MAX_STRING_LENGTH];
};

class MetricEarnFromJob : public MetricEarnMatchId
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromJob(int amount, const char* matchId) : MetricEarnMatchId(amount, matchId) {}
	virtual const char*   GetActionName() const {return "JOB";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnFromChallengeWin : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricEarnFromChallengeWin(int amount, const char* playlistId, bool headToHead) : MetricEarn((s64)amount), m_headToHead(headToHead)
	{
		m_playlistId[0] = '\0';
		if (AssertVerify(playlistId))
		{
			safecpy(m_playlistId, playlistId, COUNTOF(m_playlistId));
		}
	}

	virtual const char*   GetActionName() const {return "WIN_CHALLENGE";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteString("mid", m_playlistId) 
			&& rw->WriteBool("h2h", m_headToHead);
	}

private:
	char m_playlistId[MAX_STRING_LENGTH];
	bool m_headToHead;
};

class MetricEarnFromBounty : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromBounty(int amount
						,const rlGamerHandle& hGamerPlaced
						,const rlGamerHandle& hGamerTarget
						,const int flags) 
		: MetricEarn((s64)amount)
		, m_hGamerPlaced(hGamerPlaced)
		, m_hGamerTarget(hGamerTarget)
		, m_flags(flags) {}

	virtual const char*   GetActionName() const {return "BOUNTY";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	//"A" - Gamer that placed the bounty
	//"B" - Target of the bounty
	//"flags" - 1-Killed target
	//          2-Survived bounty (player receives the bounty amount if they survive for the time limit)
	//          3-Refund (the player placing a bounty receives a refund if target leaves)
	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricEarn::WriteContext(rw);

		if (m_hGamerPlaced.IsValid())
		{
			char gamer_A[RL_MAX_GAMER_HANDLE_CHARS];
			m_hGamerPlaced.ToUserId(gamer_A);

			if (gnetVerify(ustrlen(gamer_A)>0))
			{
				result = result && rw->WriteString("A", gamer_A);
			}
			else
			{
				result = result && rw->WriteString("A", "UNKNOWN");
			}
		}
		else
		{
			result = result && rw->WriteString("A", "UNKNOWN");
		}

		if (m_hGamerTarget.IsValid())
		{
			char gamer_B[RL_MAX_GAMER_HANDLE_CHARS];
			m_hGamerTarget.ToUserId(gamer_B);

			if (gnetVerify(ustrlen(gamer_B)>0))
			{
				result = result && rw->WriteString("B", gamer_B);
			}
			else
			{
				result = result && rw->WriteString("B", gamer_B);
			}
		}
		else
		{
			result = result && rw->WriteString("B", "UNKNOWN");
		}

		return result && rw->WriteUns("flags", m_flags);
	}

private:
	rlGamerHandle m_hGamerPlaced;
	rlGamerHandle m_hGamerTarget;
	int m_flags;
};

class MetricEarnFromImportExport : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromImportExport(int amount, int modelHash) : MetricEarn((s64)amount), m_modelHash(modelHash) {}

	virtual const char*   GetActionName() const {return "IMP_EXP";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("vid", (u32)m_modelHash);
	}
private:
	int m_modelHash;
};

class MetricEarnFromHoldups : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromHoldups(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "HOLDUP";}
	virtual const char* GetCategoryName() const {return "JOBS";}
};

class MetricEarnFromProperty : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromProperty(int amount, int propertyType) : MetricEarn((s64)amount), m_propertyType(propertyType) {}

	virtual const char* GetActionName() const {return "PROPERTY";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("id", (u32)m_propertyType);
	}
private:
	int m_propertyType;
};

class MetricEarnInitPlayer : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnInitPlayer(int amount, bool tobank) : MetricEarn((s64)amount, tobank){}
	virtual const char* GetActionName() const { return "PLAYERINIT"; }
	virtual const char* GetCategoryName() const { return "PLAYERINIT"; }
};

#if !__FINAL
class MetricEarnFromDebug : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromDebug(int amount, bool toBank) : MetricEarn((s64)amount, toBank) {}
	virtual const char* GetActionName() const {return "DEBUG";}
};
#endif // __FINAL

class MetricEarnFromAiTargetKill : public MetricEarn
{
public:
	MetricEarnFromAiTargetKill(int amount, int pedhash) : MetricEarn((s64)amount), m_pedhash(pedhash) {}
	virtual const char*   GetActionName() const {return "NPC_KILL";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("pedid", (u32)m_pedhash);
	}
private:
	int m_pedhash;
};


class MetricEarnFromNotBadSport : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromNotBadSport(int amount) : MetricEarn((s64)amount) {}
	virtual const char*   GetActionName() const {return "BADSPORT";}
	virtual const char* GetCategoryName() const {return "GOOD_SPORT";}
};

class MetricEarnFromRockstar : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromRockstar(s64 amount) : MetricEarn(amount, true) {}
	virtual const char*   GetActionName() const {return "ROCKSTAR";}
	virtual const char* GetCategoryName() const {return "ROCKSTAR_AWARD";}
};

class MetricEarnFromAward : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromAward(const int amount, const int award) : MetricEarn((s64)amount), m_award(award) { ; }
	virtual const char* GetActionName() const { return "CASINO"; }
	virtual const char* GetCategoryName() const { return "CASINO_AWARD"; }
	virtual bool WriteContext(RsonWriter* rw) const { return MetricEarn::WriteContext(rw) && rw->WriteInt("id", m_award); }
private:
	int m_award;
};


class MetricEarnFromVehicle : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromVehicle(s64 amount, int modelhash, bool personalvehicle, int oldLevel, int newLevel, bool isOffender1, bool isOffender2, int vehicleType, int oldThresholdLevel, int newThresholdLevel) 
		: MetricEarn(amount)
		, m_modelhash(modelhash)
		, m_personalvehicle(personalvehicle) 
		, m_bendProgress(StatsInterface::GetIntStat(STAT_BEND_PROGRESS_HASH))
		, m_oldLevel(oldLevel)
		, m_newLevel(newLevel)
		, m_isOffender1(isOffender1)
		, m_isOffender2(isOffender2)
		, m_vehicleType(vehicleType)
		, m_oldThresholdLevel(oldThresholdLevel)
		, m_newThresholdLevel(newThresholdLevel)
	{}

	virtual const char*   GetActionName() const {return "VEHICLE";}
	virtual const char* GetCategoryName() const {return "SELLING_VEH";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("id", (u32)m_modelhash)
			&& rw->WriteBool("personal", m_personalvehicle)
			&& rw->WriteInt("hp", m_bendProgress)
			&& rw->WriteInt("o", m_oldLevel)
			&& rw->WriteInt("n", m_newLevel)
			&& rw->WriteBool("o1", m_isOffender1)
			&& rw->WriteBool("o2", m_isOffender2)
			&& rw->WriteInt("t", m_vehicleType)
			&& rw->WriteInt("ot", m_oldThresholdLevel)
			&& rw->WriteInt("nt", m_newThresholdLevel);
	}
private:
	int  m_modelhash;
	bool m_personalvehicle;
	int  m_bendProgress;
	int  m_oldLevel;
	int  m_newLevel;
	bool m_isOffender1;
	bool m_isOffender2;
	int  m_vehicleType;
	int  m_oldThresholdLevel;
	int  m_newThresholdLevel;
};

class MetricEarnFromDailyObjective : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricEarnFromDailyObjective(int amount, const char* description, int objective) 
		: MetricEarn((s64)amount)
		, m_objective(objective) 
	{
		m_description[0] = '\0';
		if(AssertVerify(description))
			safecpy(m_description, description, COUNTOF(m_description));
	}
	virtual const char*   GetActionName() const {return "DAILY_OBJ";}
	virtual const char* GetCategoryName() const {return "DAILY_OBJ";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteString("desc", m_description)
			&& rw->WriteInt("obj", m_objective);
	}
private:
	char m_description[MAX_STRING_LENGTH];
	int  m_objective;
};

class MetricEarnFromAmbientJob : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32, MAX_VALUES=4};

public:
	struct sData
	{
	public:
		sData()
		{
			for (int i = 0; i < MAX_VALUES; i++)
				m_values[i] = 0;
		}

		int m_values[MAX_VALUES];
	};

public:
	MetricEarnFromAmbientJob(int amount, const char* description, sData& data) : MetricEarn((s64)amount)
	{
		m_description[0] = '\0';
		if(AssertVerify(description))
			safecpy(m_description, description, COUNTOF(m_description));

		for (int i=0; i<MAX_VALUES; i++)
			m_values[i] = data.m_values[i];
	}
	virtual const char*   GetActionName() const {return "AMB_JOB";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricEarn::WriteContext(rw); 

		result = result && rw->WriteString("desc", m_description);

		result = result && rw->BeginArray("q", NULL);
		{
			for (int i = 0; i < MAX_VALUES; i++)
			{
				result = result && rw->Begin(NULL,NULL);
				result = result && rw->WriteInt("v", m_values[i]);
				result = result && rw->End();
			}
		}
		result = result && rw->End();

		return result;
	}

private:
	char   m_description[MAX_STRING_LENGTH];
	int    m_values[MAX_VALUES];
};

// REFUND
class MetricEarnRefund : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricEarnRefund(const int amount, const char* type, const char* reason, bool tobank) : MetricEarn((s64)amount, tobank)
	{
		m_type[0] = '\0';
		if(AssertVerify(type)) 
			safecpy(m_type, type, COUNTOF(m_type));

		m_reason[0] = '\0';
		if(AssertVerify(reason)) 
			safecpy(m_reason, reason, COUNTOF(m_reason));
	}

	virtual const char*   GetActionName() const {return "REFUND";}
	virtual const char* GetCategoryName() const {return "CONTACT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricEarn::WriteContext(rw); 
		result = result && rw->WriteString("type", m_type);
		result = result && rw->WriteString("reason", m_reason);
		return result;
	}

private:
	char m_type[MAX_STRING_LENGTH];
	char m_reason[MAX_STRING_LENGTH];
};

class MetricEarnPurchaseClubHouse : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnPurchaseClubHouse(const int amount)
		: MetricEarn((s64)amount)
		, m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
	{
	}

	virtual const char* GetActionName() const {return "SELL_CLUB_HOUSE";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_bossId)
			&& rw->WriteInt("b", m_b)
			&& rw->WriteInt("c", m_c)
			&& rw->WriteInt("d", m_d)
			&& rw->WriteInt("e", m_e)
			&& rw->WriteInt("f", m_f)
			&& rw->WriteInt("g", m_g)
			&& rw->WriteInt("h", m_h)
			&& rw->WriteInt("id", m_id)
			&& rw->WriteInt("j", m_j);
	}

public:
	int m_b; // - mural_type
	int m_c; // - wall_style
	int m_d; // - wall_hanging_style
	int m_e; // - furniture_style
	int m_f; // - emblem
	int m_g; // - gun_locker
	int m_h; // - mod_shop
	int m_id; // - property_id (CTX.id)
	int m_j; // - signage
	u64 m_bossId;
};


class MetricEarnFromBusinessProduct : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromBusinessProduct(const int amount, int businessID, int businessType, int quantity)
		: MetricEarn((s64)amount)
		, m_businessID(businessID) 
		, m_businessType(businessType)
		, m_quantity(quantity)
	{
	}

	virtual const char* GetActionName() const {return "BUSINESS_PRODUCT";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("id", (u32)m_businessID)
			&& rw->WriteUns("type", (u32)m_businessType)
			&& rw->WriteInt("qtt", m_quantity);
	}

public:
	int m_quantity;
	int m_businessID;
	int m_businessType;
};


class MetricEarnFromBountyHunterReward : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromBountyHunterReward(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char* GetActionName() const {return "BOUNTY_HUNTER_REWARD";}
};
class MetricEarnFromBusinessBattle : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromBusinessBattle(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char* GetActionName() const {return "BUSINESS_BATTLE";}
};
class MetricEarnFromBusinessHubSell : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromBusinessHubSell(const int amount, int nightclubID,  int quantitySold) 
		: MetricEarn((s64)amount) 
		, m_nightclubID(nightclubID)
		, m_quantitySold(quantitySold)
	{;}
	virtual const char* GetActionName() const {return "BUSINESS_HUB_SELL";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("id", m_nightclubID)
			&& rw->WriteInt64("qtt", m_quantitySold);
	}
private:
	int m_nightclubID;
	int m_quantitySold;
};
class MetricEarnFromClubManagementParticipation : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromClubManagementParticipation(const int amount, int missionId) : MetricEarn((s64)amount), m_missionId(missionId) {;}
	virtual const char* GetActionName() const {return "CLUB_MANAGEMENT_PARTICIPATION";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("id", m_missionId);
	}

	int m_missionId;
};
class MetricEarnFromFMBBPhonecallMission : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromFMBBPhonecallMission(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char* GetActionName() const {return "FMBB_PHONECALL";}
};
class MetricEarnFromFMBBBossWork : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromFMBBBossWork(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char* GetActionName() const {return "FMBB_BOSS_WORK";}
};
class MetricEarnFromFMBBWageBonus : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromFMBBWageBonus(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char* GetActionName() const {return "FMBB_WAGE_BONUS";}
};

class MetricEarnFromVehicleExport : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromVehicleExport(const int amount, const s64 bossId)
		: MetricEarn((s64)amount)
		, m_bossId(bossId) 
	{
	}

	virtual const char* GetActionName() const {return "VEHICLE_EXPORT";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_bossId);
	}

public:
	s64 m_bossId;
};

class MetricEarnFromSellingBunker: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromSellingBunker(const int amount, const int bunkerHash)
		: MetricEarn((s64)amount)
		, m_bunkerHash(bunkerHash) 
	{
	}

	virtual const char* GetActionName() const {return "SELL_BUNKER";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("id", m_bunkerHash);
	}

public:
	int m_bunkerHash;
};

class MetricEarnFromRdrBonus: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromRdrBonus(const int amount, const int weapon) 
		: MetricEarn((s64)amount) 
		, m_weapon(weapon)
	{;}

	virtual const char* GetActionName() const 
	{
		static const atHashString s_revolverHash = ATSTRINGHASH("WEAPON_DOUBLEACTION", 0x97EA20B8);
		return (m_weapon == (int) s_revolverHash.GetHash()) ? "RDRHEADSHOT_BONUS" : "RDRHATCHET";
	}
	virtual const char* GetCategoryName() const { return "HEADSHOT_BONUS"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("id", m_weapon);
	}

	int m_weapon;
};

class MetricEarnFromWagePayment: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromWagePayment(const int amount, const char* category, int missionId = 0) 
		: MetricEarn((s64)amount) 
		, m_category(category)
		, m_missionId(missionId)
	{;}

	virtual const char* GetActionName() const {return "WAGE";}
	virtual const char* GetCategoryName() const { return m_category; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;
		result = result && MetricEarn::WriteContext(rw);
		if (m_missionId)
		{
			result = result && rw->WriteInt("id", m_missionId);
		}
		return result;
	}

	const char* m_category;
	int m_missionId;
};

class MetricEarnFromSellBase: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromSellBase(const int amount, const int baseHash) 
		: MetricEarn((s64)amount) 
		, m_baseHash(baseHash)
	{;}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return "SELL_BASE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_baseHash);
	}

private:
	int m_baseHash;
};

class MetricEarnFromTargetRefund: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromTargetRefund(const int amount, const int target) 
		: MetricEarn((s64)amount) 
		, m_target(target)
	{;}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return "TARGET_REFUND";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_target);
	}

private:
	int m_target;
};

class MetricEarnFromHeistWages: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromHeistWages(const int amount, const int contentid) 
		: MetricEarn((s64)amount) 
		, m_contentid(contentid)
	{;}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return "WAGES";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_contentid);
	}

private:
	int m_contentid;
};

class MetricEarnFromHeistWagesBonus: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromHeistWagesBonus(const int amount, const int contentid) 
		: MetricEarn((s64)amount) 
		, m_contentid(contentid)
	{;}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return "WAGES_BONUS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_contentid);
	}

private:
	int m_contentid;
};

class MetricEarnFromDarChallenge: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromDarChallenge(const int amount, const int contentid) 
		: MetricEarn((s64)amount) 
		, m_contentid(contentid)
	{;}

	virtual const char*   GetActionName() const {return "HEIST";}
	virtual const char* GetCategoryName() const {return "DAR_CHALLENGE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_contentid);
	}

private:
	int m_contentid;
};

class MetricEarnFromGangOpsJobs: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromGangOpsJobs(const int amount, const char* category, const char* contentid)
		: MetricEarn((s64)amount)
		, m_contentid(contentid)
	{
		m_category[0] = '\0';
		if (AssertVerify(category))
		{
			safecpy(m_category, category, COUNTOF(m_category));
		}
	}

	virtual const char*   GetActionName() const {return "GANGOPS";}
	virtual const char* GetCategoryName() const {return m_category;}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;
		result = result && MetricEarn::WriteContext(rw);
		if (m_contentid)
		{
			result = result && rw->WriteString("id", m_contentid);
		}
		return result;
	}

private:

	enum {MAX_STRING_LENGTH = 32};
	char m_category[MAX_STRING_LENGTH];
	const char* m_contentid;
};

class MetricEarnFromDoomsdayFinaleBonus : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromDoomsdayFinaleBonus(const int amount, const int vehiclemodel)
		: MetricEarn((s64)amount)
		, m_vehiclemodel(vehiclemodel)
	{
		;
	}

	virtual const char*   GetActionName() const { return "HEIST"; }
	virtual const char* GetCategoryName() const { return "FINALE_VEHICLE"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_vehiclemodel);
	}

private:
	int m_vehiclemodel;
};

class MetricEarnNightClub: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnNightClub(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char*   GetActionName() const {return "NIGHTCLUB";}
	virtual const char* GetCategoryName() const {return "INCOME";}
};

class MetricEarnBBEventBonus: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnBBEventBonus(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char*   GetActionName() const {return "BB_EVENT";}
	virtual const char* GetCategoryName() const {return "INCOME";}
};

class MetricEarnNightClubDancing: public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnNightClubDancing(const int amount) : MetricEarn((s64)amount) {;}
	virtual const char*   GetActionName() const {return "NIGHTCLUBDANCING";}
	virtual const char* GetCategoryName() const {return "INCOME";}
};

class MetricEarnTunerAward : public MetricEarnMatchId
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnTunerAward(int amount, const char* matchId, const char* challenge) : MetricEarnMatchId(amount, matchId) 
	{
		m_challenge[0] = '\0';
		if (AssertVerify(challenge))
		{
			safecpy(m_challenge, challenge, COUNTOF(m_challenge));
		}
	}

	virtual const char*   GetActionName() const {return "AWARD";}
	virtual const char* GetCategoryName() const {return "TUNER_ROB";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarnMatchId::WriteContext(rw) 
			&& rw->WriteString("ch", m_challenge);
	}

private:
	char m_challenge[MAX_STRING_LENGTH];
};

//
// SPEND metrics
//
enum ePurchaseType
{
	BUYITEM_PURCHASE_WEAPONS
	,BUYITEM_PURCHASE_WEAPONMODS
	,BUYITEM_PURCHASE_WEAPONAMMO
	,BUYITEM_PURCHASE_ARMOR
	,BUYITEM_PURCHASE_BARBERS
	,BUYITEM_PURCHASE_CLOTHES
	,BUYITEM_PURCHASE_TATTOOS
	,BUYITEM_PURCHASE_VEHICLES
	,BUYITEM_PURCHASE_CARMODS
	,BUYITEM_PURCHASE_CARINSURANCE
	,BUYITEM_PURCHASE_CARDROPOFF
	,BUYITEM_PURCHASE_CARREPAIR
	,BUYITEM_PURCHASE_FOOD
	,BUYITEM_PURCHASE_MASKS
	,BUYITEM_PURCHASE_CARIMPOUND
	,BUYITEM_PURCHASE_CARPAINT
	,BUYITEM_PURCHASE_CARWHEELMODS
	,BUYITEM_PURCHASE_FIREWORKS
	,BUYITEM_PURCHASE_ENDMISSIONCARUP
	,BUYITEM_PURCHASE_CASHEISTMISVEHUP
	,BUYITEM_PURCHASE_MAX
};

class MetricSpendShop : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricSpendShop(int amount, bool fromBank, u32 itemHash, int itemType, int quantity, const char* extratype, int shopNameHash, int extraItemHash, int colorHash) 
		: MetricSpend((s64)amount, fromBank)
		, m_itemHash(itemHash)
		, m_itemType(itemType) 
		, m_quantity(quantity)
		, m_shopNameHash(shopNameHash)
		, m_extraItemHash(extraItemHash)
		, m_colorHash(colorHash)
		, m_extratype(0)
		, m_bendProgress(StatsInterface::GetIntStat(STAT_BEND_PROGRESS_HASH))
	{
		if (extratype)
		{
			m_extratype = atStringHash(extratype);
		}
	}

	virtual const char* GetCategoryName() const 
	{
		switch (m_itemType)
		{
		case BUYITEM_PURCHASE_WEAPONMODS:
		case BUYITEM_PURCHASE_ARMOR:
		case BUYITEM_PURCHASE_WEAPONS:
		case BUYITEM_PURCHASE_WEAPONAMMO:
			return "WEAPON_ARMOR";

		case BUYITEM_PURCHASE_FIREWORKS:
			return "FIREWORKS";

		case BUYITEM_PURCHASE_CARWHEELMODS:
		case BUYITEM_PURCHASE_CARPAINT:
		case BUYITEM_PURCHASE_CARIMPOUND:
		case BUYITEM_PURCHASE_CARINSURANCE:
		case BUYITEM_PURCHASE_CARREPAIR:
		case BUYITEM_PURCHASE_VEHICLES:
		case BUYITEM_PURCHASE_CARDROPOFF:
		case BUYITEM_PURCHASE_CARMODS:
		case BUYITEM_PURCHASE_ENDMISSIONCARUP:
		case BUYITEM_PURCHASE_CASHEISTMISVEHUP:
			return "VEH_MAINTENANCE";

		case BUYITEM_PURCHASE_FOOD:
		case BUYITEM_PURCHASE_BARBERS:
		case BUYITEM_PURCHASE_CLOTHES:
		case BUYITEM_PURCHASE_TATTOOS:
		case BUYITEM_PURCHASE_MASKS:
			return "STYLE_ENT";

		case BUYITEM_PURCHASE_MAX:
			return NULL;
		}

		gnetAssertf(0, "Invalid m_itemType=%d", m_itemType);

		return "NONE";
	}

	virtual const char* GetActionName() const {return "SHOP";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		gnetAssertf(m_itemType>=BUYITEM_PURCHASE_WEAPONS && m_itemType<BUYITEM_PURCHASE_MAX, "%s: NETWORK_BUY_ITEM - invalid itemType < %d >, see PURCHASE_TYPE.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), m_itemType);

		bool result = MetricSpend::WriteContext(rw);

		result = result && rw->WriteUns("type", m_itemType);

		result = result && rw->WriteUns("item", m_itemHash);

		result = result && rw->WriteInt("q", m_quantity);

		result = result && rw->WriteUns("itemt", m_extratype);

		result = result && rw->WriteUns("sn", (u32)m_shopNameHash);

		result = result && rw->WriteUns("extraid", (u32)m_extraItemHash);

		result = result && rw->WriteUns("color", (u32)m_colorHash);

		result = result && rw->WriteInt("hp", m_bendProgress);

		int rebate = 0;
		const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("REBATE_SEASON", 0xc9f6e67b), rebate);
		if (tuneExists)
		{
			result = result && rw->WriteInt("rs", rebate);
		}

		return result;
	}
private:
	u32  m_itemHash;
	int  m_itemType;
	int  m_quantity;
	u32  m_extratype;
	int  m_shopNameHash;
	int  m_extraItemHash;
	int  m_colorHash;
	int  m_bendProgress;
};

class MetricSpendContactService : public MetricSpend
{

public:
	MetricSpendContactService(int amount, bool fromBank, int npcProvider) : MetricSpend((s64)amount, fromBank), m_npcProvider(npcProvider) {}
	virtual const char* GetCategoryName() const { return "CONTACT_SERVICE"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt("id", m_npcProvider);
	}

	int m_npcProvider;
};

class MetricSpendTaxis : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendTaxis(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "TAXI";}
};

class MetricSpendRehireDJ : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendRehireDJ(int amount, bool fromBank, int dj) : MetricSpend((s64)amount, fromBank), m_dj(dj) {}
	virtual const char*   GetActionName() const {return "REHIRE_DJ";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("id", m_dj);
	}

private:
	int  m_dj;
};

class MetricSpendEmployeeWage : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendEmployeeWage(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "WAGE";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}
};

class MetricSpendUtilityBills : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendUtilityBills(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "UTILITY_BILL";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}
};

class MetricSpendMatchId : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricSpendMatchId(int amount, bool fromBank, const char* matchId) : MetricSpend((s64)amount, fromBank) 
	{
		m_matchId[0] = '\0';
		if (AssertVerify(matchId))
		{
			safecpy(m_matchId, matchId, COUNTOF(m_matchId));
		}
	}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteString("mid", m_matchId);
	}

private:
	char m_matchId[MAX_STRING_LENGTH];
};
class MetricSpendMatchEntryFee : public MetricSpendMatchId
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendMatchEntryFee(int amount, bool fromBank, const char* matchId) : MetricSpendMatchId(amount, fromBank, matchId) {}
	virtual const char*   GetActionName() const {return "MATCH_FEE";}
	virtual const char* GetCategoryName() const {return "JOB_ACTIVITY";}
};
class MetricSpendBetting : public MetricSpendMatchId
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendBetting(int amount, bool fromBank, const char* matchId, u32 bettype) 
		: MetricSpendMatchId(amount, fromBank, matchId) 
		, m_bettype(bettype)
	{}

	virtual const char*   GetActionName() const {return "MATCH_BET";}
	virtual const char* GetCategoryName() const {return "BETTING";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("type", m_bettype);
	}

private:
	u32 m_bettype;
};

class MetricSpendWager : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendWager(int amount) : MetricSpend(amount, false) {}

	virtual const char*   GetActionName() const {return "WAGER";}
	virtual const char* GetCategoryName() const {return "WAGER";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpendInStripClub : public MetricSpend
{
	//Differentiates between the following types of expenditures: 
	//  Lap dance, Watching the pole and Drinking at the bar.
	enum {LAP_DANCE,WATCHING_THE_POLE,DRINKING_AT_THE_BAR};

	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendInStripClub(int amount, bool fromBank, int expenditureType) : MetricSpend((s64)amount, fromBank), m_expenditureType(expenditureType) 
	{}

	virtual const char*   GetActionName() const {return "STRIP";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		gnetAssert(m_expenditureType>=LAP_DANCE && m_expenditureType<=DRINKING_AT_THE_BAR);

		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("type", m_expenditureType);
	}

private:
	int m_expenditureType;
};

class MetricSpendInJukeBox : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendInJukeBox(int amount, bool fromBank, int playlist) : MetricSpend((s64)amount, fromBank), m_playlist(playlist) 
	{}

	virtual const char*   GetActionName() const {return "JUKEBOX";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("type", m_playlist);
	}

private:
	int m_playlist;
};

class MetricSpendHealthCare : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendHealthCare(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "HEALTH";}
	virtual const char* GetCategoryName() const {return "HEALTHCARE";}
};

class MetricSpendPlayerHealthCare : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendPlayerHealthCare(int amount, bool fromBank, const rlGamerHandle& gamerHandle) 
		: MetricSpend((s64)amount, fromBank)
		, m_gamerHandle(gamerHandle) 
	{
	}

	virtual const char*   GetActionName() const {return "PLAYER_HEALTH";}
	virtual const char* GetCategoryName() const {return "HEALTHCARE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);

		if (gnetVerify(m_gamerHandle.IsValid()))
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);

			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				result = rw->WriteString("to", gamerHandleStr);
			}
		}

		if (!result)
		{
			result = rw->WriteString("to", "UNKNOWN");
		}

		return result;
	}

private:
	rlGamerHandle m_gamerHandle;
};

class MetricSpendAirStrike : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendAirStrike(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "AIR_STRIKE";}
};

class MetricSpendHeliStrike : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendHeliStrike(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "HELI_STRIKE";}
};

class MetricSpendGangBackup : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGangBackup(int amount, bool fromBank, int gangType, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider), m_gangType(gangType) {}

	virtual const char*   GetActionName() const {return "GANG";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpendContactService::WriteContext(rw)
			&& rw->WriteUns("gid", (u32)m_gangType);
	}
private:
	int m_gangType;
};

class MetricSpendOnAmmoDrop : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendOnAmmoDrop(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "AMMO_DROP";}
};

class MetricSpentBuyBounty : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBuyBounty(int amount, bool fromBank, const rlGamerHandle& gamerHandle, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider), m_gamerHandle(gamerHandle) {}

	virtual const char*   GetActionName() const {return "BOUNTY";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpendContactService::WriteContext(rw);

		if (gnetVerify(m_gamerHandle.IsValid()))
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);

			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				result = rw->WriteString("to", gamerHandleStr);
			}
		}

		if (!result)
		{
			result = rw->WriteString("to", "UNKNOWN");
		}

		return result;
	}
private:
	rlGamerHandle m_gamerHandle;
};

class MetricSpentBuyProperty : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBuyProperty(int amount, bool fromBank, int propertyType) : MetricSpend((s64)amount, fromBank), m_propertyType(propertyType) {}

	virtual const char*   GetActionName() const {return "PROPERTY";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		int rebate = 0;
		const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("REBATE_SEASON", 0xc9f6e67b), rebate);
		if (tuneExists)
		{
			return MetricSpend::WriteContext(rw) 
				&& rw->WriteUns("id", (u32)m_propertyType)
				&& rw->WriteInt("rs", rebate);
		}

		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("id", (u32)m_propertyType);
	}
private:
	int m_propertyType;
};

class MetricSpentBuySmokes : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBuySmokes(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "CIGS";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}
};


class MetricSpentHeliPickup : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHeliPickup(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "HELI_PICKUP";}
};

class MetricSpentBoatPickup : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBoatPickup(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "BOAT_PICKUP";}
};

class MetricSpentBullShark : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBullShark(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "BULLSHARK";}
};

#if !__FINAL
class MetricSpentFromDebug : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentFromDebug(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char* GetActionName() const {return "DEBUG";}
};
#endif // __FINAL

class MetricSpentFromCashDrop : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentFromCashDrop(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "CASH_DROP";}
	virtual const char* GetCategoryName() const {return "DROPPED_STOLEN";}
};

class MetricSpentHireMugger : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHireMugger(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "HIRE_MUGGER";}
};

class MetricSpentRobbedByMugger : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentRobbedByMugger(int amount, bool fromBank, int npcProvider) : MetricSpend((s64)amount, fromBank), m_npcProvider(npcProvider) {}
	virtual const char*   GetActionName() const {return "MUGGED";}
	virtual const char* GetCategoryName() const {return "DROPPED_STOLEN";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt("id", m_npcProvider);
	}

	int m_npcProvider;
};

class MetricSpentHireMercenary : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHireMercenary(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "MERCENARY";}
};

class MetricSpentWantedLevel : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentWantedLevel(int amount, bool fromBank, const rlGamerHandle& gamerHandle, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider), m_gamerHandle(gamerHandle) {}

	virtual const char*   GetActionName() const {return "WANTED_LEVEL";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);

		if (gnetVerify(m_gamerHandle.IsValid()))
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);

			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				result = rw->WriteString("to", gamerHandleStr);
			}
		}

		if (!result)
		{
			result = rw->WriteString("to", "UNKNOWN");
		}

		return result;
	}

private:
	rlGamerHandle m_gamerHandle;
};

class MetricSpentOffRadar : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOffRadar(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "OFF_RADAR";}
};

class MetricSpentRevealPlayersInRadar : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentRevealPlayersInRadar(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "REVEAL_PLAYERS";}
};

class MetricSpentCarWash : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentCarWash(int amount, bool fromBank, u32 id, u32 location) : MetricSpend((s64)amount, fromBank), m_id(id), m_location(location) {}
	virtual const char*   GetActionName() const {return "CAR_WASH";}
	virtual const char* GetCategoryName() const {return "VEH_MAINTENANCE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);
		result = rw->WriteUns("id", m_id);
		result = rw->WriteUns("loc", m_location);
		return result;
	}

private:
	u32 m_id;
	u32 m_location;
};

class MetricSpentCinema : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentCinema(int amount, bool fromBank, u32 location) : MetricSpend((s64)amount, fromBank), m_location(location) {}
	virtual const char*   GetActionName() const {return "CINEMA";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("loc", m_location);
	}

private:
	u32 m_location;
};

class MetricSpentTelescope : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentTelescope(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "TELESCOPE";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}
};

class MetricSpentOnHoldUps : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnHoldUps(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "FAILED_HOLDUP";}
	virtual const char* GetCategoryName() const {return "DROPPED_STOLEN";}
};

class MetricSpentOnPassiveMode : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnPassiveMode(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "PASSIVE_MODE";}
};

class MetricSpentOnBankInterest : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnBankInterest(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "BANK_INTEREST";}
};

class MetricSpentOnProstitute : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnProstitute(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "PROSTITUTES";}
	virtual const char* GetCategoryName() const {return "STYLE_ENT";}
};

class MetricSpentOnArrestBail : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnArrestBail(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "ARREST_BAIL";}
	virtual const char* GetCategoryName() const {return "HEALTHCARE";}
};

class MetricSpentOnVehicleInsurancePremium : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnVehicleInsurancePremium(int amount, bool fromBank, int vehicle, const rlGamerHandle& gamerHandle) : MetricSpend((s64)amount, fromBank), m_vehicle(vehicle), m_gamerHandle(gamerHandle) {}
	virtual const char*   GetActionName() const {return "VEHICLE_INSURANCE";}
	virtual const char* GetCategoryName() const {return "VEH_MAINTENANCE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("item", m_vehicle);

		if (m_gamerHandle.IsValid())
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);
			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				result = result && rw->WriteString("gamer", gamerHandleStr);
			}
		}

		return result;
	}

private:
	int m_vehicle;
	rlGamerHandle m_gamerHandle;
};

class MetricSpentOnCall : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnCall(int amount, bool fromBank, const rlGamerHandle& gamerHandle) : MetricSpend((s64)amount, fromBank), m_gamerHandle(gamerHandle) {}
	virtual const char*   GetActionName() const {return "PHONE_CALL";}
	virtual const char* GetCategoryName() const {return "VEH_MAINTENANCE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);

		if (gnetVerify(m_gamerHandle.IsValid()))
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);
			if (gnetVerify(ustrlen(gamerHandleStr)>0))
			{
				result = result && rw->WriteString("gamer", gamerHandleStr);
			}
		}

		return result;
	}

private:
	rlGamerHandle m_gamerHandle;
};

class MetricSpentOnBounty : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnBounty(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "DM_BOUNTY";}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentOnRockstar : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnRockstar(s64 amount, bool fromBank) : MetricSpend(amount, fromBank) {}
	virtual const char*   GetActionName() const {return "ROCKSTAR";}
	virtual const char* GetCategoryName() const {return "ROCKSTAR_AWARDROCKSTAR_AWARD";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentOnNoCop : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnNoCop(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "NOCOP";}
};

class MetricSpentOnJobRequest : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnJobRequest(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "JOBREQUEST";}
};

class MetricSpentOnBendRequest : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnBendRequest(int amount, bool fromBank, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider) {}
	virtual const char*   GetActionName() const {return "BENDREQUEST";}
};

class MetricSpendCargoSourcing : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendCargoSourcing(int amount, bool fromBank, int cost, int warehouseID, int warehouseSlot)
	: MetricSpend(amount, fromBank)
	, m_Cost(cost)
	, m_WarehouseID(warehouseID)
	, m_WarehouseSlot(warehouseSlot)
	{ }

	virtual const char* GetActionName() const { return "CARGO_SOURCING"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt("b", m_Cost)
			&& rw->WriteInt("c", m_WarehouseID)
			&& rw->WriteInt("d", m_WarehouseSlot);
	}

private:
	int m_Cost;
	int m_WarehouseID;
	int m_WarehouseSlot;
};

class MetricSpendOnLotteryTicket : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendOnLotteryTicket(int amount, bool fromBank, int numberOfTickets) : MetricSpend((s64)amount, fromBank), m_numberOfTickets(numberOfTickets) 
	{}

	virtual const char*   GetActionName() const {return "LOTTERY";}
	virtual const char* GetCategoryName() const {return "LOTTERY";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("num", m_numberOfTickets);
	}

private:
	int m_numberOfTickets;
};

class MetricSpendNightclubEntryFee : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendNightclubEntryFee(int amount, bool fromBank, const rlGamerHandle& gamerHandle, int entryType) 
		: MetricSpend((s64)amount, fromBank)
		, m_gamerHandle(gamerHandle)
		, m_entryType(entryType)
	{}

	virtual const char*   GetActionName() const {return "NIGHTCLUB_ENTRY";}
	virtual const char* GetCategoryName() const {return "NIGHTCLUB_ENTRY";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);

		if ( gnetVerify(m_gamerHandle.IsValid()) )
		{
			char gamerHandleStr[RL_MAX_GAMER_HANDLE_CHARS];
			m_gamerHandle.ToUserId(gamerHandleStr);

			if ( gnetVerify(ustrlen(gamerHandleStr)>0) )
			{
				result = result && rw->WriteString("gamer", gamerHandleStr);
			}
		}

		result = result && rw->WriteInt("item", m_entryType);

		return result;
	}
	
private:
	rlGamerHandle m_gamerHandle;
	int m_entryType;
};

class MetricSpendNightclubBarDrink : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricSpendNightclubBarDrink(int amount, bool fromBank, int drinkid) 
		: MetricSpend((s64)amount, fromBank)
		, m_drinkid(drinkid)
	{;}

	virtual const char*   GetActionName() const {return "NIGHTCLUB_BARDRINK";}
	virtual const char* GetCategoryName() const {return "NIGHTCLUB_BARDRINK";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("id", m_drinkid);
	}

private:
	int m_drinkid;
};
class MetricSpendBountyHunterMission : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricSpendBountyHunterMission(int amount, bool fromBank) 
		: MetricSpend((s64)amount, fromBank) 
	{;}

	virtual const char*   GetActionName() const {return "BOUNTY_HUNTER";}
	virtual const char* GetCategoryName() const {return "BOUNTY_HUNTER";}
};

class MetricSpentOnFairgroundRide : public MetricSpendContactService
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnFairgroundRide(int amount, bool fromBank, int rideid, int npcProvider) : MetricSpendContactService((s64)amount, fromBank, npcProvider), m_rideid(rideid) {}
	virtual const char*   GetActionName() const {return "FAIRGROUNDRIDE";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpendContactService::WriteContext(rw)
			&& rw->WriteInt("rideid", m_rideid);
	}

private:
	int m_rideid;
};

class MetricSpendJobSkip : public MetricSpendMatchId
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendJobSkip(int amount, bool fromBank, const char* matchId) : MetricSpendMatchId(amount, fromBank, matchId) {}
	virtual const char*   GetActionName() const {return "JOB_SKIP";}
	virtual const char* GetCategoryName() const {return "JOB_ACTIVITY";}
};

// DEDUCT
class MetricSpendDeduct : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	enum {MAX_STRING_LENGTH = 32};

public:
	MetricSpendDeduct(const int amount, const char* type, const char* reason, bool tobank) : MetricSpend((s64)amount, tobank)
	{
		m_type[0] = '\0';
		if(AssertVerify(type)) 
			safecpy(m_type, type, COUNTOF(m_type));

		m_reason[0] = '\0';
		if(AssertVerify(reason)) 
			safecpy(m_reason, reason, COUNTOF(m_reason));
	}

	virtual const char* GetActionName() const {return "DEDUCT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteString("type", m_type) 
			&& rw->WriteString("reason", m_reason);
	}

private:
	char m_type[MAX_STRING_LENGTH];
	char m_reason[MAX_STRING_LENGTH];
};

class MetricSpentOnMoveYacht : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnMoveYacht(const int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) 	{	}

	virtual const char* GetActionName() const {return "MOVE_YACHT";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentOnMoveSubmarine : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOnMoveSubmarine(const int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {	}

	virtual const char* GetActionName() const { return "MOVE_SUBMARINE"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentRenameOrganization : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentRenameOrganization(u64 bossId, const int amount, bool fromBank) : MetricSpend((s64)amount, fromBank), m_bossId(bossId) 	{	}

	virtual const char* GetActionName() const {return "RENAME_ORG";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
				&& rw->WriteInt64("uuid", m_bossId);
	}

private:
	u64 m_bossId;
};

// BOSS_GOON
class MetricSpendBecomeBoss : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendBecomeBoss(const int amount, bool tobank, u64 uuidValue) : MetricSpend((s64)amount, tobank), m_uuidValue(uuidValue) {;}

	virtual const char* GetActionName() const {return "BECOME_BOSS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
				&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};


// EARN GOON
class MetricEarnGoon : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnGoon(const int amount, u64 uuidValue) : MetricEarn((s64)amount, false), m_uuidValue(uuidValue)	{}

	virtual const char*   GetActionName() const {return "GOON";}
	virtual const char* GetCategoryName() const {return "BOSS_GOON";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};

// EARN BOSS
class MetricEarnBoss : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnBoss(const int amount, u64 uuidValue) : MetricEarn((s64)amount, false), m_uuidValue(uuidValue)	{}

	virtual const char*   GetActionName() const {return "BOSS";}
	virtual const char* GetCategoryName() const {return "BOSS_GOON";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};


// EARN AGENCY
class MetricEarnAgency : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnAgency(const int amount, u64 uuidValue) : MetricEarn((s64)amount, false), m_uuidValue(uuidValue)	{}

	virtual const char*   GetActionName() const {return "AGENCY";}
	virtual const char* GetCategoryName() const {return "BOSS_GOON";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};

class MetricEarnWarehouse : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnWarehouse(const int amount, int nameHash) : MetricEarn((s64)amount, false), m_nameHash(nameHash)	{}

	virtual const char*   GetActionName() const {return "WAREHOUSE";}
	virtual const char* GetCategoryName() const {return "PROPERTY_UTIL";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_nameHash);
	}

private:
	int m_nameHash;
};

class MetricEarnContraband : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnContraband(const int amount, int quantity) : MetricEarn((s64)amount, false), m_quantity(quantity)	{}

	virtual const char*   GetActionName() const {return "CONTRABAND";}
	virtual const char* GetCategoryName() const {return "CONTRABAND";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("qtt", m_quantity);
	}

private:
	int m_quantity;
};

class MetricEarnDestroyContraband : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnDestroyContraband(const int amount) : MetricEarn((s64)amount, false)	{}
	virtual const char*   GetActionName() const {return "DESTROY_CONTRABAND";}
	virtual const char* GetCategoryName() const {return "DESTROY_CONTRABAND";}
};

class MetricEarnSmugglerWork : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnSmugglerWork(const int amount, const char* category, int quantity = 0, int type = 0) 
		: MetricEarn((s64)amount, false)
		, m_category(category)
		, m_quantity(quantity)
		, m_type(type)
	{}
	virtual const char*   GetActionName() const {return "SMUGGLER";}
	virtual const char* GetCategoryName() const {return m_category;}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("qt", m_quantity)
			&& rw->WriteInt("id", m_type);
	}

	const char* m_category;
	int m_quantity;
	int m_type;
};

class MetricEarnTradeHangar : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnTradeHangar(const int amount, const char* category, int hash) 
		: MetricEarn((s64)amount, false)
		, m_category(category)
		, m_hash(hash)
	{}
	virtual const char*   GetActionName() const {return "SELL_HANGAR";}
	virtual const char* GetCategoryName() const {return m_category;}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt("id", m_hash);
	}

	const char* m_category;
	int m_hash;
};



// SPEND GOON
class MetricSpendGoon : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGoon(int amount, u64 uuidValue) : MetricSpend((s64)amount, false), m_uuidValue(uuidValue)	{}

	virtual const char*   GetActionName() const {return "GOON";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};


// BOSS_GOON
class MetricSpendBoss : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendBoss(const int amount, bool tobank, u64 uuidValue) : MetricSpend((s64)amount, tobank), m_uuidValue(uuidValue) {;}

	virtual const char* GetActionName() const {return "BOSS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};

class MetricSpentBuyContrabandMission : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBuyContrabandMission(int amount, bool fromBank, int warehouseID, int missionID) 
		: MetricSpend((s64)amount, fromBank)
		, m_warehouseID(warehouseID) 
		, m_missionID(missionID) 
	{}

	virtual const char*   GetActionName() const {return "CONTRABAND_MISSION";}
	virtual const char* GetCategoryName() const {return "CONTRABAND";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("id", (u32)m_warehouseID)
			&& rw->WriteUns("mid", (u32)m_missionID);	}
private:
	int m_warehouseID;
	int m_missionID;
};

class MetricSpentPayBussinessSupplies : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentPayBussinessSupplies(int amount, int businessID, int businessType, int numSegments) 
		: MetricSpend((s64)amount, false)
		, m_businessID(businessID) 
		, m_businessType(businessType)
		, m_numSegments(numSegments)
	{}

	virtual const char*   GetActionName() const {return "BUSINESS_SUPPLIES";}
	virtual const char* GetCategoryName() const {return "BUSINESS";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("item", (u32)m_businessID)
			&& rw->WriteUns("type", (u32)m_businessType)
			&& rw->WriteUns("q", (u32)m_numSegments);
	}
private:
	int m_businessID;
	int m_businessType;
	int m_numSegments;
};


class MetricSpentVehicleExportMods : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentVehicleExportMods(int amount, bool fromBank, s64 bossID, int buyerID, int vehicle1, int vehicle2, int vehicle3, int vehicle4) 
		: MetricSpend((s64)amount, fromBank)
		, m_bossID(bossID)
		, m_buyerID(buyerID)
		, m_vehicle1(vehicle1)
		, m_vehicle2(vehicle2)
		, m_vehicle3(vehicle3)
		, m_vehicle4(vehicle4)
	{

	}

	virtual const char*   GetActionName() const {return "EXPORT_MODS";}
	virtual const char* GetCategoryName() const {return "EXPORTMODS";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt64("uuid", m_bossID)
			&& rw->WriteInt("bid", m_buyerID)
			&& rw->WriteInt("v1", m_vehicle1)
			&& rw->WriteInt("v2", m_vehicle2)
			&& rw->WriteInt("v3", m_vehicle3)
			&& rw->WriteInt("v4", m_vehicle4);
	}

private:
	s64 m_bossID;
	int m_buyerID;
	int m_vehicle1;
	int m_vehicle2;
	int m_vehicle3;
	int m_vehicle4;
};

// Exec Telem

class MetricSpentVipUtilityCharges : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentVipUtilityCharges(const int amount, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
    {	
    }

    virtual const char* GetActionName() const {return "UTILITY_CHARGES";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw);
    }
};

class MetricSpentPaServiceHeli : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaServiceHeli(const int amount, bool anotherMember, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_anotherMember(anotherMember)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_SERV_HELI";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteBool("a", m_anotherMember)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	bool m_anotherMember;
    u64 m_bossId;					//Unique BossID
};

class MetricSpentPaServiceVehicle : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaServiceVehicle(const int amount, u32 vehicleHash, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_vehicleHash(vehicleHash)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_SERV_VEH";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_vehicleHash)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	u32 m_vehicleHash;
    u64 m_bossId;					//Unique BossID
};

class MetricSpentPaServiceSnack : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaServiceSnack(const int amount, u32 hash, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_hash(hash)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_SERV_SNACK";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_hash)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	u32 m_hash;
    u64 m_bossId;					//Unique BossID
};

class MetricSpentPaServiceDancer : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaServiceDancer(const int amount, u32 value, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_value(value)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_SERV_DANCER";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_value)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	u32 m_value;
    u64 m_bossId;					//Unique BossID
};

class MetricSpentPaServiceImpound : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaServiceImpound(const int amount, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_SERV_IMPOUND";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	u64 m_bossId;					//Unique BossID
};

class MetricSpentPaHeliPickup : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentPaHeliPickup(const int amount, u32 hash, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_hash(hash)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {	
    }

    virtual const char* GetActionName() const {return "PA_HELI_PICKUP";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_hash)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	u32 m_hash;
    u64 m_bossId;					//Unique BossID
};

class MetricSpent_OfficeAndWarehousePurchAndUpdate : public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

    MetricSpent_OfficeAndWarehousePurchAndUpdate(const char* action, const char* category, u32 item, const int amount, bool fromBank)
        : MetricSpend((s64)amount, fromBank)
        , m_item(item)
        , m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
    {
        safecpy(m_action, action);
        safecpy(m_category, category);
    }

    virtual const char* GetActionName() const {return m_action;}
    virtual const char* GetCategoryName() const {return m_category;}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_item)
            && rw->WriteInt64("uuid", m_bossId);
    }

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
    char m_action[CATEGORY_AND_ACTION_LENGTH];
    char m_category[CATEGORY_AND_ACTION_LENGTH];
    u64 m_bossId;
};


class MetricSpent_OfficeGarage : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_OfficeGarage(const char* action, const char* category, u32 item, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_item(item)
		, m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}

	virtual const char* GetActionName() const {return m_action;}
	virtual const char* GetCategoryName() const {return m_category;}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_item)
			&& rw->WriteInt64("uuid", m_bossId);
	}

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
	u64 m_bossId;
};

class MetricSpentMcAbility: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentMcAbility(int amount, bool fromBank, int ability, int role) : MetricSpend((s64)amount, fromBank), m_ability(ability), m_role(role) {}
	virtual const char*   GetActionName() const {return "MC_ABILITIES";}
	virtual const char* GetCategoryName() const {return "MC_ABILITIES";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);
		result = rw->WriteUns("item", m_ability);
		result = rw->WriteUns("type", m_role);
		return result;
	}

private:
	int m_ability;
	int m_role;
};

class MetricSpentImportExportRepair : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentImportExportRepair(int amount, bool fromBank) 
		: MetricSpend((s64)amount, fromBank) 
	{;}

	virtual const char*   GetActionName() const {return "IMPEXP_REPAIR";}
	virtual const char* GetCategoryName() const {return "IMPEXP_REPAIR";}
};

class MetricSpent_SpentPurchaseBusinessProperty : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpent_SpentPurchaseBusinessProperty(const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
	{
	}

	virtual const char* GetCategoryName() const {return "LOCATION";}
	virtual const char* GetActionName() const {return "BUSINESS_PURCH";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_bossId)
			&& rw->WriteUns("item", m_businessId)
			&& rw->WriteUns("type", m_businessType)
			&& rw->WriteUns("extraid", m_businessUpgradeType);
	}

public:
	int m_businessId; // - m_businessID
	int m_businessType; // - m_businessType
	int m_businessUpgradeType; // - m_businessUpgradeType
	u64 m_bossId;
};

class MetricSpent_SpentUpgradeBusinessProperty : public MetricSpent_SpentPurchaseBusinessProperty
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_SpentUpgradeBusinessProperty(const int amount, bool fromBank)
		: MetricSpent_SpentPurchaseBusinessProperty((s64)amount, fromBank)
	{
	}

	virtual const char* GetCategoryName() const {return "UPGRADE";}
	virtual const char* GetActionName() const {return "BUSINESS_MOD";}
	virtual bool WriteContext(RsonWriter* rw) const { return MetricSpent_SpentPurchaseBusinessProperty::WriteContext(rw); }
};
class MetricSpent_SpentTradeBusinessProperty : public MetricSpent_SpentPurchaseBusinessProperty
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_SpentTradeBusinessProperty(const int amount, bool fromBank)
		: MetricSpent_SpentPurchaseBusinessProperty((s64)amount, fromBank)
	{
	}

	virtual const char* GetCategoryName() const {return "LOCATION";}
	virtual const char* GetActionName() const {return "BUSINESS_TRADE";}
	virtual bool WriteContext(RsonWriter* rw) const { return MetricSpent_SpentPurchaseBusinessProperty::WriteContext(rw); }
};

class MetricSpentOrderWarehouseVehicle: public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentOrderWarehouseVehicle(const int amount, u32 hash, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_hash(hash)
    {	
    }

    virtual const char* GetActionName() const {return "WAREHOUSE_VEH";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_hash);
    }

private:
	u32 m_hash;
};

class MetricSpentOrderBodyguardVehicle: public MetricSpend
{
    RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
    MetricSpentOrderBodyguardVehicle(const int amount, u32 hash, bool fromBank) 
        : MetricSpend((s64)amount, fromBank)
        , m_hash(hash)
    {	
    }

    virtual const char* GetActionName() const {return "BODYGUARD_VEH";}
    virtual bool WriteContext(RsonWriter* rw) const
    {
        return MetricSpend::WriteContext(rw)
            && rw->WriteUns("item", m_hash);
    }

private:
    u32 m_hash;
};

class MetricSpent_HangarPurchAndUpdate : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_HangarPurchAndUpdate(const char* action, const char* category, u32 item, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_item(item)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}

	virtual const char* GetActionName() const {return m_action;}
	virtual const char* GetCategoryName() const {return m_category;}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_item);
	}

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
};

class MetricSpentHeistCannon: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHeistCannon(int amount, bool fromBank, const int shoottype) 
		: MetricSpend((s64)amount, fromBank)
		, m_shoottype(shoottype)
	{;}

	virtual const char*   GetActionName() const {return "HEIST_CANNON";}
	virtual const char* GetCategoryName() const {return "HEIST_CANNON";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("type", m_shoottype);
	}

private:
	int m_shoottype;
};

class MetricSpentHeistMission: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHeistMission(int mission, int amount, bool fromBank) : MetricSpend((s64)amount, fromBank), m_mission(mission) {;}
	virtual const char*   GetActionName() const {return "HEIST_MISSION";}
	virtual const char* GetCategoryName() const {return "SKIP_PREP_MISSION";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("type", m_mission);
	}

	int m_mission;
};

class MetricSpentCasinoHeistSkipMission : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentCasinoHeistSkipMission(int mission, int amount, bool fromBank) : MetricSpend((s64)amount, fromBank), m_mission(mission) { ; }
	virtual const char*   GetActionName() const { return "CASINO_HEIST_MISSION"; }
	virtual const char* GetCategoryName() const { return "SKIP_MISSION"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("type", m_mission);
	}

	int m_mission;
};

class MetricSpentArcadeGame : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentArcadeGame(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) { ; }
	virtual const char*   GetActionName() const { return "ARCADE_GAME"; }
	virtual const char* GetCategoryName() const { return "GAME"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentOrbitalCannon: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentOrbitalCannon(int amount, bool fromBank, int option) 
		: MetricSpend((s64)amount, fromBank)
		, m_option(option)
	{;}

	virtual const char*   GetActionName() const {return "HEIST_SECURITY";}
	virtual const char* GetCategoryName() const {return "STRIKE_TEAM";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("type", m_option);
	}

private:
	int m_option;
};

class MetricSpendGangOpsStartStrand: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGangOpsStartStrand(int strand, int amount, bool fromBank) : MetricSpend((s64)amount, fromBank), m_strand(strand) {;}
	virtual const char*   GetActionName() const {return "GANGOPS";}
	virtual const char* GetCategoryName() const {return "START_STRAND";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("type", m_strand);
	}

	int m_strand;
};
class MetricSpendGangOpsTripSkip: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGangOpsTripSkip(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {;}
	virtual const char*   GetActionName() const {return "GANGOPS";}
	virtual const char* GetCategoryName() const {return "TRIP_SKIP";}
};


class MetricSpentHangarUtilityCharges : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHangarUtilityCharges(const int amount, bool fromBank) 
		: MetricSpend((s64)amount, fromBank)
	{	
	}

	virtual const char* GetActionName() const {return "HANGAR_UTILITY";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

class MetricSpentHangarStaffCharges : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentHangarStaffCharges(const int amount, bool fromBank) 
		: MetricSpend((s64)amount, fromBank)
	{	
	}

	virtual const char* GetActionName() const {return "HANGAR_STAFF";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};


// EARN AGENCY SMUGGLER
class MetricEarnAgencySmuggler : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnAgencySmuggler(const int amount, u64 uuidValue) : MetricEarn((s64)amount, false), m_uuidValue(uuidValue)	{}

	virtual const char*   GetActionName() const {return "AGENCY";}
	virtual const char* GetCategoryName() const {return "SMUGGLER";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("uuid", m_uuidValue);
	}

private:
	u64 m_uuidValue;
};

// SPEND GANGOPS
class MetricSpentGangOpsRepairCost : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentGangOpsRepairCost(const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
	{
	}

	virtual const char* GetActionName() const { return "HEIST"; }
	virtual const char* GetCategoryName() const { return "PREP_REPAIR_COST"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw);
	}
};

// SPEND ARENA WARS
class MetricSpentArenaJoinSpectator : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentArenaJoinSpectator(const int amount, bool fromBank, int entryId)
		: MetricSpend((s64)amount, fromBank)
		, m_entryId(entryId)
	{
	}

	virtual const char* GetActionName() const { return "MATCH_FEE"; }
	virtual const char* GetCategoryName() const { return "JOB_ACTIVITY"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt("type", m_entryId);
	}

private:

	int m_entryId;
};

class MetricEarnFromArenaSkillLevelProgression : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromArenaSkillLevelProgression(const int amount, const int level) : MetricEarn((s64)amount), m_level(level) { ; }
	virtual const char*   GetActionName() const { return "SKILL_PROGRESSION"; }
	virtual const char* GetCategoryName() const { return "ARENA_CAREER"; }


	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("t", m_level);
	}

	int m_level;
};

class MetricEarnFromArenaCareerProgression : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);
public:
	MetricEarnFromArenaCareerProgression(const int amount, const int tier) : MetricEarn((s64)amount), m_tier(tier) { ; }
	virtual const char*   GetActionName() const { return "ARENA_CAREER"; }
	virtual const char* GetCategoryName() const { return "CAREER_PROGRESSION "; }


	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteInt64("t", m_tier);
	}

	int m_tier;
};


// SPEND ARENA WARS
class MetricSpentArenaSpectatorBox : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentArenaSpectatorBox(const int amount, bool fromBank, int itembought)
		: MetricSpend((s64)amount, fromBank)
		, m_itembought(itembought)
	{;}

	virtual const char* GetActionName() const { return "MATCH_FEE"; }
	virtual const char* GetCategoryName() const { return "JOB_ACTIVITY"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteInt("type", m_itembought);
	}

private:
	int m_itembought;
};

class MetricSpentArenaSpectator : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentArenaSpectator(const char* action, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
	{
		safecpy(m_action, action);
	}
	virtual const char* GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return "SPECTATOR"; }

	static const int CATEGORY_AND_ACTION_LENGTH = 64;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
};

// SPEND ARENA WARS
class MetricSpentSpinTheWheelPayment : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentSpinTheWheelPayment(const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
	{
	}
	virtual const char* GetActionName() const { return "MATCH_FEE"; }
	virtual const char* GetCategoryName() const { return "JOB_ACTIVITY"; }
};
class MetricSpentArenaPremium : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentArenaPremium(const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
	{
	}
	virtual const char* GetActionName() const { return "ARENA_WARS"; }
	virtual const char* GetCategoryName() const { return "PREMIUM_EVENT"; }
};

class MetricSpent_BuyAndUpdateGeneric : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_BuyAndUpdateGeneric(const char* action, const char* category, u32 item, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_item(item)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}

	virtual const char* GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_item);
	}

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
};

class MetricEarnGeneric : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnGeneric(const char* category, const char* action, int amount, int item = MAX_UINT32, int item2 = MAX_UINT32, int item3 = MAX_UINT32) : MetricEarn((s64)amount)
		, m_item(item)
		, m_item2(item2)
		, m_item3(item3)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}
	virtual const char*   GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricEarn::WriteContext(rw);
		if (m_item != MAX_UINT32)
		{
			result &= rw->WriteUns("type", m_item);
		}
		if (m_item2 != MAX_UINT32)
		{
			result &= rw->WriteUns("id", m_item2);
		}
		if (m_item3 != MAX_UINT32)
		{
			result &= rw->WriteUns("qtt", m_item3);
		}
		return result;
	}
private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
	u32 m_item;
	u32 m_item2;
	u32 m_item3;
};

class MetricSpendGeneric : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGeneric(const char* action, const char* category, u32 item, const int amount, bool fromBank, int context2 = 0) : MetricSpend((s64)amount, fromBank)
		, m_item(item)
		, m_context2(context2)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}
	virtual const char*   GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);
		if (m_item != 0)
		{
			result &= rw->WriteUns("item", m_item);
		}
		if (m_context2 != 0)
		{
			result &= rw->WriteUns("id", m_context2);
		}
		return result;
	}
private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
	u32 m_item;
	u32 m_context2;
};

class MetricSpendGeneric2 : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendGeneric2(const char* action, const char* category, const int amount, bool fromBank, u32 item1 = 0, u32 item2 = 0) : MetricSpend((s64)amount, fromBank)
		, m_item1(item1)
		, m_item2(item2)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}
	virtual const char*   GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);
		if (m_item1 != 0)
		{
			result &= rw->WriteUns("item", m_item1);
		}
		if (m_item2 != 0)
		{
			result &= rw->WriteUns("itemt", m_item2);
		}
		return result;
	}
private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
	u32 m_item1;
	u32 m_item2;
};

class MetricSpendCasinoMembership : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendCasinoMembership(const int amount, bool fromBank, int purchasePoint)
		: MetricSpend((s64)amount, fromBank)
		, m_purchasePoint(purchasePoint)
	{
	}
	virtual const char* GetActionName() const { return "MEMBERSHIP"; }
	virtual const char* GetCategoryName() const { return "CASINO"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_purchasePoint);
	}

private:

	int m_purchasePoint;
};

class MetricCasinoBuyChips : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCasinoBuyChips(int amount, int chips) : MetricSpend(amount, false), m_chips(chips) {}
	virtual const char*   GetActionName() const {return "BUY_CHIPS";}
	virtual const char* GetCategoryName() const {return "BETTING";}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteUns("item", m_chips);
	}

private:
	u32 m_chips;
};

class MetricUnlockCnCItem : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricUnlockCnCItem(int amount, bool fromBank, int unlockedItem) : MetricSpend(amount, fromBank), m_unlockedItem(unlockedItem) { ; }
	virtual const char*   GetActionName() const { return "CNC"; }
	virtual const char* GetCategoryName() const { return "UNLOCK"; }
	virtual bool WriteContext(RsonWriter* rw) const { return MetricSpend::WriteContext(rw) && rw->WriteUns("item", m_unlockedItem); }

private:
	u32 m_unlockedItem;
};

class MetricCasinoSellChips : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCasinoSellChips(int amount, int chips) : MetricEarn((s64)amount, true), m_chips(chips) {}
	virtual const char*   GetActionName() const { return "SELL_CHIPS"; }
	virtual const char* GetCategoryName() const { return "BETTING"; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricEarn::WriteContext(rw)
			&& rw->WriteUns("type", m_chips);
	}
private:
	u32 m_chips;
};

class MetricCnCMatchResult : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCnCMatchResult(int amount, int result) : MetricEarn((s64)amount, true), m_result(result) {}
	virtual const char*   GetActionName() const 
	{
#if ARCHIVED_SUMMER_CONTENT_ENABLED         
		return "CNC_MATCH_RESULT";
#else
		return "";
#endif	
	}
	virtual const char* GetCategoryName() const {return "JOBS";}

	virtual bool WriteContext(RsonWriter* rw) const 
	{
		return MetricEarn::WriteContext(rw) 
			&& rw->WriteInt("type", m_result);
	}
private:
	int m_result;
};

class MetricYatchMission : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricYatchMission(int amount, int mission) : MetricEarn((s64)amount), m_mission(mission) {}
	virtual const char*   GetActionName() const { return "YACHT_MISSION"; }
	virtual bool WriteContext(RsonWriter* rw) const { return MetricEarn::WriteContext(rw) && rw->WriteUns("mid", m_mission); }
private:
	u32 m_mission;
};

class MetricDispatchCall : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricDispatchCall(int amount, int mission) : MetricEarn((s64)amount), m_mission(mission) {}
	virtual const char*   GetActionName() const { return "DISPATCH_CALL"; }
	virtual bool WriteContext(RsonWriter* rw) const { return MetricEarn::WriteContext(rw) && rw->WriteUns("mid", m_mission); }

private:
	u32 m_mission;
};

class MetricSpendPurchaseCncXPBoost : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendPurchaseCncXPBoost(int amount, bool fromBank, int transactionPoint, float xpMultiplier) 
		: MetricSpend(amount, fromBank)
		, m_transactionPoint(transactionPoint)
		, m_xpMultiplier(xpMultiplier) 
	{ ; }

	virtual const char*   GetActionName() const 
	{ 		
#if ARCHIVED_SUMMER_CONTENT_ENABLED        
		return "CNC_XP_BOOST"; 
#else
		return "";
#endif		
	}
	virtual const char* GetCategoryName() const { return "XP_BOOST"; }

	virtual bool WriteContext(RsonWriter* rw) const {
		return MetricSpend::WriteContext(rw) 
			&& rw->WriteInt("txn", m_transactionPoint) 
			&& rw->WriteFloat("xpmt", m_xpMultiplier); 
	}

private:
	int m_transactionPoint;
	float m_xpMultiplier;
};

class MetricSpendVehicleRequested : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpendVehicleRequested(const int amount, bool fromBank, u32 type, u32 item) : MetricSpend((s64)amount, fromBank)
		, m_type(type)
		, m_item(item)
	{
	}

	virtual const char*   GetActionName() const { return "VEH_REQ"; }
	virtual const char* GetCategoryName() const { return "VEH"; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = MetricSpend::WriteContext(rw);

		if (m_type != 0)
		{
			result &= rw->WriteUns("type", m_type);
		}
		if (m_item != 0)
		{
			result &= rw->WriteUns("item", m_item);
		}

		return result;
	}

private:
	u32 m_type; // Hash, who this vehicle was requested from. For example Tony Prince
	u32 m_item; // Vehicle model
};

#endif // !INC_NETWORK_TRANSACTION_TELEMETRY_H_
