#ifndef WEAPON_ITEM_H
#define WEAPON_ITEM_H

// Game headers
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/components/WeaponComponent.h"
#include "Weapons/Inventory/InventoryItem.h"

#if __ASSERT
#include "fwsys/timer.h"
#include "Script/Thread.h"
#endif // __ASSERT

//////////////////////////////////////////////////////////////////////////
// CWeaponItem
//////////////////////////////////////////////////////////////////////////

class CWeaponItem : public CInventoryItem
{
public:

	CWeaponItem();
	CWeaponItem(u32 uKey);
	CWeaponItem(u32 uKey, u32 uItemHash, const CPed* pPed);
	~CWeaponItem();

	//
	// Components
	//

	// Weapon components
	struct sComponent
	{
		// The point on the weapon we will be attached too - only one component is allowed per attachPoint
		eHierarchyId attachPoint;

		// The component info
		const CWeaponComponentInfo* pComponentInfo;

		// Active, currently used to know if the flash light was on or off so that it will have the same state when re-equipping it.
		bool m_bActive;
		
		// Individual tint value for this component
		u8 m_uTintIndex;
	};
	typedef atFixedArray<sComponent, CWeaponInfo::MAX_ATTACH_POINTS> Components;

	// Add a component
	bool AddComponent(u32 uComponentHash);

	// Remove a component
	bool RemoveComponent(u32 uComponentHash);

	// Clear all components
	void RemoveAllComponents();

	// Query components
	const Components* GetComponents() const { return m_pComponents; }

	// Set enabled on weapon component
	void SetActiveComponent(int i, bool active);

	// Set tint index on weapon component
	void SetComponentTintIndex(int i, u8 tintIndex);

	// Query type of component
	template<typename _Class> const _Class* GetComponentInfo() const
	{
		if(m_pComponents)
		{
			for(s32 i = 0; i < m_pComponents->GetCount(); i++)
			{
				const CWeaponComponentInfo* pComponentInfo = (*m_pComponents)[i].pComponentInfo;
				if(pComponentInfo && pComponentInfo->GetIsClass<_Class>())
				{
					return static_cast<const _Class*>(pComponentInfo);
				}
			}
		}
		return NULL;
	}

	//
	// Tints
	//

	// Set the tint index
	void SetTintIndex(u8 uTintIndex);

	// Get the tint index
	u8 GetTintIndex() const { return m_uTintIndex; }

	//
	// Accessors
	//

	// Check if the hash is valid
	static bool GetIsHashValid(u32 uItemHash);

	// Get the key that we sort the items by
	static u32 GetKeyFromHash(u32 uItemHash);

	// Get a pointer to the ammo override
	const CAmmoInfo* GetAmmoInfoOverride() const { return m_pAmmoInfoOverride; }

	// Access to the weapon data
	const CWeaponInfo* GetInfo() const { return static_cast<const CWeaponInfo*>(CInventoryItem::GetInfo()); }

	// Is the model and anims streamed in? If adding new dependencies, please ensure to update CWeaponScriptResource with the new dependencies 
	bool GetIsStreamedIn();

	// Interface to only load anims that are necessary when the weapon is equipped
	void RequestEquippedAnims(const CPed* pPed);
	void RequestEquippedAnims(const CPed* pPed, FPSStreamingFlags fpsStreamingFlags);
	void ReleaseEquippedAnims();

	u32 GetTimer() const { return m_uTimer; }
	void SetTimer(u32 uTime) { m_uTimer = uTime; }

	s32 GetLastAmmoInClip() const { return m_iLastAmmoInClip; }
	void SetLastAmmoInClip(const s32 iAmount, const CPed *pPed = NULL);

	void SetCanSelect(bool bCanSelect) { m_bCanSelect = bCanSelect; }
	bool GetCanSelect() const { return m_bCanSelect; }

	u32 GetCachedWeaponState() const { return m_uCachedWeaponState; }
	void SetCachedWeaponState(const u32 uWeaponState)  { m_uCachedWeaponState = uWeaponState; }

private:

	enum RequestType
	{
		RT_Anim,
		RT_Component,
		RT_Ammo,
		RT_Equipped,
	};

	void RequestAnims(const CPed* pPed);
	void RequestAnims(const CPed* pPed, FPSStreamingFlags fpsStreamingFlags);
	void RequestAnimsFromClipSet(const RequestType type, const fwMvClipSetId& clipSetId);
	void RequestModel(const RequestType type, const u32 modelHash);
	void RequestComponents();

	// Push the request
	void RequestAsset(const RequestType type, u32 requestId, u32 moduleId);

	//
	// Members
	//

	// Components
	Components* m_pComponents;

	// Streaming request helper
	static const s32 MAX_ANIM_REQUESTS = 80;
	typedef strRequestArray<MAX_ANIM_REQUESTS> AssetRequesterAnim;
	typedef strRequestArray<CWeaponInfo::MAX_ATTACH_POINTS> AssetRequesterComponent;
	static const s32 MAX_ORDNANCE_REQUESTS = 2;
	typedef strRequestArray<MAX_ORDNANCE_REQUESTS> AssetRequesterAmmo;
	static const s32 MAX_EQUIPPED_ANIM_REQUESTS = 6;
	typedef strRequestArray<MAX_EQUIPPED_ANIM_REQUESTS> AssetRequesterWhenEquipped;

	AssetRequesterAnim* m_pAssetRequesterAnim;
	AssetRequesterComponent* m_pAssetRequesterComponent;
	AssetRequesterAmmo* m_pAssetRequesterAmmo;
	AssetRequesterWhenEquipped* m_pAssetRequesterWhenEquipped;

#if __BANK
	u32 m_nNumberAnimRequests;
#endif

	// Timer used for the next shot. Moved from CWeapon to here.
	u32 m_uTimer;

	// Tint index
	u8 m_uTintIndex;

	// Cached tint index used when attaching a variant weapon model component. Tint is restored upon removing the component.
	u8		m_uCachedWeaponTintIndex;
	bool	m_bCachedWeaponTint;
	bool	m_bCanSelect;
	s32		m_iLastAmmoInClip;

	const CAmmoInfo* m_pAmmoInfoOverride;

	u32 m_uCachedWeaponState;

private:
	// Copy constructor and assignment operator are private
	CWeaponItem(const CWeaponItem& other);
	CWeaponItem& operator=(const CWeaponItem& other);
};

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponItem::SetActiveComponent(int i, bool active)
{
	if(m_pComponents)
	{
		weaponFatalAssertf( i < m_pComponents->GetCount(), "Accessing outside of bounds");
		(*m_pComponents)[i].m_bActive = active;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponItem::SetComponentTintIndex(int i, u8 tintIndex)
{
	if(m_pComponents)
	{
		weaponFatalAssertf( i < m_pComponents->GetCount(), "Accessing outside of bounds");
		(*m_pComponents)[i].m_uTintIndex = tintIndex;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeaponItem::GetIsStreamedIn()
{
	return CInventoryItem::GetIsStreamedIn() && 
		(!m_pAssetRequesterAnim || m_pAssetRequesterAnim->HaveAllLoaded()) && 
		(!m_pAssetRequesterComponent || m_pAssetRequesterComponent->HaveAllLoaded()) && 
		(!m_pAssetRequesterAmmo || m_pAssetRequesterAmmo->HaveAllLoaded()) && 
		(!m_pAssetRequesterWhenEquipped || m_pAssetRequesterWhenEquipped->HaveAllLoaded());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_ITEM_H
