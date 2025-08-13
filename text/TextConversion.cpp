/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextConversion.cpp
// PURPOSE : class to hold any text conversion functions for the game
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "Text/TextConversion.h"

// framework
#include "fwlocalisation/languagePack.h"
#include "fwlocalisation/templateString.h"

// game
#include "Text/TextFormat.h"
#include "frontend/PauseMenu.h"
#include "Frontend/hud_colour.h"

TEXT_OPTIMISATIONS();

//////////////////////////////////////////////////////////////////////////////
// name:	TextToHtml
//
// purpose: converts a char to html text using the parser & returns as char
//////////////////////////////////////////////////////////////////////////////
char* CTextConversion::TextToHtml(const char *pInString, char *pOutString, s32 iMaxLength)
{
	if (pInString)
	{
		bool bHtmlUsed = false;
		Color32 colour = CHudColour::GetRGBA(HUD_COLOUR_WHITE); // Set default colour to our white hud colour, rather than a hard-coded white

		CTextFormat::ParseToken(&pInString[0], &pOutString[0], true, &colour, 18, &bHtmlUsed, iMaxLength);  // go through, find and replace colour tokens with HTML tags
	}

	return (pOutString);
}

//////////////////////////////////////////////////////////////////////////////
// name:	StripHtml
//
// purpose: strips all text that is wrapped inside ~nrt~ out the text
//////////////////////////////////////////////////////////////////////////////
char* CTextConversion::StripNonRenderableText(char *Dest, const char *Src)
{
	char *dest = Dest;
	char *src = (char*)Src;
	bool bInsideNrt = false;
	s32 iSrcStringSize = fwTextUtil::GetByteCount(Src);
	s32 iCharCount = 0;

#define SIZE_OF_NRT_TOKEN (5) // ~nrt~
	char TokenTest[SIZE_OF_NRT_TOKEN];

	while(Src && *src && iCharCount < iSrcStringSize)
	{
		TokenTest[0] = *(src+1);
		TokenTest[1] = *(src+2);
		TokenTest[2] = *(src+3);
		TokenTest[3] = '\0';

		if (TokenTest[0] == FONT_CHAR_TOKEN_NON_RENDERABLE_TEXT[0] && TokenTest[1] == FONT_CHAR_TOKEN_NON_RENDERABLE_TEXT[1] && TokenTest[2] == FONT_CHAR_TOKEN_NON_RENDERABLE_TEXT[2])
		{
			bInsideNrt = !bInsideNrt;

			src += SIZE_OF_NRT_TOKEN;
			iCharCount += SIZE_OF_NRT_TOKEN;
		}

		if (!bInsideNrt)
		{
			*(dest++) = *src;
			iCharCount ++;
		}

		src++;
	}

	*dest = rage::TEXT_CHAR_TERMINATOR;

	return(Dest);
}

//////////////////////////////////////////////////////////////////////////
void CTextConversion::FormatIntForHumans(s64 iValue, char* pOutValue, int iLength, const char* const pszPrefix /* = "%s" */, bool bAddCurrencyDeliminator)
{
	char const * const currencyDelimiter = ","; //CLocalisation::GetCurrentLanguageCurrencyDelimiter();

	// handle negative numbers
	bool bIsNegative = iValue < 0;
	if(bIsNegative)
		iValue *= -1;

	s64 remainder = iValue % 1000;
	char tempStr[64] = "";
	pOutValue[0] = '\0';

	const char* pAdjPrefix = pszPrefix;
	char pTempPrefix[10] = {0};
	if(bIsNegative)
	{
		pTempPrefix[0] = '-';
		strncpy(pTempPrefix + 1, pszPrefix, 9);
		pAdjPrefix = pTempPrefix;
	}
	
	if (bAddCurrencyDeliminator)
	{
		if(iValue < 1000)
		{
			formatf(tempStr, iLength, "%" I64FMT "d", remainder);
			formatf(pOutValue, iLength, pAdjPrefix, tempStr);
			return;
		}

		formatf(tempStr, iLength, "%03" I64FMT "d", remainder);

		iValue /= 1000;

		while(iValue > 0)
		{
			remainder = iValue % 1000;
			if(iValue < 1000)
			{
				formatf(pOutValue, iLength, "%" I64FMT "d%s%s", remainder, currencyDelimiter, tempStr);
				formatf(tempStr, iLength, "%s", pOutValue);
			}
			else
			{
				formatf(pOutValue, iLength, "%03" I64FMT "d%s%s", remainder, currencyDelimiter, tempStr);
				formatf(tempStr, iLength, "%s", pOutValue);
			}
			iValue /= 1000;
		}
	}
	else
	{
		formatf(tempStr, iLength, ("%" I64FMT "d"), iValue);
	}

	formatf(pOutValue, iLength, pAdjPrefix, tempStr);
}

