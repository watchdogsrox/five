
// Rage headers
#include "script\wrapper.h"
// Game headers
#include "game\clock.h"
#include "network\NetworkInterface.h"
#include "script\script.h"
#include "script\script_helper.h"
#include "stats\StatsInterface.h"

//For localtime
#include <time.h>

namespace clock_commands
{

// time commands
void CommandSetClockTime(int hour, int minute, int second)
{
	scriptAssertf(hour>=0 && hour<=23, "script is trying to set the clock with an invalid hour %d", hour);
	scriptAssertf(minute>=0 && minute<=59, "script is trying to set the clock with an invalid minute %d", minute);
	scriptAssertf(second>=0 && second<=59, "script is trying to set the clock with an invalid second %d", second);

	// verify that we're not in a network game
	if(!scriptVerify(!NetworkInterface::IsGameInProgress()))
	{
		scriptErrorf("%s - SET_CLOCK_TIME :: Called in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	scriptAssertf(CClock::GetMode() == CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");
	
	CClock::SetTime(hour, minute, second);
	CClock::SetFromScriptThisFrame(true);
}

int CommandGetClockHours()
{
	int hour = CClock::GetHour();
	scriptAssertf(hour>=0 && hour<=23, "time has an invalid hour %d", hour);
	return hour;
}

int CommandGetClockMinutes()
{
	int minute = CClock::GetMinute();
	scriptAssertf(minute>=0 && minute<=59, "time has an invalid minute %d", minute);
	return minute;
}

int CommandGetClockSeconds()
{
	int second = CClock::GetSecond();
	scriptAssertf(second>=0 && second<=59, "time has an invalid second %d", second);
	return second;
}

void CommandAdvanceClockTimeTo(int hour, int minute, int second)
{
	scriptAssertf(hour>=0 && hour<=23, "script is trying to set the clock with an invalid hour %d", hour);
	scriptAssertf(minute>=0 && minute<=59, "script is trying to set the clock with an invalid minute %d", minute);
	scriptAssertf(second>=0 && second<=59, "script is trying to set the clock with an invalid second %d", second);

	// verify that we're not in a network game
	if(!scriptVerify(!NetworkInterface::IsGameInProgress()))
	{
		scriptErrorf("%s - ADVANCE_CLOCK_TIME_TO :: Called in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	CClock::AdvanceTimeTo(hour, minute, second);
}

void CommandAddToClockTime(int numHours, int numMinutes, int numSeconds)
{
	// verify that we're not in a network game
	if(!scriptVerify(!NetworkInterface::IsGameInProgress()))
	{
		scriptErrorf("%s - ADD_TO_CLOCK_TIME :: Called in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	CClock::AddTime(numHours, numMinutes, numSeconds);
}

void CommandPauseClock(bool pause)
{
	// verify that we're not in a network game
	if(!scriptVerify(!NetworkInterface::IsGameInProgress()))
	{
		scriptErrorf("%s - PAUSE_CLOCK :: Called in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	CClock::ScriptPause(pause);
}

void CommandSimulateAddToClockTime(int numHours, int numMinutes, int numSeconds, 
								   int &ReturnYear, int &ReturnMonth, int &ReturnDay, 
								   int &ReturnHour, int &ReturnMinute, int &ReturnSecond,
								   int &ReturnDayOfWeek)
{
	// verify that we're not in a network game
	if(!scriptVerify(!NetworkInterface::IsGameInProgress()))
	{
		scriptErrorf("%s - SIMULATE_ADD_TO_CLOCK_TIME :: Called in network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}

	DateTime NewCopyOfDateTime;
	CClock::CreateCopyOfDateTimeStruct(NewCopyOfDateTime);

	NewCopyOfDateTime.AddTime(numHours, numMinutes, numSeconds);

	ReturnYear = NewCopyOfDateTime.GetDate().GetYear();
	ReturnMonth = NewCopyOfDateTime.GetDate().GetMonth();
	ReturnDay = NewCopyOfDateTime.GetDate().GetDay();

	ReturnHour = NewCopyOfDateTime.GetTime().GetHours();
	ReturnMinute = NewCopyOfDateTime.GetTime().GetMinutes();
	ReturnSecond = NewCopyOfDateTime.GetTime().GetSeconds();

	ReturnDayOfWeek = NewCopyOfDateTime.GetDate().GetDayOfWeek();
}

void CommandSetClockTimeSample(int idx)
{
	if (scriptVerifyf(!NetworkInterface::IsNetworkOpen(), "Can't change game clock while network session is in progress"))
	{
		Assertf(CClock::GetMode()==CLOCK_MODE_FIXED, "calling a fixed mode function when not in fixed mode");
		CClock::SetTimeSample(idx);
	}
}

// date commands
void CommandSetClockDate(int day, int month, int year)
{
	CClock::SetDate(day, (Date::Month)month, year);
}

int CommandGetClockDayOfWeek()
{
	return CClock::GetDayOfWeek();
}

int CommandGetClockDayOfMonth()
{
	return CClock::GetDay();
}

int CommandGetClockMonth()
{
	return CClock::GetMonth();
}

int CommandGetClockYear()
{
	return CClock::GetYear();
}

s32 CommandGetMillisecondsPerGameMinute()
{
	return CClock::GetMsPerGameMinute();
}

void CommandGetPosixTime(int &year, int &month, int &day, int &hour, int &min, int &sec)
{
	StatsInterface::GetBSTDate(year , month , day , hour , min , sec);

	scriptDebugf1("%s - GET_POSIX_TIME : Retrieve posix time: %d:%d:%d, %d-%d-%d",
		CTheScripts::GetCurrentScriptNameAndProgramCounter(), hour, min, sec, day, month, year);
}

void CommandGetBSTTime(int &year, int &month, int &day, int &hour, int &min, int &sec)
{
	StatsInterface::GetBSTDate(year , month , day , hour , min , sec);
	scriptDebugf1("%s - GET_BST_TIME : Retrieve posix time: %d:%d:%d, %d-%d-%d"
		,CTheScripts::GetCurrentScriptNameAndProgramCounter(), hour, min, sec, day, month, year);
}

void CommandGetUTCTime(int &year, int &month, int &day, int &hour, int &min, int &sec)
{
	StatsInterface::GetPosixDate(year , month , day , hour , min , sec);
	scriptDebugf1("%s - GET_UTC_TIME : Retrieve posix time: %d:%d:%d, %d-%d-%d"
		,CTheScripts::GetCurrentScriptNameAndProgramCounter(), hour, min, sec, day, month, year);
}

void CommandGetLocalTime(int &year, int &month, int &day, int &hour, int &min, int &sec)
{
	year = month = day = hour = min = sec = 0;

	time_t timer;
	time(&timer);
#if RSG_PROSPERO
	struct tm tmBuf;
	struct tm* tm = localtime_s(&timer, &tmBuf);
#else
	struct tm* tm = localtime(&timer);
#endif
	if(tm)
	{
		year  = tm->tm_year+1900;
		month = tm->tm_mon+1;
		day   = tm->tm_mday;
		hour  = tm->tm_hour;
		min   = tm->tm_min;
		sec   = tm->tm_sec;
	}
}

void SetupScriptCommands()
{
	// time commands
	SCR_REGISTER_UNUSED(SET_CLOCK_TIME_SAMPLE,0xc3c632ebdc61bd08, CommandSetClockTimeSample);
	SCR_REGISTER_SECURE(SET_CLOCK_TIME,0xaa27c85e5be092aa, CommandSetClockTime);
	SCR_REGISTER_SECURE(PAUSE_CLOCK,0xbc9595cb59db2217, CommandPauseClock);
	SCR_REGISTER_SECURE(ADVANCE_CLOCK_TIME_TO,0x642ca1dd83a8a0a9, CommandAdvanceClockTimeTo);
	SCR_REGISTER_SECURE(ADD_TO_CLOCK_TIME,0xb60bfb8af972b257, CommandAddToClockTime);
	SCR_REGISTER_UNUSED(SIMULATE_ADD_TO_CLOCK_TIME,0xd5eaff55e354b01d, CommandSimulateAddToClockTime);
	SCR_REGISTER_SECURE(GET_CLOCK_HOURS,0x09fc827691dace59, CommandGetClockHours);
	SCR_REGISTER_SECURE(GET_CLOCK_MINUTES,0x80f97d7d29800a1a, CommandGetClockMinutes);
	SCR_REGISTER_SECURE(GET_CLOCK_SECONDS,0xaa2844cad88768b5, CommandGetClockSeconds);

	// date commands
	SCR_REGISTER_SECURE(SET_CLOCK_DATE,0xf9352288b8dec888, CommandSetClockDate);
	SCR_REGISTER_SECURE(GET_CLOCK_DAY_OF_WEEK,0x4bfee961f7bc72b6, CommandGetClockDayOfWeek);
	SCR_REGISTER_SECURE(GET_CLOCK_DAY_OF_MONTH,0xd52bd97e61483713, CommandGetClockDayOfMonth);
	SCR_REGISTER_SECURE(GET_CLOCK_MONTH,0x771485043fdc55de, CommandGetClockMonth);
	SCR_REGISTER_SECURE(GET_CLOCK_YEAR,0x1137fd08b8d3f874, CommandGetClockYear);
	SCR_REGISTER_SECURE(GET_MILLISECONDS_PER_GAME_MINUTE,0x4854cc9bef210e7c, CommandGetMillisecondsPerGameMinute);

	// Reliable date on xbox/ps3
	SCR_REGISTER_SECURE(GET_POSIX_TIME,0xa7f20303a35ca1bc,  CommandGetPosixTime);
	SCR_REGISTER_UNUSED(GET_BST_TIME,0xda27a7b28d93cc45,  CommandGetBSTTime);
	SCR_REGISTER_SECURE(GET_UTC_TIME,0x735e0c24346de543,  CommandGetUTCTime);
	SCR_REGISTER_SECURE(GET_LOCAL_TIME,0x0ef5eeafca4d8956,  CommandGetLocalTime);
}


} // namespace clock_commands
