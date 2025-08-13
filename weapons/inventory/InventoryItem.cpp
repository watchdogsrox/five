// File header
#include "Weapons/Inventory/InventoryItem.h"

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Streaming/Streaming.h"
#include "Streaming/streamingvisualize.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()


//////////////////////////////////////////////////////////////////////////
// CInventoryItem
//////////////////////////////////////////////////////////////////////////

CInventoryItem::CInventoryItem()
: m_uKey(0)
, m_pInfo(NULL)
, m_uModelHashOverride(0)
{
}

CInventoryItem::CInventoryItem(u32 uKey)
: m_uKey(uKey)
, m_pInfo(NULL)
, m_uModelHashOverride(0)
{
}

CInventoryItem::CInventoryItem(u32 uKey, u32 uItemHash, const CPed* UNUSED_PARAM(pPed))
: m_uKey(uKey)
, m_pInfo(CWeaponInfoManager::GetInfo(uItemHash))
, m_uModelHashOverride(0)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INVENTORY);

	const CItemInfo* pInfo = GetInfo();
	weaponFatalAssertf(pInfo, "CInventoryItem: m_pInfo is NULL - this is invalid");

	// Request the assets
	u32 iModelIndex = pInfo->GetModelIndex();
	fwModelId modelId((strLocalIndex(iModelIndex)));
	if(modelId.IsValid())
	{
		strLocalIndex transientLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId);
		m_assetRequester.Request(transientLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
	}
}

CInventoryItem::~CInventoryItem()
{
	m_assetRequester.Release();
}

void CInventoryItem::Init()
{
	CInventoryItem::InitPool(MEMBUCKET_GAMEPLAY);
}

void CInventoryItem::Shutdown()
{
	CInventoryItem::ShutdownPool();
}

void CInventoryItem::SetModelHash(u32 uModelHash)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::INVENTORY);

	// Release any assets
	m_assetRequester.Release();

	// Request the assets
	fwModelId modelId = CModelInfo::GetModelIdFromName(uModelHash);
	if(modelId.IsValid())
	{
		strLocalIndex transientLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId);
		m_assetRequester.Request(transientLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
	}

	// Store the model hash
	m_uModelHashOverride = uModelHash;
}

bool CInventoryItem::GetIsHashValid(u32 uItemHash)
{
	return CWeaponInfoManager::GetInfo(uItemHash) != NULL;
}
