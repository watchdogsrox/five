#ifndef TASK_VEHICLE_ESCORT_H
#define TASK_VEHICLE_ESCORT_H

// Gta headers.
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGotoHelicopter.h"
#include "VehicleAi\task\TaskVehicleGotoAutomobile.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"



//
//
//
class CTaskVehicleEscort : public CTaskVehicleMissionBase
{
public:

	typedef enum
	{
		State_Invalid = -1,
		State_Escort = 0,
		State_EscortAutomobile,
		State_EscortHelicopter,
		State_TapBrake,
		State_Brake,
		State_Stop,
	} VehicleControlState;

	//keep in sync with commands_task.sch!
	typedef enum
	{
		VEHICLE_ESCORT_REAR = -1,
		VEHICLE_ESCORT_FRONT = 0,
		VEHICLE_ESCORT_LEFT,
		VEHICLE_ESCORT_RIGHT,
		VEHICLE_ESCORT_HELI,
	} VehicleEscortType;


	// Constructor/destructor
	CTaskVehicleEscort(const sVehicleMissionParams& params
		, const VehicleEscortType escortType = VEHICLE_ESCORT_REAR
		, const float fCustomOffset = -1.0f
		, const int iMinHeightAboveTerrain=CTaskVehicleGoToHelicopter::DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN
		, const float fStraightLineDistance=ms_fStraightLineDistDefault);
	~CTaskVehicleEscort();


	int					GetTaskTypeInternal					() const { return CTaskTypes::TASK_VEHICLE_ESCORT; }
	CTask*				Copy						() const {return rage_new CTaskVehicleEscort( m_Params, m_EscortType, m_fCustomOffset, m_iMinHeightAboveTerrain, m_fStraightLineDist);}

#if __BANK
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;
#endif //__BANK

	// FSM implementations
	FSM_Return			ProcessPreFSM						();
	aiTask::FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	virtual void		CleanUp();
	s32				GetDefaultStateAfterAbort	()	const {return State_Escort;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
#endif //!__FINAL

	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const;

	VehicleEscortType GetEscortType() const {return m_EscortType;}

	void				FindTargetCoors				(const CVehicle *pVeh, sVehicleMissionParams &params) const;
	void				FindTargetCoors				(const CVehicle *pVeh, Vector3 &TargetCoors) const;
	void				AdjustTargetPositionDependingOnDistance(const CVehicle* pVeh, sVehicleMissionParams& params) const;
	void				AdjustTargetPositionDependingOnDistance(const CVehicle* pVeh, Vector3& vTargetPosition) const;
	bool				InConvoyTogether			(const CVehicle* const pVeh, const CVehicle* const pOtherVeh);

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleEscort>; }
    virtual void CloneUpdate(CVehicle *pVehicle);

	void Serialise(CSyncDataBase& serialiser)
	{
		CTaskVehicleMissionBase::Serialise(serialiser);

		// add one to avoid having to serialise an int
		unsigned escortType = (unsigned)(m_EscortType + 1);
		SERIALISE_UNSIGNED(serialiser, escortType, SIZEOF_ESCORT_TYPE, "Escort type");
		m_EscortType = (VehicleEscortType)(escortType - 1);

		SERIALISE_PACKED_FLOAT(serialiser, m_fCustomOffset,  200.0f, SIZEOF_CUSTOM_OFFSET, "Custom Offset");

		SERIALISE_INTEGER(serialiser, m_iMinHeightAboveTerrain, SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN, "Min Height Above Terrain");
	}

	static void HandleBrakingIfAheadOfTarget(const CVehicle* pVehicle, const CPhysical* pTargetEntity, const Vector3& vTargetPos, const bool bDriveInReverse, const bool bAllowMinorSlowdown, u32& nNextTimeAllowedToMove, float& fDesiredSpeed);

private:

	static const unsigned SIZEOF_ESCORT_TYPE = 3;
	static const unsigned SIZEOF_CUSTOM_OFFSET = 10;
	static const unsigned SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN = 12;

	// FSM function implementations
	// State_Escort
	FSM_Return			Escort_OnUpdate				(CVehicle* pVehicle);
	//	State_EscortHelicopter
	void				EscortHelicopter_OnEnter	(CVehicle* pVehicle);
	FSM_Return			EscortHelicopter_OnUpdate	(CVehicle* pVehicle);
	//	State_EscortAutomobile
	void				EscortAutomobile_OnEnter	(CVehicle* pVehicle);

	FSM_Return			EscortAutomobile_OnUpdate	(CVehicle* pVehicle);
	// TapBrake state
	void				TapBrake_OnEnter			(CVehicle* pVeh);
	FSM_Return			TapBrake_OnUpdate			(CVehicle* pVeh);
	// Brake state
	void				Brake_OnEnter				(CVehicle* pVeh);
	FSM_Return			Brake_OnUpdate				(CVehicle* pVeh);
	// State_Stop
	void				Stop_OnEnter				(CVehicle* pVehicle);
	FSM_Return			Stop_OnUpdate				(CVehicle* pVehicle);

	void			UpdateAvoidanceCache();

	VehicleEscortType	m_EscortType;
	float				m_fCustomOffset;
	float				m_fStraightLineDist;
	u32					m_nNextTimeAllowedToMove;
	int					m_iMinHeightAboveTerrain;

	static const float	ms_fStraightLineDistDefault;
};

#endif
