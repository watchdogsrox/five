/////////////////////////////////////////////////////////////////////////////////
//
// FILE :      messages.h
// PURPOSE :   Messages handling. Keeps track of messages that are to be displayed.
// AUTHOR :    Obbe, GraemeW, DerekP (13/12/99)
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef MESSAGES_H
#define MESSAGES_H

#include "parser/macros.h"

#include "frontend/hud_colour.h"
#include "text/text.h"

#define MAX_CHARS_IN_MESSAGE		(600)
#define MAX_CHARS_IN_EXTENDED_MESSAGE		(MAX_CHARS_IN_MESSAGE * 2)  // subtitles and some strings in info panels in MP pause have twice the size of a standard message, mainly to deal with the very long russian texts ie 1435060

enum
{
	SCRIPT_PRINT_NO_EXTRA_PARAMS,
	SCRIPT_PRINT_ONE_SUBSTRING,
	SCRIPT_PRINT_ONE_LITERAL_STRING,
	SCRIPT_PRINT_TWO_LITERAL_STRINGS,
	SCRIPT_PRINT_ONE_NUMBER,
	SCRIPT_PRINT_TWO_NUMBERS,
	SCRIPT_PRINT_THREE_NUMBERS,
	SCRIPT_PRINT_FOUR_NUMBERS,
	SCRIPT_PRINT_FIVE_NUMBERS,
	SCRIPT_PRINT_SIX_NUMBERS
};

enum eTEXT_STYLE
{
	HELP_TEXT_STYLE_NORMAL = 0,	// default (traditional GTA style help text)
	HELP_TEXT_STYLE_TAGGABLE,		// movable help text with directional arrow
	HELP_TEXT_STYLE_AMMUNATION,
	HELP_TEXT_STYLE_MP_FREEMODE
};

enum eTEXT_ARROW_ORIENTATION  // arrow positioning in help text
{
	HELP_TEXT_ARROW_NORMAL = 0,
	HELP_TEXT_ARROW_NORTH,
	HELP_TEXT_ARROW_EAST,
	HELP_TEXT_ARROW_SOUTH,
	HELP_TEXT_ARROW_WEST,
	HELP_TEXT_ARROW_FORCE_RESET
};


enum eTokenFormat
{
	TOKEN_FORMAT_NONE,
	TOKEN_FORMAT_NUMBER_ORIGINAL,		//	~1~
	TOKEN_FORMAT_NUMBER_INDEXED,		//	~1_.~ where . is a digit (0 to 9)
	TOKEN_FORMAT_SUBSTRING_ORIGINAL,	//	~a~
	TOKEN_FORMAT_SUBSTRING_INDEXED		//	~a_.~ where . is a digit (0 to 9)
};

//////////////////////////////////////////////////////////////////////////
// SDialogueCharacters
//////////////////////////////////////////////////////////////////////////
struct SDialogueCharacters
{
	struct SCharacterData
	{
		atFinalHashString m_txd;
		atHashWithStringNotFinal m_VoiceNameHash;
		atHashWithStringNotFinal m_MissionNameHash;
		PAR_SIMPLE_PARSABLE;
	};

	atArray<SCharacterData> m_characterInfo;

	const char* GetCharacterTXD(u32 iVoiceNameHash, u32 iMissionNameHash = 0);

	PAR_SIMPLE_PARSABLE;
};



class CNumberWithinMessage
{
public:
	CNumberWithinMessage() { Clear(); }

	void Set(s32 IntegerToDisplay);
	void Set(float FloatToDisplay, s32 NumberOfDecimalPlaces);

	void Clear() { m_NumberOfDecimalPlaces = NO_NUMBER_TO_DISPLAY; m_Colour = HUD_COLOUR_INVALID; }
	
	void SetColour(eHUD_COLOURS NewColour) { m_Colour = NewColour; }
	eHUD_COLOURS GetColour() const { return m_Colour; }

	//	Returns length of new string
	s32 FillString(char *pStringToFill, s32 MaxLengthOfString) const;

	bool operator==(const CNumberWithinMessage& otherNumber) const;
	bool operator!=(const CNumberWithinMessage& otherNumber) const { return !operator==(otherNumber); }

#if !__FINAL
	void DebugPrintContents();
#endif	//	!__FINAL

private:
	static const s32 DISPLAY_INTEGER = -1;
	static const s32 NO_NUMBER_TO_DISPLAY = -2;

