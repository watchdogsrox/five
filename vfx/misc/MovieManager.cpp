///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	MovieManager.cpp
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "MovieManager.h"

// rage
#include "audiosoundtypes/externalstreamsound.h"
#include "audiosoundtypes/sound.h"
#include "audiohardware/driverutil.h"
#include "bank/combo.h"
#include "bink/bink_include.h"
#include "bink/movie.h"
#include "control/replay/ReplayMovieControllerNew.h"
#include "file/asset.h"
#include "file/stream.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "paging/rscbuilder.h"
#include "system/memory.h"
#include "system/externalheap.h"
#include "system/hangdetect.h"
#include "system/memmanager.h"
#include "text/TextFile.h"
#include "fwsys/metadatastore.h"
#if __PPU
#include "system/bootmgr.h"
#include "system/taskheader.h"
#include "system/threadtype.h"


#include <cell/spurs/types.h>

namespace rage {
	extern CellSpurs*	g_Spurs;
}

extern char SPURS_TASK_START(binkspu)[];

namespace {
	struct BinkSpursThreadData {
		u32* spursEA;
		char* elfEA;
	};

	BinkSpursThreadData g_BinkThreadData;
}
#endif

// framework
#include "fwsys/timer.h"
#include "streaming/streamingengine.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// game
#include "Core/Game.h"
#include "Renderer/RenderThread.h"
#include "System/Device_XContent.h"
#include "System/FileMgr.h"
#include "Camera/CamInterface.h"
#include "vfx/misc/TVPlaylistManager.h"
#include "control/replay/Misc/MoviePacket.h"

#include "MovieManager_parser.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////
VFX_MISC_OPTIMISATIONS()

PARAM(binkasserts, "RMPTFX asset path");

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CMovieMgr	g_movieMgr;

#if __ASSERT
atString g_lastBink;
#endif

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

#include "streaming/streamingengine.h"

#define BINK_MAX_ALLOCATION (512 << 10)

CMovieMgr::CMovie	*CMovieMgr::CMovie::ms_pLastPlayedMovie = NULL;
bool	CMovieMgr::CMovie::ms_bSubsEnabled = false;
int	CMovieMgr::CMovie::ms_TextSlotRefs[NUM_BINK_MOVIE_TEXT_SLOTS];
bool CMovieMgr::CMovie::ms_TextSlotMarkedForDelete[NUM_BINK_MOVIE_TEXT_SLOTS];
atFixedArray<int, 16> CMovieMgr::CMovie::m_CancelledRequests;

#if __SCRIPT_MEM_CALC
sysCriticalSectionToken CMovieMgr::CMovie::ms_AllocMapMutex;
#endif	//	__SCRIPT_MEM_CALC

sysMessageQueue<CMovieMgr::sAllocData, 32, true> CMovieMgr::ms_pendingAllocQueue;
sysMessageQueue<CMovieMgr::sAllocData, 32, true> CMovieMgr::ms_doneAllocQueue;

// Textures MUST come from the streaming heap
void* CMovieMgr::AllocateTexture(u32 size, u32 alignment, void* SCRIPT_MEM_CALC_ONLY(owner))
{
	size = (u32)pgRscBuilder::ComputeLeafSize(size, true);
	
	MEM_USE_USERDATA(MEMUSERDATA_BINK);
#if 1
	void* ptr = strStreamingEngine::GetAllocator().Allocate(size, alignment, MEMTYPE_RESOURCE_VIRTUAL);
#else
	void* ptr = strStreamingEngine::GetAllocator().Allocate(size, alignment, MEMTYPE_RESOURCE_PHYSICAL);
#endif

#if __SCRIPT_MEM_CALC
	CMovieMgr::ms_pCurrentMovieForAllocations = (CMovieMgr::CMovie*)owner;
	g_movieMgr.AddMemoryAllocationInfo(ptr, size);
	CMovieMgr::ms_pCurrentMovieForAllocations = NULL;
#endif
	return ptr;
}

// Textures MUST come from the streaming heap
void CMovieMgr::FreeTexture(void* ptr)
{
	SCRIPT_MEM_CALC_ONLY(g_movieMgr.RemoveMemoryAllocationInfo(ptr);)

	MEM_USE_USERDATA(MEMUSERDATA_BINK);
	strStreamingEngine::GetAllocator().Free(ptr);
}

#if 1
void CMovieMgr::DumpDebugInfo(const char*){}
#else
void CMovieMgr::DumpDebugInfo(const char* title)
{
	if (strcmp(title,"wibble") == 0)
	{
		return;
	}

#if !__FINAL
	Displayf("---");
	Displayf("%s",title);
	Displayf("	CMovieMgr::DumpDebugInfo");

	for(u32 i=0; i<MAX_SCRIPT_SLOTS; i++)
	{
		Displayf("	script slot [%d] : %0x", i, m_scriptSlots[i]);
	}

	Displayf("	movie array size : %d", m_movies.GetCount());
	for(u32 i=0; i<m_movies.GetCount(); i++)
	{
		Displayf("	movie [%d] : %p", i, (&m_movies[i]));
	}
	
	Displayf("	---");
#endif
}
#endif

void* CMovieMgr::AllocateFromFragCache(u32 size, u32 alignment)
{
	void* ptr = NULL;
	if (sysThreadType::IsUpdateThread())
	{
		ptr = Allocate(size, alignment);
	}
	else
	{
		sAllocData newAlloc = {size, alignment, NULL, true};
		ms_pendingAllocQueue.Push(newAlloc);
		newAlloc = ms_doneAllocQueue.Pop();
		Assert(newAlloc.size == size);
		Assert(newAlloc.alignment == alignment);

		ptr = newAlloc.ptr;
	}
	Assertf(ptr, "Unable to allocate %u bytes for Bink (from frag cache)!", size);

	return ptr;
}

void CMovieMgr::FreeFromFragCache(void* ptr)
{
	if (sysThreadType::IsUpdateThread())
	{
		Free(ptr);
	}
	else
	{
		sAllocData newAlloc = {0, 0, ptr, false};
		ms_pendingAllocQueue.Push(newAlloc);
	}
}

void* CMovieMgr::AllocateAsync(u32 size, u32 alignment)
{
	//Assertf(size <= BINK_MAX_ALLOCATION, "Bink allocation of %d KB in file %s exceeds max allowable size of %d KB! Open a B* to Eric J Anderson and CC Klaas.", (size >> 10), g_lastBink.c_str(), (BINK_MAX_ALLOCATION >> 10));

	void* ptr;
	sysMemUseMemoryBucket b(MEMBUCKET_UI);

	if (!CNetwork::IsNetworkOpen() && !CNetwork::IsNetworkClosing() && CNetwork::GetNetworkHeap())
	{
		// SP: Use network heap
		sysMemAutoUseNetworkMemory mem;
		ptr = rage_aligned_new(alignment) u8[size];
		Assertf(ptr, "Unable to allocate %u bytes for Bink (from network heap)!", size);
	}
	else
	{
		// MP: Use resource virtual, then frag cache, then game virtual
		MEM_USE_USERDATA(MEMUSERDATA_BINK);
		const size_t power2size = (u32) pgRscBuilder::ComputeLeafSize(size, true);
		ptr = strStreamingEngine::GetAllocator().Allocate(power2size, alignment, MEMTYPE_RESOURCE_VIRTUAL);

		if (!ptr)
		{
			ptr = AllocateFromFragCache(size, alignment);
		}

		Assertf(ptr, "Unable to allocate %u/%" SIZETFMT "u bytes for Bink (from resource/frag/game heaps)!", size, power2size);
	}

	return ptr ? ptr : (void*)(-1);
}

void CMovieMgr::FreeAsync(void* ptr)
{
    // First try to delete it from network heap
	if (CNetwork::GetNetworkHeap() && CNetwork::GetNetworkHeap()->IsValidPointer(ptr))
	{		
		CNetwork::GetNetworkHeap()->Free(ptr);
	}
	else if (strStreamingEngine::GetAllocator().IsValidPointer(ptr))
	{
		MEM_USE_USERDATA(MEMUSERDATA_BINK);
		strStreamingEngine::GetAllocator().Free(ptr);
	}
	else
	{
		FreeFromFragCache(ptr);
	}
}

void* CMovieMgr::Allocate(u32 size, u32 alignment)
{
	//Assertf(size <= BINK_MAX_ALLOCATION, "Bink allocation of %d KB in file %s exceeds max allowable size of %d KB! Open a B* to Eric J Anderson and CC Klaas.", (size >> 10), g_lastBink.c_str(), (BINK_MAX_ALLOCATION >> 10));

	fragCacheAllocator* pAllocator = fragManager::GetFragCacheAllocator();
	Assert(pAllocator);
	return pAllocator->ExternalAllocate(size, alignment);
}

void CMovieMgr::Free(void* ptr)
{
	SCRIPT_MEM_CALC_ONLY(g_movieMgr.RemoveMemoryAllocationInfo(ptr);)

	Assert(!CNetwork::GetNetworkHeap()->IsValidPointer(ptr));

	sysMemAllocator* pAllocator = sysMemManager::GetInstance().GetFragCacheAllocator();
	pAllocator->Free(ptr);
}

