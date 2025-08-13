/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextFormat.cpp
// PURPOSE : manages the formatting of the text
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////


// rage
#include "diag/output.h"		// for DIAG_CONTEXT_MESSAGE
#include "grcore/allocscope.h"
#include "grcore/effect.h"
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "string/string.h"
#include "string/unicode.h"

#include "Text/TextFormat.h"

#include "Text/TextConversion.h"

#include "renderer/sprite2d.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/RenderTargets.h"
#include "replaycoordinator/Storage/MontageText.h"
#include "system/controlmgr.h"  // mapper stuff
#include "system/param.h"
#include "shaders/ShaderLib.h"

#include "camera/viewports/ViewportManager.h"

#include "fwdrawlist/drawlistmgr.h"
#include "fwsys/fileExts.h"
#include "fwsys/timer.h"
#include "streaming/streaming.h"

#include "frontend/Scaleform/ScaleFormMgr.h"
#include "bank/bank.h"
#include "profile/timebars.h"

TEXT_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

PF_PAGE(GTA_Text, "GTA Text Drawing");
PF_GROUP(GTA_TEXT);
PF_LINK(GTA_Text, GTA_TEXT);
PF_TIMER(Text_Flush, GTA_TEXT);
PF_TIMER(Text_AddToBuffer, GTA_TEXT);
PF_TIMER(Text_EndFrame, GTA_TEXT);
PF_TIMER(Text_ParseToken, GTA_TEXT);
PF_TIMER(Text_SetText, GTA_TEXT);
PF_TIMER(Text_Measure, GTA_TEXT);

#if KEYBOARD_MOUSE_SUPPORT
#define KEYBOARD_KEY_FMT			"t_"
#define KEYBOARD_ICON_FMT			"b_"

#define UNMAPPED_ICON_TEXT			KEYBOARD_ICON_FMT "995"

// NOTE: This is not hook up in code yet but is hooked up in scaleform. The button id is 996 and the small icon is 997.
#define KEYBOARD_LARGE_KEY_FMT		"T_"
#endif // KEYBOARD_MOUSE_SUPPORT

#if	__DEV

//#define __DISPLAY_DEBUG_OUTPUT

PARAM(nobuttons, "[code] disable buttons in text");

#endif

#if __BANK
char CTextFormat::ms_cDebugScriptedString[TEXT_KEY_SIZE];

#define TEXT_BUFFER_SIZE		(32)  // number of text draw commands we can queue at once
#define GFXTEXT_CACHE_SIZE		(96)  // number of GfxDrawText instances to keep around
#else
#define TEXT_BUFFER_SIZE		(32)  // number of text draw commands we can queue in any frame
#define GFXTEXT_CACHE_SIZE		(32)  // number of GfxDrawText instances to keep around - should be > the number of unique strings on screen
#endif
CompileTimeAssert(TEXT_BUFFER_SIZE <= GFXTEXT_CACHE_SIZE);

#define NON_WIDESCREEN_SCALER	(4.0f/3.0f)  // for 4:3 scaling

namespace CachedTextDrawing
{

	struct sDrawCommand
	{
		Vector2 vPosition;
		GColor	bgcolor;
		GRectF	scissor;
		float	fShadow;
		s16		iTextCacheIndex;
		u8		dropalpha;
		bool	background, backgroundOutline, outline, outlineCut, adjustForNonWidescreen;
		u8		orientation;
#if RSG_PC
		int	StereoFlag;
#endif
	};

	struct sCache
	{
		struct Key
		{
			u32                     hash;
			struct Blob
			{
				float                   width;
				int                     drawList;
				u32                     stringHash;
				u32                     fontNameHash;

				// see GFxDrawTextManager::TextParams
				GColor                  TextColor;
				GFxDrawText::Alignment  HAlignment;
				GFxDrawText::VAlignment VAlignment;
				GFxDrawText::FontStyle  FontStyle;
				Float                   FontSize;
				//GString                 FontName;
				bool                    Underline;
				bool                    Multiline;
				bool                    WordWrap;
				SInt					Leading;
				SInt					Indent;
				UInt					LeftMargin;
				UInt					RightMargin;
			};
			Blob                    blob;

			Key() : hash(0) {}
			Key(const GFxDrawTextManager::TextParams& params, const char* string, float width, int drawList);

			bool operator==(const Key &rhs) const
			{
				return hash==rhs.hash && memcmp(&blob, &rhs.blob, sizeof(blob))==0;
			}

			bool operator!=(const Key &rhs) const
			{
				return !operator==(rhs);
			}
		};

		struct Entry
		{
			Key					    key;
			u32					    iLastUsed; // last frame this item was used
			GPtr<GFxDrawText>	    pText;
			float				    fWidth;
			u16					    iRefCount;
#if __ASSERT
			u16                     iRenderThreadIndex;
#endif

			Entry() 
				: iLastUsed(0)
				, fWidth(0.0f)
				, iRefCount(0)
#if __ASSERT
				, iRenderThreadIndex(0xffff)
#endif
			{
			}
		};

		enum
		{
			INVALID_ENTRY = -1,
		};

		int GetEntry(const Key& key, bool *isNewKey);
		void ReleaseEntry(int index);

		atRangeArray<Entry, GFXTEXT_CACHE_SIZE*NUMBER_OF_RENDER_THREADS> Cache;
		u32 Pending[NUMBER_OF_RENDER_THREADS];
		sysCriticalSectionToken Mutex;
	};

	atFixedArray<sDrawCommand, TEXT_BUFFER_SIZE> DrawCommands[NUMBER_OF_RENDER_THREADS];
	sCache GfxDrawTextCache;
}

/*
static char ButtonFilenames[FONT_CHAR_TOKEN_ID_INPUT_ACCEPT-FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_NONE][40] =
{
	"",
	"rstick_up",		// FONT_CHAR_TOKEN_ID_CONTROLLER_UP,
	"rstick_down",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DOWN,
	"rstick_left",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LEFT,
	"rstick_right",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RIGHT,

	"dpad_up",			// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UP,
	"dpad_down",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_DOWN,
	"dpad_left",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFT,
	"dpad_right",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_RIGHT,
	"dpad_none",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_NONE,
	"dpad_all",			// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_ALL,
	"dpad_updown",		// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UPDOWN,
	"dpad_leftright",	// FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFTRIGHT,

	"lstick_up",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UP,
	"lstick_down",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_DOWN,
	"lstick_left",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFT,
	"lstick_right",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_RIGHT,
	"lstick_none",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_NONE,
	"lstick_all",		// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL,
	"lstick_updown",	// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UPDOWN,
	"lstick_leftright",	// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFTRIGHT,
	"lstick_rotate",	// FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ROTATE,

	"rstick_up",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UP,
	"rstick_down",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_DOWN,
	"rstick_left",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFT,
	"rstick_right",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_RIGHT,
	"rstick_none",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_NONE,
	"rstick_all",		// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ALL,
	"rstick_updown",	// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UPDOWN,
	"rstick_leftright",	// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFTRIGHT,
	"rstick_rotate",	// FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ROTATE,

	"p_cross",			// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A,
	"p_circle",			// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B,
	"p_square",			// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_X,
	"p_triangle",		// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_Y,
	"p_l1",				// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LB,
	"p_l2",				// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LT,
	"p_r1",				// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RB,
	"p_r2",				// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RT,
	"start_butt",		// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_START,
	"p_select",			// FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_BACK,

#if defined __PS3_BUTTONS
	"p_sixaxis_drive",	// FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_DRIVE,
	"p_sixaxis_pitch",	// FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_PITCH,
	"p_sixaxis_reload",	// FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_RELOAD,
	"p_sixaxis_roll"	// FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_ROLL,
#endif
};
*/

#if __BANK
static bool bOutputAllTextRenderToLog = false;
#endif  // __BANK

bank_float CTextFormat::fTextIconScaler = 1.277f;   // original value: 1.0
bank_float CTextFormat::fTextIconOffset = -0.32f;  // original value: -0.40

const IconParams CTextFormat::DEFAULT_ICON_PARAMS;


CachedTextDrawing::sCache::Key::Key(const GFxDrawTextManager::TextParams& params, const char* string, float width, int drawList)
{
	// Hash up all the parameters that could cause text to get re-formatted

	blob.width        = width;
	blob.drawList     = drawList;
	blob.stringHash   = atLiteralStringHash(string);
	blob.fontNameHash = atStringHash(params.FontName.ToCStr());

	blob.TextColor    = params.TextColor;
	blob.HAlignment   = params.HAlignment;
	blob.VAlignment   = params.VAlignment;
	blob.FontStyle    = params.FontStyle;
	blob.FontSize     = params.FontSize;
	blob.Underline    = params.Underline;
	blob.Multiline    = params.Multiline;
	blob.WordWrap     = params.WordWrap;
	blob.Leading      = params.Leading;
	blob.Indent       = params.Indent;
	blob.LeftMargin   = params.LeftMargin;
	blob.RightMargin  = params.RightMargin;

	hash = atDataHash((const char*)&blob, sizeof(blob));
}

int CachedTextDrawing::sCache::GetEntry(const Key& key, bool *isNewKey)
{
	SYS_CS_SYNC(Mutex);

	Entry* elements = Cache.GetElements();
	int numElements = Cache.GetMaxCount();
	int oldest = INVALID_ENTRY;
	u32 oldestFrame = fwTimer::GetFrameCount();
	*isNewKey = true;
	for(int i = 0; i < numElements; i++)
	{
		Entry& entry = elements[i];

		// Found entry with matching hash?
		if (entry.key == key)
		{
			oldest = i;
			*isNewKey = false;
			break;
		}

		// Available LRU entry?
		if (entry.iRefCount == 0 && (s32)(oldestFrame - entry.iLastUsed) >= 0)
		{
			oldestFrame = entry.iLastUsed;
			oldest = i;
		}
	}

	if (oldest != INVALID_ENTRY)
	{
		const unsigned rti = g_RenderThreadIndex;
		Entry *e = elements+oldest;

		// If we are recycling a cache entry, then ensure we aren't going to
		// have too many pending entries from this render thread.  If we have
		// too many, return INVALID_ENTRY to get caller to do a flush.
		if (!(e->iRefCount)++)
		{
			if (Pending[rti] >= GFXTEXT_CACHE_SIZE)
			{
				Assert(Pending[rti] == GFXTEXT_CACHE_SIZE);
				return INVALID_ENTRY;
			}
			++(Pending[rti]);
			if (*isNewKey)
			{
				e->key = key;
			}
#if __ASSERT
			Assert(e->iRenderThreadIndex == 0xffff);
			e->iRenderThreadIndex = (u16)rti;
#endif
		}
#if __ASSERT
		else
		{
			Assert(!*isNewKey);
			Assert(e->iRenderThreadIndex == rti);
		}
#endif

		return oldest;
	}

	return INVALID_ENTRY;
}


void CachedTextDrawing::sCache::ReleaseEntry(int index)
{
	Assert(index != INVALID_ENTRY);
	SYS_CS_SYNC(Mutex);
	sCache::Entry& e = Cache[index];
	const unsigned rti = g_RenderThreadIndex;
	Assert(e.iRefCount);
	Assert(e.iRenderThreadIndex == rti);
	e.iLastUsed = fwTimer::GetFrameCount();
	if (!--(e.iRefCount))
	{
#if __ASSERT
		e.iRenderThreadIndex = 0xffff;
#endif
		Assert(Pending[rti]);
		--(Pending[rti]);
	}
}


void CTextFormat::Init()
{
//	safecpy(ms_cDebugScriptedString, "WTD_BA_1", NELEM(ms_cDebugScriptedString));
}


void CTextFormat::Shutdown()
{  
	using namespace CachedTextDrawing;
	for(int i = 0; i < GfxDrawTextCache.Cache.GetMaxCount(); i++)
	{
		GfxDrawTextCache.Cache[i].pText = NULL;
	}
}


__forceinline GColor ToGColor(Color32 c)
{
	return GColor(c.GetRed(), c.GetGreen(), c.GetBlue(), c.GetAlpha());
}

