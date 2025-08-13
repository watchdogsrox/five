#include "ReplayMovieControllerNew.h"

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

bool						ReplayMovieControllerNew::m_bIsExiting = false;
bool						ReplayMovieControllerNew::m_bDoneClear = false;
atArray<ReplayMovieControllerNew::tMovieInfo>	ReplayMovieControllerNew::m_MoviesThisFrameInfo;
atArray<ReplayMovieControllerNew::CRenderTargetInfo>	ReplayMovieControllerNew::m_playbackRenderTargets;

atFixedArray<ReplayMovieControllerNew::AudioLinkageInfo,MOVIE_AUDIO_MAX_LINKAGE>	ReplayMovieControllerNew::m_AudioLinkInfo;

atArray<int>	ReplayMovieControllerNew::m_renderTargetRegistersToRecord;
atArray<ReplayMovieControllerNew::rtLink>	ReplayMovieControllerNew::m_renderTargetLinksToRecord;

atArray<atFinalHashString> ReplayMovieControllerNew::m_knownRenderTargets;
atArray<atFinalHashString> ReplayMovieControllerNew::m_knownTextureDictionariesForSprites;
bool		ReplayMovieControllerNew::m_usedForMovies = false;

void ReplayMovieControllerNew::Init()
{
	m_knownRenderTargets.clear();
	m_knownRenderTargets.PushAndGrow(atFinalHashString("tvscreen"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("ex_tvscreen"));		// Used in the new offices
	m_knownRenderTargets.PushAndGrow(atFinalHashString("TV_Flat_01"));		// Used in hangar office
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Club_Projector"));	// Used in nightclub
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Screen_Tv_01"));	// Used in arena
	m_knownRenderTargets.PushAndGrow(atFinalHashString("bigscreen_01"));	// Used in arena
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Arcade_01a_screen"));	// Used in Casino Penthouse games machines
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Arcade_02a_screen"));	// Used in Casino Penthouse games machines
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Arcade_02b_screen"));	// Used in Casino Penthouse games machines
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Arcade_02c_screen"));	// Used in Casino Penthouse games machines
	m_knownRenderTargets.PushAndGrow(atFinalHashString("Arcade_02d_screen"));	// Used in Casino Penthouse games machines
	m_knownRenderTargets.PushAndGrow(atFinalHashString("TV_RT_01a"));		// Used in Casino Penthouse
	m_knownRenderTargets.PushAndGrow(atFinalHashString("CasinoScreen_01"));	// Used in Casino main screen
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_01"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_02"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_03"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_04"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_05"));
	m_knownRenderTargets.PushAndGrow(atFinalHashString("screen_06"));

	for(int i = 2; i < 19; ++i)
	{
		char rtName[32] = {0};
		formatf(rtName, "casinoscreen_%02d", i);
		m_knownRenderTargets.PushAndGrow(atFinalHashString(rtName));		// Used in Casino Inside Track
	}
	m_knownRenderTargets.PushAndGrow(atFinalHashString("ch_TV_RT_01a"));	// Used in the Arcades

	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("Prop_Screen_Arena_Giant"));
	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("Prop_Screen_VW_Arcade"));
	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("Prop_Screen_VW_InsideTrack"));
	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("MPSubmarineStation"));
	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("MPSubmarineMissileStation"));
	m_knownTextureDictionariesForSprites.PushAndGrow(atFinalHashString("MPSubmarineSonarStation"));
}


