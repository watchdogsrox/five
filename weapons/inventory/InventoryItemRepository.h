#ifndef INVENTORY_ITEM_REPOSITORY_H
#define INVENTORY_ITEM_REPOSITORY_H

// Rage headers
#include "atl/array.h"
#include "grcore/debugdraw.h"
#include "vector/color32.h"
#include "vector/vector3.h"

// Game headers
#include "Debug/Debug.h"
#include "Weapons/Inventory/InventoryListener.h"
#include "Weapons/WeaponChannel.h"

//////////////////////////////////////////////////////////////////////////
// CInventoryItemRepository
//////////////////////////////////////////////////////////////////////////

template <typename _Item, s32 _MaxStorage>
class CInventoryItemRepository
{
public:

	CInventoryItemRepository()
	: m_bAllModelsStreamed(true)
	{
	}

	~CInventoryItemRepository()
	{
		RemoveAllItems();
	}

	// Listeners
	void RegisterListener(CInventoryListener* pListener);

	// Add an item to the inventory
	_Item* AddItem(u32 uItemNameHash, const CPed* pPed);

	// Remove an item from the inventory
	bool RemoveItem(u32 uItemNameHash);

	// Remove all items from the inventory
	void RemoveAllItems();

	// Remove all items from the inventory except items specified
	void RemoveAllItemsExcept(u32 uItemNameHash, u32 uItemNameHash2 = 0);

	// Access a particular item
	const _Item* GetItem(u32 uItemNameHash) const;

	// Access a particular item
	_Item* GetItem(u32 uItemNameHash);

	// Access a particular item by key
	const _Item* GetItemByKey(u32 uKey) const;

	// Access a particular item by index
	const _Item* GetItemByIndex(s32 iIndex) const;

	// Access a particular item by index
	_Item* GetItemByIndex(s32 iIndex);

    // Get all items
    const atArray<_Item*>& GetAllItems() const;

	// Get the number of items in the repository
	s32 GetItemCount() const;

	// Is the specified item streamed in?
	bool GetIsStreamedIn(u32 uItemNameHash);

	// Are all items streamed in?
	bool GetIsStreamedIn() const;

	// Process streaming
	void ProcessStreaming();

#if __BANK // Only included in __BANK builds
	// Debug rendering
	void RenderDebug(const Vector3& vTextPos, Color32 colour) const;
#endif // __BANK

protected:

	// Non-const access to a particular item by key
	_Item* GetItemByKeyInternal(u32 uKey);

	// Non-const access to a particular item by index
	_Item* GetItemByIndexInternal(s32 iIndex);

	//
	// Search functions
	//

	// Search for an item with the key using a binary search
	s32 GetItemIndexFromKey(u32 uKey) const;

	// Sort the items for binary searching
	void QSort();

	// Function to sort the array so we can use a binary search
	static s32 PairSort(const _Item** a, const _Item** b) { return ((*a)->m_uKey < (*b)->m_uKey ? -1 : 1); }

private:

	//
	// Members
	//

	// Item repository
	atArray<_Item*> m_items;

	// Any active listeners
	static const s32 MAX_LISTENERS = 2;
	atFixedArray<fwRegdRef<CInventoryListener>, MAX_LISTENERS> m_listeners;

	// Flags
	bool m_bAllModelsStreamed : 1;
};

//////////////////////////////////////////////////////////////////////////
// Inline function implementations
//////////////////////////////////////////////////////////////////////////

template <typename _Item, s32 _MaxStorage> 
inline void CInventoryItemRepository<_Item, _MaxStorage>::RegisterListener(CInventoryListener* pListener)
{
	if(weaponVerifyf(!m_listeners.IsFull(), "Out of listener storage, max [%d]", m_listeners.GetMaxCount()))
	{
		m_listeners.Append() = pListener;
	}
}