#if RSG_BINK_2_7D
	inline void* RADLINK BinkReplacementAlloc(u64 size)
#else
	inline void* RADLINK BinkReplacementAlloc(u32 size)
#endif
{
	// Bink alignment requirement is 32 bytes (per API docs)
	return CMovieMgr::AllocateAsync((u32)size, 32);
}

inline void RADLINK BinkReplacementFree(void* ptr)
{
	return CMovieMgr::FreeAsync(ptr);
}

///////////////////////////////////////////////////////////////////////////////
//  CMovie
///////////////////////////////////////////////////////////////////////////////

#if __BANK
bool CMovieMgr::ms_debugRender = false;
bkCombo* CMovieMgr::ms_movieCombo = NULL;
s32 CMovieMgr::ms_movieIndex = 0;
u32 CMovieMgr::ms_movieSize = 100;
u32 CMovieMgr::ms_curMovieHandle = INVALID_MOVIE_HANDLE;

grcBlendStateHandle			CMovieMgr::ms_defBlendState;
grcDepthStencilStateHandle	CMovieMgr::ms_defDepthStencilState;
grcRasterizerStateHandle	CMovieMgr::ms_defRasterizerState;

grcBlendStateHandle			CMovieMgr::ms_debugBlendState;
grcDepthStencilStateHandle	CMovieMgr::ms_debugDepthStencilState;
grcRasterizerStateHandle	CMovieMgr::ms_debugRasterizerState;
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
CMovieMgr::CMovie*			CMovieMgr::ms_pCurrentMovieForAllocations = NULL;
#endif	//	__SCRIPT_MEM_CALC

float CMovieMgr::CMovie::GetMovieDurationQuick(const char *pFullName)
{
	// TODO: Find out the best buffer size
	const u32 BUFFER_SIZE_MULTIPLIER = 16;
	const u32 READ_AHEAD_BUFFER_SIZE = 524288 * BUFFER_SIZE_MULTIPLIER;

	u32 extraFlags = 0;
	bwMovie::bwMovieParams movieParams;

	safecpy(movieParams.pFileName, pFullName);
	movieParams.extraFlags	= extraFlags;
	movieParams.ioSize		= READ_AHEAD_BUFFER_SIZE;

	float duration = bwMovie::GetMovieDurationQuick(movieParams);

	return duration;
}

#if __BANK
void CMovieMgr::InitRenderStateBlocks()
{
	// default state blocks to restore to once we're done with debug rendering
	grcRasterizerStateDesc defaultExitStateR;
	defaultExitStateR.CullMode = grcRSV::CULL_NONE;
	defaultExitStateR.HalfPixelOffset = 1;
	ms_defRasterizerState = grcStateBlock::CreateRasterizerState(defaultExitStateR);

	grcBlendStateDesc defaultExitStateB;
	defaultExitStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultExitStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultExitStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ms_defBlendState = grcStateBlock::CreateBlendState(defaultExitStateB);

	grcDepthStencilStateDesc defaultExitStateDS;
	defaultExitStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	ms_defDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultExitStateDS);

	// state blocks for debug render
	grcRasterizerStateDesc defaultStateR;
	defaultStateR.CullMode = grcRSV::CULL_NONE;
	defaultStateR.HalfPixelOffset = 1;
	ms_debugRasterizerState = grcStateBlock::CreateRasterizerState(defaultStateR);

	grcBlendStateDesc defaultStateB;
	defaultStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ms_debugBlendState = grcStateBlock::CreateBlendState(defaultStateB);

	grcDepthStencilStateDesc defaultStateDS;
	defaultStateDS.DepthFunc = grcRSV::CMP_ALWAYS;
	defaultStateDS.DepthWriteMask = 0;
	ms_debugDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultStateDS);
}
#endif


void CMovieMgr::CMovie::LoadedAsyncCb(bool* succeeded)
{
	if (*succeeded)
	{
		SetVolume(0.0f);
	}
	else
	{
		SetAsyncFail(true);
	}
}

bool CMovieMgr::CMovie::Preload(const char* pMovieName, bool hasAlpha)	//, fiStream* pFileStream )
{
	vfxAssertf( pMovieName, "CMovieMgr::CMovie::Preload - Invalid movie name" );
	vfxAssertf( m_pMovie, "CMovieMgr::CMovie::Preload - Could not create bink movie" );

	SetDoesntExist(false);

	if (m_pMovie)
	{

		u32 extraFlags = 0;
		if (hasAlpha)
		{
			extraFlags |= BINKALPHA;
		}

		const u32 BUFFER_SIZE_MULTIPLIER = 16;
		const u32 READ_AHEAD_BUFFER_SIZE = 524288 * BUFFER_SIZE_MULTIPLIER;

		bwMovie::bwMovieParams movieParams;

		CMovieMgr::GetFullName(movieParams.pFileName, pMovieName);	

		movieParams.extraFlags	= extraFlags;
		movieParams.ioSize		= READ_AHEAD_BUFFER_SIZE;

#if __SCRIPT_MEM_CALC
		movieParams.owner = this;
#endif	//	__SCRIPT_MEM_CALC

		movieParams.loadedCallback = datCallback(MFA1(CMovieMgr::CMovie::LoadedAsyncCb), (datBase*)this, NULL, true);
		if (m_pMovie->SetMovie(movieParams))
		{
			vfxAssertf(GetRefCount() == 0, "CMovieMgr::CMovie::Preload - Reference count should be zero");
			IncRefCount();

			// Load any subtitle files, same name as the movie, but .xml
			StartLoadSubtitles(pMovieName);

			return true;
		}
	}

#if __DEV
	if (PARAM_binkasserts.Get())
	{
		vfxAssertf(0, "Failed to load Bink movie, check console output for further details");
	}
#endif

	vfxAssertf(GetRefCount() == 0, "CMovieMgr::CMovie::Preload - Reference count should be zero");
	IncRefCount();

	SetDoesntExist(true);

	return false;
}


////////////////////////////////////////////////////////////////////////////
//	Subtitles
////////////////////////////////////////////////////////////////////////////

