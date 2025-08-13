// 
// scene/dlc_channel.h 
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
//

//This is deliberately outside of the #include guards
//in order to mitigate problems with unity builds.
#undef  __dlc_channel
#define __dlc_channel dlc

#ifndef SCENE_DLC_CHANNEL_H 
#define SCENE_DLC_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(dlc)

#define dlcDisplayf(fmt,...)		RAGE_DISPLAYF(__dlc_channel,fmt,##__VA_ARGS__)
#define dlcAssert(cond)				RAGE_ASSERT(__dlc_channel,cond)
#define dlcAssertf(cond,fmt,...)	RAGE_ASSERTF(__dlc_channel,cond,fmt,##__VA_ARGS__)
#define dlcVerify(cond)				RAGE_VERIFY(__dlc_channel,cond)
#define dlcVerifyf(cond,fmt,...)	RAGE_VERIFYF(__dlc_channel,cond,fmt,##__VA_ARGS__)
#define dlcDebugf(fmt, ...)			RAGE_DEBUGF1(__dlc_channel, fmt, ##__VA_ARGS__)
#define dlcDebugf1(fmt, ...)		RAGE_DEBUGF1(__dlc_channel, fmt, ##__VA_ARGS__)
#define dlcDebugf2(fmt, ...)		RAGE_DEBUGF2(__dlc_channel, fmt, ##__VA_ARGS__)
#define dlcDebugf3(fmt, ...)		RAGE_DEBUGF3(__dlc_channel, fmt, ##__VA_ARGS__)
#define dlcWarningf(fmt, ...)		RAGE_WARNINGF(__dlc_channel, fmt, ##__VA_ARGS__)
#define dlcErrorf(fmt, ...)			RAGE_ERRORF(__dlc_channel, fmt, ##__VA_ARGS__)


#define DLC_OPTIMISATIONS_OFF	0

#if DLC_OPTIMISATIONS_OFF
#define DLC_OPTIMISATIONS()	OPTIMISATIONS_OFF()
#else
#define DLC_OPTIMISATIONS()
#endif	//DLC_OPTIMISATIONS_OFF

#endif // SCENE_DLC_CHANNEL_H 
