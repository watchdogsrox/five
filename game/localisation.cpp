/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    localisation.cpp
// PURPOSE : stuff that organises all the different versions/types of the game
// AUTHOR :  Derek Payne
// STARTED : 02/06/2003
//
/////////////////////////////////////////////////////////////////////////////////

#include "localisation.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "Text/Textfile.h"
#include "System/language.h"
#include "System/param.h"

NOSTRIP_XPARAM(uilanguage);

bool CLocalisation::m_bForceGermanGame = false;
bool CLocalisation::m_bForceAussieGame = false;
int CLocalisation::m_eForceLanguage = MAX_LANGUAGES;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE  : 
// RETURNS  : NONE
/////////////////////////////////////////////////////////////////////////////////
void CLocalisation::InitWidgets()
{
	const char* languages[] = {
		"American",
		"French",
		"German",
		"Italian",
		"Spanish",
		"Portuguese",
		"Polish",
		"Russian",
		"Korean",
		"ChineseTraditional",
		"Japanese",
		"Mexican",
		"ChineseSimplified",
		"System Default"
	};

	CompileTimeAssert(COUNTOF(languages)==MAX_LANGUAGES-LANGUAGE_UNDEFINED);

	const char* langstr;

	if(PARAM_uilanguage.Get(langstr))
	{
		size_t paramlen = strlen(langstr);

		for(int i=0; i < MAX_LANGUAGES; ++i)
		{
			if(strnicmp(langstr, languages[i], paramlen) == 0)
			{
				m_eForceLanguage = i;
				break;
			}
		}

		if (m_eForceLanguage == MAX_LANGUAGES)
		{
			m_eForceLanguage = TheText.GetLanguageFromName(langstr);
		}

		Assertf(m_eForceLanguage != MAX_LANGUAGES && m_eForceLanguage != LANGUAGE_UNDEFINED, "I don't know how to use -uilanguage=%s! Reverting to system version.", langstr);
	}

	bkBank & bank = BANKMGR.CreateBank("Localisation", 100, 100, false);
	bank.AddToggle("Force German game", &m_bForceGermanGame, NullCB, "");
	bank.AddToggle("Force Australian game", &m_bForceAussieGame, NullCB, "");
	bank.AddToggle("Text Ids only", &TheText.bShowTextIdOnly);
	bank.AddButton("Unload Text",datCallback(MFA(CTextFile::UnloadCore), (datBase*)(&TheText)));
	bank.AddButton("Unload Extra Text",datCallback(MFA(CTextFile::UnloadExtra), (datBase*)(&TheText)));
	bank.AddCombo("Forced Language", &m_eForceLanguage, COUNTOF(languages), languages);
	bank.AddButton("Load Text", datCallback(MFA1(CTextFile::Load), (datBase*)(&TheText), CallbackData(true)));
}

#endif // __BANK
