/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MousePointer.h
// PURPOSE : displays the global game mouse pointer, interacts with scaleform
//			 and has some script support
// DATE    : 14/03/14
// AUTHOR  : Derek Payne
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef MOUSEPOINTER_H
#define MOUSEPOINTER_H

#if RSG_PC

#include "input/mouse.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

enum eMOUSE_CURSOR_STYLE  // this enum matches the frames on the Actionscript side
{
	MOUSE_CURSOR_STYLE_ARROW = 1,
	MOUSE_CURSOR_STYLE_ARROW_DIMMED,
	MOUSE_CURSOR_STYLE_HAND_OPEN,
	MOUSE_CURSOR_STYLE_HAND_GRAB,
	MOUSE_CURSOR_STYLE_HAND_MIDDLE_FINGER,
	MOUSE_CURSOR_STYLE_ARROW_LEFT,
	MOUSE_CURSOR_STYLE_ARROW_RIGHT,
	MOUSE_CURSOR_STYLE_ARROW_UP,
	MOUSE_CURSOR_STYLE_ARROW_DOWN,
	MOUSE_CURSOR_STYLE_ARROW_TRIMMING,
	MOUSE_CURSOR_STYLE_ARROW_PLUS,
	MOUSE_CURSOR_STYLE_ARROW_MINUS,

	MAX_MOUSE_CURSOR_STYLES
};

enum eMOUSE_CURSOR_STATE
{
	MOUSE_CURSOR_STATE_INIT = 0,
	MOUSE_CURSOR_STATE_UPDATE_CURSOR_STYLE,
	MOUSE_CURSOR_STATE_UPDATE_VISIBLILITY,
	MAX_MOUSE_CURSOR_STATES
};

enum // must match AS enum
{
	UI_MOVIE_VIDEO_EDITOR = 0,
	UI_MOVIE_ONLINE_POLICY,
	UI_MOVIE_INSTRUCTIONAL_BUTTONS,
	UI_MOVIE_REPORT_MENU,
    UI_MOVIE_GENERIC,
    UI_MOVIE_LANDING_MENU
};

enum eMOUSE_EVT // must match AS and Script enum
{
	EVENT_TYPE_MOUSE_NONE = -1,

	EVENT_TYPE_MOUSE_DRAG_OUT = 0,
	EVENT_TYPE_MOUSE_DRAG_OVER,
	EVENT_TYPE_MOUSE_DOWN,
	EVENT_TYPE_MOUSE_MOVE,
	EVENT_TYPE_MOUSE_UP,
	EVENT_TYPE_MOUSE_PRESS,
	EVENT_TYPE_MOUSE_RELEASE,
	EVENT_TYPE_MOUSE_RELEASE_OUTSIDE,
	EVENT_TYPE_MOUSE_ROLL_OUT,
	EVENT_TYPE_MOUSE_ROLL_OVER,
	EVENT_TYPE_MOUSE_WHEEL_UP,
	EVENT_TYPE_MOUSE_WHEEL_DOWN,
	EVENT_TYPE_MOUSE_MAX
};

enum
{
	MOUSE_NONE = -1,
	MOUSE_ROLLOVER = 0,
	MOUSE_ACCEPT,
	MOUSE_BACK,
	MOUSE_WHEEL_UP,
	MOUSE_WHEEL_DOWN,
	MOUSE_WHEEL_PRESSED
};

enum
{
	MOUSE_NOT_MOVED_THIS_FRAME = 0,
	MOUSE_MOVED_LAST_FRAME,
	MOUSE_MOVED_THIS_FRAME
};

struct sMousePointerStateStruct
{
	atFixedBitSet<MAX_MOUSE_CURSOR_STATES, u32> m_stateFlag;
	eMOUSE_CURSOR_STYLE m_cursorStyle;

	bool m_bActiveThisFrame;
	bool m_bActiveLastFrame;

	bool m_bVisible;
};

struct sScaleformMouseEvent
{
	s32 iMovieID;
	eMOUSE_EVT evtType;
	int iUID;
	int iContext;

	void Clear()
	{
		iMovieID = SF_INVALID_MOVIE;
		evtType = EVENT_TYPE_MOUSE_NONE;
		iUID = -1;
		iContext = -1;
	}
};

class CMousePointer
{
public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void UpdateInput();

	static void Render();

	static void DeviceReset();
	static void DeviceLost() {};

#if __BANK
	static void InitWidgets();
#endif // __BANK

