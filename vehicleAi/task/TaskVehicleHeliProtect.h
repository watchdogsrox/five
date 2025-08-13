//
// CTaskVehicleHeliProtect 
//

#pragma once


#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "Vehicleai\task\TaskVehicleGoToHelicopter.h"

//Rage headers.
#include "Vector/Vector3.h"

class CTaskVehicleHeliProtect : public CTaskVehicleMissionBase
{
public:

	enum State
	{
		State_Init,
		State_Protect,
		kNumStates
	};

	enum RunningFlags
	{
		RF_CanSeeTarget		= BIT0,
		RF_IsColliding		= BIT1,
		RF_TargetChanged	= BIT2
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_minSpeedForSlowDown;
		float	m_maxSpeedForSlowDown;
		float	m_minSlowDownDistance;
		float	m_maxSlowDownDistance;

		PAR_PARSABLE;
	};

	// Constructor/destructor
	CTaskVehicleHeliProtect(const sVehicleMissionParams& params, const float fOffsetDistance = -1.0f, const int iMinHeightAboveTerrain=CTaskVehicleGoToHelicopter::DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN, const int in_HeliFlags = 0);
	CTaskVehicleHeliProtect(const CTaskVehicleHeliProtect& in_rhs);
	~CTaskVehicleHeliProtect();

	s32	GetDefaultStateAfterAbort()	const {return State_Init;}
	int GetTaskTypeInternal() const {return CTaskTypes::TASK_VEHICLE_HELI_PROTECT;}
	CTask* Copy() const {return rage_new CTaskVehicleHeliProtect(*this);}

	// FSM implementations
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleHeliProtect>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN = 12;
		static const unsigned SIZEOF_OFFSET_DISTANCE = 8;

		CTaskVehicleMissionBase::Serialise(serialiser);

		bool hasDefaultParams = (   m_HeliFlags == 0 &&
									m_OffsetDistance == -1.0f && 
									m_MinHeightAboveTerrain == CTaskVehicleGoToHelicopter::DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN);

		SERIALISE_BOOL(serialiser, hasDefaultParams);

		if (!hasDefaultParams || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_HeliFlags, CTaskVehicleGoToHelicopter::HF_NumFlags, "Heli flags");
			u32 offsetDist = (u32)m_OffsetDistance;
			SERIALISE_UNSIGNED(serialiser, offsetDist, SIZEOF_OFFSET_DISTANCE, "Offset distance");
			m_OffsetDistance = (float) offsetDist;
			SERIALISE_UNSIGNED(serialiser, m_MinHeightAboveTerrain, SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN, "Min height above terrain");
		}
		else
		{
			m_HeliFlags = 0;
			m_OffsetDistance = -1;
			m_MinHeightAboveTerrain = CTaskVehicleGoToHelicopter::DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN;
		}
	}

#if __BANK
	virtual void Debug() const;
#endif //__BANK

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName( s32 iState );
#endif //!__FINAL

private:

	FSM_Return Init_OnUpdate(CVehicle& in_Vehicle);

	void Protect_OnEnter(CVehicle& in_Vehicle);
	FSM_Return Protect_OnUpdate(CVehicle& in_Vehicle);


	//////////////////////////////////////////////////////////////////////////
	//
	// 
	//
	//////////////////////////////////////////////////////////////////////////

	Vec3V GetDominantTargetPosition() const;
	Vec3V GetDominantTargetVelocity() const;
	bool CanAnyoneSeeATarget(CVehicle& in_Vehicle) const;

	void ComputeTargetPosition(Vec3V_Ref o_Position, const CVehicle& in_Vehicle, float in_fAngle) const;
	bool ComputeTargetHeading(float& o_Heading, const CVehicle& in_Vehicle, Vec3V_In in_Target) const;
	bool ComputeValidShooterSides(bool& o_Left, bool& o_Right, const CVehicle& in_Vehicle) const;
	void UpdateTargetCollisionProbe(const CVehicle& in_Vehicle, const Vector3& in_Position);
	float ComputeSlowDownDistance(float xySpeed);

	WorldProbe::CShapeTestSingleResult m_targetLocationTestResults;

	CTaskTimer m_NoTargetTimer;
	CTaskTimer m_NoMovementTimer;
	CTaskTimer m_MovementTimer;

	
	float m_StrafeDirection;
	float m_OffsetDistance;
	int m_MinHeightAboveTerrain;
	int m_HeliFlags;
	fwFlags8 m_uRunningFlags;

	static Tunables	sm_Tunables;
};
