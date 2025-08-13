#ifndef __SIMPLE_TIMER_H__
#define __SIMPLE_TIMER_H__

#include "fwsys/timer.h"
#include "Frontend/ui_channel.h"


typedef u32 (*TimeFunc)();

template< TimeFunc T>
class CSimpleTimerBase
{
	u32			m_iCurTimeStamp;

public:

	CSimpleTimerBase(){ Zero(); };
	void Zero() { m_iCurTimeStamp = 0; }
	void Start(s32 timeOffset = 0) // time to offset (mostly used to shorten; negative is more time)
	{
		m_iCurTimeStamp = GetTimeFunc() + timeOffset;
	}

	bool IsComplete(s32 targetTime, bool ASSERT_ONLY(bMustBeStarted) = true) const
	{
		uiAssertf(!bMustBeStarted || IsStarted(), "Attempted to query a timer that isn't started. This *COULD* be fine, but it's also fatuous.");
		return GetTimeFunc() >= (m_iCurTimeStamp + targetTime);
	}

	bool IsStarted() const { return m_iCurTimeStamp != 0; }

	u32 GetTime() const { return m_iCurTimeStamp; }
	static u32 GetTimeFunc() { return T(); }

};

// Timer that pauses when in the pause menu
typedef CSimpleTimerBase<fwTimer::GetTimeInMilliseconds_NonScaledClipped>	CSimpleTimer;

// Timer that continues to run while in the pause menu
typedef CSimpleTimerBase<fwTimer::GetSystemTimeInMilliseconds>				CSystemTimer;

#endif // __SIMPLE_TIMER_H__

