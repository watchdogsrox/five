// ProcessLightOcclusionAsync
// jlandry: 19/03/2013

#define g_auxDmaTag		(0)

#include "ProcessLightOcclusionAsync.h"

#include "system/interlocked.h"
#include "softrasterizer/scan.h"
#include "softrasterizer/softrasterizer.h"
#include "../renderer/Lights/LightCulling.h"
#include "../renderer/Lights/LightSource.h"

bool ProcessLightOcclusionAsync::RunFromDependency(const sysDependency& dependency)
{
	CLightSource* pLightArray = static_cast<CLightSource*>(dependency.m_Params[0].m_AsPtr);
	int numLights = dependency.m_DataSizes[0] / sizeof(CLightSource);
	
	ProcessLightOcclusionData* pData = static_cast<ProcessLightOcclusionData*>(dependency.m_Params[1].m_AsPtr);
	u32* pDependencyRunning = pData->m_DependencyRunningEA;

#if __SPU
	fwScanBaseInfo *pScanInfo = static_cast<fwScanBaseInfo*>(dependency.m_Params[2].m_AsPtr);
	const Vec4V* hiZBuffer = static_cast<const Vec4V*>(dependency.m_Params[3].m_AsPtr);

	const Vec3V* icosahedronPoints = static_cast<Vec3V*>(dependency.m_Params[4].m_AsPtr);
	const u8* icosahedronIndices = static_cast<u8*>(dependency.m_Params[5].m_AsPtr);
	const Vec3V* conePoints = static_cast<Vec3V*>(dependency.m_Params[6].m_AsPtr);
	const u8* coneIndices = static_cast<u8*>(dependency.m_Params[7].m_AsPtr);
	u32 numIcosahedronPoints = dependency.m_DataSizes[4] / sizeof(Vec3V);
	u32 numIcosahedronIndices = dependency.m_DataSizes[5] / sizeof(u8);
	u32 numConePoints = dependency.m_DataSizes[6] / sizeof(Vec3V);
	u32 numConeIndices = dependency.m_DataSizes[7] / sizeof(u8);

#if __DEV
	u32 *pNumOcclusionTests = pData->m_pNumOcclusionTests;
	u32 *pNumObjectsOccluded = pData->m_pNumObjectsOccluded;
	u32 *pTrivialAcceptActiveFrustum = pData->m_pTrivialAcceptActiveFrustum;
	u32 *pTrivialAcceptMinZ = pData->m_pTrivialAcceptMinZ;
	u32 *pTrivialAcceptVisiblePixel = pData->m_pTrivialAcceptVisiblePixel;
#endif
#endif

	for(int i = 0; i < numLights; i++)
	{
		CLightSource& lightSource = pLightArray[i];

#if __SPU
		bool bOccluded = !IsLightVisible(lightSource, pData->m_vCameraPos, pScanInfo, hiZBuffer,
			icosahedronPoints, numIcosahedronPoints, icosahedronIndices, numIcosahedronIndices,
			conePoints, numConePoints, coneIndices, numConeIndices
			DEV_ONLY(, pNumOcclusionTests, pNumObjectsOccluded, pTrivialAcceptActiveFrustum, pTrivialAcceptMinZ, pTrivialAcceptVisiblePixel));
#else
		bool bOccluded = !IsLightVisible(lightSource);		
#endif
		if(bOccluded)
		{
			lightSource.SetType((eLightType)0xffffffff);
		}
		else
		{
			lightSource.SetFlag(LIGHTFLAG_ALREADY_TESTED_FOR_OCCLUSION);
		}
	}

#if __SPU
	void* pLightArrayEA = pData->m_LightSourceArrayEA;
	sysDmaLargePutAndWait(pLightArray, (u32)pLightArrayEA, dependency.m_DataSizes[0], 1);
#endif

	sysInterlockedExchange(pDependencyRunning, 0);

	return true;
}