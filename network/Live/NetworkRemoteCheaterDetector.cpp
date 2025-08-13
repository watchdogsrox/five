//
// Filename:	NetworkRemoteCheaterDetector.h
// Description: Remote player cheater detection helper for network games
//

// Rage headers
#include "diag/channel.h"
#include "rline/rlgamerinfo.h"
#include "rline/rltelemetry.h"

// Standard include
#include "NetworkRemoteCheaterDetector.h"

 // Game includes
#include "Network/Players/NetGamePlayer.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/General/NetworkHasherUtil.h"
#include "Peds/PlayerInfo.h"

NETWORK_OPTIMISATIONS()

// ===========================================================================================================================
// METRICS
// ===========================================================================================================================

//Metric definitions for cheating.
class MetricRemoteCheat : public rlMetric
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricRemoteCheat(const char* target)
	{
		gnetAssert(target);
		if (target)
		{
			safecpy(m_target, target, COUNTOF(m_target));
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->WriteString("gamer", m_target);

		const char* cheatName = GetCheatNameId();
		if(cheatName)
		{
			result = result && rw->WriteString("action", GetCheatNameId());
			result = result && WriteContext(rw);

			extern u64 s_CurMpSessionId;
			result = result && rw->WriteInt64("id", static_cast<s64>(s_CurMpSessionId));
		}

		return result;
	}

	virtual const char* GetCheatNameId() const {return NULL;}
	virtual bool WriteContext(RsonWriter* ) const {return true;}

private:
	char m_target[RL_MAX_GAMER_HANDLE_CHARS];
};

//Cheats contained in here get sent only once per player/session. 
//  Their "send only once" logic is stored on CNetGamePlayer level
class MetricCheatCRCRequestFlood : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCheatCRCRequestFlood(const char* target, const u32 cheatExtraDescr1, const u32 cheatExtraDescr2) 
		: MetricRemoteCheat(target)
		, m_ext1(cheatExtraDescr1)
		, m_ext2(cheatExtraDescr2)
	{}

	virtual const char* GetCheatNameId() const {return "CF";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
};

//Cheats contained in here get sent by code every time they are detected
class MetricCheatCRCCompromised : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCheatCRCCompromised(const char* target, const u32 cheatExtraDescr1, const u32 cheatExtraDescr2, const u32 cheatExtraDescr3) 
		: MetricRemoteCheat(target)
		, m_ext1(cheatExtraDescr1)
		, m_ext2(cheatExtraDescr2)
		, m_ext3(cheatExtraDescr3)
	{}

	virtual const char* GetCheatNameId() const {return "CC";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
			result = result && rw->WriteUns("c", m_ext3);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
	u32  m_ext3;
};

//Cheats contained in here get sent by code every time they are detected
class MetricCheatCRCNotReplied : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCheatCRCNotReplied(const char* target, const u32 cheatExtraDescr1, const u32 cheatExtraDescr2) 
		: MetricRemoteCheat(target)
		, m_ext1(cheatExtraDescr1)
		, m_ext2(cheatExtraDescr2)
	{}

	virtual const char* GetCheatNameId() const {return "CNR";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
};

//Cheats contained in here get sent by code every time they are detected
class MetricCheatScript : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCheatScript(const char* target, const u32 cheatExtraDescr1, const u32 cheatExtraDescr2) 
		: MetricRemoteCheat(target)
		, m_ext1(cheatExtraDescr1)
		, m_ext2(cheatExtraDescr2)
	{}

	virtual const char* GetCheatNameId() const {return "SCRIPT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
};


//Cheats contained in here get sent only once per player/session. 
//  Their "send only once" logic is stored on CNetGamePlayer level
class MetricCheatEXESize : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricCheatEXESize(const char* target, const u32 cheatExtraDescr1, const u32 cheatExtraDescr2) 
		: MetricRemoteCheat(target)
		, m_ext1(cheatExtraDescr1)
		, m_ext2(cheatExtraDescr2)
	{}

	virtual const char* GetCheatNameId() const {return "ES";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
};

