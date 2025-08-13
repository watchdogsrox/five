//
// StatsDataMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

//Rage Headers
#include "atl/inmap.h"
#include "system/param.h"
#include "rline/rl.h"
#include "net/nethardware.h"
#include "net/netfuncprofiler.h"

#if !__FINAL
#include "math/random.h"
#endif

#if __BANK
#include "bank/bank.h"
#endif

//Framework Headers
#include "fwnet/netchannel.h"
#include "fwnet/netutils.h "
#include "fwsys/timer.h"
#include "fwsys/gameskeleton.h"

//Game Headers
#include "control/stuntjump.h"
#include "NetworkProfileStats.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Stats/stats_channel.h"
#include "STats/MoneyInterface.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsDataMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Frontend/ProfileSettings.h"
#include "frontend/PauseMenuData.h"
#include "SaveMigration/SaveMigration.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, profile_stats, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_profile_stats

PARAM(noprofilestats, "[] Disable Profile Stats");
XPARAM(nompsavegame);
XPARAM(lan);

#if !__FINAL
PARAM(nosynchronizesometimes, "[net_profile_stats] Simulate profile stats synchronize failure.");
static mthRandom s_SynchronizeRng;

PARAM(nocredentialsometimes, "[net_profile_stats] Simulate profile stats credentials failure.");
static mthRandom s_CredentialsRng;

PARAM(noflushsometimes, "[net_profile_stats] Simulate profile stats flush failure.");
static mthRandom s_FlushRng;
#endif

#if GEN9_STANDALONE_ENABLED && !__FINAL
PARAM(profileGenerationOverride, "[net_profile_stats] override the profile level stat for the generation, 0 : none, 7 gen7, 8 gen8, 9 gen9"); 
#endif


//Stat used to set the value for the last profile stats Flush Posix Time.
static StatId s_StatLastFlush("PROFILE_STATS_LAST_FLUSH");

//Used for specifyin the profile stat metric operation
enum eProfileStatsOperationTriggerType { 
	SynchronizeGroups = eMenuScreen_NUM_ENUMS
	, ConditionalSynchronizeGroups 
};

// ---------------------------------- CProfileStatsRecord 

void 
CProfileStatsRecords::ResetRecords(const unsigned numValues, rlProfileStatsValue* records)
{
	if (m_Records)
	{
		delete [] m_Records;
		m_Records    = 0;
		m_NumRecords = 0;
	}

	m_NumRecords = numValues;
	m_Records    = records;
}

const rlProfileStatsValue& 
CProfileStatsRecords::GetValue(const unsigned column) const
{
	if (gnetVerify(column < m_NumRecords) && gnetVerify(m_Records))
	{
		return m_Records[column];
	}

	static rlProfileStatsValue record;
	return record;
}

// ---------------------------------- CProfileStatsRead 

void  
CProfileStatsRead::Shutdown()
{
	Clear();
}

void 
CProfileStatsRead::Reset(const unsigned numStats, int* statIds, const unsigned numGamers,  CProfileStatsRecords* gamerRecords)
{
	gnetAssert(!Pending());

	Clear();

	if (gnetVerify(0 < numGamers) && gnetVerify(gamerRecords))
	{
		m_Results.Reset(statIds, numStats, numGamers, gamerRecords, sizeof(CProfileStatsRecords));
	}
}

void  
CProfileStatsRead::Cancel()
{
	gnetAssert(Pending());

	if (Pending())
	{
		gnetWarning("Canceling remote player's profile stats read");
		rlProfileStats::Cancel(&m_Status);
	}
}

void  
CProfileStatsRead::Clear()
{
	if (Pending())
		Cancel();

	m_Status.Reset();
	m_Results.Clear();
}

void  
CProfileStatsRead::Start(const int localGamerIndex, rlGamerHandle* gamers, unsigned numGamers)
{
	if (gnetVerify(netStatus::NET_STATUS_NONE == m_Status.GetStatus()))
	{
		gnetVerify(rlProfileStats::ReadStatsByGamer(localGamerIndex
													,gamers
													,numGamers
													,&m_Results
													,&m_Status));
	}
}

const rlProfileStatsValue* 
CProfileStatsRead::GetStatValue(const int statId, const rlGamerHandle& gamer) const
{
	gnetAssert(m_Status.Succeeded());

	for (u32 row=0; row<m_Results.GetNumRows(); row++)
	{
		if (*(m_Results.GetGamerHandle(row)) == gamer)
		{
			for (u32 column=0; column<m_Results.GetNumStatIds(); column++)
			{
				const rlProfileStatsValue* val = m_Results.GetValue(row, column);
				if (m_Results.GetStatId(column) == statId)
				{
					return val;
				}
			}
		}
	}

	return NULL;
}

unsigned 
CProfileStatsRead::GetNumberStats(const rlGamerHandle& gamer) const
{
	gnetAssert(m_Status.Succeeded());
	for (int i=0; i<m_Results.GetNumRows(); i++)
	{
		if (*m_Results.GetGamerHandle(i) == gamer)
		{
			return m_Results.GetNumStatIds();
		}
	}

	return 0;
}

int  
CProfileStatsRead::GetStatId(const int column) const 
{
	gnetAssert(m_Status.Succeeded());
	return m_Results.GetStatId(column);
}

unsigned
CProfileStatsRead::GetNumberStatIds() const 
{
	gnetAssert(m_Status.Succeeded());
	return m_Results.GetNumStatIds();
}


// ---------------------------------- CProfileStatsReadScriptRequest 

void CProfileStatsReadScriptRequest::Clear()
{
	m_Reader.Clear();
	m_GamerHandlers.clear();
	m_StatIds.clear();
	m_Records.clear();
	m_RecordValues.clear();
}

bool CProfileStatsReadScriptRequest::GamerExists(const rlGamerHandle& gamer) const 
{
	if (gnetVerify(gamer.IsValid()))
	{
		for (u32 i=0; i<m_GamerHandlers.GetCount(); i++)
		{
			if (m_GamerHandlers[i] == gamer)
				return true;
		}
	}

	return false;
}

bool CProfileStatsReadScriptRequest::AddGamer(const rlGamerHandle& gamer)
{
	const u32 MAX_NUM_GAMERS = RLSC_FRIENDS_MAX_FRIENDS;

	if (gnetVerifyf(m_GamerHandlers.GetCount() < MAX_NUM_GAMERS, "Max number of gamers=%d, reached", MAX_NUM_GAMERS))
	{
		if (gnetVerify( !m_Reader.Pending() ) && gnetVerify( gamer.IsValid() ) && gnetVerify( !GamerExists(gamer) ))
		{
			m_GamerHandlers.PushAndGrow(gamer);
			return true;
		}
	}

	return false;
}

bool CProfileStatsReadScriptRequest::StatIdExists(const int id) const 
{
	for (u32 i=0; i<m_StatIds.GetCount(); i++)
	{
		if (m_StatIds[i] == id)
			return true;
	}

	return false;
}

bool CProfileStatsReadScriptRequest::AddStatId(const int id)
{
	if (gnetVerify(!m_Reader.Pending()) && gnetVerify(!StatIdExists(id)) && gnetVerify(StatsInterface::IsKeyValid(id)))
	{
		m_StatIds.PushAndGrow(id);
		return true;
	}

	return false;
}

bool CProfileStatsReadScriptRequest::StartRead()
{
	gnetAssert(m_GamerHandlers.GetCount() > 0);
	if (m_GamerHandlers.GetCount() <= 0)
		return false;

	gnetAssert(m_StatIds.GetCount() > 0);
	if (m_StatIds.GetCount() <= 0)
		return false;

	gnetAssert(!m_Reader.Pending());
	if (m_Reader.Pending())
		return false;

	m_Reader.Clear();

	const u32 numgamers = m_GamerHandlers.GetCount();
	const u32 numcolumns = m_StatIds.GetCount();

	m_Records.clear( );
	m_Records.ResizeGrow( numgamers );

	m_RecordValues.clear( );
	m_RecordValues.ResizeGrow( numgamers );

	for (int i=0; i<numgamers; i++)
	{
		m_RecordValues[i].m_Values.clear();
		m_RecordValues[i].m_Values.ResizeGrow( numcolumns );
		m_Records[i].ResetRecords( numcolumns, m_RecordValues[i].m_Values.GetElements() );
	}

	m_Reader.Reset(numcolumns, m_StatIds.GetElements(), numgamers, m_Records.GetElements());
	m_Reader.Start(NetworkInterface::GetLocalGamerIndex(), m_GamerHandlers.GetElements(), numgamers);

	return true;
}


// ---------------------------------- CProfileStatsReadSessionStats 

void  
CProfileStatsReadSessionStats::Init(const unsigned numHighPriorityStatIds
									,int* highPriorityStatIds
									,CProfileStatsRecords* highPriorityRecords
									,const unsigned numLowPriorityStatIds
									,int* lowPriorityStatIds
									,CProfileStatsRecords* lowPriorityRecords
									,const unsigned maxNumGamers
									,EndReadCallback& cb)
{
	if (!gnetVerify(!m_Initialized))
		return;

	Shutdown();

	gnetAssert(cb.IsBound());
	gnetAssert(0 < maxNumGamers && maxNumGamers <= MAX_NUM_REMOTE_GAMERS);

	if (0 < maxNumGamers && maxNumGamers <= MAX_NUM_REMOTE_GAMERS && cb.IsBound())
	{
		m_EndReadCB = cb;

		m_MaxNumGamerRecords = maxNumGamers;

		for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
		{
			m_Gamers[i] = INVALID_PLAYER_INDEX;
		}

		m_NumHighPriorityStatIds   = numHighPriorityStatIds;
		m_HighPriorityStatIds      = highPriorityStatIds;
		m_HighPriorityGamerRecords = highPriorityRecords;

		m_NumLowPriorityStatIds   = numLowPriorityStatIds;
		m_LowPriorityStatIds      = lowPriorityStatIds;
		m_LowPriorityGamerRecords = lowPriorityRecords;

		m_Initialized = true;
	}
}

