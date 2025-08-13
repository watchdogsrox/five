//
//    Filename: VehicleModelInfo.cpp
//     Creator: Adam Fowler 
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing a model with a hierarchy
//
//
// C headers
#include "ctype.h"

// Rage headers
#include "grcore/effect_config.h" // edge
#include "crskeleton/jointdata.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "diag/art_channel.h"
#include "file/default_paths.h"
#include "fragment/typeGroup.h"
#include "parser/manager.h"
#include "parser/psofile.h"
#include "parser/restparserservices.h"
#include "phbound/BoundBox.h"
#include "phbound/BoundCapsule.h"
#include "phbound/BoundComposite.h"
#include "phbound/boundbvh.h"
#include "phBound/support.h"
#include "system/memory.h"
#include "system/nelem.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwdebug/picker.h"
#include "fwmaths/angle.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwscene/texLod.h"
#include "fwscene/stores/txdstore.h" 
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/clipdictionarystore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/particles/PtFxManager.h"

// Game headers
#include "audio/radioaudioentity.h"
#include "core/game.h"
#include "debug/DebugScene.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "frontend/NewHud.h"
#include "frontend/UIReticule.h"
#include "game/config.h"
#include "game/modelindices.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "modelinfo/VehicleModelInfoPlates.h"
#include "modelinfo/VehicleModelInfoLights.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoData_parser.h"
#include "objects/CoverTuning.h"
#include "pathserver/ExportCollision.h"
#include "pathserver/NavGenParam.h"
#include "physics/gtainst.h"
#include "physics/floater.h"
#include "renderer/HierarchyIds.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/DrawLists/drawList.h"
#include "renderer/RenderTargetMgr.h"
#include "scene/DataFileMgr.h"
#include "scene/texLod.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "system/FileMgr.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Combat/Cover/Cover.h"
#include "vehicles/cargen.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicles/Metadata/VehicleScenarioLayoutInfo.h"
#include "vehicles/HandlingMgr.h"
#include "vehicles/Planes.h"
#include "vehicles/Train.h"
#include "vehicles/Vehicle.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/vehicleLightSwitch.h"
#include "vehicles/wheel.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "scene/EntityIterator.h"
#include "stats/StatsMgr.h"
#include "peds/ped.h"
#include "peds/PedHelmetComponent.h"

#if __BANK
#include "grcore/debugdraw.h"
#include "grcore/setup.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetWindow.h"
#include "debug/UiGadget/UiGadgetList.h"
#include "debug/TextureViewer/TextureViewerSearch.h"
#include "debug/TextureViewer/TextureViewer.h"
#endif
VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()

namespace rage {
XPARAM(noptfx);
}

PARAM(vehdepdebug, "Temp param to hunt down why MP specific vehicle dependencies aren't always loaded in MP games");

EXT_PF_VALUE_INT(NumWaterSamplesInMemory);

fwPool<CVehicleStructure>* CVehicleStructure::m_pInfoPool = NULL;
fwPtrListSingleLink CVehicleModelInfo::ms_HDStreamRequests;
s8 CVehicleModelInfo::ms_numHdVehicles = 0;
float CVehicleModelInfo::ms_maxNumberMultiplier = 1.0f;
CVehicleModelInfoVarGlobal * CVehicleModelInfo::ms_VehicleColours = NULL;
fwPsoStoreLoadInstance CVehicleModelInfo::ms_VehicleColoursLoadInst;
CVehicleModelInfoVariation* CVehicleModelInfo::ms_VehicleVariations = NULL;
fwPsoStoreLoadInstance CVehicleModelInfo::ms_VehicleVariationsLoadInst;
CVehicleModColors* CVehicleModelInfo::ms_ModColors = NULL;
fwPsoStoreLoadInstance CVehicleModelInfo::ms_ModColorsLoadInst;
static CVehicleModColorsGen9 s_ModColorsGen9;

atArray<strIndex> CVehicleModelInfo::ms_residentObjects;
int CVehicleModelInfo::ms_commonTxd = -1;

sVehicleDashboardData CVehicleModelInfo::ms_cachedDashboardData;
u32 CVehicleModelInfo::ms_dialsRenderTargetOwner = 0xd1a15c0d;
u32 CVehicleModelInfo::ms_dialsRenderTargetSizeX = 0;
u32 CVehicleModelInfo::ms_dialsRenderTargetSizeY = 0;
u32 CVehicleModelInfo::ms_dialsRenderTargetId = 0;
u32 CVehicleModelInfo::ms_dialsFrameCountReq = 0;
s32 CVehicleModelInfo::ms_dialsMovieId = -1;
s32 CVehicleModelInfo::ms_dialsType = -1;
s32 CVehicleModelInfo::ms_activeDialsMovie = -1;
atFinalHashString CVehicleModelInfo::ms_rtNameHash;
u32 CVehicleModelInfo::ms_lastTrackId = 0xffffffff;
const char* CVehicleModelInfo::ms_lastStationStr = NULL;
u8 CVehicleModelInfo::ms_requestPerFrame = 0;

const char* s_dialsMovieNames[MAX_DIALS_MOVIES] = {"DASHBOARD", "AIRCRAFT_DIALS", "DIALS_LAZER", "DIALS_LAZER_VINTAGE", "PARTY_DASHBOARD"};

atFixedArray<VehicleClassInfo, VC_MAX> CVehicleModelInfo::ms_VehicleClassInfo(VC_MAX);

#if __BANK
bool CVehicleModelInfo::ms_showHDMemUsage = false;
bool CVehicleModelInfo::ms_ListHDAssets = false;
u32  CVehicleModelInfo::ms_HDTotalPhysicalMemory = 0;
u32  CVehicleModelInfo::ms_HDTotalVirtualMemory = 0;
atArray<HDVehilceRefCount> CVehicleModelInfo::ms_HDVehicleInfos;

CUiGadgetWindow* CVehicleModelInfo::ms_pDebugWindow;
CUiGadgetList*   CVehicleModelInfo::ms_pHDAssetList;
CUiGadgetList* CVehicleModelInfo::ms_pSelectItemList;
CVehicleModelInfo* CVehicleModelInfo::ms_pSelectedVehicle = NULL;

u32 CVehicleModelInfo::ms_numLoadedInfos = 0;
#endif



class CVehicleMetaDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CVehicleModelInfo::LoadVehicleMetaFile(file.m_filename, false, fwFactory::EXTRA);

#if __BANK
		CVehicleFactory::UpdateVehicleList();
#endif // __BANK

		CTrain::TrainConfigsPostLoad();	// bind carriage names to model IDs

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
	{
		CVehicleModelInfo::UnloadVehicleMetaFile(file.m_filename);

#if __BANK
		CVehicleFactory::UpdateVehicleList();
#endif // __BANK

		CTrain::TrainConfigsPostLoad();	// bind carriage names to model IDs
	}

} g_VehicleMetaDataFileMounter;

class CVehicleVariationDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CVehicleModelInfo::AppendVehicleVariations(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/) 
	{
	}

} g_VehicleVariationDataFileMounter;

class CVehicleColorsDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CVehicleModelInfo::AppendVehicleColors(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/) 
	{
	}

} g_VehicleColorsDataFileMounter;

//
// Constructor
// 
CVehicleStructure::CVehicleStructure()
{
	for(int i=0; i<VEH_NUM_NODES; i++)
		m_nBoneIndices[i] = -1;
	for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
	{
		m_nWheelInstances[i][0] = -1;
		m_nWheelInstances[i][1] = -1;
		m_fWheelTyreRadius[i] = 1.0f;
        m_fWheelTyreWidth[i] = 1.0f;
		m_fWheelRimRadius[i] = 1.0f;
		m_fWheelScaleInv[i] = 1.0f;
	}
	m_decalBoneFlags.SetAll();
	m_bWheelInstanceSeparateRear = false;
	m_numSkinnedWheels = 0xf;
}

CVehicleModelInfo::InitData::InitData() {}

void CVehicleModelInfo::InitClass(unsigned initMode)
{
    if(initMode == INIT_AFTER_MAP_LOADED)
    {
		CDataFileMount::RegisterMountInterface(CDataFileMgr::VEHICLE_METADATA_FILE, &g_VehicleMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::VEHICLE_VARIATION_FILE, &g_VehicleVariationDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::CARCOLS_FILE, &g_VehicleColorsDataFileMounter);

        SetupCommonData();
    }
}

void CVehicleModelInfo::ShutdownClass(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        DeleteCommonData();
    }
}

void CVehicleModelInfo::UnloadResidentObjects()
{
	bool bUseBurnoutTexture = CGameConfig::Get().UseVehicleBurnoutTexture();
	if (bUseBurnoutTexture)
		CCustomShaderEffectVehicle::SetupBurnoutTexture(-1);

	g_vehicleGlassMan.PrepareVehicleRpfSwitch();

	for (int i = 0; i < ms_residentObjects.GetCount(); ++i)
	{
		strIndex objIndex = ms_residentObjects[i];
		strStreamingEngine::GetInfo().ClearRequiredFlag(objIndex, STRFLAG_DONTDELETE);

#if __BANK
		// Temporary test code
		if (strStreamingEngine::GetInfo().GetStreamingInfoRef(objIndex).GetDependentCount() != 0)
		{
			Errorf("Shared resource %s still has dependencies", strStreamingEngine::GetObjectName(objIndex));
			strStreamingEngine::GetInfo().PrintAllDependents(objIndex);
			Assertf(false, "Shared vehicle resource still has dependencies - something didn't get flushed. Check TTY for dependencies.");
		}
#endif // __BANK

		strStreamingEngine::GetInfo().RemoveObject(objIndex);
	}
}

void CVehicleModelInfo::LoadResidentObjects(bool bBlockUntilDone/*=false*/)
{
	for (int i = 0; i < ms_residentObjects.GetCount(); ++i)
	{
		strStreamingEngine::GetInfo().RequestObject(ms_residentObjects[i], STRFLAG_DONTDELETE);
	}

	if (bBlockUntilDone)
	{
		CStreaming::LoadAllRequestedObjects();
	}

	bool bUseBurnoutTexture = CGameConfig::Get().UseVehicleBurnoutTexture();
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision()) bUseBurnoutTexture = false;
#endif

	g_vehicleGlassMan.CompleteVehicleRpfSwitch();

	// assume first resident object is the common texture dictionary
	if (bUseBurnoutTexture)
	{
		CCustomShaderEffectVehicle::SetupBurnoutTexture(ms_commonTxd);
	}
}

//
// name:		SetupCommonData
// description:	Setup data common to all vehicle modelinfo structures 
//				(eg Common textures, Vehicle colours)
void CVehicleModelInfo::SetupCommonData()
{
	CVehicleModelInfoVarGlobal::Init();

#if VEHICLE_SUPPORT_PAINT_RAMP
	CCustomShaderEffectVehicle::LoadRampTxd();
#endif
	
	// load colours for vehicles
	LoadVehicleColours();
	
	// load permanently resident texture dictionaries
    if (CFileMgr::IsGameInstalled())
	{
		LoadResidentObjects(true);
	}

	// initialise instanced wheel render
	CWheelInstanceRenderer::Initialise();

	// allocate space for info pool
    const char *poolName = "VehicleStruct";
    int iMaxVehicleStructPoolSize = fwConfigManager::GetInstance().GetSizeOfPool(atStringHash(poolName), CONFIGURED_FROM_FILE );

#if NAVMESH_EXPORT
	// For exporting navmeshes we need to load in all the vehicles - gameconfig.xml defined pool size *8 should be enough
	iMaxVehicleStructPoolSize = CNavMeshDataExporter::WillExportCollision() ? iMaxVehicleStructPoolSize*8 : iMaxVehicleStructPoolSize;
#endif

#if COMMERCE_CONTAINER
		CVehicleStructure::m_pInfoPool = rage_new fwPool<CVehicleStructure>(
			iMaxVehicleStructPoolSize,
			0.35f,
			poolName,
			0,
			sizeof(CVehicleStructure),
			false);
#else
		CVehicleStructure::m_pInfoPool = rage_new fwPool<CVehicleStructure>(
			iMaxVehicleStructPoolSize,
			0.35f,
			poolName);
#endif
}		

//
// name:		DeleteCommonData
// description:	Delete common data
void CVehicleModelInfo::DeleteCommonData()
{
	// close instanced wheel render
	CWheelInstanceRenderer::Shutdown();

	delete CVehicleStructure::m_pInfoPool;
	UnloadResidentObjects();

	ms_residentObjects.ResetCount();

#if VEHICLE_SUPPORT_PAINT_RAMP
	CCustomShaderEffectVehicle::UnloadRampTxd();
#endif
}

/*
//
// name:		LoadVehicleUpgrades
// description:	Loads the vehicle upgrade file
void CVehicleModelInfo::LoadVehicleUpgrades()
{
	enum ModLoadingState {
		LOADING_NOTHING,
		LOADING_LINKS,
		LOADING_MODS,
		LOADING_WHEELS
	};
	ModLoadingState loadingState = LOADING_NOTHING;
	char* pLine;
	const char whitespace[] = " \t,";
	FileHandle fd;
	char* pToken;
	s32 i;
		
	fd = CFileMgr::OpenFile("DATA\\CARMODS.DAT");
	modelinfoAssertf(CFileMgr::IsValidFileHandle(fd), "problem loading carmods.dat");
	
	// initialise wheel upgrades
	for(i=0; i<NUM_WHEEL_UPGRADE_CLASSES; i++)
		ms_numWheelUpgrades[i] = 0;

	// get each line. Each line is a model file
	while((pLine = CFileMgr::ReadLine(fd)) != NULL)
	{
		if(pLine[0] == '#' || pLine[0] == '\0')
			continue;

		if(loadingState == LOADING_NOTHING)
		{
			if(!strncmp("link", pLine, 4))
				loadingState = LOADING_LINKS;
			else if(!strncmp("mods", pLine, 4))
				loadingState = LOADING_MODS;
			else if(!strncmp("wheel", pLine, 5))
				loadingState = LOADING_WHEELS;
		}		
		else
		{
			if(!strncmp("end", pLine, 3))
				loadingState = LOADING_NOTHING;
			
			switch(loadingState)
			{
			case LOADING_MODS:
				pToken = strtok(pLine, whitespace);	// get first token
				
				if(pToken)
				{
					s32 index=-1;
					i=0;
					
					CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(pToken, &index);
					modelinfoAssertf(pModelInfo, "%s:Unrecognised model in carmods.dat", pToken);
					modelinfoAssertf(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "%s:Unrecognised car model in carmods.dat", pToken);

					while((pToken = strtok(NULL, whitespace)) != NULL)
					{
						modelinfoAssertf(i<MAX_POSSIBLE_UPGRADES, "%s:Vehicle has too many upgrades", pModelInfo->GetModelName());
						CBaseModelInfo* pUpgradeInfo = CModelInfo::GetBaseModelInfo(pToken, &index);
						modelinfoAssertf(pUpgradeInfo, "%s:Unrecognised model in carmods.dat", pToken);
						
						// setup upgrade and store in vehicle modelinfo
						pUpgradeInfo->SetupVehicleUpgradeFlags(pToken);
						((CVehicleModelInfo*)pModelInfo)->SetVehicleUpgrade(i++, index);
					}
					
					// setup hydraulics, stereo and nitro upgrades
					CBaseModelInfo* pUpgradeInfo = CModelInfo::GetBaseModelInfo("hydralics", &index);
					pUpgradeInfo->SetupVehicleUpgradeFlags("hydralics");
					((CVehicleModelInfo*)pModelInfo)->SetVehicleUpgrade(i++, index);
					pUpgradeInfo = CModelInfo::GetBaseModelInfo("stereo", &index);
					pUpgradeInfo->SetupVehicleUpgradeFlags("stereo");
					((CVehicleModelInfo*)pModelInfo)->SetVehicleUpgrade(i++, index);
				}
				break;

			case LOADING_LINKS:
				{
					s32 id1=-1, id2=-1;
					pToken = strtok(pLine, whitespace);	// get first token
					if(pToken)
					{
						// Get first model
						CBaseModelInfo* pUpgradeInfo = CModelInfo::GetBaseModelInfo(pToken, &id1);
						modelinfoAssertf(id1 != -1, "%s:Unrecognised model in carmods.dat", pToken);
						pUpgradeInfo->SetupVehicleUpgradeFlags(pToken);

						pToken = strtok(NULL, whitespace);	// get second token
						modelinfoAssertf(pToken, "Corrupt carmods.dat file");
						
						// Get second model
						pUpgradeInfo = CModelInfo::GetBaseModelInfo(pToken, &id2);
						modelinfoAssertf(id2 != -1, "%s:Unrecognised model in carmods.dat", pToken);
						pUpgradeInfo->SetupVehicleUpgradeFlags(pToken);
						
						// store link
						ms_linkedUpgrades.AddUpgradeLink(static_cast<s16>(id1), static_cast<s16>(id2));
					}
				}
				break;
			
			case LOADING_WHEELS:
				{
					s32 wheelClass;
					s32 index=-1;
					sscanf(pLine, "%d", &wheelClass);
					
					pToken = strtok(pLine, whitespace);	// get first token
					while((pToken = strtok(NULL, whitespace)) != NULL)
					{
						CBaseModelInfo* pWheelInfo = CModelInfo::GetBaseModelInfo(pToken, &index);
						modelinfoAssertf(pWheelInfo, "%s:Unrecognised wheel object", pToken);
						pWheelInfo->SetupVehicleUpgradeFlags(pToken);
						
						AddWheelUpgrade(wheelClass, index);
					}					
				}
				break;

			default:
				break;
			}	
		}	
	}
	modelinfoAssertf(loadingState == LOADING_NOTHING, "Corrupt carmods.dat file");
	CFileMgr::CloseFile(fd);

}
*/

#if !__PS3
float CVehicleModelInfo::ms_LodMultiplierBias = 0.0f;
void CVehicleModelInfo::SetLodMultiplierBias( float multiplierBias )
{ 
	ms_LodMultiplierBias = multiplierBias;
}
#endif

//
// Load vehicle colour data from xml
//
void CVehicleModelInfo::LoadVehicleColours()
{
	fwPsoStoreLoader colLoader = fwPsoStoreLoader::MakeSimpleFileLoader<CVehicleModelInfoVarGlobal>();
	colLoader.Unload(ms_VehicleColoursLoadInst);
	colLoader.Load("platform:/data/carcols." META_FILE_EXT, ms_VehicleColoursLoadInst);
	ms_VehicleColours = reinterpret_cast<CVehicleModelInfoVarGlobal*>(ms_VehicleColoursLoadInst.GetInstance());
	ms_VehicleColours->OnPostLoad(BANK_ONLY(RS_ASSETS "/titleupdate/dev_ng/data/carcols.pso.meta"));

	modelinfoFatalAssertf(ms_VehicleColours, "Error loading carcols." META_FILE_EXT);
    if (ms_VehicleColours)
		parRestRegisterSingleton("Vehicles/carcols", *ms_VehicleColours, "platform:/data/carcols." META_FILE_EXT); // Make this object browsable / editable via rest services (i.e. the web)

#if __ASSERT
	if(1)
	{
		const int kitCount = ms_VehicleColours->m_Kits.GetCount();
		for(int k=0; k<kitCount; k++)
		{
			CVehicleKit& kit = ms_VehicleColours->m_Kits[k];

			// check vehicle modkits for duplicate drawables:
			const atArray<CVehicleModVisible>& kitVisibleMods = kit.GetVisibleMods();
			const int modsCount = kitVisibleMods.GetCount();
			for(int m=0; m<modsCount; m++)
			{
				const u32 modModelHash = kitVisibleMods[m].GetNameHash();

				if(modModelHash != MID_DUMMY_MOD_FRAG_MODEL)	// skip this check for "dummy_mod_frag_model"
				{
					if (kitVisibleMods[m].GetType() == VMT_INVALID)
					{
						Assertf(false, "Vehicle modkit %s has drawable %s with type VMT_INVALID. Maybe there is a spelling mistake in the <type> field for this drawable in carcols.pso.meta. This is a vehicle art error.", 
							kit.GetNameHashString().GetCStr(), kitVisibleMods[m].GetNameHashString().GetCStr() );
						Warningf("Vehicle modkit %s has drawable %s with type VMT_INVALID. Maybe there is a spelling mistake in the <type> field for this drawable in carcols.pso.meta. This is a vehicle art error.", 
							kit.GetNameHashString().GetCStr(), kitVisibleMods[m].GetNameHashString().GetCStr() );
					}

					// check if this drawable is not shared with any other mod in this kit:
					for(int i=0; i<modsCount; i++)
					{
						if(i != m)
						{
							if(kitVisibleMods[i].GetNameHash() == modModelHash)
							{	// found shared drawable (not supported):
								Assertf(false, "Vehicle modkit %s uses the same drawable %s for 2 different mods! The indices are %d and %d. This is a vehicle art error.", kit.GetNameHashString().GetCStr(), kitVisibleMods[m].GetNameHashString().GetCStr(), m, i);
								Warningf("Vehicle modkit %s uses the same drawable %s for 2 different mods! The indices are %d and %d. This is a vehicle art error.", kit.GetNameHashString().GetCStr(), kitVisibleMods[m].GetNameHashString().GetCStr(), m, i);
							}
						}
					}
				}
			}

			// check to make sure the number of horns is correct
			ValidateHorns(kit);
		}
	}

#endif //__ASSERT...

#if __BANK
static bool bEnableSave=false;
	if(bEnableSave)
	{
		PARSER.SaveObject(RS_ASSETS "/export/data/carcols.pso.meta", "", ms_VehicleColours);
	}
	ms_VehicleColours->AddCarColFile(RS_ASSETS "/titleupdate/dev_ng/data/carcols.pso.meta");
#endif // __BANK

	fwPsoStoreLoader varLoader = fwPsoStoreLoader::MakeSimpleFileLoader<CVehicleModelInfoVariation>();
	varLoader.Unload(ms_VehicleVariationsLoadInst);
	varLoader.Load("platform:/data/carvariations." META_FILE_EXT, ms_VehicleVariationsLoadInst);
	ms_VehicleVariations = reinterpret_cast<CVehicleModelInfoVariation*>(ms_VehicleVariationsLoadInst.GetInstance());
    modelinfoFatalAssertf(ms_VehicleVariations, "Error loading platform:/data/carvariations." META_FILE_EXT);
	ms_VehicleVariations->OnPostLoad();

	fwPsoStoreLoader modLoader = fwPsoStoreLoader::MakeSimpleFileLoader<CVehicleModColors>();
	modLoader.Unload(ms_ModColorsLoadInst);
	modLoader.Load("platform:/data/carmodcols." META_FILE_EXT, ms_ModColorsLoadInst);
	ms_ModColors = reinterpret_cast<CVehicleModColors*>(ms_ModColorsLoadInst.GetInstance());
	modelinfoFatalAssertf(ms_ModColors, "Error loading platform:/data/carmodcols." META_FILE_EXT);

	if (ms_ModColors)
        parRestRegisterSingleton("Vehicles/carmodcols", *ms_ModColors, "platform:/data/carmodcols." META_FILE_EXT);

#if !__FINAL
	// Create default no siren if it doesn't exist.
	if( ms_VehicleColours->m_Sirens.GetCount() == 0 )
	{
		sirenSettings & Siren = ms_VehicleColours->m_Sirens.Grow();
		Siren.name = StringDuplicate("0 NoSirens");
	}	
#endif // !__FINAL	
	// Copy the loaded colors into the vehicle model list
	const int numCols = ms_VehicleColours->GetColorCount();

	for(int i=0;i<ms_VehicleVariations->GetVariationData().GetCount();i++)
	{
		const CVehicleVariationData& varData = ms_VehicleVariations->GetVariationData()[i];
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(varData.GetModelName(), NULL);
		artAssertf(pModelInfo, "carvariations.pso.meta is referencing a vehicle (%s) that doesn't exist", varData.GetModelName());
		if( pModelInfo )
		{
			pModelInfo->UpdateModelColors(varData, numCols);
			pModelInfo->UpdateModKits(varData);
			pModelInfo->UpdateWindowsWithExposedEdges(varData);
			pModelInfo->UpdatePlateProbabilities(varData);
		}
	}

#if !ENABLE_VEHICLECOLOURTEXT
	// clean up the color names if not needed (Russ is looking to expand the parser so we can selectively not load data in certain configurations, when this happens change this)
	for(int i=0;i<numCols;i++)
	{
		StringFree(ms_VehicleColours->m_Colors[i].m_colorName);
		ms_VehicleColours->m_Colors[i].m_colorName = NULL;
	}
#endif	

#if __BANK

	// Update the vehiclecolours data with any vehicles which have been added since we last saved
	// this is a n^2 search - may need optimizing for speed should the vehicle list become substantial (is beta only though)
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypes;
	vehModelInfoStore.GatherPtrs(vehicleTypes);

	const u32 numVehicles = vehicleTypes.GetCount();
	const int numXmlVehiclesAtStart = ms_VehicleVariations->GetVariationData().GetCount();
	atBitSet bUsedXmlEntries(numXmlVehiclesAtStart);

	for(u32 i=0;i<numVehicles;i++)
	{
		CVehicleModelInfo& modelInfo = *vehicleTypes[i];
		const char* modelName = modelInfo.GetModelName();

		bool foundIt = false;
		for(int j=0;j<numXmlVehiclesAtStart;j++)
			if (bUsedXmlEntries.IsClear(j) && stricmp(ms_VehicleVariations->GetVariationData()[j].GetModelName(), modelName) == 0)
			{
				foundIt = true;
				bUsedXmlEntries.Set(j);
				break;
			}
			if (!foundIt)
			{
				CVehicleVariationData& varData = ms_VehicleVariations->GetVariationData().Grow();
				varData.SetModelName(modelInfo.GetModelName());
				const u32 numColors = modelInfo.GetNumPossibleColours();
				varData.ReserveColors(numColors);
				for(u32 j=0;j<numColors;j++)
				{
					CVehicleModelColorIndices& indices = varData.AppendColor();
					indices.m_indices[0] = modelInfo.GetPossibleColours(0, j);
					indices.m_indices[1] = modelInfo.GetPossibleColours(1, j);
					indices.m_indices[2] = modelInfo.GetPossibleColours(2, j);
					indices.m_indices[3] = modelInfo.GetPossibleColours(3, j);
					indices.m_indices[4] = modelInfo.GetPossibleColours(4, j);
					indices.m_indices[5] = modelInfo.GetPossibleColours(5, j);
				}

				varData.SetLightSettings(modelInfo.GetLightSettingsId());
				varData.SetSirenSettings(modelInfo.GetSirenSettingsId());
			}
	}
	for(int j=0, deleteIdx=0;j<numXmlVehiclesAtStart;j++)
		if (bUsedXmlEntries.IsClear(j))
			ms_VehicleVariations->GetVariationData().Delete(deleteIdx);
		else
			deleteIdx++;

#else	// __BANK

	// We no longer need the model color data (it has been copied into the ModelInfo classes and we are not editing)
	// Free up the memory!
	ms_VehicleVariations->GetVariationData().Reset();
#endif // !__BANK
}

void CVehicleModelInfo::LoadVehicleMetaFile(const char *filename, bool permanent, s32 mapTypeDefIndex)
{
	struct InitDataList* pInitDataList = NULL;
	parStructure* structure = pInitDataList->parser_GetStaticStructure();
	parSettings settings = PARSER.Settings();
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA,true);
	if(PARSER.CreateAndLoadAnyType(filename, "meta", structure, reinterpret_cast<parPtrToStructure&>(pInitDataList),&settings))
	{
		// add slot for resident txd
		strLocalIndex txtSlot = g_TxdStore.FindSlot(pInitDataList->m_residentTxd.c_str());
		if (txtSlot == -1)
		{
			txtSlot = g_TxdStore.AddSlot(pInitDataList->m_residentTxd.c_str());
		}
		SetupHDSharedTxd(pInitDataList->m_residentTxd.c_str());

		modelinfoAssertf((ms_commonTxd == -1) || (txtSlot == ms_commonTxd), "Don't remap the common txd - existing vehicles will break");
		//@@: location CVEHICLEMODELINFO_LOADVEHICLEMETAFILE
		SetCommonTxd(txtSlot.Get());
		AddResidentObject(txtSlot, g_TxdStore.GetStreamingModuleId());

		// add slots for resident anims
		for(int i=0; i<pInitDataList->m_residentAnims.GetCount(); i++)
		{
			strLocalIndex slot = g_ClipDictionaryStore.FindSlot(pInitDataList->m_residentAnims[i].c_str());
			if (slot == -1)
			{
				slot = g_ClipDictionaryStore.AddSlot(pInitDataList->m_residentAnims[i].c_str());
			}
			AddResidentObject(slot, g_ClipDictionaryStore.GetStreamingModuleId());
		}

		u32 maxCountAsFacebookDriven = 0;
		// load model infos
        if (CFileMgr::IsGameInstalled())
		{
			for (s32 i = 0; i < pInitDataList->m_InitDatas.GetCount(); ++i)
			{
				fwModelId id;
				if(!CModelInfo::GetBaseModelInfoFromName(pInitDataList->m_InitDatas[i].m_modelName.c_str(), &id))
				{
					CVehicleModelInfo* pModelInfo = CModelInfo::AddVehicleModel(pInitDataList->m_InitDatas[i].m_modelName.c_str(), permanent, mapTypeDefIndex);
					GAMETOOL_ONLY(if (pModelInfo))
					{
						pModelInfo->Init(pInitDataList->m_InitDatas[i]);
#if __BANK
						pModelInfo->SetVehicleDLCPack(filename);
#endif //__BANK
						if (!permanent)
						{
							CModelInfo::PostLoad(pModelInfo);
						}

						if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_COUNT_AS_FACEBOOK_DRIVEN))
						{
							++maxCountAsFacebookDriven;
						}
					}
				}
				else
				{
					Warningf("CVehicleModelInfo::LoadVehicleMetaFile Model info for vehicle '%s' already exists!", pInitDataList->m_InitDatas[i].m_modelName.c_str());
				}
			}
		}
		CStatsMgr::SetMaxCountAsFacebookDriven(maxCountAsFacebookDriven);

		strLocalIndex vehiclePatchSlot = g_TxdStore.FindSlot("vehshare_tire");
		if (vehiclePatchSlot != -1)
		{
			g_TxdStore.SetParentTxdSlot(vehiclePatchSlot,strLocalIndex(GetCommonTxd()));
		}

		// load txd relationships
		for (s32 i = 0; i < pInitDataList->m_txdRelationships.GetCount(); ++i)
		{
			{
				strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(pInitDataList->m_txdRelationships[i].m_child.c_str()));

				if(txdSlot == -1)
				{
					txdSlot = g_TxdStore.AddSlot(pInitDataList->m_txdRelationships[i].m_child.c_str());
				}

				strLocalIndex txdParentSlot = strLocalIndex(g_TxdStore.FindSlot(pInitDataList->m_txdRelationships[i].m_parent.c_str()));

				if (vehiclePatchSlot != -1 && txdParentSlot == GetCommonTxd() )
				{
					txdParentSlot = vehiclePatchSlot;
				} 	
				else if(txdParentSlot == -1)
				{
					{
						txdParentSlot = g_TxdStore.AddSlot(pInitDataList->m_txdRelationships[i].m_parent.c_str());
					}
				}

				g_TxdStore.SetParentTxdSlot(txdSlot,txdParentSlot);
			}
		}			
	}

	delete pInitDataList;

	// some shared .txds for vehicles aren't being set as upgradeable through data...
	static bool bSetUpVehShare_Truck = true;
	if (bSetUpVehShare_Truck)
	{
		SetupHDSharedTxd("vehshare_truck");
		bSetUpVehShare_Truck = false;
	}
}


void CVehicleModelInfo::UnloadVehicleMetaFile(const char *filename)
{
	struct InitDataList* pInitDataList = NULL;

	if (PARSER.LoadObjectPtr(filename, "meta", pInitDataList))
	{
		// unload model infos

		for (s32 i = 0; i < pInitDataList->m_InitDatas.GetCount(); ++i)
		{
			UnloadModelInfoAssets(pInitDataList->m_InitDatas[i].m_modelName.c_str());
		}			
	}

	delete pInitDataList;
}

void CVehicleModelInfo::UnloadModelInfoAssets(const char *name) 
{
	fwModelId modelId = CModelInfo::GetModelIdFromName(name);
	Assert(modelId.IsValid());

	if(CVehicleModelInfo* vmi = static_cast<CVehicleModelInfo*>(CModelInfo::GetBaseModelInfo(modelId)))
	{
		for(int i = vmi->GetNumVehicleInstances() - 1; i >= 0; i--)
		{
			CVehicle* veh = vmi->GetVehicleInstance(i);

			if(veh)
			{
#if !__NO_OUTPUT
				Errorf("Removing vehicle model info '%s'. Found instance of still alive vehicle Veh ptr index = %d 0x%p - (%.2f, %.2f, %.2f).", vmi->GetModelName(), i,
					veh, veh->GetTransform().GetPosition().GetXf(), veh->GetTransform().GetPosition().GetYf(), veh->GetTransform().GetPosition().GetZf());
#endif // !__NO_OUTPUT

				g_ptFxManager.RemovePtFxFromEntity(veh);
				CVehicleFactory::GetFactory()->Destroy(veh);
			}
		}

		CModelInfo::RemoveAssets(modelId);

#if __BANK
		CVehicleFactory::UpdateVehicleList();
#endif // __BANK

	}
	else
	{
		Errorf("CVehicleModelInfo::UnloadModelInfo can't find model info for '%s'", name);
	}
}

void CVehicleModelInfo::AppendVehicleVariations(const char *filename)
{
	const int numCols = ms_VehicleColours->GetColorCount();

	CVehicleModelInfoVariation* pNewVehicleVariations = NULL;

	if (PARSER.LoadObjectPtr(filename, "meta", pNewVehicleVariations))
	{
		pNewVehicleVariations->OnPostLoad();

		for(int i = 0; i < pNewVehicleVariations->GetVariationData().GetCount(); i++)
		{
			const CVehicleVariationData& varData = pNewVehicleVariations->GetVariationData()[i];
			CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(varData.GetModelName(), NULL);

			artAssertf(pModelInfo, "%s is referencing a vehicle (%s) that doesn't exist", filename, varData.GetModelName());

			if( pModelInfo )
			{
				pModelInfo->UpdateModelColors(varData, numCols);
				pModelInfo->UpdateModKits(varData);
				pModelInfo->UpdateWindowsWithExposedEdges(varData);
				pModelInfo->UpdatePlateProbabilities(varData);

#if __BANK
				ms_VehicleVariations->GetVariationData().PushAndGrow(varData);
#endif // __BANK

			}
		}
	}

	delete pNewVehicleVariations;
}

void CVehicleModelInfo::AppendVehicleColors(const char *filename)
{
	CVehicleModelInfoVarGlobal pNewVehicleColors ;

	if (PARSER.LoadObject(filename, "meta", pNewVehicleColors))
	{
		Assertf(pNewVehicleColors.m_Colors.GetCount() == 0, 
			"%s has new colors new colors currently not supported by DLC code!", filename);

		Assertf(pNewVehicleColors.m_WindowColors.GetCount() == 0, 
			"%s has new window colors new window colors currently not supported by DLC code!", filename);

		Assertf(pNewVehicleColors.m_MetallicSettings.GetCount() == 0, 
			"%s has new metallic settings metallic settings can't be extended!", filename);

		int kitsStart = ms_VehicleColours->m_Kits.GetCount();
		int lightsStart = ms_VehicleColours->m_Lights.GetCount();
		int sirensStart = ms_VehicleColours->m_Sirens.GetCount();

		int nAddedKits = AppendArray(ms_VehicleColours->m_Kits, pNewVehicleColors.m_Kits, INVALID_VEHICLE_KIT_ID);
		int nAddedLights = AppendArray(ms_VehicleColours->m_Lights, pNewVehicleColors.m_Lights, INVALID_VEHICLE_LIGHT_SETTINGS_ID);
		int nAddedSirens = AppendArray(ms_VehicleColours->m_Sirens, pNewVehicleColors.m_Sirens, INVALID_SIREN_SETTINGS_ID);

		ms_VehicleColours->UpdateKitIds(kitsStart, nAddedKits BANK_ONLY(, filename));
		ms_VehicleColours->UpdateLightIds(lightsStart, nAddedLights BANK_ONLY(, filename));
		ms_VehicleColours->UpdateSirenIds(sirensStart, nAddedSirens BANK_ONLY(, filename));

		for(int i = 0; i < VWT_MAX; i++)
		{
			int wheelsStart = ms_VehicleColours->m_Wheels[i].GetCount();
			int nAddedWheels = AppendArray(ms_VehicleColours->m_Wheels[i], pNewVehicleColors.m_Wheels[i], INVALID_VEHICLE_WHEEL_ID);

			ms_VehicleColours->UpdateWheelIds((eVehicleWheelType)i, wheelsStart, nAddedWheels BANK_ONLY(, filename));
		}

#if __BANK
		const int newKitCount = pNewVehicleColors.m_Kits.GetCount();
		for(int k = 0; k < newKitCount; k++)
		{
			ValidateHorns(pNewVehicleColors.m_Kits[k]);
		}

		ms_VehicleColours->AddCarColFile(filename);
#endif // __BANK
	}

}

void CVehicleModelInfo::AppendVehicleColorsGen9(const char *filename)
{
	CVehicleModelColorsGen9 colsGen9;
	if (PARSER.LoadObject(filename, "meta", colsGen9))
	{
		u32 colNum = colsGen9.m_Colors.GetCount();
		for (u32 i = 0; i < colNum; ++i)
		{
			CVehicleModelColorGen9& c = ms_VehicleColours->GetColors().Grow();
			c = colsGen9.m_Colors[i];
		}
	}
}

CVehicleModColorsGen9* CVehicleModelInfo::GetVehicleModColorsGen9() { return &s_ModColorsGen9; }

void CVehicleModelInfo::LoadVehicleModColorsGen9(const char *filename)
{
	PARSER.LoadObject(filename, "meta", s_ModColorsGen9);
}

#if __BANK
void CVehicleModelInfo::AddWidgets(bkBank & b)
{
	b.PushGroup("Vehicle Colours", false);
	if (ms_VehicleColours)
		ms_VehicleColours->AddWidgets(b);
	b.PopGroup();

	b.PushGroup("Vehicle License Plates", false);
	if(ms_VehicleColours)
	{
		b.AddTitle(	"NOTE: License plate settings have been moved so they are grouped with the Car Colors. The save button has been added here"
			"as well for convenience. Please keep in mind that saving here will also save any changes to the car color palettes.");
		b.AddTitle("NOTE: This will NOT save the colors assigned to each individual car.");
		b.AddButton("Save", CFA(CVehicleModelInfo::SaveVehicleColours));
		ms_VehicleColours->GetLicensePlateData().AddWidgets(b);
	}
	b.PopGroup();

	b.PushGroup("Vehicle Variations", false);
	if (ms_VehicleVariations)
	{
		ms_VehicleVariations->AddWidgets(b);
		b.AddButton("Save", CFA(CVehicleModelInfo::SaveVehicleVariations));
	}
	b.PopGroup();
	b.AddButton("Reload Meta Data", CFA(CVehicleModelInfo::ReloadColourMetadata));
	b.AddButton("Dump instance data", DumpVehicleInstanceDataCB);
	b.AddButton("Dump vehicle model infos", CFA(CModelInfo::DumpVehicleModelInfos));
	b.AddButton("Dump average vehicle size", CFA(CModelInfo::DumpAverageVehicleSize));
}

void CVehicleModelInfo::AddHDWidgets(bkBank &bank)
{
	bank.PushGroup("Vehicles", false);
		bank.AddToggle("Show HD Vehicle Memory Usage", &ms_showHDMemUsage);
		bank.AddToggle("List HD Assets", &ms_ListHDAssets);
		bank.AddButton("Show select Asset's TXD in Texture Viewer", &ShowSelectTxdInTexViewCB);
	bank.PopGroup();
}


void CVehicleModelInfo::DebugUpdate()
{
	if (PARAM_vehdepdebug.Get())
		CModelInfo::CheckVehicleAssetDependencies();

	if (ms_showHDMemUsage)
	{
		GetHDMemoryUsage(ms_HDTotalPhysicalMemory, ms_HDTotalVirtualMemory);

		grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

		const atVarString temp("HD Vehicle Memory Usage - Physical %.2fMB, Virtual %.2fMB", 
								float((double)ms_HDTotalPhysicalMemory / (1024.0 * 1024.0)),
								float((double)ms_HDTotalVirtualMemory / (1024.0 * 1024.0)));

		grcDebugDraw::Text(Vector2(270.0f, 600.0f), DD_ePCS_Pixels, CRGBA_White(), temp, true, 1.0f, 1.0f);

		grcDebugDraw::TextFontPop();
	}

	if (ms_ListHDAssets)
	{
		static s32 currentSelectionSlot = -1;

		if (ms_pDebugWindow == NULL)
		{
			// this creates a window
			CUiColorScheme colorScheme;
			ms_pDebugWindow = rage_new CUiGadgetWindow(40.0f, 40.0f, 620.0f, 180.0f, colorScheme);
			ms_pDebugWindow->SetTitle("Active HD Vehicles");

			// this attaches a scrolling list to the window
			float afColumnOffsets[] = { 0.0f, 80.0f, 200.0f };
			const char* columnTitles1[] = { "Model", "HD Physical Size", "HD Virtual Size" };
			ms_pHDAssetList = rage_new CUiGadgetList(42.0f, 78.0f, 600.0f, 8, 3, afColumnOffsets, colorScheme, columnTitles1);
			ms_pDebugWindow->AttachChild(ms_pHDAssetList);
			ms_pHDAssetList->SetUpdateCellCB(UpdateCellForAsset);

			// this attaches a scrolly list to the window
			float afColumnOffsets2[] = { 0.0f, 100.0f, 200.0f, 400.0f };
			const char* columnTitles2[] = { "Asset Name", "Type", "HD Physical Size", "HD Virtual Size" };
			ms_pSelectItemList = rage_new CUiGadgetList(42.0f, 190.0f, 600.0f, 2, 4, afColumnOffsets2, colorScheme, columnTitles2);
			ms_pDebugWindow->AttachChild(ms_pSelectItemList);
			ms_pSelectItemList->SetUpdateCellCB(UpdateCellForItem);
			ms_pSelectItemList->SetSelectorEnabled(false);

			g_UiGadgetRoot.AttachChild(ms_pDebugWindow);
			currentSelectionSlot = -1;
		}

		if (ms_pHDAssetList->UserProcessClick())
		{
 			currentSelectionSlot = ms_pHDAssetList->GetCurrentIndex(); // user clicked, so pick up new slot
 		}

		if (currentSelectionSlot >= ms_HDVehicleInfos.GetCount())
		{
			currentSelectionSlot = ms_HDVehicleInfos.GetCount() - 1;
		}

		ms_pSelectedVehicle = ms_HDVehicleInfos.GetCount() && currentSelectionSlot >= 0 ?  ms_HDVehicleInfos[currentSelectionSlot].pVMI : NULL;

		ms_pHDAssetList->SetNumEntries(ms_HDVehicleInfos.GetCount());	// this invalidates current index in window!		
		if (currentSelectionSlot != -1)
		{
			ms_pHDAssetList->SetCurrentIndex(currentSelectionSlot);
		}

		if (ms_pSelectedVehicle)
		{
			ms_pSelectItemList->SetNumEntries(2);
		}

	}
	else
	{
		if (ms_pDebugWindow)
		{
			ms_pSelectItemList->DetachFromParent();
			ms_pHDAssetList->DetachFromParent();
			ms_pDebugWindow->DetachFromParent();
		}

		delete ms_pSelectItemList;
		ms_pSelectItemList = NULL;

		delete ms_pHDAssetList;
		ms_pHDAssetList = NULL;

		delete ms_pDebugWindow;
		ms_pDebugWindow = NULL;
	}

}

void CVehicleModelInfo::UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 column, void * UNUSED_PARAM( extraCallbackData ) )
{
	if (row >= ms_HDVehicleInfos.GetCount())
	{
		return;
	}

	CVehicleModelInfo *pVI = ms_HDVehicleInfos[row].pVMI;
	if (column == 0)
	{		
		pResult->SetString(pVI->GetModelName());
	}
	else if (column == 1)
	{
		strStreamingInfoManager &infoMgr = strStreamingEngine::GetInfo();

		strIndex strFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(pVI->m_HDfragIdx));
		strIndex strTexIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(pVI->m_HDtxdIdx));

		strStreamingInfo& fragInfo = infoMgr.GetStreamingInfoRef(strFragIndex);
		strStreamingInfo& texInfo = infoMgr.GetStreamingInfoRef(strTexIndex);

		u32 totalPhysicalMemory = fragInfo.ComputePhysicalSize(strFragIndex) + texInfo.ComputePhysicalSize(strTexIndex);

		char sizeInK[16];
		snprintf(sizeInK, 16, "%dK", int((double)totalPhysicalMemory / 1024.0));
		sizeInK[15] = '\0';

		pResult->SetString(sizeInK);
	}
	else //if (column == 2)
	{
		strIndex strFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(pVI->m_HDfragIdx));
		strIndex strTexIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(pVI->m_HDtxdIdx));

		strStreamingInfo& fragInfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(strFragIndex);
		strStreamingInfo& texInfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(strTexIndex);

		u32 totalVirtualMemory  = fragInfo.ComputeVirtualSize(strFragIndex) + texInfo.ComputeVirtualSize(strTexIndex);
		
		char sizeInK[16];
		snprintf(sizeInK, 16, "%dK", int((double)totalVirtualMemory / 1024.0));
		sizeInK[15] = '\0';

		pResult->SetString(sizeInK);
	}
}

void CVehicleModelInfo::UpdateCellForItem(CUiGadgetText* pResult, u32 row, u32 column, void * UNUSED_PARAM(extraCallbackData) )
{
	if (ms_pSelectedVehicle == NULL)
	{
		return;
	}

	if (column == 0)
	{		
		if (row == 0)
		{
			pResult->SetString(g_FragmentStore.GetName(strLocalIndex(ms_pSelectedVehicle->m_HDfragIdx)));
		}
		else
		{
			pResult->SetString(g_TxdStore.GetName(strLocalIndex(ms_pSelectedVehicle->m_HDtxdIdx)));
		}
	}
	else if (column == 1)
	{
		if (row == 0)
		{
			pResult->SetString("FRG");
		}
		else
		{
			pResult->SetString("TXD");
		}
	}
	else if (column == 2 || column == 3)
	{
		strStreamingInfoManager &infoMgr = strStreamingEngine::GetInfo();
		if (row == 0)
		{
			strIndex strFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(ms_pSelectedVehicle->m_HDfragIdx));
			strStreamingInfo& fragInfo = infoMgr.GetStreamingInfoRef(strFragIndex);

			u32 size = column == 2 ? fragInfo.ComputePhysicalSize(strFragIndex) : fragInfo.ComputeVirtualSize(strFragIndex);
			char sizeInK[16];
			snprintf(sizeInK, 16, "%dK", int((double)size / 1024.0));
			sizeInK[15] = '\0';

			pResult->SetString(sizeInK);
		}
		else
		{
			strIndex strTexIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(ms_pSelectedVehicle->m_HDtxdIdx));
			strStreamingInfo& texInfo = infoMgr.GetStreamingInfoRef(strTexIndex);


			u32 size = column == 2 ? texInfo.ComputePhysicalSize(strTexIndex) : texInfo.ComputeVirtualSize(strTexIndex);

			char sizeInK[16];
			snprintf(sizeInK, 16, "%dK", int((double)size / 1024.0));
			sizeInK[15] = '\0';
			pResult->SetString(sizeInK);
		}
	}
	
}

void CVehicleModelInfo::ShowSelectTxdInTexViewCB()
{
	if (ms_pSelectedVehicle)
	{
		CTxdRef	ref(AST_TxdStore, ms_pSelectedVehicle->m_HDtxdIdx, INDEX_NONE, "");
		CDebugTextureViewerInterface::SelectTxd(ref, false, true);
	}
}

void CVehicleModelInfo::GetHDMemoryUsage(u32 &physicalMem, u32 &virtualMem)
{
	physicalMem = 0;
	virtualMem  = 0;

	for (int i = 0; i < ms_HDVehicleInfos.GetCount(); ++i)
	{
		CVehicleModelInfo *pInfo = ms_HDVehicleInfos[i].pVMI;
		strStreamingInfoManager &infoMgr = strStreamingEngine::GetInfo();

		strIndex strFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(pInfo->m_HDfragIdx));
		strStreamingInfo& fragInfo = infoMgr.GetStreamingInfoRef(strFragIndex);

		physicalMem += fragInfo.ComputePhysicalSize(strFragIndex);
		virtualMem  += fragInfo.ComputeVirtualSize(strFragIndex);

		strIndex strTexIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(pInfo->m_HDtxdIdx));
		strStreamingInfo& texInfo = infoMgr.GetStreamingInfoRef(strTexIndex);

		physicalMem += texInfo.ComputePhysicalSize(strTexIndex);
		virtualMem  += texInfo.ComputeVirtualSize(strTexIndex);
	}
}

#endif // __BANK

const Vector3 &CVehicleModelInfo::GetPassengerMobilePhoneIKOffset(u32 iSeat)
{
	for(int i = 0; i < m_aFirstPersonMobilePhoneSeatIKOffset.GetCount(); ++i)
	{
		if(iSeat == (u32)m_aFirstPersonMobilePhoneSeatIKOffset[i].GetSeatIndex())
		{
			return m_aFirstPersonMobilePhoneSeatIKOffset[i].GetOffset();
		}
	}
	return m_FirstPersonPassengerMobilePhoneOffset;
}

// returns the color index as returned by GetPossibleColor
u8 CVehicleModelInfo::GetLiveryColor(s32 livery, s32 color, s32 index) const
{
	if (livery < 0)
		return 255;

	if (color < 0 || color >= MAX_NUM_LIVERY_COLORS)
		return 255;

	if (m_liveryColors[livery][color] < 0 || m_liveryColors[livery][color] >= GetNumPossibleColours())
		return 255;

	Assertf(index < NUM_VEH_BASE_COLOURS, "Invalid color index passed to GetLiveryColor (%d)", index);
	return GetPossibleColours(index, m_liveryColors[livery][color]);
}

// returns the index into the possible colors array
u8 CVehicleModelInfo::GetLiveryColorIndex(s32 livery, s32 color) const
{
	if (livery < 0)
		return 255;

	if (color < 0 || color >= MAX_NUM_LIVERY_COLORS)
		return 255;

	if (m_liveryColors[livery][color] < 0 || m_liveryColors[livery][color] >= GetNumPossibleColours())
		return 255;

	return m_liveryColors[livery][color];
}

//
// Update the colors in the model info
//
void CVehicleModelInfo::UpdateModelColors(const CVehicleVariationData& varData, const int NOTFINAL_ONLY(maxNumCols))
{
	modelinfoAssertf(GetModelType() == MI_TYPE_VEHICLE, "ModelInfo isn't a vehicle");

	m_numPossibleColours = varData.GetNumColors();
	if (m_numPossibleColours == 0) {
		modelinfoWarningf("%s: No possible colors set", varData.GetModelName());
	}
	modelinfoAssertf(m_numPossibleColours <= MAX_VEH_POSSIBLE_COLOURS, "Too many colours for vehicle: %s", varData.GetModelName());

#if __BANK
	// allow us to force all colors to be the same value (for editing)
	const int widgetForcedColor = varData.GetWidgetSelectedColorIndex();	// -1 if no color selected
#endif // __BANK

	u32 numLiveryColors[MAX_NUM_LIVERIES] = {0};
	for(int j=0; j<m_numPossibleColours; j++)
	{
#if __BANK
		const CVehicleModelColorIndices& cols = varData.GetColor(widgetForcedColor>=0 ? widgetForcedColor : j);
#else
		const CVehicleModelColorIndices& cols = varData.GetColor(j);
#endif // __BANK
		m_possibleColours[0][j] = cols.m_indices[0];
		m_possibleColours[1][j] = cols.m_indices[1];
		m_possibleColours[2][j] = cols.m_indices[2];
		m_possibleColours[3][j] = cols.m_indices[3];
		m_possibleColours[4][j] = cols.m_indices[4];
		m_possibleColours[5][j] = cols.m_indices[5];

		for (s32 i = 0; i < MAX_NUM_LIVERIES; ++i)
			if (cols.m_liveries[i] && Verifyf(numLiveryColors[i] < MAX_NUM_LIVERY_COLORS, "Trying to add more than %d colors to livery %d on '%s'", MAX_NUM_LIVERY_COLORS, i, GetModelName()))
				m_liveryColors[i][numLiveryColors[i]++] = (s8)j;
	}	

	m_lightSettings = varData.GetLightSettings();
	m_sirenSettings = varData.GetSirenSettings();

#if !__FINAL
	ASSERT_ONLY(const char *vehName = varData.GetModelName());
	// Repair any fucked up indices
	for(int j=0; j<m_numPossibleColours; j++)
	{
		modelinfoAssertf(m_possibleColours[0][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);
		modelinfoAssertf(m_possibleColours[1][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);
		modelinfoAssertf(m_possibleColours[2][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);
		modelinfoAssertf(m_possibleColours[3][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);
		modelinfoAssertf(m_possibleColours[4][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);
		modelinfoAssertf(m_possibleColours[5][j] < maxNumCols, "carvariations.pso.meta: Bad color index found in '%s', defaulting to index 0", vehName);

		if( m_possibleColours[0][j] > maxNumCols )
			m_possibleColours[0][j] = 0;

		if( m_possibleColours[1][j] > maxNumCols )
			m_possibleColours[1][j] = 0;

		if( m_possibleColours[2][j] > maxNumCols )
			m_possibleColours[2][j] = 0;

		if( m_possibleColours[3][j] > maxNumCols )
			m_possibleColours[3][j] = 0;

		if( m_possibleColours[4][j] > maxNumCols )
			m_possibleColours[4][j] = 0;

		if( m_possibleColours[5][j] > maxNumCols )
			m_possibleColours[5][j] = 0;
	}	

	modelinfoAssertf(m_lightSettings < ms_VehicleColours->m_Lights.GetCount(), "carvariations.pso.meta: Bad light index %d found in '%s', defaulting to index 0", m_lightSettings, vehName);
	if( m_lightSettings >= ms_VehicleColours->m_Lights.GetCount() )
	{
		m_lightSettings = 0;
	}
	

	modelinfoAssertf(m_sirenSettings < ms_VehicleColours->m_Sirens.GetCount(), "carvariations.pso.meta: Bad siren index %d found in '%s', defaulting to index 0", m_sirenSettings, vehName);
	if( m_sirenSettings >= ms_VehicleColours->m_Sirens.GetCount() )
	{
		m_sirenSettings = 0;
	}
#endif // __BANK	
}

void CVehicleModelInfo::UpdateVehicleClassInfo(CVehicleModelInfo* pVehicleModelInfo)	
{
#if !__GAMETOOL
	CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo(pVehicleModelInfo->GetVehicleClass());

    float maxTraction = pHandling->m_fTractionCurveMax;
    if( pHandling->GetCarHandlingData() &&
        pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
    {
        maxTraction += CWheel::ms_fDownforceMult * pHandling->m_fDownforceModifier * 2.0f;
    }

	pVehicleClassInfo->m_fMaxTraction = rage::Max( pVehicleClassInfo->m_fMaxTraction, maxTraction );

	pVehicleModelInfo->CalculateMaxFlatVelocityEstimate(pHandling);
	pVehicleClassInfo->m_fMaxSpeedEstimate = rage::Max(pVehicleClassInfo->m_fMaxSpeedEstimate, pHandling->m_fEstimatedMaxFlatVel);

	if(CFlyingHandlingData *pFlyHandling = pHandling->GetFlyingHandlingData())
	{
		pVehicleClassInfo->m_fMaxBraking = rage::Max(pVehicleClassInfo->m_fMaxBraking, pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY) * pHandling->m_fEstimatedMaxFlatVel * 0.01f);
		pVehicleClassInfo->m_fMaxAcceleration = rage::Max(pVehicleClassInfo->m_fMaxAcceleration, pFlyHandling->m_fThrust * (-GRAVITY));
	}
	else
	{
		pVehicleClassInfo->m_fMaxBraking = rage::Max(pVehicleClassInfo->m_fMaxBraking, pHandling->m_fBrakeForce);
		pVehicleClassInfo->m_fMaxAcceleration = rage::Max(pVehicleClassInfo->m_fMaxAcceleration, pHandling->m_fInitialDriveForce);
	}

	if(pHandling->GetFlyingHandlingData())
	{
		//add yaw pitch and roll together and average them
		pVehicleClassInfo->m_fMaxAgility = rage::Max(pVehicleClassInfo->m_fMaxAgility, 
			((-pHandling->GetFlyingHandlingData()->m_fYawMult) + 
			pHandling->GetFlyingHandlingData()->m_fRollMult + 
			pHandling->GetFlyingHandlingData()->m_fPitchMult) * 
			pHandling->m_fEstimatedMaxFlatVel * (-GRAVITY) / 3.0f);
	}
	else if(CBoatHandlingData *pBoatHandling = pHandling->GetBoatHandlingData())
	{
		pVehicleClassInfo->m_fMaxAgility = rage::Max(pVehicleClassInfo->m_fMaxAgility, pBoatHandling->m_vecMoveResistance.GetXf());
	}

	// HACK to fix issue with trailers weebling when falling over.
	// Modify the COM of the trailer so it doesn't naturally want to self right.
	if(pVehicleModelInfo->GetModelNameHash() == MI_TRAILER_TRAILER.GetName().GetHash())
	{
		pHandling->m_vecCentreOfMassOffset.SetZf(-1.0f);
		if(pHandling->GetTrailerHandlingData())
		{
			pHandling->GetTrailerHandlingData()->m_fUprightSpringConstant = -36.0f;
			pHandling->GetTrailerHandlingData()->m_fUprightDampingConstant = 30.0f;
		}
	}
#else 
	(void)pVehicleModelInfo;
#endif // !__GAMETOOL
}

void CVehicleModelInfo::UpdateModKits(const CVehicleVariationData& varData)
{
	m_modKits.Reset();
	m_modKits.Reserve(varData.GetNumKits());
	for (s32 i = 0; i < varData.GetNumKits(); ++i)
	{
		atHashString kit = varData.GetKit(i);

		for (s32 f = 0; f < ms_VehicleColours->m_Kits.GetCount(); ++f)
		{
			if (ms_VehicleColours->m_Kits[f].GetNameHash() == kit.GetHash())
			{
				m_modKits.Push((u16)f);
				break;
			}
		}
	}
}

void CVehicleModelInfo::UpdateWindowsWithExposedEdges(const CVehicleVariationData& varData)
{
	m_windowsWithExposedEdges = 0;

	if (varData.GetNumWindowsWithExposedEdges() > 0)
	{
		const ObjectNameIdAssociation windows[] =
		{
			{"windscreen",		VEH_WINDSCREEN},
			{"windscreen_r",	VEH_WINDSCREEN_R},
			{"window_lf",		VEH_WINDOW_LF},
			{"window_rf",		VEH_WINDOW_RF},
			{"window_lr",		VEH_WINDOW_LR},
			{"window_rr",		VEH_WINDOW_RR},
			{"window_lm",		VEH_WINDOW_LM},
			{"window_rm",		VEH_WINDOW_RM},
		};

		ASSERT_ONLY(int actualCount = 0;)

		for (int i = 0; i < NELEM(windows); i++)
		{
			const int b = windows[i].hierId - VEH_FIRST_WINDOW;
			Assert(b >= 0 && b < 8);
			const char* hierarchyIdName = windows[i].pName;
			const u32 hierarchyIdNameHash = atStringHash(hierarchyIdName);

			if (varData.GetDoesWindowHaveExposedEdges(hierarchyIdNameHash))
			{
				m_windowsWithExposedEdges |= BIT(b);
				ASSERT_ONLY(actualCount++;)
			}
		}

#if __ASSERT
		const int expectedCount = varData.GetNumWindowsWithExposedEdges();

		if (actualCount < expectedCount)
		{
			Assertf(0, "%s: carvariations.pso.meta indicates %d windows with exposed edges, only %d applied", GetModelName(), expectedCount, actualCount);
		}
#endif // __ASSERT
	}
}

void CVehicleModelInfo::UpdatePlateProbabilities(const CVehicleVariationData& varData)
{
	m_plateProbabilities = varData.GetPlateProbabilities().GetProbabilityArray();
}

void CVehicleModelInfo::GenerateVariation(CVehicle* pVehicle, CVehicleVariationInstance& variation, bool bFullyRandom /*= false*/)
{
	if (m_modKits.GetCount() == 0)
		return;

	Assertf(!GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CANNOT_BE_MODDED), "Setting variation instance on non-modable vehicle '%s'", GetModelName());

	// note, we pick a random kit, and a random mod index if bFullyRandom is set
	u64 modsSelected = 0;
	u32 kitIndex = fwRandom::GetRandomNumberInRange(0, m_modKits.GetCount());
	variation.SetKitIndex(m_modKits[(u8)kitIndex]);

	const CVehicleKit& kit = ms_VehicleColours->m_Kits[m_modKits[(u8)kitIndex]];

	// visible mods
	if (bFullyRandom)
	{
		atFixedArray<atArray<s32>, eVehicleModType::VMT_RENDERABLE> modsMatchingType;

		// Initialize mod array
		for (int i = 0; i < eVehicleModType::VMT_RENDERABLE; ++i)
		{
			atArray<s32> newArray;
			modsMatchingType.Push(newArray);
		}

		// Sort visible mods into array
		for (int i = 0; i < kit.GetVisibleMods().GetCount(); ++i)
		{
			const CVehicleModVisible& mod = kit.GetVisibleMods()[i];

			if (Verifyf(mod.GetType() != VMT_INVALID, "CVehicleModelInfo::GenerateVariation - Visible Mod %d (name='%s') has type VMT_INVALID", i, mod.GetNameHashString().TryGetCStr()))
			{
				modsMatchingType[mod.GetType()].PushAndGrow(i);
			}
		}

		// Pick random mods to assign
		for (int i = 0; i < eVehicleModType::VMT_RENDERABLE; ++i)
		{
			int iNumModsOfType = modsMatchingType[i].GetCount();
			if (iNumModsOfType > 0)
			{
				int iRandomModIndex = fwRandom::GetRandomNumberInRange(0, iNumModsOfType);
				variation.SetModIndex((eVehicleModType)i, (u8)modsMatchingType[i][iRandomModIndex], pVehicle, false);
			}
		}
	}
	else
	{
		for (s32 i = 0; i < kit.GetVisibleMods().GetCount(); ++i)
		{
			const CVehicleModVisible& mod = kit.GetVisibleMods()[i];

			// skip mod slots that have already been filled
			if ((modsSelected & (u64(1) << mod.GetType())) != 0)
				continue;

			//if (fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f)
			//	continue;

			// assign mod to instance and flag slot as filled
			variation.SetModIndex(mod.GetType(), (u8)i, pVehicle, false);
			modsSelected |= u64(1) << mod.GetType();
			Assert(variation.m_numMods <= VMT_MAX); // can't have more than one per slot
		}
	}

	// stat mods
	for (s32 i = 0; i < kit.GetStatMods().GetCount(); ++i)
	{
		const CVehicleModStat& mod = kit.GetStatMods()[i];

		// skip mod slots that have already been filled
		if ((modsSelected & (u64(1) << mod.GetType())) != 0)
			continue;

		if (fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f)
			continue;

		// assign mod to instance and flag slot as filled
		variation.SetModIndex(mod.GetType(), (u8)i, pVehicle, false);
		modsSelected |= u64(1) << mod.GetType();
		Assert(variation.m_numMods <= VMT_MAX); // can't have more than one per slot
	}

    // wheels
    u32 wheelIdx = fwRandom::GetRandomNumberInRange(0, GetVehicleColours()->m_Wheels[GetWheelType()].GetCount());
    variation.SetModIndex(VMT_WHEELS, (u8)wheelIdx, pVehicle, false);
	const u32 modelNameHash = GetModelNameHash();
	if (GetIsBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))
	{
		if(GetIsBike())
		{
			Assertf(GetWheelType()==VWT_BIKE, "Bike '%s' has wrong wheel type!", GetModelName());
		}

		u32 wheelRearIdx = fwRandom::GetRandomNumberInRange(0, GetVehicleColours()->m_Wheels[GetWheelType()].GetCount());
		variation.SetModIndex(VMT_WHEELS_REAR_OR_HYDRAULICS, (u8)wheelRearIdx, pVehicle, false);
	}

	// toggle mods
	variation.ToggleMod(VMT_NITROUS, fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f);
	variation.ToggleMod(VMT_TURBO, fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f);
	variation.ToggleMod(VMT_SUBWOOFER, fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f);
	variation.ToggleMod(VMT_TYRE_SMOKE, fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f);
	variation.ToggleMod(VMT_XENON_LIGHTS, fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f);

	// random smoke color if mod happened to be turned on
	if (variation.IsToggleModOn(VMT_TYRE_SMOKE))
	{
		variation.SetSmokeColorR((u8)fwRandom::GetRandomNumberInRange(0, 255));
		variation.SetSmokeColorG((u8)fwRandom::GetRandomNumberInRange(0, 255));
		variation.SetSmokeColorB((u8)fwRandom::GetRandomNumberInRange(0, 255));
	}

	// random window tint
	variation.SetWindowTint((u8)fwRandom::GetRandomNumberInRange(0, ms_VehicleColours->m_WindowColors.GetCount()));
}

#if __BANK
void CVehicleModelInfo::RefreshVehicleWidgets()
{
	if (ms_VehicleColours)
		ms_VehicleColours->RefreshVehicleWidgets();

	if (ms_VehicleVariations)
		ms_VehicleVariations->RefreshVehicleWidgets();
}

// Update the colors on all active vehicles (use when tuning vehicle colors)
void CVehicleModelInfo::RefreshAllVehicleBodyColors()
{
	CEntityIterator entityIterator( IterateVehicles );
	CEntity* pEntity = entityIterator.GetNext();
	while(pEntity)
	{
		// let shaders know, that body colours changed
		CVehicle * pVehicle = static_cast<CVehicle*>(pEntity);
		pVehicle->UpdateBodyColourRemapping();
		pEntity = entityIterator.GetNext();
	}
}

void CVehicleModelInfo::RefreshAllVehicleLightSettings()
{
	for(int i=0;i<ms_VehicleVariations->GetVariationData().GetCount();i++)
	{
		const CVehicleVariationData& varData = ms_VehicleVariations->GetVariationData()[i];
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(varData.GetModelName(), NULL);
		modelinfoAssertf(pModelInfo, "Vehicle ModelInfo %s doesn't exist", varData.GetModelName());
		if( pModelInfo )
		{
			pModelInfo->UpdateModelColors(varData, 256);
		}
	}
}

void CVehicleModelInfo::SaveVehicleColours(const char* filename)
{
#if !__NO_OUTPUT
	COwnershipInfo<CVehicleKit, VehicleKitId>::DumpInfo();
	COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::DumpInfo();
	COwnershipInfo<sirenSettings, sirenSettingsId>::DumpInfo();
#endif // __NO_OUTPUT

	CVehicleModelInfoVarGlobal* carCols = NULL;
	CVehicleModelInfoVarGlobal carColInst;

	// For the original file, copy over all the carcol info.
	fwPsoStoreLoadInstance tempInstance;
	if(!stricmp(filename, RS_ASSETS "/titleupdate/dev_ng/data/carcols.pso.meta"))
	{
		fwPsoStoreLoader colLoader = fwPsoStoreLoader::MakeSimpleFileLoader<CVehicleModelInfoVarGlobal>();
		colLoader.Load("platform:/data/carcols." META_FILE_EXT, tempInstance);
		carCols = reinterpret_cast<CVehicleModelInfoVarGlobal*>(tempInstance.GetInstance());
	}
	else
	{
		carCols = &carColInst;
	}

	carCols->m_Kits.Reset();
	carCols->m_Lights.Reset();
	carCols->m_Sirens.Reset();

	// Fill it up depending on ownership info.
	for(int j=0; j<ms_VehicleColours->m_Kits.GetCount(); ++j)
	{
		if(COwnershipInfo<CVehicleKit, VehicleKitId>::Found(ms_VehicleColours->m_Kits[j].GetId(), filename))
			carCols->m_Kits.PushAndGrow(ms_VehicleColours->m_Kits[j]);
	}
	for(int j=0; j<ms_VehicleColours->m_Lights.GetCount(); ++j)
	{
		if(COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Found(ms_VehicleColours->m_Lights[j].GetId(), filename))
			carCols->m_Lights.PushAndGrow(ms_VehicleColours->m_Lights[j]);
	}
	for(int j=0; j<ms_VehicleColours->m_Sirens.GetCount(); ++j)
	{
		if(COwnershipInfo<sirenSettings, sirenSettingsId>::Found(ms_VehicleColours->m_Sirens[j].GetId(), filename))
			carCols->m_Sirens.PushAndGrow(ms_VehicleColours->m_Sirens[j]);
	}

	// Save to corresponding file.
	const fiDevice *device = fiDevice::GetDevice(filename);
	if(Verifyf(device, "Couldn't get device for %s", filename))
	{
		char path[RAGE_MAX_PATH];
		device->FixRelativeName(path, RAGE_MAX_PATH, filename);
		vehicleVerifyf(PARSER.SaveObject(path, "meta", carCols, parManager::XML), "Failed to save carcols. Please add extracontent for %s in your command line", device->GetDebugName());
	}
}

void CVehicleModelInfo::ReloadColourMetadata()
{
	COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Reset();
	COwnershipInfo<CVehicleKit, VehicleKitId>::Reset();
	COwnershipInfo<sirenSettings, sirenSettingsId>::Reset();
	LoadVehicleColours();
	for(int i=0; i<ms_VehicleColours->GetCarColFileCount(); ++i)
	{
		AppendVehicleColors(ms_VehicleColours->GetCarColFile(i));
	}
}

void CVehicleModelInfo::SaveVehicleVariations()
{
	ms_VehicleVariations->OnPreSave();

	AssertVerify(PARSER.SaveObject(RS_ASSETS "/export/data/carvariations.pso.meta", "", ms_VehicleVariations, parManager::XML));
}
#endif // __BANK

#if __DEV
void CVehicleModelInfo::CheckMissingColour() 
{
	if(GetNumPossibleColours() == 0)
	{
		modelinfoWarningf( "%s Doesn't have valid colours set up in carcols.pso.meta\n", GetModelName());
	}
}
#endif	// __DEV

#if __BANK
void CVehicleModelInfo::CheckDependenciesAreLoaded()
{
	if (!GetHasLoaded())
		return;

	strIndex deps[STREAMING_MAX_DEPENDENCIES];
	s32 numDeps = CModelInfo::GetStreamingModule()->GetDependencies(GetStreamSlot(), &deps[0], STREAMING_MAX_DEPENDENCIES);
	for (s32 i = 0; i < numDeps; ++i)
	{
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(deps[i]);
		if (info.GetStatus() != STRINFO_LOADED)
		{
			Assertf(false, "Dependency '%s' (%s) for vehicle isn't loaded!", strStreamingEngine::GetObjectName(deps[i]), strStreamingInfo::GetFriendlyStatusName(info.GetStatus()));
		}
	}
}

void CVehicleModelInfo::DumpVehicleModelInfos()
{
	fwModelId modelId = CModelInfo::GetModelIdFromName(GetModelName());
	Displayf("%s, %d refs, %s %s", GetModelName(), GetNumRefs(), ( (modelId.IsValid() && CModelInfo::GetAssetRequiredFlag(modelId))? "required" : ""), (GetHasLoaded()? "(loaded)" : ""));
	if(GetHasLoaded())
	{
		ms_numLoadedInfos++;
	}	
}

atArray<sDebugSize> CVehicleModelInfo::ms_vehicleSizes;
void CVehicleModelInfo::DumpAverageVehicleSize()
{
	fwModelId modelId = CModelInfo::GetModelIdFromName(GetModelName());
	if (modelId.IsValid())
	{
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
			CStreaming::LoadAllRequestedObjects();
		}
	}

    u32 virtualSize  = 0;
    u32 physicalSize = 0;
    CModelInfo::GetObjectAndDependenciesSizes(CModelInfo::GetModelIdFromName(GetModelName()), virtualSize, physicalSize, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount(), true);

    u32 virtualSizeHd = 0;
    u32 physicalSizeHd = 0;
    if (GetHDTxdIndex() != -1)
    {
        virtualSizeHd = CStreaming::GetObjectVirtualSize(strLocalIndex(GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
        physicalSizeHd = CStreaming::GetObjectPhysicalSize(strLocalIndex(GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
    }
    if (GetHDFragmentIndex() != -1)
    {
        virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
        physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
    }

    sDebugSize data;
    data.main = virtualSize;
    data.vram = physicalSize;
    data.mainHd = virtualSizeHd;
    data.vramHd = physicalSizeHd;
    data.name = GetModelName();

    ms_vehicleSizes.PushAndGrow(data);

	CModelInfo::RemoveAssets(modelId);
}

void CVehicleModelInfo::DebugRecalculateVehicleCoverPoints()
{
	if( m_data )
	{
		CalculateVehicleCoverPoints();
	}
}

#endif // __BANK


//
//
//
//
void CVehicleModelInfo::InitMasterDrawableData(u32 UNUSED_PARAM(modelIdx))
{
	Assertf(0, "%s: called InitMasterDrawableData, is this vehicle not a frag?", GetModelName());
}

//
//
//
void CVehicleModelInfo::DeleteMasterDrawableData()
{
}

//
//
//
//
void CVehicleModelInfo::DestroyCustomShaderEffects()
{
	modelinfoAssert(m_data);
	if(m_data->m_pHDShaderEffectType)
	{
		gtaFragType* pFrag = GetHDFragType();
		if(pFrag)
		{
			// check txds are set as expected
			vehicleAssertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());

			m_data->m_pHDShaderEffectType->RestoreModelInfoDrawable(pFrag->GetCommonDrawable());
		}
		m_data->m_pHDShaderEffectType->RemoveRef();
		m_data->m_pHDShaderEffectType = NULL;
	}

	CBaseModelInfo::DestroyCustomShaderEffects();
}

void CVehicleModelInfo::InitMasterFragData(u32 modelIdx)
{
	modelinfoAssert(!m_data);
	m_data = rage_new CVehicleModelInfoData();

	CBaseModelInfo::InitMasterFragData(modelIdx);
	ConfirmHDFiles();
	InitVehData(modelIdx);
	InitPhys();

	// the rope textures are loaded already loaded since they should be a dependency for this modelinfo
	// but we need to call this function so the ropedata gets patched up correctly
	if (m_bNeedsRopeTexture)
	{
		ropeDataManager::LoadRopeTexures();
	}

	s8* skelToRenderBoneMap = NULL;
	u8 numBones = 0;
	gtaFragType* frag = GetFragType();
	if (frag)
	{
		const crSkeletonData* lodSkelData = frag->GetCommonDrawable()->GetSkeletonData();
		numBones = (u8)lodSkelData->GetNumBones();
		skelToRenderBoneMap = rage_new s8[numBones];
		memset(skelToRenderBoneMap, 0xff, numBones * sizeof(s8));

		atArray<int> skinnedBones;
		skinnedBones.Reserve(lodSkelData->GetNumBones() + NUM_VEH_CWHEELS_MAX);
		for (s32 i = 0; i < lodSkelData->GetNumBones(); ++i)
		{
			const crBoneData* boneData = lodSkelData->GetBoneData(i);
			if (boneData->HasDofs(crBoneData::IS_SKINNED))
            {
                s32 boneIndex = boneData->GetIndex();
                Assert(boneIndex < lodSkelData->GetNumBones());
                skelToRenderBoneMap[boneIndex] = (s8)skinnedBones.GetCount();
				skinnedBones.Push(i);
            }
		}

        if (skinnedBones.GetCount() > 0)
        {
            // we have all skinned bones we need for the vehicle, append the bones needed for the wheels
            u32 numWheels = 0;
            fragType* frag = GetFragType();
            if (frag && frag->GetPhysics(0))
            {
                fragTypeChild** children = frag->GetPhysics(0)->GetAllChildren();
                if (children)
                {
                    for (s32 i = 0; i < NUM_VEH_CWHEELS_MAX; ++i)
                    {
                        int nThisChild = GetStructure()->m_nWheelInstances[i][0];
                        int nDrawChild = GetStructure()->m_nWheelInstances[i][1];
                        if (nThisChild > -1 && nDrawChild > -1)
                        {
                            const fragTypeChild* child = children[nThisChild];
                            if (child)
                            {
                                s32 boneIndex = frag->GetBoneIndexFromID(child->GetBoneID());
                                if (boneIndex != -1)
                                {
                                    Assert(boneIndex < lodSkelData->GetNumBones());
                                    skelToRenderBoneMap[boneIndex] = (s8)skinnedBones.GetCount();
                                    skinnedBones.Push(boneIndex);
                                    numWheels++;
                                }
                            }
                        }
                    }
                }
            }

            Assertf(numWheels < NUM_VEH_CWHEELS_MAX, "Found %d skinned wheels in '%s', max %d!", numWheels, GetModelName(), NUM_VEH_CWHEELS_MAX);
            GetStructure()->m_numSkinnedWheels = numWheels;

            InitLodSkeletonMap(lodSkelData, skinnedBones);
        }
        else
        {
            delete[] skelToRenderBoneMap;
            skelToRenderBoneMap = NULL;
            numBones = 0;
        }
	}

	// set up the flags for the vehicle bones that want decals applied
	SetDecalBoneFlags();
	delete[] skelToRenderBoneMap;
}

void CVehicleModelInfo::DeleteMasterFragData()
{
    if (!m_data) // InitMasterFragData hasn't been called yet, can happen when game is shut down
        return;

	modelinfoAssert(GetNumHdRefs() == 0);

	if(!Verifyf(!m_data->m_pHDShaderEffectType, "HD shader effect type not cleaned when removing vehicle '%s'", GetModelName()))
	{
		gtaFragType* pFrag = GetHDFragType();
		if(pFrag)
		{
			// check txds are set as expected
			vehicleAssertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());

			m_data->m_pHDShaderEffectType->RestoreModelInfoDrawable(pFrag->GetCommonDrawable());
		}
		m_data->m_pHDShaderEffectType->RemoveRef();
		m_data->m_pHDShaderEffectType = NULL;
	}

	if(m_data->m_pStructure)
	{
		delete m_data->m_pStructure;
		m_data->m_pStructure = NULL;
	}

	if (m_bNeedsRopeTexture)
	{
		ropeDataManager::UnloadRopeTexures();
	}

	CBaseModelInfo::DeleteMasterFragData();
	m_data->m_WheelOffsets.Reset();

	delete m_data;
	m_data = NULL;
}

void CVehicleModelInfo::SetPhysics(phArchetypeDamp* pPhysics)
{
	CBaseModelInfo::SetPhysics(pPhysics);
	InitPhys();
}

void CVehicleModelInfo::Init(const InitData& rInitData)
{
	Init();

	strLocalIndex vehiclePatchSlot = g_TxdStore.FindSlot("vehshare_tire");
	if (vehiclePatchSlot == -1)
	{
		vehiclePatchSlot = CVehicleModelInfo::GetCommonTxd();
	}

	strLocalIndex parentTxdSlot = strLocalIndex(FindTxdSlotIndex(rInitData.m_txdName.c_str()));
	if(parentTxdSlot == -1)
	{
		// This model has no individual texture dictionary
		// so just set it to use the shared one
		parentTxdSlot = vehiclePatchSlot;
	}
	// setup vehicle texture dictionaries so that they look in the vehshare.txd. If a parent is already setup don't override it
	else if (g_TxdStore.GetParentTxdSlot(parentTxdSlot) == -1)
	{

		g_TxdStore.SetParentTxdSlot(parentTxdSlot, vehiclePatchSlot);
	}

	SetDrawableOrFragmentFile(rInitData.m_modelName.c_str(), parentTxdSlot.Get(), true);			// known to be a fragment

	// only setup the expressions if there is a dictionary name and an expression name.
	if (stricmp(rInitData.m_expressionDictName.c_str(), "null") && stricmp(rInitData.m_expressionName.c_str(), "null"))
	{
		SetExpressionDictionaryIndex(rInitData.m_expressionDictName.c_str());
		SetExpressionHash(rInitData.m_expressionName.c_str());
	}

	// setup the animation dictionary
	if (rInitData.m_animConvRoofDictName.c_str() && stricmp(rInitData.m_animConvRoofDictName.c_str(), "null"))
	{
		SetClipDictionaryIndex(rInitData.m_animConvRoofDictName.c_str());

        // Is the current slot valid and is the animation dictionary in the image?
        vehicleAssertf(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(GetClipDictionaryIndex())) && 
                        g_ClipDictionaryStore.IsObjectInImage(GetClipDictionaryIndex()), "Animation dictionary %s not found", rInitData.m_animConvRoofDictName.c_str() );

		if(stricmp(rInitData.m_animConvRoofName.c_str(), "null"))
		{
			//convert the name into a hash
			u32 uAnimationConvertibleRoofName = atStringHash(rInitData.m_animConvRoofName.c_str());
			SetConvertibleRoofAnimNameHash(uAnimationConvertibleRoofName);
		}
	}

	SetVehicleMetaDataFile(rInitData.m_modelName.c_str());

#if NAVMESH_EXPORT
	if(!CNavMeshDataExporter::WillExportCollision())
#endif
	{
		// setup the particle effect asset
		if (stricmp(rInitData.m_ptfxAssetName.c_str(), "null"))
		{
			strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atStringHash(rInitData.m_ptfxAssetName.c_str()));
			if (Verifyf(slot.IsValid(), "%s is trying to load a PTFX asset (%s) which doesn't exist", GetModelName(), rInitData.m_ptfxAssetName.c_str()))
			{
				SetPtFxAssetSlot(slot.Get());
			}
		}
	}

	u32 uHash = rInitData.m_layout.GetHash();
	m_pVehicleLayoutInfo = (uHash != 0) ? CVehicleMetadataMgr::GetInstance().GetVehicleLayoutInfo(uHash) : NULL;
	u32 uPOVHash = rInitData.m_POVTuningInfo.GetHash();
	m_pPOVTuningInfo = (uHash != 0) ? CVehicleMetadataMgr::GetInstance().GetPOVTuningInfo(uPOVHash) : NULL;
	u32 uCoverHash = rInitData.m_coverBoundOffsets.GetHash();
	m_pVehicleCoverBoundOffsetInfo = (uCoverHash != 0) ? CVehicleMetadataMgr::GetInstance().GetVehicleCoverBoundOffsetInfo(uCoverHash) : NULL;
	
	for(int i = 0; i < rInitData.m_firstPersonDrivebyData.GetCount(); ++i)
	{
		const CFirstPersonDriveByLookAroundData *pDrivebyData = CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundData(rInitData.m_firstPersonDrivebyData[i]);
		vehicleAssertf(pDrivebyData, "%s is trying to reference an invalid driveby data! %s", GetModelName(), rInitData.m_firstPersonDrivebyData[i].GetCStr());
		m_apFirstPersonLookAroundData.PushAndGrow(pDrivebyData);
	}

	// remove underscores from game name. Before using it
	u32 idx = 0;
	char gameName[MAX_VEHICLE_GAME_NAME] = { 0 };
	const char* pChar = rInitData.m_gameName.c_str();
	while(*pChar)
	{
		if(*pChar == '_')
			gameName[idx] = ' ';
		else
			gameName[idx] = *pChar;
		idx++;
		pChar++;

		// Stop if we have filled up the buffer, to avoid an array overrun and
		// possibly corrupting the stack. The name is already silently truncated to
		// eight characters by SetGameName().
		if(idx >= sizeof(gameName) - 1)
		{
			break;
		}
	}
	SetGameName(gameName);
	SetVehicleMakeName(rInitData.m_vehicleMakeName.c_str());

	SetAudioNameHash(rInitData.m_audioNameHash.GetHash());

	SetVehicleType(rInitData.m_type);
	Assertf((m_vehicleType > VEHICLE_TYPE_NONE) && (m_vehicleType < VEHICLE_TYPE_NUM), "Bad vehicle type on '%s'", GetModelName());


	// Setup the explosion info (after the type and name are setup)
	u32 uExplosionInfoHash = rInitData.m_explosionInfo.GetHash();
	if(uExplosionInfoHash == ATSTRINGHASH("EXPLOSION_INFO_DEFAULT",0x13416fdd))
	{
		// If the user left this vehicle type as the default, set it to the vehicle type's default
		switch(GetVehicleType())
		{
		case VEHICLE_TYPE_CAR:						uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_CAR",0xee911a2b); break;
		case VEHICLE_TYPE_PLANE:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_PLANE",0x0062ce5d); break;
		case VEHICLE_TYPE_TRAILER:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_TRAILER", 0xe5253930); break;
		case VEHICLE_TYPE_QUADBIKE:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_QUADBIKE", 0x8aa04604); break;
		case VEHICLE_TYPE_DRAFT:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_DRAFT", 0x5b987700); break;
		case VEHICLE_TYPE_SUBMARINECAR:				uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_CAR", 0xee911a2b); break; // JUST USE THE CAR EXPLOSION INFO FOR THE SUBMARINE CAR
		case VEHICLE_TYPE_HELI:						uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_HELI", 0x493dff90); break;
		case VEHICLE_TYPE_BLIMP:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_BLIMP", 0x030c2852); break;
		case VEHICLE_TYPE_AUTOGYRO:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_AUTOGYRO", 0xd74d8ad7); break;
		case VEHICLE_TYPE_BIKE:						uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_BIKE", 0xd1d17df8); break;
		case VEHICLE_TYPE_BICYCLE:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_BICYCLE", 0x9ee8b39d); break;
		case VEHICLE_TYPE_BOAT:						uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_BOAT", 0x4ce74c96); break;
		case VEHICLE_TYPE_TRAIN:					uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_TRAIN", 0xeb8b34f5); break;
		case VEHICLE_TYPE_SUBMARINE:				uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_SUBMARINE", 0xad524f8e); break;
		case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:	uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_CAR", 0xee911a2b); break;
		case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:		uExplosionInfoHash = ATSTRINGHASH("EXPLOSION_INFO_QUADBIKE", 0x8aa04604); break;
		default: Assertf(false, "Unknown vehicle type %i for '%s'", GetVehicleType(), GetGameName());
		}
	}
	m_pVehicleExplosionInfo = (uExplosionInfoHash != 0) ? CVehicleMetadataMgr::GetInstance().GetVehicleExplosionInfo(uExplosionInfoHash) : NULL;

	SetSteeringWheelMult(rInitData.m_steerWheelMult);
	SetFirstPersonSteeringWheelMult(rInitData.m_firstPersonSteerWheelMult);

	SetWheels(rInitData.m_wheelScale, rInitData.m_wheelScaleRear);

	// Set handling ID
	BANK_ONLY(SetHandlingIdHash(atHashString(rInitData.m_handlingId));)
	s32 handlingId = CHandlingDataMgr::RegisterHandlingData(rInitData.m_handlingId, true);
	Assertf(handlingId != -1, "Invalid car handling ID");
	SetHandlingId(handlingId);

	SetDefaultDirtLevel(u32(rInitData.m_dirtLevelMin * 15.0f), u32(rInitData.m_dirtLevelMax * 15.0f));

	Assertf(rInitData.m_envEffScaleMin <= rInitData.m_envEffScaleMax, "%s: envEffScale: Min must be smaller than max in vehicles.meta.", rInitData.m_modelName.c_str());
	SetEnvEffScale(rInitData.m_envEffScaleMin, rInitData.m_envEffScaleMax);

	Assertf(rInitData.m_envEffScaleMin2 <= rInitData.m_envEffScaleMax2, "%s: envEffScale2: Min must be smaller than max in vehicles.meta.", rInitData.m_modelName.c_str());
	SetEnvEffScale2(rInitData.m_envEffScaleMin2, rInitData.m_envEffScaleMax2);

	SetDamageMapScale(rInitData.m_damageMapScale);
	SetDamageOffsetScale(rInitData.m_damageOffsetScale);

	SetDiffuseTint(rInitData.m_diffuseTint);

	// set flags
	SetVehicleFreq(rInitData.m_frequency);
	SetVehicleMaxNumber((u16) rInitData.m_maxNum);
	SetVehicleSwankness((eSwankness) rInitData.m_swankness);

    m_maxNumOfSameColor = rInitData.m_maxNumOfSameColor;
	m_defaultBodyHealth = rInitData.m_defaultBodyHealth;
	
    m_VehicleFlags = rInitData.m_flags;

	SetCameraNameHash(rInitData.m_cameraName.GetHash());
	SetAimCameraNameHash(rInitData.m_aimCameraName.GetHash());
	SetBonnetCameraNameHash(rInitData.m_bonnetCameraName.GetHash());
	SetPovTurretCameraNameHash(rInitData.m_povTurretCameraName.GetHash());

	SetFirstPersonDriveByIKOffset(rInitData.m_FirstPersonDriveByIKOffset);
	SetFirstPersonDriveByUnarmedIKOffset(rInitData.m_FirstPersonDriveByUnarmedIKOffset);
	SetFirstPersonDriveByLeftPassengerIKOffset(rInitData.m_FirstPersonDriveByLeftPassengerIKOffset);
	SetFirstPersonDriveByRightPassengerIKOffset(rInitData.m_FirstPersonDriveByRightPassengerIKOffset);
	SetFirstPersonDriveByRightRearPassengerIKOffset(rInitData.m_FirstPersonDriveByRightRearPassengerIKOffset);
	SetFirstPersonDriveByLeftPassengerUnarmedIKOffset(rInitData.m_FirstPersonDriveByLeftPassengerUnarmedIKOffset);
	SetFirstPersonDriveByRightPassengerUnarmedIKOffset(rInitData.m_FirstPersonDriveByRightPassengerUnarmedIKOffset);
	SetFirstPersonProjectileDriveByIKOffset(rInitData.m_FirstPersonProjectileDriveByIKOffset);
	SetFirstPersonProjectileDriveByPassengerIKOffset(rInitData.m_FirstPersonProjectileDriveByPassengerIKOffset);
	SetFirstPersonProjectileDriveByRearLeftIKOffset(rInitData.m_FirstPersonProjectileDriveByRearLeftIKOffset);
	SetFirstPersonProjectileDriveByRearRightIKOffset(rInitData.m_FirstPersonProjectileDriveByRearRightIKOffset);
	SetFirstPersonVisorSwitchIKOffset(rInitData.m_FirstPersonVisorSwitchIKOffset);
	SetFirstPersonMobilePhoneOffset(rInitData.m_FirstPersonMobilePhoneOffset);
	SetFirstPersonPassengerMobilePhoneOffset(rInitData.m_FirstPersonPassengerMobilePhoneOffset);

	m_aFirstPersonMobilePhoneSeatIKOffset = rInitData.m_FirstPersonMobilePhoneSeatIKOffset;

	SetPovCameraNameHash(rInitData.m_povCameraName.GetHash());
	SetPovCameraOffset(rInitData.m_PovCameraOffset);
	SetPovPassengerCameraOffset(rInitData.m_PovPassengerCameraOffset);
	SetPovRearPassengerCameraOffset(rInitData.m_PovRearPassengerCameraOffset);
	SetPovCameraVerticalAdjustmentForRollCage(rInitData.m_PovCameraVerticalAdjustmentForRollCage);
	m_shouldUseCinematicViewMode = rInitData.m_shouldUseCinematicViewMode;
	m_bShouldCameraTransitionOnClimbUpDown = rInitData.m_shouldCameraTransitionOnClimbUpDown;
	m_bShouldCameraIgnoreExiting = rInitData.m_shouldCameraIgnoreExiting;
	m_bAllowPretendOccupants = rInitData.m_AllowPretendOccupants;
	m_bAllowJoyriding = rInitData.m_AllowJoyriding;
	m_bAllowSundayDriving = rInitData.m_AllowSundayDriving;
	m_bAllowBodyColorMapping = rInitData.m_AllowBodyColorMapping;

	m_pretendOccupantsScale = rInitData.m_pretendOccupantsScale;
	m_visibleSpawnDistScale = rInitData.m_visibleSpawnDistScale;

	for (s32 i = 0; i < VLT_MAX; ++i)
		m_lodDistances[i] = rInitData.m_lodDistances[i];

	// compensate for missing the VLT_LOD3 distance - 05/05/2014

	for (int i = 0; i < VLT_LOD_FADE; ++i)
	{
		Assertf(m_lodDistances[i] > 0.0f, "Unspecified lod distance for vehicle %s (index %d)", gameName, i);
	}

	if (m_lodDistances[VLT_LOD_FADE] == 0.0f)
	{
		m_lodDistances[VLT_LOD_FADE] = m_lodDistances[VLT_LOD3];
	}

	SetLodDistance(GetVehicleLodDistance(VLT_LOD_FADE));

	if (rInitData.m_lodDistances[VLT_HD] > 0.f){
		SetupHDFiles(rInitData.m_modelName);
	}

	m_identicalModelSpawnDistance = rInitData.m_identicalModelSpawnDistance;
	
	m_trackerPathWidth = rInitData.m_trackerPathWidth;

	m_weaponForceMult = rInitData.m_weaponForceMult;

	m_trailers = rInitData.m_trailers;
	m_additionalTrailers = rInitData.m_additionalTrailers;
	
	m_drivers = rInitData.m_drivers;
	m_vfxExtraInfos = rInitData.m_vfxExtraInfos;

	if(rInitData.m_doorsWithCollisionWhenClosed.GetCount()>0 || rInitData.m_driveableDoors.GetCount()>0)
	{
		CVehicleModelInfoDoors* pDoorExtension = GetExtension<CVehicleModelInfoDoors>();
		if(!pDoorExtension)
		{
			CVehicleModelInfoDoors *pNewDoorExtension = rage_new CVehicleModelInfoDoors();
			GetExtensionList().Add(*pNewDoorExtension);
			pDoorExtension = GetExtension<CVehicleModelInfoDoors>();
		}
		Assert(pDoorExtension);

		for(u32 i=0; i < rInitData.m_doorsWithCollisionWhenClosed.GetCount(); ++i)
		{
			eDoorId doorId = rInitData.m_doorsWithCollisionWhenClosed[i];
			pDoorExtension->AddDoorWithCollisionId(pDoorExtension->ConvertExtensionIdToHierarchyId(doorId));
		}

        for(u32 i=0; i < rInitData.m_driveableDoors.GetCount(); ++i)
        {
            eDoorId doorId = rInitData.m_driveableDoors[i];
            pDoorExtension->AddDriveableDoorId(pDoorExtension->ConvertExtensionIdToHierarchyId(doorId));
        }

		for(u32 i=0; i < rInitData.m_doorStiffnessMultipliers.GetCount(); ++i)
		{
			CDoorStiffnessInfo rDoorInfo = rInitData.m_doorStiffnessMultipliers[i];
			eDoorId doorId = rDoorInfo.GetDoorId();
			pDoorExtension->AddStiffnessMultForThisDoorId(pDoorExtension->ConvertExtensionIdToHierarchyId(doorId), rDoorInfo.GetStiffnessMult());
		}
	}

	if(rInitData.m_animConvRoofWindowsAffected.GetCount()>0)
	{
		CConvertibleRoofWindowInfo* pWindowExtension = GetExtension<CConvertibleRoofWindowInfo>();
		if(!pWindowExtension)
		{
			CConvertibleRoofWindowInfo *pNewWindowExtension = rage_new CConvertibleRoofWindowInfo();
			GetExtensionList().Add(*pNewWindowExtension);
			pWindowExtension = GetExtension<CConvertibleRoofWindowInfo>();
		}
		Assert(pWindowExtension);
		Assertf(rInitData.m_animConvRoofWindowsAffected.GetCount()<=VEH_EXT_NUM_WINDOWS,
			"Too many windows (%d) listed as affected by convertible roof anim in vehicles.meta for %s. Max allowed = %d",
			rInitData.m_animConvRoofWindowsAffected.GetCount(),	GetModelName(), VEH_EXT_NUM_WINDOWS);

		for(u32 i=0; i < rInitData.m_animConvRoofWindowsAffected.GetCount(); ++i)
		{
			eWindowId nWindowId = rInitData.m_animConvRoofWindowsAffected[i];
			Assertf(pWindowExtension->ConvertExtensionIdToHierarchyId(nWindowId) >= VEH_FIRST_WINDOW
				&& pWindowExtension->ConvertExtensionIdToHierarchyId(nWindowId) <= VEH_LAST_WINDOW,
				"Invalid window ID (%d) for vehicle %s in vehicles.meta", nWindowId, GetModelName());
			pWindowExtension->AddWindowId(pWindowExtension->ConvertExtensionIdToHierarchyId(nWindowId));
		}
	}

	if (rInitData.m_pOverrideRagdollThreshold!=NULL)
	{
		CVehicleModelInfoRagdollActivation* pExtension = GetExtension<CVehicleModelInfoRagdollActivation>();
		if(!pExtension)
		{
			CVehicleModelInfoRagdollActivation *pNewExtension = rage_new CVehicleModelInfoRagdollActivation(rInitData.m_pOverrideRagdollThreshold->m_MinComponent, rInitData.m_pOverrideRagdollThreshold->m_MaxComponent, rInitData.m_pOverrideRagdollThreshold->m_ThresholdMult);
			GetExtensionList().Add(*pNewExtension);
			pExtension = GetExtension<CVehicleModelInfoRagdollActivation>();
		}
		Assert(pExtension);
	}

	if(rInitData.m_buoyancySphereOffset.IsNonZero() || rInitData.m_buoyancySphereSizeScale != 1.0f
		|| rInitData.m_additionalVfxWaterSamples.GetCount() > 0)
	{
		CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
		if(!pBuoyancyExtension)
		{
			CVehicleModelInfoBuoyancy* pNewBuoyancyExtension = rage_new CVehicleModelInfoBuoyancy();
			GetExtensionList().Add(*pNewBuoyancyExtension);
			pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
		}
		Assert(pBuoyancyExtension);

		pBuoyancyExtension->SetBuoyancySphereOffset(rInitData.m_buoyancySphereOffset);
		pBuoyancyExtension->SetBuoyancySphereSizeScale(rInitData.m_buoyancySphereSizeScale);

		pBuoyancyExtension->SetAdditionalVfxWaterSamples(rInitData.m_additionalVfxWaterSamples);
	}

	if(rInitData.m_bumpersNeedToCollideWithMap)
	{
		CVehicleModelInfoBumperCollision* pBumperColExtension = GetExtension<CVehicleModelInfoBumperCollision>();
		if(!pBumperColExtension)
		{
			CVehicleModelInfoBumperCollision* pNewBumperColExtension = rage_new CVehicleModelInfoBumperCollision();
			GetExtensionList().Add(*pNewBumperColExtension);
			pBumperColExtension = GetExtension<CVehicleModelInfoBumperCollision>();
		}
		Assert(pBumperColExtension);

		pBumperColExtension->SetBumpersNeedMapCollision(rInitData.m_bumpersNeedToCollideWithMap);
	}

	m_bNeedsRopeTexture = rInitData.m_needsRopeTexture;

    m_extraIncludes = rInitData.m_extraIncludes;
    m_requiredExtras = rInitData.m_requiredExtras;

    m_wheelType = rInitData.m_wheelType;
	
	//Copy the spawn points to the base model info.
	m_uVehicleScenarioLayoutInfoHash = rInitData.m_scenarioLayout;

	const CVehicleScenarioLayoutInfo* pScenarioLayoutInfo = GetVehicleScenarioLayoutInfo();

	if ( pScenarioLayoutInfo != NULL )
	{
		u32 numScenarioPoints = pScenarioLayoutInfo->GetNumScenarioPoints();
		SetNum2dEffects(numScenarioPoints);

		for(u32 i = 0; i < numScenarioPoints; ++i)
		{
			//Allocate the spawn point.
			C2dEffect* pEffect = CModelInfo::GetSpawnPointStore().CreateItem(fwFactory::GLOBAL);
			CSpawnPoint* pSpawnPoint = pEffect->GetSpawnPoint();

			//Initialize the spawn point from the extension.
			pSpawnPoint->InitArchetypeExtensionFromDefinition(pScenarioLayoutInfo->GetScenarioPointInfo(i), NULL);

			//Add the spawn point.
			C2dEffect** ppEffect = GetNewEffect();
			*ppEffect = pEffect;
			Add2dEffect(ppEffect);
		}
	}

	SetVfxInfo(rInitData.m_vfxInfoName.GetHash());

	m_HDTextureDist = rInitData.m_HDTextureDist;

	m_rewards = rInitData.m_rewards;
	m_cinematicPartCamera = rInitData.m_cinematicPartCamera;

	m_NmBraceOverrideSet = rInitData.m_NmBraceOverrideSet;

    Assertf((rInitData.m_plateType & ~0x3) == 0, "Enum value for vehicle plate type is too large");
    m_plateType = (u8)(rInitData.m_plateType & 0x3);

    Assertf((rInitData.m_vehicleClass & ~0x1f) == 0, "Enum value for vehicle class is too large");
    m_vehicleClass = (u8)(rInitData.m_vehicleClass & 0x1f);

    m_dashboardType = rInitData.m_dashboardType;

	m_maxSteeringWheelAngle = rInitData.m_maxSteeringWheelAngle;
	m_maxSteeringWheelAnimAngle = rInitData.m_maxSteeringWheelAnimAngle;

	m_minSeatHeight = rInitData.m_minSeatHeight;

	m_lockOnPositionOffset = rInitData.m_lockOnPositionOffset;

	m_LowriderArmWindowHeight = rInitData.m_LowriderArmWindowHeight;
	m_LowriderLeanAccelModifier = rInitData.m_LowriderLeanAccelModifier;

	m_numSeatsOverride = rInitData.m_numSeatsOverride;
}

void CVehicleModelInfo::ReInit(const InitData& rInitData)
{
	// FA: we could potentially do a full init here (everything we do in Init(InitData)) but I've chosen to only do the requested
	// items first. It should be trivial to add most things if needed but might not be 100% safe to do them all blindly so we can deal
	// with those cases when needed.

	for (s32 i = 0; i < VLT_MAX; ++i)
		m_lodDistances[i] = rInitData.m_lodDistances[i];

	SetLodDistance(GetVehicleLodDistance(VLT_LOD_FADE));

	m_identicalModelSpawnDistance = rInitData.m_identicalModelSpawnDistance;

	m_trailers = rInitData.m_trailers;
	m_additionalTrailers = rInitData.m_additionalTrailers;
	m_drivers = rInitData.m_drivers;

	m_HDTextureDist = rInitData.m_HDTextureDist;
}

#if __BANK
void CVehicleModelInfo::SetVehicleDLCPack(const char* fileName)
{
	char pString[128];
	strcpy(pString, fileName);
	const char* pToken = strtok(pString, ":");
	if (pToken)
	{
		m_dlcpack = atHashString(pToken);
	}
}
#endif //__BANK

void CVehicleModelInfo::SetVehicleMetaDataFile(const char* UNUSED_PARAM(pName))
{
#if 0
	modelinfoAssert(pName);
	if (stricmp(pName, "null") == 0)
	{
		m_vehicleMetaDataFileIndex = -1;
		return;
	}

	m_vehicleMetaDataFileIndex = g_fwMetaDataStore.FindSlot(pName);
#endif
}

const CVehicleModelInfoMetaData* CVehicleModelInfo::GetVehicleMetaData() const
{
	strLocalIndex index = strLocalIndex(GetVehicleMetaDataFileIndex());
	if (index == -1)
		return NULL;

	if (!g_fwMetaDataStore.Get(index))
		return NULL;

	return g_fwMetaDataStore.Get(index)->GetObject<CVehicleModelInfoMetaData>();
}

void CVehicleModelInfo::RequestDials(const sVehicleDashboardData& params)
{
#if !__GAMETOOL
	fwModelId modelId = fwArchetypeManager::LookupModelId(this);

	if(!Verifyf(ms_requestPerFrame == 0,"Ignorable - Requesting too many Dials in one frame, the limit is ONE"))
	{
		return;
	}
	ms_requestPerFrame++;
    // link render target
    if (ms_dialsRenderTargetId == 0)
	{
        ms_rtNameHash = m_data->m_dialsTextureHash;
		const CRenderTargetMgr::namedRendertarget* rt = gRenderTargetMgr.GetNamedRendertarget(ms_rtNameHash, ms_dialsRenderTargetOwner);

        if (rt == NULL)
        {
            gRenderTargetMgr.RegisterNamedRendertarget(ms_rtNameHash, ms_dialsRenderTargetOwner, m_data->m_dialsRenderTargetUniqueId);
        }
#if RSG_PC && GTA_REPLAY
		else
		{
			const bool bReplayActive = CReplayMgr::IsPlaying() || CReplayMgr::IsEditModeActive();			
			if((GRCDEVICE.UsingMultipleGPUs() && bReplayActive) && rt->owner == ms_dialsRenderTargetOwner && rt->isLinked && rt->release)
			{
				return;
			}
		}
#endif

        if (GetDrawable() && !gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), ms_dialsRenderTargetOwner))
        {
            gRenderTargetMgr.LinkNamedRendertargets(m_data->m_dialsTextureHash, ms_dialsRenderTargetOwner, m_data->m_dialsTexture, modelId.GetModelIndex(), GetDrawableType(), GetFragmentIndex());
        }

		CRenderTargetMgr::namedRendertarget* pRenderTarget = gRenderTargetMgr.GetNamedRendertarget(ms_rtNameHash, ms_dialsRenderTargetOwner);
		if (pRenderTarget && pRenderTarget->texture)
		{
			ms_dialsRenderTargetSizeX = (u32)pRenderTarget->texture->GetWidth();
			ms_dialsRenderTargetSizeY = (u32)pRenderTarget->texture->GetHeight();
            ms_dialsRenderTargetId = pRenderTarget->uniqueId;
		}
        Assert(ms_dialsRenderTargetId != 0);
	}
	else
	{
        // these asserts trigger when char switching from one vehicle to the other. i.e. the old vehicle doesn't have enough time to free the dials
        // we can ignore this and there'll be a few frames delay
        //Assertf(gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), ms_dialsRenderTargetOwner), "%s requesting dials but render target isn't linked!", GetModelName());
        //Assert(ms_rtNameHash == m_data->m_dialsTextureHash);
	}

	if (ms_rtNameHash == m_data->m_dialsTextureHash)
	{
		ms_dialsFrameCountReq = fwTimer::GetFrameCount();
	}


    s32 requestedIndex = -1;
    switch (GetVehicleDashboardType())
	{
	case VDT_MAVERICK:  // aircraft
        requestedIndex = DIALS_AIRCRAFT;
        break;
	case VDT_LAZER:     // lazer jet
        requestedIndex = DIALS_LAZER;
        break;
	case VDT_LAZER_VINTAGE:
		requestedIndex = DIALS_LAZER_VINTAGE;
		break;
	case VDT_PBUS2:
		requestedIndex = DIALS_PBUS_2;
		break;
	default:            // all other vehicles
        requestedIndex = DIALS_VEHICLE;
        break;
	}

	// if we want a different movie, release old one here. the correct one will be allocated when Update gets called
    if (requestedIndex != ms_activeDialsMovie)
	{
		if (CScaleformMgr::IsMovieActive(ms_dialsMovieId))
		{
			CScaleformMgr::RequestRemoveMovie(ms_dialsMovieId);

			ms_dialsMovieId = -1;
			ms_dialsType = -1;
		}

		ms_activeDialsMovie = requestedIndex;
	}

    if (CScaleformMgr::IsMovieActive(ms_dialsMovieId))
	{
		bool bDialsUpdated = false;

        if (ms_dialsType == -1)
		{
            ms_dialsType = (s32)GetVehicleDashboardType();
			if (CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_VEHICLE_TYPE"))
			{
				CScaleformMgr::AddParamInt(ms_dialsType);
				CScaleformMgr::EndMethod();
			}
		}

        if (ms_activeDialsMovie == DIALS_VEHICLE)
		{
			if (ms_cachedDashboardData.HaveVehicleDialsChanged(params) && CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_DASHBOARD_DIALS"))
			{
				CScaleformMgr::AddParamFloat(params.revs);
				CScaleformMgr::AddParamFloat(params.speed);
				CScaleformMgr::AddParamFloat(params.fuel);
				CScaleformMgr::AddParamFloat(params.engineTemp);
				CScaleformMgr::AddParamFloat(params.vacuum);
				CScaleformMgr::AddParamFloat(params.boost);
				CScaleformMgr::AddParamFloat(params.oilTemperature);
				CScaleformMgr::AddParamFloat(params.oilPressure);
				CScaleformMgr::AddParamFloat(params.currentGear);
				CScaleformMgr::EndMethod();
				bDialsUpdated = true;
			}
			if (ms_cachedDashboardData.HaveVehicleLightsChanged(params) && CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_DASHBOARD_LIGHTS"))
			{
				CScaleformMgr::AddParamBool(params.indicatorLeft);
				CScaleformMgr::AddParamBool(params.indicatorRight);
				CScaleformMgr::AddParamBool(params.handBrakeLight);
				CScaleformMgr::AddParamBool(params.engineLight);
				CScaleformMgr::AddParamBool(params.absLight);
				CScaleformMgr::AddParamBool(params.fuelLight);
				CScaleformMgr::AddParamBool(params.oilLight);
				CScaleformMgr::AddParamBool(params.headLightsLight);
				CScaleformMgr::AddParamBool(params.fullBeamLight);
				CScaleformMgr::AddParamBool(params.batteryLight);
				CScaleformMgr::EndMethod();
				bDialsUpdated = true;
			}

            // radio
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				const char* stationStr = "replay";

				if ( stationStr != ms_lastStationStr )
				{
					if (CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_RADIO"))
					{
						CScaleformMgr::AddParamString(""); // tuning
						CScaleformMgr::AddParamString("");
						CScaleformMgr::AddParamString("");
						CScaleformMgr::AddParamString("");

						CScaleformMgr::EndMethod();
					}
					ms_lastStationStr = stationStr;
				}
			}
			else
#endif
			{
				u32 trackId = 1;
				WIN32PC_ONLY(bool isUserTrack = false);
				const audRadioStation *playerStation = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
				if(playerStation)
				{
					const audRadioTrack &track = playerStation->GetCurrentTrack();
					if(track.GetCategory() == RADIO_TRACK_CAT_MUSIC || track.GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC)
					{
						trackId = track.GetTextId();
						WIN32PC_ONLY(isUserTrack = track.IsUserTrack());
					}
				}
			

				const char* stationName = g_RadioAudioEntity.GetPlayerRadioStationNamePendingRetune();
				bool radioActive = stationName && (g_RadioAudioEntity.IsPlayerRadioActive() || g_RadioAudioEntity.IsRetuningVehicleRadio());
				const char* stationStr = radioActive ? stationName : (stationName ? "" : "CAR_RADOFF");

				if( !TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT) )
					TheText.RequestAdditionalText("TRACKID", RADIO_WHEEL_TEXT_SLOT);

				if ((trackId != ms_lastTrackId || stationStr != ms_lastStationStr) && TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT))
				{
					if (CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_RADIO"))
					{
	#define FILTER_UNKNOWN(inVal) ( inVal&&TheText.DoesTextLabelExist(inVal)?TheText.Get(inVal):"")
						char artistStr[16] = {0};
						char trackStr[16] = {0};

						if (radioActive)
						{
							formatf(artistStr, "%dA", trackId);
							formatf(trackStr, "%dS", trackId);
						}

						CScaleformMgr::AddParamString(""); // tuning
						CScaleformMgr::AddParamString(FILTER_UNKNOWN(stationStr));

	#if RSG_PC
						if(isUserTrack)
						{
							CScaleformMgr::AddParamString(audRadioStation::GetUserRadioTrackManager()->GetTrackArtist(audRadioTrack::GetUserTrackIndexFromTextId(trackId)));
							CScaleformMgr::AddParamString(audRadioStation::GetUserRadioTrackManager()->GetTrackTitle(audRadioTrack::GetUserTrackIndexFromTextId(trackId)));
						}
						else
						{
	#endif		
						CScaleformMgr::AddParamString(FILTER_UNKNOWN(artistStr));
						CScaleformMgr::AddParamString(FILTER_UNKNOWN(trackStr));
	#if RSG_PC
						}
	#endif // RSG_PC
						CScaleformMgr::EndMethod();
					}
					ms_lastTrackId = trackId;
					ms_lastStationStr = stationStr;
				}
			}

		}
		else if (ms_activeDialsMovie == DIALS_AIRCRAFT || DIALS_LAZER || DIALS_LAZER_VINTAGE)
		{
			if (ms_cachedDashboardData.HaveAircraftDialsChanged(params) && CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_DASHBOARD_DIALS"))
			{
				CScaleformMgr::AddParamFloat(params.aircraftFuel);
				CScaleformMgr::AddParamFloat(params.aircraftTemp);
				CScaleformMgr::AddParamFloat(params.aircraftOilPressure);
				CScaleformMgr::AddParamFloat(params.aircraftBattery);
				CScaleformMgr::AddParamFloat(params.aircraftFuelPressure);
				CScaleformMgr::AddParamFloat(params.aircraftAirSpeed);
				CScaleformMgr::AddParamFloat(params.aircraftVerticalSpeed);
				CScaleformMgr::AddParamFloat(params.aircraftCompass);
				CScaleformMgr::AddParamFloat(params.aircraftRoll);
				CScaleformMgr::AddParamFloat(params.aircraftPitch);
				CScaleformMgr::AddParamFloat(params.aircraftAltitudeSmall);
				CScaleformMgr::AddParamFloat(params.aircraftAltitudeLarge);
				CScaleformMgr::EndMethod();
				bDialsUpdated = true;
			}
			if (ms_cachedDashboardData.HaveAircraftLightsChanged(params) && CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_DASHBOARD_LIGHTS"))
			{
				CScaleformMgr::AddParamBool(params.aircraftGearUp);
				CScaleformMgr::AddParamBool(params.aircraftGearDown);
				CScaleformMgr::AddParamBool(params.pressureAlarm);
				CScaleformMgr::EndMethod();
				bDialsUpdated = true;
			}

            if (ms_activeDialsMovie == DIALS_LAZER || DIALS_LAZER_VINTAGE)
			{
				if (ms_cachedDashboardData.HaveAircraftHudDialsChanged(params) && CScaleformMgr::BeginMethod(ms_dialsMovieId, SF_BASE_CLASS_SCRIPT, "SET_AIRCRAFT_HUD"))
				{
					CScaleformMgr::AddParamFloat(params.aircraftAir);
					CScaleformMgr::AddParamFloat(params.aircraftFuel);
					CScaleformMgr::AddParamFloat(params.aircraftOil);
					CScaleformMgr::AddParamFloat(params.aircraftVacuum);
					CScaleformMgr::EndMethod();
					bDialsUpdated = true;
				}

				CPed* pPlayer = FindPlayerPed();
				if( pPlayer && pPlayer->GetHelmetComponent())
				{
					const u32 LAZER_COCKPIT_ROCKETS = (u32)-199376390;
					const u32 LAZER_COCKPIT_MACHINE = 1931187857;
					u32 uWeaponReticuleHash = CNewHud::GetReticule().GetPreviousValues().m_iWeaponHashForReticule;
					if( (uWeaponReticuleHash == LAZER_COCKPIT_ROCKETS || uWeaponReticuleHash == LAZER_COCKPIT_MACHINE) &&
						pPlayer->GetHelmetComponent()->HasPilotHelmetEquippedInAircraftInFPS(false))
					{
						if(ms_cachedDashboardData.HaveAircraftHudDialsChanged(params) && CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_AIRCRAFT_HUD"))
						{
							CScaleformMgr::AddParamFloat(params.aircraftPitch);
							CScaleformMgr::AddParamFloat(params.aircraftRoll);
							CScaleformMgr::AddParamFloat(params.aircraftAltitudeLarge);
							CScaleformMgr::AddParamFloat(params.aircraftAirSpeed);
							CScaleformMgr::EndMethod();
							bDialsUpdated = true;
						}
					}
				}
			}
		}
		else if(ms_activeDialsMovie == DIALS_PBUS_2)
		{
			// TODO: Fill in the stuff
		}

		if(bDialsUpdated)
		{
			ms_cachedDashboardData = params;
		}

	}
#else
	(void)params;
#endif
}

void CVehicleModelInfo::RenderDialsToRenderTarget(u32 targetId)
{
#if !__GAMETOOL
    if ( (ms_dialsRenderTargetId == 0) || (ms_dialsRenderTargetId != targetId) || (!CScaleformMgr::IsMovieActive(ms_dialsMovieId)) || (ms_activeDialsMovie == -1) )
	{
        return;
	}

	Vector2 vSize(1.0f, 1.0f);
	if (ms_dialsRenderTargetSizeX > 0 && ms_dialsRenderTargetSizeY > 0 && CScaleformMgr::GetScreenSize().x > 0 && CScaleformMgr::GetScreenSize().y > 0)
	{
		vSize.x = (float)ms_dialsRenderTargetSizeX / CScaleformMgr::GetScreenSize().x;
		vSize.y = (float)ms_dialsRenderTargetSizeY / CScaleformMgr::GetScreenSize().y;
	}

	// set the pos & size of the movie:
	CScaleformMgr::ChangeMovieParams(ms_dialsMovieId, Vector2(0.f, 0.f), vSize, GFxMovieView::SM_ExactFit);

	CScaleformMgr::RenderMovie(ms_dialsMovieId, fwTimer::GetSystemTimeStep());
#else
	(void)targetId;
#endif
}

void CVehicleModelInfo::SetBoneIndexes(ObjectNameIdAssociation *pAssocArray, u16* pNameHashes, bool bOverRide)
{
	modelinfoAssert(m_data);
	crSkeletonData *pSkeletonData = NULL;
	if(GetFragType())
		pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
	else
		pSkeletonData = GetDrawable()->GetSkeletonData();

	if (pSkeletonData)
	{
		s32 i=0;
		u16 rootHash = atHash16U("chassis");
		while(pAssocArray[i].pName != NULL)
		{
			const crBoneData *pBone = NULL;

            // for some reason the exporter allows the root bone (the chassis) to have a boneId of 0 in the skeleton
			s32 boneIdx = -1;
            if (pNameHashes[i] == rootHash)
                boneIdx = 0;
            else
                pSkeletonData->ConvertBoneIdToIndex(pNameHashes[i], boneIdx);

            if (boneIdx != -1)
			{
				pBone = pSkeletonData->GetBoneData(boneIdx);
			}
			
#if __ASSERT
			const crBoneData* bone2 = pSkeletonData->FindBoneData(pAssocArray[i].pName);
			if( pBone != bone2 )
			{
				Displayf( "Vehicle (%s) bone name clash: bones '%s' and '%s' have hashes %d and %d", GetModelName(), pAssocArray[i].pName, pBone ? pBone->GetName() : "null" , atHash16U(pAssocArray[i].pName), atHash16U(pBone ? pBone->GetName() : "null"));
			}
#endif // __ASSERT

			if (pBone)
			{
				m_data->m_pStructure->m_nBoneIndices[pAssocArray[i].hierId] = (s8)pBone->GetIndex();
#if __ASSERT
				// check we don't have any light bones at the centre of the vehicle
				if(pAssocArray[i].hierId >= VEH_HEADLIGHT_L && pAssocArray[i].hierId <= VEH_SIREN_GLASS_MAX && pAssocArray[i].hierId != VEH_EMISSIVES)
					modelinfoAssertf(RCC_VECTOR3(pBone->GetDefaultTranslation()).IsNonZero(), "%s bone %s is at zero", GetModelName(), pBone->GetName());
#endif
			}
			
			i++;
		}

		// only want to set up light stuff once - using base ids
		if(!bOverRide)
		{
			// Now that's done, we need to deal with remaping the missing light bones.
			const s32 tailLightL = GetBoneIndex(VEH_TAILLIGHT_L);
			const s32 BrakeLightL = GetBoneIndex(VEH_BRAKELIGHT_L);
			const s32 IndicatorLR = GetBoneIndex(VEH_INDICATOR_LR);
			
			const s32 tailLightR = GetBoneIndex(VEH_TAILLIGHT_R);
			const s32 BrakeLightR = GetBoneIndex(VEH_BRAKELIGHT_R);
			const s32 IndicatorRR = GetBoneIndex(VEH_INDICATOR_RR);

			const bool gotTail = (-1 != tailLightL) || (-1 != tailLightR);
			const bool gotBrake = (-1 != BrakeLightL) || (-1 != BrakeLightR);
			const bool gotIndicator = (-1 != IndicatorLR) || (-1 != IndicatorRR);

			m_bRemap = true;
			m_bHasTailLight = gotTail;
			m_bHasIndicatorLight = gotIndicator;
			m_bHasBrakeLight = gotBrake;

			if( (false == gotTail) || 
				(false == gotBrake) ||
				(false == gotIndicator) )
			{	
				s32 remapToL = -1;
				s32 remapToR = -1;
				
				// something's missing, we need to remap
				// Select where to remap
				if( gotTail )
				{
					remapToL = tailLightL;
					remapToR = tailLightR;
				}
				else if ( gotBrake )
				{
					remapToL = BrakeLightL;
					remapToR = BrakeLightR;
				}
				else if ( gotIndicator )
				{
					remapToL = IndicatorLR;
					remapToR = IndicatorRR;
				}
				
				// We can't fail remapping... or can we ?
				// modelinfoAssert(remapToL != -1);
				// modelinfoAssert(remapToR != -1);

				if( false == gotTail )
				{
					m_data->m_pStructure->m_nBoneIndices[VEH_TAILLIGHT_L] = (s8)remapToL;
					m_data->m_pStructure->m_nBoneIndices[VEH_TAILLIGHT_R] = (s8)remapToR;
				}

				if( false == gotBrake )
				{
					m_data->m_pStructure->m_nBoneIndices[VEH_BRAKELIGHT_L] = (s8)remapToL;
					m_data->m_pStructure->m_nBoneIndices[VEH_BRAKELIGHT_R] = (s8)remapToR;
				}

				if( false == gotIndicator )
				{
					m_data->m_pStructure->m_nBoneIndices[VEH_INDICATOR_LR] = (s8)remapToL;
					m_data->m_pStructure->m_nBoneIndices[VEH_INDICATOR_RR] = (s8)remapToR;
				}
			}
			
			// Setup extra lights
			const s32 extraLight1 = GetBoneIndex(VEH_EXTRALIGHT_1);
			const s32 extraLight2 = GetBoneIndex(VEH_EXTRALIGHT_2);
			const s32 extraLight3 = GetBoneIndex(VEH_EXTRALIGHT_3);
			const s32 extraLight4 = GetBoneIndex(VEH_EXTRALIGHT_4);

			m_bHasExtraLights = (extraLight1 != -1) ||
								(extraLight2 != -1) ||
								(extraLight3 != -1) || 
								(extraLight4 != -1);
			
			const s32 extraLightCount = ((extraLight1 != -1) ? 1 : 0) +
										((extraLight2 != -1) ? 1 : 0) +
										((extraLight3 != -1) ? 1 : 0) +
										((extraLight4 != -1) ? 1 : 0);
			
			m_bDoubleExtralights = (extraLightCount > 2);

			// Setup neons
			const s32 neonL = GetBoneIndex(VEH_NEON_L);
			const s32 neonR = GetBoneIndex(VEH_NEON_R);
			const s32 neonF = GetBoneIndex(VEH_NEON_F);
			const s32 neonB = GetBoneIndex(VEH_NEON_B);

			m_bHasNeons = (neonL != -1) ||
						  (neonR != -1) ||
						  (neonF != -1) || 
						  (neonB != -1);
		}
	}

	m_bHasSteeringWheelBone = (GetBoneIndex(VEH_STEERING_WHEEL) != -1);
}

void CVehicleModelInfo::SetDecalBoneFlags()
{
	modelinfoAssert(m_data);
	// go through the vehicle hierarchy ids
	for (int i=0; i<VEH_NUM_NODES; i++)
	{
		// get the bone index of this hierarchy id
		int boneIndex = m_data->m_pStructure->m_nBoneIndices[i];

		// check if there is a valid bone
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				// don't project onto chassis bones
				if (i<=VEH_CHASSIS_DUMMY)
				{
					m_data->m_pStructure->m_decalBoneFlags.Clear(boneIndex);
				}
				// don't project onto door handle, wheel, suspension, transmission, wheel hub or window bones
				else if (i>=VEH_HANDLE_DSIDE_F && i<=VEH_LAST_WINDOW)
				{	
					m_data->m_pStructure->m_decalBoneFlags.Clear(boneIndex);
				}
				// don't project onto exhaust, engine, petrol, steering wheel, grip or breakable light bones
				else if (i>=VEH_EXHAUST && i<=VEH_LASTBREAKABLELIGHT)
				{	
					m_data->m_pStructure->m_decalBoneFlags.Clear(boneIndex);
				}
				// don't project onto interior lights or siren bones
				else if (i>=VEH_INTERIORLIGHT && i<=VEH_SIREN_GLASS_MAX)
				{	
					m_data->m_pStructure->m_decalBoneFlags.Clear(boneIndex);
				}
				// don't project onto spring bones
				else if (i>=VEH_SPRING_RF && i<=VEH_SPRING_LR)
				{	
					m_data->m_pStructure->m_decalBoneFlags.Clear(boneIndex);
				}
			}
		}
	}

	// deal with exceptions to the general case
	if (GetIsBike())
	{
		// project onto these 
		int boneIndex = m_data->m_pStructure->m_nBoneIndices[BIKE_SWINGARM];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[BIKE_FORKS_U];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[BIKE_FORKS_L];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}
	}
	else if (GetIsPlane())
	{
		// project onto these 
		int boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_RL];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_RR];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_FL];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_FR];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_RL1];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_RR1];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_RL2];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}

		boneIndex = m_data->m_pStructure->m_nBoneIndices[LANDING_GEAR_DOOR_RR2];
		if (boneIndex>-1)
		{
			if (Verifyf(boneIndex<VEH_MAX_DECAL_BONE_FLAGS, "out of range decal bone index"))
			{
				m_data->m_pStructure->m_decalBoneFlags.Set(boneIndex);
			}
		}
	}
}

void CVehicleModelInfo::CalculateHeightsAboveRoad(float *pFrontHeight, float *pRearHeight)
{
	if(GetVehicleType()!=VEHICLE_TYPE_BOAT && GetVehicleType()!=VEHICLE_TYPE_TRAIN && GetVehicleType()!=VEHICLE_TYPE_TRAILER && GetStructure())
	{
		// find handling data for this vehicle
		CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(GetHandlingId());
		CVehicleWeaponHandlingData* pWeaponHandling = pHandling->GetWeaponHandlingData();

		if( GetBoneIndex(VEH_WHEEL_LF) != -1 )
		{
			*pFrontHeight = -CWheel::GetWheelOffset(this, VEH_WHEEL_LF).z + GetTyreRadius(true) + pHandling->m_fSuspensionRaise;
			if(pWeaponHandling)
			{
				// don't add the wheel impact offset to the front wheels on the half-track
				if( !MI_CAR_HALFTRACK.IsValid() ||
					GetModelNameHash() != MI_CAR_HALFTRACK.GetName().GetHash() )
				{
					*pFrontHeight += pWeaponHandling->GetWheelImpactOffset();
				}
				else
				{
					static float halfTrackHack = 0.01f;
					*pFrontHeight += halfTrackHack;
				}
			}
		}
		else if( GetBoneIndex(VEH_WHEEL_RF) != -1 )
		{
			*pFrontHeight = -CWheel::GetWheelOffset(this, VEH_WHEEL_RF).z + GetTyreRadius(true) + pHandling->m_fSuspensionRaise;
			if(pWeaponHandling)
			{
				*pFrontHeight += pWeaponHandling->GetWheelImpactOffset();
			}
		}
		else
		{
			*pFrontHeight = -GetBoundingBoxMin().z;
		}

		if( GetBoneIndex(VEH_WHEEL_LR)!=-1 )
		{
			*pRearHeight = -CWheel::GetWheelOffset(this, VEH_WHEEL_LR).z + GetTyreRadius(false) + pHandling->m_fSuspensionRaise;
			if(pWeaponHandling)
			{
				*pRearHeight += pWeaponHandling->GetWheelImpactOffset();
			}

			// don't add the wheel impact offset to the front wheels on the half-track
			if( MI_CAR_HALFTRACK.IsValid() &&
				GetModelNameHash() == MI_CAR_HALFTRACK.GetName().GetHash() )
			{
				static float halfTrackRearHack = 0.06f;
				*pRearHeight += halfTrackRearHack;
			}
		}
		else if( GetBoneIndex(VEH_WHEEL_RR)!=-1 )
		{
			*pRearHeight = -CWheel::GetWheelOffset(this, VEH_WHEEL_RR).z + GetTyreRadius(false) + pHandling->m_fSuspensionRaise;
			if(pWeaponHandling)
			{
				*pRearHeight += pWeaponHandling->GetWheelImpactOffset();
			}
		}
		else
		{
			*pRearHeight = -GetBoundingBoxMin().z;
		}

		modelinfoAssertf((square(*pFrontHeight) < 4.0f*4.0f && square(*pRearHeight) < 4.0f*4.0f ) || GetIsPlane(), "%s:CalculateHeightsAboveRoad: Vehicle seems too high above road", GetModelName());
	}
	else
	{
		*pFrontHeight = *pRearHeight = -GetBoundingBoxMin().z;
	}
}

//
// name:		CVehicleModelInfo::Init
// description:	Initialise vehicle model info class
void CVehicleModelInfo::Init() 
{
	CBaseModelInfo::Init(); 
	m_vehicleType = VEHICLE_TYPE_NONE; 

	for (s32 i = 0; i < VLT_MAX; ++i)
		m_lodDistances[i] = -1.f;

	m_type = MI_TYPE_VEHICLE;
	m_numPossibleColours = 0;	
	m_gangPopGroup = -1; 

	m_bRemap = false;

	m_wheelBaseMultiplier = 0.0f;

	m_FirstPersonDriveByIKOffset.Zero();
	m_FirstPersonDriveByUnarmedIKOffset.Zero();
	m_FirstPersonDriveByLeftPassengerIKOffset.Zero();
	m_FirstPersonDriveByRightPassengerIKOffset.Zero();
	m_FirstPersonDriveByRightRearPassengerIKOffset.Zero();
	m_FirstPersonDriveByLeftPassengerUnarmedIKOffset.Zero();
	m_FirstPersonDriveByRightPassengerUnarmedIKOffset.Zero();
	m_FirstPersonProjectileDriveByIKOffset.Zero();
	m_FirstPersonProjectileDriveByPassengerIKOffset.Zero();
	m_FirstPersonProjectileDriveByRearLeftIKOffset.Zero();
	m_FirstPersonProjectileDriveByRearRightIKOffset.Zero();
	m_FirstPersonVisorSwitchIKOffset.Zero();

	m_FirstPersonMobilePhoneOffset.Zero();
	m_FirstPersonPassengerMobilePhoneOffset.Zero();
	m_aFirstPersonMobilePhoneSeatIKOffset.clear();

	m_PovCameraOffset.Zero();
	m_PovPassengerCameraOffset.Zero();
	m_PovRearPassengerCameraOffset.Zero();
	m_PovCameraVerticalAdjustmentForRollCage = 0.0f;

	m_uCameraNameHash = 0;
	m_uAimCameraNameHash = 0;
	m_uBonnetCameraNameHash = 0;
	m_uPovCameraNameHash = 0;
	m_uPovTurretCameraNameHash = 0;
	m_audioNameHash = 0;
	m_shouldUseCinematicViewMode = true;
	m_bShouldCameraTransitionOnClimbUpDown = false;
	m_bShouldCameraIgnoreExiting = false;
	
	m_bAllowPretendOccupants = true;
	m_bAllowJoyriding = true;
	m_bAllowSundayDriving = true;

    m_uConvertibleRoofAnimHash = 0;
	m_fSteeringWheelMult = 0.0f;
	m_fFirstPersonSteeringWheelMult = 0.0f;

	m_pVehicleLayoutInfo = NULL;
	m_pPOVTuningInfo = NULL;
	m_pVehicleCoverBoundOffsetInfo = NULL;
	m_pVehicleExplosionInfo = NULL;
	m_uVehicleScenarioLayoutInfoHash = 0;

	m_pretendOccupantsScale = 1.0f;
	m_visibleSpawnDistScale = 1.0f;

	m_bHasSeatCollision = false;

	m_bHasSteeringWheelBone = false;
	m_bNeedsRopeTexture = false;

	m_uNumDependencies = 0;
	m_bDependenciesValid = false;

	// HD files stuff
	m_HDfragIdx = -1;
	m_HDtxdIdx = -1;
	m_bRequestLoadHDFiles = false;
	m_bAreHDFilesLoaded = false;
	m_bRequestHDFilesRemoval = false;
	m_numHDRefs	= 0;
	m_numHDRenderRefs = 0;
	
	// Light Setting
	m_lightSettings = 0;

	m_sirenSettings = 0;

	// vfx info
	m_pVfxInfo = NULL;

	// invalidate livery colors
	for (s32 i = 0; i < MAX_NUM_LIVERIES; ++i)
		for (s32 f = 0; f < MAX_NUM_LIVERY_COLORS; ++f)
			m_liveryColors[i][f] = -1;

	m_windowsWithExposedEdges = 0;

	m_bHasRequestedDrivers = false;

    m_maxNumOfSameColor = 10;
	m_defaultBodyHealth = VEH_DAMAGE_HEALTH_STD;

	SetUseAmbientScale(true);

	m_LastTimeUsed = 0;

    m_data = NULL;
}

//
// name:		CVehicleModelInfo::Shutdown
// description:	Initialise vehicle model info class
void CVehicleModelInfo::Shutdown() 
{
	CBaseModelInfo::Shutdown();
}


#if __DEV
	PARAM(carbones, "Verify all vehicle bone names at load time");
#endif

void CVehicleModelInfo::InitVehData(s32 UNUSED_PARAM(modelIdx))
{
	modelinfoAssert(m_vehicleType != VEHICLE_TYPE_NONE);
	modelinfoAssert(m_data);
	modelinfoAssert(m_data->m_pStructure == NULL);

#if __BANK
	if (CVehicleStructure::m_pInfoPool->GetNoOfFreeSpaces() <= 0)
	{
		s32 numAppropriate = gPopStreaming.GetAppropriateLoadedCars().CountMembers();
		s32 numInappropriate = gPopStreaming.GetInAppropriateLoadedCars().CountMembers();
		s32 numSpecial = gPopStreaming.GetSpecialCars().CountMembers();
		s32 numBoats = gPopStreaming.GetLoadedBoats().CountMembers();
		s32 numDiscarded = gPopStreaming.GetDiscardedCars().CountMembers();
#if GTA_REPLAY
		//s32 numReplay = gPopStreaming.GetReplayCars().CountMembers();
		Displayf("CVehicleModelInfo::InitVehData: %d appropriate, %d inappropriate, %d special, %d boats and %d discarded cars! (%d REPLAY Requested)\n",
			numAppropriate, numInappropriate, numSpecial, numBoats, numDiscarded, numSpecial);
#else
		Displayf("CVehicleModelInfo::InitVehData: %d appropriate, %d inappropriate, %d special, %d boats and %d discarded cars!\n",
			numAppropriate, numInappropriate, numSpecial, numBoats, numDiscarded);
#endif

		CModelInfo::DumpVehicleModelInfos();
	}
#endif // __BANK

	m_data->m_pStructure = rage_new CVehicleStructure;
	Assert(m_data->m_pStructure);

	// set up base set of bone ids - same for all vehicles
	SetBoneIndexes(CVehicleFactory::GetFactory()->GetBaseVehicleTypeDesc(m_vehicleType), CVehicleFactory::GetFactory()->GetBaseVehicleTypeDescHashes(m_vehicleType), false);

	// if extra set of ids is different from base, they override existing ids with these
	if( CVehicleFactory::GetFactory()->GetExtraVehicleTypeDesc(m_vehicleType) != CVehicleFactory::GetFactory()->GetBaseVehicleTypeDesc(m_vehicleType) )
	{
		SetBoneIndexes(CVehicleFactory::GetFactory()->GetExtraVehicleTypeDesc(m_vehicleType), CVehicleFactory::GetFactory()->GetExtraVehicleTypeDescHashes(m_vehicleType), true);
	}
	if( m_vehicleType == VEHICLE_TYPE_HELI ||
		m_vehicleType == VEHICLE_TYPE_PLANE )
	{
		SetBoneIndexes( CVehicleFactory::GetFactory()->GetExtraLandingGearTypeDesc(), CVehicleFactory::GetFactory()->GetExtraLandingGearTypeDescHashes(), true);
	}

#if __DEV
	if(PARAM_carbones.Get())
	{
		crSkeletonData *pSkeletonData = NULL;
		if(GetFragType())
			pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
		else
			pSkeletonData = GetDrawable()->GetSkeletonData();

		if(pSkeletonData)
		{
			for(int nBoneIndex=0; nBoneIndex<pSkeletonData->GetNumBones(); nBoneIndex++)
			{
				bool bFound = false;
				for(int i=0; i<VEH_NUM_NODES; i++)
				{
					if(m_data->m_pStructure->m_nBoneIndices[i] == (s8)nBoneIndex)
					{
						bFound = true;
						break;
					}
				}
				if(!bFound)
				{
					modelinfoAssertf(false, "Vehicle %s Bone %s is not recognised", GetModelName(), pSkeletonData->GetBoneData(nBoneIndex)->GetName());
				}
			}
		}
	}
#endif

	// Setup environment map texture.
	rmcDrawable *drawable = GetDrawable();
	modelinfoAssert(NULL != drawable);

	m_data->m_numAvailableLODs = 0;
	if (drawable)
	{
		if (drawable->GetLodGroup().ContainsLod(LOD_LOW)) {
			m_data->m_numAvailableLODs = 3;
		} else if (drawable->GetLodGroup().ContainsLod(LOD_MED)) {
			m_data->m_numAvailableLODs = 2;
		} else if (drawable->GetLodGroup().ContainsLod(LOD_HIGH)) {
			m_data->m_numAvailableLODs = 1;
		} 
	}

	modelinfoAssertf(m_data->m_numAvailableLODs > 0, "%s can't determine number of available LOD levels", GetModelName());

	m_data->m_liveryCount = -1;
	
	// We got livery/sign texture, so we need to count them...
	// Take the kids away, the following code is a dirtydirtydirty hack.
	if( GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY) )
	{
		grmShaderGroup& shaderGrp = drawable->GetShaderGroup();
		
		const s32 shaderCount = shaderGrp.GetCount();

		// Go through all shaders, and gather what we need..
		for(int i=0; i<shaderCount; i++)
		{
			const grmShader& shader = shaderGrp[i];
			
			const grcEffectVar diffuseTex2 = shader.LookupVar("DiffuseTex2",false);
			if( diffuseTex2 != grcevNONE )
			{
				grcTexture *mainTexture;
				shader.GetVar( diffuseTex2, mainTexture);
				if (modelinfoVerifyf(mainTexture, "livery: mainTexture is NULL on model %s (shader=%s)", GetModelName(), shader.GetName()))
				{
					const char *mainTexName = mainTexture->GetName();
					const char *postFix = strstr(mainTexName,"_sign_");
					if( NULL != postFix )
					{
						u32 nameLen = ptrdiff_t_to_int(postFix - mainTexName);
						modelinfoAssertf(nameLen>0, "Livery Texture name is too short for model %s\n", GetModelName());
						modelinfoAssertf(nameLen<31, "Livery Texture name is too long for model %s\n", GetModelName());
						char prefix[32];
						strncpy(prefix, mainTexName, nameLen); // ho, look! a pointer hack!
						prefix[nameLen] = 0;

						fwTxd *txd = g_TxdStore.Get(strLocalIndex(GetAssetParentTxdIndex()));
						modelinfoAssert(txd);

						char texName[32];
						int signCount = 0;
						sprintf(texName,"%s_sign_%d",prefix,signCount+1);
						grcTexture *texture = txd->Lookup(texName);
						while( texture )
						{
							if(signCount < MAX_NUM_LIVERIES)
							{
								m_data->m_liveries[signCount] = txd->ComputeHash(texName);
								signCount++;
								sprintf(texName,"%s_sign_%d",prefix,signCount+1);
								texture = txd->Lookup(texName);
							}
							else
							{
								modelinfoAssertf(false, "Too many livery textures applied to vehicle %s\n", GetModelName());
								texture = NULL;
							}
						}

						m_data->m_liveryCount = (s8)signCount;

						modelinfoAssert(m_data->m_liveryCount != 0);
						break;
					}
				}
			}// if( diffuseTex2 != grcevNONE )...
		}// for(int i=0; i<shaderCount; i++)...

		for(int i=0; i<shaderCount; i++)
		{
			const grmShader& shader = shaderGrp[i];

			const grcEffectVar diffuseTex3 = shader.LookupVar("DiffuseTex3",false);
			if( diffuseTex3 != grcevNONE )
			{
				grcTexture *mainTexture;
				shader.GetVar( diffuseTex3, mainTexture);
				if (modelinfoVerifyf(mainTexture, "livery2: mainTexture is NULL on model %s (shader=%s)", GetModelName(), shader.GetName()))
				{
					const char *mainTexName = mainTexture->GetName();
					const char *postFix = strstr(mainTexName,"_lvr_");
					if( postFix != NULL )
					{
						u32 nameLen = ptrdiff_t_to_int(postFix - mainTexName);
						modelinfoAssertf(nameLen>0, "Livery Texture name is too short for model %s\n", GetModelName());
						modelinfoAssertf(nameLen<31, "Livery Texture name is too long for model %s\n", GetModelName());
						char prefix[32];
						strncpy(prefix, mainTexName, nameLen); // ho, look! a pointer hack!
						prefix[nameLen] = 0;

						fwTxd *txd = g_TxdStore.Get(strLocalIndex(GetAssetParentTxdIndex()));
						modelinfoAssert(txd);

						char texName[32];
						int lvrCount = 0;
						sprintf(texName,"%s_lvr_%d",prefix,lvrCount+1);
						grcTexture *texture = txd->Lookup(texName);
						while( texture )
						{
							if(lvrCount < MAX_NUM_LIVERIES)
							{
								m_data->m_liveries2[lvrCount] = txd->ComputeHash(texName);
								lvrCount++;
								sprintf(texName,"%s_lvr_%d",prefix,lvrCount+1);
								texture = txd->Lookup(texName);
							}
							else
							{
								modelinfoAssertf(false, "Too many livery2 textures applied to vehicle %s\n", GetModelName());
								texture = NULL;
							}
						}

						m_data->m_livery2Count = (s8)lvrCount;

						modelinfoAssert(m_data->m_livery2Count != 0);
						break;
					}
				}
			}// if( diffuseTex2 != grcevNONE )...

		}//for(int i=0; i<shaderCount; i++)...
	}//if( GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY) )...


    // check if we have a dashboard dials rendertarget
	if (drawable)
	{	
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();
		for (u32 i = 0; i < shaderGroup->GetCount(); ++i)
		{
			grmShader* shader = shaderGroup->GetShaderPtr(i);
			grcEffectVar varId = shader->LookupVar("diffuseTex", false);
			if (varId != grcevNONE)
			{
				char texName[256];
				grcTexture *diffuseTex = NULL;
				shader->GetVar(varId, diffuseTex);

				if (diffuseTex)
				{
					StringNormalize(texName, diffuseTex->GetName(), sizeof(texName));
					fiAssetManager::RemoveExtensionFromPath(texName, sizeof(texName), texName);

					char* scriptRT = strstr(texName, "script_rt_");

					if (scriptRT)
					{
                        const char* rtName = texName + strlen("script_rt_");
						m_data->m_dialsTextureHash.SetFromString(rtName);
                        m_data->m_dialsRenderTargetUniqueId = CRenderTargetMgr::GetRenderTargetUniqueId(rtName, ms_dialsRenderTargetOwner);
                        m_data->m_dialsTexture = diffuseTex;
                        break;
					}
				}
			}
		}
	}

	if (!drawable->GetLodGroup().ContainsLod(1))
	{
		m_lodDistances[VLT_LOD0] = m_lodDistances[VLT_LOD1];
//		m_lodDistances[VLT_LOD1] = m_lodDistances[VLT_LOD_FADE];
	}

	drawable->GetLodGroup().SetLodThresh(0, GetVehicleLodDistance(VLT_LOD0));
	drawable->GetLodGroup().SetLodThresh(1, GetVehicleLodDistance(VLT_LOD1));
	drawable->GetLodGroup().SetLodThresh(2, GetVehicleLodDistance(VLT_LOD2));
	if (drawable->GetLodGroup().ContainsLod(3))
	{
		drawable->GetLodGroup().SetLodThresh(3, GetVehicleLodDistance(VLT_LOD3));
	}
	

	CacheWheelOffsets();

#if __ASSERT
	// make sure we have no embedded textures in the drawables, these would be duplicated for the HD assets unnecessarily
	fragType* frag = GetFragType();
	if (Verifyf(frag, "Frag not loaded for vehicle '%s'", GetModelName()))
	{
		rmcDrawable* commonDrawable = frag->GetCommonDrawable();
		if (commonDrawable)
			Assertf(!commonDrawable->GetShaderGroup().GetTexDict(), "Common drawable for vehicle '%s' has a texture dictionary!", GetModelName());
		
		rmcDrawable* clothDrawable = frag->GetClothDrawable();
		if (clothDrawable)
			Assertf(!clothDrawable->GetShaderGroup().GetTexDict(), "Cloth drawable for vehicle '%s' has a texture dictionary!", GetModelName());
	}
#endif // __ASSERT
}


//-------------------------------------------------------------------------
// Returns the information on the coverpoint passed
//-------------------------------------------------------------------------
CCoverPointInfo* CVehicleModelInfo::GetCoverPoint( s32 iCoverPoint )
{
	modelinfoAssert(m_data);
	modelinfoAssert( iCoverPoint >= 0 && iCoverPoint < m_data->m_iCoverPoints );

	return &m_data->m_aCoverPoints[iCoverPoint];
}

// #define DISTANCE_FROM_COVER_TO_VEHICLE_HIGH (0.1f)
// #define DISTANCE_FROM_COVER_TO_VEHICLE_LOW (0.1f)
#define DISTANCE_FROM_COVER_TO_CORNER_DEFAULT (0.7f)
#define DISTANCE_FROM_COVER_TO_CORNER_SIDE_FRONT (1.0f)

#define VEHICLE_LENGTH_FOR_MID_COVERPOINTS (2.0f)

#define NUM_VEH_COVER_INTERSECTIONS (18)
static phIntersection aCoverIntersections[NUM_VEH_COVER_INTERSECTIONS];

//
// name:		CalculateVehicleCoverPoints
// description:	Calculates a set of cover points for a vehicle collision model
void CVehicleModelInfo::CalculateVehicleCoverPoints( void )
{
	modelinfoAssert(m_data);
	m_data->m_iCoverPoints = 0;

	if (GetIsPlane() || GetIsRotaryAircraft())
		return;

	phArchetype *pArchetype = GetPhysics();
	if(GetFragType())
		pArchetype = GetFragType()->GetPhysics(0)->GetArchetype();

	// there's the potential for the physics not being there
	if(pArchetype==NULL)
		return;

	// Create a test instance of the vehicles collision model at the origin
	phInstGta* pTestInst = rage_new phInstGta(PH_INST_MAPCOL);
	Matrix34 mat;
	mat.Identity();
	pTestInst->Init(*pArchetype, mat);

	// Only use the chassis to test collision, ignoring any destroyable bits we 
	// can shoot through like the police car's lights
	phBound* pCarTestBound = pTestInst->GetArchetype()->GetBound();
	phBoundComposite* pCompositeBound = dynamic_cast<phBoundComposite*>(pTestInst->GetArchetype()->GetBound());
	if(pCompositeBound)
		pCarTestBound = pCompositeBound->GetBound(0);

	float fCapsuleRadius = 1.0f;
	float fCapsuleLength = 1.0f;
	Vector3 avCapsulePositions[CVehicleModelInfoData::HEIGHT_MAP_SIZE];

	// Size capsule tests
	SizeCoverTestForVehicle(pTestInst, avCapsulePositions, fCapsuleRadius, fCapsuleLength);

	phShapeTest<phShapeSweptSphere> capsuleTester;
	phSegment segment;
	phIntersection isectResult;
	float afHeights[CVehicleModelInfoData::HEIGHT_MAP_SIZE] = {-999.0f, -999.0f, -999.0f, -999.0f, -999.0f, -999.0f};

	for(int i=0; i<CVehicleModelInfoData::HEIGHT_MAP_SIZE; i++)
	{
		segment.Set(avCapsulePositions[i] + 0.5f*fCapsuleLength*ZAXIS, avCapsulePositions[i] - 0.5f*fCapsuleLength*ZAXIS);
		capsuleTester.InitSweptSphere(segment, fCapsuleRadius, &isectResult, 1);

		if(capsuleTester.TestOneObject(*pCarTestBound))
		{
			float fHeight = isectResult.GetPosition().GetZf() - GetBoundingBoxMin().z;
			if( fHeight > afHeights[i])
			{
				// As the base of the car is unlikely to be at the origin, use the minimum bounding box to get the true height
				afHeights[i] = fHeight;
			}
		}
	}

	// remove the vehicle collision instance;
	delete pTestInst;
	pTestInst = NULL;

	// Store the height map in the model
	for( s32 i=0; i<CVehicleModelInfoData::HEIGHT_MAP_SIZE; i++ )
	{
		m_data->m_afHeightMap[i] = afHeights[i];
	}

	// Compute the bounding box min and max
	Vector3 vBoundingMin, vBoundingMax;
	vBoundingMin = GetBoundingBoxMin();
	vBoundingMax = GetBoundingBoxMax();
	
	// If vehicle is flagged to use coverbound info for cover generation
	if( GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_COVERBOUND_INFO_FOR_COVERGEN) )
	{
		// Check if the vehicle has specified cover bounds to use instead of the model info bounding box
		const CVehicleCoverBoundOffsetInfo* pCoverBoundOffsetInfo = GetVehicleCoverBoundOffsetInfo();
		if( pCoverBoundOffsetInfo )
		{
			const atArray<CVehicleCoverBoundInfo>& coverBoundsInfoArray = pCoverBoundOffsetInfo->GetCoverBoundInfoArray();
			const int iCoverBoundCount = coverBoundsInfoArray.GetCount();
			// If there is a list of bounds to use
			if( iCoverBoundCount > 0 )
			{
				// initialize the mins at zeros and push them out using the list of bounds
				vBoundingMin.Zero();
				vBoundingMax.Zero();

				// traverse the list of cover bounds, updating the effective bounding min and max values
				for(int i=0; i < iCoverBoundCount; i++)
				{
					const CVehicleCoverBoundInfo& rCoverBoundInfo = coverBoundsInfoArray[i];
					const Vec3V vMin = VECTOR3_TO_VEC3V(rCoverBoundInfo.m_Position) + (- Vec3V(rCoverBoundInfo.m_Width * 0.5f, rCoverBoundInfo.m_Length * 0.5f, rCoverBoundInfo.m_Height * 0.5f));
					const Vec3V vMax = VECTOR3_TO_VEC3V(rCoverBoundInfo.m_Position) + (  Vec3V(rCoverBoundInfo.m_Width * 0.5f, rCoverBoundInfo.m_Length * 0.5f, rCoverBoundInfo.m_Height * 0.5f));
					if( vMin.GetXf() < vBoundingMin.x ) { vBoundingMin.x = vMin.GetXf(); }
					if( vMin.GetYf() < vBoundingMin.y ) { vBoundingMin.y = vMin.GetYf(); }
					if( vMin.GetZf() < vBoundingMin.z ) { vBoundingMin.z = vMin.GetZf(); }
					if( vMax.GetXf() > vBoundingMax.x ) { vBoundingMax.x = vMax.GetXf(); }
					if( vMax.GetYf() > vBoundingMax.y ) { vBoundingMax.y = vMax.GetYf(); }
					if( vMax.GetZf() > vBoundingMax.z ) { vBoundingMax.z = vMax.GetZf(); }
				}
			}
			else // apply extra offsets
			{
				vBoundingMin.x = vBoundingMin.x + pCoverBoundOffsetInfo->GetExtraSideOffset();
				vBoundingMax.x = vBoundingMax.x - pCoverBoundOffsetInfo->GetExtraSideOffset();
				vBoundingMin.y = vBoundingMin.y + pCoverBoundOffsetInfo->GetExtraBackwardOffset();
				vBoundingMax.y = vBoundingMax.y - pCoverBoundOffsetInfo->GetExtraForwardOffset();
				vBoundingMax.z = vBoundingMax.z - pCoverBoundOffsetInfo->GetExtraZOffset();
			}
		}
	}

	// Get the object cover tuning, which may be the default
	// Set conditions according to tuning settings.
	const CCoverTuning& coverTuning = CCoverTuningManager::GetInstance().GetTuningForModel(GetHashKey());

	bool bSkipNorthFaceEast =	(coverTuning.m_Flags & CCoverTuning::NoCoverNorthFaceEast) > 0;
	bool bSkipNorthFaceWest =	(coverTuning.m_Flags & CCoverTuning::NoCoverNorthFaceWest) > 0;
	bool bSkipSouthFaceEast =	(coverTuning.m_Flags & CCoverTuning::NoCoverSouthFaceEast) > 0;
	bool bSkipSouthFaceWest =	(coverTuning.m_Flags & CCoverTuning::NoCoverSouthFaceWest) > 0;
	bool bSkipEastFaceNorth =	(coverTuning.m_Flags & CCoverTuning::NoCoverEastFaceNorth) > 0;
	bool bSkipEastFaceSouth =	(coverTuning.m_Flags & CCoverTuning::NoCoverEastFaceSouth) > 0;
	bool bSkipWestFaceNorth =	(coverTuning.m_Flags & CCoverTuning::NoCoverWestFaceNorth) > 0;
	bool bSkipWestFaceSouth =	(coverTuning.m_Flags & CCoverTuning::NoCoverWestFaceSouth) > 0;

	bool bForceLowCornerNorthFaceEast = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerNorthFaceEast) > 0;
	bool bForceLowCornerNorthFaceWest = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerNorthFaceWest) > 0;
	bool bForceLowCornerSouthFaceEast = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerSouthFaceEast) > 0;
	bool bForceLowCornerSouthFaceWest = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerSouthFaceWest) > 0;
	bool bForceLowCornerEastFaceNorth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerEastFaceNorth) > 0;
	bool bForceLowCornerEastFaceSouth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerEastFaceSouth) > 0;
	bool bForceLowCornerWestFaceNorth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerWestFaceNorth) > 0;
	bool bForceLowCornerWestFaceSouth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerWestFaceSouth) > 0;

	// Constant height for forcing low corner cover
	const float fCoverHeightLowCorner = LOW_COVER_MAX_HEIGHT - 0.01f;

	//	Height map		//  Cover points
	//					//     1   2
	//	|-------|		//	 0|-----|3
	//	| 0	  3	|		//	  |		|
	//	|		|		//	  |		|
	//	| 1	  4	|		//	  |     |
	//	|		|		//    |		|
	//	| 2	  5	|		//    |		|
	//	|-------|		//   7|-----|4
	//					//     6   5

	// Local variables
	s32 iNextFreePoint = 0;
	Vector3 vOffset;
	float fCoverHeight;
	float fEffectiveHeight;
	CCoverPoint::eCoverHeight eCoverHeight;
	CCoverPoint::eCoverUsage eEffectiveUsage;
	float fCoverToVehicle;

	TUNE_GROUP_FLOAT(COVER_TUNE, DIST_COVER_TO_VEH_LOW, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(COVER_TUNE, DIST_COVER_TO_VEH_HIGH, 0.25f, 0.0f, 1.0f, 0.01f);

	// Uber hack for B*1613530, I think the extra seat bones make the bounding box bigger, so we need separate offsets
	TUNE_GROUP_FLOAT(COVER_TUNE, DIST_COVER_TO_VEH_LOW_GRANGER, -0.15f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(COVER_TUNE, DIST_COVER_TO_VEH_HIGH_GRANGER, -0.15f, -1.0f, 1.0f, 0.01f);
	const u32 GRANGER_HASH = ATSTRINGHASH("granger", 0x9628879c);
	const bool bIsGranger = (GetHashKey() == GRANGER_HASH);
	const float fDistCoverToVehLow = bIsGranger ? DIST_COVER_TO_VEH_LOW_GRANGER : DIST_COVER_TO_VEH_HIGH;
	const float fDistCoverToVehHigh = bIsGranger ? DIST_COVER_TO_VEH_HIGH_GRANGER : DIST_COVER_TO_VEH_HIGH;
	
	// Left Bonnet Left (0)
	if( !bSkipWestFaceNorth )
	{
		fCoverHeight = rage::Max( afHeights[0], afHeights[3] );
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? fDistCoverToVehLow : fDistCoverToVehHigh;
		vOffset = Vector3( vBoundingMin.x - fCoverToVehicle, vBoundingMax.y - DISTANCE_FROM_COVER_TO_CORNER_SIDE_FRONT, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerWestFaceNorth )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		eEffectiveUsage = CCoverPoint::COVUSE_WALLTOBOTH;
		if( eCoverHeight >= CCoverPoint::COVHEIGHT_TOOHIGH )
		{
			eEffectiveUsage = CCoverPoint::COVUSE_WALLTORIGHT;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 270.0f, fEffectiveHeight, eEffectiveUsage, CCoverPoint::COVARC_90, CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Left Bonnet Front (1)
	if( !bSkipNorthFaceWest )
	{
		fCoverHeight = rage::Max( afHeights[0], afHeights[3] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? DIST_COVER_TO_VEH_LOW : DIST_COVER_TO_VEH_HIGH;
		vOffset = Vector3( vBoundingMin.x + DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMax.y + fCoverToVehicle, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerNorthFaceWest )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 180.0f, fEffectiveHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Right Bonnet Front (2)
	if( !bSkipNorthFaceEast )
	{
		fCoverHeight = rage::Max( afHeights[0], afHeights[3] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? DIST_COVER_TO_VEH_LOW : DIST_COVER_TO_VEH_HIGH;
		vOffset = Vector3( vBoundingMax.x - DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMax.y + fCoverToVehicle, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerNorthFaceEast )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 180.0f, fEffectiveHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Right Bonnet Right (3)
	if( !bSkipEastFaceNorth )
	{
		fCoverHeight = rage::Max( afHeights[0], afHeights[3] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? fDistCoverToVehLow : fDistCoverToVehHigh;
		vOffset = Vector3( vBoundingMax.x + fCoverToVehicle, vBoundingMax.y - DISTANCE_FROM_COVER_TO_CORNER_SIDE_FRONT, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerEastFaceNorth )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		eEffectiveUsage = CCoverPoint::COVUSE_WALLTOBOTH;
		if( eCoverHeight >= CCoverPoint::COVHEIGHT_TOOHIGH )
		{
			eEffectiveUsage = CCoverPoint::COVUSE_WALLTOLEFT;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 90.0f,  fEffectiveHeight, eEffectiveUsage, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Right Boot Right (4)
	if( !bSkipEastFaceSouth )
	{
		fCoverHeight = rage::Max( afHeights[2], afHeights[5] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? fDistCoverToVehLow : fDistCoverToVehHigh;
		vOffset = Vector3( vBoundingMax.x + fCoverToVehicle, vBoundingMin.y + DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerEastFaceSouth )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		eEffectiveUsage = CCoverPoint::COVUSE_WALLTOBOTH;
		if( eCoverHeight >= CCoverPoint::COVHEIGHT_TOOHIGH )
		{
			eEffectiveUsage = CCoverPoint::COVUSE_WALLTORIGHT;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 90.0f, fEffectiveHeight, eEffectiveUsage, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Right Boot Rear (5)
	if( !bSkipSouthFaceEast )
	{
		fCoverHeight = rage::Max( afHeights[2], afHeights[5] );
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? DIST_COVER_TO_VEH_LOW : DIST_COVER_TO_VEH_HIGH;
		vOffset = Vector3( vBoundingMax.x - DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMin.y - fCoverToVehicle, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerSouthFaceEast )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 0.0f, fEffectiveHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Left Boot Rear (6)
	if( !bSkipSouthFaceWest )
	{
		fCoverHeight = rage::Max( afHeights[2], afHeights[5] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? DIST_COVER_TO_VEH_LOW : DIST_COVER_TO_VEH_HIGH;
		vOffset = Vector3( vBoundingMin.x + DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMin.y - fCoverToVehicle, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerSouthFaceWest )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 0.0f, fEffectiveHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}

	// Left Boot Left (7)
	if( !bSkipWestFaceSouth )
	{
		fCoverHeight = rage::Max( afHeights[2], afHeights[5] );	
		fCoverToVehicle = fCoverHeight < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES ? fDistCoverToVehLow : fDistCoverToVehHigh;
		vOffset = Vector3( vBoundingMin.x - fCoverToVehicle, vBoundingMin.y + DISTANCE_FROM_COVER_TO_CORNER_DEFAULT, vBoundingMin.z );
		eCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, CCoverPoint::COVTYPE_VEHICLE );
		fEffectiveHeight = fCoverHeight;
		// force low corner
		if( eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerWestFaceSouth )
		{
			fEffectiveHeight = fCoverHeightLowCorner;
		}
		eEffectiveUsage = CCoverPoint::COVUSE_WALLTOBOTH;
		if( eCoverHeight >= CCoverPoint::COVHEIGHT_TOOHIGH )
		{
			eEffectiveUsage = CCoverPoint::COVUSE_WALLTOLEFT;
		}
		m_data->m_aCoverPoints[iNextFreePoint].init( vOffset, 270.0f, fEffectiveHeight, eEffectiveUsage, CCoverPoint::COVARC_90,CCoverPoint::COVTYPE_VEHICLE );
		iNextFreePoint++;
		m_data->m_iCoverPoints++;
	}
}

//-------------------------------------------------------------------------
// Builds a list of entry/exit points for peds to use when getting in the vehicle
//-------------------------------------------------------------------------
void CVehicleModelInfo::InitLayout( void )
{
	modelinfoAssert(CVehicleMetadataMgr::GetInstance().GetIsInitialised());
	modelinfoAssert(m_data);

	const CVehicleLayoutInfo* pLayoutInfo = GetVehicleLayoutInfo();
	vehicleAssertf(pLayoutInfo,"%s: This vehicle is missing layout information",GetModelName());
	if(!pLayoutInfo)
	{
		return;
	}

	const crSkeletonData* pSkelData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
	m_data->m_SeatInfo.InitFromLayoutInfo(pLayoutInfo, pSkelData, GetNumSeatsOverride());
}



#define USE_BVH_BOUND_FOR_BOAT 1

dev_float STD_VEHICLE_LINEAR_V_COEFF = 0.01f;
dev_float STD_VEHICLE_LINEAR_C_COEFF = 0.01f;
dev_float CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF = 0.01f;

dev_float CVehicleModelInfo::STD_BOAT_LINEAR_V_COEFF = 0.02f;
dev_float CVehicleModelInfo::STD_BOAT_LINEAR_C_COEFF = 0.1f;
dev_float CVehicleModelInfo::STD_BOAT_ANGULAR_V2_COEFF_Y = 0.04f;

dev_float PLANE_DOOR_INCREASED_BREAKING_STRENGTH = 20000.0f;
dev_float PLANE_DOOR_BREAKING_STRENGTH = 2000.0f;

dev_float TANK_DOOR_BREAKING_STRENGTH = -1.0f;
dev_float TRAILER_DOOR_MASS = 400.0f;
dev_float TRAILER_DOOR_ANG_INERTIA = 400.0f;
dev_float STD_VEHICLE_DOOR_MASS = 25.0f;
dev_float STD_VEHICLE_DOOR_ANG_INERTIA = 5.0f;

dev_float sfVehicleForceMargin = 0.04f;
dev_bool sbVehicleForceSetMarginAndShrink = false;
dev_float sfCarMinGroundClearance = 0.35f; 
dev_float sfGP1MinGroundClearance = 0.23f;
dev_float sfRCCarMinGroundClearance = 0.15f;
dev_float sfJetpackMinGroundClearance = -0.1f;
dev_float sfExtraMinGroundClearance = 0.6f;

void CVehicleModelInfo::InitPhys()
{
	modelinfoAssert(m_data);
	fragType* pFragType = GetFragType();
	modelinfoAssertf(pFragType!=NULL, "All vehicles MUST be fragments");
	if(pFragType==NULL)
		return;

	// Build up a list of Entry/Exit points
	InitLayout();

	phArchetypeDamp* pPhysicsArch = pFragType->GetPhysics(0)->GetArchetype();

	// All vehicles are type NON_BVH, but only vehicles with BVHs or physical wheels need BVH_TYPE or WHEEL_TEST respectively
	u32 vehicleTypeFlags = ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE;
	if(static_cast<phBoundComposite*>(pPhysicsArch->GetBound())->GetContainsBVH())
	{
		vehicleTypeFlags |= ArchetypeFlags::GTA_VEHICLE_BVH_TYPE;
	}
	for(int wheelIndex = 0; wheelIndex < NUM_VEH_CWHEELS_MAX; ++wheelIndex)
	{
		int boneIndex = GetBoneIndex(ms_aSetupWheelIds[wheelIndex]);
		if(boneIndex != -1 && pFragType->GetGroupFromBoneIndex(0,boneIndex) != -1)
		{
			vehicleTypeFlags |= ArchetypeFlags::GTA_WHEEL_TEST;
			break;
		}
	}
	// Set flags in archetype to say what type of physics object this is.
	pPhysicsArch->SetTypeFlags(vehicleTypeFlags);

	// Set flags in archetype to say what type of physics object we wish to collide with.
	pPhysicsArch->SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES|ArchetypeFlags::GTA_VEHICLE_BVH_TYPE|ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);

	// find handling data for this vehicle
	CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(GetHandlingId());

#ifdef CHECK_VEHICLE_SETUP
#if !__NO_OUTPUT
    Vector3 oldCgOffset = VEC3V_TO_VECTOR3(GetFragType()->GetPhysics(0)->GetCompositeBounds()->GetCGOffset());
    modelinfoDisplayf( "%s COM: %.2f %.2f %.2f", GetModelName(), oldCgOffset.x, oldCgOffset.y, oldCgOffset.z );

    //Assert( oldCgOffset.Dist(pHandling->m_vecCentreOfMassAbsolute) < 0.01f || oldCgOffset.Dist(pHandling->m_vecCentreOfMassOffset) < 0.01f);//make sure the absolute COM is in the correct place or that the COM is near the modified COM
#endif // !__NO_OUTPUT
#endif

	// allocate type and include flags for all vehicles
	GetFragType()->SetAllocateTypeAndIncludeFlags();

	for(int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildGroups(); i++)
	{
		GetFragType()->GetPhysics(0)->GetAllGroups()[i]->SetStrength(-1.0f);
		GetFragType()->GetPhysics(0)->GetAllGroups()[i]->SetForceTransmissionScaleUp(0.1f);
		GetFragType()->GetPhysics(0)->GetAllGroups()[i]->SetForceTransmissionScaleDown(0.1f);
	}

	crSkeletonData *pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();

	if(GetBoneIndex(VEH_LIGHTCOVER)!=-1)
	{
		crBoneData *pBone = (crBoneData *)pSkeletonData->GetBoneData(GetBoneIndex(VEH_LIGHTCOVER));
		if(pBone && pBone->GetDofs() &(crBoneData::HAS_ROTATE_LIMITS|crBoneData::HAS_TRANSLATE_LIMITS))
		{
			int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(VEH_LIGHTCOVER));
			if(nGroupIndex != -1)
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
				pGroup->SetForceTransmissionScaleUp(0.0f);
				pGroup->SetForceTransmissionScaleDown(0.0f);
				pGroup->SetLatchStrength(-1.0f);	// unlatch manually in vehicle damage code
			}
		}
	}

	crBoneData *pBone = NULL;
	for(int i=0; i<NUM_VEH_DOORS_MAX; i++)
	{
		eHierarchyId nDoorId = ms_aSetupDoorIds[i];
		if(GetBoneIndex(nDoorId)!=-1)
		{
			pBone = (crBoneData *)pSkeletonData->GetBoneData(GetBoneIndex(nDoorId));
			if(pBone->GetDofs() &(crBoneData::HAS_ROTATE_LIMITS|crBoneData::HAS_TRANSLATE_LIMITS))
			{
				int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(nDoorId));
				if(nGroupIndex != -1)
				{
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
					pGroup->SetForceTransmissionScaleUp(0.0f);
					pGroup->SetForceTransmissionScaleDown(0.0f);
					pGroup->SetLatchStrength(-1.0f);	// unlatch manually in vehicle damage code
					pGroup->SetPedScale(0.5f);//Peds should find it really hard to break doors

					if(nDoorId!=VEH_BONNET) // Don't let bonnets fall off unless we make them in game code.
					{
						if(GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_TANK))
						{
							pGroup->SetStrength(TANK_DOOR_BREAKING_STRENGTH);
						}
						else if(GetVehicleType() == VEHICLE_TYPE_PLANE)
						{
							if( MI_PLANE_ALKONOST.IsValid() &&
								GetModelNameHash() == MI_PLANE_ALKONOST.GetName().GetHash() )
							{
								pGroup->SetStrength(PLANE_DOOR_INCREASED_BREAKING_STRENGTH);
							}
							else
							{
								pGroup->SetStrength(PLANE_DOOR_BREAKING_STRENGTH);
							}

						}
						else
						{
							pGroup->SetStrength(500.0f);		// flags in fragInstGta will stop doors breaking off until we want them too
						}

						if( nDoorId == VEH_FOLDING_WING_L ||
							nDoorId == VEH_FOLDING_WING_R )
						{
							pGroup->SetStrength(-1.0f);
						}
					}
					if(GetVehicleType() == VEHICLE_TYPE_TRAILER)
					{
						// We don't want to allow the ramps at the back of trailers to fall off either, so
						// initialise them with infinite joint strength.
						if(nDoorId==VEH_BOOT)
						{
							pGroup->SetStrength(-1.0f);
						}
					}
				}
			}
		}
	}

	// Determine if the vehicle has articulated doors
	bool bHasArticulatedDoors = false;
	if(pSkeletonData)
	{
		for(int i=0; i<NUM_VEH_DOORS_MAX; i++)
		{
			eHierarchyId nDoorId = ms_aSetupDoorIds[i];
			if(GetBoneIndex(nDoorId)!=-1)
			{
				if(pSkeletonData->GetBoneData(GetBoneIndex(nDoorId))->GetDofs() &(crBoneData::HAS_ROTATE_LIMITS|crBoneData::HAS_TRANSLATE_LIMITS))
				{
					if(GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(nDoorId)) != -1)
					{
						bHasArticulatedDoors = true;
						break;
					}
				}
			}
		}
	}
	// force vehicles to copy damping values into the articulated body
	if(bHasArticulatedDoors)
		GetFragType()->SetForceArticulatedDamping();

	// Set up the specialized masses/angular inertias for specific groups on the fragment, then scale
	//   the fragment's mass/angular inertia to the specified values. 
	if(!GetFragType()->GetIsUserModified())
	{
		InitFragType(pHandling);
		GetFragType()->SetIsUserModified();
	}

	InitFromFragType();

	// NOTE: After this point nobody should be messing with the child masses/angular inertias!
	// ---------------------------------------------------------------------------------------

	if(GetIsRotaryAircraft())
	{
		// don't set breaking strengths for extras on heli's because they're reused as rotors and such.
		// set boot breaking strength because that's should be the rear tail plane
		if(GetBoneIndex(HELI_TAIL)!=-1 && GetVehicleType() != VEHICLE_TYPE_BLIMP)
		{
			int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(HELI_TAIL));

			if(nGroupIndex != -1)
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];

				// GTAV B*1884536 increase strength of tail on Savage as it breaks off too easily
				if( MI_HELI_SAVAGE.IsValid() &&
					GetModelNameHash() == MI_HELI_SAVAGE.GetName().GetHash() )
				{
					pGroup->SetStrength( 20000.0f );
				}
				else
				{
					pGroup->SetStrength(2000.0f);
				}
				
				pGroup->SetForceTransmissionScaleUp(0.0f);
				pGroup->SetForceTransmissionScaleDown(0.0f);

				const s32 nChildGroupsPointersIdx = pGroup->GetChildGroupsPointersIndex();
				if((nChildGroupsPointersIdx > 0) && (nChildGroupsPointersIdx < 255))	// looks like -1 becomes 255 when encoded as u8
				{
					// Prevent rotor collision detaching from tail
					fragTypeGroup* pChildGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nChildGroupsPointersIdx];
					pChildGroup->SetStrength(-1.0f);
					pChildGroup->SetForceTransmissionScaleUp(0.0f);
					pChildGroup->SetForceTransmissionScaleDown(0.0f);
				}
			}
		}
	}
	else if (GetIsPlane())
	{
		// Planes use extras for flaps so don't let them break off

		// Set strengths for wings and tail
		// These are flagged on the instance not to break off until we want them to

		int nStartId = PLANE_FIRST_BREAKABLE_PART;
		for(int i = 0; i <PLANE_NUM_BREAKABLES; i++)
		{
			eHierarchyId nId = (eHierarchyId)(nStartId + i);
			int iBoneIndex = GetBoneIndex(nId);
			if(iBoneIndex > -1)
			{
				int nGroup = GetFragType()->GetGroupFromBoneIndex(0,iBoneIndex);

				if(nGroup > -1)
				{
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroup];
					pGroup->SetStrength(-1.0f);
					pGroup->SetForceTransmissionScaleUp(0.0f);
					pGroup->SetForceTransmissionScaleDown(0.0f);	
				}
			}
		}

		GetFragType()->GetPhysics(0)->GetArchetype()->SetMaxSpeed(MAX_PLANE_SPEED);
	}
	// set breaking strengths on extras, if vehicle is flagged as having 'strong' extras, they're infinitely strong
	else if(pSkeletonData && !GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_STRONG) && !GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_ONLY_BREAK_WHEN_DESTROYED))	
	{
		float breakStrength = 2000.0f;

		if( ( MI_CAR_TROPHY_TRUCK.IsValid() &&
			    GetModelNameHash() == MI_CAR_TROPHY_TRUCK.GetName().GetHash() ) ||
			( MI_CAR_TROPHY_TRUCK2.IsValid() &&
			    GetModelNameHash() == MI_CAR_TROPHY_TRUCK2.GetName().GetHash() ) ||
			GetModelNameHash() == MI_CAR_PBUS2.GetName().GetHash() )
		{
			static float sf_trophyTruckExtraBreakStrength = 20000.0f;
			breakStrength = sf_trophyTruckExtraBreakStrength;
		}

		for(int nExtra=VEH_EXTRA_1; nExtra<=VEH_LAST_EXTRA; nExtra++)
		{
			if(GetBoneIndex(nExtra)!=-1 && (!GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_CONVERTIBLE) || nExtra > VEH_EXTRA_4))
			{
				int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(nExtra));
				if(nGroupIndex != -1)
				{
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
					pGroup->SetStrength( breakStrength );
				}
			}
		}

        if( pHandling->GetCarHandlingData() &&
            pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
        {
            static const int snNumSpecialExtras = 6;

            int specialExtras[ snNumSpecialExtras  ] = { VEH_EXTRA_6, // front wing
                                                         VEH_EXTRA_1, // rear wing
                                                         VEH_EXTRA_4, // rollover hoop
                                                         VEH_EXTRA_5, // main bodywork
                                                         VEH_EXTRA_7, // nose bodywork
                                                         VEH_EXTRA_8, // under tray
                                                       };

            float specialExtraBreakingStrengths[ snNumSpecialExtras ] = {   8800.0f,
                                                                            6000.0f,
                                                                            17500.0f,
                                                                            2250.0f,
                                                                            1550.0f,
                                                                            2600.0f,
                                                                          };

            for( int i = 0; i < snNumSpecialExtras; i++ )
            {
                if( GetBoneIndex( specialExtras[ i ] ) != -1 )
                {
                    int nGroupIndex = GetFragType()->GetGroupFromBoneIndex( 0, GetBoneIndex( specialExtras[ i ] ) );
                    if( nGroupIndex != -1 )
                    {
                        fragTypeGroup* pGroup = GetFragType()->GetPhysics( 0 )->GetAllGroups()[ nGroupIndex ];
                        pGroup->SetStrength( specialExtraBreakingStrengths[ i ] );
                    }
                }
            }
        }
	}

	// do some stuff to the bounds - must be done after SetupFragDoorAndWheels
	// because code below uses CalculateHeightsAboveRoad which uses wheel radius pulled out of wheel bounds
	if(sfVehicleForceMargin > 0.0f)
	{
		if(GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE)
		{
			phBoundComposite* pBoundComposite = ((phBoundComposite*)GetFragType()->GetPhysics(0)->GetArchetype()->GetBound());
			for(int nBound=0; nBound<pBoundComposite->GetNumBounds(); nBound++)
			{
				phBound* pBound = pBoundComposite->GetBound(nBound);
				if(pBound && pBound->GetType()==phBound::GEOMETRY)
				{
					phBoundGeometry* pBoundGeometry = (phBoundGeometry*)pBound;

					if(sbVehicleForceSetMarginAndShrink)
						pBoundGeometry->SetMarginAndShrink(sfVehicleForceMargin);

					pBoundGeometry->SetMargin(sfVehicleForceMargin);
				}
			}
		}
	}

	fwModelId modelId = CModelInfo::GetModelIdFromName( GetModelNameHash() );

	bool bReducedGroundClearanceBike = (MI_BIKE_ESSKEY.IsValid() && modelId == MI_BIKE_ESSKEY);
		
	bool bReducedGroundClearanceTrike = (MI_TRIKE_RROCKET.IsValid() && modelId == MI_TRIKE_RROCKET);

	bool bIsJetPack = ( MI_JETPACK_THRUSTER.IsValid() && modelId == MI_JETPACK_THRUSTER );

	bool forceGroundClearance = ( MI_BIKE_SHOTARO.IsValid() && modelId == MI_BIKE_SHOTARO ) ||
								( MI_BIKE_SANCTUS.IsValid() && modelId== MI_BIKE_SANCTUS ) ||
								( MI_TRIKE_CHIMERA.IsValid() && modelId == MI_TRIKE_CHIMERA ) ||
								bIsJetPack ||
								bReducedGroundClearanceBike ||
								bReducedGroundClearanceTrike;

	bool useGP1GroundClearance = ( MI_CAR_GP1.IsValid() && modelId == MI_CAR_GP1 ) || 
								 ( MI_CAR_RUSTON.IsValid() && modelId == MI_CAR_RUSTON ) ||
								 ( MI_CAR_CADDY3.IsValid() && modelId == MI_CAR_CADDY3 ) ||
								 ( MI_CAR_DELUXO.IsValid() && modelId == MI_CAR_DELUXO ) ||
								 ( MI_CAR_TEZERACT.IsValid() && modelId == MI_CAR_TEZERACT );

    bool dontUpdateGroundClearance = ( GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR ) ||
                                     ( MI_CAR_CHERNOBOG.IsValid() && modelId == MI_CAR_CHERNOBOG );

    bool useExtraGroundClearance = ( MI_TANK_KHANJALI.IsValid() && modelId == MI_TANK_KHANJALI );

	// want to enforce min ground clearance
	if((GetVehicleType()==VEHICLE_TYPE_CAR || GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR || GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE || forceGroundClearance) && (!(pHandling->mFlags & MF_DONT_FORCE_GRND_CLEARANCE) || (pHandling->mFlags & MF_IS_RC)) && !dontUpdateGroundClearance )//some vehicles explicitly state they dont want their ground clearance enforced
	{
		float fHeightAboveGroundF, fHeightAboveGroundR;
		CalculateHeightsAboveRoad(&fHeightAboveGroundF, &fHeightAboveGroundR);

        float fGroundClearance = 0.0f;
		
        if((pHandling->mFlags & MF_IS_RC))
        {
            fGroundClearance = sfRCCarMinGroundClearance;
        }
		else
        {
			if( bIsJetPack )
			{
				fGroundClearance = sfJetpackMinGroundClearance;
			}
            else if( useExtraGroundClearance )
            {
                fGroundClearance = sfExtraMinGroundClearance;
            }
			else if( !useGP1GroundClearance )
			{
				fGroundClearance = sfCarMinGroundClearance;
			}
			else
			{
				fGroundClearance = sfGP1MinGroundClearance;
			}

        }

		if( GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) )
		{
			fGroundClearance -= pHandling->m_fSuspensionRaise;
		}

        float fMinVertexPos = -rage::Max(0.0f, fHeightAboveGroundF - fGroundClearance);
		// B*1889431: Make sure the Zentorno doesn't have bounds that are too high so it doesn't sink into the ground when it gets destroyed.
		if(modelId == MI_CAR_ZENTORNO)
		{
			// This keeps the bounds as close in line with the body of the car as possible without being so low that it hits the kerb and other small objects.
			fMinVertexPos -= 0.12f;
		}

		if( forceGroundClearance &&
			GetVehicleType() != VEHICLE_TYPE_TRAILER)
		{
			if(bReducedGroundClearanceBike)
			{
				fMinVertexPos += 0.07f;
			}
			else if (bReducedGroundClearanceTrike)
			{
				fMinVertexPos -= 0.02f;
			}
			else
			{
				fMinVertexPos += 0.15f;
			}
		}

		// get composite bound directly from frag type
		phBoundComposite* pBoundComp = GetFragType()->GetPhysics(0)->GetCompositeBounds();
		Vector3 vertPos;

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
		const int needsUpdateListSize = pBoundComp->GetNumBounds();
		phBoundGeometry ** needsUpdateList = Alloca(phBoundGeometry*,needsUpdateListSize);
		int needsUpdateListCount = 0;
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION

		int rampBoundIndex = -1;

		if( GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RAMP ) )
		{
			int iBoneIndex = GetBoneIndex( VEH_BUMPER_F );
			if(iBoneIndex > -1)
			{
				rampBoundIndex = GetFragType()->GetComponentFromBoneIndex( 0, iBoneIndex );
			}
		}

		int door_DSIDE_BoundIndex = -1;
		int door_PSIDE_BoundIndex = -1;
		bool bHasSpecialDoors = ((modelId == MI_CAR_CYCLONE2 || modelId == MI_CAR_CORSITA) ? true : false);
		if (bHasSpecialDoors)
		{
			int iBoneIndex_DSIDE = GetBoneIndex(VEH_DOOR_DSIDE_F);
			if (iBoneIndex_DSIDE > -1)
			{
				door_DSIDE_BoundIndex = GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex_DSIDE);
			}
			int iBoneIndex_PSIDE = GetBoneIndex(VEH_DOOR_PSIDE_F);
			if (iBoneIndex_PSIDE > -1)
			{
				door_PSIDE_BoundIndex = GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex_PSIDE);
			}
		}


		for(int nBound=0; nBound < pBoundComp->GetNumBounds(); nBound++)
		{
			// NOTE: dont touch door bounds on cyclone2
			if (door_DSIDE_BoundIndex == nBound || door_PSIDE_BoundIndex == nBound)
			{
				continue;
			}

			if( nBound != rampBoundIndex )
			{
				if(pBoundComp->GetBound(nBound) && pBoundComp->GetBound(nBound)->GetType()==phBound::GEOMETRY)
				{
					phBoundGeometry* pBoundGeom = static_cast<phBoundGeometry*>(pBoundComp->GetBound(nBound));
					const Matrix34& boundMatrix = RCC_MATRIX34(pBoundComp->GetCurrentMatrix(nBound));

					bool shrunkVertsChanged = false;
					for(int nVert=0; nVert < pBoundGeom->GetNumVertices(); nVert++)
					{
						// want to deform the shrunk geometry vertices, ignore the preshrunk ones since we want shape tests to hit the original bike
						if(pBoundGeom->GetShrunkVertexPointer())
						{
							vertPos.Set(VEC3V_TO_VECTOR3(pBoundGeom->GetShrunkVertex(nVert)));
						
							// Projecting the vertices along a cardinal axis like this shouldn't invalidate the octant map.
							// A vertex that was shadowed before will remain shadowed afterward. A vertex that wasn't
							// shadowed before may become shadowed but won't hurt anything if it's left in the octant map.
							// Ideally this would be done in the converter. If any additional vertex manipulation is needed
							// that invalidates the octant map then it will most certainly need to be done in the converter.
							// Otherwise we will have to recompute the octant map here. This would require allocating memory
							// which would wasteful since we can't free the octant map memory already used in the resource.
							const float fMinVertexPos_ = fMinVertexPos - boundMatrix.d.z;
							if (vertPos.z < fMinVertexPos_)
								vertPos.z = fMinVertexPos_;

							// need to remove const from vertex pointer to be able to set it's position
							shrunkVertsChanged |= pBoundGeom->SetShrunkVertex(nVert, RCC_VEC3V(vertPos));
						}
						// no shrunk verts is unexpected
						else
							modelinfoAssert(false);
					}
#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
					if(shrunkVertsChanged)
					{
						Assert(needsUpdateListCount < needsUpdateListSize);
						needsUpdateList[needsUpdateListCount] = pBoundGeom;
						needsUpdateListCount++;
					}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
				}
			}
		}

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
		if (needsUpdateListCount > 0)
		{
			//sysTimer timer;
			// This location is actually working with the "original" bounds
			// There is no inst or cache entry yet to work in
			// TODO: Figure out where to allocate from and then make sure we release that memory when this fragType is removed (Krehan mentioned that the gamevirt pool might be ok since our allocations should be in the <4k range)
			//fragMemStartCacheHeapFunc(pFragInst->GetCacheEntry());
	//		for (int i = 0 ; i < needsUpdateListCount ; i++)
	//			needsUpdateList[i]->RecomputeOctantMap();
			//fragMemEndCacheHeap();
			//float time = timer.GetUsTime();
			//Displayf("### MODELINFO Took %f to regenerate octant maps for %d bounds.",time,needsUpdateListCount);
		}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	}

	// setup material flags on this bound
	if(GetFragType()->GetPhysics(0)->GetArchetype()->GetBound())
		CopyMaterialFlagsToBound(GetFragType()->GetPhysics(0)->GetArchetype()->GetBound());


	// setup damping characteristics of different types of vehicle
	if(GetIsRotaryAircraft() || GetVehicleType()==VEHICLE_TYPE_PLANE)
	{
		CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
		if(pFlyingHandling)
		{
			GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_V2, Vector3(pHandling->m_fInitialDragCoeff, pHandling->m_fInitialDragCoeff, pHandling->m_fInitialDragCoeff));
			float fLV =  bHasArticulatedDoors ? 0.5f*pFlyingHandling->m_fMoveRes : pFlyingHandling->m_fMoveRes;
			GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_V, Vector3(fLV, fLV, fLV));
			GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_C, Vector3(STD_VEHICLE_LINEAR_C_COEFF, STD_VEHICLE_LINEAR_C_COEFF, STD_VEHICLE_LINEAR_C_COEFF));

			if(MagSquared(pFlyingHandling->m_vecTurnRes).Getf() > 0.0f)
			{
				Vector3 vecAV =  bHasArticulatedDoors ? 0.5f*RCC_VECTOR3(pFlyingHandling->m_vecTurnRes) : RCC_VECTOR3(pFlyingHandling->m_vecTurnRes);
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V, vecAV);
			}
			if(MagSquared(pFlyingHandling->m_vecSpeedRes).Getf() > 0.0f)
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V2, RCC_VECTOR3(pFlyingHandling->m_vecSpeedRes));

			CalculateMaxFlatVelocityEstimate(pHandling); // Need max velocity for speed damping calculation
		}
		else
			modelinfoAssert(false);
	}
	else
	{
		// force double linear damping for non-articulated vehicles to try and match the behaviour of articulated ones
		float fDV = bHasArticulatedDoors ? STD_VEHICLE_LINEAR_V_COEFF : 2.0f*STD_VEHICLE_LINEAR_V_COEFF;
		GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_V, Vector3(fDV, fDV, 0.0f));
		// same for linear angular damping
		fDV = bHasArticulatedDoors ? STD_VEHICLE_ANGULAR_V_COEFF : 2.0f*STD_VEHICLE_ANGULAR_V_COEFF;
		GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V, Vector3(fDV, fDV, fDV));

		GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_V2, Vector3(pHandling->m_fInitialDragCoeff, pHandling->m_fInitialDragCoeff, 0.0f));
		GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_C, Vector3(STD_VEHICLE_LINEAR_C_COEFF, STD_VEHICLE_LINEAR_C_COEFF, 0.0f));

		if(GetIsBoat() || GetIsAmphibiousVehicle() )
		{
			CBoatHandlingData* pBoatHandling = pHandling->GetBoatHandlingData();
			if(pBoatHandling)
			{
				fDV = bHasArticulatedDoors ? STD_BOAT_LINEAR_V_COEFF : 2.0f*STD_BOAT_LINEAR_V_COEFF;
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_V, Vector3(fDV, fDV, 0.0f));
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::LINEAR_C, Vector3(STD_BOAT_LINEAR_C_COEFF, STD_BOAT_LINEAR_C_COEFF, 0.0f));

				fDV = bHasArticulatedDoors ? 1.0f : 2.0f;
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V, fDV * RCC_VECTOR3(pBoatHandling->m_vecTurnResistance));
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V2, Vector3(0.02f, STD_BOAT_ANGULAR_V2_COEFF_Y, 0.02f));

				CalculateMaxFlatVelocityEstimate(pHandling);
			}

			if( GetIsAmphibiousVehicle() )
			{
				CalculateMaxFlatVelocityEstimate(pHandling);
			}
		}
		else if(GetIsSubmarine() || GetIsSubmarineCar())
		{
			const CSubmarineHandlingData* pSubHandling = pHandling->GetSubmarineHandlingData();
			if(physicsVerifyf(pSubHandling,"Unexpected NULL sub handling for %s",GetModelName()))
			{
				fDV = bHasArticulatedDoors ? 1.0f : 2.0f;
				GetFragType()->GetPhysics(0)->SetDamping(phArchetypeDamp::ANGULAR_V, fDV * pSubHandling->GetTurnRes());
			}

			// GTAV - B*1811173 - These need setting up so BRING_VEHICLE_TO_HALT works for the submarine car.
			if( GetIsSubmarineCar() )
			{
				CalculateMaxFlatVelocityEstimate(pHandling);
			}
		}
		else
		{
			CalculateMaxFlatVelocityEstimate(pHandling);
		}

	}

	// Make sure type archetypes are in sync with the type's damping coeff
	GetFragType()->GetPhysics(0)->ApplyDamping(GetFragType()->GetPhysics(0)->GetArchetype());

	// create a much simpler physics archetype for use by dummy vehicles
	if(!HasPhysics())
	{
		if(GetVehicleType()!=VEHICLE_TYPE_BOAT)
		{
			Vector3 vecBBoxHalfWidth, vecBBoxCentre;
			GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetBoundingBoxHalfWidthAndCenter(RC_VEC3V(vecBBoxHalfWidth), RC_VEC3V(vecBBoxCentre));

			// create a simple box bound
			phBoundBox *pBoxBound= rage_new phBoundBox(1.9f*vecBBoxHalfWidth);
			pBoxBound->SetCentroidOffset(RCC_VEC3V(vecBBoxCentre));
			pBoxBound->SetMaterial(MATERIALMGR.FindMaterialId("car_metal"));

			phArchetypeDamp* pArchetype = rage_new phArchetypeDamp;
			// set flags in archetype to say what type of physics object this is
			pArchetype->SetTypeFlags(ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);

			// set flags in archetype to say what type of physics object we wish to collide with
			pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_BOX_VEHICLE_INCLUDE_TYPES);

			pArchetype->SetMass(GetFragType()->GetPhysics(0)->GetArchetype()->GetMass());
			pArchetype->SetAngInertia(GetFragType()->GetPhysics(0)->GetArchetype()->GetAngInertia());

			float fDV = 2.0f*STD_VEHICLE_LINEAR_V_COEFF;
			pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(pHandling->m_fInitialDragCoeff, pHandling->m_fInitialDragCoeff, 0.0f));
			pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(fDV, fDV, 0.0f));
			pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C, Vector3(STD_VEHICLE_LINEAR_C_COEFF, STD_VEHICLE_LINEAR_C_COEFF, 0.0f));
			// set angular damping to fragType defaults
			pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(0.02f, 0.02f, 0.02f));
			pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(STD_VEHICLE_ANGULAR_V_COEFF, STD_VEHICLE_ANGULAR_V_COEFF, STD_VEHICLE_ANGULAR_V_COEFF));

			pArchetype->SetBound(pBoxBound);

			// Set the max speed based on the frag type so that these two are in sync from the beginning.
			pArchetype->SetMaxSpeed(GetFragType()->GetPhysics(0)->GetArchetype()->GetMaxSpeed());

			// need to relinquish control of the bounds we just created now that they're added to the archetype
			pBoxBound->Release();

			// Add a reference to the physics archetype
			// as its loading and unloading is bound to this model
			pArchetype->AddRef();

			// finally store in modelinfo
			//SetPhysics(pArchetype, -1);
			CBaseModelInfo::SetPhysics(pArchetype);
		}
	}

	// By this point the vehicle must have a physics inst
	// Examine the vehicle at each corner to determine what height of cover is available
	CalculateVehicleCoverPoints();

	// The wheelbase multiplier is used by the ai to make large vehicles steer in earlier. Value between 0.0 and 1.0
	m_wheelBaseMultiplier = ((GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetBoundingBoxMax().GetYf() - GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetBoundingBoxMin().GetYf()) - 5.0f) / 5.0f;
	m_wheelBaseMultiplier = Clamp(m_wheelBaseMultiplier, 0.0f, 1.0f);	


	// Set up seat bound indices
	// If they have collision then the vehicle has an open top
	for(int i = 0; i < m_data->m_SeatInfo.GetNumSeats(); i++)
	{
		int iBoneIndex = m_data->m_SeatInfo.GetBoneIndexFromSeat(i);
		if(iBoneIndex > -1)
		{
			s16 iComponentIndex = (s16)GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex);
			if(iComponentIndex > -1)
			{
				m_data->m_iSeatFragChildren[i] = iComponentIndex;
				m_bHasSeatCollision = true;
			}
		}
	}

}

void CVehicleModelInfo::CalculateMaxFlatVelocityEstimate(CHandlingData *pHandling)
{
	// want to estimate max speed of this vehicle (assuming it's in top gear)
	float fDv2 = pHandling->m_fInitialDragCoeff;
	float fDv = 2.0f*STD_VEHICLE_LINEAR_V_COEFF;
	float fDc = STD_VEHICLE_LINEAR_C_COEFF;

	float fT = pHandling->m_fInitialDriveForce * -GRAVITY;
	if(pHandling->GetFlyingHandlingData())
	{
		fDv = pHandling->GetFlyingHandlingData()->m_fMoveRes;
		fT = pHandling->GetFlyingHandlingData()->m_fThrust * -GRAVITY;
	}

	float fSqrTerm = fDv*fDv - 4.0f * fDv2 * (fDc - fT);
	float fMaxVel1 = (-fDv + rage::Sqrtf(fSqrTerm)) / (2.0f * fDv2);
	float fMaxVel2 = (-fDv - rage::Sqrtf(fSqrTerm)) / (2.0f * fDv2);

	pHandling->m_fEstimatedMaxFlatVel = rage::Min(fMaxVel1, fMaxVel2);
	if(pHandling->m_fEstimatedMaxFlatVel < 0.0f)
		pHandling->m_fEstimatedMaxFlatVel = rage::Max(fMaxVel1, fMaxVel2);
 
	if(GetVehicleType() == VEHICLE_TYPE_HELI)
	{
		pHandling->m_fEstimatedMaxFlatVel = pHandling->m_fEstimatedMaxFlatVel;
	}
	else if(pHandling->GetFlyingHandlingData())
	{
		float fThrustFallOffSpeed = Sqrtf(1.0f/pHandling->GetFlyingHandlingData()->m_fThrustFallOff);
		pHandling->m_fEstimatedMaxFlatVel = Min(fThrustFallOffSpeed, pHandling->m_fEstimatedMaxFlatVel);

		if( pHandling->m_fEstimatedMaxFlatVel < 10.0f )
		{
			pHandling->m_fEstimatedMaxFlatVel = Max( pHandling->m_fEstimatedMaxFlatVel, pHandling->m_fBoostMaxSpeed );
		}
	}
	else
	{	
		pHandling->m_fEstimatedMaxFlatVel = Min(pHandling->m_fInitialDriveMaxVel, pHandling->m_fEstimatedMaxFlatVel);
	}
}

dev_float VEHICLE_RUDDER_MULT = 0.1f;
dev_float VEHICLE_DRAG_MULT_XY = 15.0f;
dev_float VEHICLE_DRAG_MULT_ZUP = 200.0f;
dev_float VEHICLE_DRAG_MULT_ZDOWN = 150.0f;

dev_float BIKE_DRAG_MULT_XY = 50.0f;
dev_float BIKE_DRAG_MULT_ZUP = 125.0f;
dev_float BIKE_DRAG_MULT_ZDOWN = 75.0f;

dev_float PLANE_DRAG_MULT_XY = 300.0f;
dev_float PLANE_DRAG_MULT_ZUP = 300.0f;
dev_float PLANE_DRAG_MULT_ZDOWN = 500.0f;

void CVehicleModelInfo::InitWaterSamples()
{
#if __DEV
	if(CBaseModelInfo::ms_bPrintWaterSampleEvents)
	{
		modelinfoDisplayf("[Buoyancy] Creating water samples for vehicle %s",GetModelName());
	}
#endif
	// Check we haven't done this before
	modelinfoAssertf(m_pBuoyancyInfo == NULL,"InitWaterSamples() called when water samples already exist");
	if(m_pBuoyancyInfo != NULL)
	{
		return;
	}

	// Create buoyancy info
	m_pBuoyancyInfo = rage_new CBuoyancyInfo();

	if(GetIsBoat() || GetIsAmphibiousVehicle())
	{
		GenerateWaterSamplesForBoat();
	}
	else if(GetIsSubmarine() || GetIsSubmarineCar())
	{
		GenerateWaterSamplesForSubmarine();
	}
	else if(GetIsPlane())
	{
		GenerateWaterSamplesForPlane();
	}
	else if(GetIsBike())
    {
        GenerateWaterSamplesForBike();
    }
	else if( GetIsHeli() )
	{
		GenerateWaterSamplesForHeli();
	}
    else
	{
		GenerateWaterSamplesForVehicle();		
	}	

#if __DEV
	ms_nNumWaterSamplesInMemory += m_pBuoyancyInfo->m_nNumWaterSamples;
	PF_SET(NumWaterSamplesInMemory, ms_nNumWaterSamplesInMemory);
#endif // __DEV
}

void CVehicleModelInfo::GenerateWaterSamplesForBike()
{
    modelinfoAssert(m_pBuoyancyInfo);

    // Calculate how many samples we need
    int iSamplesNumber = 0;
    int iCurrentSample = 0;

    Vector3 vecBBoxMin, vecBBoxMax;

    phBoundComposite* pBoundComp = GetFragType()->GetPhysics(0)->GetCompositeBounds();

    vecBBoxMin.Set(LARGE_FLOAT, LARGE_FLOAT, LARGE_FLOAT);
    vecBBoxMax.Set(-LARGE_FLOAT, -LARGE_FLOAT, -LARGE_FLOAT);
    Vector3 vecTemp, vecTemp2;

    for(int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildren(); i++)
    {
        if(GetFragType()->GetPhysics(0)->GetAllChildren()[i] && pBoundComp->GetBound(i))
        {
            if(pBoundComp->GetBound(i)->GetType() != phBound::DISC 
				USE_GEOMETRY_CURVED_ONLY(&& pBoundComp->GetBound(i)->GetType() != phBound::GEOMETRY_CURVED))
            {
                RCC_MATRIX34(pBoundComp->GetCurrentMatrix(i)).Transform(VEC3V_TO_VECTOR3(pBoundComp->GetBound(i)->GetBoundingBoxMin()), vecTemp);
                vecTemp2.Set(vecBBoxMin);
                vecBBoxMin.Min(vecTemp, vecTemp2);

                RCC_MATRIX34(pBoundComp->GetCurrentMatrix(i)).Transform(VEC3V_TO_VECTOR3(pBoundComp->GetBound(i)->GetBoundingBoxMax()), vecTemp);
                vecTemp2.Set(vecBBoxMax);
                vecBBoxMax.Max(vecTemp, vecTemp2);
            }
            else
            {
                iSamplesNumber++;
            }
        }
    }

    int nMaxNumSteps = 3;
    int nMinNumStepsX = 1;
    int nMinNumStepsY = 3;

    CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(m_handlingId);
    modelinfoAssert(pHandling);

    // Vehicle buoyancy samples are always defined by the height of the car
    const float fSize = (vecBBoxMax.z - vecBBoxMin.z) / 2.0f;

    // Rows / columns of samples in XY are decided by the number of samples we can fit in each dimension
    int nNumStepsX = (int) ((vecBBoxMax.x - vecBBoxMin.x)  / (2.0f*fSize));
    int nNumStepsY = (int) ((vecBBoxMax.y - vecBBoxMin.y)  / (2.0f*fSize));

    // Clamp steps to min/max values
    if(nNumStepsX < nMinNumStepsX)
        nNumStepsX = nMinNumStepsX;
    else if(nNumStepsX > nMaxNumSteps)
        nNumStepsX = nMaxNumSteps;

    if(nNumStepsY < nMinNumStepsY)
        nNumStepsY = nMinNumStepsY;
    else if(nNumStepsY > nMaxNumSteps)
        nNumStepsY = nMaxNumSteps;

    iSamplesNumber += nNumStepsX * nNumStepsY;


    // Create array of water samples
    modelinfoAssert(m_pBuoyancyInfo->m_WaterSamples == NULL);
    m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[iSamplesNumber];
    m_pBuoyancyInfo->m_nNumWaterSamples = (s16)iSamplesNumber;


    for(int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildren(); i++)
    {
        if(GetFragType()->GetPhysics(0)->GetAllChildren()[i] && pBoundComp->GetBound(i))
        {
            if(pBoundComp->GetBound(i)->GetType() == phBound::DISC 
				USE_GEOMETRY_CURVED_ONLY(|| pBoundComp->GetBound(i)->GetType() == phBound::GEOMETRY_CURVED))
            {
                const Matrix34& boundMatrix = RCC_MATRIX34(pBoundComp->GetCurrentMatrix(i));

                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_vSampleOffset.Set(boundMatrix.d);
                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_fSize = pBoundComp->GetBound(i)->GetRadiusAroundCentroid();
                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_nComponent = 0;
                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_fBuoyancyMult = 0.0f;
                iCurrentSample++;
            }
        }
    }

    const float fStepX = (vecBBoxMax.x - vecBBoxMin.x) / (float)(nNumStepsX);
    const float fStepY = (vecBBoxMax.y - vecBBoxMin.y) / (float)(nNumStepsY);		
    const float fCentreZ = vecBBoxMin.z + fSize;

    float fSizeTotal = 0.0f;
    float fX = vecBBoxMin.x + fStepX/2.0f;
    for(int i=0; i<nNumStepsX; i++, fX += fStepX)
    {
        float fY = vecBBoxMin.y + fStepY/2.0f;
        for(int j=0; j<nNumStepsY; j++, fY += fStepY)
        {
            // want to be very careful we don't overflow the array
            if(iCurrentSample < iSamplesNumber)
            {
                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_vSampleOffset.Set(fX, fY, fCentreZ);
                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_fSize = fSize;

                fSizeTotal += fSize;

                m_pBuoyancyInfo->m_WaterSamples[iCurrentSample].m_nComponent = 0;
                iCurrentSample++;
            }
        }
    }


    if(pHandling)
    {
        m_pBuoyancyInfo->m_fBuoyancyConstant = CHandlingDataMgr::GetHandlingData(m_handlingId)->m_fBuoyancyConstant * -GRAVITY / fSizeTotal;
    }
    else
    {
        m_pBuoyancyInfo->m_fBuoyancyConstant = 0.0f;
    }
    m_pBuoyancyInfo->m_fLiftMult = 0.0f;//VEHICLE_LIFT_MULT * -GRAVITY / fSizeTotal;
    m_pBuoyancyInfo->m_fKeelMult = VEHICLE_RUDDER_MULT * -GRAVITY / fSizeTotal;

    m_pBuoyancyInfo->m_fDragMultXY =  BIKE_DRAG_MULT_XY * -GRAVITY / fSizeTotal;
    m_pBuoyancyInfo->m_fDragMultZUp =  BIKE_DRAG_MULT_ZUP * -GRAVITY / fSizeTotal;
    m_pBuoyancyInfo->m_fDragMultZDown =  BIKE_DRAG_MULT_ZDOWN * -GRAVITY / fSizeTotal;
}

void CVehicleModelInfo::GenerateWaterSamplesForVehicle()
{
	modelinfoAssert(m_pBuoyancyInfo);

	// We might be adding some additional samples defined in vehicles.meta:
	int nNumAdditionalSamples = 0;
	CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
	if(pBuoyancyExtension)
	{
		nNumAdditionalSamples = pBuoyancyExtension->GetNumAdditionalVfxSamples();
	}

	Vector3 vecBBoxMin, vecBBoxMax;
	GetVehicleBBoxForBuoyancy(vecBBoxMin, vecBBoxMax);

	// Vehicles will likely have a composite bound. Assume the first child bound of this composite is the chassis bound
	// and therefore the one which we should apply the samples to. This avoids the problem of buoyancy samples being
	// created outside the bounds of vehicles like the "rhino" where the turret expands the bounding box.
	if(!GetIsBike() && GetFragType() && GetFragType()->GetPhysics(0) && GetFragType()->GetPhysics(0)->GetArchetype())
	{
		phBound* pBound = GetFragType()->GetPhysics(0)->GetArchetype()->GetBound();
		if(pBound->GetType()==phBound::COMPOSITE)
		{
			// This should be the chassis bound?
			pBound = static_cast<phBoundComposite*>(pBound)->GetBound(0);

			Vector3 vRootBoundBBoxMin = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin());
			Vector3 vRootBoundBBoxMax = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax());

			// Don't use the root bound's bounding box size if we suspect this isn't the main bound. An example
			// would be a pickup truck where the root bound only covers the cab.
			if(vRootBoundBBoxMax.x-vRootBoundBBoxMin.x > 0.7f*(vecBBoxMax.x-vecBBoxMin.x) &&
				vRootBoundBBoxMax.y-vRootBoundBBoxMin.y > 0.7f*(vecBBoxMax.y-vecBBoxMin.y))
			{
				// We want to keep the full bounding box height though as things like lorry containers / bikes can have
				// bounds which don't quite follow our assumptions about the chassis bound.
				vecBBoxMin.x = vRootBoundBBoxMin.x;
				vecBBoxMax.x = vRootBoundBBoxMax.x;
				vecBBoxMin.y = vRootBoundBBoxMin.y;
				vecBBoxMax.y = vRootBoundBBoxMax.y;
			}
		}
	}

	int nMaxNumSteps = 0;
	int nMinNumStepsX = 2;
    int nMinNumStepsY = 2;
 	if(GetIsBike())
	{
		nMaxNumSteps = 2;
        nMinNumStepsX = 1;
	}
	else
	{
		nMaxNumSteps = 3;
	}

    CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(m_handlingId);
    modelinfoAssert(pHandling);

    // Vehicle buoyancy samples are always defined by the height of the car
	const float fSize = (vecBBoxMax.z - vecBBoxMin.z) / 2.0f;

	// Rows / columns of samples in XY are decided by the number of samples we can fit in each dimension
	int nNumStepsX = (int) ((vecBBoxMax.x - vecBBoxMin.x)  / (2.0f*fSize));
	int nNumStepsY = (int) ((vecBBoxMax.y - vecBBoxMin.y)  / (2.0f*fSize));

	// Clamp steps to min/max values
	if(nNumStepsX < nMinNumStepsX)
		nNumStepsX = nMinNumStepsX;
	else if(nNumStepsX > nMaxNumSteps)
		nNumStepsX = nMaxNumSteps;

	if(nNumStepsY < nMinNumStepsY)
		nNumStepsY = nMinNumStepsY;
	else if(nNumStepsY > nMaxNumSteps)
		nNumStepsY = nMaxNumSteps;

	const u32 nNumMainSamples = nNumStepsX * nNumStepsY;

	// Add space for the extra samples defined in metadata.
	const u32 nTotalNumSamples = nNumMainSamples + nNumAdditionalSamples;

	// Create array of water samples
	modelinfoAssert(m_pBuoyancyInfo->m_WaterSamples == NULL);
	m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[nTotalNumSamples];
	m_pBuoyancyInfo->m_nNumWaterSamples = (s16)nTotalNumSamples;

	const float fStepX = (vecBBoxMax.x - vecBBoxMin.x) / (float)(nNumStepsX);
	const float fStepY = (vecBBoxMax.y - vecBBoxMin.y) / (float)(nNumStepsY);		
	const float fCentreZ = vecBBoxMin.z + fSize;

	float fSizeTotal = fSize * nTotalNumSamples;

	int n = 0;
	float fX = vecBBoxMin.x + fStepX/2.0f;
	for(int i=0; i<nNumStepsX; i++, fX += fStepX)
	{
		float fY = vecBBoxMin.y + fStepY/2.0f;
		for(int j=0; j<nNumStepsY; j++, fY += fStepY, n++)
		{
			modelinfoAssert(n < nTotalNumSamples);
			// want to be very careful we don't overflow the array
			if(n < nTotalNumSamples)
			{
				m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset.Set(fX, fY, fCentreZ);
				m_pBuoyancyInfo->m_WaterSamples[n].m_fSize = fSize;
				m_pBuoyancyInfo->m_WaterSamples[n].m_nComponent = 0;

				CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
				if(pBuoyancyExtension)
				{
					m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset += pBuoyancyExtension->GetBuoyancySphereOffset();
					m_pBuoyancyInfo->m_WaterSamples[n].m_fSize *= pBuoyancyExtension->GetBuoyancySphereSizeScale();
				}
			}
		}
	}

	if(nNumAdditionalSamples > 0)
	{
		for(u32 i = 0; i < nNumAdditionalSamples; ++i)
		{
			u32 n = nNumMainSamples + i;

			modelinfoAssert(n < nTotalNumSamples);
			modelinfoAssert(pBuoyancyExtension);
			// Want to be very careful we don't overflow the array.
			if(n < nTotalNumSamples)
			{
				const CAdditionalVfxWaterSample* pAdditionalVfxSamples = pBuoyancyExtension->GetAdditionalVfxSamples();

				m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset = pAdditionalVfxSamples[i].GetPosition();
				m_pBuoyancyInfo->m_WaterSamples[n].m_fSize = pAdditionalVfxSamples[i].GetSize();
				m_pBuoyancyInfo->m_WaterSamples[n].m_nComponent = (u16)pAdditionalVfxSamples[i].GetComponent();
				// These samples are for VFX only and we don't want them affecting physics.
				m_pBuoyancyInfo->m_WaterSamples[n].m_fBuoyancyMult = 0.0f;
			}
		}
	}

	if(pHandling)
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant =
			ComputeBuoyancyConstantFromSubmergeValue(CHandlingDataMgr::GetHandlingData(m_handlingId)->m_fBuoyancyConstant, fSizeTotal);
	}
	else
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = 0.0f;
	}
	m_pBuoyancyInfo->m_fLiftMult = 0.0f;//VEHICLE_LIFT_MULT * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fKeelMult = VEHICLE_RUDDER_MULT * -GRAVITY / fSizeTotal;

    m_pBuoyancyInfo->m_fDragMultXY =  VEHICLE_DRAG_MULT_XY * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZUp =  VEHICLE_DRAG_MULT_ZUP * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZDown =  VEHICLE_DRAG_MULT_ZDOWN * -GRAVITY / fSizeTotal;
}

float CVehicleModelInfo::ComputeBuoyancyConstantFromSubmergeValue(float fSubmergeValue, float fSizeTotal, bool bRecompute)
{
	// If we are recomputing the buoyancy constant, we won't have access to fSizeTotal and will need to recover it first.
	if(!bRecompute)
	{
		return fSubmergeValue * -GRAVITY / fSizeTotal;
	}
	else
	{
		fSizeTotal = -GRAVITY * CHandlingDataMgr::GetHandlingData(m_handlingId)->m_fBuoyancyConstant / m_pBuoyancyInfo->m_fBuoyancyConstant;
		return fSubmergeValue * -GRAVITY / fSizeTotal;
	}
}

struct sMiniSampleData
{
	int nComponentIndex;
	float fBoundSize;
};
s32 MiniSampleCompareCB(const sMiniSampleData* e1, const sMiniSampleData* e2)
{
	return (e1->fBoundSize < e2->fBoundSize) ? 1 : -1;
}

const u32 knNumAdditionalSeaPlaneSamples = 6;

void CVehicleModelInfo::GenerateWaterSamplesForPlane()
{
	modelinfoAssert(m_pBuoyancyInfo);
	modelinfoAssert(m_pBuoyancyInfo->m_WaterSamples == NULL);

	CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(m_handlingId);
	modelinfoAssert(pHandling);

	bool bSamplesInitialised = false;

	// We might be adding some additional samples defined in vehicles.meta:
	int nNumAdditionalVfxSamples = 0;
	CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
	if(pBuoyancyExtension)
	{
		nNumAdditionalVfxSamples = pBuoyancyExtension->GetNumAdditionalVfxSamples();
	}

	// If this is a seaplane, we will be adding some buoyancy samples for the pontoons.
	CSeaPlaneHandlingData* pSeaPlaneHandling = pHandling->GetSeaPlaneHandlingData();
	u32 nNumAdditionalSeaPlaneSamples = 0;
	if(pSeaPlaneHandling)
	{
		nNumAdditionalSeaPlaneSamples = knNumAdditionalSeaPlaneSamples;
	}

	Vector3 vecBBoxMin, vecBBoxMax;
	GetVehicleBBoxForBuoyancy(vecBBoxMin, vecBBoxMax);

	// Vehicle buoyancy samples are always defined by the height of the plane.
	const float fSize = (vecBBoxMax.z - vecBBoxMin.z) / 2.0f;
	float fSizeTotal = 0.0f;

	u32 nNumMainSamples = 0;
	u32 nTotalNumSamples = 0;

	if(!GetIsBike() && GetFragType() && GetFragType()->GetPhysics(0) && GetFragType()->GetPhysics(0)->GetArchetype())
	{
		ASSERT_ONLY(float fLateralOffsetAccum = 0.0f);
		phBound* pBound = GetFragType()->GetPhysics(0)->GetArchetype()->GetBound();
		Assert(pBound);
		if(pBound->GetType()==phBound::COMPOSITE)
		{
			phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
			Assert(pCompositeBound);
			// Vector3 vecBBoxSize = VEC3V_TO_VECTOR3(pCompositeBound->GetBoundingBoxSize());

			atArray<sMiniSampleData> aMiniSamples;
			TUNE_FLOAT(fBoundSizeThreshold, 0.75f, 0.0f, 1.0f, 0.1f);
			//const float fBoundSizeThreshold = 0.75f; // As a fraction of the bounding box in each dimension.
			for(int i = 0; i < pCompositeBound->GetNumBounds(); ++i)
			{
				// For each bound which is larger than some fraction of the total bounding box, add a
				// buoyancy sample. Hopefully this should make sure we at least get samples for the
				// fuselage and the wings.
				phBound* pChildBound = pCompositeBound->GetBound(i);
				float fChildBoundMaxSize = Max(pChildBound->GetBoundingBoxSize().GetXf(), pChildBound->GetBoundingBoxSize().GetXf(),
					pChildBound->GetBoundingBoxSize().GetXf());
				if(pChildBound->GetVolume()>0.0f && fChildBoundMaxSize>fBoundSizeThreshold)
				{
					sMiniSampleData& newSample = aMiniSamples.Grow();
					// Record this component's index so that we create a sample for it below.
					newSample.nComponentIndex = i;
					// Also record the size of this bound so we can pick the largest ones later.
					newSample.fBoundSize = fChildBoundMaxSize;
				}
			}

			// Sort by volume.
			aMiniSamples.QSort(0, -1, MiniSampleCompareCB);

			// Create the water samples.
			modelinfoAssert(m_pBuoyancyInfo->m_WaterSamples == NULL);
			nNumMainSamples = aMiniSamples.GetCount();

			// Add space for the extra samples defined in metadata.
			nTotalNumSamples = nNumMainSamples + nNumAdditionalVfxSamples + nNumAdditionalSeaPlaneSamples;

			m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[nTotalNumSamples];
			m_pBuoyancyInfo->m_nNumWaterSamples = (s16)nTotalNumSamples;
			fSizeTotal = fSize * nTotalNumSamples;
			TUNE_FLOAT(fSampleSizeMultiplier, 7.0f, 0.0f, 20.0f, 0.01f);
			for(int n = 0; n < nNumMainSamples; ++n)
			{
				int nBoundId = aMiniSamples[n].nComponentIndex;
				// Want to be very careful we don't overflow the array.
				if(modelinfoVerifyf(n < nNumMainSamples, "Not enough space in water sample array."))
				{
					m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset.Set(VEC3V_TO_VECTOR3(pCompositeBound->GetBound(nBoundId)->GetCentroidOffset()));
					m_pBuoyancyInfo->m_WaterSamples[n].m_fSize = fSize*fSampleSizeMultiplier/nTotalNumSamples;
					m_pBuoyancyInfo->m_WaterSamples[n].m_nComponent = static_cast<u16>(nBoundId);
					m_pBuoyancyInfo->m_WaterSamples[n].m_fBuoyancyMult = 1.0f;

					m_pBuoyancyInfo->m_fLiftMult = 0.0f;//VEHICLE_LIFT_MULT * -GRAVITY / fSizeTotal;
					m_pBuoyancyInfo->m_fKeelMult = VEHICLE_RUDDER_MULT * -GRAVITY / fSizeTotal;

					m_pBuoyancyInfo->m_fDragMultXY =  PLANE_DRAG_MULT_XY * -GRAVITY / fSizeTotal;
					m_pBuoyancyInfo->m_fDragMultZUp =  PLANE_DRAG_MULT_ZUP * -GRAVITY / fSizeTotal;
					m_pBuoyancyInfo->m_fDragMultZDown =  PLANE_DRAG_MULT_ZDOWN * -GRAVITY / fSizeTotal;

					ASSERT_ONLY(fLateralOffsetAccum += (m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset.x));
				}
			}
		}

		// Assert that the buoyancy spheres are symmetric about the fuselage.
		modelinfoAssertf(fabs(fLateralOffsetAccum) < 0.07f*(vecBBoxMax.x-vecBBoxMin.x),
			"Buoyancy spheres not distributed evenly about fuselage. Offset=%5.3f", fLateralOffsetAccum);

		// We're done.
		bSamplesInitialised = true;
	}

	// If we didn't create valid samples above, just set the plane's buoyancy samples up using the entity bounding box method:

	if(!bSamplesInitialised)
	{
		int nMaxNumSteps = 0;
		const int nMinNumSteps = 2;
		nMaxNumSteps = 3;

		// Rows / columns of samples in XY are decided by the number of samples we can fit in each dimension
		int nNumStepsX = (int) ((vecBBoxMax.x - vecBBoxMin.x)  / (2.0f*fSize));
		int nNumStepsY = (int) ((vecBBoxMax.y - vecBBoxMin.y)  / (2.0f*fSize));

		// Clamp steps to min/max values
		if(nNumStepsX < nMinNumSteps)
			nNumStepsX = nMinNumSteps;
		else if(nNumStepsX > nMaxNumSteps)
			nNumStepsX = nMaxNumSteps;

		if(nNumStepsY < nMinNumSteps)
			nNumStepsY = nMinNumSteps;
		else if(nNumStepsY > nMaxNumSteps)
			nNumStepsY = nMaxNumSteps;

		nNumMainSamples = nNumStepsX * nNumStepsY;

		// Add space for the extra samples defined in metadata.
		nTotalNumSamples = nNumMainSamples + nNumAdditionalVfxSamples + nNumAdditionalSeaPlaneSamples;

		// Create array of water samples
		if(m_pBuoyancyInfo->m_WaterSamples)
		{
			delete [] m_pBuoyancyInfo->m_WaterSamples;
		}
		m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[nTotalNumSamples];
		m_pBuoyancyInfo->m_nNumWaterSamples = (s16)nTotalNumSamples;

		const float fStepX = (vecBBoxMax.x - vecBBoxMin.x) / (float)(nNumStepsX);
		const float fStepY = (vecBBoxMax.y - vecBBoxMin.y) / (float)(nNumStepsY);		
		const float fCentreZ = vecBBoxMin.z + fSize;

		fSizeTotal = fSize * nTotalNumSamples;

		int n = 0;
		float fX = vecBBoxMin.x + fStepX/2.0f;
		for(int i=0; i<nNumStepsX; i++, fX += fStepX)
		{
			float fY = vecBBoxMin.y + fStepY/2.0f;
			for(int j=0; j<nNumStepsY; j++, fY += fStepY, n++)
			{
				modelinfoAssert(n < nNumMainSamples);
				// want to be very careful we don't overflow the array
				if(n < nNumMainSamples)
				{
					m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset.Set(fX, fY, fCentreZ);
					m_pBuoyancyInfo->m_WaterSamples[n].m_fSize = fSize;
					m_pBuoyancyInfo->m_WaterSamples[n].m_nComponent = 0;
				}
			}
		}

		m_pBuoyancyInfo->m_fLiftMult = 0.0f;//VEHICLE_LIFT_MULT * -GRAVITY / fSizeTotal;
		m_pBuoyancyInfo->m_fKeelMult = VEHICLE_RUDDER_MULT * -GRAVITY / fSizeTotal;

		m_pBuoyancyInfo->m_fDragMultXY =  VEHICLE_DRAG_MULT_XY * -GRAVITY / fSizeTotal;
		m_pBuoyancyInfo->m_fDragMultZUp =  VEHICLE_DRAG_MULT_ZUP * -GRAVITY / fSizeTotal;
		m_pBuoyancyInfo->m_fDragMultZDown =  VEHICLE_DRAG_MULT_ZDOWN * -GRAVITY / fSizeTotal;
	} // !bSamplesInitialised

	if(nNumAdditionalVfxSamples > 0)
	{
		for(u32 i = 0; i < nNumAdditionalVfxSamples; ++i)
		{
			u32 n = nNumMainSamples + i;

			modelinfoAssert(n < nTotalNumSamples);
			modelinfoAssert(pBuoyancyExtension);
			// Want to be very careful we don't overflow the array.
			if(n < nTotalNumSamples)
			{
				const CAdditionalVfxWaterSample* pAdditionalVfxSamples = pBuoyancyExtension->GetAdditionalVfxSamples();

				m_pBuoyancyInfo->m_WaterSamples[n].m_vSampleOffset = pAdditionalVfxSamples[i].GetPosition();
				m_pBuoyancyInfo->m_WaterSamples[n].m_fSize = pAdditionalVfxSamples[i].GetSize();
				m_pBuoyancyInfo->m_WaterSamples[n].m_nComponent = (u16)pAdditionalVfxSamples[i].GetComponent();
				// These samples are for VFX only and we don't want them affecting physics.
				m_pBuoyancyInfo->m_WaterSamples[n].m_fBuoyancyMult = 0.0f;
			}
		}
	}

	if(nNumAdditionalSeaPlaneSamples > 0)
	{
		GeneratePontoonSamples( pSeaPlaneHandling, knNumAdditionalSeaPlaneSamples, nNumMainSamples + nNumAdditionalVfxSamples, nTotalNumSamples );
	}

	if(pHandling)
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = 2.0f*CHandlingDataMgr::GetHandlingData(m_handlingId)->m_fBuoyancyConstant * -GRAVITY / fSizeTotal;
	}
	else
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = 0.0f;
	}
}

void CVehicleModelInfo::GeneratePontoonSamples( CSeaPlaneHandlingData* pSeaPlaneHandling, u32 numPontoonSamples, u32 sampleStartIndex, u32 nTotalNumSamples )
{

	const u32 nPontoonComponentIdL = pSeaPlaneHandling->m_fLeftPontoonComponentId;
	const u32 nPontoonComponentIdR = pSeaPlaneHandling->m_fRightPontoonComponentId;

	if( AssertVerify( GetFragType() && GetFragType()->GetPhysics( 0 ) && GetFragType()->GetPhysics( 0 )->GetArchetype() ) )
	{
		phBound* pBound = GetFragType()->GetPhysics( 0 )->GetArchetype()->GetBound();
		if( AssertVerify( pBound && pBound->GetType() == phBound::COMPOSITE ) )
		{
			phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>( pBound );

			// Get extents of the bounding boxes for each pontoon and use these to decide where to place the samples.
			phBound* pPontoonBoundL = pCompositeBound->GetBound( nPontoonComponentIdL );
			phBound* pPontoonBoundR = pCompositeBound->GetBound( nPontoonComponentIdR );

			float fPontoonHalfLength = 0.5f*MaxElement( pPontoonBoundL->GetBoundingBoxSize() ).Getf();
			float fPontoonHalfLengthScaled = fPontoonHalfLength * pSeaPlaneHandling->m_fPontoonLengthFractionForSamples;
			
			Vector3 vPontoonBoundCentreL = VEC3V_TO_VECTOR3( pPontoonBoundL->GetCentroidOffset() );
			Vector3 vPontoonBoundCentreR = VEC3V_TO_VECTOR3( pPontoonBoundR->GetCentroidOffset() );

			float fPontoonFrontL = vPontoonBoundCentreL.y + fPontoonHalfLength;
			if( GetIsHeli() )
			{
				fPontoonFrontL = vPontoonBoundCentreL.y + fPontoonHalfLength * pSeaPlaneHandling->m_fPontoonLengthFractionForSamples;
			}

			float fPontoonFrontR = vPontoonBoundCentreR.y + fPontoonHalfLength;
			if( GetIsHeli() )
			{
				fPontoonFrontR = vPontoonBoundCentreR.y + fPontoonHalfLength * pSeaPlaneHandling->m_fPontoonLengthFractionForSamples;
			}

			float aSampleCoordsX[ knNumAdditionalSeaPlaneSamples ] =
			{
				vPontoonBoundCentreL.x, vPontoonBoundCentreL.x, vPontoonBoundCentreL.x,
				vPontoonBoundCentreR.x, vPontoonBoundCentreR.x, vPontoonBoundCentreR.x
			};

			float aSampleCoordsY[ knNumAdditionalSeaPlaneSamples ] =
			{
				fPontoonFrontL, fPontoonFrontL - fPontoonHalfLengthScaled, fPontoonFrontL - 2.0f*fPontoonHalfLengthScaled,
				fPontoonFrontR, fPontoonFrontR - fPontoonHalfLengthScaled, fPontoonFrontR - 2.0f*fPontoonHalfLengthScaled
			};
			float aSampleCoordsZ[ knNumAdditionalSeaPlaneSamples ] =
			{
				vPontoonBoundCentreL.z, vPontoonBoundCentreL.z, vPontoonBoundCentreL.z,
				vPontoonBoundCentreR.z, vPontoonBoundCentreR.z, vPontoonBoundCentreR.z
			};
			float aSampleSizes[ knNumAdditionalSeaPlaneSamples ] =
			{
				pSeaPlaneHandling->m_fPontoonSampleSizeFront, pSeaPlaneHandling->m_fPontoonSampleSizeMiddle, pSeaPlaneHandling->m_fPontoonSampleSizeRear,
				pSeaPlaneHandling->m_fPontoonSampleSizeFront, pSeaPlaneHandling->m_fPontoonSampleSizeMiddle, pSeaPlaneHandling->m_fPontoonSampleSizeRear
			};
			u32 aSampleComponentIds[ knNumAdditionalSeaPlaneSamples ] =
			{
				nPontoonComponentIdL, nPontoonComponentIdL, nPontoonComponentIdL,
				nPontoonComponentIdR, nPontoonComponentIdR, nPontoonComponentIdR
			};

			float xOffset = 0.0f;
			float zOffset = 0.0f;
			if( pPontoonBoundL == pPontoonBoundR )
			{
				static dev_float seaplanePontoonOffset = 0.5f;
				xOffset = seaplanePontoonOffset;

				if( pPontoonBoundL == 0 )
				{
					static dev_float seaplanePontoonZOffset = -0.95f;
					zOffset = seaplanePontoonZOffset;
				}
			}
			//if( GetIsHeli() )
			//{
			//	static dev_float seaHeliBuoyancyXOffset = -0.4f;
			//	xOffset = seaHeliBuoyancyXOffset;
			//}

			for( u32 i = 0; i < numPontoonSamples; ++i )
			{
				u32 n = sampleStartIndex + i;

				modelinfoAssert( n < nTotalNumSamples );
				// Want to be very careful we don't overflow the array.
				if( n < nTotalNumSamples )
				{
					float sampleCoordX = aSampleCoordsX[ i ];
					float sampleCoordZ = aSampleCoordsZ[ i ] + zOffset;
					if( i >= numPontoonSamples / 2 )
					{
						sampleCoordX -= xOffset;
					}
					else
					{
						sampleCoordX += xOffset;
					}

					m_pBuoyancyInfo->m_WaterSamples[ n ].m_vSampleOffset = Vector3( sampleCoordX, aSampleCoordsY[ i ], sampleCoordZ );
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_fSize = aSampleSizes[ i ];
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_nComponent = (u16)aSampleComponentIds[ i ];
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_fBuoyancyMult = pSeaPlaneHandling->m_fPontoonBuoyConst;
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_bPontoon = 1;
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_bKeel = 1;
				}
			}
		}
	}
}

void CVehicleModelInfo::GenerateWaterSamplesForBoat()
{
	CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(m_handlingId);

	modelinfoAssert(pHandling);
	CBoatHandlingData* pBoatHandling = pHandling->GetBoatHandlingData();
	modelinfoAssert(pBoatHandling);

	// Decide here if we want the generic boat buoyancy model or a specialised version for dingys (with buoyancy spheres
	// around the outside where the inflatable part is).
	bool bDinghySetup = pBoatHandling->m_fDinghySphereBuoyConst > 0.0f;

	// calculate how many samples we want to use
	const int nSampleStepsX = 5;
	const int nSampleStepsY = 5;

	// We want the jet-ski models to be self-righting and so we will add two extra buoyancy spheres
	// outside of the craft's hull.
	bool bAddSelfRightingSamples = false;
	int nNumAdditionalSamples = 0;
	(void)sizeof(nNumAdditionalSamples);
	if(pHandling->hFlags & HF_SELF_RIGHTING_IN_WATER)
	{
		bAddSelfRightingSamples = true;
		nNumAdditionalSamples = 2;
	}

	Assertf(!(bDinghySetup&&bAddSelfRightingSamples), "%s is configured to be set up as a dinghy and to add self-righting samples. Is this right?", m_gameName);

	const u32 knMaxNumMainSamples = 25;
	const u32 knNumSelfRightingSamples = 2;
	const u32 knNumDinghySamples = 7;
	const u32 knNumKeelSamples = 3;
	const u32 knSizeOfSampleArray = knMaxNumMainSamples+knNumSelfRightingSamples+knNumDinghySamples+knNumKeelSamples;
	atFixedArray<CWaterSample, knSizeOfSampleArray> tempWaterSamples(knSizeOfSampleArray);


	Vector3 vecBoxMin , vecBoxMax;
	GetVehicleBBoxForBuoyancy(vecBoxMin,vecBoxMax);
	vecBoxMin.x *= pBoatHandling->m_fBoxSideMult;
	vecBoxMax.x *= pBoatHandling->m_fBoxSideMult;
	vecBoxMin.y *= pBoatHandling->m_fBoxRearMult;
	vecBoxMax.y *= pBoatHandling->m_fBoxFrontMult;

	float fStartX = -0.5f * (vecBoxMax.x - vecBoxMin.x);
	float fStepSizeX = (vecBoxMax.x - vecBoxMin.x) / float(nSampleStepsX - 1);

	float fStartY = -0.5f * (vecBoxMax.y - vecBoxMin.y);
	float fStepSizeY = (vecBoxMax.y - vecBoxMin.y) / float(nSampleStepsY - 1);

	// Going to test against boat bounds so samples are definitely corresponding to boat geometry
	phIntersection intersection;
	phSegment seg;
	//phBound* pBoatBound = m_pPhysicsArch->GetBound();
	if (!GetFragType())
	{
		vehicleAssertf(0, "Failed to get fragment type for vehicle %s", m_gameName);
		return;
	}
	phBound* pBoatBound = GetFragType()->GetPhysics(0)->GetArchetype()->GetBound();

	int nSample = 0;
	float fSizeTotal = 0.0f;
	phShapeTest<phShapeProbe> shapeTest;

	int numWaterSampleRows = 0;
	atFixedArray<int, nSampleStepsY> waterSampleRowFirstIndex(nSampleStepsY);

	for(int i=0; i<nSampleStepsY; i++)
	{
		waterSampleRowFirstIndex[i] = -1;
	}

	for(int j=0; j<nSampleStepsY; j++)
	{
		bool waterSampleRowFirstIndexSet = false;

		for(int i=0; i<nSampleStepsX; i++)
		{
			Vector3 vecTop(fStartX + i*fStepSizeX, fStartY + j*fStepSizeY, pBoatHandling->m_fSampleTop);
			Vector3 vecBottom(fStartX + i*fStepSizeX, fStartY + j*fStepSizeY, vecBoxMin.z+pBoatHandling->m_fSampleBottomTestCorrection*(vecBoxMax.z-vecBoxMin.z));
			seg.Set(vecBottom,vecTop);

			intersection.Reset();

			shapeTest.InitProbe(seg, &intersection);
			if(shapeTest.TestOneObject(*pBoatBound))
			{
				Vector3 vecSamplePos = RCC_VECTOR3(intersection.GetPosition());
				vecSamplePos.z -= pBoatHandling->m_fSampleBottom;

				tempWaterSamples[nSample].m_vSampleOffset.Set(0.5f*(vecTop + vecSamplePos));
				tempWaterSamples[nSample].m_fSize = 0.5f*rage::Abs(vecTop.z - vecSamplePos.z);
				fSizeTotal += tempWaterSamples[nSample].m_fSize;

				// If we are setting this boat up as a dinghy, it will be supported by additional buoyancy spheres
				// around the outside. We still need the generic boat spheres for VFX, keel forces, etc. We just don't want them to supply
				// much buoyancy force.
				if(bDinghySetup)
				{
					tempWaterSamples[nSample].m_fBuoyancyMult = DINGHY_BUOYANCY_MULT_FOR_GENERIC_SAMPLES;
				}

				if (waterSampleRowFirstIndexSet==false)
				{
					waterSampleRowFirstIndex[numWaterSampleRows++] = nSample;
					waterSampleRowFirstIndexSet = true;
				}

				nSample++;
			}

			if(nSample > knMaxNumMainSamples)
			{
				modelinfoAssert(false);
				break;
			}
		}
		if(nSample > knMaxNumMainSamples)
			break;
	}

	// Add the extra buoyancy spheres here. These are outside the craft's hull as if coming off an
	// imaginary mast.
	static dev_float fSelfRightingMastHeight = 1.5f;
	static dev_float fSelfRightingSampleSize = 0.1f;
	static dev_float fSelfRightingMastOffset = 0.4f;
	static dev_float fSelfRightingMultiplier = 3.0f;
	if(bAddSelfRightingSamples)
	{
		tempWaterSamples[nSample].m_vSampleOffset.Set(Vector3(fSelfRightingMastOffset, 0.0f, fSelfRightingMastHeight));
		tempWaterSamples[nSample].m_fSize = fSelfRightingSampleSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fSelfRightingMultiplier;
		++nSample;

		tempWaterSamples[nSample].m_vSampleOffset.Set(Vector3(-fSelfRightingMastOffset, 0.0f, fSelfRightingMastHeight));
		tempWaterSamples[nSample].m_fSize = fSelfRightingSampleSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fSelfRightingMultiplier;
		++nSample;
	}
	physicsAssert(nSample<=knSizeOfSampleArray);

	// Add the extra dinghy inflatable side spheres here.
	static dev_float fDinghySphereSize = 0.4f;
	float fDinghySphereBuoyConst = pBoatHandling->m_fDinghySphereBuoyConst;
	if(bDinghySetup)
	{
		// From left to right, back to front (like main samples).
		tempWaterSamples[nSample].m_vSampleOffset.Set(-1.15f, -3.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		tempWaterSamples[nSample].m_vSampleOffset.Set(+1.15f, -3.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		// Second row from back.
		tempWaterSamples[nSample].m_vSampleOffset.Set(-1.15f, -1.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		tempWaterSamples[nSample].m_vSampleOffset.Set(+1.15f, -1.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		// Front row not prow.
		tempWaterSamples[nSample].m_vSampleOffset.Set(-1.15f, +1.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		tempWaterSamples[nSample].m_vSampleOffset.Set(+1.15f, +1.0f, 0.4f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;

		// Prow sample.
		tempWaterSamples[nSample].m_vSampleOffset.Set(0.00f, +3.5f, 0.75f);
		tempWaterSamples[nSample].m_fSize = fDinghySphereSize;
		tempWaterSamples[nSample].m_fBuoyancyMult = fDinghySphereBuoyConst;
		++nSample;
	}
	physicsAssert(nSample<=knSizeOfSampleArray);

    // Add spheres for use by the keel, only add if the size is greater then 0
    if(pBoatHandling->m_fKeelSphereSize > 0.0f)
    {
        tempWaterSamples[nSample].m_vSampleOffset.Set(Vector3( 0.0f, vecBoxMax.y, 0.0f));
        tempWaterSamples[nSample].m_fSize = pBoatHandling->m_fKeelSphereSize;
        tempWaterSamples[nSample].m_fBuoyancyMult = 0.0f;
        tempWaterSamples[nSample].m_bKeel = true;
        tempWaterSamples[nSample].m_nComponent = 0;
        ++nSample;

        tempWaterSamples[nSample].m_vSampleOffset.Set(Vector3( 0.0f, 0.0f, 0.0f));
        tempWaterSamples[nSample].m_fSize = pBoatHandling->m_fKeelSphereSize;
        tempWaterSamples[nSample].m_fBuoyancyMult = 0.0f;
        tempWaterSamples[nSample].m_bKeel = true;
        tempWaterSamples[nSample].m_nComponent = 0;
        ++nSample;

        tempWaterSamples[nSample].m_vSampleOffset.Set(Vector3( 0.0f, vecBoxMin.y, 0.0f));
        tempWaterSamples[nSample].m_fSize = pBoatHandling->m_fKeelSphereSize;
        tempWaterSamples[nSample].m_fBuoyancyMult = 0.0f;
        tempWaterSamples[nSample].m_bKeel = true;
        tempWaterSamples[nSample].m_nComponent = 0;
		++nSample;
    }
	physicsAssert(nSample<=knSizeOfSampleArray);

	const int iSamplesUsed = nSample;

	m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[iSamplesUsed];
	m_pBuoyancyInfo->m_nNumWaterSamples = (s16)iSamplesUsed;

	m_pBuoyancyInfo->m_nNumBoatWaterSampleRows = (s8)numWaterSampleRows;
	m_pBuoyancyInfo->m_nBoatWaterSampleRowIndices = rage_new s8[numWaterSampleRows];
	for (int i=0; i<numWaterSampleRows; i++)
	{
		m_pBuoyancyInfo->m_nBoatWaterSampleRowIndices[i] = (s8)waterSampleRowFirstIndex[i];
	}

	// Copy temp water samples to new array
	for(int iTempSampleIndex = 0 ; iTempSampleIndex < iSamplesUsed; iTempSampleIndex++)
    { 
        /* *** Don't need to do this with the new CBuoyancy::Process() code ***
		if(pBoatHandling->m_fKeelSphereSize <= 0.0f)
        {
            tempWaterSamples[iTempSampleIndex].m_bKeel = true;
        }*/

		m_pBuoyancyInfo->m_WaterSamples[iTempSampleIndex].Set(tempWaterSamples[iTempSampleIndex]);
	}

// 	m_nNumSamplesToUse = (s16)rage::Min(nSample, nNumSamples);
// 	m_nNumSamplesInArray = m_nNumSamplesToUse;

	if(pHandling)
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = pHandling->m_fBuoyancyConstant * -GRAVITY / fSizeTotal;
	}
	else
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = 0.0f;
	}
	m_pBuoyancyInfo->m_fLiftMult = pBoatHandling->m_fAquaplaneForce * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fKeelMult = pHandling->m_fTractionCurveLateral * -GRAVITY / fSizeTotal * pBoatHandling->m_fTractionMultiplier;

	m_pBuoyancyInfo->m_fDragMultXY = pBoatHandling->m_vecMoveResistance.GetXf() * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZUp = pBoatHandling->m_vecMoveResistance.GetYf() * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZDown = pBoatHandling->m_vecMoveResistance.GetZf() * -GRAVITY / fSizeTotal;
}

void CVehicleModelInfo::GenerateWaterSamplesForSubmarine()
{
	// Use vehicle water sample code but override the drag values
	GenerateWaterSamplesForVehicle();

	CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(m_handlingId);
	modelinfoAssert(pHandling);
	const CSubmarineHandlingData* pSubHandling = pHandling->GetSubmarineHandlingData();

	if(physicsVerifyf(pSubHandling,"Unexpected NULL sub handling for %s",GetModelName()))
	{
		m_pBuoyancyInfo->m_fDragMultXY = pSubHandling->GetMoveResXY();
		m_pBuoyancyInfo->m_fDragMultZUp = m_pBuoyancyInfo->m_fDragMultZDown = pSubHandling->GetMoveResZ();
	}

	// for the submarine car make sure the samples are evenly spaced so we don't get an extra torque
	if( GetIsSubmarineCar() )
	{
		int numSamples = m_pBuoyancyInfo->m_nNumWaterSamples;
		int halfNumSamples = numSamples / 2; 

		for( int i = 0; i < halfNumSamples; i++ )
		{
			int index = halfNumSamples - ( 1 + i );

			if( index < i )
			{
				break;
			}

			if( index == i )
			{
				m_pBuoyancyInfo->m_WaterSamples[ i ].m_vSampleOffset.y = 0.0f;
				m_pBuoyancyInfo->m_WaterSamples[ i + halfNumSamples ].m_vSampleOffset.y = 0.0f;
			}
			else
			{
				m_pBuoyancyInfo->m_WaterSamples[ index ].m_vSampleOffset.y = -m_pBuoyancyInfo->m_WaterSamples[ i ].m_vSampleOffset.y;
				m_pBuoyancyInfo->m_WaterSamples[ index + halfNumSamples ].m_vSampleOffset.y = -m_pBuoyancyInfo->m_WaterSamples[ i + halfNumSamples ].m_vSampleOffset.y;
			}
		}
	}
}

void CVehicleModelInfo::GenerateWaterSamplesForHeli()
{
	modelinfoAssert( m_pBuoyancyInfo );

	// We might be adding some additional samples defined in vehicles.meta:
	int nNumAdditionalSamples = 0;
	CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
	if( pBuoyancyExtension )
	{
		nNumAdditionalSamples = pBuoyancyExtension->GetNumAdditionalVfxSamples();
	}

	Vector3 vecBBoxMin, vecBBoxMax;
	GetVehicleBBoxForBuoyancy( vecBBoxMin, vecBBoxMax );

	// Vehicles will likely have a composite bound. Assume the first child bound of this composite is the chassis bound
	// and therefore the one which we should apply the samples to. This avoids the problem of buoyancy samples being
	// created outside the bounds of vehicles like the "rhino" where the turret expands the bounding box.
	if( !GetIsBike() && GetFragType() && GetFragType()->GetPhysics( 0 ) && GetFragType()->GetPhysics( 0 )->GetArchetype() )
	{
		phBound* pBound = GetFragType()->GetPhysics( 0 )->GetArchetype()->GetBound();
		if( pBound->GetType() == phBound::COMPOSITE )
		{
			// This should be the chassis bound?
			pBound = static_cast<phBoundComposite*>( pBound )->GetBound( 0 );

			Vector3 vRootBoundBBoxMin = VEC3V_TO_VECTOR3( pBound->GetBoundingBoxMin() );
			Vector3 vRootBoundBBoxMax = VEC3V_TO_VECTOR3( pBound->GetBoundingBoxMax() );

			// Don't use the root bound's bounding box size if we suspect this isn't the main bound. An example
			// would be a pickup truck where the root bound only covers the cab.
			if( vRootBoundBBoxMax.x - vRootBoundBBoxMin.x > 0.7f*( vecBBoxMax.x - vecBBoxMin.x ) &&
				vRootBoundBBoxMax.y - vRootBoundBBoxMin.y > 0.7f*( vecBBoxMax.y - vecBBoxMin.y ) )
			{
				// We want to keep the full bounding box height though as things like lorry containers / bikes can have
				// bounds which don't quite follow our assumptions about the chassis bound.
				vecBBoxMin.x = vRootBoundBBoxMin.x;
				vecBBoxMax.x = vRootBoundBBoxMax.x;
				vecBBoxMin.y = vRootBoundBBoxMin.y;
				vecBBoxMax.y = vRootBoundBBoxMax.y;
			}
		}
	}

	int nMaxNumSteps = 3;
	int nMinNumStepsX = 2;
	int nMinNumStepsY = 2;

	CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData( m_handlingId );
	modelinfoAssert( pHandling );

	// If this is a seaplane, we will be adding some buoyancy samples for the pontoons.
	CSeaPlaneHandlingData* pSeaPlaneHandling = pHandling->GetSeaPlaneHandlingData();
	u32 nNumAdditionalSeaPlaneSamples = 0;

	// Vehicle buoyancy samples are always defined by the height of the car
	const float fSize = ( vecBBoxMax.z - vecBBoxMin.z ) / 2.0f;
	static dev_float sfSeaHeliBuoyancySampleSizeScale = 0.5f;
	float fBuoyancySizeScale = 1.0f;

	if( pSeaPlaneHandling )
	{
		nNumAdditionalSeaPlaneSamples = knNumAdditionalSeaPlaneSamples;
		fBuoyancySizeScale = sfSeaHeliBuoyancySampleSizeScale;
	}

	// Rows / columns of samples in XY are decided by the number of samples we can fit in each dimension
	int nNumStepsX = (int)( ( vecBBoxMax.x - vecBBoxMin.x ) / ( 2.0f*fSize ) );
	int nNumStepsY = (int)( ( vecBBoxMax.y - vecBBoxMin.y ) / ( 2.0f*fSize ) );

	// Clamp steps to min/max values
	if( nNumStepsX < nMinNumStepsX )
		nNumStepsX = nMinNumStepsX;
	else if( nNumStepsX > nMaxNumSteps )
		nNumStepsX = nMaxNumSteps;

	if( nNumStepsY < nMinNumStepsY )
		nNumStepsY = nMinNumStepsY;
	else if( nNumStepsY > nMaxNumSteps )
		nNumStepsY = nMaxNumSteps;

	const u32 nNumMainSamples = nNumStepsX * nNumStepsY;

	// Add space for the extra samples defined in metadata.
	const u32 nTotalNumSamples = nNumMainSamples + nNumAdditionalSamples + nNumAdditionalSeaPlaneSamples;

	// Create array of water samples
	modelinfoAssert( m_pBuoyancyInfo->m_WaterSamples == NULL );
	m_pBuoyancyInfo->m_WaterSamples = rage_new CWaterSample[ nTotalNumSamples ];
	m_pBuoyancyInfo->m_nNumWaterSamples = (s16)nTotalNumSamples;

	const float fStepX = ( vecBBoxMax.x - vecBBoxMin.x ) / (float)( nNumStepsX );
	const float fStepY = ( vecBBoxMax.y - vecBBoxMin.y ) / (float)( nNumStepsY );
	const float fCentreZ = vecBBoxMin.z + fSize;

	float fSizeTotal = fSize * nTotalNumSamples;

	int n = 0;
	float fX = vecBBoxMin.x + fStepX / 2.0f;
	for( int i = 0; i < nNumStepsX; i++, fX += fStepX )
	{
		float fY = vecBBoxMin.y + fStepY / 2.0f;
		for( int j = 0; j < nNumStepsY; j++, fY += fStepY, n++ )
		{
			modelinfoAssert( n < nTotalNumSamples );
			// want to be very careful we don't overflow the array
			if( n < nTotalNumSamples )
			{
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_vSampleOffset.Set( fX, fY, fCentreZ );
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_fSize = fSize * fBuoyancySizeScale;
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_nComponent = 0;

				CVehicleModelInfoBuoyancy* pBuoyancyExtension = GetExtension<CVehicleModelInfoBuoyancy>();
				if( pBuoyancyExtension )
				{
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_vSampleOffset += pBuoyancyExtension->GetBuoyancySphereOffset();
					m_pBuoyancyInfo->m_WaterSamples[ n ].m_fSize *= pBuoyancyExtension->GetBuoyancySphereSizeScale();
				}
			}
		}
	}

	if( nNumAdditionalSamples > 0 )
	{
		for( u32 i = 0; i < nNumAdditionalSamples; ++i )
		{
			u32 n = nNumMainSamples + i;

			modelinfoAssert( n < nTotalNumSamples );
			modelinfoAssert( pBuoyancyExtension );
			// Want to be very careful we don't overflow the array.
			if( n < nTotalNumSamples )
			{
				const CAdditionalVfxWaterSample* pAdditionalVfxSamples = pBuoyancyExtension->GetAdditionalVfxSamples();

				m_pBuoyancyInfo->m_WaterSamples[ n ].m_vSampleOffset = pAdditionalVfxSamples[ i ].GetPosition();
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_fSize = pAdditionalVfxSamples[ i ].GetSize();
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_nComponent = (u16)pAdditionalVfxSamples[ i ].GetComponent();
				// These samples are for VFX only and we don't want them affecting physics.
				m_pBuoyancyInfo->m_WaterSamples[ n ].m_fBuoyancyMult = 0.0f;
			}
		}
	}

	if( nNumAdditionalSeaPlaneSamples > 0 )
	{
		GeneratePontoonSamples( pSeaPlaneHandling, knNumAdditionalSeaPlaneSamples, nNumMainSamples + nNumAdditionalSamples, nTotalNumSamples );
	}

	if( pHandling )
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant =
			ComputeBuoyancyConstantFromSubmergeValue( CHandlingDataMgr::GetHandlingData( m_handlingId )->m_fBuoyancyConstant, fSizeTotal );
	}
	else
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant = 0.0f;
	}
	m_pBuoyancyInfo->m_fLiftMult = 0.0f;//VEHICLE_LIFT_MULT * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fKeelMult = VEHICLE_RUDDER_MULT * -GRAVITY / fSizeTotal;

	m_pBuoyancyInfo->m_fDragMultXY = VEHICLE_DRAG_MULT_XY * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZUp = VEHICLE_DRAG_MULT_ZUP * -GRAVITY / fSizeTotal;
	m_pBuoyancyInfo->m_fDragMultZDown = VEHICLE_DRAG_MULT_ZDOWN * -GRAVITY / fSizeTotal;
}

void CVehicleModelInfo::GetVehicleBBoxForBuoyancy(Vector3& vecBBoxMin, Vector3& vecBBoxMax)
{
	// for heli, want to get a bounding box that doesn't include the main rotor or the tail
	if(GetIsRotaryAircraft())
	{
		phBoundComposite* pBoundComp = GetFragType()->GetPhysics(0)->GetCompositeBounds();

		vecBBoxMin.Set(LARGE_FLOAT, LARGE_FLOAT, LARGE_FLOAT);
		vecBBoxMax.Set(-LARGE_FLOAT, -LARGE_FLOAT, -LARGE_FLOAT);
		Vector3 vecTemp, vecTemp2;

		for(int i=0; i<GetFragType()->GetPhysics(0)->GetNumChildren(); i++)
		{
			if(GetFragType()->GetPhysics(0)->GetAllChildren()[i]->GetOwnerGroupPointerIndex()==0 && pBoundComp->GetBound(i))
			{
				RCC_MATRIX34(pBoundComp->GetCurrentMatrix(i)).Transform(VEC3V_TO_VECTOR3(pBoundComp->GetBound(i)->GetBoundingBoxMin()), vecTemp);
				vecTemp2.Set(vecBBoxMin);
				vecBBoxMin.Min(vecTemp, vecTemp2);

				RCC_MATRIX34(pBoundComp->GetCurrentMatrix(i)).Transform(VEC3V_TO_VECTOR3(pBoundComp->GetBound(i)->GetBoundingBoxMax()), vecTemp);
				vecTemp2.Set(vecBBoxMax);
				vecBBoxMax.Max(vecTemp, vecTemp2);
			}
		}
	}
	else
	{
		vecBBoxMin = GetBoundingBoxMin();
		vecBBoxMax = GetBoundingBoxMax();
	}
}

const CVehicleScenarioLayoutInfo* CVehicleModelInfo::GetVehicleScenarioLayoutInfo() const
{
	if(m_uVehicleScenarioLayoutInfoHash > 0)
	{
		return CVehicleMetadataMgr::GetInstance().GetVehicleScenarioLayoutInfo(m_uVehicleScenarioLayoutInfoHash);
	}

	return NULL;
}

const CVehicleLayoutInfo* CVehicleModelInfo::GetVehicleLayoutInfo() const
{
	return m_pVehicleLayoutInfo;
}

const CPOVTuningInfo* CVehicleModelInfo::GetPOVTuningInfo() const
{
	return m_pPOVTuningInfo;
}

const CVehicleCoverBoundOffsetInfo* CVehicleModelInfo::GetVehicleCoverBoundOffsetInfo() const
{
	return m_pVehicleCoverBoundOffsetInfo;
}

const CFirstPersonDriveByLookAroundData* CVehicleModelInfo::GetFirstPersonLookAroundData(s32 iSeat) const
{
	if(iSeat < m_apFirstPersonLookAroundData.GetCount())
	{
		return m_apFirstPersonLookAroundData[iSeat];
	}

	return NULL;
}

const CVehicleExplosionInfo* CVehicleModelInfo::GetVehicleExplosionInfo() const
{
	return m_pVehicleExplosionInfo;
}

const CVehicleSeatInfo* CVehicleModelInfo::GetSeatInfo(int iSeatIndex) const
{
	modelinfoAssert(m_data);
	int iSeatIndexOnLayout = m_data->m_SeatInfo.GetLayoutSeatIndex(iSeatIndex); 
	if (iSeatIndexOnLayout > -1)
	{
		const CVehicleLayoutInfo* pLayoutInfo = GetVehicleLayoutInfo();

		vehicleAssertf(pLayoutInfo,"%s This vehicle is missing layout information", GetModelName());
		if(pLayoutInfo)
		{
			return pLayoutInfo->GetSeatInfo(iSeatIndexOnLayout);
		}
	}

	return NULL;
}

const CVehicleEntryPointAnimInfo* CVehicleModelInfo::GetEntryPointAnimInfo(int iEntryIndex) const
{
	modelinfoAssert(m_data);
	int iEntryIndexOnLayout = m_data->m_SeatInfo.GetLayoutEntrypointIndex(iEntryIndex);

	if(iEntryIndexOnLayout > -1)
	{		
		const CVehicleLayoutInfo* pLayoutInfo = GetVehicleLayoutInfo();

		vehicleAssertf(pLayoutInfo,"%s This vehicle is missing layout information", GetModelName());
		if(pLayoutInfo)
		{
			return pLayoutInfo->GetEntryPointAnimInfo(iEntryIndexOnLayout);
		}
	}

	return NULL;
}


const CVehicleSeatAnimInfo* CVehicleModelInfo::GetSeatAnimationInfo(int iSeatIndex) const
{
	modelinfoAssert(m_data);
	int iSeatIndexOnLayout = m_data->m_SeatInfo.GetLayoutSeatIndex(iSeatIndex); 
	if (iSeatIndexOnLayout > -1)
	{		
		const CVehicleLayoutInfo* pLayoutInfo = GetVehicleLayoutInfo();

		vehicleAssertf(pLayoutInfo,"%s This vehicle is missing layout information", GetModelName() ? GetModelName() : "NULL");
		if(pLayoutInfo)
		{
			return pLayoutInfo->GetSeatAnimationInfo(iSeatIndexOnLayout);
		}
	}

	return NULL;
}

const CVehicleEntryPointInfo* CVehicleModelInfo::GetEntryPointInfo(int iEntryPointIndex) const
{
	int iEntryPointIndexOnLayout = m_data->m_SeatInfo.GetLayoutEntrypointIndex(iEntryPointIndex);

	if(iEntryPointIndexOnLayout > -1)
	{	
		const CVehicleLayoutInfo* pLayoutInfo = GetVehicleLayoutInfo();

		vehicleAssertf(pLayoutInfo,"%s This vehicle is missing layout information", GetModelName());

		if(pLayoutInfo)
		{
			return pLayoutInfo->GetEntryPointInfo(iEntryPointIndexOnLayout);
		}
	}

	return NULL;
}

s32 CVehicleModelInfo::GetSeatIndex(const CVehicleSeatInfo* pSeatInfo) const
{
	modelinfoAssert(m_data);
	modelinfoAssert(pSeatInfo);
	for(int i = 0; i < m_data->m_SeatInfo.GetNumSeats(); i++)
	{
		if(GetSeatInfo(i) == pSeatInfo)
		{
			return i;
		}
	}

	return  -1;
}

s32 CVehicleModelInfo::GetEntryPointIndex(const CVehicleEntryPointInfo* pEntryPointInfo) const
{
	modelinfoAssert(m_data);
	modelinfoAssert(pEntryPointInfo);
	for(int i = 0; i < m_data->m_SeatInfo.GetNumberEntryExitPoints(); i++)
	{
		if(GetEntryPointInfo(i) == pEntryPointInfo)
		{
			return i;
		}
	}

	return  -1;
}

void CVehicleModelInfo::SetBonesGroupMassAndAngularInertia(s32 boneIndex, float mass, const Vector3& angularInertia, fragGroupBits& tunedGroups, bool includeSubTree)
{
	if(boneIndex!=-1)
	{
		fragType* type = GetFragType();
		int groupIndex = type->GetGroupFromBoneIndex(0, boneIndex);
		//int componentIndex = type->GetComponentFromBoneIndex( 0, boneIndex );

		if(groupIndex != -1)
		{
			fragPhysicsLOD* physicsLOD = type->GetPhysics(0);
			//int parentBoneIndex = boneIndex;
			//
			//while( componentIndex > -1 )
			//{
			//	if( !physicsLOD->IsChildAngularInertiaScalable( (u8)componentIndex ).Getb() )
			//	{
			//		Warningf( "Wasn't able to set desired angular inertia of <%f, %f, %f> on group (%s,%i) component %i of %s",VEC3V_ARGS(RCC_VEC3V(angularInertia)),type->GetPhysics(0)->GetGroup(groupIndex)->GetDebugName(),groupIndex,componentIndex,GetModelName());
			//		return;
			//	}

			//	componentIndex = -1;
			//	parentBoneIndex = GetDrawable()->GetSkeletonData()->GetParentIndex( parentBoneIndex );

			//	if( parentBoneIndex > -1 )
			//	{
			//		componentIndex = type->GetComponentFromBoneIndex( 0, parentBoneIndex );
			//	}
			//}

			ASSERT_ONLY(BoolV wasAngularInertiaSet;)
			if(includeSubTree)
			{
				fragGroupBits groupsInSubTree;
				physicsLOD->GetGroupsAbove((u8)groupIndex,groupsInSubTree);
				tunedGroups.Union(groupsInSubTree);
				ASSERT_ONLY(wasAngularInertiaSet =) physicsLOD->SetUndamagedGroupTreeMassAndAngularInertia((u8)groupIndex,ScalarVFromF32(mass),RCC_VEC3V(angularInertia));
			}
			else
			{
				tunedGroups.Set(groupIndex);
				ASSERT_ONLY(wasAngularInertiaSet =) physicsLOD->SetUndamagedGroupMassAndAngularInertia((u8)groupIndex,ScalarVFromF32(mass),RCC_VEC3V(angularInertia));
			}
			ASSERT_ONLY(if(!wasAngularInertiaSet.Getb()) Warningf("Wasn't able to set desired angular inertia of <%f, %f, %f> on group (%s,%i) of %s",VEC3V_ARGS(RCC_VEC3V(angularInertia)),type->GetPhysics(0)->GetGroup(groupIndex)->GetDebugName(),groupIndex,GetModelName()));
		}
	}
}

void CVehicleModelInfo::SetJointStiffness(s32 boneIndex, float fStiffness, fragGroupBits& tunedGroups)
{
	if(boneIndex!=-1)
	{
		fragType* type = GetFragType();
		int groupIndex = type->GetGroupFromBoneIndex(0, boneIndex);
		if(groupIndex != -1)
		{
			fragPhysicsLOD* physicsLOD = type->GetPhysics(0);
			fragTypeGroup& group = *physicsLOD->GetGroup(groupIndex);
			group.SetJointStiffness(fStiffness);
			tunedGroups.Set(groupIndex);
		}
	}
}

void CVehicleModelInfo::SetBonesGroupMass(s32 boneIndex, float mass, fragGroupBits& tunedGroups)
{
	const int currentLOD = 0;
	if(boneIndex!=-1)
	{
		fragType* type = GetFragType();
		
		//int componentIndex = type->GetComponentFromBoneIndex( 0, boneIndex );
		//fragPhysicsLOD* physicsLOD = type->GetPhysics(0);
		//int parentBoneIndex = boneIndex;

		//while( componentIndex > -1 )
		//{
		//	if( !physicsLOD->IsChildAngularInertiaScalable( (u8)componentIndex ).Getb() )
		//	{
		//		Warningf( "Wasn't able to set mass of %f component %i of %s",mass,componentIndex,GetModelName());
		//		return;
		//	}

		//	componentIndex = -1;
		//	parentBoneIndex = GetDrawable()->GetSkeletonData()->GetParentIndex( parentBoneIndex );

		//	if( parentBoneIndex > -1 )
		//	{
		//		componentIndex = type->GetComponentFromBoneIndex( 0, parentBoneIndex );
		//	}
		//}

		SetGroupMass(type->GetGroupFromBoneIndex(currentLOD, boneIndex), mass, tunedGroups);
	}
}

void CVehicleModelInfo::SetGroupMass(s32 groupIndex, float mass, fragGroupBits& tunedGroups)
{
	const int currentLOD = 0;

	if(groupIndex != -1)
	{
		fragType* type = GetFragType();
		fragPhysicsLOD* physicsLOD = type->GetPhysics(currentLOD);
		fragTypeGroup* pGroup = physicsLOD->GetGroup(groupIndex);
		if(pGroup != NULL)
		{
			// Set the group mass
			pGroup->SetTotalUndamagedMass(currentLOD, mass, type);

			// Update the angular inertias for just the children of this changed group
			u8 numChildren = pGroup->GetNumChildren();
			u8 childEndIndex = numChildren + pGroup->GetChildFragmentIndex();
			for(u8 childIndex = pGroup->GetChildFragmentIndex(); childIndex < childEndIndex; childIndex++)
			{
				fragTypeChild& child = *(physicsLOD->GetChild(childIndex));

//				if( physicsLOD->IsChildAngularInertiaScalable( (u8)childIndex ).Getb() )
				{
					if(child.GetUndamagedEntity() && child.GetUndamagedEntity()->GetBound())
					{
						physicsLOD->SetUndamagedAngInertia(childIndex,VEC3V_TO_VECTOR3(child.GetUndamagedEntity()->GetBound()->GetComputeAngularInertia(child.GetUndamagedMass())));
					}
					else
					{
						physicsLOD->SetUndamagedAngInertia(childIndex,VEC3V_TO_VECTOR3(Vec3V(V_ZERO)));
					}

					if(child.GetDamagedEntity() && child.GetDamagedEntity()->GetBound())
					{
						physicsLOD->SetDamagedAngInertia(childIndex,VEC3V_TO_VECTOR3(child.GetDamagedEntity()->GetBound()->GetComputeAngularInertia(child.GetDamagedMass())));
					}
					else
					{
						physicsLOD->SetDamagedAngInertia(childIndex,VEC3V_TO_VECTOR3(Vec3V(V_ZERO)));
					}
				}
			}

			tunedGroups.Set(groupIndex);
		}
	}
}

Vector3 BIKE_ANG_INERTIAL_MULT(1.0f, 1.0f, 1.0f);
//

eHierarchyId CVehicleModelInfo::ms_aSetupDoorIds[NUM_VEH_DOORS_MAX] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R, VEH_BONNET, VEH_BOOT, VEH_BOOT_2, VEH_FOLDING_WING_L, VEH_FOLDING_WING_R };
eHierarchyId CVehicleModelInfo::ms_aSetupWheelIds[NUM_VEH_CWHEELS_MAX] = { 
	VEH_WHEEL_LF, 
	VEH_WHEEL_RF, 
	VEH_WHEEL_LR, 
	VEH_WHEEL_RR, 
	VEH_WHEEL_LM1, 
	VEH_WHEEL_RM1, 
	VEH_WHEEL_LM2, 
	VEH_WHEEL_RM2, 
	VEH_WHEEL_LM3, 
	VEH_WHEEL_RM3 
};
#define WHEEL_LF_INDEX (0)

dev_float STD_VEHICLE_BUMPER_MASS = 25.0f;

dev_float YACHT_MAST_STIFFNESS = 0.9f;

dev_float STD_HELI_ROTOR_MASS = 100.0f;
dev_float STD_HELI_ROTOR_ANG_INERTIA = 500.0f;
dev_float STD_HELI_TAIL_ROTOR_MASS = 50.0f;
dev_float STD_HELI_TAIL_ROTOR_ANG_INERTIA = 50.0f;

dev_float STD_WHEEL_MASS = 5.0f;
const Vector3 STD_WHEEL_INERTIA (0.65f,0.35f,0.35f);

dev_float STD_LANDING_GEAR_MASS = 5.0f;

dev_float STD_SEAT_MASS = 1.0f;
dev_float STD_SEAT_ANG_INERTIA = 1.0f;

dev_float STD_DIGGER_ARM_MASS = 200.0f;
dev_float STD_DIGGER_ARM_ANG_INERTIA = 200.0f;

dev_float STD_VEHICLE_BLOCKER_MASS = 1.0f;
dev_float STD_VEHICLE_BLOCKER_ANG_INERTIA = 1.0f;

dev_float STD_TANK_TURRET_MASS = 1100.0f;
dev_float LIGHT_TANK_TURRET_MASS = 550.0f;
dev_float BOAT_TURRET_MASS = 10.0f;
dev_float RC_CAR_TURRET_MASS = 2.5f;

dev_float STD_FIRE_TRUCK_TURRET_MASS = 50.0f;

dev_float STD_EOD_ARM_MASS = 1.0f;
dev_float STD_EOD_ARM_ANG_INERTIA = 1.0f;

dev_float STD_EXTRA_MASS = 10.0f;
dev_float STD_BREAKABLE_EXTRA_MASS = 10.0f;

dev_float STD_MOD_COLLISION_MASS = 1.0f;
dev_float STD_MOD_COLLISION_ANG_INERTIA = 1.0f;

dev_float STD_TOW_ARM_MASS = 350.0f;
dev_float STD_TOW_ARM_ANG_INERTIA = 350.0f; 

dev_float STD_FORKLIFT_MASS = 40.0f;
dev_float STD_FORKLIFT_ANG_INERTIA = 10.0f;

dev_float STD_HANDLER_FRAME_MASS = 100.0f;
dev_float STD_HANDLER_FRAME_ANG_INERTIA = 10.0f;

dev_float STD_FREIGHTCONT2_MASS = 20042.0;
dev_float STD_FREIGHTCONT2_ANG_INERTIA = 20042.0;

#define  STD_LOW_LOD_CHASSIS_MASS 1.0f
#define  STD_LOW_LOD_CHASSIS_ANG_INERTIA 1.0f

#define STD_DUMMY_CHASSIS_MASS 1.0f
#define STD_DUMMY_CHASSIS_ANG_INERTIA 1.0f
dev_float PLANE_CHASSIS_MASS_MIN_RATIO = 0.6f;

dev_float BLIMP_SHELL_FRAME_MASS = 500.0f;



void CVehicleModelInfo::InitFragType(CHandlingData* pHandling)
{
	modelinfoAssert(m_data);

	// set up joint limits on doors so they swing about
	crSkeletonData *pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
	if(pSkeletonData==NULL)
	{
		modelinfoAssertf(false, "A vehicle without a skeleton?");
	}

	fragGroupBits tunedGroups;
	fragType* type = GetFragType();
	fragPhysicsLOD* physicsLOD = type->GetPhysics(0);
	phArchetypeDamp* undamagedArchetype = physicsLOD->GetArchetype();

	// Go through seats.. some may have collision for open top vehicles
	// Need to lower the mass on them
	float seatMass = pHandling->m_fMass > STD_SEAT_MASS ? STD_SEAT_MASS : pHandling->m_fMass/4.0f;//if the mass of the total vehicle is less than a reasonable number them just set the mass of the seat to a tenth of the mass of the total vehicle
	for(int iSeatIndex = 0; iSeatIndex < m_data->m_SeatInfo.GetNumSeats(); iSeatIndex++)
	{
		SetBonesGroupMassAndAngularInertia(m_data->m_SeatInfo.GetBoneIndexFromSeat(iSeatIndex),seatMass, Vector3(STD_SEAT_ANG_INERTIA,STD_SEAT_ANG_INERTIA,STD_SEAT_ANG_INERTIA),tunedGroups);
	}

	// Set the bumper mass and angular inertia
	SetBonesGroupMass(GetBoneIndex(VEH_BUMPER_F),STD_VEHICLE_BUMPER_MASS,tunedGroups);
	SetBonesGroupMass(GetBoneIndex(VEH_BUMPER_R),STD_VEHICLE_BUMPER_MASS,tunedGroups);

	// set mass and inertia of rotors for helicopters
	if(GetIsRotaryAircraft())
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(HELI_ROTOR_MAIN),STD_HELI_ROTOR_MASS, Vector3(STD_HELI_ROTOR_ANG_INERTIA,STD_HELI_ROTOR_ANG_INERTIA,STD_HELI_ROTOR_ANG_INERTIA),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(HELI_ROTOR_REAR),STD_HELI_TAIL_ROTOR_MASS, Vector3(STD_HELI_TAIL_ROTOR_ANG_INERTIA,STD_HELI_TAIL_ROTOR_ANG_INERTIA,STD_HELI_TAIL_ROTOR_ANG_INERTIA),tunedGroups);
		
		if( ( MI_HELI_DRONE.IsValid() && ( GetModelNameHash() == MI_HELI_DRONE.GetName().GetHash() ) ) ||
			( MI_HELI_DRONE_2.IsValid() && ( GetModelNameHash() == MI_HELI_DRONE_2.GetName().GetHash() ) ) )
		{
			SetBonesGroupMassAndAngularInertia(GetBoneIndex(HELI_ROTOR_MAIN_2),STD_HELI_ROTOR_MASS, Vector3(STD_HELI_ROTOR_ANG_INERTIA,STD_HELI_ROTOR_ANG_INERTIA,STD_HELI_ROTOR_ANG_INERTIA),tunedGroups);
			SetBonesGroupMassAndAngularInertia(GetBoneIndex(HELI_ROTOR_REAR_2),STD_HELI_TAIL_ROTOR_MASS, Vector3(STD_HELI_TAIL_ROTOR_ANG_INERTIA,STD_HELI_TAIL_ROTOR_ANG_INERTIA,STD_HELI_TAIL_ROTOR_ANG_INERTIA),tunedGroups);
		}
	}

	if( GetIsPlane() || 
		GetIsHeli() )
	{
		eHierarchyId aLandingGearIds[CLandingGear::MAX_NUM_PARTS] = 
		{
			LANDING_GEAR_F,
			LANDING_GEAR_RM1,
			LANDING_GEAR_LM1,
			LANDING_GEAR_RL,
			LANDING_GEAR_RR,
			LANDING_GEAR_RM
		};

		for(int i = 0; i < CLandingGear::MAX_NUM_PARTS; i++)
		{
			int iBoneIndex = GetBoneIndex(aLandingGearIds[i]);
			if(iBoneIndex > -1)
			{
				int iGroupIndex = GetFragType()->GetGroupFromBoneIndex(0,iBoneIndex);

				if(iGroupIndex > -1)
				{
					// Need to set latch strengths on groups to be non zero because code decides to not work if strength is 0
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[iGroupIndex];
					SetBonesGroupMassAndAngularInertia(iBoneIndex,STD_LANDING_GEAR_MASS, Vector3(STD_LANDING_GEAR_MASS,STD_LANDING_GEAR_MASS,STD_LANDING_GEAR_MASS),tunedGroups);
					pGroup->SetLatchStrength(-1.0f);

					pGroup->SetForceTransmissionScaleDown(0.0f);
					pGroup->SetForceTransmissionScaleUp(0.0f);
				}
			}
		}

		if( !GetIsHeli() &&
			( !MI_PLANE_MICROLIGHT.IsValid() ||
			  GetModelNameHash() != MI_PLANE_MICROLIGHT.GetName().GetHash() ) )
		{
			fragTypeGroup *pRootGroup = physicsLOD->GetGroup(0);
			if(pRootGroup->GetTotalUndamagedMass() < undamagedArchetype->GetMass() * PLANE_CHASSIS_MASS_MIN_RATIO)
			{
				SetGroupMass(0, pHandling->m_fMass * PLANE_CHASSIS_MASS_MIN_RATIO, tunedGroups);
			}
		}
		
	}

	if(GetIsDraftVeh())
	{
		const float yokeMassScale = 0.3f;
		const float yokeExMassScale = 0.09f;

		SetBonesGroupMass(GetBoneIndex(VEH_MISC_G), pHandling->m_fMass * yokeMassScale, tunedGroups);
		SetBonesGroupMass(GetBoneIndex(VEH_MISC_F), pHandling->m_fMass * yokeExMassScale, tunedGroups);
	}

	const s32 MAX_ROOF_PARTS = 3;
	eHierarchyId aRoofIds[MAX_ROOF_PARTS] = 
	{
		VEH_ROOF2,
		VEH_ROOF,
		VEH_MISC_A,
	};

	float aRoofMass[MAX_ROOF_PARTS] = 
	{
		1.0f,
		2.0f,
		10.0f,
	};

	for(int i = MAX_ROOF_PARTS-1; i >= 0; i--)
	{
		int iBoneIndex = GetBoneIndex(aRoofIds[i]);
		if(iBoneIndex > -1)
		{
			int iGroupIndex = GetFragType()->GetGroupFromBoneIndex(0,iBoneIndex);

			if(iGroupIndex > -1)
			{
				// set mass of the roof parts
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[iGroupIndex];

				SetBonesGroupMassAndAngularInertia(iBoneIndex,aRoofMass[i],Vector3(aRoofMass[i],aRoofMass[i],aRoofMass[i]),tunedGroups);

				// Need to set latch strengths on groups to be non zero because code decides to not work if strength is 0
				pGroup->SetLatchStrength(-1.0f);

				pGroup->SetForceTransmissionScaleDown(0.0f);
				pGroup->SetForceTransmissionScaleUp(0.0f);

			}
		}
		else
		{
			break;// if we haven't found a roof element just give up
		}
	}

	//if we have a digger arm set the mass on it
	for(s16 i = VEH_DIGGER_ARM; i < VEH_DIGGER_ARM_MAX; i++)
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(i),STD_DIGGER_ARM_MASS,Vector3(STD_DIGGER_ARM_ANG_INERTIA,STD_DIGGER_ARM_ANG_INERTIA,STD_DIGGER_ARM_ANG_INERTIA),tunedGroups);
	}

	//set the mass on the vehicle blocker bound if we have one
	SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_VEHICLE_BLOCKER),STD_VEHICLE_BLOCKER_MASS,Vector3(STD_VEHICLE_BLOCKER_ANG_INERTIA,STD_VEHICLE_BLOCKER_ANG_INERTIA,STD_VEHICLE_BLOCKER_ANG_INERTIA),tunedGroups);

	//if we have turrets set the mass on it
	if(GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_TANK))
	{
		if( GetModelNameHash() == MI_TANK_KHANJALI.GetName().GetHash() )
		{
			SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_1_BASE ), LIGHT_TANK_TURRET_MASS, Vector3( LIGHT_TANK_TURRET_MASS, LIGHT_TANK_TURRET_MASS, LIGHT_TANK_TURRET_MASS ), tunedGroups );
			SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_1_BARREL ), 100.0f, Vector3( 100.0f, 100.0f, 100.0f ), tunedGroups );

			for(s16 i = VEH_TURRET_2_BASE; i <= VEH_TURRET_4_BARREL; i++)
			{
				SetBonesGroupMassAndAngularInertia( GetBoneIndex(i), STD_FIRE_TRUCK_TURRET_MASS, Vector3( STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS ), tunedGroups );
			}
		}
        else if( pHandling->mFlags & MF_IS_RC )
        {
            for( s16 i = VEH_TURRET_1_BASE; i < VEH_TURRET_2_BARREL; i++ )
            {
                SetBonesGroupMass( GetBoneIndex( i ), RC_CAR_TURRET_MASS, tunedGroups );
            }
            for( s16 i = VEH_TURRET_FIRST_MOD; i < VEH_TURRET_LAST_MOD; i++ )
            {
                SetBonesGroupMass( GetBoneIndex( i ), RC_CAR_TURRET_MASS, tunedGroups );
            }
        }
		else
		{
			for(s16 i = VEH_TURRET_1_BASE; i < VEH_TURRET_2_BARREL; i++)
			{
				SetBonesGroupMass(GetBoneIndex(i),STD_TANK_TURRET_MASS,tunedGroups);
			}
		}
	}
	else
	{
		for(s16 i = VEH_TURRET_1_BASE; i < VEH_TURRET_2_BARREL; i++)
		{
			bool heavyTurret = MI_CAR_TERBYTE.IsValid() && GetModelNameHash() == MI_CAR_TERBYTE.GetName().GetHash();

			float mass = !heavyTurret ? STD_FIRE_TRUCK_TURRET_MASS : LIGHT_TANK_TURRET_MASS;
			Vector3 inertia = !heavyTurret ? Vector3( STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS ) : Vector3( LIGHT_TANK_TURRET_MASS, LIGHT_TANK_TURRET_MASS, LIGHT_TANK_TURRET_MASS );

			SetBonesGroupMassAndAngularInertia(GetBoneIndex(i), mass, inertia,tunedGroups);
		}
	}

	if( GetIsBoat() )
	{
		for(s16 i = VEH_TURRET_PARTS_FIRST; i < VEH_TURRET_PARTS_LAST; i++)
		{
			Vector3 inertia = Vector3( BOAT_TURRET_MASS, BOAT_TURRET_MASS, BOAT_TURRET_MASS );
			SetBonesGroupMassAndAngularInertia(GetBoneIndex(i), BOAT_TURRET_MASS, inertia,tunedGroups);
		}

		for(s16 i = VEH_TURRET_FIRST_MOD; i < VEH_TURRET_LAST_MOD; i++)
		{
			Vector3 inertia = Vector3( BOAT_TURRET_MASS, BOAT_TURRET_MASS, BOAT_TURRET_MASS );
			SetBonesGroupMassAndAngularInertia(GetBoneIndex(i), BOAT_TURRET_MASS, inertia,tunedGroups);
		}
	}

	//Reduce the weight of the guns on the side of the heli as they cause it to pull to one side.
	if(( MI_HELI_VALKYRIE.IsValid() && ( GetModelNameHash() == MI_HELI_VALKYRIE.GetName().GetHash() )) ||
		(MI_HELI_VALKYRIE2.IsValid() && ( GetModelNameHash() == MI_HELI_VALKYRIE2.GetName().GetHash() ) ) )
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_MISC_H),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_MISC_G),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TURRET_1_BASE),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TURRET_1_BARREL),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TURRET_2_BASE),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TURRET_2_BARREL),STD_EXTRA_MASS, Vector3(STD_EXTRA_MASS,STD_EXTRA_MASS,STD_EXTRA_MASS),tunedGroups);
	}

	if( ( MI_CAR_DUNE3.IsValid() && GetModelNameHash() == MI_CAR_DUNE3.GetName().GetHash() ) ||
		( MI_TRAILER_TRAILERLARGE.IsValid() && GetModelNameHash() == MI_TRAILER_TRAILERLARGE.GetName().GetHash() ) )
	{
		Vector3 dune3TurretInertia = Vector3( 1.0f, 1.0f, 1.0f );

		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_1_BASE ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_1_BARREL ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_1_MOD ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_2_BASE ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_2_BARREL ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_2_MOD ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_3_BASE ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_3_BARREL ), 1.0f, dune3TurretInertia, tunedGroups );
		SetBonesGroupMassAndAngularInertia( GetBoneIndex( VEH_TURRET_3_MOD ), 1.0f, dune3TurretInertia, tunedGroups );
	}
	else if( GetVehicleFlag( CVehicleModelInfoFlags::FLAG_TURRET_MODS_ON_ROOF ) ||
			 GetIsPlane() )
	{
		const s32 MAX_TURRET_MOD_PARTS = 14;
		eHierarchyId turretModBones[ MAX_TURRET_MOD_PARTS ] = { VEH_TURRET_1_BASE, VEH_TURRET_4_BARREL, VEH_TURRET_A1_BASE, VEH_TURRET_A4_BARREL, VEH_TURRET_B1_BASE, VEH_TURRET_B4_BARREL, VEH_WEAPON_1A, VEH_WEAPON_4D_ROT, VEH_TURRET_FIRST_MOD, VEH_TURRET_LAST_MOD, VEH_TURRET_A_FIRST_MOD, VEH_TURRET_A_LAST_MOD, VEH_TURRET_B_FIRST_MOD, VEH_TURRET_B_LAST_MOD };
		Vector3 stdFireTruckTurretInertia = Vector3( STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS, STD_FIRE_TRUCK_TURRET_MASS );
		float turretMass = STD_FIRE_TRUCK_TURRET_MASS;

		if( GetIsPlane() ||
			GetModelNameHash() == MI_CAR_SPEEDO4.GetName().GetHash() )
		{
			turretMass = 1.0f;
			stdFireTruckTurretInertia = Vector3( turretMass, turretMass, turretMass );
		}

		for( int i = 0; i < MAX_TURRET_MOD_PARTS; i+=2 )
		{
			for( int j = (int)turretModBones[ i ]; j <= (int)turretModBones[ i + 1 ]; j++ )
			{
				SetBonesGroupMassAndAngularInertia( GetBoneIndex( j ), turretMass, stdFireTruckTurretInertia, tunedGroups );

				int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex( i ));

				if( nGroupIndex != -1 )
				{
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[ nGroupIndex ];
					pGroup->SetStrength( -1.0f );
					pGroup->SetForceTransmissionScaleUp(0.0f);
					pGroup->SetForceTransmissionScaleDown(0.0f);
				}
			}
		}
	}

	for( int nPanel = VEH_FIRST_BREAKABLE_PANEL; nPanel <= VEH_LAST_BREAKABLE_PANEL; nPanel++ )
	{
		if( GetBoneIndex( nPanel )!=-1 )
		{
			int nGroupIndex = GetFragType()->GetGroupFromBoneIndex( 0, GetBoneIndex( nPanel ) );
			if( nGroupIndex != -1 )
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[ nGroupIndex ];
				pGroup->SetStrength( -1.0f );
				pGroup->SetForceTransmissionScaleUp(0.0f);
				pGroup->SetForceTransmissionScaleDown(0.0f);	

				SetBonesGroupMassAndAngularInertia( GetBoneIndex( nPanel ), 10.0f, Vector3( 10.0f, 10.0f, 10.0f ), tunedGroups );
			}
		}
	}

	//if we have a tow arm set the mass on it
	SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TOW_ARM),STD_TOW_ARM_MASS,Vector3(STD_TOW_ARM_ANG_INERTIA,STD_TOW_ARM_ANG_INERTIA,STD_TOW_ARM_ANG_INERTIA),tunedGroups);


	//if we have a tipper set the mass on it
	SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_TIPPER),STD_WHEEL_MASS,Vector3(STD_WHEEL_MASS,STD_WHEEL_MASS,STD_WHEEL_MASS),tunedGroups);

	if(GetVehicleType() == VEHICLE_TYPE_TRAILER) // This is a gadget for tr3 trailer (yacht).
	{
		//if we have a misc_e bone set the mass on it
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_MISC_E),20.0f,Vector3(20.0f,20.0f,20.0f),tunedGroups);
		// ... and the damping.
		SetJointStiffness(GetBoneIndex(VEH_MISC_E), YACHT_MAST_STIFFNESS, tunedGroups);

	}

	//if we have an EOD robot arm
	for(s16 i = VEH_ARM1; i <= VEH_ARM4; i++)
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(i),STD_EOD_ARM_MASS,Vector3(STD_EOD_ARM_ANG_INERTIA,STD_EOD_ARM_ANG_INERTIA,STD_EOD_ARM_ANG_INERTIA),tunedGroups);
	}

	for(s16 i = VEH_EXTRA_1; i <= VEH_LAST_EXTRA; i++)
	{
		SetBonesGroupMass(GetBoneIndex(i),STD_EXTRA_MASS,tunedGroups);
	}

	for(s16 i = VEH_BREAKABLE_EXTRA_1; i <= VEH_LAST_BREAKABLE_EXTRA; i++)
	{
		SetBonesGroupMass(GetBoneIndex(i),STD_BREAKABLE_EXTRA_MASS,tunedGroups);
	}

	for(s16 i = VEH_MOD_COLLISION_1; i <= VEH_LAST_MOD_COLLISION; i++)
	{
#if __ASSERT
		if( GetBoneIndex( i ) != -1 )
		{
            atHashString dlcPackName = GetVehicleDLCPack();
            atHashString s_currentDLCPackName( "dlc_mpHeist3CRC", 0x7F363F2B );

            if( dlcPackName == s_currentDLCPackName )
            {
			    fragType* type = GetFragType();
			    int groupIndex = type->GetGroupFromBoneIndex( 0, GetBoneIndex( i ) );

			    Assertf( groupIndex != -1, "Mod_col_%d has no collision bound. Mod col bones should only be used for enabling and disabling collision", i - (int)VEH_MOD_COLLISION_1 );
            }
		}
#endif // #if __BANK

		if(pHandling && pHandling->hFlags & HF_REDUCED_MOD_MASS)
		{
			SetBonesGroupMassAndAngularInertia(GetBoneIndex(i),STD_MOD_COLLISION_MASS,Vector3(STD_MOD_COLLISION_ANG_INERTIA,STD_MOD_COLLISION_ANG_INERTIA,STD_MOD_COLLISION_ANG_INERTIA),tunedGroups);
		}
		else
		{
			SetBonesGroupMass(GetBoneIndex(i),STD_BREAKABLE_EXTRA_MASS,tunedGroups);
		}
	}

	if(GetVehicleType() == VEHICLE_TYPE_BLIMP)
	{
		// Ensure that the broken pieces of the blimp shell get a reasonable mass. The unbroken shell BLIMP_SHELL gets destroyed during
		//   breaking so it doesn't need any mass. 
		// NOTE: This must be after the VEH_EXTRA_1 loop since BLIMP_SHELL_X are extras
		for(s32 blimpShellFrameId = BLIMP_SHELL_FRAME_1; blimpShellFrameId <= BLIMP_SHELL_FRAME_LAST; ++blimpShellFrameId)
		{
			SetBonesGroupMass(GetBoneIndex(blimpShellFrameId),BLIMP_SHELL_FRAME_MASS,tunedGroups);
		}
	}


	for( s16 i = VEH_SPEAKER_FIRST; i <= VEH_SPEAKER_LAST; i++ )
	{
		SetBonesGroupMass( GetBoneIndex( i ), STD_BREAKABLE_EXTRA_MASS, tunedGroups );
	}

	//if we have forks, latch them
	const s32 MAX_FORKLIFT_PARTS = 3;
	eHierarchyId aForkliftIds[MAX_FORKLIFT_PARTS] = 
	{
		VEH_FORKS,
		VEH_MAST,
		VEH_CARRIAGE,
	};

	for(s16 i = 0; i < MAX_FORKLIFT_PARTS; i++)
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(aForkliftIds[i]),STD_FORKLIFT_MASS,Vector3(STD_FORKLIFT_ANG_INERTIA,STD_FORKLIFT_ANG_INERTIA,STD_FORKLIFT_ANG_INERTIA),tunedGroups);
	}

	// Latch joints for the Handler vehicle.
	const s32 MAX_HANDLER_PARTS = 3;
	eHierarchyId aHandlerFrameIds[MAX_HANDLER_PARTS] = 
	{
		VEH_HANDLER_FRAME_1,
		VEH_HANDLER_FRAME_2,
		VEH_HANDLER_FRAME_3
	};

	for(s16 i = 0; i < MAX_HANDLER_PARTS; ++i)
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(aHandlerFrameIds[i]),STD_HANDLER_FRAME_MASS,Vector3(STD_HANDLER_FRAME_ANG_INERTIA,STD_HANDLER_FRAME_ANG_INERTIA,STD_HANDLER_FRAME_ANG_INERTIA),tunedGroups);
	}

	// Increase mass on freight container so the container doesn't have a mass of 1kg when it breaks off.
	const s32 MAX_FREIGHTCONT2_PARTS = 2;
	eHierarchyId aFreightCont2Ids[MAX_FREIGHTCONT2_PARTS] = 
	{
		VEH_FREIGHTCONT2_CONTAINER,
		VEH_FREIGHTCONT2_BOGEY,
	};
	for(s16 i = 0; i < MAX_FREIGHTCONT2_PARTS; ++i)
	{
		SetBonesGroupMassAndAngularInertia(GetBoneIndex(aFreightCont2Ids[i]),STD_FREIGHTCONT2_MASS,Vector3(STD_FREIGHTCONT2_ANG_INERTIA,STD_FREIGHTCONT2_ANG_INERTIA,STD_FREIGHTCONT2_ANG_INERTIA),tunedGroups);
	}
	// We've made a big change to the freight container's weight distribution here, update the centre of mass.
	{
		fragType* pFragType = GetFragType();
		Assert(pFragType);
		pFragType->ComputeMassProperties(0);
	}

	// Now update the doors/wheels after anything that could be their child in the heirarchy is updated
	{
		crBoneData *pBone = NULL;
		for(int i=0; i<NUM_VEH_DOORS_MAX; i++)
		{
			eHierarchyId nDoorId = ms_aSetupDoorIds[i];
			if(GetBoneIndex(nDoorId)!=-1)
			{
				pBone = (crBoneData *)pSkeletonData->GetBoneData(GetBoneIndex(nDoorId));
				if(pBone->GetDofs() &(crBoneData::HAS_ROTATE_LIMITS|crBoneData::HAS_TRANSLATE_LIMITS))
				{
					int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(nDoorId));
					if(nGroupIndex != -1)
					{
						if(GetVehicleType() == VEHICLE_TYPE_TRAILER)
						{
							SetBonesGroupMassAndAngularInertia(GetBoneIndex(nDoorId),TRAILER_DOOR_MASS, Vector3(TRAILER_DOOR_ANG_INERTIA, TRAILER_DOOR_ANG_INERTIA, TRAILER_DOOR_ANG_INERTIA),tunedGroups,true);
						}
						else
						{
							if( GetModelNameHash() != MI_CAR_STAFFORD.GetName().GetHash() )
							{
								float doorMass = pHandling->m_fMass > 2 * STD_VEHICLE_DOOR_MASS ? STD_VEHICLE_DOOR_MASS : pHandling->m_fMass / 4.0f;//if the mass of the total vehicle is less than a reasonable number them just set the mass of the doors to a tenth of the mass of the total vehicle
								SetBonesGroupMassAndAngularInertia( GetBoneIndex( nDoorId ), doorMass, Vector3( STD_VEHICLE_DOOR_ANG_INERTIA, STD_VEHICLE_DOOR_ANG_INERTIA, STD_VEHICLE_DOOR_ANG_INERTIA ), tunedGroups, true );
							}
						}
					}
				}
			}
		}
	}

	// Windows transmit 100% of their force to the door. Without this it becomes much more difficult to break
	//   the door off if it gets contacts on the window. 
	for (int i=VEH_FIRST_WINDOW; i<=VEH_LAST_WINDOW; i++)
	{
		eHierarchyId hierarchyId = (eHierarchyId)i;
		int windowBoneIndex = GetBoneIndex(hierarchyId);
		if (windowBoneIndex>-1)
		{
			int groupIndex = type->GetGroupFromBoneIndex(0, windowBoneIndex);
			if(groupIndex != -1)
			{
				physicsLOD->GetGroup(groupIndex)->SetForceTransmissionScaleUp(1.0f);
			}
		}
	}

	for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
	{
		float wheelMass = STD_WHEEL_MASS;
		Vector3 wheelInertia = STD_WHEEL_INERTIA;

		// Wheels above a certain size get a bigger mass.
		dev_float sfBigWheelRadius = 0.7f;
		dev_float sfBigWheelMass = 500.0f;
		dev_float sfBigWheelHeavyVehicleThreshold = 4000.0f;
		const Vector3 svBigWheelInertia (65.0f,35.0f,35.0f);

		if(pHandling->m_fMass >= sfBigWheelHeavyVehicleThreshold)
		{
			if(GetBoneIndex(VEH_WHEEL_FIRST_WHEEL + i)!=-1)
			{
				int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(VEH_WHEEL_FIRST_WHEEL + i));
				if(nGroupIndex != -1)
				{
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];

					// get front wheel radius from bound
					fragTypeChild* pFrontLeftChild = GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()];

					if(pFrontLeftChild->GetUndamagedEntity()->GetBound()->GetRadiusAroundCentroid() > sfBigWheelRadius)
					{
						wheelMass = sfBigWheelMass;
						wheelInertia = svBigWheelInertia;
					}
				}
			}
		}
		else if (pHandling->m_fMass <= STD_WHEEL_MASS)//if the mass of the total vehicle is less than a reasonable number them just set the mass of the wheels to a tenth of the mass of the total vehicle
		{
			wheelMass = pHandling->m_fMass/4.0f;
			wheelInertia = STD_WHEEL_INERTIA * (wheelMass/STD_WHEEL_MASS);
		}

		SetBonesGroupMassAndAngularInertia(GetBoneIndex(ms_aSetupWheelIds[i]),wheelMass,wheelInertia,tunedGroups);
	}

	// Need to make sure individual link attachment matrices get allocated for all vehicles
	// This is so we can move the wheel matrices without them getting trashed by the articulated collider
	GetFragType()->SetForceAllocateLinkAttachments();

	//Set the low lod chassis to be very light so it doesn't interfere with the mass distribution.
	modelinfoAssertf(GetBoneIndex(VEH_CHASSIS)!=-1, "Vehicle %s has no chassis bone", GetModelName());
   
#if __DEV
    if (GetBoneIndex(VEH_CHASSIS_LOWLOD) == -1)
    {
        modelinfoDisplayf("Vehicle %s is missing a low lod chassis", GetModelName());
    }

	if( GetBoneIndex(VEH_CHASSIS_DUMMY) == -1)
	{
		modelinfoDisplayf("Vehicle %s is missing a dummy bound", GetModelName());
	}
	//Verify that the vehicle's wheels have disc bounds
	phBound* pBound = undamagedArchetype->GetBound();
	if(pBound->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for(int i = VEH_WHEEL_FIRST_WHEEL; i <= VEH_WHEEL_LAST_WHEEL; i++)
		{
			int nComponent = type->GetComponentFromBoneIndex(0, GetBoneIndex(i));
			if(nComponent > -1)
			{
				if(phBound* pWheelBound = pBoundComposite->GetBound(nComponent))
				{
					if(pWheelBound->GetType() != phBound::DISC)
					{
						modelinfoDisplayf("Vehicle %s has wheels without disc bounds", GetModelName());
						break;
					}
				}
			}
		}
	}
#endif
	SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_CHASSIS_LOWLOD),STD_LOW_LOD_CHASSIS_MASS, Vector3(STD_LOW_LOD_CHASSIS_ANG_INERTIA, STD_LOW_LOD_CHASSIS_ANG_INERTIA, STD_LOW_LOD_CHASSIS_ANG_INERTIA), tunedGroups);
	SetBonesGroupMassAndAngularInertia(GetBoneIndex(VEH_CHASSIS_DUMMY),STD_DUMMY_CHASSIS_MASS, Vector3(STD_HANDLER_FRAME_ANG_INERTIA,STD_HANDLER_FRAME_ANG_INERTIA,STD_HANDLER_FRAME_ANG_INERTIA), tunedGroups);

	const ScalarV newMass = ScalarVFromF32(pHandling->m_fMass);
	const Vec3V newCenterOfGravity = pHandling->m_vecCentreOfMassOffset;

	// Figure out which children we're allowed to modify the mass/angular inertia of to reach our target total mass/angular inertia
	fragGroupBits scalableGroups(true);
	scalableGroups.IntersectNegate(tunedGroups);
	fragChildBits scalableChildren;
	physicsLOD->ConvertGroupBitsetToChildBitset(scalableGroups,scalableChildren);

	ASSERT_ONLY(const ScalarV tolerance = Max(ScalarV(V_FLT_SMALL_3),Scale(ScalarV(V_FLT_SMALL_4),newMass)));

	// Scale the mass to the target amount
	type->ScaleUndamagedChildren(0,&newMass,NULL,scalableChildren,&newCenterOfGravity);
	Assertf(IsCloseAll(ScalarVFromF32(undamagedArchetype->GetMass()),newMass,tolerance), "Wasn't able to set desired mass of %f on %s. Using %f instead.",newMass.Getf(),GetModelName(),undamagedArchetype->GetMass());

	// Now that the mass has been scaled and the angular inertia alongside it, scale the angular inertia further if the tuning specifies it.
	Vec3V angularInertiaScale(V_ONE);
	if(GetVehicleType()==VEHICLE_TYPE_BIKE)
	{
		angularInertiaScale = pHandling->m_vecInertiaMultiplier;
	}
	else if(GetVehicleType()==VEHICLE_TYPE_CAR || GetVehicleType() == VEHICLE_TYPE_TRAILER || GetVehicleType() == VEHICLE_TYPE_QUADBIKE || GetVehicleType() == VEHICLE_TYPE_DRAFT || GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
	{
		angularInertiaScale = pHandling->m_vecInertiaMultiplier;
	}
	if(IsCloseAll(angularInertiaScale,Vec3V(V_ONE),Vec3V(V_FLT_SMALL_3)) == 0)
	{
		// Scale the fragment to the new mass/angular inertia
		const Vec3V newAngularInertia = Scale(VECTOR3_TO_VEC3V(undamagedArchetype->GetAngInertia()),angularInertiaScale);
		type->ScaleUndamagedChildren(0,NULL,&newAngularInertia,scalableChildren,&newCenterOfGravity);
		Assertf(IsCloseAll(VECTOR3_TO_VEC3V(undamagedArchetype->GetAngInertia()),newAngularInertia,Vec3V(tolerance)), "Wasn't able to scale angular inertia of <%f, %f, %f> by <%f, %f, %f> on %s. Final angular inertia: <%f, %f, %f>.",VEC3V_ARGS(newAngularInertia),VEC3V_ARGS(angularInertiaScale),GetModelName(),VEC3V_ARGS(VECTOR3_TO_VEC3V(undamagedArchetype->GetAngInertia())));
	}
	else
	{
		Assertf(undamagedArchetype->GetAngInertia().IsClose(undamagedArchetype->GetAngInertia(), SMALL_FLOAT), "Invalid inertia <%3.2f, %3.2f, %3.2f> of model %s", VEC3V_ARGS(VECTOR3_TO_VEC3V(undamagedArchetype->GetAngInertia())), GetModelName());
	}

	// Make sure the car void material is not in the zeroth slot, because otherwise we will pick that at random if FindElementIndex fails
	phBoundComposite* pBoundComp = ((phBoundComposite*)GetFragType()->GetPhysics(0)->GetArchetype()->GetBound());
	for(int nBound=0; nBound < pBoundComp->GetNumBounds(); nBound++)
	{
		if(pBoundComp->GetBound(nBound) && pBoundComp->GetBound(nBound)->GetType() == phBound::GEOMETRY)
		{
			phBoundGeometry* pBoundGeom = static_cast<phBoundGeometry*>(pBoundComp->GetBound(nBound));
			if (PGTAMATERIALMGR->UnpackMtlId(pBoundGeom->GetMaterialId(0)) == PGTAMATERIALMGR->g_idCarVoid)
			{
				pBoundGeom->SwapZeroAndFirstMaterials();
			}
		}
	}

	if( GetModelNameHash() == MI_TANK_KHANJALI.GetName().GetHash() ||
        GetModelNameHash() == MI_VAN_RIOT_2.GetName().GetHash() )
	{
		fragPhysicsLOD* physicsLOD = type->GetPhysics( 0 );
		if( physicsLOD )
		{
			physicsLOD->SetSmallestAngInertia( LIGHT_TANK_TURRET_MASS * rage::ANG_INERTIA_MAX_RATIO );
		}
	}

	if( GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RAMP ) )
	{
		int iBoneIndex = GetBoneIndex( VEH_BUMPER_F );
		if(iBoneIndex > -1)
		{
			int iGroupIndex = GetFragType()->GetGroupFromBoneIndex(0,iBoneIndex);

			if(iGroupIndex > -1)
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[iGroupIndex];
				pGroup->SetStrength( -1.0f );
				pGroup->SetForceTransmissionScaleUp(0.1f);
				pGroup->SetForceTransmissionScaleDown(0.1f);
			}
		}
	}
}

void CVehicleModelInfo::InitFromFragType()
{
	modelinfoAssert(m_data);
	// go through wheels
	{
		// search for instance meshes in left front and left rear wheel children
		int nFrontLeftChild = -1;
		if(GetBoneIndex(VEH_WHEEL_LF)!=-1)
		{
			int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(VEH_WHEEL_LF));
			if(nGroupIndex != -1)
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];

				if(GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()]->GetUndamagedEntity() 
				&& GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()]->GetUndamagedEntity()->GetLodGroup().GetCullRadius() > 0.0f)
				{
					nFrontLeftChild = pGroup->GetChildFragmentIndex();
				}

				// get front wheel radius from bound
				fragTypeChild* pFrontLeftChild = GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()];
				m_tyreRadiusF = pFrontLeftChild->GetUndamagedEntity()->GetBound()->GetBoundingBoxMax().GetZf();

			}
		}
		int nRearLeftChild = -1;
		if(GetBoneIndex(VEH_WHEEL_LR)!=-1)
		{
			int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(VEH_WHEEL_LR));
			if(nGroupIndex != -1)
			{
				fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];

				if(GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()]->GetUndamagedEntity() 
				&& GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()]->GetUndamagedEntity()->GetLodGroup().GetCullRadius() > 0.0f)
				{
					nRearLeftChild = pGroup->GetChildFragmentIndex();
					m_data->m_pStructure->m_bWheelInstanceSeparateRear = true;
				}

				// get rear wheel radius from bound
				fragTypeChild* pRearLeftChild = GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()];
				m_tyreRadiusR = pRearLeftChild->GetUndamagedEntity()->GetBound()->GetBoundingBoxMax().GetZf();

				// need to include wheel scale into the rim calculation because the vehicle guys are setting up rear rim radius == front for scaled wheels
				if(!m_data->m_pStructure->m_bWheelInstanceSeparateRear)
					m_rimRadiusR = m_rimRadiusF * m_tyreRadiusR / m_tyreRadiusF;
			}
		}

		m_data->m_pStructure->m_isRearWheel = 0;
		for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
		{
			eHierarchyId nWheelId = ms_aSetupWheelIds[i];

			if(GetBoneIndex(nWheelId)!=-1)
			{
				int nGroupIndex = GetFragType()->GetGroupFromBoneIndex(0, GetBoneIndex(nWheelId));
				if(nGroupIndex != -1)
				{
					// set mass and inertia of wheel
					fragTypeGroup* pGroup = GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];

					if (nWheelId != VEH_WHEEL_LF && nWheelId != VEH_WHEEL_RF)
						m_data->m_pStructure->m_isRearWheel |= (1 << i);

					// if this is a rear wheel, and we have a specific rear wheel child, then hook them up
					if(nWheelId!=VEH_WHEEL_LF && nWheelId!=VEH_WHEEL_RF && nRearLeftChild > -1)
					{
						m_data->m_pStructure->m_nWheelInstances[i][0] = (s8)pGroup->GetChildFragmentIndex();
						m_data->m_pStructure->m_nWheelInstances[i][1] = (s8)nRearLeftChild;
					}
					// else hook up to front left wheel child
					else if(nFrontLeftChild > -1)
					{
						m_data->m_pStructure->m_nWheelInstances[i][0] = (s8)pGroup->GetChildFragmentIndex();
						m_data->m_pStructure->m_nWheelInstances[i][1] = (s8)nFrontLeftChild;
					}

					// if we've got an instanced wheel, extract tyre and rim radius for wheel
					if(m_data->m_pStructure->m_nWheelInstances[i][0] > -1)
					{
						fragTypeChild* pChild = GetFragType()->GetPhysics(0)->GetAllChildren()[pGroup->GetChildFragmentIndex()];
						if(pChild->GetUndamagedEntity() && pChild->GetUndamagedEntity()->GetBound())
						{
							m_data->m_pStructure->m_fWheelTyreRadius[i] = pChild->GetUndamagedEntity()->GetBound()->GetBoundingBoxMax().GetZf();
                            m_data->m_pStructure->m_fWheelTyreWidth[i] = 2.0f*pChild->GetUndamagedEntity()->GetBound()->GetBoundingBoxMax().GetXf();
							m_data->m_pStructure->m_fWheelRimRadius[i] = GetRimRadius(nWheelId == VEH_WHEEL_LF || nWheelId == VEH_WHEEL_RF);

							if(m_data->m_pStructure->m_nWheelInstances[i][0] != m_data->m_pStructure->m_nWheelInstances[i][1] && m_data->m_pStructure->m_nWheelInstances[i][1] == (s8)nFrontLeftChild)
							{
								m_data->m_pStructure->m_fWheelScaleInv[i] = m_data->m_pStructure->m_fWheelTyreRadius[WHEEL_LF_INDEX] / m_data->m_pStructure->m_fWheelTyreRadius[i];
							}
						}
					}
				}
			}
		}


#if __PS3 && __ASSERT
	// ps3: early check for instanced wheel's geometry type:
	const int nWheelChild = (nRearLeftChild>-1)? nRearLeftChild : nFrontLeftChild;
	if(nWheelChild > -1)
	{
		fragDrawable* toDraw = GetFragType()->GetPhysics(0)->GetAllChildren()[nWheelChild]->GetUndamagedEntity();
		if(toDraw)
		{
			const rmcLodGroup& lodGroup = toDraw->GetLodGroup();
			
			for(int nLodIndex=0; nLodIndex<2; nLodIndex++)	// 0=hi LOD, 1=middle LOD
			{
				if(lodGroup.ContainsLod(nLodIndex))
				{
					const rmcLod &lod = lodGroup.GetLod(nLodIndex);
					
					const int numLods = lod.GetCount();
					for(int nLod=0; nLod<numLods; nLod++)
					{
						grmModel *pModel = lod.GetModel(nLod);
						if(pModel)
						{
							const int numGeoms = pModel->GetGeometryCount();
							for(int g=0; g<numGeoms; g++ )
							{
								if(pModel->HasGeometry(g))
								{
									grmGeometry& geom = pModel->GetGeometry(g);
									Assertf(geom.GetType()==grmGeometry::GEOMETRYQB, "%s: Instanced wheel uses non-QB geometry (lod=%d).", GetModelName(), nLodIndex);
								}
							}
						}
					}//for(int nLod=0; nLod<numLods; nLod++)...
				}
			}
		}

	} //if(nWheelChild > -1)...
#endif //__PS3 && __ASSERT...

	}// go through wheels...
}


//
// CVehicleModelInfo::ChooseVehicleColour: Choose two vehicle colours from the list of possible colours
// 
void CVehicleModelInfo::ChooseVehicleColour(u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6, s32 inc)
{
	modelinfoAssert(m_data);
	// just cycle colours instead of using random numbers
	if(m_numPossibleColours == 0)
		return;

	m_data->m_lastColUsed = (m_data->m_lastColUsed + inc) % m_numPossibleColours;
	col1 = m_possibleColours[0][m_data->m_lastColUsed];
	col2 = m_possibleColours[1][m_data->m_lastColUsed];
	col3 = m_possibleColours[2][m_data->m_lastColUsed];
	col4 = m_possibleColours[3][m_data->m_lastColUsed];
	col5 = m_possibleColours[4][m_data->m_lastColUsed];
	col6 = m_possibleColours[5][m_data->m_lastColUsed];
}

//
// CVehicleModelInfo::ChooseVehicleColour: Choose two vehicle colours from the list of possible colours
// 
void CVehicleModelInfo::GetNextVehicleColour(CVehicle *pVehicle, u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6)
{
	s32	newColour = 0;
	if (m_numPossibleColours > 1)
	{
		newColour = fwRandom::GetRandomNumberInRange(0, m_numPossibleColours);

		// First find the colour currently used.
		for (s32 n = 0; n < m_numPossibleColours; n++)
		{
			if (pVehicle->GetBodyColour1() == m_possibleColours[0][n] &&
				pVehicle->GetBodyColour2() == m_possibleColours[1][n] &&
				pVehicle->GetBodyColour3() == m_possibleColours[2][n])
			{
				newColour = (n+1) % m_numPossibleColours;
				break;
			}
		}
	}

	col1 = m_possibleColours[0][newColour];
	col2 = m_possibleColours[1][newColour];
	col3 = m_possibleColours[2][newColour];
	col4 = m_possibleColours[3][newColour];
	col5 = m_possibleColours[4][newColour];
	col6 = m_possibleColours[5][newColour];
}

struct sVehColDist
{
    s32 col;
    u32 numVehs;
    float distSqr;
};

//
// CVehicleModelInfo::ChooseVehicleColourFancy: Choose two vehicle colours from the list of possible colours
// This function goes through the vehicle pool and
// 
void CVehicleModelInfo::ChooseVehicleColourFancy(CVehicle *pNewVehicle, u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6)
{
	modelinfoAssert(m_data);
	modelinfoAssertf(m_numPossibleColours > 0, "%s doesn't have any colours setup", GetModelName());
	if (m_numPossibleColours<=0)
	{
		// should never hit here, but have been seeing issues with new vehicles when carcols.pso.meta has not been updated, best to fallback to something safe
		col1 = col2 = col3 = col4 = col5 = col6 = 0;
		return;
	}

	s32	n;
    struct sVehColDist vehCols[MAX_VEH_POSSIBLE_COLOURS];

	// for high end cars, keep track of the two lightest and darkest colors, we'd like to bias towards spawning them
	// we only do this if they're above 160 and below 50 for light/dark respectively,
	// some vehicles might not have any near white/black colors
	const u8 lightThreshold = 160;
	const u8 darkThreshold = 50;
	float distThresholdSqr = 50.f * 50.f;
	CRGBA light(0, 0, 0);
	s32 lightCol = -1;;
	CRGBA dark(255, 255, 255);
	s32 darkCol = -1;
	bool biasColorPick = GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS);

	// check if we have a valid livery and if it has any colors we need to restrict the choice to
	bool forceLiveryColor = false;
	s32 liveryId = pNewVehicle->GetLiveryId();
	if (liveryId > -1)
	{
		for (s32 i = 0; i < MAX_NUM_LIVERY_COLORS; ++i)
		{
			if (m_liveryColors[liveryId][i] > -1 && m_liveryColors[liveryId][i] < m_numPossibleColours)
			{
				forceLiveryColor = true;
				break;
			}
		}
	}

	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	for (n = 0; n < m_numPossibleColours; n++)
	{
        vehCols[n].col = n;
        vehCols[n].numVehs = 0;

		if (biasColorPick)
		{
			CRGBA col = GetVehicleColours()->GetVehicleColour(m_possibleColours[0][n]);
			if (col.GetRed() < dark.GetRed() && col.GetGreen() < dark.GetGreen() && col.GetBlue() < dark.GetBlue())
			{
				dark = col;
				darkCol = n;
			}
			if (col.GetRed() > light.GetRed() && col.GetGreen() > light.GetGreen() && col.GetBlue() > light.GetBlue())
			{
				light = col;
				lightCol = n;
			}
		}

		// if there's a player vehicle of the same type make sure that color will be the last one chosen
        // ignore this rule if we need to force a livery color though
		if (!forceLiveryColor && playerVehicle && pNewVehicle->GetModelIndex() == playerVehicle->GetModelIndex())
		{
			if (playerVehicle->GetBodyColour1() == m_possibleColours[0][n] &&
				playerVehicle->GetBodyColour2() == m_possibleColours[1][n] &&
				playerVehicle->GetBodyColour3() == m_possibleColours[2][n])
			{
                vehCols[n].distSqr = -1.f;
				vehCols[n].numVehs = 1000;
				continue;
			}
		}

        vehCols[n].distSqr = 999999.9f;
	}

    // clear unused entries
    for (u32 i = m_numPossibleColours; i < MAX_VEH_POSSIBLE_COLOURS; ++i)
    {
        vehCols[i].col = 0;
        vehCols[i].distSqr = -1;
        vehCols[i].numVehs = 1000;
    }

	// calculate number of color instances and closest distance to each
	const u32 numInsts = GetNumVehicleInstances();
	for (s32 i = 0; i < numInsts; ++i)
	{
		CVehicle* veh = GetVehicleInstance(i);
		if (veh && (veh != pNewVehicle))
		{
				// Find the colours for this occurence of this modelindex.
			for (n = 0; n < m_numPossibleColours; n++)
			{
				if (veh->GetBodyColour1() == m_possibleColours[0][n] &&
					veh->GetBodyColour2() == m_possibleColours[1][n] &&
					veh->GetBodyColour3() == m_possibleColours[2][n])
				{
					float fDist = DistSquared(veh->GetTransform().GetPosition(), pNewVehicle->GetTransform().GetPosition()).Getf();
					vehCols[n].distSqr = rage::Min(vehCols[n].distSqr, fDist);
                    vehCols[n].numVehs++;
				}
			}
		}
	}
	
	s32	PreferedColour = -1;
	if (forceLiveryColor)
	{
		float FurthestDist = 0.f;
		for (n = 0; n < MAX_NUM_LIVERY_COLORS; ++n)
		{
			s8 col = m_liveryColors[liveryId][n];
			if (col < 0)
				continue;

			if (vehCols[n].distSqr > FurthestDist)
			{
				FurthestDist = vehCols[col].distSqr;
				PreferedColour = col;
			}
		}

		Assertf(PreferedColour != -1, "No livery colors for vehicle '%s' (livery: %d, furthest dist: %.2f)", GetModelName(), liveryId, FurthestDist);
	}
	else
	{
        u32 maxNumVehsWithSameCol = GetMaxNumberOfSameColor();

		bool randomColor = true;
		if (biasColorPick)
		{
			// in order to bias towards white/black we need to make sure we don't break any rules
			bool canSpawnLight = light.GetRed() > lightThreshold && light.GetGreen() > lightThreshold && light.GetBlue() > lightThreshold;
			canSpawnLight &= vehCols[lightCol].numVehs < maxNumVehsWithSameCol;
			canSpawnLight &= vehCols[lightCol].distSqr > distThresholdSqr;

			bool canSpawnDark = dark.GetRed() < darkThreshold && dark.GetGreen() < darkThreshold && dark.GetBlue() < darkThreshold;
			canSpawnDark &= vehCols[darkCol].numVehs < maxNumVehsWithSameCol;
			canSpawnDark &= vehCols[darkCol].distSqr > distThresholdSqr;

			if (canSpawnLight || canSpawnDark)
			{
                randomColor = false;

				// roll dice, 25% to spawn black, 25% to spawn white and 50% to fallback to color based on highest distance
				float dice = fwRandom::GetRandomNumberInRange(0.f, 1.f);
				if (canSpawnDark && dice < 0.25f)
					PreferedColour = darkCol;
				else if (canSpawnLight && dice > 0.25f && dice < 0.5f)
					PreferedColour = lightCol;
				else
					randomColor = true;
			}
		}

		if (randomColor)
		{
			PreferedColour = vehCols[0].col;

			float distSqr = 0.0f;
			
			for (s32 i = 0; i < MAX_VEH_POSSIBLE_COLOURS; ++i)
			{
				if (vehCols[i].distSqr >= distSqr && vehCols[i].numVehs < maxNumVehsWithSameCol)
				{
					if (vehCols[i].distSqr != distSqr || fwRandom::GetRandomNumberInRange(0, 1) == 1)
					{
						distSqr = vehCols[i].distSqr;
						PreferedColour = vehCols[i].col;
					}
				}
			}
		}
	}

	if(Verifyf(PreferedColour != -1, "No colors for vehicle '%s'", GetModelName()))
	{
		m_data->m_lastColUsed = PreferedColour;

		col1 = m_possibleColours[0][m_data->m_lastColUsed];
		col2 = m_possibleColours[1][m_data->m_lastColUsed];
		col3 = m_possibleColours[2][m_data->m_lastColUsed];
		col4 = m_possibleColours[3][m_data->m_lastColUsed];
		col5 = m_possibleColours[4][m_data->m_lastColUsed];
		col6 = m_possibleColours[5][m_data->m_lastColUsed];
	}
}

bool CVehicleModelInfo::HasAnyColorsToSpawn() const
{
    if (!m_data)
        return true;

    const u32 numVehs = GetNumVehicleInstances();
    const u32 maxOfSameColor = GetMaxNumberOfSameColor();

    if (numVehs < maxOfSameColor * m_numPossibleColours)
        return true;

    return false;
}

int CVehicleModelInfo::SelectLicensePlateTextureIndex() const
{
	modelinfoAssertf(GetModelType() == MI_TYPE_VEHICLE, "ModelInfo isn't a vehicle");

	int selected = ms_VehicleColours ? ms_VehicleColours->GetLicensePlateData().GetDefaultTextureIndex() : -1;

	const atHashString& selectedName = CVehicleModelPlateProbabilities::SelectPlateTextureSet(m_plateProbabilities);
	if (selectedName.GetHash() != 0)
	{
		const CVehicleModelInfoPlates::TextureArray &texArray = ms_VehicleColours->GetLicensePlateData().GetTextureArray();
		for (int i=0; i<texArray.GetCount(); i++)
			if (selectedName == texArray[i].GetTextureSetName())
				return i;
	}

	return selected;
}

const vehicleLightSettings *CVehicleModelInfo::GetLightSettings() const
{
	modelinfoAssert(ms_VehicleColours);
	modelinfoAssert(ms_VehicleColours->m_Lights.GetCount() > 0);
	modelinfoAssert(m_lightSettings < ms_VehicleColours->m_Lights.GetCount());
	
	return &ms_VehicleColours->m_Lights[m_lightSettings];
}

const sirenSettings *CVehicleModelInfo::GetSirenSettings() const
{
	modelinfoAssert(ms_VehicleColours);
	if( m_sirenSettings > 0 )
	{
		modelinfoAssert(ms_VehicleColours->m_Sirens.GetCount() > 0);
		modelinfoAssert(m_sirenSettings < ms_VehicleColours->m_Sirens.GetCount());
		
		return &ms_VehicleColours->m_Sirens[m_sirenSettings];
	}
	
	return NULL;
}

int CVehicleModelInfo::GetStreamingDependencies(strIndex* iIndicies, int iMaxNumIndicies)
{
	if(!m_bDependenciesValid)
	{
		CalculateStreamingDependencies();
	}

	for(u8 i =0; i < m_uNumDependencies && (s32)i < iMaxNumIndicies; i++)
	{
		iIndicies[i] = m_iDependencyStreamingIndicies[i];
	}

	return (int)m_uNumDependencies;
}

void CVehicleModelInfo::CalculateStreamingDependencies()
{
	// Find all anim groups referenced in our model info
	const CVehicleLayoutInfo* pInfo = GetVehicleLayoutInfo();


	modelinfoAssertf(pInfo, "Vehicle %s is missing a vehicle layout info",GetModelName());

	int iActualCount = 0;
	if(pInfo)
	{
		s32 iAnimDictIndicesList[MAX_VEHICLE_STR_DEPENDENCIES];
		int iNewCount = pInfo->GetAllStreamedAnimDictionaries(iAnimDictIndicesList,MAX_VEHICLE_STR_DEPENDENCIES);

		iNewCount = Min(iNewCount, MAX_VEHICLE_STR_DEPENDENCIES);
		for(int i =0; i < iNewCount; i++)
		{			
			// Find the streaming index for the group
			if(iAnimDictIndicesList[i] > -1)
			{
				m_iDependencyStreamingIndicies[iActualCount++] = fwAnimManager::GetStreamingModule()->GetStreamingIndex(strLocalIndex(iAnimDictIndicesList[i]));
			}
		}
	}

    //Temporary hack to stream in the tow hook prop for the towtruck and cargobob
    if( GetModelNameHash() == MI_CAR_TOWTRUCK.GetName().GetHash() || 
		GetModelNameHash() == MI_HELI_CARGOBOB.GetName().GetHash() || 
		GetModelNameHash() == MI_HELI_CARGOBOB2.GetName().GetHash() ||
		GetModelNameHash() == MI_HELI_CARGOBOB3.GetName().GetHash() ||
		GetModelNameHash() == MI_HELI_CARGOBOB4.GetName().GetHash() ||
		GetModelNameHash() == MI_HELI_CARGOBOB5.GetName().GetHash())
    {
        m_iDependencyStreamingIndicies[iActualCount++] = CModelInfo::GetStreamingModule()->GetStreamingIndex(strLocalIndex(MI_PROP_HOOK));
    }

	// expression loading code
	strLocalIndex expressionDataFileIndex = GetExpressionDictionaryIndex();

    // Stream in the expression dictionary
    if (expressionDataFileIndex != -1  && g_ExpressionDictionaryStore.IsObjectInImage(expressionDataFileIndex))
    {
        m_iDependencyStreamingIndicies[iActualCount++] = g_ExpressionDictionaryStore.GetStreamingIndex(expressionDataFileIndex);
    }

	// vehicle meta data
#if LIVE_STREAMING
	if (GetVehicleMetaDataFileIndex() == -1)
		SetVehicleMetaDataFile(GetModelName());
#endif
	if (GetVehicleMetaDataFileIndex() != -1)
		m_iDependencyStreamingIndicies[iActualCount++] = g_fwMetaDataStore.GetStreamingIndex(GetVehicleMetaDataFileIndex());


	taskAssertf(iActualCount <= MAX_VEHICLE_STR_DEPENDENCIES, "CVehicleModelInfo::CalculateStreamingDependencies - %s has too many dependencies", GetModelName());
	m_uNumDependencies = (u8)iActualCount;
	m_bDependenciesValid = true;
}

//-------------------------------------------------------------------------
// Constructs a collision model to test against vehicles in order to determine
// available cover points
//-------------------------------------------------------------------------
void CVehicleModelInfo::ConstructStaticData( void )
{
}


#if !__FINAL

//-------------------------------------------------------------------------
// Draws a debug outline representing the collision model used to
// judge the height of the vehicle at various points
//-------------------------------------------------------------------------
void CVehicleModelInfo::DebugDrawCollisionModel( Matrix34& DEBUG_DRAW_ONLY(mMat) )
{
#if DEBUG_DRAW
	modelinfoAssert(m_data);
	s32 i=0;
	s32 numCapsules = 6;
	Matrix34 capsuleMatrix[6];

	// Calculate the max bounding box extents
	Vector3 vMaxBoundingBox( Max(fabs(GetBoundingBoxMax().x), fabs(GetBoundingBoxMin().x)), 
		Max(fabs(GetBoundingBoxMax().y), fabs(GetBoundingBoxMin().y)), 
		Max(fabs(GetBoundingBoxMax().z), fabs(GetBoundingBoxMin().z)) );

	for(i=0; i<numCapsules; i++)
	{
		capsuleMatrix[i].Identity();
		capsuleMatrix[i].RotateX(-HALF_PI);
	}


	// Front left
	capsuleMatrix[0].d = Vector3(0.5f*GetBoundingBoxMax().x, 0.66f*GetBoundingBoxMax().y,	0.0f);
	// Mid left
	capsuleMatrix[1].d = Vector3(0.5f*GetBoundingBoxMax().x, 0.0f,							0.0f);
	// Back left
	capsuleMatrix[2].d = Vector3(0.5f*GetBoundingBoxMax().x, 0.66f*GetBoundingBoxMin().y,	0.0f);
	// Front Right
	capsuleMatrix[3].d = Vector3(0.5f*GetBoundingBoxMin().x, 0.66f*GetBoundingBoxMax().y,	0.0f);
	// Mid right
	capsuleMatrix[4].d = Vector3(0.5f*GetBoundingBoxMin().x, 0.0f,							0.0f);
	// Back right
	capsuleMatrix[5].d = Vector3(0.5f*GetBoundingBoxMin().x, 0.66f*GetBoundingBoxMin().y,	0.0f);

	for(i=0; i<numCapsules; i++)
	{
		mMat.Transform(capsuleMatrix[i].d);
		grcDebugDraw::Line( capsuleMatrix[i].d + Vector3( 0.0f, 0.0f, -5.0f ), capsuleMatrix[i].d + Vector3( 0.0f, 0.0f, m_data->m_afHeightMap[i] + GetBoundingBoxMin().z), Color32(0xff, 0x00, 0x00, 0xff) );
		grcDebugDraw::Sphere( capsuleMatrix[i].d + Vector3( 0.0f, 0.0f, m_data->m_afHeightMap[i]+ GetBoundingBoxMin().z), 0.2f, Color32(0x00, 0xff, 0x00, 0xff) );
	}
#endif // DEBUG_DRAW
}
#endif

//-------------------------------------------------------------------------
// Returns the next coverpoint local offset in the direction given
//-------------------------------------------------------------------------
void CVehicleModelInfo::FindNextCoverPointOffsetInDirection( Vector3& vOut, const Vector3& vStart, const bool bClockwise ) const
{
	modelinfoAssert(m_data);
	float fCurrentBest = -999.0f;
	vOut = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 vStartVec = vStart;
	vStartVec.z = 0.0f;
	vStartVec.Normalize();
	Vector3 vRight;
	vRight.Cross(vStartVec, Vector3(0.0f, 0.0f, 1.0f));

	for( s32 i = 0; i < m_data->m_iCoverPoints; i++ )
	{
		// Don't check the starting point
		if( vStart != m_data->m_aCoverPoints[i].m_vLocalOffset )
		{
			Vector3 vThisVec = m_data->m_aCoverPoints[i].m_vLocalOffset;
			vThisVec.z = 0.0f;
			vThisVec.Normalize();
			float fDot = vStartVec.Dot(vThisVec);
			float fRightDot = vRight.Dot(vThisVec);
			if( ( fRightDot > 0.0f && bClockwise ) ||
				( fRightDot < 0.0f && !bClockwise ) )
			{
				fDot -= 2.0f;
			}
			if( fDot > fCurrentBest )
			{
				fCurrentBest = fDot;
				vOut = m_data->m_aCoverPoints[i].m_vLocalOffset;
			}
		}
	}
}

s32 CVehicleModelInfo::GetEntryExitPointIndex(const CEntryExitPoint* pEntryExitPoint) const
{
	modelinfoAssert(m_data);
	return m_data->m_SeatInfo.GetEntryExitPointIndex(pEntryExitPoint);
}

//-------------------------------------------------------------------------
// Constructs a collision model to test against vehicles in order to determine
// available cover points
//-------------------------------------------------------------------------
void CVehicleModelInfo::SizeCoverTestForVehicle(phInstGta* UNUSED_PARAM(pTestInst), Vector3* aTestPositions, float& fCapsuleRadius, float& fCapsuleLength)
{
	// Calculate the max bounding box extents
	Vector3 vMaxBoundingBox(Max(fabs(GetBoundingBoxMax().x), fabs(GetBoundingBoxMin().x)), 
							Max(fabs(GetBoundingBoxMax().y), fabs(GetBoundingBoxMin().y)), 
							Max(fabs(GetBoundingBoxMax().z), fabs(GetBoundingBoxMin().z)) );

	fCapsuleRadius = 0.1f * vMaxBoundingBox.y;
	fCapsuleLength = vMaxBoundingBox.z * 10.0f;

	// Front left
	aTestPositions[0] = Vector3(0.5f*GetBoundingBoxMax().x, 0.66f*GetBoundingBoxMax().y,	0.0f);
	// Mid left
	aTestPositions[1] = Vector3(0.5f*GetBoundingBoxMax().x, 0.0f,							0.0f);
	// Back left
	aTestPositions[2] = Vector3(0.5f*GetBoundingBoxMax().x, 0.66f*GetBoundingBoxMin().y,	0.0f);
	// Front Right
	aTestPositions[3] = Vector3(0.5f*GetBoundingBoxMin().x, 0.66f*GetBoundingBoxMax().y,	0.0f);
	// Mid right
	aTestPositions[4] = Vector3(0.5f*GetBoundingBoxMin().x, 0.0f,							0.0f);
	// Back right
	aTestPositions[5] = Vector3(0.5f*GetBoundingBoxMin().x, 0.66f*GetBoundingBoxMin().y,	0.0f);
}

//-------------------------------------------------------------------------
// Destroy any static data associated with vehicle models
//-------------------------------------------------------------------------
void  CVehicleModelInfo::DestroyStaticData( void )
{ 
}


// ---- HD streaming stuff ----
//

#ifdef RSG_PC
#define HD_REF_LIMIT 1500 //PC has more things going on, so for high end stuff we need to bump this number compared with NG consoles
#else
#define HD_REF_LIMIT 1000 //if you this this number of HD refs something has gone wrong
#endif

// PURPOSE: Add a reference to this archetype and add ref to any HD assets loaded
void CVehicleModelInfo::AddHDRef( bool bRenderRef )
{
	AddRef();
	if (bRenderRef){
		Assert(m_numHDRenderRefs < HD_REF_LIMIT);
		m_numHDRenderRefs++;
	} else
	{
		Assert(m_numHDRefs < HD_REF_LIMIT);
		m_numHDRefs++;
	}
}

void CVehicleModelInfo::RemoveHDRef( bool bRenderRef )
{
	Assert(GetNumHdRefs() > 0);

	RemoveRef();

	{
		if (bRenderRef){
			Assert(m_numHDRenderRefs > 0);
			if (m_numHDRenderRefs > 0)
			{
				m_numHDRenderRefs--;
			}
		} else 
		{
			Assert(m_numHDRefs > 0);
			if (m_numHDRefs > 0)
			{
				m_numHDRefs--;
			}
		}

		if (GetNumHdRefs() == 0)
		{
			modelinfoDisplayf("+++ vehicle cleanup : %s", GetModelName());
			ReleaseHDFiles();
		} 
	}
}

void CVehicleModelInfo::AddToHDInstanceList(size_t id)
{
	modelinfoAssert(id != 0);
	modelinfoAssert(!m_HDInstanceList.IsMemberOfList((void*)id));

	if (GetHasHDFiles())
	{
		// set this to true to disable HD vehicle switching
		static bool bDisableHD = false;
		if (bDisableHD){
			return;
		}

		// if this is the first one then issue the HD requests for this type
		if (m_HDInstanceList.GetHeadPtr() == NULL){
			RequestAndRefHDFiles();
		}

		m_HDInstanceList.Add((void*)id);
		ms_numHdVehicles++;
	}
}

void CVehicleModelInfo::RemoveFromHDInstanceList(size_t id)
{
	modelinfoAssert(id != 0);

	if (m_HDInstanceList.IsMemberOfList((void*)id))
	{
		m_HDInstanceList.Remove((void*)id);
		ms_numHdVehicles--;
		
		// if this was the last one then free up the HD requests for this type
		if (m_HDInstanceList.GetHeadPtr() == NULL)
		{
			ReleaseRequestOrRemoveRefHD();
		}
	}
}

void CVehicleModelInfo::SetupHDSharedTxd(const char* pName)
{
	if ( pName == NULL || strcmp(pName, "NULL") == 0 || strcmp(pName, "null") == 0)
	{
		return;
	}

	char HDName[255];
	u32 size = ustrlen(pName);

	if (size > 250){
		return;
	}

	strncpy(HDName, pName, size);

	HDName[size] = '+';
	HDName[size+1] = 'h';
	HDName[size+2] = 'i';
	HDName[size+3] = '\0';

	s32 targetTxdIdx = g_TxdStore.FindSlot(pName).Get();
	s32 HDtxdIdx = g_TxdStore.FindSlot(HDName).Get();

	if (HDtxdIdx != -1 && targetTxdIdx != -1)
	{
		CTexLod::StoreHDMapping(STORE_ASSET_TXD, targetTxdIdx, HDtxdIdx);
	}
}

// name:		SetFragment
// description:	Set texture dictionary for object
void CVehicleModelInfo::SetupHDFiles(const char* pName)
{
 	if ( pName == NULL || strcmp(pName, "NULL") == 0 || strcmp(pName, "null") == 0)
 	{
 		return;
 	}
	if (!(GetHDDist() > 0.0f)){
		return;
	}

	char HDName[255];
	u32 size = ustrlen(pName);

	if (size > 250){
		return;
	}

	strncpy(HDName, pName, size);

	HDName[size] = '_';
	HDName[size+1] = 'h';
	HDName[size+2] = 'i';
	HDName[size+3] = '\0';

	m_HDfragIdx = g_FragmentStore.FindSlot(HDName).Get();
	if (m_HDfragIdx == -1){
		m_HDfragIdx = g_FragmentStore.AddSlot(HDName).Get();
	}

	HDName[size] = '+';		// HD txd name is different because it is machine generated

	m_HDtxdIdx = g_TxdStore.FindSlot(HDName).Get();

	if (m_HDtxdIdx != -1)
	{
		CTexLod::StoreHDMapping(STORE_ASSET_TXD, GetAssetParentTxdIndex(), m_HDtxdIdx);
	}

	g_FragmentStore.SetParentTxdForSlot(strLocalIndex(m_HDfragIdx), strLocalIndex(GetAssetParentTxdIndex()));
}

void CVehicleModelInfo::ConfirmHDFiles(void){

	if (GetHasHDFiles()){
		// verify existence of HD files
		if (m_HDfragIdx != -1 && g_FragmentStore.IsObjectInImage(strLocalIndex(m_HDfragIdx))){
			return;
		}
	}
	// verification failed. Set HD dist to invalid
	SetHDDist(-1.0f);
}

// issue the HD request for this vehicle type (if it has one set up)
void CVehicleModelInfo::RequestAndRefHDFiles(void){

	if (GetAreHDFilesLoaded())
	{
		AddHDRef(false);
		return;
	}

	if (!GetRequestLoadHDFiles()){
		VehicleHDStreamReq* pNewReq = rage_new(VehicleHDStreamReq);
		Assert(pNewReq);
		if (pNewReq){
			pNewReq->m_Requests.PushRequest(GetHDFragmentIndex(), g_FragmentStore.GetStreamingModuleId());
			pNewReq->m_pTargetVehicleMI = this;

			ms_HDStreamRequests.Add(pNewReq);			// HD ref is added once loaded (in update

			SetRequestLoadHDFiles(true);
			return;
		}
	}

	modelinfoAssertf(false, "Error in CVehicleModelInfo::RequestAndRefHDFiles");
}

// remove the request if it still outstanding, otherwise remove the ref
void CVehicleModelInfo::ReleaseRequestOrRemoveRefHD(void){

	if (GetAreHDFilesLoaded())
	{
		Assert(m_numHDRefs > 0);
		RemoveHDRef(false);
		return;
	}

	if (GetRequestLoadHDFiles()){
		// remove requested files
		fwPtrNode* pLinkNode = ms_HDStreamRequests.GetHeadPtr();
		while(pLinkNode) {
			VehicleHDStreamReq*	pStreamRequest = static_cast<VehicleHDStreamReq*>(pLinkNode->GetPtr());
			pLinkNode = pLinkNode->GetNextPtr();

			if (pStreamRequest && pStreamRequest->m_pTargetVehicleMI == this)
			{
				SetRequestLoadHDFiles(false);
				ms_HDStreamRequests.Remove(pStreamRequest);
				delete pStreamRequest;
			}
		}
		return;
	}

	modelinfoAssertf(false, "Error in CVehicleModelInfo::ReleaseRequestOrRemoveRefHD");
}

// cleanup HD state back to normal state (whether HD requested or loaded)
void CVehicleModelInfo::ReleaseHDFiles(void)
{
	modelinfoAssert(m_data);
	modelinfoAssert(GetAreHDFilesLoaded());
	modelinfoAssert(GetHasHDFiles());
	modelinfoAssert(GetNumHdRefs() == 0);

	if (GetHasHDFiles())
	{
		if(m_data->m_pHDShaderEffectType)
		{
			gtaFragType* pFrag = GetHDFragType();
			if(pFrag)
			{
				// check txds are set as expected
				vehicleAssertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());

				m_data->m_pHDShaderEffectType->RestoreModelInfoDrawable(pFrag->GetCommonDrawable());
			}
			m_data->m_pHDShaderEffectType->RemoveRef();
			m_data->m_pHDShaderEffectType = NULL;
		}

		if (GetAreHDFilesLoaded())
		{
			// remove loaded files ref
			modelinfoAssert(GetHDFragmentIndex() != -1);
			g_FragmentStore.RemoveRef(strLocalIndex(GetHDFragmentIndex()), REF_OTHER);
			SetAreHDFilesLoaded(false);

#if __BANK
				int i;
				for (i = 0; i < ms_HDVehicleInfos.GetCount(); ++i)
				{
					if (ms_HDVehicleInfos[i].pVMI == this)
					{
						ms_HDVehicleInfos[i].count--;
						if (ms_HDVehicleInfos[i].count == 0)
						{
							ms_HDVehicleInfos.Delete(i);
						}
						break;
					}
				}
#endif
		}
	}
}


// update the state of any requests, start triggering requests if desired
void CVehicleModelInfo::UpdateHDRequests(void){

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::HDVEHICLES);
	fwPtrNode* pLinkNode = ms_HDStreamRequests.GetHeadPtr();

	while(pLinkNode)
	{
		VehicleHDStreamReq*	pStreamRequest = static_cast<VehicleHDStreamReq*>(pLinkNode->GetPtr());
		pLinkNode = pLinkNode->GetNextPtr();

		if (pStreamRequest)
		{
			if (pStreamRequest->m_Requests.HaveAllLoaded() && pStreamRequest->m_pTargetVehicleMI && (pStreamRequest->m_pTargetVehicleMI->m_data != NULL)){
				// add refs 
				CVehicleModelInfo* pVMI = pStreamRequest->m_pTargetVehicleMI;
				modelinfoAssert(pVMI);
				g_FragmentStore.AddRef(strLocalIndex(pVMI->GetHDFragmentIndex()), REF_OTHER);

				pVMI->SetAreHDFilesLoaded(true);
				pVMI->SetRequestLoadHDFiles(false);
				ms_HDStreamRequests.Remove(pStreamRequest);
				delete pStreamRequest;

				pVMI->SetupHDCustomShaderEffects();
				pVMI->AddHDRef(false);

			#if __BANK
				int i;
				bool found = false;
				for (i = 0; i < ms_HDVehicleInfos.GetCount(); ++i)
				{
					if (ms_HDVehicleInfos[i].pVMI == pVMI)
					{
						found = true;
						break;
					}
				}

				if (found)
				{
					ms_HDVehicleInfos[i].count++;
				}
				else
				{
					HDVehilceRefCount vmi = { pVMI, 1 };
					ms_HDVehicleInfos.PushAndGrow(vmi);
				}
			#endif

			}
		}
	}
}

gtaFragType* CVehicleModelInfo::GetHDFragType() const
{
	gtaFragType* ret = NULL;
	strLocalIndex HDfragIdx = strLocalIndex(GetHDFragmentIndex());
	if (HDfragIdx != -1)
	{
		if (g_FragmentStore.HasObjectLoaded(HDfragIdx))
		{
			ret = static_cast<gtaFragType*>(g_FragmentStore.Get(HDfragIdx));
		}
	}

	return(ret);
}

bool CVehicleModelInfo::SetupHDCustomShaderEffects()
{
	modelinfoAssert(m_data);
	gtaFragType* pFrag = GetHDFragType();

	if(pFrag)
	{
		rmcDrawable *pDrawable = pFrag->GetCommonDrawable();

		if (pDrawable)
		{
			#if __ASSERT
				extern char* CCustomShaderEffectBase_EntityName;
				CCustomShaderEffectBase_EntityName = (char*)this->GetModelName(); // debug only: defined in CCustomShaderEffectBase.cpp
			#endif //__ASSERT...

			if(m_data->m_pHDShaderEffectType)
			{	// re-initialise CSE HD (if created):
				m_data->m_pHDShaderEffectType->Recreate(pDrawable, this);
				m_data->m_pHDShaderEffectType->Initialise(pDrawable);
			}
			else
			{	// create CSE HD (if not created already):
				m_data->m_pHDShaderEffectType = static_cast<CCustomShaderEffectVehicleType*>(CCustomShaderEffectBaseType::SetupMasterVehicleHDInfo(this));
			}

			if(m_data->m_pHDShaderEffectType)
			{
				modelinfoAssertf(m_data->m_pHDShaderEffectType->AreShadersValid(pDrawable), "%s: Model has wrong shaders applied", GetModelName());
				m_data->m_pHDShaderEffectType->SetIsHighDetail(true);
			}

			return(TRUE);
		}
		
	}

	return(FALSE);
}

fwModelId CVehicleModelInfo::GetRandomTrailer() const
{
	fwModelId ret;
	if (m_trailers.GetCount() == 0)
	{
		return ret;
	}

    s32 idx = fwRandom::GetRandomNumberInRange(0, m_trailers.GetCount());

	u32 spawnTrailer = fwModelId::MI_INVALID;
	u32 lastTimeUsed = fwTimer::GetTimeInMilliseconds();
	for (s32 i = 0; i < m_trailers.GetCount(); ++i, ++idx)
	{
		if (idx == m_trailers.GetCount())
			idx = 0;

		fwModelId trailerModelId;
		CVehicleModelInfo* modelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(m_trailers[idx], &trailerModelId);

        // TR2 and TR4 not allowed in MP
        if (CNetwork::IsGameInProgress() && ((u32)trailerModelId.GetModelIndex() == MI_TRAILER_TR2 || (u32)trailerModelId.GetModelIndex() == MI_TRAILER_TR4))
            continue;

		if(modelinfoVerifyf(modelInfo, "Invalid model info for vehicle '%s'", m_trailers[idx].GetCStr()))
		{
			u32 lastTimeUsedModel = modelInfo->GetLastTimeUsed();
			if (lastTimeUsedModel < lastTimeUsed)
			{
				lastTimeUsed = lastTimeUsedModel;
				spawnTrailer = trailerModelId.GetModelIndex();
			}
		}
	}

    ret.SetModelIndex(spawnTrailer);
	return ret;
}

fwModelId CVehicleModelInfo::GetRandomLoadedTrailer() const
{
	fwModelId ret;
	if (m_trailers.GetCount() == 0)
		return ret;

    s32 idx = fwRandom::GetRandomNumberInRange(0, m_trailers.GetCount());

	float smallestOccurrence = 999.f;
    for (s32 i = 0; i < m_trailers.GetCount(); ++i, ++idx)
    {
        if (idx == m_trailers.GetCount())
            idx = 0;
		
		CVehicleModelInfo* vmi = NULL;
        fwModelId id = GetTrailer(idx, vmi);
        if(vmi && CModelInfo::HaveAssetsLoaded(id))
		{
			// TR2 and TR4 not allowed in MP
			if (CNetwork::IsGameInProgress() && ((u32)id.GetModelIndex() == MI_TRAILER_TR2 || (u32)id.GetModelIndex() == MI_TRAILER_TR4))
				continue;

			float occ = (vmi->GetNumVehicleModelRefs() + vmi->GetNumVehicleModelRefsParked() + 1) / (float)vmi->GetVehicleFreq();
			if (occ < smallestOccurrence)
			{
				smallestOccurrence = occ;
				ret = id;
			}
		}
    }

    return ret;
}

fwModelId CVehicleModelInfo::GetTrailer(u32 idx, CVehicleModelInfo*& vmi) const
{
    fwModelId trailerModelId;
    if (idx >= m_trailers.GetCount())
        return trailerModelId;

    vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(m_trailers[idx], &trailerModelId);
    return trailerModelId;
}

bool CVehicleModelInfo::GetIsCompatibleTrailer(CVehicleModelInfo * trailerModelInfo)
{
	if(trailerModelInfo)
	{
		u32 trailerHash = trailerModelInfo->GetModelNameHash();

		// GTAV - HACK to fix B*1944449 - if this is tanker2 treat it like tanker
		if( MI_TRAILER_TANKER2.IsValid() && MI_TRAILER_TANKER2.GetName().GetHash() == trailerHash )
		{
			trailerHash = MI_TRAILER_TANKER.GetName().GetHash();
		}

		for( int i = 0; i < m_trailers.GetCount(); i++ )
		{
			if( trailerHash == m_trailers[i])
			{
				return true;
			}
		}

		if( GetModelNameHash() == MI_CAR_PHANTOM2.GetName().GetHash() )
		{
			if( trailerHash == MI_TRAILER_TRAILERLARGE.GetName().GetHash() )
			{
				return true;
			}
		}

		for(int i = 0; i < m_additionalTrailers.GetCount(); i++)
		{
			if( trailerHash == m_additionalTrailers[i] )
			{
				return true;
			}
		}
	}

	return false;
}

u32 CVehicleModelInfo::GetRandomDriver() const
{
	if (m_drivers.GetCount() == 0)
	{
		return fwModelId::MI_INVALID;
	}

	//u32 spawnDriver = fwModelId::MI_INVALID;
	fwModelId spawnDriverId;
	fwModelId spawnDriverLoadedId;
	s32 smallestOccurrence = 999;
	s32 smallestOccurrenceLoaded = 999;
	s32 largestOccurrence = 0;
	for (s32 i = 0; i < m_drivers.GetCount(); ++i)
	{
		fwModelId driverModelId;
		CPedModelInfo* modelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(m_drivers[i].GetDriverName(), &driverModelId);

		if(modelinfoVerifyf(modelInfo, "Invalid model info for ped '%s'", m_drivers[i].GetDriverName().GetCStr()))
		{
			s32 occ = modelInfo->GetNumPedModelRefs() + 1;
			if (occ < smallestOccurrence)
			{
				smallestOccurrence = occ;
				spawnDriverId = driverModelId;
			}

			if (occ > largestOccurrence)
			{
				largestOccurrence = occ;
			}

			if (CModelInfo::HaveAssetsLoaded(driverModelId) && occ < smallestOccurrenceLoaded)
			{
				smallestOccurrenceLoaded = occ;
				spawnDriverLoadedId = driverModelId;
			}
		}
	}

	if (!spawnDriverLoadedId.IsValid())
	{
		if (smallestOccurrence == 999 && largestOccurrence == 999)	
		{
			s32 idx = fwRandom::GetRandomNumberInRange(0, GetDriverCount());
			CModelInfo::GetBaseModelInfoFromName(m_drivers[idx].GetDriverName(), &spawnDriverId);
		}
	}
	else
	{
		spawnDriverId = spawnDriverLoadedId;
	}

	return spawnDriverId.GetModelIndex();
}

void CVehicleModelInfo::AddVehicleInstance(CVehicle* veh)
{
	modelinfoAssert(m_data);
	if (!veh)
		return;

	s32 poolIndex = (s32) CVehicle::GetPool()->GetJustIndex(veh);
	if (Verifyf(poolIndex > -1 && poolIndex <= 65535, "Too many vehicles. See declaration of m_vehicleInstances."))
	{
#if __DEV
		for (s32 i = 0; i < m_data->m_vehicleInstances.GetCount(); ++i)
		{
			Assertf(m_data->m_vehicleInstances[i] != static_cast<u16>(poolIndex), "Stored vehicle instance already in list! index: %d - stored index: %d - truncIndex: %d - 0x%p", i, m_data->m_vehicleInstances[i], poolIndex, veh);
		}
#endif
		m_data->m_vehicleInstances.PushAndGrow(static_cast<u16>(poolIndex));
	}
}

void CVehicleModelInfo::RemoveVehicleInstance(CVehicle* veh)
{
	modelinfoAssert(m_data);
	if (!veh)
		return;

	s32 poolIndex = (s32) CVehicle::GetPool()->GetJustIndex(veh);
	if (Verifyf(poolIndex > -1 && poolIndex <= 65535, "Too many vehicles. See declaration of m_vehicleInstances."))
	{
		for (s32 i = 0; i < m_data->m_vehicleInstances.GetCount(); ++i)
		{
			if (m_data->m_vehicleInstances[i] == static_cast<u16>(poolIndex))
			{
				m_data->m_vehicleInstances.DeleteFast(i);
				break;
			}
		}
	}
}

CVehicle* CVehicleModelInfo::GetVehicleInstance(u32 index)
{
	modelinfoAssert(m_data);
	if (Verifyf(index < m_data->m_vehicleInstances.GetCount(), "CVehicleModelInfo::GetVehicleInstances - index out of range"))
	{
		s32 poolIndex = (s32)m_data->m_vehicleInstances[index];
		return CVehicle::GetPool()->GetSlot(poolIndex);
	}

	return NULL;
}

bool CVehicleModelInfo::HasAnyMissionInstances()
{
	if (!m_data)
		return false;
	
	const u32 numInsts = GetNumVehicleInstances();
	for (s32 i = 0; i < numInsts; ++i)
	{
		CVehicle* veh = GetVehicleInstance(i);
		if (veh)
		{
			if (veh->m_nDEflags.nPopType == POPTYPE_MISSION)
				return true;
		}
	}

	return false;
};

float CVehicleModelInfo::GetDistanceSqrToClosestInstance(const Vector3& pos, bool cargens)
{
    if (!m_data)
        return 0.f;

	float minDist = 999999.f;

	const u32 numInsts = GetNumVehicleInstances();
	for (s32 i = 0; i < numInsts; ++i)
	{
		CVehicle* veh = GetVehicleInstance(i);
		if (veh)
		{
			if (!cargens || veh->m_nDEflags.nPopType == POPTYPE_RANDOM_PARKED)
			{
				if(!veh->m_nVehicleFlags.bIsInVehicleReusePool)
				{
					float dist = DistSquared(veh->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(pos)).Getf();
					minDist = dist < minDist ? dist : minDist;
				}
			}
		}
	}

	if (cargens)
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName(GetModelNameHash());

		for (s32 i = 0; i < CTheCarGenerators::m_scheduledVehicles.GetCount(); ++i)
		{
			if (!CTheCarGenerators::m_scheduledVehicles[i].valid)
				continue;

			if (CTheCarGenerators::m_scheduledVehicles[i].carModelId.GetModelIndex() == modelId.GetModelIndex())
			{
				float dist = DistSquared(VECTOR3_TO_VEC3V(CTheCarGenerators::m_scheduledVehicles[i].creationMatrix.d), VECTOR3_TO_VEC3V(pos)).Getf();
				minDist = dist < minDist ? dist : minDist;
			}
		}
	}

	return minDist;
}

#if __BANK
void CVehicleModelInfo::DumpVehicleInstanceDataCB()
{
	CEntity* entity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if (!entity || !entity->GetIsTypeVehicle())
	{
		Displayf("No selected vehicle\n");
		return;
	}

	CVehicle* selectedVeh = (CVehicle*)entity;
	CVehicleModelInfo* vmi = selectedVeh->GetVehicleModelInfo();
	Assert(vmi);

	Displayf("**** Vehicle instance data for '%s' (%u / %d) ****\n", vmi->GetModelName(), vmi->GetNumVehicleInstances(), (int) CVehicle::GetPool()->GetNoOfUsedSpaces());
	for (s32 i = 0; i < vmi->GetNumVehicleInstances(); ++i)
	{
		CVehicle* veh = vmi->GetVehicleInstance(i);
		if (veh)
			Displayf("** Veh %d - 0x%p - (%.2f, %.2f, %.2f) **\n", i, veh, veh->GetTransform().GetPosition().GetXf(), veh->GetTransform().GetPosition().GetYf(), veh->GetTransform().GetPosition().GetZf());
		else
			Displayf("** Veh %d - null veh! **\n", i);
	}
}
#endif

bool CVehicleModelInfo::GetCanPassengersBeKnockedOff()
{
	return GetIsBike() || GetIsQuadBike() || GetIsAmphibiousQuad();
}

const bool CVehicleModelInfo::AllowsVehicleEntryWhenStoodOn(const CVehicle& veh)
{
	const CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();
	if (!pModelInfo)
		return false;

	if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CONSIDERED_FOR_VEHICLE_ENTRY_WHEN_STOOD_ON))
		return true;
	
	switch (veh.GetVehicleType())
	{
		case VEHICLE_TYPE_BOAT:
		case VEHICLE_TYPE_SUBMARINE:
		case VEHICLE_TYPE_PLANE:
			if (!pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_REJECT_ENTRY_TO_VEHICLE_WHEN_STOOD_ON))
				return true;
		default:
			return false;
	}
}

#if !__SPU
Vec3V_Out CVehicleModelInfo::CalculateWheelOffset(eHierarchyId wheelId) const
{
	Vec3V vecOffset(V_ZERO);

	if(GetBoneIndex(wheelId) > -1)
	{
		const crSkeletonData* pSkeletonData = NULL;
		if(GetFragType())
			pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
		else if(GetDrawable())
			pSkeletonData = GetDrawable()->GetSkeletonData();

		if(pSkeletonData)
		{
			vecOffset = pSkeletonData->GetBoneData(GetBoneIndex(wheelId))->GetDefaultTranslation();

			const crBoneData* pParentBoneData = pSkeletonData->GetBoneData(GetBoneIndex(wheelId))->GetParent();
			while(pParentBoneData)
			{
				Mat34V matParent;
				Mat34VFromQuatV(matParent, pParentBoneData->GetDefaultRotation());
				matParent.SetCol3(pParentBoneData->GetDefaultTranslation());
				vecOffset = Transform(matParent, vecOffset);

				pParentBoneData = pParentBoneData->GetParent();
			}
		}
		else
			Assertf(false, "%s:CWheel::GetWheelOffset, model has no skeleton data", GetModelName());
	}
#if __ASSERT
	else
	{
		Assertf(GetBoneIndex(wheelId), "%s:CWheel::GetWheelOffset, wheel bone doesn't exist on model", GetModelName());
	}
#endif // __ASSERT
	return vecOffset;
}

void CVehicleModelInfo::CacheWheelOffsets()
{
	modelinfoAssert(m_data);
	int wheelCount = VEH_WHEEL_LAST_WHEEL - VEH_WHEEL_FIRST_WHEEL + 1;

	Assert(m_data->m_pStructure);
	Assertf(wheelCount > 0, "Vehicle has no wheels");
	
	m_data->m_WheelOffsets.Resize(wheelCount);

	for (int x=0; x<wheelCount; x++)
	{
		eHierarchyId boneId = (eHierarchyId) (x + VEH_WHEEL_FIRST_WHEEL);
		if (GetBoneIndex(boneId) > -1)
		{
			m_data->m_WheelOffsets[x] = CalculateWheelOffset(boneId);
		}
	}
}

#endif // !__SPU

void CVehicleModelInfo::Update()
{
#if !__GAMETOOL
	ms_requestPerFrame = 0;						// reset the per frame counter
	CVehicle::ResetDialsOverriddenByScript();	//sync reset point
#if __BANK
	if (CVehicleFactory::ms_bForceDials)
	{
		CVehicle* veh = NULL;
		CEntity* selected = (CEntity*)g_PickerManager.GetSelectedEntity();
		if (selected && selected->GetIsTypeVehicle())
			veh = (CVehicle*)selected;
		if (!veh)
			veh = CVehicleFactory::ms_pLastCreatedVehicle;
		if(veh)
			veh->RequestDials(true);
	}
#endif
    if (ms_activeDialsMovie != -1 && ms_dialsFrameCountReq != 0 && fwTimer::GetFrameCount() - ms_dialsFrameCountReq <= 1)
	{
		if (ms_dialsMovieId == -1)
		{
			ms_dialsMovieId = CScaleformMgr::CreateMovie(s_dialsMovieNames[ms_activeDialsMovie], Vector2(0.f, 0.f), Vector2(1.f, 1.f), false, -1, -1, true ,SF_MOVIE_TAGGED_BY_CODE, false, true); 
			Assert(ms_dialsMovieId != -1);
		}

        if (ms_dialsRenderTargetId != 0)
		{
			gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)ms_dialsRenderTargetId);
		}
	}
	else
	{
        ReleaseDials();
	}	
#endif // !__GAMETOOL
}

void CVehicleModelInfo::ReleaseDials()
{
	if (CScaleformMgr::IsMovieActive(ms_dialsMovieId))
	{
		CScaleformMgr::RequestRemoveMovie(ms_dialsMovieId);
		ms_dialsMovieId = -1;
		ms_dialsType = -1;
		ms_activeDialsMovie = -1;
		memset(&ms_cachedDashboardData, 0, sizeof(ms_cachedDashboardData));
	}

	if (ms_dialsRenderTargetId != 0)
	{
		if (gRenderTargetMgr.GetNamedRendertarget(ms_rtNameHash, ms_dialsRenderTargetOwner) != NULL)
		{
			gRenderTargetMgr.ReleaseNamedRendertarget(ms_rtNameHash, ms_dialsRenderTargetOwner);
		}
		ms_dialsRenderTargetId = 0;
	}

    ResetRadioInfo();
}


float CVehicleModelInfo::GetSteeringWheelMult(const CPed* pDriver)
{ 
	return (pDriver && pDriver->IsInFirstPersonVehicleCamera() && m_fFirstPersonSteeringWheelMult >= 0.0f ) ? m_fFirstPersonSteeringWheelMult : m_fSteeringWheelMult;
}

void CVehicleModelInfo::ValidateHorns(CVehicleKit& rKit)
{
	const atArray<CVehicleModStat>& kitStatMods = rKit.GetStatMods();
	const int statModsCount = kitStatMods.GetCount();
	int numHornMods = 0;
	for(int m = 0; m < statModsCount; m++)
	{
		if (kitStatMods[m].GetType() == VMT_HORN)
		{
			numHornMods++;
		}
	}

	if (numHornMods != 0)
	{
		if (numHornMods < NUM_VEHICLE_HORNS)
		{
			vehicleAssertf(0, "Vehicle modkit %s has %i horn mods, expecting %i. Please fix carcols.meta.", rKit.GetNameHashString().TryGetCStr(), numHornMods, NUM_VEHICLE_HORNS);
		}
		else if (numHornMods > NUM_VEHICLE_HORNS)
		{
			vehicleAssertf(0, "Vehicle modkit %s has %i horn mods, expecting %i. Update NUM_VEHICLE_HORNS in code if new horns exist, or fix carcols.meta.", rKit.GetNameHashString().TryGetCStr(), numHornMods, NUM_VEHICLE_HORNS);					
		}
	}
}
