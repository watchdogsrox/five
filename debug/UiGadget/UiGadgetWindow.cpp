/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetWindow.cpp
// PURPOSE : a draggable window build with ui gadgets
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetWindow.h"

#if __BANK

// TODO - make these member variables which can be tweaked
#define TITLEBAR_OFFSET			(2.0f)
#define TITLEBAR_SHADOW_OFFSET	(1.0f)
#define TITLEBAR_HEIGHT			(12.0f)
#define TITLEBAR_TEXT_OFFSET_X	(1.0f)
#define TITLEBAR_TEXT_OFFSET_Y	(2.0f)

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetWindowTitleBar
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetWindowTitleBar::CUiGadgetWindowTitleBar(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme)
: CUiGadgetBox(fX, fY, fWidth, fHeight, colorScheme.GetColor(CUiColorScheme::COLOR_WINDOW_TITLEBAR_BG))
{
	m_pText = rage_new CUiGadgetText(fX+TITLEBAR_TEXT_OFFSET_X, fY+TITLEBAR_TEXT_OFFSET_Y,
		colorScheme.GetColor(CUiColorScheme::COLOR_WINDOW_TITLEBAR_TEXT), "Title");
	if (m_pText)
	{
		AttachChild(m_pText);
	}

	m_pShadowText = rage_new CUiGadgetText(fX+TITLEBAR_TEXT_OFFSET_X+TITLEBAR_SHADOW_OFFSET, fY+TITLEBAR_TEXT_OFFSET_Y+TITLEBAR_SHADOW_OFFSET,
		Color32(0,0,0), "Title");
	if (m_pShadowText)
	{
		AttachChild(m_pShadowText);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	~CUiGadgetWindowTitleBar
// PURPOSE:		dtor
//////////////////////////////////////////////////////////////////////////
CUiGadgetWindowTitleBar::~CUiGadgetWindowTitleBar()
{
	if (m_pText)
	{
		m_pText->DetachFromParent();
		delete m_pText;
		m_pText = NULL;
	}
	if (m_pShadowText)
	{
		m_pShadowText->DetachFromParent();
		delete m_pShadowText;
		m_pShadowText = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetWindow
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetWindow::CUiGadgetWindow(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme)
: CUiGadgetBox(fX, fY, fWidth, fHeight, colorScheme.GetColor(CUiColorScheme::COLOR_WINDOW_BG)), m_state(STATE_NORMAL)
{
	m_pTitleBar = rage_new CUiGadgetWindowTitleBar(
		fX+TITLEBAR_OFFSET,
		fY+TITLEBAR_OFFSET,
		fWidth-2.0f*TITLEBAR_OFFSET,
		TITLEBAR_HEIGHT,
		colorScheme
	);

	if (m_pTitleBar)
	{
		AttachChild(m_pTitleBar);
	}

	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_PRESSED);
	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_RELEASED);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	~CUiGadgetWindow
// PURPOSE:		dtor
//////////////////////////////////////////////////////////////////////////
CUiGadgetWindow::~CUiGadgetWindow()
{
	if (m_pTitleBar)
	{
		m_pTitleBar->DetachFromParent();
		delete m_pTitleBar;
		m_pTitleBar = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetState
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CUiGadgetWindow::SetState(eWindowState newState)
{
	switch (newState)
	{
	case STATE_BEINGDRAGGED:
		m_vClickDragDelta = g_UiEventMgr.GetCurrentMousePos() - GetPos();
		break;
	case STATE_NORMAL:
		break;
	default:
		break;
	}

	m_state = newState;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates position of window when dragging etc
//////////////////////////////////////////////////////////////////////////
void CUiGadgetWindow::Update()
{
	switch (m_state)
	{
	case STATE_BEINGDRAGGED:
		SetPos(g_UiEventMgr.GetCurrentMousePos() - m_vClickDragDelta, true);
		break;
	case STATE_NORMAL:
		break;
	default:
		break;
	}
}

void CUiGadgetWindow::SetSize( float fWidth, float fHeight )
{
	CUiGadgetBox::SetSize( fWidth, fHeight );

	if( m_pTitleBar )
	{
		m_pTitleBar->SetSize( fWidth, m_pTitleBar->GetHeight() );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	HandleEvent
// PURPOSE:		check for mouse events, used for dragging window etc
//////////////////////////////////////////////////////////////////////////
void CUiGadgetWindow::HandleEvent(const CUiEvent& uiEvent)
{
	switch (m_state)
	{
	case STATE_BEINGDRAGGED:
		if (uiEvent.IsSet(UIEVENT_TYPE_MOUSEL_RELEASED))
		{
			SetState(STATE_NORMAL);
		}
		break;
	case STATE_NORMAL:
		if (uiEvent.IsSet(UIEVENT_TYPE_MOUSEL_PRESSED) && m_pTitleBar->ContainsMouse())
		{
			SetState(STATE_BEINGDRAGGED);
		}
		break;
	default:
		break;
	}
}

#endif	//__BANK
