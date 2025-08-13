#ifndef _SCRIPT_TEXT_CONSTRUCTION_H_
#define _SCRIPT_TEXT_CONSTRUCTION_H_


// Game headers
#include "frontend/GameStreamMgr.h"
#include "text/messages.h"

enum eTextConstruction
{
	TEXT_CONSTRUCTION_NONE,
	TEXT_CONSTRUCTION_PRINT,
	TEXT_CONSTRUCTION_IS_MESSAGE_DISPLAYED,
	TEXT_CONSTRUCTION_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT,
	TEXT_CONSTRUCTION_GET_NUMBER_OF_LINES_FOR_DISPLAY_TEXT,
	TEXT_CONSTRUCTION_DISPLAY_TEXT,
	TEXT_CONSTRUCTION_DISPLAY_HELP,
	TEXT_CONSTRUCTION_IS_HELP_MESSAGE_DISPLAYED,
	TEXT_CONSTRUCTION_SCALEFORM_STRING,
	TEXT_CONSTRUCTION_ROW_ABOVE_PLAYERS_HEAD,
	TEXT_CONSTRUCTION_SET_BLIP_NAME,
	TEXT_CONSTRUCTION_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS,
	TEXT_CONSTRUCTION_CLEAR_PRINT,
	TEXT_CONSTRUCTION_THEFEED_POST,
	TEXT_CONSTRUCTION_BUSYSPINNER,
	TEXT_CONSTRUCTION_BUTTON_OVERRIDE
};



class CScriptTextConstruction
{
public:

	static void SetTextConstructionCommand(eTextConstruction NewValue) { m_eTextConstructionCommand = NewValue; }
	static eTextConstruction GetTextConstructionCommand() { return m_eTextConstructionCommand; }
	static bool IsConstructingText() { return (TEXT_CONSTRUCTION_NONE != m_eTextConstructionCommand); }

	static void BeginPrint(const char *pMainTextLabel);
	static void EndPrint(s32 Duration, bool bPrintNow);

	static void BeginBusySpinnerOn(const char* pMainTextLabel);
	static void EndBusySpinnerOn( int Icon );

	static void BeginOverrideButtonText(const char* pMainTextLabel);
	static void EndOverrideButtonText(int iSlotIndex);

	static void BeginTheFeedPost(const char* pMainTextLabel);
	static int EndTheFeedPostStats( const char* Title, int LevelTotal, int LevelCurrent, bool IsImportant, const char* ContactTxD, const char* ContactTxN );
	static int EndTheFeedPostMessageText( const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle = "", float timeMultiplier = 1.0f, const char* CrewTagPacked = "", int Icon2 = 0, int iHudColor = HUD_COLOUR_WHITE );
	static int EndTheFeedPostTicker( bool IsImportant, bool bCacheMessage, bool bHasTokens );
	static int EndTheFeedPostTickerF10( const char* TopLine, bool IsImportant, bool bCacheMessage );
	static int EndTheFeedPostAward(const char* TxD, const char* TxN, int iXP, eHUD_COLOURS eAwardColour, const char* Title);
	static int EndTheFeedPostCrewTag( bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int CrewRank, bool FounderStatus, bool IsImportant, int crewId, const char* GameName, int crewColR, int crewColG, int crewColB );
	static int EndTheFeedPostUnlock( const char* chTitle, int iconType, bool bIsImportant, eHUD_COLOURS eTitleColour, bool bTitleIsLiteral);
	static int EndTheFeedPostMpTicker( bool IsImportant, bool bCacheMessage);
	static int EndTheFeedPostCrewRankup(const char* chSubtitle, const char* chTXD, const char* chTXN, bool bIsImportant, bool bSubtitleIsLiteral);
	static int EndTheFeedPostVersus(const char* ch1TXD, const char* ch1TXN, int val1, const char* ch2TXD, const char* ch2TXN, int val2, eHUD_COLOURS iCustomColor1 = HUD_COLOUR_INVALID, eHUD_COLOURS iCustomColor2 = HUD_COLOUR_INVALID);
	static int EndTheFeedPostReplay(CGameStream::eFeedReplayState replayState, int iIcon, const char* cSubtitle);
	static int EndTheFeedPostReplayInput(CGameStream::eFeedReplayState replayState, const char* cIcon, const char* cSubtitle);

