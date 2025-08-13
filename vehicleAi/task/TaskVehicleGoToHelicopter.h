#ifndef TASK_VEHICLE_GO_TO_HELICOPTER_H
#define TASK_VEHICLE_GO_TO_HELICOPTER_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"
#include "physics\WorldProbe\shapetestresults.h"
#include "VehicleAi\FlyingVehicleAvoidance.h"

class CVehControls;
class CHeli;
//
//
//
class CTaskVehicleGoToHelicopter : public CTaskVehicleGoTo
{
public:

	// mission flags
	enum HeliFlags
	{
		HF_AttainRequestedOrientation = BIT(0),
        HF_DontModifyOrientation = BIT(1),
        HF_DontModifyPitch = BIT(2),
        HF_DontModifyThrottle = BIT(3),
        HF_DontModifyRoll = BIT(4),
		HF_LandOnArrival = BIT(5),
		HF_DontDoAvoidance = BIT(6),
		HF_StartEngineImmediately = BIT(7),
		HF_ForceHeightMapAvoidance = BIT(8),
		HF_DontClampProbesToDestination = BIT(9),
		HF_EnableTimeslicingWhenPossible = BIT(10),
		HF_CircleOppositeDirection = BIT(11),
		HF_MaintainHeightAboveTerrain = BIT(12),
		HF_IgnoreHiddenEntitiesDuringLand = BIT(13),
		HF_DisableAllHeightMapAvoidance = BIT(14),
		HF_NumFlags	= 15
	};
public:

	typedef enum
	{
		State_Invalid = -1,
		State_GoTo = 0,
		State_Land,
	} VehicleControlState;

	struct Tunables : public CTuning
	{
		Tunables();

		// Speed
		float m_slowDistance;
		float m_maxCruiseSpeed;
		float m_maxPitchRoll;
		float m_maxThrottle;

		// PID controller parameters
		float m_leanKp;
		float m_leanKi;
		float m_leanKd;
		float m_yawKp;
		float m_yawKi;
		float m_yawKd;
		float m_throttleKp;
		float m_throttleKi;
		float m_throttleKd;

		// Avoidance
		float m_whiskerForwardTestDistance;
		float m_whiskerForwardSpeedScale;
		float m_whiskerLateralTestDistance;
		float m_whiskerVerticalTestDistance;
		float m_whiskerTestAngle;
		float m_avoidHeadingChangeSpeed;
		float m_avoidHeadingJump;
		float m_avoidPitchChangeSpeed;
		float m_avoidPitchJump;
		float m_avoidLockDuration;
		float m_downAvoidLockDurationMaintain;
		float m_avoidMinFarExtension;
		float m_avoidForwardExtraOffset;
		float m_maintainHeightMaxZDelta;
		float m_downHitZDeltaBuffer;

		float m_DistanceXYToUseHeightMapAvoidance;
		float m_futureHeightmapSampleTime;
		int m_numHeightmapFutureSamples;

		 // Timeslicing
		float m_TimesliceMinDistToTarget;	// Minimum distance to target to use timeslicing.
		u32 m_TimesliceTimeAfterAvoidanceMs;	// Time after avoidance to disable timeslicing, in ms.
		PAR_PARSABLE;
	}; 

	enum
	{
		DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN=20
	};

	// Constructor/destructor
	CTaskVehicleGoToHelicopter(const sVehicleMissionParams& params,
								const s32 iHeliFlags = 0,
								const float fHeliRequestedOrientation = -1.0f, 
								const int iMinHeightAboveTerrain = DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN );

	virtual ~CTaskVehicleGoToHelicopter();
	
public:

	float GetSlowDownDistance() const { return m_fSlowDownDistance; }
	
public: 

