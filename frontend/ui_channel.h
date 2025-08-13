//
// frontend/ui_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


//This is deliberately outside of the #include guards
//in order to mitigate problems with unity builds.
#undef __ui_channel
#define __ui_channel ui


#ifndef INC_UI_CHANNEL_H
#define INC_UI_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(ui) // Defined in PauseMenu.cpp

#ifdef __GNUC__
#define UI_FUNC_NAME			__PRETTY_FUNCTION__		// virtual void someClass::SomeMember(argType argName,...) (on GCC, __FUNCTION__ would just be SomeMember, which isn't very useful)
#else
#define UI_FUNC_NAME			__FUNCTION__			// someClass::SomeMember
#endif

#define uiAssert(cond)						RAGE_ASSERT(__ui_channel,cond)
#define uiAssertf(cond,fmt,...)				RAGE_ASSERTF(__ui_channel,cond,"%s - " fmt,UI_FUNC_NAME,##__VA_ARGS__)
#define uiVerify(cond)                      RAGE_VERIFY(__ui_channel, cond)
#define uiVerifyf(cond,fmt,...)				RAGE_VERIFYF(__ui_channel,	cond,"%s - " fmt,UI_FUNC_NAME,##__VA_ARGS__)
#define uiErrorf(fmt,...)					RAGE_ERRORF(__ui_channel,fmt,##__VA_ARGS__)
#define uiWarningf(fmt,...)					RAGE_WARNINGF(__ui_channel,fmt,##__VA_ARGS__)
#define uiDisplayf(fmt,...)					RAGE_DISPLAYF(__ui_channel,fmt,##__VA_ARGS__)
#define uiDebugf1(fmt,...)					RAGE_DEBUGF1(__ui_channel,fmt,##__VA_ARGS__)
#define uiDebugf2(fmt,...)					RAGE_DEBUGF2(__ui_channel,fmt,##__VA_ARGS__)
#define uiDebugf3(fmt,...)					RAGE_DEBUGF3(__ui_channel,fmt,##__VA_ARGS__)
#define uiLogf(severity,fmt,...)			RAGE_LOGF(__ui_channel,severity,fmt,##__VA_ARGS__)
#define uiCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,__ui_channel,severity,fmt,##__VA_ARGS__)
#define uiFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(__ui_channel,cond,"%s - " fmt,UI_FUNC_NAME,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(ui,report)

#define reportAssertf(cond,fmt,...)				RAGE_ASSERTF(ui_report,cond,fmt,##__VA_ARGS__)
#define reportVerifyf(cond,fmt,...)				RAGE_VERIFYF(ui_report,cond,fmt,##__VA_ARGS__)
#define reportErrorf(fmt,...)					RAGE_ERRORF(ui_report,fmt,##__VA_ARGS__)
#define reportWarningf(fmt,...)					RAGE_WARNINGF(ui_report,fmt,##__VA_ARGS__)
#define reportDisplayf(fmt,...)					RAGE_DISPLAYF(ui_report,fmt,##__VA_ARGS__)
#define reportDebugf1(fmt,...)					RAGE_DEBUGF1(ui_report,fmt,##__VA_ARGS__)
#define reportDebugf2(fmt,...)					RAGE_DEBUGF2(ui_report,fmt,##__VA_ARGS__)
#define reportDebugf3(fmt,...)					RAGE_DEBUGF3(ui_report,fmt,##__VA_ARGS__)
#define reportLogf(severity,fmt,...)			RAGE_LOGF(ui_report,severity,fmt,##__VA_ARGS__)
#define reportCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ui_report,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(ui,playercard)
RAGE_DECLARE_SUBCHANNEL(ui,netplayercard)

