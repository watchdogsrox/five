
#include "savegame_data_simple_variables.h"
#include "savegame_data_simple_variables_parser.h"


// Game headers
#include "game/cheat.h"
#include "game/weather.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Peds/PlayerInfo.h"
#include "SaveLoad/GenericGameStorage.h"
#include "system/TamperActions.h"



CSimpleVariablesSaveStructure::CSimpleVariablesSaveStructure()
{
#if ENABLE_FOG_OF_WAR
	u8* fogOfWarLock = CMiniMap_Common::GetFogOfWarLockPtr();
	if(fogOfWarLock)
	{
		m_fogOfWar.Assume(fogOfWarLock,128*128);
	}
#endif // ENABLE_FOG_OF_WAR
}



void CSimpleVariablesSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSimpleVariablesSaveStructure::PreSave"))
	{
		return;
	}

	m_bClosestSaveHouseDataIsValid		= CGenericGameStorage::ms_bClosestSaveHouseDataIsValid;
	m_fHeadingOfClosestSaveHouse		= CGenericGameStorage::ms_fHeadingOfClosestSaveHouse;
	m_vCoordsOfClosestSaveHouse			= CGenericGameStorage::ms_vCoordsOfClosestSaveHouse;
	m_bFadeInAfterLoad					= CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad;
	m_bPlayerShouldSnapToGroundOnSpawn	= CGenericGameStorage::ms_bPlayerShouldSnapToGroundOnSpawn;

	m_MillisecondsPerGameMinute 		= CClock::ms_msPerGameMinute;
	m_LastClockTick 					= CClock::ms_timeLastMinAdded;

	m_GameClockHour						= CClock::GetHour();
	m_GameClockMinute					= CClock::GetMinute();
	m_GameClockSecond					= CClock::GetSecond();

	m_GameClockDay						= CClock::GetDay();
	m_GameClockMonth					= CClock::GetMonth();
	m_GameClockYear						= CClock::GetYear();

	m_moneyCheated						= CCheat::m_MoneyCheated;

#if __PPU
	m_PlayerFlags = ((CLiveManager::GetAchMgr().GetTrophiesEnabled() && CLiveManager::GetAchMgr().GetOwnedSave()) << 1) | (u8(CCheat::m_bHasPlayerCheated == true));
#else
	m_PlayerFlags = CCheat::m_bHasPlayerCheated;
#endif

	m_TimeInMilliseconds 				= fwTimer::GetTimeInMilliseconds();
	m_FrameCounter 						= fwTimer::GetSystemFrameCount();
	m_OldWeatherType 					= g_weather.GetPrevTypeIndex();
	m_NewWeatherType 					= g_weather.GetNextTypeIndex();
	m_ForcedWeatherType 				= g_weather.GetForceTypeIndex();
	m_InterpolationValue 				= g_weather.GetInterp();
	m_WeatherTypeInList 				= g_weather.GetCycleIndex();
	m_Rain				 				= g_weather.GetRain();

	m_MaximumWantedLevel				= CWanted::MaximumWantedLevel;
	m_nMaximumWantedLevel				= CWanted::nMaximumWantedLevel;

	m_NumberOfTimesPickupHelpTextDisplayed = 0;

	m_bHasDisplayedPlayerQuitEnterCarHelpText = CPlayerInfo::GetHasDisplayedPlayerQuitEnterCarHelpText();

	NA_RADIO_ENABLED_ONLY(m_bIncludeLastStationOnSinglePlayerStat = false/*CRadioHud::GetIncludeLastStationOnSinglePlayerStat()*/);

	m_activationDataThing				= TamperActions::GetTriggeredActions();

	u64 sessionId = CNetworkTelemetry::GetSpSessionId();

	m_SpSessionIdHigh = sessionId >> 32;
	m_SpSessionIdLow = (u32) sessionId;

#if ENABLE_FOG_OF_WAR
	// Yeah, it's full. so what ?	
	m_fogOfWar.Resize(128*128);
