//
// streaming/streaming.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

#include "streaming.h"

// Rage headers
#include "file/asset.h"
#include "file/diskcache.h"
#include "file/packfile.h"
#include "profile/timebars.h"
#include "system/timer.h"
#include "system/memory.h"
#include "fragment/type.h"
#include "fragment/tune.h"
#include "rmcore/drawable.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwrenderer/renderthread.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "fwscene/scan/VisibilityFlags.h"
#include "fwscene/stores/boxstreamersearch.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwsys/metadatastore.h"
#include "fwsys/timer.h"
#include "streaming/defragmentation.h"
#include "streaming/packfilemanager.h"
#include "streaming/streamingvisualize.h"
#include "streaming/zonedassetmanager.h"

// Game headers
#include "camera/caminterface.h"
#include "camera/viewports/viewportmanager.h"
#include "control/gamelogic.h"
#include "core/game.h"
#include "frontend/loadingscreens.h"
#include "frontend/PauseMenu.h"
#include "game/config.h"
#include "objects/dummyobject.h"
#include "pathserver/ExportCollision.h"
#include "pathserver/navgenparam.h"
#include "peds/ped.h"
#include "performance/clearinghouse.h"
#include "physics/physics.h"
#include "renderer/gtadrawable.h"
#include "scene/ContinuityMgr.h"
#include "scene/datafilemgr.h"
#include "scene/datafilemgr.h"
#include "scene/Entity.h"
#include "scene/focusentity.h"
#include "scene/InstancePriority.h"
#include "scene/ipl/iplcullbox.h"
#include "scene/loader/ManagedImapGroup.h"
#include "scene/loader/maptypes.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/scene.h"
#include "scene/streamer/scenestreamermgr.h"
#include "scene/world/gameworld.h"
#include "scene/ExtraContent.h"
#include "script/script.h"
#include "streaming/cacheloader.h"
#include "streaming/requestrecorder.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingloader.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingrequestlist.h"
#include "system/controlmgr.h"
#include "system/keyboard.h"
#include "text/text.h" 
#include "vehicleai/pathfind.h"
#include "streaming/populationstreaming.h"
#include "Network/Events/NetworkEventTypes.h"

#include "streaming/streamingdebuggraphextensions.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplayIPLManager.h"
#include "control/replay/Misc/InteriorPacket.h"


#if __PPU
#include <sys/memory.h>
#endif // __PPU

CStreamingCleanup				CStreaming::ms_cleanup;
bool							CStreaming::ms_FirstInitComplete;
char							CStreaming::ms_streamingFilePath[RAGE_MAX_PATH]={0};

#if !__FINAL
CStreamingDebug					CStreaming::ms_debug;
bool							CStreaming::ms_pauseStreaming = false;
#endif
bool							CStreaming::ms_enableSceneStreaming = true;

bool							CStreaming::ms_scriptSuppressingHdImaps = false;
bool							CStreaming::ms_isPlayerPositioned = false;
bool							CStreaming::ms_bRequestRenderThreadFlush = false;

#if RSG_PC
bool							CStreaming::ms_bForceEnableHdImaps = false;
#endif

Vector3							CStreaming::ms_oldFocusPosn;

#if __BANK
XPARAM(askBeforeRestart);
#endif // __BANK
XPARAM(PartialDynArch);
PARAM(previewfolder, "Enable streaming from the preview folder");
PARAM(previewcoords, "Where to put the preview tag (=x,y)");
PARAM(recordstartuprequests, "Record all requests during startup and save them to a file");
PARAM(reloadpackfilesonrestart, "[streaming] Reload all packfiles when restarting the game");

#define STARTUP_REQ_FILE "%s/startupreq"
//#define TEXTURES_FOLDER "platform:/textures/"

//////////////////////////////////////////////////////////////////////////

class CGtaStreamingInterface: public strStreamingInterface 
{
public:
	CGtaStreamingInterface(size_t virtualSize, size_t physicalSize, int archiveCount) :
		strStreamingInterface(virtualSize, physicalSize, archiveCount) {}

	// CStreaming
	virtual void LoadAllRequestedObjects(bool bPriorityRequestsOnly = false) { CStreaming::LoadAllRequestedObjects(bPriorityRequestsOnly); }

	// CStreamingCleanup entry points:
	virtual void RequestFlush(bool flag) { CStreaming::GetCleanup().RequestFlush(flag); }
	virtual bool MakeSpaceForResourceFile(const char* filename, const char* ext, s32 version) {
		return CStreaming::GetCleanup().MakeSpaceForResourceFile(filename, ext, version);
	}

	virtual bool MakeSpaceForMap(datResourceMap& map, strIndex index, bool keepMemory) {
		return CStreaming::GetCleanup().MakeSpaceForMap(map, index, keepMemory);
	}

	virtual bool WasLastAllocationAborted() const {
		return CStreaming::GetCleanup().WasLastAllocationAborted();
	}

	virtual bool WasLastAllocationFragmented() const {
		return CStreaming::GetCleanup().WasLastAllocationFragmented();
	}

#if __BANK
	virtual void PrintMemoryDistribution(const sysMemDistribution& distribution,  char* header, char* usedMemory, char* freeMemory) {
		CStreamingDebug::PrintMemoryDistribution(distribution, header, usedMemory, freeMemory);
	}
#endif // __BANK

#if __BANK
	virtual void LogStreaming(strIndex index) { CStreamingDebug::LogStreaming(index); }
#else
	virtual void LogStreaming(strIndex) {  }
#endif
	virtual void DisplayModuleMemory(bool) { CStreamingDebug::DisplayModuleMemory(true, false, false); }

