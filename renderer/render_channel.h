//
// render/render_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_RENDER_CHANNEL_H
#define INC_RENDER_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(render)

#define renderAssert(cond)						RAGE_ASSERT(render,cond)
#define renderAssertf(cond,fmt,...)				RAGE_ASSERTF(render,cond,fmt,##__VA_ARGS__)
#define renderVerifyf(cond,fmt,...)				RAGE_VERIFYF(render,cond,fmt,##__VA_ARGS__)
#define renderErrorf(fmt,...)					RAGE_ERRORF(render,fmt,##__VA_ARGS__)
#define renderWarningf(fmt,...)					RAGE_WARNINGF(render,fmt,##__VA_ARGS__)
#define renderDisplayf(fmt,...)					RAGE_DISPLAYF(render,fmt,##__VA_ARGS__)
#define renderDebugf1(fmt,...)					RAGE_DEBUGF1(render,fmt,##__VA_ARGS__)
#define renderDebugf2(fmt,...)					RAGE_DEBUGF2(render,fmt,##__VA_ARGS__)
#define renderDebugf3(fmt,...)					RAGE_DEBUGF3(render,fmt,##__VA_ARGS__)
#define renderLogf(severity,fmt,...)			RAGE_LOGF(render,severity,fmt,##__VA_ARGS__)
#define renderCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,render,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(render, drawlist)

#define drawListAssert(cond)					RAGE_ASSERT(render_drawlist,cond)
#define drawListAssertf(cond,fmt,...)			RAGE_ASSERTF(render_drawlist,cond,fmt,##__VA_ARGS__)
#define drawListVerifyf(cond,fmt,...)			RAGE_VERIFYF(render_drawlist,cond,fmt,##__VA_ARGS__)
#define drawListErrorf(fmt,...)					RAGE_ERRORF(render_drawlist,fmt,##__VA_ARGS__)
#define drawListWarningf(fmt,...)				RAGE_WARNINGF(render_drawlist,fmt,##__VA_ARGS__)
#define drawListDisplayf(fmt,...)				RAGE_DISPLAYF(render_drawlist,fmt,##__VA_ARGS__)
#define drawListDebugf1(fmt,...)				RAGE_DEBUGF1(render_drawlist,fmt,##__VA_ARGS__)
#define drawListDebugf2(fmt,...)				RAGE_DEBUGF2(render_drawlist,fmt,##__VA_ARGS__)
#define drawListDebugf3(fmt,...)				RAGE_DEBUGF3(render_drawlist,fmt,##__VA_ARGS__)
#define drawListLogf(severity,fmt,...)			RAGE_LOGF(render_drawlist,severity,fmt,##__VA_ARGS__)
#define drawListCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,render_drawlist,severity,fmt,##__VA_ARGS__)


#endif // INC_RENDER_CHANNEL_H
