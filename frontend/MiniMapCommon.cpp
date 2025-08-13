#include "MiniMapCommon.h"

#if __D3D
#	include "system/d3d9.h"
#   include "system/xtl.h"
#	if __XENON
#		include "system/xgraphics.h"
#	endif
#endif

// framework:
#include "atl/pagedpool.h"
#include "fwsys/gameskeleton.h"

// game:
#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#include "FrontEnd/Scaleform/ScaleformMgr.h"
#include "frontend/ui_channel.h"
#include "renderer/rendertargets.h"
#include "renderer/PostProcessFX.h"

#if __D3D11
#include "grcore/texturedefault.h"
#endif

CBlipUpdateQueue CMiniMap_Common::ms_blipUpdateQueue;
bool CMiniMap_Common::ms_bInteriorMovieIsActive;
atBinaryMap<ConstString, BlipLinkage> CMiniMap_Common::ms_pBlipLinkages;
sMapZoomData CMiniMap_Common::sm_zoomData;

#if ENABLE_FOG_OF_WAR

#if RSG_PC
u32					CMiniMap_Common::ms_CurExportRTIndex = 0;
grcRenderTarget*	CMiniMap_Common::ms_MiniMapFogOfWar[MAX_FOW_TARGETS];
#else
grcRenderTarget*	CMiniMap_Common::ms_MiniMapFogOfWar;
#endif

#if __D3D11
#if RSG_PC
grcTexture* CMiniMap_Common::ms_MiniMapFogOfWarTex[MAX_FOW_TARGETS];
grcTexture* CMiniMap_Common::ms_MiniMapFogofWarRevealRatioTex[MAX_FOW_TARGETS];
#else
grcTexture* CMiniMap_Common::ms_MiniMapFogOfWarTex;
grcTexture* CMiniMap_Common::ms_MiniMapFogofWarRevealRatioTex;
#endif
u8 CMiniMap_Common::ms_MiniMapFogOfWarData[FOG_OF_WAR_RT_SIZE * FOG_OF_WAR_RT_SIZE];
float CMiniMap_Common::ms_MiniMapFogofWarRevealRatioData;
#endif //__D3D11

#if RSG_PC
grcRenderTarget* CMiniMap_Common::ms_MiniMapFogofWarRevealRatio[MAX_FOW_TARGETS];
#else
grcRenderTarget* CMiniMap_Common::ms_MiniMapFogofWarRevealRatio;
#endif
grcRenderTarget* CMiniMap_Common::ms_MiniMapFogOfWarCopy;

float *CMiniMap_Common::ms_MiniMapRevealRatioLockPtr;
u8 *CMiniMap_Common::ms_MiniMapFogOfWarLockPtr;
u32 CMiniMap_Common::ms_MiniMapFogOfWarLockPitch = FOG_OF_WAR_RT_SIZE;

bank_float fowPreRevealed = 1312.0f;
#endif //ENABLE_FOG_OF_WAR

#if !__FINAL
bool CMiniMap_Common::sm_bLogMinimapTransitions = true;
#endif // __BANK

#if	!__FINAL
PARAM(noLogMinimapTransitions, "[code] For debugging transitions between the styles of minimap, bigmap etc etc and track it to ensure it completes");
#endif

sMiniMapInterior::sMiniMapInterior()
{
	Clear();
};

void sMiniMapInterior::Clear()
{
	uHash = 0u;
	vPos.Zero();
	vBoundMin.Zero();
	vBoundMax.Zero();
	fRot = 0.0f;
	iLevel = INVALID_LEVEL;	//	Or should this be -98 or 0?
}

bool sMiniMapInterior::ResolveDifferentHash(sMiniMapInterior& newData)
{
	if(uHash != newData.uHash)
	{
		uHash = newData.uHash;
		return true;
	}

	return false;
}

bool sMiniMapInterior::ResolveDifferentOrientation(sMiniMapInterior& newData)
{
	if(vPos != newData.vPos || fRot != newData.fRot)
	{
		vPos = newData.vPos;
		fRot = newData.fRot;
		return true;
	}

	return false;
}

bool sMiniMapInterior::ResolveDifferentLevels(sMiniMapInterior& newData)
{
	if( iLevel != newData.iLevel )
	{
		iLevel = newData.iLevel;
		return true;
	}

	return false;
}

