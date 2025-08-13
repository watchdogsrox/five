//
// name:		game.cpp
// description:	Class controlling all the game code
//
// C headers
#include <stdio.h>
#if __PPU
#include <sysutil/sysutil_gamecontent.h>
#include <cell/sysmodule.h>
#endif

#if __XENON
#include "system/xtl.h"
#endif

// rage headers
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "cranimation/frame.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h" 
#include "diag/restconsole.h"
#include "diag/restservice.h"
#include "diag/output.h"
#include "file/savegame.h"
#include "fragment/type.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/font.h"
#if __D3D11
#include "grcore/texturedefault.h"
#endif //__D3D11
#include "grprofile/timebars.h"
#if RSG_ORBIS
#include "video/contentExport_Orbis.h"
#include "video/contentSearch_Orbis.h"
#include "video/contentDelete_Orbis.h"
#endif //RSG_ORBIS
#include "memory/memdebug.h"
#include "parser/restparserservices.h"
#include "profile/telemetry.h"
#include "rline/rlpresence.h"
#include "rmptfx/ptxmanager.h"
#include "streaming/BoxStreamer.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "streaming/streamingvisualize.h"
#include "streaming/zonedassetmanager.h"
#include "system/exception.h"
#include "system/param.h"
#include "system/threadregistry.h"
#include "system/timemgr.h"
#include "system/service.h"
#include "system/stack.h"
#include "system/SteamVerification.h"
#include "telemetry/telemetry_rage.h"

// framework headers
#include "fwdebug/picker.h"
#include "fwutil/idgen.h"
#include "fwutil/quadtree.h"
#include "fwutil/profanityFilter.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "fwscene/stores/posematcherstore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/clothstore.h"
#include "fwscene/stores/imposedTextureDictionary.h"
#include "fwscene/stores/networkdefstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/search/Search.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "fwanimation/move.h"
#include "fwanimation/expressionsets.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwsys/metadatastore.h"
#include "fwsys/game.h"
#include "fwsys/timer.h"
#include "video/greatestmoment.h"
#include "video/VideoPlayback.h"
#include "video/VideoPlaybackThumbnailManager.h"

// game headers
#include "ai/AnimBlackboard.h"
#include "ai/EntityScanner.h"
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/ambient/ConditionalAnimManager.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "animation/AnimBones.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/ClipDictionaryStoreInterface.h"
#include "animation/debug/AnimViewer.h"
#include "animation/animmanager.h"
#include "animation/GestureManager.h"
#include "animation/MovePed.h"
#include "apps/appdata.h"
//#include "audio/audiogeometry.h"
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "cloth/ClothArchetype.h"
#include "control/Gen9ExclusiveAssets.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/gps.h"
#include "control/WaypointRecording.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "control/videorecording/videorecording.h"
#include "control/replay/ReplayRecording.h"
#include "control/restart.h"
#include "control/slownesszones.h"
#include "control/stuntjump.h"
#include "control/trafficlights.h"
#include "core/dongle.h"
#include "Core/UserEntitlementService.h"
//#include "Cutscene/CutSceneManagerNew.h"
#include "Cutscene/CutsceneManagerNew.h"
#include "debug/AmbientMarkers.h"
#include "debug/AssetAnalysis/StreamingIteratorManager.h"
#include "debug/blockview.h"
#include "debug/debug.h"
#include "debug/bar.h"
#include "debug/DebugGlobals.h"
#include "debug/Rendering/DebugRendering.h"
#include "debug/debugscene.h"
#include "debug/OverviewScreen.h"
#if __DEV
#include "debug/DebugDraw/DebugVolume2.h"
#include "debug/DebugDraw/DebugHangingCableTest.h"
#endif // __DEV
#include "debug/gesturetool.h"
#include "debug/MarketingTools.h"
#include "debug/NodeViewer.h"
#include "debug/vectormap.h"
#include "debug/LightProbe.h"
#include "debug/SceneGeometryCapture.h"
#include "debug/textureviewer/textureviewer.h"
#include "debug/TiledScreenCapture.h"
#include "event/ShockingEvents.h"
#include "event/system/EventDataManager.h"
#include "fragment/cacheheap.h"
#include "frontend/Credits.h"
#if __DEV
#include "frontend/GolfTrail.h"
#endif // __DEV
#include "frontend/NewHud.h"
#include "frontend/BusySpinner.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/landing_page/LegacyLandingScreen.h"
#include "frontend/MiniMap.h"
#include "frontend/HudMarkerManager.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/MultiplayerGamerTagHud.h"
#include "frontend/MobilePhone.h"
#if RSG_PC
#include "frontend/MultiplayerChat.h"
#include "frontend/MousePointer.h"
#endif
#include "system/TamperActions.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "frontend/ReportMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/FrontendStatsMgr.h"
#include "frontend/ControllerLabelMgr.h"
#include "frontend/landing_page/LandingPageController.h"
#include "frontend/LanguageSelect.h"
#include "frontend/LoadingScreens.h"
#include "frontend/TextInputBox.h"
#if GTA_REPLAY
#include "frontend/VideoEditor/debug/DebugVideoEditorScreen.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/WatermarkRenderer.h"
#endif
#include "Network/General/NetworkAssetVerifier.h"
#include "Network/Live/NetworkTelemetry.h"
#if !__FINAL
#include "Network/Live/NetworkDebugTelemetry.h"
#endif // !__FINAL
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "game/CrimeInformation.h"
#include "Game/Dispatch/DispatchData.h"
#include "game/dispatch/dispatchmanager.h"
#include "game/dispatch/RoadBlock.h"
#include "game/cheat.h"
#include "game/config.h"
#include "game/clock.h"
#include "game/GameSituation.h"
#include "game/localisation.h"
#include "game/modelindices.h"
#include "game/Performance.h"
#include "game/Riots.h"
#include "game/user.h"
#include "game/weather.h"
#include "game/WitnessInformation.h"
#include "game/Zones.h"
#include "objects/DummyObject.h"
#include "objects/ProceduralInfo.h"
#include "objects/ProcObjects.h"
#include "pathserver/ExportCollision.h"
#include "pathserver/navedit.h"
#include "peds/AssistedMovement.h"
#include "Peds/CombatBehaviour.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "Peds/gangs.h"
#include "Peds/Horse/horseTune.h"
#include "Peds/Horse/horseComponent.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/PedGeometryAnalyser.h"
#include "Peds/PedHelmetComponent.h"
#include "peds/PedIntelligenceFactory.h"
#include "peds/PedIntelligence/PedAILodManager.h"
#include "peds/PedMoveBlend/PedMoveBlendManager.h"
#include "peds/PedMoveBlend/pedmoveblendonfoot.h"
#include "peds/PedMoveBlend/pedmoveblendonfootPM.h"
#include "peds/PedMoveBlend/PedMoveBlendOnSkis.h"
#include "peds/pedpopulation.h"
#include "peds/ped.h"
#include "peds/popcycle.h"
#include "peds/popzones.h"
#include "peds/rendering/PedHeadshotManager.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "performance/clearinghouse.h"
#include "physics/breakable.h"
#include "physics/physics.h"
#include "physics/rope.h"
#include "physics/debugplayback.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/PickupManager.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "renderer/shadows/shadows.h"
#include "renderer/lights/AmbientLights.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LODLights.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/PostScan.h"
#include "renderer/renderer.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/renderThread.h"
#include "renderer/Shadows/ParaboloidShadows.h"
#include "renderer/sprite2d.h"
#include "renderer/SSAO.h"
#include "renderer/TreeImposters.h"
#include "renderer/UI3DDrawManager.h"
#include "renderer/water.h"
#include "renderer/ZoneCull.h"
#if GTA_REPLAY
#include "replaycoordinator/ReplayCoordinator.h"
#endif
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_new_game_checks.h"	//	For CSavegameNewGameChecks::ShouldNewGameChecksBePerformed()
#include "SaveLoad/savegame_free_space_check.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "SaveMigration/SaveMigration.h"
#include "scene/animatedbuilding.h"
#include "scene/datafilemgr.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/decorator/decoratorinterface.h"
#include "scene/entities/compEntity.h"
#include "scene/EntityBatch.h"
#include "scene/EntityExt_AltGfx.h"
#include "scene/ExtraContent.h"
#include "scene/ExtraMetadataMgr.h"
#include "scene/FocusEntity.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/loader/mapTypes.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/lod/LodDebug.h"
#include "scene/playerswitch/PlayerSwitchDebug.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalInst.h"
#include "scene/scene.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/texLod.h"
#include "scene/Volume/VolumeManager.h"
#include "scene/world/WorldDebugHelper.h"
#include "scene/world/GameScan.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "scene/WarpManager.h"
#include "scene/worldpoints.h"
#include "script/commands_decorator.h"
#include "script/Script.h"		// Include this header first for Xenon compile to define GetCurrentThread before including the winbase.h
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_hud.h"
#include "script/streamedscripts.h"
#include "script/script_ai_blips.h"
#include "shaders/ShaderHairSort.h"
#include "streaming/CacheLoader.h"
#include "streaming/defragmentation.h"
#include "streaming/populationstreaming.h"
#include "streaming/PrioritizedClipSetStreamer.h"
#include "streaming/SituationalClipSetStreamer.h"
#include "streaming/streaming.h"
#include "streaming/streamingEngine.h"
#include "streaming/streaminginstall_psn.h"
#include "streaming/streamingrequestlist.h"
#include "system/buddyheap.h"
#include "system/EntitlementManager.h"
#include "system/memmanager.h"
#include "system/InfoState.h"
#include "system/StreamingInstall.winrt.h"
#include "security/ragesecengine.h"
#include "security/ragesecpluginmanager.h"

#if __BANK
#include "streaming/ResourceVisualizer.h"
#endif

#include "system/telemetry.h"
#include "Task/Combat/CombatDirector.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/combat/Subtasks/BoatChaseDirector.h"
#include "task/combat/Subtasks/VehicleChaseDirector.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/AmbientAudio.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/movement/climbing/ladders/LadderMetadataManager.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Response/Info/AgitatedManager.h"
#include "Task/Scenario/Info/ScenarioActionManager.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/System/TaskHelpers.h"
#include "task/System/Tuning.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"
#include "text/messages.h"
#include "text/text.h"
#include "TimeCycle/TimeCycle.h"
#include "tools/SectorTools.h"
#include "tools/SectorToolsParam.h"
#include "tools/SmokeTest.h"
#include "vehicles/buses.h"
#include "vehicles/cargen.h"
#include "vehicles/heli.h"
#include "vehicles/train.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicleAi/FlyingVehicleAvoidance.h"
#include "vehicleAi/Junctions.h"
#include "vehicleAi/pathzones.h"
#include "vehicleAi/vehicleIntelligence.h"
#include "vehicleAi/vehicleIntelligenceFactory.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Coronas.h"
#include "vfx/Misc/distantlights.h"
#include "vfx/Misc/MovieManager.h"
#include "vfx/misc/MovieMeshManager.h"
#include "vfx/Misc/Scrollbars.h"
#include "vfx/Misc/TVPlaylistManager.h"
#include "vfx/Misc/LODLightManager.h"
#include "vfx/Sky/Sky.h"
#include "vfx/Particles/PtFxManager.h"
#include "vfx/Systems/VfxTrail.h"
#include "vfx/Systems/VfxWater.h"
#include "vfx/Systems/VfxWeapon.h"
#include "vfx/Misc/Markers.h"
#include "vfx/visualeffects.h"
#include "vfx/misc/fire.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "weapons/Explosion.h"
#include "Weapons/Projectiles/ProjectileRocket.h"
#include "weapons/WeaponManager.h"
#include "weapons/TargetSequence.h"
#include "Shaders/ShaderEdit.h"
#include "game/MapZones.h"
#include "task/Movement/JetpackObject.h"

#if COMMERCE_CONTAINER
#include "network/Commerce/CommerceManager.h"
#endif

// NY headers
#include "core/game.h"
#include "core/core_channel.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "frontend/loadingscreens.h"
#include "frontend/MobilePhone.h"
#include "Stats/StatsMgr.h"
#include "objects/objectpopulationNY.h"
#include "pathserver/navgenparam.h"
#include "peds/PedFactory.h"
#include "renderer/DrawLists/DrawListNY.h"

// GTA headers
#include "animation/PMDictionaryStore.h"
#include "crParameterizedMotion/parameterizedmotiondictionary.h"

//Network Headers
#include "net/nethardware.h"
#include "network/Network.h"
#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif
#include "network/Live/livemanager.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "network/NetworkInterface.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "game/PopMultiplierAreas.h"

#include "frontend/SocialClubMenu.h"
#include "frontend/PolicyMenu.h"
#include "frontend/UIWorldIcon.h"

#include "scene/ipl/IplCullBox.h"

#include "system/EndUserBenchmark.h"


#if __DEV
PARAM(allowCutscenePeds,			"[game] Allow cutscene peds to be created");
#endif

#if DR_ENABLED
PARAM(DR_AutoSaveOExcetion,			"[game] Dump current event log to file on exception");
#endif

XPARAM(nonetwork);
XPARAM(previewfolder);

#if !__FINAL
PARAM(overrideNearClip, "[game] Use overriden near clip setting");
PARAM(playercoords,	"[game] Player start coords");
PARAM(playerheading,"[game] Player start heading");
PARAM(map, "[game] Map file used when loading game");
XPARAM(randomseed);		// this is used by rage now as well, may as well share
PARAM(netSignOutStartNewGame, "[game] Start a new game instead of running the landing page flow");
#endif

#if __BANK
PARAM(debugGestures, "[game] Debug gestures");
PARAM(debugGesturesOnSelectedPed, "[game] Debug gestures on the selected ped");
PARAM(pedWidgets,"[game] Create ped widget bank on game launch");
PARAM(aiWidgets,"[game] Create AI widget bank on game launch");
PARAM(physicsWidgets,"[game] Create physics widget bank on game launch");
XPARAM(resvisualize);
PARAM(doSaveGameSpaceChecks,"do save game space checks at startup");
#endif

namespace rage
{
	extern sysMemAllocator* g_rlAllocator;
}

RAGE_DEFINE_CHANNEL(core, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_FATAL)
RAGE_DEFINE_SUBCHANNEL(core, game, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_FATAL)

static rlPresence::Delegate g_PresenceDlgt(CGame::OnPresenceEvent);
#if RSG_ORBIS
static rlNpEventDelegate g_NpDlgt(CGame::OnNpEvent);
#endif

const unsigned MAP_LOADED_DEPENDENCY_LEVEL_1 = 1;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_2 = 2;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_3 = 3;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_4 = 4;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_5 = 5;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_6 = 6;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_7 = 7;
const unsigned MAP_LOADED_DEPENDENCY_LEVEL_8 = 8;


gameSkeleton CGame::m_GameSkeleton;
CGameSessionStateMachine CGame::m_GameSessionStateMachine;

#if USE_RAGE_RAND
mthRandom CRandom::ms_Rand;
#endif

mthRandom CRandomScripts::ms_Rand;
mthRandomMWC CRandomMWCScripts::ms_RandMWC;


void DiskCacheSchedule();

enum {
	GAME_INIT,
	GAME_INITLEVEL,
	GAME_INITSESSION,
};

static s32 g_loadStatus = GAME_INIT;

void* CGameTempAllocator::Allocate(size_t size, size_t align, int heapIndex /*= 0*/)
{
	return size ? fragManager::GetFragCacheAllocator()->ExternalAllocate(size, align, heapIndex) : NULL;
}

void* CGameTempAllocator::TryAllocate( size_t size,size_t align,int heapIndex /*= 0*/ )
{
	return Allocate(size,align,heapIndex);
}

void CGameTempAllocator::Free(const void *ptr)
{
	fragManager::GetFragCacheAllocator()->ExternalFree(ptr);
}

class CGtaGameInterface : public fwGameInterface
{
public:
	virtual void RegisterStreamingModules();
};

#if FRAG_CACHE_HEAP_DEBUG
void DumpStreamingInterests()
{
	g_strStreamingInterface->DumpStreamingInterests();
}
#endif

void CGtaGameInterface::RegisterStreamingModules()
{
	g_PoseMatcherStore.RegisterStreamingModule(); 
#if ENABLE_BLENDSHAPES
	g_BlendShapeStore.RegisterStreamingModule(); 
#endif // ENABLE_BLENDSHAPES
	g_ClipDictionaryStore.RegisterStreamingModule();
	g_CutSceneStore.RegisterStreamingModule(); 
	g_ExpressionDictionaryStore.RegisterStreamingModule();

#if USE_PM_STORE
	g_PmDictionaryStore.RegisterStreamingModule();
#endif // USE_PM_STORE
	g_ScaleformStore.RegisterStreamingModule(); 
	g_StaticBoundsStore.RegisterStreamingModule();
	g_StreamedScripts.RegisterStreamingModule();
	ptfxManager::GetAssetStore().RegisterStreamingModule();
	g_NetworkDefStore.RegisterStreamingModule();
	g_FrameFilterDictionaryStore.RegisterStreamingModule();

	CWaypointRecording::RegisterStreamingModule();
	CVehicleRecordingMgr::RegisterStreamingModule();
	CModelInfo::RegisterStreamingModule();
	CPathFind::RegisterStreamingModule();
	CPathServer::RegisterStreamingModule();
}

//
// name:		CreateFactories
// description:	Create class factories
void CGame::CreateFactories()
{
	CPedFactory::CreateFactory();				// create the ped factory
	CVehicleFactory::CreateFactory();			// create the vehicle factory
	CPedIntelligenceFactory::CreateFactory();	// create the pedintelligence factory
	CVehicleIntelligenceFactory::CreateFactory();	// create the vehicleintelligence factory
}

//
// name:		DestroyFactories
// description:	destroy class factories
void CGame::DestroyFactories()
{
	CPedFactory::DestroyFactory();			// destroy the ped factory
	CVehicleFactory::DestroyFactory();			// destroy the vehicle factory
	CPedIntelligenceFactory::DestroyFactory();	// destroy the pedintelligence factory
	CVehicleIntelligenceFactory::DestroyFactory();	// destroy the vehicleintelligence factory
}

#if __BANK
restServiceFileAccess g_RestCommonService("common", "common:/");
restServiceFileAccess g_RestPlatformService("platform", "platform:/");

restStatus::Enum PrintTimebars(restCommand& cmd)
{
#if RAGE_TIMEBARS
	float threshold = cmd.m_Query.FindAttributeFloatValue("threshold", 0.0f);
	g_pfTB.DumpTimebarsXML(cmd.m_OutputStream, threshold);
#endif // RAGE_TIMEBARS
	return restStatus::REST_OK;
}

restStatus::Enum PrintKeyboardMappings(restCommand& cmd)
{
	CControlMgr::GetKeyboard().DumpDebugKeysToXML(cmd.m_OutputStream);
	return restStatus::REST_OK;
}

restStatus::Enum PrintCamera(restCommand& cmd)
{
	Vector3 camPos  = camInterface::GetPos();
	Vector3 camDir = camInterface::GetFront();
	Vector3 camUp = camInterface::GetUp();
	Vector3 camRight = camInterface::GetRight();

	fprintf(cmd.m_OutputStream, // Output in a convenient format for consumption by liveedit
		"<CFreeCamAdjust>\n"
		"<vPos x=\"%f\" y=\"%f\" z=\"%f\"/>\n"
		"<vFront x=\"%f\" y=\"%f\" z=\"%f\"/>\n"
		"<vUp x=\"%f\" y=\"%f\" z=\"%f\"/>\n"
		"<vRight x=\"%f\" y=\"%f\" z=\"%f\"/>\n"
		"</CFreeCamAdjust>\n",
		camPos.x, camPos.y, camPos.z,
		camDir.x, camDir.y, camDir.z,
		camUp.x, camUp.y, camUp.z,
		camRight.x, camRight.y, camRight.z
		);
	return restStatus::REST_OK;
}

#if __ASSERT
// gta V specific string hacks
void CGame::InitStringHacks()
{	
	// make sure this string is registered first, because it comes from DLC it is transient & looks like
	// an asset store collision when it gets registered at DLC mount
	atNonFinalHashString tempString("hi@hei_cs1_02_1");
	Assert(tempString.TryGetCStr());
	Displayf("String hacks active : %s", tempString.GetCStr());
}
#endif //__ASSERT

void CGame::InitWidgets()
{
	// Start up our REST services (and web server)
	REST.Init();
	REST.GetRootService()->AddChild(g_RestCommonService);
	REST.GetRootService()->AddChild(g_RestPlatformService);
	restRegisterSimpleService("Profiling/Timebars", restSimpleCb(PrintTimebars));
	restRegisterSimpleService("Profiling/CurrentCamera", restSimpleCb(PrintCamera));
	restRegisterSimpleService("Keyboard/Mappings", restSimpleCb(PrintKeyboardMappings));
	restAddConsoleCommand();
	parRestParserGuidService::InitClass(g_DrawRand.GetInt()); // random number is the 'session ID'
#if PARSER_USES_EXTERNAL_STRUCTURE_DEFNS
	REST.CreateDirectories("Parser")->AddChild(*(rage_new parRestParserSchemaService("Schema")));
#endif

	BANKMGR.CreateBank("Optimization"); // Should be at start of in CGame::Init because everything depends on it

	CAILogManager::InitBank();
	audNorthAudioEngine::InitWidgets();
	CAppDataMgr::InitWidgets();
	CDebug::InitWidgets();
	CNodeViewer::InitWidgets();
	camManager::InitWidgets();
	CShaderLib::InitWidgets(CDrawListMgr::RequestShaderReload);
	CCheat::InitWidgets();
	CClock::InitWidgets();
	CControlMgr::InitWidgets();
	CCredits::InitWidgets();
	CFileLoader::InitWidgets();
	CScene::InitWidgets();
	CGameLogic::InitWidgets();
	CGarages::InitWidgets();
	CGenericGameStorage::InitWidgets();
	CGps::InitWidgets();
	CPopCycle::InitWidgets();
	CIkManager::InitWidgets();
	CLocalisation::InitWidgets();
	CMarketingTools::InitWidgets();
	CNetwork::InitWidgets();
	CObject::InitWidgets();
	COverviewScreenManager::InitWidgets();
	ThePaths.InitWidgets();
	CPathServer::InitWidgets();
	CPed::InitBank();
#if SCENARIO_DEBUG
	CScenarioDebug::InitBank();
#endif // SCENARIO_DEBUG
	CPedPopulation::InitWidgets();
	CActionManager::InitWidgets();
	CPhoneMgr::InitWidgets();
	CPedHelmetComponent::InitWidgets();
	CProjectileRocket::InitWidgets();
	CPhysics::InitWidgets();
	CCover::InitWidgets();
	//ioPad::AddWidgets();
	diagChannel::InitBank();
#if __PPU
	CPS3Achievements::InitWidgets();
#endif
	fwScan::InitWidgets(); // must be done before CRenderer::InitWidgets
	CRenderer::InitWidgets();
#if GTA_REPLAY
	CDebugVideoEditorScreen::InitWidgets();
	CReplayMgr::InitWidgets();
	CVideoEditorUi::InitWidgets();
#endif
#if RSG_PC
	CMousePointer::InitWidgets();
#endif // #if RSG_PC
//@@: range CGAME_INITWIDGETS {
#if TAMPERACTIONS_ENABLED
	TamperActions::InitWidgets();
	rageSecGamePluginManager::InitWidgets();
#endif // #if TAMPERACTIONS_ENABLED
//@@: } CGAME_INITWIDGETS 
	CScaleformMgr::InitWidgets();
	CGameStreamMgr::InitWidgets();
	fwSearch::InitWidgets();
	CNewHud::InitWidgets();
	CNetworkTransitionNewsController::InitWidgets();
	CBusySpinner::InitWidgets();
	CMultiplayerGamerTagHud::InitWidgets();
	CText::InitWidgets();
	CMiniMap::InitWidgets();
	CHudMarkerManager::InitWidgets();
	CPauseMenu::InitWidgets();
#if __XENON
	CProfileSettings::InitWidgets();
#endif	//	__XENON
	CMessages::InitWidgets();
	UIWorldIconManager::InitWidgets();
    cStoreScreenMgr::InitWidgets();
#if RSG_PC
	CTextInputBox::InitWidgets();
	CMultiplayerChat::InitWidgets();
#endif // RSG_PC
#if (SECTOR_TOOLS_EXPORT)
	CSectorTools::InitWidgets();
#endif
	CStreaming::InitWidgets();
	CStuntJumpManager::InitWidgets();
	CVehiclePopulation::InitWidgets();
	CPopulationHelpers::InitWidgets();
	CVehicleAILodManager::InitWidgets();
	CObjectPopulation::InitWidgets();
	CJunctions::InitWidgets();
	CVehicleRecordingMgr::InitWidgets();
	CVisualEffects::InitWidgets();
	CWanted::InitWidgets();
	CWaypointRecording::InitWidgets();
	g_ShaderEdit::InstanceRef().InitWidgets();
#if __DEV
	CGolfTrailInterface::InitWidgets();
	CDVGeomTestInterface::Get().InitWidgets();
#if HANGING_CABLE_TEST
	CHangingCableTestInterface::InitWidgets();
#endif // HANGING_CABLE_TEST
#endif // __DEV
#if __NAVEDITORS
	CNavResAreaEditor::Init();
	CNavDataCoverpointEditor::Init();
#endif
	LightProbe::AddWidgets();
	TiledScreenCapture::AddWidgets(BANKMGR.CreateBank("Tiled Screen Capture"));
	SceneGeometryCapture::AddWidgets();
	CStreamingIteratorManager::AddWidgets(BANKMGR.CreateBank("Streaming Iterator"));
	CDebugTextureViewerInterface::InitWidgets();
	CRenderPhaseCascadeShadowsInterface::InitWidgets();

	fwRenderListBuilder::AddOptimizationWidgets();
	fwScan::InitOptimizationWidgets();
	CEntity::AddInstancePriorityWidget();
	Water::InitOptimizationWidgets();
	
	// widget dependencies
	g_procObjMan.InitWidgets();					// dependent on CObject
	CTrafficLights::InitWidgets();				// dependent on ThePaths
	CVehicleFactory::InitWidgets();				// dependent on CControlMgr
	CDebug::InitRageDebugChannelWidgets();

	bkBank* pBank = BANKMGR.FindBank("Optimization");
	RAGE_TIMEBARS_ONLY(g_pfTB.AddWidgets(*pBank));
	sysTaskManager::AddWidgets(*pBank);
#if THREAD_REGISTRY
	sysThreadRegistry::AddWidgets(*pBank);
#endif // THREAD_REGISTRY

	bkBank& bankDisplay = BANKMGR.CreateBank("Displays");
	GRCDEVICE.AddWidgets(bankDisplay);

#if VOLUME_SUPPORT
    g_VolumeManager.InitWidgets();
#endif // VOLUME_SUPPORT

#if DECORATOR_DEBUG
    gp_DecoratorInterface->InitWidgets();
#endif

#if USE_TELEMETRY
	CTelemetry::InitWidgets();
#endif
	CNetRespawnMgr::InitWidgets();

	if (PARAM_resvisualize.Get())
		CResourceVisualizerManager::GetInstance().InitWidgets();

#if __BANK
#	if __PPU || RSG_ORBIS || RSG_DURANGO
    CDongle::InitWidgets();
#	endif
#endif
}


