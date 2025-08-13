//
//
//    Filename: ModelInfo.cpp
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description:
//
//

// Rage headers
#include "atl/binmap.h"
#include "diag/art_channel.h"
#include "file/asset.h"
#include "file/packfile.h"
#include "fragment/resourceversions.h"
#include "paging/rscbuilder.h"
#include "system/ipc.h"
#include "grcore/effect.h"
#if __BANK
#include "fragment/tune.h"
#endif	// __BANK

// Framework headers
#include "entity/archetypemanager.h"
#include "entity/archetypestreamingmodule.h"
#include "fwtl/pool.h"
#include "entity/entity.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/expressionsets.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwpheffects/ropedatamanager.h"
#include "entity/archetypemanager.h"
#include "fwutil/xmacro.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwscene/stores/clothstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h" 
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/fileexts.h"
#include "fwsys/metadatastore.h"
#include "fwsys/timer.h"
#include "streaming/streaming_channel.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxmanager.h"

// Game headers
#include "audio/northaudioengine.h"
#include "core/game.h"
#include "cutscene/cutscenemanagernew.h"
#include "game/config.h"
#include "game/modelindices.h"
#include "modelinfo.h"
#include "modelinfo/basemodelinfo.h"
#include "modelinfo/compentitymodelinfo.h"
#include "modelinfo/mlomodelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/timemodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "modelinfo/weaponmodelinfo.h"
#include "Network/Live/NetworkTelemetry.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "objects/Door.h"
#include "peds/ped.h"
#include "renderer/gtadrawable.h"
#include "renderer/Lights/LightEntity.h"
#include "scene/Building.h"
#include "scene/AnimatedBuilding.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h"
#include "scene/2deffect.h"
#include "scene/loader/MapData.h"
#include "scene/loader/MapData_Extensions.h"
#include "scene/loader/MapData_Buildings.h"
#include "scene/loader/MapData_Composite.h"
#include "scene/loader/MapData_Interiors.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "system/memory.h"
#include "system/memmanager.h"
#include "vehicles/vehicle.h"
#include "vfx/systems/VfxEntity.h"
#include "task/Scenario/ScenarioPoint.h"

#include "scene/DynamicEntity.h"

#include "fwscene/stores/maptypesstore.h"

#include "scene/ExtraMetadataMgr.h"

#include "Stats/MoneyInterface.h"

STREAMING_OPTIMISATIONS();

//#include "tempColModels.h"

RAGE_DEFINE_CHANNEL(modelinfo)

FW_INSTANTIATE_CLASS_POOL(CArchetypeStreamSlot, 255, atHashString("ArchetypeStreamSlots",0xdc81ceeb));		// only storing 8 bits in baseModelInfo for this!! Arrgh!

// for extension factories
FW_INSTANTIATE_CLASS_POOL(CLightExtension, CONFIGURED_FROM_FILE, atHashString("CLightExtension",0x97930a24));		
FW_INSTANTIATE_CLASS_POOL(CDoorExtension, CONFIGURED_FROM_FILE, atHashString("CDoorExtension",0xb4e04aa4));	
FW_INSTANTIATE_CLASS_POOL(CSpawnPointOverrideExtension, CONFIGURED_FROM_FILE, atHashString("CSpawnPointOverrideExtension",0xa38f3d93));

u32		CArchetypeStreamSlot::ms_timer = 0;

// archetype factories
static fwArchetypeDynamicFactory<CBaseModelInfo> gBaseModelStore;
static fwArchetypeDynamicFactory<CTimeModelInfo> gTimeModelStore;
static fwArchetypeDynamicFactory<CMloModelInfo> gMloModelStore;
static fwArchetypeDynamicFactory<CCompEntityModelInfo> gCompEntityModelStore;
static fwArchetypeDynamicFactory<CWeaponModelInfo> gWeaponModelStore;
static fwArchetypeDynamicFactory<CVehicleModelInfo> gVehicleModelStore;
static fwArchetypeDynamicFactory<CPedModelInfo> gPedModelStore;

// Archetype Extensions
static fwArchetypeExtensionFactory<CLadderInfo> gLadderStore2;
static fwArchetypeExtensionFactory<CProcObjAttr> gProcObjStore2;
static fwArchetypeExtensionFactory<CParticleAttr> gParticleStore2;
static fwArchetypeExtensionFactory<CExplosionAttr> gExplosionStore2;
static fwArchetypeExtensionFactory<CDecalAttr> gDecalStore2;
static fwArchetypeExtensionFactory<CWindDisturbanceAttr> gWindDisturbanceStore2;
static fwArchetypeExtensionFactory<CLightShaftAttr> gLightShaftStore2;
static fwArchetypeExtensionFactory<CScrollBarAttr> gScrollStore2;
static fwArchetypeExtensionFactory<CSpawnPoint> gSpawnPointStore2;
static fwArchetypeExtensionFactory<CBuoyancyAttr> gBuoyancyStore2;
static fwArchetypeExtensionFactory<CAudioAttr> gAudioStore2;
static fwArchetypeExtensionFactory<CWorldPointAttr> gWorldPointStore2;
static fwArchetypeExtensionFactory<CExpressionExtension> gExpressionExtensionStore2;
static fwArchetypeExtensionFactory<CAudioCollisionInfo> gAudioCollisionStore2;

// ---

// 2d effects free in world
// not a factory, probably shouldn't be in here...
atArray<C2dEffect*>				CModelInfo::ms_World2dEffectArray;

// Entity Extensions
static fwExtensionPoolFactory<CDoorExtension> gDoorExtensionStore;
static fwExtensionPoolFactory<CLightExtension> gLightExtensionStore;
static fwExtensionPoolFactory<CSpawnPointOverrideExtension> gSpawnPointOverrideExtensionStore;
static fwExtensionPoolFactory<fwClothCollisionsExtension> gClothCollisionsStore;

// copied out data - looking for mods
const u32 obfuscationMask = 0x5695a421;
u32		CModelInfo::ms_obfMaleMPPlayerModelId = 0;
u32		CModelInfo::ms_obfFemaleMPPlayerModelId = 0;

//default audio model collision settings id
const u32 g_defaultAudModelCollisions = ATSTRINGHASH("MOD_DEFAULT", 0x05f03e8c2);

//
// name:		CModelInfoStreamingModule
// description:	Streaming Module class. This is the interface to the streaming system for modelinfos
class CModelInfoStreamingModule : public fwArchetypeStreamingModule
{
public:
	CModelInfoStreamingModule() : fwArchetypeStreamingModule(fwArchetypeManager::GetMaxArchetypeIndex()) {}

	virtual strLocalIndex Register(const char* name);
	virtual strLocalIndex FindSlot(const char* name) const;

	virtual void GetModelMapTypeIndex(strLocalIndex slotIndex, strIndex& retIndex) const;

	virtual int GetDependencies(strLocalIndex index, strIndex *pIndices, int indexArraySize) const;
	virtual void Remove(strLocalIndex index);
	virtual void RemoveSlot(strLocalIndex index);
	virtual bool Load(strLocalIndex index, void* object, int size);

private:
	void GetExpressionSetDependencies(const fwMvExpressionSetId &expressionSetId, strIndex *pIndices, int &count, int indexArraySize, strIndex streamingIndex) const;
	void GetClipSetDependencies(const fwMvClipSetId &clipSetId, strIndex *pIndices, int &count, int indexArraySize, strIndex streamingIndex) const;
};	

// --- CModelInfoStreamingModule ---------------------------------------------------------------------------------------

strLocalIndex CModelInfoStreamingModule::FindSlot(const char* name) const
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(name, &modelId);

	if(pModelInfo)
		return strLocalIndex(modelId.GetModelIndex());
	
	return strLocalIndex(-1);
}

strLocalIndex CModelInfoStreamingModule::Register(const char* UNUSED_PARAM(name))
{	
	Assert(false);
	return strLocalIndex(-1);
}

void CModelInfoStreamingModule::RemoveSlot(strLocalIndex UNUSED_PARAM(index))
{	
	Assert(false);
}

void CModelInfoStreamingModule::GetModelMapTypeIndex(strLocalIndex slotIndex, strIndex& retIndex) const
{
	if (CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(slotIndex.Get()))
	{
		const fwModelId& modelId = fwArchetypeManager::LookupModelId((fwArchetype*)pModelInfo);

		if (modelId.IsValid())
		{
			strLocalIndex mapTypeIndex = modelId.GetMapTypeDefIndex();

			if (mapTypeIndex.IsValid() && mapTypeIndex.Get() != fwModelId::TF_INVALID)
				retIndex = g_MapTypesStore.GetStreamingIndex(mapTypeIndex);
		}
	}
}