	// CStreamingRequestList:
#if !__FINAL
	virtual void RecordRequest(strIndex index) { gStreamingRequestList.RecordRequest(index); }
#else
	virtual void RecordRequest(strIndex) {  }
#endif

	// strStreamingEngine PS3 install stuff:
	virtual void InstallFailure() { Quitf(ERR_DEFAULT,"Install failure.  TODO: Flesh this out!"); } // HACK HACK HACK

	virtual void PostLoadRpf(int index) { CStreaming::PostLoadRpf(index); }

#if !__NO_OUTPUT
	virtual void DumpStreamingInterests() {	g_SceneStreamerMgr.DumpStreamingInterests(); }
#if !__FINAL
	virtual void FullMemoryReport() { CStreamingDebug::FullMemoryDump(); }
#endif // !__FINAL
#endif // !__NO_OUTPUT


#if __BANK
	virtual void AddFailedStreamingAllocation(strIndex index, bool bFragmented, datResourceMap& map) { CStreaming::GetDebug().AddFailedStreamingAllocation(index, bFragmented, map);}
#endif
	virtual void AddResidentObject(strIndex index ) { gPopStreaming.AddResidentObject(index); }
	virtual void RemoveResidentObject(strIndex index) { gPopStreaming.RemoveResidentObject(index);}

};

#if STREAMING_VISUALIZE
//////////////////////////////////////////////////////////////////////////
const char *CStreaming::GetStreamingContext()
{
	return CTheScripts::GetCurrentScriptName();
}

//////////////////////////////////////////////////////////////////////////
int CStreaming::EntityGuidGetter(const fwEntity *entity)
{
	u32 guid;
	if (((CEntity *) entity)->GetGuid(guid))
	{
		return (int) guid;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
float CStreaming::EntityBoundSphereGetter(const fwEntity *entity, Vector3 &outVector)
{
	return static_cast<const CEntity *>(entity)->GetBoundCentreAndRadius(outVector);
}

//////////////////////////////////////////////////////////////////////////
void CStreaming::AddStackContext()
{
	const char *scriptName = CTheScripts::GetCurrentScriptName();

	if (scriptName && scriptName[0])
	{
		char markerContext[128];
		formatf(markerContext, "From script %s", scriptName);
		strStreamingVisualize::GetInstance().SetMarkerText(markerContext);
	}

	strStreamingVisualize::GetInstance().AddCallstackToMarker();
}
#endif // STREAMING_VISUALIZE



//////////////////////////////////////////////////////////////////////////

void CStreaming::InitLevelWithMapUnloaded()
{
	USE_MEMBUCKET(MEMBUCKET_STREAMING);

	if (PARAM_PartialDynArch.Get()){
		CMapFileMgr::GetInstance().Reset();
		strStreamingEngine::InitLevelWithMapUnloaded();
		CMapFileMgr::GetInstance().SetMapFileDependencies();
 	} else {
 		strStreamingEngine::InitLevelWithMapUnloaded();
 	}

	// disable title update patching for test levels etc
// #if !__FINAL
// 	XPARAM(level);
// 	bool bEnableTitleUpdate = true;
// 	const char* levelName = NULL;
// 	if (PARAM_level.Get(levelName))
// 	{
// 		bEnableTitleUpdate = (atHashString(levelName) == atHashString("gta5", 0x6D2855E0));
// 	}
// 	if (bEnableTitleUpdate)
// #endif
	{
		EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_STREAMING, true);
		EXTRACONTENT.ExecuteTitleUpdateDataPatch(CCS_TITLE_UPDATE_STREAMING, true);
	}

	BANK_ONLY(strStreamingEngine::GetInfo().PrintPackfileRelationships();)
	BANK_ONLY(strStreamingEngine::GetInfo().ValidateRelationships();)
}

void CStreaming::InitStreamingVisualize()
{
#if STREAMING_VISUALIZE
	strStreamingVisualize::InitClass();

	if (strStreamingVisualize::IsInstantiated())
	{
		strStreamingVisualize::GetInstance().SetStreamingContextCb(GetStreamingContext);
		strStreamingVisualize::GetInstance().SetEntityGuidGetterCb(EntityGuidGetter);
		strStreamingVisualize::GetInstance().SetEntityBoundSphereGetterCb(EntityBoundSphereGetter);
		strStreamingVisualize::GetInstance().SetAddStackContextCb(AddStackContext);
		STREAMINGVIS.StartNetworkWorker();

		STRVIS_REGISTER_DEVICE(pgStreamer::OPTICAL, "Optical");
		STRVIS_REGISTER_DEVICE(pgStreamer::HARDDRIVE, "Hard Drive");
	}
#endif // STREAMING_VISUALIZE
}

void CStreaming::InitStreamingEngine()
{
	USE_MEMBUCKET(MEMBUCKET_STREAMING);

	const CConfigStreamingEngine& cfg = CGameConfig::Get().GetConfigStreamingEngine();

	// Old Consoles in KB / PC next gen in MB
	size_t virtualSize = (size_t)cfg.m_VirtualStreamingBuffer << (size_t)((__XENON || __PS3) ? 10 : 20);
	size_t physicalSize = (size_t)cfg.m_PhysicalStreamingBuffer << (size_t)((__XENON || __PS3) ? 10 : 20);
	
#if __BANK
	gStreamingLoaderPreCallback = &CStreamGraphAllocationsToResGameVirt::PreChange;
	gStreamingLoaderPostCallback = &CStreamGraphAllocationsToResGameVirt::PostChange;
#endif
	strStreamingEngine::InitClass(rage_new CGtaStreamingInterface(
			virtualSize, 
			physicalSize,
			cfg.m_ArchiveCount));

	strStreamingEngine::Init();


	CStreamingDebug::InitDebugSystems();

	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_NOTHING, "Nothing", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_BUILDING, "Building", 0xffffff, true);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_ANIMATED_BUILDING, "Animated Building", 0xff8888, true);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_VEHICLE, "Vehicle", 0x00ffff, true);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_PED, "Ped", 0xff88ff, true);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_OBJECT, "Object", 0xff00ff, true);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_DUMMY_OBJECT, "Dummy", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_PORTAL, "Portal", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_MLO, "MLO", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_NOTINPOOLS, "For Audio", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_PARTICLESYSTEM, "Particle System", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_LIGHT, "Light", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_COMPOSITE, "Composite", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_INSTANCE_LIST, "Instance List", 0x0000ff, false);
	STRVIS_REGISTER_ENTITY_TYPE(ENTITY_TYPE_GRASS_INSTANCE_LIST, "Grass Instance List", 0x0000ff, false);

