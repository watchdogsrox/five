#ifndef PLAYER_PED_TARGETING_H
#define PLAYER_PED_TARGETING_H

// Rage
#include "Vector/Color32.h"
#include "Vector/Vector3.h"

// Game headers
#include "Frontend/UIReticule.h"
#include "Physics/WorldProbe/ShapeTestResults.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Tuning.h"
#include "Weapons/TargetSequence.h"

// Forward declarations
class CEntity;
class CPed;
class CWeapon;
class CWeaponInfo;

//////////////////////////////////////////////////////////////////////////
// CCurve
//////////////////////////////////////////////////////////////////////////

#if __BANK
class CCurve : public datBase
#else
class CCurve
#endif // __BANK
{
public:

	CCurve();
	virtual ~CCurve() {};

	//
	void Set(float fInputMax, float fResultMax, float fPow) { m_fInputMax = fInputMax; m_fResultMax = fResultMax; m_fPow = fPow; UpdateOneOverInputMaxMinusInputMinInternal(); }

	// Settors
	void SetInitialValue(float fInitialValue) { m_fInitialValue = fInitialValue; }
	void SetInputMin(float fInputMin) { m_fInputMin = fInputMin; UpdateOneOverInputMaxMinusInputMinInternal(); }
	void SetInputMax(float fInputMax) { m_fInputMax = fInputMax; UpdateOneOverInputMaxMinusInputMinInternal(); }
	void SetResultMax(float fResultMax) { m_fResultMax = fResultMax; }
	void SetPow(float fPow) { m_fPow = fPow; }

	// Accessors
	float GetInitialValue() const { return m_fInitialValue; }
	float GetInputMin() const { return m_fInputMin; }
	float GetInputMax() const { return m_fInputMax; }
	float GetResultMax() const { return m_fResultMax; }
	float GetPow() const { return m_fPow; }

	//
	float GetResult(float fInput) const;

	void UpdateOneOverInputMaxMinusInputMinInternal();

	// Cache this value, as it should change
	void UpdateCurveOnChange();

private:

	// Members
	float m_fInitialValue;
	float m_fInputMin;
	float m_fInputMax;
	float m_fResultMax;
	float m_fPow;
	float m_fOneOverInputMaxMinusInputMin;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CCurveSet
//////////////////////////////////////////////////////////////////////////

class CCurveSet
{
public:

	CCurveSet();

	virtual ~CCurveSet() {};

	//
	void SetCurve(s32 iCurveIndex, float fInputMax, float fResultMax, float fPow);

	//
	float GetResult(float fInput) const;

	// Go through all the curves and update the min/max/initial values based on the other curves in the set
	void Finalise();

#if __BANK
	void PreAddWidgets(bkBank& bank);
	void PostAddWidgets(bkBank& bank);
	void Render(const Vector2& vOrigin, const Vector2& vEnd, float fInputScale = 1.0f) const;
	//void SetName(const char* pName) { m_pName = pName; }
	void SetColour(const Color32 colour) { m_colour = colour; }
#endif // __BANK

private:

	// Curves that make up the set
	atArray<CCurve>				m_curves;
	atHashWithStringNotFinal	m_Name;

#if __BANK
	Color32 m_colour;
#endif // __BANK

	PAR_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CTargettingDifficultyInfo
//////////////////////////////////////////////////////////////////////////

enum eLockType
{
	LT_Hard,
	LT_Soft,
	LT_None,
};

struct CTargettingDifficultyInfo
{
	enum CurveSets
	{
		CS_Aim,
		CS_AimAssist,
		CS_AimZoom,
		CS_AimAssistZoom,
		CS_Max
	};

	CTargettingDifficultyInfo() {};
	virtual ~CTargettingDifficultyInfo() {};

	void PostLoad();

	eLockType m_LockType;
	bool m_UseFirstPersonStickyAim;
	bool m_UseLockOnTargetSwitching;
	bool m_UseReticuleSlowDown;
	bool m_UseReticuleSlowDownForRunAndGun;
	bool m_EnableBulletBending;
	bool m_AllowSoftLockFineAim;
	bool m_UseFineAimSpring;
	bool m_UseNewSlowDownCode;
	bool m_UseCapsuleTests;
	bool m_UseDriveByAssistedAim;
	bool m_CanPauseSoftLockTimer;
	float m_LockOnRangeModifier;
	float m_ReticuleSlowDownRadius;
	float m_ReticuleSlowDownCapsuleRadius;
	float m_ReticuleSlowDownCapsuleLength;
	float m_DefaultTargetAngularLimit;
	float m_DefaultTargetAngularLimitClose;
	float m_DefaultTargetAngularLimitCloseDistMin;
	float m_DefaultTargetAngularLimitCloseDistMax;
	float m_WideTargetAngularLimit;
	float m_CycleTargetAngularLimit;
	float m_CycleTargetAngularLimitMelee;
	float m_DefaultTargetAimPitchMin;
	float m_DefaultTargetAimPitchMax;
	float m_NoReticuleLockOnRangeModifier;
	float m_NoReticuleMaxLockOnRange;
	float m_NoReticuleTargetAngularLimit;
	float m_NoReticuleTargetAngularLimitClose;
	float m_NoReticuleTargetAngularLimitCloseDistMin;
	float m_NoReticuleTargetAngularLimitCloseDistMax;
	float m_NoReticuleTargetAimPitchLimit;
	float m_MinVelocityForDriveByAssistedAim;
	float m_LockOnDistanceRejectionModifier;
	float m_FineAimVerticalMovement;
	float m_FineAimDownwardsVerticalMovement;
	float m_FineAimSidewaysScale;
	float m_MinSoftLockBreakTime;
	float m_MinSoftLockBreakTimeCloseRange;
	float m_MinSoftLockBreakAtMaxXStickTime;
	float m_SoftLockBreakDistanceMin;
	float m_SoftLockBreakDistanceMax;
	float m_MinFineAimTime;
	float m_MinFineAimTimeHoldingStick;
	float m_MinNoReticuleAimTime;
	float m_AimAssistCapsuleRadius;
	float m_AimAssistCapsuleRadiusInVehicle;
	float m_AimAssistCapsuleMaxLength;
	float m_AimAssistCapsuleMaxLengthInVehicle;
	float m_AimAssistCapsuleMaxLengthFreeAim;
	float m_AimAssistCapsuleMaxLengthFreeAimSniper;
	float m_AimAssistBlendInTime;
	float m_AimAssistBlendOutTime;
	float m_SoftLockFineAimBreakXYValue;
	float m_SoftLockFineAimBreakZValue;
	float m_SoftLockFineAimXYAbsoluteValue;
	float m_SoftLockFineAimXYAbsoluteValueClose;
	float m_SoftLockBreakValue;
	float m_SoftLockTime;
	float m_SoftLockTimeToAcquireTarget;
	float m_SoftLockTimeToAcquireTargetInCover;
	float m_FineAimHorSpeedMin;
	float m_FineAimHorSpeedMax;
	float m_FineAimVerSpeed;
	float m_FineAimSpeedMultiplier;
	float m_FineAimHorWeightSpeedMultiplier;
	float m_FineAimHorSpeedPower;
	float m_FineAimSpeedMultiplierClose;
	float m_FineAimSpeedMultiplierCloseDistMin;
	float m_FineAimSpeedMultiplierCloseDistMax;
	float m_BulletBendingNearMultiplier;
	float m_BulletBendingFarMultiplier;
	float m_BulletBendingZoomMultiplier;
	float m_InVehicleBulletBendingNearMinVelocity;
	float m_InVehicleBulletBendingFarMinVelocity;
	float m_InVehicleBulletBendingNearMaxVelocity;
	float m_InVehicleBulletBendingFarMaxVelocity;
	float m_InVehicleBulletBendingMaxVelocity;
	float m_XYDistLimitFromAimVector;
	float m_ZDistLimitFromAimVector;
	u32 m_LockOnSwitchTimeExtensionBreakLock;
	u32 m_LockOnSwitchTimeExtensionKillTarget;
	atFixedArray<CCurveSet, CS_Max> m_CurveSets;
	CCurveSet m_AimAssistDistanceCurve;

	PAR_PARSABLE;
};


//////////////////////////////////////////////////////////////////////////
// CPlayerPedTargeting
//////////////////////////////////////////////////////////////////////////

class CPlayerPedTargeting
{
public:

	// PURPOSE:	Tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

 		// DEV
 		void SetEntityTargetableDistCB();
 		float m_fTargetableDistance;
		// DEV
		void SetEntityTargetThreatCB();
		float m_fTargetThreatOverride;

		float m_UnarmedInCoverTargetingDistance;
		float m_MeleeLostLOSBreakTime;
		float m_ArrestHardLockDistance;
		float m_CoverDirectionOffsetForInCoverTarget;
		u32 m_TimeToAllowCachedStickInputForMelee;
		bool m_DoAynchronousProbesWhenFindingFreeAimAssistTarget;
		bool m_AllowDriverLockOnToAmbientPeds;
		bool m_AllowDriverLockOnToAmbientPedsInSP;
		bool m_DisplayAimAssistIntersections;
		bool m_DisplayAimAssistTest;
		bool m_DisplayAimAssistCurves;
		bool m_DisplayLockOnDistRanges;
		bool m_DisplayLockOnAngularRanges;
		bool m_DisplaySoftLockDebug;
		bool m_DebugLockOnTargets;
		bool m_DisplayFreeAimTargetDebug;
		bool m_UseRagdollTargetIfNoAssistTarget;
		bool m_UseReticuleSlowDownStrafeClamp;

		const CTargettingDifficultyInfo& GetTargettingInfo() const;
		eLockType GetLockType() const { return GetTargettingInfo().m_LockType; }
		bool GetUseFirstPersonStickyAim() const { return GetTargettingInfo().m_UseFirstPersonStickyAim; }
		bool GetUseLockOnTargetSwitching() const { return GetTargettingInfo().m_UseLockOnTargetSwitching; }
		bool GetUseReticuleSlowDown() const { return GetTargettingInfo().m_UseReticuleSlowDown; }
		bool GetUseReticuleSlowDownForRunAndGun() const { return GetTargettingInfo().m_UseReticuleSlowDownForRunAndGun; }
		bool GetEnableBulletBending() const { return GetTargettingInfo().m_EnableBulletBending; }
		bool GetAllowSoftLockFineAim() const { return GetTargettingInfo().m_AllowSoftLockFineAim; }
		bool GetUseFineAimSpring() const { return GetTargettingInfo().m_UseFineAimSpring; }
		bool GetUseNewSlowDownCode() const { return GetTargettingInfo().m_UseNewSlowDownCode; }
		bool GetUseCapsuleTests() const { return GetTargettingInfo().m_UseCapsuleTests; }
		bool GetUseDriveByAssistedAim() const { return GetTargettingInfo().m_UseDriveByAssistedAim; }
		bool CanPauseSoftLockTimer() const { return GetTargettingInfo().m_CanPauseSoftLockTimer; }
		float GetLockOnRangeModifier() const { return GetTargettingInfo().m_LockOnRangeModifier; }
		float GetReticuleSlowDownRadius() const { return GetTargettingInfo().m_ReticuleSlowDownRadius; }
		float GetReticuleSlowDownCapsuleRadius() const { return GetTargettingInfo().m_ReticuleSlowDownCapsuleRadius; }
		float GetReticuleSlowDownCapsuleLength() const { return GetTargettingInfo().m_ReticuleSlowDownCapsuleLength; }
		float GetDefaultTargetAngularLimit() const { return GetTargettingInfo().m_DefaultTargetAngularLimit; }
		float GetDefaultTargetAngularLimitClose() const { return GetTargettingInfo().m_DefaultTargetAngularLimitClose; }
		float GetDefaultTargetAngularLimitCloseDistMin() const { return GetTargettingInfo().m_DefaultTargetAngularLimitCloseDistMin; }
		float GetDefaultTargetAngularLimitCloseDistMax() const { return GetTargettingInfo().m_DefaultTargetAngularLimitCloseDistMax; }
		float GetWideTargetAngularLimit() const { return GetTargettingInfo().m_WideTargetAngularLimit; }
		float GetCycleTargetAngularLimit() const { return GetTargettingInfo().m_CycleTargetAngularLimit; }
		float GetCycleTargetAngularLimitMelee() const { return GetTargettingInfo().m_CycleTargetAngularLimitMelee; }
		float GetDefaultTargetAimPitchMin() const { return GetTargettingInfo().m_DefaultTargetAimPitchMin; }
		float GetDefaultTargetAimPitchMax() const { return GetTargettingInfo().m_DefaultTargetAimPitchMax; }
		float GetNoReticuleLockOnRangeModifier() const { return GetTargettingInfo().m_NoReticuleLockOnRangeModifier; }
		float GetNoReticuleMaxLockOnRange() const { return GetTargettingInfo().m_NoReticuleMaxLockOnRange; }
		float GetNoReticuleTargetAngularLimit() const { return GetTargettingInfo().m_NoReticuleTargetAngularLimit; }
		float GetNoReticuleTargetAngularLimitClose() const { return GetTargettingInfo().m_NoReticuleTargetAngularLimitClose; }
		float GetNoReticuleTargetAngularLimitCloseDistMin() const { return GetTargettingInfo().m_NoReticuleTargetAngularLimitCloseDistMin; }
		float GetNoReticuleTargetAngularLimitCloseDistMax() const { return GetTargettingInfo().m_NoReticuleTargetAngularLimitCloseDistMax; }
		float GetNoReticuleTargetAimPitchLimit() const { return GetTargettingInfo().m_NoReticuleTargetAimPitchLimit; }
		float GetMinVelocityForDriveByAssistedAim() const { return GetTargettingInfo().m_MinVelocityForDriveByAssistedAim; }
		float GetLockOnDistanceRejectionModifier() const { return GetTargettingInfo().m_LockOnDistanceRejectionModifier; }
		float GetFineAimVerticalMovement() const { return GetTargettingInfo().m_FineAimVerticalMovement; };
		float GetFineAimDownwardsVerticalMovement() const { return GetTargettingInfo().m_FineAimDownwardsVerticalMovement; };
		float GetFineAimSidewaysScale() const { return GetTargettingInfo().m_FineAimSidewaysScale; };
		float GetMinSoftLockBreakTime() const { return GetTargettingInfo().m_MinSoftLockBreakTime; };
		float GetMinSoftLockBreakTimeCloseRange() const { return GetTargettingInfo().m_MinSoftLockBreakTimeCloseRange; };
		float GetMinSoftLockBreakAtMaxXStickTime() const { return GetTargettingInfo().m_MinSoftLockBreakAtMaxXStickTime; };
		float GetSoftLockBreakDistanceMin() const { return GetTargettingInfo().m_SoftLockBreakDistanceMin; };
		float GetSoftLockBreakDistanceMax() const { return GetTargettingInfo().m_SoftLockBreakDistanceMax; };
		float GetMinFineAimTime() const { return GetTargettingInfo().m_MinFineAimTime; };
		float GetMinFineAimTimeHoldingStick() const { return GetTargettingInfo().m_MinFineAimTimeHoldingStick; };
		float GetMinNoReticuleAimTime() const { return GetTargettingInfo().m_MinNoReticuleAimTime; };
		float GetAimAssistCapsuleRadius() const { return GetTargettingInfo().m_AimAssistCapsuleRadius; };
		float GetAimAssistCapsuleRadiusInVehicle() const { return GetTargettingInfo().m_AimAssistCapsuleRadius; };
		float GetAimAssistCapsuleMaxLength() const { return GetTargettingInfo().m_AimAssistCapsuleMaxLength; };
		float GetAimAssistCapsuleMaxLengthInVehicle() const { return GetTargettingInfo().m_AimAssistCapsuleMaxLengthInVehicle; };
		float GetAimAssistCapsuleMaxLengthFreeAim() const { return GetTargettingInfo().m_AimAssistCapsuleMaxLengthFreeAim; };
		float GetAimAssistCapsuleMaxLengthFreeAimSniper() const { return GetTargettingInfo().m_AimAssistCapsuleMaxLengthFreeAimSniper; };
		float GetAimAssistBlendInTime() const { return GetTargettingInfo().m_AimAssistBlendInTime; };
		float GetAimAssistBlendOutTime() const { return GetTargettingInfo().m_AimAssistBlendOutTime; };
		float GetSoftLockFineAimBreakXYValue() const { return GetTargettingInfo().m_SoftLockFineAimBreakXYValue; };
		float GetSoftLockFineAimBreakZValue() const { return GetTargettingInfo().m_SoftLockFineAimBreakZValue; };
		float GetSoftLockFineAimXYAbsoluteValue() const { return GetTargettingInfo().m_SoftLockFineAimXYAbsoluteValue; };
		float GetSoftLockFineAimXYAbsoluteValueClose() const { return GetTargettingInfo().m_SoftLockFineAimXYAbsoluteValueClose; };
		float GetSoftLockBreakValue() const { return GetTargettingInfo().m_SoftLockBreakValue; };
		float GetSoftLockTime() const { return GetTargettingInfo().m_SoftLockTime; };
		float GetSoftLockTimeToAcquireTarget() const { return GetTargettingInfo().m_SoftLockTimeToAcquireTarget; };
		float GetSoftLockTimeToAcquireTargetInCover() const { return GetTargettingInfo().m_SoftLockTimeToAcquireTargetInCover; };
		float GetFineAimHorSpeedMin() const { return GetTargettingInfo().m_FineAimHorSpeedMin; };
		float GetFineAimHorSpeedMax() const { return GetTargettingInfo().m_FineAimHorSpeedMax; };
		float GetFineAimVerSpeed() const { return GetTargettingInfo().m_FineAimVerSpeed; };
		float GetFineAimSpeedMultiplier() const { return GetTargettingInfo().m_FineAimSpeedMultiplier; };
		float GetFineAimHorWeightSpeedMultiplier() const { return GetTargettingInfo().m_FineAimHorWeightSpeedMultiplier; };
		float GetFineAimHorSpeedPower() const { return GetTargettingInfo().m_FineAimHorSpeedPower; };
		float GetFineAimSpeedMultiplierClose() const { return GetTargettingInfo().m_FineAimSpeedMultiplierClose; };
		float GetFineAimSpeedMultiplierCloseDistMin() const { return GetTargettingInfo().m_FineAimSpeedMultiplierCloseDistMin; };
		float GetFineAimSpeedMultiplierCloseDistMax() const { return GetTargettingInfo().m_FineAimSpeedMultiplierCloseDistMax; };
		float GetBulletBendingNearMultiplier() const { return GetTargettingInfo().m_BulletBendingNearMultiplier; };
		float GetBulletBendingFarMultiplier() const { return GetTargettingInfo().m_BulletBendingFarMultiplier; };
		float GetBulletBendingZoomMultiplier() const { return GetTargettingInfo().m_BulletBendingZoomMultiplier; };
		float GetInVehicleBulletBendingNearMinVelocity() const { return GetTargettingInfo().m_InVehicleBulletBendingNearMinVelocity; };
		float GetInVehicleBulletBendingFarMinVelocity() const { return GetTargettingInfo().m_InVehicleBulletBendingFarMinVelocity; };
		float GetInVehicleBulletBendingNearMaxVelocity() const { return GetTargettingInfo().m_InVehicleBulletBendingNearMaxVelocity; };
		float GetInVehicleBulletBendingFarMaxVelocity() const { return GetTargettingInfo().m_InVehicleBulletBendingFarMaxVelocity; };
		float GetInVehicleBulletBendingMaxVelocity() const { return GetTargettingInfo().m_InVehicleBulletBendingMaxVelocity; };
		float GetXYDistLimitFromAimVector() const { return GetTargettingInfo().m_XYDistLimitFromAimVector; };
		float GetZDistLimitFromAimVector() const { return GetTargettingInfo().m_ZDistLimitFromAimVector; };
		u32 GetLockOnSwitchTimeExtensionBreakLock() const { return GetTargettingInfo().m_LockOnSwitchTimeExtensionBreakLock; }
		u32 GetLockOnSwitchTimeExtensionKillTarget() const { return GetTargettingInfo().m_LockOnSwitchTimeExtensionKillTarget; }

		CTargettingDifficultyInfo m_EasyTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_NormalTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_HardTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_ExpertTargettingDifficultyInfo;

		CTargettingDifficultyInfo m_FirstPersonEasyTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_FirstPersonNormalTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_FirstPersonHardTargettingDifficultyInfo;
		CTargettingDifficultyInfo m_FirstPersonExpertTargettingDifficultyInfo;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable params
	static Tunables	ms_Tunables;	

	// Constructor
	CPlayerPedTargeting();

	// Destructor
	virtual ~CPlayerPedTargeting() {};

	// Set the owner ped
	void SetOwner(CPed* pPed);

	// Reset targeting vars.
	void Reset();

	// Processing
	void Process();
	void ProcessPreCamera();

	// Get the current target
	const CEntity* GetTarget() const;
	CEntity* GetTarget();

	//
	// Lock-on API
	//

	// Get the current lock-on target
	const CEntity* GetLockOnTarget() const { return m_pLockOnTarget; }
	CEntity* GetLockOnTarget() { return m_pLockOnTarget; }

	float GetTimeLockedOn() const { return m_fTimeLockedOn; }

	bool HasTargetSwitchedToLockOnTarget() const { return m_bTargetSwitchedToLockOnTarget; }

	// Get the position of the lock-on target, this in interpolated when the target changes
	const Vector3& GetLockonTargetPos() const { return m_vLockOnTargetPos; } 

	// Get the world position of the free aim target
	const Vector3& GetClosestFreeAimTargetPos() const { return m_vClosestFreeAimTargetPos; } 

#if RSG_PC
	// Get the world position of the free aim target for 3D stereo
	const Vector3& GetClosestFreeAimTargetPosStereo() const { return m_vClosestFreeAimTargetPosStereo; } 
	const Vector3& GetFreeAimTargetDir() const { return m_vFreeAimTargetDir; }
	float GetClosestFreeAimTargetDistStereo() const { return m_fClosestFreeAimTargetDist; }
#endif

	// Set the lock-on target
	void SetLockOnTarget(CEntity* pTarget, bool bTargetSwitched = false);

	// Clear the lock-on target
	void ClearLockOnTarget();

	float GetTimeAimingAtTarget() const { return m_fTimeAiming; }
	void ResetTimeAimingAtTarget() { m_fTimeAiming = 0.0f; }

	// Override the lockon distance - is reset every frame
	void SetLockOnRangeOverride(const float fRange) { m_fLockOnRangeOverride = fRange; }

	//
	// Fine aiming API
	//

	// Get the fine aiming offset
	bool GetFineAimingOffset(Vector3& vFineAimOffset) const;

	//
	// Free aiming API
	//

	// Get the current free aim target
	const CEntity* GetFreeAimTarget() const { return m_pFreeAimTarget; }
	CEntity* GetFreeAimTarget() { return m_pFreeAimTarget; }

	// Get the time spent aiming at the current free aim target
	float GetTimeSpentAimingAtFreeAimTarget() const { return m_uStartTimeAimingAtFreeAimTarget == 0 ? 0.f : (float)(fwTimer::GetTimeInMilliseconds() - m_uStartTimeAimingAtFreeAimTarget) * 0.001f; }

	// Get the current free aim ragdoll target
	const CEntity* GetFreeAimTargetRagdoll() const { return m_pFreeAimTargetRagdoll; }
	CEntity* GetFreeAimTargetRagdoll() { return m_pFreeAimTargetRagdoll; }

	void SetFreeAimTargetRagdoll(CEntity *pEntity) { m_pFreeAimTargetRagdoll = pEntity; }

	// Get the current aim assist target
	const CEntity* GetFreeAimAssistTarget() const { return m_pFreeAimAssistTarget; }
	CEntity* GetFreeAimAssistTarget() { return m_pFreeAimAssistTarget; }

	// Get the position of the free aim assist target
	const Vector3& GetFreeAimAssistTargetPos() const { return m_vFreeAimAssistTargetPos; }

	// Get the position of the closest free aim assist target (that may not be valid)
	const Vector3& GetClosestFreeAimAssistTargetPos() const { return m_vClosestFreeAimAssistTargetPos; }

	u32 GetTimeSinceLastAiming() const { return m_uTimeSinceLastAiming; }

	// Reports if the the closest free aim target is a vehicle.
	// This is specially configured for use in the camera system and may not be suitable for general use.
	bool IsFreeAimTargetingVehicleForCamera() const { return m_bFreeAimTargetingVehicleForCamera; }

	enum eFiringVecCalcType
	{
		FIRING_VEC_CALC_FROM_POS = 0,
		FIRING_VEC_CALC_FROM_ENT,
		FIRING_VEC_CALC_FROM_AIM_CAM,
		FIRING_VEC_CALC_FROM_GUN_ORIENTATION,
		FIRING_VEC_CALC_FROM_WEAPON_MATRIX
	};

	static bool GetAreCNCFreeAimImprovementsEnabled(const CPed* pPed, const bool bControllerOnly = false);
	bool GetAreCNCFreeAimImprovementsEnabled(const bool bControllerOnly = false) const;

	// Does the current free aim assist target position fall within the specified constraints
	// Converts the free aim assist capsule test into a cone test, 
	// with the near radius defined by fNearRadius and the far radius defined by fFarRadius
	// If the free aim assist target falls within the cone then returns true
	bool GetIsFreeAimAssistTargetPosWithinConstraints(const CWeapon* pWeapon, const Matrix34& mWeapon, bool bZoomed, eFiringVecCalcType eFireVecType) const;
	bool GetIsFreeAimAssistTargetPosWithinConstraints(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool bZoomed) const;
	bool GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraints(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool bZoomed) const;
	bool GetIsFreeAimAssistTargetPosWithinReticuleSlowDownConstraintsNew(const CWeapon* pWeapon, const Vector3& vStart, Vector3& vEnd, bool bZoomed) const;

#if __BANK
	void RenderAimAssistConstraints(bool bWithinConstraints, const Vector3& vStart, const Vector3& vEnd, const Vector3& vClosestPoint, float fRadiusAtDistance) const;
#endif // __BANK

	// CS: Temp Function??
	// Takes the look around input mag from the camera and produces a scaled look around input mag
	// We do this by passing the camera input mag into a curve set
	bool GetScaledLookAroundInputMagForCamera(const float fCurrentLookAroundInputMag, float& fScaledLookAroundInputMag);

	// Go through the test results and return the index in the intersections array that matches the conditions and scores highest
	void FindFreeAimAssistTarget(const Vector3& vStart, const Vector3& vEnd);

	// Do the sideways capsule test
	void ProcessFreeAimAssistTargetCapsuleTests(const Vector3& vStart, const Vector3& vEnd);

	// Are we currently aiming?
	bool GetIsAiming() const;

	bool GetIsFineAiming() const { return m_bPlayerIsFineAiming; } 

	// Is pushing stick 
	bool IsPushingStick(const float fThreshHold) const;

	// Are we fine aiming within the limits, or should we break off
	bool IsFineAimWithinBreakLimits() const;
	bool IsFineAimWithinBreakXYLimits() const;
	bool IsFineAimWithinAbsoluteXYLimits(float fLimit) const;

	// Do I have a dead lock on target?
	bool HasDeadLockonTarget() const;

	//! Do actual find of hard lock target.
	CEntity *FindLockOnTarget();

	bool GetVehicleHomingEnabled() const { return m_bVehicleHomingEnabled; }
	void SetVehicleHomingEnabled(bool bValue) { m_bVehicleHomingEnabled = bValue; }

	bool GetVehicleHomingForceDisabled() const { return m_bVehicleHomingForceDisabled; }
	void SetVehicleHomingForceDisabled(bool bValue) { m_bVehicleHomingForceDisabled = bValue; }

	//! Return the current targeting mode (hard lock/soft lock/free aim).
	u32 GetCurrentTargetingMode() const { return (u32)m_eLastTargetingMode; }

	u32 GetLastWeaponHash() const { return m_uLastWeaponHash; }

	u32 GetNoTargetingMode() const { return m_bNoTargetingMode; }

	bool ShouldUpdateTargeting() const;

	// Get the equipped CWeaponInfo
	const CWeaponInfo* GetWeaponInfo() const;
	static const CWeaponInfo *GetWeaponInfo(CPed *pPed);

	bool IsLockOnDisabled() const;
	bool IsDisabledDueToWeaponWheel() const;

	enum ePlayerTargetLevel
	{
		PTL_TARGET_EVERYONE = 0,
		PTL_TARGET_STRANGERS,
		PTL_TARGET_ATTACKERS,
		PTL_MAX,
	};

	static void SetPlayerTargetLevel(ePlayerTargetLevel eTargetLevel) { ms_ePlayerTargetLevel = eTargetLevel; }
	static ePlayerTargetLevel GetPlayerTargetLevel() { return ms_ePlayerTargetLevel; }


#if __BANK
	//static void AddWidgets(bkBank& bank);
	void Debug();
	static bool ms_bEnableSnapShotBulletBend;
	static float ms_fSnapShotRadiusMin;
	static float ms_fSnapShotRadiusMax;
#endif // __BANK

#if USE_TARGET_SEQUENCES
	CTargetSequenceHelper& GetTargetSequenceHelper() {return m_TargetSequenceHelper; }
#endif // USE_TARGET_SEQUENCES

	// Used to determine if headshots should be inflicted
	bool IsPrecisionAiming() const;

	// Reset the soft aim variables. Allows soft lock to re-trigger without a repress of aim button.
	void ClearSoftAim();

	//
	// Curve sets
	//

	// Finalise curve sets
	static void FinaliseCurveSets();

	CEntity *FindLockOnMeleeTarget(const CPed *pPed, bool bAllowLockOnIfTargetFriendly = false);

	CEntity *FindMeleeTarget(const CPed *pPed, 
		const CWeaponInfo* pWeaponInfo, 
		bool bDoMeleeActionCheck, 
		bool bTargetDeadPeds = true,
		bool bFallBackToPedHeading = false,
		bool bMustUseDesiredDirection = false,
		const CEntity* pCurrentMeleeTarget = NULL);

	static bool IsCameraInAccurateMode();
	static float ComputeExtraZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo);
	static void GetBulletBendingData(const CPed *pPed,  const CWeapon* pWeapon, bool bZoomed, float &fNear, float &fFar);

	enum OnFootHomingLockOnState
	{
		OFH_NOT_LOCKED,
		OFH_ACQUIRING_LOCK_ON,
		OFH_LOCKED_ON,
		OFH_NO_TARGET,
	};
	OnFootHomingLockOnState GetOnFootHomingLockOnState(const CPed* pPed);

	// Targetable Entities 
	void AddTargetableEntity(CEntity* pEntity);
	void RemoveTargetableEntity(CEntity* pEntity);
	void ClearTargetableEntities();
	bool GetIsEntityAlreadyRegisteredInTargetableArray(CEntity* pEntity) const;

	int GetNumTargetableEntities() const { return m_TargetableEntities.GetCount(); } 
	CEntity* GetTargetableEntity(int iIndex) const;

	enum TargetingMode
	{
		HardLockOn,
		SoftLockOn,
		FreeAim,
		None,
		Manual
	};

#if __BANK
	static void DebugAddTargetableEntity();
	static void DebugClearTargetableEntities();
#endif	// __BANK

private:

#if USE_TARGET_SEQUENCES
	bool IsTargetSequenceReadyToFire() const;

	bool IsUsingTargetSequence() const;
#endif // USE_TARGET_SEQUENCES

	//
	// Lock-on functions
	//

	// Search for a lock-on target, if one is found returns true
	void UpdateLockOnTarget();

	// Update the lock-on target position
	void UpdateLockOnTargetPos();

	// Search for a lock-on target
	bool FindAndSetLockOnTarget();

	bool ShouldUpdateFineAim(const CWeaponInfo& weaponInfo);
	bool UpdateHardLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo);
	bool UpdateSoftLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo);
	bool UpdateFreeAim(const CWeaponInfo& weaponInfo);
	void ProcessPedEvents();
	void ProcessPedEvents(CEntity* pEntity);
	void ProcessPedEventsForFreeAimAssist();

	bool IsTaskDisablingTargeting() const;
	bool CanIncrementSoftLockTimer() const;

	TargetingMode GetTargetingMode(const CWeapon* pWeapon, const CWeaponInfo&);
	void UpdateLockOn(const TargetingMode eTargetMode, CEntity *pCurrentLockOnEntity);
	bool CannotLockOn(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo, const TargetingMode eTargetMode);
	

	bool ProcessOnFootHomingLockOn(CPed *pPed);
	bool IsSelectingLeftOnFootHomingTarget(const CPed *pPed);
	bool IsSelectingRightOnFootHomingTarget(const CPed *pPed);

	bool CanPedLockOnWithWeapon(const CWeapon* pWeapon, const CWeaponInfo& weaponInfo);

	enum HeadingDirection
	{ 
		HD_LEFT = 0,
		HD_RIGHT,
	};

	// Search for a lock-on target in the specified direction
	bool FindNextLockOnTarget(CEntity* pOldTarget, HeadingDirection eDirection, bool bSetTarget = true);

	// Query if we should break lock-on with the target entity
	bool GetShouldBreakLockOnWithTarget();

	// Query if we should break soft lock-on with the target entity.
	bool GetShouldBreakSoftLockTarget();

	// Query if we should break lock-on if we lose LOS to target.
	bool GetShouldBreakLockTargetIfNoLOS(s32 iFlags, float fBreakTime, u32 nTestTimeMS);

	// Update the fine aim variables
	void UpdateFineAim(bool bSpringback = true);

	// Reset the fine aim variables
	void ClearFineAim();

	//
	// Free aim functions
	//

	// Reset the free aim variables
	void ClearFreeAim();

	// 
	void FindFreeAimTarget(const Vector3& vStart, const Vector3& vEnd, const CWeaponInfo& weaponInfo, Vector3& vHitPos);

	// Checks if the ped passes rejection checks
	bool GetIsPedValidForFreeAimAssist(const CPed* pTargetPed) const;

	// Calculates a score for the passed in intersection - a lower score is better
	float GetScoreForFreeAimAssistIntersection(const Vector3& vStart, const Vector3& vEnd, const Vector3& vHitPos) const;

	// Do a probe horizontally from the aiming vector in the direction of the hit point
	bool GetMoreAccurateFreeAimAssistTargetPos(const Vector3& vStart, const Vector3& vEnd, const Vector3& vHitPos, const CEntity* pHitEntity, Vector3& vNewHitPos) const;

	// Returns the bullet bending radius given a distance to target
	float GetFreeAimRadiusAtADistance( float fDistToTarget ) const;

	//
	// Helper functions
	//

	// Get the target sequence group for the current weapon (if there is one)
	atHashWithStringNotFinal GetTargetSequenceGroup() const;

	// Determine if we can promote targeting mode to hard lock.
	bool CanPromoteToHardLock(TargetingMode eTargetingMode, const CWeapon* pWeapon, const CWeaponInfo& weaponInfo);

	// Construct flags for finding lock on target.
	s32 GetFindTargetFlags() const;

	// Determine if, when locking on, we should play PULL_GUN speech
	bool ShouldUsePullGunSpeech(const CWeaponInfo* pWeaponInfo);

	// Get weapon lock on range.
	float GetWeaponLockOnRange(const CWeaponInfo* pWeaponInfo, CEntity *pEntity = NULL) const;

	// Check if melee targeting should use normal lock on mechanic.
	bool IsMeleeTargetingUsingNormalLock() const;

	// Can we do an aim assist, when shooting from hip?
	bool CanUseNoReticuleAimAssist() const;

	//
	// Members
	//

	// Owner ped
	RegdPed m_pPed;

	// The length of time we have been aiming
	float m_fTimeAiming;

	// Last cached targeting mode.
	TargetingMode m_eLastTargetingMode;
	u32 m_uLastWeaponHash;
	bool m_bNoTargetingMode : 1;
	bool m_bTargetingStartedInWeaponWheelHUD : 1;

	//
	// Input caching for melee. 
	//

	Vector2 m_vLastValidStickInput;
	u32 m_LastTimeDesiredStickInputSet;

	//
	// Lock-on aiming
	//

	// The entity we are locked on to, if any
	RegdEnt m_pLockOnTarget;

	// temp locked on entity - we can use this to save having to do the work again.
	RegdEnt m_pCachedLockOnTarget;

	// The position of the lock-on target, this in interpolated when the target changes
	Vector3 m_vLastLockOnTargetPos;
	Vector3 m_vLockOnTargetPos;

	// The world position of the free aim target
	Vector3 m_vClosestFreeAimTargetPos;
