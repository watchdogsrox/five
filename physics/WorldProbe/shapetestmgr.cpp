// RAGE headers:
#include "atl/functor.h"
#include "grcore/debugdraw.h"
#include "physics/inst.h"
#include "physics/intersection.h"
#include "physics/collision.h"
#include "phbound/boundbvh.h"
#include "phbound/boundbox.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundGeomSecondSurface.h"
#include "system/task.h"
#include "spatialdata/sphere.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"

// Game headers:
#include "network/Live/livemanager.h"
#include "physics/WorldProbe/shapetestmgr.h"
#include "physics/WorldProbe/debug.h"
#include "physics/WorldProbe/shapetesttask.h"
#include "physics/WorldProbe/shapetestbatchdesc.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"
#include "physics/WorldProbe/shapetestspheredesc.h"
#include "system/ThreadPriorities.h"

#include "rockstar/NmRsInclude.h"
#include "rockstar/NmRsCharacter.h"


PHYSICS_OPTIMISATIONS()

#if __BANK
PARAM(forcesynchronousshapetests, "Force worldprobe shapetests to run on the main thread.");
PARAM(logWorldProbeShapetestPool, "Enable logging for worldprobe shapetests pool");
#endif

// Static manager singleton object instance.
WorldProbe::CShapeTestManager WorldProbe::CShapeTestManager::sm_shapeTestMgrInst;

phAsyncShapeTestMgr* WorldProbe::CShapeTestManager::ms_pRageAsyncShapeTestMgr = NULL;

sysCriticalSectionToken WorldProbe::CShapeTestManager::ms_AsyncShapeTestPoolCs;
sysCriticalSectionToken WorldProbe::CShapeTestManager::ms_RequestedAsyncShapeTestsCs;

int WorldProbe::CShapeTestManager::m_iShapeTestAsyncSchedulerIndex = 0;

atQueue<WorldProbe::CShapeTestTaskData*, MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS> WorldProbe::CShapeTestManager::ms_RequestedAsyncShapeTests;


// Definitions of some static debug values:
#if WORLD_PROBE_DEBUG
sysCriticalSectionToken WorldProbe::CShapeTestManager::ms_ShapeDebugPausedRenderBufferCs;
WorldProbe::CShapeTestDebugRenderData* WorldProbe::CShapeTestManager::ms_ShapeDebugPausedRenderBuffer;
u32 WorldProbe::CShapeTestManager::ms_ShapeDebugPausedRenderCount = 0;
u32 WorldProbe::CShapeTestManager::ms_ShapeDebugPausedRenderHighWaterMark = 0;

bool WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncQueueSummary = false;
int WorldProbe::CShapeTestManager::ms_bMaxIntersectionsToDraw = MAX_DEBUG_DRAW_INTERSECTIONS_PER_SHAPE;
int WorldProbe::CShapeTestManager::ms_bIntersectionToDrawIndex = -1;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitPoint = true;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitNormal = true;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitPolyNormal = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitInstanceName = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitHandle = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitComponentIndex = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitPartIndex = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitMaterialName = false;
bool WorldProbe::CShapeTestManager::ms_bDebugDrawHitMaterialId = false;
float WorldProbe::CShapeTestManager::ms_fHitPointNormalLength = 0.4f;
float WorldProbe::CShapeTestManager::ms_fHitPointRadius = 0.06f;
bool WorldProbe::CShapeTestManager::ms_bUseCustomCullVolume = true;
bool WorldProbe::CShapeTestManager::ms_bDrawCullVolume = false;
bool WorldProbe::CShapeTestManager::ms_bForceSynchronousShapeTests = false;
#if ENABLE_ASYNC_STRESS_TEST
unsigned int WorldProbe::CShapeTestManager::ms_uStressTestProbeCount = 50;
bool WorldProbe::CShapeTestManager::ms_bAsyncStressTestEveryFrame = false;
bool WorldProbe::CShapeTestManager::ms_bAbortAllTestsInReverseOrderImmediatelyAfterStarting = false;
WorldProbe::CShapeTestResults WorldProbe::CShapeTestManager::ms_DebugShapeTestResults[MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS];
#endif // ENABLE_ASYNC_STRESS_TEST
#endif // WORLD_PROBE_DEBUG

