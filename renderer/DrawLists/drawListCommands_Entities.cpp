/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    drawlistCommands_Entities.cpp
// PURPOSE : draw list commands for rendering entities.
// AUTHOR :  john.
// CREATED : 21/5/09
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer/DrawLists/drawList.h" 

// rage headers
#include "grmodel/matrixset.h"
#include "profile/cputrace.h"
#include "grprofile/gcmtrace.h"
#include "fragment/tune.h"
#include "fragment/manager.h"
#include "grcore/allocscope.h"
#include "grcore/wrapper_gcm.h"
#include "grcore/grcorespu.h"
#include "breakableglass/bgdrawable.h"
#include "breakableglass/glassmanager.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwscene/stores/drawablestore.h"
#if __PPU
#include "rmcore/drawablespu.h"
#endif

// game header
#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationStream.h"
#include "physics/breakable.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "scene/EntityBatch.h"
#include "scene/lod/LodScale.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "shaders/shaderLib.h"
#include "shaders/ShaderEdit.h"
#include "shaders/CustomShaderEffectPed.h"
#include "system/taskscheduler.h"
#include "system/dependencyscheduler.h"
#include "vehicles/VehicleFactory.h"

#include "renderer/DrawLists/drawList_CopyOffEntityVirtual.h"

RENDER_OPTIMISATIONS()

#if HACK_GTA4_MODELINFOIDX_ON_SPU
	CompileTimeAssert(DL_MAX_TYPES <= 32);	// must fit into 5 bits - see gta4RenderPhaseID in CGta4DbgSpuInfoStruct;
	CompileTimeAssert(NUM_MI_TYPES < 32);	// must fit into 5 bits - see gta4ModelInfoType in CGta4DbgSpuInfoStruct
	CompileTimeAssert(EXT_TYPE_PARTICLE==23);// hardcoded in ptxd_Model::DrawPoints() from ptxd_drawmodel.cpp;

	// this index is read by Rage to supply modelIdx info for drawablespu:
	namespace rage	{
		extern CGta4DbgSpuInfoStruct	gGta4DbgInfoStruct;

		void DbgSetDrawableModelIdxForSpu(u16 idx)
		{
			Assert(gGta4DbgInfoStruct.gta4ModelInfoIdx==u16(-1));
			gGta4DbgInfoStruct.gta4ModelInfoIdx = idx;		
			dlDrawListInfo *drawListInfo = DRAWLISTMGR->GetCurrExecDLInfo();
			gGta4DbgInfoStruct.gta4RenderPhaseID = drawListInfo? ((u8)drawListInfo->m_type) : (DL_RENDERPHASE);
			gGta4DbgInfoStruct.gta4ModelInfoType = CModelInfo::GetBaseModelInfo(fwModelId((u32)idx))->GetModelType();
			gGta4DbgInfoStruct.gta4MaxTextureSize = DEV_SWITCH_NT(g_ShaderEdit::GetInstance().GetMaxTexturesize(),0xF);
		}
		void DbgCleanDrawableModelIdxForSpu()
		{
			Assert(gGta4DbgInfoStruct.gta4ModelInfoIdx!=u16(-1));
			gGta4DbgInfoStruct.Invalidate();
		}
	}
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU...

#define RAGETRACE_CALL(x)	DEBUG_CALL(x)

//#################################################################################
// --- entity drawing object commands --- 
#include "fwsys/timer.h"
#include "System/ipc.h"
#include "System/Xtl.h"

bool CopyOffMatrixSetSPU_Dependency(const sysDependency& dependency);

//#################################################################################
// --- entity drawing object commands --- 

bank_bool g_cache_entities = true;

#if __BANK
DECLARE_MTR_THREAD bool gIsDrawingPed = false;
DECLARE_MTR_THREAD bool gIsDrawingVehicle = false;
DECLARE_MTR_THREAD bool gIsDrawingHDVehicle = false;
#endif

template<class EntityDrawData>
DrawListAddress CopyOffEntity(CEntity * pEntity)
{
	int sharedMemType = DL_MEMTYPE_ENTITY;
	dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

	DrawListAddress dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

	u32 dataSize = (u32) sizeof(EntityDrawData);

	if(dataAddress.IsNULL() || !g_cache_entities)
	{
		void * data = gDCBuffer->AddDataBlock(NULL, dataSize, dataAddress);
		if(g_cache_entities)
		{
			gDCBuffer->AllocateSharedData(sharedMemType, sharedDataInfo, dataSize, dataAddress);
		}

		reinterpret_cast<EntityDrawData*>(data)->Init(pEntity);

		gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
	}
	else
	{
#if __ASSERT || RAGE_INSTANCED_TECH
		void * data = gDCBuffer->GetDataBlock(dataSize, dataAddress);
#endif
#if __ASSERT
		dlCmdDataBlock * dataBlock = reinterpret_cast<dlCmdDataBlock*>(data)-1;
		FatalAssert(dataBlock->GetInstructionIdStatic() == DC_DataBlock);
		FatalAssert(dataBlock->GetCommandSizeStatic() == sizeof(dlCmdDataBlock)+dataSize);
#endif
#if RAGE_INSTANCED_TECH
		if (pEntity->GetViewportInstancedRenderBit() != 0)
		{
			reinterpret_cast<EntityDrawData*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
		}
#endif
	}

	return dataAddress;
};

#if __DEV
void EntityDetails_GetDrawableType(CBaseModelInfo* pBaseModelInfo, atString &ReturnString);

DrawCommandDebugInfo::DrawCommandDebugInfo(CEntity *entity)
{
	Assert(entity);
	m_ModelIndex = entity->GetModelIndex();
}

const char *DrawCommandDebugInfo::GetExtraDebugInfo(char * buffer, size_t bufferSize)
{
	safecpy(buffer, fwArchetype::GetModelName(m_ModelIndex.Get()), bufferSize);
	return buffer;
}
#endif // __DEV

CDrawEntityDC::CDrawEntityDC(CEntity* pEntity)
: m_DebugInfo(pEntity)
{
    m_dataAddress = SharedData(pEntity);
}

