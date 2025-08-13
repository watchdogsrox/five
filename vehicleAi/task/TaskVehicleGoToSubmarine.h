#ifndef TASK_VEHICLE_GO_TO_SUBMARINE_H
#define TASK_VEHICLE_GO_TO_SUBMARINE_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleGoto.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\VehicleNavigationHelper.h"

class CVehControls;
class CSubmarine;

class CTaskVehicleGoToSubmarine : public CTaskVehicleGoTo
{
public:

	// mission flags
	enum SubFlags
	{
		SF_SinkToTerrain = BIT(0),
		SF_DontEntityAvoidance = BIT(1),
		SF_DontObjectAvoidance = BIT(2),
		SF_DontArrivalSlowdown = BIT(3),
		SF_DontUseNavSupport = BIT(4),
		SF_DontSwitchToSubMode = BIT(5),
		SF_AttainRequestedOrientation = BIT(6),
		SF_HoverAtEnd = BIT(7),
		SF_NumFlags	= 8
	};

	typedef enum
	{
		State_Invalid = -1,
		State_Start,
		State_GoTo,
		State_Rotate,
		State_Stop,
	} VehicleControlState;

	struct Tunables : public CTuning
	{
		Tunables();

		float m_forwardProbeRadius;
		float m_maxForwardSlowdownDist;
		float m_minForwardSlowdownDist;
		float m_maxReverseSpeed;
		float m_wingProbeTimeDistance;
		float m_downDiveScaler;
		float m_maxPitchAngle;
		float m_slowdownDist;
		float m_forwardProbeVelScaler;

		PAR_PARSABLE;
	}; 

	// Constructor/destructor
	CTaskVehicleGoToSubmarine(const sVehicleMissionParams& params, const u32 iSubFlags = 0, s32 minHeightAboveTerrain = 5, float RequestedOrientation = -1.0f);
	~CTaskVehicleGoToSubmarine();
	virtual void		CleanUp();

	float				GetSlowDownDistance			() const { return m_fSlowDownDistance; }
	void				SetSlowDownDistance			(float fSlowDownDistance) { m_fSlowDownDistance = fSlowDownDistance; }
	void				SetSlowDownDistanceMin		(float fSlowDownDistanceMin) { m_fSlowDownDistanceMin = fSlowDownDistanceMin; }

	int					GetTaskTypeInternal			() const { return CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE; }
	aiTask*				Copy						() const;
	// FSM implementations
	FSM_Return			UpdateFSM					(const s32 iState, const FSM_Event iEvent);

	s32					GetDefaultStateAfterAbort	()	const {return State_GoTo;}

	bool				ShouldHoverAtEnd() const	{ return (m_iSubFlags & SF_HoverAtEnd) ? true : false; };

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoToSubmarine>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CTaskVehicleMissionBase::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_iSubFlags, SF_NumFlags, "Sub flags");
	}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
	virtual void		Debug						() const;
#endif //!__FINAL

protected:

	// FSM function implementations
	// State_Start
	void				Start_OnEnter						(CVehicle* pVehicle);
	FSM_Return			Start_OnUpdate						(CVehicle* pVehicle);
	// State_GoTo
	void				Goto_OnEnter						(CVehicle* pVehicle);
	FSM_Return			Goto_OnUpdate						(CVehicle* pVehicle);
	// State_Rotate
	void				Rotate_OnEnter						(CVehicle* UNUSED_PARAM(pVehicle));
	FSM_Return			Rotate_OnUpdate						(CVehicle* pVehicle);
	// State_Stop
	void				Stop_OnEnter						(CVehicle* pVehicle);
	FSM_Return			Stop_OnUpdate						(CVehicle* pVehicle);

	void				SteerToVelocity						(CSubmarine* pSub, CVehControls* pVehControls, const Vector3& targetPosition, Vector3 steeringDelta);
	void				UpdateAISteering					(CSubmarine* pSub, Vector3 targetPosition, CVehControls* pVehControls);
	void				UpdateHoverControl					(CSubmarine* pSub, Vector3 targetPosition);

	float				ComputeTargetZToAvoidTerrain		(CSubmarine& pSub, Vector3 targetPosition);
	void				UpdateAvoidance						(CSubmarine& pSub, Vector3& vTargetPosition);
	void				UpdateObjectAvoidance				(CSubmarine& pSub, Vector3& vTargetPosition);
//	void				UpdateEntityAvoidance				(CSubmarine& pSub, Vector3& vTargetPosition);

	void				Submarine_SteerToTarget				(CSubmarine* pSub, Vector3 targetPosition, CVehControls* pVehControls);
	float				ComputeTargetZToAvoidHeightMap		(CSubmarine& pSub, const Vector3& currentPos, float currentSpeed, const Vector3& targetPos);

	static Tunables	sm_Tunables;

	s32		m_iSubFlags;
	int     m_minHeightAboveTerrain;

	//avoidance
	WorldProbe::CShapeTestSingleResult* m_avoidanceTestResults;
	WorldProbe::CShapeTestSingleResult* m_downTestResults;
	float	m_fDistToForwardObject;
	float	m_fLastHeightCheckResult;
	float	m_fLastZCheck;
	float	m_fLocalWaterHeight;
	float	m_fRequestedOrientation;
	float	m_fSlowDownDistance;
	float	m_fSlowDownDistanceMin;

	VehicleNavigationHelper m_navigationHelper;

	const static int s_NumDownProbes = 10;
	int		m_iNumDownProbesUsed;

private:

	enum AvoidanceDir
	{
		AD_ForwardVel = 0,
		AD_Forward,
		AD_Left,
		AD_Right,

		AD_COUNT,
	};

#if DEBUG_DRAW
	Vector3 m_vModifiedTarget;
	WorldProbe::CShapeTestCapsuleDesc* probeDesc;

	WorldProbe::CShapeTestProbeDesc* heightMapProbe;
	struct debugHitData
	{
		bool bHit;
		Vector3 vHitLocation;
	};

	debugHitData* probeHits;
	debugHitData* downProbeHits;
#endif
};

#endif