sMiniMapRenderStateStruct::sMiniMapRenderStateStruct() : m_Interior()
{
	m_vCentrePosition.Zero();
	bDeadOrArrested = false;
	bInPlaneOrHeli = false;
	fRoll = 0.0f;
	m_fPlayerVehicleSpeed = 0.0f;
	bInCreator = false;
	m_bFlashWantedOverlay = false;
	bInsideInterior = false;
	bIsInParachute = false;
	bDrawCrosshair = false;

	m_vCurrentMiniMapPosition.Zero();
	m_vCurrentMiniMapSize.Zero();
	
	m_fMaxAltimeterHeight = 0.0f;
	m_fMinAltimeterHeight = 0.0f;

	m_fPauseMapScale = 0.0f;

	m_iScriptZoomValue = 0;

#if ENABLE_FOG_OF_WAR
	m_vPlayerPos.Zero();
	memset(m_ScriptReveal,0,sizeof(m_ScriptReveal));
	m_numScriptRevealRequested = 0;
	m_bUpdateFoW = false;
	m_bUpdatePlayerFoW = false;
	m_bClearFoW = false;
	m_bShowFow = false;
	m_bRevealFoW = false;
#endif // ENABLE_FOG_OF_WAR
	m_vTilesAroundPlayer.Zero();
	m_vCentreTileForPlayer.Zero();

	m_MinimapModeState = MINIMAP_MODE_STATE_NONE;

	m_CurrentGolfMap = GOLF_COURSE_OFF;
	m_AltimeterMode = ALTIMETER_OFF;

	m_bShowSonarSweep = false;

	m_bIsInBigMap = false;
	m_bBigMapFullZoom = false;
	m_bIsInPauseMap = false;
	m_bLockedToDistanceZoom = false;
	m_bIsInCustomMap = false;
	m_bIsActive = false;
	m_bBackgroundMapShouldBeHidden = false;
	m_bShowPrologueMap = false;
	m_bShowIslandMap = false;
	m_bShowMainMap = true;
	m_bHideInteriorMap = false;
	m_bBitmapOnly = false;
	m_bShouldProcessMiniMap = false;
	m_bInsideReappearance = false;
	m_bRangeLockedToBlip = false;
	m_bDisplayPlayerNames = false;
	m_bColourAltimeter = false;

#if __BANK
//	m_iDebugGolfCourseMap = -1;
	m_bDisplayAllBlipNames = false;
	m_bHasDebugInterior = false;
#endif
}

// --- CBlipUpdateQueue
// queue of blips to update on renderthread
void CBlipUpdateQueue::Init()
{
	m_Queue.Init(12, 16384/sizeof(CBlipComplex));
	m_Processed = true;
}

void CBlipUpdateQueue::LockQueue() const
{
	m_QueueLock.Lock();
}

void CBlipUpdateQueue::UnlockQueue() const
{
	m_QueueLock.Unlock();
}

void CBlipUpdateQueue::Add(const CBlipComplex& blip)
{
	CBlipComplex* pBlip = new(m_Queue.New()) CBlipComplex(); // placement new; pool's new DOESN'T call the constructor
	if(uiVerifyf(pBlip, "failed to allocate blip on update queue"))
	{
		*pBlip = blip;
	}
	m_Processed = false;
}

void CBlipUpdateQueue::Empty()
{
	Assertf(m_Processed, "Queue hasnt been processed");

	atPagedPoolIterator<CBlipComplex, fwDefaultRscMemPagedPoolAllocator<CBlipComplex, true> > it(m_Queue);
	while (!it.IsAtEnd()) {
		atPagedPoolIndex index = it.GetPagedPoolIndex();

		it->~CBlipComplex(); // manually invoke the destructor, as we're freeing it up now
		++it;
		m_Queue.Free(index);
	}
}

void CBlipUpdateQueue::UpdateBlips()
{
	QueueLock(*this);
	if(m_Processed)
		return;

	atPagedPoolIterator<CBlipComplex, fwDefaultRscMemPagedPoolAllocator<CBlipComplex, true> > it(m_Queue);
	while (!it.IsAtEnd()) {
		atPagedPoolIndex index = it.GetPagedPoolIndex();
		CBlipComplex* pBlip = const_cast<CBlipComplex*>(m_Queue.Get(index));
		CMiniMap_RenderThread::UpdateIndividualBlip(pBlip, 0, false);
		
		++it;
	}
	m_Processed = true;
}

void CBlipUpdateQueue::RenderToFow()
{
	QueueLock(*this);

	atPagedPoolIterator<CBlipComplex, fwDefaultRscMemPagedPoolAllocator<CBlipComplex, true> > it(m_Queue);
	while (!it.IsAtEnd()) {
		atPagedPoolIndex index = it.GetPagedPoolIndex();
		CBlipComplex* pBlip = const_cast<CBlipComplex*>(m_Queue.Get(index));
		CMiniMap_RenderThread::RenderToFoW(pBlip);

		++it;
	}
}

// --- CMiniMap_Common 
// Common functions to the minimap update thread and render thread
void CMiniMap_Common::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
#if !__FINAL
		if(PARAM_noLogMinimapTransitions.Get())
		{
			sm_bLogMinimapTransitions = false;
		}
		else
		{
			sm_bLogMinimapTransitions = true;
		}
#endif // !__FINAL

		ms_blipUpdateQueue.Init();
		
#if ENABLE_FOG_OF_WAR
		grcTextureFactory::CreateParams params;
		params.Format = grctfL8;
		params.Lockable = true;
