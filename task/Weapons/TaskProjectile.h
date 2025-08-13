#ifndef TASK_PROJECTILE_H
#define TASK_PROJECTILE_H

#include "network/General/NetworkUtil.h"
#include "peds/QueriableInterface.h"
#include "Scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task\System\TaskFsmClone.h"
#include "Task\System\TaskTypes.h"
#include "Task/Weapons/Gun/GunIkInfo.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/WeaponTarget.h"
#include "fwmaths/Angle.h"
#if !__FINAL
#include "atl/string.h"
#endif

class CWeapon;

//////////////////////////////////////////////////////////////////////////
// CThrowProjectileHelper
//////////////////////////////////////////////////////////////////////////
class CThrowProjectileHelper
{
public:

	// Works out an aiming trajectory from the start to the end pos
	static bool CalculateAimTrajectory(CPed* pPed, const Vector3& vStart, const Vector3& vTarget, const Vector3& vTargetVelocity, Vector3& vTrajectory);

	// Get the world space position where the projectile will be detached from the peds hand
	static Vector3 GetPedThrowPosition(CPed* pPed, const fwMvClipId &clipId);

	// Query the peds weapon and get the launch velocity from it
	static float GetProjectileVelocity(CPed* pPed);

	// Is the player pressing the trigger enough?
	static bool IsPlayerFiring(const CPed* pPed);
	static bool IsPlayerPriming(const CPed* pPed);
	static bool IsPlayerFirePressed(const CPed* pPed);

	//Combat roll
	static bool IsPlayerRolling(const CPed* pPed);

	// Has the ped held onto the projectile too long?
	static bool HasHeldProjectileTooLong(CPed* pPed, float fTimeHeld);

	// Set the weapon slot on the peds weapon mgr after throwing a projectile
	static void SetWeaponSlotAfterFiring(CPed* pPed);

private:

	// Static helper function used to calculate the trajectory needed to hit a 
	// particular target.  To narrow down the possible trajectory options, use a predefined
	// 2 dimensional velocity to calculate the best trajectory, this should give only 1 solution.
	// First using a desired 2d distance calculate a time to impact, then calculate
	// the maximum arc based on the time to impact.  Then finally calculate the 
	// vertical velocity component from the height of the arc and the time to impact.
	// If an impact is found, the best way of varying the trajectory is by varying the speed
	static float CalculateAimTrajectoryWithSpeed(const Vector3& vStart, const Vector3& vTarget, const float fSpeed, Vector3& vTrajectory);

	// Get the local space position where the projectile will be detached from the peds hand
	static Vector3 GetThrowClipOffset(CPed* pPed, const fwMvClipId &clipId);
};

// Whether the object should be thrown long, short, or at all after this task has terminated
enum eThrowType
{
	THROW_INVALID = -1,
	THROW_UNDERARM,
	THROW_UNDERARM_STAND,
	THROW_SHORT,
	THROW_LONG,
	THROW_DROP,
	THROW_CROUCHED
};

//////////////////////////////////////////////////////////////////////////
// CTaskAimAndThrowProjectile
//////////////////////////////////////////////////////////////////////////

class CTaskAimAndThrowProjectile : public CTaskFSMClone
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		bool m_bEnableGaitAdditive;
		float m_fMinHoldThrowPitch;
		u32 m_iMaxRandomExplosionTime;

		PAR_PARSABLE;
	};

