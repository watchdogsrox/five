/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LanguageSelect.h
// PURPOSE : Offers a choice of languages at start-up if the console language
//           isn't supported and a language hasnt already been selected.
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef LANGUAGESELECT_H_
#define LANGUAGESELECT_H_

#include "atl/hashstring.h"
#include "Scaleform/scaleform.h"
#include "system/language.h"

class CScaleformMovieWrapper;


enum eLang
{
	LANGUAGESEL_UNDEFINED=0,
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_FRENCH,
	LANGUAGESEL_GERMAN,
	LANGUAGESEL_ITALIAN,
	LANGUAGESEL_SPANISH,
	LANGUAGESEL_PORTUGUESE,
	LANGUAGESEL_POLISH,
	LANGUAGESEL_RUSSIAN,
	LANGUAGESEL_KOREAN,
	LANGUAGESEL_CHINESE,
	LANGUAGESEL_CHINESE_SIMPLIFIED,
	LANGUAGESEL_JAPANESE,
	LANGUAGESEL_MEXICAN 
};

//////////////////////////////////////////////////////////////////////////
class CLanguageSelect
{
public:

	static bool ShowLanguageSelectScreen();

	static bool ShouldActivateAtStartup();
	static void SetActive(bool bValue) { ms_bActive = bValue; }
	static bool IsActive() { return ms_bActive; }
	
	static bool LoadLanguageSelectMovie();
	static void RemoveLanguageSelectMovie();
	static bool UpdateInput();
	static void Render();
	static void Update();
	static void CheckIncomingFunctions( atHashWithStringBank methodName, const GFxValue* args );

private:

	static void SetLanguages();
	static bool AcceptIsCross();
	static sysLanguage TranslateScaleformToGame( eLang scaleformLanguage );

	static CScaleformMovieWrapper ms_MovieWrapper;
	static bool ms_bActive;
	static eLang ms_SelectedLanguage;
	static s32 ms_PollingId;
	static bool ms_PollingReturn;
	static eLang ms_SupportedLanguages[];
	static eLang ms_SupportedLanguages_ja[];
	static eLang ms_SupportedLanguages_na[];
};

#endif // LANGUAGESELECT_H_