////////////////////////////////////////////////////////// SHAPE TEST MANAGER //////////////////////////////////////////////////////////

// Global accessor to the shape test manager:
WorldProbe::CShapeTestManager* WorldProbe::GetShapeTestManager() { return CShapeTestManager::GetShapeTestManager(); }

void WorldProbe::CShapeTestManager::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{

#if __PPU

		m_iShapeTestAsyncSchedulerIndex = sysTaskManager::SCHEDULER_SHAPE_TEST;

#else // PS3 doesn't have task schedulers

		// JB : needs a large stack size for shapetests
		static const int iStackSize = 65536;
		static const int iScratchSize = 32768;

		static const rage::sysIpcPriority iTaskPriority = PRIO_SHAPETEST_ASYNC_THREAD;
		const u32 iThreadMask = (1<<1) | (1<<2) | (1<<3) | (1<<5);

		m_iShapeTestAsyncSchedulerIndex = sysTaskManager::AddScheduler("ShapeTestAsyncSched", iStackSize, iScratchSize, iTaskPriority, iThreadMask, 0);

#endif // __PPU


#if WORLD_PROBE_DEBUG
		{
			USE_DEBUG_MEMORY();
			ms_ShapeDebugPausedRenderBuffer = rage_new CShapeTestDebugRenderData[MAX_DEBUG_DRAW_BUFFER_SHAPES];
		}
#endif // WORLD_PROBE_DEBUG

#if WORLD_PROBE_DEBUG
		// Initialise the context filters used to select which shape tests should be visible when
		// debug drawing is enabled.
		for(int i = 0; i < LOS_NumContexts; ++i)
		{
			ms_abContextFilters[i] = true;
		}

#if WORLD_PROBE_FREQUENCY_LOGGING
		m_frequencyLog.Reserve(MAX_SHAPETEST_CALLS_FOR_LOGGING);
		m_frequencyLog.FinishInsertion(); // Sets the map state to "sorted".
#endif // WORLD_PROBE_FREQUENCY_LOGGING
#endif // WORLD_PROBE_DEBUG

		WorldProbe::CShapeTestTaskData::InitPool(MEMBUCKET_PHYSICS);

#if __BANK
		if (PARAM_logWorldProbeShapetestPool.Get())
		{
			WorldProbe::CShapeTestTaskData::GetPool()->setLogged(true);
		}
#endif

		// Create an instance of the RAGE asynchronous shape test manager and initialise the async worker thread.
		if(AssertVerify(!ms_pRageAsyncShapeTestMgr))
		{
			// EJ: Uses more stack for debuggery
#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
			const unsigned stackSize = (32 << 10);
#else // RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
			const unsigned stackSize = (16 << 10);
#endif // RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION

			ms_pRageAsyncShapeTestMgr = rage_new phAsyncShapeTestMgr;
			physicsAssert(ms_pRageAsyncShapeTestMgr);
			const int asyncProbeSchedulerIndex = 0;
			ms_pRageAsyncShapeTestMgr->Init(asyncProbeSchedulerIndex, stackSize);
			ms_pRageAsyncShapeTestMgr->SetWorkerThreadCallBackFunc(MakeFunctorRet(WorldProbe::CShapeTestManager::AsyncWorkerThreadCallBack));

			// NM also uses the RAGE async shape test manager.
			ART::NmRsCharacter::SetAsyncShapeTestMgr(ms_pRageAsyncShapeTestMgr);
		}
	}
	else if(initMode == INIT_SESSION)
	{

	}

#if WORLD_PROBE_DEBUG
	ms_bForceSynchronousShapeTests = PARAM_forcesynchronousshapetests.Get();
#endif // WORLD_PROBE_DEBUG
}

void WorldProbe::CShapeTestManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		if(ms_pRageAsyncShapeTestMgr)
		{
			ms_pRageAsyncShapeTestMgr->Shutdown();
			delete ms_pRageAsyncShapeTestMgr;
			ms_pRageAsyncShapeTestMgr = NULL;
		}

#if WORLD_PROBE_DEBUG
		{
			USE_DEBUG_MEMORY();
			delete [] ms_ShapeDebugPausedRenderBuffer;
		}