#if STREAMING_VISUALIZE
	fwBoxStreamerSearch::RegisterSearchTypesWithSV();

	for (int x=0; x<LODTYPES_DEPTH_TOTAL; x++)
	{
#if !__FINAL
		STRVIS_REGISTER_LOD_TYPE(x, fwLodData::ms_apszLevelNames[x]);
#else // !__FINAL
		char dummyLod[64];
		formatf(dummyLod, "LOD Level %d", x);
		STRVIS_REGISTER_LOD_TYPE(x, dummyLod);
#endif // !__FINAL
	}
#endif // STREAMING_VISUALIZE
}

#if !__FINAL
void CStreaming::BeginRecordingStartupSequence()
{
	if (PARAM_recordstartuprequests.Get())
	{
		REQUESTRECORDER.BeginRecording(true, false);
	}
}

void CStreaming::EndRecordingStartupSequence()
{
	if (PARAM_recordstartuprequests.Get())
	{
		char fileBuffer[RAGE_MAX_PATH];
		REQUESTRECORDER.EndRecording();
		REQUESTRECORDER.SaveRecording(GetStarupSequenceRecordingName(fileBuffer, sizeof(fileBuffer)));
	}
}
#endif // !__FINAL

const char *CStreaming::GetStarupSequenceRecordingName(char *outBuffer, size_t bufferSize)
{
	char levelName[RAGE_MAX_PATH];
	ASSET.RemoveNameFromPath(levelName, sizeof(levelName), CScene::GetLevelsData().GetFilename(CGameLogic::GetRequestedLevelIndex()));
	formatf(outBuffer, bufferSize, STARTUP_REQ_FILE, levelName);

	return outBuffer;
}

void CStreaming::ReplayStartupSequenceRecording()
{
#if !__FINAL
	if (!PARAM_recordstartuprequests.Get())
#endif // !__FINAL
	{
		char fileBuffer[RAGE_MAX_PATH];
		REQUESTRECORDER.LoadRecording(GetStarupSequenceRecordingName(fileBuffer, sizeof(fileBuffer)));
		REQUESTRECORDER.ExecuteRecording();
	}
}

void CStreaming::SetStreamingFile(const char* streamingFile)
{
	memset(ms_streamingFilePath,0,RAGE_MAX_PATH);
	strcpy(ms_streamingFilePath,streamingFile);
}

void CStreaming::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
        USE_MEMBUCKET(MEMBUCKET_STREAMING);

		strStreamingEngine::GetInfo().AllocateStreamingInfoArray();
		SetStreamingFile("platformcrc:/data/streaming");

#if LIVE_STREAMING
		static bool init = true;
		if (init && PARAM_previewfolder.Get())
		{
			strStreamingEngine::GetLive().SetEnabled(PARAM_previewfolder.Get());
			init = false;
		}
#endif // LIVE_STREAMING
		
		fwPlaceFunctor placeDrawable;
		placeDrawable.Reset<&CStreaming::PlaceDrawable>();
		g_DrawableStore.SetPlaceFunctor(placeDrawable);

		fwDefragmentFunctor defragmentDrawable;
		defragmentDrawable.Reset<&CStreaming::DefragmentDrawable>();
		g_DrawableStore.SetDefragmentFunctor(defragmentDrawable);

		fwPlaceFunctor placeFragType;
		placeFragType.Reset<&CStreaming::PlaceFragType>();
		g_FragmentStore.SetPlaceFunctor(placeFragType);

		fwDefragmentFunctor defragmentFragType;
		defragmentFragType.Reset<&CStreaming::DefragmentFragType>();
		g_FragmentStore.SetDefragmentFunctor(defragmentFragType);

		fwPlaceFunctor placeDwd;
		placeDwd.Reset<&CStreaming::PlaceDwd>();
		g_DwdStore.SetPlaceFunctor(placeDwd);

		fwDefragmentFunctor defragmentDwd;
		defragmentDwd.Reset<&CStreaming::DefragmentDwd>();
		g_DwdStore.SetDefragmentFunctor(defragmentDwd);

		g_SceneStreamerMgr.Init();
	    GetCleanup().Init();
    }
    else if(initMode == INIT_SESSION)
    {
	    ms_enableSceneStreaming = true;

		CMapFileMgr::GetInstance().Reset();
		strStreamingEngine::InitLevelWithMapLoaded();

		if (ShouldLoadStaticData())
		{
			strStreamingEngine::InitPackfiles();
		}

		CMapFileMgr::GetInstance().SetMapFileDependencies();
		CMapFileMgr::GetInstance().Cleanup();

		CZonedAssetManager::Init();
		CIslandHopper::GetInstance().Init();
    }
}

