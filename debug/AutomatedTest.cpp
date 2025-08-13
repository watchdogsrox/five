
#include "AutomatedTest.h"


#if ENABLE_AUTOMATED_TESTS

#include "fwscene/mapdata/mapdatadebug.h"
#include "math/amath.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/wrapper.h"
#include "system/ipc.h"
#include "system/memory.h"
#include "system/memops.h"
#include "system/param.h"
#include "system/threadregistry.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingvisualize.h"


using namespace rage;


PARAM(automatedtestop1, "[debug] Automated test operation (outer)");
PARAM(automatedtestop2, "[debug] Automated test operation (inner)");
PARAM(automatedtestop3, "[debug] Automated test operation (inner-most)");
PARAM(automatedtestreps, "[debug] Number of repetitions per test");



//
// nop - A null test. If won't do anything and immediately consider the test done after all iterations.
//

struct NopOperation : AutomatedTest::TestOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/) { return rage_new NopOperation(); }
	virtual void Reset() {}
	virtual bool Execute() { return false; }
	virtual const char *GetKeyword() const { return "nop"; }
	virtual void DescribeState(char *outBuffer, size_t bufferSize) const { safecpy(outBuffer, "(nop)", bufferSize); }
};

//
// Template for a test that just toggles a bool.
//

struct SimpleBoolOperation : AutomatedTest::TestOperation
{
	SimpleBoolOperation() : m_Enabled(false) {}

	virtual void Reset()
	{
		m_Enabled = false;
		Perform();
	}

	virtual bool Execute()
	{
		m_Enabled = !m_Enabled;
		Perform();
		return m_Enabled;			// Try again if we just enabled it, otherwise we're done testing.
	}

	virtual void Perform() = 0;

	bool m_Enabled;
};


//
// priority - Used to cycle one given thread (identified by its TTY name) through all priorities.
//

struct PriorityOperation : AutomatedTest::TestOperation
{
	virtual AutomatedTest::TestOperation *Create(const char *param)
	{
		PriorityOperation *result = rage_new PriorityOperation();
		result->m_ThreadTtyName = param;

		return result;
	}

	virtual void Reset()
	{
		m_CurrentPriority = PRIO_LOWEST;
		SetPriority();
	}

	virtual bool Execute()
	{
		// Try the next priority.
		m_CurrentPriority = (sysIpcPriority) (m_CurrentPriority+1);

		// Did we go through all priorities?
		if (m_CurrentPriority > PRIO_HIGHEST)
		{
			// Start over then.
			Reset();
			return false;
		}

		SetPriority();
		return true;
	}
	
	virtual const char *GetKeyword() const
	{
		return "priority";
	}

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
#if THREAD_REGISTRY
		formatf(outBuffer, bufferSize, "%s: %s", m_ThreadTtyName, sysThreadRegistry::GetPriorityName((sysIpcPriority) m_CurrentPriority));
#else
		outBuffer; bufferSize;
#endif // THREAD_REGISTRY
	}

private:
	void SetPriority()
	{
		// There could be multiple thread names, separated by a forward slash.
		char threadName[128];
		const char *nextSegment = m_ThreadTtyName;

		do 
		{
			const char *terminator = strchr(nextSegment, '/');

			// There's another one, so extract this one.
			if (terminator)
			{
				int len = ptrdiff_t_to_int(terminator - nextSegment);
				memcpy(threadName, nextSegment, len);
				threadName[len] = 0;

				// Prepare the next part.
				nextSegment = terminator + 1;
			}
			else
			{
				// That's the last one.
				safecpy(threadName, nextSegment);
				nextSegment = NULL;
			}

			Displayf("Setting '%s' priority to %d", threadName, m_CurrentPriority);
#if THREAD_REGISTRY
			sysThreadRegistry::SetPriority(threadName, m_CurrentPriority);
#endif // THREAD_REGISTRY
		} while (nextSegment);
	}

	sysIpcPriority m_CurrentPriority;

	const char *m_ThreadTtyName;
};

//
// newscenestrm - Toggle the new scene streamer.
//

struct NewSceneStreamerOperation : SimpleBoolOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new NewSceneStreamerOperation();
	}

	virtual const char *GetKeyword() const { return "newscenestrm"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
		formatf(outBuffer, bufferSize, "NewSceneStrm=%s", (m_Enabled) ? "on" : "off");
	}

	void Perform()
	{
		CSceneStreamer::sm_NewSceneStreamer = m_Enabled;
	}
};

//
// fairhddsched - Toggle the fair HDD scheduler.
//

