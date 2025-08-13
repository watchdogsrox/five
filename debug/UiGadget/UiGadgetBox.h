/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetBox.h
// PURPOSE : a trivial example of using ui gadgets to draw a coloured box
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETBOX_H_
#define _DEBUG_UIGADGET_UIGADGETBOX_H_

#if __BANK

#include "debug/UiGadget/UiGadgetBase.h"
#include "vector/color32.h"

class CUiGadgetBox : public CUiGadgetBase
{
public:
	CUiGadgetBox() {}
	CUiGadgetBox(float fX, float fY, float fWidth, float fHeight, const Color32& color);
	inline void SetColor(const Color32& color)				{ m_color = color; }
	virtual void SetSize(float fWidth, float fHeight)		{ m_fWidth = fWidth; m_fHeight = fHeight; }
	inline float GetWidth() const { return m_fWidth; }
	inline float GetHeight() const { return m_fHeight; }
	inline void SetFaded(bool bFaded) { m_bFaded = bFaded; }

	virtual fwBox2D GetBounds() const;

protected:
    virtual void ImmediateDraw();
	virtual void Draw();
	virtual void Update();
	float m_fWidth, m_fHeight;

private:
	Color32 m_color;
	Color32 m_fadedColor;
	bool m_bFaded;
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETBOX_H_
