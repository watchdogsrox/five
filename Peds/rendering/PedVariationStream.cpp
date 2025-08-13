//
// PedVariationStream.cpp
//
// Handles the automatic processing & preparation of ped models to generate multiple variations from
// the source dictionaries. Models and textures are loaded per instance to minimize cost for low count instances (ie. the player or cutscene characters)
//
// 2009/12/01 - JohnW: splitted from PedVariationMgr.cpp;
//
//
//
#include "Peds/rendering/PedVariationStream.h"


// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/button.h"
#include "bank/group.h"
#include "bank/slider.h"
#include "creature/componentskeleton.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletonData.h"
#include "data/callback.h"
#include "diag/art_channel.h"
#include "file/device.h"
#include "file/device_relative.h"
#include "file/diskcache.h"
#include "file/packfile.h"
#include "fragment/cache.h"
#include "fragment/drawable.h"
#include "fragment/tune.h"
#include "grcore/debugdraw.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/texturereference.h"
#include "grcore/image.h"
#include "grmodel/matrixset.h"
#include "rmcore/drawable.h"
#include "grmodel/geometry.h"
#include "grmodel/shaderFx.h"
#include "grmodel/shaderGroup.h"
#include "vectormath/legacyconvert.h"
#include "system/stack.h"

// fw headers
#include "fwutil/keyGen.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/clothstore.h"
#include "fwsys/fileExts.h"
#include "fwpheffects/clothmanager.h"
#include "fwrenderer/renderlistbuilder.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminglive.h"
#include "streaming/streamingvisualize.h"

//game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Debug/DebugGlobals.h"
#include "Debug/DebugScene.h"
#include "frontend/MobilePhone.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/NetworkInterface.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "peds/ped_channel.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedFactory.h"
#include "Peds/PedPopulation.h"
#include "Peds/pedType.h"
#include "Peds/Ped.h"
#include "peds/rendering/alternatevariations_parser.h"
#include "peds/rendering/PedProps.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/rendering/PedVariationPack.h"
#include "Physics/gtaInst.h"
#include "Physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/DrawLists/drawList.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "renderer/PostProcessFXHelper.h"
#include "Renderer/renderer.h"
#include "renderer/RenderThread.h"
#include "shaders/ShaderLib.h"
#include "Script/script.h"
#include "Streaming/streaming.h"
#include "Streaming/streamingmodule.h"
#include "scene/ContinuityMgr.h"
#include "scene/datafilemgr.h"
#include "scene/fileLoader.h"
#include "scene/RegdRefTypes.h"
#include "scene/texlod.h"
#include "scene/world/GameWorld.h"
#include "streaming/populationstreaming.h"
#include "system/filemgr.h"
#include "System/timer.h"
#include "Task\General\TaskBasic.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task\Vehicle\TaskCarAccessories.h"
#include "Tools/smokeTest.h"
#include "Vehicles/VehicleFactory.h"
#include "shaders/ShaderHairSort.h"
#include "scene/ExtraMetadataMgr.h"

//local header
#include "shaders/CustomShaderEffectPed.h"
#include "system/param.h"

#include "scene/dlc_channel.h"

#if __BANK
#include "peds/PedDebugVisualiser.h"
#endif //__BANK

AI_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPedStreamRequestGfx, 45, 0.45f, atHashString("StreamPed req data",0xab968f6));
#if RSG_ORBIS || RSG_DURANGO || RSG_PC
#define MAX_PED_RENDER_GFX (100)		// more slots for increased MP max players
#else
#define MAX_PED_RENDER_GFX (42)
#endif
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPedStreamRenderGfx, MAX_PED_RENDER_GFX, 0.41f, atHashString("StreamPed render data",0x2d370c34));
AUTOID_IMPL(CPedStreamRenderGfx);
AUTOID_IMPL(CPedStreamRequestGfx);


PARAM(rendergfxdebug, "temp render gfx crash debug");

#if __DEV
// ugly but an assert with the offending ped before a horrible crash will probably save some time
#define NUM_BLEND_PEDS (8)
const char* s_mpBlendPeds[NUM_BLEND_PEDS] = { "mp_f_cop_01", "mp_m_cop_01", "mp_g_f_biker_01", "mp_g_m_biker_01", "mp_g_f_vagos_01", "mp_g_m_vagos_01", "mp_m_freemode_01", "mp_f_freemode_01" };
#endif // __DEV

bool CPedVariationStream::m_bUseStreamPedFile = false;
fiDevice* CPedVariationStream::m_pDevice = NULL;
s32	CPedVariationStream::m_BSTexGroupId = -1;
bool CPedVariationStream::ms_aDrawPlayerComp[MAX_DRAWBL_VARS];
CPedFacialOverlays CPedVariationStream::ms_facialOverlays;
CPedSkinTones CPedVariationStream::ms_skinTones;
CAlternateVariations CPedVariationStream::ms_alternateVariations;
FirstPersonAssetDataManager CPedVariationStream::ms_firstPersonData;
CPedVariationStream::FirstPersonPropData CPedVariationStream::ms_firstPersonProp[CPedVariationStream::MAX_FP_PROPS];
FirstPersonAlternateData CPedVariationStream::ms_firstPersonAlternateData;
MpTintData CPedVariationStream::ms_mpTintData;
bool CPedVariationStream::ms_animPostFXEventTriggered = false;

atArray<CPedStreamRenderGfx*> CPedStreamRenderGfx::ms_renderRefs[FENCE_COUNT];
s32 CPedStreamRenderGfx::ms_fenceUpdateIdx = 0;

class CPedVariationStreamFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CPedVariationStreamFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
			case CDataFileMgr::PED_FIRST_PERSON_ALTERNATE_DATA: CPedVariationStream::AddFirstPersonAlternateData(file.m_filename); break;
			case CDataFileMgr::PED_FIRST_PERSON_ASSET_DATA: CPedVariationStream::AddFirstPersonData(file.m_filename); break;
			case CDataFileMgr::ALTERNATE_VARIATIONS_FILE: CPedVariationStream::AddAlternateVariations(file.m_filename); break;
			case CDataFileMgr::PEDSTREAM_FILE: CPedVariationStream::RegisterStreamPedFile(&file); break;
			default: return false;
		}

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		dlcDebugf3("CPedVariationStreamFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);

		switch(file.m_fileType)
		{
		case CDataFileMgr::PED_FIRST_PERSON_ASSET_DATA: CPedVariationStream::RemoveFirstPersonData(file.m_filename); break;
		case CDataFileMgr::ALTERNATE_VARIATIONS_FILE: CPedVariationStream::RemoveAlternateVariations(file.m_filename); break;
			case CDataFileMgr::PEDSTREAM_FILE: break;
			default: break;
		}
	}

} g_PedVariationStreamFileMounter;

atHashString CPedSkinTones::GetSkinToneTexture(u32 comp, u32 id, bool male) const
{
    atHashString skinTex;

	u32 localId = id * 2;
    if (!male)
        localId++;

	if (localId < m_males.GetCount())
		skinTex = m_comps[comp].m_skin[m_males[localId]];
	else
	{
		localId -= m_males.GetCount();
		if (localId < m_females.GetCount())
			skinTex = m_comps[comp].m_skin[m_females[localId]];
		else
		{
			localId -= m_females.GetCount();
			if (localId < m_uniqueMales.GetCount())
				skinTex = m_comps[comp].m_skin[m_uniqueMales[localId]];
			else
			{
				localId -= m_uniqueMales.GetCount();
				if (localId < m_uniqueFemales.GetCount())
					skinTex = m_comps[comp].m_skin[m_uniqueFemales[localId]];
				else
				{
					Assertf(false, "Invalid head blend head id %d!", id);
				}
			}
		}
	}

    return skinTex;
}

// converts a u32 into a three-character string: 5 -> "005"
const char* CPedVariationStream::CustomIToA3(u32 val, char* result)
{
	const char* const numbers = "0123456789";
	u32 digit1 = val / 100;
	u32 digit2 = (val - (digit1 * 100)) / 10;
	u32 digit3 = val - (digit1 * 100) - (digit2 * 10);

	result[0] = numbers[digit1];
	result[1] = numbers[digit2];
	result[2] = numbers[digit3];
	result[3] = 0;

	return result;
}

// converts a u32 into a string: 5 -> "5"
const char* CPedVariationStream::CustomIToA(u32 val, char* result)
{
	const char* const numbers = "0123456789";
	u32 digit1 = val / 100;
	u32 digit2 = (val - (digit1 * 100)) / 10;
	u32 digit3 = val - (digit1 * 100) - (digit2 * 10);

	result[0] = numbers[digit1];
	result[1] = numbers[digit2];
	result[2] = numbers[digit3];
	result[3] = 0;

	for (u32 i = 0; i < 2; ++i)
	{
		if (result[0] != '0')
			break;

		result[0] = result[1];
		result[1] = result[2];
		result[2] = result[3];
	}

	return result;
}

// Name			:	CPedVariationStream::InitSystem
// Purpose		:	Do some setup required for starting the system
// Parameters	:	None
// Returns		:	Nothing
void CPedVariationStream::InitSystem(){
	CPedStreamRenderGfx::InitPool( MEMBUCKET_FX );
	CPedStreamRequestGfx::InitPool( MEMBUCKET_FX );

	CPedStreamRenderGfx::Init();
	
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_FIRST_PERSON_ALTERNATE_DATA, &g_PedVariationStreamFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_FIRST_PERSON_ASSET_DATA, &g_PedVariationStreamFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::ALTERNATE_VARIATIONS_FILE, &g_PedVariationStreamFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PEDSTREAM_FILE, &g_PedVariationStreamFileMounter, eDFMI_UnloadFirst);
}

// Name			:	CPedVariationStream::Initialise
// Purpose		:	Do some setup required for restarting game
// Parameters	:	None
// Returns		:	Nothing
bool CPedVariationStream::Initialise(void)
{
	InitStreamPedFile();
	
	if (CPVDrawblData::CPVClothComponentData::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CPVDrawblData::CPVClothComponentData::PostPsoPlace);
		CPVDrawblData::CPVClothComponentData::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	//When doing in-place loading no destructors get called, adding this delegate will allow us to cleanup the data.
	if (CPedVariationInfo::parser_GetStaticStructure())
	{
		CPedVariationInfo* dummy = NULL;
		atDelegate< void (void)> RemoveFromStoreDelegate(dummy, &CPedVariationInfo::RemoveFromStore);
		CPedVariationInfo::parser_GetStaticStructure()->AddDelegate("RemoveFromStore", RemoveFromStoreDelegate);
	}

	m_BSTexGroupId = grmShaderFx::FindTechniqueGroupId("deferredbs");
	Assert(-1 != m_BSTexGroupId);

	// load facial overlays
	if (!PARSER.LoadObject(PED_FACIAL_OVERLAY_PATH, "meta", ms_facialOverlays))
		Errorf("CPedVariationStream::Initialise: failed to load facial overlays from '%s.meta'", PED_FACIAL_OVERLAY_PATH);

	// load skin tones
	if (!PARSER.LoadObject(PED_SKIN_TONES_PATH, "meta", ms_skinTones))
		Errorf("CPedVariationStream::Initialise: failed to load skin tones from '%s.meta'", PED_SKIN_TONES_PATH);

    // TODO: replace this global data structure with a modified per-ped metadata info (i.e. add to pedvariations.psc instead)
    // load alternate variation data
    if (!PARSER.LoadObject(PED_ALTERNATE_VARIATIONS_PATH, "meta", ms_alternateVariations))
		Errorf("CPedVariationStream::Initialise: failed to load alternate variation data from '%s.meta'", PED_ALTERNATE_VARIATIONS_PATH);
	
    if (!PARSER.LoadObject(PED_MP_TINT_DATA_PATH, "meta", ms_mpTintData))
		Errorf("CPedVariationStream::Initialise: failed to load mp tint data data from '%s.meta'", PED_MP_TINT_DATA_PATH);

	LoadFirstPersonData();

	return true;
}

void CPedVariationStream::LoadFirstPersonData()
{
	// Reset all first person data
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		ms_firstPersonProp[i].pPrevData = NULL;
		ms_firstPersonProp[i].pData = NULL;
		ms_firstPersonProp[i].pTC = NULL;
		ms_firstPersonProp[i].modelId.Invalidate();
		ms_firstPersonProp[i].propModel.Clear();
		ms_firstPersonProp[i].pObject = NULL;
		ms_firstPersonProp[i].pModelInfo = NULL;
		ms_firstPersonProp[i].animState = FirstPersonPropData::STATE_Idle;
		ms_firstPersonProp[i].renderThisFrame = false;
	}
	ms_firstPersonData.m_peds.Reset();
	AddFirstPersonData(PED_FIRST_PERSON_SP_ASSET_DATA_PATH);
	AddFirstPersonData(PED_FIRST_PERSON_MP_ASSET_DATA_PATH);
	
	ms_firstPersonAlternateData.m_alternates.Reset();
	AddFirstPersonAlternateData(PED_FIRST_PERSON_ALTERNATES_PATH);

	// Listen to events from effects triggered by this system
	ANIMPOSTFXMGR.RegisterListener(MakeFunctor(CPedVariationStream::OnAnimPostFXEvent), PED_FP_ANIMPOSTFX_GROUP_ID);

}
void CPedVariationStream::AddFirstPersonAlternateData(const char* filePath)
{
	FirstPersonAlternateData data;
	if(Verifyf(PARSER.LoadObject(filePath,"meta",data),"Can't load first person alternate data from %s!",filePath))
	{
		atArray<FirstPersonAlternate>&alternates = data.m_alternates;
		atArray<FirstPersonAlternate>&target = ms_firstPersonAlternateData.m_alternates;

		int targetCount = target.GetCount();
		
		for(int i=0;i<alternates.GetCount();i++)
		{
			bool bFound = false;

			for(int j=0;j<targetCount;j++)
			{
				if(target[j].m_assetName == alternates[i].m_assetName)
				{
					bFound = true;
					break;
				}
			}

			if(!bFound)
				target.PushAndGrow(alternates[i]);
		}
	}
}

void CPedVariationStream::AddFirstPersonData(const char* filePath)
{
	FirstPersonAssetDataManager data;
	if(Verifyf(PARSER.LoadObject(filePath,"meta",data),"Can't load first person asset data from %s!",filePath))
	{
		FirstPersonPedData* pedData = NULL;
		atArray<FirstPersonPedData>& pedArray = data.m_peds;
#if __ASSERT
		for (int i=0;i<pedArray.GetCount();i++)
		{
			atArray<FirstPersonAssetData>& objects = pedArray[i].m_objects;
			for(int j=0;j<objects.GetCount();j++)
			{
				if((objects[j].m_eAnchorPoint!=NUM_ANCHORS) == (objects[j].m_eCompType!= PV_MAX_COMP))
				{
					FirstPersonAssetData& assetdata = objects[j];
					Assertf(0,\
					"\nPlayer: %s, Local index: %d, Anchor point: %s, Component Type: %s, First person asset definition is malformed (in file %s)",
					pedArray[i].m_name.TryGetCStr(),
					assetdata.m_localIndex,
					parser_eAnchorPoints_Strings[assetdata.m_eAnchorPoint],
					parser_ePedVarComp_Strings[assetdata.m_eCompType+1],
					filePath);
					objects[j].m_eCompType = PV_MAX_COMP;
				}
			}
		}		
#endif
		for (int i=0;i<pedArray.GetCount();i++)
		{
			int foundIndex = -1;
			for(int j=0;j<ms_firstPersonData.m_peds.GetCount();j++)
			{
				if(ms_firstPersonData.m_peds[j].m_name == pedArray[i].m_name)
				{
					pedData = &ms_firstPersonData.m_peds[j];
					foundIndex = j;
					break;
				}
			}
			if(foundIndex==-1)
			{
				ms_firstPersonData.m_peds.PushAndGrow(pedArray[i]);				
			}
			else
			{
				for(int j = 0 ;j<pedArray[i].m_objects.GetCount();j++)
				{
					pedData->m_objects.PushAndGrow(pedArray[i].m_objects[j]);
				}
			}
		}
	}
	UpdateFirstPersonObjectData();
}
void CPedVariationStream::RemoveFirstPersonData(const char* /*filePath*/)
{

}

void CPedVariationStream::AddAlternateVariations(const char* filePath)
{
	CAlternateVariations altVariationsToAdd;

	if (PARSER.LoadObject(filePath, NULL, altVariationsToAdd))
	{
		for (u32 i = 0; i < altVariationsToAdd.GetPedCount(); i++)
		{
			CAlternateVariations::sAlternateVariationPed altVar = altVariationsToAdd.GetAltPedVarAtIndex(i);

			ms_alternateVariations.AddAlternateVariation(altVar);
		}
	}
}

void CPedVariationStream::RemoveAlternateVariations(const char* filePath)
{
	CAlternateVariations altVariationsToRemove;

	if (PARSER.LoadObject(filePath, NULL, altVariationsToRemove))
	{
		for (u32 i = 0; i < altVariationsToRemove.GetPedCount(); i++)
		{
			CAlternateVariations::sAlternateVariationPed altVar = altVariationsToRemove.GetAltPedVarAtIndex(i);

			ms_alternateVariations.RemoveAlternateVariation(altVar);
		}
	}
}

// Name			:	CPedVariationStream::Shutdown
// Purpose		:	Clean up all stuff used during the game, ready for a restart.
// Parameters	:	None
// Returns		:	Nothing
void CPedVariationStream::ShutdownSystem(void)
{
	Assert(m_pDevice==NULL);

	CPedStreamRenderGfx::ShutdownPool();
	CPedStreamRequestGfx::ShutdownPool();

	CPedStreamRenderGfx::Shutdown();
}

void CPedVariationStream::ShutdownSession(unsigned UNUSED_PARAM(shutdownMode))
{
	// Reset all first person data
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		ms_firstPersonProp[i].pData = NULL;
		ms_firstPersonProp[i].pTC = NULL;
		ms_firstPersonProp[i].modelId.Invalidate();
		ms_firstPersonProp[i].propModel.Clear();
		ms_firstPersonProp[i].pObject = NULL;
		ms_firstPersonProp[i].pModelInfo = NULL;
		ms_firstPersonProp[i].renderThisFrame = false;
	}
}

void CPedVariationStream::ShutdownLevel(void)
{
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_human"), STRFLAG_DONTDELETE);
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_small_quadped"), STRFLAG_DONTDELETE);
	CModelInfo::ClearAssetRequiredFlag(CModelInfo::GetModelIdFromName("slod_large_quadped"), STRFLAG_DONTDELETE);
}

// tag all the model infos which have fragment files in the streamed peds .rpf
void CPedVariationStream::InitStreamPedFile()
{
	m_bUseStreamPedFile = true;
	m_pDevice= NULL;

	const CDataFileMgr::DataFile* pFile = DATAFILEMGR.GetFirstFile(CDataFileMgr::PEDSTREAM_FILE);
	while (DATAFILEMGR.IsValid(pFile))
	{
		if(!pFile->m_disabled)
			RegisterStreamPedFile(pFile);

		pFile = DATAFILEMGR.GetNextFile(pFile);
	}
}

void CPedVariationStream::RegisterStreamPedFile(const CDataFileMgr::DataFile* pFile)
{
	fiDevice* pDevice = CPedVariationStream::MountTargetDevice(pFile);
	fiFindData find;
	fiHandle handle;

	// 	handle = pDevice->FindFileBegin("streampeds:/player1/", find);
	// 	handle = pDevice->FindFileBegin("streampeds:/player2/", find);
	// 	handle = pDevice->FindFileBegin("streampeds:/player3/", find);

	handle = pDevice->FindFileBegin("streampeds:/", find);
	if( fiIsValidHandle( handle ) )
	{
		do
		{
			// only interested in the fragments
			const char* pExt = ASSET.FindExtensionInPath(find.m_Name);
			if (pExt && !strcmp(&pExt[1],FRAGMENT_FILE_EXT)){		// skip past the leading full stop for comparison
				char baseName[200];
				ASSET.BaseName(baseName, 199, find.m_Name);
				u32 fileKey = atStringHash(baseName);

				fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
				atArray<CPedModelInfo*> pedTypesArray;
				pedModelInfoStore.GatherPtrs(pedTypesArray);

				int numPedsAvail = pedTypesArray.GetCount();

				for (int i=0; i<numPedsAvail; i++)
				{
					CPedModelInfo& pedModelInfo = *pedTypesArray[i];
					if (fileKey == pedModelInfo.GetHashKey()){
						pedModelInfo.SetStreamFolder(baseName);
						Assertf(pedModelInfo.GetIsStreamedGfx(), "Ped %s is not flagged as streamed gfx ped in peds.meta", pedModelInfo.GetModelName());
					}
				}
			}
		}
		while(pDevice->FindFileNext( handle, find ));

		pDevice->FindFileEnd(handle);
	}

	CPedVariationStream::UnmountTargetDevice();
}

fiDevice* CPedVariationStream::MountTargetDevice(const CDataFileMgr::DataFile* pFile){
	const char* pMount = "streamPeds:/";

	Assertf(pFile != NULL, "Player pack hasn't been setup");
	if (!m_pDevice)
	{
		m_pDevice = rage_new fiPackfile;
		fiPackfile* pPackfile = (fiPackfile*)m_pDevice;
		AssertVerify( pPackfile->Init(pFile->m_filename,true,fiPackfile::CACHE_NONE) );
		AssertVerify( pPackfile->MountAs( pMount ) );
	}
	return(m_pDevice);
}