	void SetSlowDownDistance(float fSlowDownDistance) { m_fSlowDownDistance = fSlowDownDistance; }
	void SetSlowDownDistanceMin(float fSlowDownDistanceMin) { m_fSlowDownDistanceMin = fSlowDownDistanceMin; }

public:

	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER; }
	virtual aiTask*		Copy() const;

	// FSM implementations
	virtual FSM_Return	UpdateFSM									(const s32 iState, const FSM_Event iEvent);

	// PURPOSE:	Meant to be called each frame when doing timesliced updates, instead of the regular update.
	void				ProcessTimeslicedUpdate();

	s32				GetDefaultStateAfterAbort					()	const {return State_GoTo;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
	virtual void		Debug() const;

#endif //!__FINAL

	void				SetOrientation								(float heliRequestedOrientation) { m_fHeliRequestedOrientation = fwAngle::LimitRadianAngle0to2PiSafe(heliRequestedOrientation); }
	void				SetOrientationRequested						(bool bOrientationRequested);

    void                SetHeliFlag                                 (s32 iHeliFlag){m_iHeliFlags|= iHeliFlag;}
    void                UnSetHeliFlag                               (s32 iHeliFlag){m_iHeliFlags&= ~iHeliFlag;}
    s32                 GetHeliFlags                                (){ return m_iHeliFlags;}

	void				SetMinHeightAboveTerrain					(int minHeightAboveTerrain)  { m_iMinHeightAboveTerrain = minHeightAboveTerrain; }

	void				SetShouldLandOnArrival						(bool bShouldLand);
	void				SetFlyingVehicleAvoidanceScalar				(float in_fFlyingVehicleAvoidanceScalar);

	void				SteerToTarget								(CHeli* pHeli, const Vector3& vTargetPos, bool bAvoidObstacles = true);
	void				SteerToVelocity								(CHeli *pHeli, const Vector3& vDesiredVelocity, bool preventYaw);


	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoToHelicopter>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN = 12;
		static const unsigned SIZEOF_DESIRED_ORIENTATION = 10;
		static const unsigned SIZEOF_SLOW_DOWN_DISTANCE = 8;
	
		CTaskVehicleGoTo::Serialise(serialiser);

		bool hasDefaultParams = (m_iHeliFlags == 0 && 
								m_iMinHeightAboveTerrain == DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN &&
								m_fHeliRequestedOrientation == -1.0f);

		SERIALISE_BOOL(serialiser, hasDefaultParams);

		if (!hasDefaultParams || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_iHeliFlags, HF_NumFlags, "Heli flags");
			SERIALISE_INTEGER(serialiser, m_iMinHeightAboveTerrain, SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN, "Min height above terrain");

			bool hasDesiredOrientation = m_fHeliRequestedOrientation != -1.0f;

			SERIALISE_BOOL(serialiser, hasDesiredOrientation);

			if (hasDesiredOrientation || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fHeliRequestedOrientation, TWO_PI, SIZEOF_DESIRED_ORIENTATION, "Desired orientation");
			}
			else
			{
				m_fHeliRequestedOrientation = -1.0f;
			}

			bool hasSlowDownDistance = !IsClose(m_fSlowDownDistance, sm_Tunables.m_slowDistance, SMALL_FLOAT);
			SERIALISE_BOOL(serialiser, hasSlowDownDistance, "Has Slow Down Distance Set By Script");
			if(hasSlowDownDistance || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fSlowDownDistance, 200.0f, SIZEOF_SLOW_DOWN_DISTANCE, "Slow Down Distance");
			}
			else
			{
				m_fSlowDownDistance = sm_Tunables.m_slowDistance;
			}
		}
		else
		{
			m_iHeliFlags = 0;
			m_iMinHeightAboveTerrain = DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN;
			m_fHeliRequestedOrientation = -1.0f;
			m_fSlowDownDistance = sm_Tunables.m_slowDistance;
		}
	}

protected:

	// FSM function implementations
	// State_GoTo
	void		Goto_OnEnter								(CHeli* pHeli);
	FSM_Return	Goto_OnUpdate								(CHeli* pHeli);
	void		Goto_OnExit									(CHeli* pHeli);
	// State_Land
	void		Land_OnEnter								(CHeli* pHeli);
	FSM_Return	Land_OnUpdate								(CHeli* pHeli);
	
	typedef float (*fp_HeightQuery)(float x, float y);
	float ComputeTargetZToAvoidTerrain(CHeli& in_Heli, const Vector3& vTargetPosition);
	float ComputeTargetZToAvoidHeightMap(const CHeli& in_Heli, const Vector3& vTargetPosition, fp_HeightQuery in_fpHeightQuery) const;
	void UpdateAvoidance(CHeli* pHeli, const Vector3& vTargetDir, const float fTargetDist, float timeStep, float desiredSpeed);

	// PURPOSE:	Used for timeslicing purposes, returns true if we were avoiding recently.
	bool WasAvoidingRecently() const;

	enum AvoidanceDir
	{
		AD_Forward = 0,
		AD_Left,
		AD_Right,
		AD_Down,
		AD_Up,

		AD_COUNT,
	};

	// Variables for collision avoidance
	WorldProbe::CShapeTestSingleResult* m_avoidanceTestResults;
	WorldProbe::CShapeTestSingleResult m_targetLocationTestResults;
	WorldProbe::CShapeTestSingleResult m_startLocationTestResults;
	Vector3 m_vAvoidanceDir;
	Vector3 m_vBrakeVector;
	Vector3 m_vDownProbeEndPoint;

	// PURPOSE:	When timesliced, this is the current position we are steering towards.
	Vec3V	m_vTimesliceCurrentSteerToPos;

	float	m_fAvoidanceTimerRightLeft;
	float	m_fAvoidanceTimerUpDown;
	float	m_fPreviousTargetHeight;
	float	m_fPreviousCurrentHeight;
	float	m_fPreviousHeadingChange;
	float	m_fPreviousPitchChange;

	// Additional params
	float	m_fHeliRequestedOrientation;
	float	m_fSlowDownDistance;
	float	m_fSlowDownDistanceMin;
	float	m_fFlyingVehicleAvoidanceScalar;
	int		m_iMinHeightAboveTerrain;
	bool	m_bShouldLandOnArrival;
	bool	m_bRunningLandingTask;

	bool	m_bHasAvoidanceDir;

	// PURPOSE:	True if we should call SteerToTarget() on frames between timesliced updates.
	bool	m_bTimesliceShouldSteerToPos;

	s32		m_iHeliFlags;

	// PURPOSE:	Time stamp for when we were last avoiding, stored for timeslicing purposes.
	u32		m_uTimesliceLastAvoidTime;

	CFlyingVehicleAvoidanceManager m_flyingAvoidanceHelper;

#if DEBUG_DRAW
	float	m_fPrevDesiredSpeed;
	Vector3 m_vModifiedTarget;
	Vector3 m_vVelocityDelta;
	WorldProbe::CShapeTestCapsuleDesc* probeDesc;
	struct debugHitData
	{
		bool bHit;
		Vector3 vHitLocation;
	};

	debugHitData* probeHits;
#endif

	static Tunables	sm_Tunables;
};

#endif
