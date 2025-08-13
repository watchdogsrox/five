//
// ped/ped_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_PED_CHANNEL_H
#define INC_PED_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(ped)

#define pedAssert(cond)						RAGE_ASSERT(ped,cond)
#define pedAssertf(cond,fmt,...)			RAGE_ASSERTF(ped,cond,fmt,##__VA_ARGS__)
#define pedFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(ped,cond,fmt,##__VA_ARGS__)
#define pedVerifyf(cond,fmt,...)			RAGE_VERIFYF(ped,cond,fmt,##__VA_ARGS__)
#define pedErrorf(fmt,...)					RAGE_ERRORF(ped,fmt,##__VA_ARGS__)
#define pedWarningf(fmt,...)				RAGE_WARNINGF(ped,fmt,##__VA_ARGS__)
#define pedDisplayf(fmt,...)				RAGE_DISPLAYF(ped,fmt,##__VA_ARGS__)
#define pedDebugf1(fmt,...)					RAGE_DEBUGF1(ped,fmt,##__VA_ARGS__)
#define pedDebugf2(fmt,...)					RAGE_DEBUGF2(ped,fmt,##__VA_ARGS__)
#define pedDebugf3(fmt,...)					RAGE_DEBUGF3(ped,fmt,##__VA_ARGS__)
#define pedLogf(severity,fmt,...)			RAGE_LOGF(ped,severity,fmt,##__VA_ARGS__)
#define pedCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ped,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_CHANNEL(respawn)

#define respawnAssert(cond)						RAGE_ASSERT(respawn,cond)
#define respawnAssertf(cond,fmt,...)			RAGE_ASSERTF(respawn,cond,fmt,##__VA_ARGS__)
#define respawnFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(respawn,cond,fmt,##__VA_ARGS__)
#define respawnVerifyf(cond,fmt,...)			RAGE_VERIFYF(respawn,cond,fmt,##__VA_ARGS__)
#define respawnErrorf(fmt,...)					RAGE_ERRORF(respawn,fmt,##__VA_ARGS__)
#define respawnWarningf(fmt,...)				RAGE_WARNINGF(respawn,fmt,##__VA_ARGS__)
#define respawnDisplayf(fmt,...)				RAGE_DISPLAYF(respawn,fmt,##__VA_ARGS__)
#define respawnDebugf1(fmt,...)					RAGE_DEBUGF1(respawn,fmt,##__VA_ARGS__)
#define respawnDebugf2(fmt,...)					RAGE_DEBUGF2(respawn,fmt,##__VA_ARGS__)
#define respawnDebugf3(fmt,...)					RAGE_DEBUGF3(respawn,fmt,##__VA_ARGS__)
#define respawnLogf(severity,fmt,...)			RAGE_LOGF(respawn,severity,fmt,##__VA_ARGS__)
#define respawnCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,respawn,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_CHANNEL(wanted)

#define wantedAssert(cond)						RAGE_ASSERT(wanted,cond)
#define wantedAssertf(cond,fmt,...)				RAGE_ASSERTF(wanted,cond,fmt,##__VA_ARGS__)
#define wantedFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(wanted,cond,fmt,##__VA_ARGS__)
#define wantedVerifyf(cond,fmt,...)				RAGE_VERIFYF(wanted,cond,fmt,##__VA_ARGS__)
#define wantedErrorf(fmt,...)					RAGE_ERRORF(wanted,fmt,##__VA_ARGS__)
#define wantedWarningf(fmt,...)					RAGE_WARNINGF(wanted,fmt,##__VA_ARGS__)
#define wantedDisplayf(fmt,...)					RAGE_DISPLAYF(wanted,fmt,##__VA_ARGS__)
#define wantedDebugf1(fmt,...)					RAGE_DEBUGF1(wanted,fmt,##__VA_ARGS__)
#define wantedDebugf2(fmt,...)					RAGE_DEBUGF2(wanted,fmt,##__VA_ARGS__)
#define wantedDebugf3(fmt,...)					RAGE_DEBUGF3(wanted,fmt,##__VA_ARGS__)
#define wantedLogf(severity,fmt,...)			RAGE_LOGF(wanted,severity,fmt,##__VA_ARGS__)
#define wantedCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,wanted,severity,fmt,##__VA_ARGS__)

#endif // INC_PED_CHANNEL_H
