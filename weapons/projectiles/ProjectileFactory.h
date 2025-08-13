#ifndef PROJECTILE_FACTORY_H
#define PROJECTILE_FACTORY_H

// Game headers
#include "Weapons/WeaponEnums.h"

// Forward declarations
class CEntity;
class CProjectile;
class CNetFXIdentifier;

//////////////////////////////////////////////////////////////////////////
// ProjectileFactory
//////////////////////////////////////////////////////////////////////////

namespace ProjectileFactory
{
	CProjectile* Create(u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript = false);
}

#endif // PROJECTILE_FACTORY_H
