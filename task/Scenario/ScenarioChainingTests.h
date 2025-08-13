//
// task/Scenario/ScenarioChainingTests.h
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIO_CHAINING_TESTS_H
#define TASK_SCENARIO_SCENARIO_CHAINING_TESTS_H

#include "scene/RegdRefTypes.h"
#include "task/Scenario/ScenarioPoint.h"
#include "vectormath/mat34v.h"

namespace rage
{
	class fwModelId;
}

class CBaseModelInfo;
class CPhysical;
class CScenarioChainingGraph;
class CScenarioPoint;
class CScenarioPointRegion;

//-----------------------------------------------------------------------------

/*
PURPOSE
	Data that we need to perform a test for obstructions of a parking space,
	about the vehicle and/or dimensions of the space.
*/
struct CParkingSpaceFreeTestVehInfo
{
	CParkingSpaceFreeTestVehInfo()
			: m_ModelInfo(NULL)
	{
	}

	void Reset() { m_ModelInfo = NULL; }

	// PURPOSE:	Pointer to the vehicle's model info.
	const CBaseModelInfo* m_ModelInfo;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Helper class for performing a test to see if a single parking space is
	obstructed by something.
*/
class CParkingSpaceFreeFromObstructionsTestHelper
{
public:
	inline CParkingSpaceFreeFromObstructionsTestHelper();
	inline ~CParkingSpaceFreeFromObstructionsTestHelper();

	// PURPOSE:	Start a new test.
	// PARAMS:	vehInfo		- Info about the vehicle to test for.
	//			mtrx		- Matrix of the parking space.
	//			pExclEntity	- Optional pointer to entity to exclude from the tests.
	// RETURNS:	True if the test started successfully.
	bool StartTest(const CParkingSpaceFreeTestVehInfo& vehInfo, Mat34V_In mtrx, const CPhysical* pExclEntity);

	// PURPOSE:	Get the results of the test, if ready.
	// PARAMS:	spaceIsFreeOut	- Will be set to true if the test finished and the space was found to be free.
	// RETURNS:	True if the test is done and the results are ready.
	bool GetTestResults(bool& spaceIsFreeOut);

	// PURPOSE:	Check if the test is currently active.
	// RETURNS:	True if active.
	bool IsTestActive() const { return m_VehInfo.m_ModelInfo != NULL; }

	// PURPOSE:	Cancel any test that may already be in progress, invalidating any results.
	void ResetTest();

	// PURPOSE:	Update the test, if one is in active.
	void Update();

	// PURPOSE:	Helper function to compute the matrix of a parking space, suitable to be passed
	//			in to StartTest().
	// PARAMS:	mtrxOut		- Output for the matrix.
	//			posV		- Center position of the parking space.
	//			dirOrigV	- Forward direction of the parking space.
	static void ComputeParkingSpaceMatrix(Mat34V_InOut mtrxOut, Vec3V_In posV, Vec3V_In dirOrigV);

protected:
	// PURPOSE:	Helper function that performs the actual test.
	// PARAMS:	vehModelinfo	- Model info to use.
	//			mtrx			- Matrix to use.
	//			pExclEntity		- Optional pointer to entity to exclude.
	// RETURNS:	True if the space was free, or false if it's blocked.
	static bool IsParkingSpaceFreeFromObstructions(const CBaseModelInfo& vehModelInfo, Mat34V_In mtrx, const CPhysical* pExclEntity);

	// PURPOSE:	Matrix of the parking space.
	Mat34V							m_Matrix;

	// PURPOSE:	Info about the vehicle to test for.
	CParkingSpaceFreeTestVehInfo	m_VehInfo;

	// PURPOSE:	Entity to exclude.
	RegdConstPhy					m_ExclEntity;

	// PURPOSE:	True if the space was found to be free.
	bool							m_SpaceIsFree;

	// PURPOSE:	True if the test finished.
	bool							m_TestDone;
};

CParkingSpaceFreeFromObstructionsTestHelper::CParkingSpaceFreeFromObstructionsTestHelper()
			: m_SpaceIsFree(false)
			, m_TestDone(false)
{
}

CParkingSpaceFreeFromObstructionsTestHelper::~CParkingSpaceFreeFromObstructionsTestHelper()
{
	ResetTest();
}

//-----------------------------------------------------------------------------

/*
PURPOSE
	Manager for tests performed on chains of scenarios. Specifically, it currently
	checks to see if any parking space on the chain is obstructed.
*/
class CScenarioChainTests
{
public:
	CScenarioChainTests();
	~CScenarioChainTests();

	// PURPOSE:	Update the testing procedure, called once per frame.
	void Update();

