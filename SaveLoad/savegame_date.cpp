
#include <string.h>

#if __PPU
#include <time.h>
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#endif

#include "frontend/PauseMenu.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_date.h"

#if __PPU
s32 CDate::ms_DateFormat = CELL_SYSUTIL_DATE_FMT_DDMMYYYY;
#endif

void CDate::Initialise()
{
	Year=0;
	Month=0;
	Day=0;
	Hour=0;
	Minute=0;
	Second=0;
}

void CDate::Set(int in_hour, int in_minute, int in_second,
		 int in_day, int in_month, int in_year)
{
	Hour=in_hour;
	Minute=in_minute;
	Second=in_second;

	Day=in_day;
	Month=in_month;
	Year=in_year;
}

bool CDate::IsValid()
{
	if ( (0==Year) && (0==Month) && (0==Day)
		&& (0==Hour) && (0==Minute) && (0==Second) )
	{
		return false;
	}

	return true;
}

#if __PPU
void CDate::SetPS3DateFormat()
{    // date format to display
	cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_DATE_FORMAT, &ms_DateFormat);
}
#endif

bool CDate::IsDigit(const char AsciiCharacter, int *pReturnDigit)
{
	if ( (AsciiCharacter >= '0') && (AsciiCharacter <= '9') )
	{
		if (pReturnDigit)
		{
			*pReturnDigit = (int) (AsciiCharacter - '0');
		}
		return true;
	}

	return false;
}

bool CDate::operator > (const CDate &OtherDate) const
{
	if (Year>OtherDate.Year)
	{
		return true;
	}	
	else if (Year==OtherDate.Year)
	{
		if(Month>OtherDate.Month)
		{
			return true;
		}
		else if (Month==OtherDate.Month)	
		{
			if(Day>OtherDate.Day)
			{
				return true;	
			}
			else if (Day==OtherDate.Day)
			{		
				if (Hour>OtherDate.Hour)
				{
					return true;
				}
				else if (Hour==OtherDate.Hour)
				{
					if (Minute>OtherDate.Minute)
					{
						return true;
					}
					else if (Minute==OtherDate.Minute)
					{
						if (Second>OtherDate.Second)
						{
							return true;
						}	
					}
				}
			}
		}	
	}
	return false;
}

bool CDate::operator == (const CDate &OtherDate) const
{
	if ( (Year==OtherDate.Year) && (Month==OtherDate.Month) && (Day==OtherDate.Day)
		&& (Hour==OtherDate.Hour) && (Minute==OtherDate.Minute) && (Second==OtherDate.Second) )
	{
		return true;
	}	

	return false;
}

bool CDate::ExtractDateFromString(const char *pDateString)
{
	if (!pDateString)
	{
		return false;
	}

	if (strlen(pDateString) != 17)
	{
		return false;
	}

	const bool bIsJapanese = (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_JAPANESE);

	//		"mm/dd/yy hh:mm:ss"
	u32 CharIndex = 0;
	int FirstDigit, SecondDigit;
	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}

	if (bIsJapanese)	//	#if __JAPANESE_BUILD
	{
		Year/*Month*/ = (FirstDigit * 10) + SecondDigit;
	}
	else	//	#else
	{
		Month = (FirstDigit * 10) + SecondDigit;
	}
//	#endif

	if (pDateString[CharIndex++] != '/')
	{
		return false;
	}

	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}

	if (bIsJapanese)	//	#if __JAPANESE_BUILD
	{
		Month/*Day*/ = (FirstDigit * 10) + SecondDigit;
	}
	else	//	#else
	{
		Day = (FirstDigit * 10) + SecondDigit;
	}
//	#endif

	if (pDateString[CharIndex++] != '/')
	{
		return false;
	}

	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}

	if (bIsJapanese)	//	#if __JAPANESE_BUILD
	{
		Day/*Year*/ = (FirstDigit * 10) + SecondDigit;
	}
	else	//	#else
	{
		Year = (FirstDigit * 10) + SecondDigit;
	}
