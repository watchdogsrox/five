//
// filename:	PMCPerfControl.h
// description:	
//

#ifndef INC_PMCPERFCONTROL_H_
#define INC_PMCPERFCONTROL_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers

#if	__XENON && !__FINAL && __BANK
#include <pmcpb.h>
#include <pmcpbsetup.h>
#include "Debug/Debug.h"
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#pragma comment(lib, "libpmcpb.lib")
#define	PMCPB_PERF_REPORTS	1
#else
#define PMCPB_PERF_REPORTS	0
#endif

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

#if PMCPB_PERF_REPORTS
// This structure defines a CPU performance counter that can be displayed
// on the GTA HUD. It also records the values so that they can be displayed
// every frame, instead of just on the frames when fresh values are retrieved.
struct PerfCounter
{
	// Which libpmcpb setup contains the desired counter?
	ePMCSetup	whichSetup;
	// Which counter (0 to 15) is the one you are interested in? This is best
	// found by dumping a set of counters to the debug output, finding a
	// counter you are interested in, and noting its index in the first column.
	// Note that the values of a few of the counters -- mostly instructions
	// executed per frame -- are changed in the June 2007 XDK, to correct them
	// from being half the number of instructions.
	int			counterIndex;
	// What name should be displayed for this counter?
	const char* label;

	// These members store the most recent value and other information about the
	// counters. They need to be zero initialized initially.
	unsigned int value;
	unsigned int maximum;
	unsigned int frameCount;	// For calculating average
	unsigned __int64 total;	// For calculating average
};


// List of performance counters to display on the HUD. For best results these
// should be grouped by which setup they use so that the code can cycle through
// the different setups efficiently.
static PerfCounter counters[] =
{
	{
		PMC_SETUP_OVERVIEW_PB0T0, 6, "T0 LHS"
	},
	{
		PMC_SETUP_OVERVIEW_PB0T0, 1, "T0 flushes"
		},
		{
			PMC_SETUP_OVERVIEW_PB0T0, 0, "T0 i-cache miss cycles"
		},
		{
			PMC_SETUP_OVERVIEW_PB0T0, 14, "Core 0 L2 misses"
			},
			{
				PMC_SETUP_OVERVIEW_PB0T0, 15, "Core 1 L2 misses"
			},
			{
				PMC_SETUP_OVERVIEW_PB0T0, 8,  "Core 2 L2 misses"
				},
				{
					PMC_SETUP_OVERVIEW_PB1T1, 2, "T3 LHS"
				},
				{
					PMC_SETUP_OVERVIEW_PB1T1, 9, "T3 flushes"
					},
					{
						PMC_SETUP_OVERVIEW_PB1T1, 8, "T3 i-cache miss cycles"
					},
};

