#include "ReplayMovieController.h"

#if GTA_REPLAY

// DON'T COMMIT
//OPTIMISATIONS_OFF()
#include "replay.h"
#include "ReplayRenderTargetHelper.h"

#include "vfx/misc/MovieManager.h"
#include "script/commands_graphics.h"
#include "script/commands_hud.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"

extern const char* pAcceptableRenderTargets[];
extern const char* pAcceptableTextureNames[];

bool						ReplayMovieController::m_bIsExiting = false;
bool						ReplayMovieController::m_bDoneClear = false;
atArray<ReplayMovieController::tMovieInfo>	ReplayMovieController::m_MoviesThisFrameInfo;
MovieDestinationInfo		ReplayMovieController::m_MovieDestThisFrameInfo;
u32							ReplayMovieController::m_RenderTargetId = 0;
u32							ReplayMovieController::m_CurrentTVModel = 0;
const char					*ReplayMovieController::pRenderTargetName = nullptr;

atString					ReplayMovieController::m_renderTargetName;
bool						ReplayMovieController::m_hasRegistered = false;
int							ReplayMovieController::m_linkToModelHashKey = 0;

bool						ReplayMovieController::m_isDrawingSprite = false;
atString					ReplayMovieController::m_texDictionaryName;
atString					ReplayMovieController::m_textureName;
float						ReplayMovieController::m_centreX = 0.5f;
float						ReplayMovieController::m_centreY = 0.5f;
float						ReplayMovieController::m_width = 100.0f;
float						ReplayMovieController::m_height = 100.0f;
float						ReplayMovieController::m_rotation = 0.0f;
int							ReplayMovieController::m_red = 255;
int							ReplayMovieController::m_green = 255;
int							ReplayMovieController::m_blue = 255;
int							ReplayMovieController::m_alpha = 255;
bool						ReplayMovieController::m_doStereorize = false;

atFixedArray<ReplayMovieController::AudioLinkageInfo,MOVIE_AUDIO_MAX_LINKAGE>	ReplayMovieController::m_AudioLinkInfo;

atArray<atFinalHashString>	ReplayMovieController::m_knownRenderTargets;
bool						ReplayMovieController::m_usedForMovies = false;

void ReplayMovieController::Init()
{
	m_knownRenderTargets.clear();
	m_knownRenderTargets.PushAndGrow(atFinalHashString("tvscreen"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("ex_tvscreen"));		// Used in the new offices
	m_knownRenderTargets.PushAndGrow(atFinalHashString("TV_Flat_01"));		// Used in hangar office
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Club_Projector"));	// Used in nightclub
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Screen_Tv_01"));	// Used in arena
	m_knownRenderTargets.PushAndGrow(atFinalHashString("bigscreen_01"));	// Used in arena
	m_knownRenderTargets.PushAndGrow(atFinalHashString("CasinoScreen_01"));	// Used in Casino main screen
	m_knownRenderTargets.PushAndGrow(atFinalHashString("TV_RT_01a"));		// Used in Casino Penthouse
}


void ReplayMovieController::HandleRenderTargets()
{
	u32 ownerHash = GetOwnerHash();

	// Check we're still linked?
	if( m_CurrentTVModel != 0 )
	{
		fwModelId modelId;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_CurrentTVModel,&modelId);
		if(pModelInfo)
		{
			if( gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), ownerHash) == false )
			{
				// Something's gone tit's up, release this rendertarget and let the code below acquire a new one.
				ReleaseRenderTarget(pRenderTargetName, ownerHash);
				pRenderTargetName = nullptr;
				m_CurrentTVModel = 0;
			}
		}
	}

	// Is the currently linked TV model the one we want to be playing on?
	if( m_CurrentTVModel != m_MovieDestThisFrameInfo.m_TargetModelHash )
	{
		// No
		// Is the currently linked model valid?
		if( m_CurrentTVModel == 0 )
		{
			// No
			// We should not have a valid rendertarget ID by this time
			Assert(m_RenderTargetId == 0);
			// No, acquire a rendertargetID for this models rendertarget
			int i = 0;
			while(i < m_knownRenderTargets.GetCount() && m_RenderTargetId == 0)
			{
				m_RenderTargetId = LinkToRenderTarget(m_knownRenderTargets[i].GetCStr(), ownerHash, m_MovieDestThisFrameInfo.m_TargetModelHash);
				if(m_RenderTargetId)
					pRenderTargetName = m_knownRenderTargets[i].GetCStr();
				++i;
			}
			// This may fail
			if( m_RenderTargetId != 0 )
			{
				m_CurrentTVModel = m_MovieDestThisFrameInfo.m_TargetModelHash;
				// Clear on creation, because cleanup doesn't get to wait for a frame due to replay constraints (TODO)
				BlankRenderTarget();
			}
		}
		else
		{
			// Yes
			// Are we awaiting a clear draw?
			if( m_bDoneClear == false )
			{
				// No, so we clear the current render target, & wait 1 frame for that to happen
				BlankRenderTarget();
				m_bDoneClear = true;
			}
			else
			{
				// Yes, a frame must have rendered, so we can now remove the rendertarget
				m_bDoneClear = false;
				ReleaseRenderTarget(pRenderTargetName, ownerHash);
				pRenderTargetName = nullptr;
				m_CurrentTVModel = 0;
			}
		}
	}
}

