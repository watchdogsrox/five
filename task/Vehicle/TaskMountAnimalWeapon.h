#ifndef INC_TASK_MOUNT_ANIMAL_WEAPON_H
#define INC_TASK_MOUNT_ANIMAL_WEAPON_H
/********************************************************************
filename:	TaskMountAnimalWeapon.h
created:	7/7/2011
author:		Bryan Musson

purpose:	Tasks related to combat while mounted
*********************************************************************/

#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "task/combat/TaskCombatMelee.h"

class CVehicleDriveByInfo;
struct sCollisionInfo;
struct StrikeCapsule;
struct MeleeNetworkDamage;

//////////////////////////////////////////////////////////////////////////
// Class CTaskMountThrowProjectile
// The throw from horseback class
class CTaskMountThrowProjectile  : public CTaskAimGun
{
	// For internal use
	enum eTaskInput
	{
		Input_None,
		Input_Hold,
		Input_Fire,
		Input_Cook
	};

public:
	enum eMountProjectileState
	{
		State_Init,
		State_StreamingClips,		
		State_Intro,			// Intro clip
		State_OpenDoor,
		State_SmashWindow,
		State_Hold,				// hold the projectile primed in hand
		State_Throw,
		State_DelayedThrow,
		State_Outro,
		State_Finish
	};

	struct BikeMeleeNetworkDamage : CTaskMeleeActionResult::MeleeNetworkDamage
	{
		BikeMeleeNetworkDamage()
			: CTaskMeleeActionResult::MeleeNetworkDamage()
			, m_materialId(0)
		{}

		phMaterialMgr::Id m_materialId;
	};

	CTaskMountThrowProjectile(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target, bool bRightSide = false, bool bVehicleMelee = false);

	virtual aiTask*				Copy() const { return rage_new CTaskMountThrowProjectile(m_weaponControllerType, m_iFlags, m_target, m_bVehMeleeStartingRightSide, m_bStartedByVehicleMelee); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_MOUNT_THROW_PROJECTILE; }

	virtual s32				GetDefaultStateAfterAbort() const { return State_Init; }

	virtual void			GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// Accessors
	virtual bool			GetIsReadyToFire() const			{ return GetState() == State_Hold; }
	virtual bool			GetIsFiring() const					{ return GetState() == State_Throw; }

	void RequestSpineAdditiveBlendIn(float fBlendDuration);
	void RequestSpineAdditiveBlendOut(float fBlendDuration);

	bool IsDropping() const { return m_bDrop; }
	void SetDrop(bool bDrop) { m_bDrop = bDrop; }
	void SetWindowSmashed(bool bWindowSmashed) { m_bWindowSmashed = bWindowSmashed; }

	bool ShouldBlockHandIk(bool bRightArm) const;
	void SetVehMeleeRestarting(bool bValue) { m_bVehMeleeRestarting = bValue; }
	bool GetIsCloneInDamageWindow() const;
	void AddToHitEntityCount(CEntity* pEntity);
	bool EntityAlreadyHitByResult( const CEntity* pHitEntity ) const;

	virtual bool SupportsTerminationRequest() { return true; }

	static void HitImpulseCalculation(Vector3& vImpulseDir, float& fImpulseMag, const CPed& rHitPed, const CPed& rFiringPed);
	static void CacheMeleeNetworkDamage(CPed *pHitPed,
		CPed *pFiringEntity,
		const Vector3 &vWorldHitPos,
		s32 iComponent,
		float fAppliedDamage, 
		u32 flags, 
		u32 uParentMeleeActionResultID,
		u32 uForcedReactionDefinitionID,
		u16 uNetworkActionID,
		phMaterialMgr::Id materialId);

	CTaskMountThrowProjectile::BikeMeleeNetworkDamage *FindMeleeNetworkDamageForHitPed(CPed *pHitPed);

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* GetFirstPersonIkHelper() const { return m_pFirstPersonIkHelper; }
#endif // FPS_MODE_SUPPORTED

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:
	FSM_Return ProcessPreFSM();
	void UpdateTarget(CPed* pPed);

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool		ProcessPostPreRender();
	virtual bool		ProcessPostCamera();

