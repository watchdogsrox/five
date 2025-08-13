//
// vfx/vfx_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_VFX_CHANNEL_H
#define INC_VFX_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(smash)

#define smashAssert(cond)						RAGE_ASSERT(smash,cond)
#define smashAssertf(cond,fmt,...)				RAGE_ASSERTF(smash,cond,fmt,##__VA_ARGS__)
#define smashFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(smash,cond,fmt,##__VA_ARGS__)
#define smashVerifyf(cond,fmt,...)				RAGE_VERIFYF(smash,cond,fmt,##__VA_ARGS__)
#define smashErrorf(fmt,...)					RAGE_ERRORF(smash,fmt,##__VA_ARGS__)
#define smashWarningf(fmt,...)					RAGE_WARNINGF(smash,fmt,##__VA_ARGS__)
#define smashDisplayf(fmt,...)					RAGE_DISPLAYF(smash,fmt,##__VA_ARGS__)
#define smashDebugf1(fmt,...)					RAGE_DEBUGF1(smash,fmt,##__VA_ARGS__)
#define smashDebugf2(fmt,...)					RAGE_DEBUGF2(smash,fmt,##__VA_ARGS__)
#define smashDebugf3(fmt,...)					RAGE_DEBUGF3(smash,fmt,##__VA_ARGS__)
#define smashLogf(severity,fmt,...)				RAGE_LOGF(smash,severity,fmt,##__VA_ARGS__)
#define smashCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,smash,severity,fmt,##__VA_ARGS__)
	
RAGE_DECLARE_CHANNEL(sky)

#define skyAssert(cond)							RAGE_ASSERT(sky,cond)
#define skyAssertf(cond,fmt,...)				RAGE_ASSERTF(sky,cond,fmt,##__VA_ARGS__)
#define skyFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(sky,cond,fmt,##__VA_ARGS__)
#define skyVerifyf(cond,fmt,...)				RAGE_VERIFYF(sky,cond,fmt,##__VA_ARGS__)
#define skyErrorf(fmt,...)						RAGE_ERRORF(sky,fmt,##__VA_ARGS__)
#define skyWarningf(fmt,...)					RAGE_WARNINGF(sky,fmt,##__VA_ARGS__)
#define skyDisplayf(fmt,...)					RAGE_DISPLAYF(sky,fmt,##__VA_ARGS__)
#define skyDebugf1(fmt,...)						RAGE_DEBUGF1(sky,fmt,##__VA_ARGS__)
#define skyDebugf2(fmt,...)						RAGE_DEBUGF2(sky,fmt,##__VA_ARGS__)
#define skyDebugf3(fmt,...)						RAGE_DEBUGF3(sky,fmt,##__VA_ARGS__)
#define skyLogf(severity,fmt,...)				RAGE_LOGF(sky,severity,fmt,##__VA_ARGS__)
#define skyCondLogf(cond,severity,fmt,...)		RAGE_CONDLOGF(cond,sky,severity,fmt,##__VA_ARGS__)

#endif // INC_VFX_CHANNEL_H
