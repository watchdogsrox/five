//
// entity/entitybatch.cpp : base class for batched entity lists
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include <algorithm>
#include <numeric>
//#include <limits>
//#include <type_traits>

//Rage Includes
#include "diag/art_channel.h"
#include "grcore/instancebuffer.h"
#include "grcore/device.h"
#include "grcore/wrapper_d3d.h"
#include "grcore/wrapper_gnm.h"
#include "grcore/effect.h"
#include "grmodel/modelfactory.h"
#include "fwmaths/vectorutil.h"
#include "fwrenderer/instancing.h"	//Really just for constant buffers
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/mapdata/mapinstancedata.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "fwdebug/picker.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "grcore/debugdraw.h"
#include "math/amath.h"
#include "system/stl_wrapper.h"

#if RSG_ORBIS
#include "grcore/gfxcontext_gnm.h"
#endif

#include "EntityBatch.h"

//Game Includes
#include "camera/CamInterface.h"
#include "physics/WorldProbe/shapetestprobedesc.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Entities/EntityBatchDrawHandler.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h" // for WATER_REFLECTION_PRE_REFLECTED define ..
#include "renderer/render_channel.h"
#include "scene/lod/LodDrawable.h"
#include "scene/lod/LodScale.h"
#include "scene/world/VisibilityMasks.h"
#include "shaders/ShaderLib.h"
#include "Shaders/CustomShaderEffectGrass.h"
#include "timecycle/TimeCycle.h"

//Debug Only
#include "task/System/AsyncProbeHelper.h"
#include "scene/portals/FrustumDebug.h"

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
#include "fwscene/stores/maptypesstore.h"
#endif	//KEEP_INSTANCELIST_ASSETS_RESIDENT

#if __D3D11 && __WIN32PC
#include <d3d11.h>
#endif //__D3D11 && __WIN32PC

#if NV_SUPPORT
#include "../../3rdParty/NVidia/nvapi.h"
#endif 

SCENE_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CEntityBatch, CONFIGURED_FROM_FILE, atHashString("EntityBatch",0x5817ce07));
FW_INSTANTIATE_CLASS_POOL(CGrassBatch, CONFIGURED_FROM_FILE, atHashString("GrassBatch",0xe0b50aa6));

#if GRASS_BATCH_CS_CULLING
PARAM(disableGrassComputeShader, "Disable running the compute shader on the grass (renders all visible grass batches with no fading)");
PARAM(grassDisableRenderGeometry, "Disables the rendering of the grass geometry only (still runs the CS computation)");
#endif // GRASS_BATCH_CS_CULLING

namespace EBStatic
{
	CSettings::eSettingsLevel sQuality = CSettings::High;

#if RSG_DURANGO || RSG_PC
	u32		sPerfSkipInstance = RSG_DURANGO ? 0x01 : 0x0f;	// Default to 'all batches' for PC.
	bool	bPerfForceSkipInstance = false;
#endif // RSG_DURANGO || RSG_PC...

	static BankBool sDebugDrawEnabled = false;
	static BankBool sDebugDrawBatchAABB = true;
	static BankBool sDebugDrawInstances = false;
	static BankBool sDebugDrawLinkedBounds = false;

	static BankBool sDebugDrawLinkedRenderedInfo = true;
	static BankBool sDebugDrawLinkedNotRenderedInfo = false;

	static BankBool sComputeAoScalePerInstance = true;
	static BankBool sOverrideLodDist = false;
	static BankUInt32 sOverriddenLodDistValue = 0x1fff;
	static BankBool sEnableLinkageVisibility = true;
	static BankBool sResetLinkageVisibility = true;
	static BankFloat sShadowLodFactor = 0.5f;

	BankBool sPerfOnlyDraw1stInstanceGBuf = false;
	BankBool sPerfOnlyDraw1stInstanceShadows = false;
	GRASS_BATCH_CS_CULLING_ONLY(BankBool sGrassRenderGeometry = true);
	GRASS_BATCH_CS_CULLING_ONLY(BankBool sPerfEnableComputeShader = true);

#if __BANK
	static BankBool sOutputStaticInstanceBuffer = true;
	static BankBool sOutputStaticIBData = false;
	static BankBool sProbeForNormal = true;									//Probes for actual terrain normal when reporting grass info

	static BankBool sDebugDrawBatchAABBSolid = false;
	static Color32 sDebugDrawBatchAABBColor(0.0f, 0.0f, 1.0f);				//Blue

	static BankBool sDebugDrawInstanceAABBSolid = false;
	static Color32 sDebugDrawInstanceAABBColor(0.0f, 1.0f, 0.0f);			//Green
	static Color32 sDebugDrawRenderedHdAABBColor(0.0f, 1.0f, 1.0f);
	static Color32 sDebugDrawNotRenderedHdAABBColor(1.0f, 0.0f, 1.0f);

	static BankBool sDebugDrawEntityTransformAABB = false;
	static BankBool sDebugDrawEntityTransformAABBSolid = false;
	static Color32 sDebugDrawEntityTransformAABBColor(1.0f, 0.0f, 0.0f);	//Red

	//Debug Draw - Grass Terrain Normal
	BankBool sDebugDrawGrassTerrainNormalEnabled = true;
	BankFloat sDebugDrawGrassTerrainNormalLength = 1.1f;
	BankFloat sDebugDrawGrassTerrainNormalConeLength = 0.3f;
	BankFloat sDebugDrawGrassTerrainNormalConeRadius = 0.07f;
	BankBool sDebugDrawGrassTerrainNormalConeCap = true;
	Color32 sDebugDrawGrassTerrainNormalColor(0.0f, 0.0f, 1.0f);		//Blue

	static Color32 sDebugDrawSelectedTextColor(1.0f, 1.0f, 1.0f);			//White
	static Color32 sDebugDrawSelectedBatchAABBColor(1.0f, 1.0f, 0.0f);		//Yellow

	static Color32 sDebugDrawMapAABBLoadedColor(0.5f, 1.0f, 0.5f);			//Green-ish
	static Color32 sDebugDrawMapAABBUnloadedColor(1.0f, 0.5f, 0.5f);		//Red-ish

	bkBank *sBank = NULL;
	bkGroup *sSelectionGroup = NULL;
	bkGroup *sSelectedEntityGroup = NULL;
	bkCombo *sMapNameComboWidget = NULL;

	rage::atArray<s32> sMapDataDefIndex;
	BankInt32 sSelectedMapData = -1;
	BankInt32 sSelectedEntityBatch = -1;
	BankInt32 sSelectedEntityBatchInst = -1;

	//Performance & Usage Stats
	BankBool sPerfDisplayUsageInfo = false;
	BankBool sPerfDisplayRuntimeInfo = true;
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(BankBool sPerfDisplayAllocatorInfo = true);
	BankBool sPerfDisplayPropUsageInfo = true;
	BankBool sPerfDisplayGrassUsageInfo = true;

	//Counters
	u32 sPerfBatchesRenderedGbuf = 0;
	u32 sPerfInstancesRenderedGbuf = 0;
	u32 sPerfInstancesRenderedGbuf_LODFade = 0;

	u32 sPerfBatchesRenderedShadow = 0;
	u32 sPerfInstancesRenderedShadow = 0;
	u32 sPerfInstancesRenderedShadow_LODFade = 0;

	u32 sPerfBatchesRenderedOther = 0;
	u32 sPerfInstancesRenderedOther = 0;
	u32 sPerfInstancesRenderedOther_LODFade = 0;

#if GRASS_BATCH_CS_CULLING
	BankBool sPerfDisplayGrassCSInfo = false && !__FINAL;	//Preliminary... and potentially very slow!
# if DEVICE_GPU_WAIT
	BankBool sPerfDisplayGrassCS_UseGPUFence = true;
# endif
	u32 sPerfCSInstancesRenderedGbuf = 0;
	u32 sPerfCSZeroCountBatchesGbuf = 0;
	u32 sPerfCSInstancesRenderedShadow = 0;
	u32 sPerfCSZeroCountBatchesShadow = 0;
	u32 sPerfCSInstancesRenderedOther = 0;
	u32 sPerfCSZeroCountBatchesOther = 0;

	BankBool sGrassComputeShader_FreezeVp = false;
	extern BankBool sGrassComputeShader_AllowBatching;
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(extern BankBool sGrassComputeShader_AllowCopyStructureCountBatching);
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(extern BankBool sGrassComputeShader_AvoidInterleaving);
	extern BankBool sFlushCSAfterEachDispatch;
	extern BankBool sCSDisableCrossfade;
	ORBIS_ONLY(extern BankBool sGrassComputeShader_DebugDrawIndirect);
	extern BankBool sGrassComputeShader_IgnoreLodScale_DrawableLOD;
	extern BankBool sGrassComputeShader_IgnoreLodScale_LODFade;
	extern BankBool sGrassComputeShader_UAV_Sync;
	grcViewport sFrozenVp;
	CFrustumDebug sFrustumDbg;
#endif //GRASS_BATCH_CS_CULLING

