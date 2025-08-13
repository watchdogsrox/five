
//
// filename:	commands_savemigration.cpp
// description:	Commands for save migration
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/script.h"
#include "script/script_helper.h"
#include "script/thread.h"
#include "script/wrapper.h"
#include "system/param.h"

// Game headers
#include "SaveMigration/SaveMigration.h"

// Stats headers
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"

// Network Headers
#include "Network/Network.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/NetworkInterface.h"

PARAM(debugSaveMigrationCommands, "Debug save migration commands.");

// --- Defines ------------------------------------------------------------------

//Save migration Account.
class scrSaveMigrationAccount
{
public:
	scrValue		m_AccountId;
	scrValue		m_StatusCount;
	scrValue		m_Available;

	scrTextLabel	m_Platform;
	scrTextLabel63	m_GamerName;
	scrTextLabel63	m_GamerHandle;
	scrTextLabel63	m_ErrorCode;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSaveMigrationAccount);

//Save migration Account Stat.
class scrSaveMigrationAccountStat
{
public:
	scrTextLabel63	m_Name;
	scrTextLabel63	m_Value;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSaveMigrationAccountStat);

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

namespace savemigration_commands
{
	bool CommandMPIsEnabled()
	{
		return CSaveMigration::GetMultiplayer().IsEnabled();
	}