int CModelInfoStreamingModule::GetDependencies(strLocalIndex index, strIndex *pIndices, int indexArraySize) const 
{
	Assert(index.Get() >= 0);
	s32 count = fwArchetypeStreamingModule::GetDependencies(index, pIndices, indexArraySize);

	CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(index.Get());
	modelinfoAssert(pModelInfo);

	strIndex streamingIndex = GetStreamingIndex(index);

	
	strLocalIndex clipDictionaryIndex = pModelInfo->GetClipDictionaryIndex();
	if (clipDictionaryIndex != -1) {
		AddDependencyOutput(pIndices, count, g_ClipDictionaryStore.GetStreamingIndex(clipDictionaryIndex), indexArraySize, streamingIndex);
	}

	// check if expression data file is actually in the dictionary before making it dependent
	strLocalIndex expressionFileIndex = strLocalIndex(pModelInfo->GetExpressionDictionaryIndex());
	if (expressionFileIndex != -1)
	{
		AddDependencyOutput(pIndices, count, g_ExpressionDictionaryStore.GetStreamingIndex(expressionFileIndex), indexArraySize, streamingIndex);
	}

	fwMvExpressionSetId expressionSet = pModelInfo->GetExpressionSet();
	if (expressionSet!=EXPRESSION_SET_ID_INVALID)
	{
		// If using expression sets, get all the dependencies
		GetExpressionSetDependencies(expressionSet, pIndices, count, indexArraySize, streamingIndex);
	}	

	// main particle asset dependency
	u32 ptfxAssetSlot = pModelInfo->GetPtFxAssetSlot();
	if (ptfxAssetSlot != -1) 
	{
		AddDependencyOutput(pIndices, count, ptfxManager::GetAssetStore().GetStreamingIndex(strLocalIndex(ptfxAssetSlot)), indexArraySize, streamingIndex);
	}

	if (pModelInfo->GetModelType() == MI_TYPE_BASE)
	{
		// If the model has an expression extension update 
		// the expression dictionary index and the expression hash
		// the creature metadata
		GET_2D_EFFECTS_NOLIGHTS(pModelInfo);
		for(s32 effect = 0; effect < iNumEffects; effect++)
		{
			if((*pa2dEffects)[effect] != NULL && (*pa2dEffects)[effect]->GetType() == ET_EXPRESSION)
			{
				CExpressionExtension* pExpressionExtension = (*pa2dEffects)[effect]->GetExpression();
				if (pExpressionExtension)
				{
					if (pExpressionExtension->m_expressionDictionaryName.IsNotNull() && pExpressionExtension->m_expressionName.IsNotNull())
					{
						pModelInfo->SetExpressionDictionaryIndex(pExpressionExtension->m_expressionDictionaryName.GetCStr());
						pModelInfo->SetExpressionHash(pExpressionExtension->m_expressionName.GetCStr());
					}

					if (pExpressionExtension->m_creatureMetadataName.IsNotNull())
					{
						pModelInfo->SetCreatureMetadataFile(pExpressionExtension->m_creatureMetadataName.GetCStr());
					}
				}
			}
		}

		// check if expression data file is actually in the dictionary before making it dependent
		strLocalIndex expressionFileIndex = pModelInfo->GetExpressionDictionaryIndex();
		if (expressionFileIndex != -1)
		{
			AddDependencyOutput(pIndices, count, g_ExpressionDictionaryStore.GetStreamingIndex(expressionFileIndex), indexArraySize, streamingIndex);
		}

		// check if creatureMetadata file is actually exists before making it dependent
		strLocalIndex creatureMetadataFileIndex = strLocalIndex(pModelInfo->GetCreatureMetadataFileIndex());
		if (creatureMetadataFileIndex != -1)
		{
			AddDependencyOutput(pIndices, count, g_fwMetaDataStore.GetStreamingIndex(creatureMetadataFileIndex), indexArraySize, streamingIndex);
		}

	}

	if (pModelInfo->GetModelType() == MI_TYPE_PED)
	{
		CPedModelInfo *pPedModelInfo = (CPedModelInfo *) pModelInfo;
			
		// check if props file is actually in the dictionary before making it dependent
		strLocalIndex propsFileIndex = strLocalIndex(pModelInfo->GetPropsFileIndex());
		if (propsFileIndex != -1 && pModelInfo->GetModelType() == MI_TYPE_PED && g_DwdStore.IsObjectInImage(propsFileIndex))
		{
			AddDependencyOutput(pIndices, count, g_DwdStore.GetStreamingIndex(propsFileIndex), indexArraySize, streamingIndex);
		}

		// check if drawable file is actually in the dictionary before making it dependent
		strLocalIndex pedCompFileIndex = strLocalIndex(pPedModelInfo->GetPedComponentFileIndex());
		if (pedCompFileIndex != -1 && g_DwdStore.IsObjectInImage(pedCompFileIndex))
		{
			AddDependencyOutput(pIndices, count, g_DwdStore.GetStreamingIndex(pedCompFileIndex), indexArraySize, streamingIndex);
		}

#if ENABLE_BLENDSHAPES
		// check if blendshape file is actually in the dictionary before making it dependent
		s32 blendShapeFileIndex = pModelInfo->GetBlendShapeFileIndex();
		if(blendShapeFileIndex != -1 && g_BlendShapeStore.IsObjectInImage(blendShapeFileIndex))
		{
			AddDependencyOutput(pIndices, count, g_BlendShapeStore.GetStreamingIndex(blendShapeFileIndex), indexArraySize, streamingIndex);
		}
#endif // ENABLE_BLENDSHAPES

		// check if meta data file is actually in the dictionary before making it dependent
#if LIVE_STREAMING
		if (pPedModelInfo->GetNumPedMetaDataFiles() == 0)
			pPedModelInfo->SetPedMetaDataFile(pPedModelInfo->GetModelName());
#endif // LIVE_STREAMING
		for (s32 meta = 0; meta < pPedModelInfo->GetNumPedMetaDataFiles(); ++meta)
			if (pPedModelInfo->GetPedMetaDataFileIndex(meta) != -1)
				AddDependencyOutput(pIndices, count, g_fwMetaDataStore.GetStreamingIndex(strLocalIndex(pPedModelInfo->GetPedMetaDataFileIndex(meta))), indexArraySize, streamingIndex);

		atFixedArray<SPedDLCMetaFileQueryData, STREAMING_MAX_DEPENDENCIES> creatureIndicesToLoad;
		EXTRAMETADATAMGR.GetCreatureMetaDataIndices(pPedModelInfo, creatureIndicesToLoad);

		for (u32 cmIdx = 0; cmIdx < creatureIndicesToLoad.GetCount(); cmIdx++)
		{
			// check if creatureMetadata data file is actually in the dictionary before making it dependent
			strLocalIndex creatureMetadataFileIndex = strLocalIndex(creatureIndicesToLoad[cmIdx].m_storeIndex);
			if (creatureMetadataFileIndex != -1)
			{
				AddDependencyOutput(pIndices, count, g_fwMetaDataStore.GetStreamingIndex(creatureMetadataFileIndex), indexArraySize, streamingIndex);
			}
		}

		// add the strafe movement anim set
		strLocalIndex clothFileIndex = g_ClothStore.FindSlotFromHashKey(pModelInfo->GetModelNameHash());
		if (clothFileIndex != -1)
		{
			AddDependencyOutput(pIndices, count, g_ClothStore.GetStreamingIndex(clothFileIndex), indexArraySize, streamingIndex);
		}

		// Facial Clipset Group dependencies
		// We only care about the base clipset as a dependency.  The variations are optional and will be streamed in on demand.
		fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(pPedModelInfo->GetFacialClipSetGroupId());
		if (pFacialClipSetGroup)
		{
			fwMvClipSetId facialBaseClipSetId(pFacialClipSetGroup->GetBaseClipSetName());
			Assertf(fwClipSetManager::GetClipSet(facialBaseClipSetId), "%s:Unrecognised facial base clipset", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());
			GetClipSetDependencies(facialBaseClipSetId, pIndices, count, indexArraySize, streamingIndex);
		}

		GetClipSetDependencies(pPedModelInfo->GetMovementClipSet(), pIndices, count, indexArraySize, streamingIndex);
		for(s32 i = 0; i < pPedModelInfo->GetMovementClipSets().GetCount(); i++)
		{
			GetClipSetDependencies(pPedModelInfo->GetMovementClipSets()[i], pIndices, count, indexArraySize, streamingIndex);
		}
		GetClipSetDependencies(pPedModelInfo->GetStrafeClipSet(), pIndices, count, indexArraySize, streamingIndex);
		GetClipSetDependencies(pPedModelInfo->GetInjuredStrafeClipSet(), pIndices, count, indexArraySize, streamingIndex);
		//GetClipSetDependencies(pPedModelInfo->GetStealthClipSet(), pIndices, count, indexArraySize, streamingIndex);
		GetClipSetDependencies(pPedModelInfo->GetFullBodyDamageClipSet(), pIndices, count, indexArraySize, streamingIndex);
		GetClipSetDependencies(pPedModelInfo->GetAdditiveDamageClipSet(), pIndices, count, indexArraySize, streamingIndex);
	}
	else if(pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		// Maybe could just have a virtual function on base model info to fill this out for all different types?
		strIndex iVehicleDeps[STREAMING_MAX_DEPENDENCIES];

		CVehicleModelInfo* vmi = (CVehicleModelInfo*)pModelInfo;
		int iVehicleCount = vmi->GetStreamingDependencies(iVehicleDeps,STREAMING_MAX_DEPENDENCIES);

		// Copy over dependencies
		for(int i = 0; i < iVehicleCount; i++)
		{
			AddDependencyOutput(pIndices, count, iVehicleDeps[i], indexArraySize, streamingIndex);
		}

		if (vmi->NeedsRopeTexture())
		{
			strLocalIndex txdSlot = g_TxdStore.FindSlot(ropeDataManager::GetRopeTxdName());
			if (txdSlot != -1)
				AddDependencyOutput(pIndices, count, g_TxdStore.GetStreamingIndex(txdSlot), indexArraySize, streamingIndex);
		}
	}

	return count;
} 

void CModelInfoStreamingModule::Remove(strLocalIndex index)
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(index.Get());
	modelinfoAssertf(pModelInfo, "Trying to remove a model that doesn't exist");

	//modelinfoAssert(pModelInfo->GetHasLoaded());

	// The following bit of code tells the PopulationStreaming a ped/vehicle has been loaded out.
	gPopStreaming.ModelHasBeenStreamedOut(index.Get());

	// clean up the allocated stuff which we add on top of the base drawable and fragments
	switch (pModelInfo->GetDrawableType())
	{
		case CBaseModelInfo::DT_FRAGMENT:
			pModelInfo->DeleteMasterFragData();				// this will clear drawable too...
			break;

		case CBaseModelInfo::DT_DRAWABLE:
		case CBaseModelInfo::DT_DRAWABLEDICTIONARY:
			pModelInfo->DeleteMasterDrawableData();
			break;

		default:
			break;
 	}

	pModelInfo->SetHasLoaded(false);
}

// the dependencies for this model info are satisfied. Initialise the master object and register with population
bool CModelInfoStreamingModule::Load(strLocalIndex index, void* object, int size)
{
	bool result = fwArchetypeStreamingModule::Load(index, object, size);

	if (result) {
		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(index.Get());
		modelinfoAssert(pModelInfo);

		switch (pModelInfo->GetDrawableType()) {
			case fwArchetype::DT_DRAWABLE:
			case fwArchetype::DT_DRAWABLEDICTIONARY:
				pModelInfo->InitMasterDrawableData(index.Get());
				break;

			case fwArchetype::DT_FRAGMENT:
				pModelInfo->InitMasterFragData(index.Get());
				break;

			default:
				break;
		}

		// The following bit of code tells the PopulationStreaming a ped/vehicle has been loaded in.
		gPopStreaming.ModelHasBeenStreamedIn(index.Get());
	}

	return result;
}

void CModelInfoStreamingModule::GetExpressionSetDependencies(const fwMvExpressionSetId &expressionSetId, strIndex *pIndices, int &count, int indexArraySize, strIndex streamingIndex) const
{
	fwExpressionSet* pExpressionSet = fwExpressionSetManager::GetExpressionSet(expressionSetId);
	if (pExpressionSet)
	{
		strLocalIndex index = strLocalIndex(g_ExpressionDictionaryStore.FindSlotFromHashKey(pExpressionSet->GetDictionaryName()));
		// If cannot find slot then create one. 
		if(index == -1)
		{
			index = g_ExpressionDictionaryStore.AddSlot(pExpressionSet->GetDictionaryName());
		}
		else
		{
			Assertf(!stricmp(pExpressionSet->GetDictionaryName().GetCStr(), g_ExpressionDictionaryStore.GetName(index)), "%s:Hash name clash", pExpressionSet->GetDictionaryName().GetCStr());
		}

		if (index != -1)
		{
			AddDependencyOutput(pIndices, count, g_ExpressionDictionaryStore.GetStreamingIndex(index), indexArraySize, streamingIndex);
		}
	}
}

