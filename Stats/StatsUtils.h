//
// StatsUtils.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef _STATSUTILS_H_
#define _STATSUTILS_H_

// rage headers

// Game headers
#include "Stats/StatsTypes.h"
#include "text/text.h"

#define STAT_LINE_TYPE_0 0
#define STAT_LINE_TYPE_1 1
#define STAT_LINE_TYPE_2 2

class CStatsUtils
{
	// CStatsMgr can access private part
	friend class CStatsMgr;

public:

	//PURPOSE: Creates a string with the needed values to be displayed for the frontend.
	static void CreateFrontendStatLine(char* buffer, s32 listId, s32 statPosInList);

	//PURPOSE: Given the time in milliseconds return a string DD:HH:MM:SS
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static const char* GetTime(s32 iTimeInMilliseconds, char* outBuffer, u32 bufferSize, bool bStatsScreenFormatting = false);

	static const char* GetTime(u64 iTimeInMilliseconds, char* outBuffer, u32 bufferSize, bool bStatsScreenFormatting = false);

	//PURPOSE: Given the time in milliseconds return the days.
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static s32 ConvertToDays(u64 iTimeInMilliseconds);

	//PURPOSE: Given the time in milliseconds return the hours.
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static s32 ConvertToHours(u64 iTimeInMilliseconds);

	//PURPOSE: Given the time in milliseconds return the mins.
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static s32 ConvertToMins(u64 iTimeInMilliseconds);

	//PURPOSE: Given the time in milliseconds return the seconds.
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static s32 ConvertToSeconds(u64 iTimeInMilliseconds);

	//PURPOSE: Given the time in milliseconds return the milliseconds.
	//PARAMS:  iTimeInMilliseconds - Time in milliseconds.
	static s32 ConvertToMilliseconds(u64 iTimeInMilliseconds);


	//PURPOSE:  Creates one string line with the stat value and the stat text.
	static void CreateOneStatLine(char* buffer, const StatId& keyHash, const int statType);

	//PURPOSE
	//  If we need to disable the tracking of the stats for a while
	static bool IsStatsTrackingEnabled();
	static void EnableStatsTracking(bool val=true);
	static void DisableStatsTracking();
	// resets the tracking to its default value, true
	static void ResetStatsTracking();
	//PURPOSE
	//  If there is any restriction to update the stats should be defined here.
	static bool CanUpdateStats( );

private:

	//PURPOSE
	//  Creates a stat line. 
	//  Returns the stat label in pStatLabel and the stat value in pStatValueString.
	//PARAMS
	//  pLabel           - stat label.
	//  pStatLabel       - stat label output.
	//  pStatValueString - stat string value output.
	//  iStatID1         - ID of the first stat value or -1.
	//  statType        - stat type (STAT_TYPE_INT .. )
	//  iStatID2         - ID of the Second stat value or -1.
	//  iType            - Can be 0, 1 or 2 -  each number represents.
	static void BuildStatLine(const char* pLabel, 
							  char* pStatLabel, 
							  char* pStatValueString, 
							  const StatId keyHash1 = StatId(),
							  const int statType1 = STAT_TYPE_FLOAT,
							  const StatId keyHash2 = StatId(),
							  const int statType2 = STAT_TYPE_FLOAT,
							  const int type = 0);

	//PURPOSE
	//  Works out whether we hit the threshold and return true if we do
	//  but also store the 'old' value which needs to be passed in
	//  making the function as generic as possible
	static bool CheckForThreshold(float *stored_value, float new_value);

	//PURPOSE
	//  Checks whether the stat is a combination. If true then make the necessary checks to validate it.
	//  Return true if the stat is not a combination of 2 or more stats.
	static bool CheckStatsForCombination(const StatId& keyHash);

	//PURPOSE
	//  Checks whether its ok to show this stat in this Language version.
	static bool SafeToShowThisStatInTheCurrentLanguage(const StatId& keyHash);

	//PURPOSE
	//  Returns whether this stat is limited to 1000
	static bool IsStatCapped(const StatId& keyHash);



	static float  GetLanguageMetricValue(const StatId& keyHash);
	static float  GetFrenchLanguageMetricValue(const StatId& keyHash);
	static float  GetGermanLanguageMetricValue(const StatId& keyHash);
	static float  GetItalianLanguageMetricValue(const StatId& keyHash);
	static float  GetSpanishLanguageMetricValue(const StatId& keyHash);
	static float  GetJapaneseLanguageMetricValue(const StatId& keyHash);
	static float  GetDefaultLanguageMetricValue(const StatId& keyHash);

	// If the tracking of the stats should be enabled.
	// It can be useful to turn it off during some specific missions, ex: B*1955817
	static bool sm_enabled;
};

#endif

// EOF

