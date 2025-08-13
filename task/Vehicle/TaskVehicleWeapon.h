#ifndef INC_TASK_VEHICLE_WEAPON_H
#define INC_TASK_VEHICLE_WEAPON_H
/********************************************************************
filename:	TaskVehicleWeapon.h
created:	13/02/2009
author:		jack.potter

purpose:	Tasks related to combat in vehicles
*********************************************************************/

#include "Task/System/TaskFSMClone.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Task/Weapons/WeaponController.h"

#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Vehicles/VehicleGadgets.h"

class CVehicleDriveByInfo;
class CVehicleWeapon;
class CWeaponInfo;
class CCarDoor;
class CTurret;

typedef enum
{
	DBF_AlternateFireFront				= (1<<0),		// Old
	DBF_AlternateFireRear				= (1<<1),		// Old
	DBF_IsVanFlagNeededForRear			= (1<<2),		// Obsolete
	DBF_NoRoofNeeded					= (1<<3),		// obsolete
	DBF_RoofNeeded						= (1<<4),		// obsolete
	DBF_AdjustVerticalHeightFS			= (1<<5),		
	DBF_HasRifle						= (1<<6),
	DBF_HasGrenades						= (1<<7),			
	DBF_AllSeatsUseFrontClips			= (1<<8),
	DBF_NoRearDrivebys					= (1<<9),
	DBF_NoDriverDrivebys				= (1<<10),
	DBF_DisableFrontDrivebys			= (1<<11),
	DBF_BackRightSeatUsesPassengerClips = (1<<12),
	DBF_NoDriverGrenadeDrivebys			= (1<<13),		
	DBF_DriverNeedsIntoClips			= (1<<14),
	DBF_PassengersDontNeedIntroClips	= (1<<15),
	DBF_DampenRecoilClip				= (1<<16),
	DBF_BackLeftSeatUsesDriverClips		= (1<<17),
	DBF_JustPauseFiringForReload		= (1<<18),
	DBF_DriverLimitsToSmashWindows		= (1<<19),

	// Use alternate additive clip when firing across the vehicle
} DriveByFlags;

// Define to use the mounted gun PM
#define USE_MOUNTED_GUN_PM 0

// Purpose:
// Task to take care of aiming and firing mounted vehicle weapons
class CTaskVehicleMountedWeapon : public CTaskFSMClone
{
	friend class CClonedControlTaskVehicleMountedWeaponInfo;


public:

	enum eUseVehicleWeaponState
	{
		State_StreamingClips,
		State_Idle,
		State_Aiming,
		State_Firing,
		State_Finish
	};

	enum eTaskMode
	{
		Mode_Player,	// Use pad controls
		Mode_Idle,		// Just point weapon straight forward
		Mode_Aim,		// Aim at target
		Mode_Fire,		// Fire at target
		Mode_Camera,	// Aim down the active camera
		Mode_Search,	// Search around for target
	};

	CTaskVehicleMountedWeapon(eTaskMode mode, const CAITarget* target = NULL, float fFireTolerance = ( DtoR * 20.0f));
	virtual ~CTaskVehicleMountedWeapon();

	virtual aiTask*				Copy() const { return rage_new CTaskVehicleMountedWeapon(m_mode,&m_target); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON; }

	virtual bool				ProcessPostCamera();
	
	// Required to call reset flag for PostCamera
	virtual bool				ProcessMoveSignals();
	virtual bool				ProcessPostPreRenderAfterAttachments();

	virtual s32					GetDefaultStateAfterAbort() const { return State_StreamingClips; }

#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// Fill out vTargetPosOut with the intended target position
	// Returns false if we don't have a valid target position
	bool GetTargetInfo(CPed* pPed, Vector3& vTargetPosOut, const CEntity*& pTargetOut, bool bDriveTurretTarget = false) const;

