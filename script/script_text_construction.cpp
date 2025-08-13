
#include "script/script_text_construction.h"


// Game headers
#include "frontend/MiniMap.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/BusySpinner.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetGamePlayer.h"
#include "scene/World/GameWorld.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "text/TextFormat.h"

SCRIPT_OPTIMISATIONS()

const char *CScriptTextConstruction::m_pMainTextLabel = NULL;

CNumberWithinMessage CScriptTextConstruction::m_Numbers[MaxNumberOfNumbersInPrintCommand];
CSubStringWithinMessage CScriptTextConstruction::m_SubStrings[MaxNumberOfSubStringsInPrintCommand];

u32 CScriptTextConstruction::m_NumberOfNumbers;
u32 CScriptTextConstruction::m_NumberOfSubStringTextLabels;

eTextConstruction CScriptTextConstruction::m_eTextConstructionCommand = TEXT_CONSTRUCTION_NONE;

eHUD_COLOURS CScriptTextConstruction::ms_eColourOfNextTextComponent = HUD_COLOUR_INVALID;


void CScriptTextConstruction::Clear()
{
	m_pMainTextLabel = NULL;

	u32 loop = 0;
	for (loop = 0; loop < MaxNumberOfNumbersInPrintCommand; loop++)
	{
		m_Numbers[loop].Clear();
	}

	for (loop = 0; loop < MaxNumberOfSubStringsInPrintCommand; loop++)
	{
		m_SubStrings[loop].Clear(false, false);
	}

	m_NumberOfNumbers = 0;
	m_NumberOfSubStringTextLabels = 0;
}


void CScriptTextConstruction::BeginPrint(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginPrint - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_PRINT);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndPrint(s32 Duration, bool bPrintNow)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_PRINT, "CScriptTextConstruction::EndPrint - CScriptTextConstruction::BeginPrint hasn't been called"))
	{
		char *pMainString = TheText.Get(m_pMainTextLabel);
		s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

		CMessages::AddMessage(pMainString, MainTextBlock, 
			Duration, bPrintNow, CScriptHud::GetAddNextMessageToPreviousBriefs(), CScriptHud::GetNextMessagePreviousBriefOverride(), 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			false);

		CScriptHud::SetAddNextMessageToPreviousBriefs(true);
		CScriptHud::SetNextMessagePreviousBriefOverride(PREVIOUS_BRIEF_NO_OVERRIDE);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}

void CScriptTextConstruction::BeginBusySpinnerOn(const char* pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginBusySpinner - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_BUSYSPINNER);
		Clear();
		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndBusySpinnerOn( int Icon )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_BUSYSPINNER, "CScriptTextConstruction::EndBusySpinner - CScriptTextConstruction::BeginBusySpinner hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
		CBusySpinner::On( FinalString, Icon, SPINNER_SOURCE_SCRIPT );	
	}
}

void CScriptTextConstruction::BeginOverrideButtonText(const char* pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginOverrideButtonText - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_BUTTON_OVERRIDE);
		Clear();
		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndOverrideButtonText(int iSlotIndex)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_BUTTON_OVERRIDE, "CScriptTextConstruction::EndOverrideButtonText - CScriptTextConstruction::BeginOverrideButtonText hasn't been called"))
	{
		DynamicPauseMenu* pMenu = CPauseMenu::GetDynamicPauseMenu();
		if( SCRIPT_VERIFY(pMenu, "Can't call END_OVERRIDE_BUTTON_TEXT while the pause menu's not active!"))
		{
			char *pMainString = TheText.Get(m_pMainTextLabel);

			CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
				m_Numbers, m_NumberOfNumbers, 
				m_SubStrings, m_NumberOfSubStringTextLabels, 
				pMenu->GetTimerMessage(), pMenu->GetTimerMessageSize() );

			CPauseMenu::OverrideButtonText( iSlotIndex, pMenu->GetTimerMessage() );
		}
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}

void CScriptTextConstruction::BeginTheFeedPost(const char* pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginTheFeedPost - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_THEFEED_POST);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