#if __PS3
		params.InLocalMemory = false;
#endif		
#if __XENON
		params.HasParent = true;
		params.Parent = grcTextureFactory::GetInstance().GetBackBuffer(false);
#endif		

#if __D3D11
		grcTextureFactory::TextureCreateParams TextureParams(grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

#if RSG_PC
		ms_CurExportRTIndex = 0;
		for(int i = 0; i < MAX_FOW_TARGETS; ++i)
		{
			BANK_ONLY(grcTexture::SetCustomLoadName("fogofwar");)
				ms_MiniMapFogOfWarTex[i] = grcTextureFactory::GetInstance().Create( FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, grctfL8, NULL, 1U, &TextureParams );
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
				ms_MiniMapFogOfWar[i] = grcTextureFactory::GetInstance().CreateRenderTarget("fogofwar", ms_MiniMapFogOfWarTex[i]->GetTexturePtr());

			// Fill texture data with empty data - Should just issue a clear to do this.
			grcTextureLock texLock;
			if(ms_MiniMapFogOfWarTex[i]->LockRect(0, 0, texLock))
			{
				memset(texLock.Base,0,FOG_OF_WAR_RT_SIZE*texLock.Pitch);

				ms_MiniMapFogOfWarTex[i]->UnlockRect(texLock);
			}
		}
#else
		BANK_ONLY(grcTexture::SetCustomLoadName("fogofwar");)
		ms_MiniMapFogOfWarTex = grcTextureFactory::GetInstance().Create( FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, grctfL8, NULL, 1U, &TextureParams );
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
		ms_MiniMapFogOfWar = grcTextureFactory::GetInstance().CreateRenderTarget("fogofwar", ms_MiniMapFogOfWarTex->GetTexturePtr());

		// Fill texture data with empty data - Should just issue a clear to do this.
		grcTextureLock texLock;
		if(ms_MiniMapFogOfWarTex->LockRect(0, 0, texLock))
		{
			memset(texLock.Base,0,FOG_OF_WAR_RT_SIZE*texLock.Pitch);

			ms_MiniMapFogOfWarTex->UnlockRect(texLock);
		}
#endif

		ms_MiniMapFogOfWarLockPtr = ms_MiniMapFogOfWarData;
		ms_MiniMapFogOfWarLockPitch = FOG_OF_WAR_RT_SIZE;
#else
		grcTextureLock texLock;
		params.ForceNoTiling = true;
		ms_MiniMapFogOfWar = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "fogofwar", grcrtPermanent, FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, 8, &params);		

		// Obtain the address of the texture data.
		if(ms_MiniMapFogOfWar->LockRect(0, 0, texLock))
		{
			ms_MiniMapFogOfWarLockPtr = (u8*)texLock.Base;
			ms_MiniMapFogOfWarLockPitch = texLock.Pitch;
			memset(ms_MiniMapFogOfWarLockPtr,0,FOG_OF_WAR_RT_SIZE*ms_MiniMapFogOfWarLockPitch);
			ms_MiniMapFogOfWar->UnlockRect(texLock);
		}
#endif //__D3D11

#if __PS3
		params.InLocalMemory = false;
		params.InTiledMemory = false;
		params.IsSwizzled = false;
		params.PoolID = PostFX::g_RTPoolIdPostFX;
		params.PoolHeap = 0;
#endif
		params.Format = grctfR32F;

#if __D3D11
#if RSG_PC
		for(int i = 0; i < MAX_FOW_TARGETS; ++i)
		{
			BANK_ONLY(grcTexture::SetCustomLoadName("fogofwar Ratio");)
			ms_MiniMapFogofWarRevealRatioTex[i] = grcTextureFactory::GetInstance().Create( 1, 1, grctfR32F, NULL, 1U, &TextureParams );
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			ms_MiniMapFogofWarRevealRatio[i] = CRenderTargets::CreateRenderTarget("fogofwar Ratio", ms_MiniMapFogofWarRevealRatioTex[i]->GetTexturePtr());
			ms_MiniMapRevealRatioLockPtr = &ms_MiniMapFogofWarRevealRatioData;
		}
#else
		BANK_ONLY(grcTexture::SetCustomLoadName("fogofwar Ratio");)
			ms_MiniMapFogofWarRevealRatioTex = grcTextureFactory::GetInstance().Create( 1, 1, grctfR32F, NULL, 1U, &TextureParams );
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			ms_MiniMapFogofWarRevealRatio = CRenderTargets::CreateRenderTarget("fogofwar Ratio", ms_MiniMapFogofWarRevealRatioTex->GetTexturePtr());
		ms_MiniMapRevealRatioLockPtr = &ms_MiniMapFogofWarRevealRatioData;
#endif
#else

#if __PS3
		ms_MiniMapFogofWarRevealRatio = grcTextureFactory::GetInstance().CreateRenderTarget("fogofwar Ratio", grcrtPermanent, 1, 1, 32, &params);
		ms_MiniMapFogofWarRevealRatio->AllocateMemoryFromPool();
