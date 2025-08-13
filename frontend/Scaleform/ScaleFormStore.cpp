/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformStore.cpp
// PURPOSE : store for a Scaleform object
// AUTHOR  : Derek Payne
// STARTED : 19/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#include "scaleform/tweenstar.h"
#include "system/param.h"
#include "system/threadpool.h"
#include "diag/output.h"		// for DIAG_CONTEXT_MESSAGE

#include "Frontend\Scaleform\ScaleformStore.h"
#include "Frontend\Scaleform\Colour.h"
#include "Frontend\Scaleform\pauseMenuLUT.h"
#include "Frontend\Scaleform\ScaleformMgr.h"
#include "Frontend\Scaleform\ScaleformDef.h"
#include "Network/NetworkInterface.h"

#include "fwscene/stores/txdstore.h" 
#include "fwsys/fileExts.h"
#include "streaming/defragmentation.h"
#include "streaming/streaming.h"

#include "string/string.h"
#include "string/stringutil.h"

#if __XENON
#pragma comment(lib,"scxenon.lib")
#elif RSG_CPU_X86
#pragma comment(lib,"scwin32.lib")
#elif __PS3
#if !__FINAL
#pragma comment(lib,"scps3d")
#else
#pragma comment(lib,"scps3")
#endif
#endif

PARAM(noactionscriptclassoverride, "Don't override any actionscript classes with C++ implementations");

SCALEFORM_OPTIMISATIONS()

CScaleformStore g_ScaleformStore;
#if __BANK
char CScaleformStore::sm_WatchedMovieSubstr[64];
#endif
GFxResource* sm_sfWatchedResource;

extern sysThreadPool s_TexMgrThreadPool;

class CScaleformMovieCreationTask : public sysThreadPool::WorkItem
{
public:
	CScaleformMovieCreationTask(int objIndex, strStreamingLoader::StreamingFile& file) : m_ObjIndex(objIndex), m_File(&file) {}

	virtual void DoWork()
	{
		strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(m_File->m_Index);
		int fileSize = pInfo->ComputeVirtualSize(m_File->m_Index) + pInfo->ComputePhysicalSize(m_File->m_Index);
		g_ScaleformStore.Load(strLocalIndex(m_ObjIndex), m_File->m_LoadingMap.GetVirtualBase(), fileSize);
		m_File->m_IsPlaced = true;
	}

	virtual void OnFinished()
	{
		delete this;
	}

private:
	int m_ObjIndex;
	strStreamingLoader::StreamingFile* m_File;
};

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::CScaleformStore
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
CScaleformStore::CScaleformStore() : fwAssetStore<CScaleformMovieObject, CScaleformDef >("ScaleformStore", PI_SCALEFORM_FILE_ID, CONFIGURED_FROM_FILE, 157, true)
{
	m_usesExtraMemory = true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::Set
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::Set(strLocalIndex iIndex, CScaleformMovieObject* pObject)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObject==NULL, "Movie is not in memory");
	Assertf(pDef->m_pObjectPendingLoad == pObject, "Setting object %p, but objectPendingLoad was %p", pObject, pDef->m_pObjectPendingLoad);

	sfDebugf3("Scaleform Movie %s object set", pDef->m_cFullFilename);

	UPDATE_KNOWN_REF(pDef->m_pObject , pObject);
	pDef->m_pObjectPendingLoad = NULL;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::Remove
