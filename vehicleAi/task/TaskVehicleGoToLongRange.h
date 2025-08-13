#ifndef TASK_VEHICLE_GOTO_LONGRANGE_H
#define TASK_VEHICLE_GOTO_LONGRANGE_H

#include "TaskVehicleCruise.h"

class CTaskVehicleGotoLongRange : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_FindTarget = 0,
		State_Goto,
		State_Stop, 
	} VehicleControlState;

	CTaskVehicleGotoLongRange(const sVehicleMissionParams& params);
	virtual ~CTaskVehicleGotoLongRange();


	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_LONGRANGE; }
	aiTask*		Copy() const {return rage_new CTaskVehicleGotoLongRange(m_Params);}


	// FSM implementations
	FSM_Return	UpdateFSM							(const s32 iState, const FSM_Event iEvent);
	virtual void		CleanUp();

	s32			GetDefaultStateAfterAbort			()	const {return State_FindTarget;}

	void		ProcessTimeslicedUpdate() const;

	virtual void DoSoftReset();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
#endif //!__FINAL

	// network:
	 virtual void CloneUpdate(CVehicle *pVehicle);
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGotoLongRange>; }

protected:

	//State_FindTarget
	void FindTarget_OnEnter();
	FSM_Return FindTarget_OnUpdate();

	//State_GoTo
	void Goto_OnEnter();
	FSM_Return Goto_OnUpdate();

	//State Stop
	void				Stop_OnEnter	();
	FSM_Return			Stop_OnUpdate	();

	bool		GetExtentsForRequiredNodesToLoad(const CVehicle& vehicle, const Vector3& vDest, Vector2& vMinOut, Vector2& vMaxOut);
	void		LoadRequiredNodes(const Vector2& vMin, const Vector2& vMax) const;
	bool		AreRequiredLoadesNoded() const;

	Vector3		m_CurrentIntermediateDestination;
	Vector2		m_vNodesMin;
	Vector2		m_vNodesMax;
	s32			m_iZoneWeAreIn;
	bool		m_bNavigatingToFinalDestination;
	bool		m_bExtentsCalculated;
	mutable u32	m_uLastFrameRequested;

	static const float	ms_fMinArrivalDistToIntermediateDestination;
};

#endif	//TASK_VEHICLE_GOTO_LONGRANGE_H