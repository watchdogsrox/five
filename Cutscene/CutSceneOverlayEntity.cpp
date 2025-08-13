/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsSceneOverlayEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutscene/cutsevent.h"
#include "cutscene/cutseventargs.h"

//Game Headers
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneOverlayEntity.h"
#include "cutscene/cutscene_channel.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneCustomEvents.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "renderer/RenderTargetMgr.h"
#include "grcore/texture.h"
#include "script/commands_graphics.h"
#include "script/commands_hud.h"
#include "script/script_hud.h"
#include "vfx/misc/MovieManager.h"

#include "control/replay/ReplaySettings.h"

ANIM_OPTIMISATIONS()

#if GTA_REPLAY
u32		gCurrentCutsceneMovieHandle = 0;
#endif	//GTA_REPLAY

CCutSceneScaleformOverlayEntity::CCutSceneScaleformOverlayEntity (const cutfObject* pObject)//, s32 iAttachedEntityId)
:cutsUniqueEntity ( pObject )
{
	//m_OverlayId = -1; 
	m_OverlayId[0] = SF_INVALID_MOVIE; 
	m_OverlayId[1] = SF_INVALID_MOVIE; 
	m_OverlayIdLocal = SF_INVALID_MOVIE; 

	m_ShouldRender[0] = false; 
	m_ShouldRender[1] = false;
	m_LocalShouldRender = false; 

	m_bProcessShowEventPostSceneUpdate = false;
	m_bDisableShouldRenderInPostSceneUpdate = false;
	m_bIsStreamedOut = false; 

	m_RenderTargetId = CRenderTargetMgr::RTI_MainScreen;

	//should get from the object but for now just hard wire
	m_vMin.x = 0.0f; 
	m_vMin.y = 0.0f; 
	m_vMax.x = 1.0f; 
	m_vMax.y = 1.0f; 

	m_uRenderTargetSizeX = 0;
	m_uRenderTargetSizeY = 0;

	m_bUseMainRenderTarget = true; 

	m_pCutfOverlayObject = static_cast<const cutfOverlayObject*> (pObject); 
	m_bStreamOutFullScreenRenderTargetNextUpdate = false; 

	m_streamOutFrameCount = 0; 
}

CCutSceneScaleformOverlayEntity::~CCutSceneScaleformOverlayEntity ()
{	
}

void CCutSceneScaleformOverlayEntity::StreamOutOverlay(cutsManager* pManager, const cutfObject* pObject)
{
	if(!m_bIsStreamedOut)
	{
		CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

		if(pManager)
		{	
			CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 
			if(pAssetManager)
			{
				if(pAssetManager->GetGameIndex(pObject->GetObjectId()) != SF_INVALID_MOVIE)
				{
					pAssetManager->RemoveStreamingRequest(pObject->GetObjectId(), true, SCALEFORM_OVERLAY_TYPE);
				}
				m_OverlayIdLocal = SF_INVALID_MOVIE; 
				m_LocalShouldRender = false;
				m_bIsStreamedOut = true; 
			}
		}
	}
}

void CCutSceneScaleformOverlayEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs), const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	//Sets the overlay to display, 
	case CUTSCENE_SHOW_OVERLAY_EVENT:
		{
			cutsceneScaleformOverlayEntityDebugf("CUTSCENE_SHOW_OVERLAY_EVENT (SCALEFORM)");

			// NOTE...
			// Scaleforms being rendered to specific rendertargets get processed here now.
			// Scaleforms being rendered to the main, full screen rendertarget are processed in the PostSceneUpdate (see CUTSCENE_UPDATE_EVENT)

			// Does this render to a specific rendertarget?
			const char* RendertargetName = m_pCutfOverlayObject->GetRenderTargetName(); 
			u32 ModelHash = m_pCutfOverlayObject->GetModelHashName();

			if(strcmp(RendertargetName,"") !=0 && ModelHash)
			{
				// Render target
				m_bProcessShowEventPostSceneUpdate = false;

				CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager);

				u32 uOwnerHash = GetRenderTargetOwnerTag();

				m_RenderTargetId = CCutSceneBinkOverlayEntity::LinkToRenderTarget(uOwnerHash, RendertargetName, ModelHash);

				// Rendering into a rendertarget.  This might not be the same size as the screen resolution, so scale accordingly.
				atFinalHashString hashStr(RendertargetName);
				CRenderTargetMgr::namedRendertarget* pRenderTarget = gRenderTargetMgr.GetNamedRendertarget(hashStr, uOwnerHash);
				if (pRenderTarget && pRenderTarget->texture)
				{
					m_uRenderTargetSizeX = (u32)pRenderTarget->texture->GetWidth();
					m_uRenderTargetSizeY = (u32)pRenderTarget->texture->GetHeight();
				}

				if(pManager)
				{	
					CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 
					if(pAssetManager)
					{
						if(pAssetManager->GetGameIndex(pObject->GetObjectId()) != SF_INVALID_MOVIE)
						{
							m_OverlayIdLocal = pAssetManager->GetGameIndex(pObject->GetObjectId()); 
							m_LocalShouldRender = true; 
						}
					}
				}
			}
			else
			{
				// Full screen
				m_bProcessShowEventPostSceneUpdate = true;
			}
			SetOverlayId(m_OverlayIdLocal);
			SetShouldRender(m_LocalShouldRender); 

		}
		break;

	case CUTSCENE_HIDE_OVERLAY_EVENT:
		{
			cutsceneScaleformOverlayEntityDebugf("CUTSCENE_HIDE_OVERLAY_EVENT (SCALEFORM)");

			//need to decide to look a head and if the move is not used again the stream out 
			bool bStreamOutOverlay = true; 
			
			atArray<cutfEvent *> objectEventList;
			const cutfCutsceneFile2* pCutfile = const_cast<const cutsManager*>(pManager)->GetCutfFile(); 
			
			pCutfile->FindEventsForObjectIdOnly( pObject->GetObjectId(), pCutfile->GetEventList(), objectEventList );
			
			//look at the up coming events for this object and see if the object needs to be displayed again if so dont stream it
			const char* RendertargetName = m_pCutfOverlayObject->GetRenderTargetName(); 
			u32 ModelHash = m_pCutfOverlayObject->GetModelHashName();
			bool isFullScreen = true; 

			if(strcmp(RendertargetName,"") !=0 && ModelHash)
			{
				isFullScreen = false; 
			}

			for(int i=0; i<objectEventList.GetCount(); i++)
			{
				if(objectEventList[i]->GetTime() > pManager->GetTime())
				{
					if(objectEventList[i]->GetEventId() == CUTSCENE_SHOW_OVERLAY_EVENT)
					{
						bStreamOutOverlay = false;	//looked ahead and there is another show event so dont stream out 
						break; 
					}
				}
			}
			
			if(!isFullScreen)
			{
				if(bStreamOutOverlay)
				{
					StreamOutOverlay(pManager, pObject); 
				}
				else
				{
					m_LocalShouldRender = false; 
				}
			}
			else
			{
				if(bStreamOutOverlay)
				{
					m_bStreamOutFullScreenRenderTargetNextUpdate = true; 
					m_LocalShouldRender = false;
					m_OverlayIdLocal = SF_INVALID_MOVIE; 
					m_streamOutFrameCount = fwTimer::GetFrameCount(); 

				}
				else
				{
					m_LocalShouldRender = false; 
				}
			}

			SetOverlayId(m_OverlayIdLocal);
			SetShouldRender(m_LocalShouldRender); 
		}
		break;

	case CUTSCENE_UPDATE_EVENT:
		{
			CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
			if (pCutManager->GetPostSceneUpdate())
			{
				if(m_bStreamOutFullScreenRenderTargetNextUpdate && fwTimer::GetFrameCount() > m_streamOutFrameCount )
				{
					StreamOutOverlay(pManager, pObject); 
					m_bStreamOutFullScreenRenderTargetNextUpdate = false; 
					Displayf("Stream out called: %d", fwTimer::GetFrameCount()); 
				}

				// Full screen scaleform - Show
				if (m_bProcessShowEventPostSceneUpdate)
				{
					m_bProcessShowEventPostSceneUpdate = false;

					gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);

					if(pManager)
					{
						CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager);
						CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 

						if(pAssetManager)
						{
							if(pAssetManager->GetGameIndex(pObject->GetObjectId()) != SF_INVALID_MOVIE)
							{
								m_OverlayIdLocal = pAssetManager->GetGameIndex(pObject->GetObjectId());
								m_LocalShouldRender = true;
							}
						}
					}
				}
			}
			SetOverlayId(m_OverlayIdLocal);
			SetShouldRender(m_LocalShouldRender); 
		}
		break; 

		case CUTSCENE_PAUSE_EVENT:
			{
				SetOverlayId(m_OverlayIdLocal);
				SetShouldRender(m_LocalShouldRender); 
			}
			break; 

	case CUTSCENE_END_OF_SCENE_EVENT:
		{
			m_LocalShouldRender = false; 
			SetShouldRender(m_LocalShouldRender); 
			
			cutsceneScaleformOverlayEntityDebugf("CUTSCENE_END_OF_SCENE_EVENT (SCALEFORM)");

			atFinalHashString hashStr(m_pCutfOverlayObject->GetRenderTargetName());

			u32 uOwnerHash = GetRenderTargetOwnerTag();

			if (gRenderTargetMgr.GetNamedRendertarget(hashStr, uOwnerHash) != NULL)
			{
				gRenderTargetMgr.ReleaseNamedRendertarget(hashStr, uOwnerHash);
			}
		}
		break;
	} //END OF SWITCH
}