public:

	enum eState
	{		
		State_Start,
		State_StreamingClips,
		State_Intro,
		State_PullPin,
		State_HoldingProjectile,
		State_ThrowProjectile,
		State_PlaceProjectile,
		State_CombatRoll,
		State_Reload,
		State_CloneIdle,
		State_Outro,
		State_Blocked,
		State_Finish,
	};

    static const unsigned MAX_ROLL_TOKEN = 8;

	 CTaskAimAndThrowProjectile(const CWeaponTarget& targeting = CWeaponTarget(), CEntity* pIgnoreCollisionEntity = NULL, bool bCreateInvincibileProjectile = false
#if ENABLE_GUN_PROJECTILE_THROW
		 , bool bAimGunAndThrow = false, int iAmmoInClip = -1, u32 uAimAndThrowWeaponHash = 0, bool bProjectileThrownFromDPadInput = false
#endif	//ENABLE_GUN_PROJECTILE_THROW
	);
	~CTaskAimAndThrowProjectile();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE; }    

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskAimAndThrowProjectile(m_target, m_pIgnoreCollisionEntity); }

	void UpdateTarget(CPed* pPed);

	// FSM required implementations
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return			ProcessPostFSM();
	virtual bool				ProcessPostPreRender();
	virtual bool				ProcessPostPreRenderAfterAttachments();
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	// Camera system interfaces
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	virtual void CleanUp();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32);
#endif

	//
	// Accessors
	//

	// Get projectile trajectory
	bool IsThrowing() const { return GetState() == State_ThrowProjectile; }
	bool IsDroppingProjectile() const { return m_bDropProjectile; }
	bool GetIsReadyToFire() const { return GetState() == State_HoldingProjectile; }	
	bool IsFalseAiming() const {return m_bFalseAiming;}
	const CWeaponTarget& GetTarget() const { return m_target; }
	bool GetFPSFidgetClipFinished() const { return m_bMoveFPSFidgetEnded; }

	bool		IsPriming(CPed* pPed);
	bool		CanPlaceProjectile(CPed* pPed);

	static Tunables& GetTunables() { return sm_Tunables; }	

	float SelectFPSBlendTime(CPed *pPed, bool bStateToTransition = true, CPedMotionData::eFPSState prevFPSStateOverride = CPedMotionData::FPS_MAX, CPedMotionData::eFPSState currFPSStateOverride = CPedMotionData::FPS_MAX);
	bool ShouldUseProjectileStrafeClipSet();
private:

	// Start
	FSM_Return Start_OnUpdate(CPed* pPed);

	void		StreamingClips_OnEnter(CPed* pPed);
	FSM_Return	StreamingClips_OnUpdate(CPed* pPed);
	// Intro	
	void		Intro_OnEnter(CPed* pPed);
	FSM_Return	Intro_OnUpdate(CPed* pPed);
#if FPS_MODE_SUPPORTED
	void Intro_OnExit(CPed *pPed);
#endif
	// PullPin	
	FSM_Return	PullPin_OnUpdate();
	// HoldingProjectile
	void		HoldingProjectile_OnEnter(CPed* pPed);
	FSM_Return	HoldingProjectile_OnUpdate(CPed* pPed);
	// ThrowProjectile
	void		ThrowProjectile_OnEnter(CPed* pPed);
	FSM_Return	ThrowProjectile_OnUpdate(CPed* pPed);
	void		ThrowProjectile_OnExit(CPed* pPed);
	// PlaceProjectile
	FSM_Return	StatePlaceProjectile_OnEnter(CPed* pPed);
	FSM_Return	StatePlaceProjectile_OnUpdate(CPed* pPed);
	// Combat Roll
	void		CombatRoll_OnEnter(CPed* pPed);
	FSM_Return	CombatRoll_OnUpdate(CPed* pPed);	
	// Reload
	void		Reload_OnEnter(CPed* pPed);
	FSM_Return	Reload_OnUpdate(CPed* pPed);
	void		Reload_OnExit(CPed* pPed);	
	// Clone idle
	void		CloneIdle_OnEnter(CPed* pPed);
	FSM_Return	CloneIdle_OnUpdate(CPed* pPed);
	// Outro
	void		Outro_OnEnter(CPed* pPed);
	FSM_Return	Outro_OnUpdate(CPed* pPed);

	//Blocked
	void		Blocked_OnEnter(CPed* pPed);
	FSM_Return	Blocked_OnUpdate(CPed* pPed);

	void		UpdateAiming(CPed* pPed, bool popIt = false);
	void		ThrowProjectileNow(CPed* pPed, CWeapon* pWeapon, Vector3 *pvOverrideStartPosition = NULL);

	bool		IsAiming(CPed* pPed);
	bool		IsFiring(CPed* pPed);