#if RSG_PC
	Vector3	m_vClosestFreeAimTargetPosStereo;
	Vector3 m_vFreeAimTargetDir;
	float m_fClosestFreeAimTargetDist;
#endif

	// On-foot homing
	RegdEnt		m_pPotentialTarget;
	float		m_fPotentialTargetTimer;
	u32			m_uManualSwitchTargetTime;

	// LockOn range override.  Set every frame to modify the lockon range of the player (-1.0f when not overriding)
	float m_fLockOnRangeOverride;

	// Timer to track the time we stay locked onto a dead ped in assisted aiming mode
	float m_fAssistedAimDeadPedLockOnTimer;

	// Adds the ability to toggle homing on/off for vehicle weapons
	bool m_bVehicleHomingEnabled;

	// Adds the ability for script to force disable homing on all vehicle weapons (overriding value of m_bVehicleHomingEnabled)
	bool m_bVehicleHomingForceDisabled;

#if USE_TARGET_SEQUENCES
	// Helper for picking interesting target positions to shoot at when aiming at peds using lock on
	CTargetSequenceHelper m_TargetSequenceHelper;
#endif // USE_TARGET_SEQUENCES

	//
	// Fine aiming
	//

	// The current fine aim offset from the target position
	Vector3 m_vFineAimOffset;

	// The time spent fine aiming
	float m_fTimeFineAiming;

	// Time spent locked on.
	float m_fTimeLockedOn;

	// Hor Fine Aim weight (to increase speed as we hold stick down).
	float m_fFineAimHorizontalSpeedWeight;

	// Whether we are currently fine aiming
	bool m_bPlayerIsFineAiming : 1;
	bool m_bPlayerReleasedUpDownControlForFineAim : 1;
	bool m_bPlayerPressingUpDownControlForFineAim : 1;

	// Has attempted non spring fine aim (indicates we have moved from initial position).
	bool m_bHasAttemptedNonSpringFineAim : 1;

	// Indicates that we manually switched to our current target.
	bool m_bTargetSwitchedToLockOnTarget : 1;

	//
	// Soft aiming
	//

	// Whether we have started to aim
	bool m_bPlayerHasStartedToAim : 1;
	
	// Have we updated the lockon target position since we first had one
	bool m_bHasUpdatedLockOnPos : 1;

	// Check for a target to the left
	bool m_bCheckForLeftTarget : 1;

	// Check for a target to the right
	bool m_bCheckForRightTarget : 1;

	// Has lockon been broken since we started to aim
	bool m_bPlayerHasBrokenLockOn : 1;

	// Have we been within the limits of fine aim since we started to aim
	bool m_bWasWithinFineAimLimits : 1;

	// Check if we can switch targets after lock on has finished.
	bool m_bCheckForLockOnSwitchExtension : 1;

	// Time started lock on switch extension.
	u32 m_uTimeLockOnSwitchExtensionExpires;

	// Time we last soft fired, but didn't aim.
	u32 m_uTimeSinceSoftFiringAndNotAiming;

	// The entity we will use as the base for lock on switching.
	RegdEnt m_pLockOnSwitchExtensionTarget;

	// How long the player has been soft locked onto the target
	float m_fTimeSoftLockedOn;

	// How long the player has been trying to break soft lock
	float m_fTimeTryingToBreakSoftLock;

	// How long the player has been trying to break soft lock
	float m_fLostLockLOSTimer;

	// Keep track of how often we do the test for LOS.
	u32 m_uLastLockLOSTestTime;

	// Time we were last aiming.
	u32 m_uTimeSinceLastAiming;

	//
	// Free aiming
	//

	// The entity under our reticule
	RegdEnt m_pFreeAimTarget;

	// The time we have been aiming at the same free aim target for
	u32 m_uStartTimeAimingAtFreeAimTarget;

	// The ragdoll under our reticule - primarily used to hud health display
	RegdEnt m_pFreeAimTargetRagdoll;

	// The entity near our reticule that is a threat, we'll use this to base the aim assist calculations on
	RegdEnt m_pFreeAimAssistTarget;

	// The point that we detected the free aim assist target
	Vector3 m_vFreeAimAssistTargetPos;
	Vector3 m_vClosestFreeAimAssistTargetPos;

	// Reports if the the closest free aim target is a vehicle.
	// This is specially configured for use in the camera system and may not be suitable for general use.
	bool m_bFreeAimTargetingVehicleForCamera;

	// The current blend value between curve sets
	float m_fCurveSetBlend;

	// The last value passed to GetScaledLookAroundInputMagForCamera
	float m_fLastCameraInput;

	// Asynchronous probe result storage
	WorldProbe::CShapeTestResults m_FreeAimProbeResults;
	WorldProbe::CShapeTestResults m_FreeAimAssistProbeResults;

	// No reticule aim assist has been enabled.
	bool m_bNoReticuleAimAssist;
	bool m_bBlockNoReticuleAimAssist;

	static const int s_iMaxTargetableEntities = 6;
	atFixedArray<RegdEnt, s_iMaxTargetableEntities>  m_TargetableEntities;

	//
	// Static members
	//

	static const u8   MAX_FREE_AIM_ASYNC_RESULTS;
	static CCurveSet* GetCurveSet(s32 iCurveType);

	static CCurveSet* GetAimDistanceScaleCurve();

	static ePlayerTargetLevel ms_ePlayerTargetLevel;

};

#endif // PLAYER_PED_TARGETING_H
