#ifndef SHAPETEST_MGR_H
#define SHAPETEST_MGR_H

// RAGE headers:
#include "atl/binmap.h"
#include "physics/asyncshapetestmgr.h"

// Game headers:
#include "physics/WorldProbe/wpcommon.h"

#define ENABLE_ASYNC_STRESS_TEST (0 && __BANK)

// Forward declarations:
namespace WorldProbe
{
	class CShapeTestResults;
	class CShapeTestTaskData;
	class CShapeTestTaskResultsDescriptor;
}


namespace WorldProbe
{

	///////////////////////
	// Shape test manager.
	///////////////////////
	class CShapeTestManager
	{
	public:
		CShapeTestManager()
#if WORLD_PROBE_DEBUG
			: ms_abContextFilters(WorldProbe::LOS_NumContexts)
#endif // WORLD_PROBE_DEBUG
		{
		}

	public:
		static CShapeTestManager* GetShapeTestManager() { return &sm_shapeTestMgrInst; }

#if !WORLD_PROBE_DEBUG
		// For synchronous tests: Returns true/false to indicate whether any hits were detected. This can be useful if a NULL results structure is used.
		// For asynchronous tests: Returns whether the request creation succeeded
		bool SubmitTest(CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode = PERFORM_SYNCHRONOUS_TEST);
#endif // !WORLD_PROBE_DEBUG

		void Init(unsigned initMode);
		void Shutdown(unsigned shutdownMode);

		// This function will get called from the RAGE asynchronous shape test manager's dispatcher thread.
		static bool AsyncWorkerThreadCallBack();

		static void KickAsyncWorker() { Assert(ms_pRageAsyncShapeTestMgr); ms_pRageAsyncShapeTestMgr->KickAsyncWorker(); }		

	private:
		static bool IsAcceptableTimeToIssueShapeTest();

		// Delete the CShapeTestResults or call CShapeTestResults::Reset() rather than this and it will call AbortTest if necessary for you.
		// If you call this while a task is in progress, then you lose ownership of the phIntersections in the CShapeTestResults which will
		// be replaced with some new ones in the CShapeTestResults.
		void AbortTest(CShapeTestResults* results);
		friend class CShapeTestResults;

#if WORLD_PROBE_DEBUG
	public:
		static void ResetPausedRenderBuffer();

	private:
		static bool AddToDebugPausedRenderBuffer(const CShapeTestTaskData& shapeTestTaskData, WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors);
		static void RenderPausedRenderBuffer();
		friend class CShapeTestTaskData;
#endif

#if __ASSERT
		static bool CanBePerformedAsynchronously(CShapeTestDesc& shapeTestDesc);
#endif

#if WORLD_PROBE_DEBUG
		// For debug rendering.
	public:

		void RenderDebug();
		static void DebugRenderTextLineNormalisedScreenSpace(const char* text, Vec2V_In screenPos, const Color32 textCol, const int lineNum, int persistFrames=1);
		static void DebugRenderTextLineWorldSpace(const char* text, const Vec3V& worldPos, const Color32 textCol, const int lineNum=0, int persistFrames=1);

		bool DebugSubmitTest(const char *strCodeFile, u32 nCodeLine, CShapeTestDesc& shapeTestDesc, eBlockingMode blockingMode=PERFORM_SYNCHRONOUS_TEST);

		// General:
		atFixedArray<bool, WorldProbe::LOS_NumContexts> ms_abContextFilters;

#if WORLD_PROBE_FREQUENCY_LOGGING
		static void DumpFreqLogToTTY();
		static void ClearFreqLog();
#endif // WORLD_PROBE_FREQUENCY_LOGGING
#endif // WORLD_PROBE_DEBUG

	private:
		static CShapeTestManager sm_shapeTestMgrInst;

		static int					m_iShapeTestAsyncSchedulerIndex;

		static phAsyncShapeTestMgr* ms_pRageAsyncShapeTestMgr;

#if !__PPU
		static sysIpcSema			ms_NewRequestSemaphore;
#endif
		static sysCriticalSectionToken ms_AsyncShapeTestPoolCs;
		static sysCriticalSectionToken ms_RequestedAsyncShapeTestsCs;

	private:

		static atQueue<CShapeTestTaskData*, MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS>	ms_RequestedAsyncShapeTests;

#if WORLD_PROBE_DEBUG
		static CShapeTestDebugRenderData* ms_ShapeDebugPausedRenderBuffer;
		static u32 ms_ShapeDebugPausedRenderCount;
		static u32 ms_ShapeDebugPausedRenderHighWaterMark;
		static sysCriticalSectionToken ms_ShapeDebugPausedRenderBufferCs;

	public:
		static bool ms_bDebugDrawSynchronousTestsEnabled;
		static bool ms_bDebugDrawAsyncTestsEnabled;
		static bool ms_bDebugDrawAsyncQueueSummary;
		static int ms_bMaxIntersectionsToDraw;
		static int ms_bIntersectionToDrawIndex;
		static bool ms_bDebugDrawHitPoint;
		static bool ms_bDebugDrawHitNormal;
		static bool ms_bDebugDrawHitPolyNormal;
		static bool ms_bDebugDrawHitInstanceName;
		static bool ms_bDebugDrawHitHandle;
		static bool ms_bDebugDrawHitComponentIndex;
		static bool ms_bDebugDrawHitPartIndex;
		static bool ms_bDebugDrawHitMaterialName;
		static bool ms_bDebugDrawHitMaterialId;
		static float ms_fHitPointNormalLength;
		static float ms_fHitPointRadius;
		static bool ms_bUseCustomCullVolume;
		static bool ms_bDrawCullVolume;
		static bool ms_bForceSynchronousShapeTests;

#if ENABLE_ASYNC_STRESS_TEST
		static unsigned int ms_uStressTestProbeCount;
		static bool ms_bAsyncStressTestEveryFrame;
		static bool ms_bAbortAllTestsInReverseOrderImmediatelyAfterStarting;
		static CShapeTestResults ms_DebugShapeTestResults[MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS];
#endif // ENABLE_ASYNC_STRESS_TEST

#if WORLD_PROBE_FREQUENCY_LOGGING
		struct SFreqLoggingData
		{
			u32 nCount;
			atString nCodeFile;
			u32 nCodeLine;
		};
		atStringMap<SFreqLoggingData> m_frequencyLog;
#endif // WORLD_PROBE_FREQUENCY_LOGGING

#endif // WORLD_PROBE_DEBUG
	};

} // namespace WorldProbe

#endif // SHAPETEST_MGR_H
