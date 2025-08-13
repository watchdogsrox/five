/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BusySpinner.h
// PURPOSE : allows control of the busy spinner via multiple systems.
//
// See: url:bugstar:966421 for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef BUSYSPINNER_H
#define BUSYSPINNER_H

// Rage headers
#include "atl/array.h"
#include "system/timer.h"

#define SPINNER_MAX_MESSAGE_LEN	64
#define SPINNER_SHOWING_NONE	-1
#define SPINNER_BACKOFF_TIME	1000
#define SPINNER_ICON_LOADING	1
#define SPINNER_ICON_NONE		-1

// matches what script and AS uses:
#define BUSYSPINNER_LOADING	5
#define BUSYSPINNER_SAVING	1
#define BUSYSPINNER_CLOUD		4

//#define USE_THEFEED_SAVING_MESSAGE

// The lower the value, the higher the priority.

enum
{
	SPINNER_SOURCE_SAVEGAME=0,	// Special case. ( Icon always has to show when active )
	SPINNER_SOURCE_SCRIPT,
	SPINNER_SOURCE_STORE_LOADING,
	SPINNER_SOURCE_FACEBOOK,
    SPINNER_SOURCE_INVITE_DETAILS,
	SPINNER_SOURCE_VIDEO_EDITOR,
	SPINNER_SOURCE_VIDEO_UPLOAD,
	SPINNER_SOURCE_LOADING_SCREEN,
	SPINNER_SOURCE_PROFANITY_CHECK,
	SPINNER_SOURCE_MAX
};

enum eBS_STATE
{
	BS_STATE_INACTIVE = 0,
	BS_STATE_LOADING,
	BS_STATE_SETUP,
	BS_STATE_RESETUP,
	BS_STATE_ACTIVE
};

class CBusySpinner
{
public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();

	static void Preload(bool waitForLoad = false);
	static void On( const char* bodyText, int Icon, int sourceIndex );
	static void Off( int sourceIndex );
	static bool IsOn();
	static bool IsDisplaying();
	static bool CanRender();
	static bool HasBodyText() {return ms_CurrentBodyTextActive[0] != '\0'; }

	static void RegisterInstructionalButtonMovie(s32 iNewButtonMovieId);
	static void UnregisterInstructionalButtonMovie(s32 iButtonMovieId);

	static bool IsActive() { return ms_iSpinnerMovieState == BS_STATE_ACTIVE; }

	static void SetInstantUpdate(bool value) { ms_bInstantUpdate = value; }

#if __BANK
	static void InitWidgets();
#endif // __BANK


private:

	struct sSpinner
	{
		int	Icon;
		char BodyText[SPINNER_MAX_MESSAGE_LEN];
	};

	static void UpdateSpinnerStates();
	static void HideSpinner();

	static void DisplaySpinnerOnAllMovies();
	static void HideSpinnerOnAllMovies();
	static void UpdateSpinnerDisplay();
	static bool CanChangeSpinner();
	static void CheckSaveGameSystemMessages();
	static void SetSavingText(s32 iMovieId);
	static void HideSavingText(s32 iMovieId);
	static void CheckSpinnerMoviesAreStillActive();

	static int		ms_ShowingIndex;
	static sSpinner	ms_SpinnerList[SPINNER_SOURCE_MAX];
	static u32		ms_BackoffTime;
	static sysTimer	ms_SysTimer;

	static char		ms_CurrentBodyText[SPINNER_MAX_MESSAGE_LEN];
	static int		ms_CurrentIcon;
	
	static char		ms_CurrentBodyTextActive[SPINNER_MAX_MESSAGE_LEN];
	static int		ms_CurrentIconActive;

	static bool		ms_IsBusySpinnerOn;
	static bool		ms_IsBusySpinnerDisplaying;

	static s32		ms_iSpinnerMovie;
	static eBS_STATE ms_iSpinnerMovieState;

	static bool sm_bReinitSpinnerOnAllMovies;
	static atArray<s32> sm_iInstructionalButtonMovies;

	static bool ms_bInstantUpdate;
	static sysTimer sm_time;

#if __BANK
	static void DebugSpinnerPreload();
	static void DebugSpinnerOn();
	static void DebugSpinnerOff();
	static void DebugCreateTheSpinnerBankWidgets();
	static void ShutdownWidgets();
#endif
};

#endif // BUSYSPINNER_H

