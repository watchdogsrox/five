// 
// SaveLoad/savegame_channel.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef INC_SAVEGAME_CHANNEL_H 
#define INC_SAVEGAME_CHANNEL_H 

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(savegame)		// defined in GenericGameStorage.cpp
#define __savegame_channel savegame

#define savegameAssertf(cond,fmt,...)				RAGE_ASSERTF(__savegame_channel,cond,fmt,##__VA_ARGS__)
#define savegameVerify(cond)				        RAGE_VERIFY(__savegame_channel,cond)
#define savegameVerifyf(cond,fmt,...)				RAGE_VERIFYF(__savegame_channel,cond,fmt,##__VA_ARGS__)
#define savegameErrorf(fmt,...)					RAGE_ERRORF(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameWarningf(fmt,...)					RAGE_WARNINGF(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameDisplayf(fmt,...)					RAGE_DISPLAYF(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameDebugf1(fmt,...)					RAGE_DEBUGF1(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameDebugf2(fmt,...)					RAGE_DEBUGF2(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameDebugf3(fmt,...)					RAGE_DEBUGF3(__savegame_channel,fmt,##__VA_ARGS__)
#define savegameLogf(severity,fmt,...)			RAGE_LOGF(__savegame_channel,severity,fmt,##__VA_ARGS__)
#define savegameCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,__savegame_channel,severity,fmt,##__VA_ARGS__)


RAGE_DECLARE_SUBCHANNEL(savegame,photo)

#define photoAssertf(cond,fmt,...)				RAGE_ASSERTF(savegame_photo,cond,fmt,##__VA_ARGS__)
#define photoVerifyf(cond,fmt,...)				RAGE_VERIFYF(savegame_photo,cond,fmt,##__VA_ARGS__)
#define photoErrorf(fmt,...)					RAGE_ERRORF(savegame_photo,fmt,##__VA_ARGS__)
#define photoWarningf(fmt,...)					RAGE_WARNINGF(savegame_photo,fmt,##__VA_ARGS__)
#define photoDisplayf(fmt,...)					RAGE_DISPLAYF(savegame_photo,fmt,##__VA_ARGS__)
#define photoDebugf1(fmt,...)					RAGE_DEBUGF1(savegame_photo,fmt,##__VA_ARGS__)
#define photoDebugf2(fmt,...)					RAGE_DEBUGF2(savegame_photo,fmt,##__VA_ARGS__)
#define photoDebugf3(fmt,...)					RAGE_DEBUGF3(savegame_photo,fmt,##__VA_ARGS__)
#define photoLogf(severity,fmt,...)			RAGE_LOGF(savegame_photo,severity,fmt,##__VA_ARGS__)
#define photoCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,savegame_photo,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(savegame, psosize)

#define psoSizeAssertf(cond,fmt,...)				RAGE_ASSERTF(savegame_psosize,cond,fmt,##__VA_ARGS__)
#define psoSizeVerifyf(cond,fmt,...)				RAGE_VERIFYF(savegame_psosize,cond,fmt,##__VA_ARGS__)
#define psoSizeErrorf(fmt,...)					RAGE_ERRORF(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeWarningf(fmt,...)					RAGE_WARNINGF(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeDisplayf(fmt,...)					RAGE_DISPLAYF(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeDebugf1(fmt,...)					RAGE_DEBUGF1(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeDebugf2(fmt,...)					RAGE_DEBUGF2(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeDebugf3(fmt,...)					RAGE_DEBUGF3(savegame_psosize,fmt,##__VA_ARGS__)
#define psoSizeLogf(severity,fmt,...)			RAGE_LOGF(savegame_psosize,severity,fmt,##__VA_ARGS__)
#define psoSizeCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,savegame_psosize,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(savegame,script_data)
#define savegameScriptDataDisplayf(fmt,...)					RAGE_DISPLAYF(savegame_script_data,fmt,##__VA_ARGS__)


RAGE_DECLARE_SUBCHANNEL(savegame, mp_fow)

#define savegameMpFowAssertf(cond,fmt,...)				RAGE_ASSERTF(savegame_mp_fow,cond,fmt,##__VA_ARGS__)
#define savegameMpFowVerifyf(cond,fmt,...)				RAGE_VERIFYF(savegame_mp_fow,cond,fmt,##__VA_ARGS__)
#define savegameMpFowErrorf(fmt,...)					RAGE_ERRORF(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowWarningf(fmt,...)					RAGE_WARNINGF(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowDisplayf(fmt,...)					RAGE_DISPLAYF(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowDebugf1(fmt,...)					RAGE_DEBUGF1(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowDebugf2(fmt,...)					RAGE_DEBUGF2(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowDebugf3(fmt,...)					RAGE_DEBUGF3(savegame_mp_fow,fmt,##__VA_ARGS__)
#define savegameMpFowLogf(severity,fmt,...)				RAGE_LOGF(savegame_mp_fow,severity,fmt,##__VA_ARGS__)
#define savegameMpFowCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,savegame_mp_fow,severity,fmt,##__VA_ARGS__)


#if RSG_PC
	RAGE_DECLARE_SUBCHANNEL(savegame, photo_mouse)

	#define photomouseAssertf(cond,fmt,...)				RAGE_ASSERTF(savegame_photo_mouse,cond,fmt,##__VA_ARGS__)
	#define photomouseVerifyf(cond,fmt,...)				RAGE_VERIFYF(savegame_photo_mouse,cond,fmt,##__VA_ARGS__)
	#define photomouseErrorf(fmt,...)					RAGE_ERRORF(savegame_photo_mouse,fmt,##__VA_ARGS__)
	#define photomouseWarningf(fmt,...)					RAGE_WARNINGF(savegame_photo_mouse,fmt,##__VA_ARGS__)
	#define photomouseDisplayf(fmt,...)					RAGE_DISPLAYF(savegame_photo_mouse,fmt,##__VA_ARGS__)
	#define photomouseDebugf1(fmt,...)					RAGE_DEBUGF1(savegame_photo_mouse,fmt,##__VA_ARGS__)
	#define photomouseDebugf2(fmt,...)					RAGE_DEBUGF2(savegame_photo_mouse,fmt,##__VA_ARGS__)
	#define photomouseDebugf3(fmt,...)					RAGE_DEBUGF3(savegame_photo_mouse,fmt,##__VA_ARGS__)
#endif	//	RSG_PC

#endif // INC_SAVEGAME_CHANNEL_H 
