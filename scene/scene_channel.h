//
// scene/scene_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SCENE_CHANNEL_H
#define INC_SCENE_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(scene)

#define sceneAssert(cond)							RAGE_ASSERT(scene,cond)
#define sceneAssertf(cond,fmt,...)					RAGE_ASSERTF(scene,cond,fmt,##__VA_ARGS__)
#define sceneAssertBefore(year,month,day)			RAGE_ASSERT_BEFORE(scene,year,month,day)
#define	sceneAssertBeforef(year,month,day,fmt,...)	RAGE_ASSERT_BEFOREF(scene,year,month,day,fmt,##__VA_ARGS__)
#define sceneVerifyf(cond,fmt,...)					RAGE_VERIFYF(scene,cond,fmt,##__VA_ARGS__)
#define sceneErrorf(fmt,...)						RAGE_ERRORF(scene,fmt,##__VA_ARGS__)
#define sceneWarningf(fmt,...)						RAGE_WARNINGF(scene,fmt,##__VA_ARGS__)
#define sceneDisplayf(fmt,...)						RAGE_DISPLAYF(scene,fmt,##__VA_ARGS__)
#define sceneDebugf1(fmt,...)						RAGE_DEBUGF1(scene,fmt,##__VA_ARGS__)
#define sceneDebugf2(fmt,...)						RAGE_DEBUGF2(scene,fmt,##__VA_ARGS__)
#define sceneDebugf3(fmt,...)						RAGE_DEBUGF3(scene,fmt,##__VA_ARGS__)
#define sceneLogf(severity,fmt,...)					RAGE_LOGF(scene,severity,fmt,##__VA_ARGS__)
#define sceneCondLogf(cond,severity,fmt,...)		RAGE_CONDLOGF(cond,scene,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(scene, world)

#define worldAssert(cond)						RAGE_ASSERT(scene_world,cond)
#define worldAssertf(cond,fmt,...)				RAGE_ASSERTF(scene_world,cond,fmt,##__VA_ARGS__)
#define worldVerifyf(cond,fmt,...)				RAGE_VERIFYF(scene_world,cond,fmt,##__VA_ARGS__)
#define worldErrorf(fmt,...)					RAGE_ERRORF(scene_world,fmt,##__VA_ARGS__)
#define worldWarningf(fmt,...)					RAGE_WARNINGF(scene_world,fmt,##__VA_ARGS__)
#define worldDisplayf(fmt,...)					RAGE_DISPLAYF(scene_world,fmt,##__VA_ARGS__)
#define worldDebugf1(fmt,...)					RAGE_DEBUGF1(scene_world,fmt,##__VA_ARGS__)
#define worldDebugf2(fmt,...)					RAGE_DEBUGF2(scene_world,fmt,##__VA_ARGS__)
#define worldDebugf3(fmt,...)					RAGE_DEBUGF3(scene_world,fmt,##__VA_ARGS__)
#define worldLogf(severity,fmt,...)				RAGE_LOGF(scene_world,severity,fmt,##__VA_ARGS__)
#define worldCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,scene_world,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(scene, visibility)

#define visibilityAssert(cond)						RAGE_ASSERT(scene_visibility,cond)
#define visibilityAssertf(cond,fmt,...)				RAGE_ASSERTF(scene_visibility,cond,fmt,##__VA_ARGS__)
#define visibilityVerifyf(cond,fmt,...)				RAGE_VERIFYF(scene_visibility,cond,fmt,##__VA_ARGS__)
#define visibilityErrorf(fmt,...)					RAGE_ERRORF(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityWarningf(fmt,...)					RAGE_WARNINGF(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityDisplayf(fmt,...)					RAGE_DISPLAYF(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityDebugf1(fmt,...)					RAGE_DEBUGF1(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityDebugf2(fmt,...)					RAGE_DEBUGF2(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityDebugf3(fmt,...)					RAGE_DEBUGF3(scene_visibility,fmt,##__VA_ARGS__)
#define visibilityLogf(severity,fmt,...)			RAGE_LOGF(scene_visibility,severity,fmt,##__VA_ARGS__)
#define visibilityCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,scene_visibility,severity,fmt,##__VA_ARGS__)

#if __NO_OUTPUT
#define visibilityDebugf1StackTrace()				do {} while(false)
#define worldDebugf1StackTrace()					do {} while(false)
#else
extern void visibilityDebugf1StackTraceImpl();
extern void worldDebugf1StackTraceImpl();
__forceinline void visibilityDebugf1StackTrace()	{ if ( Channel_scene_visibility.FileLevel >= DIAG_SEVERITY_DEBUG1 ) visibilityDebugf1StackTraceImpl(); }
__forceinline void worldDebugf1StackTrace()			{ if ( Channel_scene_world.FileLevel >= DIAG_SEVERITY_DEBUG1 ) worldDebugf1StackTraceImpl(); }
#endif


#endif // INC_SCENE_CHANNEL_H
