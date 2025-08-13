// 
// script/script_itemsets.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "script_itemsets.h"
#include "script.h"						// CTheScripts
#include "script_channel.h"				// scriptVerifyf

static const int kDefaultMaxItemSets = 9;
FW_INSTANTIATE_CLASS_POOL(CItemSet, kDefaultMaxItemSets, atHashString("ItemSet",0x6db663ab));

//-----------------------------------------------------------------------------

CItemSetManager* CItemSetManager::sm_Instance = NULL;


void CItemSetManager::InitClass(int bufferSizeRefs)
{
	CItemSet::InitPool(MEMBUCKET_GAMEPLAY);	// Not sure if this is the most appropriate bucket (or location of the call, for that matter). /FF

	Assert(!sm_Instance);
	sm_Instance = rage_new CItemSetManager(bufferSizeRefs);
}


void CItemSetManager::ShutdownClass()
{
	Assert(sm_Instance);	// Supposed to be matched with InitClass().

	Assert(CItemSet::GetPool()->GetNoOfUsedSpaces() == 0);

	delete sm_Instance;
	sm_Instance = NULL;

	CItemSet::ShutdownPool();
}


void CItemSetManager::DefragAll(bool removeUnusedElements)
{
	// Keep a pointer to the first part of the buffer that hasn't
	// been assigned yet.
	CItemSetRef* freeBuffPtr = m_Buffer;

	// Loop over the sets that are using the buffer, which should be in
	// rising order of allocated pointers.
	const int numSets = m_ItemSetsSortedByAlloc.GetCount();
	for(int i = 0; i < numSets; i++)
	{
		CItemSet& set = *m_ItemSetsSortedByAlloc[i];

		// Check to see that it's not already starting right where
		// the previous set ends, in which case we don't have to move it.
		if(set.m_EntityArray != freeBuffPtr)
		{
			// Copy the array elements to the new location. Note that
			// overlap here shouldn't be a problem, as we know that we
			// are always copying to an earlier address in memory, and
			// we do it from the beginning of the array towards the end.
			const int setSize = set.m_NumEntities;
			Assert(freeBuffPtr < set.m_EntityArray);
			for(int j = 0; j < setSize; j++)
			{
				freeBuffPtr[j] = set.m_EntityArray[j];

				// Make sure we don't leave unnecessary object references,
				// as tracking these also uses memory.
				set.m_EntityArray[j].Reset(NULL);
			}

			// Store the pointer to the new address.
			set.m_EntityArray = freeBuffPtr;
		}

		if(removeUnusedElements)
		{
			// In this case, we shrink each set's current allocation down to what's
			// currently used, meaning that it will have to reallocate if something
			// needs to be added.
			freeBuffPtr += set.m_NumEntities;
			set.m_MaxEntities = set.m_NumEntities;
		}
		else
		{
			// In this case, we leave any unused space for each set intact,
			// just eliminating space between the set allocations.
			freeBuffPtr += set.m_MaxEntities;
		}
	}
}