void CMovieMgr::CMovie::StartLoadSubtitles(const char* pName)
{
	char subName[256];
	sprintf(subName, "BINK_%s", pName);

	m_LastEventIndex = 0;
	m_LastShownIndex = -1;

	m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_UNLOADED;

	// See if we can find our name in the metadata
	m_SubObjIDX = g_fwMetaDataStore.FindSlot(subName).Get();
	if( m_SubObjIDX >= 0 )
	{
		// If it was cancelled previously, cancel the cancel
		s32 cancelIDX = m_CancelledRequests.Find(m_SubObjIDX);
		if( cancelIDX != -1 )
		{
			m_CancelledRequests.DeleteFast(cancelIDX);
		}

		g_fwMetaDataStore.StreamingRequest(strLocalIndex(m_SubObjIDX), STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
		m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TITLES;
	}
}

void CMovieMgr::CMovie::UpdateLoadSubtitles()
{
	switch( m_LoadState )
	{
	case	CMovieSubtitleContainer::SUB_LOAD_STATE_UNLOADED:
	case	CMovieSubtitleContainer::SUB_LOAD_STATE_LOADED:
		// Nowt
		break;

	case	CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TITLES:
		if( g_fwMetaDataStore.HasObjectLoaded(strLocalIndex(m_SubObjIDX)) )
		{
			g_fwMetaDataStore.ClearRequiredFlag(m_SubObjIDX, STRFLAG_DONTDELETE);
			g_fwMetaDataStore.AddRef(strLocalIndex(m_SubObjIDX), REF_OTHER);	// Add our own ref, currently 1 at first load

			m_TextSlotID = -1;
			m_pSubtitles = g_fwMetaDataStore.Get(strLocalIndex(m_SubObjIDX))->GetObject< CMovieSubtitleContainer >();

			Assert(m_pSubtitles);

			// Sort the time entries, cos they don't seem to come out in order
			m_pSubtitles->SortEventListByTime();

			// Fixup pointers
			for(int i=0; i<m_pSubtitles->m_pCutsceneEventList.size(); i++)
			{
				CMovieSubtitleEvent *pEvent = m_pSubtitles->m_pCutsceneEventList[i];
				pEvent->m_pEventArgsPtr = m_pSubtitles->m_pCutsceneEventArgsList[pEvent->m_iEventArgsIndex];
			}

			// Now we wanna load this textblock.
			const char *pTextToLoad = m_pSubtitles->m_TextBlockName.c_str();
			// See if any slot already contains our text, or is requesting our text already
			for(int i=0;i<NUM_BINK_MOVIE_TEXT_SLOTS;i++)
			{
				if( TheText.HasThisAdditionalTextLoaded(pTextToLoad, BINK_MOVIE_TEXT_SLOT + i ) ||
					TheText.IsRequestingThisAdditionalText(pTextToLoad, BINK_MOVIE_TEXT_SLOT + i) )
				{
					m_TextSlotID = i;
					break;
				}
			}

			if( m_TextSlotID == -1 )
			{
				// Find an unused textSlot
				for(int i=0;i<NUM_BINK_MOVIE_TEXT_SLOTS;i++)
				{
					if(ms_TextSlotRefs[i] == 0)
					{
						m_TextSlotID = i;
						break;
					}
				}

				Assertf(m_TextSlotID != -1,"CMovie::Unable to find empty TextSlot");
				TheText.RequestAdditionalText(pTextToLoad, BINK_MOVIE_TEXT_SLOT + m_TextSlotID,CTextFile::TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC);
			}

			m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TEXTBLOCK;
			ms_TextSlotRefs[m_TextSlotID]++;
			ms_TextSlotMarkedForDelete[m_TextSlotID] = false;	// If we marked it for delete, prevent that!
		}
		break;

	case	CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TEXTBLOCK:
		{
			if( TheText.HasAdditionalTextLoaded(BINK_MOVIE_TEXT_SLOT + m_TextSlotID) )
			{
				m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_LOADED;
			}
		}
		break;
	}
}


void CMovieMgr::CMovie::ReleaseSubtitles()
{
	// In case we never got to add the ref
	if( m_LoadState == CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TITLES )
	{
		// May or may not have loaded by this time.. what to do here?
		// Add to a list to be processed later.
		m_CancelledRequests.Push(m_SubObjIDX);
	}
	else if(m_LoadState >= CMovieSubtitleContainer::SUB_LOAD_STATE_REQUESTED_TEXTBLOCK )
	{
		// Remove any message we might be displaying
		CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(BINK_MOVIE_TEXT_SLOT + m_TextSlotID, false);

		// We've set a ref on the streaming slot, and set a ref on the textblock by this time. Remove them both
		ms_TextSlotRefs[m_TextSlotID]--;
		if( ms_TextSlotRefs[m_TextSlotID] == 0 )
		{
			// No more references, mark for delete
			ms_TextSlotMarkedForDelete[m_TextSlotID] = true;
		}
		m_TextSlotID = -1;

		g_fwMetaDataStore.RemoveRef(strLocalIndex(m_SubObjIDX), REF_OTHER);
		// Remove it...
		if( g_fwMetaDataStore.GetNumRefs(strLocalIndex(m_SubObjIDX)) == 1 )
		{
			g_fwMetaDataStore.SafeRemove(strLocalIndex(m_SubObjIDX));
		}

		m_SubObjIDX = -1;
	}

	m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_UNLOADED;
}

void CMovieMgr::CMovie::UpdateSubtitles()
{
	UpdateLoadSubtitles();

	if( m_LoadState == CMovieSubtitleContainer::SUB_LOAD_STATE_LOADED )
	{
		// No subs (or we couldn't load them for some reason)
		if(m_pSubtitles->m_pCutsceneEventList.size() == 0 || !ms_bSubsEnabled )
		{
			return;
		}

		// Get the current movie time
		float movieTime = m_pMovie->GetMovieTimeReal();
		float durationAdjust = 0.0f;

		// Movie time has changed, reset
		if(m_pSubtitles->m_pCutsceneEventList[m_LastEventIndex]->m_fTime > movieTime)
		{
			m_LastEventIndex = 0;
		}

		const CMovieSubtitleEventArg *pOutArgs = NULL;
		for(int i=m_LastEventIndex; i<m_pSubtitles->m_pCutsceneEventList.size(); i++)
		{
			CMovieSubtitleEvent &theEvent = *(m_pSubtitles->m_pCutsceneEventList[i]);
			if( theEvent.m_iEventId == MOVIE_SHOW_SUBTITLE_EVENT )
			{
				const CMovieSubtitleEventArg *pArgs = static_cast<const CMovieSubtitleEventArg*>(theEvent.m_pEventArgsPtr);
				if( pArgs )
				{
					// Go through the list and get the start times and end times for anything at this time.
					float startTime = theEvent.m_fTime;
					float endTime = pArgs->m_fSubtitleDuration + startTime;

					if(startTime > movieTime)
					{
						break;
					}

					if( startTime <= movieTime && endTime > movieTime )
					{
						// TODO: Only display a line if the remaining time is longer than a minimum, so it doesn't just flash up and vanish instantly?
						pOutArgs = pArgs;
						m_LastEventIndex = i;
						// The duration must be shortened by the length we're already through (incase we skipped ahead)
						durationAdjust = movieTime - startTime;
					}
				}
			}
		}

		// If pOutArgs == NULL, then we shouldn't be showing anything
		if(pOutArgs == NULL && m_LastShownIndex != -1)
		{
			// Turn off? Not required, but reset shown index
			m_LastShownIndex = -1;
		}
		else if( pOutArgs != NULL && (m_LastEventIndex != m_LastShownIndex ) )
		{ 
			// Something to show
			u32 uSubtitleDuration =  (u32)((pOutArgs->m_fSubtitleDuration  - durationAdjust) * 1000.0f); 
			atHashString textLabel = pOutArgs->m_cName; 
			m_LastShownIndex = m_LastEventIndex;
			char *pString = NULL; 
#if !__FINAL
			pString = TheText.Get(textLabel.GetHash(),textLabel.GetCStr());
#else
			if(TheText.DoesTextLabelExist(textLabel.GetHash()))
			{
				pString = TheText.Get(textLabel.GetHash(),"");
			}
			else
			{
				return; 
			}
#endif

			CMessages::AddMessage( pString, TheText.GetBlockContainingLastReturnedString(), uSubtitleDuration, true, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
		}
	}
}

void CMovieMgr::CMovie::ProcessCancelledSubtitleRequests()
{
	// Process backwards for DeleteFast usage
	for(int i = m_CancelledRequests.size()-1; i >= 0; i--)
	{
		strLocalIndex objIDX = strLocalIndex(m_CancelledRequests[i]);
		if( g_fwMetaDataStore.HasObjectLoaded(objIDX) )
		{
			g_fwMetaDataStore.ClearRequiredFlag(objIDX.Get(), STRFLAG_DONTDELETE);

			// Remove it... if nothing else relies on it.
			if( g_fwMetaDataStore.GetNumRefs(objIDX) == 1 )
			{
				g_fwMetaDataStore.SafeRemove(objIDX);
			}
			m_CancelledRequests.DeleteFast(i);
		}
	}

	// Clear out any textblocks that are marked for delete
	for(int i = 0;i<NUM_BINK_MOVIE_TEXT_SLOTS;i++)
	{
		if( ms_TextSlotMarkedForDelete[i] )
		{
			Assertf(ms_TextSlotRefs[i] == 0, "ERROR: CMovie::ProcessCancelledSubtitleRequests() - Trying to free a textslot that is referenced.");
			// Ensure we get loaded before we try to free
			if( TheText.HasAdditionalTextLoaded(BINK_MOVIE_TEXT_SLOT + i))
			{
				TheText.FreeTextSlot(BINK_MOVIE_TEXT_SLOT + i, false);	// RequestAdditionalText() does not use extra text blocks. 
				ms_TextSlotMarkedForDelete[i] = false;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
//	
////////////////////////////////////////////////////////////////////////////

void CMovieMgr::CMovie::WaitTillLoaded()
{
	// If the movie doesn't exist, then we aren't loaded, and will never be, don't wait!
	if( GetDoesntExist() == false )
	{
		AUTO_HANG_DETECT_CRASH_ZONE;
		while (m_pMovie && !IsLoaded() && !HasAsyncFailed())
		{
	#if !__WIN32PC
			ProcessBinkAllocs();
	#endif // !__WIN32PC
			sysIpcSleep(1);
		}
	}
}


void CMovieMgr::CMovie::Reset()
{
	ResetFlags();
	m_pMovie = NULL;
	m_hashId = INVALID_MOVIE_HANDLE;
	m_volume = 0.0f;
	m_refCount = 0;

	// Subtitles
	m_LoadState = CMovieSubtitleContainer::SUB_LOAD_STATE_UNLOADED;
	m_TextSlotID = -1;

#if __SCRIPT_MEM_CALC
	{
		SYS_CS_SYNC(ms_AllocMapMutex);
		m_MapOfMemoryAllocations.Reset();
	}
#endif	//	__SCRIPT_MEM_CALC
}

void CMovieMgr::CMovie::Release()
{
	vfxAssertf(GetRefCount() == 0, "CMovieMgr::CMovie::Release - Reference count is not zero");

	bwMovie* pMovie;
	{
		pMovie		= m_pMovie;
		m_pMovie	= NULL;
	}

	if( pMovie )
	{
		bwMovie::Destroy(pMovie);
		ReleaseSubtitles();
	}

	Reset();
}

void CMovieMgr::CMovie::Play()
{
	// Can't play anything if it's not yet loaded.
	WaitTillLoaded();

	if(m_pMovie && !IsMarkedForDelete())
	{
		CreateSound();

		u32 timeMs = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		m_pMovie->Play(timeMs);
		SetStopFlag(false);
		SetIsPlaying(true);
		SetIsPaused(false);


	}
}

void CMovieMgr::CMovie::Stop()
{
	// Can't stop anything if it's not yet loaded.
	WaitTillLoaded();

	if(m_pMovie)
	{
		SetIsPlaying(false);
		SetStopFlag(true);
	}
}

void CMovieMgr::CMovie::Pause()
{
	// Can't stop anything if it's not yet loaded.
	WaitTillLoaded();

	if(m_pMovie && m_pMovie->IsPlaying() && !IsMarkedForDelete())
	{
		SetPauseFlag(true);
	}
}

void CMovieMgr::CMovie::Resume() 
{
	// Can't stop anything if it's not yet loaded.
	WaitTillLoaded();

	if(m_pMovie && IsMarkedForDelete() == false && (m_flags & MOVIEFLAG_PAUSED))
	{
		SetStopFlag(false);
		SetIsPlaying(true);
		SetIsPaused(false);
		m_pMovie->Play();
	}
}

float CMovieMgr::CMovie::GetTime() 
{
	WaitTillLoaded();

	if(m_pMovie)
	{
		return m_pMovie->GetMovieTime();
	}

	return 0.0f;
}

float CMovieMgr::CMovie::GetTimeReal() 
{
	WaitTillLoaded();

	if(m_pMovie)
	{
		return m_pMovie->GetMovieTimeReal();
	}

	return 0.0f;
}

void CMovieMgr::CMovie::SetTime(float percentage)
{
	if(m_pMovie && !IsMarkedForDelete())
	{
		m_pMovie->SetMovieTime(percentage);
	}
}

float CMovieMgr::CMovie::SetTimeReal(float time)
{
	if(m_pMovie && !IsMarkedForDelete())
	{
		return m_pMovie->SetMovieTimeReal(time);
	}
	return 0.0f;
}

void CMovieMgr::CMovie::SetVolume(float volume)
{
	const float finalVol = ComputeVolumeOffset() + volume;
	if(m_pSound)
	{
		m_pSound->SetRequestedVolume(finalVol);
	}
	else if (m_pMovie)
	{
		m_pMovie->SetVolume(finalVol);
	}
    m_volume = volume;
}

void CMovieMgr::CMovie::RestoreVolume() 
{
	const float vol = ComputeVolumeOffset() + m_volume;
	if(m_pSound)
	{
		m_pSound->SetRequestedVolume(vol);
	}
	if (m_pMovie)
	{
		m_pMovie->SetVolume(vol);
	}
}

void CMovieMgr::CMovie::SetShouldSkip(bool shouldSkip)
{
	if(m_pMovie)
	{
		m_pMovie->SetShouldSkip(shouldSkip);
	}
}

float CMovieMgr::CMovie::ComputeVolumeOffset() const
{
	return audDriverUtil::ComputeDbVolumeFromLinear(audNorthAudioEngine::GetSfxVolume());
}

#if __SCRIPT_MEM_CALC
u32 CMovieMgr::CMovie::GetMemoryUsage() const
{
	SYS_CS_SYNC(ms_AllocMapMutex);

	u32 TotalMemory = 0;

	atMap< void*, u32 >::ConstIterator memoryMapIterator = m_MapOfMemoryAllocations.CreateIterator();
	while (!memoryMapIterator.AtEnd())
	{
		TotalMemory += memoryMapIterator.GetData();
		memoryMapIterator.Next();
	}

	return TotalMemory;
}

//	SizeOfAllocatedMemory doesn't take alignment into account.
void CMovieMgr::CMovie::AddMemoryAllocationInfo(void *pAddressOfAllocatedMemory, u32 SizeOfAllocatedMemory)
{
	SYS_CS_SYNC(ms_AllocMapMutex);
	u32 *pPreviousEntry = m_MapOfMemoryAllocations.Access(pAddressOfAllocatedMemory);
	if (Verifyf(!pPreviousEntry, "CMovieMgr::CMovie::AddMemoryAllocationInfo - didn't expect m_MapOfMemoryAllocations to already contain an entry for the start address of this allocation"))
	{
		m_MapOfMemoryAllocations.Insert(pAddressOfAllocatedMemory, SizeOfAllocatedMemory);
	}
}

bool CMovieMgr::CMovie::RemoveMemoryAllocationInfo(void *pAddressOfAllocatedMemory)
{
	SYS_CS_SYNC(ms_AllocMapMutex);
	return m_MapOfMemoryAllocations.Delete(pAddressOfAllocatedMemory);
}

#endif	//	__SCRIPT_MEM_CALC


u32 CMovieMgr::CMovie::GetWidth() const
{
	if(m_pMovie)
	{
		return m_pMovie->GetWidth();
	}

	return 0;
}

u32	CMovieMgr::CMovie::GetHeight() const
{
	if(m_pMovie)
	{
		return m_pMovie->GetHeight();
	}

	return 0;
}

bool CMovieMgr::CMovie::BeginDraw()
{
	Assert(sysThreadType::IsRenderThread());

	if(m_pMovie && !GetStopFlag() && !GetPauseFlag() && IsLoaded())	
	{
		SetIsRendering(true);
		if( m_pMovie->BeginDraw() )
		{
			return true;
		}
	}

	SetIsRendering(false);
	return false;
}

void CMovieMgr::CMovie::EndDraw()
{
	Assert(sysThreadType::IsRenderThread());

	// even if movie is marked for delete, if BeginDraw has already been called 
	//(ie: rendering in progress), make sure EndDraw is called
	if(IsRendering())
	{
		bwMovie::ShaderEndDraw(m_pMovie);
		if(m_pMovie)
		{
			m_pMovie->EndDraw();
		}
		SetIsRendering(false);
	}
}

void CMovieMgr::CMovie::UpdateFrame()
{
	UpdateSubtitles();

	u32 timeMs = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_pMovie->UpdateMovieTime(timeMs);
}

s32 CMovieMgr::CMovie::GetFramesRemaining() const
{
	if( m_pMovie )
	{
		return m_pMovie->GetFramesRemaining();
	}

	return 0;
}

float CMovieMgr::CMovie::GetFrame() const
{
	if( m_pMovie )
	{
		return m_pMovie->GetCurrentFrame();
	}
	return 0;
}

float CMovieMgr::CMovie::GetPlaybackRate() const
{
	if( m_pMovie )
	{
		return m_pMovie->GetPlaybackRate();
	}
	return 0;
}

float CMovieMgr::CMovie::GetNextKeyFrame(float time) const
{
	if( m_pMovie )
	{
		return m_pMovie->GetNextKeyFrame(time);
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//  CMovieMgr
///////////////////////////////////////////////////////////////////////////////

CMovieMgr::CMovieMgr()
{
#if BINK_ASYNC

#if RSG_XENON || RSG_DURANGO || RSG_ORBIS || RSG_PC 

	BinkStartAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0, 0 );

#elif RSG_PS3

	g_BinkThreadData.spursEA= (u32*)g_Spurs;
	g_BinkThreadData.elfEA= SPURS_TASK_START(binkspu);

	// We'll use SPU1 and SPU2 for BINK.
	// NOTE that we have to avoid the SPUs that are used by the zlib
	// decompression (currently SPU0 and SPU3) or else we get lockups.
	// See inflateClient.cpp.
	if (!BinkStartAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0, &g_BinkThreadData ))
	{
		Errorf("BinkStartAsyncThread error: %s", BinkGetError());
	}
	if (!BinkStartAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1, &g_BinkThreadData ))
	{
		Errorf("BinkStartAsyncThread error: %s", BinkGetError());
	}

#endif // __PS3

#endif // BINK_ASYNC

	CMovie::ResetAllSubtitleText();
}

CMovieMgr::~CMovieMgr()
{
	ReleaseAll();

#if BINK_ASYNC

#if RSG_XENON || RSG_ORBIS || RSG_DURANGO || RSG_PC 

	BinkRequestStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0 );
	BinkWaitStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0 );

#elif RSG_PS3

	BinkRequestStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0 );
	BinkWaitStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0 );
	BinkRequestStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1 );
	BinkWaitStopAsyncThread( bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1 );

#endif

#endif	// BINK_ASYNC
}