struct FairHDDSchedulerOperation : SimpleBoolOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new FairHDDSchedulerOperation();
	}

	virtual const char *GetKeyword() const { return "fairhddsched"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
		formatf(outBuffer, bufferSize, "FairHDDSched=%s", (m_Enabled) ? "on" : "off");
	}

	void Perform()
	{
		*(strStreamingInfoManager::GetFairHDDSchedulerPtr()) = m_Enabled;
	}
};

//
// drivemode - Toggle the drive mode scheduler.
//

struct DriveModeOperation : SimpleBoolOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new DriveModeOperation();
	}

	virtual const char *GetKeyword() const { return "drivemode"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
		formatf(outBuffer, bufferSize, "DriveMode=%s", (m_Enabled) ? "on" : "off");
	}

	void Perform()
	{
		*(g_SceneStreamerMgr.GetStreamingLists().GetDriveModePtr()) = m_Enabled;
	}
};

//
// drivemode - Toggle the HD IMAP flying suppressor.
//

struct HdImapSuppressorOperation : SimpleBoolOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new HdImapSuppressorOperation();
	}

	virtual const char *GetKeyword() const { return "suppresshdimap"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
#if __BANK
		formatf(outBuffer, bufferSize, "SuppressHdImaps=%s", (m_Enabled) ? "on" : "off");
#else // __BANK
		safecpy(outBuffer, bufferSize, "SuppressHdImaps is BANK only");
#endif // __BANK
	}

	void Perform()
	{
#if __BANK
		*(fwMapDataDebug::GetSuppressHdWhenFlyingFastPtr()) = m_Enabled;
#endif // __BANK
	}
};

//
// streamingtrifecta - Enable all three experimental streaming features
//

struct StreamingTrifectaOperation : SimpleBoolOperation
{
	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new StreamingTrifectaOperation();
	}

	virtual const char *GetKeyword() const { return "streamingtrifecta"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
		formatf(outBuffer, bufferSize, "StreamingTrifecta=%s", (m_Enabled) ? "on" : "off");
	}

	void Perform()
	{
		*(g_SceneStreamerMgr.GetStreamingLists().GetDriveModePtr()) = m_Enabled;
		CSceneStreamer::sm_NewSceneStreamer = m_Enabled;
		*(g_SceneStreamerMgr.GetStreamingLists().GetReadAheadFactorPtr()) = (m_Enabled) ? 0.5f : 0.0f;
	}
};

//
// readahead - Play with the readahead value.
//

struct ReadAheadOperation : AutomatedTest::TestOperation
{
	ReadAheadOperation() : m_ReadAhead(0.0f) {}

	virtual AutomatedTest::TestOperation *Create(const char * /*param*/)
	{
		return rage_new ReadAheadOperation();
	}

	virtual void Reset()
	{
		m_ReadAhead = 0.0f;
		Perform();
	}

	virtual bool Execute()
	{
		m_ReadAhead += 0.2f;
		Perform();

		// Try again if we just enabled it, otherwise we're done testing.
		if (m_ReadAhead <= 1.0f)
		{
			return true;
		}

		Reset();
		return false;
	}

	virtual const char *GetKeyword() const { return "readahead"; }

	virtual void DescribeState(char *outBuffer, size_t bufferSize) const
	{
		formatf(outBuffer, bufferSize, "ReadAhead=%.2f", m_ReadAhead);
	}

	void Perform()
	{
		*(g_SceneStreamerMgr.GetStreamingLists().GetReadAheadFactorPtr()) = m_ReadAhead;
	}

	float m_ReadAhead;
};



int AutomatedTest::sm_Repetitions;
int AutomatedTest::sm_Iteration;
AutomatedTest::TestOperation *AutomatedTest::sm_CurrentOperations[OP_COUNT];
atArray<AutomatedTest::TestOperation *> AutomatedTest::sm_KnownOperations;


void AutomatedTest::Init()
{
	// Invalidate the iterator so we know whether or not the user initialized it before use
	sm_Iteration = -1;

	// Read out the PARAMs.
	sm_Repetitions = 5;
	PARAM_automatedtestreps.Get(sm_Repetitions);

	USE_DEBUG_MEMORY();
	RegisterTestOperation(rage_new NopOperation());
	RegisterTestOperation(rage_new PriorityOperation());
	RegisterTestOperation(rage_new NewSceneStreamerOperation());
	RegisterTestOperation(rage_new FairHDDSchedulerOperation());
	RegisterTestOperation(rage_new DriveModeOperation());
	RegisterTestOperation(rage_new ReadAheadOperation());
	RegisterTestOperation(rage_new HdImapSuppressorOperation());
	RegisterTestOperation(rage_new StreamingTrifectaOperation());

	for (int x=0; x<OP_COUNT; x++)
	{
		sm_CurrentOperations[x] = rage_new NopOperation();
	}

	ParseParameters();
}