//	#endif

	if (pDateString[CharIndex++] != ' ')
	{
		return false;
	}

	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}
	Hour = (FirstDigit * 10) + SecondDigit;

	if (pDateString[CharIndex++] != ':')
	{
		return false;
	}

	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}
	Minute = (FirstDigit * 10) + SecondDigit;

	if (pDateString[CharIndex++] != ':')
	{
		return false;
	}

	if (!IsDigit(pDateString[CharIndex++], &FirstDigit))
	{
		return false;
	}
	if (!IsDigit(pDateString[CharIndex++], &SecondDigit))
	{
		return false;
	}
	Second = (FirstDigit * 10) + SecondDigit;

	if (pDateString[CharIndex++] != '\0')
	{
		return false;
	}

	return true;
}

bool CDate::ExtractDateFromPhotoFilename(const char *pDateString)
{
	if (!pDateString)
	{
		savegameAssertf(0, "CDate::ExtractDateFromPhotoFilename - pDateString is NULL");
		return false;
	}

	if (strlen(pDateString) != 14)
	{
		savegameAssertf(0, "CDate::ExtractDateFromPhotoFilename - expected pDateString %s to have length 14", pDateString);
		return false;
	}

	// "yyyymmddhhmmss"
	if (!ReadIntegerFromString(pDateString, 0, 3, Year))	//	, "Year"))
	{
		return false;
	}

	if (!ReadIntegerFromString(pDateString, 4, 5, Month))	//	, "Month"))
	{
		return false;
	}

	if (!ReadIntegerFromString(pDateString, 6, 7, Day))	//	, "Day"))
	{
		return false;
	}

	if (!ReadIntegerFromString(pDateString, 8, 9, Hour))	//	, "Hours"))
	{
		return false;
	}
	
	if (!ReadIntegerFromString(pDateString, 10, 11, Minute))	//	, "Minutes"))
	{
		return false;
	}

	if (!ReadIntegerFromString(pDateString, 12, 13, Second))	//	, "Seconds"))
	{
		return false;
	}

	return true;
}


bool CDate::ReadIntegerFromString(const char *pDateString, u32 firstDigit, u32 lastDigit, int &integerToWrite)	//	, const char *pAssertString)	//	ASSERT_ONLY
{
	if (!pDateString)
	{
		savegameAssertf(0, "CDate::ReadIntegerFromString - pDateString is NULL");
		return false;
	}

	if (lastDigit < firstDigit)
	{
		savegameAssertf(0, "CDate::ReadIntegerFromString - lastDigit %u is less than firstDigit %u", lastDigit, firstDigit);
		return false;
	}

	const u32 stringLength = ustrlen(pDateString);
	if (firstDigit >= stringLength)
	{
		savegameAssertf(0, "CDate::ReadIntegerFromString - firstDigit %u is beyond the end of the string %s", firstDigit, pDateString);
		return false;
	}

	if (lastDigit >= stringLength)
	{
		savegameAssertf(0, "CDate::ReadIntegerFromString - lastDigit %u is beyond the end of the string %s", lastDigit, pDateString);
		return false;
	}

	u32 digitLoop = firstDigit;
	int runningTotal = 0;
	while (digitLoop <= lastDigit)
	{
		int valueOfCurrentDigit = 0;
		if (!IsDigit(pDateString[digitLoop], &valueOfCurrentDigit))
		{
			savegameAssertf(0, "CDate::ReadIntegerFromString - character %u of string %s is not an integer", digitLoop, pDateString);
			return false;
		}

		runningTotal *= 10;
		runningTotal += valueOfCurrentDigit;

		digitLoop++;
	}

	integerToWrite = runningTotal;
	return true;
}