void CTextFormat::AddTextDrawCommand(const CTextLayout& FormatDetails, const GFxDrawTextManager::TextParams& params, const char* string, const Vector2& position, float width, bool bHtmlUsed)
{
	using namespace CachedTextDrawing;

	// Pseudocode
	// Get a new draw command
	//		Do we have too many? If so, flush them
	// Check the cache for a GFxDrawText object that contains the right text:
	//		* Compute a hash based on the text layout and the string data
	//		* check the cache for an existing entry with that hash
	//		* If not found, grab the oldest _unreferenced_ cache item
	//		* No oldest unreferenced cache item?
	//			* flush and try again
	//

	const unsigned rti = g_RenderThreadIndex;

	if (DrawCommands[rti].IsFull())
	{
		FlushFontBuffer();
	}

	Assertf(pCurrExecutingList || g_RenderThreadIndex == 0, "If there is no currently executing drawlist then we should not be executing on a subrender thread");
	int drawList = pCurrExecutingList ? pCurrExecutingList->m_type : -1;
	sCache::Key key(params, string, width, drawList);
	bool isNewKey;
	int cacheIndex = GfxDrawTextCache.GetEntry(key, &isNewKey);
	if (cacheIndex == sCache::INVALID_ENTRY)
	{
		FlushFontBuffer();
		cacheIndex = GfxDrawTextCache.GetEntry(key, &isNewKey);

		// Fatal assert, as this will corrupt memory
		FatalAssertf(cacheIndex != sCache::INVALID_ENTRY, "Flushed the font buffer, but all cache entries were still referenced");
	}

	// Cache entry hash will mismatch if there was a cache miss.
	sCache::Entry& entry = GfxDrawTextCache.Cache[cacheIndex];
	if (isNewKey)
	{
		// Now fill out the new cache entry

		GRectF textBox(0.0f, 0.0f, width, 10000.0f); // start with a very large text box height to just let text flow down the screen

		bool needsTrueHeightExtent = FormatDetails.Background || FormatDetails.BackgroundOutline;
		GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();

		if (needsTrueHeightExtent)
		{
			GSizeF trueSize;
			if (bHtmlUsed)
			{
				trueSize = drawText->GetHtmlTextExtent(string, width, &params);
			}
			else
			{
				trueSize = drawText->GetTextExtent(string, width, &params);
			}

			trueSize.Expand(0.001f, 0.0f);

			textBox = GRectF(0.0f, 0.0f, trueSize);
		}

		if (bHtmlUsed)
		{
			entry.pText = *drawText->CreateHtmlText(string, textBox, &params);
		}
		else
		{
			entry.pText = *drawText->CreateText(string, textBox, &params);
		}

		entry.fWidth = width;
		entry.pText->SetAAMode(GFxDrawText::AA_Animation);
	}

	entry.iLastUsed = fwTimer::GetFrameCount();

	sDrawCommand& cmd = DrawCommands[rti].Append();
	cmd.iTextCacheIndex = (s16)cacheIndex;
	cmd.bgcolor = ToGColor(FormatDetails.BGColor);
	cmd.dropalpha = (u8)(FormatDetails.Color.GetAlpha()*0.75f);
	cmd.background = FormatDetails.Background;
	cmd.backgroundOutline = FormatDetails.BackgroundOutline;
	cmd.adjustForNonWidescreen = FormatDetails.bAdjustForNonWidescreen;
	cmd.orientation = FormatDetails.Orientation;
#if RSG_PC
	cmd.StereoFlag = FormatDetails.StereoFlag;
#endif

	if (FormatDetails.m_scissor.right != 0.0f && FormatDetails.m_scissor.bottom != 0.0f)  // we have some width and height
	{
		cmd.scissor.SetRect(FormatDetails.m_scissor.left,
							FormatDetails.m_scissor.top,
							FormatDetails.m_scissor.right,
							FormatDetails.m_scissor.bottom);
	}
	else
	{
		cmd.scissor.Clear();
	}

	if (FormatDetails.bDrop)
	{
//#define DROP_SHADOW_SCALER (0.009f)  // removed scaler as per bug 966734 from StuP
		cmd.outline = false;
		cmd.fShadow = 1.0f;//(DROP_SHADOW_SCALER * FormatDetails.Scale.y);
	}
	else if (FormatDetails.bOutline)
	{
		cmd.fShadow = 0.0f;
		cmd.outline = true;
		cmd.outlineCut = FormatDetails.bOutlineCutout;
	}
	else
	{
		cmd.fShadow = 0.0f;
		cmd.outline = false;
	}

	cmd.vPosition = position;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::AddToTextBuffer
// PURPOSE: adds text to the font buffer
/////////////////////////////////////////////////////////////////////////////////////
#if SUPPORT_MULTI_MONITOR
bool CTextFormat::AddToTextBuffer(float fXPos, float fYPos, const CTextLayout& FormatDetails, const char *pStringToRender, bool bEnableMultiHeadBoxWidthAdjustment)
#else
bool CTextFormat::AddToTextBuffer(float fXPos, float fYPos, const CTextLayout& FormatDetails, const char *pStringToRender)
#endif // SUPPORT_MULTI_MONITOR
{
	PF_FUNC(Text_AddToBuffer);

	if (!CText::AreScaleformFontsInitialised())
	{
		return false;
	}

	char cTextString[MAX_CHARS_FOR_TEXT_STRING];

	const grcViewport *pCurrentViewport = grcViewport::GetCurrent();

	AssertMsg(pCurrentViewport, "CTextFormat - Invalid viewport");

	if (!pCurrentViewport)
	{
		return false;  // if no viewport return false
	}

	CTextLayout LocalFormatDetails = FormatDetails;

	Vector2 vViewportSize = Vector2((float)pCurrentViewport->GetWidth(), (float)pCurrentViewport->GetHeight());

	if (FormatDetails.bAdjustForNonWidescreen)  // adjust position, here - scale will be adjusted later in the render
	{
		fXPos /= NON_WIDESCREEN_SCALER;
		LocalFormatDetails.WrapStart /= NON_WIDESCREEN_SCALER;  // fix for the wrap issue mentioned in 
		LocalFormatDetails.WrapEnd /= NON_WIDESCREEN_SCALER;
	}

	//
	// setup the text params based on our current font layout:
	//
	Vector2 vFinalPos(0,0);
	GFxDrawTextManager::TextParams params;
	SetupFontParams(params, &LocalFormatDetails, fXPos, &vFinalPos);

	if (CTextConversion::GetByteCount(pStringToRender) >= MAX_CHARS_FOR_TEXT_STRING)
	{
		Assertf(0, "CTextFormat: Html string starting with '%s'... exceeds max character count of %d", pStringToRender, MAX_CHARS_FOR_TEXT_STRING);
		return false;
	}
	
	//
	// Get final formatted string:
	//
	bool bHtmlUsed = false;
	ParseToken(pStringToRender, cTextString, LocalFormatDetails.TextColours, &LocalFormatDetails.Color, params.FontSize, &bHtmlUsed, NELEM(cTextString), false);  // go through, find and replace colour tokens with HTML tags

	if (cTextString[0] == 0)
	{
/*#if __ASSERT
		char cTempDebugString[MAX_CHARS_FOR_TEXT_STRING];
		sfDisplayf("Invalid String After Parse: %s", CTextConversion::charToAscii(pStringToRender, cTempDebugString, MAX_CHARS_FOR_TEXT_STRING));
		AssertMsg(0, "CTextFormat: Text string contained nothing after parse - see log");
#endif // __ASSERT*/
		return false;
	}



	//
	// get the final width and height used for this segment of text (only do this if we need to use the data)
	//
	GSizeF WidthAndHeight(10000.0f, 10000.0f);

	if (FormatDetails.RenderUpwards)
	{
		PF_START(Text_Measure);
		GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();
		if (bHtmlUsed)
			WidthAndHeight = drawText->GetHtmlTextExtent(cTextString, (vFinalPos.y-vFinalPos.x)*vViewportSize.x, &params);
		else
			WidthAndHeight = drawText->GetTextExtent(cTextString, (vFinalPos.y-vFinalPos.x)*vViewportSize.x, &params);
		PF_STOP(Text_Measure);

		fYPos -= (WidthAndHeight.Height / vViewportSize.y);  // adjust so that we render at the bottom of the "window"
	}

	Vector2 position(vFinalPos.x * vViewportSize.x, fYPos * vViewportSize.y);
	float fBoxWidth = (vFinalPos.y - vFinalPos.x) * vViewportSize.x;
#if SUPPORT_MULTI_MONITOR
		Vector2 vOriginalPosition(vFinalPos.x, vFinalPos.y);
		float fWidth = 1.0f;

		if (GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
			const fwRect& area = mon.getArea();

			vOriginalPosition -= Vector2(area.left, area.bottom);
			vOriginalPosition.Multiply(Vector2(1/area.GetWidth(), 1/area.GetHeight()));
			fWidth = (float)mon.getWidth();

			if(bEnableMultiHeadBoxWidthAdjustment && (params.HAlignment != GFxDrawText::Align_Center))
				fBoxWidth = (vOriginalPosition.y - vOriginalPosition.x) * fWidth;
		}
#endif //SUPPORT_MULTI_MONITOR
	AddTextDrawCommand(FormatDetails, params, cTextString, position, fBoxWidth, bHtmlUsed);

	return true;  // success
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::ParseToken
// PURPOSE: parses any tokens that are in the text and sets up the correct html 
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::ParseToken(const char *pStringIn, char *pStringOut, bool bFindColourTokens, const Color32 *pOriginalColour, float fButtonSize, bool *pUsingHtml, s32 iMaxStringLength, bool bStripColourTokens)
{
	PF_FUNC(Text_ParseToken);
#define MAX_CHARS_FOR_MINOR_HTML_CODE		(10)
#define MAX_CHARS_FOR_DETAILED_HTML_CODE	(300)
#define MAX_CHARS_FOR_KEY_NAME              (16)

	if (!pStringOut)  // ensure we have a valid output string
	{
		Assertf(0, "CTextFormat - Invalid output string passed");
		return;
	}

	s32 iScaledButtonSize = (s32)rage::Floorf(fButtonSize * fTextIconScaler);
	const char *pNewCharacters = &pStringIn[0];

	s32 iNewTextCount = 0;

	*pUsingHtml = false;

	pStringOut[0] = 0;

enum eTOKEN_STATE
{
	TOKEN_STATE_STANDARD_AREA,  // standard text (not found a token yet)
	TOKEN_STATE_INSIDE_TOKEN_AREA,  // inside a token, gathering all the token text
	TOKEN_STATE_COMPLETED,  // fully completed, ready to go back to standard text again
};

	char cTokenName[MAX_TOKEN_LEN];
	s32 iTokenNameCharCount = 0;

	bool bInsideBold = false;
	bool bInsideItalic = false;

	eTOKEN_STATE iTokenState = TOKEN_STATE_STANDARD_AREA;

	const char* prevChar = NULL;
	// loop through the text...
	while (*pNewCharacters != '\0' && *pNewCharacters != 0 && iNewTextCount < iMaxStringLength)
	{
		// CHECK TOKEN ESCAPING
		//   If the current character is an escape character, and there is room to look ahead...
		if (*pNewCharacters == FONT_CHAR_TOKEN_ESCAPE && (iNewTextCount+1) < iMaxStringLength)
		{
			// Get the next character. If valid (not null), check if it is the delimiter.
			// If it is the delimiter, skip this escape character. The code below
			// will handle treating the delimiter as a normal character.
			const char* peek = (pNewCharacters+1);
			if (peek && *peek == FONT_CHAR_TOKEN_DELIMITER)
			{
				prevChar = pNewCharacters;
				pNewCharacters++;
				continue;
			}
		}

		// FOUND A TOKEN:
		if (*pNewCharacters == FONT_CHAR_TOKEN_DELIMITER &&
			(prevChar == NULL || *prevChar != FONT_CHAR_TOKEN_ESCAPE)) // Check Escaping of Token
		{
			// START TOKEN - outside of token area, so enter it here and continue...
			if (iTokenState == TOKEN_STATE_STANDARD_AREA)
			{
				iTokenNameCharCount = 0;
				iTokenState = TOKEN_STATE_INSIDE_TOKEN_AREA;
				prevChar = pNewCharacters;
				pNewCharacters++;
				continue;
			}
			else
			// END TOKEN - inside token area, so we have at this point parsed the token so we process it here... 
			if (iTokenState == TOKEN_STATE_INSIDE_TOKEN_AREA)
			{
				cTokenName[iTokenNameCharCount] = '\0';
				iTokenNameCharCount = 0;  // reset for next time

				bool bInsertColour = false;
				Color32 newColour;

				if (bFindColourTokens)
				{
					newColour = GetColourFromToken(cTokenName, &bInsertColour, pOriginalColour);

					if (bStripColourTokens)  // stip the colour token but dont apply the hmtl
					{
						bInsertColour = false;
					}

				}

				IconList Icons;
				GetControllerIconsFromToken(cTokenName, Icons, pStringIn);

				char *pBlipToken = GetRadarBlipFromToken(cTokenName);
				s32 iInsertSpecific = GetSpecificToken(cTokenName);

				if (iInsertSpecific != FONT_CHAR_TOKEN_INVALID)
				{
					switch (iInsertSpecific)
					{
						case FONT_CHAR_TOKEN_ID_BOLD:
						{
							const char* htmlCode = bInsideBold ? "</b>" : "<b>";
							bInsideBold = !bInsideBold;

							strcat( pStringOut, htmlCode );
							iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

							*pUsingHtml = true;
							break;
						}
						case FONT_CHAR_TOKEN_ID_ITALIC:
							{
								const char* htmlCode = bInsideItalic ? "</i>" : "<i>";
								bInsideItalic = !bInsideItalic;

								strcat( pStringOut, htmlCode );
								iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

								*pUsingHtml = true;
								break;
						}
						case FONT_CHAR_TOKEN_ID_WANTED_STAR:
						{
							//
							// wanted star icon, so convert this to html tag and insert it into the text
							//
							char htmlCode[MAX_CHARS_FOR_DETAILED_HTML_CODE];
							sprintf(htmlCode, "<img src='wanted_star' width='%d' height='%d'/>", iScaledButtonSize, iScaledButtonSize);

							strcat( pStringOut, htmlCode );
							iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

							*pUsingHtml = true;
							break;
						}
						case FONT_CHAR_TOKEN_ID_ROCKSTAR_LOGO:
						{
							//
							// rockstar logo icon, Using sigma which has been replaced on the character sheet.
							//
							const char* cWantedHtmlCode = "\xE2\x88\x91";

							strcat( pStringOut, cWantedHtmlCode );
							iNewTextCount += fwTextUtil::GetByteCount(cWantedHtmlCode);
							break;
						}
						case FONT_CHAR_TOKEN_ID_NEWLINE:
							{
								const char* htmlCode = "<br/>";

								strcat( pStringOut, htmlCode );
								iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

								*pUsingHtml = true;
								break;
						}
						case FONT_CHAR_TOKEN_ID_NON_RENDERABLE_TEXT:
						{
							break;
						}
						case FONT_CHAR_TOKEN_ID_DIALOGUE:
						{
							break;
						}
						default:
						{
							Assertf(0, "CTextFormat - Invalid specific code used: ~%c~", iInsertSpecific);
							break;
						}
					}
				}
				else
				if (bInsertColour)
				{
					//
					// found the colour token, so convert this to html tag and insert it into the text
					//
					char htmlCode[MAX_CHARS_FOR_DETAILED_HTML_CODE];
					sprintf(htmlCode, "<FONT COLOR='#%02X%02X%02X'>", newColour.GetRed(), newColour.GetGreen(), newColour.GetBlue());

					strcat( pStringOut, htmlCode );
					iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

					*pUsingHtml = true;
				}
				else
				{
					if (pBlipToken)
					{
						//
						// found a radar blip, so convert this to html tag and insert it into the text
						//
						char htmlCode[MAX_CHARS_FOR_DETAILED_HTML_CODE];

						for (s32 i = 0; i < (s32)strlen(pBlipToken); i++)
							pBlipToken[i] = (char)tolower(pBlipToken[i]);

						sprintf(htmlCode, "<img src='img_radar_%s' vspace='%d' width='%d' height='%d'/>", pBlipToken, (s32)rage::Floorf(iScaledButtonSize * fTextIconOffset), iScaledButtonSize, iScaledButtonSize);

						strcat( pStringOut, htmlCode );
						iNewTextCount += fwTextUtil::GetByteCount(htmlCode);

						*pUsingHtml = true;
					}
					else if(Icons.size() > 0)
					{
						char htmlCode[MAX_CHARS_FOR_DETAILED_HTML_CODE] = "";

						IconParams params	 = DEFAULT_ICON_PARAMS;
						params.m_AllowXOSwap = false;

						GetIconListFormatString(Icons, htmlCode, MAX_CHARS_FOR_DETAILED_HTML_CODE, params);
						strcat( pStringOut, htmlCode );
						iNewTextCount += fwTextUtil::GetByteCount(htmlCode);
					}
					else
					{
						// Check for "~~", allowing the escape character to be escaped
						if (*cTokenName == '\0')
						{
							char tildeCode[] = "~";

							strcat( pStringOut, tildeCode );
							iNewTextCount += fwTextUtil::GetByteCount(tildeCode);
						}
						else
						{
#if __ASSERT
							// final check incase we didnt need the colour
							GetColourFromToken(cTokenName, &bInsertColour, pOriginalColour);
							// only assert if we didnt find a colour token (this step may of been skipped if not required by the render, so need to check here aswell)
							Assertf(bInsertColour, "CTextFormat - Invalid text token used: ~%s~. If all icons show up correctly the next frame ignore this assert.", cTokenName);
#endif
						}
					}
				}

				// move onto the next character and drop out of the token areas:
				do
				{
					iTokenNameCharCount = 0;
					iTokenState = TOKEN_STATE_STANDARD_AREA;
					prevChar = pNewCharacters;
					pNewCharacters++;

				// if we had a newline this time, we need to skip past any space characters that come directly after it.
				// this allows line breaks in the next file to exist after newlines and also we want to avoid having invalid
				// spaces at the start of lines - this simply loops until the character isnt a space character anymore IF we
				// processed a newline this time.   Fixes bugs like 116104
				} while (iInsertSpecific == FONT_CHAR_TOKEN_ID_NEWLINE && *pNewCharacters == 32);
				
			}
		}
		else
		// No token found, so get the data
		// either from inside the token area:
		if (iTokenState == TOKEN_STATE_INSIDE_TOKEN_AREA)
		{
			Assert(iTokenNameCharCount < MAX_TOKEN_LEN - 1);

			if (iTokenNameCharCount < MAX_TOKEN_LEN - 1)
			{
				cTokenName[iTokenNameCharCount] = (char)*pNewCharacters;
				iTokenNameCharCount++;
			}

			prevChar = pNewCharacters;
			pNewCharacters++;
		}
		else
		// otherwise outside the token area:
		if (iTokenState == TOKEN_STATE_STANDARD_AREA)
		{
			// just copy over the existing chars
			pStringOut[iNewTextCount] = *pNewCharacters;

			iNewTextCount++;
			prevChar = pNewCharacters;
			pNewCharacters++;

			pStringOut[iNewTextCount] = 0; // must terminate each time as we may wish to append to this string
		}
	}

	if (CTextConversion::GetByteCount(pStringOut) < iMaxStringLength-MAX_CHARS_FOR_MINOR_HTML_CODE)
	{
		//
		// if bold or italic is left "open" then close these off in the text:
		//
		if (bInsideBold)
		{
			char boldCode[] = "</b>";

			strcat( pStringOut, boldCode );
			iNewTextCount += fwTextUtil::GetByteCount(boldCode);

			*pUsingHtml = true;
		}
		if (bInsideItalic)
		{
			char italicCode[] = "</i>";

			strcat( pStringOut, italicCode );
			iNewTextCount += fwTextUtil::GetByteCount(italicCode);

			*pUsingHtml = true;
		}
	}

	pStringOut[iNewTextCount++] = 0;

	u32 iCharCount = fwTextUtil::GetByteCount(pStringOut);
	if (iCharCount >= iMaxStringLength)
	{
		Assertf(0, "Text string has more final characters (%d) after creating final string than max of %d", iCharCount, iMaxStringLength);
		pStringOut[iMaxStringLength-1] = 0;
	}
}

void CTextFormat::GetIconListFormatString(const IconList& icons, char* buffer, u32 bufferSize, const IconParams& iconParams)
{
	buffer[0] = '\0';
	
	const u32 TMP_BUFFER_SIZE = 15;
	char tmpBuffer[TMP_BUFFER_SIZE];

	for(s32 i = 0; i < icons.size(); ++i)
	{
		tmpBuffer[0] = '\0';

		const char* terminator;
		if(i == (icons.size() -1))
		{
			terminator = "";
		}
		else
		{
			terminator = KEY_FMT_SEPERATOR;
		}

		if(icons[i].GetIconText(tmpBuffer, TMP_BUFFER_SIZE, iconParams))
		{
			safecat( buffer, tmpBuffer, bufferSize );
			safecat( buffer, terminator, bufferSize );
		}
	}
}



void CTextFormat::FillIconListFromTokenizedString(char* buffer, int* iIDs, int iIdSize, CTextFormat::IconList &icons)
{
	//@TODO: This function is here for compatibility, but using a number to write to a string to then atoi the number back out is wasteful.
	// We'd need to split CTextFormat::Icon::GetIconText in two.
	if(iIDs != NULL)
	{
		const int COPY_BUFFER_SIZE = 64;
		char copyBuffer[COPY_BUFFER_SIZE];
		char* tok = NULL;
		int iCount = 0;
		strcpy(copyBuffer, buffer);

		tok = strtok(copyBuffer, KEY_FMT_SEPERATOR);
		while(tok && iCount < iIdSize && iCount < icons.size())
		{
			iIDs[iCount] = icons[iCount].m_IconId;
			if(tok[0] != 't' && tok[0] != 'T')
			{
				iIDs[iCount] = atoi(tok+2); // atoi past the 'b_' part of b_xxxx
			}

			tok = strtok(NULL, KEY_FMT_SEPERATOR);
			iCount++;
		}
	}
}

bool CTextFormat::GetInputButtons(const char* inputName, char* buffer, u32 bufferSize, int* iIDs,  int iIdSize, const IconParams& iconParams)
{
	return GetInputButtons(CControl::GetInputByName(inputName), buffer, bufferSize, iIDs, iIdSize, iconParams);
}

bool CTextFormat::GetInputButtons(InputType input, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams)
{
	ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(input, iconParams.m_DeviceId, iconParams.m_MappingSlot, iconParams.m_AllowIconFallback);
	IconList icons;

	if(uiVerifyf(source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER, "Invalid source for input '%s'! If all icons show up correctly the next frame ignore this.", CControl::GetInputName(input)))
	{
		CTextFormat::GetTokenIDFromPadButton(source.m_Device, source.m_Parameter, icons);
		if(uiVerifyf(icons.size() > 0, "No icon to display for '%s'!", CControl::GetInputName(input)))
		{
			CorrectButtonOrder(icons, iconParams);
			CTextFormat::GetIconListFormatString(icons, buffer, bufferSize, iconParams);
			CTextFormat::FillIconListFromTokenizedString(buffer, iIDs, iIdSize, icons);
			return true;
		}
	}

	return false;
}

bool CTextFormat::GetInputGroupButtons(const char* inputGroupName, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams)
{
	return GetInputGroupButtons(CControlMgr::GetPlayerMappingControl().GetInputGroupByName(inputGroupName), buffer, bufferSize, iIDs, iIdSize, iconParams);
}

bool CTextFormat::GetInputGroupButtons(InputGroup inputGroup, char* buffer, u32 bufferSize, int* iIDs, int iIdSize, const IconParams& iconParams)
{
	CControl::InputGroupList igList;
	IconList icons;

	CControlMgr::GetPlayerMappingControl().GetInputsInGroup(inputGroup, igList);
	if(!igList.IsEmpty())
	{
		CTextFormat::GroupSourceList slSources;
		for(u32 i = 0; i < igList.size(); ++i)
		{
			ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(igList[i], iconParams.m_DeviceId, iconParams.m_MappingSlot, iconParams.m_AllowIconFallback);

			if( uiVerifyf(source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER,
				"Invalid source for an input in input group '%s'!", CControl::GetInputGroupName(inputGroup)) )
			{
				slSources.Push(source);
			}
		}

		CTextFormat::GetDeviceIconsFromInputGroup(slSources, icons);

		if(uiVerifyf(icons.size() > 0, "No icon to display for '%s'!", CControl::GetInputGroupName(inputGroup)))
		{
			CorrectButtonOrder(icons, iconParams);
			CTextFormat::GetIconListFormatString(icons, buffer, bufferSize, iconParams);
			CTextFormat::FillIconListFromTokenizedString(buffer, iIDs, iIdSize, icons);

			return true;
		}
	}
	else
	{
		uiAssertf(false,"INPUTGROUP (%s) does not contain any valid inputs.", CControl::GetInputGroupName(inputGroup));
	}

	return false;
}

void CTextFormat::CorrectButtonOrder(IconList& icons, const IconParams& iconParams)
{
	if(iconParams.m_CorrectButtonOrder)
	{
		Icon tmp;

		// We don't appear to have a reverse function so do it manually.
		for(s32 front = 0, back = icons.size() -1; front < back; ++front, --back)
		{
			tmp			 = icons[front];
			icons[front] = icons[back];
			icons[back]	 = tmp;
		}
	}
}

#define BLIP_STR_LEN 5

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetRadarBlipFromToken
// PURPOSE: retrieves the correct blip id based on the token number passed
/////////////////////////////////////////////////////////////////////////////////////
char *CTextFormat::GetRadarBlipFromToken(char *pTokenText)
{
	if (!strncmp("BLIP_", pTokenText, BLIP_STR_LEN))
	{
		return pTokenText + BLIP_STR_LEN;
	}

	return NULL;
}

const char *CTextFormat::GetRadarBlipFromToken(const char *pTokenText)
{
	if (!strncmp("BLIP_", pTokenText, BLIP_STR_LEN))
	{
		return pTokenText + BLIP_STR_LEN;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetControllerIconFromToken
// PURPOSE: retrieves the correct controller pad icon based on the token
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::GetControllerIconsFromToken(char *pTokenText, IconList &Icons, const char* ASSERT_ONLY(pFullString))
{
#if __ASSERT
	if (strncasecmp(pTokenText, "PAD_", 4) == 0)
	{
		if ( (strncasecmp(pTokenText, "PAD_RSTICK_ROTATE", 17) != 0) && (strncasecmp(pTokenText, "PAD_LSTICK_ROTATE", 17) != 0))
		{
			Assertf(0, "Graeme - Support for ~PAD_ will soon be removed. Use an equivalent ~INPUT_ instead. Token is %s. Full string is %s", pTokenText, pFullString);
		}
	}
#endif	//	__ASSERT

	if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_UP))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_UP);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LEFT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LEFT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_A))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_B))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_X))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_X);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_Y))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_Y);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_START))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_START);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_BACK))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_BACK);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LB))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LB);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RB))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RB);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RT);

	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_UP))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UP);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_DOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_DOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_LEFT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_RIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_RIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_NONE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_NONE);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_ALL))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_ALL);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_UPDOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UPDOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_DPAD_LEFTRIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFTRIGHT);

	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_UP))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UP);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_DOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_DOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_LEFT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_RIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_RIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_NONE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_NONE);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_ALL))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_UPDOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UPDOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_LEFTRIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFTRIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_LSTICK_ROTATE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ROTATE);

	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_UP))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UP);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_DOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_DOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_LEFT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_RIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_RIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_NONE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_NONE);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_ALL))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ALL);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_UPDOWN))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UPDOWN);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_LEFTRIGHT))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFTRIGHT);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_RSTICK_ROTATE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ROTATE);