bool  
CProfileStatsReadSessionStats::ReadStats(const unsigned priority)
{
	if (!m_Initialized)
		return false;

	if (!gnetVerify(NetworkInterface::IsGameInProgress()))
		return false;

	if (!gnetVerify(!m_Reader.Pending()))
		return false;

	if (!gnetVerify(0 == m_TotalNumGamers))
		return false;

	//Make sure we cleanup everything
	EndStatsRead();

	//Set download priority
	m_Priority = priority;

	//Make sure we download in the next update frame
	m_ReadDelayTimer = (sysTimer::GetSystemMsTime() | 0x01) + SYNCH_DELAY_TIMER;

	//Set stats to be synched in the next update
	m_StartStatsRead = true;

	return true;
}

void  
CProfileStatsReadSessionStats::Shutdown()
{
	if (!m_Initialized)
		return;

	EndStatsRead();

	m_EndReadCB.Unbind();

	CNetwork::UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
	m_Reader.Shutdown();
	CNetwork::StopUsingNetworkHeap();

	m_NumHighPriorityStatIds   = 0;
	m_HighPriorityStatIds      = 0;
	m_HighPriorityGamerRecords = 0;

	m_NumLowPriorityStatIds   = 0;
	m_LowPriorityStatIds      = 0;
	m_LowPriorityGamerRecords = 0;

	m_Initialized = false;
}

void
CProfileStatsReadSessionStats::Update()
{
	if (!m_Initialized)
		return;

	//Currently not doing anything
	if (m_Reader.Idle())
	{
		const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;
		if (m_StartStatsRead && (curTime - m_ReadDelayTimer) >= SYNCH_DELAY_TIMER)
		{
			StartStatsRead();
			m_StartStatsRead = false;

			if (m_ReadForAllPriorities)
			{
				if (PS_HIGH_PRIORITY == m_Priority)
				{
					m_Priority = PS_LOW_PRIORITY;
					m_StartStatsRead = true;
				}
				else
				{
					m_ReadForAllPriorities = false;
				}
			}
		}
		else if (!m_StartStatsRead)
		{
			if ((curTime - m_LastHighPrioritySynch) >= TIME_FOR_HIGH_PRIORITY_SYNCH)
			{
				ReadStats(PS_HIGH_PRIORITY);
			}
			else if ((curTime - m_LastLowPrioritySynch) >= TIME_FOR_LOW_PRIORITY_SYNCH)
			{
				ReadStats(PS_LOW_PRIORITY);
			}
		}

		return;
	}

	//Read Operation Pending
	if (m_Reader.Pending())
		return;

	gnetDebug1("Finish Read profile stats for remote players.");
	gnetDebug1(".... Result = %s", m_Reader.Succeeded() ? "Succeeded" : ( m_Reader.Failed() ? "Failed" : "Canceled" ));

	m_EndReadCB( m_Reader.Succeeded() );

	if (m_Reader.Succeeded())
	{
#if __DEV
		for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
		{
			if (INVALID_PLAYER_INDEX != m_Gamers[i])
			{
				CNetGamePlayer* player = NetworkInterface::GetActivePlayerFromIndex(m_Gamers[i]);
				if (gnetVerify(player))
				{
					gnetAssert(!player->GetGamerInfo().GetGamerHandle().IsLocal());
					gnetAssert(player->GetGamerInfo().GetGamerHandle().IsValid());

					const rlGamerHandle& gamer = player->GetGamerInfo().GetGamerHandle();

					const unsigned numstats = m_Reader.GetNumberStats(gamer);
					gnetAssert(0 < numstats);

					gnetDebug1("Profile Stats downloaded:");
					gnetDebug1("                   Gamer - %s", player->GetLogName());
					gnetDebug1("            Number stats - %d", numstats);

					for (unsigned i=0; i<numstats; i++)
					{
						const int statId = m_Reader.GetStatId( i );
						const rlProfileStatsValue* psValue = m_Reader.GetStatValue(statId, gamer);
						if (gnetVerify(psValue))
						{
							char statsMsg[255];
							gnetDebug1("                    Stat - id='%s:%d' - value='%s'", StatsInterface::GetKeyName(statId), statId, psValue->ToString(statsMsg, 255));
						}
					}
				}
			}
		}
#endif // __DEV
	}

	//We are done downloading all Shared stats
	EndStatsRead();
}

bool 
CProfileStatsReadSessionStats::GamerExists(const ActivePlayerIndex playerIndex) const
{
	bool exists = false;

	gnetAssert(m_Initialized);
	if (m_Initialized && INVALID_PLAYER_INDEX != playerIndex)
	{
		for (int i=0; i<MAX_NUM_REMOTE_GAMERS && !exists; i++)
		{
			exists = ((INVALID_PLAYER_INDEX != m_Gamers[i]) && (playerIndex == m_Gamers[i]));
		}
	}

	return exists;
}

void
CProfileStatsReadSessionStats::PlayerHasLeft(const ActivePlayerIndex OUTPUT_ONLY(playerIndex))
{
	if (!m_Initialized)
		return;

	gnetDebug1("PlayerHasLeft - \"%s\" - Player index=\"%d\".", (INVALID_PLAYER_INDEX != playerIndex && NetworkInterface::GetActivePlayerFromIndex(playerIndex)) ? NetworkInterface::GetActivePlayerFromIndex(playerIndex)->GetLogName() : "Unknown Gamer", playerIndex);

	//Operation in progress
	if (m_Reader.Pending())
	{
		EndStatsRead();
		ReadStats(m_Priority);
		m_ReadDelayTimer = sysTimer::GetSystemMsTime() | 0x01;
	}
}

void  
CProfileStatsReadSessionStats::PlayerHasJoined(const ActivePlayerIndex OUTPUT_ONLY(playerIndex))
{
	gnetDebug1("PlayerHasJoined - \"%s\" - Player index=\"%d\".", (INVALID_PLAYER_INDEX != playerIndex && NetworkInterface::GetActivePlayerFromIndex(playerIndex)) ? NetworkInterface::GetActivePlayerFromIndex(playerIndex)->GetLogName() : "Unknown Gamer", playerIndex);

	//Operation in progress
	if (m_Initialized && m_Reader.Pending())
	{
		EndStatsRead();
	}

	m_Priority             = PS_HIGH_PRIORITY;
	m_ReadForAllPriorities = true;
	m_StartStatsRead       = true;
	m_ReadDelayTimer       = sysTimer::GetSystemMsTime() | 0x01;
}

void
CProfileStatsReadSessionStats::StartStatsRead()
{
	gnetAssert(m_Initialized);
	gnetAssert(m_Reader.Idle());
	if (!m_Initialized || !m_Reader.Idle())
		return;

	//Records
	CProfileStatsRecords* records = (m_Priority == PS_HIGH_PRIORITY) ? m_HighPriorityGamerRecords : m_LowPriorityGamerRecords;
	gnetAssert(records);
	if (!records)
		return;

	//number of stats
	const unsigned numStats = (m_Priority == PS_HIGH_PRIORITY) ? m_NumHighPriorityStatIds : m_NumLowPriorityStatIds;
	if (0 == numStats)
		return;

	//Stat ids
	int* statIds = (m_Priority == PS_HIGH_PRIORITY) ? m_HighPriorityStatIds : m_HighPriorityStatIds;
	gnetAssert(statIds);
	if (!statIds)
		return;

	//Get the new number of gamers
	gnetAssert(0 == m_TotalNumGamers);
	m_TotalNumGamers = 0;
	{
		//Get the number of Gamers
		unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const netPlayer *player = remoteActivePlayers[index];

			if (!player->IsLocal())
			{
				ASSERT_ONLY(const rlGamerHandle gamerHandler = player->GetGamerInfo().GetGamerHandle();)
				ASSERT_ONLY(gnetAssertf(gamerHandler.IsValid(), "Gamer handler is not valid.");)

				if (INVALID_PLAYER_INDEX != player->GetActivePlayerIndex() && !GamerExists(player->GetActivePlayerIndex()))
				{
					m_Gamers[m_TotalNumGamers] = player->GetActivePlayerIndex();
					m_TotalNumGamers++;
				}
			}
		}
	}

	if (0 < m_TotalNumGamers)
	{
		m_TotalNumGamers++;

		gnetDebug1("Read profile stats for Remote Players.");
		gnetDebug1("..... Priority   = %s", (m_Priority == PS_HIGH_PRIORITY) ? "HIGH_PRIORITY" : "LOW_PRIORITY");

		CNetwork::UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
		m_Reader.Reset(numStats, statIds, m_TotalNumGamers, records);
		CNetwork::StopUsingNetworkHeap();

		unsigned numGamers = 0;
		for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
		{
			if (INVALID_PLAYER_INDEX != m_Gamers[i])
			{
				CNetGamePlayer* player = NetworkInterface::GetActivePlayerFromIndex(m_Gamers[i]);
				if (gnetVerify(player))
				{
					gnetAssert(!player->GetGamerInfo().GetGamerHandle().IsLocal());
					gnetAssert(player->GetGamerInfo().GetGamerHandle().IsValid());

					//Add Gamer
					gnetDebug1("...... Add gamer = %s", player->GetLogName());
					m_GamerHandlers[numGamers] = player->GetGamerInfo().GetGamerHandle();
					numGamers++;
				}
			}
		}

		m_Reader.Start(NetworkInterface::GetLocalGamerIndex(), m_GamerHandlers, numGamers);

		//reset timers
		if (m_Priority == PS_HIGH_PRIORITY)
		{
			m_LastHighPrioritySynch = sysTimer::GetSystemMsTime() | 0x01;;
		}
		else if (m_Priority == PS_LOW_PRIORITY)
		{
			m_LastLowPrioritySynch = sysTimer::GetSystemMsTime() | 0x01;;
		}
	}
	else
	{
		EndStatsRead();
	}
}

