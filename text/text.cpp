/////////////////////////////////////////////////////////////////////////////////
//
// FILE     :  Text.h
// PURPOSE  :  Main wrapper class for all text functions
// AUTHOR   :  Derek Payne
// STARTED  :  17/04/09
//
/////////////////////////////////////////////////////////////////////////////////

#include "Text/Text.h"

// Framework headers
#include "fwmaths/Rect.h"

// Game headers
#include "core/game.h"
#include "Camera/viewports/ViewportManager.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "renderer/rendertargets.h"
#include "system/param.h"
#include "system/system.h"
#include "Text/TextFile.h"
#include "Text/TextFormat.h"

#if __DEV
#include "Text/TextConversion.h"
#endif

TEXT_OPTIMISATIONS();

PARAM(fontlib, "[text] font library to use (ppefigs, japanese, korrean, russian, trad_chinese, efigs)");

const float HACK_FOR_OLD_SCALE = 0.0555f;  // (now scaled with ui resolution taken into account) // want to get to a point where this value is 0 - but scripters will need to change their stuff

s32 CText::ms_iMovieId = SF_INVALID_MOVIE;
char CText::ms_cFontFilename[100];

#define STANDARD_COLOUR Color32(240,240,240,255)  // matches HUD_COLOUR_WHITE