void CStreaming::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        ms_cleanup.Shutdown();
		g_SceneStreamerMgr.Shutdown();

		strStreamingEngine::ShutdownPackfiles();
	    strStreamingEngine::Shutdown();
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        strStreamingEngine::ShutdownLevelWithMapUnloaded();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
//		strStreamingEngine::ShutdownLevelWithMapLoaded();	used to be called here. It'll probably still need to be called
														//	when shutting down the map
		strStreamingEngine::ShutdownSession();

		if (GetReloadPackfilesOnRestart())
		{
			strStreamingEngine::ShutdownPackfiles();
		}
    }
}

#if !__FINAL
bool CStreaming::GetReloadPackfilesOnRestart()
{
#if __BANK
	if (PARAM_askBeforeRestart.Get())
	{
		return true;
	}
#endif // __BANK

	return PARAM_reloadpackfilesonrestart.Get();
}
#endif // !__FINAL

bool CStreaming::ShouldLoadStaticData()
{
	return !ms_FirstInitComplete || GetReloadPackfilesOnRestart();
}


#if __PS3 
#define	PLATFORM_STREAMING_FILES CDataFileMgr::STREAMING_FILE_PLATFORM_PS3
#elif __XENON
#define	PLATFORM_STREAMING_FILES CDataFileMgr::STREAMING_FILE_PLATFORM_XENON
#else
#define	PLATFORM_STREAMING_FILES CDataFileMgr::STREAMING_FILE_PLATFORM_OTHER
#endif

void CStreaming::LoadStreamingMetaFile()
{
	// streaming.meta specifies a list of streaming files to be registered with the packfile manager
	DATAFILEMGR.Load(ms_streamingFilePath, true);
}

void CStreaming::RegisterStreamingFiles()
{
	const CDataFileMgr::DataFileType streamTypes[] = { CDataFileMgr::STREAMING_FILE, PLATFORM_STREAMING_FILES };
	u32 uType = 0;
	char relativePathBuffer[256];

	// First iteration registers platform agnostic files, the second deals with platform specific ones. 
	do 
	{
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(streamTypes[uType]);

		while(DATAFILEMGR.IsValid(pData))
		{
			char FilenameWithoutExtension[RAGE_MAX_PATH];
			char PlatformFilename[RAGE_MAX_PATH];
			const char *ext = ASSET.FindExtensionInPath( pData->m_filename );
			streamAssert(ext!=NULL);
			ASSET.RemoveExtensionFromPath( FilenameWithoutExtension, sizeof( FilenameWithoutExtension ), pData->m_filename );

			// All of the file paths include a wildcard # token in the extension which 
			// pgRscBuilder::ConstructName can patch to create a platform specific path for us to register.

			pgRscBuilder::ConstructName(PlatformFilename, sizeof( PlatformFilename ), FilenameWithoutExtension, &ext[1]);

			const char *relativePath = NULL;
			const char *srcRelativePath = pData->m_registerAs.c_str();

			// The parser likes to create empty strings, we don't want that.
			if (srcRelativePath && *srcRelativePath)
			{
				safecpy(relativePathBuffer, srcRelativePath);

				// Append the extension, which might be platform-specific.
				const char *extPtr = strrchr(PlatformFilename, '.');

				if (extPtr)
				{
					safecat(relativePathBuffer, extPtr);
				}

				relativePath = relativePathBuffer;
			}

			strPackfileManager::RegisterIndividualFile(PlatformFilename, true, relativePath);
			pData = DATAFILEMGR.GetNextFile(pData);
		}
	} while (++uType < COUNTOF(streamTypes));
}

void CStreaming::PostLoadRpf(int index)
{
	strStreamingFile* file = strPackfileManager::GetImageFile(index);
	fiPackfile* pack = static_cast<fiPackfile*>(file->m_packfile);
	// streamAssert(pack->GetEntries()); // this is okay now when we have the cache file active
	streamVerify( pack->MountAs( "localPack:/" ) );
	CMapFileMgr::GetInstance().LoadPackFileMetaData(pack, file->m_name.GetCStr());
	streamVerify( fiDevice::Unmount("localPack:/"));
}


void CStreaming::SetKeepAliveCallback(void (*callback)(), u32 intervalInMS)
{
    strStreamingEngine::GetLoader().SetKeepAliveCallback(callback, intervalInMS);
}

