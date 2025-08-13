//
// task/Scenario/ScenarioChainingTests.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "task/Scenario/ScenarioChainingTests.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#include "ai/aichannel.h"
#include "ai/stats.h"
#include "Objects/object.h"
#include "Objects/Door.h"
#include "physics/WorldProbe/shapetestboundingboxdesc.h"
#include "physics/WorldProbe/shapetestprobedesc.h"
#include "scene/Physical.h"

using namespace AIStats;

//-----------------------------------------------------------------------------

bool CParkingSpaceFreeFromObstructionsTestHelper::StartTest(const CParkingSpaceFreeTestVehInfo& vehInfo, Mat34V_In mtrx, const CPhysical* pExclEntity)
{
	ResetTest();

	m_VehInfo = vehInfo;
	m_Matrix = mtrx;
	m_ExclEntity = pExclEntity;

	return true;
}


bool CParkingSpaceFreeFromObstructionsTestHelper::GetTestResults(bool& spaceIsFreeOut)
{
	spaceIsFreeOut = m_SpaceIsFree;
	return m_TestDone;
}


void CParkingSpaceFreeFromObstructionsTestHelper::ResetTest()
{
	m_TestDone = false;
	m_VehInfo.m_ModelInfo = NULL;
	m_SpaceIsFree = false;
}


void CParkingSpaceFreeFromObstructionsTestHelper::Update()
{
#if 0	//DEBUG_DRAW
	if(m_VehInfo.m_ModelInfo && m_VehInfo.m_ModelInfo->GetFragType() && m_TestDone)
	{
		const phBound* bound = m_VehInfo.m_ModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();
		const Vec3V boxMinV = bound->GetBoundingBoxMin();
		const Vec3V boxMaxV = bound->GetBoundingBoxMax();
		grcDebugDraw::BoxOriented(boxMinV, boxMaxV, m_Matrix, m_SpaceIsFree ? Color_green : Color_red, false);
	}
#endif

	if(m_TestDone || !m_VehInfo.m_ModelInfo)
	{
		return;
	}

	m_SpaceIsFree = IsParkingSpaceFreeFromObstructions(*m_VehInfo.m_ModelInfo, m_Matrix, m_ExclEntity.Get());
	m_TestDone = true;
}


void CParkingSpaceFreeFromObstructionsTestHelper::ComputeParkingSpaceMatrix(Mat34V_InOut mtrxOut, Vec3V_In posV, Vec3V_In dirOrigV)
{
	// This simply builds a flattened (upright) orthonormal matrix, as one would expect.

	const ScalarV zeroV(V_ZERO);

	Vec3V dirFlatV = dirOrigV;
	dirFlatV.SetZ(zeroV);

	const Vec3V dirV = Normalize(dirFlatV);
	const Vec3V orthDirV(dirV.GetY(), Negate(dirV.GetX()), zeroV);

	mtrxOut.SetCol0(orthDirV);
	mtrxOut.SetCol1(dirV);
	mtrxOut.SetCol2(Vec3V(V_Z_AXIS_WONE));
	mtrxOut.SetCol3(posV);
}


