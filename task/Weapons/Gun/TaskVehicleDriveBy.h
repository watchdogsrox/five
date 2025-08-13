#ifndef INC_TASK_VEHICLE_DRIVE_BY_H
#define INC_TASK_VEHICLE_DRIVE_BY_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "System/Control.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Vehicles/metadata/VehicleSeatInfo.h"
#include "Weapons/Info/WeaponInfo.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CTaskAimGunVehicleDriveBy
// Purpose: Gun task for vehicle drive by, handles aiming and firing
//////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

class CTaskAimGunVehicleDriveBy : public CTaskAimGun
{

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float m_MinTimeBetweenInsults;
		float m_MaxDistanceToInsult;
		float m_MinDotToInsult;

		u32	  m_MinAimTimeMs;
		u32	  m_MaxAimTimeOnStickMs;
		u32	  m_AimingOnStickCooldownMs; 

		fwMvFilterId m_BicycleDrivebyFilterId;
		fwMvFilterId m_BikeDrivebyFilterId;
		fwMvFilterId m_JetskiDrivebyFilterId;
		fwMvFilterId m_ParachutingFilterId;

		fwMvClipId m_FirstPersonGripLeftClipId;
		fwMvClipId m_FirstPersonGripRightClipId;
		fwMvClipId m_FirstPersonFireLeftClipId;
		fwMvClipId m_FirstPersonFireRightClipId;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;
	
public:
	friend class CWeapon;

	enum State
	{
		State_Start = 0,
		State_Intro,
		State_Aim,
		State_Fire,
		State_Reload,
		State_VehicleReload,
		State_Outro,
		State_Finish,
	};

	enum UnarmedState
	{
		Unarmed_None,
		Unarmed_Aim,
		Unarmed_Look,
		Unarmed_Finish
	};

	// Statics
	static const fwMvBooleanId ms_DrivebyOutroBlendOutId;
	static const fwMvFloatId ms_SpineAdditiveBlendDurationId;
	static const fwMvFlagId	 ms_UseSpineAdditiveFlagId;
	static float			ms_DefaultMinYawChangeRate;
	static float			ms_DefaultMaxYawChangeRate;
	static float			ms_DefaultMinYawChangeRateOverriddenContext;
	static float			ms_DefaultMaxYawChangeRateOverriddenContext;
	static float			ms_DefaultMinYawChangeRateFirstPerson;
	static float			ms_DefaultMaxYawChangeRateFirstPerson;
	static float			ms_DefaultMaxPitchChangeRate;
	static float			ms_DefaultMaxPitchChangeRateFirstPerson;
	static float			ms_DefaultIntroRate;
	static float			ms_DefaultIntroRateFirstPerson;
	static float			ms_DefaultOutroRate;
	static float			ms_DefaultOutroRateFirstPerson;
	static float			ms_DefaultRemoteMinYawChangeRateScalar;
	static float			ms_DefaultRemoteMaxYawChangeRateScalar;
	static float			ms_DefaultRemoteMaxPitchChangeRate;
	static float			ms_DefaultRemoteIntroRate;
	static float			ms_DefaultRemoteOutroRate;
	static bank_bool		ms_UseAssistedAiming;
	static bank_bool		ms_OnlyAllowLockOnToPedThreats;

	// Constructor
	CTaskAimGunVehicleDriveBy(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target);

	// Task required functions
	virtual aiTask*			Copy() const		{ return rage_new CTaskAimGunVehicleDriveBy(m_weaponControllerType, m_iFlags, m_target); }
	virtual s32				GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY; }
	virtual s32				GetDefaultStateAfterAbort() const { return State_Finish; }

	// Task optional functions
	virtual void			GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;
	virtual	void			CleanUp();
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }
	virtual bool			SupportsTerminationRequest() { return true; }

	// Accessors
	virtual bool			GetIsReadyToFire() const					{ return GetState() == State_Aim || GetState() == State_Fire; }
	virtual bool			GetIsFiring() const							{ return GetState() == State_Fire; }
	const CWeaponInfo*		GetPedWeaponInfo() const;
	
	bool IsFlippingSomeoneOff() const;
	bool GetIsPlayerAimMode(const CPed* pPed) const;
	bool ShouldBlendOutAimOutro();
	void RequestSpineAdditiveBlendIn(float fBlendDuration);
	void RequestSpineAdditiveBlendOut(float fBlendDuration);

	const bool IsStateValid() const;

	bool IsFirstPersonDriveBy() const;

	bool IsPoVCameraConstrained(const CPed* pPed) const;

	static bool IsFlippingSomeoneOff(const CPed& rPed);
	static bool IsHidingGunInFirstPersonDriverCamera(const CPed* pPed, bool bSkipPoVConstrainedCheck = false);

	static bool	ShouldUseRestrictedDrivebyAnimsOrBlockingIK(const CPed *pPed, bool bCheckCanUseRestrictedAnims = false);
	static bool CanUseCloneAndAIBlocking(const CPed* pPed);
	static bool CanUseRestrictedDrivebyAnims();
	void SetShouldSkipIntro(bool bSkip) { m_bSkipDrivebyIntro = bSkip; }
	void SetShouldInstantlyBlendDrivebyAnims(bool bInstant) { m_bInstantlyBlendDrivebyAnims = bInstant; }

	void SetBlockOutsideAimingRange(bool bBlockOutsideAimingRange) { m_bBlockOutsideAimingRange = bBlockOutsideAimingRange; }

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* GetFirstPersonIkHelper() const { return m_pFirstPersonIkHelper; }
#endif // FPS_MODE_SUPPORTED

