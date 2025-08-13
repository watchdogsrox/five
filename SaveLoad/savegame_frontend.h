

#ifndef SAVEGAME_FRONTEND_H
#define SAVEGAME_FRONTEND_H

// rage headers
#include "atl/array.h"

// Game headers
#include "frontend/PauseMenu.h"
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/SavegameMigration/savegame_export.h"

#define HIDE_ACCEPTBUTTON_CONTEXT UIATSTRINGHASH("HIDE_ACCEPTBUTTON", 0x14211b54)
#define DELETE_SAVEGAME_CONTEXT UIATSTRINGHASH("DELETE_SAVEGAME",0x3dd23862)

class CSavegameFrontEnd : public CMenuBase
{
	enum eSavegameMenuFunction
	{
		SAVEGAME_MENU_FUNCTION_LOAD,
		SAVEGAME_MENU_FUNCTION_SAVE,
		SAVEGAME_MENU_FUNCTION_DELETE

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		, SAVEGAME_MENU_FUNCTION_EXPORT
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		, SAVEGAME_MENU_FUNCTION_IMPORT
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
	};

	enum eLoadMenuInstructionalButtonContexts
	{
		BUTTON_CONTEXT_WAITING_FOR_SIGN_IN,
		BUTTON_CONTEXT_WAITING_FOR_DEVICE_SELECT,
		BUTTON_CONTEXT_LOAD_GAME_MENU,
		BUTTON_CONTEXT_LOAD_GAME_MENU_DELETE_IN_PROGRESS,
		BUTTON_CONTEXT_SAVE_GAME_MENU_WAITING_FOR_MENU_TO_DISPLAY,
		BUTTON_CONTEXT_SAVE_GAME_MENU_IS_DISPLAYING,
		BUTTON_CONTEXT_SAVE_GAME_MENU_DELETE_IN_PROGRESS
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		, BUTTON_CONTEXT_UPLOAD_MENU_IS_DISPLAYING
		, BUTTON_CONTEXT_MIGRATION_INFO_SCREEN
		, BUTTON_CONTEXT_MIGRATION_INFO_SCREEN_NO_ACCEPT
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	};

private:
//	private data
	static s32 ms_SaveGameSlotToLoad;
	static s32 ms_SaveGameSlotToDelete;
	static s32 ms_CurrentlyHighlightedSlot;

	static eSavegameMenuFunction ms_SavegameMenuFunction;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static s32 ms_SaveGameSlotToExport;
	static char sm_MigrationInfoTextBuffer[MAX_CHARS_IN_EXTENDED_MESSAGE];
	static bool sm_bAllowAcceptButtonOnMigrationInfoScreen;
	static CSaveGameExport::eNetworkProblemsForSavegameExport sm_NetworkProblemForSavegameExport;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static s32 ms_SaveGameSlotToImport;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
	
	static atFixedArray<s32, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES> ms_SavegameSlotsArrangedByTime;
	static s32 ms_BaseMenuItem;

	static bool ms_bNeedToRestartDamagedCheck;

//	private functions
	static void SetLoadMenuContext(eLoadMenuInstructionalButtonContexts loadMenuContext);

	static s32 ConvertMenuItemToSlotIndex(s32 menu_item);
#if RSG_ORBIS
	static void ReplaceMenuItemWithSlotIndex(s32 menu_item, s32 slotIndex);
#endif
	static bool AddOneItemToSaveGameList(s32 MenuItem);

	static MemoryCardError SaveGameListScreenSetup(bool bRescanMemoryCard, bool bForceDeviceSelection, bool bForceCloseWhenSafe);

	static bool CreateListSortedByDate();

	static MemoryCardError CheckVisibleFilesForDamage(bool bForceCloseWhenSafe);

public:
	CSavegameFrontEnd(CMenuScreen& owner);
	~CSavegameFrontEnd();

	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	static void SetHighlightedMenuItem( s32 iMenuItem );

	static bool BeginSaveGameList();

	static MemoryCardError SaveGameMenuCheck(bool bQuitAsSoonAsNoSavegameOperationsAreRunning);

	static s32 GetSaveGameSlotToLoad()	{ return ms_SaveGameSlotToLoad; }

	static s32 GetSaveGameSlotToDelete()	{ return ms_SaveGameSlotToDelete; }
	static bool ShouldDeleteSavegame() { return (ms_SavegameMenuFunction == SAVEGAME_MENU_FUNCTION_DELETE); }

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static s32 GetSaveGameSlotToExport() { return ms_SaveGameSlotToExport; }
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static s32 GetSaveGameSlotToImport() { return ms_SaveGameSlotToImport; }
	static void SetSaveGameSlotForImporting(s32 slotNumber) { ms_SaveGameSlotToImport = slotNumber; }
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	static s32 GetNumberOfLinesToDisplayInSavegameList(bool bSaveMenu);
};


#endif	//	SAVEGAME_FRONTEND_H


