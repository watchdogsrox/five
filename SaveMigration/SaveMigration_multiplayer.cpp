#include "SaveMigration_multiplayer.h"

// Rage headers
#include "rline/savemigration/rlsavemigration.h"

// network headers
#include "Network/NetworkInterface.h"
#include "scene/DynamicEntity.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/title.h"
#include "data/callback.h"
#include "system/bit.h"
#endif // __BANK

SAVEGAME_OPTIMISATIONS()

#if __BANK
enum eSaveMigrationBankRequestsFlags
{
	SMBR_NONE				= BIT0,
	SMBR_GET_ACCOUNTS		= BIT1,
	SMBR_MIGRATE_ACCOUNT	= BIT2,
	SMBR_GET_STATUS			= BIT3,
};

const char * g_SavemigrationMPAccountsOverrideNames[6] = {
	"Zero Accounts",
	"One Account",
	"Two Account",
	"Account Unavaliable",
	"Account Error",
	"Error",
};
const char * g_SavemigrationMPStatusOverrideNames[6] = {
	"NONE",
	"INPROGRESS",
	"FINISHED",
	"CANCELED",
	"ROLLEDBACK",
	"Error",
};

bool		g_SavemigrationMPAccountOverrideEnabled = false;
int			g_SavemigrationMPAccountOverride = 0;
bool		g_SavemigrationMPAccountMigrationOverrideEnabled = false;
int			g_SavemigrationMPAccountMigrationOverride = 0;
bool		g_SavemigrationMPAccountStatusOverrideEnabled = false;
int			g_SavemigrationMPAccountStatusOverride = 0;

PARAM(SavemigrationMPEnabledOverride,   "Enable | Disable the multiplayer save migration");

PARAM(SavemigrationMPAccountsOverride, "Save migration multiplayer accounts overrider (0:Zero Accounts, 1:One Account 2:Two Accounts 3:Account Unavaliable 4:Account Error 5:Error)");
PARAM(SavemigrationMPMigrationOverride, "Save migration multiplayer migration override (0:NONE, 1:INPROGRESS, 2:FINISHED 3:CANCELED 4:ROLLEDBACK 5:ERROR)");
PARAM(SavemigrationMPStatusOverride,    "Save migration multiplayer status override (0:NONE, 1:INPROGRESS, 2:FINISHED 3:CANCELED 4:ROLLEDBACK 5:ERROR)");
#endif

//////////////////////////////////////////////////////////////////////////

CSaveMigrationGetAccounts::CSaveMigrationGetAccounts()
	: m_accounts()
{}