	void InitMapDataDefIndexMap()
	{
		s32 storeSize = g_MapDataStore.GetSize();
		for(int i = 0; i < storeSize; ++i)
		{
			fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(i));
			if(pDef && (pDef->GetContentFlags() & fwMapData::CONTENTFLAG_INSTANCE_LIST) > 0)
			{
				sMapDataDefIndex.PushAndGrow(i);
			}
		}
	}

	fwEntity *GetSelectedEntity()
	{
		fwEntity *entity = NULL;
		if(sSelectedMapData >= 0 && sSelectedEntityBatch >= 0)
		{
			fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
			if(pDef && pDef->IsLoaded() && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
			{
				fwMapDataContents *contents = pDef->GetContents();
				fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
				u32 numEntities = contents->GetNumEntities();
				u32 instEntityOffset = numEntities - (instList.m_PropInstanceList.size() + instList.m_GrassInstanceList.size());
				u32 ebIndex = instEntityOffset + sSelectedEntityBatch;
				if(ebIndex < numEntities)
					entity = contents->GetEntities()[ebIndex];
			}
		}

		return entity;
	}

	int FindEBIndexInMap(fwEntity *entity, s32 mapDataDefIndex)
	{
		if(mapDataDefIndex < 0)
			return -1;

		fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(mapDataDefIndex));
		if(pDef && pDef->IsLoaded() && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
		{
			fwMapDataContents *contents = pDef->GetContents();
			u32 numEntities = contents->GetNumEntities();
			fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
			u32 instEntityOffset = numEntities - (instList.m_PropInstanceList.size() + instList.m_GrassInstanceList.size());

			for(int i = instEntityOffset; i < numEntities; ++i)
			{
				if(entity == contents->GetEntities()[i])
					return i - instEntityOffset;
			}
		}

		return -1;
	}

	void OnSelectedEntityLodDistanceChanged()
	{
		if(CEntity *entity = static_cast<CEntity *>(GetSelectedEntity()))
		{
			if(entity->GetIsTypeInstanceList())
			{
				CEntityBatch *eb = static_cast<CEntityBatch *>(entity);
				if(const CEntityBatch::InstanceList *list = eb->GetInstanceList())
				{
					eb->SetLodDistance(list->m_lodDist);
				}
			}
			else if(entity->GetIsTypeGrassInstanceList())
			{
				CGrassBatch *eb = static_cast<CGrassBatch *>(entity);
				if(const CGrassBatch::InstanceList *list = eb->GetInstanceList())
				{
#if GRASS_BATCH_CS_CULLING
					//Setup LOD distance - Must be large enough to cover LOD fade for farthest element.
					ScalarV minLodDist = Mag(list->m_BatchAABB.GetExtent());
					float lodDist = minLodDist.Getf() + list->m_lodDist;
#endif //#if GRASS_BATCH_CS_CULLING
					eb->SetLodDistance(GRASS_BATCH_CS_CULLING_SWITCH(static_cast<u32>(lodDist), list->m_lodDist));
				}
			}
		}
	}

	void SetSelectedEntityInPicker()
	{
		if(CEntity *entity = static_cast<CEntity *>(GetSelectedEntity()))
		{
			g_PickerManager.ResetList(true);
			g_PickerManager.AddEntityToList(entity, false, false);
			g_PickerManager.SetIndexOfSelectedEntity(0);
		}
	}

	void OnBatchValuesChanged()
	{
		if(CEntity *entity = static_cast<CEntity *>(GetSelectedEntity()))
		{
			if(entity->GetIsTypeGrassInstanceList())
			{
				CGrassBatch *eb = static_cast<CGrassBatch *>(entity);
				if(const CGrassBatch::InstanceList *list = eb->GetInstanceList())
				{
					if(fwDrawData *dh = eb->GetDrawHandlerPtr())
					{
						if(CCustomShaderEffectGrass *cse = static_cast<CCustomShaderEffectGrass *>(dh->GetShaderEffect()))
						{
							cse->SetScaleRange(list->m_ScaleRange);
							cse->SetOrientToTerrain(list->m_OrientToTerrain);
						}
					}
				}
			}
		}
	}

	void UpdateSelectedEntityBatchWidgets(bkBank &bank)
	{
		if(CEntity *entity = static_cast<CEntity *>(GetSelectedEntity()))
		{
			if(entity->GetIsTypeInstanceList())
			{
				CEntityBatch *eb = static_cast<CEntityBatch *>(entity);
				if(const CEntityBatch::InstanceList *list = eb->GetInstanceList())
				{
					bank.AddText("Archetype Name", const_cast<atHashString *>(&(list->m_archetypeName)), true);
					bank.AddSlider("LOD Distance", const_cast<u32 *>(&(list->m_lodDist)), 0u, 0x1FFFu, 1u, OnSelectedEntityLodDistanceChanged);
				
					bank.AddSeparator("");
					sSelectedEntityBatchInst = -1;
					bank.AddSlider("Instance Index", &sSelectedEntityBatchInst, -1, list->m_InstanceList.size() - 1, 1);
				}
			}
			else if(entity->GetIsTypeGrassInstanceList())
			{
				CGrassBatch *eb = static_cast<CGrassBatch *>(entity);
				if(const CGrassBatch::InstanceList *list = eb->GetInstanceList())
				{
					bank.AddText("Archetype Name", const_cast<atHashString *>(&(list->m_archetypeName)), true);
					bank.AddSlider("LOD Distance", const_cast<u32 *>(&(list->m_lodDist)), 0u, 0x1FFFu, 1u, OnSelectedEntityLodDistanceChanged);
					bank.AddSeparator("");
					bank.AddVector("Scale Range (Min|Max|Rand Scale)", const_cast<Vec3V *>(&(list->m_ScaleRange)), 0.0f, FLT_MAX / 1000.0f, 0.1f, OnBatchValuesChanged);
					bank.AddSlider("Orient To Terrain", const_cast<float *>(&(list->m_OrientToTerrain)), 0.0f, 1.0f, 0.1f, OnBatchValuesChanged);
					bank.AddSeparator("");
					bank.AddSlider("LOD Fade Start Distance", const_cast<float *>(&(list->m_LodFadeStartDist)), 0.0f, static_cast<float>(0x1FFFu), 0.5f);
					bank.AddSlider("LOD Instance Fade Range", const_cast<float *>(&(list->m_LodInstFadeRange)), 0.0f, 1.0f, 0.01f);

					bank.AddSeparator("");
					sSelectedEntityBatchInst = -1;
					bank.AddSlider("Instance Index", &sSelectedEntityBatchInst, -1, list->m_InstanceList.size() - 1, 1);
				}
			}

			bank.AddButton("Set Selected Entity To Picker", SetSelectedEntityInPicker);
		}
	}

	void OnSelectedEntityBatchChanged();
	void OnMapNameComboChanged()
	{
		while(bkWidget *widget = sMapNameComboWidget->GetNext())
			widget->Destroy();

		//Reset bank context and re-add widgets
		if(sSelectedMapData >= 0)
		{
			sBank->SetCurrentGroup(*sSelectionGroup);
			{
				bkBank &bank = *sBank;
				if(fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(sMapDataDefIndex[sSelectedMapData])))
				{
					if(pDef->IsLoaded())
					{
						if(Verifyf(pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance(), "MapDataDef Contents/MapData is NULL even though it's supposedly streamed in?"))
						{
							fwMapDataContents *contents = pDef->GetContents();
							fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData; //contents->GetMapData()->m_instancedData;
							s32 maxSelectedBatchIndex = instList.m_PropInstanceList.size() + instList.m_GrassInstanceList.size() - 1;
							sSelectedEntityBatch = Clamp(sSelectedEntityBatch, -1, maxSelectedBatchIndex);	//Make sure sSelectedEntityBatch is valid for newly selected map data
							bank.AddSlider("Entity Batch", &sSelectedEntityBatch, -1, maxSelectedBatchIndex, 1, OnSelectedEntityBatchChanged); //datCallback(OnSelectedEntityBatchChanged));
							sSelectedEntityGroup = bank.PushGroup("Selected EntityBatch");
							UpdateSelectedEntityBatchWidgets(bank);
							bank.PopGroup();
						}
					}
					else
					{
						bank.AddTitle("");
						char buf[256];
						formatf(buf, "%s is not currently loaded!", g_MapDataStore.GetName(strLocalIndex(sMapDataDefIndex[sSelectedMapData])));
						bank.AddTitle(buf);

						//JKINZ
						const spdAABB &aabb = fwMapDataStore::GetStore().GetStreamingBounds(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
						bank.AddTitle("");
						bank.AddTitle("Streaming extents:");
						bank.AddTitle("Min =    {%f, %f, %f}", aabb.GetMin().GetXf(), aabb.GetMin().GetYf(), aabb.GetMin().GetZf());
						bank.AddTitle("Max =    {%f, %f, %f}", aabb.GetMax().GetXf(), aabb.GetMax().GetYf(), aabb.GetMax().GetZf());
						bank.AddTitle("Center = {%f, %f, %f}", aabb.GetCenter().GetXf(), aabb.GetCenter().GetYf(), aabb.GetCenter().GetZf());

						bank.AddSeparator("");
						bank.AddButton("Refresh Map Widgets", OnMapNameComboChanged);
					}
				}
			}
			sBank->UnSetCurrentGroup(*sSelectionGroup);
		}
	}

	void OnSelectedEntityBatchChanged()
	{
		if(sSelectedMapData >= 0)
		{
			fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
			if(pDef && !pDef->IsLoaded())
				OnMapNameComboChanged();	//Map streamed out, update widgets
			else
			{
				while(bkWidget *widget = sSelectedEntityGroup->GetChild())
					widget->Destroy();

				sBank->SetCurrentGroup(*sSelectedEntityGroup);
				UpdateSelectedEntityBatchWidgets(*sBank);
				sBank->UnSetCurrentGroup(*sSelectedEntityGroup);
			}
		}
	}

	bkCombo *AddMapNameComboWidget(bkBank &bank)
	{
		static const char *sNoSelecitonName = ""; //1st element is an empty string that means no selection.
		typedef rage::atArray<const char *> name_array;
		name_array names(sMapDataDefIndex.size() + 1, sMapDataDefIndex.size() + 1);

		//Populate names array
		name_array::iterator iter = names.begin();
		*iter = sNoSelecitonName;
		std::transform(sMapDataDefIndex.begin(), sMapDataDefIndex.end(), ++iter, std::bind1st(std::mem_fun<const char *, const fwMapDataStore, int>(&fwMapDataStore::GetName), &g_MapDataStore));

		return bank.AddCombo("Map Data", &sSelectedMapData, names.size(), names.GetElements(), -1, OnMapNameComboChanged);
	}

	void RefreshMapDataDefIndexMap()
	{
		sMapDataDefIndex.Reset();
		InitMapDataDefIndexMap();

		if(sMapNameComboWidget)
		{
			static const char *sNoSelecitonName = ""; //1st element is an empty string that means no selection.
			typedef rage::atArray<const char *> name_array;
			name_array names(sMapDataDefIndex.size() + 1, sMapDataDefIndex.size() + 1);

			//Populate names array
			name_array::iterator iter = names.begin();
			*iter = sNoSelecitonName;
			std::transform(sMapDataDefIndex.begin(), sMapDataDefIndex.end(), ++iter, std::bind1st(std::mem_fun<const char *, const fwMapDataStore, int>(&fwMapDataStore::GetName), &g_MapDataStore));

			sMapNameComboWidget->UpdateCombo("Map Data", reinterpret_cast<void *>(&sSelectedMapData), names.size(), names.GetElements(), -1, OnMapNameComboChanged);
		}
	}

	void OnSetSelectedEntityFromPicker()
	{
		if(fwEntity *entity = g_PickerManager.GetSelectedEntity())
		{
			int mapIndex = -1;
			if(entity->GetType() == ENTITY_TYPE_INSTANCE_LIST)				//CEntity::GetIsTypeInstanceList()
			{
				CEntityBatch *eb = static_cast<CEntityBatch *>(entity);
				mapIndex = eb->GetMapDataDefIndex();
			}
			else if(entity->GetType() == ENTITY_TYPE_GRASS_INSTANCE_LIST)	//CEntity::GetIsTypeGrassInstanceList()
			{
				CGrassBatch *eb = static_cast<CGrassBatch *>(entity);
				mapIndex = eb->GetMapDataDefIndex();
			}
			else
			{
				return;	//Not a batch entity
			}

			int batchIndex = FindEBIndexInMap(entity, mapIndex);
			int remappedIndex = -1; //Find remapped map index
			{
				rage::atArray<s32>::iterator end = sMapDataDefIndex.end();
				rage::atArray<s32>::iterator iter =  std::find(sMapDataDefIndex.begin(), end, mapIndex);
				if(iter == end)
				{
					RefreshMapDataDefIndexMap();	//Maybe array is stale?
					iter =  std::find(sMapDataDefIndex.begin(), end, mapIndex);
				}

				if(Verifyf(iter != end, "WARNING: MapData \"%s\" (index: %d) is not being found by Entity Batch Selection system!", g_MapDataStore.GetName(strLocalIndex(sMapDataDefIndex[sSelectedMapData])), mapIndex))
					remappedIndex = static_cast<int>(iter - sMapDataDefIndex.begin());
			}

			if(remappedIndex >= 0 && batchIndex >= 0)
			{
				if(remappedIndex != sSelectedMapData)
				{
					sSelectedMapData = remappedIndex;
					sSelectedEntityBatch = batchIndex;
					OnMapNameComboChanged();
				}
				else if(batchIndex != sSelectedEntityBatch)
				{
					sSelectedEntityBatch = batchIndex;
					OnSelectedEntityBatchChanged();
				}
			}
		}
	}

	//Update entity batch AO values
	bool UpdateBatchAoScales(void *pItem, void *UNUSED_PARAM(data))
	{
		if(CEntity *entity = static_cast<CEntity *>(pItem))
			entity->CalculateDynamicAmbientScales();

		return true;
	}

	void UpdateAllBatchAoScales()
	{
		CEntityBatch::GetPool()->ForAll(EBStatic::UpdateBatchAoScales, NULL);
		CGrassBatch::GetPool()->ForAll(EBStatic::UpdateBatchAoScales, NULL);
	}

	void DisplayStaticInstanceBuffer(fwPropInstanceListDef::IBList *OUTPUT_ONLY(ibList), const char *OUTPUT_ONLY(prefix), int OUTPUT_ONLY(batchIndex = -1))
	{
#if !__NO_OUTPUT
		if(ibList)
		{
			grcStaticInstanceBufferList &list = ibList->GetCurrentList();
			grcInstanceBufferBasic *first = static_cast<grcInstanceBufferBasic *>(list.GetFirst());
			
			u32 ibCount = 0;
			for(grcInstanceBuffer *curr = first; curr; curr = curr->GetNext())
				++ibCount;

			Displayf(	"%sIBList<%d>[%d]%s%3d: Count: %5d\tInst Per Draw: %3d\tlist: (grcStaticInstanceBufferList *)0x%p, first: (grcInstanceBufferBasic *)0x%p",
						prefix, fwPropInstanceListDef::IBList::MaxFrames, grcInstanceBuffer::GetCurrentFrame(fwPropInstanceListDef::IBList::MaxFrames), (batchIndex >= 0 ? " " : ""), batchIndex,
						ibCount, grcInstanceBuffer::MaxPerDraw, &list, first);

			u32 ibIndex = 0;
			for(grcInstanceBufferBasic *curr = first; curr; curr = static_cast<grcInstanceBufferBasic *>(curr->GetNext()))
			{
				//Displayf("%s\t%5d - (grcInstanceBufferBasic *)0x%p:\tCount: %2d\tElemSizeQW: %2d", prefix, ibIndex, curr, curr->GetCount(), curr->GetElemSizeQW());
				Displayf("%s\t%5d - (grcInstanceBufferBasic *)0x%p:\tCount: %2zu\tElemSizeQW: %2zu\tData: (Vec4V *)0x%p", prefix, ibIndex, curr, curr->GetCount(), curr->GetElemSizeQW(), curr->GetData());

				const Vec4V *data = reinterpret_cast<const Vec4V *>(curr->GetData());
				if(sOutputStaticIBData && data)
				{
					for(int i = 0; i < curr->GetCount(); ++i)
					{
						for(int j = 0; j < curr->GetElemSizeQW(); ++j)
						{
							const Vec4V vec = data[(i * curr->GetElemSizeQW()) + j];
							Displayf("%s\t\tinst[%d].Vec4V[%d]: {%.8f, %.8f, %.8f, %.8f}", prefix, i, j, vec.GetXf(), vec.GetYf(), vec.GetZf(), vec.GetWf());
						}
					}
				}

				++ibIndex;
			}
		}
#endif
	}

	void DisplayPropEntityInfo(const CEntityBatch *OUTPUT_ONLY(eb), const char *OUTPUT_ONLY(prefix), int OUTPUT_ONLY(batchIndex = -1))
	{
#if !__NO_OUTPUT
		if(eb && eb->GetInstanceList())
		{
			typedef CEntityBatch::InstanceList InstanceList;
			const InstanceList *list = eb->GetInstanceList();

			//Display batch header info:
			atHashString imapLink;
			fwMapDataDef *pDef = (eb->GetMapDataDefIndex() > -1 ? g_MapDataStore.GetSlot(strLocalIndex(eb->GetMapDataDefIndex())) : NULL);
			if(pDef && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
			{
				fwMapDataContents *contents = pDef->GetContents();
				fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
				imapLink = instList.m_ImapLink;
			}
			const char *imapName = (imapLink.TryGetCStr() != NULL ? imapLink.TryGetCStr() : "<NULL>");
			Displayf(	"%sBatch%s%3d: Instance Count: %d\tArchetype: %s\tAABB [min|max] = {%f, %f, %f} | {%f, %f, %f}\tLOD Dist: %d (0x%x)\tLink Map Name|Index: %32s (0x%8x) | %5d",
						prefix, (batchIndex >= 0 ? " " : ""), batchIndex,
						list->m_InstanceList.size(), (list->m_archetypeName.TryGetCStr() ? list->m_archetypeName.TryGetCStr() : "<Unknown>"),
						list->m_BatchAABB.GetMin().GetXf(), list->m_BatchAABB.GetMin().GetYf(), list->m_BatchAABB.GetMin().GetZf(),
						list->m_BatchAABB.GetMax().GetXf(), list->m_BatchAABB.GetMax().GetYf(), list->m_BatchAABB.GetMax().GetZf(), list->m_lodDist, list->m_lodDist,
						imapName, imapLink.GetHash(), eb->GetHdMapDataDefIndex()	);

			//fwMapDataDef *pDefHD = (eb->GetHdMapDataDefIndex() > -1 ? g_MapDataStore.GetSlot(eb->GetHdMapDataDefIndex()) : NULL);
			InstanceList::InstanceDataList::const_iterator end = list->m_InstanceList.end();
			InstanceList::InstanceDataList::const_iterator begin = list->m_InstanceList.begin();
			InstanceList::InstanceDataList::const_iterator iter;
			for(iter = begin; iter != end; ++iter)
			{
				Displayf(	"%s\t%4d:\tPosition: {%5.6f, %5.6f, %5.6f}\tColor: {%3d, %3d, %3d, %3d}\tVisible: %-5s\t"
							"Matrix: {%.8f, %.8f, %.8f} | {%.8f, %.8f, %.8f} | {%.8f, %.8f, %.8f}\tLink Entity Index: %5d\tGlobals: {%5.6f, %5.6f, %5.6f, %5.6f}",
							prefix, (int)(iter - begin), iter->m_InstMat[0].GetWf(), iter->m_InstMat[1].GetWf(), iter->m_InstMat[2].GetWf(),
							iter->m_Tint.GetRed(), iter->m_Tint.GetGreen(), iter->m_Tint.GetBlue(), iter->m_Tint.GetAlpha(),  iter->m_IsVisible ? "true" : "false",
							iter->m_InstMat[0].GetXf(), iter->m_InstMat[0].GetYf(), iter->m_InstMat[0].GetZf(),
							iter->m_InstMat[1].GetXf(), iter->m_InstMat[1].GetYf(), iter->m_InstMat[1].GetZf(),
							iter->m_InstMat[2].GetXf(), iter->m_InstMat[2].GetYf(), iter->m_InstMat[2].GetZf(),
							iter->m_Index, iter->m_Globals.GetXf(), iter->m_Globals.GetYf(), iter->m_Globals.GetZf(), iter->m_Globals.GetWf()	);
			}

			if(sOutputStaticInstanceBuffer)
			{
				Displayf("%s", prefix);
				DisplayStaticInstanceBuffer(const_cast<fwPropInstanceListDef::IBList *>(list->GetStaticInstanceBufferList()), prefix, batchIndex);
				Displayf("%s", prefix);
			}
		}
#endif
	}

	void DisplayGrassEntityInfo(const CGrassBatch *OUTPUT_ONLY(eb), const char *OUTPUT_ONLY(prefix), int OUTPUT_ONLY(batchIndex = -1))
	{
#if !__NO_OUTPUT
		if(eb && eb->GetInstanceList())
		{
			typedef CGrassBatch::InstanceList InstanceList;
			const InstanceList *list = eb->GetInstanceList();

			//Display batch header info:
			Displayf(	"%sBatch%s%3d: Instance Count: %d\tArchetype: %s\tAABB [min|max] = {%f, %f, %f} | {%f, %f, %f}\tLOD Dist: %d (0x%x)", prefix, (batchIndex >= 0 ? " " : ""),
						batchIndex, list->m_InstanceList.size(), (list->m_archetypeName.TryGetCStr() ? list->m_archetypeName.TryGetCStr() : "<Unknown>"),
						list->m_BatchAABB.GetMin().GetXf(), list->m_BatchAABB.GetMin().GetYf(), list->m_BatchAABB.GetMin().GetZf(),
						list->m_BatchAABB.GetMax().GetXf(), list->m_BatchAABB.GetMax().GetYf(), list->m_BatchAABB.GetMax().GetZf(), list->m_lodDist, list->m_lodDist	);
			
			const Vec3V basePos = list->m_BatchAABB.GetMin();
			const Vec3V offsetScale = list->m_BatchAABB.GetMax() - basePos;

			InstanceList::InstanceDataList::const_iterator end = list->m_InstanceList.end();
			InstanceList::InstanceDataList::const_iterator begin = list->m_InstanceList.begin();
			InstanceList::InstanceDataList::const_iterator iter;
			for(iter = begin; iter != end; ++iter)
			{
				//Compute position
				typedef std::remove_reference<decltype(iter->m_Position[0])>::type pos_offset_type;
				CompileTimeAssert(std::is_arithmetic<pos_offset_type>::value); //If this isn't the case, the line below will likely fail.
			#if RSG_PC || RSG_DURANGO
				static const pos_offset_type sMaxVal = static_cast<pos_offset_type>(-1);
			#else //RSG_PC
				static const auto sMaxVal = std::numeric_limits<pos_offset_type>::max();
			#endif //RSG_PC
			
				const ScalarV dist = ScalarV(static_cast<float>(sMaxVal));
				const Vec3V offset = Vec3V(static_cast<float>(iter->m_Position[0]), static_cast<float>(iter->m_Position[1]), static_cast<float>(iter->m_Position[2])) / dist;
				Vec3V position = basePos + (offset * offsetScale);
				Vec3V normal = iter->ComputeNormal();

				Vec3V vProbePos(V_ZERO);
				Vec3V vProbeNorm(V_ZERO);
				ScalarV vDot(V_NAN);
				if(sProbeForNormal)
				{
					WorldProbe::CShapeTestProbeDesc probeDesc; 
					WorldProbe::CShapeTestHitPoint probeIsect; 
					WorldProbe::CShapeTestResults probeResult(probeIsect); 
					probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(position + (Vec3V(V_UP_AXIS_WZERO) * ScalarV(V_QUARTER))), VEC3V_TO_VECTOR3(position + (Vec3V(V_UP_AXIS_WZERO) * ScalarV(V_NEGSIXTEEN)))); 
					probeDesc.SetResultsStructure(&probeResult); 
					probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED); 
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES); 
					probeDesc.SetContext(WorldProbe::ENotSpecified); 

					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc)) 
					{
						vProbePos = probeResult[0].GetHitPositionV();
						vProbeNorm = probeResult[0].GetHitNormalV();
						vDot = Dot(vProbeNorm, normal);
					}
					else
					{
						//Were we under the terrain? Try reversing the direction of the probe.
						probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(position - (Vec3V(V_UP_AXIS_WZERO) * ScalarV(V_QUARTER))), VEC3V_TO_VECTOR3(position + (Vec3V(V_UP_AXIS_WZERO) * ScalarV(V_FIFTEEN)))); 
						if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc)) 
						{
							vProbePos = probeResult[0].GetHitPositionV();
							vProbeNorm = probeResult[0].GetHitNormalV();
							vDot = Dot(vProbeNorm, normal);
						}
					}
				}

				Displayf(	"%s\t%4d:\tPosition [Offset|Final]: {%3d, %3d, %3d} | {%5.6f, %5.6f, %5.6f}\tColor: {%3d, %3d, %3d}\tNormal: {%5.6f, %5.6f, %5.6f}\tScale: %3d\tAo: %3d\t"
							"ProbePos: {%5.6f, %5.6f, %5.6f}\tProbeNormal: {%5.6f, %5.6f, %5.6f}\tDot(Normal, ProbeNormal): %5.6f",
							prefix, (int)(iter - begin), iter->m_Position[0], iter->m_Position[1], iter->m_Position[2], position.GetXf(), position.GetYf(), position.GetZf(),
							iter->m_Color[0], iter->m_Color[1], iter->m_Color[2],  normal.GetXf(), normal.GetYf(), normal.GetZf(), iter->m_Scale, iter->m_Ao,
							vProbePos.GetXf(), vProbePos.GetYf(), vProbePos.GetZf(), vProbeNorm.GetXf(), vProbeNorm.GetYf(), vProbeNorm.GetZf(), vDot.Getf()	);
			}
		}
