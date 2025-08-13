#ifndef __TIMEWARPER_H__
#define __TIMEWARPER_H__

#include "data/callback.h"

#define NORMAL_TIME_WARP 1.0f
#define DEFAULT_TIMEWARP_LERP 250.0f

#if __BANK
namespace rage
{
	class bkGroup;
};
#endif

enum TW_Dir {
	TW_Normal,
	TW_Slow
};


class CTimeWarper
{
public:
	CTimeWarper(float fLerpTimeMS = DEFAULT_TIMEWARP_LERP) 
		: m_fLastTimeWarp(NORMAL_TIME_WARP)
		, m_fLerpTimeMS(fLerpTimeMS)
	{
	}

	void SetCallback( datCallback& callThis ) { m_TargetReachedCB = callThis; }
	void SetTargetTimeWarp(TW_Dir whichDir);
	void Reset(TW_Dir whichDir = TW_Normal);

#if __BANK
	void AddWidgets(bkGroup* pOwningGroup);
#endif

private:
	bool WarpTime(float fNewTime);

	float m_fLastTimeWarp;
	float m_fLerpTimeMS;
	datCallback	m_TargetReachedCB;
};


#endif // __TIMEWARPER_H__