#if defined __PS3_BUTTONS
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_DRIVE))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_DRIVE);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_PITCH))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_PITCH);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_RELOAD))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_RELOAD);
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_CONTROLLER_SIXAXIS_ROLL))
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_SIXAXIS_ROLL);
#endif  // __PS3_BUTTONS

	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_INPUT_ACCEPT))
	{
		Assertf(false, "~%s~  in help text is not compatable with PC. It is possible that ~INPUT_FRONTEND_ACCEPT~ might be appropriate!", FONT_CHAR_TOKEN_INPUT_ACCEPT);
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A);  // here we can also check and return different values depenent on setup
	}
	else if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_INPUT_CANCEL))
	{
		Assertf(false, "~%s~  in help text is not compatable with PC. It is possible that ~INPUT_FRONTEND_CANCEL~ might be appropriate!", FONT_CHAR_TOKEN_INPUT_CANCEL);
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B);  // here we can also check and return different values dependent on setup
	}

	else if (!::strnicmp(pTokenText, "INPUT_", 6))
	{
		ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(pTokenText);

		Assertf( source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER,
				 "No devices are mapped to %s! If all icons show up correctly the next frame ignore this.", pTokenText);

		if(source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER)
		{
			GetTokenIDFromPadButton(source.m_Device, source.m_Parameter, Icons);
		}
	}
	else if(!::strnicmp(pTokenText, "INPUTGROUP_", 11))
	{
		CControl::InputGroupList inputs;
		CControl::GetInputsInGroup(pTokenText, inputs);

		GroupSourceList sources;
		for(u32 i = 0; i < inputs.size(); ++i)
		{
			ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(inputs[i]);

			if( Verifyf(source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER,
				"An invalid sources for an input in input group '%s'!", pTokenText) )
			{
				sources.Push(source);
			}
		}

		GetDeviceIconsFromInputGroup(sources, Icons);
	}
}