	// PURPOSE:	Request the chain belonging to a specific scenario point to be tested,
	//			or retrieve any test results that may be available.
	// PARAMS:	pt				- The point to test for; note that this is the entry (spawn/attractor) point, not a parking space.
	//			vehInfo			- The vehicle that we will test for.
	//			requestDoneOut	- Will be set to true if the test is done and we got some results.
	//			usableOut		- If the test is done, this will be true if the chain was found to be usable.
	void RequestTest(CScenarioPoint& pt, const CParkingSpaceFreeTestVehInfo& vehInfo, bool& requestDoneOut, bool& usableOut);

	// PURPOSE:	Clear out any test results that may no longer be valid. Normally just
	//			called internally as a part of the update.
	void ClearExpiredResults();

	// PURPOSE:	Attempt to clear one test result, the one that seems least important to keep.
	void ClearLeastRelevantResult();

#if __BANK
	// PURPOSE:	Add widgets.
	// PARAMS:	bank - The bank to add to.
	void AddWidgets(bkBank& bank);

	// PURPOSE:	Perform debug rendering.
	void DebugDraw() const;

	// PURPOSE:	True if debug drawing is enabled.
	bool	m_DebugDrawingEnabled;
#endif	// __BANK

protected:

	// PURPOSE:	The maximum number of pending requests.
	static const int kMaxRequests = 16;

	// PURPOSE:	The maximum number of nodes on each chain that we need to test -
	//			this is currently just the parking spaces.
	static const int kMaxNodesToTestPerChain = 8;

	// PURPOSE:	Data about a request to get a point tested in some way.
	struct TestRequest
	{
		void Reset() { m_Point = NULL; m_VehInfo.Reset(); }

		RegdScenarioPnt					m_Point;
		CParkingSpaceFreeTestVehInfo	m_VehInfo;
	};

	// PURPOSE:	Initiate testing of a request.
	void StartTest(const TestRequest& req);

	// PURPOSE:	Update the test that's already in progress.
	void UpdateCurrentTest();

	// PURPOSE:	Check if a given node in a scenario chain will need further testing.
	// PARAMS:	graph		- The graph the node is in.
	//			nodeIndex	- The node index within the graph.
	// RETURNS:	True if the node needs testing.
	static bool ChainingNodeNeedsToBeTested(const CScenarioChainingGraph& graph, int nodeIndex);

	// PURPOSE:	Helper function to find the nodes on a chain that need to be tested.
	// PARAMS:	reg					- The scenario region the chain is in.
	//			chainIndex			- The index of the chain.
	//			nodesToTestArrayOut	- Output array for the indices of nodes we need to test.
	//			maxNodesToTest		- The max number of elements that can fit in nodesToTestArrayOut.
	// RETURNS:	Number of nodes written to nodesToTestArrayOut[].
	static int FindNodesToTest(const CScenarioPointRegion& reg, int chainIndex,
			u16* nodesToTestArrayOut, int maxNodesToTest);

	// PURPOSE:	Helper used for testing a single parking space.
	CParkingSpaceFreeFromObstructionsTestHelper m_TestHelper;

	// PURPOSE:	The request that we are currently working on.
	TestRequest m_CurrentRequestBeingTested;

	// PURPOSE:	Queue of requests that we haven't started processing yet.
	atQueue<TestRequest, kMaxRequests> m_RequestQueue;

	// PURPOSE:	Results from a completed test.
	struct TestResult
	{
		// PURPOSE:	Reset the test data.
		void Reset() { m_Point = NULL; m_ExpirationTimeMs = 0; m_Usable = false; }

		// PURPOSE:	Pointer to the scenario point we tested for.
		RegdScenarioPnt		m_Point;

		// PURPOSE:	The time at which the result will be considered expired, compatible with fwTimer::GetTimeInMilliseconds().
		u32					m_ExpirationTimeMs;

		// PURPOSE:	True if the chain was found to be usable.
		bool				m_Usable;
	};

	// PURPOSE:	Array of results from completed tests.
	atArray<TestResult>	m_TestResults;

	u16		m_NodesToTest[kMaxNodesToTestPerChain];

	// PURPOSE:	If a test is in progress, this is index of the chain within the region.
	s16		m_CurrentRequestChainIndex;

	// PURPOSE:	If a test is in progress, this is the index of the region.
	s16		m_CurrentRequestRegionIndex;

	// PURPOSE:	The index of the next node to test in the m_NodesToTest array.
	u8		m_NextNodeInArrayToTest;

	// PURPOSE:	The number of valid elements in m_NodesToTest.
	u8		m_NumNodesToTest;

	// PURPOSE:	True if we have populated m_NodesToTest with the nodes to test.
	bool	m_FoundNodesToTest;

	// PURPOSE:	True if the CParkingSpaceFreeFromObstructionsTestHelper has been activated.
	bool	m_HelperActive;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIO_CHAINING_TESTS_H

// End of file 'task/Scenario/ScenarioChainingTests.h'
