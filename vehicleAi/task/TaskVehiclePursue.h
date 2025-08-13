#ifndef TASK_VEHICLE_PURSUE_H
#define TASK_VEHICLE_PURSUE_H

// Rage headers
#include "atl/queue.h"
#include "fwutil/Flags.h"

// Game headers
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

// Forward declarations
class CPhysical;
class CTaskVehicleGoToAutomobileNew;

//=========================================================================
// CTaskVehiclePursue
//=========================================================================

class CTaskVehiclePursue : public CTaskVehicleMissionBase
{

private:

	enum RunningFlags
	{
		RF_HasNavMeshLineOfSight		= BIT0,
		RF_IsObstructedByMapGeometry	= BIT1,
	};

	enum State
	{
		State_Start = 0,
		State_GetBehind,
		State_Pursue,
		State_Seek,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		struct Drift
		{
			Drift()
			{}
			
			float m_MinValueForCorrection;
			float m_MaxValueForCorrection;
			float m_MinRate;
			float m_MaxRate;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		Drift m_DriftX;
		Drift m_DriftY;
		float m_TimeToLookBehind;
		float m_MinDistanceToLookBehind;
		float m_SpeedDifferenceForMinDistanceToStartMatchingSpeed;
		float m_SpeedDifferenceForMaxDistanceToStartMatchingSpeed;
		float m_MinDistanceToStartMatchingSpeed;
		float m_MaxDistanceToStartMatchingSpeed;
		float m_CruiseSpeedMultiplierForBackOff;
		float m_DotToClampSpeedToMinimum;
		float m_DotToClampSpeedToMaximum;
		float m_SpeedForMinimumDot;
		float m_TimeBetweenLineOfSightChecks;
		float m_DistanceForStraightLineModeAlways;
		float m_DistanceForStraightLineModeIfLos;

		PAR_PARSABLE;
	};

	CTaskVehiclePursue(const sVehicleMissionParams& rParams, float fIdealDistance = 15.0f, float fDesiredOffset = 0.0f);
	virtual ~CTaskVehiclePursue();
	
public:

	float GetDesiredOffset() const { return m_fDesiredOffset; }
	
public:

	Vec3V_Out	GetDrift() const;
	void		SetDesiredOffset(float fDesiredOffset);
	void		SetIdealDistance(float fIdealDistance);
	
public:

	static Mat34V_Out	CalculateTargetMatrix(const CPhysical& rTarget);
	static Vec3V_Out	CalculateTargetPosition(const CPhysical& rTarget);
	static Vec3V_Out	CalculateTargetPosition(Mat34V_In mTarget);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePursue>; }

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehiclePursue(m_Params, m_fIdealDistance, m_fDesiredOffset); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PURSUE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		GetBehind_OnEnter();
	FSM_Return	GetBehind_OnUpdate();
	void		Pursue_OnEnter();
	FSM_Return	Pursue_OnUpdate();
	void		Pursue_OnExit();
	void		Seek_OnEnter();
	FSM_Return	Seek_OnUpdate();

private:

	float				CalculateCruiseSpeedForPursue() const;
	Vec3V_Out			CalculateTargetPosition() const;
	float				ClampCruiseSpeedForPursue(float fCruiseSpeed) const;
	const CPhysical*	GetDominantTarget() const;
	const CPed*			GetTargetPed() const;
	bool				IsPursueAppropriate() const;
	bool				IsPursueNoLongerAppropriate() const;
	bool				IsSeekAppropriate() const;
	bool				IsSeekNoLongerAppropriate() const;
	bool				IsStuck() const;
	void				ProcessDrift();
	void				ProcessProbes();
	void				UpdateAvoidanceCache();
	void				UpdateCruiseSpeedForPursue(CTaskVehicleGoToAutomobileNew& rTask);
	void				UpdateNavMeshLineOfSight();
	void				UpdateStraightLineDistanceForPursue(CTaskVehicleGoToAutomobileNew& rTask);
	void				UpdateTargetPositionForGetBehind(CTaskVehicleGoToAutomobileNew& rTask);
	void				UpdateTargetPositionForPursue(CTaskVehicleGoToAutomobileNew& rTask);
	void				UpdateVehicleMissionForGetBehind();
	void				UpdateVehicleMissionForPursue();
	void				CloneUpdate(CVehicle* pVehicle);

private:

	CNavmeshLineOfSightHelper			m_NavMeshLineOfSightHelper;
	CVehicleDriftHelper					m_DriftHelperX;
	CVehicleDriftHelper					m_DriftHelperY;
	WorldProbe::CShapeTestSingleResult	m_ProbeResults;
	u32									m_nNextTimeAllowedToMove;
	float								m_fIdealDistance;
	float								m_fDesiredOffset;
	float								m_fTimeSinceLastLineOfSightCheck;
	fwFlags8							m_uRunningFlags;

private:

	static Tunables	sm_Tunables;

};

#endif