int CScriptTextConstruction::EndTheFeedPostStats( const char* Title, int LevelTotal, int LevelCurrent, bool IsImportant, const char* ContactTxD, const char* ContactTxN )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostStats - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostStats( TheText.Get(Title), FinalString, LevelTotal, LevelCurrent, IsImportant, ContactTxD, ContactTxN ) );
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostMessageText( const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier/*=1.0f*/, const char* CrewTagPacked/*=""*/, int Icon2/*=0*/, int iHudColor  )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostMessageText - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		//	Graeme - Colin Considine needs to send an email containing a list of ten animals for his Wildlife Photography script
		//	The email already uses MAX_CHARS_FOR_TEXT_STRING in EndScaleformString()
		//	The feed currently displays the entire email so I'll try also using MAX_CHARS_FOR_TEXT_STRING here instead of (2 * MAX_CHARS_IN_MESSAGE)
		char StrippedString[MAX_CHARS_FOR_TEXT_STRING];
		char FinalString[MAX_CHARS_FOR_TEXT_STRING];
		char *pMainString = TheText.Get(m_pMainTextLabel);
		
		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CTextConversion::StripNonRenderableText(StrippedString, FinalString);  // remove the text wrapped in ~nrt~ so we dont show image or text relating to it (ie "here is the photo").

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			const char* pLocCharName = TheText.DoesTextLabelExist(CharacterName) ? TheText.Get(CharacterName) : CharacterName; // Script is currently sending both localised, and unlocalised strings.  This is a temporary catch all until they fix their shit
			return( GameStream->PostMessageText( StrippedString, ContactTxD, ContactTxN, IsImportant, Icon, pLocCharName, Subtitle, timeMultiplier, CrewTagPacked, Icon2, -1, iHudColor ) );
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostTicker( bool IsImportant, bool bCacheMessage, bool bHasTokens )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostTicker - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char* pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostTicker( FinalString, IsImportant, bCacheMessage, bHasTokens ) );
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostTickerF10( const char* TopLine, bool IsImportant, bool bCacheMessage )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostTicker - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostTickerF10( TopLine, FinalString, IsImportant, bCacheMessage ) );
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostAward( const char* TxD, const char* TxN, int iXP, eHUD_COLOURS eAwardColour, const char* Title)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostAward - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostAward(FinalString, TxD, TxN, iXP, eAwardColour, Title ? TheText.Get(Title) : ""));
		}
	}
	return( -1 );

}

int CScriptTextConstruction::EndTheFeedPostCrewTag( bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int CrewRank, bool FounderStatus, bool IsImportant, int crewId, const char* GameName, int crewColR, int crewColG, int crewColB)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostCrewTag - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );
	
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostCrewTag( IsPrivate, ShowLogoFlag, CrewString, CrewRank, FounderStatus, FinalString, IsImportant, (rlClanId) crewId, GameName, Color32((u8)crewColR, (u8)crewColG, (u8)crewColB) ));
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostUnlock( const char* chTitle, int iIconType, bool bIsImportant, eHUD_COLOURS eTitleColour, bool bTitleIsLiteral)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostUnlock - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostUnlock(bTitleIsLiteral? chTitle : TheText.Get(chTitle), FinalString, iIconType, bIsImportant, eTitleColour));
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostCrewRankup(const char* chSubtitle, const char* chTXD, const char* chTXN, bool bIsImportant, bool bSubtitleIsLiteral)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostCrewRankup - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			// NOTE: cSubtitle requires no translation
			return( GameStream->PostCrewRankup( FinalString, bSubtitleIsLiteral ? chSubtitle : TheText.Get(chSubtitle), chTXD, chTXN, bIsImportant));
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostVersus( const char* ch1TXD, const char* ch1TXN, int val1, const char* ch2TXD, const char* ch2TXN, int val2, eHUD_COLOURS iCustomColor1, eHUD_COLOURS iCustomColor2 )
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostVersus - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* GameStream = CGameStreamMgr::GetGameStream();
		if( GameStream != NULL )
		{
			return( GameStream->PostVersus( ch1TXD, ch1TXN, val1, ch2TXD, ch2TXN, val2, iCustomColor1, iCustomColor2));
		}
	}
	return( -1 );
}

int CScriptTextConstruction::EndTheFeedPostReplay(CGameStream::eFeedReplayState replayState, int iIcon, const char* cSubtitle)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostReplay - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			FinalString, NELEM(FinalString) );

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if( pGameStream )
		{
			return pGameStream->PostReplay( replayState, pMainString, cSubtitle, iIcon );
		}
	}

	return -1;
}