void CPedVariationStream::UnmountTargetDevice(){
	const char* pMount = "streamPeds:/";

	if( !fiDevice::Unmount( pMount ) )
	{
		Errorf( "Failed to unmount the pack" );
		return;
	}
	Assert(m_pDevice);
	if(m_bUseStreamPedFile)
	{
		((fiPackfile*)m_pDevice)->Shutdown();
		delete m_pDevice;
		m_pDevice= NULL;
	}
}

void CPedVariationInfoCollection::EnumClothTargetFolder(const char* pFolderName)
{
	if ((pFolderName == NULL) || (pFolderName[0] == '\0'))
	{
		return;
	}

	if (m_infos.GetCount() == 0)
		return;
	
#if LIVE_STREAMING
	int passCount = 0;

	if (strStreamingEngine::GetLive().GetEnabled())
	{
		passCount += strStreamingEngine::GetLive().GetFolderCount();
	}

	for (int pass=0; pass<passCount; pass++)
	{
		fiFindData find;
		fiHandle handle = fiHandleInvalid;
		// count files at root
		char	targetPath[200];
		const char *prefix = strStreamingEngine::GetLive().GetFolderPath(pass);

		formatf(targetPath,"%s%s/", prefix, pFolderName);

		const fiDevice* pDevice = NULL;

		if ((pDevice = fiDevice::GetDevice(targetPath)) != NULL)
		{
			handle = pDevice->FindFileBegin(targetPath, find);
		}

		// It's acceptable not to have a valid handle for preview folders.
		if (!fiIsValidHandle( handle ))
		{
			continue;
		}

		if( Verifyf(fiIsValidHandle( handle ), "Can't find in %s", targetPath) )
		{
			do
			{
				const char* pExt = ASSET.FindExtensionInPath(find.m_Name);
				if (pExt && !strcmp(&pExt[1],CLOTH_FILE_EXT))
				{
					char compName[80];
					s32 clothIdx;
					char	texIdx;
					//Printf("found: %s\n",find.m_Name);
					/*s32 ret = */sscanf(find.m_Name, "%[^_]_%d_%c" ,compName, &clothIdx, &texIdx );

					// establish which component this cloth is to be mapped onto
					ePedVarComp compId = PV_COMP_INVALID;
					for(s32 i=0;i<PV_MAX_COMP;i++){
						if (strncmp(compName, varSlotNames[i], 4) == 0){
							compId = varSlotEnums[i];
							break;
						}
					}

					if (compId == PV_COMP_INVALID)
					{
						// warning that an invalid cloth was found (unknown component name)
						Printf("Invalid cloth: Unknown component name: %s\n",compName);
						Assertf(compId != PV_COMP_INVALID, "cloth doesn't specify component name" );
						continue;
					}

					// if we don't have an entry for this component then create it
					CPVComponentData* pCompData = m_infos[0]->GetCompData(compId);
					if (!pCompData)
					{
						Assertf(false, "Trying to add cloth '%s' to non-existing component!", find.m_Name);
						continue;
					}
//					Assert(pCompData);
					if( pCompData )
					{
						Assert( clothIdx > -1 && clothIdx < MAX_DRAWBL_VARS );
						CPVDrawblData* pDrawable = pCompData->GetDrawbl(clothIdx);
//					Assert(pDrawable);
						if( pDrawable )
							pDrawable->m_clothData.m_ownsCloth = true;
					}
				}				
			}
			while(pDevice->FindFileNext( handle, find ));

			pDevice->FindFileEnd(handle);
		}
	}
#endif // LIVE_STREAMING
}


// scan through the folder in the stream ped file to determine what variation models and textures are available for this model
void CPedVariationInfoCollection::EnumTargetFolder(const char* pFolderName, CPedModelInfo* ASSERT_ONLY(pModelInfo))
{
	if ((pFolderName == NULL) || (pFolderName[0] == '\0')){
		pedAssertf(false, "Ped %s is flagged as streamed but folder isn't found. Is the IsStreamedGfx flag wrong?", pModelInfo->GetModelName());
		return;
	}

	if (m_infos.GetCount() == 0)
		return;

#if LIVE_STREAMING
	m_bHasLowLODs = false;		// player doesn't have low LODs

	int passCount = 0;

	if (strStreamingEngine::GetLive().GetEnabled())
	{
		passCount += strStreamingEngine::GetLive().GetFolderCount();
	}

	for (int pass = 0 ; pass < passCount; pass++)
	{
		fiFindData find;
		fiHandle handle = fiHandleInvalid;
		// count files at root
		char	targetPath[200];
		const char *prefix = strStreamingEngine::GetLive().GetFolderPath(pass);

		formatf(targetPath,"%s%s/", prefix, pFolderName);

		const fiDevice* pDevice = NULL;

		if ((pDevice = fiDevice::GetDevice(targetPath)) != NULL)
		{
			handle = pDevice->FindFileBegin(targetPath, find);
		}

		// It's acceptable not to have a valid handle for preview folders.
		if (!fiIsValidHandle( handle ))
		{
			continue;
		}

		if( Verifyf(fiIsValidHandle( handle ), "Can't find in %s", targetPath) )
		{
			do
			{
				const char* pExt = ASSET.FindExtensionInPath(find.m_Name);
				if (pExt && !strcmp(&pExt[1],TXD_FILE_EXT))
				{		// skip past the leading full stop for comparison
					char compName[80];
					char texType[80];
					char raceType[80];
					s32 drawblIdx;
					char	texIdx;
					//Printf("found: %s\n",find.m_Name);
					s32 ret = sscanf(find.m_Name, "%[^_]_%[^_]_%d_%c_%[^.]" ,compName, texType, &drawblIdx, &texIdx, raceType);

					// only using the diffuse textures to enumerate the models and textures
					if ((ret == 5) && !strncmp(texType,"diff",4))
					{
						//Printf("scanned as: %s, %s, %d, %c %s\n\n", compName, texType, drawblIdx, texIdx, raceType);
						texIdx -= 'a';		// convert char to int index

						// establish which component this texture is to be mapped onto
						ePedVarComp compId = PV_COMP_INVALID;
						for(s32 i=0;i<PV_MAX_COMP;i++){
							if (strncmp(compName, varSlotNames[i], 4) == 0){
								compId = varSlotEnums[i];
								break;
							}
						}

						// establish which racial type this texture is
						ePVRaceType raceId = PV_RACE_UNIVERSAL;
						for(s32 i=0; i<PV_RACE_MAX_TYPES; i++) {
							if (strcmp(raceType, raceTypeNames[i])==0){
								raceId = static_cast<ePVRaceType>(i);
							}
						}

						if (compId == PV_COMP_INVALID){
							// warning that an invalid ped texture was found (unknown component name)
							Printf("Invalid ped tex: Unknown component name: %s\n",compName);
							Assertf(compId != PV_COMP_INVALID, "ped diff texture doesn't specify component name" );
							continue;
						}

						Assert(drawblIdx < MAX_PLAYER_DRAWBL_VARS);
						Assert(texIdx < MAX_TEX_VARS);

						// if we don't have an entry for this component then create it
						CPVComponentData* pCompData = m_infos[0]->GetCompData(compId);
 						if (!pCompData)
						{
							m_infos[0]->m_aComponentData3.Grow();
							m_infos[0]->m_availComp[compId] = (u8)(m_infos[0]->m_aComponentData3.GetCount() - 1);

							pCompData = m_infos[0]->GetCompData(compId);
 						}
						if(pCompData)
						{
							CPVDrawblData* pDrawblData = pCompData->ObtainDrawblMapEntry(drawblIdx);
							// a valid variation has been found
							pCompData->m_numAvailTex++;					//  inc num tex avail for this component

							if (pDrawblData->GetNumTex() <= texIdx)
								pDrawblData->m_aTexData.ResizeGrow(texIdx + 1);

							pDrawblData->SetTexData(u32(texIdx), u8(raceId));
							pDrawblData->SetHasActiveDrawbl(true);

							if (raceId != PV_RACE_UNIVERSAL){
								pDrawblData->SetIsUsingRaceTex(true);
							}
						}
					}
				}
			}
			while(pDevice->FindFileNext( handle, find ));

			pDevice->FindFileEnd(handle);
		}
	}
#endif // LIVE_STREAMING
}

void CPedVariationStream::AddBodySkinRequest(CPedStreamRequestGfx* pNewStreamGfx, u32 id, u32 comp, u32 slot, u32 streamFlags, CPedModelInfo* modelInfo)
{
	atHashString skinTex = ms_skinTones.GetSkinToneTexture(comp, id, modelInfo->GetPersonalitySettings().GetIsMale());
	pNewStreamGfx->AddHeadBlendReq(slot, skinTex, streamFlags, modelInfo);
}

bool CPedVariationStream::AddComponentRequests(CPedStreamRequestGfx* gfx, CPedModelInfo* pmi, u8 drawableId, u8 texDrawableId, u8 texId, s32 streamFlags, eHeadBlendSlots headSlot, eHeadBlendSlots feetSlot, eHeadBlendSlots upprSlot, eHeadBlendSlots lowrSlot, eHeadBlendSlots texSlot, u32 headHash)
{
#if __ASSERT
	char drawablName[STORE_NAME_LENGTH];
	char diffMap[STORE_NAME_LENGTH];

	const char*	pedFolder = pmi->GetStreamFolder();
#endif // __ASSERT

	CPedVariationInfoCollection* pVarInfo = pmi->GetVarInfo();

	CPVDrawblData* drawable0 = pVarInfo->GetCollectionDrawable(PV_COMP_HEAD, drawableId);
	if(drawableId != 0xff && Verifyf(drawable0, "Requested ped component %s/head_%03d (_r or _u not known) which does not exist!", pedFolder, drawableId))
	{
		char raceModifier[] = "_u";
		if (drawable0->IsUsingRaceTex())
			raceModifier[1] = 'r';

		char drawableIdStr[4] = {0};
		u32 drawableHash = atPartialStringHash(CustomIToA3(drawableId, drawableIdStr), headHash);
		drawableHash = atPartialStringHash(raceModifier, drawableHash);
		drawableHash = atFinalizeHash(drawableHash);

		ASSERT_ONLY(sprintf(drawablName, "%s/head_%03d%s", pedFolder, drawableId, raceModifier);)
		Assertf(drawableHash == atStringHash(drawablName), "Head blend asset '%s' hash not what expected: %d == %d", drawablName, drawableHash, atStringHash(drawablName));

		gfx->AddHeadBlendReq(headSlot , drawableHash, streamFlags, pmi);

		if (headSlot != HBS_PARENT_HEAD_0 && headSlot != HBS_PARENT_HEAD_1)
		{
			// make sure we use male skin tones for male child peds and female for female child peds
			u32 skinDrawable = texDrawableId;

			// request body skin
			AddBodySkinRequest(gfx, skinDrawable, PBC_FEET, feetSlot, streamFlags, pmi);
			AddBodySkinRequest(gfx, skinDrawable, PBC_UPPR, upprSlot, streamFlags, pmi);
			AddBodySkinRequest(gfx, skinDrawable, PBC_LOWR, lowrSlot, streamFlags, pmi);

			// texture
			CPVDrawblData* texDrawable = pVarInfo->GetCollectionDrawable(PV_COMP_HEAD, texDrawableId);
			if (texDrawable && texId < texDrawable->GetNumTex())
			{
				u8 raceIdx = texDrawable->GetTexData(texId);
				if (raceIdx != (u8)PV_RACE_INVALID)
				{
					char textureIndex[] = "a";
					textureIndex[0] += texId;

					u32 texHash = atPartialStringHash("diff_", headHash);
					texHash = atPartialStringHash(CustomIToA3(texDrawableId, drawableIdStr), texHash);
					texHash = atPartialStringHash("_", texHash);
					texHash = atPartialStringHash(textureIndex, texHash);
					texHash = atPartialStringHash("_", texHash);
					texHash = atPartialStringHash(raceTypeNames[raceIdx], texHash);
					texHash = atFinalizeHash(texHash);

					ASSERT_ONLY(sprintf(diffMap, "%s/head_diff_%03d_%c_%s", pedFolder, texDrawableId, (texId + 'a'), raceTypeNames[raceIdx]);)
                    Assertf(texHash == atStringHash(diffMap), "Head blend asset '%s' hash not what expected: %d == %d", diffMap, texHash, atStringHash(diffMap));

					gfx->AddHeadBlendReq(texSlot, texHash, streamFlags, pmi);
				}
			}
		}
		return true;
	}
	return false;
}

void CPedVariationStream::SetPaletteTexture(CPed* pPed, u32 comp)
{
	CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
	if (!pGfxData)
		return;

	// blended heads use the custom palette texture from meshblendmanager
	if (pPed->HasHeadBlend() && comp != PV_COMP_HAND) // in MP the parachute is on the hand component
	{
		CPedHeadBlendData* blendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
		if (blendData)
		{
			s32 palSelect = -1;
			grcTexture* palTex = MESHBLENDMANAGER.GetCrewPaletteTexture(blendData->m_blendHandle, palSelect);

			s32 palSelectHair = -1;
			s32 palSelectHair2 = -1;
			grcTexture* palTexHair = MESHBLENDMANAGER.GetTintTexture(blendData->m_blendHandle, RT_HAIR, palSelectHair, palSelectHair2);
			if (palTex && palSelect != -1)
			{
				if (comp == PV_COMP_HAIR)
				{
					pGfxData->m_pShaderEffect->SetDiffuseTexturePal(palTexHair, comp);
					pGfxData->m_pShaderEffect->SetDiffuseTexturePalSelector((u8)palSelectHair, comp, true);
					pGfxData->m_pShaderEffect->SetSecondHairPalSelector((u8)palSelectHair2);
				}
				else
				{
					pGfxData->m_pShaderEffect->SetDiffuseTexturePal(palTex, comp);
					pGfxData->m_pShaderEffect->SetDiffuseTexturePalSelector((u8)palSelect, comp, true);
				}
			}
		}
	}
	else
	{
		rmcDrawable* pDrawable = pGfxData->GetDrawable(comp);
		CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
		CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();

		if (!pDrawable || !pPedModelInfo || !pVarInfo)
			return;

		s32 globalDrawIdx = pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[comp];

		if (globalDrawIdx == PV_NULL_DRAWBL)
			return;

		s32 localDrawIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)comp, globalDrawIdx);
		char paletteTexName[STORE_NAME_LENGTH] = {0};
		sprintf(paletteTexName, "%s_pall_%03d_x", varSlotNames[comp], localDrawIdx);

		// find the specular in the drawable to use as the skin mask
		fwTxd* txd = pDrawable->GetShaderGroup().GetTexDict();
		if (txd)
		{
			grcTexture* paletteTex = txd->Lookup(paletteTexName);
			if (paletteTex)
			{
				pGfxData->m_pShaderEffect->SetDiffuseTexturePal(paletteTex, comp);
				pGfxData->m_pShaderEffect->SetDiffuseTexturePalSelector(pPed->GetPedDrawHandler().GetVarData().m_aPaletteId[comp], comp, false);
			}
		}
	}
}



void CPedVariationStream::RequestStreamPedFiles(CPed* pPed, CPedVariationData* pNewVarData, s32 flags)
{
	RequestStreamPedFilesInternal(pPed, pNewVarData, false, NULL, flags);
}


bool CPedVariationStream::RequestStreamPedFilesDummy(CPed* pPed, CPedVariationData* pNewVarData, CPedStreamRequestGfx* pNewStreamGfx)
{
	return RequestStreamPedFilesInternal(pPed, pNewVarData, true, pNewStreamGfx, 0);
}

#if RSG_PC && __BANK
namespace rage 
{
	XPARAM(noSocialClub);
};
#endif

// generate the name drawable & texture file names we want to request for the desired StreamPed variation
bool CPedVariationStream::RequestStreamPedFilesInternal(CPed* pPed, CPedVariationData* pNewVarData, bool isDummy, CPedStreamRequestGfx* pNewStreamGfx, s32 streamFlags) {

	// Make sure this is really a player model (with special data) otherwise don't
	// do anything special.
	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pPedModelInfo);
	if (!pPedModelInfo->GetIsStreamedGfx())
	{
		return false;
	}

	u32	drawblIdx = 0;
	u8	drawblAltIdx = 0;
	u8	texIdx = 0;

#if __ASSERT
	char	drawablName[STORE_NAME_LENGTH];
	char	diffMap[STORE_NAME_LENGTH];
#endif // __ASSERT

	// generate the name of the drawable to extract
	CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
	{
		return false;
	}

	if(isDummy == false)
	{
		Assert(pNewStreamGfx == NULL);
		pPed->GetExtensionList().Destroy(CPedStreamRequestGfx::GetAutoId());			// strip all ped stream requests for this entity

		pNewStreamGfx = rage_new CPedStreamRequestGfx(REPLAY_ONLY(false));
		Assert(pNewStreamGfx);
		pNewStreamGfx->SetTargetEntity(pPed);

		// if we request both props and assets, sync them so they display at the same time (e.g. clothes shops)
		CPedPropRequestGfx* propGfx = pPed->GetPropRequestGfx();
		if (propGfx)
		{
			propGfx->SetWaitForGfx(pNewStreamGfx);
			pNewStreamGfx->SetWaitForGfx(propGfx);
		}
	}

	const char*	pedFolder=pPedModelInfo->GetStreamFolder();
	u32 pedFolderHash = atPartialStringHash(pedFolder);
	u32 pedFolderWithDlcHash = atPartialStringHash("_", pedFolderHash);
	pedFolderHash = atPartialStringHash("/", pedFolderHash);

	// url:bugstar:3042656 - When the user has now ugc privileges locally he can't see any crew emblem
	// if crewEmblemHidden is true we don't use drawblIdx to avoid to see a grayish block where the crew emblem would be
	bool crewEmblemHidden =	!CLiveManager::CheckUserContentPrivileges()	&& !pNewVarData->GetOverrideCrewLogoTexHash() && !pNewVarData->GetOverrideCrewLogoTxdHash();
#if RSG_PC && __BANK
	if(PARAM_noSocialClub.Get())
	{	// disable above check when -nosocialclub param present: B*7128325 (PC: DECL components not rendering in-game)
		crewEmblemHidden = false;
	}
#endif


	// generate the required slot names and request them accordingly
	for(u32 i=0;i<PV_MAX_COMP; i++){

		// generate the file names of the files we require. Read above about crewEmblemHidden
		drawblIdx = (crewEmblemHidden && i == PV_COMP_DECL) ? 0 : pNewVarData->m_aDrawblId[i];		// current drawable for each component
		drawblAltIdx = pNewVarData->m_aDrawblAltId[i];	// current variation of drawable
		texIdx = pNewVarData->m_aTexId[i];

		const char* dlcName = pVarInfo->GetDlcNameFromDrawableIdx((ePedVarComp)i, drawblIdx);

		CPVDrawblData* pDrawblData = pVarInfo->GetCollectionDrawable(i, drawblIdx);
		if (!pDrawblData)
			continue;

        ASSERT_ONLY(u32 origDrawableIdx = drawblIdx);
		drawblIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)i, (u32)drawblIdx);

		if (drawblIdx == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			continue;
		}

		if (!Verifyf(texIdx < pDrawblData->GetNumTex(), "Selected texture '%c' doesn't exists for '%s_%03d' component on ped '%s'. Is it present in meta data?", 'a' + texIdx, varSlotNames[i], origDrawableIdx, pPed->GetModelName()))
			continue;


		char modifier[] = "_u_";
		if (pDrawblData->IsMatchingPrev())
			modifier[1] = 'm';
		else if (pDrawblData->IsUsingRaceTex())
			modifier[1] = 'r';

		if (drawblAltIdx > 0)
		{
#if __ASSERT
			if (dlcName)
				sprintf(drawablName, "%s_%s/%s_%03d%s%d", pedFolder, dlcName, varSlotNames[i], drawblIdx, modifier, drawblAltIdx);
			else
				sprintf(drawablName, "%s/%s_%03d%s%d", pedFolder, varSlotNames[i], drawblIdx, modifier, drawblAltIdx);
#endif // __ASSERT
		}
		else
		{
			modifier[2] = 0;
#if __ASSERT
			if (dlcName)
				sprintf(drawablName, "%s_%s/%s_%03d%s", pedFolder, dlcName, varSlotNames[i], drawblIdx, modifier);
			else
				sprintf(drawablName, "%s/%s_%03d%s", pedFolder, varSlotNames[i], drawblIdx, modifier);
#endif // __ASSERT
		}

		char drawblIdxStr[4] = {0};
		char drawblAltIdxStr[4] = {0};

		u32 slotHash = 0;
		u32 drawableHash = pedFolderHash;
		if (dlcName)
		{
			drawableHash = atPartialStringHash(dlcName, pedFolderWithDlcHash);
			drawableHash = atPartialStringHash("/", drawableHash);
		}
		drawableHash = atPartialStringHash(varSlotNames[i], drawableHash);
		slotHash = drawableHash; // reuse this for the texture below
		drawableHash = atPartialStringHash("_", drawableHash);
		drawableHash = atPartialStringHash(CustomIToA3(drawblIdx, drawblIdxStr), drawableHash);

#if FPS_MODE_SUPPORTED
		// We need the drawable hash that includes the modifier without the underscore for the first person alternate asset
 		char fpModifier[3];
 		memcpy(fpModifier,modifier,3);
 		fpModifier[2] = 0;
 		u32 fpPreAltHash = atPartialStringHash(fpModifier, drawableHash);
		u32 fpPreAltHashFinalized = atFinalizeHash(fpPreAltHash);
