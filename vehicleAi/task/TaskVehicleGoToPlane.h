#ifndef TASK_VEHICLE_GO_TO_PLANE_H
#define TASK_VEHICLE_GO_TO_PLANE_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"
#include "VehicleAi\FlyingVehicleAvoidance.h"

class CVehControls;
class CPlane;

//
//
//
class CTaskVehicleGoToPlane : public CTaskVehicleGoTo
{
public:
	
	enum VehicleControlState
	{
		State_Invalid = -1,
		State_GoTo,
		//State_Land,//not implemented yet.
	} ;


	enum VirtualSpeedControlMode
	{
		VirtualSpeedControlMode_Off,
		VirtualSpeedControlMode_On,
		VirtualSpeedControlMode_SlowDescent,
	};

	struct Tunables : public CTuning
	{
		Tunables();

		int m_numFutureSamples;
		float m_futureSampleTime;

		float m_maxDesiredAngleYawDegrees;
		float m_maxDesiredAnglePitchDegrees;
		float m_maxDesiredAngleRollDegrees;
		float m_angleToTargetDegreesToNotUseMinRadius;

		float m_minMinDistanceForRollComputation;
		float m_maxMinDistanceForRollComputation;

		// Control
		float m_maxYaw;
		float m_maxPitch;
		float m_maxRoll;
		float m_maxThrottle;

		// PID controller parameters
		float m_yawKp;
		float m_yawKi;
		float m_yawKd;
		float m_pitchKp;
		float m_pitchKi;
		float m_pitchKd;
		float m_rollKp;
		float m_rollKi;
		float m_rollKd;
		float m_throttleKp;
		float m_throttleKi;
		float m_throttleKd;

		PAR_PARSABLE;
	}; 

	static const unsigned DEFAULT_FLIGHT_HEIGHT = 30;
	static const unsigned DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN = 20;

	// Constructor/destructor
	CTaskVehicleGoToPlane( const sVehicleMissionParams& params,
							const int iFlightHeight = DEFAULT_FLIGHT_HEIGHT, 
							const int iMinHeightAboveTerrain = DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN,
							const bool bPrecise = false,
							const bool bUseDesiredOrientation = false,
							const float fDesiredOrientation = 0.0f);

	virtual ~CTaskVehicleGoToPlane();

	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_PLANE; }
	virtual aiTask*		Copy() const;
	virtual void        CleanUp();

	// FSM implementations
	FSM_Return ProcessPreFSM();
	virtual FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);

	s32					GetDefaultStateAfterAbort	()	const {return State_GoTo;}

	void				SetOrientation				(float fRequestedOrientation);
	void				SetOrientationRequested		(bool bOrientationRequested);
	void				SetIsPrecise				(bool bPrecise);
	void				SetFlightHeight				(int flightHeight);
	void				SetMinHeightAboveTerrain	(int minHeightAboveTerrain);
	int					GetMinHeightAboveTerrain() const { return m_iMinHeightAboveTerrain; }
	void				SetIgnoreZDeltaForEndCondition( bool in_Value);
	void				SetVirtualSpeedControlMode( VirtualSpeedControlMode in_Mode);
	void				SetVirtualSpeedMaxDescentRate(float in_rate);
	void				SetForceDriveOnGround(bool in_Value);
	void				SetMaxThrottle(float in_fValue);
	void				SetMinDistanceToTargetForRollComputation(float in_Value);

	void				SetTurnOnEngineAtStart(bool bTurnOnEngine) { m_bTurnOnEngineAtStart = bTurnOnEngine; }

	float				GetDesiredPitch() const { return m_fDesiredPitch; }
	void				OverrideMaxRoll(float fValue) { m_fMaxOverriddenRoll = fValue; }
	float				GetMaxRoll() const { return m_fMaxOverriddenRoll > 1.0f ? m_fMaxOverriddenRoll : sm_Tunables.m_maxDesiredAngleRollDegrees; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );

	virtual void		Debug() const;
