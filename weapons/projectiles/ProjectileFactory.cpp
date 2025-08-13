// File header
#include "Weapons/Projectiles/ProjectileFactory.h"

// Game headers
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Projectiles/ProjectileRocket.h"
#include "Weapons/Projectiles/ProjectileThrown.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// ProjectileFactory
//////////////////////////////////////////////////////////////////////////

namespace ProjectileFactory
{
	
CProjectile* Create(u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript)
{
#if __DEV
	// Have to set this flag to ensure objects are only created through the appropriate methods
	CProjectile::bInObjectCreate = true;
#endif // __DEV

	CProjectile* pProjectile = NULL;

	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(uProjectileNameHash);
	if(pInfo)
	{
		if(pInfo->GetClassId() == CAmmoProjectileInfo::GetStaticClassId())
		{
			pProjectile = rage_checked_pool_new(CProjectile) (ENTITY_OWNEDBY_RANDOM, uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pNetIdentifier, bCreatedFromScript);
#if USE_PROJECTION_EDGE_FILTERING
			if(uProjectileNameHash == AMMOTYPE_GRENADELAUNCHER || uProjectileNameHash == AMMOTYPE_EMPLAUNCHER)
			{
				//Add bound hackery here
				if (pProjectile)
				{
					rage::phInst * inst = pProjectile->GetCurrentPhysicsInst();
					if (inst)
					{
						rage::phArchetype * arch = inst->GetArchetype();
						if (arch)
						{
							rage::phBound * bound = arch->GetBound();
							if (bound)
							{
								bound->EnableProjectionEdgeFiltering();
								if (bound->GetType() == rage::phBound::COMPOSITE)
								{
									rage::phBoundComposite * boundComposite = reinterpret_cast<rage::phBoundComposite*>(bound);
									for (int i = 0 ; i < boundComposite->GetNumBounds() ; i++)
									{
										rage::phBound * boundPart = boundComposite->GetBound(i);
										if (boundPart)
											boundPart->EnableProjectionEdgeFiltering();
									}
								}
							}
						}
					}
				}
			}
#endif // USE_PROJECTION_EDGE_FILTERING
		}
		else if(pInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId())
		{
			pProjectile = rage_checked_pool_new(CProjectileRocket) (ENTITY_OWNEDBY_RANDOM, uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pTarget, pNetIdentifier, bCreatedFromScript);
		}
		else if(pInfo->GetClassId() == CAmmoThrownInfo::GetStaticClassId())
		{
			pProjectile = rage_checked_pool_new(CProjectileThrown) (ENTITY_OWNEDBY_RANDOM, uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pNetIdentifier, bCreatedFromScript);
		}
	}

#if __DEV
	CProjectile::bInObjectCreate = false;
#endif // __DEV

	if(pProjectile && !pProjectile->GetCurrentPhysicsInst())
	{
		pProjectile->Destroy();
		pProjectile = NULL;
	}

	return pProjectile;
}

} // namespace ProjectileFactory
