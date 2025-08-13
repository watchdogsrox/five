/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextFormat.h
// PURPOSE : manages the formatting of the text
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef TEXTFORMAT_H
#define TEXTFORMAT_H

#include "fwscene/stores/txdstore.h"

// game includes:
#include "atl/array.h"
#include "bank/bkmgr.h"
#include "renderer/color.h"
#include "system/control.h"
#include "text/text.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/hud_colour.h"

#if __XENON || __WIN32PC
#define __XENON_BUTTONS  // 360 buttons
#elif __PPU || RSG_ORBIS
#define __PS3_BUTTONS  // ps3 buttons
#endif

#define FONT_CHAR_TOKEN_COLOUR_RED						('r')
#define FONT_CHAR_TOKEN_COLOUR_GREEN					('g')
#define FONT_CHAR_TOKEN_COLOUR_BLUE						('b')
#define FONT_CHAR_TOKEN_COLOUR_WHITE					('w')
#define FONT_CHAR_TOKEN_COLOUR_YELLOW					('y')
#define FONT_CHAR_TOKEN_COLOUR_PURPLE					('p')
#define FONT_CHAR_TOKEN_COLOUR_PINK						('q')
#define FONT_CHAR_TOKEN_COLOUR_MENU						('m')
#define FONT_CHAR_TOKEN_COLOUR_BLACK					('l')
#define FONT_CHAR_TOKEN_COLOUR_STANDARD					('s')
#define FONT_CHAR_TOKEN_COLOUR_GREY						('c')
#define FONT_CHAR_TOKEN_COLOUR_ORANGE					('o')
#define FONT_CHAR_TOKEN_COLOUR_BLUEDARK					('d')
#define FONT_CHAR_TOKEN_COLOUR_FRIENDLY					('f')

#define FONT_CHAR_TOKEN_COLOUR_FOREIGN_LANGUAGE_SUBTITLE	('t')	//	We're going to use ~t~ at the start of a line of dialogue that is not in English (not sure what happens when English is not your native language)
																	//	This subtitle will be displayed even when subtitles are turned off in the pause menu. Also the subtitle will be coloured grey.
#define FONT_CHAR_TOKEN_SCRIPT_VARIABLE					('v')
#define FONT_CHAR_TOKEN_SCRIPT_VARIABLE_2				('u')

#define FONT_CHAR_TOKEN_HIGHLIGHT						('h')
#define FONT_CHAR_TOKEN_BOLD							("bold")
#define FONT_CHAR_TOKEN_ITALIC							("italic")
#define FONT_CHAR_TOKEN_WANTED_STAR						("wanted_star")
#define FONT_CHAR_TOKEN_NON_RENDERABLE_TEXT				("nrt")
#define FONT_CHAR_TOKEN_ROCKSTAR_LOGO					("EX_R*")
#define FONT_CHAR_TOKEN_WANTED_STAR_ABBRIEVIATED		("ws")  // the text 'wanted_star' can be too long for use in text strings in script

enum {
	FONT_CHAR_TOKEN_INVALID							= NULL,

	FONT_CHAR_END									= (0),

	FONT_CHAR_TOKEN_DELIMITER						= ('~'),
	FONT_CHAR_TOKEN_UNDERLINE						= ('_'),
	
	FONT_CHAR_TOKEN_NEWLINE							= ('n'),

	FONT_CHAR_TOKEN_ESCAPE							= ('\\'),

	FONT_CHAR_TOKEN_NUMBER							= (49),		//	1
	FONT_CHAR_TOKEN_SUBSTRING						= (97),		//	a
	FONT_CHAR_TOKEN_DIALOGUE						= (122),	//	z

	FONT_CHAR_TOKEN_CONTROL_KEY						= (107),	//	k
	FONT_CHAR_TOKEN_EXCLUDE_FROM_PREVIOUS_BRIEFS	= (120),	//	x