	union 
	{
		s32 m_Integer;
		float m_Float;
	};

	// Should m_NumberOfDecimalPlaces be an s8?
	s32 m_NumberOfDecimalPlaces;	//	DISPLAY_INTEGER = Integer, 
									//	NO_NUMBER_TO_DISPLAY = don't display.

	eHUD_COLOURS m_Colour;
};


class CSubStringWithinMessage
{
public:
	enum eLiteralStringType
	{
		LITERAL_STRING_TYPE_PERSISTENT,
		LITERAL_STRING_TYPE_SINGLE_FRAME,
		LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE,			//	The arrays of immediate-use literals will be cleared at the end of CMessages::InsertNumbersAndSubStringsIntoString()
		LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG		//	They are only intended to be used for the construction of a larger string by InsertNumbersAndSubStringsIntoString()
	};

public:
	CSubStringWithinMessage() { m_BlockContainingThisText = NO_STRING_TO_DISPLAY; m_Colour = HUD_COLOUR_INVALID; }

	void SetTextLabel(const char *pTextLabel);
	void SetTextLabelHashKey(u32 HashKeyOfTextLabel);
	
	 // a string that is handled elsewhere that doesn't need to be stored by this system
	// COULD probably replace LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE outright, but relies on the caller to not overwrite the value before insertion
	void SetCharPointer(const char* pLocalizedString);

	void SetLiteralString(const char *pLiteralString, eLiteralStringType literalStringType, bool bUseDebugPortionOfSingleFrameArray);

	void SetPersistentLiteralStringOccursInSubtitles(bool bOccursInSubtitles);

	void Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs);

	bool UsesTextFromThisBlock(s32 TextBlockIndex);
	u32 GetStringCharacterCount();
	const char *GetString() const;

	void SetColour(eHUD_COLOURS NewColour) { m_Colour = NewColour; }
	eHUD_COLOURS GetColour() const { return m_Colour; }

	void DetermineWhichPreviousBriefArrayToUse(bool &bAddToPreviousBriefs, bool &bAddToDialogueBriefs);

	void SetLiteralStringOccursInPreviousBriefs();

	bool operator==(const CSubStringWithinMessage& otherSubString) const;
	bool operator!=(const CSubStringWithinMessage& otherSubString) const { return !operator==(otherSubString); }

private:
	static const s32 LITERAL_STRING = -2;
	static const s32 NO_STRING_TO_DISPLAY = -3;
	static const s32 CHAR_POINTER = -4;

	union
	{
		const char* m_pText;
		struct {
			s16 m_LiteralStringIndex;
			u16 m_LiteralStringType;	//	Uses eLiteralStringType
		} uLiteralString;
	};

	s32 m_BlockContainingThisText;
									//	LITERAL_STRING = use m_LiteralStringIndex instead of m_pTextFromTextFile
									//	NO_STRING_TO_DISPLAY = nothing to display
	eHUD_COLOURS m_Colour;
};


class CSubtitleMessage
{
public:
	void Set(char const *pText, s32 TextBlock, 
		CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
		bool bUsesUnderscore, bool bHelpText, u32 iVoiceNameHash=0, u32 iMissionNameHash=0);

	void Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs);

	bool UsesTextFromThisBlock(s32 TextBlockIndex);

	bool IsEmpty() { return (m_pMessage == NULL); }

	void FillInString(char *pStringToFillIn, u32 MaxLengthOfString);

	//	I'm not sure how useful this is. Shouldn't we be comparing all numbers and substrings as well?
	bool DoesMainTextMatch(CSubtitleMessage &OtherSubtitle);

	bool Matches(CSubtitleMessage &OtherSubtitle);

	void DetermineWhichPreviousBriefArrayToUse(bool &bAddToPreviousBriefs, bool &bAddToDialogueBriefs, bool &bAddToHelpTextBriefs);

	void SetAllLiteralStringsOccurInPreviousBriefs();

	u32 GetVoiceNameHash() const { return m_iVoiceNameHash; }
	u32 GetMissionNameHash() const { return m_iMissionNameHash; }

