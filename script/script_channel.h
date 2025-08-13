// 
// debug/script_channel.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_SCRIPT_CHANNEL_H 
#define INC_SCRIPT_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(script)		// defined in debug.cpp
#define __script_channel script

#define scriptAssertf(cond,fmt,...)				RAGE_ASSERTF(__script_channel,cond,fmt,##__VA_ARGS__)
#define scriptVerify(cond)				        RAGE_VERIFY(__script_channel,cond)
#define scriptVerifyf(cond,fmt,...)				RAGE_VERIFYF(__script_channel,cond,fmt,##__VA_ARGS__)
#define scriptErrorf(fmt,...)					RAGE_ERRORF(__script_channel,fmt,##__VA_ARGS__)
#define scriptWarningf(fmt,...)					RAGE_WARNINGF(__script_channel,fmt,##__VA_ARGS__)
#define scriptDisplayf(fmt,...)					RAGE_DISPLAYF(__script_channel,fmt,##__VA_ARGS__)
#define scriptDebugf1(fmt,...)					RAGE_DEBUGF1(__script_channel,fmt,##__VA_ARGS__)
#define scriptDebugf2(fmt,...)					RAGE_DEBUGF2(__script_channel,fmt,##__VA_ARGS__)
#define scriptDebugf3(fmt,...)					RAGE_DEBUGF3(__script_channel,fmt,##__VA_ARGS__)
#define scriptLogf(severity,fmt,...)			RAGE_LOGF(__script_channel,severity,fmt,##__VA_ARGS__)
#define scriptCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,__script_channel,severity,fmt,##__VA_ARGS__)

#if !__ASSERT && !__NO_OUTPUT
#define scriptLogAssertf(cond,fmt,...)	do { if(!(cond)) { scriptErrorf(fmt,##__VA_ARGS__); } } while (false);
#else
#define scriptLogAssertf(cond,fmt,...) scriptAssertf(cond,fmt,##__VA_ARGS__)
#endif


RAGE_DECLARE_SUBCHANNEL(script,camera)

#define scriptcameraAssertf(cond,fmt,...)				RAGE_ASSERTF(script_camera,cond,fmt,##__VA_ARGS__)
#define scriptcameraVerifyf(cond,fmt,...)				RAGE_VERIFYF(script_camera,cond,fmt,##__VA_ARGS__)
#define scriptcameraErrorf(fmt,...)					RAGE_ERRORF(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraWarningf(fmt,...)					RAGE_WARNINGF(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraDisplayf(fmt,...)					RAGE_DISPLAYF(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraDebugf1(fmt,...)					RAGE_DEBUGF1(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraDebugf2(fmt,...)					RAGE_DEBUGF2(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraDebugf3(fmt,...)					RAGE_DEBUGF3(script_camera,fmt,##__VA_ARGS__)
#define scriptcameraLogf(severity,fmt,...)			RAGE_LOGF(script_camera,severity,fmt,##__VA_ARGS__)
#define scriptcameraCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,script_camera,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(script,hud)

#define scripthudAssertf(cond,fmt,...)				RAGE_ASSERTF(script_hud,cond,fmt,##__VA_ARGS__)
#define scripthudVerifyf(cond,fmt,...)				RAGE_VERIFYF(script_hud,cond,fmt,##__VA_ARGS__)
#define scripthudErrorf(fmt,...)					RAGE_ERRORF(script_hud,fmt,##__VA_ARGS__)
#define scripthudWarningf(fmt,...)					RAGE_WARNINGF(script_hud,fmt,##__VA_ARGS__)
#define scripthudDisplayf(fmt,...)					RAGE_DISPLAYF(script_hud,fmt,##__VA_ARGS__)
#define scripthudDebugf1(fmt,...)					RAGE_DEBUGF1(script_hud,fmt,##__VA_ARGS__)
#define scripthudDebugf2(fmt,...)					RAGE_DEBUGF2(script_hud,fmt,##__VA_ARGS__)
#define scripthudDebugf3(fmt,...)					RAGE_DEBUGF3(script_hud,fmt,##__VA_ARGS__)
#define scripthudLogf(severity,fmt,...)			RAGE_LOGF(script_hud,severity,fmt,##__VA_ARGS__)
#define scripthudCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,script_hud,severity,fmt,##__VA_ARGS__)


#endif // INC_SCRIPT_CHANNEL_H 
