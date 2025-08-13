//
// filename:	commands_streaming.cpp
// description:	Commands for streaming 
//

// --- Include Files ------------------------------------------------------------

#include "script/commands_streaming.h"

// C headers
// Rage headers
#include "script/hash.h"
#include "script/wrapper.h"
// Framework headers
#include "fwscene/stores/mapdatastore.h"
#include "fwanimation/anim_channel.h"
#include "fwanimation/animmanager.h"
#include "fwrenderer/renderthread.h"
#include "fwscene/stores/imapgroup.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/staticboundsstore.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxmanager.h"
// Game headers
#include "camera/CamInterface.h"
#include "camera/switch/SwitchDirector.h"
#include "control/replay/Effects/ParticleMiscFxPacket.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelInfo/PedModelInfo.h"
#include "modelInfo/MloModelInfo.h"
#include "modelInfo/VehicleModelInfo.h"
#include "network/NetworkInterface.h"
#include "scene/entities/compEntity.h"
#include "scene/FocusEntity.h"
#include "scene/LoadScene.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/WarpManager.h"
#include "scene/world/GameWorldHeightMap.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "script/script_helper.h"
#include "streaming/IslandHopper.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "scene/portals/portal.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodScale.h"
#include "fwsys/timer.h"
#include "scene/ipl/IplCullBox.h"
#include "scene/texLod.h"
#include "spatialdata/sphere.h"
#include "vfx/systems/vfxscript.h"
#include "peds/ped.h"
#include "renderer/HorizonObjects.h"
#include "vehicleai/pathfind.h"
#include "control/gamelogic.h"
#include "peds/PlayerInfo.h"
#include "vfx/particles/PtFxManager.h"
#include "system/FileMgr.h"

#if __BANK
#include "debug/Rendering/DebugLighting.h"
#endif

#if __ASSERT
#include "frontend/PauseMenu.h"
#include "frontend/loadingscreens.h"
#endif

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

#if __ASSERT
u32 g_lastLoadSceneFrame = 0;
class scrThread* g_lastLoadThread = NULL;
Vector3 g_lastLoadSceneCoord(0.0f, 0.0f, 0.0f);
#define SET_LOAD_SCENE_FRAME(vCoord) { g_lastLoadSceneFrame = fwTimer::GetSystemFrameCount(); g_lastLoadThread = scrThread::GetActiveThread(); g_lastLoadSceneCoord=vCoord;}
#define ASSERT_NO_LOAD_SCENE(cmd) Assertf(g_lastLoadSceneFrame != fwTimer::GetSystemFrameCount() || g_lastLoadThread != scrThread::GetActiveThread(), "SCRIPT: %s Running %s after a load scene could remove some of the scene you have just loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cmd);
#define ASSERT_NO_LOAD_SCENE_AT_COORDS(cmd, vCoord) Assertf(g_lastLoadSceneFrame != fwTimer::GetSystemFrameCount() || g_lastLoadThread != scrThread::GetActiveThread() || (g_lastLoadSceneCoord.Dist(vCoord) < 5.0f), "SCRIPT: %s Running %s after a load scene could remove some of the scene you have just loaded. Original Coord=%f,%f,%f, New Coord=%f,%f,%f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cmd, g_lastLoadSceneCoord.x, g_lastLoadSceneCoord.y, g_lastLoadSceneCoord.z, vCoord.x, vCoord.y, vCoord.z);
#else
#define SET_LOAD_SCENE_FRAME(vCoord)
#define ASSERT_NO_LOAD_SCENE(cmd)
#define ASSERT_NO_LOAD_SCENE_AT_COORDS(cmd, vCoord)
#endif
// --- Code ---------------------------------------------------------------------

namespace streaming_commands
{
u32 g_additionalStreamingRequestFlags=0;

//PURPOSE: Does checks on HasLoaded requests from streaming
class CScriptStreamingChecker
{
	struct HasLoadedRecord
	{
		HasLoadedRecord() {numberOfChecks = 0;}

		bool IsOld() {return fwTimer::GetSystemFrameCount() - lastCheckTime > 1;}
		void IncrementCheck() {lastCheckTime = fwTimer::GetSystemFrameCount(); numberOfChecks++;}

		int lastCheckTime;
		int numberOfChecks;
	};

public:
	bool HasObjectLoaded(strLocalIndex objId, int moduleId);
	void RemoveEntry(int id) {m_hasLoadedRecordMap.Delete(id);}
	void RemoveOld()
	{
		atMap<int, HasLoadedRecord>::Iterator iHasLoaded = m_hasLoadedRecordMap.CreateIterator();

		while(!iHasLoaded.AtEnd())
		{
			if(iHasLoaded.GetData().IsOld())
			{
				m_hasLoadedRecordMap.Delete(iHasLoaded.GetKey());
				iHasLoaded.Start();
			}
			else
				iHasLoaded.Next();
		}
	}
	void IncrementCheck(int id) {m_hasLoadedRecordMap[id].IncrementCheck();}
	int GetNumberOfChecks(int id) {return m_hasLoadedRecordMap[id].numberOfChecks;}

private:
	atMap<int, HasLoadedRecord> m_hasLoadedRecordMap;
};


bool CScriptStreamingChecker::HasObjectLoaded(strLocalIndex objId, int moduleId)
{
#if __ASSERT
	int streamingId = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId)->GetStreamingIndex(strLocalIndex(objId)).Get();

	RemoveOld();
	IncrementCheck(streamingId);
	int checks = GetNumberOfChecks(streamingId);

	if(checks > 100 && (checks%20) == 0)
		Displayf("Waiting on %s to load (%d times)", strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId)->GetName(strLocalIndex(objId)), checks);
#endif

	bool rt = CStreaming::HasObjectLoaded(objId, moduleId);
#if __ASSERT
// Graeme already checks for this so this isnt necessary
//	scriptAssertf(rt == true || CStreaming::IsObjectRequested(objId, moduleId) || CStreaming::IsObjectLoading(objId, moduleId), 
//		"You must request %s before checking if it has loaded", strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId)->GetName(objId));

	if(rt)
		RemoveEntry(streamingId);
#endif
	return rt;
}

static CScriptStreamingChecker g_streamingChecker;

//
// name:		CommandRequestModel
// description:	Request a model to be loaded
void CommandRequestModel(int ModelHashKey)
{
	ASSERT_NO_LOAD_SCENE("REQUEST_MODEL");

	fwModelId ModelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);

	if (scriptVerifyf(pModelInfo, "%s: REQUEST_MODEL - model with hash %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey))
	{
		if (scriptVerifyf(ModelId.IsValid(), "%s: REQUEST_MODEL - model with hash %d exists but its model index is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ModelHashKey))
		{
			CScriptResource_Model model(ModelHashKey, g_additionalStreamingRequestFlags);

			bool bHasJustBeenAdded = false;
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(model, &bHasJustBeenAdded);

			if (bHasJustBeenAdded)
			{
				scriptHandler *pAnotherScriptWhichHasRequestedThisModel = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerForResource(CGameScriptResource::SCRIPT_RESOURCE_MODEL, ModelHashKey, CTheScripts::GetCurrentGtaScriptHandler());
				if (pAnotherScriptWhichHasRequestedThisModel == NULL)
				{
					fwArchetypeManager::AddArchetypeToTypeFileRef(pModelInfo);
				}
				else
				{
					scriptDisplayf("%s: REQUEST_MODEL - another script (%s) has already requested the model with hash %d so don't call AddArchetypeToTypeFileRef() for it", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pAnotherScriptWhichHasRequestedThisModel->GetLogName(), ModelHashKey);
				}
			}

			g_additionalStreamingRequestFlags = 0;
		}
	}
}

void CommandRequestMenuPedModel(int ModelHashKey)
{
	g_additionalStreamingRequestFlags |= STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD;
	CommandRequestModel(ModelHashKey);
}

//
// name:		HasModelLoaded
// description:	Return if a model has loaded
bool HasModelLoaded(int ModelHashKey)
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHashKey, &modelId);

	if(SCRIPT_VERIFY( modelId.IsValid(), "HAS_MODEL_LOADED - this is not a valid model index"))
	{
		if(scriptVerifyf(pModelInfo, "HAS_MODEL_LOADED - Model [%d] does not exist", ModelHashKey))
		{
#if !__FINAL
			if (CTheScripts::GetCurrentGtaScriptHandler())
			{
				scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MODEL, ModelHashKey), "HAS_MODEL_LOADED - %s has not requested model %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CModelInfo::GetBaseModelInfoName(modelId) );
			}
#endif
			// if not a MLO then check the object, otherwise check all objects in room 0 of MLO
			if (pModelInfo->GetModelType() == MI_TYPE_MLO)
			{
				return(static_cast<CMloModelInfo*>(pModelInfo)->HaveObjectsInRoomLoaded("limbo"));			// "limbo" is always the name of room 0
			}
			else
			{
				strLocalIndex transientLocalIdx = CModelInfo::LookupLocalIndex(modelId);
				return (g_streamingChecker.HasObjectLoaded(transientLocalIdx, CModelInfo::GetStreamingModuleId()));
			}
		}
	}
	return false;
}

//
// name:		RequestInteriorModels
// description:	Request that all the models in the given room load in
void RequestInteriorModels(int ModelHashKey, const char* pRoomName)
{
	fwModelId ModelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);

	if(SCRIPT_VERIFY( ModelId.IsValid(), "REQUEST_INTERIOR_MODELS - this is not a valid model index"))
	{
		if(SCRIPT_VERIFY( (pModelInfo->GetModelType() == MI_TYPE_MLO), "REQUEST_INTERIOR_MODELS - model is not an interior"))
		{
			static_cast<CMloModelInfo*>(pModelInfo)->RequestObjectsInRoom("limbo", g_additionalStreamingRequestFlags);	 // always attempt to load room 0 objects too
			static_cast<CMloModelInfo*>(pModelInfo)->RequestObjectsInRoom((char *) pRoomName, g_additionalStreamingRequestFlags);	
		}
	}
	g_additionalStreamingRequestFlags = 0;
}

// issue model requests for the given room in this interior
void RequestModelsInRoom(int InteriorProxyIndex, const char* roomName)
{
	if (SCRIPT_VERIFY( InteriorProxyIndex != NULL_IN_SCRIPTING_LANGUAGE, "REQUEST_MODELS_IN_ROOM - Interior index isn't valid"))
	{
		// get the interior 
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		if (SCRIPT_VERIFY(pIntProxy, "REQUEST_MODELS_IN_ROOM - Interior proxy doesn't exist"))
		{
			// get the interior 
			CInteriorInst *pIntInst = pIntProxy->GetInteriorInst();
			if (SCRIPT_VERIFY(pIntInst, "REQUEST_MODELS_IN_ROOM - Interior is not yet populated"))
			{
				s32 roomId = pIntInst->FindRoomByHashcode(atHashString(roomName));
				if (scriptVerifyf(roomId > -1, "%s: REQUEST_MODELS_IN_ROOM - unknown room (%s) in interior (%s)", 
													CTheScripts::GetCurrentScriptNameAndProgramCounter(), roomName, pIntProxy->GetName().GetCStr()))
				{
					pIntInst->RequestRoom(roomId, g_additionalStreamingRequestFlags);
				}
			}
		}
	}
}

//
// name:		LoadSceneForRoomByKey
// description:	Loads the room models 
void LoadSceneForRoomByKey(int interiorInstance, int roomKey)
{
	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(interiorInstance);
	Assert(pIntProxy);
	CInteriorInst* pInterior = pIntProxy->GetInteriorInst();

	if(SCRIPT_VERIFY(pInterior, "LOAD_SCENE_FOR_ROOM_BY_KEY - Interior doesn't exist"))
	{
		s32 roomIndex = pInterior->FindRoomByHashcode(roomKey);
		if(SCRIPT_VERIFY(roomIndex > 0, "LOAD_SCENE_FOR_ROOM_BY_KEY - room key invalid"))
		{
			g_SceneStreamerMgr.LoadInteriorRoom(pInterior, roomIndex);
		}
	}
}

void CommandActivateInterior(int interiorInstance, bool bActivate)
{
	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(interiorInstance);
	Assert(pIntProxy);
	CInteriorInst* pInterior = pIntProxy->GetInteriorInst();

	if(SCRIPT_VERIFY(pInterior, "SET_INTERIOR_ACTIVE - Interior doesn't exist"))
	{
		if (bActivate)
		{
//			CPortal::AddToActiveInteriorList(pInterior);
		}
		else
		{
//			CPortal::RemoveFromActiveInteriorList(pInterior);
		}
	}
}