#endif
	}

	void DisplaySelectedInfo()
	{
		if(CEntity *entity = static_cast<CEntity *>(GetSelectedEntity()))
		{
			if(entity->GetIsTypeInstanceList())
			{
				DisplayPropEntityInfo(static_cast<CEntityBatch *>(entity), "BATCH-DBG:\t", sSelectedEntityBatch);
			}
			else if(entity->GetIsTypeGrassInstanceList())
			{
				DisplayGrassEntityInfo(static_cast<CGrassBatch *>(entity), "GRASS-DBG:\t", sSelectedEntityBatch);
			}
		}
		else if(sSelectedMapData >= 0)
		{
			//Output data for the whole map.
			fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
			if(pDef && pDef->IsLoaded() && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
			{
				fwMapDataContents *contents = pDef->GetContents();
				u32 numEntities = contents->GetNumEntities();
				fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
				u32 instEntityOffset = numEntities - (instList.m_PropInstanceList.size() + instList.m_GrassInstanceList.size());
				u32 grassEntityOffset = numEntities - instList.m_GrassInstanceList.size();

				//Display map data header:
				spdAABB aabb = fwMapDataStore::GetStore().GetStreamingBounds(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
				Displayf(	"GRASS-DBG:\t%s (MapData Slot: %d): EntityBatch Count: %d\tGrassBatch Count: %d\tStreaming AABB[min|max] = {%f, %f, %f} | {%f, %f, %f}",
							g_MapDataStore.GetName(strLocalIndex(sMapDataDefIndex[sSelectedMapData])), sMapDataDefIndex[sSelectedMapData], instList.m_PropInstanceList.size(), instList.m_GrassInstanceList.size(),
							aabb.GetMin().GetXf(), aabb.GetMin().GetXf(), aabb.GetMin().GetXf(),
							aabb.GetMax().GetXf(), aabb.GetMax().GetXf(), aabb.GetMax().GetXf()	);

				for(int i = instEntityOffset; i < grassEntityOffset; ++i)
				{
					CEntity *entity = static_cast<CEntity *>(contents->GetEntities()[i]);
					if(entity && Verifyf(entity->GetIsTypeGrassInstanceList(), "Expected Grass Batch entity was incorrect type!"))
						DisplayPropEntityInfo(static_cast<CEntityBatch *>(entity), "BATCH-DBG:\t\t", i - instEntityOffset);
				}
				
				for(int i = grassEntityOffset; i < numEntities; ++i)
				{
					CEntity *entity = static_cast<CEntity *>(contents->GetEntities()[i]);
					if(entity && Verifyf(entity->GetIsTypeGrassInstanceList(), "Expected Grass Batch entity was incorrect type!"))
						DisplayGrassEntityInfo(static_cast<CGrassBatch *>(entity), "GRASS-DBG:\t\t", i - grassEntityOffset);
				}
			}
		}
	}

	void AddWidgets(bkBank &bank)
	{
		//Cache off main group ptr
		sBank = &bank;

		//Initialize MapDataDef Index Map
		InitMapDataDefIndexMap();

		//Selection Widgets
		sSelectionGroup = bank.PushGroup("Select/Edit Batch", false);
		{
			bank.AddButton("Set Selected Entity From Picker", OnSetSelectedEntityFromPicker);
			sMapNameComboWidget = AddMapNameComboWidget(bank);
		}
		bank.PopGroup();

		//Performance & Usage Stats
		bank.PushGroup("Performance & Usage Stats", false);
		{
			bank.AddSlider(" Shadow LOD Factor ", &sShadowLodFactor,0.f, 1.f, 0.001f);
			bank.AddToggle("Enable Usage Info Display", &sPerfDisplayUsageInfo);
			bank.AddSeparator("");

			bank.AddToggle("Display Runtime Info", &sPerfDisplayRuntimeInfo);
#if GRASS_BATCH_CS_CULLING
			bank.AddToggle("Display CS Culled Info - *WARNING! SLOW!*", &sPerfDisplayGrassCSInfo);
			ORBIS_ONLY(bank.AddToggle("Add GPU Fence for CS Culled Info", &sPerfDisplayGrassCS_UseGPUFence));
#endif //GRASS_BATCH_CS_CULLING
			GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(bank.AddToggle("Display Pre-Allocated Buffer Info", &sPerfDisplayAllocatorInfo));
			bank.AddToggle("Display Prop Usage Info", &sPerfDisplayPropUsageInfo);
			bank.AddToggle("Display Grass Usage Info", &sPerfDisplayGrassUsageInfo);
		}
		bank.PopGroup();

		//Debug Draw
		bank.PushGroup("Debug Draw", false);
		{
			bank.AddToggle("Enable Debug Draw", &sDebugDrawEnabled);
			bank.AddSeparator("");

			bank.AddToggle("Draw Batch Bounds", &sDebugDrawBatchAABB);
			bank.AddToggle("Draw Instance Bounds", &sDebugDrawInstances);
			bank.AddToggle("Draw Linked Bounds", &sDebugDrawLinkedBounds);

			bank.AddTitle("");
			bank.AddToggle("Draw Linked Info - Rendered", &sDebugDrawLinkedRenderedInfo);
			bank.AddToggle("Draw Linked Info - Not Rendered", &sDebugDrawLinkedNotRenderedInfo);
			
			bank.PushGroup("Extra Debug Draw Params", false);
			{
				bank.AddToggle("Draw Grass Terrain Normal", &sDebugDrawGrassTerrainNormalEnabled);
				bank.AddSlider("Terrain Normal Length", &sDebugDrawGrassTerrainNormalLength, 0.0f, 10.0f, 0.1f);
				bank.AddSlider("Terrain Normal Cone Length", &sDebugDrawGrassTerrainNormalConeLength, 0.0f, 10.0f, 0.1f);
				bank.AddSlider("Terrain Normal Cone Radius", &sDebugDrawGrassTerrainNormalConeRadius, 0.0f, 10.0f, 0.1f);
				bank.AddColor("Terrain Normal Color", &sDebugDrawGrassTerrainNormalColor);

				bank.AddTitle("");
				bank.AddToggle("Draw Batch Bounds Solid", &sDebugDrawBatchAABBSolid);
				bank.AddColor("Batch Bounds Color", &sDebugDrawBatchAABBColor);

				bank.AddTitle("");
				bank.AddToggle("Draw Instance Bounds Solid", &sDebugDrawInstanceAABBSolid);
				bank.AddColor("Instance Bounds Color", &sDebugDrawInstanceAABBColor);
				bank.AddColor("Linked Bounds - Drawn Color", &sDebugDrawRenderedHdAABBColor);
				bank.AddColor("Linked Bounds - Not Drawn Color", &sDebugDrawNotRenderedHdAABBColor);

				bank.AddTitle("");
				bank.AddColor("Selected Bounds Color", &sDebugDrawSelectedBatchAABBColor);
				bank.AddColor("Selected Batch Text Color", &sDebugDrawSelectedTextColor);
			}
			bank.PopGroup();
		}
		bank.PopGroup();

		//CSE Widgets
		CCustomShaderEffectGrass::AddWidgets(bank);

		//Debug Widgets
		bank.PushGroup("----== Debug ==----", false);
		{
			bank.PushGroup("Performance", false);
			{
#if RSG_DURANGO || RSG_PC
	#if RSG_PC
				bank.AddToggle("Force Skip Instances", &bPerfForceSkipInstance);
	#endif
				bank.AddSlider("Skip Instances",&sPerfSkipInstance,0,15,1);
#endif // RSG_DURANGO || RSG_PC...
				bank.AddToggle("GBuf: Only draw 1st instance of batch - Is draw call bound?", &sPerfOnlyDraw1stInstanceGBuf);
				bank.AddToggle("Shadows: Only draw 1st instance of batch - Is draw call bound?", &sPerfOnlyDraw1stInstanceShadows);
#if GRASS_BATCH_CS_CULLING
				bank.PushGroup("Compute Shader", false);
				{
					bank.AddToggle("Compute Shader: Enable", &sPerfEnableComputeShader);
					bank.AddSeparator("");
					bank.AddToggle("CS: Freeze Current Culling VP", &sGrassComputeShader_FreezeVp);
					bank.AddToggle("CS: Allow Batching", &sGrassComputeShader_AllowBatching);
					GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(bank.AddToggle("CS: Allow CopyStructureCount Batching", &sGrassComputeShader_AllowCopyStructureCountBatching));
					GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(bank.AddToggle("CS: Avoid interleaving Dispatch() and CopySubresource() calls", &sGrassComputeShader_AvoidInterleaving));
					bank.AddToggle("CS: Flush CS After Each Batch", &sFlushCSAfterEachDispatch);
					bank.AddToggle("CS: LOD - Disable Crossfading", &sCSDisableCrossfade);
					bank.AddToggle("CS: LOD - Disable LOD Scale", &sGrassComputeShader_IgnoreLodScale_DrawableLOD);
					bank.AddToggle("CS: LOD Fade - Disable LOD Scale", &sGrassComputeShader_IgnoreLodScale_LODFade);
					ORBIS_ONLY(bank.AddToggle("CS: Debug Draw Indirect Call", &sGrassComputeShader_DebugDrawIndirect));
					bank.AddToggle("CS: UAV Sync" , &sGrassComputeShader_UAV_Sync);
					bank.PushGroup("Frozen Viewport", false);
					{
						const Vec4V *planes = sFrozenVp.GetFrustumClipPlaneArray();
						for(int i = 0; i < 6; ++i)
							bank.AddVector("Viewport:", const_cast<Vec4V *>(&(planes[i])), -20000.0f, 20000.0f, 0.1f);
					}
					bank.PopGroup();
				}
				bank.PopGroup();
#endif //GRASS_BATCH_CS_CULLING
			}
			bank.PopGroup();
			bank.AddToggle("Compute AO Per Instance", &sComputeAoScalePerInstance, UpdateAllBatchAoScales);
			bank.AddToggle("Enable Dynamic Visibility From Entity Link", &sEnableLinkageVisibility);
			bank.AddToggle("Allow Linkage Visibility Reset Each Frame", &sResetLinkageVisibility);

			bank.AddToggle("Override Creation LOD Distance", &sOverrideLodDist);
			bank.AddSlider("LOD Distance Value", &sOverriddenLodDistValue, 0u, 0x1fffu, 1u);

			bank.AddSeparator("");
			bank.AddToggle("Props: Output Static Instance Buffer", &sOutputStaticInstanceBuffer);
			bank.AddToggle("Props: Output Static IB Data", &sOutputStaticIBData);
			bank.AddToggle("Grass: Probe for actual terrain normal", &sProbeForNormal);
			bank.AddButton("Display Selected Info", DisplaySelectedInfo);

			bank.AddSeparator("");
			bank.AddButton("Refresh MapData Map (dropdown)", RefreshMapDataDefIndexMap);
		}
		bank.PopGroup();
	}

	///////////////////////////////////
	//Display Performance & Usage Stats

	template <class T, class count_t>
	count_t AccumulateArraySize(count_t count, const T &def)
	{
		return count + def.m_InstanceList.size();
	}

	size_t AccumulateIBMemUsage(size_t usage, const fwPropInstanceListDef &def)
	{
		return usage + (def.GetStaticInstanceBufferList() ? def.GetStaticInstanceBufferList()->GetIBMemUsage() : 0);
	}

	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(void DisplayAppendBufferAllocatorInfo(bool isProportional, int seperatorPixelHeight));

	void DisplayUsageInfo()
	{
		static BankInt32 sSeperatorPixelHeight = 3;
		static BankBool sProportional = true;
		bool addSeperator = false;

		if(sMapDataDefIndex.empty())
			RefreshMapDataDefIndexMap(); //Make sure the data def map is valid

		//Runtime stats:
#if GRASS_BATCH_CS_CULLING
		if(sPerfDisplayRuntimeInfo && sPerfDisplayGrassCSInfo)
		{
			grcDebugDraw::AddDebugOutputEx(sProportional, "  Pass    # batches rendered    # instances rendered    # instances rendered (LOD Fade)    # Inst CS    # 0-Size Batches Culled");
			grcDebugDraw::AddDebugOutputEx(sProportional, "GBuff:    %9d         %9d           %9d                        %9d    %9d", sPerfBatchesRenderedGbuf, sPerfInstancesRenderedGbuf, sPerfInstancesRenderedGbuf_LODFade, sPerfCSInstancesRenderedGbuf, sPerfCSZeroCountBatchesGbuf);
			grcDebugDraw::AddDebugOutputEx(sProportional, "Shadow:   %9d         %9d           %9d                        %9d    %9d", sPerfBatchesRenderedShadow, sPerfInstancesRenderedShadow, sPerfInstancesRenderedShadow_LODFade, sPerfCSInstancesRenderedShadow, sPerfCSZeroCountBatchesShadow);
			if(sPerfBatchesRenderedOther > 0)
			grcDebugDraw::AddDebugOutputEx(sProportional, "Other:    %9d         %9d           %9d                        %9d    %9d", sPerfBatchesRenderedOther, sPerfInstancesRenderedOther, sPerfInstancesRenderedOther_LODFade, sPerfCSInstancesRenderedOther, sPerfCSZeroCountBatchesOther);
			grcDebugDraw::AddDebugOutputSeparator(sSeperatorPixelHeight);
		}
		else
#endif //GRASS_BATCH_CS_CULLING
		if(sPerfDisplayRuntimeInfo)
		{
			grcDebugDraw::AddDebugOutputEx(sProportional, "  Pass    # batches rendered    # instances rendered    # instances rendered (LOD Fade)");
			grcDebugDraw::AddDebugOutputEx(sProportional, "GBuff:    %9d         %9d           %9d", sPerfBatchesRenderedGbuf, sPerfInstancesRenderedGbuf, sPerfInstancesRenderedGbuf_LODFade);
			grcDebugDraw::AddDebugOutputEx(sProportional, "Shadow:   %9d         %9d           %9d", sPerfBatchesRenderedShadow, sPerfInstancesRenderedShadow, sPerfInstancesRenderedShadow_LODFade);
			if(sPerfBatchesRenderedOther > 0)
			grcDebugDraw::AddDebugOutputEx(sProportional, "Other:    %9d         %9d           %9d", sPerfBatchesRenderedOther, sPerfInstancesRenderedOther, sPerfInstancesRenderedOther_LODFade);
			grcDebugDraw::AddDebugOutputSeparator(sSeperatorPixelHeight);
		}

		GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(if(sPerfDisplayAllocatorInfo)	DisplayAppendBufferAllocatorInfo(sProportional, sSeperatorPixelHeight));

		grcDebugDraw::AddDebugOutputEx(sProportional, "            Map Name              # batches  # instances MapData Mem (KB) IB Mem (KB)");

		rage::atArray<s32>::const_iterator end = sMapDataDefIndex.end();
		rage::atArray<s32>::const_iterator iter;
		if(sPerfDisplayPropUsageInfo)
		{
			u32 totalMaps = 0; u32 totalBatches = 0; u32 totalInstances = 0; size_t totalMapMem = 0; size_t totalIBMem = 0;
			for(iter = sMapDataDefIndex.begin(); iter != end; ++iter)
			{
				fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(*iter));
				if(pDef && pDef->IsLoaded() && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
				{
					fwMapDataContents *contents = pDef->GetContents();
					fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
					if(!instList.m_PropInstanceList.empty())
					{
						typedef fwPropInstanceListDef::InstanceDataList InstanceDataList;
						const size_t defSize = sizeof(fwPropInstanceListDef);
						const size_t instSize = sizeof(InstanceDataList::value_type);
						int numBatches = instList.m_PropInstanceList.size();
						InstanceDataList::size_type numInstances = 0;
						numInstances = std::accumulate(instList.m_PropInstanceList.begin(), instList.m_PropInstanceList.end(), numInstances, std::ptr_fun(&AccumulateArraySize<fwPropInstanceListDef, InstanceDataList::size_type>));
						size_t mapMem = defSize * numBatches + instSize * numInstances;
						
						size_t ibMem = 0;
						ibMem = std::accumulate(instList.m_PropInstanceList.begin(), instList.m_PropInstanceList.end(), ibMem, std::ptr_fun(&AccumulateIBMemUsage));

						grcDebugDraw::AddDebugOutputEx(	sProportional, "%-32s %11d %11d %16d %11d", g_MapDataStore.GetName(strLocalIndex(*iter)), numBatches, numInstances, mapMem / 1024, ibMem / 1024);

						//Store totals:
						++totalMaps; totalBatches += numBatches; totalInstances += numInstances; totalMapMem += mapMem; totalIBMem += ibMem;
						addSeperator = true;
					}
				}
			}
			if(addSeperator)
				grcDebugDraw::AddDebugOutputEx(	sProportional, "Totals:      %11d Maps   %11d %11d %16d %11d", totalMaps, totalBatches, totalInstances, totalMapMem / 1024, totalIBMem / 1024);
		}

		if(addSeperator)
			grcDebugDraw::AddDebugOutputSeparator(sSeperatorPixelHeight);

		addSeperator = false;
		if(sPerfDisplayGrassUsageInfo)
		{
			u32 totalMaps = 0; u32 totalBatches = 0; u32 totalInstances = 0; size_t totalMapMem = 0;
			for(iter = sMapDataDefIndex.begin(); iter != end; ++iter)
			{
				fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(*iter));
				if(pDef && pDef->IsLoaded() && pDef->GetContents() && pDef->GetContents()->GetLoaderInstRef().GetInstance())
				{
					fwMapDataContents *contents = pDef->GetContents();
					fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
					if(!instList.m_GrassInstanceList.empty())
					{
						typedef fwGrassInstanceListDef::InstanceDataList InstanceDataList;
						const size_t defSize = sizeof(fwGrassInstanceListDef);
						const size_t instSize = sizeof(InstanceDataList::value_type);
						int numBatches = instList.m_GrassInstanceList.size();
						InstanceDataList::size_type numInstances = 0;
						numInstances = std::accumulate(instList.m_GrassInstanceList.begin(), instList.m_GrassInstanceList.end(), numInstances, std::ptr_fun(&AccumulateArraySize<fwGrassInstanceListDef, InstanceDataList::size_type>));
						size_t mem = defSize * numBatches + instSize * numInstances;

						grcDebugDraw::AddDebugOutputEx(	sProportional, "%32s %11d %11d %16d %11s", g_MapDataStore.GetName(strLocalIndex(*iter)), numBatches, numInstances, mem / 1024, "<N/A>");

						//Store totals:
						++totalMaps; totalBatches += numBatches; totalInstances += numInstances; totalMapMem += mem;
						addSeperator = true;
					}
				}
			}

			if(addSeperator)
				grcDebugDraw::AddDebugOutputEx(	sProportional, "Totals:      %11d Maps   %11d %11d %16d %11s", totalMaps, totalBatches, totalInstances, totalMapMem / 1024, "<N/A>");
		}
	}

	///////////////////////////////////
	// Debug Draw functionality

	inline void CopyMatrixFromInstance(const CEntityBatch *UNUSED_PARAM(eb), const CEntityBatch::InstanceList::InstanceData &inst, Mat34V_InOut mat)
	{
		inst.GetMatrixCopy(mat);
	}

	inline void CopyMatrixFromInstance(const CGrassBatch *eb, const CGrassBatch::InstanceList::InstanceData &inst, Mat34V_InOut mat)
	{
		Vec3V position(V_ZERO);
		if(eb && eb->GetInstanceList())
		{
			//numeric_limits w/ decltype don't work, presumably b/c of STL port.
			//CompileTimeAssert(std::is_arithmetic<decltype(iter->m_Position[0])>()); //If this isn't the case, the line below will likely fail.
			//const ScalarV dist = ScalarV(static_cast<float>(std::numeric_limits<decltype(iter->m_Position[0])>::max()));
			//const ScalarV dist = ScalarV(static_cast<float>(decltype(iter->m_Position[0])(-1)));
			const ScalarV dist = ScalarV(static_cast<float>(u16(-1)));

			const Vec3V offset = Saturate(Vec3V(static_cast<float>(inst.m_Position[0]), static_cast<float>(inst.m_Position[1]), static_cast<float>(inst.m_Position[2])) / dist);
			const Vec3V basePos = eb->GetInstanceList()->m_BatchAABB.GetMin();
			const Vec3V offsetScale = eb->GetInstanceList()->m_BatchAABB.GetMax() - basePos;
			position = basePos + (offset * offsetScale);
		}

		mat.SetIdentity3x3();
		mat.Setd(position);
	}

	void DebugDrawEntityBatchInstance_Additional(const CEntityBatch *UNUSED_PARAM(eb), const CEntityBatch::InstanceList::InstanceData &UNUSED_PARAM(inst), Mat34V_InOut UNUSED_PARAM(mat))
	{
		//Do nothing for prop batches
	}

	void DebugDrawEntityBatchInstance_Additional(const CGrassBatch *UNUSED_PARAM(eb), const CGrassBatch::InstanceList::InstanceData &inst, Mat34V_InOut mat)
	{
		//Draw terrain normal
		if(sDebugDrawGrassTerrainNormalEnabled)
		{
			Vec3V normal = inst.ComputeNormal();
			Vec3V v1 = mat.d();
			Vec3V v2 = v1 + (normal * ScalarV(sDebugDrawGrassTerrainNormalLength));
			grcDebugDraw::Line(v1, v2, sDebugDrawGrassTerrainNormalColor);

			ScalarV coneLen(sDebugDrawGrassTerrainNormalConeLength / 2.0f);
			Vec3V c1 = v2 + (normal * coneLen);
			Vec3V c2 = v2 - (normal * coneLen);
			grcDebugDraw::Cone(c1, c2, sDebugDrawGrassTerrainNormalConeRadius, sDebugDrawGrassTerrainNormalColor, sDebugDrawGrassTerrainNormalConeCap);
		}
	}

	template<class T>
	void DebugDrawEntityBatchInstance(const T *eb, const typename T::InstanceList::InstanceData &inst)
	{
		CBaseModelInfo *pModelInfo = (eb ? eb->GetBaseModelInfo() : NULL);
		if(pModelInfo)
		{
			Mat34V mat;
			spdAABB aabb = pModelInfo->GetBoundingBox();
			CopyMatrixFromInstance(eb, inst, mat);
			grcDebugDraw::BoxOriented(aabb.GetMin(), aabb.GetMax(), mat, sDebugDrawInstanceAABBColor, sDebugDrawInstanceAABBSolid);

			//Draw additional per-instance stuff?
			DebugDrawEntityBatchInstance_Additional(eb, inst, mat);
		}
	}

	template<class T>
	void DebugDrawLinkedEntityBounds(const T &inst, fwMapDataDef *pDef)
	{
		if(pDef && pDef->IsLoaded() && pDef->GetContents())
		{
			fwMapDataContents *contents = pDef->GetContents();
			fwEntity** entities = contents->GetEntities();
			u32 numEntities = contents->GetNumEntities();

			if(inst.m_Index < numEntities)
			{
				if(CEntity *entity = static_cast<CEntity *>(entities[inst.m_Index]))
				{
					if(CBaseModelInfo *pModelInfo = entity->GetBaseModelInfo())
					{
						spdAABB aabb = pModelInfo->GetBoundingBox();
						bool drawnThisFrame = entity->GetAddedToDrawListThisFrame(DL_RENDERPHASE_GBUF);
						grcDebugDraw::BoxOriented(aabb.GetMin(), aabb.GetMax(), entity->GetNonOrthoMatrix(), drawnThisFrame ? sDebugDrawRenderedHdAABBColor : sDebugDrawNotRenderedHdAABBColor, sDebugDrawInstanceAABBSolid);

						if((sDebugDrawLinkedRenderedInfo && drawnThisFrame) || (sDebugDrawLinkedNotRenderedInfo && !drawnThisFrame))
						{
							char buf[256];
							entity->GetAABB(aabb);
							float distFromCamera = Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), aabb.GetCenter()).Getf(); //Show dist from camera to make setting lodDist easier.
							const char *modelName = "";
#if !__NO_OUTPUT
							modelName = entity->GetModelName();
#endif //!__NO_OUTPUT
							formatf(buf, sizeof(buf) / sizeof(char), "%s : %d [Rendered = %s - LodDist = %d - DistFromCamera = %f]", modelName,
									inst.m_Index, drawnThisFrame ? "true" : "false", entity->GetLodDistance(), distFromCamera);
							grcDebugDraw::Text(aabb.GetCenter(), sDebugDrawSelectedTextColor, buf);
						}
					}
				}
			}
		}
	}

	template<class T>
	bool IsSelected(T *eb)
	{
		bool selected = false;

		if(eb && sSelectedMapData >= 0)
		{
			if(eb->GetMapDataDefIndex() == sMapDataDefIndex[sSelectedMapData])
			{
				if(sSelectedEntityBatch >= 0)
				{
					selected = (static_cast<CEntity *>(GetSelectedEntity()) == static_cast<CEntity *>(eb));
				}
				else
					selected = true;
			}
		}

		return selected;
	}

	//Grass batches are not linked, so they should not call this function.
	void DebugDrawLinkedEntities(CEntityBatch *eb, bool selected)
	{
		if(eb)
		{
			if(const CEntityBatch::InstanceList *list = eb->GetInstanceList())
			{
				if(sDebugDrawLinkedBounds && (sDebugDrawEnabled || (selected && sSelectedEntityBatchInst < 0)))
				{
					fwMapDataDef *pDef = (eb->GetHdMapDataDefIndex() > -1 ? g_MapDataStore.GetSlot(strLocalIndex(eb->GetHdMapDataDefIndex())) : NULL);
					//since we have removed stlport stl and atArray don't seem to want to play well together
					//std::for_each(list->m_InstanceList.begin(), list->m_InstanceList.end(), std::bind2nd(std::ptr_fun(&DebugDrawLinkedEntityBounds<CEntityBatch::InstanceList::InstanceData>), pDef));
					for (rage::fwPropInstanceListDef::InstanceDataList::const_iterator i = list->m_InstanceList.begin(); i != list->m_InstanceList.end();i++)
					{
						DebugDrawLinkedEntityBounds((*i), pDef);
					}					
				}

				if(selected && sSelectedEntityBatchInst > -1) //Draw selected inst?
				{
					fwMapDataDef *pDef = (eb->GetHdMapDataDefIndex() > -1 ? g_MapDataStore.GetSlot(strLocalIndex(eb->GetHdMapDataDefIndex())) : NULL);
					DebugDrawLinkedEntityBounds(list->m_InstanceList[sSelectedEntityBatchInst], pDef);
				}
			}
		}
	}

	template <class T>
	void DebugDrawBatch(const T *eb, bool selected, int index)
	{
		if(eb)
		{
			typedef typename T::InstanceList InstanceList;
			typedef typename T::InstanceList::InstanceData InstanceData;
			typedef typename T::InstanceList::InstanceDataList::const_iterator InstanceDataIterator;
			const InstanceList *list = eb->GetInstanceList();
			if(list)
			{
				if(sDebugDrawEnabled && sDebugDrawBatchAABB && !selected)
					grcDebugDraw::BoxAxisAligned(list->m_BatchAABB.GetMin(), list->m_BatchAABB.GetMax(), sDebugDrawBatchAABBColor, sDebugDrawBatchAABBSolid);

				if(sDebugDrawInstances && (sDebugDrawEnabled || (selected && sSelectedEntityBatchInst < 0)))
				{
					//since we have removed stlport stl and atArray don't seem to want to play well together
					//std::for_each(list->m_InstanceList.begin(), list->m_InstanceList.end(), std::bind1st(std::ptr_fun(&DebugDrawEntityBatchInstance<T>), eb));
					for (InstanceDataIterator i = list->m_InstanceList.begin(); i != list->m_InstanceList.end();i++)
					{
						DebugDrawEntityBatchInstance(eb,(*i));
					}					
				}
			}

			//Show model aabb transformed by the computed re-scale matrix to emulate the batch aabb.
			if(selected || (sDebugDrawEnabled && sDebugDrawEntityTransformAABB))
			{
				spdAABB aabb;
				eb->GetAABB(aabb);
				grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), selected ? sDebugDrawSelectedBatchAABBColor : sDebugDrawEntityTransformAABBColor, selected ? false : sDebugDrawEntityTransformAABBSolid);

				if(selected)
				{
					static const char *sUnknownArchetype = "<Unknown Archetype Name>";
					const char *archetypeName = (list ? list->m_archetypeName.GetCStr() : sUnknownArchetype);
					float distFromCamera = Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), aabb.GetCenter()).Getf(); //Show dist from camera to make setting lodDist easier.

					char buf[256];
					formatf(buf, sizeof(buf) / sizeof(char), "[%d]%s - %s  [DistFromCamera = %f]", index, archetypeName, g_MapDataStore.GetName(strLocalIndex(sMapDataDefIndex[sSelectedMapData])), distFromCamera);
					grcDebugDraw::Text(aabb.GetCenter(), sDebugDrawSelectedTextColor, buf);

					//Draw selected inst?
					if(sSelectedEntityBatchInst > -1 && list)
						DebugDrawEntityBatchInstance(eb, list->m_InstanceList[sSelectedEntityBatchInst]);
				}
			}
		}
	}

	int DebugGetEBIndex(CEntity *eb)
	{
		if(sSelectedMapData == -1)
			return -1;

		return FindEBIndexInMap(eb, sMapDataDefIndex[sSelectedMapData]);
	}

	bool DebugDrawEntityBatch(void *pItem, void *UNUSED_PARAM(data))
	{
		if(CEntity *entity = static_cast<CEntity *>(pItem))
		{
			if(entity->GetIsTypeInstanceList())
			{
				CEntityBatch *eb = static_cast<CEntityBatch *>(entity);
				bool selected = IsSelected(eb);
				int index = selected ? DebugGetEBIndex(eb) : -1;
				DebugDrawBatch(eb, selected, index);
				DebugDrawLinkedEntities(eb, selected);
			}
			else if(entity->GetIsTypeGrassInstanceList())
			{
				CGrassBatch *eb = static_cast<CGrassBatch *>(entity);
				bool selected = IsSelected(eb);
				int index = selected ? DebugGetEBIndex(eb) : -1;
				DebugDrawBatch(eb, selected, index);
			}
		}

		return true;
	}

	bool IsDebugDrawEnabled()
	{
		return (sDebugDrawEnabled && (sDebugDrawBatchAABB || sDebugDrawInstances || sDebugDrawEntityTransformAABB)) || sSelectedMapData >= 0;
	}