#endif // FPS_MODE_SUPPORTED

		drawableHash = atPartialStringHash(modifier, drawableHash);
		u32 preAltDrawableHash = drawableHash;
		if (drawblAltIdx > 0)
			drawableHash = atPartialStringHash(CustomIToA(drawblAltIdx, drawblAltIdxStr), drawableHash);

		u32 hdDrawableHash = drawableHash;
		drawableHash = atFinalizeHash(drawableHash);

		Assertf(drawableHash == atStringHash(drawablName), "Drawable hash not expected for '%s': %d == %d", drawablName, drawableHash, atStringHash(drawablName));

#if FPS_MODE_SUPPORTED
		if (pPed->IsLocalPlayer())
		{
			u8 altId = 255;
			ASSERT_ONLY(char assetName[STORE_NAME_LENGTH]);
			for (s32 alt = 0; alt < ms_firstPersonAlternateData.m_alternates.GetCount(); ++alt)
			{
				if (ms_firstPersonAlternateData.m_alternates[alt].m_assetName.GetHash() == fpPreAltHashFinalized)
				{
					altId = ms_firstPersonAlternateData.m_alternates[alt].m_alternate;
					ASSERT_ONLY(safecpy(assetName, ms_firstPersonAlternateData.m_alternates[alt].m_assetName.GetCStr()));
					break;
				}
			}

			if (altId != 255)
			{
#if __ASSERT
				char preAltDrawableName[STORE_NAME_LENGTH];
				if (dlcName)
					formatf(preAltDrawableName, "%s_%s/%s_%03d%s_%d", pedFolder, dlcName, varSlotNames[i], drawblIdx, drawblAltIdx <= 0 ? modifier : fpModifier, altId);
				else
					formatf(preAltDrawableName, "%s/%s_%03d%s_%d", pedFolder, varSlotNames[i], drawblIdx, drawblAltIdx <= 0 ? modifier : fpModifier, altId);
#endif

				// we have a first person alternate drawable we need to stream in
				if(drawblAltIdx <= 0)
					preAltDrawableHash = atPartialStringHash("_", fpPreAltHash);
				char altIdxStr[4] = {0};
				preAltDrawableHash = atPartialStringHash(CustomIToA(altId, altIdxStr), preAltDrawableHash);
				preAltDrawableHash = atFinalizeHash(preAltDrawableHash);

				pNewStreamGfx->AddFpAlt(i, preAltDrawableHash, streamFlags, pPedModelInfo
					ASSERT_ONLY(, assetName, preAltDrawableName));
			}
		}
#endif // FPS_MODE_SUPPORTED

		u8 raceIdx = pDrawblData->GetTexData(texIdx);
		if (!Verifyf(raceIdx != (u8)PV_RACE_INVALID, "Bad race index for drawable %s with texIdx %d on ped %s", drawablName, texIdx, pPed->GetModelName()))
			continue;

		// generate the name of the textures required
#if __ASSERT
		if (dlcName)
			sprintf(diffMap, "%s_%s/%s_diff_%03d_%c_%s", pedFolder, dlcName, varSlotNames[i], drawblIdx, (texIdx+'a'), raceTypeNames[raceIdx]);
		else
			sprintf(diffMap, "%s/%s_diff_%03d_%c_%s", pedFolder, varSlotNames[i], drawblIdx, (texIdx+'a'), raceTypeNames[raceIdx]);
#endif // __ASSERT

		char textureIndex[] = "a";
		textureIndex[0] += texIdx;

		u32 txdHash = slotHash;
		txdHash = atPartialStringHash("_diff_", txdHash);
		txdHash = atPartialStringHash(CustomIToA3(drawblIdx, drawblIdxStr), txdHash);
		txdHash = atPartialStringHash("_", txdHash);
		txdHash = atPartialStringHash(textureIndex, txdHash);
		txdHash = atPartialStringHash("_", txdHash);
		txdHash = atPartialStringHash(raceTypeNames[raceIdx], txdHash);

		u32 hdTxdHash = txdHash;
		txdHash = atFinalizeHash(txdHash);

		Assertf(txdHash == atStringHash(diffMap), "Texture hash not expected for '%s': %d == %d", diffMap, txdHash, atStringHash(diffMap));

		Verifyf(pNewStreamGfx->AddTxd(i, txdHash, streamFlags, pPedModelInfo), "Couldn't make request for TXD '%s', does it exist?", diffMap);
		pNewStreamGfx->AddDwd(i, drawableHash, streamFlags, pPedModelInfo);
		if( pDrawblData->m_clothData.HasCloth() )
			pNewStreamGfx->AddCld(i, drawableHash, streamFlags, pPedModelInfo);

		// create hd txd bindings for the texlod system
		if (!pNewVarData->HasHeadBlendData(pPed)) // don't upgrade blended peds
		{
			s32 ldTxd = g_TxdStore.FindSlot(txdHash).Get();
			if (ldTxd != -1)
			{
#if __ASSERT
				if (dlcName)
					sprintf(diffMap, "%s_%s/%s_diff_%03d_%c_%s+hi", pedFolder, dlcName, varSlotNames[i], drawblIdx, (texIdx+'a'), raceTypeNames[raceIdx]);
				else
					sprintf(diffMap, "%s/%s_diff_%03d_%c_%s+hi", pedFolder, varSlotNames[i], drawblIdx, (texIdx+'a'), raceTypeNames[raceIdx]);
#endif // __ASSERT
                hdTxdHash = atPartialStringHash("+hi", hdTxdHash);
				hdTxdHash = atFinalizeHash(hdTxdHash);

				Assertf(hdTxdHash == atStringHash(diffMap), "HD texture hash not expected for '%s': %d == %d", diffMap, hdTxdHash, atStringHash(diffMap));

				s32 hdTxd = g_TxdStore.FindSlot(hdTxdHash).Get();
				if (hdTxd != -1)
				{
					CTexLod::StoreHDMapping(STORE_ASSET_TXD, ldTxd, hdTxd);
					pNewStreamGfx->SetHighTxdForComponentTxd(i);
				}
			}

			s32 ldDwd = g_DwdStore.FindSlot(drawableHash).Get();
			if (ldDwd != -1)
			{
#if __ASSERT
				char drawablHDTxdName[STORE_NAME_LENGTH];
				sprintf(drawablHDTxdName, "%s+hidd", drawablName);
#endif // __ASERT
				hdDrawableHash = atPartialStringHash("+hidd", hdDrawableHash);
				hdDrawableHash = atFinalizeHash(hdDrawableHash);

				Assertf(hdDrawableHash == atStringHash(drawablHDTxdName), "HD drawable hash not expected for '%s': %d == %d", drawablHDTxdName, hdDrawableHash, atStringHash(drawablHDTxdName));

				s32 hdDwdTxd = g_TxdStore.FindSlot(hdDrawableHash).Get();
				if (hdDwdTxd != -1)
				{
					CTexLod::StoreHDMapping(STORE_ASSET_DWD, ldDwd, hdDwdTxd);
					pNewStreamGfx->SetHighTxdForComponentDwd(i);
				}
			}
		}
	}

	// head blend data
	{
		if (pNewVarData->HasHeadBlendData(pPed))
		{
 			pedDebugf1("[HEAD BLEND] CPedVariationStream::RequestStreamPedFilesInternal(%p): Has head blend data, requesting assets...", pPed);
			bool addedHeadBlendReqs = false;

			// drawables and textures
			CPedHeadBlendData* blendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

			// when selecting the textures to stream in for the parents pick the one favored by the blend value
			u8 texDrawable0 = blendData->m_tex0; // texture is used as drawable index
			u8 texDrawable1 = blendData->m_tex1;
			u8 texDrawable2 = blendData->m_tex2;

			u32 headHash = 0;
			headHash = atPartialStringHash(pedFolder, headHash);
			headHash = atPartialStringHash("/head_", headHash);

			addedHeadBlendReqs |= AddComponentRequests(pNewStreamGfx, pPedModelInfo, blendData->m_head0, texDrawable0, 0, streamFlags,
													    HBS_HEAD_0, HBS_SKIN_FEET_0, HBS_SKIN_UPPR_0, HBS_SKIN_LOWR_0, HBS_TEX_0, headHash);

			addedHeadBlendReqs |= AddComponentRequests(pNewStreamGfx, pPedModelInfo, blendData->m_head1, texDrawable1, 0, streamFlags,
													    HBS_HEAD_1, HBS_SKIN_FEET_1, HBS_SKIN_UPPR_1, HBS_SKIN_LOWR_1, HBS_TEX_1, headHash);

			addedHeadBlendReqs |= AddComponentRequests(pNewStreamGfx, pPedModelInfo, blendData->m_head2, texDrawable2, 0, streamFlags,
													    HBS_HEAD_2, HBS_SKIN_FEET_2, HBS_SKIN_UPPR_2, HBS_SKIN_LOWR_2, HBS_TEX_2, headHash);

			addedHeadBlendReqs |= AddComponentRequests(pNewStreamGfx, pPedModelInfo, blendData->m_extraParentData.head0Parent0, (u8)-1, (u8)-1, streamFlags,
													    HBS_PARENT_HEAD_0, HBS_MAX, HBS_MAX, HBS_MAX, HBS_MAX, headHash);

			addedHeadBlendReqs |= AddComponentRequests(pNewStreamGfx, pPedModelInfo, blendData->m_extraParentData.head0Parent1, (u8)-1, (u8)-1, streamFlags,
													    HBS_PARENT_HEAD_1, HBS_MAX, HBS_MAX, HBS_MAX, HBS_MAX, headHash);

			// overlays
			for (s32 i = 0; i < HOS_MAX; ++i)
			{
				const atArray<atHashString>* arr = NULL;
				switch (i)
				{
				case HOS_BLEMISHES: arr = &CPedVariationStream::GetFacialOverlays().m_blemishes; break;
				case HOS_FACIAL_HAIR: arr = &CPedVariationStream::GetFacialOverlays().m_facialHair; break;
				case HOS_EYEBROW: arr = &CPedVariationStream::GetFacialOverlays().m_eyebrow; break;
				case HOS_AGING: arr = &CPedVariationStream::GetFacialOverlays().m_aging; break;
				case HOS_MAKEUP: arr = &CPedVariationStream::GetFacialOverlays().m_makeup; break;
				case HOS_BLUSHER: arr = &CPedVariationStream::GetFacialOverlays().m_blusher; break;
				case HOS_DAMAGE: arr = &CPedVariationStream::GetFacialOverlays().m_damage; break;
				case HOS_BASE_DETAIL: arr = &CPedVariationStream::GetFacialOverlays().m_baseDetail; break;
				case HOS_SKIN_DETAIL_1: arr = &CPedVariationStream::GetFacialOverlays().m_skinDetail1; break;
				case HOS_SKIN_DETAIL_2: arr = &CPedVariationStream::GetFacialOverlays().m_skinDetail2; break;
				case HOS_BODY_1: arr = &CPedVariationStream::GetFacialOverlays().m_bodyOverlay1; break;
				case HOS_BODY_2: arr = &CPedVariationStream::GetFacialOverlays().m_bodyOverlay2; break;
				case HOS_BODY_3: arr = &CPedVariationStream::GetFacialOverlays().m_bodyOverlay3; break;
				}
				Assertf(arr, "New entry added to eBodyOverlaySlots but not the above case block");

				if (blendData->m_overlayTex[i] < arr->GetCount())
				{
					atHashString overlay = (*arr)[blendData->m_overlayTex[i]];
					pNewStreamGfx->AddHeadBlendReq(HBS_OVERLAY_TEX_BLEMISHES + i, overlay, streamFlags, pPedModelInfo);
					addedHeadBlendReqs = true;				
				}
			}

			// micro morphs
			for (s32 i = 0; i < MMT_MAX; ++i)
			{
				if (rage::Abs(blendData->m_microMorphBlends[i]) < 0.01f)
					continue;

				const char* assetName = NULL;

				// deal with micro morph pairs. i.e. these blend [-1, 1] and use two different assets:
				// one for the [-1, 0] range and another for the [0, 1] range.
				if (i < MMT_NUM_PAIRS)
				{
					if (blendData->m_microMorphBlends[i] < 0.f)
					{
						assetName = s_microMorphAssets[i * 2];
					}
					else
					{
						assetName = s_microMorphAssets[i * 2 + 1];
					}
				}
				else
				{
					Assertf(blendData->m_microMorphBlends[i] >= 0.f, "CPedVariationData::MicroMorph: Invalid blend value specified for morph type %s, blend %f", s_microMorphAssets[i], blendData->m_microMorphBlends[i]);
					assetName = s_microMorphAssets[MMT_NUM_PAIRS * 2 + (i - MMT_NUM_PAIRS)];
				}

				u32 assetHash = pedFolderHash;
				assetHash = atPartialStringHash(assetName, assetHash);
				assetHash = atFinalizeHash(assetHash);

				pNewStreamGfx->AddHeadBlendReq(HBS_MICRO_MORPH_SLOTS + i, assetHash, streamFlags, pPedModelInfo);
                addedHeadBlendReqs = true;
			}

            if (addedHeadBlendReqs)
            {
				pedDebugf1("[HEAD BLEND] CPedVariationStream::RequestStreamPedFilesInternal(%p): Requests in progress for handle 0x%08x", pPed, blendData->m_blendHandle);
                MESHBLENDMANAGER.SetRequestsInProgress(blendData->m_blendHandle, true);
                pNewStreamGfx->SetHasHeadBlendAssets(blendData->m_blendHandle);
            }
			else
			{
 				pedDebugf1("[HEAD BLEND] CPedVariationStream::RequestStreamPedFilesInternal(%p): No blend requests added!", pPed);
			}
		}
	}

	if(isDummy == false)
		pNewStreamGfx->PushRequest();		//activate this set of requests

	return true;
}

// load the files which are required by the new variation data (and not already loaded previously)
void CPedVariationStream::ApplyStreamPedFiles(CPed* pPed, CPedVariationData* pNewVarData, CPedStreamRequestGfx* pPSGfx){

	// reset the component disable array
	for(u32 i=0; i< MAX_DRAWBL_VARS; i++){
		ms_aDrawPlayerComp[i] = true;
	}

	// Make sure this is really a player model (with special data) otherwise don't
	// do anything special.
	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pPedModelInfo);
	if (!pPedModelInfo->GetIsStreamedGfx())
	{
		return;
	}

	u32	drawblIdx = 0;

	// generate the name of the drawable to extract
	CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return;

	Assert(pPSGfx);
	pPSGfx->RequestsComplete();						// add refs to loaded gfx & free up the requests

	CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
	if (!Verifyf(pGfxData, "Ped '%s' has no render gfx. Is pool full? Check tty!", pPed->GetModelName()))
		return;

	rmcDrawable* drawableFeet = pGfxData->GetDrawable(PV_COMP_FEET);
	CCustomShaderEffectBaseType *cset = pGfxData->m_pShaderEffectType;
	if (drawableFeet)
		cset = CCustomShaderEffectPed::SetupForDrawableComponent(drawableFeet, PV_COMP_FEET, cset);
	rmcDrawable* drawableUppr = pGfxData->GetDrawable(PV_COMP_UPPR);
	if (drawableUppr)
		cset = CCustomShaderEffectPed::SetupForDrawableComponent(drawableUppr, PV_COMP_UPPR, cset);
	rmcDrawable* drawableLowr = pGfxData->GetDrawable(PV_COMP_LOWR);
	if (drawableLowr)
		cset = CCustomShaderEffectPed::SetupForDrawableComponent(drawableLowr, PV_COMP_LOWR, cset);
	pGfxData->m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(cset);

	USE_MEMBUCKET(MEMBUCKET_SYSTEM);

	s32 boneCount = -1;
	bool needsSkeletonMap = false;

    s32 headBoneCount = -1;

	// all of the gfx are streamed now - lets build what we need...
	for(u32 i=0;i<PV_MAX_COMP; i++){

		// generate the file names of the files we require
		drawblIdx = pNewVarData->m_aDrawblId[i];		// current drawable for each component

		if (drawblIdx == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			continue;
		}

		CPVDrawblData* pDrawblData = pVarInfo->GetCollectionDrawable(i, drawblIdx);
		if (!pDrawblData)
            continue;

		grcTexture* pDiffMap = pGfxData->GetTexture(i);
		rmcDrawable* pDrawable = pGfxData->GetDrawable(i);
		Assertf(pDiffMap, "Missing diffuse texture on ped %s, %s_%03d", pPed->GetModelName(), varSlotNames[i], drawblIdx);
		if (!Verifyf(pDrawable, "No drawable for slot %d for ped %s, %s_%03d", i, pPed->GetModelName(), varSlotNames[i], drawblIdx))
			continue;

		if (!needsSkeletonMap)
		{
			crSkeletonData* skel = pDrawable->GetSkeletonData();
			if (skel)
			{
                if (i == PV_COMP_HEAD)
                    headBoneCount = skel->GetNumBones();

				if (boneCount == -1)
					boneCount = skel->GetNumBones();
				else if (boneCount != skel->GetNumBones())
					needsSkeletonMap = true;
			}
		}

		// we can access the drawable now, so check it's alpha state & store it
		pDrawable->GetLodGroup().ComputeBucketMask(pDrawable->GetShaderGroup());

        pDrawblData->SetHasAlpha(false);
        pDrawblData->SetHasDecal(false);
        pDrawblData->SetHasCutout(false);

		// check if this ped has a decal decoration
		if (pDrawable && pDrawable->GetShaderGroup().LookupShader("ped_decal_decoration"))
		{
			pNewVarData->m_bIsCurrVarDecalDecoration = true;
		}

		// update flags for LOD0 and LOD1:
		for(u32 lod=0; lod<2; lod++)
		{
			if(pDrawable->GetLodGroup().ContainsLod(lod))
			{
				pPed->SetLodBucketInfoExists(lod, true);

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_ALPHA))
				{
					pDrawblData->SetHasAlpha(true);
					pPed->SetLodHasAlpha(lod, true);
				}
				
				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_DECAL))
				{
					pDrawblData->SetHasDecal(true);
					pPed->SetLodHasDecal(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_CUTOUT))
				{
					pDrawblData->SetHasCutout(true);
					pPed->SetLodHasCutout(lod, true);
				}
			}
		}

		// update ped bucket mask bits only for LOD2/LOD3
		for (u32 lod=2; lod<LOD_COUNT; lod++)
		{
			if(pDrawable->GetLodGroup().ContainsLod(lod))
			{
				pPed->SetLodBucketInfoExists(lod, true);

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_ALPHA))
				{
					pPed->SetLodHasAlpha(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_DECAL))
				{
					pPed->SetLodHasDecal(lod, true);
				}

				if (pDrawable->GetBucketMask(lod)&(0x01<<CRenderer::RB_CUTOUT))
				{
					pPed->SetLodHasCutout(lod, true);
				}
			}
		}

		characterCloth* pCCloth = pGfxData->GetCharacterCloth(i);
		if( pCCloth )
		{
			clothVariationData* pClothVarData = CPedVariationPack::CreateCloth( pCCloth, pPed->GetSkeleton(), pGfxData->m_cldIdx[i] );
			Assert( pClothVarData );

			u8 _flags = pPed->m_CClothController[i] ? pPed->m_CClothController[i]->GetFlags() : 0;
			u8 _packageIndex = pPed->m_CClothController[i] ? ((characterClothController*)pPed->m_CClothController[i])->GetPackageIndex(): 0;

			characterClothController* pCharClothController = pClothVarData->GetCloth()->GetClothController();
			pPed->m_CClothController[i] = pCharClothController;
		#if GTA_REPLAY
			CReplayMgr::RecordCloth(pPed, (rage::clothController *)pPed->m_CClothController[i], i);
		#endif // GTA_REPLAY

			Assert( pCharClothController );

			if( _flags)
			{
				// NOTE: copy over the flags, the variation may have been re-created in the middle of a cutscene for some reason
				pCharClothController->SetFlags( _flags );
			}
			else
			{
				const bool bForceSkin = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth );			
				pCharClothController->SetFlag( characterClothController::enIsForceSkin, bForceSkin );
			}

			if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcePackageCharacterCloth ) )
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePackageCharacterCloth, false );
				pCharClothController->SetPackageIndex( pPed->GetQueuedClothPackageIndex() );
			}
			else
			{
				pCharClothController->SetPackageIndex(_packageIndex);
			}

			if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth ))
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth, false );
				pCharClothController->SetFlag( characterClothController::enIsProneFlipped, true );
			}

			if( pCharClothController->GetFlag( characterClothController::enIsQueuedPose ) 
				|| pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcePoseCharacterCloth )
				)
			{
				pCharClothController->SetFlag( characterClothController::enIsQueuedPose, false );
				pCharClothController->SetForcePin( 0 );
		
				characterCloth* pCharCloth =  (characterCloth*)pCharClothController->GetOwner();
				Assert( pCharCloth );

				pCharCloth->SetPose( pPed->GetQueuedClothPoseIndex() );				

				for(int k = 0; k < NUM_CLOTH_BUFFERS; ++k)
				{
					Vec3V* RESTRICT verts = pClothVarData->GetClothVertices(k);
					Assert( verts );
					// NOTE: don't want to crash if the game run out of streaming memory 
					if( verts )
					{
					// TODO character cloth doesn't support LODs yet
						const int lodIndex = 0;
						phVerletCloth* pCloth = pCharClothController->GetCloth(lodIndex);
						Assert( pCloth );
						Vec3V* RESTRICT origVerts = pCloth->GetClothData().GetVertexPointer();
						Assert( origVerts );
						// origVerts should exists, if this fails there is bug somewhere in cloth instancing
						for(int j=0; j < pCloth->GetNumVertices(); ++j)
						{
							verts[j] = origVerts[j];
						}
					}
				}
			} // ...pCharClothController->GetFlag

			Assert( !pGfxData->m_clothData[i] );
			pGfxData->m_clothData[i] = pClothVarData;
#if HAS_RENDER_MODE_SHADOWS
			pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);	// We have to do the shadows differently for clothed peds...