bool CSaveMigrationGetAccounts::OnConfigure()
{
	m_accounts.Reset();

#if __BANK
	if (g_SavemigrationMPAccountOverrideEnabled)
	{
		m_netStatus.Finished();
		switch (g_SavemigrationMPAccountOverride)
		{
		default:
		case 0:
			
			break;
		case 2:
		{
			rlSaveMigrationMPAccount account;
			account.m_AccountId = 996352064;
			account.m_Available = true;
			
			safecpy(account.m_Gamertag, "Test Gamertag 1", rage::RLSM_MAX_GAMERTAG_CHARS);
			safecpy(account.m_GamerHandle, "Test GamerHandle 1", rage::RLSM_MAX_GAMERHANDLE_CHARS);
			safecpy(account.m_Platform, "xboxone", rage::RLSM_MAX_PLATFORM_CHARS);

			m_accounts.Push(account);

			// Fall through to add the 2nd account
		}
		case 1:
		{
			rlSaveMigrationMPAccount account;
			account.m_AccountId = 996352063;
			account.m_Available = true;

			safecpy(account.m_Gamertag, "Test Gamertag", rage::RLSM_MAX_GAMERTAG_CHARS);
			safecpy(account.m_GamerHandle, "Test GamerHandle", rage::RLSM_MAX_GAMERHANDLE_CHARS);
			safecpy(account.m_Platform, "ps4", rage::RLSM_MAX_PLATFORM_CHARS);

			m_accounts.Push(account);
			break;
		}
		case 3:
		{
			rlSaveMigrationMPAccount account;
			account.m_AccountId = 996352063;
			account.m_Available = false;

			safecpy(account.m_Gamertag, "Test Gamertag Unavaliable", rage::RLSM_MAX_GAMERTAG_CHARS);
			safecpy(account.m_GamerHandle, "Test GamerHandle", rage::RLSM_MAX_GAMERHANDLE_CHARS);
			safecpy(account.m_Platform, "ps4", rage::RLSM_MAX_PLATFORM_CHARS);

			safecpy(account.m_ErrorCode, "ERROR_ALREADY_DONE", rage::RLSM_MAX_ERROR_CODE);

			m_accounts.Push(account);
			break;
		}
		case 4:
		{
			rlSaveMigrationMPAccount account;
			account.m_AccountId = 996352063;
			account.m_Available = false;

			safecpy(account.m_Gamertag, "Test Gamertag Error", rage::RLSM_MAX_GAMERTAG_CHARS);
			safecpy(account.m_GamerHandle, "Test GamerHandle", rage::RLSM_MAX_GAMERHANDLE_CHARS);
			safecpy(account.m_Platform, "ps4", rage::RLSM_MAX_PLATFORM_CHARS);

			safecpy(account.m_ErrorCode, "ERROR_NOT_AVAILABLE", rage::RLSM_MAX_ERROR_CODE);

			m_accounts.Push(account);
			break;
		}
			break;
		case 5:
			m_netStatus.Failed();
			break;
		}
		savemigrationDebug1("CSaveMigrationGetAccounts::OnConfigure : OVERRIDE %d", g_SavemigrationMPAccountOverride);
		return true;
	}
#endif

	savemigrationDebug1("CSaveMigrationGetAccounts::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
	return rlSaveMigration::GetMPAccounts(NetworkInterface::GetLocalGamerIndex(), &m_accounts, &m_netStatus);
}

void CSaveMigrationGetAccounts::OnCompleted()
{
	m_status = SM_STATUS_OK;

	savemigrationDebug1("CSaveMigrationGetAccounts::OnCompleted : localGamerIndex %d : Accounts Count %d", NetworkInterface::GetLocalGamerIndex(), m_accounts.GetCount());
}

void CSaveMigrationGetAccounts::OnFailed()
{
	savemigrationDebug1("CSaveMigrationGetAccounts::OnFailed : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

void CSaveMigrationGetAccounts::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationGetAccounts::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigrationMigrateAccount::CSaveMigrationMigrateAccount()
	: m_sourceAccount(RLSM_INVALID_ACCOUNT_ID)
	, m_sourcePlatform()
	, m_transactionAttempts(0)
	, m_transactionPollTime(SAVEMIGRATION_DEFAULT_POOL_TIME_MS)
	, m_transactionMaxAttempts(SAVEMIGRATION_DEFAULT_NUM_ATTEMPTS)
	, m_transactionTimeStamp(0)
	, m_transactionId(RLSM_INVALID_TRANSACTION_ID)
	, m_transactionState()
{}

bool CSaveMigrationMigrateAccount::Setup(const int sourceAccountid, const char* sourcePlatform, u32 pollTimeIncrementMs, u32 maxPollingAttempts)
{
	if (!IsActive())
	{
		m_sourceAccount = sourceAccountid;
		m_sourcePlatform = sourcePlatform;
		m_transactionPollTime = pollTimeIncrementMs;
		m_transactionMaxAttempts = maxPollingAttempts;
		m_transactionAttempts = 0;
		m_transactionTimeStamp = 0;
		return true;
	}
	return false;
}

void CSaveMigrationMigrateAccount::OnWait()
{
#if __BANK
	if (g_SavemigrationMPAccountMigrationOverrideEnabled)
	{
		CSaveMigrationTask::OnWait(); // Run through the base code to handle task completion
		return;
	}
#endif

	if (m_transactionId == RLSM_INVALID_TRANSACTION_ID)
	{
		if (m_netStatus.GetStatus() == rage::netStatus::NET_STATUS_SUCCEEDED)
		{
			if (m_transactionState.InProgress())
			{
				savemigrationDebug1("CSaveMigrationMigrateAccount::OnWait Migration Started : localGamerIndex %d : transactionId %d", NetworkInterface::GetLocalGamerIndex(), m_transactionState.GetTransactionId());
				savemigrationAssertf(m_transactionState.GetTransactionId() != RLSM_INVALID_TRANSACTION_ID, "CSaveMigrationMigrateAccount::OnWait : Transaction ID is invalid");

				m_netStatus.Reset();
				m_transactionAttempts = 1;
				m_transactionTimeStamp = fwTimer::GetTimeInMilliseconds();
				m_transactionId = m_transactionState.GetTransactionId();
			}
			else
			{
				CSaveMigrationTask::OnWait(); // Run through the base code to handle task completion
			}
		}
		else
		{
			CSaveMigrationTask::OnWait(); // Run through the base code to handle task completion
		}
	}
	else
	{
		if (m_netStatus.GetStatus() == rage::netStatus::NET_STATUS_NONE)
		{
			if (m_transactionAttempts > m_transactionMaxAttempts)
			{
				savemigrationWarning("CSaveMigrationMigrateAccount::OnWait Migration State Requested reached max polling attempts %d : %dms", m_transactionAttempts, m_transactionAttempts * m_transactionPollTime);

				m_taskState = SM_TASK_DONE;
				m_status = SM_STATUS_ERROR;
				m_netStatus.Reset();

				OnFailed();
			}
			else if ((fwTimer::GetTimeInMilliseconds() - m_transactionTimeStamp) > Min((m_transactionAttempts * m_transactionPollTime), (u32)SAVEMIGRATION_DEFAULT_MAX_POOL_TIME_MS)) // Poll once to see if the migration has competed
			{
				m_transactionState.Reset();
				m_transactionAttempts++;

				if (!rlSaveMigration::GetTransactionState(NetworkInterface::GetLocalGamerIndex(), m_transactionId, &m_transactionState, &m_netStatus))
				{
					m_taskState = SM_TASK_DONE;
					m_status = SM_STATUS_ERROR;
					m_netStatus.Reset();

					OnFailed();
				}
				else
				{
					savemigrationDebug1("CSaveMigrationMigrateAccount::OnWait Migration State Requested : localGamerIndex %d : transactionId %d", NetworkInterface::GetLocalGamerIndex(), m_transactionId);
				}
			}
		}
		else if (m_netStatus.GetStatus() == rage::netStatus::NET_STATUS_SUCCEEDED)
		{
			if (m_transactionState.InProgress())
			{
				m_netStatus.Reset();
				m_transactionTimeStamp = fwTimer::GetTimeInMilliseconds();
			}
			else
			{
				CSaveMigrationTask::OnWait(); // Run through the base code to handle task completion
			}
		}
		else
		{
			CSaveMigrationTask::OnWait(); // Run through the base code to handle task completion
		}
	}
}

bool CSaveMigrationMigrateAccount::OnConfigure()
{
	m_transactionState.Reset();

#if __BANK
	if (g_SavemigrationMPAccountMigrationOverrideEnabled)
	{
		m_netStatus.Finished();
		switch (g_SavemigrationMPAccountMigrationOverride)
		{
		default:
		case rage::RLSM_STATE_INVALID:
			m_transactionState.Reset();
			break;
		case rage::RLSM_STATE_INPROGRESS:
			m_transactionState.Set(996352063, true, NULL);
			break;
		case rage::RLSM_STATE_FINISHED:
			m_transactionState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Finished");
			break;
		case rage::RLSM_STATE_CANCELED:
			m_transactionState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Canceled");
			break;
		case rage::RLSM_STATE_ROLLEDBACK:
			m_transactionState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "RolledBack");
			break;
		case rage::RLSM_STATE_ERROR:
			m_transactionState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Error");
			break;
		}
		savemigrationDebug1("CSaveMigrationMigrateAccount::OnConfigure : OVERRIDE %d : state %d : %s", g_SavemigrationMPAccountMigrationOverride, m_transactionState.GetStateId(), m_transactionState.GetState());
		return true;
	}
#endif

	savemigrationDebug1("CSaveMigrationMigrateAccount::OnConfigure : localGamerIndex %d : sourceAccount %d : sourcePlatform %s", NetworkInterface::GetLocalGamerIndex(), m_sourceAccount, m_sourcePlatform.c_str());
	return rlSaveMigration::MigrateMPAccount(NetworkInterface::GetLocalGamerIndex(), m_sourceAccount, m_sourcePlatform.c_str(), &m_transactionState, &m_netStatus);
}

void CSaveMigrationMigrateAccount::OnCompleted()
{
	switch (m_transactionState.GetStateId())
	{
	case rage::RLSM_STATE_INVALID:
		m_status = SM_STATUS_NONE;
		break;
	case rage::RLSM_STATE_INPROGRESS:
		m_status = SM_STATUS_INPROGRESS;
		break;
	case rage::RLSM_STATE_FINISHED:
		m_status = SM_STATUS_OK;
		break;
	case rage::RLSM_STATE_CANCELED:
		m_status = SM_STATUS_CANCELED;
		break;
	case rage::RLSM_STATE_ROLLEDBACK:
		m_status = SM_STATUS_ROLLEDBACK;
		break;
	case rage::RLSM_STATE_ERROR:
		m_status = SM_STATUS_ERROR;
		break;
	}

	savemigrationDebug1("CSaveMigrationMigrateAccount::OnCompleted : localGamerIndex %d : status %s", NetworkInterface::GetLocalGamerIndex(), GetSaveMigrationStatusChar(m_status));
}

void CSaveMigrationMigrateAccount::OnFailed()
{
	savemigrationDebug1("CSaveMigrationMigrateAccount::OnFailed : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

void CSaveMigrationMigrateAccount::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationMigrateAccount::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigrationGetMPStatus::CSaveMigrationGetMPStatus()
	:m_playState()
	,m_migrated(false)
{}

bool CSaveMigrationGetMPStatus::OnConfigure()
{
#if __BANK
	if (g_SavemigrationMPAccountStatusOverrideEnabled)
	{
		m_netStatus.Finished();
		switch (g_SavemigrationMPAccountStatusOverride)
		{
		default:
		case rage::RLSM_STATE_INVALID:
			m_playState.Reset();
			break;
		case rage::RLSM_STATE_INPROGRESS:
			m_playState.Set(996352063, true, NULL);
			break;
		case rage::RLSM_STATE_FINISHED:
			m_playState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Finished");
			break;
		case rage::RLSM_STATE_CANCELED:
			m_playState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Canceled");
			break;
		case rage::RLSM_STATE_ROLLEDBACK:
			m_playState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "RolledBack");
			break;
		case rage::RLSM_STATE_ERROR:
			m_playState.Set(rage::RLSM_INVALID_TRANSACTION_ID, false, "Error");
			break;
		}
		savemigrationDebug1("CSaveMigrationGetMPStatus::OnConfigure : OVERRIDE %d : state %d : %s", g_SavemigrationMPAccountStatusOverride, m_playState.GetStateId(), m_playState.GetState());
		return true;
	}
#endif
	savemigrationDebug1("CSaveMigrationGetMPStatus::OnConfigure : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
	return rlSaveMigration::GetState(NetworkInterface::GetLocalGamerIndex(), &m_playState, &m_netStatus);
}

void CSaveMigrationGetMPStatus::OnCompleted()
{
	int stateId = m_playState.GetStateId();
	switch (stateId)
	{
	case rage::RLSM_STATE_INVALID:
		m_status = SM_STATUS_OK; // No transtaction information returned
		break;
	case rage::RLSM_STATE_INPROGRESS:
		m_status = SM_STATUS_INPROGRESS;
		break;
	case rage::RLSM_STATE_FINISHED:
		m_status = SM_STATUS_OK;
		m_migrated = true;
		break;
	case rage::RLSM_STATE_CANCELED:
		m_status = SM_STATUS_CANCELED;
		break;
	case rage::RLSM_STATE_ROLLEDBACK:
		m_status = SM_STATUS_ROLLEDBACK;
		break;
	case rage::RLSM_STATE_ERROR:
		m_status = SM_STATUS_ERROR;
		break;
	}

	savemigrationDebug1("CSaveMigrationGetMPStatus::OnCompleted : localGamerIndex %d : status %d : %s : %s", NetworkInterface::GetLocalGamerIndex(), m_playState.GetStateId(), m_playState.GetState(), GetSaveMigrationStatusChar(m_status));
}

void CSaveMigrationGetMPStatus::OnFailed()
{
	savemigrationDebug1("CSaveMigrationGetMPStatus::OnFailed : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

void CSaveMigrationGetMPStatus::OnCanceled()
{
	savemigrationDebug1("CSaveMigrationGetMPStatus::OnCanceled : localGamerIndex %d", NetworkInterface::GetLocalGamerIndex());
}

//////////////////////////////////////////////////////////////////////////

CSaveMigration_multiplayer::CSaveMigration_multiplayer()
	: m_accountsTask()
	, m_migrateTask()
	, m_playStateTask()
	, m_queue()
	, m_enabled(true)
{
#if __BANK
	m_BkRequest = SMBR_NONE;
	m_BkGroup = nullptr;
	m_bBankCreated = false;

	m_BkAccountsGroup = nullptr;
	m_bBankAccountsCreated = false;
#endif
}

CSaveMigration_multiplayer::~CSaveMigration_multiplayer()
{
}

void CSaveMigration_multiplayer::Init()
{
	m_enabled = !Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SAVEMIGRATION_DISABLED", 0xFB18BC93), false) || 
				!Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SAVEMIGRATION_DISABLE_MULTIPLAYER", 0x4BF38BBB), false);

#if __BANK
	int iEnabled;
	if (PARAM_SavemigrationMPEnabledOverride.Get(iEnabled))
	{
		m_enabled = iEnabled > 0;
	}
	g_SavemigrationMPAccountOverrideEnabled = PARAM_SavemigrationMPAccountsOverride.Get(g_SavemigrationMPAccountOverride);
	g_SavemigrationMPAccountMigrationOverrideEnabled = PARAM_SavemigrationMPMigrationOverride.Get(g_SavemigrationMPAccountMigrationOverride);
	g_SavemigrationMPAccountStatusOverrideEnabled = PARAM_SavemigrationMPStatusOverride.Get(g_SavemigrationMPAccountStatusOverride);
#endif
}

void CSaveMigration_multiplayer::Shutdown()
{
	Cancel();

	m_queue.Reset();

	m_accountsTask.Reset();
	m_migrateTask.Reset();
	m_playStateTask.Reset();
}

void CSaveMigration_multiplayer::Update()
{
	if (!m_queue.IsEmpty())
	{
		CSaveMigrationTask* task = m_queue.Top();
		if (task->IsActive())
		{
			task->Update();
		}
		else
		{
			m_queue.Pop();
		}
	}
}

bool CSaveMigration_multiplayer::RequestAccounts()
{
	if (m_enabled && m_accountsTask.Start())
	{
		m_queue.Push(&m_accountsTask);
		return true;
	}
	return false;
}

eSaveMigrationStatus CSaveMigration_multiplayer::GetAccountsStatus() const
{
	return m_accountsTask.GetStatus();
}

const rlSaveMigrationMPAccountsArray& CSaveMigration_multiplayer::GetAccounts() const
{
	return m_accountsTask.GetAccounts();
}

bool CSaveMigration_multiplayer::MigrateAccount(const int sourceAccountid, const char* sourcePlatform, u32 pollTimeIncrementMs, u32 maxPollingAttempts)
{
	if (m_enabled && m_migrateTask.Setup(sourceAccountid, sourcePlatform, pollTimeIncrementMs, maxPollingAttempts) && m_migrateTask.Start())
	{
		m_queue.Push(&m_migrateTask);
		return true;
	}
	return false;
}

eSaveMigrationStatus CSaveMigration_multiplayer::GetMigrationStatus() const
{
	return m_migrateTask.GetStatus();
}

bool CSaveMigration_multiplayer::RequestStatus()
{
	if (m_enabled && m_playStateTask.Start())
	{
		m_queue.Push(&m_playStateTask);
		return true;
	}
	return false;
}

eSaveMigrationStatus CSaveMigration_multiplayer::GetStatus(bool* out_migrated) const
{
	if (out_migrated && m_playStateTask.GetStatus()== eSaveMigrationStatus::SM_STATUS_OK)
	{
		*out_migrated = m_playStateTask.HasMigrated();
	}
	return m_playStateTask.GetStatus();
}

void CSaveMigration_multiplayer::Cancel()
{
	m_accountsTask.Cancel();
	m_migrateTask.Cancel();
	m_playStateTask.Cancel();
}

void CSaveMigration_multiplayer::SetChosenMpCharacterSlot(const int chosenMpCharacterSlot)
{
	m_chosenMpCharacterSlot = chosenMpCharacterSlot;
}

#if __BANK
void CSaveMigration_multiplayer::Bank_InitWidgets(bkBank* bank)
{
	if (bank)
	{
		m_BkGroup = bank->PushGroup("Multiplayer", false);
		{
			bank->AddButton("Create MP widgets", datCallback(MFA(CSaveMigration_multiplayer::Bank_CreateWidgets), (datBase*)this));
		}
		bank->PopGroup();
	}
}

void CSaveMigration_multiplayer::Bank_ShutdownWidgets(bkBank* bank)
{
	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
	}
	m_BkGroup = nullptr;
	m_bBankCreated = false;
}

void CSaveMigration_multiplayer::Bank_CreateWidgets()
{
	bkBank* bank = BANKMGR.FindBank("SaveMigration");
	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);

		m_bBankAccountsCreated = false;
		m_BkAccountsGroup = nullptr;
	}
	m_BkGroup = nullptr;
	m_bBankCreated = false;

	if ((!m_bBankCreated) && bank && !m_BkGroup)
	{
		m_bBankCreated = true;

		m_BkGroup = bank->PushGroup("Multiplayer", true);
		{
			bank->PushGroup("Overrides", true, "Lets you force the return result from the request.");
			{
				bank->AddToggle("Enable", &g_SavemigrationMPAccountOverrideEnabled);
				bank->AddCombo("Account Request Result", &g_SavemigrationMPAccountOverride, 6, &g_SavemigrationMPAccountsOverrideNames[0], 0, NullCB, "");
#if IS_GEN9_PLATFORM
				bank->AddSeparator();
				bank->AddToggle("Enable", &g_SavemigrationMPAccountMigrationOverrideEnabled);
				bank->AddCombo("Migrate Account Result", &g_SavemigrationMPAccountMigrationOverride, 6, &g_SavemigrationMPStatusOverrideNames[0], 0, NullCB, "");
				bank->AddSeparator();
				bank->AddToggle("Enable", &g_SavemigrationMPAccountStatusOverrideEnabled);
				bank->AddCombo("Account Status Result", &g_SavemigrationMPAccountStatusOverride, 6, &g_SavemigrationMPStatusOverrideNames[0], 0, NullCB, "");
				bank->AddSeparator();
#endif
			}
			bank->PopGroup();

			bank->AddSeparator();
			bank->PushGroup("Tests", true, "Use to test the MP save migration API");
			{
				bank->AddButton("Request Accounts", datCallback(MFA(CSaveMigration_multiplayer::Bank_RequestAccounts), (datBase*)this));
				{
					strcpy(m_accountstatus, "None");
					m_accountsCount = 0;

					bank->AddText("Status", m_accountstatus, 32, true);
					bank->AddText("Count", &m_accountsCount, true);

					if (!m_bBankAccountsCreated && !m_BkAccountsGroup)
					{
						m_bBankAccountsCreated = true;
						m_BkAccountsGroup = bank->PushGroup("Accounts", false);
						// Will be filled in when they accounts are pulled
						bank->PopGroup();
					}
				}

#if IS_GEN9_PLATFORM
				bank->AddButton("Migrate Account", datCallback(MFA(CSaveMigration_multiplayer::Bank_MigrateAccount), (datBase*)this));
				{
					strcpy(m_migrationstatus, "None");
					m_migrationAccountid = 0;
					strcpy(m_migrationPlatform, "");

					bank->AddText("Status", m_migrationstatus, 32, true);
					bank->AddText("Accountid", &m_migrationAccountid, false);
					bank->AddText("Platform", m_migrationPlatform, 32, false);
				}

				bank->AddButton("Request Status", datCallback(MFA(CSaveMigration_multiplayer::Bank_RequestStatus), (datBase*)this));
				{
					strcpy(m_playstatus, "None");
					m_playstatusTransactionid = 0;

					bank->AddText("Status", m_playstatus, 32, true);
					bank->AddText("Transactionid", &m_playstatusTransactionid, false);
				}
#else
				bank->AddTitle("Migrate Account is not avaliable on this generation of the game.");
#endif
			}
			bank->PopGroup();
		}
		bank->PopGroup();
	}
}

void CSaveMigration_multiplayer::Bank_Update()
{
	if (m_BkRequest.IsFlagSet(SMBR_GET_ACCOUNTS))
	{
		strcpy(m_accountstatus, GetSaveMigrationStatusChar(GetAccountsStatus()));
		m_accountsCount = GetAccounts().GetCount();

		if (GetAccountsStatus() != SM_STATUS_PENDING)
		{
			m_BkRequest.ClearFlag(SMBR_GET_ACCOUNTS);

			for (int i = 0; i < m_accountsCount; i++)
			{
				const rlSaveMigrationMPAccount& account = GetAccounts()[i];

				savemigrationDebug1("Bank_RequestAccounts : AccountId %d : Platform %s : Gamertag %s : ugcPublishCount %d : Available : %s", account.m_AccountId, account.m_Platform, account.m_Gamertag, account.m_ugcPublishCount, account.m_Available ? "True" : "False");

				if (m_bBankAccountsCreated && m_BkAccountsGroup)
				{
					m_BankAccounts.Push(m_BkAccountsGroup->AddTitle("Id %d : %s : %s : ugc %d : %s", account.m_AccountId, account.m_Platform, account.m_Gamertag, account.m_ugcPublishCount, account.m_Available ? "Avaliable" : "Unavaliable"));
				}

				if (account.m_Available)
				{
					m_migrationAccountid = account.m_AccountId;
					strcpy(m_migrationPlatform, account.m_Platform);
				}
			}

			savemigrationDebug1("Bank_RequestAccounts : Completed : %s", m_accountstatus);
		}
		else if (m_BankAccounts.GetCount() > 0)
		{
			for (int i = 0; i < m_BankAccounts.GetCount(); i++)
			{
				m_BankAccounts[i]->Destroy();
			}
			m_BankAccounts.clear();
		}
	}

	if (m_BkRequest.IsFlagSet(SMBR_MIGRATE_ACCOUNT))
	{
		strcpy(m_migrationstatus, GetSaveMigrationStatusChar(GetMigrationStatus()));

		if (GetMigrationStatus() != SM_STATUS_PENDING)
		{
			savemigrationDebug1("Bank_MigrateAccounts : Completed : %s", m_migrationstatus);
			m_BkRequest.ClearFlag(SMBR_MIGRATE_ACCOUNT);
		}
	}

	if (m_BkRequest.IsFlagSet(SMBR_GET_STATUS))
	{
		strcpy(m_playstatus, GetSaveMigrationStatusChar(GetStatus()));

		if (GetStatus() != SM_STATUS_PENDING)
		{
			m_playstatusTransactionid = m_playStateTask.GetState().GetTransactionId();

			savemigrationDebug1("Bank_RefreshStatus : Completed : %s", m_playstatus);
			m_BkRequest.ClearFlag(SMBR_GET_STATUS);
		}
	}
}

void CSaveMigration_multiplayer::Bank_RequestAccounts()
{
	if (!m_BkRequest.IsFlagSet(SMBR_GET_ACCOUNTS))
	{
		if (RequestAccounts())
		{
			savemigrationDebug1("Bank_RequestAccounts : Requested");

			strcpy(m_accountstatus, "Requested");
			m_BkRequest.SetFlag(SMBR_GET_ACCOUNTS);
		}
		else
		{
			strcpy(m_accountstatus, "Failed");
		}
	}
}

void CSaveMigration_multiplayer::Bank_MigrateAccount()
{
	if (!m_BkRequest.IsFlagSet(SMBR_MIGRATE_ACCOUNT))
	{
		if (MigrateAccount(m_migrationAccountid, m_migrationPlatform))
		{
			savemigrationDebug1("Bank_MigrateAccounts : Requested");

			strcpy(m_migrationstatus, "Requested");
			m_BkRequest.SetFlag(SMBR_MIGRATE_ACCOUNT);
		}
		else
		{
			strcpy(m_migrationstatus, "Failed");
		}
	}
}

void CSaveMigration_multiplayer::Bank_RequestStatus()
{
	if (!m_BkRequest.IsFlagSet(SMBR_GET_STATUS))
	{
		if (RequestStatus())
		{
			savemigrationDebug1("Bank_RequestStatus : Requested");

			strcpy(m_playstatus, "Requested");
			m_BkRequest.SetFlag(SMBR_GET_STATUS);
		}
		else
		{
			strcpy(m_playstatus, "Failed");
		}
	}
}
#endif 