#else
		ms_MiniMapFogofWarRevealRatio = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE,"fogofwar Ratio", grcrtPermanent, 1, 1, 32, &params);
#endif

		if(ms_MiniMapFogofWarRevealRatio->LockRect(0, 0, texLock))
		{
			ms_MiniMapRevealRatioLockPtr = (float*)texLock.Base;
			ms_MiniMapFogofWarRevealRatio->UnlockRect(texLock);
			Assert(ms_MiniMapRevealRatioLockPtr);
		}
#endif //__D3D11

		*ms_MiniMapRevealRatioLockPtr = fowPreRevealed;

		params.Format = grctfL8;
#if __PS3
		params.InLocalMemory = true;
		ms_MiniMapFogOfWarCopy = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "fogofwar CP", grcrtPermanent, FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, 8, &params, 2);
		ms_MiniMapFogOfWarCopy ->AllocateMemoryFromPool();
#elif __XENON
		ms_MiniMapFogOfWarCopy = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_SHADOWS, "fogofwar CP", grcrtPermanent, FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, 8, &params, kPhotoHeap);
		ms_MiniMapFogOfWarCopy ->AllocateMemoryFromPool();
#else
		params.Lockable = false;
		ms_MiniMapFogOfWarCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "fogofwar CP", grcrtPermanent, FOG_OF_WAR_RT_SIZE, FOG_OF_WAR_RT_SIZE, 8, &params);
#endif
#endif // ENABLE_FOG_OF_WAR
		
		InitZoomLevels();
	}
	else if(initMode == INIT_SESSION)
	{
		if( CMiniMap::GetIgnoreFowOnInit() == false )
		{
			Assert(ms_MiniMapFogOfWarLockPtr);
			memset(ms_MiniMapFogOfWarLockPtr,0,128*128);
			*ms_MiniMapRevealRatioLockPtr = fowPreRevealed;;
		}
	}
}


void CMiniMap_Common::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		ms_pBlipLinkages.Reset();
		sm_zoomData.zoomLevels.Reset();
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
	}
}


void CMiniMap_Common::InitZoomLevels()
{

	static bool bZoomValuesDone = false;  // only ever do once

	if (bZoomValuesDone)
	{
		return;
	}

	static const char* MAP_ZOOM_DATA_META_FILE = "common:/data/ui/mapZoomData";
	PARSER.LoadObject(MAP_ZOOM_DATA_META_FILE, "meta", sm_zoomData);

	bZoomValuesDone = true;
}

Vector2 CMiniMap_Common::GetTilesFromZoomLevel(s32 iZoomLevel)
{
	s32 iSecondZoomLevel = iZoomLevel;
	if (CMiniMap::GetPauseMapScale() > GetScaleFromZoomLevel(iZoomLevel))
	{
		iSecondZoomLevel++;

		if (iSecondZoomLevel > MAX_ZOOMED_IN)
		{
			iSecondZoomLevel = MAX_ZOOMED_IN;
		}
	}
	else if (CMiniMap::GetPauseMapScale() < GetScaleFromZoomLevel(iZoomLevel))
	{
		iSecondZoomLevel--;

		s32 iMaxZoomLevel = MAX_ZOOMED_OUT;

		if (CMiniMap::GetInPrologue())
		{
			iMaxZoomLevel = MAX_ZOOMED_OUT_PROLOGUE;
		}

		if (iSecondZoomLevel < iMaxZoomLevel)
		{
			iSecondZoomLevel = iMaxZoomLevel;
		}
	}

	if (uiVerifyf(iZoomLevel < MAX_ZOOM_LEVELS && iZoomLevel < sm_zoomData.zoomLevels.GetCount() && iSecondZoomLevel < MAX_ZOOM_LEVELS && iSecondZoomLevel < sm_zoomData.zoomLevels.GetCount(), "PauseMap: Zoom level invalid %d", iZoomLevel))
	{
		atArray<sMapZoomData::sMapZoomLevel>& zoomLevels = sm_zoomData.zoomLevels;
		if (iZoomLevel != iSecondZoomLevel)
		{
			if ( (zoomLevels[iZoomLevel].vTiles.x > zoomLevels[iSecondZoomLevel].vTiles.x) || (zoomLevels[iZoomLevel].vTiles.y > zoomLevels[iSecondZoomLevel].vTiles.y) )
			{
				return zoomLevels[iZoomLevel].vTiles;
			}
			else
			{
				return zoomLevels[iSecondZoomLevel].vTiles;
			}
		}
		else
		{
			return zoomLevels[iZoomLevel].vTiles;
		}
	}

	return Vector2(0,0);
}