void CModelInfoStreamingModule::GetClipSetDependencies(const fwMvClipSetId &clipSetId, strIndex *pIndices, int &count, int indexArraySize, strIndex streamingIndex) const
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (pClipSet)
	{
		do 
		{
			strLocalIndex dictionaryIndex = pClipSet->GetClipDictionaryIndex();
			if (dictionaryIndex != -1)
			{
				const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(pClipSet->GetClipDictionaryName());				
				if (pClipDictionaryMetadata == NULL)
				{
					// It is valid not to have a clipdictionary entry it implies streaming
					AddDependencyOutput(pIndices, count, g_ClipDictionaryStore.GetStreamingIndex(dictionaryIndex), indexArraySize, streamingIndex);
				}
				else if (pClipDictionaryMetadata && !(pClipDictionaryMetadata->GetStreamingPolicy() & SP_SINGLEPLAYER_RESIDENT))
				{
					AddDependencyOutput(pIndices, count, g_ClipDictionaryStore.GetStreamingIndex(dictionaryIndex), indexArraySize, streamingIndex);
				}
			}
			pClipSet = pClipSet->GetFallbackSet();
		} while (pClipSet);
	}
}

//
// name:		PrintModelInfoStoreUsage
// description:	Print how many of each modelinfo there are
void CModelInfo::PrintModelInfoStoreUsage()
{
	modelinfoDisplayf("Base ModelInfo %d", gBaseModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("Time ModelInfo %d", gTimeModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("Weapon ModelInfo %d", gWeaponModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("Vehicle ModelInfo %d", gVehicleModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("Ped ModelInfo %d", gPedModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("World 2dEffect %d", ms_World2dEffectArray.GetCount());
	modelinfoDisplayf("Mlo ModelInfo %d", gMloModelStore.GetCount(fwFactory::GLOBAL));
	modelinfoDisplayf("Comp Entity model info %d", gCompEntityModelStore.GetCount(fwFactory::GLOBAL));
}

void CModelInfo::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		fwTransform::InitPools();

		fwArchetypeManager::Init(MI_HIGHEST_FACTORY_ID);
		fwExtensionManager::Init(EXT_HIGHEST_FACTORY_ID);

		const CModelInfoConfig& cfgModel = CGameConfig::Get().GetConfigModelInfo();
		const C2dEffectConfig& cfg2d = CGameConfig::Get().GetConfig2dEffects();
		const CExtensionConfig& cfgExt = CGameConfig::Get().GetConfigExtensions();
		
	    {
		    RAGE_TRACK(ModelInfoStore);

			gBaseModelStore.Init( fwArchetypeManager::GetMaxArchetypeIndex() );
			gTimeModelStore.Init( cfgModel.m_MaxTimeModelInfos );
			gWeaponModelStore.Init( cfgModel.m_MaxWeaponModelInfos );

			gVehicleModelStore.Init( cfgModel.m_MaxVehicleModelInfos );
			gPedModelStore.Init( cfgModel.m_MaxPedModelInfos );
			gMloModelStore.Init( cfgModel.m_MaxMloModelInfos );
			gCompEntityModelStore.Init( cfgModel.m_MaxCompEntityModelInfos );

			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_BASE, gBaseModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_TIME, gTimeModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_WEAPON, gWeaponModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_VEHICLE, gVehicleModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_PED, gPedModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_MLO, gMloModelStore );
			fwArchetypeManager::RegisterArchetypeFactory( MI_TYPE_COMPOSITE, gCompEntityModelStore );

			fwMapTypes::AssociateArchetypeDef(CBaseArchetypeDef::parser_GetStaticStructure()->GetNameHash(), MI_TYPE_BASE);
			fwMapTypes::AssociateArchetypeDef(CTimeArchetypeDef::parser_GetStaticStructure()->GetNameHash(), MI_TYPE_TIME);
			fwMapTypes::AssociateArchetypeDef(CMloArchetypeDef::parser_GetStaticStructure()->GetNameHash(), MI_TYPE_MLO);
			fwMapTypes::AssociateArchetypeDef(CCompositeEntityArchetypeDef::parser_GetStaticStructure()->GetNameHash(), MI_TYPE_COMPOSITE);

			{
#if (__PS3 || __XENON) && !__TOOL && !__RESOURCECOMPILER
				sysMemAutoUseFragCacheMemory mem;
#endif
				fwArchetypeManager::GetArchetypeFactory(MI_TYPE_BASE)->AddStorageBlock(fwFactory::GLOBAL, cfgModel.m_MaxBaseModelInfos);		// to handle portal and light archetypes
				fwArchetypeManager::GetArchetypeFactory(MI_TYPE_COMPOSITE)->AddStorageBlock(fwFactory::GLOBAL, cfgModel.m_MaxCompEntityModelInfos);
				fwArchetypeManager::GetArchetypeFactory(MI_TYPE_WEAPON)->AddStorageBlock(fwFactory::GLOBAL, cfgModel.m_MaxWeaponModelInfos);
				fwArchetypeManager::GetArchetypeFactory(MI_TYPE_VEHICLE)->AddStorageBlock(fwFactory::GLOBAL, cfgModel.m_MaxVehicleModelInfos);
				fwArchetypeManager::GetArchetypeFactory(MI_TYPE_PED)->AddStorageBlock(fwFactory::GLOBAL, cfgModel.m_MaxPedModelInfos);
			}			
		}

		{
			RAGE_TRACK(2DEffectStore);

			ms_World2dEffectArray.Reset();
			ms_World2dEffectArray.Reserve(cfg2d.m_MaxEffectsWorld2d );
		}

		{
			RAGE_TRACK(2DEffects);

			gLadderStore2.Init( cfg2d.m_MaxAttrsLadder );
			gProcObjStore2.Init( cfg2d.m_MaxAttrsProcObj );
			gParticleStore2.Init( cfg2d.m_MaxAttrsParticle );
			gExplosionStore2.Init( cfg2d.m_MaxAttrsExplosion );
			gDecalStore2.Init( cfg2d.m_MaxAttrsDecal );
			gWindDisturbanceStore2.Init( cfg2d.m_MaxAttrsWindDisturbance );
			gLightShaftStore2.Init( cfg2d.m_MaxAttrsLightShaft );
			gScrollStore2.Init( cfg2d.m_MaxAttrsScrollBar );
			gSpawnPointStore2.Init( cfg2d.m_MaxAttrsSpawnPoint );
			gBuoyancyStore2.Init( cfg2d.m_MaxAttrsBuoyancy );
			gAudioStore2.Init( cfg2d.m_MaxAttrsAudio );
			gWorldPointStore2.Init( cfg2d.m_MaxAttrsWorldPoint );
			gExpressionExtensionStore2.Init( cfgExt.m_MaxExpressionExtensions );
			gAudioCollisionStore2.Init(cfg2d.m_MaxAttrsAudioCollision);

			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_LADDER, gLadderStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_PROCOBJ, gProcObjStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_EXPLOSION, gExplosionStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_PARTICLE, gParticleStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_DECAL, gDecalStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_WINDDISTURBANCE, gWindDisturbanceStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_LIGHTSHAFT, gLightShaftStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_SCROLL, gScrollStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_SPAWNPOINT, gSpawnPointStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_BUOYANCY, gBuoyancyStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_AUDIO, gAudioStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_WORLDPOINT, gWorldPointStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_EXPRESSION, gExpressionExtensionStore2);
			fwArchetypeManager::Register2dEffectFactory( EXT_TYPE_AUDIO_COLLISIONS, gAudioCollisionStore2);

			CMapTypes::AssociateExtensionDef(CExtensionDefLadder::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_LADDER);
			CMapTypes::AssociateExtensionDef(CExtensionDefProcObject::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_PROCOBJ);
			CMapTypes::AssociateExtensionDef(CExtensionDefExplosionEffect::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_EXPLOSION);
			CMapTypes::AssociateExtensionDef(CExtensionDefParticleEffect::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_PARTICLE);
			CMapTypes::AssociateExtensionDef(CExtensionDefDecal::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_DECAL);
			CMapTypes::AssociateExtensionDef(CExtensionDefWindDisturbance::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_WINDDISTURBANCE);	
			CMapTypes::AssociateExtensionDef(CExtensionDefLightShaft::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_LIGHTSHAFT);	
			CMapTypes::AssociateExtensionDef(CExtensionDefScrollbars::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_SCROLL);	
			CMapTypes::AssociateExtensionDef(CExtensionDefBuoyancy::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_BUOYANCY);
			CMapTypes::AssociateExtensionDef(CExtensionDefAudioEmitter::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_AUDIO);
			CMapTypes::AssociateExtensionDef(CExtensionDefSpawnPoint::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_SPAWNPOINT);
			CMapTypes::AssociateExtensionDef(CExtensionDefExpression::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_EXPRESSION);
			CMapTypes::AssociateExtensionDef(CExtensionDefAudioCollisionSettings::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_AUDIO_COLLISIONS);

			{
#if (__PS3 || __XENON) && !__TOOL && !__RESOURCECOMPILER
				sysMemAutoUseFragCacheMemory mem;
#endif
				fwArchetypeManager::Get2dEffectFactory(EXT_TYPE_SPAWNPOINT)->AddStorageBlock(fwFactory::GLOBAL, cfg2d.m_MaxAttrsSpawnPoint);
				fwArchetypeManager::Get2dEffectFactory(EXT_TYPE_WORLDPOINT)->AddStorageBlock(fwFactory::GLOBAL, cfg2d.m_MaxAttrsWorldPoint);
			}
		}

		{
			RAGE_TRACK(Extensions);
			gDoorExtensionStore.Init( 0 );
			gLightExtensionStore.Init( 0 );
			gSpawnPointOverrideExtensionStore.Init( 0 );
			gClothCollisionsStore.Init(0);

			CDoorExtension::InitPool(cfgExt.m_MaxDoorExtensions, MEMBUCKET_WORLD);
			CLightExtension::InitPool(cfgExt.m_MaxLightExtensions, MEMBUCKET_WORLD);
			CSpawnPointOverrideExtension::InitPool(cfgExt.m_MaxSpawnPointOverrideExtensions, MEMBUCKET_WORLD);
			fwClothCollisionsExtension::InitPool( cfgExt.m_MaxClothCollisionsExtensions, MEMBUCKET_WORLD );

			gDoorExtensionStore.SetStoragePool(CDoorExtension::GetPool());
			gLightExtensionStore.SetStoragePool(CLightExtension::GetPool());
			gSpawnPointOverrideExtensionStore.SetStoragePool(CSpawnPointOverrideExtension::GetPool());
			gClothCollisionsStore.SetStoragePool(fwClothCollisionsExtension::GetPool());

			fwExtensionManager::RegisterFactory( EXT_TYPE_DOOR, gDoorExtensionStore );
			fwExtensionManager::RegisterFactory( EXT_TYPE_LIGHT, gLightExtensionStore );
			fwExtensionManager::RegisterFactory( EXT_TYPE_SPAWN_POINT_OVERRIDE, gSpawnPointOverrideExtensionStore );
			fwExtensionManager::RegisterFactory( EXT_TYPE_CLOTH_COLLISIONS, gClothCollisionsStore );

			fwMapData::AssociateExtensionDef(CExtensionDefDoor::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_DOOR);
			fwMapData::AssociateExtensionDef(CExtensionDefLightEffect::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_LIGHT);
			fwMapData::AssociateExtensionDef(CExtensionDefSpawnPointOverride::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_SPAWN_POINT_OVERRIDE);
			fwMapData::AssociateExtensionDef(phVerletClothCustomBounds::parser_GetStaticStructure()->GetNameHash(), EXT_TYPE_CLOTH_COLLISIONS);
		}

		fwMapTypes::SetMaxFactoryId(NUM_MI_TYPES);

	    // construct any static data associated with vehicle models
	    CVehicleModelInfo::ConstructStaticData();
    }
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
		fwArchetypeManager::ClearArchetypePtrStore();
    }
	else if(initMode == INIT_SESSION)
	{
		const CModelInfoConfig& cfgModel = CGameConfig::Get().GetConfigModelInfo();

		fwArchetypeManager::GetArchetypeFactory(MI_TYPE_WEAPON)->AddStorageBlock(fwFactory::EXTRA, cfgModel.m_MaxExtraWeaponModelInfos);
		fwArchetypeManager::GetArchetypeFactory(MI_TYPE_VEHICLE)->AddStorageBlock(fwFactory::EXTRA, cfgModel.m_MaxExtraVehicleModelInfos);
		fwArchetypeManager::GetArchetypeFactory(MI_TYPE_PED)->AddStorageBlock(fwFactory::EXTRA, cfgModel.m_MaxExtraPedModelInfos);
	}
}

