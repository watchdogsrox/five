// File header
#include "Weapons/Inventory/PedEquippedWeapon.h"

// Game headers
#include "FrontEnd/NewHud.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Scene/World/GameWorld.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Components/WeaponComponentFactory.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Projectiles/ProjectileFactory.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/WeaponFactory.h"
#include "Weapons/WeaponManager.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponModelInfo* GetWeaponModelInfo(u32 iModelIndex)
{
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(iModelIndex)));
	if(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_WEAPON)
	{
		return static_cast<CWeaponModelInfo*>(pBaseModelInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CPedEquippedWeapon
//////////////////////////////////////////////////////////////////////////

CPedEquippedWeapon::CPedEquippedWeapon()
: m_pPed(NULL)
, m_pEquippedWeaponInfo(NULL)
, m_pEquippedVehicleWeaponInfo(NULL)
, m_pObject(NULL)
, m_pSecondObject(NULL)
, m_pUnarmedWeapon(NULL)
, m_pUnarmedKungFuWeapon(NULL)
, m_pWeaponNoModel(NULL)
, m_AttachPoint(AP_RightHand)
, m_bWeaponAnimsRequested(false)
, m_bWeaponAnimsStreamed(false)
, m_bArmed(false)
, m_fAutoReloadTimer(0.0f)
, m_bHasInterruptedReload(false)
{
}

CPedEquippedWeapon::~CPedEquippedWeapon()
{
	ReleaseWeaponAnims();

	// Destroy any existing object
	DestroyObject();

	// Delete any weapon that doesn't have an associated model
	if(m_pWeaponNoModel)
	{
		delete m_pWeaponNoModel;
		m_pWeaponNoModel = NULL;
	}
}

void CPedEquippedWeapon::Init(CPed* pPed)
{
	// Store the ped
	m_pPed = pPed;

	// Default to unarmed
	SetWeaponHash(m_pPed->GetDefaultUnarmedWeaponHash());
}

void CPedEquippedWeapon::Process()
{
	for(s32 i = 0; i < RHT_Count; i++)
	{
		for(s32 j = 0; j < FPS_StreamCount; j++)
		{
			if(!m_ClipRequestHelpers[i][j].IsLoadedOrInvalid())
				m_ClipRequestHelpers[i][j].Request();
		}
	}

	if( m_pPed )
	{
		// Melee stealth anims (only while ped is in stealth)
		if( m_pEquippedWeaponInfo && m_pPed->GetMotionData()->GetUsingStealth() )
		{
			if( !m_WeaponMeleeStealthClipRequestHelper.IsLoaded() )
				m_WeaponMeleeStealthClipRequestHelper.Request( m_pEquippedWeaponInfo->GetMeleeStealthClipSetId( *m_pPed ) );
		}
		else if( m_WeaponMeleeStealthClipRequestHelper.IsLoaded() )
			m_WeaponMeleeStealthClipRequestHelper.Release();


		// Had to change this from CPedEquippedWeapon::SetWeaponHash since we are creating new player peds
		// without CPed::IsLocalPlayer returning true.
		if( !HaveWeaponAnimsBeenRequested() && m_pPed->IsPlayer() )
		{
			RequestWeaponAnims();
		}

		// Continually ask the streaming system to AddRef otherwise the anims will mysteriously disappear
		if( !AreWeaponAnimsStreamedIn() && m_pEquippedWeaponInfo )
		{
			weaponFatalAssertf( m_pPed->GetInventory(), "m_pPed inventory is null!" );
			CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon( m_pEquippedWeaponInfo->GetHash() );
			m_bWeaponAnimsStreamed = pWeaponItem && pWeaponItem->GetIsStreamedIn();
		}

		// Ugh.. well I don't see another way around this without completely having to refactor the way unarmed
		// CWeapon objects are created/destroyed
		if( !m_pPed->GetIsDeadOrDying() && m_pEquippedWeaponInfo && m_pEquippedWeaponInfo->GetIsUnarmed() && !m_pObject && !m_pUnarmedWeapon )
		{
			CreateUnarmedWeaponObject( m_pEquippedWeaponInfo->GetHash() );
		}

		if( !m_pPed->GetIsDeadOrDying() && m_pEquippedWeaponInfo && m_pEquippedWeaponInfo->GetIsUnarmed() && m_pEquippedWeaponInfo->GetIsMeleeKungFu() && !m_pObject && !m_pUnarmedKungFuWeapon)
		{
			CreateUnarmedKungFuWeaponObject( m_pEquippedWeaponInfo->GetHash() );
		}

		//If our reload was interrupted and 10 seconds have past, automatically reload it
		//"IsReloadInterrupted" flag set in TASK_GUN if we interrupt reload due to swimming
		if (m_bHasInterruptedReload)
		{
			static dev_float RELOAD_TIME = 10000.0f;
			m_fAutoReloadTimer += fwTimer::GetTimeInMilliseconds() - fwTimer::GetPrevElapsedTimeInMilliseconds();

			// If we aim while running the timer, reset the flag and timer
			if (m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun))
			{
				//Reset interrupt flag and timer
				SetIsReloadInterrupted(false);
				m_fAutoReloadTimer = 0.0f;
			}
			// Else if timer is over 10s, reload weapon
			else if (m_fAutoReloadTimer > RELOAD_TIME)
			{
				if(m_pObject && m_pObject->GetWeapon())
				{
					m_pObject->GetWeapon()->DoReload();

					//Reset interrupt flag and timer
					SetIsReloadInterrupted(false);
					m_fAutoReloadTimer = 0.0f;
				}
			}
		}
	}
}

void CPedEquippedWeapon::SetWeaponHash(u32 uWeaponHash)
{
	if( !m_pEquippedWeaponInfo || m_pEquippedWeaponInfo->GetHash() != uWeaponHash ) 
	{
		const CWeaponInfo* pOldEquippedWeaponInfo = m_pEquippedWeaponInfo;

		ReleaseWeaponAnims();

		if(m_pWeaponNoModel)
		{
			delete m_pWeaponNoModel;
			m_pWeaponNoModel = NULL;
		}

		m_pEquippedWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);

		if( m_pEquippedWeaponInfo )
		{
			for(s32 i = 0; i < FPS_StreamCount; i++)
			{
				FPSStreamingFlags fpsStreamingFlags = (FPSStreamingFlags)i;

				fwMvClipSetId weaponClipSetId = m_pEquippedWeaponInfo->GetWeaponClipSetStreamedId(*m_pPed, fpsStreamingFlags);
				if( weaponClipSetId != CLIP_SET_ID_INVALID )
					m_ClipRequestHelpers[RHT_Weapon][i].Request(weaponClipSetId);

				fwMvClipSetId stealthClipSetId = m_pEquippedWeaponInfo->GetStealthWeaponClipSetId(*m_pPed, fpsStreamingFlags);
				if( stealthClipSetId != CLIP_SET_ID_INVALID )
					m_ClipRequestHelpers[RHT_Stealth][i].Request(stealthClipSetId);

				fwMvClipSetId scopeClipSetId = m_pEquippedWeaponInfo->GetScopeWeaponClipSetId(*m_pPed, fpsStreamingFlags);
				if( scopeClipSetId != CLIP_SET_ID_INVALID )
					m_ClipRequestHelpers[RHT_Scope][i].Request(scopeClipSetId);

				fwMvClipSetId hiCoverClipSetId = m_pEquippedWeaponInfo->GetHiCoverWeaponClipSetId(*m_pPed, fpsStreamingFlags);
				if( hiCoverClipSetId != CLIP_SET_ID_INVALID )
					m_ClipRequestHelpers[RHT_HiCover][i].Request(hiCoverClipSetId);
			}

			m_bArmed = m_pEquippedWeaponInfo->GetIsArmed();

			// Should we create a weapon without a model?
			if(m_pEquippedWeaponInfo->GetCreateWeaponWithNoModel())
			{
				m_pWeaponNoModel = WeaponFactory::Create(m_pEquippedWeaponInfo->GetHash(), m_pPed->GetInventory()->GetWeaponAmmo(m_pEquippedWeaponInfo), NULL, "CPedEquippedWeapon::SetWeaponHash - GetCreateWeaponWithNoModel");
			}
		}

		m_pEquippedVehicleWeaponInfo = NULL;

		if(pOldEquippedWeaponInfo && pOldEquippedWeaponInfo->GetRemoveWhenUnequipped())
		{
			if(m_pPed->GetInventory())
				m_pPed->GetInventory()->RemoveWeapon(pOldEquippedWeaponInfo->GetHash());
		}
	}
}

