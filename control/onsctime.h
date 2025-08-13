
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    onsctime.h
// PURPOSE : Onscreen counters and timers
// AUTHOR :  Obbe with stuff nicked from GTA2 added on by Graeme
// CREATED : 15.6.00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _ONSCTIME_H_
#define _ONSCTIME_H_

#include "text/text.h"

#define NUM_ONSCREEN_COUNTERS	(4)
#define NUM_ONSCREEN_TIMERS		(1)	// Number of timers which can be displayed on the screen at one time

enum {
		COUNTER_DISPLAY_NUMBER = 0,
		COUNTER_DISPLAY_BAR
//		COUNTER_DISPLAY_NUMBER_NUMBER
	 };

class COnscreenCounterEntry
{
	public :

	int *pCounterVariable;	//	Pointer to a Global integer variable in the script
//	int *pCounter2Variable;

	char CounterTextKey[TEXT_KEY_SIZE];
	
	u16 CounterDisplayMethod;	//	Number, bar, etc.

	char CounterString[42];
	bool Counter;
	bool bFlashWhenFirstDisplayed;


	void ProcessForDisplayCounter(u16 CounterDisplayMethod);

	void	SetColourID(u8 iCol_id);
	u8	GetColourID() { return iColour_id; }

private:
	u8 iColour_id;		// Index into HUD_COLOUR array defined in hud_colour.h
};


class COnscreenTimerEntry
{
public :
	int *pClockVariable;	//	Pointer to a Global integer variable in the script
							// Counts down to 0 - stored as milliseconds, displayed as minutes/seconds
							
	char ClockTextKey[TEXT_KEY_SIZE];

	char ClockString[42];
	bool Clock;
	bool bCountDown;

	u32	SecsToStartBeeping;

	void ProcessForDisplayClock(void);
	void Process(void);
};


class COnscreenTimer
{

public :

	void Init(void);
	void ProcessForDisplay(void);
	void Process(void);

	void AddClock(int *pNewClockVariable, const char *pTextKey, bool bCountdown);
	void AddCounter(int *pNewCounterVariable, s32 DisplayMethod, const char *pTextKey, s32 CounterSlot = 0);
//	void AddCounterCounter(int *pNewCounterVariable, int *pNewCounter2Variable, char *pTextKey, s32 CounterSlot);
	void ClearClock(int *pClockVariableToRemove);
	void ClearCounter(int *pCounterVariableToClear);
	
	void SetCounterFlashWhenFirstDisplayed(int *pCounterVariableToFlash, bool bFlash);

	void SetClockBeepCountdownSecs(int *pClockVariable, s32 Secs);
//	void SetCounterColourID(int *pCounterVariableToAlter, s32 ColourID);


public :

	COnscreenTimerEntry TimerEntry[NUM_ONSCREEN_TIMERS];
	COnscreenCounterEntry CounterEntry[NUM_ONSCREEN_COUNTERS];
	
	bool ClockOrCountersToDisplay;
	bool FreezeTimers;
};

#endif

