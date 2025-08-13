//
// camera/camera_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_CAMERA_CHANNEL_H
#define INC_CAMERA_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(camera)

#define cameraAssert(cond)						RAGE_ASSERT(camera,cond)
#define cameraAssertf(cond,fmt,...)				RAGE_ASSERTF(camera,cond,fmt,##__VA_ARGS__)
#define cameraFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(camera,cond,fmt,##__VA_ARGS__)
#define cameraVerifyf(cond,fmt,...)				RAGE_VERIFYF(camera,cond,fmt,##__VA_ARGS__)
#define cameraErrorf(fmt,...)					RAGE_ERRORF(camera,fmt,##__VA_ARGS__)
#define cameraWarningf(fmt,...)					RAGE_WARNINGF(camera,fmt,##__VA_ARGS__)
#define cameraDisplayf(fmt,...)					RAGE_DISPLAYF(camera,fmt,##__VA_ARGS__)
#define cameraDebugf1(fmt,...)					RAGE_DEBUGF1(camera,fmt,##__VA_ARGS__)
#define cameraDebugf2(fmt,...)					RAGE_DEBUGF2(camera,fmt,##__VA_ARGS__)
#define cameraDebugf3(fmt,...)					RAGE_DEBUGF3(camera,fmt,##__VA_ARGS__)
#define cameraLogf(severity,fmt,...)			RAGE_LOGF(camera,severity,fmt,##__VA_ARGS__)
#define cameraCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,camera,severity,fmt,##__VA_ARGS__)


#endif // INC_CAMERA_CHANNEL_H