#if FPS_MODE_SUPPORTED
	bool		HandleFPSStateUpdate();
#endif


	void HoldPrimeMoveNetworkRestart(CPed* pPed, bool bPrime);
	void SetPriming(CPed* pPed, bool bHoldToPrime);
	void SetGripClip();
	bool HasValidWeapon(CPed* pPed) const;

	void CreateNetworkPlayer(fwMvNetworkDefId networkDefId);

	void UpdateAdditives();
	void ProcessThrowProjectile();

#if ENABLE_GUN_PROJECTILE_THROW
	// For Gun-Projectile Throws: creates dummy gun object in peds right hand while we throw the projectile.
	void CreateDummyWeaponObject(CPed *pPed, CWeapon *pWeapon, const CWeaponInfo *pWeaponInfo);
#endif	//ENABLE_GUN_PROJECTILE_THROW

	// Targeting 
	CWeaponTarget m_target;
	RegdEnt m_pIgnoreCollisionEntity;

	// Flags
	bool m_bPriming;

	// For Clones
	bool m_bIsAiming;
	bool m_bIsFiring; 
	bool m_bHasTargetPosition; 
	Vector3 m_vTargetPosition;

#if ENABLE_GUN_PROJECTILE_THROW
	// Flag for aiming gun & throwing projectile. Skips us to State_ThrowProjectile.
	bool m_bAimGunAndThrow;
	// Flag used to let us know if we have thrown from pressing left DPad (not an up swipe on PS4).
	bool m_bProjectileThrownFromDPadInput;
	// Stores the amount of ammo in the clip before throwing grenade.
	int m_iBackupWeaponClipAmmo;
	// Stores the projectile weapon hash we need to create when doing a gun-aim throw.
	u32 m_uAimAndThrowWeaponHash;
	// Temporary weapon object attached to right hand while throwing grenade. Destroyed in CleanUp().
	RegdObj m_pTempWeaponObject;
#endif	//ENABLE_GUN_PROJECTILE_THROW

	// PURPOSE: Ik constraints
	CGunIkInfo m_ikInfo;

	crFrameFilter*	m_pFrameFilter; 
	crFrameFilter*	m_pGripFrameFilter; 
	crFrameFilter*	m_pPullPinFilter;

	Vector3			m_vLastProjectilePos;
	static float	m_sfThrowingMainMoverCapsuleRadius;
	float			m_fCurrentPitch;
	float			m_fDesiredPitch;
	float			m_fDesiredThrowPitch;
	float			m_fThrowPitch;
	float			m_fThrowPhase;
	float			m_fReloadPhase;
	float			m_fInterruptPhase;
	float			m_fStrafeSpeed;
	float			m_fStrafeDirection;
	float			m_fStrafeDirectionSignal;
	float			m_fCurrentShadowWeight;
	float			m_fTimeMoving;
	float			m_fBlockAimingIndependentMoverTime;
	bool			m_bFire:1;	
	bool			m_bHasThrownProjectile:1;
	bool			m_bHasReloaded:1;
	bool			m_bDropProjectile:1;
	bool			m_bDropUnderhand:1;
	bool			m_bForceStandingThrow:1;
	bool			m_bFalseAiming:1;
	bool			m_bCanBePlaced:1;
	bool			m_bHasNoPullPin:1;
	bool			m_bPinPulled:1;
	bool			m_bWaitingToDie:1;
	bool			m_bMoveTreeIsDropMode:1;
	bool			m_bPrimedButPlacing:1;
	bool			m_bThrowPlace:1;	
	bool			m_bAllowPriming:1;
	bool			m_bWasAiming:1;
	bool			m_bPlayFPSTransition:1;
	bool			m_bSkipFPSOutroState:1;
	bool			m_bPlayFPSRNGIntro:1;
	bool			m_bPlayFPSPullPinIntro:1;
	bool			m_bPlayFPSFidget:1;
	bool			m_bMoveFPSFidgetEnded:1;
	bool			m_bFPSReachPinOnIntro:1;
	bool			m_bFPSWantsToThrowWhenReady:1;
	bool			m_bFPSUseAimIKForWeapon:1;
	bool			m_bFPSPlayingUnholsterIntro:1;
	bool			m_bCreateInvincibileProjectile:1;

	float			m_fFPSTaskNetworkBlendDuration;
	float			m_fFPSTaskNetworkBlendTimer;
	float			m_fFPSStateToTransitionBlendTime;
	float			m_fFPSTransitionToStateBlendTime;
	float			m_fFPSDropThrowSwitchTimer;

	float m_fStrafeSpeedAdditive;
	float m_fStrafeDirectionAdditive;

     // Used in MP to ensure we don't roll too many times
    u8              m_LastLocalRollToken;
    u8              m_LastCloneRollToken;

	// Cache
	bool m_bHasCachedWeaponValues:1; 

	// Time we have 'cooked' the projectile for (usually related to the time we have held aim down for). 0.0f is no cooking.	
	
	static const fwMvRequestId		ms_StartPrime;				
	static const fwMvRequestId		ms_StartReload;
	static const fwMvRequestId		ms_StartFinish;	
	static const fwMvRequestId		ms_StartOutro;	
	static const fwMvRequestId		ms_StartHold;	
	static const fwMvRequestId		ms_StartDropPrime;
	static const fwMvFilterId		ms_GripFilterId;
	static const fwMvFilterId		ms_FrameFilterId;	
	static const fwMvFilterId		ms_PullPinFilterId;
