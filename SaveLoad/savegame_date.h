

#ifndef SAVEGAME_DATE_H
#define SAVEGAME_DATE_H

#if __PPU
#include <time.h>
#endif	//	__PPU

class CDate
{
private:
//	Private data
	int Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;

#if __PPU
	static s32 ms_DateFormat;
#endif

//	Private functions
	bool IsDigit(const char AsciiCharacter, int *pReturnDigit);

	bool ReadIntegerFromString(const char *pDateString, u32 firstDigit, u32 lastDigit, int &integerToWrite);

public:
//	Public functions
	CDate() { Initialise(); }

	void Initialise();

	bool IsValid();

	bool operator > (const CDate &OtherDate) const;
	bool operator == (const CDate &OtherDate) const;

	bool ExtractDateFromString(const char *pDateString);
	bool ExtractDateFromPhotoFilename(const char *pDateString);
	bool ConstructStringFromDate(char *pStringToContainDate, u32 MaxStringLength);

#if RSG_ORBIS
	bool ExtractDateFromTimeT(const time_t &SlotTime);
#endif

#if __PPU
	static void SetPS3DateFormat();
#endif

	void Set(int in_hour, int in_minute, int in_second,
		int in_day, int in_month, int in_year);

	int GetYear() { return Year; }
	int GetMonth() { return Month; }
	int GetDay() { return Day; }
	int GetHour() { return Hour; }
	int GetMinute() { return Minute; }
	int GetSecond() { return Second; }

#if __XENON
//	Required for hack in CMergedListOfPhotos::FillList() (in savegame_photo_manager.cpp)
	static void Convert2DigitYearTo4Digits(s32 &year);
	static void Convert4DigitYearTo2Digits(s32 &year);
	void SetYear(int newYear) { Year = newYear; }
#endif	//	__XENON
};


#endif	//	SAVEGAME_DATE_H

