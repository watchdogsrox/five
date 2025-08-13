// File header
#include "Weapons/Projectiles/ProjectileThrown.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CProjectileThrown
//////////////////////////////////////////////////////////////////////////

CProjectileThrown::CProjectileThrown(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript)
: CProjectile(ownedBy, uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pNetIdentifier, bCreatedFromScript)
{
}

CProjectileThrown::~CProjectileThrown()
{
}