void ReplayMovieControllerNew::HandleRenderTargets()
{
	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
	{
		if(!rtInfo.m_renderTargetRegistered && rtInfo.m_renderTargetName.GetLength() != 0)
		{
			CRenderTargetMgr::namedRendertarget const* pRT = gRenderTargetMgr.GetNamedRendertarget(rtInfo.m_renderTargetName.GetHash());
			if(!pRT)
			{
				gRenderTargetMgr.RegisterNamedRendertarget(rtInfo.m_renderTargetName.GetCStr(), GetOwnerHash());
				pRT = gRenderTargetMgr.GetNamedRendertarget(rtInfo.m_renderTargetName.GetHash());
			}
			
			if(pRT)
			{
				if(replayVerifyf(pRT->owner == GetOwnerHash(), "Render Target is registered but wrong owner.  Was it cleared properly before entering Replay? (RT %s, Owner: %u)", pRT->name.GetCStr(), pRT->owner))
				{
					rtInfo.m_renderTargetRegistered = true;
				}
			}
		}
		
		if(rtInfo.m_renderTargetRegistered && !rtInfo.m_renderTargetLinked && rtInfo.m_linkModelHash != 0)
		{
			fwModelId modelId;
			CBaseModelInfo const* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(rtInfo.m_linkModelHash, &modelId);
			if(pModelInfo)
			{
				if(pModelInfo->GetDrawable())
				{
					if(!gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), GetOwnerHash()))
					{
						gRenderTargetMgr.LinkNamedRendertargets(modelId.GetModelIndex(), GetOwnerHash());
					}

					if(gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(), GetOwnerHash()))
					{
						rtInfo.m_renderTargetId = gRenderTargetMgr.GetNamedRendertargetId(rtInfo.m_renderTargetName.GetCStr(), GetOwnerHash(), modelId.GetModelIndex());
						rtInfo.m_renderTargetLinked = true;

						BlankRenderTarget(rtInfo.m_renderTargetId);
					}
				}
			}
		}
	}
}

void	ReplayMovieControllerNew::OnEntry()
{
	Displayf("ReplayMovieControllerNew: OnEntry() called");
	// Reset Any start data
	ClearMoviesThisFrame();
	m_bDoneClear = 0;
	// Clear out all movies currently playing
	g_movieMgr.ReleaseAll();
	m_AudioLinkInfo.Reset();
	//pRenderTargetName = nullptr;
	m_bIsExiting = false;

	m_usedForMovies = false;
}

void	ReplayMovieControllerNew::OnExit()
{
	Displayf("ReplayMovieControllerNew: OnExit() called");

	// Clear out all movies currently playing
	g_movieMgr.ReleaseAll();
	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
	{
		ReleaseRenderTarget(rtInfo.m_renderTargetName, GetOwnerHash(), rtInfo.m_renderTargetId);
	}
	m_playbackRenderTargets.Reset();
	
	m_AudioLinkInfo.Reset();
	m_bIsExiting = true;
}

void	ReplayMovieControllerNew::Process(bool bPlaying)
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
	}
	else
	{
		// Stop all movies that are playing
		StopPlayingMovies();
	}

	// Only draw any playing movie to any rendertarget if we're playing forward.
	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
	{
		DrawToRenderTarget(rtInfo);
	}
}

void	ReplayMovieControllerNew::PurgeUnusedMovies()
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
				Displayf("ReplayMovieControllerNew: Releasing Movie 0x%x", handle );
				g_movieMgr.Release(handle);
			}
		}
	}
}

void	ReplayMovieControllerNew::LoadRequiredMovies()
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
			Displayf("ReplayMovieControllerNew: Loading Movie 0x%x", handle);
		}
	}
}

void	ReplayMovieControllerNew::ClearMoviesThisFrame()
{
	//	Reset our list of movies
	m_MoviesThisFrameInfo.Reset();
}

void	ReplayMovieControllerNew::SubmitMoviesThisFrame(const atFixedArray<tMovieInfo, MAX_ACTIVE_MOVIES> &info)
{
	ClearMoviesThisFrame();

	// Loop through the movies we have registered
	for(int i=0;i<info.size();i++)
	{
		const tMovieInfo &thisMovie = info[i];
		m_MoviesThisFrameInfo.PushAndGrow( thisMovie );
	}

	m_usedForMovies = true;
}

void	ReplayMovieControllerNew::SubmitMovieTargetThisFrame(const MovieDestinationInfo &info)
{
	m_MoviesThisFrameInfo.Reset();
 	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
 	{
		if(rtInfo.m_linkModelHash == info.m_TargetModelHash)
		{
			rtInfo.m_movieDestThisFrameInfo = info;
			rtInfo.m_isDrawingSprite = false;
			break;
		}
 	}

	m_usedForMovies = true;
}