void CTextFormat::GetTokenIDFromPadButton(ioMapperSource PadSource, unsigned PadButton, IconList &Icons)
{
	switch (PadSource)
	{
		case IOMS_PAD_AXIS :
		{
			switch (PadButton)
			{
			case IOM_AXIS_LX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFTRIGHT);
				break;
			case IOM_AXIS_LY :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UPDOWN);
				break;
			case IOM_AXIS_RX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFTRIGHT);
				break;
			case IOM_AXIS_RY :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UPDOWN);
				break;
			case IOM_AXIS_RY_UP :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_UP);
				break;
			case IOM_AXIS_RY_DOWN :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_DOWN);
				break;
			case IOM_AXIS_RX_LEFT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_LEFT);
				break;
			case IOM_AXIS_RX_RIGHT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_RIGHT);
				break;
			case IOM_AXIS_LY_UP :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UP);
				break;
			case IOM_AXIS_LY_DOWN :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_DOWN);
				break;
			case IOM_AXIS_LX_LEFT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFT);
				break;
			case IOM_AXIS_LX_RIGHT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_RIGHT);
				break;
			default :
				Assertf(0, "CFont::GetTokenIDFromPadButton - unhandled PadButton value within IOMS_PAD_AXIS");
				break;
			}
		}
		break;

		case IOMS_PAD_ANALOGBUTTON :
		{
			switch (PadButton)
			{
			case ioPad::LRIGHT_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_RIGHT);
				break;
			case ioPad::LLEFT_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFT);
				break;
			case ioPad::LUP_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UP);
				break;
			case ioPad::LDOWN_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_DOWN);
				break;
			case ioPad::RUP_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_Y);
				break;
			case ioPad::RRIGHT_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B);
				break;
			case ioPad::RDOWN_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A);
				break;
			case ioPad::RLEFT_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_X);
				break;
			case ioPad::L1_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LB);
				break;
			case ioPad::R1_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RB);
				break;
			case ioPad::L2_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LT);
				break;
			case ioPad::R2_INDEX :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RT);
				break;
			default :
				Assertf(0, "CFont::GetTokenIDFromPadButton - unhandled PadButton value within IOMS_PAD_ANALOGBUTTON");
				break;
			}
		}
		break;

		case IOMS_PAD_DIGITALBUTTON :
		{
			switch (PadButton)
			{
			case ioPad::L2 : 
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LT);
				break;
			case ioPad::R2 :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RT);
				break;
			case ioPad::L1 :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_LB);
				break;
			case ioPad::R1 :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_RB);
				break;
			case ioPad::RUP :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_Y);
				break;
			case ioPad::RRIGHT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B);
				break;
			case ioPad::RDOWN :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A);
				break;
			case ioPad::RLEFT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_X);
				break;
			case ioPad::SELECT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_BACK);
				break;
			case ioPad::L3 :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_NONE);	//	or FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL
				break;
			case ioPad::R3 :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_NONE);	//	or FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ALL
				break;
			case ioPad::START :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_START);
				break;
			case ioPad::LUP :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UP);
				break;
			case ioPad::LRIGHT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_RIGHT);
				break;
			case ioPad::LDOWN :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_DOWN);
				break;
			case ioPad::LLEFT :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFT);
				break;
			case ioPad::TOUCH :
				SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_BACK);	// TODO: Change to the correct button icon once we have an icon for pressing the touch pad
				break;
			default :
				Assertf(0, "CFont::GetTokenIDFromPadButton - unhandled PadButton value (%d) within IOMS_PAD_DIGITALBUTTON (%d)", PadButton, PadSource);
				break;
			}
		}
		break;

#if __WIN32PC
		case IOMS_KEYBOARD:
		{
			SafeAddIconToList(Icons, Icon(PadButton & 0xFF, Icon::KEYCODE_ID));
		}
		break;

		case IOMS_MKB_AXIS:
		{
			// create negative button icon.
			Icon::Type type = Icon::KEYCODE_ID;
			ioMapperSource source = IOMS_KEYBOARD;
			if(ioMapper::IsMkbAxisNegativeAMouseButton(PadButton))
			{
				type = Icon::MOUSE_ID;
				source = IOMS_MOUSE_BUTTON;
			}
			else if(ioMapper::IsMkbAxisNegativeAMouseWheel(PadButton))
			{
				type = Icon::MOUSE_ID;
				source = IOMS_MOUSE_WHEEL;
			}

			ioMapperParameter param = ioMapper::ConvertParameterToMapperValue(source, ioMapper::GetMkbAxisNegative(PadButton));
			SafeAddIconToList(Icons, Icon(param, type));

			// create positive button icon.
			type = Icon::KEYCODE_ID;
			source = IOMS_KEYBOARD;
			if(ioMapper::IsMkbAxisPositiveAMouseButton(PadButton))
			{
				type = Icon::MOUSE_ID;
				source = IOMS_MOUSE_BUTTON;
			}
			else if(ioMapper::IsMkbAxisPositiveAMouseWheel(PadButton))
			{
				type = Icon::MOUSE_ID;
				source = IOMS_MOUSE_WHEEL;
			}

			param = ioMapper::ConvertParameterToMapperValue(source, ioMapper::GetMkbAxisPositive(PadButton));
			SafeAddIconToList(Icons, Icon(param, type));
		}
		break;

		case IOMS_MOUSE_BUTTON:
		case IOMS_MOUSE_ABSOLUTEAXIS:
		case IOMS_MOUSE_CENTEREDAXIS:
		case IOMS_MOUSE_RELATIVEAXIS:
		case IOMS_MOUSE_SCALEDAXIS:
		{
			ioMapperParameter param = ioMapper::ConvertParameterToMapperValue(PadSource, PadButton);
			
			if(param == IOM_IAXIS_X)
			{
				param = IOM_AXIS_X;
			}
			else if(param == IOM_IAXIS_Y)
			{
				param = IOM_AXIS_Y;
			}
			else if(ioMouse::IsLeftRightButtonSwapped())
			{
				if(param == MOUSE_LEFT)
				{
					param = MOUSE_RIGHT;
				}
				else if(param == MOUSE_RIGHT)
				{
					param = MOUSE_LEFT;
				}
			}

			SafeAddIconToList(Icons, Icon(param, Icon::MOUSE_ID));
		}
		break;

		case IOMS_MOUSE_WHEEL:
		{
			if(PadButton == IOM_WHEEL_UP || PadButton == IOM_WHEEL_DOWN)
			{
				SafeAddIconToList(Icons, Icon(PadButton, Icon::MOUSE_ID));
			}
			else
			{
				SafeAddIconToList(Icons, Icon(Icon::MOUSE_WHL_UD, Icon::MOUSE_ID));
			}
		}
		break;
#endif // __WIN32PC

#if USE_STEAM_CONTROLLER
		case IOMS_GAME_CONTROLLED:
			// NOTE: 0 is invalid so we do not use >=
			if(Verifyf(PadButton > 0 && PadButton < k_EControllerActionOrigin_Count, "Invalid steam icon id %u!", PadButton))
			{
				// The steam icons start at 50, our valid ids start at 1 so the offset is 49.
				static const unsigned SteamIconOffset = 49;

				unsigned iconId = PadButton;
				// We only have 1 icon for all icons in this range.
				if(iconId > k_EControllerActionOrigin_Gyro_Move && iconId <= k_EControllerActionOrigin_Gyro_Roll)
				{
					iconId = k_EControllerActionOrigin_Gyro_Move;
				}
				// The icons for the A and B buttons had to be moved in scaleform as they overlapped with reserved values. Rather than redo all icons,
				// The A and B icon ids were moved to the end of the steam icons.
				else if(iconId == k_EControllerActionOrigin_A || iconId == k_EControllerActionOrigin_B)
				{
					// Move the icon id up to after the move icon.
					iconId += k_EControllerActionOrigin_Gyro_Move;
				}

				// The steam icon doesn't support icon collapsing so there could be duplicate icons. If so, do not add the icon again.
				const Icon steamIcon = SteamIconOffset + iconId;
				if(Icons.Find(steamIcon) == -1)
				{
					SafeAddIconToList(Icons, steamIcon);
				}
			}
			break;
#endif // USE_STEAM_CONTROLLER

		default :
		{
			Assertf(0, "CFont::GetTokenIDFromPadButton - value of ioMapperSource not handled");
			break;
		}
	}

	if(Icons.size() == 0)
		SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_NONE);
}

#if __WIN32PC
bool IsXAxis(const ioSource& source)
{
	return source.m_Parameter == IOM_AXIS_X || source.m_Parameter == IOM_IAXIS_X;
}