int CScriptTextConstruction::EndTheFeedPostReplayInput(CGameStream::eFeedReplayState replayState, const char* cIcon, const char* cSubtitle)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_THEFEED_POST, "CScriptTextConstruction::EndTheFeedPostReplayInput - CScriptTextConstruction::BeginTheFeedPost hasn't been called"))
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString,
			m_Numbers, m_NumberOfNumbers,
			m_SubStrings, m_NumberOfSubStringTextLabels,
			FinalString, NELEM(FinalString) );
		
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);

		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if (pGameStream)
		{
			return pGameStream->PostReplay( replayState, pMainString, cSubtitle, 0, 0.0f, false, cIcon );
		}
	}

	return -1;
}

void CScriptTextConstruction::BeginDisplayText(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginDisplayText - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_DISPLAY_TEXT);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

Vector2 CScriptTextConstruction::CalculateTextPosition(Vector2 vPos)
{
	Vector2 vSize(CScriptHud::ms_FormattingForNextDisplayText.GetWidth(), CScriptHud::ms_FormattingForNextDisplayText.GetHeight());
	Vector2 vNewPos = CScriptHud::ms_CurrentScriptGfxAlignment.CalculateHudPosition(vPos, vSize);

	// offset margins
	CScriptHud::ms_FormattingForNextDisplayText.OffsetMargins(vNewPos.x - vPos.x);

	return vNewPos;
}

void CScriptTextConstruction::EndDisplayText(float DisplayAtX, float DisplayAtY, bool NOTFINAL_ONLY(bForceUseDebugText), int Stereo)
{
#if __DEV
	AssertMsg((CSystem::IsThisThreadId(SYS_THREAD_UPDATE)), "CScriptTextConstruction::EndDisplayText - Only on update!");
#endif

	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_DISPLAY_TEXT, "CScriptTextConstruction::EndDisplayText - CScriptTextConstruction::BeginDisplayText hasn't been called"))
	{
		bool bUseDebugDisplayTextLines = false;
#if !__FINAL
		if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "CScriptTextConstruction::EndDisplayText - not called by a script command"))
		{
			if (CTheScripts::GetCurrentGtaScriptThread()->UseDebugPortionOfScriptTextArrays())
			{
				bUseDebugDisplayTextLines = true;
			}
		}

		if (bForceUseDebugText)
		{
			bUseDebugDisplayTextLines = true;
		}
#endif	//	!__FINAL

		if (SCRIPT_VERIFY(m_pMainTextLabel, "CScriptTextConstruction::EndDisplayText - main text label is a NULL pointer"))
		{
			CScriptHud::ms_FormattingForNextDisplayText.SetScriptWidescreenFormat(CScriptHud::CurrentScriptWidescreenFormat, CScriptHud::bScriptHasChangedWidescreenFormat);
			Vector2 pos = CalculateTextPosition(Vector2(DisplayAtX, DisplayAtY));

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::EndDisplayText - m_pMainTextLabel=%s, x=%f, y=%f, m_NumberOfNumbers=%u, m_NumberOfSubStringTextLabels=%u",
					m_pMainTextLabel, pos.x, pos.y, m_NumberOfNumbers, m_NumberOfSubStringTextLabels);
			}
#endif	//	__BANK

#if RSG_PC
			CScriptHud::ms_FormattingForNextDisplayText.SetStereo(Stereo);
#else
			(void)Stereo;
#endif
			CScriptHud::GetIntroTexts().GetWriteBuf().AddTextLine(pos.x, pos.y, CScriptHud::scriptTextRenderID, CScriptHud::ms_IndexOfDrawOrigin,
				CScriptHud::ms_iCurrentScriptGfxDrawProperties, CScriptHud::ms_FormattingForNextDisplayText, 
				m_pMainTextLabel, 
				m_Numbers, m_NumberOfNumbers, 
				m_SubStrings, m_NumberOfSubStringTextLabels, 
				bUseDebugDisplayTextLines);

#if __BANK
			CScriptHud::ms_FormattingForNextDisplayText.OutputDebugInfo(true, pos.x, pos.y);
#endif // __BANK
		}

		CScriptHud::ms_FormattingForNextDisplayText.Reset();

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}


void CScriptTextConstruction::BeginIsMessageDisplayed(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginIsMessageDisplayed - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_IS_MESSAGE_DISPLAYED);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