void CGame::ProcessWidgetCommandLineHelpers()
{
	if(PARAM_pedWidgets.Get())
	{
		CPed::CreateBank();
	}
	if(PARAM_aiWidgets.Get())
	{
		CPedDebugVisualiserMenu::CreateBank();
	}
	if(PARAM_physicsWidgets.Get())
	{
		CPhysics::CreatePhysicsBank();
	}
}


void CGame::InitLevelWidgets()
{
	CAnimViewer::InitLevelWidgets();
	CBlockView::InitLevelWidgets();
	CutSceneManager::GetInstance()->InitLevelWidgets();
	CNetwork::InitLevelWidgets();
	CPickupManager::InitLevelWidgets();
//	CPlayerSettingsMgr::InitLevelWidgets();

#if TC_DEBUG
	g_timeCycle.InitLevelWidgets();
#endif // TC_DEBUG

	g_weather.InitLevelWidgets();
}

void CGame::ShutdownLevelWidgets()
{
	CAnimViewer::ShutdownLevelWidgets();
	CBlockView::ShutdownLevelWidgets();
	CutSceneManager::GetInstance()->ShutdownLevelWidgets();
	CNetwork::ShutdownLevelWidgets();
	CPickupManager::ShutdownLevelWidgets();
//	CPlayerSettingsMgr::ShutdownLevelWidgets();

#if TC_DEBUG
	g_timeCycle.ShutdownLevelWidgets();
#endif 

	g_weather.ShutdownLevelWidgets();
}

static void UpdateRestServices()
{
	PF_START_TIMEBAR("REST");
	REST.Update();
}

#endif // __BANK

// callback for when blocking on loading to avoid network timeouts
static const unsigned int KEEP_ALIVE_CALLBACK_INTERVAL = 50;
extern bool g_GameRunning;
BANK_ONLY(sysTimer keepalivetime);
BANK_ONLY(bool firstKeepAliveCall = true);
static void KeepAliveCallback()
{
#if __BANK
	if(firstKeepAliveCall)
	{
		keepalivetime.Reset();
		firstKeepAliveCall = false;
	}
	float elapsedtime = keepalivetime.GetMsTime();
#endif // __BANK
#if RSG_PC
#if __D3D11
	GRCDEVICE.CheckDeviceStatus();
#endif
#endif
	CNetwork::Update(true);
	CLoadingScreens::KeepAliveUpdate();

	g_SysService.UpdateClass();

#if __BANK && 0
	// Also keep the bank up to date here, to make sure we're flushing any queues (in particular the invoke queue)
	BANKMGR.Update();
#endif

#if __BANK
		static size_t trace[32];
		if (!g_GameRunning)
		{
			if (elapsedtime > 1000)
			{
				diagLoggedPrintf("=== Very long wait until keepalive %2.3fms\n", elapsedtime);
				diagLoggedPrintf("=== Last called from\n");
				sysStack::PrintCapturedStackTrace(trace, 32);
				diagLoggedPrintf("=== Now at\n");
				sysStack::PrintStackTrace();

			}
			keepalivetime.Reset();
		}
		sysStack::CaptureStackTrace(trace, 32, 1);
#endif // __BANK
}


//
// name:		CGame::PreLoadingScreensInit()
// description:	things that need to be initialised before the display of the loading screens to get text rendered
void CGame::PreLoadingScreensInit()
{
	g_movieMgr.Init(INIT_CORE);	
	CRenderer::PreLoadingScreensInit();

	//Temp fix for crash on startup due to new bink movies.
	//The bink movies create and lock textures on the main thread but never process them on the render thread until after they're deleted.
	//Unregister the renderthread update functions to prevent this happening. Damo wants to rewrite some of this stuff at some point to fix these issues.
#if __D3D11 && RSG_PC
	grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(NULL);
	grcTextureD3D11::SetCancelPendingUpdateGPUCopy(NULL);
#endif //__D3D11 && RSG_PC

#if RSG_ORBIS
    // load prxs early to avoid audio corruption during the intro movie
    rlNpCommonDialog::InitLibs();
    rlNpParty::InitLibs();
    rlNp::InitLibs();
#endif

	CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_INTRO_MOVIE, 0);

	// A Separate Render thread & the startup movie should be ready by this point
	// so we are safe to load the remaining shaders
	CShaderLib::Init();

#if __D3D11 && RSG_PC
	grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(dlCmdGrcDeviceUpdateBuffer::AddTextureUpdate);
	grcTextureD3D11::SetCancelPendingUpdateGPUCopy(dlCmdGrcDeviceUpdateBuffer::CancelBufferUpdate);
#endif //__D3D11 && RSG_PC

#if __BANK
	sysException::GameExceptionDisplay = CGame::GameExceptionDisplay;
	sysMemOutOfMemoryDisplay = CGame::OutOfMemoryDisplay;	
#endif 

#if ENABLE_DEFRAG_CALLBACK
	// Set the buddy heap callback
	sysBuddyHeap::SetIsObjectReadyToDelete(CStreaming::IsObjectReadyToDelete);
#endif

	ASSERT_ONLY(sysMemVerifyMainThread = CGame::VerifyMainThread;)

	fwGame::InitClass(rage_new CGtaGameInterface);
	INIT_DATAFILEMGR;

	fwGame::Init();

	fwScan::InitClass();

	fwProfanityFilter::GetInstance().Init();

	CStreaming::Init(INIT_CORE);

	CMapFileMgr::Init(INIT_CORE);

	// Load the startup files - the RPFs we need right away.
	DATAFILEMGR.Load("platformcrc:/data/startup");

	// Make their contents available to the streaming system.
	CFileLoader::AddAllRpfFiles(true);

#if !__FINAL
	fwAssetStoreBase::InitClass();
#endif // !__FINAL

	CFileLoader::InitClass();

	if(ASSET.Exists("commoncrc:/data/contentpatch.xml", NULL))
	{
		DATAFILEMGR.Load("commoncrc:/data/contentpatch");
	}

	if(ASSET.Exists("update:/content.xml", NULL))
	{
		DATAFILEMGR.Load("update:/content");
	}

	DATAFILEMGR.Load("platformcrc:/data/default");

	const char *previewFolder = NULL;
	PARAM_previewfolder.Get(previewFolder);
	strStreamingEngine::InitLive(previewFolder);

#if __PPU
	fiPsnUpdateDevice::Instance();
	fiPsnExtraContentDevice::Instance();
#endif

	// setup LiveManager
	AssertVerify(CLiveManager::Init(INIT_CORE));

	CGenericGameStorage::Init(INIT_CORE);

	// setup profile settings
	CProfileSettings::InitClass();

	// Initialise DLC system
	CExtraContentWrapper::Init(INIT_CORE);

	EXTRACONTENT.ExecuteTitleUpdateDataPatch(CCS_TITLE_UPDATE_STARTUP, true); 
	EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_TITLEUPDATE_STARTUP, true);
	if (CExtraContentManagerSingleton::IsInstantiated())
	{
		EXTRACONTENT.ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_DLC_PATCH, true);
		EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_DLC_PATCH, true);
		EXTRACONTENT.LoadLevelPacks();
	}
	CStreaming::LoadStreamingMetaFile();
	CStreaming::RegisterStreamingFiles();
	DATAFILEMGR.SnapshotFiles();
#if __STEAM_BUILD
	CSteamVerification::Init();
#endif
	CScaleformMgr::LoadPreallocationInfo(SCALEFORM_PREALLOC_XML_FILENAME);
	CScaleformMgr::HandleNewTextureDependencies();

	// Set profile change functions. If we have a logged in user this calls CGame::InitProfile() which requires LiveManager, ExtraContent and Profilesettings to be initialised
	CControlMgr::SetProfileChangeFunctions(MakeFunctor(&CGame::InitProfile), MakeFunctor(&CGame::ShutdownProfile));

	// ensure profile is loaded
	CProfileSettings::GetInstance().UpdateProfileSettings(true);

	// setup language preferences based the system
	CPauseMenu::SetDefaultValues();

	// because we now use text on the legal/loading screen, we initialise the font & text 1st:
	CScaleformMgr::Init(INIT_CORE);

#if __PS3
	// ps3 profile is loaded by this point so we can work out which language to select
	CLanguageSelect::ShowLanguageSelectScreen();
#endif

	CMessages::Init();
	TheText.Load();  

	EXTRACONTENT.ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_TEXT, true);
	EXTRACONTENT.ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_TEXT, true);

#if __XENON
	// Needed for cloth rendering
	// 1 for update/1 for draw/2 for double buffered command buffer
	grmGeometry::SetVertexMultiBufferCount(4);
#endif // __xENON

#if RSG_ENTITLEMENT_ENABLED
	CEntitlementManager::Init(0);
#endif
}


//
// name:		CGame::Init()
// description:	Initialise CGame class
void CGame::Init()
{	
	gameDebugf1("### Init ###");

	//@@: location CGAME_REGISTER_GAME_AUTOID_INIT
	AutoIdInit();
	//@@:  range CGAME_INIT_BODY { 
	//@@: location CGAME_REGISTER_GAME_SKELETON_FUNCTIONS
    RegisterGameSkeletonFunctions();


    m_GameSessionStateMachine.Init();

	// delegate for sign in state events
	rlPresence::AddDelegate(&g_PresenceDlgt);
	
	CStreaming::SetKeepAliveCallback(KeepAliveCallback, KEEP_ALIVE_CALLBACK_INTERVAL);
	KeepAliveCallback();
	//@@: location CGAME_INIT_CREATE_FACTORIES
	CreateFactories();

    SAVEGAME.InitClass(); // does nothing

#if RSG_ORBIS
	SaveGameSetup();
#endif
	
    fwDecoratorInterface::InitClass(rage_new CDecoratorInterface());

#if ENABLE_HORSE
	// Horse simulation tuning
	hrsSimTuneMgr::InitClass();	
	CReins::InitClass();
#endif	

	// Create the frag cache allocator before we do ANYTHING with skeletons!
	//@@: location CGAME_INIT_FRAGMANAGER_CREATEALLOCATOR
	fragManager::CreateAllocator();
	FRAG_CACHE_HEAP_DEBUG_ONLY(fragCacheAllocator::SetStreamingInterestsCallback(DumpStreamingInterests));

	static CGameTempAllocator gameTempAllocator;
	sysMemManager::GetInstance().SetFragCacheAllocator(gameTempAllocator);

	// START INIT
		PF_PUSH_STARTUPBAR("Init Core");
		m_GameSkeleton.Init(INIT_CORE);
		PF_POP_STARTUPBAR();

	PF_START_STARTUPBAR("Profile Setup");
	// needs to come after INIT_CORE

	// force an update on profile settings in case we just setup a valid profile
	//@@: location CGAME_INIT_UPDATE_PROFILE_SETTINGS
	CProfileSettings::GetInstance().UpdateProfileSettings(true);
	SPlayerCardXMLDataManager::Instantiate();
	//@@: } CGAME_INIT_BODY 
#if __BANK
	if (PARAM_resvisualize.Get())
	{
		if (!CResourceVisualizerManager::IsInstantiated())
			CResourceVisualizerManager::Instantiate();

		CResourceVisualizerManager::GetInstance().InitClass();		
	}	
#endif

#if __BANK
	InitWidgets();
#endif
	
#if __ASSERT
	InitStringHacks();
#endif //__ASSERT
	//@@: location CGAME_INIT_TELEMETRY_INIT
	TelemetryInit();

#if USE_TELEMETRY
	CTelemetry::Init();
#endif

#if __BANK
	COverviewScreenManager::Init();
#endif	//__BANK
}

//
// name:		CGame::Shutdown()
// description:	shutdown CGame class
void CGame::Shutdown()
{
	gameDebugf1("### Shutdown ###");

#if __BANK
	if (PARAM_resvisualize.Get())
		CResourceVisualizerManager::GetInstance().ShutdownClass();

	COverviewScreenManager::Shutdown();
#endif	//__BANK

	TelemetryShutdown();

    fwDecoratorInterface::ShutdownClass();

	CGtaOldLoadingScreen::ForceLoadingRenderFunction(true);

	// If the level haven't been shutdown
	if(g_loadStatus == GAME_INITSESSION)
		ShutdownSession();

	if(g_loadStatus == GAME_INITLEVEL)
		ShutdownLevel();

	CGenericGameStorage::FinishAllUnfinishedSaveGameOperations();
	CProfileSettings::GetInstance().WriteNow();
	SPlayerCardXMLDataManager::Destroy();

    gRenderThreadInterface.Shutdown();		// halt render thread and return d3d device to this thread

#if __DEV
	ShutdownNYDrawListDebug();
#endif

#if !__FINAL
	CGameWorld::ValidateShutdown();
#endif // !__FINAL

    m_GameSkeleton.Shutdown(SHUTDOWN_CORE);
	CStreaming::MarkFirstInitComplete(false);
	
    DestroyFactories();
	fwQuadTreeNode::ShutdownPool();
	SAVEGAME.ShutdownClass();

	AutoIdShutdown();

	fwScan::ShutDownClass();

	fwGame::ShutdownClass();
}

//
// name:		ReadPlayerCoordsFile
// description:	Read the player coordinates from a file 	
void ReadPlayerCoordsFile(int NOTFINAL_ONLY(level))
{
#if !__FINAL
	char *pLine;
	Vector3 posn;
	
	CScriptDebug::SetPlayerCoordsHaveBeenOverridden(false);

	float coords[3];
	if(PARAM_playercoords.GetArray(coords, 3))
	{
		posn = Vector3(coords[0], coords[1], coords[2]);
		CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		if(pPlayer)
		{
			float heading = pPlayer->GetCurrentHeading();	
			PARAM_playerheading.Get(heading);
			g_SceneStreamerMgr.LoadScene(posn);
			pPlayer->Teleport(posn, heading);

			CScriptDebug::SetPlayerCoordsHaveBeenOverridden(true);
		}	
		CStreaming::SetIsPlayerPositioned(true);
	}
	else
	{
		CFileMgr::SetDir("");

		char filename[64];
		sprintf(filename, "playercoords_%s.txt", CScene::GetLevelsData().GetFriendlyName(level));
		FileHandle fid = CFileMgr::OpenFile(filename);
		if(!CFileMgr::IsValidFileHandle(fid))
		{
			fid = CFileMgr::OpenFile("playercoords.txt");
		}

		if(CFileMgr::IsValidFileHandle(fid))
		{
			pLine = CFileMgr::ReadLine(fid);
			if(sscanf(pLine, "%f%f%f", &posn.x, &posn.y, &posn.z) == 3)
			{
				CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
				if(pPlayer)
				{
					g_SceneStreamerMgr.LoadScene(posn);
					pPlayer->Teleport(posn, pPlayer->GetCurrentHeading());

					CScriptDebug::SetPlayerCoordsHaveBeenOverridden(true);
				}	
			}
			CFileMgr::CloseFile(fid);
			CStreaming::SetIsPlayerPositioned(true);
		}	
	}
#endif
}

u32 reverseBits(u32 n) {
	n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
	n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
	n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
	n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
	n = ((n >> 16) & 0x0000ffff) | ((n << 16) & 0xffff0000);
	return n;
}

//
// name:		CGame::InitLevel()
// description:	Initialise level
void CGame::InitLevel(int level)
{
	gameDebugf1("### InitLevel ###");
	CLoadingScreens::TickLoadingProgress(false);

	PF_START_STARTUPBAR("Init random");

	u32 seed = static_cast<u32>(sysTimer::GetTicks());
#if !__FINAL
	PARAM_randomseed.Get(seed);
#endif
	//@@: location CGAME_INITLEVEL
	fwRandom::SetRandomSeed(seed);
	CRandomScripts::SetRandomSeed(seed);

	u32 scrambleSeed = (seed^reverseBits(seed));
	CRandomMWCScripts::SetRandomSeed(scrambleSeed);

#if RSG_NP
	// Not sure if this is a good place to put these two lines
	CLiveManager::GetAchMgr().SetTrophiesEnabled(g_rlNp.GetTrophy().IsInitialised());
	CLiveManager::GetAchMgr().SetOwnedSave(true);
#endif

		PF_PUSH_STARTUPBAR("Init before map loaded");
		m_GameSkeleton.Init(INIT_BEFORE_MAP_LOADED);
		PF_POP_STARTUPBAR();

	WIN32PC_ONLY(CApp::CheckExit());

	//g_DrawableStore.PlaceDummies();

#if !__FINAL
	// Load the game in animation viewer mode?
	const char* pMapName;
	if(PARAM_map.Get(pMapName))
	{
		// load in map file specified by the command line
		PF_PUSH_STARTUPBAR("Load map");
		CScene::LoadMap(pMapName);
		CModelIndex::MatchAllModelStrings();
	}
	else
#endif // !__FINAL
	{
		PF_PUSH_STARTUPBAR("Load map");
		//@@: range CSCENE_LOADMAP {
		CScene::LoadMap(level);
		//@@: } CSCENE_LOADMAP

		CModelIndex::MatchAllModelStrings();
	}
	PF_POP_STARTUPBAR();

	WIN32PC_ONLY(CApp::CheckExit());

		PF_PUSH_STARTUPBAR("Init after map loaded");
		m_GameSkeleton.Init(INIT_AFTER_MAP_LOADED);
		PF_POP_STARTUPBAR();

	WIN32PC_ONLY(CApp::CheckExit());

#if __BANK
	PF_START_STARTUPBAR("Init widgets");
	InitLevelWidgets();
#endif

	CLODLightManager::CreateLODLightImapIDXCache();
	//CLoadingScreens::TickLoadingProgress(false);
	g_loadStatus = GAME_INITLEVEL;

#if SCRATCH_ALLOCATOR
	sysMemManager::GetInstance().GetScratchAllocator()->SetReady(true);
#endif
}

void CGame::InitLevelAfterLoadingFromMemoryCard(int level)
{
//	CGameLogic::SetCurrentEpisodeIndex(ms_NewEpisodeIndex); // no leak
	CGameLogic::SetCurrentLevelIndex(level);

#if __BANK
	CScriptDebug::InitWidgets();				// no leak
	CVehicleFactory::UpdateCarList();
#endif

	CPed * pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{
#if __BANK
		CAnimViewer::Init();
#endif //__BANK
    }


	CGenericGameStorage::Init(INIT_AFTER_MAP_LOADED);

	CSavegameAutoload::SetPerformingAutoLoadAtStartOfGame(false);

#if RSG_ORBIS 

#if RSG_BANK
	if (PARAM_doSaveGameSpaceChecks.Get())
	{
#endif
		if (CSavegameNewGameChecks::ShouldNewGameChecksBePerformed())
		{
			//Check that we aren't doing any other file IO during this section
			//don't call network update after this either as it can issue new 
			//requests for cloud cache
			CloudCache::Drain();
			pgStreamer::Drain();
			//check that we have the minimum required memory to save the game
			//take into account the already existing saved games
			CSavegameQueuedOperation_SpaceCheck NewGameChecks;
			NewGameChecks.Init();
			CGenericGameStorage::PushOnToSavegameQueue(&NewGameChecks);
			//sleep while we do these checks
			PF_PUSH_TIMEBAR("CGenericGameStorage Update");
			while (NewGameChecks.GetStatus() == MEM_CARD_BUSY)
			{
				CGenericGameStorage::Update();
				sysIpcSleep(1);
			}
			PF_POP_TIMEBAR();
			CProfileSettings::GetInstance().WriteNow(false); // ensure profile is saved in setup, before we start heavy streaming
		}	
#if RSG_BANK
	}
	else
	{
		CSavegameNewGameChecks::SetFreeSpaceChecksCompleted(true);
	}
#endif
#endif

//	g_loadStatus = GAME_INITLEVEL;
}

//
// name:		CGame::ShutdownLevel()
// description:	shutdown level
void CGame::ShutdownLevel()
{
	gameDebugf1("### ShutdownLevel ###");
    
    g_loadStatus = GAME_INIT;

    m_GameSkeleton.Shutdown(SHUTDOWN_WITH_MAP_LOADED);

	// Some debug stuff to make sure there are no entities left.
	Assert(CVehicle::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CPed::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CObject::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CDummyObject::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CBuilding::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CEntityBatch::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CGrassBatch::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CInteriorInst::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CInteriorProxy::GetPool()->GetNoOfUsedSpaces() == 0);
	Assert(CPortalInst::GetPool()->GetNoOfUsedSpaces() == 0);

	// This must be the last thing to be shutdown, hence it is not registered with the game skeleton.
    // This way any extra content devices are still available when the other systems are shutting down.
    // 
	g_uniqueObjectIDGenerator.Reset();

#if __BANK
	ShutdownLevelWidgets();
#endif
}

//
// name:		CGame::InitProfile
// description:	
void CGame::InitProfile()
{
	if(CLiveManager::IsInitialized())
		CLiveManager::InitProfile();

	if(CExtraContentManagerSingleton::IsInstantiated())
		EXTRACONTENT.InitProfile();
}

//
// name:		CGame::ShutdownProfile
// description:	
void CGame::ShutdownProfile()
{
	if (CExtraContentManagerSingleton::IsInstantiated())
		EXTRACONTENT.ShutdownProfile();

	if(CLiveManager::IsInitialized())
		CLiveManager::ShutdownProfile();
}

void CGame::InitSession(int level, bool bLoadingAPreviouslySavedGame)
{
    gameDebugf1("### InitSession ###");
	CLoadingScreens::TickLoadingProgress(false);
	PF_PUSH_STARTUPBAR("Init Session");
	PF_PUSH_STARTUPBAR("game skel");
		m_GameSkeleton.Init(INIT_SESSION);
	CStreaming::MarkFirstInitComplete();
	PF_POP_STARTUPBAR();

	WIN32PC_ONLY(CApp::CheckExit());

#if !__FINAL
	CStreaming::EndRecordingStartupSequence();
#endif // !__FINAL

	
	// At this point, the vehicle model info should be set up, so now the navmesh store can
	// set the model index for the dynamic navmeshes.
	PF_START_STARTUPBAR("nav mesh");
	// JB: This is no longer necessary
	CPathServer::GetNavMeshStore(kNavDomainRegular)->InitDynamicNavMeshesAfterVehSetup();

#if !__FINAL
	CScriptDebug::SetPlayerCoordsHaveBeenOverridden(false);
#endif	//	!__FINAL

	PF_START_STARTUPBAR("create player");
    //TODO: [MIGUEL]
	// Create Main Player - This will reviewed later on. When I get my hands on 
	//   making the game run without a player this will be removed/moved to a 
	//   better place if that is the case... [Miguel]
 	if (!CGameWorld::GetMainPlayerInfo())
 	{
 		Vector3 vec(0.0f, 0.0f, 0.0f);
		const CModelInfoConfig& cfgModel = CGameConfig::Get().GetConfigModelInfo();
		CGameWorld::CreatePlayer(vec, atHashString(CFileMgr::IsGameInstalled() ? cfgModel.m_defaultPlayerName : cfgModel.m_defaultProloguePlayerName), 200.0f);
	}
	// END TODO

    // finished loading, generate asset checks network session integrity
    CNetwork::GetAssetVerifier().Init();

	PF_START_STARTUPBAR("script init");
	CTheScripts::SetProcessingTheScriptsDuringGameInit(true);
	CLoadingScreens::TickLoadingProgress(true);	
	CTheScripts::Process();
	CTheScripts::SetProcessingTheScriptsDuringGameInit(false);
	strStreamingEngine::GetLoader().CallKeepAliveCallback();
	PF_START_STARTUPBAR("load game");
	if (!bLoadingAPreviouslySavedGame)
	{
		// If we aren't loading from memory card then schedule the imgs about the player start posn
		DiskCacheSchedule();

#if !__FINAL
		ReadPlayerCoordsFile(level);		//	GSW_ - given that there are 5 maps, does this still make sense at all?
#endif

		InitLevelAfterLoadingFromMemoryCard(level);	// leak free

		CNetworkTelemetry::NewGame();
	}

#if __BANK
	CGame::ProcessWidgetCommandLineHelpers();
#endif

	PF_POP_STARTUPBAR();
	
	g_loadStatus = GAME_INITSESSION;
}

bool CGame::IsSessionInitialized()
{
    return g_loadStatus == GAME_INITSESSION;
}

bool CGame::IsLevelInitialized()
{
	return g_loadStatus == GAME_INITLEVEL;	
}

void CGame::ShutdownSession()
{
	STRVIS_SET_MARKER_TEXT("Shutdown Session");
    gameDebugf1("### ShutdownSession ###");

	g_loadStatus = GAME_INITLEVEL;

	CLiveManager::GetAchMgr().ClearAchievementProgress();

	gStreamingRequestList.Finish();
    CControlMgr::StopPlayerPadShaking(true);
	STRVIS_SET_MARKER_TEXT("Game Stream Queue Flush");
	CGameStreamMgr::GetGameStream()->FlushQueue();
    gRenderThreadInterface.Flush();

	STRVIS_SET_MARKER_TEXT("Streamer Drain");
    pgStreamer::Drain();
	STRVIS_SET_MARKER_TEXT("Shutdown Stuff");

#if USE_DEFRAGMENTATION
	strStreamingEngine::GetDefragmentation()->Abort();
#endif // USE_DEFRAGEMENTATION

    // Make sure to clear any old input data history.
	CControlMgr::GetMainPlayerControl().InitEmptyControls();

    CMiniMap::SwitchOffWaypoint();  // switch of any waypoint/gps blip that may have previously been set up

	g_ScriptAudioEntity.AbortScriptedConversation(false BANK_ONLY(,"Shutting down"));

#if __PPU
	if (!CProfileSettings::GetInstance().GetDontWriteProfile())
#endif
	{
		CProfileSettings::GetInstance().Write();
		// start the write. Ensure on PS3 that the write finishes as well
		CProfileSettings::GetInstance().UpdateProfileSettings(__PPU);
	}

#if __BANK
	CPickupManager::ShutdownLevelWidgets(); // clean up widgets
#endif
	STRVIS_SET_MARKER_TEXT("GameSkel ShutdownSession");
    m_GameSkeleton.Shutdown(SHUTDOWN_SESSION);

	StreamingInstall::ClearInstalledThisSession();
}

