#include "physics/WorldProbe/shapetestbatchdesc.h"


void WorldProbe::CShapeTestBatchDesc::InitSubDescriptor(CShapeTestDesc& descriptor) const
{
	// The batch descriptor's options and include / exclude flags will override those
	// on the individual line tests.
	descriptor.SetOptions(GetOptions());
	descriptor.SetIncludeInstances(m_aIncludeInstList.GetElements(), m_aIncludeInstList.GetCount());
	descriptor.SetExcludeInstances(m_aExcludeInstList.GetElements(), m_aExcludeInstList.GetCount());
	descriptor.SetContext(GetContext());
	descriptor.SetDomainForTest(m_eShapeTestDomain);
	descriptor.SetIncludeFlags(m_nIncludeFlags);
	descriptor.SetTypeFlags(m_nTypeFlags);
	descriptor.SetStateIncludeFlags(m_nStateIncludeFlags);
}

bool WorldProbe::CShapeTestBatchDesc::AddLineTest(const CShapeTestProbeDesc& probeDesc)
{
	if(AssertVerify(m_pProbeDescriptors && m_nNumProbes < m_nMaxNumProbes))
	{
		++m_nNumShapeTests;
		int newProbeIndex = m_nNumProbes++;
		m_pProbeDescriptors[newProbeIndex] = probeDesc;
		InitSubDescriptor(m_pProbeDescriptors[newProbeIndex]);
		return true;
	}
	return false;
}

bool WorldProbe::CShapeTestBatchDesc::AddCapsuleTest(const CShapeTestCapsuleDesc& capsuleDesc)
{
	if(AssertVerify(m_pCapsuleDescriptors && m_nNumCapsules < m_nMaxNumCapsules))
	{
		++m_nNumShapeTests;
		int newCapsuleIndex = m_nNumCapsules++;
		m_pCapsuleDescriptors[newCapsuleIndex] = capsuleDesc;
		InitSubDescriptor(m_pCapsuleDescriptors[newCapsuleIndex]);
		return true;
	}
	return false;
}

#if 0 // Not used yet! 
bool WorldProbe::CShapeTestBatchDesc::AddSphereTest(const CShapeTestSphereDesc& sphereDesc)
{
	m_aSphereDescriptors.PushAndGrow(sphereDesc);
	++m_nNumSphereTests;
	++m_nNumShapeTests;

	physicsAssert(m_nNumShapeTests == m_nNumProbeTests+m_nNumCapsuleTests+m_nNumSphereTests);

	return true;
}
#endif // 0 - Not used yet.

void WorldProbe::CShapeTestBatchDesc::SetCustomCullVolume(const CCullVolumeDesc* pCullVolumeDesc)
{
	m_pCustomCullVolumeDesc = pCullVolumeDesc;
}

void WorldProbe::CShapeTestBatchDesc::Reset()
{
	SetProbeDescriptors(NULL,0);
	SetCapsuleDescriptors(NULL,0);
}

void WorldProbe::CShapeTestBatchDesc::SetProbeDescriptors(CShapeTestProbeDesc* pProbeDescriptors, int numProbeDescriptors)
{
	Assert(pProbeDescriptors != NULL || numProbeDescriptors == 0);
	m_nMaxNumProbes = numProbeDescriptors;
	m_nNumProbes = 0;
	m_nNumShapeTests = m_nNumCapsules + m_nNumProbes;
	m_pProbeDescriptors = pProbeDescriptors;
}
void WorldProbe::CShapeTestBatchDesc::SetCapsuleDescriptors(CShapeTestCapsuleDesc* pCapsuleDescriptors, int numCapsuleDescriptors)
{
	Assert(pCapsuleDescriptors != NULL || numCapsuleDescriptors == 0);
	m_nMaxNumCapsules = numCapsuleDescriptors;
	m_nNumCapsules = 0;
	m_nNumShapeTests = m_nNumCapsules + m_nNumProbes;
	m_pCapsuleDescriptors = pCapsuleDescriptors;
}
