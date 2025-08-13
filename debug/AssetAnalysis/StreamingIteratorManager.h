// ==============================================
// debug/AssetAnalysis/StreamingIteratorManager.h
// (c) 2013 RockstarNorth
// ==============================================

#ifndef _DEBUG_ASSETANALYSIS_STREAMINGITERATORMANAGER_H_
#define _DEBUG_ASSETANALYSIS_STREAMINGITERATORMANAGER_H_

#if __BANK

#include "atl/array.h"

namespace rage { class bkBank; }

class CStreamingIteratorSlot;

class CStreamingIteratorManager
{
public:
	static void AddWidgets(bkBank& bk);
	static void Update();
	static void FinishAndReset();
	static void Logf(const char* format, ...);

	static const atArray<CStreamingIteratorSlot>* GetFailedSlots(int assetType);
};

#endif // __BANK
#endif // _DEBUG_ASSETANALYSIS_STREAMINGITERATORMANAGER_H_
