/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiEvent.h
// PURPOSE : user input event types and management
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIEVENT_H_
#define _DEBUG_UIGADGET_UIEVENT_H_

#if __BANK

#include "vector/vector2.h"

#define UIEVENT_CLICK_MAX_DURATION (10)

class CUiGadgetBase;

enum eUiEventType
{
	UIEVENT_TYPE_MOUSEL_PRESSED = 0,
	UIEVENT_TYPE_MOUSEL_RELEASED,
	UIEVENT_TYPE_MOUSER_PRESSED,
	UIEVENT_TYPE_MOUSER_RELEASED,
	UIEVENT_TYPE_MOUSEL_SINGLECLICK,
	UIEVENT_TYPE_MOUSER_SINGLECLICK,

	UIEVENT_TYPE_KEYBOARD_UP_PRESSED,
	UIEVENT_TYPE_KEYBOARD_UP_RELEASED,
	UIEVENT_TYPE_KEYBOARD_DOWN_PRESSED,
	UIEVENT_TYPE_KEYBOARD_DOWN_RELEASED,

	UIEVENT_TYPE_TOTAL
};

// represents a set of event masks for ui gadget processing
class CUiEvent
{
public:
	friend class CUiEventMgr;

	CUiEvent(u32 eventFlags) : m_eventTypeFlags(eventFlags) { }
	
	inline bool IsSet(eUiEventType eventType) const { return (m_eventTypeFlags & GetTypeMask(eventType)) != 0; }
	inline bool IsInteresting(u32 ofInterestMask) const { return (m_eventTypeFlags & ofInterestMask) != 0; }
	static inline u32 GetTypeMask(eUiEventType eventType) { return (1 << eventType); }
	
private:
	u32 m_eventTypeFlags;
};

// state of mouse buttons etc
class CUiEventMouseState
{
public:
	inline bool GetMouseLeft() const { return m_bMouseLeft; }
	inline bool GetMouseRight() const { return m_bMouseRight; }
	inline s32 GetMouseWheel() const { return m_mouseWheel; }
	inline Vector2 GetMousePos() const { return m_vMousePos; }

	inline void SetMouseLeft(bool bPressed) { m_bMouseLeft = bPressed; }
	inline void SetMouseRight(bool bPressed) { m_bMouseRight = bPressed; }
	inline void SetMouseWheel(s32 wheelZ) { m_mouseWheel = wheelZ; }
	inline void SetMousePos(const Vector2& vPos) { m_vMousePos = vPos; }

private:
	bool	m_bMouseLeft;
	bool	m_bMouseRight;
	s32		m_mouseWheel;
	Vector2	m_vMousePos;

};

// state of cursor keys etc
class CUiEventKeyboardState
{
public:
	inline bool GetUp() const { return m_bUp; }
	inline bool GetDown() const { return m_bDown; }

	inline void SetUp(bool bUp) { m_bUp = bUp; }
	inline void SetDown(bool bDown) { m_bDown = bDown; }

private:
	bool m_bUp;
	bool m_bDown;
};

// responsible for detecting state changes and issuing events
class CUiEventMgr
{
public:
	void Update(CUiGadgetBase* pRootWidget);
	inline Vector2 GetCurrentMousePos() const { return m_aMouseStates[INPUT_STATE_CURRENT].GetMousePos(); }
	inline s32 GetCurrentMouseWheel() const { return m_aMouseStates[INPUT_STATE_CURRENT].GetMouseWheel(); }
	inline bool GetCurrentKeyUp() const { return m_aKeyboardStates[INPUT_STATE_CURRENT].GetUp(); }
	inline bool GetCurrentKeyDown() const { return m_aKeyboardStates[INPUT_STATE_CURRENT].GetDown(); }

private:
	enum
	{
		INPUT_STATE_PREVIOUS = 0,
		INPUT_STATE_CURRENT,
		INPUT_STATE_TOTAL
	};

	CUiEventMouseState m_aMouseStates[INPUT_STATE_TOTAL];
	CUiEventKeyboardState m_aKeyboardStates[INPUT_STATE_TOTAL];
	u32 m_eventTimer[UIEVENT_TYPE_TOTAL];
};

extern CUiEventMgr g_UiEventMgr;

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIEVENT_H_