void CStreaming::Update()
{
	USE_MEMBUCKET(MEMBUCKET_STREAMING);

	CIslandHopper::GetInstance().Update();

#if GTA_REPLAY
	ReplayIPLManager::DoOnExit();
	ReplayIPLManager::DoProcess();
	CPacketInteriorEntitySet::ProcessDeferredEntitySetUpdates();
#endif	//GTA_REPLAY

	// notify load monitor of num streaming requsets
	strStreamingEngine::SetBurstMode(camInterface::IsFadedOut() || CPauseMenu::GetPauseRenderPhasesStatus() || !gRenderThreadInterface.IsUsingDefaultRenderFunction());

	PF_START_TIMEBAR_DETAIL("Cleanup");
	ms_cleanup.Update();

#if USE_DEFRAGMENTATION
	strStreamingEngine::GetDefragmentation()->Frack(CTheScripts::IsFracked());
#endif 

#if RSG_PC
	static bool bHackProcessed = false;
	if(NetworkInterface::IsGameInProgress() && !bHackProcessed && CTheScripts::IsFracked())
	{
		CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::CODE_TAMPERING);
		bHackProcessed = true;
	}
#endif // RSG_PC

#if LIVE_STREAMING
	strStreamingEngine::GetLive().UpdateUnlinkedFiles();
#endif // LIVE_STREAMING

#if !__FINAL
	PF_START_TIMEBAR_DETAIL("Debug");
	GetDebug().Update();

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SCROLL, KEYBOARD_MODE_DEBUG, "{Pause Streaming")) 
		ms_pauseStreaming = !ms_pauseStreaming;

	if(ms_pauseStreaming)
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput("Streaming locked"));

#if __BANK
		COMMERCE_CONTAINER_ONLY(if (!CLiveManager::GetCommerceMgr().ContainerIsStoreMode()))
		{
			strStreamingEngine::DebugDraw();
		}
#endif // __BANK

		return;
	}
#endif // /!__FINAL

	if(strStreamingEngine::IsStreamingDisabled())
	{
		return;
	}

	CTheScripts::UnFrack();
	gStreamingRequestList.ComputeSearchesAndFocus();

	strStreamingEngine::PreLoadedUpdate();

	PF_PUSH_TIMEBAR_BUDGETED("Synchronous resource placement", 1.0f);
	strStreamingEngine::GetLoader().ProcessAsyncFiles();
	PF_POP_TIMEBAR();

	PF_START_TIMEBAR_DETAIL("Text");
	TheText.ProcessStreamingAdditionalText();

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		ms_isPlayerPositioned = true;
#endif

	if (ms_isPlayerPositioned COMMERCE_CONTAINER_ONLY(&& !CLiveManager::GetCommerceMgr().ContainerIsStoreMode()))
	{
		Vector3 playerPosn = CFocusEntityMgr::GetMgr().GetPos();

		// a >300m shift in focus can generate a big .ityp overlap : if we are faded out, then we can do something to avoid that...
		if ( ms_oldFocusPosn.Dist(playerPosn) > 300.0f)		
		{
			ms_bRequestRenderThreadFlush = true;		
		}

		PF_PUSH_TIMEBAR_BUDGETED("IMAP streaming", 1.0f); 
		{
			CInstancePriority::Update();

			//////////////////////////////////////////////////////////////////////////
			// is the player in an aircraft and moving quickly? if so disable hd imap streaming
#if RSG_PC
			const bool bFastMovement = ( (g_ContinuityMgr.HighSpeedFlying() || g_ContinuityMgr.HighSpeedFalling()) && !ms_bForceEnableHdImaps) || ms_scriptSuppressingHdImaps;
#else
			const bool bFastMovement = ( g_ContinuityMgr.HighSpeedFlying() || g_ContinuityMgr.HighSpeedFalling() ) || ms_scriptSuppressingHdImaps;
#endif
			const bool bAllowHdStreaming = !( bFastMovement && CFocusEntityMgr::GetMgr().IsPlayerPed() ) BANK_ONLY(|| !fwMapDataDebug::SuppressHdWhenFlyingFast());
			INSTANCE_STORE.SetHdStreaming( bAllowHdStreaming );
			ms_scriptSuppressingHdImaps = false;
			//////////////////////////////////////////////////////////////////////////

			PF_START_TIMEBAR_DETAIL("IMAP cull boxes");
			// load IPLs (model instance info)
			CIplCullBox::Update(playerPosn);

			PF_START_TIMEBAR_DETAIL("Zoned assets");
			CZonedAssetManager::Update(VECTOR3_TO_VEC3V(playerPosn));

			PF_START_TIMEBAR_DETAIL("Managed IMAP groups");
			CManagedImapGroupMgr::Update(RCC_VEC3V(playerPosn));

			PF_START_TIMEBAR_DETAIL("RequestAboutPos");
			INSTANCE_STORE.GetBoxStreamer().RequestAboutPos(RCC_VEC3V(playerPosn), (camInterface::IsFadedOut()) ? STRFLAG_PRIORITY_LOAD : 0);

			PF_START_TIMEBAR_DETAIL("Update");
			INSTANCE_STORE.Update();

			const bool bGameIsNotRendering = (camInterface::IsFadedOut() || CLoadingScreens::AreActive() || CPauseMenu::GetPauseRenderPhasesStatus() BANK_ONLY(|| gStreamingRequestList.IsRecording()));
			if(ms_bRequestRenderThreadFlush && bGameIsNotRendering)
			{	
				gRenderThreadInterface.Flush();
			}
			ms_bRequestRenderThreadFlush = false;

			g_MapTypesStore.Update();


	// 		PF_START_TIMEBAR_DETAIL("EnsureLoadedAboutPos");
	// 		if (CTheScripts::GetAllowGameToPauseForStreaming())
	// 		{				
	// 			INSTANCE_STORE.GetBoxStreamer().EnsureLoadedAboutPos(RCC_VEC3V(playerPosn));
	// 		}

		}
		PF_POP_TIMEBAR();

#if GTA_REPLAY
		PF_PUSH_TIMEBAR("Replay export wait for mapdata");
		{
			if (CReplayMgr::IsExporting())
			{
				CStreaming::LoadAllRequestedObjects();
			}
		}
		PF_POP_TIMEBAR();
#endif	//GTA_REPLAY

		PF_PUSH_TIMEBAR_BUDGETED("MetaData streaming", 1.0f); 
		{
			PF_START_TIMEBAR_DETAIL("RequestAboutPos");
			
			// Use the population position for this, so we stream in scenario regions around the point where we want to spawn.
			const Vec3V popPosV = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPopPos());

			Vec3V offset(ScalarV(0.1f));//0.1f is the default sphere size used if you just pass in a position to RequestAboutPos
			Vec3V min = popPosV - offset;
			Vec3V max = popPosV + offset;
			
			//Want to stretch the bounds in the Z direction if the entity is in a heli or is in the air(falling, parachuting)
			// - probably shouldn't do this if we have an override position.
			if(!CFocusEntityMgr::GetMgr().HasOverridenPop())
			{
				const CEntity* ntt = CFocusEntityMgr::GetMgr().GetEntity();
				if (ntt && ntt->GetIsTypePed())
				{
					const CPed* ped = (const CPed*)ntt;
					CVehicle* veh = ped->GetVehiclePedInside();
					if (	ped->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) ||
							(veh && (veh->InheritsFromHeli() || veh->GetHoverMode()))
						)
					{
						ScalarV extraZ(100.0f);
						min.SetZ(min.GetZ() - extraZ);
						max.SetZ(max.GetZ() + extraZ);
					}
				}
			}

			spdAABB mySearchBox(min, max);
			fwBoxStreamerSearch primarySearch(mySearchBox, fwBoxStreamerSearch::TYPE_PRIMARY, fwBoxStreamerAsset::FLAG_METADATA);
			g_fwMetaDataStore.GetBoxStreamer().RequestAboutPos(primarySearch);

			PF_START_TIMEBAR_DETAIL("Update");
			g_fwMetaDataStore.Update();
		}
		PF_POP_TIMEBAR();

		PF_START_TIMEBAR_DETAIL("physics streaming");
		CPhysics::UpdateStreaming();

		PF_START_TIMEBAR_DETAIL("path streaming");
		ThePaths.UpdateStreaming();
		PF_START_TIMEBAR_DETAIL("scene streaming");

		ms_oldFocusPosn = playerPosn;
	}

	gStreamingRequestList.Update();

	// If we have no more open requests, we can re-enable HD vehicles.
	// This is fucking voodoo, and sucks. I'm open for suggestions for a better approach.
	if (strStreamingEngine::GetInfo().GetNumberObjectsRequested() == 0)
	{
		CVehicle::SetDisableHDVehicles(false);
	}


	strStreamingEngine::Update();

