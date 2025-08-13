#ifndef TASK_FLIGHTLESS_BIRD_LOCOMOTION_H
#define TASK_FLIGHTLESS_BIRD_LOCOMOTION_H

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"
//*********************************************************************
//	CTaskFlightlessBirdLocomotion
//	The top-level motion task for the on-foot moveblender for a flightless bird. 
//*********************************************************************

class CTaskFlightlessBirdLocomotion
	: public CTaskMotionBase
{
public:
	CTaskFlightlessBirdLocomotion( u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId );
	virtual ~CTaskFlightlessBirdLocomotion();

	enum
	{
		State_Initial,
		State_Idle, 
		State_Walk,
		State_Run
	};

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool		ProcessMoveSignals();

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskFlightlessBirdLocomotion(m_initialState, m_initialSubState, m_blendType, m_clipSetId); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_FLIGHTLESS_BIRD; }

	// Player hens are able to adjust their pitch while walking on slopes.
	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void	CleanUp();

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
			return GetState()==State_Idle;
		case CPedMotionStates::MotionState_Walk:
			return GetState()==State_Walk;
		case CPedMotionStates::MotionState_Run:
			return GetState()==State_Run;
		default:
			return false;
		}
	}

	virtual CTask* CreatePlayerControlTask();

protected:

	void SetNewHeading(  const float assistanceAngle );

private:

	bool IsInWaterWhileLowLod() const;

public:

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_clipSetId;
		outClip = CLIP_IDLE;
	}

	virtual bool IsOnFoot() { return true; }

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	virtual	bool CanFlyThroughAir()	{ return false; }

	virtual bool ShouldStickToFloor() { return true; }

	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return GetState()==State_Walk || GetState()==State_Run; }

	void		StateInitial_OnEnter(); 
	FSM_Return	StateInitial_OnUpdate(); 

	void		StateIdle_OnEnter(); 
	FSM_Return	StateIdle_OnUpdate();
	void		StateIdle_OnProcessMoveSignals();
	
	void		StateWalk_OnEnter(); 
	FSM_Return	StateWalk_OnUpdate();
	void		StateWalk_OnProcessMoveSignals();
	
	void		StateRun_OnEnter(); 
	FSM_Return	StateRun_OnUpdate();
	void		StateRun_OnProcessMoveSignals();

	void AllowTimeslicing();	

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 

	u16 m_initialState;
	u16 m_initialSubState;
	u32 m_blendType;		//corresponds to the motion task network blend type. (see CMove::MotionTaskBlendType)
	fwMvClipSetId m_clipSetId;
	float m_fTurnDiscrepancyTurnRate; 
	float m_fDesiredTurnForHeadingLerp;

	// PURPOSE:  Cached bool for timeslicing with MoVE.
	bool m_bMoveInTargetState;

	static const fwMvRequestId ms_startIdleId;
	static const fwMvBooleanId ms_OnEnterIdleId;

	static const fwMvRequestId ms_startWalkId;
	static const fwMvBooleanId ms_OnEnterWalkId;

	static const fwMvRequestId ms_startRunId;
	static const fwMvBooleanId ms_OnEnterRunId;

	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_IdleRateId;

	static dev_float ms_fMaxPitch;

};

#endif // TASK_FLIGHTLESS_BIRD_LOCOMOTION_H
