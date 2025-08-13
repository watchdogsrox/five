//
// population/population_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_POPULATION_CHANNEL_H
#define INC_POPULATION_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(population)

#define popAssert(cond)						RAGE_ASSERT(population,cond)
#define popAssertf(cond,fmt,...)			RAGE_ASSERTF(population,cond,fmt,##__VA_ARGS__)
#define popVerifyf(cond,fmt,...)			RAGE_VERIFYF(population,cond,fmt,##__VA_ARGS__)
#define popErrorf(fmt,...)					RAGE_ERRORF(population,fmt,##__VA_ARGS__)
#define popWarningf(fmt,...)				RAGE_WARNINGF(population,fmt,##__VA_ARGS__)
#define popDisplayf(fmt,...)				RAGE_DISPLAYF(population,fmt,##__VA_ARGS__)
#define popDebugf1(fmt,...)					RAGE_DEBUGF1(population,fmt,##__VA_ARGS__)
#define popDebugf2(fmt,...)					RAGE_DEBUGF2(population,fmt,##__VA_ARGS__)
#define popDebugf3(fmt,...)					RAGE_DEBUGF3(population,fmt,##__VA_ARGS__)
#define popLogf(severity,fmt,...)			RAGE_LOGF(population,severity,fmt,##__VA_ARGS__)
#define popCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,population,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_CHANNEL(pedReuse)

#define pedReuseAssert(cond)						RAGE_ASSERT(pedReuse,cond)
#define pedReuseAssertf(cond,fmt,...)			RAGE_ASSERTF(pedReuse,cond,fmt,##__VA_ARGS__)
#define pedReuseVerifyf(cond,fmt,...)			RAGE_VERIFYF(pedReuse,cond,fmt,##__VA_ARGS__)
#define pedReuseErrorf(fmt,...)					RAGE_ERRORF(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseWarningf(fmt,...)				RAGE_WARNINGF(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseDisplayf(fmt,...)				RAGE_DISPLAYF(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseDebugf1(fmt,...)					RAGE_DEBUGF1(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseDebugf2(fmt,...)					RAGE_DEBUGF2(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseDebugf3(fmt,...)					RAGE_DEBUGF3(pedReuse,fmt,##__VA_ARGS__)
#define pedReuseLogf(severity,fmt,...)			RAGE_LOGF(pedReuse,severity,fmt,##__VA_ARGS__)
#define pedReuseCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,pedReuse,severity,fmt,##__VA_ARGS__)


#endif // INC_POPULATION_CHANNEL_H