bool CScriptTextConstruction::EndIsMessageDisplayed()
{
	bool LatestCmpFlagResult =  false;

	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_IS_MESSAGE_DISPLAYED, "CScriptTextConstruction::EndIsMessageDisplayed - CScriptTextConstruction::BeginIsMessageDisplayed hasn't been called"))
	{
		if (!CMessages::BriefMessages[0].IsEmpty())
		{
			char TestString[2 * MAX_CHARS_IN_MESSAGE];

			char *pMainString = TheText.Get(m_pMainTextLabel);

			CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
				m_Numbers, m_NumberOfNumbers, 
				m_SubStrings, m_NumberOfSubStringTextLabels, 
				TestString, NELEM(TestString) );

#if __WIN32PC
			CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

			char TextAtHeadOfSubtitleQueue[2 * MAX_CHARS_IN_MESSAGE];

			CMessages::BriefMessages[0].FillInString(TextAtHeadOfSubtitleQueue, NELEM(TextAtHeadOfSubtitleQueue));

			LatestCmpFlagResult = ( strcmp(TestString, TextAtHeadOfSubtitleQueue) == 0 );
		}

		// Fix for GTA5 bug 2152571
		//	Ensure ClearArrayOfImmediateUseLiteralStrings() gets called.
		//	It's usually called by InsertNumbersAndSubStringsIntoString() but that won't get called if BriefMessages[0].IsEmpty()
		CScriptHud::ScriptLiteralStrings.ClearArrayOfImmediateUseLiteralStrings(false);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}

	return LatestCmpFlagResult;
}


void CScriptTextConstruction::BeginGetScreenWidthOfDisplayText(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginGetScreenWidthOfDisplayText - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

float CScriptTextConstruction::EndGetScreenWidthOfDisplayText(bool bIncludeSpaces)
{
	float fReturnValue = 0.0f;

	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT, "CScriptTextConstruction::EndGetScreenWidthOfDisplayText - CScriptTextConstruction::BeginGetScreenWidthOfDisplayText hasn't been called"))
	{
		char TestString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			TestString, NELEM(TestString) );

#if __WIN32PC
		CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

		fReturnValue = CScriptHud::ms_FormattingForNextDisplayText.GetStringWidthOnScreen(TestString, bIncludeSpaces);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}

	return fReturnValue;
}


void CScriptTextConstruction::BeginGetNumberOfLinesForString(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginGetNumberOfLinesForString - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_GET_NUMBER_OF_LINES_FOR_DISPLAY_TEXT);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

s32 CScriptTextConstruction::EndGetNumberOfLinesForString(float DisplayAtX, float DisplayAtY)
{
	s32 ReturnNumberOfLines = 0;
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_GET_NUMBER_OF_LINES_FOR_DISPLAY_TEXT, "CScriptTextConstruction::EndGetNumberOfLinesForString - CScriptTextConstruction::BeginGetNumberOfLinesForString hasn't been called"))
	{
		char TestString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			TestString, NELEM(TestString) );

#if __WIN32PC
		CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

		CScriptHud::ms_FormattingForNextDisplayText.SetScriptWidescreenFormat(CScriptHud::CurrentScriptWidescreenFormat, CScriptHud::bScriptHasChangedWidescreenFormat);
		Vector2 pos = CalculateTextPosition(Vector2(DisplayAtX, DisplayAtY));

		ReturnNumberOfLines = CScriptHud::ms_FormattingForNextDisplayText.GetNumberOfLinesForString(pos.x, pos.y, TestString);

#if __BANK
		if (CTextFormat::ms_cDebugScriptedString[0] != '\0')
		{
			if (!stricmp(m_pMainTextLabel, CTextFormat::ms_cDebugScriptedString))
			{
				Displayf("\n\n*****************************************\n");
				Displayf("GET_NUMBER_LINES is returning %d for %s\n", ReturnNumberOfLines, m_pMainTextLabel);
				Displayf("*****************************************\n\n\n");
			}
		}
#endif // __BANK

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}

	return ReturnNumberOfLines;
}


