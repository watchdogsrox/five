// ============================================
// debug/textureviewer/textureviewerdebugging.h
// (c) 2010 RockstarNorth
// ============================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDEBUGGING_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDEBUGGING_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

// This is a lame attempt to track when asset store names change .. the conclusion is that yes,
// they do sometimes change, but not very often and not very many of them and (?) usually they
// are not in the streaming image. If this needs to be revisited then it would be better to
// actually track the hash of each asset name separately instead of tracking a hash of everything,
// so that we can figure out which assets are changing and which systems are doing it.
class CAssetStoreHashCheck
{
public:
	CAssetStoreHashCheck();
	void Reset();
	template <typename AssetStoreType> void Check(AssetStoreType& store, const char* name);
	static void Update(bool bTrack);

	int m_count;
	u32 m_hash;
};

#endif // DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

// ================================================================================================

void PrintTextureDictionaries();
void PrintTextureDictionarySizes();
void PrintModelInfoStuff();

void PrintTxdChildren(int parentTxdIndex);

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERDEBUGGING_H_
