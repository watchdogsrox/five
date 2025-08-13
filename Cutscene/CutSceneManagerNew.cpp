
#include "Cutscene/CutSceneManagerNew.h"

//Rage Headers
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "cranimation/vcrwidget.h"
#include "crmetadata/tag.h"
#include "crmetadata/tagiterators.h"
#include "cutfile/cutfevent.h"
#include "cutfile/cutfobject.h"
#include "cutfile/cutfeventargs.h"
#include "cutfile/cutfile2.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutsanimentity.h"
#include "event/ShockingEvents.h"
#include "grcore/debugdraw.h"
#include "grcore/edgeExtractgeomspu.h"
#include "grcore/indexbuffer.h"
#include "grcore/matrix43.h"
#include "grprofile/timebars.h"
#include "file/default_paths.h"
#include "phbound/boundcomposite.h"
#include "streaming/streaming_channel.h"

//fw headers
#include "fwscene/stores/mapdatastore.h"
#include "fwsys/fileExts.h"

//Game Headers
#include "animation/debug/AnimDebug.h"
#include "animation/MoveVehicle.h"
#include "audio/cutsceneaudioentity.h"
#include "audio/radioaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/ReplayBufferMarker.h"
#include "core/game.h"
#include "cutscene/AnimatedModelEntity.h"
#include "cutscene/CutsceneCustomEvents.h"
#include "cutscene/CutSceneAnimManager.h"
#include "cutscene/CutSceneBoundsEntity.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "cutscene/CutSceneFadeEntity.h"
#include "cutscene/CutSceneLightEntity.h"
#include "cutscene/CutSceneParticleEffectEntity.h"
#include "cutscene/CutSceneOverlayEntity.h"
#include "cutscene/CutSceneSoundEntity.h"

#include "debug/Debug.h"
#include "debug/Bar.h"
#include "debug/DebugScene.h"
#include "debug/FrameDump.h"
#include "debug/Editing/CutsceneEditing.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "frontend/PauseMenu.h"
#include "frontend/MiniMap.h"
#include "frontend/MobilePhone.h"
#include "frontend/HudTools.h"
#include "game/clock.h"
#include "modelinfo/PedModelInfo.h"
#include "objects/object.h"
#include "objects/objectintelligence.h"
#include "network/Live/NetworkTelemetry.h"
#include "pathserver/pathserver.h"
#include "peds/Ped.h"
#include "peds/rendering/PedVariationPack.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/rendering/PedVariationStream.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightEntity.h"
#include "renderer/PlantsGrassRenderer.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderTargetMgr.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "scene/debug/ObjExporter.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/portals/Portal.h"
#include "streaming/streaming.h"			// For CStreaming::LoadAllRequestedObjects(), etc.
#include "streaming/streamingrequestlist.h"
#include "system/bootmgr.h"
#include "system/controlMgr.h"
#include "text/messages.h"
#include "text/TextConversion.h"
#include "vfx/misc/Fire.h"

#include "Network/Live/NetworkDebugTelemetry.h"
#include "string/stringutil.h"

#if __BANK
#include "cutscene/CutsceneDebugMetadata_parser.h"
#endif

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
#include "cutscene/AuthorizedCutsceneMetadata_parser.h"
#endif

//channel
#include "Cutscene/cutscene_channel.h"

ANIM_OPTIMISATIONS ()
RAGE_DEFINE_CHANNEL(cutscene)

PARAM(nocuttext, "[cutscene] Don't display debug text during cutscenes");
PARAM(nocutarrowkeys, "[cutscene] Enable keyboard arrow keys for cutscene playback control");
PARAM(rawcutsceneauthorizeddata, "[cutscene] Ignore the script preappoval list"); 
PARAM(nocutsceneauthorization, "[cutscene] Ignore the script authorized list"); 
PARAM(cutscenecallstacklogging, "add call stacks set matrix, visibilty and deletion calls");
PARAM(cutsceneverboseexitstatelogging, "Enable verbose logging to help debug exit state issues.");

#if __BANK

#define OBJ_CAPTURE_PATH "x:/gta5/art/animation/resources/sets/obj/"

bool CutSceneManager::m_displayActiveLightsOnly = true;
bool CutSceneManager::m_bRenderCutsceneStaticLight = true;
bool CutSceneManager::m_bRenderAnimatedLights = true;

CutSceneManager::DebugRenderState CutSceneManager::ms_DebugState;
#endif


#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
bool CutSceneManager::m_RenderUnauthorizedScreen; 
bool CutSceneManager::m_RenderUnauthorizedWaterMark = false; 
bool CutSceneManager::m_IsAuthorizedForPlayback = false; 
u32 CutSceneManager::m_RenderUnauthorizedScreenTimer; 
#endif

#if USE_MULTIHEAD_FADE
bool CutSceneManager::m_bBlindersUp = false;
bool CutSceneManager::m_bManualControl = false;
int CutSceneManager:: m_iBlinderDelay = 0;
#endif // USE_MULTIHEAD_FADE

int CutSceneManager::m_RenderBufferIndex = 0; 
sysCriticalSectionToken CutSceneManager::sm_CutsceneLock; 

bank_float g_TweakTimeStep = -100.0f;

#if __WIN32PC
namespace rage {
	XPARAM(frameLimit);
}
#endif

#if ENABLE_CUTSCENE_TELEMETRY

//////////////////////////////////////
//		CutsceneLightsTelemetry
//////////////////////////////////////

CutsceneLightsTelemetry	g_CutSceneLightTelemetryCollector;

void	CutsceneLightsTelemetry::CutSceneStart(const char *pName, bool bIsCutsceneCameraApproved, bool bIsCutsceneLightingApproved)
{
	m_Name = pName;			// Set name
	Reset();				// Reset timing stat var
	m_CameraApproved = bIsCutsceneCameraApproved;
	m_LightingApproved = bIsCutsceneLightingApproved;
	m_Started = true;		// Tell class we've started
}

void	CutsceneLightsTelemetry::CutSceneStop()
{
	m_Started = false;
}

// Get data from the budget display class
void	CutsceneLightsTelemetry::Update(CutSceneManager *pManager)
{
	if(m_Started && pManager->IsPlaying())		// Only capture when playing, not paused
	{
#if RAGE_TIMEBARS
		float totalTime;
		float time;
		int count;

		// Directional Lights
		::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Directional/Ambient/Reflections", count, time);
		m_DirectionalLightsTimeSample.AddSample(time);
		totalTime = time;

		// Scene Lights
		::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Scene Lights", count, time);
		m_SceneLightsTimeSample.AddSample(time);
		totalTime += time;

		// LOD Lights
		::rage::g_pfTB.GetGpuBar().GetFunctionTotals("LOD Lights", count, time);
		m_LODLightsTimeSample.AddSample(time);
		totalTime += time;

		// Total Time
		m_TotalLightsTimeSample.AddSample(totalTime);
#endif // RAGE_TIMEBARS
		m_DOFWasActive |= pManager->IsDepthOfFieldEnabled();
	}

	// Did we just stop?
	if(m_WasStarted && !m_Started)
	{
		m_OutputNow = true;
	}

	m_WasStarted = m_Started;
}

// NOTE: Resets after returning true
// If the calling code doesn't act on this, then it's unlucky
bool	CutsceneLightsTelemetry::ShouldOutputTelemetry()
{
	bool	retVal = m_OutputNow;
	m_OutputNow = false;
	return retVal;
}


#endif	// ENABLE_CUTSCENE_TELEMETRY


#if USE_MULTIHEAD_FADE
namespace fade
{
	static	bool	bFadingIn	= true;
	static	bool	bInstant	= false;
	static	u32		uEndTime	= 0;
	static	u32		uDuration	= 1;
	static	float	fAcceleration	= 1.f;
	static	float	fWaveSize		= 0.0f;
	static	float	fAlphaOffset	= 0.5f;
	static	u32		uLastMonitorSyncTimestamp = 0;

#if __BANK
	static	void bankFadeIn()	{ CutSceneManager::StartMultiheadFade(true, bInstant); CutSceneManager::SetManualBlinders(true);}
	static	void bankFadeOut()	{ CutSceneManager::StartMultiheadFade(false);}

	static	void addWidgets(bkBank &bank)
	{
		bank.PushGroup("Multi-head Fade", false);
		bank.AddToggle("Instant",		&bInstant);
		bank.AddSlider("Duration",		&uDuration, 0, 10, 1);
		bank.AddSlider("Acceleration",	&fAcceleration, 0.1f, 10.f, 0.1f);
		bank.AddSlider("Wave Size",		&fWaveSize,	0.0f, 1.f, 0.01f);
		bank.AddSlider("Alpha Offset",	&fAlphaOffset, 0.f, 2.f, 0.1f);
		bank.AddButton("Fade In",		datCallback(bankFadeIn));
		bank.AddButton("Fade Out",		datCallback(bankFadeOut));
		bank.PopGroup();
	}
#endif	//__BANK
}
PARAM(cutscene_multihead,"[cutscene] Expand cutscenes on all of the screens in multi-head configuration");
#endif	//USE_MULTIHEAD_FADE


///////////////////////////////////////////////////////////////////////////////////////////////////
//Registers a game that the cutscene will use instead of creating
void CutSceneManager::RegisterGameEntity(CDynamicEntity* pEntity, atHashString& SceneHandleHash, atHashString& ModelNameHash, bool bDeleteBeforeEnd, bool bCreatedForScript, bool bAppearInScene, u32 options)
{
	CCutsceneAnimatedModelEntity* pAnimModelEnt = NULL;

	// Currently only allow forced replacement of the ped animated by the cutscene.
	if (!bAppearInScene)
	{
#if !__NO_OUTPUT
		if ((options&CCutsceneAnimatedModelEntity::CEO_IGNORE_MODEL_NAME)!=0)
		{
			cutsceneManagerDebugf1("Removing flag CEO_IGNORE_MODEL_NAME - Entity %s(%s) is registered not to appear in the scene", SceneHandleHash.TryGetCStr() ? SceneHandleHash.TryGetCStr() : "null", ModelNameHash.TryGetCStr() ? ModelNameHash.TryGetCStr() : "null");
		}
#endif //!__NO_OUTPUT
		options&=(~CCutsceneAnimatedModelEntity::CEO_IGNORE_MODEL_NAME);
	}

	pAnimModelEnt = GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

	//store the frame count of the first register call to make sure that we have not registered.
	if(!m_bHasScriptRegisteredAnEntity)
	{
		m_uFrameCountOfFirstRegisterCall = fwTimer::GetFrameCount();
		m_bHasScriptRegisteredAnEntity = true;
	}

	if(pAnimModelEnt)
	{
#if __ASSERT
		if(bCreatedForScript)
		{
			if(pAnimModelEnt->IsBlockingVariationStreamingAndApplication())
			{
				cutsceneAssertf(0, "Entity %s(%s): Variation streaming has been blocked so the cut scene cannot succesfully create a game object", SceneHandleHash.TryGetCStr() ? SceneHandleHash.TryGetCStr() : "null", ModelNameHash.TryGetCStr() ? ModelNameHash.TryGetCStr() : "null"); 
			}
		}
		if (NetworkInterface::IsGameInProgress() && bAppearInScene && (options&CCutsceneAnimatedModelEntity::CEO_IGNORE_MODEL_NAME)!=0 && pEntity && pEntity->GetIsTypeVehicle())
		{
			// if script is asking us to animate a specific vehicle model, check that the model they passed in matches the one the cutscene was made with.
			// Since vehicle skeletons aren't standardised, we can't share animations between them so we need to catch cases where this happens.
			SCRIPT_ASSERTF(pEntity->GetModelNameHash()==pAnimModelEnt->GetModelNameHash(), "Script has registered a mismatched vehicle model '%s' to the cutscene handle'%s'. Expecting model '%s'. The vehicle animation will likely look broken.", pEntity->GetModelName(), pAnimModelEnt->GetSceneHandleHash().GetCStr(), pAnimModelEnt->GetModelName());
		}
#endif
		
		pAnimModelEnt->m_RegisteredEntityFromScript.SceneNameHash = SceneHandleHash;
		pAnimModelEnt->m_RegisteredEntityFromScript.ModelNameHash = ModelNameHash;
		pAnimModelEnt->m_RegisteredEntityFromScript.pEnt = pEntity;
		pAnimModelEnt->m_RegisteredEntityFromScript.bDeleteBeforeEnd = bDeleteBeforeEnd;
		pAnimModelEnt->m_RegisteredEntityFromScript.bCreatedForScript = bCreatedForScript;
		pAnimModelEnt->m_RegisteredEntityFromScript.bAppearInScene = bAppearInScene;

		// Respect options flags if already set
		if (pAnimModelEnt->GetOptionFlags() == 0)
		{
			pAnimModelEnt->SetOptionFlags(options);
		}
		cutsceneManagerDebugf1("RegisterGameEntity: pEntity (%s)  SceneHandleHash (%d)  ModelNameHash (%d)  bDeleteBeforeEnd (%d)  bCreatedForScript (%d)  bAppearInScene (%d)\n", pAnimModelEnt->GetModelName(), SceneHandleHash.GetHash(), ModelNameHash.GetHash(), bDeleteBeforeEnd, bCreatedForScript, bAppearInScene);
	}
#if !__NO_OUTPUT
	else
	{
		Assertf(pAnimModelEnt, "CUTSCENE: Could not find model entity (\"%s\" %u) from scene handle (\"%s\" %u), scripters please check the scene handle is correct!", ModelNameHash.TryGetCStr(), ModelNameHash.GetHash(), SceneHandleHash.TryGetCStr(), SceneHandleHash.GetHash());
	}
#endif
}

void CutSceneManager::ChangeCutSceneModel(atHashString& sceneHashString, atHashString& modelNameHash, atHashString& newModelNameHash)
{
	if(GetCutfFile())
	{
		const atArray<cutfObject*>& ObjectList = GetCutfFile()->GetObjectList();

		for(int i=0; i < ObjectList.GetCount(); i++ )
		{
			if(ObjectList[i] && ObjectList[i]->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
			{
				cutfModelObject* pModel = static_cast<cutfModelObject*>(ObjectList[i]); 

				if(pModel && pModel->GetHandle() == sceneHashString.GetHash() && (modelNameHash.GetHash() == 0 || pModel->GetStreamingName() == modelNameHash.GetHash()))
				{
					cutsceneManagerDebugf1("Changing model for scene handle: %s (model: %s) to %s", sceneHashString.TryGetCStr() ? sceneHashString.TryGetCStr() : "null", modelNameHash.TryGetCStr() ? modelNameHash.TryGetCStr() : "null", newModelNameHash.TryGetCStr() ? newModelNameHash.TryGetCStr() : "null");				
					pModel->OverrideStreamingName(newModelNameHash); 
				}
			}
		}
	}
}

void CutSceneManager::SetCutSceneEntityStreamingFlags(atHashString& sceneHashString, atHashString& modelNameHash, u32 flags)
{
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		cutsEntity* pEntity = entry.GetData().pEntity;
		if(pEntity && (pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
			|| pEntity->GetType() == CUTSCENE_WEAPON_GAME_ENTITY || pEntity->GetType() == CUTSCENE_PROP_GAME_ENITITY))
		{
			CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntity);
			if(pModel && pModel->GetSceneHandleHash() == sceneHashString.GetHash() && (modelNameHash.GetHash() == 0 || pModel->GetModelNameHash() == modelNameHash.GetHash()))
			{
				pModel->SetStreamingOptionFlags(flags); 
				cutsceneManagerDebugf1("SetCutSceneEntityStreamingFlags: Scene handle: %s, Model: %s, flags: %d",sceneHashString.TryGetCStr(), modelNameHash.TryGetCStr(), flags);				
			}
		}
	}
}

void CutSceneManager::SetCutScenePedVariationFromPed(atHashString& sceneHashString, const CPed* pPed, atHashString& modelNameHash)
{
	if (pPed)
	{
		cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariationFromPed: Searching for ped (%s) and model (%s)", pPed->GetModelName(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : modelNameHash.TryGetCStr());

		// If ModelHash is defined sets the variation on the given model otherwise sets the variation on all matching handles
		atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
		for ( entry.Start(); !entry.AtEnd(); entry.Next() )
		{
			cutsEntity* pEntity = entry.GetData().pEntity;
			if(pEntity && pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
			{
				CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntity);
				if(pModel && pModel->GetSceneHandleHash() == sceneHashString.GetHash() && (modelNameHash.GetHash() == 0 || pModel->GetModelNameHash() == modelNameHash.GetHash()))
				{
					CCutsceneAnimatedActorEntity* pActorManager = static_cast<CCutsceneAnimatedActorEntity*>(pModel);
					if(pActorManager)
					{
						/*if(pActorManager->GetGameEntity())
						{
							pActorManager->CopyPedVariations(pPed, pActorManager->GetGameEntity());
							cutsceneManagerVariationDebugf("CutSceneManager::SetCutScenePedVariationFromPed::CopyPedVariations");
						}
						else
						{*/
							for(int iComponent=0; iComponent < PV_MAX_COMP; iComponent++)
							{
								u32 iDrawableId = CPedVariationPack::GetCompVar(pPed, static_cast<ePedVarComp>(iComponent));
								u8 TextureVarId = CPedVariationPack::GetTexVar(pPed, static_cast<ePedVarComp>(iComponent));
								pActorManager->SetScriptActorVariationData(iComponent, iDrawableId, TextureVarId);
						}
							cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariationFromPed::SetInitialActorVariation");
						//}

						// Early out if we have found the specific handle
						if (pModel->GetModelNameHash().GetHash() == modelNameHash.GetHash())
						{
							cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariationFromPed: Matched ped (%s) and specific model (%s)", pPed->GetModelName(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : modelNameHash.TryGetCStr() );
							return;
						}

						cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariationFromPed: Matched ped (%s) and wildcard model (%s)", pPed->GetModelName(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : modelNameHash.TryGetCStr() );
					}
				}
			}
		}
	}
}

void CutSceneManager::SetCutScenePedVariation(atHashString& sceneHashString, int ComponentID, int DrawableID, int TextureID, atHashString& modelNameHash)
{
	cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariation: Searching for handle (%s) and model (%s)", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : modelNameHash.TryGetCStr() );

	// If ModelHash is defined sets the variation on the given model otherwise sets the variation on all matching handles
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		cutsEntity* pEntity = entry.GetData().pEntity;
		if(pEntity && pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
		{
			CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntity);
			if(pModel && pModel->GetSceneHandleHash() == sceneHashString.GetHash() && (modelNameHash.GetHash() == 0 || pModel->GetModelNameHash() == modelNameHash.GetHash()))
			{
				CCutsceneAnimatedActorEntity* pActorManager = static_cast<CCutsceneAnimatedActorEntity*>(pModel);
				if(pActorManager)
				{
					pActorManager->SetScriptActorVariationData(ComponentID, DrawableID, TextureID);

					// Early out if we have found the specific handle
					if (pModel->GetModelNameHash() == modelNameHash.GetHash())
					{
						cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariation::SetInitialActorVariation: Matched handle (%s) and specific model (%s)", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : pModel->GetModelName() );
						return;
					}

					cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedVariation::SetInitialActorVariation Matched handle (%s) and wildcard model (%s))", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : pModel->GetModelName() );
				}
			}
		}
	}
}

void CutSceneManager::SetCutScenePedPropVariation(atHashString& sceneHashString, int Position, int NewPropIndex, int NewTextIndex, atHashString& modelNameHash)
{
	cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedPropVariation: Searching for handle (%s) and model (%s)", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : modelNameHash.TryGetCStr() );

	// If ModelHash is defined sets the variation on the given model otherwise sets the variation on all matching handles
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		cutsEntity* pEntity = entry.GetData().pEntity;
		if(pEntity && pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
		{
			CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntity);
			if(pModel && pModel->GetSceneHandleHash() == sceneHashString.GetHash() && (modelNameHash.GetHash() == 0 || pModel->GetModelNameHash() == modelNameHash.GetHash()))
			{
				CCutsceneAnimatedActorEntity* pActorManager = static_cast<CCutsceneAnimatedActorEntity*>(pModel);
				if(pActorManager)
				{
					// the cut scene data adds this offset to make them prop specific
					pActorManager->SetScriptActorVariationData(Position + PV_MAX_COMP, NewPropIndex, NewTextIndex);

					// Early out if we have found the specific handle
					if (pModel->GetModelNameHash() == modelNameHash.GetHash())
					{
						cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedPropVariation::SetInitialActorVariation Matched handle (%s) and specific model (%s)", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : pModel->GetModelName());
						return;
					}

					cutsceneManagerVariationDebugf3("CutSceneManager::SetCutScenePedPropVariation::SetInitialActorVariation Matched handle (%s) and wildcard model (%s))", sceneHashString.TryGetCStr(), modelNameHash.GetHash() == 0 ? "Wildcard Model" : pModel->GetModelName());
				}
			}
		}
	}
}

CCutsceneAnimatedModelEntity* CutSceneManager::GetAnimatedModelEntityFromSceneHandle(atHashString& sceneHandleHash, atHashString& ModelHash)
{
	CCutsceneAnimatedModelEntity* pFinalModel = NULL;
	atArray<const cutfObject *> modelObjects;

	if(GetCutfFile())
	{
		GetCutfFile()->FindObjectsOfType( CUTSCENE_MODEL_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY || pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

					if(pModel && pModel->GetSceneHandleHash().GetHash() == sceneHandleHash.GetHash() )
					{
						pFinalModel = pModel;
					}

					if(pModel && pModel->GetSceneHandleHash().GetHash() == sceneHandleHash.GetHash() && pModel->GetModelNameHash().GetHash() == ModelHash.GetHash())
					{
						pFinalModel = pModel;
						return pFinalModel;
					}
				}
			}
		}
	}
	return pFinalModel;
}

void CutSceneManager::HideNonRegisteredModelEntities()
{
	atArray<const cutfObject *> modelObjects;

	if(GetCutfFile())
	{
		GetCutfFile()->FindObjectsOfType( CUTSCENE_MODEL_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY || pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = smart_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

					if (!pModel->IsRegisteredWithScript())
					{
						if (pModel->GetGameEntity())
						{
							pModel->GetGameEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false);
							cutsceneManagerVisibilityDebugf3("CutSceneManager::HideNonRegisteredModelEntities: Hiding GetGameEntity (%s)", pModel->GetGameEntity()->GetModelName());
						}
					}
				}
			}
		}
	}
}


bool CutSceneManager::HasScriptVisibleTagPassedForEntity(const CEntity *pEntity, s32 EventHash)
{
	atArray<const cutfObject *> modelObjects;
	if(GetCutfFile())
	{
		GetCutfFile()->FindObjectsOfType( CUTSCENE_MODEL_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject && pEntityObject->pEntity)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY|| pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

					if(pModel && pModel->GetGameEntity() == pEntity )
					{
						return pModel->HasScriptVisibleTagPassed(EventHash); 
					
					}
				}
			}
		}
	}
	return false; 
}


CCutsceneAnimatedModelEntity* CutSceneManager::GetAnimatedModelEntityFromModelHash(atHashString& ModelNameHash)
{
	CCutsceneAnimatedModelEntity* pFinalModel = NULL;
	atArray<const cutfObject *> modelObjects;
	if(GetCutfFile())
	{
		GetCutfFile()->FindObjectsOfType( CUTSCENE_MODEL_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject && pEntityObject->pEntity)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY|| pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

					if(pModel && pModel->GetModelNameHash().GetHash() == ModelNameHash.GetHash() )
					{
						pFinalModel = pModel;
					}
				}
			}
		}
	}
	return pFinalModel;
}

CCutsceneAnimatedModelEntity* CutSceneManager::GetAnimatedModelEntityFromEntity(const CEntity *pEntity)
{
	atArray<const cutfObject *> modelObjects;
	if(GetCutfFile())
	{
		GetCutfFile()->FindObjectsOfType( CUTSCENE_MODEL_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject && pEntityObject->pEntity)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY|| pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

					if(pModel && (pModel->GetGameEntity() == pEntity || pModel->GetGameRepositionOnlyEntity() == pEntity))
					{
						return pModel;
					}
				}
			}
		}
	}

	return NULL; 
}


void CutSceneManager::GetEntityByType(s32 EntityType, atArray<cutsEntity*> &pEntityList)
{
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		SEntityObject *pEntityObject = m_cutsceneEntityObjects.Access(entry.GetKey());

		if(pEntityObject && pEntityObject->pEntity->GetType() == EntityType)
		{
			if(pEntityObject->pEntity)
			{
				pEntityList.Grow() = pEntityObject->pEntity;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//check that the phase of cut scene is 1.0, the final frame of the scene

bool CutSceneManager::CanSetCutSceneExitStateForEntity(atHashString& SceneHandleHash, atHashString& ModelNameHash)
{
	CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

	bool rv = false;
	int reason = 0;
	if(pAnimModelEnt)
	{
		reason = 1;
		if ( pAnimModelEnt->m_RegisteredEntityFromScript.pEnt)
		{
			reason = 2;
			if (!pAnimModelEnt->IsRegisteredGameEntityUnderScriptControl())
			{
						reason = 3;
				if(pAnimModelEnt->m_RegisteredEntityFromScript.SceneNameHash.GetHash() == SceneHandleHash.GetHash())
				{
							reason = 4;
					if (pAnimModelEnt->HasStoppedBeenCalled())
					{
								reason = 5;
						if (!pAnimModelEnt->m_RegisteredEntityFromScript.bDeleteBeforeEnd)
						{
									reason = 6;
							s32 type = pAnimModelEnt->GetType();
							if( type == CUTSCENE_PROP_GAME_ENITITY || type == CUTSCENE_ACTOR_GAME_ENITITY || type == CUTSCENE_VEHICLE_GAME_ENITITY || type ==  CUTSCENE_WEAPON_GAME_ENTITY )
							{
										reason = -1;

								pAnimModelEnt->SetRegisteredGameEntityIsUnderScriptControl(true);
								if(!pAnimModelEnt->IsGameEntityReadyForGame())
								{
									pAnimModelEnt->SetGameEntityReadyForGame(this);
								}

								if (type==CUTSCENE_ACTOR_GAME_ENITITY)
								{
									// must do at least a post camera ai update on the ped, or we'll get a bind pose next frame.
									CCutsceneAnimatedActorEntity* pActorEnt = static_cast<CCutsceneAnimatedActorEntity*>(pAnimModelEnt);
									if (pActorEnt->GetGameEntity())
									{
										pActorEnt->GetGameEntity()->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate, true);
										pActorEnt->GetGameEntity()->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAiAnimUpdateIfFirstPerson, true);
									}
								}
								rv = true;
#if __BANK
								if (!m_bVerboseExitStateLogging)
								{
									cutsceneManagerDebugf3("CanSetCutSceneExitStateForEntity: pAnimModelEnt (%s) result (%s) SceneHandleHash (%d)  ModelNameHash (%d)\n", pAnimModelEnt ? pAnimModelEnt->GetModelName() : "NULL", rv ? "TRUE" : "FALSE", SceneHandleHash.GetHash(), ModelNameHash.GetHash());
								}
#endif // __BANK
							}
						}
					}
				}
			}
		}
	}

#if __BANK
	if (m_bVerboseExitStateLogging)
	{
		cutsceneManagerDebugf3("CanSetCutSceneExitStateForEntity: pAnimModelEnt (%s) result (%s) reason (%d) SceneHandleHash (%d)  ModelNameHash (%d)\n", pAnimModelEnt ? pAnimModelEnt->GetModelName() : "NULL", rv ? "TRUE" : "FALSE", reason, SceneHandleHash.GetHash(), ModelNameHash.GetHash());
	}
#endif // __BANK

	return rv;
}

bool CutSceneManager::CanSetCutSceneExitStateForCamera(bool bHideNonRegisteredEntities)
{	
	if(GetCutfFile())
	{
		atArray<const cutfObject *> modelObjects;
		GetCutfFile()->FindObjectsOfType( CUTSCENE_CAMERA_OBJECT_TYPE, modelObjects );

		for(int i =0; i < modelObjects.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(modelObjects[i]->GetObjectId());

			if(pEntityObject && pEntityObject->pEntity)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_CAMERA_GAME_ENITITY)
				{
					CCutSceneCameraEntity* pCam = static_cast<CCutSceneCameraEntity*>(pEntityObject->pEntity);

					if(!pCam->IsRegisteredGameEntityUnderScriptControl())
					{
						if(pCam->CanBlendOutThisFrame())
						{
							pCam->SetRegisteredGameEntityIsUnderScriptControl(true);
							pCam->SetCameraReadyForGame(this, CCutSceneCameraEntity::SCRIPT_REQUSTED_RETURN);

							if (GetCutfFile()->GetBlendOutCutsceneDuration() == 0)
							{
								cutsceneManagerDebugf3("CanSetCutSceneExitStateForCamera - Camera has instantly cut back to game");
								m_bCameraWillCameraBlendBackToGame = false;
							}

							if (bHideNonRegisteredEntities)
							{
								HideNonRegisteredModelEntities();
								cutsceneManagerDebugf3("CanSetCutSceneExitStateForCamera - hiding non registered entities (specified by script)");
							}
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//Check that that we can mess with the ped just before the scene starts.

bool CutSceneManager::CanSetCutSceneEnterStateForEntity(atHashString& SceneNameHash, atHashString& ModelNameHash)
{
	if (IsRunning())
	{
		CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneNameHash, ModelNameHash);

		if(pAnimModelEnt)
		{
			if(pAnimModelEnt->m_RegisteredEntityFromScript.SceneNameHash == SceneNameHash)
			{
				if(pAnimModelEnt->m_RegisteredEntityFromScript.pEnt && (pAnimModelEnt->m_RegisteredEntityFromScript.iEnterStatePhase >= GetCutScenePhase()))
				{
					return true;
				}
			}
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
bool CutSceneManager::HasSceneStartedInTheSameFrameAsObjectsAreRegistered()
{
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		SEntityObject *pEntityObject = m_cutsceneEntityObjects.Access(entry.GetKey());

		if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
			|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY|| pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
		{
			CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

			if(pModel && pModel->m_RegisteredEntityFromScript.SceneNameHash > 0)
			{
				if(fwTimer::GetFrameCount() != m_uFrameCountOfFirstRegisterCall)
				{
					return false;
				}
			}
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Skip blocked by anim tag on the player clip functionality

bool CutSceneManager::IsSkipBlockedByCameraAnimTag() const
{
	bool bSkipBlocked = false;

	if (m_fSkipBlockedCameraAnimTagTime >= 0.0f)
	{
		if (GetCutSceneCurrentTime() >= m_fSkipBlockedCameraAnimTagTime)
		{
			bSkipBlocked = true;
		}
	}

	return bSkipBlocked;
}

void CutSceneManager::SetSkippedBlockedByCameraAnimTagTime(float fAnimTagTime)
{
	if (m_fSkipBlockedCameraAnimTagTime == -1.0f)
	{
		m_fSkipBlockedCameraAnimTagTime = fAnimTagTime;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Set the seamless trigger origin of the scene

void CutSceneManager::SetSeamlessTriggerOrigin( const Vector3 &vTriggerCoord, bool bOverRide )
{
	sSeamlessTrigger.TriggerMat.Identity();
	if (bOverRide)
	{
		sSeamlessTrigger.TriggerMat.d = vTriggerCoord;
	}
	else
	{
		sSeamlessTrigger.TriggerMat.d = sSeamlessTrigger.vTriggerOffset;
	}

	sSeamlessTrigger.TriggerMat.RotateZ(sSeamlessTrigger.fTriggerOrient);

	//Get the scene matrix
	Matrix34 SceneMat(M34_IDENTITY);
	GetSceneOrientationMatrix(SceneMat);
	SceneMat.d = GetSceneOffset();

	//set our trigger into scene space
	sSeamlessTrigger.TriggerMat.Dot(SceneMat);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Set the seamless trigger area of the scene

void CutSceneManager::SetSeamlessTriggerArea(const Vector3 &TriggerPoint, float fTriggerRadius, float fTriggerOrientation, float fTriggerAngle  )
{
	SetSeamlessTriggerOrigin(TriggerPoint, true);

	sSeamlessTrigger.fTriggerOrient = DtoR * fTriggerOrientation;
	sSeamlessTrigger.fTriggerAngle = DtoR * fTriggerAngle;
	sSeamlessTrigger.fTriggerRadius = fTriggerRadius;
	sSeamlessTrigger.vTriggerOffset = TriggerPoint;
	sSeamlessTrigger.bScriptOveride = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//Check if the ped can trigger the seamless scene

bool CutSceneManager::CanPedTriggerSCS()
{
	CPed* pPlayer=CGameWorld::FindLocalPlayer();

	if (pPlayer)
	{
		//Set a unit vector down the y axis
		Vector3 vVector (0.0, 1.0f, 0.0f);

		//Set it into trigger space
		sSeamlessTrigger.TriggerMat.Transform3x3(vVector);

		Matrix34 mPlayer = MAT34V_TO_MATRIX34(pPlayer->GetMatrix());

		Vector3 vPlayer = mPlayer.d;

		vPlayer = vPlayer - sSeamlessTrigger.TriggerMat.d;

		float fPlayerDist = vPlayer.Mag();

		if(fPlayerDist < sSeamlessTrigger.fTriggerRadius)
		{
			//calculate the angle between the player and the scene origin only if we have a angle > 0.0
			if (sSeamlessTrigger.fTriggerAngle > 0.0f)
			{
				float fPlayerAngle = 0.0f;
				vPlayer.Normalize();
				fPlayerAngle = vPlayer.AngleZ(vVector);

				if (fPlayerAngle > 0.0f)
				{
					if ( fPlayerAngle > sSeamlessTrigger.fTriggerAngle * 0.5  )
					{
						sSeamlessTrigger.fPlayerAngle = sSeamlessTrigger.fTriggerAngle * 0.5f;

					}
					else
					{
						sSeamlessTrigger.fPlayerAngle = fPlayerAngle;
					}
				}
				else
				{
					if (fPlayerAngle < 0.0f)
					{
						if(fPlayerAngle > sSeamlessTrigger.fTriggerAngle * -0.5f)
						{
							sSeamlessTrigger.fPlayerAngle = fPlayerAngle;
						}
						else
						{
							sSeamlessTrigger.fPlayerAngle = sSeamlessTrigger.fTriggerAngle * -0.5f;
						}
					}
				}
			}
			else
			{
				sSeamlessTrigger.fPlayerAngle = 0.0f;
			}
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
//Gets the entity around the passed position

CEntity* CutSceneManager::GetEntityToAtPosition(const Vector3 &vPos, float fRadius, s32 iObjectModelIndex)
{
	ClosestObjectToHideDataStruct ClosestObjectData;
	ClosestObjectData.fClosestDistanceSquared = fRadius * fRadius * 4.0f;	//	set this to larger than the scan range (remember it's squared distance)
	ClosestObjectData.pClosestObject = NULL;

	ClosestObjectData.CoordToCalculateDistanceFrom = vPos;

	ClosestObjectData.ModelIndexToCheck = iObjectModelIndex;

	//	used to do a 2d distance check with FindObjectsOfTypeInRange
	spdSphere testSphere(RCC_VEC3V(ClosestObjectData.CoordToCalculateDistanceFrom), ScalarV(fRadius));
	fwIsSphereIntersecting searchSphere(testSphere);
	CGameWorld::ForAllEntitiesIntersecting(&searchSphere, GetClosestObjectCB, (void*) &ClosestObjectData,
		(ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT), (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS));

	return ClosestObjectData.pClosestObject;
}

//////////////////////////////////////////////////////////////////////
//Callback function when an object is found

bool CutSceneManager::GetClosestObjectCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	ClosestObjectToHideDataStruct* pClosestObjectData = reinterpret_cast<ClosestObjectToHideDataStruct*>(data);

	//reject objects animated by the cut scene.
	if(pEntity && pEntity->GetIsTypeObject())
	{
		const CObject* pObject = static_cast<const CObject*>(pEntity);

		if(pObject->GetObjectIntelligence())
		{
			CTask* pTask = pObject->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE);

			if(pTask)
			{
				return true;
			}
		}

		if (pObject->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE)
		{
			return true;
		}
	}

	//Make sure that the model index is the same as the object we passed in
	if (pEntity->GetModelIndex() != pClosestObjectData->ModelIndexToCheck)  // not the same model
		return true;

	//Get the distance between the centre of the locate and the entities position
	Vector3 DiffVector = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - pClosestObjectData->CoordToCalculateDistanceFrom;

	float ObjDistanceSquared = DiffVector.Mag2();

	//If the the distance is less than the radius store this as our entity
	if (ObjDistanceSquared < pClosestObjectData->fClosestDistanceSquared)
	{
		pClosestObjectData->pClosestObject = pEntity;  // we have found our model
		pClosestObjectData->fClosestDistanceSquared = ObjDistanceSquared;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//Sets the objects visibility

void CutSceneManager::SetObjectInAreaVisibility(const Vector3 &vPos, float fRadius, s32 iModelIndex, bool bVisble)
{
	CEntity* pFoundEntity = CutSceneManager::GetEntityToAtPosition(vPos, fRadius,iModelIndex);

	if(pFoundEntity && pFoundEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		CPhysical* pObject = static_cast<CPhysical*>(pFoundEntity);
		pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, bVisble);
		if (bVisble)
		{
			cutsceneManagerVisibilityDebugf3("CutSceneManager::SetObjectInAreaVisibility: Showing object (%p, %s)", pObject, pObject->GetModelName());
		}
		else
		{
			cutsceneManagerVisibilityDebugf3("CutSceneManager::SetObjectInAreaVisibility: Hiding object (%p, %s)", pObject, pObject->GetModelName());
		}
	}
}

//////////////////////////////////////////////////////////////////////
//Fixup requested object

void CutSceneManager::FixupRequestedObjects(const Vector3 &vPos, float fRadius, s32 iModelIndex)
{
	if (iModelIndex != -1)
	{
		CEntity* pFoundEntity = GetEntityToAtPosition(vPos, fRadius, iModelIndex);

		if(pFoundEntity && pFoundEntity->GetType() == ENTITY_TYPE_OBJECT)
		{
			CObject* pObject = static_cast<CObject*>(pFoundEntity);

			if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_TEMP || pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
			{
				CObjectPopulation::DestroyObject(pObject);
			}
			else
			{
				CObjectPopulation::ConvertToDummyObject(pObject);
			}
		}
		else
		{
			Assertf(0, "CUTSCENE: couldn't find objectwithin radius to fix up!");
		}
	}
}

//////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
//Render the seamless trigger area

#if __BANK
fwEntity*	CutSceneManager::GetIsolatedPedEntity() const
{
	int index = m_iSelectedIsolatePedIndex - 1;  // Subtract 1, because SetFace goes from [0, GetNumFaces())
	if(index != -1)
	{
		s32 objectId = m_pedModelObjectList[index]->GetObjectId();

		cutsEntity *pEntity = GetEntityByObjectId(objectId);

		if(pEntity && pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
		{
			CCutsceneAnimatedActorEntity *pActor = static_cast< CCutsceneAnimatedActorEntity * >(pEntity);
			CPed *pPed = pActor->GetGameEntity();

			return pPed;
		}
	}

	return NULL;
}

void CutSceneManager::RenderCutSceneTriggerArea()
{
	//Draw a sphere round the trigger radius
	grcDebugDraw::Sphere(sSeamlessTrigger.TriggerMat.d, sSeamlessTrigger.fTriggerRadius, Color32(255, 0, 0,255), false);

	Vector3 vVector (0.0, 1.0f, 0.0f);

	sSeamlessTrigger.TriggerMat.Transform3x3(vVector);

	//draw a line to represent the orientation of the trigger zone
	grcDebugDraw::Line(sSeamlessTrigger.TriggerMat.d, sSeamlessTrigger.TriggerMat.d + (vVector * sSeamlessTrigger.fTriggerRadius) , Color32(255, 0, 0,255));

	Vector3 vVectorMin (0.0f, 1.0f, 0.0f);
	Vector3 vVectorMax (0.0f, 1.0f, 0.0f);

	vVectorMin.RotateZ(sSeamlessTrigger.fTriggerAngle * -0.5f);
	vVectorMax.RotateZ(sSeamlessTrigger.fTriggerAngle * 0.5f);

	sSeamlessTrigger.TriggerMat.Transform3x3(vVectorMin);
	sSeamlessTrigger.TriggerMat.Transform3x3(vVectorMax);

	//Draw a the min and max angles of the trigger zone
	grcDebugDraw::Line(sSeamlessTrigger.TriggerMat.d, sSeamlessTrigger.TriggerMat.d +(vVectorMax* sSeamlessTrigger.fTriggerRadius), Color32(0, 0, 255,255));
	grcDebugDraw::Line(sSeamlessTrigger.TriggerMat.d, sSeamlessTrigger.TriggerMat.d +(vVectorMin* sSeamlessTrigger.fTriggerRadius), Color32(0, 0, 255,255));


	grcDebugDraw::Axis(sSeamlessTrigger.TriggerMat, 1.0, true);
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
bool  CutSceneManager::m_bShouldPausePlaybackForAudioLoad = false;

#if __BANK
bool CutSceneManager::m_bIsFinalApproved = false;
bool CutSceneManager::m_bIsCutsceneAnimationApproved = false;
bool CutSceneManager::m_bIsCutsceneCameraApproved = false;
bool CutSceneManager::m_bIsCutsceneFacialApproved = false;
bool CutSceneManager::m_bIsCutsceneLightingApproved = false;
bool CutSceneManager::m_bIsCutsceneDOFApproved = false;
float CutSceneManager::m_fApprovalMessagesOnScreenTimer = 0.0f;
bool CutSceneManager::m_IsDlcScene = false; 
#endif
CutSceneManager::CutSceneManager()
{
	Init();
	cutsManager::InitClass();
	crClip::InitClass();
	m_iScriptRefCount = 0;
	m_fValidFrameStep = false;
	m_bIsPlayerInScene = false;
	m_bIsSeamless = false;
	m_bRequestedScriptAssetsForEndOfScene = false;
	m_bAreScriptReservedEntitiesLoaded = false;
	m_bIsSeamlessSkipping = false;
	m_SeamlessSkipState = SS_DEFAULT;
	//m_fLastFrameTime = 0.0f;
	m_iNextLoadEvent = 0;
	m_iNextUnloadEvent = 0;
	m_iNextAudioLoadEvent = 0;
	m_bApplyTargetSkipTime = false;
	m_bFadeOnSeamlessSkip = true;
	m_fTargetSkipTime = 0.0f;
	m_fFinalAudioPlayEventTime = 0.0f;
	m_SkipFrame = 0;
	m_ScriptThread = THREAD_INVALID;
	m_uFrameCountOfFirstRegisterCall = 0;
	m_VehicleModelThePlayerExitsTheSceneIn = 0; 
	m_iPlayerObjectId = -1;
	m_bCutsceneWasSkipped = false;
	m_bSkipCallBackSetup = false;
	m_ValidConcatSectionFlags = INVALID_PLAYBACK_FLAG;
	m_PlaybackFlags = 0;
	m_bCanVibratePad = false;
	m_bAllowCargenToUpdate = false;
	m_bCanUseMobilePhone = false;
	m_HasScriptOverridenFadeValues = false;
	m_CanSkipCutSceneInMultiplayerGame = false;
	m_bCanSkipScene = true;
	m_fSkipBlockedCameraAnimTagTime = -1.0f;
	m_bHasScriptRegisteredAnEntity = false;
	m_bShouldRestoreCutsceneAtEnd = false;
	m_bShouldStopNow = false;
	m_bCanStartCutscene = false;
	m_bDelayTerminatingAFrame = false;
	m_bWasDelayedTerminating = false;
	m_bDeleteAllRegisteredEntites = false;
	m_bCanScriptSetupEntitiesPreUpdateLoading = false;
	m_bHasScriptSetupEntitiesPreUpdateLoadingFlagBeenSet = false;
	m_bCanScriptChangeEntitiesModel = false;
	m_bHasScriptChangeEntitiesModelFlagBeenSet = false;
	m_bFailedToLoadBeforePlayWasRequested = false;
	m_bWasSingledStepped = false;
	m_IsResumingPlayBackFromSkipping = false;
	m_bHasCutThisFrame = false;
	m_bDisplayMiniMapThisUpdate = false;
	m_bAllowGameToPauseForStreaming = true;

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
	LoadAuthorizedCutsceneList();
#endif
#if __BANK
	m_bStreamObjectsWhileSceneIsPlaying = true; //Is there a reason for this to not be true in BANK? CW
	m_pCutSceneBank = NULL;
	m_bRenderCutSceneBorders = false;
	//m_Frame = 0;
	m_pSkipToFrameSlider = NULL;
	m_iSkipToFrame = 0;
	m_pCaptureRadiusSlider = NULL;
	m_pObjsCombo = NULL;
	m_iCaptureRadius = 10;
	formatf(m_capturePath, "%s", "x:/gta5/art/animation/resources/sets/obj/entername.obj");
	m_captureMap = true;
	m_captureProps = true;
	m_captureVehicles = true;
	m_capturePeds = true;
	m_useSceneOrigin = false;
	m_showCaptureSphere = false;
	m_capturePedCapsules = false;
	//m_bWasPausedForScrubbing = false;
	m_bScrubbingFromLooping = false;
	m_bStartedFromWidget = false;
	m_uRenderMBFrame[0] = static_cast< u32 >(-1);
	m_uRenderMBFrame[1] = static_cast< u32 >(-1);
	m_bRenderMBFrameAndSceneName = false;
	m_bVerboseExitStateLogging = false;
	m_LogUsingCallStacks = PARAM_cutscenecallstacklogging.Get(); 
	m_bVerboseExitStateLogging = PARAM_cutsceneverboseexitstatelogging.Get();
	m_RunSoakTest = false;
	m_SoakTestPlaybackRateIdx = 0;
	m_selectedObj = -1;
	m_bUseExternalTimeStep = false; 
	m_ExternalTimeStep = 0.0f; 
	m_iExternalFrame = 0;
	m_searchText[0] = '\0';
	m_sceneNameCombo = NULL; 
	m_lightSaveStatus[0] = '\0';
	m_pSaveLightStatusText = NULL; 
	XPARAM(EnableCutsceneTuning); 
	XPARAM(EnableCutSceneGameDataTuning); 
	m_fDuration = 0.0f;
	m_fPlayTime = 0.0f;

	if(PARAM_EnableCutsceneTuning.Get())
	{
		CutfileNonParseData::m_FileTuningFlags.SetFlag(CutfileNonParseData::CUTSCENE_LIGHT_TUNING_FILE); 
	}

	if(PARAM_EnableCutSceneGameDataTuning.Get())
	{
		CutfileNonParseData::m_FileTuningFlags.SetFlag(CutfileNonParseData::CUTSCENE_GAMEDATA_TUNING_FILE); 
	}

#else
	m_bStreamObjectsWhileSceneIsPlaying = true;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CutSceneManager::~CutSceneManager()
{
	cutsManager::ShutdownClass();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Initialise cut scene vars for the constructor, only called once when the cut scene manager is initalised

void CutSceneManager::Init()
{
	m_fDeltaTime = 0.0f;
	m_fPreviousTime = 0.0f;
	m_fPlayTime = 0.0f;
	m_fDuration = 0.0f;
	m_fFinalAudioPlayEventTime = 0.0f;
	m_pAssetManger = NULL;
	m_pAnimManager = NULL;
	m_bIsCutSceneActive = false;
	m_CutSceneHashString.Clear();
	m_bStreamedCutScene = false;
	m_IsCutscenePlayingBack = false;
	m_bCameraWillCameraBlendBackToGame = false;
	m_ShouldEnablePlayerControlPostScene = false;
	m_bPostSceneUpdateEventCalled = false;
	m_pInteriorProxy = NULL;
	sSeamlessTrigger.bScriptOveride = false;
	sSeamlessTrigger.fPlayerAngle = 0.0f;
	sSeamlessTrigger.fTriggerAngle = 0.0f;
	sSeamlessTrigger.fTriggerOrient = 0.0f;
	sSeamlessTrigger.vTriggerOffset = Vector3(0.0f, 0.0f, 0.0f);
	m_EndFadeTime=0;
	m_bAllowGameToPauseForStreaming = true;
	m_HasSetCutsceneToGameStatePostScene = false; 
	m_bUpdateCutsceneTimer = false; 
	m_bPreSceneUpdateEventCalled = false; 
	m_bPresceneLightUpdateEvent = false; 
	m_vPlayerPositionBeforeScene = VEC3_ZERO; 
	m_RenderBufferIndex = 0; 
	m_bEnableReplayRecord = true;
	m_bReplayRecording = false;
	m_ShutDownMode = 0; 
	m_bHasProccessedEarlyCutEventForSinglePlayerExits = false; 
	m_bShouldProcessEarlyCameraCutEventsForSinglePlayerExits = false; 
	
#if __BANK
	m_ActivateMapObjectEditing = false;
	m_FixupMapObject = false;
	m_bActivateBlockingObjectEditing = false;
	m_bRenderBlockingBoundObjects = false;
	m_fBoundingBoxWidth = 1.0f;
	m_vBlockingBoundPos = VEC3_ZERO;
	m_vBlockingBoundRot = VEC3_ZERO;
	m_fBoundingBoxHeight = 10.0f;
	m_fBoundingBoxLength = 1.0f;
	m_fShadowBlur = 0.0f; 
	m_CanSaveLightAuthoringFile = false; 
	m_pDataLightXml = NULL; 
	m_AllowScrubbingToZero = true; 
	m_bSnapCameraToLights = false; 
	DebugEditing::CutsceneEditing::InitCommands();

	PushAssetFolder("assets:/cuts");
#endif

#if GTA_REPLAY
	//In replay we dont have a cutscene camera so store these values here;
	m_ReplayCharacterLightParams.Reset();
#endif

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::ShutDown()
{
	ShutDownCutscene();

#if __BANK
	DeleteAllAssetFolders();
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise static pointer definition

CutSceneManager* CutSceneManager::sm_pInstance = NULL;

void CutSceneManager::CreateInstance()
{
	if (Verifyf(sm_pInstance == NULL, "A cutscene manager aready exists, only one can exist"))
	{
		sm_pInstance = rage_new CutSceneManager();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Get a pointer to the instance of the cut scene manager

CutSceneManager* CutSceneManager::GetInstance()
{
	Assertf(sm_pInstance, "Cutscene manager pointer has been nulled");
	return sm_pInstance;

}

///////////////////////////////////////////////////////////////////////////////////////////////////
//	Delete instance of cut scene manager

void CutSceneManager::DeleteInstance()
{
	Assertf(sm_pInstance, "Cutscene manager is already null");

	if(sm_pInstance)
	{
		delete sm_pInstance;
	}
	sm_pInstance = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::ShutDownCutscene()
{
	//FILL IN STATS
	SetCutSceneToGameState(true);

		//CHECK ON FUNCTIONALITY
	//ShutdownAudio(true);
	//Don't allow this to be called if scene is not active at all.
	if (m_bIsCutSceneActive)
	{
		if(GetCutfFile())
		{
			if(IsCutscenePlayingBack())
			{
				// Immediately stop any audio, do it now as the default stop will fade out the audio
				g_CutsceneAudioEntity.StopCutscene(true, 0);

				Stop(SKIP_FADE, SKIP_FADE);
				DoStoppedState();
			}
			else
			{
				TerminateLoadedOnlyScene();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::AddScriptResource(scrThreadId ScriptId)
{
	if(!GetPlayBackFlags().IsFlagSet(CUTSCENE_REQUESTED_FROM_WIDGET) )
	{
		scrThread *pScriptThread = scrThread::GetThread(ScriptId);
		if(Verifyf(pScriptThread, "Could not get the script thread!"))
		{
			CGameScriptId gameScriptId(*pScriptThread);
			scriptHandler *pScriptHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(gameScriptId);
			if(Verifyf(pScriptHandler, "Could not get the script handler!"))
			{
				CScriptResource_CutScene cutScene(m_CutSceneHashString);
				bool bResourceAdded = false; pScriptHandler->RegisterScriptResource(cutScene, &bResourceAdded);
				if(bResourceAdded)
				{
					m_iScriptRefCount ++;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::ReleaseScriptResource(u32 cutSceneNameHash)
{
	if(Verifyf(cutSceneNameHash == m_CutSceneHashString, "Cutscene is not the one being released by the script resource!"))
	{
		if(Verifyf(m_iScriptRefCount > 0, "Cutscene has no script resources to release!"))
		{
			m_iScriptRefCount --;
			if(m_iScriptRefCount == 0)
			{
				SetDeleteAllRegisteredEntites(true);

				ShutDownCutscene();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::Load( const char* UNUSED_PARAM(pFilename), const char*  UNUSED_PARAM(pExtension), bool bPlayNow, EScreenFadeOverride fadeOutGameAtBeginning, EScreenFadeOverride fadeInCutsceneAtBeginning, EScreenFadeOverride fadeOutCutsceneAtEnd, EScreenFadeOverride fadeInGameAtEnd, bool bJustCutfile )
{
	cutsceneManagerDebugf3 ("Cutscene manager: Load");

	m_bPlayNow = bPlayNow;

	m_fadeOutGameAtBeginning = fadeOutGameAtBeginning;
	m_fadeInCutsceneAtBeginning = fadeInCutsceneAtBeginning;
	m_fadeOutCutsceneAtEnd = fadeOutCutsceneAtEnd;
	m_fadeInGameAtEnd = fadeInGameAtEnd;
	m_fadeInGameAtEndWhenStopping = fadeInGameAtEnd;
	m_bJustLoadCutfile = bJustCutfile;
	m_HasScriptOverridenFadeValues = false; // make sure script can't override the fade values before they are set.

	//Updates pre scene update to load cutfile
	m_cutsceneState = CUTSCENE_LOAD_CUTFILE_STATE;
}

/////////////////////////////////////////////////////////////////////////////////////
// Make a request to the streaming system

bool CutSceneManager::LoadCutFile(const char* UNUSED_PARAM (pFileName))
{
	//Make a request to the streaming system with a hash of the cut scene name
	strLocalIndex iIndex = g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash());

	if (Verifyf((iIndex != -1), "Cut scene doesn't exist, and will not be loaded"))
	{
		g_CutSceneStore.StreamingRequest(iIndex, STRFLAG_PRIORITY_LOAD|STRFLAG_CUTSCENE_REQUIRED);
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CutSceneManager::FadeOut( const Color32 &color, float fDuration )
{
	m_EndFadeTime = fwTimer::GetTimeInMilliseconds()+(u32)(fDuration*1000.0f);
	return cutsManager::FadeOut(color, fDuration);
}

//////////////////////////////////////////////////////////////////////////
bool CutSceneManager::IsFadeTimerComplete()
{
	return m_EndFadeTime>0 && fwTimer::GetTimeInMilliseconds()>m_EndFadeTime;
}

//////////////////////////////////////////////////////////////////////////
void CutSceneManager::SetEnableReplayRecord(bool bEnable)
{
	if(!Verifyf(!(bEnable && GetPlayBackFlags().IsFlagSet(CUTSCENE_REQUESTED_FROM_WIDGET)), "Can't enable cutscene replay record for cutscenes played through RAG!"))
	{
		return;
	}

	if(!Verifyf(!IsCutscenePlayingBack(), "Can't enable/disable cutscene replay record once the cutscene has started playing!"))
	{
		return;
	}

	m_bEnableReplayRecord = bEnable;
}

//////////////////////////////////////////////////////////////////////////
bool CutSceneManager::GetEnableReplayRecord() const
{
	return m_bEnableReplayRecord;
}

#if GTA_REPLAY
bool CutSceneManager::ReplayLoadCutFile()
{
	if(CutSceneManager::GetInstance()->GetCutfFile() == NULL)
	{
		//make sure that the cut file has been created
		cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())));
		replayAssertf(pDef, "No object at this slot");

		if (replayVerifyf(pDef->m_pObject, "%s:Object is not in memory", pDef->m_name.GetCStr()))
		{
			//Pass the pointer of the cutfile2 object to the cut scene manager so it can be referred to later.
			CutSceneManager::GetInstance()->SetCutfFile(pDef->m_pObject);
			g_CutSceneStore.AddRef(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())), REF_OTHER);
			return true;
		}
		return false;
	}
	return true;
}

void CutSceneManager::ReplayCleanup()
{
	if(m_CutSceneHashString.IsNotNull() && GetCutfFile() && g_CutSceneStore.HasObjectLoaded(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())))
	{
		if (Verifyf(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash() != -1).Get(), "Cut scene doesn't exist, and will not be loaded"))
		{
			g_CutSceneStore.RemoveRef(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())), REF_OTHER);
			g_CutSceneStore.ClearRequiredFlag(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash()).Get(),STRFLAG_CUTSCENE_REQUIRED);
			g_CutSceneStore.StreamingRemove(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash()));
			m_CutSceneHashString.Clear();
			SetCutfFile(NULL);
		}
	}

	g_CutsceneAudioEntity.StopCutscene(true, 0);

	g_CutsceneAudioEntity.StopSyncScene();

	ShutDownCutscene();	
}

#endif // GTA_REPLAY

/////////////////////////////////////////////////////////////////////////////////////
// Virtual override of the cut scene system

void CutSceneManager::DoLoadingCutfileState()
{
	bool bIsLoaded = false;

	//wait for our streaming system to stream in the files
	if(g_CutSceneStore.HasObjectLoaded(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())) )
	{
		//wait for audio to not be prepared
		if(!g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
		{
		if(GetCutfFile() == NULL)
		{
			//make sure that the cut file has been created
				cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())));
			Assertf(pDef, "No object at this slot");

			if (Verifyf(pDef->m_pObject, "%s:Object is not in memory", pDef->m_name.GetCStr()))
			{
				//Pass the pointer of the cutfile2 object to the cut scene manager so it can be referred to later.
				SetCutfFile(pDef->m_pObject);
					g_CutSceneStore.AddRef(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())), REF_OTHER);
					
					//used to add adaption events here
					
				m_streamingState = STREAMING_IDLE_STATE;	//set this so the cut scene manager doesn't think its loading anything
				bIsLoaded = true;
			}
			else
			{
				//reset the manager
				m_cutsceneState = CUTSCENE_UNLOAD_STATE;
				m_streamingState = STREAMING_IDLE_STATE;
			}
		}
		else
		{
			m_streamingState = STREAMING_IDLE_STATE;	//set this so the cut scene manager doesn't think its loading anything
			bIsLoaded = true;
		}
	}
#if !__NO_OUTPUT
		else
		{
			cutsceneManagerDebugf3("Loading cutfile state - Waiting for existing audio to finish");
		}
#endif
	}
	//Call the base function to fill out member variables now that the cut scene has loaded.
	if (bIsLoaded)
	{
#if __BANK
		GetModelObjectsOfType(m_DebugManager.m_VehicleModelObjectList, CUTSCENE_VEHICLE_MODEL_TYPE);
		m_DebugManager.PopulateSelectorList(m_DebugManager.m_pVehicleSelectCombo, "Vehicle Selector", m_DebugManager.m_iVehicleModelIndex, m_DebugManager.m_VehicleModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedVehicleEntityCallBack), (datBase*)&m_DebugManager) );

		GetCutfFile()->FindObjectsOfType( CUTSCENE_OVERLAY_OBJECT_TYPE, m_DebugManager.m_OverlayObjectList );

		GetCutfFile()->FindObjectsOfType( CUTSCENE_CAMERA_OBJECT_TYPE, m_DebugManager.m_CameraObjectList);

		GetCutfFile()->FindObjectsOfType( CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE, m_DebugManager.m_PtfxList);

		m_DebugManager.PopulateSelectorList(m_DebugManager.m_pPTFXSelectCombo, "Prop Selector", m_DebugManager.m_iPtfxIndex, m_DebugManager.m_PtfxList, datCallback(MFA(CCutSceneDebugManager::GetSelectedPtfxEntityCallBack), (datBase*)&m_DebugManager));

		m_DebugManager.PopulateSelectorList(m_DebugManager.m_pOverlaySelectCombo, "Overlay Selector", m_DebugManager.m_iOverlayIndex, m_DebugManager.m_OverlayObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedOverlayEntityCallBack), (datBase*)&m_DebugManager) );

		GetModelObjectsOfType(m_DebugManager.m_PropList, CUTSCENE_PROP_MODEL_TYPE);
		m_DebugManager.PopulateSelectorList(m_DebugManager.m_pPropSelectCombo, "Vehicle Selector", m_DebugManager.m_iPropModelIndex, m_DebugManager.m_PropList,datCallback(MFA(CCutSceneDebugManager::GetSelectedPropEntityCallBack), (datBase*)&m_DebugManager) );

		GetCutfFile()->FindObjectsOfType( CUTSCENE_ASSET_MANAGER_OBJECT_TYPE, m_DebugManager.m_pAssetManagerList );

		if(m_DebugManager.m_CameraObjectList.GetCount() > 0)
		{
			m_DebugManager.PopulateCameraCutEventList(m_DebugManager.m_CameraObjectList[0], m_DebugManager.m_pTimeCycleCameraCutCombo, m_DebugManager.m_iTimecycleEventsIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateTimeCycleParamsCB), (datBase*)&m_DebugManager));
			m_DebugManager.PopulateCameraCutEventList(m_DebugManager.m_CameraObjectList[0], m_DebugManager.m_pCoCModifiersCameraCutCombo, m_DebugManager.m_iCoCModifiersCamerCutComboIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateCoCModifierEventOverridesCB), (datBase*)&m_DebugManager) ); 
			m_DebugManager.UpdateFirstPersonEventList(); 
			m_DebugManager.m_CascadeBounds.PopulateSetShadowBoundsEventList(m_DebugManager.m_CameraObjectList[0]); 
		}

#endif
		cutsManager::DoLoadingCutfileState();

		if(GetSectionStartTimeList().GetCount() == 0)
		 {
			cutsceneAssertf(0, "Scene %s will not play because there is no section time list which is required for anim streaming", GetCutsceneName());
			m_cutsceneState = CUTSCENE_UNLOAD_STATE;
			m_streamingState = STREAMING_IDLE_STATE;
			return;
		 }


		if(GetCutfFile()->GetCutsceneFlags().IsSet(cutfCutsceneFile2::CUTSCENE_FADE_IN_GAME_FLAG))
		{
			m_fadeInGameAtEnd = DEFAULT_FADE;
		}
		if(GetCutfFile()->GetCutsceneFlags().IsSet(cutfCutsceneFile2::CUTSCENE_FADE_OUT_GAME_FLAG))
		{
			m_fadeOutGameAtBeginning = DEFAULT_FADE;
		}
		if(GetCutfFile()->GetCutsceneFlags().IsSet(cutfCutsceneFile2::CUTSCENE_FADE_IN_FLAG))
		{
			m_fadeInCutsceneAtBeginning = DEFAULT_FADE;
		}
		if(GetCutfFile()->GetCutsceneFlags().IsSet(cutfCutsceneFile2::CUTSCENE_FADE_OUT_FLAG))
		{
			m_fadeOutCutsceneAtEnd = DEFAULT_FADE;
		}



		//create the concat playback list now we have the list of concatenated scenes
		//if the script passes an invalid list ie with no valid sections stop the cutscene running.
		if(!CreateConcatSectionPlayBackList(m_ValidConcatSectionFlags))
		{
			m_cutsceneState = CUTSCENE_UNLOAD_STATE;
			m_streamingState = STREAMING_IDLE_STATE;
			return;
		}

		//check that and sections have been updated need to modify the start and end times
		SetStartAndEndTimeBasedOnPlayBackList(m_ValidConcatSectionFlags);

#if __BANK
		m_IsDlcScene = IsDLCCutscene(); 
#endif	


#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
		UpdateAuthorization();
#endif

#if __BANK
		if (m_bStartedFromWidget)
		{
			CPed* pPed = FindPlayerPed();
			pPed->SetPosition(m_vSceneOffset);
		}
#endif
		LoadCutSceneSectionMapCollision(!IsCutSceneSeamless());

		cutsceneManagerDebugf3("Loading cutfile state - Initing cutscene audio track");
		g_CutsceneAudioEntity.InitCutsceneTrack();

		if(!m_bHasScriptChangeEntitiesModelFlagBeenSet && !m_bCanScriptChangeEntitiesModel)
		{
			cutsceneManagerDebugf3("Loading cutfile state - Script can change cutfile models (m_bCanScriptSetupEntitiesModel = true)");
			m_bCanScriptChangeEntitiesModel = true;
			m_bHasScriptChangeEntitiesModelFlagBeenSet = true;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::RequestScriptReservedAssetsForSkip()
{
		//DispatchEventToAllEntities(CUTSCENE_PAUSE_EVENT);
	if(!m_bRequestedScriptAssetsForEndOfScene)
	{
		atArray< CCutsceneAnimatedActorEntity * > actors;

		//get a list of object ids reserved for end
		atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
		for ( entry.Start(); !entry.AtEnd(); entry.Next() )
		{
			SEntityObject *pEntityObject = m_cutsceneEntityObjects.Access(entry.GetKey());

			if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
				|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY || pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
			{
				CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

				if(pModel && pModel->m_RegisteredEntityFromScript.SceneNameHash > 0)
				{
					if(pModel->m_RegisteredEntityFromScript.bCreatedForScript || (!pModel->m_RegisteredEntityFromScript.bDeleteBeforeEnd && pModel->m_RegisteredEntityFromScript.bAppearInScene) )
					{
						m_ObjectIdsReserevedForPostScene.PushAndGrow(pModel->m_RegisteredEntityFromScript.iSceneId);

						if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
						{
							CCutsceneAnimatedActorEntity* pActor = static_cast<CCutsceneAnimatedActorEntity*>(pModel);

							actors.PushAndGrow(pActor);
						}
					}
				}
			}
		}


		//create load events for these object cause we need them now
		if(m_ObjectIdsReserevedForPostScene.GetCount() > 0 )
		{
			m_streamingState = STREAMING_ASSETS_STATE;

			//cutfObjectIdListEvent* pLoadEvent = rage_new cutfObjectIdListEvent( m_ObjectIdsReserevedForPostScene, 0.0,  );
			//need to preserve this list so lets create a local copy because the constructor will destroy this list
			atArray<s32> tempList;

			for(int i =0; i < m_ObjectIdsReserevedForPostScene.GetCount(); i++)
			{
				tempList.PushAndGrow(m_ObjectIdsReserevedForPostScene[i]);
			}

			cutfObjectIdListEventArgs LoadList(tempList);

			cutfObjectIdEvent LoadEvent(GetAssetManager()->GetObject()->GetObjectId(), 0.0f, CUTSCENE_LOAD_MODELS_EVENT, &LoadList );

			SetIsLoading(GetAssetManager()->GetObject()->GetObjectId(), true);

			//GetAssetManager()->SetShouldCreateEntities(true);

			DispatchEvent(&LoadEvent);

			// This needs to be done after dispatching the load events or the cutscene will stall

			m_bRequestedScriptAssetsForEndOfScene = true;
		}
	}
}

void CutSceneManager::DoLoadingBeforeResumingState()
{
	if ( !IsLoading() && AreReservedEntitiesReady())
	{
		//skipped to the end
		m_streamingState = STREAMING_IDLE_STATE;
		m_bAssetsStreamed = true;

		// we need to be in this state in order for Play() to resume the scene
		m_cutsceneState = CUTSCENE_PAUSED_STATE;

#if __BANK
		if(m_bScrubbingFromLooping)
		{
			if(m_PlayBackState == PLAY_STATE_BACKWARDS)
			{
				BankPlayBackwardsCallback();
			}
			else
			{
				//make sure we requeue this
				m_PlayBackState = PLAY_STATE_PAUSED;
				BankPlayForwardsCallback();
			}
			m_bScrubbingFromLooping = false;
		}

		if(GetScrubbingState() == SCRUBBING_NONE)
		{
			//want to call
			Play();
			//DispatchEventToObjectsOfType( CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT );
		}
		else
		{
			m_cutsceneState = CUTSCENE_PLAY_STATE;

			if (GetScrubbingState() == SCRUBBING_STEPPING_FORWARDS || GetScrubbingState() == SCRUBBING_STEPPING_BACKWARDS ||
				GetScrubbingState() == SCRUBBING_FORWARDS_FROM_TIME_LINE_BAR || GetScrubbingState() ==  SCRUBBING_BACKWARDS_FROM_TIME_LINE_BAR)
			{
				DispatchEventToAllEntities(CUTSCENE_PAUSE_EVENT);
			}
			//DispatchEventToObjectsOfType( CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT );
		}
#else
		//DispatchEventToObjectsOfType( CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT );
		Play();

#endif

	}
	else
	{
		DispatchEventToAllEntities(CUTSCENE_PAUSE_EVENT);
		DispatchEventToAllEntities(CUTSCENE_UPDATE_LOADING_EVENT );
	}
}

void CutSceneManager::DictionaryLoadedCB(crClipDictionary* pDictionary, s32 section)
{
	cutsDictionaryLoadedEventArgs args(pDictionary, section);
	DispatchEventToAllEntities(CUTSCENE_DICTIONARY_LOADED_EVENT, &args );
}

void CutSceneManager::DoAuthorizedState()
{
#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK	
	if(!m_RenderUnauthorizedScreen)
	{
		m_RenderUnauthorizedScreenTimer = 0; 
		m_RenderUnauthorizedScreen = true; 
	}
	else
	{
		m_RenderUnauthorizedScreenTimer += fwTimer::GetSystemTimeStepInMilliseconds(); 

		if(m_RenderUnauthorizedScreenTimer > 3000)
		{
			ShutDownCutscene(); 
		}
	}
#endif
}

void CutSceneManager::ValidateAudioLoadAndPlayEvents()
{
	u32 LoadAudioEvents = 0;
	u32 PlayAudioEvents = 0;
	u32 StopAudioEvents = 0;

	const atArray<cutfEvent*>& pLoadEventList = GetCutfFile()->GetLoadEventList();

	for(int i =0; i < pLoadEventList.GetCount(); i++)
	{
		if(pLoadEventList[i]->GetEventId() == CUTSCENE_LOAD_AUDIO_EVENT)
		{

			LoadAudioEvents++;
		}
	}

	const atArray<cutfEvent*>& pEventList = GetCutfFile()->GetEventList();

	for(int i =0; i < pEventList.GetCount(); i++)
	{
		if(pEventList[i]->GetEventId() == CUTSCENE_PLAY_AUDIO_EVENT)
		{
			PlayAudioEvents++;

			s32 section = GetConcatSectionForTime(pEventList[i]->GetTime() + 0.1f);

			if(section != -1 && IsConcatSectionValidForPlayBack(section))
			{
				m_fFinalAudioPlayEventTime = pEventList[i]->GetTime();
			}
		}

		if(pEventList[i]->GetEventId() == CUTSCENE_STOP_AUDIO_EVENT)
		{
			StopAudioEvents++;
		}
	}


	#if __ASSERT
		cutsceneAssertf(LoadAudioEvents == PlayAudioEvents, "Cutscene %s requires to be re-exported as the number of Load Audio (%d) and Play Audio(%d) events dont match, may result in audio playback issues", GetCutsceneName(), LoadAudioEvents, PlayAudioEvents);
		cutsceneAssertf(StopAudioEvents == PlayAudioEvents, "Cutscene %s requires to be re-exported as the number of Play Audio (%d) and Stop Audio(%d) events dont match, may result in audio playback issues", GetCutsceneName(), PlayAudioEvents, StopAudioEvents);
	#endif
}


void CutSceneManager::DoLoadState()
{
	if(!cutsceneVerifyf(GetCutfFile(),"DoLoadState: No cutfile object loaded"))
	{
		m_cutsceneState = CUTSCENE_UNLOAD_STATE;
		m_streamingState = STREAMING_IDLE_STATE;
		return;
	}

	m_bCanScriptChangeEntitiesModel = false;

	if(IsConcatted())
	{
		m_bStreamObjectsWhileSceneIsPlaying = true;
	}

	ValidateAudioLoadAndPlayEvents();

	//the scene has been skipped, need to deal with the streaming of models that have been reserved for the end but may not be loaded.

		m_streamingState = STREAMING_ASSETS_STATE;

		GetCutfFile()->SortLoadEvents();

		const atArray<cutfEvent*>& pLoadEventList = GetCutfFile()->GetLoadEventList();

		// create the entities we need for loading purposes
		atArray<s32> iObjectIdsToReserve;
		GetObjectIdsInEventList( pLoadEventList, iObjectIdsToReserve );

		const atArray<cutfObject*>& pObjectList = GetCutfFile()->GetObjectList();

		//Create AnimatedModelEntityManagers because the streaming of cut scene peds has changed.
		//A ped pointer is required to know which variation is required, this is a change to only having
		//the peds model in memory.
		for(int j=0; j < pObjectList.GetCount(); ++j)
		{
			if(pObjectList[j]->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
			{
				const cutfModelObject* pModelObject = static_cast<const cutfModelObject*>(pObjectList[j]);

				if(pModelObject->GetModelType() == CUTSCENE_PED_MODEL_TYPE)
				{
					iObjectIdsToReserve.PushAndGrow(pObjectList[j]->GetObjectId());
				}
			}
		}

		for ( int i = 0; i < iObjectIdsToReserve.GetCount(); ++i )
		{
			GetEntityObject( pObjectList[iObjectIdsToReserve[i]] );
		}

		iObjectIdsToReserve.Reset();

		//decide if need to stream objects
		if(m_bStreamObjectsWhileSceneIsPlaying BANK_ONLY(&& !m_bLoop))
		{
			if(IsConcatted() && !IsConcatSectionValidForPlayBack(0))
			{
				fwFlags32 StreamingFlags;
				StreamingFlags.SetFlag(CS_LOAD_MODELS_EVENT | CS_LOAD_ANIM_DICT_EVENT | CS_LOAD_PARTICLE_EFFECTS_EVENT
					| CS_LOAD_SCENE_EVENT | CS_LOAD_OVERLAYS_EVENT | CS_LOAD_RAYFIRE_EVENT | CS_LOAD_INTERIORS_EVENT | CS_LOAD_SUBTITLES_EVENT);

				SyncLoadIndicesToStartTime(m_iNextLoadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();

				StreamingFlags.SetFlag(CS_LOAD_AUDIO_EVENT);
				SyncLoadIndicesToStartTime(m_iNextAudioLoadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();

				StreamingFlags.SetFlag(CS_UNLOAD_MODELS_EVENT | CS_UNLOAD_ANIM_DICT_EVENT | CS_UNLOAD_PARTICLE_EFFECTS_EVENT | CS_UNLOAD_SCENE_EVENT
					| CS_UNLOAD_AUDIO_EVENT| CS_UNLOAD_OVERLAYS_EVENT | CS_UNLOAD_RAYFIRE_EVENT | CS_UNLOAD_SUBTITLES_EVENT);

				SyncLoadIndicesToStartTime(m_iNextUnloadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();

				//this is because we know this is unique event and always needs to be dispatched at the beginning
				for ( int i = 0; i < pLoadEventList.GetCount(); ++i )
				{
					if(pLoadEventList[i]->GetEventId() == CUTSCENE_LOAD_ANIM_DICT_EVENT)
					{
						DispatchEvent( pLoadEventList[i]);
						break;
					}
				}

				//this is because we know this is unique event and always needs to be dispatched at the beginning
				for ( int i = 0; i < pLoadEventList.GetCount(); ++i )
				{
					if(pLoadEventList[i]->GetEventId() == CUTSCENE_LOAD_SUBTITLES_EVENT)
					{
						DispatchEvent( pLoadEventList[i]);
						break;
					}
				}


				//IMPORTANT:Dispatch the unload events first, because they will do nothing otherwise they will just unload something we just loaded
				StreamingFlags.SetFlag(CS_UNLOAD_MODELS_EVENT | CS_UNLOAD_ANIM_DICT_EVENT | CS_UNLOAD_PARTICLE_EFFECTS_EVENT | CS_UNLOAD_SCENE_EVENT
					| CS_UNLOAD_AUDIO_EVENT| CS_UNLOAD_OVERLAYS_EVENT | CS_UNLOAD_RAYFIRE_EVENT | CS_UNLOAD_SUBTITLES_EVENT);

				UpdateStreaming(0.0f, m_iNextUnloadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();

				StreamingFlags.SetFlag(CS_LOAD_MODELS_EVENT | CS_LOAD_ANIM_DICT_EVENT | CS_LOAD_PARTICLE_EFFECTS_EVENT
					| CS_LOAD_SCENE_EVENT | CS_LOAD_OVERLAYS_EVENT | CS_LOAD_RAYFIRE_EVENT | CS_LOAD_INTERIORS_EVENT | CS_LOAD_SUBTITLES_EVENT);

				UpdateStreaming(DEFAULT_STREAMING_OFFSET, m_iNextLoadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();

				StreamingFlags.SetFlag(CS_LOAD_AUDIO_EVENT);
				UpdateStreaming(DEFAULT_AUDIO_STREAMING_OFFSET, m_iNextAudioLoadEvent, StreamingFlags, GetStartTime());
				StreamingFlags.ClearAllFlags();
			}
			else
			{
				fwFlags32 StreamingFlags;
				StreamingFlags.SetFlag(CS_LOAD_MODELS_EVENT | CS_LOAD_ANIM_DICT_EVENT | CS_LOAD_PARTICLE_EFFECTS_EVENT
					| CS_LOAD_SCENE_EVENT | CS_LOAD_OVERLAYS_EVENT | CS_LOAD_RAYFIRE_EVENT | CS_LOAD_INTERIORS_EVENT | CS_LOAD_SUBTITLES_EVENT);

				UpdateStreaming(DEFAULT_STREAMING_OFFSET, m_iNextLoadEvent, StreamingFlags, GetTime());

				StreamingFlags.ClearAllFlags();

				StreamingFlags.SetFlag(CS_LOAD_AUDIO_EVENT);
				UpdateStreaming(0.0f, m_iNextAudioLoadEvent, StreamingFlags, GetTime());

				StreamingFlags.ClearAllFlags();
			}
		}
		else
		{
			for ( int i = 0; i < pLoadEventList.GetCount(); ++i )
			{
				//dispatch the load events but check the types because they now contain unload events
				fwFlags32 StreamingFlags;
				StreamingFlags.SetFlag(CS_LOAD_MODELS_EVENT | CS_LOAD_ANIM_DICT_EVENT | CS_LOAD_PARTICLE_EFFECTS_EVENT
					| CS_LOAD_AUDIO_EVENT| CS_LOAD_SCENE_EVENT | CS_LOAD_OVERLAYS_EVENT | CS_LOAD_RAYFIRE_EVENT | CS_LOAD_INTERIORS_EVENT | CS_LOAD_SUBTITLES_EVENT);

				if(ValidateEvent(pLoadEventList[i]->GetEventId(), StreamingFlags))
				{
					DispatchEvent( pLoadEventList[i] );
				}
			}
		}

	// Also load the streaming request list file.
	if (!gStreamingRequestList.IsRunFromScript())
	{
	gStreamingRequestList.SetTime(GetCutSceneCurrentTime()*1000.0f);
	gStreamingRequestList.BeginPrestream(GetCutsceneName(), false);
	}

	// HACK HACK HACK
	// See B* 3705257 for more details. Basically some scenes are missing (or skipping due to concat play back) the 
	// load audio event that would normally inform the cutscene manager that it needs to wait on the audio loading in.
	// The audio gets requested by a call to InitCutsceneTrack in DoLoadingCutfileState, but without a LOAD_AUDIO_EVENT
	// we don't track that it's still being loaded. As a result, we can end up trying to start the audio before it's ready, 
	// if the audio happens to be the last asset to load. 
	// For some reason this has only come up in one specific scene in GTA (more than 2 years after release!)
	// so we're limiting the fix to that one specific scene for now.
	if (GetPlayBackFlags().IsFlagSet(CUTSCENE_PLAYBACK_FORCE_LOAD_AUDIO_EVENT))
	{
		cutsceneManagerDebugf3("Setting audio entity to loading for scene %s ( flagged from script using CUTSCENE_PLAYBACK_FORCE_LOAD_AUDIO_EVENT - see b* 3705257", GetCutsceneName());
		SetIsLoading(FindAudioObjectId(), true);
	}

	Displayf("Do load State Frame:%d, beginning at time %f", fwTimer::GetFrameCount(), GetCutSceneCurrentTime());
	if(!m_bCanScriptSetupEntitiesPreUpdateLoading && !m_bHasScriptSetupEntitiesPreUpdateLoadingFlagBeenSet)
	{
		cutsceneManagerDebugf3("Script can request assets this frame");
		m_bCanScriptSetupEntitiesPreUpdateLoading = true;
		m_bHasScriptSetupEntitiesPreUpdateLoadingFlagBeenSet = true;
	}
	m_cutsceneState = CUTSCENE_LOADING_STATE;
}

/////////////////////////////////////////////////////////////////////////////////////
//
 bool CutSceneManager::ValidateEvent(s32 type, fwFlags32 StreamingFlags)
 {
	 switch(type)
	 {
	 case CUTSCENE_LOAD_SCENE_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_SCENE_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_MODELS_EVENT:
		 {
			return StreamingFlags.IsFlagSet(CS_LOAD_MODELS_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_ANIM_DICT_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_ANIM_DICT_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_PARTICLE_EFFECTS_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_PARTICLE_EFFECTS_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_AUDIO_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_AUDIO_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_RAYFIRE_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_RAYFIRE_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_OVERLAYS_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_OVERLAYS_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_MODELS_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_MODELS_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_ANIM_DICT_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_ANIM_DICT_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_PARTICLE_EFFECTS_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_PARTICLE_EFFECTS_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_SCENE_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_SCENE_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_AUDIO_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_AUDIO_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_RAYFIRE_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_RAYFIRE_EVENT);
		 }
		 break;
	 case CUTSCENE_UNLOAD_OVERLAYS_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_OVERLAYS_EVENT);
		 }
		 break;
	 case CUTSCENE_LOAD_SUBTITLES_EVENT:
		{
			return StreamingFlags.IsFlagSet(CS_LOAD_SUBTITLES_EVENT);
		}
		break;

	 case CUTSCENE_UNLOAD_SUBTITLES_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_UNLOAD_SUBTITLES_EVENT);
		 }
		 break;

	 case CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT:
		 {
			 return StreamingFlags.IsFlagSet(CS_LOAD_INTERIORS_EVENT);
		 }
		 break;

	 default:
		 {
			 return false;
		 }
	 }
 }
bool CutSceneManager::ValidateEventTime(float fEventTime)
{
	s32 section = GetConcatSectionForTime(fEventTime);
	if (IsConcatSectionValidForPlayBack(section))
	{
		return true;
	}
	return false;
}

float CutSceneManager::CalculateStreamingOffset(float Buffer, float currentTime) const
{
	//Set the streaming offset time to be
	float StreamingOffsetBuffer = Min(Buffer, GetCutfFile()->GetTotalDuration() );

	//Only modify the streaming buffer if its shorter than the scene duration.
	//Take into account any invalid sections for playback and ensure that effective steaming buffer is always constant.

	if(GetCutfFile()->GetTotalDuration() > Buffer)
	{
		s32 CurrentPlayBackSection = GetConcatSectionForTime(currentTime);
		s32 StreamingSection =	GetConcatSectionForTime(currentTime + Buffer);

		float InvalidSectionTime = 0.0f; //hold the values of invalid sections

		if(IsConcatSectionValid(CurrentPlayBackSection) && IsConcatSectionValid(StreamingSection)) //both sections are valid
		{
			if(CurrentPlayBackSection !=  StreamingSection)	//the play back section is different from th streaming section
			{
				//start by looking in the next section to the play section, this might not be the same as the streaming section
				//need to look through all the sections as there may be multiple sequential invalid sections.
				for(s32 i= CurrentPlayBackSection + 1; i < m_concatDataList.GetCount(); i++)
				{
					//float startTime = m_concatDataList[i].fStartTime;

					//only interested in sections  that are invalid for playback as these are dead time
					if(!IsConcatSectionValidForPlayBack(i))
					{
						if( (currentTime + Buffer + InvalidSectionTime) > m_concatDataList[i].fStartTime)
						{
							InvalidSectionTime += GetConcatSectionDuration(i); //add the duration of the invalid section
						}
					}
				}
			}
		}

		StreamingOffsetBuffer += InvalidSectionTime;
	}

	return StreamingOffsetBuffer;
}

void CutSceneManager::UpdateStreaming(float Buffer, s32 &LoadIndex, fwFlags32 StreamingFlags, float CurrentTime)
{
	//this will be active in final and release builds
	if (m_bStreamObjectsWhileSceneIsPlaying BANK_ONLY(&& !m_bLoop))
	{
		float StreamingOffsetBuffer = CalculateStreamingOffset(Buffer, CurrentTime);

		//cutsceneManagerDebugf("UpdateStreaming - Current Time (%6.4f) -> Streaming time(%6.4f)", CurrentTime, CurrentTime + StreamingOffsetBuffer);

		//need to dispatch load events as the scene is playing
		const atArray<cutfEvent*>& pLoadEventList = GetCutfFile()->GetLoadEventList();

		//Load events are dispatched with respect to the streaming time, which is the current time + the streaming offset buffer
		while ( (LoadIndex < pLoadEventList.GetCount()) && pLoadEventList[LoadIndex]->GetTime() <= CurrentTime + StreamingOffsetBuffer)
		{
			if(!(pLoadEventList[LoadIndex]->GetTime() < 0.0f))
			{
				//Only call load type events
				if (ValidateEvent(pLoadEventList[LoadIndex]->GetEventId(),StreamingFlags))
				{
					if(IsConcatted())
					{
						cutfEvent* pCutfEvent = const_cast<cutfEvent*>(pLoadEventList[LoadIndex]);
						float fEventTime = pCutfEvent->GetTime();

						if (pCutfEvent->GetEventId() == CUTSCENE_UNLOAD_MODELS_EVENT)
						{
							// Only adjust unload events between the scene start and end time
							float fEpsilon = 0.0001f;			
							if (fEventTime>m_fStartTime && fEventTime<m_fEndTime)
							{
								fEventTime=fEventTime-fEpsilon;
							}
						}	

						if (ValidateEventTime(fEventTime))
						{
							DispatchEvent( pLoadEventList[LoadIndex]);	//refs are added to the model requests.
						}
					}
					else
					{
						DispatchEvent( pLoadEventList[LoadIndex] );
					}
				}
			}

			++LoadIndex;
		}
	}
};

void CutSceneManager::SyncLoadIndicesToStartTime(s32 &LoadIndex, fwFlags32 StreamingFlags, float CurrentTime)
{
	//this will be active in final and release builds
	if (m_bStreamObjectsWhileSceneIsPlaying BANK_ONLY(&& !m_bLoop))
	{
		//need to dispatch load events as the scene is playing
		const atArray<cutfEvent*>& pLoadEventList = GetCutfFile()->GetLoadEventList();

		s32 Index = 0;

		//Load events are dispatched with respect to the streaming time, which is the current time + the streaming offset buffer
		while ( (Index < pLoadEventList.GetCount()) && pLoadEventList[Index]->GetTime() < CurrentTime)
		{
			if (ValidateEvent(pLoadEventList[Index]->GetEventId(),StreamingFlags))
			{
				LoadIndex = Index;
			}

			Index ++;
		}
	}
}


void CutSceneManager::DoPlayState()
{
	if(m_bDelayTerminatingAFrame && !m_bWasDelayedTerminating)
	{
#if __BANK
		if(!m_bStepped)
#endif
		{
			DispatchEventToAllEntities(CUTSCENE_UPDATE_EVENT);

			m_bDelayTerminatingAFrame = false;
			m_bWasDelayedTerminating = true;
			return;
		}
	}

	fwFlags32 StreamingFlags;
	StreamingFlags.SetFlag(CS_LOAD_MODELS_EVENT | CS_LOAD_ANIM_DICT_EVENT | CS_LOAD_PARTICLE_EFFECTS_EVENT
		| CS_LOAD_SCENE_EVENT | CS_LOAD_OVERLAYS_EVENT | CS_LOAD_RAYFIRE_EVENT | CS_LOAD_INTERIORS_EVENT | CS_LOAD_SUBTITLES_EVENT);

	UpdateStreaming(DEFAULT_STREAMING_OFFSET, m_iNextLoadEvent, StreamingFlags, GetTime());
	StreamingFlags.ClearAllFlags();

	StreamingFlags.SetFlag(CS_LOAD_AUDIO_EVENT);
	UpdateStreaming(DEFAULT_AUDIO_STREAMING_OFFSET, m_iNextAudioLoadEvent, StreamingFlags, GetTime());

	StreamingFlags.ClearAllFlags();

	cutsManager::DoPlayState();

	StreamingFlags.SetFlag(CS_UNLOAD_MODELS_EVENT | CS_UNLOAD_ANIM_DICT_EVENT | CS_UNLOAD_PARTICLE_EFFECTS_EVENT | CS_UNLOAD_SCENE_EVENT
		| CS_UNLOAD_AUDIO_EVENT| CS_UNLOAD_OVERLAYS_EVENT | CS_UNLOAD_RAYFIRE_EVENT | CS_UNLOAD_SUBTITLES_EVENT);

	UpdateStreaming(0.0f, m_iNextUnloadEvent, StreamingFlags, GetTime());

	StreamingFlags.ClearAllFlags();

	if(IsSkippingPlayback())
	{
		//we are skipping
		if( m_bFadeOnSeamlessSkip && (m_SkipFrame != m_iEndFrame && m_SkipFrame != m_iEndFrame -1) )
		{
			FadeInGameAtEnd(m_fadeInGameAtEndWhenStopping);
		}

		SetIsSkipping(false);
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DoLoadingState()
{
	cutsceneManagerDebugf3("Do DoLoadingState Frame:%d", fwTimer::GetFrameCount());
	
	cutsManager::DoLoadingState();

	if(cutsManager::IsLoading())
	{
		for(atMap<s32, SEntityObject>::Iterator It = m_cutsceneEntityObjects.CreateIterator(); !It.AtEnd(); It.Next())
		{
			SEntityObject &entityObject = It.GetData();
			if(entityObject.bIsLoading)
			{
				if(entityObject.pObject)
				{
					cutsceneManagerDebugf3("Do DoLoadingState Loading: %s", entityObject.pObject->GetDisplayName().c_str());
				}
			}
		}

#if __BANK

		if(m_pAssetManger)
		{
			m_pAssetManger->PrintStreamingRequests();
		}

		if(m_pAnimManager)
		{
			m_pAnimManager->PrintStreamingRequests();
		}

#endif // __BANK
	}
#if __BANK
	else
	{
		SyncLightData(); 
	}
#endif
#if __BANK
	m_DebugManager.PopulateSelectorList(m_DebugManager.m_pPedSelectCombo, "Ped Selector", m_DebugManager.m_iPedModelIndex, m_pedModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedPedEntityCallBack), (datBase*)&m_DebugManager) );
	SetFrameRangeForPlayBackSliders();
#endif
}


bool CutSceneManager::AreReservedEntitiesReady()
{
	if(m_bAreScriptReservedEntitiesLoaded)
	{
		return true;
	}
	bool AllEntitiesAreReady = true;

	if(m_ObjectIdsReserevedForPostScene.GetCount() > 0)
	{
		for(int i =0; i < m_ObjectIdsReserevedForPostScene.GetCount(); i++)
		{
			SEntityObject* pEntityObject =  m_cutsceneEntityObjects.Access(m_ObjectIdsReserevedForPostScene[i]);

			if(pEntityObject)
			{
				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
				{
					CCutsceneAnimatedActorEntity* pActor = static_cast<CCutsceneAnimatedActorEntity*>(pEntityObject->pEntity);

					if(pActor)
					{
						if(pActor->GetGameEntity())
						{
							if(!pActor->GetGameEntity()->HaveAllStreamingReqsCompleted() || !GetAssetManager()->HaveAllVariationsLoaded(PED_VARIATION_TYPE))
							{
								AllEntitiesAreReady = false;
							}
						}
						else if(pActor->GetGameRepositionOnlyEntity())
						{
							if(!pActor->GetGameRepositionOnlyEntity()->HaveAllStreamingReqsCompleted() )
							{
								AllEntitiesAreReady = false;
							}
						}
						else
						{
							cutfObjectIdEvent UpdateEvent(m_ObjectIdsReserevedForPostScene[i], 0.0f, CUTSCENE_UPDATE_EVENT );
							DispatchEvent(&UpdateEvent);
							AllEntitiesAreReady = false;
						}
					}
				}
			}
		}
	}

	if(AllEntitiesAreReady)
	{
		return m_bAreScriptReservedEntitiesLoaded = true;
	}
	else
	{
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DoPausedState()
{
	// this is only for debug pause
#if __BANK
	if(IsCutscenePlayingBack())
	{
		DispatchEventToObjectsOfType(CUTSCENE_MODEL_OBJECT_TYPE, CUTSCENE_PAUSE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_CAMERA_OBJECT_TYPE, CUTSCENE_PAUSE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE, CUTSCENE_PAUSE_EVENT);
	}

#endif
}

/////////////////////////////////////////////////////////////////////////////////////
//This function is called when scene fades out. Primarily this is done when the scene first starts and also when skipping.
//The fade out state is also called for a seamless scene whose assets have not loaded yet, need a fade to cover this. Need also to cover when a
//cut file hasn't loaded yet.

void CutSceneManager::DoFadingOutState()
{
	// wait for the fade to complete
	if (camInterface::IsFadedOut() || !m_bFadeOnSeamlessSkip || IsFadeTimerComplete())	//this is a change from checking the cut scene timer, lets just look at the camera system.
	{
		if ( m_bIsStoppingForEndFadeOut )
		{
			m_cutsceneState = CUTSCENE_STOPPED_STATE;

			DispatchEventToAllEntities( CUTSCENE_STOP_EVENT );
		}
		else
		{
			if(IsSkippingPlayback())
			{
				//we are loading so we cant skip

				m_cutsceneState = CUTSCENE_SKIPPING_STATE;
			}
			else
			{

				if(m_streamingState == STREAMING_ASSETS_STATE)
				{
					m_cutsceneState = CUTSCENE_LOADING_STATE;	// the cut scene is already loading but a fade has been called
				}
				else
				{
					if(m_streamingState == STREAMING_CUTFILE_STATE)
					{
						m_cutsceneState = CUTSCENE_LOADING_CUTFILE_STATE; //some has called play even before cutfile has loaded
					}
					else if(m_streamingState == STREAMING_IDLE_STATE && !GetCutfFile())
					{
						m_cutsceneState = CUTSCENE_LOAD_CUTFILE_STATE;				// load
					}
					else
					{
						m_cutsceneState = CUTSCENE_LOAD_STATE;
					}
				}
			}
		}
		if (m_cutsceneState!= CUTSCENE_FADING_OUT_STATE)
			m_EndFadeTime = 0;
	}
	else
	{
		if(IsSkippingPlayback())
		{
			//specifically calling the base class method as there is no code related to skipping like the derived class.
			//This ensures we keep dispatching events correctly. 
			cutsManager::UpdatePlayBackEvents(); 	
		}
		else
		{
			// otherwise, keep updating
			cutsUpdateEventArgs updateEventArgs( GetTime(), m_fDeltaTime, m_fSectionStartTime, m_fSectionDuration );
			DispatchEventToAllEntities( m_bIsStoppingForEndFadeOut ? CUTSCENE_UPDATE_EVENT : CUTSCENE_UPDATE_FADING_OUT_AT_BEGINNING_EVENT,
				&updateEventArgs );
		}

	}

}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DoUnloadState()
{
	if(!GetCutfFile())
	{
		m_cutsceneState = CUTSCENE_IDLE_STATE;
		m_streamingState = STREAMING_IDLE_STATE;
		ResetCutSceneVars();
		return;
	}

	if (!gStreamingRequestList.IsRunFromScript())
	{
	streamDisplayf("Cutscene unloaded - removing SRL");
	gStreamingRequestList.Finish();
	}

	const atArray<cutfEvent*>& pLoadEventList = GetCutfFile()->GetLoadEventList();
	const atArray<cutfObject*>& pObjectList = GetCutfFile()->GetObjectList();

	// release all entities, except for those that we need to dispatch the unload events.  That way if we unload
	// a model, for example, the entity won't still have control over it.
	atArray<s32> iObjectIdsToRelease;
	GetObjectIdsInEventList( pLoadEventList, iObjectIdsToRelease, true );

	for ( int i = 0; i < iObjectIdsToRelease.GetCount(); ++i )
	{
		if ( (pObjectList[iObjectIdsToRelease[i]]->GetType() == CUTSCENE_SCREEN_FADE_OBJECT_TYPE)
			&& (((m_fadeInGameAtEnd == DEFAULT_FADE) && GetCutfFile()->GetCutsceneFlags().IsSet( cutfCutsceneFile2::CUTSCENE_FADE_IN_GAME_FLAG ))
			|| (m_fadeInGameAtEnd == FORCE_FADE)) )
		{
			// If we need to do a fade in, don't delete the Screen Fade Object just yet.
			continue;
		}

		DeleteEntityObject( pObjectList[iObjectIdsToRelease[i]] );

#if __BANK
		//need to cleanup the entity lists for debugging
		m_DebugManager.m_pActorEntity = NULL;
#endif
	}


	//Remove all remaining requests from the streaming system
	if (m_bStreamObjectsWhileSceneIsPlaying BANK_ONLY(&& !m_bLoop))
	{
		for (;m_iNextLoadEvent < pLoadEventList.GetCount(); m_iNextLoadEvent++ )
		{
			fwFlags32 StreamingFlags;

			StreamingFlags.SetFlag(CS_UNLOAD_MODELS_EVENT | CS_UNLOAD_ANIM_DICT_EVENT | CS_UNLOAD_PARTICLE_EFFECTS_EVENT | CS_UNLOAD_SCENE_EVENT
				| CS_UNLOAD_AUDIO_EVENT| CS_UNLOAD_OVERLAYS_EVENT | CS_UNLOAD_RAYFIRE_EVENT);

			if (ValidateEvent(pLoadEventList[m_iNextLoadEvent]->GetEventId(), StreamingFlags))
			{
				DispatchEvent(pLoadEventList[m_iNextLoadEvent]);
			}
		}
	}
	else
	{
		//Dispatch all unload events, dont know what we have loaded
		for (int i = 0; i < pLoadEventList.GetCount(); i++ )
		{
			fwFlags32 StreamingFlags;

			StreamingFlags.SetFlag(CS_UNLOAD_MODELS_EVENT | CS_UNLOAD_ANIM_DICT_EVENT | CS_UNLOAD_PARTICLE_EFFECTS_EVENT | CS_UNLOAD_SCENE_EVENT
				| CS_UNLOAD_AUDIO_EVENT| CS_UNLOAD_OVERLAYS_EVENT | CS_UNLOAD_RAYFIRE_EVENT);

			if (ValidateEvent(pLoadEventList[i]->GetEventId(), StreamingFlags))
			{
				DispatchEvent(pLoadEventList[i]);
			}
		}
	}


	m_cutsceneState = CUTSCENE_UNLOADING_STATE;
}

/////////////////////////////////////////////////////////////////////////////////////
//dispatch the load dictionary events

void CutSceneManager::DoSkippingState()
{
	// If we are skipping the entire cutscene, unload the SRL now so we have enough memory left over for the final state.
	if (WasSkipped())
	{
		if (!gStreamingRequestList.IsRunFromScript())
		{
		streamDisplayf("Cutscene skipped - removing SRL");
		gStreamingRequestList.Finish();
	}
	}

	//we are faded out pause all the anims playing
	DispatchEventToAllEntities(CUTSCENE_PAUSE_EVENT);

	// dispatch the skipped event to our cutscene entities
	DispatchEventToAllEntities(CUTSCENE_SKIPPED_EVENT);

	//Store the section before the skip
	m_SectionBeforeSkip = GetSectionForTime( m_fPreviousTime );

	//calculate the time change
	m_fTargetSkipTime = CalculateSkipTargetTime (m_SkipFrame);

	if(m_fTargetSkipTime != GetTime())
	{
		m_bApplyTargetSkipTime = true;
	}

	int iTargetSkipSectionSection = GetSectionForTime( m_fTargetSkipTime );

	if(m_bFadeOnSeamlessSkip && (m_SkipFrame == m_iEndFrame || m_SkipFrame == m_iEndFrame -1) )
	{
		if(!m_HasScriptOverridenFadeValues)
		{
			m_fadeInGameAtEnd = FORCE_FADE;
			m_fadeInGameAtEndWhenStopping = FORCE_FADE;
		}
	}
	else
	{
		m_fadeInGameAtEndWhenStopping = FORCE_FADE;
	}

	if(m_SectionBeforeSkip != iTargetSkipSectionSection)
	{
		cutfTwoFloatValuesEventArgs UpdateEventArgs((float)m_SectionBeforeSkip, (float)iTargetSkipSectionSection);
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE , CUTSCENE_LOAD_ANIM_DICT_EVENT, &UpdateEventArgs);
		m_streamingState = STREAMING_ASSETS_STATE;
	}

	//if(m_bApplyTargetSkipTime)
	{
		RequestScriptReservedAssetsForSkip();

		m_cutsceneState = CUTSCENE_LOADING_BEFORE_RESUMING_STATE;
	//	DoLoadingBeforeResumingState();
	}

	// simulate time passing on the radio
	g_RadioAudioEntity.SkipStationsForward();
}

void CutSceneManager::DoStoppedState()
{
	cutsManager::DoStoppedState();

	//update the the unload states immediately don't need to wait
	DoUnloadState();
	DoUnloadingState();

	cutsceneManagerDebugf3("DoStoppedState CUTSCENE_END_OF_SCENE_EVENT");
}

/////////////////////////////////////////////////////////////////////////////////////

bool CutSceneManager::IsLoading() const
{
	if (cutsManager::IsLoading())
	{
		return true;
	}

	if (m_cutsceneState == CUTSCENE_LOADING_STATE)
	{
		return !gStreamingRequestList.AreAllAssetsLoaded();
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
//Updates the cut scene timer and dispatched all the vents for the scene

void CutSceneManager::PreSceneUpdate()
{
	SYS_CS_SYNC(sm_CutsceneLock);

#if GTA_REPLAY
	//we don't need to process the cutscenes in replay so we can skip this
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		return;
	}
#endif


	DIAG_CONTEXT_MESSAGE("Cutscene pre scene update: %s", GetCutsceneName());

	m_bHasCutThisFrame = false;

	TriggerCutsceneSkip();

	if(m_bShouldStopNow)
	{
		Stop( SKIP_FADE , m_fadeInGameAtEnd);
		m_bShouldStopNow = false;
		return;
	}

	if(!fwTimer::IsGamePaused() )
	{
		StartCutscene();

#if __BANK
		SoakTest();
#endif
	}

#if  !__FINAL
	if(IsCutscenePlayingBack())
	{
		if(fwTimer::GetSingleStepThisFrame() && !m_bWasSingledStepped)
		{
			m_bWasSingledStepped = true;
		}
	}
#endif

	if(!fwTimer::IsGamePaused() )
	{
		//need to resync audio after stepping in game
#if  !__FINAL
		if(IsCutscenePlayingBack())
		{
			if(m_bWasSingledStepped && !fwTimer::GetSingleStepThisFrame())
			{
				if(m_PlayBackState == PLAY_STATE_FORWARDS_NORMAL_SPEED)
				{
					PauseResume();
					m_bWasSingledStepped = false;
				}
			}
		}
#endif

		UpdateCutSceneTimer();

		CleanupTerminatedScriptCutSceneAssets();

		cutsManager::PreSceneUpdate();

#if ENABLE_CUTSCENE_TELEMETRY
		g_CutSceneLightTelemetryCollector.Update(this);
#endif

	}

	if(IsCutscenePlayingBack())
	{
		CShockingEventsManager::SuppressAllShockingEvents();
	}
	//RemoveIncorrectlyRegisteredScriptObjects();

	// Minimap rendering during cutscene
	if (IsCutscenePlayingBack())
	{
		if (GetDisplayMiniMapThisUpdate())
		{
			CMiniMap::SetVisible(true);
		}
		else
		{
			CMiniMap::SetVisible(false);
		}
	}
	SetDisplayMiniMapThisUpdate(false); // Reset

#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD
	if(!IsPlaying() && !IsPaused())
	{
		grcDebugDraw::SetDisplayDebugText(true);
	}
	else
	{
		grcDebugDraw::SetDisplayDebugText(false);


	}
#endif

#if __BANK
	if (IsCutscenePlayingBack())
	{
		if(!fwTimer::IsGamePaused() && !fwTimer::GetSingleStepThisFrame() )
		{
			if(!PARAM_nocutarrowkeys.Get())
			{
				if(!IsLoading() && !WasSkipped())
				{
					m_DebugManager.RightArrowFastForward();
					m_DebugManager.LeftArrowRewind();
					m_DebugManager.UpArrowPause();
					m_DebugManager.DownArrowPlay();
					m_DebugManager.CtrlRightArrowStep();
					m_DebugManager.CtrlLeftArrowStep();
				}
			}
		}
		OverrideGameTime();

		UpdateLightDebugging(); 
	}

	cutsManager::DebugDraw();
	DebugDraw();

	m_bAllowAudioSyncFromRag = m_bAudioSyncWhenJogging;

	if (IsRunning())
	{
		m_DebugManager.Update();

		if (IsCutscenePlayingBack())
		{
			if(!fwTimer::IsGamePaused() )
			{
				if(m_bActivateBlockingObjectEditing)
				{
					m_DebugManager.UpdateMouseCursorPosition(m_vBlockingBoundPos, m_vBlockingBoundPos, m_DebugManager.m_vWorldZOffset);
				}
				m_DebugManager.GetMapObject();
			}

			if(m_DebugManager.m_bDisplayLoadedModels)
			{
				grcDebugDraw::AddDebugOutput("- Loaded Models -");
				atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
				for ( entry.Start(); !entry.AtEnd(); entry.Next() )
				{
					SEntityObject *pEntityObject = m_cutsceneEntityObjects.Access(entry.GetKey());

					if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
						|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY || pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
					{
						CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);

						if(pModel && pModel->GetGameEntity())
						{
							if(pModel->GetCutfObject())
							{
								grcDebugDraw::AddDebugOutput("Loaded: %s", pModel->GetCutfObject()->GetDisplayName().c_str());
							}
						}
					}
				}
			}

			m_DebugManager.DisplayAndEditBlockingBounds();
			m_DebugManager.DisplayHiddenObject();
			m_DebugManager.FixupSelectedObject();

			//cutsceneDisplayf("Cutscene time:%f, Section:%d, Frame:%d ", GetCutSceneCurrentTime(), GetCurrentSection(), GetCutSceneCurrentFrame() + m_iStartFrame);
		}
	}
#endif

#if USE_MULTIHEAD_FADE
	if(!m_bManualControl)
	{
		const bool bCutIsActive = IsPlaying();
		u32 uCurrentTime = fwTimer::GetSystemTimeInMilliseconds();
		if(uCurrentTime > fade::uEndTime)
		{
			if (bCutIsActive && !m_bBlindersUp)
			{
				StartMultiheadFade(true);
			}

			if(!bCutIsActive && m_bBlindersUp)
			{
				StartMultiheadFade(false);
			}
		}
	}
#endif
}

///////////////////////////////////////////////////////////////////////////
// Applies post scene update

void SetPlayerControl(bool ActivateControl)
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	
	if(pPlayerPed)
	{
		if(ActivateControl)	
		{
			pPlayerPed->GetPlayerInfo()->EnableControlsCutscenes();
			pPlayerPed->GetPlayerInfo()->EnableControlsCamera();
		}
		else
		{
			pPlayerPed->GetPlayerInfo()->DisableControlsCutscenes();
			pPlayerPed->GetPlayerInfo()->DisableControlsCamera();
		}
	}
}

void CutSceneManager::PostSceneUpdate(float UNUSED_PARAM(fDelta))
{
#if GTA_REPLAY
	//we don't need to process the cutscenes in replay so we can skip this
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	DIAG_CONTEXT_MESSAGE("Cutscene post scene update: %s", GetCutsceneName());

	DispatchUpdateEventPostScene();

	if (IsCutscenePlayingBack())
	{
		SetGameToCutSceneStatePostScene(); 

		if(!fwTimer::IsGamePaused())
		{
			if(!NetworkInterface::IsGameInProgress())
		{
			// To make sure the world is always being streamed in correctly,
			// when the player is invisible, move them to the scene origin.
			if(m_iPlayerObjectId != -1)
			{
				int iPreviousConcatSection = GetConcatSectionForTime(GetCutScenePreviousTime());
				int iCurrentConcatStection = GetConcatSectionForTime(GetCutSceneCurrentTime());
				if(iPreviousConcatSection != iCurrentConcatStection)
				{
					cutsEntity *pEntity = GetEntityByObjectId(m_iPlayerObjectId);
					if(pEntity && pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
					{
						CCutsceneAnimatedActorEntity *pActor = static_cast< CCutsceneAnimatedActorEntity * >(pEntity);
						CPed *pPed = pActor->GetGameEntity();
							if(pPed && pPed->IsLocalPlayer() && !pPed->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE) && !pActor->IsGameEntityReadyForGame())
						{
							cutsceneManagerDebugf3("Setting player '%s' to the scene origin (cos he's invisible)", pPed->GetModelName());
							pPed->SetPosition(GetSceneOffset(), true, false, true);
						}
					}
				}
			}
		}
		}
		else
		{
			//because lights keep updating even though the game is paused, crazy i know.
			SetPostSceneUpdate(true);
			DispatchEventToObjectsOfType(CUTSCENE_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
			DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		}
	}


	if(m_ShouldEnablePlayerControlPostScene)
	{
		SetPlayerControl(true); 	
		m_ShouldEnablePlayerControlPostScene = false; 
	}

#if __BANK
	if(!fwTimer::IsGamePaused() )
	{
		//we have called a step forward set the flags to say we have finished skipping
		//if ( m_bIsStepping )
		//{
		//	m_bIsStepping = false;
		//	m_bStepped = true;
		//}

		//if you are playing a scene backwards and then want to stop, it the fade will fail so set to 1.0 so it will fade out.
		if(IsFadingOutAtEnd() && GetPlayBackState() == PLAY_STATE_BACKWARDS)
		{
			m_PlayBackState = PLAY_STATE_FORWARDS_NORMAL_SPEED;
		}

		if(GetHasCutThisFrame())
		{
			PopulateActiveLightList(); 
			BankActiveLightSelectedCallBack(); 
		}
	}
#endif

#if __BANK
	if(!fwTimer::IsGamePaused() )
	{
		if(m_bStartedFromWidget)
		{
			if(IsLoaded())
			{
				m_DebugManager.UpdateTimeCycleParamsCB();

				PlaySeamlessCutScene(THREAD_INVALID);
				m_bStartedFromWidget = false;
			}
		}
	}

#endif
}

bool CutSceneManager::AreAllAudioAssetsLoaded()
{
	//mangers to the cuts class
	//atArray<cutsEntity*>  AudioEntities;

	//GetEntityByType(CUTSCENE_SOUND_GAME_ENTITY, AudioEntities);
	//
	//for(int i =0; i < AudioEntities.GetCount(); i++)
	//{
	//	CCutSceneAudioEntity* pAudioEnt = static_cast<CCutSceneAudioEntity*>(AudioEntities[i]);

	//	if(/*pAudioEnt->IsAudioRequested() &&*/ !pAudioEnt->IsAudioLoaded())
	//	{
	//		return false;
	//	}
	//}

	return true;
};

//void CutSceneManager::CheckAudioAssetsAreReadyThisFrame()
//{
//	if(IsCutscenePlayingBack() && !m_bShouldPausePlaybackForAudioLoad && !m_bCutsceneWasSkipped)
//	{
//		if(GetCutfFile())
//		{
//			atArray<cutfEvent*> AllEventList;
//			AllEventList = GetCutfFile()->GetEventList();
//
//			if(m_iNextEventIndex >= 0)
//			{
//				for(int i = m_iNextEventIndex; i < AllEventList.GetCount(); i++)
//				{
//					if(AllEventList[i])
//					{
//						//check for play audio events
//						if(AllEventList[i]->GetEventId() == CUTSCENE_PLAY_AUDIO_EVENT && AllEventList[i]->GetTime() <= GetSeconds() + m_fClockTimeStep)
//						{
//							//check that the object that this event is meant for is ready to play the event, if not then pause.
//							const cutfObjectIdEvent* pObjectEvent = static_cast<const cutfObjectIdEvent*>(AllEventList[i]);
//
//							if(pObjectEvent)
//							{
//								const cutfObject* pAudioObj = GetObjectById(pObjectEvent->GetObjectId());
//
//								if(pAudioObj)
//								{
//									 SEntityObject* pEnt = GetEntityObject(pAudioObj, false);
//
//									if(pEnt&& pEnt->pEntity)
//									{
//										cutsEntity* pEntity = pEnt->pEntity;
//
//										if(pEntity->GetType() == CUTSCENE_SOUND_GAME_ENTITY)
//										{
//											CCutSceneAudioEntity* pAudioEnt = static_cast<CCutSceneAudioEntity*>(pEntity);
//
//											if((!pAudioEnt->IsAudioLoaded()) && pAudioEnt->IsAudioRequested())
//											{
//												m_bShouldPausePlaybackForAudioLoad = true;
//												Pause();
//												break;
//											}
//										}
//									}
//								}
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//
//}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DispatchEventToAllEntities(s32 iEventId, const cutfEventArgs *pEventArgs )
{
	cutsManager::DispatchEventToAllEntities(iEventId, pEventArgs );

	switch (iEventId)
	{
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
			//cutsceneDisplayf("Start cutscene playback");
			//defaulted to 0 this sets the cut scene time to zero at the start of the play back.
			cutsceneManagerDebugf2("DispatchEventToAllEntities CUTSCENE_START_OF_SCENE_EVENT %s", GetCutsceneName());
			InitialiseCutSceneTimer();
			UpdateSectionInfo();
			m_IsCutscenePlayingBack = true;
			m_bCameraWillCameraBlendBackToGame = true;
			m_bShouldRestoreCutsceneAtEnd = true;
			m_ShouldEnablePlayerControlPostScene = false;
			m_bPresceneLightUpdateEvent = false; 
			m_bShouldProcessEarlyCameraCutEventsForSinglePlayerExits = true; 
			m_ScriptThread = THREAD_INVALID; //set to invalid because if the script dies the scene can will clean itself up.

			if(NetworkInterface::IsGameInProgress() && !m_OptionFlags.IsFlagSet(CUTSCENE_DO_NOT_REPOSITION_PLAYER_TO_SCENE_ORIGIN))
			{
				m_OptionFlags.SetFlag(CUTSCENE_DO_NOT_REPOSITION_PLAYER_TO_SCENE_ORIGIN); 
			}
			
			//Move the player to the scene origin to stream in the collision if not in the scene.
			CPed * pPlayer = CGameWorld::FindLocalPlayer();
			if(!GetIsPlayerInScene() && !m_OptionFlags.IsFlagSet(CUTSCENE_DO_NOT_REPOSITION_PLAYER_TO_SCENE_ORIGIN) && !NetworkInterface::IsGameInProgress())
			{
				if (pPlayer)
				{
					m_vPlayerPositionBeforeScene = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
					pPlayer->Teleport(GetSceneOffset(), 0.0f);
					pPlayer->RemovePhysics();
					pPlayer->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false, true);
					cutsceneManagerVisibilityDebugf3("CUTSCENE_START_OF_SCENE_EVENT: Hiding player (%s)", pPlayer->GetModelName());
				}
			}

			if (pPlayer && !GetOptionFlags().IsFlagSet(CUTSCENE_PLAYER_TARGETABLE))
			{
				CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
				if (pPlayerInfo)
				{
					if (pPlayerInfo->GetWanted().m_EverybodyBackOff)
					{
						// if the flag is already set, leav eit as it is, and
						// set the CUTSCENE_PLAYER_TARGETABLE flag so that we don't
						// try to turn it back off again at the end
						GetOptionFlags().SetFlag(CUTSCENE_PLAYER_TARGETABLE);
					}
					else
					{
						// set the everybody back off flag if requested
						// to stop hostile peds attacking the player
						pPlayerInfo->GetWanted().m_EverybodyBackOff = true;
					}
				}
			}

			if(GetOptionFlags().IsFlagSet(CUTSCENE_PROCGRASS_FORCE_HD))
			{
				CPlantMgr::SetForceHDGrass(true);
			}

			// Begin playback of the streaming request list recording
			if (!gStreamingRequestList.IsRunFromScript())
			{
			gStreamingRequestList.Start();
			}

#if __BANK
			IsSceneApproved();
			m_fApprovalMessagesOnScreenTimer = 0.0f;
#endif
			if(FindAudioObjectId() == -1)
			{
				if(g_CutsceneAudioEntity.GetNumCutsceneEvents() == 0 )
				{
					cutsceneManagerDebugf1("No audio entity in data - Triggering cutscene audio");
					g_CutsceneAudioEntity.TriggerCutscene();
				}
				else
				{
					cutsceneManagerWarningf("No audio entity in data, but cutscene audio entity has events! Will the audio start!?");
				}
			}
		}
		break;

	case CUTSCENE_PAUSE_EVENT:
		if (!gStreamingRequestList.IsRunFromScript())
		{
		gStreamingRequestList.Pause();
		}
		break;

	case CUTSCENE_PLAY_EVENT:
		{
			cutsceneManagerDebugf2 ("Cutscene manager: CUTSCENE_PLAY_EVENT");

			// Begin/resume playback of the streaming request list recording
			if (!gStreamingRequestList.IsRunFromScript())
			{
			gStreamingRequestList.Start();
		}
			m_bUpdateCutsceneTimer = true; 
		}
		break;


	case CUTSCENE_STOP_EVENT:
		{
			streamDisplayf("Cutscene stopped - removing SRL");
			if (!gStreamingRequestList.IsRunFromScript())
			{
			gStreamingRequestList.Finish();
			}

			CPed * pPlayer = CGameWorld::FindLocalPlayer();
			//Return the player to the point he was at when the scene began
			if(!GetIsPlayerInScene() && IsCutscenePlayingBack() && !m_OptionFlags.IsFlagSet(CUTSCENE_DO_NOT_REPOSITION_PLAYER_TO_SCENE_ORIGIN) && !NetworkInterface::IsGameInProgress())
			{
				if (pPlayer)
				{
					if (pPlayer->GetUsingRagdoll())
					{
						pPlayer->SwitchToAnimated();
					}
					else
					{
						if(!pPlayer->GetCurrentPhysicsInst()->IsInLevel())
						{
							pPlayer->AddPhysics();
						}
					}

					pPlayer->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true, true);
					cutsceneManagerVisibilityDebugf3("CUTSCENE_STOP_EVENT: Showing player (%s)", pPlayer->GetModelName());

					pPlayer->Teleport(m_vPlayerPositionBeforeScene, 0.0f, true);
				}
			}

			if (pPlayer && !GetOptionFlags().IsFlagSet(CUTSCENE_PLAYER_TARGETABLE))
			{
				CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
				if (pPlayerInfo)
				{
					pPlayerInfo->GetWanted().m_EverybodyBackOff = false;
				}
			}

			if(GetOptionFlags().IsFlagSet(CUTSCENE_PROCGRASS_FORCE_HD))
			{
				CPlantMgr::SetForceHDGrass(false);
			}

			if(!WasSkipped())
			{
				CNetworkTelemetry::CutSceneEnded(atStringHash(GetCutsceneName()), (u32)(GetCutSceneCurrentTime() * 1000.0f));
			}

			//m_bCutSceneUpdating = false;

			if(m_bShouldRestoreCutsceneAtEnd)
			{
				if(m_OptionFlags.IsFlagSet(CUTSCENE_DELAY_ENABLING_PLAYER_CONTROL_FOR_UP_TO_DATE_GAMEPLAY_CAMERA))
				{
					SetCutSceneToGameState(false);
				}
				else
				{
					SetCutSceneToGameState(true);
				}
				m_bShouldRestoreCutsceneAtEnd = false;
			}

			m_IsCutscenePlayingBack = false;
			m_bCameraWillCameraBlendBackToGame = false;
			m_bUpdateCutsceneTimer = false; 
			SetLightPresceneUpdate(true); 

			if(m_bReplayRecording)
			{
				m_bReplayRecording = false;

				// !!TODO!! Stop replay recording...
			}
		}
		break;

	case CUTSCENE_END_OF_SCENE_EVENT:
		{
			if(m_OptionFlags.IsFlagSet(CUTSCENE_DELAY_ENABLING_PLAYER_CONTROL_FOR_UP_TO_DATE_GAMEPLAY_CAMERA))
			{	
				if(GetCamEntity() && GetCamEntity()->IsRegisteredGameEntityUnderScriptControl())
				{
					SetPlayerControl(true); 
				}
				else
				{
					m_ShouldEnablePlayerControlPostScene = true; 
				}
			}
		}
		break;

	//case CUTSCENE_STOP_EVENT:
	//	{
	//		m_bWaitingToFinish = true;
	//
	//		cutsceneDisplayf("MANAGER CUTSCENE_STOP_EVENT %d", fwTimer::GetFrameCount());
	//	}
	//	break;

	case CUTSCENE_UPDATE_EVENT:
		{
			SetPreSceneUpdate(true);

			//Check that the cut scene can be skipped

			if (!gStreamingRequestList.IsRunFromScript())
			{
				gStreamingRequestList.SetTime(GetCutSceneCurrentTime()*1000.0f);
			}
		}
		break;

	case CUTSCENE_START_REPLAY_RECORD:
		{
			if (m_bEnableReplayRecord && Verifyf(!m_bReplayRecording, "We are already replay recording!"))
			{
				m_bReplayRecording = true;

				// !!TODO!! Start replay recording...
			}
		}
		break;

	case CUTSCENE_STOP_REPLAY_RECORD:
		{
			if (m_bEnableReplayRecord && Verifyf(m_bReplayRecording, "We are not replay recording!"))
			{
				m_bReplayRecording = false;

				// !!TODO!! Stop replay recording...
			}
		}
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DispatchUpdateEventPostScene()
{

	if(IsCutscenePlayingBack())
	{

#if __BANK
	bool updatedThisFrame = false;
#endif
	//check that the pre scen update has been called.
	if (GetPreSceneUpdate())
	{
		//tell our post scene systems to update
		SetPostSceneUpdate(true);
		DispatchEventToObjectsOfType(CUTSCENE_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_PARTICLE_EFFECT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_DECAL_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_OVERLAY_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);

#if __BANK
		updatedThisFrame = true;
#endif
		//reset our update flags to be set next time round.
		SetPostSceneUpdate(false);
		SetPreSceneUpdate(false);
	}

	if(IsPlaying() || IsLoadingBeforeResuming())
	{
		//this is to unload the unused dictionaries
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE, CUTSCENE_UNLOAD_ANIM_DICT_EVENT);
	}

	//Lights need to be updated even if paused so we dispatch the update event. Particle effects an other systems are updated outside
	// by other game systems.
#if __BANK
	if (IsPaused() && !updatedThisFrame)
	{
		SetPostSceneUpdate(true);
		DispatchEventToObjectType( CUTSCENE_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectType( CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_PARTICLE_EFFECT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
		DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);

		SetPostSceneUpdate(false);
	}
#endif
	}
	else if(GetLightPresceneUpdateOnly())
	{
		if(GetCamEntity() && !GetCamEntity()->IsRegisteredGameEntityUnderScriptControl())
		{
			SetPostSceneUpdate(true);
			DispatchEventToObjectsOfType(CUTSCENE_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);
			DispatchEventToObjectsOfType(CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE, CUTSCENE_UPDATE_EVENT);

			SetPostSceneUpdate(false);
		}

		SetLightPresceneUpdate(false); 
	}
};

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DispatchEventToObjectsOfType( s32 iObjectType, s32 iEventId, cutfEventArgs *pEventArgs )
{
	if(!Verifyf(GetCutfFile(), "DispatchEventToObjectType: No cutf file object loaded to find objects of type "))
	{
		return;
	}

	atArray<cutfObject*> objectList;
	GetCutfFile()->FindObjectsOfType( iObjectType, objectList );

	cutfObjectIdEvent evt( 0, GetTime(), iEventId, pEventArgs );

	for ( int i = 0; i < objectList.GetCount(); ++i )
	{
		evt.SetObjectId( objectList[i]->GetObjectId() );
		DispatchEvent( &evt );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::SetNewStartPos(const Vector3 &vPos)
{
	SetSceneOffset(vPos);
}

void CutSceneManager::OverrideConcatSectionPosition(const Vector3 &vPos, s32 concatsection)
{
	if(IsConcatSectionValid(concatsection))
	{
		m_concatDataList[concatsection].vOffset = vPos;
	}
}

void CutSceneManager::OverrideConcatSectionHeading(const float& heading, s32 concatsection)
{
	if(IsConcatSectionValid(concatsection))
	{
		m_concatDataList[concatsection].fRotation = heading;
	}
}

void CutSceneManager::OverrideConcatSectionPitch(const float& pitch, s32 concatsection)
{
	if(IsConcatSectionValid(concatsection))
	{
		m_concatDataList[concatsection].fPitch = pitch;
	}
}

void CutSceneManager::OverrideConcatSectionRoll(const float& roll, s32 concatsection)
{
	if(IsConcatSectionValid(concatsection))
	{
		m_concatDataList[concatsection].fRoll = roll;
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::SetNewStartHeading(const float &fHeading)
{
	SetSceneRotation(fHeading);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//search the entities for a blocking bound and see if the point is in it

bool CutSceneManager::IsPointInBlockingBound(const Vector3 &vVec)
{
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		if (entry.GetData().pEntity->GetType() == CUTSCENE_BLOCKING_BOUND_GAME_ENTITY)
		{
			CCutSceneBlockingBoundsEntity* pBounds = static_cast<CCutSceneBlockingBoundsEntity*>(entry.GetData().pEntity);
			if (pBounds->IsInside(vVec))
			{
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////
//Calculate the time step from a unified audio timer.

void CutSceneManager::ComputeTimeStep()
{
//	double fMixerTimer = 0.0;
//
//#if __BANK
//
//	if (m_bUseAudioTimeStep)
//	{
//		if(audDriver::GetMixer())
//		{
//			fMixerTimer = g_CutsceneAudioEntity.GetCutsceneTime()/1000.f;
//		}
//	}
//	else
//	{
//		fMixerTimer = fwTimer::GetTimeInMilliseconds() / 1000.0f;
//	}
//
//#else
//	if(audDriver::GetMixer())
//	{
//		fMixerTimer= audDriver::GetMixer()->GetMixerTimeS();
//	}
//	else
//	{
//		fMixerTimer = fwTimer::GetTimeInMilliseconds() / 1000.0f ;
//	}
//#endif
//
//	m_fClockTimeStep = static_cast<float>(fMixerTimer - m_fLastFrameTime);
//
//	m_fLastFrameTime = fMixerTimer;

	//m_fTime =  = g_CutsceneAudioEntity.GetCutsceneTime()/1000.f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Get the phase of the cut scene.

float CutSceneManager::GetCutScenePhase() const
{
	if (GetCutfFile())
	{
		float fPhase = Clamp(GetCutSceneCurrentTime()/ GetCutfFile()->GetTotalDuration(), 0.0f, 1.0f);
		return  fPhase;
	}
	else
	{
		return 0.0f;
	}
}


bool CutSceneManager::CreateConcatSectionPlayBackList(s32 PlayBackList)
{
	if(PlayBackList != INVALID_PLAYBACK_FLAG && IsConcatDataValid())
	{
		const atFixedArray<cutfCutsceneFile2::SConcatData, CUTSCENE_MAX_CONCAT> &concatDataList = GetConcatDataList();

		s32 NumberOfInvalidSections = 0;

		for( int i = 0; i < concatDataList.GetCount(); i++ )
		{
			if(!(PlayBackList & BIT(i)))
			{
				m_concatDataList[i].bValidForPlayBack = false;
				NumberOfInvalidSections++;

			}
		}

#if __ASSERT
		char PlayBackSections[256];
		char Section[16];
		formatf(PlayBackSections, sizeof(PlayBackSections)-1, "Sections:" );

		for( int i = 0; i < 32; i++ )
		{
			if(PlayBackList & BIT(i))
			{
				formatf(Section, sizeof(Section)-1, " %d ,", i + 1);

				strcat_s(PlayBackSections, sizeof(PlayBackSections), Section);
			}
		}
#endif


		if(cutsceneVerifyf(NumberOfInvalidSections < concatDataList.GetCount(), "%s: The script playback list is requesting %s but scene has %d sections. The scene will not play", GetCutsceneName(), PlayBackSections, concatDataList.GetCount()) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return true;
}

void CutSceneManager::SetStartAndEndTimeBasedOnPlayBackList(s32 PlayBackList)
{
	if(PlayBackList != INVALID_PLAYBACK_FLAG && IsConcatted())
	{
		//update the start time if the first section is invalid.
		if(!IsConcatSectionValidForPlayBack(0))
		{
			s32 nextValidsection = GetNextValidConcatSection(0);

			if(IsConcatSectionValid(nextValidsection))
			{
				m_fStartTime = m_concatDataList[nextValidsection].fStartTime;
				IntialiseTimers(m_fStartTime, m_fStartTime);
			}

			const atArray<cutfEvent*>& pEventList = GetCutfFile()->GetEventList();
			while ( (m_iNextEventIndex < pEventList.GetCount()) && pEventList[m_iNextEventIndex]->GetTime() < m_fStartTime )
			{
				++m_iNextEventIndex;
			}
		}

		//update the end frame if the final section is invalid for playback.
		for ( int j = m_concatDataList.GetCount() - 1; j >= 0; --j )
		{
			if(m_concatDataList[j].bValidForPlayBack)
			{
				break;
			}
			else
			{
				m_fEndTime = m_concatDataList[j].fStartTime;
			}
		}

		m_iStartFrame = (s32)(m_fStartTime*CUTSCENE_FPS);
		m_iEndFrame = (s32)(m_fEndTime*CUTSCENE_FPS);

		m_fDuration = GetPlayTime(m_fEndTime);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//gets the local phase of that anim for this section

float CutSceneManager::GetAnimPhaseForSection(float fDuration, float fSectionStartTime, float fCurrentTime) const
{
	float fPhase = 0.0f;

	float fAnimCurrentTime = fCurrentTime - fSectionStartTime;

	fPhase = fAnimCurrentTime / fDuration;

	fPhase = Clamp (fPhase, 0.0f, 1.0f);

	return fPhase;
}

/////////////////////////////////////////////////////////////////////////////////////
//Get the phase update for the anim

float CutSceneManager::GetPhaseUpdateAmount(const crClip* pClip, float fEventDispatchTime) const
{
	//check for a blocking tag for a jump cut and set the phase accordingly
	float fPhase = 0.0f;
	if (pClip)
	{

		float fDuration = pClip->GetDuration();

		//get the current phase and previous frame to work out any blocking tags for jump cuts
		 fPhase = GetAnimPhaseForSection(fDuration, fEventDispatchTime,GetCutSceneCurrentTime());

		 float time = pClip->ConvertPhaseToTime(fPhase);
		 float blockTime = pClip->CalcBlockedTime(time);

		fPhase = pClip->ConvertTimeToPhase(blockTime);

	}
	return fPhase;
}

/////////////////////////////////////////////////////////////////////////////////////
//Tell the cut scene manager the name of the cut scene file we want to load
void CutSceneManager::RequestCutscene(const char* pFileName, bool bPlayNow,  EScreenFadeOverride fadeOutGameAtBeginning, EScreenFadeOverride fadeInCutsceneAtBeginning, EScreenFadeOverride fadeOutCutsceneAtEnd, EScreenFadeOverride fadeInGameAtEnd, scrThreadId ScriptId, s32 sPlayBackContextFlags  )
{
#if __BANK
		CDebugTextureViewerInterface::Close(true);
#endif // __BANK
#if ENTITYSELECT_ENABLED_BUILD
		CEntitySelect::DisableEntitySelectPass(); // turn off entityselect, this seems to fix BS#351756
#endif // ENTITYSELECT_ENABLED_BUILD

		//allow the scripts to call request multiple times without asserting.
		if(ScriptId !=0)
		{
			if(ScriptId == m_ScriptThread)
			{
				return;
			}
		}

		if (Verifyf(!m_bIsCutSceneActive, "Cutscene: %s is already active, must be stopped before starting a new cutscene", m_CutSceneHashString.GetCStr()))
		{
			//Check that we have a valid slot
			if (Verifyf(g_CutSceneStore.FindSlot(pFileName) != -1, "Cut scene %s doesn't exist, and will not be loaded", pFileName ))
			{
				cutsceneManagerDebugf1("RequestCutscene: filename - %s  playNow - %d  fadeOutGameAtBeginning - %d  fadeInCutsceneAtBeginning - %d  fadeOutCutsceneAtEnd  - %d fadeInGameAtEnd - %d  ScriptId - %d  sPlayBackContextFlags - %d", pFileName, bPlayNow, fadeOutGameAtBeginning, fadeInCutsceneAtBeginning, fadeOutCutsceneAtEnd, fadeInGameAtEnd, ScriptId, sPlayBackContextFlags);

				SetPlaybackFlags(sPlayBackContextFlags);

				//starting a new cut scene reset all member variables
				SetUpCutSceneData();

				if(GetPlayBackFlags().IsFlagSet(CUTSCENE_REQUESTED_FROM_WIDGET))
				{
					SetEnableReplayRecord(false);
				}

				//Set the game ready for a cut scene, disable radar, set player controls false etc
				if ( bPlayNow )
				{
					SetGameToCutSceneState();
				}
				else
				{
					m_bIsSeamless = true;
				}

				//store the script id so if the script terminates and the scene hasn't played yet but is loaded, the assets can be streamed out.
				m_ScriptThread = ScriptId;

#if __BANK
				if(m_ScriptThread > 0)
				{
					m_bStreamObjectsWhileSceneIsPlaying = true;
				}
#endif
				//store the hash of the cut scene

				m_CutSceneHashString.SetFromString(pFileName);

				AddScriptResource(ScriptId);

				m_bStreamedCutScene = !bPlayNow;

				//call the load on the cut scene
				Load(pFileName, NULL, bPlayNow, fadeOutGameAtBeginning, fadeInCutsceneAtBeginning, fadeOutCutsceneAtEnd, fadeInGameAtEnd);
			}
		}
}

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
bool CutSceneManager::IsSceneAuthorized(atHashString& CutSceneHashString)
{
	for(int i = 0; i < m_AuthorizedCutscenes.m_AuthorizedCutsceneList.GetCount(); i++)
	{	
		if(CutSceneHashString.GetHash() == m_AuthorizedCutscenes.m_AuthorizedCutsceneList[i].GetHash())
		{
			return true; 
		}	
	}

	return false; 
}


void CutSceneManager::UpdateAuthorization()
{
	m_RenderUnauthorizedWaterMark = false; 

#if __BANK
	if(IsDLCCutscene())
	{
		m_IsAuthorizedForPlayback = IsSceneAuthorized(m_CutSceneHashString); 
			}
	else
#endif // __BANK
	{
		m_IsAuthorizedForPlayback = true; 
		}

#if __BANK
	if(!m_IsAuthorizedForPlayback && m_bStartedFromWidget)
	{
		if(!m_IsAuthorizedForPlayback)
		{
			cutsceneDisplayf("WARNING CUTSCENE %s IS NOT AUTHORIZED, NO BUGS ARE VALID FOR THIS SCENE", m_CutSceneHashString.TryGetCStr()); 
}

		m_IsAuthorizedForPlayback = true; 

			if(!CFrameDump::GetBinkMode() && !CFrameDump::GetCutsceneMode())
			{
		m_RenderUnauthorizedWaterMark = true; 
	}
		}
#endif //__BANK
};
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////
//When a cut scene starts make sure that all member variables are reset

void CutSceneManager::SetUpCutSceneData()
{
	//once a cut scene has been started set true and only set false at end
	m_IsCutscenePlayingBack = false;
	m_bCameraWillCameraBlendBackToGame = false;
	m_bIsCutSceneActive = true;
	m_CutSceneHashString.Clear();
	m_bStreamedCutScene = false;
	SetDeleteAllRegisteredEntites(false);
#if __BANK
	m_bRenderHiddenObjects = false;
#endif
}



///////////////////////////////////////////////////////////////////////////////////////////////////
//Call a cleanup on the cut scene

void CutSceneManager::Clear()
{
	if (!gStreamingRequestList.IsRunFromScript())
	{
		gStreamingRequestList.Finish();
	}

#if __BANK
	//make sure we turn all objects back on before we clear up the cut scene
	m_ActivateMapObjectEditing = false;
	m_bRenderHiddenObjects = true;
	m_bActivateBlockingObjectEditing = false;
	m_FixupMapObject = false;
	m_fPlaybackRate = 1.0f;
	cOrignalSceneName.Clear();
	//render any objects we decided to hide on the way
	m_DebugManager.DisplayHiddenObject();
	//Reset the vehicle list.

	m_DebugManager.Clear();

#endif

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
	m_RenderUnauthorizedScreen = false; 
	m_RenderUnauthorizedWaterMark = false;
#endif

	// PURPOSE: Clears all of the data as both an initialization and cleanup step "Rage quote"
	cutsManager::Clear();
#if __BANK
		//PopulatePedVariationList();
		m_DebugManager.PopulatePedVariationEventList(NULL);
		m_DebugManager.PopulateVehicleVariationEventList(NULL);
		m_DebugManager.PopulateOverlayEventList(NULL);
		//m_DebugManager.m_CascadeBounds.PopulateSetShadowBoundsPropertyEventList(NULL);
		m_DebugManager.m_CascadeBounds.PopulateSetShadowBoundsEventList(NULL);


#endif
	if(g_CutSceneStore.HasObjectLoaded(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())) && GetCutfFile())
	{
		if (Verifyf(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash() != -1).Get(), "Cut scene doesn't exist, and will not be loaded"))
		{
			g_CutSceneStore.RemoveRef(strLocalIndex(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash())), REF_OTHER);
			g_CutSceneStore.ClearRequiredFlag(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash()).Get(),STRFLAG_CUTSCENE_REQUIRED);
			g_CutSceneStore.StreamingRemove(g_CutSceneStore.FindSlotFromHashKey(m_CutSceneHashString.GetHash()));
			SetCutfFile(NULL);
		}
	}
	Assertf (m_pAnimManager == NULL, "The pointer to the anim manager is valid this could lead to memory leak, object should be deleted and pointer nulled");
	Assertf (m_pAssetManger == NULL, "The pointer to the asset manager is valid this could lead to memory leak, object should be deleted and pointer nulled ");

	CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_SCENE, m_CutSceneHashString);
	Assertf(m_iScriptRefCount == 0, "There are still outstanding script resources for this cutscene!");
	m_iScriptRefCount = 0;

	//Reset the cut scene vars
	ResetCutSceneVars();

	if(m_bShouldRestoreCutsceneAtEnd)
	{
		ShutDown();
	}

#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD

	CPed *pLocalPlayer = CGameWorld::FindLocalPlayer();

	if(pLocalPlayer)
	{
		Vector3 StartPos(14.7f, 0.5f, 0.0f);
		pLocalPlayer->Teleport(StartPos, 0.0f, false);
	}
#endif

	m_bShouldRestoreCutsceneAtEnd = false;

	cutsceneManagerDebugf1("Cutscene has fully terminated");


#if ENABLE_CUTSCENE_TELEMETRY
	g_CutSceneLightTelemetryCollector.CutSceneStop();
#endif

}

///////////////////////////////////////////////////////////////////////////

const CEntity* CutSceneManager::GetCascadeShadowFocusEntity()
{
	if (GetCamEntity())
	{
		return GetCamEntity()->GetCascadeShadowFocusEntityForSeamlessExit();
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////
//Restore stuff that we saved before cutscene started

void CutSceneManager::SetCutSceneToGameState(bool forcePlayerControlOn)
{
	//MINIMAP
	CMiniMap::SetVisible(true);

	//VIEWPORT CHECK FUNCTIONALITY WITH CtotheE
	if (!GetOptionFlags().IsFlagSet(CUTSCENE_NO_WIDESCREEN_BORDERS))
	{
		gVpMan.SetWidescreenBorders(false,0);
	}

	SetAllowGameToPauseForStreaming(true);

	// CLEAR ANY SUBTITLES
	CSubtitleText::ClearMessage();

	// re-enable the player again:
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if (pPlayerPed)
	{

			pPlayerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true);
			cutsceneManagerVisibilityDebugf3("CutSceneManager::SetCutSceneToGameState: Showing player (%s)", pPlayerPed->GetModelName());

		if(forcePlayerControlOn)
		{
			SetPlayerControl(true); 
		}
	}

	CClock::Pause(false);
}


///////////////////////////////////////////////////////////////////////////
//Checks the cut scene is not in the idle state

bool CutSceneManager::IsRunning()
{
	return m_bPlayNow;
}

///////////////////////////////////////////////////////////////////////////
//Checks that the cut scene is streaming

bool CutSceneManager::IsStreaming()
{
	if (m_pAssetManger)
	{
		return m_pAssetManger->GetLoadState() == LOAD_STATE_STREAMING;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

void CutSceneManager::ResetCutSceneVars()
{
	m_fTime = 0.0f;
	m_fInternalTime = 0.0f;
	m_fDeltaTime = 0.0f;
	m_bStreamedCutScene = false;
	m_CutSceneHashString.Clear();
	m_bIsCutSceneActive = false;
	m_fValidFrameStep = false;
	m_bIsPlayerInScene = false;
	m_bIsSeamless = false;
	m_bRequestedScriptAssetsForEndOfScene = false;
	m_bAreScriptReservedEntitiesLoaded = false;
	m_bIsSeamlessSkipping = false;
	m_iPlayerObjectId = -1;
	m_SeamlessSkipState = SS_DEFAULT;
	sSeamlessTrigger.bScriptOveride = false;
	sSeamlessTrigger.fPlayerAngle = 0.0f;
	sSeamlessTrigger.fTriggerAngle = 0.0f;
	sSeamlessTrigger.fTriggerOrient = 0.0f;
	sSeamlessTrigger.vTriggerOffset = Vector3(0.0f, 0.0f, 0.0f);
	m_iNextLoadEvent = 0;
	m_iNextUnloadEvent = 0;
	m_iNextAudioLoadEvent = 0;
	m_uFrameCountOfFirstRegisterCall = 0;
	m_VehicleModelThePlayerExitsTheSceneIn = 0; 
	m_ObjectIdsReserevedForPostScene.Reset();
	m_bCutsceneWasSkipped = false;
	m_ValidConcatSectionFlags = INVALID_PLAYBACK_FLAG;
	m_PlaybackFlags = 0;
	m_ScriptThread = THREAD_INVALID;
	m_bCanVibratePad = false;
	m_bAllowCargenToUpdate = false;
	m_bCanUseMobilePhone = false;
	m_HasScriptOverridenFadeValues = false;
	m_CanSkipCutSceneInMultiplayerGame = false;
	m_bCanSkipScene = true;
	m_fSkipBlockedCameraAnimTagTime = -1.0f;
	m_bHasScriptRegisteredAnEntity = false;
	m_bShouldPausePlaybackForAudioLoad = false;
	m_bShouldStopNow = false;
	m_bApplyTargetSkipTime = false;
	m_fTargetSkipTime = 0.0f;
	m_fFinalAudioPlayEventTime = 0.0f;
	m_bCanStartCutscene = false;
	m_IsCutscenePlayingBack = false;
	m_bCameraWillCameraBlendBackToGame = false;
	m_bDelayTerminatingAFrame = false;
	m_bWasDelayedTerminating = false;
	m_bDeleteAllRegisteredEntites = false;
	m_bCanScriptSetupEntitiesPreUpdateLoading = false;
	m_bHasScriptSetupEntitiesPreUpdateLoadingFlagBeenSet = false;
	m_bCanScriptChangeEntitiesModel = false;
	m_bHasScriptChangeEntitiesModelFlagBeenSet = false;
	m_bFadeOnSeamlessSkip = true;
	m_bFailedToLoadBeforePlayWasRequested = false;
	m_bWasSingledStepped = false;
	m_IsResumingPlayBackFromSkipping = false;
	m_bHasCutThisFrame = false;
	m_bDisplayMiniMapThisUpdate = false;
	m_bAllowGameToPauseForStreaming = true;
	m_SkipFrame = 0;
	m_HasSetCutsceneToGameStatePostScene = false; 
	m_bUpdateCutsceneTimer = false; 
	m_bEnableReplayRecord = true;
	m_bReplayRecording = false;
	m_ShutDownMode = 0; 
	m_bShouldProcessEarlyCameraCutEventsForSinglePlayerExits = false; 
	m_bHasProccessedEarlyCutEventForSinglePlayerExits = false; 


#if __BANK
	m_DebugManager.Clear();
	m_bStartedFromWidget = false;
	m_bIsFinalApproved = false;
	m_bIsCutsceneAnimationApproved = false;
	m_bIsCutsceneCameraApproved = false;
	m_bIsCutsceneFacialApproved = false;
	m_bIsCutsceneLightingApproved = false;
	m_bIsCutsceneDOFApproved = false;
	m_fApprovalMessagesOnScreenTimer = 0.0f;
	m_ApprovedCutsceneList.m_ApprovalStatuses.Reset();
	m_IsDlcScene = false; 
	m_fCurrentAudioTime = 0.0f;
	m_fLastAudioTime = 0.0f;
	m_CanSaveLightAuthoringFile = false; 
	m_fCurrentAudioTime = 0.0f;
	m_fLastAudioTime = 0.0f;
	m_iExternalFrame = 0;
	
	if(m_pDataLightXml)
	{
		delete m_pDataLightXml; 
	}
	m_pDataLightXml = NULL; 

	ms_DebugState.Reset();
	m_AllowScrubbingToZero = true; 
	m_bSnapCameraToLights = false; 
#endif
}

//void CutSceneManager::RemoveIncorrectlyRegisteredScriptObjects()
//{
//	if(!m_bCutSceneUpdating && !m_bWaitingToFinish)
//	{
//		if(m_RegisteredGameEntities.GetCount()>0)
//		{
//			cutsceneAssertf(0,"Objects have been registered but the scene has not started, Removing all registerd objects");
//			ResetRegisteredGameEntities();
//		}
//	}
//}

void CutSceneManager::ForceFade()
{
	//something very wrong has happened we have no cutfile loaded but a play has been called and we are loading
	s32 iFadeTime = (s32) (GetDefaultFadeOutDuration() * 1000.0f);

	Color32 FadeColor = GetDefaultFadeColor();

	//this fixes a problem in the fade system where is does not deal with colours that have alphas less than 200
	FadeColor.SetAlpha(255);
	camInterface::FadeOut(iFadeTime, true, FadeColor);
}
///////////////////////////////////////////////////////////////////////////
//unload a pre-streamed scene because the script that called this has been terminated.
void CutSceneManager::CleanupTerminatedScriptCutSceneAssets()
{
	//Scene is active but not playing, if playing m_ScriptThread would be null
	if( m_bIsCutSceneActive && m_ScriptThread)
	{
		scrThread* pThread = scrThread::GetThread(m_ScriptThread);

		if(!pThread)
		{
			//clear the list of  registered entities
			m_ScriptThread = THREAD_INVALID;
			UnloadPreStreamedScene(THREAD_INVALID);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//Terminates the current scene.

void CutSceneManager::UnloadPreStreamedScene(scrThreadId ScriptId)
{
	//Don't allow this to be called if scene is not active at all.
	if (m_bIsCutSceneActive)
	{
		if(GetCutfFile())
		{
			if(!IsRunning())
			{
				if(ScriptId == m_ScriptThread)
				{
					TerminateLoadedOnlyScene();
				}
			}
		}
		else
		{
			Clear();
		}

		streamDisplayf("Cutscene prestreaming canceled - removing SRL");
		if (!gStreamingRequestList.IsRunFromScript())
		{
		gStreamingRequestList.Finish();
	}
}
}

void CutSceneManager::TerminateLoadedOnlyScene()
{
	if(m_bIsCutSceneActive && !IsRunning())
	{
		//tell all the entities to remove all the loading.
		DispatchEventToAllEntities(CUTSCENE_CANCEL_LOAD_EVENT);

		g_CutsceneAudioEntity.StopCutscene(true);

		//delete all the entity manager objects
		if(GetCutfFile())
		{
			const atArray<cutfObject*>& pObjectList = GetCutfFile()->GetObjectList();
			for ( int i = 0; i < pObjectList.GetCount(); ++i )
			{
				DeleteEntityObject( pObjectList[i] );
			}
		}

		//reset all the vars
		Clear();
	}
}
///////////////////////////////////////////////////////////////////////////
//At the start of the scene set the timer variables
void CutSceneManager::InitialiseCutSceneTimer()
{
	m_fDeltaTime = 0.0f;

#if __BANK
	m_iExternalFrame = 0;
#endif // __BANK
}


float CutSceneManager::AdjustTimeForBlockingTags(float DesiredTime)
{
	const CCutSceneCameraEntity* pCamEntity = GetCamEntity();

	const crClip* pClip =  m_pAnimManager->GetCameraAnimForTime( pCamEntity->GetCameraObject(), DesiredTime, this );

	if(pClip)
	{
		//return the end time here because the desired time is past the end of the scene. 
		//dont need to process any blocking tags
		if(DesiredTime >= m_fEndTime)
		{
			return m_fEndTime;
		}
		
		u32 section = GetSectionForTime(DesiredTime);

		float startSectionTime = GetSectionStartTime(section);

		float clampedCurrentTime = DesiredTime;

		clampedCurrentTime = Clamp(clampedCurrentTime, m_fStartTime, m_fEndTime);

		float CurrentAnimTime = clampedCurrentTime - startSectionTime;

		float BlockingTagTime = 0.0f; 
		
		BlockingTagTime = pClip->CalcBlockedTime(CurrentAnimTime);

		//cut scene time
		float blockAdjustedCutSceneTime = BlockingTagTime + startSectionTime;

		//For concat boundaries where the start time is in an invalid concat section then move the time to the end of the blocking tag
		if(blockAdjustedCutSceneTime != DesiredTime)
		{
			float TagStartTime = 0.0f;
			float TagEndTime = 0.0f;
			bool HasTag = false; 

			if(pClip)
			{
				const float duration = pClip->GetDuration();
				const float durationInv = 1.f / duration;

				static const crTag::Key TagKeyBlock = crTag::CalcKey("Block", 0xE433D77D);			
				crTagIterator it(*pClip->GetTags(), TagKeyBlock);
				while(*it)
				{
					const crTag* tagBlock = static_cast<const crTag*>(*it);

					float phase = BlockingTagTime * durationInv;

					if(tagBlock->Contains(phase))
		{
						//compute the times in cutscene times. 
						TagStartTime = (tagBlock->GetStart() * duration) + startSectionTime; 
						TagEndTime = (tagBlock->GetEnd() * duration) + startSectionTime;
						HasTag = true; 
						break; 
					}
					++it;
				}
		}

			//the start tag is in an invalid concat section so lets push forward, like having a 0 property blocking tag
			int concatSection = GetConcatSectionForTime(TagStartTime); 
			if(concatSection > -1 && HasTag)
			{
				if(!IsConcatSectionValidForPlayBack(concatSection))
				{
					blockAdjustedCutSceneTime = TagEndTime; 
				}
			}
		}

		if(DesiredTime != blockAdjustedCutSceneTime)
		{
			cutsceneManagerDebugf3("Aligning to blocking tag time from %f to %f", DesiredTime, blockAdjustedCutSceneTime);
		}

#if __BANK
		if(m_bUseExternalTimeStep && blockAdjustedCutSceneTime >= m_fEndTime)
		{
			if(pClip)
			{
				const float duration = pClip->GetDuration();
				const float durationInv = 1.f / duration;

				static const crTag::Key TagKeyBlock = crTag::CalcKey("Block", 0xE433D77D);			
				crTagIterator it(*pClip->GetTags(), TagKeyBlock);
				while(*it)
				{
					const crTag* tagBlock = static_cast<const crTag*>(*it);

					float phase = BlockingTagTime * durationInv;

					if(tagBlock->Contains(phase))
					{
						m_fEndTime = startSectionTime + ((tagBlock->GetStart() * duration) - 0.0001f); 
						break; 
					}
					++it;
				}
			}
		}
#endif
		return blockAdjustedCutSceneTime;
	}

	return DesiredTime;
}

///////////////////////////////////////////////////////////////////////////
//Applies the time step for the last frame and applies it this frame

void CutSceneManager::UpdateCutSceneTimer()
{
	if(m_bUpdateCutsceneTimer)
	{
		//cache the previous time
		m_fPreviousTime = GetTime();

#if __BANK
		if(IsCutscenePlayingBack() && m_bLoop && !m_bScrubbingFromLooping)
		{
			if(GetPlayBackState() == PLAY_STATE_BACKWARDS)
			{
				if(m_iCurrentFrameWithFrameRanges < m_iLoopStartFrame)
				{
					m_bScrubbingFromLooping = true;
					m_iCurrentFrameWithFrameRanges = m_iLoopEndFrame;
					BankFrameScrubbingCallback();
				}
			} 
			else
			{
				if(m_iCurrentFrameWithFrameRanges > m_iLoopEndFrame)
				{
					m_bScrubbingFromLooping = true;
					m_iCurrentFrameWithFrameRanges = m_iLoopStartFrame;
					BankFrameScrubbingCallback();
				}
			}
		}

	
#endif //__BANK

		//update the timer
#if !__FINAL
		m_PreviousPlayBackState = m_PlayBackState;

		switch(GetScrubbingState())
		{
		case SCRUBBING_STEPPING_FORWARDS:
		case SCRUBBING_STEPPING_BACKWARDS:
		case SCRUBBING_FORWARDS_FROM_TIME_LINE_BAR:
		case SCRUBBING_BACKWARDS_FROM_TIME_LINE_BAR:
		case SCRUBBING_PAUSING:
			{
				float ftime = float(m_SkipFrame) / CUTSCENE_FPS;

#if __BANK
				if(m_AllowScrubbingToZero)
				{
					ftime = Clamp(ftime, 0.0f, GetCutfFile()->GetTotalDuration());
				}
				else
				{
					ftime = Clamp(ftime, 0.00001f, GetCutfFile()->GetTotalDuration());
				}
#endif // __BANK
				SetInternalTime(ftime);
			}
			break;

		case SCRUBBING_FAST_FORWARDING:
		case SCRUBBING_PLAYING_BACKWARDS:
		case SCRUBBING_REWINDING:
			{
				float ftime = GetInternalTime();
				
				ftime += fwTimer::GetTimeStep_ScaledNonClipped() * m_fPlaybackRate;
#if __BANK
				if(m_AllowScrubbingToZero)
				{
					ftime = Clamp(ftime, 0.0f, GetCutfFile()->GetTotalDuration());
				}
				else
				{
					ftime = Clamp(ftime, 0.00001f, GetCutfFile()->GetTotalDuration());
				}
#endif // __BANK

				SetInternalTime(ftime);

			}
			break;

		case SCRUBBING_NONE:
			{
				UpdateTime();
			}
			break;

		default:
			{
				UpdateTime();
			}

		}
#else //
		UpdateTime();
#endif //  !__FINAL

		// Apply any time change due to entering blocking tags
		float BlockAdjustedTime = AdjustTimeForBlockingTags(m_fInternalTime);

		// sanity clamp to scene end
		BlockAdjustedTime = (Clamp(BlockAdjustedTime, m_fStartTime, m_fEndTime));

		// Handle the case where we hit a blocking tag (moving forward in time) and it's moved us into the next concat section in time
		// but this section is not valid for playback, we need to jump a further on section.
		if (BlockAdjustedTime > m_fInternalTime && IsConcatted())
		{
			s32 newSection = GetConcatSectionForTime(BlockAdjustedTime);
			if (newSection != -1 && !(IsConcatSectionValid(newSection) && IsConcatSectionValidForPlayBack(newSection)))
			{
				s32 nextValidSection = GetNextValidConcatSection(newSection);
				if (nextValidSection != -1)
				{
					float fNextValidStartTime = GetConcatSectionStartTime(nextValidSection);
					BlockAdjustedTime = AdjustTimeForBlockingTags(fNextValidStartTime);
					cutsceneDisplayf("Gone into invalid section %d. Adjusting to section %d time %f ", newSection, nextValidSection, BlockAdjustedTime ); 
				}
			}
		}

#if __BANK
		if(m_HoldAtEnd && !m_bCutsceneWasSkipped && GetPlayBackState() == PLAY_STATE_FORWARDS_NORMAL_SPEED && BlockAdjustedTime >= GetEndTime())
		{
			BankPauseCallback(); 
		}
		else
#endif
		{
			cutsceneDisplayf("Updating cutscene time. finalTime: %.4f", BlockAdjustedTime);
			// Update the time
		SetTime(BlockAdjustedTime);
		}

		// Update the section
		if (IsConcatted())
		{
			u32 CurrentSection = GetConcatSectionForTime(GetTime());
			if(IsConcatSectionValid(CurrentSection) && IsConcatSectionValidForPlayBack(CurrentSection))
			{
				m_iCurrentConcatSection = CurrentSection;
			}
		}

		//update the section info for jumps
		UpdateSectionInfo();
	}

#if !__NO_OUTPUT
	//update the frame count
	if(GetCutfFile())
	{
		m_iCurrentFrame = u32(rage::round(GetTime()*CUTSCENE_FPS));
		BANK_ONLY(m_iCurrentFrameWithFrameRanges = m_iCurrentFrame + GetCutfFile()->GetRangeStart();)
		BANK_ONLY(m_fCurrentPhase = GetCutSceneCurrentTime() /  GetCutfFile()->GetTotalDuration();)
	}

#endif //!__NO_OUTPUT

#if __BANK
	if(GetCutfFile())
	{
		if(m_uRenderMBFrame[0] == static_cast< u32 >(-1))
		{
			m_uRenderMBFrame[0] = GetMBFrame();
		}
		else
		{
			m_uRenderMBFrame[1] = GetMBFrame();
		}
	}
#endif //__BANK
}

void CutSceneManager::UpdateTime()
{
	bool useAudioTime = true;

	cutsceneDisplayf("Updating cutscene time. state: %d, initalTime: %.4f, IsPaused:%s, WasSkipped:%s", m_cutsceneState, m_fInternalTime, IsPaused() ? "yes" : "no", WasSkipped() ? "yes" : "no");

	if(WasSkipped())
	{
		if(m_cutsceneState == CUTSCENE_LOADING_BEFORE_RESUMING_STATE || m_cutsceneState == CUTSCENE_SKIPPING_STATE)
		{
			m_fInternalTime = (float)(m_SkipFrame) / CUTSCENE_FPS;
			useAudioTime = false;
			m_IsResumingPlayBackFromSkipping = true;
			cutsceneDebugf1("Skipping to time %.3f. State:%d", m_fInternalTime, m_cutsceneState);
		}
		else
		{
			if(IsPlaying())
			{
				if(m_IsResumingPlayBackFromSkipping)
				{
					m_fInternalTime = (float)(m_SkipFrame) / CUTSCENE_FPS;
					m_IsResumingPlayBackFromSkipping = false;
					cutsceneDebugf1("Resuming from skipping to time %.3f.", m_fInternalTime);
					useAudioTime = false;
				}
				else
				{
#if !__NO_OUTPUT
					if (fwTimer::GetTimeStep_ScaledNonClipped()<=0.0f && !IsPaused())
					{
						cutsceneWarningf("Cutscene is using the game time step instead of audio (post skipping) but the timer isn't progressing. If this continues for a number of frames it could indicate a bug. timeStep:%.4f", fwTimer::GetTimeStep_ScaledNonClipped());
					}
#endif // !__NO_OUTPUT
					m_fInternalTime += fwTimer::GetTimeStep_ScaledNonClipped();
					useAudioTime = false;
				}
			}
		}
	}

	if(useAudioTime)
	{
		if(FindAudioObjectId() == -1 || fwTimer::GetSingleStepThisFrame() BANK_ONLY(||  m_bUseExternalTimeStep))
		{
#if __BANK			
			if(m_bUseExternalTimeStep)

			{
				m_fInternalTime = (m_iExternalFrame * m_ExternalTimeStep);
				m_iExternalFrame ++;
#if !__NO_OUTPUT
				if (m_fInternalTime<=m_fPreviousTime && !IsPaused())
				{
					cutsceneWarningf("Cutscene timer isn't progressing as the external timestep is holding us back! If this continues for a number of frames it could indicate a bug.");
				}
#endif // !__NO_OUTPUT
			}
			else
#endif
		{
			m_fInternalTime += fwTimer::GetTimeStep_ScaledNonClipped();
#if !__NO_OUTPUT
			if (fwTimer::GetTimeStep_ScaledNonClipped()<=0.0f && !IsPaused())
			{
				cutsceneWarningf("Cutscene is using the game time step instead of audio (Due to no audio object or single stepping) but the timer isn't progressing. If this continues for a number of frames it could indicate a bug. timeStep:%.4f, AudioObjectId:%d, Stepping:%s", fwTimer::GetTimeStep_ScaledNonClipped(), FindAudioObjectId(), fwTimer::GetSingleStepThisFrame() ? "yes" : "no");
			}
#endif // !__NO_OUTPUT
			}
	
#if __BANK
			if(m_fInternalTime < GetEndTime())
			{
				s32 section = GetConcatSectionForTime(m_fInternalTime);

				if(IsConcatSectionValid(section) && !IsConcatSectionValidForPlayBack(section))	
				{
					for(int i = section + 1; i < m_concatDataList.GetCount(); i++)
					{
						if(IsConcatSectionValidForPlayBack(i))
						{
							m_fInternalTime = m_concatDataList[i].fStartTime;
#if !__NO_OUTPUT
							if (m_fInternalTime<=m_fPreviousTime && !IsPaused())
							{
								cutsceneWarningf("Cutscene timer isn't progressing as the code to move to the next concat section is holding us back! If this continues for a number of frames it could indicate a bug.");
							}
#endif // !__NO_OUTPUT
							break; 
						}
					}
				}
			}
#endif // __BANK
		}
		else
		{
			float newTime = g_CutsceneAudioEntity.GetCutsceneTime()/1000.f;

#if __BANK
			m_fLastAudioTime = m_fCurrentAudioTime;
			m_fCurrentAudioTime = newTime;
			cutsceneAssertf(m_fCurrentAudioTime>=m_fLastAudioTime || IsClose(m_fCurrentAudioTime, m_fLastAudioTime, 0.015f), "Audio time has gone backwards! Last time=%.3f, new time=%.3f", m_fLastAudioTime, m_fCurrentAudioTime);
			//cutsceneAssertf(newTime>=m_fInternalTime || IsClose(newTime, m_fInternalTime, 0.015f), "Cutscene adjusted time has gone backwards! Last time=%.3f, new time=%.3f, Last audio time=%.3f ", m_fInternalTime, newTime, m_fLastAudioTime);
#endif // __BANK
#if !__NO_OUTPUT
			if (newTime<=m_fInternalTime && !IsPaused())
			{
				cutsceneManagerWarningf("Audio time isn't progressing. If this continues for a number of frames it could indicate a bug. newTime:%.4f, lastTime:%.4f", newTime, m_fInternalTime);
			}			
#endif // !__NO_OUTPUT
			m_fInternalTime = newTime;

			// if the new time is in an invalid section, clamp to the last valid section.
			s32 section = GetConcatSectionForTime(m_fInternalTime);
			if (section>=0 && !(IsConcatSectionValid(section) && IsConcatSectionValidForPlayBack(section)))
			{
				cutsceneManagerErrorf("Audio returned a time in an invalid concat section! newTime:%.6f. Clamping to the end of the last valid section...", m_fInternalTime);
				s32 lastValidSection = GetPreviousValidConcatSection(section);
				if (lastValidSection>=0)
				{
					m_fInternalTime = GetConcatSectionStartTime(lastValidSection) + GetConcatSectionDuration(lastValidSection) - SMALL_FLOAT;
				}
			}
		}

		//Dont allow the internal time go backwards 
		m_fInternalTime = Max(m_fInternalTime, m_fPreviousTime);
		m_fPlayTime = GetPlayTime(m_fInternalTime);
	}
};

float CutSceneManager::GetPlayTime(float sceneTime)
{
	if(IsConcatDataValid())
	{
		s32 sceneSection = GetConcatSectionForTime(sceneTime);
		float playTime = 0.0f;
		for (s32 i=0; i<m_concatDataList.GetCount() && i<sceneSection; i++)
		{
			if (IsConcatSectionValidForPlayBack(i))
			{
				playTime+=GetConcatSectionDuration(i);
			}
		}
		playTime+=sceneTime - GetConcatSectionStartTime(sceneSection);
		return playTime;
	}
	return sceneTime; 
}

void CutSceneManager::UpdateEventsForInvalidSections(float NewTime)
{
	if (IsConcatted())
	{
		//need the ability to jump an arbitrary section without dispatching those events
		//so we will need to update the event index before dispatching new events
		u32 CurrentSection = GetConcatSectionForTime(NewTime);
		float sectionStartTime = GetConcatSectionStartTime(CurrentSection);

		const atArray<cutfEvent*>& pEventList = GetCutfFile()->GetEventList();
		while ( (m_iNextEventIndex < pEventList.GetCount()) && pEventList[m_iNextEventIndex]->GetTime() < sectionStartTime )
		{
			++m_iNextEventIndex;
		}

		if(IsConcatSectionValid(CurrentSection) && IsConcatSectionValidForPlayBack(CurrentSection))
		{
			m_iCurrentConcatSection = CurrentSection;
		}
		else
		{
			cutsceneAssertf(0, "Audio has jumped to an invalid concat section. Time: %.6f, New concat section: %d, current section: %d", NewTime, CurrentSection, m_iCurrentConcatSection );
		}	
	}

	//	if(
	//	{
	//		float TargetTime = 0.0f;
	//		bool ValidTarget = false;
	//		//look through the next sections and until we find a valid one
	//		if (CurrentSection ==  (u32)m_concatDataList.GetCount() - 1 )
	//		{
	//			StopCutsceneAndDontProgressAnim();
	//			//cutsceneAssertf(0, "We are in the final concat section and its invalid for play back we need to terminate now");
	//		}
	//		else
	//		{
	//			for(int i = CurrentSection + 1; i < m_concatDataList.GetCount(); i++)
	//			{
	//				if(IsConcatSectionValidForPlayBack(i))
	//				{
	//					TargetTime = m_concatDataList[i].fStartTime;
	//					ValidTarget = true;
	//					m_iCurrentConcatSection = i;
	//					break;
	//				}
	//			}
	//		}
	//
	//
	//		if(ValidTarget)
	//		{
	//			//update the events so that they are ready for the play state.
	//

	//			//we need to skip the time onwards immediately this section is invalid for play back
	//			m_bApplyTargetSkipTime = true;
	//
	//			//float DeltaTime = NewTime - m_concatDataList[CurrentSection].fStartTime;
	//			m_fTargetSkipTime = TargetTime; // + DeltaTime;
	//

	//
	//		}
	//	}
	//}
}


void CutSceneManager::UpdateSkip()
{
	switch(m_SeamlessSkipState)
	{
	case SS_SET_SEAMLESS_SKIP:
		{
			if(!m_bFadedOutCutsceneAtEnd && !IsSkippingPlayback() && !m_bShouldStopNow)	//only set if not fading
			{
				SetIsSkipping(true);
	#if __BANK
				if(GetCutfFile())
				{
					if(m_bFadeOnSeamlessSkip && !camInterface::IsFadedOut() && !camInterface::IsFadingOut())
					{
						FadeOutCutsceneAtEnd( FORCE_FADE, true );
					}
				}
				else
				{
					ForceFade();
				}
	#else
				FadeOutCutsceneAtEnd( FORCE_FADE, true );	//fade out screen

	#endif
				m_bFadedOutCutsceneAtEnd = true;
				m_cutsceneState = CUTSCENE_FADING_OUT_STATE;
			}
			m_SeamlessSkipState = SS_DEFAULT;
		}
		break;

	case SS_STOP_CUTSCENE_NOW:
		{
			m_bShouldStopNow = true;
			m_SeamlessSkipState = SS_DEFAULT;
		}
		break;

	case SS_DEFAULT:
		{
		}
		break;
	}
}



///////////////////////////////////////////////////////////////////////////
//add the ability to skip a cut scene
void CutSceneManager::TriggerCutsceneSkip(bool bForceSkip)
{
	if (IsCutscenePlayingBack())
	{
		if(!bForceSkip)
		{
			float fStartTime = GetStartTime();
			float fEndTime = GetEndTime();

			if (GetCutSceneCurrentTime() < (fStartTime + MIN_CUTSCENE_SKIP_TIME)  || GetCutSceneCurrentTime() >= fEndTime - GetCutfFile()->GetFadeOutCutsceneAtEndDuration()  )//&& GetCutSceneCurrentTime() < m_end)
			{
				return;
			}
		}

		bool bSkip = false;

		bSkip = bForceSkip;

		if(NetworkInterface::IsGameInProgress())
		{
			bSkip = m_CanSkipCutSceneInMultiplayerGame;
		}
		else
		{
			// We pass false into GetMainPlayerControl() in case player controls are disabled but we still need to read the input.
			if( !CPauseMenu::IsActive() && CControlMgr::GetMainPlayerControl(false).GetCustsceneSkip().IsPressed() && !CPhoneMgr::IsDisplayed() && m_bCanSkipScene && !IsSkipBlockedByCameraAnimTag())
			{
				bSkip = true;
			}
		}

		if(bSkip && !m_bCutsceneWasSkipped)
		{
			m_bCutsceneWasSkipped = true;

			m_bFadeOnSeamlessSkip = true;

			m_SeamlessSkipState = SS_SET_SEAMLESS_SKIP;

			SetSkipFrame(m_iEndFrame - 1);

			CNetworkTelemetry::CutSceneSkipped(atStringHash(GetCutsceneName()), (u32)(GetCutSceneCurrentTime() * 1000.0f));
		}

		UpdateSkip();
	}
}

void CutSceneManager::StopCutsceneAndDontProgressAnim()
{
	m_SeamlessSkipState = SS_STOP_CUTSCENE_NOW;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::StartCutscene()
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if(pPlayerPed && !pPlayerPed->IsInjured())
	{
		if(m_bCanStartCutscene)
		{
			if (cutsManager::IsRunning())
			{
				if(!cutsManager::IsLoading())
				{
					if(!IsPlaying())
					{
#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK		
						SetGameToCutSceneState();

						if(m_IsAuthorizedForPlayback)
						{
						Play();

#if ENABLE_CUTSCENE_TELEMETRY
						g_CutSceneLightTelemetryCollector.CutSceneStart(GetCutsceneName(), m_bIsCutsceneCameraApproved, m_bIsCutsceneLightingApproved);
#endif //ENABLE_CUTSCENE_TELEMETRY
							m_bPlayNow = true;
						}
						else
						{
							m_cutsceneState = CUTSCENE_AUTHORIZED_STATE; 
						}
#else
						//set the cut scene state for the start of the scene 
						SetGameToCutSceneState();

						m_bPlayNow = true;

						Play();

#if ENABLE_CUTSCENE_TELEMETRY
						g_CutSceneLightTelemetryCollector.CutSceneStart(GetCutsceneName(), m_bIsCutsceneCameraApproved, m_bIsCutsceneLightingApproved);
#endif //ENABLE_CUTSCENE_TELEMETRY
#endif //CUTSCENE_AUTHORIZED_FOR_PLAYBACK
					}
				}
				else
				{
					//pre-streaming has failed we must fade because the script has called play but we are not ready yet
					if(GetPlayBackFlags().IsFlagSet(CUTSCENE_REQUESTED_IN_MISSION))
					{
						cutsceneAssertf(0, "Cutscene %s started before it has loaded properly, script must wait for a scene to be loaded to avoid fading", GetCutsceneName());
					}

					SetGameToCutSceneState();
					m_fadeInCutsceneAtBeginning = FORCE_FADE;
					m_cutsceneState = CUTSCENE_FADING_OUT_STATE;

					//play soon as when we finish loading
					if(GetCutfFile())
					{					
						cutsManager::FadeOutGameAtBeginning( FORCE_FADE );
						if(GetAssetManager())
						{
							GetAssetManager()->ResetStreamingTime();
						}
					}
					else
					{
						ForceFade();
					}

					m_bPlayNow = true;
					m_bFailedToLoadBeforePlayWasRequested = true;
				}
				m_bCanStartCutscene = false;
			}
		}
	}
}


void CutSceneManager::PlaySeamlessCutScene(scrThreadId ScriptId, u32 OptionFlags)
{
	if(ScriptId != m_ScriptThread && m_bCanStartCutscene == false)
	{
		const char* pScriptName = NULL;
		scrThread* pThread = scrThread::GetThread(m_ScriptThread);
		if(pThread)
		{
			pScriptName = pThread->GetScriptName();
		}
		cutsceneAssertf(ScriptId == m_ScriptThread, "Script %s (%d) has already loaded cutscene %s. This Script cannot start a scene loaded by another script, unless allowed" ,pScriptName,m_ScriptThread, m_CutSceneHashString.GetCStr());

	}

	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if(pPlayerPed && !pPlayerPed->IsInjured())
	{
		if(ScriptId == m_ScriptThread)
		{
			m_OptionFlags.SetAllFlags(OptionFlags);
			m_bCanStartCutscene = true;
			
			AddScriptResource(ScriptId);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::LoadCutSceneSectionMapCollision(bool bLoad)
{
	if (bLoad)
	{
		g_SceneStreamerMgr.LoadScene(m_vSceneOffset);

	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Work out which type of to create and also set the model index, depending if the object has been streamed in or not.

cutsEntity*	CutSceneManager::ReserveEntity(const cutfObject* pObject)
{
	if (!pObject)
	{
		//Add assert here
		return NULL;
	}

	cutsEntity* pEntity = NULL;

	//associate the object on the rage side derived from the data with a entity/delegate on the game side, will need to write an entity/delegate class
	// per instance

	switch ( pObject->GetType() )
	{
	case CUTSCENE_ASSET_MANAGER_OBJECT_TYPE:
		{
			m_pAssetManger = rage_new CCutSceneAssetMgrEntity( pObject );
			pEntity = m_pAssetManger;
		}
		break;

	case CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE:
		{
			m_pAnimManager = rage_new CCutSceneAnimMgrEntity ( pObject );
			pEntity = m_pAnimManager;
		}
		break;

	case CUTSCENE_MODEL_OBJECT_TYPE:
		{
			//We have no asset manager so we cant stream any game objects
			if (!m_pAssetManger)
			{
				Assertf(0, "Trying to load assests but there is no assest manager to have streamed our object" );
				return NULL;
			}

			const cutfModelObject* pModelObject = static_cast<const cutfModelObject*>(pObject);

			if (pModelObject)
			{
				switch (pModelObject->GetModelType())
				{
				case CUTSCENE_PED_MODEL_TYPE:
					{
						pEntity = rage_new CCutsceneAnimatedActorEntity(pObject);
					}
					break;

				case CUTSCENE_VEHICLE_MODEL_TYPE:
					{
						pEntity = rage_new CCutsceneAnimatedVehicleEntity(pObject);
					}
					break;

				case CUTSCENE_PROP_MODEL_TYPE:
					{
						pEntity = rage_new CCutSceneAnimatedPropEntity(pObject);
					}
					break;
				case CUTSCENE_WEAPON_MODEL_TYPE:
					{
						pEntity = rage_new CCutSceneAnimatedWeaponEntity(pObject);
					}
					break;

				default:
					{

					}
					break;
				}
			}
		}
		break;

	case CUTSCENE_CAMERA_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneCameraEntity(pObject);
		}
		break;

	case CUTSCENE_AUDIO_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneAudioEntity(pObject);
		}
		break;

	case CUTSCENE_FIXUP_MODEL_OBJECT_TYPE:
		{
			pEntity = rage_new  CCutSceneFixupBoundsEntity(pObject);
		}
		break;

	case CUTSCENE_LIGHT_OBJECT_TYPE:
	case CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE:
		{
			 pEntity = rage_new CCutSceneLightEntity(pObject);
		}
		break;

	case CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE:
	case CUTSCENE_PARTICLE_EFFECT_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneParticleEffectsEntity(pObject);
		}
		break;

	case CUTSCENE_SUBTITLE_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneSubtitleEntity(pObject);
		}
		break;

	case CUTSCENE_SCREEN_FADE_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneFadeEntity(pObject->GetObjectId(),  pObject );
		}
		break;

	case CUTSCENE_BLOCKING_BOUNDS_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneBlockingBoundsEntity(pObject);
		}
		break;

	case CUTSCENE_REMOVAL_BOUNDS_OBJECT_TYPE:
		{
			cutsceneAssertf(0, "CUTSCENE_REMOVAL_BOUNDS_OBJECT_TYPE not supported");
		}
		break;

	case CUTSCENE_HIDDEN_MODEL_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneHiddenBoundsEntity(pObject);
		}
		break;

	case CUTSCENE_OVERLAY_OBJECT_TYPE:
		{
			const cutfOverlayObject* pOverlayObject = static_cast<const cutfOverlayObject*>(pObject);

			if(pOverlayObject->GetOverlayType() == CUTSCENE_SCALEFORM_OVERLAY_TYPE)
			{
				pEntity = rage_new CCutSceneScaleformOverlayEntity(pObject);
			}
			else if(pOverlayObject->GetOverlayType() == CUTSCENE_BINK_OVERLAY_TYPE)
			{
				pEntity = rage_new CCutSceneBinkOverlayEntity(pObject);
			}
		}
		break;

	case CUTSCENE_RAYFIRE_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneRayFireEntity(pObject);
		}
		break;

	case CUTSCENE_DECAL_OBJECT_TYPE:
		{
			pEntity = rage_new CCutSceneDecalEntity(pObject);
		}
		break;
	default:
		break;
	}

	if(pEntity)
	{
		if(pObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
		{
#if !__NO_OUTPUT
			const cutfModelObject* pModel = static_cast<const cutfModelObject*>(pObject);
#endif // !__NO_OUTPUT
			cutsceneManagerDebugf2("Created %s: %s with Scene Handle: %s", pObject->GetTypeName(), pObject->GetDisplayName().c_str(), pModel->GetHandle().TryGetCStr());
		}
		else
		{
			cutsceneManagerDebugf2("Created '%s' entity for object '%s'.", pObject->GetTypeName(), pObject->GetDisplayName().c_str());
		}
	}
	else
	{
		cutsceneAssertf(0, "Object of type '%s' not supported.", pObject->GetTypeName());
	}

	return pEntity;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Release the entities and delete the game side object.

void CutSceneManager::ReleaseEntity(cutsEntity* pEntity, const cutfObject* pObject)
{
	if ( !pEntity )
	{
		cutsceneDebugf1( "cutsEntity is NULL.  Unable to release entity." );
		return;
	}

	if ( !pObject )
	{
		cutsceneDebugf1(  "cutfObject is NULL.  Unable to release entity." );
		return;
	}

	switch ( pEntity->GetType() )
	{
	case CUTSCENE_SINGLETON_ENTITY_TYPE:
		{
			//make sure that the type checking is correct
			cutsSingletonEntity* pSingletonEntity = dynamic_cast<cutsSingletonEntity*>( pEntity );
			if (pSingletonEntity)
			{
				pSingletonEntity->RemoveObject( pObject );

				if ( pSingletonEntity->GetObjectList().GetCount() == 0 )
				{
					delete pSingletonEntity;
				}
			}
		}
		break;
	default:
		{
			//Null the cut scene managers pointer to the anim manager
			if (pEntity == m_pAnimManager)
			{
				m_pAnimManager = NULL;
			}

			//Null the cut scene managers pointer to the asset manager
			if (pEntity == m_pAssetManger)
			{
				m_pAssetManger = NULL;
			}

			delete pEntity;
			pEntity = NULL;
		}
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Patches the skeleton data for the cut scene objects

void CutSceneManager::GetInteriorInfo(bool bForceLoading)
{
	//	NEED TO IMPLEMENT BACK IN
	if(camInterface::GetCutsceneDirector().GetJumpCutStatus() == camAnimatedCamera::POST_JUMP_CUT)
	{
		Vector3 camStartPos = VEC3_ZERO;
		if (camInterface::GetCutsceneDirector().GetCameraPosition(camStartPos))
		{
			CInteriorInst* pIntInst = NULL;
			s32	roomIdx = -1;
			CPortalTracker::ProbeForInterior(camStartPos, pIntInst, roomIdx, NULL, CPortalTracker::NEAR_PROBE_DIST);


			if (pIntInst && (m_pInteriorProxy != pIntInst->GetProxy()) && roomIdx >= 0)
			{
				m_pInteriorProxy = pIntInst->GetProxy();
				pIntInst->RequestRoom(0, STRFLAG_PRIORITY_LOAD);
				pIntInst->RequestRoom(roomIdx, STRFLAG_PRIORITY_LOAD);

				if (bForceLoading)
				{
					CStreaming::LoadAllRequestedObjects();
					pIntInst->ForceFadeIn();
				}
			}
		}
	}
}


void CutSceneManager::SetGameToCutSceneStatePostScene()
{
	if(!m_HasSetCutsceneToGameStatePostScene)
	{
		//Fire manager
		g_fireMan.ExtinguishAll(true);
		m_HasSetCutsceneToGameStatePostScene = true; 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Purpose: Responsible for the general state of the cut scene before we start playing
//Player control, Hud, Player health etc etc
void CutSceneManager::SetGameToCutSceneState()
{
	cutsceneManagerDebugf3("SetGameToCutSceneState");
	
	//decide if the player is in the scene, do at the start event as the player can change at any point
	if (!NetworkInterface::IsGameInProgress())
	SetPlayerIsInScene();

	//Setup the trigger heading
	float newHeading = m_fSceneRotation - (sSeamlessTrigger.fPlayerAngle * RtoD) + (sSeamlessTrigger.fTriggerOrient * RtoD);

	if (newHeading < 0.0)
	{
		newHeading = newHeading + 360.0f;
	}
	else
	{
		if (newHeading > 360.0f)
		{
			newHeading = newHeading - 360.0f;
		}
	}

	this->SetNewStartHeading(newHeading);


	//may remove this

	//Hud and MINIMAP
	if (!GetDisplayMiniMapThisUpdate())
	{
		CMiniMap::SetVisible(false);
	}

	SetAllowGameToPauseForStreaming(false);

//	CHud::SetUniqueDisplay(CHud::HudData[HUD_MISSION_NAME].iItemId, 1);

	// switch off any existing subtitles:
	CMessages::ClearMessages();

// 	// fade off the mission title if its there
// 	if (CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->IsActive())
// 	{
// 		CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->SetActive(false);  // switch off (it should fade out as the scene fades in)
// 	}

	CMessages::ClearMessages();  // clear any messages in the queue

	if (!GetOptionFlags().IsFlagSet(CUTSCENE_NO_WIDESCREEN_BORDERS))
	{
		gVpMan.SetWidescreenBorders(true,0);
	}

	//Player
	SetPlayerControl(false); 

	//NEED TO STORE THE NAME OF THE SCENE
	//CPlayStats::CutSceneStarted(ms_cName);

	//Pad
	if(CControlMgr::GetPlayerPad())
		CControlMgr::GetPlayerPad()->Clear();
	CControlMgr::StopPadsShaking();  // stop pads shaking



	//Time
	m_GameTimeHours = CClock::GetHour();
	m_GameTimeMinutes = CClock::GetMinute();
	m_GameTimeSeconds = CClock::GetSecond();

	CClock::Pause(true);

#if __BANK
	m_DebugManager.m_fTimeCycleOverrideTime = m_GameTimeHours*60;
	m_DebugManager.m_fTimeCycleOverrideTime += m_GameTimeMinutes;
#endif
}

//////////////////////////////////////////////////////////////////////////

grcRasterizerStateHandle	CutSceneManager::ms_LetterBoxPrevRSHandle = grcStateBlock::RS_Invalid;
grcDepthStencilStateHandle	CutSceneManager::ms_LetterBoxPrevDSSHandle = grcStateBlock::DSS_Invalid;
grcBlendStateHandle			CutSceneManager::ms_LetterBoxPrevBSHandle = grcStateBlock::BS_Invalid;

void CutSceneManager::LetterBoxRenderingPrologue()
{
	// Just keep whatever render state is now active after we render the letterbox...
	ms_LetterBoxPrevBSHandle = grcStateBlock::BS_Active;
	ms_LetterBoxPrevDSSHandle = grcStateBlock::DSS_Active;
	ms_LetterBoxPrevRSHandle = grcStateBlock::RS_Active;

	// ...And render the letterbox without testing nor writing depth, which happens to be invalid
	// (i.e.: depth/stencil buffer might have random stuff on it...) when the game is paused.
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
}

void CutSceneManager::LetterBoxRenderingEpilogue()
{
	// Set back the previous render states - ideally we'd set them to default states -, but just to avoid changing too much
	grcStateBlock::SetStates(ms_LetterBoxPrevRSHandle, ms_LetterBoxPrevDSSHandle, ms_LetterBoxPrevBSHandle);
}

//////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::RenderCutsceneBorders()
{
	if (!CHudTools::GetWideScreen())
	{
		bool bBuildDrawList = gDrawListMgr->IsBuildingDrawList();

#define SIZE_OF_LETTERBOX_BORDER	(0.125f)  // size of letterbox border

		if (bBuildDrawList)
		{
			DLC_Add(CutSceneManager::LetterBoxRenderingPrologue);
			DLC ( CDrawRectDC, ( fwRect(0.0f, 0.0f, 1.0f, SIZE_OF_LETTERBOX_BORDER), CRGBA(0,0,0, 255)) );
			DLC ( CDrawRectDC, ( fwRect(0.0f, (1.0f-SIZE_OF_LETTERBOX_BORDER), 1.0f, 1.0f), CRGBA(0,0,0, 255)) );
			DLC_Add(CutSceneManager::LetterBoxRenderingEpilogue);
		}
		else
		{
			CutSceneManager::LetterBoxRenderingPrologue();
			CSprite2d::DrawRect( fwRect(0.0f, 0.0f, 1.0f, SIZE_OF_LETTERBOX_BORDER), CRGBA(0,0,0, 255));
			CSprite2d::DrawRect( fwRect(0.0f, (1.0f-SIZE_OF_LETTERBOX_BORDER), 1.0f, 1.0f), CRGBA(0,0,0, 255));
			CutSceneManager::LetterBoxRenderingEpilogue();
		}
	}
}

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK	
void CutSceneManager::RenderAuthorizedForScriptScreen()
{
	if(!m_IsAuthorizedForPlayback && m_RenderUnauthorizedScreen)
	{
		CTextLayout CutsceneDebugText;
		char gxtDebugString[128];

		float scale = 0.5f; 

		formatf(gxtDebugString, 128, "%s : %s", CutSceneManager::GetInstance()->GetCutsceneName(), "Not Authorized For Playback");

		CutsceneDebugText.SetScale(Vector2(scale, scale)); 

		float strWidth = CutsceneDebugText.GetStringWidthOnScreen(gxtDebugString, true);
		float strHeight = CutsceneDebugText.GetCharacterHeight();

		Vector2 vTextPos = Vector2(0.5f - (strWidth/2.0f), 0.5f - (strHeight/2.0f));

		CutsceneDebugText.SetColor(CRGBA(255,255,255,255));
		CutsceneDebugText.SetDropShadow(true);

		CutsceneDebugText.Render(vTextPos, &gxtDebugString[0]);

		bool bBuildDrawList = gDrawListMgr->IsBuildingDrawList();

		if (bBuildDrawList)
		{
			DLC ( CDrawRectDC, ( fwRect(0.0f, 0.0f, 1.0f, 1.0f), CRGBA(0,0,0, 255)) );
		} 
		else
		{ 
			CSprite2d::DrawRect( fwRect(0.0f, 0.0f, 1.0f, 1.0f), CRGBA(0,0,0, 255));
		}
	}
}
#endif

void CutSceneManager::RenderOverlayToMainRenderTarget(bool bIsCutscenePlayingBack)
{
	SYS_CS_SYNC(sm_CutsceneLock);

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK	
	RenderAuthorizedForScriptScreen(); 
#if __BANK
	if(m_IsAuthorizedForPlayback && m_RenderUnauthorizedWaterMark)
	{
		RenderWatermark(); 
	}
#endif // __BANK
#endif

#if __BANK

	RenderCameraInfo(); 

#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD
	RenderContentRestrictedData(true);
#else

	RenderContentRestrictedData(false);

#if ALLOW_APPROVAL_RENDER
	bool RenderAuthorisedWaterMark = false; 

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK	
	RenderAuthorisedWaterMark = m_RenderUnauthorizedWaterMark; 
#endif //CUTSCENE_AUTHORIZED_FOR_PLAYBACK

	if(!PARAM_nocuttext.Get() && m_IsDlcScene && !RenderAuthorisedWaterMark)
	{
		// Only render approval messages for 10 seconds max.
		if (m_fApprovalMessagesOnScreenTimer <= 10.0f)
		{
			RenderUnapprovedSceneInfoCallback(m_bIsFinalApproved, m_bIsCutsceneAnimationApproved, m_bIsCutsceneCameraApproved, m_bIsCutsceneFacialApproved, m_bIsCutsceneLightingApproved, m_bIsCutsceneDOFApproved);
		}
		m_fApprovalMessagesOnScreenTimer += fwTimer::GetSystemTimeStep();
	}
	RenderStreamingPausedForAudio(m_bShouldPausePlaybackForAudioLoad);
#endif // ALLOW_APPROVAL_RENDER

#endif // SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD
#endif // __BANK

	//search the entity list and call the render function
	//can optimise by storing the entity list as a member variable and iterating
	if(bIsCutscenePlayingBack)
	{
		if(CutSceneManager::GetInstance()->m_cutsceneEntityObjects.GetNumUsed() > 0 )
		{
			atMap<s32,SEntityObject>::Iterator entry = CutSceneManager::GetInstance()->m_cutsceneEntityObjects.CreateIterator();
			for ( entry.Start(); !entry.AtEnd(); entry.Next() )
			{
				if (entry.GetData().pEntity && entry.GetData().pEntity->GetType() == CUTSCENE_SCALEFORM_OVERLAY_GAME_ENTITY)
				{
						const CCutSceneScaleformOverlayEntity* pOverlay = static_cast<const CCutSceneScaleformOverlayEntity*>(entry.GetData().pEntity);

						if(pOverlay && pOverlay->GetOverlayObject() && pOverlay->GetOverlayObject()->GetOverlayType() == CUTSCENE_SCALEFORM_OVERLAY_TYPE)
						{
							pOverlay->RenderOverlayToMainRenderTarget();
						}
				}
			}
		}

		RenderCutsceneBorders();
	}

#if USE_MULTIHEAD_FADE
	if (fade::uEndTime)
		CSprite2d::DrawMultiFade(!fade::bInstant);
#endif
}

void CutSceneManager::RenderBinkMovieAndUpdateRenderTargets()
{
	if(CutSceneManager::GetInstance()&& CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		if(CutSceneManager::GetInstance()->m_cutsceneEntityObjects.GetNumUsed() > 0 )
		{
			atMap<s32,SEntityObject>::Iterator entry = CutSceneManager::GetInstance()->m_cutsceneEntityObjects.CreateIterator();
			for ( entry.Start(); !entry.AtEnd(); entry.Next() )
			{
				if (entry.GetData().pEntity && entry.GetData().pEntity->GetType() == CUTSCENE_BINK_OVERLAY_GAME_ENTITY)
				{
					const CCutSceneBinkOverlayEntity* pOverlay = static_cast<const CCutSceneBinkOverlayEntity*>(entry.GetData().pEntity);

					if(pOverlay->GetOverlayObject()->GetOverlayType() == CUTSCENE_BINK_OVERLAY_TYPE)
					{
						pOverlay->RenderBinkMovie();
					}
				}
				else if (entry.GetData().pEntity && entry.GetData().pEntity->GetType() == CUTSCENE_SCALEFORM_OVERLAY_GAME_ENTITY)
				{
					const CCutSceneScaleformOverlayEntity* pOverlay = static_cast<const CCutSceneScaleformOverlayEntity*>(entry.GetData().pEntity);

					if (pOverlay->GetRenderId())
					{
						gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)pOverlay->GetRenderId());
					}
				}
			}
		}
	}
}

void CutSceneManager::RenderOverlayToRenderTarget(unsigned int targetId)
{
	SYS_CS_SYNC(sm_CutsceneLock);
	if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		if(CutSceneManager::GetInstance()->m_cutsceneEntityObjects.GetNumUsed() > 0 )
		{
			atMap<s32,SEntityObject>::Iterator entry = CutSceneManager::GetInstance()->m_cutsceneEntityObjects.CreateIterator();
			for ( entry.Start(); !entry.AtEnd(); entry.Next() )
			{
				if (entry.GetData().pEntity && entry.GetData().pEntity->GetType() == CUTSCENE_SCALEFORM_OVERLAY_GAME_ENTITY)
				{
					const CCutSceneScaleformOverlayEntity* pOverlay = static_cast<const CCutSceneScaleformOverlayEntity*>(entry.GetData().pEntity);

					// Render scaleform now if it's the same render target id
					if (pOverlay->GetRenderId() == targetId)
					{
						pOverlay->RenderOverlayToRenderTarget();
					}
				}
			}
		}
	}
}

void CutSceneManager::SetPlayerIsInScene()
{
	CPedModelInfo* pModelInfo =  CGameWorld::FindLocalPlayer()->GetPedModelInfo();

	if(pModelInfo)
	{
		atHashString playerHash(pModelInfo->GetHashKey());

		CCutsceneAnimatedModelEntity* pAnimEntity = GetAnimatedModelEntityFromModelHash(playerHash);

		if (pAnimEntity && pAnimEntity->GetCutfObject())
		{
			m_bIsPlayerInScene = true;

			m_iPlayerObjectId = pAnimEntity->GetCutfObject()->GetObjectId();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////

bool CutSceneManager::GetIsPedModel(u32 iModelIndex) const
{
	if(iModelIndex != strLocalIndex::INVALID_INDEX)
	{
		CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(iModelIndex)));
		if(pMI && pMI->GetModelType() == MI_TYPE_PED)
		{
			//Note this used to check if the model was streamed as in the player
			//but we are trying to remove support for creating objects that look like peds
			//so now we just create peds

			//CPedModelInfo* pPMI = static_cast<CPedModelInfo*>(pMI);

			//if (pPMI->GetIsStreamedGfx())
			//{
				return true;
			//}
			//else
			//{
			//	return false;
			//}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

float CutSceneManager::CalculateSkipTargetTime(int frame)
{
	//could convert this to a phase
	int  fFrameTime = frame;

	//only apply for non concatenated scenes as concatenated scene offsets are always zero unless the scene is branched,
	//but still want to calculate the absolute end frame.

	if(!IsConcatted())
	{
		 fFrameTime -= m_iStartFrame;
	}

	float ftime = float(fFrameTime) / CUTSCENE_FPS;

	return ftime = Clamp(ftime, 0.0f, GetCutfFile()->GetTotalDuration());
}

#if __BANK

//////////////////////////////////////////////////////////////////////////



void CutSceneManager::ScrubBackwardsCB()
{
	FixUpEventIndicesForPlayBackDirectionChange();

	m_ScrubbingState = SCRUBBING_BACKWARDS_FROM_TIME_LINE_BAR;

}

///////////////////////////////////////////////////////////////////////////

void CutSceneManager::ScrubForwardsCB()
{
	FixUpEventIndicesForPlayBackDirectionChange();

	m_ScrubbingState = SCRUBBING_FORWARDS_FROM_TIME_LINE_BAR;
}

///////////////////////////////////////////////////////////////////////////

void CutSceneManager::SetScrubbingControl(u32 CurrentFrame)
{
	if (IsCutscenePlayingBack() && !IsSkippingPlayback())
	{
		if (!IsPaused())
		{
			Pause();
		}

		if (IsPaused())
		{

			if ( CurrentFrame < GetCutSceneCurrentFrame())	//we are scrubbing so lets keep dispatching events by adjusting the playback rate
			{
				ScrubBackwardsCB();
			}
			else if( CurrentFrame > GetCutSceneCurrentFrame())
			{
				ScrubForwardsCB();
			}
			else
			{
				return;
			}

			m_bFadeOnSeamlessSkip = false;

			SetSkipFrame(CurrentFrame);

			DoSkippingState();

		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Save scene around the current view to obj
bool GatherEntities(CEntity* pEntity, void* data)
{
	Assert(pEntity && data);
	atArray<CEntity*>* entities = reinterpret_cast<atArray<CEntity*>*>(data);

	entities->PushAndGrow(pEntity);
	return true;
}

PS3_ONLY(namespace rage { extern u32 g_AllowVertexBufferVramLocks; })

#if USE_EDGE
static const int kGetGeometryMaxVertices = 32768;
static const int kGetGeometryMaxIndices  = 49152;
static Vector4* sGetGeometryVertices = NULL;
static u16* sGetGeometryIndices = NULL;
#endif
void CutSceneManager::CaptureDrawable(CEntity* entity, rmcDrawable* drawable, FileHandle& objFile, FileHandle& pivotFile, Matrix34& entityMatrix, u32& nextIndex, Matrix43* ms, const Vector3& euler)
{
	if (!drawable)
	{
		return;
	}

	rmcLodGroup* lodGroup = &drawable->GetLodGroup();
	rmcLod* lod = &lodGroup->GetLod(LOD_HIGH);
	if (!lod)
	{
		return;
	}

	grmModel* model = lod->GetModel(0);
	if (!model)
	{
		return;
	}

	CBaseModelInfo* mi = entity->GetBaseModelInfo();
	if (!mi)
	{
		return;
	}

	if (!mi->GetIsProp() && !entity->GetIsTypeVehicle() && !m_captureMap)
	{
		return;
	}
	else if ((mi->GetIsProp() && !m_captureProps) || (entity->GetIsTypeVehicle() && !m_captureVehicles))
	{
		return;
	}

	char str[256] = { 0 };
	char str2[256] = { 0 };

	if (mi->GetIsProp())
	{
		sprintf(str, "%s_prop", entity->GetModelName());
	}
	else if (entity->GetIsTypeVehicle())
	{
		sprintf(str, "%s_vehicle", entity->GetModelName());
	}
	else if (entity->GetIsTypePed())
	{
		sprintf(str, "%s_ped", entity->GetModelName());
	}
	else
	{
		sprintf(str, "%s_map", entity->GetModelName());
	}

	// output entity pivot point
	if (!entity->GetIsTypePed())
	{
		formatf(str2, "%s %f %f %f %f %f %f\n", str, entityMatrix.d.x, entityMatrix.d.y, entityMatrix.d.z, euler.x, euler.y, euler.z);
		CFileMgr::Write(pivotFile, str2, istrlen(str2));

		CObjExporter::AddGroup(objFile, str);
	}

	u8 extractMask = CExtractGeomParams::extractPos | CExtractGeomParams::extractUv;
	if (ms)
		extractMask |= CExtractGeomParams::extractSkin;

	PS3_ONLY(g_AllowVertexBufferVramLocks++;)
		for (u32 f = 0; f < model->GetGeometryCount(); ++f)
		{
			grmGeometry* geometry = &model->GetGeometry(f);
			if (geometry->GetType() == grmGeometry::GEOMETRYEDGE)
			{
#if USE_EDGE
				Assert(sGetGeometryVertices && sGetGeometryIndices);
				grmGeometryEdge* edgeGeometry = static_cast<grmGeometryEdge*>(geometry);

				// indices to outputBufferVertsPtrsEa
				Vector4** pointers = rage_aligned_new(16) Vector4*[CExtractGeomParams::obvIdxMax];
				u32 numVerts = 0;

				const int numIndices = edgeGeometry->GetVertexAndIndex(
					sGetGeometryVertices,
					kGetGeometryMaxVertices,
					pointers,
					sGetGeometryIndices,
					kGetGeometryMaxIndices,
					NULL, 0, NULL, NULL, NULL, NULL, NULL,
					&numVerts,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
					NULL,
#endif
					ms,
					extractMask);

				if (numIndices == 0)
				{
					continue;
				}

				sprintf(str, "%d verts", numVerts);
				CObjExporter::AddComment(objFile, str);

				CObjExporter::AddVerticesTransformed(objFile, pointers[CExtractGeomParams::obvIdxPositions], numVerts, &entityMatrix);
				CObjExporter::AddNewLine(objFile);

				CObjExporter::AddUVs(objFile, pointers[CExtractGeomParams::obvIdxUVs], numVerts);
				CObjExporter::AddNewLine(objFile);

				sprintf(str, "%d indices - %d tris", numIndices, numIndices / 3);
				CObjExporter::AddComment(objFile, str);
				CObjExporter::AddTriangles(objFile, sGetGeometryIndices, numIndices, nextIndex, true);

				nextIndex += numVerts;

				delete[] pointers;
#endif // USE_EDGE...
			}
			else
			{
				grcVertexBuffer* vb = geometry->GetVertexBuffer(true);
				grcIndexBuffer* ib = geometry->GetIndexBuffer(true);

				Assert(geometry->GetPrimitiveType() == drawTris);
				Assert(geometry->GetPrimitiveCount() * 3 == (u32)ib->GetIndexCount());
				Assert(vb && ib);

				sprintf(str, "%d verts", vb->GetVertexCount());
				CObjExporter::AddComment(objFile, str);
				CObjExporter::AddVerticesTransformed(objFile, vb, &entityMatrix, true);
				CObjExporter::AddNewLine(objFile);

				sprintf(str, "%d indices - %d tris", ib->GetIndexCount(), ib->GetIndexCount() / 3);
				CObjExporter::AddComment(objFile, str);
				CObjExporter::AddTriangles(objFile, ib, nextIndex, true);

				nextIndex += vb->GetVertexCount();
			}
		}
	PS3_ONLY(g_AllowVertexBufferVramLocks--;)
}

void CutSceneManager::CaptureSceneToObj()
{
	// create target obj file
	FileHandle objFile = CObjExporter::OpenFileForWriting(m_capturePath);
	if (objFile == NULL)
		return;

	// create txt file to store list of pivot points for each model
	char pivotFilename[256] = {0};
	ASSET.RemoveExtensionFromPath(pivotFilename, sizeof(pivotFilename), m_capturePath);
	safecat(pivotFilename, ".txt");
	FileHandle pivotFile = CFileMgr::OpenFileForWriting(pivotFilename);
	if (pivotFile == NULL)
		return;

	HANG_DETECT_SAVEZONE_ENTER();

	char str[256] = { 0 };

	Matrix34 sceneOrigin(M34_IDENTITY);
	GetSceneOrientationMatrix(sceneOrigin);

	// rotation and scale requested by artists
	float xRotation = -PI / 2.f;
	float scale = 100.f;

	Vector3 cameraPos = camInterface::GetPos();

	if (m_useSceneOrigin)
		cameraPos = sceneOrigin.d;

	formatf(str, "%f %f %f", cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
	CFileMgr::Write(pivotFile, str, istrlen(str));
	CFileMgr::Write(pivotFile, "\n", istrlen("\n"));

#if USE_EDGE
	sGetGeometryVertices = rage_aligned_new(16) Vector4[kGetGeometryMaxVertices*2];	// pos+UVs requested
	sGetGeometryIndices = rage_aligned_new(16) u16[kGetGeometryMaxIndices];
#endif

	// enumerate entities
	spdSphere captureSphere(RCC_VEC3V(cameraPos), ScalarV((float)m_iCaptureRadius));
	fwIsSphereIntersecting intersection(captureSphere);

	u32 oldCapacity = fwSearch::GetDefaultSearch().GetResult().GetCapacity();
	fwSearch::GetDefaultSearch().GetResult().ResizeGrow(oldCapacity * 3);

	atArray<CEntity*> entities;
	CGameWorld::ForAllEntitiesIntersecting(&intersection, GatherEntities, (void*)&entities,
		(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_VEHICLE | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT | ENTITY_TYPE_MASK_PED), (SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS),
		SEARCH_LODTYPE_ALL, SEARCH_OPTION_FORCE_PPU_CODEPATH | SEARCH_OPTION_LARGE, WORLDREP_SEARCHMODULE_DEBUG);

	sprintf(str, "camera position: %f %f %f", cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
	CObjExporter::AddComment(objFile, str);

	u32 nextIndex = 1;

	// grab geometry
	for (u32 i = 0; i < entities.GetCount(); ++i)
	{
		// account for camera translation, rotate and scale as requested
		Matrix34 entityMatrix = MAT34V_TO_MATRIX34(entities[i]->GetMatrix());
		Vector3 euler = entityMatrix.GetEulers();
		entityMatrix.Translate(-cameraPos);
		entityMatrix.RotateFullX(xRotation);
		entityMatrix.ScaleFull(scale);

        if (m_useSceneOrigin)
            entityMatrix.Dot3x3(sceneOrigin);

		rmcDrawable* drawable = NULL;
		if (entities[i]->GetIsTypePed())
		{
			CPedModelInfo* modelInfo = (CPedModelInfo*)entities[i]->GetBaseModelInfo();
			if (!modelInfo || !modelInfo->GetVarInfo())
				continue;

			CPed* ped = (CPed*)entities[i];
			if (m_capturePeds)
			{
				// create matrix set
				u32 numBones = ped->GetSkeleton()->GetBoneCount();
				Matrix43* ms = rage_aligned_new(16) Matrix43[numBones];
				ped->GetSkeleton()->Attach(true, ms);

				formatf(str, "%s_ped %f %f %f %f %f %f\n", ped->GetModelName(), entityMatrix.d.x, entityMatrix.d.y, entityMatrix.d.z, euler.x, euler.y, euler.z);
				CFileMgr::Write(pivotFile, str, istrlen(str));
				formatf(str, "%s_ped", ped->GetModelName());
				CObjExporter::AddGroup(objFile, str);

				for (s32 f = 0; f < PV_MAX_COMP; ++f)
				{
					if (ped->GetPedDrawHandler().GetPedRenderGfx())
					{
						drawable = ped->GetPedDrawHandler().GetPedRenderGfx()->GetDrawable(f);
					}
					else
					{
						CPedVariationData& varData = ped->GetPedDrawHandler().GetVarData();

						s32 drawableIdx = varData.GetPedCompIdx((ePedVarComp)f);
						if (drawableIdx == PV_NULL_DRAWBL)
							continue;

						if (!modelInfo->GetVarInfo())
							continue;

						if (!modelInfo->GetVarInfo()->IsValidDrawbl(f, drawableIdx))
							continue;

						Dwd* dwd = g_DwdStore.Get(strLocalIndex(modelInfo->GetPedComponentFileIndex()));
						drawable = CPedVariationPack::ExtractComponent(dwd, (ePedVarComp)f, drawableIdx, varData.GetPedCompHashPtr((ePedVarComp)f), varData.GetPedCompAltIdx((ePedVarComp)f));
					}

					CaptureDrawable(ped, drawable, objFile, pivotFile, entityMatrix, nextIndex, ms, euler);
				}

				// free matrices
				delete[] ms;
			}

			// capture mover capsule
			if (m_capturePedCapsules)
			{
				phInst* inst = entities[i]->GetCurrentPhysicsInst();
				if (inst)
				{
					phBound* bound = inst->GetArchetype()->GetBound();
					if (bound)
					{
						if (bound->GetType() == phBound::CAPSULE)
							GenerateCapsule(inst->GetMatrix(), (phBoundCapsule*)bound, objFile, nextIndex, -cameraPos, xRotation, scale);
						else if (bound->GetType() == phBound::COMPOSITE)
						{
							phBoundComposite* composite = (phBoundComposite*)bound;
							for (s32 i = 0; i < composite->GetNumBounds(); ++i)
							{
								if (composite->GetBound(i)->GetType() == phBound::CAPSULE)
									GenerateCapsule(inst->GetMatrix(), (phBoundCapsule*)composite->GetBound(i), objFile, nextIndex, -cameraPos, xRotation, scale);
							}
						}
					}
				}
			}
		}
		else
		{
			drawable = entities[i]->GetDrawable();
			CaptureDrawable(entities[i], drawable, objFile, pivotFile, entityMatrix, nextIndex, NULL, euler);
		}
	}

#if USE_EDGE
	delete[] sGetGeometryVertices;
	delete[] sGetGeometryIndices;
#endif

	CObjExporter::CloseFile(objFile);
	CFileMgr::CloseFile(pivotFile);

	RefreshObjList();

	fwSearch::GetDefaultSearch().GetResult().ResizeGrow(oldCapacity);

	HANG_DETECT_SAVEZONE_EXIT(NULL);
}

void CutSceneManager::SnapCamToSceneOrigin()
{
	Matrix34 sceneOrigin(M34_IDENTITY);
	GetSceneOrientationMatrix(sceneOrigin);

    if (sceneOrigin.d.x < 0.0001f && sceneOrigin.d.x > -0.0001 &&
        sceneOrigin.d.y < 0.0001f && sceneOrigin.d.y > -0.0001 &&
        sceneOrigin.d.z < 0.0001f && sceneOrigin.d.z > -0.0001)
        return;

    Vector3* camPos = (Vector3*)&camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetPosition();
    camPos->x = sceneOrigin.d.x;
    camPos->y = sceneOrigin.d.y;
    camPos->z = sceneOrigin.d.z;
}

void CutSceneManager::GenerateCapsule(Mat34V_In mtxIn, phBoundCapsule* capsule, FileHandle& objFile, u32& nextIndex, const Vector3& translation, float rotation, float scale)
{
	const int STEPS = 16;
	const Matrix34 mtx = RCC_MATRIX34(mtxIn);
	Matrix34 offsetMtx = mtx;

	float capsuleLength = capsule->GetLength();
	float capsuleRadius = capsule->GetRadius();

	mtx.Transform(VEC3V_TO_VECTOR3(capsule->GetCentroidOffset()), offsetMtx.d);

	offsetMtx.Translate(translation);
	offsetMtx.RotateFullX(rotation);
	offsetMtx.RotateLocalX(-rotation); // our coordinate system uses +z as up so need to fix the capsule orientation
	offsetMtx.ScaleFull(scale);

	const u32 numVerts = (STEPS >> 1) * (STEPS * 2) + (STEPS * 2);
	Vector4 verts[numVerts];
	u32 vertCount = 0;

	const u32 numIndices = (STEPS >> 1) * (STEPS * 6) + (STEPS * 6);
	u16 indices[numIndices];
	u32 indexCount = 0;

	{
		float halflength = capsuleLength * 0.5f;

		for (s32 i = 0; i < (STEPS >> 1); ++i)
		{
			float y0 = capsuleRadius * cosf(PI * float(i) / float(STEPS >> 1));
			float y1 = capsuleRadius * cosf(PI * float(i + 1) / float(STEPS >> 1));
			float deltay;
			if (i < (STEPS >> 2))
			{
				deltay = halflength;
			}
			else
			{
				deltay = -halflength;
			}

			float s0 = sinf(PI * float(i) / float(STEPS >> 1));
			float s1 = sinf(PI * float(i + 1) / float(STEPS >> 1));

			u32 wrapVertCount = vertCount;
			for (s32 f = 0; f < STEPS; ++f)
			{
				float x0 = capsuleRadius * s0 * sinf(2.0f * PI * float(f)		/ float(STEPS));
				float x1 = capsuleRadius * s1 * sinf(2.0f * PI * float(f + 1)	/ float(STEPS));
				float z0 = capsuleRadius * s0 * cosf(2.0f * PI * float(f)		/ float(STEPS));
				float z1 = capsuleRadius * s1 * cosf(2.0f * PI * float(f + 1)	/ float(STEPS));

				// on last iteration make sure to wrap the indices
				if (f == (STEPS - 1))
				{
					indices[indexCount + 0] = (u16)(vertCount + 0);
					indices[indexCount + 1] = (u16)(vertCount + 1);
					indices[indexCount + 2] = (u16)wrapVertCount;

					indices[indexCount + 3] = (u16)(vertCount + 1);
					indices[indexCount + 4] = (u16)(wrapVertCount + 1);
					indices[indexCount + 5] = (u16)wrapVertCount;
				}
				else
				{
					indices[indexCount + 0] = (u16)(vertCount + 0);
					indices[indexCount + 1] = (u16)(vertCount + 1);
					indices[indexCount + 2] = (u16)(vertCount + 2);

					indices[indexCount + 3] = (u16)(vertCount + 1);
					indices[indexCount + 4] = (u16)(vertCount + 3);
					indices[indexCount + 5] = (u16)(vertCount + 2);
				}
				indexCount += 6;

				verts[vertCount++] = Vector4(x0, y0 + deltay, z0, 0.f);
				verts[vertCount++] = Vector4(x1, y1 + deltay, z1, 0.f);
			}
		}

		u32 wrapVertCount = vertCount;
		for (s32 f = 0; f < STEPS; ++f)
		{
			Vector3 normal(cosf(2.0f * PI * float(f) / float(STEPS)), 0.0f, sinf(2.0f * PI * float(f) / float(STEPS)));
			float x = capsuleRadius * normal.x;
			float z = capsuleRadius * normal.z;

			verts[vertCount + 0] = Vector4(x, -halflength, z, 0.f);
			verts[vertCount + 1] = Vector4(x,  halflength, z, 0.f);

			if (f == (STEPS - 1))
			{
				indices[indexCount + 0] = (u16)(vertCount + 0);
				indices[indexCount + 1] = (u16)(vertCount + 1);
				indices[indexCount + 2] = (u16)wrapVertCount;

				indices[indexCount + 3] = (u16)(vertCount + 1);
				indices[indexCount + 4] = (u16)(wrapVertCount + 1);
				indices[indexCount + 5] = (u16)wrapVertCount;
			}
			else
			{
				indices[indexCount + 0] = (u16)(vertCount + 0);
				indices[indexCount + 1] = (u16)(vertCount + 1);
				indices[indexCount + 2] = (u16)(vertCount + 2);

				indices[indexCount + 3] = (u16)(vertCount + 1);
				indices[indexCount + 4] = (u16)(vertCount + 3);
				indices[indexCount + 5] = (u16)(vertCount + 2);
			}

			vertCount += 2;
			indexCount += 6;
		}
	}

	Assertf(numVerts == vertCount, "numVerts: %d - vertCount: %d", numVerts, vertCount);
	Assertf(numIndices == indexCount, "numIndices: %d - indexCount: %d", numIndices, indexCount);

	CObjExporter::AddVerticesTransformed(objFile, verts, numVerts, &offsetMtx);
	CObjExporter::AddNewLine(objFile);
	CObjExporter::AddUVs(objFile, verts, numVerts); // dummy uvs to not mess upp the indexing
	CObjExporter::AddNewLine(objFile);
	CObjExporter::AddTriangles(objFile, indices, numIndices, nextIndex, false);
	nextIndex += numVerts;
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////
// This wrapper class is used to provide an interface consistent with the rest
// of the game for initialising the cutscene manager

void CutSceneManagerWrapper::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		CutSceneManager::CreateInstance();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManagerWrapper::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		CutSceneManager::GetInstance()->SetShutDownMode(shutdownMode); 
        CutSceneManager::GetInstance()->ShutDown();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		CutSceneManager::GetInstance()->SetShutDownMode(shutdownMode); 
		CutSceneManager::GetInstance()->SetDeleteAllRegisteredEntites(true);
        CutSceneManager::GetInstance()->ShutDownCutscene();

    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManagerWrapper::PreSceneUpdate()
{
    CutSceneManager::GetInstance()->PreSceneUpdate();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CutSceneManagerWrapper::PostSceneUpdate()
{
    CutSceneManager::GetInstance()->PostSceneUpdate(fwTimer::GetTimeStep());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const CCutSceneCameraEntity* CutSceneManager::GetCamEntity()
{
	atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
	for ( entry.Start(); !entry.AtEnd(); entry.Next() )
	{
		if (entry.GetData().pEntity->GetType() == CUTSCENE_CAMERA_GAME_ENITITY)
		{
			 return static_cast<const CCutSceneCameraEntity*>(entry.GetData().pEntity);
		}
	}
	return NULL;
}

bool CutSceneManager::HasCameraCutEarlyFromCutsceneInFirstPerson()
{
	if(m_bShouldProcessEarlyCameraCutEventsForSinglePlayerExits)
	{
		if(!m_bHasProccessedEarlyCutEventForSinglePlayerExits)
		{
			const CCutSceneCameraEntity* pcam = GetCamEntity(); 
			if(pcam && pcam->IsCuttingBackEarlyForScript() && pcam->IsInFirstPerson())
			{
				m_bHasProccessedEarlyCutEventForSinglePlayerExits = true; 
				return true; 
			}
		}
	}
	return false; 
}

#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
void CutSceneManager::LoadAuthorizedCutsceneList()
{
	if(m_AuthorizedCutscenes.m_AuthorizedCutsceneList.GetCount() == 0)
	{
		char metadataFilename[RAGE_MAX_PATH];

		formatf(metadataFilename, RAGE_MAX_PATH, "%s", "common:/data/debug/authorizedcutscene.pso.meta");
		bool hasSucceeded = PARSER.InitAndLoadObject(metadataFilename, "", m_AuthorizedCutscenes);
		if(!hasSucceeded)
		{
			cutsceneWarningf("Failed to load the authorized cutscene list (%s)", metadataFilename);
		}
	}
}
#endif

#if !__FINAL
bool CutSceneManager::IsDLCCutscene()
{
	if(GetCutfFile())
	{
		const char* facedir = GetCutfFile()->GetFaceDirectory(); 

		atString filePath(facedir); 

		if(stristr(filePath.c_str(), "dlc") != NULL)
		{
			return true; 
		}
	}

	return false; 
}
#endif //!__FINAL


#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
void CutSceneManager::RenderWatermark()
{
	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack() && (CutSceneManager::GetInstance()->IsPlaying() ||  CutSceneManager::GetInstance()->IsPaused()))
	{
		const char *text = "Not Authorized";
		const float scale = 8.0f;
		const Vector2 pos(0.04f, 0.925f);
		const Color32 col(255, 255, 255, 63);

		CTextLayout textLayout;

		NetworkUtils::SetFontSettingsForNetworkOHD(&textLayout, scale, false);
		textLayout.SetColor(col);

		textLayout.SetWrap(Vector2(pos.x, pos.x + textLayout.GetStringWidthOnScreen(text, true)));
		textLayout.SetEdge(0.0f);
		textLayout.SetBackground(false, false);
		textLayout.SetBackgroundColor(Color32(0,0,0,150));
		textLayout.SetRenderUpwards(true);

		textLayout.Render(pos, text);
	}
}
#endif //CUTSCENE_AUTHORIZED_FOR_PLAYBACK

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//BANK
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK

int CutSceneManager::ms_AssetFoldersCount = 0;
const char* CutSceneManager::ms_AssetFolders[CutSceneManager::ms_AssetFoldersMax];

///////////////////////////////////////////////////////////////////////////////////////////////////
//Add an asset folder to the array of asset folders

bool CutSceneManager::PushAssetFolder(const char *szAssetFolder)
{
	if(szAssetFolder && ms_AssetFoldersCount < ms_AssetFoldersMax)
	{
		const size_t length = strlen(szAssetFolder);
		if(length > 0)
		{
			// Don't add duplicate paths
			for(int i = 0; i < ms_AssetFoldersCount; i ++)
			{
				if(stricmp(ms_AssetFolders[i], szAssetFolder) == 0)
				{
					return false;
				}
			}

			// Copy path
			char *szCopy = rage_new char[length + 1];
			strncpy(szCopy, szAssetFolder, length);
			szCopy[length] = '\0';

			// Lowercase path
			strlwr(szCopy);

			// Add path
			ms_AssetFolders[ms_AssetFoldersCount ++] = szCopy;

			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Remove all asset folders from the array of asset folders

void CutSceneManager::DeleteAllAssetFolders()
{
	for(int i = 0; i < ms_AssetFoldersCount; i ++)
	{
		if(ms_AssetFolders[i])
		{
			delete ms_AssetFolders[i];

			ms_AssetFolders[i] = NULL;
		}
	}

	ms_AssetFoldersCount = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Find an asset folder for the given scene name

const char *CutSceneManager::FindAssetFolderForCutfile(const char *sceneName)
{
	int iFoundAssetFolder = -1;

	if(cutsceneVerifyf(sceneName && sceneName[0] != '\0', "Scene name must exist!"))
	{
		for(int i = 0; i < ms_AssetFoldersCount; i ++)
		{
			if(ms_AssetFolders[i])
			{
				ASSET.PushFolder(ms_AssetFolders[i]);

				bool bFound = ASSET.Exists(sceneName, "");

				ASSET.PopFolder();

				if(bFound)
				{
					if(cutsceneVerifyf(iFoundAssetFolder == -1, "Found more than one asset folder for scene %s! %s %s", sceneName, ms_AssetFolders[iFoundAssetFolder], ms_AssetFolders[i]))
					{
						iFoundAssetFolder = i;
					}
				}
			}
		}
	}

	return iFoundAssetFolder != -1 ? ms_AssetFolders[iFoundAssetFolder] : NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Save the file to be saved so that it can be read into the MB scene.

void CutSceneManager::SaveCutfile()
{
	cutsceneAssertf(!IsConcatted(), "Cannot save cutxml file if the scene is concatenated. Save event in correct section");

	if(!IsConcatted() && GetCutfFile())
	{
		if (cOrignalSceneName.GetLength() == 0 )
		{
			cOrignalSceneName = GetCutfFile()->GetSceneName();
		}

		atString sceneName = cOrignalSceneName; 

		atString folder = GetCutfFile()->GetAssetPathForScene(sceneName, false);
		
		if(folder.GetLength() > 0 )
		{
			bool bFound = ASSET.Exists(folder, "");

			if (cutsceneVerifyf(bFound, "Could not find asset folder %s!", folder.c_str()))
			{
				atString file = GetCutfFile()->GetAssetPathForScene(sceneName);
				GetCutfFile()->SaveFile(file.c_str());
			}
		}
	}
}

void CutSceneManager::SaveGameDataCutfile()
{
	if(GetCutfFile())
	{
	if (cOrignalSceneName.GetLength() == 0 )
	{
		cOrignalSceneName = GetCutfFile()->GetSceneName();
	}
		char cFullPath[256];
		
		if(IsDLCCutscene())
		{
			const char* facedir = GetCutfFile()->GetFaceDirectory(); 

			atString filePath(facedir); 
			filePath.Lowercase(); 
			atArray<atString> stringResults; 

			if(stristr(filePath, "/cache/") != NULL)
			{
				filePath.Split(stringResults, "cache/"); 

				atString RootFile(stringResults[0]); 
#ifdef RS_BRANCHSUFFIX
				formatf(cFullPath, sizeof(cFullPath)-1, "%sassets_ng/cuts/!!game_data/%s", RootFile.c_str(), cOrignalSceneName.c_str());
#else
				formatf(cFullPath, sizeof(cFullPath)-1, "%sassets/cuts/!!game_data/%s", RootFile.c_str(), cOrignalSceneName.c_str());
#endif
			}
			else
			{
				atString TempSceneName(cOrignalSceneName); 
				TempSceneName.Lowercase(); 

				filePath.Split(stringResults, TempSceneName.c_str()); 

				atString RootFile(stringResults[0]); 

				formatf(cFullPath, sizeof(cFullPath)-1, "%s!!game_data/%s", RootFile.c_str(), cOrignalSceneName.c_str());
			}
		}
		else
		{
	formatf(cFullPath, sizeof(cFullPath)-1, "assets:/cuts/!!game_data/%s", cOrignalSceneName.c_str());
		}
	
	GetCutfFile()->SaveFile(cFullPath);
}
}

void CutSceneManager::UpdateDebugRenderState()
{
	CutSceneManager* pThis = CutSceneManager::GetInstance();
	if(pThis && pThis->IsRunning())
	{
		ms_DebugState.isRunning = true;
		ms_DebugState.cutSceneCurrentTime = pThis->m_fTime;
		ms_DebugState.cutScenePreviousTime = pThis->m_fPreviousTime;
		ms_DebugState.cutscenePlayTime = pThis->m_fPlayTime;
		ms_DebugState.cutsceneDuration = pThis->m_fDuration;
		ms_DebugState.totalSeconds = pThis->GetTotalSeconds();
		ms_DebugState.currentConcatSectionIdx = pThis->GetCurrentConcatSection();
		ms_DebugState.concatSectionCount = pThis->GetConcatDataList().GetCount();
		formatf(ms_DebugState.cutsceneName, "%s", pThis->GetCutsceneName());
		if(ms_DebugState.concatSectionCount > 0 && pThis->IsConcatSectionValid(ms_DebugState.currentConcatSectionIdx))
		{
			formatf(ms_DebugState.concatDataSceneName, "%s", pThis->GetConcatDataList()[ms_DebugState.currentConcatSectionIdx].cSceneName.GetCStr());
		}
		else
		{
			ms_DebugState.currentConcatSectionIdx = -1;
			ms_DebugState.concatDataSceneName[0] = 0;
		}
	}
	else
	{
		ms_DebugState.Reset();
	}
}

void CutSceneManager::SaveDrawDistanceFromWidget()
{
	cutsManager::SaveDrawDistances();
	SaveGameDataCutfile();
}
/////////////////////////////////////////////////////////////////////////////////////
// Adds the initial cut scene widget

void CutSceneManager::InitLevelWidgets()
{
	if (AssertVerify(!m_pCutSceneBank))
	{
		m_pCutSceneBank = fwDebugBank::CreateBank("Cut Scene Debug", MakeFunctor(*sm_pInstance,&CutSceneManager::ActivateBank), MakeFunctor(*sm_pInstance, &CutSceneManager::DeactivateBank));
	}
}

void EnumObjsCb(const fiFindData& data, void* user)
{
	Assert(user);
    const char* ext = ASSET.FindExtensionInPath(data.m_Name);
	if (ext && strcmp(ext, ".obj") == 0)
	{
		atArray<atString>* results = reinterpret_cast<atArray<atString>*>(user);
		atString& newEntry = results->Grow();
		newEntry.Set(ASSET.FileName(data.m_Name), istrlen(data.m_Name), 0);
	}
}

void CutSceneManager::SelectNewObjCb()
{
	if (m_selectedObj > -1 && m_pObjsCombo)
	{
		safecpy(m_capturePath, OBJ_CAPTURE_PATH);
		const char* itemString = m_pObjsCombo->GetString(m_selectedObj);
		if (!itemString)
			return;
		safecat(m_capturePath, itemString);

		char buf[256] = {0};
		ASSET.RemoveExtensionFromPath(buf, sizeof(buf), m_capturePath);
		safecat(buf, ".txt");
		FileHandle fh = CFileMgr::OpenFile(buf);
		if (fh != NULL)
		{
			if (CFileMgr::ReadLine(fh, buf, sizeof(buf)))
			{
				// we just read the first line which should be of the format "%f %f %f"
				// try to get three floats out of it
				char* ptr = buf;

				float x = (float)atof(ptr);

				ptr = strchr(ptr, ' ');
				if (!ptr)
					return;

				float y = (float)atof(ptr);

				ptr = strchr(ptr + 1, ' ');
				if (!ptr)
					return;

				float z = (float)atof(ptr);
				((Vector3*)&camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetPosition())->Set(x, y, z);
			}
		}
	}
}

void CutSceneManager::RefreshObjList()
{
	atArray<atString> m_capturedObjs;
	atArray<const char*> m_capturedObjsPtrs;
	ASSET.EnumFiles(OBJ_CAPTURE_PATH, EnumObjsCb, &m_capturedObjs);

	for (s32 i = 0; i < m_capturedObjs.GetCount(); ++i)
	{
		m_capturedObjsPtrs.PushAndGrow(m_capturedObjs[i].c_str());
	}

	if (m_selectedObj >= m_capturedObjsPtrs.GetCount())
		m_selectedObj = m_capturedObjsPtrs.GetCount() - 1;

	if (m_pObjsCombo && m_capturedObjsPtrs.GetCount() > 0)
		m_pObjsCombo->UpdateCombo("Objs", &m_selectedObj, m_capturedObjsPtrs.GetCount(), m_capturedObjsPtrs.GetCount() > 0 ? &m_capturedObjsPtrs[0] : NULL, datCallback(MFA(CutSceneManager::SelectNewObjCb), this));
}

void CutSceneManager::ActivateBank()
{
	XPARAM(enablecutscenelightauthoring);

	m_pCutSceneBank->AddButton("Start or End Selected Cut scene", datCallback(MFA(CutSceneManager::StartEndCutsceneButton), (datBase*)this));
	m_BankFileNameSelected = 0;
	m_BankFileNames.Reset();

	for (s32 c = 0; c < g_CutSceneStore.GetSize(); c++)
	{
		cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(c));
		if(pDef)
		{
			m_BankFileNames.PushAndGrow(pDef->m_name.GetCStr());
		}
	}

	if(m_BankFileNames.GetCount() == 0)
	{
		return;
	}


	std::sort(&m_BankFileNames[0], &m_BankFileNames[0] + m_BankFileNames.size(), AlphabeticalSort());

	if( m_BankFileNames.GetCount() > 0 )
	m_sceneNameCombo = m_pCutSceneBank->AddCombo("Cut Scenes", &m_BankFileNameSelected, m_BankFileNames.GetCount(), &m_BankFileNames[0]);
	m_pCutSceneBank->AddText("Search:", m_searchText, 260, datCallback(MFA(CutSceneManager::OnSearchTextChanged), (datBase*)this));
	m_pCutSceneBank->AddToggle("Enable audio sync when jogging", &m_bAllowAudioSyncFromRag, datCallback(MFA(CutSceneManager::EnableAudioSyncing), (datBase*)this));
#if __WIN32PC
	m_pCutSceneBank->AddSlider("Anim Time Tweak", &g_TweakTimeStep, -1000.0, 1000.0, 1.0f);
#endif
	m_pCutSceneBank->AddToggle("Stream Cutscene models", &m_bStreamObjectsWhileSceneIsPlaying, NullCB, "Stream Cutscene Models");
	m_pCutSceneBank->AddToggle("Show MB frame and scene name", &m_bRenderMBFrameAndSceneName, NullCB, "");
	m_pCutSceneBank->AddButton("Check for cutscene existance using the approval list", datCallback(MFA(CutSceneManager::CheckForCutsceneExistanceUsingApprovedList), (datBase*)this));
	m_pCutSceneBank->AddButton("Validate cutscene", datCallback(MFA(CutSceneManager::ValidateCutscene), (datBase*)this));
	m_pCutSceneBank->AddToggle("Use External Time Step", &m_bUseExternalTimeStep, NullCB, "");
	m_pCutSceneBank->AddSlider("External Time Step", &m_ExternalTimeStep, 0.0, 1.0, 0.1f);
	m_pCutSceneBank->AddSlider("PlayBackflags", &m_ValidConcatSectionFlags, -1, 1073741824, 1);
	m_pCutSceneBank->AddToggle("Log Using Call Stacks", &m_LogUsingCallStacks, NullCB, ""); 
	if(PARAM_enablecutscenelightauthoring.Get())
	{
		m_HoldAtEnd = true;
	}
	m_pCutSceneBank->AddToggle("Hold at end", &m_HoldAtEnd, NullCB, "");
	m_pCutSceneBank->AddToggle("Allow Scrubbing To Frame 0", &m_AllowScrubbingToZero, NullCB, "");
	m_pCutSceneBank->AddToggle("Snap debug cam to lights", &m_bSnapCameraToLights, NullCB, "");

#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD
	m_bRenderMBFrameAndSceneName = true;
#endif //SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD

	AddWidgets(*m_pCutSceneBank);	// Add the rage widgets

	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	bkGroup* PlayBackGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Play Back"));
	if(Ragebank)
	{
		if(PlayBackGrp)
		{
			Ragebank->SetCurrentGroup(*PlayBackGrp);
				Ragebank->AddToggle("Fade on skips", &m_bFadeOnSeamlessSkip , NullCB, "no fade on skips");
			Ragebank->UnSetCurrentGroup(*PlayBackGrp);
		}
	}

	m_DebugManager.InitWidgets();

#if !SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD


	//add the lights debug functionality
	bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Light Editing"));
	if(Ragebank)
	{
		if(group)
		{
			// initialise this var to a valid range for the widget, on reset the light vars get initialized after the cut scene
			if(EditLightSource::GetIndex() < 0)
			{
				EditLightSource::SetIndex(-1);
			}

			Ragebank->SetCurrentGroup(*group);



			Ragebank->AddToggle("Freeze lights update", &DebugLights::m_freeze, NullCB, "light fix");
			Ragebank->AddToggle("Debug Draw", &m_bDisplayLightLines, datCallback(MFA(CutSceneManager::RenderCutsceneLightsInfo), (datBase*)this));
			Ragebank->AddToggle("Show only active lights", &m_displayActiveLightsOnly , datCallback(MFA(CutSceneManager::ShowOnlyActiveCutsceneLights), (datBase*)this));
			Ragebank->AddToggle("Show animated lights", &m_bRenderAnimatedLights , NullCB, "render only animated lights");
			Ragebank->AddToggle("Show static lights", &m_bRenderCutsceneStaticLight , NullCB, "render only static lights");
			Ragebank->AddToggle("Display All Lights", &DebugLights::m_debug , NullCB, "render all lights");
			Ragebank->AddSlider("Scene Light", EditLightSource::GetIndexPtr(), -1, MAX_STORED_LIGHTS - 1, 1);
			Ragebank->AddToggle("Enable z-test for debug draw", &grcDebugDraw::g_doDebugZTest);
			Ragebank->AddButton("Save cutfile", datCallback(MFA(CutSceneManager::SaveMaxLightXml), (datBase*)this));
			m_pSaveLightStatusText = Ragebank->AddText("Save Status:",&m_lightSaveStatus[0], 256, true);
			Ragebank->UnSetCurrentGroup(*group);
		}
	}

	SetFrameRangeForPlayBackSliders();
	gStreamingRequestList.InitWidgets(*m_pCutSceneBank);
	m_pCutSceneBank->PushGroup("Map Capture", false);
		m_pObjsCombo = m_pCutSceneBank->AddCombo("Objs", &m_selectedObj, 0, NULL, datCallback(MFA(CutSceneManager::SelectNewObjCb), this));
		m_pCaptureRadiusSlider = m_pCutSceneBank->AddSlider("Scene capture radius", &m_iCaptureRadius, 10, 500, 1, NullCB);
		m_pCutSceneBank->AddText("Save path", m_capturePath, sizeof(m_capturePath));
		m_pCutSceneBank->AddToggle("Capture map", &m_captureMap);
		m_pCutSceneBank->AddToggle("Capture props", &m_captureProps);
		m_pCutSceneBank->AddToggle("Capture vehicles", &m_captureVehicles);
		m_pCutSceneBank->AddToggle("Capture peds", &m_capturePeds);
		m_pCutSceneBank->AddToggle("Capture ped capsules", &m_capturePedCapsules);
		m_pCutSceneBank->AddButton("Capture scene to OBJ", datCallback(MFA(CutSceneManager::CaptureSceneToObj), (datBase*)this));
		m_pCutSceneBank->AddToggle("Use scene origin", &m_useSceneOrigin);
		m_pCutSceneBank->AddButton("Snap free cam to scene origin", datCallback(MFA(CutSceneManager::SnapCamToSceneOrigin), (datBase*)this));
		m_pCutSceneBank->AddToggle("Show capture sphere", &m_showCaptureSphere);
		m_pCutSceneBank->AddSlider("FreeCam Position", (Vector3*)&camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetPosition(), -99999.f, 99999.f, 0.01f);
	m_pCutSceneBank->PopGroup();
	m_pCutSceneBank->PushGroup("Code", false);
		m_pCutSceneBank->AddToggle("Run Soak Test", &m_RunSoakTest);
		const char *playbackRates[] = { "1x", "2x", "4x", "8x" };
		m_pBank->AddCombo( "Soak Test Playback Rate", &m_SoakTestPlaybackRateIdx, 4, playbackRates);
		m_pCutSceneBank->AddToggle("Use verbose exit state logging", &m_bVerboseExitStateLogging, NullCB, "");
		m_pCutSceneBank->AddButton("Dump Cutfile", datCallback(MFA(CutSceneManager::DumpCutFile), (datBase*)this));
	m_pCutSceneBank->PopGroup();

#if USE_MULTIHEAD_FADE
	fade::addWidgets(*m_pCutSceneBank);
#endif

	RefreshObjList();
#endif
}

void CutSceneManager::RenderCutsceneLightsInfo()
{
	BankDisplayLightLinesChangedCallback();
}

void CutSceneManager::EnableAudioSyncing()
{
	if(!IsActive())
	{
		m_bAudioSyncWhenJogging = m_bAllowAudioSyncFromRag;
	}
}

void CutSceneManager::ShowOnlyActiveCutsceneLights()
{
	if(m_displayActiveLightsOnly)
	{
		DebugLights::m_debug = false;
		m_bDisplayLightLines = m_displayActiveLightsOnly;
	}
	RenderCutsceneLightsInfo();
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DeactivateBank()
{
	m_DebugManager.DestroyWidgets();
	m_pCutSceneBank->RemoveAllMainWidgets();
	m_pCurrentSceneGroup = NULL; //null the base group pointer so in add widgets its doesnt try to remove them again
	s_bIsBankOpen = false;
	m_sceneNameCombo = NULL;
	// remove the base cutsmanager widgets
	RemoveWidgets();
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::ShutdownLevelWidgets()
{
	m_pCutSceneBank->Shutdown();
	m_pCutSceneBank = NULL;
	m_pBank = NULL;			//null the base class bank pointer
	m_pCurrentSceneGroup = NULL;
	s_bIsBankOpen = false;
	// remove the base cutsmanager widgets
	RemoveWidgets();
}

/////////////////////////////////////////////////////////////////////////////////////
u32 CutSceneManager::GetMBFrame()
{
	if (!GetCutfFile())
	{
		return 0;
	}
	const atFixedArray<cutfCutsceneFile2::SConcatData, CUTSCENE_MAX_CONCAT> &concatDataList = GetConcatDataList();

	u32 frame = 0;
	if(concatDataList.GetCount() > 0)
	{
		u32 StartFrame = 0;

		if(m_concatDataList.GetCount() > 0)
		{
			u32 currentConcatSection = 0;
			//compute the current section for an internal concat
			if(GetCutfFile()->GetCutsceneFlags().IsSet(cutfCutsceneFile2::CUTSCENE_INTERNAL_CONCAT_FLAG))
			{
				for ( int i = m_concatDataList.GetCount() - 1; i >= 0; --i )
				{
					if ( GetCutSceneCurrentTime() >= m_concatDataList[i].fStartTime && GetCutSceneCurrentTime() <= GetEndTime() )
					{
						currentConcatSection = i;
						break;
					}
				}
			}
			else
			{
				currentConcatSection = GetCurrentConcatSection();
			}

			u32 totaldiscardedFrames = 0;
			StartFrame = concatDataList[currentConcatSection].iRangeStart;

			if(GetCutfFile()->GetDiscardedFrameList().GetCount() > 0 && GetCutfFile()->GetDiscardedFrameList().GetCount() >= currentConcatSection)
			{
				for(int j =0; j<GetCutfFile()->GetDiscardedFrameList()[currentConcatSection].frames.GetCount();)
				{
					if((m_iCurrentFrameWithFrameRanges - (u32)(concatDataList[currentConcatSection].fStartTime * CUTSCENE_FPS)) + totaldiscardedFrames + concatDataList[currentConcatSection].iRangeStart >= GetCutfFile()->GetDiscardedFrameList()[currentConcatSection].frames[j])
					{
						totaldiscardedFrames += GetCutfFile()->GetDiscardedFrameList()[currentConcatSection].frames[j+1] - GetCutfFile()->GetDiscardedFrameList()[currentConcatSection].frames[j];
					}
					j = j+2;
				}
			}

			frame = (m_iCurrentFrameWithFrameRanges - (u32)(concatDataList[currentConcatSection].fStartTime * CUTSCENE_FPS))  + StartFrame + totaldiscardedFrames;
		}
	}
	else
	{
		frame = m_iCurrentFrameWithFrameRanges;
	}
	return frame;
}

u32 CutSceneManager::GetRenderMBFrame()
{
	u32 uRenderMBFrame = m_uRenderMBFrame[0];
	m_uRenderMBFrame[0] = m_uRenderMBFrame[1];
	m_uRenderMBFrame[1] = static_cast< u32 >(-1);
	return uRenderMBFrame;
}

const char* CutSceneManager::GetPlaybackFlagName()
{
	u32 myflags = GetPlayBackFlags().GetAllFlags();
	u32 flagBit = 0;

	while (myflags >>= 1)
	{
		flagBit++;
	}

	static const char *c_cutscenePlayBackNames[] =
	{
		"CUTSCENE_REQUESTED_FROM_WIDGET",
		"CUTSCENE_REQUESTED_DIRECTLY_FROM_SKIP",
		"CUTSCENE_REQUESTED_FROM_Z_SKIP",
		"CUTSCENE_REQUESTED_IN_MISSION",
		"CUTSCENE_PLAYBACK_FORCE_LOAD_AUDIO_EVENT"
	};

	return c_cutscenePlayBackNames[flagBit];

}

void CutSceneManager::DebugDraw()
{
#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD
	return;
#else
	if(IsActive() && GetCutfFile())
	{
		const atFixedArray<cutfCutsceneFile2::SConcatData, CUTSCENE_MAX_CONCAT> &concatDataList = GetConcatDataList();

		u32 finalFrame = (u32) (GetCutfFile()->GetTotalDuration() * CUTSCENE_FPS);
		float phase = GetCutSceneCurrentTime() /  GetCutfFile()->GetTotalDuration();
		grcDebugDraw::AddDebugOutput("Cutscene Name: %s - %s", GetCutsceneName(), GetPlaybackFlagName());
		grcDebugDraw::AddDebugOutput("Cutscene State: %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput("");

		/*
		if(IsLoaded())
		{
			grcDebugDraw::AddDebugOutput("Scene Name: %s (Loaded)", GetCutsceneName());
		}
		else
		{
			grcDebugDraw::AddDebugOutput("Scene Name: %s (Loading)", GetCutsceneName());
		}*/

		if(IsCutscenePlayingBack())
		{
			grcDebugDraw::AddDebugOutput("Cutscene Time: Current - Previous = delta (%f - %f = %f)", GetCutSceneCurrentTime(), GetCutScenePreviousTime(), GetCutSceneCurrentTime()-GetCutScenePreviousTime());
			grcDebugDraw::AddDebugOutput("Audio    Time: Current - Previous = delta (%f - %f = %f)", m_fCurrentAudioTime,  m_fLastAudioTime, m_fCurrentAudioTime - m_fLastAudioTime);
			grcDebugDraw::AddDebugOutput("");
			grcDebugDraw::AddDebugOutput("Anim Section: %d of %d", GetCurrentSection()+1, m_fSectionStartTimeList.GetCount());
			grcDebugDraw::AddDebugOutput("Current Frames: %f of %d", (GetCutSceneCurrentTime() * CUTSCENE_FPS), finalFrame);
			grcDebugDraw::AddDebugOutput("Phase: %f", phase);

			grcDebugDraw::AddDebugOutput("MB Frame: %d", GetMBFrame());

			if(concatDataList.GetCount() > 0 && IsConcatDataValid())
			{
				grcDebugDraw::AddDebugOutput("ConcatName: %s Concat Section: %d of %d",concatDataList[GetCurrentConcatSection()].cSceneName.GetCStr() , GetCurrentConcatSection()+1, GetConcatDataList().GetCount());
			}
			else
			{
				grcDebugDraw::AddDebugOutput("Not concatenated");
			}
		}
	}


	if (m_showCaptureSphere)
	{
		Vector3 cameraPos = camInterface::GetPos();
		grcDebugDraw::Sphere(cameraPos, (float)m_iCaptureRadius, Color32(255, 0, 0), false, 1, 64);
	}
	grcDebugDraw::AddDebugOutput("");
	#endif
}

void CutSceneManager::ActivateCameraTrackingCB()
{
	if(!CControlMgr::IsDebugPadOn())
	{
		CControlMgr::SetDebugPadOn(true); 
	}
}

void CutSceneManager::BankLightSelectedCallback()
{
	cutsManager::BankLightSelectedCallback(); 
	{
		if ( (m_iSelectedLightIndex > 0) && (m_iSelectedLightIndex <= m_editLightList.GetCount() ) )
		{
			SEditCutfLightInfo *pEditLightInfo = GetSelectedLightInfo();

			//need to postion the camera to a newly selected light but only if we are driving the light from the otherwise all tyhe lights might 
			//just end up here
			if(camInterface::GetDebugDirector().IsFreeCamActive() && pEditLightInfo && m_bUpdateLightWithCamera )
			{
				UpdateCameraToLight(pEditLightInfo); 
			}
		}
	}
}

void CutSceneManager::SnapCameraToLight()
{
	if(!camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camInterface::GetDebugDirector().ActivateFreeCam();
		CControlMgr::SetDebugPadOn(true); 
	}

	if(camInterface::GetDebugDirector().IsFreeCamActive())
	{
		if ( (m_iSelectedLightIndex > 0) && (m_iSelectedLightIndex <= m_editLightList.GetCount() ) )
		{
			SEditCutfLightInfo *pEditLightInfo = GetSelectedLightInfo();
			if(pEditLightInfo)
			{
				UpdateCameraToLight(pEditLightInfo); 
			}
		}
	}
}

void CutSceneManager::SnapLightToCamera()
{
	if ( (m_iSelectedLightIndex > 0) && (m_iSelectedLightIndex <= m_editLightList.GetCount() ) )
	{
		SEditCutfLightInfo *pEditLightInfo = GetSelectedLightInfo();

		if(pEditLightInfo && pEditLightInfo->pObject && pEditLightInfo->pObject->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE)
		{
			pEditLightInfo->bOverridePosition = true; 
			pEditLightInfo->bOverrideDirection = true; 

			SetLightToCamera(pEditLightInfo, camInterface::GetFrame()); 
		}
	}
}

void CutSceneManager::SetLightToCamera(SEditCutfLightInfo *pEditLightInfo, const camFrame& frame)
{
	if(pEditLightInfo)
	{
		Matrix34 sceneMat(M34_IDENTITY);
		GetSceneOrientationMatrix(sceneMat); 
		Matrix34 lightLocal(M34_IDENTITY); 

		lightLocal = frame.GetWorldMatrix(); 
		lightLocal.RotateLocalX(HALF_PI); 

		lightLocal.DotTranspose(sceneMat); 

		pEditLightInfo->AuthoringPos = lightLocal.d; 
		lightLocal.ToQuaternion(pEditLightInfo->AuthoringQuat); 
		
	//	const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );
	//
	//	if(pLightObject->GetLightType() == CUTSCENE_SPOT_LIGHT_TYPE)
	//	{
	//		float ConeAngle; 
	//		if(pEditLightInfo->bOverrideConeAngle)
	//		{
	//			pEditLightInfo->fConeAngle = frame.GetFov() * 2.0f; 
	//		}
	//		else
	//		{
	//			ConeAngle = pEditLightInfo->fOriginalConeAngle; 
	//		}
	//	}
	}
}

void CutSceneManager::UpdateLightWithCamera(SEditCutfLightInfo *pEditLightInfo)
{
	if(camInterface::GetDebugDirector().IsFreeCamActive() && m_bUpdateLightWithCamera)
	{		
		SetLightToCamera(pEditLightInfo, camInterface::GetDebugDirector().GetFreeCamFrame()); 
	}
}

void CutSceneManager::UpdateCameraToLight(SEditCutfLightInfo *pEditLightInfo)
{
	if(pEditLightInfo && pEditLightInfo->pObject)
	{
		Matrix34 LocalAuthoringHelper(M34_IDENTITY); 
		Matrix34 SceneAuthoringHelper(M34_IDENTITY); 

		camFrame &freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();

		if(pEditLightInfo->pObject->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE )
		{
			pEditLightInfo->AuthoringQuat.Normalize(); 
			LocalAuthoringHelper.FromQuaternion(pEditLightInfo->AuthoringQuat); 
			LocalAuthoringHelper.d = pEditLightInfo->AuthoringPos; 
			BankOriginalSceneSpaceToWorldSpace(LocalAuthoringHelper, SceneAuthoringHelper); 

			freeCamFrame.SetWorldMatrix(SceneAuthoringHelper, false);
			freeCamFrame.SetPosition(SceneAuthoringHelper.d);
			Vector3 dir = pEditLightInfo->vDirection;
			dir.Normalize();
			freeCamFrame.SetWorldMatrixFromFront(dir); 
		}
		else if(pEditLightInfo->pObject->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE )
		{
			SEntityObject* pEntityObject = GetEntityObject(pEditLightInfo->pObject, false); 

			if(pEntityObject && pEntityObject->pEntity && pEntityObject->pEntity->GetType() == CUTSCENE_LIGHT_GAME_ENITITY)
			{
				Matrix34 mat = (static_cast<CCutSceneLightEntity*>(pEntityObject->pEntity))->GetCutSceneLight()->GetLightMatWorld(); 
				freeCamFrame.SetWorldMatrix(mat, false);
			}
		}

		const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

		if(pLightObject->GetLightType() == CUTSCENE_SPOT_LIGHT_TYPE)
		{
			float ConeAngle; 
			if(pEditLightInfo->bOverrideConeAngle)
			{
				ConeAngle = pEditLightInfo->fConeAngle;
			}
			else
			{
				ConeAngle = pEditLightInfo->fOriginalConeAngle; 
			}

			freeCamFrame.SetFov(ConeAngle * 2.0f); 
		}
	}

}

void CutSceneManager::UpdateLightDebugging()
{
	if ( (m_iSelectedLightIndex > 0) && (m_iSelectedLightIndex <= m_editLightList.GetCount()) && m_bSnapCameraToLights )
	{
		SEditCutfLightInfo *pEditLightInfo = GetSelectedLightInfo();

		if(pEditLightInfo)
		{
			const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

			char LightType[32];
			bool isSpot = false; 

			switch ( pLightObject->GetLightType() )
			{
			case CUTSCENE_DIRECTIONAL_LIGHT_TYPE:
				strcpy( LightType, "Directional" );
				break;
			case CUTSCENE_POINT_LIGHT_TYPE:
				strcpy( LightType, "Point" );
				break;
			case CUTSCENE_SPOT_LIGHT_TYPE:
				isSpot = true; 
				strcpy( LightType, "Spot" );
				break;
			default:
				strcpy( LightType, "(unknown)" );
				break;
			}

			char active[16]; 

			if(pEditLightInfo->IsActive)
			{
				strcpy( active, "Active" );
			}
			else
			{
				strcpy( active, "Not Active" );
			}

			grcDebugDraw::AddDebugOutput("%s: %s ", LightType, pLightObject->GetDisplayName().c_str());
			grcDebugDraw::AddDebugOutput("%s: Intensity: %f ", active, pEditLightInfo->fIntensity);


			//track the debug state s
			if(!m_debugActiveState && camInterface::GetDebugDirector().IsFreeCamActive())
			{
				UpdateCameraToLight(pEditLightInfo); 
			}

			m_debugActiveState = camInterface::GetDebugDirector().IsFreeCamActive(); 
			//allow the light to driven by teh camera,unless its a animated light 
			if(pEditLightInfo && pLightObject->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE)
			{
				if(m_bUpdateLightWithCamera && CControlMgr::IsDebugPadOn())
				{
					UpdateLightWithCamera(pEditLightInfo);
				}
			}
			else if(pEditLightInfo && pLightObject->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE && m_debugActiveState)
			{
				UpdateCameraToLight(pEditLightInfo); 
			}
		}
	}
}

void CutSceneManager::CreateLightAuthoringMat(SEditCutfLightInfo *pEditLightInfo )
{
	if(pEditLightInfo)
	{
		const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

		Matrix34 LightMat; 
		Vector3 ltan;
		Vector3 Direction = -pLightObject->GetLightDirection(); 
		ltan.Cross(Direction, FindMinAbsAxis(Direction));
		ltan.Normalize();
		Vector3 front;
		front.Cross(Direction, ltan);
		LightMat.Set(ltan, front, Direction, pEditLightInfo->vOriginalPosition);
		LightMat.ToQuaternion(pEditLightInfo->AuthoringQuat); 
		pEditLightInfo->OrginalAuthoringQuat = pEditLightInfo->AuthoringQuat; 
		pEditLightInfo->OriginaAuthoringPos = pEditLightInfo->vOriginalPosition; 
		pEditLightInfo->AuthoringPos = pEditLightInfo->vOriginalPosition; 
	}
}

bool CutSceneManager::ValidateLightObjects(const cutfLightObject* pFirstLight, const cutfLightObject* pSecondLight)
{
	if(pFirstLight && pSecondLight)
	{
		bool isValid = true; 
		
		if(pFirstLight->GetLightPosition() != pSecondLight->GetLightPosition())
		{
		//	errorMessage = " Positions dont match"; 
			Displayf("%s - positions dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false; 
		}

		if(pFirstLight->GetLightDirection() != pSecondLight->GetLightDirection())
		{
			Displayf("%s - direction dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false;  
		}

		if(pFirstLight->GetLightFallOff() != pSecondLight->GetLightFallOff())
		{
			Displayf("%s - falloff dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false; 
		}

		if(pFirstLight->GetLightIntensity() != pSecondLight->GetLightIntensity())
		{
			Displayf("%s - intensity dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false; 
		}

		if(pFirstLight->GetLightConeAngle() != pSecondLight->GetLightConeAngle())
		{
			Displayf("%s - coneangle dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false;  
		}

		if(pFirstLight->GetLightColour() != pSecondLight->GetLightColour())
		{
			Displayf("%s - colour dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false; 
		}

		if(pFirstLight->GetLightFlags() != pSecondLight->GetLightFlags())
		{
			Displayf("%s - flags dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false;  
		}

		if(pFirstLight->GetLightHourFlags() != pSecondLight->GetLightHourFlags())
		{
			Displayf("%s - hour flags dont match", pFirstLight->GetDisplayName().c_str()); 
			isValid = false;  
		}

		if(pFirstLight->GetVolumeIntensity() != pSecondLight->GetVolumeIntensity())
		{
			Displayf("%s - volume intensity dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}

		//if(pFirstLight->IsLightStatic() != pSecondLight->IsLightStatic())
		//{
		//		return false; 
		//}

		if(pFirstLight->GetVolumeSizeScale() != pSecondLight->GetVolumeSizeScale())
		{
			Displayf("%s - volume size scale dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false;  
		}

		if(pFirstLight->GetVolumeOuterColourAndIntensity() != pSecondLight->GetVolumeOuterColourAndIntensity())
		{
			Displayf("%s - volume outer colour intensity dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}

		if(pFirstLight->GetCoronaSize() != pSecondLight->GetCoronaSize())
		{
			Displayf("%s - corona size dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false;  
		}

		if(pFirstLight->GetCoronaIntensity() != pSecondLight->GetCoronaIntensity())
		{
			Displayf("%s - corona intensity dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}

		if(pFirstLight->GetCoronaZBias() != pSecondLight->GetCoronaZBias())
		{
			Displayf("%s - corona z bias dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}


		if(pFirstLight->GetExponentialFallOff() != pSecondLight->GetExponentialFallOff())
		{
			Displayf("%s - exp fall off dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}
		

		if(pFirstLight->GetInnerConeAngle() != pSecondLight->GetInnerConeAngle())
		{
			Displayf("%s - inner cone dont match", pFirstLight->GetDisplayName().c_str());
			isValid = false; 
		}
		return isValid; 
	}
	else
	{
		return false; 
	}
}

void CutSceneManager::DeleteLightCB()
{
	if(m_iSelectedLightIndex < 1 || m_iSelectedLightIndex > m_editLightList.GetCount())
	{
		return;
	}

	SEditCutfLightInfo *pEditLightInfo = GetSelectedLightInfo();

	if(pEditLightInfo && pEditLightInfo->pObject && !pEditLightInfo->markedForDelete)
	{
		const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

		if(pLightObject)
		{
			atHashString LightName = pLightObject->GetName(); 

			//just mark for delete and set it not active.
			pEditLightInfo->markedForDelete = true; 
			pEditLightInfo->IsActive = false; 

			//remove from our list of lights
			m_editLightList.Delete(m_iSelectedLightIndex - 1);

			if(m_iSelectedLightIndex > m_editLightList.GetCount())
			{
				m_iSelectedLightIndex --;
			}

			if(m_deletedLightList.Find(LightName) == -1)
			{
				m_deletedLightList.PushAndGrow(LightName);
			}

			UpdateLightCombo();

			PopulateActiveLightList(true); 

			BankActiveLightSelectedCallBack(); 
		}
	}
}

void CutSceneManager::RenameLightCB() 
{
	if(strlen(m_LightNewName) == 0)
	{
		Displayf("LIGHT EDITING: Failed to rename light because the Light name was empty"); 
		return;
	}
	



	for(int i=0; i < m_editLightList.GetCount(); i++)
	{
		if(m_editLightList[i]->pObject)
		{
			if(m_editLightList[i]->pObject->GetDisplayName() == m_LightNewName)
			{
				Displayf("LIGHT EDITING: Failed to rename light %s because its a duplicate name", m_editLightList[i]->pObject->GetDisplayName().c_str() ); 
				return; 
			}
		}
	}

	SEditCutfLightInfo *pEditCutfLightInfo = m_editLightList[m_iSelectedLightIndex - 1];

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE )
	{		
		Displayf("LIGHT EDITING: CANNOT RENAME AN ANIMATED LIGHT" ); 

		if(m_pEditLightStatusText)
		{
			m_pEditLightStatusText->SetString("LIGHT EDITING: CANNOT RENAME AN ANIMATED LIGHT"); 
		}
		return; 
	}

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject)
	{
		cutfLightObject* pLight = static_cast<cutfLightObject*>(pEditCutfLightInfo->pObject); 
		atHashString newName(m_LightNewName); 
		pLight->SetName(newName); 
		pEditCutfLightInfo->HasBeenRenamed = true; 
	}

	UpdateLightCombo(); 

	UpdateActiveLightListCombo(); 

}

void CutSceneManager::UpdateActiveLightSelectionHistory()
{
	s32 EventIndex; 
	atHashString name = GetDebugManager().FindCameraCutNameForTime(GetDebugManager().m_CameraCutsIndexEvents, GetTime(), EventIndex); 
	
	bool containsEntry = false; 
	for(int i=0; i < ActiveLightSelectionList.GetCount(); i++)
	{
		if(ActiveLightSelectionList[i].camCut == name)
		{
			ActiveLightSelectionList[i].selectionIndex = m_iActiveLightIndex; 
			containsEntry = true; 
		}
	}

	if(!containsEntry)
	{
		SelectionIndexCamCuts& Selection = ActiveLightSelectionList.Grow(); 
		Selection.camCut = name; 
		Selection.selectionIndex = m_iActiveLightIndex; 
	}
}

s32 CutSceneManager::GetActiveLightSelectionIndex()
{
	s32 EventIndex; 
	atHashString name = GetDebugManager().FindCameraCutNameForTime(GetDebugManager().m_CameraCutsIndexEvents, GetTime(), EventIndex); 

	for(int i=0; i < ActiveLightSelectionList.GetCount(); i++)
	{
		if(ActiveLightSelectionList[i].camCut == name)
		{
			if(ActiveLightSelectionList[i].selectionIndex > m_activeLightList.GetCount())
			{
				 ActiveLightSelectionList[i].selectionIndex = 0;  
			}

			return ActiveLightSelectionList[i].selectionIndex;
		}
	}

	return 1; 
}

void CutSceneManager::PopulateActiveLightList( bool forceRepopulateList)
{
	if(m_pActiveLightCombo)
	{
		bool repopulateList = false; 

		if(!forceRepopulateList)
		{	
			if(m_activeLightList.GetCount() == 0)
			{
				repopulateList = true; 
			}
			else
			{
				for(int i = 0; i < m_activeLightList.GetCount(); i++)
				{
					if(!m_activeLightList[i]->IsActive)
					{
						repopulateList = true; 
					}
				}
			}
		}
		else
		{
			repopulateList = forceRepopulateList; 
		}

		if(repopulateList)
		{
			m_activeLightList.Reset(false); 
			m_iActiveLightIndex = 0; 

			for ( int i = 0; i < m_editLightList.GetCount(); ++i )
			{
				if(m_editLightList[i]->IsActive)
				{
					m_activeLightList.Grow() = m_editLightList[i];
				}
			}

			m_iActiveLightIndex = GetActiveLightSelectionIndex();


			UpdateActiveLightListCombo(); 
		}

	}
}

void CutSceneManager::DuplicateLightFromCameraCB()
{
	//return if there is a light with this name
	for(int i=0; i < m_editLightList.GetCount(); i++)
	{
		if(m_editLightList[i]->pObject)
		{
			if(m_editLightList[i]->pObject->GetDisplayName() == m_duplicatedLightName)
			{
				Displayf("LIGHT EDITING: Failed to duplicate light %s because the name is a duplicate", m_editLightList[i]->pObject->GetDisplayName().c_str() ); 
				return; 
			}
		}
	}
	
	SEditCutfLightInfo *pEditCutfLightInfo = GetSelectedLightInfo();

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE )
	{		
		Displayf("LIGHT EDITING: CANNOT DUPLICATE AN ANIMATED LIGHT" ); 

		if(m_pEditLightStatusText)
		{
			m_pEditLightStatusText->SetString("LIGHT EDITING: CANNOT DUPLICATE AN ANIMATED LIGHT"); 
		}
		return; 
	}

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject)
	{
		const cutfLightObject* pLight = static_cast<const cutfLightObject*>(pEditCutfLightInfo->pObject); 

		cutfLightObject* pNewLight = static_cast<cutfLightObject*>(pLight->Clone()); 

		//now set the light data from the cameras position

		Matrix34 sceneMat(M34_IDENTITY);
		GetSceneOrientationMatrix(sceneMat); 
		Vector3 lightLocal(VEC3_ZERO); 

		camFrame frame = camInterface::GetFrame(); 

		sceneMat.UnTransform(frame.GetPosition(), lightLocal); 
		
		pNewLight->SetLightDirection(frame.GetWorldMatrix().b); 
		
		if(pNewLight->GetType() == CUTSCENE_SPOT_LIGHT_TYPE)
		{
			pNewLight->SetLightConeAngle(frame.GetFov()); 
		}

		pNewLight->SetLightPosition(lightLocal); 
		pNewLight->SetName(m_duplicatedLightName); 
		atHashString camCut; 
		
		if(AddLightWithEvents(pNewLight, camCut))
		{
			SEditCutfLightInfo* lightInfo = DuplicateLightEditInfo(pNewLight, pEditCutfLightInfo); 

			lightInfo->vOriginalPosition = lightLocal; 
			lightInfo->vPosition = lightLocal; 

			CreateLightAuthoringMat(lightInfo); 

			SEntityObject* pEntityObject = GetEntityObject(pNewLight); 

			if(pEntityObject && pEntityObject->pEntity && pEntityObject->pEntity->GetType() == CUTSCENE_LIGHT_GAME_ENITITY)
			{
				(static_cast<CCutSceneLightEntity*>(pEntityObject->pEntity))->GetCutSceneLight()->SetLightInfo(lightInfo); 
			}

			AddLightToActiveList(lightInfo); 

			UpdateLightCombo(); 
		}
	}
}

void CutSceneManager::DuplicateLightCB()
{

	//return if there is a light with this name
	for(int i=0; i < m_editLightList.GetCount(); i++)
	{
		if(m_editLightList[i]->pObject)
		{
			if(m_editLightList[i]->pObject->GetDisplayName() == m_duplicatedLightName)
			{
				atVarString message("LIGHT EDITING: Failed to duplicate light %s because the name is a duplicate", m_editLightList[i]->pObject->GetDisplayName().c_str() );
				atString debugMessage(message); 
			
				if(m_pEditLightStatusText)
				{
					m_pEditLightStatusText->SetString(debugMessage.c_str()); 
				}
				return; 
			}
		}
	}

	SEditCutfLightInfo *pEditCutfLightInfo = GetSelectedLightInfo();

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE )
	{	
		if(m_pEditLightStatusText)
		{
			m_pEditLightStatusText->SetString("LIGHT EDITING: CANNOT DUPLICATE AN ANIMATED LIGHT"); 
		}

		Displayf("LIGHT EDITING: CANNOT DUPLICATE AN ANIMATED LIGHT" ); 
		return; 
	}

	if(pEditCutfLightInfo && pEditCutfLightInfo->pObject)
	{
		const cutfLightObject* pLight = static_cast<const cutfLightObject*>(pEditCutfLightInfo->pObject); 

		cutfLightObject* pNewLight = static_cast<cutfLightObject*>(pLight->Clone()); 

		pNewLight->SetName(m_duplicatedLightName); 
		atHashString camCut; 
		if(AddLightWithEvents(pNewLight, camCut))
		{
			SEditCutfLightInfo* lightInfo = DuplicateLightEditInfo(pNewLight, pEditCutfLightInfo); 
			lightInfo->OriginalLightName = m_duplicatedLightName; 

			lightInfo->CamCutName = camCut; 

			SEntityObject* pEntityObject = GetEntityObject(pNewLight); 

			if(pEntityObject && pEntityObject->pEntity && pEntityObject->pEntity->GetType() == CUTSCENE_LIGHT_GAME_ENITITY)
			{
				(static_cast<CCutSceneLightEntity*>(pEntityObject->pEntity))->GetCutSceneLight()->SetLightInfo(lightInfo); 
			}
			
			AddLightToActiveList(lightInfo); 
			
			UpdateLightCombo(); 
			
			BankActiveLightSelectedCallBack(); 

			UpdateActiveLightSelectionHistory();
		}
	}
}

bool CutSceneManager::AddLightWithEvents(cutfLightObject* pLight, atHashString& camerHashName)
{
	bool addedlight = false;

	GetCutfFile()->AddObject(pLight); 

	atArray<const cutfObject*> cameraObjectList;
	s32 CameraObjectId = -1;
	GetCutfFile()->FindObjectsOfType(CUTSCENE_CAMERA_OBJECT_TYPE, cameraObjectList);

	if(cameraObjectList.GetCount() > 0)
	{
		CameraObjectId = cameraObjectList[0]->GetObjectId(); 
	}

	if(CameraObjectId > -1)
	{
		//create a list with all the camera cuts
		atArray<cutfEvent *>cameraCutEventList; 
		GetCutfFile()->FindObjectIdEventsForObjectIdOnly( CameraObjectId, GetCutfFile()->GetEventList(), cameraCutEventList, true, (u32)CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE); 

		//store the index for the camera cut for the set light anim event; 
		int iSetLightOnEventindex = -1; 
		for ( int i = cameraCutEventList.GetCount() - 1; i >= 0; --i )
		{
			if ( GetTime() >= cameraCutEventList[i]->GetTime())
			{
				if (cameraCutEventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					iSetLightOnEventindex = i;

					const cutfCameraCutEventArgs* pEventArgs = static_cast<const cutfCameraCutEventArgs*>(cameraCutEventList[i]->GetEventArgs()); 
					camerHashName = pEventArgs->GetName(); 
					break; 
				}
			}
		}

		cutfEvent* pSetLightEvent = NULL;
		cutfEvent* pClearLightEvent = NULL; 

		if(iSetLightOnEventindex > -1 && iSetLightOnEventindex < cameraCutEventList.GetCount())
		{
			//create the set light event
			pSetLightEvent = rage_new cutfObjectIdEvent(pLight->GetObjectId(), cameraCutEventList[iSetLightOnEventindex]->GetTime(), CUTSCENE_SET_LIGHT_EVENT, NULL); 

			int iClearLightEventIndex = iSetLightOnEventindex + 1; 

			if(iClearLightEventIndex < cameraCutEventList.GetCount())
			{
				pClearLightEvent = rage_new cutfObjectIdEvent(pLight->GetObjectId(), cameraCutEventList[iClearLightEventIndex]->GetTime(), CUTSCENE_CLEAR_LIGHT_EVENT, NULL); 
			}
			else if(iSetLightOnEventindex == cameraCutEventList.GetCount() - 1)
			{
				pClearLightEvent = rage_new cutfObjectIdEvent(pLight->GetObjectId(), GetCutfFile()->GetTotalDuration(), CUTSCENE_CLEAR_LIGHT_EVENT, NULL); 
			}

			if(pSetLightEvent && pClearLightEvent)
			{
				GetEntityObject(pLight, true);

				GetCutfFile()->AddEvent(pClearLightEvent); 
				GetCutfFile()->AddEvent(pSetLightEvent); 
				GetCutfFile()->SortEvents(); 

				DispatchEvent(pSetLightEvent);

				addedlight = true; 
			}
			else
			{
				GetCutfFile()->RemoveObject(pLight); 
				if(pSetLightEvent)
				{
					delete pSetLightEvent; 
					pSetLightEvent = NULL; 
				}

				if(pClearLightEvent)
				{
					delete pClearLightEvent; 
					pClearLightEvent = NULL; 
				}
			}
		}
		else
		{
			GetCutfFile()->RemoveObject(pLight);
		}
	}
	else
	{
		GetCutfFile()->RemoveObject(pLight); 
	}

	return addedlight; 
}

void CutSceneManager::CreateLightCB()
{
	if(!Verifyf(m_createdLightName[0] != '\0', "Light Name is empty!"))
	{
		return;
	}

	u32 uCreatedLightNameHash = atStringHash(m_createdLightName);
	for(int i = 0; i < m_editLightList.GetCount(); i ++)
	{
		SEditCutfLightInfo *pEditCutfLightInfo = m_editLightList[i];
		if(pEditCutfLightInfo)
		{
			const cutfLightObject *pCutfLightObject = static_cast< const cutfLightObject * >(pEditCutfLightInfo->pObject);
			if(pCutfLightObject)
			{
				if(!Verifyf(pCutfLightObject->GetName().GetHash() != uCreatedLightNameHash, "Cannot create a light with the same name as an existing light!"))
				{
					return;
				}
			}
		}
	}

	//create the light
	cutfLightObject* pLight = rage_new cutfLightObject(-1, m_createdLightName, m_createdLightTypeIndex, 0);  //( 0, m_createdLightName, frame.GetPosition(), frame.GetWorldMatrix().b, m_createdLightTypeIndex, 0, VEC3_ZERO, 0.0f, frame.GetFov()*2.0f, 1.0f, NULL);
	if(!pLight)
	{
		return; 
	}

	Matrix34 sceneMat(M34_IDENTITY);
	GetSceneOrientationMatrix(sceneMat); 
	Vector3 lightLocal(VEC3_ZERO); 

	camFrame frame = camInterface::GetFrame();
	sceneMat.UnTransform(frame.GetPosition(), lightLocal); 

	pLight->SetLightPosition(lightLocal); 

	pLight->SetLightDirection(frame.GetWorldMatrix().b); 
	pLight->SetLightConeAngle(frame.GetFov()); 
	
	pLight->SetLightIntensity(5.0f);
	pLight->SetLightFallOff(5.0f); 
	pLight->SetExponentialFallOff(2.0f); 
	pLight->SetLightColour(Vector3(1.0f, 1.0f, 1.0f));
	pLight->SetLightConeAngle(50.0f); 
	pLight->SetShadowBlur(2.0f); 
	pLight->SetLightFlag(CUTSCENE_LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS); 
	pLight->SetLightFlag(CUTSCENE_LIGHTFLAG_DONT_LIGHT_ALPHA); 
	pLight->SetLightFlag(CUTSCENE_LIGHTFLAG_NOT_IN_REFLECTION); 

	atHashString CutName; 

	//add it to the cutfile
	if(AddLightWithEvents(pLight, CutName))
	{
		AddLightEditInfo(pLight); 

		SEditCutfLightInfo*  pEditInfo = GetEditLightInfo(pLight->GetObjectId()); 

		if(pEditInfo)
		{
			SEntityObject* pEntityObject = GetEntityObject(pLight); 

			if(pEntityObject && pEntityObject->pEntity && pEntityObject->pEntity->GetType() == CUTSCENE_LIGHT_GAME_ENITITY)
			{
				(static_cast<CCutSceneLightEntity*>(pEntityObject->pEntity))->GetCutSceneLight()->SetLightInfo(pEditInfo); 
			}
			//set active so so the active light list is correct
			pEditInfo->WasCreatedInGame = true; 
			pEditInfo->CamCutName = CutName; 
			pEditInfo->IsActive = true; 
			CreateLightAuthoringMat(pEditInfo); 

			AddLightToActiveList(pEditInfo); 

			UpdateLightCombo(); 

			BankActiveLightSelectedCallBack(); 
			UpdateActiveLightSelectionHistory();
		}
	}
}



//next time editing rewrite to return a state of failure
void CutSceneManager::SyncLightData()
{	
	XPARAM(enablecutscenelightauthoring);
	bool IsTheFileDataValid = true; 
	bool containsDuplicateLights = false; 
	bool containsTheSameNumberOfLights = true; 
	atHashString duplicateLightName; 
	char filePath[RAGE_MAX_PATH];
	atString sceneName(GetCutsceneName()); 

	if(GetCutfFile())
	{
		formatf(filePath, RAGE_MAX_PATH, "%s%s", GetCutfFile()->GetAssetPathForScene(sceneName).c_str(), ".lightxml");
	}


	if(PARAM_enablecutscenelightauthoring.Get() && !IsConcatted())
	{
		m_AllowScrubbingToZero = false; 
		m_bSnapCameraToLights = true; 

		if(m_pDataLightXml == NULL)
		{
			m_pDataLightXml = rage_new cutfCutsceneFile2(); 

			if(GetCutfFile() && m_pDataLightXml)
			{
				m_pDataLightXml->SetFaceDirectory(GetCutfFile()->GetFaceDirectory()); 
			}

			m_CanSaveLightAuthoringFile = m_pDataLightXml->LoadMaxLightFile(GetCutsceneName()); 
		}
		else
		{
			if(GetCutfFile())
			{
				m_pDataLightXml->SetFaceDirectory(GetCutfFile()->GetFaceDirectory()); 
			}
			m_CanSaveLightAuthoringFile = m_pDataLightXml->LoadMaxLightFile(GetCutsceneName()); 
		}
		
		if(m_pDataLightXml == NULL)
		{
			return; 
		}

		if(m_CanSaveLightAuthoringFile)
		{
			cutsceneDisplayf("LIGHT EDITING: Validating the light data in %s against the light data in the cutfile", filePath); 
		}

		atArray<cutfObject *> maxlightObjectList;
		m_pDataLightXml->FindObjectsOfType( CUTSCENE_LIGHT_OBJECT_TYPE, maxlightObjectList );

		//check this incoming light file duplicate light names
		for(int maxLightObjectIndex = 0; maxLightObjectIndex < maxlightObjectList.GetCount(); maxLightObjectIndex++)
		{
			cutfLightObject *pMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[maxLightObjectIndex]);

			if(pMaxLightObject)
			{
				for(int secondMaxLightObjectIndex = 0; secondMaxLightObjectIndex < maxlightObjectList.GetCount(); secondMaxLightObjectIndex++)
				{
					if(maxLightObjectIndex != secondMaxLightObjectIndex)
					{
						cutfLightObject *pSecondMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[secondMaxLightObjectIndex]);

						if(pSecondMaxLightObject)
						{
							if(pMaxLightObject->GetName() == pSecondMaxLightObject->GetName())
							{
								duplicateLightName = pMaxLightObject->GetName(); 
								Displayf("data.lightxml contains duplicate light name %s. Please ensure the data.lightxml from Max has unique names", duplicateLightName.GetCStr()); 
								containsDuplicateLights = true; 
								m_CanSaveLightAuthoringFile = false; 
							}
						}
					}
				}
			}
		}

		// improve validation rewrite validation function to return a fail state.
		//if(m_editLightList.GetCount() != maxlightObjectList.GetCount())
		//{
		//	containsTheSameNumberOfLights = false; 
		//	m_CanSaveLightAuthoringFile = false; 
		//}

		for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
		{
			bool haveFoundLight = false; 

			//look at the override flags and update only the edited data
			SEditCutfLightInfo *pEditLightInfo = m_editLightList[lightListIndex];

			if(pEditLightInfo->pObject && pEditLightInfo->pObject->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE)
			{
				const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

				atHashString LightName = pLightObject->GetName(); 

				//now look in the loaded light data and find the light 
				if(m_CanSaveLightAuthoringFile)
				{
					for(int i =0; i < maxlightObjectList.GetCount(); i++)
					{
						cutfLightObject *pMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[i]);

						if(pMaxLightObject->GetName() == LightName)
						{
							haveFoundLight = true; 

							if(!ValidateLightObjects(pMaxLightObject, pLightObject))
							{
								m_CanSaveLightAuthoringFile = false;
								IsTheFileDataValid = false; 
								break;
							}
						
							parAttributeList& attList = const_cast<parAttributeList &>(pMaxLightObject->GetAttributeList());

							parAttribute* camAtt  = attList.FindAttribute("camera"); 

							if(camAtt)
							{
								pEditLightInfo->CamCutName = camAtt->GetStringValue(); 
							}

							parAttribute* BoneAtt = attList.FindAttribute("bonename");

							if(BoneAtt == NULL)
							{
								parAttribute* rotAtt = attList.FindAttribute("maxrelrotation"); 
								parAttribute* posAtt = attList.FindAttribute("maxrelposition"); 

								if(rotAtt)
								{
									Quaternion qLightRotation(0,0,0,0);

									atString strRotation(rotAtt->GetStringValue());

									atArray<atString> atSplit;
									strRotation.Split(atSplit, "," ,true);

									qLightRotation.x = (float)atof(atSplit[0]);
									qLightRotation.y = (float)atof(atSplit[1]);
									qLightRotation.z = (float)atof(atSplit[2]);
									qLightRotation.w = (float)atof(atSplit[3]);

									pEditLightInfo->AuthoringQuat = qLightRotation;
									pEditLightInfo->OrginalAuthoringQuat = qLightRotation; 

								}

								//if(BoneAtt == NULL)
								//{
									if(posAtt)
									{
										Vector3 vlightPos(VEC3_ZERO); 

										atString strPos(posAtt->GetStringValue());

										atArray<atString> atSplit;
										strPos.Split(atSplit, "," ,true);

										vlightPos.x = (float)atof(atSplit[0]);
										vlightPos.y = (float)atof(atSplit[1]);
										vlightPos.z = (float)atof(atSplit[2]);

										pEditLightInfo->AuthoringPos = vlightPos;
										pEditLightInfo->OriginaAuthoringPos = vlightPos; 

									}
							}
							else
							{
								pEditLightInfo->IsAttached = true; 
								for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
								{
									CreateLightAuthoringMat(m_editLightList[lightListIndex]); 
								}
							}
						}
					}

					if(!haveFoundLight)
					{
						Displayf("Cannot find light %s", LightName.GetCStr()); 
						m_CanSaveLightAuthoringFile = false;
						IsTheFileDataValid = false; 
					}
				}
			}
			else
			{
				for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
				{
					CreateLightAuthoringMat(m_editLightList[lightListIndex]); 
				}
			}
		}
	}
	else
	{
		for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
		{
			CreateLightAuthoringMat(m_editLightList[lightListIndex]); 
		}
	}

	if(m_pLightHotloadingStatusText)
	{
		if(!CutfileNonParseData::m_FileTuningFlags.IsFlagSet(CutfileNonParseData::CUTSCENE_LIGHT_TUNING_FILE))
		{
			m_pLightHotloadingStatusText->SetString("Hot Loading Not Enabled"); 
		}
		else if(CutfileNonParseData::m_FileTuningStatusFlags.IsFlagSet(CutfileNonParseData::CUTSCENE_LIGHT_TUNING_FILE_LOADED))
		{
			m_pLightHotloadingStatusText->SetString("Hot Loading Successful"); 
		}
		else 
		{
			char info[256]; 
			formatf(info, 256, "Hot Loading Failed: Requires: %s", filePath); 
			m_pLightHotloadingStatusText->SetString(info); 
		}
	}

	if(m_pEditLightStatusText)
	{
		if(IsConcatted())
		{
			m_pEditLightStatusText->SetString("CANNOT EDIT LIGHTS ON CONCATENATTED SCENES - USE THE PART"); 
		}
		else if(!PARAM_enablecutscenelightauthoring.Get())
		{
			m_pEditLightStatusText->SetString("CANNOT EDIT LIGHTS: use commandline -enablecutscenelightauthoring"); 
		}
		else if(containsDuplicateLights)
		{
			char info[256]; 
			formatf(info, 256, "CANNOT EDIT LIGHTS - data.lightxml contains duplicate light name %s. Please ensure the data.lightxml from Max has unique names", duplicateLightName.GetCStr()); 
			m_pEditLightStatusText->SetString(info); 
		}
		else if(!containsTheSameNumberOfLights)
		{
			m_pEditLightStatusText->SetString("CANNOT EDIT LIGHTS - The lights in light.dataxml do not match the same number of lights in the cutfile. Try building locally or try hotloading"); 
		}
		else if(!IsTheFileDataValid)
		{
			m_pEditLightStatusText->SetString("CANNOT EDIT LIGHTS - The lights in data.lightxml do not match the lights in the cutfile, get latest or build locally or try hotloading"); 
		}
		else if(PARAM_enablecutscenelightauthoring.Get() && !m_CanSaveLightAuthoringFile)
		{
			char info[128]; 
			formatf(info, 128, "CANNOT EDIT LIGHTS: failed to load from: %s", filePath); 
			m_pEditLightStatusText->SetString(info); 
		}
		else
		{
			m_pEditLightStatusText->SetString("LIGHTS AVAILIBLE FOR EDIT"); 
			cutsceneDisplayf("LIGHT EDITING: Validation success, the lights in the data.lightxml match those in the cut file"); 
		}
	}
}


void CutSceneManager::AddMaxProperties(cutfLightObject *pMaxLightObject, SEditCutfLightInfo *pEditLightInfo)
{
	parAttributeList& attList = const_cast<parAttributeList &>(pMaxLightObject->GetAttributeList());

	atVarString pos("%f,%f,%f", pEditLightInfo->AuthoringPos.x, pEditLightInfo->AuthoringPos.y, pEditLightInfo->AuthoringPos.z);
	atVarString rot("%f,%f,%f,%f", pEditLightInfo->AuthoringQuat.x, pEditLightInfo->AuthoringQuat.y, pEditLightInfo->AuthoringQuat.z, pEditLightInfo->AuthoringQuat.w);
	
	attList.AddAttribute("maxrelposition", pos.c_str());

	attList.AddAttribute("maxrelrotation", rot.c_str()); 

	attList.AddAttribute("camera", pEditLightInfo->CamCutName.TryGetCStr());
}

void CutSceneManager::ModifyMaxLightObject(cutfLightObject *pMaxLightObject, SEditCutfLightInfo *pEditLightInfo)
{
	if(!pEditLightInfo->IsAttached)
	{
		Matrix34 sceneMat; 
		Vector3 Position;

		if(pEditLightInfo->bOverridePosition)
		{
			Position = pEditLightInfo->AuthoringPos; 
		}
		else
		{
			Position = pEditLightInfo->OriginaAuthoringPos; 
		}

		if(pEditLightInfo->bHaveEditedPosition)
		{
			pMaxLightObject->SetLightPosition(Position); 

			char cPosition[RAGE_MAX_PATH];
			//char cRotation[RAGE_MAX_PATH];
			sprintf(cPosition, "%f,%f,%f", Position.x,Position.y,Position.z );
			//sprintf(cRotation, "%f,%f,%f,%f", qRotation.x,qRotation.y,qRotation.z,qRotation.w );

			parAttributeList& attList = const_cast<parAttributeList &>(pMaxLightObject->GetAttributeList());

			parAttribute* posAtt = attList.FindAttribute("maxrelposition"); 

			if(posAtt)
			{
				posAtt->SetValue(cPosition); 
			}
		}


		if(pEditLightInfo->bHaveEditedDirection)
		{
			//rotation
			Quaternion Rot; 
			parAttributeList& attList = const_cast<parAttributeList &>(pMaxLightObject->GetAttributeList());

			if(pEditLightInfo->bOverrideDirection)
			{
				Rot = pEditLightInfo->AuthoringQuat; 
			}
			else
			{
				Rot = pEditLightInfo->OrginalAuthoringQuat; 
			}

			char cRotation[RAGE_MAX_PATH];

			sprintf(cRotation, "%f,%f,%f,%f", Rot.x, Rot.y, Rot.z, Rot.w );

			parAttribute* rotAtt = attList.FindAttribute("maxrelrotation"); 

			if(rotAtt)
			{
				rotAtt->SetValue(cRotation); 
			}

			if(!pEditLightInfo->IsAttached)
			{
				Matrix34 lightMat(M34_IDENTITY); 
				lightMat.FromQuaternion(Rot); 
				lightMat.d = Position; 

				Matrix34 OriginalSceneMat(M34_IDENTITY); 
				GetOriginalSceneOrientationMatrix(OriginalSceneMat); 

				lightMat.Dot(OriginalSceneMat); 

				Vector3 LightDirWorld; 
				lightMat.Transform3x3(Vector3(0.0f, 0.0f, -1.0f), LightDirWorld);

				pMaxLightObject->SetLightDirection(LightDirWorld); 
			}
		}
	}

	if(pEditLightInfo->bHaveEditedIntensity)
	{
		if(pEditLightInfo->bOverrideIntensity)
		{
			pMaxLightObject->SetLightIntensity(pEditLightInfo->fIntensity); 
		}
		else
		{
			pMaxLightObject->SetLightIntensity(pEditLightInfo->fOriginalIntensity); 
		}
	}

	if(pEditLightInfo->bHaveEditedFalloff)
	{
		if(pEditLightInfo->bOverrideFallOff )
		{
			pMaxLightObject->SetLightFallOff(pEditLightInfo->fFallOff); 
		}
		else
		{
			pMaxLightObject->SetLightFallOff(pEditLightInfo->fOriginalFallOff); 
		}
	}

	if(pEditLightInfo->bHaveEditedConeAngle)
	{
		if(pEditLightInfo->bOverrideConeAngle )
		{
			pMaxLightObject->SetLightConeAngle(pEditLightInfo->fConeAngle); 
		}
		else
		{
			pMaxLightObject->SetLightConeAngle(pEditLightInfo->fOriginalConeAngle); 
		}
	}

	if(pEditLightInfo->bHaveEditedInnerConeAngle)
	{
		if(pEditLightInfo->bOverrideInnerConeAngle )
		{
			pMaxLightObject->SetInnerConeAngle(pEditLightInfo->InnerConeAngle); 
		}
		else
		{
			pMaxLightObject->SetInnerConeAngle(pEditLightInfo->OriginalInnerConeAngle); 
		}
	}


	if(pEditLightInfo->bHaveEditedShadowBlur)
	{
		if(pEditLightInfo->bOverrideShadowBlur )
		{
			pMaxLightObject->SetShadowBlur(pEditLightInfo->ShadowBlur); 
		}
		else
		{
			pMaxLightObject->SetShadowBlur(pEditLightInfo->OriginalShadowBlur); 
		}
	}

	if(pEditLightInfo->bHaveEditedColor)
	{
		if(pEditLightInfo->bOverrideColor )
		{
			pMaxLightObject->SetLightColour(pEditLightInfo->vColor); 
		}
		else
		{
			pMaxLightObject->SetLightColour(pEditLightInfo->vOriginalColor);
		}
	}

	if(pEditLightInfo->bHaveEditedFlags)
	{
		if(pEditLightInfo->bOverrideLightFlags )
		{
			pMaxLightObject->SetLightFlags((ECutsceneLightFlag)pEditLightInfo->LightFlags); 
		}
		else
		{
			pMaxLightObject->SetLightFlags((ECutsceneLightFlag)pEditLightInfo->OriginalLightFlags); 
		}
	}

	if(pEditLightInfo->bHaveEditedTimeFlags)
	{
		if(pEditLightInfo->bOverrideTimeFlags )
		{
			pMaxLightObject->SetLightHourFlags(pEditLightInfo->TimeFlags); 
		}
		else
		{
			pMaxLightObject->SetLightHourFlags(pEditLightInfo->OriginalTimeFlags); 
		}
	}

	if(pEditLightInfo->bHaveEditedVolumeIntensity)
	{
		if(pEditLightInfo->bOverrideVolumeIntensity )
		{
			pMaxLightObject->SetVolumeIntensity(pEditLightInfo->VolumeIntensity); 
		}
		else
		{
			pMaxLightObject->SetVolumeIntensity(pEditLightInfo->OriginalVolumeIntensity); 
		}
	}

	if(pEditLightInfo->bHaveEditedVolumeSizeScale)
	{
		if(pEditLightInfo->bOverrideVolumeSizeScale)
		{
			pMaxLightObject->SetVolumeSizeScale(pEditLightInfo->VolumeSizeScale); 
		}
		else
		{
			pMaxLightObject->SetVolumeSizeScale(pEditLightInfo->OriginalVolumeSizeScale); 
		}
	}

	if(pEditLightInfo->bHaveEditedOuterColourAndIntensity)
	{
		if(pEditLightInfo->bOverrideOuterColourAndIntensity )
		{
			pMaxLightObject->SetVolumeOuterColourAndIntensity(pEditLightInfo->OuterColourAndIntensity); 
		}
		else
		{
			pMaxLightObject->SetVolumeOuterColourAndIntensity(pEditLightInfo->OriginalOuterColourAndIntensity); 
		}
	}

	if(pEditLightInfo->bHaveEditedCoronaIntensity)
	{
		if(pEditLightInfo->bOverrideCoronaIntensity )
		{
			pMaxLightObject->SetCoronaIntensity(pEditLightInfo->CoronaIntensity); 
		}
		else
		{
			pMaxLightObject->SetCoronaIntensity(pEditLightInfo->OriginalCoronaIntensity); 
		}
	}


	if(pEditLightInfo->bHaveEditedCoronaSize)
	{
		if(pEditLightInfo->bOverrideCoronaSize )
		{
			pMaxLightObject->SetCoronaSize(pEditLightInfo->CoronaSize); 
		}
		else
		{
			pMaxLightObject->SetCoronaSize(pEditLightInfo->OriginalCoronaSize); 
		}
	}

	if(pEditLightInfo->bHaveEditedCoronaZBias)
	{
		if(pEditLightInfo->bOverrideCoronaZbias )
		{
			pMaxLightObject->SetCoronaZBias(pEditLightInfo->CoronaZbias); 
		}
		else
		{
			pMaxLightObject->SetCoronaZBias(pEditLightInfo->OriginalCoronaZbias); 
		}
	}

	if(pEditLightInfo->bHaveEditedExpoFallOff)
	{
		if(pEditLightInfo->bOverrideExpoFallOff )
		{
			pMaxLightObject->SetExponentialFallOff(pEditLightInfo->ExpoFallOff); 
		}
		else
		{
			pMaxLightObject->SetExponentialFallOff(pEditLightInfo->OriginalExpoFallOff); 
		}
	}
}

void CutSceneManager::SaveMaxLightXml()
{
	XPARAM(enablecutscenelightauthoring);
	if(PARAM_enablecutscenelightauthoring.Get() && m_CanSaveLightAuthoringFile && !IsConcatted())
	{
		//cutfCutsceneFile2 LightMaxXml; 
		//LightMaxXml.LoadMaxLightFile(GetCutsceneName()); 
		if(m_pDataLightXml == NULL)
		{
			return; 
		}

		atArray<cutfObject *> maxlightObjectList;
		m_pDataLightXml->FindObjectsOfType( CUTSCENE_LIGHT_OBJECT_TYPE, maxlightObjectList );

		//upgrade our max light list with renamed lights
		for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
		{
			//look at the override flags and update only the edited data
			SEditCutfLightInfo *pEditLightInfo = m_editLightList[lightListIndex];
			
			if(pEditLightInfo->HasBeenRenamed && !pEditLightInfo->WasCreatedInGame && !pEditLightInfo->markedForDelete)
			{
				for(int i =0; i < maxlightObjectList.GetCount(); i++)
				{
					cutfLightObject *pMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[i]);

					if(pMaxLightObject->GetName().GetHash() == pEditLightInfo->OriginalLightName.GetHash())
					{
						cutfLightObject* pLight = static_cast<cutfLightObject*>(m_editLightList[lightListIndex]->pObject); 
						
						pMaxLightObject->SetName(pLight->GetName()); 
					}
				}
			}
		}

		//lets look at the edited data and only change that
		for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
		{
			//look at the override flags and update only the edited data
			SEditCutfLightInfo *pEditLightInfo = m_editLightList[lightListIndex];

			const cutfLightObject *pLightObject = static_cast<const cutfLightObject *>( pEditLightInfo->pObject );

			atHashString LightName = pLightObject->GetName(); 
			bool FoundLight = false; 
			//now look in the loaded light data and find the light 
			for(int i =0; i < maxlightObjectList.GetCount(); i++)
			{
				cutfLightObject *pMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[i]);

				if(pMaxLightObject->GetName().GetHash() == LightName.GetHash())
				{
					ModifyMaxLightObject(pMaxLightObject, pEditLightInfo); 
					FoundLight = true; 
				}
			}

			if(!FoundLight && pEditLightInfo->WasCreatedInGame && !pEditLightInfo->markedForDelete)
			{
				cutfLightObject* pNewMaxLight = static_cast<cutfLightObject*>(pLightObject->Clone());
				AddMaxProperties(pNewMaxLight, pEditLightInfo); 
				ModifyMaxLightObject(pNewMaxLight, pEditLightInfo); 

				//get our light events
				atArray<cutfEvent*> LightEvents; 
				GetCutfFile()->FindEventsForObjectIdOnly(pNewMaxLight->GetObjectId(), GetCutfFile()->GetEventList(), LightEvents); 

				//add them
				m_pDataLightXml->AddObject(pNewMaxLight); 

				for(int i=0; i<LightEvents.GetCount(); i++)
				{
					if(LightEvents[i]->GetType() == CUTSCENE_OBJECT_ID_EVENT_TYPE)
					{
						cutfEvent* pEvent =  LightEvents[i]->Clone(); 
						
						static_cast<cutfObjectIdEvent*>(pEvent)->SetObjectId(pNewMaxLight->GetObjectId()); 
						
						m_pDataLightXml->AddEvent(pEvent); 
					}
				}

				m_pDataLightXml->SortEvents(); 
			}
		}

		for(int deletedLightListIndex = 0; deletedLightListIndex < m_deletedLightList.GetCount(); deletedLightListIndex ++)
		{
			atHashString deletedLightName = m_deletedLightList[deletedLightListIndex];

			for(int i = 0; i < maxlightObjectList.GetCount(); i ++)
			{
				cutfLightObject *pMaxLightObject = static_cast<cutfLightObject *>( maxlightObjectList[i]);

				if(pMaxLightObject->GetName().GetHash() == deletedLightName.GetHash())
				{
					m_pDataLightXml->RemoveObject(pMaxLightObject);

					break;
				}
			}
		}

		//save to the max file
		if(GetCutfFile())
		{
			atString SceneName(GetCutsceneName()); 
			atString scenePath = GetCutfFile()-> GetAssetPathForScene(SceneName);

			m_pDataLightXml->SaveFile(scenePath, PI_CUTSCENE_LIGHTFILE_EXT);

			//we have saved so we have transfered the info across, the data version is now up to date. 
			for(int lightListIndex = 0; lightListIndex < m_editLightList.GetCount(); lightListIndex++)
			{
				SEditCutfLightInfo *pEditLightInfo = m_editLightList[lightListIndex];

				if(pEditLightInfo && !pEditLightInfo->markedForDelete)
				{
					pEditLightInfo->WasCreatedInGame = false;
					pEditLightInfo->HasBeenRenamed = false; 
				}
			}

			if(m_pSaveLightStatusText)
			{
				m_pSaveLightStatusText->SetString("SAVE SUCCESS: Saved Max and Tune File");
			}
		}
	}
	else
	{
		if(m_pSaveLightStatusText)
		{
			if(!PARAM_enablecutscenelightauthoring.Get())
			{
				m_pSaveLightStatusText->SetString("SAVE FAILED: Need commandLine -enablecutscenelightauthoring");
			}
			else if(!m_CanSaveLightAuthoringFile)
			{
				char saveFileStatus[256]; 

				formatf(saveFileStatus, 256, "SAVE FAILED: failed to load assets_ng/cuts/%s/data.lightxml, make sure you have latest", GetCutsceneName()); 

				m_pSaveLightStatusText->SetString(saveFileStatus); 
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::StartEndCutsceneButton()
{
	CutSceneManager* pManager = CutSceneManager::GetInstance();

	if (!CGameWorld::FindLocalPlayer())
		return;


	if (pManager->IsRunning())
	{
		if(pManager->GetCutfFile())
		{
			m_PlayBackState = PLAY_STATE_FORWARDS_NORMAL_SPEED;

			m_bLoop = false;
			pManager->StopCutsceneAndDontProgressAnim();
			if (IsCutscenePlayingBack())
			{
				UpdateSkip(); 
			}
		}
		else
		{
			cutsceneWarningf("The cutscene is loading but something is taking a long time we havnt loaded the cutfile yet.");
		}
	}
	else
	{
		if(pManager->IsLoading())
		{
			TerminateLoadedOnlyScene();
		}
		else if (pManager->m_BankFileNames.GetCount() > 0)
		{
			if(pManager->m_BankFileNames[pManager->m_BankFileNameSelected])
		{
#if __BANK
			if(!m_bStartedFromWidget)
			{
				m_bStartedFromWidget = true;
			}
#endif

				pManager->RequestCutscene(pManager->m_BankFileNames[pManager->m_BankFileNameSelected], false, SKIP_FADE, SKIP_FADE, SKIP_FADE, SKIP_FADE, THREAD_INVALID, CUTSCENE_REQUESTED_FROM_WIDGET); // last param set to false so we dont fade in/out
			}
		}
	}
}

void CutSceneManager::OnSearchTextChanged()
{
	bool FoundMatch = false; 
	m_BankFileNameSelected = 0; 

	for(int i =0; i <m_BankFileNames.GetCount(); i++)
	{
		m_BankFileNames[i] = NULL; 
	}

	m_BankFileNames.Reset(); 

	if(m_searchText && strlen(m_searchText) > 0)
	{
		for (s32 c = 0; c < g_CutSceneStore.GetSize(); c++)
		{
			cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(c));
			if(pDef)
			{
				if(stristr(pDef->m_name.GetCStr(), m_searchText) != NULL)
				{
					m_BankFileNames.PushAndGrow(pDef->m_name.GetCStr()); 
					FoundMatch = true; 
				}
			}
		}
	}

	if(!FoundMatch)
	{
		for (s32 c = 0; c < g_CutSceneStore.GetSize(); c++)
		{
			cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(c));
			if(pDef)
			{
				m_BankFileNames.PushAndGrow(pDef->m_name.GetCStr());
		}
	}
}

	if(m_BankFileNames.GetCount() > 1)
	{
		std::sort(&m_BankFileNames[0], &m_BankFileNames[0] + m_BankFileNames.size(), AlphabeticalSort());
	}

	m_sceneNameCombo->UpdateCombo( "Cut Scenes", &m_BankFileNameSelected, m_BankFileNames.GetCount(), &m_BankFileNames[0] );

}


void CutSceneManager::DumpCutFile()
{
	if(GetCutfFile())
	{
		if (cOrignalSceneName.GetLength() == 0 )
		{
			cOrignalSceneName = GetCutfFile()->GetSceneName();
		}

		char cFullPath[256];
		formatf(cFullPath, sizeof(cFullPath)-1, "assets:/cuts/%s/dumpedfromgamedata", cOrignalSceneName.c_str());
		GetCutfFile()->SaveFile(cFullPath);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
//Creates a pointer to our selected hidden object.

SEditCutfObjectLocationInfo* CutSceneManager::GetHiddenObjectInfo()
{
	return m_DebugManager.GetHiddenObjectInfo();

}

///////////////////////////////////////////////////////////////////////////////////////////////////

SEditCutfObjectLocationInfo* CutSceneManager::GetFixupObjectInfo()
{
	return m_DebugManager.GetFixupObjectInfo();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sEditCutfBlockingBoundsInfo* CutSceneManager::GetBlockingBoundObjectInfo()
{
	return m_DebugManager.GetBlockingBoundObjectInfo();
}

/////////////////////////////////////////////////////////////////////////////////////

void CutSceneManager::DrawSceneOrigin()
{
	Matrix34 mMat;
	GetSceneOrientationMatrix(0, mMat);

	grcDebugDraw::Axis(mMat ,0.5, true);
	grcDebugDraw::Text(mMat.d, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), m_cCurrentSceneName );
}


/////////////////////////////////////////////////////////////////////////////////////
//Call back when a new cutscene ped has been selected.

/////////////////////////////////////////////////////////////////////////////////////
//Update the slider to the current frame range

void CutSceneManager::SetFrameRangeForPlayBackSliders()
{
	float fStartFrame = float(m_iStartFrame);
	float fEndFrame = float(m_iEndFrame);

	if(m_DebugManager.m_pVarFrameSlider)
	{
		m_DebugManager.m_pVarFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pVarPedFrameSlider)
	{
		m_DebugManager.m_pVarPedFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pVarDebugFrameSlider)
	{
		m_DebugManager.m_pVarDebugFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pVarSkipFrameSlider)
	{
		m_DebugManager.m_pVarSkipFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pPedVarPlayBackSlider)
	{
		m_DebugManager.m_pPedVarPlayBackSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pVehicleVarPlayBackSlider)
	{
		m_DebugManager.m_pVehicleVarPlayBackSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pOverlayPlayBackSlider)
	{
		m_DebugManager.m_pOverlayPlayBackSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_pWaterIndexFrameSlider)
	{
		m_DebugManager.m_pWaterIndexFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_pSkipToFrameSlider)
	{
		m_pSkipToFrameSlider->SetRange(fStartFrame, fEndFrame);
	}

	if(m_DebugManager.m_CascadeBounds.m_pShadowFrameSlider)
	{
		m_DebugManager.m_CascadeBounds.m_pShadowFrameSlider->SetRange(fStartFrame, fEndFrame);
	}
	cutsManager::SetFrameRangeForPlayBackSliders();
}

void CutSceneManager::LoadApprovedList()
{
	bool  bfileLoaded;

	if (!sysBootManager::IsPackagedBuild())
	{
		USE_DEBUG_MEMORY();

		ASSET.PushFolder("common:/non_final/cutscene");

		// Camera
		if (PARSER.LoadObject("ApprovedCutsceneList", "meta", m_ApprovedCutsceneList))
		{
			bfileLoaded = true;
		}
		else
		{
			Assertf(0, "Failed to load ApprovedCutsceneList.meta");
		}

		ASSET.PopFolder();
	}
}

void CutSceneManager::IsSceneApproved()
{
	LoadApprovedList();

	// Defaults
	m_bIsFinalApproved				= false;
	m_bIsCutsceneAnimationApproved	= false;
	m_bIsCutsceneCameraApproved		= false;
	m_bIsCutsceneFacialApproved		= false;
	m_bIsCutsceneLightingApproved	= false;
	m_bIsCutsceneDOFApproved		= false;

	// Find the cutscene in the approved list
	const u32 uCsHash = m_CutSceneHashString.GetHash();
	const int iApprovedCsCount = m_ApprovedCutsceneList.m_ApprovalStatuses.GetCount();
	bool bFound = false;
	for (int i=0; i < iApprovedCsCount; i++)
	{
		if (uCsHash == m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CutsceneName.GetHash())
		{
			m_bIsFinalApproved			= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_FinalApproved;
			m_bIsCutsceneAnimationApproved		= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CameraApproved;
			m_bIsCutsceneCameraApproved		= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_AnimationApproved;
			m_bIsCutsceneFacialApproved		= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_FacialApproved;
			m_bIsCutsceneLightingApproved	= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_LightingApproved;
			m_bIsCutsceneDOFApproved		= m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_DofApproved;

			bFound = true;
			break;
		}
	}


	if (!bFound)
	{
		// Cutscene is missing from the approved list
		cutsceneWarningf("Cutscene '%s' missing from ApprovedCutsceneList.meta.  Assuming unapproved.", m_CutSceneHashString.GetCStr());
	}
}


void CutSceneManager::CheckForCutsceneExistanceUsingApprovedList()
{
#if __ASSERT
	LoadApprovedList();

	cutsceneDisplayf("Started checking for cutscene existance using the approaval list");
	const int iApprovedCsCount = m_ApprovedCutsceneList.m_ApprovalStatuses.GetCount();
	for (int i=0; i < iApprovedCsCount; i++)
	{
		//Make a request to the streaming system with a hash of the cut scene name
		strLocalIndex iIndex = g_CutSceneStore.FindSlotFromHashKey(m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CutsceneName.GetHash());
		if (iIndex.Get() == -1)
		{
			cutsceneDisplayf("Failed to find Cutscene : %s ", m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CutsceneName.TryGetCStr());
		}
		else
		{
			//cutsceneDisplayf("Succeeded to find Cutscene : %s ", m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CutsceneName.TryGetCStr());
		}
		cutsceneAssertf((iIndex.Get() != -1), "Cutscene is missing : %s ", m_ApprovedCutsceneList.m_ApprovalStatuses[i].m_CutsceneName.TryGetCStr());

	}
	cutsceneDisplayf("Finished checking for cutscene existance using the approaval list");

	m_ApprovedCutsceneList.m_ApprovalStatuses.Reset();

#endif //__ASSERT
}

void CutSceneManager::ValidateCutscene()
{
	m_DebugManager.ValidateCutscene();
}


/////////////////////////////////////////////////////////////////////////////////////
//update the frame number if the scene is playing otherwise let this be manually overridden.

//void CutSceneManager::BankUpdateFrameNumber(u32 uFrame)
//{
//	if (IsPlaying())
//	{
//		m_iCurrentFrameWithFrameRanges = uFrame + GetCutfFile()->GetRangeStart(); //need to offset with the start frame number
//	}
//}

void CutSceneManager::OutputMoveNetworkForEntities()
{
	TUNE_GROUP_BOOL(CUTSCENE, DisplayMoveNetwork, false);
	if(DisplayMoveNetwork)
	{
		if(m_cutsceneEntityObjects.GetNumUsed() > 0)
		{
			atMap<s32, SEntityObject>::Iterator entry = m_cutsceneEntityObjects.CreateIterator();
			for ( entry.Start(); !entry.AtEnd(); entry.Next() )
			{
				SEntityObject *pEntityObject = m_cutsceneEntityObjects.Access(entry.GetKey());

				if(pEntityObject->pEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY || pEntityObject->pEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY
					|| pEntityObject->pEntity->GetType() ==  CUTSCENE_PROP_GAME_ENITITY || pEntityObject->pEntity->GetType() ==  CUTSCENE_WEAPON_GAME_ENTITY)
				{
					CCutsceneAnimatedModelEntity* pModel = static_cast<CCutsceneAnimatedModelEntity*>(pEntityObject->pEntity);
					CEntity* pEnt =pModel->GetGameEntity();
					if(pEnt)
					{
						/*if(pEnt->GetIsTypePed())
						{
							CPed* pPed = (CPed*)pEnt;
							pPed->GetMovePed().Dump();
						}*/

						if(pEnt->GetIsTypeVehicle())
						{
							CVehicle* pVeh = (CVehicle*)pEnt;
							pVeh->GetMoveVehicle().Dump();
						}
					}
				}
			}
		}
	}
}

void CutSceneManager::RenderCameraInfo()
{
	if(CCutSceneCameraEntity::m_RenderCameraStatus)
	{
		if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack() && (CutSceneManager::GetInstance()->IsPlaying() ||  CutSceneManager::GetInstance()->IsPaused()))
		{
			
			TUNE_GROUP_FLOAT ( DOF Info, BlendingX, 0.02f, 0.0f, 1.0f,0.01f);
			TUNE_GROUP_FLOAT ( DOF Info, BlendingY, 0.76f, 0.0f, 1.0f,0.01f);

			TUNE_GROUP_FLOAT ( DOF Info, DOfStateX, 0.02f, 0.0f, 1.0f,0.01f);
			TUNE_GROUP_FLOAT ( DOF Info, DOfStateY, 0.78f, 0.0f, 1.0f,0.01f);

			TUNE_GROUP_FLOAT ( DOF Info, DOfPlaneX, 0.02f, 0.0f, 1.0f,0.01f);
			TUNE_GROUP_FLOAT ( DOF Info, DOfPlaneY, 0.8f, 0.0f, 1.0f,0.01f);

			TUNE_GROUP_FLOAT ( DOF Info, CoCX, 0.02f, 0.0f, 1.0f,0.01f);
			TUNE_GROUP_FLOAT ( DOF Info, CoCY, 0.82f, 0.0f, 1.0f,0.01f);

			TUNE_GROUP_FLOAT ( DOF Info, DOfEffectX, 0.02f, 0.0f, 1.0f,0.01f);
			TUNE_GROUP_FLOAT ( DOF Info, DOfEffectY, 0.84f, 0.0f, 1.0f,0.01f);

			TUNE_GROUP_FLOAT ( DOF Info, scaleX, 0.4f, 0.0f, 1.0f,0.1f);
			TUNE_GROUP_FLOAT ( DOF Info, scaleY, 0.4f, 0.0f, 1.0f,0.1f);

			if (CDebugBar::GetDisplayReleaseDebugText() == DEBUG_DISPLAY_STATE_STANDARD)
			{
				CTextLayout CutsceneDebugText;
				CutsceneDebugText.SetScale(Vector2(scaleX, scaleY));
				CutsceneDebugText.SetColor(CRGBA(255,255,255,255));
				CutsceneDebugText.SetDropShadow(true);

				CutsceneDebugText.Render(Vector2(DOfStateX,DOfStateY), CCutSceneCameraEntity::ms_DofState.c_str());

				CutsceneDebugText.Render(Vector2(DOfPlaneX,DOfPlaneY), CCutSceneCameraEntity::ms_DofPlanes.c_str());

				CutsceneDebugText.Render(Vector2(CoCX,CoCY), CCutSceneCameraEntity::ms_CoCMod.c_str());

				CutsceneDebugText.Render(Vector2(DOfEffectX,DOfEffectY), CCutSceneCameraEntity::ms_DofEffect.c_str());
				CutsceneDebugText.Render(Vector2(BlendingX,BlendingY), CCutSceneCameraEntity::ms_Blending.c_str());
				
			}
		}
	}
}

void CutSceneManager::RenderUnapprovedSceneInfoCallback(bool UNUSED_PARAM(FinalApproved), bool AnimationApproved, bool CameraApproved, bool FacialApproved, bool LightingApproved, bool DOFApproved)
{
	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack() && (CutSceneManager::GetInstance()->IsPlaying() ||  CutSceneManager::GetInstance()->IsPaused()))
	{
		float AnimationApproved_X = 0.825f;
		float AnimationApproved_Y = 0.78f;
		float CameraApproved_X = 0.825f;
		float CameraApproved_Y = 0.81f;
		float FacialApproved_X = 0.825f;
		float FacialApproved_Y = 0.84f;
		float LightingApproved_X = 0.825f;
		float LightingApproved_Y = 0.87f;
		float DOFApproved_X = 0.825f;
		float DOFApproved_Y = 0.90f;
		float scaleX = 0.40f;
		float scaleY = 0.40f;

		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, FinalApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, FinalApproved_Y, 0.75f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AnimationApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AnimationApproved_Y, 0.78f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CameraApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CameraApproved_Y, 0.81f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, FacialApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, FacialApproved_Y, 0.84f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, LightingApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, LightingApproved_Y, 0.87f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, DOFApproved_X, 0.825f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, DOFApproved_Y, 0.90f, 0.0f,10.0f,0.001f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, scaleX, 0.40f, 0.0f, 10.0f,0.01f);
		//TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, scaleY, 0.40f, 0.0f, 10.0f,0.01f);

		if (CDebugBar::GetDisplayReleaseDebugText() == DEBUG_DISPLAY_STATE_STANDARD)
		{
			CTextLayout CutsceneDebugText;
			CutsceneDebugText.SetScale(Vector2(scaleX, scaleY));
			CutsceneDebugText.SetColor(CRGBA(255,255,255,255));
			CutsceneDebugText.SetDropShadow(true);

			CutsceneDebugText.Render(Vector2(AnimationApproved_X,AnimationApproved_Y), AnimationApproved ? "Animation Approved" : "Animation Not Approved");

			CutsceneDebugText.Render(Vector2(CameraApproved_X,CameraApproved_Y), CameraApproved ? "Camera Approved" : "Camera Not Approved" );

			CutsceneDebugText.Render(Vector2(FacialApproved_X, FacialApproved_Y), FacialApproved ? "Facial Approved" : "Facial Not Approved" );

			CutsceneDebugText.Render(Vector2(LightingApproved_X, LightingApproved_Y), LightingApproved ? "Lighting Approved" : "Lighting Not Approved" );

			CutsceneDebugText.Render(Vector2(DOFApproved_X,DOFApproved_Y), DOFApproved ? "DOF Approved" : "DOF Not Approved" );
		}
	}
}

void CutSceneManager::RenderStreamingPausedForAudio(bool AudioPaused)
{
	float AudioPaused_X = 0.039f;
	float AudioPaused_Y = 0.816f;
	float AudioSize_x = 0.533f;
	float AudioSize_y = 0.6f;

	/*
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AudioPaused_X, 0.039f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AudioPaused_Y, 0.816f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AudioSize_x, 0.533f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, AudioSize_y, 0.6f, 0.0f,10.0f,0.001f);
	*/

	if(AudioPaused)
	{
		CTextLayout CutscneDebugText;

		CutscneDebugText.SetScale(Vector2(AudioSize_x, AudioSize_y));
		CutscneDebugText.SetColor(CRGBA(255,0,0,255));
		CutscneDebugText.Render(Vector2(AudioPaused_X,AudioPaused_Y), "CUTSCENE PAUSED DUE TO AUDIO STREAMING");
		//CText::Flush();
	}
}

void CutSceneManager::RenderContentRestrictedData(bool bRestictedBuild)
{
	float MBFrame_x = 0.900f;
	float MBFrame_y = 0.870f;
	float MBFrame_scale_x = 0.14f;
	float MBFrame_scale_y = 1.0f;

	float CutsceneName_x = 0.032f;
	float CutsceneName_y = 0.870f;
	float CutsceneName_scale_x = 0.14f;
	float CutsceneName_scale_y = 1.0f;

	/*
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, MBFrame_x, 0.470f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, MBFrame_y, 0.870f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, MBFrame_scale_x, 0.14f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, MBFrame_scale_y, 1.0f, 0.0f,10.0f,0.001f);

	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CutsceneName_x, 0.032f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CutsceneName_y, 0.870f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CutsceneName_scale_x, 0.14f, 0.0f,10.0f,0.001f);
	TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, CutsceneName_scale_y, 1.0f, 0.0f,10.0f,0.001f);
	*/

	if((CutSceneManager::GetInstance()->IsPlaying() ||  CutSceneManager::GetInstance()->IsPaused())&& CutSceneManager::GetInstance()->m_bRenderMBFrameAndSceneName)
	{
		CTextLayout CutscneDebugText;
		char debugString[550];

		formatf(debugString, 550, "%d", CutSceneManager::GetInstance()->GetRenderMBFrame()); 
		CutscneDebugText.SetScale(Vector2(MBFrame_scale_x, MBFrame_scale_y));
		CutscneDebugText.SetColor(CRGBA(255,255,255,255));
		CutscneDebugText.SetBackground(TRUE, FALSE);
		CutscneDebugText.SetBackgroundColor(CRGBA(0,0,0,255));
		CutscneDebugText.Render(Vector2(MBFrame_x,MBFrame_y), &debugString[0]);

		formatf(debugString, 550, "%s", CutSceneManager::GetInstance()->GetCutsceneName());
		CutscneDebugText.SetScale(Vector2(CutsceneName_scale_x, CutsceneName_scale_y));
		CutscneDebugText.SetColor(CRGBA(255,255,255,255));
		CutscneDebugText.SetBackground(TRUE, FALSE);
		CutscneDebugText.SetBackgroundColor(CRGBA(0,0,0,255));
		CutscneDebugText.Render(Vector2(CutsceneName_x,CutsceneName_y), &debugString[0]);
		//CText::Flush();
	}

	if (bRestictedBuild)
	{
		float WaterMark_X = 0.747f;
		float WaterMark_Y = 0.045f;
		float WaterMarkScale_X = 0.53f;
		float WaterMarkScale_Y = 0.53f;

		/*
		TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, WaterMark_X, 0.747f, 0.0f,10.0f,0.001f);
		TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, WaterMark_Y, 0.045f, 0.0f,10.0f,0.001f);
		TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, WaterMarkScale_X, 0.53f, 0.0f,10.0f,0.001f);
		TUNE_GROUP_FLOAT ( CUTSCENE_DEBUG, WaterMarkScale_Y, 0.53f, 0.0f,10.0f,0.001f);
		*/

		CTextLayout WaterMarkText;

		WaterMarkText.SetScale(Vector2(WaterMarkScale_X, WaterMarkScale_Y));
		WaterMarkText.SetColor(CRGBA(0,0,0,255));
		WaterMarkText.Render(Vector2(WaterMark_X,WaterMark_Y), "TECHNICOLOR BUILD");
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Set the game time every frame stored at the start of the scene
void CutSceneManager::OverrideGameTime()
{
	if (m_DebugManager.m_bOverrideTimeCycle)
	{
		s32 numHours = m_DebugManager.m_fTimeCycleOverrideTime / 60;
		s32 numMins = m_DebugManager.m_fTimeCycleOverrideTime - (numHours*60);

		formatf(m_DebugManager.m_cGameTime, sizeof(m_DebugManager.m_cGameTime), "%d:%d:0",numHours, numMins );
		m_DebugManager.m_vTimeClock.x = (float) numHours;
		m_DebugManager.m_vTimeClock.y = (float) numMins;
		m_DebugManager.m_vTimeClock.z = 0.0f;
		CClock::SetTime(numHours, numMins, 0);
	}
}


void CutSceneManager::SoakTest()
{
	if(m_RunSoakTest)
	{
		if(!IsActive())
		{
			if (m_BankFileNameSelected < 0 || m_BankFileNameSelected == m_BankFileNames.GetCount())
			{
				m_BankFileNameSelected = 0;
			}

			StartEndCutsceneButton();
			m_BankFileNameSelected ++;
		}
		else
		{
			// Cutscene active, does it need speeding up?
			if (IsPlaying())
			{
				// Current playback rate
				const float fCurrentPlaybackRate = GetPlayBackRate();

				// What speedup do we want?
				float fDesiredPlaybackRate = 1.0f;
				switch (m_SoakTestPlaybackRateIdx)
				{
					case 0: fDesiredPlaybackRate = 1.0f; break;
					case 1: fDesiredPlaybackRate = 2.0f; break;
					case 2: fDesiredPlaybackRate = 4.0f; break;
					case 3: fDesiredPlaybackRate = 8.0f; break;
					default: fDesiredPlaybackRate = 1.0f; break;
				}

				if (fDesiredPlaybackRate == 1.0f)
				{
					if (fCurrentPlaybackRate != 1.0f)
					{
						// Play normally
						BankPlayForwardsCallback();
					}
				}
				else
				{
					if (fCurrentPlaybackRate != fDesiredPlaybackRate)
					{
						BankFastForwardCallback();
					}
				}
			}
		}
	}
}

void CutSceneManager::CutsceneSmokeTest()
{
	if (!CGameWorld::FindLocalPlayer())
		return;

	if(!IsActive())
	{
		int numberOfCutscenes = g_CutSceneStore.GetCount();
		strLocalIndex chosenCutscene = strLocalIndex((u32)fwRandom::GetRandomNumberInRange(0, numberOfCutscenes));

		if(g_CutSceneStore.IsValidSlot(chosenCutscene))
		{
			cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(chosenCutscene);
			if(pDef)
			{
				if (IsRunning() && GetCutfFile())
				{
					StopCutsceneAndDontProgressAnim();
				}
				else
				{
					if(IsLoading())
					{
						TerminateLoadedOnlyScene();
					}
					else
					{
						CPed * pPlayer = CGameWorld::FindLocalPlayer();
						if (pPlayer)
						{
							pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = true;
							pPlayer->RemovePhysics();
						}

#if __BANK
						if(!m_bStartedFromWidget)
						{
							// If m_bStartedFromWidget is set to true once the cutscene is loaded (inside CutSceneManager::DoLoadingCutfileState) 
							// the player position will be moved to the cutscene origin this is needed for rayfire cutscenes 
							m_bStartedFromWidget = true;
						}
#endif
						// last param set to false so we dont fade in/out
						RequestCutscene(pDef->m_name.GetCStr(), true, SKIP_FADE, SKIP_FADE, SKIP_FADE, SKIP_FADE, THREAD_INVALID, CUTSCENE_REQUESTED_FROM_WIDGET); 
					}
				}
			}
		}

	}

}

#endif //__BANK

#if !__NO_OUTPUT
void CutSceneManager::CommonDebugStr(char * debugStr)
{
	if (!CutSceneManager::GetInstance()->m_bIsCutSceneActive)
	{
		sprintf(debugStr, "Cutscene not requested yet - ");
		return;
	}

	sprintf(debugStr, "%s(%s): %6.2f(%u) - ",
		CutSceneManager::GetInstance()->GetCutsceneName(), CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(), CutSceneManager::GetInstance()->GetCutSceneCurrentFrame());
}
#endif //!__NO_OUTPUT



#if USE_MULTIHEAD_FADE
bool CutSceneManager::StartMultiheadFade(bool bFadeIn, bool bInstant, bool bFullscreenMovie)
{
	// Don't try to fade out if the blinders aren't up
	if(!bFadeIn && !m_bBlindersUp)
		return false;

	u32 endTime = 0;
	const float SIXTEEN_BY_NINE = 16.0f/9.0f;
	const float BLINDER_EPSILON = 0.001f;				// Offset to slightly bring in the blinders by 1 pixel to compensate for inaccuracies.

	if (GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		 const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
		 if(CHudTools::GetAspectRatio() > SIXTEEN_BY_NINE)
		 {
			 float fAspect = CHudTools::GetAspectRatio(true);
			 
			 float fDifference = ( (SIXTEEN_BY_NINE / fAspect) * 0.5f ) - BLINDER_EPSILON;
			 fwRect area(0.5f - fDifference, 0, 0.5f + fDifference, 1.0f);
			 endTime = CSprite2d::SetMultiFadeParams(area, bFadeIn,
				 fade::uDuration, fade::fAcceleration, fade::fWaveSize, fade::fAlphaOffset, bInstant, m_iBlinderDelay );
		 }
		 else
		 {
			 const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
			 endTime = CSprite2d::SetMultiFadeParams(area, bFadeIn,
				 fade::uDuration, fade::fAcceleration, fade::fWaveSize, fade::fAlphaOffset, bInstant, m_iBlinderDelay );
		 }
	}
	else
	{
		const GridMonitor &monitor = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		float fAspect = monitor.fPhysicalAspect;
		if (!GRCDEVICE.IsWindowed())
		{
			fAspect = monitor.fActualPhysicalAspect;
		}
		if(CHudTools::IsSuperWideScreen(false))
		{
			float fDifference = ( (SIXTEEN_BY_NINE / fAspect) * 0.5f ) - BLINDER_EPSILON;
			fwRect area(0.5f - fDifference, 0, 0.5f + fDifference, 1.0f);
			endTime = CSprite2d::SetMultiFadeParams(area, bFadeIn,
				fade::uDuration, fade::fAcceleration, fade::fWaveSize, fade::fAlphaOffset, bInstant, m_iBlinderDelay );
		}
		else if (fAspect > monitor.getLogicalAspect() && !bFullscreenMovie)
		{
			float fDifference = ( (monitor.getLogicalAspect() / fAspect) * 0.5f ) - BLINDER_EPSILON;
			fwRect area(0.5f - fDifference, 0, 0.5f + fDifference, 1.0f);
			endTime = CSprite2d::SetMultiFadeParams(area, bFadeIn,
				fade::uDuration, fade::fAcceleration, fade::fWaveSize, fade::fAlphaOffset, bInstant, m_iBlinderDelay );
		}
		else
		{
			return false;
		}
	}

	if(bInstant && !bFadeIn)
	{
		fade::uEndTime = 0;
	}
	else
	{
		fade::uEndTime = MAX( endTime, CSprite2d::GetFadeCurrentTime() );
	}
	fade::bInstant = bInstant;

	fade::bFadingIn = bFadeIn;

	m_bBlindersUp = bFadeIn;
	if(!bFadeIn)
	{
		if(m_bManualControl)
			m_bManualControl = false;

		m_iBlinderDelay = 0;
	}

	return true;
}
#endif	//USE_MULTIHEAD_FADE
//eof