	void SetTargetPosition(const Vector3 &vWorldTargetPos) { m_target.SetPosition(vWorldTargetPos); }
	void SetTarget(const CEntity* pTarget, const Vector3* vOffset = NULL) { vOffset ? m_target.SetEntityAndOffset(pTarget, *vOffset) : m_target.SetEntity(pTarget);}
	void SetMode(eTaskMode newMode) { m_mode = newMode; }
	void SetFireTolerance(float fireTolerance) { m_fFireTolerance = fireTolerance; }
	bool IsAimingOrFiring() const { return GetState() == State_Aiming || GetState() == State_Firing; }

	// Some checks for validity put into a single function
	static bool IsTaskValid(CPed* pPed);
	// Clone task implementation for CTaskVehicleMountedWeapon
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

	bool IsFiring() const
	{
		return GetState() == State_Firing; 
	}

	bool IsAiming() const
	{
		return GetState() == State_Aiming; 
	}

	bool WantsToFire(CPed* pPed) const;
	bool WantsToAim(CPed* pPed) const;

	bool FiredThisFrame() const { return m_bFiredThisFrame; }

	bool ShouldDimReticule() const { return m_DimRecticle; }

	static void ComputeSearchTarget( Vector3& o_Target, CPed& in_Ped );

	static int GetVehicleTargetingFlags(CVehicle *pVehicle, const CPed *pPed, const CVehicleWeapon* pEquippedVehicleWeapon, CEntity* pCurrentTarget);
	static CEntity* FindTarget(CVehicle *pVehicle, 
		const CPed *pPed, 
		const CVehicleWeapon* pEquippedVehicleWeapon, 
		CEntity* pCurrentTarget, 
		CEntity* pCurrentTargetPreSwitch, 
		int iTargetingFlags, 
		float &fWeaponRange, 
		float &fHeadingLimit,
		float &fPitchLimitMin, 
		float &fPitchLimitMax);

#if __BANK
	static int iTurretTuneDebugTimeToLive;
#endif

protected:
	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void		CleanUp();

	// Is the specified state handled by the clone FSM
	bool StateChangeHandledByCloneFSM(s32 iState);

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StreamingClips_OnUpdate(CPed* pPed);

	void		Idle_OnEnter(CPed* pPed);
	FSM_Return	Idle_OnUpdate(CPed* pPed);

	void		Aiming_OnEnter(CPed* UNUSED_PARAM(pPed));
	FSM_Return	Aiming_OnUpdate(CPed* pPed);
	FSM_Return	Aiming_OnUpdateClone(CPed* UNUSED_PARAM(pPed));

	void		Firing_OnEnter(CPed* UNUSED_PARAM(pPed));
	FSM_Return	Firing_OnUpdate(CPed* pPed);
	FSM_Return	Firing_OnUpdateClone(CPed* pPed);

	void ProcessIgnoreLookBehindForLocalPlayer(CPed* pPed);

	// Make vehicle weapons associated with the ped's seat point in the right direction
	void ControlVehicleWeapons(CPed* pPed, bool bFireWeapons);
	
	bool ShouldPreventAimingOrFiringDueToLookBehind() const;
	const CTurret* GetHeldTurretIfShouldSyncLocalDirection() const;

	bool EvaluateTerrainForFireDirection(Vector3 &vFireDirection);
	bool DoTerrainProbe(const CVehicle* pVehicle, Vector3 &start, Vector3 &end, WorldProbe::CShapeTestHitPoint &hitPoint, bool bIncludeVehicles);

	void ProcessLockOn(CPed* pPed);
	void ProcessVehicleWeaponControl(CPed& rPed);

	void CheckForWeaponSpin(CPed* pPed);

	CVehicleWeapon* GetEquippedVehicleWeapon(const CPed* pPed) const;

	RegdEnt		m_pPotentialTarget;
	float		m_fPotentialTargetTimer;
	u32			m_uManualSwitchTargetTime;
	float		m_fTargetingResetTimer;

	CAITarget	m_target; 
	Vector3		m_targetPosition;   //for serialising to remote, dont want to use m_target since manually setting position will NULL entity pointer which may still be needed
	Vector3		m_targetPositionCam; //for serialising to remote, the position that will be used to drive the physical turret. m_targetPosition is actualy reticule position that's camera independent