// FPS Mode
	static const fwMvRequestId		ms_FPSFidget;
	static const fwMvFlagId			ms_UsingFPSMode;
	static const fwMvFlagId			ms_HasBreatheHighAdditives;
	static const fwMvFlagId			ms_HasBwdAdditive;
// End FPS Mode
	static const fwMvFlagId			ms_SkipPullPinId;
	static const fwMvFlagId			ms_DoDropId;
	static const fwMvFlagId			ms_DropProjectileFlagId;
	static const fwMvFlagId			ms_SkipIntroId;
	static const fwMvFlagId			ms_OverhandOnlyId;
	static const fwMvFlagId			ms_UseThrowPlaceId;
	static const fwMvFlagId			ms_UseDropUnderhandId;
	static const fwMvFlagId			ms_NoStaticDropId;
	static const fwMvFlagId			ms_IsRollingId;
	static const fwMvFloatId		ms_PitchPhase;
	static const fwMvFloatId		ms_ThrowPitchPhase;
	static const fwMvFloatId		ms_ThrowPhase;
	static const fwMvFloatId		ms_ReloadPhase;
	static const fwMvFloatId		ms_TransitionPhase;	
	static const fwMvFloatId		ms_StrafeSpeed;	
	static const fwMvFloatId		ms_DesiredStrafeSpeed;	
	static const fwMvFloatId		ms_StrafeDirectionId;
	static const fwMvFloatId		ms_GaitAdditive;		
	static const fwMvFloatId		ms_GaitAdditiveTransPhase;
	static const fwMvFloatId		ms_ShadowWeight;
	static const fwMvFloatId		ms_PullPinPhase;
// FPS Mode
	static const fwMvFloatId		ms_AimingLeanAdditiveWeight;
	static const fwMvFloatId		ms_AimingBreathingAdditiveWeight;
	static const fwMvFlagId			ms_UseGripClip;
	static const fwMvFloatId		ms_StrafeSpeed_AdditiveId;
	static const fwMvFloatId		ms_StrafeDirection_AdditiveId;
// End FPS mode
	static const fwMvBooleanId		ms_ThrowCompleteEvent;		
	static const fwMvBooleanId		ms_ReloadCompleteEvent;
	static const fwMvBooleanId		ms_EnterHoldEvent;
	static const fwMvBooleanId		ms_EnterIntroEvent;	
	static const fwMvBooleanId		ms_EnterOutroEvent;
	static const fwMvBooleanId		ms_ReloadEvent;
// FPS Mode
	static const fwMvBooleanId		ms_FPSReachPin;