bool CParkingSpaceFreeFromObstructionsTestHelper::IsParkingSpaceFreeFromObstructions(const CBaseModelInfo& vehModelInfo, Mat34V_In mtrx, const CPhysical* pExclEntity)
{
	PF_FUNC(IsParkingSpaceFreeFromObstructions);

	// The test done before basically assumes that we have a valid frag type, which probably
	// means that the model needs to be streamed in. We could fail an assert if that's not the case,
	// but I have a feeling that that might fail on occasion since we queue up these tests over time.
	//	if(!aiVerifyf(vehModelInfo.GetFragType(), "Expected frag type for model '%s'.", vehModelInfo.GetModelName()))
	gtaFragType* vehFragType = vehModelInfo.GetFragType();
	if(!vehFragType)
	{
		return true;	// This is probably safer than returning false.
	}

	const phBound* bound = vehFragType->GetPhysics(0)->GetCompositeBounds();

	// Note: this code was adapted from CCarGenerator::DoSyncProbeForBlockingObjects().

	// Test against vehicles, peds, and objects.
	const int nTestTypeFlags
			= ArchetypeFlags::GTA_BOX_VEHICLE_TYPE
			| ArchetypeFlags::GTA_VEHICLE_TYPE
			| ArchetypeFlags::GTA_PED_TYPE
			| ArchetypeFlags::GTA_OBJECT_TYPE;

	const Vec3V extendBoxMinV(0.0f, 0.0f, -2.0f); // extend bbox check 2m down
	const Vec3V extendBoxMaxV(V_ZERO);
	const u32 uMaxNumResults = 12;
	WorldProbe::CShapeTestFixedResults<uMaxNumResults> hpStructure;
	WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
	bboxDesc.SetBound(bound);
	bboxDesc.SetTransform(&RCC_MATRIX34(mtrx));
	bboxDesc.SetIncludeFlags(nTestTypeFlags);

	// I found some objects such as gas cans which are considered pickups, which have a large
	// sphere around them, which could potentially be considered blocking us. For now, the easy
	// solution is to just exclude those from the test. This probably doesn't just exclude the
	// pickup sphere but the whole object, so it's not ideal, but those are probably not
	// too common to be a problem - the player is probably more likely to pick them up than to push
	// them around, and they are probably not large enough to really be in the way of a
	// vehicle trying to park.
	bboxDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);

	bboxDesc.SetExtensionToBoundingBoxMin(RCC_VECTOR3(extendBoxMinV));
	bboxDesc.SetExtensionToBoundingBoxMax(RCC_VECTOR3(extendBoxMaxV));
	bboxDesc.SetExcludeEntity(pExclEntity);
	bboxDesc.SetResultsStructure(&hpStructure);
	bboxDesc.SetMaxNumResultsToUse(uMaxNumResults);

	// Objects which have not been uprooted are no longer considered as blockers for parking spaces
	if(WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc))
	{
		const WorldProbe::CShapeTestResults * pResults = bboxDesc.GetResultsStructure();
		for(s32 i=0; i<pResults->GetNumHits(); ++i)
		{
			const WorldProbe::CShapeTestHitPoint & hp = (*pResults)[i];
			const CEntity * pHitEntity = hp.GetHitEntity();
			const CObject * pHitObject = (pHitEntity && pHitEntity->GetIsTypeObject()) ? static_cast<const CObject *>(pHitEntity) : NULL;
			const CDoor   * pDoor = (pHitObject && pHitObject->IsADoor()) ? static_cast<const CDoor *>(pHitObject) : NULL;
			if (!pHitObject || (pHitObject && pHitObject->m_nObjectFlags.bHasBeenUprooted))
			{
				// If our uprooted object is a door that is nearly at rest it is not considered an obstruction
				if (!(pDoor && (Abs(pDoor->GetDoorOpenRatio()) < 0.1f) && (Abs(pDoor->GetTargetDoorRatio()) < SMALL_FLOAT)))
				{
					return false;
				}				
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------

CScenarioChainTests::CScenarioChainTests()
		: m_CurrentRequestChainIndex(-1)
		, m_CurrentRequestRegionIndex(-1)
		, m_NextNodeInArrayToTest(0)
		, m_NumNodesToTest(0)
		, m_FoundNodesToTest(false)
		, m_HelperActive(false)
{

#if __BANK
	m_DebugDrawingEnabled = false;
#endif	// __BANK

	// TODO: Think more about how many of these we might need:
	m_TestResults.Reset();
	m_TestResults.Reserve(64);
}


CScenarioChainTests::~CScenarioChainTests()
{
}


void CScenarioChainTests::Update()
{
	// Probably shouldn't update while paused:
	if(fwTimer::IsGamePaused())
	{
		return;
	}

	PF_FUNC(CScenarioChainTests);

	// Clear out any old results.
	ClearExpiredResults();

	// If we don't currently have a point to test, find the next valid request.
	if(!m_CurrentRequestBeingTested.m_Point)
	{
		while(!m_RequestQueue.IsEmpty())
		{
			// Pop the first request and check if it's valid.
			TestRequest& r = m_RequestQueue.Pop();
			CScenarioPoint* pt = r.m_Point;
			if(!pt)
			{
				// This point no longer exists, nothing to do.
				r.Reset();
				continue;
			}

			// Start a new test, and reset the request data.
			StartTest(r);
			r.Reset();
			break;
		}
	}

	// If we now have a valid test, update it.
	if(m_CurrentRequestBeingTested.m_Point)
	{
		UpdateCurrentTest();
	}
}


void CScenarioChainTests::RequestTest(CScenarioPoint& pt,
		const CParkingSpaceFreeTestVehInfo& vehInfo,
		bool& requestDoneOut, bool& usableOut)
{
	PF_FUNC(CScenarioChainTests);

	// If the point isn't chained at all, then we don't want to just consider it usable,
	// without any delay.
	if(!pt.IsChained())
	{
		requestDoneOut = true;
		usableOut = true;
		return;
	}

	// Note: we currently treat each entry point of a chain as if it were a new chain
	// that needs to be tested. If there is a lot of overlap, like in a chain with multiple
	// spawn points, we would potentially want to do something better to reuse the test results.

	// Note 2: the other thing that's a bit sketchy here is that we basically make the assumption
	// that the tests done for one vehicle would apply to another vehicle, if multiple requests came
	// in before the results expired. We could do something more sophisticated, checking if we are testing
	// for a vehicle of the same or smaller size, etc, but not sure that it's really going to be necessary.

	requestDoneOut = false;
	usableOut = false;

	// Check if this point is already requested. If so, we don't request again.
	const int numRequests = m_RequestQueue.GetCount();
	for(int i = 0; i < numRequests; i++)
	{
		if(m_RequestQueue[i].m_Point == &pt)
		{
			return;
		}
	}

	// Check if we are already working on testing this point.
	if(&pt == m_CurrentRequestBeingTested.m_Point)
	{
		return;
	}

	// Check if we already have results for this point.
	const int numTestResults = m_TestResults.GetCount();
	for(int i = 0; i < numTestResults; i++)
	{
		TestResult& r = m_TestResults[i];
		if(r.m_Point == &pt)
		{
			requestDoneOut = true;
			usableOut = r.m_Usable;
			return;
		}
	}

	// If the request queue is full, just return. The way this works, the user should automatically
	// be tolerant to that, and just request a test again if one is still desired.
	if(m_RequestQueue.IsFull())
	{
		return;
	}

	// Push the request on the queue.
	// Note: not entirely sure that this gives us FIFO, but I think it does. Should check...
	TestRequest& req = m_RequestQueue.Append();
	req.m_Point = &pt;
	req.m_VehInfo = vehInfo;
}


void CScenarioChainTests::ClearExpiredResults()
{
	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();

	// Loop over the test results and remove any where the point no
	// longer exists, or where the result is just too old.
	int numTestResults =  m_TestResults.GetCount();
	for(int i = 0; i < numTestResults; i++)
	{
		TestResult& r = m_TestResults[i];
		if(!r.m_Point || currentTimeMs >= r.m_ExpirationTimeMs)
		{
			if(i != numTestResults - 1)
			{
				r = m_TestResults[numTestResults - 1];
				m_TestResults[numTestResults - 1].Reset();
			}

			i--;
			numTestResults--;
		}
	}
	m_TestResults.Resize(numTestResults);
}


void CScenarioChainTests::ClearLeastRelevantResult()
{
	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();

	// Loop over the test results and try to find the best one to remove.
	int numTestResults =  m_TestResults.GetCount();
	int toRemove = -1;
	u32 leastTimeUntilWillBeRemovedMs = 0xffffffff;
	for(int i = 0; i < numTestResults; i++)
	{
		TestResult& r = m_TestResults[i];
		if(!r.m_Point || currentTimeMs >= r.m_ExpirationTimeMs)
		{
			// This point was actually removed, or the entry has expired, we can use this one.
			toRemove = i;
			break;
		}

		// Compute how much more time has to elapse until this one would expire.
		const u32 timeUntilWillBeRemovedMs = r.m_ExpirationTimeMs - currentTimeMs;
		if(timeUntilWillBeRemovedMs < leastTimeUntilWillBeRemovedMs)
		{
			// This is the one that will expire the soonest.
			toRemove = i;
			leastTimeUntilWillBeRemovedMs = timeUntilWillBeRemovedMs;
		}
	}

	if(toRemove >= 0)
	{
		// Do the removal (basically a DeleteFast() with a call to Reset thrown in to properly
		// clear out registered references).
		TestResult& r = m_TestResults[toRemove];
		if(toRemove != numTestResults - 1)
		{
			r = m_TestResults[numTestResults - 1];
			m_TestResults[numTestResults - 1].Reset();
		}
		numTestResults--;
		m_TestResults.Resize(numTestResults);
	}
}


#if __BANK

void CScenarioChainTests::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Draw Chain Tests", &m_DebugDrawingEnabled);
}

void CScenarioChainTests::DebugDraw() const
{
#if DEBUG_DRAW
	if(!m_DebugDrawingEnabled)
	{
		return;
	}

	const int numRequests = m_RequestQueue.GetCount();
	for(int i = 0; i < numRequests; i++)
	{
		CScenarioPoint* pt = m_RequestQueue[i].m_Point;
		if(pt)
		{
			const Vec3V posV = pt->GetWorldPosition();
			grcDebugDraw::Sphere(posV, 0.8f, Color_yellow, false);
		}
	}

	if(m_CurrentRequestBeingTested.m_Point)
	{
		const Vec3V posV = m_CurrentRequestBeingTested.m_Point->GetWorldPosition();
		grcDebugDraw::Sphere(posV, 0.85f, Color_white, false);
	}

	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();

	const int numTestResults = m_TestResults.GetCount();
	for(int i = 0; i < numTestResults; i++)
	{
		const TestResult& r = m_TestResults[i];
		CScenarioPoint* pt = r.m_Point;
		if(pt)
		{
			const Vec3V posV = pt->GetWorldPosition();
			Color32 col = m_TestResults[i].m_Usable ? Color_green : Color_red;
			grcDebugDraw::Sphere(posV, 0.7f, col, false);

			u32 timeRemaining = 0;
			if(currentTimeMs < r.m_ExpirationTimeMs)
			{
				timeRemaining = r.m_ExpirationTimeMs - currentTimeMs;
			}

			char buff[256];
			formatf(buff, "%.1f", timeRemaining*0.001f);
			grcDebugDraw::Text(posV, Color_white, buff, false);
		}
	}
#endif	// DEBUG_DRAW
}

#endif	// __BANK

void CScenarioChainTests::StartTest(const TestRequest& req)
{
	// Copy the request data.
	m_CurrentRequestBeingTested = req;

	// Reset to prepare for a new test.
	m_FoundNodesToTest = false;
	m_HelperActive = false;
	m_TestHelper.ResetTest();

	CScenarioPoint* pt = req.m_Point;
	aiAssert(pt);	// Should have been checked elsewhere

	// Find the chain this point is a part of. This can be a pretty expensive operatin
	// the way things are done now.
	aiAssert(pt->IsChained());	// If the point wasn't chained, we shouldn't even get here.
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	int regionIndex = -1;
	int chainIndex = -1;
	for (int i = 0; i < numRegions; i++)
	{
		CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegionFast(i);
		if(!region)
		{
			continue;
		}

		const CScenarioChainingGraph& graph = region->GetChainingGraph();

		// Note: this is the slow part.
		const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(*pt);

		if(nodeIndex >= 0)
		{
			// Found it.
			chainIndex = graph.GetNodesChainIndex(nodeIndex);
			regionIndex = i;
			aiAssert(chainIndex >= 0);
			break;
		}
	}

	// Note that these might be -1 at this point:
	aiAssert(chainIndex <= 0x8000);
	aiAssert(regionIndex <= 0x8000);
	m_CurrentRequestChainIndex = (s16)chainIndex;
	m_CurrentRequestRegionIndex = (s16)regionIndex;
}


void CScenarioChainTests::UpdateCurrentTest()
{
	// MAGIC! TODO: Make tunable
	static const int expirationTimeFree = 10000;
	static const int expirationTimeBlocked = 30000;

	// Get a pointer to the region the chain in - if it's still resident.
	CScenarioPointRegion* pRegion = NULL;
	const int regionIndex = m_CurrentRequestRegionIndex;
	if(regionIndex >= 0 && regionIndex < SCENARIOPOINTMGR.GetNumRegions())
	{
		pRegion = SCENARIOPOINTMGR.GetRegion(regionIndex);
	}

	if(!pRegion || !m_CurrentRequestBeingTested.m_Point)
	{
		// The region is no longer resident and/or the point doesn't exist,
		// forget about this test.
		m_CurrentRequestChainIndex = -1;
		m_CurrentRequestRegionIndex = -1;
		m_CurrentRequestBeingTested.Reset();
		return;
	}

	// If we haven't already found the nodes we want to test, do so now.
	// Note: this could also potentially be slow if we have a really long chain.
	// We could potentially bail out if the chain has too many edges to protect against that.
	const CScenarioChainingGraph& graph = pRegion->GetChainingGraph();
	if(!m_FoundNodesToTest)
	{
		const int numNodesToTest = FindNodesToTest(*pRegion, m_CurrentRequestChainIndex, m_NodesToTest, kMaxNodesToTestPerChain);
		aiAssert(numNodesToTest <= 0xff);
		m_NumNodesToTest = (u8)numNodesToTest;
		m_NextNodeInArrayToTest = 0;
		m_FoundNodesToTest = true;
	}

	bool spaceFree = true;

	// Test the next node in the chain.
	if(m_NextNodeInArrayToTest < m_NumNodesToTest)
	{
		aiAssert(m_NextNodeInArrayToTest < kMaxNodesToTestPerChain);
		const int testNode = m_NodesToTest[m_NextNodeInArrayToTest];

		// Should hold up:
		//	aiAssert(NeedsToBeTested(graph, testNode));

		bool thisSpaceIsFree = true;

		CScenarioPoint* pt = graph.GetChainingNodeScenarioPoint(testNode);
		if(pt)
		{
			// If the parking spot test helper isn't already active, start it.
			if(!m_HelperActive)
			{
#if __ASSERT
				const int virtualScenarioType = pt->GetScenarioTypeVirtualOrReal();
				const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(virtualScenarioType);
				aiAssert(pInfo->GetIsClass<CScenarioVehicleParkInfo>() || pt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival));
#endif
				const Vec3V posV = pt->GetWorldPosition();
				const Vec3V dirV = pt->GetWorldDirectionFwd();

				Mat34V mtrx;
				CParkingSpaceFreeFromObstructionsTestHelper::ComputeParkingSpaceMatrix(mtrx, posV, dirV);
				m_TestHelper.StartTest(m_CurrentRequestBeingTested.m_VehInfo, mtrx, NULL);

				m_HelperActive = true;
			}

			// Update the helper.
			m_TestHelper.Update();

			// Check if the test is done, and get the results if so.
			thisSpaceIsFree = false;
			if(!m_TestHelper.GetTestResults(thisSpaceIsFree))
			{
				// Not done yet, we will continue the next frame.
				return;
			}

			// The test is done and we have the results (thisSpaceIsFree).
			m_HelperActive = false;
			m_TestHelper.ResetTest();
		}

		if(!thisSpaceIsFree)
		{
			// If this spot wasn't free, the chain can't be considered free.
			// Note that we will fall through to the code below and finalize the
			// testing of this chain.
			spaceFree = false;
		}
		else
		{
			// This space was free - advance to the next one we need to test.
			m_NextNodeInArrayToTest++;

			// Check if we are not done already.
			if(m_NextNodeInArrayToTest < m_NumNodesToTest)
			{
				// Not done, continue next frame.
				return;
			}
		}
	}

	// Clear some space in the test result cache, if needed. Should be better to retire an old entry than
	// to not have space to add the new one.
	if(m_TestResults.GetCount() >= m_TestResults.GetCapacity())
	{
		ClearLeastRelevantResult();
	}

	// Check if there is room to store the result.
	// Note: if this assert fails, we should consider writing code to just dump the oldest result.
	if(aiVerifyf(m_TestResults.GetCount() < m_TestResults.GetCapacity(), "Scenario chain test result array filled up, capacity is %d. Should have cleared one up...", m_TestResults.GetCapacity()))
	{
		// Store the result.
		TestResult& r = m_TestResults.Append();
		r.m_Point = m_CurrentRequestBeingTested.m_Point;
		r.m_Usable = spaceFree;

		// Make it expire in some time, depending on if it was free or not (if it was not,
		// we probably don't want to rush to do another test).
		const u32 currentTime = fwTimer::GetTimeInMilliseconds();
		if(spaceFree)
		{
			r.m_ExpirationTimeMs = currentTime + expirationTimeFree;
		}
		else
		{
			r.m_ExpirationTimeMs = currentTime + expirationTimeBlocked;
		}
	}

	// Clear out the current request, since this test is done.
	m_TestHelper.ResetTest();
	m_CurrentRequestBeingTested.Reset();
}


bool CScenarioChainTests::ChainingNodeNeedsToBeTested(const CScenarioChainingGraph& graph, int nodeIndex)
{
	CScenarioPoint* pt = graph.GetChainingNodeScenarioPoint(nodeIndex);
	if(!pt)
	{
		// No point, nothing to test.
		return false;
	}

	const int virtualScenarioType = pt->GetScenarioTypeVirtualOrReal();
	if(SCENARIOINFOMGR.IsVirtualIndex(virtualScenarioType))
	{
		// No testing of virtual scenarios for now.
		return false;
	}

	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(virtualScenarioType);
	if(!pInfo)
	{
		// Invalid type, somehow.
		return false;
	}

	// We only test parking scenarios and landing scenarios at this time
	if(!pInfo->GetIsClass<CScenarioVehicleParkInfo>() && !pt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival))
	{
		return false;
	}

	// Test it.
	return true;	
}


