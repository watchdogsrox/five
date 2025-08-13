// 
// core/core_channel.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_CORE_CHANNEL_H 
#define INC_CORE_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(core) // defined in game.cpp

#define coreAssert(cond)                    RAGE_ASSERT(core,cond)
#define coreAssertf(cond,fmt,...)           RAGE_ASSERTF(core,cond,fmt,##__VA_ARGS__)
#define coreVerifyf(cond,fmt,...)           RAGE_VERIFYF(core,cond,fmt,##__VA_ARGS__)
#define coreErrorf(fmt,...)					RAGE_ERRORF(core,fmt,##__VA_ARGS__)
#define coreWarningf(fmt,...)               RAGE_WARNINGF(core,fmt,##__VA_ARGS__)
#define coreDisplayf(fmt,...)               RAGE_DISPLAYF(core,fmt,##__VA_ARGS__)
#define coreDebugf1(fmt,...)                RAGE_DEBUGF1(core,fmt,##__VA_ARGS__)
#define coreDebugf2(fmt,...)                RAGE_DEBUGF2(core,fmt,##__VA_ARGS__)
#define coreDebugf3(fmt,...)                RAGE_DEBUGF3(core,fmt,##__VA_ARGS__)
#define coreLogf(severity,fmt,...)          RAGE_LOGF(core,severity,fmt,##__VA_ARGS__)
#define coreCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,core,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(core, game) // defined in game.cpp

#define gameAssert(cond)                    RAGE_ASSERT(core_game,cond)
#define gameAssertf(cond,fmt,...)           RAGE_ASSERTF(core_game,cond,fmt,##__VA_ARGS__)
#define gameVerifyf(cond,fmt,...)           RAGE_VERIFYF(core_game,cond,fmt,##__VA_ARGS__)
#define gameErrorf(fmt,...)					RAGE_ERRORF(core_game,fmt,##__VA_ARGS__)
#define gameWarningf(fmt,...)               RAGE_WARNINGF(core_game,fmt,##__VA_ARGS__)
#define gameDisplayf(fmt,...)               RAGE_DISPLAYF(core_game,fmt,##__VA_ARGS__)
#define gameDebugf1(fmt,...)                RAGE_DEBUGF1(core_game,fmt,##__VA_ARGS__)
#define gameDebugf2(fmt,...)                RAGE_DEBUGF2(core_game,fmt,##__VA_ARGS__)
#define gameDebugf3(fmt,...)                RAGE_DEBUGF3(core_game,fmt,##__VA_ARGS__)
#define gameLogf(severity,fmt,...)          RAGE_LOGF(core_game,severity,fmt,##__VA_ARGS__)
#define gameCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,core_game,severity,fmt,##__VA_ARGS__)

#endif // INC_CORE_CHANNEL_H 