// End FPS mode
	static const fwMvClipSetVarId	ms_StreamedClipSet;
	static const fwMvFlagId			ms_AimGunThrowId;
	static const fwMvFlagId			ms_FPSCookingProjectile;

	fwClipSetRequestHelper	m_ClipSetRequestHelper;
	fwClipSetRequestHelper	m_StreamedClipSetRequestHelper;
	fwClipSetRequestHelper  m_StrafeClipSetRequestHelper;
	CMoveNetworkHelper		m_MoveNetworkHelper;
	fwMvNetworkDefId		m_NetworkDefId;
	fwMvClipSetId			m_iClipSet;
	fwMvClipSetId			m_iStreamedClipSet;

// FPS Mode
	CMoveNetworkHelper*      m_pFPSWeaponMoveNetworkHelper;

	static Tunables sm_Tunables;
};

//-------------------------------------------------------------------------
// Task info for aiming and throwing a projectile
//-------------------------------------------------------------------------
class CClonedAimAndThrowProjectileInfo : public CSerialisedFSMTaskInfo
{
public:

	  CClonedAimAndThrowProjectileInfo(CEntity* pIgnoreCollisionEntity, const u32 state, bool bPriming, bool bAiming, bool bFiring, bool bDropping, bool bThrowPlace, 
#if ENABLE_GUN_PROJECTILE_THROW
		  bool bAimGunAndThrow,
		  u32 uAimAndThrowWeaponHash,
#endif // ENABLE_GUN_PROJECTILE_THROW
		  float fThrowPitch, float fCurrentPitch, bool bHasTargetVector, const Vector3& vTarget, bool bHasAimTarget, u8 rollToken, const Vector3& vAimTarget = VEC3_ZERO);
	  CClonedAimAndThrowProjectileInfo() {}
	 ~CClonedAimAndThrowProjectileInfo() {}

	virtual CTaskFSMClone* CreateCloneFSMTask();
	virtual CTaskFSMClone* CreateLocalTask(fwEntity* pEntity);

	virtual s32		GetTaskInfoType() const			{ return INFO_TYPE_AIM_PROJECTILE; }
	virtual bool    HasState() const				{ return true; }
	virtual u32		GetSizeOfState() const			{ return datBitsNeeded<CTaskAimAndThrowProjectile::State_Finish>::COUNT; }
	virtual int		GetScriptedTaskType() const		{ return SCRIPT_TASK_THROW_PROJECTILE; }

	bool			IsPriming()	const				{ return m_bIsPriming; }
	bool			IsAiming() const				{ return m_bIsAiming; }
	bool			IsFiring() const				{ return m_bIsFiring; }
	bool			IsDropping() const  			{ return m_bIsDropping; }
	bool			IsThrowPlace() const			{ return m_bThrowPlace; }
#if ENABLE_GUN_PROJECTILE_THROW
    bool            IsAimGunAndThrow() const        { return m_bAimGunAndThrow; }
	u32				GetAimAndThrowWeaponHash()const { return m_uAimAndThrowWeaponHash; }
#endif	//ENABLE_GUN_PROJECTILE_THROW

	float			GetThrowPitch() const			{ return m_fThrowPitch; }
	float			GetCurrentPitch() const			{ return m_fCurrentPitch; }

	bool			HasTargetPosition() const		{ return m_bHasTargetPosition; }
	Vector3			GetTargetPosition() const		{ return m_vTargetPosition; }
	
	bool			HasAimTarget() const			{ return m_bHasAimTarget; }
	Vector3			GetAimTarget() const			{ return m_vAimTarget; }

	CEntity*		GetIgnoreCollisionEntity()		{ return m_IgnoreCollisionEntity.GetEntity(); }

    u8              GetRollToken() const            { return m_RollToken;   }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

        const unsigned SIZEOF_ROLL_TOKEN = datBitsNeeded<CTaskAimAndThrowProjectile::MAX_ROLL_TOKEN>::COUNT;