////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::CTextLayout()
//
// purpose: ensures new layouts are always set up with the default values
////////////////////////////////////////////////////////////////////////////
CTextLayout::CTextLayout()
{
	Default();  // default
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::~CTextLayout()
//
// purpose: Flushes all renderred text
////////////////////////////////////////////////////////////////////////////
CTextLayout::~CTextLayout()
{
	// CTexLayout gets used in the update thread in functions like CTextFormat::GetStringWidth
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		CTextFormat::FlushFontBuffer();
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::Default()
//
// purpose: Apply defaults - quick call we can make to apply generic defaults
//			to the font formatting
/////////////////////////////////////////////////////////////////////////////
void CTextLayout::Default()
{
	SetUseTextColours(true);
	SetLeading(0);
	SetBackgroundColor(Color32(0,0,0,0));
	SetWrap(Vector2(0.0f, 1.0f));
	SetDropShadow(false);
	SetEdge(0.0f);
	SetEdgeCutout(false);
	SetScale(Vector2(1.0f, 1.0f));
	SetColor(STANDARD_COLOUR);  // this cant be a HUDCOLOUR as that code may not of been setup by the time this gets used here.   We can just use the RGB of HUD_COLOUR_WHITE here instead
	SetButtonScale(1.0f);
	SetOrientation(FONT_LEFT);
	SetBackground(false);
	SetBackgroundOutline(false);
	SetStyle(FONT_STYLE_STANDARD);
	SetRenderUpwards(false);
	SetAdjustForNonWidescreen(false);
	SetScissor(0.0f,0.0f,0.0f,0.0f);
#if RSG_PC
	SetEnableMultiheadBoxWidthAdjustment(true);
	SetStereo(0);
#endif
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::SetStyle()
//
// purpose: sets the current style of the font after consulting the text
//			formatter to ensure we set the correct variables for each font
/////////////////////////////////////////////////////////////////////////////
void CTextLayout::SetStyle(s32 iNewStyle)
{
	Style = (u8)iNewStyle;
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::GetCharacterHeight()
//
// purpose: returns the height of a character if rendered on the current
//			layout
/////////////////////////////////////////////////////////////////////////////
float CTextLayout::GetCharacterHeight()
{
	// NOTE: This can be called in the UT or the RT
	return (CTextFormat::GetCharacterHeight(*this));
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::GetTextPadding()
//
// purpose: returns the size of the padding of this text.  This is the gap
//			which is from the position the text is drawn at and the position
//			you can actually see it on screen (Scaleform text has padding)
/////////////////////////////////////////////////////////////////////////////
float CTextLayout::GetTextPadding()
{
#define __GUTTER_ADJUSTER (0.00026f)  // padding of the text (based on the standard font)
	
	return (__GUTTER_ADJUSTER * Scale.y);
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::Render()
//
// purpose: renders the text to the buffer
/////////////////////////////////////////////////////////////////////////////
void CTextLayout::Render(const Vector2 *vPos, const char *pCharacters)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(vPos);
	Assert(pCharacters);

	CTextFormat::AddToTextBuffer(vPos->x, vPos->y, *this, pCharacters WIN32PC_ONLY(, bEnableMultiheadBoxWidthAdjustment));
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::Render()
//
// purpose: sends the details of this layout off to the formatter to be
//			setup to be rendered
/////////////////////////////////////////////////////////////////////////////
void CTextLayout::Render(const Vector2& vPos, const char *pCharacters)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	Assert(pCharacters);

	CTextFormat::AddToTextBuffer(vPos.x, vPos.y, *this, pCharacters WIN32PC_ONLY(, bEnableMultiheadBoxWidthAdjustment));
}


/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::GetNumberOfLines()
//
// purpose: returns the number of seperate lines this text will take up if
//			rendered using the current layout
/////////////////////////////////////////////////////////////////////////////
s32 CTextLayout::GetNumberOfLines(const Vector2 *vPos, const char *pCharacters) const
{
	Assert( vPos );
	Assert(pCharacters);

	// NOTE: This can be called in the UT or the RT
	return (CTextFormat::GetNumberLines(*this, vPos->x, vPos->y, &pCharacters[0]));
}
s32 CTextLayout::GetNumberOfLines(const Vector2& vPos, const char *pCharacters) const
{
	Assert(pCharacters);

	// NOTE: This can be called in the UT or the RT
	return (CTextFormat::GetNumberLines(*this, vPos.x, vPos.y, &pCharacters[0]));
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::GetStringWidthOnScreen()
//
// purpose: returns the rendered length of this string based upon the current
//			layout values
/////////////////////////////////////////////////////////////////////////////
float CTextLayout::GetStringWidthOnScreen(const char *pCharacters, bool bIncludeSpaces) const
{
	Assert(pCharacters);

	// NOTE: This can be called in the UT or the RT
	return (CTextFormat::GetStringWidth(*this, pCharacters, bIncludeSpaces ));
}



/////////////////////////////////////////////////////////////////////////////
// name:	CTextLayout::SetScale()
//
// purpose: set the scale of the text
/////////////////////////////////////////////////////////////////////////////
void CTextLayout::SetScale(const Vector2* vScale)
{
	Scale = Vector2(vScale->x, vScale->y);

	Scale.y *= (HACK_FOR_OLD_SCALE * VideoResManager::GetUIHeight());  // apply the multiplier to bring the text to old GTA font size.

	// setup the size of the buttons to be proportional to the size of the text (and adjust for 16:9)
	ButtonScale = vScale->y*0.75f;
	if (gVpMan.IsUsersMonitorWideScreen())
		ButtonScale *= 0.75f;
}
void CTextLayout::SetScale(const Vector2& vScale)
{
	Scale = vScale;

	Scale.y *= (HACK_FOR_OLD_SCALE * VideoResManager::GetUIHeight());  // apply the multiplier to bring the text to old GTA font size.

	// setup the size of the buttons to be proportional to the size of the text (and adjust for 16:9)
	ButtonScale = vScale.y*0.75f;
	if (gVpMan.IsUsersMonitorWideScreen())
		ButtonScale *= 0.75f;;
}


void CTextLayout::SetAdjustForNonWidescreen(bool bAdjust)
{
	// only allow this to set it to TRUE when in 4:3
	if (!gVpMan.IsUsersMonitorWideScreen())
	{
		bAdjustForNonWidescreen = bAdjust;
	}
	else
	{
		bAdjustForNonWidescreen = false;
	}
}


/////////////////////////////////////////////////////////////////////////////
// name:	CText::Init()
//
// purpose: initialises the whole text system
/////////////////////////////////////////////////////////////////////////////
void CText::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		ms_cFontFilename[0] = '\0';
		ms_cFontFilename[0] = '\0';
		ms_iMovieId = SF_INVALID_MOVIE;
		CText::InitScaleformFonts();
		CTextFormat::Init();
	}
}



/////////////////////////////////////////////////////////////////////////////
// name:	CText::Shutdown()
//
// purpose: shuts down the whole text system
/////////////////////////////////////////////////////////////////////////////
void CText::Shutdown(unsigned shutdownMode)
{
	CTextFormat::Shutdown();

	if (AreScaleformFontsInitialised() && CScaleformMgr::IsMovieActive(ms_iMovieId))
	{
		if (ShutdownScaleformFonts())
		{
			ms_iMovieId = SF_INVALID_MOVIE;
		}
	}

	if (shutdownMode == SHUTDOWN_CORE)
	{
#if __BANK
		ShutdownWidgets();
#endif // __BANK
	}
}



bool CText::AreScaleformFontsInitialised()
{
	return (ms_iMovieId >= 0);  // return true if we have a valid movie id
}



void CText::FilterOutTokensFromString(char *String)
{
	Assert(String);

	CTextFormat::FilterOutTokensFromString(String);
}



void CText::Flush()
{
	if (AreScaleformFontsInitialised())
	{
	#if __DEV
		AssertMsg(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CText - Cannot call this Text function on the update thread!");
	#endif

		CTextFormat::FlushFontBuffer();
	}
}


bool CText::IsAsianLanguage()
{
	switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
	{
		case LANGUAGE_JAPANESE:
		case LANGUAGE_CHINESE_TRADITIONAL:
		case LANGUAGE_CHINESE_SIMPLIFIED:
		case LANGUAGE_KOREAN:
		{
			return true;
		}

		default:
		{
			return false;
		}
	}
}

rlScLanguage CText::GetScLanguageFromCurrentLanguageSetting()
{
	switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
	{
	case LANGUAGE_ENGLISH :
		return RLSC_LANGUAGE_ENGLISH;

	case LANGUAGE_FRENCH :
		return RLSC_LANGUAGE_FRENCH;

	case LANGUAGE_GERMAN :
		return RLSC_LANGUAGE_GERMAN;

	case LANGUAGE_ITALIAN :
		return RLSC_LANGUAGE_ITALIAN;

	case LANGUAGE_SPANISH :
		return RLSC_LANGUAGE_SPANISH;

	case LANGUAGE_PORTUGUESE :
		return RLSC_LANGUAGE_PORTUGUESE_BRAZILIAN;

	case LANGUAGE_POLISH :
		return RLSC_LANGUAGE_POLISH;

	case LANGUAGE_RUSSIAN :
		return RLSC_LANGUAGE_RUSSIAN;

	case LANGUAGE_KOREAN :
		return RLSC_LANGUAGE_KOREAN;

	case LANGUAGE_CHINESE_TRADITIONAL :
		return RLSC_LANGUAGE_CHINESE;

	case LANGUAGE_CHINESE_SIMPLIFIED:
		return RLSC_LANGUAGE_CHINESE_SIMPILIFIED;

	case LANGUAGE_JAPANESE :
		return RLSC_LANGUAGE_JAPANESE;

	case LANGUAGE_MEXICAN :
		return RLSC_LANGUAGE_SPANISH_MEXICAN;

	default :
		Assertf(0, "CText::GetScLanguageFromCurrentLanguageSetting - unsupported language setting %d so returning RLSC_LANGUAGE_ENGLISH", CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE));
	}

	return RLSC_LANGUAGE_ENGLISH;
}

const char *CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting()
{
	static char languagesString[128];

	rlScLanguage nonAsianLanguages[] = {
		RLSC_LANGUAGE_ENGLISH, RLSC_LANGUAGE_FRENCH, RLSC_LANGUAGE_GERMAN,
		RLSC_LANGUAGE_ITALIAN, RLSC_LANGUAGE_SPANISH, RLSC_LANGUAGE_PORTUGUESE_BRAZILIAN,
		RLSC_LANGUAGE_POLISH, RLSC_LANGUAGE_RUSSIAN, RLSC_LANGUAGE_SPANISH_MEXICAN
	};

	rlScLanguage currentLanguage = GetScLanguageFromCurrentLanguageSetting();

	formatf(languagesString, sizeof(languagesString), "'%s'", rlScLanguageToString(currentLanguage));

	switch (currentLanguage)
	{
	case RLSC_LANGUAGE_ENGLISH :
	case RLSC_LANGUAGE_FRENCH :
	case RLSC_LANGUAGE_GERMAN :
	case RLSC_LANGUAGE_ITALIAN :
	case RLSC_LANGUAGE_SPANISH :
    case RLSC_LANGUAGE_PORTUGUESE_BRAZILIAN:
	case RLSC_LANGUAGE_POLISH :
	case RLSC_LANGUAGE_RUSSIAN :
	case RLSC_LANGUAGE_SPANISH_MEXICAN :
		//	Append all other non-Asian languages
		for (u32 loop = 0; loop < NELEM(nonAsianLanguages); loop++)
		{
			if (nonAsianLanguages[loop] != currentLanguage)
			{
				safecat(languagesString, ",'");
				safecat(languagesString, rlScLanguageToString(nonAsianLanguages[loop]));
				safecat(languagesString, "'");
			}
		}
		break;

	case RLSC_LANGUAGE_KOREAN :
	case RLSC_LANGUAGE_CHINESE :
	case RLSC_LANGUAGE_CHINESE_SIMPILIFIED:
	case RLSC_LANGUAGE_JAPANESE :
		//	Append English
		safecat(languagesString, ",'");
		safecat(languagesString, rlScLanguageToString(RLSC_LANGUAGE_ENGLISH));
		safecat(languagesString, "'");
		break;

	default :
		break;
	}

	return languagesString;
}

bool CText::IsScLanguageSupported(rlScLanguage language)
{
    switch(language)
    {
    case RLSC_LANGUAGE_ENGLISH:
    case RLSC_LANGUAGE_FRENCH:
    case RLSC_LANGUAGE_GERMAN:
    case RLSC_LANGUAGE_ITALIAN:
    case RLSC_LANGUAGE_SPANISH:
    case RLSC_LANGUAGE_PORTUGUESE_BRAZILIAN:
    case RLSC_LANGUAGE_POLISH:
    case RLSC_LANGUAGE_RUSSIAN:
    case RLSC_LANGUAGE_SPANISH_MEXICAN:
        return !IsAsianLanguage();
    case RLSC_LANGUAGE_KOREAN:
    case RLSC_LANGUAGE_CHINESE:
	case RLSC_LANGUAGE_CHINESE_SIMPILIFIED:
    case RLSC_LANGUAGE_JAPANESE:
        return (language == GetScLanguageFromCurrentLanguageSetting()) || (language == RLSC_LANGUAGE_ENGLISH);
    default:
        return false;
    }
}

const char *CText::GetLanguageFilename()
{
#if !__FINAL
	const char* pFontLibName = NULL;
	PARAM_fontlib.Get(pFontLibName);

	if (pFontLibName)
	{
		return pFontLibName;
	}
#endif // !__FINAL

	return GetLanguageFilename(CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE));
}


const char *CText::GetLanguageFilename(int language)
{
	switch (language)
	{
		case LANGUAGE_JAPANESE:
		{
			return "japanese";
		}

		case LANGUAGE_CHINESE_TRADITIONAL:
		{
			return "chinese";
		}

		case LANGUAGE_CHINESE_SIMPLIFIED:
		{
#if RSG_ORBIS || RSG_DURANGO
            return "chinese";
#else
            return "chinesesimp";
#endif // RSG_ORBIS || RSG_DURANGO
		}

/*		case LANGUAGE_RUSSIAN:
		case LANGUAGE_POLISH:
		{
			return "russian";
		}*/

		case LANGUAGE_KOREAN:
		{
			return "korean";
		}

		default:
		{
			return "efigs";
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// name:	CText::GetFullFontLibFilename()
// purpose: returns the full platform-specific gfxfontlib filename to use for
//			this build
/////////////////////////////////////////////////////////////////////////////
char *CText::GetFullFontLibFilename()
{
 	if ((ms_cFontFilename[0] != '\0') )  // if the static string has already been set with a name, just return it
 	{
 		return ms_cFontFilename;
 	}

	//
	// this is the define we will probably want to change based on build (ie Japanese etc etc)
	//

	formatf(ms_cFontFilename, "font_lib_%s", GetLanguageFilename(), NELEM(ms_cFontFilename));
		
	//
	// choose platform:
	//
#ifdef SCALEFORM_MOVIE_PLATFORM_SUFFIX
	safecat(ms_cFontFilename, SCALEFORM_MOVIE_PLATFORM_SUFFIX, NELEM(ms_cFontFilename));
#endif

	return ms_cFontFilename;
}


// load and init the scaleform font system
void CText::InitScaleformFonts()
{
	//
	// load movie:
	//
	ms_iMovieId = CScaleformMgr::CreateFontMovie();

	Assertf(ms_iMovieId != SF_INVALID_MOVIE, "Missing or invalid font library - '%s'", GetFullFontLibFilename());
}

bool CText::ShutdownScaleformFonts()
{
	//bool bSuccess = CScaleformMgr::RequestRemoveMovie(ms_iMovieId);
	bool bSuccess = CScaleformMgr::RequestRemoveMovie(ms_iMovieId);
	CScaleformMgr::RemoveFontMovie();

	return bSuccess;
}





//
// DEBUG FUNCTIONS UNDER HERE:
//
#if __BANK

/////////////////////////////////////////////////////////////////////////////
// name:	CText::LoadFontValues()
//
// purpose: a call to reload the values from the fonts.dat file
/////////////////////////////////////////////////////////////////////////////
void CText::LoadFontValues()
{
	//CTextFormat::LoadGenericFontValues();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CText::InitWidgets()
// PURPOSE: inits the ui bank widget and "Create" button
/////////////////////////////////////////////////////////////////////////////////////
void CText::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create Scaleform Font widgets", &CTextFormat::CreateBankWidgets);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CText::ShutdownWidgets()
// PURPOSE: removes the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CText::ShutdownWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (pBank)
	{
		pBank->Destroy();
	}
}

#endif  // __BANK

// EOF