//
// name: CModelInfo::ShutdownLevelWithMapUnloaded
// description: Remove all allocated memory after map has finished
//
void CModelInfo::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
    {
        gBaseModelStore.Shutdown();
	    //	gDamageAtomicModelStore.Init();
	    gTimeModelStore.Shutdown();
	    gWeaponModelStore.Shutdown();
	    gVehicleModelStore.Shutdown();

	    CVehicleModelInfo::DestroyStaticData();

	    gPedModelStore.Shutdown();
	    gMloModelStore.Shutdown();
		gCompEntityModelStore.Shutdown();
	    
		//extensions
		gLadderStore2.Shutdown();
		gProcObjStore2.Shutdown();
		gExplosionStore2.Shutdown();
		gParticleStore2.Shutdown();
		gDecalStore2.Shutdown();
		gWindDisturbanceStore2.Shutdown();
		gLightShaftStore2.Shutdown();
		gScrollStore2.Shutdown();
		gSpawnPointStore2.Shutdown();
		gBuoyancyStore2.Shutdown();
		gAudioStore2.Shutdown();
		gWorldPointStore2.Shutdown();

		ms_World2dEffectArray.Reset();
		
		// ---



		fwArchetypeManager::Shutdown();
		fwTransform::ShutdownPools();

		CDoorExtension::ShutdownPool();
		CLightExtension::ShutdownPool();
		CSpawnPointOverrideExtension::ShutdownPool();
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
	    gBaseModelStore.ForAllItemsUsed(&CBaseModelInfo::Shutdown);
	    gTimeModelStore.ForAllItemsUsed(&CTimeModelInfo::Shutdown);
	    gWeaponModelStore.ForAllItemsUsed(&CWeaponModelInfo::Shutdown);
	    gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::Shutdown);
	    gPedModelStore.ForAllItemsUsed(&CPedModelInfo::Shutdown);
	    gMloModelStore.ForAllItemsUsed(&CMloModelInfo::Shutdown);
		gCompEntityModelStore.ForAllItemsUsed(&CCompEntityModelInfo::Shutdown);

	    gBaseModelStore.Reset();
	    gTimeModelStore.Reset();
	    gWeaponModelStore.Reset();
	    gVehicleModelStore.Reset();
	    gPedModelStore.Reset();
	    gMloModelStore.Reset();
		gCompEntityModelStore.Reset();
	    
		// extensions
		gLadderStore2.Reset();
		gProcObjStore2.Reset();
		gExplosionStore2.Reset();
		gParticleStore2.Reset();
		gDecalStore2.Reset();
		gWindDisturbanceStore2.Reset();
		gLightShaftStore2.Reset();
		gScrollStore2.Reset();
		gSpawnPointStore2.Reset();
		gBuoyancyStore2.Reset();
		gAudioStore2.Reset();
		gWorldPointStore2.Reset();

		ms_World2dEffectArray.Reset();

		// ---


		fwArchetypeManager::Reset();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		CStreaming::GetCleanup().DeleteAllDrawableReferences();

        // get streaming to remove all the models
		const s32 maxModelInfos = fwArchetypeManager::GetMaxArchetypeIndex();
		strStreamingModule *module = GetStreamingModule();

	    for(s32 i=0; i<maxModelInfos; i++)
	    {
			if (module->IsAllocated(i))
			{
				if(module->IsObjectInImage(strLocalIndex(i)))
				{
					module->ClearRequiredFlag(i, STR_DONTDELETE_MASK);
					module->StreamingRemove(strLocalIndex(i));
				}
			}
	    }
		
		// Remove any extra data from the global model infos
		gVehicleModelStore.ForAllUsed(fwFactory::GLOBAL, &CModelInfo::ShutdownModelInfoExtra);
		gWeaponModelStore.ForAllUsed(fwFactory::GLOBAL, &CModelInfo::ShutdownModelInfoExtra);
		gPedModelStore.ForAllUsed(fwFactory::GLOBAL, &CModelInfo::ShutdownModelInfoExtra);

		// Shutdown any extra model infos
		fwArchetypeManager::FreeArchetypes(fwFactory::EXTRA);
    }
}

void CModelInfo::ShutdownModelInfoExtra(rage::fwArchetype *arch)
{
	if (CBaseModelInfo* pInfo = static_cast<CBaseModelInfo*>(arch))
		pInfo->ShutdownExtra();
}

void CModelInfo::CopyOutCurrentMPPedMapping()
{
	MoneyInterface::MemoryTampering( false );

#if (__PS3 || __XENON)
	fwModelId modelID;
	GetBaseModelInfoFromHashKey(ATSTRINGHASH("mp_m_freemode_01",0x705e61f2), &modelID);

	modelinfoDisplayf("tamper check : store - male : %d,%d : %d\n",(modelID.GetMapTypeDefIndex()).Get(), modelID.GetModelIndex(), modelID.ConvertToU32());

	ms_obfMaleMPPlayerModelId = (obfuscationMask ^ modelID.ConvertToU32());

	GetBaseModelInfoFromHashKey(ATSTRINGHASH("mp_f_freemode_01",0x9c9effd8), &modelID);
	ms_obfFemaleMPPlayerModelId = (obfuscationMask ^ modelID.ConvertToU32());

	modelinfoDisplayf("tamper check : store - female : %d,%d : %d\n",(modelID.GetMapTypeDefIndex()).Get(), modelID.GetModelIndex(), modelID.ConvertToU32());
#endif //(__PS3 || __XENON)
}

void CModelInfo::ValidateCurrentMPPedMapping()
{
// 	if (NetworkInterface::IsGameInProgress())
// 	{
// 		CPed *pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
// 		if (pPlayer)
// 		{
// 			u32 playerHash = pPlayer->GetArchetype()->GetModelNameHash();
// 
// 			if (playerHash != ATSTRINGHASH("mp_m_freemode_01",0x705e61f2)	&&
// 				playerHash != ATSTRINGHASH("mp_f_freemode_01",0x9c9effd8))
// 			{
// 				modelinfoDisplayf("tamper check : not a valid MP model\n");
// //				Quitf("tamper check :  not a valid MP model\n");
// 			}
// 		}
// 	}

#if (__PS3 || __XENON)
	MoneyInterface::MemoryTampering( false );

	bool bMaleValidated = true;
	bool bFemaleValidated = true;

	fwModelId modelID;
	GetBaseModelInfoFromHashKey(ATSTRINGHASH("mp_m_freemode_01",0x705e61f2), &modelID);
	bMaleValidated = (ms_obfMaleMPPlayerModelId == (obfuscationMask ^ modelID.ConvertToU32()));

	modelinfoDisplayf("tamper check : test - male : %d,%d : %d\n",modelID.GetMapTypeDefIndex(), modelID.GetModelIndex(), modelID.ConvertToU32());

	if (!bMaleValidated)
	{
		modelinfoDisplayf("male check failed");

		MoneyInterface::MemoryTampering( true );

		//report memory modification to MP player archetype
		Assertf(false,"Failed male MP ped fwModelID validation - indicates memory tampering using (%x)",modelID.ConvertToU32());
		CNetworkTelemetry::AppendMetricMem(ATSTRINGHASH("mp_m_freemode_01",0x705e61f2), modelID.ConvertToU32());
	}

	GetBaseModelInfoFromHashKey(ATSTRINGHASH("mp_f_freemode_01",0x9c9effd8), &modelID);
	bFemaleValidated = (ms_obfFemaleMPPlayerModelId == (obfuscationMask ^ modelID.ConvertToU32()));

	modelinfoDisplayf("tamper check : test - female : %d,%d : %d\n",modelID.GetMapTypeDefIndex(), modelID.GetModelIndex(), modelID.ConvertToU32());

	if (!bFemaleValidated)
	{
		modelinfoDisplayf("female check failed");

		MoneyInterface::MemoryTampering( true );

		//report memory modification to MP player archetype
		Assertf(false,"Failed female MP ped fwModelID validation - indicates memory tampering using (%x)",modelID.ConvertToU32());
		CNetworkTelemetry::AppendMetricMem(ATSTRINGHASH("mp_f_freemode_01",0x9c9effd8), modelID.ConvertToU32());
	}
#endif //(__PS3 || __XENON)
}

