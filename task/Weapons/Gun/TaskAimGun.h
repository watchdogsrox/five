#ifndef TASK_AIM_GUN_H
#define TASK_AIM_GUN_H

// Game headers
#include "Task/System/TaskFSMClone.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/WeaponTarget.h"

// Forward declarations
class CFiringPatternData;
class CWeaponInfo;

//////////////////////////////////////////////////////////////////////////
// CTaskAimGun
//////////////////////////////////////////////////////////////////////////

class CTaskAimGun : public CTaskFSMClone
{
public:

	static bool				ms_bUseTorsoIk;
	static bool				ms_bUseLeftHandIk;
	static bool				ms_bUseIntersectionPos;

#if __BANK
	
	static bool				ms_bUseAltClipSetHash;
	static bool				ms_bUseOverridenYaw;
	static bool				ms_bUseOverridenPitch;
	static float			ms_fOverridenYaw;
	static float			ms_fOverridenPitch;

	static bool				ms_bRenderTestPositions;
	static bool				ms_bRenderYawDebug;
	static bool				ms_bRenderPitchDebug;
	static bool				ms_bRenderDebugText;
	static float			ms_fDebugArcRadius;

	static Vector3			ms_vGoToPosition;
	static CEntity*			ms_pAimAtEntity;
	static CEntity*			ms_pTaskedPed;
	static void				SetGoToPosCB();
	static void				SetAimAtEntityCB();
	static void				SetTaskedEntityCB();
	static void				GivePedTaskCB();

	// Draws debug to show the yaw and pitch sweep values
	static void				RenderYawDebug(const CPed* pPed, Vec3V_In vAimFromPosition, const float fDesiredYawAngle,  float fMinYawSweep, float fMaxYawSweep);
	static void				RenderPitchDebug(const CPed* pPed, Vec3V_In vAimFromPosition, const float fDesiredYawAngle, const float fDesiredPitchAngle, float fMinPitchSweep, float fMaxPitchSweep);
#endif

	static bool				ShouldUseAimingMotion(const CPed* pPed);
	static bool				ShouldUseAimingMotion(const CWeaponInfo& weaponInfo, fwMvClipSetId appropriateWeaponClipSetId, const CPed* pPed);

	CTaskAimGun(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target);

	// Static Helper Functions for new gun tasks

	// Calculates the yaw angle required to rotate by from the start position to point to the end position
	static float CalculateDesiredYaw(const CPed* pPed, const Vector3& vStart, const Vector3& vEnd);

	// Calculates the pitch angle required to rotate by to point the gun at the target position
	static float CalculateDesiredPitch(const CPed* pPed, const Vector3& vStart, const Vector3& vEnd, bool bUntranformRelativeToPed = false);

	// Maps a radian angle in the range [fMinValue, fMaxValue] to the range [0.0,1.0] with the option to invert in case the 
	// clips are authored the opposite direction
	static float ConvertRadianToSignal(float fRadianValue, float fMinValue, float fMaxValue, bool bInvert = false);

	// To provide smooth transitions between signals
	static float SmoothInput(float fCurrentValue, float fDesiredValue, float fMaxChangeRate);

	static bool	 ComputeAimMatrix(const CPed* pPed, Matrix34& mAimMat);

	// Updates the accuracy based on what the target is doing
	static void UpdatePedAccuracyModifier(CPed* pPed, const CEntity* pTargetEntity);

	// Calculate the gun firing vector
	virtual bool CalculateFiringVector(CPed* pPed, Vector3& vStart, Vector3& vEnd, CEntity *pCameraEntity = 0, float* fTargetDistOut = NULL, bool bAimFromCamera = false) const;

	virtual bool ShouldFireWeapon() { return false; } 
	virtual bool GetIsWeaponBlocked() const { return false; }
	
#if 0 // example vector math conversion
	virtual bool CalculateFiringVectorV(CPed* pPed, Vec3V_InOut vStart, Vec3V_InOut vEnd, CEntity *pCameraEntity = 0) const;
#endif 

	//
	// Settors
	//

	// Sets the target data
	void SetTarget(const CWeaponTarget& target)                                        { m_target = target; }

