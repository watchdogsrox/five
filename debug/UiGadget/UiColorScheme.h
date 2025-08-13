/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiColorScheme.h
// PURPOSE : represents a color scheme for user interface widgets
// AUTHOR :  Ian Kiigan
// CREATED : 25/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UICOLORSCHEME_H_
#define _DEBUG_UIGADGET_UICOLORSCHEME_H_

#if __BANK
#include "vector/color32.h"

// color scheme for ui widgets
class CUiColorScheme
{
public:
	enum eColorType
	{
		COLOR_WINDOW_BG = 0,
		COLOR_WINDOW_TITLEBAR_BG,
		COLOR_WINDOW_TITLEBAR_TEXT,
		COLOR_LIST_BG1,
		COLOR_LIST_BG2,
		COLOR_LIST_SELECTOR,
		COLOR_LIST_ENTRY_TEXT,
		COLOR_SCROLLBAR_BG,
		COLOR_SCROLLBAR_FG,

		COLOR_TOTAL
	};

	CUiColorScheme() { SetAllColors(m_aDefaultColors); }
	inline void SetColor(eColorType eType, const Color32& color) { m_aColors[eType] = color; }
	void SetAllColors(Color32* pColors);
	Color32 GetColor(eColorType eType) const { return m_aColors[eType]; }

private:
	Color32 m_aColors[COLOR_TOTAL];
	static Color32 m_aDefaultColors[COLOR_TOTAL];
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UICOLORSCHEME_H_