// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#include "Renderer/Lights/AsyncLightOcclusionMgr.h"

// rage headers
#include "softrasterizer/scansetup.h"

// framework headers
#include "fwscene/scan/scan.h"
#include "fwscene/scan/scanDebug.h"
#include "system/dependencyscheduler.h"

// game headers
#include "Debug/Rendering/DebugLights.h"
#include "Renderer/Lights/Lights.h"
#include "Renderer/occlusion.h"
#include "Renderer/ProcessLightOcclusionAsync.h"

RENDER_OPTIMISATIONS()

extern bool		g_render_lock;

ProcessLightOcclusionData	g_LightOcclusionData ;
atArray<CLightSource>		g_potentialLights;
sysDependency				g_LightOcclusionDependency;
u32							g_LightOcclusionDependencyRunning;
#if __DEV
u32							g_LightOcclusionTrivialAcceptActiveFrustum;
u32							g_LightOcclusionTrivialAcceptMinZ;
u32							g_LightOcclusionTrivialAcceptVisiblePixel;
#endif

#if __PPU
DECLARE_FRAG_INTERFACE(ProcessLightOcclusionAsync);
#endif

namespace CAsyncLightOcclusionMgr
{
	void Init()
	{
		u32 flags = sysDepFlag::INPUT0 | sysDepFlag::INPUT1 | sysDepFlag::INPUT2 | sysDepFlag::INPUT3 | 
			sysDepFlag::INPUT4 | sysDepFlag::INPUT5 | sysDepFlag::INPUT6 | sysDepFlag::INPUT7;
#if __PPU
		g_LightOcclusionDependency.Init( FRAG_SPU_CODE(ProcessLightOcclusionAsync), 0, flags );
#else
		g_LightOcclusionDependency.Init( ProcessLightOcclusionAsync::RunFromDependency, 0, flags );
#endif
		g_LightOcclusionDependency.m_Priority = sysDependency::kPriorityCritical;
	}

	void Shutdown()
	{
	}

	void StartAsyncProcessing()
	{
		if( g_potentialLights.GetCount() == 0
			|| (g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION) == 0
			BANK_ONLY(|| !DebugLights::m_EnableLightAABBTest)
			)
		{
			return;
		}

		const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
		g_LightOcclusionData.m_vCameraPos = grcVP->GetCameraPosition();

		g_LightOcclusionData.m_LightSourceArrayEA = g_potentialLights.GetElements();
		g_LightOcclusionData.m_DependencyRunningEA = &g_LightOcclusionDependencyRunning;
#if __DEV
		g_LightOcclusionData.m_pNumOcclusionTests = &COcclusion::m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT];
		g_LightOcclusionData.m_pNumObjectsOccluded = &COcclusion::m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT];
		g_LightOcclusionData.m_pTrivialAcceptActiveFrustum = &g_LightOcclusionTrivialAcceptActiveFrustum;
		g_LightOcclusionData.m_pTrivialAcceptMinZ = &g_LightOcclusionTrivialAcceptMinZ;
		g_LightOcclusionData.m_pTrivialAcceptVisiblePixel = &g_LightOcclusionTrivialAcceptVisiblePixel;
#endif

		fwScanBaseInfo&		scanBaseInfo = fwScan::GetScanBaseInfo();

		g_LightOcclusionDependency.m_Params[0].m_AsPtr = g_potentialLights.GetElements();
		g_LightOcclusionDependency.m_Params[1].m_AsPtr = &g_LightOcclusionData;
		g_LightOcclusionDependency.m_Params[2].m_AsPtr = &scanBaseInfo;
		g_LightOcclusionDependency.m_Params[3].m_AsPtr = COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY);
		g_LightOcclusionDependency.m_Params[4].m_AsPtr = (void*)COcclusion::GetIcosahedronPoints();
		g_LightOcclusionDependency.m_Params[5].m_AsPtr = (void*)COcclusion::GetIcosahedronIndices();
		g_LightOcclusionDependency.m_Params[6].m_AsPtr = (void*)COcclusion::GetConePoints();
		g_LightOcclusionDependency.m_Params[7].m_AsPtr = (void*)COcclusion::GetConeIndices();

		g_LightOcclusionDependency.m_DataSizes[0] = g_potentialLights.GetCount() * sizeof(CLightSource);
		g_LightOcclusionDependency.m_DataSizes[1] = sizeof(ProcessLightOcclusionData);
		g_LightOcclusionDependency.m_DataSizes[2] = sizeof(fwScanBaseInfo);
		g_LightOcclusionDependency.m_DataSizes[3] = RAST_HIZ_AND_STENCIL_SIZE_BYTES;
		g_LightOcclusionDependency.m_DataSizes[4] = sizeof(Vec3V)*COcclusion::GetNumIcosahedronPoints();
		g_LightOcclusionDependency.m_DataSizes[5] = sizeof(u8)*COcclusion::GetNumIcosahedronIndices();
		g_LightOcclusionDependency.m_DataSizes[6] = sizeof(Vec3V)*COcclusion::GetNumConePoints();
		g_LightOcclusionDependency.m_DataSizes[7] = sizeof(u8)*COcclusion::GetNumConeIndices();

		g_LightOcclusionDependencyRunning = 1;

		sysDependencyScheduler::Insert( &g_LightOcclusionDependency );
	}

	void ProcessResults()
	{
		PF_PUSH_TIMEBAR("CAsyncLightOcclusionMgr Add Lights");

		PF_PUSH_TIMEBAR("Wait for Dependency");

		while(true)
		{
			volatile u32 *pDependencyRunning = &g_LightOcclusionDependencyRunning;
			if(*pDependencyRunning == 0)
			{
				break;
			}

			sysIpcYield(PRIO_NORMAL);
		}

		PF_POP_TIMEBAR();

		for(int i = 0; i < g_potentialLights.GetCount(); i++)
		{
			CLightSource &light = g_potentialLights[i];
			if((u32)light.GetType() != 0xffffffffu)
			{
				Lights::AddSceneLight(light);
			}
		}

		g_potentialLights.ResetCount();

		PF_POP_TIMEBAR();
	}

	CLightSource* AddLight()
	{
#if __BANK
		if(g_render_lock)
		{
			return nullptr;
		}
#endif

		Assert(!g_LightOcclusionDependencyRunning);
		return &g_potentialLights.Grow();
	}

} // end of namespace CAsyncLightOcclusionMgr