#if WORLD_PROBE_FREQUENCY_LOGGING
		m_frequencyLog.Reset();
#endif // WORLD_PROBE_FREQUENCY_LOGGING
#endif // WORLD_PROBE_DEBUG

		WorldProbe::CShapeTestTaskData::ShutdownPool();
	}
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
		//Do nothing.
		//CShapeTestResults structures being destructed will clean things up
	}
}

#if WORLD_PROBE_DEBUG && WORLD_PROBE_FREQUENCY_LOGGING
void WorldProbe::CShapeTestManager::DumpFreqLogToTTY()
{
	CShapeTestManager* pManagerInst = GetShapeTestManager();
	//atStringMap<SFreqLoggingData>::Iterator it(pManagerInst->m_frequencyLog);
	Displayf("*************** WorldProbe shape test statistics ***************");
/*	it = pManagerInst->m_frequencyLog.Begin();
	while(it != pManagerInst->m_frequencyLog.End())
	{
		Displayf("%d times: %s:%d", (*it).nCount, (*it).nCodeFile.c_str(), (*it).nCodeLine);
		it++;
	}*/
	for(int i = 0; i < pManagerInst->m_frequencyLog.GetCount(); ++i)
	{
		SFreqLoggingData* s = pManagerInst->m_frequencyLog.GetItem(i);
		Displayf("%d times: %s:%d", s->nCount, s->nCodeFile.c_str(), s->nCodeLine);
	}
}

void WorldProbe::CShapeTestManager::ClearFreqLog()
{
	CShapeTestManager* pManagerInst = GetShapeTestManager();
	pManagerInst->m_frequencyLog.Reset();
}
#endif // WORLD_PROBE_DEBUG && WORLD_PROBE_FREQUENCY_LOGGING

#if __ASSERT
bool WorldProbe::CShapeTestManager::CanBePerformedAsynchronously(CShapeTestDesc& shapeTestDesc)
{
	switch(shapeTestDesc.GetTestType())
	{
	case DIRECTED_LINE_TEST:
		return true;

	case UNDIRECTED_LINE_TEST:
		return false;	// No SPU job, should work on non PS3 platforms

	case CAPSULE_TEST:
		return false;	// No SPU job, should work on non PS3 platforms

	case SWEPT_SPHERE_TEST:
		return true;

	case POINT_TEST:
		physicsAssertf(shapeTestDesc.GetTestType() != POINT_TEST, "Don't support point shape tests");
		return false;

	case SPHERE_TEST:
		return false;	// No SPU job, should work on non PS3 platforms

	case BOUND_TEST:
		return false;	// No SPU job, should work on non PS3 platforms

	case BOUNDING_BOX_TEST:
		return false;

	case BATCHED_TESTS:
		//All of the tests in a batch will have the same status and therefore the same colour
		return false;	// No SPU job, should work on non PS3 platforms

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Unknown shape test descriptor type: %d.", shapeTestDesc.GetTestType() );
		return false;
	}
}
#endif // __ASSERT

bool WorldProbe::CShapeTestManager::IsAcceptableTimeToIssueShapeTest()
{

#if 0
	static const bool bOnlyIssueRequestsAtAcceptableTimes = false;

	if(!bOnlyIssueRequestsAtAcceptableTimes)
		return true;

	// Don't issue shapetest tasks when animation needs exclusive access to all available cpu cores
	if(fwAnimDirectorComponentMotionTree::IsLockedGlobal())
		return false;

	// Don't issue shapetest tasks when physics needs exclusive access to all available cpu cores
	if(CPhysics::GetIsSimUpdateActive())
		return false;

#endif //0

	// All clear
	return true;
}

