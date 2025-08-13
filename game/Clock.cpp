///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Clock.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	16 October 2009
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// us
#include "Clock.h" 

// rage
#include "net/timesync.h"
#include "parser/manager.h"

// framework
#include "fwdebug\debugdraw.h"
#include "fwsys/timer.h"

// game
#include "Core/Game.h"
#include "Debug\DebugGlobals.h"
#include "Debug\MarketingTools.h"
#include "Network\Events\NetworkEventTypes.h"
#include "Network\NetworkInterface.h"
#include "Scene\DataFileMgr.h"
#include "System\ControlMgr.h"
#include "Shaders\ShaderLib.h"
#include "TimeCycle\TimeCycle.h"

// game (for ProcessTimeSkip command - doesn't really belong in here)
#include "Audio\PoliceScanner.h"
#include "Audio\RadioAudioEntity.h"
#include "peds/Ped.h"
#include "Renderer\PostProcessFx.h"
#include "Scene\World\GameWorld.h"
#include "control\replay\ReplaySettings.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS
///////////////////////////////////////////////////////////////////////////////

NETWORK_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  STATIC VARIABLES
///////////////////////////////////////////////////////////////////////////////

// date info
int				Date::ms_daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const char*			Date::ms_monthsOfYearNames[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
const char*			Date::ms_daysOfWeekNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// Set this frame flag
bool			CClock::ms_bClockSetThisFrame;

// settings
ClockMode_e		CClock::ms_mode;

s32 CClock::ms_numTimeSamples;
TimeSample_s	CClock::ms_timeSampleInfo[CLOCK_MAX_TIME_SAMPLES];

u32				CClock::ms_msPerGameMinute;

// state
DateTime		CClock::ms_dateTime;

// dynamic mode specific
u32				CClock::ms_timeLastMinAdded;						// time at which the last minute was added to the clock
u32				CClock::ms_timeSinceLastMinAdded;					// the difference between the current time and the time the last minute was added

// fixed mode specific
s32				CClock::ms_currTimeSample;

// misc
bool			CClock::ms_isPaused = false;
bool			CClock::ms_isPausedByScript = false;

// debug / network
#if !__FINAL
bool			CClock::ms_DebugModifiedInNetworkGame = false;
#endif

s32				CClock::ms_PreviousInGameTimeInSeconds = 0;
float			CClock::ms_ApproxTimeStepMS = 0.0f;

// If you change this tell Immy
static const unsigned MS_PER_GAME_MINUTE = 2000;

#if __BANK
bool			CClock::ms_debugOutput;
float			CClock::m_timeSpeedMult;
bool			CClock::m_overrideTime;
int				CClock::m_overrideTimeVal;
bool			CClock::ms_OverrideGameRate = false;
u32				CClock::ms_OverridenMsPerGameMinute = MS_PER_GAME_MINUTE;

s32				CClock::ms_debugDayOfMonthToSet = 1;
Date::Month		CClock::ms_debugMonthToSet = Date::JANUARY;
s32				CClock::ms_debugYearToSet = 2011;

#endif

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS Time
///////////////////////////////////////////////////////////////////////////////

Time::Time()
{
	Set(0, 0, 0);
}

Time::Time(int hour, int minute, int second)
{
	if (!IsValid(hour, minute, second))
	{
		Assertf(0, "Invalid time set");
	}

	m_hour = hour;
	m_min = minute;
	m_sec = second;

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);
}

void Time::Set(const Time& time)
{
	Set(time.m_hour, time.m_min, time.m_sec);

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);
}

void Time::Set(int hour, int minute, int second)
{
	if (!IsValid(hour, minute, second))
	{
		Assertf(0, "Invalid time set");
	}

	m_hour = hour;
	m_min = minute;
	m_sec = second;

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);
}

int Time::Add(int numHours, int numMinutes, int numSeconds)
{
	int dayCount = 0;
	dayCount += AddSecond(numSeconds);
	dayCount += AddMinute(numMinutes);
	dayCount += AddHour(numHours);

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);

	return dayCount;
}

int Time::Add(const Time& time)
{
	int dayCount = Add(time.m_hour, time.m_min, time.m_sec);

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);

	return dayCount;
}

int Time::AddHour(int numHours)
{
	m_hour += numHours;

	int dayCount = 0;
	while (m_hour>=24)
	{
		m_hour-=24;
		dayCount++;
	}
	while (m_hour<0)
	{
		m_hour+=24;
		dayCount--;
	}

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);

	return dayCount;
}

int Time::AddMinute(int numMinutes)
{
	m_min += numMinutes;

	int dayCount = 0;
	while (m_min>=60)
	{
		m_min-=60;
		dayCount += AddHour(1);
	}
	while (m_min<0)
	{
		m_min+=60;
		dayCount += AddHour(-1);
	}

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);

	return dayCount;
}

int Time::AddSecond(int numSeconds)
{
	m_sec += numSeconds;

	int dayCount = 0;
	while (m_sec>=60)
	{
		m_sec-=60;
		dayCount += AddMinute(1);
	}
	while (m_sec<0)
	{
		m_sec+=60;
		dayCount += AddMinute(-1);
	}

	Assertf(m_hour>=0 && m_hour<=23, "time has an invalid hour %d", m_hour);
	Assertf(m_min>=0 && m_min<=59, "time has an invalid minute %d", m_min);
	Assertf(m_sec>=0 && m_sec<=59, "time has an invalid second %d", m_sec);

	return dayCount;
}