void CGame::HandleSignInStateChange()
{
#if GEN9_LANDING_PAGE_ENABLED
#if !__FINAL
	if(PARAM_netSignOutStartNewGame.Get())
	{
		gnetDebug1("HandleSignInStateChange :: Calling StartNewGame");
		StartNewGame(CGameLogic::GetCurrentLevelIndex());
	}
	else
#endif
	{
		gnetDebug1("HandleSignInStateChange :: Calling ReturnToLandingPageStateMachine");
		ReturnToLandingPageStateMachine(ReturnToLandingPageReason::Reason_SignInStateChange);
	}
#elif RSG_PC
	gnetDebug1("HandleSignInStateChange :: Calling ReInitStateMachine");
	ReInitStateMachine(CGameLogic::GetCurrentLevelIndex());
#else
	gnetDebug1("HandleSignInStateChange :: Calling StartNewGame");
	StartNewGame(CGameLogic::GetCurrentLevelIndex());
#endif
}

//
void CGame::RegisterGameSkeletonFunctions()
{
	//@@: range CGAME_REGISTERGAMESKELETONFUNCTIONS {
	RegisterCoreInitFunctions();
	RegisterCoreShutdownFunctions();
	
	RegisterBeforeMapLoadedInitFunctions();
	RegisterAfterMapLoadedInitFunctions();
	RegisterAfterMapLoadedShutdownFunctions();

	RegisterSessionInitFunctions();
	RegisterSessionShutdownFunctions();
	//@@: location CGAME_REGISTERGAMESKELETONFUNCTIONS_REGISTER_UPDATES
	RegisterCommonMainUpdateFunctions();
	RegisterFullMainUpdateFunctions();
	RegisterSimpleMainUpdateFunctions();

#if GEN9_LANDING_PAGE_ENABLED
	if (Gen9LandingPageEnabled())
	{
		RegisterLandingPagePreInitUpdateFunctions();
		RegisterLandingPageUpdateFunctions();

	}
#endif // GEN9_LANDING_PAGE_ENABLED
	//@@: } CGAME_REGISTERGAMESKELETONFUNCTIONS

    
#if COMMERCE_CONTAINER
	RegisterCommerceMainUpdateFunctions();
#endif
}

// make some quick macros for making the game skeleton registration functions more readable
#define REGISTER_SHUTDOWN_CALL(func, hashname)                         m_GameSkeleton.RegisterShutdownSystem(func, hashname)
#if USE_PROFILER
#define REGISTER_INIT_CALL(func, hashname)                             m_GameSkeleton.RegisterInitSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__, Profiler::CategoryColor::Loading))
#define REGISTER_UPDATE_CALL(func, hashname)                           m_GameSkeleton.RegisterUpdateSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__))
#define REGISTER_UPDATE_CALL_WITH_CATEGORY(func, hashname, category)   m_GameSkeleton.RegisterUpdateSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__, category))
#define REGISTER_UPDATE_CALL_TIMED(func, hashname)                     m_GameSkeleton.RegisterUpdateSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__), true)
#define REGISTER_UPDATE_CALL_TIMED_WITH_CATEGORY(func, hashname, category) m_GameSkeleton.RegisterUpdateSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__, category), true)
#define REGISTER_UPDATE_CALL_TIMED_BUDGETTED(func, hashname, budget)   m_GameSkeleton.RegisterUpdateSystem(func, hashname, Profiler::EventDescription::Create(#func, __FILE__, __LINE__, Profiler::CategoryColor::None, 0, budget), true, budget)
#define PUSH_UPDATE_GROUP(funcname, hash)                              m_GameSkeleton.PushUpdateGroup(atHashString(funcname,hash), Profiler::EventDescription::Create(funcname, __FILE__, __LINE__), true)
#define PUSH_UPDATE_GROUP_BUDGETTED(funcname, hash, budget, category)  m_GameSkeleton.PushUpdateGroup(atHashString(funcname,hash), Profiler::EventDescription::Create(funcname, __FILE__, __LINE__, category, 0, budget), true, budget)
#else
#define REGISTER_INIT_CALL(func, hashname)                             m_GameSkeleton.RegisterInitSystem(func, hashname)
#define REGISTER_UPDATE_CALL(func, hashname)                           m_GameSkeleton.RegisterUpdateSystem(func, hashname)
#define REGISTER_UPDATE_CALL_WITH_CATEGORY(func, hashname, category)   m_GameSkeleton.RegisterUpdateSystem(func, hashname)
#define REGISTER_UPDATE_CALL_TIMED(func, hashname)                     m_GameSkeleton.RegisterUpdateSystem(func, hashname, true)
#define REGISTER_UPDATE_CALL_TIMED_WITH_CATEGORY(func, hashname, cat)  m_GameSkeleton.RegisterUpdateSystem(func, hashname, true)
#define REGISTER_UPDATE_CALL_TIMED_BUDGETTED(func, hashname, budget)   m_GameSkeleton.RegisterUpdateSystem(func, hashname, true, budget)
#define PUSH_UPDATE_GROUP(funcname, hash)                              m_GameSkeleton.PushUpdateGroup(atHashString(funcname,hash), true)
#define PUSH_UPDATE_GROUP_BUDGETTED(funcname, hash, budget, color)     m_GameSkeleton.PushUpdateGroup(atHashString(funcname,hash), true, budget)
#endif
#define POP_UPDATE_GROUP()                                         m_GameSkeleton.PopUpdateGroup()
#define SET_INIT_TYPE(type)                                        m_GameSkeleton.SetCurrentInitType(type)
#define SET_SHUTDOWN_TYPE(type)                                    m_GameSkeleton.SetCurrentShutdownType(type)
#define SET_UPDATE_TYPE(type)                                      m_GameSkeleton.SetCurrentUpdateType(type)
#define SET_CURRENT_DEPENDENCY_LEVEL(level)                        m_GameSkeleton.SetCurrentDependencyLevel(level)

