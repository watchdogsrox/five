/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MovieClipText.h
// PURPOSE : manages drawing text with textures
// AUTHOR  : James Chagaris
// STARTED : 13/07/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __MOVIE_CLIP_TEXT_H__
#define __MOVIE_CLIP_TEXT_H__

//game
#include "Frontend/hud_colour.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Text/TextFormat.h"

#define DEFAULT_COLOR (HUD_COLOUR_WHITE)
#define MAX_ICON_LENGTH 4
#define BLANK_NBSP_CHARACTER ("\xC2\xB5") // mu/micro symbol, which on OTHER fonts has been edited to be empty to use for french punctuationµ!

class CTextLayout;

class CMovieClipText
{
public:
	static bool SetFormattedTextWithIcons(const GFxValue* args, bool bExplicitSlot = false);

#if __BANK
	static bool ms_bDontUseIcons;
#endif // __BANK
private:
	struct ImageInfo
	{
		GFxValue m_movieClip;
		atString m_name;
		float m_x, m_y;
		eHUD_COLOURS m_color;

		ImageInfo(): m_x(0), m_y(0), m_name(""), m_color(DEFAULT_COLOR) {}
	};

	struct ParserResults
	{
		char m_convertedTagNameBuffer[MAX_TOKEN_LEN];
		int m_iInputIDs[MAX_ICON_LENGTH];
		char m_Prefix[MAX_ICON_LENGTH];
		int m_iNumOfRequiredSpaces[MAX_ICON_LENGTH];
		bool m_bBlip;
		eHUD_COLOURS m_color;

		void Reset()
		{
			m_color = DEFAULT_COLOR;
			memset(m_convertedTagNameBuffer, 0, MAX_TOKEN_LEN);
			m_bBlip = true;

			for(int i = 0; i < MAX_ICON_LENGTH; ++i)
			{
				m_iInputIDs[i] = 0;
				m_Prefix[i] = '\0';
				m_iNumOfRequiredSpaces[i] = 1;
			}
		}

		ParserResults(): m_color(DEFAULT_COLOR)
		{
			m_color = DEFAULT_COLOR;
			memset(m_convertedTagNameBuffer, 0, MAX_TOKEN_LEN);
			m_bBlip = true;

			for(int i = 0; i < MAX_ICON_LENGTH; ++i)
			{
				m_iInputIDs[i] = 0;
				m_Prefix[i] = '\0';
				m_iNumOfRequiredSpaces[i] = 1;
			}
		}
	};

	struct stBlipInfo
	{
		int iCharacterPosition;
		int iGroupIndex;
		float fGroupLeftBound;
		float fGroupInterCharacterWidth;
		ParserResults parserResult;
		atString tagName;
		GFxValue movieClip;
	};

	static bool ConvertTag(const char* pTag, ParserResults& parseResults, u8 slot = 0, bool bAllowFallback = true);
	static float GetWidthOnScreen(const CTextLayout& layout, const char* pText, bool forBlip);
	static int GetStartOfLastWord(const char* pText);
	static int GetStartOfNextWord(const char* pText);
	static int GetNextNewLineBeforeBlip(const char* pText, const int blipIndex);

	static void ConvertToHTMLImageString(char* rawImgString, int iInputID, bool bTextButton, bool bWideTextButton);
	static void FillPCButtonInformation(ParserResults& parseResults);

	static void SetMovieColor(GFxValue& movie, eHUD_COLOURS color);

private:
	static eHUD_COLOURS m_CurrentColor;
};

#endif // #ifndef __MOVIE_CLIP_TEXT_H__

// eof
