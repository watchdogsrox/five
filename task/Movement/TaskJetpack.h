#ifndef __TASK_JETPACK_H__
#define __TASK_JETPACK_H__

#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"
#include "physics/PhysicsHelpers.h"
#include "peds/ped.h"
#include "Task/Movement/JetpackObject.h"

#if ENABLE_JETPACK

class CControl;

// Jetpack flags.
enum JetpackFlags
{
	// Config Flags
	JPF_HasTarget = BIT0,
	JPF_HasShootAtTarget = BIT1,

	JPF_MAX_Flags
};

enum eJetpackBoneId
{
	Nozzle_Up_R	= 27419,
	Nozzle_Up_L	= 27445,
	Nozzle_Lo_R	= 53764,
	Nozzle_Lo_L	= 53790,
};

typedef fwFlags8 fwJetpackFlags;

//////////////////////////////////////////////////////////////////////////
// Class:	CTaskJetpack
// Purpose:	Task to handle a jetpack using an animated/physical solution.
class CTaskJetpack : public CTaskFSMClone
{
public:

	struct Tunables : CTuning
	{
		struct HoverControlParams
		{
			HoverControlParams()
			{}

			bool m_bGravityDamping;
			bool m_bDoRelativeToCam;
			bool m_bAntiGravityThrusters;
			bool m_bAutoLand;
			bool m_bSyncHeadingToCamera;
			bool m_bSyncHeadingToDesired;

			float m_fGravityDamping; 
			float m_fGravityDecelerationModifier;
			float m_fHorThrustModifier; 
			float m_fForThrustModifier; 
			float m_fVerThrustModifier; 
			float m_fCameraHeadingModifier;
			float m_fDesiredHeadingModifier;
			float m_fDesiredHeadingModifierBoost;
			float m_fMinPitchForBoost;
			float m_fMinPitchForHeadingChange;
			float m_fMinYawForHeadingChange;

			float m_fThrustLerpRate; 
			float m_fPitchLerpRate; 
			float m_fRollLerpRate; 
			float m_fThrustLerpOutRate; 
			float m_fPitchLerpOutRate; 
			float m_fRollLerpOutRate; 
			float m_fAngVelocityLerpRate;

			float m_fBoostApproachRateIn;
			float m_fBoostApproachRateOut;

			float m_MaxIdleAnimVelocity;

			Vector3 m_vMinStrafeSpeedMinThrust;
			Vector3 m_vMaxStrafeSpeedMinThrust;
			Vector3 m_vMinStrafeSpeedMaxThrust;
			Vector3 m_vMaxStrafeSpeedMaxThrust;

			Vector3 m_vMinStrafeSpeedMinThrustBoost;
			Vector3 m_vMaxStrafeSpeedMinThrustBoost;
			Vector3 m_vMinStrafeSpeedMaxThrustBoost;
			Vector3 m_vMaxStrafeSpeedMaxThrustBoost;

			Vector3 m_vMaxStrafeAccel;
			Vector3 m_vMinStrafeAccel;

			float m_fLandHeightThreshold;
			float m_fTimeNearGroundToLand;

			float m_fMaxAngularVelocityZ;

			float m_fMaxHolsterTimer;

			bool m_bUseKillSwitch;

			PAR_SIMPLE_PARSABLE;
		};

		struct FlightControlParams
		{
			FlightControlParams()
			{}

			bool m_bThrustersAlwaysOn;
			bool m_bEnabled;

			float m_fThrustLerpRate; 
			float m_fPitchLerpRate; 
			float m_fRollLerpRate; 
			float m_fYawLerpRate; 
			float m_fAngVelocityLerpRate;

			float m_fThrustModifier;

			float m_fTimeToFlattenVelocity;

			float m_fMaxSpeedForMoVE;

			float m_fMaxAngularVelocityX;
			float m_fMaxAngularVelocityY;
			float m_fMaxAngularVelocityZ;

			PAR_SIMPLE_PARSABLE;
		};

		struct AvoidanceSettings
		{
			AvoidanceSettings()
			{}

			float m_fAvoidanceRadius;
			float m_fVerticalBuffer;
			float m_fArrivalTolerance;
			float m_fAvoidanceMultiplier;
			float m_fMinTurnAngle;

