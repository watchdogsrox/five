//
// Task/Motion/Locomotion/TaskMotionAimingTransition.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_MOTION_AIMING_TRANSITION_H
#define TASK_MOTION_AIMING_TRANSITION_H

#include "Task/Motion/TaskMotionBase.h"

class CTaskMotionAimingTransition : public CTaskMotionBase
{
public:

	static bool UseMotionAimingTransition(const float fCurrentHeading, const float fDesiredHeading, const CPed* pPed);
	static bool UseMotionAimingTransition(const float fHeadingDelta, const CPed* FPS_MODE_SUPPORTED_ONLY(pPed));

	enum State
	{
		State_Initial,
		State_Transition,
		State_Finish,
	};

	enum Direction
	{
		D_Invalid,
		D_CW,
		D_CCW,
	};

	CTaskMotionAimingTransition(const fwMvNetworkDefId& networkDefId, const fwMvClipSetId& clipSetId, const bool bLeftFoot, const bool bDoIndependentMoverExpression, const bool bStrafeTransition, const float fMovingDirection, const Direction direction = D_Invalid);
	virtual ~CTaskMotionAimingTransition();

	//
	// aiTask API
	//

	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_AIMING_TRANSITION; }
	virtual aiTask*	Copy() const { return rage_new CTaskMotionAimingTransition(m_NetworkDefId, m_ClipSetId, m_bLeftFoot, m_bDoIndependentMoverExpression, m_bStrafeTransition, m_fInitialMovingDirection); }

#if !__FINAL
	virtual void Debug() const;
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

	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state);
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds);
	virtual	bool IsInMotion(const CPed* pPed) const;
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual CTask* CreatePlayerControlTask();
	virtual bool ShouldStickToFloor() { return true; }
	virtual bool IsOnFoot() { return true; }

	//
	// CTaskMotionAimingTransition API
	//

	bool ShouldStartAimIntro() const;
	bool ShouldBlendBackToStrafe() const;
	bool HasQuitDueToInput() const { return m_bQuitDueToInput; }
	Direction GetDirection() const;
	void ForceQuit() { m_bForceQuit = true; }
	bool WantsToDoIndependentMover() const { return m_bWantsToDoIndependentMover; }

protected:

	//
	// aiTask API
	//

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual	void CleanUp();
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	virtual bool ProcessMoveSignals();

private:

	//
	// States
	//

	FSM_Return	StateInitial_OnUpdate();
	FSM_Return	StateTransition_OnEnter();
	FSM_Return	StateTransition_OnUpdate();
	bool		StateTransition_OnProcessMoveSignals();

#if __ASSERT
	void VerifyClipSet(const bool bCheckWalkAnims);
#endif // __ASSERT

	//
	// Members
	//

	// Move helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Network
	fwMvNetworkDefId m_NetworkDefId;

	// Clip set
	fwMvClipSetId m_ClipSetId;

	// Interpolated moving direction
	float m_fMovingDirection;

	//
	float m_fInitialMovingDirection;

	//
	Direction m_Direction;

	// Flags
	bool m_bLeftFoot : 1;
	bool m_bDoIndependentMoverExpression : 1;
	bool m_bBlockWalk : 1;
	bool m_bMoveBlendOut : 1;
	bool m_bMoveStartAimIntro : 1;
	bool m_bStrafeTransition : 1;
	bool m_bQuitDueToInput : 1;
	bool m_bForceQuit : 1;
	bool m_bWantsToDoIndependentMover : 1;

	//
	// Statics
	//

	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_RateId;
	static const fwMvBooleanId ms_BlendOutId;
	static const fwMvBooleanId ms_StartAimIntroId;
	static const fwMvBooleanId ms_BlendBackToStrafe;
	static const fwMvFlagId ms_LeftFootId;
	static const fwMvFlagId ms_UseWalkId;
	static const fwMvFlagId ms_CWId;
	static const fwMvRequestId ms_IndependentMoverExpressionId;
};


#endif // TASK_MOTION_AIMING_TRANSITION_H