protected:

    // Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
    virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
    virtual bool IsInScope(const CPed* pPed);
	virtual float GetScopeDistance() const;

	FSM_Return				CloneVehicleReload_OnUpdate();

private:

	// State Machine functions
	virtual FSM_Return		ProcessPreFSM();
	virtual FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool			ProcessPostPreRenderAfterAttachments();
	virtual bool			ProcessPostCamera();
	virtual bool			ProcessMoveSignals();

	// State functions
	FSM_Return				Start_OnUpdate();
	void					Intro_OnEnter();
	FSM_Return				Intro_OnUpdate();
	void					Aim_OnEnter();
	FSM_Return				Aim_OnUpdate();
	void					Aim_OnExit();
	void					Fire_OnEnter();
	FSM_Return				Fire_OnUpdate();
	void					Fire_OnExit();
	void					Fire_OnProcessMoveSignals();
	void					Reload_OnEnter();
	FSM_Return				Reload_OnUpdate();
	void					Reload_OnExit();
	void					VehicleReload_OnEnter();
	FSM_Return				VehicleReload_OnUpdate();
	void					Outro_OnEnter();
	FSM_Return				Outro_OnUpdate();
	void					Outro_OnExit();
	void					Outro_OnProcessMoveSignals();
	FSM_Return				Finish_OnUpdate();

	// Helper functions
	bool					WantsToFire() const;
	bool					WantsToFire(const CPed& ped, const CWeaponInfo& weaponInfo) const;
	bool					ReadyToFire() const;
	bool					ReadyToFire(const CPed& ped) const;

	void					UpdateMoVEParams();
	void					UpdateFPMoVEParams();
	void					UpdateUnarmedDriveBy(CPed* pPed);

	bool					FindIntersection(CPed* pPed, Vector3& vStart, const Vector3& vEnd, Vector3& vIntersection, float& fIntersectionDist); // Returns the intersection distance
	bool					ShouldFindIntersection(float fNewIntersectionDist);
	void					CalculateIntroBlendParam(float fDesiredYaw);
	float					ComputeYawSignalFromDesired(float &fDesiredYaw, float fSweepHeadingMin, float fSweepHeadingMax);	
	bool					FineTuneAim(CPed* pPed, Vector3& vStart, Vector3& vEnd,  float& fDesiredHeading, float& fDesiredPitch);
	void					CheckForBlocking();
	bool					CheckForFireLoop();
	bool					CheckForOutroOnBicycle(WeaponControllerState controllerState, const CControl& control) const;

    // Returns the current weapon controller state for the specified ped
    virtual WeaponControllerState GetWeaponControllerState(const CPed* ped) const;
    
    void					ProcessInsult();
    void					ProcessTimers();

#if FPS_MODE_SUPPORTED
	bool					IsStateValidForFPSIK() const;
#endif // FPS_MODE_SUPPORTED

	void					ProcessWheelClipIKOffset(Vector3 &vTargetOffset);
	float					TranslateRangeToRange(float fCurrentValue, float fOldMin, float fOldMax, float fNewMin, float fNewMax);
	void					ProcessBlockedIKOffset(Vector3 &vTargetOffset, const Vector3& vGunTargetOffset);

	CPed*					GetBlockingFirePedFirstPerson(const Vector3 &vStart, const Vector3 &vEnd);

	void					ProcessLookAtWhileWaiting(CPed* pPed);