bool CItemSetManager::Allocate(CItemSet& set, int numRefs, bool allowDefrag)
{
	// We don't currently support reallocating a set that already has something in it,
	// user must free first.
	Assert(!set.m_EntityArray);
	Assert(!set.m_NumEntities);
	Assert(!set.m_MaxEntities);

	// Keep track of the start of a range of unallocated buffer space.
	CItemSetRef* freeRangeStartPtr = m_Buffer;

	// Also keep track of how much total unused space we have, in case
	// we need to defragment.
	int freeRangeSum = 0;

	// Loop over the sets, and then one step further to include space after
	// the last set.
	const int numSets = m_ItemSetsSortedByAlloc.GetCount();
	int foundSetIndex = -1;
	for(int i = 0; i < numSets + 1; i++)
	{
		CItemSetRef* nextFreeRangeStartPtr = NULL;

		int unusedSpace = 0;
		CItemSetRef* freeRangeEndPtr;
		if(i < numSets)
		{
			// Random verification check: the sets should be ordered based on the pointer into the buffer.
			Assert(i == numSets - 1 || m_ItemSetsSortedByAlloc[i + 1]->m_EntityArray >= m_ItemSetsSortedByAlloc[i]->m_EntityArray);

			// The current range of free addresses ends where this set begins.
			CItemSet* rangeSet = m_ItemSetsSortedByAlloc[i];
			freeRangeEndPtr = rangeSet->m_EntityArray;

			// The user set shouldn't be in here, since we just verified that
			// it has been freed.
			Assert(rangeSet != &set);

			// The next free range starts where this set ends.
			nextFreeRangeStartPtr = freeRangeEndPtr + rangeSet->m_MaxEntities;

			// Get the unused space within the set.
			unusedSpace = rangeSet->m_MaxEntities - rangeSet->m_NumEntities;
		}
		else
		{
			// For this last iteration, the current range of free addresses ends where the buffer ends.
			freeRangeEndPtr = m_Buffer + m_BufferSizeRefs;
		}

		// Compute the free space, and see if it's large enough.
		const int freeRangeSize = ptrdiff_t_to_int(freeRangeEndPtr - freeRangeStartPtr);
		if(freeRangeSize >= numRefs)
		{
			// Break out of the loop, found an acceptable range.
			// Note: we could be less greedy here and use some heuristic to pick
			// the range that is the best fit, somehow, 
			set.m_EntityArray = freeRangeStartPtr;
			set.m_MaxEntities = numRefs;
			foundSetIndex = i;
			break;
		}

		// Advance to the next range of free space.
		freeRangeStartPtr = nextFreeRangeStartPtr;

		// Sum up how much space we've got, for defragmentation purposes.
		// We include the unused space within each set here, because we use the
		// removeUnusedElements parameter for the DefragAll() call.
		freeRangeSum += freeRangeSize;
		freeRangeSum += unusedSpace;
	}

	if(foundSetIndex >= 0)
	{
		// Add an element to the array, to make space for the new one. Note that we should
		// have sized this array based on the pool of CItemSets, so if it fills up, it may
		// be worth investigating how that can be possible.
		m_ItemSetsSortedByAlloc.Append();

		// Now, move all the other sets and insert the new one to preserve the order.
		for(int i = numSets; i > foundSetIndex; i--)
		{
			m_ItemSetsSortedByAlloc[i] = m_ItemSetsSortedByAlloc[i - 1];
		}
		m_ItemSetsSortedByAlloc[foundSetIndex] = &set;

		// Success!
		return true;
	}

	// If allowed to defrag, check to see if we would have enough space after defragmentation.
	if(allowDefrag && freeRangeSum >= numRefs)
	{
		// Note: we use the removeUnusedElements option here, because if we are full, it
		// may be a good thing to reclaim unused space in some sets that may be hanging around
		// for a long time without changing, even though it means that we may have to reallocate
		// others soon again.
		DefragAll(true);

		// Perform the allocation again (disallowing defragmentation this time). This is expected
		// to succeed, because we already determined that there should be enough space.
		if(Verifyf(Allocate(set, numRefs, false), "Unexpected defragmentation failure."))
		{
			// Success!
			return true;
		}
	}

	// Not enough space.
	return false;
}


void CItemSetManager::Free(CItemSet& set)
{
	// Freeing memory for a set involves finding it in the
	// set array, resetting the references to set members,
	// clearing out the pointers/counts, and removing it
	// from the set array.
	// Note: we could use binary search here, based on the
	// pointer into the buffer, but it's not clear that it would
	// be faster because we would have to access memory within each set.
	const int numSets = m_ItemSetsSortedByAlloc.GetCount();
	for(int i = 0; i < numSets; i++)
	{
		if(m_ItemSetsSortedByAlloc[i] == &set)
		{
			const int numUsed = set.m_NumEntities;
			CItemSetRef* ptr = set.m_EntityArray;
			for(int j = 0; j < numUsed; j++)
			{
				ptr[j].Reset(NULL);
			}

			set.m_EntityArray = NULL;
			set.m_MaxEntities = 0;
			set.m_NumEntities = 0;

			// Note: can't be DeleteFast() since we want to maintain the order.
			m_ItemSetsSortedByAlloc.Delete(i);

			return;
		}
	}
}


CItemSetManager::CItemSetManager(int bufferSizeRefs)
{
	// Allocate the buffer.
	m_Buffer = rage_new CItemSetRef[bufferSizeRefs]; 
	m_BufferSizeRefs = bufferSizeRefs;

	// Resize the array of set pointers to match the pool.
	// Shouldn't need any more than that, unless there is some other source of objects.
	const int maxPossibleItemSets = CItemSet::GetPool()->GetSize();
	m_ItemSetsSortedByAlloc.Reserve(maxPossibleItemSets);
}


CItemSetManager::~CItemSetManager()
{
	// Any sets that use space in the manager need to be destroyed
	// before we destroy the manager, or bad things will happen.
	Assert(!m_ItemSetsSortedByAlloc.GetCount());

	delete []m_Buffer;
}

//-----------------------------------------------------------------------------

INSTANTIATE_RTTI_CLASS(CItemSet,0x129A1749);

CItemSet::CItemSet(bool autoClean)
		: m_EntityArray(NULL)
		, m_MaxEntities(0)
		, m_NumEntities(0)
		, m_AutoClean(autoClean)
{
}


CItemSet::~CItemSet()
{
	if(m_EntityArray)
	{
		CItemSetManager::GetInstance().Free(*this);
	}
}