void CGame::RegisterCoreInitFunctions()
{
    // Register all functions for calling at the start of the game
    SET_INIT_TYPE(INIT_CORE);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
		REGISTER_INIT_CALL(CDebug::InitSystem,					 atHashString("InitSystem",0x4ab9c4f));
		//REGISTER_INIT_CALL(CStreaming::Init,                     atHashString("CStreaming",0x9a0892b1)); // this has to be before anything that wants to stream (*duh*) so it should be pretty close to the top of the list
		REGISTER_INIT_CALL(CShaderLib::InitTxd,					 atHashString("CShaderLib",0x99a20455));
		REGISTER_INIT_CALL(PostFX::Init,						 atHashString("PostFX",0x6e413d11));
		REGISTER_INIT_CALL(CGtaAnimManager::Init,                atHashString("CGtaAnimManager",0xf22520c7));
		REGISTER_INIT_CALL(fwClipSetManager::Init,               atHashString("fwClipSetManager",0x56872c54));
		REGISTER_INIT_CALL(fwExpressionSetManager::Init,         atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_INIT_CALL(fwFacialClipSetGroupManager::Init,    atHashString("fwFacialClipSetGroupManager",0xedcf3461));
		REGISTER_INIT_CALL(audNorthAudioEngine::Init,            atHashString("audNorthAudioEngine",0xe6d408df));
		REGISTER_INIT_CALL(CCheat::Init,                         atHashString("CCheat",0x7e5bc866));
		REGISTER_INIT_CALL(CClock::Init,                         atHashString("CClock",0x54f8bb2c));
		REGISTER_INIT_CALL(CCullZones::Init,                     atHashString("CCullZones",0xb3808319));
		REGISTER_INIT_CALL(CExpensiveProcessDistributer::Init,   atHashString("CExpensiveProcessDistributer",0x387c2f52));
		REGISTER_INIT_CALL(CGameLogic::Init,                     atHashString("CGameLogic",0xcd86c3fe));
		REGISTER_INIT_CALL(CGen9ExclusiveAssets::Init,           atHashString("CGen9ExclusiveAssets",0xaa19234e));
		//REGISTER_INIT_CALL(CGenericGameStorage::Init,            atHashString("CGenericGameStorage",0x8f569024));
		REGISTER_INIT_CALL(CutSceneManagerWrapper::Init,         atHashString("CutSceneManagerWrapper",0x9a63cd5e));
		REGISTER_INIT_CALL(CAnimSceneManager::Init,              atHashString("CAnimSceneManager",0xd2cfbc62));
#if __BANK
		REGISTER_INIT_CALL(CNodeViewer::Init,					 atHashString("CNodeViewer",0x25474b2c));
		REGISTER_INIT_CALL(CStreamGraph::Init,					 atHashString("CStreamGraph",0xa1690ea4));
#endif
		REGISTER_INIT_CALL(CPortalTracker::Init,				 atHashString("CPortalTracker",0x9413bb7));
		REGISTER_INIT_CALL(CNetwork::Init,                       atHashString("CNetwork",0xb6331929)); // dependent on CGame::PreLoadingScreensInit() (contains CLiveManager::Init())

		REGISTER_INIT_CALL(CObjectPopulationNY::Init,            atHashString("CObjectPopulationNY",0xd923f525));
		REGISTER_INIT_CALL(COcclusion::Init,                     atHashString("COcclusion",0x91ed20d7));
#if __BANK
		REGISTER_INIT_CALL(g_PickerManager.StaticInit,			 atHashString("CPicker",0xdc843ce6));
#endif
		REGISTER_INIT_CALL(CPlantMgr::Init,                      atHashString("CPlantMgr",0x33a23607));
		REGISTER_INIT_CALL(CShaderHairSort::Init,				 atHashString("CShaderHairSort",0x1caa3843));
		REGISTER_INIT_CALL(CNetworkTelemetry::Init,              atHashString("CNetworkTelemetry",0x7353c05d));
		NOTFINAL_ONLY( REGISTER_INIT_CALL(CNetworkDebugTelemetry::Init,      atHashString("CNetworkDebugTelemetry",0xadc4ae61)); )
		REGISTER_INIT_CALL(CPopulationStreamingWrapper::Init,    atHashString("CPopulationStreamingWrapper",0x5d4bc852));
		REGISTER_INIT_CALL(CRenderer::Init,                      atHashString("CRenderer",0x9eaf5465));
#if GTA_REPLAY
		REGISTER_INIT_CALL(CReplayMgr::InitSession,              atHashString("CReplayMgr",0xcaf18c95));
#endif

#if RSG_ORBIS
		REGISTER_INIT_CALL(CContentExport::Init,		         atHashString("CContentExport",0x105c6210));
		REGISTER_INIT_CALL(CContentSearch::Init,		         atHashString("CContentSearch",0x36ac773f));
		REGISTER_INIT_CALL(CContentDelete::Init,		         atHashString("CContentDelete",0xe223c36f));
#endif	//RSG_ORBIS

#if VIDEO_RECORDING_ENABLED
		REGISTER_INIT_CALL(VideoRecording::Init,				 atHashString("VideoRecording",0x93ae9b6e));
#endif	//VIDEO_RECORDING_ENABLED

#if GREATEST_MOMENTS_ENABLED
		REGISTER_INIT_CALL(fwGreatestMoment::Init,				atHashString("fwGreatestMoments",0xca760256));
#endif // GREATEST_MOMENTS_ENABLED

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		REGISTER_INIT_CALL(VideoPlayback::Init,						atHashString("VideoPlayback",0x52ae9cff));
		REGISTER_INIT_CALL(VideoPlaybackThumbnailManager::Init,		atHashString("VideoPlaybackThumbnailManager",0xcff8e8f8));
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#if (SECTOR_TOOLS_EXPORT)
        REGISTER_INIT_CALL(CSectorTools::Init,                   atHashString("CSectorTools",0x903d31ad));
#endif // SECTOR_TOOLS_EXPORT
		REGISTER_INIT_CALL(CSaveMigration::Init,				 atHashString("CSaveMigration", 0x8930D2F7));
		REGISTER_INIT_CALL(CStatsMgr::Init,                      atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_INIT_CALL(CFrontendStatsMgr::Init,				 atHashString("CFrontendStatsMgr",0xfc705e70));
		REGISTER_INIT_CALL(CControllerLabelMgr::Init,			 atHashString("CControllerLabelMgr",0x67fbdaac));
		REGISTER_INIT_CALL(CStuntJumpManager::Init,              atHashString("CStuntJumpManager",0xebc4e679));
		REGISTER_INIT_CALL(CTheScripts::Init,					 atHashString("CTheScripts",0x249760f7));
		REGISTER_INIT_CALL(CScriptAreas::Init,					 atHashString("CScriptAreas",0xf94a04df));
		REGISTER_INIT_CALL(CScriptDebug::Init,					 atHashString("CScriptDebug",0x481538de));
		REGISTER_INIT_CALL(CScriptHud::Init,					 atHashString("CScriptHud",0xe2156445));
		REGISTER_INIT_CALL(CScriptPedAIBlips::Init,				 atHashString("CScriptPedAIBlips",0xa62f89da));
		REGISTER_INIT_CALL(CTrain::Init,                         atHashString("CTrain",0x36860703));
		REGISTER_INIT_CALL(CWaypointRecording::Init,             atHashString("CWaypointRecording",0xe7de7e92));
#if !GPU_DAMAGE_WRITE_ENABLED
		REGISTER_INIT_CALL(CVehicleDeformation::TexCacheInit,    atHashString("CVehicleDeformation",0x808725f0));
#endif
		REGISTER_INIT_CALL(CVehicleRecordingMgr::Init,           atHashString("CVehicleRecordingMgr",0x8d873b09));
		REGISTER_INIT_CALL(CTaskClassInfoManager::Init,          atHashString("CTaskClassInfoManager",0x882ca002));
		REGISTER_INIT_CALL(CTuningManager::Init,                 atHashString("CTuningManager",0xa8b63f65));
		REGISTER_INIT_CALL(CAppDataMgr::Init,					 atHashString("CAppDataMgr",0xbe3c8dde));
		REGISTER_INIT_CALL(CNetRespawnMgr::Init,				 atHashString("CNetRespawnMgr",0x8b1bc3e4));
		REGISTER_INIT_CALL(CAgitatedManager::Init,               atHashString("CAgitatedManager",0x6a8a7ce2));
		REGISTER_INIT_CALL(CScenarioActionManager::Init,		 atHashString("CScenarioActionManager",0x855bdf6c));
		REGISTER_INIT_CALL(CVehiclePopulation::Init,			 atHashString("CVehiclePopulation",0xab533b92));
		REGISTER_INIT_CALL(CPostScan::Init,						 atHashString("CPostScan",0x5eee381));
		REGISTER_INIT_CALL(CExtraMetadataMgr::ClassInit,		 atHashString("CExtraMetadataMgr",0x781255c1));
		REGISTER_INIT_CALL(BackgroundScripts::InitClass,		 atHashString("BackgroundScripts",0x6c7320d3));
		REGISTER_INIT_CALL(CIplCullBox::Initialise,              atHashString("CIplCullBox",0xe4821bf9));
		REGISTER_INIT_CALL(perfClearingHouse::Initialize,		 atHashString("perfClearingHouse",0x9fdfa0ec));
#if GEN9_STANDALONE_ENABLED
		REGISTER_INIT_CALL(CLandingPage::Init, atHashString("CLandingPage", 0x165B8EED));
		// TODO_UI CAREER_BUILDER: Move this to Windfall flow. It will not be used in subsequent runs of the game
		REGISTER_INIT_CALL(CCriminalCareerServiceWrapper::Init,  atHashString("CCriminalCareerServiceWrapper", 0x63B909C1));
		REGISTER_INIT_CALL(CUserEntitlementServiceWrapper::Init, atHashString("CUserEntitlementServiceWrapper", 0x100B3155));
#endif // GEN9_STANDALONE_ENABLED
	}

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
		//REGISTER_INIT_CALL(CText::Init,			atHashString("CText",0xd82b15de));
        REGISTER_INIT_CALL(CScene::Init,			atHashString("CScene",0xc5d4866b));             // dependent on CreateFactories(), CStreaming::Init(), CRenderer::Init()
        REGISTER_INIT_CALL(MeshBlendManager::Init,	atHashString("MeshBlendManager",0xe78441ef));             // dependent on CreateFactories(), CStreaming::Init(), CRenderer::Init()
        REGISTER_INIT_CALL(CVisualEffects::Init,	atHashString("CVisualEffects",0xbc186f00));     // dependent on some rendering setup (not exactly sure what)
        REGISTER_INIT_CALL(ViewportSystemInit,		atHashString("ViewportSystemInit",0x96629ccb)); // dependent on CScene::Init()
		REGISTER_INIT_CALL(CPhoneMgr::Init,			atHashString("CPhoneMgr",0x897d8554));          // dependent on CRenderer::Init() and ViewportSystemInit //Create the phone screen render target when game start for referencing
		REGISTER_INIT_CALL(CPhotoManager::Init,		atHashString("CPhotoManager",0xaec90076));
#if VOLUME_SUPPORT
        REGISTER_INIT_CALL(CVolumeManager::Init,    atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
		REGISTER_INIT_CALL(CLODLightManager::Init,  atHashString("CLODLightManager",0xd203e710));
		REGISTER_INIT_CALL(CLODLights::Init,		atHashString("CLODLights",0xbb31917));
		REGISTER_INIT_CALL(AmbientLights::Init,		atHashString("AmbientLights",0x17304d82));
		REGISTER_INIT_CALL(CGps::Init,              atHashString("CGps",0x82f2dd62));
		REGISTER_INIT_CALL(CPedAILodManager::Init,  atHashString("CPedAILodManager",0x77366627));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_3);
    {
		REGISTER_INIT_CALL(CPauseMenu::Init,       atHashString("CPauseMenu",0xf0f5a94d));	// pause menu init
		REGISTER_INIT_CALL(cStoreScreenMgr::Init,  atHashString("cStoreScreenMgr",0xd9d0e49d));	// Store menu init
#if GTA_REPLAY
		REGISTER_INIT_CALL(CVideoEditorUi::Init,  atHashString("CVideoEditorUi",0xa05ca7da));	// replay editor ui init
		REGISTER_INIT_CALL(WatermarkRenderer::Init,  atHashString("WatermarkRenderer",0xD81097FD));	// replay editor WatermarkRenderer
#endif // GTA_REPLAY
#if RSG_PC
		REGISTER_INIT_CALL(CMousePointer::Init,  atHashString("CMousePointer",0xa3200712));	// mousepointer movie
		REGISTER_INIT_CALL(CTextInputBox::Init,  atHashString("CTextInputBox",0xFAAB3737));
#endif // RSG_PC
#if TAMPERACTIONS_ENABLED
		REGISTER_INIT_CALL(TamperActions::Init,  atHashString("TamperActions",0x602ee6e2));
#endif // TAMPERACTIONS_ENABLED

		REGISTER_INIT_CALL(CMiniMap::Init,         atHashString("CMiniMap",0xb3bbe4a1));	// depends on some 360 render target setup
		REGISTER_INIT_CALL(CNewHud::Init,          atHashString("CNewHud",0x7b6a9afe));		// depends on some 360 render target setup
		REGISTER_INIT_CALL(CHudMarkerManager::Init,atHashString("CHudMarkerManager", 0x4BFBFA2C));
		REGISTER_INIT_CALL(CWarningScreen::Init,   atHashString("WarningScreen",0x37153f31));
		REGISTER_INIT_CALL(CBusySpinner::Init,     atHashString("CBusySpinner",0x8c989e94));
		REGISTER_INIT_CALL(camManager::Init,       atHashString("camManager",0xa571eef5));		// dependent on CScene::Init() and ViewportSystemInit()
		REGISTER_INIT_CALL(CDLCScript::Init,	   atHashString("CDLCScript",0x2527583e));


		// CALLS FRONTENDXMLMGR.READ()!
		// So anything that needs those callbacks needs to be registered before this!
		REGISTER_INIT_CALL(CGameStreamMgr::Init,   atHashString("CGameStreamMgr",0xcbe4ae9c));

#if !__FINAL
		REGISTER_INIT_CALL(CDebug::Init,           atHashString("CDebug",0x68a317a2));			// dependant on Cscene:Init()
		REGISTER_INIT_CALL(CDebug::InitProfiler,   atHashString("CDebugProfiler",0xccccf9ce));
#endif // !__FINAL
		FINAL_DISPLAY_BAR_ONLY(REGISTER_INIT_CALL(CDebugBar::Init,   atHashString("CDebugBar",0xba6fe146)));

#if COLLECT_END_USER_BENCHMARKS
		REGISTER_INIT_CALL(EndUserBenchmarks::Init, atHashString("EndUserBenchmarks",0x4d167300));
#endif	//COLLECT_END_USER_BENCHMARKS

#if __BANK
		REGISTER_INIT_CALL(CMarketingTools::Init, atHashString("Marketing Tools",0xb0a609cd));
#endif // __BANK
		REGISTER_INIT_CALL(fwMetaDataStore::Init, atHashString("fwMetaDataStore",0x280679d4));

#if GTA_REPLAY
#if __BANK
		REGISTER_INIT_CALL(CDebugVideoEditorScreen::Init,   atHashString("CDebugVideoEditorScreen",0x3556d2e2));
#endif // __BANK
		REGISTER_INIT_CALL(CReplayCoordinator::Init,    atHashString("CReplayCoordinator",0x7ef069c3));
		REGISTER_INIT_CALL(CVideoEditorInterface::Init, atHashString("CVideoEditorInterface",0xc90b32b2)); 
#endif //GTA_REPLAY

		REGISTER_INIT_CALL(UI3DDrawManager::ClassInit,   atHashString("UI3DDrawManager",0x8fece306));
#if USE_RAGESEC
#if __FINAL
		REGISTER_INIT_CALL(rageSecEngine::Init,   atHashString("fwClothMeshing",0x4F916676));
		REGISTER_INIT_CALL(rageSecGamePluginManager::Init,   atHashString("CCreditsText",0x71CB65D7));
#else // __FINAL
		REGISTER_INIT_CALL(rageSecEngine::Init,   atHashString("rageSecEngine",0x73aa6f9e));
		REGISTER_INIT_CALL(rageSecGamePluginManager::Init,   atHashString("rageSecGamePluginManager",0xA9C688FA));
#endif // __FINAL
#endif // USE_RAGESEC
    }
}

void CGame::RegisterCoreShutdownFunctions()
{
    // Register all shutdown functions for calling at the end of the game
    SET_SHUTDOWN_TYPE(SHUTDOWN_CORE);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
		REGISTER_SHUTDOWN_CALL(CExtraContentWrapper::ShutdownStart,  atHashString("CExtraContentWrapper::ShutdownStart",0x4220ae24));
		REGISTER_SHUTDOWN_CALL(BackgroundScripts::ShutdownClass,	 atHashString("BackgroundScripts",0x6c7320d3));
		REGISTER_SHUTDOWN_CALL(CExtraMetadataMgr::ClassShutdown,	 atHashString("CExtraMetadataMgr",0x781255c1));
		REGISTER_SHUTDOWN_CALL(CPostScan::Shutdown,					 atHashString("CPostScan",0x5eee381));
		REGISTER_SHUTDOWN_CALL(CVehiclePopulation::Shutdown,		 atHashString("CVehiclePopulation",0xab533b92));
#if (SECTOR_TOOLS_EXPORT)
        REGISTER_SHUTDOWN_CALL(CSectorTools::Shutdown,               atHashString("CSectorTools",0x903d31ad));
#endif
		REGISTER_SHUTDOWN_CALL(fwClipSetManager::Shutdown,           atHashString("fwClipSetManager",0x56872c54));
		REGISTER_SHUTDOWN_CALL(fwExpressionSetManager::Shutdown,     atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_SHUTDOWN_CALL(fwFacialClipSetGroupManager::Shutdown,atHashString("fwFacialClipSetGroupManager",0xedcf3461));
		REGISTER_SHUTDOWN_CALL(CGen9ExclusiveAssets::Shutdown,       atHashString("CGen9ExclusiveAssets",0xaa19234e));
		REGISTER_SHUTDOWN_CALL(CAnimSceneManager::Shutdown,          atHashString("CAnimSceneManager",0xd2cfbc62));
        REGISTER_SHUTDOWN_CALL(camManager::Shutdown,                 atHashString("camManager",0xa571eef5));
        REGISTER_SHUTDOWN_CALL(CutSceneManagerWrapper::Shutdown,     atHashString("CutSceneManager",0xae7573c1));
        REGISTER_SHUTDOWN_CALL(CCredits::Shutdown,                   atHashString("CCredits",0x2ee4e735));
        REGISTER_SHUTDOWN_CALL(CExplosionManager::Shutdown,          atHashString("CExplosionManager",0x1715fd90));
        REGISTER_SHUTDOWN_CALL(CGenericGameStorage::Shutdown,        atHashString("CGenericGameStorage",0x8f569024));
        REGISTER_SHUTDOWN_CALL(CLoadingScreens::Shutdown,            atHashString("CLoadingScreens",0x21b66612));
        REGISTER_SHUTDOWN_CALL(CMessages::Shutdown,                  atHashString("CMessages",0xf0e7fef9));
        REGISTER_SHUTDOWN_CALL(CNetwork::Shutdown,                   atHashString("CNetwork",0xb6331929));
#if __BANK
		REGISTER_SHUTDOWN_CALL(CNodeViewer::Shutdown,                atHashString("CNodeViewer",0x25474b2c));
		REGISTER_SHUTDOWN_CALL(CStreamGraph::Shutdown,               atHashString("CStreamGraph",0xa1690ea4));
#endif
		REGISTER_SHUTDOWN_CALL(CPortalTracker::ShutDown,			 atHashString("CPortalTracker",0x9413bb7));
		REGISTER_SHUTDOWN_CALL(CMovieMeshManager::Shutdown,			 atHashString("CMovieMeshManager",0xe607a023));
		REGISTER_SHUTDOWN_CALL(CObjectPopulationNY::Shutdown,        atHashString("CObjectPopulationNY",0xd923f525));
		REGISTER_SHUTDOWN_CALL(COcclusion::Shutdown,                 atHashString("COcclusion",0x91ed20d7));
		REGISTER_SHUTDOWN_CALL(CPhoneMgr::Shutdown,                  atHashString("CPhoneMgr",0x897d8554));
		REGISTER_SHUTDOWN_CALL(CPhotoManager::Shutdown,				 atHashString("CPhotoManager",0xaec90076));
		REGISTER_SHUTDOWN_CALL(CPlantMgr::Shutdown,                  atHashString("CPlantMgr",0x33a23607));
		REGISTER_SHUTDOWN_CALL(CNetworkTelemetry::Shutdown,          atHashString("CNetworkTelemetry",0x7353c05d));
		NOTFINAL_ONLY( REGISTER_SHUTDOWN_CALL(CNetworkDebugTelemetry::Shutdown,      atHashString("CNetworkDebugTelemetry",0xadc4ae61)); )
		REGISTER_SHUTDOWN_CALL(CSprite2d::Shutdown,                  atHashString("CSprite2d",0x37bfe263));
		REGISTER_SHUTDOWN_CALL(CSaveMigration::Shutdown,			 atHashString("CSaveMigration", 0x8930D2F7));
		REGISTER_SHUTDOWN_CALL(CStatsMgr::Shutdown,                  atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_SHUTDOWN_CALL(CFrontendStatsMgr::Shutdown,          atHashString("CFrontendStatsMgr",0xfc705e70));
		REGISTER_SHUTDOWN_CALL(CStuntJumpManager::Shutdown,          atHashString("CStuntJumpManager",0xebc4e679));
		REGISTER_SHUTDOWN_CALL(CTheScripts::Shutdown,                atHashString("CTheScripts",0x249760f7));
		REGISTER_SHUTDOWN_CALL(CScriptAreas::Shutdown,				 atHashString("CScriptAreas",0xf94a04df));
		REGISTER_SHUTDOWN_CALL(CScriptDebug::Shutdown,				 atHashString("CScriptDebug",0x481538de));
		REGISTER_SHUTDOWN_CALL(CScriptHud::Shutdown,				 atHashString("CScriptHud",0xe2156445));
		REGISTER_SHUTDOWN_CALL(CScriptPedAIBlips::Shutdown,			 atHashString("CScriptPedAIBlips",0xa62f89da));


#if !GPU_DAMAGE_WRITE_ENABLED
        REGISTER_SHUTDOWN_CALL(CVehicleDeformation::TexCacheShutdown, atHashString("CVehicleDeformation",0x808725f0));
#endif
        REGISTER_SHUTDOWN_CALL(CVisualEffects::Shutdown,              atHashString("CVisualEffects",0xbc186f00));

#if VIDEO_RECORDING_ENABLED
		REGISTER_SHUTDOWN_CALL(VideoRecording::Shutdown,              atHashString("VideoRecording",0x93ae9b6e));
#endif	//VIDEO_RECORDING_ENABLED

#if GREATEST_MOMENTS_ENABLED
		REGISTER_SHUTDOWN_CALL(fwGreatestMoment::Shutdown,			atHashString("fwGreatestMoments",0xca760256));
#endif // GREATEST_MOMENTS_ENABLED

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		REGISTER_SHUTDOWN_CALL(VideoPlayback::Shutdown,					atHashString("VideoPlayback",0x52ae9cff));
		REGISTER_SHUTDOWN_CALL(VideoPlaybackThumbnailManager::Shutdown,	atHashString("VideoPlaybackThumbnailManager",0xcff8e8f8));
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#if RSG_ORBIS
		REGISTER_SHUTDOWN_CALL(CContentExport::Shutdown,			atHashString("CContentExport",0x105c6210));
		REGISTER_SHUTDOWN_CALL(CContentSearch::Shutdown,			atHashString("CContentSearch",0x36ac773f));
		REGISTER_SHUTDOWN_CALL(CContentDelete::Shutdown,			atHashString("CContentDelete",0xe223c36f));
#endif	//RSG_ORBIS
#if GTA_REPLAY
		REGISTER_SHUTDOWN_CALL(CReplayMgr::ShutdownSession,			atHashString("CReplayMgr",0xcaf18c95));
#endif

#if VOLUME_SUPPORT
        REGISTER_SHUTDOWN_CALL(CVolumeManager::Shutdown,            atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
		REGISTER_SHUTDOWN_CALL(ViewportSystemShutdown,              atHashString("ViewportSystemShutdown",0xb516267));
		REGISTER_SHUTDOWN_CALL(CRenderer::Shutdown,                 atHashString("CRenderer",0x9eaf5465));
		REGISTER_SHUTDOWN_CALL(CBusySpinner::Shutdown,              atHashString("CBusySpinner",0x8c989e94));
		REGISTER_SHUTDOWN_CALL(CNewHud::Shutdown,                   atHashString("CNewHud",0x7b6a9afe));
		REGISTER_SHUTDOWN_CALL(CMiniMap::Shutdown,					atHashString("CMiniMap",0xb3bbe4a1));
		REGISTER_SHUTDOWN_CALL(CHudMarkerManager::Shutdown,			atHashString("CHudMarkerManager", 0x4BFBFA2C));
		REGISTER_SHUTDOWN_CALL(CGameStreamMgr::Shutdown,            atHashString("CGameStreamMgr",0xcbe4ae9c));
#if RSG_PC
		REGISTER_SHUTDOWN_CALL(CMultiplayerChat::Shutdown,			atHashString("CMultiplayerChat",0xb8568feb));
		REGISTER_SHUTDOWN_CALL(CMousePointer::Shutdown,				atHashString("CMousePointer",0xa3200712));
		REGISTER_SHUTDOWN_CALL(CTextInputBox::Shutdown,				atHashString("CTextInputBox",0xFAAB3737));
#endif // #if RSG_PC
#if GTA_REPLAY
		REGISTER_SHUTDOWN_CALL(CVideoEditorUi::Shutdown,            atHashString("CVideoEditorUi",0xa05ca7da));
		REGISTER_SHUTDOWN_CALL(WatermarkRenderer::Shutdown,         atHashString("WatermarkRenderer",0xD81097FD));
#endif
		REGISTER_SHUTDOWN_CALL(CPauseMenu::Shutdown,				atHashString("CPauseMenu",0xf0f5a94d));
		REGISTER_SHUTDOWN_CALL(cStoreScreenMgr::Shutdown,           atHashString("cStoreScreenMgr",0xd9d0e49d));
		REGISTER_SHUTDOWN_CALL(CVehicleRecordingMgr::Shutdown,      atHashString("CVehicleRecordingMgr",0x8d873b09));
		REGISTER_SHUTDOWN_CALL(CWaypointRecording::Shutdown,        atHashString("CWaypointRecording",0xe7de7e92));
		REGISTER_SHUTDOWN_CALL(CTaskClassInfoManager::Shutdown,     atHashString("CTaskClassInfoManager",0x882ca002));
		REGISTER_SHUTDOWN_CALL(CTuningManager::Shutdown,            atHashString("CTuningManager",0xa8b63f65));
		REGISTER_SHUTDOWN_CALL(CAppDataMgr::Shutdown,				atHashString("CAppDataMgr",0xbe3c8dde));
		REGISTER_SHUTDOWN_CALL(CNetRespawnMgr::Shutdown,			atHashString("CNetRespawnMgr",0x8b1bc3e4));
		REGISTER_SHUTDOWN_CALL(CAgitatedManager::Shutdown,          atHashString("CAgitatedManager",0x6a8a7ce2));
		REGISTER_SHUTDOWN_CALL(CScenarioActionManager::Shutdown,	atHashString("CScenarioActionManager",0x855bdf6c));

#if GTA_REPLAY
#if __BANK
		REGISTER_SHUTDOWN_CALL(CDebugVideoEditorScreen::Shutdown,	atHashString("CDebugVideoEditorScreen",0x3556d2e2));
#endif 
		REGISTER_SHUTDOWN_CALL(CReplayCoordinator::Shutdown,		atHashString("CReplayCoordinator",0x7ef069c3));
		REGISTER_SHUTDOWN_CALL(CVideoEditorInterface::Shutdown,		atHashString("CVideoEditorInterface",0xc90b32b2));
#endif //GTA_REPLAY

		REGISTER_SHUTDOWN_CALL(UI3DDrawManager::ClassShutdown,		atHashString("UI3DDrawManager",0x8fece306));

		REGISTER_SHUTDOWN_CALL(audNorthAudioEngine::Shutdown,       atHashString("audNorthAudioEngine",0xe6d408df));

#if RSG_ENTITLEMENT_ENABLED
		REGISTER_SHUTDOWN_CALL(CEntitlementManager::Shutdown,       atHashString("CEntitlementManager",0xeb1ae026));
#endif
#if GEN9_STANDALONE_ENABLED
		REGISTER_SHUTDOWN_CALL(CLandingPage::Init, atHashString("CLandingPage", 0x165B8EED));
		REGISTER_SHUTDOWN_CALL(CCriminalCareerServiceWrapper::Shutdown, atHashString("CCriminalCareerServiceWrapper",0x63B909C1));
		REGISTER_SHUTDOWN_CALL(CUserEntitlementServiceWrapper::Shutdown, atHashString("CUserEntitlementServiceWrapper",0x100B3155));
#endif // GEN9_STANDALONE_ENABLED
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
		REGISTER_SHUTDOWN_CALL(CGameWorldHeightMap::Shutdown,		 atHashString("CGameWorldHeightMap",0xa330dcde));
		REGISTER_SHUTDOWN_CALL(CGameWorldWaterHeight::Shutdown,		 atHashString("CGameWorldWaterHeight",0xf813e523));

		REGISTER_SHUTDOWN_CALL(CText::Shutdown,						 atHashString("CText",0xd82b15de));
        REGISTER_SHUTDOWN_CALL(CScene::Shutdown,                     atHashString("CScene",0xc5d4866b));
        REGISTER_SHUTDOWN_CALL(MeshBlendManager::Shutdown,           atHashString("MeshBlendManager",0xe78441ef));
		REGISTER_SHUTDOWN_CALL(CStreamingRequestList::Shutdown,      atHashString("CStreamingRequestList",0xe9b96fae));

		REGISTER_SHUTDOWN_CALL(CGps::Shutdown,						 atHashString("CGps",0x82f2dd62));
		REGISTER_SHUTDOWN_CALL(CPedAILodManager::Shutdown,           atHashString("CPedAILodManager",0x77366627));

		//REGISTER_SHUTDOWN_CALL(CActionManager::Shutdown,            atHashString("CActionManager",0xf3b5d7d9));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_3);
    {
#if GTA_REPLAY
        REGISTER_SHUTDOWN_CALL(CTextTemplate::CoreShutdown,          atHashString("CTextTemplate",0x4FFAC1F8));
#endif
		REGISTER_SHUTDOWN_CALL(CScaleformMgr::Shutdown,              atHashString("CScaleformMgr",0xd3fb2763));

#if COLLECT_END_USER_BENCHMARKS
		REGISTER_SHUTDOWN_CALL(EndUserBenchmarks::Shutdown,          atHashString("EndUserBenchmarks",0x4d167300));
#endif	//COLLECT_END_USER_BENCHMARKS

#if USE_RAGESEC
#if __FINAL
		REGISTER_SHUTDOWN_CALL(rageSecEngine::Shutdown,              atHashString("fwClothMeshing",0x4F916676));
#else
		REGISTER_SHUTDOWN_CALL(rageSecEngine::Shutdown,              atHashString("rageSecEngine",0x73aa6f9e));
#endif
#endif
#if !__FINAL
		REGISTER_SHUTDOWN_CALL(CDebug::Shutdown,                     atHashString("CDebug",0x68a317a2));
#endif
		REGISTER_SHUTDOWN_CALL(fwMetaDataStore::ShutdownSystem,      atHashString("fwMetaDataStore",0x280679d4));
        REGISTER_SHUTDOWN_CALL(CGtaAnimManager::Shutdown,            atHashString("CGtaAnimManager",0xf22520c7));
        REGISTER_SHUTDOWN_CALL(fwDwdStoreWrapper::Shutdown,          atHashString("fwDwdStoreWrapper",0x585ff839));
		REGISTER_SHUTDOWN_CALL(fwDrawableStoreWrapper::Shutdown,	 atHashString("fwDrawawableStoreWrapper",0xdd32fbf7));
		REGISTER_SHUTDOWN_CALL(fwFragmentStoreWrapper::Shutdown,	 atHashString("fwFragmentStoreWrapper",0x6b086952));
        REGISTER_SHUTDOWN_CALL(fwTxdStoreWrapper::Shutdown,			 atHashString("fwTxdStore",0xf9c93f03)); // Don't shutdown anything that uses textures after g_TxdStore
		REGISTER_SHUTDOWN_CALL(fwClothStoreWrapper::Shutdown,		 atHashString("fwClothStore",0x9546ef55));
		REGISTER_SHUTDOWN_CALL(CExtraContentWrapper::Shutdown,       atHashString("CExtraContentWrapper::Shutdown",0x8bc3edb));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_4);
    {
        REGISTER_SHUTDOWN_CALL(CStreaming::Shutdown,                 atHashString("CStreaming",0x9a0892b1));
    }
}

void CGame::RegisterBeforeMapLoadedInitFunctions()
{
    SET_INIT_TYPE(INIT_BEFORE_MAP_LOADED);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
#if !__FINAL
		REGISTER_INIT_CALL(CBlockView::Init,              atHashString("CBlockView",0xd4ba82c9));
#endif
        // This vehicle data must be set up before we parse vehicles.ide in LoadMap
        REGISTER_INIT_CALL(CGtaAnimManager::Init,         atHashString("CGtaAnimManager",0xf22520c7));
        REGISTER_INIT_CALL(CGameWorld::Init,              atHashString("CGameWorld",0xe4c9fbc));
		REGISTER_INIT_CALL(CGarages::Init,                atHashString("CGarages",0x3c8124b5));
		REGISTER_INIT_CALL(CHandlingDataMgr::Init,        atHashString("CHandlingDataMgr",0x744e0c2));
        REGISTER_INIT_CALL(CInstanceStoreWrapper::Init,   atHashString("INSTANCESTORE",0xa64a7aa5));
		REGISTER_INIT_CALL(CMapAreas::Init,				  atHashString("CMapAreas",0x6bdb3a4c));
        REGISTER_INIT_CALL(CMapTypesStoreWrapper::Init,   atHashString("fwMapTypesStore",0xab221220));
        REGISTER_INIT_CALL(CModelInfo::Init,              atHashString("CModelInfo",0x683c1964));
        REGISTER_INIT_CALL(CPatrolRoutes::Init,           atHashString("CPatrolRoutes",0x70a66299));
		REGISTER_INIT_CALL(CPopZones::Init,               atHashString("CPopZones",0xc7015997));
		REGISTER_INIT_CALL(CMapZoneManager::Init,         atHashString("CMapZoneManager",0xebfeb4c2));
		REGISTER_INIT_CALL(CPhysics::Init,                atHashString("CPhysics",0x782937ba));
        REGISTER_INIT_CALL(CTrain::Init,                  atHashString("CTrain",0x36860703));
        REGISTER_INIT_CALL(Water::Init,                   atHashString("Water",0xcadd9676));
        REGISTER_INIT_CALL(COcclusion::Init,              atHashString("COcclusion",0x91ed20d7));
		REGISTER_INIT_CALL(fwClipSetManager::Init,        atHashString("fwClipSetManager",0x56872c54));
		REGISTER_INIT_CALL(fwExpressionSetManager::Init,  atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_INIT_CALL(fwFacialClipSetGroupManager::Init, atHashString("fwFacialClipSetGroupManager",0xedcf3461));

		REGISTER_INIT_CALL(CTVPlaylistManager::Init,	  atHashString("CTVPlaylistManager",0x62a4aedc));

		REGISTER_INIT_CALL(CAmbientModelSetManager::InitClass, atHashString("CAmbientModelSetManager",0x1f713cf3));
		REGISTER_INIT_CALL(CLadderMetadataManager::Init,       atHashString("CLadderMetadataManager",0x5bb25da));
		REGISTER_INIT_CALL(CScenarioManager::Init,             atHashString("CScenarioManager",0x7731e590));
		REGISTER_INIT_CALL(CConditionalAnimManager::Init,      atHashString("CConditionalAnimManager",0x140aada5));
		REGISTER_INIT_CALL(CVehicleMetadataMgr::Init,          atHashString("CVehicleMetadataMgr",0xc2f04b3a));
        //REGISTER_INIT_CALL(CVehicleRecordingMgr::Init,         atHashString("CVehicleRecordingMgr",0x8d873b09)); // this needs to be done before CStreaming since at that point recording files will be registered
        REGISTER_INIT_CALL(CRandomEventManager::Init,          atHashString("CRandomEventManager",0xda84b2ea));
		REGISTER_INIT_CALL(CCrimeInformationManager::Init,     atHashString("CCrimeInformationManager",0xb29ed180));
		REGISTER_INIT_CALL(CWitnessInformationManager::Init,   atHashString("CWitnessInformationManager",0xa37fd148));

		REGISTER_INIT_CALL(CPortal::Init,                     atHashString("CPortal",0x7840472b));
		REGISTER_INIT_CALL(LightEntityMgr::Init,		      atHashString("LightEntityMgr",0x5d795831));
		REGISTER_INIT_CALL(CAnimBlackboard::Init,					atHashString("AnimBlackboard",0xbc11d4e6));

		REGISTER_INIT_CALL(CPathServer::InitBeforeMapLoaded,	atHashString("CPathServer::InitBeforeMapLoaded",0xbc292625));
    }
}

void CGame::RegisterAfterMapLoadedInitFunctions()
{
    SET_INIT_TYPE(INIT_AFTER_MAP_LOADED);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
        REGISTER_INIT_CALL(audNorthAudioEngine::Init,         atHashString("audNorthAudioEngine",0xe6d408df));
        REGISTER_INIT_CALL(CClock::Init,                      atHashString("CClock",0x54f8bb2c));
        REGISTER_INIT_CALL(CEventDataManager::Init,           atHashString("CEventDataManager",0xf3bcaf9d));
		REGISTER_INIT_CALL(CHandlingDataMgr::Init,			  atHashString("CHandlingDataMgr",0x744e0c2));
		REGISTER_INIT_CALL(CMapAreas::Init,				      atHashString("CMapAreas",0x6bdb3a4c));
        REGISTER_INIT_CALL(CPed::Init,                        atHashString("CPed",0x94821659));
        REGISTER_INIT_CALL(CPopCycle::Init,                   atHashString("CPopCycle",0xd23b1168));
		REGISTER_INIT_CALL(CPopZones::Init,                   atHashString("CPopZones",0xc7015997));
		REGISTER_INIT_CALL(CMapZoneManager::Init,             atHashString("CMapZoneManager",0xebfeb4c2));
		REGISTER_INIT_CALL(CPopulationStreamingWrapper::Init,	atHashString("CPopulationStreamingWrapper",0x5d4bc852));
/*#if NA_RADIO_ENABLED
		REGISTER_INIT_CALL(CRadioHud::Init,                   atHashString("CRadioHud",0xc9106614));
#endif*/
        REGISTER_INIT_CALL(CStatsMgr::Init,                   atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_INIT_CALL(CStreamingRequestList::Init,		  atHashString("CStreamingRequestList",0xe9b96fae));
        REGISTER_INIT_CALL(CTask::Init,                       atHashString("CTask",0xdffc5283));
        REGISTER_INIT_CALL(CTrain::Init,                      atHashString("CTrain",0x36860703));
		REGISTER_INIT_CALL(CVehicleModelInfo::InitClass,      atHashString("CVehicleModelInfo",0xcc6d84e1));
		REGISTER_INIT_CALL(CVehiclePopulation::Init,		  atHashString("CVehiclePopulation",0xab533b92));
		REGISTER_INIT_CALL(Water::Init,						  atHashString("Water",0xcadd9676));
		REGISTER_INIT_CALL(CGameWorldHeightMap::Init,		  atHashString("CGameWorldHeightMap",0xa330dcde));
		REGISTER_INIT_CALL(CGameWorldWaterHeight::Init,		  atHashString("CGameWorldWaterHeight",0xf813e523));
		REGISTER_INIT_CALL(CAmbientAnimationManager::Init,    atHashString("CAmbientAnimationManager",0x7df9e5f9));
		REGISTER_INIT_CALL(CAmbientAudioManager::Init,        atHashString("CAmbientAudioManager",0xe108ecd9));
		REGISTER_INIT_CALL(CNetwork::Init,                    atHashString("CNetwork",0xb6331929));
		REGISTER_INIT_CALL(fwClipSetManager::Init,            atHashString("fwClipSetManager",0x56872c54));
		REGISTER_INIT_CALL(fwAnimDirector::InitStatic,		  atHashString("fwAnimDirector",0xca5ed810));
		REGISTER_INIT_CALL(fwExpressionSetManager::Init,      atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_INIT_CALL(fwFacialClipSetGroupManager::Init, atHashString("fwFacialClipSetGroupManager",0xedcf3461));
		REGISTER_INIT_CALL(CThePopMultiplierAreas::Init,	  atHashString("CThePopMultiplierAreas",0x21e944a0));
		REGISTER_INIT_CALL(MeshBlendManager::Init,			  atHashString("MeshBlendManager",0xe78441ef));

#if NORTH_CLOTHS
	// Read in the bendable plants data bendableplants.dat
        REGISTER_INIT_CALL(CPlantTuning::Init, atHashString("CPlantTuning",0xbc68c896));
#endif

        REGISTER_INIT_CALL(CProceduralInfoWrapper::Init,      atHashString("CProceduralInfo",0x30ebb91d));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
        REGISTER_INIT_CALL(CVisualEffects::Init,              atHashString("CVisualEffects",0xbc186f00));       // dependent on CClock
#if VOLUME_SUPPORT
        REGISTER_INIT_CALL(CVolumeManager::Init,              atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
		REGISTER_INIT_CALL(CPedModelInfo::InitClass,          atHashString("CPedModelInfo",0x74599d94));
    }
}

void CGame::RegisterAfterMapLoadedShutdownFunctions()
{
    SET_SHUTDOWN_TYPE(SHUTDOWN_WITH_MAP_LOADED);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
	{
		REGISTER_SHUTDOWN_CALL(CDLCScript::Shutdown,						atHashString("CDLCScript",0x2527583e));
		REGISTER_SHUTDOWN_CALL(CStatsMgr::Shutdown,						atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_SHUTDOWN_CALL(CNetwork::Shutdown,						atHashString("CNetwork",0xb6331929));
        REGISTER_SHUTDOWN_CALL(CPhysics::Shutdown,                      atHashString("CPhysics",0x782937ba));

		REGISTER_SHUTDOWN_CALL(fwClipSetManager::Shutdown,              atHashString("fwClipSetManager",0x56872c54));
		REGISTER_SHUTDOWN_CALL(fwAnimDirector::ShutdownStatic,			atHashString("fwAnimDirector",0xca5ed810));
        REGISTER_SHUTDOWN_CALL(CVisualEffects::Shutdown,                atHashString("CVisualEffects",0xbc186f00));
#if VOLUME_SUPPORT
        REGISTER_SHUTDOWN_CALL(CVolumeManager::Shutdown,                atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
        REGISTER_SHUTDOWN_CALL(CVehiclePopulation::Shutdown,            atHashString("CVehiclePopulation",0xab533b92));
        REGISTER_SHUTDOWN_CALL(CTrain::Shutdown,                        atHashString("CTrain",0x36860703));
#if GTA_REPLAY
		REGISTER_SHUTDOWN_CALL(CReplayMgr::ShutdownSession,				atHashString("CReplayMgr",0xcaf18c95));
#endif 
/*#if NA_RADIO_ENABLED
        REGISTER_SHUTDOWN_CALL(CRadioHud::Shutdown,                     atHashString("CRadioHud",0xc9106614));
#endif*/
        REGISTER_SHUTDOWN_CALL(CPedVariationPack::Shutdown,             atHashString("CPedVariationPack",0xa99d54c4));
		REGISTER_SHUTDOWN_CALL(CPedModelInfo::ShutdownClass,			atHashString("CPedModelInfo",0x74599d94));

		REGISTER_SHUTDOWN_CALL(CHandlingDataMgr::Shutdown,				atHashString("CHandlingDataMgr",0x744e0c2));
		REGISTER_SHUTDOWN_CALL(CEventDataManager::Shutdown,             atHashString("CEventDataManager",0xf3bcaf9d));
        REGISTER_SHUTDOWN_CALL(CGtaAnimManager::Shutdown,               atHashString("CGtaAnimManager",0xf22520c7));
		REGISTER_SHUTDOWN_CALL(CAmbientAudioManager::Shutdown,          atHashString("CAmbientAudioManager",0xe108ecd9));
        REGISTER_SHUTDOWN_CALL(CAmbientAnimationManager::Shutdown,      atHashString("CAmbientAnimationManager",0x7df9e5f9));
		REGISTER_SHUTDOWN_CALL(CWitnessInformationManager::Shutdown,    atHashString("CWitnessInformationManager",0xa37fd148));
		REGISTER_SHUTDOWN_CALL(CCrimeInformationManager::Shutdown,      atHashString("CCrimeInformationManager",0xb29ed180));
        REGISTER_SHUTDOWN_CALL(CRandomEventManager::Shutdown,          atHashString("CRandomEventManager",0xda84b2ea));
		REGISTER_SHUTDOWN_CALL(CAmbientModelSetManager::ShutdownClass,	atHashString("CAmbientModelSetManager",0x1f713cf3));
		REGISTER_SHUTDOWN_CALL(CLadderMetadataManager::Shutdown,		atHashString("CLadderMetadataManager",0x5bb25da));
		REGISTER_SHUTDOWN_CALL(CConditionalAnimManager::Shutdown,       atHashString("CConditionalAnimManager",0x140aada5));
		REGISTER_SHUTDOWN_CALL(camManager::Shutdown,                    atHashString("camManager",0xa571eef5));
        REGISTER_SHUTDOWN_CALL(audNorthAudioEngine::Shutdown,           atHashString("audNorthAudioEngine",0xe6d408df));
        REGISTER_SHUTDOWN_CALL(Water::Shutdown,                         atHashString("Water",0xcadd9676));
        REGISTER_SHUTDOWN_CALL(CVehicleRecordingMgr::Shutdown,          atHashString("CVehicleRecordingMgr",0x8d873b09));
			REGISTER_SHUTDOWN_CALL(CPopulationStreamingWrapper::Shutdown,	atHashString("CPopulationStreamingWrapper",0x5d4bc852)); // CRandomEventManager keeps refs to cop peds/vehs
			REGISTER_SHUTDOWN_CALL(CAnimBlackboard::Shutdown,							atHashString("CAnimBlackboard",0xaa046da1));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
        REGISTER_SHUTDOWN_CALL(CVehicleMetadataMgr::Shutdown,           atHashString("CVehicleMetadataMgr",0xc2f04b3a));  // Do this after population streaming has released all vehicle models
        REGISTER_SHUTDOWN_CALL(CVehicleModelInfo::ShutdownClass,        atHashString("CVehicleModelInfo",0xcc6d84e1));
        REGISTER_SHUTDOWN_CALL(CGameWorld::Shutdown2,                   atHashString("CGameWorld 2",0xacdc8ba9));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_3);
    {
        REGISTER_SHUTDOWN_CALL(CModelInfo::Shutdown,                    atHashString("CModelInfo",0x683c1964));  //  GSW_ - Or maybe this could be considered to be UnloadMap?
		REGISTER_SHUTDOWN_CALL(CScenarioManager::Shutdown,              atHashString("CScenarioManager",0x7731e590));	//Wait until after entities have been removed (CGameWorld::Shutdown2)
		REGISTER_SHUTDOWN_CALL(CTask::Shutdown,                         atHashString("CTask",0xdffc5283));				//Wait until after entities have been removed (CGameWorld::Shutdown2)
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_4);
    {
        REGISTER_SHUTDOWN_CALL(CInstanceStoreWrapper::Shutdown2,        atHashString("INSTANCESTORE 2",0x14e4701f));
        REGISTER_SHUTDOWN_CALL(CMapTypesStoreWrapper::Shutdown2,        atHashString("fwMapTypesStore 2",0x49deb954));
		REGISTER_SHUTDOWN_CALL(fwDrawableStoreWrapper::Shutdown,		atHashString("fwDrawawableStoreWrapper",0xdd32fbf7));
		REGISTER_SHUTDOWN_CALL(fwFragmentStoreWrapper::Shutdown,		atHashString("fwFragmentStoreWrapper",0x6b086952));
		REGISTER_SHUTDOWN_CALL(fwDwdStoreWrapper::Shutdown,             atHashString("fwDwdStore",0x24730dbb));
#if !__FINAL
		REGISTER_SHUTDOWN_CALL(CBlockView::Shutdown,                    atHashString("CBlockView",0xd4ba82c9));
#endif  // !__FINAL
        REGISTER_SHUTDOWN_CALL(fwTxdStoreWrapper::Shutdown,              atHashString("fwTxdStore",0xf9c93f03));
        REGISTER_SHUTDOWN_CALL(CGameWorld::Shutdown3,                    atHashString("CGameWorld 3",0x815d34a7));
		REGISTER_SHUTDOWN_CALL(CStreaming::Shutdown,                    atHashString("CStreaming",0x9a0892b1));
    }
}

void CGame::RegisterSessionInitFunctions()
{
    SET_INIT_TYPE(INIT_SESSION);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
		REGISTER_INIT_CALL(CVehicleRecordingMgr::Init,        atHashString("CVehicleRecordingMgr",0x8d873b09)); // this needs to be done before CStreaming since at that point recording files will be registered
        REGISTER_INIT_CALL(CStreaming::Init,                  atHashString("CStreaming",0x9a0892b1)); // streaming has to be done first since lots of systems depend on it
		REGISTER_INIT_CALL(CFocusEntityMgr::Init,			  atHashString("CFocusEntityMgr",0xc7155016));
		REGISTER_INIT_CALL(audNorthAudioEngine::Init,		  atHashString("audNorthAudioEngine",0xe6d408df));
		REGISTER_INIT_CALL(CGtaAnimManager::Init,             atHashString("CGtaAnimManager",0xf22520c7));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
		REGISTER_INIT_CALL(CModelInfo::Init,						   atHashString("CModelInfo::Init",0xb051c59e));

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
		REGISTER_INIT_CALL(CInstanceListAssetLoader::Init,				atHashString("CInstanceListAssetLoader::Init",0x5fc7af0d));
#endif

		REGISTER_INIT_CALL(CTask::Init,								   atHashString("CTask",0xdffc5283));
		REGISTER_INIT_CALL(CScenarioPointManager::InitSession,		   atHashString("CScenarioPointManagerInitSession",0x19157c4e));
		REGISTER_INIT_CALL(CRenderPhaseCascadeShadowsInterface::Init_, atHashString("CRenderPhaseCascadeShadowsInterface",0xea9c8516));
        REGISTER_INIT_CALL(fwAnimDirector::InitStatic,                 atHashString("fwAnimDirector",0xca5ed810));
        REGISTER_INIT_CALL(fwClipSetManager::Init,                     atHashString("fwClipSetManager",0x56872c54));
		REGISTER_INIT_CALL(fwExpressionSetManager::Init,               atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_INIT_CALL(fwFacialClipSetGroupManager::Init,          atHashString("fwFacialClipSetGroupManager",0xedcf3461));
		REGISTER_INIT_CALL(CTaskGetUp::InitMetadata,				   atHashString("CTaskRecover",0x746579a));
#if USE_TARGET_SEQUENCES
		REGISTER_INIT_CALL(CTargetSequenceManager::InitMetadata,	   atHashString("CTargetSequenceManager",0x9d97513a));
#endif // USE_TARGET_SEQUENCES
		REGISTER_INIT_CALL(camManager::Init,                           atHashString("camManager",0xa571eef5));
        REGISTER_INIT_CALL(CAssistedMovementRouteStore::Init,          atHashString("CAssistedMovementRouteStore",0x66f6e894));
        REGISTER_INIT_CALL(CBuses::Init,                               atHashString("CBuses",0x3419e7e5));
        REGISTER_INIT_CALL(CCheat::Init,                               atHashString("CCheat",0x7e5bc866));
        REGISTER_INIT_CALL(CControlMgr::Init,                          atHashString("CControlMgr",0xd6761487));
        REGISTER_INIT_CALL(CGameLogic::Init,                           atHashString("CGameLogic",0xcd86c3fe));
        REGISTER_INIT_CALL(CGameWorld::Init,                           atHashString("CGameWorld",0xe4c9fbc));
        REGISTER_INIT_CALL(CGarages::Init,                             atHashString("CGarages",0x3c8124b5));
        REGISTER_INIT_CALL(CGps::Init,                                 atHashString("CGps",0x82f2dd62));
        REGISTER_INIT_CALL(CJunctions::Init,                           atHashString("CJunctions",0x48343c11));
		REGISTER_INIT_CALL(CPathZoneManager::Init,					   atHashString("CPathZoneManager",0x8ec3a09d));
        REGISTER_INIT_CALL(CMessages::Init,                            atHashString("CMessages",0xf0e7fef9));
        REGISTER_INIT_CALL(CNetwork::Init,                             atHashString("CNetwork",0xb6331929));
        REGISTER_INIT_CALL(CObjectPopulationNY::Init,                  atHashString("CObjectPopulationNY",0xd923f525));
        REGISTER_INIT_CALL(CPathFindWrapper::Init,                     atHashString("CPathFind",0xf828707e));
        REGISTER_INIT_CALL(CPed::Init,                                 atHashString("CPed",0x94821659));
		REGISTER_INIT_CALL(CPedModelInfo::InitClass,                   atHashString("CPedModelInfo",0x74599d94));
        REGISTER_INIT_CALL(CPedPopulation::Init,                       atHashString("CPedPopulation",0xc09bae66));
        REGISTER_INIT_CALL(CPerformance::Init,                         atHashString("CPerformance",0x270993cb));
		REGISTER_INIT_CALL(CPedAILodManager::Init,                     atHashString("CPedAILodManager",0x77366627));
		REGISTER_INIT_CALL(CVehicleAILodManager::Init,                 atHashString("CVehicleAILodManager",0xce15cb3));
        REGISTER_INIT_CALL(CPhoneMgr::Init,                            atHashString("CPhoneMgr",0x897d8554));
		REGISTER_INIT_CALL(CPhotoManager::Init,				  atHashString("CPhotoManager",0xaec90076));
        REGISTER_INIT_CALL(CPhysics::Init,                             atHashString("CPhysics",0x782937ba));
#if __BANK
		REGISTER_INIT_CALL(g_PickerManager.StaticInit,                 atHashString("CPicker",0xdc843ce6));
#endif
		REGISTER_INIT_CALL(CPickupManager::Init,              atHashString("CPickupManager",0xa69ecc96));
        REGISTER_INIT_CALL(CNetworkTelemetry::Init,           atHashString("CNetworkTelemetry",0x7353c05d));
		NOTFINAL_ONLY( REGISTER_INIT_CALL(CNetworkDebugTelemetry::Init,      atHashString("CNetworkDebugTelemetry",0xadc4ae61)); )
		REGISTER_INIT_CALL(CPopCycle::Init,                   atHashString("CPopCycle",0xd23b1168));
        REGISTER_INIT_CALL(CProcObjectManWrapper::Init,       atHashString("CProcObjectMan",0x73300b42));
/*#if NA_RADIO_ENABLED
		REGISTER_INIT_CALL(CRadioHud::Init,                   atHashString("CRadioHud",0xc9106614));
#endif*/
#if GTA_REPLAY
        REGISTER_INIT_CALL(CReplayMgr::InitSession,                  atHashString("CReplayMgr",0xcaf18c95));
#endif
        REGISTER_INIT_CALL(CRestart::Init,                    atHashString("CRestart",0x1204952b));
		REGISTER_INIT_CALL(CScaleformMgr::Init,				  atHashString("CScaleformMgr",0xd3fb2763));
        REGISTER_INIT_CALL(CSlownessZoneManager::Init,        atHashString("CSlownessZonesManager",0x7c06ad6d));
		REGISTER_INIT_CALL(CStreamingRequestList::Init,		  atHashString("CStreamingRequestList",0xe9b96fae));
        REGISTER_INIT_CALL(CStuntJumpManager::Init,           atHashString("CStuntJumpManager",0xebc4e679));
        REGISTER_INIT_CALL(CActionManager::Init,              atHashString("CActionManager",0xf3b5d7d9));
		REGISTER_INIT_CALL(CRenderTargetMgr::InitClass,		  atHashString("CRenderTargetMgr",0xa7a2930c));
		REGISTER_INIT_CALL(CTheScripts::Init,                 atHashString("CTheScripts",0x249760f7));
        REGISTER_INIT_CALL(CScriptAreas::Init,				  atHashString("CScriptAreas",0xf94a04df));
        REGISTER_INIT_CALL(CScriptCars::Init,				  atHashString("CScriptCars",0xa258fab4));
		REGISTER_INIT_CALL(CScriptPeds::Init,				  atHashString("CScriptPeds",0x806a092f));
		REGISTER_INIT_CALL(CScriptEntities::Init,			  atHashString("CScriptEntities",0x5417f27f));
        REGISTER_INIT_CALL(CScriptDebug::Init,				  atHashString("CScriptDebug",0x481538de));
        REGISTER_INIT_CALL(CScriptHud::Init,				  atHashString("CScriptHud",0xe2156445));
        REGISTER_INIT_CALL(CScriptPedAIBlips::Init,			  atHashString("CScriptPedAIBlips",0xa62f89da));
		REGISTER_INIT_CALL(CStatsMgr::Init,                   atHashString("CStatsMgr",0xfe7a308b));
        REGISTER_INIT_CALL(CTrain::Init,                      atHashString("CTrain",0x36860703));
        REGISTER_INIT_CALL(CUserDisplay::Init,                atHashString("CUserDisplay",0x53ba7867));
        REGISTER_INIT_CALL(CVehiclePopulation::Init,          atHashString("CVehiclePopulation",0xab533b92));
        REGISTER_INIT_CALL(CVisualEffects::Init,              atHashString("CVisualEffects",0xbc186f00));
#if VOLUME_SUPPORT
        REGISTER_INIT_CALL(CVolumeManager::Init,              atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
		REGISTER_INIT_CALL(CDispatchData::Init,				  atHashString("CDispatchData",0x54b59cca));
		REGISTER_INIT_CALL(CRoadBlock::Init,				  atHashString("CRoadBlock",0xf72cc701));
        REGISTER_INIT_CALL(CWeaponManager::Init,              atHashString("CWeaponManager",0xb1c90505));
		REGISTER_INIT_CALL(CScriptedGunTaskMetadataMgr::Init, atHashString("CScriptedGunTaskMetadataMgr",0xa8e151ec));
		REGISTER_INIT_CALL(CVehicleCombatAvoidanceArea::Init, atHashString("CVehicleCombatAvoidanceArea",0x670737c4));
		REGISTER_INIT_CALL(CCombatInfoMgr::Init,              atHashString("CCombatInfoMgr",0x7d65b3e8));
		REGISTER_INIT_CALL(CCombatDirector::Init,             atHashString("CCombatDirector",0x7e653152));
		REGISTER_INIT_CALL(CVehicleChaseDirector::Init,       atHashString("CVehicleChaseDirector",0xd2f56334));
		REGISTER_INIT_CALL(CBoatChaseDirector::Init,          atHashString("CBoatChaseDirector",0xa53ca289));
		REGISTER_INIT_CALL(CTacticalAnalysis::Init,           atHashString("CTacticalAnalysis",0xa203ea4b));
		REGISTER_INIT_CALL(CCoverFinder::Init,				  atHashString("CCoverFinder",0x4128232e));
        REGISTER_INIT_CALL(CWorldPoints::Init,                atHashString("CWorldPoints",0xcbc1e977));
        REGISTER_INIT_CALL(CGame::TimerInit,                  atHashString("fwTimer",0x43198286));
        REGISTER_INIT_CALL(PostFX::Init,                      atHashString("PostFX",0x6e413d11));
		REGISTER_INIT_CALL(CRenderer::Init,                   atHashString("CRenderer",0x9eaf5465));
        REGISTER_INIT_CALL(ViewportSystemInit,                atHashString("ViewportSystemInitLevel",0x43dd9eb0));
		REGISTER_INIT_CALL(CGameSituation::Init,              atHashString("CGameSituation",0x72377418));
		REGISTER_INIT_CALL(CPrioritizedClipSetRequestManager::Init,		atHashString("CPrioritizedClipSetRequestManager",0x61c3aa1));
		REGISTER_INIT_CALL(CPrioritizedClipSetStreamer::Init, atHashString("CPrioritizedClipSetStreamer",0x49fcbc12));
		REGISTER_INIT_CALL(CSituationalClipSetStreamer::Init, atHashString("CSituationalClipSetStreamer",0x83698af8));
		REGISTER_INIT_CALL(CClipDictionaryStoreInterface::Init, atHashString("CClipDictionaryStoreInterface",0x273a78d));
		REGISTER_INIT_CALL(CRiots::Init,                      atHashString("CRiots",0x8005b3e6));
		REGISTER_INIT_CALL(CCover::InitSession,					atHashString("CCover",0xf146972b));
		REGISTER_INIT_CALL(CExtraMetadataMgr::ClassInit, atHashString("CExtraMetadataMgr::ClassInit",0x4468c2ce));
		REGISTER_INIT_CALL(BackgroundScripts::InitClass,      atHashString("BackgroundScripts",0x6c7320d3));
#if AI_DEBUG_OUTPUT_ENABLED
		REGISTER_INIT_CALL(CAILogManager::Init,				atHashString("CAILogManager",0xc507959f));
#endif // AI_DEBUG_OUTPUT_ENABLED

#if ENABLE_JETPACK
		REGISTER_INIT_CALL(CJetpackManager::Init,              atHashString("CJetpackManager",0xe5558a3a));
#endif
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_3);
    {
		REGISTER_INIT_CALL(CText::Init,						  atHashString("CText",0xd82b15de));
		REGISTER_INIT_CALL(CMiniMap::Init,                 atHashString("CMiniMap",0xb3bbe4a1));
		REGISTER_INIT_CALL(CNewHud::Init,                     atHashString("CNewHud",0x7b6a9afe));
		REGISTER_INIT_CALL(CHudMarkerManager::Init,           atHashString("CHudMarkerManager", 0x4BFBFA2C));
		REGISTER_INIT_CALL(CBusySpinner::Init,                atHashString("CBusySpinner",0x8c989e94));
//		REGISTER_INIT_CALL(CMultiplayerGamerTagHud::Init,     atHashString("CMultiplayerGamerTagHud",0x1ae8d1));
#if RSG_PC
		REGISTER_INIT_CALL(CMultiplayerChat::Init,			   atHashString("CMultiplayerChat",0xb8568feb));
#endif
        REGISTER_INIT_CALL(CExplosionManager::Init,           atHashString("CExplosionManager",0x1715fd90));    // dependent on weaponmgr
		REGISTER_INIT_CALL(CPickupDataManager::Init,          atHashString("CPickupDataManager",0x6ec4812b));    // dependent on weaponmgr
        REGISTER_INIT_CALL(CPopulationStreamingWrapper::Init, atHashString("CPopulationStreaming",0x41c3b683)); // dependent on popcycle being initialised
        REGISTER_INIT_CALL(CDecoratorInterface::Init,         atHashString("CDecoratorInterface",0xb3966886));
		REGISTER_INIT_CALL(CScenarioManager::Init,		      atHashString("CScenarioManager",0x7731e590));
		REGISTER_INIT_CALL(CGestureManager::Init,			  atHashString("CGestureManager",0x10951fd5));
		REGISTER_INIT_CALL(CTexLod::InitSession,				atHashString("CTexLod",0x113f50c8));
		REGISTER_INIT_CALL(CPathServer::InitSession,			atHashString("CPathServer::InitSession",0xc262f7a1));
		REGISTER_INIT_CALL(CPauseMenu::Init,					atHashString("CPauseMenu",0xf0f5a94d));
		REGISTER_INIT_CALL(CExtraContentWrapper::Init, atHashString("CExtraContentWrapper",0xc2471be7));
		REGISTER_INIT_CALL(CDLCScript::Init,					atHashString("CDLCScript",0x2527583e));
		REGISTER_INIT_CALL(audNorthAudioEngine::DLCInit,        atHashString("audNorthAudioEngineDLC",0x1b8b001));
   }

#if __DEV
	SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_4);
	{
		REGISTER_INIT_CALL(CScaleformMgr::PreallocTest, atHashString("CScaleformMgr::PreallocTest",0xced723b3));
	}
#endif
}

void CGame::RegisterSessionShutdownFunctions()
{
    SET_SHUTDOWN_TYPE(SHUTDOWN_SESSION);

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_1);
    {
		REGISTER_SHUTDOWN_CALL(CExtraContentWrapper::ShutdownStart, atHashString("CExtraContentWrapper::ShutdownStart",0x4220ae24));
		REGISTER_SHUTDOWN_CALL(BackgroundScripts::ShutdownClass,	 atHashString("BackgroundScripts",0x6c7320d3));
		REGISTER_SHUTDOWN_CALL(CDLCScript::Shutdown,					atHashString("CDLCScript",0x2527583e));
		REGISTER_SHUTDOWN_CALL(CStatsMgr::Shutdown,						atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_SHUTDOWN_CALL(CCompEntity::Shutdown,					atHashString("CCompEntity",0x8ab3dccd));		//	These need to be done before CGameWorld::Shutdown1 gets a chance to
        REGISTER_SHUTDOWN_CALL(CProcObjectManWrapper::Shutdown,         atHashString("CProcObjectMan",0x73300b42));	//	delete the objects owned by CompEntity's and ProcObjects.
		REGISTER_SHUTDOWN_CALL(audNorthAudioEngine::Shutdown,           atHashString("audNorthAudioEngine",0xe6d408df));
		REGISTER_SHUTDOWN_CALL(decorator_commands::Shutdown,            atHashString("decorators",0x77394ac)); // allow registration of decorators again.
		REGISTER_SHUTDOWN_CALL(CConditionalAnimManager::Shutdown,       atHashString("CConditionalAnimManager",0x140aada5));
		REGISTER_SHUTDOWN_CALL(CAmbientModelSetManager::ShutdownClass,	atHashString("CAmbientModelSetManager",0x1f713cf3));
		REGISTER_SHUTDOWN_CALL(CScenarioPointManager::ShutdownSession,			atHashString("CScenarioPointManager",0xd1ab6c75));

#if __BANK
		REGISTER_SHUTDOWN_CALL(CPlayerSwitchDbg::Shutdown,				atHashString("CPlayerSwitchDbg",0x956a0a53));	// needs to be done before camera director shutdown
#endif	//__BANK

		REGISTER_SHUTDOWN_CALL(CPlayerSwitch::Shutdown,					atHashString("CPlayerSwitch",0xd02171e7));		// needs to be done before camera director shutdown
	}

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_2);
    {
#if AI_DEBUG_OUTPUT_ENABLED
        REGISTER_SHUTDOWN_CALL(CAILogManager::ShutDown,				    atHashString("CAILogManager",0xc507959f));
#endif // AI_DEBUG_OUTPUT_ENABLED
		REGISTER_SHUTDOWN_CALL(CClipDictionaryStoreInterface::Shutdown, atHashString("CClipDictionaryStoreInterface",0x273a78d));
		REGISTER_SHUTDOWN_CALL(CSituationalClipSetStreamer::Shutdown,   atHashString("CSituationalClipSetStreamer",0x83698af8));
		REGISTER_SHUTDOWN_CALL(CPrioritizedClipSetStreamer::Shutdown,   atHashString("CPrioritizedClipSetStreamer",0x49fcbc12));
		REGISTER_SHUTDOWN_CALL(CGameSituation::Shutdown,                atHashString("CGameSituation",0x72377418));
        REGISTER_SHUTDOWN_CALL(ViewportSystemShutdown,                  atHashString("ViewportSystemShutdownLevel",0xae5fd4b0));
        REGISTER_SHUTDOWN_CALL(CWorldPoints::Shutdown,                  atHashString("CWorldPoints",0xcbc1e977));
        REGISTER_SHUTDOWN_CALL(CWaypointRecording::Shutdown,            atHashString("CWaypointRecording",0xe7de7e92));
        REGISTER_SHUTDOWN_CALL(CVehiclePopulation::Shutdown,            atHashString("CVehiclePopulation",0xab533b92));
        REGISTER_SHUTDOWN_CALL(CVehicleModelInfo::ShutdownClass,        atHashString("CVehicleModelInfo",0xcc6d84e1));
        REGISTER_SHUTDOWN_CALL(CPedVariationStream::ShutdownSession,    atHashString("CPedVariationStream",0x69867e5a));
        REGISTER_SHUTDOWN_CALL(CutSceneManagerWrapper::Shutdown,        atHashString("CutSceneManagerWrapper",0x9a63cd5e));
		REGISTER_SHUTDOWN_CALL(CRenderTargetMgr::ShutdownClass,				atHashString("CRenderTargetMgr",0xa7a2930c));
        REGISTER_SHUTDOWN_CALL(CTheScripts::Shutdown,                   atHashString("CTheScripts",0x249760f7));
        REGISTER_SHUTDOWN_CALL(CScriptAreas::Shutdown,					atHashString("CScriptAreas",0xf94a04df));
        REGISTER_SHUTDOWN_CALL(CScriptDebug::Shutdown,					atHashString("CScriptDebug",0x481538de));
        REGISTER_SHUTDOWN_CALL(CScriptHud::Shutdown,					atHashString("CScriptHud",0xe2156445));
		REGISTER_SHUTDOWN_CALL(CScriptPedAIBlips::Shutdown,				atHashString("CScriptPedAIBlips",0xa62f89da));
        REGISTER_SHUTDOWN_CALL(CStuntJumpManager::Shutdown,             atHashString("CStuntJumpManager",0xebc4e679));
        REGISTER_SHUTDOWN_CALL(CStreamingRequestList::Shutdown,			atHashString("CStreamingRequestList",0xe9b96fae));
        REGISTER_SHUTDOWN_CALL(CRestart::Shutdown,                      atHashString("CRestart",0x1204952b));
        REGISTER_SHUTDOWN_CALL(CRenderer::Shutdown,                     atHashString("CRenderer",0x9eaf5465));
/*#if NA_RADIO_ENABLED
        REGISTER_SHUTDOWN_CALL(CRadioHud::Shutdown,                     atHashString("CRadioHud",0xc9106614));
#endif*/
		REGISTER_SHUTDOWN_CALL(LightEntityMgr::Shutdown,				atHashString("LightEntityMgr",0x5d795831));
        REGISTER_SHUTDOWN_CALL(CPortal::Shutdown,                       atHashString("CPortal",0x7840472b));
        REGISTER_SHUTDOWN_CALL(CNetworkTelemetry::Shutdown,             atHashString("CPlayStats",0xc8ca6fe6));
		NOTFINAL_ONLY( REGISTER_SHUTDOWN_CALL(CNetworkDebugTelemetry::Shutdown,      atHashString("CNetworkDebugTelemetry",0xadc4ae61)); )
		REGISTER_SHUTDOWN_CALL(CPlantMgr::Shutdown,                     atHashString("CPlantMgr",0x33a23607));
#if __BANK
		REGISTER_SHUTDOWN_CALL(g_PickerManager.StaticShutdown,			atHashString("CPicker",0xdc843ce6));
#endif
		REGISTER_SHUTDOWN_CALL(CPhysics::Shutdown,                      atHashString("CPhysics",0x782937ba));
		REGISTER_SHUTDOWN_CALL(CPhoneMgr::Shutdown,                     atHashString("CPhoneMgr",0x897d8554));
		REGISTER_SHUTDOWN_CALL(CPhotoManager::Shutdown,					atHashString("CPhotoManager",0xaec90076));
		REGISTER_SHUTDOWN_CALL(CPerformance::Shutdown,                  atHashString("CPerformance",0x270993cb));
		REGISTER_SHUTDOWN_CALL(CPedPopulation::Shutdown,                atHashString("CPedPopulation",0xc09bae66));
		REGISTER_SHUTDOWN_CALL(CPedModelInfo::ShutdownClass,			atHashString("CPedModelInfo",0x74599d94));
		REGISTER_SHUTDOWN_CALL(CPedGeometryAnalyser::Shutdown,          atHashString("CPedGeometryAnalyser",0xac566460));
		REGISTER_SHUTDOWN_CALL(CHandlingDataMgr::Shutdown,				atHashString("CHandlingDataMgr",0x744e0c2));
		REGISTER_SHUTDOWN_CALL(CPatrolRoutes::Shutdown,                 atHashString("CPatrolRoutes",0x70a66299));
		REGISTER_SHUTDOWN_CALL(CPathFindWrapper::Shutdown,              atHashString("CPathFind",0xf828707e));
		REGISTER_SHUTDOWN_CALL(CMovieMeshManager::Shutdown,				atHashString("CMovieMeshManager",0xe607a023));
		REGISTER_SHUTDOWN_CALL(CObjectPopulationNY::Shutdown,           atHashString("CObjectPopulationNY",0xd923f525));
		REGISTER_SHUTDOWN_CALL(CNetwork::Shutdown,                      atHashString("CNetwork",0xb6331929));
#if GTA_REPLAY
		REGISTER_SHUTDOWN_CALL(CReplayMgr::ShutdownSession,				atHashString("CReplayMgr",0xcaf18c95));
#endif
		REGISTER_SHUTDOWN_CALL(CInstanceStoreWrapper::Shutdown1,        atHashString("INSTANCESTORE 1",0x6bcf1deb));
		REGISTER_SHUTDOWN_CALL(CMapTypesStoreWrapper::Shutdown1,        atHashString("fwMapTypesStore 1",0x3a7c1a8f));
//		REGISTER_SHUTDOWN_CALL(CMultiplayerGamerTagHud::Shutdown,       atHashString("CMultiplayerGamerTagHud",0x1ae8d1));
		REGISTER_SHUTDOWN_CALL(CBusySpinner::Shutdown,                  atHashString("CBusySpinner",0x8c989e94));
		REGISTER_SHUTDOWN_CALL(CNewHud::Shutdown,                       atHashString("CNewHud",0x7b6a9afe));
		REGISTER_SHUTDOWN_CALL(CMiniMap::Shutdown,						atHashString("CMiniMap",0xb3bbe4a1));
		REGISTER_SHUTDOWN_CALL(CHudMarkerManager::Shutdown,	            atHashString("CHudMarkerManager", 0x4BFBFA2C));
#if GTA_REPLAY
		REGISTER_SHUTDOWN_CALL(CVideoEditorUi::Shutdown,				atHashString("CVideoEditorUi",0xa05ca7da));
		REGISTER_SHUTDOWN_CALL(WatermarkRenderer::Shutdown,         atHashString("WatermarkRenderer",0xD81097FD));
#endif
		REGISTER_SHUTDOWN_CALL(CPauseMenu::Shutdown,					atHashString("CPauseMenu",0xf0f5a94d));
		REGISTER_SHUTDOWN_CALL(cStoreScreenMgr::Shutdown,				atHashString("cStoreScreenMgr",0xd9d0e49d));
		REGISTER_SHUTDOWN_CALL(CGenericGameStorage::Shutdown,           atHashString("CGenericGameStorage",0x8f569024));
		REGISTER_SHUTDOWN_CALL(CGarages::Shutdown,                      atHashString("CGarages",0x3c8124b5));
		REGISTER_SHUTDOWN_CALL(CGameWorld::Shutdown1,                   atHashString("CGameWorld 1",0x9b18e81e));
		REGISTER_SHUTDOWN_CALL(CPickupManager::Shutdown,                atHashString("CPickupManager",0xa69ecc96)); // after gameworld Shutdown1 as this is where peds get deleted
		REGISTER_SHUTDOWN_CALL(CGameLogic::Shutdown,					atHashString("CGameLogic",0xcd86c3fe));
		REGISTER_SHUTDOWN_CALL(CCredits::Shutdown,                      atHashString("CCredits",0x2ee4e735));
		REGISTER_SHUTDOWN_CALL(CBuses::Shutdown,                        atHashString("CBuses",0x3419e7e5));
		REGISTER_SHUTDOWN_CALL(CAssistedMovementRouteStore::Shutdown,   atHashString("CAssistedMovementRouteStore",0x66f6e894));
#if __BANK
		REGISTER_SHUTDOWN_CALL(CAnimViewer::Shutdown,                   atHashString("CAnimViewer",0xa5c0a97b));
#endif // __BANK
        REGISTER_SHUTDOWN_CALL(fwClipSetManager::Shutdown,              atHashString("fwClipSetManager",0x56872c54));
		REGISTER_SHUTDOWN_CALL(fwExpressionSetManager::Shutdown,        atHashString("fwExpressionSetManager",0xe2b5b8ff));
		REGISTER_SHUTDOWN_CALL(fwFacialClipSetGroupManager::Shutdown,   atHashString("fwFacialClipSetGroupManager",0xedcf3461));

		REGISTER_SHUTDOWN_CALL(CTVPlaylistManager::Shutdown,			atHashString("CTVPlaylistManager",0x62a4aedc));

		REGISTER_SHUTDOWN_CALL(CLODLightManager::Shutdown,				atHashString("CLODLightManager",0xd203e710));
		REGISTER_SHUTDOWN_CALL(CLODLights::Shutdown,					atHashString("CLODLights",0xbb31917));
		REGISTER_SHUTDOWN_CALL(AmbientLights::Shutdown,					atHashString("AmbientLights",0x17304d82));

        REGISTER_SHUTDOWN_CALL(CTaskGetUp::ShutdownMetadata,			atHashString("CTaskRecover",0x746579a));
#if USE_TARGET_SEQUENCES
        REGISTER_SHUTDOWN_CALL(CTargetSequenceManager::ShutdownMetadata,atHashString("CTargetSequenceManager",0x9d97513a));
#endif // USE_TARGET_SEQUENCES
		REGISTER_SHUTDOWN_CALL(fwAnimDirector::ShutdownStatic, 			atHashString("fwAnimDirector",0xca5ed810));
		REGISTER_SHUTDOWN_CALL(camManager::Shutdown,					atHashString("camManager",0xa571eef5));
		REGISTER_SHUTDOWN_CALL(CJunctions::Shutdown,                    atHashString("CJunctions",0x48343c11));
#if __PPU
        REGISTER_SHUTDOWN_CALL(CPadGestureMgr::Shutdown,                atHashString("CPadGestureMgr",0x2ec0a772));
#endif
#if __DEV
        REGISTER_SHUTDOWN_CALL(CDebugScene::ShutdownModelViewer,        atHashString("CDebugScene",0x653e90cb));
#endif // __DEV
    
		REGISTER_SHUTDOWN_CALL(CRiots::Shutdown,                        atHashString("CRiots",0x8005b3e6));
		REGISTER_SHUTDOWN_CALL(CPrioritizedClipSetRequestManager::Shutdown,		atHashString("CPrioritizedClipSetRequestManager",0x61c3aa1));
	}

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_3);
	{
#if VOLUME_SUPPORT
        REGISTER_SHUTDOWN_CALL(CVolumeManager::Shutdown,                atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT
		REGISTER_SHUTDOWN_CALL(CVehicleRecordingMgr::Shutdown,          atHashString("CVehicleRecordingMgr",0x8d873b09));		//	dependent on CTheScripts::Shutdown - let the individual scripts clean up any recordings they've requested and vehicles that are playing those recordings
        REGISTER_SHUTDOWN_CALL(CVehicleCombatAvoidanceArea::Shutdown,   atHashString("CVehicleCombatAvoidanceArea",0x670737c4));
		REGISTER_SHUTDOWN_CALL(CCombatInfoMgr::Shutdown,				atHashString("CCombatInfoMgr",0x7d65b3e8));
		REGISTER_SHUTDOWN_CALL(CTacticalAnalysis::Shutdown,				atHashString("CTacticalAnalysis",0xa203ea4b));
		REGISTER_SHUTDOWN_CALL(CBoatChaseDirector::Shutdown,			atHashString("CBoatChaseDirector",0xa53ca289));
		REGISTER_SHUTDOWN_CALL(CVehicleChaseDirector::Shutdown,			atHashString("CVehicleChaseDirector",0xd2f56334));
		REGISTER_SHUTDOWN_CALL(CCombatDirector::Shutdown,				atHashString("CCombatDirector",0x7e653152));
		REGISTER_SHUTDOWN_CALL(CCoverFinder::Shutdown,					atHashString("CCoverFinder",0x4128232e));
		REGISTER_SHUTDOWN_CALL(CScriptedGunTaskMetadataMgr::Shutdown,	atHashString("CScriptedGunTaskMetadataMgr",0xa8e151ec));
		REGISTER_SHUTDOWN_CALL(CRoadBlock::Shutdown,					atHashString("CRoadBlock",0xf72cc701));
        REGISTER_SHUTDOWN_CALL(CWeaponManager::Shutdown,                atHashString("CWeaponManager",0xb1c90505));       // must be called after all peds removed from game world
		REGISTER_SHUTDOWN_CALL(CPickupDataManager::Shutdown,			atHashString("CPickupDataManager",0x6ec4812b));
        REGISTER_SHUTDOWN_CALL(CPopulationStreamingWrapper::Shutdown,   atHashString("CPopulationStreaming",0x41c3b683)); // must be called after all peds removed from game world
        REGISTER_SHUTDOWN_CALL(CPed::Shutdown,                          atHashString("CPed",0x94821659));                 // must be called after all peds removed from game world
		REGISTER_SHUTDOWN_CALL(CZonedAssetManager::Shutdown,			atHashString("CZonedAssetManager",0x2941253a));	 // must be called before model info shutdown

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
		REGISTER_SHUTDOWN_CALL(CInstanceListAssetLoader::Shutdown,		atHashString("CInstanceListAssetLoader::Shutdown",0xccc47f6b));
#endif
        REGISTER_SHUTDOWN_CALL(CModelInfo::Shutdown,                    atHashString("CModelInfo",0x683c1964));           // must be called after all entities removed from gameworld
		REGISTER_SHUTDOWN_CALL(CGestureManager::Shutdown,				atHashString("CGestureManager",0x10951fd5));
        REGISTER_SHUTDOWN_CALL(CGtaAnimManager::Shutdown,               atHashString("CGtaAnimManager",0xf22520c7));         // dependent on taskmelee being shutdown
		REGISTER_SHUTDOWN_CALL(CGtaRenderThreadGameInterface::ShutdownLevel,	atHashString("CRenderThreadInterface",0xe905ee7e));
        REGISTER_SHUTDOWN_CALL(CDecoratorInterface::Shutdown,           atHashString("CDecoratorInterface",0xb3966886));
		REGISTER_SHUTDOWN_CALL(CScenarioManager::Shutdown,              atHashString("CScenarioManager",0x7731e590));
		REGISTER_SHUTDOWN_CALL(CTexLod::ShutdownSession,				atHashString("CTexLod",0x113f50c8));
		REGISTER_SHUTDOWN_CALL(CPathServer::ShutdownSession,           atHashString("CPathServer::ShutdownSession",0x1c071d81));
#if ENABLE_JETPACK
		REGISTER_SHUTDOWN_CALL(CJetpackManager::Shutdown,              atHashString("CJetpackManager",0xe5558a3a));
#endif
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_4);
	{
		REGISTER_SHUTDOWN_CALL(CVisualEffects::Shutdown,                atHashString("CVisualEffects",0xbc186f00));		// dependent on all objects that stream particle assets being removed first (i.e. CPopulationStreamingWrapper::Shutdown)
#if GTA_REPLAY
        REGISTER_SHUTDOWN_CALL(CTextTemplate::CoreShutdown,             atHashString("CTextTemplate",0x4FFAC1F8));
#endif
        REGISTER_SHUTDOWN_CALL(CScaleformMgr::Shutdown,					atHashString("CScaleformMgr",0xd3fb2763));
		REGISTER_SHUTDOWN_CALL(fwStaticBoundsStoreWrapper::Shutdown,	atHashString("CStaticBoundsStore",0x567a3dfd));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_5);
    {
        REGISTER_SHUTDOWN_CALL(CStreaming::Shutdown,                    atHashString("CStreaming",0x9a0892b1)); // systems at a lower dependency need to use the streaming system to shutdown
		REGISTER_SHUTDOWN_CALL(CTrain::Shutdown,                      atHashString("CTrain",0x36860703));
    }

    SET_CURRENT_DEPENDENCY_LEVEL(MAP_LOADED_DEPENDENCY_LEVEL_6);
    {
        REGISTER_SHUTDOWN_CALL(fwTxdStoreWrapper::Shutdown,              atHashString("fwTxdStore",0xf9c93f03));  //dependent on streaming being shutdown
		REGISTER_SHUTDOWN_CALL(fwClothStoreWrapper::Shutdown,            atHashString("fwClothStore",0x9546ef55));
		REGISTER_SHUTDOWN_CALL(MeshBlendManager::Shutdown,              atHashString("MeshBlendManager",0xe78441ef));
		REGISTER_SHUTDOWN_CALL(CExtraContentWrapper::Shutdown, atHashString("CExtraContentWrapper::Shutdown",0x8bc3edb));
		REGISTER_SHUTDOWN_CALL(CExtraMetadataMgr::ShutdownDLCMetaFiles, atHashString("CExtraMetadataMgr::ShutdownDLCMetaFiles",0xf7320989));
		REGISTER_SHUTDOWN_CALL(CExtraMetadataMgr::ClassShutdown,        atHashString("CExtraMetadataMgr::ClassShutdown",0x3fa1b37b));
    }
}

void CGame::RegisterCommonMainUpdateFunctions()
{
    m_GameSkeleton.SetCurrentUpdateType(COMMON_MAIN_UPDATE);

	PUSH_UPDATE_GROUP_BUDGETTED("Common Main",0xca49e244, 1.0f, Profiler::CategoryColor::SceneUpdate);

	{
#if __PPU && __BANK
		REGISTER_UPDATE_CALL(CPS3Achievements::UpdateWidgets, atHashString("CPS3Achievements - UpdateWidgets",0x5315411b));
#endif
#if __BANK
		REGISTER_UPDATE_CALL(UpdateRestServices, atHashString("REST (web) services",0x17b4202a));
#endif

#if COLLECT_END_USER_BENCHMARKS
		REGISTER_UPDATE_CALL(EndUserBenchmarks::Update,	atHashString("EndUserBenchmarks",0x4d167300));
#endif	//COLLECT_END_USER_BENCHMARKS

#if !__FINAL
		REGISTER_UPDATE_CALL(CDebug::UpdateProfiler, atHashString("CDebug - UpdateProfiler",0x8d5ebed1));
        REGISTER_UPDATE_CALL(CDebug::Update,                             atHashString("CDebug",0x68a317a2));
        REGISTER_UPDATE_CALL(fwVectorMap::Update,                        atHashString("CVectorMap",0xad5da252));
        REGISTER_UPDATE_CALL(SmokeTests::ProcessSmokeTest,               atHashString("SmokeTests",0xd56d27c));
#endif // !__FINAL

#if DEBUG_PAD_INPUT && FINAL_DISPLAY_BAR &! __FINAL_LOGGING
		REGISTER_UPDATE_CALL(CDebug::UpdateDebugScaleformInfo, atHashString("CDebug - UpdateDebugScaleformInfo",0xcdf8344a));
#endif // DEBUG_PAD_INPUT && FINAL_DISPLAY_BAR

		FINAL_DISPLAY_BAR_ONLY(REGISTER_UPDATE_CALL(CDebugBar::Update,   atHashString("CDebugBar",0xba6fe146)));

#if __BANK
		REGISTER_UPDATE_CALL(CMarketingTools::Update,					 atHashString("Marketing Tools",0xb0a609cd));
#endif // __BANK

		REGISTER_UPDATE_CALL(CPauseMenu::Update,							 atHashString("PauseMenu",0x2c6e5c9f));

#if GTA_REPLAY
		REGISTER_UPDATE_CALL(CVideoEditorUi::Update,						 atHashString("CVideoEditorUi",0xa05ca7da));
		REGISTER_UPDATE_CALL(CVideoEditorInterface::Update,					 atHashString("CVideoEditorInterface",0xc90b32b2));
		REGISTER_UPDATE_CALL(WatermarkRenderer::Update,						 atHashString("WatermarkRenderer",0xD81097FD));
#endif // #if GTA_REPLAY
		REGISTER_UPDATE_CALL(SocialClubMenu::UpdateWrapper,					 atHashString("SocialClubMenu",0xbd732eef));
		REGISTER_UPDATE_CALL(PolicyMenu::UpdateWrapper,						 atHashString("PolicyMenu",0x1229e828));
		REGISTER_UPDATE_CALL(CReportMenu::Update,							 atHashString("CReportMenu",0x6149a34f));
#if RSG_PC
		REGISTER_UPDATE_CALL(CTextInputBox::Update,							 atHashString("CTextInputBox",0xfaab3737));
#endif // RSG_PC
//@@: range CGAME_REGISTERCOMMONMAINUPDATEFUNCTIONS {
#if TAMPERACTIONS_ENABLED
        REGISTER_UPDATE_CALL(TamperActions::Update,                          atHashString("TamperActions",0x602ee6e2));
#endif // TAMPERACTIONS_ENABLED
//@@: } CGAME_REGISTERCOMMONMAINUPDATEFUNCTIONS
		REGISTER_UPDATE_CALL(cStoreScreenMgr::Update,                        atHashString("cStoreScreenMgr",0xd9d0e49d));
		REGISTER_UPDATE_CALL(CGenericGameStorage::Update,			         atHashString("CGenericGameStorage",0x8f569024));
		REGISTER_UPDATE_CALL(CGenericGameStorage::UpdatePhotoGallery,        atHashString("GenericGameStoragePhotoGallery",0x849d8d4d));
		REGISTER_UPDATE_CALL(CPhotoManager::Update,							 atHashString("CPhotoManager",0xaec90076));
		REGISTER_UPDATE_CALL(CProfileSettingsWrapper::Update,                atHashString("CProfileSettings",0xcc979b1a));
		REGISTER_UPDATE_CALL(CExtraContentWrapper::Update,                   atHashString("CExtraContent",0x8d37277b));
		REGISTER_UPDATE_CALL(CDLCScript::Update,							 atHashString("CDLCScript",0x2527583e));
		REGISTER_UPDATE_CALL(CPerformanceWrapper::Update,                    atHashString("CPerformance",0x270993cb));
		REGISTER_UPDATE_CALL(CSaveMigration::Update,						 atHashString("CSaveMigration", 0x8930D2F7));
		REGISTER_UPDATE_CALL(CStatsMgr::Update,                              atHashString("CStatsMgr",0xfe7a308b));
		REGISTER_UPDATE_CALL(CNetwork::Update,                               atHashString("CNetwork",0xb6331929));
		REGISTER_UPDATE_CALL(CTheScripts::Update,                            atHashString("CTheScripts",0x249760f7));
#if GTA_REPLAY
#if __BANK
		REGISTER_UPDATE_CALL(CDebugVideoEditorScreen::Update,				 atHashString("CDebugVideoEditorScreen",0x3556d2e2) );
#endif
		REGISTER_UPDATE_CALL(CReplayCoordinator::Update,					 atHashString("CReplayCoordinator",0x7ef069c3) );
#endif

// #if VIDEO_RECORDING_ENABLED
// 		REGISTER_UPDATE_CALL(VideoRecording::Update,						 atHashString("VideoRecording",0x93ae9b6e));
// #endif	//VIDEO_RECORDING_ENABLED

#if GREATEST_MOMENTS_ENABLED
		REGISTER_UPDATE_CALL(fwGreatestMoment::Update,						 atHashString("fwGreatestMoments",0xca760256));
#endif // GREATEST_MOMENTS_ENABLED

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		REGISTER_UPDATE_CALL(VideoPlayback::Update,							 atHashString("VideoPlayback",0x52ae9cff));
		REGISTER_UPDATE_CALL(VideoPlaybackThumbnailManager::Update,			 atHashString("VideoPlaybackThumbnailManager",0xcff8e8f8));
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		
		REGISTER_UPDATE_CALL(fwImposedTextureDictionary::UpdateCleanup,		 atHashString("ImposedTxdCleanup",0xf32410b5));
		REGISTER_UPDATE_CALL(BackgroundScripts::UpdateClass,				 atHashString("BackgroundScripts",0x6c7320d3));

#if RSG_ENTITLEMENT_ENABLED
		REGISTER_UPDATE_CALL(CEntitlementManager::Update,					 atHashString("CEntitlementManager",0xeb1ae026));
#endif
#if USE_RAGESEC && !RAGE_SEC_IN_SEPARATE_THREAD
#if __FINAL
		REGISTER_UPDATE_CALL(rageSecEngine::Update,							atHashString("fwClothMeshing",0x73aa6f9e));
#else
		REGISTER_UPDATE_CALL(rageSecEngine::Update,							atHashString("rageSecEngine",0x73aa6f9e));
#endif
#endif
	}

	POP_UPDATE_GROUP(); // Common Main
}

void CGame::RegisterFullMainUpdateFunctions()
{
    m_GameSkeleton.SetCurrentUpdateType(FULL_MAIN_UPDATE);

	PUSH_UPDATE_GROUP_BUDGETTED("Misc", 0xe7c56e9c, 1.0f, Profiler::CategoryColor::SceneUpdate);
	{
		REGISTER_UPDATE_CALL(Water::UnflipWaterUpdateIndex,			 atHashString("Water Update Buffer Unflip",0xabe41f99));
		REGISTER_UPDATE_CALL(CVisualEffects::PreUpdate,				 atHashString("Visual Effects Pre Update",0x1a87f9ed));
		REGISTER_UPDATE_CALL(Lights::PreSceneUpdate,				 atHashString("Lights - PreSceneUpdate",0x2060bff0));
		REGISTER_UPDATE_CALL(CScriptHud::PreSceneProcess,			 atHashString("ScriptHud - PreSceneProcess",0x5b826729));
		REGISTER_UPDATE_CALL(CutSceneManagerWrapper::PreSceneUpdate, atHashString("CutSceneManager - PreSceneUpdate",0x5ac056c9));
		REGISTER_UPDATE_CALL(CAnimSceneManager::UpdatePreAnim,		 atHashString("AnimSceneManager - PreAnimUpdate",0x9ec80a4d));
		REGISTER_UPDATE_CALL(CCheat::DoCheats,                       atHashString("CCheat",0x7e5bc866)); // Has to be done after SaveOrRetrieveDataForThisFrame or cheats won't work in replay
		REGISTER_UPDATE_CALL(CClock::Update,                         atHashString("CClock",0x54f8bb2c));
		REGISTER_UPDATE_CALL(PedHeadshotManager::UpdateSafe,		 atHashString("PedHeadShotManager",0xcaa88743));
#if __DEV && RSG_PC
		REGISTER_UPDATE_CALL(CSettingsManager::ProcessTests,		 atHashString("SettingsManager",0x5aaf506b));
#endif
#if __BANK
		REGISTER_UPDATE_CALL(CDebugScene::PreUpdate,				 atHashString("CDebug::PreUpdate",0x3dcc78e));
#endif
	}
	POP_UPDATE_GROUP();

    PUSH_UPDATE_GROUP_BUDGETTED("Streaming", 0xd3ac6be7, 4.0f, Profiler::CategoryColor::Resources);
    {
		REGISTER_UPDATE_CALL(CSituationalClipSetStreamer::Update, atHashString("Situational Clip Set Streaming",0xda77bb6b));
		REGISTER_UPDATE_CALL(CPrioritizedClipSetStreamer::Update, atHashString("Prioritized Clip Set Streaming",0xb97f7f43));
        REGISTER_UPDATE_CALL(CPopulationStreamingWrapper::Update, atHashString("Population Streaming",0x27d33b6b));
        REGISTER_UPDATE_CALL(CStreaming::Update,                  atHashString("CStreaming",0x9a0892b1));
    }
    POP_UPDATE_GROUP(); // Streaming

	PUSH_UPDATE_GROUP_BUDGETTED("Ped and Vehicle assets", 0xaccd7dc5, 1.0f, Profiler::CategoryColor::Population);
		REGISTER_UPDATE_CALL(CPedVariationStream::ProcessStreamRequests, atHashString("CPedVariationStream - ProcessStreamRequests",0x9454bb80)); // change player models if required
		REGISTER_UPDATE_CALL(CVehicleVariationInstance::ProcessStreamRequests, atHashString("CVehicleVariationInstance - ProcessStreamRequests",0x2af5cd03)); // change player vehicle mods if required
		REGISTER_UPDATE_CALL(CPedPropsMgr::ProcessStreamRequests,	 atHashString("CPedPropsMgr - ProcessStreamRequests",0xfd83a35));
		REGISTER_UPDATE_CALL(MeshBlendManager::Update,               atHashString("MeshBlendManager",0xe78441ef));
		REGISTER_UPDATE_CALL(fwFacialClipSetGroupManager::Update,    atHashString("FacialClipSetGroupManager",0x6121d740));
    POP_UPDATE_GROUP();

    PUSH_UPDATE_GROUP_BUDGETTED("Game Update", 0xfa1c56ec, 19.0f, Profiler::CategoryColor::SceneUpdate);
    {
		PUSH_UPDATE_GROUP_BUDGETTED("World and Population", 0x4A825A8B, 2.0f, Profiler::CategoryColor::Population);
		{
			PUSH_UPDATE_GROUP_BUDGETTED("Zones", 0x8a426dc7, 3.0f, Profiler::CategoryColor::Population);
			{
				REGISTER_UPDATE_CALL(CMapAreas::Update,                  atHashString("CMapAreas",0x6bdb3a4c));
				REGISTER_UPDATE_CALL(CPopZones::Update,                  atHashString("CPopZones",0xc7015997));
				REGISTER_UPDATE_CALL(CMapZoneManager::Update,			 atHashString("CMapZoneManager",0xebfeb4c2));
			}
			POP_UPDATE_GROUP();

			PUSH_UPDATE_GROUP_BUDGETTED("Vehicles", 0x74b31be3, 3.0f, Profiler::CategoryColor::Population);
			{
				REGISTER_UPDATE_CALL(CVehiclePopulation::Process,        atHashString("CVehiclePopulation",0xab533b92));
				REGISTER_UPDATE_CALL(CVehicleAILodManager::Update,       atHashString("CVehicleAILodManager",0xce15cb3));
				REGISTER_UPDATE_CALL(CFlyingVehicleAvoidanceManager::Update, atHashString("CFlyingVehicleAvoidanceManager",0x8661dff));
			}
			POP_UPDATE_GROUP();

			PUSH_UPDATE_GROUP_BUDGETTED("Objects", 0x60a6bcea, 3.0f, Profiler::CategoryColor::Population);
			{
				REGISTER_UPDATE_CALL(CObjectPopulationNY::Process,       atHashString("CObjectPopulationNY",0xd923f525));
			}
			POP_UPDATE_GROUP();

			PUSH_UPDATE_GROUP_BUDGETTED("Peds", 0x8da12117, 3.0f, Profiler::CategoryColor::Population);
			{
				REGISTER_UPDATE_CALL(CPedPopulation::Process,            atHashString("CPedPopulation",0xc09bae66));
				REGISTER_UPDATE_CALL(CPedAILodManager::Update,           atHashString("CPedAILodManager",0x77366627));
			}
			POP_UPDATE_GROUP();

			PUSH_UPDATE_GROUP_BUDGETTED("Misc", 0xe7c56e9c, 3.0f, Profiler::CategoryColor::Population);
			{
				REGISTER_UPDATE_CALL_TIMED(CNetworkTelemetry::Update,    atHashString("CNetworkTelemetry",0x7353c05d));
				NOTFINAL_ONLY( REGISTER_UPDATE_CALL_TIMED(CNetworkDebugTelemetry::Update,      atHashString("CNetworkDebugTelemetry",0xadc4ae61)); )
				REGISTER_UPDATE_CALL(CRandomEventManager::Update,        atHashString("CRandomEventManager",0xda84b2ea));
				REGISTER_UPDATE_CALL(CRoadBlock::Update,                 atHashString("CRoadBlock",0xf72cc701));
				REGISTER_UPDATE_CALL(CScenarioManager::Update,			 atHashString("CScenarioManager",0x7731e590));
				REGISTER_UPDATE_CALL(CGameSituation::Update,			 atHashString("CGameSituation",0x72377418));
				REGISTER_UPDATE_CALL(CRecentlyPilotedAircraft::Update,	 atHashString("CRecentlyPilotedAircraft",0x155bb315));
			}
			POP_UPDATE_GROUP();
		}
		POP_UPDATE_GROUP();

		REGISTER_UPDATE_CALL(strStreamingEngine::SubmitDeferredAsyncPlacementRequests, atHashString("strStreamingEngine::SubmitDeferredAsyncPlacementRequests",0x57589bd5));

        REGISTER_UPDATE_CALL(CScene::Update,                     atHashString("CScene",0xc5d4866b));

		REGISTER_UPDATE_CALL(CScenarioManager::ResetExclusiveScenarioGroup,   atHashString("CScenarioManager::ResetExclusiveScenarioGroup",0x46ea34b));

		REGISTER_UPDATE_CALL(CWarpManager::Update,				 atHashString("CWarpManager",0x7c352747));

		REGISTER_UPDATE_CALL(CPedPopulation::ResetPerFrameScriptedMultipiers, atHashString("CPedPopulation::ResetPerFrameScriptedMultipiers",0x17d12ae8));
		REGISTER_UPDATE_CALL(CVehiclePopulation::ResetPerFrameModifiers,      atHashString("CVehiclePopulation::ResetPerFrameModifiers",0x8455d6e0));

		PUSH_UPDATE_GROUP_BUDGETTED("Game Logic", 0xeebf6236, 0.5f, Profiler::CategoryColor::Other);
        {
            REGISTER_UPDATE_CALL_TIMED(CGps::Update,                   atHashString("CGps",0x82f2dd62));
            REGISTER_UPDATE_CALL_TIMED(CGameLogic::Update,             atHashString("CGameLogic",0xcd86c3fe));
            REGISTER_UPDATE_CALL_TIMED(CSlownessZoneManager::Update,   atHashString("CSlownessZonesManager",0x7c06ad6d));
            REGISTER_UPDATE_CALL_TIMED(CGarages::Update,               atHashString("CGarages",0x3c8124b5));
            REGISTER_UPDATE_CALL_TIMED(CStuntJumpManager::Update,      atHashString("CStuntJumpManager",0xebc4e679));
            REGISTER_UPDATE_CALL_TIMED(CJunctions::Update,             atHashString("CJunctions",0x48343c11));
            REGISTER_UPDATE_CALL_TIMED(CPhoneMgr::Update,              atHashString("CPhoneMgr",0x897d8554));
            REGISTER_UPDATE_CALL_TIMED(CTask::StaticUpdate,            atHashString("CTask",0xdffc5283));
            REGISTER_UPDATE_CALL_TIMED(CTrafficLights::Update,         atHashString("CTrafficLights",0x238c6136));
			REGISTER_UPDATE_CALL_TIMED(CAppDataMgr::Update,			   atHashString("CAppDataMgr",0xbe3c8dde));
			REGISTER_UPDATE_CALL_TIMED(CNetRespawnMgr::Update,		   atHashString("CNetRespawnMgr",0x8b1bc3e4));

    #if __BANK
            REGISTER_UPDATE_CALL(CAnimViewer::Process,           atHashString("CAnimViewer",0xa5c0a97b));
    #endif //__BANK

			REGISTER_UPDATE_CALL_TIMED(CPedFactory::ProcessDestroyedPedsCache,              atHashString("PedPopulation Cache",0xdcc1fa69));
			REGISTER_UPDATE_CALL_TIMED(CVehicleFactory::ProcessDestroyedVehsCache,          atHashString("VehPopulation Cache",0x5d2cdadd));
			REGISTER_UPDATE_CALL_TIMED(WorldProbe::CShapeTestManager::KickAsyncWorker,      atHashString("Kick WorldProbe Async Worker",0x41cc6ab7));
			REGISTER_UPDATE_CALL_TIMED(CPedVariationStream::PreScriptUpdateFirstPersonProp, atHashString("FirstPersonProp Pre-script",0xd5301eb7));
        }
        POP_UPDATE_GROUP(); // Game Logic

        PUSH_UPDATE_GROUP_BUDGETTED("Script", 0xe7a8cdf1, NetworkInterface::IsGameInProgress()?3.0f:1.0f, Profiler::CategoryColor::Script);
        {
            REGISTER_UPDATE_CALL(CTheScripts::Process,           atHashString("CTheScripts",0x249760f7));
			REGISTER_UPDATE_CALL(CScriptPedAIBlips::Update,      atHashString("CScriptPedAIBlips",0xa62f89da));
			REGISTER_UPDATE_CALL(CMessages::Update,              atHashString("CMessages",0xf0e7fef9));
        }
        POP_UPDATE_GROUP(); // Script

#if __BANK
		REGISTER_UPDATE_CALL(CNodeViewer::Update,                atHashString("CNodeViewer",0x25474b2c));
#endif

        // this need to occur AFTER all bits of code that move things
        // PostProcessPhysics wasn't good enough because the scripts move stuff too
		PUSH_UPDATE_GROUP_BUDGETTED("ProcessAfterMovement", 0x5640404e, 1.5f, Profiler::CategoryColor::ProcessAfterMovement);
        {
            REGISTER_UPDATE_CALL_TIMED(CGameWorld::ProcessAfterAllMovement,atHashString("ProcessAfterMovement",0x5640404e));
        }
        POP_UPDATE_GROUP(); // ProcessAfterMovement

		// We've already done our fire searches async, and collected the entities that we found.
		// Finally, we'll process them during the game update while the scene graph is not locked.
		REGISTER_UPDATE_CALL(CFireManager::ProcessAllEntitiesNextToFires, atHashString("FireManager Entity Update",0xb1725922));

		REGISTER_UPDATE_CALL(CWarningScreen::Update,						 atHashString("WarningScreen",0x37153f31));  // warningscreens after all other updates (so it appears after any code updates and also script updates)
    }
    POP_UPDATE_GROUP(); // Game Update

    //since lights can be dynamic this needs to be done before shadow viewports are recalculated
    PUSH_UPDATE_GROUP_BUDGETTED("Render Update", 0xf230246a, 2.0f, Profiler::CategoryColor::Render);
    {
        // This should be AFTER scripts and AFTER objects are moved
        // DW - At present since perspective is a function of the cameras its best to process viewports before cameras as the aspect ratio will have latency for animating windows otherwise
		// Graeme - I've moved camInterface::UpdateFade before CViewportManagerWrapper::Process to fix GTA5 Bug 999476. The script was calling LOAD_SCENE as soon as IS_SCREEN_FADED_OUT returned TRUE.
		//	This caused the game to stall before CGtaOldLoadingScreen::SetLoadingScreenIfRequired() had a chance to change to the "Loading..." screen.
		//	Adam said to fix this in code to avoid any future script issues.
		REGISTER_UPDATE_CALL(camInterface::UpdateFade,				 atHashString("Update Fade",0x69b5fb3f));
        REGISTER_UPDATE_CALL(CViewportManagerWrapper::Process,       atHashString("Viewport Manager",0x4ad75fd3));

        // NB. the camera is run AFTER objects have moved. ONLY NOW is it valid to request the cameras position for this frame for camera relative SFX etc. - DW
        REGISTER_UPDATE_CALL(camManager::Update,                     atHashString("camManager",0xa571eef5));

		REGISTER_UPDATE_CALL(CPedVariationStream::UpdateFirstPersonProp, atHashString("FirstPersonProp",0x468992a7));

		REGISTER_UPDATE_CALL(Lights::Update,					 atHashString("Lights",0xa478703e));
		REGISTER_UPDATE_CALL(CLODLightManager::Update,           atHashString("CLODLightManager",0xd203e710));
		REGISTER_UPDATE_CALL(CLODLights::Update,				 atHashString("CLODLights",0xbb31917));
		REGISTER_UPDATE_CALL(AmbientLights::Update,				 atHashString("AmbientLights",0x17304d82));

		REGISTER_UPDATE_CALL(CVehicle::PreVisUpdate,			atHashString("Pre-vis Vehicle update",0xf8344776));

		// not dependant on lights or timecycle light shadow occlusion so can fire it off really early
		REGISTER_UPDATE_CALL_TIMED(COcclusion::KickOcclusionJobs,                atHashString("Occlusion jobs early kick",0x5d1be01a));

		// Update grass. Do this as early as we can so the proc object lists are ready for the proc object update.
		REGISTER_UPDATE_CALL_TIMED(CPlantMgrWrapper::UpdateBegin,          atHashString("PlantsMgr::UpdateBegin",0x1fba8319));

		REGISTER_UPDATE_CALL_TIMED(CShaderLib::Update,						atHashString("ShaderLib::Update",0x0f685a68));

		// This processes just peds early, so that they can launch any animation updates immediately after the camera 
		// update (we also allow occlusion to kick off before this so it can get started). We then block and wait for 
		// any animations that were kicked off here during CGameWorld::ProcessAfterCameraUpdate, which is registered/called 
		// below (as well as do the rest of the processing for other stuff, vehicles, objects, etc.)
		REGISTER_UPDATE_CALL_TIMED(CGameWorld::ProcessPedsEarlyAfterCameraUpdate,   atHashString("ProcessPedsEarlyAfterCameraUpdate",0xcb4a42fd));

		PUSH_UPDATE_GROUP("Water Update",0x5842c9ff);
		{
			REGISTER_UPDATE_CALL(Water::Update,                      atHashString("Water",0xcadd9676));
		}
		POP_UPDATE_GROUP(); // Water Processing

#if __BANK
		REGISTER_UPDATE_CALL(CMarketingTools::UpdateLights,			  atHashString("Marketing Tools",0xb0a609cd));
#endif // __BANK

		
#if __BANK
		REGISTER_UPDATE_CALL(CRenderPhaseDebugOverlayInterface::Update, atHashString("CRenderPhaseDebugOverlayInterface",0xd649516a));
		REGISTER_UPDATE_CALL(SceneGeometryCapture::Update, atHashString("SceneGeometryCapture",0x5be998f6));
#endif // __BANK

		REGISTER_UPDATE_CALL_TIMED(CTimeCycle::UpdateWrapper,						atHashString("CTimeCycle",0x4191a90a));
		REGISTER_UPDATE_CALL_TIMED(Water::UpdateHeightSimulation,					atHashString("WaterHeightSim",0xbe0bb025));//I need time cycle info to check if water is enabled
		REGISTER_UPDATE_CALL_TIMED(Lights::UpdateBaseLights,						atHashString("Lights",0xa478703e));
		REGISTER_UPDATE_CALL_TIMED(CRenderPhaseCascadeShadowsInterface::Update,	atHashString("CRenderPhaseCascadeShadowsInterface",0xea9c8516));

	
		REGISTER_UPDATE_CALL_TIMED(CParaboloidShadow::Update,						atHashString("CParaboloidShadow",0xc5cb0f00));  // This need to be moved later in the scene.


		REGISTER_UPDATE_CALL_TIMED(CViewportManagerWrapper::CreateFinalScreenRenderPhaseList, atHashString("CreateFinalScreenRenderPhaseList",0x6d9e4219));

		// shadow occlusion job must be after occlusion jobs kick
		REGISTER_UPDATE_CALL_TIMED(COcclusion::KickShadowOcclusionJobs,                atHashString("Occlusion shadows jobs early kick",0x7d51b352));


		REGISTER_UPDATE_CALL_TIMED(Water::ScanCutBlocksWrapper,                atHashString("Run ScanCutBlocks Early",0xc56d2732));

		// these need to occur AFTER the camera system has been updated
        // calls ProcessPostCamera on all AI tasks
        // updates the visibility (and lod distance) of dynamic entities
        // updates the skeleton of entities that are visible since the camera update
        REGISTER_UPDATE_CALL_TIMED(CGameWorld::ProcessAfterCameraUpdate,   atHashString("ProcessAfterCameraUpdate",0x7d777220));

		REGISTER_UPDATE_CALL(CPedVariationStream::UpdateFirstPersonPropFromCamera, atHashString("FirstPersonPropCam",0x3734def3));

		REGISTER_UPDATE_CALL_TIMED(LightEntityMgr::Update,					atHashString("LightEntityMgr",0x5d795831));

		// Update grass:
		REGISTER_UPDATE_CALL_TIMED(CPlantMgrWrapper::UpdateEnd,				atHashString("PlantsMgr::UpdateEnd",0x5a0409dc));

		// Do this as late as possible as we want the proc object lists generated by the plantmgr task to be ready
		REGISTER_UPDATE_CALL_TIMED(CProcObjectManWrapper::Update,			atHashString("Proc Objects",0x303abc50));

		REGISTER_UPDATE_CALL_TIMED(CSceneStreamerMgrWrapper::PreScanUpdate,	atHashString("CSceneStreamerMgr::PreScanUpdate",0x06d75616));

		// 8/15/12 - cthomas - We want to kick the visibility jobs as early as possible. They already have a dependency 
		// setup on the occlusion jobs, and so they shouldn't start doing the visibility scan before the occlusion 
		// software rasterization is finished anyways - but the earlier we kick them, the better the possibility that 
		// the rage dependency system will schedule them immediately when the occlusion rasterizer is finished. To be 
		// able to kick them as early as possible, we need to make sure we've done everything we need with the scene 
		// graph before kicking them, since as soon as we kick them we can possibly lock the scene graph if they start 
		// up the visibility scanning. Moved the light entity update above this kick visibility call to make sure that 
		// is done beforehand. Also moved the procedural object manager and plant manager end update above this, since 
		// it wants to update the scene graph. This also has to be done after CGameWorld::ProcessAfterCameraUpdate(), since 
		// position/AABB of entities can change as a result of it.

		// CNewHud::Update calls Scaleform functions which call CStreaming::LoadAllRequestedObjects which that then calls
		// rage::strStreamingLoaderManager::LoadAllRequestedObjects which ends up calling rage::fwMapDataContents::Entities_AddToWorld.
		// This has to happen before CGameScan::KickVisibilityJobs!
		REGISTER_UPDATE_CALL_TIMED(CNewHud::Update,                        atHashString("NewHud Update",0xe603884d));
		REGISTER_UPDATE_CALL_TIMED(CBusySpinner::Update,                   atHashString("CBusySpinner Update",0xce275c62));

#if !__FINAL // This updates the scene as well. Its !__FINAL so I'm moving it in front of KickVisibilityJobs
		REGISTER_UPDATE_CALL(CDebugScene::Update,                    atHashString("CDebugScene",0x653e90cb));
#endif

        REGISTER_UPDATE_CALL(CutSceneManagerWrapper::PostSceneUpdate,		atHashString("CutSceneManager - PostSceneUpdate",0xa4ca188));

		REGISTER_UPDATE_CALL(CVehicle::UpdateLightsOnFlagsForAllVehicles,	"UpdateLightsOnFlagsForAllVehicles");

#if GTA_REPLAY
		 // Call to process the replay engine
		REGISTER_UPDATE_CALL(CReplayMgr::Process,							atHashString("ReplayManager - Process",0xda9ccacf));
#endif

		REGISTER_UPDATE_CALL_TIMED(CGameScan::KickVisibilityJobs,			atHashString("Visibility jobs early kick",0xa0521bbf));

	#if CPLANT_USE_OCCLUSION
		REGISTER_UPDATE_CALL_TIMED(CPlantMgrWrapper::UpdateOcclusion,		atHashString("PlantsMgr::UpdateOcclusion",0x4c88109c));
	#endif

		REGISTER_UPDATE_CALL_TIMED(CMultiplayerGamerTagHud::Update,        atHashString("CMultiplayerGamerTagHud Update",0x11fa31a7));
		REGISTER_UPDATE_CALL(UIWorldIconManager::UpdateWrapper,				 atHashString("UIWorldIconManager",0xa04c7a9));

	#if RSG_PC
		REGISTER_UPDATE_CALL(CMultiplayerChat::Update,				atHashString("CMultiplayerChat Update",0x536de9a1));
	#endif
// #if NA_RADIO_ENABLED
// 		REGISTER_UPDATE_CALL_TIMED(CRadioHud::UpdateCarRadioHud,           atHashString("Radio Hud Update",0xff68d108));
// #endif

        REGISTER_UPDATE_CALL_TIMED(CCredits::Update,                       atHashString("Credits",0xa053bda2));
        REGISTER_UPDATE_CALL_TIMED(CCullZones::Update,                     atHashString("Occlusion",0x2d898a9));


        REGISTER_UPDATE_CALL(CLoadingScreens::Update,                atHashString("CLoadingScreens",0x21b66612));
#if __DEV
        REGISTER_UPDATE_CALL(CDebugScene::UpdateModelViewer,         atHashString("UpdateModelViewer",0xc1e406d7));
#endif
#if !__FINAL
        REGISTER_UPDATE_CALL(CBlockView::Update,                     atHashString("CBlockView",0xd4ba82c9));
#endif
#if __BANK
        REGISTER_UPDATE_CALL(CDebugScene::DisplayEntitiesOnVectorMap,atHashString("DisplayEntitiesOnVectorMap",0xf9425a0e));
#endif
#if (SECTOR_TOOLS_EXPORT)
        REGISTER_UPDATE_CALL(CSectorTools::Update,                   atHashString("CSectorTools",0x903d31ad));
#endif
        REGISTER_UPDATE_CALL(CVehicleDeformation::TexCacheUpdate,    atHashString("CVehicleDeformation",0x808725f0));
        REGISTER_UPDATE_CALL(CVisualEffects::Update,						atHashString("CVisualEffects",0xbc186f00));
#if VOLUME_SUPPORT
        REGISTER_UPDATE_CALL(CVolumeManager::Update,						atHashString("CVolumeManager",0xab925927));
#endif // VOLUME_SUPPORT

		REGISTER_UPDATE_CALL(CSky::Update,									atHashString("CSky",0xffcc9c60));

#if USE_TREE_IMPOSTERS
        REGISTER_UPDATE_CALL(CTreeImposters::Update,						atHashString("CTreeImposters",0x5ac79079));
#endif // USE_TREE_IMPOSTERS

        REGISTER_UPDATE_CALL_TIMED(PostFX::Update,							atHashString("PostFx",0x6e413d11));
#if __BANK
		REGISTER_UPDATE_CALL_TIMED(DebugRenderingMgr::Update,				atHashString("DebugRenderingMgr",0x3f4c6358));
#endif
    }
    POP_UPDATE_GROUP(); // Render Update

    // start updating the audio

	PUSH_UPDATE_GROUP_BUDGETTED("Audio Update", 0x50dd0ef5, 2.0f, Profiler::CategoryColor::Sound);
	{
		REGISTER_UPDATE_CALL_TIMED(audNorthAudioEngine::StartUpdate,           atHashString("Audio",0xb80c6ae8));
	}
	POP_UPDATE_GROUP(); // Audio Update

	PUSH_UPDATE_GROUP_BUDGETTED("Misc Update", 0xcd438623, 1.0f, Profiler::CategoryColor::SceneUpdate);
	{
		REGISTER_UPDATE_CALL_TIMED(CEntityExt_AltGfxMgr::ProcessExtensions,	atHashString("Streaming of alternate entity graphics",0x89f85028));

	#if __DEV
		REGISTER_UPDATE_CALL(CThePopMultiplierAreas::Update,			atHashString("Population Multplier areas",0x52feac1b));
	#endif /* _DEV */

		REGISTER_UPDATE_CALL_TIMED(perfClearingHouse::Update, atHashString("perfClearingHouse Update",0xa7b04034));
	}
	POP_UPDATE_GROUP(); // Misc Update
}

void CGame::RegisterSimpleMainUpdateFunctions()
{
    m_GameSkeleton.SetCurrentUpdateType(SIMPLE_MAIN_UPDATE);

    REGISTER_UPDATE_CALL(Water::Update,                             atHashString("Water",0xcadd9676));
    REGISTER_UPDATE_CALL_TIMED(CPopulationStreamingWrapper::Update, atHashString("Population Streaming",0x27d33b6b));
    REGISTER_UPDATE_CALL(CStreaming::Update,                        atHashString("CStreaming",0x9a0892b1));
	REGISTER_UPDATE_CALL(strStreamingEngine::SubmitDeferredAsyncPlacementRequests, atHashString("strStreamingEngine::SubmitDeferredAsyncPlacementRequests",0x57589bd5));
    REGISTER_UPDATE_CALL(Lights::ResetSceneLights,                  atHashString("ResetSceneLights",0x60b70f64));// Do during front end to stop lights from persisting into the game from the previous game

	PUSH_UPDATE_GROUP_BUDGETTED("Script", 0xe7a8cdf1, 1.0f, Profiler::CategoryColor::Script);
    {
        REGISTER_UPDATE_CALL(CTheScripts::Process,            atHashString("CTheScripts",0x249760f7));
		REGISTER_UPDATE_CALL(CScriptPedAIBlips::Update,       atHashString("CScriptPedAIBlips",0xa62f89da));
    }
    POP_UPDATE_GROUP(); // Script

    // This should be AFTER scripts and AFTER objects are moved
    // DW - At present since perspective is a function of the cameras its best to process viewports before cameras as the aspect ratio will have latency for animating windows otherwise
	REGISTER_UPDATE_CALL(camInterface::UpdateFade,			  atHashString("Update Fade",0x69b5fb3f));
	REGISTER_UPDATE_CALL(CViewportManagerWrapper::Process,    atHashString("Viewport Manager",0x4ad75fd3));

    // NB. the camera is run AFTER objects have moved. ONLY NOW is it valid to request the cameras position for this frame for camera relative SFX etc. - DW
    REGISTER_UPDATE_CALL(camManager::Update,                  atHashString("camManager",0xa571eef5));
#if __BANK
	REGISTER_UPDATE_CALL(CMarketingTools::UpdateLights,		  atHashString("Marketing Tools",0xb0a609cd));
#endif // !__FINAL

	REGISTER_UPDATE_CALL(CTimeCycle::UpdateWrapper,			atHashString("CTimeCycle",0x4191a90a));
	REGISTER_UPDATE_CALL(CViewportManagerWrapper::CreateFinalScreenRenderPhaseList, atHashString("CreateFinalScreenRenderPhaseList",0x6d9e4219));
    REGISTER_UPDATE_CALL_TIMED(PostFX::Update,              atHashString("PostFx",0x6e413d11));

	PUSH_UPDATE_GROUP_BUDGETTED("Audio Update", 0x50dd0ef5, 0.1f, Profiler::CategoryColor::Sound);
    {
        REGISTER_UPDATE_CALL_TIMED(audNorthAudioEngine::StartUpdate,atHashString("Audio",0xb80c6ae8));
    }
    POP_UPDATE_GROUP(); // Audio

	REGISTER_UPDATE_CALL(CWarningScreen::Update,						 atHashString("WarningScreen",0x37153f31));  // warningscreens after all other updates (so it appears after any code updates and also script updates)

}

#if GEN9_LANDING_PAGE_ENABLED
void CGame::RegisterLandingPagePreInitUpdateFunctions()
{
	m_GameSkeleton.SetCurrentUpdateType(LANDING_PAGE_PRE_INIT_UPDATE);

	REGISTER_UPDATE_CALL(CSaveMigration::Update,			atHashString("CSaveMigration", 0x8930D2F7));

    PUSH_UPDATE_GROUP(atHashString("Script", 0xe7a8cdf1));
    {
        REGISTER_UPDATE_CALL(CTheScripts::Process,          atHashString("CTheScripts",0x249760f7));
    }
    POP_UPDATE_GROUP(); // Script

    REGISTER_UPDATE_CALL(CPauseMenu::Update,           atHashString("PauseMenu",0x2c6e5c9f));
  }

void CGame::RegisterLandingPageUpdateFunctions()
{
	m_GameSkeleton.SetCurrentUpdateType(LANDING_PAGE_UPDATE);

    // Only add class updates here which DO NOT occur under COMMON_MAIN_UPDATE.
    // ** All pre-session updates should be added to LANDING_PAGE_PRE_INIT_UPDATE **
	REGISTER_UPDATE_CALL(CLandingPage::Update,				atHashString("CLandingPage", 0x165B8EED ));

    PUSH_UPDATE_GROUP(atHashString("Streaming", 0xd3ac6be7));
    {
        REGISTER_UPDATE_CALL(CStreaming::Update, atHashString("CStreaming", 0x9a0892b1));
        REGISTER_UPDATE_CALL(strStreamingEngine::SubmitDeferredAsyncPlacementRequests, atHashString("strStreamingEngine::SubmitDeferredAsyncPlacementRequests", 0x57589bd5));
    }
    POP_UPDATE_GROUP(); // Streaming

    REGISTER_UPDATE_CALL(CWarningScreen::Update,			atHashString("WarningScreen", 0x37153f31));  
    // warningscreens after all other updates (so it appears after any code updates and also script updates)

}
#endif // GEN9_LANDING_PAGE_ENABLED

bool CGame::ShouldDoFullUpdateRender()
{
    bool doFullUpdateRender = true;

    // new pausemenu design doesnt fade out the screen so we cannot do this anymore, or need another solution

	// new pausemenu shows screen behind... disable this for now but probably need to rethink it
/*	if (CPauseMenu::IsActive()) 
	{
        doFullUpdateRender=false;
	}
	//except...
	if (CPauseMenu::IsInMapScreen())  // we need to update the game on the map screen
	{
        doFullUpdateRender=true;
	}*/

	// the game always needs to be updated when in a network game
	if (NetworkInterface::IsNetworkOpen())
	{
		doFullUpdateRender=true;
	}

#if __DEV
	static dev_bool debugForceDoFullRender=false;
	static dev_bool debugForceDontFullRender=false;
	if (debugForceDontFullRender==true)
	{
        doFullUpdateRender=false;
	}
	if (debugForceDoFullRender==true)
	{
        doFullUpdateRender=true;
	}
#endif

	if (IsChangingSessionState())
	{
        doFullUpdateRender=false;
	}

#if GEN9_LANDING_PAGE_ENABLED
	if ( CLandingPageArbiter::ShouldGameRunLandingPageUpdate() )
	{
		doFullUpdateRender = false;
	}

	if (!CGame::IsSessionInitialized())
	{
		doFullUpdateRender = false;
	}
#endif // GEN9_LANDING_PAGE_ENABLED

    return doFullUpdateRender;
}

#if COMMERCE_CONTAINER
void CGame::RegisterCommerceMainUpdateFunctions()
{
	m_GameSkeleton.SetCurrentUpdateType(COMMERCE_MAIN_UPDATE);

	PUSH_UPDATE_GROUP(atHashString("Common Main",0xca49e244));	
	{
		REGISTER_UPDATE_CALL(cStoreScreenMgr::Update,      atHashString("cStoreScreenMgr",0xd9d0e49d));
		REGISTER_UPDATE_CALL(CLiveManager::CommerceUpdate, atHashString("CLiveManager",0x66ef616b));
		REGISTER_UPDATE_CALL(CExtraContentWrapper::Update, atHashString("CExtraContent",0x8d37277b));
		REGISTER_UPDATE_CALL(CDLCScript::Update,           atHashString("CDLCScript",0x2527583e));
        REGISTER_UPDATE_CALL(CWarningScreen::Update,       atHashString("WarningScreen",0x37153f31));
		REGISTER_UPDATE_CALL(CPauseMenu::Update,           atHashString("PauseMenu",0x2c6e5c9f));
	}

	PUSH_UPDATE_GROUP_BUDGETTED(atHashString("Streaming",0xd3ac6be7), 4.0f);
	{
		REGISTER_UPDATE_CALL(CStreaming::Update, atHashString("CStreaming",0x9a0892b1));
		REGISTER_UPDATE_CALL(strStreamingEngine::SubmitDeferredAsyncPlacementRequests, atHashString("strStreamingEngine::SubmitDeferredAsyncPlacementRequests",0x57589bd5));
	}
	POP_UPDATE_GROUP(); // Streaming

	PUSH_UPDATE_GROUP_BUDGETTED(atHashString("Render Update",0xf230246a), 2.0f);
	{
		REGISTER_UPDATE_CALL(CViewportManagerWrapper::Process, atHashString("Viewport Manager",0x4ad75fd3));
		REGISTER_UPDATE_CALL_TIMED(CViewportManagerWrapper::CreateFinalScreenRenderPhaseList, atHashString("CreateFinalScreenRenderPhaseList",0x6d9e4219));
		REGISTER_UPDATE_CALL_TIMED(CBusySpinner::Update,                   atHashString("CBusySpinner Update",0xce275c62));
	}
	POP_UPDATE_GROUP(); // Render Update

	PUSH_UPDATE_GROUP(atHashString("Audio",0xb80c6ae8));
	{
		REGISTER_UPDATE_CALL_TIMED(audNorthAudioEngine::StartUpdate,atHashString("Audio",0xb80c6ae8));
	}
	POP_UPDATE_GROUP(); // Audio
}

void CGame::MarketplaceUpdate()
{
	m_GameSessionStateMachine.Update();

	PF_PUSH_TIMEBAR_BUDGETED("Update", 20);
	gVpMan.PushViewport(gVpMan.GetGameViewport());
	m_GameSkeleton.Update(COMMERCE_MAIN_UPDATE);
	gVpMan.PopViewport();
	PF_POP_TIMEBAR(); // Update

	PF_PUSH_TIMEBAR_BUDGETED("Visibility",4.0f);
	PF_START_TIMEBAR_DETAIL("BuildRenderLists Init");
	CRenderer::ClearRenderLists();
	PF_POP_TIMEBAR(); //visibility
}
#endif

#if RSG_PC
static void EmptyFunction()
{
}

void CGame::InitCallbacks()
{
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CSettingsManager::DeviceLost), MakeFunctor(CSettingsManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(VideoResManager::DeviceLost), MakeFunctor(VideoResManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CPtFxManager::DeviceLost), MakeFunctor(CPtFxManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CRenderPhaseCascadeShadowsInterface::DeviceLost), MakeFunctor(CRenderPhaseCascadeShadowsInterface::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CParaboloidShadow::DeviceLost), MakeFunctor(CParaboloidShadow::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CRenderTargets::DeviceLost), MakeFunctor(CRenderTargets::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(GBuffer::DeviceLost), MakeFunctor(GBuffer::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CViewportManager::DeviceLost), MakeFunctor(CViewportManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(PostFX::DeviceLost), MakeFunctor(PostFX::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(Water::DeviceLost), MakeFunctor(Water::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CMirrors::DeviceLost), MakeFunctor(CMirrors::DeviceReset)); /////
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(SSAO::DeviceLost), MakeFunctor(SSAO::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CPauseMenu::DeviceLost), MakeFunctor(CPauseMenu::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CNewHud::DeviceLost), MakeFunctor(CNewHud::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CMousePointer::DeviceLost), MakeFunctor(CMousePointer::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CWarningScreen::DeviceLost), MakeFunctor(CWarningScreen::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CLegacyLandingScreen::DeviceLost), MakeFunctor(CLegacyLandingScreen::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CLoadingScreens::DeviceLost), MakeFunctor(CLoadingScreens::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CMiniMap::DeviceLost), MakeFunctor(CMiniMap::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CHudMarkerManager::DeviceLost), MakeFunctor(CHudMarkerManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CGameStreamMgr::DeviceLost), MakeFunctor(CGameStreamMgr::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CPhoneMgr::DeviceLost), MakeFunctor(CPhoneMgr::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CPedDamageSetBase::DeviceLost), MakeFunctor(CPedDamageSetBase::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(CScaleformMgr::DeviceLost), MakeFunctor(CScaleformMgr::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(UI3DDrawManager::DeviceLost), MakeFunctor(UI3DDrawManager::DeviceReset));
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(PhonePhotoEditor::DeviceLost), MakeFunctor(PhonePhotoEditor::DeviceReset));

#if USE_MULTIHEAD_FADE
	GRCDEVICE.RegisterDeviceLostCallbacks(MakeFunctor(EmptyFunction), MakeFunctor(CSprite2d::ResetMultiFadeArea));
#endif
#if NV_SUPPORT
	GRCDEVICE.RegisterStereoChangeSepCallbacks(MakeFunctor(CSettingsManager::GetInstance(), &CSettingsManager::ChangeStereoSeparation));
	GRCDEVICE.RegisterStereoChangeConvCallbacks(MakeFunctor(CSettingsManager::GetInstance(), &CSettingsManager::ChangeStereoConvergence));
#endif
}
#endif //RSG_PC

//
// name:        CGame::Update()
// description: Update CGame class
void CGame::Update()
{
#if COMMERCE_CONTAINER
	if (CLiveManager::GetCommerceMgr().ContainerIsStoreMode())
	{
		if (!CLiveManager::GetCommerceMgr().ContainerIsLoaded())
			cStoreScreenMgr::OpenInternalPhase2();

		return MarketplaceUpdate();
	}		
#endif

	// Update timebar doesn't play nicely with creating the game session from the new Landing Page.
	// Only track it once game session is initialized
	const bool bSessionInitializedBeforeThisFrame = CGame::IsSessionInitialized();
	if (bSessionInitializedBeforeThisFrame)
	{
		PF_PUSH_TIMEBAR_BUDGETED("Update", 20);
	}

	// force cycle the MWC random number generator every frame
	if (NetworkInterface::IsGameInProgress())
	{
		CRandomMWCScripts::ms_RandMWC.GetInt();
	}

#if RAGE_MEMORY_DEBUG
	// KEY_F11
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F11, KEYBOARD_MODE_OVERRIDE, "Print Memory Stats"))
	{
		sysMemDebug::GetInstance().PrintAllMemory();

		//////////////////////////////////////////////////////////////////////////////////////////////////////
		// NETWORK
		//
		Memoryf("\n");
		Memoryf("%24s %8s %8s %8s %8s %6s  %s\n", "NETWORK KB", "USED KB", "FREE KB", "TOTAL KB", "HIGH KB", "FRAG %", "ADDRESS RANGE");

		// Network
		sysMemAllocator* pAllocator = CNetwork::GetNetworkHeap();
		sysMemDebug::GetInstance().PrintAllocator("Network", pAllocator);

		pAllocator = g_rlAllocator;
		sysMemDebug::GetInstance().PrintAllocator("RLine", pAllocator);

		pAllocator = &CNetwork::GetSCxnMgrAllocator();
		sysMemDebug::GetInstance().PrintAllocator("SCxnMgr", pAllocator);
	}		
#endif

	// Complete the rest of the decal update, and start any new async work.
	// Want to do this fairly soon after the render thread safe zone from
	// previous frame has completed, so as to allow enough time for asynchronous
	// work to complete before the next render thread safe zone.
	PF_START_TIMEBAR_DETAIL("Vfx Decals - Start Async");
	g_decalMan.UpdateStartAsync(fwTimer::GetTimeStep());

#if __ASSERT
	g_timeCycle.InvalidateTimeCycle();
#endif // __ASSERT
    bool doFullUpdateRender = ShouldDoFullUpdateRender() VIDEO_PLAYBACK_ONLY( || VideoPlayback::IsPlaybackActive() );

    m_GameSessionStateMachine.Update();

#if GTA_REPLAY
	gVpMan.PushViewport(gVpMan.GetGameViewport());
	CReplayMgr::WaitForEntityStoring();
	CReplayMgr::PreProcess();
	gVpMan.PopViewport();
#endif

#if RSG_PC // Dynamically update CPU speed as it can change on PC with power management
	sysTimer::UpdateCpuSpeed();
#endif // RSG_PC

    gVpMan.PushViewport(gVpMan.GetGameViewport());
    {
        USE_MEMBUCKET(MEMBUCKET_SYSTEM);

		if (IsSessionInitialized())
		{
			m_GameSkeleton.Update(COMMON_MAIN_UPDATE);
		}
		else
		{
			m_GameSkeleton.Update(LANDING_PAGE_PRE_INIT_UPDATE);
		}

#if GEN9_LANDING_PAGE_ENABLED
		if ( CLandingPageArbiter::ShouldGameRunLandingPageUpdate() )
		{
			m_GameSkeleton.Update(LANDING_PAGE_UPDATE);
		}
		else
#endif // GEN9_LANDING_PAGE_ENABLED
		{
			CSavingMessage::DisplaySavingMessage();

			CGameWorld::AllowDelete(true);

			if (doFullUpdateRender)
			{
				m_GameSkeleton.Update(FULL_MAIN_UPDATE);
			}
			else
			{
				m_GameSkeleton.Update(SIMPLE_MAIN_UPDATE);
			}

			CGameWorld::AllowDelete(false); // disable world remove so that entities in render list aren't removed
		}
    }

    // $$$ END OF UPDATE... render type stuff now proceeds... NB. the behaviour with push and pop viewport contexts.
	gVpMan.PopViewport();

	if (bSessionInitializedBeforeThisFrame)
	{
		PF_POP_TIMEBAR(); // Update
	}

	PF_PUSH_TIMEBAR_BUDGETED("Visibility",NetworkInterface::IsGameInProgress()?3.0f:4.0f);
	PF_START_TIMEBAR_DETAIL("BuildRenderLists Init");

    if(doFullUpdateRender)
	{
		// reset high priority flag
		strStreamingEngine::SetIsLoadingPriorityObjects(false);

		CRenderer::ClearRenderLists();

		if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_VISIBILITY_JOBS) || !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_VISIBILITY_ENTITIES_JOBS) )
			gPostScan.Reset();

		// disable world remove so that entities in render list aren't removed
		CGameWorld::AllowDelete(false);

		if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_VISIBILITY_JOBS) )
		{
			PF_START_TIMEBAR_DETAIL("PreScan");
			gWorldMgr.PreScan();
		}

		// Build Phase Render Lists
		PF_START_TIMEBAR_DETAIL("BuildRenderLists");
		gVpMan.BuildRenderLists();

#if __PS3
		if( !fwTimer::IsGamePaused() )
		{
			g_vehicleGlassMan.PrepareCache();
		}
#endif // __PS3

		//PF_POP_TIMEBAR_DETAIL(); // build renderlist

		// Water::BlockTillUpdateComplete():
		// Please keep this call as last thing in CScene::Update().
		// Currently it waits ~800microsecs for update thread to finish.
		// Talk to JonhW if you want to change anything here.
		PF_START_TIMEBAR_DETAIL("Water block");
		PF_PUSH_TIMEBAR_BUDGETED("Water block",1.0f);
		//if (!fwTimer::IsGamePaused())
		Water::BlockTillUpdateComplete();
		PF_POP_TIMEBAR();

#if GTA_REPLAY
		//Process moved to before PreRender()
		// Attempt to create entities.  Hopefully some time will have been
		// given for any required models to be loaded
		CReplayMgr::PostProcess();
#endif


		PF_START_TIMEBAR_DETAIL("PreRender");
		// this should be the last thing done in update
		CRenderer::PreRender();

		PF_START_TIMEBAR_DETAIL("Verlet cloth Wait");
		CPhysics::WaitForVerletTasks();
		CPhysics::VerletSimPostUpdate();

		PF_START_TIMEBAR_DETAIL("Occlusion");

		COcclusion::BeforeRender();

	}
	else
	{
		CRenderer::ClearRenderLists();

	#if __XENON
		static dev_bool debugClearRenderLists=true;
		if (debugClearRenderLists==true)
		{			
			CRenderer::ClearRenderLists();
			gPostScan.Reset();
		}
	#endif

		Water::BlockTillUpdateComplete();

		// reset high priority flag
		strStreamingEngine::SetIsLoadingPriorityObjects(false);
	}

	PF_POP_TIMEBAR(); //visibility

	fwGame::Update();

#if __BANK
	COverviewScreenManager::Update();
	
	CPed::DrawDebugVisibilityQuery();

	if (PARAM_resvisualize.Get())
		CResourceVisualizerManager::GetInstance().Update();
#endif // __BANK
}

//---------------------------
// The new slimmer game render
// uses renderphases.
void CGame::Render()
{
#if GEN9_LANDING_PAGE_ENABLED
	if (!CGame::IsSessionInitialized())
	{
		// Game world has not loaded yet, nothing to do here.
		return;
	}

	if (Gen9LandingPageEnabled() && CLandingPage::IsActive())
	{
		// Landing page is active, the rest of the game is inactive
		return;
	}
#endif // GEN9_LANDING_PAGE_ENABLED

#if !__FINAL
	CDebugScene::Render_MainThread();
#endif

	WorldProbe::CShapeTestManager::KickAsyncWorker();

	CPostScan::WaitForPostScanHelper();

#if SORT_RENDERLISTS_ON_SPU
	gRenderListGroup.WaitForSortEntityListJob();
#endif	//SORT_RENDERLISTS_ON_SPU

	CPedVariationStream::AddFirstPersonEntitiesToRenderList();

#if __BANK
	CPostScanDebug::Update(); // this needs to be called after gRenderListGroup.WaitForSortEntityListJob to prevent a crash
#endif // __BANK

	gStreamingRequestList.RecordPvs();
#if __BANK
	gStreamingRequestList.UpdateListProfile();
#endif 

	PF_PUSH_TIMEBAR("Tex LOD");
	CTexLod::Update();
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Set High Priority Dummy Objects");
	CObjectPopulation::SetHighPriorityDummyObjects();
	PF_POP_TIMEBAR();

	//GOOD PLACE TO ADD ASYNC WORK. - We should have a few ms before and after building the draw list.
	//
	// Kick off async searches
	PF_PUSH_TIMEBAR("Start Async Searches");
	CObjectScanner::StartAsyncUpdateOfObjectScanners();
	audNorthAudioEngine::StartEnvironmentLocalObjectListUpdate();
	g_fireMan.StartFireSearches();
	PF_POP_TIMEBAR();

	USE_MEMBUCKET(MEMBUCKET_RENDER);
		PF_PUSH_TIMEBAR_BUDGETED("Build Drawlist", 3.0f);
		CSystem::AddDrawList_BeginRender();
		gVpMan.RenderPhases();
		CSystem::AddDrawList_EndRender();
		PF_POP_TIMEBAR();  // render

		// Wait for async searches
		PF_PUSH_TIMEBAR("End Async Searches");
		CObjectScanner::EndAsyncUpdateOfObjectScanners();
		g_fireMan.FinalizeFireSearches();
		//g_markers.DeleteAll();				// once the markers are all rendered we need to delete them all ready for the next frame's marker registration
		PF_POP_TIMEBAR();
	}

void CGame::TimerInit(unsigned /*initMode*/)
{
    fwTimer::InitLevel();
}

void CGame::ViewportSystemInit(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
        gVpMan.Init();
	    gVpMan.Add(rage_new CViewportPrimaryOrtho());
	    gVpMan.Add(rage_new CViewportGame());
	    gVpMan.Add(rage_new CViewportFrontend3DScene()); //Player settings.
    }
    else if(initMode == INIT_SESSION)
    {
	    gVpMan.InitSession();
    }
}

void CGame::ViewportSystemShutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        gVpMan.Shutdown();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    gVpMan.ShutdownSession();
    }
}