void ReplayMovieControllerNew::SubmitSpriteThisFrame(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex)
{
	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
	{
		if(rtInfo.m_renderTargetIndex == renderTargetIndex)
		{
			rtInfo.m_drawableSpriteInfo.m_texDictionaryName = texDictName;
			rtInfo.m_drawableSpriteInfo.m_textureName = textName;
			rtInfo.m_drawableSpriteInfo.m_centreX = centreX;
			rtInfo.m_drawableSpriteInfo.m_centreY = centreY;
			rtInfo.m_drawableSpriteInfo.m_width = width;
			rtInfo.m_drawableSpriteInfo.m_height = height;
			rtInfo.m_drawableSpriteInfo.m_rotation = rotation;
			rtInfo.m_drawableSpriteInfo.m_red = r;
			rtInfo.m_drawableSpriteInfo.m_green = g;
			rtInfo.m_drawableSpriteInfo.m_blue = b;
			rtInfo.m_drawableSpriteInfo.m_alpha = a;
			rtInfo.m_drawableSpriteInfo.m_doStereorize = doStereorize;

			rtInfo.m_isDrawingSprite = true;
			rtInfo.m_isDrawingMovie = false;
			break;
		}
	}
}

void ReplayMovieControllerNew::SubmitMovieThisFrame(float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, u32 movieHandle, int renderTargetIndex)
{
	for(CRenderTargetInfo& rtInfo : m_playbackRenderTargets)
	{
		if(rtInfo.m_renderTargetIndex == renderTargetIndex)
		{
			rtInfo.m_drawableMovieInfo.m_centreX = centreX;
			rtInfo.m_drawableMovieInfo.m_centreY = centreY;
			rtInfo.m_drawableMovieInfo.m_width = width;
			rtInfo.m_drawableMovieInfo.m_height = height;
			rtInfo.m_drawableMovieInfo.m_rotation = rotation;
			rtInfo.m_drawableMovieInfo.m_red = r;
			rtInfo.m_drawableMovieInfo.m_green = g;
			rtInfo.m_drawableMovieInfo.m_blue = b;
			rtInfo.m_drawableMovieInfo.m_alpha = a;
			rtInfo.m_drawableMovieInfo.m_movieHandle = movieHandle;

			rtInfo.m_isDrawingSprite = false;
			rtInfo.m_isDrawingMovie = true;
		}
	}
}

void	ReplayMovieControllerNew::StartPlayingMovies()
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

				Displayf("ReplayMovieControllerNew: Starting Movie %s (0x%x) at time %f", thisMovie.movieName, thisMovieHash, thisMovie.movieTime);
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

void	ReplayMovieControllerNew::RegisterMovieAudioLink(u32 modelNameHash, Vector3 &TVPos, u32 movieNameHash, bool isFrontendAudio)
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

void	ReplayMovieControllerNew::LinkMovieAudio(u32 movieNameHash)
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



CEntity *ReplayMovieControllerNew::FindEntity(const u32 modelNameHash, float searchRadius, Vector3 &searchPos)
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

bool ReplayMovieControllerNew::GetClosestObjectCB(CEntity* pEntity, void* data)
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


void	ReplayMovieControllerNew::StopPlayingMovies()
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
				Displayf("ReplayMovieControllerNew: Stopping Movie %s (0x%x)", thisMovie.movieName, thisMovieHash);
				g_movieMgr.Stop(thisMovieHash);
			}
		}
	}
}


void ReplayMovieControllerNew::ReleaseRenderTarget(atFinalHashString renderTargetName, u32 ownerHash, int& renderTargetId)
{
	if (gRenderTargetMgr.GetNamedRendertarget(renderTargetName, ownerHash) != nullptr)
	{
		gRenderTargetMgr.ReleaseNamedRendertarget(renderTargetName, ownerHash);
	}
	renderTargetId = 0;
}