#endif // HAS_RENDER_MODE_SHADOWS
		}
		else
		{
			if( pPed->m_CClothController[i] )
			{
				clothInstance* pClothInstace = (clothInstance*)pPed->m_CClothController[i]->GetClothInstance();
				Assert( pClothInstace );
				pClothInstace->SetActive(false);
				pPed->m_CClothController[i] = NULL;
			}

			if(pPed->IsLocalPlayer())
			{
			#if HAS_RENDER_MODE_SHADOWS
		 		// force custom shadows for player ped (using um, etc.)
 				pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
			#endif // HAS_RENDER_MODE_SHADOWS
			}

			const CPedModelInfo* pPedMI = pPed->GetPedModelInfo();
			if(pPedMI && pPedMI->GetIsPlayerModel())
			{
			#if HAS_RENDER_MODE_SHADOWS
 				// force custom shadows for all 3 player peds (using um, etc.)
 				pPed->SetRenderFlag(CEntity::RenderFlag_USE_CUSTOMSHADOWS);
			#endif // HAS_RENDER_MODE_SHADOWS
			}
		}

		if (pGfxData->m_pShaderEffect)
			pGfxData->m_pShaderEffect->ClearTextures((ePedVarComp)i);

		// setup this drawable and this texture together
		if (pDiffMap){
			// We need to call this even if the type isn't already NULL because it may be a new pDrawable that
			// still needs to have its shadergroupvar's set up properly.
			pGfxData->m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(CCustomShaderEffectPed::SetupForDrawableComponent(pDrawable, i, pGfxData->m_pShaderEffectType));

#if FPS_MODE_SUPPORTED
			rmcDrawable* pFpAlt = pGfxData->GetFpAlt(i);
			if (pFpAlt)
				pGfxData->m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(CCustomShaderEffectPed::SetupForDrawableComponent(pFpAlt, i, pGfxData->m_pShaderEffectType));
#endif // FPS_MODE_SUPPORTED

			if (pGfxData->m_pShaderEffectType && !pGfxData->m_pShaderEffect)
				pGfxData->m_pShaderEffect = static_cast<CCustomShaderEffectPed*>(pGfxData->m_pShaderEffectType->CreateInstance(pPed));

			if(pGfxData->m_pShaderEffect)	{
				pGfxData->m_pShaderEffect->SetDiffuseTexture(pDiffMap, i);
			}
		}

		// setup palette texture, if available it should be in the dwd
		if (pGfxData->m_pShaderEffect)
		{
			SetPaletteTexture(pPed, i);
		}
	}

	if (needsSkeletonMap)
		SetupSkeletonMap(pPedModelInfo, pGfxData, false);

	// we use the head drawable for lod numbers and always keep the largest number recorded
	rmcDrawable* draw = pGfxData->GetDrawable(PV_COMP_HEAD);
	if (Verifyf(draw, "No drawable for newly streamed in head component! (%s)", pPed->GetModelName()))
	{
		if (draw->GetLodGroup().ContainsLod(LOD_LOW))
			pPedModelInfo->SetNumAvailableLODs(3);
		else if (draw->GetLodGroup().ContainsLod(LOD_MED))
			pPedModelInfo->SetNumAvailableLODs(2);
		else if (draw->GetLodGroup().ContainsLod(LOD_HIGH))
			pPedModelInfo->SetNumAvailableLODs(1);
	}

	CPedVariationPack::UpdatePedForAlphaComponents(pPed, pPed->GetPedDrawHandler().GetPropData(), pPed->GetPedDrawHandler().GetVarData());

	// Support for having different ethnic origin heads in the same ped pack
	// Different ethnic origin heads have the same number of bones, the same hierarchy and the same boneids
	// But the facial bones may be in slightly different positions and of slightly different lengths
	// The skeletondata loaded with frag (body model) may not be correct for the given head model
	// So we change the skeletondata to use the skeletondata from the head model
    if (headBoneCount == pPed->GetSkeletonData().GetNumBones())
	{
		bool copySkel = false;
		rmcDrawable* pHeadComponent = GetComponent(PV_COMP_HEAD, pPed, *pNewVarData);

        if (pPedModelInfo->m_bIsHeadBlendPed || (pPed->HasHeadBlend() && pPed->GetPedDrawHandler().GetConstPedRenderGfx()))
		{
            rmcDrawable* newHeadComponent = pPed->GetPedDrawHandler().GetConstPedRenderGfx()->GetHeadBlendDrawable(pPed->IsMale() ? HBS_HEAD_1 : HBS_HEAD_0);
            if (newHeadComponent)
                pHeadComponent = newHeadComponent;
			copySkel = newHeadComponent != NULL;
		}

		// we only change skeleton if we don't have a blend head or if we do but the blend head is streamed in and we can copy the skel data
		// otherwise it'll just be temporary and cause a visual artifact until the head blend drawable is streamed in
		if (pHeadComponent && (!pPedModelInfo->m_bIsHeadBlendPed || copySkel))
		{
            if (artVerifyf(pPed->GetSkeletonData().GetNumBones() ==  pHeadComponent->GetSkeletonData()->GetNumBones(), 
				"Ped %s has mismatched number of bones in frag and head component %d", pPedModelInfo->GetModelName(), pNewVarData->m_aDrawblId[PV_COMP_HEAD]))
            {
                if (artVerifyf(pPed->GetSkeletonData().GetSignature() ==  pHeadComponent->GetSkeletonData()->GetSignature(), 
				"Ped %s has mismatched skeleton signature in frag and head component %d (ie. it has different DOFs)", pPedModelInfo->GetModelName(), pNewVarData->m_aDrawblId[PV_COMP_HEAD]))
                {
					// Dirty hack until we re-export the skeletondatas, make these bones animatable in translation
					pPed->SetupSkeletonData(const_cast<crSkeletonData&>(*pHeadComponent->GetSkeletonData()));

					if (copySkel)
						pPed->GetFragInst()->GetCacheEntry()->CopySkeletonData(*pHeadComponent->GetSkeletonData());
					else
						pPed->GetFragInst()->GetCacheEntry()->SetSkeletonData(*pHeadComponent->GetSkeletonData());

#if __DEV
                    for (s32 t = 0; t < NUM_BLEND_PEDS; ++t)
                    {
                        if (!stricmp(s_mpBlendPeds[t], pPedModelInfo->GetModelName()))
                            artAssertf(!pPed->HasHeadBlend() || copySkel, "We're not copying the head skeleton for ped '%s', meaning we'll most likely crash later when streaming out assets! Does the head skeleton not match the frag one?", pPedModelInfo->GetModelName());

                    }
#endif // __DEV
				}
            }
		}
	}	

	pPed->AddShaderVarCreatureComponent(pGfxData->m_pShaderEffect);

	pPed->GetAnimDirector()->InitExpressions(false);

	pPed->CheckComponentCloth();
}

void CPedVariationStream::ApplyHdAssets(CPed* pPed, CPedStreamRequestGfx* pPSGfx)
{
	CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	Assert(pPedModelInfo);
	if (!pPedModelInfo->GetIsStreamedGfx())
		return;

	CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
	Assertf(pGfxData, "PedRenderGfx is NULL - have stream ped variations been set up? Have the files been requested and streamed in successfully?");
    if (!pGfxData)
        return;

	pGfxData->SetHdAssets(pPSGfx);

	// fix up drawables and textures and find number of bones for each split skeleton, if we want a split skeleton
	s32 boneCount = -1;
	u32 numDrawables = 0;
	bool needsSkeletonMap = false;
	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		rmcDrawable* pDrawable = pGfxData->GetHDDrawable(i);
		if (pDrawable)
		{
			numDrawables++;
			pGfxData->m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(CCustomShaderEffectPed::SetupForDrawableComponent(pDrawable, i, pGfxData->m_pShaderEffectType));

			if (!needsSkeletonMap)
			{
                crSkeletonData* skel = pDrawable->GetSkeletonData();
                if (skel)
				{
					if (boneCount == -1)
						boneCount = skel->GetNumBones();
					else if (boneCount != skel->GetNumBones())
						needsSkeletonMap = true;
				}
			}
		}

#if 0 // we don't need to replace the texture, it will be the same one fixed up with an extra mip by texLod
		grcTexture* pDiffMap = pGfxData->GetHDTexture(i);
		if (!pDiffMap && pDrawable)
			pDiffMap = pGfxData->GetTexture(i);

		if (!pDiffMap)
			continue;

		if (pGfxData->m_pShaderEffect)
			pGfxData->m_pShaderEffect->SetDiffuseTexture(pDiffMap, i);
#endif
	}

	// create skeleton map for hd assets
	if (needsSkeletonMap || (boneCount > -1 && numDrawables == 1))
		SetupSkeletonMap(pPedModelInfo, pGfxData, true);
}

void CPedVariationStream::RemoveHdAssets(CPed* pPed)
{
	CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	Assert(pPedModelInfo);
	if (!pPedModelInfo->GetIsStreamedGfx())
		return;

	CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
	Assertf(pGfxData, "PedRenderGfx is NULL - have stream ped variations been set up? Have the files been requested and streamed in successfully?");
	if(!pGfxData)
		return;

	s32 boneCount = -1;
	u32 numDrawables = 0;
	bool needsSkeletonMap = false;

	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		rmcDrawable* pDrawable = pGfxData->GetDrawable(i);
		if (pGfxData->GetHDDwdIndex(i) != -1 && pDrawable)
			pGfxData->m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(CCustomShaderEffectPed::SetupForDrawableComponent(pDrawable, i, pGfxData->m_pShaderEffectType));

		grcTexture* pDiffMap = pGfxData->GetTexture(i);

#if 0
		if ((pGfxData->GetHDDwdIndex(i) != -1 || pGfxData->GetHDTxdIndex(i) != -1) && pDiffMap)
#else
		if (pDiffMap)
#endif
			pGfxData->m_pShaderEffect->SetDiffuseTexture(pDiffMap, i);

		if (pDrawable)
		{
			numDrawables++;
			if (!needsSkeletonMap)
			{
				crSkeletonData* skel = pDrawable->GetSkeletonData();
				if (skel)
				{
					if (boneCount == -1)
						boneCount = skel->GetNumBones();
					else if (boneCount != skel->GetNumBones())
						needsSkeletonMap = true;
				}
			}
		}
	}

	if (needsSkeletonMap || (boneCount > -1 && numDrawables == 1))
		SetupSkeletonMap(pPedModelInfo, pGfxData, false);

	pGfxData->SetHdAssets(NULL);
}

void CPedVariationStream::SetupSkeletonMap(CPedModelInfo* pmi, CPedStreamRenderGfx* gfx, bool hd)
{
	crSkeletonData* mainSkel = NULL;
	if(pmi->GetFragType())
		mainSkel = pmi->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	else
		mainSkel = pmi->GetDrawable()->GetSkeletonData();

	if (mainSkel)
	{
		for (u32 i = 0; i < PV_MAX_COMP; ++i)
		{
			rmcDrawable* pDrawable = hd ? gfx->GetHDDrawable(i) : gfx->GetDrawable(i);
			if (pDrawable)
			{
				crSkeletonData* skel = pDrawable->GetSkeletonData();
				if (skel)
				{
					Assertf(skel->GetNumBones() < 256, "Too many bones %d in ped skeleton '%s' for component '%s'", skel->GetNumBones(), pmi->GetModelName(), varSlotNames[i]);

					if (gfx->m_skelMapBoneCount[i] < (u8)skel->GetNumBones())
					{
						delete[] gfx->m_skelMap[i];
						gfx->m_skelMap[i] = rage_aligned_new(16) u16[skel->GetNumBones()]; // these will be dmad to the spus
					}

					gfx->m_skelMapBoneCount[i] = (u8)skel->GetNumBones();

					for (s32 f = 0; f < gfx->m_skelMapBoneCount[i]; ++f)
					{
						// get bone indices as they are in the head skeleton
						s32 boneIndex = 0;
						if (!Verifyf(mainSkel->ConvertBoneIdToIndex(skel->GetBoneData(f)->GetBoneId(), boneIndex), "Couldn't find bone index for id %d and slot '%s' (%s) in head skeleton for '%s'", skel->GetBoneData(f)->GetBoneId(), varSlotNames[i], skel->GetBoneData(f)->GetName(), pmi->GetModelName()))
						{
							boneIndex = (f > 0) ? gfx->m_skelMap[i][f - 1] : 0;
						}

						Assert(boneIndex < 65536);
						gfx->m_skelMap[i][f] = (u16)(boneIndex & 0xffff);
					}
				}
			}
		}
	}
}

void CPedVariationStream::TriggerHDTxdUpgrade(CPed* pPed)
{
	CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
	Assert(pGfxData);

	if (pGfxData)
	{
		u32 modelIndex = pPed->GetModelIndex();
		for (u32 i = 0; i < PV_MAX_COMP; ++i)
		{
			s32 txdIndex = pGfxData->GetTxdIndex(i);
			if (txdIndex != -1 && ((pGfxData->m_hasHighTxdForComponentTxd & (1<<i)) != 0))
			{
				fwAssetLocation assetLoc(STORE_ASSET_TXD, txdIndex);
				CTexLod::AddHDTxdRequest(assetLoc, modelIndex);
			}

			s32 dwdIndex = pGfxData->GetDwdIndex(i);
			if (dwdIndex != -1 && ((pGfxData->m_hasHighTxdForComponentDwd & (1<<i)) != 0))
			{
				fwAssetLocation assetLoc(STORE_ASSET_DWD, dwdIndex);
				CTexLod::AddHDTxdRequest(assetLoc, modelIndex);
			}
		}
	}
}

// determine if blendshapes have been set up on the given drawable or not
bool CPedVariationStream::IsBlendshapesSetup(rmcDrawable* pDrawable)
{

	// kind of fiddly to determine if blendshapes set up or not...
	grmModel* pModel = pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0);
	Assert(pModel);
	for(s32 n=0; n < pModel->GetGeometryCount(); n++)
	{
		// I'm assuming that I don't need to check further than first LOD & model for player head
		int geomType = pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(n).GetType();
		if (geomType==grmGeometry::GEOMETRYEDGE)
		{
			// nothing to do here - EDGE applies blendshapes
		}
		else if (geomType==grmGeometry::GEOMETRYQB)
		{
			grmGeometryQB& geom = (grmGeometryQB&)(pDrawable->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(n));
			if (geom.HasOffsets())
			{
				return(true);
			}
		}
	}
	return(false);
}

extern void DrawPedLODLevel(const Matrix34& matrix, s32 lodLevel, bool bIsSlod, bool bIsStreamed);

// render a ped without the CPed ptr
void CPedVariationStream::RenderPed(CPedModelInfo* pModelInfo, const Matrix34& rootMatrix, CAddCompositeSkeletonCommand& skeletonCmd, CPedStreamRenderGfx* pGfx,
						CCustomShaderEffectPed *pShaderEffect, u32 bucketMask, eRenderMode renderMode, u16 drawableStats, u8 lodIdx, u32 perComponentWetness, bool bIsFPSSwimming, bool bIsFpv, bool bEnableMotionBlurMask, bool bSupportsHairTint)
{
	Assert(pGfx);

	const bool mirrorPhase = DRAWLISTMGR->IsExecutingMirrorReflectionDrawList();
	const bool shadowPhase = DRAWLISTMGR->IsExecutingShadowDrawList();
	const bool globalReflPhase = DRAWLISTMGR->IsExecutingGlobalReflectionDrawList();
	const bool waterReflPhase = DRAWLISTMGR->IsExecutingWaterReflectionDrawList();
	bool allowStripHead = !mirrorPhase && !shadowPhase && !globalReflPhase && (!waterReflPhase || bIsFPSSwimming);

	const bool bIsPlayerModel = pModelInfo->GetIsPlayerModel();

	const grmMatrixSet* overrideMs = NULL;
	bool bOverrideMtxSet = dlCmdAddCompositeSkeleton::IsCurrentMatrixSetOverriden();
	bool bOverrideRootMtxOnly = dlCmdAddCompositeSkeleton::IsCurrentRootMatrixOverriden();

	Matrix34 currentRootMatrix(rootMatrix);
	if (bOverrideMtxSet)
	{
		Mat34V newRootMtx;
		dlCmdAddCompositeSkeleton::GetCurrentMatrixSetOverride(overrideMs, newRootMtx);
		currentRootMatrix = RCC_MATRIX34(newRootMtx);
	}
	else if (bOverrideRootMtxOnly)
	{
		Mat34V newRootMtx;
		dlCmdAddCompositeSkeleton::GetCurrentRootMatrixOverride(newRootMtx);
		currentRootMatrix = RCC_MATRIX34(newRootMtx);
	}

//	const bool bRenderHiLOD = (lodIdx == 0);

	bool noHd = true;
	CCustomShaderEffectPed::EnableShaderVarCaching(false);
	bool bSetShaderVarCaching = false;

	u8 requestedLod = lodIdx;
	// go through all the component slots and draw the component for this ped
	for(u32 i=0;i<PV_MAX_COMP;i++)	
	{
		rmcDrawable* pDrawable = NULL;

		// check the don't draw array - see if anything needs skipped
		if (ms_aDrawPlayerComp[i] == false){
			continue;
		}

		// skip culled components (culling happens in CPed::UpdatedLodState)	
		if (pGfx->m_culledComponents & (1 << i))
		{
			continue;
		}

		const grmMatrixSet* ms = NULL;
		if (bOverrideMtxSet)
		{
			ms = overrideMs;
		}
		else
		{
			skeletonCmd.Execute(i);
			ms = dlCmdAddCompositeSkeleton::GetCurrentMatrixSet();
		}
		Assert(ms);

		rmcDrawable* pFpvDrawable = NULL;
		if (bIsFpv && pGfx->m_hdDwdIdx[i] == -1)
			pFpvDrawable = pGfx->GetFpAlt(i);

		if (!pFpvDrawable && allowStripHead && dlCmdAddCompositeSkeleton::GetIsStrippedHead() && (i == PV_COMP_HEAD || i == PV_COMP_TEEF || i == PV_COMP_HAIR || i == PV_COMP_BERD || (pGfx->m_cullInFirstPerson & (1 << i))))
			continue;

		if (pGfx->m_hdDwdIdx[i] == -1)
		{
			pDrawable = pFpvDrawable;
			if (!pDrawable)
				pDrawable = pGfx->GetDrawable(i);
		}
		else
		{
			pDrawable = pGfx->GetHDDrawable(i);

			if (i == PV_COMP_HEAD && pDrawable && pDrawable->GetSkeletonData())
				noHd = dlCmdAddCompositeSkeleton::GetNumHeadBones() != pDrawable->GetSkeletonData()->GetNumBones();

			if (!pDrawable || noHd)
				pDrawable = pGfx->GetDrawable(i);
		}

		if (pDrawable)
		{
			clothVariationData* clothVarData = pGfx->GetClothData(i);
			characterCloth* pCCloth = NULL;

			Matrix34 *parentMatrix = NULL;
#if __XENON
			rmcLodGroup &group = pDrawable->GetLodGroup();
			grmModel &model = group.GetLodModel0(LOD_HIGH);
#endif
			if( clothVarData )
			{
				const int bufferIdx = clothManager::GetBufferIndex(true);
				parentMatrix = (Matrix34*)clothVarData->GetClothParentMatrixPtr(bufferIdx);
				Assert( parentMatrix );
				pCCloth = (characterCloth*)clothVarData->GetCloth();
#if __XENON				
				grcVertexBuffer* clothVtxBuffer = clothVarData->GetClothVtxBuffer(bufferIdx);
				Assert( clothVtxBuffer );

				// Don't check the length of the secondary stream on cloth garments: they don't match, but it's on purpose.
				ASSERT_ONLY(grmGeometry::SetCheckSecondaryStreamVertexLength(false));
				for(int j=0;j<model.GetGeometryCount();j++)
				{
					model.GetGeometry(j).SetOffsets(clothVtxBuffer);
				}
#endif
#if __PS3 && SPU_GCM_FIFO
				SPU_COMMAND(GTA4__SetCharacterClothData,clothVarData->GetClothNumVertices());
					cmd->morphData = (void*)clothVarData->GetClothVertices( clothManager::GetBufferIndex(true) );
#endif // __PS3 && SPU_GCM_FIFO
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
				if ( clothVarData )
				{
					const int bufferIdx = clothManager::GetBufferIndex(true);
					pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
				}
#endif //  __WIN32PC || RSG_DURANGO || RSG_ORBIS
			}

			lodIdx = requestedLod;
			while (!pDrawable->GetLodGroup().ContainsLod(lodIdx) && lodIdx > 0)
			{
				lodIdx--;
			}

			const bool bRenderPresentHiLOD = (lodIdx == 0);

			if (!pDrawable->GetLodGroup().ContainsLod(lodIdx))
			{
				// no lod, skip rendering
			}
			else if (renderMode==rmStandard) 
			{
				// draw the hi LOD
				if (pShaderEffect)
				{
					u32 wetness = perComponentWetness >> (i*2);
					static const int wetnessAdjust[4] = {0, 1, -1, 0};
					pShaderEffect->PedVariationMgr_SetWetnessAdjust(wetnessAdjust[wetness & 3]);

#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
					pShaderEffect->PedVariationMgr_SetShaderVariables(pDrawable, i, parentMatrix);
#else // __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
					pShaderEffect->PedVariationMgr_SetShaderVariables(pDrawable, i);
#endif // __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
					if(Unlikely(!bSetShaderVarCaching))
					{
						CCustomShaderEffectPed::EnableShaderVarCaching(true);
						bSetShaderVarCaching = true;
					}
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
					if ( clothVarData )
					{
						const int bufferIdx = clothManager::GetBufferIndex(true);
						pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
					}
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS
				}

				bool bRenderExpensiveHairSpiked	= FALSE;
				bool bRenderExpensiveFur		= FALSE;
				bool bRenderHairCutout			= FALSE;
				bool bRenderHairNothing			= FALSE;

				if (bRenderPresentHiLOD)
				{
					if(DRAWLISTMGR->IsExecutingGBufDrawList() || DRAWLISTMGR->IsExecutingHudDrawList()) // expensive hair allowed only in GBuffer stage
					{
						if((i==PV_COMP_HAIR) || (i==PV_COMP_BERD) || (i==PV_COMP_ACCS))
						{
							if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
							{
								bRenderExpensiveHairSpiked = TRUE;
							}
						}

						const bool IsFurShader	= pShaderEffect->IsFurShader();
						const bool IsInCutout	= CRenderer::IsBaseBucket(bucketMask,CRenderer::RB_CUTOUT);
						const bool IsPedFur		= pDrawable->GetShaderGroup().LookupShader("ped_fur") != NULL;
						bRenderExpensiveFur = IsFurShader && IsInCutout && IsPedFur;
					}// if(gDrawListMgr->IsExecutingGBufDrawList())...
					// spiked hair allowed in DL_MIRROR_REFLECTION:
					else if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList())
					{
						if((i==PV_COMP_HAIR) || (i==PV_COMP_BERD) || (i==PV_COMP_ACCS))
						{
							if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
							{
								bRenderExpensiveHairSpiked = TRUE;
							}
						}
					}
					// spiked hair allowed in DL_WATER_REFLECTION, but only for player peds:
					else if(bIsPlayerModel && DRAWLISTMGR->IsExecutingWaterReflectionDrawList())
					{
						if((i==PV_COMP_HAIR) || (i==PV_COMP_BERD) || (i==PV_COMP_ACCS))
						{
							if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
							{
								bRenderExpensiveHairSpiked = TRUE;
							}
						}
					}
					// spiked hair allowed in DL_SHADOWS stage
					else if(DRAWLISTMGR->IsExecutingShadowDrawList())
					{
						if(i==PV_COMP_HAIR)
						{
							if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
							{
								bRenderExpensiveHairSpiked = TRUE;
							}
						}
						else if((i==PV_COMP_BERD) || (i==PV_COMP_ACCS))
						{
							if(pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_enveff") || pDrawable->GetShaderGroup().LookupShader("ped_hair_spiked_mask"))
							{
								bRenderHairNothing = TRUE;
							}
						}
					}// if(gDrawListMgr->IsExecutingShadow)...

					// hair cutout is always rendered, regardless of renderphase:
					if((i==PV_COMP_HAIR) || (i==PV_COMP_BERD) || (i==PV_COMP_ACCS))
					{
						if(pDrawable->GetShaderGroup().LookupShader("ped_hair_cutout_alpha") || pDrawable->GetShaderGroup().LookupShader("ped_hair_cutout_alpha_cloth"))
						{
							bRenderHairCutout = TRUE;
						}
					}
				} // if (bRenderHiLOD)

				if (bEnableMotionBlurMask)
				{
					CShaderLib::BeginMotionBlurMask();
				}
				
				if(bRenderHairNothing)
				{
					// render nothing
				}
				else if(bRenderExpensiveHairSpiked)
				{
					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PushHairTintTechnique();

					CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *ms, bucketMask, lodIdx, CShaderHairSort::HAIRSORT_SPIKED);

					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PopHairTintTechnique();
				}
				else if(bRenderHairCutout)
				{
					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PushHairTintTechnique();

					CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *ms, bucketMask, lodIdx, CShaderHairSort::HAIRSORT_CUTOUT);

					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PopHairTintTechnique();
				}
				else if(bRenderExpensiveFur)
				{
					Vector3 wind;
					pShaderEffect->GetLocalWindVec(wind);
					CShaderHairSort::DrawableDrawSkinned(pDrawable, currentRootMatrix, *ms, bucketMask, lodIdx, CShaderHairSort::HAIRSORT_FUR,&wind);
				}
				else
				{
					if (i==0) { DrawPedLODLevel(currentRootMatrix, lodIdx, false, true); }

					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PushHairTintTechnique();
					pDrawable->DrawSkinned(currentRootMatrix, *ms, bucketMask, lodIdx, drawableStats);
					if (bSupportsHairTint && i == PV_COMP_HAIR)
						CShaderHairSort::PopHairTintTechnique();

				}

				if (bEnableMotionBlurMask)
				{
					CShaderLib::EndMotionBlurMask();
				}

				if (pShaderEffect)
				{
					pShaderEffect->ClearShaderVars(pDrawable, i);
				}
			} 
			else if (NULL == pCCloth)
			{
#if HAS_RENDER_MODE_SHADOWS
				if (IsRenderingShadowsNotSkinned(renderMode))
				{
					pDrawable->DrawSkinnedNoShaders(currentRootMatrix, *ms, bucketMask, lodIdx, rmcDrawable::RENDER_NONSKINNED);
				}
				else if (IsRenderingShadowsSkinned(renderMode))
				{
					pDrawable->DrawSkinnedNoShaders(currentRootMatrix, *ms, bucketMask, lodIdx, rmcDrawable::RENDER_SKINNED);
				}
#endif // HAS_RENDER_MODE_SHADOWS
			}

			if( pCCloth )
			{
#if __XENON
				for(int j=0;j<model.GetGeometryCount();j++)
				{
					model.GetGeometry(j).SetOffsets(NULL);
				}

				ASSERT_ONLY(grmGeometry::SetCheckSecondaryStreamVertexLength(true));
#endif // __XENON
#if __PS3 && SPU_GCM_FIFO
				SPU_COMMAND(GTA4__SetCharacterClothData,0);
					cmd->morphData = (void*)NULL;
#endif // __PS3 && SPU_GCM_FIFO
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
				if ( clothVarData )
				{
					const int bufferIdx = clothManager::GetBufferIndex(true);
					pShaderEffect->PedVariationMgr_SetShaderClothVertices(pDrawable, clothVarData->GetClothNumVertices(), clothVarData->GetClothVertices(bufferIdx));
				}
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS
				
			}
			
		}
	}
	CCustomShaderEffectPed::EnableShaderVarCaching(false);
	pShaderEffect->ClearGlobalShaderVars();
}

