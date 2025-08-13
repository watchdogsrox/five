// WorldProbe headers:
#include "physics/WorldProbe/shapetesttask.h"

// RAGE headers:
#include "phbound/boundcomposite.h"

// Game headers:
#include "network/live/livemanager.h"
#include "Peds/ped.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "scene/Entity.h"
#include "vehicles/Automobile.h"
#include "vehicles/Boat.h"
#include "Vehicles/vehicle.h"

PHYSICS_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(WorldProbe::CShapeTestTaskData, MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS, atHashString("ShapeTestTaskData",0x4a35d452));

//Assumes at least one free slot after the specified range
void WorldProbe::CShapeTestTaskData::AddIntersectionToSortedResults(phIntersection* pInOutIntersections, int nNumIntersections,
																	 phIntersection* insertand)
{
	Assert(pInOutIntersections);
	Assert(insertand);

	for(int i = 0; i < nNumIntersections; i++)
	{
		if(insertand->GetT() < pInOutIntersections[i].GetT())
		{
			for(int j = nNumIntersections ; j > i ; j--)
			{
				pInOutIntersections[j] = pInOutIntersections[j-1];
			}

			pInOutIntersections[i] = *insertand;
			return;
		}
	}

	if(physicsVerifyf(nNumIntersections >= 0, "Bad index into intersection array: nNumIntersections=%d.", nNumIntersections))
	{
		pInOutIntersections[nNumIntersections] = *insertand;
	}
}

int WorldProbe::CShapeTestTaskData::FilterAndSortResults(CShapeTestTaskResultsDescriptor* pResultsDescriptors, int resultsDescriptorIndex)
{
	CShapeTestTaskResultsDescriptor& resultsDescriptor = pResultsDescriptors[resultsDescriptorIndex];
	int numHits = m_ShapeTest.GetNumHits(resultsDescriptorIndex);
	Assert(numHits <= resultsDescriptor.m_NumResultIntersections);
	physicsAssert(resultsDescriptor.m_pResults);
	CShapeTestHitPoint* pInOutIntersections = &resultsDescriptor.m_pResults->m_pResults[resultsDescriptor.m_FirstResultIntersectionIndex];
	physicsAssert(pInOutIntersections);

	for(int i=0; i < numHits; ++i)
	{
		// We may still hold the level index to a recently deleted object if we are looking at the results of an asynchronous test. Don't
		// do anything special here because the user code should be wary of invalid results like this anyway.
		Assertf(pInOutIntersections[i].IsAHit() || m_BlockingMode==WorldProbe::PERFORM_ASYNCHRONOUS_TEST,
			"level index=%d, test issued from: '%s:%d'.", pInOutIntersections[i].GetLevelIndex(), GetCodeFileName(), GetCodeLineNumber());

		Assert((resultsDescriptor.m_FirstResultIntersectionIndex+i <= resultsDescriptor.m_pResults->GetSize())
			|| i==0);
		if(m_ShapeTest.RequiresResultSorting())
		{
			phIntersection insertand = pInOutIntersections[i];
			AddIntersectionToSortedResults(pInOutIntersections, i, &insertand);
		}
	}

	return numHits;
}

#if WORLD_PROBE_DEBUG
float WorldProbe::CShapeTestTaskData::GetCompletionTime() const
{
#if PROFILE_INDIVIDUAL_SHAPETESTS
	switch(m_ShapeTest.GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:	return reinterpret_cast< const phShapeTest<phShapeProbe>* >(&m_ShapeTest)->GetCompletionTime();
	case UNDIRECTED_LINE_TEST:	return reinterpret_cast< const phShapeTest<phShapeEdge>* >(&m_ShapeTest)->GetCompletionTime();
	case CAPSULE_TEST:			return reinterpret_cast< const phShapeTest<phShapeCapsule>* >(&m_ShapeTest)->GetCompletionTime();
	case SWEPT_SPHERE_TEST:		return reinterpret_cast< const phShapeTest<phShapeSweptSphere>* >(&m_ShapeTest)->GetCompletionTime();
	case SPHERE_TEST:			return reinterpret_cast< const phShapeTest<phShapeSphere>* >(&m_ShapeTest)->GetCompletionTime();
	case BOUND_TEST:			return reinterpret_cast< const phShapeTest<phShapeObject>* >(&m_ShapeTest)->GetCompletionTime();
	case BATCHED_TESTS:			return reinterpret_cast< const phShapeTest<phShapeBatch>* >(&m_ShapeTest)->GetCompletionTime();
	default: return -1.0f;
	}
#else // PROFILE_INDIVIDUAL_SHAPETESTS
	return -1.0f;
#endif // PROFILE_INDIVIDUAL_SHAPETESTS
}