	virtual CTaskInfo* CreateQueriableState() const;	
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual bool IsInScope(const CPed* pPed);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// FSM Update functions
	FSM_Return Init_OnUpdate(CPed* pPed);

	FSM_Return StreamingClips_OnUpdate();

	void Intro_OnEnter(CPed* pPed);
	FSM_Return Intro_OnUpdate(CPed* pPed);

	FSM_Return OpenDoor_OnUpdate(CPed* pPed);

	void SmashWindow_OnEnter();
	FSM_Return SmashWindow_OnUpdate();
	
	void Hold_OnEnter();
	FSM_Return Hold_OnUpdate(CPed* pPed);

	void Throw_OnEnter();
	FSM_Return Throw_OnUpdate(CPed* pPed);
	void Throw_OnExit();

	void DelayedThrow_OnEnter();
	FSM_Return DelayedThrow_OnUpdate();

	void Outro_OnEnter();
	FSM_Return Outro_OnUpdate();

	void Finish_OnEnter();
	FSM_Return Finish_OnUpdate(CPed* UNUSED_PARAM(pPed)) { return FSM_Quit; }

	bool ThrowProjectileImmediately(CPed* pPed);
	void CalculateTrajectory( CPed* pPed );

	eTaskInput GetInput(CPed* pPed);

private:
	void UpdateAiming(CPed* pPed, bool popIt=false, bool bAllowAimTransition = true);
	bool CalculateAimVector(CPed* pPed, Vector3& vStart, Vector3& vEnd);
	bool CheckForDrop(CPed* pPed);
	void SetNextStateFromNetwork();

	bool IsFirstPersonDriveBy() const;
	bool IsStateValidForFPSIK() const;
	void UpdateFPSMoVESignals();
	void RestartMoveNetwork(bool bUseUpperBodyFilter, bool bRestartNetwork);
	void UpdateMoveFilter();

	void ProcessMeleeCollisions( CPed* pPed );
	phBound* GetCustomHitTestBound( CPed* pPed );
	void TriggerHitReaction( CPed* pPed, CEntity* pHitEnt, sCollisionInfo& refResult, bool bEntityAlreadyHit, bool bBreakableObject );
	bool ShouldUseUpperBodyFilter(CPed* pPed) const;
	bool ShouldDelayThrow() const;
	void CleanUpHitEntities();
	bool GetIsBikeMoving(const CPed& rPed) const;
	bool ShouldCreateWeaponObject(const CPed& rPed) const;
	void ProcessCachedNetworkDamage (CPed* pPed);
	bool GetIsPerformingVehicleMelee() const;

	//// Members ////
	static const fwMvRequestId		ms_StartAim;
	static const fwMvRequestId		ms_StartIntro;
	static const fwMvRequestId		ms_StartThrow;	
	static const fwMvRequestId		ms_Drop;
	static const fwMvRequestId		ms_StartOutro;
	static const fwMvRequestId		ms_StartFinish;		
	static const fwMvRequestId		ms_StartBlocked;
	static const fwMvFlagId			ms_UseSimpleThrow;
	static const fwMvFlagId			ms_UseDrop;
	static const fwMvFlagId			ms_UseThreeAnimThrow;
	static const fwMvFlagId			ms_UseBreathing;
	static const fwMvFlagId			ms_FirstPersonGrip;
	static const fwMvFloatId		ms_AimPhase;
	static const fwMvFloatId		ms_PitchPhase;
	static const fwMvFloatId		ms_ThrowPhase;
	static const fwMvFloatId		ms_IntroPhase;
	static const fwMvFloatId		ms_DropIntroRate;
	static const fwMvBooleanId		ms_ThrowCompleteEvent;	
	static const fwMvBooleanId		ms_OutroFinishedId;		
	static const fwMvBooleanId		ms_OnEnterThrowId;
	static const fwMvBooleanId		ms_OnEnterAimId;	
	static const fwMvBooleanId		ms_DisableGripId;
	static const fwMvClipId			ms_FirstPersonGripClipId;
	static const fwMvFilterId		ms_FirstPersonGripFilter;