bool CPedEquippedWeapon::GetRequiresSwitch() const
{
	const CItemInfo* pInfo = GetWeaponInfo();
	if(pInfo)
	{
		if(!m_pObject)
		{
			if(pInfo->GetModelHash() != 0)
			{
				// No object and info has a model specified
				return true;
			}

			// When called from CTaskAmbientClips::GivePedProp(), pInfo->GetModelHash() will be 0,
			// but we'll have an override in CInventoryItem::m_uModelHashOverride, which we check for here.
			CPedInventory* pInv = m_pPed->GetInventory();
			if(pInv)
			{
				const CInventoryItem* pItem = pInv->GetWeapon(pInfo->GetHash());
				if(pItem && pItem->GetModelHash() != 0)
				{
					return true;
				}
			}
		}
		else if(GetObjectHash() != pInfo->GetHash())
		{
			// Object and info do not match
			return true;
		}
		else
		{
			weaponAssert(m_pPed->GetInventory());
			const CInventoryItem* pItem = m_pPed->GetInventory()->GetWeapon(pInfo->GetHash());
			if(pItem)
			{
				CWeaponItem* pWeaponItem = static_cast<CWeaponItem*>(const_cast<CInventoryItem*>(pItem));
				const CWeapon::Components& components = m_pObject->GetWeapon()->GetComponents();

				if(!pWeaponItem->GetComponents())
				{
					if(components.GetCount() == 0)
					{
						return false;
					}
					else
					{
						return true;
					}
				}

				const CWeaponItem::Components& attachments = *pWeaponItem->GetComponents();

				if(components.GetCount() != attachments.GetCount())
				{
					return true;
				}

				for(s32 i = 0; i < components.GetCount(); i++)
				{
					s32 j;
					for(j = 0; j < attachments.GetCount(); j++)
					{
						if(components[i]->GetInfo()->GetHash() == attachments[j].pComponentInfo->GetHash())
						{
							break;
						}
					}

					if(j == attachments.GetCount())
					{
						return true;
					}
				}
			}
		}
	}
	else if(m_pObject)
	{
		// No equipped item but we have an object
		return true;
	}

	return false;
}

u32 CPedEquippedWeapon::GetModelIndex() const
{
	const CObject* pObject = GetObject();
	if(pObject)
	{
		return pObject->GetModelIndex();
	}

	fwModelId weaponModelId;

	// Get the weapon info
	const CWeaponInfo* pInfo = GetWeaponInfo();
	if(weaponVerifyf(pInfo, "WeaponInfo is NULL"))
	{
		// Get the item
		weaponAssert(m_pPed->GetInventory());
		const CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon(pInfo->GetHash());
		if(weaponVerifyf(pWeaponItem, "Weapon [%s] does not exist in inventory", pInfo->GetName()))
		{
			CModelInfo::GetBaseModelInfoFromHashKey(pWeaponItem->GetModelHash(), &weaponModelId);
		}
	}

	return weaponModelId.GetModelIndex();
}

const CProjectile* CPedEquippedWeapon::GetProjectileOrdnance() const
{
	return GetProjectileOrdnance(GetObject());
}

CProjectile* CPedEquippedWeapon::GetProjectileOrdnance()
{
	return GetProjectileOrdnance(GetObject());
}

const CProjectile* CPedEquippedWeapon::GetProjectileOrdnance(const CObject* pWeaponObject)
{
	if( pWeaponObject )
	{
		CEntity* pAttachedChild = (CEntity*)pWeaponObject->GetChildAttachment();
		if ( pAttachedChild && pAttachedChild->GetIsClassId( CProjectile::GetStaticClassId() )  )
		{
			return (CProjectile*)pAttachedChild;
		}
	}

	return NULL;
}

CProjectile* CPedEquippedWeapon::GetProjectileOrdnance(CObject* pWeaponObject)
{
	if( pWeaponObject )
	{
		CEntity* pAttachedChild = (CEntity*)pWeaponObject->GetChildAttachment();
		if ( pAttachedChild && pAttachedChild->GetIsClassId( CProjectile::GetStaticClassId() )  )
		{
			return (CProjectile*)pAttachedChild;
		}
	}

	return NULL;
}