	// For streaming in the mounted vehicle clips
	//fwClipSetRequestHelper m_clipSetRequestHelper;

#if USE_MOUNTED_GUN_PM
	// Streaming helper for the mounted weapon PM
	CPmRequestHelper	m_pmRequestHelper;
	CClipHelper			m_pmHelper;

	// static vars
	static char* ms_szPmDictName;
	static char* ms_szPmDataName;
	static s32 ms_siPmDictIndex;
#endif //USE_MOUNTED_GUN_PM

	eTaskMode m_mode;

	// How close to we have to be to target before we open fire?
	float m_fFireTolerance;
	float m_fTimeLookingBehind;

	// Has fired at least once
	bool m_bFiredAtLeastOnce;
	bool m_bFiredThisFrame;
	bool m_bCloneCanFire;
	bool m_bCloneFiring;

	// Dim the reticle if the weapon !IsWithinFiringAngularThreshold
	bool m_DimRecticle;

	// Used for spycar to track whether weapon has changed to be above/below water
	bool m_bInWater;
	bool m_bForceShotInWeaponDirection;

	float m_fChargedLaunchTimer;
	bool m_bWasReadyToFire;
	bool m_bFireWeaponLastFrame;

	// Static members
	// Is lockon button pressed for for this vehicle type
	static bool IsLockonEnabled(CVehicle* pVehicle, CVehicleWeapon* pEquippedVehicleWeapon);
	static bool IsLockonPressedForVehicle(CPed* pPlayerPed, CVehicle* pVehicle, CVehicleWeapon* pEquippedVehicleWeapon);
	bool CanFireAtTarget(CPed* pPed);
};

//-------------------------------------------------------------------------
// Task info for CTaskVehicleMountedWeapon
//-------------------------------------------------------------------------
class CClonedControlTaskVehicleMountedWeaponInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedControlTaskVehicleMountedWeaponInfo();
	CClonedControlTaskVehicleMountedWeaponInfo(CTaskVehicleMountedWeapon::eTaskMode mode, const CEntity *targetEntity, const Vector3 &targetPosition, const Vector3 &targetPositionCam, const u32 vehicleMountedWeaponState, const float fFireTolerance, bool firingWeapon );
	~CClonedControlTaskVehicleMountedWeaponInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_VEHICLE_MOUNTED_WEAPON;}
	
	CTaskVehicleMountedWeapon::eTaskMode GetMode() const { return m_mode; }
	CEntity*					GetTargetEntity() { return m_targetEntity.GetEntity(); }
	const Vector3             &GetTargetPosition() const { return m_targetPosition; }
	const Vector3             &GetTargetCameraPosition() const { return m_targetPositionCam; }
	float						GetFireTolerance()	const {return m_fFireTolerance;}
	bool						GetFiringWeapon() const	{return m_firingWeapon;}
	
	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity* pEntity);

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return CTaskVehicleMountedWeapon::State_Finish;}
	virtual const char*	GetStatusName(u32) const { return "Vehicle Mounted Weapon State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		
		u32 mode = static_cast<u32>(m_mode);
		SERIALISE_UNSIGNED(serialiser, mode, SIZEOF_MODE, "Mode");
		m_mode = static_cast<CTaskVehicleMountedWeapon::eTaskMode>(mode);

		ObjectId targetID = m_targetEntity.GetEntityID();

		bool hasTargetID = targetID != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_BOOL(serialiser, hasTargetID, "Has Target ID");

		if(hasTargetID)
		{
			SERIALISE_OBJECTID(serialiser, targetID, "Target entity");
		}
		else
		{
			SERIALISE_POSITION(serialiser, m_targetPosition, "Target Position");
			SERIALISE_POSITION(serialiser, m_targetPositionCam, "Target Camera Position");
		}

		m_targetEntity.SetEntityID(targetID);

		SERIALISE_PACKED_FLOAT(serialiser, m_fFireTolerance, 50.0f, SIZEOF_FIRE_TOLERANCE, "Fire tolerance");
		SERIALISE_BOOL(serialiser, m_firingWeapon, "Firing weapon");
	}