//
// name:		CommandMarkModelAsNoLongerNeeded
// description:	Set model to be deleteable
void CommandMarkModelAsNoLongerNeeded(int ModelHashKey)
{
	fwModelId ModelId;	
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);	

	if (SCRIPT_VERIFY( pModelInfo, "SET_MODEL_AS_NO_LONGER_NEEDED - model does not exist"))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MODEL, ModelHashKey);
		}
	}
}

//
// name:		RequestCollisionAtPosn
// description:	Request collision about point is loaded
void RequestCollisionAtPosn(const scrVector & scrVecCoors)
{
	ASSERT_ONLY(Vector3 vCoord(scrVecCoors);)
	ASSERT_NO_LOAD_SCENE_AT_COORDS("REQUEST_COLLISION_AT_COORD", vCoord);
	g_StaticBoundsStore.GetBoxStreamer().Deprecated_AddSecondarySearch((Vec3V) scrVecCoors);
}

//
// name:		RequestMoverCollisionOnly
// description:	sets the standard collision streaming to only request mover collision and ignore weapon collision.
//				should be called each frame, otherwise we revert to requesting both weapon and mover collision
void RequestMoverCollisionOnly()
{
	g_StaticBoundsStore.GetBoxStreamer().OverrideRequiredAssets(fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
}

//
// name:		RequestCollision
// description:	Request that collision (physics dictionary) be loaded
void RequestCollisionForModel(int ModelHashKey)
{
	CommandRequestModel(ModelHashKey);
}

bool HasCollisionForModelLoaded(int ModelHashKey)
{
	fwModelId ModelId;	
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);		//	ignores return value
	if(SCRIPT_VERIFY( ModelId.IsValid(), "HAS_COLLISION_FOR_MODEL_LOADED - this is not a valid model index"))
	{
		if (pModelInfo->GetHasLoaded() && pModelInfo->GetHasBoundInDrawable())
			return true;

		return (pModelInfo->GetPhysics() != NULL);
	}
	return false;
}

//
// name:		DoesAnimDictExist
// description:	Return if animation dictionary exists
bool DoesAnimDictExist(const char* pName)
{
	strLocalIndex index = fwAnimManager::FindSlot(pName);
	if(index.IsValid())
	{
		return true;
	}
	return false;
}

//
// name:		RequestAnimDict
// description:	Request that animation dictionary be loaded
void RequestAnimDict(const char* pName)
{
	ASSERT_NO_LOAD_SCENE("REQUEST_ANIMS");

	strLocalIndex index = fwAnimManager::FindSlot(pName);
	animDebugf3("REQUEST_ANIM_DICT - %s - Anim Dict %s (Slot index: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, index.Get());
	if(SCRIPT_VERIFY_TWO_STRINGS(index.IsValid(), "REQUEST_ANIM_DICT - Anim dict name does not exist: ", pName))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			CScriptResource_Animation animation(index.Get(), g_additionalStreamingRequestFlags);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(animation);
		}
	}

	g_additionalStreamingRequestFlags = 0;
}

//
// name:		HasAnimDictLoaded
// description:	Return if animation dictionary is in memory
bool HasAnimDictLoaded(const char* pName)
{
	strLocalIndex index = fwAnimManager::FindSlot(pName);
	animDebugf3("HAS_ANIM_DICT_LOADED - %s - Anim Dict %s (Slot index: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, index.Get());
	
	if(SCRIPT_VERIFY_TWO_STRINGS(index != -1, "HAS_ANIM_DICT_LOADED - Anim dict name does not exist: ", pName))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ANIMATION, index.Get()), "HAS_ANIM_DICT_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName);
		}

		return g_streamingChecker.HasObjectLoaded(index, fwAnimManager::GetStreamingModuleId());
	}
	return false;
}

//
// name:		RemoveAnimDict
// description:	Remove animation dictionary
void RemoveAnimDict(const char* pName)
{
	s32 index = fwAnimManager::FindSlot(pName).Get();
	animDebugf3("REMOVE_ANIM_DICT - %s - Anim Dict %s (Slot index: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, index);
		
	if(SCRIPT_VERIFY_TWO_STRINGS(index != -1, "REMOVE_ANIM_DICT - Anim dict name does not exist: ", pName))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ANIMATION, index);
		}
	}
}

//
// name:		RequestClipSet
// description:	Requests a clipset (and its fallbacks) to load
void RequestClipSet(const char* pName)
{
	ASSERT_NO_LOAD_SCENE("REQUEST_CLIP_SET");

	fwMvClipSetId clipSetId(pName);
	animDebugf3("REQUEST_CLIP_SET - %s - ClipSet %s (ClipSetId: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, clipSetId.GetHash());

	fwClipSet* pSet = fwClipSetManager::GetClipSet(clipSetId);
	if (SCRIPT_VERIFY_TWO_STRINGS(pSet != NULL, "REQUEST_CLIP_SET - Invalid ClipSet name: ", pName))
	{
		scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLIP_SET, clipSetId);
		if (!pResource)
		{
			CScriptResource_ClipSet newClipSetResource(clipSetId);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(newClipSetResource);
			fwClipSetManager::Script_AddAndRequestClipSet(clipSetId);
		}
		else
		{
			fwClipSetManager::Script_RequestClipSet(clipSetId);
		}		
	}
}

//
// name:		HasClipSetLoaded
// description:	Return if the clipset is in memory
bool HasClipSetLoaded(const char* pName)
{
	fwMvClipSetId clipSetId(pName);

	fwClipSet* pSet = fwClipSetManager::GetClipSet(clipSetId);
	animDebugf3("HAS_CLIP_SET_LOADED - %s - ClipSet %s (ClipSetId: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, clipSetId.GetHash());
	
	if (SCRIPT_VERIFY_TWO_STRINGS(pSet != NULL, "HAS_CLIP_SET_LOADED - Invalid ClipSet name: ", pName))
	{
		scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLIP_SET, clipSetId);
		if (pResource)
		{
			return fwClipSetManager::Script_HasClipSetLoaded(clipSetId);
		}
		else
		{
			scriptWarningf("HAS_CLIP_SET_LOADED - Could not find clip set resource for %s - was this clipset requested?", clipSetId.TryGetCStr());
		}
	}
	return false;
}

//
// name:		RemoveClipSet
// description:	Remove/Unload a clipset (and its fallbacks).
void RemoveClipSet(const char* pName)
{
	fwMvClipSetId clipSetId(pName);

	fwClipSet* pSet = fwClipSetManager::GetClipSet(clipSetId);
	animDebugf3("REMOVE_CLIP_SET - %s - ClipSet %s (ClipSetId: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, clipSetId.GetHash());
	
	if (SCRIPT_VERIFY_TWO_STRINGS(pSet != NULL, "REMOVE_CLIP_SET - Invalid ClipSet name: ", pName))
	{
		scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLIP_SET, clipSetId);
		if (pResource)
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLIP_SET, clipSetId, false, true);
		}
		else
		{
			scriptWarningf("REMOVE_CLIP_SET - Could not find clip set resource for %s - was this clipset requested?", clipSetId.TryGetCStr());
		}
	}
}


//
// name:		RequestIpl
// description:	Request Ipl file
void RequestIpl(const char* pName)
{
	s32 index = INSTANCE_STORE.FindSlot(pName).Get();
	if(SCRIPT_VERIFY_TWO_STRINGS(index != -1, "REQUEST_IPL - IPL group does not exist", pName))
	{
		u32 iplNameHash = atStringHash(pName);
		Displayf("%s called REQUEST_IPL(%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName);
		scriptAssertf(!g_LoadScene.IsActive(), "[script] Don't modify Ipl groups (%s) whilst load scene is active! (Load scene triggered by %s)", pName, g_LoadScene.WasStartedByScript() ? g_LoadScene.GetScriptName().c_str() : "CODE");
		INSTANCE_STORE.RequestGroup(strLocalIndex(index), iplNameHash);
		CCompEntity::UpdateCompEntitiesUsingGroup(index, CE_STATE_INIT);

		// url:bugstar:6571961 - enable the heightmap specifically for the heist island
		if(iplNameHash == ATSTRINGHASH("h4_islandx_mansion", 0x9a95c101))
		{
			CGameWorldHeightMap::EnableHeightmap("heightmapheistisland", true);
		}
	}
	
}

//
// name:		RemoveIpl
// description:	Remove Ipl file
void RemoveIpl(const char* pName)
{
	strLocalIndex index = strLocalIndex(INSTANCE_STORE.FindSlot(pName));
	if(SCRIPT_VERIFY_TWO_STRINGS(index != -1, "REMOVE_IPL - IPL group does not exist", pName))
	{
		u32 iplNameHash = atStringHash(pName);
		Displayf("%s called REMOVE_IPL(%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName);
		scriptAssertf(!g_LoadScene.IsActive(), "[script] Don't modify Ipl groups (%s) whilst load scene is active! (Load scene triggered by %s)", pName, g_LoadScene.WasStartedByScript() ? g_LoadScene.GetScriptName().c_str() : "CODE");
		CCompEntity::UpdateCompEntitiesUsingGroup(index.Get(), CE_STATE_ABANDON);

		//////////////////////////////////////////////////////////////////////////
		fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(index);
		if (pDef && (pDef->GetContentFlags()&fwMapData::CONTENTFLAG_MASK_LODLIGHTS)!=0)
		{
#if __ASSERT
			const bool bGameIsNotRendering = (camInterface::IsFadedOut() || CLoadingScreens::AreActive() || CPauseMenu::GetPauseRenderPhasesStatus());
			// special case - these types of IMAP files contain data that is consumed by GPU so may need to force RT flush
			if (!bGameIsNotRendering)
			{
				scriptWarningf("Script %s called REMOVE_IPL(%s) but the screen is not faded out - this is unsafe because %s contains LOD lights",
					CTheScripts::GetCurrentScriptNameAndProgramCounter(), pName, pName );
			}
#endif
			gRenderThreadInterface.Flush();
		}
		//////////////////////////////////////////////////////////////////////////

		INSTANCE_STORE.RemoveGroup(index, iplNameHash);

		// url:bugstar:6571961 - disable the heightmap specifically for the heist island
		if(iplNameHash == ATSTRINGHASH("h4_islandx_mansion", 0x9a95c101))
		{
			CGameWorldHeightMap::EnableHeightmap("heightmapheistisland", false);
		}
	}
}

//
// name:		IsIplActive
// description:	return if the named ipl is set to be streamable or not
bool IsIplActive(const char* pName)
{
	strLocalIndex index = strLocalIndex(INSTANCE_STORE.FindSlot(pName));
	if(SCRIPT_VERIFY_TWO_STRINGS(index != -1, "IS_IPL_ACTIVE - IPL group does not exist", pName))
	{
		return(INSTANCE_STORE.GetIsStreamable(index));
	}
	return(false);
}


//
// Loads selected water.xml file:
// 0 - main V water file
// 1 - Island Heist DLC water file
//
void LoadGlobalWaterFile(s32 waterXmlID)
{
	Assertf((waterXmlID==Water::WATERXML_V_DEFAULT) || (waterXmlID==Water::WATERXML_ISLANDHEIST), "LOAD_GLOBAL_WATER_FILE: Invalid waterID id (%d)!", waterXmlID);
	Water::RequestGlobalWaterXmlFile(waterXmlID);
}

//
//
//
//
s32 GetGlobalWaterFile()
{
	return Water::GetGlobalWaterXmlFile();
}


//
// name:		LoadAllObjectsNow
// description:	Loads all the objects on the request list
void LoadAllObjectsNow()
{
	Assertf(!camInterface::IsFadedIn(), "Don't call LOAD_ALL_OBJECTS_NOW while the game isn't faded out - it will cause stutters on disc builds.");

	// Ensure that all requests are issued this frame
	ThePaths.UpdateStreaming(true);

	CStreaming::LoadAllRequestedObjects();
}

//
// name:		LoadScene
// description:	Load scene about point
void LoadScene(const scrVector & scrVecCoors)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling LOAD_SCENE while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		SCRIPT_ASSERT( !NetworkInterface::IsGameInProgress(), "You should use NETWORK_START_LOAD_SCENE/NETWORK_UPDATE_LOAD_SCENE in network games!");

		Vector3 VecCoors (scrVecCoors);
		ASSERT_NO_LOAD_SCENE_AT_COORDS("LOAD_SCENE", VecCoors);
		Printf("Script \"%s\" requested a load scene at %f,%f,%f\n", CTheScripts::GetCurrentScriptName(), VecCoors.x, VecCoors.y, VecCoors.z);
		g_SceneStreamerMgr.LoadScene(VecCoors);
		SET_LOAD_SCENE_FRAME(VecCoors);
	}
}