void	ReplayMovieController::OnEntry()
{
	Displayf("ReplayMovieController: OnEntry() called");
	// Reset Any start data
	ClearMoviesThisFrame();
	m_MovieDestThisFrameInfo.m_TargetModelHash = (u32)0;
	m_MovieDestThisFrameInfo.m_MovieHandle = 0;
	m_bDoneClear = 0;
	m_CurrentTVModel = 0;
	// Clear out all movies currently playing
	g_movieMgr.ReleaseAll();
	m_AudioLinkInfo.Reset();
	pRenderTargetName = nullptr;
	m_bIsExiting = false;

	m_isDrawingSprite = false;
	m_usedForMovies = false;
}

void	ReplayMovieController::OnExit()
{
	Displayf("ReplayMovieController: OnExit() called");

	if(m_texDictionaryName.GetLength() > 0)
	{
		// Free up the texture if we loaded it
		strLocalIndex index = g_TxdStore.FindSlot(m_texDictionaryName.c_str());
		if(g_TxdStore.HasObjectLoaded(index))
		{
			CStreaming::ClearRequiredFlag(index, g_TxdStore.GetStreamingModuleId(), STRFLAG_DONTDELETE);
			CStreaming::RemoveObject(index, g_TxdStore.GetStreamingModuleId());
		}
		m_texDictionaryName = "";
	}

	// Clear out all movies currently playing
	g_movieMgr.ReleaseAll();
	ReleaseRenderTarget(pRenderTargetName, GetOwnerHash());
	pRenderTargetName = nullptr;
	m_AudioLinkInfo.Reset();
	m_MovieDestThisFrameInfo.m_TargetModelHash = (u32)0;
	m_bIsExiting = true;

	m_isDrawingSprite = false;
}

void	ReplayMovieController::Process(bool bPlaying)
{
	if( m_bIsExiting )
		return;

	// Purge any movies that aren't required
	PurgeUnusedMovies();

	// Load any movies that are required.
	LoadRequiredMovies();

	// Update whatever we're supposed to be pointing to
	HandleRenderTargets();

	// If we're playing, start any movies that should be playing
	// otherwise stop movies that are playing.
	if(bPlaying)
	{
		// Start any movies that should be playing
		StartPlayingMovies();
		// Only draw any playing movie to any rendertarget if we're playing forward.
		DrawMovieToRenderTarget();
	}
	else
	{
		// Stop all movies that are playing
		StopPlayingMovies();
	}
}

void	ReplayMovieController::PurgeUnusedMovies()
{
	if(!m_usedForMovies)
		return;

	// Find all the movies that are loaded, but shouldn't be this frame
	for(int i=0;i<MAX_ACTIVE_MOVIES;i++)
	{
		u32	handle = g_movieMgr.GetHandle(i);

		bool	found = false;
		if(handle != INVALID_MOVIE_HANDLE)
		{
			// Whizz through our list of movies, if this one ain't there, bin it.
			for(int j=0;j<m_MoviesThisFrameInfo.size();j++)
			{
				tMovieInfo &theMovie = m_MoviesThisFrameInfo[j];
				u32	theMovieHash = atStringHash(theMovie.movieName);

				if( handle == theMovieHash )
				{
					found = true;
					break;
				}
			}
			if(!found)
			{
				// Remove this movie
				Displayf("ReplayMovieController: Releasing Movie 0x%x", handle );
				g_movieMgr.Release(handle);
			}
		}
	}
}