int Time::GetInMinutes() const
{
	return (m_hour*(60)) + m_min;
}

int Time::GetInSeconds() const
{
	return (m_hour*(60*60)) + (m_min*60) + m_sec;
}

bool Time::GetTimeBetween(Time& tRes, const Time& t1, const Time& t2)
{
	int t1Seconds = t1.GetInSeconds();
	int t2Seconds = t2.GetInSeconds();

	bool dayPassed = false;
	int secondsBetween;
	if (t2Seconds>=t1Seconds)
	{
		secondsBetween = t2Seconds-t1Seconds;
	}
	else
	{
		secondsBetween = (SECONDS_IN_DAY-t1Seconds) + t2Seconds; 
		dayPassed = true;
	}

	tRes.Set(0, 0, 0);
	tRes.Add(0, 0, secondsBetween);

	return dayPassed;
}

bool Time::IsValid(int hour, int minute, int second) const
{
	return hour>=0 && 
		hour<24 && 
		minute>=0 && 
		minute<60 && 
		second>=0 && 
		second<60;
}




///////////////////////////////////////////////////////////////////////////////
//  CLASS Date
///////////////////////////////////////////////////////////////////////////////

Date::Date()
{
	Set(1, JANUARY, 2000);
}

Date::Date(int day, Month month, int year)
{
	Set(day, month, year);
}

void Date::Set(const Date& date)
{
	Set(date.m_day, date.m_month, date.m_year);
}

void Date::Set(int day, Month month, int year)
{
	if (!IsValid(day, month, year))
	{
		Assertf(0, "Invalid date set");
	}

	m_day = day;
	m_month = month;
	m_year = year;

	m_wday = CalcDayOfWeek(m_day, m_month, m_year);
}

void Date::Add(int numDays)
{
	AddDay(numDays);
	m_wday = CalcDayOfWeek(m_day, m_month, m_year);
}

void Date::AddDay(int numDays)
{
	m_day += numDays;

	int daysInMonth = GetDaysInMonth(m_month, m_year);
	while (m_day>daysInMonth)
	{
		m_day-=daysInMonth;
		AddMonth(1);
		daysInMonth = GetDaysInMonth(m_month, m_year);
	}
	while (m_day<=0)
	{
		Month prevMonth;
		int prevMonthsYear;
		GetPrevMonth(prevMonth, prevMonthsYear, m_month, m_year);

		int daysInPreviousMonth = GetDaysInMonth(prevMonth, prevMonthsYear);
		m_day+=daysInPreviousMonth;
		AddMonth(-1);
	}
}

void Date::AddMonth(int numMonths)
{
	m_month = (Month)(m_month+numMonths);
	while (m_month>DECEMBER)
	{
		m_month = (Month)(m_month-12);
		AddYear(1);
	}
	while (m_month<0)
	{
		m_month = (Month)(m_month+12);
		AddYear(-1);
	}
}

void Date::AddYear(int numYears)
{
	m_year += numYears;
}

int Date::GetDaysSince1900() const
{
	int daysSince1900 = 0;
	for (int i=1900; i<m_year; i++)
	{
		daysSince1900 += DAYS_IN_NON_LEAP_YEAR + IsLeapYear(i);
	}
	daysSince1900 += GetDayOfYear();
	return daysSince1900;
}

int Date::GetDaysSince(int year) const
{
	int daysSince = 0;
	for (int i=year; i<m_year; i++)
	{
		daysSince += DAYS_IN_NON_LEAP_YEAR + IsLeapYear(i);
	}
	daysSince += GetDayOfYear();
	return daysSince;
}

int Date::GetDayOfYear() const
{
	int dayOfYear = 0;
	for (int i=0; i<m_month; i++)
	{
		dayOfYear += GetDaysInMonth((Month)i, m_year);
	}
	return dayOfYear+m_day;
}

int Date::GetDaysBetween(const Date& d1, const Date& d2)
{
	int d1DaysSince1900 = d1.GetDaysSince1900();
	int d2DaysSince1900 = d2.GetDaysSince1900();

	return d2DaysSince1900-d1DaysSince1900;
}

int Date::GetDaysInMonth(Month month, int year)
{
	if (month==FEBRUARY && IsLeapYear(year))
	{
		return 29;
	}

	return ms_daysInMonth[month];
}

void Date::GetPrevMonth(Month& prevMonth, int& prevMonthsYear, Month month, int year)
{
	if (month==JANUARY)
	{
		prevMonth = DECEMBER;
		prevMonthsYear = year-1;
	}
	else
	{
		prevMonth = (Month)(month-1);
		prevMonthsYear = year;
	}
}

bool Date::IsLeapYear(int year)
{
	return (((year%4==0) && (year%100!=0)) || year%400==0);
}

