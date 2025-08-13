#if 0
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextDraw.h
// PURPOSE : manages the rendering of the text
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef TEXTDRAW_H
#define TEXTDRAW_H

#include "renderer/color.h"
#include "scene/stores/txdstore.h"
#include "system/control.h"
#include "text/text.h"

class CSprite2d;

namespace rage {
	class fwRect;
}


#define TEXTDRAW_BUFFER_SIZE	1024

struct CFontRenderState
{
	Vector2 pos;
	Vector2 Scale;
	float ButtonScale;
	Color32 Colour;
	float PixelsToAdd;
	s32 Orientation;
	bool Shadow;
	u8 ExtraFont;
	bool Proportional;
	bool TextColours;
	bool UnderScore;
	u8 Style;
	float EdgeAmount;
	s32 ControllerButtonSymbol;
};



class CTextDraw
{
private:

	static float GetCharacterTexelWidth(GxtChar Character);
	static float GetRenderedCharacterWidth ( GxtChar Character, bool bUseSpacing = true);
	static void DrawSprite(const fwRect& screen, const fwRect& texture, Color32 col);
	static void PrintChar( float x, float y, GxtChar Character);
	static float GetRenderedCharacterHeight();





public:
	static GxtChar fontBuffer[TEXTDRAW_BUFFER_SIZE] ;
	static GxtChar *pFontBufferPosn;

	static void FlushFontBuffer();

	static void AddToRenderBuffer (CTextLayout *pFormatDetails, float x, float y, GxtChar *pCharacterStart, GxtChar *pCharacterEnd);
	static void RenderFontBuffer();
	static void RenderFontBuffer(GxtChar* pStart, GxtChar* pEnd);

	static void RenderBackground(fwRect TextBox, bool bOutline, Color32 color);

};




#endif // TEXTDRAW_H

// eof


#endif