//	Set all components to 0
void	CPedVariationStream::SetVarDefault(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 streamingFlags)
{
	Assert(pDynamicEntity);
#if __ASSERT
	if(!pDynamicEntity)
		return;
#endif // __ASSERT

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);
#if __ASSERT
	if(!pModelInfo)
		return;
#endif // __ASSERT

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return;

	if (pVarInfo->IsSuperLod()){
		return;
	}

	if(pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pDynamicEntity);
		for(u32 lod = 0; lod < LOD_COUNT; lod++)
		{
			pPed->SetLodBucketInfoExists(lod, false);
			pPed->SetLodHasAlpha(lod, false);
			pPed->SetLodHasDecal(lod, false);
			pPed->SetLodHasCutout(lod, false);
		}
	}

	for(s32 i = 0; i<PV_MAX_COMP; i++)
	{
		SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(i), 0, 0, 0, 0, streamingFlags, false);
	}

	CPedVariationPack::UpdatePedForAlphaComponents(pDynamicEntity, pedPropData, pedVarData);
}


bool	CPedVariationStream::IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB)
{
	// races are compatible if they are the same, or if either one is universal
	if ((typeA != typeB) &&
		(typeA != PV_RACE_UNIVERSAL) &&
		(typeB != PV_RACE_UNIVERSAL))
	{
		return(false);			// non matching race types found
	}

	return(true);
}


u8 CPedVariationStream::GetPaletteVar(CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);

	return(pPed->GetPedDrawHandler().GetVarData().GetPedPaletteIdx(slotId));
}

u8 CPedVariationStream::GetTexVar(CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);
	
	return(pPed->GetPedDrawHandler().GetVarData().GetPedTexIdx(slotId));
}

u32 CPedVariationStream::GetCompVar(CPed* pPed, ePedVarComp slotId)
{
	Assert(pPed);
	
	return(pPed->GetPedDrawHandler().GetVarData().GetPedCompIdx(slotId));
}



// Return if ped has the variation
bool CPedVariationStream::HasVariation(CPed* pPed, ePedVarComp slotId, u32 drawblId, u32 texId)
{
	Assert(pPed);

	const CPedVariationData& varData = pPed->GetPedDrawHandler().GetVarData();

	return (varData.GetPedCompIdx(slotId) == drawblId && varData.GetPedTexIdx(slotId) == texId);
}

bool 
CPedVariationStream::IsVariationValid(CDynamicEntity* pDynamicEntity, 
								   ePedVarComp componentId, 
								   s32 drawableId, 
								   s32 textureId)
{
	Assert(pDynamicEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return false;

	if ( drawableId >= pVarInfo->GetMaxNumDrawbls(componentId) || textureId >= pVarInfo->GetMaxNumTex(componentId, drawableId) )
	{
		return false;
	}

	if (!pVarInfo->IsValidDrawbl(componentId, drawableId))
	{
		return false;
	}

	return true;
}



// set the component part of the given ped to use the desired drawable and desired texture
// return false if unable to set the variation
bool	CPedVariationStream::SetVariation(CDynamicEntity* pDynamicEntity,  CPedPropData& pedPropData, CPedVariationData& pedVarData, 
									   ePedVarComp slotId, u32 drawblId, u32 drawablAltId, u32 texId, u32 paletteId, s32 streamFlags, bool force)
{
	Assert(pDynamicEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);
	Assert(pModelInfo->GetIsStreamedGfx());

	CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
	Assert(pVarInfo);

	if (!pVarInfo)
	{
		return false;
	}

	Assert(dynamic_cast<CPed*>(pDynamicEntity));
	CPed* pPed = static_cast<CPed*>(pDynamicEntity);
	const CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetConstPedRenderGfx();
	if (pGfxData){
		rmcDrawable* pDrawable = pGfxData->GetDrawable(PV_COMP_HEAD);
		if (pDrawable && IsBlendshapesSetup(pDrawable)){
			Assertf(false,"Can't modify player variation when blendshapes are enabled");
			return(false);
		}
	}

	CPVDrawblData* pDrawblData = NULL;
	if (drawblId != PV_NULL_DRAWBL)
	{
		pDrawblData = pVarInfo->GetCollectionDrawable(slotId, drawblId);
		if (!pDrawblData) { return false;}

		if (!pDrawblData->HasActiveDrawbl())
		{
			return false;
		}

		//If this flag is set, set the texture variation to the 0th component.
		texId = pDrawblData->IsProxyTex() ? 0 : texId;
	}

	Assertf(drawblId != PV_NULL_DRAWBL || slotId != PV_COMP_HEAD, "Empty drawable cannot be set on head component! (%s)", pDynamicEntity->GetModelName());
	Assertf(drawblId == PV_NULL_DRAWBL || drawblId < pVarInfo->GetMaxNumDrawbls(slotId), " DrawableId(%d) >= (%d)MaxNumDrawbls || Invalid variation: %s : %s : (drawable %d, texture %d) [%s_%03d_%c]", 
																 drawblId, 
																 pVarInfo->GetMaxNumDrawbls(slotId), 
																 pModelInfo->GetModelName(),
																 varSlotNames[slotId],
																 drawblId, 
																 texId,
																 varSlotNames[slotId],
																 drawblId,
																 'a' + texId);

	Assertf(drawblId == PV_NULL_DRAWBL || texId < pVarInfo->GetMaxNumTex(slotId, drawblId), " texId(%d) >= (%d)MaxNumTex || Invalid variation: %s : %s : (drawable %d, texture %d) [%s_%03d_%c]", 
																	texId, 
																	pVarInfo->GetMaxNumTex(slotId, drawblId), 
																	pModelInfo->GetModelName(), 
																	varSlotNames[slotId],
																	drawblId, 
																	texId,
																	varSlotNames[slotId],
																	drawblId,
																	'a' + texId);

	Assertf(drawblId == PV_NULL_DRAWBL || drawablAltId <= pDrawblData->GetNumAlternatives(), " drawablAltId(%d) > (%d)NumAlternatives || Invalid variation: %s : %s : (drawable %d, texture %d) [%s_%03d_%c]", 
																	drawablAltId, 
																	pDrawblData->GetNumAlternatives(), 
																	pModelInfo->GetModelName(), 
																	varSlotNames[slotId],
																	drawblId, 
																	texId,
																	varSlotNames[slotId],
																	drawblId,
																	'a' + texId);

	bool requestAssets = false;

    // if we're setting an alt we ignore this code, or we might get loops with bad metadata.
    // plus, we don't want alt drawables to include additional alts
    if (drawablAltId == 0)
    {
        const CAlternateVariations::sAlternateVariationPed* altPedVar = GetAlternateVariations().GetAltPedVar(pModelInfo->GetModelNameHash());
        if (altPedVar)
        {
			CAlternateVariations::sAlternates* altStorage = Alloca(CAlternateVariations::sAlternates, MAX_NUM_ALTERNATES);
			atUserArray<CAlternateVariations::sAlternates> alts(altStorage, MAX_NUM_ALTERNATES);

			u32 globalDrawIdx = pedVarData.GetPedCompIdx((ePedVarComp)slotId);
			s32 localDrawIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)slotId, globalDrawIdx);
			u32 dlcNameHash = pVarInfo->GetDlcNameHashFromDrawableIdx((ePedVarComp)slotId, globalDrawIdx);

            // check if the variation we replace forced an alternate drawable when it was set. that means we need to undo that now
            if (altPedVar->GetAlternates((u8)slotId, (u8)localDrawIdx, dlcNameHash, alts))
            {
				for (s32 i = 0; i < alts.GetCount(); ++i)
				{
					Assert(alts[i].altComp != 0xff && alts[i].altIndex != 0xff && alts[i].alt != 0xff);

					u32 globalAltDrawIdx = pVarInfo->GetGlobalDrawableIndex(alts[i].altIndex, (ePedVarComp)alts[i].altComp, alts[i].m_dlcNameHash);

					// the current asset had an alternate, check if we currently have this alternate and if we do, replace it with the original
					if (pedVarData.GetPedCompIdx((ePedVarComp)alts[i].altComp) == globalAltDrawIdx && pedVarData.GetPedCompAltIdx((ePedVarComp)alts[i].altComp) == alts[i].alt)
					{
						u8 texIdx = pedVarData.GetPedTexIdx((ePedVarComp)alts[i].altComp);
						u8 paletteIdx = pedVarData.GetPedPaletteIdx((ePedVarComp)alts[i].altComp);

						pedVarData.SetPedVariation((ePedVarComp)alts[i].altComp, globalAltDrawIdx, 0, texIdx, paletteIdx, NULL);
						requestAssets = true;
					}
				}
			}

			localDrawIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)slotId, drawblId);
			dlcNameHash = pVarInfo->GetDlcNameHashFromDrawableIdx((ePedVarComp)slotId, drawblId);
            
            // check if the variation we set now needs to change a different slot
            if (altPedVar->GetAlternates((u8)slotId, (u8)localDrawIdx, dlcNameHash, alts))
            {
				for (s32 i = 0; i < alts.GetCount(); ++i)
				{
					Assert(alts[i].altComp != 0xff && alts[i].altIndex != 0xff && alts[i].alt != 0xff);

					if (Verifyf(alts[i].alt != 0, "An alternate variation metadata entry specifies a 0 alternate! (%s)", pModelInfo->GetModelName()))
					{
						u32 compIdx = pedVarData.GetPedCompIdx((ePedVarComp)alts[i].altComp);
						u32 globalAltDrawIdx = pVarInfo->GetGlobalDrawableIndex(alts[i].altIndex, (ePedVarComp)alts[i].altComp, alts[i].m_dlcNameHash);

						if (compIdx == globalAltDrawIdx)
						{
							u8 texIdx = pedVarData.GetPedTexIdx((ePedVarComp)alts[i].altComp);
							u8 paletteIdx = pedVarData.GetPedPaletteIdx((ePedVarComp)alts[i].altComp);

							SetVariation(pDynamicEntity, pedPropData, pedVarData, (ePedVarComp)alts[i].altComp, globalAltDrawIdx, alts[i].alt, texIdx, paletteIdx, streamFlags, force);
						}
					}
				}
            }

			// check if any assets currently on the ped require the variation we want to set now to use an alternate drawable
			for (s32 i = 0; i < PV_MAX_COMP; ++i)
			{
				if (i == slotId)
					continue;

				u32 globalIdx = pedVarData.GetPedCompIdx((ePedVarComp)i);
				u32 localIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)i, globalIdx);
				u32 dlcHash = pVarInfo->GetDlcNameHashFromDrawableIdx((ePedVarComp)i, globalIdx);

				if (altPedVar->GetAlternates((u8)i, (u8)localIdx, dlcHash, alts))
				{
					for (s32 f = 0; f < alts.GetCount(); ++f)
					{
						Assert(alts[f].altComp != 0xff && alts[f].altIndex != 0xff && alts[f].alt != 0xff);

						u32 globalAltDrawIdx = pVarInfo->GetGlobalDrawableIndex(alts[f].altIndex, (ePedVarComp)alts[f].altComp, alts[f].m_dlcNameHash);

						if (alts[f].altComp == slotId && globalAltDrawIdx == drawblId)
						{
							Assert(alts[f].alt != 0);
							drawablAltId = alts[f].alt;        

							Assertf(drawablAltId <= pDrawblData->GetNumAlternatives(), " drawablAltId(%d) > (%d)NumAlternatives || Invalid variation: %s : %s : (drawable %d, texture %d) [%s_%03d_%c]", 
								drawablAltId, 
								pDrawblData->GetNumAlternatives(), 
								pModelInfo->GetModelName(), 
								varSlotNames[slotId],
								drawblId, 
								texId,
								varSlotNames[slotId],
								drawblId,
								'a' + texId);
							break;
						}
					}
				}
			}

			// Check props to ensure we got the right alternate.
			for(int i=0; i<pedPropData.GetNumActiveProps() && drawablAltId == 0; ++i)
			{
				CPedPropData::SinglePropData& sPropData = pedPropData.GetPedPropData(i);
				if(sPropData.m_anchorID != ANCHOR_NONE)
					drawablAltId = CPedPropsMgr::GetPedCompAltForProp(pPed, sPropData.m_anchorID, sPropData.m_propModelID, localDrawIdx, dlcNameHash, (u8)slotId);
			}
        }
    }

	// for blended peds force the setting if the component uses a blended drawable, we could be setting the same item
	// as we had previously and it'd think nothing has changed, ignoring the request
	if (pPed->HasHeadBlend())
	{
		CPedHeadBlendData* blendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
		if (MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle))
		{
			if (slotId == PV_COMP_FEET)
			{
				s32 feetDrawable = -1;
				MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_FEET, true, false, feetDrawable);
				force = feetDrawable != -1;
			}
			else if (slotId == PV_COMP_UPPR)
			{
				s32 upprDrawable = -1;
				MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_UPPR, true, false, upprDrawable);
				force = upprDrawable != -1;
			}
			else if (slotId == PV_COMP_LOWR)
			{
				s32 lowrDrawable = -1;
				MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_LOWR, true, false, lowrDrawable);
				force = lowrDrawable != -1;
			}
		}
	}

	if (force || !pedVarData.HasFirstVariationBeenSet() || !pedVarData.IsThisSettingSameAs(slotId, drawblId, drawablAltId, texId, paletteId, NULL, -1))
	{
		pedVarData.SetPedVariation(slotId, drawblId, drawablAltId, texId, paletteId, NULL );
		requestAssets = true;
		if (pDrawblData)
			pDrawblData->IncrementUseCount(texId);

#if !__FINAL
		if (PARAM_rendergfxdebug.Get())
		{
			sysStack::PrintStackTrace();
		}
#endif
	}

	if(pPed && pPed->GetPedAudioEntity())
	{
		pPed->GetPedAudioEntity()->GetFootStepAudio().UpdateVariationData(slotId);
	}

	if (requestAssets)
		RequestStreamPedFiles(pPed, &pedVarData, streamFlags);

	pPed->GetPedDrawHandler().UpdateCachedWetnessFlags(pPed);

	return(true);
}

void CPedVariationStream::FirstPersonPropData::Cleanup()
{
	if(pObject)
	{
		CObjectPopulation::DestroyObject(pObject);
		pObject = NULL;
	}
	if(pModelInfo)
	{
		pModelInfo = NULL;
		modelId.Invalidate();
		propModel.Clear();
	}
	renderThisFrame = false;
	inEffectActive = false;
	tcModTriggered = false;
	animTimer = 0U;
	animT = 1.0f;
	triggerFXOutro = false;
	hasConsumedTriggerFXOutro = false;
}

