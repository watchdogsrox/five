/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LanguageSelect.cpp
// PURPOSE : Offers a choice of languages at start-up if the console language
//           isn't supported and a language hasnt already been selected.
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#include "grprofile/timebars.h"

#include "Frontend/LanguageSelect.h"
#include "audio/frontendaudioentity.h"
#include "Frontend/LoadingScreens.h"
#include "Frontend/NewHud.h"
#include "Frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "Frontend/ui_channel.h"
#include "frontend/scaleform/scaleformmgr.h"
#include "Frontend/WarningScreen.h"
#include "frontend\ButtonEnum.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/Util/ShaderUtil.h"
#include "Renderer/rendertargets.h"
#include "Renderer/MeshBlendManager.h"
#include "System/controlMgr.h"
#include "text/TextFile.h"
#include "Renderer/sprite2d.h"
#include "system/appcontent.h"

FRONTEND_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#if !__FINAL
	PARAM(displaylangselect, "[code] show display language select on startup");
#endif
NOSTRIP_XPARAM(uilanguage);

CScaleformMovieWrapper CLanguageSelect::ms_MovieWrapper;
bool CLanguageSelect::ms_bActive = false;
eLang CLanguageSelect::ms_SelectedLanguage = LANGUAGESEL_UNDEFINED;
s32 CLanguageSelect::ms_PollingId = 0;
bool CLanguageSelect::ms_PollingReturn = false;