#if 0 // KS fixme
//
// name:		ScheduleImgs
// description:	Schedule list of imgs to be cached
void ScheduleImgs(char** ppImgs, s32 num)
{
	for(s32 j=0; j<num; j++)
	{
		char imgName[32];

		if(ppImgs[0] == '\0')
			continue;

		strcpy(imgName, ppImgs[j]);
		strcat(imgName, ".img");

		for(s32 k=0; k<strPackfileManager::GetImageFileCount(); k++)
		{
			strStreamingFile* pFile = strPackfileManager::GetImageFile(k);
			if(strstr(pFile->m_name, imgName))
			{
				pFile->m_device->ScheduleCaching();
				break;
			}
		}
	}

}
#endif // KS fixme

//
// name:		DiskCacheSchedule
// description:	Schedule the correct bits of map based on where the player starts
void CGame::DiskCacheSchedule()
{
#if 0 // // KS fixme
#define NUM_START_POINTS	6
#define NUM_IMGS			8
	struct {
		Vector3 posn;
		char* imgs[NUM_IMGS];
	} scheduleImgs[NUM_START_POINTS] = {
		// Game start point
		{Vector3(0,0,0), {"brook_s", "brook_s2", "brook_s3", "brook_n", "brook_n2", "", "", ""}},
		// brooklyn
		{Vector3(892.0f,-499.0f,18.0f), {"brook_s", "brook_s2", "brook_s3", "brook_n", "brook_n2", "", "", ""}},
		// bronx
		{Vector3(601.0f,1409.0f,16.0f), {"bronx_e", "bronx_e2", "bronx_w", "bronx_w2", "brook_n2", "", "", ""}},
		// manhat
		{Vector3(99.0f,851.0f,44.0f), {"manhat08", "manhat06", "manhat07", "manhat09", "manhat04", "manhat03", "manhat05", "manhat12"}},
		// jersey
		{Vector3(-969.0f,887.0f,18.0f), {"nj_02", "nj_01", "nj_03", "nj_docks", "nj_04e", "nj_04w", "", ""}},
		// playboy X
		{Vector3(-426.0f,1463.0f,38.0f), {"manhat05", "manhat12", "manhat06", "manhat04", "manhat08", "nj01", "bronx_w", ""}}
		};
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	Vector3 posn;

	// if there is no player ped assume we are starting at the beginning of the game
	if(pPlayerPed == NULL)
		ScheduleImgs(scheduleImgs[0].imgs, NUM_IMGS);
	else
	{
		Assert(pPlayerPed);
		posn = pPlayerPed->GetPosition();

		for(s32 i=0; i<NUM_START_POINTS; i++)
		{
			Vector3 dist = posn - scheduleImgs[i].posn;
			// If within 50 metres of start point
			if(dist.Mag() < 50.0f)
			{
				ScheduleImgs(scheduleImgs[i].imgs, NUM_IMGS);
			}
		}
	}
#endif // KS fixme
}