	FONT_CHAR_TOKEN_ID_BOLD							= (200),
	FONT_CHAR_TOKEN_ID_ITALIC,
	FONT_CHAR_TOKEN_ID_NEWLINE,
	FONT_CHAR_TOKEN_ID_DIALOGUE,

	FONT_CHAR_TOKEN_ID_WANTED_STAR,
	FONT_CHAR_TOKEN_ID_ROCKSTAR_LOGO,
	FONT_CHAR_TOKEN_ID_NON_RENDERABLE_TEXT,

	// these start at 255 onwards so they dont overlap the ascii table ones above:
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_NONE = 255,
	
	FONT_CHAR_TOKEN_ID_CONTROLLER_UP,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LEFT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UP,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_DOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_RIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_NONE, // Appears to be Left Stick Click
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_ALL,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UPDOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFTRIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UP,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_DOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_RIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_NONE, // Appears to be Right Stick Click
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UPDOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFTRIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ROTATE,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UP,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_DOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_RIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_NONE,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ALL,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UPDOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFTRIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ROTATE,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_X,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_Y,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LB,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RB,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_START,
	FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_BACK,

// #if defined __PS3_BUTTONS
	FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_DRIVE,
	FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_PITCH,
	FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_RELOAD,
	FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_ROLL,
// #endif  // __PS3_BUTTONS

	FONT_CHAR_TOKEN_ID_UNUSED,

	FONT_CHAR_TOKEN_ID_CONTROLLER_UPDOWN,
	FONT_CHAR_TOKEN_ID_CONTROLLER_LEFTRIGHT,
	FONT_CHAR_TOKEN_ID_CONTROLLER_ALL,


	FONT_CHAR_TOKEN_ID_INPUT_ACCEPT,
	FONT_CHAR_TOKEN_ID_INPUT_CANCEL,

	FONT_CHAR_TOKEN_ID_COLOUR_NET_1,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_2,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_3,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_4,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_5,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_6,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_7,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_8,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_9,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_10,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_11,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_12,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_13,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_14,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_15,
	FONT_CHAR_TOKEN_ID_COLOUR_NET_16,

	// The mouse icons start at 100. NOTE: We do not add FONT_CHAR_TOKEN_ID_CONTROLLER_UP like above as when these are used they are
	// In the correct range.
	FONT_CHAR_TOKEN_MOUSE_BUTTON_LEFT = 100,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_RIGHT,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_MIDDLE,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_4,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_5,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_6,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_7,
	FONT_CHAR_TOKEN_MOUSE_BUTTON_8,
	FONT_CHAR_TOKEN_MOUSE_MOVE_LEFT,
	FONT_CHAR_TOKEN_MOUSE_MOVE_RIGHT,
	FONT_CHAR_TOKEN_MOUSE_MOVE_UP,
	FONT_CHAR_TOKEN_MOUSE_MOVE_DOWN,
	FONT_CHAR_TOKEN_MOUSE_MOVE_LEFTRIGHT,
	FONT_CHAR_TOKEN_MOUSE_MOVE_UPDOWN,
	FONT_CHAR_TOKEN_MOUSE_MOVE_ALL,
	FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_UP,
	FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_DOWN,
	FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_UPDOWN,

	FONT_MAX_CHAR_TOKEN_IDS
};


//
// extended token string ids:
//

#define FONT_CHAR_TOKEN_RADAR_BLIP				("BLIP_")  // id without the number on the end

