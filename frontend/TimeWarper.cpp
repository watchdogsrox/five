#include "TimeWarper.h"
#include "fwsys/timer.h"

#if __BANK
#include "bank/group.h"
#endif

//OPTIMISATIONS_OFF()

static BankFloat SLOWED_TIME_WARP = 0.1f;

//////////////////////////////////////////////////////////////////////////
void CTimeWarper::Reset(TW_Dir whichDir)
{
	float fTargetWarp = (whichDir==TW_Normal) ? NORMAL_TIME_WARP : SLOWED_TIME_WARP;

	if( WarpTime(fTargetWarp) )
	{
		m_TargetReachedCB.Call();
	}
	SetCallback(NullCB);
}

bool CTimeWarper::WarpTime(float fNewTime)
{
	if( m_fLastTimeWarp != fNewTime )
	{
		fwTimer::SetTimeWarpUI(fNewTime);
		m_fLastTimeWarp = fNewTime;
		return false;
	}
	return true;
}

void CTimeWarper::SetTargetTimeWarp(TW_Dir whichDir)
{
	float fTargetWarp = (whichDir==TW_Normal) ? NORMAL_TIME_WARP : SLOWED_TIME_WARP;

	if( m_fLastTimeWarp != fTargetWarp )
	{
		u32 lastFrameTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped() - fwTimer::GetPrevElapsedTimeInMilliseconds_NonScaledClipped();
		float fTimeRate = ((NORMAL_TIME_WARP - SLOWED_TIME_WARP) / m_fLerpTimeMS) * float(lastFrameTime);

		float fNewTime = m_fLastTimeWarp + Selectf(fTargetWarp-NORMAL_TIME_WARP, fTimeRate,-fTimeRate);
		fNewTime = Clamp(fNewTime, SLOWED_TIME_WARP, NORMAL_TIME_WARP);

		if( WarpTime(fNewTime) )
		{
			m_TargetReachedCB.Call();
			SetCallback(NullCB);
		}
	}
	else
	{
		m_TargetReachedCB.Call();
		SetCallback(NullCB);
	}
}

#if __BANK
void CTimeWarper::AddWidgets(bkGroup* pOwningGroup)
{
	pOwningGroup->AddTitle("Time Warp");
	pOwningGroup->AddSlider("Last Set", &m_fLastTimeWarp, 0.0f, 1.0f, 0.1f);
	pOwningGroup->AddSlider("TUNE: Lerp Time", &m_fLerpTimeMS, 0, 10000, 100);
	pOwningGroup->AddSlider("TUNE: Slow Warp", &SLOWED_TIME_WARP, 0.0f, 1.0f, 0.1f);
}
#endif
