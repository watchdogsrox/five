

#ifndef SAVEGAME_AUTOLOAD_H
#define SAVEGAME_AUTOLOAD_H

// Rage headers
#include "file/savegame.h"

// Game headers
#include "SaveLoad/savegame_date.h"
#include "SaveLoad/savegame_defines.h"
#include "scene/ExtraContent.h"
#include "text/TextFile.h"


enum eAutoLoadSignInReturnValue
{
	AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING,
	AUTOLOAD_SIGN_IN_RETURN_PLAYER_IS_SIGNED_IN,
	AUTOLOAD_SIGN_IN_RETURN_PLAYER_NOT_SIGNED_IN
};

enum eAutoLoadEnumerationReturnValue
{
	AUTOLOAD_ENUMERATION_RETURN_STILL_CHECKING,
	AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED,
	AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_COMPLETE
};

enum eAutoLoadInviteReturnValue
{
	AUTOLOAD_INVITE_RETURN_STILL_CHECKING_INVITE,
	AUTOLOAD_INVITE_RETURN_NO_INVITE,
	AUTOLOAD_INVITE_RETURN_INVITE_ACCEPTED
};

enum eAutoLoadDecisionReturnValue
{
	AUTOLOAD_DECISION_RETURN_STILL_CHECKING,
	AUTOLOAD_DECISION_RETURN_START_NEW_GAME,
	AUTOLOAD_DECISION_RETURN_DO_AUTOLOAD
	//	AUTOLOAD_DECISION_RETURN_JOIN_NETWORK_GAME
};


#define MAX_LENGTH_OF_AUTOLOAD_LOADING_SCREEN_TEXT	(64)


class CSavegameAutoload
{
	enum eAutoLoadSignInState
	{
		AUTOLOAD_STATE_CHECK_IF_PLAYER_IS_SIGNED_IN,
		AUTOLOAD_STATE_ASK_IF_PLAYER_WANTS_TO_SIGN_IN,
//		AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_ARE_YOU_SURE,
		AUTOLOAD_STATE_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN,
		AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_OK,
		AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN,
		AUTOLOAD_STATE_HIDE_WARNING_SCREEN_AND_SHOW_SYSTEM_UI,
		AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS,
		AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS_SIGN_IN_UI_IS_DISPLAYED
	};

#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	//	set a flag saying that the files have been scanned so it is okay to check for the most recent save?
	enum eAutoLoadFileEnumerationState	//	make this static within the second of the three functions
	{
		AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES,
		AUTOLOAD_STATE_CHECK_ENUMERATE_ALL_DEVICES
	};
#endif

	enum eAutoLoadInviteState
	{
		AUTOLOAD_STATE_BEGIN_CHECK_FOR_INVITE,
		AUTOLOAD_STATE_CONTINUE_CHECK_FOR_INVITE
	};

	enum eAutoLoadDecisionState
	{
		AUTOLOAD_STATE_DECIDE_ON_COURSE_OF_ACTION,
//		AUTOLOAD_STATE_WAIT_FOR_ACCEPT_INVITE,

#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
		AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE_QUICK,
		AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK,
		AUTOLOAD_STATE_GET_NAME_OF_MOST_RECENT_SAVE_QUICK,
#else
		AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE,
		AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE,
#endif	//	__PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

		AUTOLOAD_STATE_DISPLAY_EPISODIC_CONTENT_MESSAGE,
		AUTOLOAD_STATE_CHECK_IF_MOST_RECENT_SAVE_IS_DAMAGED,
		AUTOLOAD_STATE_DISPLAY_ERROR_MESSAGE,
		AUTOLOAD_STATE_BEGIN_LOAD
//		AUTOLOAD_STATE_DO_NOTHING
	};

private:
//	Private data
	static eAutoLoadSignInState ms_AutoLoadSignInState;

#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	static eAutoLoadFileEnumerationState ms_AutoLoadFileEnumerationState;
#endif

	static eAutoLoadInviteState ms_AutoLoadInviteState;