// name:		CGame::OnPresenceEvent
// description:	Deal with profiles signing in and out
void CGame::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if(PRESENCE_EVENT_SIGNIN_STATUS_CHANGED == evt->GetId())
	{
		const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;

		// If signed in...
		if(s->SignedIn())
		{
			gameDebugf1("### Event Sign In ###");
		}
		// If signing out...
		else if(s->SignedOut())
		{
			gameDebugf1("### Event Sign Out ###");

			if (IsChangingSessionState())
			{
				gameWarningf("Event Sign Out - Game is loading - Full Restart Needed...");
			}
		}
	}
}

#if RSG_ORBIS
void CGame::OnNpEvent(rlNp* /*np*/, const rlNpEvent* evt)
{
	// Various game services need to be made aware of user service status changes, as the Orbis SDK calls will fail.
	if (RLNP_EVENT_USER_SERVICE_STATUS_CHANGED)
	{
		rlNpEventUserServiceStatusChanged* msg = (rlNpEventUserServiceStatusChanged*)evt;
		if (!msg)
			return;

		// Update the SAVEGAME to be aware that the user has connected or disconnected. 
		SAVEGAME.SetUserServiceId(msg->m_LocalGamerIndex, msg->m_UserServiceId);

		// Update ioPad to either initialize or shutdown the pad
		ioPad::GetPad(msg->m_LocalGamerIndex).HandleUserServiceStatusChange(msg->m_UserServiceId);
	}
}