//Cheats contained in here get sent only once per player/session. 
//  Their "send only once" logic is stored on CNetGamePlayer level
#define MetricCheatCODECRCFAIL MetricRemoteInfoChange
class MetricCheatCODECRCFAIL : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatCODECRCFAIL(const char* target, const u32 versionAndType, const u32 address, const u32 val)
		: MetricRemoteCheat(target)
		, m_ext1(versionAndType)
		, m_ext2(address)
		, m_ext3(val)
	{}

	virtual const char* GetCheatNameId() const {return "CCF";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_ext1);
			result = result && rw->WriteUns("b", m_ext2);
			result = result && rw->WriteUns("c", m_ext3);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32  m_ext1;
	u32  m_ext2;
	u32  m_ext3;
};


#define MetricCheatCODETAMPER MetricRemoteInfoChange2
class MetricCheatCODETAMPER : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatCODETAMPER(const char* target, const u32 param)
		: MetricRemoteCheat(target), m_param(param)
	{}

	virtual const char* GetCheatNameId() const {return "CCT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_param);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32 m_param;
};

#define MetricCheatTelemetryBlock MetricRemoteInfoChange3
class MetricCheatTelemetryBlock : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatTelemetryBlock(const char* target)
		: MetricRemoteCheat(target)
	{}

	virtual const char* GetCheatNameId() const {return "RTB";}
};

#if RSG_PC

#define MetricCheatGameServerCashWallet MetricRemoteInfoChange4
class MetricCheatGameServerCashWallet : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatGameServerCashWallet(const char* target, const u32 param)
		: MetricRemoteCheat(target)
		, m_param(param)
	{}

	virtual const char* GetCheatNameId() const {return "GSCW";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_param);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32 m_param;
};

#define MetricCheatGameServerCashBank MetricRemoteInfoChange5
class MetricCheatGameServerCashBank : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatGameServerCashBank(const char* target, const u32 param)
		: MetricRemoteCheat(target)
		, m_param(param)
	{}

	virtual const char* GetCheatNameId() const {return "GSCB";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_param);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32 m_param;
};

#define MetricCheatGameServerInventory MetricRemoteInfoChange6
class MetricCheatGameServerInventory : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatGameServerInventory(const char* target, const u32 param)
		: MetricRemoteCheat(target)
		, m_param(param)
	{}

	virtual const char* GetCheatNameId() const {return "GSINV";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_param);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32 m_param;
};