void CTextConversion::FormatMsTimeAsString( char * out_buffer, size_t const bufferSize, u32 const timeMs, bool roundMs )
{
	if( Verifyf( out_buffer, "CTextConversion::FormatMsTimeAsString - NULL destination buffer" ) &&
		Verifyf( bufferSize > 0, "CTextConversion::FormatMsTimeAsString - Invalid buffer size" ) )
	{
		u32 const c_timeMilliseconds		= timeMs;
		u32 const c_timeMillisecondsPart	= ( c_timeMilliseconds % 1000 ) / (roundMs ? 10 : 1);
		u32 const c_timeSecondsPart			= ( c_timeMilliseconds / 1000 ) % 60;
		u32 const c_timeMinutesPart			= ( c_timeMilliseconds / 1000 ) / 60;

		char const * const c_formatString = roundMs ? "%02u:%02u.%02u" : "%02u:%02u.%03u";
		formatf( out_buffer, bufferSize, c_formatString, c_timeMinutesPart, c_timeSecondsPart, c_timeMillisecondsPart );
	}
}

void CTextConversion::FormatMsTimeAsLongString( char * out_buffer, size_t const bufferSize, u32 const timeMs )
{
	if( Verifyf( out_buffer, "CTextConversion::FormatMsTimeAsLongString - NULL destination buffer" ) &&
		Verifyf( bufferSize > 0, "CTextConversion::FormatMsTimeAsLongString - Invalid buffer size" ) )
	{
		int offset = 0;

		u32 const c_seconds = ( timeMs / 1000 );
		u32 const c_secondsPart = c_seconds % 60;

		u32 const c_minutes = c_seconds / 60;
		u32 const c_minutesPart = c_minutes % 60;

		u32 const c_hoursPart = c_minutes / 60;

		if( c_hoursPart > 0 )
		{
			offset = formatf_n( out_buffer, bufferSize, 
				"%u %s", c_hoursPart, TheText.Get( c_hoursPart == 1 ? "TIMER_HOUR_L" : "TIMER_HOUR_LPL") );
		}

		if( c_minutesPart > 0 )
		{
			offset += formatf_n( out_buffer + offset, bufferSize - offset,
				"%s%u %s", c_hoursPart > 0 ? " " : "",
				c_minutesPart, TheText.Get( c_minutesPart == 1 ? "TIMER_MINUTE_L" : "TIMER_MINUTE_LPL" ) );
		}

		if( c_secondsPart > 0 )
		{
			formatf_n( out_buffer + offset, bufferSize - offset, 
				"%s%u %s", (c_minutesPart > 0 || c_hoursPart > 0) ? " " : "",
				c_secondsPart, TheText.Get( c_secondsPart == 1 ? "TIMER_SEC_L" : "TIMER_SEC_LPL" ) );
		}
	}
}

void CTextConversion::ConvertFileTimeToLongString( char* out_buffer, size_t const bufferSize, u64 const fileTime )
{
	fiDevice::SystemTime sysTime;
	fiDevice::ConvertFileTimeToSystemTime( fileTime , sysTime );
	ConvertSystemTimeToLongString( out_buffer, bufferSize, sysTime );
}

