#ifndef __CSELECTION_WHEEL__
#define __CSELECTION_WHEEL__

#include "vector/vector2.h"
#include "Frontend/TimeWarper.h"
#include "Frontend/SimpleTimer.h"
#include "Peds/PlayerArcadeInformation.h"

namespace rage
{
	class ioValue;
#if __BANK
	class bkGroup;
#endif
};

template<class T> 
class EdgeVal
{
public:
	EdgeVal() { Clear();}
	EdgeVal(T tSetting) : tValue(tSetting){;}

	void Clear() { tValue = static_cast<T>(0); }

	bool SetState(T tDesired) 
	{ 
		if( tValue != tDesired )
		{
			tValue = tDesired;
			return true;
		}
		return false;
	}

	bool SetState(const EdgeVal<T>& tType) 
	{ 
		if( tValue != tType.GetState() )
		{
			tValue = tType.GetState();
			return true;
		}
		return false;
	}

	const T GetState() const { return tValue; }
#if __BANK
	T& GetStateRef() { return tValue; }
#endif

	const bool operator==(const T testVal) const			{ return tValue==testVal; }
	const bool operator==(const EdgeVal<T>& tType) const	{ return tValue==tType.tValue; }
	const bool operator!=(const T testVal) const			{ return tValue!=testVal; }
	const bool operator!=(const EdgeVal<T>& tType) const	{ return tValue!=tType.tValue; }
protected:
	T tValue;
};

typedef EdgeVal<bool> EdgeBool;
typedef EdgeVal<int>  EdgeInt;

// Very basic base-class, mostly just encapsulating the common-enough elements
class CSelectionWheel : public datBase
{
public:
	CSelectionWheel(BankInt32 defaultHoldTime = 250, BankInt32 defaultShowTime = 75) 
		: BUTTON_HOLD_TIME(defaultHoldTime)
		, WHEEL_DELAY_TIME(defaultShowTime)
		, WHEEL_EDGE_DEADZONE_SQRD(0.6f*0.6f)
		, WHEEL_HYSTERESIS_PCT(0.2f)
		, WHEEL_HYSTERESIS_PCT2(0.2f)
		, m_bStickPressed(false)
		, m_vPreviousPos(0.0f, 0.0f)
		, m_FxPlayerId(-1)
		, m_bTriggeredFX(false)
	{};

	enum ButtonState {
		eBS_NotPressed, // not pressed at all
		eBS_FirstDown,  // first press, but not tapped
		eBS_Tapped,		// let go in < threshold time	
		eBS_NotHeldEnough, 
		eBS_HeldEnough,	
	};

protected:

	Vector2		m_vPreviousPos;
	CSimpleTimer m_iTimeToShowTimer;
	CTimeWarper m_TimeWarper;
	s32			m_FxPlayerId;
	bool		m_bStickPressed;
	

	BankInt32	BUTTON_HOLD_TIME;
	BankInt32	WHEEL_DELAY_TIME;
	BankFloat	WHEEL_EDGE_DEADZONE_SQRD;
	BankFloat	WHEEL_HYSTERESIS_PCT;
	BankFloat	WHEEL_HYSTERESIS_PCT2;

private:
	EdgeBool	m_bTriggeredFX;


protected:
#if __BANK
	void		CreateBankWidgets(bkGroup* pOwningGroup);
#endif

	ButtonState	HandleButton(const ioValue& relevantControl, const Vector2& stickPos, bool BANK_ONLY(bInvertControls) = false);
	Vector2		GenerateStickInput() const;
	int			HandleStick( const Vector2& stickPos, s32 iWheelSegments, int defaultValue, float fBaseOffset = 0.0f );
	void		ClearBase(bool bHardSet = true, bool bZeroTimer = true);
	void		SetStickPressed() { m_bStickPressed = true; }
	void		SetTargetTimeWarp(TW_Dir whichWay);
	void		TriggerFadeInEffect();
	void		TriggerFadeOutEffect();
	virtual void GetInputAxis(const ioValue*& LRAxis, const ioValue*& UDAxis) const;

	eArcadeTeam GetArcadeTeam();
};



#endif // __CSELECTION_WHEEL__