			PAR_SIMPLE_PARSABLE;
		};

		struct AimSettings
		{	
			AimSettings()
			{}

			bool m_bEnabled;

			PAR_SIMPLE_PARSABLE;
		};

		struct CameraSettings
		{
			CameraSettings()
			{}

			atHashString	m_HoverCamera;
			atHashString	m_FlightCamera;

			PAR_SIMPLE_PARSABLE;
		};

		struct JetpackModelData
		{
			JetpackModelData()
			{}

			atHashString				m_JetpackModelName;
			Vector3						m_JetpackAttachOffset;

			PAR_SIMPLE_PARSABLE;
		};

		struct JetpackClipsets
		{
			JetpackClipsets()
			{}

			atHashString				m_HoverClipSetHash;
			atHashString				m_FlightClipSetHash;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		HoverControlParams			m_HoverControlParams;
		FlightControlParams			m_FlightControlParams;
		AimSettings					m_AimSettings;
		CameraSettings				m_CameraSettings;
		JetpackModelData			m_JetpackModelData;
		JetpackClipsets				m_JetpackClipsets;
		AvoidanceSettings			m_AvoidanceSettings;

		PAR_PARSABLE;
	};

	enum State
	{
		State_Start,			// Init state.
		State_StreamAssets,		// Stream anims/model etc.
		State_Hover,			// Strafe mode (vertical).
		State_Flight,			// Flight mode (horizontal, like a plane).
		State_Fall,				// Fall (if we press kill switch).
		State_Land,				// Land
		State_Quit				// Shutdown task
	};

	CTaskJetpack(fwJetpackFlags flags = 0);
	virtual ~CTaskJetpack();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskJetpack(m_iFlags); }

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_JETPACK; }	

	bool ShouldStickToFloor() const;

	bool CalcDesiredVelocity(const Matrix34& mUpdatedPed, float fTimeStep, Vector3& vDesiredVelocity) const;
	void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) const;

	// Is moving for purposes of ambient idle selection?
	bool IsInMotion() const;

	void SetTargetGoTo(const CPhysical* pEntity, Vec3V_In vPosition);
	void SetTargetEntityGoTo(const CPhysical* pEntity);
	void SetTargetPositionGoTo(Vec3V_In vPosition);
	void SetTargetShootAt(const CPhysical* pEntity, Vec3V_In vPosition);
	void SetTargetEntityShootAt(const CPhysical* pEntity);
	void SetTargetPositionShootAt(Vec3V_In vPosition);

	static bool DoesPedHaveJetpack(const CPed *pPed);

	void SetGoToParametersForAI(CPhysical *pTargetEntity, Vector3 vTargetPosition, float fMinHeightAboveSurface, bool bDisableThrustAtDestination, bool bUseRandomTimerWhenClose = false, float fProbeHeight = 50.0f);
	void SetShootAtParametersForAI(CPhysical *pTargetEntity, Vector3 vTargetPos, float fAbortRange, int iFrequencyPercentage, int iFiringPatternHash);
	void RequestDrivebyTermination(bool bDisable) { m_bTerminateDrivebyTask = bDisable; }

protected:

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual void ProcessPreRender2();

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

public:

	// Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool				OverridesNetworkBlender(CPed*);
	virtual bool                OverridesNetworkAttachment(CPed* pPed);
	virtual bool 				IsInScope(const CPed* pPed);

	bool						IsStateChangeHandledByNetwork(s32 iState, s32 iStateFromNetwork) const;

protected:

	void Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);

	void StreamAssets_OnEnter(CPed* pPed);
	FSM_Return StreamAssets_OnUpdate(CPed* pPed);

	void Hover_OnEnter(CPed* pPed);
	FSM_Return Hover_OnUpdate(CPed* pPed);
	void Hover_OnExit(CPed* pPed);

	void Flight_OnEnter(CPed* pPed);
	FSM_Return Flight_OnUpdate(CPed* pPed);

	void Land_OnEnter(CPed* pPed);
	FSM_Return Land_OnUpdate(CPed* pPed);

	void Fall_OnEnter(CPed* pPed);
	FSM_Return Fall_OnUpdate(CPed* pPed);

	FSM_Return Quit_OnUpdate(CPed* pPed);

