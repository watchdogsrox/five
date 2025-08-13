#ifndef SAVEGAME_DATA_SIMPLE_VARIABLES_H
#define SAVEGAME_DATA_SIMPLE_VARIABLES_H

// Rage headers
#include "parser/macros.h"

// Game headers
#include "game/Wanted.h"

class CSimpleVariablesSaveStructure
{
public:
	CSimpleVariablesSaveStructure();

	bool		m_bClosestSaveHouseDataIsValid;
	float		m_fHeadingOfClosestSaveHouse;
	Vector3		m_vCoordsOfClosestSaveHouse;
	bool		m_bFadeInAfterLoad;
	bool		m_bPlayerShouldSnapToGroundOnSpawn;

	u32			m_MillisecondsPerGameMinute;
	u32			m_LastClockTick;
	//	u32			m_GameClockMonths;
	//	u32			m_GameClockDays;
	//	u32			m_GameClockHours;
	//	u32			m_GameClockMinutes;
	//	u32			m_CurrentDay;

	u32			m_GameClockHour;		// hours since midnight - [0,23]
	u32			m_GameClockMinute;		// minutes after the hour - [0,59]
	u32			m_GameClockSecond;		// seconds after the minute - [0,59]

	u32			m_GameClockDay;			// day of the month - [1,31]
	u32			m_GameClockMonth;  		// months since January - [0,11]
	u32			m_GameClockYear;		// year

	u32			m_moneyCheated;
	u8			m_PlayerFlags;
	u32			m_TimeInMilliseconds;
	u32			m_FrameCounter;
	s32			m_OldWeatherType;
	s32			m_NewWeatherType;
	s32			m_ForcedWeatherType;
	float			m_InterpolationValue;
	s32			m_WeatherTypeInList;
	float			m_Rain;

	eWantedLevel 	m_MaximumWantedLevel;
	s32			m_nMaximumWantedLevel;

	s32				m_NumberOfTimesPickupHelpTextDisplayed;

	bool 			m_bHasDisplayedPlayerQuitEnterCarHelpText;

	bool            m_bIncludeLastStationOnSinglePlayerStat;

	// Only actually used on PC, contains triggered tamper actions - renamed for obfuscation
	u32             m_activationDataThing;

	//Id of saved single player session.
	u32             m_SpSessionIdHigh;
	u32             m_SpSessionIdLow;

	atUserArray<u8>		m_fogOfWar;

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE;
};

#endif // SAVEGAME_DATA_SIMPLE_VARIABLES_H