//
//        name: CModelInfo::AddBaseModel
// description: Allocate a new model from one of the model stores and give it an index
//          in: index = index model will have in model array
CBaseModelInfo* CModelInfo::AddBaseModel(const char* name, u32 *modelIndex)
{
	ASSERT_ONLY(fwArchetypeManager::VerifyNameClashes(name));

	Assertf(gBaseModelStore.GetCount(fwFactory::GLOBAL) < gBaseModelStore.GetCapacity(fwFactory::GLOBAL), "Reached limit of base models allowed (%d)!", gBaseModelStore.GetCapacity(fwFactory::GLOBAL));
	CBaseModelInfo *pModel = gBaseModelStore.CreateItem(fwFactory::GLOBAL);
	pModel->Init();
	pModel->SetModelName(name);
	u32 newIndex = fwArchetypeManager::RegisterPermanentArchetype(pModel, fwMapTypes::GLOBAL, false);

	if (modelIndex) {
		*modelIndex = newIndex;
	}

	return pModel;
}

CWeaponModelInfo* CModelInfo::AddWeaponModel(const char* name, bool permanent /*= true*/, s32 mapTypeDefIndex /* = fwMapTypes::GLOBAL */)
{
	ASSERT_ONLY(fwArchetypeManager::VerifyNameClashes(name));

	Assertf(gWeaponModelStore.GetCount(fwFactory::GLOBAL) < gWeaponModelStore.GetCapacity(fwFactory::GLOBAL), "Reached limit of weapon models allowed (%d)!", gWeaponModelStore.GetCapacity(fwFactory::GLOBAL));
	CWeaponModelInfo *pModel = gWeaponModelStore.CreateItem(mapTypeDefIndex);
	pModel->Init();
	pModel->SetModelName(name);

	if  (permanent)
	{
		fwArchetypeManager::RegisterPermanentArchetype(pModel, mapTypeDefIndex, true);
	}
	else
	{
		fwArchetypeManager::RegisterStreamedArchetype(pModel, mapTypeDefIndex);
	}

	char audioName[128];
	formatf(audioName, "MOD_%s", name);

	pModel->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(audioName), atStringHash(audioName));

	return pModel;
}
CVehicleModelInfo* CModelInfo::AddVehicleModel(const char* name, bool permanent /*= true*/, s32 mapTypeDefIndex /* = fwMapTypes::GLOBAL */)
{
	ASSERT_ONLY(fwArchetypeManager::VerifyNameClashes(name));

	Assertf(gVehicleModelStore.GetCount(mapTypeDefIndex) < gVehicleModelStore.GetCapacity(mapTypeDefIndex), "Reached limit of vehicle models allowed (%d)!", gVehicleModelStore.GetCapacity(mapTypeDefIndex));
	CVehicleModelInfo *pModel = gVehicleModelStore.CreateItem(mapTypeDefIndex);
	GAMETOOL_ONLY(if (pModel))
	{
		pModel->Init();
		pModel->SetModelName(name);
		if  (permanent)
		{
			fwArchetypeManager::RegisterPermanentArchetype(pModel, mapTypeDefIndex, true);
		}
		else
		{
			fwArchetypeManager::RegisterStreamedArchetype(pModel, mapTypeDefIndex);
		}
	}
	return pModel;
}


CPedModelInfo* CModelInfo::AddPedModel(const char* name, bool permanent /*= true*/, s32 mapTypeDefIndex /* = fwMapTypes::GLOBAL */)
{
	ASSERT_ONLY(fwArchetypeManager::VerifyNameClashes(name));

	// ToDo - new version of verifyNameClashes for all builds, which prevents dup'ed archetypes - because that can be fatal much later.
	u32 hash = atStringHash(name);
	if (fwArchetypeManager::GetArchetypeExistsForHashKey(hash))
	{
		modelinfoAssertf(false, "(%s) has already been registered, skipping it", name);
		return(NULL);
	}

	Assertf(gPedModelStore.GetCount(fwFactory::GLOBAL) < gPedModelStore.GetCapacity(fwFactory::GLOBAL), "Reached limit of ped models allowed (%d)!", gPedModelStore.GetCapacity(fwFactory::GLOBAL));
	CPedModelInfo *pModel = gPedModelStore.CreateItem(mapTypeDefIndex);
	pModel->Init();
	pModel->SetModelName(name);

	if  (permanent)
	{
		fwArchetypeManager::RegisterPermanentArchetype(pModel, mapTypeDefIndex, true);
	}
	else
	{
		fwArchetypeManager::RegisterStreamedArchetype(pModel, mapTypeDefIndex);
	}

	return pModel;
}

CCompEntityModelInfo* CModelInfo::AddCompEntityModel(const char* name)
{
	ASSERT_ONLY(fwArchetypeManager::VerifyNameClashes(name));

	Assertf(gCompEntityModelStore.GetCount(fwFactory::GLOBAL) < gCompEntityModelStore.GetCapacity(fwFactory::GLOBAL), "Reached limit of comp entity models allowed (%d)!", gCompEntityModelStore.GetCapacity(fwFactory::GLOBAL));
	CCompEntityModelInfo *pModel = gCompEntityModelStore.CreateItem(fwFactory::GLOBAL);
	pModel->Init();
	pModel->SetModelName(name);
	fwArchetypeManager::RegisterPermanentArchetype(pModel, fwMapTypes::GLOBAL, false);
	return pModel;
}

//
// name:		FinishAddingModels
// description:	Finalize modelinfo map
void CModelInfo::PostLoad()
{
	const char *modelName = "unknown";

/*	sysTimer t;*/

//	s32 fredFrag = g_FragmentStore.FindSlot("platform:/models/z_z_fred");
//	s32 wilmaFrag = g_FragmentStore.FindSlot("platform:/models/z_z_wilma");

	u32 maxModelInfos = fwArchetypeManager::GetMaxArchetypeIndex();
	for(u32 model= fwArchetypeManager::GetStartArchetypeIndex(); model<maxModelInfos; model++)
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(model)));

		if(pModelInfo == NULL)
			continue;

#if !__FINAL
		modelName = pModelInfo->GetModelName();
#endif //!__FINAL

		// go through each model info attaching it to a drawable dictionary, drawable or fragment store entry of the same name
		if(pModelInfo->GetDrawableType() == CBaseModelInfo::DT_DRAWABLEDICTIONARY)
		{
			ASSERT_ONLY(u32 drawDictIndex = pModelInfo->GetDrawDictIndex();)
			Assert(drawDictIndex != -1);
			Assertf(g_DwdStore.IsObjectInImage(strLocalIndex(drawDictIndex)),"model type %s (from %s.ityp) is missing drawable dictionary",pModelInfo->GetModelName(), fwArchetypeManager::LookupModelId(pModelInfo).GetTypeFileName());

			if (CStreaming::RegisterDummyObject(
				modelName, strLocalIndex(model), CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))			// note: this is NOT -1

			{
				modelinfoAssertf(false,
					"The object '%s' exists as a separate drawable file AND inside a drawable dictionary '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
					pModelInfo->GetModelName(),
					g_DwdStore.GetName(strLocalIndex(drawDictIndex))

					);
				pModelInfo->SetDrawDictFile("NULL", -1);
			}
		} 
		else if (pModelInfo->GetModelType() == MI_TYPE_COMPOSITE) 
		{
			(void)AssertVerify(CStreaming::RegisterDummyObject(modelName,	strLocalIndex(model), CModelInfo::GetStreamingModuleId()) != strIndex(strIndex::INVALID_INDEX));	// note: this is NOT -1
		} 
		else if (pModelInfo->GetDrawableType() == CBaseModelInfo::DT_FRAGMENT)
		{
			ASSERT_ONLY(u32 fragIndex = pModelInfo->GetFragmentIndex();)
			Assert(fragIndex != -1);
			Assertf(g_FragmentStore.IsObjectInImage(strLocalIndex(fragIndex)),"model type %s (from %s.ityp) is missing fragment model",pModelInfo->GetModelName(), fwArchetypeManager::LookupModelId(pModelInfo).GetTypeFileName());

			if (CStreaming::RegisterDummyObject(
				modelName,	strLocalIndex(model), CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))		// note: this is NOT -1
			{
				modelinfoAssertf(false,
					"The model '%s' exists as a separate file AND as a fragment '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
					pModelInfo->GetModelName(),
					g_FragmentStore.GetName(strLocalIndex(pModelInfo->GetFragmentIndex()))
					);
				pModelInfo->ResetAssetReference();
			}
		} 
		else if (pModelInfo->GetDrawableType() == CBaseModelInfo::DT_DRAWABLE)
		{
			ASSERT_ONLY(u32 drawableIndex = pModelInfo->GetDrawableIndex();)
			Assert(drawableIndex != -1);
			Assertf(g_DrawableStore.IsObjectInImage(strLocalIndex(drawableIndex)),"model type %s (from %s.ityp) is missing drawable model",pModelInfo->GetModelName(), fwArchetypeManager::LookupModelId(pModelInfo).GetTypeFileName());

			if (CStreaming::RegisterDummyObject(
				modelName,	strLocalIndex(model), CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))		// note: this is NOT -1
			{
				modelinfoAssertf(false,
					"The model '%s' exists as a separate file AND as a drawable '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
					pModelInfo->GetModelName(),
					g_DrawableStore.GetName(strLocalIndex(pModelInfo->GetDrawableIndex()))
					);
				pModelInfo->ResetAssetReference();
			}
		} 		 
		else if (pModelInfo->GetDrawableType() == CBaseModelInfo::DT_ASSETLESS){
			// this is ok - the archetype has been marked as having no assets - this is expected!
		} 
		else
		{
			// couldn't find anything to attach to this model info - display warning
			modelinfoWarningf("Modelinfo %s cannot attach to any streamed graphics component.",pModelInfo->GetModelName());
		}

		pModelInfo->CheckForHDTxdSlots();		// check the assets assigned to this modelinfo for HD upgrade ability
	}

// 	 	modelinfoDisplayf("%f ms in ModelInfo::PostLoad",t.GetMsTime());
// 	 	modelinfoDisplayf("");
}

//------

// mediated streaming interface
CBaseModelInfo* CModelInfo::GetModelInfoFromLocalIndex(s32 idx) 
{ 
	// find the streaming index assigned to this model info (if it has one)
	fwModelId modelId; 
	modelId.ConvertFromStreamingIndex(idx); 
	return(CModelInfo::GetBaseModelInfo(modelId)); 
}