	static void SetMouseCursorThisFrame();
	static void SetMouseCursorStyle(const eMOUSE_CURSOR_STYLE iNewStyle);
	static void SetMouseCursorVisible(const bool bIsVisible);

	static void CheckIncomingFunctions(const atHashWithStringBank methodName, const GFxValue* args, s32 iMovieCalledFrom);

	static void ResetMouseInput();

	static void SetMouseAccept() { ms_iMouseEvent =  MOUSE_ACCEPT; }
	static void SetMouseBack() { ms_iMouseEvent =  MOUSE_BACK; }

	static bool IsMouseAccept() { return ms_iMouseEvent == MOUSE_ACCEPT; }
	static bool IsMouseBack() { return ms_iMouseEvent == MOUSE_BACK; }
	static bool IsMouseWheelUp() { return ms_iMouseEvent == MOUSE_WHEEL_UP; }
	static bool IsMouseWheelDown() { return ms_iMouseEvent == MOUSE_WHEEL_DOWN; }
	static bool IsMouseWheelPressed() { return ms_iMouseEvent == MOUSE_WHEEL_PRESSED; }

	static void ConsumeInput() { ms_iMouseEvent = MOUSE_NONE; }

	static bool HasMouseInputOccurred() { return ms_iMouseEvent != MOUSE_NONE || ms_bMouseMoved; }
	static void SetKeyPressed() { ms_bMouseMoved = ms_iMouseEvent != MOUSE_NONE; }  // key has been pressed, so remove mouse cursor if mouse input hasn't occurred this frame
	static bool IsMouseRolledOverInstructionalButtons() { return ms_bMouseRolledOverInstructionalButtons; }

	// Scaleform mouse events (with extra data)
	static const sScaleformMouseEvent GetLastMouseEvent() { return ms_sfMouseEvent; }
	static bool HasSFMouseEvent(s32 movieIndex) { return ms_sfMouseEvent.iMovieID == movieIndex; }

	static bool IsSFMouseReleased(s32 movieIndex, s32 uID);
	static bool IsSFMouseRollOver(s32 movieIndex, s32 uID);
	static bool IsSFMouseRollOut(s32 movieIndex, s32 uID);
	static bool IsSFMouseWheelUp(s32 movieIndex);
	static bool IsSFMouseWheelDown(s32 movieIndex);

private:
	static s32 ms_iMovieId;

	static sMousePointerStateStruct ms_State;

	static CComplexObject ms_MousePointerRoot;
	static CComplexObject ms_MousePointerObject;

	static s32 ms_iMouseEvent;
	static bool ms_bMouseMoved;
	static bool ms_bMouseRolledOverInstructionalButtons;
	static bool ms_bFirstActiveUpdate;

	static sScaleformMouseEvent ms_sfMouseEvent;

	static void AutomaticallyTurnOn();
	static void UpdateObjects();

#if __BANK
	static void ShutdownWidgets();
	static void DebugCreateMousePointerBankWidgets();
	static void DebugSetMouseCursorStyle();
	static void DebugSetMouseCursorVisible();
	static void DebugDeviceReset();

	static bool ms_bBankMousePointerWidgetsCreated;
	static s32 ms_iWidgetCursorStyle;
	static bool ms_bWidgetCursorVisible;
	static bool ms_bWidgetMousePointerOnThisFrame;
#endif // __BANK

};

#else
class CMousePointer  // exists for other platforms so checks here dont have to be wrapped in PC only, always return false
{
public:
	static bool IsMouseAccept() { return false; }
	static bool IsMouseBack() { return false; }
	static bool IsMouseWheelUp() { return false; }
	static bool IsMouseWheelDown() { return false; }
	static bool IsMouseWheelPressed() { return false; }
	static bool HasMouseInputOccurred() { return false; }

	static bool IsSFMouseReleased(s32 UNUSED_PARAM(movieIndex), s32 UNUSED_PARAM(uID)) { return false; }
	static bool IsSFMouseRollOver(s32 UNUSED_PARAM(movieIndex), s32 UNUSED_PARAM(uID)) { return false; }
	static bool IsSFMouseRollOut(s32 UNUSED_PARAM(movieIndex), s32 UNUSED_PARAM(uID)) { return false; }
	static bool IsSFMouseWheelUp(s32 UNUSED_PARAM(movieIndex)) { return false; }
	static bool IsSFMouseWheelDown(s32 UNUSED_PARAM(movieIndex)) { return false; }
};

#endif  // RSG_PC

#endif // MOUSEPOINTER_H

// eof
