// File header
#include "Weapons/Inventory/WeaponItem.h"

#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "streaming/streamingvisualize.h"

// Game headers
#include "Debug/DebugScene.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Pickups/Data/PickupDataManager.h"
#include "Pickups/PickupManager.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Inventory/AmmoItem.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CWeaponItem
//////////////////////////////////////////////////////////////////////////

CWeaponItem::CWeaponItem()
: m_pComponents(NULL)
, m_pAssetRequesterAnim(NULL)
, m_pAssetRequesterComponent(NULL)
, m_pAssetRequesterAmmo(NULL)
, m_pAssetRequesterWhenEquipped(NULL)
, m_uTintIndex(0)
, m_uTimer(0)
, m_iLastAmmoInClip(-1)
, m_bCanSelect(true)
, m_pAmmoInfoOverride(nullptr)
, m_uCachedWeaponState(0)
{
}

CWeaponItem::CWeaponItem(u32 uKey)
: CInventoryItem(uKey)
, m_pComponents(NULL)
, m_pAssetRequesterAnim(NULL)
, m_pAssetRequesterComponent(NULL)
, m_pAssetRequesterAmmo(NULL)
, m_pAssetRequesterWhenEquipped(NULL)
, m_uTintIndex(0)
, m_uTimer(0)
, m_iLastAmmoInClip(-1)
, m_bCanSelect(true)
, m_pAmmoInfoOverride(nullptr)
, m_uCachedWeaponState(0)
{
}

CWeaponItem::CWeaponItem(u32 uKey, u32 uItemHash, const CPed* pPed)
: CInventoryItem(uKey, uItemHash, pPed)
, m_pComponents(NULL)
, m_pAssetRequesterAnim(NULL)
, m_pAssetRequesterComponent(NULL)
, m_pAssetRequesterAmmo(NULL)
, m_pAssetRequesterWhenEquipped(NULL)
, m_uTintIndex(0)
, m_uTimer(0)
, m_iLastAmmoInClip(-1)
, m_bCanSelect(true)
, m_pAmmoInfoOverride(nullptr)
, m_uCachedWeaponState(0)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::WEAPONITEM);

	weaponFatalAssertf(GetInfo()->GetIsClassId(CWeaponInfo::GetStaticClassId()), "CWeaponItem::CWeaponItem: m_pInfo is not of type weapon");

	//
	// Request our animations to be streamed in
	//

	RequestAnims(pPed);

	const CWeaponInfo::AttachPoints& attachPoints = GetInfo()->GetAttachPoints();
	for(s32 i = 0; i < attachPoints.GetCount(); i++)
	{
		const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents();
		for(int j = 0; j < components.GetCount(); j++)
		{
			if(components[j].m_Default)
			{
				AddComponent(components[j].m_Name.GetHash());
			}
		}
	}

	if(GetInfo()->GetCreateVisibleOrdnance())
	{
		const CAmmoInfo* pAmmoInfo = GetInfo()->GetAmmoInfo();
		if(pAmmoInfo)
		{
			RequestModel(RT_Ammo, pAmmoInfo->GetModelHash());
		}
	}

	// Request pickup model if it's not same as the weapon item.
	u32 uPickupHash = CPickupManager::ShouldUseMPPickups(NULL) ? GetInfo()->GetMPPickupHash() : GetInfo()->GetPickupHash();
	if(uPickupHash != 0)
	{
		const CPickupData* pPickupData = CPickupDataManager::GetPickupData( uPickupHash );
		u32 uModelHash = pPickupData->GetModelHash();
		if(pPickupData && uModelHash != GetInfo()->GetModelHash())
		{
			RequestModel(RT_Ammo, uModelHash);
		}
	}
}

CWeaponItem::~CWeaponItem()
{
	if(m_pComponents)
	{
		delete m_pComponents;
		m_pComponents = NULL;
	}

	if(m_pAssetRequesterAnim)
	{
		m_pAssetRequesterAnim->ClearRequiredFlags(STRFLAG_DONTDELETE);
		delete m_pAssetRequesterAnim;
		m_pAssetRequesterAnim = NULL;
	}

	if(m_pAssetRequesterComponent)
	{
		m_pAssetRequesterComponent->ClearRequiredFlags(STRFLAG_DONTDELETE);
		delete m_pAssetRequesterComponent;
		m_pAssetRequesterComponent = NULL;
	}

	if(m_pAssetRequesterAmmo)
	{
		m_pAssetRequesterAmmo->ClearRequiredFlags(STRFLAG_DONTDELETE);
		delete m_pAssetRequesterAmmo;
		m_pAssetRequesterAmmo = NULL;
	}

	if(m_pAssetRequesterWhenEquipped)
	{
		m_pAssetRequesterWhenEquipped->ClearRequiredFlags(STRFLAG_DONTDELETE);
		delete m_pAssetRequesterWhenEquipped;
		m_pAssetRequesterWhenEquipped = NULL;
	}
}