#if STREAMING_VISUALIZE
	STREAM_TICKER_ONLY(StrTicker::StepPut());	
	STREAM_TICKER_ONLY(StrTicker::StepGet());	
#endif

#if __BANK
	COMMERCE_CONTAINER_ONLY(if (!CLiveManager::GetCommerceMgr().ContainerIsStoreMode()))
	{
		strStreamingEngine::DebugDraw();
	}
#endif // __BANK

	perfClearingHouse::SetValue(perfClearingHouse::NORMAL_PRIORITY_STREAMING_REQUESTS, strStreamingEngine::GetInfo().GetNumberObjectsRequested() - strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested());
	perfClearingHouse::SetValue(perfClearingHouse::HIGH_PRIORITY_STREAMING_REQUESTS, strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested());
}

// bank_bool g_RequestNearbyObjects = true;
// 
// void CStreaming::RequestNearbyObjects()
// {
//     if (!g_RequestNearbyObjects)
//         return;
// 
//     const Vector3 playerPosn = CGameWorld::FindLocalPlayerCoors();
// #if NAVMESH_EXPORT
//     if(CNavMeshDataExporter::IsExportingCollision())
//         ms_scene.AddObjectsToRequestListForNavMeshExport(playerPosn);
//     else
// #endif
//     if(strStreamingEngine::GetIsLoadingPriorityObjects() == false && gStreamingRequestList.IsActive() == false && ms_enableSceneStreaming)
//     {
//         const Vector3& camPosn = camInterface::GetPos();
//         bool bCanSeeExterior = true;
//         CInteriorInst*		pInterior = CPortalVisTracker::m_pPrimaryIntInst;
//         s32				roomIdx = CPortalVisTracker::m_primaryRoomIdx;
// 
//         // If there are no exit portals in current room then set bCanSeeExterior to false
//         if(pInterior && roomIdx > 0){
//             if ((pInterior->GetNumExitPortals() == 0) || (pInterior->GetNumExitPortalsInRoom(roomIdx) == 0)){
//                 bCanSeeExterior = false;
//             }
//         }
// 			if(camPosn.z < 50.0f)
//         ms_scene.AddObjectsToRequestList(camPosn, bCanSeeExterior);
//         if(bCanSeeExterior)
//             ms_scene.AddLodsToRequestList(camPosn);
//     }
// }


