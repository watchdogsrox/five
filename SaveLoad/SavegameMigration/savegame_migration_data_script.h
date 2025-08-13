#ifndef SAVEGAME_MIGRATION_DATA_SCRIPT_H
#define SAVEGAME_MIGRATION_DATA_SCRIPT_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Forward declarations
namespace rage {
	class parTree;
	class psoFile;
}


class CSaveGameMigrationData_Script
{
public:
	CSaveGameMigrationData_Script() { Initialize(); }
	~CSaveGameMigrationData_Script() { Delete(); }

	void Initialize();
	bool Create();
	void Delete();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	bool ReadDataFromPsoFile(psoFile* psofile);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	bool ReadDataFromParTree(parTree *pParTree);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

private:
	u8 *AllocateMemoryForScriptGlobals(s32 numberOfBytes);
	void FreeMemoryForScriptGlobals(u8 **ppScriptGlobalsMemory);

	u8 *m_pSavedScriptGlobalsForMainScript;
	atArray<u8*> m_pSavedScriptGlobalsForDLCScript;
};

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_MIGRATION_DATA_SCRIPT_H

