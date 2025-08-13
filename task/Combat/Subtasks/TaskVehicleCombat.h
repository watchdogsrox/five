// FILE :    TaskVehicleCombat.h
// PURPOSE : Subtask of combat used to attack a target whilst in a vehicle
// AUTHOR :  Phil H

#ifndef TASK_VEHICLE_COMBAT_H
#define TASK_VEHICLE_COMBAT_H

// Game headers
#include "AI/AITarget.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "game/Wanted.h"


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
class CTaskVehiclePersuit : public CTask
{

public:

	enum ConfigFlags
	{
		CF_ExitImmediately	= BIT0,
	};

private:

	enum RunningFlags
	{
		RF_TargetChanged					= BIT0,
		RF_ProvidingCoverDuringExitVehicle	= BIT1,
		RF_CanExitVehicleEarly				= BIT2,
		RF_ForceExitVehicleWarp				= BIT3,
		RF_HasSaidInitialAudio				= BIT4,
		RF_HasSaidFollowUpAudio				= BIT5,
		RF_IsBlockedByLockedDoor			= BIT6,
		RF_RunningTempAction				= BIT7,
		RF_SlowingToReasonableSpeed			= BIT8,
	};

public:

	struct Tunables : public CTuning
	{
		struct ApproachTarget
		{
			ApproachTarget()
			{}

			float m_TargetArriveDist;
			float m_CruiseSpeed;
			float m_MaxDistanceToConsiderClose;
			float m_CruiseSpeedWhenClose;
			float m_CruiseSpeedWhenObstructedByLawEnforcementPed;
			float m_CruiseSpeedWhenObstructedByLawEnforcementVehicle;
			float m_CruiseSpeedTooManyNearbyEntities;
			float m_DistanceToConsiderCloseEntitiesTarget;
			float m_DistanceToConsiderEntityBlocking;
			int	  m_MaxNumberVehiclesNearTarget;
			int   m_MaxNumberPedsNearTarget;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		ApproachTarget m_ApproachTarget;

		float	m_ObstructionProbeAngleA;
		float	m_ObstructionProbeAngleB;
		float	m_ObstructionProbeAngleC;
		float	m_IdealDistanceOnBikeAndTargetUnarmed;
		float	m_IdealDistanceOnBikeAndTargetArmed;
		float	m_IdealDistanceInVehicleAndTargetUnarmed;
		float	m_IdealDistanceInVehicleAndTargetArmed;
		float	m_IdealDistanceShotAt;
		float	m_IdealDistanceCouldLeaveCar;
		float	m_DistanceToStopMultiplier;
		float	m_DistanceToStopMassIdeal;
		float	m_DistanceToStopMassWeight;
		float	m_MinDriverTimeToLeaveVehicle;
		float	m_MaxDriverTimeToLeaveVehicle;
		float	m_MinPassengerTimeToLeaveVehicle;
		float	m_MaxPassengerTimeToLeaveVehicle;
		float	m_MaxSpeedForEarlyCombatExit;
		float	m_MinSpeedToJumpOutOfVehicle;
		float	m_MinTimeBoatOutOfWaterForExit;
		float	m_AvoidanceMarginForOtherLawEnforcementVehicles;
		float	m_TargetArriveDistForApproachTarget;
		float	m_MinTimeToWaitForOtherPedToExit;
		float	m_MinDelayExitTime;
		float	m_MaxDelayExitTime;
		float	m_PreventShufflingExtraRange;
		float	m_MaxTimeWaitForExitBeforeWarp;
		float	m_MinTargetStandingOnTrainSpeed;
		int		m_DistanceToFollowInCar;

		PAR_PARSABLE;
	};

	static bank_float EXIT_FAILED_TIME_IN_STATE;

	// State
	typedef enum
	{
		State_invalid = -1,
		State_start = 0,
		State_fleeFromTarget,
		State_WaitWhileCruising,
		State_attack,
		State_chaseInCar,
		State_followInCar,
		State_heliCombat,
		State_subCombat,
		State_boatCombat,
		State_passengerInVehicle,
		State_waitForBuddiesToEnter,
		State_slowdownSafely,
		State_emergencyStop,
		State_exitVehicle,
		State_exitVehicleImmediately, // Immediately, doesn't wait for the car to slow
		State_provideCoverDuringExitVehicle,
		State_approachTarget,
		State_waitForVehicleExit,
		state_waitInCar,
		State_exit
	} VehiclePersuitState;