void WorldProbe::CShapeTestManager::AbortTest(CShapeTestResults* results)
{
	results->m_ResultsStatus = WorldProbe::TEST_ABORTED;
	bool bInProgress = false;
	bool bAbortThisOne = false;

	ms_RequestedAsyncShapeTestsCs.Lock();

	for(int i = 0; i < ms_RequestedAsyncShapeTests.GetCount(); ++i)
	{
		CShapeTestTaskData* pTaskData = ms_RequestedAsyncShapeTests[i];
		CShapeTestTaskResultsDescriptor& resultsDescriptor = pTaskData->m_ResultsDescriptor;
		if(resultsDescriptor.m_pResults == results)
		{
			resultsDescriptor.m_pResults = NULL; //This signifies that the test was aborted
			bAbortThisOne = true;
			bInProgress = pTaskData->m_TaskHandleCopy != NULL && !sysTaskManager::Poll(pTaskData->m_TaskHandleCopy);
		}

		if(bAbortThisOne)
		{
			Assertf(results->OwnsResults(),
				"If you own the intersections (as opposed to the CShapeTestResults struct owning them), you aren't allowed to abort the test."
				"Culprit: %s:%d", 
				pTaskData->GetCodeFileName(), pTaskData->GetCodeLineNumber());
			if(bInProgress)
			{
				//The user has lost access to their phIntersections for safety reasons. They'll have to allocate more to do another test
				results->m_pResults = NULL;

				//Let it makes its way through the system and get cleaned up as soon as the task has finished
				ms_RequestedAsyncShapeTestsCs.Unlock();
				return;
			}
			else
			{
				//If it hasn't started prevent it from starting
				ms_RequestedAsyncShapeTests.Delete(i);
				ms_RequestedAsyncShapeTestsCs.Unlock();

#if WORLD_PROBE_DEBUG
				//Draw it a different colour to show that it's being deleted
				pTaskData->DebugDrawShape(WorldProbe::TEST_ABORTED);
#endif // WORLD_PROBE_DEBUG
				pTaskData->~CShapeTestTaskData();

				//physicsDisplayf("Frame %u. Aborting shape test with handle 0x%p", gDispatchThreadLoopCounter, pTaskData->m_TaskHandleCopy);

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
				if(pTaskData->m_TaskHandleCopy != NULL)
				{
					int debugRecordIndex = g_DebugRecordTaskManagerInteractionShapeTestHandles.Find(pTaskData->m_TaskHandleCopy);
					Assertf(debugRecordIndex != -1, "Polled for shape test handle that wasn't in the list of handles we own");
					g_DebugRecordTaskManagerInteractionShapeTestHandles.DeleteFast(debugRecordIndex);
				}
#endif // RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION

				{ // scope for critical section
					SYS_CS_SYNC(ms_AsyncShapeTestPoolCs);
					CShapeTestTaskData::GetPool()->Delete(pTaskData);
				}

				//The user now owns the phIntersections again
				return;
			}
		}
	}

	//We couldn't find it
	ms_RequestedAsyncShapeTestsCs.Unlock();
}