// base the token names on X-BOX 360 pad:
#define FONT_CHAR_TOKEN_CONTROLLER_A				("PAD_A")
#define FONT_CHAR_TOKEN_CONTROLLER_B				("PAD_B")
#define FONT_CHAR_TOKEN_CONTROLLER_X				("PAD_X")
#define FONT_CHAR_TOKEN_CONTROLLER_Y				("PAD_Y")
#define FONT_CHAR_TOKEN_CONTROLLER_UP				("PAD_UP")
#define FONT_CHAR_TOKEN_CONTROLLER_DOWN			("PAD_DOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_LEFT			("PAD_LEFT")
#define FONT_CHAR_TOKEN_CONTROLLER_RIGHT			("PAD_RIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_LB				("PAD_LB")
#define FONT_CHAR_TOKEN_CONTROLLER_LT				("PAD_LT")
#define FONT_CHAR_TOKEN_CONTROLLER_RB				("PAD_RB")
#define FONT_CHAR_TOKEN_CONTROLLER_RT				("PAD_RT")
#define FONT_CHAR_TOKEN_CONTROLLER_START			("PAD_START")
#define FONT_CHAR_TOKEN_CONTROLLER_BACK			("PAD_BACK")

#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_UP			("PAD_DPAD_UP")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_DOWN			("PAD_DPAD_DOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_LEFT			("PAD_DPAD_LEFT")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_RIGHT			("PAD_DPAD_RIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_NONE			("PAD_DPAD_NONE")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_ALL			("PAD_DPAD_ALL")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_LEFTRIGHT		("PAD_DPAD_LEFTRIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_DPAD_UPDOWN		("PAD_DPAD_UPDOWN")

#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_UP			("PAD_LSTICK_UP")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_DOWN		("PAD_LSTICK_DOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_LEFT		("PAD_LSTICK_LEFT")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_RIGHT		("PAD_LSTICK_RIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_NONE		("PAD_LSTICK_NONE")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_ALL			("PAD_LSTICK_ALL")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_LEFTRIGHT	("PAD_LSTICK_LEFTRIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_UPDOWN		("PAD_LSTICK_UPDOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_LSTICK_ROTATE		("PAD_LSTICK_ROTATE")

#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_UP			("PAD_RSTICK_UP")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_DOWN		("PAD_RSTICK_DOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_LEFT		("PAD_RSTICK_LEFT")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_RIGHT		("PAD_RSTICK_RIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_NONE		("PAD_RSTICK_NONE")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_ALL			("PAD_RSTICK_ALL")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_LEFTRIGHT	("PAD_RSTICK_LEFTRIGHT")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_UPDOWN		("PAD_RSTICK_UPDOWN")
#define FONT_CHAR_TOKEN_CONTROLLER_RSTICK_ROTATE		("PAD_RSTICK_ROTATE")

#define FONT_CHAR_TOKEN_INPUT_ACCEPT					("ACCEPT")
#define FONT_CHAR_TOKEN_INPUT_CANCEL					("CANCEL")


#if defined __PS3_BUTTONS
#define FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_DRIVE		("PAD_SIXAXIS_DRIVE")
#define FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_PITCH		("PAD_SIXAXIS_PITCH")
#define FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_RELOAD		("PAD_SIXAXIS_RELOAD")
#define FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_ROLL		("PAD_SIXAXIS_ROLL")
#endif  // __PS3_BUTTONS

#define FONT_CHAR_TOKEN_COLOUR_NETWORK_1				("COL_NET_1")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_2				("COL_NET_2")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_3				("COL_NET_3")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_4				("COL_NET_4")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_5				("COL_NET_5")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_6				("COL_NET_6")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_7				("COL_NET_7")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_8				("COL_NET_8")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_9				("COL_NET_9")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_10				("COL_NET_10")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_11				("COL_NET_11")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_12				("COL_NET_12")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_13				("COL_NET_13")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_14				("COL_NET_14")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_15				("COL_NET_15")
#define FONT_CHAR_TOKEN_COLOUR_NETWORK_16				("COL_NET_16")

#define MAX_CHARS_IN_CONTROL_TOKEN	(40)
#define MAX_TOKEN_LEN (64)


#define BUTTON_KEY_FMT "b_%d"
#define KEY_FMT_SEPERATOR "%"

enum eTEXT_BUFFER_TYPE
{
	TEXT_BUFF_STANDARD = 0,
	TEXT_BUFF_HTML,
	MAX_TEXT_BUFFER_TYPES
};

enum eOverlayTextColours
{
	eOverlayTextColours_First = 0,