float CMiniMap_Common::GetScrollSpeedFromZoomLevel(s32 iZoomLevel)
{
	if (uiVerifyf(iZoomLevel < MAX_ZOOM_LEVELS && iZoomLevel < sm_zoomData.zoomLevels.GetCount(), "PauseMap: Zoom level invalid %d", iZoomLevel))
	{
		const float scaleMeterSize = 2750.0f;
		float fFinalZoomValue = scaleMeterSize/CMiniMap::GetPauseMapScale();
		// *30 to adjust for making this framerate independent without having to readjust the tunings into proper values
		return (sm_zoomData.zoomLevels[iZoomLevel].fScrollSpeed * fFinalZoomValue) * 30.0f * fwTimer::GetTimeStep_NonPausedNonScaledClipped();
	}

	return 1.0f;
}

float CMiniMap_Common::GetZoomSpeedFromZoomLevel(s32 iZoomLevel, float fPressedAmount)
{
	if (uiVerifyf(iZoomLevel < MAX_ZOOM_LEVELS && iZoomLevel < sm_zoomData.zoomLevels.GetCount(), "PauseMap: Zoom level invalid %d", iZoomLevel))
	{
		// *30 to adjust for making this framerate independent without having to readjust the tunings into proper values
		return (fPressedAmount * sm_zoomData.zoomLevels[iZoomLevel].fZoomSpeed) * 30.0f * fwTimer::GetTimeStep_NonPausedNonScaledClipped();
	}

	return 1.0f;
}

float CMiniMap_Common::GetScaleFromZoomLevel(s32 iZoomLevel)
{
	if (uiVerifyf(iZoomLevel < MAX_ZOOM_LEVELS && iZoomLevel < sm_zoomData.zoomLevels.GetCount(), "PauseMap: Zoom level invalid %d", iZoomLevel))
	{
		return sm_zoomData.zoomLevels[iZoomLevel].fZoomScale;
	}

	return 1.0f;
}

float CMiniMap_Common::WrapBlipRotation( float fDegrees )
{
	return rage::Wrap(fDegrees, 0.0f, 359.9f);
}

