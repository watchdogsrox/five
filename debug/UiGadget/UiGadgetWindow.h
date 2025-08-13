/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetWindow.h
// PURPOSE : a draggable window build with ui gadgets
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETWINDOW_H_
#define _DEBUG_UIGADGET_UIGADGETWINDOW_H_

#if __BANK

#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetText.h"
#include "vector/color32.h"

// a box with some text, acts as the title bar for a window
class CUiGadgetWindowTitleBar : public CUiGadgetBox
{
public:
	CUiGadgetWindowTitleBar(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme);
	virtual ~CUiGadgetWindowTitleBar();
	void SetTitle(const char* pszTitle) { m_pText->SetString(pszTitle); m_pShadowText->SetString(pszTitle); }

private:
	CUiGadgetText* m_pText;
	CUiGadgetText* m_pShadowText;
};

// a window consisting of a box and a title bar
class CUiGadgetWindow : public CUiGadgetBox
{
public:
	CUiGadgetWindow(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme);
	virtual ~CUiGadgetWindow();
	virtual void HandleEvent(const CUiEvent& uiEvent);
	virtual void Update();
	void SetTitle(const char* pszTitle) { m_pTitleBar->SetTitle(pszTitle); }
	virtual void SetSize(float fWidth, float fHeight);
private:

	enum eWindowState
	{
		STATE_NORMAL,
		STATE_BEINGDRAGGED		
	};

	void SetState(eWindowState newState);
	CUiGadgetWindowTitleBar* m_pTitleBar;
	eWindowState m_state;
	Vector2 m_vClickDragDelta;
};

#endif	//__BANK

#endif	//_DEBUG_DEBUGDRAW_UIGADGETWINDOW_H_