//
// name:		StartLoadScene
// description:	Load scene about point
void StartLoadScene(const scrVector & scrVecCoors)
{
	SCRIPT_ASSERT(false, "Script is calling NETWORK_START_LOAD_SCENE. Don't use that, use the NEW_LOAD_SCENE commands instead. Game may crash soon.");
	
	Vector3 VecCoors (scrVecCoors);
	ASSERT_NO_LOAD_SCENE_AT_COORDS("NETWORK_START_LOAD_SCENE", VecCoors);
	Printf("Script \"%s\" requested a load scene at %f,%f,%f\n", CTheScripts::GetCurrentScriptName(),VecCoors.x, VecCoors.y, VecCoors.z);
	g_SceneStreamerMgr.StartLoadScene(VecCoors);
}

//
// name:		UpdateLoadScene
// description:	Load scene about point
bool UpdateLoadScene()
{
	return g_SceneStreamerMgr.UpdateLoadScene();
}

// name:		StopLoadScene
// description:	abandon an async load scene if there is one in progress
void StopLoadScene()
{ 
	Printf("Script %s called NETWORK_STOP_LOAD_SCENE\n", CTheScripts::GetCurrentScriptName());

	// if (g_SceneStreamerMgr.IsLoading())
	{
		g_SceneStreamerMgr.StopLoadScene();
	}
}

bool IsLoadSceneActive()
{
	return g_SceneStreamerMgr.IsLoading();
}

//
// name:		IsModelInCdImage
// description:	Returns if model is currently valid
bool IsModelInCdImage(s32 modelHash)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
	if (!modelId.IsValid())
	{
		return false;
	}

	return true;
}


bool CommandIsModelValid(s32 modelHash)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
	if (modelId.IsValid())
	{
		if (modelId.IsGlobal())
		{
			return true;
		}
	}

	return false;
}

bool CommandGetPedModelFromIndex(int iPedModelIndex, int &ReturnPedModelHashKey)
{
	ReturnPedModelHashKey = 0;

//	Returns false if there is no ped model with that index. 
//	iPedModelIndex starts at 0 and counts up till it returns false.

	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypesArray;
	pedModelInfoStore.GatherPtrs(pedTypesArray);

	int numPedsAvail = pedTypesArray.GetCount();
	if (iPedModelIndex >= numPedsAvail)
	{
		return false;
	}
	CPedModelInfo& pedModelInfo = *pedTypesArray[iPedModelIndex];
	ReturnPedModelHashKey = (int) pedModelInfo.GetHashKey();
	return true;
}

bool CommandGetVehicleModelFromIndex(int iVehicleModelIndex, int &ReturnVehicleModelHashKey)
{
	ReturnVehicleModelHashKey = 0;

//	Returns false if there is no vehicle model with that index. 
//	iVehicleModelIndex starts at 0 and counts up till it returns false.

	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehicleModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypes;
	vehicleModelInfoStore.GatherPtrs(vehicleTypes);

	int numVehiclesAvail = vehicleTypes.GetCount();
	if (iVehicleModelIndex >= numVehiclesAvail)
	{
		return false;
	}

	CVehicleModelInfo& vehicleModelInfo = *vehicleTypes[iVehicleModelIndex];
	ReturnVehicleModelHashKey = (int) vehicleModelInfo.GetHashKey();
	return true;
}

bool CommandIsThisModelAPed(int ModelHashKey)
{
	bool LatestCmpFlagResult = false;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, NULL);

	if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_PED)
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}


bool CommandIsThisModelAVehicle(int ModelHashKey)
{
	bool LatestCmpFlagResult = false;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, NULL);

	if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
	{
		LatestCmpFlagResult = true;
	}
	return LatestCmpFlagResult;
}


//
// name:		SwitchStreaming
// description:	Switch streaming on and off
void SwitchStreaming(bool bOn)
{
	if (bOn)
	{
		CStreaming::EnableStreaming();
	}
	else
	{
		CStreaming::DisableStreaming();
	}
}

//
// name:		EnableSceneStreaming
// description:	Enable/Disable scene streaming
void EnableSceneStreaming(bool bEnable)
{
	CStreaming::EnableSceneStreaming(bEnable);
}

// Ensure game loads collision and ipls around an additional point. This function needs calling every frame the 
// additional data is required
void AddNeededAtPosn(const scrVector & scrVecCoors)
{
 	g_StaticBoundsStore.GetBoxStreamer().Deprecated_AddSecondarySearch((Vec3V)scrVecCoors);
}

// This should only be called by the end credits script. The credits will call it with FALSE at the start and 
//	TRUE at the end. This is to stop the scrolling of the credits from pausing when the player warps from 
//	one area of the city to another during the screen fades.
void AllowGameToPauseForStreaming(bool bAllowPause)
{
	CTheScripts::SetAllowGameToPauseForStreaming(bAllowPause);
}

void SetReducePedModelBudget(bool bReduce)
{
	gPopStreaming.SetReduceAmbientPedModelBudget(bReduce);
}

void SetReduceVehicleModelBudget(bool bReduce)
{
	gPopStreaming.SetReduceAmbientVehicleModelBudget(bReduce);
}

void SetDitchPoliceModels(bool UNUSED_PARAM(bDitch))
{
}



s32 GetNumStreamingRequests()
{
	return strStreamingEngine::GetInfo().GetNumberRealObjectsRequested();
}

bool IsStreamingPriorityRequests()
{
	return strStreamingEngine::GetIsLoadingPriorityObjects();
}



//
// name:		RequestPtFxAsset
// description:	Request the particle asset be loaded
void RequestPtFxAsset()
{
	ASSERT_NO_LOAD_SCENE("REQUEST_PTFX_ASSET");

	const char* pScriptName = CTheScripts::GetCurrentScriptName();

	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);

	if (ptfxVerifyf(ptfxAssetName.GetHash(), "script %s has no particle asset set up - can't request the asset", pScriptName))
	{
		strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(ptfxAssetName);
		if (scriptVerifyf(slot.IsValid(), "%s REQUEST_PTFX_ASSET - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ptfxAssetName.GetCStr()))
		{
			CScriptResource_PTFX_Asset ptfxAsset(slot.Get(), g_additionalStreamingRequestFlags);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(ptfxAsset);

#if GTA_REPLAY
			g_ptFxManager.SetReplayScriptPtfxAssetID(slot.Get());
#endif
		}
	}

	g_additionalStreamingRequestFlags = 0;
}

//
// name:		HasPtFxAssetLoaded
// description:	Return if particle asset is in memory
bool HasPtFxAssetLoaded()
{
	const char* pScriptName = CTheScripts::GetCurrentScriptName();
	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);

	if (ptfxVerifyf(ptfxAssetName.GetHash(), "script %s has no particle asset set up - can't check if the asset has loaded", pScriptName))
	{
		strLocalIndex slot = strLocalIndex(ptfxManager::GetAssetStore().FindSlotFromHashKey(ptfxAssetName));
		if (scriptVerifyf(slot.IsValid(), "%s HAS_PTFX_ASSET_LOADED - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ptfxAssetName.GetCStr()))
		{
			if (CTheScripts::GetCurrentGtaScriptHandler())
			{
				scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX_ASSET, slot.Get()), "HAS_PTFX_ASSET_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ptfxAssetName.GetCStr());
			}

			return ptfxManager::GetAssetStore().HasObjectLoaded(slot);
		}
	}

	return false;
}

//
// name:		RemovePtFxAsset
// description:	Remove particle asset
void RemovePtFxAsset()
{
	const char* pScriptName = CTheScripts::GetCurrentScriptName();
	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);

	if (ptfxVerifyf(ptfxAssetName.GetHash(), "script %s has no particle asset set up - can't remove the asset", pScriptName))
	{
		if (SCRIPT_VERIFY(ptfxAssetName, "invalid particle asset name"))
		{
			strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(ptfxAssetName);
			if (scriptVerifyf(slot.IsValid(), "%s REMOVE_PTFX_ASSET - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ptfxAssetName.GetCStr()))
			{
				CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX_ASSET, slot.Get());

#if GTA_REPLAY
				g_ptFxManager.SetReplayScriptPtfxAssetID(-1);
#endif
			}
		}
	}
}

//
// name:		RequestNamedPtFxAsset
// description:	Request that a named particle asset be loaded
void RequestNamedPtFxAsset(const char* pPtFxAssetName)
{
	ASSERT_NO_LOAD_SCENE("REQUEST_NAMED_PTFX_ASSET");

	u32 assetHash = atStringHash(pPtFxAssetName);
	s32 slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(assetHash).Get();
	if (scriptVerifyf(slot != -1, "%s REQUEST_NAMED_PTFX_ASSET - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPtFxAssetName))
	{
		CScriptResource_PTFX_Asset ptfxAsset(slot, g_additionalStreamingRequestFlags);
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(ptfxAsset);

#if GTA_REPLAY
		if(CPacketRequestNamedPtfxAsset::CheckRequestedHash(assetHash))
		{
			CReplayMgr::RecordFx<CPacketRequestNamedPtfxAsset>(CPacketRequestNamedPtfxAsset(assetHash));
		}
#endif // GTA_REPLAY
	}

	g_additionalStreamingRequestFlags = 0;
}

//
// name:		HasNamedPtFxAssetLoaded
// description:	Return if particle asset is in memory
bool HasNamedPtFxAssetLoaded(const char* pPtFxAssetName)
{
	s32 slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atStringHash(pPtFxAssetName)).Get();
	if (scriptVerifyf(slot != -1, "%s HAS_NAMED_PTFX_ASSET_LOADED - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPtFxAssetName))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX_ASSET, slot), "HAS_NAMED_PTFX_ASSET_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPtFxAssetName);
		}

		return ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(slot));
	}

	return false;
}