Date::DayOfWeek Date::CalcDayOfWeek(int day, Month month, int year)
{
	static int monthsTable[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
	static int monthsTableLeap[12] = {6, 2, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};

	int yearInCentury = year % 100;
	int century = (year-yearInCentury)/100;
	int c = 2*(3-(century%4));
	int y = yearInCentury + yearInCentury/4;
	int m = monthsTable[month];

	if (IsLeapYear(year))
	{
		m = monthsTableLeap[month];
	}

	int sum = c + y + m + day;

	return (DayOfWeek)(sum%7);
}

bool Date::IsValid(int day, Month month, int year) const
{
	return year>=1898 && 
		   year<2500 && 
		   month>=0 && 
		   month<12 &&
		   day>0 && 
		   day<=Date::GetDaysInMonth((Date::Month)month, year);
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS DateTime
///////////////////////////////////////////////////////////////////////////////

DateTime::DateTime()
{
}

DateTime::DateTime(const Date& date, const Time& time)
{
	m_date.Set(date);
	m_time.Set(time);
}

void DateTime::Set(const Date& date, const Time& time)
{
	m_date.Set(date);
	m_time.Set(time);
}

void DateTime::Set(int day, Date::Month month, int year, int hour, int minute, int second)
{
	m_date.Set(day, month, year);
	m_time.Set(hour, minute, second);
}

void DateTime::SetDate(const Date& date)
{
	m_date.Set(date);
}

void DateTime::SetDate(int day, Date::Month month, int year)
{
	m_date.Set(day, month, year);
}

void DateTime::SetTime(const Time& time)
{
	m_time.Set(time);
}

void DateTime::SetTime(int hour, int minute, int second)
{
	m_time.Set(hour, minute, second);
}

void DateTime::AddDays(int numDays)
{
	m_date.Add(numDays);
}

void DateTime::AddTime(int numHours, int numMinutes, int numSeconds)
{
	int dayCount = 0;
	dayCount += m_time.Add(numHours, numMinutes, numSeconds);
	m_date.Add(dayCount);
}

int DateTime::GetDateTimeBetween(Time& t, const DateTime& dt1, const DateTime& dt2)
{
	int d1DaysSince1900 = dt1.GetDate().GetDaysSince1900();
	int d1SecondsIntoDay = dt1.GetTime().GetInSeconds();

	int d2DaysSince1900 = dt2.GetDate().GetDaysSince1900();
	int d2SecondsIntoDay = dt2.GetTime().GetInSeconds();

	int d1SecondsSince1900 = d1DaysSince1900*SECONDS_IN_DAY + d1SecondsIntoDay;
	int d2SecondsSince1900 = d2DaysSince1900*SECONDS_IN_DAY + d2SecondsIntoDay;

	if (d1SecondsSince1900>d2SecondsSince1900)
	{
		Assertf(0, "Invalid date input - date 2 must be after date 1");
	}

	int secondsBetween = d2SecondsSince1900-d1SecondsSince1900;

	t.Set(0, 0, 0);
	return t.Add(0, 0, secondsBetween);
}

int DateTime::GetSecondsSince1900() const
{
	int daysSince1900 = GetDate().GetDaysSince1900();
	int secondsIntoDay = GetTime().GetInSeconds();

	return daysSince1900*SECONDS_IN_DAY + secondsIntoDay;
}

int DateTime::GetSecondsSince(int year) const
{
	int daysSince = GetDate().GetDaysSince(year);
	int secondsIntoDay = GetTime().GetInSeconds();
	return daysSince*SECONDS_IN_DAY + secondsIntoDay;
}




///////////////////////////////////////////////////////////////////////////////
//  CLASS CClock
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CClock::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    // settings
	    ms_mode = CLOCK_MODE_DYNAMIC;
	    ms_numTimeSamples = 0;

	    ms_msPerGameMinute = MS_PER_GAME_MINUTE;

	    // state
	    ms_dateTime.SetDate(1, Date::JANUARY, 2011);
	    ms_dateTime.SetTime(12, 0, 0);
		ms_isPaused = false;
		ms_isPausedByScript = false;

	    // dynamic mode specific
	    ms_timeLastMinAdded = fwTimer::GetTimeInMilliseconds();
		ms_timeSinceLastMinAdded = 0;

	    // fixed mode specific
	    ms_currTimeSample = 0;

    #if __BANK
	    ms_debugOutput = true;
    #endif
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
        // reset the number of time samples
	    ms_numTimeSamples = 0;

	    // load in this levels time data file
	    const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TIME_FILE);
	    if (DATAFILEMGR.IsValid(pData))
	    {
		    LoadTimeData(pData->m_filename);
	    }
	    else
	    {
		    Assertf(0, "no time data file is specified for this level");
	    }

	    if (ms_mode==CLOCK_MODE_FIXED)
	    {
		    SetTimeSample(2);
	    }
    }
} 

///////////////////////////////////////////////////////////////////////////////
//  LoadTimeData
///////////////////////////////////////////////////////////////////////////////

