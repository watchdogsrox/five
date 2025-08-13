/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiEvent.cpp
// PURPOSE : user input event types and management
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiEvent.h"

#if __BANK
#include "input/mouse.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "system/controlmgr.h"

CUiEventMgr g_UiEventMgr;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates to latest keyboard and mouse state, passing generated events to ui widget tree
//////////////////////////////////////////////////////////////////////////
void CUiEventMgr::Update(CUiGadgetBase* pRootWidget)
{
	//  update mouse state
	m_aMouseStates[INPUT_STATE_PREVIOUS] = m_aMouseStates[INPUT_STATE_CURRENT];

	CUiEventMouseState* pPrevious = &m_aMouseStates[INPUT_STATE_PREVIOUS];
	CUiEventMouseState* pCurrent = &m_aMouseStates[INPUT_STATE_CURRENT];

	pCurrent->SetMouseLeft((ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0);
	pCurrent->SetMouseRight((ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT) != 0);
	pCurrent->SetMouseWheel(ioMouse::GetDZ());
	pCurrent->SetMousePos(Vector2((float)ioMouse::GetX(), (float)ioMouse::GetY()));

	// update timers (e.g. for single click, double click etc as opposed to button up/down)
	for (s32 i=0; i<UIEVENT_TYPE_TOTAL; i++)
	{
		if (m_eventTimer[i] > 0)
		{
			m_eventTimer[i] -= 1;
		}
	}

	// update keyboard state
	m_aKeyboardStates[INPUT_STATE_PREVIOUS] = m_aKeyboardStates[INPUT_STATE_CURRENT];
	CUiEventKeyboardState* pPreviousKey = &m_aKeyboardStates[INPUT_STATE_PREVIOUS];
	CUiEventKeyboardState* pCurrentKey = &m_aKeyboardStates[INPUT_STATE_CURRENT];
	pCurrentKey->SetUp( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG) );
	pCurrentKey->SetDown( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG) );

	// issue events
	if (pRootWidget)
	{	
		u32 eventMask = 0;

		if (!pPrevious->GetMouseLeft() && pCurrent->GetMouseLeft())
		{
			// left mouse pressed event
			m_eventTimer[UIEVENT_TYPE_MOUSEL_PRESSED] = UIEVENT_CLICK_MAX_DURATION; 
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSEL_PRESSED);
		}
		else if (pPrevious->GetMouseLeft() && !pCurrent->GetMouseLeft())
		{
			// left mouse released event
			m_eventTimer[UIEVENT_TYPE_MOUSEL_RELEASED] = UIEVENT_CLICK_MAX_DURATION;
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSEL_RELEASED);
			if (m_eventTimer[UIEVENT_TYPE_MOUSEL_PRESSED] > 0)
			{
				// left mouse click event
				eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSEL_SINGLECLICK);
			}
		}
		if (!pPrevious->GetMouseRight() && pCurrent->GetMouseRight())
		{
			// right mouse pressed event
			m_eventTimer[UIEVENT_TYPE_MOUSER_PRESSED] = UIEVENT_CLICK_MAX_DURATION; 
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSER_PRESSED);
		}
		else if (pPrevious->GetMouseRight() && !pCurrent->GetMouseRight())
		{
			// right mouse released event
			m_eventTimer[UIEVENT_TYPE_MOUSER_RELEASED] = UIEVENT_CLICK_MAX_DURATION;
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSER_RELEASED);
			if (m_eventTimer[UIEVENT_TYPE_MOUSER_PRESSED] > 0)
			{
				// right mouse click event
				eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_MOUSER_SINGLECLICK);
			}
		}

		// cursor keys
		if (!pPreviousKey->GetUp() && pCurrentKey->GetUp())
		{
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_KEYBOARD_UP_PRESSED);
		}
		else if (pPreviousKey->GetUp() && !pCurrentKey->GetUp())
		{
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_KEYBOARD_UP_RELEASED);
		}
		if (!pPreviousKey->GetDown() && pCurrentKey->GetDown())
		{
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_KEYBOARD_DOWN_PRESSED);
		}
		else if (pPreviousKey->GetDown() && !pCurrentKey->GetDown())
		{
			eventMask |= CUiEvent::GetTypeMask(UIEVENT_TYPE_KEYBOARD_DOWN_RELEASED);
		}

		if (eventMask)
		{
			pRootWidget->PropagateEvent(CUiEvent(eventMask));
		}
	}
}

#endif	//__BANK
