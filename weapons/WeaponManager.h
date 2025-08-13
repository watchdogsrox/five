#ifndef WEAPON_MANAGER_H
#define WEAPON_MANAGER_H

#include "Weapons/WeaponTypes.h"

#include "atl/array.h"

// Forward declarations
class CWeapon;

//////////////////////////////////////////////////////////////////////////
// CWeaponManager
//////////////////////////////////////////////////////////////////////////

class CWeaponManager
{
public:

	// Initialise
	static void Init(unsigned initMode);

	// Shutdown
	static void Shutdown(unsigned shutdownMode);

	// Process
	static void Process();

	// Access weapon unarmed
	static CWeapon* GetWeaponUnarmed(u32 uWeaponHash = WEAPONTYPE_UNARMED);

	// Create weapon unarmed
	static CWeapon* CreateWeaponUnarmed(u32 nWeaponHash);
	static void EnableLaserSightRendering( bool bEnabled ) { ms_bRenderLaserSight = bEnabled; }
	static bool ShouldRenderLaserSight() { return ms_bRenderLaserSight; }

private:

	// Delete weapon unarmed
	static void DestroyWeaponUnarmed();

	// Global weapon unarmed - stored here so it is created after the weapon info is loaded
	// Should get rid of this and create unarmed weapons when unarmed is selected
	static atArray<CWeapon*> ms_pUnarmedWeapons;
	static bool ms_bRenderLaserSight;
};

#endif // WEAPON_MANAGER_H