int CScenarioChainTests::FindNodesToTest(const CScenarioPointRegion& reg, int chainIndex,
		u16* nodesToTestArrayOut, int maxNodesToTest)
{
	// Note: this is basically assuming that this is just one linear chain and we want
	// to test all of it. If the chain is actually a graph with lots of branches etc,
	// we may need something more sophisticated to support it properly.

	// Temporary array of nodes in the chain.
	static const int kMaxNodes = 1024;
	u16 nodesToTest[kMaxNodes];

	int numNodesToTest = 0;

	// Loop over all the edges of this chain.
	const CScenarioChainingGraph& graph = reg.GetChainingGraph();
	const CScenarioChain& chain = graph.GetChain(chainIndex);
	const int numEdges = chain.GetNumEdges();
	for(int i = 0; i < numEdges; i++)
	{
		// Get the index of the from and to nodes.
		const int edgeId = chain.GetEdgeId(i);
		const CScenarioChainingEdge& edge = graph.GetEdge(edgeId);
		const int nodeIndexFrom = edge.GetNodeIndexFrom();
		const int nodeIndexTo = edge.GetNodeIndexTo();

		const CScenarioChainingNode& nodeFrom = graph.GetNode(nodeIndexFrom);

		// If we have incoming edges, we will already consider this node
		// from one of those edges, but if not, it may need to be tested.
		if(!nodeFrom.m_HasIncomingEdges && ChainingNodeNeedsToBeTested(graph, nodeIndexFrom))
		{
			if(numNodesToTest < kMaxNodes)
			{
				aiAssert(nodeIndexFrom <= 0xffff);
				nodesToTest[numNodesToTest++] = (u16)nodeIndexFrom;
			}
		}

		// Check if the destination node of this edge needs to be tested.
		if(ChainingNodeNeedsToBeTested(graph, nodeIndexTo))
		{
			if(numNodesToTest < kMaxNodes)
			{
				aiAssert(nodeIndexTo <= 0xffff);
				nodesToTest[numNodesToTest++] = (u16)nodeIndexTo;
			}
		}
	}

	// This isn't entirely accurate in case we just filled up, but should be a pretty minor risk of that.
	aiAssertf(numNodesToTest < kMaxNodes, "Filled up local array of chaining nodes to test (%d).", kMaxNodes);

	// Didn't even find any candidates.
	if(!numNodesToTest)
	{
		return 0;
	}

	// Sort the array to help remove duplicates.
	std::sort(nodesToTest, nodesToTest + numNodesToTest);

	// Loop over the sorted results, and write all unique arrays to the user's array.
	int newIndex = 0;
	int prevNode = -1;
	for(int i = 0; i < numNodesToTest; i++)
	{
		const int curNode = nodesToTest[i];
		if(curNode != prevNode)
		{
			if(aiVerifyf(newIndex < maxNodesToTest, "Too many nodes to test on scenario chain (%d).", maxNodesToTest))
			{
				nodesToTestArrayOut[newIndex++] = (u16)curNode;
			}
			prevNode = curNode;
		}
	}

	// Return the number of unique nodes we found.
	numNodesToTest = newIndex;
	return numNodesToTest;
}

//-----------------------------------------------------------------------------

// End of file 'task/Scenario/ScenarioChainingTests.cpp'
