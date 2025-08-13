/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetBox.cpp
// PURPOSE : a trivial example of using ui gadgets to draw a coloured box
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetBox.h"

#if __BANK

#include "grcore/debugdraw.h"


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetBox
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetBox::CUiGadgetBox(float fX, float fY, float fWidth, float fHeight, const Color32& color)
: m_fWidth(fWidth), m_fHeight(fHeight), m_color(color), m_fadedColor(color), m_bFaded(false)
{
	m_fadedColor.SetAlpha(color.GetAlpha()/2);
	SetPos(Vector2(fX, fY));
}

void CUiGadgetBox::ImmediateDraw()
{
    fwBox2D rect = GetBounds();
    Color32 const c_col(m_bFaded ? m_fadedColor : m_color);
    grcBindTexture(NULL);
    grcDrawSingleQuadf( rect.x0, rect.y0, rect.x1, rect.y1, 0.0f, 0.f, 0.f, 0.f, 0.f, c_col );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:		draw a coloured rectangle onscreen
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBox::Draw()
{
	fwBox2D rect = GetBounds();
	grcDebugDraw::RectAxisAligned(
		UIGADGET_SCREEN_COORDS(rect.x0, rect.y0),
		UIGADGET_SCREEN_COORDS(rect.x1, rect.y1),
		(m_bFaded ? m_fadedColor : m_color),
		true
	);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBox::Update()
{

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetBounds
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
fwBox2D CUiGadgetBox::GetBounds() const
{
	Vector2 vPos = GetPos();
	return fwBox2D(vPos.x, vPos.y, vPos.x+m_fWidth, vPos.y+m_fHeight);
}

#endif	//__BANK