void  
CProfileStatsReadSessionStats::EndStatsRead()
{
	gnetAssert(m_Initialized);
	if (!m_Initialized)
		return;

	CNetwork::UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
	if (m_Reader.Pending())
	{
		m_Reader.Cancel();
	}
	m_Reader.Clear();
	CNetwork::StopUsingNetworkHeap();

	m_TotalNumGamers = 0;
	for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
	{
		m_Gamers[i] = INVALID_PLAYER_INDEX;
		m_GamerHandlers[i].Clear();
	}
}

// ---------------------------------- CProfileStats

void CProfileStats::sSynchOperation::Shutdown()
{
	Cancel();
	m_SuccessCounterForSP = 0;
	m_SuccessCounterForMP = 0;
	m_ForceInterval = 0;
	m_Force = false;
	m_FailedMP = false;
	m_FailedSP = false;
	m_httpErrorCode = 0;
}

void CProfileStats::sSynchOperation::Cancel( )
{
	if (!m_Status.Pending())
		return;

	if (m_OperationId == PS_SYNCH_IDLE)
		return;

	gnetDebug1("Profile stats synch canceled: %s - counter=%d, time=%d"
		,(PS_SYNC_SP==m_OperationId)?"PS_SYNC_SP":"PS_SYNC_MP"
		,(PS_SYNC_SP==m_OperationId)?m_SuccessCounterForSP:m_SuccessCounterForMP
		,sysTimer::GetSystemMsTime() - m_Timer);

	rlProfileStats::Cancel(&m_Status);
	rlProfileStats::Update();
	m_Status.Reset();
	m_OperationId = PS_SYNCH_IDLE;
}

void CProfileStats::sSynchOperation::OperationDone(const bool success)
{
	if (m_OperationId == PS_SYNCH_IDLE)
		return;

	if (success)
	{
		if (PS_SYNC_SP == m_OperationId)
			m_SuccessCounterForSP += 1;
		else if (gnetVerify(PS_SYNC_MP == m_OperationId))
		{
			m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;
			m_SuccessCounterForMP += 1;

			//HACK - 5th Character does not exist just now.
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_5, true, true);

			MoneyInterface::SyncAndValidateBalances();
			MoneyInterface::CheckForCheaterStatsClear();

			if(gnetVerifyf(SStuntJumpManager::IsInstantiated(), "CProfileStats::sSynchOperation::OperationDone - the stunt jump manager is not initialized yet"))
			{
				SStuntJumpManager::GetInstance().ValidateStuntJumpStats(false);
			}
		}

		gnetDebug1("SUCCESS: profile stats synch %s - counter=%d, time=%d"
						 ,(PS_SYNC_SP==m_OperationId)?"PS_SYNC_SP":"PS_SYNC_MP"
						 ,(PS_SYNC_SP==m_OperationId)?m_SuccessCounterForSP:m_SuccessCounterForMP
						 ,sysTimer::GetSystemMsTime() - m_Timer);
	}
	else
	{
		gnetWarning("FAILED: profile stats synch %s - counter=%d, time=%d, enable re-synch"
						,(PS_SYNC_SP==m_OperationId)?"PS_SYNC_SP":"PS_SYNC_MP"
						,sysTimer::GetSystemMsTime() - m_Timer
						,(PS_SYNC_SP==m_OperationId)?m_SuccessCounterForSP:m_SuccessCounterForMP);

		if (PS_SYNC_MP == m_OperationId)
		{
			m_SuccessCounterForMP = 0;
			m_FailedMP = true;
			m_httpErrorCode = m_Status.GetResultCode();
		}
		else
		{
			m_Force = true;
			m_ForceInterval += FORCE_SYNCH_INTERVAL;
			m_FailedSP = true;
		}
	}

	m_OperationId = PS_SYNCH_IDLE;

	m_Status.Reset();
}


CProfileStats::CProfileStats()
: m_FlushInProgress(false)
, m_FlushOnlineStats(false)
, m_FlushFailedMP(false)
, m_ForceFlush(false)
, m_minForceFlushTime(0)
, m_lastForceFlushTime(0)
, m_CouldntFlushAllStats(false)
NOTFINAL_ONLY(, m_SimulateCredentialsFailPct(0))
NOTFINAL_ONLY(, m_SimulateFlushFailPct(0))
, m_ForceCloudOverwriteOnMPLocalStats(false)
, m_FlushPosixtimeMP(0)
, m_FlushPreviousPosixTime(0)
BANK_ONLY(, m_ClearSaveTransferStats(false))
, m_MPSynchIsDone(false)
GEN9_STANDALONE_ONLY(, m_MPMigrationStatusChecked(false))
{
	m_EventDlgt.Bind(this, &CProfileStats::HandleEvent);
}

CProfileStats::~CProfileStats()
{
	Shutdown(SHUTDOWN_CORE);
}

void
CProfileStats::Init()
{
	//Make sure we have a timer to force a synch.
	if (!m_SynchOp.m_SuccessCounterForSP)
	{
		m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;
	}

	if (PARAM_noprofilestats.Get())
		return;

	if (rlProfileStats::IsInitialized())
		return;

#if !__FINAL
	if (!PARAM_nosynchronizesometimes.Get(m_SynchOp.m_SimulateFailPct))
		m_SynchOp.m_SimulateFailPct = 0;

	if (!PARAM_nocredentialsometimes.Get(m_SimulateCredentialsFailPct))
		m_SimulateCredentialsFailPct = 0;

	if (!PARAM_noflushsometimes.Get(m_SimulateFlushFailPct))
		m_SimulateFlushFailPct = 0;
#endif 

	NET_ASSERTS_ONLY(bool result =) rlProfileStats::Init(m_EventDlgt, MAX_SUBMISSION_SIZE);
	gnetAssert(result);

	m_ForceCloudOverwriteOnMPLocalStats = false;
}

#if __BANK
void 
CProfileStats::Bank_InitWidgets( bkBank& bank )
{
	bank.PushGroup("Profile Stats");
	{
		bank.AddSlider("nosynchronizesometimes", &m_SynchOp.m_SimulateFailPct, 0, 100, 1);
		bank.AddSlider("nocredentialsometimes", &m_SimulateCredentialsFailPct, 0, 100, 1);
		bank.AddSlider("noflushsometimes", &m_SimulateFlushFailPct, 0, 100, 1);
		bank.AddButton("Clear all save transfer stats.",  datCallback(MFA(CProfileStats::Bank_ClearAllSaveTransferData), (datBase*)this));
	}
	bank.PopGroup();
}

void
CProfileStats::Bank_ClearAllSaveTransferData()
{
	gnetAssert(!PARAM_noprofilestats.Get());
	gnetAssert(NetworkInterface::IsLocalPlayerOnline());
	gnetAssert(rlProfileStats::IsInitialized());
	gnetAssert(!m_FlushInProgress);

	if(gnetVerifyf(!NetworkInterface::IsNetworkOpen(), "THIS WIDGET CAN ONLY BE USED DURING SINGLE PLAYER."))
	{
		m_ClearSaveTransferStats = true;
		m_ForceFlush             = true;
		m_FlushOnlineStats       = true;
	}
}
#endif

void
CProfileStats::CancelPendingFlush()
{
	//Any pending operations are canceled.
	if (m_FlushStatus.Pending())
	{
		gnetDebug1("CancelPendingFlush: Canceling Stats FUSH in progress.");
		rlProfileStats::Cancel(&m_FlushStatus);
	}

	//Make sure any pending Force Flushes are disabled.
	m_ForceFlush = false;
}