private:
	static const u32 MAX_SUBSTRINGS_IN_SUBTITLES = 6;
	static const u32 MAX_NUMBERS_IN_SUBTITLES = 6;

	CNumberWithinMessage m_NumbersWithinMessage[MAX_NUMBERS_IN_SUBTITLES];
	CSubStringWithinMessage	m_SubstringsWithinMessage[MAX_SUBSTRINGS_IN_SUBTITLES];

	const char	*m_pMessage;
	s32		m_TextBlockContainingThisMessage;

	bool	m_bUseUnderscore;
	bool	m_bIsHelpText;
	u32		m_iVoiceNameHash;
	u32		m_iMissionNameHash;
};

enum ePreviousBriefOverride
{
	PREVIOUS_BRIEF_NO_OVERRIDE = 0,
	PREVIOUS_BRIEF_FORCE_DIALOGUE,
	PREVIOUS_BRIEF_FORCE_GOD_TEXT
};

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CQueuedMessage
// PURPOSE: message that is ready to be displayed
/////////////////////////////////////////////////////////////////////////////////////
class CQueuedMessage
{
public:
	void Set(char const *pText, s32 TextBlock, 
		u32 Duration, bool bAddToPrevBriefs, ePreviousBriefOverride OverrideOfPreviousBriefList, 
		CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
		bool bUsesUnderscore, u32 iVoiceNameHash=0, u32 iMissionNameHash=0);

	void Clear(bool ClearPersistentLiterals, bool bClearingFromPreviousBriefs);

	bool UsesTextFromThisBlock(s32 TextBlockIndex) { return m_MessageDetails.UsesTextFromThisBlock(TextBlockIndex); }
	bool IsEmpty() { return m_MessageDetails.IsEmpty(); }
	bool HasDisplayTimeExpired();

	void SetWhenStarted(u32 StartTime) { m_WhenStarted = StartTime; }

	void SetNewlyAddedToTheHeadOfTheQueue(bool b) { m_bNewlyAddedToTheHeadOfTheQueue = b; }

	void AddToPreviousBriefsIfRequired();

	void FillInString(char *pStringToFillIn, u32 MaxLengthOfString) { m_MessageDetails.FillInString(pStringToFillIn, MaxLengthOfString); }

	bool GetDisplayEvenIfSubtitlesAreSwitchedOff() { return m_bDisplayEvenIfSubtitlesAreSwitchedOff; }

	bool DoesMainTextMatch(CSubtitleMessage &OtherSubtitle) { return m_MessageDetails.DoesMainTextMatch(OtherSubtitle); }
private:
	CSubtitleMessage m_MessageDetails;

	u32		m_Duration;		// How many millisecs
	u32		m_WhenStarted;	// At what time was this guy started?

	bool					m_bAddToPreviousBriefs;
	ePreviousBriefOverride	m_OverrideBriefList;
	bool					m_bNewlyAddedToTheHeadOfTheQueue;
	bool					m_bDisplayEvenIfSubtitlesAreSwitchedOff;
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousMessages
// PURPOSE: stores previous messages
/////////////////////////////////////////////////////////////////////////////////////
class CPreviousMessages
{
public:
	void Init(s32 MaxSizeOfArray);
	void Shutdown();

	void ClearAllPreviousBriefs();
	void ClearAllPreviousBriefsBelongingToThisTextBlock(s32 TextBlock);

	void AddBrief(CSubtitleMessage &SubtitleMessage);

	s32 GetMaxNumberOfMessages() { return m_MaxNumberOfPreviousMessages; }