#endif //!__FINAL
	void				Plane_ModifyTargetForOrientation(Vector3 &io_targetPos, const CPlane *pPlane ) const;
	void				Plane_ModifyTargetForArrivalTolerance(Vector3 &io_targetPos, const CPlane *pPlane ) const;
	void				Plane_SteerToTarget(CPlane *pPlane, const Vector3 &targetPos);
	void				Plane_SteerToSpeed(CPlane *pPlane, float fDesiredSpeed, bool bEnableVirtualSpeed);
	void				Plane_SteerXYUsingRoll(CPlane *pPlane, const Vector3& vTargetPos);
	void				Plane_SteerHeightZ(CPlane *pPlane, const Vector3& vTargetPos, bool bEnableHeightmapAvoidance);
	void				Plane_SteerXYUsingYawAndWheels(CPlane *pPlane, const Vector3& vTargetPos);


	void				Plane_SteerToTargetCoors	(CAutomobile *pVeh, const Vector3 &targetPos);
	void				Plane_SteerToTargetPrecise_FixedOrientation(CPlane *pPlane, float orientationToAchieveAndKeep, const Vector3& target, bool bLanding, bool bForceHeight = false, bool bLimitDescendSpeed = false);
	void				Plane_SteerToTargetPrecise(CPlane *pPlane);

	bool				Vehicle_WorldCollsionTest				(const Vector3 &vStart, const Vector3 &vEnd, Vector3& intersectionPoint);
	float				Vehicle_TestForFutureWorldCollisions	(const CPhysical* pEntity, const Vector3& forwardDirFlat, const float targetDeltaAngle);

	static float ComputeRollForTurnRadius(CPlane *pPlane, float in_Speed, float in_Radius);
	static float ComputeTurnRadiusForRoll(const CPlane *pPlane, float in_Speed, float in_Roll, bool bIgnoreHealth = false);
	static float ComputeTurnRadiusForTarget(const CPlane* pPlane, const Vector3& vTargetPos, float fMinDistanceToTarget);
	static float ComputeSpeedForRadiusAndRoll(CPlane& in_Plane, float in_TurnRadius, float in_RollDegrees);
	static float ComputeSlopeToAvoidTerrain(CPlane *pPlane, float in_Speed, float turnRadius, float in_HeightAboveTerrain, bool bMaxHeight);
	static bool ComputeApproachTangentPosition(Vector3& o_TangentPos, const CPlane* pPlane, const Vector3& vTargetPos
		, const Vector3& in_vDirection, float in_Speed, float in_MaxRoll, bool bIgnoreAlignment, bool DEBUG_DRAW_ONLY(in_bDebugDraw) );
	static float ComputeMaxRollForPitch( float in_Pitch, float maxRoll );
	static float GetMinAirSpeed(const CPlane *pPlane);

	static Tunables	sm_Tunables;

	void SetMinTurnRadiusPadding(float in_fMinTurnRadiusPadding) { m_fMinTurnRadiusPadding = in_fMinTurnRadiusPadding; }

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoToPlane>; }

	void SetAvoidTerrainMaxZThisFrame(bool bValue) { m_bAvoidTerrainMaxZThisFrame = bValue; }
	bool GetAvoidTerrainMaxZThisFrame() const { return m_bAvoidTerrainMaxZThisFrame; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_FLIGHT_HEIGHT = 12;
		static const unsigned SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN = 12;
		static const unsigned SIZEOF_DESIRED_ORIENTATION = 10;

		CTaskVehicleGoTo::Serialise(serialiser);

		bool hasDefaultParams = (m_iFlightHeight == DEFAULT_FLIGHT_HEIGHT && 
								m_iMinHeightAboveTerrain == DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN &&
								!m_bPrecise &&
								!m_bUseDesiredOrientation);

		SERIALISE_BOOL(serialiser, hasDefaultParams);

		if (!hasDefaultParams || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_iFlightHeight, SIZEOF_FLIGHT_HEIGHT, "Flight height");
			SERIALISE_UNSIGNED(serialiser, m_iMinHeightAboveTerrain, SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN, "Min height above terrain");
			SERIALISE_BOOL(serialiser, m_bPrecise, "Precise");
			SERIALISE_BOOL(serialiser, m_bUseDesiredOrientation);

			if (m_bUseDesiredOrientation || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_PACKED_FLOAT(serialiser, m_fDesiredOrientation, TWO_PI, SIZEOF_DESIRED_ORIENTATION, "Desired orientation");
			}
		}
		else
		{
			m_iFlightHeight				= DEFAULT_FLIGHT_HEIGHT;
			m_iMinHeightAboveTerrain	= DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN;
			m_bPrecise					= false;
			m_bUseDesiredOrientation	= false;
			m_fDesiredOrientation		= 0.0f;
		}
	}

protected:

	// FSM function implementations
	// State_GoTo
	void		Goto_OnEnter						(CVehicle* pVehicle);
	FSM_Return	Goto_OnUpdate						(CVehicle* pVehicle);

	CFlyingVehicleAvoidanceManager m_flyingAvoidanceHelper;
	VirtualSpeedControlMode m_VirtualSpeedControlMode;
	float	m_fVirtualSpeedDescentRate;
	float   m_fCurrentTurnRadius;
	float	m_fDesiredTurnRadius;
	float   m_fDesiredPitch;
	float	m_fDesiredOrientation;
	float   m_fMinTurnRadiusPadding;
	float   m_fMaxThrottle;
	float	m_fMaxOverriddenRoll;
	float	m_fMinDistanceToTargetForRollComputation;
	float	m_fFlyingVehicleAvoidanceScalar;
	int		m_iFlightHeight; 
	int		m_iMinHeightAboveTerrain;
	bool	m_bIgnoreZDeltaForEndCondition;
	bool	m_bPrecise;
	bool	m_bUseDesiredOrientation;
	bool	m_bForceHeightUpdate;//used to force the plane to do a line scan to update its desired height.
	bool	m_bTurnOnEngineAtStart;
	bool	m_bForceDriveOnGround; 
	bool	m_bAvoidTerrainMaxZThisFrame;


};

#endif
