#ifndef PICKUP_REWARDS_H
#define PICKUP_REWARDS_H

// Game headers
#include "Pickups/Data/RewardData.h"

class CPickup;
class CPed;
class CWeaponInfo;

//////////////////////////////////////////////////////////////////////////
// CPickupRewardAmmo
//////////////////////////////////////////////////////////////////////////

class CPickupRewardAmmo : public CPickupRewardData
{
public:
	CPickupRewardAmmo( ) : CPickupRewardData(), AmmoRef(), Amount(1) {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	bool PreventCollectionIfCantGive(const CPickup*, const CPed*) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_AMMO; }
	s32 GetAmmoAmountRewarded(const CPickup* pPickup, const CPed* pPed) const;
	s32 GetAmount() const { return Amount; }
	bool CanGiveToPassengers(const CPickup*) const { return true; }

protected:
	rage::atHashString AmmoRef;
	rage::s32 Amount;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardAmmoMP
//////////////////////////////////////////////////////////////////////////

class CPickupRewardBulletMP : public CPickupRewardData
{
public:
	CPickupRewardBulletMP( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_BULLET_MP; }
	const CPickupRewardData* GetAmmoReward(const CPed* pPed) const;

	static const CWeaponInfo* GetRewardedWeaponInfo(const CPed* pPed, bool bIgnoreProjectileInWater = false);

protected:

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardAmmoMP
//////////////////////////////////////////////////////////////////////////

class CPickupRewardMissileMP : public CPickupRewardData
{
public:
	CPickupRewardMissileMP( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_MISSILE_MP; }

protected:

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardFireworkMP
//////////////////////////////////////////////////////////////////////////

class CPickupRewardFireworkMP : public CPickupRewardData
{
public:
	CPickupRewardFireworkMP( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_FIREWORK_MP; }

protected:

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardGrenadeLauncherMP
//////////////////////////////////////////////////////////////////////////

class CPickupRewardGrenadeLauncherMP : public CPickupRewardData
{
public:
	CPickupRewardGrenadeLauncherMP( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_GRENADE_LAUNCHER_MP; }

protected:

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardArmour
//////////////////////////////////////////////////////////////////////////

class CPickupRewardArmour : public CPickupRewardData
{
public:
	CPickupRewardArmour( ) : CPickupRewardData(), Armour(0) {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_ARMOUR; }

protected:
	rage::s32 Armour;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardHealth
//////////////////////////////////////////////////////////////////////////

class CPickupRewardHealth : public CPickupRewardData
{
public:
	CPickupRewardHealth( ) : CPickupRewardData(), Health(0) {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_HEALTH; }

protected:
	rage::s32 Health;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardHealthVariable
//////////////////////////////////////////////////////////////////////////

class CPickupRewardHealthVariable : public CPickupRewardData
{
public:
	CPickupRewardHealthVariable( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_HEALTH; }

protected:

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardMoneyVariable
//////////////////////////////////////////////////////////////////////////

class CPickupRewardMoneyVariable : public CPickupRewardData
{
public:
	CPickupRewardMoneyVariable( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_MONEY; }

protected:

	PAR_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CPickupRewardMoneyFixed
//////////////////////////////////////////////////////////////////////////

class CPickupRewardMoneyFixed : public CPickupRewardData
{
public:
	CPickupRewardMoneyFixed( ) : CPickupRewardData(), Money(100) {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_MONEY; }

protected:
	rage::s32 Money;

	PAR_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CPickupRewardStat
//////////////////////////////////////////////////////////////////////////

class CPickupRewardStat : public CPickupRewardData
{
public:
	CPickupRewardStat( ) : CPickupRewardData(), Amount(0) { memset(Stat, 0, sizeof(Stat)); }

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_STAT; }

protected:
	char Stat[32];
	rage::f32 Amount;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardStatVariable
//////////////////////////////////////////////////////////////////////////

class CPickupRewardStatVariable : public CPickupRewardData
{
public:
	CPickupRewardStatVariable( ) : CPickupRewardData(), Amount(0) { memset(Stat, 0, sizeof(Stat)); }

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_STAT; }

protected:
	char Stat[32];
	rage::f32 Amount;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupRewardWeapon
//////////////////////////////////////////////////////////////////////////

class CPickupRewardWeapon : public CPickupRewardData
{
public:
	CPickupRewardWeapon( ) : CPickupRewardData(), WeaponRef(), Equip(false) {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	bool CanGive(const CPickup* pPickup, const CPed* pPed, bool bAutoSwap) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_WEAPON; }
	u32 GetWeaponHash() const { return WeaponRef.GetHash(); }
	bool AutoSwap(const CPed* pPed) const;
	bool CanGiveToPassengers(const CPickup*) const { return true; }
	bool IsUpgradedVersionInInventory(const CPed* pPed) const;

	static bool ShouldAutoSwap(const CPed* pPed, const CWeaponInfo* pPickedUpWeaponInfo, const CWeaponInfo* pEquippedWeaponInfo);
protected:
	rage::atHashString WeaponRef;
	bool Equip;

	PAR_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CPickupRewardVehicleFix
//////////////////////////////////////////////////////////////////////////

class CPickupRewardVehicleFix : public CPickupRewardData
{
public:
	CPickupRewardVehicleFix( ) : CPickupRewardData() {}

	bool CanGive(const CPickup* pPickup, const CPed* pPed) const;
	void Give(const CPickup* pPickup, CPed* pPed) const;
	PickupRewardType GetType() const { return PICKUP_REWARD_TYPE_VEHICLE_FIX; }

protected:

	PAR_PARSABLE;
};


#endif // PICKUP_REWARDS_H