private:
	static const unsigned int SIZEOF_MODE = 3;
	static const unsigned int SIZEOF_FIRE_TOLERANCE	 = 32;

	CTaskVehicleMountedWeapon::eTaskMode m_mode;

	CSyncedEntity              m_targetEntity;  //NOTE: pEntity is required by CVehicleWeapon::Fire but there is no current use made of it as far as I can see and just pasisng position would probably suffice
	Vector3                    m_targetPosition;
	Vector3					   m_targetPositionCam;
	float					   m_fFireTolerance;
	bool					   m_firingWeapon;
};

//////////////////////////////////////////////////////////////////////////
// Class CTaskVehicleGun
// The new complex drive by task
class CTaskVehicleGun : public CTaskFSMClone
{
public:

	static bank_float ms_fAIMinTimeBeforeFiring;
	static bank_float ms_fAIMaxTimeBeforeFiring;

	// does this ped need to smash the window to fire at the desired heading?
	// Calculates the phase from the target position in world space
	static bool NeedsToSmashWindowFromTargetPos(const CPed* pPed, const Vector3 &vPosition, bool bHeadingCheckOnly = false);

	// If the window is partially broken, then add some force to smash the rest out
	static bool ClearOutPartialSmashedWindow(const CPed* pPed, const Vector3 &vPosition);

	static bool GetRestrictedAimSweepHeadingAngleRads(const CPed& rPed, Vector2& vMinMax);

	static const CWeaponInfo* GetPedWeaponInfo(const CPed& rPed);

	static CCarDoor* GetDoorForPed(const CPed* pPed);

	enum eVehicleGunState
	{
		State_Init,
		State_StreamingClips,
		State_SmashWindow,
		State_WaitingToFire,
		State_OpeningDoor,
		State_Gun,
		State_Finish
	};

	// Modes are specified at creation time
	enum eTaskMode
	{
		Mode_Player = 0,// Player's pad inputs controls firing
		Mode_Aim,		// Aim at target only
		Mode_Fire,		// Fire at target
		Num_FireModes
	};

	// If firing pattern is 0 then ped will fire continuously.
	// Shoot rate modifier affects time between bursts if there is a firing pattern hash,
	// otherwise it affects time between shots
	CTaskVehicleGun(eTaskMode mode, u32 uFiringPatternHash = 0, const CAITarget* pTarget = NULL, float fShootRateModifier = 1.0f, float fAbortRange = -1.0f);
	virtual ~CTaskVehicleGun();

	virtual aiTask*				Copy() const { return rage_new CTaskVehicleGun(m_mode,m_uFiringPatternHash,&m_target,1.0f/m_fShootRateModifierInv, m_fAbortRange); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GUN; }

	virtual s32					GetDefaultStateAfterAbort() const { return State_Init; }

    virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }
    virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
    virtual bool                IsInScope(const CPed* pPed);
	virtual float				GetScopeDistance() const;

	const CWeaponInfo*			GetPedWeaponInfo() const;

	bool						GetAimVectors(Vector3& vStart, Vector3& vEnd) {if (m_bAimComputed) {vStart=m_vAimStart; vEnd = m_vAimEnd;} return m_bAimComputed;}

#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	void SetTargetPosition(const Vector3 &vWorldTargetPos) { m_target.SetPosition(vWorldTargetPos); }
	void SetTarget(const CEntity* pTarget, const Vector3* vOffset = NULL) { vOffset ? m_target.SetEntityAndOffset(pTarget, *vOffset) : m_target.SetEntity(pTarget);}
	const CAITarget& GetTarget() const { return m_target; }
	bool GetTargetPosition(const CPed* pPed, Vector3& vTargetPosWorldOut);

	// This task supports termination requests
	virtual bool SupportsTerminationRequest() { return true; }