	// Vehicle Melee
	static const fwMvClipId			ms_VehMeleeIntroClipId;
	static const fwMvClipId			ms_VehMeleeHoldClipId;
	static const fwMvClipId			ms_VehMeleeHitClipId;
	static const fwMvFlagId			ms_UseVehMelee;
	static phBoundComposite*		sm_pTestBound;
	static const unsigned int		sm_nMaxHitEntities = 8;
	static const fwMvFloatId		ms_VehMeleeIntroToHitBlendTime;
	static const fwMvFloatId		ms_VehMeleeIntroToHoldBlendTime;
	static const fwMvFloatId		ms_VehMeleeHitToIntroBlendTime;
	static const fwMvFloatId		ms_HoldPhase;
	static const fwMvBooleanId		ms_InterruptVehMeleeTag;
	static const fwMvBooleanId		ms_OnEnterIntro;
	// End of vehicle melee

	static const u8					ms_MaxDelay;
	static const bool				ms_AllowDelay;

	fwClipSetRequestHelper			m_ClipSetRequestHelper;
	CMoveNetworkHelper				m_MoveNetworkHelper;
	const CVehicleDriveByInfo*		m_pVehicleDriveByInfo;

	// Grip
	fwMvClipSetId					m_GripClipSetId;

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper*			m_pFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

	fwMvClipSetId					m_iClipSet;
	float							m_fThrowPhase;
	float							m_fCreatePhase;
	float							m_fMinYaw;
	float							m_fMaxYaw;	
	float							m_fCurrentHeading;
	float							m_fDesiredHeading;
	float							m_fCurrentPitch;
	float							m_fDesiredPitch;

	// Throw trajectory
	Vector3 m_vTrajectory;

	u16  m_cloneWaitingToThrowTimer;

	u8 m_iDelay;

	bool m_bThrownGrenade;
	bool m_bWantsToHold;
	bool m_bWantsToFire;
	bool m_bWantsToCook;
	bool m_bFlipping;
	bool m_bOverAimingClockwise;
	bool m_bOverAimingCClockwise;
	bool m_bSimpleThrow;
	bool m_bDrop;
	bool m_bWaitingForDropRelease;
	bool m_bUsingSecondaryTaskNetwork;
	bool m_bCloneWaitingToThrow;
	bool m_bWindowSmashed;
	bool m_bCloneWindowSmashed;
	bool m_bCloneThrowComplete;
	bool m_bHadValidWeapon;
	bool m_bNeedsToOpenDoors;
	bool m_bDisableGripFromThrow;
	bool m_bEquippedWeaponChanged;
	bool m_bWantsToDrop;

	bool m_bInterruptible;

	// Vehicle Melee
	bool m_bVehMelee;
	u32 m_nHitEntityCount;
	CEntity* m_hitEntities[sm_nMaxHitEntities];
	float m_fIntroToThrowTransitionDuration;
	bool m_bUseUpperBodyFilter;
	Matrix34 m_matLastKnownWeapon;
	bool m_bLastKnowWeaponMatCached;
	float m_fThrowIkBlendInStartPhase;
	bool m_bVehMeleeRestarting;
	bool m_bInterruptingWithOpposite;
	bool m_bRestartingWithOppositeSide;
	bool m_bVehMeleeStartingRightSide;
	bool m_bHaveAlreadySetPlayerInfo;
	bool m_bOwnersCurrentSideRight;
	u16 m_nUniqueNetworkVehMeleeActionID;
	bool m_bStartedByVehicleMelee;