#if GRASS_BATCH_CS_CULLING
	void DrawCSFrozenFrustum()
	{
		if(sGrassComputeShader_FreezeVp)
		{
			sFrustumDbg.Set(sFrozenVp);
			sFrustumDbg.DebugRender();
		}
	}
#endif //GRASS_BATCH_CS_CULLING

	void RenderDebug()
	{
		if(IsDebugDrawEnabled())
		{
			CEntityBatch::GetPool()->ForAll(EBStatic::DebugDrawEntityBatch, NULL);
			CGrassBatch::GetPool()->ForAll(EBStatic::DebugDrawEntityBatch, NULL);
		}

		//Draw selected streaming bounds if it's not loaded to make it easier to find.
		if(sSelectedMapData >= 0)
		{
			fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
			if(pDef)
			{
				const spdAABB &aabb = fwMapDataStore::GetStore().GetStreamingBounds(strLocalIndex(sMapDataDefIndex[sSelectedMapData]));
				grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), pDef->IsLoaded() ? sDebugDrawMapAABBLoadedColor : sDebugDrawMapAABBUnloadedColor, false);
				//grcDebugDraw::Text(aabb.GetCenter(), sDebugDrawSelectedTextColor, g_MapDataStore.GetName(sMapDataDefIndex[sSelectedMapData]));
			}
		}

		//Performance & Usage Stats
		if(sPerfDisplayUsageInfo)
			DisplayUsageInfo();

		GRASS_BATCH_CS_CULLING_ONLY(DrawCSFrozenFrustum());
	}

	void Init(unsigned initMode)
	{
		static bool sMapDataDefIndexMapNeedsInit = true;	//Only need to do this once.
		if(sMapDataDefIndexMapNeedsInit && initMode == INIT_SESSION)
		{
			if(sMapNameComboWidget)	//If bank hasn't been created yet, map data def index map will get update when needed.
				EBStatic::RefreshMapDataDefIndexMap();

			sMapDataDefIndexMapNeedsInit = false;
		}

#if GRASS_BATCH_CS_CULLING
		if (PARAM_disableGrassComputeShader.Get())
		{
			sPerfEnableComputeShader = false;
		}
		if (PARAM_grassDisableRenderGeometry.Get())
		{
			sGrassRenderGeometry = false;
		}
#endif // GRASS_BATCH_CS_CULLING
	}