void CClock::LoadTimeData(const char* pFilename)
{
	parTree* pTree = PARSER.LoadTree(pFilename, "xml");

	if (pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();

		ms_mode = CLOCK_MODE_DYNAMIC;
		parAttribute* pAttribute = pRootNode->GetElement().FindAttribute("mode");
		if (pAttribute)
		{
			const char* pModeName = pAttribute->GetStringValue();
			if (strcmp(pModeName, "fixed")==0)
			{
				ms_mode = CLOCK_MODE_FIXED;
			}
			else if (strcmp(pModeName, "dynamic")==0)
			{
				ms_mode = CLOCK_MODE_DYNAMIC;
			}
			else
			{
				Assertf(0, "clock mode attribute not recognised");
			}
		}
		else
		{
			Assertf(0, "missing attribute \"mode\" in root node in file %s", pFilename);
		}

		// read in the timecycle sun info
		parTreeNode* pSunInfoNode = pRootNode->FindChildWithName("suninfo", NULL);
		if (pSunInfoNode)
		{
			pAttribute = pSunInfoNode->GetElement().FindAttribute("sun_roll");
			if (pAttribute)
			{
				g_timeCycle.SetSunRollAngle(DtoR * static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"sun_roll\" in suninfo node in file %s", pFilename);
			}

			pAttribute = pSunInfoNode->GetElement().FindAttribute("sun_yaw");
			if (pAttribute)
			{
				g_timeCycle.SetSunYawAngle(DtoR * static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"sun_yaw\" in suninfo node in file %s", pFilename);
			}
		}

		// read in the timecycle sun info
		parTreeNode* pMoonInfoNode = pRootNode->FindChildWithName("mooninfo", NULL);
		if (pMoonInfoNode)
		{
			pAttribute = pMoonInfoNode->GetElement().FindAttribute("moon_roll");
			if (pAttribute)
			{
				g_timeCycle.SetMoonRollOffset(DtoR * static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"moon_roll\" in suninfo node in file %s", pFilename);
			}

			pAttribute = pMoonInfoNode->GetElement().FindAttribute("moon_wobble_freq");
			if (pAttribute)
			{
				g_timeCycle.SetMoonWobbleFrequency(static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"moon_wobble_freq\" in suninfo node in file %s", pFilename);
			}

			pAttribute = pMoonInfoNode->GetElement().FindAttribute("moon_wobble_amp");
			if (pAttribute)
			{
				g_timeCycle.SetMoonWobbleAmplitude(static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"moon_wobble_amp\" in suninfo node in file %s", pFilename);
			}

			pAttribute = pMoonInfoNode->GetElement().FindAttribute("moon_wobble_offset");
			if (pAttribute)
			{
				g_timeCycle.SetMoonWobbleOffset(static_cast<float>(atof(pAttribute->GetStringValue())));
			}
			else
			{
				Assertf(0, "missing attribute \"moon_wobble_offset\" in suninfo node in file %s", pFilename);
			}
		}

		// read in the time sample data
		ms_numTimeSamples = 0;
		parTreeNode* pSampleNode = NULL;
		while ((pSampleNode = pRootNode->FindChildWithName("sample", pSampleNode)) != NULL)
		{
			// get the name of this cycle
			char sampleName[64];
			pAttribute = pSampleNode->GetElement().FindAttribute("name");
			if (pAttribute)
			{
				strcpy(sampleName, pAttribute->GetStringValue());
#if __BANK
				strcpy(ms_timeSampleInfo[ms_numTimeSamples].name, sampleName);
#endif	
			}
			else
			{
				Assertf(0, "missing attribute \"name\" in sample node in file %s", pFilename);
			}

			pAttribute = pSampleNode->GetElement().FindAttribute("hour");
			if (pAttribute)
			{
				ms_timeSampleInfo[ms_numTimeSamples].hour = (s32)atoi(pAttribute->GetStringValue());
			}
			else
			{
				Assertf(0, "missing attribute \"hour\" in sample node in file %s", pFilename);
			}

			ms_timeSampleInfo[ms_numTimeSamples].duration = 0;
			pAttribute = pSampleNode->GetElement().FindAttribute("duration");
			if (pAttribute)
			{
				ms_timeSampleInfo[ms_numTimeSamples].duration = (s32)atoi(pAttribute->GetStringValue());
			}
//	no errors, in case of missing duration, just use 0.
//			else
//			{
//				Assertf(0, "missing attribute \"duration\" in sample node in file %s", pFilename);
//			}


			char uwTCModName[64];
			pAttribute = pSampleNode->GetElement().FindAttribute("uw_tc_mod");
			if (pAttribute)
			{
				strcpy(uwTCModName, pAttribute->GetStringValue());
				ms_timeSampleInfo[ms_numTimeSamples].uwTimeCycleModHash = atStringHash(uwTCModName);
#if __BANK
				strcpy(ms_timeSampleInfo[ms_numTimeSamples].uwTimeCycleModName, uwTCModName);
#endif	
			}
			else
			{
				Assertf(0, "missing attribute \"uw_tc_mod\" in sample node in file %s", pFilename);
			}

			ms_numTimeSamples++;
		}

#if __ASSERT
		// Fix up interpolation values
		for(int i=0;i<ms_numTimeSamples-1;i++)
		{
			TimeSample_s &sample = ms_timeSampleInfo[i];
			TimeSample_s &nextSample = ms_timeSampleInfo[i+1];
			Assertf(sample.hour + sample.duration <= nextSample.hour,"Invalid interp value for sample %d: max is %d, currently set at %d",sample.hour,nextSample.hour - sample.hour,sample.duration);
		}
		TimeSample_s &sample = ms_timeSampleInfo[ms_numTimeSamples-1];
		Assertf(sample.hour + sample.duration <= 24,"Invalid interp value for sample %d: max is %d, currently set at %d",sample.hour,24 - sample.hour,sample.duration);
#endif // __ASSERT

		// read in the region data
		parTreeNode* pRegionNode = NULL;
		g_timeCycle.ResetRegionInfo();
		g_timeCycle.AddRegionInfo("GLOBAL");
		while ((pRegionNode = pRootNode->FindChildWithName("region", pRegionNode)) != NULL)
		{
			// get the name of this cycle
			char regionName[64];
			pAttribute = pRegionNode->GetElement().FindAttribute("name");
			if (pAttribute)
			{
				strcpy(regionName, pAttribute->GetStringValue());
				g_timeCycle.AddRegionInfo(regionName);
			}
			else
			{
				Assertf(0, "missing attribute \"name\" in sample node in file %s", pFilename);
			}
		}
	}

	delete pTree;
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void CClock::Update()
{
	// Reset Script set flag
	SetFromScriptThisFrame(false);

	u32 currUpdateTime = fwTimer::GetTimeInMilliseconds();

	// process debug controls
#if !__FINAL
	UpdateDebug(currUpdateTime);
#endif

	//work out a timing delta based of how much the clock has move, so we can tie stuff that should be tied to 
	//the clock update, but update via a delta time, this is not quite as accurate as the timer, but it's close
	float numberOfInGameMinutesPassed = (ms_dateTime.GetSecondsSince1900() - ms_PreviousInGameTimeInSeconds) / 60.0f;
	ms_ApproxTimeStepMS = numberOfInGameMinutesPassed * ms_msPerGameMinute;
	ms_PreviousInGameTimeInSeconds = ms_dateTime.GetSecondsSince1900();
	
	// don't update the clock if the game is paused
	if (!fwTimer::IsGamePaused() REPLAY_ONLY( && !CReplayMgr::IsEditModeActive()) )
	{
		// update the time
		if (GetIsPaused() || ms_mode==CLOCK_MODE_FIXED)
		{
			ms_timeLastMinAdded = currUpdateTime - ms_timeSinceLastMinAdded;
		}
		else
#if RSG_CPU_INTEL && __BANK
		if ( m_timeSpeedMult > 0.0f)	// Intel CPUs don't like division by zero
#endif
		{
			// get the number of milliseconds per game minute
			s32 msPerGameMinute = ms_msPerGameMinute;
#if __BANK
			msPerGameMinute = (s32)((float)msPerGameMinute * (1.0f / m_timeSpeedMult));	
#endif 

#if GTA_REPLAY
			// Because time can go backwards when replay whizzes about
			// We need to check and set the last time to current time if that has happened to prevent "seconds" below going negative (massively positive).
			// This does mean the clock won't update correctly if we jump backwards in time, but replay sets the time anyway, via CClock::SetTime()
			if( ms_timeLastMinAdded > currUpdateTime )
			{
				ms_timeLastMinAdded = currUpdateTime;
			}
#endif	// GTA_REPLAY

			// update the time with any minutes that have passed
			while ((s32)currUpdateTime - (s32)ms_timeLastMinAdded >= msPerGameMinute)
			{
				ms_timeLastMinAdded += msPerGameMinute;
				ms_dateTime.AddTime(0, 1, 0);
			}

			// calc the remaining time that hasn't been added to the update yet (used to calc the number of seconds)
			ms_timeSinceLastMinAdded = currUpdateTime - ms_timeLastMinAdded;

			s32 seconds = (s32)((60.0f * ((float)ms_timeSinceLastMinAdded)) / (float)msPerGameMinute);
			ms_dateTime.GetTime().Set(ms_dateTime.GetTime().GetHours(), ms_dateTime.GetTime().GetMinutes(), seconds);
		}
	}

#if __BANK
	if (ms_debugOutput)
	{
		OutputDebugInfo();
	}

	if (m_overrideTime)
	{
		s32 numHours = m_overrideTimeVal / 60;
		s32 numMins = m_overrideTimeVal - (numHours*60);
		SetTime(numHours, numMins, 0);
	}
#endif
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdateDebug
///////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CClock::UpdateDebug(u32 currUpdateTime)
{
	XPARAM(nocheats);
	if (PARAM_nocheats.Get())
		return;

	bool bClockChanged = false;

	if (ms_mode==CLOCK_MODE_DYNAMIC)
	{
		// decrease time by an hour
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_MINUS, KEYBOARD_MODE_DEBUG, "decrease time 1 hour"))
		{
			DebugAddHoursAndTruncate(-1, currUpdateTime);
			bClockChanged = true;
		}

		// increase time by an hour
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_EQUALS, KEYBOARD_MODE_DEBUG, "increase time 1 hour"))
		{
			DebugAddHoursAndTruncate(1, currUpdateTime);
			bClockChanged = true;
		}

		// increase time by 2 minutes
		// NOTE: We ensure that the debug keyboard mode is active for debug pad input also, as other keyboard modes may wish to assign their own
		// pad input mappings.
		if ((CControlMgr::GetKeyboard().GetKeyboardMode() == KEYBOARD_MODE_DEBUG) && !CControlMgr::GetDebugPad().GetButtonTriangle() &&
			CControlMgr::GetDebugPad().GetRightShoulder1())
		{
			ms_timeLastMinAdded = currUpdateTime;
			ms_timeSinceLastMinAdded = 0;
			ms_dateTime.AddTime(0, 2, 0);
			bClockChanged = true;
		}
	}
	else if (ms_mode==CLOCK_MODE_FIXED)
	{
		// go to previous time step
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_MINUS, KEYBOARD_MODE_DEBUG, "decrease time 1 hour"))
		{
			ms_timeLastMinAdded = currUpdateTime;
			ms_timeSinceLastMinAdded = 0;
			s32 newTimeSample = (ms_currTimeSample+ms_numTimeSamples-1)%ms_numTimeSamples;
			SetTimeSample(newTimeSample);
			bClockChanged = true;
		}

		// go to next time step
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_EQUALS, KEYBOARD_MODE_DEBUG, "increase time 1 hour"))
		{
			ms_timeLastMinAdded = currUpdateTime;
			ms_timeSinceLastMinAdded = 0;
			s32 newTimeSample = (ms_currTimeSample+1)%ms_numTimeSamples;
			SetTimeSample(newTimeSample);
			bClockChanged = true;
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (bClockChanged)
		{
            gnetDebug1("CClock::UpdateDebug :: Debug clock applied. %02d:%02d:%02d", GetHour(), GetMinute(), GetSecond());
			ms_DebugModifiedInNetworkGame = true; 
			CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_DEBUG_CHANGE);
		}
	}

	if (bClockChanged)
	{
		PostFX::ResetAdaptedLuminance();
	}
}

