// Title:		TouchPadGestures.h
// Author:		Matthew Puthiampadavil
// Description: Classes to analyse input from controller touchpad
// Created:		28/7/2013

#ifndef TouchPadGestures_h__
#define TouchPadGestures_h__

#include "input/pad.h"
#include "vector/color32.h"

#if RSG_ORBIS

class CPad;

#if __BANK
namespace rage{
//	class Color32;
//	class Vector2;
}
#endif

#define INVALID_TOUCH_ID		255

// Class to analyse motion sensor data and translate into game actions
class CTouchPadGesture
{
	struct TouchPoint
	{
		u16		x;
		u16		y;
	};

public:
	enum SwipeDirection
	{
		NONE	= 0,
		LEFT,
		RIGHT,
		UP,
		DOWN
	};

	CTouchPadGesture();
	~CTouchPadGesture()								{ };

	bool			IsSwipe(SwipeDirection dir) const	{ return (m_FinalDirection == dir); }

	void			Update(CPad* pPad);

#if __BANK

	struct TouchPointTime
	{
		TouchPointTime()
			: m_uTime(0)
			, m_uTouchId(INVALID_TOUCH_ID)
			, m_nColor(1.00f,0.00f,0.00f)
		{
		}

		TouchPoint m_Touch;
		u32 m_uTime;
		u8 m_uTouchId;
		Color32 m_nColor;
	};

	struct TrackPadHistory
	{
	public:

		TrackPadHistory()
			: m_nStartIndex(0)
			, m_nEndIndex(0)
		{}

		void AddRecord(const TouchPointTime &touchPoint);
		void Clear();
		void Draw();

		u32 GetCount() const;
		TouchPointTime &GetIndex(u32 i);

		void SetColourForTouchID(u8 uTouchID, const Color32 &nColor);

	private:

		//! use a simple ring/cyclic buffer system to track history.
		static const int nMAX_HISTORY = 200;
		TouchPointTime m_aTouchInputs[nMAX_HISTORY];
		u32 m_nStartIndex;
		u32 m_nEndIndex;
	};

	static void		InitWidgets(bkBank& bank);
	void Debug();

#endif // __BANK

private:

	void			UpdateSingleTouch(CPad* pPad);
	void			UpdateMultiTouch(CPad* pPad);

	TouchPoint		m_Start[rage::ioPad::MAX_TOUCH_NUM];

	SwipeDirection	m_CurrentDirection;
	SwipeDirection	m_FinalDirection;

	u8				m_PreviousTouchId[rage::ioPad::MAX_TOUCH_NUM];
	u8				m_InvalidTouch;
	bool			m_bMultiTouch;
	bool			m_bLockDirection;

#if __BANK
	TouchPoint		m_LastTouch[rage::ioPad::MAX_TOUCH_NUM];
	u32				m_LastTimeTouched;
	TrackPadHistory m_TrackPadHistory;
#endif
};

#endif // RSG_ORBIS

#endif // TouchPadGestures_h__
