#ifndef TASK_GUN_H
#define TASK_GUN_H

// Game headers
#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/Gun/GunIkInfo.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Task/Weapons/WeaponController.h"

// Forward declarations
class CEventGunShotWhizzedBy;
class CWeaponController;
class CWeaponInfo;

//////////////////////////////////////////////////////////////////////////
// CTaskGun
// - Stores all data required for sub tasks to function
// - Interface between non-gun tasks and gun tasks
//////////////////////////////////////////////////////////////////////////

class CTaskGun : public CTaskFSMClone
{
public:

	static const unsigned CLONE_TASK_SCOPE_DISTANCE = 800; // extended so that gunfire audio can be heard for players shooting in the distance

	enum State
	{
		State_Init = 0,
		State_Decide,
		State_AimIntro,
		State_Aim,
		State_Reload,
		State_Swap,
		State_BulletReaction,
		State_Quit,
	};

	struct Tunables : public CTuning
	{
		Tunables();

		s32				m_iMinLookAtTime;
		s32				m_iMaxLookAtTime;
		float			m_fMinTimeBetweenBulletReactions;
		float			m_fMaxTimeBetweenBulletReactions;
		float			m_fMaxDistForOverheadReactions;
		float			m_fMaxAboveHeadForOverheadReactions;
		float			m_fBulletReactionPosAdjustmentZ;
		float			m_fMinTimeBetweenLookAt;
		float			m_fMaxTimeBetweenLookAt;
		bool			m_bDisable2HandedGetups;
		float			m_TimeForEyeIk;
		float			m_MinTimeBetweenEyeIkProcesses;
		float			m_MinDotToPointGunAtPositionWhenUnableToTurn;
		atHashString	m_AssistedAimCamera;
		atHashString	m_RunAndGunAimCamera;
		u32				m_AssistedAimInterpolateInDuration;
		u32				m_RunAndGunInterpolateInDuration;
		u32				m_MinTimeBetweenOverheadBulletReactions;
		float			m_MaxTimeInBulletReactionState;
		u32				m_uFirstPersonRunAndGunWhileSprintTime;
		u32				m_uFirstPersonMinTimeToSprint;

		PAR_PARSABLE;
	};

public:

	static bool CanPedTurn(const CPed& rPed);
	static bool ProcessUseAssistedAimingCamera(CPed& ped, bool bForceAssistedAimingOn = false, bool bTestResetFlag = false);
	static bool ProcessUseRunAndGunAimingCamera(CPed& ped, bool bForceRunAndGunAimingOn = false, bool bTestResetFlag = false);
	static void ProcessTorsoIk(CPed& ped, const fwFlags32& flags, const CWeaponTarget& target, const CClipHelper* pClipHelper);
	static bool ShouldPointGunAtPosition(const CPed& rPed, const Vector3& vTargetPos);
	static float GetTorsoIKBlendFromWeaponFlag(const CPed& rPed, const Vector3& vTargetPos, const float fCurrentBlendAmount);
	static float GetTargetHeading(const CPed& ped);
	static void ResetLastTimeOverheadUsed() { ms_uLastTimeOverheadUsed = 0; }
	static void ClearToggleAim(CPed* pPed);

	// Returns true if ped is player, swimming and not holding down aim
	static bool IsPlayerSwimmingAndNotAiming(const CPed* pPed);