	bool IsPreviousMessageEmpty(s32 IndexOfString);
	bool FillStringWithPreviousMessage(s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfString);
	const char* GetPreviousMessageCharacterTXD(s32 IndexOfMsg);

private:
	CSubtitleMessage	*m_pArrayOfPreviousMessages;
	s32					m_MaxNumberOfPreviousMessages;
};


/*
class CHelpMessageText
{
public:
	char cHelpMessage[MAX_CHARS_IN_MESSAGE];
	void Clear() { cHelpMessage[0] = '\0'; }
	char *GetMessage() { return cHelpMessage; }
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPreviousHelpMessages
// PURPOSE: stores previous help messages - cut down version of CPreviousMessages
/////////////////////////////////////////////////////////////////////////////////////
class CPreviousHelpMessages
{
public:
	void Init(s32 MaxSizeOfArray);
	void Shutdown();

	void ClearAllPreviousBriefs();
	void AddBrief(char *pHelpMessageText);
	char *GetMessageText(s32 i) { return m_pArrayOfPreviousMessages[i].GetMessage(); }
	s32 GetMaxNumberOfMessages() { return m_MaxNumberOfPreviousMessages; }
	bool FillStringWithPreviousMessage(s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfString);

private:
	CHelpMessageText	*m_pArrayOfPreviousMessages;
	s32					m_MaxNumberOfPreviousMessages;
};
*/


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMessages
// PURPOSE: main "message" control class
/////////////////////////////////////////////////////////////////////////////////////
class CMessages
{
public:
	enum ePreviousMessageTypes
	{
		PREV_MESSAGE_TYPE_DIALOG = 0,
		PREV_MESSAGE_TYPE_HELP,
		PREV_MESSAGE_TYPE_MISSION
	};

	enum { MAX_NUM_BRIEF_MESSAGES = 8};

	// Text queue for the standard brief messages at the bottom of the screen.
	static class CQueuedMessage	BriefMessages[MAX_NUM_BRIEF_MESSAGES];

	static class CPreviousMessages PreviousDialogueMessages;
	static class CPreviousMessages PreviousMissionMessages;
	static class CPreviousMessages PreviousHelpTextMessages;

	static SDialogueCharacters ms_dialogueCharacters;

	static void Init();
	static void	Init(unsigned initMode);
	static void InitDialogueMeta();
#if __BANK
	static void InitWidgets();
#endif
	static void Shutdown(unsigned shutdownMode);

	static bool bTitleActive;
	static bool IsMissionTitleActive() { return bTitleActive; }
	static void SetMissionTitleActive(bool bActive) { bTitleActive = bActive; }

	static void	Update(void);

	static void	AddMessage( char const *pText, s32 TextBlock, 
		u32 Duration, bool bJumpQ, bool bAddToPrevBriefs, ePreviousBriefOverride PreviousBriefOverride, 
		CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
		bool bUsesUnderscore, u32 iVoiceNameHash=0, u32 iMissionNameHash=0);

	static void	ClearMessages();

	static void ClearAllBriefMessages();

	static void AddToPreviousBriefArray(CSubtitleMessage &SubtitleMessage, ePreviousBriefOverride PreviousBriefOverride);

	static void ClearAllPreviousBriefsThatBelongToThisTextBlock(s32 TextBlock);

	static s32 GetMaxNumberOfPreviousMessages(ePreviousMessageTypes iMessageType);

	static bool FillStringWithPreviousMessage(ePreviousMessageTypes iMessageType, s32 IndexOfString, char *pGxtStringToFill, u32 MaxLengthOfStringToFill);

	static bool IsPreviousMessageEmpty(ePreviousMessageTypes iMessageType, s32 IndexOfString);

	static void InsertNumbersAndSubStringsIntoString(const char *pOriginalString, 
		const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray, 
		char *pNewString, u32 MaxSizeOfNewString);

	static const char* GetPreviousMessageCharacterTXD(s32 IndexOfMsg);

#if __WIN32PC
	static void InsertPlayerControlKeysInString(char *pBigString);
#endif // __WIN32PC

	static void GetExpectedNumberOfTextComponents(char *pGxtString, s32 &ReturnNumberOfNumbers, s32 &ReturnNumberOfSubstrings);

	static void ClearThisPrint(CSubtitleMessage &Message, bool bCompareMainStringsOnly = false);

	static void ClearAllMessagesDisplayedByGame();	//	bIgnoreMissionTitle = FALSE);
	static void ClearAllDisplayedMessagesThatBelongToThisTextBlock(s32 TextBlock, bool bClearPreviousBriefs);	//	, bool bIgnoreMissionTitle = FALSE);

	static bool IsThisTextSwitchedOffByFrontendSubtitleSetting(char const *pText);

private:
	static bool IsThisMessageEmpty(char const *pText);
	static bool DoesTextBeginWithThisControlCharacter(char const *pText, s32 TokenToSearchFor, s32 TokenToIgnore);
	static bool ShouldThisMessageBeExcludedFromThePreviousBriefs(char const *pText);

