// 
// cutscene/cutscene_channel.h 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_CUTSCENE_CHANNEL_H 
#define INC_CUTSCENE_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(cutscene)

#define cutsceneAssert(cond)						RAGE_ASSERT(cutscene,cond)
#define cutsceneAssertf(cond,fmt,...)				RAGE_ASSERTF(cutscene,cond,fmt,##__VA_ARGS__)
#define cutsceneVerifyf(cond,fmt,...)				RAGE_VERIFYF(cutscene,cond,fmt,##__VA_ARGS__)
#define cutsceneErrorf(fmt,...)						RAGE_ERRORF(cutscene,fmt,##__VA_ARGS__)
#define cutsceneWarningf(fmt,...)					RAGE_WARNINGF(cutscene,fmt,##__VA_ARGS__)
#define cutsceneDisplayf(fmt,...)					RAGE_DISPLAYF(cutscene,fmt,##__VA_ARGS__)
#define cutsceneDebugf1(fmt,...)					RAGE_DEBUGF1(cutscene,fmt,##__VA_ARGS__)
#define cutsceneDebugf2(fmt,...)					RAGE_DEBUGF2(cutscene,fmt,##__VA_ARGS__)
#define cutsceneDebugf3(fmt,...)					RAGE_DEBUGF3(cutscene,fmt,##__VA_ARGS__)
#define cutsceneLogf(severity,fmt,...)				RAGE_LOGF(cutscene,severity,fmt,##__VA_ARGS__)
#define cutsceneCondLogf(cond,severity,fmt,...)		RAGE_CONDLOGF(cond,cutscene,severity,fmt,##__VA_ARGS__)


#endif // INC_cutscene_CHANNEL_H 