	CTaskGun(CWeaponController::WeaponControllerType weaponControllerType, CTaskTypes::eTaskType aimTaskType, const CWeaponTarget& target = CWeaponTarget(), float fDuration = -1.0f);
	CTaskGun(const CTaskGun& other);
	virtual ~CTaskGun();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskGun(*this); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_GUN; }
	
	virtual void CleanUp();

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	//
	// Settors
	//

	// Override the clips
	void SetOverrideAimClipId(const fwMvClipSetId &overrideClipSetId, const fwMvClipId &overrideClipId) { m_overrideAimClipSetId  = overrideClipSetId; m_overrideAimClipId  = overrideClipId; }
	void SetOverrideFireClipId(const fwMvClipSetId &overrideClipSetId, const fwMvClipId &overrideClipId) { m_overrideFireClipSetId = overrideClipSetId; m_overrideFireClipId = overrideClipId; }
	void SetOverrideClipSetId(const fwMvClipSetId &overrideClipSetId) { m_overrideClipSetId = overrideClipSetId; }

	void SetScriptedGunTaskInfoHash(u32 uHash) { m_uScriptedGunTaskInfoHash = uHash; }

	void SetTarget(const CWeaponTarget& target) { m_target = target; }

	void SetFiringPatternHash(u32 uFiringPattern)
	{
		m_uFiringPatternHash = uFiringPattern;
		m_bHasFiringPatternOverride = true;
	}

	// Set if we can restart after an abort
	void SetAllowRestartAfterAbort(bool bAllowRestart) { m_bAllowRestartAfterAbort = bAllowRestart; }

	bool GetReloadingOnEmpty() { return m_ReloadingOnEmpty; }
	void SetReloadingOnEmpty(bool bValue) { m_ReloadingOnEmpty = bValue; }

	// Set the remaining duration
	void SetDuration(float fNewDuration) { m_fDuration = fNewDuration; }

	// Block firing until the time is up (miliseconds)
	void SetBlockFiringTime(u32 uBlockFiringTime);

	//
	// Accessors
	//

	// Get the move network helper
	virtual const CMoveNetworkHelper*	GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	// Get the flags
	const fwFlags32& GetGunFlags() const { return m_iFlags; }
	fwFlags32& GetGunFlags() { return m_iFlags; }
	const fwFlags32& GetGunFireFlags() const { return m_iGFFlags; }
	fwFlags32& GetGunFireFlags() { return m_iGFFlags; }
	
	// Get the targeting
	const CWeaponTarget& GetTarget() const { return m_target; }
	CWeaponTarget& GetTarget() { return m_target; }

	// Get the Ik limits
	const CGunIkInfo& GetIkInfo() const { return m_ikInfo; }
	CGunIkInfo& GetIkInfo() { return m_ikInfo; }

	// Retrieve the weapon info
	const CWeaponInfo* GetPedWeaponInfo() const;

	// Get whether we are aiming
	bool GetIsAiming() const { return GetState() == State_Aim; }

	// Is the task able to fire?
	bool GetIsReadyToFire() const;

	// Get whether we are firing
	bool GetIsFiring() const;

	// Get whether we are reloading
	bool GetIsReloading() const { return GetState() == State_Reload; }
	bool GetWantsToReload() const { return m_bWantsToReload; }

	bool GetWantsToRoll() const { return m_bWantsToRoll; };

	bool CanReload() const;

	// Set if we want to play the aim intro
	void SetPlayAimIntro(bool bPlayAimIntro) { m_bPlayAimIntro = bPlayAimIntro; }

	// Get whether we are swapping weapons
	bool GetIsSwitchingWeapon() const { return GetState() == State_Swap; }

	// Get time spent firing
	float GetTimeSpentFiring() const;

	// Check if we're assisted aiming
	bool GetIsAssistedAiming() const { return m_bShouldUseAssistedAimingCamera; }

	// Check if we're running and gunning
	bool GetIsRunAndGunAiming() const { return m_bShouldUseRunAndGunAimingCamera; }

	// Get the remaining duration
	float GetDuration() const { return m_fDuration; }

	// PURPOSE: Returns the current weapon controller state for the specified ped
	WeaponControllerState GetWeaponControllerState(const CPed* pPed) const;

	void SetWeaponControllerType(const CWeaponController::WeaponControllerType type) { m_weaponControllerType = type; }

	// Called from the whizzed by event to let the task know there was one
	void OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);

	// PURPOSE: Called by nm tasks that override this task. Use to set up gun specific getup animations
	virtual bool		UseCustomGetup() { return true; }
	virtual bool		AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed );

	// PURPOSE: Called by nm tasks that override this task when a blend out finishes
	virtual void		GetUpComplete(eNmBlendOutSet, CNmBlendOutItem*, CTaskMove*, CPed*);

	// PURPOSE: Called every frame whilst in ragdoll to to request any necessary assets
	virtual void		RequestGetupAssets(CPed* pPed);

	static bool			SetupAimingGetupSet( CNmBlendOutSetList& set, CPed* pPed, const CAITarget& target);

	static const Tunables& GetTunables() { return ms_Tunables; }

	// I want to see if ped was running in "combat" before when NMGetup was running
	virtual bool IsConsideredInCombat() { return true; }

	void SetShouldSkipDrivebyIntro(bool bSkip) { m_bSkipDrivebyIntro = bSkip; }
	void SetShouldInstantlyBlendDrivebyAnims(bool bInstant) { m_bInstantlyBlendDrivebyAnims = bInstant; }

