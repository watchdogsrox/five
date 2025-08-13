#ifndef SHAPETEST_TASK_H
#define SHAPETEST_TASK_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/shapetest.h"
#include "physics/WorldProbe/debug.h"

namespace WorldProbe
{

	// One of these per shape test. If it is a batched shape test then there is one per test within the batch.
	class CShapeTestTaskResultsDescriptor
	{
	public:

		CShapeTestTaskResultsDescriptor() {}
		CShapeTestTaskResultsDescriptor(WorldProbe::CShapeTestResults* pResults, phIntersection* pIntersections,
			int firstResultIntersectionIndex, int numResultIntersections)
			: m_pResults(pResults), m_pIntersections(pIntersections)
			, m_FirstResultIntersectionIndex(firstResultIntersectionIndex)
			, m_NumResultIntersections(numResultIntersections) {}

#if WORLD_PROBE_DEBUG
		void DebugDrawResults(ELosContext eLOSContext);
#endif // WORLD_PROBE_DEBUG


	public:
		WorldProbe::CShapeTestResults* m_pResults;
		// The subset of the phIntersection array to write to.
		int m_FirstResultIntersectionIndex;
		int m_NumResultIntersections;
		// We take temporary ownership of the phIntersections while the task is in progress. If some descriptors share a
		// CShapeTestResults, this will be NULL for all but one of them.
		phIntersection* m_pIntersections;
	};

	// All of the data necessary to do a shape test synchronously or asynchronously. When doing it synchronously, the
	// phShapeTestTaskData (the base class) is not initialised or used.
	class CShapeTestTaskData : public phShapeTestTaskData
	{
	public:
		FW_REGISTER_CLASS_POOL(CShapeTestTaskData);

#if WORLD_PROBE_DEBUG
		CShapeTestTaskData(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode, const char *strCodeFile, u32 nCodeLine );
#else // WORLD_PROBE_DEBUG
		CShapeTestTaskData(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode );
#endif // WORLD_PROBE_DEBUG
		~CShapeTestTaskData() {}

		// Both return number of phIntersections.
		int PerformTest();
		void ReturnResults(WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors);

#if WORLD_PROBE_DEBUG
		float GetCompletionTime() const;
		void DebugDrawShape(WorldProbe::eResultsState resultsStatus);
		void ConstructPausedRenderData(WorldProbe::CShapeTestDebugRenderData* outRenderData,
			WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors) const;
#endif // WORLD_PROBE_DEBUG

		static void InitBatchedResultsDescriptors(WorldProbe::CShapeTestBatchDesc& batchTestDesc,
			WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors); 

#if __BANK
#if WORLD_PROBE_DEBUG
		const char* GetCodeFileName() const { return m_DebugData.m_strCodeFile; }
		int GetCodeLineNumber() const { return m_DebugData.m_nCodeLine; }
#else // WORLD_PROBE_DEBUG
		const char* GetCodeFileName() const { return "NoDebugData"; }
		int GetCodeLineNumber() const { return 0; }
#endif // WORLD_PROBE_DEBUG
#endif // __BANK
	private:
		// Results processing.
		int FilterAndSortResults(CShapeTestTaskResultsDescriptor* pResultsDescriptors, int resultsDescriptorIndex);
		void AddIntersectionToSortedResults(phIntersection* pInOutIntersections, int nNumIntersections, phIntersection* insertand);


	public:
		// The phShapeTest<X> (which may be a phShapeTest<phShapeBatch>).
		CShapeTest m_ShapeTest;
		// Array of 1 unless it's a batch test, in which case there is one for each test in the batch.
		CShapeTestTaskResultsDescriptor m_ResultsDescriptor;
		WorldProbe::eBlockingMode m_BlockingMode;
#if WORLD_PROBE_DEBUG
		CShapeTestTaskDebugData m_DebugData;
#endif // WORLD_PROBE_DEBUG
		// Keep a copy because m_TaskHandle in the phShapeTestTaskData gets written to by SPU at unpredictable times.
		sysTaskHandle m_TaskHandleCopy;
	};

} // namespace WorldProbe

#endif // SHAPETEST_TASK_H