bool CWeaponItem::AddComponent(u32 uComponentHash)
{
	// Cache the weapon info
	const CWeaponInfo* pWeaponInfo = GetInfo();

	// Get the component info
	const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uComponentHash);
	if(weaponVerifyf(pComponentInfo, "Component Info Hash [%d] does not exist in metadata", uComponentHash))
	{
		// Find the appropriate attach point for this attachment
		const CWeaponComponentPoint* pAttachPoint = pWeaponInfo->GetAttachPoint(pComponentInfo);

#if __ASSERT
		if(!pAttachPoint)
		{
			weaponDisplayf("Attachments available for [%s]:", pWeaponInfo->GetName());
			const CWeaponInfo::AttachPoints& attachPoints = pWeaponInfo->GetAttachPoints();
			for(s32 i = 0; i < attachPoints.GetCount(); i++)
			{
				const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents(); 
				for(s32 j = 0; j < components.GetCount(); j++)
				{
					weaponDisplayf(" %s", components[j].m_Name.TryGetCStr());
				}
			}
		}
#endif // __ASSERT

		if(weaponVerifyf(pAttachPoint, "Attachment [%s] does not fit on weapon [%s]. See TTY for available attachments", pComponentInfo->GetName(), pWeaponInfo->GetName()))
		{
			if(m_pAssetRequesterComponent)
			{
				m_pAssetRequesterComponent->ReleaseAll();
			}

			if(m_pComponents)
			{
				for(s32 i = 0; i < m_pComponents->GetCount(); i++)
				{
					if((*m_pComponents)[i].attachPoint == pAttachPoint->GetAttachBoneId())
					{
						// Remove the old attachment at this point
						m_pComponents->DeleteFast(i);
						break;
					}
				}
			}

			// Add the attachment
			sComponent attach;
			attach.attachPoint = pAttachPoint->GetAttachBoneId();
			attach.pComponentInfo = pComponentInfo;
			attach.m_bActive = pComponentInfo->GetClassId() == WCT_Scope ? true: false;
			attach.m_uTintIndex = 0;
			if(!m_pComponents)
			{
				// Allocate if doesn't exist
				m_pComponents = rage_new Components;
			}
			weaponAssert(m_pComponents);
			m_pComponents->Push(attach);

			// Re-request all component assets
			RequestComponents();

			if (pComponentInfo->GetIsClassId(CWeaponComponentVariantModelInfo::GetStaticClassId()))
			{
				// Cache original tint index.
				m_uCachedWeaponTintIndex = GetTintIndex();
				m_bCachedWeaponTint = true;

				// Set the weapon tint index to value defined in metadata. 
				const CWeaponComponentVariantModelInfo *pComponentVariantModelInfo = static_cast<const CWeaponComponentVariantModelInfo*>(pComponentInfo);
				if (pComponentVariantModelInfo)
				{
					SetTintIndex(pComponentVariantModelInfo->GetTintIndexOverride());
				}
			}

			// B*2315688: Reset cached ammo info when adding a clip component.
			if (pComponentInfo->GetClassId() == WCT_Clip)
			{
				SetLastAmmoInClip(-1);
			}

			// If this component is an ammo component, store it as an override
			if( pComponentInfo->GetIsClass<CWeaponComponentClipInfo>() )
			{
				// Clear first, as the clip component we're adding might not have an override
				m_pAmmoInfoOverride = nullptr;

				const u32 uAmmoOverrideHash = static_cast<const CWeaponComponentClipInfo*>( pComponentInfo )->GetAmmoInfo();

				if( const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(uAmmoOverrideHash) )
					m_pAmmoInfoOverride = pAmmoInfo;
				else
					m_pAmmoInfoOverride = nullptr;
			}

			return true;
		}
	}

	return false;
}

