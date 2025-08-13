///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Clock.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	16 October 2009
//
///////////////////////////////////////////////////////////////////////////////

#ifndef INC_CLOCK_H_
#define INC_CLOCK_H_


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define CLOCK_MAX_TIME_SAMPLES	(16)


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

enum ClockMode_e
{
	CLOCK_MODE_DYNAMIC			= 0,
	CLOCK_MODE_FIXED
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct TimeSample_s
{
	s32 hour;
	s32 duration;
	u32 uwTimeCycleModHash;
#if __BANK
	char name[32];
	char uwTimeCycleModName[32];
#endif
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Time
///////////////////////////////////////////////////////////////////////////////

class Time
{
#define SECONDS_IN_MINTE	(60)
#define SECONDS_IN_HOUR		(60*SECONDS_IN_MINTE)
#define SECONDS_IN_DAY		(24*SECONDS_IN_HOUR)

	// functions
public:
	Time();
	Time(int hour, int minute, int second);

	void Set(const Time& time);
	void Set(int hour, int minute, int second);

	int Add(const Time& time);
	int Add(int numHours, int numMinutes, int numSeconds);

	int GetInMinutes() const;	
	int GetInSeconds() const;

	static bool GetTimeBetween(Time& tRes, const Time& t1, const Time& t2);

	bool IsValid(int hour, int minute, int second) const;

	int GetHours() const {return m_hour;}
	int GetMinutes() const {return m_min;}
	int GetSeconds() const {return m_sec;}

private:
	int AddHour(int numHours);
	int AddMinute(int numMinutes);
	int AddSecond(int numSeconds);

	// variables
private:
	int m_hour;		// hours since midnight - [0,23]
	int m_min;		// minutes after the hour - [0,59]
	int m_sec;		// seconds after the minute - [0,59]

};


///////////////////////////////////////////////////////////////////////////////
//  Date
///////////////////////////////////////////////////////////////////////////////

class Date
{
#define DAYS_IN_NON_LEAP_YEAR (365)

public:
	enum DayOfWeek
	{
		SUNDAY = 0,
		MONDAY,
		TUESDAY,
		WEDNESDAY,
		THURSDAY,
		FRIDAY,
		SATURDAY
	};

	enum Month
	{
		JANUARY = 0,
		FEBRUARY,
		MARCH,
		APRIL,
		MAY,
		JUNE, 
		JULY,
		AUGUST,
		SEPTEMBER,
		OCTOBER,
		NOVEMBER,
		DECEMBER
	};

	friend class DateTime;

	// functions 
public:
	Date();
	Date(int day, Month month, int year);

	void Set(const Date& date);
	void Set(int day, Month month, int year);

	void Add(int numDays);

	int GetDayOfYear() const;

	static int GetDaysBetween(const Date& d1, const Date& d2);

	bool IsValid(int day, Month month, int year) const;

	DayOfWeek GetDayOfWeek() const {return m_wday;} 
	int GetDay() const {return m_day;} 
	Month GetMonth() const {return m_month;} 
	int GetYear() const {return m_year;} 

	const char* GetDayOfWeekName() const {return ms_daysOfWeekNames[m_wday];} 
	const char* GetMonthName() const {return ms_monthsOfYearNames[m_month];} 

	bool operator ==(const Date &other) const 
	{
		if( m_wday != other.m_wday ||
			m_day != other.m_day ||
			m_month != other.m_month || 
			m_year != other.m_year )
		{
			return false;
		}
		return true;
	}


private:

	void AddDay(int numDays);
	void AddMonth(int numMonths);
	void AddYear(int numYears);

	int GetDaysSince1900() const;
	int GetDaysSince(int year) const;

	static bool IsLeapYear(int year);
	static int GetDaysInMonth(Month month, int year);
	static void GetPrevMonth(Month& prevMonth, int& prevMonthsYear, Month month, int year);
	static DayOfWeek CalcDayOfWeek(int day, Month month, int year);

	// variables
private:
	DayOfWeek m_wday;		// days since Sunday - [0,6]
	int m_day;				// day of the month - [1,31]
	Month m_month;  		// months since January - [0,11]
	int m_year;				// year

	static int ms_daysInMonth[12];
	static const char* ms_monthsOfYearNames[12];
	static const char* ms_daysOfWeekNames[7];
};


///////////////////////////////////////////////////////////////////////////////
//  DateTime
///////////////////////////////////////////////////////////////////////////////

class DateTime
{
	// functions 
public:
	DateTime();
	DateTime(const Date& date, const Time& time);

	void Set(const Date& date, const Time& time);
	void Set(int day, Date::Month month, int year, int hour, int minute, int second);

	void SetDate(const Date& date);
	void SetDate(int day, Date::Month month, int year);
	Date& GetDate() {return m_date;}
	const Date& GetDate() const {return m_date;}

	void SetTime(const Time& time);
	void SetTime(int hour, int minute, int second);
	Time& GetTime() {return m_time;}
	const Time& GetTime() const {return m_time;}

	void AddDays(int numDays);
	void AddTime(int numHours, int numMinutes, int numSeconds);

	int GetSecondsSince1900() const;
	int GetSecondsSince(int year) const;

	static int GetDateTimeBetween(Time& t, const DateTime& dt1, const DateTime& dt2);

private:

	// variables
private:
	Date m_date;
	Time m_time;
};


///////////////////////////////////////////////////////////////////////////////
//  CClock
///////////////////////////////////////////////////////////////////////////////

class CClock
{
	// friends
	friend class CSimpleVariablesSaveStructure;

	// functions (public)
	public:

		static void Init(unsigned initMode);
		static void Update();

		// clock settings
		static ClockMode_e GetMode() {return ms_mode;}

		static s32 GetNumTimeSamples() {return ms_numTimeSamples;}
		static const TimeSample_s& GetTimeSampleInfo(s32 idx) {return ms_timeSampleInfo[idx];}

		static void SetMsPerGameMinute(u32 msPerGameMinute) {ms_msPerGameMinute = msPerGameMinute;}
		static s32 GetMsPerGameMinute() {return ms_msPerGameMinute;}
		static s32 GetDefaultMsPerGameMinute();

		// time manipulation
		static void SetTime(s32 hours, s32 minutes, s32 seconds);
		static void AddTime(s32 numHours, s32 numMinutes, s32 numSeconds);
		static void AdvanceTimeTo(s32 hours, s32 minutes, s32 seconds);
		static void PassTime(u32 GameMinutes, bool bAvoidNight); // moved from CGameLogic
		static void Pause(bool val) {ms_isPaused = val;}
		static void ScriptPause(bool val) { ms_isPausedByScript = val; }
		static bool GetIsPaused() {return ms_isPaused || ms_isPausedByScript;}

		// time accessors
		static s32 GetHour() {return ms_dateTime.GetTime().GetHours();}
		static s32 GetMinute() {return ms_dateTime.GetTime().GetMinutes();}
		static s32 GetSecond() {return ms_dateTime.GetTime().GetSeconds();}

#if !__NO_OUTPUT
		static const char* GetDisplayCode();
#endif

		static float GetApproxTimeStepMS() { return ms_ApproxTimeStepMS; } //get an approximate delta time based on the clock update

		// time helpers
		static bool IsTimeInRange(s32 startHour, s32 endHour);
		static float GetDayRatio() {return (((float)CClock::GetHour()/1.0f) + ((float)CClock::GetMinute()/60.0f) + ((float)CClock::GetSecond()/(60.0f*60.0f))) / 24.0f;}
		static float GetDayRatio(Time time) {return (((float)time.GetHours()/1.0f) + ((float)time.GetMinutes()/60.0f) + ((float)time.GetSeconds()/(60.0f*60.0f))) / 24.0f;}
		static void FromDayRatio(float ratio, s32& hours, s32& minutes, s32& seconds);
		static s32 GetMinutesSinceMidnight() {return ms_dateTime.GetTime().GetInMinutes();}
		static s32 GetMinutesUntil(s32 hour, s32 minute);

		// date manipulation
		static void SetDate(s32 day, Date::Month month, s32 year);
		static void AddDays(s32 numDays);

		// date accessors
		static s32 GetDay () {return ms_dateTime.GetDate().GetDay();}
		static s32 GetMonth() {return ms_dateTime.GetDate().GetMonth();}
		static s32 GetYear() {return ms_dateTime.GetDate().GetYear();}
		static s32 GetDayOfWeek() {return ms_dateTime.GetDate().GetDayOfWeek();}
		static const char *GetDayOfWeekTextId();
		static u32 GetSecondsSince1900(){ return ms_dateTime.GetSecondsSince1900();}
		static u32 GetSecondsSince(u32 year){ return ms_dateTime.GetSecondsSince(year);}



		// fixed mode specific
        static s32 GetTimeSample();
		static void SetTimeSample(s32 idx);

		// misc
		static void CreateCopyOfDateTimeStruct(DateTime &NewCopyOfDateTime);
		static u32 GetTimeLastMinuteAdded() { return ms_timeLastMinAdded; }
		static void SetFromScriptThisFrame(bool val) { ms_bClockSetThisFrame = val; }
		static bool WasSetFromScriptThisFrame() { return ms_bClockSetThisFrame; }

		// debug
#if __BANK
		static void InitWidgets();
		static float& GetTimeSpeedMultRef() {return m_timeSpeedMult;}
		static bool& GetTimeOverrideFlagRef() {return m_overrideTime;}
		static s32& GetTimeOverrideValRef() {return m_overrideTimeVal;}
#endif

#if !__FINAL
		static void DebugAddHoursAndTruncate(s32 hours, u32 currUpdateTime);

		static void SyncDebugNetworkTime(s32 hours, s32 minutes, s32 seconds);
		static bool HasDebugModifiedInNetworkGame() { return ms_DebugModifiedInNetworkGame; }
		static void ResetDebugModifiedInNetworkGame();
#endif

	// functions (private)
	private:

		static void LoadTimeData(const char* pFilename);
		static void SetTimeAnyMode(s32 hours, s32 minutes, s32 seconds);
		static void UpdateDayNightBalance();										// doesn't really belong in here - can this be moved?
		static void ProcessTimeSkip();												// doesn't really belong in here - can this be moved?

		// debug
#if !__FINAL
		static void UpdateDebug(u32 syncTimeInMs);
#endif
#if __BANK
		static void OutputDebugInfo();
		static void ReloadTimeFile();
		static void DebugSetDate();
		static void OverrideGameRate();
		static void OverrideGameRateMinuteValue();
#endif


	// variables (private)
	private:

		static bool	ms_bClockSetThisFrame;

		// settings
		static ClockMode_e ms_mode;													// whether we are in fixed or dynamic clock mode

		static s32 ms_numTimeSamples;
		static TimeSample_s ms_timeSampleInfo[CLOCK_MAX_TIME_SAMPLES];

		static u32 ms_msPerGameMinute;		

		// state
		static DateTime ms_dateTime;

		// dynamic mode specific
		static u32 ms_timeLastMinAdded;
		static u32 ms_timeSinceLastMinAdded;

		// fixed mode specific
		static int ms_currTimeSample;

		// misc
		static float ms_dayNightBalance[2];											// doesn't really belong in here - can this be moved?
		static bool ms_isPaused;
		static bool ms_isPausedByScript;

#if !__FINAL
		static bool ms_DebugModifiedInNetworkGame;
#endif

		static s32 ms_PreviousInGameTimeInSeconds;
		static float ms_ApproxTimeStepMS;

#if __BANK
		static bool ms_debugOutput;
		static float m_timeSpeedMult;
		static bool m_overrideTime;
		static s32 m_overrideTimeVal;
		static bool ms_OverrideGameRate;
		static u32 ms_OverridenMsPerGameMinute;

		static s32 ms_debugDayOfMonthToSet;
		static Date::Month ms_debugMonthToSet;
		static s32 ms_debugYearToSet;
#endif

};

#endif

