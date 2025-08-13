/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiLandingPageMessages.h
// PURPOSE : Contains the core messages used by the landing page. We have
//           some generic messages in the page deck system, but these all
//           slant to functionality that does not genericise
// 
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_LANDING_PAGE_MESSAGES_H
#define UI_LANDING_PAGE_MESSAGES_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// rage
#include "parser/macros.h"
#include "system/noncopyable.h"
#include "atl/string.h"

// game
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "game/ScriptRouterContextEnums.h"

class uiLandingPage_GoToStoryMode final : public uiPageDeckMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiLandingPage_GoToStoryMode, uiPageDeckMessageBase );
public:
    uiLandingPage_GoToStoryMode() {}
    virtual ~uiLandingPage_GoToStoryMode() {}

private:
    PAR_PARSABLE;
};

class uiLandingPage_GoToOnlineMode final : public uiPageDeckMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiLandingPage_GoToOnlineMode, uiPageDeckMessageBase );
public:
    uiLandingPage_GoToOnlineMode() : m_scriptRouterOnlineModeType(SRCM_FREE), m_scriptRouterArgType(SRCA_NONE) {}
	explicit uiLandingPage_GoToOnlineMode(eScriptRouterContextMode const& scriptRouterOnlineMode, eScriptRouterContextArgument const& scriptRouterArgType, const char* scriptRouterArg) :
		m_scriptRouterOnlineModeType(scriptRouterOnlineMode), m_scriptRouterArgType(scriptRouterArgType), m_scriptRouterArg(scriptRouterArg) { }
    virtual ~uiLandingPage_GoToOnlineMode() {}
	
	virtual void CleanupOnConsumed() final;

	eScriptRouterContextMode GetScriptRouterOnlineModeType() const { return m_scriptRouterOnlineModeType; }
	eScriptRouterContextArgument GetScriptRouterArgType() const { return m_scriptRouterArgType; }
	const char* GetScriptRouterArg() const { return m_scriptRouterArg; }

private:
	eScriptRouterContextMode m_scriptRouterOnlineModeType;
	eScriptRouterContextArgument m_scriptRouterArgType;
	atString m_scriptRouterArg;

    PAR_PARSABLE;
};

class uiLandingPage_GoToEditorMode final : public uiPageDeckMessageBase
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiLandingPage_GoToEditorMode, uiPageDeckMessageBase);
public:
	uiLandingPage_GoToEditorMode() {}
	virtual ~uiLandingPage_GoToEditorMode() {}

private:
	PAR_PARSABLE;
};

class uiLandingPage_Dismiss final : public uiPageDeckMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiLandingPage_Dismiss, uiPageDeckMessageBase );
public:
    uiLandingPage_Dismiss() {}
    virtual ~uiLandingPage_Dismiss() {}

private:
    PAR_PARSABLE;
};

class uiRefreshStoryItemMsg : public uiPageDeckMessageBase
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiRefreshStoryItemMsg, uiPageDeckMessageBase);
public:
	uiRefreshStoryItemMsg() {}
	virtual ~uiRefreshStoryItemMsg() {}
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // UI_LANDING_PAGE_MESSAGES_H