void CScriptTextConstruction::BeginDisplayHelp(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginDisplayHelp - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_DISPLAY_HELP);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndDisplayHelp(s32 iHelpId, bool bDisplayForever, bool bPlaySound, s32 OverrideDuration)
{
	scriptDebugf1("PRINT_HELP_... called by %s script", CTheScripts::GetCurrentScriptName());

	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_DISPLAY_HELP, "CScriptTextConstruction::EndDisplayHelp - CScriptTextConstruction::BeginDisplayHelp hasn't been called"))
	{
		bool bAddHelpMessage = true;
		if (iHelpId == HELP_TEXT_SLOT_STANDARD)
		{
			bAddHelpMessage = false;
			if (CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript || (CTheScripts::GetNumberOfMiniGamesInProgress() == 0) || CScriptHud::ShouldNonMiniGameHelpMessagesBeDisplayed() )
			{
				bAddHelpMessage = true;
			}
		}

		if (bAddHelpMessage)
		{
			char *pMainString = TheText.Get(m_pMainTextLabel);
			s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

			//Displayf("PRINT_HELP called by %s script", CTheScripts::GetCurrentScriptName());

			CHelpMessage::SetMessageText(iHelpId, pMainString, 
				m_Numbers, m_NumberOfNumbers, 
				m_SubStrings, m_NumberOfSubStringTextLabels, 
				bPlaySound, bDisplayForever, OverrideDuration);

			if (CScriptHud::GetAddNextMessageToPreviousBriefs())
			{
				CSubtitleMessage PreviousBrief;

				PreviousBrief.Set(pMainString, MainTextBlock, 
					m_Numbers, m_NumberOfNumbers, 
					m_SubStrings, m_NumberOfSubStringTextLabels, 
					false, true);

				CMessages::AddToPreviousBriefArray(PreviousBrief, CScriptHud::GetNextMessagePreviousBriefOverride());
			}
		}
		else
		{	//	If we've added any literal strings for this message and then don't actually display the message
			//	then remove the literal strings from the array. The literals will actually only have been added to
			//	the persistent array if CScriptHud::GetAddNextMessageToPreviousBriefs() returned true
			for (u32 sub_string_loop = 0; sub_string_loop < m_NumberOfSubStringTextLabels; sub_string_loop++)
			{
				m_SubStrings[sub_string_loop].Clear(true, false);
			}
		}

		CScriptHud::SetAddNextMessageToPreviousBriefs(true);
		CScriptHud::SetNextMessagePreviousBriefOverride(PREVIOUS_BRIEF_NO_OVERRIDE);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}


void CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_IS_HELP_MESSAGE_DISPLAYED);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

bool CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(s32 iHelpId)
{
	bool LatestCmpFlagResult = false;

	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_IS_HELP_MESSAGE_DISPLAYED, "CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed - CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed hasn't been called"))
	{
		char TestString[2 * MAX_CHARS_IN_MESSAGE];
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			TestString, NELEM(TestString) );

#if __WIN32PC
		CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

		LatestCmpFlagResult = ( strcmp(TestString, CHelpMessage::GetMessageText(iHelpId)) == 0 );
		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}

	return LatestCmpFlagResult;
}


void CScriptTextConstruction::BeginScaleformString(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginScaleformString - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_SCALEFORM_STRING);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndScaleformString(bool bConvertToHtml)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_SCALEFORM_STRING, "CScriptTextConstruction::EndScaleformString - CScriptTextConstruction::BeginScaleformString hasn't been called"))
	{
		char TestString[MAX_CHARS_FOR_TEXT_STRING];  // needs to use scaleform string, not the messsage string as we may be dealing with very long strings here passing into scaleform, especially in other languages
		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			TestString, NELEM(TestString) );

#if __WIN32PC
		CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

		CScaleformMgr::AddParamString(TestString, bConvertToHtml);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}

void CScriptTextConstruction::BeginRowAbovePlayersHead(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(CScriptHud::ms_AbovePlayersHeadDisplay.IsConstructing(), "CScriptTextConstruction::BeginRowAbovePlayersHead - you have to call BEGIN_DISPLAY_PLAYER_NAME first"))
	{
		if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginRowAbovePlayersHead - can't call this while an earlier text command is still open. Call End first"))
		{
			SetTextConstructionCommand(TEXT_CONSTRUCTION_ROW_ABOVE_PLAYERS_HEAD);

			Clear();

			m_pMainTextLabel = pMainTextLabel;
		}
	}
}


