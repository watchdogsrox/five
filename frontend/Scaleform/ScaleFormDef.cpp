/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformDef.cpp
// PURPOSE : definition of one Scaleform object
// AUTHOR  : Derek Payne
// STARTED : 19/10/2009
//
////////////////////////////////////////////`/////////////////////////////////////

// Rage headers
#include "diag/output.h"		// for DIAG_CONTEXT_MESSAGE
#include "fwscene/stores/txdstore.h"

// Game headers
#include "Frontend/Scaleform/ScaleFormDef.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Streaming/streaming.h"

SCALEFORM_OPTIMISATIONS()



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMovieObject::CScaleformMovieObject()
// PURPOSE: constructor
/////////////////////////////////////////////////////////////////////////////////////
CScaleformMovieObject::CScaleformMovieObject()
{
	m_Movie = NULL;
	m_MovieView = NULL;
	m_MemoryArena = 0;
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMovieObject::~CScaleformMovieObject()
// PURPOSE: destructor
/////////////////////////////////////////////////////////////////////////////////////
CScaleformMovieObject::~CScaleformMovieObject()
{
	sfAssertf(CScaleformMgr::GetMovieMgr(), "CScaleformDef: Movie manager invalid");

#if !__NO_OUTPUT
	char cDebugFilename[RAGE_MAX_PATH];
	if ( (m_Movie) && (&m_Movie->GetMovie()) )
		safecpy(cDebugFilename, m_Movie->GetMovie().GetFileURL(), RAGE_MAX_PATH);
	else
		safecpy(cDebugFilename, "(m_Movie is NULL)", RAGE_MAX_PATH);

	sfDebugf3("Scaleform movie object %s is about to be removed", cDebugFilename);

	DIAG_CONTEXT_MESSAGE("Deleting movie %s", cDebugFilename);
	sfScaleformManager::AutoSetCurrMovieName currMovie(cDebugFilename);
#endif  // __ASSERT

	CScaleformMgr::GetMovieMgr()->DeleteMovie(m_Movie);
	m_Movie = NULL;

#if !__NO_OUTPUT
	sfDebugf3("Scaleform movie object %s has been removed", cDebugFilename);
#endif  // __ASSERT

	if (m_MemoryArena)
	{
		CScaleformMgr::GetMovieMgr()->DestroyPreallocatedMemoryArena(m_MemoryArena);
#if __ASSERT
		sfDebugf3("Scaleform memory arena for %s has been destroyed", cDebugFilename);
#endif  // __ASSERT
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMovieObject::GetMovieView()
// PURPOSE: returns a GFxMovieView if one exists for the current movie
/////////////////////////////////////////////////////////////////////////////////////
GFxMovieView *CScaleformMovieObject::GetMovieView()
{
	sfScaleformMovieView *pRawMovieView = GetRawMovieView();
	if (pRawMovieView)
		return &pRawMovieView->GetMovieView();

	return NULL;
}

void CScaleformDef::Init( const strStreamingObjectNameString name )
{
	fwAssetNameDef<CScaleformMovieObject>::Init(name);
	m_iTextureSlot = -1;
	m_cFullFilename[0] = '\0';
	m_cFullTexDictName[0] = '\0';
	m_iAdditionalFonts = -1;
	m_bMovieView = false;
	m_pObjectPendingLoad = NULL;

	for (s32 i = 0; i < MAX_ASSET_MOVIES; i++)
	{
		m_iMovieAssetSlot[i] = -1;
	}

	ResetPrealloc();
}

bool CScaleformDef::HasSeperateMultiplayerPrealloc() const
{
	return memcmp(&m_PreallocationInfo, &m_PreallocationInfoMP, sizeof(m_PreallocationInfo)) != 0;
}

void CScaleformDef::ResetPrealloc()
{
	memset(&m_PreallocationInfo, 0, sizeof(m_PreallocationInfo));
	memset(&m_PreallocationInfoMP, 0, sizeof(m_PreallocationInfoMP));

	m_GranularityKb = m_GranularityKb_MP = 16;
#if __SF_STATS
	m_PeakAllowed = m_PeakAllowedMP = 0;
#endif
}

// eof