void CPedVariationStream::UpdateFirstPersonObjectData()
{
#if FPS_MODE_SUPPORTED
	// Reset current prop data
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		if(ms_firstPersonProp[i].animState == FirstPersonPropData::STATE_Idle)
		{
			ms_firstPersonProp[i].pPrevData = ms_firstPersonProp[i].pData;
		}
		ms_firstPersonProp[i].pData = NULL;
		ms_firstPersonProp[i].pTC = NULL;
		ms_firstPersonProp[i].triggerFXOutro = false; 
	}

	// Grab player ped
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!pPed)
	{
		return;
	}
	CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
	if(!pModelInfo)
	{
		return;
	}

	bool bTriggerFxOutroOnHelmetProp = pPed->GetPedResetFlag(CPED_RESET_FLAG_PutDownHelmetFX);

	// Get the list of props / accessories from the metadata for this ped
	FirstPersonPedData* pPedData = NULL;
	for(u32 i=0; i<ms_firstPersonData.m_peds.size(); ++i)
	{
		if(ms_firstPersonData.m_peds[i].m_name == pModelInfo->GetModelNameHash())
		{
			pPedData = &ms_firstPersonData.m_peds[i];
			break;
		}
	}
	if(!pPedData)
	{
		return;
	}
	CPedVariationInfoCollection* pVarInfoColl = pModelInfo->GetVarInfo();
	if(!pVarInfoColl)
	{
		return;
	}
	atArray<FirstPersonAssetData>& assetData = pPedData->m_objects;

	// Go through all anchor points we care about and see what ped props are there
	static const eAnchorPoints anchorPoints[] = {ANCHOR_HEAD, ANCHOR_EYES, ANCHOR_EARS, ANCHOR_MOUTH};
	const CPedPropData& pedPropData = pPed->GetPedDrawHandler().GetPropData();
	size_t propSlot = 0;
	for(u32 i=0; i<MAX_PROPS_PER_PED; ++i)
	{
		// Is this prop anchored to a point we care about?
		const CPedPropData::SinglePropData& propData = pedPropData.GetPedPropData(i);
		bool isAnchoredSomewhereInteresting = false;
		for(u32 j=0; j<NELEM(anchorPoints); ++j)
		{
			if(propData.m_anchorID == anchorPoints[j])
			{
				isAnchoredSomewhereInteresting = true;
				break;
			}
		}
		if(!isAnchoredSomewhereInteresting)
		{
			// Next prop...
			continue;
		}

		// Is there a first person model for this prop?
		for(int j=0; j<assetData.GetCount(); ++j)
		{
			FirstPersonAssetData& asset = assetData[j];
			if(asset.m_eAnchorPoint == propData.m_anchorID)
			{
				int globalIndex = pVarInfoColl->GetGlobalPropIndex(asset.m_localIndex,static_cast<eAnchorPoints>(asset.m_eAnchorPoint),asset.m_dlcHash);
				if(propData.m_propModelID == globalIndex)
				{
					FirstPersonPropData& fpData = ms_firstPersonProp[propSlot];
					fpData.pData = &assetData[j];
					fpData.anchorOverride = propData.m_anchorOverrideID;

					if(bTriggerFxOutroOnHelmetProp && (propData.m_propModelID == CPedPropsMgr::GetPedPropIdx(pPed, ANCHOR_HEAD) ))
					{
						fpData.triggerFXOutro = true;
					}

					// Cache TC mod if there is one
					for(u32 k=0; k<fpData.pData->m_timeCycleMods.size(); ++k)
					{
						if(fpData.pData->m_timeCycleMods[k].m_textureId == propData.m_propTexID)
						{
							fpData.pTC = &fpData.pData->m_timeCycleMods[k];
							break;
						}
					}

					// Don't bother searching more assets
					++propSlot;
					if(propSlot >= NELEM(ms_firstPersonProp))
					{
						return;
					}
					break;
				}
			}
		}
	}
	// Go through all ped component slots and see what drawable is there
	for(size_t i=0; i<PV_MAX_COMP; ++i)
	{
		u32 drawblIdx = pPed->GetPedDrawHandler().GetVarData().m_aDrawblId[i];
		for(u32 j=0; j<assetData.size(); ++j)
		{
			FirstPersonAssetData& asset = assetData[j];			
			if(asset.m_eCompType == static_cast<ePedVarComp>(i))
			{
				if(pVarInfoColl->GetGlobalDrawableIndex(asset.m_localIndex,static_cast<ePedVarComp>(asset.m_eCompType),asset.m_dlcHash) == drawblIdx)
				{
					FirstPersonPropData& fpData = ms_firstPersonProp[propSlot];
					fpData.pData = &assetData[j];

					// Cache TC mod if there is one
					for(u32 k=0; k<fpData.pData->m_timeCycleMods.size(); ++k)
					{
						if(fpData.pData->m_timeCycleMods[k].m_textureId == pPed->GetPedDrawHandler().GetVarData().m_aTexId[i])
						{
							fpData.pTC = &fpData.pData->m_timeCycleMods[k];
							break;
						}
					}

					// Don't bother searching more assets
					++propSlot;
					if(propSlot >= NELEM(ms_firstPersonProp))
					{
						return;
					}
					break;
				}
			}
		}
	}
#endif // FPS_MODE_SUPPORTED
}

size_t CPedVariationStream::GetHighestWeightedTCModFirstPersonSlotIdx()
{
	// Find highest weighted TC mod
	size_t maxWeightIdx = NELEM(ms_firstPersonProp);
	float maxWeight = 0.0f;
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		const bool bNeedsToWaitOnAnimPostFXEvent = (ms_firstPersonProp[i].pData && ms_firstPersonProp[i].pData->m_takeOffAnim.m_duration > 0.0f);
		const bool bCanApplyTCMod = (bNeedsToWaitOnAnimPostFXEvent == false || ms_animPostFXEventTriggered);

		if(ms_firstPersonProp[i].pData && bCanApplyTCMod && ms_firstPersonProp[i].pTC && ms_firstPersonProp[i].pTC->m_intensity > maxWeight)
		{
			// If this looks like a helmet (Attached to head), don't apply TC mod if helmet isn't fully equipped
			if(ms_firstPersonProp[i].pData->m_eAnchorPoint == ANCHOR_HEAD)
			{
				CPed *pPed = CGameWorld::FindLocalPlayer();
				if (pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS))
				{
					continue;
				}
			}

			maxWeight = ms_firstPersonProp[i].pTC->m_intensity;
			maxWeightIdx = i;
		}
	}
	return maxWeightIdx;
}

bool CPedVariationStream::HasFirstPersonTimeCycleModifier()
{
	// Find highest weighted TC mod / see if any anims in progress
	size_t maxWeightIdx = GetHighestWeightedTCModFirstPersonSlotIdx();

	// No prop data, or this prop has no TC effect?
	if(maxWeightIdx >= NELEM(ms_firstPersonProp))
	{
		return false;
	}

	// Mobile phone cam?
	if(CPhoneMgr::CamGetState())
	{
		return false;
	}

	// First person cam?
	if(!camInterface::IsDominantRenderedCameraAnyFirstPersonCamera())
	{
		return false;
	}

	return true;
}

atHashString CPedVariationStream::GetFirstPersonTimeCycleModifierName()
{
	// Find highest weighted TC mod
	size_t maxWeightIdx = GetHighestWeightedTCModFirstPersonSlotIdx();
	Assertf(maxWeightIdx < NELEM(ms_firstPersonProp), "Not got first person TC mod!");

	FirstPersonPropData& propData = ms_firstPersonProp[maxWeightIdx];

	return propData.pTC->m_timeCycleMod;
}

float CPedVariationStream::GetFirstPersonTimeCycleModifierIntensity()
{
	// Find highest weighted TC mod
	size_t maxWeightIdx = GetHighestWeightedTCModFirstPersonSlotIdx();
	Assertf(maxWeightIdx < NELEM(ms_firstPersonProp), "Not got first person TC mod!");

	return ms_firstPersonProp[maxWeightIdx].pTC->m_intensity*ms_firstPersonProp[maxWeightIdx].animT;
}

void CPedVariationStream::PreScriptUpdateFirstPersonProp()
{
	// Reset first person camera flag before script update
	camInterface::SetScriptedCameraIsFirstPersonThisFrame(false);
}

void CPedVariationStream::UpdateFirstPersonProp()
{
#if FPS_MODE_SUPPORTED
	// Wearing a pedprop or have a ped component we care about?
	UpdateFirstPersonObjectData();

	// Update FPV flags
	CPed* pPed = CGameWorld::FindLocalPlayer();
	bool isFps = pPed->IsFirstPersonCameraOrFirstPersonSniper();
	pPed->UpdateFirstPersonFlags(isFps);

	// Update props
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		UpdateFirstPersonPropSlot(ms_firstPersonProp[i] BANK_ONLY(, i));
	}
#endif // FPS_MODE_SUPPORTED
}

void CPedVariationStream::UpdateFirstPersonPropSlot(FirstPersonPropData& propData BANK_ONLY(, size_t idx))
{
#if FPS_MODE_SUPPORTED

	bool isFirstPersonCam = camInterface::IsDominantRenderedCameraAnyFirstPersonCamera() && !CPhoneMgr::CamGetState();

	// Default to not render; updated at the end of the function
	propData.renderThisFrame = false;

	// If the prop is on-screen, and it's anchor is overridden, it's removed from the head. Act as if it was removed from the head if
	// there's a take off animation
	if(propData.anchorOverride != ANCHOR_NONE && propData.pData && propData.pData->m_takeOffAnim.m_duration > 0.0f)
	{
		// Also don't do anything if we're putting the helmet on (Needed for some vehicles)
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer && !pPlayer->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
		{
			// Act as if the prop was removed
			propData.pData = NULL;
		}
	}

	const bool bPropChanged = propData.pData != propData.pPrevData;
	const bool bNoProp = !propData.pData && !propData.pPrevData;
	if(bPropChanged || bNoProp)
	{
		propData.animT = 1.0f;
		propData.animTimer = 0U;
		propData.tcModTriggered = false;
	}

	if (isFirstPersonCam)
	{
		const bool bAnyData = (propData.pData || propData.pPrevData);
		if (bAnyData)
		{
			const float fadeOffLength = (propData.pData ? propData.pData->m_tcModFadeOffSecs : propData.pPrevData->m_tcModFadeOffSecs);
			eFPTCInterpType interpType = (propData.pData ? propData.pData->m_tcModInterpType : propData.pPrevData->m_tcModInterpType);

			if (propData.tcModTriggered == false)
			{
				propData.tcModTriggered = true;
				propData.animTimer = fwTimer::GetTimeInMilliseconds();
				propData.animT = 1.0f;
			}
			else
			{
				float animTimer = (float)(fwTimer::GetTimeInMilliseconds()-propData.animTimer) * 0.001f;
				float t = fadeOffLength > 0.0f ? (animTimer/fadeOffLength) : 0.0f;

				if (interpType == FPTC_POW2)
				{
					t = t*t;
				}
				else if (interpType == FPTC_POW4)
				{
					t = t*t*t*t;
				}

				propData.animT = 1.0f-Saturate(t);
			}
		}
		else
		{
			propData.animT = 1.0f;
			propData.animTimer = 0U;
			propData.tcModTriggered = false;	
		}
	}
	else
	{
		propData.animT = 1.0f;
		propData.animTimer = 0U;
		propData.tcModTriggered = false;	
	}

	bool bInEffectJustTriggered = false;

	// New prop going on, and we have a "put on" animation?
	if(propData.pData && !propData.pPrevData)
	{
		if(propData.pData->m_putOnAnim.m_duration > 0.0f)
		{
			// Only trigger if in 1st person
			if (isFirstPersonCam)
			{
				ms_animPostFXEventTriggered = false;
				const atHashString blinkFXIntroName = propData.pData->m_putOnAnim.m_timeCycleMod;
				ANIMPOSTFXMGR.Start(blinkFXIntroName, 0U, false, false, false, 0U, AnimPostFXManager::kFirstPersonProp);
			}

			// Otherwise keep track of the fx needing to trigger if player switches to 1st person
			propData.inEffectActive = true;
			bInEffectJustTriggered = true;
			propData.hasConsumedTriggerFXOutro = false;
		}
		else
		{
			// Flag the intro anim as triggered
			ms_animPostFXEventTriggered = true;
		}
	}
	// Prop taken off and we have a "take off" animation?
	else if( (!propData.pData && propData.pPrevData) || (propData.triggerFXOutro && propData.pPrevData) )
	{
		if (isFirstPersonCam && !propData.hasConsumedTriggerFXOutro)
		{
			if (propData.pPrevData && propData.pPrevData->m_takeOffAnim.m_duration > 0.0f)
			{
				const atHashString blinkFXOutroName = propData.pPrevData->m_takeOffAnim.m_timeCycleMod;
				ANIMPOSTFXMGR.Start(blinkFXOutroName, 0U, false, false, false, 0U, AnimPostFXManager::kFirstPersonProp);
			}
			propData.inEffectActive = false;
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (!pPlayer || !pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
				propData.hasConsumedTriggerFXOutro = true;
		}
	}
	// Kill effects if not in 1st person
	if (!isFirstPersonCam)
	{
		if (propData.pData && propData.pData->m_putOnAnim.m_duration > 0.0f)
		{
			const atHashString blinkFXIntroName = propData.pData->m_putOnAnim.m_timeCycleMod;
			ANIMPOSTFXMGR.Stop(blinkFXIntroName, AnimPostFXManager::kFirstPersonProp);
		}
		if (propData.pData && propData.pData->m_takeOffAnim.m_duration > 0.0f)
		{
			const atHashString blinkFXOutroName = propData.pData->m_takeOffAnim.m_timeCycleMod;
			ANIMPOSTFXMGR.Stop(blinkFXOutroName, AnimPostFXManager::kFirstPersonProp);
		}
	}

	// If we're now in first person camera but skipped triggering the fading fx (ie. player was in 3rd person view),
	// act as if the intro blink effect has already triggered
	bool bNeedsHelmetOnEffect = ((propData.pData && bInEffectJustTriggered == false && propData.hasConsumedTriggerFXOutro == false) &&	(ANIMPOSTFXMGR.IsRunning(propData.pData->m_putOnAnim.m_timeCycleMod) == false));
	if (isFirstPersonCam && propData.inEffectActive && bNeedsHelmetOnEffect)
	{
		ms_animPostFXEventTriggered = true;
	}

	// No prop?
	if(!propData.pData)
	{
		propData.Cleanup();
		return;
	}

	// Prop changed?
	if(propData.pObject && propData.propModel != propData.pData->m_fpsPropModel)
	{
		propData.Cleanup();
	}

	// Need to create prop?
	if(!propData.pObject
#if SUPPORT_MULTI_MONITOR
		&& !GRCDEVICE.GetMonitorConfig().isMultihead()
#endif
		)
	{
		// Request model every frame until it's available
		fwModelId modelId = CModelInfo::GetModelIdFromName(propData.pData->m_fpsPropModel);
		if(!modelId.IsValid())
		{
			return;
		}
		propData.modelId = modelId;
		propData.propModel = propData.pData->m_fpsPropModel;

		propData.pModelInfo = CModelInfo::GetBaseModelInfo(propData.modelId);
		if(!propData.pModelInfo->GetHasLoaded())
		{
			CModelInfo::RequestAssets(propData.pModelInfo, STRFLAG_PRIORITY_LOAD|STRFLAG_FORCE_LOAD);
		}

		// Are we in first person?
		if(!isFirstPersonCam)
		{
			// Not in first person, bail
			return;
		}

		// Finished loading?
		if(propData.pModelInfo && propData.pModelInfo->GetHasLoaded())
		{
			CObject* pObject = CObjectPopulation::CreateObject(propData.modelId, ENTITY_OWNEDBY_GAME, true, false, false);
			if(!pObject)
			{
				return;
			}
			pObject->SetExtremelyCloseToCamera(true);
			pObject->AssignProtectedBaseFlag(fwEntity::HAS_FPV, true);
			pObject->CreateDrawable();
			pObject->SetRenderPhaseVisibilityMask(VIS_PHASE_MASK_GBUF);
			pObject->DisableCollision(NULL, true);
			pObject->ClearBaseFlag(fwEntity::USE_SCREENDOOR);
			pObject->SetBaseFlag(fwEntity::HAS_OPAQUE);
			pObject->SetBaseFlag(fwEntity::HAS_ALPHA);

			#if __BANK
				CPedPropsMgr::GetFirstPersonEyewearOffset(idx) = propData.pData->m_fpsPropModelOffset;
			#endif

			propData.pObject = pObject;

			REPLAY_ONLY(CReplayMgr::RecordObject(pObject));
		}
	}

	// Ensure the helmet is actually on the peds head before enabling the overlay
	bool bPuttingOnOrTakingOffHelmet = false;
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		bPuttingOnOrTakingOffHelmet = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS);
	}

	if(propData.pObject && isFirstPersonCam && !bPuttingOnOrTakingOffHelmet)
	{
		// Add the object straight into the render list group, bypassing all the portal tracking, etc
		propData.renderThisFrame = true;
	}
#endif // FPS_MODE_SUPPORTED
}

// This is called after the render lists are sorted, so we can be sure that these entities are pushed
// at the end of the lists.
void CPedVariationStream::AddFirstPersonEntitiesToRenderList()
{
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		Assert(!ms_firstPersonProp[i].renderThisFrame || ms_firstPersonProp[i].pObject);
		if(ms_firstPersonProp[i].renderThisFrame && ms_firstPersonProp[i].pObject)
		{
			CObject* pEntity = ms_firstPersonProp[i].pObject;
			gRenderListGroup.GetRenderListForPhase(CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex()).AddEntity(pEntity,
				pEntity->GetBaseFlags(), 0, 0, 0.0f, RPASS_ALPHA, 0);
			gRenderListGroup.GetRenderListForPhase(CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex()).AddEntity(pEntity,
				pEntity->GetBaseFlags(), 0, 0, 0.0f, RPASS_VISIBLE, 0);
		}
	}
}

void CPedVariationStream::UpdateFirstPersonPropFromCamera()
{
#if FPS_MODE_SUPPORTED
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		// No op if no prop
		if(!ms_firstPersonProp[i].pObject || !ms_firstPersonProp[i].pData)
		{
			continue;
		}

		#if __BANK
			ms_firstPersonProp[i].pData->m_fpsPropModelOffset = CPedPropsMgr::GetFirstPersonEyewearOffset(i);
		#endif

		const Vector3& offset = ms_firstPersonProp[i].pData->m_fpsPropModelOffset;
		Matrix34 mtx = camInterface::GetMat();
		mtx.a = -mtx.a; // Rotate 180 degrees about Z
		mtx.b = -mtx.b; //
		mtx.d = offset.x * camInterface::GetRight() + offset.y * camInterface::GetFront() + offset.z * camInterface::GetUp();
		ms_firstPersonProp[i].pObject->SetMatrix(mtx);
	}
#endif // FPS_MODE_SUPPORTED
}

float CPedVariationStream::GetFirstPersonPropEnvironmentSE()
{
	float maxValue = -1.0f;
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		if(ms_firstPersonProp[i].pData)
		{
			float val = ms_firstPersonProp[i].pData->m_seEnvironment;
			if(val > maxValue)
			{
				maxValue = val;
			}
		}
	}
	return maxValue;
}

float CPedVariationStream::GetFirstPersonPropWeatherSE()
{
	float maxValue = -1.0f;
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		if(ms_firstPersonProp[i].pData)
		{
			float val = ms_firstPersonProp[i].pData->m_seWeather;
			if(val > maxValue)
			{
				maxValue = val;
			}
		}
	}
	return maxValue;
}

#if __BANK
void CPedVariationStream::ReloadFirstPersonMeta()
{
	// Reset all first person data
	for(size_t i=0; i<NELEM(ms_firstPersonProp); ++i)
	{
		ms_firstPersonProp[i].pPrevData = NULL;
		ms_firstPersonProp[i].pData = NULL;
		ms_firstPersonProp[i].pTC = NULL;
		ms_firstPersonProp[i].modelId.Invalidate();
		ms_firstPersonProp[i].propModel.Clear();
		ms_firstPersonProp[i].pObject = NULL;
		ms_firstPersonProp[i].pModelInfo = NULL;
		ms_firstPersonProp[i].animState = FirstPersonPropData::STATE_Idle;
		ms_firstPersonProp[i].renderThisFrame = false;
	}
	ms_firstPersonData.m_peds.clear();
	LoadFirstPersonData();
}
#endif

void CPedVariationStream::OnAnimPostFXEvent(AnimPostFXEvent* pEvent)
{
	if (pEvent->eventType == ANIMPOSTFX_EVENT_ON_FRAME)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		bool bSwitchingVisor = pPlayer && pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor);

		if (pEvent->userTag == PED_FP_ANIMPOSTFX_EVENT_TAG_INTRO || bSwitchingVisor)
			ms_animPostFXEventTriggered = true;
		else if (pEvent->userTag == PED_FP_ANIMPOSTFX_EVENT_TAG_OUTRO)
			ms_animPostFXEventTriggered = false;
	}
}

bool CPedVariationStream::RequestHdAssets(CPed* ped)
{
	CPedModelInfo* pPedModelInfo = ped->GetPedModelInfo();
	Assert(pPedModelInfo);
	if (!pPedModelInfo->GetIsStreamedGfx())
		return false;

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::PED_HD);

	u32 drawblIdx = 0;
	u8 drawblAltIdx = 0;
#if 0
	u8 texIdx = 0;