bool IsYAxis(const ioSource& source)
{
	return source.m_Parameter == IOM_AXIS_Y || source.m_Parameter == IOM_IAXIS_Y;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetDeviceIconsFromTwoAxis
// PURPOSE: retrieves the correct icons for the two sources.
/////////////////////////////////////////////////////////////////////////////////////
bool CTextFormat::GetDeviceIconsFromTwoAxis( const ioSource& UpDown, const ioSource& LeftRight, IconList& Icons )
{
	bool iconFound = false;
	bool sameDevice = UpDown.m_DeviceIndex == LeftRight.m_DeviceIndex && UpDown.m_Device == LeftRight.m_Device;

	// Pad-Left Stick
	if(sameDevice)
	{
		if((UpDown.m_Parameter == IOM_AXIS_LY && LeftRight.m_Parameter == IOM_AXIS_LX) || (UpDown.m_Parameter == IOM_AXIS_LX && LeftRight.m_Parameter == IOM_AXIS_LY))
		{
			SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL);
			iconFound = true;
		}
		// Pad-Right Stick
		else if((UpDown.m_Parameter == IOM_AXIS_RY && LeftRight.m_Parameter == IOM_AXIS_RX) || (UpDown.m_Parameter == IOM_AXIS_RX && LeftRight.m_Parameter == IOM_AXIS_RY))
		{
			SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ALL);
			iconFound = true;
		}
#if __WIN32PC
		else if( (IsXAxis(UpDown) && IsYAxis(LeftRight)) || (IsXAxis(LeftRight) && IsYAxis(UpDown)) )
		{
			SafeAddIconToList(Icons, Icon(Icon::MOUSE_MOV_ALL, Icon::MOUSE_ID));
			iconFound = true;
		}
		else if((UpDown.m_Parameter == IOM_WHEEL_UP && LeftRight.m_Parameter == IOM_WHEEL_DOWN) || (UpDown.m_Parameter == IOM_WHEEL_DOWN && LeftRight.m_Parameter == IOM_WHEEL_UP))
		{
			SafeAddIconToList(Icons, Icon(Icon::MOUSE_WHL_UD, Icon::MOUSE_ID));
			iconFound = true;
		}

		// Really these should not come through as input groups but just in case we can collaps them.
		else if((UpDown.m_Parameter == IOM_AXIS_X_LEFT && LeftRight.m_Parameter == IOM_AXIS_X_RIGHT) || (UpDown.m_Parameter == IOM_AXIS_X_RIGHT && LeftRight.m_Parameter == IOM_AXIS_X_LEFT))
		{
			SafeAddIconToList(Icons, Icon(Icon::MOUSE_MOV_LR, Icon::MOUSE_ID));
			iconFound = true;
		}
		else if((UpDown.m_Parameter == IOM_AXIS_Y_UP && LeftRight.m_Parameter == IOM_AXIS_Y_DOWN) || (UpDown.m_Parameter == IOM_AXIS_Y_DOWN && LeftRight.m_Parameter == IOM_AXIS_Y_UP))
		{
			SafeAddIconToList(Icons, Icon(Icon::MOUSE_MOV_UD, Icon::MOUSE_ID));
			iconFound = true;
		}
#endif // __WIN32PC
		else
		{
			u32 dpadFlags = GetDpadIconDirection(UpDown) | GetDpadIconDirection(LeftRight);

			 if(dpadFlags == (DPAD_LEFT | DPAD_RIGHT))
			 {
				 SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFTRIGHT);
				 iconFound = true;
			 }
			 else if(dpadFlags == (DPAD_UP | DPAD_DOWN))
			 {
				 SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UPDOWN);
				 iconFound = true;
			 }
		}
	}
	return iconFound;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetDeviceIconsFromInputGroup
// PURPOSE: retrieves the correct icons for the devices used for each input in the
//          group.
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::GetDeviceIconsFromInputGroup( const GroupSourceList& groupSources, IconList& Icons )
{
	// As we might alter the sources we will need a copy.
	GroupSourceList sources(groupSources);

	const IconList::size_type iconCount = Icons.size();

	if(Verifyf(sources.size() > 0, "No inputs in the input group!"))
	{
		switch (sources.size())
		{
		case IGT_SINGLE_OR_DOUBLE_AXIS:
			GetDeviceIconsFromTwoAxis(sources[0], sources[1], Icons);
			break;

		case IGT_DOUBLE_DPAD_AXIS:
			{
				u32 dpadFlags = DPAD_NONE;

				for(s32 i = 0; i < sources.size(); ++i)
				{
					dpadFlags |= GetDpadIconDirection(sources[i]);
				}

				// If all 4 directions of the dpad are found.
				if(dpadFlags == DPAD_ALL)
				{
					SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_ALL);
				}
			}
			break;

		case IGT_GENERIC_SINGLE_AXIS:
		case IGT_GENERIC_DOUBLE_AXIS:
			{
				// To simplify things we split the source types into groups of type.
				GroupSourceList axis;
				GroupSourceList buttons;
#if RSG_PC
				GroupSourceList keyboardMouse;
#endif // RSG_PC

				for(s32 i = 0; i < sources.size(); ++i)
				{
#if RSG_PC
					// If the input is a keyboard axis or a mouse axis.
					if(sources[i].m_Device == IOMS_MKB_AXIS || (sources[i].m_Device >= IOMS_MOUSE_ABSOLUTEAXIS && sources[i].m_Device <= IOMS_MOUSE_SCALEDAXIS))
					{
						keyboardMouse.Push(sources[i]);
					}
					// If the input is a joystick axis.
					else if(sources[i].m_Device >= IOMS_JOYSTICK_AXIS && sources[i].m_Device <= IOMS_JOYSTICK_AXIS_POSITIVE)
					{
						axis.Push(sources[i]);
					}
					// There are other keyboard mouse sources we do not care about!
					else if(sources[i].m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE)
#endif // RSG_PC
					{
						if(sources[i].m_Device == IOMS_PAD_AXIS)
						{
							axis.Push(sources[i]);
						}
						else
						{
							buttons.Push(sources[i]);
						}
					}
				}

#if RSG_PC
				// If there are any keyboard mouse controls then use them as a priority!
				if(keyboardMouse.size() > 0)
				{
					sources = keyboardMouse;
				}
				else
#endif // RSG_PC
				{
					if(IsPossbleGenericAxis(buttons, axis))
					{
						// use recursion to get the icons rather than copying the code.
						GetDeviceIconsFromInputGroup(buttons, Icons);
						GetDeviceIconsFromInputGroup(axis, Icons);

						ConvertToGenericAxisIcons(Icons);
					}
				}
			}
			break;

		default:
			break;
		}
		
		// If no icons were added.
		if(Icons.size() == iconCount)
		{
			for(s32 i = 0; i < sources.size(); ++i)
			{
				GetTokenIDFromPadButton(sources[i].m_Device, sources[i].m_Parameter, Icons);
			}
		}
	}
}

eHUD_COLOURS CTextFormat::ConvertToHudColour( eOverlayTextColours const memeColour )
{
	switch( memeColour )
	{
	default:
	case eOverlayTextColours_White:
		return HUD_COLOUR_WHITE;

	case eOverlayTextColours_RedDark:
		return HUD_COLOUR_REDDARK;

	case eOverlayTextColours_Red:
		return HUD_COLOUR_RED;

	case eOverlayTextColours_RedLight:
		return HUD_COLOUR_REDLIGHT;

	case eOverlayTextColours_Pink:
		return HUD_COLOUR_PINK;

	case eOverlayTextColours_Purple:
		return HUD_COLOUR_PURPLE;

	case eOverlayTextColours_PurpleDark:
		return HUD_COLOUR_PURPLEDARK;

	case eOverlayTextColours_BlueDark:
		return HUD_COLOUR_BLUEDARK;

	case eOverlayTextColours_Blue:
		return HUD_COLOUR_BLUE;

	case eOverlayTextColours_BlueLight:
		return HUD_COLOUR_BLUELIGHT;

	case eOverlayTextColours_GreenLight:
		return HUD_COLOUR_GREENLIGHT;

	case eOverlayTextColours_Green:
		return HUD_COLOUR_GREEN;

	case eOverlayTextColours_GreenDark:
		return HUD_COLOUR_GREENDARK;

	case eOverlayTextColours_YellowLight:
		return HUD_COLOUR_YELLOWLIGHT;

	case eOverlayTextColours_Yellow:
		return HUD_COLOUR_YELLOW;

	case eOverlayTextColours_YellowDark:
		return HUD_COLOUR_YELLOWDARK;

	case eOverlayTextColours_OrangeLight:
		return HUD_COLOUR_ORANGELIGHT;

	case eOverlayTextColours_Orange:
		return HUD_COLOUR_ORANGE;

	case eOverlayTextColours_OrangeDark:
		return HUD_COLOUR_ORANGEDARK;

	case eOverlayTextColours_GreyLight:
		return HUD_COLOUR_GREYLIGHT;

	case eOverlayTextColours_Grey:
		return HUD_COLOUR_GREY;

	case eOverlayTextColours_GreyDark:
		return HUD_COLOUR_GREYDARK;

	case eOverlayTextColours_Black:
		return HUD_COLOUR_BLACK;
	}
}



eOverlayTextColours CTextFormat::ConvertToOverlayTextColour( eHUD_COLOURS const c_hudColor )
{
	for (s32 overlayColor = eOverlayTextColours_First; overlayColor < eOverlayTextColours_MAX; overlayColor++)
	{
		eHUD_COLOURS const c_testHudColor = ConvertToHudColour((eOverlayTextColours)overlayColor);
		
		if (c_testHudColor == c_hudColor)
		{
			return (eOverlayTextColours)overlayColor;
		}
	}

	return eOverlayTextColours_White;
}



CRGBA CTextFormat::ConvertToRGBA( eOverlayTextColours const overlayColour, float const * const alphaOverride )
{
	CRGBA outputColour( CHudColour::GetRGBA( ConvertToHudColour( overlayColour ) ) );

	int const c_alpha = alphaOverride ? (int)Clamp(  255.f * (*alphaOverride), 0.f, 255.f ) : outputColour.GetAlpha();
	outputColour.SetAlpha( c_alpha );

	return outputColour;
}



s32 CTextFormat::GetGameDisplayNameIndexOfOverlayFont( s32 const c_currentFont )
{
	switch (c_currentFont)
	{
		case FONT_STYLE_STANDARD: return 1;
		case FONT_STYLE_CURSIVE: return 2;
		case FONT_STYLE_CONDENSED: return 3;
		case FONT_STYLE_PRICEDOWN: return 4;
		default: break;
	}

	Assertf(0, "CTextFormat::GetGameDisplayNameIndexOfOverlayFont Font %d is used but we dont have a valid name index for it", c_currentFont);

	return 0;
}

// PURPOSE: Filters the given input font until we hit a valid font
// PARAMS:
//		currentFont - The font to filter
//		c_incrementalFilter - If true, we increment until we find a valid font. If false we decrement.
//		pStringToDisplay - check each character in this string. If a font doesn't support all those characters then skip that font.
// RETURNS:
//		s32 representing a valid entry in the font styles enum (See text.h)
s32 CTextFormat::FilterOverlayFonts( s32 currentFont, bool const c_incrementalFilter, const char *pStringToDisplay )
{
	s32 const c_increment = c_incrementalFilter ? 1 : -1;

#if GTA_REPLAY
	char16 wideStringToDisplay[CMontageText::MAX_MONTAGE_TEXT];
	Utf8ToWide(wideStringToDisplay, pStringToDisplay, CMontageText::MAX_MONTAGE_TEXT);
#else
	char16 wideStringToDisplay[PhonePhotoEditor::PHOTO_TEXT_MAX_LENGTH];
	Utf8ToWide(wideStringToDisplay, pStringToDisplay, PhonePhotoEditor::PHOTO_TEXT_MAX_LENGTH);
#endif

	bool bCheckAllCharactersInString = true;
	const s32 startingFont = currentFont;
	s32 pass = 0;

	bool bFontIsValid = false;
	while( !bFontIsValid )
	{
		if (currentFont == startingFont)
		{	//	In pass 1, we'll check that all characters in the string are supported in the font.
			//	If no valid font is found in pass 1 then do a second pass where we don't check the characters.
			//	Hopefully pass 2 will always succeed.
			pass++;
			if (pass > 1)
			{
				bCheckAllCharactersInString = false;
			}
		}

		// Clamp and wrap-around the font style
		currentFont = ( currentFont >= FONT_MAX_FONT_STYLES ) ? FONT_STYLE_FIRST : 
			( currentFont < FONT_STYLE_FIRST ) ? (FONT_MAX_FONT_STYLES-1) : currentFont;

		if (
			// Skip any fonts that have incomplete/invalid character sets....
			( currentFont == FONT_STYLE_LEADERBOARD || currentFont == FONT_STYLE_FIXED_WIDTH_NUMBERS || currentFont == FONT_STYLE_ROCKSTAR_TAG
				|| currentFont == FONT_STYLE_CONDENSED_NOT_GAMERNAME || currentFont == FONT_MAX_FONT_STYLES ) ||

			// ...or any fonts that re-map to the standard font style to avoid duplicate fonts 
			( currentFont != FONT_STYLE_STANDARD && CTextFormat::DoFontsShareMapping( FONT_STYLE_STANDARD, currentFont ) ) || 

			// ...or any fonts that don't support all the characters in the string to be displayed
			( bCheckAllCharactersInString && !DoAllCharactersExistInFont(currentFont, wideStringToDisplay, NELEM(wideStringToDisplay)) )
			)
		{
			currentFont += c_increment;
		}
		else
		{
			bFontIsValid = true;
		}
	}

	return currentFont;
}

