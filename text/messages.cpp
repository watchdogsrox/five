/////////////////////////////////////////////////////////////////////////////////
//
// FILE :      messages.cpp
// PURPOSE :   Messages handling. Keeps track of messages that are to be displayed.
// AUTHOR :    Obbe, GraemeW, DerekP (13/12/99)
//
/////////////////////////////////////////////////////////////////////////////////

// C headers
#include <stdio.h>

// fw headers
#include "fwsys/timer.h"
#include "fwmaths/angle.h"

// game headers
#include "messages.h"
#include "audio/frontendaudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/Caminterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Control/GameLogic.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "cutscene\CutSceneManagerNew.h"
#include "debug\Debug.h"
#include "frontend\BusySpinner.h"
#include "frontend\PauseMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend\NewHud.h"
#include "Frontend/Scaleform/ScaleFormStore.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/ui_channel.h"
#include "game\Clock.h"
#include "peds/Ped.h"
#include "Stats\StatsMgr.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_messages.h"
#include "scene/world/GameWorld.h"
#include "script/script_channel.h"
#include "script/script_hud.h"
#include "system/controlMgr.h"
#include "Text\Text.h"
#include "Text\TextFormat.h"
#include "Text\TextConversion.h"

// Parser headers
#include "parser/manager.h"
#include "Text\dialogueCharacters_parser.h"


#include "data/aes_init.h"
AES_INIT_C;

TEXT_OPTIMISATIONS()

CQueuedMessage	CMessages::BriefMessages[MAX_NUM_BRIEF_MESSAGES];

CPreviousMessages CMessages::PreviousDialogueMessages;
CPreviousMessages CMessages::PreviousMissionMessages;
CPreviousMessages CMessages::PreviousHelpTextMessages;

bool CMessages::bTitleActive = false;

u32 CMessages::m_PreviousSubtitleTimer = 0;

char CHelpMessage::m_HelpMessageText[MAX_HELP_TEXT_SLOTS][MAX_CHARS_IN_MESSAGE];
bool CHelpMessage::m_bHelpMessagesFading[MAX_HELP_TEXT_SLOTS];
bool CHelpMessage::m_bTriggerSound[MAX_HELP_TEXT_SLOTS];
CHelpMessage::eTEXT_STATE CHelpMessage::m_iNewText[MAX_HELP_TEXT_SLOTS];
eTEXT_STYLE CHelpMessage::m_iStyle[MAX_HELP_TEXT_SLOTS];
eTEXT_ARROW_ORIENTATION CHelpMessage::m_iArrowDirection[MAX_HELP_TEXT_SLOTS];
s32 CHelpMessage::m_iFloatingTextOffset[MAX_HELP_TEXT_SLOTS];
Color32 CHelpMessage::m_iColour[MAX_HELP_TEXT_SLOTS];
Vector2 CHelpMessage::m_vScreenPosition[MAX_HELP_TEXT_SLOTS];
s32 CHelpMessage::m_OverrideDuration[MAX_HELP_TEXT_SLOTS];

// Dialogue Character Data
#define DEFAULT_CHARACTER_TXD "CHAR_DEFAULT"
#define DIALOGUE_CHARACTERS_META_FILE	"common:/data/ui/dialogueCharacters"

SDialogueCharacters CMessages::ms_dialogueCharacters;

// ******************************************************************************************************
// ** SDialogueCharacters
// ******************************************************************************************************
// Gets the character txd associated with the specified voice name hash
const char* SDialogueCharacters::GetCharacterTXD(u32 iVoiceNameHash, u32 iMissionNameHash/*=0*/)
{
	if(iVoiceNameHash != 0)
	{
		for(int i = 0; i < m_characterInfo.GetCount(); ++i)
		{
			u32 charMissionHash = m_characterInfo[i].m_MissionNameHash;
			if ( m_characterInfo[i].m_VoiceNameHash == iVoiceNameHash && 
				( charMissionHash == 0 || charMissionHash == iMissionNameHash) )
			{
				return m_characterInfo[i].m_txd.GetCStr();
			}
		}
	}

	return DEFAULT_CHARACTER_TXD;
}

// ******************************************************************************************************
// ** CNumberWithinMessage
// ******************************************************************************************************
void CNumberWithinMessage::Set(s32 IntegerToDisplay)
{
	m_Integer = IntegerToDisplay;
	m_NumberOfDecimalPlaces = DISPLAY_INTEGER;
}

void CNumberWithinMessage::Set(float FloatToDisplay, s32 NumberOfDecimalPlaces)
{
	m_Float = FloatToDisplay;
	m_NumberOfDecimalPlaces = NumberOfDecimalPlaces;
}


//	Returns length of new string
s32 CNumberWithinMessage::FillString(char *pStringToFill, s32 MaxLengthOfString) const
{
	switch (m_NumberOfDecimalPlaces)
	{
		case NO_NUMBER_TO_DISPLAY :
			Assertf(0, "CNumberWithinMessage::FillString - text contains a ~1~ but there is no number to display");
			safecpy(pStringToFill, "", MaxLengthOfString);
			break;

		case DISPLAY_INTEGER :	//	Treat as an integer
			formatf(pStringToFill, MaxLengthOfString, "%d", m_Integer);
			break;

		default :	//	Treat as a float
			Assertf(m_NumberOfDecimalPlaces >= 0, "CNumberWithinMessage::FillString - number of decimal places (%d) for float (%f) is less than 0", m_NumberOfDecimalPlaces, m_Float);
			Assertf(m_NumberOfDecimalPlaces < 5, "CNumberWithinMessage::FillString - trying to display a floating point number (%f) to more than 5 decimal places (%d)", m_Float, m_NumberOfDecimalPlaces);
			formatf(pStringToFill, MaxLengthOfString, "%.*f", m_NumberOfDecimalPlaces, m_Float);
			break;
	}

	return istrlen(pStringToFill);
}

#if !__FINAL
void CNumberWithinMessage::DebugPrintContents()
{
	switch (m_NumberOfDecimalPlaces)
	{
	case NO_NUMBER_TO_DISPLAY :
		break;

	case DISPLAY_INTEGER :	//	Treat as an integer
		scriptDisplayf("Integer Value = %d", m_Integer);
		break;

	default :	//	Treat as a float
		Assertf(m_NumberOfDecimalPlaces >= 0, "CNumberWithinMessage::DebugPrintContents - number of decimal places (%d) for float (%f) is less than 0", m_NumberOfDecimalPlaces, m_Float);
		Assertf(m_NumberOfDecimalPlaces < 5, "CNumberWithinMessage::DebugPrintContents - trying to display a floating point number (%f) to more than 5 decimal places (%d)", m_Float, m_NumberOfDecimalPlaces);
		scriptDisplayf("Float Value = %.*f", m_NumberOfDecimalPlaces, m_Float);
		break;
	}
}
#endif	//	!__FINAL


bool CNumberWithinMessage::operator==(const CNumberWithinMessage& otherNumber) const
{
	// Should I be comparing m_Colour as well?

	if (m_NumberOfDecimalPlaces == otherNumber.m_NumberOfDecimalPlaces)
	{
		switch (m_NumberOfDecimalPlaces)
		{
		case NO_NUMBER_TO_DISPLAY :
			return true;
			break;

		case DISPLAY_INTEGER :
			if (m_Integer == otherNumber.m_Integer)
			{
				return true;
			}
			break;

		default :
			if (m_Float == otherNumber.m_Float)
			{
				return true;
			}
			break;
		}
	}

	return false;
}


// ******************************************************************************************************
// ** CSubStringWithinMessage
// ******************************************************************************************************
void CSubStringWithinMessage::SetTextLabel(const char *pTextLabel)
{
	m_pText = TheText.Get(pTextLabel);
	m_BlockContainingThisText = TheText.GetBlockContainingLastReturnedString();
}

void CSubStringWithinMessage::SetTextLabelHashKey(u32 HashKeyOfTextLabel)
{
	char StringContainingHashKey[24];
	formatf(StringContainingHashKey, 24, "%u", HashKeyOfTextLabel);

	m_pText = TheText.Get(HashKeyOfTextLabel, StringContainingHashKey);
	m_BlockContainingThisText = TheText.GetBlockContainingLastReturnedString();
}

void CSubStringWithinMessage::SetCharPointer(const char *pLocalizedString)
{
	m_pText = pLocalizedString;
	m_BlockContainingThisText = CHAR_POINTER;
}

void CSubStringWithinMessage::SetLiteralString(const char *pLiteralString, eLiteralStringType literalStringType, bool bUseDebugPortionOfSingleFrameArray)
{
	uLiteralString.m_LiteralStringIndex = static_cast<s16>(CScriptHud::ScriptLiteralStrings.SetLiteralString(pLiteralString, literalStringType, bUseDebugPortionOfSingleFrameArray));
	uLiteralString.m_LiteralStringType = static_cast<u16>(literalStringType);

	m_BlockContainingThisText = LITERAL_STRING;
}

void CSubStringWithinMessage::SetPersistentLiteralStringOccursInSubtitles(bool bOccursInSubtitles)
{
	if (Verifyf(m_BlockContainingThisText == LITERAL_STRING, "CSubStringWithinMessage::SetPersistentLiteralStringOccursInSubtitles - expected this string to be a literal string"))
	{
		if (Verifyf(uLiteralString.m_LiteralStringType == LITERAL_STRING_TYPE_PERSISTENT, "CSubStringWithinMessage::SetPersistentLiteralStringOccursInSubtitles - Expected literal strings in subtitles to always be persistent"))
		{
			CScriptHud::ScriptLiteralStrings.SetPersistentLiteralOccursInSubtitles(uLiteralString.m_LiteralStringIndex, bOccursInSubtitles);
		}
	}
}

void CSubStringWithinMessage::Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs)
{
	if (m_BlockContainingThisText == LITERAL_STRING)
	{
		if (uLiteralString.m_LiteralStringType == LITERAL_STRING_TYPE_PERSISTENT)
		{
			if (ClearPersistentLiterals)
			{
				CScriptHud::ScriptLiteralStrings.ClearPersistentString(uLiteralString.m_LiteralStringIndex, bClearingFromPreviousBriefs);
			}
		}
		uLiteralString.m_LiteralStringIndex = -1;
	}
	else
	{
		m_pText = NULL;
	}

	m_BlockContainingThisText = NO_STRING_TO_DISPLAY;
	m_Colour = HUD_COLOUR_INVALID;
}

bool CSubStringWithinMessage::UsesTextFromThisBlock(s32 TextBlockIndex)
{
	if (m_BlockContainingThisText == TextBlockIndex)
	{
		return true;
	}

	return false;
}


u32 CSubStringWithinMessage::GetStringCharacterCount()
{
	if (m_BlockContainingThisText == NO_STRING_TO_DISPLAY)
	{
		return 0;
	}

	return CTextConversion::GetCharacterCount(GetString());
}