void			CMovieMgr::Init	    					(unsigned initMode)
{

	if (initMode==INIT_CORE)
	{
        bwMovie::Init();

#if GTA_REPLAY
		bwMovie::SetShouldControlPlaybackRateCB(ShouldControlPlaybackRate);
#endif	//GTA_REPLAY

		bwMovie::InitShaders();
		bwMovie::SetTextureMemoryAllocator(MakeFunctorRet(CMovieMgr::AllocateTexture));
		bwMovie::SetTextureMemoryDeallocator(MakeFunctor(CMovieMgr::FreeTexture));

		bwMovie::SetMemoryFuncs(BinkReplacementAlloc, BinkReplacementFree);
		bwAudio::SetAllocator(MakeFunctorRet(CMovieMgr::AllocateAsync));
		bwAudio::SetDeallocator(MakeFunctor(CMovieMgr::FreeAsync));

		m_movies.Reset();
		m_movies.Reserve(MAX_ACTIVE_MOVIES);
		
		// reset pool
		for (int i = 0; i < MAX_ACTIVE_MOVIES; i++) 
		{	
			CMovie& curMovieInst = m_movies.Append();
			curMovieInst.Reset();
		}
	}
	else if (initMode==INIT_AFTER_MAP_LOADED)
	{
		m_movies.Reset();
		m_movies.Reserve(MAX_ACTIVE_MOVIES);

		// reset pool
		for (int i = 0; i < MAX_ACTIVE_MOVIES; i++) 
		{	
			CMovie& curMovieInst = m_movies.Append();
			curMovieInst.Reset();
		}

		// bwMovie::AllocSplitShader();		// split off shader allocs to reduce fragmentation MIGRATE_FIXME
	}
#if __BANK
	CBinkDebugTool::Init(initMode);
#endif

	DumpDebugInfo("test");
}

