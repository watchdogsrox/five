#ifndef PROJECTILE_THROWN_H
#define PROJECTILE_THROWN_H

// Game headers
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Projectiles/Projectile.h"

//////////////////////////////////////////////////////////////////////////
// CProjectileThrown
//////////////////////////////////////////////////////////////////////////

class CProjectileThrown : public CProjectile
{
public:

	// Construction
	CProjectileThrown(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript = false);

	// Destruction
	virtual ~CProjectileThrown();

	//
	// Types
	//

	virtual const CProjectileThrown* GetAsProjectileThrown() const { return this; }
	virtual CProjectileThrown* GetAsProjectileThrown() { return this; }

protected:

	// Get the data
	const CAmmoThrownInfo* GetInfo() const { return static_cast<const CAmmoThrownInfo*>(CProjectile::GetInfo()); }
};


#endif // PROJECTILE_THROWN_H
