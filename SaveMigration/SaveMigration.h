#ifndef SAVEMIGRATION_H
#define SAVEMIGRATION_H

#include "SaveMigration_common.h"
#include "SaveMigration_multiplayer.h"
#include "SaveMigration_singleplayer.h"

class CSaveMigration
{
private:

	// Manages the MP save migration
	static CSaveMigration_multiplayer sm_Multiplayer;
	static CSaveMigration_singleplayer sm_Singleplayer;

public:

	// PURPOSE:  Access the multiplayer manager.
	static CSaveMigration_multiplayer& GetMultiplayer() { return sm_Multiplayer; }

	// PURPOSE:  Access the singleplayer manager.
	static CSaveMigration_singleplayer& GetSingleplayer() { return sm_Singleplayer; }

public:

	static void  Init(unsigned initMode);
	static void  Shutdown(unsigned shutdownMode);
	static void  Update();

private:

#if __BANK
	static void Bank_InitWidgets();
	static void Bank_ShutdownWidgets();
	static void Bank_Update();
#endif 

};

#endif // SAVEMIGRATION_H