void CMovieMgr::Shutdown(unsigned shutdownMode)
{

#if __BANK
	CBinkDebugTool::Shutdown(shutdownMode);
#endif

	if (shutdownMode==SHUTDOWN_WITH_MAP_LOADED || shutdownMode == SHUTDOWN_SESSION)
	{
		DeleteAll();
		// bwMovie::DeallocSplitShader(); MIGRATE_FIXME

#if __SCRIPT_MEM_CALC
		ms_pCurrentMovieForAllocations = NULL;
#endif	//	__SCRIPT_MEM_CALC
	}

	if (shutdownMode==SHUTDOWN_CORE)
	{
		bwMovie::ShutdownShaders();
		bwMovie::Shutdown();
	}
}

#if __BANK
void FindFileCallback(const fiFindData& findData, void* userData)
{
	Assert(userData);
	atArray<fiFindData*>* files = reinterpret_cast<atArray<fiFindData*>*>(userData);
	if ((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && (findData.m_Name[0] != '.'))
	{
		files->PushAndGrow(rage_new fiFindData(findData));
	}
}

void CMovieMgr::InitWidgets()
{
	vfxWidget::GetBank()->PushGroup("Movie Manager");
		
		atArray<fiFindData*> files;
		ASSET.EnumFiles("platform:/movies", FindFileCallback, &files);
		ASSET.EnumFiles("dlcMovies:/", FindFileCallback, &files, true);
		if (files.GetCount() > 0)
		{
			ms_movieCombo = vfxWidget::GetBank()->AddCombo("Movies", &ms_movieIndex, files.GetCount(), (const char**)&files[0]);
		}
		else
		{
			ms_movieCombo = vfxWidget::GetBank()->AddCombo("Movies", &ms_movieIndex, files.GetCount(), NULL);
		}

		vfxWidget::GetBank()->AddToggle("Enable debug render", &ms_debugRender, NullCB, "Renders a quad with the current active movie");
		vfxWidget::GetBank()->AddButton("Play", datCallback(MFA(CMovieMgr::PlayMovie), (datBase*)this));
		vfxWidget::GetBank()->AddButton("Stop", datCallback(MFA(CMovieMgr::StopMovie), (datBase*)this));
		vfxWidget::GetBank()->AddSlider("Size percentage", &ms_movieSize, 10, 100, 1);


	vfxWidget::GetBank()->PopGroup();

	CBinkDebugTool::InitWidgets();

	InitRenderStateBlocks();
}

void CMovieMgr::PlayMovie()
{
	const char* movieName = ms_movieCombo->GetString(ms_movieIndex);
	if (!movieName)
	{
		return;
	}

    char movieNameNoExt[64] = {0};
    safecpy(movieNameNoExt, movieName);
    char* extPos = movieNameNoExt + strlen(movieNameNoExt) - 4;
    if (!strcmp(extPos, ".bik"))
        extPos[0] = '\0';

	ms_curMovieHandle = PreloadMovie(movieNameNoExt, true);
	Play(ms_curMovieHandle);
}

void CMovieMgr::StopMovie()
{
	Stop(ms_curMovieHandle);
	Release(ms_curMovieHandle);
	ms_curMovieHandle = INVALID_MOVIE_HANDLE;
}

void CMovieMgr::RenderDebug()
{

	if (ms_curMovieHandle == INVALID_MOVIE_HANDLE || !ms_debugRender)
	{
		return;
	}

	grcViewport *prevVP = grcViewport::GetCurrent();

	grcViewport::SetCurrent(grcViewport::GetDefaultScreen());
	grcViewport::SetCurrentWorldIdentity();
	grcBindTexture(NULL);

	// set render state blocks for debug rendering
	grcStateBlock::SetRasterizerState(ms_debugRasterizerState);
	grcStateBlock::SetBlendState(ms_debugBlendState);
	grcStateBlock::SetDepthStencilState(ms_debugDepthStencilState);

	if(BeginDraw(ms_curMovieHandle))
	{
		float width = GRCDEVICE.GetWidth() * (ms_movieSize * 0.01f);
		float height = GRCDEVICE.GetHeight() * (ms_movieSize * 0.01f);
		float widthOffset = (GRCDEVICE.GetWidth() * 0.5f) - (width * 0.5f);
		float heightOffset = (GRCDEVICE.GetHeight() * 0.5f) - (height * 0.5f);

		grcBegin(drawTriStrip, 4);
			grcColor3f(1.f, 1.f, 1.f);

			grcTexCoord2f(0.f, 1.f);	grcVertex2f(widthOffset,			height + heightOffset);
			grcTexCoord2f(0.f, 0.f);	grcVertex2f(widthOffset,			heightOffset);
			grcTexCoord2f(1.f, 1.f);	grcVertex2f(width + widthOffset,	height + heightOffset);
			grcTexCoord2f(1.f, 0.f);	grcVertex2f(width + widthOffset,	heightOffset);
		grcEnd();

		EndDraw(ms_curMovieHandle);
	}

	grcViewport::SetCurrent(prevVP);

	// restore render state to previous state
	grcStateBlock::SetRasterizerState(ms_defRasterizerState);
	grcStateBlock::SetBlendState(ms_defBlendState);
	grcStateBlock::SetDepthStencilState(ms_defDepthStencilState);
}

void CMovieMgr::RenderDebug3D() 
{
	CBinkDebugTool::RenderDebug();
}
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
void CMovieMgr::AddMemoryAllocationInfo(void *pAddressOfAllocatedMemory, u32 SizeOfAllocatedMemory)
{
	if (ms_pCurrentMovieForAllocations)
	{
		ms_pCurrentMovieForAllocations->AddMemoryAllocationInfo(pAddressOfAllocatedMemory, SizeOfAllocatedMemory);
	}
}

void CMovieMgr::RemoveMemoryAllocationInfo(void *pAddressOfMemoryAllocation)
{
	for (int movie_loop = 0; movie_loop < m_movies.GetCount(); movie_loop++)
	{
		m_movies[movie_loop].RemoveMemoryAllocationInfo(pAddressOfMemoryAllocation);
	}
}

u32 CMovieMgr::GetMemoryUsage(u32 handle) const
{
	const CMovie* pMovie = FindMovie(handle);

	if (pMovie)
	{
		return pMovie->GetMemoryUsage();
	}

	return 0;
}
#endif	//	__SCRIPT_MEM_CALC

void CMovieMgr::UpdateFrame()
{
	ProcessBinkAllocs();

	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		if (!m_movies[i].IsRendering())
		{
			if (m_movies[i].GetStopFlag())
			{
				m_movies[i].StopSound();

				m_movies[i].SetStopFlag(false);
				m_movies[i].SetIsPlaying(false);
				m_movies[i].SetIsPaused(false);
				m_movies[i].GetMovie()->Stop();

			}

			if (m_movies[i].GetPauseFlag())
			{
				m_movies[i].SetIsPlaying(false);
				m_movies[i].SetIsPaused(true);
				m_movies[i].GetMovie()->Stop();

				m_movies[i].SetPauseFlag(false);
			}

			if (m_movies[i].HasAsyncFailed())
			{
				ReleaseOnFailure(m_movies[i].GetId());
			}

			if (m_movies[i].IsMarkedForDelete())
			{
				m_movies[i].IncDeleteCount();
				if( m_movies[i].GetDeleteCount() >= 2 )
				{
					Delete(m_movies[i].GetId());
				}
			}
		}
	}

#if GTA_REPLAY
	atFixedArray<ReplayMovieControllerNew::tMovieInfo, MAX_ACTIVE_MOVIES> movieInfo;
#endif	//GTA_REPLAY

	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		CMovie& curMovie = m_movies[i];

		if (curMovie.IsLoaded() == true) 
		{
			curMovie.UpdateFrame();
		}

#if GTA_REPLAY
		if(curMovie.IsLoaded())
		{
			ReplayMovieControllerNew::tMovieInfo info;
			strncpy(info.movieName, curMovie.GetName(), sizeof(info.movieName));
			info.movieTime = curMovie.GetTimeReal();
			info.isPlaying = curMovie.IsPlaying();
			movieInfo.Append() = info;
		}

		if(CReplayMgr::ShouldRecord() && curMovie.GetEntity())
		{
			CReplayMgr::RecordFx<CPacketMovieEntity>( CPacketMovieEntity(curMovie.GetName(), curMovie.IsFrontendAudio(), curMovie.GetEntity()));
		}
		
#endif	//GTA_REPLAY

	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketMovie2>( CPacketMovie2(movieInfo) );
	}
#endif	//GTA_REPLAY


#if __BANK
	CBinkDebugTool::Update();
#endif

	CMovie::ProcessCancelledSubtitleRequests();
}