#if __DEV
const char *CDrawEntityDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawEntityDC *pDLC = (CDrawEntityDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

RAGETRACE_DECL(DrawEntity);

void CDrawEntityDC::Execute()
{
	RAGETRACE_SCOPE(DrawEntity);
    Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
    CEntityDrawData & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

DrawListAddress CDrawEntityDC::SharedData(CEntity * pEntity)
{
	return CopyOffEntity<CEntityDrawData>(pEntity);
}

CDrawEntityFmDC::CDrawEntityFmDC(CEntity* pEntity)
: m_DebugInfo(pEntity)
{
    m_dataAddress = SharedData(pEntity);
}

#if __DEV
const char *CDrawEntityFmDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawEntityFmDC *pDLC = (CDrawEntityFmDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

RAGETRACE_DECL(DrawEntityFm);

void CDrawEntityFmDC::Execute()
{
	RAGETRACE_SCOPE(DrawEntityFm);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	CEntityDrawDataFm & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

DrawListAddress CDrawEntityFmDC::SharedData(CEntity * pEntity)
{
	return CopyOffEntity<CEntityDrawDataFm>(pEntity);
}

//////////////////////////////////////////////////////////////////////////
//
// CDrawEntityInstancedDC
//

CDrawEntityInstancedDC::CDrawEntityInstancedDC(CEntity* pEntity, grcInstanceBufferList &list, int lod)
: m_DebugInfo(pEntity)
{
	m_dataAddress = SharedData(pEntity, list.GetFirst(), lod);
}

CDrawEntityInstancedDC::CDrawEntityInstancedDC(CEntity* pEntity, grcInstanceBuffer *ib, int lod)
	: m_DebugInfo(pEntity)
{
	m_dataAddress = SharedData(pEntity, ib, lod);
}

#if __DEV
const char *CDrawEntityInstancedDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawEntityInstancedDC *pDLC = static_cast<CDrawEntityInstancedDC *>(&base);
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

RAGETRACE_DECL(DrawInstancedEntity);

void CDrawEntityInstancedDC::Execute()
{
	RAGETRACE_SCOPE(DrawInstancedEntity);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	EntityDrawData &data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

DrawListAddress CDrawEntityInstancedDC::SharedData(CEntity * pEntity, grcInstanceBuffer *ib, int lod)
{
	//Not the typical use case..
	DrawListAddress dataAddress;
	u32 dataSize = static_cast<u32>(sizeof(EntityDrawData));
	void *data = gDCBuffer->AddDataBlock(NULL, dataSize, dataAddress);

	reinterpret_cast<EntityDrawData *>(data)->Init(pEntity, ib, lod);

	gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());

	if(pEntity->GetIsTypeInstanceList())	//Entity batches use static instance buffers, which are a render resource, so make sure it's not unloaded while we're drawing
		gDrawListMgr->AddMapDataReference(static_cast<CEntityBatch *>(pEntity)->GetMapDataDefIndex());

	return dataAddress;
}

//////////////////////////////////////////////////////////////////////////
//
// CDrawGrassBatchDC
//

#if defined(_MSC_VER) && (_MSC_VER == 1600) // VS2010
	#pragma warning(push)
	#pragma warning(disable: 4355) // 'this' used in base member initializer list
#endif

CDrawGrassBatchDC::CDrawGrassBatchDC(CGrassBatch* pEntity)
: parent_type(parent_type::functor_type(this, &CDrawGrassBatchDC::DispatchComputeShader) WIN32PC_ONLY(, parent_type::functor_type(this, &CDrawGrassBatchDC::CopyStructureCount)))
, m_DebugInfo(pEntity)
{
	m_dataAddress = SharedData(pEntity);
}

#if defined(_MSC_VER) && (_MSC_VER == 1600)
	#pragma warning(pop)
#endif

#if __DEV
const char *CDrawGrassBatchDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawGrassBatchDC *pDLC = static_cast<CDrawGrassBatchDC *>(&base);
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

RAGETRACE_DECL(DrawGrassBatch);

void CDrawGrassBatchDC::Execute()
{
	RAGETRACE_SCOPE(DrawInstancedEntity);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	EntityDrawData &data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

DrawListAddress CDrawGrassBatchDC::SharedData(CGrassBatch * pEntity)
{
	//Also not the typical use case..
	DrawListAddress dataAddress;
	u32 dataSize = static_cast<u32>(sizeof(EntityDrawData));
	void *data = gDCBuffer->AddDataBlock(NULL, dataSize, dataAddress);

	reinterpret_cast<EntityDrawData *>(data)->Init(pEntity);

	gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
	gDrawListMgr->AddMapDataReference(pEntity->GetMapDataDefIndex());

	return dataAddress;
}

// --------------

void UpdateMatrixSetTask(const CTaskParams& params)
{
	grmMatrixSet* pMatrixSet = static_cast<grmMatrixSet*>(params.UserData[0].asPtr);
	crSkeleton* pSkeleton = static_cast<crSkeleton*>(params.UserData[1].asPtr);
	bool bIsSkinned = params.UserData[2].asBool;

	pMatrixSet->Update(*pSkeleton, bIsSkinned);
}

namespace FPSPedDraw {
	BankBool sEnable3rdPersonSkel = true;
}

const crSkeleton *GetSkeletonForDraw(const CEntity *entity)
{
	const crSkeleton *skeleton = entity->GetSkeleton();

#if FPS_MODE_SUPPORTED
	if(entity->GetIsTypePed() && !(DRAWLISTMGR->IsBuildingGBufDrawList()) && !(DRAWLISTMGR->IsBuildingSeeThroughDrawList()) && FPSPedDraw::sEnable3rdPersonSkel)
	{
		const crSkeleton *thirdPersonSkeleton = static_cast<const CPed *>(entity)->GetIkManager().GetThirdPersonSkeleton();
		if(thirdPersonSkeleton && static_cast<const CPed *>(entity)->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false))
			skeleton = thirdPersonSkeleton;
	}
#endif

	return skeleton;
}

const crSkeleton *GetSkeletonForDrawIgnoringDrawlist(const CEntity *entity)
{
	const crSkeleton *skeleton = entity->GetSkeleton();

#if FPS_MODE_SUPPORTED
	if(entity->GetIsTypePed() && FPSPedDraw::sEnable3rdPersonSkel)
	{
		const crSkeleton *thirdPersonSkeleton = static_cast<const CPed *>(entity)->GetIkManager().GetThirdPersonSkeleton();
		if(thirdPersonSkeleton && static_cast<const CPed *>(entity)->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false))
			skeleton = thirdPersonSkeleton;
	}
#endif

	return skeleton;
}

u16 CopyOffMatrixSet(const crSkeleton &skel, s32& drawListOffset, eSkelMatrixMode mode = SKEL_NORMAL, CBaseModelInfo* modelInfo = NULL, bool isLodded = false, u16* skelMap = NULL, u8 numSkelMapBones = 0, u8 skelMapComponentId = 0, u16 firstBoneToScale = 0, u16 lastBoneToScale = 0, float fScaleFactor = 1.0f) {

	DL_PF_FUNC( CopyOffMatrixSet );
	const void* id = &skel;

	if (skelMap)
	{
		Assertf(skelMapComponentId < sizeof(crSkeleton), "Component id used to augment the shared matrix set id is too large!");
		id = (char*)id + skelMapComponentId;
	}

	Assertf(fScaleFactor >= 0.0f, "Invalid draw scale factor!");

	dlSharedDataInfo *sharedDataInfo = gDCBuffer->LookupSharedDataById(DL_MEMTYPE_MATRIXSET, id);

	if (sharedDataInfo)
	{
		drawListOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_MATRIXSET, *sharedDataInfo);	// lookup to see if this skel has already been copied
	}
	else
	{
#if __BANK
		const atArray<dlSharedDataMapEntry> &sharedDataMap = gDCBuffer->GetSharedMemData(DL_MEMTYPE_MATRIXSET).GetSharedDataMap();
		if (sharedDataMap.GetCount() >= sharedDataMap.GetCapacity())
		{
			u32 totalMemory = 0;
			for (int i = 0; i < sharedDataMap.GetCount(); ++i)
			{
				size_t debugData = sharedDataMap[i].m_debugData;
				CBaseModelInfo* modelInfo = (CBaseModelInfo*)(debugData & ~0x1);
				crSkeleton *pSkel = (crSkeleton *)sharedDataMap[i].m_ID;

				u32 numBones = (debugData & 0x1) ? modelInfo->GetLodSkeletonBoneNum() : pSkel->GetBoneCount();
				u32 dataSize = grmMatrixSet::ComputeSize(numBones);
				dataSize =  (dataSize + 0x3) & ~0x3; // Align to 4 bytes

				Printf("Model Name %s, bone count %d, data size %d, lodded - %s\n", modelInfo->GetModelName(), numBones, dataSize, (debugData & 0x1) ? "true" : "false");
				totalMemory += dataSize;
			}
			Printf("\nTotal Memory use %.2fK\n", (float)totalMemory / 1024.0f);
		}
#endif

#if __BANK
		sharedDataInfo = &gDCBuffer->GetSharedMemData(DL_MEMTYPE_MATRIXSET).AllocateSharedData(id, (size_t)modelInfo | (isLodded ? 1 : 0));
#else
		sharedDataInfo = &gDCBuffer->GetSharedMemData(DL_MEMTYPE_MATRIXSET).AllocateSharedData(id);
#endif
		drawListOffset = -1;
	}

	Assert(!isLodded || modelInfo);
    if (isLodded && modelInfo->GetLodSkeletonBoneNum() == 0)
        isLodded = false;

	u32 numBones = skel.GetBoneCount();
	u32 numLoddedBones = isLodded ? modelInfo->GetLodSkeletonBoneNum() : numBones;

	Assertf(numLoddedBones < MAX_UINT16, "More bone indices than expected!");

	bool hasSkelMap = isLodded;
	u16* skelMapParam = const_cast<u16*>(modelInfo->GetLodSkeletonMap());

	// use skeleton map only when skeleton isn't lodded
	if (!isLodded && skelMap && numSkelMapBones)
	{
		hasSkelMap = true;
		skelMapParam = skelMap;
		numLoddedBones = numSkelMapBones;
	}

	u32 dataSize = grmMatrixSet::ComputeSize(numLoddedBones);

	Assertf(dataSize < MAX_DATA_BLOCK_BYTES, "Matrix set from : %s too big to store. %d bones.",modelInfo?modelInfo->GetModelName():"NULL", numBones);

	if (drawListOffset != -1)
	{
		// don't add a data block in this case - we are passing back the drawListOffset instead to use the existing copy
	} 
	else 
	{
		// copy off the global mtxs into the drawList
		// going to have to do some copying...
		DrawListAddress offset;
		grmMatrixSet* pDest = (grmMatrixSet*)gDCBuffer->AddDataBlock(NULL, dataSize, offset);
		drawListOffset = offset;
		gDCBuffer->AllocateSharedData(DL_MEMTYPE_MATRIXSET, *sharedDataInfo, dataSize, offset);
		//gDCBuffer->AddSkelCopy(id, gDCBuffer->GetDrawListOffset()); // add an empty data block big enough for mtxs
		Assert((numLoddedBones > 0) && (numBones > 0));
		grmMatrixSet::Create(pDest, numLoddedBones);

		sysDependency&	dependency = gDrawListMgr->CreateDrawlistDependency();

		const u32		flags =
			sysDepFlag::INPUT0						|
			sysDepFlag::INPUT1						|
			( hasSkelMap ? sysDepFlag::INPUT2 : 0 )	|
			sysDepFlag::OUTPUT3;

		dependency.Init( CopyOffMatrixSetSPU_Dependency, 0, flags );
		dependency.m_Priority = sysDependency::kPriorityMed;
		dependency.m_Params[0].m_AsPtr = const_cast< Mat34V* >( skel.GetObjectMtxs() );
		dependency.m_Params[1].m_AsPtr = const_cast< Mat34V* >( skel.GetSkeletonData().GetCumulativeInverseJoints() );
		dependency.m_Params[2].m_AsPtr = hasSkelMap ? skelMapParam : NULL;
		dependency.m_Params[3].m_AsPtr = pDest->GetMatrices();
		dependency.m_Params[4].m_AsShort.m_Low = (mode == SKEL_MODEL_RELATIVE);
		dependency.m_Params[4].m_AsShort.m_High = (u16)numLoddedBones;
		dependency.m_Params[5].m_AsShort.m_Low = firstBoneToScale;
		dependency.m_Params[5].m_AsShort.m_High = lastBoneToScale;
		dependency.m_Params[6].m_AsFloat = fScaleFactor;
		dependency.m_DataSizes[0] = sizeof(Mat34V) * numBones;
		dependency.m_DataSizes[1] = sizeof(Mat34V) * numBones;
		dependency.m_DataSizes[2] = sizeof(u16) * numLoddedBones;
		dependency.m_DataSizes[3] = sizeof(Matrix43) * numLoddedBones;

		// NOTE: The insert call is now deferred to the end of the drawlist; we want to group them so we
		// can minimize the overhead from signaling the dependency threads
		//sysDependencyScheduler::Insert( &dependency );
	}

	return(static_cast<u16>(dataSize));
}



grmMatrixSet* InitSkelWithData(u32 dataSize, s32 drawListOffset)
{
	grmMatrixSet * pData = reinterpret_cast<grmMatrixSet*>(gDCBuffer->GetDataBlock(dataSize, drawListOffset));
	return pData;
}

void InitShaderWithData(CCustomShaderEffectBase* pShader, u32 dataSize, DrawListAddress::Parameter drawListOffset){

	void* pData = gDCBuffer->GetDataBlock(dataSize, drawListOffset);
	sysMemCpy((void*) pShader, pData, dataSize);
}

// take the source skeleton & alloc memory for the matrix set to generate from it.
DECLARE_MTR_THREAD grmMatrixSet*	dlCmdAddSkeleton::ms_pCurrentMatSet = NULL;
DECLARE_MTR_THREAD bool			dlCmdAddSkeleton::ms_bStrippedHead = false;
dlCmdAddSkeleton::dlCmdAddSkeleton(crSkeleton* pSourceSkel, eSkelMatrixMode skelMatMode)
: m_AddSkeleton(pSourceSkel, skelMatMode)
{
	ms_bStrippedHead = false;
}

// on execution pickup the created matrix set and set it as the current matrix set ready for subsequent commands
void dlCmdAddSkeleton::Execute()
{
	m_AddSkeleton.Execute();
}

void dlCmdAddSkeleton::ExecuteCore(u32 skelDataSize, s32 skelDrawListOffset)
{
	ms_pCurrentMatSet = InitSkelWithData(skelDataSize, skelDrawListOffset);
}

// --- override skeleton data ---
dlCmdOverrideSkeleton::dlCmdOverrideSkeleton(grmMatrixSet* pMtxSetOverride, Mat34V_In rootMatOverride) 
: m_mtxSetOverride(pMtxSetOverride)
, m_rootMtxOverride(rootMatOverride)
, m_bOnlyOverrideRootMtx(false)
{
}

dlCmdOverrideSkeleton::dlCmdOverrideSkeleton(Mat34V_In rootMatOverride) 
: m_mtxSetOverride(NULL)
, m_rootMtxOverride(rootMatOverride)
, m_bOnlyOverrideRootMtx(true)
{
}


dlCmdOverrideSkeleton::dlCmdOverrideSkeleton(bool) 
: m_mtxSetOverride(NULL)
, m_bOnlyOverrideRootMtx(false)
{
}

void dlCmdOverrideSkeleton::Execute()
{
	if (m_bOnlyOverrideRootMtx == false)
	{
		dlCmdAddCompositeSkeleton::SetCurrentMatrixSetOverride(m_mtxSetOverride, m_rootMtxOverride);
	}
	else
	{
		dlCmdAddCompositeSkeleton::SetCurrentRootMatrixOverride(m_rootMtxOverride);
	}
}


DECLARE_MTR_THREAD grmMatrixSet*	dlCmdAddCompositeSkeleton::ms_pCurrentMatSet = NULL;
DECLARE_MTR_THREAD grmMatrixSet*	dlCmdAddCompositeSkeleton::ms_pCurrentMatSetOverride = NULL;
DECLARE_MTR_THREAD ThreadMat34V		dlCmdAddCompositeSkeleton::ms_rootMatrixOverride;
DECLARE_MTR_THREAD u8				dlCmdAddCompositeSkeleton::ms_numHeadBones = 0;
DECLARE_MTR_THREAD bool			dlCmdAddCompositeSkeleton::ms_bOnlyOverrideRootMtx = false;
DECLARE_MTR_THREAD bool			dlCmdAddCompositeSkeleton::ms_bStrippedHead = false;
dlCmdAddCompositeSkeleton::dlCmdAddCompositeSkeleton()
: m_AddCompositeSkeleton(true)
{
	ms_bStrippedHead = false;
}

// on execution pickup the created matrix set and set it as the current matrix set ready for subsequent commands
void dlCmdAddCompositeSkeleton::Execute(u32 idx)
{
	m_AddCompositeSkeleton.Execute(idx);
}

void dlCmdAddCompositeSkeleton::ExecuteCore(u32 skelDataSize, s32 skelDrawListOffset, u8 numHeadBones)
{
	ms_pCurrentMatSet = InitSkelWithData(skelDataSize, skelDrawListOffset);
	ms_numHeadBones = numHeadBones;
}


// --- draw a player ---
CDrawStreamPedDC::CDrawStreamPedDC(CEntity *pEntity)
: m_AddCompositeSkeleton(static_cast<CDynamicEntity*>(pEntity), SKEL_MODEL_RELATIVE)
, m_DebugInfo(pEntity)
{
	// EJ: Memory Optimization
	m_numProps = pEntity->GetNumProps();
	m_isFPVPed = pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV);

	switch (m_numProps)
	{
	case 0:		m_dataAddress = SharedData(pEntity);
				break;
	case 1:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<1, 3>(pEntity) : SharedDataWithProps<1, 1>(pEntity);
				break;
	case 2:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<2, 3>(pEntity) : SharedDataWithProps<2, 1>(pEntity);
				break;
	case 3:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<3, 3>(pEntity) : SharedDataWithProps<3, 1>(pEntity);
				break;
	case 4:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<4, 3>(pEntity) : SharedDataWithProps<4, 1>(pEntity);
				break;
	case 5:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<5, 3>(pEntity) : SharedDataWithProps<5, 1>(pEntity);
				break;
	case 6:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<6, 3>(pEntity) : SharedDataWithProps<6, 1>(pEntity);
				break;
	default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	m_perComponentWetness = pPed->GetPedDrawHandler().GetPerComponentWetnessFlags();
}

CDrawStreamPedDC::CDrawStreamPedDC(CEntity* pEntity, bool /*noImplicitAddSkeleton*/)
: m_AddCompositeSkeleton(false)
, m_DebugInfo(pEntity)
{
	// EJ: Memory Optimization
	m_numProps = pEntity->GetNumProps();
	m_isFPVPed = pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV);

	switch (m_numProps)
	{
	case 0:		m_dataAddress = SharedData(pEntity);
				break;
	case 1:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<1, 3>(pEntity) : SharedDataWithProps<1, 1>(pEntity);
				break;
	case 2:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<2, 3>(pEntity) : SharedDataWithProps<2, 1>(pEntity);
				break;
	case 3:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<3, 3>(pEntity) : SharedDataWithProps<3, 1>(pEntity);
				break;
	case 4:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<4, 3>(pEntity) : SharedDataWithProps<4, 1>(pEntity);
				break;
	case 5:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<5, 3>(pEntity) : SharedDataWithProps<5, 1>(pEntity);
				break;
	case 6:		m_dataAddress = m_isFPVPed ? SharedDataWithProps<6, 3>(pEntity) : SharedDataWithProps<6, 1>(pEntity);
				break;
	default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	m_perComponentWetness = pPed->GetPedDrawHandler().GetPerComponentWetnessFlags();
}

RAGETRACE_DECL(DrawPlayerBIG);

void CDrawStreamPedDC::Execute()
{
	RAGETRACE_SCOPE(DrawPlayerBIG);
	Assert(!m_dataAddress.IsNULL());

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	dlCmdAddCompositeSkeleton::SetStrippedHead(m_AddCompositeSkeleton.GetIsStrippedHead());

	// EJ: Memory Optimization
	switch (m_numProps)
	{
		case 0:
		{
			CEntityDrawDataStreamPed& data = GetDrawData();
			RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			break;
		}
		case 1:
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<1, 3>& data = GetDrawDataWithProps<1, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<1, 1>& data = GetDrawDataWithProps<1, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		case 2:
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<2, 3>& data = GetDrawDataWithProps<2, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<2, 1>& data = GetDrawDataWithProps<2, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		case 3:		
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<3, 3>& data = GetDrawDataWithProps<3, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<3, 1>& data = GetDrawDataWithProps<3, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		case 4:		
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<4, 3>& data = GetDrawDataWithProps<4, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<4, 1>& data = GetDrawDataWithProps<4, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		case 5:		
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<5, 3>& data = GetDrawDataWithProps<5, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<5, 1>& data = GetDrawDataWithProps<5, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		case 6:		
		{
			if(m_isFPVPed)
			{
				CEntityDrawDataStreamPedWithProps<6, 3>& data = GetDrawDataWithProps<6, 3>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			else
			{
				CEntityDrawDataStreamPedWithProps<6, 1>& data = GetDrawDataWithProps<6, 1>();
				RAGETRACE_CALL(data.Draw(m_AddCompositeSkeleton, (u8)m_AddCompositeSkeleton.GetLodIdx(), m_perComponentWetness));
			}
			break;
		}
		default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}

	dlCmdAddCompositeSkeleton::SetStrippedHead(false);
}

DrawListAddress CDrawStreamPedDC::SharedData(CEntity* pEntity)
{
	return CopyOffEntity<CEntityDrawDataStreamPed>(pEntity);
}

#if __DEV
const char *CDrawStreamPedDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawStreamPedDC *pDLC = (CDrawStreamPedDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV


CDrawDetachedPedPropDC::CDrawDetachedPedPropDC(CEntity* pEntity)
: m_DebugInfo(pEntity)
{
	m_dataAddress = CopyOffEntity<CEntityDrawDataDetachedPedProp>(pEntity);
}

void CDrawDetachedPedPropDC::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	CEntityDrawDataDetachedPedProp & data = GetDrawData();
	data.Draw();
}

#if __DEV
const char *CDrawDetachedPedPropDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawDetachedPedPropDC *pDLC = (CDrawDetachedPedPropDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

CAddSkeletonCommand::CAddSkeletonCommand(bool /*disabled*/) {
	m_skelDrawListOffset = -2;
}

CAddSkeletonCommand::CAddSkeletonCommand(CDynamicEntity *entity, int skelMatrixMode, bool damaged) {
	m_strippedHead = false;
	const crSkeleton *skeleton = GetSkeletonForDraw(entity);

	m_damaged = damaged;
	if (damaged && entity->GetFragInst() && entity->GetFragInst()->GetCached())
	{
		fragCacheEntry* entry = entity->GetFragInst()->GetCacheEntry();
		Assert(entry);
		if (entry->GetHierInst() && entry->GetHierInst()->anyGroupDamaged)
		{
            skeleton = entry->GetHierInst()->damagedSkeleton;
		}
	}

	if (skeleton){
		m_skelDrawListOffset = -1;

		if (skelMatrixMode == -1) {
			skelMatrixMode = entity->GetSkelMode();
		}

        float fScaleFactor = 1.0f;
        u16 firstBoneToScale = 0;
        u16 lastBoneToScale = 0;

		bool isLodded = false;
		if (entity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(entity);
			CPedModelInfo* pedMi = pPed->GetPedModelInfo();

			m_lodIdx = pPed->GetModelLodIndex();		// get the model LOD we should be using for this type

			//isLodded = m_lodIdx >= 3;
			isLodded = m_lodIdx >= pedMi->GetNumAvailableLODs();

            // handle scaling
            if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseAmbientModelScaling))
            {
                fScaleFactor = pPed->GetRandomNumberInRangeFromSeed(CPed::ms_minAmbientDrawingScaleFactor, CPed::ms_maxAmbientDrawingScaleFactor);
                firstBoneToScale = 0;
                lastBoneToScale = (u16)skeleton->GetBoneCount();
            }
			else if ((pPed->GetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible)
						REPLAY_ONLY(&& (!CReplayMgr::IsEditModeActive() || camInterface::GetReplayDirector().IsRecordedCamera()))) ||
						camInterface::ComputeShouldMakePedHeadInvisible(*pPed))
			{
#if 1
				m_strippedHead = true;
#else
                s32 headBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
                if (headBoneIndex != BONETAG_INVALID)
                {
                    u32 endBoneIndex = pPed->GetSkeleton()->GetTerminatingPartialBone((u32)headBoneIndex);
                    if (endBoneIndex != BONETAG_INVALID)
                    {
                        fScaleFactor = 0.f;
                        firstBoneToScale = (u16)headBoneIndex;
                        lastBoneToScale = (u16)(endBoneIndex - 1);
                    }
                }
#endif
            }
		}

        if (entity->GetIsTypeVehicle())
        {
            // vehicles should have a lod skeleton setup to strip any non skinned bones from being copied here
            isLodded = true;
        }

		m_skelDataSize = CopyOffMatrixSet(*skeleton, m_skelDrawListOffset, (eSkelMatrixMode) skelMatrixMode, entity->GetBaseModelInfo(), isLodded, NULL, 0, 0, firstBoneToScale, lastBoneToScale, fScaleFactor);
	} else {
		m_skelDrawListOffset = -2;
		Assertf(false, "Calling CAddSkeletonCommand on an entity without a skeleton");
	}
}

CAddSkeletonCommand::CAddSkeletonCommand(crSkeleton* pSourceSkel, eSkelMatrixMode skelMatMode) {
	if (pSourceSkel){
		m_skelDrawListOffset = -1;
		m_skelDataSize = CopyOffMatrixSet(*pSourceSkel, m_skelDrawListOffset, skelMatMode);
	} else {
		m_skelDrawListOffset = -2;
	}
}

void CAddSkeletonCommand::Execute() {
	if (m_skelDrawListOffset != -2) {
		dlCmdAddSkeleton::ExecuteCore(m_skelDataSize, m_skelDrawListOffset);
	}
}

CAddCompositeSkeletonCommand::CAddCompositeSkeletonCommand(bool /*disabled*/) {
	m_skelDrawListOffset[0] = -2;
}

CAddCompositeSkeletonCommand::CAddCompositeSkeletonCommand(CDynamicEntity *entity, int skelMatrixMode) {
	m_strippedHead = false;

	const crSkeleton *skeleton = GetSkeletonForDraw(entity);

	if (skeleton){
		m_skelDrawListOffset[0] = -1;

		if (skelMatrixMode == -1) {
			skelMatrixMode = entity->GetSkelMode();
		}

		bool isLodded = false;
		u16* skelMap = NULL;
		u8 numSkelMapBones = 0;
		m_numHeadBones = numSkelMapBones;
		if (entity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(entity);
			CPedModelInfo* pedMi = pPed->GetPedModelInfo();

			m_lodIdx = pPed->GetModelLodIndex();		// get the model LOD we should be using for this type

			// in hud menu we want high lods
			if (DRAWLISTMGR->IsBuildingHudDrawList())
				m_lodIdx = 0;

            // handle scaling
            float fScaleFactor = 1.0f;
            u16 firstBoneToScale = 0;
            u16 lastBoneToScale = 0;

            if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseAmbientModelScaling))
            {
                fScaleFactor = pPed->GetRandomNumberInRangeFromSeed(CPed::ms_minAmbientDrawingScaleFactor, CPed::ms_maxAmbientDrawingScaleFactor);
                firstBoneToScale = 0;
                lastBoneToScale = (u16)skeleton->GetBoneCount();
            }
			else if ((pPed->GetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible)
						REPLAY_ONLY(&& (!CReplayMgr::IsEditModeActive() || camInterface::GetReplayDirector().IsRecordedCamera()))) ||
						camInterface::ComputeShouldMakePedHeadInvisible(*pPed))
			{
#if 1
				m_strippedHead = true;
#else
                s32 headBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
                if (headBoneIndex != BONETAG_INVALID)
                {
                    u32 endBoneIndex = pPed->GetSkeleton()->GetTerminatingPartialBone((u32)headBoneIndex);
                    if (endBoneIndex != BONETAG_INVALID)
                    {
                        fScaleFactor = 0.f;
                        firstBoneToScale = (u16)headBoneIndex;
                        lastBoneToScale = (u16)(endBoneIndex - 1);
                    }
                }
#endif
            }

			//isLodded = m_lodIdx >= 3;
			isLodded = m_lodIdx >= pedMi->GetNumAvailableLODs();

			if (!pedMi->GetIsStreamedGfx())
			{
				m_skelDataSize[0] = CopyOffMatrixSet(*skeleton, m_skelDrawListOffset[0], (eSkelMatrixMode) skelMatrixMode, entity->GetBaseModelInfo(), isLodded, skelMap, numSkelMapBones, 0, firstBoneToScale, lastBoneToScale, fScaleFactor);
			}
			else
			{
				CPed* ped = (CPed*)entity;
				if (ped->GetPedDrawHandler().GetPedRenderGfx())
				{
					if (!ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMap[PV_COMP_HEAD] || isLodded)
					{
						m_skelDataSize[0] = CopyOffMatrixSet(*skeleton, m_skelDrawListOffset[0], (eSkelMatrixMode) skelMatrixMode, entity->GetBaseModelInfo(), isLodded, skelMap, numSkelMapBones, 0, firstBoneToScale, lastBoneToScale, fScaleFactor);

						for (s32 i = 1; i < PV_MAX_COMP; ++i)
						{
							m_skelDataSize[i] = 0;
							m_skelDrawListOffset[i] = -2;
						}
					}
					else
					{
						for (s32 i = 0; i < PV_MAX_COMP; ++i)
						{
							if (ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMap[i] && ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMapBoneCount[i] > 0)
								m_skelDataSize[i] = CopyOffMatrixSet(*skeleton, m_skelDrawListOffset[i], (eSkelMatrixMode) skelMatrixMode, entity->GetBaseModelInfo(), isLodded, ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMap[i], ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMapBoneCount[i], (u8)i, firstBoneToScale, lastBoneToScale, fScaleFactor);
							else
								m_skelDataSize[i] = 0;
						}

						m_numHeadBones = ped->GetPedDrawHandler().GetPedRenderGfx()->m_skelMapBoneCount[PV_COMP_HEAD];
					}
				}
			}
		}
		else
		{
			m_skelDrawListOffset[0] = -2;
			Assertf(false, "Has support for non peds been added to composite skeletons?");
		}
	} else {
		m_skelDrawListOffset[0] = -2;
		Assertf(false, "Calling CAddCompositeSkeletonCommand on an entity without a skeleton");
	}
}

