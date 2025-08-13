#ifndef SAVEMIGRATION_MULTIPLAYER_H
#define SAVEMIGRATION_MULTIPLAYER_H

#include "savemigration_common.h"

// Base
#include "atl/string.h"
#include "rline/savemigration/rlsavemigrationcommon.h"

// Framework
#include "fwutil/Flags.h"
#include "atl/queue.h"

#if __BANK
#include "atl/array.h"
#endif

#if __BANK
namespace rage
{
	class bkGroup;
	class bkBank;
	class bkTitle;
}
#endif

//////////////////////////////////////////////////////////////////////////

class CSaveMigrationGetAccounts
	: public CSaveMigrationTask
{
public:
	CSaveMigrationGetAccounts();
	virtual ~CSaveMigrationGetAccounts() {}

	const rlSaveMigrationMPAccountsArray& GetAccounts() const { return m_accounts; }

protected:

	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:

	rlSaveMigrationMPAccountsArray	m_accounts;
};


//////////////////////////////////////////////////////////////////////////

class CSaveMigrationMigrateAccount
	: public CSaveMigrationTask
{
public:
	CSaveMigrationMigrateAccount();
	virtual ~CSaveMigrationMigrateAccount() {}

	bool Setup(const int sourceAccountid, const char* sourcePlatform, u32 pollTimeIncrementMs, u32 maxPollingAttempts);

protected:

	virtual void OnWait() override;
	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:

	int								m_sourceAccount;
	rage::atString					m_sourcePlatform;

	u32								m_transactionMaxAttempts;
	u32								m_transactionAttempts;
	u32								m_transactionPollTime;
	u32								m_transactionTimeStamp;
	int								m_transactionId;
	rlSaveMigrationState			m_transactionState;
};

//////////////////////////////////////////////////////////////////////////

class CSaveMigrationGetMPStatus 
	: public CSaveMigrationTask
{
public:
	CSaveMigrationGetMPStatus();
	virtual ~CSaveMigrationGetMPStatus() {}

	bool HasMigrated() const { return m_migrated; }

#if __BANK
	const rlSaveMigrationState& GetState() const { return m_playState; }
#endif

protected:

	virtual bool OnConfigure() override;
	virtual void OnCompleted() override;

	virtual void OnFailed() override;
	virtual void OnCanceled() override;

private:

	rlSaveMigrationState			m_playState;
	bool							m_migrated;
};

//////////////////////////////////////////////////////////////////////////

class CSaveMigration_multiplayer
{
private:

#if __BANK
	fwFlags32	m_BkRequest;
	bkGroup*	m_BkGroup;
	bool		m_bBankCreated;

	bkGroup*		m_BkAccountsGroup;
	bool			m_bBankAccountsCreated;

	atFixedArray<bkTitle*, 8> m_BankAccounts;

	char			m_accountstatus[32];
	int				m_accountsCount;

	char		m_migrationstatus[32];
	int			m_migrationAccountid;
	char		m_migrationPlatform[rage::RLSM_MAX_PLATFORM_CHARS];

	char		m_playstatus[32];
	int			m_playstatusTransactionid;
#endif

	CSaveMigrationGetAccounts		m_accountsTask;
	CSaveMigrationMigrateAccount	m_migrateTask;
	CSaveMigrationGetMPStatus		m_playStateTask;

	atQueue<CSaveMigrationTask*, 3> m_queue;

	bool							m_enabled;
	rlSaveMigrationMPAccount		m_chosenAccount;
	int								m_chosenMpCharacterSlot;
public:

	CSaveMigration_multiplayer();
	~CSaveMigration_multiplayer();

	inline bool IsEnabled() { return m_enabled; }
	rlSaveMigrationMPAccount GetChosenAccount() { return m_chosenAccount; }
	void SetChosenAccount(rlSaveMigrationMPAccount account) { m_chosenAccount = account; }

	void Init();
	void Shutdown();
	void Update();

	// PURPOSE:
	// Refresh the multiplayer accounts
	bool RequestAccounts();

	// PURPOSE:
	// Get the current multiplayer status of the accounts
	eSaveMigrationStatus GetAccountsStatus() const;

	// PURPOSE:
	// Get the current multiplayer accounts
	const rlSaveMigrationMPAccountsArray& GetAccounts() const;

	// PURPOSE:
	// Migrate the reqested multiplayer account
	bool MigrateAccount(const int sourceAccountid, const char* sourcePlatform, u32 pollTimeIncrementMs = SAVEMIGRATION_DEFAULT_POOL_TIME_MS, u32 maxPollingAttempts = SAVEMIGRATION_DEFAULT_NUM_ATTEMPTS);

	// PURPOSE:
	// Get the current migration status
	eSaveMigrationStatus GetMigrationStatus() const;

	// PURPOSE:
	// Request the multiplayer status
	bool RequestStatus();

	// PURPOSE:
	// Get the current multiplayer status to see if they can start multiplayer
	//
	// Should be called by the game client before entering into multiplayer, on both gen8 and gen9 consoles.
	//
	// SM_STATUS_OK will be returned if they can proceed.
	eSaveMigrationStatus GetStatus(bool* out_migrated = nullptr) const;

	// PURPOSE:
	// Cancel any in flight transactions
	void Cancel();
	const int GetChosenMpCharacterSlot() const { return m_chosenMpCharacterSlot; }
	void SetChosenMpCharacterSlot(const int chosenMpCharacterSlot);
	void ResetChosenMpCharacterSlot() { m_chosenMpCharacterSlot = -1; };

#if __BANK
	void Bank_InitWidgets(bkBank* bank);
	void Bank_ShutdownWidgets(bkBank* bank);
	void Bank_CreateWidgets();
	void Bank_Update();

	void Bank_RequestAccounts();
	void Bank_MigrateAccount();
	void Bank_RequestStatus();
#endif 

};

#endif // SAVEMIGRATION_MULTIPLAYER_H