bool CPedEquippedWeapon::CreateObject(eAttachPoint attach)
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");

	// Destroy any existing object
	DestroyObject();

	// Get the weapon info
	const CWeaponInfo* pInfo = GetWeaponInfo();
	if(weaponVerifyf(pInfo, "WeaponInfo is NULL"))
	{
		// Get the item
		weaponAssert(m_pPed->GetInventory());
		const CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon(pInfo->GetHash());
		if(weaponVerifyf(pWeaponItem, "Weapon [%s] does not exist in inventory", pInfo->GetName()))
		{
			fwInteriorLocation interiorLocation = m_pPed->GetInteriorLocation().IsValid() ? m_pPed->GetInteriorLocation() : CGameWorld::OUTSIDE;
			Matrix34 mat = MAT34V_TO_MATRIX34(m_pPed->GetTransform().GetMatrix());
			weaponitemDebugf3("CPedEquippedWeapon::CreateObject pWeaponItem->GetTintIndex[%u]",pWeaponItem->GetTintIndex());

			// Check for variant model attachment to use instead of base weapon model
			u32 weaponModelHash = pWeaponItem->GetModelHash();
			const CWeaponComponentVariantModelInfo* pVariantInfo = pWeaponItem->GetComponentInfo<CWeaponComponentVariantModelInfo>();
			if (pVariantInfo)
			{
				// Make sure the model exists
				fwModelId variantModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(pVariantInfo->GetModelHash(), &variantModelId);	
				if (weaponVerifyf(variantModelId.IsValid(), "ModelId is invalid for VariantModel component on [%s], using original model instead.", pInfo->GetName()))
				{
					weaponModelHash = pVariantInfo->GetModelHash();
				}
			}

			m_pObject = CreateObject(pInfo, m_pPed->GetInventory()->GetWeaponAmmo(pInfo), mat, false, m_pPed, weaponModelHash, &interiorLocation, 1.0f, false, pWeaponItem->GetTintIndex());
			if(m_pObject)
			{
#if !__NO_OUTPUT
				if (NetworkInterface::IsGameInProgress() && m_pPed && m_pPed->IsPlayer())
				{
					weaponitemDebugf2("CPedEquippedWeapon::CreateObject CreateObject succeeded -- player[%s] isplayer[%d] isnetworkclone[%d] ped[%s] weapon[%s]",
						m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
						m_pPed->IsPlayer(),
						m_pPed->IsNetworkClone(),
						m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "", 
						m_pObject->GetModelName());
				}
#endif

				if (pInfo->GetIsMeleeFist() && pInfo->GetIsTwoHanded())
				{
					// Attach second object before main object so we store correct main attach point.
					m_pSecondObject = CreateObject(pInfo, m_pPed->GetInventory()->GetWeaponAmmo(pInfo), mat, false, m_pPed, weaponModelHash, &interiorLocation, 1.0f, false, pWeaponItem->GetTintIndex());
					eAttachPoint otherAttachPoint = (attach == AP_RightHand) ? AP_LeftHand : AP_RightHand;
					AttachObject(otherAttachPoint, true);
				}

				// Attach the object to the ped - after its been added to the world
				AttachObject(attach);

				// Create any components
				fwModelId weaponModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(pWeaponItem->GetModelHash(), &weaponModelId);
				if(pWeaponItem->GetComponents() && m_pObject->GetWeapon())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();

					// Create the variant model component first, as this can affect creation of other components
					const CWeaponComponentVariantModelInfo* pVariantInfo = pWeaponItem->GetComponentInfo<CWeaponComponentVariantModelInfo>();
					if (pVariantInfo)
					{
						CreateWeaponComponent(pVariantInfo, m_pObject, eHierarchyId::WEAPON_ROOT);
					}
					
					// Create the rest of the components
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						if(components[i].pComponentInfo->GetClassId() != WCT_VariantModel)
						{
							// Some components share the same tint as the weapon
							u8 uTintIndex = components[i].pComponentInfo->GetApplyWeaponTint() ? pWeaponItem->GetTintIndex() : components[i].m_uTintIndex;

							CreateWeaponComponent(components[i].pComponentInfo, m_pObject, components[i].attachPoint, true, uTintIndex);
						}

						// Set the flash light active history (this history is cleared when swapping weapons manually, but persists when weapon object is temp. destroyed through other means)
						if(components[i].pComponentInfo->GetClassId() == WCT_FlashLight)
						{
							if(components[i].m_bActive && m_pObject->GetWeapon()->GetFlashLightComponent())
							{
								m_pObject->GetWeapon()->GetFlashLightComponent()->SetActiveHistory(true); 
							}
						}

						// Set the night vision / thermal scope active history (this history persists through all cases, including weapon swaps)
						if(components[i].pComponentInfo->GetClassId() == WCT_Scope)
						{
							if(m_pObject->GetWeapon()->GetScopeComponent())
							{
								m_pObject->GetWeapon()->GetScopeComponent()->SetActiveHistory(components[i].m_bActive); 
							}
						}
					}
				}

				if(m_pObject->GetWeapon())
				{
					// Update shader
					if(m_pObject->GetWeapon()->GetDrawableEntityTintIndex() != pWeaponItem->GetTintIndex())
					{
						weaponitemDebugf3("CPedEquippedWeapon::CreateObject--(m_pObject->GetWeapon()->GetDrawableEntityTintIndex()[%u] != pWeaponItem->GetTintIndex()[%u])-->invoke UpdateShaderVariables",m_pObject->GetWeapon()->GetDrawableEntityTintIndex(),pWeaponItem->GetTintIndex());
						m_pObject->GetWeapon()->UpdateShaderVariables(pWeaponItem->GetTintIndex());
					}

					// Update weapon idle
					fwMvClipSetId ClipSetId = pInfo->GetAppropriateWeaponClipSetId(m_pPed);
					bool bInVehicle = m_pPed->GetIsInVehicle();
					m_pObject->GetWeapon()->CreateWeaponIdleAnim(ClipSetId, bInVehicle);

					if(m_pPed->IsPlayer() && CNewHud::IsUsingWeaponWheel() && CNewHud::GetWheelDisableReload())
					{
						CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
						if(pWeapon && pWeapon->GetWeaponHash() == CNewHud::GetWheelWeaponInHand())
						{
							pWeapon->SetAmmoInClip(CNewHud::GetWheelHolsterAmmoInClip());
						}
						CNewHud::SetWheelDisableReload(false);
					}

					// B*1900737: Hack for 1-shot weapons. Force a reload if the flag has been set (in CTaskVault::StateClamberStand_OnEnter) by setting the clip ammo to 0. 
					// ! Unless we're using new ammo caching code; toggled using m_bUseAmmoCaching tunable in player info.
					if (m_pPed->GetWeaponManager() && m_pPed->GetWeaponManager()->GetForceReloadOnEquip() && m_pPed->GetPlayerInfo() && !m_pPed->GetPlayerInfo()->ms_Tunables.m_bUseAmmoCaching)
					{
						CWeapon* pWeapon = m_pPed->GetWeaponManager()->GetEquippedWeapon();
						if(pWeapon)
						{
							pWeapon->SetAmmoInClip(0);
						}
						m_pPed->GetWeaponManager()->SetForceReloadOnEquip(false);
					}
				}
			}
		}
	}

	return (m_pObject != NULL);
}