private:

	CMoveNetworkHelper		m_MoveNetworkHelper;

	CVehicleDriveByAnimInfo::eDrivebyNetworkType m_eType;

	const CVehicleDriveByInfo* m_pVehicleDriveByInfo;

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper*	m_pFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

	// Variables which directly control the phase of the clips 
	// in move to point the ped in the correct direction
	float					m_fIntroHeadingBlend;
	float					m_fCurrentHeading;
	float					m_fCurrentPitch;
	float					m_fOutroHeadingBlend;
	float					m_fLastYaw;
	float					m_fLastPitch;

	float					m_fMinHeadingAngle;
	float					m_fMaxHeadingAngle;

	// Variables which are updated according to the aim direction, 
	// the current variables above are interpolated to these values smoothly
	float					m_fDesiredHeading;
	float					m_fDesiredPitch;

	// Defines the maximum amount we allow the phase of the heading/pitch params in one step
	float					m_fHeadingChangeRate;
	float					m_fPitchChangeRate;

	float					m_fTransitionDesiredHeading;
	float					m_fDrivebyOutroBlendOutPhase;
	Vector3					m_vTarget;
	
	float					m_fTimeSinceLastInsult;
	float					m_fLastHoldingTime;

	// for computing firePhase
	u32						m_uFireTime; 

	// for blocking FPS IK if camera is constrained.
	u32						m_uTimePoVCameraConstrained;

	// Recoil
	fwMvClipSetId			m_FireClipSetId;

	// for forcing an aim duration (players)
	u32						m_uAimTime; 
	u32						m_uLastUsingAimStickTime;

	float					m_fMinFPSIKYaw;
	float					m_fMaxFPSIKYaw;

	u8						m_targetFireCounter; // used when a clone task

	bool					m_bForceParamChange : 1;
	bool					m_bBlocked : 1;
	bool					m_bUseVehicleAsCamEntity : 1;
	bool					m_bFire : 1;
	bool					m_bFiredOnce : 1;
	bool					m_bFiredTwice : 1;
	bool					m_bFireLoopOnEnter : 1;
	bool					m_bOverAimingClockwise : 1;
	bool					m_bOverAimingCClockwise : 1;
	bool					m_bFlipping : 1;
	bool					m_bHolding : 1;
	bool					m_bCheckForBlockTransition : 1;
	bool					m_bMoveFireLoopOnEnter : 1;
	bool					m_bMoveFireLoop1 : 1;
	bool					m_bMoveFireLoop2 : 1;
	bool					m_bMoveOutroClipFinished : 1;
	bool					m_bJustReloaded : 1;
	bool					m_bRemoteInvokeFire : 1;
	bool					m_bUsingSecondaryTaskNetwork : 1;
	bool					m_bEquippedWeaponChanged : 1;
	bool					m_bDelayFireFinishedRequest : 1;
	bool					m_bForceUpdateMoveParamsForIntro : 1;
	u32						m_uTimeLastBlocked;
	bool					m_bRestoreWeaponToVisibleOnCleanup : 1;
	bool					m_bFlippedForBlockedInFineTuneLastUpdate : 1;

	// FPS blocked driveby parameters:
	Vector3					m_vBlockedPosition;
	Vector3					m_vBlockedOffset;
	Vector3					m_vWheelClipOffset;
	Vector3					m_vWheelClipPosition;
	float					m_fPenetrationDistance;
	float					m_fSmoothedPenetrationDistance;
	float					m_fBlockedTimer;
	float					m_fLastBlockedHeading;
	bool					m_bShouldDoProbeTestForAIOrClones : 1;
	bool					m_bShouldRestartNetworkWithRestrictedAnims : 1;
	bool					m_bCloneOrAIBlockedDueToPlayerFPS : 1;
	bool					m_bSkipDrivebyIntro : 1;
	bool					m_bInstantlyBlendDrivebyAnims : 1;
	bool					m_bBlockedHitPed : 1;
	bool					m_bBlockOutsideAimingRange : 1;
	u32						m_uBlockingCheckStartTime;

	//Unarmed Driveby:
	UnarmedState			m_UnarmedState;
	u32						m_uLastUnarmedStateChangeTime;
	u32						m_uNextUnarmedStateChangeTime;

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvClipSetVarId ms_FireClipSetVarId;
	static const fwMvBooleanId ms_OutroClipFinishedId;
	static const fwMvBooleanId ms_StateAimId;
	static const fwMvBooleanId ms_FireLoopOnEnter;
	static const fwMvBooleanId ms_FireLoop1;
	static const fwMvBooleanId ms_FireLoop2;
	static const fwMvRequestId ms_Fire;
	static const fwMvRequestId ms_FireAuto;	
	static const fwMvRequestId ms_FireWithRecoil;	
	static const fwMvRequestId ms_FireFinished;
	static const fwMvRequestId ms_FlipLeft;
	static const fwMvRequestId ms_FlipRight;
	static const fwMvRequestId ms_FlipEnd;
	static const fwMvRequestId ms_Intro;
	static const fwMvRequestId ms_Aim;
	static const fwMvRequestId ms_Hold;
	static const fwMvRequestId ms_HoldEnd;
	static const fwMvRequestId ms_Blocked;
	static const fwMvRequestId ms_BikeBlocked;
	static const fwMvFloatId ms_HeadingId;
	static const fwMvFloatId ms_IntroHeadingId;
	static const fwMvFloatId ms_IntroRateId;
	static const fwMvFloatId ms_OutroHeadingId;
	static const fwMvFloatId ms_OutroRateId;
	static const fwMvFloatId ms_PitchId;
	static const fwMvFloatId ms_FirePhaseId;
	static const fwMvFloatId ms_RecoilPhaseId;
	static const fwMvFloatId ms_FlipPhaseId;
	static const fwMvFloatId ms_BlockTransitionPhaseId;
	static const fwMvFloatId ms_IntroTransitionPhaseId;
	static const fwMvFloatId ms_OutroTransitionPhaseId;
	static const fwMvFloatId ms_IntroToAimPhaseId;
	static const fwMvFloatId ms_FireRateId;
	static const fwMvFlagId  ms_UsePitchSolutionFlagId;
	static const fwMvFlagId  ms_UseThreeAnimIntroOutroFlagId;
	static const fwMvFlagId  ms_UseIntroFlagId;
	static const fwMvFlagId  ms_FiringFlagId;
	static const fwMvFlagId  ms_FirstPersonMode;
	static const fwMvFlagId  ms_FirstPersonModeIK;
	static const fwMvClipId  ms_OutroClipId;
	static const fwMvClipId  ms_FirstPersonGripClipId;
	static const fwMvClipId  ms_FirstPersonFireRecoilClipId;
	static const fwMvFilterId  ms_FirstPersonRecoilFilter;
	static const fwMvFilterId  ms_FirstPersonGripFilter;