// PURPOSE: removes from store
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::Remove(strLocalIndex iIndex)
{
	if (iIndex != -1)
	{
#if !__NO_OUTPUT
		CScaleformDef* pDef = GetSlot(iIndex);

		Assertf(pDef, "No movie type at this slot");
		Assertf(pDef->m_pObject!=NULL, "Movie is not in memory");
		Assertf(pDef->m_pObjectPendingLoad==NULL, "Can't remove a movie that's still loading");
		sfDebugf3("Scaleform Movie %s object is about to be destroyed", pDef->m_cFullFilename);
#endif // #if !__NO_OUTPUT

		fwAssetStore<CScaleformMovieObject, CScaleformDef>::Remove(iIndex);

#if !__NO_OUTPUT
		sfDebugf1("Scaleform Movie %s has been destroyed", pDef->m_cFullFilename);
#endif // #if !__NO_OUTPUT
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetName
// PURPOSE: returns the name of the slot so it can be queried when we set up the
//			texture dictionary dependencies
/////////////////////////////////////////////////////////////////////////////////////
const char *CScaleformStore::GetName(strLocalIndex iIndex) const
{
	const CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");

	return pDef ? pDef->m_name.GetCStr() : "";
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::RequestExtraMemory
// PURPOSE: fills out a datResourceMap with the extra allocations the streaming system should do
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::RequestExtraMemory( strLocalIndex iIndex, datResourceMap& rMap, int /*iMaxAllocs*/ )
{
	CScaleformDef* pDef = GetSlot(iIndex);
	if (NetworkInterface::IsGameInProgress())
	{
		pDef->m_PreallocationInfoMP.GenerateMap(rMap);
	}
	else
	{
		pDef->m_PreallocationInfo.GenerateMap(rMap);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	FreePreallocatedScaleformMemory()
// PURPOSE: Callback for freeing up the preallocated memory we grabbed with StreamingAllocate()
/////////////////////////////////////////////////////////////////////////////////////
void FreePreallocatedScaleformMemory(void* mem)
{
	strStreamingEngine::GetAllocator().StreamingFree(mem);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::ReceiveExtraMemory
// PURPOSE: Once the streaming system allocates the extra memory and streams in the file, but before the
//			file is loaded up, this gets called. This is where we create a new arena for the movie to use.
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::ReceiveExtraMemory( strLocalIndex iIndex, const datResourceMap& rMap )
{
	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObject==NULL, "Movie is already in memory");
	Assertf(pDef->m_pObjectPendingLoad==NULL, "Movie is already loading");

	CScaleformMovieObject* pNewObject = rage_new CScaleformMovieObject;

	atDelegate<void (void*)> freePreallocDel(FreePreallocatedScaleformMemory);

	const char* longName = pDef->m_name.GetCStr();
	const char* shortName = strrchr(longName, ':');
	shortName = shortName ? shortName + 1 : longName;

	u32 peakAllowed = 0;
#if __SF_STATS
	peakAllowed = NetworkInterface::IsGameInProgress() ? pDef->m_PeakAllowedMP : pDef->m_PeakAllowed;
#endif

	size_t granularity = NetworkInterface::IsGameInProgress() ? pDef->m_GranularityKb_MP : pDef->m_GranularityKb;
	granularity *= 1024;

	sysMemAllocator* overallocator = pDef->m_bOverallocInGameHeap ? sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL) : &strStreamingEngine::GetAllocator();

	pNewObject->m_MemoryArena = CScaleformMgr::GetMovieMgr()->CreatePreallocatedMemoryArena(rMap, granularity, peakAllowed, overallocator, freePreallocDel, shortName);
	pDef->m_pObjectPendingLoad = pNewObject;
	pDef->m_pObject = NULL;
}

size_t CScaleformStore::GetExtraVirtualMemory(strLocalIndex index) const
{
	const CScaleformDef* pDef = GetSlot(index);

	// TODO: Should we assert if there is no movie here?
	if (!pDef)
	{
		return 0;
	}

	const CScaleformMovieObject *pObj = pDef->m_pObject;
	if (!pObj)
	{
		pObj = pDef->m_pObjectPendingLoad;
	}

	if (!pObj)
	{
		return 0;
	}

	int arena = pObj->m_MemoryArena;
	const sfPreallocatedMemoryWrapper* allocator = CScaleformMgr::GetMovieMgr()->GetAllocatorForArena((u32) arena);
	
	if (!allocator)
	{
		return 0;
	}

	size_t result = 0;

	for (int x=0; x<allocator->m_PreallocatedChunks.GetCount(); x++)
	{
		result += allocator->m_PreallocatedChunks[x].m_Size;
	}

  	for (int y=0; y<allocator->m_ExtraChunks.GetCount(); y++)
  	{
  		result += allocator->m_ExtraChunks[y].m_Size;
  	}

	return result;	
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::PrintExtraInfo
// PURPOSE: Print extra info about these movies.
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::PrintExtraInfo(strLocalIndex BANK_ONLY(index), char* BANK_ONLY(extraInfo), size_t BANK_ONLY(maxSize)) const
{
#if __BANK
	const CScaleformDef* pDef = GetSlot(index);

	datResourceMap map;
	bool isMP = NetworkInterface::IsGameInProgress();
	bool isMPDifferent = false;

	if (isMP)
	{
		pDef->m_PreallocationInfoMP.GenerateMap(map);
		isMPDifferent = pDef->HasSeperateMultiplayerPrealloc();
	}
	else
	{
		pDef->m_PreallocationInfo.GenerateMap(map);
	}

	size_t virtualSize = 0;
	for (int i = 0; i != map.VirtualCount; ++i)
	{
		virtualSize += map.Chunks[i].Size;
	}

	size_t physicalSize = 0;
	for (int i = map.VirtualCount; i != map.VirtualCount
#if !FREE_PHYSICAL_RESOURCES
		+ map.PhysicalCount
#endif // !FREE_PHYSICAL_RESOURCES
		; ++i)
	{
		physicalSize += map.Chunks[i].Size;
	}

	snprintf(extraInfo, maxSize, "extra memory %" SIZETFMT "dKb/%" SIZETFMT "dKb", virtualSize >> 10, physicalSize >> 10);

	const CScaleformMovieObject *pObj = pDef->m_pObject;

	if (pObj)
	{
		int arena = pObj->m_MemoryArena;
		const sfPreallocatedMemoryWrapper* allocator = CScaleformMgr::GetMovieMgr()->GetAllocatorForArena((u32) arena);

		if (allocator)
		{
			size_t result = 0;

			for (int y=0; y<allocator->m_ExtraChunks.GetCount(); y++)
			{
				result += allocator->m_ExtraChunks[y].m_Size;
			}

			if (isMPDifferent)
			{
				snprintf(extraInfo, maxSize, "prealloc(mp) %" SIZETFMT "dKb/%" SIZETFMT "dKb extra %" SIZETFMT "dKb", virtualSize >> 10, physicalSize >> 10, result >> 10);
			}
			else
			{
				snprintf(extraInfo, maxSize, "prealloc     %" SIZETFMT "dKb/%" SIZETFMT "dKb extra %" SIZETFMT "dKb", virtualSize >> 10, physicalSize >> 10, result >> 10);
			}
		}
	}
#endif // __BANK
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::PlaceAsynchronously
// PURPOSE: Starts a request to load this movie and create a movie view via a worker thread
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::PlaceAsynchronously(strLocalIndex index, strStreamingLoader::StreamingFile& file, datResourceInfo& UNUSED_PARAM(rsc))
{
	strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(file.m_Index);
	streamAssertf(pInfo->GetStatus() == STRINFO_LOADING, "Info status is %d", pInfo->GetStatus());
	pInfo->SetFlag(file.m_Index, STRFLAG_INTERNAL_PLACING);

	CScaleformDef* pDef = GetSlot(index);
	if (pDef->m_iTextureSlot != -1)
	{
		g_TxdStore.AddRef(pDef->m_iTextureSlot, REF_OTHER);
#if USE_DEFRAGMENTATION
		strStreamingEngine::GetDefragmentation()->Lock(g_TxdStore.Get(pDef->m_iTextureSlot), g_TxdStore.GetStreamingIndex(pDef->m_iTextureSlot));
#endif
	}

	CScaleformMovieCreationTask* workItem = rage_new CScaleformMovieCreationTask(index.Get(), file);

	s_TexMgrThreadPool.QueueWork(workItem);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::SetResource
// PURPOSE: called when async placement is complete (even though this isn't a resource)
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::SetResource(strLocalIndex index, datResourceMap& /*map*/)
{
	CScaleformDef* pDef = GetSlot(index);
	if (pDef->m_iTextureSlot != -1)
	{
#if USE_DEFRAGMENTATION
		strStreamingEngine::GetDefragmentation()->Unlock(g_TxdStore.Get(pDef->m_iTextureSlot), g_TxdStore.GetStreamingIndex(pDef->m_iTextureSlot));
#endif
		g_TxdStore.RemoveRef(pDef->m_iTextureSlot, REF_OTHER);
	}

	Set(index, pDef->m_pObjectPendingLoad);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::Load
// PURPOSE: calls to Loads the movie from stream 
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformStore::Load(strLocalIndex iIndex, void* pData, s32 iSize)
{
	Assert(iIndex.Get() >= 0);

	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObjectPendingLoad, "Movie is not pending load"); // Expect ReceiveExtraMemory to allocate the m_pObjectPendingLoad
	Assertf(!pDef->m_pObject, "Movie has been loaded?"); // Expect no m_pObject

	char cFilename[RAGE_MAX_PATH];
	char cRefName[RAGE_MAX_PATH];

	formatf(cRefName, "%05d_%s", iIndex, GetName(iIndex));

	DIAG_CONTEXT_MESSAGE("Streaming load of Flash Movie %s", cRefName);
	sfScaleformManager::AutoSetCurrMovieName currMovie(cRefName);

	fiDeviceMemory::MakeMemoryFileName(cFilename, RAGE_MAX_PATH, pData, iSize, false, cRefName);

	bool success = LoadFileCore(iIndex, cFilename);

	strStreamingInfo* pInfo = GetStreamingInfo(iIndex);
	if (!pInfo->IsFlagSet(STRFLAG_INTERNAL_PLACING)) // If this flag is clear we are NOT placing on another thread, which means SetResource won't get called.
	{
		Set(iIndex, pDef->m_pObjectPendingLoad);
	}

	return success;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::Load
// PURPOSE: Does a blocking load of the file
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformStore::LoadFile(strLocalIndex iIndex, const char* pFilename)
{
	Assert(iIndex.Get() >= 0);

	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObject==NULL, "Movie is already in memory");
	Assertf(pDef->m_pObjectPendingLoad==NULL, "Movie is already loading");

	DIAG_CONTEXT_MESSAGE("Blocking load of Flash Movie %s", pFilename);
	sfScaleformManager::AutoSetCurrMovieName currMovie(pFilename);

	CScaleformMovieObject* pNewObject = rage_new CScaleformMovieObject;
	pDef->m_pObjectPendingLoad = pNewObject;

	bool success = LoadFileCore(iIndex, pFilename);

	Set(iIndex, pDef->m_pObjectPendingLoad);

	return success;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::Load
// PURPOSE: The main file loading implementation - same for streaming and non-streaming cases
/////////////////////////////////////////////////////////////////////////////////////
sysCriticalSectionToken g_ScaleformLoaderLock;

bool CScaleformStore::LoadFileCore(strLocalIndex iIndex, const char* pFilename)
{
	SYS_CS_SYNC(g_ScaleformLoaderLock); // Not sure this could really get called by two threads at once, but just in case.

	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(!pDef->m_pObject, "Movie is in memory"); // Expect ReceiveExtraMemory to allocate the m_pObject
	Assertf(pDef->m_pObjectPendingLoad, "Object isn't loading");

#if !__FINAL
	if (strlen(pDef->m_cFullFilename) >= SCALEFORM_MAX_PATH)
		Quitf("Scaleform path: %s is too long (%d). Decrease to < %d characters!", pDef->m_cFullFilename, (int)strlen(pDef->m_cFullFilename), SCALEFORM_MAX_PATH);
#endif		
	
	safecpy(pDef->m_cFullFilename, pFilename);
	safecat(pDef->m_cFullFilename, ".");
	safecat(pDef->m_cFullFilename, PI_SCALEFORM_FILE_EXT);  // apply the gfx to it

	CScaleformMovieObject* pObject = pDef->m_pObjectPendingLoad;

	GPtr<GFxFontLib> oldFontLib;

	if (pDef->m_iAdditionalFonts >= 0)
	{
		// Create a new fontlib for the loader
		oldFontLib = CScaleformMgr::GetMovieMgr()->GetLoader()->GetFontLib();
		SetNewFontLibrary(pDef->m_iAdditionalFonts);
	}

	if (pDef->m_iTextureSlot != -1)
	{
		CScaleformMgr::sm_CurrTextureDictName = &pDef->m_cFullTexDictName[0];
		pObject->m_Movie = CScaleformMgr::GetMovieMgr()->LoadMovie(pDef->m_cFullFilename, true, g_TxdStore.Get(pDef->m_iTextureSlot), pObject->m_MemoryArena);
		CScaleformMgr::sm_CurrTextureDictName = NULL;
	}
	else
	{
		CScaleformMgr::sm_CurrTextureDictName = NULL;
		pObject->m_Movie = CScaleformMgr::GetMovieMgr()->LoadMovie(pDef->m_cFullFilename, false, NULL, pObject->m_MemoryArena);
	}
	
#if __BANK
	if (pObject->m_Movie && sm_WatchedMovieSubstr[0] != '\0' && stristr(pFilename, sm_WatchedMovieSubstr))
	{
		sfErrorf("Watching reference changes for %s", pFilename);
		CScaleformMgr::ms_watchedResource = &pObject->m_Movie->GetMovie();
	}
#endif

	if (pObject->m_Movie)
	{
		// Explicitly set the fontlib on the movie (why do I have to do this - the fontlib is supposedly a "binding state"
		pObject->m_Movie->GetMovie().SetState(GFxState::State_FontLib, CScaleformMgr::GetMovieMgr()->GetLoader()->GetFontLib());

		CreateMovieView(iIndex);  // try to create a movie view here
	}
	else
	{
		sfAssertf(0, "Could not load scaleform movie %s (index %d). Send a bug to Russ Schaaf. Include any TTY output", pFilename, iIndex.Get());
	}

	if (oldFontLib)
	{
		CScaleformMgr::GetMovieMgr()->GetLoader()->SetFontLib(oldFontLib);
	}

	return (pObject->m_Movie != NULL);
}

void CScaleformStore::AddFontsToLib(GFxFontLib* lib, int fontMovieId)
{
	CScaleformMovieObject* pObj = Get(strLocalIndex(fontMovieId));
	if (sfVerifyf(pObj, "Couldn't load fonts from %s, the font movie isn't loaded yet", GetName(strLocalIndex(fontMovieId))))
	{
		sfScaleformMovie* movie = pObj->GetRawMovie();
		if (movie)
		{
			lib->AddFontsFrom(&movie->GetMovie(), false);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetNumRefs
// PURPOSE: returns the ref count on a movie, artifically increasing it during memory operations
//          so we don't try to free one movie while creating another (causes internal SF problems)
/////////////////////////////////////////////////////////////////////////////////////
int CScaleformStore::GetNumRefs(strLocalIndex index) const
{
	int baseRefs = fwAssetStore<CScaleformMovieObject, CScaleformDef>::GetNumRefs(index);

	return baseRefs + (sfScaleformManager::IsPerformingMemoryOperation() ? 1 : 0);
}

void CScaleformStore::AddRef(strLocalIndex index, strRefKind refKind)
{
	fwAssetStore<CScaleformMovieObject, CScaleformDef>::AddRef(index, refKind);

#if !__NO_OUTPUT
	CScaleformDef* pDef = GetSlot(index);
	sfDebugf1("Scaleform AddRef %s -> %d", pDef->m_cFullFilename, fwAssetStore<CScaleformMovieObject, CScaleformDef>::GetNumRefs(index));
#endif
}

// static bool breakHere = true;

void CScaleformStore::RemoveRef(strLocalIndex index, strRefKind refKind)
{
	fwAssetStore<CScaleformMovieObject, CScaleformDef>::RemoveRef(index, refKind);

#if !__NO_OUTPUT || __ASSERT
	int numRefs = fwAssetStore<CScaleformMovieObject, CScaleformDef>::GetNumRefs(index);
	CScaleformDef* pDef = GetSlot(index);
	sfDebugf1("Scaleform RemoveRef %s -> %d", pDef->m_cFullFilename, numRefs);
	sfAssertf(numRefs >= 0, "Invalid scaleform move (%s) reference count %d. What happened?", pDef->m_cFullFilename, numRefs);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::CreateMovieView
// PURPOSE: creates the movie view
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::CreateMovieView(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObject || pDef->m_pObjectPendingLoad, "Movie is not in memory for %s", pDef->m_name.GetCStr());

	CScaleformMovieObject* pObject = pDef->m_pObject;
	if (!pObject)
	{
		pObject = pDef->m_pObjectPendingLoad;
	}
	Assertf(pObject->m_Movie, "Missing the actual movie object for %s", pDef->m_name.GetCStr());

	if (pDef->m_bMovieView)
	{


		if (!pObject->m_MovieView)
		{
			pObject->m_MovieView = CScaleformMgr::GetMovieMgr()->CreateMovieView(*pObject->m_Movie, pObject->m_MemoryArena);

			if (pObject->m_MovieView)
			{
				// Overriding the scale to compute from 1280x720 (see B#694157 for details)
				pObject->m_MovieView->RecomputeScaleToUnitLengthVector((float)ACTIONSCRIPT_STAGE_SIZE_X, (float)ACTIONSCRIPT_STAGE_SIZE_Y);

#if RSG_PC
				// On PC, only allow for support mouse input on scaleform movies.  All other input can be handled by code
				pObject->m_MovieView->SetFlag(sfScaleformMovieView::FLAG_MOUSE_INPUT, true);
#endif

				if (!PARAM_noactionscriptclassoverride.Get())
				{
					// @TODO consider replacing this with a fancy registration system, especially if attaching one is expensive
					sfScaleformMovieView& movie = *pObject->m_MovieView;
					sfInstallTweenstar(movie);
					CScaleformInstallPauseMenuLUT(movie);
					CScaleformInstallColour(movie);

				}
			}

			sfDebugf1("Scaleform Movieview created for %s", pDef->m_cFullFilename);
		}
	}
	else
	{
		pObject->m_MovieView = NULL;

		sfDebugf1("Scaleform Movieview not created for %s", pDef->m_cFullFilename);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::RemoveMovieView
// PURPOSE: removes the movie view
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformStore::RemoveMovieView(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	DIAG_CONTEXT_MESSAGE("Removing movie view %s", pDef->m_cFullFilename);
	sfScaleformManager::AutoSetCurrMovieName currMovie(pDef->m_cFullFilename);

	Assertf(pDef, "No movie type at this slot");
	Assertf(pDef->m_pObject, "Movie is not in memory");

	if (pDef->m_pObject->m_MovieView)
	{
		CScaleformMgr::GetMovieMgr()->DeleteMovieView(pDef->m_pObject->m_MovieView);
		pDef->m_pObject->m_MovieView = NULL;

		sfDebugf1("Scaleform Movieview removed for %s", pDef->m_cFullFilename);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetDependencies()
// PURPOSE: returns any dependencies this movie has
/////////////////////////////////////////////////////////////////////////////////////
int CScaleformStore::GetDependencies(strLocalIndex iIndex, strIndex *pIndices, int indexArraySize) const
{
	s32 iCount = 0;
	strLocalIndex iParentTxd = CScaleformStore::GetParentTxdForSlot(iIndex);
	if(iParentTxd != -1)
	{
		AddDependencyOutput(pIndices, iCount, g_TxdStore.GetStreamingModule()->GetStreamingIndex(iParentTxd), indexArraySize, GetStreamingIndex(iIndex));
	}

	const CScaleformDef* pDef = GetSlot(iIndex);
	if (pDef && pDef->m_iAdditionalFonts >= 0)
	{
		AddDependencyOutput(pIndices, iCount, g_ScaleformStore.GetStreamingModule()->GetStreamingIndex(strLocalIndex(pDef->m_iAdditionalFonts)), indexArraySize, GetStreamingIndex(iIndex));
	}

	for (s32 i = 0; i < MAX_ASSET_MOVIES; i++)
	{
		s32 iMovieAsset = CScaleformStore::GetMovieAssetForSlot(iIndex, i);
		if(iMovieAsset != -1)
		{
			AddDependencyOutput(pIndices, iCount, g_ScaleformStore.GetStreamingModule()->GetStreamingIndex(strLocalIndex(iMovieAsset)), indexArraySize, GetStreamingIndex(iIndex));
		}
	}

	return iCount;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetFilenameUsedForLoad()
// PURPOSE: gets the memory filename of the movie
/////////////////////////////////////////////////////////////////////////////////////
char *CScaleformStore::GetFilenameUsedForLoad(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");

	return pDef->m_cFullFilename;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetRawMovie()
// PURPOSE: Gets the actual "movie" itself
/////////////////////////////////////////////////////////////////////////////////////
sfScaleformMovie *CScaleformStore::GetRawMovie(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");
	sfAssertf(pDef->m_pObject!=NULL, "Movie not in memory");

	return pDef->m_pObject->GetRawMovie();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetNumberOfRefs()
// PURPOSE: returns number of refs
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformStore::GetNumberOfRefs(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");
	sfAssertf(pDef->m_pObject!=NULL, "Movie not in memory");

	return (pDef->m_pObject->GetRawMovieRefs());
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetRawMovieView()
// PURPOSE: Gets the actual "movie" we can draw/update
/////////////////////////////////////////////////////////////////////////////////////
sfScaleformMovieView *CScaleformStore::GetRawMovieView(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");
	sfAssertf(pDef->m_pObject!=NULL, "Movie not in memory");

	return pDef->m_pObject->GetRawMovieView();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformStore::GetMovieView()
// PURPOSE: Gets a direct path to the movie view we can adjust/configure
/////////////////////////////////////////////////////////////////////////////////////
GFxMovieView *CScaleformStore::GetMovieView(strLocalIndex iIndex)
{
	CScaleformDef* pDef = GetSlot(iIndex);

	sfAssertf(pDef, "ScaleformStore: No movie at this slot");

	if (pDef->m_pObject)
		return pDef->m_pObject->GetMovieView();
	else
		return NULL;
}

void CScaleformStore::SetNewFontLibrary( int additionalFonts )
{
	GPtr<GFxFontLib> newFontLib = *(new GFxFontLib);

	CScaleformMgr::AddGlobalFontsToLib(newFontLib);
	AddFontsToLib(newFontLib, additionalFonts);

	CScaleformMgr::GetMovieMgr()->GetLoader()->SetFontLib(newFontLib);

#if !__NO_OUTPUT
	GStringHash<GString> fontNames;
	newFontLib->LoadFontNames(fontNames);
	int i = 0;
	for(GStringHash<GString>::Iterator iter = fontNames.Begin(); iter != fontNames.End(); ++iter)
	{
		sfDebugf2("Font %d: %s, %s", i, (*iter).First.ToCStr(), (*iter).Second.ToCStr());
		i++;
	}
#endif
}

// eof
