//
// weapons/ammoinfo.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Info/AmmoInfo.h"

// Parser files
#include "AmmoInfo_parser.h"

// Game headers
#include "Vfx/Systems/VfxVehicle.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Stats/StatsInterface.h"
#include "Peds/Ped.h" 

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Simple rtti implementation
INSTANTIATE_RTTI_CLASS(CAmmoInfo,0xACC55F2C);

////////////////////////////////////////////////////////////////////////////////

CAmmoInfo::CAmmoInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

const char* CAmmoInfo::GetNameFromInfo(const CAmmoInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CAmmoInfo* CAmmoInfo::GetInfoFromName(const char* name)
{
	const CAmmoInfo* pInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(atStringHash(name));
	weaponAssertf(pInfo, "Ammo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CAmmoProjectileInfo,0xEB431A0A);

CAmmoProjectileInfo::CAmmoProjectileInfo()
{
}

////////////////////////////////////////////////////////////////////////////////
s32 CAmmoInfo::GetAmmoMax(const CPed* pPed) const
{
	// Use the maximum shooting ability by default, so the ammo won't be clamped down when switching between players.
	float fShootingAbility  = 100.0f;
	if(pPed && pPed->IsLocalPlayer())
	{
		fShootingAbility = StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY"));
	}
	bool bNetworkGame = NetworkInterface::IsGameInProgress();
	if(fShootingAbility <= 50.0f)
	{
		return Lerp(fShootingAbility/50.0f, bNetworkGame ? m_AmmoMaxMP : m_AmmoMax, bNetworkGame ? m_AmmoMax50MP : m_AmmoMax50);
	}
	else
	{
		return Lerp((fShootingAbility - 50.0f)/50.0f, bNetworkGame ? m_AmmoMax50MP : m_AmmoMax50, bNetworkGame ? m_AmmoMax100MP : m_AmmoMax100);
	}
}


////////////////////////////////////////////////////////////////////////////////

eExplosionTag CAmmoProjectileInfo::GetExplosionTag(const CEntity* pHitEntity) const
{
	eExplosionTag expTag = EXP_TAG_DONTCARE;

	if(pHitEntity && pHitEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pHitEntity);
		if(pVehicle->m_nVehicleFlags.bIsBig)
		{
			expTag = m_Explosion.HitTruck;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAILER)
		{
			expTag = m_Explosion.HitCar;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || pVehicle->GetIsRotaryAircraft())
		{
			expTag = m_Explosion.HitPlane;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			expTag = m_Explosion.HitBike;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			expTag = m_Explosion.HitBoat;
		}
	}

	// If we are set to don't care, set it to the default explosion tag
	if(expTag == EXP_TAG_DONTCARE)
	{
		expTag = GetExplosionTag();
	}

	return expTag;
}

////////////////////////////////////////////////////////////////////////////////

void CAmmoProjectileInfo::OnPostLoad()
{
}

////////////////////////////////////////////////////////////////////////////////

float CAmmoProjectileInfo::GetProximityActivationTime() const
{
#if ARCHIVED_SUMMER_CONTENT_ENABLED        
	if (NetworkInterface::IsInCopsAndCrooks() && (GetHash() == atStringHash("AMMO_VEHICLEMINE_CNCSPIKE")))
	{
		if (CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationArming > 0)
		{
			return CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationArming * 0.001f;
		}
	}
#endif
	
	return m_ProximityActivationTime; 
}

////////////////////////////////////////////////////////////////////////////////

// How long till the projectile expires?
f32 CAmmoProjectileInfo::GetLifeTime() const
{ 
#if ARCHIVED_SUMMER_CONTENT_ENABLED        	
	if (NetworkInterface::IsInCopsAndCrooks() && (GetHash() == atStringHash("AMMO_VEHICLEMINE_CNCSPIKE")))
	{
		if (CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime > 0)
		{
			return CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime * 0.001f;
		}
	}
#endif
	return m_LifeTime; 
}

////////////////////////////////////////////////////////////////////////////////

// How long till the projectile expires?
f32 CAmmoProjectileInfo::GetFromVehicleLifeTime() const
{ 
#if ARCHIVED_SUMMER_CONTENT_ENABLED        
	if (NetworkInterface::IsInCopsAndCrooks() && (GetHash() == atStringHash("AMMO_VEHICLEMINE_CNCSPIKE")))
	{
		if (CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime > 0)
		{
			return CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime * 0.001f;
		}
	}
#endif

	return m_FromVehicleLifeTime; 
}


////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CAmmoRocketInfo,0x7144909D);

CAmmoRocketInfo::CAmmoRocketInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CAmmoThrownInfo,0xB11D52E5);

CAmmoThrownInfo::CAmmoThrownInfo()
{
}

s32 CAmmoThrownInfo::GetAmmoMax(const CPed* pPed) const
{
	s32 iMaxAmmo = CAmmoInfo::GetAmmoMax(pPed);

	// B*3421540: Allow script to unlock an increased ammo capacity for thrown weapons
	if(NetworkInterface::IsGameInProgress() && pPed && pPed->IsLocalPlayer())
	{
		if (StatsInterface::GetBooleanStat(StatsInterface::GetStatsModelHashId("SR_INCREASE_THROW_CAP")))
		{
			iMaxAmmo += m_AmmoMaxMPBonus;
		}
	}

	return iMaxAmmo;
}

////////////////////////////////////////////////////////////////////////////////