bool WorldProbe::CShapeTestManager::AsyncWorkerThreadCallBack()
{
#if 0
	if(!IsAcceptableTimeToIssueShapeTest()) 
		continue;
#endif

	bool bTasksPending = false;

	//Move the pointers to the completed tasks from ms_RequestedAsyncShapeTests into here 
	CShapeTestTaskData* completedTasks[MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS];
	int numCompletedTasks = 0;

	//Loop through the queue making the first MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS entries progress to the processing state
	//Also remove completed shape test tasks from the front of the queue
	//The time spent with the queue locked is minimised
	{
		SYS_CS_SYNC(ms_RequestedAsyncShapeTestsCs);

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
		sysTaskHandle origHandlesInSystem[MAX_SHAPE_TEST_TASK_HANDLES_RECORDED];
		unsigned int origHandlesInSystemCount = 0u;
		if( g_DebugRecordTaskManagerInteractionShapeTestHandles.GetCount() != 0 )
		{
			physicsDisplayf("[Orig Handles %u]", gDispatchThreadLoopCounter);
			for(unsigned int i = 0 ; i < g_DebugRecordTaskManagerInteractionShapeTestHandles.GetCount() ; i++)
			{
				origHandlesInSystem[origHandlesInSystemCount++] = g_DebugRecordTaskManagerInteractionShapeTestHandles[i];
				physicsDisplayf("0x%p ", origHandlesInSystem[i]);
			}
		}

		sysTaskHandle origHandlesInQueue[MAX_SHAPE_TEST_TASK_HANDLES_RECORDED];
		unsigned int origHandlesInQueueCount = 0u;

		if(ms_RequestedAsyncShapeTests.GetCount() != 0)
		{
			physicsDisplayf("[Queued Handles %u]", gDispatchThreadLoopCounter);
			for(unsigned int i = 0 ; i < ms_RequestedAsyncShapeTests.GetCount() ; i++)
			{
				origHandlesInQueue[origHandlesInQueueCount++] = ms_RequestedAsyncShapeTests[i]->m_TaskHandleCopy;
				physicsDisplayf("0x%p ", origHandlesInQueue[i]);
			}
		}
#endif

		for(int i = 0, numProcessing = 0
			; i < ms_RequestedAsyncShapeTests.GetCount() && numProcessing < MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS
			; i++)
		{
			CShapeTestTaskData* pTaskData = ms_RequestedAsyncShapeTests[i];

			if( pTaskData->m_TaskHandleCopy != NULL)
			{
				bool bComplete = sysTaskManager::Poll(pTaskData->m_TaskHandleCopy);

				if(bComplete)
				{
					completedTasks[numCompletedTasks++] = pTaskData;

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
					physicsDisplayf("Frame %u. Poll returned true 0x%p", gDispatchThreadLoopCounter, pTaskData->m_TaskHandleCopy);

					int debugRecordIndex = g_DebugRecordTaskManagerInteractionShapeTestHandles.Find(pTaskData->m_TaskHandleCopy);
					Assertf(debugRecordIndex != -1, "Polled for shape test handle that wasn't in the list of handles we own");
					g_DebugRecordTaskManagerInteractionShapeTestHandles.DeleteFast(debugRecordIndex);
#endif // RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION

					//Remove it from the queue by shuffling everything before it upwards 
					//(which shouldn't be many things because MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS is small and
					// the entries nearer the front of the queue will have started before and so will likely have completed before this one)
					ms_RequestedAsyncShapeTests.DeleteByCopyingUpwards(i);
					i--;
				}
				else
				{
					bTasksPending = true;
					numProcessing++;
				}
			}
			else
			{
				bTasksPending = true;
				pTaskData->m_TaskHandleCopy = pTaskData->CreateTask(m_iShapeTestAsyncSchedulerIndex);

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
				physicsDisplayf("Frame %u. Created task 0x%p", gDispatchThreadLoopCounter, pTaskData->m_TaskHandleCopy);

				Assertf(g_DebugRecordTaskManagerInteractionShapeTestHandlesHighWaterMark <= MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS,
					"Created a shape test task when there were already MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS in the system");
#endif

				if(!physicsVerifyf(pTaskData->m_TaskHandleCopy, "phShapeTestTaskData::CreateTask failed"))
				{
					//Try again next time. We could alternatively ditch this shape test. Things are bad if this just happened.
					break;
				}
				else
				{

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
					if(pTaskData->m_TaskHandleCopy)
					{
						g_DebugRecordTaskManagerInteractionShapeTestHandles.Append() = pTaskData->m_TaskHandleCopy;
						g_DebugRecordTaskManagerInteractionShapeTestHandlesHighWaterMark = Max(g_DebugRecordTaskManagerInteractionShapeTestHandlesHighWaterMark, (unsigned int)g_DebugRecordTaskManagerInteractionShapeTestHandles.GetCount());
					}
#endif
					numProcessing++;
				}
			}
		}

		//Give the client their results and destruct the task data
		for(int i = 0 ; i < numCompletedTasks ; i++)
		{
			completedTasks[i]->ReturnResults(&completedTasks[i]->m_ResultsDescriptor,1);
			completedTasks[i]->~CShapeTestTaskData();
		}

	}

	//Lock the pool and free the task data because the results have been returned
	//The time spent with the pool locked is minimised
	{
		SYS_CS_SYNC(ms_AsyncShapeTestPoolCs);
		for(int i = 0 ; i < numCompletedTasks ; i++)
		{
			CShapeTestTaskData::GetPool()->Delete(completedTasks[i]);
		}
	}

#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
	gDispatchThreadLoopCounter++;
#endif

	return (!bTasksPending && ms_RequestedAsyncShapeTests.GetCount() == 0);
}

/////////////////////////////////////////////////////////////////// SUBMIT TEST ///////////////////////////////////////////////////////////////////

