/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformDef.h
// PURPOSE : definition of a Scaleform object
// AUTHOR  : Derek Payne
// STARTED : 19/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCALEFORMDEF_H_
#define _SCALEFORMDEF_H_

// Rage headers
#include "fwsys/timer.h"
#include "Scaleform/scaleform.h"

// Game headers
#include "Frontend/Scaleform/ScaleFormMgr.h"

#define MAX_ASSET_MOVIES (5)

//
// CScaleformMovieObject - one class that holds all the info for one Scaleform movie
//
class CScaleformMovieObject : public pgBase, CScaleformMgr
{
public:
	CScaleformMovieObject();
	~CScaleformMovieObject();

	GFxMovieView *GetMovieView();

	sfScaleformMovieView *GetRawMovieView() { return m_MovieView; }
	sfScaleformMovie *GetRawMovie() { return m_Movie; }
	s32 GetRawMovieRefs() { return m_Movie->GetMovie().GetRefCount(); }

	sfScaleformMovie*		m_Movie;
	sfScaleformMovieView*	m_MovieView;
	int						m_MemoryArena;
};



//
// CScaleformDef
//
class CScaleformDef : public fwAssetNameDef<CScaleformMovieObject>, CScaleformMgr
{
public:
	void Init(const strStreamingObjectNameString name);

	strLocalIndex m_iTextureSlot;
	s32 m_iMovieAssetSlot[MAX_ASSET_MOVIES];
	CScaleformMovieObject* m_pObjectPendingLoad;
	s16 m_iAdditionalFonts;
	bool m_bMovieView;
	bool m_bOverallocInGameHeap; // Normally we use the resource heap for overallocs

	bool HasSeperateMultiplayerPrealloc() const;

	void ResetPrealloc();

	datResourceInfo m_PreallocationInfo;
	datResourceInfo m_PreallocationInfoMP;
	u16 m_GranularityKb, m_GranularityKb_MP;
#if __SF_STATS
	u32 m_PeakAllowed;		// these could probably be shifted u16s if we need them to be. Each unit could represent 16k of mem
	u32 m_PeakAllowedMP;
#endif
	// EJ: Don't fuck with this.  This optimization saves 110K
	char m_cFullFilename[SCALEFORM_MAX_PATH];
	char m_cFullTexDictName[SCALEFORM_DICT_MAX_PATH];
};



#endif // _SCALEFORMDEF_H_

// eof