void CCutSceneScaleformOverlayEntity::GetAttributesFromArgs(const cutfEventArgs* pEventArgs, atArray<float>* fValue, atArray<const char*> *cValue,  
												   atArray<int> *iValue, atArray<bool> *bValue)
{
	for(int i =0; i<MAX_NUMBER_OVERLAY_PARAMS; i++)
	{
		char ParamName [32] ; 
		formatf(ParamName, 32, "param_%d", i+1); 
		const parAttribute* pAttribute = pEventArgs->GetAttributeList().FindAttributeAnyCase(ParamName); 
		if(pAttribute)
		{
			switch(pAttribute->GetType())
			{
			case parAttribute::DOUBLE:
				{
					fValue->PushAndGrow(pAttribute->GetFloatValue()); 
				}
				break; 
			
			case parAttribute::STRING:
				{
					cValue->PushAndGrow(pAttribute->GetStringValue()); 
				}
				break;
			case parAttribute::BOOL:
				{
					bValue->PushAndGrow(pAttribute->GetBoolValue()); 
				}
				break; 

			case parAttribute::INT64:
				{
					iValue->PushAndGrow(pAttribute->GetIntValue()); 
				}
				break; 
			}
			
		}
	}
}

void CCutSceneScaleformOverlayEntity::RenderOverlayToMainRenderTarget() const
{

	if(!GetShouldRender() || GetOverlayId() == SF_INVALID_MOVIE || m_RenderTargetId != CRenderTargetMgr::RTI_MainScreen)// || /*!CScaleformMgr::IsMovieActive(GetOverlayId())*/)
	{		 
		return; 
	}

	RenderScaleformFullScreen();
}

void CCutSceneScaleformOverlayEntity::RenderOverlayToRenderTarget() const
{
	if(!GetShouldRender() || GetOverlayId() == SF_INVALID_MOVIE || m_RenderTargetId == CRenderTargetMgr::RTI_MainScreen || !CScaleformMgr::IsMovieActive(GetOverlayId()))
	{
		return; 
	}

	RenderScaleformRenderTarget(); 
}

u32 CCutSceneScaleformOverlayEntity::GetRenderTargetOwnerTag()
{
	static u32 baseTag = atPartialStringHash("CUTSCENE_OVERLAY_SCALEFORM_OWNER_");

	u32 fullTag = atPartialStringHash(atVarString("%d", GetOverlayObject()->GetObjectId()).c_str(), baseTag);
	return atFinalizeHash(fullTag);
}

void CCutSceneScaleformOverlayEntity::RenderScaleformFullScreen() const
{
	Vector2 vSize(m_vMax.x - m_vMin.x, m_vMax.y - m_vMin.y);

	// set the pos & size of the movie:
	CScaleformMgr::ChangeMovieParams(GetOverlayId(), m_vMin, vSize, GFxMovieView::SM_ExactFit);

	// render the movie:
	CScaleformMgr::RenderMovie(GetOverlayId(), CutSceneManager::GetInstance()->GetCurrentCutSceneTimeStep());
}

void CCutSceneScaleformOverlayEntity::RenderScaleformRenderTarget() const
{
	Vector2 vSize(1.0f, 1.0f);
	if (m_uRenderTargetSizeX > 0 && m_uRenderTargetSizeY > 0 && CScaleformMgr::GetScreenSize().x > 0 && CScaleformMgr::GetScreenSize().y > 0)
	{
		vSize.x = (float)m_uRenderTargetSizeX / CScaleformMgr::GetScreenSize().x;
		vSize.y = (float)m_uRenderTargetSizeY / CScaleformMgr::GetScreenSize().y;
	}

	// set the pos & size of the movie:
	CScaleformMgr::ChangeMovieParams(GetOverlayId(), m_vMin, vSize, GFxMovieView::SM_ExactFit, -1, true);

	// render the movie:
	CScaleformMgr::RenderMovie(GetOverlayId(), CutSceneManager::GetInstance()->GetCurrentCutSceneTimeStep());
}