void AutomatedTest::ParseParameters()
{
	USE_DEBUG_MEMORY();

	const char *param;

	if (PARAM_automatedtestop1.Get(param))
	{
		delete sm_CurrentOperations[0];
		sm_CurrentOperations[0] = ParseParameter(param);
	}

	if (PARAM_automatedtestop2.Get(param))
	{
		delete sm_CurrentOperations[1];
		sm_CurrentOperations[1] = ParseParameter(param);
	}

	if (PARAM_automatedtestop3.Get(param))
	{
		delete sm_CurrentOperations[2];
		sm_CurrentOperations[2] = ParseParameter(param);
	}

	CompileTimeAssert(OP_COUNT == 3);	// We need more PARAMs if we bump this up.
}

AutomatedTest::TestOperation *AutomatedTest::ParseParameter(const char *param)
{
	// A '_' separates the command and the parameter.
	const char *testParameter = strchr(param, '_');

	char cmd[64];

	if (testParameter)
	{
		// Copy out the command part of the parameter.
		size_t cmdLen = Min((size_t) sizeof(cmd), (size_t) (testParameter - param));
		strncpy(cmd, param, cmdLen);
		cmd[cmdLen] = 0;

		// Skip the underscore if there is one.
		testParameter++;
	}
	else
	{
		// No underscore means the entire thing is the command.
		safecpy(cmd, param);
		testParameter = "";
	}

	// Find the command.
	for (int x=0; x<sm_KnownOperations.GetCount(); x++)
	{
		if (!stricmp(sm_KnownOperations[x]->GetKeyword(), cmd))
		{
			return sm_KnownOperations[x]->Create(testParameter);
		}
	}

	Assertf(false, "Unknown test operation '%s'", cmd);
	return rage_new NopOperation();
}

void AutomatedTest::BeginTest()
{
	sm_Iteration = 0;
#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER | strStreamingVisualize::BEGIN_TEST);
	STRVIS_SET_MARKER_TEXT("Begin Automated Test");
#endif // STREAMING_VISUALIZE

	for (int x=0; x<OP_COUNT; x++)
	{
		ResetOperation(x);
	}
}

void AutomatedTest::StartIteration()
{
#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER | strStreamingVisualize::BEGIN_TEST);
	char markerText[256];
	formatf(markerText, "Next Iteration Begins:");
	for (int x=0; x<OP_COUNT; x++)
	{
		char testState[128];
		sm_CurrentOperations[x]->DescribeState(testState, sizeof(testState));
		safecat(markerText, " / ");
		safecat(markerText, testState);
	}
	STRVIS_SET_MARKER_TEXT(markerText);
#endif // STREAMING_VISUALIZE
}

bool AutomatedTest::NextIteration()
{
	Assertf(sm_Iteration != -1, "Call AUTOMATED_TEST_BEGIN before starting the next iteration");

	// Keep repeating the current test.
	if (++sm_Iteration >= sm_Repetitions)
	{
		sm_Iteration = 0;

		static int op = 0;

		// Switch to the next test setup.
		while (op < OP_COUNT)
		{
			// If this returns true, we're starting a new set of tests with a new setup.
			if (ExecuteOperation(op))
			{
				STRVIS_SET_MARKER_TEXT("All iterations done - beginning next configuration");
				STRVIS_ADD_MARKER(strStreamingVisualize::TEST_NEW_CONFIG);
				return true;
			}

			// Returning false means that we've done all possible configurations of this test. Let's
			// start the next test.
			op++;
		}

#if STREAMING_VISUALIZE
		STRVIS_ADD_MARKER(strStreamingVisualize::MARKER | strStreamingVisualize::END_TEST);
		STRVIS_SET_MARKER_TEXT("Automated test concluded.");
#endif // STREAMING_VISUALIZE

		// To keep the test from repeating itself, let's set the iteration back so we end here again.
		sm_Iteration = sm_Repetitions - 1;

		Displayf("Test concluded.");
		return false;
	}

	char markerText[128];
	formatf(markerText, "Automated test: Iteration %d/%d", sm_Iteration, sm_Repetitions);
	Displayf("%s", markerText);
#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER | strStreamingVisualize::END_TEST);
	STRVIS_SET_MARKER_TEXT(markerText);
#endif // STREAMING_VISUALIZE


	return true;
}

void AutomatedTest::ResetOperation(int operation)
{
	sm_CurrentOperations[operation]->Reset();
}

bool AutomatedTest::ExecuteOperation(int operation)
{
	return 	sm_CurrentOperations[operation]->Execute();
}

void AutomatedTest::RegisterTestOperation(AutomatedTest::TestOperation *operation)
{
	USE_DEBUG_MEMORY();
	sm_KnownOperations.Grow() = operation;
}

#endif // ENABLE_AUTOMATED_TESTS
