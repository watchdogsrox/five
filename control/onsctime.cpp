
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    onsctime.cpp
// PURPOSE : Onscreen counters and timers
// AUTHOR :  Obbe with stuff nicked from GTA2 added on by Graeme
// CREATED : 15.12.99
//
/////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>

#include "control/onsctime.h"
#include "control/replay/replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debug.h"
#include "frontend/hud_colour.h"
#include "script/script.h"
#include "system/timer.h"




enum { DEFAULT_SECS_TO_START_BEEPING=12 };


// Name			:	COnscreenTimerEntry::DisplayClock
// Purpose		:	Updates the timer's string with the current clock time in minutes and seconds
void COnscreenTimerEntry::ProcessForDisplayClock()
{
	s32 TotalSeconds, Minutes, Seconds;

	TotalSeconds = *pClockVariable / 1000;

	Minutes = TotalSeconds / 60;
	Seconds = TotalSeconds % 60;

	Minutes = Minutes % 100;
//	Assert(Minutes < 100);
	Assert(Seconds < 60);

	sprintf(ClockString, "%02d:%02d", Minutes, Seconds);
}

// Name			:	COnscreenCounterEntry::DisplayCounter
// Purpose		:	Updates the counter's string with its current value
void COnscreenCounterEntry::ProcessForDisplayCounter(u16 CounterDisplayMethod)
{
	int CounterValue1 = MAX(0, *pCounterVariable);
//	Assertf(CounterValue1 >= 0, "ProcessForDisplayCounter - Can't display a negative number in the onscreen counter");
/*
	if (CounterDisplayMethod == COUNTER_DISPLAY_NUMBER_NUMBER)
	{
		CounterValue2 = *pCounter2Variable;
		Assertf(CounterValue2 >= 0, "ProcessForDisplayCounter - Can't display a negative number in the onscreen counter (second value)");
	}
*/
	switch (CounterDisplayMethod)
	{
		case COUNTER_DISPLAY_NUMBER:
		case COUNTER_DISPLAY_BAR :
			sprintf(CounterString, "%d", CounterValue1);
			break;
//		case COUNTER_DISPLAY_NUMBER_NUMBER:
//			sprintf(CounterString, "%d / %d", CounterValue1, CounterValue2);
//			break;
		default:
			Assertf(0, "Trying to display an unknown counter display type");
			break;
	}
}

// Name			:	COnscreenCounterEntry::SetColourID
// Purpose		:	Sets the colour index to use for this entry
void  COnscreenCounterEntry::SetColourID(u8 iCol_id)
{
	Assertf((iCol_id < HUD_COLOUR_MAX_COLOURS), "Invalid Colour Index");
	iColour_id = iCol_id;
}

// Name			:	COnscreenTimerEntry::Process
// Purpose		:	Reduces the Clock time.
void COnscreenTimerEntry::Process(void)
{
	s32 TimeStep;

	if (pClockVariable != NULL)
	{
		TimeStep = (s32)fwTimer::GetTimeStepInMilliseconds();

		if (bCountDown)
		{
			*pClockVariable -= TimeStep;

			if (*pClockVariable < 0)
			{	// Clear clock when it reaches 0
			
				// Ensure variable actually equals 0
				*pClockVariable = 0;
			
				pClockVariable = NULL;
				ClockTextKey[0] = 0;
				Clock = false;
			}
		}
		else
		{	//	This timer counts up
			*pClockVariable += TimeStep;
		}
	}
}