const char *CSubStringWithinMessage::GetString() const
{
	switch (m_BlockContainingThisText)
	{
	case NO_STRING_TO_DISPLAY :
		return NULL;

	case LITERAL_STRING :
		if ( (uLiteralString.m_LiteralStringType == LITERAL_STRING_TYPE_SINGLE_FRAME) && !CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{	//	If we're trying to access a single frame literal outside the Render Thread then we need to access the write buffer of the double buffer
			return CScriptHud::ScriptLiteralStrings.GetSingleFrameLiteralStringFromWriteBuffer(uLiteralString.m_LiteralStringIndex);
		}
		else
		{
			return CScriptHud::ScriptLiteralStrings.GetLiteralString(uLiteralString.m_LiteralStringIndex, (CSubStringWithinMessage::eLiteralStringType) uLiteralString.m_LiteralStringType);
		}
		break;

	default:
		return m_pText;
		break;
	}
}

void CSubStringWithinMessage::DetermineWhichPreviousBriefArrayToUse(bool &bAddToPreviousBriefs, bool &bAddToDialogueBriefs)
{
	switch (m_BlockContainingThisText)
	{
		case GTA5_GLOBAL_TEXT_SLOT :	//	main text block
		case LITERAL_STRING :		//	not sure what to do for this - going to try leaving the two flags as they are
		case NO_STRING_TO_DISPLAY :	//	not sure what to do for this - going to try leaving the two flags as they are
		case MISSION_TEXT_SLOT :
		case CREDITS_TEXT_SLOT :
		case MINIGAME_TEXT_SLOT :
		case ODDJOB_TEXT_SLOT :
		case DLC_TEXT_SLOT0 :
		case DLC_TEXT_SLOT1 :
		case DLC_TEXT_SLOT2 :
			break;

		case MISSION_DIALOGUE_TEXT_SLOT :
		case AMBIENT_DIALOGUE_TEXT_SLOT :
		case DLC_MISSION_DIALOGUE_TEXT_SLOT :
		case DLC_AMBIENT_DIALOGUE_TEXT_SLOT :
		case DLC_MISSION_DIALOGUE_TEXT_SLOT2 :
			bAddToDialogueBriefs = true;
			break;
		default :
			bAddToPreviousBriefs = false;
			bAddToDialogueBriefs = false;
			return;
	}
}

void CSubStringWithinMessage::SetLiteralStringOccursInPreviousBriefs()
{
	if (m_BlockContainingThisText == LITERAL_STRING)
	{
		if (Verifyf(uLiteralString.m_LiteralStringType == LITERAL_STRING_TYPE_PERSISTENT, "CSubStringWithinMessage::SetLiteralStringOccursInPreviousBriefs - Expected literal strings in subtitles to always be persistent"))
		{
			CScriptHud::ScriptLiteralStrings.SetPersistentLiteralOccursInPreviousBriefs(uLiteralString.m_LiteralStringIndex, true);
		}
	}
}

bool CSubStringWithinMessage::operator==(const CSubStringWithinMessage& otherSubString) const
{
	// Should I be comparing m_Colour as well?

	//	Should I actually be calling CTextConversion::charStrcmp to check that the characters 
	//	in both strings match?
	if (m_BlockContainingThisText == otherSubString.m_BlockContainingThisText)
	{
		switch (m_BlockContainingThisText)
		{
		case NO_STRING_TO_DISPLAY :
			return true;
			break;

		case LITERAL_STRING :
			if (uLiteralString.m_LiteralStringIndex == otherSubString.uLiteralString.m_LiteralStringIndex)
			{
				if (uLiteralString.m_LiteralStringType == otherSubString.uLiteralString.m_LiteralStringType)
				{
					return true;
				}
			}
			break;

		case CHAR_POINTER :
			return m_pText == otherSubString.m_pText; // this MAY want to compare strcmps if they're pointing at different values... but none of the other fields do


		default :
			if (m_pText == otherSubString.m_pText)
			{
				return true;
			}
			break;
		}
	}

	return false;
}


// ******************************************************************************************************
// ** CSubtitleMessage
// ******************************************************************************************************
void CSubtitleMessage::Set(char const *pText, s32 TextBlock, 
						   CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
						   CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
						   bool bUsesUnderscore, bool bHelpText, u32 iVoiceNameHash/*=0*/, u32 iMissionNameHash/*=0*/)
{
	Clear(false, false);

	if (pArrayOfSubStrings)
	{
		if (Verifyf(SizeOfSubStringArray <= MAX_SUBSTRINGS_IN_SUBTITLES, "CSubtitleMessage::Set - new subtitle has more than %d substrings", MAX_SUBSTRINGS_IN_SUBTITLES))
		{
			for (u32 string_loop = 0; string_loop < SizeOfSubStringArray; string_loop++)
			{
				m_SubstringsWithinMessage[string_loop] = pArrayOfSubStrings[string_loop];
			}
		}
	}

	if (pArrayOfNumbers)
	{
		if (Verifyf(SizeOfNumberArray <= MAX_NUMBERS_IN_SUBTITLES, "CSubtitleMessage::Set - new subtitle has more than %d numbers", MAX_NUMBERS_IN_SUBTITLES))
		{
			for (u32 number_loop = 0; number_loop < SizeOfNumberArray; number_loop++)
			{
				m_NumbersWithinMessage[number_loop] = pArrayOfNumbers[number_loop];
			}
		}
	}

	m_pMessage = pText;
	m_TextBlockContainingThisMessage = TextBlock;

	m_bUseUnderscore = bUsesUnderscore;
	m_bIsHelpText = bHelpText;
	m_iVoiceNameHash = iVoiceNameHash;
	m_iMissionNameHash = iMissionNameHash;
}


void CSubtitleMessage::Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs)
{
	m_pMessage = NULL;
	m_TextBlockContainingThisMessage = -1;

	u32 loop = 0;
	for (loop = 0; loop < MAX_NUMBERS_IN_SUBTITLES; loop++)
	{
		m_NumbersWithinMessage[loop].Clear();
	}

	for (loop = 0; loop < MAX_SUBSTRINGS_IN_SUBTITLES; loop++)
	{
		m_SubstringsWithinMessage[loop].Clear(ClearPersistentLiterals, bClearingFromPreviousBriefs);
	}

	m_bUseUnderscore = false;
	m_bIsHelpText = false;
}

bool CSubtitleMessage::UsesTextFromThisBlock(s32 TextBlockIndex)
{
	if (m_TextBlockContainingThisMessage == TextBlockIndex)
	{
		return true;
	}

	for (u32 loop = 0; loop < MAX_SUBSTRINGS_IN_SUBTITLES; loop++)
	{
		if (m_SubstringsWithinMessage[loop].UsesTextFromThisBlock(TextBlockIndex))
		{
			return true;
		}
	}

	return false;
}

void CSubtitleMessage::FillInString(char *pStringToFillIn, u32 MaxLengthOfString)
{
	CMessages::InsertNumbersAndSubStringsIntoString(m_pMessage, 
		m_NumbersWithinMessage, MAX_NUMBERS_IN_SUBTITLES, 
		m_SubstringsWithinMessage, MAX_SUBSTRINGS_IN_SUBTITLES, 
		pStringToFillIn, MaxLengthOfString);

#if __WIN32PC
	CMessages::InsertPlayerControlKeysInString(pStringToFillIn);
#endif // __WIN32PC
}

bool CSubtitleMessage::DoesMainTextMatch(CSubtitleMessage &OtherSubtitle)
{
	if ( strcmp( m_pMessage, OtherSubtitle.m_pMessage ) == 0 )
	{
		return true;
	}

	return false;
}

bool CSubtitleMessage::Matches(CSubtitleMessage &OtherSubtitle)
{
	if (m_pMessage != OtherSubtitle.m_pMessage)
	{
		return false;
	}

	if (m_TextBlockContainingThisMessage != OtherSubtitle.m_TextBlockContainingThisMessage)
	{
		return false;
	}


	for (u32 number_loop = 0; number_loop < MAX_NUMBERS_IN_SUBTITLES; number_loop++)
	{
		if (m_NumbersWithinMessage[number_loop] != OtherSubtitle.m_NumbersWithinMessage[number_loop])
		{
			return false;
		}
	}

	for (u32 sub_string_loop = 0; sub_string_loop < MAX_SUBSTRINGS_IN_SUBTITLES; sub_string_loop++)
	{
		if (m_SubstringsWithinMessage[sub_string_loop] != OtherSubtitle.m_SubstringsWithinMessage[sub_string_loop])
		{
			return false;
		}
	}

	return true;
}

void CSubtitleMessage::DetermineWhichPreviousBriefArrayToUse(bool &bAddToPreviousBriefs, bool &bAddToDialogueBriefs, bool &bAddToHelpTextBriefs)
{
	bAddToPreviousBriefs = true;
	bAddToDialogueBriefs = false;

	if (m_bIsHelpText)
	{
		bAddToHelpTextBriefs = true;
		return;
	}

	switch (m_TextBlockContainingThisMessage)
	{
	case GTA5_GLOBAL_TEXT_SLOT :	//	main text block
	case MISSION_TEXT_SLOT :
	case CREDITS_TEXT_SLOT :
	case MINIGAME_TEXT_SLOT :
	case ODDJOB_TEXT_SLOT :
	case DLC_TEXT_SLOT0 :
	case DLC_TEXT_SLOT1 :
	case DLC_TEXT_SLOT2 :
		break;
	case PHONE_TEXT_SLOT:
	case MISSION_DIALOGUE_TEXT_SLOT :
	case AMBIENT_DIALOGUE_TEXT_SLOT :
	case DLC_MISSION_DIALOGUE_TEXT_SLOT :
	case DLC_AMBIENT_DIALOGUE_TEXT_SLOT :
	case DLC_MISSION_DIALOGUE_TEXT_SLOT2 :
		bAddToDialogueBriefs = true;
		break;
	default :
		bAddToPreviousBriefs = false;
		bAddToDialogueBriefs = false;
		return;
	}

	for (u32 SubStringLoop = 0; SubStringLoop < MAX_SUBSTRINGS_IN_SUBTITLES; SubStringLoop++)
	{
		m_SubstringsWithinMessage[SubStringLoop].DetermineWhichPreviousBriefArrayToUse(bAddToPreviousBriefs, bAddToDialogueBriefs);
		if (!bAddToPreviousBriefs)
		{
			bAddToDialogueBriefs = false;
			return;
		}
	}
}


void CSubtitleMessage::SetAllLiteralStringsOccurInPreviousBriefs()
{
	for (u32 sub_string_loop = 0; sub_string_loop < MAX_SUBSTRINGS_IN_SUBTITLES; sub_string_loop++)
	{
		m_SubstringsWithinMessage[sub_string_loop].SetLiteralStringOccursInPreviousBriefs();
	}
}


// ******************************************************************************************************
// ** CQueuedMessage
// ******************************************************************************************************
void CQueuedMessage::Set(char const *pText, s32 TextBlock, 
		 u32 Duration, bool bAddToPrevBriefs, ePreviousBriefOverride OverrideOfPreviousBriefList, 
		 CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		 CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
		 bool bUsesUnderscore, u32 iVoiceNameHash/*=0*/, u32 iMissionNameHash/*=0*/)
{
	m_MessageDetails.Set(pText, TextBlock, 
		pArrayOfNumbers, SizeOfNumberArray, 
		pArrayOfSubStrings, SizeOfSubStringArray, 
		bUsesUnderscore, false, iVoiceNameHash, iMissionNameHash);

	m_Duration = Duration;
	m_WhenStarted = fwTimer::GetTimeInMilliseconds();
	m_bAddToPreviousBriefs = bAddToPrevBriefs;
	m_OverrideBriefList = OverrideOfPreviousBriefList;
	m_bNewlyAddedToTheHeadOfTheQueue = false;

	//	If main text begins with ~z~ then it won't be displayed if subtitles are switched off
	//	(There could be a ~x~ before the ~z~)
	//	Should I bother testing if pShortText begins with ~z~?
	if (CMessages::IsThisTextSwitchedOffByFrontendSubtitleSetting(pText))
	{
		m_bDisplayEvenIfSubtitlesAreSwitchedOff = false;
	}
	else
	{
		m_bDisplayEvenIfSubtitlesAreSwitchedOff = true;
	}
}


void CQueuedMessage::Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs)
{
	m_MessageDetails.Clear(ClearPersistentLiterals, bClearingFromPreviousBriefs);

	m_Duration = 0;
	m_WhenStarted = 0;

	m_bAddToPreviousBriefs = true;
	m_OverrideBriefList = PREVIOUS_BRIEF_NO_OVERRIDE;
	m_bNewlyAddedToTheHeadOfTheQueue = false;
	m_bDisplayEvenIfSubtitlesAreSwitchedOff = false;
}