void CMovieMgr::ProcessBinkAllocs()
{
	CMovieMgr::sAllocData newAlloc;
	while (ms_pendingAllocQueue.PopPoll(newAlloc))
	{
		if (newAlloc.alloc)
		{
			newAlloc.ptr = Allocate(newAlloc.size, newAlloc.alignment);
			ms_doneAllocQueue.Push(newAlloc);
		}
		else
		{
			Free(newAlloc.ptr);
		}
	}
}

bool CMovieMgr::BeginDraw(u32 hashId)
{
	Assert(sysThreadType::IsRenderThread());

	CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		return pMovie->BeginDraw();
	}

	return false;
}

void CMovieMgr::EndDraw(u32 hashId)
{
	Assert(sysThreadType::IsRenderThread());

	// We're blacked out unless the movie says we're not
	bool	bBlackOut = true;

	CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		pMovie->EndDraw();

		if(pMovie->GetMovie() && !pMovie->GetMovie()->IsWithinBlackout())
		{
			bBlackOut = false;
		}
	}

	if(bBlackOut)
	{
		// Draw black square over entire rendertarget
		CSprite2d::DrawRect(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, Color_black);
	}
}

int	CMovieMgr::PreloadMovieFromScript(const char* pName) 
{

	u32 handle = PreloadMovie(pName, true);

	if (handle == INVALID_MOVIE_HANDLE)
	{
		return 0;
	}

	int slot = FindFreeScriptSlot(handle);

	if (slot == -1)
	{
		vfxAssertf(0, "CMovieMgr::PreloadMovieFromScript - no script slots available");
		return 0;
	}
	m_scriptSlots[slot] = handle;
	return slot+1;
}

int CMovieMgr::FindFreeScriptSlot(u32 handle) const
{
	// check if movie is already being used
	for (int i=0; i<MAX_SCRIPT_SLOTS; i++)
	{
		if (m_scriptSlots[i] == handle)
		{
			return i;
		}
	}

	// check for a free slot
	for (int i=0; i<MAX_SCRIPT_SLOTS; i++)
	{
		if (m_scriptSlots[i] == INVALID_MOVIE_HANDLE)
		{
			return i;
		}
	}

	return -1;
}

void CMovieMgr::ReleaseScriptSlot(u32 handle)
{
	for (int i=0; i<MAX_SCRIPT_SLOTS; i++)
	{
		if (m_scriptSlots[i] == handle)
		{
			m_scriptSlots[i] = INVALID_MOVIE_HANDLE;
		}
	}
}

u32 CMovieMgr::PreloadMovie(const char* pMovieName, bool hasAlpha)
{
	vfxAssertf(pMovieName, "CMovieMgr::PreloadMovie - invalid movie name");

	USE_MEMBUCKET(MEMBUCKET_FX);

	u32 hashId = atStringHash(pMovieName);

	// check if movie is already active
	CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		// tbr: avoid duplicates - if the movie's already been tagged for deletion
		// and we don't remove the flag a duplicate will be created: we'll have
		// two Bink streams for the same file and the reference counting mechanism 
		// will implode (checks are based on the hashId and there will be more than
		// entry with the same id)
		pMovie->UnmarkForDelete();

		pMovie->IncRefCount();
		return hashId;
	}

	pMovie = GetFreeMovie();
	if (pMovie == NULL)
	{
		return INVALID_MOVIE_HANDLE;
	}

	vfxAssertf(pMovie->GetRefCount() == 0, "CMovieMgr::PreloadMovie - Reference count should be zero");

	pMovie->SetId(hashId);

#if GTA_REPLAY
	pMovie->SetName(pMovieName);
#endif //GTA_REPLAY

	if (!pMovie->Preload(pMovieName, hasAlpha))
	{
#if __DEV
		if (PARAM_binkasserts.Get())
		{
			vfxAssertf(0, "Failed to load Bink movie, check console output for further details");
		}
#endif

		ReleaseOnFailure(hashId);
		hashId = INVALID_MOVIE_HANDLE;
	}

	return hashId;
}

CMovieMgr::CMovie* CMovieMgr::GetFreeMovie() 
{
	bwMovie* pMovie = bwMovie::Create();
	pMovie->SetControlledByAnother();

#if RSG_PC
	// if there was no audio device when the game started the bink may have not have re-initialized correctly
	if(!bwMovie::IsUsingRAGEAudio())
	{
		bwMovie::Init();
	}
#endif

	// try re-using a recently released slot
	int movieCount = m_movies.GetCount();

	for (int i = 0; i < movieCount; i++)
	{
		if (m_movies[i].GetMovie() == NULL)
		{
			m_movies[i].SetMovie(pMovie);
			return &m_movies[i];
		}
	}
	
	return NULL;
}

CMovieMgr::CMovie* CMovieMgr::FindMovie(u32 hashId)
{
	if (hashId != INVALID_MOVIE_HANDLE)
	{
		for (int i = 0; i < m_movies.GetCount(); i++)
		{
			if (m_movies[i].GetId() == hashId)
			{
				return &m_movies[i];
			}
		}
	}

	return NULL;
}

const CMovieMgr::CMovie* CMovieMgr::FindMovie(u32 hashId) const
{
	if (hashId != INVALID_MOVIE_HANDLE)
	{
		for (int i = 0; i < m_movies.GetCount(); i++)
		{
			if (m_movies[i].GetId() == hashId)
			{
				return &m_movies[i];
			}
		}
	}

	return NULL;
}

void CMovieMgr::CMovie::CreateSound()
{
	if(bwMovie::IsUsingRAGEAudio())
	{
		Displayf("Trying to play bink audio using game engine");
		Assert(m_pMovie);
		const bwAudio::Info *info = m_pMovie->FindAudio();
		// If 'info' is NULL then there is no audio track
		if(info && info->ringBuffer)
		{
			audSoundInitParams initParams;

			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
			if(m_pEntity && !m_frontendAudio)
			{
				// Try and grab an existing environmentgroup, and if we don't find one then allocate a new group that is destroyed when the sound ends
				naEnvironmentGroup* environmentGroup = (naEnvironmentGroup*)m_pEntity->GetAudioEnvironmentGroup();
				if(!environmentGroup)
				{
					environmentGroup = naEnvironmentGroup::Allocate("Movie");
					if(environmentGroup)
					{
						environmentGroup->Init(NULL, 20);	// Init with NULL entity so that it's destroyed when the sound is no longer playing
						environmentGroup->SetSource(m_pEntity->GetTransform().GetPosition());
						environmentGroup->SetInteriorInfoWithEntity(m_pEntity);
						if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
						{
							environmentGroup->SetUsePortalOcclusion(true);
							environmentGroup->SetMaxPathDepth(audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());
						}
					}
				}

				initParams.Tracker = m_pEntity->GetPlaceableTracker();
				initParams.EnvironmentGroup = environmentGroup;
				Displayf("Trying to play bink audio positionally using game engine");
			}
			else
			{
				m_frontendAudio = true;
				// for now force the sound to play with a 2d pan
				initParams.Pan = 0;
				Displayf("Trying to play bink audio FE using game engine");
			}

			if(m_pSound)
			{
				audWarningf("Stopping existing MovieManager (Bink) sound to play a new one");
				m_pSound->StopAndForget();
			}
		
			const char *soundName = info->numChannels == 1 ? "BINK_MONO_SOUND" : "BINK_STEREO_SOUND";

			g_FrontendAudioEntity.CreateSound_PersistentReference(soundName, &m_pSound, &initParams);

			if(m_pSound)
			{
				Assert(m_pSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID);
			
				audExternalStreamSound *str = reinterpret_cast<audExternalStreamSound*>(m_pSound);	
			
				str->InitStreamPlayer(info->ringBuffer, info->numChannels, info->sampleRate);
				str->PrepareAndPlay();
				Displayf("Playing bink audio using game engine");
			}
		}
		else
		{
			// If we get to hear then either the movie has no audio track, or something went wrong and Bink is using
			// it's internal audio system rather than the one we specified, in which case it will play very loudly.
			SetVolume(-28.f);
			Displayf("No audio track - bink audio using bink engine");
		}
	}
}

void CMovieMgr::CMovie::StopSound()
{
	if(m_pSound)
	{
		if(m_pSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID)
		{
			((audExternalStreamSound*)m_pSound)->StopStreamPlayback();
		}
		m_pSound->StopAndForget();
	}
}

void CMovieMgr::Release(u32 hashId)
{
	CMovie* pMovieInst = FindMovie(hashId);
	if (pMovieInst != NULL && pMovieInst->IsMarkedForDelete() == false)
	{
		vfxAssertf(pMovieInst->GetRefCount() != 0, "CMovieMgr::Release - Reference count should be zero");
		pMovieInst->DecRefCount();
		
		if (pMovieInst->GetRefCount() == 0) 
		{
			pMovieInst->MarkForDelete();
		}
	}

}

void CMovieMgr::ReleaseOnFailure(u32 id)
{
	CMovie* pMovieInst = FindMovie(id);
	
	if (pMovieInst == NULL)
	{
		return;
	}
	
	// remove all references
	while (pMovieInst->IsMarkedForDelete() == false) 
	{
		Release(id);
	}

	// delete in place
	Delete(id);
}