bool CWeaponItem::RemoveComponent(u32 uComponentHash)
{
	if(m_pComponents)
	{
		for(s32 i = 0; i < m_pComponents->GetCount(); i++)
		{
			if((*m_pComponents)[i].pComponentInfo->GetHash() == uComponentHash)
			{
				// Find the appropriate attach point for this attachment
				const CWeaponInfo* pWeaponInfo = GetInfo();
				const CWeaponComponentPoint* pAttachPoint = pWeaponInfo->GetAttachPoint((*m_pComponents)[i].pComponentInfo);

				// If we're removing a model variant, restore the original model hash.
				if ((*m_pComponents)[i].pComponentInfo->GetIsClassId(CWeaponComponentVariantModelInfo::GetStaticClassId()))
				{
					// Restore original weapon tint.
					if (m_bCachedWeaponTint)
					{
						SetTintIndex(m_uCachedWeaponTintIndex);
					}
				}

				// If this component is an ammo component, remove any ammoinfo override
				if( (*m_pComponents)[i].pComponentInfo->GetIsClass<CWeaponComponentClipInfo>() )
				{
					m_pAmmoInfoOverride = nullptr;
				}

				// Remove the component
				m_pComponents->DeleteFast(i);

				// Check if we should have a default component and add it
				if (pAttachPoint)
				{
					const CWeaponComponentPoint::Components& components = pAttachPoint->GetComponents(); 
					for(s32 j = 0; j < components.GetCount(); j++)
					{
						if (components[j].m_Default)
						{
							AddComponent(components[j].m_Name.GetHash());
							break;
						}
					}
				}

				// We need to update the asset requester
				if(m_pAssetRequesterComponent)
				{
					m_pAssetRequesterComponent->ReleaseAll();
				}

				// Re-request all component assets
				RequestComponents();

				return true;
			}
		}
	}

	return false;
}

void CWeaponItem::RemoveAllComponents()
{
	if(m_pComponents)
	{
		u32 hashcomponents[CWeaponInfo::MAX_ATTACH_POINTS] = {0};

		//Gather the components to remove by hash
		for(s32 i = 0; i < m_pComponents->GetCount(); i++)
			hashcomponents[i] = (*m_pComponents)[i].pComponentInfo->GetHash();

		//Then remove them explicitly by hash - previously iterating the m_pComponents->GetCount() and invoking RemoveComponent on each caused problems
		//because implicitly in the RemoveComponent new components are added to the m_pComponents atfixedarray.  This gather then remove approach will 
		//eliminate this issue. Bail once we find the first zero hashcomponent. lavalley.
		for(s32 i = 0; ((i < CWeaponInfo::MAX_ATTACH_POINTS) && (hashcomponents[i])); i++)
			RemoveComponent(hashcomponents[i]);
	}
}

void CWeaponItem::SetTintIndex(u8 uTintIndex) 
{
#if __ASSERT
    if(m_uTintIndex != uTintIndex)
    {
	    Displayf("Frame: %i, TINT %i BEING SET on weapon", fwTimer::GetFrameCount(), uTintIndex);
        scrThread::PrePrintStackTrace();
        sysStack::PrintStackTrace();
    }
#endif // __ASSERT
	
	weaponitemDebugf3("CWeaponItem::SetTintIndex uTintIndex[%u]",uTintIndex);
	m_uTintIndex = uTintIndex; 
}

bool CWeaponItem::GetIsHashValid(u32 uItemHash)
{
	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(uItemHash);
	return pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId());
}

u32 CWeaponItem::GetKeyFromHash(u32 uItemHash)
{
	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(uItemHash);
	if(pInfo)
	{
		if(weaponVerifyf(pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Item Info [%s] is not of type weapon", pInfo->GetName()))
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
			return pWeaponInfo->GetSlot();
		}
	}

	return 0;
}

void CWeaponItem::RequestEquippedAnims(const CPed* pPed)
{
	RequestEquippedAnims(pPed, FPS_StreamDefault);
#if FPS_MODE_SUPPORTED
	RequestEquippedAnims(pPed, FPS_StreamIdle);
	RequestEquippedAnims(pPed, FPS_StreamRNG);
	RequestEquippedAnims(pPed, FPS_StreamLT);
	RequestEquippedAnims(pPed, FPS_StreamScope);
	RequestEquippedAnims(pPed, FPS_StreamThirdPerson);
#endif // FPS_MODE_SUPPORTED

#if __BANK
	if(m_pAssetRequesterAnim)
	{
		Assertf(m_pAssetRequesterAnim->GetRequestCount() <= MAX_ANIM_REQUESTS, "m_nNumberAnimRequests = %d,  need to bump MAX_ANIM_REQUESTS?", m_pAssetRequesterAnim->GetRequestCount());
	}
#endif
}

void CWeaponItem::RequestEquippedAnims(const CPed* pPed, FPSStreamingFlags fpsStreamingFlags)
{
	// Load anim requests that will only activate when this weapon is equipped
	if(pPed)
	{
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetMeleeClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetJumpUpperbodyClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetFallUpperbodyClipSetId(*pPed, fpsStreamingFlags) );
	}
	else
	{
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetMeleeClipSetId() );
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetJumpUpperbodyClipSetId() );
		RequestAnimsFromClipSet( RT_Equipped, GetInfo()->GetFallUpperbodyClipSetId() );
	}
}