#endif // ENABLE_FOG_OF_WAR
}

void CSimpleVariablesSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSimpleVariablesSaveStructure::PostLoad"))
	{
		return;
	}

	CGenericGameStorage::ms_bClosestSaveHouseDataIsValid = m_bClosestSaveHouseDataIsValid;
	CGenericGameStorage::ms_fHeadingOfClosestSaveHouse = m_fHeadingOfClosestSaveHouse;
	CGenericGameStorage::ms_vCoordsOfClosestSaveHouse = m_vCoordsOfClosestSaveHouse;
	CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad = m_bFadeInAfterLoad;
	CGenericGameStorage::ms_bPlayerShouldSnapToGroundOnSpawn = m_bPlayerShouldSnapToGroundOnSpawn;

	CClock::SetTime(m_GameClockHour, m_GameClockMinute, m_GameClockSecond);
	CClock::SetDate(m_GameClockDay, static_cast<Date::Month>(m_GameClockMonth), m_GameClockYear);

	//	Fix for B*516684 - time of day not being restored properly when loading a savegame
	//	Ensure that CClock::ms_timeLastMinAdded is set after the call to CClock::SetTime().
	//	CClock::SetTime() leads to CClock::ms_timeLastMinAdded being set with the current game time.
	//	So overwrite CClock::ms_timeLastMinAdded with the value from the savegame file.
	//	This ensures that the variable stays in sync with the game time in milliseconds (which is set using m_TimeInMilliseconds from the savegame file).
	CClock::ms_msPerGameMinute					= m_MillisecondsPerGameMinute;

	// commenting this out goes hand in hand with the commenting out of the timer update
	//	CClock::ms_timeLastMinAdded					= m_LastClockTick;

	CCheat::m_MoneyCheated							= m_moneyCheated;

#if __PPU
	CCheat::m_bHasPlayerCheated 				= (m_PlayerFlags & 0x01) == 0x01;
	CLiveManager::GetAchMgr().SetTrophiesEnabled((m_PlayerFlags & 0x02) == 0x02);
#else
	CCheat::m_bHasPlayerCheated 				= (m_PlayerFlags & 0x01) == 0x01;
#endif

	//fwTimer::SetTimeInMilliseconds(m_TimeInMilliseconds);
	//fwTimer::SetSystemFrameCount(m_FrameCounter);
	g_weather.SetPrevTypeIndex(m_OldWeatherType);
	g_weather.SetNextTypeIndex(m_NewWeatherType);
	g_weather.SetForceTypeIndex(m_ForcedWeatherType);
	g_weather.SetInterp(m_InterpolationValue);
	g_weather.SetCycleIndex(m_WeatherTypeInList);
	g_weather.SetRain(m_Rain); 

	CWanted::MaximumWantedLevel					= m_MaximumWantedLevel;
	CWanted::nMaximumWantedLevel				= m_nMaximumWantedLevel;

	CPlayerInfo::SetHasDisplayedPlayerQuitEnterCarHelpText( m_bHasDisplayedPlayerQuitEnterCarHelpText );

	//NA_RADIO_ENABLED_ONLY(CRadioHud::SetIncludeLastStationOnSinglePlayerStat(m_bIncludeLastStationOnSinglePlayerStat));

	TamperActions::SetTriggeredActionsFromSaveGame(m_activationDataThing);

	u64 sessionId = static_cast<u64>(m_SpSessionIdHigh) << 32;
	sessionId += static_cast<u64>(m_SpSessionIdLow);
	CNetworkTelemetry::SetSpSessionId(sessionId);

#if ENABLE_FOG_OF_WAR
	CMiniMap::SetRequestFoWClear(false);
	CMiniMap::SetFoWMapValid(true);
#if __D3D11
	CMiniMap::SetUploadFoWTextureData(true);
#endif
#endif // ENABLE_FOG_OF_WAR
}