void WorldProbe::CShapeTestTaskData::DebugDrawShape(WorldProbe::eResultsState UNUSED_PARAM(resultsStatus))
{
	IF_COMMERCE_STORE_RETURN();

	if(!WorldProbe::GetShapeTestManager()->ms_abContextFilters[m_DebugData.m_eLOSContext])
		return;

	if( m_BlockingMode == PERFORM_ASYNCHRONOUS_TEST
		&& !WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled)
		return;

	if( m_BlockingMode == PERFORM_SYNCHRONOUS_TEST
		&& !WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled)
		return;

	// TODO: Add toggle to choose between using colour to debug asynchronous test status and telling synchronous/asynchronous tests apart.
	rage::Color32 colour = (m_BlockingMode==WorldProbe::PERFORM_ASYNCHRONOUS_TEST) ? Color_yellow : Color_blue;
	//rage::Color32 colour = CShapeTestResults::GetDebugDrawColour(resultsStatus);

	ASSERT_ONLY(
		int nDbgStringLength = istrlen(m_DebugData.m_strCodeFile) + 1
		+ int(log((float)m_DebugData.m_nCodeLine));
	)

	char debugText[1024]; physicsAssertf(nDbgStringLength < 1024, "Shape test function name buffer is not large enough for debug string.");
	formatf(debugText, "%s:%d [%i]", m_DebugData.m_strCodeFile, m_DebugData.m_nCodeLine, (int)GetCompletionTime());

	switch(m_ShapeTest.GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:
	case UNDIRECTED_LINE_TEST:
		return m_ShapeTest.DrawLine(colour, debugText);

	case CAPSULE_TEST:
	case SWEPT_SPHERE_TEST:
		return m_ShapeTest.DrawCapsule(colour, debugText);

	case POINT_TEST:
		physicsAssertf(m_ShapeTest.GetShapeTestType() != POINT_TEST, "Don't support point shape tests");
		return;

	case SPHERE_TEST:
		return m_ShapeTest.DrawSphere(colour, debugText);

	case BOUND_TEST:
		return m_ShapeTest.DrawBound(colour, debugText);

	case BOUNDING_BOX_TEST:
		return m_ShapeTest.DrawBoundingBox(colour, debugText);

	case BATCHED_TESTS:
		//All of the tests in a batch will have the same status and therefore the same colour
		return m_ShapeTest.DrawBatch(colour, debugText);

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Unknown shape test descriptor type: %d.", m_ShapeTest.GetShapeTestType());
		return;
	}
}
#endif // WORLD_PROBE_DEBUG