// Name			:	COnscreenTimer::Init
// Purpose		:	Initialises all the timers/counters
void COnscreenTimer::Init(void)
{
	u16 loop, loop2;

	FreezeTimers = FALSE;

	for (loop = 0; loop < NUM_ONSCREEN_COUNTERS; loop++)
	{
		CounterEntry[loop].pCounterVariable = NULL;
//		CounterEntry[loop].pCounter2Variable = NULL;

		for (loop2 = 0; loop2 < TEXT_KEY_SIZE; loop2++)
		{
			CounterEntry[loop].CounterTextKey[loop2] = 0;
		}
		CounterEntry[loop].CounterDisplayMethod = COUNTER_DISPLAY_NUMBER;
		CounterEntry[loop].Counter=false;
		CounterEntry[loop].bFlashWhenFirstDisplayed=true;
	}

	for (loop = 0; loop < NUM_ONSCREEN_TIMERS; loop++)
	{
		TimerEntry[loop].pClockVariable = NULL;
		
		for (loop2 = 0; loop2 < TEXT_KEY_SIZE; loop2++)
		{
			TimerEntry[loop].ClockTextKey[loop2] = 0;
		}
		TimerEntry[loop].Clock=false;
		TimerEntry[loop].bCountDown = true;
		TimerEntry[loop].SecsToStartBeeping = DEFAULT_SECS_TO_START_BEEPING;
	}
}

// Name			:	COnscreenTimer::Display
// Purpose		:	Prepares all currently-valid timers/counters for being displayed
//					(should be called every time the timers/counters are about to be displayed)
void COnscreenTimer::ProcessForDisplay(void)
{
	u16 loop;

	COnscreenTimerEntry *pTempTimer;
	COnscreenCounterEntry *pTempCounter;
	
	ClockOrCountersToDisplay = FALSE;
	for (loop = 0, pTempTimer = TimerEntry; loop < NUM_ONSCREEN_TIMERS; ++loop, ++pTempTimer)
	{
		pTempTimer->Clock=false;

		if (pTempTimer->pClockVariable != NULL)
		{	// Display some background stuff here
			pTempTimer->ProcessForDisplayClock();
			pTempTimer->Clock=true;
			ClockOrCountersToDisplay = TRUE;
		}
	}

	for (loop = 0, pTempCounter = CounterEntry; loop < NUM_ONSCREEN_COUNTERS; ++loop, ++pTempCounter)
	{
		pTempCounter->Counter=false;
	
		if (pTempCounter->pCounterVariable != NULL)  // should only need to check pCounterVariable as pCounter2Variable gets checked in ProcessForDisplayCounter
		{	// Display some background stuff here
			pTempCounter->ProcessForDisplayCounter(pTempCounter->CounterDisplayMethod);
			pTempCounter->Counter=true;
			ClockOrCountersToDisplay = TRUE;
		}
	}
}

// Name			:	COnscreenTimer::Process
// Purpose		:	Processes each of the timers in turn (counters don't need processed)
void COnscreenTimer::Process(void)
{
	u16 loop;
	COnscreenTimerEntry *pTempEntry;
	//TF **REPLACE WITH NEW CUTSCENE**

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) return;

	if (!FreezeTimers)
	{
		for (loop = 0, pTempEntry = TimerEntry; loop < NUM_ONSCREEN_TIMERS; ++loop, ++pTempEntry)
		{
			pTempEntry->Process();
		}
	}
}

// Name			:	COnscreenTimer::AddClock
// Purpose		:	Finds a free clock and starts it counting down
// Parameters	:	Pointer to the global variable to use as a clock, text label of string to display alongside the clock, flag (count up or down)
void COnscreenTimer::AddClock(int *pNewClockVariable, const char *pTextKey, bool bCountdown)
{
	u16 loop;
	COnscreenTimerEntry *pTempEntry;

	// Make sure that the variable is not already being used as a clock
	for (loop = 0, pTempEntry = TimerEntry; loop < NUM_ONSCREEN_TIMERS; ++loop, ++pTempEntry)
	{
		Assertf(pTempEntry->pClockVariable != pNewClockVariable, "AddClock - variable is already being used as a timer");
	}

	for (loop = 0, pTempEntry = TimerEntry; loop < NUM_ONSCREEN_TIMERS; ++loop, ++pTempEntry)
	{
		if (pTempEntry->pClockVariable == NULL)
		{	// This timer is free so initialise it
			pTempEntry->pClockVariable = pNewClockVariable;
			pTempEntry->bCountDown = bCountdown;
			if (pTextKey)
			{
				strncpy(pTempEntry->ClockTextKey, pTextKey, TEXT_KEY_SIZE);
			}
			else
			{
				pTempEntry->ClockTextKey[0] = 0;
			}
			return;
		}
	}

	Assertf(0, "AddClock - couldn't find a free onscreen timer");
}


