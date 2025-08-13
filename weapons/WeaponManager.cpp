// File header
#include "Weapons/WeaponManager.h"

// Game headers
#include "Peds/PedDefines.h"	// For MAXNOOFPEDS
#include "Task/Weapons/WeaponController.h"
#include "Weapons\AirDefence.h"
#include "Weapons/Bullet.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/FiringPattern.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Inventory/InventoryItem.h"
#include "Weapons/Inventory/PedInventoryLoadOut.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/ThrownWeaponInfo.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponDebug.h"
#include "Weapons/WeaponScriptResource.h"
#include "Weapons/WeaponTypes.h"
#include "Weapons/WeaponFactory.h"

//////////////////////////////////////////////////////////////////////////
// CWeaponManager
//////////////////////////////////////////////////////////////////////////

#include "data/aes_init.h"
AES_INIT_F;

// Statics
atArray<CWeapon*> CWeaponManager::ms_pUnarmedWeapons;
bool CWeaponManager::ms_bRenderLaserSight = true;

void CWeaponManager::Init(unsigned /*initMode*/)
{
	// This used to be CWeapon::MAX_STORAGE:
	//const s32 maxStorage = MAXNOOFPEDS;
	const int maxStorage = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "CWeapon", 0x3A2BC128 ), CONFIGURED_FROM_FILE );
	CWeapon::InitPool(maxStorage, MEMBUCKET_GAMEPLAY);
	CWeaponComponentManager::Init();
	CWeaponAnimationsManager::Init();
	CWeaponInfoManager::Init();
	CThrownWeaponInfo::Init();
	CProjectileManager::Init();
	CInventoryItem::Init();
	CBulletManager::Init();
	CFiringPatternInfoManager::Init();
	CPedInventoryLoadOutManager::Init();
	CWeaponScriptResourceManager::Init();
	CWeaponControllerManager::Init();

#if __BANK
	CWeaponDebug::InitBank();
#endif // __BANK
}

void CWeaponManager::Shutdown(unsigned /*shutdownMode*/)
{
	// Delete the unarmed weapon
	DestroyWeaponUnarmed();

#if __BANK
	CWeaponDebug::ShutdownBank();
#endif // __BANK

	CWeaponControllerManager::Shutdown();
	CWeaponScriptResourceManager::Shutdown();
	CPedInventoryLoadOutManager::Shutdown();
	CFiringPatternInfoManager::Shutdown();
	CBulletManager::Shutdown();
	CInventoryItem::Shutdown();
	CProjectileManager::Shutdown();
	CThrownWeaponInfo::Shutdown();
	CWeaponInfoManager::Shutdown();
	CWeaponAnimationsManager::Shutdown();
	CWeaponComponentManager::Shutdown();
	CWeapon::ShutdownPool();
}

void CWeaponManager::Process()
{
	if(!fwTimer::IsGamePaused())
	{
		CProjectileManager::Process();
		CBulletManager::Process();
#if __BANK
		CAirDefenceManager::RenderDebug();
#endif	// __BANK
	}
}

CWeapon* CWeaponManager::CreateWeaponUnarmed(u32 nWeaponHash)
{
	CWeapon* pUnarmedWeapon = NULL;
	if(CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash))
	{
		pUnarmedWeapon = GetWeaponUnarmed(nWeaponHash);
		if(!pUnarmedWeapon)
		{
			pUnarmedWeapon = WeaponFactory::Create(nWeaponHash, CWeapon::INFINITE_AMMO, NULL, "CWeaponManager::CreateWeaponUnarmed" );
			Assert(pUnarmedWeapon);
			ms_pUnarmedWeapons.PushAndGrow(pUnarmedWeapon, 1);
		}

//#if !__FINAL
//		if (NetworkInterface::IsGameInProgress())
//			weaponDisplayf("CWeaponManager::CreateWeaponUnarmed rage_new CWeapon GetNoOfUsedSpaces[%d]/GetSize[%d]",CWeapon::GetPool()->GetNoOfUsedSpaces(),CWeapon::GetPool()->GetSize());
//#endif
	}

	return pUnarmedWeapon;
}

void CWeaponManager::DestroyWeaponUnarmed()
{
	// Delete the unarmed weapons
	for(int i = 0; i < ms_pUnarmedWeapons.GetCount(); i++)
	{
		if(ms_pUnarmedWeapons[i])
		{
			delete ms_pUnarmedWeapons[i];
			ms_pUnarmedWeapons[i] = NULL;
		}
	}
	ms_pUnarmedWeapons.Reset();
}

CWeapon* CWeaponManager::GetWeaponUnarmed(u32 uWeaponHash)
{
	for(int i = 0; i < ms_pUnarmedWeapons.GetCount(); i++)
	{
		if(ms_pUnarmedWeapons[i]->GetWeaponHash() == uWeaponHash)
		{
			return ms_pUnarmedWeapons[i];
		}
	}

	return NULL;
}