#endif //__BANK

	bool ResetHdEntityVisibilityForBatch(void *pItem, void *UNUSED_PARAM(data))
	{
		if(CEntityBatch *eb = static_cast<CEntityBatch *>(pItem))
		{
			const fwPropInstanceListDef *list = eb->GetInstanceList();
			fwMapDataDef *pDef = (eb->GetHdMapDataDefIndex() > -1 ? g_MapDataStore.GetSlot(strLocalIndex(eb->GetHdMapDataDefIndex())) : NULL);
			if(list && pDef && pDef->IsLoaded() && pDef->GetContents())
			{
				fwMapDataContents *contents = pDef->GetContents();
				fwEntity** entities = contents->GetEntities();
				u32 numEntities = contents->GetNumEntities();

				fwPropInstanceListDef::InstanceDataList::const_iterator end = list->m_InstanceList.end();
				fwPropInstanceListDef::InstanceDataList::const_iterator iter;
				for(iter = list->m_InstanceList.begin(); iter != end; ++iter)
				{
					if(iter->m_Index < numEntities)
					{
						if(CEntity *entity = static_cast<CEntity *>(entities[iter->m_Index]))
							entity->SetAddedToDrawListThisFrame(DL_RENDERPHASE_GBUF, false);
					}
				}
			}
		}

		return true;
	}

	void ResetHdEntityVisibility()
	{
		if(EBStatic::sResetLinkageVisibility)
			CEntityBatch::GetPool()->ForAll(EBStatic::ResetHdEntityVisibilityForBatch, NULL);
	}

	//Compute Shader
#if GRASS_BATCH_CS_CULLING
	inline const grcViewport *GetClippingVP()
	{
		const grcViewport *vp = dlCmdSetCurrentViewport::GetCurrentViewport_UpdateThread();
#if __BANK
		if(DRAWLISTMGR->IsBuildingGBufDrawList())
		{
			if(sGrassComputeShader_FreezeVp)
			{
				vp = &sFrozenVp;
			}
			else if(vp)
			{
				sFrozenVp = *vp;
			}
		}
#endif
		return vp;
	}
#endif //GRASS_BATCH_CS_CULLING

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	//Pre-allocated pool of grass append buffers.
	template<class T, u32 _InitialPreAllocatedCount = static_cast<u32>(1000 * grcInstanceBuffer::MaxFrames)>
	class PreAllocatedResourcePool
	{
	public:
		static const u32 sDefaultInitialPreAllocatedCount = _InitialPreAllocatedCount;
		u32 actualInitialPreAllocatedCount;
		typedef T value_type;
		typedef stlList<T> list_type;
		typedef typename list_type::size_type size_type;

		PreAllocatedResourcePool() : m_CurrFrame(fwTimer::GetSystemFrameCount()),actualInitialPreAllocatedCount(0)	{ }

		void PreAllocateResources(u32 count = sDefaultInitialPreAllocatedCount)
		{
			const T initVal;	//Let's just create/destroy 1 for the whole pre-allocate process.
			for(u32 i = 0; i < count; ++i)
			{
				m_FreeList.push_back(initVal);
				m_FreeList.back().PreCreate();
			}
			actualInitialPreAllocatedCount = count;
		}

		T *Allocate()
		{
			if(m_CurrFrame != fwTimer::GetSystemFrameCount())
				NextFrame();

			//Check if there are enough elements in the free list, and allocate overflow if there is not.
			if(m_FreeList.empty())
			{
				renderWarningf("%s: Pre-Allocated Resource List Is Empty! Allocating overflow resources for now, which can cause stalls. Please report this assert to graphics so we can bump the pre-allocated count. [Pre-Allocated Count = %d | Current Count = %d]", typeid(T).name(), actualInitialPreAllocatedCount,  GetAllocatedCount());
				m_FreeList.push_back(T());
				m_FreeList.back().Create(ASSERT_ONLY(false));
			}

			//Splice 1st free resource into current alloc list and return it.
			u32 currList = m_CurrFrame % static_cast<u32>(m_AllocList.GetMaxCount());
			m_AllocList[currList].splice(m_AllocList[currList].end(), m_FreeList, m_FreeList.begin());
			return &(m_AllocList[currList].back());
		}

		size_type GetAllocatedCount() const
		{
			return std::accumulate(m_AllocList.begin(), m_AllocList.end(), size_type(0), [](size_type count, const list_type &list){ return count + list.size(); });
		}

		size_type GetFreeCount() const
		{
			return m_FreeList.size();
		}

	private:
		list_type m_FreeList;
		atRangeArray<list_type, grcInstanceBuffer::MaxFrames> m_AllocList;
		u32 m_CurrFrame;

		void NextFrame()
		{
			u32 nextFrame = fwTimer::GetSystemFrameCount();
			u32 maxFrames = static_cast<u32>(m_AllocList.GetMaxCount());
			u32 framesElapsed = Min(nextFrame - m_CurrFrame, maxFrames);

			//Reap allocated resources.
			for(int i = 0; i < framesElapsed; ++i)
				m_FreeList.splice(m_FreeList.end(), m_AllocList[(m_CurrFrame + 1 + i) % maxFrames]);

			m_CurrFrame = nextFrame;
		}
	};

	typedef PreAllocatedResourcePool<PreAllocatedAppendBuffer> PreAllocatedAppendBufferPool;
	PreAllocatedAppendBufferPool sAppendBufferPool;

	typedef PreAllocatedResourcePool<PreAllocatedIndirectArgBuffer> PreAllocatedIndirectArgBufferPool;
	PreAllocatedIndirectArgBufferPool sIndirectArgBufferPool;

#if __BANK
	void DisplayAppendBufferAllocatorInfo(bool isProportional, int seperatorPixelHeight)
	{
		grcDebugDraw::AddDebugOutputEx(isProportional, "Append Buffer List:\tFree Count\tAllocated Count");
		grcDebugDraw::AddDebugOutputEx(isProportional, "                   \t %9d         %9d", sAppendBufferPool.GetFreeCount(), sAppendBufferPool.GetAllocatedCount());
		grcDebugDraw::AddDebugOutputSeparator(seperatorPixelHeight);
	}
#endif //__BANK
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
}

//////////////////////////////////////////////////////////////////////////////
//
// Entity Batch Base Implementation
//

template <class S, class T>
CEntityBatchBase<S, T>::CEntityBatchBase(const eEntityOwnedBy ownedBy)
: parent_type(ownedBy)
, m_InstanceList(NULL)
, m_MapDataDefIndex(-1)
{
	SetVisibilityType( VIS_ENTITY_INSTANCELIST );
}

template <class S, class T>
void CEntityBatchBase<S, T>::InitVisibilityFromDefinition(const typename CEntityBatchBase<S, T>::InstanceList *ASSERT_ONLY(definition), fwArchetype *archetype)
{
	if(archetype == NULL)
		return;

	CBaseModelInfo &modelInfo = *static_cast<CBaseModelInfo *>(archetype);
	fwFlags16 &visibilityMask = GetRenderPhaseVisibilityMask();

	// set archetype flags
	if(modelInfo.GetDontCastShadows())
		visibilityMask.ClearFlag(VIS_PHASE_MASK_SHADOWS);

	if(modelInfo.GetIsShadowProxy())
		visibilityMask.SetFlag(VIS_PHASE_MASK_SHADOW_PROXY);

	if(modelInfo.GetHasDrawableProxyForWaterReflections())
	{
		visibilityMask.SetFlag(VIS_PHASE_MASK_WATER_REFLECTION_PROXY);

#if __ASSERT
		if(!visibilityMask.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION))
			Assertf(0, "%s is a water reflection proxy but does not render in water reflection", definition->m_archetypeName.GetCStr());
#endif // __ASSERT
	}

#if WATER_REFLECTION_PRE_REFLECTED
	if(modelInfo.GetHasPreReflectedWaterProxy())
	{
		visibilityMask.SetFlag(VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED);
	}

#if __ASSERT
	if(visibilityMask.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED))
	{
		if(!visibilityMask.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION))
			Assertf(0, "%s has a pre-reflected water proxy but is not visible in water reflection", definition->m_archetypeName.GetCStr());
		if(!visibilityMask.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION_PROXY))
			Warningf("%s has a pre-reflected water proxy but is not actually a water reflection proxy", definition->m_archetypeName.GetCStr());
	}
#endif // __ASSERT
#endif // WATER_REFLECTION_PRE_REFLECTED
}

template <class S, class T>
void CEntityBatchBase<S, T>::InitBatchTransformFromDefinition(const typename CEntityBatchBase<S, T>::InstanceList *UNUSED_PARAM(definition), fwArchetype *archetype)
{
	//Note: In order to keep from making parent_type::GetAABB virtual, we are using the fact that it transforms AABB by the entity matrix. So here we need to compute 
	//a transform that transforms from the model AABB to the batch AABB.
	spdAABB modelAABB = archetype->GetBoundingBox();
	Vec3V modelCenter = modelAABB.GetCenter();
	Vec3V modelExtent = modelAABB.GetExtent();

	Vec3V batchCenter = m_InstanceList->m_BatchAABB.GetCenter();
	Vec3V batchExtent = m_InstanceList->m_BatchAABB.GetExtent();

	Vec3V scale = InvScale(batchExtent, modelExtent); //Shouldn't need InvScaleSafe since the model/batch AABB should never be 0/an invalid value.

	//Transform steps:
	//	1 - transform modelCenter to origin
	//	2 - Scale to batchExtent / modelExtent (scale to 1, then to batch extent)
	//	3 - transform position to batchCenter
	// Can condense 2 & 3

	Mat44V modelToOriginMat, scalePlusOriginToBatchMat, compositeMtx;
	Mat44VFromTranslation(modelToOriginMat, Vec4V(-modelCenter, ScalarV(V_ONE)));
	Mat44VFromScale(scalePlusOriginToBatchMat, scale, Vec4V(batchCenter, ScalarV(V_ONE)));
	Multiply(compositeMtx, scalePlusOriginToBatchMat, modelToOriginMat);

	//Create transform
	#if ENABLE_MATRIX_MEMBER
	Mat34V m = compositeMtx.GetMat34();	
	SetTransform(m);
	SetTransformScale(1.0f, 1.0f);
	#else
	fwMatrixTransform *trans = rage_new fwMatrixTransform(compositeMtx.GetMat34());
	SetTransform(trans);
	#endif
}

template <class S, class T>
void CEntityBatchBase<S, T>::SetModelId(fwModelId modelId)
{
	parent_type::SetModelId(modelId);
}

//////////////////////////////////////////////////////////////////////////////
//
// Entity Batch Implementation
//

CEntityBatch::CEntityBatch(const eEntityOwnedBy ownedBy)
: parent_type(ownedBy)
, m_Lod(0)
, m_HdMapDataDefIndex(-1)
{
	SetTypeInstanceList();
}

CEntityBatch::~CEntityBatch()
{
	GetInstanceList()->ReleaseDeviceResources();
}

#if __BANK
template <class S, class T>
s32 CEntityBatchBase<S, T>::GetNumInstances()
{
	return m_InstanceList->m_InstanceList.GetCount();
}
#endif

void CEntityBatch::InitEntityFromDefinition(const fwPropInstanceListDef *definition, fwArchetype *archetype, s32 mapDataDefIndex)
{
	if(	Verifyf(definition,	"WARNING: Can't Initialize Entity Batch from NULL definition!") &&
		Verifyf(archetype,	"WARNING: Can't Initialize Entity Batch from NULL archetype!")	)
	{
		//fwPropInstanceListDef parser data should be marked as retained, so it should be safe to just store off the pointer...
		//Storing off map data index as well, which can later be used for validation purposes.
		m_InstanceList = definition;
		m_MapDataDefIndex = mapDataDefIndex;

		//Initialize list's instance buffers
		m_InstanceList->CreateDeviceResources();

		//Cache off HD map data index
		fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(m_MapDataDefIndex));
		if(	Verifyf(pDef, "WARNING: CEntityBatch::InitEntityFromDefinition() - Trying to access owning mapdata (index: %d) but the slot does not appear to be valid!", m_MapDataDefIndex) &&
			Verifyf(pDef->GetContents(), "WARNING: CEntityBatch::InitEntityFromDefinition() - Trying to access owning mapdata contents (index: %d) but the contents is NULL!", m_MapDataDefIndex) &&
			Verifyf(pDef->GetContents()->GetLoaderInstRef().GetInstance(), "WARNING: CEntityBatch::InitEntityFromDefinition() - Trying to access owning mapdata ptr (index: %d) but it is NULL!", m_MapDataDefIndex)	)
		{
			fwMapDataContents *contents = pDef->GetContents();
			fwInstancedMapData &instList = reinterpret_cast<fwMapData *>(contents->GetLoaderInstRef().GetInstance())->m_instancedData;
			m_HdMapDataDefIndex = g_MapDataStore.FindSlotFromHashKey(instList.m_ImapLink.GetHash()).Get();
			Assertf(m_HdMapDataDefIndex >= 0, "WARNING! CEntityBatch::InitEntityFromDefinition() - Trying to cache of HD mapdata index for instance linking, but the system is unable to find"
					"mapdata %s (0x%x) in mapdata store. Linking will fail.", instList.m_ImapLink.TryGetCStr() ? instList.m_ImapLink.TryGetCStr() : "<Unknown hash>", instList.m_ImapLink.GetHash());
		}

		//Setup LOD distance
		SetLodDistance(EBStatic::sOverrideLodDist ? EBStatic::sOverriddenLodDistValue : m_InstanceList->m_lodDist); //Max LOD distance.

		InitVisibilityFromDefinition(definition, archetype);
		InitBatchTransformFromDefinition(definition, archetype);

		//Do this here in case drawable was already created and we missed the call to setup the AO scales.
		CalculateAmbientScales();
	}
}

fwDrawData *CEntityBatch::AllocateDrawHandler(rmcDrawable *pDrawable)
{
	//This is a convenient place to try and find the LOD to draw.
	if(pDrawable && pDrawable->HasInstancedGeometry())
	{
		for(int lod = 0; lod < LOD_COUNT; ++lod)
		{
			if(pDrawable->GetLodGroup().IsLodInstanced(lod, pDrawable->GetShaderGroup()))
			{
				m_Lod = lod;
				break;
			}
		}
	}

	return rage_new CEntityBatchDrawHandler(this, pDrawable);
}