#if WORLD_PROBE_DEBUG
bool WorldProbe::CShapeTestManager::DebugSubmitTest(const char *strCodeFile, u32 nCodeLine, CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode)
#else // WORLD_PROBE_DEBUG
bool WorldProbe::CShapeTestManager::SubmitTest(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode)
#endif // WORLD_PROBE_DEBUG
{
	BANK_ONLY(IF_COMMERCE_STORE_RETURN_FALSE());

#if WORLD_PROBE_DEBUG && WORLD_PROBE_FREQUENCY_LOGGING
	Assert( m_frequencyLog.GetCount() < MAX_SHAPETEST_CALLS_FOR_LOGGING );
	if(m_frequencyLog.Has(strCodeFile))
	{
		m_frequencyLog[strCodeFile].nCount += 1;
	}
	else
	{
		SFreqLoggingData newElement;
		newElement.nCodeFile = atString(strCodeFile);
		newElement.nCodeLine = nCodeLine;
		newElement.nCount = 1;

		m_frequencyLog.InsertSorted(strCodeFile, newElement);
	}
#endif // WORLD_PROBE_DEBUG && WORLD_PROBE_FREQUENCY_LOGGING

#if __ASSERT
	if( blockingMode == PERFORM_ASYNCHRONOUS_TEST )
	{
		// The only way it would be safe for the user to own the results of an async shape test is if they guaranteed that the
		// results wouldn't be deleted until the shape test was done. Otherwise they could delete the results with the async shape test manager
		// thread still writing to them. Nobody is doing that at the moment and I doubt they ever will. This assert will let us instantly
		// catch shape tests that will inevitably lead to uncommon timing dependent crashes.
		Assertf(shapeTestDesc.GetResultsStructure() == NULL || shapeTestDesc.GetResultsStructure()->OwnsResults(),
			"You aren't allowed to submit async shapetests with results that aren't owned by the CShapeTestResults.");
		if(
#if WORLD_PROBE_DEBUG
			ms_bForceSynchronousShapeTests || 
#endif // WORLD_PROBE_DEBUG
			!physicsVerifyf(CanBePerformedAsynchronously(shapeTestDesc),"Attempting to perform a %s asynchronously", GetShapeTypeString(shapeTestDesc.GetTestType())))
		{
			blockingMode = PERFORM_SYNCHRONOUS_TEST;
		}
	}

	if(CShapeTestResults* pShapeTestResults = shapeTestDesc.GetResultsStructure())
	{
		Assert(pShapeTestResults->HasResultsAllocated());
		Assert(pShapeTestResults->GetResultsStatus() != WAITING_ON_RESULTS);
	}
	else
	{
		Assert(blockingMode == PERFORM_SYNCHRONOUS_TEST);
	}
#endif


	if( blockingMode == PERFORM_SYNCHRONOUS_TEST )
	{
		StartProbeTimer(shapeTestDesc.GetContext());

		//Allocate the shape test on the stack
		CShapeTestTaskData shapeTestTaskData(shapeTestDesc, blockingMode 
#if WORLD_PROBE_DEBUG
			, strCodeFile, nCodeLine
#endif
			);

		phShapeTestCullResults cullResults;
		if(shapeTestDesc.m_eShapeTestDomain == TEST_AGAINST_INDIVIDUAL_OBJECTS)
		{
			// Set up some previous cull results for the shapetest to read
			u16 numInstancesToTest = (u16)shapeTestDesc.m_aIncludeInstList.GetCount();
			cullResults.SetInstanceArray(Alloca(phShapeTestCullResults::CulledInstance,numInstancesToTest),numInstancesToTest);
			cullResults.CopyInstanceArray(shapeTestDesc.m_aIncludeInstList.GetElements(),numInstancesToTest);
			cullResults.SetResultLevel(phShapeTestCullResults::RESULTLEVEL_INSTANCE);
			shapeTestTaskData.m_ShapeTest.SetShapeTestCullResults(&cullResults,phShapeTestCull::READ);
		}
		else if((shapeTestDesc.GetOptions() & LOS_TEST_VEH_TYRES) != 0)
		{
			// Set up the shapetest to store off all of the instances it encounters. 
			physicsAssertf((shapeTestDesc.GetIncludeFlags() & ArchetypeFlags::GTA_WHEEL_TEST),"Using LOS_TEST_VEH_TYRES and TEST_IN_LEVEL requires including ArchetypeFlags::GTA_WHEEL_TEST.");
			u16 maxInstancesToSave = 256;
			cullResults.SetInstanceArray(Alloca(phShapeTestCullResults::CulledInstance,maxInstancesToSave),maxInstancesToSave);
			cullResults.SetResultLevel(phShapeTestCullResults::RESULTLEVEL_INSTANCE);
			shapeTestTaskData.m_ShapeTest.SetShapeTestCullResults(&cullResults,phShapeTestCull::WRITE);
		}

		int numResultsDescriptors;
		CShapeTestTaskResultsDescriptor* pResultsDescriptors;
		if(shapeTestDesc.GetTestType() != BATCHED_TESTS)
		{
			if(shapeTestDesc.GetResultsStructure() != NULL)
			{
				numResultsDescriptors = 1;
				pResultsDescriptors = &shapeTestTaskData.m_ResultsDescriptor;
			}
			else
			{
				numResultsDescriptors = 0;
				pResultsDescriptors = NULL;
			}
		}
		else
		{
			// If this is a batch test we need to allocate extra result descriptors on the stack
			WorldProbe::CShapeTestBatchDesc& batchTestDesc = static_cast< WorldProbe::CShapeTestBatchDesc& >(shapeTestDesc);
			numResultsDescriptors = batchTestDesc.GetNumShapeTests();
			pResultsDescriptors = Alloca(CShapeTestTaskResultsDescriptor,batchTestDesc.GetNumShapeTests());
			CShapeTestTaskData::InitBatchedResultsDescriptors(batchTestDesc,pResultsDescriptors);
		}

		int numHits = shapeTestTaskData.PerformTest();
		shapeTestTaskData.ReturnResults(pResultsDescriptors,numResultsDescriptors);

		StopProbeTimer(shapeTestDesc.GetContext());

		return numHits != 0;
	}
	else //(blockingMode == PERFORM_ASYNCHRONOUS_TEST)
	{
		StartProbeTimer(shapeTestDesc.GetContext());

		Assertf(shapeTestDesc.m_eShapeTestDomain != TEST_AGAINST_INDIVIDUAL_OBJECTS,
			"TEST_AGAINST_INDIVIDUAL_OBJECTS isn't supported for async probes.");

		//Allocation in the pool
		//Allocation but not construction to minimise the time spent with the pool locked
		CShapeTestTaskData* pShapeTestTaskData = NULL;
		{
			SYS_CS_SYNC(ms_AsyncShapeTestPoolCs);
			pShapeTestTaskData = CShapeTestTaskData::GetPool()->GetNoOfFreeSpaces() != 0 ? CShapeTestTaskData::GetPool()->New() : NULL;
		}

		//Construction
		if( !physicsVerifyf(pShapeTestTaskData, "Unable to allocate a CShapeTestTaskData" ))
		{
			shapeTestDesc.GetResultsStructure()->m_ResultsStatus = WorldProbe::SUBMISSION_FAILED;

			StopProbeTimer(shapeTestDesc.GetContext());

			return false;
		}

		//Construction using global placement operator new
		::new(pShapeTestTaskData) CShapeTestTaskData(shapeTestDesc, blockingMode 
#if WORLD_PROBE_DEBUG
			, strCodeFile, nCodeLine
#endif
			);

		shapeTestDesc.GetResultsStructure()->m_ResultsStatus = WorldProbe::WAITING_ON_RESULTS;

#if WORLD_PROBE_DEBUG
		pShapeTestTaskData->DebugDrawShape(WorldProbe::WAITING_ON_RESULTS);
#endif

		//Movement into the queue
		{
			SYS_CS_SYNC(ms_RequestedAsyncShapeTestsCs);
			physicsAssertf(!ms_RequestedAsyncShapeTests.IsFull(),
				"Async shape test request queue is full despite a pool slot being free. How is this possible?");
			ms_RequestedAsyncShapeTests.Append() = pShapeTestTaskData;
		}

		StopProbeTimer(shapeTestDesc.GetContext());

		return true;
	}
}
