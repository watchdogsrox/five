//
// Title	:	Bmx.cpp
// Author	:	Ben Lyons
// Started	:	17/09/10 (from Bike.cpp)
//

// Rage headers

#if __BANK
#include "bank/bank.h"
#endif

// Framework headers

// Game headers
#include "vehicles\bike.h"
#include "vehicles\bmx.h"
#include "debug\debugglobals.h"
#include "Stats\StatsMgr.h"
#include "Peds\ped.h"
#include "scene\world\GameWorld.h"
#include "network\Events\NetworkEventTypes.h"

ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()


//
//
//
CBmx::CBmx(const eEntityOwnedBy ownedBy, u32 popType) : CBike(ownedBy, popType, VEHICLE_TYPE_BICYCLE)
{

}
//////////////////////////////////////////////////////////////////////

//
//
//
CBmx::~CBmx()
{

}


///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a car
///////////////////////////////////////////////////////////////////////////////////

void CBmx::BlowUpCar( CEntity *pCulprit, bool UNUSED_PARAM(bInACutscene), bool UNUSED_PARAM(bAddExplosion), bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool UNUSED_PARAM(bDelayedExplosion) )
{
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	//	If the flag is set for don't damage this bmx (usually during a cutscene)
	}

	// we can't blow up bmx controlled by another machine
	// but we still have to change their status to wrecked
	// so the bike doesn't blow up if we take control of an
	// already blown up bmx
	if (IsNetworkClone())
	{
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		m_nPhysicalFlags.bRenderScorched = TRUE;  
		SetTimeOfDestruction();

		SetIsWrecked();

		// Break lights, windows and sirens
		GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

		// Allow deformation
		m_nVehicleFlags.bUseDeformation = true;

		// Reset the CG offset
		phBoundComposite* pBoundComp = (phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound();
		pBoundComp->SetCGOffset(VECTOR3_TO_VEC3V(VEC3_ZERO));
		// Update the collider position since it's dependent on the bound CG offset
		if(phCollider* pCollider = GetCollider())
			pCollider->SetColliderMatrixFromInstance();

		this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		this->m_nVehicleFlags.bLightsOn = FALSE;

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
		}

		return;
	}

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, false, weaponHash, false);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

	//AddToMoveSpeedZ(0.13f);
	ApplyImpulseCg(Vector3(0.0f, 0.0f, 6.5f));
	
	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());

	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);

	SetIsWrecked();

	m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off

	// do it before SpawnFlyingComponent() so this bit is propagated to all vehicle parts before they go flying:
	// let know pipeline, that we don't want any extra passes visible for this clump anymore:
	//	CVisibilityPlugins::SetClumpForAllAtomicsFlag((RpClump*)this->m_pRwObject, VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES);

	SetHealth(0.0f);

	KillPedsInVehicle(pCulprit, weaponHash);

	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

	// Allow deformation
	m_nVehicleFlags.bUseDeformation = true;

	// Reset the CG offset
	phBoundComposite* pBoundComp = (phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound();
	pBoundComp->SetCGOffset(VECTOR3_TO_VEC3V(VEC3_ZERO));
	// Update the collider position since it's dependent on the bound CG offset
	if(phCollider* pCollider = GetCollider())
		pCollider->SetColliderMatrixFromInstance();

	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
	}

	NetworkInterface::ForceHealthNodeUpdate(*this);
	m_nVehicleFlags.bBlownUp = true;
}