	// Constructor/destructor
	CTaskVehiclePersuit(const CAITarget& target, fwFlags8 uConfigFlags = 0);
	~CTaskVehiclePersuit();

	virtual aiTask* Copy() const { return rage_new CTaskVehiclePersuit(m_target, m_uConfigFlags); }
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_VEHICLE_PERSUIT;}

	// FSM implementations
	FSM_Return		ProcessPreFSM		();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	FSM_Return		ProcessPostFSM();
	virtual s32	GetDefaultStateAfterAbort()	const {return State_exit;}
	virtual bool MayDeleteOnAbort() const { return true; }	// This task doesn't resume, OK to delete if aborted.

	// Ik 
	void			ProcessIk();

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// Update the target externally, will set a flag to restart the current task if
	// the target has changed.
	const CEntity*  GetTarget() const {return m_target.GetEntity(); }
	void			SetTarget(CAITarget& val);
	static s32		CountNumberBuddiesEnteringVehicle( CPed* pPed, CVehicle* pVehicle );
	void			CountNumberVehiclesAndPedsNearTarget(s32& iNumVehicles, s32& iNumPeds) const;
	bool			ShouldStopDueToBlockingLawVehicles() const;
	static bool		IsPedOnMovingVehicleOrTrain(const CPed* pPed);
	static Tunables &GetTunables() { return sm_Tunables; }

private:
	// State implementations
	// State_start
	FSM_Return	Start_OnUpdate				(CPed* pPed);
	// ChaseInCar
	void		ChaseInCar_OnEnter			(CPed* pPed);
	FSM_Return	ChaseInCar_OnUpdate			(CPed* pPed);
	// FleeFromTarget
	void		FleeFromTarget_OnEnter		(CPed* pPed);
	FSM_Return	FleeFromTarget_OnUpdate		(CPed* pPed);
	// WaitWhileCrusiing
	void		WaitWhileCruising_OnEnter	();
	FSM_Return	WaitWhileCruising_OnUpdate	();
	// Attack
	void		Attack_OnEnter				();
	FSM_Return	Attack_OnUpdate				();
	// FollowInCar
	void		FollowInCar_OnEnter			(CPed* pPed);
	FSM_Return	FollowInCar_OnUpdate		(CPed* pPed);
	// HeliCombat
	void		HeliCombat_OnEnter			();
	FSM_Return	HeliCombat_OnUpdate			();
	// SubCombat
	void		SubCombat_OnEnter			();
	FSM_Return	SubCombat_OnUpdate			();
	// BoatCombat
	void		BoatCombat_OnEnter			();
	FSM_Return	BoatCombat_OnUpdate			();
	// PassengerInVehicle
	void		PassengerInVehicle_OnEnter	(CPed* pPed);
	FSM_Return	PassengerInVehicle_OnUpdate	(CPed* pPed);
	// SlowdownSafely
	void		SlowdownSafely_OnEnter		(CPed* pPed);
	FSM_Return	SlowdownSafely_OnUpdate		(CPed* pPed);
	// EmergencyStop
	void		EmergencyStop_OnEnter		(CPed* pPed);
	FSM_Return	EmergencyStop_OnUpdate		(CPed* pPed);
	void		EmergencyStop_OnExit		();
	// ExitVehicle
	void		ExitVehicle_OnEnter			(CPed* pPed);
	FSM_Return	ExitVehicle_OnUpdate		(CPed* pPed);
	// ExitVehicleImmediately Immediately, doesnt wait for the car to slow
	void		ExitVehicleImmediately_OnEnter	(CPed* pPed);
	FSM_Return	ExitVehicleImmediately_OnUpdate	(CPed* pPed);
	// WaitForBuddiesToEnter
	void		WaitForBuddiesToEnter_OnEnter	(CPed* pPed);
	FSM_Return	WaitForBuddiesToEnter_OnUpdate	(CPed* pPed);
	// ProvideCoverDuringExitVehicle
	void		ProvideCoverDuringExitVehicle_OnEnter	(CPed* pPed);
	FSM_Return	ProvideCoverDuringExitVehicle_OnUpdate	(CPed* pPed);
	void		ProvideCoverDuringExitVehicle_OnExit	(CPed* pPed);
	// ApproachTarget
	void		ApproachTarget_OnEnter	();
	FSM_Return	ApproachTarget_OnUpdate	();
	// Wait for vehicle exit
	void		WaitForVehicleExit_OnEnter();
	FSM_Return	WaitForVehicleExit_OnUpdate();
	void		WaitInCar_OnEnter();
	FSM_Return	WaitInCar_OnUpdate();