void CAddCompositeSkeletonCommand::Execute(u32 idx) {
	if (m_skelDrawListOffset[0] != -2 && m_skelDrawListOffset[idx] != -2 && m_skelDataSize[idx] != 0) {
		dlCmdAddCompositeSkeleton::ExecuteCore(m_skelDataSize[idx], m_skelDrawListOffset[idx], m_numHeadBones);
	}
}

CDrawPedBIGDC::CDrawPedBIGDC(CEntity *pEntity)
: m_AddSkeleton(static_cast<CDynamicEntity*>(pEntity), SKEL_MODEL_RELATIVE)
, m_DebugInfo(pEntity)
{
	// EJ: Memory Optimization
	m_numProps = pEntity->GetNumProps();

	switch (m_numProps)
	{
	case 0:		m_dataAddress = SharedData(pEntity);
				break;
	case 1:		m_dataAddress = SharedDataWithProps<1>(pEntity);
				break;
	case 2:		m_dataAddress = SharedDataWithProps<2>(pEntity);
				break;
	case 3:		m_dataAddress = SharedDataWithProps<3>(pEntity);
				break;
	case 4:		m_dataAddress = SharedDataWithProps<4>(pEntity);
				break;
	case 5:		m_dataAddress = SharedDataWithProps<5>(pEntity);
				break;
	case 6:		m_dataAddress = SharedDataWithProps<6>(pEntity);
				break;
	default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}
}