void	ReplayMovieController::LoadRequiredMovies()
{
	// Now load any movies we really want
	for(int i=0;i<m_MoviesThisFrameInfo.size();i++)
	{
		u32	handle = INVALID_MOVIE_HANDLE;

		tMovieInfo &theMovie = m_MoviesThisFrameInfo[i];
		u32	theMovieHash = atStringHash(theMovie.movieName);

		// See if already loaded
		bool found = false;
		for(int j=0;j<MAX_ACTIVE_MOVIES;j++)
		{
			handle =  g_movieMgr.GetHandle(j);
			if( handle == theMovieHash )
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			// Wasn't found, load it
			handle = g_movieMgr.PreloadMovie(theMovie.movieName);
			Displayf("ReplayMovieController: Loading Movie 0x%x", handle);
		}
	}
}

void	ReplayMovieController::ClearMoviesThisFrame()
{
	//	Reset our list of movies
	m_MoviesThisFrameInfo.Reset();
}

void	ReplayMovieController::SubmitMoviesThisFrame(const atFixedArray<tMovieInfo, MAX_ACTIVE_MOVIES> &info)
{
	ClearMoviesThisFrame();

	// Loop through the movies we have registered
	for(int i=0;i<info.size();i++)
	{
		const tMovieInfo &thisMovie = info[i];
		m_MoviesThisFrameInfo.PushAndGrow( thisMovie );
	}

	if(m_MoviesThisFrameInfo.GetCount() > 0)
	{
		m_isDrawingSprite = false;
	}

	m_usedForMovies = true;
}

void	ReplayMovieController::SubmitMovieTargetThisFrame(const MovieDestinationInfo &info)
{
	m_MoviesThisFrameInfo.Reset();
	m_MovieDestThisFrameInfo = info;

	if(m_MovieDestThisFrameInfo.m_MovieHandle != 0)
	{
		m_isDrawingSprite = false;
	}

	m_usedForMovies = true;
}


void ReplayMovieController::SubmitSpriteThisFrame(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize)
{
	m_texDictionaryName = texDictName;
	m_textureName = textName;
	m_centreX = centreX;
	m_centreY = centreY;
	m_width = width;
	m_height = height;
	m_rotation = rotation;
	m_red = r;
	m_green = g;
	m_blue = b;
	m_alpha = a;
	m_doStereorize = doStereorize;

	m_isDrawingSprite = true;
}

void	ReplayMovieController::StartPlayingMovies()
{
	for(int i=0;i<m_MoviesThisFrameInfo.size();i++)
	{
		const tMovieInfo &thisMovie = m_MoviesThisFrameInfo[i];

		// Was this movie playing at this time?
		u32	thisMovieHash = atStringHash(thisMovie.movieName);
		if( thisMovie.isPlaying )
		{
			if( g_movieMgr.IsLoaded(thisMovieHash) && g_movieMgr.IsPlaying(thisMovieHash) == false )
			{
				// Setup any audio linkage
				LinkMovieAudio(thisMovieHash);

				Displayf("ReplayMovieController: Starting Movie %s (0x%x) at time %f", thisMovie.movieName, thisMovieHash, thisMovie.movieTime);
				g_movieMgr.Play(thisMovieHash);
				g_movieMgr.SetTimeReal(thisMovieHash, thisMovie.movieTime);
			}
		}
		else
		{
			if( g_movieMgr.IsLoaded(thisMovieHash) && g_movieMgr.IsPlaying(thisMovieHash) == true )
			{
				g_movieMgr.Stop(thisMovieHash);
			}
		}
	}
}

void	ReplayMovieController::RegisterMovieAudioLink(u32 modelNameHash, Vector3 &TVPos, u32 movieNameHash, bool isFrontendAudio)
{
	// See if we already have one registered
	for(int i=0;i<m_AudioLinkInfo.size();i++)
	{
		AudioLinkageInfo &linkInfo = m_AudioLinkInfo[i];
		if( linkInfo.movieNameHash == movieNameHash )
		{
			linkInfo.modelNameHash = modelNameHash;
			linkInfo.pos = TVPos;
			linkInfo.isFrontendAudio = isFrontendAudio;
			return;
		}
	}

	// Didn't find it, add it, if we can
	if( m_AudioLinkInfo.IsFull() == false )
	{
		AudioLinkageInfo thisInfo;
		thisInfo.modelNameHash = modelNameHash;
		thisInfo.pos = TVPos;
		thisInfo.movieNameHash = movieNameHash;
		thisInfo.isFrontendAudio = isFrontendAudio;
		m_AudioLinkInfo.Append() = thisInfo;
	}
}

