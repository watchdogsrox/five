//
// SPU PlantsMgr Update
//
//	10/10/2007	- matts:	- initial;
//	02/12/2008	- Andrzej:	- cleanup and refactor;
//
//
//
//
//
#if __SPU

// Rage headers
#include "atl/bitset.h"
#include "basetypes.h"
#include "math/random.cpp"
#include "phbound/boundgeom.h"
#include "phbound/primitives.cpp"
#include "physics/inst.h"
#include "system/taskheaderspu.h"
#include "vector/color32.h"
#include "vector/color32.cpp"	// Color32::GetRGBA()...

// Game headers
#include "renderer/Renderer.h"			// JW- had to move this up here to get a successful compile...

#include "Physics/GtaMaterialManager.h"
#include "Objects/ProceduralInfo.h"
#include "Objects/ProcObjects.h"
#include "scene/Entity.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PlantsGrassRenderer.h"

#if __ASSERT
	// redefine (locally) FastAssert() as simple TrapZ() (otherwise on __SPU it's just Assert()):
	#undef  FastAssert
	#define FastAssert(X)		TrapZ((int)(X))
#endif

#define USE_POLY_MRU_CACHE	(1)
#if __DEV
	u32 g_PolygonsFetched;	// polygon fetching stats
#endif
#if USE_POLY_MRU_CACHE
	struct phPolygonMruCache;
	phPolygonMruCache			*g_pPolygonMruCache;
	#define g_PolygonMruCache	(*g_pPolygonMruCache)
#endif

#define PLANTS_DMATAGID								(19)	// main update dma tag

u8 _g_DrawRand[sizeof(mthRandom)];
#define g_DrawRandSpu	(*((mthRandom*)_g_DrawRand))

CPlantMgr* g_pPlantMgr;
#define gPlantMgr (*g_pPlantMgr)

CProceduralInfo* g_pProcInfo;
#define g_procInfo (*g_pProcInfo)

spuPlantsMgrUpdateStruct* g_jobParams;
ProcObjectCreationInfo* g_pAddList;
ProcObjectCreationInfo* g_pAddListEnd;
ProcTriRemovalInfo* g_pRemoveList;
ProcTriRemovalInfo* g_pRemoveListEnd;
spuPlantsMgrUpdateStruct::CResultSize* g_pResultSize;

#define PlantsMinGroundAngleSlope		(g_jobParams->m_GroundSlopeAngleMin)

#include "renderer/PlantsMgrUpdateCommon.h"


void g_procObjMan_AddObjectToAddList(CPlantLocTriArray& triTab, Vector3 pos, Vector3 normal, CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri)
{
	if(!g_jobParams->m_maxAdd)
	{
		return;	// can't add more!
	}

	FastAssert(g_jobParams->m_maxAdd);
	g_jobParams->m_maxAdd--;

	ProcObjectCreationInfo& info = *g_pAddList++;
	pos.uw		= ((u8*)pProcObjInfo - (u8*)&g_procInfo.m_procObjInfos[0]);

CTriHashIdx16 idx;
	idx.Make(triTab.m_listID, triTab.GetIdx(pLocTri));
	normal.uw	= (u32)idx;

	info.pos	= pos;
	info.normal = normal;
	//Displayf("AddObjectToAddList (%f %f %f) (%f %f %f) %p %p", info.pos.x, info.pos.y, info.pos.z, info.normal.x, info.normal.y, info.normal.z, pProcObjInfo, pLocTri);
	if (g_pAddList == g_pAddListEnd)
	{
		// handle list overflow
		g_pResultSize->m_numAdd += g_jobParams->m_addBufSize;
		g_pAddList -= g_jobParams->m_addBufSize;
		sysDmaLargePut(g_pAddList, (u64)g_jobParams->m_pAddList, (u8*)g_pAddListEnd - (u8*)g_pAddList, PLANTS_DMATAGID);
		g_jobParams->m_pAddList += g_jobParams->m_addBufSize;
		sysDmaWait(1<<PLANTS_DMATAGID);
	}

}

void g_procObjMan_AddTriToRemoveList(CPlantLocTriArray& triTab, CPlantLocTri* pLocTri)
{
	FastAssert(g_jobParams->m_maxRemove);
	g_jobParams->m_maxRemove--;

	ProcTriRemovalInfo& info = *g_pRemoveList++;
	
CTriHashIdx16 idx;
	idx.Make(triTab.m_listID, triTab.GetIdx(pLocTri));
	info.pLocTri	= (CPlantLocTri*)(u32)idx;

	info.procTagId	= phMaterialMgrGta::UnpackProcId(pLocTri->m_nSurfaceType);
	if (g_pRemoveList == g_pRemoveListEnd)
	{
		// handle list overflow
		g_pResultSize->m_numRemove += g_jobParams->m_removeBufSize;
		g_pRemoveList -= g_jobParams->m_removeBufSize;
		sysDmaLargePut(g_pRemoveList, (u64)g_jobParams->m_pRemoveList, (u8*)g_pRemoveListEnd - (u8*)g_pRemoveList, PLANTS_DMATAGID);
		g_jobParams->m_pRemoveList += g_jobParams->m_removeBufSize;
		sysDmaWait(1<<PLANTS_DMATAGID);
	}
}


