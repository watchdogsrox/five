#ifndef TASK_COMBAT_ROLL_H
#define TASK_COMBAT_ROLL_H

// Game headers
#include "Task/Motion/TaskMotionBase.h"

//////////////////////////////////////////////////////////////////////////
// CTaskCombatRoll
//////////////////////////////////////////////////////////////////////////

class CTaskCombatRoll : public CTaskMotionBase
{
public:

	CTaskCombatRoll();

	virtual aiTask* Copy() const { return rage_new CTaskCombatRoll(); }

	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_COMBAT_ROLL; }

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	virtual bool SupportsMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) { return false; }
	virtual bool IsInMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) const { return false; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds);
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return true; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual CTask* CreatePlayerControlTask();
	virtual bool ShouldStickToFloor() { return true; }
	virtual bool IsOnFoot() { return true; }

	bool ShouldTurnOffUpperBodyAnimations() const;
	float GetDirection() const { return m_fDirection; }
	bool GetIsLanding() const {return GetState()==State_RollLand;}
	bool IsForwardRoll() const { return m_bForwardRoll; }

	void SetRemotActive(bool bValue) { m_bRemoteActive = bValue; }
	void SetRemoteDirection(float fDirection) { m_fRemoteDirection = fDirection; }
	void SetRemoteForwardRoll(bool bForwardRoll) { m_bRemoteForwardRoll = bForwardRoll; }

protected:

	enum State
	{
		State_Initial = 0,
		State_RollLaunch,
		State_RollLand,
		State_Quit,
	};

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	virtual	void CleanUp();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif // !__FINAL

	virtual bool ProcessMoveSignals();

private:

	//
	// State functions
	//

	FSM_Return StateInitial_OnUpdate();
	FSM_Return StateRollLaunch_OnEnter();
	FSM_Return StateRollLaunch_OnUpdate();
	FSM_Return StateRollLand_OnEnter();
	FSM_Return StateRollLand_OnUpdate();

	//
	// Members
	//

	// Move helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Interpolated direction
	float m_fDirection;

	float m_fRemoteDirection;

	// Keep track if this is a forward or backward roll
	bool m_bForwardRoll : 1;

	// Cached move signals
	bool m_bMoveExitLaunch : 1;
	bool m_bMoveExitLand : 1;
	bool m_bMoveRestartUpperBody : 1;

	bool m_bRemoteActive : 1;
	bool m_bRemoteForwardRoll : 1;
	
	//
	// Statics
	//

	static dev_float ms_FwdMinAngle;
	static dev_float ms_FwdMaxAngle;

	// Move signals
	static fwMvClipSetId ms_ClipSetId;
	static fwMvClipSetId ms_FpsClipSetId;
	static fwMvRequestId ms_RequestLaunchId;
	static fwMvRequestId ms_RequestLandId;
	static fwMvBooleanId ms_LaunchOnEnterId;
	static fwMvBooleanId ms_LandOnEnterId;
	static fwMvBooleanId ms_ExitLaunchId;
	static fwMvBooleanId ms_ExitLandId;
	static fwMvBooleanId ms_RestartUppderBodyId;
	static fwMvFloatId ms_DirectionId;
};

#endif // TASK_COMBAT_ROLL_H