void 
CProfileStats::Shutdown(const u32 shutdownMode)
{
	if (PARAM_noprofilestats.Get())
		return;

	//Any pending operations are canceled.
	if (m_FlushStatus.Pending())
	{
		//Run Update after to flush canceled tasks
		rlProfileStats::Cancel(&m_FlushStatus);
		rlProfileStats::Update();
	}
	m_FlushStatus.Reset();

	//Any pending operations are canceled.
	if (m_SynchOp.m_Status.Pending())
	{
		//Run Update after to flush cancelled tasks
		rlProfileStats::Cancel(&m_SynchOp.m_Status);
		rlProfileStats::Update();
	}
	m_SynchOp.m_Status.Reset();

	m_SynchOp.m_OperationId = PS_SYNCH_IDLE;
	m_FlushInProgress = false;
	m_FlushOnlineStats = false;
    m_CouldntFlushAllStats = false;

	for(u32 i=PS_GRP_MP_0; i<=PS_GRP_SP_END; i++)
		m_ResetGroups.UnSet(i);

	gnetWarning("Profile Stats Flush - Clearing Flag m_FlushFailedMP - CProfileStats::Shutdown()");
	m_FlushFailedMP = false;

	bool shutdown = (shutdownMode != SHUTDOWN_CORE);

	//We shutdown if the the player is not Online or it is a core shutdown.
	if (!NetworkInterface::IsLocalPlayerOnline())
		shutdown = true;

	if (shutdown && rlProfileStats::IsInitialized())
	{
		m_SynchOp.Shutdown();
		m_ForceFlush = false;

		if (shutdownMode == SHUTDOWN_CORE)
		{
			gnetWarning("rlProfileStats::Shutdown() - Calling CProfileStats::Shutdown().");
			rlProfileStats::Shutdown();
			gnetWarning("Done with rlProfileStats::Shutdown().");
		}
	}

	m_ForceCloudOverwriteOnMPLocalStats = false;
	MoneyInterface::ClearFlagForResync();

	m_MPSynchIsDone = false;

	m_FlushPosixtimeMP = 0;
	m_FlushPreviousPosixTime = 0;
}

void  
CProfileStats::CheckAndTryFlushing( )
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		const rlProfileStatsFlushStatus flushStatus = rlProfileStats::GetFlushStatus(NetworkInterface::GetLocalGamerIndex());

		// Do not try to flush anything if blocked!
		if(StatsInterface::GetBlockSaveRequests())
		{
			return;
		}

		switch (flushStatus)
		{
		case RL_PROFILESTATS_FLUSH_READY:
		case RL_PROFILESTATS_FLUSH_TOO_SOON_SINCE_LAST:

			// Flush if forcing, or if we failed to flush all stats the last time around
			if (m_ForceFlush || m_CouldntFlushAllStats)
			{
				////During Transitions we want to ignore to catch more stats flushes in one go.
				//if (g_PlayerSwitch.IsActive())
				//	break;

				//We need the read the tunable to be able to flush.
				if (m_ForceFlush && 0 == m_minForceFlushTime)
					break;

				const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
				if (!m_FlushOnlineStats && m_ForceFlush && m_lastForceFlushTime>0 && currentTime-m_lastForceFlushTime <= m_minForceFlushTime)
				{
                    // Allow us to bypass the force flush frequency limit if we couldn't flush all stats in one go
                    if (!m_CouldntFlushAllStats)
                    {
					    gnetWarning("CProfileStats::Flush: Force flush averted Time=%d since last flush, is under the max allowed time=%d.", currentTime-m_lastForceFlushTime, m_minForceFlushTime);
					    m_ForceFlush = false;
					    break;
                    }
                    else
                    {
                        gnetWarning("CProfileStats::Flush: Force flushed allowed since not all stats were flushed last time");
                    }
				}

				gnetDebug1("CProfileStats::Flush: Trying to flush.");

				CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();

				// Check to see if we have any dirty stats that need to be flushed
				// This call iterates over all stats in CStatsDataMgr::StatsMap and sees if their dirty, and if so
				// updates a cached set of dirty stats in a rlProfileStatsDirtySet. This is slow, but is skipped
				// if s_DirtyStatsCount == 0
				// We also only do this if s_DirtyProfileStatsCount > 0, this way we don't flush stats if no stats
				// were dirtied with STATUPDATEFLAG_DIRTY_PROFILE
				if (statsDataMgr.GetDirtyStatCount() > 0 && statsDataMgr.GetDirtyProfileStatCount() > 0 BANK_ONLY(|| m_ClearSaveTransferStats))
				{
					const s64 posixTime = (s64)rlGetPosixTime();

					//Set posixtime of the last Flush done.
					if ( m_FlushOnlineStats )
					{
						//Cache Previous value.
						m_FlushPreviousPosixTime = StatsInterface::GetInt64Stat(s_StatLastFlush);

						BANK_ONLY(if (!m_ClearSaveTransferStats))
						{
							//Set New value.
							StatsInterface::SetStatData(s_StatLastFlush, posixTime, STATUPDATEFLAG_DIRTY_PROFILE);
						}
					}

					DirtyCache dirtyCache;

#if __BANK
					if(m_ClearSaveTransferStats)
					{
						rlProfileStatsValue dirtyStatValue;
						rlProfileDirtyStat dirtyStat;
						dirtyStatValue.SetInt32(0);
						dirtyStat.Reset(atStringHash( "SERVER_TRANSFER_USED" ), dirtyStatValue, RL_PROFILESTATS_MAX_PRIORITY);
						dirtyCache.AddDirtyStat(dirtyStat, dirtyCache.GetMaxPriorityBin());
					}
#endif // __BANK

					const u32 importantStatCount = statsDataMgr.CheckForDirtyProfileStats(dirtyCache, true, m_FlushOnlineStats);

					// If our dirty set actually contains some dirty stats, flush them
					if (dirtyCache.GetCount() > 0)
					{
						const unsigned flushFlags = m_ForceFlush || m_CouldntFlushAllStats ? RL_PROFILESTATS_FLUSH_FORCE : RL_PROFILESTATS_FLUSH_NONE;

						m_FlushInProgress = true;
                        m_CouldntFlushAllStats = false;

						DirtyCache::FlushIterator flushIt(dirtyCache);
						if (!rlProfileStats::Flush(flushIt, localGamerIndex, &m_FlushStatus, flushFlags))
						{
							gnetWarning("CProfileStats::Flush failed to flush stats");

							//Set that flush has failed for the multiplayer
							if (m_FlushOnlineStats)
							{
								gnetWarning("Profile Stats Flush - FAILED - Unable to start rlProfileStats::Flush()");
								m_FlushFailedMP = true;

								BANK_ONLY(if (!m_ClearSaveTransferStats))
								{
									//Set back the last cached value because we failed to start the flush.
									StatsInterface::SetStatData(s_StatLastFlush, m_FlushPreviousPosixTime, STATUPDATEFLAG_DIRTY_PROFILE);
								}
							}

							// Clear our in progress flag since we didn't start a flush
							m_FlushInProgress = false;

							BANK_ONLY( m_ClearSaveTransferStats = false; )

							// If for some reason we failed to flush (most likely out of rline heap space), and
							// we had at least some important stats to flush, then break so we don't clear our 
							// m_ForceFlush flag and will try again later
							if (importantStatCount > 0)
							{
								break;
							}
						}
						else
						{
							// Successfully started flush.

#if __BANK
							m_ClearSaveTransferStats = false;

							if ( m_FlushOnlineStats )
								m_FlushPreviousPosixTime = m_FlushPosixtimeMP = StatsInterface::GetInt64Stat(s_StatLastFlush);
#else
							//Set posixTime of the last Flush done.
							if ( m_FlushOnlineStats )
								m_FlushPosixtimeMP = posixTime;
#endif // __BANK


							// Emit a warning if there are still dirty stats left after flushing for tracking purposes. These
							// are in danger of being lost, or causing us to do CheckForDirtyProfileStats unnecessarily frequently
                            // We only want to do this if there were stats that would have been flushed if we had more room. This can
                            // only happen if either our dirty cache is completely full or it wasn't completely emptied. This can generate a 
                            // false positive in the case that we had exactly MAX_DIRTY stats that needed to be flushed and were flushed, 
                            // in which case we'll just make one unnecessary call to CheckForDirtyProfileStats on the next update
                            if (dirtyCache.GetCount() >= MAX_DIRTY || flushIt.GetFlushedCount() < dirtyCache.GetCount())
							{
								gnetWarning("CProfileStats::Flush: %d remaining dirty stats after flush", statsDataMgr.GetDirtyStatCount());

								// Go ahead and allow us to flush again if we couldn't flush all stats
								m_CouldntFlushAllStats = true;
							}
						}
					}
					else
					{
						m_CouldntFlushAllStats = false;
						gnetWarning("CProfileStats::Flush - NO FLUSH NEEDED - DirtyStatCount='%d', DirtyProfileStatCount='%d', DirtyCacheCount='%d'.", statsDataMgr.GetDirtyStatCount(), statsDataMgr.GetDirtyProfileStatCount(), dirtyCache.GetCount());
					}
				}
				else
				{
					gnetWarning("CProfileStats::Flush - NO FLUSH NEEDED - DirtyStatCount='%d', DirtyProfileStatCount='%d'", statsDataMgr.GetDirtyStatCount(), statsDataMgr.GetDirtyProfileStatCount());
				}

#if !__NO_OUTPUT
				static const u32 MIN_FLUSH_INTERVAL = 5*60*1000;
				static u32 s_LastFlushTime = 0;

				if (0<s_LastFlushTime && currentTime-s_LastFlushTime <= MIN_FLUSH_INTERVAL)
					gnetWarning("CProfileStats::Flush: Time=%d since last flush, is under the allowed interval=%d", currentTime-s_LastFlushTime, MIN_FLUSH_INTERVAL);
				else
					gnetDebug1("CProfileStats::Flush: Time=%d since last flushed", currentTime-s_LastFlushTime);

				s_LastFlushTime = currentTime;
#endif // !__NO_OUTPUT

				if (m_ForceFlush)
					m_lastForceFlushTime = currentTime;

				// Reset force flush since we flushed (or didn't flush because there were no dirty stats to flush)
                m_ForceFlush = false;
			}
			break;

		case RL_PROFILESTATS_FLUSH_INVALID_CREDENTIALS:
		case RL_PROFILESTATS_FLUSH_DISABLED:
		default:
			break;
		}
	}
};