	static eAutoLoadDecisionState ms_AutoLoadDecisionState;

	static fiSaveGame::Content *ms_pSaveGameSlotsForAutoload;
	static s32		ms_NumberOfSaveGameSlotsForAutoload;

	static int ms_AutoLoadEpisodeForDiscBuild;

	static int ms_FirstCharacterOfAutoloadName;
	static int ms_LengthOfAutoloadName;

	static bool ms_bPerformingAutoLoadAtStartOfGame;

#if __PPU
	static bool ms_bAutoloadEnumerationHasCompleted;
#endif	//	__PPU

	static bool ms_InviteConfirmationBlockedDuringAutoload;

	static bool ms_bEpisodicMsgAlreadySeen;

	static CDate ms_MostRecentSaveDate;
	static int ms_MostRecentSaveSlot;
	static int ms_EpisodeNumberForSaveToBeLoaded;

//	Private functions

	static MemoryCardError GetTimeAndMissionNameFromDisplayName(int SaveGameSlot);

	static void ResetFileTimeScan();
	static MemoryCardError GetFileTimes();

	static void ClearSlot(s32 slotIndex);
	static void ClearSlotData(void);

	static bool IsSaveGameSlotEmpty(int SlotIndex);

	static char* GetNameOfSavedGameForMenu(int SlotNumber);
	static u32 GetDeviceId(int SlotIndex);
	static const char *GetFilename(int SlotIndex);
	static const char16 *GetDisplayNameForAutoload(int SlotIndex);

	static void ClearMostRecentSaveData();

#if RSG_ORBIS
	static void GetModificationTime(int SlotIndex, u64 &ModificationTime);
#endif

//	static void SetAutoloadEpisodeForDiscBuild(int EpisodeIndex) { ms_AutoLoadEpisodeForDiscBuild = EpisodeIndex; }

public:
//	Public functions
	static void Init(unsigned initMode);
	static void InitialiseAutoLoadAtStartOfGame();

	static void ResetAfterAutoloadAtStartOfGame();

	static void ResetAutoLoadSignInState() { ms_AutoLoadSignInState = AUTOLOAD_STATE_CHECK_IF_PLAYER_IS_SIGNED_IN; }
	static eAutoLoadSignInReturnValue AutoLoadCheckForSignedInPlayer();

#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	static void ResetAutoLoadFileEnumerationState() { ms_AutoLoadFileEnumerationState = AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES; }

	static eAutoLoadEnumerationReturnValue AutoLoadEnumerateContent();
#endif

	static void ResetAutoLoadInviteState() { ms_AutoLoadInviteState = AUTOLOAD_STATE_BEGIN_CHECK_FOR_INVITE; }
	static eAutoLoadInviteReturnValue AutoLoadCheckForAcceptingInvite();

	static void ResetAutoLoadDecisionState() { ms_AutoLoadDecisionState = AUTOLOAD_STATE_DECIDE_ON_COURSE_OF_ACTION; }
	static eAutoLoadDecisionReturnValue AutoLoadDecideWhetherToLoadOrStartANewGame();

	static MemoryCardError BeginGameAutoload();

	static void SetPerformingAutoLoadAtStartOfGame(bool bPerformingAutoLoad) { ms_bPerformingAutoLoadAtStartOfGame = bPerformingAutoLoad; }
	static bool IsPerformingAutoLoadAtStartOfGame() { return ms_bPerformingAutoLoadAtStartOfGame; }

#if __PPU
	static void SetAutoloadEnumerationHasCompleted();
	static bool GetAutoloadEnumerationHasCompleted() { return ms_bAutoloadEnumerationHasCompleted; }
#endif	//	__PPU

	static bool IsInviteConfirmationBlockedDuringAutoload();

	static int GetEpisodeNumberForTheSaveGameToBeLoaded() { return ms_EpisodeNumberForSaveToBeLoaded; }

	static bool ShowLoggedSignInUi();
};


#endif	//	SAVEGAME_AUTOLOAD_H