void CTextConversion::ConvertSystemTimeToLongString( char* out_buffer, size_t const bufferSize, fiDevice::SystemTime const & sysTime )
{
	if( uiVerifyf( out_buffer && bufferSize > 0, "CTextConversion::ConvertSystemTimeToLongString - Invalid buffer!" ) )
	{
		sysLanguage const c_currentLanguage = (sysLanguage)CPauseMenu::GetMenuPreference( PREF_CURRENT_LANGUAGE );

		char const * const c_dateDelimiter = fwLanguagePack::GetDateFormatDelimiter( c_currentLanguage );

		fwLanguagePack::DATE_FORMAT const c_dateFormat = fwLanguagePack::GetDateFormatType( c_currentLanguage );

		u32 const c_firstItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? sysTime.wDay :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? sysTime.wMonth : sysTime.wYear;

		u32 const c_secondItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? sysTime.wMonth :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? sysTime.wDay : sysTime.wMonth;

		u32 const c_thirdItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? sysTime.wYear :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? sysTime.wYear : sysTime.wDay;

		formatf_n( out_buffer, bufferSize, "%u%s%u%s%02u - %02u:%02u", 

			c_firstItem, 
			c_dateDelimiter, 
			c_secondItem, 
			c_dateDelimiter, 
			c_thirdItem, 
			
			sysTime.wHour, sysTime.wMinute );
	}
}

void CTextConversion::ConvertFileTimeToString( char* out_buffer, size_t const bufferSize, bool const shorthandMonth, bool const shorthandDay, u64 const fileTime )
{
	fiDevice::SystemTime sysTime;
	fiDevice::ConvertFileTimeToSystemTime( fileTime , sysTime );
	ConvertSystemTimeToString( out_buffer, bufferSize, shorthandMonth, shorthandDay, sysTime );
}

void CTextConversion::ConvertSystemTimeToString( char* out_buffer, size_t const bufferSize, bool const shorthandMonth, bool const shorthandDay, 
												fiDevice::SystemTime const & sysTime )
{
	if( uiVerifyf( out_buffer && bufferSize > 0, "CTextConversion::ConvertSystemTimeToString - Invalid buffer!" ) )
	{
		sysLanguage const c_currentLanguage = (sysLanguage)CPauseMenu::GetMenuPreference( PREF_CURRENT_LANGUAGE );
		char const * const c_dateDelimiter = fwLanguagePack::GetDateFormatDelimiter( c_currentLanguage );

		fwLanguagePack::DATE_FORMAT const c_dateFormat = fwLanguagePack::GetDateFormatType( c_currentLanguage );

		LocalizationKey c_monthLngKey;
		fwLanguagePack::GetMonthString( c_monthLngKey.getBufferRef(), sysTime.wMonth, shorthandMonth, false );

		LocalizationKey c_dayLngKey;
		fwLanguagePack::GetDayString( c_monthLngKey.getBufferRef(), sysTime.wDayOfWeek, shorthandDay );

		SimpleString_16 c_yearString;
        c_yearString.sprintf( "%02u", sysTime.wYear );

		char const * const c_firstItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? TheText.Get( c_dayLngKey.getBuffer() ) :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? TheText.Get( c_monthLngKey.getBuffer() ) : c_yearString.getBuffer();

		char const * const c_secondItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? TheText.Get( c_monthLngKey.getBuffer() ) :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? TheText.Get( c_dayLngKey.getBuffer() ) : TheText.Get( c_monthLngKey.getBuffer() );

		char const * const c_thirdItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? c_yearString.getBuffer() : 
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? c_yearString.getBuffer() : TheText.Get( c_dayLngKey.getBuffer() );

		formatf_n( out_buffer, bufferSize, "%s%s%s%s%s", 
			
			c_firstItem, 
			c_dateDelimiter,
			c_secondItem, 
			c_dateDelimiter,
			c_thirdItem );
	}
}

