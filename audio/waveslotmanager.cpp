//  
// audio/waveslotmanager.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "waveslotmanager.h"
#include "audio_channel.h"

#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "audiohardware/waveslot.h"

#include "audioengine/engine.h"
#include "audioengine/soundfactory.h"

AUDIO_OPTIMISATIONS()

// ----------------------------------------------------------------
// Check if the bank has finished loading
// ----------------------------------------------------------------
bool audDynamicWaveSlot::IsLoaded() const
{
	if(waveSlot)
	{
		audWaveSlot::audWaveSlotLoadStatus status = waveSlot->GetBankLoadingStatus(loadedBankId);

		if(status == audWaveSlot::LOADED)
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Check if the bank is loading
// ----------------------------------------------------------------
bool audDynamicWaveSlot::IsLoading() const
{
	if(waveSlot)
	{
		audWaveSlot::audWaveSlotLoadStatus status = waveSlot->GetBankLoadingStatus(loadedBankId);

		if(status == audWaveSlot::LOADING)
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// audWaveSlotManager constructor
// ----------------------------------------------------------------
audWaveSlotManager::audWaveSlotManager()
{
}

// ----------------------------------------------------------------
// Add a new wave slot
// ----------------------------------------------------------------
bool audWaveSlotManager::AddWaveSlot(const char* slotName, const char* slotType, const u32 slotPriority)
{
	u32 slotNameHash = atStringHash(slotName);
	u32 slotTypeHash = atStringHash(slotType);
	return AddWaveSlot(slotNameHash, slotTypeHash, slotPriority);
}

// ----------------------------------------------------------------
// Add a new wave slot
// ----------------------------------------------------------------
bool audWaveSlotManager::AddWaveSlot(const u32 slotNameHash, const u32 slotTypeHash, const u32 slotPriority)
{
	for(int slotLoop = 0; slotLoop < m_dynamicWaveSlots.GetCount(); slotLoop++)
	{
		if(m_dynamicWaveSlots[slotLoop].slotName == slotNameHash)
		{
			for(int typeLoop = 0; typeLoop < m_dynamicWaveSlots[slotLoop].slotTypes.GetCount(); typeLoop++)
			{
				// This slot/type combo is already registered!
				if(m_dynamicWaveSlots[slotLoop].slotTypes[typeLoop] == slotTypeHash)
				{
					return true;
				}
			}

			// Slot exists, but not type - so add it now
			m_dynamicWaveSlots[slotLoop].slotTypes.PushAndGrow(slotTypeHash);
			return true;
		}
	}

	audWaveSlot* waveSlot = audWaveSlot::FindWaveSlot(slotNameHash);

	if(waveSlot)
	{
		audDynamicWaveSlot& dynamicWaveSlot = m_dynamicWaveSlots.Grow();
		dynamicWaveSlot.slotName = slotNameHash;
		dynamicWaveSlot.slotPriority = slotPriority;
		dynamicWaveSlot.slotTypes.PushAndGrow(slotTypeHash);
		dynamicWaveSlot.waveSlot = waveSlot;
		dynamicWaveSlot.refCount = 0;
		return true;
	}

	return false;
}


// ----------------------------------------------------------------
// Query how many slots exist in total
// ----------------------------------------------------------------
u32 audWaveSlotManager::GetNumSlots() const
{
	return m_dynamicWaveSlots.GetCount();
}

// ----------------------------------------------------------------
// Query how many slots have been used
// ----------------------------------------------------------------
u32 audWaveSlotManager::GetNumUsedSlots() const
{
	u32 numSlotsUsed = 0;

	for(u32 i = 0; i < m_dynamicWaveSlots.GetCount(); i++)
	{
		if(m_dynamicWaveSlots[i].refCount > 0)
		{
			numSlotsUsed++;
		}
	}

	return numSlotsUsed;
}

// ----------------------------------------------------------------
// Query how many slots are currently loading data
// ----------------------------------------------------------------
u32 audWaveSlotManager::GetNumLoadsInProgress() const
{
	u32 numSlotsLoading = 0;

	for(u32 i = 0; i < m_dynamicWaveSlots.GetCount(); i++)
	{
		if(m_dynamicWaveSlots[i].IsLoading())
		{
			numSlotsLoading++;
		}
	}

	return numSlotsLoading;
}

#if __BANK
// ----------------------------------------------------------------
// Render the current status of the slot manager
// ----------------------------------------------------------------
void audWaveSlotManager::DebugDraw(f32 xOffset, f32 yOffset)
{
	for(int loop = 0; loop < m_dynamicWaveSlots.GetCount(); loop++)
	{
		float currentXOffset = xOffset;
		char tempString[128];

		grcDebugDraw::Text(Vector2(currentXOffset, yOffset), Color32(255,255,255), m_dynamicWaveSlots[loop].waveSlot->GetSlotName());
		currentXOffset += 0.17f;
		const char* bankName = NULL;

		if(m_dynamicWaveSlots[loop].waveSlot->GetLoadedBankId() < AUD_INVALID_BANK_ID &&
		   m_dynamicWaveSlots[loop].refCount > 0)
		{
			audWaveSlot::audWaveSlotLoadStatus bankStatus = m_dynamicWaveSlots[loop].waveSlot->GetBankLoadingStatus(m_dynamicWaveSlots[loop].waveSlot->GetLoadedBankId());

			if(bankStatus == audWaveSlot::LOADING)
			{ 
				bankName = "Loading";
			}
			else if(bankStatus == audWaveSlot::FAILED)
			{
				bankName = "Failed";
			}
			else if(bankStatus == audWaveSlot::LOADED)
			{
				bankName = m_dynamicWaveSlots[loop].waveSlot->GetLoadedBankName();
			}
			else
			{
				bankName = "Buh?";
			}
		}
		else
		{
			bankName = "Empty";
		}

		u32 waveSlotRefCounts = 0;
		
		if(m_dynamicWaveSlots[loop].waveSlot)
		{
			waveSlotRefCounts = m_dynamicWaveSlots[loop].waveSlot->GetReferenceCount();
		}

		sprintf(tempString, "%s    (%d ref / %d wave ref)", bankName, m_dynamicWaveSlots[loop].refCount, waveSlotRefCounts);
		grcDebugDraw::Text(Vector2(currentXOffset, yOffset), Color32(255,255,255), tempString);
		yOffset += 0.02f;
	}
}
#endif

// ----------------------------------------------------------------
// Load a bank into a free slot of the given type. Pass in 
// AUD_WAVE_SLOT_ANY if you don't care which slot type is used
// ----------------------------------------------------------------
audDynamicWaveSlot* audWaveSlotManager::LoadBank(const u32 bankId, const u32 slotTypeHash, bool allowBankShuffling, audEntity* entity, naWaveLoadPriority::Priority loadPriority, f32 delayTime)
{
	naAssertf(bankId!=0xffff, "Invalid bankId passed through as a parameter");
	s32 bestMatchSlotIndex = -1;
	u32 bestFreeSlotTime = 0;
	u32 bestPriority = 0;

	for(u32 i = 0; i < GetNumSlots(); i++)
	{
		audWaveSlot::audWaveSlotLoadStatus status = m_dynamicWaveSlots[i].waveSlot->GetBankLoadingStatus(bankId);

		// Always prefer a loaded slot, whether it has refs or not
		if(m_dynamicWaveSlots[i].loadedBankId == bankId &&
		   (status == audWaveSlot::LOADED || status == audWaveSlot::LOADING || m_dynamicWaveSlots[i].delayTime > 0.0f))
		{
			RegisterEntity(entity, i);
			return &m_dynamicWaveSlots[i];
		}
		else
		{
			bool validSlot = (slotTypeHash == AUD_WAVE_SLOT_ANY);

			// First check that this slot can have banks of the given type loaded into it
			if(!validSlot)
			{
				for(int typeLoop = 0; typeLoop < m_dynamicWaveSlots[i].slotTypes.GetCount(); typeLoop++)
				{
					if(m_dynamicWaveSlots[i].slotTypes[typeLoop] == slotTypeHash)
					{
						validSlot = true;
						break;
					}
				}
			}

			// If this slot can load banks of the required type
			if(validSlot)
			{
				// Check both our high level reference count and the waveslots voice ref count to be sure its free
				if(m_dynamicWaveSlots[i].refCount == 0 && m_dynamicWaveSlots[i].waveSlot->GetReferenceCount() == 0)
				{
					// So we have a free slot - need to work out if its preferable to any free slots we've already found
					bool useThisSlot = false;

					// First slot we've found
					if(bestMatchSlotIndex == -1)
					{
						useThisSlot = true;
					}
					// Better priority than the best slot so far
					else if(m_dynamicWaveSlots[i].slotPriority < bestPriority)
					{
						useThisSlot = true;
					}
					// Equal priority, but slot has gone unused for longer
					else if(m_dynamicWaveSlots[i].slotPriority == bestPriority &&
							m_dynamicWaveSlots[i].timeLastUsed < bestFreeSlotTime)
					{
						useThisSlot = true;
					}

					if(useThisSlot)
					{
						bestMatchSlotIndex = i;
						bestPriority = m_dynamicWaveSlots[i].slotPriority;
						bestFreeSlotTime = m_dynamicWaveSlots[i].timeLastUsed;
					}
				}
				// This slot is valid, has a better priority, is currently in use by a rule, but is not actually being played from
				else if(m_dynamicWaveSlots[i].refCount > 0 &&
						m_dynamicWaveSlots[i].waveSlot->GetReferenceCount() == 0 &&
						(m_dynamicWaveSlots[i].slotPriority < bestPriority || bestMatchSlotIndex == -1))
				{
					if(allowBankShuffling)
					{
						// Check all the other slots and see if theres one that this bank would be better suited to (ie. better priority, currently empty)
						for(u32 comparisonIndex = 0; comparisonIndex < GetNumSlots(); comparisonIndex++)
						{
							if(comparisonIndex != i &&
								m_dynamicWaveSlots[comparisonIndex].slotPriority < m_dynamicWaveSlots[i].slotPriority &&
								m_dynamicWaveSlots[comparisonIndex].waveSlot->GetReferenceCount() == 0 &&
								m_dynamicWaveSlots[comparisonIndex].refCount == 0)
							{
								for(int typeLoop = 0; typeLoop < m_dynamicWaveSlots[comparisonIndex].slotTypes.GetCount(); typeLoop++)
								{
									if(m_dynamicWaveSlots[comparisonIndex].slotTypes[typeLoop] == m_dynamicWaveSlots[i].loadedBankType)
									{
										// Job done! Mark this as a suitable slot. Once we load the incoming bank into here, the calling
										// system needs to recognise that the slot has been invalidated (by seeing that the loadedBankId
										// value has changed) and request a re-load of the evicted bank into the more optimal slot.
										bestMatchSlotIndex = i;
										bestPriority = m_dynamicWaveSlots[i].slotPriority;
										bestFreeSlotTime = 0; // Always want to bank shuffle if poss, so set the time to min
										break;
									}
								}
							}
						}	
					}	
				}
			}
		}
	}

	if(bestMatchSlotIndex != -1)
	{
		// Mark this slot as used and request the load
		if(delayTime > 0.0f || m_dynamicWaveSlots[bestMatchSlotIndex].waveSlot->LoadBank(bankId, loadPriority))
		{
			RegisterEntity(entity, bestMatchSlotIndex);
			m_dynamicWaveSlots[bestMatchSlotIndex].delayTime = delayTime;
			m_dynamicWaveSlots[bestMatchSlotIndex].loadPriority = loadPriority;
			m_dynamicWaveSlots[bestMatchSlotIndex].loadedBankId = bankId;
			m_dynamicWaveSlots[bestMatchSlotIndex].loadedBankType = slotTypeHash;
			return &m_dynamicWaveSlots[bestMatchSlotIndex];
		}	
	}

	return NULL;
}

// ----------------------------------------------------------------
// Register an entity as being associated with this wave slot
// ----------------------------------------------------------------
void audWaveSlotManager::RegisterEntity(audEntity* entity, u32 waveSlotIndex)
{
	if(entity)
	{
		m_dynamicWaveSlots[waveSlotIndex].registeredEntities.PushAndGrow(entity);
		m_dynamicWaveSlots[waveSlotIndex].refCount++;
	}
	else
	{
		m_dynamicWaveSlots[waveSlotIndex].refCount++;
	}
}

// ----------------------------------------------------------------
// Load a bank required by a particular sound
// ----------------------------------------------------------------
audDynamicWaveSlot* audWaveSlotManager::LoadBankForSound(const u32 soundNameHash, const u32 slotType, bool allowBankShuffling, audEntity* entity, naWaveLoadPriority::Priority loadPriority, f32 delayTime)
{
	// This helper class is passed down to the sound factory's ProcessHierarchy function, and gets called 
	// on every sound that we encounter in the hierarchy
	class RequiredBankBuilderFn : public audSoundFactory::audSoundProcessHierarchyFn
	{
	public:
		atArray<u32> bankList;

		void operator()(u32 classID, const void* soundData)
		{
			u32 bankID = AUD_INVALID_BANK_ID;

			if(classID == SimpleSound::TYPE_ID)
			{
				const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
				bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
			}
			else if(classID == GranularSound::TYPE_ID)
			{
				const GranularSound * sound = reinterpret_cast<const GranularSound*>(soundData);
				bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->Channel0.BankName);
			}

			if(bankID < AUD_INVALID_BANK_ID)
			{
				// Bail out if we've already encountered this bank
				for(u32 existingBankLoop = 0; existingBankLoop < bankList.GetCount(); existingBankLoop++)
				{
					if(bankList[existingBankLoop] == bankID)
					{
						return;
					}
				}

				bankList.PushAndGrow(bankID);
			}
		}
	};

	RequiredBankBuilderFn bankListBuilder;
	SOUNDFACTORY.ProcessHierarchy(soundNameHash, bankListBuilder);

	for(s32 loop = 0; loop < bankListBuilder.bankList.GetCount(); )
	{
		audWaveSlot* bankSlot = audWaveSlot::FindLoadedBankWaveSlot(bankListBuilder.bankList[loop]);

		if(bankSlot && bankSlot->IsStatic())
		{
			bankListBuilder.bankList.Delete(loop);
		}
		else
		{
			loop++;
		}
	}
	
	if(bankListBuilder.bankList.GetCount() == 1)
	{
		return LoadBank(bankListBuilder.bankList[0], slotType, allowBankShuffling, entity, loadPriority, delayTime);
	}
#if __DEV
	else if(bankListBuilder.bankList.GetCount() == 0)
	{
		audWarningf("Bank load failed: No banks identified as being required to play sound %d", soundNameHash);
	}
	else
	{
		audWarningf("Bank load failed: Multiple (%d) banks identified as being required to play sound %d", bankListBuilder.bankList.GetCount(), soundNameHash);
	}
#endif
	
	return NULL;
}

// ----------------------------------------------------------------
// Free up a wave slot
// ----------------------------------------------------------------
void audWaveSlotManager::FreeWaveSlot(audDynamicWaveSlot*& slot, audEntity* entity)
{
	if(slot &&
	   slot->refCount > 0)
	{
		slot->refCount--;

		if(entity)
		{
			s32 entityIndex = slot->registeredEntities.Find(entity);
			audAssertf(entityIndex >= 0, "Could not find registered entity in list!");
			slot->registeredEntities.Delete(entityIndex);
		}

		if(slot->refCount == 0)
		{
			audAssertf(slot->registeredEntities.GetCount() == 0, "Expected registered entities to be clear!");
			slot->timeLastUsed = fwTimer::GetTimeInMilliseconds();
			slot->delayTime = 0.0f;
		}

		slot = NULL;
	}
}

// ----------------------------------------------------------------
// Update the wave slot manager
// ----------------------------------------------------------------
void audWaveSlotManager::Update()
{
	for(u32 i = 0; i < GetNumSlots(); i++)
	{
		if(m_dynamicWaveSlots[i].delayTime > 0.0f)
		{
			m_dynamicWaveSlots[i].delayTime -= fwTimer::GetTimeStep();

			if(m_dynamicWaveSlots[i].delayTime <= 0.0f)
			{
				m_dynamicWaveSlots[i].waveSlot->LoadBank(m_dynamicWaveSlots[i].loadedBankId, m_dynamicWaveSlots[i].loadPriority);
			}
		}
	}
}