void CWeaponItem::ReleaseEquippedAnims()
{
	if(m_pAssetRequesterWhenEquipped)
	{
		m_pAssetRequesterWhenEquipped->ReleaseAll();
	}
}

void CWeaponItem::RequestAnims(const CPed* pPed)
{
	RequestAnims(pPed, FPS_StreamDefault);
#if FPS_MODE_SUPPORTED
	// Stream all possible FPS anims
	RequestAnims(pPed, FPS_StreamIdle);
	RequestAnims(pPed, FPS_StreamRNG);
	RequestAnims(pPed, FPS_StreamLT);
	RequestAnims(pPed, FPS_StreamScope);

	//! Ensure we stream 3rd person anims (as contextual load may fail to do this).
	RequestAnims(pPed, FPS_StreamThirdPerson);
#endif // FPS_MODE_SUPPORTED
}

void CWeaponItem::RequestAnims(const CPed* pPed, FPSStreamingFlags fpsStreamingFlags)
{
	if(pPed)
	{
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverMovementClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverAlternateMovementClipSetId(*pPed, fpsStreamingFlags) );

		TUNE_GROUP_BOOL(COVER_TUNE, LOAD_WEAPON_ANIMS_AS_WEAPON_DEPENDENCIES, false);
		if (LOAD_WEAPON_ANIMS_AS_WEAPON_DEPENDENCIES)
		{
			RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverWeaponClipSetId(*pPed, fpsStreamingFlags) );
		}

		// Load the weapon clipset
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetWeaponClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetWeaponClipSetIdForClone(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetAlternativeClipSetWhenBlockedId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetInjuredWeaponClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetSwapWeaponClipSetId(*pPed, fpsStreamingFlags) );

		// Load motion set 
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionCrouchClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingStealthClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingUpperBodyClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFromStrafeTransitionUpperBodyClipSetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionClipSetIdForClone(*pPed, fpsStreamingFlags) );

		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetWeaponClipSetId(*pPed, fpsStreamingFlags) );

#if FPS_MODE_SUPPORTED
		// Load all possible FPS transition clips for this weapon
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromIdleClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromRNGClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromLTClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromScopeClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromUnholsterClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionFromStealthClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionToStealthClipsetId(*pPed, fpsStreamingFlags) );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFPSTransitionToStealthFromUnholsterClipsetId(*pPed, fpsStreamingFlags) );

		RequestAnimsFromClipSet( RT_Anim, fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSStrafeClipSetHash(*pPed, fpsStreamingFlags)) );
#endif	// FPS_MODE_SUPPORTED
	}
	else
	{
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverMovementClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverAlternateMovementClipSetId() );

		TUNE_GROUP_BOOL(COVER_TUNE, LOAD_WEAPON_ANIMS_AS_WEAPON_DEPENDENCIES, false);
		if (LOAD_WEAPON_ANIMS_AS_WEAPON_DEPENDENCIES)
		{
			RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedCoverWeaponClipSetId() );
		}

		// Load the weapon clipset
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetWeaponClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetAlternativeClipSetWhenBlockedId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetInjuredWeaponClipSetId() );

		// Load motion set 
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionCrouchClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingStealthClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetPedMotionStrafingUpperBodyClipSetId() );
		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetFromStrafeTransitionUpperBodyClipSetId() );

		RequestAnimsFromClipSet( RT_Anim, GetInfo()->GetWeaponClipSetId() );
	}
}