	// Overrides the clip set id used
	void SetOverrideAimClipId(const fwMvClipSetId &overrideClipSetId, const fwMvClipId &overrideClipId)  { m_overrideAimClipSetId  = overrideClipSetId; m_overrideAimClipId  = overrideClipId; }
	void SetOverrideFireClipId(const fwMvClipSetId &overrideClipSetId, const fwMvClipId &overrideClipId) { m_overrideFireClipSetId = overrideClipSetId; m_overrideFireClipId = overrideClipId; }
	void SetOverrideClipSetId(const fwMvClipSetId &overrideClipSetId) { m_overrideClipSetId = overrideClipSetId; }
	fwMvClipSetId GetOverrideAimClipId() const { return m_overrideAimClipSetId; }
	fwMvClipSetId GetOverrideFireClipId() const { return m_overrideFireClipSetId; }
	fwMvClipSetId GetOverrideClipSetId() const { return m_overrideClipSetId; }

	void SetScriptedGunTaskInfoHash(u32 uHash) { m_uScriptedGunTaskInfoHash = uHash; }

	void SetWeaponControllerType(const CWeaponController::WeaponControllerType type) { m_weaponControllerType = type; }

	//
	// Accessors
	//

	virtual bool GetIsReadyToFire() const = 0;
	virtual bool GetIsFiring() const = 0;
	const fwFlags32& GetGunFlags() const { return m_iFlags; }
	fwFlags32& GetGunFlags() { return m_iFlags; }

	const fwFlags32& GetGunFireFlags() const { return m_iGunFireFlags; }
	fwFlags32& GetGunFireFlags() { return m_iGunFireFlags; }

	const CWeaponTarget& GetTarget() const { return m_target; }
	CWeaponTarget& GetTarget() { return m_target; }

	u8 GetFireCounter() const { return m_fireCounter; }

	// Fire the gun and perform related firing tasks
	bool FireGun(CPed* pPed, bool bDriveBy = false, bool bUseVehicleAsCamEntity = false);

protected:

	virtual FSM_Return ProcessPreFSM();

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// Work out the playback rate for the fire anim
	float GetFiringAnimPlaybackRate() const;

	// Returns the current weapon controller state for the specified ped
	virtual WeaponControllerState GetWeaponControllerState(const CPed*) const { return WCS_None; }

	// Check if the vector to the target position is blocked by an obstruction
	bool IsWeaponBlocked(CPed* pPed, bool bIgnoreControllerState = false, float sfProbeLengthMultiplier = 1.0f, float fRadiusMultiplier = 1.0f, bool bCheckAlternativePos = false, bool bWantsToFire = false, bool bGrenadeThrow = false);

	// Check if we can fire at target (i.e. they aren't friendly).
	bool CanFireAtTarget(CPed *pPed, CPed *pForceTarget = NULL) const;

private:

	// Perform VO when firing
	void DoFireVO(CPed* pPed);

private:

	// Decides whether the shot should be a blank
	bool ShouldFireBlank(CPed* pPed);

//---------------

protected:

	//
	// Members
	//

	// Gun controller
	CWeaponController::WeaponControllerType m_weaponControllerType;

	// Further control of what the task does
	fwFlags32 m_iFlags;
	fwFlags32 m_iGunFireFlags;

	//
	// Targeting
	//

	CWeaponTarget m_target;

	//
	// Shooting
	//

	// How many blanks have been fired in succession
	s16 m_iBlanksFired;

	//
	// Animation overrides
	//

	// Overridden clip set id, so we will use this one rather than the one from the weapon
	fwMvClipSetId m_overrideAimClipSetId;
	fwMvClipId m_overrideAimClipId;
	fwMvClipSetId m_overrideFireClipSetId;
	fwMvClipId m_overrideFireClipId;
	fwMvClipSetId m_overrideClipSetId;

	u32 m_uTimeInStandardAiming; 
	u32 m_uTimeInAlternativeAiming;

	// used in MP for shotgun spread pattern
	u32 m_seed;

	u32	m_uScriptedGunTaskInfoHash;

	// The firing animation playback rate
	float m_fFireAnimRate;

	// Force the weapon to instantly be able to fire if we need to
	bool m_bForceWeaponStateToFire;

	bool m_bCancelledFireRequest;

	// used in MP, counts the number of times the gun has been fired. This is to used to prevent gun shots being missed in 
	// network updates when the fire state is only entered briefly and not sent in the update.
	u8 m_fireCounter;

	// used in MP for ammo before the shot is taken
	u8 m_iAmmoInClip;

#if __BANK
public:
	static void InitWidgets();
#endif

private:

	// Private copy constructor
	CTaskAimGun(const CTaskAimGun& other);
};

#endif // TASK_AIM_GUN_H