void ReplayMovieControllerNew::DrawToRenderTarget(CRenderTargetInfo const& renderTargetInfo)
{
	/*
	// DEBUG, render to debug texture
	if(m_MovieDestThisFrameInfo.m_MovieHandle)
	{
		g_movieMgr.ms_curMovieHandle = m_MovieDestThisFrameInfo.m_MovieHandle;
		g_movieMgr.ms_debugRender = true;
		g_movieMgr.ms_movieSize = 25;
	}
	*/
	if(renderTargetInfo.m_renderTargetId != 0)
	{
		if(renderTargetInfo.m_isDrawingMovie)
		{
			CReplayDrawableMovie const& movieInfo = renderTargetInfo.m_drawableMovieInfo;
			// SCRIPT_GFX_VISIBLE_WHEN_PAUSED has to be set because intro_script_rectangle::Draw() checks of if the pause menu is active, which it is when viewing previews in the replay editor.
			CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD | SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

			// We have a renderTargetID, render
			CScriptHud::scriptTextRenderID = renderTargetInfo.m_renderTargetId;
			gRenderTargetMgr.UseRenderTargetForReplay((CRenderTargetMgr::RenderTargetId)renderTargetInfo.m_renderTargetId);
			CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
			
			graphics_commands::DrawSpriteTex(NULL, movieInfo.m_centreX, movieInfo.m_centreY, movieInfo.m_width, movieInfo.m_height, movieInfo.m_rotation, movieInfo.m_red, movieInfo.m_green, movieInfo.m_blue, movieInfo.m_alpha, movieInfo.m_movieHandle, SCRIPT_GFX_SPRITE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
		}
		else if(renderTargetInfo.m_isDrawingSprite)
		{
			CReplayDrawableSprite const& spriteInfo = renderTargetInfo.m_drawableSpriteInfo;
			strLocalIndex index = g_TxdStore.FindSlot(spriteInfo.m_texDictionaryName.c_str());
			if(!g_TxdStore.HasObjectLoaded(index))
			{
				g_TxdStore.StreamingRequest(index, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			}
			else
			{
				CScriptHud::scriptTextRenderID = renderTargetInfo.m_renderTargetId;
				gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)renderTargetInfo.m_renderTargetId);
				CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;

				// SET_SCRIPT_GFX_DRAW_ORDER(GFX_ORDER_AFTER_HUD)
				CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
				// SET_SCRIPT_GFX_DRAW_BEHIND_PAUSEMENU(TRUE)
				CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

				// DRAW_SPRITE_NAMED_RENDERTARGET(...)
				grcTexture* pTextureToDraw = graphics_commands::GetTextureFromStreamedTxd(spriteInfo.m_texDictionaryName.c_str(), spriteInfo.m_textureName.c_str());

				EScriptGfxType gfxType = SCRIPT_GFX_SPRITE_NON_INTERFACE;

				graphics_commands::DrawSpriteTex(pTextureToDraw, spriteInfo.m_centreX, spriteInfo.m_centreY, spriteInfo.m_width, spriteInfo.m_height, spriteInfo.m_rotation, spriteInfo.m_red, spriteInfo.m_green, spriteInfo.m_blue, spriteInfo.m_alpha, INVALID_MOVIE_HANDLE, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false, false);
			}
		}
		else
		{
			BlankRenderTarget(renderTargetInfo.m_renderTargetId);
		}
	}
}


void ReplayMovieControllerNew::BlankRenderTarget(u32 renderTargetId)
{
	if( AssertVerify(renderTargetId != 0) )
	{
		CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD | SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
		// Clear the rectangle
		hud_commands::CommandSetTextRenderId((CRenderTargetMgr::RenderTargetId)renderTargetId);
		graphics_commands::CommandDrawRect(0.5f, 0.5f, 1.0f, 1.0f, 0, 0, 0, 255);
	}
}


void ReplayMovieControllerNew::OnRegisterNamedRenderTarget(const char *name, u32 OUTPUT_ONLY(ownerID))
{
	atFinalHashString hashString(name);
	int i = m_knownRenderTargets.Find(hashString);
	if(i != -1)
	{
		if(m_renderTargetRegistersToRecord.Find(i) == -1)
		{
			m_renderTargetRegistersToRecord.PushAndGrow(i);
			replayDebugf1("ReplayMovieController::OnRegisterNamedRenderTarget %s Owner %u 0x%08X", name, ownerID, hashString.GetHash());
		}
	}
}


void ReplayMovieControllerNew::OnReleaseNamedRenderTarget(const char*)
{
}