bool CItemSet::AddEntityToSet(fwExtensibleBase& rNewEntity)
{
	// TODO: Implement type restriction?
#if 0
	if (	(m_iTypeRestriction != -1) 
		&&	(rNewEntity->GetEntityClassID() != m_iTypeRestriction)
		)
	{
		sagCoreScript::SCErrorf("Cannot add object of type %s to set restricted to type %s", GOHGetEntityTypeName(rNewEntity->GetEntityClassID()), GOHGetEntityTypeName(m_iTypeRestriction));
		return false;
	}
#endif

	if(!scriptVerifyf(!IsEntityInSet(rNewEntity), "%s:Entity already in set - cannot add multiple times", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return false;
	}

	if(!scriptVerifyf(m_NumEntities < kMaxRefsPerSet, "Limit of number of references in a CItemSet reached, increase kMaxRefsPerSet (%d).", kMaxRefsPerSet))
	{
		return false;
	}

	// Check to see if we need to allocate more memory because we've reached the
	// limit of what we allocated.
	const int numBefore = m_NumEntities;
	if(numBefore >= m_MaxEntities)
	{
		Assert(m_NumEntities == m_MaxEntities);

		// TODO: Think more about how to grow here, maybe. Perhaps should be relative to
		// the current size instead?
		const int kGrowSize = 16;
		int newMaxEntities = Min(m_MaxEntities + kGrowSize, kMaxRefsPerSet);

		// We could try to resize the current allocated range, which would
		// add some complexity and risk, or allocate a new range before we free
		// the old one, which would require space for it to be in memory twice.
		// But to keep things simple for now, we make a temporary copy of the
		// array contents first, then free the array, allocate a new one, and
		// restore from the copy.

		// Create the copy.
		fwExtensibleBase* tempPtrs[kMaxRefsPerSet];
		Assert(numBefore <= kMaxRefsPerSet);
		for(int i = 0; i < numBefore; i++)
		{
			tempPtrs[i] = m_EntityArray[i];
		}

		// Free old space.
		CItemSetManager& mgr = CItemSetManager::GetInstance();
		mgr.Free(*this);

		// Allocate new space.
		if(!Verifyf(mgr.Allocate(*this, newMaxEntities),
				"Failed to allocate set space for %d objects. Total size is %d, use PoolSize \"ItemSetBuffer\" in 'gameconfig.xml' to override.",
				newMaxEntities, mgr.GetBufferSize()))
		{
			// Note that if we get here, we've actually lost the current contents of the array,
			// may want to do something about that.
			return false;
		}

		// Restore the copy.
		Assert(m_MaxEntities > numBefore);
		for(int i = 0; i < numBefore; i++)
		{
			m_EntityArray[i] = tempPtrs[i];
		}
		m_NumEntities = numBefore;
	}

	// If we get here, there should be space in the array.
	Assert(m_NumEntities < m_MaxEntities);
	m_EntityArray[m_NumEntities++] = &rNewEntity;

	// Success!
	return true;
}


bool CItemSet::RemoveEntityFromSet(fwExtensibleBase& rEntity)
{
	// Find out if and where in the set this object is.
	int index = FindInSet(rEntity);

	if(index >= 0)
	{
		// Perform an order-changing delete by moving the
		// last element to where the removed object was.
		const int cnt = m_NumEntities;
		const int lastInArray = cnt - 1;
		if(index != lastInArray)
		{
			m_EntityArray[index] = m_EntityArray[lastInArray];
		}
		m_EntityArray[lastInArray].Reset(NULL);
		m_NumEntities = lastInArray;

		return true;
	}
	return false;
}


void CItemSet::Clean()
{
	// Here, we loop from the end of the array and clean out
	// any references to deleted objects. Note that the order
	// in the array may change here.
	int index = m_NumEntities - 1;
	int lastInArray = index;
	while(index >= 0)
	{
		if(!m_EntityArray[index])
		{
			if(index != lastInArray)
			{
				m_EntityArray[index] = m_EntityArray[lastInArray];
			}
			m_EntityArray[lastInArray].Reset(NULL);
			lastInArray--;
		}
		index--;
	}

	// Set the new size (same as it was if nothing got cleaned out).
	m_NumEntities = lastInArray + 1;
	Assert(m_NumEntities <= m_MaxEntities);
}


int CItemSet::FindInSet(const fwExtensibleBase& obj) const
{
	// Note: we could try to keep the set ordered and perform a binary search.
	// This may be a performance improvement if the set is large and doesn't change
	// often.

	const int cnt = m_NumEntities;
	for(int i = 0; i < cnt; i++)
	{
		if(m_EntityArray[i] == &obj)
		{
			return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------

// End of file script/script_itemsets.cpp
