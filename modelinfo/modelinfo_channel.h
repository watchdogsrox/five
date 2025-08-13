//
// modelinfo/modelinfo_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_MODELINFO_CHANNEL_H
#define INC_MODELINFO_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(modelinfo)

#define modelinfoAssert(cond)						RAGE_ASSERT(modelinfo,cond)
#define modelinfoAssertf(cond,fmt,...)				RAGE_ASSERTF(modelinfo,cond,fmt,##__VA_ARGS__)
#define modelinfoFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(modelinfo,cond,fmt,##__VA_ARGS__)
#define modelinfoVerifyf(cond,fmt,...)				RAGE_VERIFYF(modelinfo,cond,fmt,##__VA_ARGS__)
#define modelinfoErrorf(fmt,...)					RAGE_ERRORF(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoWarningf(fmt,...)					RAGE_WARNINGF(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoDisplayf(fmt,...)					RAGE_DISPLAYF(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoDebugf1(fmt,...)					RAGE_DEBUGF1(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoDebugf2(fmt,...)					RAGE_DEBUGF2(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoDebugf3(fmt,...)					RAGE_DEBUGF3(modelinfo,fmt,##__VA_ARGS__)
#define modelinfoLogf(severity,fmt,...)				RAGE_LOGF(modelinfo,severity,fmt,##__VA_ARGS__)
#define modelinfoCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,modelinfo,severity,fmt,##__VA_ARGS__)

#endif // INC_MODELINFO_CHANNEL_H
