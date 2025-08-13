/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiColorScheme.cpp
// PURPOSE : represents a color scheme for user interface widgets
// AUTHOR :  Ian Kiigan
// CREATED : 25/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiColorScheme.h"

#if __BANK

// default color scheme
Color32 CUiColorScheme::m_aDefaultColors[CUiColorScheme::COLOR_TOTAL] =
{
	Color32(255, 255, 255),					// COLOR_WINDOW_BG
	Color32(80, 80, 120),					// COLOR_WINDOW_TITLEBAR_BG
	Color32(255, 255, 255),					// COLOR_WINDOW_TITLEBAR_TEXT
	Color32(150, 150, 150),					// COLOR_LIST_BG1
	Color32(140, 140, 140),					// COLOR_LIST_BG2
	Color32(0, 255, 255),					// COLOR_LIST_SELECTOR
	Color32(0, 0, 0),						// COLOR_LIST_ENTRY_TEXT
	Color32(140, 140, 140),					// COLOR_SCROLLBAR_BG
	Color32(0, 0, 0)						// COLOR_SCROLLBAR_FG
};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetAllColors
// PURPOSE:		sets all colors in scheme from an array
//////////////////////////////////////////////////////////////////////////
void CUiColorScheme::SetAllColors(Color32* pColors)
{
	for (u32 i=0; i<COLOR_TOTAL; i++)
	{
		m_aColors[i] = pColors[i];
	}
}

#endif	//__BANK