bool CPedEquippedWeapon::UseExistingObject(CObject& obj, eAttachPoint attach)
{
	if(!weaponVerifyf(!m_pObject, "Trying to use another object as a weapon, when there already is an object."))
	{
		return false;
	}

	const CWeaponInfo* pInfo = GetWeaponInfo();
	if(!weaponVerifyf(pInfo, "WeaponInfo is NULL"))
	{
		return false;
	}

	// Get the item
	weaponAssert(m_pPed->GetInventory());
	const CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon(pInfo->GetHash());
	if(!weaponVerifyf(pWeaponItem, "Weapon [%s] does not exist in inventory", pInfo->GetName()))
	{
		return false;
	}

	int iAmmo = 0;
	bool bCreateDefaultComponents = false;
	if(!SetupAsWeapon(&obj, pInfo, iAmmo, bCreateDefaultComponents, m_pPed))
	{
		return false;
	}

	m_pObject = &obj;
	AttachObject(attach);
	return true;
}


void CPedEquippedWeapon::CreateProjectileOrdnance()
{
	CreateProjectileOrdnance(GetObject(), m_pPed);
}

void CPedEquippedWeapon::DestroyObject()
{
	if(m_pObject)
	{
		// We can't destroy a mission object - it is managed by another system
		// Just detach procedural objects - as the procobj code is in charge of them
		if(m_pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT || m_pObject->GetIsAnyManagedProcFlagSet())
		{
			// Just detach to the world
			if(!m_pObject->GetCurrentPhysicsInst()->IsInLevel())
			{
				m_pObject->AddPhysics();
			}

			m_pObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);
		}
		else
		{
			// Can't delete anything from inside ProcessControl because it screws up the list
			// set it to be a temp object, and set it's timer so it'll get deleted next frame
			weaponFatalAssertf(m_pObject->GetRelatedDummy() == NULL, "Planning to remove an object but it has a dummy");

			// Mark the object for delete on next process
			DestroyObject(m_pObject, m_pPed);
		}
	}

	if(m_pSecondObject)
	{
		// We can't destroy a mission object - it is managed by another system
		// Just detach procedural objects - as the procobj code is in charge of them
		if(m_pSecondObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT || m_pSecondObject->GetIsAnyManagedProcFlagSet())
		{
			// Just detach to the world
			if(!m_pSecondObject->GetCurrentPhysicsInst()->IsInLevel())
			{
				m_pSecondObject->AddPhysics();
			}

			m_pSecondObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);
		}
		else
		{
			// Can't delete anything from inside ProcessControl because it screws up the list
			// set it to be a temp object, and set it's timer so it'll get deleted next frame
			weaponFatalAssertf(m_pSecondObject->GetRelatedDummy() == NULL, "Planning to remove an object but it has a dummy");

			// Mark the object for delete on next process
			DestroyObject(m_pSecondObject, m_pPed);
		}
	}

	m_pObject = NULL;
	m_pSecondObject = NULL;
}

void CPedEquippedWeapon::CreateUnarmedWeaponObject(u32 nWeaponHash)
{
	Assert( !m_pUnarmedWeapon );

	m_pUnarmedWeapon = CWeaponManager::CreateWeaponUnarmed(nWeaponHash);
	Assert( m_pUnarmedWeapon );
}

void CPedEquippedWeapon::CreateUnarmedKungFuWeaponObject(u32 nWeaponHash)
{
	Assert( !m_pUnarmedKungFuWeapon );

	m_pUnarmedKungFuWeapon = CWeaponManager::CreateWeaponUnarmed(nWeaponHash);
	Assert( m_pUnarmedKungFuWeapon );
}

CObject* CPedEquippedWeapon::ReleaseObject()
{
	// Cache the object, as we are NULLing our pointer to it and releasing ownership
	CObject* pReleasedObject = m_pObject;
	m_pObject = NULL;
	// Always destroy the secondary object if we're releasing the main one.
	if (m_pSecondObject)
	{
		DestroyObject(m_pSecondObject, m_pPed);
	}
	m_pSecondObject = NULL;
	return pReleasedObject;
}

void CPedEquippedWeapon::RequestWeaponAnims()
{
	if( !m_bWeaponAnimsRequested )
	{
		weaponFatalAssertf( m_pPed, "m_pPed is NULL" );
		if( m_pEquippedWeaponInfo )
		{
			weaponFatalAssertf( m_pPed->GetInventory(), "m_pPed inventory is null!" );
			CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon( m_pEquippedWeaponInfo->GetHash() );
			if( weaponVerifyf( pWeaponItem, "Weapon [%s] does not exist in inventory", m_pEquippedWeaponInfo->GetName() ) )
				pWeaponItem->RequestEquippedAnims(m_pPed);
		}

		m_bWeaponAnimsRequested = true;
		m_bWeaponAnimsStreamed = false;
	}
}

void CPedEquippedWeapon::ReleaseWeaponAnims()
{
	if( m_bWeaponAnimsRequested )
	{
		weaponFatalAssertf( m_pPed, "m_pPed is NULL" );
		if( m_pEquippedWeaponInfo )
		{
			weaponFatalAssertf( m_pPed->GetInventory(), "m_pPed inventory is null!" );
			CWeaponItem* pWeaponItem = m_pPed->GetInventory()->GetWeapon( m_pEquippedWeaponInfo->GetHash() );
			if( pWeaponItem )
				pWeaponItem->ReleaseEquippedAnims();
		}

		m_bWeaponAnimsRequested = false;
	}

	for(s32 i = 0; i < RHT_Count; i++)
	{
		for(s32 j = 0; j < FPS_StreamCount; j++)
		{
			m_ClipRequestHelpers[i][j].Release();
		}
	}
	m_WeaponMeleeStealthClipRequestHelper.Release();
}

void CPedEquippedWeapon::AttachObject(eAttachPoint attach, bool bUseSecondObject)
{
	weaponFatalAssertf(m_pObject, "m_pObject is NULL");

	CObject *pObject = m_pObject;

	if (bUseSecondObject && m_pSecondObject)
	{
		pObject = m_pSecondObject;
	}

	// Get the ped bone
	s32 iPedBoneIndex;
	switch(attach)
	{
	case AP_LeftHand:
		iPedBoneIndex = m_pPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND);
		break;

	case AP_RightHand:
	default:
		iPedBoneIndex = m_pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
		break;
	}

	// Get the object bone
	s32 iObjectBoneIndex = -1;
	CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo(pObject->GetModelIndex());
	if(pWeaponModelInfo)
	{
		iObjectBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_GRIP_R);
	}
#if !__NO_OUTPUT
	else
	{
		if (NetworkInterface::IsGameInProgress() && m_pPed && m_pPed->IsPlayer())
		{
			weaponitemDebugf2("CPedEquippedWeapon::AttachObject CreateObject !pWeaponModelInfo iObjectBoneIndex -1 -- could cause bad attachment -- player[%s] isplayer[%d] isnetworkclone[%d] ped[%s] weapon[%s]",
				m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
				m_pPed->IsPlayer(),
				m_pPed->IsNetworkClone(),
				m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "", 
				pObject->GetModelName());
		}
	}