void	ReplayMovieController::LinkMovieAudio(u32 movieNameHash)
{
	AudioLinkageInfo *pLinkInfo = NULL;
	for(int i=0;i<m_AudioLinkInfo.size();i++)
	{
		AudioLinkageInfo *pTempInfo = &m_AudioLinkInfo[i];
		if( pTempInfo->movieNameHash == movieNameHash )
		{
			pLinkInfo = pTempInfo;
			break;
		}
	}

	if( pLinkInfo )
	{
		// HACK, specific to Micheals Lounge TV, needed due to the way the TV works at this location, is a bit of a special case.
		if( pLinkInfo->modelNameHash == ATSTRINGHASH("v_ilev_mm_scre_off",0x72ad642d) )
		{
			pLinkInfo->modelNameHash = ATSTRINGHASH("v_ilev_mm_screen2",0x122ffaf4);
		}
		// HACK

		// Only search for the entity as a last resort
		// Find our movie and get it's entity
		// If we can't find the movie, then we can't link anyway
		const CMovieMgr::CMovie *pMovie = g_movieMgr.FindMovie(movieNameHash);
		if(pMovie)
		{
			// Don't set any linkage until the audio is told to be playing.
			// Get the entity this is linked to
			CEntity *pEntity = pMovie->GetEntity();
			if( pEntity )
			{
				// See if this is the same one we want to link to?
				CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
				if( pModelInfo )
				{
					if( pModelInfo->GetModelNameHash() == pLinkInfo->modelNameHash )
					{
						// Check coord.
						Vector3 pos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
						float dist = (pos - pLinkInfo->pos).Mag2();
						if( dist < (MOVIE_AUDIO_TV_SEARCH_RADIUS * MOVIE_AUDIO_TV_SEARCH_RADIUS) )
						{
							// Yep, already linked.
							return;
						}
					}
				}
			}

			// We didn't early out because we're already linked, so link now
			pEntity = FindEntity(pLinkInfo->modelNameHash, MOVIE_AUDIO_TV_SEARCH_RADIUS, pLinkInfo->pos);
			if( pEntity )
			{
				//Set the entity on the movie manager
				g_movieMgr.AttachAudioToEntity(movieNameHash, pEntity);

				//Set if it's a frontend played audio
				g_movieMgr.SetFrontendAudio(movieNameHash, pLinkInfo->isFrontendAudio);
			}
		}
	}
}



CEntity *ReplayMovieController::FindEntity(const u32 modelNameHash, float searchRadius, Vector3 &searchPos)
{
	ReplayClosestMovieObjectSearchStruct findStruct;

	findStruct.pEntity = NULL;
	findStruct.closestDistanceSquared = searchRadius * searchRadius;	//	remember it's squared distance
	findStruct.coordToCalculateDistanceFrom = searchPos;
	findStruct.modelNameHashToCheck = modelNameHash;

	//	Check range
	spdSphere testSphere(RCC_VEC3V(	findStruct.coordToCalculateDistanceFrom), ScalarV(searchRadius));
	fwIsSphereIntersecting searchSphere(testSphere);
	CGameWorld::ForAllEntitiesIntersecting(	&searchSphere, 
		GetClosestObjectCB, 
		(void*)&findStruct, 
		ENTITY_TYPE_MASK_OBJECT, 
		SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS, 
		SEARCH_LODTYPE_HIGHDETAIL, 
		SEARCH_OPTION_DYNAMICS_ONLY | SEARCH_OPTION_SKIP_PROCEDURAL_ENTITIES, 
		WORLDREP_SEARCHMODULE_REPLAY );
	return findStruct.pEntity;
}

bool ReplayMovieController::GetClosestObjectCB(CEntity* pEntity, void* data)
{
	ReplayClosestMovieObjectSearchStruct *pInfo = reinterpret_cast<ReplayClosestMovieObjectSearchStruct*>(data);

	if(pEntity)
	{
		CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
		if(pModelInfo)
		{
			if( pModelInfo->GetModelNameHash() == pInfo->modelNameHashToCheck )
			{
				// It's the right model
				//Get the distance between the centre of the locate and the entities position
				Vector3 DiffVector = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - pInfo->coordToCalculateDistanceFrom;
				float ObjDistanceSquared = DiffVector.Mag2();

				//If the the distance is less than the radius store this as our entity
				if (ObjDistanceSquared < pInfo->closestDistanceSquared)
				{
					pInfo->pEntity = pEntity;
					pInfo->closestDistanceSquared = ObjDistanceSquared;
				}
			}
		}
	}
	return true;
}