#endif
	char assetName[STORE_NAME_LENGTH];

	// generate the name of the drawable to extract
	CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();
	Assert(pVarInfo);
	if (!pVarInfo)
		return false;

	// don't request hd assets if we have a regular asset req in progress
	CPedStreamRequestGfx* reqGfx = ped->GetExtensionList().GetExtension<CPedStreamRequestGfx>();
	if (reqGfx)
		return false;

	if (ped->GetExtensionList().GetExtension<CPedHeadBlendData>()) // we don't do hd assets on blended peds
		return false;

	CPedStreamRequestGfx* pNewStreamGfx = rage_new CPedStreamRequestGfx;
	Assert(pNewStreamGfx);
	pNewStreamGfx->SetTargetEntity(ped);
	pNewStreamGfx->SetHasHdAssets(true);

	const char*	pedFolder = pPedModelInfo->GetStreamFolder();

	for (u32 i = 0; i < PV_MAX_COMP; ++i)
	{
		// generate the file names of the files we require
		drawblIdx = ped->GetPedDrawHandler().GetVarData().m_aDrawblId[i];		// current drawable for each component
		drawblAltIdx = ped->GetPedDrawHandler().GetVarData().m_aDrawblAltId[i];	// current variation of drawable
#if 0
		texIdx = ped->GetPedDrawHandler().GetVarData().m_aTexId[i];
#endif

		if (drawblIdx == PV_NULL_DRAWBL)
		{
			Assert(i != PV_COMP_HEAD);
			continue;
		}

		CPVDrawblData* pDrawblData = pVarInfo->GetCollectionDrawable(i, drawblIdx);
        if (!pDrawblData)
        {
			Assert(i != PV_COMP_HEAD);
            continue;
        }

#if 0 // txd swap is handled by texLod now
		if (!Verifyf(texIdx < pDrawblData->GetNumTex(), "Selected texture '%c' doesn't exists for '%s_%03d' component on ped '%s'. Is it present in meta data?", 'a' + texIdx, varSlotNames[i], drawblIdx, ped->GetModelName()))
			continue;

		u8 raceIdx = pDrawblData->GetTexData(texIdx);
		if (!Verifyf(raceIdx != (u8)PV_RACE_INVALID, "Bad race index for drawable %d with texIdx %d on ped %s", drawblIdx, texIdx, ped->GetModelName()))
			continue;

		// texture
		sprintf(assetName, "%s/%s_diff_%03d_%c_%s+hi", pedFolder, varSlotNames[i], drawblIdx, (texIdx+'a'), raceTypeNames[raceIdx]);	// diffuse map
		if (g_TxdStore.FindSlot(assetName) != -1)
			pNewStreamGfx->AddTxd(i, assetName, 0, pPedModelInfo);
#endif

		// drawable
		if (pDrawblData->IsMatchingPrev() == true)
			sprintf(assetName, "%s/%s_%03d_m_hi", pedFolder, varSlotNames[i], drawblIdx);
		else if (pDrawblData->IsUsingRaceTex() == true)
			sprintf(assetName, "%s/%s_%03d_r_hi", pedFolder, varSlotNames[i], drawblIdx);
		else
			sprintf(assetName, "%s/%s_%03d_u_hi", pedFolder, varSlotNames[i], drawblIdx);

		if (g_DwdStore.FindSlot(assetName) != -1)
			pNewStreamGfx->AddDwd(i, assetName, 0, pPedModelInfo);
	}

	pNewStreamGfx->PushRequest();
	return true;
}

void CPedVariationStream::ProcessStreamRequests(){

	CPedStreamRequestGfx::Pool* pStreamReqPool = CPedStreamRequestGfx::GetPool();

	if (pStreamReqPool->GetNoOfUsedSpaces() == 0){
		return;												// early out if no requests outstanding
	}

	u32 poolSize=pStreamReqPool->GetSize();

	for(u32 i=0; i<poolSize; i++)
	{
		CPedStreamRequestGfx* streamReq = pStreamReqPool->GetSlot(i);
		if(streamReq)
		{
			ProcessSingleStreamRequest(streamReq);
		}
	}
}

void CPedVariationStream::ProcessSingleStreamRequest(CPedStreamRequestGfx* pStreamReq)
{
	if (pStreamReq && pStreamReq->HaveAllLoaded() && pStreamReq->IsSyncGfxLoaded())
	{
		CPed* ped = pStreamReq->GetTargetEntity();
		CPedVariationData* pNewVarData = &(ped->GetPedDrawHandler().GetVarData());
		bool hasHdAssets = pStreamReq->HasHdAssets();

		if (pStreamReq->WasLoading())
		{
			if (!hasHdAssets)
			{
				const bool blendStarted = pNewVarData->AssetLoadFinished(ped, pStreamReq);

				// If we actually started a blend, we need to delay the rest of
				// the processing till next frame.  When there is no blend, we
				// can finish the rest of the processing now.
				if (blendStarted)
				{
					pStreamReq->SetBlendingStarted();

					// FA: This doesn't seem to be needed anymore. The texture seems to be rendered/compressed the same frame
					// and we don't need to delay this, like we did on last gen. I wonder if it's random, that would be really
					// nasty to deal with.
#if 0
					return;
#endif
				}
			}
		}

        // for blended peds only defer the creation of the renderGfx until the blend slot is streamed in.
        // that way we'll have all the assets ready when creating the gfx and we can continue setting up
        // the drawables and texture we'll be using to render with
		if (pStreamReq->WasBlending())
		{
			CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
            if (blendData && MESHBLENDMANAGER.IsWaitingForAssets(blendData->m_blendHandle))
                return;
		}

		{
			CPedPropRequestGfx* propGfx = pStreamReq->GetWaitForGfx();

			// apply the files we requested and which have now all loaded to the target ped
			if (hasHdAssets)
				ApplyHdAssets(ped, pStreamReq);
			else
				ApplyStreamPedFiles(ped, pNewVarData, pStreamReq);

			pNewVarData->UpdateAllHeadBlends(ped);

			CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
			if (blendData && blendData->m_needsFinalizing)
			{
                pNewVarData->FinalizeHeadBlend(ped);
                blendData->m_needsFinalizing = false;
			}

			pStreamReq->ClearWaitForGfx();
			ped->GetExtensionList().Destroy(pStreamReq);		// will remove from pool & from extension list

			CPedPropsMgr::ProcessSingleStreamRequest(propGfx);
		}
	}
}


// Return the specified component for the current ped
rmcDrawable* CPedVariationStream::GetComponent(ePedVarComp eComponent, CEntity* pEntity, CPedVariationData& UNUSED_PARAM(pedVarData))
{
	Assert(pEntity);

	rmcDrawable* pComponent = NULL;

	//if(isPlayer)
	{
		CPed* pPlayerPed = static_cast<CPed *>(pEntity);
		Assert(pPlayerPed);
		const CPedStreamRenderGfx* pGfxData = pPlayerPed->GetPedDrawHandler().GetConstPedRenderGfx();
	//	Assert(pPlayerPed->GetPedRenderGfx());
		if (pPlayerPed && pGfxData){
			pComponent = pGfxData->GetDrawable(eComponent);
			return pComponent;
		}
	}

	return(NULL);
}

// ----- CPedStreamRequestGfx

CPedStreamRequestGfx::CPedStreamRequestGfx(REPLAY_ONLY(bool isDummy)) 
{ 
	m_pTargetEntity=NULL; 
	m_state=PS_INIT; 
	m_bDelayCompletion = false;
	m_bHdAssets = false;
    m_blendHandle = MESHBLEND_HANDLE_INVALID;

	m_waitForGfx = NULL;

	m_reqTxds.UseAsArray();
	m_reqDwds.UseAsArray();
	m_reqClds.UseAsArray();
	m_reqHeadBlend.UseAsArray();
	m_reqFpAlts.UseAsArray();

    m_dontDeleteMaskBlend = 0;

    m_dontDeleteMaskTxd = 0;
	m_dontDeleteMaskDwd = 0;
	m_dontDeleteMaskCld = 0;
	m_dontDeleteMaskFpAlt = 0;

	m_hasHighTxdForComponentTxd = 0;
	m_hasHighTxdForComponentDwd = 0;

#if GTA_REPLAY
	m_isDummy = isDummy;
	m_reqHeadBlendNames.Resize(HBS_MAX);
#endif // GTA_REPLAY
}

CPedStreamRequestGfx::~CPedStreamRequestGfx() {

	Release();
	Assert(m_state == PS_FREED);
}

bool CPedStreamRequestGfx::AddTxd(u32 componentSlot, const strStreamingObjectName txdName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo)) {

#if GTA_REPLAY
	m_reqTxdsNames.Append() = txdName;
	if(m_isDummy) return true;
#endif // GTA_REPLAY

	s32 txdSlotId = g_TxdStore.FindSlot(txdName).Get();

	if (txdSlotId != -1){
        if ((streamFlags & STRFLAG_DONTDELETE) == 0)
        {
            Assert(componentSlot < 16);
            streamFlags |= STRFLAG_DONTDELETE;
            m_dontDeleteMaskTxd |= (1 << componentSlot);
        }
		m_reqTxds.SetRequest(componentSlot, txdSlotId, g_TxdStore.GetStreamingModuleId(), streamFlags);
		return true;
	} else {
		pedAssertf(false,"Streamed .txd request for ped '%s' is not found : %s", pPedModelInfo->GetModelName(), txdName.TryGetCStr());
		return false;
	}
}

void CPedStreamRequestGfx::AddDwd(u32 componentSlot, const strStreamingObjectName dwdName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo)) {

#if GTA_REPLAY
	m_reqDwdsNames.Append() = dwdName;
	if(m_isDummy) return;
#endif // GTA_REPLAY

	strLocalIndex dwdSlotId = strLocalIndex(g_DwdStore.FindSlot(dwdName));

	if (dwdSlotId != -1){
        if ((streamFlags & STRFLAG_DONTDELETE) == 0)
        {
            Assert(componentSlot < 16);
            streamFlags |= STRFLAG_DONTDELETE;
            m_dontDeleteMaskDwd |= (1 << componentSlot);
        }

		OUTPUT_ONLY(g_DwdStore.SetNoAssignedTxd(dwdSlotId, true);)
		g_DwdStore.SetParentTxdForSlot(dwdSlotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));	
		m_reqDwds.SetRequest(componentSlot,dwdSlotId.Get(), g_DwdStore.GetStreamingModuleId(), streamFlags);
	}else {
		pedAssertf(false,"Streamed .dwd request for ped '%s' is not found : %s", pPedModelInfo->GetModelName(), dwdName.GetCStr());
	}
}

void CPedStreamRequestGfx::AddCld(u32 componentSlot, const strStreamingObjectName cldName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo)) {

#if GTA_REPLAY
	m_reqCldsNames.Append() = cldName;
	if(m_isDummy) return;
#endif // GTA_REPLAY

	s32 cldSlotId = g_ClothStore.FindSlot(cldName).Get();

	if (cldSlotId != -1)
	{
        if ((streamFlags & STRFLAG_DONTDELETE) == 0)
        {
            Assert(componentSlot < 16);
            streamFlags |= STRFLAG_DONTDELETE;
            m_dontDeleteMaskCld |= (1 << componentSlot);
        }
		m_reqClds.SetRequest(componentSlot,cldSlotId, g_ClothStore.GetStreamingModuleId(), streamFlags);
	}
	else 
	{
		pedAssertf(false,"Streamed .cloth request for ped '%s' is not found : %s", pPedModelInfo->GetModelName(), cldName.GetCStr());
	}
}

void CPedStreamRequestGfx::AddHeadBlendReq(u32 slot, const strStreamingObjectName assetName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo)) {
	if (!Verifyf(slot < HBS_MAX, "Bad slot id %d give to AddHeadBlendReq", slot))
	{
		return;
	}

#if GTA_REPLAY
	m_reqHeadBlendNames[slot] = assetName;
	if(m_isDummy) return;
#endif // GTA_REPLAY

	strLocalIndex slotId = (slot < HBS_HEAD_DRAWABLES) ? strLocalIndex(g_DwdStore.FindSlot(assetName)) : (slot < HBS_MICRO_MORPH_SLOTS ? strLocalIndex(g_TxdStore.FindSlot(assetName)) : strLocalIndex(g_DrawableStore.FindSlot(assetName)));

	if (slotId != -1)
	{
        if ((streamFlags & STRFLAG_DONTDELETE) == 0)
        {
            Assert(slot < 64);
            streamFlags |= STRFLAG_DONTDELETE;
            m_dontDeleteMaskBlend |= (u64)(1 << slot);
        }

		if (slot < HBS_HEAD_DRAWABLES)
			g_DwdStore.SetParentTxdForSlot(slotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));	
		m_reqHeadBlend.SetRequest(slot, slotId.Get(), (slot < HBS_HEAD_DRAWABLES) ? g_DwdStore.GetStreamingModuleId() : (slot < HBS_MICRO_MORPH_SLOTS ? g_TxdStore.GetStreamingModuleId() : g_DrawableStore.GetStreamingModuleId()), streamFlags | STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);
	}
	else 
	{
		pedAssertf(false, "Streamed asset request for ped '%s' is not found, slot %d: %s", pPedModelInfo->GetModelName(), slot, assetName.GetCStr());
	}
}

void CPedStreamRequestGfx::AddFpAlt(u32 slot, const strStreamingObjectName fpAltName, s32 flags, CPedModelInfo* ASSERT_ONLY(pmi) ASSERT_ONLY(, const char* assetName, const char* drawablName))
{
#if GTA_REPLAY
	m_reqFpAltsNames.Append() = fpAltName;
	if(m_isDummy) return;
#endif // GTA_REPLAY

	// NB: Can't just convert fpAltName to a char* string, since it's built from partial hashes, so GetCStr() returns null
	Assertf(fpAltName == atStringHash(drawablName), "First-person alt drawable hash not expected for '%s': %d == %d",
		drawablName, fpAltName.GetHash(), atStringHash(drawablName));

	strLocalIndex dwdSlotId = g_DwdStore.FindSlot(fpAltName);
	if(pedVerifyf(dwdSlotId != -1, "Streamed .dwd request for first person asset for ped '%s' is not found. Drawable: %s, Asset: %s", pmi->GetModelName(), drawablName, assetName))
	{
        if ((flags & STRFLAG_DONTDELETE) == 0)
        {
            Assert(slot < 16);
            flags |= STRFLAG_DONTDELETE;
            m_dontDeleteMaskFpAlt |= (1 << slot);
        }

		OUTPUT_ONLY(g_DwdStore.SetNoAssignedTxd(dwdSlotId, true);)
		g_DwdStore.SetParentTxdForSlot(dwdSlotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));	
		m_reqFpAlts.SetRequest(slot, dwdSlotId.Get(), g_DwdStore.GetStreamingModuleId(), flags);
	}
}

void CPedStreamRequestGfx::Release(void)
{
    for (s32 i = 0; i < HBS_MAX; ++i)
    {
        if ((m_dontDeleteMaskBlend & (u64)(1 << i)) != 0)
        {
            strLocalIndex idx = strLocalIndex(m_reqHeadBlend.GetRequestId(i));
            if (idx != -1)
            {
                if (i < HBS_HEAD_DRAWABLES)
                {
                    g_DwdStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
                }
                else if (i < HBS_MICRO_MORPH_SLOTS)
                {
                    g_TxdStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
                }
				else
				{
                    g_DrawableStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
				}
            }
        }
    }

    for (s32 i = 0; i < PV_MAX_COMP; ++i)
    {
        if ((m_dontDeleteMaskTxd & (1 << i)) != 0)
        {
            strLocalIndex idx = strLocalIndex(m_reqTxds.GetRequestId(i));
            if (idx != -1)
            {
                g_TxdStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
            }
        }

        if ((m_dontDeleteMaskDwd & (1 << i)) != 0)
        {
            strLocalIndex idx = strLocalIndex(m_reqDwds.GetRequestId(i));
            if (idx != -1)
            {
                g_DwdStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
            }
        }

        if ((m_dontDeleteMaskCld & (1 << i)) != 0)
        {
            strLocalIndex idx = strLocalIndex(m_reqClds.GetRequestId(i));
            if (idx != -1)
            {
                g_ClothStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
            }
        }

        if ((m_dontDeleteMaskFpAlt & (1 << i)) != 0)
        {
            strLocalIndex idx = strLocalIndex(m_reqFpAlts.GetRequestId(i));
            if (idx != -1)
            {
                g_DwdStore.ClearRequiredFlag(idx.Get(), STRFLAG_DONTDELETE);
            }
        }
    }

    m_dontDeleteMaskBlend = 0;

    m_dontDeleteMaskTxd = 0;
    m_dontDeleteMaskDwd = 0;
    m_dontDeleteMaskCld = 0;
    m_dontDeleteMaskFpAlt = 0;

	m_reqTxds.ReleaseAll();
	m_reqDwds.ReleaseAll();
	m_reqClds.ReleaseAll();
	m_reqHeadBlend.ReleaseAll();
	m_reqFpAlts.ReleaseAll();

    if (m_blendHandle != MESHBLEND_HANDLE_INVALID)
		MESHBLENDMANAGER.SetRequestsInProgress(m_blendHandle, false);

	m_state = PS_FREED;
}

// requests have completed - add refs to the slots we wanted and free up the initial requests
void CPedStreamRequestGfx::RequestsComplete() { 

	Assert(m_state == PS_REQ || m_state == PS_BLEND);

	CPed* pTarget = GetTargetEntity();
	CPedDrawHandler* pDrawHandler = &pTarget->GetPedDrawHandler();

	if (CPedStreamRenderGfx::GetPool()->GetNoOfFreeSpaces() <= 0)
	{
#if !__NO_OUTPUT
		Displayf("Ran out of pool entries for CPedStreamRenderGfx frame %d", fwTimer::GetFrameCount());
		for (s32 i = 0; i < CPedStreamRenderGfx::GetPool()->GetSize(); ++i)
		{
			CPedStreamRenderGfx* gfx = CPedStreamRenderGfx::GetPool()->GetSlot(i);
			if (gfx)
			{
				Displayf("CPedStreamRenderGfx %d / %d: %s (%p) (%s) (%d refs)\n", i, CPedStreamRenderGfx::GetPool()->GetSize(), gfx->m_targetEntity ? gfx->m_targetEntity->GetModelName() : "NULL", gfx->m_targetEntity.Get(), gfx->m_used ? "used" : "not used", gfx->m_refCount);
			}
			else
			{
				Displayf("CPedStreamRenderGfx %d / %d: null slot\n", i, CPedStreamRenderGfx::GetPool()->GetSize());
			}
		}
#endif // __NO_OUTPUT
		//Quitf(ERR_GEN_PED_VAR, "Ran out of pool entries for CPedStreamRenderGfx!");
	}
	else
	{
		pDrawHandler->SetPedRenderGfx(rage_new CPedStreamRenderGfx(this));
	}

	m_state = PS_USE;
}

void CPedStreamRequestGfx::SetTargetEntity(CPed *pEntity) { 

	Assert(pEntity);

	// add this request extension to the extension list
	pEntity->GetExtensionList().Add(*this);
	m_pTargetEntity = pEntity; 
}

void CPedStreamRequestGfx::ClearWaitForGfx()
{
    if (m_waitForGfx)
        m_waitForGfx->SetWaitForGfx(NULL);
    m_waitForGfx = NULL;
}

// --- CpedStreamRenderGfx

// set default empty values for the stored drawables and textures (for a player ped)
CPedStreamRenderGfx::CPedStreamRenderGfx() 
{

	for(u32 i=0;i<PV_MAX_COMP;i++) {
		m_dwdIdx[i] = -1;
		m_txdIdx[i] = -1;
		m_hdTxdIdx[i] = -1;
		m_hdDwdIdx[i] = -1;
		m_cldIdx[i] = -1;
		m_clothData[i] = NULL;

		m_fpAltIdx[i] = -1;

		m_skelMap[i] = NULL;
		m_skelMapBoneCount[i] = 0;
	}

	for (u32 i = 0; i < HBS_MAX; ++i)
	{
		m_headBlendIdx[i] = -1;
	}

	m_pShaderEffectType = NULL;
	m_pShaderEffect = NULL;
	m_bDisableBulkyItems = false;
	m_culledComponents = 0;
	m_cullInFirstPerson = 0;

	m_hasHighTxdForComponentTxd = 0;
	m_hasHighTxdForComponentDwd = 0;

	m_refCount = 0;
	m_used = true;

#if !__NO_OUTPUT
	m_targetEntity = NULL;
#endif
}

CPedStreamRenderGfx::CPedStreamRenderGfx(CPedStreamRequestGfx *pReq)  
{
	Assert(pReq);

	m_cullInFirstPerson = 0;

	if (pReq){

		CPedModelInfo* pmi = NULL;

		CPed* targetPed = pReq->GetTargetEntity();
		if (targetPed)
		{
			pmi = targetPed->GetPedModelInfo();
		}

		for(u32 i=0;i<PV_MAX_COMP;i++) {
			strLocalIndex dwdSlot = pReq->GetDwdSlot(i);
			if (dwdSlot.IsValid()){
				g_DwdStore.AddRef(dwdSlot, REF_OTHER);
				m_dwdIdx[i] = dwdSlot.Get();
			}
			else
			{
				m_dwdIdx[i] = -1;
			}

			strLocalIndex txdSlot = strLocalIndex(pReq->GetTxdSlot(i));
			if (txdSlot.IsValid()){
				g_TxdStore.AddRef(txdSlot, REF_OTHER);
				m_txdIdx[i] = txdSlot.Get();
			}
			else
			{
				m_txdIdx[i] = -1;
			}

			strLocalIndex fpAltSlot = pReq->GetFpAltSlot(i);
			if (fpAltSlot.IsValid())
			{
				g_DwdStore.AddRef(fpAltSlot, REF_OTHER);
				m_fpAltIdx[i] = fpAltSlot.Get();
			}
			else
			{
				m_fpAltIdx[i] = -1;
			}

			m_clothData[i] = NULL;
			strLocalIndex cldSlot = strLocalIndex(pReq->GetCldSlot(i));
			if (cldSlot.IsValid()){
				g_ClothStore.AddRef(cldSlot, REF_OTHER);
				m_cldIdx[i] = cldSlot.Get();
			}
			else
			{
				m_cldIdx[i] = -1;
			}

			m_skelMap[i] = NULL;
			m_skelMapBoneCount[i] = 0;

			m_hdTxdIdx[i] = -1;
			m_hdDwdIdx[i] = -1;

			// tag all components that need culling in first person cameras
			if (pmi)
			{
				if (pmi->GetVarInfo()->HasComponentFlagsSet((ePedVarComp)i, targetPed->GetPedDrawHandler().GetVarData().GetPedCompIdx((ePedVarComp)i), PV_FLAG_HIDE_IN_FIRST_PERSON))
					m_cullInFirstPerson |= 1 << i;
			}

		}

		for (u32 i = 0; i < HBS_MAX; ++i)
		{
			strLocalIndex assetSlot = strLocalIndex(pReq->GetHeadBlendSlot(i));
			if (assetSlot.IsValid())
			{
				if (i < HBS_HEAD_DRAWABLES)
					g_DwdStore.AddRef(assetSlot, REF_OTHER);
				else if (i < HBS_MICRO_MORPH_SLOTS)
					g_TxdStore.AddRef(assetSlot, REF_OTHER);
				else
					g_DrawableStore.AddRef(assetSlot, REF_OTHER);
				m_headBlendIdx[i] = assetSlot.Get();
			}
			else
			{
				m_headBlendIdx[i] = -1;
			}
		}
	}

	m_pShaderEffectType = NULL;
	m_pShaderEffect = NULL;
	m_bDisableBulkyItems = false;
	m_culledComponents = 0;

	m_hasHighTxdForComponentTxd = pReq->HasHighTxdForComponentTxd();
	m_hasHighTxdForComponentDwd = pReq->HasHighTxdForComponentDwd();

	m_refCount = 0;
	m_used = true;

#if !__NO_OUTPUT 
	m_targetEntity = pReq->GetTargetEntity();
#endif
}