	eOverlayTextColours_White = eOverlayTextColours_First,
	eOverlayTextColours_Pink,
	eOverlayTextColours_RedLight,
	eOverlayTextColours_Red,
	eOverlayTextColours_RedDark,
	eOverlayTextColours_OrangeLight,
	eOverlayTextColours_Orange,
	eOverlayTextColours_OrangeDark,
	eOverlayTextColours_YellowLight,
	eOverlayTextColours_Yellow,
	eOverlayTextColours_YellowDark,
	eOverlayTextColours_GreenLight,
	eOverlayTextColours_Green,
	eOverlayTextColours_GreenDark,
	eOverlayTextColours_BlueLight,
	eOverlayTextColours_Blue,
	eOverlayTextColours_BlueDark,
	eOverlayTextColours_Purple,
	eOverlayTextColours_PurpleDark,
	eOverlayTextColours_GreyLight,
	eOverlayTextColours_Grey,
	eOverlayTextColours_GreyDark,
	eOverlayTextColours_Black,

	eOverlayTextColours_MAX
};

// PURPOSE: Params for icon extraction.
struct IconParams
{
	IconParams()
	: m_DeviceId(ioSource::IOMD_ANY)
	, m_AllowXOSwap(true)
	, m_AllowIconFallback(true)
	, m_MappingSlot(0)
	, m_CorrectButtonOrder(true)
	{}

	// PURPOSE: The device to extract an icon from.
	s32	 m_DeviceId;

	// PURPOSE: Allow swapping of the X and O buttons on Sony consoles.
	bool m_AllowXOSwap;

	// PURPOSE: Show an alternative icon if the specified icon could not be retrieved.
	bool m_AllowIconFallback;

	// PURPOSE: Reverses order of button icons.
	// NOTES:	The instructional buttons movie interprets the grouped icons from right to left. This flag is needed to show the icons
	//			in the correct order (i.e. this flag is needed whenever the icons are being shown in the instructional buttons movie).
	bool m_CorrectButtonOrder;

	// PURPOSE: The mapping slot of the device.
	u8	 m_MappingSlot;
};

class CTextFormat
{
public:
	static void Init();
	static void Shutdown();

	static void EndFrame();

	static s32 GetNumberLines (const CTextLayout& FormatDetails, float fXPos, float fYPos, const char *pStringToRender);

	//	FontStyle should be taken from the enum in text.h e.g. FONT_STYLE_STANDARD
	static bool DoesCharacterExistInFont(int FontStyle, UInt16 characterCode);
	static bool DoAllCharactersExistInFont(s32 fontStyle, const char16 *pWideString, s32 maxStringLength);
	static bool IsValidDisplayCharacter(rage::char16 c);

	static float GetCharacterHeight(const CTextLayout& FormatDetails);
	static float GetStringWidth ( const CTextLayout& FormatDetails, const char *pStringToRender, bool bIncludeSpaces = false);

	static void FilterOutTokensFromString(char *String);

	static bool IsUnsupportedSpace(char16 c);

#if SUPPORT_MULTI_MONITOR
	static bool AddToTextBuffer(float fXPos, float fYPos, const CTextLayout& FormatDetails, const char *pStringToRender, bool bEnableMultiHeadBoxWidthAdjustment = true);
#else
	static bool AddToTextBuffer(float fXPos, float fYPos, const CTextLayout& FormatDetails, const char *pStringToRender);
#endif // SUPPORT_MULTI_MONITOR
	static void FlushFontBuffer();
	static void ParseToken(const char *pStringIn, char *pStringOut, bool bFindColourTokens, const Color32 *pOriginalColour, float fButtonSize, bool *pUsingHtml, s32 iMaxStringLength, bool bStripColourTokens = false);

	static char *GetRadarBlipFromToken(char *pTokenText);
	static const char *GetRadarBlipFromToken(const char *pTokenText);