#if __BANK
void CMiniMap_Common::CreateWidgets(bkBank* pBank)
{
	if(pBank)
	{
		for (s32 i = 0; i < MAX_ZOOM_LEVELS; i++)
		{
			char cBankString[50];
			formatf(cBankString, "Zoom Level %d (%d)", i+1, i, NELEM(cBankString));

			pBank->PushGroup(cBankString);
			{
				pBank->AddSlider("Scale", &sm_zoomData.zoomLevels[i].fZoomScale, 0.0f, 1000.0f, 0.001f);
				pBank->AddSlider("Zoom Speed", &sm_zoomData.zoomLevels[i].fZoomSpeed, 0.0f, 1000.0f, 0.001f);
				pBank->AddSlider("Scroll Speed", &sm_zoomData.zoomLevels[i].fScrollSpeed, 0.0f, 100.0f, 0.001f);
				pBank->AddSlider("Amount of Tiles", &sm_zoomData.zoomLevels[i].vTiles, 0.0f, 40.0f, 1.0f);


				pBank->PopGroup();
			}
		}

	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_Common::EnumerateBlipObjectNames()
// PURPOSE: gets a list of blips from the minimap movie and adds them to an array
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_Common::EnumerateBlipLinkages(s32 iMovieId)
{
	GFxValue asAssetArray;

	{ // scope for AutoLock
		CScaleformMgr::AutoLock lock(iMovieId);
		if (CScaleformMgr::BeginMethod(iMovieId, SF_BASE_CLASS_MINIMAP, "GET_ASSET_ARRAY"))
		{
			asAssetArray = CScaleformMgr::EndMethodGfxValue(true);  // safe to invoke straight away as we are in an INIT state
		}
	}

	if (asAssetArray.IsArray())
	{
		int iNumOfBlipObjects = asAssetArray.GetArraySize();

		ms_pBlipLinkages.Reserve(iNumOfBlipObjects);
		GFxValue asArrayElement;

		for (s32 i = 0; i < iNumOfBlipObjects; i++)
		{
			asAssetArray.GetElement(i, &asArrayElement);

			if (asArrayElement.IsString())
			{
				// insert new element and add it to 
#if HASHED_BLIP_IDS
				*ms_pBlipLinkages.Insert( BlipLinkage(asArrayElement.GetString()) ) = asArrayElement.GetString();
#else
				*ms_pBlipLinkages.Insert( BlipLinkage(i) ) = asArrayElement.GetString();
#endif
				uiDebugf1("MiniMap: %d RADAR FOUND BLIP: %s", i, asArrayElement.GetString());
			}
		}

		ms_pBlipLinkages.FinishInsertion();

		return true;
	}

	return false;
}

const char* CMiniMap_Common::GetBlipName(const BlipLinkage& iBlipNamesIndex)
{
	ConstString* pString = ms_pBlipLinkages.Access(iBlipNamesIndex);
#if HASHED_BLIP_IDS
	if( uiVerifyf(pString, "Can't access linkage for " HASHFMT ", as it doesn't exist", HASHOUT(iBlipNamesIndex)) )
#else
	if( uiVerifyf(pString, "Can't access linkage for %d, as it doesn't exist", iBlipNamesIndex) )
#endif
		return pString->c_str();
	
	return NULL; //"-BAD BLIP NAME-";
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	GetColourFromBlipSettings()
// PURPOSE:	gets the colour that we want to use on this blip
/////////////////////////////////////////////////////////////////////////////////////
Color32 CMiniMap_Common::GetColourFromBlipSettings(eBLIP_COLOURS nColourIndex, bool bBrightness, eHUD_COLOURS *pReturnHudColour)
{
	Color32 iReturnColour;
	eHUD_COLOURS eHudColour;
	u8 iAlpha = 255;  // default alpha

	switch (nColourIndex)
	{
	case BLIP_COLOUR_DEFAULT:
		eHudColour = HUD_COLOUR_PURE_WHITE;
		break;

	case BLIP_COLOUR_RED :
		if (bBrightness)
			eHudColour = HUD_COLOUR_REDLIGHT;
		else
			eHudColour = HUD_COLOUR_RED;
		break;

	case BLIP_COLOUR_COORD :
	case BLIP_COLOUR_YELLOW :
		if (bBrightness)
			eHudColour = HUD_COLOUR_YELLOWLIGHT;
		else
			eHudColour = HUD_COLOUR_YELLOW;
		break;

	case BLIP_COLOUR_CONTACT :
	case BLIP_COLOUR_OBJECT :
	case BLIP_COLOUR_BLUE :
		if (bBrightness)
			eHudColour = HUD_COLOUR_BLUELIGHT;
		else
			eHudColour = HUD_COLOUR_BLUE;
		break;

	case BLIP_COLOUR_GREEN :
	case BLIP_COLOUR_MISSION_OBJECT :
		if (bBrightness)
			eHudColour = HUD_COLOUR_GREENLIGHT;
		else
			eHudColour = HUD_COLOUR_GREEN;
		break;

	case BLIP_COLOUR_WHITE :
		if (bBrightness)
			eHudColour = HUD_COLOUR_PURE_WHITE;
		else
			eHudColour = HUD_COLOUR_WHITE;
		break;

	case BLIP_COLOUR_FRIENDLY :
		if (bBrightness)
			eHudColour = HUD_COLOUR_BLUE;
		else
			eHudColour = HUD_COLOUR_RED;
		break;

	case BLIP_COLOUR_CHECKPOINT :
		eHudColour = HUD_COLOUR_YELLOW;
		break;

	case BLIP_COLOUR_DESTINATION :
		eHudColour = HUD_COLOUR_YELLOW;
		break;

	case BLIP_COLOUR_MISSION_MARKER :
		eHudColour = HUD_COLOUR_YELLOW;
		break;

	case BLIP_COLOUR_MISSION_ARROW :
		eHudColour = HUD_COLOUR_BLUE;
		break;

	case BLIP_COLOUR_POLICE :
		eHudColour = HUD_COLOUR_RED;
		break;

	case BLIP_COLOUR_POLICE_INACTIVE :
		eHudColour = HUD_COLOUR_REDDARK;
		break;

	case BLIP_COLOUR_POLICE2 :
		eHudColour = HUD_COLOUR_BLUE;
		break;

	case BLIP_COLOUR_POLICE2_INACTIVE :
		eHudColour = HUD_COLOUR_BLUEDARK;
		break;

	case BLIP_COLOUR_POLICE_RADAR_RED :
	{
		eHudColour = HUD_COLOUR_REDDARK;
		iAlpha = 100;
		break;
	}

	case BLIP_COLOUR_POLICE_RADAR_BLUE :
	{
		eHudColour = HUD_COLOUR_BLUEDARK;
		iAlpha = 100;
		break;
	}

	case BLIP_COLOUR_NET_PLAYER1:
		eHudColour = HUD_COLOUR_NET_PLAYER1;
		break;

	case BLIP_COLOUR_NET_PLAYER2:
		eHudColour = HUD_COLOUR_NET_PLAYER2;
		break;

	case BLIP_COLOUR_NET_PLAYER3:
		eHudColour = HUD_COLOUR_NET_PLAYER3;
		break;

	case BLIP_COLOUR_NET_PLAYER4:
		eHudColour = HUD_COLOUR_NET_PLAYER4;
		break;

	case BLIP_COLOUR_NET_PLAYER5:
		eHudColour = HUD_COLOUR_NET_PLAYER5;
		break;

	case BLIP_COLOUR_NET_PLAYER6:
		eHudColour = HUD_COLOUR_NET_PLAYER6;
		break;

	case BLIP_COLOUR_NET_PLAYER7:
		eHudColour = HUD_COLOUR_NET_PLAYER7;
		break;

	case BLIP_COLOUR_NET_PLAYER8:
		eHudColour = HUD_COLOUR_NET_PLAYER8;
		break;

	case BLIP_COLOUR_NET_PLAYER9:
		eHudColour = HUD_COLOUR_NET_PLAYER9;
		break;

	case BLIP_COLOUR_NET_PLAYER10:
		eHudColour = HUD_COLOUR_NET_PLAYER10;
		break;

	case BLIP_COLOUR_NET_PLAYER11:
		eHudColour = HUD_COLOUR_NET_PLAYER11;
		break;

	case BLIP_COLOUR_NET_PLAYER12:
		eHudColour = HUD_COLOUR_NET_PLAYER12;
		break;

	case BLIP_COLOUR_NET_PLAYER13:
		eHudColour = HUD_COLOUR_NET_PLAYER13;
		break;

	case BLIP_COLOUR_NET_PLAYER14:
		eHudColour = HUD_COLOUR_NET_PLAYER14;
		break;

	case BLIP_COLOUR_NET_PLAYER15:
		eHudColour = HUD_COLOUR_NET_PLAYER15;
		break;

	case BLIP_COLOUR_NET_PLAYER16:
		eHudColour = HUD_COLOUR_NET_PLAYER16;
		break;

	case BLIP_COLOUR_NET_PLAYER17:
		eHudColour = HUD_COLOUR_NET_PLAYER17;
		break;

	case BLIP_COLOUR_NET_PLAYER18:
		eHudColour = HUD_COLOUR_NET_PLAYER18;
		break;

	case BLIP_COLOUR_NET_PLAYER19:
		eHudColour = HUD_COLOUR_NET_PLAYER19;
		break;

	case BLIP_COLOUR_NET_PLAYER20:
		eHudColour = HUD_COLOUR_NET_PLAYER20;
		break;

	case BLIP_COLOUR_NET_PLAYER21:
		eHudColour = HUD_COLOUR_NET_PLAYER21;
		break;

	case BLIP_COLOUR_NET_PLAYER22:
		eHudColour = HUD_COLOUR_NET_PLAYER22;
		break;

	case BLIP_COLOUR_NET_PLAYER23:
		eHudColour = HUD_COLOUR_NET_PLAYER23;
		break;

	case BLIP_COLOUR_NET_PLAYER24:
		eHudColour = HUD_COLOUR_NET_PLAYER24;
		break;

	case BLIP_COLOUR_NET_PLAYER25:
		eHudColour = HUD_COLOUR_NET_PLAYER25;
		break;

	case BLIP_COLOUR_NET_PLAYER26:
		eHudColour = HUD_COLOUR_NET_PLAYER26;
		break;

	case BLIP_COLOUR_NET_PLAYER27:
		eHudColour = HUD_COLOUR_NET_PLAYER27;
		break;

	case BLIP_COLOUR_NET_PLAYER28:
		eHudColour = HUD_COLOUR_NET_PLAYER28;
		break;

	case BLIP_COLOUR_NET_PLAYER29:
		eHudColour = HUD_COLOUR_NET_PLAYER29;
		break;

	case BLIP_COLOUR_NET_PLAYER30:
		eHudColour = HUD_COLOUR_NET_PLAYER30;
		break;

	case BLIP_COLOUR_NET_PLAYER31:
		eHudColour = HUD_COLOUR_NET_PLAYER31;
		break;

	case BLIP_COLOUR_NET_PLAYER32:
		eHudColour = HUD_COLOUR_NET_PLAYER32;
		break;

	case BLIP_COLOUR_SIMPLEBLIP_DEFAULT:
		eHudColour = HUD_COLOUR_SIMPLEBLIP_DEFAULT;
		break;

	case BLIP_COLOUR_FRONTEND_YELLOW:
		eHudColour = HUD_COLOUR_MENU_YELLOW;
		break;

	case BLIP_COLOUR_WAYPOINT:
		eHudColour = HUD_COLOUR_WAYPOINT;
		break;

	case BLIP_COLOUR_STEALTH_GREY:
		eHudColour = HUD_COLOUR_GREYDARK;
		break;

	case BLIP_COLOUR_WANTED:
		eHudColour = HUD_COLOUR_REDLIGHT;
		break;

	case BLIP_COLOUR_FREEMODE:
		eHudColour = HUD_COLOUR_FREEMODE;
		break;

	case BLIP_COLOUR_INACTIVE_MISSION:
		eHudColour = HUD_COLOUR_INACTIVE_MISSION;
		break;

	case BLIP_COLOUR_MICHAEL:
		eHudColour = HUD_COLOUR_MICHAEL;
		break;

	case BLIP_COLOUR_FRANKLIN:
		eHudColour = HUD_COLOUR_FRANKLIN;
		break;

	case BLIP_COLOUR_TREVOR:
		eHudColour = HUD_COLOUR_TREVOR;
		break;

	case BLIP_COLOUR_GOLF_P1:
		eHudColour = HUD_COLOUR_GOLF_P1;
		break;

	case BLIP_COLOUR_GOLF_P2:
		eHudColour = HUD_COLOUR_GOLF_P2;
		break;

	case BLIP_COLOUR_GOLF_P3:
		eHudColour = HUD_COLOUR_GOLF_P3;
		break;

	case BLIP_COLOUR_GOLF_P4:
		eHudColour = HUD_COLOUR_GOLF_P4;
		break;

	case BLIP_COLOUR_PURPLE:
		eHudColour = HUD_COLOUR_PURPLE;
		break;

	case BLIP_COLOUR_ORANGE:
		eHudColour = HUD_COLOUR_ORANGE;
		break;

	case BLIP_COLOUR_GREENDARK:
		eHudColour = HUD_COLOUR_GREENDARK;
		break;

	case BLIP_COLOUR_BLUELIGHT:
		eHudColour = HUD_COLOUR_BLUELIGHT;
		break;

	case BLIP_COLOUR_BLUEDARK:
		eHudColour = HUD_COLOUR_BLUEDARK;
		break;

	case BLIP_COLOUR_GREY:
		eHudColour = HUD_COLOUR_GREY;
		break;

	case BLIP_COLOUR_YELLOWDARK:
		eHudColour = HUD_COLOUR_YELLOWDARK;
		break;

	case BLIP_COLOUR_HUDCOLOUR_BLUE:
		eHudColour = HUD_COLOUR_BLUE;
		break;

	case BLIP_COLOUR_PURPLEDARK:
		eHudColour = HUD_COLOUR_PURPLEDARK;
		break;

	case BLIP_COLOUR_HUDCOLOUR_RED:
		eHudColour = HUD_COLOUR_RED;
		break;

	case BLIP_COLOUR_HUDCOLOUR_YELLOW:
		eHudColour = HUD_COLOUR_YELLOW;
		break;

	case BLIP_COLOUR_PINK :
		eHudColour = HUD_COLOUR_PINK;
		break;

	case BLIP_COLOUR_HUDCOLOUR_GREYLIGHT:
		eHudColour = HUD_COLOUR_GREYLIGHT;
		break;

	case BLIP_COLOUR_GANG1:
		eHudColour = HUD_COLOUR_GANG1;
		break;

	case BLIP_COLOUR_GANG2:
		eHudColour = HUD_COLOUR_GANG2;
		break;

	case BLIP_COLOUR_GANG3:
		eHudColour = HUD_COLOUR_GANG3;
		break;

		//	Assume that if nColourIndex is greater than 6 then it is actually an RGBA value
	default :
		if (pReturnHudColour)
			*pReturnHudColour = HUD_COLOUR_PURE_WHITE;
		return (CRGBA( (u8)(nColourIndex>>24)&255, (u8)(nColourIndex>>16)&255, (u8)(nColourIndex>>8)&255, (u8)(nColourIndex)&255));
	}

	iReturnColour = CHudColour::GetRGB(eHudColour, iAlpha);

	if (pReturnHudColour)
		*pReturnHudColour = eHudColour;

	return (iReturnColour);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_Common::CheckIncomingFunctions
// PURPOSE:	checks for incoming functions from ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_Common::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // as this method can be called on either thread, we need to check the relevant buffer here
	{
		CMiniMap_RenderThread::CheckIncomingFunctions(methodName, args);
	}
	else
	{
		CMiniMap::CheckIncomingFunctions(methodName, args);
	}
}


#if ENABLE_FOG_OF_WAR
// prereveal part of the map based on water presence.
float CMiniMap_Common::GetFogOfWarDiscoveryRatio()
{
	float accumulated = 0.0f;

	accumulated = *CMiniMap_Common::GetFogOfWarRevealRatioLockPtr();

	return Saturate((accumulated - fowPreRevealed)/(FOG_OF_WAR_RT_SIZE*FOG_OF_WAR_RT_SIZE - 4.0f*fowPreRevealed));
}

bool CMiniMap_Common::IsCoordinateRevealed(const Vector3 &coord)
{
	Vector2 res = CMiniMap_RenderThread::WorldToFowCoord(coord.x, coord.y);

	u8 *FoW = GetFogOfWarLockPtr();
	if(FoW)
	{
		int x = (int)(res.x * (FOG_OF_WAR_RT_SIZE - 0.5f)) & (FOG_OF_WAR_RT_SIZE - 1);
		int y = (int)(res.y * (FOG_OF_WAR_RT_SIZE - 0.5f)) & (FOG_OF_WAR_RT_SIZE - 1);
		u32 idx = 0 ;

#if __XENON
		idx = XGAddress2DTiledOffset(x, y, FOG_OF_WAR_RT_SIZE, 1); 
#else
		idx = x+y*GetFogOfWarLockPitch();
#endif
		return FoW[idx] > 128;
	}
	return true;
}
#endif // ENABLE_FOG_OF_WAR