void ReplayMovieControllerNew::OnLinkNamedRenderTarget(int ModelHashKey, u32 ownerID, CRenderTargetMgr::namedRendertarget const* pRT)
{
	if(m_renderTargetLinksToRecord.Find(rtLink(ModelHashKey, 0, 0)) == -1)
	{
		int rtIndex = 0;
		if(pRT)
		{
			rtIndex = m_knownRenderTargets.Find(pRT->name);

			m_renderTargetLinksToRecord.PushAndGrow(rtLink(ModelHashKey, rtIndex, ownerID));
			replayDebugf1("ReplayMovieController::OnLinkNamedRenderTarget %u (%s) rtIndex %d, Owner %u", ModelHashKey, pRT->name.GetCStr(), rtIndex, ownerID);
		}
	}
}


void ReplayMovieControllerNew::Record()
{
	for(int i = 0; i < m_renderTargetRegistersToRecord.GetCount(); ++i)
	{
		CReplayMgr::RecordFx<CPacketRegisterRenderTarget>(CPacketRegisterRenderTarget(m_renderTargetRegistersToRecord[i]));
	}
	m_renderTargetRegistersToRecord.Reset();

	for(rtLink const& rtl : m_renderTargetLinksToRecord)
	{
		CReplayMgr::RecordFx<CPacketLinkRenderTarget>(CPacketLinkRenderTarget(rtl.modelHashKey, rtl.renderTargetIndex, rtl.ownerID));
	}
	m_renderTargetLinksToRecord.Reset();
}

bool startsWith(char const* str1, int str1length, char const* str2)
{
	int length=(int)strlen(str2);
	if (length>str1length)
		return false;

	for (int i=0;i<str1length;i++)
	{
		if (str1[i]!=str2[i])
			return false;
	}
	return true;
}

void ReplayMovieControllerNew::OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, int renderTargetID)
{
	bool recordPacket = false;
	int textureNameLength = 0;
	int textureDictNameLength = 0;
	int rtIndex = 0;
	CRenderTargetMgr::namedRendertarget const* pRT = nullptr;

	if(pTextureName)
	{
		textureNameLength = (int)strlen(pTextureName);
		int texIndex = 0;
		const char* ptexName = pAcceptableTextureNames[0];
		while(ptexName)
		{
			if(startsWith(pTextureName, textureNameLength, ptexName))
			{
				recordPacket = true;
				break;
			}

			ptexName = pAcceptableTextureNames[++texIndex];
		}
	}

	if(pTextureDictionaryName)
	{
		recordPacket = false;
		textureDictNameLength = (int)strlen(pTextureDictionaryName);
		for(atFinalHashString const& knownTexDic : m_knownTextureDictionariesForSprites)
		{
			if(startsWith(pTextureDictionaryName, textureDictNameLength, knownTexDic.GetCStr()))
			{
				recordPacket = true;
				break;
			}
		}
	}
	

	if(recordPacket)
	{
		pRT = gRenderTargetMgr.GetNamedRenderTargetFromUniqueID(renderTargetID);
		if(replayVerifyf(pRT, "Render Target with ID %d not found when trying to render texture %s", renderTargetID, pTextureName))
		{
			rtIndex = m_knownRenderTargets.Find(pRT->name);
			if(textureDictNameLength > 31 || textureNameLength > 31)
			{
				CReplayMgr::RecordFx(CPacketDrawSprite2(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, DoStereorize, rtIndex));
			}
			else
			{
				CReplayMgr::RecordFx(CPacketDrawSprite(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, DoStereorize, rtIndex));
			}
		}

#if 0
		replayDebugf1("CReplayRenderTargetHelper::OnDrawSprite");
		replayDebugf1("  textureDictionaryName %s", pTextureDictionaryName);
		replayDebugf1("  textureName %s", pTextureName);
		replayDebugf1("  centreX %f", CentreX);
		replayDebugf1("  centreY %f", CentreY);
		replayDebugf1("  width %f", Width);
		replayDebugf1("  height %f", Height);
		replayDebugf1("  rotation %f", Rotation);
		replayDebugf1("  red %d", R);
		replayDebugf1("  green %d", G);
		replayDebugf1("  blue %d", B);
		replayDebugf1("  alpha %d", A);
		replayDebugf1("  doStereorize %s", DoStereorize ? "true" : "false");
#endif
	}
}