RAGETRACE_DECL(DrawPedBIG);

void CDrawPedBIGDC::Execute()
{
	RAGETRACE_SCOPE(DrawPedBIG);

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	m_AddSkeleton.Execute();
	Assert(!m_dataAddress.IsNULL());

	dlCmdAddSkeleton::SetStrippedHead(m_AddSkeleton.GetIsStrippedHead());

	// EJ: Memory Optimization
	switch (m_numProps)
	{
		case 0:		
		{
			CEntityDrawDataPedBIG& data = GetDrawData();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 1:
		{
			CEntityDrawDataPedBIGWithProps<1>& data = GetDrawDataWithProps<1>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 2:
		{
			CEntityDrawDataPedBIGWithProps<2>& data = GetDrawDataWithProps<2>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 3:
		{
			CEntityDrawDataPedBIGWithProps<3>& data = GetDrawDataWithProps<3>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 4:
		{
			CEntityDrawDataPedBIGWithProps<4>& data = GetDrawDataWithProps<4>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 5:
		{
			CEntityDrawDataPedBIGWithProps<5>& data = GetDrawDataWithProps<5>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		case 6:
		{
			CEntityDrawDataPedBIGWithProps<6>& data = GetDrawDataWithProps<6>();
			RAGETRACE_CALL(data.Draw((u8)m_AddSkeleton.GetLodIdx()));
			break;
		}
		default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}

	dlCmdAddSkeleton::SetStrippedHead(false);
}

DrawListAddress CDrawPedBIGDC::SharedData(CEntity * pEntity)
{
	return CopyOffEntity<CEntityDrawDataPedBIG>(pEntity);
}

#if __DEV
const char *CDrawPedBIGDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawPedBIGDC *pDLC = (CDrawPedBIGDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

CDrawVehicleVariationDC::CDrawVehicleVariationDC(CEntity *pEntity)
{
	// add shared data
	{
		int sharedMemType = DL_MEMTYPE_ENTITY;
		Assertf(pEntity->GetBaseModelInfo()->GetModelType() == MI_TYPE_VEHICLE, "Trying to render vehicle variation with a non vehicle entity!");

		CVehicle* veh = (CVehicle*)pEntity;
		dlSharedDataInfo& sharedVariationDataInfo = veh->GetVehicleDrawHandler().GetSharedVariationDataOffset();

		m_dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedVariationDataInfo);

		if(m_dataAddress.IsNULL() || !g_cache_entities)
		{
			u32 dataSize = (u32) sizeof(CEntityDrawDataVehicleVar);
			void * data = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
			if(g_cache_entities)
			{
				gDCBuffer->AllocateSharedData(sharedMemType, sharedVariationDataInfo, dataSize, m_dataAddress);
			}

			reinterpret_cast<CEntityDrawDataVehicleVar*>(data)->Init(pEntity);
		}
#if RAGE_INSTANCED_TECH
		else
		{
			if (pEntity->GetViewportInstancedRenderBit() != 0)
			{
				u32 dataSize = (u32) sizeof(CEntityDrawDataVehicleVar);
				void *data = gDCBuffer->GetDataBlock(dataSize,m_dataAddress);
				reinterpret_cast<CEntityDrawDataVehicleVar*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
			}
		}
#endif
	}
	gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
}

RAGETRACE_DECL(DrawVehicleVariation);

void CDrawVehicleVariationDC::Execute()
{
	RAGETRACE_SCOPE(DrawVehicleVariation);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	CEntityDrawDataVehicleVar & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

void CDrawVehicleVariationDC::SetupBurstWheels(const u8* burstAndSideRatios)
{
	CEntityDrawDataVehicleVar & data = GetDrawData();
	sysMemCpy(&data.m_wheelBurstRatios[0][0], burstAndSideRatios, sizeof(u8)*NUM_VEH_CWHEELS_MAX*2);
	data.m_hasBurstWheels = true;
}

// ------------------
// - frag object (with skeleton - assumes is a vehicle)-
CDrawFragDC::CDrawFragDC(CEntity* pEntity, bool damaged)
: m_AddSkeleton(static_cast<CDynamicEntity*>(pEntity), -1, damaged)
, m_DebugInfo(pEntity)
{
    // add shared data
    {
        int sharedMemType = DL_MEMTYPE_ENTITY;
        dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

        m_dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

        if(m_dataAddress.IsNULL() || !g_cache_entities)
        {
            u32 dataSize = (u32) sizeof(CEntityDrawDataFrag);
            void * data = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
            if(g_cache_entities)
            {
                gDCBuffer->AllocateSharedData(DL_MEMTYPE_ENTITY, sharedDataInfo, dataSize, m_dataAddress);
            }

            reinterpret_cast<CEntityDrawDataFrag*>(data)->Init(pEntity);

			gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
			if (pEntity->GetIsCurrentlyHD()){
				gDrawListMgr->AddArchetype_HD_Reference(pEntity->GetModelIndex());
			}
        }
#if RAGE_INSTANCED_TECH
		else
		{
			if (pEntity->GetViewportInstancedRenderBit() != 0)
			{
				u32 dataSize = (u32) sizeof(CEntityDrawDataFrag);
				void *data = gDCBuffer->GetDataBlock(dataSize,m_dataAddress);
				reinterpret_cast<CEntityDrawDataFrag*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
			}
		}
#endif
    }
}

RAGETRACE_DECL(DrawFrag);

void CDrawFragDC::Execute()
{
	RAGETRACE_SCOPE(DrawFrag);
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	m_AddSkeleton.Execute();
	Assert(!m_dataAddress.IsNULL());
	CEntityDrawDataFrag & data = GetDrawData();
	RAGETRACE_CALL(data.Draw(m_AddSkeleton.GetDamaged()));
}

void CDrawFragDC::SetupBurstWheels(const u8* burstAndSideRatios)
{
    CEntityDrawDataFrag & data = GetDrawData();
	sysMemCpy(&data.m_wheelBurstRatios[0][0], burstAndSideRatios, sizeof(u8)*NUM_VEH_CWHEELS_MAX*2);
	data.m_flags |= WHEEL_BURST_RATIOS;
}

#if __DEV
const char *CDrawFragDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawFragDC *pDLC = (CDrawFragDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV


void CDrawBreakableGlassDC::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// Make sure this is the right combination of render mode/bucket
	Assertf(!DRAWLISTMGR->IsExecutingShadowDrawList() && (gDrawListMgr->GetRtBucket() == CRenderer::RB_ALPHA || gDrawListMgr->GetRtBucket() == CRenderer::RB_CUTOUT), "Trying to render breakable glass with the an unsupported render mode/bucket");
	if(gDrawListMgr->GetRtBucket() == CRenderer::RB_CUTOUT)
	{
		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
	}
	
	m_data.Draw();
}

dlCmdDrawTwoSidedDrawable::dlCmdDrawTwoSidedDrawable(rmcDrawable *drawable, Vector3& vOffset, s32 
#if HACK_GTA4_MODELINFOIDX_ON_SPU
									 modelInfoIdx
#endif
									 , u32 alphaFade, u32 naturalAmb, u32 artificialAmb, u32 matID, bool inInterior,
									 grcRasterizerStateHandle rsHandle, grcDepthStencilStateHandle dssHandle, bool BANK_ONLY(captureStats), bool shadowWithShaders, bool bIsVehCloth, fwEntity* ENTITYSELECT_ONLY(pEntityForId) )
{
	m_drawable = drawable;
	m_offset = vOffset;

	m_fadeAlpha = (u8)alphaFade;
	m_naturalAmb = (u8)naturalAmb;
	m_artificialAmb = (u8)artificialAmb;
	m_matID = (u8)matID;
	m_bIsInInterior = (u8)inInterior;

	m_rsState = rsHandle;
	m_dssState = dssHandle;

	m_bShadowWithShaders = shadowWithShaders;
	m_bIsVehicleCloth = bIsVehCloth;

#if HACK_GTA4_MODELINFOIDX_ON_SPU	
	Assert(modelInfoIdx <= 65535);
	m_modelInfoIdx	= (u16)modelInfoIdx;
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU	

#if __BANK
	m_captureStats = captureStats;
#if ENTITYSELECT_ENABLED_BUILD
	m_entityId = CEntityIDHelper::ComputeEntityID(pEntityForId);
#endif // ENTITYSELECT_ENABLED_BUILD
#endif // __BANK
}

RAGETRACE_DECL(DrawDrawable);

void dlCmdDrawTwoSidedDrawable::UpdateVehicleCloth()
{
	Assert(m_bIsVehicleCloth);
	CCustomShaderEffectBase *pCSE = CCustomShaderEffectDC::GetLatestShader();
	if(pCSE && pCSE->GetType()==CSE_VEHICLE)
	{
		// CSE for vehicle cloth (note: uses SD CSE):
		CCustomShaderEffectVehicle *pVehicleCSE = (CCustomShaderEffectVehicle*)pCSE;
		Assert(pVehicleCSE->GetIsHighDetail()==false);	// only SD CSE allowed for vehicle cloth
		if(!pVehicleCSE->GetIsHighDetail())
		{
			Assertf(m_drawable->GetShaderGroup().GetShaderGroupVarCount()>0, "CSEVehicle: Uninitialized cloth drawable!");
			if(m_drawable->GetShaderGroup().GetShaderGroupVarCount()>0)
			{
				pVehicleCSE->SetShaderVariables(m_drawable);
			}
		}
	}
}

void dlCmdDrawTwoSidedDrawable::Execute()
{
	RAGETRACE_SCOPE(DrawDrawable);
	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if( m_drawable )
	{
		START_SPU_STAT_RECORD(m_captureStats);
#if DEBUG_ENTITY_GPU_COST
		if(m_captureStats)
		{
			gDrawListMgr->StartDebugEntityTimer();
		}
#endif
		const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
		const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
		const bool isHeightmapDrawList = DRAWLISTMGR->IsExecutingRainCollisionMapDrawList();
		const bool bDontWriteDepth = false;
		const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple) && 
								!isGBufferDrawList;
		const bool setStipple = isGBufferDrawList;
		CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, (u32)m_fadeAlpha, setStipple, false, (u32)m_fadeAlpha, bDontWriteDepth, false, false);
		CShaderLib::SetGlobalDeferredMaterial(m_matID);
		CShaderLib::SetGlobalInInterior(m_bIsInInterior);

		// Thanks to the nice mix of world/object space bullshit going on in there,
		// We turn off geometry culling and rely on the engine to do the right thing...
		grmModel::ModelCullbackType prevCullBack = grmModel::GetModelCullback();
		grmModel::SetModelCullback(NULL);

#if HACK_GTA4_MODELINFOIDX_ON_SPU
		DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU

		if (isShadowDrawList || isHeightmapDrawList)
		{
#if HAS_RENDER_MODE_SHADOWS
			if (!IsRenderingShadowsSkinned(renderMode)) // do we need this test?
#endif // HAS_RENDER_MODE_SHADOWS
			{
				CRenderPhaseCascadeShadowsInterface::SetRasterizerState(CSM_RS_TWO_SIDED); // sets grcRSV::CULL_NONE but leaves the depth/slope bias intact
#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
				m_bShadowWithShaders = grmModel::GetForceShader() ? false : m_bShadowWithShaders;

				Matrix34 M = M34_IDENTITY;
				M.d = m_offset;

				if(m_bIsVehicleCloth)
				{
					UpdateVehicleCloth();
				}

				//If m_ShadowWithShaders set draw with shaders - mainly used for vehicles with cloth that expects a skinned shader for the cloth which it doesnt have so needs setting up correctly.
				if( m_bShadowWithShaders )
					m_drawable->Draw(M, bucketMask, 0, 0);
				else
				{
				#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
					m_drawable->DrawNoShaders(M, bucketMask, 0);
				#else
					// when drawing the tessellated shadows, we have no forced shader set, but the render mode is rmStandard (opposed to rmShadows) 
					// to indicate we want to render with shaders.
					if(renderMode == rmStandard)
						m_drawable->Draw(M, bucketMask, 0, 0);
					else
						m_drawable->DrawNoShadersTessellationControlled(M, bucketMask, 0);
				#endif
				}
#else
				m_drawable->Draw(M, bucketMask, 0, 0);
#endif
				CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState(); // restores grcRSV::CULL_BACK and leaves the depth/slope bias intact
			}
		}
		else
		{
			CCustomShaderEffectBase *pCSE = CCustomShaderEffectDC::GetLatestShader();
			if(m_bIsVehicleCloth)
			{
				UpdateVehicleCloth();
			}
			else
			{
				if(pCSE && pCSE->GetType()==CSE_TINT)
				{
					// CSE for tinted cloth:
					CCustomShaderEffectTint *pTintCSE = (CCustomShaderEffectTint*)pCSE;
					pTintCSE->SetShaderVariables(m_drawable);
				}
			}

#if ENTITYSELECT_ENABLED_BUILD
			const int bucket = (int)DRAWLISTMGR->GetRtBucket();
			if (CEntityDrawDataCommon::SetShaderParamsStatic(m_entityId, bucket))
#endif // ENTITYSELECT_ENABLED_BUILD
			{
				// Draw two-sided (no backface culling)
				const grcRasterizerStateHandle rs_prev = grcStateBlock::RS_Active;
				grcStateBlock::SetRasterizerState(m_rsState);

				const grcDepthStencilStateHandle dss_prev = grcStateBlock::DSS_Active;
				grcStateBlock::SetDepthStencilState(m_dssState, CShaderLib::GetGlobalDeferredMaterial());

				Matrix34 M = M34_IDENTITY;
				M.d = m_offset;
				m_drawable->Draw(M, bucketMask, 0, 0);

				grcStateBlock::SetRasterizerState(rs_prev);
				grcStateBlock::SetDepthStencilState(dss_prev);
			}
		}

		DbgCleanDrawableModelIdxForSpu();

		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
		CShaderLib::ResetAlpha(setAlpha,setStipple);
		
		grmModel::SetModelCullback(prevCullBack);

#if DEBUG_ENTITY_GPU_COST
		if(m_captureStats)
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
#endif
		STOP_SPU_STAT_RECORD(m_captureStats);
	}
}


//
//
// PSN: no instanced wheel renderer (until full Edge integration):
//
#define USE_INSTANCED_WHEEL_RENDERER		(1)

// tyreDeformParams:
// "left-right" side deform scale <0; 1.0f> (bigger = tyre more flat)
dev_float tyreDeformScaleX = 0.6f;
// "up-down" deform scale towards alloy <0; 0.20f> (bigger = tyre more sticks to alloy)
dev_float tyreDeformScaleYZ		= 0.10f;	
// balances flat tyre point: 0=none, >0 - goes outside, <0 - goes inside
dev_float tyreDeformContactPointX	= 0.0f;	//-0.02f; 0.02f

dev_bool sbUseBurstDepth = true;
dev_float sfTyreSideMult = 0.1f;

// inverse of 255
const float inv255 = 1.0f / 255.0f;
void FragDrawCarWheels(CVehicleModelInfo* pModelInfo, const Matrix34& rootMatrix, const grmMatrixSet *ms, u64 destroyedBits, int bucket, bool wheelBurstRatios, u8 m_wheelBurstRatios[NUM_VEH_CWHEELS_MAX][2], float dist, u8 flags, CVehicleStreamRenderGfx* renderGfx, bool canHaveRearWheel, float lodMult)
{
	fragType* pFragType = pModelInfo->GetFragType();

static dev_bool bForceTireLodShadows = true;
	const bool bIsShadowTirePass = bForceTireLodShadows && DRAWLISTMGR->IsExecutingShadowDrawList();
	
    bool allowLodding = true;
	if((g_pBreakable->GetIsHd() || (flags&CDrawFragDC::HD_REQUIRED)) && (!bIsShadowTirePass))
	{
		if (pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
		{
			CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pModelInfo);
			gtaFragType* pFragTypeHd = pVMI->GetHDFragType();
			if (pFragTypeHd != NULL)
			{
				bool bAllowHD = true;
				gDrawListMgr->AdjustVehicleHD(true, bAllowHD);
				if (bAllowHD)
				{
                    pFragType = pFragTypeHd;
                    allowLodding = false; // HD mesh has no lods and in some situations it can be forced to render beyond the hd distance (e.g. cutscenes)
				}
			}
		} 
	}

	if (!Verifyf(pFragType, "Model info '%s' with NULL fragType was passed in to FragDrawCarWheels", pModelInfo->GetModelName()))
            return;

	// calculate which lods to render
	float unscaledLowLodDistance = pModelInfo->GetVehicleLodDistance(VLT_LOD1);
	float unscaledLowerLodDistance = pModelInfo->GetVehicleLodDistance(VLT_LOD2);

	// push the lower lods to a minimum distance for wrecked cars to avoid popping of broken parts and other damage
	if (g_pBreakable->GetIsWreckedVehicle())
	{
		if (unscaledLowLodDistance < 70.f)
			unscaledLowLodDistance = 70.f;
		if (unscaledLowerLodDistance < 70.f)
			unscaledLowerLodDistance = 70.f;
	}

	float scaledHighLODDistance = pModelInfo->GetVehicleLodDistance(VLT_LOD0) * g_LodScale.GetGlobalScaleForRenderThread() * lodMult;
	float scaledLowLODDistance = unscaledLowLodDistance * g_LodScale.GetGlobalScaleForRenderThread() * lodMult * g_pBreakable->GetLowLodMultiplier();
	float scaledLowerLODDistance = unscaledLowerLodDistance * g_LodScale.GetGlobalScaleForRenderThread() * lodMult * g_pBreakable->GetLowLodMultiplier();

	int nLodIndex = 2;	// low LOD
    if (allowLodding)
    {
        if (dist < scaledHighLODDistance)
            nLodIndex = 0;	// high LOD
		else if (dist < scaledLowLODDistance)
            nLodIndex = 1; // mid LOD
        else if (dist > scaledLowerLODDistance)
            return;	// lower LOD (no lower lod for instanced wheels)

        u32 newLod = (u32)nLodIndex;
        gDrawListMgr->AdjustFragmentLOD(true, newLod, pModelInfo->GetModelType() == MI_TYPE_VEHICLE);
        if (newLod > 2)
            return;

        nLodIndex = (int)newLod;

		s8 clampedLod = g_pBreakable->GetClampedLod();
		if (clampedLod > -1)
		{
			if (clampedLod == 3)
				return;

			if (clampedLod < nLodIndex)
				nLodIndex = clampedLod;
		}
    }
    else
    {
        nLodIndex = 0;
    }

	const bool isHeightmapDrawList = DRAWLISTMGR->IsExecutingRainCollisionMapDrawList();
	static dev_float ShadowTireLodDistance = 0.33f;
	if((bIsShadowTirePass || isHeightmapDrawList) && dist > (scaledHighLODDistance* ShadowTireLodDistance))
	{
		nLodIndex = 1;	// force tire SD LOD1 for shadows
	}

#if USE_INSTANCED_WHEEL_RENDERER
	CWheelInstanceRenderer::InitBeforeRender();
	int nLastDrawChild = -1;
#endif //USE_INSTANCED_WHEEL_RENDERER...

	float tyreWidthScaleFront = 1.f;
	float tyreWidthScaleRear = 1.f;
	float tyreRadiusScaleFront = 1.f;
	float tyreRadiusScaleRear = 1.f;
	rmcDrawable* customDrawableFront = NULL;
	rmcDrawable* customDrawableRear = NULL;

	if (renderGfx)
	{
		customDrawableFront = renderGfx->GetDrawable(VMT_WHEELS);
		customDrawableRear = renderGfx->GetDrawable(VMT_WHEELS_REAR_OR_HYDRAULICS);

		tyreWidthScaleFront = renderGfx->GetWheelTyreWidthScale();
		tyreWidthScaleRear = renderGfx->GetRearWheelTyreWidthScale();
		tyreRadiusScaleFront = renderGfx->GetWheelTyreRadiusScale();
		tyreRadiusScaleRear = renderGfx->GetRearWheelTyreRadiusScale();

		CCustomShaderEffectVehicleType* wheelTypeFront = renderGfx->GetDrawableCSEType(VMT_WHEELS);
		CCustomShaderEffectVehicleType* wheelTypeRear = renderGfx->GetDrawableCSEType(VMT_WHEELS_REAR_OR_HYDRAULICS);

		// fallback to front wheel if no rear wheel is set (rear wheels are only set for bikes, so make sure not to overwrite those)
		if (!customDrawableRear && customDrawableFront && !canHaveRearWheel)
		{
			customDrawableRear = customDrawableFront;
			
			if(pModelInfo->GetModelNameHash()==MID_INFERNUS2)
			{
				// do nothing: infernus2 has custom scale on rear wheel
			}
			else
			{
				tyreWidthScaleRear = tyreWidthScaleFront;
				tyreRadiusScaleRear = tyreRadiusScaleFront;
			}
		}

		CCustomShaderEffectVehicle* pCSEVehicle = (CCustomShaderEffectVehicle*)CCustomShaderEffectDC::GetLatestShader();
		Assert(pCSEVehicle);

		// set shader vars for wheel drawables
		static dev_bool bEnableCSE3 = true;
		if(bEnableCSE3 && customDrawableFront && wheelTypeFront)
		{
			// replace main vehicle Type with mod Type:
			CCustomShaderEffectVehicleType *oldType = wheelTypeFront->SwapType(pCSEVehicle);
			Assert(oldType);
			pCSEVehicle->SetShaderVariables(customDrawableFront);
			// swap to old type!
			oldType->SwapType(pCSEVehicle);
		}
		if(bEnableCSE3 && customDrawableRear && wheelTypeRear)
		{
			// replace main vehicle Type with mod Type:
			CCustomShaderEffectVehicleType *oldType = wheelTypeRear->SwapType(pCSEVehicle);
			Assert(oldType);
			pCSEVehicle->SetShaderVariables(customDrawableRear);
			// swap to old type!
			oldType->SwapType(pCSEVehicle);
		}
	}

	// From B*737358, it would appear that one of these pointers can be NULL.
	// This may or may not be valid.  If one of the asserts below fire,
	// hopefully we can get a better idea if these NULL pointer checks are
	// really required, or are just hiding another issue.
	fragPhysicsLOD *const physicsLod = pFragType->GetPhysics(0);
	Assertf(physicsLod, "please reopen B*737358 and add this assert as a comment");
	if (Unlikely(!physicsLod))
	{
		return;
	}
	fragTypeChild **const children = physicsLod->GetAllChildren();
	Assertf(children, "please reopen B*737358 and add this assert as a comment");
	if (Unlikely(!children))
	{
		return;
	}

	for(int nWheel=0; nWheel<NUM_VEH_CWHEELS_MAX; nWheel++)
	{
		int nThisChild = pModelInfo->GetStructure()->m_nWheelInstances[nWheel][0];
		int nDrawChild = pModelInfo->GetStructure()->m_nWheelInstances[nWheel][1];

		if(nThisChild > -1 && !(destroyedBits &((u64)1<<nThisChild))
		&& nDrawChild > -1)
		{
#if USE_INSTANCED_WHEEL_RENDERER
			// if we're starting to draw a new child model, need to render all the previous ones (e.g. starting to draw rear wheels after finished all front ones)
			if(nDrawChild != nLastDrawChild && nLastDrawChild > -1)
				CWheelInstanceRenderer::RenderAllWheels();

			nLastDrawChild = nDrawChild;
#endif

			fragTypeChild *curChild = children[nThisChild];
			const rmcDrawable* toDraw = children[nDrawChild]->GetUndamagedEntity();

			bool isFrontWheel = (pModelInfo->GetStructure()->m_isRearWheel & (1 << nWheel)) == 0;
			float tyreWidthScale = 1.f;
			float tyreRadiusScale = 1.f;

			// select correct drawable to render
			if (renderGfx)
			{
	            if (isFrontWheel && customDrawableFront)
				{
	                toDraw = customDrawableFront;
					tyreWidthScale = tyreWidthScaleFront;
					tyreRadiusScale = tyreRadiusScaleFront;
				}

				if (!isFrontWheel && customDrawableRear)
				{
					toDraw = customDrawableRear;
					tyreWidthScale = tyreWidthScaleRear;
					tyreRadiusScale = tyreRadiusScaleRear;
				}
			}

			if(toDraw)
			{
				const rmcLodGroup& lodGroup = toDraw->GetLodGroup();
				if(lodGroup.GetCullRadius())
				{
					if(!lodGroup.ContainsLod(nLodIndex))
					{	// force highest LOD if selected LOD not available:
						//nLodIndex = 0;
						continue;
					}

#if ENABLE_FRAG_OPTIMIZATION
					Matrix34 childMatrix(Matrix34::IdentityType);
					int childBoneIndex = pFragType->GetBoneIndexFromID(curChild->GetBoneID());

					const bool bInstanced = pModelInfo->GetStructure()->m_nWheelInstances[nWheel][0] > -1;
					float fRightWheelOrienationScale = 1.0f;

					if(ms)
					{
						if (pModelInfo->GetStructure()->m_numSkinnedWheels <= NUM_VEH_CWHEELS_MAX)
						{
							s32 mtxIndex = ms->GetMatrixCount() - pModelInfo->GetStructure()->m_numSkinnedWheels + nWheel;
							ms->ComputeMatrix(childMatrix, mtxIndex);
						}
						else
						{
							ms->ComputeMatrix(childMatrix, childBoneIndex);
						}
					}
					else if(bInstanced && (nWheel & 1))
					{
						// Flip instanced right wheels
						fRightWheelOrienationScale = -1.0f;
					}
#else
					Matrix34 childMatrix;
					int childBoneIndex = pFragType->GetBoneIndexFromID(curChild->GetBoneID());
					if (pModelInfo->GetStructure()->m_numSkinnedWheels <= NUM_VEH_CWHEELS_MAX)
					{
						s32 mtxIndex = ms->GetMatrixCount() - pModelInfo->GetStructure()->m_numSkinnedWheels + nWheel;
						ms->ComputeMatrix(childMatrix, mtxIndex);
					}
					else
					{
						ms->ComputeMatrix(childMatrix, childBoneIndex);
					}
#endif
					//childMatrix.Dot(matrix);

					// need to undo AttachModelRelative
					Matrix34 boneMatrix;
					pFragType->GetCommonDrawable()->GetSkeletonData()->GetBoneData(childBoneIndex)->CalcCumulativeJointScaleOrients(RC_MAT34V(boneMatrix));
					Vector3 vecBoneOffset = boneMatrix.d;
					childMatrix.Transform3x3(vecBoneOffset);
					childMatrix.d.Add(vecBoneOffset);
					childMatrix.Dot(rootMatrix);

					childMatrix.a.Scale(tyreWidthScale ENABLE_FRAG_OPTIMIZATION_ONLY(* fRightWheelOrienationScale));
					childMatrix.b.Scale(tyreRadiusScale);
					childMatrix.c.Scale(tyreRadiusScale ENABLE_FRAG_OPTIMIZATION_ONLY(* fRightWheelOrienationScale));

                    //scale wheels if need be
                    if(!pModelInfo->GetStructure()->m_bWheelInstanceSeparateRear)
                    {
                        if(pModelInfo->GetStructure()->m_nWheelInstances[0][0] > -1 && 
                            pModelInfo->GetStructure()->m_nWheelInstances[nWheel][0] > -1 )
                        {
                            childMatrix.a.Scale(pModelInfo->GetStructure()->m_fWheelTyreWidth[nWheel] / pModelInfo->GetStructure()->m_fWheelTyreWidth[0]);
                            float fRadiusScale = pModelInfo->GetStructure()->m_fWheelTyreRadius[nWheel] / pModelInfo->GetStructure()->m_fWheelTyreRadius[0];
                            childMatrix.b.Scale(fRadiusScale);
                            childMatrix.c.Scale(fRadiusScale);
                        }
                    }

					if(lodGroup.ContainsLod(nLodIndex))
					{
						const rmcLod &lod = lodGroup.GetLod(nLodIndex);

						const int numLods = lod.GetCount();
						Assertf(numLods==1, "Incorrectly exported wheel - contains more than one model per lod!");	// only one model allowed per lod for wheels
						for (int nLod=0; nLod<numLods; nLod++)
						{
							if (lod.GetModel(nLod))
							{
#if USE_INSTANCED_WHEEL_RENDERER
								Vector4 tyreDeformParams(VECTOR4_ORIGIN);
								Vector4 tyreDeformParams2(VEC4_ONEW);

								if(wheelBurstRatios && m_wheelBurstRatios[nWheel][0] > 0)
								{
									float fInnerRadius = pModelInfo->GetStructure()->m_fWheelRimRadius[nWheel];
									float fOuterRadius = pModelInfo->GetStructure()->m_fWheelTyreRadius[nWheel];
									float fTyreDepth = fOuterRadius - fInnerRadius;
									float fTyreScale = pModelInfo->GetStructure()->m_fWheelScaleInv[nWheel] * (1.0f/tyreRadiusScale);

									float fRenderFlag = m_wheelBurstRatios[nWheel][0] >= 254 ? 0.0f : 1.0f;
									float fDeformSide = -(m_wheelBurstRatios[nWheel][1] * inv255 - 0.5f) * sfTyreSideMult * fTyreDepth; // effectively 0.5 * tyre depth because of the way side is stored
									float fDeformBurst = m_wheelBurstRatios[nWheel][0] * inv255;

									float fGroundPosZ = 1.0f - (m_wheelBurstRatios[nWheel][0] * inv255);

									fGroundPosZ = childMatrix.d.z - fInnerRadius - fGroundPosZ*fTyreDepth;

									if(sbUseBurstDepth)
									{
										float fDeformXYInv = 1.0f / (fDeformBurst * fTyreDepth * fTyreScale);

										float fDeformY = childMatrix.b.z * fTyreScale*fTyreScale;
										float fDeformZ = childMatrix.c.z * fTyreScale*fTyreScale;

										tyreDeformParams.Set(fDeformBurst*tyreDeformScaleX, fDeformY, fDeformZ, fDeformSide*fTyreScale);
										tyreDeformParams2.Set(fInnerRadius*fTyreScale, fGroundPosZ, fDeformXYInv, fRenderFlag);
									}
									else
									{
										tyreDeformParams.Set(tyreDeformScaleX, tyreDeformScaleYZ, fDeformSide, fRenderFlag);
										tyreDeformParams2.Set(fInnerRadius, fGroundPosZ, 0.0f, 0.0f);
									}
								}

								static bool sbForceAndrezTestValues = false;
								if(sbForceAndrezTestValues && nWheel==0)
								{
									tyreDeformParams.Set(tyreDeformScaleX, tyreDeformScaleYZ, tyreDeformContactPointX, 1.0f);
									tyreDeformParams2.Set(0.262f, childMatrix.d.z - 0.29f, 0.0f, 0.0f);
								}
							#if CSE_VEHICLE_EDITABLEVALUES
								// editable inner wheel radius stuff (__BANK only):
								if(CCustomShaderEffectVehicle::GetEWREnabled())
								{
									tyreDeformParams.w = CCustomShaderEffectVehicle::GetEWRTyreEnabled()? 1.0f : 0.0f;
									tyreDeformParams2.x= CCustomShaderEffectVehicle::GetEWRInnerRadius();
								}
							#endif //CSE_VEHICLE_EDITABLEVALUES...

								CWheelInstanceRenderer::AddWheelModel(childMatrix, lod.GetModel(nLod), &toDraw->GetShaderGroup(), bucket,
											tyreDeformParams, tyreDeformParams2);
#else //USE_INSTANCED_WHEEL_RENDERER

								grmModel &model = *lod.GetModel(nLod);
								grcViewport::SetCurrentWorldMtx(childMatrix);
								model.Draw(toDraw->GetShaderGroup(), bucket, nLodIndex);

#endif //USE_INSTANCED_WHEEL_RENDERER...
							}
						}
					}
				}
			}
		}
	}

#if USE_INSTANCED_WHEEL_RENDERER
	CWheelInstanceRenderer::RenderAllWheels();
#endif

}

// - frag object (no skeleton) -
CDrawFragTypeDC::CDrawFragTypeDC(CEntity* pEntity)
: m_DebugInfo(pEntity)
{
    // add shared data
    {
        int sharedMemType = DL_MEMTYPE_ENTITY;
        dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

        m_dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

        if(m_dataAddress.IsNULL() || !g_cache_entities)
        {
            u32 dataSize = (u32)sizeof(CEntityDrawDataFragType);
            if(g_cache_entities)
            {
                gDCBuffer->AllocateSharedData(DL_MEMTYPE_ENTITY, sharedDataInfo, dataSize, m_dataAddress);
            }
			
#if ENABLE_FRAG_OPTIMIZATION
			void * dataBlock = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
			CEntityDrawDataFragType* data = new (dataBlock) CEntityDrawDataFragType();
			data->Init(pEntity);
#else
			void * data = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
			reinterpret_cast<CEntityDrawDataFragType*>(data)->Init(pEntity);
#endif	

			gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
        }
#if RAGE_INSTANCED_TECH
		else
		{
			if (pEntity->GetViewportInstancedRenderBit() != 0)
			{
				u32 dataSize = (u32) sizeof(CEntityDrawDataFragType);
				void *data = gDCBuffer->GetDataBlock(dataSize,m_dataAddress);
				reinterpret_cast<CEntityDrawDataFragType*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
			}
		}
#endif
    }
}

RAGETRACE_DECL(DrawFragType);

void CDrawFragTypeDC::Execute()
{
	RAGETRACE_SCOPE(DrawFragType);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	CEntityDrawDataFragType & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

#if __DEV
const char *CDrawFragTypeDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawFragTypeDC *pDLC = (CDrawFragTypeDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV

#if ENABLE_FRAG_OPTIMIZATION
// - frag object (no skeleton) -
CDrawFragTypeVehicleDC::CDrawFragTypeVehicleDC(CEntity* pEntity)
	: m_DebugInfo(pEntity)
{
	// add shared data
	{
		int sharedMemType = DL_MEMTYPE_ENTITY;
		dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

		m_dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

		if(m_dataAddress.IsNULL() || !g_cache_entities)
		{
			u32 dataSize = (u32)sizeof(CEntityDrawDataFragTypeVehicle);
			void * dataBlock = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
			if(g_cache_entities)
			{
				gDCBuffer->AllocateSharedData(DL_MEMTYPE_ENTITY, sharedDataInfo, dataSize, m_dataAddress);
			}

			CEntityDrawDataFragTypeVehicle* data = new (dataBlock) CEntityDrawDataFragTypeVehicle();

			data->Init(pEntity);

			gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
		}
#if RAGE_INSTANCED_TECH
		else
		{
			if (pEntity->GetViewportInstancedRenderBit() != 0)
			{
				u32 dataSize = (u32) sizeof(CEntityDrawDataFragTypeVehicle);
				void *dataBlock = gDCBuffer->GetDataBlock(dataSize,m_dataAddress);
				reinterpret_cast<CEntityDrawDataFragTypeVehicle*>(dataBlock)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
			}
		}
#endif
	}
}

RAGETRACE_DECL(DrawFragTypeVehicle);

void CDrawFragTypeVehicleDC::Execute()
{
	RAGETRACE_SCOPE(DrawFragTypeVehicle);
	Assert(!m_dataAddress.IsNULL());
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	CEntityDrawDataFragType & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

#if __DEV
const char *CDrawFragTypeVehicleDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawFragTypeVehicleDC *pDLC = (CDrawFragTypeVehicleDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV
#endif // ENABLE_FRAG_OPTIMIZATION

// fill out the bit fields correctly to render children of frag
// TODO: Move this into a class.
namespace rage 
{
void PopulateBitFields(fragInst* pFrag, u64& damagedBits, u64& destroyedBits)
{
	Assert(pFrag->GetType());
	Assert(pFrag->GetTypePhysics()->GetNumChildren() < MAX_FRAG_CHILDREN);

	fragHierarchyInst* pInstanceInfo = NULL;
	if (pFrag->GetCached())
	{
		fragCacheEntry* entry = pFrag->GetCacheEntry();
		Assert(entry);
		pInstanceInfo = entry->GetHierInst();
	}

	damagedBits = 0;
	destroyedBits = 0;

	// The loop below is very expensive and incurs many cache misses, especially for vehicles (which 
	// have many frag pieces). Early out optimization checks the anyGroupDamaged bool and groupBroken 
	// bitset - if nothing is damaged or broken, just return.
	Assert(!pInstanceInfo || (pInstanceInfo->groupBroken != NULL));
	if(pInstanceInfo && !pInstanceInfo->anyGroupDamaged && !pInstanceInfo->groupBroken->AreAnySet())
	{
		return;
	}

	// make a copy of the child damage array
	bool	bDamaged;
	bool	bDestroyed;

	u8 numChildren = pFrag->GetTypePhysics()->GetNumChildren();
	for (u64 child = 0; child < numChildren; ++child)
	{
		const fragTypeChild *curChild = pFrag->GetTypePhysics()->GetAllChildren()[child];
		int groupIndex = curChild->GetOwnerGroupPointerIndex();
		if (!pInstanceInfo || pInstanceInfo->groupBroken->IsClear(groupIndex))
		{
			bDestroyed = false;
			bDamaged = false;

			if (pInstanceInfo &&	pInstanceInfo->groupInsts[groupIndex].IsDamaged())
			{
				bDamaged = true;
			}
			else if(pInstanceInfo==NULL && pFrag->GetDamageHealth() < 0.0f)
			{
				bDamaged = true;
			}

		}
		else 
		{
			bDestroyed = true;
			bDamaged = false;
		}

		bDamaged = true;

		if (bDestroyed)
		{
			destroyedBits = destroyedBits | (((u64)1)<<child);
		}

		if (bDamaged)
		{
			damagedBits = damagedBits | (((u64)1)<<child);
		}
	}
}
} // namespace rage

// draw a frag with no skeleton
void FragDraw(int currentLOD, CBaseModelInfo* pModelInfo, const grmMatrixSet *ms, const Matrix34& matrix, u64 damagedBits, u64 destroyedBits, float& dist, u8 flags, float lodMult, bool damaged, const fragType* pFragType)
{
	const fragType* type = pFragType;
#if __BANK 
	u16 drawableStats = DRAWLISTMGR->GetDrawableStatContext();
#else
	u16 drawableStats = 0;
#endif

	if (flags&CDrawFragDC::HD_REQUIRED && type == pModelInfo->GetFragType())
	{
		if (pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
		{
			CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pModelInfo);
			if (pVMI->GetAreHDFilesLoaded())
			{
				bool bAllowHD = true;
				gDrawListMgr->AdjustVehicleHD(true, bAllowHD);
				if (bAllowHD)
				{
					type = pVMI->GetHDFragType();
#if __BANK
					gIsDrawingHDVehicle = true;
#endif
				}
			}
		} 
	}

#if __BANK
	gIsDrawingVehicle = CVehicleFactory::ms_bShowVehicleLODLevel;
	dist = dist / CVehicleFactory::ms_vehicleLODScaling;
#endif //__BANK

	if (type)
	{
		grmMatrixSet * sharedSet = NULL;

		sharedSet = type->GetSharedMatrixSet();

		if(1)
		{
			fragDrawable* toDraw = type->GetCommonDrawable();

			Assert(toDraw != NULL);

			if (type->GetSkinned())
			{
                dist = matrix.d.Dist(gDrawListMgr->GetCalcPosDrawableLOD());

#if __BANK
                if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
                {
                    dist = dist / CVehicleFactory::ms_vehicleLODScaling;
                }
#endif //__BANK

                // force high lod for fragments with broken components (not vehicles - they are built to handle it), and for local player
                const bool forceHiLOD = (flags&(CDrawFragDC::IS_LOCAL_PLAYER | CDrawFragDC::FORCE_HI_LOD)) || ((damagedBits != 0 || destroyedBits != 0) && (pModelInfo->GetModelType()!=MI_TYPE_VEHICLE));

                if (damaged && type->GetDamagedDrawable())
				{
                    g_pBreakable->AddToDrawBucket(*type->GetDamagedDrawable(), matrix, ms, sharedSet, dist, drawableStats, lodMult, forceHiLOD);
				}
                else
				{
                    g_pBreakable->AddToDrawBucket(*toDraw, matrix, ms, sharedSet, dist, drawableStats, lodMult, forceHiLOD);
				}
			} 
			else 
			{
                //complex draw...
                dist = matrix.d.Dist(gDrawListMgr->GetCalcPosDrawableLOD());

#if __BANK
                if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
                {
                    dist = dist / CVehicleFactory::ms_vehicleLODScaling;
                }
#endif //__BANK

                if (fragTune::IsInstantiated())
                {
                    dist += FRAGTUNE->GetLodDistanceBias();
                }

				//in this case, we have a list of entities to draw...
				u8 numChildren = type->GetPhysics(currentLOD)->GetNumChildren();

				for (u64 child = 0; child < numChildren; ++child)
				{
					fragTypeChild* childType = type->GetPhysics(currentLOD)->GetAllChildren()[child];

					toDraw = NULL;
					bool destroyed = ((destroyedBits & ((u64)1<<child)) != 0);

					if (!destroyed) 
					{
						bool damaged = ((damagedBits & ((u64)1<<child)) != 0);
						damaged &= (childType->GetDamagedEntity() != NULL);
						toDraw = damaged ? childType->GetDamagedEntity() : childType->GetUndamagedEntity();
					}

					if (toDraw)
					{
						const rmcLodGroup& lod = toDraw->GetLodGroup();

						if( !fragTune::IsInstantiated() || dist < FRAGTUNE->GetGlobalMaxDrawingDistance() + lod.GetCullRadius())
						{
							// force high lod for fragments with broken components
							const bool forceHiLOD = (destroyedBits != 0) || ((flags&CDrawFragDC::IS_LOCAL_PLAYER) == CDrawFragDC::IS_LOCAL_PLAYER);
							g_pBreakable->AddToDrawBucket(*toDraw, matrix, ms, sharedSet, dist, drawableStats, lodMult, forceHiLOD);
#if __PFDRAW
							toDraw->ProfileDraw(NULL, &matrix, lod.ComputeLod(dist));
#endif
						}
					}
				}
			}
		}

#if __PPU && USE_EDGE
		if(bIsEdgeVehicle)
		{
			pEdgeVehicleDamageTex = NULL;
			#if SPU_GCM_FIFO
				SPU_COMMAND(GTA4__SetDamageTexture,0x00);
				cmd->damageTexture	= NULL;
				cmd->boundRadius	= 0.0f;
			#endif //SPU_GCM_FIFO...
		}
#endif //__PPU && USE_EDGE...

#if __BANK
		gIsDrawingHDVehicle = false;
		gIsDrawingVehicle = false;
#endif

	} // if(type)...

}// end of FragDraw()...


CDrawSkinnedEntityDC::CDrawSkinnedEntityDC(CEntity* pEntity)
: m_AddSkeleton(static_cast<CDynamicEntity*>(pEntity))
, m_DebugInfo(pEntity)
{
    // add shared data
    {
        int sharedMemType = DL_MEMTYPE_ENTITY;
        dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

        m_dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

        if(m_dataAddress.IsNULL() || !g_cache_entities)
        {
            u32 dataSize = (u32) sizeof(CEntityDrawDataSkinned);
            void * data = gDCBuffer->AddDataBlock(NULL, dataSize, m_dataAddress);
            if(g_cache_entities)
            {
                gDCBuffer->AllocateSharedData(DL_MEMTYPE_ENTITY, sharedDataInfo, dataSize, m_dataAddress);
            }

            reinterpret_cast<CEntityDrawDataSkinned*>(data)->Init(pEntity);

			gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
        }
#if RAGE_INSTANCED_TECH
		else
		{
			if (pEntity->GetViewportInstancedRenderBit() != 0)
			{
				u32 dataSize = (u32) sizeof(CEntityDrawDataSkinned);
				void *data = gDCBuffer->GetDataBlock(dataSize,m_dataAddress);
				reinterpret_cast<CEntityDrawDataSkinned*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
			}
		}
#endif
    }
}

RAGETRACE_DECL(DrawSkinnedEntity);

void CDrawSkinnedEntityDC::Execute()
{
	RAGETRACE_SCOPE(DrawSkinnedEntity);
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	m_AddSkeleton.Execute();
	Assert(!m_dataAddress.IsNULL());
	CEntityDrawDataSkinned & data = GetDrawData();
	RAGETRACE_CALL(data.Draw());
}

#if __DEV
const char *CDrawSkinnedEntityDC::GetExtraDebugInfo(dlCmdBase & base, char * buffer, size_t bufferSize, Color32 &/*color*/)
{
	CDrawSkinnedEntityDC *pDLC = (CDrawSkinnedEntityDC *) &base;
	return pDLC->m_DebugInfo.GetExtraDebugInfo(buffer, bufferSize);
}
#endif // __DEV