void CClock::DebugAddHoursAndTruncate(s32 hours, u32 currUpdateTime)
{
	ms_timeLastMinAdded = currUpdateTime;
	ms_timeSinceLastMinAdded = 0;
	ms_dateTime.AddTime(hours, 0, 0);
	ms_dateTime.GetTime().Set(ms_dateTime.GetTime().GetHours(), 0, 0);
}
#endif // __FINAL


s32 CClock::GetDefaultMsPerGameMinute()
{
	return MS_PER_GAME_MINUTE;
}

///////////////////////////////////////////////////////////////////////////////
//  SetTime
///////////////////////////////////////////////////////////////////////////////

void CClock::SetTime(s32 hours, s32 minutes, s32 seconds)
{
	Assertf(ms_mode==CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");

	SetTimeAnyMode(hours, minutes, seconds);
}


///////////////////////////////////////////////////////////////////////////////
//  SyncDebugNetworkTime
///////////////////////////////////////////////////////////////////////////////
#if !__FINAL
void CClock::SyncDebugNetworkTime(s32 hours, s32 minutes, s32 seconds)
{
	gnetAssertf(ms_mode == CLOCK_MODE_DYNAMIC, "Calling a dynamic mode function when not in dynamic mode");

    gnetDebug1("CClock::SyncDebugNetworkTime :: Debug clock applied. Was: %02d:%02d:%02d, Now: %02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond(), hours, minutes, seconds);

    // same as SetTimeAnyMode (without ProcessTimeSkip)
    ms_dateTime.GetTime().Set(hours, minutes, seconds);
    ms_timeSinceLastMinAdded = (int)((float)ms_dateTime.GetTime().GetSeconds()/60.0f * (float)ms_msPerGameMinute);
    ms_timeLastMinAdded = fwTimer::GetTimeInMilliseconds() - ms_timeSinceLastMinAdded;

    // reflect that time is debug modified
	ms_DebugModifiedInNetworkGame = true;
}

void CClock::ResetDebugModifiedInNetworkGame()
{
	if(ms_DebugModifiedInNetworkGame)
	{
		gnetDebug1("NetClock :: ResetModifiedInNetworkGame");
		ms_DebugModifiedInNetworkGame = false;
	}
}
#endif

#if !__NO_OUTPUT
const char* CClock::GetDisplayCode()
{
#if !__FINAL
	if(ms_DebugModifiedInNetworkGame)
		return "d";
#endif

#if __BANK
	if(GetTimeOverrideFlagRef())
		return "f";
#endif

	if(CNetwork::IsClockOverrideFromScript())
		return "o";

	if(CNetwork::IsClockTimeOverridden())
		return "c";

	return "";
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  SetTimeAnyMode
///////////////////////////////////////////////////////////////////////////////

// private function that any mode can call
void CClock::SetTimeAnyMode(s32 hours, s32 minutes, s32 seconds)
{
	ms_dateTime.GetTime().Set(hours, minutes, seconds);

	// update the time since the last minute was added to reflect the new seconds
	ms_timeSinceLastMinAdded = (int)((float)ms_dateTime.GetTime().GetSeconds()/60.0f * (float)ms_msPerGameMinute);

	// update the time the last minute was added to reflect this
	ms_timeLastMinAdded = fwTimer::GetTimeInMilliseconds() - ms_timeSinceLastMinAdded;

	ProcessTimeSkip();
}


///////////////////////////////////////////////////////////////////////////////
//  AddTime
///////////////////////////////////////////////////////////////////////////////

void CClock::AddTime(s32 numHours, s32 numMinutes, s32 numSeconds)
{
	Assertf(ms_mode==CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");

	ms_dateTime.AddTime(numHours, numMinutes, numSeconds);

	// update the time since the last minute was added to reflect the new seconds
	ms_timeSinceLastMinAdded = (int)((float)ms_dateTime.GetTime().GetSeconds()/60.0f * (float)ms_msPerGameMinute);

	ProcessTimeSkip();
}


///////////////////////////////////////////////////////////////////////////////
//  AdvanceTimeTo
///////////////////////////////////////////////////////////////////////////////

void CClock::AdvanceTimeTo(s32 hours, s32 minutes, s32 seconds)
{
	Assertf(ms_mode==CLOCK_MODE_DYNAMIC, "calling a dynamic mode function when not in dynamic mode");

	// set the time to the target time
	SetTime(hours, minutes, seconds);

	// check if the time is the following day
	Time targetTime(hours, minutes, seconds);
	int currSecondsInDay = ms_dateTime.GetTime().GetInSeconds();
	int targetSecondsInDay = targetTime.GetInSeconds();
	if (targetSecondsInDay<currSecondsInDay)
	{
		// target time is the next day - advance the date by a day
		ms_dateTime.GetDate().Add(1);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// PassTime
/////////////////////////////////////////////////////////////////////////////////

// moved from CGameLogic
void CClock::PassTime(u32 GameMinutes, bool bAvoidNight)
{
	if (GetMode()==CLOCK_MODE_DYNAMIC)
	{
		// If we want to avoid the night we don't pass the time until between 22:00 and 7:00.
		// In this window we skip straight to 7:00.
		if (bAvoidNight)
		{
			s32 predictedMinutes = GetMinute() + GameMinutes;
			s32 predictedHours = (GetHour() + (predictedMinutes / 60))%24;

			if (predictedHours >= 22 || predictedHours < 7)
			{
				GameMinutes = GetMinutesUntil(7, 0);
			}
		}

		AddTime(0, GameMinutes, 0);
	}	
}

///////////////////////////////////////////////////////////////////////////////
//  SetDate
///////////////////////////////////////////////////////////////////////////////

void CClock::SetDate(s32 day, Date::Month month, s32 year)
{
	ms_dateTime.GetDate().Set(day, month, year);
}


///////////////////////////////////////////////////////////////////////////////
//  AddDays
///////////////////////////////////////////////////////////////////////////////

void CClock::AddDays(s32 numDays)
{
	ms_dateTime.GetDate().Add(numDays);
}

///////////////////////////////////////////////////////////////////////////////
//  IsTimeInRange
///////////////////////////////////////////////////////////////////////////////

bool CClock::IsTimeInRange(s32 startHour, s32 endHour)
{
	s32 currHour = ms_dateTime.GetTime().GetHours();
	if (startHour <= endHour)
	{
		if (currHour>=startHour && currHour<endHour)
		{
			return true;
		}
	}	
	else
	{
		if (currHour>=startHour || currHour<endHour)
		{
			return true;
		}
	}

	return false;
} 

///////////////////////////////////////////////////////////////////////////////
//  FromDayRatio
///////////////////////////////////////////////////////////////////////////////

void CClock::FromDayRatio(float ratio, s32& hour, s32& minute, s32& second)
{
	float fHours = ratio * 24.0f;
	hour = static_cast<s32>(fHours);
	float fMinutes = (fHours - static_cast<float>(hour)) * 60.0f;
	minute = static_cast<s32>(fMinutes);
	float fSeconds = (fMinutes - static_cast<float>(minute)) * 60.0f;
	second = static_cast<s32>(fSeconds);
} 


///////////////////////////////////////////////////////////////////////////////
//  GetMinutesUntil
///////////////////////////////////////////////////////////////////////////////

s32 CClock::GetMinutesUntil(s32 hour, s32 minute)
{
	Time tRes;
	Time::GetTimeBetween(tRes, ms_dateTime.GetTime(), Time(hour, minute, 0));
	return tRes.GetInMinutes();
} 

///////////////////////////////////////////////////////////////////////////////
//  GetTimeSample
///////////////////////////////////////////////////////////////////////////////

s32 CClock::GetTimeSample()
{
    Assertf(ms_mode==CLOCK_MODE_FIXED, "calling a fixed mode function when not in fixed mode");

    return ms_currTimeSample;
}

///////////////////////////////////////////////////////////////////////////////
//  GetDayOfWeekString
///////////////////////////////////////////////////////////////////////////////

const char *CClock::GetDayOfWeekTextId()
{
	Date& currDate = ms_dateTime.GetDate();

	switch (currDate.GetDayOfWeek())
	{
		case Date::SUNDAY:
		{
			return "fSu";
		}

		case Date::MONDAY:
		{
			return "fMo";
		}

		case Date::TUESDAY:
		{
			return "fTu";
		}

		case Date::WEDNESDAY:
		{
			return "fWe";
		}

		case Date::THURSDAY:
		{
			return "fTh";
		}

		case Date::FRIDAY:
		{
			return "fFr";
		}

		case Date::SATURDAY:
		{
			return "fSa";
		}

		default:
		{
			Assertf(0, "Invalid day %d", (s32)currDate.GetDayOfWeek());

			return " ";
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  SetTimeSample
///////////////////////////////////////////////////////////////////////////////

void CClock::SetTimeSample(s32 idx)
{
	Assertf(ms_mode==CLOCK_MODE_FIXED, "calling a fixed mode function when not in fixed mode");

	if (Verifyf(idx>=0 && idx<ms_numTimeSamples, "time sample out of range"))
	{
		ms_currTimeSample = idx;
		SetTimeAnyMode(ms_timeSampleInfo[idx].hour, 0, 0);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  CreateCopyOfDateTimeStruct
///////////////////////////////////////////////////////////////////////////////

void CClock::CreateCopyOfDateTimeStruct(DateTime &NewCopyOfDateTime)
{
	NewCopyOfDateTime = ms_dateTime;
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessTimeSkip
///////////////////////////////////////////////////////////////////////////////

void CClock::ProcessTimeSkip()
{
	// reset the police scanner 
	NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.CancelAnyPlayingReports());

	// reset the player's wet feet
	if (CGameWorld::FindLocalPlayer()) 
	{
		CGameWorld::FindLocalPlayer()->GetPedAudioEntity()->GetFootStepAudio().ResetWetFeet();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  OutputDebugInfo
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CClock::OutputDebugInfo()
{
	Date& currDate = ms_dateTime.GetDate();
	Time& currTime = ms_dateTime.GetTime();

	char txt[64];
	
	if (ms_mode==CLOCK_MODE_FIXED)
	{
		sprintf(txt, "%s %02d %s %04d %02d:%02d:%02d (%d - %s)", currDate.GetDayOfWeekName(), currDate.GetDay(), currDate.GetMonthName(), currDate.GetYear(), currTime.GetHours(), currTime.GetMinutes(), currTime.GetSeconds(), ms_currTimeSample, ms_timeSampleInfo[ms_currTimeSample].name);
	}
	else if (ms_mode==CLOCK_MODE_DYNAMIC)
	{
		sprintf(txt, "%s %02d %s %04d %02d:%02d:%02d", currDate.GetDayOfWeekName(), currDate.GetDay(), currDate.GetMonthName(), currDate.GetYear(), currTime.GetHours(), currTime.GetMinutes(), currTime.GetSeconds());
	}
	
	grcDebugDraw::AddDebugOutput(txt);
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK

PARAM(overridetime,"override time with this value (Same as using the Clock Widget");

void CClock::ReloadTimeFile()
{
	// reset the number of time samples
	ms_numTimeSamples = 0;

	// load in this levels time data file
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TIME_FILE);
	if (DATAFILEMGR.IsValid(pData))
	{
		LoadTimeData(pData->m_filename);
	}
	else
	{
		Assertf(0, "no time data file is specified for this level");
	}

	if (ms_mode==CLOCK_MODE_FIXED)
	{
		SetTimeSample(2);
	}
}

void CClock::DebugSetDate()
{
	SetDate(ms_debugDayOfMonthToSet, ms_debugMonthToSet, ms_debugYearToSet);

	// inform all other machines about the time change
	if (NetworkInterface::IsGameInProgress())
	{
        gnetDebug1("CClock::DebugSetDate :: Debug clock applied. %02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());
		ms_DebugModifiedInNetworkGame = true;
		CDebugGameClockEvent::Trigger(CDebugGameClockEvent::FLAG_IS_DEBUG_CHANGE);
	}
}

void CClock::OverrideGameRate()
{
	if (ms_OverrideGameRate)
	{
		CNetwork::OverrideClockRate(ms_OverridenMsPerGameMinute);
	}
	else
	{
		CNetwork::ClearClockTimeOverride();
	}
}

void CClock::OverrideGameRateMinuteValue()
{
	if (ms_OverrideGameRate)
	{
		CNetwork::OverrideClockRate(ms_OverridenMsPerGameMinute);
	}
}

void CClock::InitWidgets()
{
	m_timeSpeedMult = 1.0f;
	m_overrideTimeVal = 0;
	m_overrideTime = PARAM_overridetime.Get(m_overrideTimeVal);

	bkBank& bank = BANKMGR.CreateBank("Clock", 0, 0, false);
	{
		bank.AddTitle("DEBUG");
		{
			bank.AddToggle("Output Debug", &ms_debugOutput);
			bank.AddSlider("Time Speed Mult" ,&m_timeSpeedMult , 0.0f, 1000.0f, 0.1f);
			bank.AddToggle("Override Time", &m_overrideTime);
			bank.AddSlider("Curr Time", &m_overrideTimeVal, 0, (24*60)-1, 1);
			bank.AddToggle("Pause Time", &ms_isPaused);
			bank.AddButton("Reload time data", datCallback(CFA(ReloadTimeFile)));

			bank.PushGroup("Set Date", false);
				bank.AddSlider("Day of month to set", &ms_debugDayOfMonthToSet, 1, 31, 1);
				bank.AddSlider("Month to set", (int*) &ms_debugMonthToSet, 0, 11, 1);
				bank.AddSlider("Year to set", &ms_debugYearToSet, 1970, 2020, 1);
				bank.AddButton("Set Date Now", datCallback(CFA(DebugSetDate)));
			bank.PopGroup();

			bank.PushGroup("Override Game Speed", true);
				bank.AddToggle("Override Game Speed", &ms_OverrideGameRate, datCallback(CFA(OverrideGameRate)));
				bank.AddSlider("MS per Game Minute", &ms_OverridenMsPerGameMinute, 1, 10000, 10, datCallback(CFA(OverrideGameRateMinuteValue)));
			bank.PopGroup();
		}
	}
}
#endif