#if WORLD_PROBE_DEBUG
void WorldProbe::CShapeTestTaskData::ConstructPausedRenderData(WorldProbe::CShapeTestDebugRenderData* outRenderData, WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors) const
{
	for(u32 i = 0 ; i < numResultsDescriptors; i++)
	{
		outRenderData[i].m_eLOSContext = m_DebugData.m_eLOSContext;
		outRenderData[i].m_strCodeFile = m_DebugData.m_strCodeFile;
		outRenderData[i].m_nCodeLine = m_DebugData.m_nCodeLine;

		// TODO: Add toggle to choose between using colour to debug asynchrnous test status and telling synchronous/asynchronous tests apart.
		//		rage::Color32 colour = CShapeTestResults::GetDebugDrawColour(resultsStatus);
		rage::Color32 colour = (m_BlockingMode==WorldProbe::PERFORM_ASYNCHRONOUS_TEST) ? Color_yellow : Color_blue;

		outRenderData[i].m_Colour = colour;
		outRenderData[i].m_bPartOfBatch = (m_ShapeTest.GetShapeTestType() == BATCHED_TESTS);
		outRenderData[i].m_ShapeTestType = m_ShapeTest.GetShapeTestType();
		outRenderData[i].m_bAsyncTest = (m_BlockingMode == WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		outRenderData[i].m_CompletionTime = GetCompletionTime();
	}

	switch(m_ShapeTest.GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:
	case UNDIRECTED_LINE_TEST:
		m_ShapeTest.ConstructPausedRenderDataLine(*outRenderData);
		break;

	case CAPSULE_TEST:
	case SWEPT_SPHERE_TEST:
		m_ShapeTest.ConstructPausedRenderDataCapsule(*outRenderData);
		break;

	case POINT_TEST:
		physicsAssertf(m_ShapeTest.GetShapeTestType() != POINT_TEST, "Don't support point shape tests");
		break;

	case SPHERE_TEST:
		m_ShapeTest.ConstructPausedRenderDataSphere(*outRenderData);
		break;

	case BOUND_TEST:
		m_ShapeTest.ConstructPausedRenderDataBound(*outRenderData);
		break;

	case BOUNDING_BOX_TEST:
		m_ShapeTest.ConstructPausedRenderDataBoundingBox(*outRenderData);
		break;

	case BATCHED_TESTS:
		//All of the tests in a batch will have the same status and therefore the same colour
		m_ShapeTest.ConstructPausedRenderDataBatch(outRenderData);
		break;

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Unknown shape test descriptor type: %d.", m_ShapeTest.GetShapeTestType());
		break;
	}

	int numRenderedIntersections = 0;

	for(u32 i = 0 ; i < numResultsDescriptors; i++)
	{
		CShapeTestTaskResultsDescriptor const * pResultsDescriptor = &pResultsDescriptors[i];

		bool aborted = (pResultsDescriptor->m_pResults == NULL);

		// Don't try to access results if this shape test was aborted or if the results structure has already been deleted.
		if(!aborted && pResultsDescriptor->m_pResults->m_pResults)
		{
			CShapeTestHitPoint* pInOutIntersections = &pResultsDescriptor->m_pResults->m_pResults[pResultsDescriptor->m_FirstResultIntersectionIndex];

			for(int j=0; j < pResultsDescriptor->m_NumResultIntersections && pInOutIntersections[j].IsAHit() && numRenderedIntersections < CShapeTestManager::ms_bMaxIntersectionsToDraw; ++j)
			{
				outRenderData->m_Intersections[numRenderedIntersections++] = pInOutIntersections[j];
			}
		}
	}

	for(int unusedIntersectionIndex = numRenderedIntersections; unusedIntersectionIndex < CShapeTestManager::ms_bMaxIntersectionsToDraw; ++unusedIntersectionIndex)
	{
		outRenderData->m_Intersections[unusedIntersectionIndex].Zero();
	}
}
#endif // WORLD_PROBE_DEBUG

void WorldProbe::CShapeTestTaskData::InitBatchedResultsDescriptors(WorldProbe::CShapeTestBatchDesc& batchTestDesc, WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors)
{
	int shapeIndex = 0;
	for(int i = 0 ; i < batchTestDesc.m_nNumProbes; i++)
	{
		WorldProbe::CShapeTestProbeDesc& probeTestDesc = batchTestDesc.m_pProbeDescriptors[i];

		CShapeTestResults* pResults = probeTestDesc.GetResultsStructure(); //The probes in the batch may or may not share results structures
		phIntersection* pIntersections = pResults->m_pResults;

		//This is to avoid us having duplicate pointers to phIntersection arrays
		pResults->m_pResults = NULL;

		pResultsDescriptors[shapeIndex++] = CShapeTestTaskResultsDescriptor( pResults, pIntersections, probeTestDesc.m_nFirstFreeIntersectionIndex, probeTestDesc.m_nNumIntersections );
	}

	for(int i = 0 ; i < batchTestDesc.m_nNumCapsules; i++)
	{
		WorldProbe::CShapeTestCapsuleDesc& capsuleTestDesc = batchTestDesc.m_pCapsuleDescriptors[i];

		CShapeTestResults* pResults = capsuleTestDesc.GetResultsStructure(); //The probes in the batch may or may not share results structures
		phIntersection* pIntersections = pResults->m_pResults;

		//This is to avoid us having duplicate pointers to phIntersection arrays
		pResults->m_pResults = NULL;

		pResultsDescriptors[shapeIndex++] = CShapeTestTaskResultsDescriptor( pResults, pIntersections, capsuleTestDesc.m_nFirstFreeIntersectionIndex, capsuleTestDesc.m_nNumIntersections );
	}

	Assert(shapeIndex == batchTestDesc.GetNumShapeTests());
	//This is so the results don't lose their phIntersection pointers
	for(int i = 0 ; i < batchTestDesc.GetNumShapeTests() ; i++)
	{
		if( pResultsDescriptors[i].m_pIntersections )
			pResultsDescriptors[i].m_pResults->m_pResults = (CShapeTestHitPoint*)pResultsDescriptors[i].m_pIntersections;
	}
}

#if WORLD_PROBE_DEBUG
WorldProbe::CShapeTestTaskData::CShapeTestTaskData(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode, const char *strCodeFile, u32 nCodeLine )
#else
WorldProbe::CShapeTestTaskData::CShapeTestTaskData(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode )
#endif
: m_ShapeTest()
, m_BlockingMode( blockingMode ) 
#if WORLD_PROBE_DEBUG
, m_DebugData(strCodeFile, nCodeLine, shapeTestDesc.GetContext()) 
#endif
, m_TaskHandleCopy(sysTaskHandle(NULL))
{
	// A few checks to warn the user about anything they have forgotten to fill in for the descriptor:
	physicsAssertf(shapeTestDesc.m_pResults || blockingMode==PERFORM_SYNCHRONOUS_TEST,
		"WARNING: A result structure must be associated with an asynchronous shape test!");
	// Make this assert fatal as we risk stomping memory otherwise.
	physicsFatalAssertf(!shapeTestDesc.m_pResults ||
		shapeTestDesc.m_nFirstFreeIntersectionIndex+shapeTestDesc.m_nNumIntersections <= shapeTestDesc.m_pResults->GetSize(),
		"WARNING: Number of results set exceed the size of the results structure! Look at values given in SetFirstFreeResultOffset() and SetMaxNumResultsToUse() on shape test descriptor");

	// We don't need this data initialised when doing a synchronous test
	if( blockingMode == PERFORM_ASYNCHRONOUS_TEST )
		phShapeTestTaskData::InitFromDefault();

	//Construct the shape test
	switch( shapeTestDesc.GetTestType() )
	{
	case DIRECTED_LINE_TEST:
		physicsAssertf(((shapeTestDesc.GetOptions()&LOS_TEST_VEH_TYRES) == 0) 
			|| blockingMode == PERFORM_SYNCHRONOUS_TEST, "PerformLineAgainstVehicleTyresTest has not been parallelised");
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestProbeDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetProbe( *reinterpret_cast< phShapeTest<phShapeProbe>* >(&m_ShapeTest) );
		break;

	case UNDIRECTED_LINE_TEST:
		physicsAssertf(((shapeTestDesc.GetOptions()&LOS_TEST_VEH_TYRES) == 0) 
			|| blockingMode == PERFORM_SYNCHRONOUS_TEST, "PerformLineAgainstVehicleTyresTest has not been parallelised");
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestProbeDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetEdge( *reinterpret_cast< phShapeTest<phShapeEdge>* >(&m_ShapeTest) );
		break;

	case CAPSULE_TEST:
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestCapsuleDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetCapsule( *reinterpret_cast< phShapeTest<phShapeCapsule>* >(&m_ShapeTest) );
		break;

	case SWEPT_SPHERE_TEST:
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestCapsuleDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetSweptSphere( *reinterpret_cast< phShapeTest<phShapeSweptSphere>* >(&m_ShapeTest) );
		break;

	case POINT_TEST:
		physicsAssertf(shapeTestDesc.GetTestType() != POINT_TEST, "Point test unsupported");
		break;

	case SPHERE_TEST:
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestSphereDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetSphere( *reinterpret_cast< phShapeTest<phShapeSphere>* >(&m_ShapeTest) );
		break;

	case BOUND_TEST:
		physicsAssertf(shapeTestDesc.GetTestType() != BOUND_TEST || blockingMode != PERFORM_ASYNCHRONOUS_TEST,
			"Can't do async bound test because phShapeTestTaskData doesn't support it");
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestBoundDesc& >(shapeTestDesc));
		break;

	case BOUNDING_BOX_TEST:
		physicsAssertf(shapeTestDesc.GetTestType() != BOUNDING_BOX_TEST || blockingMode != PERFORM_ASYNCHRONOUS_TEST, "Don't support doing bounding box test asynchronously");
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestBoundingBoxDesc& >(shapeTestDesc));
		break;

	case BATCHED_TESTS:
		physicsAssertf(((shapeTestDesc.GetOptions()&LOS_TEST_VEH_TYRES) == 0) 
			|| blockingMode == PERFORM_SYNCHRONOUS_TEST, "PerformLineAgainstVehicleTyresTest has not been parallelised");
		new(&m_ShapeTest) CShapeTest(static_cast< WorldProbe::CShapeTestBatchDesc& >(shapeTestDesc));
		if( blockingMode == PERFORM_ASYNCHRONOUS_TEST ) 
			phShapeTestTaskData::SetBatch( *reinterpret_cast< phShapeTest<phShapeBatch>* >(&m_ShapeTest) );
		break;

	case INVALID_TEST_TYPE:
	default: // Intentional fall-through.
		physicsAssertf(false, "Unknown shape test descriptor type: %d.", shapeTestDesc.GetTestType());
	}

	//Construct the results descriptor
	CShapeTestResults* pResults = shapeTestDesc.GetResultsStructure();
	Assertf(pResults != NULL || blockingMode == PERFORM_SYNCHRONOUS_TEST, "Boolean tests with async probes aren't supported");
	if(pResults)
	{
		physicsAssertf(pResults->m_pResults != NULL, "The results structure has a NULL phIntersection pointer");

		//Take ownership of the phIntersections until the results are returned. This is necessary because it's possible to delete the CShapeTestResults and/or abort
		//a task that will write to the phIntersections while the task is in progress.
		phIntersection* pIntersections = pResults->m_pResults;
		m_ResultsDescriptor = CShapeTestTaskResultsDescriptor(pResults, pIntersections, shapeTestDesc.m_nFirstFreeIntersectionIndex, shapeTestDesc.m_nNumIntersections);
	}
	else
	{
		m_ResultsDescriptor = CShapeTestTaskResultsDescriptor(NULL,NULL,0,0);
	}
}