		SERIALISE_BOOL(serialiser, m_bIsPriming, "Is Priming");
		SERIALISE_BOOL(serialiser, m_bIsAiming, "Is Aiming");
		SERIALISE_BOOL(serialiser, m_bIsFiring, "Is Firing");
		SERIALISE_BOOL(serialiser, m_bIsDropping, "Is Dropping");
		SERIALISE_BOOL(serialiser, m_bThrowPlace, "Throw place");
#if ENABLE_GUN_PROJECTILE_THROW
        SERIALISE_BOOL(serialiser, m_bAimGunAndThrow, "Is Aim Gun And Throw");
		SERIALISE_UNSIGNED(serialiser, m_uAimAndThrowWeaponHash, 32, "Aim And Throw Weapon Hash");
#endif	//ENABLE_GUN_PROJECTILE_THROW

        SERIALISE_UNSIGNED(serialiser, m_RollToken, SIZEOF_ROLL_TOKEN, "Roll Token");
	
		SERIALISE_PACKED_FLOAT(serialiser, m_fThrowPitch, 1.0f, SIZEOF_PITCH, "Throw Pitch");
		SERIALISE_PACKED_FLOAT(serialiser, m_fCurrentPitch, 1.0f, SIZEOF_PITCH, "Current Pitch");
		
		SERIALISE_BOOL(serialiser, m_bHasTargetPosition, "Has Target Position");
		if(m_bHasTargetPosition || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_POSITION(serialiser, m_vTargetPosition, "Target Position");

		SERIALISE_BOOL(serialiser, m_bHasAimTarget, "Has Aim Target");
		if(m_bHasAimTarget || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_POSITION(serialiser, m_vAimTarget, "Aim Target");
		
		SERIALISE_UNSIGNED(serialiser, m_uAimAndThrowWeaponHash, 32, "Aim And Throw Weapon Hash");

		ObjectId ignoreCollisionEntityID = m_IgnoreCollisionEntity.GetEntityID();
		SERIALISE_OBJECTID(serialiser, ignoreCollisionEntityID, "Ignore collision entity for the projectile");
		m_IgnoreCollisionEntity.SetEntityID(ignoreCollisionEntityID);
	}

private:

	static const unsigned int SIZEOF_PITCH		= 8;
	
	bool m_bIsPriming;
	bool m_bIsAiming;
	bool m_bIsFiring;
	bool m_bIsDropping;
	bool m_bThrowPlace;
#if ENABLE_GUN_PROJECTILE_THROW
	bool m_bAimGunAndThrow;
	u32 m_uAimAndThrowWeaponHash;
#endif	//ENABLE_GUN_PROJECTILE_THROW

    u8   m_RollToken;

	float m_fThrowPitch;
	float m_fCurrentPitch;

	bool	m_bHasTargetPosition;
	Vector3 m_vTargetPosition;	

	bool	m_bHasAimTarget;
	Vector3 m_vAimTarget;

	CSyncedEntity	m_IgnoreCollisionEntity;

	CClonedAimAndThrowProjectileInfo(const CClonedAimAndThrowProjectileInfo &);
	CClonedAimAndThrowProjectileInfo &operator=(const CClonedAimAndThrowProjectileInfo &);
};


//////////////////////////////////////////////////////////////////////////
// CTaskThrowProjectile
//////////////////////////////////////////////////////////////////////////

class CTaskThrowProjectile : public CTaskFSMClone
{
public:

	enum eState
	{
		State_Start = 0,
		State_Intro,
		State_HoldingProjectile,
		State_ThrowProjectile,
		State_Finish,
	};

	////////////////////////////////////////////////////////////////////////////////

	 CTaskThrowProjectile(const CWeaponTarget& targeting, const bool bRotateToFaceTarget = true, const fwMvClipSetId &clipSetId = CLIP_SET_ID_INVALID);
	~CTaskThrowProjectile();

	void CleanUp();

    bool HasThrownProjectile() const { return m_bHasThrownProjectile; }
    void SetProjectileThrown() { m_bHasThrownProjectile = true; }