void CPlantMgr::UpdateTask(u32 listID)
{
	// fetch update buffer:
	sysDmaLargeGet(&g_pPlantMgr->m_LocTrisTabSpu, (u64)g_jobParams->m_LocTrisTab[listID], sizeof(CPlantLocTriArray), PLANTS_DMATAGID);
	sysDmaWait(1<<PLANTS_DMATAGID);


	UpdateAllLocTris(g_pPlantMgr->m_LocTrisTabSpu, m_CameraPos, g_jobParams->m_iTriProcessSkipMask);


	// store update buffer:
	sysDmaLargePut(&g_pPlantMgr->m_LocTrisTabSpu, (u64)g_jobParams->m_LocTrisTab[listID], sizeof(CPlantLocTriArray), PLANTS_DMATAGID);
	// update render buffer:
	sysDmaLargePut(&g_pPlantMgr->m_LocTrisTabSpu, (u64)g_jobParams->m_LocTrisRenderTab[listID], sizeof(CPlantLocTriArray), PLANTS_DMATAGID);
	sysDmaWait(1<<PLANTS_DMATAGID);
}

//
//
//
//
//
void PlantsMgrUpdateSPU(sysTaskContext& c)
{
#if __BANK
	const u32 jobTimeStart = spu_read_decrementer();
#endif

	g_jobParams					= c.GetUserDataAs<spuPlantsMgrUpdateStruct>();
 	g_pProcInfo					= c.GetInputAs<CProceduralInfo>();
	g_pProcInfo->m_procObjInfos.SetElements(c.GetInputAs<CProcObjInfo>(g_pProcInfo->m_procObjInfos.GetCount()));
	g_pProcInfo->m_plantInfos.SetElements(c.GetInputAs<CPlantInfo>(g_pProcInfo->m_plantInfos.GetCount()));
	g_pAddList					= c.GetInputAs<ProcObjectCreationInfo>(g_jobParams->m_addBufSize);
	g_pRemoveList				= c.GetInputAs<ProcTriRemovalInfo>(g_jobParams->m_removeBufSize);
	g_pResultSize				= c.GetInputAs<spuPlantsMgrUpdateStruct::CResultSize>();
	g_pResultSize->m_numAdd		= 0;
	g_pResultSize->m_numRemove	= 0;
	g_pAddListEnd				= g_pAddList	+ g_jobParams->m_addBufSize;
	g_pRemoveListEnd			= g_pRemoveList + g_jobParams->m_removeBufSize;

	g_pPlantMgr = (CPlantMgr*)Alloca(CPlantMgrBase, 1);
	sysDmaLargeGet(g_pPlantMgr, (u64)g_jobParams->m_pPlantsMgr, sizeof(CPlantMgrBase0), PLANTS_DMATAGID);

	g_DrawRandSpu.Reset( spu_readch(SPU_RdDec) );

	ProcObjectCreationInfo*	pAddList	= g_pAddList;
	ProcTriRemovalInfo*		pRemoveList	= g_pRemoveList;

#if USE_POLY_MRU_CACHE
	phPolygonMruCache	polyMruCache;
	g_pPolygonMruCache = &polyMruCache;
#endif
	DEV_ONLY(g_PolygonsFetched=0;)	// reset fetching stats
	
#if HACK_GTA4_MEASURE_TIME
	TimerInit();
	const u32 startTime = TimerLap();
#endif

	sysDmaWait(1<<PLANTS_DMATAGID);

	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		g_pPlantMgr->UpdateTask(listID);
	}	

#if HACK_GTA4_MEASURE_TIME
	const u32 stopTime = TimerLap();
	Displayf("taskTime=%.4fms, polygonsFetched=%d.", MeasureTime(startTime, stopTime), g_PolygonsFetched);
#endif

const u32 jobOutputDmaTag = c.DmaTag();

	// store CPlantMgrBase0:
	sysDmaLargePut(g_pPlantMgr, (u64)g_jobParams->m_pPlantsMgr, sizeof(CPlantMgrBase0), jobOutputDmaTag);

	if(g_pAddList != pAddList)
	{
		g_pResultSize->m_numAdd		+= (g_pAddList - pAddList);
		sysDmaLargePut(pAddList, (u64)g_jobParams->m_pAddList, (u8*)g_pAddList - (u8*)pAddList, jobOutputDmaTag);
	}

	if(g_pRemoveList != pRemoveList)
	{
		g_pResultSize->m_numRemove	+= (g_pRemoveList - pRemoveList);
		sysDmaLargePut(pRemoveList, (u64)g_jobParams->m_pRemoveList, (u8*)g_pRemoveList - (u8*)pRemoveList, jobOutputDmaTag);
	}

	sysDmaPut(g_pResultSize, (u64)g_jobParams->m_pResultSize, sizeof(spuPlantsMgrUpdateStruct::CResultSize), jobOutputDmaTag);

#if FURGRASS_TEST_V4
	// store fur grass pickup info for RT:
	g_pPlantMgr->FurGrassStoreRenderInfo(g_jobParams->m_furGrassPickupRenderInfo, jobOutputDmaTag);
#endif

#if __BANK
	if(g_jobParams->m_printSpuJobTimings)
	{
		spu_printf("\n PlantsMgrUpdateSpu time: %.6fms.", float((jobTimeStart-spu_read_decrementer())*1000/79800) / 1000.0f);
	}
#endif
}// end of PlantsMgrUpdateSPU()...

#endif //#if __SPU...