void CEntityBatch::CalculateDynamicAmbientScales()
{
	if(m_InstanceList)	//Drawable can stream in before entity is initialized.
	{
		if(EBStatic::sComputeAoScalePerInstance)
		{
			fwInteriorLocation interiorLocation = GetInteriorLocation();
			InstanceList::InstanceDataList::const_iterator end = m_InstanceList->m_InstanceList.end();
			InstanceList::InstanceDataList::const_iterator iter;
			for(iter = m_InstanceList->m_InstanceList.begin(); iter != end; ++iter)
			{
				const Vec2V vAmbientScale = g_timeCycle.CalcAmbientScale(iter->GetPosition(), interiorLocation);
				Assertf(vAmbientScale.GetXf() >= 0.0f && vAmbientScale.GetXf() <= 1.0f,"Dynamic Natural Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetXf());
				Assertf(vAmbientScale.GetYf() >= 0.0f && vAmbientScale.GetYf() <= 1.0f,"Dynamic Artificial Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetYf());

				iter->SetGlobals(Vec4V(ScalarV(V_ONE), vAmbientScale.GetX(), vAmbientScale.GetY(), ScalarV(V_ONE)));
			}
		}
		else
		{
			parent_type::CalculateDynamicAmbientScales();
			Vec4V globals(1.0f, CShaderLib::DivideBy255(GetNaturalAmbientScale()), CShaderLib::DivideBy255(GetArtificialAmbientScale()), 1.0f);
			std::for_each(m_InstanceList->m_InstanceList.begin(), m_InstanceList->m_InstanceList.end(), std::bind2nd(std::mem_fun_ref(&InstanceList::InstanceData::SetGlobals), globals));
		}
	}
}

u32 CEntityBatch::UpdateLinkedInstanceVisibility() const
{
	u32 count = 0;
	if(m_InstanceList)
	{
		fwMapDataDef *pDef = (m_HdMapDataDefIndex > -1 ? g_MapDataStore.GetSlot(strLocalIndex(m_HdMapDataDefIndex)) : NULL);
		if(pDef && pDef->IsLoaded() && pDef->GetContents() && EBStatic::sEnableLinkageVisibility)
		{
			fwMapDataContents *contents = pDef->GetContents();
			fwEntity** entities = contents->GetEntities();
			u32 numEntities = contents->GetNumEntities();

			InstanceList::InstanceDataList::const_iterator end = m_InstanceList->m_InstanceList.end();
			InstanceList::InstanceDataList::const_iterator iter;
			for(iter = m_InstanceList->m_InstanceList.begin(); iter != end; ++iter)
			{
				if(iter->m_Index == 0xffffffff) //No linkage. Might not need to support later.
				{
					iter->m_IsVisible = true;
					++count;
				}
				else if(Verifyf(	iter->m_Index < numEntities, "WARNING! CEntityBatch::UpdateLinkedInstanceVisibility() - Instance %d has an invalid/out-of-bounds HD link index! [index = %d | max = %d]",
									(int)(iter - m_InstanceList->m_InstanceList.begin()), iter->m_Index, numEntities 	))
				{
					CEntity *entity = static_cast<CEntity *>(entities[iter->m_Index]);
					if(entity && entity->GetAddedToDrawListThisFrame(DL_RENDERPHASE_GBUF))
					{
						iter->m_IsVisible = false;
					}
					else
					{
						iter->m_IsVisible = true;
						++count;
					}
				}
				else
				{
					//What should we draw in the case of an invalid linkage? (This would really be an asset issue, and we'll have already asserted thanks to the verify above.)
					iter->m_IsVisible = true;
					++count;
				}
			}
		}
		else
		{
			std::for_each(m_InstanceList->m_InstanceList.begin(), m_InstanceList->m_InstanceList.end(), std::bind2nd(std::mem_fun_ref(&InstanceList::InstanceData::SetIsVisible), true));
			count = m_InstanceList->m_InstanceList.size();
		}
	}

	return count;
}

grcInstanceBuffer *CEntityBatch::CreateInstanceBuffer() const
{
	u32 numVisible = UpdateLinkedInstanceVisibility();

	if(m_InstanceList && numVisible > 0)
		return m_InstanceList->CreateInstanceBuffer();

	return NULL;
}

void CEntityBatch::ResetHdEntityVisibility()
{
	EBStatic::ResetHdEntityVisibility();
}

#if __BANK
void CEntityBatch::RenderDebug()
{
	CCustomShaderEffectGrass::RenderDebug();
	CEntityBatchDrawHandler::RenderDebug();
	EBStatic::RenderDebug();
}

void CEntityBatch::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Instance Lists", false);
	{
		CEntityBatchDrawHandler::AddWidgets(bank);

		//Debug Draw
		EBStatic::AddWidgets(bank);
	}
	bank.PopGroup();
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Grass Batch Implementation
//
#include "streaming/streamingengine.h"

#define USE_CONSTANT_BUFFER (__D3D11)

namespace EBStatic
{
	struct SharedVertex
	{
		Mat33V Matrix;
	};

#if RSG_PC && USE_CONSTANT_BUFFER
	static grcCBuffer *sCB = NULL;
#elif RSG_DURANGO
	static char *sSharedData = NULL;
#elif RSG_ORBIS
	static void *sSharedData = NULL;
	static sce::Gnm::Buffer sCB;
#else
	static grcVertexBuffer *sSharedVerts = NULL;
	static grcFvf sSharedVertFvf;
	static const u32 sSharedVertStream = 2;
#endif

	//PS3 Rendering implementation relies on GRASS_BATCH_SHARED_DATA_COUNT to match the number of times the instance buffer was duplicated in ragebuilder.
	PS3_ONLY(CompileTimeAssert(grcInstanceBuffer::MaxPerDraw == GRASS_BATCH_SHARED_DATA_COUNT));

#if (RSG_PC && USE_CONSTANT_BUFFER) || RSG_DURANGO || RSG_ORBIS
	//Make sure our buffers are the expected size from the shader!
	static const u32 sSharedCBSize = sizeof(SharedVertex[GRASS_BATCH_SHARED_DATA_COUNT]);
	CompileTimeAssert(sSharedCBSize == GRASS_BATCH_NUM_CONSTS_PER_INSTANCE * GRASS_BATCH_SHARED_DATA_COUNT * 16);
#endif

	bool CreateSharedBuffer()
	{
		bool success = true;

#if (RSG_PC && USE_CONSTANT_BUFFER) || RSG_DURANGO || RSG_ORBIS
		SharedVertex *sharedData = NULL;

	#if RSG_PC && USE_CONSTANT_BUFFER
		u16 registers[NONE_TYPE] = { 0 };
		registers[VS_TYPE] = (u16)INSTANCE_CONSTANT_BUFFER_SLOT;

		sCB = rage_new rage::grcCBuffer(sSharedCBSize, false);
		sCB->SetRegisters(registers);
		GRCDEVICE.LockContext();
		sharedData = static_cast<SharedVertex *>(sCB->BeginUpdate(sSharedCBSize));
	#elif RSG_DURANGO
		sSharedData = rage_aligned_new(16) char[sSharedCBSize];
		sharedData = (SharedVertex*)sSharedData;
	#elif RSG_ORBIS
		sSharedData = allocateVideoSharedMemory(sSharedCBSize, 16);
		sCB.initAsConstantBuffer(sSharedData, sSharedCBSize);
		sharedData = static_cast<SharedVertex *>(sSharedData);
	#endif

		if(Verifyf(sharedData, "Failed to get buffer lock to write out shared data?"))
		{
			for(u32 i = 0; i < GRASS_BATCH_SHARED_DATA_COUNT; ++i)
			{
				//Setup randomized rotation
				static const ScalarV angleStep = ScalarV(V_TWO_PI) / ScalarV(static_cast<float>(GRASS_BATCH_SHARED_DATA_COUNT));
				const ScalarV angle = angleStep * ScalarV(static_cast<float>(i));
				Mat33VFromAxisAngle(sharedData[i].Matrix, Vec3V(V_Z_AXIS_WZERO), angle);

				//Setup randomized scale
				static const ScalarV scaleStep = ScalarV(V_ONE) / ScalarV(static_cast<float>(GRASS_BATCH_SHARED_DATA_COUNT - 1));
				sharedData[i].Matrix.GetCol0Ref().SetW(scaleStep * ScalarV(static_cast<float>(i)));
			}
		}
		else
			success = false;

	#if RSG_PC && USE_CONSTANT_BUFFER
		sCB->EndUpdate();
		GRCDEVICE.UnlockContext();
	#endif
#else
		sSharedVertFvf.ClearAllChannels();
		sSharedVertFvf.SetTextureChannel(2, true, grcFvf::grcdsFloat4);
		sSharedVertFvf.SetTextureChannel(3, true, grcFvf::grcdsFloat4);
		sSharedVertFvf.SetTextureChannel(4, true, grcFvf::grcdsFloat4);

		const bool createReadWrite = true;
		sSharedVerts = grcVertexBuffer::Create(GRASS_BATCH_SHARED_DATA_COUNT, sSharedVertFvf, createReadWrite, false, NULL);
		if(sSharedVerts)
		{
			{
				//not resourcing this currently
				PS3_ONLY(g_AllowVertexBufferVramLocks++);

				grcVertexBuffer::LockHelper lock(sSharedVerts);
				if(SharedVertex *vertLock = reinterpret_cast<SharedVertex *>(lock.GetLockPtr()))
				{
					for(u32 i = 0; i < GRASS_BATCH_SHARED_DATA_COUNT; ++i)
					{
						//Setup randomized rotation
						static const ScalarV angleStep = ScalarV(V_TWO_PI) / ScalarV(static_cast<float>(GRASS_BATCH_SHARED_DATA_COUNT));
						const ScalarV angle = angleStep * ScalarV(static_cast<float>(i));
						Mat33VFromAxisAngle(vertLock[i].Matrix, Vec3V(V_Z_AXIS_WZERO), angle);

						//Setup randomized scale
						static const ScalarV scaleStep = ScalarV(V_ONE) / ScalarV(static_cast<float>(GRASS_BATCH_SHARED_DATA_COUNT - 1));
						vertLock[i].Matrix.GetCol0Ref().SetW(scaleStep * ScalarV(static_cast<float>(i)));
					}
				}

				PS3_ONLY(g_AllowVertexBufferVramLocks--);
			}

			sSharedVerts->MakeReadOnly();
		}
		else
			success = false;
#endif

		return success;
	}

	void DestorySharedBuffer()
	{
#if RSG_PC && USE_CONSTANT_BUFFER
		delete sCB;
		sCB = NULL;
#elif RSG_DURANGO
		delete[] sSharedData;
		sSharedData = NULL;
#elif RSG_ORBIS
		sCB = sce::Gnm::Buffer(); //Is this the best way to reset a sce::Gnm::Buffer??
		freeVideoSharedMemory(sSharedData);
		sSharedData = NULL;
#else
		delete sSharedVerts;
#endif
	}

	bool BindSharedBuffer()
	{
		bool success = true;

#if RSG_PC && USE_CONSTANT_BUFFER
		if(grcCBuffer *pCBuffer = sCB)
		{
			ID3D11Buffer *pD3DBuffer = pCBuffer->GetBuffer();
			g_grcCurrentContext->VSSetConstantBuffers(INSTANCE_CONSTANT_BUFFER_SLOT, 1, &pD3DBuffer);
			g_grcCurrentContext->CSSetConstantBuffers(INSTANCE_CONSTANT_BUFFER_SLOT, 1, &pD3DBuffer);
		}
		else
			success = false;
#elif RSG_DURANGO
		grcContextHandle *const ctx = g_grcCurrentContext;
		grcDevice::SetConstantBuffer<VS_TYPE>(ctx, INSTANCE_CONSTANT_BUFFER_SLOT, sSharedData);
		grcDevice::SetConstantBuffer<CS_TYPE>(ctx, INSTANCE_CONSTANT_BUFFER_SLOT, sSharedData);
#elif RSG_ORBIS
		if(grcVertexProgram::GetCurrent())
			gfxc.setConstantBuffers(grcVertexProgram::GetCurrent()->GetGnmStage(), INSTANCE_CONSTANT_BUFFER_SLOT, 1, &sCB);

		gfxc.setConstantBuffers(sce::Gnm::kShaderStageCs, INSTANCE_CONSTANT_BUFFER_SLOT, 1, &sCB);
#else
		GRCDEVICE.SetStreamSource(sSharedVertStream, *sSharedVerts, 0, sSharedVerts->GetVertexStride());
		success = (sSharedVerts != NULL);
#endif

		return success;
	}

	void SetGrassBatchVertexDeclarations(rmcDrawable *pDrawable)
	{
		if(pDrawable)
		{
			rmcLodGroup &lodGrp = pDrawable->GetLodGroup();

			for(int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
			{
				if(lodGrp.ContainsLod(lodIdx))
				{
					const rmcLod &lod = lodGrp.GetLod(lodIdx);
					for(int i = 0; i < lod.GetCount(); ++i)
					{
						if(grmModel *model = lod.GetModel(i))
						{
							int count = model->GetGeometryCount();
							for(int index = 0; index < count; ++index)
							{
// 							#if __ASSERT
// 								static const char *sGrassBatchShaderName = "grass_batch";
// 								const grmShaderGroup &shaderGroup = pDrawable->GetShaderGroup();
// 								const int shaderIndex = model->GetShaderIndex(index);
// 								Assertf(!stricmp(sGrassBatchShaderName, shaderGroup.GetShader(shaderIndex).GetName()),
// 										"WARNING: Found non grass_batch shader \"%s\" in grass model \"%s\" LOD chain! [LOD: %d]",
// 										shaderGroup.GetShader(shaderIndex).GetName(), pDrawable->GetDebugName(NULL, 0), lodIdx);
// 							#endif

								grmGeometry &geometry = model->GetGeometry(index);
								grcVertexDeclaration *decl = NULL;
							#if __XENON
								if(const grcVertexBuffer *indexVerts = geometry.GetVertexBufferByIndex(3))
								{
									grcFvf instFvf = CGrassBatch::InstanceList::GetFVF();
									decl = grmModelFactory::BuildDeclarator(geometry.GetFvf(), &instFvf, &sSharedVertFvf, indexVerts->GetFvf());
								}
							#else //__XENON
								PS3_ONLY(u32 indexCount = geometry.GetIndexCount());
								typedef atRangeArray<grcVertexElement, grmModelFactory::MaxVertElements> ElementArray;
								ElementArray elements;
								atRangeArray<int, grcVertexElement::grcvetCount> elemCounts;
								grcFvf *sharedFvf = 
								#if (RSG_PC && USE_CONSTANT_BUFFER) || RSG_DURANGO || RSG_ORBIS
									NULL;
								#else
									&sSharedVertFvf;
								#endif
								#if GRASS_BATCH_CS_CULLING
									grcFvf *pInstFvf = NULL;
								#else //GRASS_BATCH_CS_CULLING
									grcFvf instFvf = CGrassBatch::InstanceList::GetFVF();
									grcFvf *pInstFvf = &instFvf;
								#endif //GRASS_BATCH_CS_CULLING
								int numElem = grmModelFactory::ComputeVertexElements(geometry.GetFvf(), pInstFvf, sharedFvf, NULL, elements, elemCounts);

								//Need to add appropriate instance modes & divide values before building the final declarator
								for(u32 elem = 0; elem < numElem; ++elem)
								{
								#if __PS3
									elements[elem].streamFrequencyMode = (elements[elem].stream != 0) ? grcFvf::grcsfmDivide : grcFvf::grcsfmModulo;
									elements[elem].streamFrequencyDivider = indexCount;
								#else //D3D11 || RSG_ORBIS
									if(elements[elem].stream != 0)
									{
										elements[elem].streamFrequencyMode = grcFvf::grcsfmIsInstanceStream;
										elements[elem].streamFrequencyDivider = 1;
									}
								#endif
								}

								decl = grmModelFactory::BuildCachedDeclarator(geometry.GetFvf(), pInstFvf, sharedFvf, NULL, elements, numElem);
							#endif //__XENON

								geometry.SetCustomDecl(decl);
							}
						}
					}
				}
			}
		}
	}

#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
	u32 ComputeIdirectArgCount(const rmcDrawable *drawable, atRangeArray<u32, LOD_COUNT> &lodOffsets)
	{
		u32 count = 0;
		if(drawable)
		{
			const rmcLodGroup &lodGrp = drawable->GetLodGroup();
			for(int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
			{
				lodOffsets[lodIdx] = count;
				if(lodGrp.ContainsLod(lodIdx))
				{
					const rmcLod &lod = lodGrp.GetLod(lodIdx);
					for(int modelIdx = 0; modelIdx < lod.GetCount(); ++modelIdx)
					{
						const grmModel *model = lod.GetModel(modelIdx);
						if(model)
						{
							count += static_cast<u32>(model->GetGeometryCount());
						}
					}
				}
			}
		}

		return count;
	}
#endif
}

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
PF_PAGE(CGrassBatch, "Grass Batch profiling");
PF_GROUP(IndirectArgCritSec);
PF_LINK(CGrassBatch, IndirectArgCritSec);
PF_COUNTER_CUMULATIVE_FLOAT(AddToMapCritSecTimeMs, IndirectArgCritSec);
PF_COUNTER_CUMULATIVE_FLOAT(RemoveFromMapCritSecTimeMs, IndirectArgCritSec);
PF_COUNTER_CUMULATIVE_FLOAT(PlaceDrawableCritSecTimeMs, IndirectArgCritSec);
PF_COUNTER_CUMULATIVE_FLOAT(CumulativeCritSecTimeMs, IndirectArgCritSec);

PF_GROUP(DeviceResourceCreation);
PF_LINK(CGrassBatch, DeviceResourceCreation);
PF_COUNTER_CUMULATIVE_FLOAT(IndirectArgBuffersSec, DeviceResourceCreation);
PF_COUNTER_CUMULATIVE_FLOAT(StructuredBuffersSec, DeviceResourceCreation);
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

namespace EBStatic
{
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	DrawableDeviceResources::DrawableDeviceResources(rmcDrawable &drawable)
	: m_IndirectArgsBuffer(ORBIS_ONLY(grcBuffer_Raw, true))
	{
		u32 indirectArgCount = 0;
		rmcLodGroup &lodGrp = drawable.GetLodGroup();
		for(int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
		{
			if(lodGrp.ContainsLod(lodIdx) && lodGrp.GetLod(lodIdx).GetCount() > 0)
			{
				rmcLod &lod = lodGrp.GetLod(lodIdx);
				m_OffsetMap[lodIdx].Resize(lod.GetCount());
				for(int modelIdx = 0; modelIdx < lod.GetCount(); ++modelIdx)
				{
					if(grmModel *model = lod.GetModel(modelIdx))
					{
						m_OffsetMap[lodIdx][modelIdx].Resize(model->GetGeometryCount());
						for(int geoIdx = 0; geoIdx < model->GetGeometryCount(); ++geoIdx)
							m_OffsetMap[lodIdx][modelIdx][geoIdx] = sizeof(IndirectArgParams) * indirectArgCount++;
					}
				}
			}
		}

		//Now we know indirect arg count... we need an initial value buffer.
		IndirectArgParams *bufferInit = rage_aligned_new(16) IndirectArgParams[indirectArgCount];
		u32 currentArg = 0;
		for(int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
		{
			if(lodGrp.ContainsLod(lodIdx) && lodGrp.GetLod(lodIdx).GetCount() > 0)
			{
				rmcLod &lod = lodGrp.GetLod(lodIdx);
				for(int modelIdx = 0; modelIdx < lod.GetCount(); ++modelIdx)
				{
					if(grmModel *model = lod.GetModel(modelIdx))
					{
						for(int geoIdx = 0; geoIdx < model->GetGeometryCount(); ++geoIdx)
							bufferInit[currentArg++].m_indexCountPerInstance = model->GetGeometry(geoIdx).GetIndexCount();
					}
				}
			}
		}
		Assert(indirectArgCount == currentArg); //Make sure our counts match!

		//Lastly, create the device resource.
	#if RSG_ORBIS
		u32 indirectArgSize = sizeof(IndirectArgParams) * indirectArgCount;
		const sce::Gnm::SizeAlign indirectSizeAlign = { indirectArgSize, sce::Gnm::kAlignmentOfIndirectArgsInBytes };
		m_IndirectArgsBuffer.Allocate(indirectSizeAlign, true, bufferInit);
	#elif RSG_DURANGO
		u32 indirectArgSize = sizeof(IndirectArgParams) * indirectArgCount;
		GRCDEVICE.LockContext();
		m_IndirectArgsBuffer.Initialise(indirectArgSize, 1, grcBindShaderResource, NULL, grcBufferMisc_DrawIndirectArgs);
		u32 *pDest = static_cast<u32 *>(uav.Lock(grcsWrite, 0, 0, NULL));
		memcpy(pDest, bufferInit, indirectArgSize);
		uav.Unlock(grcsWrite);
		GRCDEVICE.UnlockContext();
	#else // RSG_PC
		m_IndirectArgsBuffer.Initialise(indirectArgCount, sizeof(IndirectArgParams), grcBindNone, grcsBufferCreate_ReadWrite, grcsBufferSync_None, bufferInit, false, grcBufferMisc_DrawIndirectArgs);
	#endif

		delete[] bufferInit;
	}

	void PlaceDrawableIndirectArgs(strLocalIndex drawableIdx, rmcDrawable *drawable)
	{
		//Only add indirect args if the drawable has a compute shader?
		bool hasComputeShader = false;
		if(drawable)
		{
			grmShaderGroup &shaderGrp = drawable->GetShaderGroup();
			for(int i = 0; i < shaderGrp.GetCount(); ++i)
			{
				grmShader &shader = shaderGrp.GetShader(i);
				if(shader.GetComputeProgram(0) != NULL)
				{
					hasComputeShader = true;
					break;
				}
			}
		}

		if(!hasComputeShader)
			return;

		fwDrawableDef *drawableDef = g_DrawableStore.GetSlot(drawableIdx);
		if(Verifyf(drawableDef, "Invalid drawable index being placed??") && Verifyf(drawable, "NULL drawable was just placed??"))
		{
			if(drawableDef->m_deviceResources)
				return; //Already been created previously? Should always be correct once created... so just return.

			DrawableDeviceResources *drawableDR = rage_new DrawableDeviceResources(*drawable);
			drawableDef->m_deviceResources = reinterpret_cast<void *>(drawableDR);
		}
	}

	void RemoveDrawableIndirectArgs(strLocalIndex drawableIdx)
	{
		fwDrawableDef *drawableDef = g_DrawableStore.GetSlot(drawableIdx);
		if(Verifyf(drawableDef, "Invalid drawable index being removed??"))
		{
			delete reinterpret_cast<EBStatic::DrawableDeviceResources *>(drawableDef->m_deviceResources);
			drawableDef->m_deviceResources = NULL;
		}
	}
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

	void *CreateDeviceResources(fwGrassInstanceListDef &batch)
	{
		return rage_new CGrassBatch::DeviceResources(batch);
	}

	void ReleaseDeviceResources(void *dr)
	{
		CGrassBatch::DeviceResources *deviceRsc = static_cast<CGrassBatch::DeviceResources *>(dr);
		delete deviceRsc;
	}
}

CGrassBatch::DeviceResources::DeviceResources(fwGrassInstanceListDef &batch)
#if GRASS_BATCH_CS_CULLING
: m_RawInstBuffer(ORBIS_ONLY(grcBuffer_Structured, false))
#else //GRASS_BATCH_CS_CULLING
: m_Verts(NULL)
#endif //GRASS_BATCH_CS_CULLING
{
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(STATS_ONLY(sysTimer indirectArgTimer));
#if GRASS_BATCH_CS_CULLING
# if RSG_ORBIS
	m_RawInstBuffer.Initialize(static_cast<void *>(batch.m_InstanceList.GetElements()), batch.m_InstanceList.GetCount(), false, sizeof(CGrassBatch::InstanceList::InstanceData));
# elif RSG_DURANGO
	//GRCDEVICE.LockContext(); //No longer necessary?
	m_RawInstBuffer.Initialise(batch.m_InstanceList.GetCount(), sizeof(CGrassBatch::InstanceList::InstanceData), grcBindShaderResource|grcBindUnorderedAccess, static_cast<void *>(batch.m_InstanceList.GetElements()), grcBufferMisc_BufferStructured);
	//GRCDEVICE.UnlockContext();
# else // RSG_PC
	if(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50))
		m_RawInstBuffer.Initialise(batch.m_InstanceList.GetCount(), sizeof(CGrassBatch::InstanceList::InstanceData), grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWriteOnceOnly, grcsBufferSync_None, static_cast<void *>(batch.m_InstanceList.GetElements()), false, grcBufferMisc_BufferStructured);
# endif
#else //GRASS_BATCH_CS_CULLING
	const bool bReadWrite = false;
	m_Verts = grcVertexBuffer::Create(batch.m_InstanceList.size(), fwGrassInstanceListDef::GetFVF(), bReadWrite, false, static_cast<void *>(batch.m_InstanceList.GetElements()));
#endif //GRASS_BATCH_CS_CULLING
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(PF_INCREMENTBY(StructuredBuffersSec, indirectArgTimer.GetTime()));
}

CGrassBatch::DeviceResources::~DeviceResources()
{
#if GRASS_BATCH_CS_CULLING
#else
	delete m_Verts;
	m_Verts = NULL;
#endif
}

CGrassBatch::CGrassBatch(const eEntityOwnedBy ownedBy)
: parent_type(ownedBy)
, m_DistToCamera(0.0f)
{
	SetTypeGrassInstanceList();
}

CGrassBatch::~CGrassBatch()
{
	GetInstanceList()->ReleaseDeviceResources();
}

void CGrassBatch::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
#if RSG_PC
		if(CGrassBatch::IsRenderingEnabled())
#endif
		{
			//Setup shared vertex data
			ASSERT_ONLY(bool success = ) EBStatic::CreateSharedBuffer();
			Assertf(success, "Failed to create grass shared buffer!");

			fwGrassInstanceListDef::Init(&EBStatic::CreateDeviceResources, &EBStatic::ReleaseDeviceResources);
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
			if(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50))
			{
				const float maxLodScale = CSettingsManager::GetInstance().GetSettings().m_graphics.m_MaxLodScale;
				const float delta = maxLodScale; // Max Lod Scale goes from 0.0f to 1.0f
				int appendBufferPreAllocateCount = Lerp(delta,(int)EBStatic::PreAllocatedAppendBufferPool::sDefaultInitialPreAllocatedCount,4750);
				int sIndirectBufferPreAllocateCount = Lerp(delta,(int)EBStatic::PreAllocatedAppendBufferPool::sDefaultInitialPreAllocatedCount,4250);

				fwGrassInstanceListDef::SetDrawablePlacementNotifyFuncs(&EBStatic::PlaceDrawableIndirectArgs, & EBStatic::RemoveDrawableIndirectArgs);
				EBStatic::sAppendBufferPool.PreAllocateResources(appendBufferPreAllocateCount);
				GRCDEVICE.LockContext();
				EBStatic::sIndirectArgBufferPool.PreAllocateResources(sIndirectBufferPreAllocateCount);
				GRCDEVICE.UnlockContext();
			}
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
		}
	}

	BANK_ONLY(EBStatic::Init(initMode));
}

void CGrassBatch::Shutdown()
{
	EBStatic::DestorySharedBuffer();
}

////////////////////////////////////////////////////////////////////////////////
//								Grass Settings
//
// Custom:	No grass rendering at all.
// Low:		Grass renders only in gbuffer (w/ low lod?)
// Medium:	Grass renders gbuffer/shadow pass. (w/ low lod in shadows?)
// High:	Normal grass rendering.
// Ultra:	High + Force shadows on for all grass and set some min fade distance
// 
////////////////////////////////////////////////////////////////////////////////
void CGrassBatch::SetQuality(CSettings::eSettingsLevel quality)
{
	EBStatic::sQuality = quality;

	switch(quality)
	{
	case CSettings::Low: // No Grass
		{
			CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(false);
			g_LodScale.SetGrassQualityScale(1.0f);
			break;
		}
	case CSettings::Medium: // No Grass in shadows
		{
			CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(false);
			g_LodScale.SetGrassQualityScale(1.0f);
			break;
		}
	case CSettings::High: // Some Grass in shadows
		{
			CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(true);
			g_LodScale.SetGrassQualityScale(1.0f);
			break;
		}
	case CSettings::Ultra: // All the grass everywhere
		{
			CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(true);
			g_LodScale.SetGrassQualityScale(1.6f);
			break;
		}
	default:
		{
			CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(false);
			g_LodScale.SetGrassQualityScale(1.0f);
			break;
		}
	}
}

bool CGrassBatch::IsRenderingEnabled()
{
	return (EBStatic::sQuality != CSettings::Low) && (GRCDEVICE.GetDxFeatureLevel() >= 1100);
}

int CGrassBatch::GetSharedInstanceCount()
{
	return GRASS_BATCH_SHARED_DATA_COUNT;
}

bool CGrassBatch::BindSharedBuffer()
{
	return EBStatic::BindSharedBuffer();
}

void CGrassBatch::InitEntityFromDefinition(fwGrassInstanceListDef *definition, fwArchetype *archetype, s32 mapDataDefIndex)
{
	if(	Verifyf(definition,	"WARNING: Can't Initialize Entity Batch from NULL definition!") &&
		Verifyf(archetype,	"WARNING: Can't Initialize Entity Batch from NULL archetype!")	)
	{
		//fwGrassInstanceListDef parser data should be marked as retained, so it should be safe to just store off the pointer...
		//Storing off map data index as well, which can later be used for validation purposes.
		m_InstanceList = definition;
		m_MapDataDefIndex = mapDataDefIndex;

#if GRASS_BATCH_CS_CULLING
		//Setup LOD distance - Must be large enough to cover LOD fade for farthest element.
		ScalarV minLodDist = Mag(definition->m_BatchAABB.GetExtent());
		float lodDist = minLodDist.Getf() + m_InstanceList->m_lodDist;
#endif //#if GRASS_BATCH_CS_CULLING
		SetLodDistance(EBStatic::sOverrideLodDist ? EBStatic::sOverriddenLodDistValue : GRASS_BATCH_CS_CULLING_SWITCH(static_cast<u32>(lodDist), m_InstanceList->m_lodDist)); //Max LOD distance.

		InitVisibilityFromDefinition(definition, archetype);
		InitBatchTransformFromDefinition(definition, archetype);

		if(EBStatic::sQuality == CSettings::Ultra)
		{
			fwFlags16 &visibilityMask = GetRenderPhaseVisibilityMask();
			visibilityMask.SetFlag(VIS_PHASE_MASK_SHADOWS);
		}

		if(m_InstanceList->GetDeviceResources() == NULL)
		{
#if !__GAMETOOL
			Warningf("Grass data needs to be rebuilt to avoid stalls on PC");
#endif
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("CreateDeviceResources", 0.4f);
			m_InstanceList->CreateDeviceResources();
		}

	}
}

ePrerenderStatus CGrassBatch::PreRender(const bool bIsVisibleInMainViewport)
{
	//Must compute distance to camera before calling parent's pre-render! This value is used by the CSE's update.
	spdAABB box;
	GetAABB(box);
	Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	m_DistToCamera = box.DistanceToPoint(camPos).Getf();

	return parent_type::PreRender(bIsVisibleInMainViewport);
}

fwDrawData *CGrassBatch::AllocateDrawHandler(rmcDrawable *pDrawable)
{
	//Good place to setup the vertex declaration?
	EBStatic::SetGrassBatchVertexDeclarations(pDrawable);

#if GRASS_BATCH_CS_CULLING
	//Reset cs params?
	m_CSParams = EBStatic::GrassCSBaseParams();

	m_CSParams.m_InstanceCount = m_InstanceList->m_InstanceList.GetCount();
	m_CSParams.m_InstanceList = GetInstanceList();
	GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(m_CSParams.m_IndirectArgCount = EBStatic::ComputeIdirectArgCount(pDrawable , m_CSParams.m_IndirectArgLodOffsets));

	CBaseModelInfo *pModelInfo = GetBaseModelInfo();
	CCustomShaderEffectBaseType *cseType = pModelInfo ? pModelInfo->GetCustomShaderEffectType() : NULL;
	if(	Verifyf(cseType, "Could not get CSE type! Entity Name: %s | Drawable Name: %s | pModelInfo = 0x%p", GetModelName(), pDrawable ? pDrawable->GetDebugName(NULL, 0) : "<NULL Drawable>", pModelInfo) && 
		Verifyf(cseType->GetType() == CSE_GRASS, "CSE type is not grass! Entity Name: %s | Drawable Name: %s | type = %d", GetModelName(), pDrawable ? pDrawable->GetDebugName(NULL, 0) : "<NULL Drawable>", cseType->GetType()))
	{
		m_CSParams.m_Vars = &(static_cast<CCustomShaderEffectGrassType *>(cseType)->GetCSVars());
		artAssertf(m_CSParams.m_Vars->m_ShaderIndex != -1, "Batch drawable found that does not appear to have a valid batch shader! Batch will not render! Entity Name: %s | Drawable Name: %s", GetModelName(), pDrawable ? pDrawable->GetDebugName(NULL, 0) : "<NULL Drawable>");
	}

	if(GetArchetype())
	{
		const spdAABB &instAabb = GetArchetype()->GetBoundingBox();
		m_CSParams.m_InstAabbRadius = Mag(instAabb.GetMax() - instAabb.GetMin()).Getf();
	}

	const DeviceResources *dr = GetDeviceResources();
	m_CSParams.m_RawInstBuffer = dr ? &(dr->m_RawInstBuffer) : NULL;

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	strLocalIndex drawableIndex;
	if(const fwArchetype *archetype = GetArchetype())
	{
		switch(archetype->GetDrawableType())
		{
		case fwArchetype::DT_DRAWABLE:
			drawableIndex = strLocalIndex(archetype->GetDrawableIndex());
			break;

		case fwArchetype::DT_FRAGMENT:
		case fwArchetype::DT_DRAWABLEDICTIONARY:
		default:
			Assertf(false, "WARNING: Grass batch drawable is an unsupported type: %d", archetype->GetDrawableType());
			break;
		}
	}

	if(drawableIndex.IsValid())
	{
		if(fwDrawableDef *drawableDef = g_DrawableStore.GetSlot(drawableIndex))
		{
			if(EBStatic::DrawableDeviceResources *ddr = reinterpret_cast<EBStatic::DrawableDeviceResources *>(drawableDef->m_deviceResources))
			{
				m_CSParams.m_IndirectArgsBuffer = &(ddr->m_IndirectArgsBuffer);
				m_CSParams.m_OffsetMap = &(ddr->m_OffsetMap);
			}
		}
	}
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

	PIX_TAGGING_ONLY(m_CSParams.m_ModelIndex = GetModelIndex());

	m_CSParams.UpdateValidity();
	Assert(m_CSParams.IsValid() WIN32PC_ONLY(|| !GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50)));
#endif //GRASS_BATCH_CS_CULLING

	CGrassBatchDrawHandler *dh = rage_new CGrassBatchDrawHandler(this, pDrawable);
	return dh;
}

u32 CGrassBatch::ComputeLod(float dist) const
{
	u32 LODIdx = 0;
	if(CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(GetModelIndex()))))
	{
		if(rmcDrawable *pDrawable = pModelInfo->GetDrawable())
		{
			const bool bUseAltfadeDist = GetUseAltFadeDistance();
			LODDrawable::crossFadeDistanceIdx fadeDist = bUseAltfadeDist ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;
			u32 phaseLODs = GetPhaseLODs();
			u32 lastLODIdx = GetLastLODIdx();
			int crossfade = -1;
			u32 LODFlag = (phaseLODs >> dlCmdNewDrawList::GetCurrDrawListLODFlagShift_UpdateThread()) & LODDrawable::LODFLAG_MASK;
			const bool isShadowDrawList = DRAWLISTMGR->IsBuildingShadowDrawList();

			if (LODFlag != LODDrawable::LODFLAG_NONE)
			{
				if (LODFlag & LODDrawable::LODFLAG_FORCE_LOD_LEVEL)
				{
					LODIdx = LODFlag & ~LODDrawable::LODFLAG_FORCE_LOD_LEVEL;
				}
				else
				{
					LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(pDrawable, dist, LODFlag, LODIdx, crossfade, fadeDist, lastLODIdx, g_LodScale.GetGrassBatchScale());
				}
			}
			else
			{
				LODDrawable::CalcLODLevelAndCrossfade(pDrawable, dist, LODIdx, crossfade, fadeDist, lastLODIdx ASSERT_ONLY(, pModelInfo), g_LodScale.GetGrassBatchScale());
			}

			if((LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList) && crossfade > -1)
			{
				if(crossfade < 128 && LODIdx + 1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODIdx + 1))
					LODIdx = LODIdx + 1;
			}
		}
	}

	return LODIdx;
}