bool CQueuedMessage::HasDisplayTimeExpired()
{
	if (fwTimer::GetTimeInMilliseconds() > (m_WhenStarted + m_Duration) )
	{
		return true;
	}

	return false;
}


void CQueuedMessage::AddToPreviousBriefsIfRequired()
{
	if (m_bNewlyAddedToTheHeadOfTheQueue)
	{
		m_bNewlyAddedToTheHeadOfTheQueue = false;

		if (!IsEmpty())
		{
			if (m_bAddToPreviousBriefs)
			{
				CMessages::AddToPreviousBriefArray(m_MessageDetails, m_OverrideBriefList);
			}
		}
	}
}

// ******************************************************************************************************
// ** CMessages
// ******************************************************************************************************

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::Init
// PURPOSE: initialises the class
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::Init()
{
	const s32 MAX_NUM_PREVIOUS_BRIEF_ITEMS = 20;

	PreviousHelpTextMessages.Init(MAX_NUM_PREVIOUS_BRIEF_ITEMS);
	PreviousDialogueMessages.Init(MAX_NUM_PREVIOUS_BRIEF_ITEMS);
	PreviousMissionMessages.Init(MAX_NUM_PREVIOUS_BRIEF_ITEMS);

	InitDialogueMeta();
}

#if __BANK
void CMessages::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (pBank)
	{
		pBank->AddButton("Reload Dialogue Meta File", &CMessages::InitDialogueMeta);
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::Init
// PURPOSE: initialises dialogue character data
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::InitDialogueMeta()
{
	// Init Dialogue Character Info
	PARSER.LoadObject(DIALOGUE_CHARACTERS_META_FILE, "meta", ms_dialogueCharacters);
	uiDebugf1("Dialogue Character Metadata Loaded");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::Init
// PURPOSE: initialises the class at game startup
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
	    s32	C;

	    SetMissionTitleActive(false);

	    for (C = 0; C < MAX_NUM_BRIEF_MESSAGES; C++)
	    {
			BriefMessages[C].Clear(false, false);
	    }

	    ClearAllMessagesDisplayedByGame();

		CHelpMessage::Init();
		CSubtitleText::Init();
    }
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::Shutdown
// PURPOSE: shuts down the class at game shutdown
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        TheText.Unload();
		PreviousHelpTextMessages.Shutdown();
	    PreviousDialogueMessages.Shutdown();
	    PreviousMissionMessages.Shutdown();
		CLoadingText::Shutdown();
    }
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearAllBriefMessages
// PURPOSE: Wipes the brief messages array
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearAllBriefMessages()
{
	PreviousHelpTextMessages.ClearAllPreviousBriefs();
	PreviousDialogueMessages.ClearAllPreviousBriefs();
	PreviousMissionMessages.ClearAllPreviousBriefs();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearAllMessagesDisplayedByGame
// PURPOSE: This will be called when the language changes and will make sure no
//			messages are displayed on screen
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearAllMessagesDisplayedByGame()
{
	CHelpMessage::ClearAll();
	ClearMessages();
	ClearAllBriefMessages();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock
// PURPOSE: clears all messages that belong to the text block passed in, but leaves
//			the rest intact
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(s32 TextBlock, bool bClearPreviousBriefs)	//	, bool bIgnoreMissionTitle)
{
	Assertf(TextBlock >= 0, "ClearAllDisplayedMessagesThatBelongToThisTextBlock - TextBlock index should be >= 0");

	// Brief Messages
	for (s32 loop = 0; loop < MAX_NUM_BRIEF_MESSAGES; loop++)
	{
		if (BriefMessages[loop].UsesTextFromThisBlock(TextBlock))
		{
			BriefMessages[loop].Clear(true, false);
		}
	}

	// Brief Messages - shift up any remaining messages to fill the gaps
	s32 CopyToIndex = 0;
	while (CopyToIndex < MAX_NUM_BRIEF_MESSAGES)
	{
		if (BriefMessages[CopyToIndex].IsEmpty())
		{
			s32 CopyFromIndex = (CopyToIndex + 1);
			while ( (CopyFromIndex < MAX_NUM_BRIEF_MESSAGES) && (BriefMessages[CopyFromIndex].IsEmpty()) )
			{
				CopyFromIndex++;
			}

			if (CopyFromIndex < MAX_NUM_BRIEF_MESSAGES)
			{
				BriefMessages[CopyToIndex] = BriefMessages[CopyFromIndex];
				if (CopyToIndex == 0)
				{
					BriefMessages[0].SetWhenStarted(fwTimer::GetTimeInMilliseconds());
					BriefMessages[0].SetNewlyAddedToTheHeadOfTheQueue(true);
				}
				BriefMessages[CopyFromIndex].Clear(false, false);
			}
		}

		CopyToIndex++;
	}

	// Previous Briefs
	if (bClearPreviousBriefs)
	{
		ClearAllPreviousBriefsThatBelongToThisTextBlock(TextBlock);
	}	//	end of if (bClearPreviousBriefs)
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearMessages
// PURPOSE: Clears all the messages (the brief ones anyway)
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearMessages()
{
	for (s32 C = 0; C < MAX_NUM_BRIEF_MESSAGES; C++)
	{
		BriefMessages[C].Clear(true, false);
	}
}

//	Return false if copy fails to complete (because destination string isn't long enough)
bool CMessages::CopyGxtStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, const char *pSourceGxtString, u32 SourceStringLength)
{
	if (pSourceGxtString == NULL)
	{
		return true;
	}

	if (SourceStringLength == 0)
	{
		SourceStringLength = CTextConversion::GetByteCount(pSourceGxtString);
	}

	u32 CopyLoop = 0;
	while (CopyLoop < SourceStringLength)
	{
		if (DestIndex < MaxDestStringLength)
		{
			pDestString[DestIndex++] = pSourceGxtString[CopyLoop++];
		}
		else
		{
#if __ASSERT
			Assertf(0, "CMessages::CopyGxtStringIntoStringAtIndex - destination string has length %d. It's not long enough. Source string begins %s", MaxDestStringLength, pSourceGxtString );
#endif	//	__ASSERT
			return false;
		}
	}

	return true;
}

//	Return false if copy fails to complete (because destination string isn't long enough, or source string is too long to store in temporary char array)
bool CMessages::CopyStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, const char *pSourceString, u32 SourceStringLength)
{
	if (pSourceString == NULL)
	{
		return true;
	}

	if (SourceStringLength == 0)
	{
		SourceStringLength = ustrlen(pSourceString);
	}

	return CopyGxtStringIntoStringAtIndex(pDestString, DestIndex, MaxDestStringLength, pSourceString, SourceStringLength);
}

void CMessages::CopyColouredGxtStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, eHUD_COLOURS ColourOfSourceString, const char *pSourceGxtString, u32 SourceStringLength)
{
	bool bResult = false;

	const u32 MaxLengthOfColourString = 16;
	char ColourString[MaxLengthOfColourString];

	if (HUD_COLOUR_INVALID != ColourOfSourceString)
	{
		formatf(ColourString, MaxLengthOfColourString, "~HC_%d~", ColourOfSourceString);
		bResult = CopyStringIntoStringAtIndex(pDestString, DestIndex, MaxDestStringLength, ColourString);
		Assertf(bResult, "CMessages::CopyColouredGxtStringIntoStringAtIndex - failed to insert colour for substring/number into main string");
	}

	bResult = CopyGxtStringIntoStringAtIndex(pDestString, DestIndex, MaxDestStringLength, pSourceGxtString, SourceStringLength);
	Assertf(bResult, "CMessages::CopyColouredGxtStringIntoStringAtIndex - failed to insert substring/number into main string");

//	Maybe it's more flexible to leave it up to the line of text to reset the colour in case it should be reset to something other than COLOUR_STANDARD
// 	if (HUD_COLOUR_INVALID != ColourOfSourceString)
// 	{	//	Now write ~s~ after the substring/number to reset the colour for the rest of the line of text
// 		formatf(ColourString, MaxLengthOfColourString, "~%c~", FONT_CHAR_TOKEN_COLOUR_STANDARD);		
// 		bResult = CopyStringIntoStringAtIndex(pDestString, DestIndex, MaxDestStringLength, ColourString);
// 		Assertf(bResult, "CMessages::CopyColouredGxtStringIntoStringAtIndex - failed to insert ~s~ for substring/number into main string");
// 	}
}


eTokenFormat CMessages::CheckForInsertionToken(const char *pOriginalString, const u32 CharacterIndex, u32 &IndexInteger)
{
	eTokenFormat Format = TOKEN_FORMAT_NONE;

	if (pOriginalString[CharacterIndex] == FONT_CHAR_TOKEN_DELIMITER)
	{
		char CharactersToCheck[4];
		bool bReachedEndOfOriginalString = false;
		for (u32 loop = 0; loop < 4; loop++)
		{
			if (bReachedEndOfOriginalString || (pOriginalString[CharacterIndex + 1 + loop] == rage::TEXT_CHAR_TERMINATOR) )
			{
				bReachedEndOfOriginalString = true;
				CharactersToCheck[loop] = rage::TEXT_CHAR_TERMINATOR;
			}
			else
			{
				CharactersToCheck[loop] = pOriginalString[CharacterIndex + 1 + loop];
			}			
		}

		if (CharactersToCheck[1] == FONT_CHAR_TOKEN_DELIMITER)
		{	//	Check if it's ~a~ or ~1~. Could still be something else like ~n~
			if (CharactersToCheck[0] == FONT_CHAR_TOKEN_NUMBER)
			{
				Format = TOKEN_FORMAT_NUMBER_ORIGINAL;
			}
			else if (CharactersToCheck[0] == FONT_CHAR_TOKEN_SUBSTRING)
			{
				Format = TOKEN_FORMAT_SUBSTRING_ORIGINAL;
			}
		}
		else if (CharactersToCheck[1] == FONT_CHAR_TOKEN_UNDERLINE)
		{	//	Check if it's ~a_.~ or ~1_.~ Could still be something else
			if (CharactersToCheck[3] == FONT_CHAR_TOKEN_DELIMITER)
			{
				if (CharactersToCheck[0] == FONT_CHAR_TOKEN_NUMBER)
				{
					Format = TOKEN_FORMAT_NUMBER_INDEXED;
				}
				else if (CharactersToCheck[0] == FONT_CHAR_TOKEN_SUBSTRING)
				{
					Format = TOKEN_FORMAT_SUBSTRING_INDEXED;
				}

				if (Format != TOKEN_FORMAT_NONE)
				{
					if (Verifyf( (CharactersToCheck[2] >= '0') && (CharactersToCheck[2] <= '9'), 
						"CMessages::CheckForInsertionToken - expected an integer after _ inside ~a_~ or ~1_~ but character is %c", CharactersToCheck[2]))
					{
						IndexInteger = CharactersToCheck[2] - '0';
					}
				}
			}
		}
	}

	return Format;
}