#if !__NO_OUTPUT

void CCutSceneScaleformOverlayEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Overlay Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}

#endif // !__NO_OUTPUT


//Bink 

CCutSceneBinkOverlayEntity::CCutSceneBinkOverlayEntity (const cutfObject* pObject)//, s32 iAttachedEntityId)
:cutsUniqueEntity ( pObject )
, m_RenderTargetId(CRenderTargetMgr::RTI_MainScreen)
, m_pCutfOverlayObject(static_cast<const cutfOverlayObject*> (pObject))
, m_BinkHandle(0)
, m_canRender(false)
{
}

CCutSceneBinkOverlayEntity::~CCutSceneBinkOverlayEntity()
{	
}

void CCutSceneBinkOverlayEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs), const float fTime, const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
		//Sets the overlay to display, 
	case CUTSCENE_SHOW_OVERLAY_EVENT:
		{
			cutsceneBinkOverlayEntityDebugf("CUTSCENE_SHOW_OVERLAY_EVENT (BINK)"); 

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

			if(m_pCutfOverlayObject->GetRenderTargetName() != NULL)
			{
				char modifiedRendertargetName [CUTSCENE_OBJNAMELEN] ; 			
				strncpy(modifiedRendertargetName, m_pCutfOverlayObject->GetRenderTargetName(), CUTSCENE_OBJNAMELEN);
				//put a terminating character where the 
				char *pChar = strrchr(modifiedRendertargetName, '.') ; 
			
				if(pChar != NULL)
				{
					*pChar = '\0'; 
				}

				u32 ModelHash = m_pCutfOverlayObject->GetModelHashName().GetHash(); 
		
				if(strcmp(modifiedRendertargetName,"") !=0 && ModelHash)
				{
					// Generate unique owner id - can't just use the scene hash, must be unique to the entity too
					u32 uOwnerHash = GetRenderTargetOwnerTag();
					m_RenderTargetId = LinkToRenderTarget(uOwnerHash, modifiedRendertargetName, ModelHash);
				}
			}

			if(pManager)
			{	
				CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 
				if(pAssetManager)
				{
					if(pAssetManager->GetGameIndex(pObject->GetObjectId()) != 0)
					{
						m_BinkHandle = pAssetManager->GetGameIndex(pObject->GetObjectId()); 
						
						if(m_BinkHandle != 0)
						{
							g_movieMgr.Play(m_BinkHandle);
							m_canRender = true;
#if GTA_REPLAY
							gCurrentCutsceneMovieHandle = m_BinkHandle;
#endif
						}
					}
				}
			}
		}
		break;

	case CUTSCENE_HIDE_OVERLAY_EVENT:
		{
			cutsceneBinkOverlayEntityDebugf("CUTSCENE_HIDE_OVERLAY_EVENT (BINK)");

			//need to decide to look a head and if the move is not used again the stream out 
			bool bStreamOutOverlay = true; 

			atArray<cutfEvent *> objectEventList;
			const cutfCutsceneFile2* pCutfile = const_cast<const cutsManager*>(pManager)->GetCutfFile(); 

			pCutfile->FindEventsForObjectIdOnly( pObject->GetObjectId(), pCutfile->GetEventList(), objectEventList );

			//look at the up coming events for this object and see if the object needs to be displayed again if so dont stream it
			for(int i=0; i<objectEventList.GetCount(); i++)
			{
				// A show event may be present for this object on the same frame as the hide.
				// So we use the hide event's time, not the current cutscene time when disregarding previous events.
				if(objectEventList[i]->GetTime() >= fTime)
				{
					if(objectEventList[i]->GetEventId() == CUTSCENE_SHOW_OVERLAY_EVENT)
					{
						bStreamOutOverlay = false;	//looked ahead and there is another show event so dont stream out 
						break; 
					}
				}
			}

			if(m_BinkHandle != 0)
			{

#if GTA_REPLAY
				gCurrentCutsceneMovieHandle = 0;
#endif

				g_movieMgr.Stop(m_BinkHandle);
				if(bStreamOutOverlay)
				{
					CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 
		
					if(pManager)
					{	
						CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 
						if(pAssetManager)
						{
							if(pAssetManager->GetGameIndex(pObject->GetObjectId()) != 0)
							{
								pAssetManager->RemoveStreamingRequest(pObject->GetObjectId(), true, BINK_OVERLAY_TYPE);

								atFinalHashString hashStr(m_pCutfOverlayObject->GetRenderTargetName());

								u32 uOwnerHash = GetRenderTargetOwnerTag();

								if (gRenderTargetMgr.GetNamedRendertarget(hashStr, uOwnerHash) != NULL)
								{
									// Clear it
									hud_commands::CommandSetTextRenderId((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
									graphics_commands::CommandDrawRect(0.5f, 0.5f, 1.0f, 1.0f, 0, 0, 0, 255);
								}
							}
							m_BinkHandle = 0; 
						}
					}
				}
			}
		}
		break;

	case CUTSCENE_END_OF_SCENE_EVENT:
		{
			cutsceneBinkOverlayEntityDebugf("CUTSCENE_END_OF_SCENE_EVENT (BINK)");

			atFinalHashString hashStr(m_pCutfOverlayObject->GetRenderTargetName());

			u32 uOwnerHash = GetRenderTargetOwnerTag();
			
			if (gRenderTargetMgr.GetNamedRendertarget(hashStr, uOwnerHash) != NULL)
			{
				gRenderTargetMgr.ReleaseNamedRendertarget(hashStr, uOwnerHash);
			}
		}
		break;
	}
}