void CStreaming::PurgeRequestList(u32 ignoreFlags)
{
	strStreamingEngine::GetInfo().PurgeRequestList(ignoreFlags);
}


// #if USE_DUALLAYER
// static bool gbForceLayerRead = false;
// static s32 gLayerToRead = 0;
// void CStreaming::ForceLayerToRead(s32 layer)
// {
// 	gbForceLayerRead = true;
// 	gLayerToRead = layer;
// }
// #endif


void CStreaming::SetObjectIsDeletable(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetObjectIsDeletable(index);
}



void CStreaming::SetObjectIsNotDeletable(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetObjectIsNotDeletable(index);
}


bool CStreaming::GetObjectIsDeletable(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().IsObjectDeletable(index);
}


void CStreaming::SetMissionDoesntRequireObject(strLocalIndex objIndex, s32 module)
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetMissionDoesntRequireObject(index);
}


void CStreaming::SetInteriorDoesntRequireObject(strLocalIndex objIndex, s32 module)
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetInteriorDoesntRequireObject(index);
}

void CStreaming::SetCutsceneDoesntRequireObject(strLocalIndex objIndex, s32 module)
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetCutsceneDoesntRequireObject(index);
}

void CStreaming::SetDoNotDefrag(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().SetDoNotDefrag(index);
}


u64 CStreaming::GetImageTime(strLocalIndex index)
{
	return strPackfileManager::GetImageTime(index.Get());
}


const char* CStreaming::GetImageFilename(strLocalIndex index)
{
	return strPackfileManager::GetImageFile(index.Get())->m_name.GetCStr();
}


void CStreaming::SetImageIsNew(strLocalIndex index)
{
	strPackfileManager::GetImageFile(index.Get())->m_bNew = true;
}


bool CStreaming::GetImageIsNew(strLocalIndex index)
{
	return strPackfileManager::GetImageFile(index.Get())->m_bNew;
}


void CStreaming::LoadAllRequestedObjects(bool bPriorityRequestsOnly)
{
#if STREAMING_VISUALIZE
	const char *origin = CTheScripts::GetCurrentScriptNameAndProgramCounter();
	if (!origin || !origin[0])
	{
		origin = "Code";
	}

	STRVIS_SET_MARKER_TEXT("Block: Load all, from %s", origin);
#endif // STREAMING_VISUALIZE

	USE_MEMBUCKET(MEMBUCKET_STREAMING);
	strStreamingEngine::GetLoader().LoadAllRequestedObjects(bPriorityRequestsOnly); // @todo
}

#if GTA_REPLAY
void CStreaming::LoadAllRequestedObjectsWithTimeout(bool bPriorityRequestsOnly, u32 timeoutMS)
{
#if STREAMING_VISUALIZE
	const char *origin = CTheScripts::GetCurrentScriptNameAndProgramCounter();
	if (!origin || !origin[0])
	{
		origin = "Replay";
	}

	STRVIS_SET_MARKER_TEXT("Block: Load all, from %s", origin);
#endif // STREAMING_VISUALIZE

	USE_MEMBUCKET(MEMBUCKET_STREAMING);
	strStreamingEngine::GetLoader().LoadAllRequestedObjectsWithTimeout(bPriorityRequestsOnly, timeoutMS);
}
#endif


void CStreaming::Flush()
{
	strStreamingEngine::Flush();
}


bool CStreaming::RequestObject(strLocalIndex objIndex, s32 module, s32 flags) 
{
	streamAssert(objIndex.Get() >= 0); 
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().RequestObject(index, flags);
}

void CStreaming::ClearRequiredFlag(strLocalIndex objIndex, s32 module, s32 flags)
{
	streamAssert(objIndex.Get() >= 0); 
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().ClearRequiredFlag(index, flags);
}

void CStreaming::RemoveObject(strLocalIndex objIndex, s32 module) 
{
	streamAssert(objIndex.Get() >= 0); 
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	strStreamingEngine::GetInfo().RemoveObject(index);
}


u32 CStreaming::GetObjectFlags(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().GetObjectFlags(index);
}



u32 CStreaming::GetObjectVirtualSize(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().GetObjectVirtualSize(index);
}


u32 CStreaming::GetObjectPhysicalSize(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().GetObjectPhysicalSize(index);
}


void CStreaming::GetObjectAndDependenciesSizes(strLocalIndex objIndex, s32 module, u32& virtualSize, u32& physicalSize, const strIndex* ignoreList, s32 numIgnores, bool mayFail)
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	virtualSize = 0;
	physicalSize = 0;
	strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, virtualSize, physicalSize, ignoreList, numIgnores, mayFail);
}

void CStreaming::GetObjectAndDependencies(strLocalIndex objIndex, s32 module, atArray<strIndex>& allDeps, const strIndex* ignoreList, s32 numIgnores)
{
	strStreamingModule *pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module);
	strIndex index = pModule->GetStreamingIndex(objIndex);
	pModule->GetObjectAndDependencies(index, allDeps, ignoreList, numIgnores);
}