void	ReplayMovieController::StopPlayingMovies()
{
	for(int i=0;i<m_MoviesThisFrameInfo.size();i++)
	{
		const tMovieInfo &thisMovie = m_MoviesThisFrameInfo[i];
		// Was this movie playing at this time?
		if( thisMovie.isPlaying )
		{
			u32	thisMovieHash = atStringHash(thisMovie.movieName);
			if( g_movieMgr.IsPlaying(thisMovieHash) )
			{
				Displayf("ReplayMovieController: Stopping Movie %s (0x%x)", thisMovie.movieName, thisMovieHash);
				g_movieMgr.Stop(thisMovieHash);
			}
		}
	}
}


//	renderTargetName = name of the render target
//	ownerHash = name of the target owner
//	modelHash = name of the model
u32 ReplayMovieController::LinkToRenderTarget(const char* renderTargetName, u32 ownerHash, u32 modelHash)
{
	atFinalHashString hashStr(renderTargetName);

	if (gRenderTargetMgr.GetNamedRendertarget(hashStr, ownerHash) == NULL)
	{
		gRenderTargetMgr.RegisterNamedRendertarget(renderTargetName, ownerHash);
	}

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelHash,&modelId);
	if(pModelInfo)
	{
		if( pModelInfo->GetDrawable())
		{
			if(!gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), ownerHash))
			{
				gRenderTargetMgr.LinkNamedRendertargets(modelId.GetModelIndex(), ownerHash);
			}

			if(gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), ownerHash))
			{
				u32 renderTarget = gRenderTargetMgr.GetNamedRendertargetId(renderTargetName, ownerHash, modelId.GetModelIndex());
				return renderTarget; 
			}
		}
	}
	return 0; 
}

void ReplayMovieController::ReleaseRenderTarget(const char* renderTargetName, u32 ownerHash)
{
	if(!renderTargetName)
		return;

	atFinalHashString hashStr(renderTargetName);

	if (gRenderTargetMgr.GetNamedRendertarget(hashStr, ownerHash) != NULL)
	{
		gRenderTargetMgr.ReleaseNamedRendertarget(hashStr, ownerHash);
	}
	m_RenderTargetId = 0;
}