protected:
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void		CleanUp();

	FSM_Return			Init_OnUpdate();

	FSM_Return			StreamingClips_OnUpdate();

	void				SmashWindow_OnEnter();
	FSM_Return			SmashWindow_OnUpdate();

	void				OpeningDoor_OnEnterClone();
	FSM_Return			OpeningDoor_OnUpdate();
	void				OpeningDoor_OnExitClone();

	FSM_Return			WaitingToFire_OnUpdate();

	void				Gun_OnEnter();
	FSM_Return			Gun_OnUpdate();

	void				ProcessWeaponObject(CPed* pPed);

	void				SelectClipSet(const CPed *pPed);
	bool				ShouldRestartStateDueToCameraSwitch();

	void				ProcessLookAtWhileWaiting(CPed* pPed);

	//// Member variables ////
	//////////////////////////

	CAITarget						m_target;
	eTaskMode						m_mode;

	fwMvClipSetId					m_iClipSet;
	float							m_fMinYaw;
	float							m_fMaxYaw;

	Vector3							m_vAimStart;
	Vector3							m_vAimEnd;

	fwClipSetRequestHelper			m_ClipSetRequestHelper;

	// Script can edit how fast the peds fire
	float							m_fShootRateModifierInv;

	// If target is further away than this then task will abort
	// A value of <0 is interpreted as infinite
	float							m_fAbortRange;

	bool							m_bWeaponInLeftHand;

	// Do we need to destroy the weapon object in the wait state?
	bool							m_bShouldDestroyWeaponObject;	

	bool							m_bNeedToDriveDoorOpen;

	bool							m_bUsingDriveByOverrideAnims;

	bool							m_bAimComputed;

	bool							m_bRestarting;
	bool							m_bInFirstPersonVehicleCamera;

	u32								m_uFiringPatternHash;

	bool							m_bSkipDrivebyIntroOnRestart;
	bool							m_bInstantlyBlendFPSDrivebyAnims;

	// Remove when all vehicles are setup
	bool m_bSmashedWindow;
	bool m_bRolledDownWindow;

	CTaskTimer m_firingDelayTimer;

#if __BANK
	// Debug target pos
	Vector3 m_vTargetPos;
#endif 

	//// Static utility functions ////
	//////////////////////////////////

	// Can this ped fire at the desired angle?
	static bool CheckIfOutsideRange(float fDesiredYaw, float fMinYaw, float fMaxYaw, bool bAllowSingleSideOvershoot = false);
	static bool CheckFiringConditions( const CPed *pPed, Vec3V_In vTargetPosition, float fDesiredYaw, float fMinYaw, float fMaxYaw, bool bBlockOutsideRange = false);
	// Can ped fire from current vehicle?
	static bool CheckVehicleFiringConditions( const CPed* pPed, Vec3V_In vTargetPosition );

	static void CalculateYawAndPitchToTarget(const CPed* pPed, const Vector3& vTarget, float &fYaw, float &fPitch);
};

//-------------------------------------------------------------------------
// Task info for firing a weapon while in a vehicle
//-------------------------------------------------------------------------
class CClonedVehicleGunInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedVehicleGunInfo(CTaskVehicleGun::eTaskMode mode, const CEntity *targetEntity, const Vector3 &targetPosition, const u32 taskState);
    CClonedVehicleGunInfo() {}
    ~CClonedVehicleGunInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_VEHICLE_GUN;}
	
    CTaskVehicleGun::eTaskMode GetMode() const { return m_mode; }
    CEntity                   *GetTargetEntity() { return m_targetEntity.GetEntity(); }
    const Vector3             &GetTargetPosition() const { return m_targetPosition; }

    virtual bool    HasState() const { return true; }
    virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskVehicleGun::State_Finish>::COUNT;}
    static const char*	GetStateName(s32)  { return "Task State";}
	virtual CTaskFSMClone *CreateCloneFSMTask();

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);

        u32 mode = static_cast<u32>(m_mode);
        SERIALISE_UNSIGNED(serialiser, mode, SIZEOF_MODE, "Mode");
        m_mode = static_cast<CTaskVehicleGun::eTaskMode>(mode);

        ObjectId targetID = m_targetEntity.GetEntityID();
        SERIALISE_OBJECTID(serialiser, targetID, "Target entity");
        m_targetEntity.SetEntityID(targetID);

		SERIALISE_POSITION(serialiser, m_targetPosition, "Target Position");
	}