void CScriptTextConstruction::EndRowAbovePlayersHead()
{
	if (SCRIPT_VERIFY(CScriptHud::ms_AbovePlayersHeadDisplay.IsConstructing(), "CScriptTextConstruction::EndRowAbovePlayersHead - you have to call BEGIN_DISPLAY_PLAYER_NAME first"))
	{
		if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_ROW_ABOVE_PLAYERS_HEAD, "CScriptTextConstruction::EndRowAbovePlayersHead - CScriptTextConstruction::BeginRowAbovePlayersHead hasn't been called"))
		{
			char GxtRowString[CNetObjPlayer::MAX_PLAYER_DISPLAY_NAME];

			char *pMainString = TheText.Get(m_pMainTextLabel);

			CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
				m_Numbers, m_NumberOfNumbers, 
				m_SubStrings, m_NumberOfSubStringTextLabels, 
				GxtRowString, NELEM(GxtRowString) );

#if __WIN32PC
			CMessages::InsertPlayerControlKeysInString(&GxtRowString[0]);
#endif // __WIN32PC

			CScriptHud::ms_AbovePlayersHeadDisplay.AddRow(GxtRowString);

			SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
		}
	}
}



void CScriptTextConstruction::BeginSetBlipName(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginSetBlipName - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_SET_BLIP_NAME);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndSetBlipName(s32 iBlipIndex)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_SET_BLIP_NAME, "CScriptTextConstruction::EndSetBlipName - CScriptTextConstruction::BeginSetBlipName hasn't been called"))
	{
		char TestString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = TheText.Get(m_pMainTextLabel);

		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			TestString, NELEM(TestString) );

#if __WIN32PC
		CMessages::InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC

		if (SCRIPT_VERIFY(CTextConversion::GetByteCount(TestString) < MAX_BLIP_NAME_SIZE, "CScriptTextConstruction::EndSetBlipName - string is too long for a blip name"))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME_FROM_ASCII, iBlipIndex, TestString);
		}

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}


void CScriptTextConstruction::BeginAddDirectlyToPreviousBriefs(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginAddDirectlyToPreviousBriefs - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}

void CScriptTextConstruction::EndAddDirectlyToPreviousBriefs(bool bUsesUnderscore)
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS, "CScriptTextConstruction::EndAddDirectlyToPreviousBriefs - CScriptTextConstruction::BeginAddDirectlyToPreviousBriefs hasn't been called"))
	{
		char *pMainString = TheText.Get(m_pMainTextLabel);
		s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

		CSubtitleMessage NewPreviousBrief;
		NewPreviousBrief.Set(pMainString, MainTextBlock, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			bUsesUnderscore, false);

		CMessages::AddToPreviousBriefArray(NewPreviousBrief, CScriptHud::GetNextMessagePreviousBriefOverride());

		CScriptHud::SetNextMessagePreviousBriefOverride(PREVIOUS_BRIEF_NO_OVERRIDE);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}

void CScriptTextConstruction::BeginClearPrint(const char *pMainTextLabel)
{
	if (SCRIPT_VERIFY(!IsConstructingText(), "CScriptTextConstruction::BeginClearPrint - can't call this while an earlier text command is still open. Call End first"))
	{
		SetTextConstructionCommand(TEXT_CONSTRUCTION_CLEAR_PRINT);

		Clear();

		m_pMainTextLabel = pMainTextLabel;
	}
}


