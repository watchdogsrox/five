//
// weapons/weapondamage.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_IMPACT_H
#define WEAPON_IMPACT_H

#include "physics/WorldProbe/worldprobe.h"
#include "weapons/WeaponEnums.h"

// Forward declarations
namespace rage { class Matrix34; }
namespace rage { class phIntersection; }
namespace rage { class Vector3; }
class CEntity;
class CPed;
class CTask;
class CWeapon;

class CWeaponDamage
{
public:
	static const float MELEE_VEHICLE_DAMAGE_MODIFIER;

	struct NetworkWeaponImpactInfo
	{
		NetworkWeaponImpactInfo(bool const damagedHoldingPed, bool const hitAmmoAttachment)
		:
			m_bHitWeaponAndDamagedHoldingPed(damagedHoldingPed),
			m_bHitWeaponAmmoAttachment(hitAmmoAttachment)
		{}

		NetworkWeaponImpactInfo()
		:
			m_bHitWeaponAmmoAttachment(false),
			m_bHitWeaponAndDamagedHoldingPed(false)
		{}

		inline bool GetAmmoAttachment(void)		const		{ return m_bHitWeaponAmmoAttachment;		}
		inline bool GetDamagedHoldingPed(void)	const		{ return m_bHitWeaponAndDamagedHoldingPed;	}

		inline void SetAmmoAttachment(bool const flag)		{ m_bHitWeaponAmmoAttachment = flag;		}
		inline void SetDamagedHoldingPed(bool const flag)	{ m_bHitWeaponAndDamagedHoldingPed = flag;	}

		void Clear()
		{
			m_bHitWeaponAmmoAttachment			= false;
			m_bHitWeaponAndDamagedHoldingPed	= false;
		}

	private:

		bool m_bHitWeaponAmmoAttachment:1;
		bool m_bHitWeaponAndDamagedHoldingPed:1;
	};

public:

	struct sDamageModifiers
	{
		sDamageModifiers()
			: fPedDamageMult(1.0f)
			, fVehDamageMult(1.0f)
			, fObjDamageMult(1.0f)
		{
		}

		f32 fPedDamageMult;
		f32 fVehDamageMult;
		f32 fObjDamageMult;
	};

	struct sMeleeInfo
	{
		u32 uParentMeleeActionResultID;
		u32 uForcedReactionDefinitionID;
		u16 uNetworkActionID;
	};

	// Once a weapon has a result it should call this
	static f32 DoWeaponImpact(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestResults& refResults, const f32 fApplyDamage, const fwFlags32& flags, const bool bFireDriveby = false, const bool bTemporaryNetworkWeapon = false, const sDamageModifiers* pModifiers = NULL, const sMeleeInfo *pMeleeInfo = NULL, bool bulletUnderWater = false, NetworkWeaponImpactInfo * const networkWeaponImpactInfo = NULL, const float fRecoilAccuracyWhenFired = 1.f, const float fFallOffRangeModifier = 1.f, const float fFallOffDamageModifier = 1.f, bool bReceiveFireMessage = false, bool bGenerateVFX = true, Vector3* hitDirOut = 0, const Vector3* hitDirIn = 0, u8 damageAggregationCount = 0);

	// Handle ped damage
	static f32 GeneratePedDamageEvent(CEntity* pFiringEntity, CPed* pHitPed, const u32 uWeaponHash, const f32 fWeaponDamage, const Vector3& vStart, WorldProbe::CShapeTestHitPoint* pResult, const fwFlags32& flags, const CWeapon* pWeapon = NULL, s32 iHitComponent = -1, const f32 fForce = 0.0f, const sMeleeInfo *pMeleeInfo = NULL, bool bDeferRagdollActivation = false, const float fRecoilAccuracyWhenFired = 1.f, bool bTemporaryNetworkWeapon = false, u8 damageAggregationCount = 0);

	//
	static float ApplyFallOffDamageModifier(const CEntity* pHitEntity, const CWeaponInfo* pWeaponInfo, const Vector3& vWeaponPos, const float fFallOffRangeModifier, const float fFallOffDamageModifier, const float fOrigDamage, CEntity* pFiringEntity = NULL);

// private:

	//
	// Impacts
	//

	// Handle hitting specific types of entities
	static f32 DoWeaponImpactPed(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, const sMeleeInfo *pMeleeInfo = NULL, const float fRecoilAccuracyWhenFired = 1.0f, bool* bGenerateVFX = NULL, u8 damageAggregationCount = 0);
	static f32 DoWeaponImpactCar(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bFireDriveby, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, Vector3* hitDirOut = NULL, const Vector3* hitDirIn = NULL, bool bShouldApplyDamageToGlass = true);
	static f32 DoWeaponImpactObject(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, phMaterialMgr::Id hitMaterial = 0U);
	static f32 DoWeaponImpactWeapon(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, const sMeleeInfo *pMeleeInfo = NULL, NetworkWeaponImpactInfo * const networkWeaponImpactInfo = NULL, const float fRecoilAccuracyWhenFired = 1.0f, u8 damageAggregationCount = 0);
	static f32 DoWeaponImpactBreakableGlass(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResults, const f32 fApplyDamage);

	// traffic light and search light response to weapon impact
	static void DoWeaponImpactTrafficLight(CEntity* pHitEntity, const Vector3& vDamagePos);
	static void DoWeaponImpactSearchLight(CEntity* pHitEntity, const Vector3& vDamagePos);

	// AI response to weapon impact
	static void DoWeaponImpactAI(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone);

	// Effects for impact
	static void DoWeaponImpactVfx(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags, const bool headShotNearby, const bool isExitFx, const bool bTemporaryNetworkWeapon = false, const bool bShouldApplyDamageToGlass = true);

	// Audio for impact
	static void DoWeaponImpactAudio(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags);

	// If the player is the firing entity, do stat tracking etc.
	static void DoWeaponFiredByPlayer(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags);

	// Set the last significant shot bone
	static void SetLastSignificantShotBone(CWeapon* pWeapon, CPed* pHitPed, WorldProbe::CShapeTestHitPoint* pResult);

	//
	// Damage
	//

	// 
	static bool CanUseRagdollBulletImpact(CEntity* pFiringEntity, CPed* pHitPed, const u32 uWeaponHash, const Vector3& vStart, WorldProbe::CShapeTestHitPoint* pResult, Vector3& vRagdollImpulseDir, float& fRagdollImpulseMag);

	// 
	static CTask* GenerateRagdollTask(CEntity* pFiringEntity, CPed* pHitPed, const u32 uWeaponHash, const f32 fWeaponDamage, const fwFlags32& flags, const bool bWasKilledOrInjured, const Vector3& vStart, WorldProbe::CShapeTestHitPoint* pResult, 
		const Vector3& vRagdollImpulseDir, const f32 fRagdollImpulseMag, bool &bTaskAppliesImpulse, bool &bUpdatedPreviousEvent);
	
private:

	static bool CanDropWeaponWhenShot(const CWeapon& rWeapon, const CPed& rHitPed, const CWeapon& rHitWeapon);

	static eRagdollTriggerTypes GetRagdollTriggerForDamageType(const eDamageType damageType);

	static bool IsEnduranceDamage(CPed* pPed, const CWeapon* pWeapon, const fwFlags32& flags);
};

#endif // WEAPON_IMPACT_H