#if RSG_PC
eLang CLanguageSelect::ms_SupportedLanguages[] = {
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
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_ja[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_JAPANESE,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_na[] = {
	LANGUAGESEL_UNDEFINED
};
#elif IS_GEN9_PLATFORM
eLang CLanguageSelect::ms_SupportedLanguages[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_FRENCH,
	LANGUAGESEL_GERMAN,
	LANGUAGESEL_ITALIAN,
	LANGUAGESEL_SPANISH,
	LANGUAGESEL_PORTUGUESE,
	LANGUAGESEL_POLISH,
	LANGUAGESEL_RUSSIAN,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_ja[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_JAPANESE,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_na[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_SPANISH,
	LANGUAGESEL_FRENCH,
	LANGUAGESEL_PORTUGUESE,
	LANGUAGESEL_KOREAN,
	LANGUAGESEL_CHINESE,
	LANGUAGESEL_CHINESE_SIMPLIFIED,
	LANGUAGESEL_UNDEFINED
};
#elif RSG_DURANGO
eLang CLanguageSelect::ms_SupportedLanguages[] = {
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
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_ja[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_JAPANESE,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_na[] = {
	LANGUAGESEL_UNDEFINED
};

#else	// orbis and others

eLang CLanguageSelect::ms_SupportedLanguages[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_FRENCH,
	LANGUAGESEL_GERMAN,
	LANGUAGESEL_ITALIAN,
	LANGUAGESEL_SPANISH,
	LANGUAGESEL_PORTUGUESE,
	LANGUAGESEL_POLISH,
	LANGUAGESEL_RUSSIAN,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_ja[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_JAPANESE,
	LANGUAGESEL_UNDEFINED
};
eLang CLanguageSelect::ms_SupportedLanguages_na[] = {
	LANGUAGESEL_ENGLISH,
	LANGUAGESEL_SPANISH,
	LANGUAGESEL_FRENCH,
	LANGUAGESEL_PORTUGUESE,
	LANGUAGESEL_KOREAN,
	LANGUAGESEL_CHINESE,
	LANGUAGESEL_UNDEFINED
};
#endif	// RSG_DURANGO || RSG_PC

bool CLanguageSelect::ShowLanguageSelectScreen()
{
	if( !ShouldActivateAtStartup() )
	{
		return( false );
	}

	if( !LoadLanguageSelectMovie() )
	{
		return( false );
	}

	CLoadingScreens::SleepUntilPastLegalScreens();

	SetLanguages();
	SetActive(true);

	RenderFn originalRenderFunc = gRenderThreadInterface.GetRenderFunction();
	gRenderThreadInterface.SetRenderFunction(MakeFunctor(Render));
	gRenderThreadInterface.Flush();

	ms_SelectedLanguage = LANGUAGESEL_UNDEFINED;
	ms_PollingReturn = false;
	
	while( ms_SelectedLanguage == LANGUAGESEL_UNDEFINED )
	{
		Update();

#if __BANK
		BANKMGR.Update();
#endif
		CScaleformMgr::UpdateAtEndOfFrame(false);
		sysIpcSleep(20);
	}

	gRenderThreadInterface.SetRenderFunction(originalRenderFunc);
	gRenderThreadInterface.Flush();

	RemoveLanguageSelectMovie();
	
	sysLanguage finalLanguage = TranslateScaleformToGame( ms_SelectedLanguage ); 
	CPauseMenu::SetMenuPreference(PREF_CURRENT_LANGUAGE, finalLanguage);
	CProfileSettings& settings = CProfileSettings::GetInstance();
	settings.Set(CProfileSettings::DISPLAY_LANGUAGE, finalLanguage);
	settings.Set(CProfileSettings::NEW_DISPLAY_LANGUAGE, finalLanguage);
	rlSetLanguage(finalLanguage);
	return( true );
}

void CLanguageSelect::SetLanguages()
{
	if( ms_MovieWrapper.BeginMethod( "SET_LANGUAGES" ) )
	{
		eLang* ptr = ms_SupportedLanguages;
		if(sysAppContent::IsJapaneseBuild())
			ptr = ms_SupportedLanguages_ja;
		else if(sysAppContent::IsAmericanBuild())
			ptr = ms_SupportedLanguages_na;
		while( *ptr != LANGUAGESEL_UNDEFINED )
		{
			ms_MovieWrapper.AddParam( (int)*ptr++ );
		}
		ms_MovieWrapper.EndMethod();	
	}
}

//PURPOSE: Return true if the language select screen should be displayed at startup
bool CLanguageSelect::ShouldActivateAtStartup()
{
#if !__FINAL
	if( PARAM_displaylangselect.Get() )
	{
		return( true );
	}
#endif
	// If already have chosen a language don't display language select
	if( PARAM_uilanguage.Get() )
	{
		return( false );
	}

	// If language exists in profile settings then shouldnt activate
	if(CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_LANGUAGE)) 
		return false;

	// get system language
	u32 language = CPauseMenu::GetLanguageFromSystemLanguage();

#if RSG_ORBIS
	if(language == LANGUAGE_CHINESE_SIMPLIFIED)
	{
		return sysAppContent::IsEuropeanBuild();
	}
#endif // RSG_ORBIS

	if(language != LANGUAGE_UNDEFINED)
		return false;

	return true;
}

sysLanguage CLanguageSelect::TranslateScaleformToGame( eLang scaleformLanguage )
{
	sysLanguage retLanguage = LANGUAGE_UNDEFINED;
	switch( scaleformLanguage )
	{
		case LANGUAGESEL_ENGLISH:				retLanguage = LANGUAGE_ENGLISH;break;
		case LANGUAGESEL_FRENCH:				retLanguage = LANGUAGE_FRENCH;break;
		case LANGUAGESEL_GERMAN:				retLanguage = LANGUAGE_GERMAN;break;
		case LANGUAGESEL_ITALIAN:				retLanguage = LANGUAGE_ITALIAN;break;
		case LANGUAGESEL_PORTUGUESE:			retLanguage = LANGUAGE_PORTUGUESE;break;
		case LANGUAGESEL_POLISH:				retLanguage = LANGUAGE_POLISH;break;
		case LANGUAGESEL_RUSSIAN:				retLanguage = LANGUAGE_RUSSIAN;break;
		case LANGUAGESEL_KOREAN:				retLanguage = LANGUAGE_KOREAN;break;
		case LANGUAGESEL_CHINESE:				retLanguage = LANGUAGE_CHINESE_TRADITIONAL;break;
		case LANGUAGESEL_CHINESE_SIMPLIFIED:	retLanguage = LANGUAGE_CHINESE_SIMPLIFIED;break;
		case LANGUAGESEL_JAPANESE:				retLanguage = LANGUAGE_JAPANESE;break;
		case LANGUAGESEL_SPANISH:		
#if __XENON
			{
				u32 systemLocale = XGetLocale();
				switch(systemLocale)
				{
				case XC_LOCALE_ARGENTINA:
				case XC_LOCALE_BRAZIL:
				case XC_LOCALE_CHILE:
				case XC_LOCALE_MEXICO:
				case XC_LOCALE_COLOMBIA:
				case XC_LOCALE_UNITED_STATES:
					retLanguage =  LANGUAGE_MEXICAN;
					break;
				default:
					retLanguage = LANGUAGE_SPANISH;
					break;
				}
			}
#else
			if(sysAppContent::IsAmericanBuild())
				retLanguage = LANGUAGE_MEXICAN;
			else
				retLanguage = LANGUAGE_SPANISH;
#endif
			break;

		default:
		break;
	}
	return( retLanguage );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLanguageSelect::Render()
// PURPOSE: Render's the language select screen
/////////////////////////////////////////////////////////////////////////////////////
#if __PS3
namespace rage {
	extern bool s_NeedWaitFlip;
}
#endif
void CLanguageSelect::Render()
{
#if __PS3
	s_NeedWaitFlip = true;
#endif

	PF_FRAMEINIT_TIMEBARS(0);

	CControlMgr::Update();

	CSystem::BeginRender();
	CRenderTargets::LockUIRenderTargets();
	const grcViewport* pVp = grcViewport::GetDefaultScreen();
	GRCDEVICE.Clear(true, Color32(0,0,0), false, 0, false, 0);
	grcViewport::SetCurrent(pVp);
	grcStateBlock::Default();

	UpdateInput();
	if (IsActive() && ms_MovieWrapper.IsActive())
	{
		ms_MovieWrapper.Render(true);
	}
	
	CRenderTargets::UnlockUIRenderTargets();
	PS3_ONLY(CRenderTargets::Rescale();)
	MeshBlendManager::RenderThreadUpdate();
	CSystem::EndRender();
}

bool CLanguageSelect::AcceptIsCross()
{
	return( CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS) != 0);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLanguageSelect::UpdateInput
// PURPOSE:	Deals with processing of the language select screen
/////////////////////////////////////////////////////////////////////////////////////
bool CLanguageSelect::UpdateInput()
{
	if ( !IsActive() )
	{
		return( false );
	}

	if( ms_PollingReturn )
	{
		if( CScaleformMgr::IsReturnValueSet(ms_PollingId) )
		{
			ms_SelectedLanguage = (eLang)CScaleformMgr::GetReturnValueInt(ms_PollingId);
			ms_PollingReturn = false;
		}
		return( false );
	}

	CPad* pPad = CControlMgr::GetPlayerPad();
	if( pPad == NULL )
	{
		return( false );
	}

	if( pPad->DPadDownJustDown() )
	{
		ms_MovieWrapper.CallMethod("SET_INPUT_EVENT", PAD_DPADDOWN);
		return( true );
	}

	if( pPad->DPadUpJustDown() )
	{
		ms_MovieWrapper.CallMethod("SET_INPUT_EVENT", PAD_DPADUP);
		return( true );
	}

	if( AcceptIsCross() )
	{
		if( pPad->ButtonCrossJustUp() )
		{
			if( ms_MovieWrapper.BeginMethod( "GET_CURRENT_SELECTION" ) )
			{
				ms_PollingId = ms_MovieWrapper.EndMethodReturnValue(ms_PollingId);
				ms_PollingReturn = true;
			}
			return( true );
		}
	}
	else
	{
		if( pPad->ButtonCircleJustUp() )
		{
			if( ms_MovieWrapper.BeginMethod( "GET_CURRENT_SELECTION" ) )
			{
				ms_PollingId = ms_MovieWrapper.EndMethodReturnValue(ms_PollingId);
				ms_PollingReturn = true;
			}
			return( true );
		}
	}
	return( false );
}

// ---------------------------------------------------------
void CLanguageSelect::CheckIncomingFunctions( atHashWithStringBank UNUSED_PARAM(methodName), const GFxValue* UNUSED_PARAM(args) )
{
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLanguageSelect::Update
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CLanguageSelect::Update()
{
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLanguageSelect::LoadLanguageSelectMovie
// PURPOSE:	streams in the movie
/////////////////////////////////////////////////////////////////////////////////////
bool CLanguageSelect::LoadLanguageSelectMovie()
{

#if RSG_ORBIS
	ms_MovieWrapper.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, "LANGUAGE_SELECT_PS4", Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
	return( true );
#elif RSG_DURANGO
	ms_MovieWrapper.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, "LANGUAGE_SELECT_XBOXONE", Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
	return( true );
#elif RSG_PC
	ms_MovieWrapper.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, "LANGUAGE_SELECT_PC", Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
	return( true );
#else
	return( false );
#endif
	
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLanguageSelect::RemoveLanguageSelectMovie
// PURPOSE:	Removes the movie
/////////////////////////////////////////////////////////////////////////////////////
void CLanguageSelect::RemoveLanguageSelectMovie()
{
	if (ms_MovieWrapper.IsActive())
	{
		ms_MovieWrapper.RemoveMovie();
	}
}