void CScriptTextConstruction::EndClearPrint()
{
	if (SCRIPT_VERIFY(GetTextConstructionCommand() == TEXT_CONSTRUCTION_CLEAR_PRINT, "CScriptTextConstruction::EndClearPrint - CScriptTextConstruction::BeginClearPrint hasn't been called"))
	{
		char *pMainString = TheText.Get(m_pMainTextLabel);
		s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

		CSubtitleMessage MessageToClear;
		MessageToClear.Set(pMainString, MainTextBlock, 
			m_Numbers, m_NumberOfNumbers, 
			m_SubStrings, m_NumberOfSubStringTextLabels, 
			false, false);	//	Not sure what bUsesUnderscore should be set to - should it be passed as a parameter to the END_CLEAR_PRINT command?

		CMessages::ClearThisPrint(MessageToClear);

		SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}

void CScriptTextConstruction::AddInteger(s32 IntegerToAdd)
{
	if (SCRIPT_VERIFY(IsConstructingText(), "CScriptTextConstruction::AddInteger - you need to Begin a text command before adding any parameters"))
	{
		if (scriptVerifyf(m_NumberOfNumbers < MaxNumberOfNumbersInPrintCommand, "%s : CScriptTextConstruction::AddInteger - Too many numbers. The maximum is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MaxNumberOfNumbersInPrintCommand))
		{
			m_Numbers[m_NumberOfNumbers].Set(IntegerToAdd);
			m_Numbers[m_NumberOfNumbers].SetColour(ms_eColourOfNextTextComponent);
			ClearColourOfNextTextComponent();
			m_NumberOfNumbers++;

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::AddInteger - Just added %d m_NumberOfNumbers is now %u", IntegerToAdd, m_NumberOfNumbers);
			}
#endif	//	__BANK
		}
	}
}

void CScriptTextConstruction::AddFloat(float FloatToAdd, s32 NumberOfDecimalPlaces)
{
	if (SCRIPT_VERIFY(IsConstructingText(), "CScriptTextConstruction::AddFloat - you need to Begin a text command before adding any parameters"))
	{
		if (scriptVerifyf(m_NumberOfNumbers < MaxNumberOfNumbersInPrintCommand, "%s : CScriptTextConstruction::AddFloat - Too many numbers. The maximum is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MaxNumberOfNumbersInPrintCommand))
		{
			m_Numbers[m_NumberOfNumbers].Set(FloatToAdd, NumberOfDecimalPlaces);
			m_Numbers[m_NumberOfNumbers].SetColour(ms_eColourOfNextTextComponent);
			ClearColourOfNextTextComponent();
			m_NumberOfNumbers++;

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::AddFloat - Just added %f m_NumberOfNumbers is now %u", FloatToAdd, m_NumberOfNumbers);
			}
#endif	//	__BANK
		}
	}
}

void CScriptTextConstruction::AddSubStringTextLabel(const char *pSubStringTextLabelToAdd)
{
	if (SCRIPT_VERIFY(IsConstructingText(), "CScriptTextConstruction::AddSubStringTextLabel - you need to Begin a text command before adding any parameters"))
	{
		if (scriptVerifyf(m_NumberOfSubStringTextLabels < MaxNumberOfSubStringsInPrintCommand, "%s : CScriptTextConstruction::AddSubStringTextLabel - Too many substrings. The maximum is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MaxNumberOfSubStringsInPrintCommand))
		{
			m_SubStrings[m_NumberOfSubStringTextLabels].SetTextLabel(pSubStringTextLabelToAdd);
			m_SubStrings[m_NumberOfSubStringTextLabels].SetColour(ms_eColourOfNextTextComponent);
			ClearColourOfNextTextComponent();
			m_NumberOfSubStringTextLabels++;

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::AddSubStringTextLabel - Just added %s m_NumberOfSubStringTextLabels is now %u", pSubStringTextLabelToAdd, m_NumberOfSubStringTextLabels);
			}
#endif	//	__BANK
		}
	}
}

void CScriptTextConstruction::AddSubStringTextLabelHashKey(u32 HashKeyOfSubStringTextLabel)
{
	if (SCRIPT_VERIFY(IsConstructingText(), "CScriptTextConstruction::AddSubStringTextLabelHashKey - you need to Begin a text command before adding any parameters"))
	{
		if (scriptVerifyf(m_NumberOfSubStringTextLabels < MaxNumberOfSubStringsInPrintCommand, "%s : CScriptTextConstruction::AddSubStringTextLabelHashKey - Too many substrings. The maximum is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MaxNumberOfSubStringsInPrintCommand))
		{
			m_SubStrings[m_NumberOfSubStringTextLabels].SetTextLabelHashKey(HashKeyOfSubStringTextLabel);
			m_SubStrings[m_NumberOfSubStringTextLabels].SetColour(ms_eColourOfNextTextComponent);
			ClearColourOfNextTextComponent();
			m_NumberOfSubStringTextLabels++;

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::AddSubStringTextLabelHashKey - Just added %u m_NumberOfSubStringTextLabels is now %u", HashKeyOfSubStringTextLabel, m_NumberOfSubStringTextLabels);
			}
#endif	//	__BANK
		}
	}
}

void CScriptTextConstruction::AddSubStringLiteralString(const char *pSubStringLiteralStringToAdd)
{
	if (SCRIPT_VERIFY(IsConstructingText(), "CScriptTextConstruction::AddSubStringLiteralString - you need to Begin a text command before adding any parameters"))
	{
		if (scriptVerifyf(m_NumberOfSubStringTextLabels < MaxNumberOfSubStringsInPrintCommand, "%s : CScriptTextConstruction::AddSubStringLiteralString - Too many substrings. The maximum is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MaxNumberOfSubStringsInPrintCommand))
		{
			CSubStringWithinMessage::eLiteralStringType literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;
			bool bPersistentLiteralOccursInSubtitles = false;

			switch (GetTextConstructionCommand())
			{
			case TEXT_CONSTRUCTION_NONE :
				//	We've already checked IsConstructingText() so should never get in here
				break;

			case TEXT_CONSTRUCTION_PRINT :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT;
				bPersistentLiteralOccursInSubtitles = true;
				break;

			case TEXT_CONSTRUCTION_IS_MESSAGE_DISPLAYED :
			case TEXT_CONSTRUCTION_IS_HELP_MESSAGE_DISPLAYED :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE;
				break;

			case TEXT_CONSTRUCTION_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT :
			case TEXT_CONSTRUCTION_GET_NUMBER_OF_LINES_FOR_DISPLAY_TEXT :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE;
				break;

			case TEXT_CONSTRUCTION_DISPLAY_TEXT :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;	//	All Display Text commands have to be called every frame, so the literals shouldn't be persistent
				break;

			case TEXT_CONSTRUCTION_DISPLAY_HELP :
				// Help messages store their entire message in an array of char's
				// so they don't refer to literal strings after CHelpMessage::SetMessageText()
				// has been called. However if the message is going to be added to the previous briefs
				// then the literals have to be persistent
				if (CScriptHud::GetAddNextMessageToPreviousBriefs())
				{
					literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT;
					bPersistentLiteralOccursInSubtitles = false;
				}
				break;

			case TEXT_CONSTRUCTION_SCALEFORM_STRING :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE;
				break;

			case TEXT_CONSTRUCTION_ROW_ABOVE_PLAYERS_HEAD :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;
				break;

			case TEXT_CONSTRUCTION_SET_BLIP_NAME :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;
				break;

			case TEXT_CONSTRUCTION_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT;
				bPersistentLiteralOccursInSubtitles = false;
				break;

			case TEXT_CONSTRUCTION_CLEAR_PRINT :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE;
				break;

			case TEXT_CONSTRUCTION_THEFEED_POST :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;
				break;

			case TEXT_CONSTRUCTION_BUTTON_OVERRIDE :
			case TEXT_CONSTRUCTION_BUSYSPINNER :
				literalStringType = CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME;
				break;

			}

			bool bUseDebugPortionOfSingleFrameLiteralStringArray = false;
#if !__FINAL
			if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "CScriptTextConstruction::AddSubStringLiteralString - not called by a script command"))
			{
				if (CTheScripts::GetCurrentGtaScriptThread()->UseDebugPortionOfScriptTextArrays())
				{
					bUseDebugPortionOfSingleFrameLiteralStringArray = true;
				}
			}
#endif	//	!__FINAL

			m_SubStrings[m_NumberOfSubStringTextLabels].SetLiteralString(pSubStringLiteralStringToAdd, 
				literalStringType, 
				bUseDebugPortionOfSingleFrameLiteralStringArray);

			if (literalStringType == CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT)
			{
				if (!bPersistentLiteralOccursInSubtitles)
				{
					m_SubStrings[m_NumberOfSubStringTextLabels].SetPersistentLiteralStringOccursInSubtitles(false);
				}
			}
			m_SubStrings[m_NumberOfSubStringTextLabels].SetColour(ms_eColourOfNextTextComponent);
			ClearColourOfNextTextComponent();

			m_NumberOfSubStringTextLabels++;

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CScriptTextConstruction::AddSubStringLiteralString - Just added %s m_NumberOfSubStringTextLabels is now %u", pSubStringLiteralStringToAdd, m_NumberOfSubStringTextLabels);
			}
#endif	//	__BANK
		}
	}
}

void CScriptTextConstruction::AddSubStringBlipName(s32 blipIndex)
{
	if (SCRIPT_VERIFY(blipIndex != INVALID_BLIP_ID, "CScriptTextConstruction::AddSubStringBlipName - Blip doesn't exist"))
	{	//	What if the blip name actually contains non-ASCII characters?
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(blipIndex);

		if (pBlip)
		{
			AddSubStringLiteralString( CMiniMap::GetBlipNameValue(pBlip) );
		}
	}
}

void CScriptTextConstruction::SetColourOfNextTextComponent(s32 HudColourIndex)
{
	ms_eColourOfNextTextComponent = static_cast<eHUD_COLOURS>(HudColourIndex);
}
