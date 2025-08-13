/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetText.cpp
// PURPOSE : a simple text field
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetText.h"

#if __BANK

#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "grcore/image.h"
#include "grcore/setup.h"

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetText
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetText::CUiGadgetText(float fX, float fY, const Color32& color, const char* pszText) : m_color(color), m_fScale(1.0f)
{
	SetPos(Vector2(fX, fY));
	SetString(pszText);
}

void CUiGadgetText::ImmediateDraw()
{
    float const c_x = GetPos().x;
    float const c_y = GetPos().y;

    grcColor(m_color);
    grcDraw2dText( c_x, c_y, m_string.c_str(), false, m_fScale, m_fScale, GetActiveFont());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CUiGadgetText::Draw()
{
	grcDebugDraw::TextFontPush( GetActiveFont() );
	grcDebugDraw::Text(GetPos(), DD_ePCS_Pixels, m_color, m_string.c_str(), false, m_fScale, m_fScale);
	grcDebugDraw::TextFontPop();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CUiGadgetText::Update()
{

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetBounds
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
fwBox2D CUiGadgetText::GetBounds() const
{
	Vector2 vPos = GetPos();
 	const char* pszText = m_string.c_str();
	float fWidth = 0.0f;
	float fHeight = 0.0f;
	const grcFont* pFont = GetActiveFont();
	if (pFont)
	{
		fWidth = (float) pFont->GetStringWidth(pszText, istrlen(pszText));
		fHeight = (float) pFont->GetHeight();
	}
	return fwBox2D(vPos.x, vPos.y, vPos.x+fWidth, vPos.y+fHeight);
}

float CUiGadgetText::GetWidth() const
{
	const char* pszText = m_string.c_str();
	float width = 0.0f;
	const grcFont* pFont = GetActiveFont();
	if( pFont && pszText )
	{
		width = (float) pFont->GetStringWidth(pszText, istrlen(pszText));
	}

	return width * m_fScale;
}

float CUiGadgetText::GetHeight() const
{
	const char* pszText = m_string.c_str();
	float height = 0.0f;
	const grcFont* pFont = GetActiveFont();
	if( pFont && pszText )
	{
		height = (float) pFont->GetHeight();
	}

	return height * m_fScale;
}

/*s32 CUiGadgetText::LoadFont(const char* pszPath, int charWidth, int charHeight)
{
	grcImage* image = grcImage::Load(pszPath);
	s32 fontIndex = -1;

	if (image)
	{
		grcFont* font = grcFont::CreateFontFromImage(image, charWidth, charHeight, 1, 127);

		if (font)
		{
			ms_apFonts.PushAndGrow(font);
			fontIndex = ms_apFonts.size() - 1;
		}

		image->Release();
	}

	return fontIndex;
}

const grcFont* CUiGadgetText::GetFont(s32 index)
{
	if (index >= 0 && index < ms_apFonts.size())
	{
		return ms_apFonts[index];
	}

	return NULL;
}

atArray<const grcFont*> CUiGadgetText::ms_apFonts;*/

grcFont* CUiGadgetText::GetActiveFont()
{
	return grcSetup::GetMiniFixedWidthFont();
}

grcFont const * CUiGadgetText::GetActiveFont() const
{
	return grcSetup::GetMiniFixedWidthFont();
}

#endif	//__BANK