void CMovieMgr::Delete(u32 hashId)
{
	CMovie* pMovieInst = FindMovie(hashId);

	if (pMovieInst != NULL)
	{
		vfxAssertf(pMovieInst->GetRefCount() == 0, "CMovieMgr::Release - Reference count should be zero");
		pMovieInst->Stop();
		pMovieInst->Release();
		ReleaseScriptSlot(hashId);
	}
}

void CMovieMgr::SetFrontendAudio(u32 handle, bool frontend)
{
	CMovie* pMovieInst = FindMovie(handle);

	if (pMovieInst != NULL)
	{
		pMovieInst->SetFrontendAudio(frontend);
	}
}

void CMovieMgr::AttachAudioToEntity(u32 handle, CEntity *entity)
{
	CMovie* pMovieInst = FindMovie(handle);

	if (pMovieInst != NULL)
	{
		pMovieInst->AttachAudioToEntity(entity);
	}
}

void CMovieMgr::DeleteAll()
{
	ReleaseAll();

	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		Delete(m_movies[i].GetId());
	}

	CMovie::ProcessCancelledSubtitleRequests();
}

void CMovieMgr::ReleaseAll()
{
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		Stop(m_movies[i].GetId());
		Release(m_movies[i].GetId());
	}
}

float CMovieMgr::GetTime(u32 hashId)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie)
	{
		return pMovie->GetTime();
	}

	return 0.0f;
}


float CMovieMgr::GetTimeReal(u32 hashId)
{
	CMovie* pMovie = FindMovie(hashId);
	if (pMovie)
	{
		return pMovie->GetTimeReal();
	}

	return 0.0f;
}

void CMovieMgr::SetTime(u32 hashId, float percentage)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie)
	{
		pMovie->SetTime(percentage);
	}
}

float CMovieMgr::SetTimeReal(u32 hashId, float time)
{
	CMovie* pMovie = FindMovie(hashId);
	if (pMovie)
	{
		return pMovie->SetTimeReal(time);
	}
	return 0.0f;
}

void CMovieMgr::SetVolume(u32 hashId, float vol)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie != NULL)
	{
		pMovie->SetVolume(vol);
	}
}

void CMovieMgr::MuteAll()
{
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		m_movies[i].SetVolume(-100.0f);
	}
}

void CMovieMgr::Mute(u32 hashId)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie != NULL)
	{
		pMovie->SetVolume(-100.0f);
	}
}

void CMovieMgr::SetShouldSkip(u32 hashId, bool shouldSkip)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie != NULL)
	{
		pMovie->SetShouldSkip(shouldSkip);
	}
}

void CMovieMgr::PauseAll()
{
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		m_movies[i].Pause();
	}
}

void CMovieMgr::Stop(u32 hashId)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie != NULL)
	{
		pMovie->Stop();
	}
}

void CMovieMgr::ResumeAll()
{
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		m_movies[i].Resume();
	}
}

void CMovieMgr::Play (u32 hashId)
{
	CMovie* pMovie = FindMovie(hashId);

	if (pMovie != NULL && pMovie->IsPlaying() == false)
	{
		pMovie->Play();
	}
}

void CMovieMgr::SetLooping(u32 hashId, bool bSet)
{
	CMovie* pMovie = FindMovie(hashId);
	{
		if (pMovie != NULL && pMovie->IsPlaying())
		{
			pMovie->GetMovie()->SetLooping(bSet);
		}
	}
}

bool CMovieMgr::IsPlaying(u32 hashId)	const
{
	const CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		return pMovie->IsPlaying();
	}
	return false;
}

bool CMovieMgr::IsLoaded(u32 hashId)	const
{
	const CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		return pMovie->IsLoaded();
	}
	return false;
}


bool CMovieMgr::IsAnyPlaying() const
{
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		if (m_movies[i].IsPlaying()) 
		{
			return true;
		}
	}

	return false;
}

u32 CMovieMgr::GetWidth(u32 hashId) const 
{
	const CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		return pMovie->GetWidth();
	}

	return 0;
}

u32 CMovieMgr::GetHeight(u32 hashId) const
{
	const CMovie* pMovie = FindMovie(hashId);
	if (pMovie != NULL)
	{
		return pMovie->GetHeight();
	}
	return 0;
}

float	CMovieMgr::GetCurrentFrame(u32 id) const
{
	const CMovie* pMovie = FindMovie(id);
	if (pMovie != NULL)
	{
		return pMovie->GetFrame();
	}

	Assertf(0, "CMovieMgr::GetCurrentFrame - No Movie Loaded or playing");
	return 0.0f;
}

float	CMovieMgr::GetPlaybackRate(u32 id) const
{
	const CMovie* pMovie = FindMovie(id);
	if (pMovie != NULL)
	{
		return pMovie->GetPlaybackRate();
	}
	Assertf(0, "CMovieMgr::GetPlaybackRate - No Movie Loaded or playing");
	return 0.0f;
}

float	CMovieMgr::GetNextKeyFrame(u32 id, float time) const
{
	const CMovie* pMovie = FindMovie(id);
	if (pMovie != NULL)
	{
		return pMovie->GetNextKeyFrame(time);
	}
	Assertf(0, "CMovieMgr::GetNextKeyFrame - No Movie Loaded or playing");
	return 0.0f;
}

void CMovieMgr::GetFullName(char* buffer, const char* movieName)
{
	if (CFileMgr::NeedsPath(movieName))
	{
		sprintf(buffer, "platform:/movies/%s", movieName);
		if(!ASSET.Exists(buffer,"bik"))
		{
			memset(buffer,0,80);
			sprintf(buffer, "dlcMovies:/%s", movieName);
		}
	} 
	else 
	{
		sprintf(buffer, "%s", movieName);
	}

	char* pPeriod = strchr(buffer, '.');
	if (pPeriod)
	{
		*pPeriod = '\0';
	}
}

u32 CMovieMgr::GetNumMovies() const
{
	u32 numMovies = 0;
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		if (m_movies[i].GetMovie() != NULL)
		{
			numMovies++;
		}
	}

	return numMovies;
}

u32 CMovieMgr::GetNumRefs() const
{
	u32 numRefs = 0;
	for (int i = 0; i < m_movies.GetCount(); i++)
	{
		numRefs += m_movies[i].GetRefCount();
	}

	return numRefs;
}

// Just opens the file and gets it's duration, used in TVPlaylistManager
float CMovieMgr::GetMovieDurationQuick(const char *pMovieName)
{
	char movieName[80];
	CMovieMgr::GetFullName(movieName, pMovieName);

	CMovie	aMovie;

	// Tried to circumvent this by calling directly down to bwMovie, but the allocations tracker asserts
#if __SCRIPT_MEM_CALC
	ms_pCurrentMovieForAllocations = &aMovie;
#endif	//	__SCRIPT_MEM_CALC

	float duration = aMovie.GetMovieDurationQuick(movieName);

#if __SCRIPT_MEM_CALC
	ms_pCurrentMovieForAllocations = NULL;
#endif	//	__SCRIPT_MEM_CALC

	return duration;
}


u32 CMovieMgr::GetHandleFromScriptSlot(int slot) const
{
	if (Verifyf(slot > 0 && slot <= MAX_SCRIPT_SLOTS, "CMovieMgr::GetHandleFromScriptSlot: script passed in an invalid slot: %d", slot))
	{
		return m_scriptSlots[slot-1]; 
	} 

	return INVALID_MOVIE_HANDLE;
}


//////////////////////////////////////////////////////

#if GTA_REPLAY
bool	CMovieMgr::ShouldControlPlaybackRate()
{
	if( CReplayMgr::IsReplayInControlOfWorld() )
	{
		return true;
	}
	return false;
}
#endif	//GTA_REPLAY


//////////////////////////////////////////////////////

#if __BANK

atArray<CBinkDebugTool::binkDebugBillboard> CBinkDebugTool::ms_billboards;
bkCombo*									CBinkDebugTool::ms_movieCombo = NULL;
s32											CBinkDebugTool::ms_movieIndex = 0;
float										CBinkDebugTool::ms_camDistance = 2.0f;
float										CBinkDebugTool::ms_movieSizeScale = 1.0f;
s32											CBinkDebugTool::ms_numMovies = 0;
u32											CBinkDebugTool::ms_numRefs = 0;

grcBlendStateHandle							CBinkDebugTool::ms_defBlendState;
grcDepthStencilStateHandle					CBinkDebugTool::ms_defDepthStencilState;
grcRasterizerStateHandle					CBinkDebugTool::ms_defRasterizerState;

grcBlendStateHandle							CBinkDebugTool::ms_debugBlendState;
grcDepthStencilStateHandle					CBinkDebugTool::ms_debugDepthStencilState;
grcRasterizerStateHandle					CBinkDebugTool::ms_debugRasterizerState;