bool CTextFormat::IsUnsignedInteger(const char *pString, u32 &ReturnInteger)
{
	ReturnInteger = 0;
	s32 string_length = istrlen(pString);
	if (string_length == 0)
	{
		return false;
	}

	s32 char_loop = 0;
	while (char_loop < string_length)
	{
		s32 current_char = pString[char_loop] - '0';
		if ((current_char >= 0) && (current_char <= 9))
		{
			ReturnInteger *= 10;
			ReturnInteger += current_char;
		}
		else
		{
			return false;
		}
		char_loop++;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetColourFromToken
// PURPOSE: retrieves the correct hud colour for the token passed
/////////////////////////////////////////////////////////////////////////////////////
Color32 CTextFormat::GetColourFromToken(char *pTokenText, bool *bFoundColour, const Color32 *pOriginalColour)
{
	*bFoundColour = true;  // assume we find a colour unless we get to end of function

	if ( (!strncmp("HUD_COLOUR_", pTokenText, 11)) ||  // check if the actual hud colour id is the token
		(!strncmp("HC_", pTokenText, 3)) )  // also HUD_COLOUR_ abbreviated to HC_
	{
		char cFullColourTokenText[255];
		if ((strlen(pTokenText) >= 3) && (!strncmp("HC_", pTokenText, 3)))
		{
			u32 StringAsInteger = 0;
			if (IsUnsignedInteger(&pTokenText[3], StringAsInteger))
			{
				return CHudColour::GetRGBA(static_cast<eHUD_COLOURS>(StringAsInteger));
			}
			sprintf(cFullColourTokenText, "HUD_COLOUR_%s", &pTokenText[3]);
		}
		else
		{
			safecpy(cFullColourTokenText, pTokenText, 255);
		}

		return CHudColour::GetRGBA(CHudColour::GetColourFromKey(cFullColourTokenText));
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_RED)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_RED);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_GREEN)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_GREEN);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_BLUE)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_BLUE);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_WHITE)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_WHITE);
	}

	if ( (*pTokenText == FONT_CHAR_TOKEN_COLOUR_GREY) || (*pTokenText == FONT_CHAR_TOKEN_COLOUR_FOREIGN_LANGUAGE_SUBTITLE) )
	{
		return CHudColour::GetRGBA(HUD_COLOUR_MENU_GREY);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_ORANGE)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_ORANGE);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_YELLOW)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_YELLOW);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_PURPLE)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_PURPLE);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_PINK)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_PINK);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_MENU)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_MID_GREY_MP);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_BLACK)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_BLACK);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_BLUEDARK)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_BLUEDARK);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_STANDARD)
	{				

		return (*pOriginalColour);  // revert back to the original colour
	}

	if (*pTokenText == FONT_CHAR_TOKEN_SCRIPT_VARIABLE)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_SCRIPT_VARIABLE);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_SCRIPT_VARIABLE_2)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_SCRIPT_VARIABLE_2);
	}

	if (*pTokenText == FONT_CHAR_TOKEN_COLOUR_FRIENDLY)
	{
		return CHudColour::GetRGBA(HUD_COLOUR_FRIENDLY);
	}

	// we didnt find a colour
	*bFoundColour = false;
	return Color32(0,0,0,0);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetSpecificToken
// PURPOSE: retrieves the correct id for specific text tokens
/////////////////////////////////////////////////////////////////////////////////////
s32 CTextFormat::GetSpecificToken(char *pTokenText)
{
	if (*pTokenText == FONT_CHAR_TOKEN_NEWLINE)
		return FONT_CHAR_TOKEN_ID_NEWLINE;

	if ((!::strcmpi(pTokenText, FONT_CHAR_TOKEN_BOLD)) || (*pTokenText == FONT_CHAR_TOKEN_HIGHLIGHT))
		return FONT_CHAR_TOKEN_ID_BOLD;

	if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_ITALIC))
		return FONT_CHAR_TOKEN_ID_ITALIC;

	if ((!::strcmpi(pTokenText, FONT_CHAR_TOKEN_WANTED_STAR)) ||
		(!::strcmpi(pTokenText, FONT_CHAR_TOKEN_WANTED_STAR_ABBRIEVIATED)))
		return FONT_CHAR_TOKEN_ID_WANTED_STAR;

	if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_ROCKSTAR_LOGO))
		return FONT_CHAR_TOKEN_ID_ROCKSTAR_LOGO;

	if (!::strcmpi(pTokenText, FONT_CHAR_TOKEN_NON_RENDERABLE_TEXT))
		return FONT_CHAR_TOKEN_ID_NON_RENDERABLE_TEXT;

	if (*pTokenText == FONT_CHAR_TOKEN_DIALOGUE)
		return FONT_CHAR_TOKEN_ID_DIALOGUE;

	// no token found, return invalid
	return FONT_CHAR_TOKEN_INVALID;
}