	static void GetFontName(GString& fontName, int style);
	static bool DoFontsShareMapping( int styleA, int styleB );

	static const IconParams DEFAULT_ICON_PARAMS;

	static bool GetInputButtons(const char* inputName, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams = DEFAULT_ICON_PARAMS);
	static bool GetInputButtons(      InputType input, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams = DEFAULT_ICON_PARAMS);
	static bool GetInputGroupButtons(const char* inputGroupName, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams = DEFAULT_ICON_PARAMS);
	static bool GetInputGroupButtons(     InputGroup inputGroup, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams = DEFAULT_ICON_PARAMS);

#if __BANK
	static void FontTest();
	static void CreateBankWidgets();

	static char ms_cDebugScriptedString[TEXT_KEY_SIZE];
#endif // __BANK

	struct Icon
	{
		// NOTE: Intentionally implicit (not explicit) for compatibility with console code where they will set the
		// icon to a u32 and the type as a defined icon id (ICON_ID).
		Icon(u32 iconId = 0);

		// The id of the icon
		u32  m_IconId;

		bool GetIconText(char* buffer, u32 bufferSize, const IconParams& iconParams) const;

#if KEYBOARD_MOUSE_SUPPORT
		// On PC we need to be able to display keyboard names instead of icons. This way text labels are a kind of
		// icon. As the keyboard key-codes overlap with the defined icons, we keep track of the kind of icon. 
		enum Type
		{
			ICON_ID,
			KEYCODE_ID,
			MOUSE_ID,
		};

		enum MouseIcon
		{
			MOUSE_MOV_LEFT  = rage::IOM_AXIS_X_LEFT,
			MOUSE_MOV_RIGHT = rage::IOM_AXIS_X_RIGHT,
			MOUSE_MOV_UP    = rage::IOM_AXIS_Y_UP,
			MOUSE_MOV_DOWN  = rage::IOM_AXIS_Y_DOWN,
			MOUSE_MOV_LR    = rage::IOM_AXIS_X,
			MOUSE_MOV_UD    = rage::IOM_AXIS_Y,
			MOUSE_MOV_ALL,  // default to next value.

			MOUSE_WHL_UP    = rage::IOM_WHEEL_UP,
			MOUSE_WHL_DOWN  = rage::IOM_WHEEL_DOWN,
			MOUSE_WHL_UD    = rage::IOM_AXIS_WHEEL,

			MOUSE_LEFT      = rage::MOUSE_LEFT,
			MOUSE_RIGHT     = rage::MOUSE_RIGHT,
			MOUSE_MIDDLE    = rage::MOUSE_MIDDLE,
			MOUSE_EXTRABTN1 = rage::MOUSE_EXTRABTN1,
			MOUSE_EXTRABTN2 = rage::MOUSE_EXTRABTN2,
			MOUSE_EXTRABTN3 = rage::MOUSE_EXTRABTN3,
			MOUSE_EXTRABTN4 = rage::MOUSE_EXTRABTN4,
			MOUSE_EXTRABTN5 = rage::MOUSE_EXTRABTN5,
		};
		Type m_Type;

		Icon(u32 iconId, Type type);
#endif // KEYBOARD_MOUSE_SUPPORT

		const Icon& operator=(const Icon& icon);
		bool operator == (const Icon& icon) const;
	};

	// Used to return all icons that make up an input.
	typedef atFixedArray<Icon, 4> IconList;

	// Used internally to store group input sources.
	typedef atFixedArray<ioSource, ControlInput::INPUT_GROUP_LIMIT> GroupSourceList;


	static void GetIconListFormatString(const IconList& icons, char* buffer, u32 bufferSize, const IconParams& iconParams);
	static void GetControllerIconsFromToken(char *pTokenText, IconList &Icons, const char* pFullString);
	static void GetTokenIDFromPadButton(ioMapperSource PadSource, unsigned PadButton, IconList &Icons);
	static bool GetDeviceIconsFromTwoAxis(const ioSource& UpDown, const ioSource& LeftRight, IconList& Icons);
	static void GetDeviceIconsFromInputGroup(const GroupSourceList& groupSources, IconList& Icons);

