//
// PlaneChase
// Designed for following some sort of entity
//

#ifndef TASK_VEHICLE_PLANE_CHASE_H
#define TASK_VEHICLE_PLANE_CHASE_H

#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"

class CVehControls;
class CPlane;

// Forward declarations
class CPlane;

//=========================================================================
// CTaskVehiclePlaneChase
//=========================================================================

class CTaskVehiclePlaneChase : public CTaskVehicleGoTo
{

public:

	enum State
	{
		State_Start = 0,
		State_Pursue,
		State_EscapePursuer,
		State_Finish,
	};

	struct Tunables : public CTuning
	{
		Tunables();
		float MinSpeed;
		float MaxSpeed;
		PAR_PARSABLE;
	};
	
	CTaskVehiclePlaneChase(const sVehicleMissionParams& params, const CAITarget& rTarget);
	virtual ~CTaskVehiclePlaneChase();
	
	void SetMinChaseSpeed(float in_Speed);
	void SetMaxChaseSpeed(float in_Speed);
	
#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);

	float m_fComputedMinSpeed;
#endif // !__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePlaneChase>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		const unsigned int SIZE_OF_MIN_CHASE_SPEED = 8;
		const unsigned int SIZE_OF_MAX_CHASE_SPEED = 8;

		CTaskVehicleMissionBase::Serialise(serialiser);

		CSyncedEntity entity;
		entity.SetEntity(const_cast<CEntity*>(m_Target.GetEntity()));

		ObjectId targetID = entity.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Target Entity");
		entity.SetEntityID(targetID);

		if (entity.GetEntity())
		{
			m_Target.SetEntity(entity.GetEntity());		
		}
		else
		{
			m_Target.Clear();
		}

		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMinSpeed, 50.0f,  SIZE_OF_MIN_CHASE_SPEED, "Min Chase Speed");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMaxSpeed, 500.0f,  SIZE_OF_MAX_CHASE_SPEED, "Max Chase Speed");
	}

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehiclePlaneChase(m_Params, m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLANE_CHASE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return	Start_OnUpdate();
	void		Pursue_OnEnter();
	FSM_Return	Pursue_OnUpdate();

	void		EscapePursuer_OnEnter();
	FSM_Return	EscapePursuer_OnUpdate();
	void		EscapePursuer_OnExit();
	
	Vec3V_Out	CalculateTargetPosition() const;
	Vec3V_Out	CalculateTargetFuturePosition(float in_fTimeAhead = 1.0f) const;
	Vec3V_Out	CalculateTargetVelocity() const;

	bool			IsTargetInAir() const;
	CPlane*			GetPlane() const;
	const CEntity*	GetTargetEntity() const;
	void			UpdateTargetPursuingUs();
	void			CalculateEvadeTargetPosition();
	const CVehicle*	GetTargetVehicle() const;
	CAITarget			m_Target;

	// lets try to smooth this out a bit
	Vector3				m_PreviousTargetPosition;

	float				m_fMinSpeed;
	float				m_fMaxSpeed;
	float				m_fTargetAvgSpeed;
	float				m_fTimeBeingPursued;
	bool				m_bWasDiveBombingLastFrame;

	struct EvadeData
	{
		CTaskGameTimer		m_beingPursuedTimer;
		bool				m_bIsRollingRight;
		float				m_fReavaluateTime;
		Vector3				m_vCurrentTarget;
	};
	EvadeData m_evadeData;


	static Tunables	sm_Tunables;
};

#endif // TASK_PLANE_CHASE_H