	bool CommandMPRequestAccounts()
	{
#if __BANK
		if (PARAM_debugSaveMigrationCommands.Get())
		{
			savemigrationDebug1("%s:SAVEMIGRATION_MP_REQUEST_ACCOUNTS", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scrThread::PrePrintStackTrace();
		}
#endif
		return CSaveMigration::GetMultiplayer().RequestAccounts();
	}

	eSaveMigrationStatus CommandMPGetAccountsStatus()
	{
		return CSaveMigration::GetMultiplayer().GetAccountsStatus();
	}

	int CommandMPGetNumberOfAccounts()
	{
		if (CSaveMigration::GetMultiplayer().GetAccountsStatus() == SM_STATUS_OK)
		{
			return CSaveMigration::GetMultiplayer().GetAccounts().GetCount();
		}
		return 0;
	}

	bool CommandMPGetAccount(const int index, scrSaveMigrationAccount* out_pAccountData)
	{
		if (!out_pAccountData)
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_GET_ACCOUNT - out_pAccountData is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		if (CSaveMigration::GetMultiplayer().GetAccountsStatus() == SM_STATUS_OK)
		{
			const rlSaveMigrationMPAccountsArray& acounts = CSaveMigration::GetMultiplayer().GetAccounts();
			if (index >= 0 && index < acounts.GetCount())
			{
				const rlSaveMigrationMPAccount account = acounts[index];

				out_pAccountData->m_AccountId.Int = account.m_AccountId;
				out_pAccountData->m_Available.Int = account.m_Available;
				out_pAccountData->m_StatusCount.Int = account.m_Stats.GetCount();

				formatf(out_pAccountData->m_Platform, "%s", account.m_Platform);
				formatf(out_pAccountData->m_GamerName, "%s", account.m_Gamertag);
				formatf(out_pAccountData->m_GamerHandle, "%s", account.m_GamerHandle);
				formatf(out_pAccountData->m_ErrorCode, "%s", account.m_ErrorCode);

				return true;
			}
			else
			{
				savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_GET_ACCOUNT - index is out of range.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		return false;
	}

	bool CommandMPGetAccountStat(const int index, const int stat, scrSaveMigrationAccountStat* out_pAccountStat)
	{
		if (!out_pAccountStat)
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_GET_ACCOUNT_STAT - out_pAccountStat is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		if (CSaveMigration::GetMultiplayer().GetAccountsStatus() == SM_STATUS_OK)
		{
			const rlSaveMigrationMPAccountsArray& acounts = CSaveMigration::GetMultiplayer().GetAccounts();
			if (index >= 0 && index < acounts.GetCount())
			{
				const rlSaveMigrationMPAccount account = acounts[index];
				if (stat >= 0 && stat < account.m_Stats.GetCount())
				{
					formatf(out_pAccountStat->m_Name, "%s", account.m_Stats[stat].m_Name);
					formatf(out_pAccountStat->m_Value, "%s", account.m_Stats[stat].m_Value);
					return true;
				}
				else
				{
					savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_GET_ACCOUNT_STAT - stat is out of range.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
			else
			{
				savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_GET_ACCOUNT_STAT - index is out of range.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		return false;
	}

	bool CommandMPMigrateAccount(const int sourceAccountid, const char* sourcePlatform)
	{
#if __BANK
		if (PARAM_debugSaveMigrationCommands.Get())
		{
			savemigrationDebug1("%s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scrThread::PrePrintStackTrace();
		}
#endif

		savemigrationAssertf(sourcePlatform, "[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - sourceplatform is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!sourcePlatform)
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - sourceplatform is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		savemigrationAssertf(!NetworkInterface::IsNetworkOpen(), "[code_SAVETRANS_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - can not be done in multiplayer.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsNetworkOpen())
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - can not be done in multiplayer.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		savemigrationAssertf(StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "[code_SAVETRANS_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - Stats are loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - Stats are loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
		savemigrationAssertf(!profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP), "[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - Profile Stats are loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP))
		{
			savemigrationError("[code_savemigration] %s:SAVEMIGRATION_MP_MIGRATATE_ACCOUNT - Profile Stats are loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		return CSaveMigration::GetMultiplayer().MigrateAccount(sourceAccountid, sourcePlatform);
	}

	eSaveMigrationStatus CommandMPAccountMigrationStatus()
	{
		return CSaveMigration::GetMultiplayer().GetMigrationStatus();
	}

	bool CommandMPRequestStatus()
	{
#if __BANK
		if (PARAM_debugSaveMigrationCommands.Get())
		{
			savemigrationDebug1("%s:SAVEMIGRATION_MP_REQUEST_STATUS", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scrThread::PrePrintStackTrace();
		}
#endif
		return CSaveMigration::GetMultiplayer().RequestStatus();
	}

	eSaveMigrationStatus CommandMPGetStatus()
	{
		return CSaveMigration::GetMultiplayer().GetStatus();
	}

	bool CommandMPIsPlatformGeneration(int GEN9_STANDALONE_ONLY(nGeneration))
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s : SAVEMIGRATION_MP_IS_GENERATION - Trying to check the generation without being online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "%s : SAVEMIGRATION_MP_IS_GENERATION - Trying to check the generation without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s : SAVEMIGRATION_MP_IS_GENERATION - Trying to check the generation but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		
#if GEN9_STANDALONE_ENABLED
		scriptAssertf(nGeneration >= CProfileStats::ePofileGeneration::PS_GENERATION_NOTSET, "%s : SAVEMIGRATION_MP_IS_GENERATION - Trying check the generation with a negative generation value.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(nGeneration < CProfileStats::ePofileGeneration::PS_GENERATION_MAX, "%s : SAVEMIGRATION_MP_IS_GENERATION - Trying check the generation with a value that is out of range.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		return StatsInterface::GetIntStat(STAT_MP_PLATFORM_GENERATION) == nGeneration;
#else
		return false;
#endif
	}

	void SetupScriptCommands()
	{
		// --------- Multiplayer
		SCR_REGISTER_SECURE(SAVEMIGRATION_IS_MP_ENABLED, 0x522cbfb9bc78c5a8, CommandMPIsEnabled);

		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_REQUEST_ACCOUNTS, 0x85f41f9225d08c72, CommandMPRequestAccounts);
		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_GET_ACCOUNTS_STATUS, 0xc8cb5999919ea2ca, CommandMPGetAccountsStatus);

		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_NUM_ACCOUNTS, 0x77a16200e18e0c55, CommandMPGetNumberOfAccounts);
		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_GET_ACCOUNT, 0xfce2747eef1d05fc, CommandMPGetAccount);
		SCR_REGISTER_UNUSED(SAVEMIGRATION_MP_GET_ACCOUNT_STAT, 0x83f619aa7b5992db, CommandMPGetAccountStat);

		SCR_REGISTER_UNUSED(SAVEMIGRATION_MP_MIGRATE_ACCOUNT, 0x472ec2413d9cfc26, CommandMPMigrateAccount);
		SCR_REGISTER_UNUSED(SAVEMIGRATION_MP_GET_ACCOUNT_MIGRATION_STATUS, 0x5988c7025ab82599, CommandMPAccountMigrationStatus);

		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_REQUEST_STATUS, 0xa06b0c3f6c7cb4bc, CommandMPRequestStatus);
		SCR_REGISTER_SECURE(SAVEMIGRATION_MP_GET_STATUS, 0x3e4ea30497613d3d, CommandMPGetStatus);

#if GEN9_STANDALONE_ENABLED
		SCR_REGISTER_UNUSED(SAVEMIGRATION_MP_IS_PLATFORM_GENERATION, 0x18a958b38123ed15, CommandMPIsPlatformGeneration);
#endif
	}
}