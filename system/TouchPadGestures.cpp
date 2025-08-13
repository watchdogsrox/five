#if RSG_ORBIS


// Rage headers
#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "bank/group.h"
#endif

// Framework headers
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
//#include "fwmaths/Maths.h"

// Game headers
#include "System/TouchPadGestures.h"
#include "System/pad.h"
#include "frontend/PauseMenu.h"
#include "system/controlmgr.h"
#include "peds/Ped.h"
#include "scene/world/gameWorld.h"
#include "system/controlmgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "frontend/LoadingScreens.h"

bank_s32 s_MinSwipeDistance = 200;
bank_s32 s_MinChangeSwipeDistance = 120;
bank_s32 s_MaxChangeSwipeDistance = 500;
bank_bool s_AllowDirectionChange = true;

#if __BANK
	bank_bool s_DebugRender = false;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////

CTouchPadGesture::CTouchPadGesture() :
  m_CurrentDirection(NONE)
, m_FinalDirection(NONE)
, m_InvalidTouch(INVALID_TOUCH_ID)
, m_bMultiTouch(false)
, m_bLockDirection(false)
{
	for (int i = 0; i < rage::ioPad::MAX_TOUCH_NUM; i++)
	{
		m_PreviousTouchId[i] = INVALID_TOUCH_ID;
	
#if __BANK
		m_LastTouch[i].x = 0;
		m_LastTouch[i].y = 0;
#endif
	}

#if __BANK
	m_LastTimeTouched = 0;
#endif
}

static bank_s32 s_TimeToRender = 2000; 

void CTouchPadGesture::Update( CPad* pPad )
{
	if (pPad && pPad->IsPs4Pad())
	{
		if (pPad->GetNumTouches() == 0)
		{
			// Terminate gesture and set state.
			m_bMultiTouch = false;
			for (int i = 0; i < rage::ioPad::MAX_TOUCH_NUM; i++)
			{
				m_PreviousTouchId[i] = INVALID_TOUCH_ID;
			}

			m_FinalDirection = m_CurrentDirection;
			#if __DEV && 0
				if (m_FinalDirection != NONE)
				{
					Warningf("### TouchPad (released) swipe %s", GetGestureString(m_FinalDirection));
				}
			#endif
			m_InvalidTouch = INVALID_TOUCH_ID;
			m_CurrentDirection = NONE;
			
#if __BANK
			for(int i = 0; i < rage::ioPad::MAX_TOUCH_NUM; ++i)
			{
				m_LastTouch[i].x = 0;
				m_LastTouch[i].y = 0;
			}
#endif
			m_bLockDirection = false;
		}
		else if (!m_bMultiTouch && pPad->GetNumTouches() == 1)
		{
			// Handle single touch gestures.
			UpdateSingleTouch(pPad);
		}
		else if (m_bMultiTouch || pPad->GetNumTouches() == 2)
		{
			// As soon as we have two touch points, we are dealing with a multi-touch gesture.
			m_bMultiTouch = true;
			UpdateMultiTouch(pPad);
		}
	
#if __BANK
		bool bDebugRender = s_DebugRender;
#if DEBUG_PAD_INPUT
		if (CControlMgr::IsDebugPadValuesOn() && 
			(m_LastTimeTouched > 0) &&  
			(fwTimer::GetTimeInMilliseconds() < (m_LastTimeTouched + (u32)s_TimeToRender)))
		{
			bDebugRender=true;
		}
#endif // DEBUG_PAD_INPUT

		if(bDebugRender && (CControlMgr::GetPlayerPad() == pPad)) //only render for player pad.
		{
			Debug();
		}
		else
		{
			m_TrackPadHistory.Clear();
		}
#endif // __BANK
	}
}