int WorldProbe::CShapeTestTaskData::PerformTest()
{
	switch(m_ShapeTest.GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:
		return m_ShapeTest.PerformDirectedLineTest();

	case UNDIRECTED_LINE_TEST:
		return m_ShapeTest.PerformUndirectedLineTest();

	case CAPSULE_TEST:
		return m_ShapeTest.PerformCapsuleTest();

	case SWEPT_SPHERE_TEST:
		return m_ShapeTest.PerformSweptSphereTest();

	case POINT_TEST:
		physicsAssertf(m_ShapeTest.GetShapeTestType() != POINT_TEST, "Don't support point shape tests");
		return 0;

	case SPHERE_TEST:
		return m_ShapeTest.PerformSphereTest();

	case BOUND_TEST:
		return m_ShapeTest.PerformBoundTest();

	case BOUNDING_BOX_TEST:
		return m_ShapeTest.PerformBoundingBoxTest();

	case BATCHED_TESTS:
		return m_ShapeTest.PerformBatchTest();

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Unknown shape test descriptor type: %d.", m_ShapeTest.GetShapeTestType());
		return 0;
	}
}

void WorldProbe::CShapeTestTaskData::ReturnResults(CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors)
{
	bool aborted = false;

	for(int i=0; i < numResultsDescriptors; ++i)
	{
		CShapeTestTaskResultsDescriptor& resultsDescriptor = pResultsDescriptors[i];

		//The test was aborted if m_pResults == NULL. We took ownership of the phIntersections
		if(resultsDescriptor.m_pResults) 
		{
			int nNumHits;

			if( m_ShapeTest.GetShapeTestType() == BOUNDING_BOX_TEST )
			{
				nNumHits = reinterpret_cast<CBoundingBoxShapeTestData *>(&m_ShapeTest)->m_nNumHits;
			}
			else
			{
				nNumHits = FilterAndSortResults(pResultsDescriptors,i);
			}

			resultsDescriptor.m_pResults->m_nNumHits += (u8)nNumHits;

#if WORLD_PROBE_DEBUG
			if( (m_BlockingMode == PERFORM_ASYNCHRONOUS_TEST && WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled) 
				|| (m_BlockingMode == PERFORM_SYNCHRONOUS_TEST && WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled) )
				resultsDescriptor.DebugDrawResults(m_DebugData.m_eLOSContext);
#endif


		}
		else
		{
			aborted = true;

			//The task was aborted whilst in progress. We took ownership of the phIntersections
			if(resultsDescriptor.m_pIntersections)
			{
				sysMemStartTemp();
				{
					USE_MEMBUCKET(MEMBUCKET_PHYSICS);
					delete [] resultsDescriptor.m_pIntersections;
				}
				sysMemEndTemp();
			}
		}
	}

#if WORLD_PROBE_DEBUG
	DebugDrawShape(aborted ? WorldProbe::TEST_ABORTED : WorldProbe::RESULTS_READY);
	GetShapeTestManager()->AddToDebugPausedRenderBuffer(*this, pResultsDescriptors, numResultsDescriptors);
#endif

	if(!aborted)
	{
		sys_lwsync(); //Ensure the the phIntersections and other fields are written to before the reader is informed

		for(int i=0; i < numResultsDescriptors; ++i)
		{
			CShapeTestTaskResultsDescriptor& resultsDescriptor = pResultsDescriptors[i];
			resultsDescriptor.m_pResults->m_ResultsStatus = WorldProbe::RESULTS_READY;
		}
	}
}
