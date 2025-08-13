#ifndef SHAPETEST_BATCH_DESC_H
#define SHAPETEST_BATCH_DESC_H

// RAGE headers:

// Game headers:
//#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"
#include "physics/WorldProbe/shapetestprobedesc.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"

// Forward declarations:
namespace WorldProbe
{
	class CCullVolumeDesc;
}

namespace WorldProbe
{

	/////////////////////////////////////////////
	// Descriptor class for batched shape tests.
	/////////////////////////////////////////////
	class CShapeTestBatchDesc : public CShapeTestDesc
	{
	public:

		CShapeTestBatchDesc()
			: CShapeTestDesc(),
			m_nNumShapeTests(0),
			m_nNumProbes(0),
			m_nNumCapsules(0),
			m_nMaxNumProbes(0),
			m_nMaxNumCapsules(0),
			m_pProbeDescriptors(0),
			m_pCapsuleDescriptors(NULL),
			m_pCustomCullVolumeDesc(NULL)
		{
			m_eShapeTestType = BATCHED_TESTS;
		}

		// Add new shape tests to this batch. Returns false if there was a problem doing this. Each 
		// test in the batch can write results to its own CShapeTestResults structure. Setting the
		// global results structure on a batch test descriptor will result in an assert at run-time.
		bool AddLineTest(const CShapeTestProbeDesc& probeDesc);
		bool AddCapsuleTest(const CShapeTestCapsuleDesc& capsuleDesc);
#if 0 // Not used yet!
		bool AddSphereTest(const CShapeTestSphereDesc& sphereDesc);
#endif // 0
		void Reset();

		void SetCustomCullVolume(const CCullVolumeDesc* pCullVolumeDesc);

		int GetNumShapeTests() const { return m_nNumShapeTests; }

		void SetProbeDescriptors(CShapeTestProbeDesc* pProbeDescriptors, int numProbeDescriptors);
		void SetCapsuleDescriptors(CShapeTestCapsuleDesc* pCapsuleDescriptors, int numCapsuleDescriptors);

	private:
		void InitSubDescriptor(CShapeTestDesc& descriptor) const;

		// Number of tests to be processed.
		int m_nNumShapeTests;
		int m_nNumProbes;
		int m_nNumCapsules;

		int m_nMaxNumProbes;
		int m_nMaxNumCapsules;

		CShapeTestProbeDesc* m_pProbeDescriptors;
		CShapeTestCapsuleDesc* m_pCapsuleDescriptors;

		const CCullVolumeDesc* m_pCustomCullVolumeDesc;

		friend class CShapeTest;
		friend class CShapeTestTaskData;
	};

#define ALLOC_AND_SET_PROBE_DESCRIPTORS(batchDescriptor, maxNumProbes)												\
	{																												\
		WorldProbe::CShapeTestProbeDesc* probeDescriptors = Alloca(WorldProbe::CShapeTestProbeDesc,maxNumProbes);	\
		batchDescriptor.SetProbeDescriptors(probeDescriptors,maxNumProbes);											\
	}
#define ALLOC_AND_SET_CAPSULE_DESCRIPTORS(batchDescriptor, maxNumCapsules)													\
	{																														\
		WorldProbe::CShapeTestCapsuleDesc* capsuleDescriptors = Alloca(WorldProbe::CShapeTestCapsuleDesc,maxNumCapsules);	\
		batchDescriptor.SetCapsuleDescriptors(capsuleDescriptors,maxNumCapsules);											\
	}
} // namespace WorldProbe


#endif // SHAPETEST_BATCH_DESC_H
