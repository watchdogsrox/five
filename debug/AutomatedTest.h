#ifndef DEBUG_AUTOMATEDTEST_H
#define DEBUG_AUTOMATEDTEST_H


#define ENABLE_AUTOMATED_TESTS	(__DEV || __BANK)


#if ENABLE_AUTOMATED_TESTS

#include "atl/array.h"


class AutomatedTest
{
public:
	struct TestOperation
	{
		virtual ~TestOperation() {};

		// Create this operation using settings passed via command line argument
		virtual TestOperation *Create(const char * /*param*/) = 0;

		// Reset the environment to start a new test.
		virtual void Reset() {};

		// Perform the next configuration in the test.
		virtual bool Execute() = 0;

		// Returns the keyword the user can use to trigger this test.
		virtual const char *GetKeyword() const = 0;

		virtual void DescribeState(char *outBuffer, size_t bufferSize) const = 0;
	};

	enum
	{
		OP_COUNT = 3,
	};

	static void Init();

	// PURPOSE: Begin the automated test - reset every configuration.
	static void BeginTest();

	// PURPOSE: Mark the beginning of an iteration in the automated test.
	static void StartIteration();

	// PURPOSE: Mark the end of the current iteration. Increment the iteration count, and if
	// this configuration has been executed sm_Repetitions times, reset the count and start
	// the next configuration.
	static bool NextIteration();

	// PURPOSE: Iteration count of the current configuration - between 0 and sm_Repetitions-1.
	static int GetCurrentIteration()			{ return sm_Iteration; }

	// PURPOSE: Get the number of iterations per configurations.
	static int GetRepetitions()					{ return sm_Repetitions; }

	static void RegisterTestOperation(AutomatedTest::TestOperation *operation);

private:
	static void ParseParameters();

	static TestOperation *ParseParameter(const char *param);

	static bool ExecuteOperation(int operation);

	static void ResetOperation(int operation);


	// Number of times to run each test (m_Iteration)
	static int sm_Repetitions;

	// Current iteration number
	static int sm_Iteration;

	// The currently set test operations
	static TestOperation *sm_CurrentOperations[OP_COUNT];

	// All known kinds of test operations
	static rage::atArray<TestOperation *> sm_KnownOperations;
};

#endif // ENABLE_AUTOMATED_TESTS

#endif // DEBUG_AUTOMATEDTEST_H