#if FPS_MODE_SUPPORTED
	bool ShouldDelayAfterSwap() const;
#endif // FPS_MODE_SUPPORTED

#if !__FINAL
	virtual void Debug() const;
#endif // !__FINAL

protected:

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	virtual bool ProcessMoveSignals();

	virtual s32 GetDefaultStateAfterAbort() const { return m_bAllowRestartAfterAbort ? State_Init : State_Quit; }

    // Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
    virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual bool				IsInScope(const CPed* pPed);
	virtual float				GetScopeDistance() const;

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	//
	// Local State functions
	//

	FSM_Return	StateInit(CPed* pPed);
	FSM_Return	StateDecide(CPed* pPed);
	FSM_Return	StateAimIntroOnEnter();
	FSM_Return	StateAimIntroOnUpdate();
	FSM_Return	StateAimOnEnter(CPed* pPed);
	FSM_Return	StateAimOnUpdate(CPed* pPed);
	FSM_Return	StateAimOnExit(CPed* pPed);
	FSM_Return	StateReloadOnEnter(CPed* pPed);
	FSM_Return	StateReloadOnUpdate(CPed* pPed);
	FSM_Return	StateReloadOnExit(CPed* pPed);
	FSM_Return	StateSwapOnEnter(CPed* pPed);
	FSM_Return	StateSwapOnUpdate(CPed* pPed);
	FSM_Return	StateBulletReactionOnEnter();
	FSM_Return	StateBulletReactionOnUpdate();
	FSM_Return	StateBulletReactionOnExit(CPed* pPed);
	bool		StateBulletReaction_OnProcessMoveSignals();

    //
	// Clone State functions
	//
	FSM_Return StateAimOnUpdateClone(CPed* pPed);
	FSM_Return StateReloadOnUpdateClone(CPed* pPed);
	FSM_Return StateSwapOnEnterClone(CPed* pPed);
	FSM_Return StateSwapOnUpdateClone(CPed* pPed);

public:

	// 
	// Clone target management functions (public so CClonedGunInfo can get / set)....
	//

	void			SetTargetOutOfScopeID(ObjectId _networkId )	{ m_TargetOutOfScopeId_Clone = _networkId;		 }
	inline ObjectId GetTargetOutOfScopeID(void )				{ return m_TargetOutOfScopeId_Clone; }

	// Update the target - made public so it can be called by CNetObjPlayer::GetCameraData - should not be called anywhere else....
	void UpdateTarget(CPed* pPed);

	u32 GetLastIsAimingTimeForOutroDelay() const { return m_uLastIsAimingTimeForOutroDelay; }

	void SetReloadIsBlocked(bool bBlocked) { m_bReloadIsBlocked = bBlocked; }
	bool GetReloadIsBlocked() const { return m_bReloadIsBlocked; }
	void SetPedPosWhenReloadIsBlocked(Vec3V& vPedPos) { m_vPedPosWhenReloadIsBlocked = vPedPos; }
	void SetPedHeadingWhenReloadIsBlocked(float fPedHeading) { m_vPedHeadingWhenReloadIsBlocked = fPedHeading; }

private:

    // Is the specified state handled by the clone FSM
    bool StateChangeHandledByCloneFSM(s32 iState);

	// Process ped head look at
	void ProcessLookAt(CPed* pPed);

	// Process ped eye ik
	void ProcessEyeIk();

	// Check if eye ik should be processed
	bool ShouldProcessEyeIk() const;

	// Has the duration run out?
	bool HasDurationExpired() const { return (!GetGunFlags().IsFlagSet(GF_InfiniteDuration) && m_fDuration < 0.0f) ? true : false; }

	// Validate the peds weapon
	bool HasValidWeapon(CPed* pPed) const;
	
	// Chooses the clip set for bullet reactions
	fwMvClipSetId ChooseClipSetForBulletReaction() const;

	// Determines if we should play our bullet reactions
	bool ShouldPlayBulletReaction();
	
	void ProcessFacialIdleClip();

#if !__NO_OUTPUT
	virtual atString GetName() const;
