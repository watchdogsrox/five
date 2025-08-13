/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextConversion.h
// PURPOSE : class to hold any text conversion functions for the game
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef TEXTCONVERSION_H
#define TEXTCONVERSION_H

// Rage Includes:
#include "file/device.h"
#include "string/unicode.h"

// Framework includes
#include "fwlocalisation/textUtil.h"

class CTextConversion : public fwTextUtil
{
public:
	static char* TextToHtml(const char *pInString, char *pOutString, s32 iMaxLength);
	static char* StripNonRenderableText(char *Dest, const char *Src);

	static void FormatIntForHumans( s64 iValue, char* pOutValue, int iLength, const char* const pszPrefix = "%s", bool bAddCurrencyDeliminator = true);

	// TODO - JamesS move these into localization system when brought back from RDR. Technically into own class of unit conversion
	static void FormatMsTimeAsString( char * out_buffer, size_t const bufferSize, u32 const timeMs, bool roundMs = true );
	static void FormatMsTimeAsLongString( char * out_buffer, size_t const bufferSize, u32 const timeMs );

	static void ConvertFileTimeToLongString( char* out_buffer, size_t const bufferSize, u64 const fileTime );
	static void ConvertSystemTimeToLongString( char* out_buffer, size_t const bufferSize, fiDevice::SystemTime const & sysTime );

	static void ConvertFileTimeToString( char* out_buffer, size_t const bufferSize, bool const shorthandMonth, bool const shorthandDay, u64 const fileTime );
	static void ConvertSystemTimeToString( char* out_buffer, size_t const bufferSize, bool const shorthandMonth, bool const shorthandDay,
		fiDevice::SystemTime const & sysTime );

	static void ConvertFileTimeToFileNameString( char* out_buffer, size_t const bufferSize, u64 const fileTime );
	static void ConvertSystemTimeToFileNameString( char* out_buffer, size_t const bufferSize, fiDevice::SystemTime const & sysTime );

	static size_t ConvertToWideChar( const char* szString, wchar_t* wszString, size_t length );
};

#endif // TEXTCONVERSION_H