#define MetricCheatGameServerIntegrity MetricRemoteInfoChange7
class MetricCheatGameServerIntegrity : public MetricRemoteCheat
{
	RL_DECLARE_METRIC(DIG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	// NB: Pass obfuscated values in here please!
	MetricCheatGameServerIntegrity(const char* target, const u32 param)
		: MetricRemoteCheat(target)
		, m_param(param)
	{}

	virtual const char* GetCheatNameId() const {return "GSINT";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;

		result = result && rw->Begin("data", NULL);
		{
			result = result && rw->WriteUns("a", m_param);
		}
		result = result && rw->End();

		return result;
	}

private:
	u32 m_param;
};

#endif //RSG_PC

// ===========================================================================================================================
// GENERAL HELPERS
// ===========================================================================================================================

void NetworkRemoteCheaterDetector::NotifyPlayerLeftBadSport(rlGamerHandle& remoteHandler, u32 cheatDescr1, u32 cheatDescr2)
{
	if(gnetVerify(remoteHandler.IsValid()) && gnetVerify(!remoteHandler.IsLocal()))
	{
		char target[RL_MAX_GAMER_HANDLE_CHARS];
		remoteHandler.ToString(target);

		BANK_ONLY(Warningf("Remote bad sport leave detected! Cheat [%d , extr %d + %d] target %s", SCRIPT_CHEAT_DETECTION, cheatDescr1, cheatDescr2, target);)
		gnetVerify(CNetworkTelemetry::AppendMetric(MetricCheatScript(target, cheatDescr1, cheatDescr2)));
	}
}

void NetworkRemoteCheaterDetector::RemoteCheaterDetected(CNetGamePlayer* pNetPlayer, eDETECTABLE_CHEAT cheat,  u32 cheatExtraData1, u32 cheatExtraData2, u32 cheatExtraData3)
{
	if( pNetPlayer && pNetPlayer->GetPlayerInfo() )
	{
		const u32 playerPhysIndex = (const u32)pNetPlayer->GetPhysicalPlayerIndex();

		if( gnetVerifyf( playerPhysIndex< MAX_NUM_PHYSICAL_PLAYERS, "NetworkRemoteCheaterDetector::RemoteCheaterDetected: Wrong player physical index (%d)! Leaving(%d)  [%d %d %d]", playerPhysIndex, pNetPlayer->IsLeaving(), cheat, cheatExtraData1, cheatExtraData2) )
		{
			//Must be valid!
			gnetAssert(pNetPlayer->GetPlayerInfo()->m_GamerInfo.IsValid());

			//Must be a remote player?
			gnetAssert(pNetPlayer->GetPlayerInfo()->m_GamerInfo.IsRemote());

			char target[RL_MAX_GAMER_HANDLE_CHARS];
			pNetPlayer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle().ToString(target);

			gnetDebug3("Remote cheater %s detected! Cheat [%d , extr 0x%08X + 0x%08X + 0x%08X] on player %s", target, cheat, cheatExtraData1, cheatExtraData2, cheatExtraData3, pNetPlayer->GetLogName());

			// Only send if this cheat can be detected N times per player & session, or if it hasn't been detected yet
			if (SCRIPT_CHEAT_DETECTION == cheat)
			{
				gnetVerify(CNetworkTelemetry::AppendMetric(MetricCheatScript(target, cheatExtraData1, cheatExtraData2)));
			}
			else if(gnetVerifyf(cheat < MAX_DETECTABLE_ONCE_CHEATS, "Unrecognized cheat id %d.", cheat))
			{
				// Can only be sent if it hasn't been detected yet.
				if (!pNetPlayer->IsCheatAlreadyNotified((u32)cheat, cheatExtraData1))
				{
					bool notified = false;

					if(CRC_COMPROMISED == cheat)
					{
						gnetAssert(cheatExtraData1 < NetworkHasherUtil::INVALID_HASHABLE);
						notified = CNetworkTelemetry::AppendMetric(MetricCheatCRCCompromised(target, cheatExtraData1, cheatExtraData2, cheatExtraData3));
					}
					else if (CRC_NOT_REPLIED == cheat)
					{
						gnetAssert(cheatExtraData1 < NetworkHasherUtil::INVALID_HASHABLE);
						notified = CNetworkTelemetry::AppendMetric(MetricCheatCRCNotReplied(target, cheatExtraData1, cheatExtraData2));
					}
					else if (CRC_REQUEST_FLOOD == cheat)
					{
						gnetAssert(cheatExtraData1 < NetworkHasherUtil::INVALID_HASHABLE);
						notified = CNetworkTelemetry::AppendMetric(MetricCheatCRCRequestFlood(target, cheatExtraData1, cheatExtraData2));
					}
					else if (CRC_EXE_SIZE == cheat)
					{
#if RSG_PC
						// Just set to true; we're not going to notify the EXE size for PC, because 
						// this is firing whenever a Steam user encounters a non-Steam user. It's just
						// flooding the database. The best thing to do is to just forge it along as 
						// if it's been notified.
						notified = true;
#else
						notified = CNetworkTelemetry::AppendMetric(MetricCheatEXESize(target, cheatExtraData1, cheatExtraData2));
#endif
					}
					else if (CRC_CODE_CRCS == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatCODECRCFAIL(target, cheatExtraData1, cheatExtraData2, cheatExtraData3));
					}
					else if (CODE_TAMPERING == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatCODETAMPER(target, cheatExtraData1));
					}
					else if (TELEMETRY_BLOCK == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatTelemetryBlock(target));
					}
#if RSG_PC
					else if (GAME_SERVER_CASH_WALLET == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatGameServerCashWallet(target, cheatExtraData1));
					}
					else if (GAME_SERVER_CASH_BANK == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatGameServerCashBank(target, cheatExtraData1));
					}
					else if (GAME_SERVER_INVENTORY == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatGameServerInventory(target, cheatExtraData1));
					}
					else if (GAME_SERVER_SERVER_INTEGRITY == cheat)
					{
						notified = CNetworkTelemetry::AppendMetric(MetricCheatGameServerIntegrity(target, cheatExtraData1));
					}
#endif //RSG_PC
					else
					{
						gnetError("Unhandled send once cheat detected %d", cheat);
					}

					// Set it has been detected.
					if (gnetVerifyf(notified, "Failed to append cheat %d", cheat))
					{
						pNetPlayer->SetCheatAsNotified((u32)cheat, cheatExtraData1);
					}
				}
			}
		}
	}
}