#endif

	// Attach
	AttachObjects(m_pPed, pObject, iPedBoneIndex, iObjectBoneIndex);

	// Store the attach point
	m_AttachPoint = attach;
}

CObject* CPedEquippedWeapon::CreateObject(u32 uWeaponNameHash, s32 iAmmo, const Matrix34& mat, const bool bCreateDefaultComponents, CEntity* pOwner, const u32 uModelHashOverride, const fwInteriorLocation* pInteriorLocation, float fScale, bool bCreatedFromScript, u8 tintIndex)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
	if(weaponVerifyf(pWeaponInfo, "pWeaponInfo is NULL"))
	{
		return CreateObject(pWeaponInfo, iAmmo, mat, bCreateDefaultComponents, pOwner, uModelHashOverride, pInteriorLocation, fScale, bCreatedFromScript, tintIndex);
	}
	return NULL;
}

CObject* CPedEquippedWeapon::CreateObject(const CWeaponInfo* pWeaponInfo, s32 iAmmo, const Matrix34& mat, const bool bCreateDefaultComponents, CEntity* pOwner, const u32 uModelHashOverride, const fwInteriorLocation* pInteriorLocation, float fScale, bool bCreatedFromScript, u8 tintIndex)
{
	weaponFatalAssertf(pWeaponInfo, "pWeaponInfo is NULL");

	u32 uModelHash = uModelHashOverride != 0 ? uModelHashOverride : pWeaponInfo->GetModelHash();
	if(uModelHash != 0)
	{
		fwModelId weaponModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(uModelHash, &weaponModelId);

		CObject* pWeaponObject = CreateObject(pWeaponInfo, weaponModelId, mat, pOwner, pInteriorLocation, fScale, bCreatedFromScript);
		if( pWeaponObject )
		{
			if(!SetupAsWeapon(pWeaponObject, pWeaponInfo, iAmmo, bCreateDefaultComponents, pOwner, NULL, bCreatedFromScript, tintIndex))
			{
				// If we've failed to setup, delete it
				DestroyObject(pWeaponObject, pOwner);
				return NULL;
			}

			// Weapon object should be recorded by replay
			REPLAY_ONLY(CReplayMgr::RecordObject(pWeaponObject));

			return pWeaponObject;
		}
	}

	return NULL;
}

void CPedEquippedWeapon::DestroyObject(CObject* pObject, CEntity* pOwner)
{
	if(pOwner && pOwner->GetIsTypePed())
	{
		static_cast<CPed*>(pOwner)->SetUsingActionMode(false, CPed::AME_Equipment, -1.0f, true);
	}

	// Mark the object for delete on next process
 	pObject->DetachFromParent(0);
 	pObject->RemovePhysics();
 	pObject->DisableCollision();
	if( pObject->GetIsVisible() )
 		pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
	pObject->FlagToDestroyWhenNextProcessed();
}

void CPedEquippedWeapon::AttachObjects(CPhysical* pParent, CPhysical* pChild, s32 iAttachBoneParent, s32 iOffsetBoneChild)
{
	weaponFatalAssertf(pParent, "pParent is NULL");
	weaponFatalAssertf(pChild, "pChild is NULL");

	// Ensure we are detached
	pChild->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);

#if __ASSERT
	bool bSourceOrthonormal = true;
#endif // __ASSERT

	// Get the offsets
	Matrix34 mOffsetBone(Matrix34::IdentityType);
	if(iOffsetBoneChild != -1)
	{
		if(!pChild->GetSkeleton() && pChild->GetDrawable())
		{
			// Ensure the skeleton is created for animation/querying
			if(pChild->GetDrawable()->GetSkeletonData())
			{
				pChild->CreateSkeleton();
			}
		}
		if(pChild->GetSkeleton())
		{
			Mat34V mOffset;
			pChild->GetSkeletonData().ComputeGlobalTransform(iOffsetBoneChild, pChild->GetMatrix(), mOffset);
			mOffsetBone = MAT34V_TO_MATRIX34(mOffset);
		}
		else
		{
			// In case the child has no skeleton, make sure the position offset is correct when we transform this
			// matrix into model space.
			mOffsetBone.d = VEC3V_TO_VECTOR3(pChild->GetMatrix().d());
		}

		Matrix34 m = MAT34V_TO_MATRIX34(pChild->GetMatrix());
#if __ASSERT
		bSourceOrthonormal = m.IsOrthonormal();
		weaponAssertf(m.IsOrthonormal(), "result of pChild->GetMatrix() is non-orthonormal");
#endif // __ASSERT
		mOffsetBone.DotTranspose(m);
		mOffsetBone.FastInverse();
#if __ASSERT
 		bSourceOrthonormal = bSourceOrthonormal && mOffsetBone.IsOrthonormal();
 		weaponAssertf(mOffsetBone.IsOrthonormal(), "result of DotTranspose and FastInverse is non-orthonormal, source matrix was %s", bSourceOrthonormal ? "Correct" : "Wrong");
#endif // __ASSERT
	}

	Quaternion q;
	mOffsetBone.ToQuaternion(q);
	q.Normalize();

#if __ASSERT
	weaponAssertf(q.Mag2() >= square(0.999f) && q.Mag2() <= square(1.001f),"Quaternion <%f, %f, %f, %f> is non-orthonormal (%f), source matrix was %s", q.x, q.y, q.z,q.w, q.Mag2(), bSourceOrthonormal ? "Correct" : "Wrong");
#endif //__ASSERT

	//B*1800517: Hard-coded offset for female peds
	if (pParent->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pParent);
		// B*2269590: Only do this offset for peds using female skeleton.
		if (pPed && !pPed->IsMale() && pPed->GetPedModelInfo() && pPed->GetPedModelInfo()->IsUsingFemaleSkeleton())
		{
			const CWeaponInfo *pWeaponInfo= pPed->GetEquippedWeaponInfo();
			if (pWeaponInfo)
			{
				//Do specific check for melee weapon (so we don't mess with coffee cups etc)
				if (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsMeleeFist())
				{
					mOffsetBone.d.x += -0.025f;
				}
				else if(pWeaponInfo->GetIsGun())
				{
					// B*2434430: Musket-specific reload attachment offset when attaching to left hand.
					if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_MUSKET", 0xa89cb99e) && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading) && pPed->GetBoneTagFromBoneIndex(iAttachBoneParent) == BONETAG_L_PH_HAND)
					{
						TUNE_GROUP_FLOAT(WEAPON_ATTACH_OFFSET, fMusketReloadOffsetX, -0.015f, -1.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(WEAPON_ATTACH_OFFSET, fMusketReloadOffsetZ, -0.015f, -1.0f, 1.0f, 0.01f);
						mOffsetBone.d.x += fMusketReloadOffsetX;
						mOffsetBone.d.z += fMusketReloadOffsetZ;

					}
					else
					{
						// B*2346868: Gun-specific offset. Also added same offset in CPed::ProcessLeftHandGripIk to keep weapon aligned properly.
						TUNE_GROUP_FLOAT(WEAPON_ATTACH_OFFSET, fOffsetX, -0.015f, -1.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(WEAPON_ATTACH_OFFSET, fOffsetY, -0.010f, -1.0f, 1.0f, 0.01f);
						mOffsetBone.d.x += fOffsetX;
						mOffsetBone.d.y += fOffsetY;
					}
				}
			}
		}
	}

	// Attach the child to the parent, using the inverse of the offset bone as the attachment offset
	pChild->AttachToPhysicalBasic(pParent, s16(iAttachBoneParent), ATTACH_STATE_BASIC|ATTACH_FLAG_DELETE_WITH_PARENT, &mOffsetBone.d, &q);

	// Copy the parent visibility
	pChild->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, pParent->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY), true);
}