void CTouchPadGesture::UpdateSingleTouch( CPad* pPad )
{
	// ===   Detect swipe direction   ===

	const u8 uTouchId = pPad->GetTouchId(0);

#if __BANK
	pPad->GetTouchPoint(0, m_LastTouch[0].x, m_LastTouch[0].y);
	
	//! Add new debug record.
	TouchPointTime newRecord;
	newRecord.m_Touch = m_LastTouch[0];
	newRecord.m_uTouchId = uTouchId;
	newRecord.m_uTime = fwTimer::GetTimeInMilliseconds();
	newRecord.m_nColor = m_CurrentDirection == NONE ? Color_red : Color_green; 
	m_TrackPadHistory.AddRecord(newRecord);
	
	m_LastTimeTouched = fwTimer::GetTimeInMilliseconds();
#endif

	if (pPad->GetTouchClick() != 0)
	{
		m_InvalidTouch = uTouchId;
	}

	m_FinalDirection = NONE;

	if (uTouchId == m_InvalidTouch)
	{
		m_CurrentDirection = NONE;
		return;
	}

	if (uTouchId != m_PreviousTouchId[0])
	{
		// Dealing with a new touch so record start point.
		m_PreviousTouchId[0] = uTouchId;
		pPad->GetTouchPoint(0, m_Start[0].x, m_Start[0].y);
	}
	else
	{
		// Determine direction and save delta.
		u16 x, y;
		pPad->GetTouchPoint(0, x, y);
		s16 deltaX = x - m_Start[0].x;
		s16 deltaY = y - m_Start[0].y;

		float minDistance = m_CurrentDirection == NONE ? s_MinSwipeDistance : s_MinChangeSwipeDistance;

		if((m_CurrentDirection != NONE) && ( (abs(deltaX) > s_MaxChangeSwipeDistance) || (abs(deltaY) > s_MaxChangeSwipeDistance) ) )
		{
			m_bLockDirection = true;
		}
		else
		{
#if __BANK
			SwipeDirection PrevDirection = m_CurrentDirection;
#endif

			if (deltaX > minDistance || deltaX < -minDistance ||
				deltaY > minDistance || deltaY < -minDistance)
			{
				SwipeDirection direction = NONE;
				s16 deltaXAbs = (deltaX >= 0) ? deltaX : -deltaX;
				s16 deltaYAbs = (deltaY >= 0) ? deltaY : -deltaY;
				if (deltaXAbs > deltaYAbs)
				{
					direction = (deltaX >= 0) ? RIGHT : LEFT;
				}
				else
				{
					direction = (deltaY >= 0) ? DOWN : UP;
				}
				if (m_CurrentDirection == NONE)
				{
					m_CurrentDirection = direction;
				}
				else if (direction != m_CurrentDirection)
				{
					if(s_AllowDirectionChange && !m_bLockDirection)
					{
						m_CurrentDirection = direction;
					}
					else
					{
						// Direction changed, finalize swipe and ignore rest of gesture.
						m_FinalDirection = m_CurrentDirection;
						m_InvalidTouch = uTouchId;
					}
				#if __DEV && 0
					Warningf("### TouchPad (direction change) swipe %s", GetGestureString(m_FinalDirection));
				#endif
				}

#if __BANK
				//! Inform debug system that we have detected a gesture change. Change colour of touch history to visualize this.
				if(m_CurrentDirection != PrevDirection)
				{
					m_TrackPadHistory.SetColourForTouchID(uTouchId, m_CurrentDirection != NONE  ? Color_green : Color_red);
				}
#endif
			}
		}
	}
}

void CTouchPadGesture::UpdateMultiTouch( CPad* pPad )
{
	// TODO: If one touch is released, stop updating gesture even if one touch is still held.

	// TODO: Invalidate gesture if touchpad 'button' is down.
}

#if __BANK

const char *GetGestureString(CTouchPadGesture::SwipeDirection dir)
{
	switch (dir)
	{
	case CTouchPadGesture::LEFT:
		return "LEFT";
	case CTouchPadGesture::RIGHT:
		return "RIGHT";
	case CTouchPadGesture::UP:
		return "UP";
	case CTouchPadGesture::DOWN:
		return "DOWN";
	case CTouchPadGesture::NONE:
	default:
		return "NONE";
	}

	return "NONE";
}

void CTouchPadGesture::TrackPadHistory::AddRecord(const CTouchPadGesture::TouchPointTime &touchPoint)
{
	u32 currentIndex = m_nEndIndex;
	
	if(m_nEndIndex == nMAX_HISTORY) //!Catch full ring buffer.
		currentIndex = 0;

	m_aTouchInputs[currentIndex] = touchPoint;

	m_nEndIndex++;
	if(m_nEndIndex > nMAX_HISTORY)
	{
		m_nEndIndex = 0;
	}

	//! Increment start if it equals end.
	if(m_nEndIndex == m_nStartIndex)
	{
		m_nStartIndex++;
		if(m_nStartIndex >= nMAX_HISTORY)
		{
			m_nStartIndex = 0;
		}
	}
}

void CTouchPadGesture::TrackPadHistory::Clear()
{
	m_nStartIndex = 0;
	m_nEndIndex = 0;
}

u32 CTouchPadGesture::TrackPadHistory::GetCount() const
{
	//! init case.
	if(m_nStartIndex == 0 && m_nEndIndex == 0)
	{
		return 0;
	}

	if(m_nEndIndex == nMAX_HISTORY || (m_nEndIndex > m_nStartIndex) )
	{
		return m_nEndIndex-m_nStartIndex;
	}

	//! At this point, consider pad full.
	return nMAX_HISTORY;
}

CTouchPadGesture::TouchPointTime &CTouchPadGesture::TrackPadHistory::GetIndex(u32 i)
{
	u32 nIndex = m_nStartIndex + i;
	if(nIndex >= nMAX_HISTORY)
	{
		nIndex = nIndex - nMAX_HISTORY;
	}

	Assertf(nIndex < nMAX_HISTORY, "Index out of bounds!");
	return m_aTouchInputs[nIndex];
}

