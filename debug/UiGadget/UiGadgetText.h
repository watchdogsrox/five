/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetText.h
// PURPOSE : a simple text field
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETTEXT_H_
#define _DEBUG_UIGADGET_UIGADGETTEXT_H_

#if __BANK

#include "debug/UiGadget/UiGadgetBase.h"
#include "atl/string.h"
#include "vector/color32.h"

namespace rage {

class grcFont;

} // namespace rage

class CUiGadgetText : public CUiGadgetBase
{
public:
	CUiGadgetText() : m_fScale(1.0f), m_color(Color32(0,0,0)) {}
	CUiGadgetText(float fX, float fY, const Color32& color, const char* pszText);
	inline void SetString(const char* pszTitle)				{ pszTitle ? m_string.Set(pszTitle, istrlen(pszTitle), 0, -1) : m_string.Clear(); }
	inline void SetColor(const Color32& color)				{ m_color = color; }
	inline void SetScale(float fScale)						{ m_fScale = fScale; }
	float GetWidth() const;
	float GetHeight() const;

	// Disabling these font functions for now as they aren't used or implemented fully...
	//static s32 LoadFont(const char* pszPath, int charWidth, int charHeight); // returns -1 on failure
	//static const grcFont* GetFont(s32 index);
	//static const grcFont* LoadNewFont(const char* pszPath, int charWidth, int charHeight) { return GetFont(LoadFont(pszPath, charWidth, charHeight)); }
	virtual fwBox2D GetBounds() const;

protected:
    virtual void ImmediateDraw();
	virtual void Draw();
	virtual void Update();
	
	grcFont* GetActiveFont();
	grcFont const * GetActiveFont() const;

private:
	float m_fScale;
	Color32 m_color;
	atString m_string;

	//static atArray<const grcFont*> ms_apFonts;
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETTEXT_H_