void CGame::SaveGameSetup()
{
	for (int i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
	{
		int userId = g_rlNp.GetUserServiceId(i);
		SAVEGAME.SetUserServiceId(i, userId);
#if USE_SAVE_DATA_MEMORY
		//SAVEGAME.SetSaveDataMemory(i);
#endif
	}
}

#endif

void CGame::GameExceptionDisplay()
{
#if __BANK
	diagContextMessage::DisplayContext(TPurple);
#endif

#if !__NO_OUTPUT
	// Keep in mind this function can be called from any thread, when some systems are dead or dying
	// so keep the access simple and don't call functions. 
	Vector3 playerPos = CPlayerInfo::ms_cachedMainPlayerPos;
	Vector3 camPos  = camInterface::GetPos();
	Vector3 camDir = camInterface::GetFront();

	Displayf("================================");
	Displayf("GameState");
	Displayf("\tPlayer pos: %f, %f %f", playerPos.x, playerPos.y, playerPos.z);
	Displayf("\tCamera pos: %f, %f %f", camPos.x, camPos.y, camPos.z);
	Displayf("\tCamera dir: %f, %f %f", camDir.x, camDir.y, camDir.z);
	Displayf("\tNumber of peds: %d", CPed::GetPool()?CPed::GetPool()->GetNoOfUsedSpaces():0);
	Displayf("\tNumber of vehicles: %d", CVehicle::GetPool() ? (int) CVehicle::GetPool()->GetNoOfUsedSpaces() : 0);
#if __BANK
	diagContextMessage::DisplayContext("...");
#endif
	Displayf("/GameState");
	Displayf("================================");
#endif // !__NO_OUTPUT

#if DR_ENABLED
	const char *pOutputOnExceptionFile="x:\\DR_ExceptionEvents.pso";
	if (PARAM_DR_AutoSaveOExcetion.Get(pOutputOnExceptionFile))
	{
		Displayf("Writing exception events to '%s'",pOutputOnExceptionFile);

		if (debugPlayback::DebugRecorder::GetInstance())
		{
			debugPlayback::RecordCurrentDebugContext();

			debugPlayback::DebugRecorder::GetInstance()->Save(pOutputOnExceptionFile);
		}
		else
		{
			Errorf("FAILED TO FIND DEBUGRECORDER TO WRITE LOG");
		}
		Displayf("================================");
	}
#endif
}

void CGame::OutOfMemoryDisplay()
{
#if !__NO_OUTPUT
	// Keep in mind this function can be called from any thread, when some systems are dead or dying
	// so keep the access simple and don't call functions.
	Vector3 playerPos = CPlayerInfo::ms_cachedMainPlayerPos;
	Vector3 camPos  = camInterface::GetPos();
	Vector3 camDir = camInterface::GetFront();

	Displayf("================================");
	Displayf("OutOfMemory");
	Displayf("\tPlayer pos: %f, %f %f", playerPos.x, playerPos.y, playerPos.z);
	Displayf("\tCamera pos: %f, %f %f", camPos.x, camPos.y, camPos.z);
	Displayf("\tCamera dir: %f, %f %f", camDir.x, camDir.y, camDir.z);
	Displayf("\tNumber of peds: %d", CPed::GetPool()?CPed::GetPool()->GetNoOfUsedSpaces():0);
	Displayf("\tNumber of vehicles: %d", CVehicle::GetPool() ? (int) CVehicle::GetPool()->GetNoOfUsedSpaces() : 0);
	Displayf("");

	// Skeletons
	Displayf("[CVehicle Skeletons]");
	CVehicle::PrintSkeletonData();

	Displayf("[CPed Skeletons]");
	CPed::PrintSkeletonData();

	Displayf("[CObject Skeletons]");
	CObject::PrintSkeletonData();

	Displayf("[CAnimatedBuilding Skeletons]");
	CAnimatedBuilding::PrintSkeletonData();

#if __BANK
	diagContextMessage::DisplayContext("...");
#endif
	Displayf("/OutOfMemory");
	Displayf("================================");
#endif // !__NO_OUTPUT
}

#if __ASSERT
void CGame::VerifyMainThread()
{
	// This must be called from MainThread
	Assert(sysThreadType::IsUpdateThread());
}
#endif

// EOF
