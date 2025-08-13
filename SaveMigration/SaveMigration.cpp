#include "SaveMigration.h"

// Network Headers
#include "Network/Cloud/Tunables.h"

// Game headers
#include "Core/Game.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#endif // __BANK

SAVEGAME_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// --- Static Class Members -----------------------------------------------------

CSaveMigration_multiplayer   CSaveMigration::sm_Multiplayer;
CSaveMigration_singleplayer  CSaveMigration::sm_Singleplayer;

// --- End Static Class Members -----------------------------------------------------

void CSaveMigration::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		sm_Multiplayer.Init();
		sm_Singleplayer.Init();

		BANK_ONLY(Bank_InitWidgets();)
	}
}

void CSaveMigration::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		BANK_ONLY(Bank_ShutdownWidgets();)

		sm_Multiplayer.Shutdown();
		sm_Singleplayer.Shutdown();
	}
}

void CSaveMigration::Update()
{
	sm_Multiplayer.Update();
	sm_Singleplayer.Update();

	BANK_ONLY(Bank_Update();)
}

#if __BANK
void CSaveMigration::Bank_InitWidgets()
{
	bkBank* bank = BANKMGR.FindBank("SaveMigration");
	if (!bank)
	{
		bank = &BANKMGR.CreateBank("SaveMigration");

		sm_Multiplayer.Bank_InitWidgets(bank);
		sm_Singleplayer.Bank_InitWidgets(bank);
	}
}

void CSaveMigration::Bank_ShutdownWidgets()
{
	bkBank* bank = BANKMGR.FindBank("SaveMigration");
	if (bank)
	{
		sm_Multiplayer.Bank_ShutdownWidgets(bank);
		sm_Singleplayer.Bank_ShutdownWidgets(bank);

		BANKMGR.DestroyBank(*bank);
	}
}

void CSaveMigration::Bank_Update()
{
	sm_Multiplayer.Bank_Update();
	sm_Singleplayer.Bank_Update();
}
#endif 