#ifndef INVENTORY_ITEM_H
#define INVENTORY_ITEM_H

// Game headers
#include "Streaming/StreamingRequest.h"
#include "fwtl/pool.h"
#include "Weapons/Info/ItemInfo.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Inventory/InventoryItemRepository.h"

//////////////////////////////////////////////////////////////////////////
// CInventoryItem
//////////////////////////////////////////////////////////////////////////

class CInventoryItem
{
	// Allow CInventoryItemRepository to access protected functions
	template <typename _Item, s32 _MaxStorage> friend class CInventoryItemRepository;
public:
	FW_REGISTER_CLASS_POOL(CInventoryItem);

	// The maximum items we can allocate
	static const s32 MAX_STORAGE = 2000;

	CInventoryItem();
	CInventoryItem(u32 uKey);
	CInventoryItem(u32 uKey, u32 uItemHash, const CPed* pPed);
	~CInventoryItem();

	//
	// Init/Shutdown
	//

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	//
	// Model
	//

	// Set a model override
	void SetModelHash(u32 uModelHash);

	// Get the model hash
	u32 GetModelHash() const;

	//
	// Accessors
	//

	// Check if the hash is valid
	static bool GetIsHashValid(u32 uItemHash);

	// Get the key that we sort the items by
	static u32 GetKeyFromHash(u32 uItemHash);

	// Access to the item data
	const CItemInfo* GetInfo() const;

	// Is the model streamed in?
	bool GetIsStreamedIn();

#if !__NO_OUTPUT
	static void SpewPoolUsage();
#endif // !__NO_OUTPUT

protected:

	//
	// Members
	//

	// Key
	u32 m_uKey;

	// Pointer to the data
	RegdItemInfo m_pInfo;

	// Overridden model hash
	u32 m_uModelHashOverride;

	// Streaming request helper
	strRequest m_assetRequester;

private:
	// Copy constructor and assignment operator are private
	CInventoryItem(const CInventoryItem& other);
	CInventoryItem& operator=(const CInventoryItem& other);
};

////////////////////////////////////////////////////////////////////////////////

inline u32 CInventoryItem::GetModelHash() const
{
	if(m_uModelHashOverride != 0)
	{
		return m_uModelHashOverride;
	}

	const CItemInfo* pInfo = GetInfo();
	if(pInfo)
	{
		return pInfo->GetModelHash();
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CInventoryItem::GetKeyFromHash(u32 uItemHash)
{
	return uItemHash;
}

////////////////////////////////////////////////////////////////////////////////

inline const CItemInfo* CInventoryItem::GetInfo() const
{
	return m_pInfo;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CInventoryItem::GetIsStreamedIn()
{
	const CItemInfo* pInfo = GetInfo();
	return !pInfo || GetModelHash() == 0 || m_assetRequester.HasLoaded();
}

#endif // INVENTORY_ITEM_H