	//! Overlay text Functionality
	static eHUD_COLOURS ConvertToHudColour( eOverlayTextColours const overlayColour );
	static CRGBA ConvertToRGBA( eOverlayTextColours const overlayColour, float const * const alphaOverride = NULL );
	static eOverlayTextColours ConvertToOverlayTextColour( eHUD_COLOURS const c_hudColor );
	static s32 GetGameDisplayNameIndexOfOverlayFont( s32 const c_currentFont );
	static s32 FilterOverlayFonts( s32 currentFont, bool const c_incrementalFilter, const char *pStringToDisplay );

	static bool IsUnsignedInteger(const char *pString, u32 &ReturnInteger);

private:

	static bank_float fTextIconScaler;
	static bank_float fTextIconOffset;

	static Color32 GetColourFromToken(char *pTokenText, bool *bFoundColour, const Color32 *pOriginalColour);
	static s32 GetSpecificToken(char *pTokenText);


	static void ConvertToGenericAxisIcons( IconList &Icons );

	static bool IsPossbleGenericAxis( GroupSourceList &buttons, GroupSourceList &axis );

	static void CorrectButtonOrder(IconList& icons, const IconParams& iconParams);

	
	// These are used to determine what dpad icons to show.
	enum DpadIconDirection
	{
		DPAD_NONE  = 0x0,
		DPAD_UP    = 0x1,
		DPAD_LEFT  = 0x2,
		DPAD_RIGHT = 0x4,
		DPAD_DOWN  = 0x8,

		DPAD_ALL   = 0xF,
	};
	static DpadIconDirection GetDpadIconDirection(const ioSource& source);

	static void SetupFontParams(GFxDrawTextManager::TextParams& outParams, const CTextLayout *pFormatDetails, float fPosition, Vector2 *LeftRight);

	static void SafeAddIconToList(IconList& Icons, const Icon &icon);
	static void FillIconListFromTokenizedString(char* buffer, int* iIDs, int iIdSize, CTextFormat::IconList &icons);

	static void AddTextDrawCommand(const CTextLayout& FormatDetails, const GFxDrawTextManager::TextParams& params, const char* string, const Vector2& position, float width, bool isHtml);
};

inline void CTextFormat::SafeAddIconToList(IconList& icons, const Icon &icon)
{
	if(Verifyf(icons.size() < icons.GetMaxCount(), "Too many icons added to icon list, maximum set at %d!", icons.GetMaxCount()))
	{
		icons.Push(icon);
	}
}

inline bool CTextFormat::IsPossbleGenericAxis( GroupSourceList &buttons, GroupSourceList &axis )
{
	// A single generic axis is made up of two dpad buttons (on the same axis) and the matching left stick axis!
	// A double generic axis is made up of all (four) dpad buttons and both left stic axes.
	return (buttons.size() == 2 && axis.size() == 1) || (buttons.size() == 4 && axis.size() == 2);
}

inline CTextFormat::Icon::Icon(u32 iconId)
: m_IconId(iconId)
#if __WIN32PC
, m_Type(ICON_ID)
#endif // __WIN32PC
{}

#if __WIN32PC
inline CTextFormat::Icon::Icon(u32 iconId, Type type)
: m_IconId(iconId)
, m_Type(type)
{}
#endif // __WIN32PC

inline const CTextFormat::Icon& CTextFormat::Icon::operator=(const Icon& icon)
{
#if __WIN32PC
	m_Type =   icon.m_Type;
#endif // __WIN32PC
	
	m_IconId = icon.m_IconId;
	return *this;
}

inline bool CTextFormat::Icon::operator ==(const Icon& icon) const
{
	return m_IconId == icon.m_IconId WIN32PC_ONLY(&& m_Type == icon.m_Type);
}


#endif // TEXTFORMAT_H

// eof