strLocalIndex CModelInfo::AssignLocalIndexToModelInfo2(fwModelId modelId){

	// does this archetype already have an assigned streaming slot?
	CBaseModelInfo* pModelInfo = GetBaseModelInfo(modelId);
	Assert(pModelInfo);

	// valid stream slot already assigned, so just return it
	if (pModelInfo->GetStreamSlot().Get() > 0) {
		return(pModelInfo->GetStreamSlot());
	}

	CArchetypeStreamSlot::Pool* pPool = CArchetypeStreamSlot::GetPool();
	CArchetypeStreamSlot* pNewEntry = rage_new CArchetypeStreamSlot (modelId);
	Assert(pNewEntry);

	strLocalIndex streamSlot = strLocalIndex(pPool->GetJustIndex(pNewEntry));
	pModelInfo->SetTempStreamSlot((u8)streamSlot.Get());			// only 256 of these for now!

	const char *modelName = "unknown";
#if !__FINAL
		modelName = pModelInfo->GetModelName();
#endif //!__FINAL

	if (CStreaming::RegisterDummyObject(
		modelName, streamSlot, CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))			// note: this is NOT -1
	{
		modelinfoAssertf(false,
		"Failed to register slot %d to model %s during local index assignment\n", streamSlot.Get(), pModelInfo->GetModelName());
		pModelInfo->ResetAssetReference();
		ReleaseAssignedLocalIndex(modelId);
		streamSlot = 0;
	}

	return(streamSlot);
}

void CModelInfo::ReleaseAssignedLocalIndex(fwModelId modelId){

	CBaseModelInfo* pModelInfo = GetBaseModelInfo(modelId);
	Assert(pModelInfo);

	strLocalIndex streamSlot = pModelInfo->GetStreamSlot();
	if (streamSlot == 0){
		return;
	}

	//u32 streamIndex = lookup(StreamSlot);
	//CStreamingInfo::UnregisterObject(streamIndex);

	CArchetypeStreamSlot::Pool* pPool = CArchetypeStreamSlot::GetPool();
	CArchetypeStreamSlot* pEntry = pPool->GetSlot(streamSlot.Get());

	delete pEntry;

	pModelInfo->InvalidateStreamSlot();
}

void CModelInfo::UpdateArchetypeStreamSlots(void)
{
	CArchetypeStreamSlot::Update();			// inc timer so anything allocated this frame isn't immediately recycled

	CArchetypeStreamSlot::Pool* pPool = CArchetypeStreamSlot::GetPool();
	Assert(pPool);
	
	CArchetypeStreamSlot* pArchetypeStreamSlot = NULL;
	s32 poolSize=pPool->GetSize();
	s32 offset = 0;

	while(offset < poolSize)
	{
		pArchetypeStreamSlot = CArchetypeStreamSlot::GetPool()->GetSlot(offset);
		if (pArchetypeStreamSlot && pArchetypeStreamSlot->IsDueForRecycling()){
 			fwModelId modelId = pArchetypeStreamSlot->GetModelId();
			ReleaseAssignedLocalIndex(modelId);
// 			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
// 			Assert(pModelInfo);
// 			pModelInfo->SetStreamSlot(0);
// 			delete pArchetypeStreamSlot;
		}
		offset++;
	}
}

bool CModelInfo::HaveAssetsLoaded(fwModelId modelId)
{
	return(CStreaming::HasObjectLoaded(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId()));
}

bool CModelInfo::HaveAssetsLoaded(atHashString modelName)
{
	CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelName, NULL);
	return HaveAssetsLoaded(modelInfo);
}

bool CModelInfo::HaveAssetsLoaded(CBaseModelInfo* pModelInfo)
{
	return(CStreaming::HasObjectLoaded(pModelInfo->GetStreamSlot(), CModelInfo::GetStreamingModuleId()));
}

bool CModelInfo::AreAssetsLoading(fwModelId modelId)
{
	return(CModelInfo::GetStreamingModule()->IsObjectLoading(modelId.ConvertToStreamingIndex()));
}

bool CModelInfo::AreAssetsRequested(fwModelId modelId)
{
	return(CModelInfo::GetStreamingModule()->IsObjectRequested(modelId.ConvertToStreamingIndex()));
}

bool CModelInfo::RequestAssets(fwModelId modelId, u32 streamFlags)
{
	return(CStreaming::RequestObject(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId(), streamFlags));
}

bool CModelInfo::RequestAssets(CBaseModelInfo* pModelInfo, u32 streamFlags)
{
	return(CStreaming::RequestObject(pModelInfo->GetStreamSlot(), CModelInfo::GetStreamingModuleId(), streamFlags));
}

void CModelInfo::SetAssetsAreDeletable(fwModelId modelId)
{
	CStreaming::SetObjectIsDeletable(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId());
}

bool CModelInfo::GetAssetsAreDeletable(fwModelId modelId)
{
	return(CStreaming::GetObjectIsDeletable(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId()));
}

u32 CModelInfo::GetAssetStreamFlags(fwModelId modelId)
{
	return(CStreaming::GetObjectFlags(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId()));
}

void CModelInfo::RemoveAssets(fwModelId modelId)	
{
	CStreaming::RemoveObject(modelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId());
}

bool CModelInfo::GetAssetRequiredFlag(fwModelId modelId)
{
	return (CModelInfo::GetStreamingModule()->IsObjectRequired(modelId.ConvertToStreamingIndex()));   
}

void CModelInfo::SetAssetRequiredFlag(fwModelId modelId, u32 streamFlags)
{
	CModelInfo::GetStreamingModule()->SetRequiredFlag(modelId.ConvertToStreamingIndex().Get(), streamFlags);
}

void CModelInfo::ClearAssetRequiredFlag(fwModelId modelId,u32 streamFlags)
{
	CModelInfo::GetStreamingModule()->ClearRequiredFlag(modelId.ConvertToStreamingIndex().Get(), streamFlags);
}

void CModelInfo::GetObjectAndDependencies(fwModelId modelId, atArray<strIndex>& allDeps, const strIndex* ignoreList, s32 numIgnores)
{
	strLocalIndex streamingIndex = CModelInfo::LookupLocalIndex(modelId);
	if (streamingIndex == CModelInfo::INVALID_LOCAL_INDEX){
		return;
	}

	CStreaming::GetObjectAndDependencies(streamingIndex, CModelInfo::GetStreamingModuleId(), allDeps, ignoreList, numIgnores);

}

void CModelInfo::GetObjectAndDependenciesSizes(fwModelId modelId, u32& virtualSize, u32& physicalSize, const strIndex* ignoreList, s32 numIgnores, bool mayFail)
{
	strLocalIndex streamingIndex = CModelInfo::LookupLocalIndex(modelId);
	if (streamingIndex == CModelInfo::INVALID_LOCAL_INDEX){
		return;
	}

	CStreaming::GetObjectAndDependenciesSizes(streamingIndex, CModelInfo::GetStreamingModuleId(), virtualSize, physicalSize, ignoreList, numIgnores, mayFail);

}

void CModelInfo::SetGlobalResidentTxd(int txd)
{
	// Whizz through the txd, look for const char *name & const grcTexture *texture
	fwTxd *pTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(txd));
	for (int i=0; i<pTxd->GetCount(); i++)
	{
		rage::grcTexture *pTexture = pTxd->GetEntry(i);
		const char *pName = pTexture->GetName();
		grcTextureFactory::GetInstance().RegisterTextureReference(pName, pTexture);
	}
}

// -----


//
// name:		PostLoad
// description:	Finalize this modelinfo (fix up any required ptrs)
void CModelInfo::PostLoad(fwArchetype* pArchetype)
{
	const char *modelName = "unknown";

	CBaseModelInfo* pMI = smart_cast<CBaseModelInfo*>(pArchetype);

#if !__FINAL
	modelName = pMI->GetModelName();
#endif //!__FINAL

	strLocalIndex streamId = pMI->GetStreamSlot();
	Assert(streamId != fwArchetypeManager::INVALID_STREAM_SLOT);

	// go through each model info attaching it to a drawable dictionary, drawable or fragment store entry of the same name
	if(pMI->GetDrawableType() == CBaseModelInfo::DT_DRAWABLEDICTIONARY)
	{
		ASSERT_ONLY(u32 drawDictIndex = pMI->GetDrawDictIndex();)
			Assert(drawDictIndex != -1);
		Assertf(g_DwdStore.IsObjectInImage(strLocalIndex(drawDictIndex)),"model type %s (from %s.ityp) is missing drawable dictionary",pMI->GetModelName(), fwArchetypeManager::LookupModelId(pMI).GetTypeFileName());

		if (CStreaming::RegisterDummyObject(
			modelName, streamId, CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))			// note: this is NOT -1
		{
			modelinfoAssertf(false,
				"The object '%s' exists as a separate drawable file AND inside a drawable dictionary '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
				pMI->GetModelName(),
				g_DwdStore.GetName(strLocalIndex(drawDictIndex))
			);
			pMI->SetDrawDictFile("NULL", -1);
		}
	} 
	else if (pMI->GetModelType() == MI_TYPE_COMPOSITE) 
	{
		(void)AssertVerify(CStreaming::RegisterDummyObject(modelName,	streamId, CModelInfo::GetStreamingModuleId()) != strIndex(strIndex::INVALID_INDEX));	// note: this is NOT -1
	} 
	else if (pMI->GetDrawableType() == CBaseModelInfo::DT_FRAGMENT)
	{
		ASSERT_ONLY(u32 fragIndex = pMI->GetFragmentIndex();)
		Assert(fragIndex != -1);
		Assertf(g_FragmentStore.IsObjectInImage(strLocalIndex(fragIndex)),"model type %s (from %s.ityp) is missing fragment",pMI->GetModelName(), fwArchetypeManager::LookupModelId(pMI).GetTypeFileName());

		if (CStreaming::RegisterDummyObject(
			modelName,	streamId, CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))		// note: this is NOT -1
		{
			modelinfoAssertf(false,
				"The model '%s' exists as a separate file AND as a fragment '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
				pMI->GetModelName(),
				g_FragmentStore.GetName(strLocalIndex(pMI->GetFragmentIndex()))
			);
			pMI->ResetAssetReference();
		}
	} 
	else if (pMI->GetDrawableType() == CBaseModelInfo::DT_DRAWABLE)
	{
		ASSERT_ONLY(u32 drawableIndex = pMI->GetDrawableIndex();)
		Assert(drawableIndex != -1);
		Assertf(g_DrawableStore.IsObjectInImage(strLocalIndex(drawableIndex)),"model type %s (from %s.ityp) is missing drawable",pMI->GetModelName(), fwArchetypeManager::LookupModelId(pMI).GetTypeFileName());

		if (CStreaming::RegisterDummyObject(
			modelName,	streamId, CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))		// note: this is NOT -1
		{
			modelinfoAssertf(false,
				"The model '%s' exists as a separate file AND as a drawable '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.\n",
				pMI->GetModelName(),
				g_DrawableStore.GetName(strLocalIndex(pMI->GetDrawableIndex()))
				);
			pMI->ResetAssetReference();
		}
	} 		
	else if (pMI->GetDrawableType() == CBaseModelInfo::DT_ASSETLESS){
		// this is ok - the archetype has been marked as having no assets - this is expected!

		if (CStreaming::RegisterDummyObject(
			modelName,	streamId, CModelInfo::GetStreamingModuleId()) == strIndex(strIndex::INVALID_INDEX))		// note: this is NOT -1
		{
			modelinfoAssertf(false,
				"The model '%s' exists as a separate file AND as a drawable '%s'  - the name clash will lead to crashes later on! This needs to be fixed before commiting this file.",
				pMI->GetModelName(),
				g_DrawableStore.GetName(strLocalIndex(pMI->GetDrawableIndex()))
				);
			pMI->ResetAssetReference();
		}
	} 
	else
	{
#if __DEV
		// couldn't find anything to attach to this model info - display warning
		modelinfoWarningf("Modelinfo %s cannot attach to any streamed graphics component.",pMI->GetModelName());
#endif //__DEV
	}

	// ensure that this archetype doesn't have an existing physics bound loaded - it can't be handled yet!

	pMI->CheckForHDTxdSlots();		// check the assets assigned to this modelinfo for HD upgrade ability
}

