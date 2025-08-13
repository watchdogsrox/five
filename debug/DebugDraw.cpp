/////////////////////////////////////////////////////////////////////////////////
// FILE		: DebugDraw.cpp
// PURPOSE	: To provide in game visual helpers for debugging.
// AUTHOR	: G. Speirs, Adam Croston, Alex Hadjadj
// STARTED	: 10/08/99
/////////////////////////////////////////////////////////////////////////////////

#include "DebugDraw.h"

// Rage headers
#include "grcore/font.h"
#include "grcore/quads.h"
#include "fwdebug/debugdraw.h"
#include "system/param.h"

// Game headers
#include "camera/viewports/Viewport.h"
#include "renderer/color.h"
#include "text/text.h"
#include "text/TextConversion.h"
#include "vector/colors.h"
#include "shaders/ShaderLib.h"

PARAM(scaleformdebugfont, "Use Scaleform to render debug text");

namespace rage {
	XPARAM(fixedheightdebugfont);
}

AI_OPTIMISATIONS()

#if DEBUG_DRAW

extern bool gLastGenMode;


struct CGtaDebugTextRenderer : public grcDebugDraw::TextRenderer
{
	CGtaDebugTextRenderer()
		: m_nCharWidth(8)			// Dummy values just to keep things sane if this class is used prematurely
		, m_nPropCharHeight(8)
		, m_nFixedCharHeight(8)
	{
	}

	virtual void Begin();
	virtual void RenderText(Color32, int xPos, int yPos, const char *string, bool proportional, bool drawBGQuad, bool rawCoords);
	virtual void End();
	virtual int GetTextHeight(bool proportional) const;

private:
	CTextLayout m_DebugTextLayout;
	int m_nCharWidth;
	int m_nPropCharHeight;
	int m_nFixedCharHeight;
};


static CGtaDebugTextRenderer s_GtaDebugTextRenderer;

void DebugDraw::Init()
{
	fwDebugDraw::Init();
	grcDebugDraw::Init(CShaderLib::DSS_LessEqual_Invert);

	if (PARAM_scaleformdebugfont.Get()) {
		grcDebugDraw::SetTextRenderer(s_GtaDebugTextRenderer);
	}
}


void DebugDraw::Shutdown()
{
	grcDebugDraw::Shutdown();
}

#if __BANK
float s_lastGenX = 53.0f;
float s_lastGenY = 665.0f;
void DebugDraw::RenderLastGen()
{
	// Print out the text for LastGen mode, make it noticeable
	if (gLastGenMode)
	{
		const Color32	color = Color_red;
		const Color32	halfColor( color.GetRGBA() * ScalarV(V_HALF) );

		const float sx = 1.0f / (float)SCREEN_WIDTH,
				    sy = 1.0f / (float)SCREEN_HEIGHT;

		grcDebugDraw::RectAxisAligned(
				Vec2V((s_lastGenX + 0.0f)* sx, (s_lastGenY + 0.0f) *  sy),
				Vec2V((s_lastGenX + 10.0f) * sx, (s_lastGenY + 10.0f) *  sy),
				halfColor,
				true
			);

		grcDebugDraw::RectAxisAligned(
			Vec2V((s_lastGenX + 1.0f) * sx, (s_lastGenY + 1.0f) *  sy),
			Vec2V((s_lastGenX + 9.0f) * sx, (s_lastGenY + 9.0f) *  sy),
			color,
			true
			);

		grcDebugDraw::RectAxisAligned(
			Vec2V((s_lastGenX + 2.0f) * sx, (s_lastGenY + 2.0f) *  sy),
			Vec2V((s_lastGenX + 5.0f)* sx, (s_lastGenY + 8.0f) *  sy),
			halfColor,
			true
			);

		grcDebugDraw::RectAxisAligned(
			Vec2V((s_lastGenX + 2.0f) * sx, (s_lastGenY + 5.0f) *  sy),
			Vec2V((s_lastGenX + 8.0f) * sx, (s_lastGenY + 8.0f) *  sy),
			halfColor,
			true
			);
	}
}
#endif


void CGtaDebugTextRenderer::Begin()
{
	m_DebugTextLayout.Default();

	Vector2 vTextScale = Vector2(0.17f, 0.27f);
	m_DebugTextLayout.SetScale(&vTextScale);
	// CharWidth & CharHeight (in pixels) - when setting new font scale, 
	// you have to readjust these too (used only for calculating start text position on screen):
	m_nCharWidth = 10;
	m_nPropCharHeight= 16;
	m_nFixedCharHeight= grcFont::GetCurrent().GetHeight() + 1;

	m_DebugTextLayout.SetBackgroundColor(CRGBA(0, 0, 0, 140));
	m_DebugTextLayout.SetBackground(true);
}

void CGtaDebugTextRenderer::RenderText(Color32 color, int xPos, int yPos, const char *string, bool proportional, bool drawBGQuad, bool rawCoords)
{
	char tallestHeight = (char) Max(m_nFixedCharHeight, m_nPropCharHeight);
	if (!proportional)
	{
		s32 charWidth = grcFont::GetCurrent().GetWidth();
		
		if (!rawCoords) {
			xPos *= charWidth;
			yPos *= tallestHeight;
		}

		Vector2 pos = Vector2(float(xPos), float(yPos));

		PUSH_DEFAULT_SCREEN();
		if (drawBGQuad) {
			int barHeight = (PARAM_fixedheightdebugfont.Get()) ? tallestHeight : GetTextHeight(false);
			grcDrawSingleQuadf(pos.x, pos.y, pos.x+(charWidth * strlen(string)), pos.y+barHeight, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Color32(0, 0, 0, 140));
		}
		grcFont::GetCurrent().Drawf(pos.x, pos.y, color, string);
		POP_DEFAULT_SCREEN();
		return;
	}

	m_DebugTextLayout.SetWrap(Vector2(0.0f, 4.0f));

	m_DebugTextLayout.SetColor(color);
	m_DebugTextLayout.Background = drawBGQuad;

	if (!rawCoords) {
		xPos *= m_nCharWidth;
		yPos *= tallestHeight;
	}

	Vector2 pos = Vector2(float(xPos) / float(GRCDEVICE.GetWidth()), 
		float(yPos)/ float(GRCDEVICE.GetHeight()));

	if(pos.y < 1.0f)  // no point in rendering anything that cant be seen on screen
	{
		m_DebugTextLayout.SetWrap(Vector2(pos.x, (pos.x+0.01f) + m_DebugTextLayout.GetStringWidthOnScreen(string, true)));
		m_DebugTextLayout.Render(&pos, string);
	}
}

void CGtaDebugTextRenderer::End()
{
	CText::Flush();
}

int CGtaDebugTextRenderer::GetTextHeight(bool proportional) const
{
	return (proportional) ? m_nPropCharHeight : m_nFixedCharHeight;
}



#endif // DEBUG_DRAW