bool CStreaming::HasObjectLoaded(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return (strStreamingEngine::GetInfo().GetStreamingInfo(index)->GetStatus() == STRINFO_LOADED);
}	


bool CStreaming::IsObjectRequested(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return (strStreamingEngine::GetInfo().GetStreamingInfo(index)->GetStatus() == STRINFO_LOADREQUESTED);
}

bool CStreaming::IsObjectLoading(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return (strStreamingEngine::GetInfo().GetStreamingInfo(index)->GetStatus() == STRINFO_LOADING);
}


bool CStreaming::IsObjectInImage(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().IsObjectInImage(index);
}


bool CStreaming::IsObjectNew(strLocalIndex objIndex, s32 module) 
{
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);
	return strStreamingEngine::GetInfo().IsObjectNew(index);
}


s32 CStreaming::GetNumberObjectsRequested()
{
	return strStreamingEngine::GetInfo().GetNumberObjectsRequested();
}


strIndex CStreaming::RegisterDummyObject(const char* name, strLocalIndex objectIndex, s32 module)
{
	return strStreamingEngine::GetInfo().RegisterDummyObject(name, objectIndex, module);
}


bool CStreaming::LoadObject(strLocalIndex objIndex, s32 module, bool bCanBeDefragged, bool bCanBeDeleted)
{
	s32 flags = STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE;
	if(!strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->StreamingBlockingLoad(objIndex, flags))
	{
		return false;
	}

	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(objIndex);

	if(bCanBeDeleted)
	{
		strStreamingEngine::GetInfo().ClearRequiredFlag(index, STRFLAG_DONTDELETE);
	}

	if(!bCanBeDefragged && (!(strStreamingEngine::GetInfo().GetStreamingInfoRef(index).GetFlags() & STRFLAG_INTERNAL_DUMMY)))
	{
		CStreaming::SetDoNotDefrag( objIndex, module );
	}

	return true;
}


void CStreaming::ClearFlagForAll(u32 flag)
{
	strStreamingEngine::GetInfo().ClearFlagForAll(flag);
}


void CStreaming::DisableStreaming()
{
	strStreamingEngine::DisableStreaming();
}


void CStreaming::EnableStreaming()
{
	strStreamingEngine::EnableStreaming();
}


bool CStreaming::IsStreamingDisabled()
{
	return strStreamingEngine::IsStreamingDisabled();
}

#if ENABLE_DEFRAG_CALLBACK
bool CStreaming::IsObjectReadyToDelete(void* ptr)
{
	sysMemAllocator* pAllocator = strStreamingEngine::GetAllocator().GetPointerOwner(ptr);
	Assert(pAllocator);

	void* pBlock = pAllocator->GetCanonicalBlockPtr(ptr);
	streamAssert(pBlock);

	const strIndex index = strStreamingInfoManager::GetStreamingIndexFromMemBlock(pBlock);
	return index.IsValid() && strStreamingEngine::GetInfo().IsObjectReadyToDelete(index, STRFLAG_NONE);
}
#endif

#if __BANK
void CStreaming::InitWidgets()
{
	GetDebug().InitWidgets();
}
#endif

//////////////////////////////////////////////////////////////////////////
// fwTxd Defragmentation Support
#include "streaming/defragmentation.h"

#include "cutscene/cutscenemanagernew.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/timemodelinfo.h"
#include "renderer/drawlists/drawlist.h"
#include "scene/entity.h"
#include "shaders/customshadereffectped.h"
#include "shaders/customshadereffectvehicle.h"
#include "vehicles/vehicle.h"

namespace rage {
	extern grcEffect* g_DefaultEffect;
}

pgBase* CStreaming::PlaceDrawable(datResourceInfo& info, datResourceMap &map, const char *debugName, bool isDefrag)
{
	gtaDrawable* t = NULL;
	pgRscBuilder::PlaceStream(t, info, map, debugName, isDefrag);
	return t;
}

pgBase* CStreaming::PlaceFragType(datResourceInfo& info,  datResourceMap &map, const char *debugName, bool isDefrag)
{
	gtaFragType* t = NULL;
	pgRscBuilder::PlaceStream(t, info, map, debugName, isDefrag);
	return t;
}

pgBase* CStreaming::PlaceDwd(datResourceInfo& info,  datResourceMap &map, const char *debugName, bool isDefrag)
{
	pgDictionary<gtaDrawable>* t = NULL;
	pgRscBuilder::PlaceStream(t, info, map, debugName, isDefrag);
	return t;
}

pgBase* CStreaming::DefragmentDrawable(datResourceMap &map)
{
	gtaDrawable* object = static_cast<gtaDrawable*>(map.GetVirtualBase());
	datResource rsc(map,"defrag",true);
	gtaDrawable::Place(object, rsc);
	return object;
}

pgBase* CStreaming::DefragmentFragType(datResourceMap& map)
{
	gtaFragType* object = static_cast<gtaFragType*>(map.GetVirtualBase());
	datResource rsc(map,"defrag",true);
	gtaFragType::Place(object, rsc);
	return object;
}

pgBase* CStreaming::DefragmentDwd(datResourceMap& map)
{
	pgDictionary<gtaDrawable>* object = static_cast<pgDictionary<gtaDrawable>*>(map.GetVirtualBase());
	datResource rsc(map,"defrag",true);
	pgDictionary<gtaDrawable>::Place(object, rsc);
	return object;
}