#define playercardAssertf(cond,fmt,...)				RAGE_ASSERTF(ui_playercard,cond,fmt,##__VA_ARGS__)
#define playercardNetAssertf(cond,fmt,...)			RAGE_ASSERTF(ui_netplayercard,cond,fmt,##__VA_ARGS__)
#define playercardVerify(cond)                      RAGE_VERIFY(ui_playercard, cond)
#define playercardVerifyf(cond,fmt,...)				RAGE_VERIFYF(ui_playercard,cond,fmt,##__VA_ARGS__)
#define playercardNetVerifyf(cond,fmt,...)			RAGE_VERIFYF(ui_netplayercard,cond,fmt,##__VA_ARGS__)
#define playercardFatalf(fmt,...)					RAGE_FATALF(ui_playercard,fmt,##__VA_ARGS__)
#define playercardErrorf(fmt,...)					RAGE_ERRORF(ui_playercard,fmt,##__VA_ARGS__)
#define playercardWarningf(fmt,...)					RAGE_WARNINGF(ui_playercard,fmt,##__VA_ARGS__)
#define playercardNetWarningf(fmt,...)				RAGE_WARNINGF(ui_netplayercard,fmt,##__VA_ARGS__)
#define playercardDisplayf(fmt,...)					RAGE_DISPLAYF(ui_playercard,fmt,##__VA_ARGS__)
#define playercardDebugf1(fmt,...)					RAGE_DEBUGF1(ui_playercard,fmt,##__VA_ARGS__)
#define playercardNetDebugf1(fmt,...)				RAGE_DEBUGF1(ui_netplayercard,fmt,##__VA_ARGS__)
#define playercardDebugf2(fmt,...)					RAGE_DEBUGF2(ui_playercard,fmt,##__VA_ARGS__)
#define playercardDebugf3(fmt,...)					RAGE_DEBUGF3(ui_playercard,fmt,##__VA_ARGS__)
#define playercardLogf(severity,fmt,...)			RAGE_LOGF(ui_playercard,severity,fmt,##__VA_ARGS__)
#define playercardCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ui_playercard,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(ui,spinner)

#define spinnerAssertf(cond,fmt,...)				RAGE_ASSERTF(ui_spinner,cond,fmt,##__VA_ARGS__)
#define spinnerVerify(cond)							RAGE_VERIFY(ui_spinner, cond)
#define spinnerVerifyf(cond,fmt,...)				RAGE_VERIFYF(ui_spinner,cond,fmt,##__VA_ARGS__)
#define spinnerFatalf(fmt,...)						RAGE_FATALF(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerErrorf(fmt,...)						RAGE_ERRORF(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerWarningf(fmt,...)					RAGE_WARNINGF(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerDisplayf(fmt,...)					RAGE_DISPLAYF(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerDebugf1(fmt,...)						RAGE_DEBUGF1(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerDebugf2(fmt,...)						RAGE_DEBUGF2(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerDebugf3(fmt,...)						RAGE_DEBUGF3(ui_spinner,fmt,##__VA_ARGS__)
#define spinnerLogf(severity,fmt,...)				RAGE_LOGF(ui_spinner,severity,fmt,##__VA_ARGS__)
#define spinnerCondLogf(cond,severity,fmt,...)		RAGE_CONDLOGF(cond,ui_spinner,severity,fmt,##__VA_ARGS__)


#if RSG_PC
RAGE_DECLARE_SUBCHANNEL(ui,mpchat)

#define mpchatAssertf(cond,fmt,...)				RAGE_ASSERTF(ui_mpchat,cond,fmt,##__VA_ARGS__)
#define mpchatVerifyf(cond,fmt,...)				RAGE_VERIFYF(ui_mpchat,cond,fmt,##__VA_ARGS__)
#define mpchatErrorf(fmt,...)					RAGE_ERRORF(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatWarningf(fmt,...)					RAGE_WARNINGF(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatDisplayf(fmt,...)					RAGE_DISPLAYF(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatDebugf1(fmt,...)					RAGE_DEBUGF1(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatDebugf2(fmt,...)					RAGE_DEBUGF2(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatDebugf3(fmt,...)					RAGE_DEBUGF3(ui_mpchat,fmt,##__VA_ARGS__)
#define mpchatLogf(severity,fmt,...)			RAGE_LOGF(ui_mpchat,severity,fmt,##__VA_ARGS__)
#define mpchatCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ui_mpchat,severity,fmt,##__VA_ARGS__)
#endif  // RSG_PC

RAGE_DECLARE_SUBCHANNEL(ui,playerlist)

#define playerlistAssertf(cond,fmt,...)				RAGE_ASSERTF(ui_playerlist,cond,fmt,##__VA_ARGS__)
#define playerlistVerifyf(cond,fmt,...)				RAGE_VERIFYF(ui_playerlist,cond,fmt,##__VA_ARGS__)
#define playerlistErrorf(fmt,...)					RAGE_ERRORF(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistWarningf(fmt,...)					RAGE_WARNINGF(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistDisplayf(fmt,...)					RAGE_DISPLAYF(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistDebugf1(fmt,...)					RAGE_DEBUGF1(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistDebugf2(fmt,...)					RAGE_DEBUGF2(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistDebugf3(fmt,...)					RAGE_DEBUGF3(ui_playerlist,fmt,##__VA_ARGS__)
#define playerlistLogf(severity,fmt,...)			RAGE_LOGF(ui_playerlist,severity,fmt,##__VA_ARGS__)
#define playerlistCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ui_playerlist,severity,fmt,##__VA_ARGS__)

#endif // INC_UI_CHANNEL_H
