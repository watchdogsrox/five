/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LegacyLandingScreen.h
// PURPOSE : Legacy landing screen, used on PC builds
//
// AUTHOR  : james.strain
// STARTED : 2015, relocated October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LEGACY_LANDING_SCREEN_H
#define LEGACY_LANDING_SCREEN_H

// rage
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/singleton.h"

// game 
#include "frontend/boot_flow/BootupScreen.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

#if RSG_PC

class CLegacyLandingScreen : public CBootupScreen
{
public:
	CLegacyLandingScreen();
	~CLegacyLandingScreen();

    static void Init();
    static bool IsLandingPageInitialized();
	static void Shutdown();

	static void DeviceLost() {}
	static void DeviceReset();

	static bool IsLandingPageEnabled();

	bool DoesCurrentContextAllowNews() { return m_context != Context::SETTINGS; }

	bool Update();
	void Render();
	void UpdateDisplayConfig();
	
private: // declarations and variables
    enum class Context
    {
        INITIAL,
        MAIN,
        KEYMAPPING,
        SETTINGS,
        CONFIRM_QUIT
    };

    enum class BtnType
    {
        NONE = -1,
        STORY_MODE,
        ONLINE,
        RANDOM_JOB,
        EVENT,
        SETTINGS,
        QUIT
    };

    CScaleformMovieWrapper m_movie;
    atArray<BtnType> m_buttons;
    int m_currentSelection;
    Context m_context;

private: // methods
	BtnType CheckInput();

	void DrawButtons();
	void AddButton(const char* label, BtnType type, bool bIsLocString = true);

	void SetContext(Context const eContext)
    {
        uiDebugf3("CLandingScreen::SetLandingContext -- Old Context = %d, New Context = %d", m_context, eContext);
        m_context = eContext;
    }

	void SetSelection(int i);
};

typedef atSingleton<CLegacyLandingScreen> SLegacyLandingScreen;

#else // RSG_PC

class CLegacyLandingScreen : public CBootupScreen
{
public:
    static bool IsLandingPageEnabled() { return false; }
    static bool IsLandingPageInitialized() { return false; }
    static void DeviceReset() {}
};

#endif // RSG_PC

#endif // LEGACY_LANDING_SCREEN_H