private:

    static const unsigned int SIZEOF_MODE = 2;

    CClonedVehicleGunInfo(const CClonedVehicleGunInfo &);

    CClonedVehicleGunInfo &operator=(const CClonedVehicleGunInfo &);

    CTaskVehicleGun::eTaskMode m_mode;
    bool                       m_firingWeapon;
    CSyncedEntity              m_targetEntity;
    Vector3                    m_targetPosition;
};

//////////////////////////////////////////////////////////////////////////
// Class CTaskVehicleProjectile
// The new throw grenade from vehicle class
class CTaskVehicleProjectile : public CTaskFSMClone
{
	// For internal use
	enum eTaskInput
	{
		Input_None,
		Input_Hold,
		Input_Fire
	};

public:

	enum eVehicleProjectileState
	{
		State_Init,
		State_StreamingClips,
		State_WaitingToFire,		
		State_SmashWindow,
		State_Intro,			// Intro clip
		State_Hold,				// hold the projectile primed in hand
		State_Throw,
		State_Finish
	};

	enum eTaskMode
	{
		Mode_Player,
		Mode_Fire
	};

	CTaskVehicleProjectile(eTaskMode mode);

	virtual aiTask*				Copy() const { return rage_new CTaskVehicleProjectile(m_taskMode); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PROJECTILE; }

	virtual s32				GetDefaultStateAfterAbort() const { return State_Init; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:
	FSM_Return ProcessPreFSM();

	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool                IsInScope(const CPed* pPed);

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// FSM Update functions
	FSM_Return Init_OnUpdate(CPed* pPed);

	FSM_Return StreamingClips_OnUpdate();

	FSM_Return WaitingToFire_OnUpdate();

	void SmashWindow_OnEnter(CPed* pPed);
	FSM_Return SmashWindow_OnUpdate(CPed* pPed);

	void Intro_OnEnter(CPed* pPed);
	FSM_Return Intro_OnUpdate(CPed* pPed);

	void Hold_OnEnter(CPed* pPed);
	FSM_Return Hold_OnUpdate(CPed* pPed);

	void Throw_OnEnter(CPed* pPed);
	FSM_Return Throw_OnUpdate(CPed* pPed);

	FSM_Return Finish_OnUpdate(CPed* UNUSED_PARAM(pPed)) { return FSM_Quit; }

	bool GetTargetPosition(const CPed* pPed, Vector3& vTargetPosWorldOut);
	bool ThrowProjectileImmediately(CPed* pPed);

	eTaskInput GetInput(CPed* pPed);

	//// Members ////

	// Currently all of the throw projectile stuff is done with one clip:
	// It has an intro, then loop 1 is a hold section
	// and after loop 1 is the throw clip
	// with a AEF_FIRE_1 event when the projectile is actually thrown
	fwClipSetRequestHelper m_ClipSetRequestHelper;

	static fwMvClipId ms_ThrowGrenadeClipId;
	fwMvClipSetId m_ClipSet;
	eTaskMode m_taskMode;

	bool m_bSmashedWindow;
	bool m_bThrownGrenade;

	float m_fHoldPhase;

	// Used to keep track of if the ped has stopped holding fire
	// Want it so that if player lets go and then presses fire again before the
	// projectile is thrown then the projectile will still be thrown
	bool m_bWantsToHold;
	bool m_bWantsToFire;

};

//////////////////////////////////////////////////////////////////////////
// CClonedVehicleProjectileInfo
//////////////////////////////////////////////////////////////////////////

class CClonedVehicleProjectileInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedVehicleProjectileInfo(){}
	CClonedVehicleProjectileInfo(s32 vehicleProjectileState, bool bWantsToHold)
		: m_bWantsToHold(bWantsToHold)
	{
		SetStatusFromMainTaskState(vehicleProjectileState);		
	}
	~CClonedVehicleProjectileInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_VEHICLE_PROJECTILE;}

	bool GetWantsToHold() const { return m_bWantsToHold; }

	virtual bool    HasState() const { return true; }
	virtual u32	    GetSizeOfState() const { return datBitsNeeded<CTaskVehicleProjectile::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Status";}
	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bWantsToHold, "m_bWantsToHold");
	}

private:

	bool m_bWantsToHold;
};

#endif // INC_TASK_VEHICLE_WEAPON_H