void CWeaponItem::RequestAnimsFromClipSet(const RequestType type, const fwMvClipSetId& clipSetId)
{
	// Go through all the anim dictionaries in the clip set and request them to be streamed in
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		while(pClipSet)
		{
			s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash()).Get();
			if(weaponVerifyf(animDictIndex > -1, "Can't find clip dictionary [%s]", pClipSet->GetClipDictionaryName().GetCStr()))
			{
				RequestAsset(type, animDictIndex, fwAnimManager::GetStreamingModuleId());
			}

			// Load the fallbacks aswell if they exist
			pClipSet = fwClipSetManager::GetClipSet(pClipSet->GetFallbackId());
		}
	}
}

void CWeaponItem::RequestComponents()
{
	// Check if we have a variant model component attached, and get index
	s32 varModId = -1;
	for(s32 i = 0; i < m_pComponents->GetCount(); i++)
	{
		if ((*m_pComponents)[i].pComponentInfo->GetClassId() == WCT_VariantModel)
		{
			varModId = i;
			break;
		}
	}

	// Request all component models attached to weapon item
	if(m_pComponents)
	{
		for(s32 i = 0; i < m_pComponents->GetCount(); i++)
		{
			u32 componentModelHash = (*m_pComponents)[i].pComponentInfo->GetModelHash();

			if (varModId >= 0)
			{
				// If we have variant model component and a valid replacement is found, request that model instead
				const CWeaponComponentVariantModelInfo* pComponentVariantModelInfo = static_cast<const CWeaponComponentVariantModelInfo*>((*m_pComponents)[varModId].pComponentInfo);
				if (pComponentVariantModelInfo)
				{
					u32 componentHash = (*m_pComponents)[i].pComponentInfo->GetHash();
					u32 variantModelHash = pComponentVariantModelInfo->GetExtraComponentModel(componentHash);
					if (variantModelHash != 0)
					{
						componentModelHash = variantModelHash;
					}
				}
			}

			if(componentModelHash != 0)
			{
				RequestModel(RT_Component, componentModelHash);
			}
		}
	}
}

void CWeaponItem::RequestModel(const RequestType type, const u32 modelHash)
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
	if(modelId.IsValid())
	{
		s32 transientLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId).Get();
		RequestAsset(type, transientLocalIndex, CModelInfo::GetStreamingModuleId());
	}
}

void CWeaponItem::RequestAsset(const RequestType type, u32 requestId, u32 moduleId)
{
	if((!m_pAssetRequesterAnim || m_pAssetRequesterAnim->RequestIndex(requestId, moduleId) == -1) && 
		(!m_pAssetRequesterComponent || m_pAssetRequesterComponent->RequestIndex(requestId, moduleId) == -1) &&
		(!m_pAssetRequesterAmmo || m_pAssetRequesterAmmo->RequestIndex(requestId, moduleId) == -1) &&
		(!m_pAssetRequesterWhenEquipped || m_pAssetRequesterWhenEquipped->RequestIndex(requestId, moduleId) == -1))
	{
		switch(type)
		{
		case RT_Anim:
			if(!m_pAssetRequesterAnim)
			{
				m_pAssetRequesterAnim = rage_new AssetRequesterAnim;
			}
			weaponAssert(m_pAssetRequesterAnim);
			m_pAssetRequesterAnim->PushRequest(requestId, moduleId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			break;
		case RT_Component:
			if(!m_pAssetRequesterComponent)
			{
				m_pAssetRequesterComponent = rage_new AssetRequesterComponent;
			}
			weaponAssert(m_pAssetRequesterComponent);
			m_pAssetRequesterComponent->PushRequest(requestId, moduleId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			break;
		case RT_Ammo:
			if(!m_pAssetRequesterAmmo)
			{
				m_pAssetRequesterAmmo = rage_new AssetRequesterAmmo;
			}
			weaponAssert(m_pAssetRequesterAmmo);
			m_pAssetRequesterAmmo->PushRequest(requestId, moduleId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			break;
		case RT_Equipped:
			if(!m_pAssetRequesterWhenEquipped)
			{
				m_pAssetRequesterWhenEquipped = rage_new AssetRequesterWhenEquipped;
			}
			weaponAssert(m_pAssetRequesterWhenEquipped);
			m_pAssetRequesterWhenEquipped->PushRequest(requestId, moduleId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			break;
		default:
			weaponAssertf(0, "Unhandled case [%d]", type);
			break;
		}
	}
}

void CWeaponItem::SetLastAmmoInClip(const s32 iAmount, const CPed *pPed)
{ 
	if (pPed && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->ms_Tunables.m_bUseAmmoCaching)
	{
		m_iLastAmmoInClip = iAmount;
	}
	else
	{
		m_iLastAmmoInClip = -1;
	}
}