	static u8 sm_uMeleeDamageCounter;
	static const int s_MaxCachedMeleeDamagePackets = 10;
	static CTaskMountThrowProjectile::BikeMeleeNetworkDamage sm_CachedMeleeDamage[s_MaxCachedMeleeDamagePackets];

public:

	static const float ms_fBikeStationarySpeedThreshold;
	static u8 m_nActionIDCounter;
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskMountThrowProjectile
//////////////////////////////////////////////////////////////////////////

class CClonedMountThrowProjectileInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedMountThrowProjectileInfo(){}
	CClonedMountThrowProjectileInfo(s32 mountThrowTransportState, const fwFlags32& iFlags, const CWeaponTarget& target, bool bDrop, bool bWindowSmashed, bool bInteruptingHoldWithOtherSide, bool bCurrentSwipeSideRight, bool bStartingRighSide, u16 nUniqueNetworkVehMeleeActionID, bool bStartedByVehicleMelee)
	: m_iFlags(iFlags)	
	, m_target(target)
	, m_bDrop(bDrop)
	, m_bWindowSmashed(bWindowSmashed)
	, m_bInteruptingHoldWithOtherSide(bInteruptingHoldWithOtherSide)
	, m_bVehMeleeStartingRightSide(bStartingRighSide)
	, m_bIsCurrentSwipeSideRight(bCurrentSwipeSideRight)
	, m_nUniqueNetworkVehMeleeActionID(nUniqueNetworkVehMeleeActionID)
	, m_bStartedByVehicleMelee(bStartedByVehicleMelee)
	{
		SetStatusFromMainTaskState(mountThrowTransportState);		
	}
	~CClonedMountThrowProjectileInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_MOUNT_THROW_PROJECTILE;}

	virtual bool    HasState() const { return true; }
	virtual u32	    GetSizeOfState() const { return datBitsNeeded<CTaskMountThrowProjectile::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Status";}
	virtual CTaskFSMClone *CreateCloneFSMTask();

	bool GetDrop() const { return m_bDrop; }
	bool GetWindowSmashed() const { return m_bWindowSmashed; }
	bool GetInteruptingHoldWithOtherSide() const { return m_bInteruptingHoldWithOtherSide; }
	bool GetVehMeleeStartingRightSide() const { return m_bVehMeleeStartingRightSide; }
	bool GetIsCurrentSwipeSideRight() const { return m_bIsCurrentSwipeSideRight; }
	u16 GetUniqueNetworkVehMeleeActionID() const { return m_nUniqueNetworkVehMeleeActionID; }
	bool GetStartedByVehicleMelee() const { return m_bStartedByVehicleMelee; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_BOOL(serialiser, m_bDrop, "Drop");
		SERIALISE_BOOL(serialiser, m_bWindowSmashed, "Window smashed");
		SERIALISE_BOOL(serialiser, m_bInteruptingHoldWithOtherSide, "Interrupted with other side");
		SERIALISE_BOOL(serialiser, m_bVehMeleeStartingRightSide, "Starting with right side swipe");
		SERIALISE_BOOL(serialiser, m_bIsCurrentSwipeSideRight, "Current swipe side");
		SERIALISE_UNSIGNED(serialiser, m_nUniqueNetworkVehMeleeActionID, 16, "Unique veh melee action ID");
		SERIALISE_BOOL(serialiser, m_bStartedByVehicleMelee, "Started by vehicle melee");
	}

private:

	fwFlags32 m_iFlags;
	CWeaponTarget m_target;
	bool m_bDrop;
	bool m_bWindowSmashed;
	bool m_bInteruptingHoldWithOtherSide;
	bool m_bVehMeleeStartingRightSide;
	bool m_bIsCurrentSwipeSideRight;
	u16 m_nUniqueNetworkVehMeleeActionID;
	bool m_bStartedByVehicleMelee;
};


#endif // INC_TASK_MOUNT_ANIMAL_WEAPON_H