void CTextFormat::ConvertToGenericAxisIcons( IconList &Icons )
{
	if(Icons.size() == 2)
	{
		if(Icons[0].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_ALL && Icons[1].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ALL)
		{
			Icons.clear();
			SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_ALL);
		}
		else if(Icons[0].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_LEFTRIGHT && Icons[1].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_LEFTRIGHT)
		{
			Icons.clear();
			SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_LEFTRIGHT);
		}
		else if(Icons[0].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_DPAD_UPDOWN && Icons[1].m_IconId == FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_UPDOWN)
		{
			Icons.clear();
			SafeAddIconToList(Icons, FONT_CHAR_TOKEN_ID_CONTROLLER_UPDOWN);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetDpadIcon
// PURPOSE: retrieves the dpad icon type for a source.
// PARAMS:  source - the source to return the icon for.
/////////////////////////////////////////////////////////////////////////////////////
CTextFormat::DpadIconDirection CTextFormat::GetDpadIconDirection( const ioSource& source )
{
	if(source.m_Device != IOMS_UNDEFINED STEAM_CONTROLLER_ONLY(&& source.m_Device != IOMS_GAME_CONTROLLED))
	{
		switch(ioMapper::ConvertParameterToMapperValue(source.m_Device, source.m_Parameter))
		{
		case LUP:
		case LUP_INDEX:
#if __WIN32PC
		case IOM_POV1_UP:
		case IOM_POV2_UP:
		case IOM_POV3_UP:
		case IOM_POV4_UP:
#endif // __WIN32PC
			return DPAD_UP;

		case LDOWN:
		case LDOWN_INDEX:
#if __WIN32PC
		case IOM_POV1_DOWN:
		case IOM_POV2_DOWN:
		case IOM_POV3_DOWN:
		case IOM_POV4_DOWN:
#endif // __WIN32PC
			return DPAD_DOWN;

		case LLEFT:
		case LLEFT_INDEX:
#if __WIN32PC
		case IOM_POV1_LEFT:
		case IOM_POV2_LEFT:
		case IOM_POV3_LEFT:
		case IOM_POV4_LEFT:
#endif // __WIN32PC
			return DPAD_LEFT;

		case LRIGHT:
		case LRIGHT_INDEX:
#if __WIN32PC
		case IOM_POV1_RIGHT:
		case IOM_POV2_RIGHT:
		case IOM_POV3_RIGHT:
		case IOM_POV4_RIGHT:
#endif // __WIN32PC
			return DPAD_RIGHT;

		default:
			break;
		}
	}

	return DPAD_NONE;
}

// PURPOSE: get the font name based on the style enum
void CTextFormat::GetFontName(GString& fontName, int style)
{
	switch (style)
	{
	case FONT_STYLE_STANDARD:
		{
			fontName = "$Font2";
			break;
		}

	case FONT_STYLE_CURSIVE:
		{
			fontName = "$Font5";
			break;
		}

	case FONT_STYLE_ROCKSTAR_TAG:
		{
			fontName = "$RockstarTAG";
			break;
		}

	case FONT_STYLE_LEADERBOARD:
		{
			fontName = "$GTAVLeaderboard";
			break;
		}

	case FONT_STYLE_CONDENSED:
		{
			fontName = "$Font2_cond";
			break;
		}

	case FONT_STYLE_FIXED_WIDTH_NUMBERS:
		{
			fontName = "$FixedWidthNumbers";
			break;
		}

	case FONT_STYLE_CONDENSED_NOT_GAMERNAME:
		{
			fontName = "$Font2_cond_NOT_GAMERNAME";
			break;
		}

	case FONT_STYLE_PRICEDOWN:
		{
			fontName = "$gtaCash";
			break;
		}
	case FONT_STYLE_TAXI:
		{
			fontName = "$Taxi_font";
			break;
		}

	default:
		{
			Warningf( "CTextFormat::GetFontName - Invalid font style (%d) set", style );
			fontName = "$Font2";  // default to standard font
			break;
		}
	}
}

bool CTextFormat::DoFontsShareMapping( int styleA, int styleB )
{
	GString unmappedNameA;
	GetFontName( unmappedNameA, styleA );

	GString unmappedNameB;
	GetFontName( unmappedNameB, styleB );

	return CScaleformMgr::DoFontsShareMapping( unmappedNameA.ToCStr(), unmappedNameB.ToCStr() );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::SetupFontParams
// PURPOSE: sets up all the Scaleform font params in one function
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::SetupFontParams(GFxDrawTextManager::TextParams& params, const CTextLayout *pFormatDetails, float fPosition, Vector2 *LeftRight)
{
	if (!CText::AreScaleformFontsInitialised())
	{
		return;
	}

	GetFontName(params.FontName, pFormatDetails->Style);

	params.FontSize = pFormatDetails->Scale.y;
	params.Multiline = true;
	params.WordWrap = true;
	params.TextColor = GColor(pFormatDetails->Color.GetRed(),pFormatDetails->Color.GetGreen(),pFormatDetails->Color.GetBlue(),pFormatDetails->Color.GetAlpha());
	params.Leading = pFormatDetails->iLeading;

	if (pFormatDetails->Orientation == FONT_CENTRE)
	{
		params.HAlignment = GFxDrawText::Align_Center;

		// we need to adjust so that the centre is always around the X pos we pass in, but keeping the
		// values set via the wrap positions
		float fHalfDiffBetweenSpace = ((pFormatDetails->WrapEnd - pFormatDetails->WrapStart) * 0.5f);
		float fAmountToAdjust = (pFormatDetails->WrapStart + fHalfDiffBetweenSpace) - fPosition;

		LeftRight->x = pFormatDetails->WrapStart-fAmountToAdjust;
		LeftRight->y = pFormatDetails->WrapEnd-fAmountToAdjust;
	}
	else if (pFormatDetails->Orientation == FONT_LEFT)
	{
		params.HAlignment = GFxDrawText::Align_Left;

		LeftRight->x = fPosition;
		LeftRight->y = pFormatDetails->WrapEnd;
	}
	else
	{
		params.HAlignment = GFxDrawText::Align_Right;

		float fOffset = 1.0f;

#if SUPPORT_MULTI_MONITOR
		if(GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			fOffset = 0.6666f;
		}
#endif // SUPPORT_MULTI_MONITOR

		LeftRight->x = pFormatDetails->WrapStart * fOffset;
		LeftRight->y = pFormatDetails->WrapEnd;
	}

	if (pFormatDetails->RenderUpwards)
	{
		params.VAlignment = GFxDrawText::VAlign_Bottom;
	}
	else
	{
		params.VAlignment = GFxDrawText::VAlign_Top;
	}

	return;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetCharacterHeight
// PURPOSE: returns the character height if the character was rendered in the current
//			viewport
/////////////////////////////////////////////////////////////////////////////////////
float CTextFormat::GetCharacterHeight(const CTextLayout& FormatDetails)
{
	float fScreenHeight = (float)GRCDEVICE.GetHeight();  // use device resolution

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // if on RT we can use the current viewport
	{
		const grcViewport *pCurrentViewport = grcViewport::GetCurrent();

		if (pCurrentViewport)
			fScreenHeight = (float)pCurrentViewport->GetHeight();
	}

	return ( (FormatDetails.Scale.y + FormatDetails.iLeading) / fScreenHeight);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::GetNumberLines
// PURPOSE: returns the number of lines that would be used if the text was to be
//			rendered inside the current viewport
/////////////////////////////////////////////////////////////////////////////////////
s32 CTextFormat::GetNumberLines(const CTextLayout& FormatDetails, float fXPos, float /*fYPos*/, const char *pStringToRender)
{
	char cTextString[MAX_CHARS_FOR_TEXT_STRING];

	s32 iNumberOfLines = 0;

	if (!CText::AreScaleformFontsInitialised())
	{
		return iNumberOfLines;
	}

	float fWidth = (float)VideoResManager::GetUIWidth();
	float fHeight = (float)VideoResManager::GetUIHeight();
#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	fWidth = (float)mon.getWidth();
	fHeight = (float)mon.getHeight();
#endif // SUPPORT_MULTI_MONITOR
	Vector2 vScreenSpace = Vector2(fWidth, fHeight);  // use device resolution

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // if on RT we can use the current viewport
	{
		const grcViewport *pCurrentViewport = grcViewport::GetCurrent();

		AssertMsg(pCurrentViewport, "CTextFormat - Invalid viewport");

		if (pCurrentViewport)
			vScreenSpace = Vector2((float)pCurrentViewport->GetWidth(), (float)pCurrentViewport->GetHeight());
	}

	//
	// setup the text params based on our current font layout:
	//

	CTextLayout LocalFormatDetails = FormatDetails;

	if (FormatDetails.bAdjustForNonWidescreen)  // adjust position, here - scale will be adjusted later in the render
	{
		fXPos /= NON_WIDESCREEN_SCALER;
		LocalFormatDetails.WrapStart /= NON_WIDESCREEN_SCALER;  // fix for the wrap issue mentioned in
		LocalFormatDetails.WrapEnd /= NON_WIDESCREEN_SCALER;
	}


	Vector2 vFinalPos(0,0);
	bool bUsingHtml;
	GFxDrawTextManager::TextParams params;
	SetupFontParams(params, &LocalFormatDetails, fXPos, &vFinalPos);

	//
	// Get final formatted string:
	//
	ParseToken(pStringToRender, cTextString, LocalFormatDetails.TextColours, &LocalFormatDetails.Color, params.FontSize, &bUsingHtml, NELEM(cTextString), true);  // go through, find and replace colour tokens with HTML tags

	if (cTextString[0] != 0)
	{
		float fWidth = (vFinalPos.y-vFinalPos.x)*vScreenSpace.x;
		GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();
		iNumberOfLines = drawText->GetNumberLines(	cTextString,
													fWidth,
													&params, bUsingHtml);
	}

	return (iNumberOfLines);
}

//	FontStyle should be taken from the enum in text.h e.g. FONT_STYLE_STANDARD
bool CTextFormat::DoesCharacterExistInFont(int FontStyle, UInt16 characterCode)
{
	if (!CText::AreScaleformFontsInitialised())
	{
		return false;
	}

	bool bCharacterExists = false;

	GString fontName;
	GetFontName(fontName, FontStyle);

	GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();
	bCharacterExists = drawText->DoesCharacterExistInFont(fontName.ToCStr(), 0, characterCode);

	return bCharacterExists;
}

//	FontStyle should be taken from the enum in text.h e.g. FONT_STYLE_STANDARD
bool CTextFormat::DoAllCharactersExistInFont(s32 fontStyle, const char16 *pWideString, s32 maxStringLength)
{
	if (pWideString)
	{
		for (int i=0; i<maxStringLength && pWideString[i]; i++)
		{
			if (!CTextFormat::DoesCharacterExistInFont(fontStyle, pWideString[i]))
			{
				uiDebugf1("Character %d doesn't exist in font %d", pWideString[i], fontStyle);
				return false;
			}
		}
	}

	return true;
}

bool CTextFormat::IsValidDisplayCharacter(rage::char16 c)
{
	const int MAX_OFF_LIMIT_CHARS = 55;
	const char16 OFF_LIMIT_CHARACTERS[MAX_OFF_LIMIT_CHARS] = {0x7f, 0x60, 0x3c, 0x3e, 0x7e, 0xa4, 0xa6, 
		0xa7, 0xab, 0xaf, 0xb5, 0xf7, 0x1b1, 0x28a, 0x2cd, 0xaa, 0xba, 0xbc, 0xbd, 0xbe, 0x38f, 0x91, 0x92, 0x93,
		0x94, 0x87, 0x95, 0x8b, 0x9b, 0x2039, 0x203a, 0x203b, 0x2190, 0x2191, 0x2192, 0x2193, 0x2260, 0x2211, 0x2215, 
		0x2223, 0x2295, 0x2299, 0x2500, 0x2502, 0x2329, 0x232a, 0x301d, 0x301e, 0x2014, 0xfe33, 0xffe0, 
		0xffe1, 0xffe3, 0};

	for(int i = 0; i < MAX_OFF_LIMIT_CHARS; ++i)
	{
		if(OFF_LIMIT_CHARACTERS[i] == c)
			return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetStringWidth
//
// purpose: returns the overall width of this string in 0-1 space
//			if a newline exists, then it only gets the width of the longest
//			string
////////////////////////////////////////////////////////////////////////////
float CTextFormat::GetStringWidth(const CTextLayout& FormatDetails, const char *pStringToRender, bool /*bIncludeSpaces*/)
{
	char cOriginalTextString[MAX_CHARS_FOR_TEXT_STRING];

	float fStringWidth = 0.0f;

	if (!CText::AreScaleformFontsInitialised())
	{
		return fStringWidth;
	}

	float fWidth = (float)VideoResManager::GetUIWidth();
#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	fWidth = (float)mon.getWidth();
#endif // SUPPORT_MULTI_MONITOR
	float fScreenWidth = fWidth;

	CTextLayout LocalFormatDetails = FormatDetails;

	if (FormatDetails.bAdjustForNonWidescreen)  // adjust position, here - scale will be adjusted later in the render
	{
		LocalFormatDetails.WrapStart /= NON_WIDESCREEN_SCALER;  // fix for the wrap issue mentioned in
		LocalFormatDetails.WrapEnd /= NON_WIDESCREEN_SCALER;
	}

	//
	// setup the text params based on our current font layout:
	//
	Vector2 vFinalPos(0,0);
	GFxDrawTextManager::TextParams params;
	SetupFontParams(params, &LocalFormatDetails, 0.0f, &vFinalPos);

	// ensure no multi-lines, by forcing some values in the param set up:
	params.Multiline = true;
	params.HAlignment = GFxDrawText::Align_Left;
	vFinalPos = Vector2(0.0f, 10000.0f);
 
	//
	// Get final formatted string:
	//
	bool bUsingHtml = false;
	ParseToken(pStringToRender, cOriginalTextString, LocalFormatDetails.TextColours, &LocalFormatDetails.Color, params.FontSize, &bUsingHtml, NELEM(cOriginalTextString), false);  // go through, find and replace colour tokens with HTML tags

	s32 iCharCount = 0;
	while (cOriginalTextString[iCharCount] != '\0')
	{
		// go through the whole string and split each line into a new string so we can test widths and get the longest
		char cPartTextString[MAX_CHARS_FOR_TEXT_STRING];

		s32 iNewStringCharCount = 0;
		while (cOriginalTextString[iCharCount] != '\0')
		{
			if (cOriginalTextString[iCharCount] == '<')  // if we hit an html tag
			{
				if (cOriginalTextString[iCharCount+1] == 'b' &&  // and if its the <br/>
					cOriginalTextString[iCharCount+2] == 'r' &&
					cOriginalTextString[iCharCount+3] == '/' &&
					cOriginalTextString[iCharCount+4] == '>')
				{
					// we found a line break, so break out
					iCharCount += 5; // skip past the <br/>
					break;
				}
			}

			cPartTextString[iNewStringCharCount++] = cOriginalTextString[iCharCount++];  // copy over each char
		}

		cPartTextString[iNewStringCharCount] = '\0';  // terminate the part string

		char *pString = &cPartTextString[0];

		if (pString[0] != 0)
		{
			GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();

			GSizeF stringSize;

			if (bUsingHtml)
				stringSize = drawText->GetHtmlTextExtent(pString, 0, &params);
			else
				stringSize = drawText->GetTextExtent(pString, 0, &params);

			float fNewWidth = (stringSize.Width / fScreenWidth);
			if (fNewWidth > fStringWidth)
			{
				fStringWidth = fNewWidth;
			}
		}
	}
	
	return (fStringWidth + 0.001f);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::FilterOutTokensFromString
// PURPOSE: removes any tokens from the string passed in
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::FilterOutTokensFromString(char *String)
{
	char buf[256];

	safecpy( buf, String, sizeof(buf) );
	char *pCharacters = &buf[0];

	s32 i=0;

	while(*pCharacters != 0)
	{

		if( *pCharacters == FONT_CHAR_TOKEN_DELIMITER )
		{
			pCharacters++;

			while(*pCharacters != FONT_CHAR_TOKEN_DELIMITER && *pCharacters != FONT_CHAR_END)
			{
				pCharacters++;
			}
		}
		else
		{

			String[i++] = *pCharacters;
		}

		pCharacters++;

	}

	// end of string:
	String[i] = char(0);
}

bool CTextFormat::IsUnsupportedSpace(char16 c)
{
	return c == 0x3000 || // Ideographic
		c == 0x205F || // Medium Mathematical
		c == 0x202F || // Narrow No-Break
		c == 0x200B || // Zero Width
		c == 0x200A || // Hair
		c == 0x2009 || // Thin
		c == 0x2008 || // Punctuation
		c == 0x2007 || // Figure
		c == 0x2006 || // Six-Per-Em
		c == 0x2005 || // Four-Per-Em
		c == 0x2004 || // Three-Per-Em
		c == 0x2003 || // Em Space
		c == 0x2002 || // En Space
		c == 0x2001 || // Em Quad
		c == 0x2000 || // En Quad
		c == 0x1680 || // Ogham Mark
		c == 0x00A0;   // No Break
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::EndFrame
// PURPOSE: at the end of each frame, we can go through and delete any unused items
//			from the buffer - must be called in the synced safe area
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::EndFrame()
{
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::FlushFontBuffer
// PURPOSE: flushes the current font buffer to the screen
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::FlushFontBuffer()
{
	using namespace CachedTextDrawing;

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_FUNC(Text_Flush);

	if (!CText::AreScaleformFontsInitialised())  // font system not initialised
	{
		return;
	}

	const unsigned rti = g_RenderThreadIndex;

	if (DrawCommands[rti].IsEmpty())
	{
		return; // nothing to draw
	}

	DIAG_CONTEXT_MESSAGE("Drawing scaleform text");

	const grcViewport *pCurrentViewport = grcViewport::GetCurrent();

	AssertMsg(pCurrentViewport, "CTextFormat - Invalid viewport");

	if (pCurrentViewport)
	{
		Vector2 vViewportSize = Vector2((float)pCurrentViewport->GetWidth(), (float)pCurrentViewport->GetHeight());

		GViewport vp((s32)vViewportSize.x, (s32)vViewportSize.y, 0, 0, (s32)vViewportSize.x, (s32)vViewportSize.y, 0);

		GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();
		CShaderLib::SetGlobalEmissiveScale(1.0f, true);

		drawText->BeginDisplay(vp);

		int numDrawCommands = DrawCommands[rti].GetCount();
		for (s32 iCount = 0; iCount < numDrawCommands; iCount++)
		{
			sDrawCommand& drawCmd = DrawCommands[rti][iCount];
			FastAssert(drawCmd.iTextCacheIndex != sCache::INVALID_ENTRY);
			sCache::Entry& cacheEntry = GfxDrawTextCache.Cache[drawCmd.iTextCacheIndex];

			GFxDrawText& gfxText = *cacheEntry.pText;

			gfxText.SetBackgroundColor(drawCmd.background ? drawCmd.bgcolor : GColor::Alpha0);
			gfxText.SetBorderColor(drawCmd.backgroundOutline ? drawCmd.bgcolor : GColor::Alpha0);

			// if we are in 4:3 we need to adjust the aspect ratio of the text
			// doing it this way round (rather than squashing it in 16:9) creates
			// a better quality font in 16:9 which is our main target display
			if (drawCmd.adjustForNonWidescreen)  // adjust the scale here, position was done earlier
			{
				GFxDrawText::Matrix scaleMatrix;
				scaleMatrix.AppendScaling(NON_WIDESCREEN_SCALER, 1.0f);
				gfxText.SetMatrix(scaleMatrix);
			}

			GRectF origRect = gfxText.GetRect();
			GRectF offsetRect = origRect;
			offsetRect.Offset(drawCmd.vPosition.x, drawCmd.vPosition.y);

			gfxText.SetRect(offsetRect);

			// Set the text filters (shadow / outline) since these apparently don't cause a re-format
			if (drawCmd.fShadow != 0.0f)
			{
				GFxDrawText::Filter filter(GFxDrawText::Filter_DropShadow);
				filter.DropShadow.Strength = drawCmd.fShadow * 2000.0f;
				filter.DropShadow.Color = GColor(0,0,0, drawCmd.dropalpha).ToColor32();
				filter.DropShadow.Angle = 65.0f;
				filter.DropShadow.Distance = 1.0f;

				gfxText.SetFilters(&filter, 1);
			}
			else if (drawCmd.outline)
			{
				GFxDrawText::Filter filter(GFxDrawText::Filter_Glow);

				filter.Glow.Strength = 2000.0f;
				filter.Glow.BlurX = 1.5f;
				filter.Glow.BlurY = 1.5f;
				filter.DropShadow.Color = GColor(0,0,0, drawCmd.dropalpha).ToColor32();
				
				if(drawCmd.outlineCut)
				{
					filter.SetKnockOut();
				}

				gfxText.SetFilters(&filter, 1);
			}
			else
			{
				gfxText.ClearFilters();
			}

			const bool c_scissorRequired = (drawCmd.scissor.Right != 0.0f && drawCmd.scissor.Bottom != 0.0f);

			if (c_scissorRequired)
			{
				// setup the scissor
				GRCDEVICE.SetScissor((int)(drawCmd.scissor.Left * vViewportSize.x),
									 (int)(drawCmd.scissor.Top * vViewportSize.y),
									 (int)(drawCmd.scissor.Right * vViewportSize.x),
									 (int)(drawCmd.scissor.Bottom * vViewportSize.y));
			}

#if RSG_PC
			if (drawCmd.StereoFlag == 1)
			{
				CShaderLib::SetReticuleDistTexture(true);
				CShaderLib::SetStereoParams(Vector4(0,0,0,1));
			}
			else if (drawCmd.StereoFlag == 2)
			{
				CShaderLib::SetStereoParams(Vector4(0,1,0,0));
			}
#endif

			gfxText.Display();

#if RSG_PC
			if (drawCmd.StereoFlag != 0)
				CShaderLib::SetStereoParams(Vector4(0,0,0,0));
#endif
			if (c_scissorRequired)
			{
				GRCDEVICE.DisableScissor();  // turn off the scissor
			}

			gfxText.SetRect(origRect);

			GfxDrawTextCache.ReleaseEntry(drawCmd.iTextCacheIndex);
		}


		DrawCommands[rti].Reset();

		drawText->EndDisplay();
	}
}



#if __BANK
static const char*		testLine[10] = {"ABCDEFGHIJ",
										"KLMNOPQRST",
										"UVWXYZ",
										"abcdefghij",
										"klmnopqrst",
										"uvwxyz",
										"0123456789",
										"!@#$%^&*()",
										"_+{}|[]\\:""",
										";'<>?,./" };

static bool fontTestRender = false;

static s32 fontType = 0;
static Color32 col(0,0,0,255);
static float fontSize = 32.90f;
static s32 style = GFxDrawText::Normal;
static bool underline = false;
static s32 aaMode = GFxDrawText::AA_Animation;
static GRectF rect[10];


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextFormat::CreateBankWidgets()
// PURPOSE: creates the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CTextFormat::CreateBankWidgets()
{
	static bool bBankCreated = false;

	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
	if(!Verifyf(bank, "CTextFormat::CreateBankWidgets - Trying to use %s before it was made.", UI_DEBUG_BANK_NAME))
		return;

	Vector2 vRes = Vector2((float)VideoResManager::GetUIWidth(), (float)VideoResManager::GetUIHeight());  // use device resolution

	//safecpy(ms_cDebugScriptedString, "WTD_BA_0", NELEM(ms_cDebugScriptedString));

	bank->AddText("Debug Scripted Text", ms_cDebugScriptedString, sizeof(ms_cDebugScriptedString), false);

	bank->AddToggle("Text Ids only", &TheText.bShowTextIdOnly);

	bank->AddToggle("Render Text to Log", &bOutputAllTextRenderToLog);

	bank->AddSlider("Text icon size scaler", &fTextIconScaler, -4.0f, 4.0f, 0.02f);
	bank->AddSlider("Text icon size offset", &fTextIconOffset, -4.0f, 4.0f, 0.02f);

	if ((!bBankCreated))
	{
		bank->PushGroup("Scaleform Font Test");

			bank->AddToggle("fontTestRender", &fontTestRender);

			bank->AddSlider("Font Type",&fontType,0,1,1);
			bank->AddColor("Font color",&col);
			bank->AddSlider("Font Size",&fontSize,0.0f,50.0f,0.1f);
			bank->AddSlider("Font Style",&style,0,3,1);
			bank->AddSlider("AA Mode",&aaMode,0,1,1);
			bank->AddToggle("Underline", &underline);


			rect[0].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[1].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[2].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[3].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[4].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[5].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[6].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[7].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[8].SetRect(128.0f, 128.0f, vRes.x, vRes.y);
			rect[9].SetRect(128.0f, 128.0f, vRes.x, vRes.y);

			bank->AddSlider("Rect 0 Left",&rect[0].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 0 Top",&rect[0].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 0 Right",&rect[0].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 0 Bottom",&rect[0].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 1 Left",&rect[1].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 1 Top",&rect[1].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 1 Right",&rect[1].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 1 Bottom",&rect[1].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 2 Left",&rect[2].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 2 Top",&rect[2].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 2 Right",&rect[2].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 2 Bottom",&rect[2].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 3 Left",&rect[3].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 3 Top",&rect[3].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 3 Right",&rect[3].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 3 Bottom",&rect[3].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 4 Left",&rect[4].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 4 Top",&rect[4].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 4 Right",&rect[4].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 4 Bottom",&rect[4].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 5 Left",&rect[5].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 5 Top",&rect[5].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 5 Right",&rect[5].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 5 Bottom",&rect[5].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 6 Left",&rect[6].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 6 Top",&rect[6].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 6 Right",&rect[6].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 6 Bottom",&rect[6].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 7 Left",&rect[7].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 7 Top",&rect[7].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 7 Right",&rect[7].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 7 Bottom",&rect[7].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 8 Left",&rect[8].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 8 Top",&rect[8].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 8 Right",&rect[8].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 8 Bottom",&rect[8].Bottom,0.0f,vRes.y,1.0f);

			bank->AddSlider("Rect 9 Left",&rect[9].Left,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 9 Top",&rect[9].Top,0.0f,vRes.y,1.0f);
			bank->AddSlider("Rect 9 Right",&rect[9].Right,0.0f,vRes.x,1.0f);
			bank->AddSlider("Rect 9 Bottom",&rect[9].Bottom,0.0f,vRes.y,1.0f);

		bank->PopGroup();

		bBankCreated = true;
	}
}



void CTextFormat::FontTest()
{
	if (!CText::AreScaleformFontsInitialised())
	{
		return;
	}

	if( false == fontTestRender )
		return;
		
	const grcViewport *pCurrentViewport = grcViewport::GetCurrent();

	AssertMsg(pCurrentViewport, "CTextFormat - Invalid viewport");

	Vector2 vViewportSize = Vector2((float)pCurrentViewport->GetWidth(), (float)pCurrentViewport->GetHeight());

	GPtr<GFxDrawText> fontTest[10];

	GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();

	for(s32 i=0;i<10;i++)
	{
		GString str(testLine[i]);

		fontTest[i] = drawText->CreateText();
		fontTest[i]->SetText(str);
		switch(fontType)
		{
			case 0:
				fontTest[i]->SetFont("$Font1");
				break;
			case 1:
			default:
				fontTest[i]->SetFont("$Font2");
				break;
		}
	
		fontTest[i]->SetColor(GColor(col.GetRed(),col.GetGreen(),col.GetBlue(),col.GetAlpha()));
		fontTest[i]->SetFontSize(fontSize);
		fontTest[i]->SetFontStyle((GFxDrawText::FontStyle)style);
		fontTest[i]->SetUnderline(underline);
		fontTest[i]->SetAAMode((GFxDrawText::AAMode)aaMode);

		
		GRectF actRect = rect[i];
		actRect.Top += (float)i*fontSize;
		fontTest[i]->SetRect(actRect);
	}

	GViewport vp((s32)vViewportSize.x, (s32)vViewportSize.y, 0, 0, (s32)vViewportSize.x, (s32)vViewportSize.y, 0);

	drawText->BeginDisplay(vp);

	for (s32 i=0; i<10; i++)
	{
		fontTest[i]->Display();
	}

	drawText->EndDisplay();
	
	for(s32 i=0;i<10;i++)
	{
		fontTest[i]->Release();
	}
	
}

#endif // __BANK


bool CTextFormat::Icon::GetIconText(char* buffer, u32 bufferSize, const IconParams& ORBIS_ONLY(iconParams)) const
{
#if KEYBOARD_MOUSE_SUPPORT
	if(m_Type == MOUSE_ID)
	{
		switch(m_IconId)
		{
		case MOUSE_MOV_LEFT:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_LEFT);
			return true;

		case MOUSE_MOV_RIGHT:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_RIGHT);
			return true;

		case MOUSE_MOV_UP:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_UP);
			return true;

		case MOUSE_MOV_DOWN:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_DOWN);
			return true;

		case MOUSE_MOV_LR:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_LEFTRIGHT);
			return true;

		case MOUSE_MOV_UD:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_UPDOWN);
			return true;

		case MOUSE_MOV_ALL:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_MOVE_ALL);
			return true;

		case MOUSE_WHL_UP:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_UP);
			return true;

		case MOUSE_WHL_DOWN:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_DOWN);
			return true;

		case MOUSE_WHL_UD:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_WHEEL_MOVE_UPDOWN);
			return true;

		case MOUSE_LEFT:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_LEFT);
			return true;

		case MOUSE_RIGHT:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_RIGHT);
			return true;

		case MOUSE_MIDDLE:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_MIDDLE);
			return true;

		case MOUSE_EXTRABTN1:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_4);
			return true;

		case MOUSE_EXTRABTN2:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_5);
			return true;

		case MOUSE_EXTRABTN3:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_6);
			return true;

		case MOUSE_EXTRABTN4:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_7);
			return true;

		case MOUSE_EXTRABTN5:
			formatf(buffer, bufferSize, BUTTON_KEY_FMT, FONT_CHAR_TOKEN_MOUSE_BUTTON_8);
			return true;

		default:
			Assertf(false, "Invalid mouse icon id %d\n", m_IconId);
			return false;
		}
	}
	else if(m_Type == KEYCODE_ID)
	{		
		const CControl::KeyInfo& info = CControl::GetKeyInfo(m_IconId);
		Assertf(info.m_Icon != CControl::KeyInfo::INVALID_ICON || m_IconId == KEY_NULL, "Invalid key icon for keycode %u", m_IconId);

		// In the case of invalid, we will end up showing a key icon with '???'.
		if(info.m_Icon == CControl::KeyInfo::TEXT_ICON)
		{
			formatf(buffer, bufferSize, KEYBOARD_KEY_FMT "%s", info.m_Text);
		}
		// In the case of invalid, we will end up showing a key icon with '???'.
		else if(info.m_Icon == CControl::KeyInfo::LARGE_TEXT_ICON)
		{
			formatf(buffer, bufferSize, KEYBOARD_LARGE_KEY_FMT "%s", info.m_Text);
		}
		else if(info.m_Icon == CControl::KeyInfo::INVALID_ICON)
		{
			safecpy(buffer, UNMAPPED_ICON_TEXT, bufferSize);
		}
		else
		{
			formatf(buffer, bufferSize, KEYBOARD_ICON_FMT "%u", info.m_Icon);
		}

		return true;
	}
	else
#endif // KEYBOARD_MOUSE_SUPPORT
	if(m_IconId != FONT_CHAR_TOKEN_INVALID)
	{
		u32 buttonId;
		if(m_IconId >= FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_NONE)
		{
			buttonId = m_IconId;

#if RSG_ORBIS
			// Some scaleform clips automatically swap the X/O buttons on the playstation. When we do not want this to happen we have to swap then first.
			if(iconParams.m_AllowXOSwap && !CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS))
			{
				if(buttonId == FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A)
				{
					buttonId = FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B;
				}
				else if(buttonId == FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_B)
				{
					buttonId = FONT_CHAR_TOKEN_ID_CONTROLLER_BUTTON_A;
				}
			}
#endif // RSG_ORBIS

			buttonId -= FONT_CHAR_TOKEN_ID_CONTROLLER_UP;
		}
		else
		{
			buttonId = m_IconId;
		}

		formatf( buffer,
			bufferSize,
			BUTTON_KEY_FMT,
			buttonId );

		return true;
	}
	else
	{
		return false;
	}
}