void
CProfileStats::OpenNetwork()
{
	m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;
}

void
CProfileStats::CloseNetwork()
{
	m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;
}

void  
CProfileStats::ProcessSynchOpStatusNone()
{
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;
	if (NetworkInterface::IsLocalPlayerOnline() && !NetworkInterface::IsNetworkOpen() && !m_SynchOp.m_SuccessCounterForSP && (m_SynchOp.m_ForceOnNewGameTimer+FORCE_SYNCH_NEWGAME_INTERVAL) < curTime)
	{
		NPROFILE( Synchronize(true, false); )
			m_SynchOp.m_ForceOnNewGameTimer = curTime+3*FORCE_SYNCH_NEWGAME_INTERVAL;
	}

	if (m_SynchOp.m_Force)
	{
		bool triggerSynch = true;
		bool multiplayer = false;

		if (m_SynchOp.m_FailedSP)
		{
			multiplayer = false;

			if (NetworkInterface::IsNetworkOpen())
			{
				triggerSynch = false;
				m_SynchOp.m_FailedSP = false;

				if (!m_SynchOp.m_FailedMP)
				{
					m_SynchOp.m_Force = false;
				}
			}
		}
		else if (m_SynchOp.m_FailedMP)
		{
			multiplayer = true;

			if (!NetworkInterface::IsNetworkOpen())
			{
				triggerSynch = false;
				m_SynchOp.m_FailedMP = false;

				if (m_SynchOp.m_FailedSP)
				{
					m_SynchOp.m_Force = false;
				}
			}
		}

		if (triggerSynch)
		{
			NPROFILE( Synchronize( false, multiplayer ); )
		}
	}
}

void  
CProfileStats::ProcessSynchOpStatusSucceeded()
{
	const u32 operation = m_SynchOp.m_OperationId;

	bool success = true;

#if !__FINAL
	if(m_SynchOp.m_SimulateFailPct)
	{
		gnetDebug1("Profile Stats: Synchronize request - Simulating failure %d%% of the time...", m_SynchOp.m_SimulateFailPct);
		if(s_SynchronizeRng.GetRanged(1, 100) <= m_SynchOp.m_SimulateFailPct)
		{
			gnetDebug1("Profile Stats: Synchronize request - Failure simulated!");
			success = false;
		}
		else
		{
			gnetDebug1("Profile Stats: Synchronize request - Failure averted!");
		}
	}
#endif // !__FINAL

	m_SynchOp.OperationDone( success );
	if (operation > PS_SYNCH_IDLE)
	{
		//Flush stats sync the operation was successful.
		if(success)
		{
			if (operation == PS_SYNC_SP)
			{
				NPROFILE( Flush(true, false); )
			}
			else if (operation == PS_SYNC_MP)
			{
				gnetDebug1("Set m_MPSynchIsDone True.");
				m_MPSynchIsDone = true;

				#if GEN9_STANDALONE_ENABLED
				m_MPMigrationStatusChecked = false;
				#endif

				// We only flush in mp if the network is open otherwise we should not be synching anyways.
				if (NetworkInterface::IsNetworkOpen())
				{
					CStatsSavesMgr& savemgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
					if (savemgr.CanFlushProfileStats( ))
					{
						NPROFILE( Flush(true, true); )
					}
				}
			}
		}

		// Clear flag if this was an MP sync
		m_ForceCloudOverwriteOnMPLocalStats &= ( operation!=PS_SYNC_MP );
	}
}

void  
CProfileStats::ProcessStatusSucceeded()
{
	m_FlushStatus.Reset();

	if (m_FlushInProgress)
	{
		m_FlushInProgress = false;

		//Set that flush has succeeded for the multiplayer
		if (m_FlushOnlineStats)
		{
			gnetWarning("Profile stats Flush - SUCCEEDED - Clear flag m_FlushFailedMP");
			m_FlushFailedMP = false;

			//Set Local value for last MP flush.
			CProfileSettings& settings = CProfileSettings::GetInstance();
			gnetAssert( settings.AreSettingsValid() );
			settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32,  (int)(m_FlushPosixtimeMP & 0xFFFFFFFF));
			settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32, (int)(m_FlushPosixtimeMP >> 32));
			settings.Write( true );
		}

#if !__FINAL
		if(m_SimulateFlushFailPct)
		{
			gnetDebug1("Profile Stats: Flush request - Simulating Flush failure %d%% of the time...", m_SimulateFlushFailPct);
			if(s_FlushRng.GetRanged(1, 100) <= m_SimulateFlushFailPct)
			{
				gnetDebug1("Profile Stats: Flush request - Failure simulated!");

				//If a flush or reset fails, then the values stored
				//in profile stats are wrong and we need to do a re-sync
				//in order to figure out what mismatches and needs to
				//be flushed again

				if (m_FlushOnlineStats)
				{
					gnetWarning("Profile stats Flush - Simulating FAILED - Set flag m_FlushFailedMP");
					m_FlushFailedMP = true;
					m_SynchOp.m_FailedMP = true;
					ClearSynchronized( PS_SYNC_MP );
				}
				else
				{
					m_SynchOp.m_FailedSP = true;
					ClearSynchronized( PS_SYNC_SP );
				}

				m_SynchOp.m_Force = true;
				m_SynchOp.m_ForceInterval += FORCE_SYNCH_INTERVAL;
			}
			else
			{
				gnetDebug1("Profile Stats: Flush request - Failure averted!");
			}
		}
#endif // !__FINAL

		//If we don't have remaining stats to flush we need to Flush them.
		if (!m_CouldntFlushAllStats && m_FlushOnlineStats)
		{
			m_FlushOnlineStats = false;
		}
	}

	if (m_ResetGroups.m_Pending)
		m_ResetGroups.m_Pending = false;
}

void  
CProfileStats::ProcessStatusFailed()
{
	m_FlushStatus.Reset();

	if (m_FlushInProgress || m_ResetGroups.m_Pending)
	{
		//If a flush or reset fails, then the values stored
		//in profile stats are wrong and we need to do a re-sync
		//in order to figure out what mismatches and needs to
		//be flushed again

		if (m_FlushOnlineStats)
		{
			//Set back the cached value because we failed to flush.
			StatsInterface::SetStatData(s_StatLastFlush, m_FlushPreviousPosixTime, STATUPDATEFLAG_DIRTY_PROFILE);

			if (NetworkInterface::IsNetworkOpen())
			{
				gnetWarning("Profile stats Flush - FAILED - Set flag m_FlushFailedMP");
				m_FlushFailedMP      = true;
				m_SynchOp.m_FailedMP = true;
				m_SynchOp.m_Force    = true;
				m_SynchOp.m_ForceInterval += FORCE_SYNCH_INTERVAL;
			}
			else
			{
				m_FlushFailedMP = false;
				gnetWarning("Profile stats Flush - FAILED LAST MULTIPLAYER PROFILE STATS FLUSH - QUIT NO MORE RESYNCH");
			}

			ClearSynchronized( PS_SYNC_MP );
		}
		else
		{
			m_SynchOp.m_FailedSP = true;

			ClearSynchronized( PS_SYNC_SP );

			m_SynchOp.m_Force = true;
			m_SynchOp.m_ForceInterval += FORCE_SYNCH_INTERVAL;
		}
	}

	if (m_FlushInProgress)
		m_FlushInProgress = false;

	if (m_ResetGroups.m_Pending)
	{
		gnetAssertf(0, "Reset Profile Stats BY_GROUP FAILED");
		m_ResetGroups.m_Pending = false;
	}
}