private:

	////////////////////////////////////////////////////////////////////////////////

#if __BANK

	float					m_fDebugDesiredYaw;
	float					m_fDebugDesiredPitch;
	Vector3					m_vStartPos;
	Vector3					m_vEndPos;

#endif // __BANK

public:

#if !__FINAL
	virtual void			Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

#if __BANK	
	static bool				ms_bRenderVehicleGunDebug;
	static bool				ms_bRenderVehicleDriveByDebug;

	static bool				ms_bDisableOutro;
	static bool				ms_bDisableBlocking;
	static bool				ms_bDisableWindowSmash;

	static void				InitWidgets(bkBank& bank);
#endif // __BANK
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CClonedAimGunVehicleDriveByInfo
// Purpose: Task info for aim gun drive by task
//////////////////////////////////////////////////////////////////////////

class CClonedAimGunVehicleDriveByInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedAimGunVehicleDriveByInfo();
	CClonedAimGunVehicleDriveByInfo(s32 aimGunVehicleDriveByState, u8 fireCounter, u8 ammoInClip, u32 seed);
    ~CClonedAimGunVehicleDriveByInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_AIM_GUN_VEHICLE_DRIVE_BY;}

    virtual bool    HasState() const { return true; }
    virtual u32	    GetSizeOfState() const { return SIZEOF_AIM_GUN_VEHICLE_DRIVE_BY;}
    virtual const char*	GetStatusName(u32) const { return "Status";}

	u8 GetFireCounter() const { return m_fireCounter; }
	u8 GetAmmoInClip()  const { return m_iAmmoInClip; }
	u32 GetSeed() const { return m_seed; }

	void Serialise(CSyncDataBase& serialiser);

private:

    static const unsigned int SIZEOF_AIM_GUN_VEHICLE_DRIVE_BY = 3;

	u32		m_seed;
	u8		m_fireCounter;
	u8		m_iAmmoInClip;
	bool    m_bHasSeed;
};


#endif // INC_TASK_VEHICLE_DRIVE_BY_H