	static Vector2 CalculateTextPosition(Vector2 vPos);
	static void BeginDisplayText(const char *pMainTextLabel);
	static void EndDisplayText(float DisplayAtX, float DisplayAtY, bool bForceUseDebugText, int Stereo = 0);

	static void BeginIsMessageDisplayed(const char *pMainTextLabel);
	static bool EndIsMessageDisplayed();

	static void BeginGetScreenWidthOfDisplayText(const char *pMainTextLabel);
	static float EndGetScreenWidthOfDisplayText(bool bIncludeSpaces);

	static void BeginGetNumberOfLinesForString(const char *pMainTextLabel);
	static s32 EndGetNumberOfLinesForString(float DisplayAtX, float DisplayAtY);

	static void BeginDisplayHelp(const char *pMainTextLabel);
	static void EndDisplayHelp(s32 iHelpId, bool bDisplayForever, bool bPlaySound, s32 OverrideDuration);

	static void BeginIsThisHelpMessageBeingDisplayed(const char *pMainTextLabel);
	static bool EndIsThisHelpMessageBeingDisplayed(s32 iHelpId);

	static void BeginScaleformString(const char *pMainTextLabel);
	static void EndScaleformString(bool bConvertToHtml);

	static void BeginRowAbovePlayersHead(const char *pMainTextLabel);
	static void EndRowAbovePlayersHead();

	static void BeginSetBlipName(const char *pMainTextLabel);
	static void EndSetBlipName(s32 iBlipIndex);

	static void BeginAddDirectlyToPreviousBriefs(const char *pMainTextLabel);
	static void EndAddDirectlyToPreviousBriefs(bool bUsesUnderscore);

	static void BeginClearPrint(const char *pMainTextLabel);
	static void EndClearPrint();


	static void AddInteger(s32 IntegerToAdd);
	static void AddFloat(float FloatToAdd, s32 NumberOfDecimalPlaces);

	static void AddSubStringTextLabel(const char *pSubStringTextLabelToAdd);
	static void AddSubStringTextLabelHashKey(u32 HashKeyOfSubStringTextLabel);
	static void AddSubStringLiteralString(const char *pSubStringLiteralStringToAdd);

	static void AddSubStringBlipName(s32 blipIndex);

	static void SetColourOfNextTextComponent(s32 HudColourIndex);

#if __BANK
	static const char *GetMainTextLabel() { return &m_pMainTextLabel[0]; }
#endif // __BANK

private:
	//	Private functions
	static void Clear();

	static void ClearColourOfNextTextComponent() { ms_eColourOfNextTextComponent = HUD_COLOUR_INVALID; }

	//	Private members
	static const u32 MaxNumberOfNumbersInPrintCommand = 8;
	static const u32 MaxNumberOfSubStringsInPrintCommand = 10;	//	Increased from 4 to 5 for Bug 357951
																//	Increased from 5 to 10 for Bug 503118 - Wanted Car App

	static const char *m_pMainTextLabel;

	static CNumberWithinMessage m_Numbers[MaxNumberOfNumbersInPrintCommand];
	static CSubStringWithinMessage m_SubStrings[MaxNumberOfSubStringsInPrintCommand];

	static u32 m_NumberOfNumbers;
	static u32 m_NumberOfSubStringTextLabels;

	static eTextConstruction m_eTextConstructionCommand;

	static eHUD_COLOURS ms_eColourOfNextTextComponent;
};



#endif	//	_SCRIPT_TEXT_CONSTRUCTION_H_


