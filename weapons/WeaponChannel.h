//
// weapons/weaponchannel.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_CHANNEL_H
#define WEAPON_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(weapon)

#define weaponAssert(cond)						RAGE_ASSERT(weapon,cond)
#define weaponAssertf(cond,fmt,...)				RAGE_ASSERTF(weapon,cond,fmt,##__VA_ARGS__)
#define weaponFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(weapon,cond,fmt,##__VA_ARGS__)
#define weaponVerifyf(cond,fmt,...)				RAGE_VERIFYF(weapon,cond,fmt,##__VA_ARGS__)
#define weaponErrorf(fmt,...)					RAGE_ERRORF(weapon,fmt,##__VA_ARGS__)
#define weaponWarningf(fmt,...)					RAGE_WARNINGF(weapon,fmt,##__VA_ARGS__)
#define weaponDisplayf(fmt,...)					RAGE_DISPLAYF(weapon,fmt,##__VA_ARGS__)
#define weaponDebugf1(fmt,...)					RAGE_DEBUGF1(weapon,fmt,##__VA_ARGS__)
#define weaponDebugf2(fmt,...)					RAGE_DEBUGF2(weapon,fmt,##__VA_ARGS__)
#define weaponDebugf3(fmt,...)					RAGE_DEBUGF3(weapon,fmt,##__VA_ARGS__)
#define weaponLogf(severity,fmt,...)			RAGE_LOGF(weapon,severity,fmt,##__VA_ARGS__)
#define weaponCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,weapon,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_CHANNEL(weaponitem)

#define weaponitemAssert(cond)						RAGE_ASSERT(weaponitem,cond)
#define weaponitemAssertf(cond,fmt,...)				RAGE_ASSERTF(weaponitem,cond,fmt,##__VA_ARGS__)
#define weaponitemFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(weaponitem,cond,fmt,##__VA_ARGS__)
#define weaponitemVerifyf(cond,fmt,...)				RAGE_VERIFYF(weaponitem,cond,fmt,##__VA_ARGS__)
#define weaponitemErrorf(fmt,...)					RAGE_ERRORF(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemWarningf(fmt,...)					RAGE_WARNINGF(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemDisplayf(fmt,...)					RAGE_DISPLAYF(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemDebugf1(fmt,...)					RAGE_DEBUGF1(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemDebugf2(fmt,...)					RAGE_DEBUGF2(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemDebugf3(fmt,...)					RAGE_DEBUGF3(weaponitem,fmt,##__VA_ARGS__)
#define weaponitemLogf(severity,fmt,...)			RAGE_LOGF(weaponitem,severity,fmt,##__VA_ARGS__)
#define weaponitemCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,weaponitem,severity,fmt,##__VA_ARGS__)

#endif // WEAPON_CHANNEL_H