// Name			:	COnscreenTimer::AddCounter
// Purpose		:	Finds a free counter and sets it up with a global variable from the script
// Parameters	:	Pointer to the Global integer variable in the script to use as a counter
//					Display method - number, bar, etc.
//					text label of string to display beside the counter
//					Counter slot to use for this variable
void COnscreenTimer::AddCounter(int *pNewCounterVariable, s32 DisplayMethod, const char *pTextKey, s32 CounterSlot)
{
	u16 loop;
	COnscreenCounterEntry *pTempEntry;

	// Make sure that the variable is not already being used as a counter
	for (loop = 0, pTempEntry = CounterEntry; loop < NUM_ONSCREEN_COUNTERS; ++loop, ++pTempEntry)
	{
		Assertf(pTempEntry->pCounterVariable != pNewCounterVariable, "AddCounter - variable is already being used as a counter");
	}

//	for (loop = 0, pTempEntry = CounterEntry; loop < NUM_ONSCREEN_COUNTERS; ++loop, ++pTempEntry)
	{
		Assertf( (CounterSlot >= 0) && (CounterSlot < NUM_ONSCREEN_COUNTERS), "AddCounter - Counter Slot should be between 0 and 3 (inclusive)");
		pTempEntry = &CounterEntry[CounterSlot];
	
		if (pTempEntry->pCounterVariable == NULL)
		{
			pTempEntry->pCounterVariable = pNewCounterVariable;
			if (pTextKey)
			{
				strncpy(pTempEntry->CounterTextKey, pTextKey, TEXT_KEY_SIZE);
			}
			else
			{
				pTempEntry->CounterTextKey[0] = 0;
			}
//			pTempEntry->pCounter2Variable = NULL;
			pTempEntry->CounterDisplayMethod = static_cast<u16>(DisplayMethod);
			pTempEntry->bFlashWhenFirstDisplayed = true;
			pTempEntry->SetColourID(HUD_COLOUR_BLUELIGHT);
			return;
		}
	}

	Assertf(0, "AddCounter - Counter slot is already in use");
//	Assertf(0, "AddCounter - couldn't find a free onscreen counter");
}



// Name			:	COnscreenTimer::AddCounterCounter
// Purpose		:	Finds a free counter and sets it up with two global variable from the script (ie 1/10)
// Parameters	:	Pointer to the Global integer variable in the script to use as a counter1
//					Pointer to the Global integer variable in the script to use as a max counter number
//					text label of string to display beside the counter
//					Counter slot to use for this variable
/*
void COnscreenTimer::AddCounterCounter(int *pNewCounterVariable, int *pNewCounter2Variable, char *pTextKey, s32 CounterSlot)
{
	u16 loop;
	COnscreenCounterEntry *pTempEntry;

	// Make sure that the variable is not already being used as a counter
	for (loop = 0, pTempEntry = CounterEntry; loop < NUM_ONSCREEN_COUNTERS; ++loop, ++pTempEntry)
	{
		Assertf((pTempEntry->pCounterVariable != pNewCounterVariable && pTempEntry->pCounter2Variable != pNewCounter2Variable), "AddCounter - variable is already being used as a counter");
	}


//	for (loop = 0, pTempEntry = CounterEntry; loop < NUM_ONSCREEN_COUNTERS; ++loop, ++pTempEntry)
	{
		Assertf( (CounterSlot >= 0) && (CounterSlot < NUM_ONSCREEN_COUNTERS), "AddCounter - Counter Slot should be 0, 1 or 2");
		pTempEntry = &CounterEntry[CounterSlot];
	
		if (pTempEntry->pCounterVariable == NULL)
		{
			pTempEntry->pCounterVariable = pNewCounterVariable;
			pTempEntry->pCounter2Variable = pNewCounter2Variable;

			if (pTextKey)
			{
				strncpy(pTempEntry->CounterTextKey, pTextKey, TEXT_KEY_SIZE);
			}
			else
			{
				pTempEntry->CounterTextKey[0] = 0;
			}
			pTempEntry->CounterDisplayMethod = COUNTER_DISPLAY_NUMBER_NUMBER;
			pTempEntry->bFlashWhenFirstDisplayed = true;
			return;
		}
	}

	Assertf(0, "AddCounter - Counter slot is already in use");
//	Assertf(0, "AddCounter - couldn't find a free onscreen counter");
}
*/