template <typename _Item, s32 _MaxStorage> 
inline _Item* CInventoryItemRepository<_Item, _MaxStorage>::AddItem(u32 uItemNameHash, const CPed* pPed)
{
	// Check if the item is valid
	if(_Item::GetIsHashValid(uItemNameHash))
	{
		// Cache the key
		u32 uKey = _Item::GetKeyFromHash(uItemNameHash);

		// Check if the key currently exists in our data
		s32 iIndex = GetItemIndexFromKey(uKey);

		u32 uItemRemovedHash = 0;
		if(iIndex != -1)
		{
			uItemRemovedHash = m_items[iIndex]->GetInfo()->GetHash();

			// Free up the old storage
			delete m_items[iIndex];
			m_items[iIndex] = NULL;

			// Delete old entry that is using the same key
			m_items.DeleteFast(iIndex);
		}

		// Abort if we are out of storage
		weaponFatalAssertf(m_items.GetCount() < _MaxStorage, "Out of inventory storage: Max [%d]", _MaxStorage);

		// Construct the item to be added
		if(_Item::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			_Item* pItem = rage_new _Item(uKey, uItemNameHash, pPed);

			// Add the new item
			m_items.PushAndGrow(pItem);

			// Check if the item model is loaded and available, 
			// if not remember we have to stream it in
			if(!m_items.Top()->GetIsStreamedIn())
			{
				m_bAllModelsStreamed = false;
			}

			// Sort the new data
			QSort();

			if(uItemRemovedHash != 0)
			{
				for(s32 i = 0; i < m_listeners.GetCount(); i++)
				{
					if(m_listeners[i])
					{
						m_listeners[i]->ItemRemoved(uItemRemovedHash);
					}
				}
			}

			for(s32 i = 0; i < m_listeners.GetCount(); i++)
			{
				if(m_listeners[i])
				{
					m_listeners[i]->ItemAdded(uItemNameHash);
				}
			}

			// Return the item
			return pItem;
		}
#if !__NO_OUTPUT
		else
		{
			static bool sbSpewed = false;
			if(!sbSpewed)
			{
				_Item::SpewPoolUsage();
				sbSpewed = true;
			}
			weaponDisplayf("Ran out of [%s] storage, consider upping pool [%d]", _Item::GetPool()->GetName(), _Item::GetPool()->GetSize());
			weaponAssertf(0, "Ran out of [%s] storage, consider upping pool [%d]", _Item::GetPool()->GetName(), _Item::GetPool()->GetSize());
		}
#endif // !__NO_OUTPUT
	}

	// Failed to add item
	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline bool CInventoryItemRepository<_Item, _MaxStorage>::RemoveItem(u32 uItemNameHash)
{
	if(weaponVerifyf(uItemNameHash != 0, "Invalid ItemNameHash [%d]", uItemNameHash))
	{
		s32 iIndex = GetItemIndexFromKey(_Item::GetKeyFromHash(uItemNameHash));
		if(iIndex != -1)
		{
			delete m_items[iIndex];
			m_items[iIndex] = NULL;
			m_items.DeleteFast(iIndex);
			QSort();

			for(s32 i = 0; i < m_listeners.GetCount(); i++)
			{
				if(m_listeners[i])
				{
					m_listeners[i]->ItemRemoved(uItemNameHash);
				}
			}

			return true;
		}
	}

	return false;
}

template <typename _Item, s32 _MaxStorage>
inline void CInventoryItemRepository<_Item, _MaxStorage>::RemoveAllItems()
{
	// Free all the items
	for(s32 i = 0; i < m_items.GetCount(); i++)
	{
		delete m_items[i];
		m_items[i] = NULL;
	}

	// Reset the array
	m_items.Reset();

	// Invalidate flags
	m_bAllModelsStreamed = true;

	for(s32 i = 0; i < m_listeners.GetCount(); i++)
	{
		if(m_listeners[i])
		{
			m_listeners[i]->AllItemsRemoved();
		}
	}
}

template <typename _Item, s32 _MaxStorage>
inline void CInventoryItemRepository<_Item, _MaxStorage>::RemoveAllItemsExcept(u32 uItemNameHash,  u32 uNextItemNameHash /* = 0 */)
{
	s32 iIndex		= (uItemNameHash != 0)		? GetItemIndexFromKey(_Item::GetKeyFromHash(uItemNameHash))		: -1;
	s32 iNextIndex	= (uNextItemNameHash != 0)	? GetItemIndexFromKey(_Item::GetKeyFromHash(uNextItemNameHash)) : -1;
	
	if((iIndex == -1) && (iNextIndex == -1))
	{
		RemoveAllItems();
	}
	else
	{
		atArray<u32> removedItems(0, m_items.GetCount());
		for(s32 i = 0; i < m_items.GetCount(); i++)
		{
			if((i != iIndex) && (i != iNextIndex))
			{
				removedItems.Push(m_items[i]->GetInfo()->GetHash());
				delete m_items[i];
				m_items[i] = NULL;
			}
		}

		// Backup the excluded items
		_Item* pItem		= (iIndex != -1)		? m_items[iIndex]		: NULL;
		_Item* pNextItem	= (iNextIndex != -1)	? m_items[iNextIndex]	: NULL;

		// Reset the array
		m_items.Reset();

		// Add items back on
		if(pItem)
		{
			m_items.PushAndGrow(pItem);
		}

		if(pNextItem)
		{
			// need to do this again in case it hasn' been called above, won't grow a second time...
			m_items.PushAndGrow(pNextItem);
		}

		// Sort the new data as the object / keys may be in the wrong order...
		QSort();

		// Inform listeners of removed items
		for(s32 i = 0; i < m_listeners.GetCount(); i++)
		{
			if(m_listeners[i])
			{
				for(s32 j = 0; j < removedItems.GetCount(); j++)
				{
					m_listeners[i]->ItemRemoved(removedItems[j]);
				}
			}
		}
	}
}

template <typename _Item, s32 _MaxStorage>
inline const _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItem(u32 uItemNameHash) const
{
	if(weaponVerifyf(uItemNameHash != 0, "Invalid ItemNameHash [%d]", uItemNameHash))
	{
		s32 iIndex = GetItemIndexFromKey(_Item::GetKeyFromHash(uItemNameHash));
		if(iIndex != -1)
		{
			// Ensure the item we are returning has the right hash
			if(m_items[iIndex] && m_items[iIndex]->GetInfo()->GetHash() == uItemNameHash)
			{
				return m_items[iIndex];
			}
		}
	}

	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItem(u32 uItemNameHash)
{
	if(weaponVerifyf(uItemNameHash != 0, "Invalid ItemNameHash [%d]", uItemNameHash))
	{
		s32 iIndex = GetItemIndexFromKey(_Item::GetKeyFromHash(uItemNameHash));
		if(iIndex != -1)
		{
			// Ensure the item we are returning has the right hash
			if(m_items[iIndex] && m_items[iIndex]->GetInfo()->GetHash() == uItemNameHash)
			{
				return m_items[iIndex];
			}
		}
	}

	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline const _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItemByKey(u32 uKey) const
{
	if(weaponVerifyf(uKey != 0, "Invalid Key [%d]", uKey))
	{
		s32 iIndex = GetItemIndexFromKey(uKey);
		if(iIndex != -1)
		{
			return m_items[iIndex];
		}
	}

	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline const _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItemByIndex(s32 iIndex) const
{
	if(weaponVerifyf(iIndex != -1, "Invalid Index [%d]", iIndex))
	{
		return m_items[iIndex];
	}
	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItemByIndex(s32 iIndex)
{
	if(weaponVerifyf(iIndex != -1, "Invalid Index [%d]", iIndex))
	{
		return m_items[iIndex];
	}
	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline const atArray<_Item*>& CInventoryItemRepository<_Item, _MaxStorage>::GetAllItems() const
{
    return m_items;
}

template <typename _Item, s32 _MaxStorage>
inline s32 CInventoryItemRepository<_Item, _MaxStorage>::GetItemCount() const
{
	return m_items.GetCount();
}

template <typename _Item, s32 _MaxStorage>
inline bool CInventoryItemRepository<_Item, _MaxStorage>::GetIsStreamedIn(u32 uItemNameHash)
{
	_Item* pItem = GetItem(uItemNameHash);
	if(pItem)
	{
		return pItem->GetIsStreamedIn();
	}

	return false;
}

template <typename _Item, s32 _MaxStorage>
inline bool CInventoryItemRepository<_Item, _MaxStorage>::GetIsStreamedIn() const
{
	return m_bAllModelsStreamed;
}

template <typename _Item, s32 _MaxStorage>
inline void CInventoryItemRepository<_Item, _MaxStorage>::ProcessStreaming()
{
	m_bAllModelsStreamed = true;

	for(s32 i = 0; i < m_items.GetCount(); i++)
	{
		if(m_items[i] && !m_items[i]->GetIsStreamedIn())
		{
			m_bAllModelsStreamed = false;
		}
	}
}

#if __BANK // Only included in __BANK builds
template <typename _Item, s32 _MaxStorage>
void CInventoryItemRepository<_Item, _MaxStorage>::RenderDebug(const Vector3& vTextPos, Color32 colour) const
{
	s32 iLineCount = 0;
	char itemText[56];

	for(s32 i = 0; i < m_items.GetCount(); i++)
	{
		if(m_items[i] && m_items[i]->GetInfo())
		{
			sprintf(itemText, "Item: %s (0x%X)\n", m_items[i]->GetInfo()->GetName(), m_items[i]->GetInfo()->GetHash());
			grcDebugDraw::Text(vTextPos, colour, 0, grcDebugDraw::GetScreenSpaceTextHeight() * iLineCount++, itemText);
		}
	}
}
#endif // __BANK

template <typename _Item, s32 _MaxStorage>
inline _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItemByKeyInternal(u32 uKey)
{
	if(weaponVerifyf(uKey != 0, "Invalid Key [%d]", uKey))
	{
		s32 iIndex = GetItemIndexFromKey(uKey);
		if(iIndex != -1)
		{
			return m_items[iIndex];
		}
	}

	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline _Item* CInventoryItemRepository<_Item, _MaxStorage>::GetItemByIndexInternal(s32 iIndex)
{
	if(weaponVerifyf(iIndex != -1, "Invalid Index [%d]", iIndex))
	{
		return m_items[iIndex];
	}
	return NULL;
}

template <typename _Item, s32 _MaxStorage>
inline s32 CInventoryItemRepository<_Item, _MaxStorage>::GetItemIndexFromKey(u32 uKey) const
{
	s32 iLow = 0;
	s32 iHigh = m_items.GetCount()-1;
	while(iLow <= iHigh)
	{
		s32 iMid = (iLow + iHigh) >> 1;
		weaponFatalAssertf(m_items[iMid], "FindItem: Querying NULL pointer, shouldn't happen");
		if(uKey == m_items[iMid]->m_uKey)
			return iMid;
		else if(uKey < m_items[iMid]->m_uKey)
			iHigh = iMid-1;
		else
			iLow = iMid+1;
	}
	return -1;
}

template <typename _Item, s32 _MaxStorage>
inline void CInventoryItemRepository<_Item, _MaxStorage>::QSort()
{
	if(m_items.GetCount() > 1)
	{
		qsort(&m_items[0], m_items.GetCount(), sizeof(_Item*), (int (/*__cdecl*/ *)(const void*, const void*))PairSort);
	}
}

#endif // INVENTORY_ITEM_REPOSITORY_H
