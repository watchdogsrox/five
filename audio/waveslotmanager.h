//  
// audio/waveslotmanager.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#ifndef AUD_WAVESLOTMANAGER_H
#define AUD_WAVESLOTMANAGER_H

#include "atl/array.h"
#include "audiohardware/waveslot.h"
#include "audiodefines.h"

static const u32 AUD_WAVE_SLOT_ANY = 0xffffffff;

namespace rage 
{
	class audWaveSlot;
	class audEntity;
}

struct audDynamicWaveSlot
{
	audDynamicWaveSlot() :
		waveSlot(NULL)
	  , slotName(0)
	  , slotPriority(0)
	  , loadedBankId(AUD_INVALID_BANK_ID)
	  , loadedBankType(0)
	  , refCount(0)
	  , timeLastUsed(0)
	  , delayTime(0.0f)
	  , loadPriority(naWaveLoadPriority::General)
	{}

	bool IsLoaded() const;
	bool IsLoading() const;

	atArray<u32> 					slotTypes;
	atArray<audEntity*>				registeredEntities;
	audWaveSlot* 					waveSlot;
	u32		     					slotName;
	u32			 					slotPriority;
	u32			 					loadedBankId;
	u32			 					loadedBankType;
	u32			 					refCount;
	u32		     					timeLastUsed;
	f32		     					delayTime;
	naWaveLoadPriority::Priority	loadPriority;
};

// ----------------------------------------------------------------
// Helper class to assist managing dynamically loaded banks
// ----------------------------------------------------------------
class audWaveSlotManager
{
public:
	audWaveSlotManager();
	~audWaveSlotManager() {};

	u32 GetNumSlots() const;
	u32 GetNumUsedSlots() const;
	u32 GetNumLoadsInProgress() const;

	bool AddWaveSlot(const char* slotName, const char* slotType, const u32 slotPriority = 0);
	bool AddWaveSlot(const u32 slotNameHash, const u32 slotTypeHash, const u32 slotPriority = 0);

	static void FreeWaveSlot(audDynamicWaveSlot*& slot, audEntity* entity = NULL);
	void Update();

#if __BANK
	void DebugDraw(f32 xOffset, f32 yOffset);
#endif

	const audDynamicWaveSlot* GetWaveSlot(u32 index) const { return &m_dynamicWaveSlots[index]; }
	audDynamicWaveSlot* LoadBank(const u32 bankId, const u32 slotType, bool allowBankShuffling = false, audEntity* entity = NULL, naWaveLoadPriority::Priority loadPriority = naWaveLoadPriority::General, f32 delayTime = 0.0f);
	audDynamicWaveSlot* LoadBankForSound(const u32 soundNameHash, const u32 slotType, bool allowBankShuffling = false, audEntity* entity = NULL, naWaveLoadPriority::Priority loadPriority = naWaveLoadPriority::General, f32 delayTime = 0.0f);

private:
	void RegisterEntity(audEntity* entity, u32 slotIndex);

private:
	atArray<audDynamicWaveSlot> m_dynamicWaveSlots;
};

#endif // AUD_WAVESLOTMANAGER_H