#endif

	//
	// Members
	//
	
	// The gun event position (when we have one)
	Vec3V m_vGunEventPosition;
	float m_fGunEventReactionDir;

	// Weapon controller
	CWeaponController::WeaponControllerType m_weaponControllerType;

	// Further control of what the task does
	fwFlags32 m_iFlags;
	fwFlags32 m_iGFFlags;

	// Should we allow this task to be restarted after aborting?
	bool m_bAllowRestartAfterAbort:1;

	bool m_ReloadingOnEmpty : 1;

	//
	// Sub tasks
	//

	CTaskTypes::eTaskType m_aimTaskType;

	//
	// Targeting
	//

	CWeaponTarget m_target;

	//
	// Shooting
	//

	// Used to look up firing pattern from CFiringPatternManager
	u32 m_uFiringPatternHash;

	// If this is true it means we should be overriding the ped's firing pattern
	bool m_bHasFiringPatternOverride;

	//
	// Timers
	//

	// Time (in milliseconds) before ending the task (-1 for infinite)
	float m_fDuration;

	// The amount of time this task has been active for
	float m_fTimeActive;

	// Randomise the head look at
	float m_fTimeTillNextLookAt;

	float m_fTimeSinceLastEyeIkWasProcessed;

	// Time to prevent firing until (global ms)
	u32 m_uBlockFiringTime;

	// Last time player was actually aiming for aiming outro (excludes reload, switch weapon etc)
	u32 m_uLastIsAimingTimeForOutroDelay;
	u32 m_uLastIsAimingTimeForOutroDelayCached;

	// Last time the player switched to run and gun while sprinting
	u32 m_uLastFirstPersonRunAndGunTimeWhileSprinting;
	s32 m_iTimeToAllowSprinting;

	static u32 ms_uLastTimeOverheadUsed;

	//
	// Ik
	//

	CGunIkInfo m_ikInfo;


	// 
	// Target syncing
	//

	ObjectId	m_TargetOutOfScopeId_Clone;

	//
	// Animation overrides
	//

	// Used for blind fire task
	CMoveNetworkHelper	m_MoveNetworkHelper;

	CPrioritizedClipSetRequestHelper m_BulletReactionClipSetRequestHelper;

	// Overridden clip set id, so we will use this one rather than the one from the weapon
	fwMvClipSetId m_overrideAimClipSetId;
	fwMvClipId m_overrideAimClipId;
	fwMvClipSetId m_overrideFireClipSetId;
	fwMvClipId m_overrideFireClipId;
	fwMvClipSetId m_overrideClipSetId;

	u32	m_uScriptedGunTaskInfoHash;

	u32 m_uLastSwapTime;

	// We need to keep track that we used aim assist because otherwise we get a frame when request an aim
	// cam when we stop aiming 
	u32 m_bShouldUseAssistedAimingCamera : 1;
	u32 m_bShouldUseRunAndGunAimingCamera : 1;
	u32 m_bWantsToReload : 1; // If this is set, we quit the subtask because we wanted to reload
	u32 m_bMoveReactionFinished : 1;
	u32 m_bPlayAimIntro : 1;
	u32 m_bWantsToRoll : 1;
	u32 m_bReloadIsBlocked : 1;
	u32 m_bSkipDrivebyIntro : 1;
	u32 m_bInstantlyBlendDrivebyAnims : 1;

	Vec3V m_vPedPosWhenReloadIsBlocked;
	float m_vPedHeadingWhenReloadIsBlocked;

	static const u32 ms_iMaxCombatGetupHelpers = 9;

	CGetupSetRequestHelper m_GetupReqHelpers[ms_iMaxCombatGetupHelpers];

	// Our bullet reaction move network ID/info
	static fwMvClipSetId ms_BulletReactionPistolClipSetId;
	static fwMvClipSetId ms_BulletReactionRifleClipSetId;
	static fwMvBooleanId ms_ReactionFinished;
	static fwMvRequestId ms_React;
	static fwMvFloatId	 ms_DirectionId;
	static fwMvFlagId	 ms_VariationId;
	static fwMvFlagId	 ms_ForceOverheadId;

	// Our tunables
	static Tunables		 ms_Tunables;
};