float CGrassBatch::GetShadowLODFactor()
{
	return EBStatic::sShadowLodFactor;
}

#if GRASS_BATCH_CS_CULLING
const u32 EBStatic::IndirectArgParams::sIndexCountPerInstanceOffset = offsetof(EBStatic::IndirectArgParams, m_indexCountPerInstance);		//Setup correct offset for index count per instance arg.
const u32 EBStatic::IndirectArgParams::sInstanceCountOffset = offsetof(EBStatic::IndirectArgParams, m_instanceCount);						//Setup correct offset for instance count arg.
const u32 EBStatic::IndirectArgParams::sStartIndexLocationOffset = offsetof(EBStatic::IndirectArgParams, m_startIndexLocation);
const u32 EBStatic::IndirectArgParams::sBaseVertexLocationOffset = offsetof(EBStatic::IndirectArgParams, m_baseVertexLocation);
const u32 EBStatic::IndirectArgParams::sStartInstanceLocationOffset = offsetof(EBStatic::IndirectArgParams, m_startInstanceLocation);

EBStatic::GrassCSVars::GrassCSVars()
: m_ShaderIndex(-1)
, m_idVarAppendInstBuffer(grcevNONE)
DURANGO_ONLY(GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(, m_idIndirectArgs(grcevNONE)))
, m_idInstCullParams(grcevNONE)
, m_idNumClipPlanes(grcevNONE)
, m_idClipPlanes(grcevNONE)
, m_idCameraPosition(grcevNONE)
, m_idLodInstantTransition(grcevNONE)
DURANGO_ONLY(, m_idGrassSkipInstance(grcevNONE))
WIN32PC_ONLY(, m_idGrassSkipInstance(grcevNONE))
, m_idLodThresholds(grcevNONE)
, m_idCrossFadeDistance(grcevNONE)
, m_idIsShadowPass(grcevNONE)
, m_idLodFadeControlRange(grcevNONE)
DURANGO_ONLY(, m_idIndirectCountPerLod(grcevNONE))
, m_idVarInstanceBuffer(grmsgvNONE)
, m_idVarRawInstBuffer(grmsgvNONE)
, m_idVarUseCSOutputBuffer(grmsgvNONE)
, m_idAabbMin(grmsgvNONE)
, m_idAabbDelta(grmsgvNONE)
, m_idScaleRange(grmsgvNONE)
, m_IsValid(false)
{
}