private:

	void UpdateMoVE();

	void TakeOff(CPed* pPed);

	bool CanActivateKillSwitch(CPed *pPed) const;
	bool CanActivateFlightMode(CPed *pPed) const;

	void ProcessHoverControls(const CPed *pPed);
	void ProcessHoverControlsForPlayer(const CPed *pPed);
	void ProcessHoverControlsForAI(const CPed *pPed);
	void ProcessFlightControls(const CPed *pPed);

	typedef float (*fp_HeightQuery)(float x, float y);
	float ComputeTargetZToAvoidTerrain(const CPed* pPed, const Vector3& vJetpackPos, const Vector3& vTargetPosition);
	float ComputeTargetZToAvoidFlyingPeds(const CPed* pPed, const Vector3& vJetpackPos, const Vector3& vTargetPosition, float targetZ);

	void ProcessHoverPhysics(CPed *pPed, float fTimeStep);
	void ProcessFlightPhysics(CPed *pPed);

	void ProcessAimControls(bool bAiming);

	CTask* CreateTaskForAiming() const;
	CTask* CreateTaskForAmbientClips() const;

	bool ShouldBlockWeaponSwitching() const;
	bool CanAim() const;
	bool ScanForCrashCollision() const;
	bool CheckForLanding(CPed *pPed);
	bool ShouldUseAiControls() const;
	bool ShouldUsePlayerControls() const;

	Vec3V_Out CalculateTargetGoToPosition();
	Vec3V_Out CalculateTargetShootAtPosition() const;
	Vec3V_Out CalculateTargetPositionFuture();

	void CalculateThrusterValuesForJetpackAI(float fDistanceZ);
	void CalculateStickForPitchForJetpackAI(Vector3 vPedToTarget, bool bDisablePitch);
	void CalculateStickForRollForJetpackAI();
	void CalculateDesiredHeadingForAvoidance(const CPed *pPed, Vector3 vPedPosition, Vector3 vPedToTarget);

	void PopulateCloseEntityList(const CPed *pPed);

	float GetFlightSpeedForMoVE() const;
	void GetAnglesForMoVE(Vector3 &vAngles) const;
	Vector3 GetAngularVelocityForMoVE(const Vector3 &vAngVel) const;

	float GetUpThrust() const;
	float GetUpThrustInput() const;

	const CJetpack *GetJetpack() const;

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	virtual CMoveNetworkHelper*	GetMoveNetworkHelper()
	{
		return &m_networkHelper;
	}

	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

#if DEBUG_DRAW
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;
#endif

#if __BANK
public:
	static void InitWidgets();

private:

	static void GiveJetpackTestCB();
	static void RemoveJetpackTestCB();
	static void ToggleEquipJetpack();

	static void AIShootAtTest();
	static void AIGoToTest();
	static void SetDebugGoToFromCamera();

	static void SetGoToTestPoint1();
	static void SetGoToTestPoint2();
	static void SetGoToTestPoint3();
	static void DisableDrivebyTest();

	static bool ms_bDebugControls;
	static bool ms_bDebugAngles;
	static bool ms_bDebugAI;
	static Vector3 ms_vTargetPositionGoTo;
	static Vector3 ms_vTargetPositionShoot;
	static bool ms_bGoToCoords;
	static bool ms_bShootAtCoords;
	static float ms_fAbortRange;
	static float ms_fProbeHeight;

	static Vector3 ms_vTargetLocationTestStart;
	static Vector3 ms_vTargetLocationTestEnd;
	static Vector3 ms_vHeightmapSamplePosition;

	static Vector3 ms_vJetpackTestPoint1;
	static Vector3 ms_vJetpackTestPoint2;
	static Vector3 ms_vJetpackTestPoint3;

	static float ms_fMinHeightAboveSurface;
	static bool ms_bDisableThrustAtDestination;
	static bool ms_bUseRandomTimerWhenClose;
#endif

public:

	static Tunables	sm_Tunables;