void CTouchPadGesture::TrackPadHistory::SetColourForTouchID(u8 uTouchID, const Color32 &nColor)
{
	//! Iterate through list and set colour for entries with the same touch ID.
	for(int i = 0; i < GetCount(); ++i)
	{
		TouchPointTime &touchPoint = GetIndex(i);
		if(touchPoint.m_uTouchId == uTouchID)
		{
			touchPoint.m_nColor.Set(nColor.GetColor());
		}
	}
};

static Vector2 s_vHistoryDebugPos(0.275f, 0.8f);
static Vector2 s_vHistoryDebugSize(0.15f, 0.1f);

//! Not sure how we get these figures, but this is what system gives us - seems pretty arbitrary.
static dev_u32 s_MaxTrackPadValueX = 1910;
static dev_u32 s_MaxTrackPadValueY = 950;

void CTouchPadGesture::InitWidgets( bkBank& bank )
{
	bank.PushGroup("PS4 TouchPad Debug");
	bank.AddSlider("Min Swipe Distance", &s_MinSwipeDistance, 1, 5000, 1);
	bank.AddSlider("Min Change Direction Swipe Distance", &s_MinChangeSwipeDistance, 1, 5000, 1);
	bank.AddSlider("Max Change Direction Swipe Distance", &s_MaxChangeSwipeDistance, 1, 5000, 1);
	bank.AddToggle("Debug Render", &s_DebugRender);
	bank.AddToggle("Allow direction change", &s_AllowDirectionChange);
	bank.AddSlider("History Pos x", &s_vHistoryDebugPos.x, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("History Pos y", &s_vHistoryDebugPos.y, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("History Size x", &s_vHistoryDebugSize.x, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("History Size y", &s_vHistoryDebugSize.y, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Time to render history", &s_TimeToRender, 1, 60000, 100); 

	bank.PopGroup();
}

void CTouchPadGesture::TrackPadHistory::Draw()
{
	if(GetCount() > 1) //need at least 2 samples to draw a line.
	{
		//! Draw box
		Vector2 vTL = s_vHistoryDebugPos;
		Vector2 vTR = s_vHistoryDebugPos; vTR.x += s_vHistoryDebugSize.x;
		Vector2 vBR = s_vHistoryDebugPos; vBR.x += s_vHistoryDebugSize.x; vBR.y += s_vHistoryDebugSize.y;
		Vector2 vBL = s_vHistoryDebugPos; vBL.y += s_vHistoryDebugSize.y;

		grcDebugDraw::Quad(vTL, vTR, vBR, vBL, Color_blue, false);

		//! Get origin for box.
		Vector2 vBoxOrigin = vTL;

		//Draw line segments for each connecting pair. 
		for(int i = 0; i < (GetCount()-1); ++i)
		{
			const TouchPointTime &touchPoint = GetIndex(i);
			const TouchPointTime &touchPointNext = GetIndex(i+1);

			if( (fwTimer::GetTimeInMilliseconds() < (touchPointNext.m_uTime + (u32)s_TimeToRender)) && 
				(touchPoint.m_uTouchId == touchPointNext.m_uTouchId) )
			{
				Vector2 TouchStart(touchPoint.m_Touch.x, touchPoint.m_Touch.y);
				Vector2 TouchEnd(touchPointNext.m_Touch.x, touchPointNext.m_Touch.y);
			
				TouchStart.x /= s_MaxTrackPadValueX;
				TouchStart.y /= s_MaxTrackPadValueY;

				TouchEnd.x /= s_MaxTrackPadValueX;
				TouchEnd.y /= s_MaxTrackPadValueY;

				Vector2 start = vBoxOrigin;
				start.x += TouchStart.x * s_vHistoryDebugSize.x;
				start.y += TouchStart.y * s_vHistoryDebugSize.y;

				Vector2 end = vBoxOrigin;
				end.x += TouchEnd.x * s_vHistoryDebugSize.x;
				end.y += TouchEnd.y * s_vHistoryDebugSize.y;

				grcDebugDraw::Line(start, end, touchPoint.m_nColor, touchPointNext.m_nColor);
			}
		}
	}
}

void CTouchPadGesture::Debug()
{
	if(	CLoadingScreens::AreActive() || CLoadingScreens::IsInitialLoadingScreenActive() ||	// BS#4468680: skip debug draw during GTAO loading screens
		g_PlayerSwitch.IsActive()														)	// BS#4468680: skip debug draw during GTAO transition screens
		return;

	Vector2 vDebugPos = s_vHistoryDebugPos;
	vDebugPos.y += s_vHistoryDebugSize.y + 0.01f;
	static dev_float s_fTextSize = 0.02f;

	char buff[128];
	sprintf(buff, "Touch Pad X: %d Y: %d Gesture : %s", m_LastTouch[0].x, m_LastTouch[0].y, GetGestureString(m_CurrentDirection));
	grcDebugDraw::Text(vDebugPos, Color32(255,0,0), buff);
	vDebugPos.y += s_fTextSize;

	m_TrackPadHistory.Draw();
}

#endif	// __BANK

#endif // RSG_ORBIS