	static bool CopyGxtStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, const char *pSourceGxtString, u32 SourceStringLength = 0);
	static bool CopyStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, const char *pSourceString, u32 SourceStringLength = 0);
	static void CopyColouredGxtStringIntoStringAtIndex(char *pDestString, u32 &DestIndex, u32 MaxDestStringLength, eHUD_COLOURS ColourOfSourceString, const char *pSourceGxtString, u32 SourceStringLength = 0);

	static eTokenFormat CheckForInsertionToken(const char *pOriginalString, const u32 CharacterIndex, u32 &IndexInteger);
	static u32 GetArrayIndexAccordingToFormat(const eTokenFormat FormatUsedForThisPosition, const u32 IndexInteger, eTokenFormat &MethodToUseForInserting, const u32 NumberOfItemsAlreadyInserted, const char *pOriginalStringForAssertMessage);
	static void InsertNumberIntoString(char *pNewString, u32 &NewCharLoop, const u32 MaxSizeOfNewString, const CNumberWithinMessage *pArrayOfNumbers, const u32 SizeOfNumberArray, const u32 ArrayElementToUse, const char *pOriginalStringForAssertMessage);
	static void InsertSubStringIntoString(char *pNewString, u32 &NewCharLoop, const u32 MaxSizeOfNewString, const CSubStringWithinMessage *pArrayOfSubStrings, const u32 SizeOfSubStringArray, const u32 ArrayElementToUse, const char *pOriginalStringForAssertMessage);

	static u32 m_PreviousSubtitleTimer;
};

enum
{
	HELP_TEXT_SLOT_STANDARD = 0,
	HELP_TEXT_SLOT_FLOATING_1,
	HELP_TEXT_SLOT_FLOATING_2,
	MAX_HELP_TEXT_SLOTS
};


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHelpMessage
// PURPOSE: help text class
/////////////////////////////////////////////////////////////////////////////////////
class CHelpMessage
{
	enum eTEXT_STATE			// the state of the text this frame
	{
		HELP_TEXT_STATE_NONE = 0,		// no text this frame (off)
		HELP_TEXT_STATE_NEW,			// new text, or different to previous frame
		HELP_TEXT_STATE_SAME,			// same text as last frame
		HELP_TEXT_STATE_FOREVER,		// just displays this forever (never fades out)
		HELP_TEXT_STATE_IDLE,			// default idle state
		HELP_TEXT_STATE_FOREVER_IDLE	// idle state for STATE_FOREVER
	};

public:
	static void Init();

	static void SetMessageText(s32 iId, const char *pHelpMessage, 
		const CNumberWithinMessage *pArrayOfNumbers = NULL, u32 SizeOfNumberArray = 0, 
		const CSubStringWithinMessage *pArrayOfSubStrings = NULL, u32 SizeOfSubStringArray = 0,
		bool bPlaySound = true, bool bDisplayForever = false, s32 OverrideDuration = -1, bool bIME = false);

	static void SetMessageTextAndAddToBrief(s32 iId, const char *pHelpMessageLabel, 
		CNumberWithinMessage *pArrayOfNumbers = NULL, u32 SizeOfNumberArray = 0, 
		CSubStringWithinMessage *pArrayOfSubStrings = NULL, u32 SizeOfSubStringArray = 0,
		bool bPlaySound = true, bool bDisplayForever = false, s32 OverrideDuration = -1);

	static void SetScreenPosition(s32 iId, Vector2 vScreenPos);
	static void SetWorldPosition(s32 iId, Vector3::Vector3Param vWorldPosition);
	static void SetWorldPositionFromEntity(s32 iId, CPhysical* pEntity, Vector2 vOffset);
	static void SetStyle(s32 iId, s32 iNewStyle, s32 iArrowDirection, s32  iFloatingTextOffset, s32 iHudColour, s32 iAlpha = -1);
	static char *GetMessageText(s32 iId);
	static bool DoesMessageTextExist(s32 iId);
	static bool IsMessageFadingOut(s32 iId);
	static void Clear(s32 iId, bool bClearNow);
	static void ClearAll();
	static void ClearMessage(s32 iId);

	static bool IsHelpTextNew(s32 iId);
	static bool IsHelpTextSame(s32 iId);
	static bool IsHelpTextForever(s32 iId);
	static bool WasHelpTextForever(s32 iId);
	
	static void SetHelpMessageAsProcessed(s32 iId);