bool CPedEquippedWeapon::CreateWeaponComponent(u32 uComponentNameHash, CObject* pWeaponObject, const bool bDoReload, u8 uTintIndex)
{
	if(weaponVerifyf(pWeaponObject, "pWeaponObject is NULL"))
	{
		if(weaponVerifyf(pWeaponObject->GetWeapon(), "pWeaponObject has no weapon"))
		{
			// Get the weapon component info
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uComponentNameHash);
			if(weaponVerifyf(pComponentInfo, "pComponentInfo is NULL"))
			{
				const CWeaponComponentPoint* pAttachPoint = pWeaponObject->GetWeapon()->GetWeaponInfo()->GetAttachPoint(pComponentInfo);
				if(weaponVerifyf(pAttachPoint, "Weapon component [%s] doesn't fit on weapon [%s]", pComponentInfo->GetName(), pWeaponObject->GetWeapon()->GetWeaponInfo()->GetName()))
				{
					return CreateWeaponComponent(pComponentInfo, pWeaponObject, pAttachPoint->GetAttachBoneId(), bDoReload, uTintIndex);
				}
			}
		}
	}
	return false;
}

bool CPedEquippedWeapon::CreateWeaponComponent(u32 uComponentNameHash, CObject* pWeaponObject, eHierarchyId attachPoint, const bool bDoReload, u8 uTintIndex)
{
	// Get the weapon component info
	const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uComponentNameHash);
	if(weaponVerifyf(pComponentInfo, "pComponentInfo is NULL"))
	{
		return CreateWeaponComponent(pComponentInfo, pWeaponObject, attachPoint, bDoReload, uTintIndex);
	}
	return false;
}

bool CPedEquippedWeapon::CreateWeaponComponent(const CWeaponComponentInfo* pComponentInfo, CObject* pWeaponObject, eHierarchyId attachPoint, const bool bDoReload, u8 uTintIndex)
{
	weaponAssertf(pWeaponObject, "pWeaponObject is NULL");
	weaponAssertf(pWeaponObject->GetWeapon(), "pWeaponObject has no weapon");
	weaponAssertf(pComponentInfo, "pComponentInfo is NULL");

	// Create the component model
	CObject* pComponentObject = NULL;
	if(pComponentInfo->GetModelHash() != 0 && pComponentInfo->GetCreateObject())
	{
		// Check for variant component model to use instead of base component model
		u32 componentModelHash = pComponentInfo->GetModelHash();

		const CWeaponComponentVariantModel* pVariantComponent = pWeaponObject->GetWeapon() ? pWeaponObject->GetWeapon()->GetVariantModelComponent() : NULL;
		if (pVariantComponent)
		{
			const CWeaponComponentVariantModelInfo* pVariantInfo = pVariantComponent->GetInfo();
			u32 variantModelHash = pVariantInfo->GetExtraComponentModel(pComponentInfo->GetHash());
			if (variantModelHash != 0)
			{
				componentModelHash = variantModelHash;
			}
		}

		// Check if the model is loaded
		fwModelId componentModelId;
		const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(componentModelHash, &componentModelId);
		if(weaponVerifyf(pModelInfo, "pModelInfo for model [%s] is NULL", pComponentInfo->GetModelName()))
		{
			if(weaponVerifyf(pModelInfo->GetHasLoaded(), "pModelInfo for model [%s] is not loaded for weapon [%s]", pModelInfo->GetModelName(), pWeaponObject->GetDebugName()))
			{
				// Create the component model
				fwInteriorLocation interiorLocation = pWeaponObject->GetInteriorLocation().IsValid() ? pWeaponObject->GetInteriorLocation() : CGameWorld::OUTSIDE;
				Matrix34 mat = MAT34V_TO_MATRIX34(pWeaponObject->GetTransform().GetMatrix());
				pComponentObject = CreateObject(NULL, componentModelId, mat, NULL, &interiorLocation);
				if(weaponVerifyf(pComponentObject, "Failed to create object with model [%s] for component [%s]", CModelInfo::GetBaseModelInfoName(componentModelId), pComponentInfo->GetName()))
				{
					const CWeaponModelInfo* pWeaponObjectModelInfo = GetWeaponModelInfo(pWeaponObject->GetModelIndex());
					if(weaponVerifyf(pWeaponObjectModelInfo, "Object is not a weapon, model [%s] for weapon [%s]", pComponentInfo->GetModelName(), pWeaponObject->GetDebugName()))
					{
						s32 iAttachBone = pWeaponObjectModelInfo->GetBoneIndex(attachPoint);
						s32 iOffsetBone = pComponentInfo->GetAttachBone().GetBoneIndex(pComponentObject->GetModelIndex());
						if(weaponVerifyf(iAttachBone != -1 && iOffsetBone != -1, "iAttachBone [%s][%d], iOffsetBone [%s][%d], invalid", pWeaponObjectModelInfo->GetModelName(), iAttachBone, pComponentInfo->GetModelName(), iOffsetBone))
						{
							// Ensure nothing is attached to our attach point
							const CWeapon::Components& components = pWeaponObject->GetWeapon()->GetComponents();
							for(s32 i = 0; i < components.GetCount(); i++)
							{
								if (components[i]->GetDrawable())
								{
									const fwAttachmentEntityExtension* pAttachExt = components[i]->GetDrawable()->GetAttachmentExtension();
									if(pAttachExt)
									{
										if(pAttachExt->GetOtherAttachBone() == iAttachBone)
										{
											DestroyWeaponComponent(pWeaponObject, components[i]);
										}
									}
								}
							}

							// Scale its components accordingly if the weapon is scaled
							if(pWeaponObject->GetTransform().IsMatrixScaledTransform())
							{
								fwMatrixScaledTransform* pWeaponTransform = (fwMatrixScaledTransform*)pWeaponObject->GetTransformPtr(); 
								#if ENABLE_MATRIX_MEMBER
								Mat34V trans = pComponentObject->GetMatrix();
								trans.GetCol1Ref().SetWf(pWeaponTransform->GetScaleXY());
								trans.GetCol2Ref().SetWf(pWeaponTransform->GetScaleZ());
								pComponentObject->SetTransform(trans);
								#else
								fwMatrixScaledTransform* trans = rage_new fwMatrixScaledTransform(pComponentObject->GetMatrix());
								trans->SetScale(pWeaponTransform->GetScaleXY(), pWeaponTransform->GetScaleZ());
								pComponentObject->SetTransform(trans);
								#endif
							}

							// Attach after adding to world
							AttachObjects(pWeaponObject, pComponentObject, iAttachBone, iOffsetBone);
						}
						else
						{
							// Mark the object for delete on next process
							DestroyObject(pComponentObject, NULL);
							pComponentObject = NULL;
						}
					}
				}
			}
		}
	}

	CWeaponComponent* pComponent = WeaponComponentFactory::Create(pComponentInfo, pWeaponObject->GetWeapon(), pComponentObject);
	if(pComponent)
	{
		pComponent->UpdateShaderVariables(uTintIndex);
		pWeaponObject->GetWeapon()->AddComponent(pComponent, bDoReload);
		
		// Weapon object should be recorded by replay
		REPLAY_ONLY(CReplayMgr::RecordObject(pComponentObject));
		return true;
	}
	else if(pComponentObject)
	{
		// Mark the object for delete on next process
		DestroyObject(pComponentObject, NULL);
		pComponentObject = NULL;
	}
	
	return false;
}