	void SetClipSetId(const fwMvClipSetId& clipsetId) { m_clipSetId = clipsetId; }
	void SetClipIds(const fwMvClipId& introClipId, const fwMvClipId& baseClipId, 
					const fwMvClipId& throwLongClipId, const fwMvClipId& throwShortClipId, 
					const fwMvClipId& throwLongFaceCoverClipId, const fwMvClipId& throwShortFaceCoverClipId) 
	{ 
		m_IntroClipId = introClipId; m_BaseClipId = baseClipId; 
		m_ThrowLongClipId = throwLongClipId; m_ThrowShortClipId = throwShortClipId;
		m_ThrowLongFaceCoverClipId = throwLongFaceCoverClipId; m_ThrowShortFaceCoverClipId = throwShortFaceCoverClipId;
	}

	void SetTurnClips(const crClip* pLeftTurn, const crClip* pRightTurn) {m_pLeftTurnClip = pLeftTurn; m_pRightTurnClip = pRightTurn;}
	
	virtual aiTask* Copy() const { return rage_new CTaskThrowProjectile(m_target, m_bRotateToFaceTarget, m_clipSetId); }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_THROW_PROJECTILE; }

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	
	// FSM required implementation
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	virtual bool				ProcessPostPreRender();
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32);
#endif


private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Intro_OnEnter();
	FSM_Return	Intro_OnUpdate();
	FSM_Return	HoldingProjectile_OnEnter();
	FSM_Return	HoldingProjectile_OnUpdate();
	FSM_Return	ThrowProjectile_OnEnter();
	FSM_Return	ThrowProjectile_OnUpdate();	

	bool		CanTriggerTurn(CPed* pPed);
	void		TriggerTurn(CPed* pPed, bool bFacingLeft);
	void		DropProjectile(CPed* pPed, CWeapon* pWeapon);

	bool		OffsetProjectileIfNecessaryInCover(const CPed *pPed, const Vector3 vTestStartCam, const Vector3 vTestEndCam, const Vector3 vTestStartThrow, const Vector3 vTestEndThrow, const Matrix34 weaponMat, float &fOffsetZ);

	//
	// Members
	//

	float m_fThrowPhase;
	bool  m_bFire;
	bool  m_bTurning;
	bool  m_bTurnSwappedAnims;

	// Targeting 
	CWeaponTarget m_target;

	//MoVE
	CMoveNetworkHelper		m_MoveNetworkHelper;

	// Specified throw clip set
	fwMvClipSetId m_clipSetId;

	// Specified throw clip
	fwMvClipId m_IntroClipId;
	fwMvClipId m_BaseClipId;
	fwMvClipId m_ThrowLongClipId;
	fwMvClipId m_ThrowShortClipId;
	fwMvClipId m_ThrowLongFaceCoverClipId;
	fwMvClipId m_ThrowShortFaceCoverClipId;

	//Turn anims
	const crClip* m_pLeftTurnClip;
	const crClip* m_pRightTurnClip;

	static const fwMvRequestId	ms_StartThrow;
	static const fwMvRequestId	ms_StartPrime;				
	static const fwMvRequestId	ms_TurnRight;
	static const fwMvRequestId	ms_TurnLeft;
	static const fwMvClipId		ms_MvIntroClip;
	static const fwMvClipId		ms_MvPrimeClip;
	static const fwMvClipId		ms_MvThrowClip;
	static const fwMvClipId		ms_MvThrowFaceCoverClip;
	static const fwMvClipId		ms_MvTurnLeftClip;
	static const fwMvClipId		ms_MvTurnRightClip;
	static const fwMvClipId		ms_MvThrowShortClip;
	static const fwMvClipId		ms_MvThrowShortFaceCoverClip;
	static const fwMvBooleanId	ms_IntroCompleteEvent;
	static const fwMvBooleanId	ms_ThrowCompleteEvent;	
	static const fwMvBooleanId	ms_EnterPrimeEvent;
	static const fwMvFloatId	ms_ThrowPhase;
	static const fwMvFloatId	ms_TurnPhase;
	static const fwMvFloatId	ms_ThrowDirection;

	// Flags
	bool m_bRotateToFaceTarget;
    bool m_bHasThrownProjectile;
	bool m_bPriming;	
	s32 m_iTurnTimer;

	u32 m_WeaponNameHash;
	bool m_bRightHand;
};