	static eTEXT_ARROW_ORIENTATION GetDirectionHelpText(s32 iId, Vector2 vNewPosition);
	static bool ShouldPlaySound(s32 iId) { return m_bTriggerSound[iId]; }
	static void SetShouldPlaySound(s32 iId, bool bPlaysSound) { m_bTriggerSound[iId] = bPlaysSound; }
	static Vector2 GetScreenPosition(s32 iId) { return m_vScreenPosition[iId]; }
	static eTEXT_STYLE GetStyle(s32 iId) { return m_iStyle[iId]; }
	static eTEXT_ARROW_ORIENTATION GetArrowDirection(s32 iId) { return m_iArrowDirection[iId]; }
	static s32 GetFloatingTextOffset(s32 iId) { return m_iFloatingTextOffset[iId]; }
	static Color32 GetColour(s32 iId) { return m_iColour[iId]; }
	static s32 GetOverrideDuration(s32 iId) { return m_OverrideDuration[iId]; }
private:
	static char m_HelpMessageText[MAX_HELP_TEXT_SLOTS][MAX_CHARS_IN_MESSAGE];
	static bool m_bHelpMessagesFading[MAX_HELP_TEXT_SLOTS];
	static bool m_bTriggerSound[MAX_HELP_TEXT_SLOTS];
	static eTEXT_STATE m_iNewText[MAX_HELP_TEXT_SLOTS];

	static eTEXT_STYLE m_iStyle[MAX_HELP_TEXT_SLOTS];
	static eTEXT_ARROW_ORIENTATION m_iArrowDirection[MAX_HELP_TEXT_SLOTS];
	static s32 m_iFloatingTextOffset[MAX_HELP_TEXT_SLOTS];
	static Color32 m_iColour[MAX_HELP_TEXT_SLOTS];
	static Vector2 m_vScreenPosition[MAX_HELP_TEXT_SLOTS];
	static s32 m_OverrideDuration[MAX_HELP_TEXT_SLOTS];
};

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSubtitleText
// PURPOSE: subtitle text class
/////////////////////////////////////////////////////////////////////////////////////
class CSubtitleText
{
public:
	static void Init();
	static void SetMessageText(char *pHelpMessage);
	static void ClearMessage();

	static char *GetMessageText();

private:
	static char m_SubtitleMessageText[MAX_CHARS_IN_EXTENDED_MESSAGE];  // uses longer message to cater for longer strings
};


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CSavingGameMessage
// PURPOSE: saving game message
/////////////////////////////////////////////////////////////////////////////////////
class CSavingGameMessage
{
public:
	enum eICON_STYLE
	{
		SAVEGAME_ICON_STYLE_NONE = 0,
		SAVEGAME_ICON_STYLE_SAVING,
		SAVEGAME_ICON_STYLE_SUCCESS,
		SAVEGAME_ICON_STYLE_FAILED,
		SAVEGAME_ICON_STYLE_CLOUD_WORKING,
		SAVEGAME_ICON_STYLE_LOADING,
		SAVEGAME_ICON_STYLE_SAVING_NO_MESSAGE
	};

	static void Init();
	static void SetMessageText(const char *pText, eICON_STYLE iconStyle);

	static char *GetMessageText();
	static s32 GetIconStyle();

	static bool IsSavingMessageActive();

	static void Clear();

private:
	static char m_cMessageText[MAX_CHARS_IN_MESSAGE];
	static eICON_STYLE m_iIconStyle;
};


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingText
// PURPOSE: the text that appears when the game is loading on a black screen
/////////////////////////////////////////////////////////////////////////////////////
class CLoadingText
{
public:
	static void Init();
	static void Shutdown();

	static void SetText(const char *pText);
	static char *GetText();
	static bool IsActive();
	static void Clear();
	static void UpdateAndRenderLoadingText(float fCurrentTimer);

	static void SetActive(bool bSet);

private:
	static char m_cText[MAX_CHARS_IN_MESSAGE];
	static bool bActive;
};

// Graeme - I used to have -1 all over the place to indicate that there was no integer to insert in a string.
//	It would be good to use NO_NUMBER_TO_INSERT_IN_STRING instead and eventually remove it entirely.
#define NO_NUMBER_TO_INSERT_IN_STRING		(-1)

#endif

// eof