u32 CCutSceneBinkOverlayEntity::LinkToRenderTarget(u32 OwnerHash, const char* RenderTargetName, u32 ModelHash)
{
	atFinalHashString hashStr(RenderTargetName);

	if (gRenderTargetMgr.GetNamedRendertarget(hashStr, OwnerHash) == NULL)
	{
		gRenderTargetMgr.RegisterNamedRendertarget(RenderTargetName, OwnerHash);
	}

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHash,&modelId);
	
	if(pModelInfo)
	{
		pModelInfo->SetCarryScriptedRT(true); 
		pModelInfo->SetScriptId(OwnerHash);

		// Only link (create actual rendertarget) if it's not already linked.
		// Calling this more than once can leak rendertargets.
		if( pModelInfo->GetDrawable() && !gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), OwnerHash))
		{
			gRenderTargetMgr.LinkNamedRendertargets(modelId.GetModelIndex(), OwnerHash);
		}

		u32 renderTarget = gRenderTargetMgr.GetNamedRendertargetId(RenderTargetName, OwnerHash);
		return renderTarget; 
	}

	return 0; 
}

u32 CCutSceneBinkOverlayEntity::GetRenderTargetOwnerTag() const
{
	static u32 baseTag = atPartialStringHash("CUTSCENE_OVERLAY_BINK_OWNER_");

	u32 fullTag = atPartialStringHash(atVarString("%d", GetOverlayObject()->GetObjectId()).c_str(), baseTag);
	return atFinalizeHash(fullTag);
}

void CCutSceneBinkOverlayEntity::RenderBinkMovie() const
{
	if(m_RenderTargetId != 0 && m_canRender)	
	{
		// Do we have a bink to render?  If not, then clear the rendertarget if it's not the main screen
		if ( m_BinkHandle != 0)
		{
			CScriptHud::scriptTextRenderID = m_RenderTargetId;
			gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
			CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;

			EScriptGfxType gfxType = (m_RenderTargetId == CRenderTargetMgr::RTI_MainScreen) ? SCRIPT_GFX_SPRITE : SCRIPT_GFX_SPRITE_NON_INTERFACE;
			graphics_commands::DrawSpriteTex(NULL, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 255, 255, 255, 255, m_BinkHandle, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
		}
		else
		{
			atFinalHashString hashStr(m_pCutfOverlayObject->GetRenderTargetName());

			u32 uOwnerHash = GetRenderTargetOwnerTag();

			if (gRenderTargetMgr.GetNamedRendertarget(hashStr, uOwnerHash) != NULL)
			{
				// Clear it if nobody else is using this rendertarget
				hud_commands::CommandSetTextRenderId((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
				graphics_commands::CommandDrawRect(0.5f, 0.5f, 1.0f, 1.0f, 0, 0, 0, 255);
			}
		}
	}
}

#if !__NO_OUTPUT

void CCutSceneBinkOverlayEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Overlay Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}

#endif // !__NO_OUTPUT