private:

	// Network Helper
	CMoveNetworkHelper	m_networkHelper;

	fwJetpackFlags m_iFlags;

	Vector3 m_vAngularVelocity;

	float m_fThrusterLeft;
	float m_fThrusterRight;
	float m_fThrusterLeftInput;
	float m_fThrusterRightInput;
	float m_fRoll;
	float m_fPitch;
	float m_fYaw;
	float m_fHoverModeBoost;
	float m_fNearGroundTime;
	float m_fHolsterTimer;

	//AI Parameters
	Vector3 m_vAvoidTargetPosition;
	Vec3V m_vTargetPositionGoTo;
	Vec3V m_vTargetPositionShootAt;
	RegdConstPhy m_pTargetEntityGoTo;
	RegdConstPhy m_pTargetEntityShootAt;
	bool m_bDisableThrustAtDestination;
	bool m_bIsAiming;
	bool m_bArrivedAtDestination;
	bool m_bStopRotationToTarget;
	bool m_bIsAvoidingEntity;
	bool m_rotateForAvoid;
	float m_fAbortRange; 
	int m_iFrequencyPercentage;
	int m_iFiringPatternHash;
	bool m_bRestartShootingTask;
	bool m_bTerminateDrivebyTask;
	bool m_bUseRandomTimerWhenClose;
	bool m_bFlyingPedAvoidance;
	float m_fMovementDelayTime;
	float m_fMovementTimer;
	Vec3V m_vCachedTargetPositionGoToDelay;
	// Variables for collision avoidance
	WorldProbe::CShapeTestSingleResult m_startLocationTestResults;
	WorldProbe::CShapeTestSingleResult* m_targetLocationTestResults;
	static const int NUM_VERTICAL_CAPSULE_TESTS = 5;
	float m_fPreviousTargetHeight;
	float m_fMinHeightAboveSurface;
	float m_fProbeHeight;
	float m_fHitObstacleDistance;

	bool m_bCrashLanded;

	static CFlightModelHelper ms_FlightModeHelper;

	static const fwMvRequestId ms_Hover;
	static const fwMvRequestId ms_Flight; 

	static const fwMvBooleanId ms_HoverOnEnter;
	static const fwMvBooleanId ms_FlightOnEnter;

	static const fwMvStateId ms_HoverState;
	static const fwMvStateId ms_FlightState;

	static const fwMvFloatId ms_PitchId;
	static const fwMvFloatId ms_RollId;
	static const fwMvFloatId ms_YawId;
	static const fwMvFloatId ms_PitchAngleId;
	static const fwMvFloatId ms_RollAngleId;
	static const fwMvFloatId ms_YawAngleId;
	static const fwMvFloatId ms_AngVelocityXId;
	static const fwMvFloatId ms_AngVelocityYId;
	static const fwMvFloatId ms_AngVelocityZId;
	static const fwMvFloatId ms_ThrusterRightId;
	static const fwMvFloatId ms_ThrusterLeftId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_HoverModeBoostId;
	static const fwMvFloatId ms_CombinedThrustId;
};

//-------------------------------------------------------------------------
// Task info for jetpack
//-------------------------------------------------------------------------
class CClonedJetpackInfo : public CSerialisedFSMTaskInfo
{
public:
	struct InitParams
	{
		InitParams(s32 const state, u8	const iFlags)
		: m_state(state)
		, m_iFlags(iFlags)
		{}

		s32 const		m_state;
		u8 const		m_iFlags;
	};

	CClonedJetpackInfo( InitParams const & _initParams );
	CClonedJetpackInfo();

	~CClonedJetpackInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_JETPACK;}
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool			HasState() const					{ return true; }
	virtual u32				GetSizeOfState() const				{ return datBitsNeeded<CTaskJetpack::State_Quit>::COUNT; }

	inline u8				GetFlags(void)						{ return m_iFlags; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		
		u8 flags = m_iFlags;
		SERIALISE_UNSIGNED(serialiser, 			flags,								datBitsNeeded<JPF_MAX_Flags>::COUNT,	"Flags");
		m_iFlags = flags;
	}

private:

	u8 m_iFlags;
};

#endif //ENABLE_JETPACK

class CFlyingPedCollector
{
public:
	static void AddPed(const RegdPed& ped);
	static void RemovePed(const RegdPed& ped);
	static atFixedArray<RegdPed, 20> m_flyingPeds;
};

#endif