	// Helper functions - returns a new state if the ped should exit the current vehicle
	VehiclePersuitState GetDesiredState(CPed* pPed);
	bool				GetDesiredAircraftState(CPed* pPed, CTaskVehiclePersuit::VehiclePersuitState &iDesiredState);
	
			void				ActivateTimeslicing();
			bool				AreAbnormalExitsDisabledForSeat() const;
			Vec3V_Out			CalculateDirectionForPed(const CPed& rPed) const;
			bool				CanChangeState() const;
			bool				CanExitVehicle() const;
			VehiclePersuitState	ChooseDefaultState() const;
			bool				DoesPedHaveLosToTarget(const CPed& rPed, const CPed& rTarget) const;
			bool				FindObstructions(bool& bLeft, bool& bRight);
			bool				FindObstructionAtAngle(const CVehicle* pVehicle, Vec3V_ConstRef vStart, const float fAngle, const float fMagnitude, bool bIsOffRoad);
			float				GetDistToStopAtCurrentSpeed(const CVehicle& rVehicle) const;
			bool				GetObstructionInformation(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation& rInformation) const;
			CPed*				GetTargetMount() const;
	const	CPed*				GetTargetPed() const;
			bool				GetTargetPositionForApproach(Vec3V_InOut vTargetPosition) const;
			Vec3V_Out			GetTargetPositionForEmergencyStop() const;
			CVehicle*			GetTargetVehicle() const;
			bool				HasMountedGuns() const;
			bool				IsCollidingWithLockedDoor() const;
			bool				IsCollisionWithLawEnforcementImminent(float fMaxDistance, float fMaxTime, bool& bWithPed, bool& bWithVehicle) const;
			bool				IsCurrentStateValid() const;
			bool				IsInFistFight() const;
			bool				IsPedArmedWithGun(const CPed& rPed) const;
			bool				IsPedTryingToExitVehicle(const CPed& rPed) const;
			bool				IsPedsBestWeaponAGun(const CPed& rPed) const;
			bool				IsStuck() const;
			bool				IsTargetInFrontOfUs() const;
			bool				IsTargetInVehicle() const;
			bool				IsTargetInBoat()	const;
			bool				IsTargetMovingSlowlyInVehicle() const;
			bool				IsTargetOnMount() const;
			bool				MustPedBeInVehicle() const;
			void				ProcessAudio();
			void				ProcessBlockedByLockedDoor();
			void				ProcessTimers();
			bool				ShouldAnyoneProvideCoverDuringExitVehicle() const;
			bool				ShouldAttack() const;
			bool				ShouldChaseInCar() const;
			bool				ShouldCloseDoor() const;
			bool				ShouldEquipBestWeaponAfterExitingVehicle() const;
			bool				ShouldExitBoat() const;
			bool				ShouldExitVehicleToAim() const;
			bool				ShouldJumpOutOfVehicle() const;
			bool				ShouldMakeEmergencyStopDueToAvoidanceArea() const;
			bool				ShouldPedProvideCoverDuringExitVehicle(const CPed& rPed, fwMvClipSetId& clipSetId) const;
			bool				ShouldWaitInCar() const;
			void				UpdateVehicleTaskForApproachTarget();
			bool				WillHaveGunAfterExitingVehicle() const;
			bool				HasClearVehicleExitPoint() const;
			bool				ShouldWaitWhileCruising() const;
			bool				HasTargetPassedUs() const;
			bool				WillTargetVehicleOvertakeUsWithinTime(float fTime) const;
			bool				FacingSameDirectionAsTarget() const;
			void				CreateSlowDownGoToTask(CVehicle* vehicle);

			// To request the exit to aim clip sets
	fwClipSetRequestHelper	m_clipSetRequestHelper;

	// Current target and the original target
	Vec3V			m_vTargetPositionWhenWaitBegan;
	Vec3V			m_vPositionWhenCollidedWithLockedDoor;
	CAITarget		m_target;
	u32				m_uTimeCollidedWithLockedDoor;
	fwFlags8		m_uConfigFlags;
	fwFlags8		m_uRunningFlags;
	fwMvClipSetId	m_ExitToAimClipSetId;
	float			m_fTimeVehicleUndriveable;
	float			m_fJumpOutTimer;
	CTaskGameTimer  m_NextExitCheckTimer; 
	float			m_fTimeTargetMovingSlowlyInVehicle;
	
#if !__FINAL
	static const u32 ms_uMaxObstructionProbePoints = 6;
	
