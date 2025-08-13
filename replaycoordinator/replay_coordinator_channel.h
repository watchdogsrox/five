//
// replaycoordinator/replay_coordinator_channel.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

//This is deliberately outside of the #include guards
//in order to mitigate problems with unity builds.
#undef __rc_channel
#define __rc_channel replayCoordinator

#ifndef INC_REPLAY_COORDINATOR_CHANNEL_H
#define INC_REPLAY_COORDINATOR_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(replayCoordinator) // Defined in ReplayCoordinator.cpp

#define rcAssert(cond)						RAGE_ASSERT(__rc_channel,cond)
#define rcAssertf(cond,fmt,...)				RAGE_ASSERTF(__rc_channel,cond,fmt,##__VA_ARGS__)
#define rcVerify(cond)                      RAGE_VERIFY(__rc_channel, cond)
#define rcVerifyf(cond,fmt,...)				RAGE_VERIFYF(__rc_channel,cond,fmt,##__VA_ARGS__)
#define rcErrorf(fmt,...)					RAGE_ERRORF(__rc_channel,fmt,##__VA_ARGS__)
#define rcWarningf(fmt,...)					RAGE_WARNINGF(__rc_channel,fmt,##__VA_ARGS__)
#define rcDisplayf(fmt,...)					RAGE_DISPLAYF(__rc_channel,fmt,##__VA_ARGS__)
#define rcDebugf1(fmt,...)					RAGE_DEBUGF1(__rc_channel,fmt,##__VA_ARGS__)
#define rcDebugf2(fmt,...)					RAGE_DEBUGF2(__rc_channel,fmt,##__VA_ARGS__)
#define rcDebugf3(fmt,...)					RAGE_DEBUGF3(__rc_channel,fmt,##__VA_ARGS__)
#define rcLogf(severity,fmt,...)			RAGE_LOGF(__rc_channel,severity,fmt,##__VA_ARGS__)
#define rcCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,__rc_channel,severity,fmt,##__VA_ARGS__)

#endif // INC_REPLAY_COORDINATOR_CHANNEL_H