#if GEN9_STANDALONE_ENABLED
void
CProfileStats::ProcessSaveMigrationStatus()
{
	if (m_MPSynchIsDone)
	{
		if (!m_MPMigrationStatusChecked)
		{
#if !__FINAL
			int genOverride = ePofileGeneration::PS_GENERATION_NOTSET;
			if (PARAM_profileGenerationOverride.Get(genOverride))
			{
				gnetDisplay("ProcessSaveMigrationStatus : Override MP_PLATFORM_GENERATION to generation %d.", genOverride);
				StatsInterface::SetStatData(STAT_MP_PLATFORM_GENERATION, genOverride, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
#endif

#if RSG_PC || RSG_ORBIS || RSG_DURANGO
			m_MPMigrationStatusChecked = true;
#else
			if (StatsInterface::GetIntStat(STAT_MP_PLATFORM_GENERATION) == ePofileGeneration::PS_GENERATION_NOTSET)
			{
				u32 generation = ePofileGeneration::PS_GENERATION_NOTSET;
				CSaveMigration_multiplayer& mp = CSaveMigration::GetMultiplayer();
				if (mp.GetMigrationStatus() == eSaveMigrationStatus::SM_STATUS_OK)
				{
					gnetDisplay("ProcessSaveMigrationStatus : User migrated on this load so no need to check it's status.");
					generation = ePofileGeneration::PS_GENERATION_GEN8;
					m_MPMigrationStatusChecked = true;
				}
				else 
				{
					if (mp.GetStatus() == eSaveMigrationStatus::SM_STATUS_NONE)
					{
						gnetDisplay("ProcessSaveMigrationStatus : Request migration status.");
						mp.RequestStatus();
					}
					else if (mp.GetStatus() != eSaveMigrationStatus::SM_STATUS_PENDING)
					{
						m_MPMigrationStatusChecked = true;

						bool bMigrated = false;
						if (mp.GetStatus(&bMigrated) == eSaveMigrationStatus::SM_STATUS_OK)
						{
							gnetDisplay("ProcessSaveMigrationStatus : Migration status OK : Has migrated %s.", bMigrated ? "TRUE" : "FALSE");
							generation = (bMigrated) ? ePofileGeneration::PS_GENERATION_GEN8 : ePofileGeneration::PS_GENERATION_GEN9;
						}
					}
				}

				if (generation > ePofileGeneration::PS_GENERATION_NOTSET)
				{
					gnetDisplay("ProcessSaveMigrationStatus : Update MP_PLATFORM_GENERATION to generation %d.", generation);
					StatsInterface::SetStatData(STAT_MP_PLATFORM_GENERATION, generation, STATUPDATEFLAG_ASSERTONLINESTATS);
				}
			}
			else
			{
				m_MPMigrationStatusChecked = true;
			}
#endif
		}
	}
}
#endif

void  
CProfileStats::Update()
{
	if (PARAM_noprofilestats.Get())
		return;

	if (!rlProfileStats::IsInitialized())
		return;

	//Get the Tunable.
	if (0 == m_minForceFlushTime && Tunables::IsInstantiated() && Tunables::GetInstance().HasCloudRequestFinished())
	{
		m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime() + FORCE_SYNCH_NEWGAME_INTERVAL;

		static const u32 MIN_FORCE_FLUSH_INTERVAL = 60000;
		m_minForceFlushTime = MIN_FORCE_FLUSH_INTERVAL;

		int tempVal = 0; 
		if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MIN_FORCE_FLUSH_INTERVAL", 0xdc53b91a), tempVal))
		{
			m_minForceFlushTime = (u32)tempVal;
		}
	}

	switch (m_SynchOp.m_Status.GetStatus())
	{
	case netStatus::NET_STATUS_NONE:
		{
			NPROFILE( ProcessSynchOpStatusNone(); )
		}
		break;

	case netStatus::NET_STATUS_SUCCEEDED:
		{
			NPROFILE( ProcessSynchOpStatusSucceeded(); )
		}
		break;

	case netStatus::NET_STATUS_CANCELED:
	case netStatus::NET_STATUS_FAILED:
			// Clear flag if this was an MP sync
			m_ForceCloudOverwriteOnMPLocalStats &= ( m_SynchOp.m_OperationId!=PS_SYNC_MP );
			m_SynchOp.OperationDone( false );
		break;

	case netStatus::NET_STATUS_PENDING:
		break;
	default:
		gnetAssertf(0, "Invalid status on profile status writes");
	}

	switch (m_FlushStatus.GetStatus())
	{
	case netStatus::NET_STATUS_NONE:
		{
            //Don't do anything while sync is still pending
            if (!m_SynchOp.m_Status.None())
            {
                break;
            }

			//Prioritize finishing resetting any groups before doing a flush, that way resets/flushes
            //are properly sequenced. Otherwise we may overwrite a flushed value with a reset that
            //was supposed to occur before the flush
            if (m_ResetGroups.IsAnySet() && gnetVerify(!m_ResetGroups.m_Pending))
			{
                int nextGroup = m_ResetGroups.m_ResetGroup.GetNthBitIndex(0);

                if (gnetVerify(nextGroup != -1))
                {
                    u32 grp = nextGroup;

                    m_ResetGroups.UnSet(grp);
                    m_ResetGroups.m_Pending = ResetGroup(grp);
                }
			}
            else
	        {
				//If no groups left to reset, consider flushing.
				NPROFILE( CheckAndTryFlushing(); )
	        }
		}
		break;

	case netStatus::NET_STATUS_SUCCEEDED:
		{
			NPROFILE( ProcessStatusSucceeded(); );
		}
		break;

	case netStatus::NET_STATUS_CANCELED:
	case netStatus::NET_STATUS_FAILED:
		{
			NPROFILE( ProcessStatusFailed(); );
		}
		break;

	case netStatus::NET_STATUS_PENDING:
		break;

	default:
		gnetAssertf(0, "Invalid status on profile status writes");
	}

#if GEN9_STANDALONE_ENABLED
	NPROFILE( ProcessSaveMigrationStatus(); )
#endif

	NPROFILE( rlProfileStats::Update(); )
}

void  
CProfileStats::Flush(const bool forceFlush, const bool flushMpStats)
{
	if (PARAM_noprofilestats.Get())
		return;

	gnetAssert( rlProfileStats::IsInitialized() );

	if (!NetworkInterface::IsLocalPlayerOnline())
		return;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return;

	if (!rlProfileStats::IsInitialized())
		return;

	if (m_FlushInProgress)
		return;

	//Avoid flushing MP stats if we failed to Synch or are not synched.
	int errorcode = 0;
	if (flushMpStats && (SynchronizedFailed(CProfileStats::PS_SYNC_MP, errorcode) || !Synchronized(CProfileStats::PS_SYNC_MP)))
	{
		gnetWarning("Avoid flushing MP stats if we failed to Synch or are not synched");
		return;
	}
	else if (flushMpStats && m_SynchOp.m_SuccessCounterForMP > 0 && !m_MPSynchIsDone)
	{
		gnetDebug1("Set m_MPSynchIsDone True.");
		m_MPSynchIsDone = true;
	}

	//Avoid flushing SP stats if we failed to Synch or are not synched.
	if (!flushMpStats && (SynchronizedFailed(CProfileStats::PS_SYNC_SP, errorcode) || !Synchronized(CProfileStats::PS_SYNC_SP)))
	{
		gnetWarning("Avoid flushing SP stats if we failed to Synch or are not synched");
		return;
	}

	// Simply set m_ForceFlush here, this will get processed later in our Update which will call
	// rlProfileStats::Flush with RL_PROFILESTATS_FLUSH_FORCE once m_Status is idle
	// Note that we don't allow resetting m_ForceFlush to false here, as that would allow a quick sequence
	// of calls like Flush(TRUE)...Flush(FALSE) to effectively cancel the initial Flush. m_ForceFlush gets cleared
	// in Update().
	// Also note that Flush(FALSE) doesn't need to do anything, as Update() will pick up the dirty
	// stats and flush them automatically once the flush status becomes RL_PROFILESTATS_FLUSH_READY
	if (forceFlush)
	{
		m_ForceFlush = forceFlush;
	}

	m_FlushOnlineStats = flushMpStats;
}

bool
CProfileStats::ResetGroup(const u32 group)
{
	if ( !NetworkInterface::IsLocalPlayerOnline() )
		return true;

	if ( !rlProfileStats::IsInitialized() )
		return true;

	//Do not allow Group 7 to be reset. Its where the cash stats are saved.
	if (group == PS_GRP_MP_7)
	{
		gnetDebug1("Reset Profile Stats BY_GROUP - FAILED for group PS_GRP_MP_7, group is blocked to resets");
		return true;
	}

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return true;

	rlProfileStatsFlushStatus flushstatus = rlProfileStats::GetFlushStatus(localGamerIndex);
	if (RL_PROFILESTATS_FLUSH_INVALID_CREDENTIALS == flushstatus || RL_PROFILESTATS_FLUSH_DISABLED == flushstatus)
		return true;

	// Do not reset (aka send rlProfileStatsResetByGroupTask to the server) if flush blocked!
	if(StatsInterface::GetBlockSaveRequests())
		return true;

	bool result = false;

	if (gnetVerifyf(!m_ResetGroups.IsSet(group), "Group < %d > is already set to be cleared.", group))
	{
		m_ResetGroups.Set(group);

		//Don't start a reset until any pending synchronize is finished
		if (netStatus::NET_STATUS_NONE == m_FlushStatus.GetStatus() &&
            m_SynchOp.m_Status.None())
		{
			result = m_ResetGroups.m_Pending = rlProfileStats::ResetStatsByGroup(NetworkInterface::GetLocalGamerIndex(), group, &m_FlushStatus);

			if (result)
			{
				gnetDebug1("Reset Profile Stats BY_GROUP started for group %d", group);

				m_ResetGroups.UnSet(group);
			}
		}
	}

	return result;
}


bool 
CProfileStats::Synchronize(const bool force, const bool multiplayer)
{
	if (PARAM_noprofilestats.Get())
		return false;

	//Allow force in SP.
	if (!multiplayer && m_SynchOp.m_SuccessCounterForSP>0 && !force)
		return false;

	if (multiplayer && m_SynchOp.m_SuccessCounterForMP>0 /*&& !force*/)
		return false;

	gnetAssertf(m_SynchOp.m_OperationId == PS_SYNCH_IDLE, "Synch Operation %s already in Progress", m_SynchOp.m_OperationId == PS_SYNC_SP ? "PS_SYNC_SP" : "PS_SYNC_MP");
	if (m_SynchOp.m_OperationId > PS_SYNC_MP)
		return false;

	gnetAssert( rlProfileStats::IsInitialized() );

	m_SynchOp.m_Force = true;

	if (netStatus::NET_STATUS_NONE != m_SynchOp.m_Status.GetStatus())
		return false;

	const unsigned curTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	if (!force && (curTime - m_SynchOp.m_ForceTimer) <= m_SynchOp.m_ForceInterval)
		return false;

	if (!NetworkInterface::IsLocalPlayerOnline())
		return false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		return false;

	if (netStatus::NET_STATUS_NONE != m_SynchOp.m_Status.GetStatus())
		return false;

	rlProfileStatsFlushStatus flushstatus = rlProfileStats::GetFlushStatus(localGamerIndex);
	if (RL_PROFILESTATS_FLUSH_INVALID_CREDENTIALS == flushstatus || RL_PROFILESTATS_FLUSH_DISABLED == flushstatus)
		return false;

#if !__FINAL
	if(m_SimulateCredentialsFailPct)
	{
		gnetDebug1("Profile Stats: Synchronize request - Simulating Credentials failure %d%% of the time...", m_SimulateCredentialsFailPct);
		if(s_CredentialsRng.GetRanged(1, 100) <= m_SimulateCredentialsFailPct)
		{
			gnetDebug1("Profile Stats: Synchronize request - Credentials Failure simulated!");
			return false;
		}
		else
		{
			gnetDebug1("Profile Stats: Synchronize request - Credentials Failure averted!");
		}
	}
#endif // !__FINAL

	m_SynchOp.m_ForceTimer = curTime;

    bool result = false;

    // Sync only SP/MP stats as needed
    if (multiplayer)
    {
        atFixedArray<unsigned, RL_PROFILESTATS_MAX_SYNC_GROUPS> groupsToSync;

        gnetDebug1("Syncing MP stats");

        // For MP, we sync all MP groups, as some stats in each group are always needed
        for (unsigned groupId = PS_GRP_MP_START; groupId <= PS_GRP_MP_END; groupId++)
        {
			//HACK - 5th Character does not exist just now.
			if (groupId == PS_GRP_MP_5)
				continue;

            gnetDebug1("Synching MP group %d", groupId);
            // We do a force sync, which ignores the version stat, although we could just update the interface
            // to not take it
            groupsToSync.Push(groupId);
        }
		groupsToSync.Push(PS_GRP_SP_0);

		//HACK - 5th Character does not exist just now.
		StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_5, true, true);

#if !__FINAL
		if (PARAM_nompsavegame.Get() || PARAM_lan.Get())
		{
			result = true;

			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_0, true, true);
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_1, true, true);
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_2, true, true);
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_3, true, true);
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_4, true, true);
			StatsInterface::SetAllStatsToSynchedByGroup(PS_GRP_MP_5, true, true);

			SetSynchronized(PS_SYNC_MP, true);
		}
		else