u32 CMessages::GetArrayIndexAccordingToFormat(const eTokenFormat FormatUsedForThisPosition, const u32 IndexInteger, eTokenFormat &MethodToUseForInserting, const u32 NumberOfItemsAlreadyInserted, const char * ASSERT_ONLY(pOriginalStringForAssertMessage))
{
	if (MethodToUseForInserting == TOKEN_FORMAT_NONE)
	{	//	If this is the first element of this type (number or substring) to be inserted into the main string 
		//	then use the method (original or indexed) that has just been read from the first token
		MethodToUseForInserting = FormatUsedForThisPosition;
	}
	else
	{	//	Otherwise check that the current token is using the same method (original or indexed) 
		//	as the first token for this type (number or substring)
		if (FormatUsedForThisPosition != MethodToUseForInserting)
		{
#if __ASSERT
			Displayf("CMessages::GetArrayIndexAccordingToFormat - Main string is %s", pOriginalStringForAssertMessage );
			Assertf(0, "CMessages::GetArrayIndexAccordingToFormat - Main string uses a combination of ~a~ and ~a_.~ or a combination of ~1~ and ~1_.~. See TTY for details");
#endif	//	__ASSERT
			return 0;
		}
	}

	switch (MethodToUseForInserting)
	{
	case TOKEN_FORMAT_NONE :
		Assertf(0, "CMessages::GetArrayIndexAccordingToFormat - didn't expect MethodToUseForInserting to be TOKEN_FORMAT_NONE at this stage");
		break;
	case TOKEN_FORMAT_NUMBER_ORIGINAL :
	case TOKEN_FORMAT_SUBSTRING_ORIGINAL :
		break;
	case TOKEN_FORMAT_NUMBER_INDEXED :
	case TOKEN_FORMAT_SUBSTRING_INDEXED :
		return IndexInteger;	//	Return the index that has been read from the token
		break;
	}

	return NumberOfItemsAlreadyInserted;	//	For original method, the index will count up from 0
}


void CMessages::InsertNumberIntoString(char *pNewString, u32 &NewCharLoop, const u32 MaxSizeOfNewString, const CNumberWithinMessage *pArrayOfNumbers, const u32 SizeOfNumberArray, const u32 ArrayElementToUse, const char * ASSERT_ONLY(pOriginalStringForAssertMessage))
{
	eHUD_COLOURS ColourForThisTextComponent = HUD_COLOUR_INVALID;

	const s32 MaxLengthOfNumberString = 10;
	char NumberAsAsciiString[MaxLengthOfNumberString];

	u32 NumberStringLength = 0;
	if (pArrayOfNumbers && (ArrayElementToUse < SizeOfNumberArray))
	{
		NumberStringLength = pArrayOfNumbers[ArrayElementToUse].FillString(NumberAsAsciiString, MaxLengthOfNumberString);
		ColourForThisTextComponent = pArrayOfNumbers[ArrayElementToUse].GetColour();
	}
	else
	{
		safecpy(NumberAsAsciiString, "", MaxLengthOfNumberString);
	}

	Assertf(NumberStringLength != 0, "The number of numbers doesn't match the number of ~1~ in \"%s\"", 
		pOriginalStringForAssertMessage );

	CopyColouredGxtStringIntoStringAtIndex(pNewString, NewCharLoop, MaxSizeOfNewString, ColourForThisTextComponent, NumberAsAsciiString, NumberStringLength);
}


void CMessages::InsertSubStringIntoString(char *pNewString, u32 &NewCharLoop, const u32 MaxSizeOfNewString, const CSubStringWithinMessage *pArrayOfSubStrings, const u32 SizeOfSubStringArray, const u32 ArrayElementToUse, const char * ASSERT_ONLY(pOriginalStringForAssertMessage))
{
	eHUD_COLOURS ColourForThisTextComponent = HUD_COLOUR_INVALID;

	const char *pSubStringToInsert = NULL;
	if (pArrayOfSubStrings && (ArrayElementToUse < SizeOfSubStringArray))
	{
		pSubStringToInsert = pArrayOfSubStrings[ArrayElementToUse].GetString();
		ColourForThisTextComponent = pArrayOfSubStrings[ArrayElementToUse].GetColour();
	}

#if __ASSERT
	if (pSubStringToInsert == NULL)
	{
		Displayf("Main string is %s", pOriginalStringForAssertMessage );
		for (s32 debug_loop = 0; debug_loop < SizeOfSubStringArray; debug_loop++)
		{
			const char *pGxtSubString = pArrayOfSubStrings[debug_loop].GetString();
			if (pGxtSubString)
			{
				Displayf("SubString %d is %s", debug_loop, pGxtSubString );
			}
			else
			{
				Displayf("SubString %d is empty", debug_loop);
			}
		}
		Assertf(0, "CMessages::InsertSubStringIntoString - The number of substrings doesn't match the number of ~a~ in the main string. See TTY for details");
	}
#endif	//	__ASSERT

	CopyColouredGxtStringIntoStringAtIndex(pNewString, NewCharLoop, MaxSizeOfNewString, ColourForThisTextComponent, pSubStringToInsert);
}


void CMessages::InsertNumbersAndSubStringsIntoString(const char *pOriginalString, 
										const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
										const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
										char *pNewString, u32 MaxSizeOfNewString)
{
	if (pNewString == NULL)
	{
		Assertf(0, "CMessages::InsertNumbersAndSubStringsIntoString - Destination string is NULL");
		return;
	}

	if (pOriginalString == NULL)
	{
		pNewString[0] = rage::TEXT_CHAR_TERMINATOR;
		return;
	}

	if (pArrayOfNumbers == NULL)
	{
		SizeOfNumberArray = 0;
	}

	if (pArrayOfSubStrings == NULL)
	{
		SizeOfSubStringArray = 0;
	}

	u32 NumberOfNumbersInserted = 0;
	u32 NumberOfSubStringsInserted = 0;

	eTokenFormat MethodToUseForInsertingNumbers = TOKEN_FORMAT_NONE;
	eTokenFormat MethodToUseForInsertingSubStrings = TOKEN_FORMAT_NONE;

	u32 CharLoop = 0;
	u32 NewCharLoop = 0;
	while ( (pOriginalString[CharLoop] != rage::TEXT_CHAR_TERMINATOR) && (NewCharLoop < MaxSizeOfNewString) )
	{
		bool bNumberOrStringInserted = false;
		u32 IndexInteger = 0;
		eTokenFormat FormatUsedForThisPosition = CheckForInsertionToken(pOriginalString, CharLoop, IndexInteger);

		if (FormatUsedForThisPosition != TOKEN_FORMAT_NONE)
		{
			switch (FormatUsedForThisPosition)
			{
				case TOKEN_FORMAT_NONE :
					break;
				case TOKEN_FORMAT_NUMBER_ORIGINAL :
				case TOKEN_FORMAT_NUMBER_INDEXED :
					IndexInteger = GetArrayIndexAccordingToFormat(FormatUsedForThisPosition, IndexInteger, MethodToUseForInsertingNumbers, NumberOfNumbersInserted, pOriginalString);
					InsertNumberIntoString(pNewString, NewCharLoop, MaxSizeOfNewString, pArrayOfNumbers, SizeOfNumberArray, IndexInteger, pOriginalString);
					NumberOfNumbersInserted++;
					break;
				case TOKEN_FORMAT_SUBSTRING_ORIGINAL :
				case TOKEN_FORMAT_SUBSTRING_INDEXED :
					IndexInteger = GetArrayIndexAccordingToFormat(FormatUsedForThisPosition, IndexInteger, MethodToUseForInsertingSubStrings, NumberOfSubStringsInserted, pOriginalString);
					InsertSubStringIntoString(pNewString, NewCharLoop, MaxSizeOfNewString, pArrayOfSubStrings, SizeOfSubStringArray, IndexInteger, pOriginalString);
					NumberOfSubStringsInserted++;
					break;
			}

			switch (FormatUsedForThisPosition)
			{
				case TOKEN_FORMAT_NONE :
					break;
				case TOKEN_FORMAT_NUMBER_ORIGINAL :
				case TOKEN_FORMAT_SUBSTRING_ORIGINAL :
					bNumberOrStringInserted = true;
					CharLoop += 3;
					break;
				case TOKEN_FORMAT_NUMBER_INDEXED :
				case TOKEN_FORMAT_SUBSTRING_INDEXED :
					bNumberOrStringInserted = true;
					CharLoop += 5;
					break;
			}
		}

		if (!bNumberOrStringInserted)
		{	// Just copy the current character from the OriginalString to the NewString
			if (NewCharLoop < MaxSizeOfNewString)
			{
				pNewString[NewCharLoop++] = pOriginalString[CharLoop++];
			}
#if __ASSERT
			else
			{
				Assertf(0, "CMessages::InsertNumbersAndSubStringsIntoString - NewString has length %d. It's not long enough. Original string is %s", MaxSizeOfNewString, pOriginalString );
			}
#endif	//	__ASSERT
		}
	}

	if (NewCharLoop >= MaxSizeOfNewString)
	{
#if __ASSERT
		Assertf(0, "CMessages::InsertNumbersAndSubStringsIntoString - string is too long. The maximum length is %d. Original string is %s", MaxSizeOfNewString, pOriginalString );
#endif	//	__ASSERT

		NewCharLoop = MaxSizeOfNewString-1;

		u32 NumberOfTildes = 0;
		for (u32 char_loop = 0; char_loop < NewCharLoop; char_loop++)
		{
			if (pNewString[char_loop] == FONT_CHAR_TOKEN_DELIMITER)
			{
				NumberOfTildes++;
			}
		}

		if ((NumberOfTildes % 2) == 1)
		{	//	There are an odd number of tildes so add one as the second last character.
			//	This is so that CFont::GetTokenType doesn't have to scan forward until it finds a closing tilde.
			//	It seems to already handle an unrecognised token.
			pNewString[NewCharLoop-1] = FONT_CHAR_TOKEN_DELIMITER;
		}
	}

	pNewString[NewCharLoop] = rage::TEXT_CHAR_TERMINATOR;

//	This is what differentiates LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE from LITERAL_STRING_TYPE_SINGLE_FRAME
//	After creating your immediate-use literals, you should call InsertNumbersAndSubStringsIntoString() to construct your larger string.
//	As soon as the larger string has been constructed, we assume that it is safe to clear all the immediate-use literals on this thread (update or render).
	CScriptHud::ScriptLiteralStrings.ClearArrayOfImmediateUseLiteralStrings(false);
}


void CMessages::GetExpectedNumberOfTextComponents(char *pGxtString, s32 &ReturnNumberOfNumbers, s32 &ReturnNumberOfSubstrings)
{
	ReturnNumberOfNumbers = 0;
	ReturnNumberOfSubstrings = 0;

	if (pGxtString == NULL)
	{
		return;
	}

	u32 CharLoop = 0;
	while ( (pGxtString[CharLoop] != rage::TEXT_CHAR_TERMINATOR) )
	{
		u32 IndexInteger = 0;
		eTokenFormat FormatUsedForThisPosition = CheckForInsertionToken(pGxtString, CharLoop, IndexInteger);

		switch (FormatUsedForThisPosition)
		{
		case TOKEN_FORMAT_NONE :
			CharLoop++;
			break;
		case TOKEN_FORMAT_NUMBER_ORIGINAL :
			ReturnNumberOfNumbers++;
			CharLoop += 3;
			break;
		case TOKEN_FORMAT_SUBSTRING_ORIGINAL :
			ReturnNumberOfSubstrings++;
			CharLoop += 3;
			break;
		case TOKEN_FORMAT_NUMBER_INDEXED :
			ReturnNumberOfNumbers++;
			CharLoop += 5;
			break;
		case TOKEN_FORMAT_SUBSTRING_INDEXED :
			ReturnNumberOfSubstrings++;
			CharLoop += 5;
			break;
		}
	}
}