// This class controls the displaying of performance counters on the HUD
// and in the debugger output, both individual counters and complete
// counter sets. You have to call StartFrame/EndFrame to make it run.
class PMCPerfControl
{
public:
	PMCPerfControl()
	{
		memset(dumpThreadOverviews, 0, sizeof(dumpThreadOverviews));
		memset(dumpThreadFlushCauses, 0, sizeof(dumpThreadFlushCauses));
		dumpHUDCounters = false;
		enableHUDCounters = false;

		bkBank *bank = BANKMGR.FindBank("PerfStuff");
		if(bank==NULL)
		{
			bank = &BANKMGR.CreateBank("PerfStuff");

			bank->AddToggle("Dump Thread 0 Overview", &dumpThreadOverviews[0]);
			bank->AddToggle("Dump Thread 0 Flushes", &dumpThreadFlushCauses[0]);
			bank->AddToggle("Dump Thread 2 Overview", &dumpThreadOverviews[2]);
			bank->AddToggle("Dump Thread 2 Flushes", &dumpThreadFlushCauses[2]);
			bank->AddToggle("Dump Thread 3 Overview", &dumpThreadOverviews[3]);
			bank->AddToggle("Dump Thread 3 Flushes", &dumpThreadFlushCauses[3]);
			bank->AddToggle("Dump HUD counters", &dumpHUDCounters);
			// If you use the libpmcpb performance counters (i.e.; if you
			// enable the HUD counters) then the sampling
			// profiler and the instrumented profiler (both in XbPerfView) will
			// not work because they rely on these performance counters.
			bank->AddToggle("Enable HUD counters (breaks CPU profilers)", &enableHUDCounters);
		}
		nextPerfCounter = 0;
	}
	void StartFrame()
	{
		getHUDCounters = false;
		dumpCounterSet = false;

		for (int i = 0; i < _countof(dumpThreadOverviews) && !dumpCounterSet; ++i)
		{
			if (dumpThreadOverviews[i])
			{
				// Dump the flush reasons counter set for the appropriate thread.
				StartTimers((ePMCSetup)(PMC_SETUP_OVERVIEW_PB0T0 + i));
				// Only dump the counters once each time they are requested.
				dumpThreadOverviews[i] = false;
				dumpCounterSet = true;
			}
		}
		for (int i = 0; i < _countof(dumpThreadFlushCauses) && !dumpCounterSet; ++i)
		{
			if (dumpThreadFlushCauses[i])
			{
				// Dump the flush reasons counter set for the appropriate thread.
				StartTimers((ePMCSetup)(PMC_SETUP_FLUSHREASONS_PB0T0 + i));
				// Only dump the counters once each time they are requested.
				dumpThreadFlushCauses[i] = false;
				dumpCounterSet = true;
			}
		}

		// If we aren't dumping a complete counter set, and if the user has the HUD
		// counters enabled, then start up the next counter set.
		if (!dumpCounterSet && enableHUDCounters)
		{
			// Select a set of sixteen counters
			StartTimers( counters[nextPerfCounter].whichSetup );
			getHUDCounters = true;
		}
	}
	void EndFrame()
	{
		if (dumpCounterSet)
			StopTimersAndDumpResults();

		if (getHUDCounters)
		{
			// Stop the counters.
			PMCStop();
			// Get the current values of the counters
			PMCState pmcstate;
			PMCGetCounters( &pmcstate );
			// Grab all the counter values that use the current counter set.
			ePMCSetup currentSetup = counters[nextPerfCounter].whichSetup;
			for (int i = 0; i < _countof(counters); ++i)
			{
				if (counters[i].whichSetup == currentSetup)
				{
					counters[i].value = (unsigned)pmcstate.pmc[counters[i].counterIndex];
					// If the frame was of a reasonable length (less than 1/10th of a second)
					// then add the statistics into the average/maximum/etc. Longer frames
					// probably indicate that we were broken in the debugger, doing a loading
					// screen, or otherwise anomalous behavior.
					if (pmcstate.ticks < 5000000)
					{
						counters[i].frameCount++;
						counters[i].total += counters[i].value;
						if (counters[i].value > counters[i].maximum)
							counters[i].maximum = counters[i].value;
					}
				}
			}
			// Move to the next counter, skipping over any adjacent counters that use the
			// same counter set.
			while (nextPerfCounter < _countof(counters) && counters[nextPerfCounter].whichSetup == currentSetup)
				++nextPerfCounter;
			if (nextPerfCounter >= _countof(counters))
				nextPerfCounter = 0;
		}
		if (enableHUDCounters)
		{
			// Display all of our counters. Some of the values may be a few frames old. Oh well.
			for (int i = 0; i < _countof(counters); ++i)
			{
				if (counters[i].frameCount)
				{
					// Display the counter to the HUD, and optionally print it to
					// the debugger output.
					char buffer[1000];
					sprintf(buffer, "%s = %dK, max = %dK, avg = %dK", counters[i].label, counters[i].value / 1000, counters[i].maximum / 1000, (int)(counters[i].total / (counters[i].frameCount * 1000)));
					grcDebugDraw::AddDebugOutput(buffer);
					if (dumpHUDCounters)
						Printf("%s\n", buffer);
				}
			}
			// When dumpHUDCounters is requested only dump them once.
			if (dumpHUDCounters)
			{
				Printf("\n");
				dumpHUDCounters = false;
			}
		}
	}
private:
	void StartTimers(ePMCSetup 
#if _XDK_VER < 7978
		whichSetup
#endif
		)
	{
#if _XDK_VER < 7978
		// Select a set of sixteen counters
		PMCInstallSetup( &PMCDefaultSetups[whichSetup] );
#endif
		// Reset the Performance Monitor Counters in preparation for a new sampling run.
		PMCResetCounters();
		// To make this code work on cores other than 0 we have to tell
		// the PMC library what core we are running on.
		//PMCSetTriggerProcessor(GetCurrentProcessorNumber() / 2);

		// Start up the Performance Monitor Counters.
		PMCStart(); // Start up counters
	}
	void StopTimersAndDumpResults()
	{
		PMCStop();
		PMCState pmcstate;
		PMCGetCounters( &pmcstate );
		PMCDumpCountersVerbose( &pmcstate, PMC_VERBOSE_NOL2ECC );
		Printf("\n");
	}
	int nextPerfCounter;
	bool getHUDCounters;
	bool dumpCounterSet;

	static const int numHardwareThreads = 6;
	bool dumpThreadOverviews[numHardwareThreads];
	bool dumpThreadFlushCauses[numHardwareThreads];
	bool dumpHUDCounters;
	bool enableHUDCounters;
};
#else
class PMCPerfControl
{
public:
	PMCPerfControl() {}
	void StartFrame() {}
	void EndFrame() {}
};
#endif

// --- Globals ------------------------------------------------------------------

#endif // !INC_PMCPERFCONTROL_H_