#endif // !__FINAL
		{
			// We do a force sync for MP, because we have server authoritative stats that aren't read from another source
			result = rlProfileStats::SynchronizeGroups(localGamerIndex, groupsToSync, &m_SynchOp.m_Status);

			APPEND_PF_READBYGROUP( SynchronizeGroups )
		}
    }
    else
    {
        atFixedArray<rlProfileStatsGroupDefinition, RL_PROFILESTATS_MAX_SYNC_GROUPS> groupsToSync;

        // SP just has a single save, but multiple groups, so sync all of them
        gnetDebug1("Syncing SP stats");

        for (unsigned groupId = PS_GRP_SP_START; groupId <= PS_GRP_SP_END; groupId++)
        {
            gnetDebug1("Syncing SP group %d", groupId);
            // These groups all use the same version stat
            // Configure the conditional sync with whatever version our savegame has
            // We'll skip syncing if our savegame was saved with the same profile stats version,
            // and profile stats was saved with the same savegame version.
            rlProfileStatsValue localProfileVersion;
            rlProfileStatsValue localSavegameVersion;

            localProfileVersion.SetInt64(CStatsMgr::GetStatsDataMgr().GetProfileStatsTimestampLoadedFromSavegame());
            localSavegameVersion.SetInt64(CStatsMgr::GetStatsDataMgr().GetSavegameTimestampLoadedFromSavegame());
            groupsToSync.Push(rlProfileStatsGroupDefinition(groupId, 
                                                            STAT_SP_PROFILE_STAT_VERSION,
                                                            STAT_SP_SAVE_TIMESTAMP,
                                                            localProfileVersion,
                                                            localSavegameVersion));
        }

        // Do a conditional sync for SP, since we've potentially loaded the same stats already from
        // our savegame that are stored in profile stats
        result = rlProfileStats::ConditionalSynchronizeGroups(localGamerIndex, groupsToSync, &m_SynchOp.m_Status);

		APPEND_PF_READBYGAMER( ConditionalSynchronizeGroups, 1 )
    }

	if (result)
	{
		if (multiplayer)
		{
			m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;
			m_SynchOp.m_OperationId = PS_SYNC_MP;
			m_SynchOp.m_FailedMP = false;
		}
		else
		{
			m_SynchOp.m_OperationId = PS_SYNC_SP;
			m_SynchOp.m_FailedSP = false;
		}

		m_SynchOp.m_ForceInterval = 0;
		m_SynchOp.m_Force = false;
		OUTPUT_ONLY( m_SynchOp.m_Timer = sysTimer::GetSystemMsTime(); )
	}
	else
	{
		m_SynchOp.m_ForceInterval += FORCE_SYNCH_INTERVAL;
	}

	gnetAssert(result);

	return result;
}

bool  
CProfileStats::CanSynchronize(const bool force)
{
	static bool s_CanSynchronize = false;
	static u32 s_PreviousFrame   = 0;

	const u32 currFrame = fwTimer::GetFrameCount();
	if (s_PreviousFrame != currFrame)
	{
		s_CanSynchronize = false;

		if (PARAM_noprofilestats.Get())
		{
			statWarningf("CanSynchronize() - FALSE :: PARAM_noprofilestats.Get()");
			return false;
		}
		else if (!rlProfileStats::IsInitialized())
		{
			statWarningf("CanSynchronize() - FALSE :: !rlProfileStats::IsInitialized()");
			return false;
		}
		else if (!NetworkInterface::IsLocalPlayerOnline())
		{
			statWarningf("CanSynchronize() - FALSE :: !NetworkInterface::IsLocalPlayerOnline()");
			return false;
		}

		//Any pending operations are canceled.
		if (force && m_SynchOp.m_Status.Pending())
		{
			statDebugf1("CanSynchronize() - Cancelling SynchOp");
			m_SynchOp.Cancel( );
		}

		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rlProfileStatsFlushStatus flushstatus = rlProfileStats::GetFlushStatus(localGamerIndex);

		if (!force && (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_SynchOp.m_ForceTimer) <= m_SynchOp.m_ForceInterval)
		{
			statWarningf("CanSynchronize() - FALSE :: (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_SynchOp.m_ForceTimer) <= m_SynchOp.m_ForceInterval");
			return false;
		}
		else if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		{
			statWarningf("CanSynchronize() - FALSE :: !RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)");
			return false;
		}
		else if (RL_PROFILESTATS_FLUSH_INVALID_CREDENTIALS == flushstatus || RL_PROFILESTATS_FLUSH_DISABLED == flushstatus)
		{
			statWarningf("CanSynchronize() - FALSE :: RL_PROFILESTATS_FLUSH_INVALID_CREDENTIALS == flushstatus || RL_PROFILESTATS_FLUSH_DISABLED == flushstatus");
			return false;
		}
		else if (m_SynchOp.m_OperationId>PS_SYNCH_IDLE)
		{
			statWarningf("CanSynchronize() - FALSE :: m_SynchOp.m_OperationId>PS_SYNCH_IDLE");
			return false;
		}
		else if (netStatus::NET_STATUS_NONE != m_SynchOp.m_Status.GetStatus())
		{
			statWarningf("CanSynchronize() - FALSE :: netStatus::NET_STATUS_NONE != m_SynchOp.m_Status.GetStatus()");
			return false;
		}

		s_CanSynchronize = true;
		s_PreviousFrame  = currFrame;
	}

	return s_CanSynchronize;
}

bool  
CProfileStats::Synchronized(const eSyncType type) const
{
	if (type == PS_SYNC_MP)
		return (m_SynchOp.m_SuccessCounterForMP>0);

	gnetAssert(type == PS_SYNC_SP);

	return (m_SynchOp.m_SuccessCounterForSP>0);
}

void 
CProfileStats::SetSynchronized(const eSyncType type NOTFINAL_ONLY(, const bool force)) 
{
	gnetAssert( !SynchronizePending(type) );

	if (type == PS_SYNC_MP)
	{
		//Don't ever clear MP unless it has FAILED.
		if (m_SynchOp.m_FailedMP NOTFINAL_ONLY(|| force))
		{
			m_MPSynchIsDone = true;
			m_SynchOp.m_SuccessCounterForMP = 1;
			gnetWarning("Set PS_SYNC_MP Synchronized counter=%d.", m_SynchOp.m_SuccessCounterForMP);
		}
		else
		{
			gnetWarning("AVOIDED Set PS_SYNC_MP Synchronized counter=%d.", m_SynchOp.m_SuccessCounterForMP);
		}
	}
	else if (gnetVerify(type == PS_SYNC_SP))
	{
		m_SynchOp.m_SuccessCounterForSP = 1;
		gnetWarning("Set PS_SYNC_SP Synchronized counter=%d.", m_SynchOp.m_SuccessCounterForSP);
	}
}