void	CModelInfo::UpdateVehicleClassInfos()
{
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::UpdateVehicleClassInfo);
}

#if __BANK
void	CModelInfo::UpdateVehicleHandlingInfos()
{
	gVehicleModelStore.ForAllItemsUsed(&CVehicle::UpdateVehicleHandlingInfo);
}
#endif // __BANK

#if __DEV
void	CModelInfo::CheckMissingVehicleColours()
{
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::CheckMissingColour);
}
#endif

#if __DEV
void	CModelInfo::UpdateVfxPedInfos()
{
	gPedModelStore.ForAllItemsUsed(&CVfxPedInfoMgr::UpdatePedModelInfo);
}
#endif

#if __DEV
void	CModelInfo::UpdateVfxVehicleInfos()
{
	gVehicleModelStore.ForAllItemsUsed(&CVfxVehicleInfoMgr::UpdateVehicleModelInfo);
}
#endif

#if __BANK
void CModelInfo::CheckVehicleAssetDependencies()
{
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::CheckDependenciesAreLoaded);
}

void CModelInfo::DumpVehicleModelInfos()
{
	CVehicleModelInfo::ms_numLoadedInfos = 0;
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::DumpVehicleModelInfos);
	Displayf("Num loaded infos: %d\n", CVehicleModelInfo::ms_numLoadedInfos);
}

enum eDebugFloatCompareType
{
    DFCT_MAIN,
    DFCT_VRAM,
    DFCT_MAIN_HD,
    DFCT_VRAM_HD,
    DFCT_COMBINED,
    DFCT_COMBINED_HD,
};
eDebugFloatCompareType g_compareType;
int DebugFloatCompare(sDebugSize const* lhs, sDebugSize const* rhs)
{
    switch (g_compareType)
    {
        case DFCT_MAIN:
            return lhs->main -  rhs->main;
            break;
        case DFCT_VRAM:
            return lhs->vram -  rhs->vram;
            break;
        case DFCT_MAIN_HD:
            return lhs->mainHd -  rhs->mainHd;
            break;
        case DFCT_VRAM_HD:
            return lhs->vramHd -  rhs->vramHd;
            break;
        case DFCT_COMBINED:
            return (lhs->main + lhs->vram) -  (rhs->main + rhs->vram);
            break;
        case DFCT_COMBINED_HD:
            return (lhs->mainHd + lhs->vramHd) -  (rhs->mainHd + rhs->vramHd);
            break;
    }

    Assert(false);
	return 0;
}

void CalculateAverageAndMedianSizes(atArray<sDebugSize>& sizes, u32& totalMain, u32& totalVram, u32& totalMainHd, u32& totalVramHd, u32& count, u32& countHd, u32& medianMain, u32& medianVram, u32& medianMainHd, u32& medianVramHd, u32& combinedMedian, u32& combinedMedianHd)
{
	totalMain = totalVram = totalMainHd = totalVramHd = count = countHd = medianMain = medianVram = medianMainHd = medianVramHd = combinedMedian = combinedMedianHd = 0;

    count = sizes.GetCount();
	if (count > 0)
	{
		for (s32 i = 0; i < count; ++i)
        {
			totalMain += sizes[i].main;
			totalVram += sizes[i].vram;

            if (sizes[i].mainHd > 0 || sizes[i].vramHd > 0)
                countHd++;

			totalMainHd += sizes[i].mainHd;
			totalVramHd += sizes[i].vramHd;
        }

        g_compareType = DFCT_MAIN;
		sizes.QSort(0, -1, DebugFloatCompare);
		medianMain = sizes[count >> 1].main;

        g_compareType = DFCT_VRAM;
		sizes.QSort(0, -1, DebugFloatCompare);
		medianVram = sizes[count >> 1].vram;

        g_compareType = DFCT_MAIN_HD;
		sizes.QSort(0, -1, DebugFloatCompare);
		medianMainHd = sizes[count >> 1].mainHd;

        g_compareType = DFCT_VRAM_HD;
		sizes.QSort(0, -1, DebugFloatCompare);
		medianVramHd = sizes[count >> 1].vramHd;

        g_compareType = DFCT_COMBINED;
		sizes.QSort(0, -1, DebugFloatCompare);
		combinedMedian = sizes[count >> 1].main + sizes[count >> 1].vram;

        g_compareType = DFCT_COMBINED_HD;
		sizes.QSort(0, -1, DebugFloatCompare);
		combinedMedianHd = sizes[count >> 1].mainHd + sizes[count >> 1].vramHd;
    }
}

void CModelInfo::DumpAverageVehicleSize()
{
    CVehicleModelInfo::ms_vehicleSizes.Reset();
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::DumpAverageVehicleSize);

	u32 totalMain = 0;
	u32 totalVram = 0;
	u32 totalMainHd = 0;
	u32 totalVramHd = 0;
	u32 count = 0;
	u32 countHd = 0;
	u32 medianMain = 0;
	u32 medianVram = 0;
    u32 medianMainHd = 0;
    u32 medianVramHd = 0;
	u32 combinedMedian = 0;
	u32 combinedMedianHd = 0;
	CalculateAverageAndMedianSizes(CVehicleModelInfo::ms_vehicleSizes, totalMain, totalVram, totalMainHd, totalVramHd, count, countHd, medianMain, medianVram, medianMainHd, medianVramHd, combinedMedian, combinedMedianHd);

	u32 total = totalMain + totalVram;
    u32 totalHd = totalMainHd + totalVramHd;

	Displayf("Total vehicle models: %d - average size: %d (m%d - v%d) - average hd: %d (m%d - v%d) - median size: %d (m%d - v%d) - median hd: %d (m%d - v%d)\n", count,
		total / count, totalMain / count, totalVram / count, totalHd / countHd, totalMainHd / countHd, totalVramHd / countHd,
		combinedMedian, medianMain, medianVram, combinedMedianHd, medianMainHd, medianVramHd);


	FileHandle file = CFileMgr::OpenFileForWriting("vehicle_sizes.xls");

    if (CFileMgr::IsValidFileHandle(file))
    {
        g_compareType = DFCT_COMBINED;
		CVehicleModelInfo::ms_vehicleSizes.QSort(0, -1, DebugFloatCompare);

        char line[256];
        formatf(line, "Total vehicle models: %d - average size: %d (m%d - v%d) - average hd: %d (m%d - v%d) - median size: %d (m%d - v%d) - median hd: %d (m%d - v%d)\n\n", count,
		total / count, totalMain / count, totalVram / count, totalHd / countHd, totalMainHd / countHd, totalVramHd / countHd,
		combinedMedian, medianMain, medianVram, combinedMedianHd, medianMainHd, medianVramHd);
        CFileMgr::Write(file, line, istrlen(line));

        formatf(line, "Model, Main, Vram, Main Hd, Vram Hd\n");
        CFileMgr::Write(file, line, istrlen(line));

        for (s32 i = 0; i < count; ++i)
        {
            formatf(line, "%s\t%d\t%d\t%d\t%d\n",
                    CVehicleModelInfo::ms_vehicleSizes[i].name,
                    CVehicleModelInfo::ms_vehicleSizes[i].main,
                    CVehicleModelInfo::ms_vehicleSizes[i].vram,
                    CVehicleModelInfo::ms_vehicleSizes[i].mainHd,
                    CVehicleModelInfo::ms_vehicleSizes[i].vramHd);
            CFileMgr::Write(file, line, istrlen(line));
        }

        CFileMgr::CloseFile(file);
    }

    CVehicleModelInfo::ms_vehicleSizes.Reset();
}

void CModelInfo::DumpAveragePedSize()
{
	CPedModelInfo::ms_pedSizes.Reset();
	gPedModelStore.ForAllItemsUsed(&CPedModelInfo::DumpAveragePedSize);

	u32 totalMain = 0;
	u32 totalVram = 0;
	u32 totalMainHd = 0;
	u32 totalVramHd = 0;
	u32 count = 0;
	u32 countHd = 0;
	u32 medianMain = 0;
	u32 medianVram = 0;
    u32 medianMainHd = 0;
    u32 medianVramHd = 0;
	u32 combinedMedian = 0;
	u32 combinedMedianHd = 0;
	CalculateAverageAndMedianSizes(CPedModelInfo::ms_pedSizes, totalMain, totalVram, totalMainHd, totalVramHd, count, countHd, medianMain, medianVram, medianMainHd, medianVramHd, combinedMedian, combinedMedianHd);

	u32 total = totalMain + totalVram;
    u32 totalHd = totalMainHd + totalVramHd;

	Displayf("Total (pack) ped models: %d - average size: %d (m%d - v%d) - average hd: %d (m%d - v%d) - median size: %d (m%d - v%d) - median hd: %d (m%d - v%d)\n", count,
		total / count, totalMain / count, totalVram / count, totalHd / countHd, totalMainHd / countHd, totalVramHd / countHd,
		combinedMedian, medianMain, medianVram, combinedMedianHd, medianMainHd, medianVramHd);

	FileHandle file = CFileMgr::OpenFileForWriting("ped_sizes.xls");

    if (CFileMgr::IsValidFileHandle(file))
    {
        g_compareType = DFCT_COMBINED;
		CPedModelInfo::ms_pedSizes.QSort(0, -1, DebugFloatCompare);

        char line[256];
        formatf(line, "Total (pack) ped models: %d - average size: %d (m%d - v%d) - average hd: %d (m%d - v%d) - median size: %d (m%d - v%d) - median hd: %d (m%d - v%d)\n\n", count,
		total / count, totalMain / count, totalVram / count, totalHd / countHd, totalMainHd / countHd, totalVramHd / countHd,
		combinedMedian, medianMain, medianVram, combinedMedianHd, medianMainHd, medianVramHd);
        CFileMgr::Write(file, line, istrlen(line));

        formatf(line, "Model, Main, Vram, Main Hd, Vram Hd\n");
        CFileMgr::Write(file, line, istrlen(line));

        for (s32 i = 0; i < count; ++i)
        {
            formatf(line, "%s\t%d\t%d\t%d\t%d\n",
                    CPedModelInfo::ms_pedSizes[i].name,
                    CPedModelInfo::ms_pedSizes[i].main,
                    CPedModelInfo::ms_pedSizes[i].vram,
                    CPedModelInfo::ms_pedSizes[i].mainHd,
                    CPedModelInfo::ms_pedSizes[i].vramHd);
            CFileMgr::Write(file, line, istrlen(line));
        }

        CFileMgr::CloseFile(file);
    }

	CPedModelInfo::ms_pedSizes.Reset();
}