bool CDate::ConstructStringFromDate(char *pStringToContainDate, u32 MaxStringLength)
{
	if (!pStringToContainDate)
	{
		return false;
	}

	if (MaxStringLength <= 17)
	{	//	string[17], the 18th character will be '\0'
		Assertf(0, "CDate::ConstructStringFromDate - String is not long enough to contain a date");
		return false;
	}

	//		"mm/dd/yy hh:mm:ss"
	

	pStringToContainDate[0] = '\0';
#if __PPU

	const bool bIsJapanese = (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_JAPANESE);

	int year2digits = (Year % 100);

	switch(ms_DateFormat)
	{
	case CELL_SYSUTIL_DATE_FMT_YYYYMMDD:
		sprintf(pStringToContainDate, "%02d/%02d/%02d %02d:%02d:%02d", year2digits, Month, Day, Hour, Minute, Second);
		break;
	case CELL_SYSUTIL_DATE_FMT_MMDDYYYY:
		sprintf(pStringToContainDate, "%02d/%02d/%02d %02d:%02d:%02d", Month, Day, year2digits, Hour, Minute, Second);
		break;
	default:
		if (bIsJapanese)	//	#if __JAPANESE_BUILD
		{
			// assume yy/mm/dd
			sprintf(pStringToContainDate, "%02d/%02d/%02d %02d:%02d:%02d", year2digits, Month, Day, Hour, Minute, Second);
		}
		else	//	#else
		{
			// assume dd/mm/yy
			sprintf(pStringToContainDate, "%02d/%02d/%02d %02d:%02d:%02d", Day, Month, year2digits, Hour, Minute, Second);
		}
//	#endif
		break;
	}
#else
	char TempString[10];
	for (int loop = 0; loop < 6; loop++)
	{
		switch (loop)
		{
			case 0 :	//	Month (2 digits)
				sprintf(TempString, "%02d/", Month);
				break;

			case 1 :	//	Day (2 digits)
				sprintf(TempString, "%02d/", Day);
				break;

			case 2 :	//	Year (2 digits)
				{
					int LastTwoDigits = (Year % 100);
					sprintf(TempString, "%02d ", LastTwoDigits);
				}
				break;

			case 3 :	//	Hours (2 digits)
				sprintf(TempString, "%02d:", Hour);
				break;

			case 4 :	//	Minutes (2 digits)
				sprintf(TempString, "%02d:", Minute);
				break;

			case 5 :	//	Seconds (2 digits)
				sprintf(TempString, "%02d", Second);
				break;
		}
		strcat(pStringToContainDate, TempString);
		Assertf(strlen(pStringToContainDate) < MaxStringLength, "CDate::ConstructStringFromDate - the date string is longer than expected");
	}
#endif // __PPU

	return true;
}

#if	RSG_ORBIS
bool CDate::ExtractDateFromTimeT(const time_t &SlotTime)
{
	tm *pTimeForSlot = localtime(&SlotTime);	//	gmtime(&SlotTime);
	if (pTimeForSlot == NULL)
	{
		return false;
	}

	//	int tm_mon;        month of the year (from 0)
	Month = pTimeForSlot->tm_mon + 1;

	//	int tm_mday;       day of the month (from 1)
	Day = pTimeForSlot->tm_mday;

	//	int tm_year;       years since 1900 (from 0)
	Year = pTimeForSlot->tm_year + 1900;

	//	int tm_hour;       hour of the day (from 0)
	Hour = pTimeForSlot->tm_hour;

	//	int tm_min;        minutes after the hour (from 0)
	Minute = pTimeForSlot->tm_min;

	//	int tm_sec;        seconds after the minute (from 0)
	Second = pTimeForSlot->tm_sec;

	return true;
}
#endif

#if __XENON
void CDate::Convert2DigitYearTo4Digits(s32 &year)
{
	if (savegameVerifyf(year < 100, "CDate::Convert2DigitYearTo4Digits - expected input year to be less than 100 but it's %d", year))
	{
		savegameAssertf(year >= 0, "CDate::Convert2DigitYearTo4Digits - expected input year to be positive but it's %d", year);
		year += 2000;
	}
}

void CDate::Convert4DigitYearTo2Digits(s32 &year)
{
	if (savegameVerifyf(year > 2000, "CDate::Convert4DigitYearTo2Digits - expected input year to be greater than 2000 but it's %d", year))
	{
		savegameAssertf(year < 3000, "CDate::Convert4DigitYearTo2Digits - expected input year to be less than 3000 but it's %d", year);
		year -= 2000;
	}
}
#endif	//	__XENON





