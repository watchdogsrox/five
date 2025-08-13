// 
// debug/art_channel.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_ART_CHANNEL_H 
#define INC_ART_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(art)		// defined in debug.cpp

#define artAssertf(cond,fmt,...)			RAGE_ASSERTF(art,cond,fmt,##__VA_ARGS__)
#define artFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(art,cond,fmt,##__VA_ARGS__)
#define artVerifyf(cond,fmt,...)			RAGE_VERIFYF(art,cond,fmt,##__VA_ARGS__)
#define artFatalf(fmt,...)					RAGE_FATALF(art,fmt,##__VA_ARGS__)
#define artErrorf(fmt,...)					RAGE_ERRORF(art,fmt,##__VA_ARGS__)
#define artWarningf(fmt,...)				RAGE_WARNINGF(art,fmt,##__VA_ARGS__)
#define artDisplayf(fmt,...)				RAGE_DISPLAYF(art,fmt,##__VA_ARGS__)
#define artDebugf1(fmt,...)					RAGE_DEBUGF1(art,fmt,##__VA_ARGS__)
#define artDebugf2(fmt,...)					RAGE_DEBUGF2(art,fmt,##__VA_ARGS__)
#define artDebugf3(fmt,...)					RAGE_DEBUGF3(art,fmt,##__VA_ARGS__)
#define artLogf(severity,fmt,...)			RAGE_LOGF(art,severity,fmt,##__VA_ARGS__)


#endif // INC_ART_CHANNEL_H 