void ReplayMovieControllerNew::OnDrawTVChannel(float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, u32 movieHandle, int renderTargetID)
{
	
	CRenderTargetMgr::namedRendertarget const* pRT = gRenderTargetMgr.GetNamedRenderTargetFromUniqueID(renderTargetID);
	if(pRT)
	{
		int rtIndex = 0;
		rtIndex = m_knownRenderTargets.Find(pRT->name);
		if(rtIndex >= 0)
		{
			CReplayMgr::RecordFx(CPacketDrawMovie(CentreX, CentreY, Width, Height, Rotation, R, G, B, A, movieHandle, rtIndex));
		}
	}
}

void ReplayMovieControllerNew::AddRenderTarget(int i)
{
	if(replayVerifyf(i < GetKnownRenderTargets().GetCount(), "Render target out of bounds"))
	{
		atFinalHashString pRtName = GetKnownRenderTargets()[i];

		for(CRenderTargetInfo const& info : m_playbackRenderTargets)
		{
			if(info.m_renderTargetName == pRtName)
			{
				// Already in the system
				return;
			}
		}

		CRenderTargetInfo newInfo;
		newInfo.m_renderTargetName = pRtName;
		newInfo.m_renderTargetIndex = i;
		m_playbackRenderTargets.PushAndGrow(newInfo);
	}
}

void ReplayMovieControllerNew::SetRenderTargetLink(int i, u32 modelHash)
{
	for(CRenderTargetInfo& info : m_playbackRenderTargets)
	{
		if(info.m_renderTargetIndex == i)
		{
			info.m_linkModelHash = modelHash;
			break;
		}
	}
}

CPacketRegisterRenderTarget::CPacketRegisterRenderTarget(int renderTargetIndex)
	: CPacketEvent(PACKETID_REGISTER_RENDERTARGET, sizeof(CPacketRegisterRenderTarget))
	, CPacketEntityInfo()
{
	replayAssert(renderTargetIndex >= 0 && renderTargetIndex < ReplayMovieControllerNew::GetKnownRenderTargets().GetCount());
	m_renderTargetIndex = renderTargetIndex;

	replayDebugf1("Record Register %d", m_renderTargetIndex);
}

void CPacketRegisterRenderTarget::Extract(const CEventInfo<void>&) const
{
	replayDebugf1("Extract Register %d", m_renderTargetIndex);
	ReplayMovieControllerNew::AddRenderTarget(m_renderTargetIndex);
}

bool CPacketRegisterRenderTarget::HasExpired(const CEventInfoBase&) const
{
	if(replayVerifyf(m_renderTargetIndex >= 0 && m_renderTargetIndex < ReplayMovieControllerNew::GetKnownRenderTargets().GetCount(), "Render target index out of bounds"))
	{
		if(gRenderTargetMgr.GetNamedRendertarget(ReplayMovieControllerNew::GetKnownRenderTargets()[m_renderTargetIndex]))
			return false;
	}

	replayDebugf1("Expire Register %d", m_renderTargetIndex);
	return true;
}


CPacketLinkRenderTarget::CPacketLinkRenderTarget(u32 modelHashKey, int rtIndex, u32 ownerTag)
	: CPacketEvent(PACKETID_LINK_RENDERTARGET, sizeof(CPacketLinkRenderTarget))
	, CPacketEntityInfo()
{
	m_modelHashKey = modelHashKey;
	m_renderTargetIndex = rtIndex;
	m_ownerTag = ownerTag;

	replayDebugf1("Record Link 0x%08X, %d, %u", m_modelHashKey.GetHash(), m_renderTargetIndex, m_ownerTag);
}

void CPacketLinkRenderTarget::Extract(const CEventInfo<void>&) const
{
	replayDebugf1("Extract Link 0x%08X, %d, %u", m_modelHashKey.GetHash(), m_renderTargetIndex, m_ownerTag);
	ReplayMovieControllerNew::SetRenderTargetLink(m_renderTargetIndex, m_modelHashKey);
}

bool CPacketLinkRenderTarget::HasExpired(const CEventInfoBase&) const
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_modelHashKey, &modelId);
	if(gRenderTargetMgr.IsNamedRenderTargetLinkedConst(modelId.GetModelIndex(), m_ownerTag))
	{
		return false;
	}

	replayDebugf1("Expire Link 0x%08X, %d, %u", m_modelHashKey.GetHash(), m_renderTargetIndex, m_ownerTag);
	return true;
}

#endif	//GTA_REPLAY
