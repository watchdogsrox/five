// 
// stats/stats_channel.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_STATS_CHANNEL_H 
#define INC_STATS_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(stat)   // defined in StatsMgr.cpp
#define __stat_channel stat

#define statAssert(cond)                    RAGE_ASSERT(__stat_channel,cond)
#define statAssertf(cond,fmt,...)           RAGE_ASSERTF(__stat_channel,cond,fmt,##__VA_ARGS__)
#define statVerify(cond)                    RAGE_VERIFY(__stat_channel,cond)
#define statVerifyf(cond,fmt,...)           RAGE_VERIFYF(__stat_channel,cond,fmt,##__VA_ARGS__)
#define statErrorf(fmt,...)					RAGE_ERRORF(__stat_channel,fmt,##__VA_ARGS__)
#define statWarningf(fmt,...)               RAGE_WARNINGF(__stat_channel,fmt,##__VA_ARGS__)
#define statDisplayf(fmt,...)               RAGE_DISPLAYF(__stat_channel,fmt,##__VA_ARGS__)
#define statDebugf1(fmt,...)                RAGE_DEBUGF1(__stat_channel,fmt,##__VA_ARGS__)
#define statDebugf2(fmt,...)                RAGE_DEBUGF2(__stat_channel,fmt,##__VA_ARGS__)
#define statDebugf3(fmt,...)                RAGE_DEBUGF3(__stat_channel,fmt,##__VA_ARGS__)
#define statLogf(severity,fmt,...)          RAGE_LOGF(__stat_channel,severity,fmt,##__VA_ARGS__)
#define statCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,__stat_channel,severity,fmt,##__VA_ARGS__)

#endif // INC_STATS_CHANNEL_H 

