//
// vehicles/vehicle_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_VEHICLE_CHANNEL_H
#define INC_VEHICLE_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(vehicle)

#define vehicleAssert(cond)						RAGE_ASSERT(vehicle,cond)
#define vehicleAssertf(cond,fmt,...)			RAGE_ASSERTF(vehicle,cond,fmt,##__VA_ARGS__)
#define vehicleFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(vehicle,cond,fmt,##__VA_ARGS__)
#define vehicleVerifyf(cond,fmt,...)			RAGE_VERIFYF(vehicle,cond,fmt,##__VA_ARGS__)
#define vehicleErrorf(fmt,...)					RAGE_ERRORF(vehicle,fmt,##__VA_ARGS__)
#define vehicleWarningf(fmt,...)				RAGE_WARNINGF(vehicle,fmt,##__VA_ARGS__)
#define vehicleDisplayf(fmt,...)				RAGE_DISPLAYF(vehicle,fmt,##__VA_ARGS__)
#define vehicleDebugf1(fmt,...)					RAGE_DEBUGF1(vehicle,fmt,##__VA_ARGS__)
#define vehicleDebugf2(fmt,...)					RAGE_DEBUGF2(vehicle,fmt,##__VA_ARGS__)
#define vehicleDebugf3(fmt,...)					RAGE_DEBUGF3(vehicle,fmt,##__VA_ARGS__)
#define vehicleLogf(severity,fmt,...)			RAGE_LOGF(vehicle,severity,fmt,##__VA_ARGS__)
#define vehicleCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,vehicle,severity,fmt,##__VA_ARGS__)

//-----------------------------------------------------------------------------------------
RAGE_DECLARE_CHANNEL(vehicledoor)

#define vehicledoorAssert(cond)						RAGE_ASSERT(vehicledoor,cond)
#define vehicledoorAssertf(cond,fmt,...)			RAGE_ASSERTF(vehicledoor,cond,fmt,##__VA_ARGS__)
#define vehicledoorFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(vehicledoor,cond,fmt,##__VA_ARGS__)
#define vehicledoorVerifyf(cond,fmt,...)			RAGE_VERIFYF(vehicledoor,cond,fmt,##__VA_ARGS__)
#define vehicledoorFatalf(fmt,...)					RAGE_FATALF(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorErrorf(fmt,...)					RAGE_ERRORF(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorWarningf(fmt,...)				RAGE_WARNINGF(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorDisplayf(fmt,...)				RAGE_DISPLAYF(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorDebugf1(fmt,...)					RAGE_DEBUGF1(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorDebugf2(fmt,...)					RAGE_DEBUGF2(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorDebugf3(fmt,...)					RAGE_DEBUGF3(vehicledoor,fmt,##__VA_ARGS__)
#define vehicledoorLogf(severity,fmt,...)			RAGE_LOGF(vehicledoor,severity,fmt,##__VA_ARGS__)
#define vehicledoorCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,vehicledoor,severity,fmt,##__VA_ARGS__)

//-----------------------------------------------------------------------------------------
RAGE_DECLARE_CHANNEL(train)

#define trainAssert(cond)						RAGE_ASSERT(train,cond)
#define trainAssertf(cond,fmt,...)				RAGE_ASSERTF(train,cond,fmt,##__VA_ARGS__)
#define trainFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(train,cond,fmt,##__VA_ARGS__)
#define trainVerifyf(cond,fmt,...)				RAGE_VERIFYF(train,cond,fmt,##__VA_ARGS__)
#define trainErrorf(fmt,...)					RAGE_ERRORF(train,fmt,##__VA_ARGS__)
#define trainWarningf(fmt,...)					RAGE_WARNINGF(train,fmt,##__VA_ARGS__)
#define trainDisplayf(fmt,...)					RAGE_DISPLAYF(train,fmt,##__VA_ARGS__)
#define trainDebugf1(fmt,...)					RAGE_DEBUGF1(train,fmt,##__VA_ARGS__)
#define trainDebugf2(fmt,...)					RAGE_DEBUGF2(train,fmt,##__VA_ARGS__)
#define trainDebugf3(fmt,...)					RAGE_DEBUGF3(train,fmt,##__VA_ARGS__)
#define trainLogf(severity,fmt,...)				RAGE_LOGF(train,severity,fmt,##__VA_ARGS__)
#define trainCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,train,severity,fmt,##__VA_ARGS__)
//-----------------------------------------------------------------------------------------

RAGE_DECLARE_CHANNEL(vehicle_reuse)

#define vehicleReuseAssert(cond)						RAGE_ASSERT(vehicle_reuse,cond)
#define vehicleReuseAssertf(cond,fmt,...)			RAGE_ASSERTF(vehicle_reuse,cond,fmt,##__VA_ARGS__)
#define vehicleReuseFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(vehicle_reuse,cond,fmt,##__VA_ARGS__)
#define vehicleReuseVerifyf(cond,fmt,...)			RAGE_VERIFYF(vehicle_reuse,cond,fmt,##__VA_ARGS__)
#define vehicleReuseErrorf(fmt,...)					RAGE_ERRORF(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseWarningf(fmt,...)				RAGE_WARNINGF(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseDisplayf(fmt,...)				RAGE_DISPLAYF(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseDebugf1(fmt,...)					RAGE_DEBUGF1(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseDebugf2(fmt,...)					RAGE_DEBUGF2(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseDebugf3(fmt,...)					RAGE_DEBUGF3(vehicle_reuse,fmt,##__VA_ARGS__)
#define vehicleReuseLogf(severity,fmt,...)			RAGE_LOGF(vehicle_reuse,severity,fmt,##__VA_ARGS__)
#define vehicleReuseCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,vehicle_reuse,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_CHANNEL(boatTrailer)

#define boatTrailerAssert(cond)											RAGE_ASSERT(boatTrailer,cond)
#define boatTrailerAssertf(cond,fmt,...)						RAGE_ASSERTF(boatTrailer,cond,fmt,##__VA_ARGS__)
#define boatTrailerVerifyf(cond,fmt,...)						RAGE_VERIFYF(boatTrailer,cond,fmt,##__VA_ARGS__)
#define boatTrailerErrorf(fmt,...)									RAGE_ERRORF(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerWarningf(fmt,...)								RAGE_WARNINGF(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerDisplayf(fmt,...)								RAGE_DISPLAYF(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerDebugf1(fmt,...)									RAGE_DEBUGF1(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerDebugf2(fmt,...)									RAGE_DEBUGF2(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerDebugf3(fmt,...)									RAGE_DEBUGF3(boatTrailer,fmt,##__VA_ARGS__)
#define boatTrailerLogf(severity,fmt,...)						RAGE_LOGF(boatTrailer,severity,fmt,##__VA_ARGS__)
#define boatTrailerCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,boatTrailer,severity,fmt,##__VA_ARGS__)

#endif // INC_VEHICLE_CHANNEL_H