#if __WIN32PC
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::InsertPlayerControlKeysInString
// PURPOSE: Insert player control key descriptions into text
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::InsertPlayerControlKeysInString(char* UNUSED_PARAM(pBigString))
{

}
#endif // #if __WIN32PC


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::Update
// PURPOSE: updates any messages we may have
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::Update(void)
{
	s32 iCopyIndex;

	if (!BriefMessages[0].IsEmpty())
	{		// May have run out
		if (BriefMessages[0].HasDisplayTimeExpired())
		{
			BriefMessages[0].Clear(true, false);

			// Copy the new ones in
			iCopyIndex = 0;
			while (iCopyIndex < (MAX_NUM_BRIEF_MESSAGES-1) && !BriefMessages[iCopyIndex+1].IsEmpty())
			{
				BriefMessages[iCopyIndex] = BriefMessages[iCopyIndex+1];
				iCopyIndex++;
			}
			BriefMessages[iCopyIndex].Clear(false, false);

			// Set the start time of new message. (If there is one)
			BriefMessages[0].SetWhenStarted(fwTimer::GetTimeInMilliseconds());

			if (!BriefMessages[0].IsEmpty())
			{	//	Add the message to the previous brief array when it is first displayed
				BriefMessages[0].SetNewlyAddedToTheHeadOfTheQueue(true);
			}
		}
	}

	BriefMessages[0].AddToPreviousBriefsIfRequired();

	if (CPauseMenu::IsActive() && CWarningScreen::IsActive())
		return;

#if __BANK
	if (!CHudTools::bDisplayHud)
		return;
#endif


	//
	// update the subtitles:
	//

	CRGBA colour;

	char StringToDisplay[MAX_CHARS_IN_EXTENDED_MESSAGE];

	//	Update subtitles depending on when the script wants subtitles displayed
	BriefMessages[0].FillInString(StringToDisplay, NELEM(StringToDisplay));

	bool bDisplaySubtitles = true;
	
	// if hud is hidden then dont display subtitles when pausemenu is active
	if (CNewHud::IsFullHudHidden())
	{
		if ( CPauseMenu::IsActive() 
#if GTA_REPLAY
			|| CVideoEditorInterface::IsActive() 
#endif 
			)
		{
			bDisplaySubtitles = false;
		}
	}

	if ( (StringToDisplay[0] != 0 && bDisplaySubtitles) 
		&& (CPauseMenu::GetMenuPreference(PREF_SUBTITLES) || BriefMessages[0].GetDisplayEvenIfSubtitlesAreSwitchedOff() ) )
	{
		// set value:
		CSubtitleText::SetMessageText(StringToDisplay);
	}
	else
	{
		CSubtitleText::ClearMessage();
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearThisPrint
// PURPOSE: clear this message if its the one passed in
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearThisPrint(CSubtitleMessage &Message, bool bCompareMainStringsOnly)
{
	bool bAStringHasBeenCleared = true;

	char FirstString[MAX_CHARS_IN_EXTENDED_MESSAGE];
	char SecondString[MAX_CHARS_IN_EXTENDED_MESSAGE];

	if (!bCompareMainStringsOnly)
	{
		Message.FillInString(FirstString, NELEM(FirstString));
	}

	while (bAStringHasBeenCleared)
	{
		bAStringHasBeenCleared = false;
		u32 Index = 0;
		while (!bAStringHasBeenCleared && (Index < MAX_NUM_BRIEF_MESSAGES) && !BriefMessages[Index].IsEmpty())
		{
			bool bStringsAreEqual = false;

			if (bCompareMainStringsOnly)
			{
				if (BriefMessages[Index].DoesMainTextMatch(Message))
				{
					bStringsAreEqual = true;
				}
			}
			else
			{
				BriefMessages[Index].FillInString(SecondString, NELEM(SecondString));

				if ( strcmp(FirstString, SecondString) == 0 )
				{
					bStringsAreEqual = true;
				}
			}

			if (bStringsAreEqual)
			{
				BriefMessages[Index].Clear(true, false);

				// Copy the new ones in
				u32 CopyIndex = Index;
				while (CopyIndex < (MAX_NUM_BRIEF_MESSAGES-1) && !BriefMessages[CopyIndex+1].IsEmpty())
				{
					BriefMessages[CopyIndex] = BriefMessages[CopyIndex+1];
					CopyIndex++;
				}
				BriefMessages[CopyIndex].Clear(false, false);

				if ( (Index == 0) && (!BriefMessages[0].IsEmpty()) )
				{	//	If there's a new message to display
					BriefMessages[0].SetWhenStarted(fwTimer::GetTimeInMilliseconds());
					BriefMessages[0].SetNewlyAddedToTheHeadOfTheQueue(true);
				}

				bAStringHasBeenCleared = true;
			}
			else
			{
				Index++;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::IsThisMessageEmpty
// PURPOSE: returns whether this string is "empty" (pText is NULL, StringLength == 0, or pText contains only "~z~")
/////////////////////////////////////////////////////////////////////////////////////
bool CMessages::IsThisMessageEmpty(char const* pText)
{
	bool bMessageEmpty = (pText == NULL);
	if(pText)
	{
		s32 StringLength = CTextConversion::GetByteCount(pText);
		if(StringLength == 0 || 
			(StringLength == 3 && pText[0] == FONT_CHAR_TOKEN_DELIMITER && pText[1] == FONT_CHAR_TOKEN_DIALOGUE && pText[2] == FONT_CHAR_TOKEN_DELIMITER))
		{
			bMessageEmpty = true;
		}
	}

	return bMessageEmpty;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::DoesTextBeginWithThisControlCharacter
// PURPOSE: returns whether this string of text begins with the token passed in
/////////////////////////////////////////////////////////////////////////////////////
bool CMessages::DoesTextBeginWithThisControlCharacter(char const *pText, s32 TokenToSearchFor, s32 TokenToIgnore)
{
	enum eSearchStringState
	{
		EXPECTING_OPENING_DELIMITER,
		EXPECTING_CONTROL_CHARACTER,
		EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_SEARCH_FOR,
		EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_IGNORE
	};

	if (pText == NULL)
	{
		return true;
	}

	s32 StringLength = CTextConversion::GetByteCount(pText);
	bool bContinueChecking = true;
	s32 CharLoop = 0;
	eSearchStringState State = EXPECTING_OPENING_DELIMITER;
	while ( (CharLoop < StringLength) && bContinueChecking)
	{
		switch (State)
		{
			case EXPECTING_OPENING_DELIMITER :
				if (pText[CharLoop] == FONT_CHAR_TOKEN_DELIMITER)
				{
					State = EXPECTING_CONTROL_CHARACTER;
				}
				else
				{	//	String does not begin with a control character so return false
					bContinueChecking = false;
				}
				break;

			case EXPECTING_CONTROL_CHARACTER :
				if (pText[CharLoop] == TokenToSearchFor)
				{	//	Found ~TokenToSearchFor so now expect to find a closing delimiter
					State = EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_SEARCH_FOR;
				}
				else if (pText[CharLoop] == TokenToIgnore)
				{	// Found ~TokenToIgnore. Simon said this could happen so ignore it and continue looking for ~TokenToSearchFor~
					State = EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_IGNORE;
				}
				else
				{	//	String does not begin with ~TokenToSearchFor~ or ~TokenToIgnore~ so return false
					bContinueChecking = false;
				}
				break;

			case EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_SEARCH_FOR :
				if (pText[CharLoop] == FONT_CHAR_TOKEN_DELIMITER)
				{	//	Found a complete ~TokenToSearchFor~ so return true
					return true;
				}
				else
				{	//	String does not begin with ~TokenToSearchFor~ so return false
					bContinueChecking = false;
				}
				break;

			case EXPECTING_CLOSING_DELIMITER_AFTER_FINDING_TOKEN_TO_IGNORE :
				if (pText[CharLoop] == FONT_CHAR_TOKEN_DELIMITER)
				{	//	Found a complete ~TokenToIgnore~ so continue looking for ~TokenToSearchFor~
					State = EXPECTING_OPENING_DELIMITER;
				}
				else
				{	//	String does not begin with ~TokenToIgnore~ so give up
					bContinueChecking = false;
				}
				break;

		}
		CharLoop++;
	}

	//	String does not begin with ~TokenToSearchFor~ so return false
	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ShouldThisMessageBeExcludedFromThePreviousBriefs
// PURPOSE: returns whether the text should be added to the brief or not
/////////////////////////////////////////////////////////////////////////////////////
bool CMessages::ShouldThisMessageBeExcludedFromThePreviousBriefs(char const *pText)
{
	return (IsThisMessageEmpty(pText) || DoesTextBeginWithThisControlCharacter(pText, FONT_CHAR_TOKEN_EXCLUDE_FROM_PREVIOUS_BRIEFS, FONT_CHAR_TOKEN_DIALOGUE));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::IsThisTextSwitchedOffByFrontendSubtitleSetting
// PURPOSE: If text begins with ~z~ then it won't be displayed if Subtitles are
//			switched off
/////////////////////////////////////////////////////////////////////////////////////
bool CMessages::IsThisTextSwitchedOffByFrontendSubtitleSetting(char const *pText)
{	//	If text begins with ~z~ then it won't be displayed if Subtitles are switched off
	return (DoesTextBeginWithThisControlCharacter(pText, FONT_CHAR_TOKEN_DIALOGUE, FONT_CHAR_TOKEN_EXCLUDE_FROM_PREVIOUS_BRIEFS));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::AddMessage
// PURPOSE:  Adds one message to the Q to be displayed
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::AddMessage(char const* pText, s32 TextBlock, 
					   u32 Duration, bool bJumpQ, bool bAddToPrevBriefs, ePreviousBriefOverride PreviousBriefOverride, 
					   CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
					   CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
					   bool bUsesUnderscore, u32 iVoiceNameHash/*=0*/, u32 iMissionNameHash/*=0*/)
{
#if __ASSERT
	char TestString[MAX_CHARS_IN_EXTENDED_MESSAGE];

	InsertNumbersAndSubStringsIntoString(pText, 
		pArrayOfNumbers, SizeOfNumberArray, 
		pArrayOfSubStrings, SizeOfSubStringArray, 
		TestString, NELEM(TestString) );

#if __WIN32PC
	InsertPlayerControlKeysInString(&TestString[0]);
#endif // __WIN32PC
	u32 StringLength = CTextConversion::GetByteCount(TestString);
	Assertf(StringLength < MAX_CHARS_IN_EXTENDED_MESSAGE, "CMessages::AddMessage - string is too long");
#endif	//	__ASSERT

	if (ShouldThisMessageBeExcludedFromThePreviousBriefs(pText))
	{
		bAddToPrevBriefs = false;
		PreviousBriefOverride = PREVIOUS_BRIEF_NO_OVERRIDE;
	}

	s32 Index = 0;
	if (!bJumpQ)
	{	// Find the next empty one.
		while (Index < (MAX_NUM_BRIEF_MESSAGES) && !BriefMessages[Index].IsEmpty())
		{
			Index++;
		}
	}
	if (Index < MAX_NUM_BRIEF_MESSAGES)
	{		// Add this one
		BriefMessages[Index].Clear(true, false);

		BriefMessages[Index].Set(pText, TextBlock, 
			Duration, bAddToPrevBriefs, PreviousBriefOverride, 
			pArrayOfNumbers, SizeOfNumberArray, 
			pArrayOfSubStrings, SizeOfSubStringArray, 
			bUsesUnderscore, iVoiceNameHash, iMissionNameHash);

		if (Index == 0)
		{	//	Add the new message to the previous brief array when it is first displayed
			BriefMessages[0].SetNewlyAddedToTheHeadOfTheQueue(true);
		}
	}
}



// ******************************************************************************************************
// ** CPreviousHelpMessages
// ******************************************************************************************************
/*
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages::Init
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousHelpMessages::Init(s32 MaxSizeOfArray)
{
	Assertf(m_pArrayOfPreviousMessages == NULL, "CPreviousMessages::Init - array of previous messages has already been allocated");

	m_MaxNumberOfPreviousMessages = MaxSizeOfArray;
	m_pArrayOfPreviousMessages = rage_new CHelpMessageText[m_MaxNumberOfPreviousMessages];
	Assert(m_pArrayOfPreviousMessages);

	ClearAllPreviousBriefs();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages::Shutdown
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousHelpMessages::Shutdown()
{
	if (m_pArrayOfPreviousMessages)
	{
		delete[] m_pArrayOfPreviousMessages;
		m_pArrayOfPreviousMessages = NULL;
	}

	m_MaxNumberOfPreviousMessages = 0;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages::ClearAllPreviousBriefs
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousHelpMessages::ClearAllPreviousBriefs()
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousHelpMessages::ClearAllPreviousBriefs - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return;

	for (s32 loop = 0; loop < m_MaxNumberOfPreviousMessages; loop++)
	{
		m_pArrayOfPreviousMessages[loop].Clear();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages::AddBrief
// PURPOSE: adds message
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousHelpMessages::AddBrief(char *pMessage)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousHelpMessages::AddBrief - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return;

	// find our current number of previous brief messages:
	s32 NumberOfPreviousBriefs = m_MaxNumberOfPreviousMessages-1;
	while ( (NumberOfPreviousBriefs > 0) && (m_pArrayOfPreviousMessages[NumberOfPreviousBriefs].cHelpMessage[0] != '\0') )
	{
		NumberOfPreviousBriefs--;
	}

	if (NumberOfPreviousBriefs > 0)  // we have some previous messages, so lets readjust the list so we can add the new one at the top
	{
		s32 copy_loop = (NumberOfPreviousBriefs - 1);

		if (NumberOfPreviousBriefs == m_MaxNumberOfPreviousMessages)
		{	//	array of previous briefs is full - need to clear any literal strings for the final one (about to drop off the end of the array)
			m_pArrayOfPreviousMessages[copy_loop].Clear();
			copy_loop -= 1;
		}

		for (; copy_loop >= 0; copy_loop--)
		{
			m_pArrayOfPreviousMessages[copy_loop + 1] = m_pArrayOfPreviousMessages[copy_loop];
		}
	}

	CTextConversion::charStrcpy(m_pArrayOfPreviousMessages[0].cHelpMessage, pMessage, MAX_CHARS_IN_MESSAGE);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages::FillStringWithPreviousMessage
// PURPOSE: copies a previous brief message into a string
/////////////////////////////////////////////////////////////////////////////////////
bool CPreviousHelpMessages::FillStringWithPreviousMessage(s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfString)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::FillStringWithPreviousMessage - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return false;

	Assertf(IndexOfString >= 0, "CPreviousMessages::FillStringWithPreviousMessage - array index is negative");
	Assertf(IndexOfString < m_MaxNumberOfPreviousMessages, "CPreviousMessages::FillStringWithPreviousMessage - array index is too large");

	if ( (IndexOfString < 0) || (IndexOfString >= m_MaxNumberOfPreviousMessages) )
	{
		return false;
	}

	CTextConversion::charStrcpy(pGxtStringToFill, GetMessageText(IndexOfString), MaxLengthOfString);
	
	return true;
}
*/


// ******************************************************************************************************
// ** CPreviousMessages
// ******************************************************************************************************

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::Init
// PURPOSE: Inits previous brief
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousMessages::Init(s32 MaxSizeOfArray)
{
	Assertf(m_pArrayOfPreviousMessages == NULL, "CPreviousMessages::Init - array of previous messages has already been allocated");

	m_MaxNumberOfPreviousMessages = MaxSizeOfArray;
	m_pArrayOfPreviousMessages = rage_new CSubtitleMessage[m_MaxNumberOfPreviousMessages];
	Assert(m_pArrayOfPreviousMessages);

	for (s32 loop = 0; loop < m_MaxNumberOfPreviousMessages; loop++)
	{
		m_pArrayOfPreviousMessages[loop].Clear(false, false);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::Shutdown
// PURPOSE: shutdown of the brief messages
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousMessages::Shutdown()
{
	if (m_pArrayOfPreviousMessages)
	{
		delete[] m_pArrayOfPreviousMessages;
		m_pArrayOfPreviousMessages = NULL;
	}
	
	m_MaxNumberOfPreviousMessages = 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::ClearAllPreviousBriefs
// PURPOSE: clear the contents of all previous briefs
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousMessages::ClearAllPreviousBriefs()
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::ClearAllPreviousBriefs - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return;

	for (s32 loop = 0; loop < m_MaxNumberOfPreviousMessages; loop++)
	{
		m_pArrayOfPreviousMessages[loop].Clear(true, true);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::ClearAllPreviousBriefsBelongingToThisTextBlock
// PURPOSE: clear the contents of all previous briefs if it matches the text block
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousMessages::ClearAllPreviousBriefsBelongingToThisTextBlock(s32 TextBlock)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::ClearAllPreviousBriefsBelongingToThisTextBlock - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return;

	for (s32 loop = 0; loop < m_MaxNumberOfPreviousMessages; loop++)
	{
		if (m_pArrayOfPreviousMessages[loop].UsesTextFromThisBlock(TextBlock))
		{
			m_pArrayOfPreviousMessages[loop].Clear(true, true);
		}
	}

	// Previous Briefs - shift up any remaining messages to fill the gaps
	s32 CopyToIndex = 0;
	s32 CopyFromIndex = 0;
	while (CopyToIndex < m_MaxNumberOfPreviousMessages)
	{
		if (m_pArrayOfPreviousMessages[CopyToIndex].IsEmpty())
		{
			CopyFromIndex = (CopyToIndex + 1);
			while ( (CopyFromIndex < m_MaxNumberOfPreviousMessages) && (m_pArrayOfPreviousMessages[CopyFromIndex].IsEmpty()) )
			{
				CopyFromIndex++;
			}

			if (CopyFromIndex < m_MaxNumberOfPreviousMessages)
			{
				m_pArrayOfPreviousMessages[CopyToIndex] = m_pArrayOfPreviousMessages[CopyFromIndex];
				m_pArrayOfPreviousMessages[CopyFromIndex].Clear(false, false);
			}
		}

		CopyToIndex++;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::AddBrief
// PURPOSE: adds message to the brief
/////////////////////////////////////////////////////////////////////////////////////
void CPreviousMessages::AddBrief(CSubtitleMessage &SubtitleMessage)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::AddBrief - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return;

	// find our current number of previous brief messages:
	s32 NumberOfPreviousBriefs = 0;
	while ( (NumberOfPreviousBriefs < m_MaxNumberOfPreviousMessages) && !m_pArrayOfPreviousMessages[NumberOfPreviousBriefs].IsEmpty() )
	{
		if (m_pArrayOfPreviousMessages[NumberOfPreviousBriefs].Matches(SubtitleMessage))
		{
			return;
		}

		NumberOfPreviousBriefs++;
	}

	if (NumberOfPreviousBriefs > 0)  // we have some previous messages, so lets readjust the list so we can add the new one at the top
	{
		s32 copy_loop = (NumberOfPreviousBriefs - 1);

		if (NumberOfPreviousBriefs == m_MaxNumberOfPreviousMessages)
		{	//	array of previous briefs is full - need to clear any literal strings for the final one (about to drop off the end of the array)
			m_pArrayOfPreviousMessages[copy_loop].Clear(true, true);
			copy_loop -= 1;
		}
	
		for (; copy_loop >= 0; copy_loop--)
		{
			m_pArrayOfPreviousMessages[copy_loop + 1] = m_pArrayOfPreviousMessages[copy_loop];
		}
	}
	
	m_pArrayOfPreviousMessages[0] = SubtitleMessage;
	m_pArrayOfPreviousMessages[0].SetAllLiteralStringsOccurInPreviousBriefs();
}


bool CPreviousMessages::IsPreviousMessageEmpty(s32 IndexOfString)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::IsPreviousMessageEmpty - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return true;

	Assertf(IndexOfString >= 0, "CPreviousMessages::IsPreviousMessageEmpty - array index is negative");
	Assertf(IndexOfString < m_MaxNumberOfPreviousMessages, "CPreviousMessages::IsPreviousMessageEmpty - array index is too large");

	if ( (IndexOfString < 0) || (IndexOfString >= m_MaxNumberOfPreviousMessages) )
	{
		return true;
	}

	if(!m_pArrayOfPreviousMessages[IndexOfString].IsEmpty())
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::FillStringWithPreviousMessage
// PURPOSE: copies a previous brief message into a string
/////////////////////////////////////////////////////////////////////////////////////
bool CPreviousMessages::FillStringWithPreviousMessage(s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfString)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::FillStringWithPreviousMessage - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return false;

	Assertf(IndexOfString >= 0, "CPreviousMessages::FillStringWithPreviousMessage - array index is negative");
	Assertf(IndexOfString < m_MaxNumberOfPreviousMessages, "CPreviousMessages::FillStringWithPreviousMessage - array index is too large");

	if ( (IndexOfString < 0) || (IndexOfString >= m_MaxNumberOfPreviousMessages) )
	{
		return false;
	}

	if(!m_pArrayOfPreviousMessages[IndexOfString].IsEmpty())
	{
		m_pArrayOfPreviousMessages[IndexOfString].FillInString(pGxtStringToFill, MaxLengthOfString);
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages::GetPreviousMessageCharacterTXD
// PURPOSE: returns the previous message's character txd
/////////////////////////////////////////////////////////////////////////////////////
const char* CPreviousMessages::GetPreviousMessageCharacterTXD(s32 IndexOfMsg)
{
	Assertf(m_pArrayOfPreviousMessages, "CPreviousMessages::GetPreviousMessageCharacterTXD - array of previous messages has not been allocated");
	if (!m_pArrayOfPreviousMessages)
		return DEFAULT_CHARACTER_TXD;

	Assertf(IndexOfMsg >= 0, "CPreviousMessages::GetPreviousMessageCharacterTXD - array index is negative");
	Assertf(IndexOfMsg < m_MaxNumberOfPreviousMessages, "CPreviousMessages::GetPreviousMessageCharacterTXD - array index is too large");

	if ( (IndexOfMsg < 0) || (IndexOfMsg >= m_MaxNumberOfPreviousMessages) )
	{
		return DEFAULT_CHARACTER_TXD;
	}

	if(!m_pArrayOfPreviousMessages[IndexOfMsg].IsEmpty())
	{
		u32 iVoiceNameHash = m_pArrayOfPreviousMessages[IndexOfMsg].GetVoiceNameHash();
		u32 iMissionNameHash = m_pArrayOfPreviousMessages[IndexOfMsg].GetMissionNameHash();
		return CMessages::ms_dialogueCharacters.GetCharacterTXD(iVoiceNameHash, iMissionNameHash);
	}

	return DEFAULT_CHARACTER_TXD;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CMessages::AddToPreviousBriefArray
// PURPOSE :  Adds a message as the first previous message. Any messages already stored
//				are moved back one space to accommodate the new one. Only MAX_NUM_PREVIOUS_BRIEFS
//				previous messages are remembered
/////////////////////////////////////////////////////////////////////////////////
void CMessages::AddToPreviousBriefArray(CSubtitleMessage &SubtitleMessage, ePreviousBriefOverride PreviousBriefOverride)
{
	bool bAddToPreviousBriefs = false;
	bool bAddToDialogueBriefs = false;
	bool bAddToHelpTextBriefs = false;
	SubtitleMessage.DetermineWhichPreviousBriefArrayToUse(bAddToPreviousBriefs, bAddToDialogueBriefs, bAddToHelpTextBriefs);

	//	If bAddToPreviousBriefs is false at this stage then I think it's safer to leave it as false
	//	even if PreviousBriefOverride has a "force" value
	switch (PreviousBriefOverride)
	{
	case PREVIOUS_BRIEF_NO_OVERRIDE :
		break;
	case PREVIOUS_BRIEF_FORCE_DIALOGUE :
		bAddToDialogueBriefs = true;
		break;
	case PREVIOUS_BRIEF_FORCE_GOD_TEXT :
		bAddToDialogueBriefs = false;
		break;
	}

	if (bAddToPreviousBriefs)
	{
		if (bAddToDialogueBriefs)
		{
			PreviousDialogueMessages.AddBrief(SubtitleMessage);
		}
		else if (bAddToHelpTextBriefs)
		{
			PreviousHelpTextMessages.AddBrief(SubtitleMessage);
		}
		else
		{
			PreviousMissionMessages.AddBrief(SubtitleMessage);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::ClearAllPreviousBriefsThatBelongToThisTextBlock
// PURPOSE: clears any brief that belongs to the passed text block
/////////////////////////////////////////////////////////////////////////////////////
void CMessages::ClearAllPreviousBriefsThatBelongToThisTextBlock(s32 TextBlock)
{
	PreviousDialogueMessages.ClearAllPreviousBriefsBelongingToThisTextBlock(TextBlock);
	PreviousMissionMessages.ClearAllPreviousBriefsBelongingToThisTextBlock(TextBlock);
	PreviousHelpTextMessages.ClearAllPreviousBriefsBelongingToThisTextBlock(TextBlock);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::GetMaxNumberOfPreviousMessages
// PURPOSE: returns the number of previous messages in use
/////////////////////////////////////////////////////////////////////////////////////
s32 CMessages::GetMaxNumberOfPreviousMessages(ePreviousMessageTypes iMessageType)
{
	switch (iMessageType)
	{
		case PREV_MESSAGE_TYPE_DIALOG:
			return PreviousDialogueMessages.GetMaxNumberOfMessages();
		case PREV_MESSAGE_TYPE_MISSION:
			return PreviousMissionMessages.GetMaxNumberOfMessages();
		case PREV_MESSAGE_TYPE_HELP:
			return PreviousHelpTextMessages.GetMaxNumberOfMessages();
	}

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages::FillStringWithPreviousMessage
// PURPOSE: fills the passed string with a brief message
/////////////////////////////////////////////////////////////////////////////////////
bool CMessages::FillStringWithPreviousMessage(ePreviousMessageTypes iMessageType, s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfStringToFill)
{
	switch (iMessageType)
	{
		case PREV_MESSAGE_TYPE_DIALOG:
			return PreviousDialogueMessages.FillStringWithPreviousMessage(IndexOfString, pGxtStringToFill, MaxLengthOfStringToFill);
		case PREV_MESSAGE_TYPE_MISSION:
			return PreviousMissionMessages.FillStringWithPreviousMessage(IndexOfString, pGxtStringToFill, MaxLengthOfStringToFill);
		case PREV_MESSAGE_TYPE_HELP:
			return PreviousHelpTextMessages.FillStringWithPreviousMessage(IndexOfString, pGxtStringToFill, MaxLengthOfStringToFill);
	}

	return false;
}

bool CMessages::IsPreviousMessageEmpty(ePreviousMessageTypes iMessageType, s32 IndexOfString)
{
	switch (iMessageType)
	{
	case PREV_MESSAGE_TYPE_DIALOG:
		return PreviousDialogueMessages.IsPreviousMessageEmpty(IndexOfString);
	case PREV_MESSAGE_TYPE_MISSION:
		return PreviousMissionMessages.IsPreviousMessageEmpty(IndexOfString);
	case PREV_MESSAGE_TYPE_HELP:
		return PreviousHelpTextMessages.IsPreviousMessageEmpty(IndexOfString);
	}

	return true;
}

const char* CMessages::GetPreviousMessageCharacterTXD(s32 IndexOfMsg)
{
	return PreviousDialogueMessages.GetPreviousMessageCharacterTXD(IndexOfMsg);
}



// ******************************************************************************************************
// ** CHelpMessage
// ******************************************************************************************************

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::Init
// PURPOSE: initialises some variables
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::Init()
{
	for (s32 i = 0; i < MAX_HELP_TEXT_SLOTS; i++)
	{
		m_HelpMessageText[i][0] = 0;
		m_bHelpMessagesFading[i] = false;
		m_iNewText[i] = HELP_TEXT_STATE_NONE;
		m_vScreenPosition[i].x = -9999.0f;
		m_vScreenPosition[i].y = -9999.0f;
		m_iArrowDirection[i] = HELP_TEXT_ARROW_FORCE_RESET;
		m_iFloatingTextOffset[i] = 0;
		m_iStyle[i] = HELP_TEXT_STYLE_NORMAL;
		m_iColour[i] = CRGBA(0,0,0,214);
		m_bTriggerSound[i] = false;
		m_OverrideDuration[i] = -1;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetMessageText
// PURPOSE: sets the content of the help message text
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetMessageText(s32 iId, const char *pHelpMessage, 
	const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
	const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
	bool bPlaySound, bool bDisplayForever, s32 OverrideDuration, bool WIN32PC_ONLY(bIME))
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CHelpMessage::SetMessageText only allowed to be called on UPDATE thread");
		return;
	}

#if RSG_PC
	if(ioKeyboard::ImeIsInProgress() && !bIME)
		return;
#endif // RSG_PC

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		if (!pHelpMessage)
		{
			if (DoesMessageTextExist(iId))
			{
				Clear(iId, true);
			}
		}
		else
		{
			char NewString[MAX_CHARS_IN_EXTENDED_MESSAGE];

			CMessages::InsertNumbersAndSubStringsIntoString(pHelpMessage, 
				pArrayOfNumbers, SizeOfNumberArray, 
				pArrayOfSubStrings, SizeOfSubStringArray, 
				NewString, NELEM(NewString));

	#if __WIN32PC
			CMessages::InsertPlayerControlKeysInString(&NewString[0]);
	#endif // #if __WIN32PC

	#if __ASSERT
			s32 StringLength = CTextConversion::GetByteCount(NewString);
			Assertf(StringLength < MAX_CHARS_IN_MESSAGE, "CHelpMessage::SetMessageText - string is too long");
	#endif


			if ( (CNewHud::GetHelpTextWaitingForActionScriptResponse(iId)) || ( strcmp(NewString, m_HelpMessageText[iId]) != 0 ) )
			{
				safecpy( m_HelpMessageText[iId], NewString);

				m_iNewText[iId] = HELP_TEXT_STATE_NEW;
				m_bHelpMessagesFading[iId] = false;
				m_vScreenPosition[iId].x = -9999.0f;
				m_vScreenPosition[iId].y = -9999.0f;
				m_iArrowDirection[iId] = HELP_TEXT_ARROW_FORCE_RESET;
				m_iFloatingTextOffset[iId] = 0;
				m_iStyle[iId] = HELP_TEXT_STYLE_NORMAL;
				m_iColour[iId] = CRGBA(0,0,0,214);
				m_bTriggerSound[iId] = bPlaySound;
				m_OverrideDuration[iId] = OverrideDuration;

				// will ignore any CLEAR_HELP_TEXT callbacks that may come in from a previous CLEAR as we have set the help text again
				CNewHud::SetHelpTextWaitingForActionScriptResponse(iId, false);
			}
			else
			{
	/*#if __ASSERT
				//
				// simple check to catch any instances where help text is set 2 frames in a row.
				// this is a waste
				//
				static u32 iPreviousFrameHelpSet = 0;

				if (iPreviousFrameHelpSet == GRCDEVICE.GetFrameCounter()-1)
				{
					uiAssertf(0, "Help text shouldn't be called every frame - use the FOREVER param and CLEAR instead");
				}

				iPreviousFrameHelpSet = GRCDEVICE.GetFrameCounter(); 
	#endif // __ASSERT*/

				m_iNewText[iId] = HELP_TEXT_STATE_SAME;
				m_bTriggerSound[iId] = false;
			}

			if (bDisplayForever)
			{
				m_iNewText[iId] = HELP_TEXT_STATE_FOREVER;
			}
		}
	}
}

void CHelpMessage::SetMessageTextAndAddToBrief(s32 iId, const char *pHelpMessageLabel, 
											   CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
											   CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
											   bool bPlaySound, bool bDisplayForever, s32 OverrideDuration)
{

	char *pHelpMessage = TheText.Get(pHelpMessageLabel);
	s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

	CHelpMessage::SetMessageText(iId, pHelpMessage, 
		pArrayOfNumbers, SizeOfNumberArray, 
		pArrayOfSubStrings, SizeOfSubStringArray, 
		bPlaySound, bDisplayForever, OverrideDuration);

	CSubtitleMessage PreviousBrief;

	PreviousBrief.Set(pHelpMessage, MainTextBlock, 
		pArrayOfNumbers, SizeOfNumberArray, 
		pArrayOfSubStrings, SizeOfSubStringArray, 
		false, true);

	CMessages::AddToPreviousBriefArray(PreviousBrief, CScriptHud::GetNextMessagePreviousBriefOverride());
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetScreenPosition
// PURPOSE: sets the position of the help text on the screen
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetScreenPosition(s32 iId, Vector2 vScreenPos)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		m_vScreenPosition[iId] = vScreenPos;

		if (m_vScreenPosition[iId].x > 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			m_vScreenPosition[iId].x = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;

		if (m_vScreenPosition[iId].x < OFFSET_FROM_EDGE_OF_SCREEN)
			m_vScreenPosition[iId].x = OFFSET_FROM_EDGE_OF_SCREEN;

		if (m_vScreenPosition[iId].y > 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			m_vScreenPosition[iId].y = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;

		if (m_vScreenPosition[iId].y < OFFSET_FROM_EDGE_OF_SCREEN)
			m_vScreenPosition[iId].y = OFFSET_FROM_EDGE_OF_SCREEN;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetWorldPosition
// PURPOSE: sets the position of the help text in the world and onto the screen
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetWorldPosition(s32 iId, Vector3::Vector3Param vWorldPositon)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		Vector3 vWorldPositionV(vWorldPositon);

		eTEXT_ARROW_ORIENTATION iArrowDir;

		SetScreenPosition(iId, CHudTools::GetHudPosFromWorldPos(vWorldPositionV, &iArrowDir));
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetWorldPositionFromEntity
// PURPOSE: sets the position of the help text in the world from an entity pos and
//			to a screen pos
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetWorldPositionFromEntity(s32 iId, CPhysical* pEntity, Vector2 vOffset)
{
	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			SetWorldPosition(iId, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			CPed *pDriverPed = static_cast<CVehicle*>(pEntity)->GetDriver();

			if (pDriverPed)
			{	
				SetWorldPosition(iId, VEC3V_TO_VECTOR3(pDriverPed->GetTransform().GetPosition()));
			}
			else
			{
				SetWorldPosition(iId, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
			}
		}
		else if (pEntity->GetIsTypeObject())
		{
			SetWorldPosition(iId, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
		}

		// apply the offset and pass it to be the screen pos
		Vector2 vScreenPosWithOffset = (GetScreenPosition(iId) + vOffset);
		SetScreenPosition(iId, vScreenPosWithOffset);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetStyle
// PURPOSE: sets the style of this help text
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetStyle(s32 iId, s32 iNewStyle, s32 iNewArrowDirection, s32  iFloatingTextOffset, s32 iHudColour, s32 iAlpha)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		m_iStyle[iId] = (eTEXT_STYLE)iNewStyle;
		m_iArrowDirection[iId] = (eTEXT_ARROW_ORIENTATION)iNewArrowDirection;
		m_iFloatingTextOffset[iId] = iFloatingTextOffset;

		if (iAlpha >= 0 && iAlpha <= 255)
		{
			m_iColour[iId] = CHudColour::GetRGB((eHUD_COLOURS)iHudColour, (u8)iAlpha);
		}
		else
		{
			m_iColour[iId] = CHudColour::GetRGBA((eHUD_COLOURS)iHudColour);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::Clear
// PURPOSE: flags to clear the help message
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::Clear(s32 iId, bool bClearNow)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CHelpMessage::Clear only allowed to be called on UPDATE thread");
		return;
	}

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		if (m_HelpMessageText[iId][0] != 0)
		{
			m_bTriggerSound[iId] = false;
			m_bHelpMessagesFading[iId] = !bClearNow;
			CNewHud::ClearHelpText(iId, bClearNow);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::ClearAll
// PURPOSE: clears all help messages
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::ClearAll()
{
	for (s32 i = 0; i < MAX_HELP_TEXT_SLOTS; i++)
	{
		Clear(i, true);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::ClearMessage
// PURPOSE: clears the actual help message content
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::ClearMessage(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		m_HelpMessageText[iId][0] = 0;
		m_bHelpMessagesFading[iId] = false;
		m_iNewText[iId] = HELP_TEXT_STATE_NONE;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::GetMessageText
// PURPOSE: returns the content of the help message text
/////////////////////////////////////////////////////////////////////////////////////
char *CHelpMessage::GetMessageText(s32 iId)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CHelpMessage::GetMessageText only allowed to be called on UPDATE thread");
		return NULL;
	}	

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return &m_HelpMessageText[iId][0];
	}
	else
	{
		return NULL;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::DoesMessageTextExist
// PURPOSE: returns whether a help message is currently set or not
/////////////////////////////////////////////////////////////////////////////////////
bool CHelpMessage::DoesMessageTextExist(s32 iId)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		AssertMsg(0, "CHelpMessage::GetMessageText only allowed to be called on UPDATE thread");
		return false;
	}	

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_HelpMessageText[iId][0] != 0);
	}
	else
	{
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::IsMessageFadingOut
// PURPOSE: returns whether a help message is currently fading out
/////////////////////////////////////////////////////////////////////////////////////
bool CHelpMessage::IsMessageFadingOut(s32 iId)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		AssertMsg(0, "CHelpMessage::GetMessageText only allowed to be called on UPDATE thread");
		return false;
	}

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_HelpMessageText[iId][0] != 0) && m_bHelpMessagesFading[iId];
	}
	else
	{
		return false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::IsHelpTextNew
// PURPOSE: returns whether the help text is new
/////////////////////////////////////////////////////////////////////////////////////
bool CHelpMessage::IsHelpTextNew(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_iNewText[iId] == HELP_TEXT_STATE_NEW);
	}
	else
	{
		return false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::IsHelpTextSame
// PURPOSE: returns whether the help text is the same as the previous frame
/////////////////////////////////////////////////////////////////////////////////////
bool CHelpMessage::IsHelpTextSame(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_iNewText[iId] == HELP_TEXT_STATE_SAME);
	}
	else
	{
		return false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::IsHelpTextForever
// PURPOSE: returns whether the help text is to display forever
/////////////////////////////////////////////////////////////////////////////////////
bool CHelpMessage::IsHelpTextForever(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_iNewText[iId] == HELP_TEXT_STATE_FOREVER);
	}
	else
	{
		return false;
	}
}

bool CHelpMessage::WasHelpTextForever(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		return (m_iNewText[iId] == HELP_TEXT_STATE_FOREVER_IDLE);
	}
	else
	{
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::SetHelpMessageAsProcessed
// PURPOSE: flags the help message as processed - called after the scaleform movie
//			has been updated with the content
/////////////////////////////////////////////////////////////////////////////////////
void CHelpMessage::SetHelpMessageAsProcessed(s32 iId)
{
	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		if(IsHelpTextForever(iId))
		{
			m_iNewText[iId] = HELP_TEXT_STATE_FOREVER_IDLE;
		}
		else
		{
			m_iNewText[iId] = HELP_TEXT_STATE_IDLE;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage::GetDirectionHelpText
// PURPOSE: returns the direction of the help text
/////////////////////////////////////////////////////////////////////////////////////
eTEXT_ARROW_ORIENTATION CHelpMessage::GetDirectionHelpText(s32 iId, Vector2 vNewPosition)
{
	eTEXT_ARROW_ORIENTATION iDirectionOffScreen = HELP_TEXT_ARROW_NORMAL;

	if (uiVerifyf(iId < MAX_HELP_TEXT_SLOTS, "CHelpMessage: Invalid Help Message Slot Passed: %d", iId))
	{
		// work out whether the new position of the help text is offscreen
		if (vNewPosition.x >= 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			iDirectionOffScreen = HELP_TEXT_ARROW_EAST;
		else if (vNewPosition.x <= OFFSET_FROM_EDGE_OF_SCREEN)
			iDirectionOffScreen = HELP_TEXT_ARROW_WEST;
		else if (vNewPosition.y >= 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			iDirectionOffScreen = HELP_TEXT_ARROW_SOUTH;
		else if (vNewPosition.y <= OFFSET_FROM_EDGE_OF_SCREEN)
			iDirectionOffScreen = HELP_TEXT_ARROW_NORTH;
	}

	return iDirectionOffScreen;
}


// ******************************************************************************************************
// ** CSubtitleText
// ******************************************************************************************************

char CSubtitleText::m_SubtitleMessageText[MAX_CHARS_IN_EXTENDED_MESSAGE];


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSubtitleText::Init
// PURPOSE: initialises some variables
/////////////////////////////////////////////////////////////////////////////////////
void CSubtitleText::Init()
{
	m_SubtitleMessageText[0] = 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSubtitleText::SetMessageText
// PURPOSE: stores the new text and sets it up in Scaleform if its different
/////////////////////////////////////////////////////////////////////////////////////
void CSubtitleText::SetMessageText(char *pText)
{
	if ( strcmp(m_SubtitleMessageText, pText) != 0 )
	{
		safecpy(m_SubtitleMessageText, pText);
		
		CNewHud::SetSubtitleText(pText);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSubtitleText::ClearMessage
// PURPOSE: clears the text
/////////////////////////////////////////////////////////////////////////////////////
void CSubtitleText::ClearMessage()
{
	if (m_SubtitleMessageText[0] != 0)
	{
		m_SubtitleMessageText[0] = 0;
		CNewHud::ClearSubtitleText();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSubtitleText::GetMessageText
// PURPOSE: returns the content of the subtitle text
/////////////////////////////////////////////////////////////////////////////////////
char *CSubtitleText::GetMessageText()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CSubtitleText::GetMessageText only allowed to be called on UPDATE thread");
		return NULL;
	}	

	return &m_SubtitleMessageText[0];
}





// ******************************************************************************************************
// ** CSavingGameMessage
// ******************************************************************************************************


char CSavingGameMessage::m_cMessageText[MAX_CHARS_IN_MESSAGE];
CSavingGameMessage::eICON_STYLE CSavingGameMessage::m_iIconStyle;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::Init
// PURPOSE: initialises some variables
/////////////////////////////////////////////////////////////////////////////////////
void CSavingGameMessage::Init()
{
	Clear();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::SetMessageText
// PURPOSE: stores the new text
/////////////////////////////////////////////////////////////////////////////////////
void CSavingGameMessage::SetMessageText(const char *pText, eICON_STYLE iconStyle)
{
	safecpy(m_cMessageText, pText);

	m_iIconStyle = iconStyle;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::GetMessageText
// PURPOSE: returns the content of the subtitle text
/////////////////////////////////////////////////////////////////////////////////////
char *CSavingGameMessage::GetMessageText()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CSavingGameMessage::GetMessageText only allowed to be called on UPDATE thread");
		return NULL;
	}	

	return &m_cMessageText[0];
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::GetIconStyle
// PURPOSE: returns the icon style as an integer
/////////////////////////////////////////////////////////////////////////////////////
s32 CSavingGameMessage::GetIconStyle()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		AssertMsg(0, "CSavingGameMessage::GetMessageText only allowed to be called on UPDATE thread");
		return 0;
	}	

	return s32( m_iIconStyle == SAVEGAME_ICON_STYLE_SAVING_NO_MESSAGE ? SAVEGAME_ICON_STYLE_SAVING : m_iIconStyle);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::IsSavingMessageActive
// PURPOSE: returns whether the saving message is active or not
/////////////////////////////////////////////////////////////////////////////////////
bool CSavingGameMessage::IsSavingMessageActive()
{
	return (m_cMessageText[0] != 0) || m_iIconStyle == SAVEGAME_ICON_STYLE_SAVING_NO_MESSAGE;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage::Clear
// PURPOSE: clears the message
/////////////////////////////////////////////////////////////////////////////////////
void CSavingGameMessage::Clear()
{
	m_cMessageText[0] = 0;
	m_iIconStyle = SAVEGAME_ICON_STYLE_NONE;
}




// ******************************************************************************************************
// ** CLoadingText
// ******************************************************************************************************


char CLoadingText::m_cText[MAX_CHARS_IN_MESSAGE];
bool CLoadingText::bActive = false;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::Init
// PURPOSE: initialises some variables
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingText::Init()
{
	bActive = false;

	Clear();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::Shutdown
// PURPOSE: clears and shuts down the scaleform movie for the Loading Text
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingText::Shutdown()
{
	Clear();
	SetActive(false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::SetActive
// PURPOSE: sets loading text on/off
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingText::SetActive(bool bSet)
{
	if (bSet)
	{
		if (!bActive)
		{
			bActive = true;
			Clear();
		}
	}
	else
	{
		if (bActive)
		{
			bActive = false;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::SetText
// PURPOSE: stores the new text
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingText::SetText(const char *pText)
{
	safecpy(m_cText, pText);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::GetText
// PURPOSE: returns the content of the loading text
/////////////////////////////////////////////////////////////////////////////////////
char *CLoadingText::GetText()
{
	return &m_cText[0];
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::IsActive
// PURPOSE: returns whether the loading text is active or not
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingText::IsActive()
{
	return (m_cText[0] != 0);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText::Clear
// PURPOSE: clears the message
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingText::Clear()
{
	m_cText[0] = 0;
}



//
// name:		CLoadingText::UpdateAndRenderLoadingText
// description:	update and render the loading text
void CLoadingText::UpdateAndRenderLoadingText(float fCurrentTimer)
{
	if (!bActive)
		return;

#define FADEOUT_WITHOUT_LOADING_TEXT	(4.0f)

	CMessages::ClearMessages();

	if(CWarningScreen::IsActive())
	{
		if (TheText.IsBasicTextLoaded())  // only render text if the text file is loaded
		{
			if(CControlMgr::GetMainFrontendControl(false).GetFrontendAccept().IsPressed())
			{
				CWarningScreen::Remove();
			}
		}
	}
	else
	{
		if ( (fCurrentTimer > FADEOUT_WITHOUT_LOADING_TEXT) || CSavingMessage::ShouldDisplaySavingSpinner() )
		{
			if (CBusySpinner::CanRender() && CBusySpinner::HasBodyText())  // if we have text then we need to display the real busy spinner now
			{
				CBusySpinner::Render();
			}
			else
			{
				// if we have no text, lets just render same way as we have done (this limits the change of using the busy spinner here to just a few places)
				Vector2 vSize(0.0073125f, 0.013f);
				const Vector2 vPos = CHudTools::CalculateHudPosition(Vector2(0,0), vSize, 'R', 'B');
				CPauseMenu::RenderAnimatedSpinner(vSize, vPos, true, false, (CSavingMessage::ShouldDisplaySavingSpinner() REPLAY_ONLY(|| CReplayMgr::IsSaving()))?true:false);
			}
		}
	}
}

// eof
