#ifndef SHAPETEST_H
#define SHAPETEST_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestboundingboxdesc.h"

namespace WorldProbe
{

	//Big enough to store any phShapeTest<X>. Initialises the phShapeTest. Also performs the phShapeTest if done synchronously
	class CShapeTest
	{
	public:

		CShapeTest() : m_eShapeTestType(INVALID_TEST_TYPE) {} //Invoke one of the constructors below to initialise
		CShapeTest(const WorldProbe::CShapeTestProbeDesc& shapeTestDesc);
		CShapeTest(const WorldProbe::CShapeTestCapsuleDesc& shapeTestDesc);
		CShapeTest(const WorldProbe::CShapeTestSphereDesc& shapeTestDesc);
		CShapeTest(const WorldProbe::CShapeTestBoundDesc& shapeTestDesc);
		CShapeTest(const WorldProbe::CShapeTestBoundingBoxDesc& shapeTestDesc);
		CShapeTest(const WorldProbe::CShapeTestBatchDesc& shapeTestDesc);

		~CShapeTest();

		//Functions that perform the test synchronously (and return the number of phIntersections)
		int PerformDirectedLineTest();
		int PerformUndirectedLineTest();
		int PerformCapsuleTest();
		int PerformSweptSphereTest();
		int PerformSphereTest();
		int PerformBoundTest();
		int PerformBoundingBoxTest();
		int PerformBatchTest();

#if __BANK
		void DrawLine(rage::Color32 colour, const char* debugText, phShapeProbe* probeShapePointer = NULL /*Used by DrawBatch*/);
		void DrawCapsule(rage::Color32 colour, const char* debugText, phShapeCapsule* capsuleShapePointer = NULL /*Used by DrawBatch*/);
		void DrawSphere(rage::Color32 colour, const char* debugText);
		void DrawBound(rage::Color32 colour, const char* debugText);
		void DrawBoundingBox(rage::Color32 colour, const char* debugText);
		void DrawBatch(rage::Color32 colour, const char* debugText);

		void ConstructPausedRenderDataLine(WorldProbe::CShapeTestDebugRenderData& outRenderData, phShapeProbe const * probeShapePointer = NULL) const;
		void ConstructPausedRenderDataCapsule(WorldProbe::CShapeTestDebugRenderData& outRenderData) const;
		void ConstructPausedRenderDataSphere(WorldProbe::CShapeTestDebugRenderData& outRenderData) const;
		void ConstructPausedRenderDataBound(WorldProbe::CShapeTestDebugRenderData& outRenderData) const;
		void ConstructPausedRenderDataBoundingBox(WorldProbe::CShapeTestDebugRenderData& outRenderData) const;
		void ConstructPausedRenderDataBatch(WorldProbe::CShapeTestDebugRenderData outRenderData[]) const;
#endif

		WorldProbe::eShapeTestType GetShapeTestType() const { return m_eShapeTestType; }
		WorldProbe::eTestDomain	GetShapeTestDomain() const { return m_eShapeTestDomain; }

		int GetNumHits(int shapeIndex) const;

		void SetShapeTestCullResults(phShapeTestCullResults* pShapeTestCullResults, phShapeTestCull::State cullState);

		bool IsBoundingBoxTestAHit() const;
		bool IsDirected() const;
		bool RequiresResultSorting() const { return m_bSortResults; }

	private:

		//Done for probes by PerformLineTest and PerformBatchTest
		int PerformLineAgainstVehicleTyresTest(phShapeProbe& shape, const phInst* const * pInstExceptions, int nNumInstExceptions, const phShapeTestCullResults* cullResults);

		static const size_t knSizeOfLargestShapeTest = __64BIT ? sizeof(CBoundingBoxShapeTestData) : sizeof(phShapeTest<phShapeBatch>);
		CompileTimeAssert(sizeof(phShapeTest<phShapeProbe>) <= knSizeOfLargestShapeTest);
		CompileTimeAssert(sizeof(phShapeTest<phShapeSphere>) <= knSizeOfLargestShapeTest);
		CompileTimeAssert(sizeof(phShapeTest<phShapeObject>) <= knSizeOfLargestShapeTest);
		CompileTimeAssert(sizeof(phShapeTest<phShapeCapsule>) <= knSizeOfLargestShapeTest);
		CompileTimeAssert(sizeof(CBoundingBoxShapeTestData) <= knSizeOfLargestShapeTest);

		//The information needed to perform the shape test
		ALIGNAS(16) u8 m_ShapeTest[knSizeOfLargestShapeTest] ; //phShapeTest<X>
		phCullShape m_CullShape;
		WorldProbe::eTestDomain m_eShapeTestDomain;
		WorldProbe::eShapeTestType m_eShapeTestType;
		bool m_bSortResults;
		bool m_bIncludeTyres; //Whether to perform test against tyres. Only used by line tests (and line test batches)
	};

} // namespace WorldProbe

#endif // SHAPETEST_H