	Vec3V	m_avObstructionProbeStartPoints[ms_uMaxObstructionProbePoints];
	Vec3V	m_avObstructionProbeEndPoints[ms_uMaxObstructionProbePoints];
	float	m_avObstructionProbeRadii[ms_uMaxObstructionProbePoints];
	u32		m_uNumObstructionPoints;
#endif
	
	static Tunables	sm_Tunables;
};

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
class CTaskVehicleCombat : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float	m_MinTimeBetweenShootOutTiresGlobal;
		float	m_MaxTimeBetweenShootOutTiresGlobal;
		float	m_MinTimeInCombatToShootOutTires;
		float	m_MaxTimeInCombatToShootOutTires;
		float	m_MinTimeToPrepareWeapon;
		float	m_MaxTimeToPrepareWeapon;
		float	m_ChancesToApplyReactionWhenShootingOutTire;
		u32		m_MaxTimeSinceTargetLastHostileForLawDriveby;

		PAR_PARSABLE;
	};

	// State
	typedef enum
	{
		State_invalid = -1,
		State_start = 0,
		State_waitForClearShot,
		State_mountedWeapon,
		State_vehicleGun,
		State_projectile,
		State_shootOutTire,
		State_exit
	} VehicleCombatState;

	enum
	{
		Flag_justAim			= BIT0,
		Flag_targetChanged		= BIT1,
		Flag_useCamera			= BIT2,
		Flag_forceAllowDriveBy	= BIT3,
	};

	// Constructor/destructor
	CTaskVehicleCombat(const CAITarget* pTarget, u32 uFlags = 0, float fFireTolerance = 20.0f);
	~CTaskVehicleCombat();

	virtual aiTask* Copy() const { return rage_new CTaskVehicleCombat(&m_target, m_flags, m_fFireTolerance); }
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_VEHICLE_COMBAT;}

	// FSM implementations
	FSM_Return		ProcessPreFSM();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort()	const {return State_exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// Update the target externally, will set a flag to restart the current task if
	// the target has changed.
	const CEntity* GetTarget() const {return m_target.GetEntity(); }
	void SetTarget(CAITarget& val);

	// This task supports termination requests
	virtual bool SupportsTerminationRequest() { return GetState() == State_vehicleGun; }

	static void ResetNextShootOutTireTimeGlobal() { ms_uNextShootOutTireTimeGlobal = 0; }
	static void InitTunables(); 

private:
	// State implementations
	// State_start
	FSM_Return	Start_OnUpdate				(CPed* pPed);
	// State_waitingForClearShot
	void		WaitForClearShot_OnEnter	(CPed* pPed);
	FSM_Return	WaitForClearShot_OnUpdate	(CPed* pPed);
	// State_mountedWeapon
	void		MountedWeapon_OnEnter		(CPed* pPed);
	FSM_Return	MountedWeapon_OnUpdate		(CPed* pPed);
	// State_vehicleGun
	void		VehicleGun_OnEnter			(CPed* pPed);
	FSM_Return	VehicleGun_OnUpdate			(CPed* pPed);
	// State_throwProjectile
	void		Projectile_OnEnter			(CPed* pPed);
	FSM_Return	Projectile_OnUpdate			(CPed* pPed);
	// State_shootOutTire
	void		ShootOutTire_OnEnter		(CPed* pPed);
	FSM_Return	ShootOutTire_OnUpdate		(CPed* pPed);

	// Helper functions
	void				InitialiseTargetingSystems	(CPed* pPed);
	bool				CheckTargetingLOS			(CPed* pPed);
	bool				CanFireWithMountedWeapon	(CPed* pPed);
	VehicleCombatState	GetDesiredState				(CPed* pPed);
	
	bool				CanShootOutTire() const;
	const CPed*			GetTargetPed() const;
	const CVehicle*		GetTargetVehicle() const;
	bool				ShouldApplyReactionWhenShootingOutTire() const;
	
private:

	static u32	GenerateNextShootOutTireTime();
	static u32	GenerateNextShootOutTireTimeGlobal();
	
private:

	CAITarget	m_target;
	fwFlags32	m_flags;
	u32			m_uWeaponPrepareTime;
	u32			m_uNextShootOutTireTime;
	u32			m_uRequestWaitForClearShotTime;
	float		m_fFireTolerance;
	
private:

	static u32			ms_uNextShootOutTireTimeGlobal;
	static eWantedLevel	ms_TargetDriveByWantedLevel; 
	static Tunables		ms_Tunables;

};


#endif // TASK_VEHICLE_COMBAT_H