// Name			:	COnscreenTimer::ClearClock
// Purpose		:	Resets the specified Clock so that it is no longer displayed
// Parameters	:	Pointer to the global variable Clock to be cleared
void COnscreenTimer::ClearClock(int *pClockVariableToRemove)
{
	u16 loop;

	for (loop = 0; loop < NUM_ONSCREEN_TIMERS; loop++)
	{
		if (TimerEntry[loop].pClockVariable == pClockVariableToRemove)
		{	// This should allow ClearClock to be called more than once for the same global variable
			// without any complaint
			TimerEntry[loop].pClockVariable = NULL;
			TimerEntry[loop].ClockTextKey[0] = 0;
			TimerEntry[loop].Clock=false;
			TimerEntry[loop].bCountDown = true;
		}
	}
}

// Name			:	COnscreenTimer::ClearCounter
// Purpose		:	Resets the specified Counter so that it is no longer displayed
// Parameters	:	Pointer to the global variable Counter to be cleared
void COnscreenTimer::ClearCounter(int *pCounterVariableToClear)
{
	u16 loop;

	for (loop = 0; loop < NUM_ONSCREEN_COUNTERS; loop++)
	{
		if (CounterEntry[loop].pCounterVariable == pCounterVariableToClear)
		{	// This should allow ClearCounter to be called more than once for the same global variable
			// without any complaint
			CounterEntry[loop].pCounterVariable = NULL;
//			CounterEntry[loop].pCounter2Variable = NULL;
			CounterEntry[loop].CounterTextKey[0] = 0;
			CounterEntry[loop].CounterDisplayMethod = COUNTER_DISPLAY_NUMBER;
			CounterEntry[loop].Counter=false;
		}
	}
}

void COnscreenTimer::SetCounterFlashWhenFirstDisplayed(int *pCounterVariableToFlash, bool bFlash)
{
	u16 loop;
	
	for (loop = 0; loop < NUM_ONSCREEN_COUNTERS; loop++)
	{
		if (CounterEntry[loop].pCounterVariable == pCounterVariableToFlash)  // only need to do pCounterVariable here as we don't care if there is a 2nd one yet
		{
			CounterEntry[loop].bFlashWhenFirstDisplayed = bFlash;
		}
	}
}

// Name			:	COnscreenTimer::SetClockBeepCountdownSecs
// Purpose		:	Set the amount of seconds that the (countdown) timer will start beeping at.
// Parameters	:	Pointer to the global variable Clock. The number of secs to start beeping
void COnscreenTimer::SetClockBeepCountdownSecs(int *pClockVariableToAlter, s32 Secs)
{
	u16 loop;

	for (loop = 0; loop < NUM_ONSCREEN_COUNTERS; loop++)
	{
		if (TimerEntry[loop].pClockVariable == pClockVariableToAlter)
		{
			TimerEntry[loop].SecsToStartBeeping=Secs;
		}
	}
}

// Name			:	COnscreenTimer::SetCounterColourID
// Purpose		:	Sets the specified Counter so that it is displayed
//						in the colour with the index passed (defined hud_colours.h)
// Parameters	:	Pointer to the global variable Counter to be Set, Colour
/*
void COnscreenTimer::SetCounterColourID(int *pCounterVariableToAlter, s32 ColourID)
{
	u16 loop;

	for (loop = 0; loop < NUM_ONSCREEN_COUNTERS; loop++)
	{
		if (CounterEntry[loop].pCounterVariable == pCounterVariableToAlter)
		{
			CounterEntry[loop].SetColourID(static_cast<u8>(ColourID));
		}
	}
}
*/
