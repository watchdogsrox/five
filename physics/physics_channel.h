//
// physics/physics_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_PHYSICS_CHANNEL_H
#define INC_PHYSICS_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(physics)

#define physicsAssert(cond)						RAGE_ASSERT(physics,cond)
#define physicsAssertf(cond,fmt,...)			RAGE_ASSERTF(physics,cond,fmt,##__VA_ARGS__)
#define physicsFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(physics,cond,fmt,##__VA_ARGS__)
#define physicsVerifyf(cond,fmt,...)			RAGE_VERIFYF(physics,cond,fmt,##__VA_ARGS__)
#define physicsErrorf(fmt,...)					RAGE_ERRORF(physics,fmt,##__VA_ARGS__)
#define physicsWarningf(fmt,...)				RAGE_WARNINGF(physics,fmt,##__VA_ARGS__)
#define physicsDisplayf(fmt,...)				RAGE_DISPLAYF(physics,fmt,##__VA_ARGS__)
#define physicsDebugf1(fmt,...)					RAGE_DEBUGF1(physics,fmt,##__VA_ARGS__)
#define physicsDebugf2(fmt,...)					RAGE_DEBUGF2(physics,fmt,##__VA_ARGS__)
#define physicsDebugf3(fmt,...)					RAGE_DEBUGF3(physics,fmt,##__VA_ARGS__)
#define physicsLogf(severity,fmt,...)			RAGE_LOGF(physics,severity,fmt,##__VA_ARGS__)
#define physicsCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,physics,severity,fmt,##__VA_ARGS__)


#endif // INC_PHYSICS_CHANNEL_H
