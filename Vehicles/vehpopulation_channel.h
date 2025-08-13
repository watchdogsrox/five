//
// vehicles/vehpopulation_channel.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_VEHPOPULATION_CHANNEL_H
#define INC_VEHPOPULATION_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(vehicle_population)

#define vehPopAssert(cond)						RAGE_ASSERT(vehicle_population,cond)
#define vehPopAssertf(cond,fmt,...)				RAGE_ASSERTF(vehicle_population,cond,fmt,##__VA_ARGS__)
#define vehPopVerifyf(cond,fmt,...)				RAGE_VERIFYF(vehicle_population,cond,fmt,##__VA_ARGS__)
#define vehPopErrorf(fmt,...)					RAGE_ERRORF(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopWarningf(fmt,...)					RAGE_WARNINGF(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopDisplayf(fmt,...)					RAGE_DISPLAYF(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopDebugf1(fmt,...)					RAGE_DEBUGF1(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopDebugf2(fmt,...)					RAGE_DEBUGF2(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopDebugf3(fmt,...)					RAGE_DEBUGF3(vehicle_population,fmt,##__VA_ARGS__)
#define vehPopLogf(severity,fmt,...)			RAGE_LOGF(vehicle_population,severity,fmt,##__VA_ARGS__)
#define vehPopCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,vehicle_population,severity,fmt,##__VA_ARGS__)


#endif // INC_VEHPOPULATION_CHANNEL_H