//-------------------------------------------------------------------------
// Task info for throwing a projectile
//-------------------------------------------------------------------------
class CClonedThrowProjectileInfo : public CSerialisedFSMTaskInfo
{
public:

	 CClonedThrowProjectileInfo(const u32 state, bool bHasTarget, const Vector3& vTarget, const fwMvClipSetId& clipsetId,const fwMvClipId& introClipId, const fwMvClipId& baseClipId, const fwMvClipId& throwLongClipId, const fwMvClipId& throwShortClipId, const u32 weaponNameHash, const bool bRightHand);
	 CClonedThrowProjectileInfo() {}
	~CClonedThrowProjectileInfo() {}

	virtual CTaskFSMClone* CreateCloneFSMTask();
	virtual CTaskFSMClone* CreateLocalTask(fwEntity* pEntity);

	inline bool		HasTargetPosition()	const				{ return m_bHasTargetPosition; }
	inline Vector3	GetTargetPosition()	const				{ return m_vTargetPosition; }

	inline const fwMvClipSetId	GetThrowClipSetId() const	{ return m_clipSetId; }
	inline const fwMvClipId		GetIntroClipId() const		{ return m_IntroClipId;	}
	inline const fwMvClipId		GetBaseClipId() const		{ return m_BaseClipId;	}
	inline const fwMvClipId		GetThrowLongClipId() const	{ return m_ThrowLongClipId;	}
	inline const fwMvClipId		GetThrowShortClipId() const	{ return m_ThrowShortClipId;	}

	inline const u32			GetWeaponNameHash() const	{ return m_WeaponNameHash; }
	inline const bool			GetRightHand() const		{ return m_bRightHand; }

	virtual s32		GetTaskInfoType() const			{ return INFO_TYPE_THROW_PROJECTILE; }
	virtual bool    HasState() const				{ return true; }
	virtual u32		GetSizeOfState() const			{ return datBitsNeeded<CTaskThrowProjectile::State_Finish>::COUNT; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 clipSetIdHash = m_clipSetId.GetHash();
		SERIALISE_UNSIGNED(serialiser, clipSetIdHash, SIZEOF_CLIP_SET_ID, "Clip Set ID");
		m_clipSetId.SetHash(clipSetIdHash);

		u32 animIdHash = m_IntroClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Intro Clip ID");
		m_IntroClipId.SetHash(animIdHash);

		animIdHash = m_BaseClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Base Clip ID");
		m_BaseClipId.SetHash(animIdHash);

		animIdHash = m_ThrowLongClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Throw long Clip ID");
		m_ThrowLongClipId.SetHash(animIdHash);

		animIdHash = m_ThrowShortClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Throw short Clip ID");
		m_ThrowShortClipId.SetHash(animIdHash);

		SERIALISE_BOOL(serialiser, m_bHasTargetPosition, "Has Target Position");
		if(m_bHasTargetPosition || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_POSITION(serialiser, m_vTargetPosition, "Target Position");

		SERIALISE_UNSIGNED(serialiser, m_WeaponNameHash, 32, "Weapon Hash");

		SERIALISE_BOOL(serialiser, m_bRightHand, "Right Hand");
	}

private:

	static const unsigned SIZEOF_CLIP_SET_ID = 32;
	static const unsigned SIZEOF_ANIM_ID     = 32;

	bool m_bHasTargetPosition; 
	Vector3 m_vTargetPosition;
	fwMvClipSetId m_clipSetId;
	fwMvClipId m_IntroClipId;
	fwMvClipId m_BaseClipId;
	fwMvClipId m_ThrowLongClipId;
	fwMvClipId m_ThrowShortClipId;

	u32 m_WeaponNameHash;
	bool m_bRightHand;

	CClonedThrowProjectileInfo(const CClonedThrowProjectileInfo &);
	CClonedThrowProjectileInfo &operator=(const CClonedThrowProjectileInfo &);
};

#endif // TASK_PROJECTILE_H