void 
CProfileStats::ClearSynchronized(const eSyncType type)
{
	//gnetAssert( !SynchronizePending(type) );

	if (type == PS_SYNC_MP)
	{
		//Don't ever clear MP unless it has FAILED, or forced by scripts.
		if (m_SynchOp.m_FailedMP || ForcedToOverwriteAllLocalMPStats())
		{
			gnetWarning("Clear PS_SYNC_MP Synchronized counter=%d reset to 0   (forced? %d)", m_SynchOp.m_SuccessCounterForMP, ForcedToOverwriteAllLocalMPStats());

			if (SynchronizePending(type))
			{
				gnetWarning("Synchronize for PS_SYNC_MP Pending - Cancel operation is progress.");
				m_SynchOp.Cancel();
			}

			if (ForcedToOverwriteAllLocalMPStats() && m_FlushFailedMP)
			{
				gnetWarning("Profile Stats Flush - Clearing Flag m_FlushFailedMP - ForcedToOverwriteAllLocalMPStats()");
				m_FlushFailedMP = false;
			}

			m_SynchOp.m_SuccessCounterForMP = 0;
			MoneyInterface::ClearFlagForResync();

			//Make sure we clear the flush flags for MP.
			if (m_ForceFlush && m_FlushOnlineStats)
			{
				gnetWarning( "Clear PS_SYNC_MP Synchronized - clear pending flush for multiplayer" );
				m_FlushOnlineStats = false;
				m_ForceFlush = false;
			}
		}
		else
		{
			gnetWarning("AVOIDED Clear PS_SYNC_MP Synchronized counter=%d reset to 0", m_SynchOp.m_SuccessCounterForMP);
		}
	}
	else if (gnetVerify(type == PS_SYNC_SP))
	{
		gnetWarning("Clear PS_SYNC_SP Synchronized counter=%d reset to 0", m_SynchOp.m_SuccessCounterForSP);

		if (SynchronizePending(type))
		{
			gnetWarning("Synchronize for PS_SYNC_SP Pending - Cancel operation is progress.");
			m_SynchOp.Cancel();
		}

		m_SynchOp.m_SuccessCounterForSP = 0;
		m_SynchOp.m_ForceOnNewGameTimer = sysTimer::GetSystemMsTime()+FORCE_SYNCH_NEWGAME_INTERVAL;

		//Make sure we clear the flush flags for SP.
		if (m_ForceFlush && !m_FlushOnlineStats)
		{
			gnetWarning( "Clear PS_SYNC_SP Synchronized - clear pending flush for singleplayer" );
			m_FlushOnlineStats = false;
			m_ForceFlush = false;
		}
	}
}

bool  
CProfileStats::SynchronizePending( const eSyncType type) const
{
	if (PS_SYNCH_IDLE == m_SynchOp.m_OperationId)
		return false;

	if (type == PS_SYNC_MP)
		return (PS_SYNC_MP == m_SynchOp.m_OperationId);

	gnetAssert(type == PS_SYNC_SP);
	return (PS_SYNC_SP == m_SynchOp.m_OperationId);
}

bool  
CProfileStats::SynchronizedFailed(const eSyncType type, int& errorCode) const
{
	if (type == PS_SYNC_MP)
	{
		errorCode = m_SynchOp.m_httpErrorCode;
		return m_SynchOp.m_FailedMP;
	}

	gnetAssert(type == PS_SYNC_SP);
	return m_SynchOp.m_FailedSP;
}

void 
CProfileStats::HandleEvent(const class rlProfileStatsEvent* evt)
{
	if (!rlProfileStats::IsInitialized())
		return;

	if (!gnetVerify(evt))
		return;

	CStatsMgr::GetStatsDataMgr().HandleEventProfileStats(evt);
}

void CProfileStats::ForceMPProfileStatsGetFromCloud() 
{
	m_ForceCloudOverwriteOnMPLocalStats = true;
	ClearSynchronized( CProfileStats::PS_SYNC_MP );
	ClearMPSynchIsDone( );
}

// CProfileStats::DirtyCache
CProfileStats::DirtyCache::FlushIterator::FlushIterator(const DirtyCache &dirtyCache)
: m_DirtyCache(dirtyCache)
, m_CurBinIndex(BIN_COUNT - 1)
, m_CurStat(NULL)
{
	// Initialize m_CurStat to the first stat in our highest priority bin
	for (; m_CurBinIndex >= 0; m_CurBinIndex--)
	{
		if (m_DirtyCache.m_DirtyStatBins[m_CurBinIndex].m_FirstInBin != NULL)
		{
			m_CurStat = m_DirtyCache.m_DirtyStatBins[m_CurBinIndex].m_FirstInBin;
			break;
		}
	}
}

bool 
CProfileStats::DirtyCache::FlushIterator::GetCurrentStat(rlProfileDirtyStat &stat) const
{
	if (m_CurStat != NULL)
	{
		stat = m_CurStat->m_Stat;

		return true;
	}
	else
	{
		return false;
	}
}

void 
CProfileStats::DirtyCache::FlushIterator::Next()
{
	if (!AtEnd())
	{
		// To get the next stat, either use the next stat in the current bin,
		// or get the first stat in the next non-empty bin
		if (m_CurStat->m_NextInBin != NULL)
		{
			m_CurStat = m_CurStat->m_NextInBin;
		}
		else
		{
			// Null out our current stat in case we can't find one that
			// way we're considered at the end
			m_CurStat = NULL;
			// Start looking in the next bin
			m_CurBinIndex--;
			// Now iterate over our bins in priority until we find one with a stat in it
			for (; m_CurBinIndex >= 0; m_CurBinIndex--)
			{
				if (m_DirtyCache.m_DirtyStatBins[m_CurBinIndex].m_FirstInBin != NULL)
				{
					m_CurStat = m_DirtyCache.m_DirtyStatBins[m_CurBinIndex].m_FirstInBin;
					break;
				}
			}
		}
	}
}

bool 
CProfileStats::DirtyCache::FlushIterator::AtEnd() const
{
	return m_CurStat == NULL;
}

void 
CProfileStats::DirtyCache::DirtyStatBin::Link(DirtyStat *stat)
{
	// gnetVerify that this stat was previously unlinked from its old bin
	gnetAssert(stat->m_NextInBin == NULL);

	// Add this stat to the beginning of the bin, and be sure to point it to the next stat in the bin
	stat->m_NextInBin = m_FirstInBin;
	m_FirstInBin = stat;
}

void 
CProfileStats::DirtyCache::DirtyStatBin::Unlink(DirtyStat *stat)
{
	gnetAssert(stat == m_FirstInBin);

	// Update the first stat in this begin to be the one after the current one that we're removing
	m_FirstInBin = stat->m_NextInBin;

	// And zero out this stat's next link
	stat->m_NextInBin = NULL;
}

CProfileStats::DirtyCache::DirtyStatBin*
CProfileStats::DirtyCache::GetBin(const rlProfileDirtyStat &stat)
{
    const rlProfileStatsPriority priority = stat.GetPriority();

    if (Likely(priority <= RL_PROFILESTATS_MAX_PRIORITY))
    {
        // We add one to priority here because bin 0 is the lowest priority
        // for any version stats
        return &m_DirtyStatBins[priority - RL_PROFILESTATS_MIN_PRIORITY + 1];
    }

    return NULL;
}

bool 
CProfileStats::DirtyCache::AddDirtyStat(const rlProfileDirtyStat& stat)
{
    DirtyStatBin *bin = GetBin(stat);

    return AddDirtyStat(stat, bin);
}

bool
CProfileStats::DirtyCache::AddDirtyStat(const rlProfileDirtyStat& stat, DirtyStatBin *bin)
{
    // We should always have a bin to place this stat in
    if (gnetVerify(bin != NULL))
    {
        if (m_Count < MAX_DIRTY)
        {
            // If we haven't exceeded our max dirty stats yet, simply add this to the end of the list
            m_DirtyStats[m_Count] = DirtyStat(stat);

			// Now link the newly allocated stat with its proper bin
			bin->Link(&m_DirtyStats[m_Count]);

			m_Count++;

			return true;
		}
		else
		{
			// There isn't room for this stat, so find an existing one of a lower priority to replace
			// Start with the lowest priority bin, and stop once we reach the same bin as what this stat is going into
			for (u32 replBinIndex = 0; replBinIndex < BIN_COUNT && bin != &m_DirtyStatBins[replBinIndex]; replBinIndex++)
			{
				// See if there's a stat in this lower priority bin to replace
				DirtyStat *statToRepl = m_DirtyStatBins[replBinIndex].m_FirstInBin;

				if (statToRepl != NULL)
				{
					// Remove the stat from the bin
					m_DirtyStatBins[replBinIndex].Unlink(statToRepl);

					// Update it with our new stat
					statToRepl->m_Stat = stat;

					// And place it in the proper bin
					bin->Link(statToRepl);

					return true;
				}
			}
		}
	}

	return false;
}

CProfileStats::DirtyCache::DirtyStatBin*
CProfileStats::DirtyCache::GetMinPriorityBin()
{
    return &m_DirtyStatBins[0];
}

CProfileStats::DirtyCache::DirtyStatBin*
CProfileStats::DirtyCache::GetMaxPriorityBin()
{
    return &m_DirtyStatBins[BIN_COUNT - 1];
}

// EOF

