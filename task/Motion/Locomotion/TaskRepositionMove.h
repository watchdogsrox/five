#ifndef TASK_REPOSITION_MOVE_H
#define TASK_REPOSITION_MOVE_H

// Game headers
#include "Task/Motion/TaskMotionBase.h"

class CTaskRepositionMove : public CTaskMotionBase
{
public:	

	enum State
	{
		State_Initial,
		State_Reposition,
		State_Quit,
	};

	CTaskRepositionMove();
	virtual ~CTaskRepositionMove();

	//
	// aiTask API
	//

	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_REPOSITION_MOVE; }
	virtual aiTask*	Copy() const { return rage_new CTaskRepositionMove(); }

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif // !__FINAL

	//
	// CTask API
	//

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	//
	// Motion Base API
	//

	virtual bool SupportsMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) { return false; }
	virtual bool IsInMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) const { return false; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& UNUSED_PARAM(speeds)) {}
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& UNUSED_PARAM(outClipSet), fwMvClipId& UNUSED_PARAM(outClip)) {}

protected:

	//
	// aiTask API
	//

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual	void CleanUp();
	virtual s32	GetDefaultStateAfterAbort() const { return State_Quit; }

private:

	//
	// States
	//

	FSM_Return StateInitial_OnUpdate();
	FSM_Return StateReposition_OnEnter();
	FSM_Return StateReposition_OnUpdate();

	//
	// Members
	//

	// Move helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Target position
	Vector3 m_vTargetPos;

	// Start position
	Vector3 m_vStartPos;

	// Desired heading
	float m_fDesiredHeading;

	// Distance
	float m_fDistance;

	// Direction
	float m_fDirection;

	//
	// Move Ids
	//

	// Control params
	static const fwMvFloatId ms_DesiredHeading;
	static const fwMvFloatId ms_Distance;
	static const fwMvFloatId ms_Direction;
	// Requests
	static const fwMvRequestId ms_RepositionMoveRequestId;
	// State Enters/Exits
	static const fwMvBooleanId ms_RepositionMoveOnEnterId;
	// Early outs
	static const fwMvBooleanId ms_BlendOutId;
};

#endif // TASK_REPOSITION_MOVE_H