void CPedEquippedWeapon::DestroyWeaponComponent(CObject* pWeaponObject, CWeaponComponent* pComponentToDestroy)
{
	weaponAssert( pComponentToDestroy );

	CObject* pComponentObject = (CObject*)pComponentToDestroy->GetDrawable();

	// Remove the component from the weapon
	ReleaseWeaponComponent( pWeaponObject, pComponentToDestroy );

	if (pComponentObject)
	{
		// Mark this drawable for delete on next process
		DestroyObject( pComponentObject, NULL );
	}
}

void CPedEquippedWeapon::ReleaseWeaponComponent(CObject* pWeaponObject, CWeaponComponent* pComponentToRelease)
{
	weaponAssert( pWeaponObject );
	weaponAssert( pWeaponObject->GetWeapon() );
	weaponAssert( pComponentToRelease );

	// Detach the component from the current weapon
	pWeaponObject->GetWeapon()->ReleaseComponent( pComponentToRelease );

	CObject* pComponentObject = (CObject*)pComponentToRelease->GetDrawable();

	if (pComponentObject)
	{
		// Release the component to physics
		pComponentObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY|DETACH_FLAG_USE_ROOT_PARENT_VELOCITY);

		if (phCollider* pCollider = pComponentObject->GetCollider())
		{
			pCollider->DisablePushCollisions();
		}

		// make sure game objects get deleted
		// don't delete frag instances or non game objects
		if (pComponentObject->GetIsAnyManagedProcFlagSet())
		{
			// don't do anything with procedural objects - the procobj code needs to be in charge of them
		}
		else if(pComponentObject->GetOwnedBy() == ENTITY_OWNEDBY_GAME && (!pComponentObject->GetCurrentPhysicsInst() || pComponentObject->GetCurrentPhysicsInst()->GetClassType() != PH_INST_FRAG_CACHE_OBJECT))
		{
			weaponAssertf(pComponentObject->GetRelatedDummy() == NULL, "Planning to remove a weapon component object but it has a dummy");

			pComponentObject->SetOwnedBy(ENTITY_OWNEDBY_TEMP);

			// Temp objects stick around for 30000 milliseconds. Reduce so we only stick around for 10000
			static dev_u32 LIFE_TIMER_REDUCTION = 20000;
			pComponentObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() - LIFE_TIMER_REDUCTION;
			pComponentObject->m_nObjectFlags.bSkipTempObjectRemovalChecks = true;
		}

		pComponentObject->m_nFlags.bAddtoMotionBlurMask = false;
	}

	// Release the CWeaponComponent from the pool
	delete pComponentToRelease;	
}

void CPedEquippedWeapon::CreateProjectileOrdnance(CObject* pWeaponObject, CEntity* pOwner, bool bReloading, bool bCreatedFromScript)
{
	if( pWeaponObject && !pWeaponObject->GetChildAttachment() )
	{
		CWeapon* pWeapon = pWeaponObject->GetWeapon();
		if( pWeapon )
		{
			bool bCanCreateOrdnance = pWeapon->GetCanReload();
			if(!bReloading && pWeapon->GetAmmoTotal() > 0 )
			{
				bCanCreateOrdnance = true;
			}
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(bCanCreateOrdnance && pWeaponInfo && pWeaponInfo->GetCreateVisibleOrdnance())
			{
				const CWeaponModelInfo* pWeaponObjectModelInfo = pWeapon->GetWeaponModelInfo();
				if(weaponVerifyf(pWeaponObjectModelInfo, "Object is not a weapon") && pWeaponObjectModelInfo->GetBoneIndex(WEAPON_PROJECTILE) != -1)
				{
					const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
					if(pAmmoInfo && pAmmoInfo->GetModelHash() != 0)
					{
						CProjectile* pProjectile = CProjectileManager::CreateProjectile(pAmmoInfo->GetHash(), pWeaponInfo->GetHash(), pOwner, MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix()), pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup(), NULL, NULL, pWeapon->GetTintIndex(), bCreatedFromScript);
						if(pProjectile)
						{
							pProjectile->SetOrdnance(true);

							s32 iProjectileBoneIndex = -1;
							const CBaseModelInfo* pProjectileModelInfo = pProjectile->GetBaseModelInfo();
							if(pProjectileModelInfo && pProjectileModelInfo->GetModelType() == MI_TYPE_WEAPON)
							{
								weaponAssertf(pProjectileModelInfo->GetDrawable(), "The projectile ordnance has not been streamed in. It won't attach to the weapon properly.");
								iProjectileBoneIndex = static_cast<const CWeaponModelInfo*>(pProjectileModelInfo)->GetBoneIndex(WEAPON_GRIP_R);
							}

							CPedEquippedWeapon::AttachObjects(pWeaponObject, pProjectile, pWeaponObjectModelInfo->GetBoneIndex(WEAPON_PROJECTILE), iProjectileBoneIndex);
						}
					}
				}
			}		
		}
	}
}