void ReplayMovieController::DrawMovieToRenderTarget()
{
	/*
	// DBEUG, render to debug texture
	if(m_MovieDestThisFrameInfo.m_MovieHandle)
	{
		g_movieMgr.ms_curMovieHandle = m_MovieDestThisFrameInfo.m_MovieHandle;
		g_movieMgr.ms_debugRender = true;
		g_movieMgr.ms_movieSize = 25;
	}
	*/

	if( m_RenderTargetId != 0 )
	{
		if( m_MovieDestThisFrameInfo.m_MovieHandle != 0 )
		{
			// SCRIPT_GFX_VISIBLE_WHEN_PAUSED has to be set because intro_script_rectangle::Draw() checks of if the pause menu is active, which it is when viewing previews in the replay editor.
			CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD | SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

			// We have a renderTargetID, render
			CScriptHud::scriptTextRenderID = m_RenderTargetId;
			gRenderTargetMgr.UseRenderTargetForReplay((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
			CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
			
			Color32 colour = m_MovieDestThisFrameInfo.m_colour;
			graphics_commands::DrawSpriteTex(NULL, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, colour.GetRed(), colour.GetGreen(), colour.GetBlue(), colour.GetAlpha(), m_MovieDestThisFrameInfo.m_MovieHandle, SCRIPT_GFX_SPRITE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
		}
		else if(m_isDrawingSprite)
		{
			strLocalIndex index = g_TxdStore.FindSlot(m_texDictionaryName.c_str());
			if(!g_TxdStore.HasObjectLoaded(index))
			{
				g_TxdStore.StreamingRequest(index, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			}
			else
			{
				CScriptHud::scriptTextRenderID = m_RenderTargetId;
				gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
				CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;

				// SET_SCRIPT_GFX_DRAW_ORDER(GFX_ORDER_AFTER_HUD)
				CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
				// SET_SCRIPT_GFX_DRAW_BEHIND_PAUSEMENU(TRUE)
				CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

				// DRAW_SPRITE_NAMED_RENDERTARGET(...)
				grcTexture* pTextureToDraw = graphics_commands::GetTextureFromStreamedTxd(m_texDictionaryName.c_str(), m_textureName.c_str());

				EScriptGfxType gfxType = SCRIPT_GFX_SPRITE_NON_INTERFACE;

				graphics_commands::DrawSpriteTex(pTextureToDraw, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, INVALID_MOVIE_HANDLE, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false, false);
			}
		}
		else
		{
			BlankRenderTarget();
		}
	}
}


void ReplayMovieController::BlankRenderTarget()
{
	if( AssertVerify(m_RenderTargetId != 0) )
	{
		CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD | SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
		// Clear the rectangle
		hud_commands::CommandSetTextRenderId((CRenderTargetMgr::RenderTargetId)m_RenderTargetId);
		graphics_commands::CommandDrawRect(0.5f, 0.5f, 1.0f, 1.0f, 0, 0, 0, 255);
	}
}


void ReplayMovieController::OnRegisterNamedRenderTarget(const char *name)
{
	m_renderTargetName = name;

	replayDebugf1("ReplayMovieController::OnRegisterNamedRenderTarget %s", name);

	m_hasRegistered = true;
}


void ReplayMovieController::OnReleaseNamedRenderTarget(const char *name)
{
	if(m_renderTargetName == name)
	{
		m_renderTargetName = "";
		m_hasRegistered = false;
		m_linkToModelHashKey = 0;
	}

}

void ReplayMovieController::OnLinkNamedRenderTarget(int ModelHashKey, u32 /*ownerID*/)
{
	if(m_hasRegistered)
	{
		m_linkToModelHashKey = ModelHashKey;
	}
}

void ReplayMovieController::Record()
{
	if(m_linkToModelHashKey)
	{
		CReplayMgr::RecordFx<CPacketMovieTarget>(CPacketMovieTarget(m_linkToModelHashKey, 0, Color32(1.0f, 1.0f, 1.0f, 1.0f)));
	}
}

void ReplayMovieController::OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize)
{
	m_texDictionaryName = pTextureDictionaryName;
	m_textureName = pTextureName;
	m_centreX = CentreX;
	m_centreY = CentreY;
	m_width = Width;
	m_height = Height;
	m_rotation = Rotation;
	m_red = R;
	m_green = G;
	m_blue = B;
	m_alpha = A;
	m_doStereorize = DoStereorize;



	bool recordPacket = false;

	int texIndex = 0;
	const char* ptexName = pAcceptableTextureNames[0];
	while(ptexName)
	{
		if(m_textureName.StartsWith(ptexName))
		{
			recordPacket = true;
			break;
		}

		ptexName = pAcceptableTextureNames[++texIndex];
	}

	if(m_texDictionaryName.StartsWith("Prop_Screen_Arena_Giant"))
	{
		recordPacket = true;
	}

	if(recordPacket)
	{
		CReplayMgr::RecordFx(CPacketDrawSprite(m_texDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize, 0));

#if 0
		replayDebugf1("CReplayRenderTargetHelper::OnDrawSprite");
		replayDebugf1("  m_textureDictionaryName %s", m_texDictionaryName.c_str());
		replayDebugf1("  m_textureName %s", m_textureName.c_str());
		replayDebugf1("  m_centreX %f", m_centreX);
		replayDebugf1("  m_centreY %f", m_centreY);
		replayDebugf1("  m_width %f", m_width);
		replayDebugf1("  m_height %f", m_height);
		replayDebugf1("  m_rotation %f", m_rotation);
		replayDebugf1("  m_red %d", m_red);
		replayDebugf1("  m_green %d", m_green);
		replayDebugf1("  m_blue %d", m_blue);
		replayDebugf1("  m_alpha %d", m_alpha);
		replayDebugf1("  m_doStereorize %s", m_doStereorize ? "true" : "false");
#endif

		m_texDictionaryName = "";
		m_textureName = "";
		m_centreX = 0.0f;
		m_centreY = 0.0f;
		m_width = 0.0f;
		m_height = 0.0f;
		m_rotation = 0.0f;
		m_red = 0;
		m_green = 0;
		m_blue = 0;
		m_alpha = 0;
		m_doStereorize = false;
	}
}

#endif	//GTA_REPLAY