void EBStatic::GrassCSVars::UpdateValidity()
{
	m_IsValid =	m_ShaderIndex > -1 &&
				std::accumulate(m_idVarAppendInstBuffer.begin(), m_idVarAppendInstBuffer.end(), true, [](bool valid, grcEffectVar var){ return valid && var != grcevNONE; }) &&
				DURANGO_ONLY(GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(m_idIndirectArgs != grcevNONE &&)) m_idInstCullParams != grcevNONE && m_idNumClipPlanes != grcevNONE && 
				m_idClipPlanes != grcevNONE && m_idCameraPosition != grcevNONE && m_idLodInstantTransition != grcevNONE && m_idLodThresholds != grcevNONE && 
				m_idCrossFadeDistance != grcevNONE && m_idIsShadowPass != grcevNONE && m_idLodFadeControlRange != grcevNONE && DURANGO_ONLY(m_idIndirectCountPerLod != grcevNONE &&)
				m_idVarInstanceBuffer != grmsgvNONE && m_idVarUseCSOutputBuffer != grmsgvNONE && m_idAabbMin != grmsgvNONE && m_idAabbDelta != grmsgvNONE && m_idScaleRange != grmsgvNONE;
}

EBStatic::GrassCSBaseParams::GrassCSBaseParams()
: m_InstanceList(NULL)
, m_Vars(NULL)
, m_InstanceCount(0)
, m_InstAabbRadius(0.0f)
GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(, m_IndirectArgCount(0))
GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(, m_IndirectArgLodOffsets(0))
, m_RawInstBuffer(NULL)
GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, m_IndirectArgsBuffer(NULL))
GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, m_OffsetMap(NULL))
PIX_TAGGING_ONLY(, m_ModelIndex(-1))
, m_IsValid(false)
{
}

void EBStatic::GrassCSBaseParams::UpdateValidity()
{
	m_IsValid =	m_InstanceList != NULL && m_Vars && m_Vars->IsValid() && m_InstanceCount > 0 && GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(m_IndirectArgCount > 0 &&)
				GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(m_IndirectArgsBuffer != NULL && m_OffsetMap != NULL &&) m_RawInstBuffer != NULL;
}

const EBStatic::GrassCSBaseParams EBStatic::GrassCSParams::sm_InvalidBaseForDefaultConstructor;
EBStatic::GrassCSParams::GrassCSParams()
: m_Base(sm_InvalidBaseForDefaultConstructor)
, m_CurrVp(NULL)
GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, m_AppendInstBuffer(NULL))
#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
, m_AppendBufferMem(NULL)
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS
, m_phaseLODs(0)
, m_lastLODIdx(0)
, m_LODIdx(0)
, m_UseAltfadeDist(false)
, m_Active(false)
{
}

EBStatic::GrassCSParams::GrassCSParams(const GrassCSBaseParams &base)
: m_Base(base)
, m_CurrVp(NULL)
GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, m_AppendInstBuffer(NULL))
#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
, m_AppendBufferMem(NULL)
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS
, m_phaseLODs(0)
, m_lastLODIdx(0)
, m_LODIdx(0)
, m_UseAltfadeDist(false)
, m_Active(false)
{
}

void CGrassBatch::GetCurrentCSParams(CSParams &params) GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(const)
{
	rage_placement_new(&params) EBStatic::GrassCSParams(m_CSParams);	//Re-initialize params

	if(DRAWLISTMGR->IsBuildingCascadeShadowDrawList())
		params.m_CurrVp = NULL;
	else
	{
		params.m_CurrVp = EBStatic::GetClippingVP();
		if(params.m_CurrVp == NULL)
			return;
	}

	//Need to find farthest point on batch...
	spdAABB aabb;
	GetAABB(aabb);
	Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	Vec3V farthestPointOnAABB = SelectFT(IsLessThan(camPos, aabb.GetCenter()), aabb.GetMin(), aabb.GetMax());

	//Find LOD span
	u32 minLod = ComputeLod(aabb.DistanceToPoint(camPos).Getf());
	u32 maxLod = ComputeLod(Dist(camPos, farthestPointOnAABB).Getf());
#if __ASSERT
	Vec3V closestPoint(V_NAN);
	if(maxLod < minLod)
	{
		const Vec3V center = GetInstanceList()->m_BatchAABB.GetCenter();
		const Vec3V extent = GetInstanceList()->m_BatchAABB.GetExtent();
		const Vec3V v      = Max(Vec3V(V_ZERO), Abs(camPos - center) - extent);
		closestPoint = camPos - v;
	}
#endif //_ASSERT

	Assertf(maxLod >= minLod, "WARNING: maxLod is less than minLod. Please report this assert to Alan Goykhman.\n"
			"Camera Pos: {%f, %f, %f}\naabb.Min: {%f, %f, %f}\naabb.Max: {%f, %f, %f}\nClosest Point: {%f, %f, %f | %f}\nFarthest Point: {%f, %f, %f | %f}\nDistance From Camera: %f\nMin|Max LOD: {%d|%d}\nEntity Name: %s", 
			camPos.GetXf(), camPos.GetYf(), camPos.GetZf(),
			GetInstanceList()->m_BatchAABB.GetMin().GetXf(), GetInstanceList()->m_BatchAABB.GetMin().GetYf(), GetInstanceList()->m_BatchAABB.GetMin().GetZf(),
			GetInstanceList()->m_BatchAABB.GetMax().GetXf(), GetInstanceList()->m_BatchAABB.GetMax().GetYf(), GetInstanceList()->m_BatchAABB.GetMax().GetZf(),
			closestPoint.GetXf(), closestPoint.GetYf(), closestPoint.GetZf(), GetInstanceList()->m_BatchAABB.DistanceToPoint(camPos).Getf(),
			farthestPointOnAABB.GetXf(), farthestPointOnAABB.GetYf(), farthestPointOnAABB.GetZf(), Dist(camPos, farthestPointOnAABB).Getf(),
			GetDistanceToCamera(), minLod, maxLod, GetModelName());

	maxLod = Max(maxLod, minLod);
	u32 lodCount = 1 + (maxLod - minLod);

	params.m_phaseLODs = GetPhaseLODs();
	params.m_lastLODIdx = maxLod; //GetLastLODIdx();
	params.m_LODIdx = minLod;
	params.m_UseAltfadeDist = GetUseAltFadeDistance();

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	artAssertf(m_InstanceList->m_InstanceList.GetCount() <= EBStatic::PreAllocatedAppendBufferPool::value_type::sMaxBufferCount, "WARNING! Pre-allocated append buffer size it not large enough for all instances of grass in this batch! Some instances will may not be displayed! Please report this error to the graphics team.");
	if(m_InstanceList->m_InstanceList.GetCount() > EBStatic::PreAllocatedAppendBufferPool::value_type::sMaxBufferCount)
		return;	//Don't use CS in this case as it could cause popping. Assert will be triggered to indicate the problem.
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

	if(EBStatic::sPerfEnableComputeShader WIN32PC_ONLY(&& GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50)))
	{
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
		for(int i = 0; i < lodCount; ++i)
		{
			params.m_AppendInstBuffer[i] = EBStatic::sAppendBufferPool.Allocate();
			Assertf(params.m_AppendInstBuffer[i], "WARNING: Failed to allocate an append buffer for grass CS. This should not happen...");
			if(params.m_AppendInstBuffer[i] == NULL)
				return;
		}

		params.m_LocalIndirectArgBuffer = EBStatic::sIndirectArgBufferPool.Allocate();
		Assertf(params.m_LocalIndirectArgBuffer, "WARNING: Failed to allocate a local indirect buffer for grass CS. This should not happen...");
		if(params.m_LocalIndirectArgBuffer == NULL)
			return;
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
		//Initialize indirect arg temp buffer so render thread can allocate later.
		params.m_IndirectBufferMem.init(params.m_Base.m_IndirectArgCount ORBIS_ONLY(, sce::Gnm::kAlignmentOfIndirectArgsInBytes));

		const eDrawListPageType dpType = DPT_LIFETIME_GPU_PHYSICAL;
		const bool mayFail = false;
		const size_t appendBufSize = GrassBatchCSInstanceData_Size * m_InstanceList->m_InstanceList.GetCount();
		int appendBufAlign = GrassBatchCSInstanceData_Align;	//ORBIS_ONLY(appendBufAlign = 16); //Should this be 16-byte align for PS4?

		for(int i = 0; i < lodCount; ++i)
		{
			params.m_AppendBufferMem[i] = reinterpret_cast<void *>(gDCBuffer->AllocatePagedMemory(dpType, appendBufSize, mayFail, appendBufAlign));
			if(params.m_AppendBufferMem[i] == NULL)
				return;
		}
		Assert(params.m_AppendBufferMem[0] != NULL); //At least 1 append buffer must be created.

		//Create actual buffer resources.
#	if RSG_ORBIS
		for(int i = 0; i < lodCount; ++i)
		{
			params.m_AppendDeviceBuffer[i].Initialize(params.m_AppendBufferMem[i], m_InstanceList->m_InstanceList.GetCount(), true, GrassBatchCSInstanceData_Size);
			params.m_AppendDeviceBuffer[i].m_GdsCounterOffset = dlComputeShaderBatch::GetGDSOffsetForCounter();
		}
#	endif
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS

		params.m_Active = true;
	}
}
#endif //GRASS_BATCH_CS_CULLING

//////////////////////////////////////////////////////////////////////////////
//
// Pool functionality
//
#if __DEV
template<> void fwPool<CEntityBatch>::PoolFullCallback()
{
	INSTANCE_STORE.PrintLoadedObjs();
}

template<> void fwPool<CGrassBatch>::PoolFullCallback()
{
	INSTANCE_STORE.PrintLoadedObjs();
}
#endif

#if KEEP_INSTANCELIST_ASSETS_RESIDENT

//////////////////////////////////////////////////////////////////////////
//
// Asset loading
//
void CInstanceListAssetLoader::Init(u32 initMode)
{
	if (initMode == INIT_SESSION)
	{
		RequestAllModels("v_proc1");
		RequestAllModels("procobj");		// strictly speaking not grassbatch assets, but never mind.
	}
}

void CInstanceListAssetLoader::Shutdown(u32 initMode)
{
	if (initMode == SHUTDOWN_SESSION)
	{
		ReleaseAllModels("v_proc1");
		ReleaseAllModels("procobj");
	}
}

void CInstanceListAssetLoader::RequestAllModels(const char* pszItypName)
{
	// load all IP veg assets up front, so they don't need to be processed by scene streaming at all
	strLocalIndex itypSlot = g_MapTypesStore.FindSlot(pszItypName);
	if (itypSlot.IsValid())
	{
		fwArchetypeManager::ForAllArchetypesInFile(itypSlot.Get(), RequestModel);
		strStreamingEngine::GetLoader().LoadAllRequestedObjects();
	}
}

void CInstanceListAssetLoader::ReleaseAllModels(const char* pszItypName)
{
	// load all IP veg assets up front, so they don't need to be processed by scene streaming at all
	strLocalIndex itypSlot = g_MapTypesStore.FindSlot(pszItypName);
	if (itypSlot.IsValid())
	{
		fwArchetypeManager::ForAllArchetypesInFile(itypSlot.Get(), ReleaseModel);
	}
}

void CInstanceListAssetLoader::RequestModel(fwArchetype* pArchetype)
{
	if (pArchetype)
	{
		CModelInfo::RequestAssets((CBaseModelInfo*) pArchetype, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_ZONEDASSET_REQUIRED);
	}
}

void CInstanceListAssetLoader::ReleaseModel(fwArchetype* pArchetype)
{
	if (pArchetype)
	{
		const s32 slot = pArchetype->GetStreamSlot().Get();
		strStreamingModule* pModule = fwArchetypeManager::GetStreamingModule();
		if (pModule)
		{
			pModule->ClearRequiredFlag(slot, STRFLAG_ZONEDASSET_REQUIRED);
		}
	}
}

#endif	//KEEP_INSTANCELIST_ASSETS_RESIDENT
