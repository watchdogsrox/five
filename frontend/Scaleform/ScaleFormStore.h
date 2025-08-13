/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformStore.h
// PURPOSE : store for a Scaleform object
// AUTHOR  : Derek Payne
// STARTED : 19/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCALEFORMSTORE_H_
#define _SCALEFORMSTORE_H_

// Rage headers
#include "fwtl\assetstore.h"

// Game headers
#include "Frontend\Scaleform\ScaleformDef.h"
#include "Frontend\Scaleform\ScaleFormMgr.h"



//
// store for scaleform movies:
//
class CScaleformStore : public fwAssetStore<CScaleformMovieObject, CScaleformDef>, CScaleformMgr
{
public:
	typedef fwAssetStore<CScaleformMovieObject, CScaleformDef> BaseType;

	CScaleformStore();

	virtual void Set(strLocalIndex iIndex, CScaleformMovieObject* m_pObject);
	virtual void Remove(strLocalIndex iIndex);

	virtual bool Load(strLocalIndex index, void* pData, s32 size);

	bool LoadFile(strLocalIndex iIndex, const char* pFilename);

	virtual void RequestExtraMemory(strLocalIndex iIndex, datResourceMap& rMap, int iMaxAllocs);
	virtual void ReceiveExtraMemory(strLocalIndex iIndex, const datResourceMap& rMap);
	virtual size_t GetExtraVirtualMemory(strLocalIndex index) const;

	virtual bool CanPlaceAsynchronously(strLocalIndex UNUSED_PARAM(objIndex)) const { return false; }
	virtual void PlaceAsynchronously(strLocalIndex objIndex, strStreamingLoader::StreamingFile& file, datResourceInfo& rsc);
	virtual void SetResource(strLocalIndex index, datResourceMap& map); // Called when the async placement is complete

	virtual void PrintExtraInfo(strLocalIndex index, char* extraInfo, size_t maxSize) const;

	virtual int GetNumRefs(strLocalIndex index) const;

	virtual void AddRef(strLocalIndex index, strRefKind refKind);
	virtual void RemoveRef(strLocalIndex index, strRefKind refKind);

	virtual const char *GetName(strLocalIndex iIndex) const;
	virtual int GetDependencies(strLocalIndex iIndex, strIndex *pIndices, int indexArraySize) const;

	void SetParentTxdForSlot(strLocalIndex iIndex, strLocalIndex iParent) { Assign(GetSlot(iIndex)->m_iTextureSlot, iParent); }
	strLocalIndex GetParentTxdForSlot(strLocalIndex iIndex) const { return strLocalIndex(GetSlot(iIndex)->m_iTextureSlot); }

	void SetMovieAssetForSlot(strLocalIndex iIndex, s32 iAssetIndex, s32 iNum) { Assign(GetSlot(iIndex)->m_iMovieAssetSlot[iNum], iAssetIndex); }
	s32 GetMovieAssetForSlot(strLocalIndex iIndex, s32 iNum) const { return GetSlot(iIndex)->m_iMovieAssetSlot[iNum]; }

	void SetMovieAsRequiringMovieView(strLocalIndex iIndex, bool bNeedsMovieView) { Assign(GetSlot(iIndex)->m_bMovieView, bNeedsMovieView); }

	char *GetFilenameUsedForLoad(strLocalIndex iIndex);
	sfScaleformMovie *GetRawMovie(strLocalIndex iIndex);
	sfScaleformMovieView *GetRawMovieView(strLocalIndex iIndex);
	s32 GetNumberOfRefs(strLocalIndex iIndex);
	GFxMovieView *GetMovieView(strLocalIndex iIndex);

	void CreateMovieView(strLocalIndex iIndex);
	void RemoveMovieView(strLocalIndex iIndex);

	void AddFontsToLib(GFxFontLib* lib, int fontMovieId);

#if __BANK
	static char sm_WatchedMovieSubstr[64];
#endif

protected:
	bool LoadFileCore(strLocalIndex iIndex, const char* pFilename);
	void SetNewFontLibrary( int additionalFonts );
};

extern CScaleformStore g_ScaleformStore;



#endif // _SCALEFORMSTORE_H_

// eof