//-------------------------------------------------------------------------
// Task info for being in cover
//-------------------------------------------------------------------------
class CClonedGunInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedGunInfo();
	CClonedGunInfo(s32 gunState, const CWeaponTarget* pTarget, u32 gunFlags, s32 aimTaskType, u32 uScriptedGunTaskInfoHash, float bulletReactDir, bool reloadingOnEmpty);
    ~CClonedGunInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_GUN;}
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_AIM_GUN_SCRIPTED; }
	
	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity *pEntity);

    virtual bool    HasState() const { return true; }
    virtual u32		GetSizeOfState() const { return SIZEOF_GUN_STATUS;}
    virtual const char*	GetStatusName(u32) const { return "Status";}
    
	inline u32				GetGunFlags(void) const					{ return m_GunFlags; }
	inline u32				GeAimTaskType(void) const				{ return m_AimTaskType; }
	inline u32				GetScripedGunaskInfoHash(void) const	{ return m_uScriptedGunTaskInfoHash; }
	inline float			GetBulletReactionDir(void) const		{ return m_BulletReactionDir; }
	inline bool				GetReloadingOnEmpty() const				{ return m_ReloadingOnEmpty; }

	inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
	inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);
        
		SERIALISE_UNSIGNED(serialiser, m_GunFlags, SIZEOF_GUN_FLAGS, "Gun Flags");
        SERIALISE_UNSIGNED(serialiser, m_AimTaskType, SIZEOF_AIM_TASK_TYPE, "Aim Task Type");

		m_weaponTargetHelper.Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_ReloadingOnEmpty,"Reloading On Empty");

		bool scriptedHashValid = m_uScriptedGunTaskInfoHash != 0;

		SERIALISE_BOOL(serialiser, scriptedHashValid,"Scripted hash valid");

		if(scriptedHashValid || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_uScriptedGunTaskInfoHash, SIZEOF_SCRIPTED_GUN_TASK_INFO_HASH, "Scripted gun taskinfo hash");
		}

		SERIALISE_PACKED_FLOAT(serialiser, m_BulletReactionDir, 1.0f, SIZEOF_BULLET_REACTION_DIR, "Gun Event React Dir");
    }

private:

    static const unsigned int SIZEOF_GUN_STATUS    = 3;
    static const unsigned int SIZEOF_GUN_FLAGS     = 2;
    static const unsigned int SIZEOF_AIM_TASK_TYPE = 3;
	static const unsigned int SIZEOF_SCRIPTED_GUN_TASK_INFO_HASH = 32;
	static const unsigned int SIZEOF_BULLET_REACTION_DIR = 8;

    u32 GetPackedGunFlags(s32 unpackedFlags);
    s32 GetUnPackedGunFlags(u32 packedFlags);
    u32 GetPackedAimTaskType(s32 unpackedType);
    s32 GetUnPackedAimTaskType(u32 packedType);

    u32				m_GunFlags;
    u32				m_AimTaskType;
	u32				m_uScriptedGunTaskInfoHash;
	float			m_BulletReactionDir;

	bool			m_ReloadingOnEmpty;

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;
};


//////////////////////////////////////////////////////////////////////////
// CTaskAimSweep
// - Creates a CTaskGun and controls/update the target position to aim at
//////////////////////////////////////////////////////////////////////////

class CTaskAimSweep : public CTask
{
public:

	enum State
	{
		State_Start = 0,
		State_Aim,
		State_Finish,
	};

	enum AimType
	{
		Aim_Forward = 0,
		Aim_Left,
		Aim_Right,
		Aim_Varied,
		Aim_World_Direction,
		Aim_Local_Direction,
	};

	CTaskAimSweep(CTaskAimSweep::AimType aimType, bool bIgnoreGroup);
	~CTaskAimSweep(){}

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskAimSweep(m_aimType, m_bIgnoreGroup); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_AIM_SWEEP; }

protected:

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// PURPOSE: Updates the current target we want to aim at
	void UpdateAimTarget();

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	FSM_Return			Start_OnUpdate();
	void				Aim_OnEnter();
	FSM_Return			Aim_OnUpdate();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	//////////////////////////////////////////////////////////////////////////
	// Member variables
	//////////////////////////////////////////////////////////////////////////

	// What type of aiming we should be doing
	AimType m_aimType;

	// This is the target we calculate based on our aim type
	Vector3 m_vTargetPosition;

	// This is the position that we intend to aim at
	Vector3 m_vDesiredTarget;

	// This is a position we keep track of that is ahead of where we want to currently aim, in case we get too close to our target
	Vector3 m_vAheadPosition;

	// Timer keeping track of when to update our aim target
	float	m_fUpdateAimTimer;

	// If this is set, a non-leader group member will ignore their group when deciding to aim (will use navigation info to determine aim pos)
	bool	m_bIgnoreGroup : 1;
};

#endif // TASK_GUN_H