bool CPedEquippedWeapon::SetupAsWeapon(CObject* pWeaponObject, const CWeaponInfo* pWeaponInfo, s32 iAmmo, const bool bCreateDefaultComponents, CEntity* pOwner, CWeapon* pWeapon, bool bCreatedFromScript, u8 tintIndex)
{
	weaponFatalAssertf(pWeaponObject, "pWeaponObject is NULL");

	// Setup the weapon
	pWeaponObject->SetWeapon(pWeapon ? pWeapon : WeaponFactory::Create(pWeaponInfo->GetHash(), iAmmo, pWeaponObject, "CPedEquippedWeapon::SetupAsWeapon", tintIndex));

	// Setup the weapon
	if(pOwner && pOwner->GetIsTypePed() && pWeaponObject->GetWeapon())
	{
		CPedInventory* pInventory = static_cast<CPed*>(pOwner)->GetInventory();
		pWeaponObject->GetWeapon()->SetObserver(pInventory);

		if(pInventory)
		{
			if (const CWeaponItem* pWeaponItem = pInventory->GetWeapon(pWeaponInfo->GetHash()))
			{
				const f32 fTimeBeforeNextShot = (static_cast<f32>(pWeaponItem->GetTimer()) - static_cast<f32>(fwTimer::GetTimeInMilliseconds())) / 1000.0f;

				// Reset stungun_MP to use cached fire time and state so we continue the recharge
				if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3) && fTimeBeforeNextShot > 0.0f)
				{
					pWeaponObject->GetWeapon()->SetTimer(pWeaponItem->GetTimer());
					pWeaponObject->GetWeapon()->SetState((CWeapon::State)pWeaponItem->GetCachedWeaponState());
					pWeaponObject->GetWeapon()->SetWeaponRechargeAnimation();
				}
				else
				{
					pWeaponObject->GetWeapon()->SetTimer(pWeaponItem->GetTimer());
				}
			}
		}

		if(pWeaponInfo->GetForcesActionMode())
		{
			static_cast<CPed*>(pOwner)->SetUsingActionMode(true, CPed::AME_Equipment);
		}
	}

	// Attempt to create a projectile attached to the projectile bone
	if(pWeaponInfo->GetCreateVisibleOrdnance())
	{
		// Don't create the ordnance if we're about to do a forced reload! CreateProjectileOrdnance() will get called through the reload task instead.
		bool bNeedsToReload = false;
		if (pOwner && pOwner->GetIsTypePed())
		{
			CPed *pPed = static_cast<CPed*>(pOwner);
			if (pPed && pPed->IsLocalPlayer())
			{
				if (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->ms_Tunables.m_bUseAmmoCaching)
				{
					CWeaponItem* pWeaponItem = pPed->GetInventory() ? pPed->GetInventory()->GetWeapon(pWeaponInfo->GetHash()) : NULL;
					s32 iRestoredAmmoInClip = pWeaponItem ? pWeaponItem->GetLastAmmoInClip() : -1;
					bNeedsToReload = (iRestoredAmmoInClip != -1) && (iRestoredAmmoInClip == 0);
				}
				else
				{
					// ! Only use "force reload" code if not using new ammo caching code.
					bNeedsToReload = pPed && pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetForceReloadOnEquip() : false;
				}
			}
		}

		if((!pOwner || !pOwner->GetIsTypePed() || !static_cast<CPed*>(pOwner)->IsPlayer() || !CNewHud::IsUsingWeaponWheel() || !CNewHud::GetWheelDisableReload() || CNewHud::GetWheelHolsterAmmoInClip() != 0) && !bNeedsToReload)
		{
			CreateProjectileOrdnance(pWeaponObject, pOwner, false, bCreatedFromScript);
		}
	}

	// Create any default components
	if(bCreateDefaultComponents)
	{
		const CWeaponInfo::AttachPoints& attachPoints = pWeaponInfo->GetAttachPoints();
		for(s32 i = 0; i < attachPoints.GetCount(); i++)
		{
			const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents();
			for(int j = 0; j < components.GetCount(); j++)
			{
				if(components[j].m_Default)
				{
					if(!CreateWeaponComponent(components[j].m_Name.GetHash(), pWeaponObject, attachPoints[i].GetAttachBoneId()))
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

CObject* CPedEquippedWeapon::CreateObject(const CWeaponInfo* pWeaponInfo, const fwModelId& modelId, const Matrix34& mat, CEntity* pOwner, const fwInteriorLocation* pInteriorLocation, float fScale, bool bCreatedFromScript)
{
	if(CModelInfo::IsValidModelInfo(modelId.GetModelIndex()))
	{
		// Get the model info
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		if(weaponVerifyf(pModelInfo, "pModelInfo [%d] is NULL", modelId.GetModelIndex()))
		{
			if(weaponVerifyf(pModelInfo->GetHasLoaded(), "pModelInfo [%s] is not loaded", pModelInfo->GetModelName()))
			{
				pModelInfo->SetUseAmbientScale(true);

				// Create the object
				CObject* pObject;
				if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon() && pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE)
				{
					pObject = ProjectileFactory::Create(pWeaponInfo->GetAmmoInfo()->GetHash(), pWeaponInfo->GetHash(), pOwner, pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup(), NULL, NULL, bCreatedFromScript);
				}
				else
				{
					pObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_GAME, true, true, false, -1, pOwner?false:true);
					//set the objects Ambient Scales to the same as it's owners
					if(pOwner && pObject)
					{
							pObject->SetNaturalAmbientScale(pOwner->GetNaturalAmbientScale());
							pObject->SetArtificialAmbientScale(pOwner->GetArtificialAmbientScale());
							pObject->SetCalculateAmbientScaleFlag(true);
					}
				}

				if ( pObject )
				{
					// In order to avoid phLevelNew::AddObjectToOctree artAssert we need to slightly
					// fudge the initial position. When multiple weapons are streamed in and a script
					// switches back and forth between them, it will attempt to create and destroy 
					// weapons and their components within the same frame. Being that it takes longer than
					// a single frame to actually delete, the same object will exist in the same location.
					Matrix34 tempMat = mat;
					float fJitterFactor = fwRandom::GetRandomNumberInRange(-0.05f, 0.05f);
					tempMat.d += Vector3(fJitterFactor, fJitterFactor, fJitterFactor);
					pObject->SetMatrix(tempMat);

					// Don't fade in
					pObject->GetLodData().SetResetDisabled(true);

					// Add to world
					CGameWorld::Add(pObject, pInteriorLocation && pInteriorLocation->IsValid() ? *pInteriorLocation : CGameWorld::OUTSIDE);

					// Update portal tracker
					if(pObject->GetPortalTracker())
					{
						pObject->GetPortalTracker()->SetLoadsCollisions(false);
						pObject->GetPortalTracker()->RequestRescanNextUpdate();
						pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));
					}

					pObject->ScaleObject(fScale);

					// Success
					return pObject;
				}
			}
		}
	}

	return NULL;
}
