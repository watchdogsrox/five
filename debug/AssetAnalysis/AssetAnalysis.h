// ===================================
// debug/AssetAnalysis/AssetAnalysis.h
// (c) 2013 RockstarNorth
// ===================================

#ifndef _DEBUG_ASSETANALYSIS_ASSETANALYSIS_H_
#define _DEBUG_ASSETANALYSIS_ASSETANALYSIS_H_

#if __BANK

namespace rage { class bkBank; }

namespace AssetAnalysis {

void AddWidgets(bkBank& bk);

bool ShouldLoadTxdSlot(int slot);
bool ShouldLoadDwdSlot(int slot);
bool ShouldLoadDrawableSlot(int slot);
bool ShouldLoadFragmentSlot(int slot);
bool ShouldLoadParticleSlot(int slot);
bool ShouldLoadMapDataSlot(int slot);

bool IsWritingStreams_DEPRECATED(); // TODO -- remove these functions once i've unhooked this system from the old CDTVStreamingIterators
void BeginWritingStreams_PRIVATE();
void FinishWritingStreams_PRIVATE();

void ProcessTxdSlot(int slot);
void ProcessDwdSlot(int slot);
void ProcessDrawableSlot(int slot);
void ProcessFragmentSlot(int slot);
void ProcessParticleSlot(int slot);
void ProcessMapDataSlot(int slot);

void Start();
void FinishAndReset();

} // namespace AssetAnalysis

#endif // __BANK
#endif // _DEBUG_ASSETANALYSIS_ASSETANALYSIS_H_