void CTextConversion::ConvertFileTimeToFileNameString( char* out_buffer, size_t const bufferSize, u64 const fileTime )
{
	fiDevice::SystemTime sysTime;
	fiDevice::ConvertFileTimeToSystemTime( fileTime , sysTime );
	ConvertSystemTimeToFileNameString( out_buffer, bufferSize, sysTime );
}

void CTextConversion::ConvertSystemTimeToFileNameString( char* out_buffer, size_t const bufferSize, fiDevice::SystemTime const & sysTime )
{
	if( uiVerifyf( out_buffer && bufferSize > 0, "CTextConversion::ConvertSystemTimeToFileNameString - Invalid buffer!" ) )
	{
		sysLanguage const c_currentLanguage = (sysLanguage)CPauseMenu::GetMenuPreference( PREF_CURRENT_LANGUAGE );
		fwLanguagePack::DATE_FORMAT const c_dateFormat = fwLanguagePack::GetDateFormatType( c_currentLanguage );

		LocalizationKey c_monthLngKey;
		fwLanguagePack::GetMonthString( c_monthLngKey.getBufferRef(), sysTime.wMonth, true, false );

		SimpleString_16 c_dayString;
        c_dayString.sprintf( "%u", sysTime.wDay );
		SimpleString_16 c_yearString;
        c_yearString.sprintf( "%02u", sysTime.wYear );

		char const * const c_firstItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? c_dayString.getBuffer() :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? TheText.Get( c_monthLngKey.getBuffer() ) : c_yearString.getBuffer();

		char const * const c_secondItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? TheText.Get( c_monthLngKey.getBuffer() ) :
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? c_dayString.getBuffer() : TheText.Get( c_monthLngKey.getBuffer() );

		char const * const c_thirdItem = c_dateFormat == fwLanguagePack::DATE_FORMAT_DMY ? c_yearString.getBuffer() : 
			c_dateFormat == fwLanguagePack::DATE_FORMAT_MDY ? c_yearString.getBuffer() : c_dayString.getBuffer();

		formatf_n( out_buffer, bufferSize, "%s-%s-%s", 

			c_firstItem, 
			c_secondItem, 
			c_thirdItem );
	}
}


size_t CTextConversion::ConvertToWideChar( const char* szString, wchar_t* wszString, size_t length )
{
	size_t returnSize = 0;

	if( uiVerifyf( szString, "CTextConversion::ConvertToWideChar - Null source string!" ) 
		&& uiVerifyf( wszString && length > 0, "CMontage::CTextConversion::ConvertToWideChar - Null buffer!" ) )
	{		
		size_t lengthString = strlen(szString);

		if( uiVerifyf( lengthString < length, "CMontage::GetNameW - Invalid buffer size %" SIZETFMT "u, needs %" SIZETFMT "u", 
			length, lengthString + 1 ) )
		{
			
#if defined(__RUSSIAN_BUILD) && __RUSSIAN_BUILD
		char szRussianName[RAGE_MAX_PATH];

		// convert to Russian 1251 code page.
		CFont::ConvertToRussian(szString, szRussianName, RAGE_MAX_PATH);

		memset(wszString, 0, length*sizeof(wchar_t));
		MultiByteToWideChar(1251, MB_PRECOMPOSED, szRussianName, lengthString, wszString, length);
#else
		/*TODO4FIVE
		if (!CLocalisation::IsCurrentLanguageRussian())*/
		{
			::mbstowcs_s(&returnSize, wszString, length, szString, lengthString);
		}
		/*TODO4FIVE
		else
		{
			char szRussianName[RAGE_MAX_PATH];

			// convert to Russian 1251 code page.
			CFont::ConvertToRussian(szString, szRussianName, RAGE_MAX_PATH);

			memset(wszString, 0, length*sizeof(WCHAR));
			MultiByteToWideChar(1251, MB_PRECOMPOSED, szRussianName, lengthString, wszString, length);
		}*/
#endif	// __RUSSIAN_BUILD

		}
	}

	return returnSize;
}