//
// name:		RemoveNamedPtFxAsset
// description:	Remove particle asset
void RemoveNamedPtFxAsset(const char* pPtFxAssetName)
{
	u32 assetHash = atStringHash(pPtFxAssetName);
	s32 slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(assetHash).Get();
	if (scriptVerifyf(slot != -1, "%s REMOVE_NAMED_PTFX_ASSET - cannot find particle asset slot (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPtFxAssetName))
	{
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX_ASSET, slot);

		REPLAY_ONLY(CPacketRequestNamedPtfxAsset::RemoveRequestedHash(assetHash));
	}
}

//
// name:		SetVehiclePopulationBudget
// description:	Set the memory budget level for the vehicle population
void SetVehiclePopulationBudget(int budgetLevel)
{
	Displayf("SET_VEHICLE_POPULATION_BUDGET(%i) - called from script \"%s\".", budgetLevel, CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (SCRIPT_VERIFY(budgetLevel >= 0 && budgetLevel < MAX_MEM_LEVELS, "Invalid vehicle population budget level specified"))
	{
		gPopStreaming.SetVehMemoryBudgetLevel(budgetLevel);
	}
}

//
// name:		SetPedPopulationBudget
// description:	Set the memory budget level for the ped population
void SetPedPopulationBudget(int budgetLevel)
{
	Displayf("SET_PED_POPULATION_BUDGET(%i) - called from script \"%s\".", budgetLevel, CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (SCRIPT_VERIFY(budgetLevel >= 0 && budgetLevel < MAX_MEM_LEVELS, "Invalid ped population budget level specified"))
	{
		gPopStreaming.SetPedMemoryBudgetLevel(budgetLevel);
	}
}

//
// name:		ClearFocus
// description:	resets the streaming focus entity to default (player ped)
void ClearFocus()
{
	CFocusEntityMgr::GetMgr().SetDefault();
#if !__FINAL
			scriptDebugf1("%s: CLEAR_FOCUS()", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scrThread::PrePrintStackTrace();
#endif //__FINAL
}

//
// name:		SetFocusPosAndVel
// description:	overrides the streaming focus to specified position and velocity
void SetFocusPosAndVel(const scrVector & pos, const scrVector & vel)
{
#if __ASSERT
	if (g_PlayerSwitch.IsActive())
	{
		Displayf("Changing focus during player switch! Call stack follows");
		scrThread::PrePrintStackTrace();
	}
#endif

	if ( Verifyf(!g_PlayerSwitch.IsActive(), "%s called SET_FOCUS_POS_AND_VEL during player switch - this is unsafe!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		SCRIPT_ASSERT(!(pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f), "SET_FOCUS_POS_AND_VEL was passed a zero vector for the position");
		Displayf("Script %s called SET_FOCUS_POS_AND_VEL() pos=(%.2f, %.2f, %.2f), vel=(%.2f, %.2f, %.2f)",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			pos.x, pos.y, pos.z, vel.x, vel.y, vel.z
			);

		Vector3 vFocusPos(pos);
		Vector3 vFocusVel(vel);
#if GTA_REPLAY
		bool shouldRecord = false;

		CPed* pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			// For url:bugstar:4252330 - Record the focus entity manager being given a position and velocity...
			// BUT only do it if we're in the interior of the Avenger (as this call is used in many places in script
			// and we don't want to risk suddenly recording something we've managed without so far and causing bother)
			fwInteriorLocation interiorLoc = pPlayer->GetInteriorLocation();
			CInteriorProxy *pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLoc);
			if(pInteriorProxy)
			{
				const u32 interiorHashesForRoomOneToRecord[] = { 
					0xdababf3e, /*"xm_x17dlc_int_01"*/	// Avenger interior
					0x90d05f68, /*"ba_dlc_int_03_ba"*/	// Terrorbyte truck interior - url:bugstar:5065997
					0x82396e18, /*"ch_dlc_plan"*/		// Arcade interior - url:bugstar:6139015
				};

				const u32 interiorHashesForAnyRoomTorRecord[] = {
					0x92455476, /*"h4_int_sub_h4"*/		// Submarine interior - url:bugstar:6794037
				};

				bool matchesInterior = false;

				if(interiorLoc.GetRoomIndex() == 1)
				{
					for(int i = 0; i < NELEM(interiorHashesForRoomOneToRecord); ++i)
					{
						if(interiorHashesForRoomOneToRecord[i] == pInteriorProxy->GetNameHash())
							matchesInterior = true;
					}
				}
				else
				{
					for(int i = 0; i < NELEM(interiorHashesForAnyRoomTorRecord); ++i)
					{
						if(interiorHashesForAnyRoomTorRecord[i] == pInteriorProxy->GetNameHash())
							matchesInterior = true;
					}
				}

				if(matchesInterior)
				{
					if(CFocusEntityMgr::GetMgr().GetFocusState() != CFocusEntityMgr::FOCUS_OVERRIDE_POS)
					{
						shouldRecord = true;
					}
					else if(CFocusEntityMgr::GetMgr().GetPos() != pos || CFocusEntityMgr::GetMgr().GetVel() != vel)
					{
						shouldRecord = true;
					}
				}
			}
		}
#endif // GTA_REPLAY
		
		CFocusEntityMgr::GetMgr().SetPosAndVel(vFocusPos, vFocusVel);
#if !__FINAL
		scriptDebugf1("%s: SET_FOCUS_POS_AND_VEL((%.2f, %.2f, %.2f),(%.2f, %.2f, %.2f))", CTheScripts::GetCurrentScriptNameAndProgramCounter(),  pos.x, pos.y, pos.z, vel.x, vel.y, vel.z);
		scrThread::PrePrintStackTrace();
#endif //__FINAL

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && shouldRecord)
		{
			CReplayMgr::RecordFx<CPacketFocusPosAndVel>(CPacketFocusPosAndVel(vFocusPos, vFocusVel));
		}
#endif // GTA_REPLAY
	}
}

//
// name:		SetFocusEntity
// description:	overrides the streaming focus to specified entity
void SetFocusEntity(int entityIndex)
{
#if __ASSERT
	if (g_PlayerSwitch.IsActive())
	{
		Displayf("Changing focus during player switch! Call stack follows");
		scrThread::PrePrintStackTrace();
	}
#endif

	if ( Verifyf(!g_PlayerSwitch.IsActive(), "%s called SET_FOCUS_ENTITY during player switch - this is unsafe!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_FOCUS_ENTITY - entity does not exist"))
		{
			CFocusEntityMgr::GetMgr().SetEntity(*pEntity);
#if !__FINAL
			scriptDebugf1("%s: SET_FOCUS_ENITTY(%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(),  entityIndex);
			scrThread::PrePrintStackTrace();
#endif //__FINAL

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				const u32 entityHashes[] = {0x5993f939, /*"trailerlarge"*/
											0x81bd2ed0, /*avenger*/
											0x897afc65, /*"terbyte"*/};

				bool recordPacket = false;
				for(int i = 0; i < NELEM(entityHashes); ++i)
				{
					if(pEntity->GetModelNameHash() == entityHashes[i])
					{
						recordPacket = true;
						break;
					}
				}

				if(recordPacket)
				{
					CReplayMgr::RecordFx<CPacketFocusEntity>(CPacketFocusEntity(), pEntity);
				}
			}
#endif // GTA_REPLAY
		}
	}
}

// very special case - restore focus to specified entity after player switch
void SetRestoreFocusEntity(int entityIndex)
{
#if __ASSERT
	if (!g_PlayerSwitch.IsActive())
	{
		Displayf("Changing restore focus only permitted during player switch! Call stack follows");
		scrThread::PrePrintStackTrace();
	}
#endif

	if ( Verifyf(g_PlayerSwitch.IsActive(), "%s called SET_RESTORE_FOCUS_ENTITY outside of player switch - this is unsafe!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_RESTORE_FOCUS_ENTITY - entity does not exist"))
		{
			CFocusEntityMgr::GetMgr().SetSavedOverrideEntity(*pEntity);
#if !__FINAL
			scriptDebugf1("%s: SET_RESTORE_FOCUS_ENTITY(%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(),  entityIndex);
			scrThread::PrePrintStackTrace();
#endif //__FINAL
		}
	}
}

bool IsEntityFocus(int entityIndex)
{
	const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	if ( pEntity && pEntity==CFocusEntityMgr::GetMgr().GetEntity() )
	{
		return true;
	}
	return false;
}

//
// name:		SetMapDataCullBoxEnabled
// description:	allows script to enable and disable specific map data cull boxes referred to by name.
//				typically in GTAV this allows us to cull out the entire GTAV map when playing prologue
void SetMapDataCullBoxEnabled(const char* boxName, bool bEnabled)
{
	u32 nameHash = atStringHash(boxName);

	// If the cullbox isn't already enabled and it's to be enabled then regenerate the cull list
	// as the map data may have changed
	if(bEnabled && !CIplCullBox::IsBoxEnabled(nameHash))
	{
		INSTANCE_STORE.CreateChildrenCache();
		CIplCullBox::GenerateCullListForBox(nameHash);
#if !__BANK
		INSTANCE_STORE.DestroyChildrenCache(); // used by cull box editor
#endif // !__BANK
	}

	CIplCullBox::SetBoxEnabled(nameHash, bEnabled);
}

//
// name:		SetAllMapDataCulled
// description:	allows script to cull all map data in one go, or not
void SetAllMapDataCulled(bool /*bCulled*/)
{
	//I am killing off this command - IK

// 	CIplCullBox::CullEverything(bCulled);
// 	CHorizonObjects::CullEverything(bCulled);
}

//
// name:		SetHorizonObjectsCulled
// description:	allows script to cull all horizon objects
void SetHorizonObjectsCulled(bool bCulled)
{
	CHorizonObjects::CullEverything(bCulled);
}

//
// name:		CommandStreamVolCreateSphere
// description:	registers a new spherical stream volume for the specified asset types
int CommandStreamVolCreateSphere(const scrVector & vPos, float fRadius, s32 assetFlags, s32 lodFlags=LODTYPES_MASK_ALL)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling STREAMVOL_CREATE_SPHERE while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		// create spherical streaming volume
		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		bool bSceneStreaming = ( assetFlags & fwBoxStreamerAsset::MASK_MAPDATA ) != 0;
		spdSphere streamVolSphere((Vec3V)vPos, ScalarV(fRadius));
		CStreamVolumeMgr::RegisterSphere(streamVolSphere, (fwBoxStreamerAssetFlags)assetFlags, bSceneStreaming, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);
		CStreamVolumeMgr::SetVolumeScriptInfo(pGtaThread->GetThreadId(), pGtaThread->GetScriptName());

		// only request specific lod levels
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
		if ( volume.IsActive() )
		{
			volume.SetLodMask( lodFlags );
		}
	}

	return 0;
}

int CommandStreamVolCreateFrustum(const scrVector & vPos, const scrVector & vDir, float fFarClip, s32 assetFlags, s32 lodFlags=LODTYPES_MASK_ALL)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling STREAMVOL_CREATE_FRUSTUM while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		// create frustum streaming volume
		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		bool bSceneStreaming = ( assetFlags & fwBoxStreamerAsset::MASK_MAPDATA ) != 0;
		CStreamVolumeMgr::RegisterFrustum(Vec3V(vPos), Vec3V(vDir), fFarClip, (fwBoxStreamerAssetFlags)assetFlags, bSceneStreaming, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);
		CStreamVolumeMgr::SetVolumeScriptInfo(pGtaThread->GetThreadId(), pGtaThread->GetScriptName());

		// only request specific lod levels
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
		if ( volume.IsActive() )
		{
			volume.SetLodMask( lodFlags );
		}
	}

	return 0;
}

int CommandStreamVolCreateLine(const scrVector & vPos1, const scrVector & vPos2, s32 assetFlags)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling STREAMVOL_CREATE_LINE while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		// create line segment streaming volume
		scriptAssertf(assetFlags==fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER,
			"STREAMVOL_CREATE_LINE only supports mover collision at present- %s",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		bool bSceneStreaming = ( assetFlags & fwBoxStreamerAsset::MASK_MAPDATA ) != 0;
		CStreamVolumeMgr::RegisterLine(Vec3V(vPos1), Vec3V(vPos2), (fwBoxStreamerAssetFlags)assetFlags, bSceneStreaming, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);
		CStreamVolumeMgr::SetVolumeScriptInfo(pGtaThread->GetThreadId(), pGtaThread->GetScriptName());
	}

	return 0;
}