void CBinkDebugTool::InitRenderStateBlocks()
{
	// default state blocks to restore to once we're done with debug rendering
	grcRasterizerStateDesc defaultExitStateR;
	defaultExitStateR.CullMode = grcRSV::CULL_NONE;
	defaultExitStateR.HalfPixelOffset = 1;
	ms_defRasterizerState = grcStateBlock::CreateRasterizerState(defaultExitStateR);

	grcBlendStateDesc defaultExitStateB;
	defaultExitStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultExitStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultExitStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ms_defBlendState = grcStateBlock::CreateBlendState(defaultExitStateB);

	grcDepthStencilStateDesc defaultExitStateDS;
	defaultExitStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	ms_defDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultExitStateDS);

	// state blocks for debug render
	grcRasterizerStateDesc defaultStateR;
	defaultStateR.CullMode = grcRSV::CULL_NONE;
	defaultStateR.HalfPixelOffset = 1;
	ms_debugRasterizerState = grcStateBlock::CreateRasterizerState(defaultStateR);

	grcBlendStateDesc defaultStateB;
	defaultStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ms_debugBlendState = grcStateBlock::CreateBlendState(defaultStateB);

	grcDepthStencilStateDesc defaultStateDS;
	defaultStateDS.DepthFunc = grcRSV::CMP_ALWAYS;
	defaultStateDS.DepthWriteMask = 0;
	ms_debugDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultStateDS);
}

void CBinkDebugTool::Init(u32 initMode)
{
	if (initMode==INIT_CORE || initMode == INIT_AFTER_MAP_LOADED)
	{
		ms_billboards.Reset();
		ms_billboards.Reserve(MAX_BINK_BILLBOARDS);

		InitRenderStateBlocks();
	}
}

void CBinkDebugTool::Shutdown(u32 UNUSED_PARAM(shutdownMode))
{
	RemoveAll();
	ms_billboards.Reset();
}

void CBinkDebugTool::InitWidgets()
{

	vfxWidget::GetBank()->PushGroup("Bink Debug Tool");

	vfxWidget::GetBank()->AddTitle("");
	vfxWidget::GetBank()->AddSlider("Num movies loaded", &ms_numMovies, 0, MAX_ACTIVE_MOVIES, 0);
	vfxWidget::GetBank()->AddSlider("Num movie references", &ms_numRefs, 0, 256, 0);
	vfxWidget::GetBank()->AddTitle("");

	atArray<fiFindData*> files;
	ASSET.EnumFiles("platform:/movies", FindFileCallback, &files);
	ASSET.EnumFiles("dlcMovies:/", FindFileCallback, &files, true);
	if (files.GetCount() > 0)
	{
		ms_movieCombo = vfxWidget::GetBank()->AddCombo("Movies", &ms_movieIndex, files.GetCount(), (const char**)&files[0]);
	}
	else
	{
		ms_movieCombo = vfxWidget::GetBank()->AddCombo("Movies", &ms_movieIndex, files.GetCount(), NULL);
	}

	vfxWidget::GetBank()->AddSlider("Distance From Camera", &ms_camDistance, 1.0f, 20.0f, 0.1f);
	vfxWidget::GetBank()->AddSlider("Size Scalar", &ms_movieSizeScale, 0.1f, 3.0f, 0.1f);
	vfxWidget::GetBank()->AddButton("Place Billboard In Front Of Camera", PlaceInfrontOfCamera);
	vfxWidget::GetBank()->AddButton("Remove Billboard In Front Of Camera", RemoveInfrontOfCamera);
	vfxWidget::GetBank()->AddButton("Remove All", RemoveAll);


	vfxWidget::GetBank()->PopGroup();

}

void CBinkDebugTool::Update()
{
	ms_numMovies = g_movieMgr.GetNumMovies();
	ms_numRefs = g_movieMgr.GetNumRefs();
}

void CBinkDebugTool::RenderDebug()
{

	if (ms_billboards.GetCount() == 0)
	{
		return;
	}

	// set render state blocks for debug rendering
	grcStateBlock::SetRasterizerState(ms_debugRasterizerState);
	grcStateBlock::SetBlendState(ms_debugBlendState);
	grcStateBlock::SetDepthStencilState(ms_debugDepthStencilState);

	for (int i = 0; i < ms_billboards.GetCount(); i++)
	{
		const binkDebugBillboard& curBillboard = ms_billboards[i];

		Assert(curBillboard.movieHandle != INVALID_MOVIE_HANDLE);

		// shouldn't happen, skip if it does
		if (curBillboard.movieHandle == INVALID_MOVIE_HANDLE) 
		{
			continue;
		}

		Mat34V vWorldMat;
		Vec3V vSide = Normalize(Cross(curBillboard.vUp,curBillboard.vForward));
		Vec3V vUp = Normalize(curBillboard.vUp);
		Vec3V vForward = Normalize(curBillboard.vForward);

		vWorldMat.Seta(vSide);
		vWorldMat.Setb(vUp);
		vWorldMat.Setc(vForward);
		vWorldMat.Setd(curBillboard.vPos);
		
		grcWorldMtx(vWorldMat);

		if (g_movieMgr.BeginDraw(curBillboard.movieHandle))
		{
			grcBegin(drawTriStrip, 4);
			grcColor3f(1.f, 1.f, 1.f);

			float w = (float)g_movieMgr.GetWidth(curBillboard.movieHandle);
			float h = (float)g_movieMgr.GetHeight(curBillboard.movieHandle); 

			float halfWidth = 0.5f;
			float halfHeight = 0.5f;

			if (w > h)
			{
				halfHeight = h/w*0.5f;
			} 
			else 
			{
				halfWidth = w/h*0.5f;
			}

			halfWidth *= ms_movieSizeScale;
			halfHeight *= ms_movieSizeScale;

			grcTexCoord2f(0.f, 1.f);	grcVertex2f(-halfWidth, halfHeight);
			grcTexCoord2f(0.f, 0.f);	grcVertex2f(-halfWidth, -halfHeight);
			grcTexCoord2f(1.f, 1.f);	grcVertex2f(halfWidth, halfHeight);
			grcTexCoord2f(1.f, 0.f);	grcVertex2f(halfWidth, -halfHeight);
			grcEnd();

			g_movieMgr.EndDraw(curBillboard.movieHandle);
		}
	}

	// restore render state to previous state
	grcStateBlock::SetRasterizerState(ms_defRasterizerState);
	grcStateBlock::SetBlendState(ms_defBlendState);
	grcStateBlock::SetDepthStencilState(ms_defDepthStencilState);
}

void CBinkDebugTool::PlaceInfrontOfCamera()
{
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
	Vec3V_ConstRef vCamUp = RCC_VEC3V(camInterface::GetUp());

	ScalarV vCamDistance = ScalarVFromF32(ms_camDistance);

	u32 movieHandle = RequestMoviePlayback();

	if (movieHandle != INVALID_MOVIE_HANDLE)
	{
		Vec3V vPos = vCamPos+(vCamForward*vCamDistance);
		AddBillboard(vPos, vCamForward, Negate(vCamUp), movieHandle);
	}

}

void CBinkDebugTool::RemoveInfrontOfCamera()
{
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());

	// assume unit sphere times ms_movieSizeScale
	ScalarV vRadSqr = ScalarVFromF32(0.25f*ms_movieSizeScale);

	for (int i = 0; i < ms_billboards.GetCount(); i++)
	{
		// simple line-sphere intersection test
		Vec3V vQuadPos = ms_billboards[i].vPos;
		Vec3V vDif = vCamPos - vQuadPos;
		ScalarV vA0		= Dot(vDif, vDif) - vRadSqr;
		ScalarV vA1		= Dot(vCamForward, vDif);
		ScalarV vDisc	= vA1*vA1 - vA0;

		if (IsGreaterThanOrEqual(vDisc, ScalarV(V_ZERO)).Getb())
		{
			g_movieMgr.Release(ms_billboards[i].movieHandle);
			ms_billboards.Delete(i);
			break;
		}
	}
}

void CBinkDebugTool::RemoveAll()
{
	for (int i = 0; i < ms_billboards.GetCount(); i++)
	{
		g_movieMgr.Release(ms_billboards[i].movieHandle);
	}
	ms_billboards.Reset();
	ms_billboards.Reserve(MAX_BINK_BILLBOARDS);
}

u32 CBinkDebugTool::RequestMoviePlayback()
{
	const char* movieName = ms_movieCombo->GetString(ms_movieIndex);

	if (movieName == NULL)
	{
		return INVALID_MOVIE_HANDLE;
	}

	u32 movieHandle = g_movieMgr.PreloadMovie(movieName, true);
	
	g_movieMgr.Play(movieHandle);

	return movieHandle;
}

void CBinkDebugTool::AddBillboard(Vec3V_In vPos, Vec3V_In vForward, Vec3V_In vUp, u32 movieHandle) 
{
	Assertf(movieHandle != INVALID_MOVIE_HANDLE, "[CBinkDebugTool::AddBillboard] trying to add a bink billboard with an invalid movie handle");

	if (ms_billboards.GetCount() == MAX_BINK_BILLBOARDS)
	{
		Warningf("[CBinkDebugTool::AddBillboard] Billboard pool exhausted");
		return;
	}

	binkDebugBillboard billboard;
	billboard.vPos			= vPos;
	billboard.vForward		= vForward;
	billboard.vUp			= vUp;
	billboard.movieHandle	= movieHandle;

	ms_billboards.Push(billboard);
}


#endif // __BANK
                       