void CModelInfo::DebugRecalculateAllVehicleCoverPoints()
{
	gVehicleModelStore.ForAllItemsUsed(&CVehicleModelInfo::DebugRecalculateVehicleCoverPoints);
}

#endif // __BANK

void	CModelInfo::ClearAllCountedAsScenarioPedFlags()
{
	gPedModelStore.ForAllItemsUsed(&CPedModelInfo::ClearCountedAsScenarioPed);
}

void CModelInfo::RegisterStreamingModule()
{
	static CModelInfoStreamingModule modelinfoStreamingModule;
	fwArchetypeManager::SetStreamingModuleId(strStreamingEngine::GetInfo().GetModuleMgr().AddModule(&modelinfoStreamingModule));
}

#if DEBUGINFO_2DFX

void CModelInfo::DebugInfo2dEffect()
{
	s32 size = sizeof(C2dEffect);

	//model effects

	s32 i,used = g2dEffectStore.GetItemsUsed();

	s32 CurrentSize = used * size;
	s32 RealSize = 0;

	s32 SizeHeader = sizeof(Vector3) + sizeof(u32);

	s32 SizeLightStruct = sizeof(CLightAttr);
	s32 SizeParticleStruct = sizeof(CParticleAttr);
	s32 SizeExplosionStruct = sizeof(CExplosionAttr);
	s32 SizeDecalStruct = sizeof(CDecalAttr);
	s32 SizeWindDisturbanceStruct = sizeof(CWindDisturbanceAttr);
	s32 SizeProceduralStruct = sizeof(CProcObjAttr);
	s32 SizeCoordScriptStruct = sizeof(CWorldPointAttr);
	s32 SizeLadderStruct = sizeof(CLadderInfo);
	s32 SizeAudioEffectStruct = sizeof(CAudioAttr);
	s32 SizeSpawnPointStruct = sizeof(CSpawnPoint);
	s32 SizeLightsShaftStruct = sizeof(CLightShaftAttr);
	s32 SizeScrollBarStruct = sizeof(CScrollBarAttr);
	s32 SizeSwayableStruct = sizeof(CSwayableAttr);
	s32 SizeBuoyancyStruct = sizeof(CBuoyancyAttr);

	s32 NumLights = 0;
	s32 NumParticles = 0;
	s32 NumExplosions = 0;
	s32 NumDecals = 0;
	s32 NumWindDisturbances = 0;
	s32 NumEscalators = 0;
	s32 NumProcedurals = 0;
	s32 NumCoordScripts = 0;
	s32 NumLadders = 0;
	s32 NumAudioEffects = 0;
	s32 NumSpawnPoints = 0;
	s32 NumLightsShafts = 0;
	s32 NumScrollBars = 0;
	s32 NumSwayables = 0;
	s32 NumBuoyancys = 0;
	s32 NumWalkDontWalks = 0;

	for(i=0;i<used;i++)
	{
		switch((g2dEffectStore[i])->GetType())
		{
		case ET_LIGHT:
			NumLights++;
			break;
		case ET_PARTICLE:
			NumParticles++;
			break;
		case ET_EXPLOSION:
			NumExplosions++;
			break;
		case ET_DECAL:
			NumDecals++;
			break;
		case ET_WINDDISTURBANCE:
			NumWindDisturbances++;
			break;
		case ET_PROCEDURALOBJECTS:
			NumProcedurals++;
			break;
		case ET_COORDSCRIPT:
			NumCoordScripts++;
			break;
		case ET_LADDER:
			NumLadders++;
			break;
		case ET_AUDIOEFFECT:
			NumAudioEffects++;
			break;
		case ET_SPAWN_POINT:
			NumSpawnPoints++;
			break;
		case ET_LIGHT_SHAFT:
			NumLightsShafts++;
			break;
		case ET_SCROLLBAR:
			NumScrollBars++;
			break;
		case ET_BUOYANCY:
			NumBuoyancys++;
			break;
		}
	}

	RealSize = 0;

	RealSize += (sizeof(CLightAttr) + SizeHeader) * NumLights;
	RealSize += (sizeof(CParticleAttr) + SizeHeader) * NumParticles;
	RealSize += (sizeof(CExplosionAttr) + SizeHeader) * NumExplosions;
	RealSize += (sizeof(CDecalAttr) + SizeHeader) * NumDecals;
	RealSize += (sizeof(CWindDisturbanceAttr) + SizeHeader) * NumWindDisturbances;
	RealSize += (sizeof(CProcObjAttr) + SizeHeader) * NumProcedurals;
	RealSize += (sizeof(CWorldPointAttr) + SizeHeader) * NumCoordScripts;
	RealSize += (sizeof(CLadderInfo) + SizeHeader) * NumLadders;
	RealSize += (sizeof(CAudioAttr) + SizeHeader) * NumAudioEffects;
	RealSize += (sizeof(CSpawnPoint) + SizeHeader) * NumSpawnPoints;
	RealSize += (sizeof(CLightShaftAttr) + SizeHeader) * NumLightsShafts;
	RealSize += (sizeof(CScrollBarAttr) + SizeHeader) * NumScrollBars;
	RealSize += (sizeof(CSwayableAttr) + SizeHeader) * NumSwayables;
	RealSize += (sizeof(CBuoyancyAttr) + SizeHeader) * NumBuoyancys;

	//world effects
	used = ms_World2dEffectArray.GetCount();

	CurrentSize = used * size;
	RealSize = 0;

	SizeHeader = sizeof(Vector3) + sizeof(u32);

	SizeLightStruct = sizeof(CLightAttr);
	SizeParticleStruct = sizeof(CParticleAttr);
	SizeExplosionStruct = sizeof(CExplosionAttr);
	SizeDecalStruct = sizeof(CDecalAttr);
	SizeWindDisturbanceStruct = sizeof(CWindDisturbanceAttr);
	SizeProceduralStruct = sizeof(CProcObjAttr);
	SizeCoordScriptStruct = sizeof(CWorldPointAttr);
	SizeLadderStruct = sizeof(CLadderInfo);
	SizeAudioEffectStruct = sizeof(CAudioAttr);
	SizeSpawnPointStruct = sizeof(CSpawnPoint);
	SizeLightsShaftStruct = sizeof(CLightShaftAttr);
	SizeScrollBarStruct = sizeof(CScrollBarAttr);
	SizeSwayableStruct = sizeof(CSwayableAttr);
	SizeBuoyancyStruct = sizeof(CBuoyancyAttr);

	NumLights = 0;
	NumParticles = 0;
	NumExplosions = 0;
	NumDecals = 0;
	NumWindDisturbances = 0;
	NumEscalators = 0;
	NumProcedurals = 0;
	NumCoordScripts = 0;
	NumLadders = 0;
	NumAudioEffects = 0;
	NumSpawnPoints = 0;
	NumLightsShafts = 0;
	NumScrollBars = 0;
	NumSwayables = 0;
	NumBuoyancys = 0;
	NumWalkDontWalks = 0;

	for(i=0;i<used;i++)
	{
		switch(ms_World2dEffectArray[i]->GetType())
		{
		case ET_LIGHT:
			NumLights++;
			break;
		case ET_PARTICLE:
			NumParticles++;
			break;
		case ET_EXPLOSION:
			NumExplosions++;
			break;
		case ET_DECAL:
			NumDecals++;
			break;
		case ET_WINDDISTURBANCE:
			NumWindDisturbances++;
			break;
		case ET_PROCEDURALOBJECTS:
			NumProcedurals++;
			break;
		case ET_COORDSCRIPT:
			NumCoordScripts++;
			break;
		case ET_LADDER:
			NumLadders++;
			break;
		case ET_AUDIOEFFECT:
			NumAudioEffects++;
			break;
		case ET_SPAWN_POINT:
			NumSpawnPoints++;
			break;
		case ET_LIGHT_SHAFT:
			NumLightsShafts++;
			break;
		case ET_SCROLLBAR:
			NumScrollBars++;
			break;
		case ET_BUOYANCY:
			NumBuoyancys++;
			break;
		case ET_WALKDONTWALK:
			NumWalkDontWalks++;
			break;
		}
	}

	RealSize = 0;

	RealSize += (sizeof(CLightAttr) + SizeHeader) * NumLights;
	RealSize += (sizeof(CParticleAttr) + SizeHeader) * NumParticles;
	RealSize += (sizeof(CExplosionAttr) + SizeHeader) * NumExplosions;
	RealSize += (sizeof(CDecalAttr) + SizeHeader) * NumDecals;
	RealSize += (sizeof(CWindDisturbanceAttr) + SizeHeader) * NumWindDisturbances;
	RealSize += (sizeof(CProcObjAttr) + SizeHeader) * NumProcedurals;
	RealSize += (sizeof(CWorldPointAttr) + SizeHeader) * NumCoordScripts;
	RealSize += (sizeof(CLadderInfo) + SizeHeader) * NumLadders;
	RealSize += (sizeof(CAudioAttr) + SizeHeader) * NumAudioEffects;
	RealSize += (sizeof(CSpawnPoint) + SizeHeader) * NumSpawnPoints;
	RealSize += (sizeof(CLightShaftAttr) + SizeHeader) * NumLightsShafts;
	RealSize += (sizeof(CScrollBarAttr) + SizeHeader) * NumScrollBars;
	RealSize += (sizeof(CSwayableAttr) + SizeHeader) * NumSwayables;
	RealSize += (sizeof(CBuoyancyAttr) + SizeHeader) * NumBuoyancys;
}

#endif //DEBUGINFO_2DFX