//
// name:		CommandStreamVolDelete
// description:	removes an existing streaming volume, specified by index
void CommandStreamVolDelete(int UNUSED_PARAM(volumeIndex))
{
	if ( scriptVerifyf(!g_LoadScene.IsActive(), "Calling STREAMVOL_DELETE while a new load scene is in progress is not permitted. Calling script is %s, new load scene started by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), g_LoadScene.GetScriptName().c_str() ))
	{
		CStreamVolumeMgr::Deregister( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
	}
}
//
// name:		CommandStreamVolHasLoaded
// description:	returns true if the stream volume at the specified index is all loaded, false otherwise
bool CommandStreamVolHasLoaded(int UNUSED_PARAM(volumeIndex))
{
	return CStreamVolumeMgr::HasLoaded( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
}

//
// name:		CommandStreamVolIsValid
// description:	returns true if there is a valid, active stream volume at the specified index, false otherwise
bool CommandStreamVolIsValid(int UNUSED_PARAM(volumeIndex))
{
	return CStreamVolumeMgr::IsVolumeActive( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
}

//
// name:		CommandIsStreamVolActive
// description:	return true if there is some active streaming volume currently
bool CommandIsStreamVolActive()
{
	return CStreamVolumeMgr::IsVolumeActive( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
}

//
// name:		CommandNewLoadSceneStartFrustum
// description:	starts a new load scene at the specified pos, with specified camera direction and farclip
bool CommandNewLoadSceneStartFrustum(const scrVector & vPos, const scrVector & vDir, float fFarClip, s32 controlFlags=0)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling NEW_LOAD_SCENE_START while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		Displayf("Script %s called NEW_LOAD_SCENE_START pos=(%.2f, %.2f, %.2f), dir=(%.2f, %.2f, %.2f), farClip=%.2f, flags=%d",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			vPos.x, vPos.y, vPos.z, vDir.x, vDir.y, vDir.z, fFarClip, controlFlags
		);

		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		g_LoadScene.StartFromScript((Vec3V) vPos, (Vec3V) vDir, fFarClip, pGtaThread->GetThreadId(), true, controlFlags);
		return true;
	}
	return false;
}

//
// name:		CommandNewLoadSceneStartSphere
// description:	starts a new load scene at the specified pos, and radius
bool CommandNewLoadSceneStartSphere(const scrVector & vPos, float fRadius, s32 controlFlags=0)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling NEW_LOAD_SCENE_START_SPHERE while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		Displayf("Script %s called NEW_LOAD_SCENE_START_SPHERE pos=(%.2f, %.2f, %.2f), radius=%.2f, flags=%d",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			vPos.x, vPos.y, vPos.z, fRadius, controlFlags
		);

		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		g_LoadScene.StartFromScript((Vec3V) vPos, Vec3V(V_ZERO), fRadius, pGtaThread->GetThreadId(), false, controlFlags);
		return true;
	}
	return false;
}

//
// name:		CommandNewLoadSceneStop
// description:	stops the new load scene, if active
void CommandNewLoadSceneStop()
{
	Displayf("Script %s called NEW_LOAD_SCENE_STOP", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	g_LoadScene.Stop();
}

//
// name:		CommandIsNewLoadSceneActive
// description:	returns true if the new load scene is active, false otherwise
bool CommandIsNewLoadSceneActive()
{
	return g_LoadScene.IsActive();
}

//
// name:		CommandIsNewLoadSceneLoaded
// description:	returns true of the new load scene is fully loaded, false otherwise
bool CommandIsNewLoadSceneLoaded()
{
	scriptAssertf( g_LoadScene.IsActive(), "IS_NEW_LOAD_SCENE_LOADED called, but no load scene is currently active: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	return g_LoadScene.IsLoaded();
}

//
// name:		CommandIsSafeToStartPlayerSwitch
// description:	Sees if it's OK to start a player switch, not in the middle of death or arrest
bool CommandIsSafeToStartPlayerSwitch()
{
#if __BANK
	//////////////////////////////////////////////////////////////////////////
	// some debug spew to help track down B* 1941681
	//
	if ( !CGameLogic::IsGameStateInPlay() )
	{
		scriptDisplayf("Script %s call to IS_SAFE_TO_START_PLAYER_SWITCH returns false because gamestate is not PLAYING (state=%d)", CTheScripts::GetCurrentScriptName(), CGameLogic::GetGameState() );
	}
	else
	{
		CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		if (!pPlayerInfo)
		{
			scriptDisplayf("Script %s call to IS_SAFE_TO_START_PLAYER_SWITCH returns false because player info is NULL", CTheScripts::GetCurrentScriptName() );
		}
		else
		{
			if ( !pPlayerInfo->GetPlayerPed() )
			{
				scriptDisplayf("Script %s call to IS_SAFE_TO_START_PLAYER_SWITCH returns false because player ped is NULL", CTheScripts::GetCurrentScriptName() );
			}
			else if ( pPlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING )
			{
				scriptDisplayf("Script %s call to IS_SAFE_TO_START_PLAYER_SWITCH returning false because player state is not PLAYING (state=%d)", CTheScripts::GetCurrentScriptName(), pPlayerInfo->GetPlayerState() );
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
#endif	//__BANK

	if( CGameLogic::IsGameStateInPlay() )
	{
		// We're in the GAME_PLAY state
		CPlayerInfo	*pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		// Is the player about to die or get arrested?
		if( pPlayerInfo && pPlayerInfo->GetPlayerPed())
		{
			if( pPlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_PLAYING )
			{
				// no
				return true;
			}
		}
	}
	return false;	
}

//
// name:		CommandStartPlayerSwitch
// description:	starts a player switch
void CommandStartPlayerSwitch(int oldPedIndex, int newPedIndex, int flags, s32 switchType=CPlayerSwitchInterface::SWITCH_TYPE_AUTO)
{
	scriptAssertf( CommandIsSafeToStartPlayerSwitch(), "Script %s called START_PLAYER_SWITCH but IS_SAFE_TO_START_PLAYER_SWITCH is false", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	scriptAssertf( !CWarpManager::IsActive(), "Script %s calling START_PLAYER_SWITCH while a warp is in progress!", CTheScripts::GetCurrentScriptNameAndProgramCounter() );

	CPed *pOldPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(oldPedIndex, 0);
	CPed *pNewPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(newPedIndex, 0);

	scriptAssertf(pOldPed, "Invalid old ped index %d passed from script %s. Switch will not proceed", oldPedIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(pNewPed, "Invalid new ped index %d passed from script %s. Switch will not proceed", newPedIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());

#if !__FINAL

	if (!pOldPed || !pNewPed)
		return;

	Vector3 vOldPos = VEC3V_TO_VECTOR3( pOldPed->GetTransform().GetPosition() );
	Vector3 vNewPos = VEC3V_TO_VECTOR3( pNewPed->GetTransform().GetPosition() );

	Displayf( "START_PLAYER_SWITCH called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	Displayf( "... src=(%.2f, %.2f, %.2f) dst=(%.2f, %.2f, %.2f) flags=0x%x switchType=%d",
		vOldPos.x, vOldPos.y, vOldPos.z, vNewPos.x, vNewPos.y, vNewPos.z, flags, switchType );
#endif	//!__FINAL

#if __ASSERT

	//////////////////////////////////////////////////////////////////////////
	// check to see if script is forcing us to do a switch that is unlikely to work well
	if ((flags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST) != 0)
	{
		scriptAssertf( switchType!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"Script %s is calling START_PLAYER_SWITCH for short range switch to unknown destination. That won't work",
			CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	}
	else
	{
		s32 idealSwitchType = g_PlayerSwitch.GetIdealSwitchType( pOldPed->GetTransform().GetPosition(), pNewPed->GetTransform().GetPosition() );

		switch (switchType)
		{
		case CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM:
			scriptAssertf( idealSwitchType!=CPlayerSwitchInterface::SWITCH_TYPE_LONG,
				"Script %s forcing START_PLAYER_SWITCH to do a medium range switch over too great a distance",
				CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			break;

		case CPlayerSwitchInterface::SWITCH_TYPE_SHORT:

// 			scriptWarningf( idealSwitchType!=CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM,
// 				"Script %s forcing START_PLAYER_SWITCH to do a short range switch over too great a distance",
// 				CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			break;

		default:
			break;
		}
	}
	//////////////////////////////////////////////////////////////////////////
#endif	//__ASSERT

	if ( pOldPed && pNewPed && scriptVerifyf(!g_PlayerSwitch.IsActive(), "START_PLAYER_SWITCH - a switch is already in progress") )
	{
		g_PlayerSwitch.Start(switchType, *pOldPed, *pNewPed, flags);
	}
}

//
// name:		CommandGetPlayerSwitchType
// description:	returns the current player switch type
s32 CommandGetPlayerSwitchType()
{
	return g_PlayerSwitch.GetSwitchType();
}

//
// name:		CommandGetIdealPlayerSwitchType
// description:	returns the ideal desired switch type based on the distance from start to end
s32 CommandGetIdealPlayerSwitchType(const scrVector & vStartPos, const scrVector & vEndPos)
{
	return g_PlayerSwitch.GetIdealSwitchType((Vec3V) vStartPos, (Vec3V) vEndPos);
}

// name:		CommandSetPlayerSwitchOutro
// description:	overrides the end scene which is streamed - use if the end scene position differs from the ped pos
void CommandSetPlayerSwitchOutro(const scrVector & vCamPos, const scrVector & vCamRot, float fCamFov, float fCamFarClip, int rotOrder)
{
	if (scriptVerifyf(g_PlayerSwitch.IsActive(), "SET_PLAYER_SWITCH_OUTRO called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf( g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT, "SET_PLAYER_SWITCH_OUTRO called by %s but that's not valid for a short range switch", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			// Vector3 vPos(vCamPos);
			Vector3 vRot(vCamRot);

			Matrix34 worldMatrix;
			CScriptEulers::MatrixFromEulers( worldMatrix, vRot*DtoR, static_cast<EulerAngleOrder>(rotOrder) );
			worldMatrix.d = vCamPos;

			g_PlayerSwitch.GetLongRangeMgr().OverrideOutroScene( RCC_MAT34V(worldMatrix), fCamFov, fCamFarClip );
		}
	}
}

// name:		CommandSetPlayerSwitchEstablishingShot
// description:	sets an establishing which occurs before the outro
void CommandSetPlayerSwitchEstablishingShot(const char* pszShotName)
{
	g_PlayerSwitch.GetLongRangeMgr().SetEstablishingShot(atHashString(pszShotName));
}

//
// name:		CommandIsPlayerSwitchInProgress
// description:	returns true if a switch is in progress, false otherwise
bool CommandIsPlayerSwitchInProgress()
{
	return g_PlayerSwitch.IsActive();
}

//
// name:		CommandGetPlayerSwitchState
// description:	returns the current state of an active player switch
int CommandGetPlayerSwitchState()
{
	scriptAssertf( g_PlayerSwitch.IsActive(),
		"GET_PLAYER_SWITCH_STATE called by %s but switch is not active",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
	);
	scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
		"GET_PLAYER_SWITCH_STATE called by %s - not valid for short range switches",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
	);

	return g_PlayerSwitch.GetLongRangeMgr().GetState();
}

//
// name:		CommandGetPlayerShortSwitchState
// description:	returns the current state of an active player short-range switch
int CommandGetPlayerShortSwitchState()
{
	scriptAssertf( g_PlayerSwitch.IsActive(),
		"GET_PLAYER_SHORT_SWITCH_STATE called by %s but switch is not active",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);
	scriptAssertf(g_PlayerSwitch.GetSwitchType()==CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
		"GET_PLAYER_SHORT_SWITCH_STATE called by %s - only valid for short range switches",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

	return g_PlayerSwitch.GetShortRangeMgr().GetState();
}


void CommandSetPlayerShortSwitchStyle(int style)
{
	scriptAssertf( style >= 0,
		"SET_PLAYER_SHORT_SWITCH_STYLE - called by %s, invalid style",
		CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	scriptAssertf( style < CPlayerSwitchMgrShort::NUM_SHORT_SWITCH_STYLES,
		"SET_PLAYER_SHORT_SWITCH_STYLE - called by %s, invalid style",
		CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	g_PlayerSwitch.GetShortRangeMgr().SetStyle( (CPlayerSwitchMgrShort::eStyle)style );
}

//
// name:		CommandGetPlayerSwitchJumpCutIndex
// description:	returns the index of the current jump cut of an active player switch
int CommandGetPlayerSwitchJumpCutIndex()
{
	scriptAssertf( g_PlayerSwitch.IsActive(),
		"GET_PLAYER_SWITCH_JUMP_CUT_INDEX called by %s but switch is not active",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
	);
	scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
		"GET_PLAYER_SWITCH_JUMP_CUT_INDEX called by %s - not valid for short range switches",
		CTheScripts::GetCurrentScriptNameAndProgramCounter()
	);

	return static_cast<int>(g_PlayerSwitch.GetLongRangeMgr().GetCurrentJumpCutIndex());
}

//
// name:		CommandAllowPlayerSwitchPan
// description:	if script requires the switch to pause before proceeding with the pan
//				(e.g. to stream in some script requested assets etc) this command
//				gives the switch mgr the go-ahead to proceed
void CommandAllowPlayerSwitchPan()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "ALLOW_PLAYER_SWITCH_PAN called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"ALLOW_PLAYER_SWITCH_PAN called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		g_PlayerSwitch.GetLongRangeMgr().SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_PAN, false);
	}
}

//
// name:		CommandAllowPlayerSwitchOutro
// description:	if script requires the switch to pause before proceeding with the outro
//				(e.g. to stream in some script requested assets etc) this command
//				gives the switch mgr the go-ahead to proceed
void CommandAllowPlayerSwitchOutro()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "ALLOW_PLAYER_SWITCH_OUTRO called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"ALLOW_PLAYER_SWITCH_OUTRO called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		g_PlayerSwitch.GetLongRangeMgr().SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_OUTRO, false);
	}
}

//
// name:		CommandAllowPlayerSwitchAscent
// description:	if script requires the switch to pause before proceeding with the ascent
//				this command gives the switch mgr the go-ahead to proceed when ready
void CommandAllowPlayerSwitchAscent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "ALLOW_PLAYER_SWITCH_ASCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"ALLOW_PLAYER_SWITCH_ASCENT called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		g_PlayerSwitch.GetLongRangeMgr().SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT, false);
	}
}

//
// name:		CommandAllowPlayerSwitchDescent
// description:	if script requires the switch to pause before proceeding with the descent
//				this command gives the switch mgr the go-ahead to proceed when ready
void CommandAllowPlayerSwitchDescent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "ALLOW_PLAYER_SWITCH_DESCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"ALLOW_PLAYER_SWITCH_DESCENT called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		g_PlayerSwitch.GetLongRangeMgr().SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT, false);
	}
}

bool CommandIsSwitchReadyForAscent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "IS_SWITCH_READY_FOR_ASCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"IS_SWITCH_READY_FOR_ASCENT called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		return g_PlayerSwitch.GetLongRangeMgr().ReadyForAscent();
	}
	return false;
}

bool CommandIsSwitchReadyForDescent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "IS_SWITCH_READY_FOR_DESCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"IS_SWITCH_READY_FOR_DESCENT called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		return g_PlayerSwitch.GetLongRangeMgr().ReadyForDescent();
	}
	return false;
}

void CommandEnableSwitchPauseBeforeDescent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "ENABLE_SWITCH_PAUSE_BEFORE_DESCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT,
			"ENABLE_SWITCH_PAUSE_BEFORE_DESCENT called by %s - not valid for short range switches",
			CTheScripts::GetCurrentScriptNameAndProgramCounter()
		);

		Displayf("[Playerswitch] Script %s calling ENABLE_SWITCH_PAUSE_BEFORE_DESCENT. Switch will hang before descent until script permit progress", CTheScripts::GetCurrentScriptName());

		g_PlayerSwitch.GetLongRangeMgr().EnablePauseBeforeDescent();
	}
}

