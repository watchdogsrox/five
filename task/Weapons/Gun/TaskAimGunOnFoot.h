//
// Task/Weapons/Gun/TaskAimGunOnFoot.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_AIM_GUN_ON_FOOT_H
#define INC_TASK_AIM_GUN_ON_FOOT_H

////////////////////////////////////////////////////////////////////////////////

// Game headers
#include "Peds/Ped.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/Gun/GunIkInfo.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/WeaponController.h"

#if RSG_ORBIS || RSG_DURANGO || RSG_PC
	#define ENABLE_GUN_PROJECTILE_THROW 1
#else
	#define ENABLE_GUN_PROJECTILE_THROW 0
#endif

#if ENABLE_GUN_PROJECTILE_THROW
	# define GUN_PROJECTILE_THROW_ONLY(x) x
#else 
	# define GUN_PROJECTILE_THROW_ONLY(x) 
#endif

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CTaskMotionAiming;

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls weapon aiming and firing (Controls animations from move network
//			associated with CTaskMotionAiming)
////////////////////////////////////////////////////////////////////////////////

class CTaskAimGunOnFoot : public CTaskAimGun
{
public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float	m_MinTimeBetweenFiringVariations;
		float	m_IdealPitchForFiringVariation;
		float	m_MaxPitchDifferenceForFiringVariation;
		float	m_AssistedAimOutroTime;
		float	m_RunAndGunOutroTime;
		float	m_AimOutroTime;
		float	m_AimOutroTimeIfAimingOnStick;
		float	m_AimOutroMinTaskTimeWhenRunPressed;
		float   m_AimingOnStickExitCooldown;
		float   m_TimeForRunAndGunOutroDelays;
		float	m_DampenRootTargetWeight;
		float	m_DampenRootTargetHeight;
		float	m_AlternativeAnimBlockedHeight;
		Vector3 m_CoverAimOffsetFromBlocked;
		u32		m_DelayTimeWhenOutOfAmmoInScope;

		PAR_PARSABLE;
	};
	
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvStateId ms_FireOnlyStateId;
	static const fwMvStateId ms_FiringStateId;
	static const fwMvStateId ms_AimingStateId;
	static const fwMvStateId ms_AimStateId;
	static const fwMvFlagId ms_FiringFlagId;
	static const fwMvFlagId ms_HasCustomFireLoopFlagId;
	static const fwMvFlagId ms_UseIntroId;
	static const fwMvFlagId ms_FireLoop1FinishedCacheId;
	static const fwMvFlagId ms_FireLoop2FinishedCacheId;
	static const fwMvBooleanId ms_AimOnEnterStateId;
	static const fwMvBooleanId ms_FiringOnEnterStateId;
	static const fwMvBooleanId ms_AimIntroOnEnterStateId;
	static const fwMvBooleanId ms_EnteredFireLoopStateId;
	static const fwMvBooleanId ms_AimingClipEndedId;
	static const fwMvBooleanId ms_FidgetClipEndedId;
	static const fwMvBooleanId ms_FidgetClipNonInterruptibleId;
	static const fwMvBooleanId ms_FireId;
	static const fwMvBooleanId ms_InterruptSettleId;
	static const fwMvBooleanId ms_SettleFinishedId;
	static const fwMvBooleanId ms_FireInterruptable;
	static const fwMvBooleanId ms_FireLoop1StateId;
	static const fwMvBooleanId ms_FireLoop2StateId;
	static const fwMvBooleanId ms_FPSAimIntroTransition;
	static const fwMvBooleanId ms_AimIntroInterruptibleId;
	static const fwMvBooleanId ms_MoveRecoilEnd;
	static const fwMvBooleanId ms_FireLoopRevolverEarlyOutReload;
	static const fwMvClipId ms_AimingClipId;
	static const fwMvClipId ms_CustomFireLoopClipId;
	static const fwMvClipId ms_FireMedLoopClipId;
	static const fwMvRequestId ms_RestartAimId;
	static const fwMvRequestId ms_UseSettleId;
	static const fwMvRequestId ms_EmptyFire;
	static const fwMvFloatId ms_FireRateId;
	static const fwMvFloatId ms_AnimRateId;
	static const fwMvFloatId ms_AimingBreathingAdditiveWeight;
	static const fwMvFloatId ms_AimingLeanAdditiveWeight;
	static const fwMvFloatId ms_FPSIntroBlendOutTime;
	static const fwMvFloatId ms_FPSIntroPlayRate;

	static const float ms_fTargetDistanceFromCameraForAimIk;

    static const unsigned MAX_ROLL_TOKEN = 8;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: FSM states
	enum eAimGunOnFootState
	{
		State_Start = 0,
		State_Aiming,
		State_Firing,
		State_FireOutro,
		State_AimToIdle,
		State_AimOutro,
		State_Blocked,
		State_CombatRoll,
#if ENABLE_GUN_PROJECTILE_THROW
		State_ThrowingProjectileFromAiming,
#endif	//ENABLE_GUN_PROJECTILE_THROW
		State_Finish,
	};

	CTaskAimGunOnFoot(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target, const CGunIkInfo& ikInfo, const u32 uBlockFiringTime);

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_ON_FOOT; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskAimGunOnFoot(m_weaponControllerType, m_iFlags, m_iGunFireFlags, m_target, m_ikInfo, m_uBlockFiringTime); }

	// PURPOSE: Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual void UpdateClonedSubTasks(CPed* pPed, int const iState);

	// PURPOSE: Should we fire the weapon
	virtual bool ShouldFireWeapon();

	// PURPOSE: Aim gun implementation
	virtual bool GetIsReadyToFire() const; 
	virtual bool GetIsFiring() const; 
	virtual bool GetIsUsingFiringVariation() { return m_bUsingFiringVariation; }
	virtual bool GetIsWeaponBlocked() const { return GetState() == State_Blocked; }
	bool GetWillFire() const { return m_bFire; }
	void ResetWillFire() { m_bFire = false; }
	
	void SetCrouchStateChanged(bool bCrouchStateChanged) { m_bCrouchStateChanged = bCrouchStateChanged; }
	void SetForceRestart(bool b) { m_bForceRestart = b; m_iFlags.ClearFlag(GF_InstantBlendToAim); }

	// Block firing until the time is up (milliseconds)
	void SetBlockFiringTime(u32 uBlockFiringTime) { m_uBlockFiringTime = uBlockFiringTime; }

	// PURPOSE: Is ped reloading
	bool IsReloading() const { return m_bIsReloading; }

	// PURPOSE: Send firing signals, set variables
	void SetupToFire(const bool bForceFiringState = false);

	//
	void SetForceCombatRoll(const bool bForceRoll) { m_bForceRoll = bForceRoll; }

	CObject* GetWeaponObject() const { return m_WeaponObject; }

	// PURPOSE: Get tunables
	static Tunables& GetTunables() { return sm_Tunables; }

	// PURPOSE: Equips the backed-up weapon and destroys the dummy weapon object used for the grenade throw
	static void RestorePreviousWeapon(CPed *pPed, CObject* pWepObject, int iAmmoInClip);

	bool GetTempWeaponObjectHasScope() const { return  m_bTempWeaponHasScope; }

	static void PostCameraUpdate(CPed* pPed, const bool bDisableTorsoIk);

	bool GetWaitingToSwitchWeapon() const;

	// PURPOSE: Returns if fire anim is currently in an interruptible state
	bool GetIsFireInterruptible() const { return m_bMoveFireInterruptable; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE:	If this function returns true, the task says it's OK to delete if it gets aborted,
	//			because it lacks the ability to resume.
	virtual bool MayDeleteOnAbort() const { return true; }	// Won't resume, OK to delete.

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Clone task implementation
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

    // PURPOSE: Returns whether this task can be run on a clone ped based on it's current state
    virtual bool IsInScope(const CPed* pPed);

	// PURPOSE: Returns the distance from the local player at which the clone task will be processed
	virtual float GetScopeDistance() const;

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Called after the camera system update
	virtual bool ProcessPostCamera();

	// PURPOSE: Called after animation and Ik
	virtual bool ProcessPostPreRender();

	virtual bool ProcessMoveSignals();

	// PURPOSE: Update AIs blocked flag
	void UpdateAIBlockStatus(CPed& ped);

	// PURPOSE: Gets the line of sight status to the target
	LosStatus GetTargetLos(CPed& ped, bool& bBlockedNear);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	Start_OnExit();
	FSM_Return	Aiming_OnEnter();
	FSM_Return	Aiming_OnUpdate();
	FSM_Return	Aiming_OnExit();
	bool		Aiming_OnProcessMoveSignals();
	FSM_Return	Firing_OnEnter();
	FSM_Return	Firing_OnUpdate();
	FSM_Return	Firing_OnExit();
	FSM_Return	FireOutro_Update();
	bool		Firing_OnProcessMoveSignals();
	FSM_Return	AimToIdle_OnEnter();
	FSM_Return	AimToIdle_OnUpdate();
	FSM_Return	AimToIdle_OnExit();
	FSM_Return	AimOutro_OnUpdate();
	FSM_Return	Blocked_OnEnter();
	FSM_Return	Blocked_OnUpdate();
	FSM_Return	CombatRoll_OnEnter();
	FSM_Return	CombatRoll_OnUpdate();
#if ENABLE_GUN_PROJECTILE_THROW
	FSM_Return  ThrowingProjectileFromAiming_OnEnter();
	FSM_Return  ThrowingProjectileFromAiming_OnUpdate();
#endif	//ENABLE_GUN_PROJECTILE_THROW

	////////////////////////////////////////////////////////////////////////////////
	
	// PURPOSE: Gets the current pitch
	float GetCurrentPitch() const;

	void SetClothPackageIndex(u8 packageIndex, u8 transitionFrames = 0);

	// PURPOSE: Returns the current weapon controller state for the specified ped
	virtual WeaponControllerState GetWeaponControllerState(const CPed* pPed) const;

	// PURPOSE: Request aim state in the motion aiming network
	void RequestAim();

	// PURPOSE: Check if we entered the aim state in the move network
	bool IsInAimState();

	// PURPOSE: Check if we entered the firing state in the move network
	bool IsInFiringState();

	// PURPOSE: Set a flag on the motion aiming network
	void SetMoveFlag(const bool bVal, const fwMvFlagId flagId);
	
	// PURPOSE: Set a clip on the motion aiming network
	void SetMoveClip(const crClip* pClip, const fwMvClipId clipId);

	// PURPOSE: Set a float on the motion aiming network
	void SetMoveFloat(const float fVal, const fwMvFloatId floatId);

	// PURPOSE: Set a bool on the motion aiming network
	void SetMoveBoolean(const bool bVal, const fwMvBooleanId boolId);

	// PURPOSE: Get a bool from the motion aiming move network
	bool GetMoveBoolean(const fwMvBooleanId boolId) const;

	// PURPOSE: Trigger a fire on the motion aiming network
	void TriggerFire();

	// PURPOSE: Store whether we are crouching
	void UpdateCrouching(const CPed* pPed) { m_bCrouching = pPed->GetIsCrouching(); } 

	// PURPOSE: Should we trigger a reload 
	bool ShouldReloadWeapon() const;

	bool ProcessShouldReloadWeapon();

	// PURPOSE: Check is out of ammo
	bool IsOutOfAmmo() const;

	// PURPOSE: Check for fire loop end
	bool CheckLoopEvent();

	// PURPOSE: Get the motion aiming task
	CTaskMotionAiming* FindMotionAimingTask();
	const CTaskMotionAiming* FindMotionAimingTask() const;

	// PURPOSE: Get the aiming network helper
	CMoveNetworkHelper* FindAimingNetworkHelper();
	
	// PURPOSE: Chooses a clip for the firing variations
	const crClip* ChooseFiringVariationClip() const;

	// PURPOSE: Chooses the mid clip
	const crClip* ChooseFiringMedClip() const;

	// PURPOSE: Gets the clip set for the firing variations
	fwMvClipSetId GetFiringVariationsClipSet() const;
	
	// PURPOSE: Handle streaming requests
	void ProcessStreaming();
	
	// PURPOSE: Helper function to determine if the min time has expired before firing
	bool ShouldBlockFireDueToMinTime() const;
	
	// PURPOSE: Helper function to determine if we should use a firing variation
	bool ShouldUseFiringVariation() const;

	// PURPOSE: Updates everything related to the LOS probe (timer, results, etc)
	void UpdateLosProbe();

	// PURPOSE: Allowed to perform heading updates
	static bool CanSetDesiredHeading(const CPed* pPed);

	// 
	void UpdateAdditiveWeights(const float fDesiredBreathingAdditiveWeight, const float fDesiredLeanAdditiveWeight);

	bool IsAimingScopedWeapon() const;
	
	// PURPOSE: Process if any glass intersecting the weapon that needs smashing
	void ProcessSmashGlashFromBlocked(CPed* pPed);

#if ENABLE_GUN_PROJECTILE_THROW
	// PURPOSE: Returns true if the player can throw a grenade from aiming
	bool ShouldThrowGrenadeFromGunAim(CPed *pPed, const CWeaponInfo* pWeaponInfo);
	// PURPOSE: Allows player to throw a grenade from State_Aiming or State_Firing
	bool ProcessGrenadeThrowFromGunAim(CPed *pPed, CWeapon* pWeapon);
#endif	// ENABLE_GUN_PROJECTILE_THROW

	// If able to start spin up
	bool CanSpinUp() const;

	// PURPOSE: Helper to stream firing variations
	CPrioritizedClipSetRequestHelper m_FiringVariationsClipSetRequestHelper;

	// Time until we can fire
	u32 m_uBlockFiringTime;

	// PURPOSE: Is ped crouching
	bool m_bCrouching : 1;

	// PURPOSE: Does ped need to fire
	bool m_bFire : 1;

	// PURPOSE: Ped has fired once in the fire state
	bool m_bFiredOnce : 1;

	// PURPOSE: Ped has fired twice in the fire state
	bool m_bFiredTwice : 1;

	// PURPOSE: Ped has fired once this trigger press
	bool m_bFiredOnceThisTriggerPress : 1;

	// PURPOSE: Ped is reloading
	bool m_bIsReloading : 1;

	// PURPOSE: Crouch state change
	bool m_bCrouchStateChanged : 1;
	bool m_bForceRestart : 1;

	// PURPOSE: Used in MP to force the task to fire
	bool m_bForceFire : 1;

	// PURPOSE: Ignore any fire event next frame
	bool m_bSuppressNextFireEvent : 1;

	// PURPOSE: Set when we are using a firing variation
	bool m_bUsingFiringVariation : 1;

	// PURPOSE: Have we done the intro?
	bool m_bDoneIntro : 1;

	// PURPOSE: If we should fire but waiting for the weapon to be ready then fire when next available 
	bool m_bWantsToFireWhenWeaponReady : 1;

	// PURPOSE: Flag if we have already setup the signals for firing
	bool m_bDoneSetupToFire : 1;

	// PURPOSE: The firing state has been forced externally
	bool m_bForceFiringState : 1;

	//
	bool m_bForceRoll : 1;

	bool m_bMoveInterruptSettle : 1;
	bool m_bMoveFire : 1;
	bool m_bMoveFireLoop1 : 1;
	bool m_bMoveFireLoop2 : 1;
	bool m_bMoveCustomFireLoopFinished : 1;
	bool m_bMoveCustomFireIntroFinished : 1;
	bool m_bMoveCustomFireOutroFinished : 1;
	bool m_bMoveCustomFireOutroBlendOut : 1;
	bool m_bMoveDontAbortFireIntro : 1;
	bool m_bMoveFireLoop1Last : 1;
	bool m_bMoveFireInterruptable : 1;
	bool m_bMoveAimIntroInterruptible : 1;
	bool m_bMoveRecoilEnd : 1;
	bool m_bMoveFireLoopRevolverEarlyOutReload : 1;

	// PURPOSE: Ik constraints
	CGunIkInfo m_ikInfo;

	// Target probe variables. Cached result, timer for when to start next probe and probe helper
	CAsyncProbeHelper	m_asyncProbeHelper;
	CTaskTimer			m_probeTimer;
	bool				m_bLosClear;

	// Used in MP preserves interval time so repeat fire rate is correct
	u32 m_uNextShotAllowedTime;

	// Used to control how long it takes to exit aim outro after giving up aim stick use.
	u32 m_uLastUsingAimStickTime;

	// Used in MP to get time controller is held on remote
	mutable bool m_bCloneWeaponControllerFiring;

    // Used in MP to ensure we don't roll too many times
    u8  m_LastLocalRollToken;
    u8  m_LastCloneRollToken;
	bool m_bUseAlternativeAimingWhenBlocked : 1;

    // Used in MP to ensure we don't throw grenade too many times
    u32 m_LastThrowProjectileSequence;

	// 
	float m_fBreathingAdditiveWeight;
	float m_fLeanAdditiveWeight;
	float m_fTimeTryingToFire;
	RegdObj m_WeaponObject;

	bool m_bHasSetQuitTimeWhenOutOfAmmoInScope;
	u32 m_uQuitTimeWhenOutOfAmmoInScope;

	bool m_bOnCloneTaskNoLongerRunningOnOwner; 

#if ENABLE_GUN_PROJECTILE_THROW
	// PURPOSE: Stores weapon hash of weapon used before doing a projectile quick throw.
	u32 m_uQuickThrowWeaponHash;

	// PURPOSE: True if temp weapon object (used for gun grenade throw) has a scope attachment (used to decide what throw clip to use for weapons like the combat_mg)
	bool m_bTempWeaponHasScope;

	// PURPOSE: Flag used to let us know if we have thrown from pressing left DPad (not an up swipe on PS4). Passed through to CTaskAimAndThrowProjectile.
	bool m_bProjectileThrownFromDPadInput;
#endif	//ENABLE_GUN_PROJECTILE_THROW

	s32 m_BulletsLeftAtStartOfFire;

#if FPS_MODE_SUPPORTED
	u32 m_iFPSSwitchToScopeStartTime;
#endif // FPS_MODE_SUPPORTED

private:

	static Tunables sm_Tunables;

	////////////////////////////////////////////////////////////////////////////////

protected:
	// PURPOSE: Set when trigger is hammered, rather than being held down, to fire pistols rapidly
	bool m_bFireWhenReady;
	// PURPOSE: Storage for previous weapon controller state
	WeaponControllerState m_previousControllerState;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Aim gun on foot task information for cloned peds
////////////////////////////////////////////////////////////////////////////////
class CClonedAimGunOnFootInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedAimGunOnFootInfo();
	CClonedAimGunOnFootInfo(s32 aimGunOnFootState, u8 fireCounter, u8 ammoInClip, bool bWeaponControllerFiring, u32 seed, u8 rollToken, bool bUseAlternativeAimingWhenBlocked, bool bCombatRoll, bool bForwardRoll, float fRollDirection);
	~CClonedAimGunOnFootInfo() {}

	virtual s32		GetTaskInfoType( ) const {return INFO_TYPE_AIM_GUN_ON_FOOT;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return SIZEOF_AIM_GUN_ON_FOOT_STATUS;}
	virtual const char*	GetStatusName(u32) const { return "Status";}
	
	u8 GetFireCounter() const { return m_fireCounter; }
	u8 GetAmmoInClip()  const { return m_iAmmoInClip; }
    u8 GetRollToken()   const { return m_RollToken;   }

	bool IsCombatRollActive() const { return m_bCombatRoll; }
	bool GetRollForward() const { return m_bRollForward; }
	float GetRollDirection() const { return m_fRollDirection; }

	bool	GetWeaponControllerFiring()	const {return m_bWeaponControllerFiring; }

	u32 GetSeed() const { return m_seed; }

	bool GetUseAlternativeAimingWhenBlocked() const { return m_bUseAlternativeAimingWhenBlocked; }

	void Serialise(CSyncDataBase& serialiser);

private:

	static const unsigned int SIZEOF_AIM_GUN_ON_FOOT_STATUS = 4;

	float	m_fRollDirection;
	u32		m_seed;
	u8		m_fireCounter;
	u8		m_iAmmoInClip;
    u8      m_RollToken;
	bool	m_bWeaponControllerFiring;
	bool    m_bHasSeed;
	bool	m_bUseAlternativeAimingWhenBlocked;
	bool	m_bRollForward;
	bool	m_bCombatRoll;
};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_TASK_AIM_GUN_ON_FOOT_H