// remove references to any drawables & txds we've loaded
CPedStreamRenderGfx::~CPedStreamRenderGfx() {
	ReleaseGfx();
}

void CPedStreamRenderGfx::ReleaseGfx()
{
	for(u32 i=0;i<PV_MAX_COMP;i++) {

		// remove the diffuse texture we set for this component
		if(m_pShaderEffect) {
			m_pShaderEffect->SetDiffuseTexture(NULL, i);
			m_pShaderEffect->SetDiffuseTexturePal(NULL, i);
		}

		// release the ref to the drawable dict - should cause it to delete
		if (m_dwdIdx[i] != -1) {
			g_DwdStore.RemoveRef(strLocalIndex(m_dwdIdx[i]), REF_OTHER);
			m_dwdIdx[i] = -1;
		}

		// release the ref to the diff tex - should cause it to delete
		if (m_txdIdx[i] != -1) {
			g_TxdStore.RemoveRef(strLocalIndex(m_txdIdx[i]), REF_OTHER);
			m_txdIdx[i] = -1;
		}

		if (m_hdTxdIdx[i] != -1) {
			g_TxdStore.RemoveRef(strLocalIndex(m_hdTxdIdx[i]), REF_OTHER);
			m_hdTxdIdx[i] = -1;
		}

		if (m_hdDwdIdx[i] != -1) {
			g_DwdStore.RemoveRef(strLocalIndex(m_hdDwdIdx[i]), REF_OTHER);
			m_hdDwdIdx[i] = -1;
		}

		if (m_fpAltIdx[i] != -1)
		{
			g_DwdStore.RemoveRef(strLocalIndex(m_fpAltIdx[i]), REF_OTHER);
			m_fpAltIdx[i] = -1;
		}

		DeleteCloth(i);

		delete[] m_skelMap[i];
		m_skelMap[i] = NULL;
		m_skelMapBoneCount[i] = 0;
	}

	ReleaseHeadBlendAssets();

	delete m_pShaderEffect;
	m_pShaderEffect = NULL;

	if (m_pShaderEffectType)
		m_pShaderEffectType->RemoveRef();
	m_pShaderEffectType = NULL;
}

void CPedStreamRenderGfx::DisableAllCloth()
{
	for( int i = 0; i < PV_MAX_COMP; ++i)
	{
		clothVariationData* clothData = m_clothData[i];
		if( clothData )
		{
			Assert( clothData->GetCloth() );
			pgRef<characterCloth>& pCCloth = clothData->GetCloth();
			Assert( pCCloth );
			Assert( pCCloth->GetClothController() );
			clothInstance* cinst = (clothInstance*)pCCloth->GetClothController()->GetClothInstance();
			Assert( cinst );

			clothManager* cManager = CPhysics::GetClothManager();
			Assert( cManager );
			cManager->DisableInstancesWithCommonSkeleton( cinst->GetSkeleton() );
		}
	}
}

void CPedStreamRenderGfx::ReleaseHeadBlendAssets()
{
	gPopStreaming.RemoveStreamedPedVariation(this);
	for (u32 i = 0; i < HBS_MAX; ++i)
	{
		if (m_headBlendIdx[i] == -1)
			continue;

		if (i < HBS_HEAD_DRAWABLES)
			g_DwdStore.RemoveRef(strLocalIndex(m_headBlendIdx[i]), REF_OTHER);
		else if (i < HBS_MICRO_MORPH_SLOTS)
			g_TxdStore.RemoveRef(strLocalIndex(m_headBlendIdx[i]), REF_OTHER);
		else
			g_DrawableStore.RemoveRef(strLocalIndex(m_headBlendIdx[i]), REF_OTHER);

		m_headBlendIdx[i] = -1;
	}
	gPopStreaming.AddStreamedPedVariation(this);
}

void CPedStreamRenderGfx::Init()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reserve(MAX_PED_RENDER_GFX);
	}
}

void CPedStreamRenderGfx::Shutdown()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reset();
	}
}

void CPedStreamRenderGfx::DeleteCloth(u32 compSlot)
{
	clothVariationData* clothData = m_clothData[compSlot];
	if( clothData )
	{
		Assert( m_cldIdx[compSlot] != -1 );

		Assert( clothData->GetCloth() );
		pgRef<characterCloth>& pCCloth = clothData->GetCloth();
		Assert( pCCloth );

		characterClothController* cccontroller = pCCloth->GetClothController();
		Assert( cccontroller );

		clothManager* cManager = CPhysics::GetClothManager();
		Assert( cManager );
		cManager->RemoveCharacterCloth( cccontroller );

		gDrawListMgr->CharClothRemove(clothData);

		g_ClothStore.RemoveRef(strLocalIndex(m_cldIdx[compSlot]), REF_OTHER);
		m_clothData[compSlot] = NULL;
	}
	m_cldIdx[compSlot] = -1;
}


clothVariationData* CPedStreamRenderGfx::GetClothData(u32 compSlot)
{
	Assert( compSlot < PV_MAX_COMP );
	return m_clothData[compSlot];
}


characterCloth* CPedStreamRenderGfx::GetCharacterCloth(u32 compSlot)		
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex cldIdx = strLocalIndex(m_cldIdx[compSlot]);

	if (cldIdx.IsValid() && g_ClothStore.Get(cldIdx)){
		return(g_ClothStore.Get(cldIdx)->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

grcTexture* CPedStreamRenderGfx::GetTexture(u32 compSlot)		
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex txdIdx = strLocalIndex(m_txdIdx[compSlot]);

	if (txdIdx.IsValid() && g_TxdStore.Get(txdIdx)){
		if (Verifyf(g_TxdStore.Get(txdIdx)->GetCount(), "No textures found in txd '%s'", g_TxdStore.GetName(txdIdx)))
			return(g_TxdStore.Get(txdIdx)->GetEntry(0)); 
		else
			return NULL;
	} else {
		return(NULL);
	}
}

grcTexture* CPedStreamRenderGfx::GetHDTexture(u32 compSlot)		
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex hdTxdIdx = strLocalIndex(m_hdTxdIdx[compSlot]);

	if (hdTxdIdx.IsValid() && g_TxdStore.Get(hdTxdIdx)){
		if (Verifyf(g_TxdStore.Get(hdTxdIdx)->GetCount(), "No textures found in txd '%s'", g_TxdStore.GetName(hdTxdIdx)))
			return(g_TxdStore.Get(hdTxdIdx)->GetEntry(0)); 
		else
			return NULL;
	} else {
		return(NULL);
	}
}

s32 CPedStreamRenderGfx::GetTxdIndex(u32 compSlot) const
{ 
	Assert(compSlot < PV_MAX_COMP); 
	return m_txdIdx[compSlot];
}

s32 CPedStreamRenderGfx::GetHDTxdIndex(u32 compSlot) const
{ 
	Assert(compSlot < PV_MAX_COMP); 
	return m_hdTxdIdx[compSlot];
}

s32 CPedStreamRenderGfx::GetDwdIndex(u32 compSlot) const
{ 
	Assert(compSlot < PV_MAX_COMP);
	return m_dwdIdx[compSlot];
}

s32 CPedStreamRenderGfx::GetHDDwdIndex(u32 compSlot) const
{ 
	Assert(compSlot < PV_MAX_COMP); 
	return m_hdDwdIdx[compSlot];
}

rmcDrawable* CPedStreamRenderGfx::GetDrawable(u32 compSlot) const 
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex dwdIdx = strLocalIndex(m_dwdIdx[compSlot]);

	if (dwdIdx.IsValid())
	{
		Dwd* dwdDef = g_DwdStore.Get(dwdIdx);
		return dwdDef ? dwdDef->GetEntry(0) : NULL;
	}
	else
	{
		return NULL;
	}
}

rmcDrawable* CPedStreamRenderGfx::GetHDDrawable(u32 compSlot)		
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex hdDwdIdx = strLocalIndex(m_hdDwdIdx[compSlot]);

	if (hdDwdIdx.IsValid())
	{
		Dwd* dwdDef = g_DwdStore.Get(hdDwdIdx);
		return dwdDef ? dwdDef->GetEntry(0) : NULL;
	}
	else
	{
		return NULL;
	}
}

const rmcDrawable* CPedStreamRenderGfx::GetDrawableConst(u32 compSlot) const
{ 
	Assert(compSlot < PV_MAX_COMP); 
	strLocalIndex dwdIdx = strLocalIndex(m_dwdIdx[compSlot]);

	if (dwdIdx.IsValid())
	{
		Dwd* dwdDef = g_DwdStore.Get(dwdIdx);
		return dwdDef ? dwdDef->GetEntry(0) : NULL;
	}
	else
	{
		return NULL;
	}
}

rmcDrawable* CPedStreamRenderGfx::GetFpAlt(u32 slot) const 
{ 
	Assert(slot < PV_MAX_COMP); 
	strLocalIndex dwdIdx = strLocalIndex(m_fpAltIdx[slot]);

	if (dwdIdx.IsValid())
	{
		Dwd* dwdDef = g_DwdStore.Get(dwdIdx);
		return dwdDef ? dwdDef->GetEntry(0) : NULL;
	}
	else
	{
		return NULL;
	}
}

void CPedStreamRenderGfx::AddRefsToGfx()
{
	for (int i = 0; i < PV_MAX_COMP; ++i)
	{
		// Drawable
		strLocalIndex dwdIdx = strLocalIndex(m_dwdIdx[i]);
		if (dwdIdx.IsValid())
		{
			g_DwdStore.AddRef(dwdIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(dwdIdx.Get(), &g_DwdStore);
		}

		// Texture
		strLocalIndex txdIdx = strLocalIndex(m_txdIdx[i]);
		if (txdIdx.IsValid())
		{
			g_TxdStore.AddRef(txdIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(txdIdx.Get(), &g_TxdStore);
		}

		// HD Texture
		strLocalIndex hdTxdIdx = strLocalIndex(m_hdTxdIdx[i]);
		if (hdTxdIdx.IsValid())
		{
			g_TxdStore.AddRef(hdTxdIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(hdTxdIdx.Get(), &g_TxdStore);
		}

		// HD Drawable
		strLocalIndex hdDwdIdx = strLocalIndex(m_hdDwdIdx[i]);
		if (hdDwdIdx.IsValid())
		{
			g_DwdStore.AddRef(hdDwdIdx, REF_OTHER);
			gDrawListMgr->AddRefCountedModuleIndex(hdDwdIdx.Get(), &g_DwdStore);
		}

		// Cloth
		strLocalIndex clthIdx = strLocalIndex(m_cldIdx[i]);
		if (clthIdx.IsValid())
		{
			g_ClothStore.AddRef(clthIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(clthIdx.Get(), &g_ClothStore);
		}

		// Cloth Data
		clothVariationData *pClothData = m_clothData[i];
		if (pClothData)
		{
			gDrawListMgr->AddCharClothReference(pClothData);
		}

		strLocalIndex fpAltIdx = strLocalIndex(m_fpAltIdx[i]);
		if (fpAltIdx.IsValid())
		{
			g_DwdStore.AddRef(fpAltIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(fpAltIdx.Get(), &g_DwdStore);
		}
	}

	// head blend data
	for (u32 i = 0; i < HBS_MAX; ++i)
	{
		strLocalIndex headBlendIdx = strLocalIndex(m_headBlendIdx[i]);
		if (headBlendIdx.IsValid())
		{
			strIndex headBlendStrIndex;
			strStreamingModule *module = (i < HBS_HEAD_DRAWABLES) ? (strStreamingModule *) &g_DwdStore : (i < HBS_MICRO_MORPH_SLOTS ? (strStreamingModule *) &g_TxdStore : (strStreamingModule *) &g_DrawableStore);
			module->AddRef(headBlendIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(headBlendIdx.Get(), module);
		}
	}

	AddRef();
	ms_renderRefs[ms_fenceUpdateIdx].Push(this);
}
#if __DEV
bool CPedStreamRenderGfx::ValidateStreamingIndexHasBeenCached() 
{
	bool indexInCache = true;

	for (int i = 0; indexInCache && i < PV_MAX_COMP; ++i)
	{
		// Drawable
		s32 dwdIdx = m_dwdIdx[i];
		if (dwdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(dwdIdx, &g_DwdStore);
		}

		// HD Drawable
		s32 hdDwdIdx = m_hdDwdIdx[i];
		if (hdDwdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(hdDwdIdx, &g_DwdStore);
		}
	}

	for (int i = 0; indexInCache && i < PV_MAX_COMP; ++i)
	{	// Texture
		s32 txdIdx = m_txdIdx[i];
		if (txdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(txdIdx, &g_TxdStore);
		}

		// HD Texture
		s32 hdTxdIdx = m_hdTxdIdx[i];
		if (hdTxdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(hdTxdIdx, &g_TxdStore);
		}
	}

	for (int i = 0; indexInCache && i < PV_MAX_COMP; ++i)
	{	// Cloth
		s32 clthIdx = m_cldIdx[i];
		if (clthIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(clthIdx, &g_ClothStore);
		}
	}

	for (int i = 0; indexInCache && i < PV_MAX_COMP; ++i)
	{
		s32 dwdIdx = m_fpAltIdx[i];
		if (dwdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(dwdIdx, &g_DwdStore);
		}
	}

	// head blend data
	for (int i = 0; indexInCache && i < HBS_MAX; ++i)
	{
		s32 headBlendIdx = m_headBlendIdx[i];
		if (headBlendIdx > -1)
		{
			strIndex headBlendStrIndex;
			strStreamingModule *module = (i < HBS_HEAD_DRAWABLES) ? (strStreamingModule *) &g_DwdStore : (i < HBS_MICRO_MORPH_SLOTS ? (strStreamingModule *) &g_TxdStore : (strStreamingModule *) &g_DrawableStore);

			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(headBlendIdx, module);
		}
	}

	return indexInCache;
}
#endif

void CPedStreamRenderGfx::SetUsed(bool val)
{
	m_used = val;

	if (!m_used && !m_refCount)
		delete this;
}

void CPedStreamRenderGfx::SetHdAssets(CPedStreamRequestGfx* reqGfx)
{
	for (s32 i = 0; i < PV_MAX_COMP; ++i)
	{
		// remove old refs
		if (m_hdTxdIdx[i] != -1)
			g_TxdStore.RemoveRef(strLocalIndex(m_hdTxdIdx[i]), REF_OTHER);
		m_hdTxdIdx[i] = -1;

		if (m_hdDwdIdx[i] != -1)
			g_DwdStore.RemoveRef(strLocalIndex(m_hdDwdIdx[i]), REF_OTHER);
		m_hdDwdIdx[i] = -1;

		// add new ones, if we have any hd assets
		if (reqGfx)
		{
			// textures
			strLocalIndex txdSlot = strLocalIndex(reqGfx->GetTxdSlot(i));
			if (txdSlot.IsValid())
			{
				g_TxdStore.AddRef(txdSlot, REF_OTHER);
				m_hdTxdIdx[i] = txdSlot.Get();
			}
			else
				m_hdTxdIdx[i] = -1;

			// drawables
			strLocalIndex dwdSlot = strLocalIndex(reqGfx->GetDwdSlot(i));
			if (dwdSlot.IsValid())
			{
				g_DwdStore.AddRef(dwdSlot, REF_OTHER);
				m_hdDwdIdx[i] = dwdSlot.Get();
			}
			else
				m_hdDwdIdx[i] = -1;
		}
	}
}

void CPedStreamRenderGfx::RemoveRefs()
{
	s32 newUpdateIdx = (ms_fenceUpdateIdx + 1) % FENCE_COUNT;

	for (s32 i = 0; i < ms_renderRefs[newUpdateIdx].GetCount(); ++i)
	{
		CPedStreamRenderGfx* gfx = ms_renderRefs[newUpdateIdx][i];
		if (!gfx)
			continue;

		gfx->m_refCount--;
	}

	ms_renderRefs[newUpdateIdx].Resize(0);
	ms_fenceUpdateIdx = newUpdateIdx;

	// free unused entries
	CPedStreamRenderGfx::Pool* pool = CPedStreamRenderGfx::GetPool();
	if (pool)
	{
		for (s32 i = 0; i < pool->GetSize(); ++i)
		{
			CPedStreamRenderGfx* gfx = pool->GetSlot(i);
			if (!gfx)
				continue;

			if (!gfx->m_used && !gfx->m_refCount)
				delete gfx;
		}
	}
}


grcTexture* CPedStreamRenderGfx::GetHeadBlendTexture(u32 slot) const
{ 
	Assert(slot < HBS_MICRO_MORPH_SLOTS && slot >= HBS_HEAD_DRAWABLES); 
	strLocalIndex txdIdx = strLocalIndex(m_headBlendIdx[slot]);

	if (txdIdx.IsValid()){
		return(g_TxdStore.Get(txdIdx)->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

rmcDrawable* CPedStreamRenderGfx::GetHeadBlendDrawable(u32 slot) const 
{ 
	Assert(slot < HBS_HEAD_DRAWABLES); 
	strLocalIndex dwdIdx = strLocalIndex(m_headBlendIdx[slot]);

	if (dwdIdx.IsValid()){
		return(g_DwdStore.Get(dwdIdx)->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

void CPedStreamRenderGfx::ReplaceTexture(u32 compSlot, s32 newAsset)
{
	g_TxdStore.AddRef(strLocalIndex(newAsset), REF_OTHER);

	if (m_txdIdx[compSlot] != -1)
	{
		g_TxdStore.RemoveRef(strLocalIndex(m_txdIdx[compSlot]), REF_OTHER);
	}

	m_txdIdx[compSlot] = newAsset;
}

void CPedStreamRenderGfx::ReplaceDrawable(u32 compSlot, s32 newAsset)
{
	g_DwdStore.AddRef(strLocalIndex(newAsset), REF_OTHER);

	if (m_dwdIdx[compSlot] != -1)
	{
		g_DwdStore.RemoveRef(strLocalIndex(m_dwdIdx[compSlot]), REF_OTHER);
	}

	m_dwdIdx[compSlot] = newAsset;
}

void CPedStreamRenderGfx::ApplyNewAssets(CPed* pPed)
{
	for (int i = 0; i < PV_MAX_COMP; ++i)
	{
		// fix shader with new assets
		grcTexture* pDiffMap = GetTexture(i);
		rmcDrawable* pDrawable = GetDrawable(i);
		if (!pDrawable)
			continue;

		if (m_pShaderEffect)
			m_pShaderEffect->ClearTextures((ePedVarComp)i);

		// setup this drawable and this texture together
		if (pDiffMap)
		{
			m_pShaderEffectType = static_cast<CCustomShaderEffectPedType*>(CCustomShaderEffectPed::SetupForDrawableComponent(pDrawable, i, m_pShaderEffectType));

			if(m_pShaderEffect)
			{
				m_pShaderEffect->SetDiffuseTexture(pDiffMap, i);
				CPedVariationStream::SetPaletteTexture(pPed, (u32)i);
			}
		}
	}
}

//------------------------------------------------------------------------------------------

#if __DEV 
template<> void fwPool<CPedStreamRequestGfx>::PoolFullCallback() 
{
	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CPedStreamRequestGfx* req = GetSlot(size);
		if(req)
		{
			if (req->GetTargetEntity())
				Displayf("%i, \"%s\", entity addr: 0x%p\n", iIndex, req->GetTargetEntity()->GetPedModelInfo()->GetModelName(), req->GetTargetEntity());
			else
				Displayf("%i, no target entity\n", iIndex);
		}
		else
		{
			Displayf("%i, NULL ped stream request gfx\n", iIndex);
		}
		iIndex++;
	}
}

template<> void fwPool<CPedStreamRenderGfx>::PoolFullCallback() 
{
	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CPedStreamRenderGfx* gfx = GetSlot(size);
		if(gfx)
		{
			if (gfx->m_targetEntity)
				Displayf("%i, \"%s\", entity addr: 0x%p\n", iIndex, gfx->m_targetEntity->GetPedModelInfo()->GetModelName(), gfx->m_targetEntity.Get());
			else
				Displayf("%i, no target entity\n", iIndex);
		}
		else
		{
			Displayf("%i, NULL ped stream request gfx\n", iIndex);
		}
		iIndex++;
	}
}

#endif // __DEV 