void CommandDisableSwitchOutroFX()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "DISABLE_SWITCH_OUTRO_FX called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		Displayf("[Playerswitch] Script %s calling DISABLE_SWITCH_OUTRO_FX. Switch will skip outro FX", CTheScripts::GetCurrentScriptName());

		s32 switchType = g_PlayerSwitch.GetSwitchType();
		CPlayerSwitchMgrBase* mgr = g_PlayerSwitch.GetMgr(switchType);
		mgr->SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_OUTRO_FX,true);
	}
}

bool CommandIsSwitchSkippingDescent()
{
	if ( scriptVerifyf(g_PlayerSwitch.IsActive(), "IS_SWITCH_SKIPPING_DESCENT called by %s but switch is not active", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		return g_PlayerSwitch.GetLongRangeMgr().IsSkippingDescent();
	}
	return false;
}

//
// name:		CommandSwitchToMultiFirstPart
// description:	perform the first part of the player switch (ascent part) when the destination
//				is currently unknown (pending user input during character selection screen etc)
void CommandSwitchToMultiFirstPart(int oldPedIndex, int flags, s32 switchType=CPlayerSwitchInterface::SWITCH_TYPE_LONG)
{
	scriptAssertf( !CWarpManager::IsActive(), "Script %s calling SWITCH_TO_MULTI_FIRSTPART while a warp is in progress!", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	scriptAssertf( CommandIsSafeToStartPlayerSwitch(), "Script %s called SWITCH_TO_MULTI_FIRSTPART but IS_SAFE_TO_START_PLAYER_SWITCH is false", CTheScripts::GetCurrentScriptNameAndProgramCounter() );

	CPed *pOldPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(oldPedIndex, 0);

	if ( pOldPed && scriptVerifyf(!g_PlayerSwitch.IsActive(), "SWITCH_TO_MULTI_FIRSTPART - a switch is already in progress") )
	{

#if !__FINAL
		Vector3 vOldPos = VEC3V_TO_VECTOR3( pOldPed->GetTransform().GetPosition() );
		Displayf( "SWITCH_TO_MULTI_FIRSTPART called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
		Displayf( "... src=(%.2f, %.2f, %.2f) flags=0x%x type=%d", vOldPos.x, vOldPos.y, vOldPos.z, flags, switchType );
#endif	//!__FINAL

		g_PlayerSwitch.Start(switchType, *pOldPed, *pOldPed, flags | CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST);
	}
}

//
// name:		CommandIsSwitchToMultiFirstPartFinished
// description:	returns true if the player switch is currently finished its ascent and is holding,
//				waiting for more information about the destination of the switch
bool CommandIsSwitchToMultiFirstPartFinished()
{
	return ( g_PlayerSwitch.GetLongRangeMgr().GetState() == CPlayerSwitchMgrLong::STATE_WAITFORINPUT );
}

//
// name:		CommandSwitchToMultiSecondPart
// description:	perform the second part of the player switch (pan and descent parts) because the destination ped
//				is now known.
void CommandSwitchToMultiSecondPart(int newPedIndex)
{
	CPed *pNewPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(newPedIndex, 0);
	if ( pNewPed && scriptVerifyf(g_PlayerSwitch.IsActive(), "START_PLAYER_SWITCH - a switch is not already in progress") )
	{

#if !__FINAL
		Vector3 vNewPos = VEC3V_TO_VECTOR3( pNewPed->GetTransform().GetPosition() );
		Displayf( "[Playerswitch] SWITCH_TO_MULTI_SECONDPART called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
		Displayf( "... dst=(%.2f, %.2f, %.2f)", vNewPos.x, vNewPos.y, vNewPos.z );
#endif	//!__FINAL

#if __ASSERT

		scriptAssertf(g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT, "SWITCH_TO_MULTI_SECOND_PART is not valid for short range switching");

		//////////////////////////////////////////////////////////////////////////
		s32 idealSwitchType = g_PlayerSwitch.GetIdealSwitchType( g_PlayerSwitch.GetLongRangeMgr().GetStartPos(), pNewPed->GetTransform().GetPosition() );
		switch (g_PlayerSwitch.GetSwitchType())
		{
		case CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM:
			scriptAssertf( idealSwitchType!=CPlayerSwitchInterface::SWITCH_TYPE_LONG,
				"Script %s forcing SWITCH_TO_MULTI_SECOND_PART to do a medium range switch over too great a distance!",
				CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			break;

		case CPlayerSwitchInterface::SWITCH_TYPE_SHORT:
			scriptAssertf( idealSwitchType!=CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM,
				"Script %s forcing SWITCH_TO_MULTI_SECOND_PART to do a short range switch over too great a distance",
				CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			break;

		default:
			break;
		}
		//////////////////////////////////////////////////////////////////////////
#endif	//__ASSERT

		g_PlayerSwitch.SetDest(*pNewPed);
	}
}

//
// name:		CommandSstopPlayerSwitch
// description:	abandon a switch, if active
void CommandStopPlayerSwitch()
{
	Displayf( "STOP_PLAYER_SWITCH called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter() );

	if (g_PlayerSwitch.IsActive())
	{
		g_PlayerSwitch.Stop();
	}
}

// name:		CommandGetPlayerSwitchInterpOutDuration
// description:	returns the duration of any interpolation out of an establishing shot, in milliseconds, or 0 if invalid
int CommandGetPlayerSwitchInterpOutDuration()
{
	const CPlayerSwitchEstablishingShotMetadata* pEstablishingShotData = g_PlayerSwitch.GetLongRangeMgr().GetEstablishingShotData();

	s32 duration = pEstablishingShotData ? static_cast<s32>(pEstablishingShotData->m_InterpolateOutDuration) : 0;

	return duration;
}

void CommandSetSceneStreamingTracksCamPosThisFrame()
{
	CSceneStreamerMgr::ForceSceneStreamerToTrackWithCameraPosThisFrame();
}

float CommandGetLodScale()
{
	return g_LodScale.GetGlobalScale();
}

// name:		CommandGetPlayerSwitchInterpOutCurrentTime
// description:	returns the current interpolation time out of an establishing shot, in milliseconds. -1 will be returned if interpolation is not in progress
int CommandGetPlayerSwitchInterpOutCurrentTime()
{
	const camSwitchDirector& switchDirector = camInterface::GetSwitchDirector();
	camBaseDirector::eRenderStates renderState = switchDirector.GetRenderState();
	s32 currentTime = (renderState == camBaseDirector::RS_INTERPOLATING_OUT) ? switchDirector.GetInterpolationTime() : -1;

	return currentTime;
}

//
// name:		CommandOverrideLodScaleThisFrame
// 
void CommandOverrideLodScaleThisFrame(float fLodScale)
{
	g_LodScale.SetGlobalScaleFromScript(fLodScale);
}

void CommandRemapLodScaleRangeThisFrame(float fOldMin, float fOldMax, float fNewMin, float fNewMax)
{
	g_LodScale.SetRemappingFromScript(fOldMin, fOldMax, fNewMin, fNewMax);
}

void CommandSuppressHdMapStreamingThisFrame()
{
	CStreaming::ScriptSuppressHdImapsThisFrame();
}

void CommandForceAllowTimeBasedFadingThisFrame()
{
	g_LodMgr.ScriptForceAllowTimeBasedFadingThisFrame();
}

//
// name:		CommandSetRenderHdOnly
//
void CommandSetRenderHdOnly(bool bHdOnly)
{
	if ( scriptVerifyf(!g_PlayerSwitch.IsActive(), "Calling SET_RENDER_HD_ONLY while a player switch is in progress is not safe! %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
	{
		Displayf( "Script called SET_RENDER_HD_ONLY(%s) - %s", (bHdOnly?"true":"false"), CTheScripts::GetCurrentScriptNameAndProgramCounter() );

		if (bHdOnly)
		{
			g_LodMgr.ScriptRequestRenderHdOnly();
		}
		else
		{
			g_LodMgr.StopSlodMode();
		}
	}
}


//
// name:		CommandIplGroupSwapStart
//
// allows user to seamlessly switch between two IPL groups in a frame, by pre-streaming the assets for the second group
// before switching it on (and switching off the first one)
//
void CommandIplGroupSwapStart(const char* iplGroupBefore, const char* iplGroupAfter)
{
	Displayf("Script %s called IPL_GROUP_SWAP_START(%s, %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iplGroupBefore, iplGroupAfter);

	strLocalIndex indexBefore		= strLocalIndex(INSTANCE_STORE.FindSlot(iplGroupBefore));
	strLocalIndex indexAfter		= strLocalIndex(INSTANCE_STORE.FindSlot(iplGroupAfter));

	scriptAssertf(indexBefore!=-1,	"IPL_GROUP_SWAP_START called by %s but %s is missing", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iplGroupBefore);
	scriptAssertf(indexAfter!=-1,	"IPL_GROUP_SWAP_START called by %s but %s is missing", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iplGroupAfter);

	const bool bBothIplGroupsExist = (indexBefore != -1) && (indexAfter != -1);

	if (bBothIplGroupsExist)
	{
		scriptAssertf(	g_MapDataStore.GetIsStreamable(indexBefore),	"IPL_GROUP_SWAP_START: %s should be enabled",	iplGroupBefore	);
		scriptAssertf(	!g_MapDataStore.GetIsStreamable(indexAfter),	"IPL_GROUP_SWAP_START: %s should be disabled",	iplGroupAfter	);

		const bool bCorrectInitialState = (g_MapDataStore.GetIsStreamable(indexBefore) && !g_MapDataStore.GetIsStreamable(indexAfter));

		if (bCorrectInitialState)
		{
			// register new swap, mark as script owned, and return swap index
			atHashString startNameHash(iplGroupBefore);
			atHashString endNameHash(iplGroupAfter);

			GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
			g_MapDataStore.GetGroupSwapper().CreateNewSwapForScript(startNameHash, endNameHash, pGtaThread->GetThreadId());		
		}
	}
}

//
// name:		CommandIplGroupSwapIsReady
//
// returns true if the IPL group swap is fully loaded and ready to transition, false otherwise
//
bool CommandIplGroupSwapIsReady()
{
	return g_MapDataStore.GetGroupSwapper().GetIsLoaded(fwImapGroupMgr::SCRIPT_INDEX);
}

//
// name:		CommandIplGroupSwapFinish
//
// performs the swap - switches on the end state, switches off the start state, removes the active swapper
//
void CommandIplGroupSwapFinish()
{
	Displayf("Script %s called IPL_GROUP_SWAP_FINISH", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(g_MapDataStore.GetGroupSwapper().GetIsActive(fwImapGroupMgr::SCRIPT_INDEX), "IPL group swap is not active (ie has been completed or canceled)");
	g_MapDataStore.GetGroupSwapper().CompleteSwap(fwImapGroupMgr::SCRIPT_INDEX, false);
}

//
// name:		CommandIplGroupSwapCancel
//
// abandon a currently active IPL group swap
void CommandIplGroupSwapCancel()
{
	Displayf("Script %s called IPL_GROUP_SWAP_CANCEL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf( g_MapDataStore.GetGroupSwapper().GetIsActive(fwImapGroupMgr::SCRIPT_INDEX), "IPL group swap is not active (ie has been completed or canceled)");
	g_MapDataStore.GetGroupSwapper().AbandonSwap(fwImapGroupMgr::SCRIPT_INDEX);
}

bool CommandIplGroupSwapIsActive()
{
	return g_MapDataStore.GetGroupSwapper().GetIsActive(fwImapGroupMgr::SCRIPT_INDEX);
}


#if __BANK

	//////////////////////////////////////////////////////////////////////////
	// TRAILER 2 ONLY
	void CommandEnableExtraDistantLights(const scrVector & vPos, float fRadius)
	{
		DebugLighting::EnableExtraDistantLights(vPos, fRadius);
	}
	void CommandDisableExtraDistantLights()
	{
		// TRAILER 2 ONLY
		DebugLighting::DisableExtraDistantLights();
	}
	//////////////////////////////////////////////////////////////////////////

#else

	void CommandEnableExtraDistantLights(const scrVector &, float) {}
	void CommandDisableExtraDistantLights() {}

#endif

void CommandPrefetchSrl(const char *srlName)
{
	gStreamingRequestList.BeginPrestream(srlName, true);
}

bool CommandIsSrlLoaded()
{
	return gStreamingRequestList.AreAllAssetsLoaded();
}

void CommandBeginSrl()
{
	gStreamingRequestList.Start();
}

void CommandEndSrl()
{
	gStreamingRequestList.Finish();
}

void CommandSetSrlTime(float time)
{
	gStreamingRequestList.SetTime(time);
}

void CommandSetSrlPostCutsceneCamera(const scrVector & vPos, const scrVector & vDir)
{
	gStreamingRequestList.SetPostCutsceneCamera(vPos, vDir);
}

void CommandSetSrlReadAheadTimes(int prestreamMap, int prestreamAssets, int playbackMap, int playbackAssets)
{
	gStreamingRequestList.SetSrlReadAheadTimes(prestreamMap, prestreamAssets, playbackMap, playbackAssets);
}

void CommandSetSrlLongJumpMode(bool enableLongJumpMode)
{
	gStreamingRequestList.SetLongJumpMode(enableLongJumpMode);
}

void CommandSetSrlForcePrestream(s32 prestreamMode)
{
	gStreamingRequestList.SetPrestreamMode((SrlPrestreamMode) prestreamMode);
}

void CommandSetHdArea(const scrVector& scrPos, float radius)
{
	CTexLod::SetHdArea(scrPos.x, scrPos.y, scrPos.z, radius);
}

void CommandClearHdArea()
{
	CTexLod::ClearHdArea();
}

void CommandInitCreatorBudget()
{
    MissionCreatorAssets::Init();
}

void CommandShutdownCreatorBudget()
{
    MissionCreatorAssets::Shutdown();
}

bool CommandHasRoomForModelInCreatorBudget(int modelHash)
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32)modelHash, &modelId);

	if (scriptVerifyf(pModelInfo, "%s: HAS_ROOM_FOR_MODEL_IN_CREATOR_BUDGET - model with hash %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
	{
		if (scriptVerifyf(modelId.IsValid(), "%s: HAS_ROOM_FOR_MODEL_IN_CREATOR_BUDGET - model with hash %d exists but its model index is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
		{
			return MissionCreatorAssets::HasRoomForModel(modelId);
		}
	}

    return false;
}

bool CommandAddModelToCreatorBudget(int modelHash)
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32)modelHash, &modelId);

	if (scriptVerifyf(pModelInfo, "%s: ADD_MODEL_TO_CREATOR_BUDGET - model with hash %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
	{
		if (scriptVerifyf(modelId.IsValid(), "%s: ADD_MODEL_TO_CREATOR_BUDGET - model with hash %d exists but its model index is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
		{
			return MissionCreatorAssets::AddModel(modelId);
		}
	}

	return false;
}

void CommandRemoveModelFromCreatorBudget(int modelHash)
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32)modelHash, &modelId);

	if (scriptVerifyf(pModelInfo, "%s: REMOVE_MODEL_FROM_CREATOR_BUDGET - model with hash %d does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
	{
		if (scriptVerifyf(modelId.IsValid(), "%s: REMOVE_MODEL_FROM_CREATOR_BUDGET - model with hash %d exists but its model index is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash))
		{
			MissionCreatorAssets::RemoveModel(modelId);
		}
	}
}

float CommandGetUsedCreatorBudget()
{
    return MissionCreatorAssets::GetUsedBudget();
}


void CommandSetIslandEnabled(char const* pIslandName, bool enable)
{
	CIslandHopper::GetInstance().RequestToggleIsland(pIslandName, enable);
}


bool CommandAreIslandIPLsActive()
{
	return CIslandHopper::GetInstance().AreCurrentIslandIPLsActive();
}

bool CommandIsGameInstalled()
{
	return CFileMgr::IsGameInstalled();
}

//
// name:		SetupScriptCommands
// description:	Setup streaming script commands
void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(LOAD_ALL_OBJECTS_NOW,0x0f3dc7925afebdfc, LoadAllObjectsNow);
	SCR_REGISTER_SECURE(LOAD_SCENE,0x42c4310e76ad635a, LoadScene);
	SCR_REGISTER_UNUSED(NETWORK_START_LOAD_SCENE,0xba65ca1a62ca5efa, StartLoadScene);
	SCR_REGISTER_SECURE(NETWORK_UPDATE_LOAD_SCENE,0x5ee9da6c0c12274b, UpdateLoadScene);
	SCR_REGISTER_UNUSED(NETWORK_STOP_LOAD_SCENE,0x61c0d41714aac740, StopLoadScene);
	SCR_REGISTER_SECURE(IS_NETWORK_LOADING_SCENE,0x340c8b9022477b2f, IsLoadSceneActive);
	SCR_REGISTER_UNUSED(LOAD_SCENE_FOR_ROOM_BY_KEY,0x77266ff77886d086, LoadSceneForRoomByKey);
	SCR_REGISTER_SECURE(SET_INTERIOR_ACTIVE,0x28a0a1eb9d59910d, CommandActivateInterior);
	SCR_REGISTER_SECURE(REQUEST_MODEL,0xd69a0c3662175e68, CommandRequestModel);
	SCR_REGISTER_SECURE(REQUEST_MENU_PED_MODEL,0x24f2c68f9513392f, CommandRequestMenuPedModel);
	SCR_REGISTER_SECURE(HAS_MODEL_LOADED,0x0152aa00fa3130f1, HasModelLoaded);
	SCR_REGISTER_UNUSED(REQUEST_INTERIOR_MODELS,0x620e68adda3d0a93, RequestInteriorModels);
	SCR_REGISTER_SECURE(REQUEST_MODELS_IN_ROOM,0x0f1bef3db99b3f66, RequestModelsInRoom);
	SCR_REGISTER_SECURE(SET_MODEL_AS_NO_LONGER_NEEDED,0xf1a23b343dfedfd0, CommandMarkModelAsNoLongerNeeded);
	SCR_REGISTER_SECURE(IS_MODEL_IN_CDIMAGE,0x51046eee00e2bbda, IsModelInCdImage);
	SCR_REGISTER_SECURE(IS_MODEL_VALID,0x61adf697df551da6, CommandIsModelValid);
	SCR_REGISTER_UNUSED(GET_PED_MODEL_FROM_INDEX,0x25a52190ae4733a7, CommandGetPedModelFromIndex);
	SCR_REGISTER_UNUSED(GET_VEHICLE_MODEL_FROM_INDEX,0x0c5271cf0ccaf6e4, CommandGetVehicleModelFromIndex);
	SCR_REGISTER_SECURE(IS_MODEL_A_PED,0x5a01a43be2689db0, CommandIsThisModelAPed);
	SCR_REGISTER_SECURE(IS_MODEL_A_VEHICLE,0x2886b1bfe0896a9a, CommandIsThisModelAVehicle);

	SCR_REGISTER_UNUSED(REQUEST_MOVER_COLLISION_ONLY,0xfeab902a1748345f, RequestMoverCollisionOnly);
	SCR_REGISTER_SECURE(REQUEST_COLLISION_AT_COORD,0xe49a23f069c82c93, RequestCollisionAtPosn);
	SCR_REGISTER_SECURE(REQUEST_COLLISION_FOR_MODEL,0xb557c195bbd7d051, RequestCollisionForModel);
	SCR_REGISTER_SECURE(HAS_COLLISION_FOR_MODEL_LOADED,0x30229254f181806b, HasCollisionForModelLoaded);
	SCR_REGISTER_SECURE(REQUEST_ADDITIONAL_COLLISION_AT_COORD,0x616bf5910d72dca9, AddNeededAtPosn);

	SCR_REGISTER_SECURE(DOES_ANIM_DICT_EXIST,0xa430c2581c66a9ad, DoesAnimDictExist);
	SCR_REGISTER_SECURE(REQUEST_ANIM_DICT,0x8fb472886552d737, RequestAnimDict);
	SCR_REGISTER_SECURE(HAS_ANIM_DICT_LOADED,0x6f940c2636c076ad, HasAnimDictLoaded);
	SCR_REGISTER_SECURE(REMOVE_ANIM_DICT,0xafc1fbf3f6ae7b9a, RemoveAnimDict);

	// START OF DEPRECATED
	SCR_REGISTER_SECURE(REQUEST_ANIM_SET,0x3aca4f727ac4606e, RequestClipSet);
	SCR_REGISTER_SECURE(HAS_ANIM_SET_LOADED,0xbb103a5dec572725,HasClipSetLoaded);
	SCR_REGISTER_SECURE(REMOVE_ANIM_SET,0x5f75840181672fad, RemoveClipSet);
	// END OF DEPRECATED

	SCR_REGISTER_SECURE(REQUEST_CLIP_SET,0x511a0af0e8b0bf9a, RequestClipSet);
	SCR_REGISTER_SECURE(HAS_CLIP_SET_LOADED,0xa7a5d14f877c9047,HasClipSetLoaded);
	SCR_REGISTER_SECURE(REMOVE_CLIP_SET,0x840dce5f5f53d8e0, RemoveClipSet);

	SCR_REGISTER_SECURE(REQUEST_IPL,0x13a763a67ba2a95b, RequestIpl);
	SCR_REGISTER_SECURE(REMOVE_IPL,0x9d2accf306f3a0c5, RemoveIpl);
	SCR_REGISTER_SECURE(IS_IPL_ACTIVE,0x8dd54478cfa35f08, IsIplActive);
	SCR_REGISTER_SECURE(SET_STREAMING,0x8e4329fd81081a2a, SwitchStreaming);
	SCR_REGISTER_UNUSED(SET_SCENE_STREAMING,0xafa0231603f22be9, EnableSceneStreaming);

	SCR_REGISTER_SECURE(LOAD_GLOBAL_WATER_FILE,0x11f9af5e230d40d6, LoadGlobalWaterFile);
	SCR_REGISTER_SECURE(GET_GLOBAL_WATER_FILE,0xcb446f8296efd9bb, GetGlobalWaterFile);

	SCR_REGISTER_SECURE(SET_GAME_PAUSES_FOR_STREAMING,0x47c032cd609587ee, AllowGameToPauseForStreaming);
	SCR_REGISTER_SECURE(SET_REDUCE_PED_MODEL_BUDGET,0x718886bcae2bd894, SetReducePedModelBudget);
	SCR_REGISTER_SECURE(SET_REDUCE_VEHICLE_MODEL_BUDGET,0x9a78aa1fbfc8a2ac, SetReduceVehicleModelBudget);
	SCR_REGISTER_SECURE(SET_DITCH_POLICE_MODELS,0x2613fa815056d2d9, SetDitchPoliceModels);

	SCR_REGISTER_SECURE(GET_NUMBER_OF_STREAMING_REQUESTS,0xee21e8a6031cea47, GetNumStreamingRequests);
	SCR_REGISTER_UNUSED(IS_STREAMING_PRIORITY_REQUESTS,0x443cd194a3b45769, IsStreamingPriorityRequests);

	SCR_REGISTER_SECURE(REQUEST_PTFX_ASSET,0x9e63031c188793e1, RequestPtFxAsset);
	SCR_REGISTER_SECURE(HAS_PTFX_ASSET_LOADED,0x51b6c2ef8dbe2e1a, HasPtFxAssetLoaded);
	SCR_REGISTER_SECURE(REMOVE_PTFX_ASSET,0xe6239ccb98b8ab66, RemovePtFxAsset);

	SCR_REGISTER_SECURE(REQUEST_NAMED_PTFX_ASSET,0xd86c99569d4749b0, RequestNamedPtFxAsset);
	SCR_REGISTER_SECURE(HAS_NAMED_PTFX_ASSET_LOADED,0x6264b811e8d92198, HasNamedPtFxAssetLoaded);
	SCR_REGISTER_SECURE(REMOVE_NAMED_PTFX_ASSET,0x416ba6ce9f7e648b, RemoveNamedPtFxAsset);

	SCR_REGISTER_SECURE(SET_VEHICLE_POPULATION_BUDGET,0xa121d58142738a8d, SetVehiclePopulationBudget);
	SCR_REGISTER_SECURE(SET_PED_POPULATION_BUDGET,0x8c5e1b04c94ff212, SetPedPopulationBudget);

	SCR_REGISTER_SECURE(CLEAR_FOCUS,0x5639e771f6085bf6, ClearFocus);
	SCR_REGISTER_SECURE(SET_FOCUS_POS_AND_VEL,0x7d5c42a7cdb11db6, SetFocusPosAndVel);
	SCR_REGISTER_SECURE(SET_FOCUS_ENTITY,0x34582635da718e5a, SetFocusEntity);
	SCR_REGISTER_SECURE(IS_ENTITY_FOCUS,0xf184bef0dbd3ad20, IsEntityFocus);
	SCR_REGISTER_SECURE(SET_RESTORE_FOCUS_ENTITY,0xe35fcef2127afccf, SetRestoreFocusEntity);

	SCR_REGISTER_SECURE(SET_MAPDATACULLBOX_ENABLED,0xa624331352defdcd, SetMapDataCullBoxEnabled);
	SCR_REGISTER_SECURE(SET_ALL_MAPDATA_CULLED,0x4c6cbcfdd1730e10, SetAllMapDataCulled);
	SCR_REGISTER_UNUSED(SET_HORIZON_OBJECTS_CULLED,0x0243893e8540fa67, SetHorizonObjectsCulled);

	SCR_REGISTER_SECURE(STREAMVOL_CREATE_SPHERE,0xd6d670d39e5eb332, CommandStreamVolCreateSphere);
	SCR_REGISTER_SECURE(STREAMVOL_CREATE_FRUSTUM,0xedf40cc24ee4ff61, CommandStreamVolCreateFrustum);
	SCR_REGISTER_SECURE(STREAMVOL_CREATE_LINE,0x83a445abf2242661, CommandStreamVolCreateLine);
	SCR_REGISTER_SECURE(STREAMVOL_DELETE,0x7de67eeefec38205, CommandStreamVolDelete);
	SCR_REGISTER_SECURE(STREAMVOL_HAS_LOADED,0x247b9cd82fd64f32, CommandStreamVolHasLoaded);
	SCR_REGISTER_SECURE(STREAMVOL_IS_VALID,0x52d132f41e04cb67, CommandStreamVolIsValid);
	SCR_REGISTER_SECURE(IS_STREAMVOL_ACTIVE,0xff4a0a9572c2f565, CommandIsStreamVolActive);

	SCR_REGISTER_SECURE(NEW_LOAD_SCENE_START,0xb37c5672ee23d04f, CommandNewLoadSceneStartFrustum);
	SCR_REGISTER_SECURE(NEW_LOAD_SCENE_START_SPHERE,0xca60df1d01dbd440, CommandNewLoadSceneStartSphere);
	SCR_REGISTER_SECURE(NEW_LOAD_SCENE_STOP,0xee3a19a84a10f8b9, CommandNewLoadSceneStop);
	SCR_REGISTER_SECURE(IS_NEW_LOAD_SCENE_ACTIVE,0x2981c54770e1d19c, CommandIsNewLoadSceneActive);
	SCR_REGISTER_SECURE(IS_NEW_LOAD_SCENE_LOADED,0x8a6ab093d1ee5368, CommandIsNewLoadSceneLoaded);

	SCR_REGISTER_SECURE(IS_SAFE_TO_START_PLAYER_SWITCH,0x3510bf4043201732, CommandIsSafeToStartPlayerSwitch);
	SCR_REGISTER_SECURE(START_PLAYER_SWITCH,0xccd7aa32a884016d, CommandStartPlayerSwitch);
	SCR_REGISTER_SECURE(STOP_PLAYER_SWITCH,0x310f3c51704851d4, CommandStopPlayerSwitch);
	SCR_REGISTER_SECURE(IS_PLAYER_SWITCH_IN_PROGRESS,0x04458b4e5d9e466a, CommandIsPlayerSwitchInProgress);
	SCR_REGISTER_SECURE(GET_PLAYER_SWITCH_TYPE,0xb6bdac890771ed04, CommandGetPlayerSwitchType);
	SCR_REGISTER_SECURE(GET_IDEAL_PLAYER_SWITCH_TYPE,0x22c67338735160aa, CommandGetIdealPlayerSwitchType);
	SCR_REGISTER_SECURE(GET_PLAYER_SWITCH_STATE,0xeefb36b938654045, CommandGetPlayerSwitchState);
	SCR_REGISTER_SECURE(GET_PLAYER_SHORT_SWITCH_STATE,0xaa8fd727e9cd9929, CommandGetPlayerShortSwitchState);
	SCR_REGISTER_SECURE(SET_PLAYER_SHORT_SWITCH_STYLE,0xe48e4b99dba9eaa1, CommandSetPlayerShortSwitchStyle);
	SCR_REGISTER_SECURE(GET_PLAYER_SWITCH_JUMP_CUT_INDEX,0x39c67356e2acaf22, CommandGetPlayerSwitchJumpCutIndex);
	SCR_REGISTER_SECURE(SET_PLAYER_SWITCH_OUTRO,0xd6466d9b50e967e5, CommandSetPlayerSwitchOutro);
	SCR_REGISTER_SECURE(SET_PLAYER_SWITCH_ESTABLISHING_SHOT,0x6c31ccf5981b2c06, CommandSetPlayerSwitchEstablishingShot);
	SCR_REGISTER_SECURE(ALLOW_PLAYER_SWITCH_PAN,0x4e554357e32f827c, CommandAllowPlayerSwitchPan);
	SCR_REGISTER_SECURE(ALLOW_PLAYER_SWITCH_OUTRO,0x92fe9bfdb58c08b1, CommandAllowPlayerSwitchOutro);
	SCR_REGISTER_SECURE(ALLOW_PLAYER_SWITCH_ASCENT,0x01aa29ae1a82b574, CommandAllowPlayerSwitchAscent);
	SCR_REGISTER_SECURE(ALLOW_PLAYER_SWITCH_DESCENT,0x2192f61b6fb23e19, CommandAllowPlayerSwitchDescent);
	SCR_REGISTER_UNUSED(IS_SWITCH_READY_FOR_ASCENT,0xa7589c3c447ac818, CommandIsSwitchReadyForAscent);
	SCR_REGISTER_SECURE(IS_SWITCH_READY_FOR_DESCENT,0xf7fa5e5f48d7ef30, CommandIsSwitchReadyForDescent);
	SCR_REGISTER_SECURE(ENABLE_SWITCH_PAUSE_BEFORE_DESCENT,0xe28d19155d6799a8, CommandEnableSwitchPauseBeforeDescent);
	SCR_REGISTER_SECURE(DISABLE_SWITCH_OUTRO_FX,0x3e99c894fa419af2, CommandDisableSwitchOutroFX);
	SCR_REGISTER_SECURE(SWITCH_TO_MULTI_FIRSTPART,0xa1bc090595ba935f, CommandSwitchToMultiFirstPart);
	SCR_REGISTER_SECURE(SWITCH_TO_MULTI_SECONDPART,0xbaac2fbaae6257db, CommandSwitchToMultiSecondPart);
	SCR_REGISTER_SECURE(IS_SWITCH_TO_MULTI_FIRSTPART_FINISHED,0x5e9ec8eea03bcec7, CommandIsSwitchToMultiFirstPartFinished);
	SCR_REGISTER_SECURE(GET_PLAYER_SWITCH_INTERP_OUT_DURATION,0x78fd52dd96d53522, CommandGetPlayerSwitchInterpOutDuration);
	SCR_REGISTER_SECURE(GET_PLAYER_SWITCH_INTERP_OUT_CURRENT_TIME,0x5ad9f5341c05bc7d, CommandGetPlayerSwitchInterpOutCurrentTime);
	SCR_REGISTER_SECURE(IS_SWITCH_SKIPPING_DESCENT,0x9fd60deda01f20a5, CommandIsSwitchSkippingDescent);

	SCR_REGISTER_SECURE(SET_SCENE_STREAMING_TRACKS_CAM_POS_THIS_FRAME,0xa63f1768421c7a80, CommandSetSceneStreamingTracksCamPosThisFrame);

	SCR_REGISTER_SECURE(GET_LODSCALE,0xb32301583643b6b3, CommandGetLodScale);
	SCR_REGISTER_SECURE(OVERRIDE_LODSCALE_THIS_FRAME,0x9b8b94a1196f345f, CommandOverrideLodScaleThisFrame);
	SCR_REGISTER_SECURE(REMAP_LODSCALE_RANGE_THIS_FRAME,0x34666baa70bf66df, CommandRemapLodScaleRangeThisFrame);
	SCR_REGISTER_SECURE(SUPPRESS_HD_MAP_STREAMING_THIS_FRAME,0xb1a39942d4e31aa5, CommandSuppressHdMapStreamingThisFrame);
	SCR_REGISTER_SECURE(SET_RENDER_HD_ONLY,0xd2726749ba8dac02, CommandSetRenderHdOnly);
	SCR_REGISTER_SECURE(FORCE_ALLOW_TIME_BASED_FADING_THIS_FRAME,0x7426ead34c015246, CommandForceAllowTimeBasedFadingThisFrame);

	SCR_REGISTER_SECURE(IPL_GROUP_SWAP_START,0x25cf3b0410cd653c, CommandIplGroupSwapStart);
	SCR_REGISTER_SECURE(IPL_GROUP_SWAP_CANCEL,0x8de9b7e224bcb723, CommandIplGroupSwapCancel);
	SCR_REGISTER_SECURE(IPL_GROUP_SWAP_IS_READY,0x329d6926716ee57f, CommandIplGroupSwapIsReady);
	SCR_REGISTER_SECURE(IPL_GROUP_SWAP_FINISH,0x015da57f7a0fa1a6, CommandIplGroupSwapFinish);
	SCR_REGISTER_SECURE(IPL_GROUP_SWAP_IS_ACTIVE,0x29978d4627985414, CommandIplGroupSwapIsActive);

	SCR_REGISTER_UNUSED(ENABLE_EXTRA_DISTANTLIGHTS,0x0ea9823cbd5fc533, CommandEnableExtraDistantLights);
	SCR_REGISTER_UNUSED(DISABLE_EXTRA_DISTANTLIGHTS,0x04ea50ecc60d4da3, CommandDisableExtraDistantLights);

	SCR_REGISTER_SECURE(PREFETCH_SRL,0x0728c4d61e5ace06, CommandPrefetchSrl);
	SCR_REGISTER_SECURE(IS_SRL_LOADED,0xd66fb6b74ee3da66, CommandIsSrlLoaded);
	SCR_REGISTER_SECURE(BEGIN_SRL,0x670baa84466115ca, CommandBeginSrl);
	SCR_REGISTER_SECURE(END_SRL,0x7e8f5ae44588d398, CommandEndSrl);
	SCR_REGISTER_SECURE(SET_SRL_TIME,0x7e887f52de52d931, CommandSetSrlTime);
	SCR_REGISTER_SECURE(SET_SRL_POST_CUTSCENE_CAMERA,0xf5d67a1497505f00, CommandSetSrlPostCutsceneCamera);
	SCR_REGISTER_SECURE(SET_SRL_READAHEAD_TIMES,0x125f5e3a0150ab0d, CommandSetSrlReadAheadTimes);
	SCR_REGISTER_SECURE(SET_SRL_LONG_JUMP_MODE,0xa694a53a397fc676, CommandSetSrlLongJumpMode);
	SCR_REGISTER_SECURE(SET_SRL_FORCE_PRESTREAM,0xa8b7321f953cff17, CommandSetSrlForcePrestream);

	SCR_REGISTER_SECURE(SET_HD_AREA,0x74f17c1a1b5f2f56, CommandSetHdArea);
	SCR_REGISTER_SECURE(CLEAR_HD_AREA,0x41bb0bac020b994f, CommandClearHdArea);

	SCR_REGISTER_SECURE(INIT_CREATOR_BUDGET,0x3e2d4acdeb176503, CommandInitCreatorBudget);
	SCR_REGISTER_SECURE(SHUTDOWN_CREATOR_BUDGET,0xdd631a688bd16327, CommandShutdownCreatorBudget);
	SCR_REGISTER_UNUSED(HAS_ROOM_FOR_MODEL_IN_CREATOR_BUDGET,0x9fa7ce1e863f69fb, CommandHasRoomForModelInCreatorBudget);
	SCR_REGISTER_SECURE(ADD_MODEL_TO_CREATOR_BUDGET,0x66eb6fa2c84f8e14, CommandAddModelToCreatorBudget);
	SCR_REGISTER_SECURE(REMOVE_MODEL_FROM_CREATOR_BUDGET,0xac51edc6ac06cfee, CommandRemoveModelFromCreatorBudget);
	SCR_REGISTER_SECURE(GET_USED_CREATOR_BUDGET,0x67b39fa5cb56f775, CommandGetUsedCreatorBudget);

	SCR_REGISTER_SECURE(SET_ISLAND_ENABLED,0x17d70b6d85547996, CommandSetIslandEnabled);
	SCR_REGISTER_UNUSED(ARE_ISLAND_IPLS_ACTIVE,0x24bed3d8303d66b6, CommandAreIslandIPLsActive);

#if GEN9_STANDALONE_ENABLED
	SCR_REGISTER_UNUSED(IS_GAME_INSTALLED,0xa6fa11b890aaa882, CommandIsGameInstalled);
#endif
}

};
