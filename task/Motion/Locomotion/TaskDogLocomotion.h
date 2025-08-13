#ifndef TASK_DOG_LOCOMOTION_H
#define TASK_DOG_LOCOMOTION_H

#include "Task/Motion/TaskMotionBase.h"

//*********************************************************************
//	CTaskDogLocomotion
//	The top-level motion task for the on-foot moveblender for a dog. 
//*********************************************************************

class CTaskDogLocomotion
	: public CTaskMotionBase
{
public:
	CTaskDogLocomotion( u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId);
	virtual ~CTaskDogLocomotion();

	enum
	{
		State_Initial,
		State_Idle, 
		State_Locomotion,
		State_TurnL,
		State_TurnR,
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool	ProcessPostMovement();

	virtual CTask * CreatePlayerControlTask();

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskDogLocomotion(m_initialState, m_initialSubState, m_blendType, m_clipSetId); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_DOG; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual const char * GetStateName( s32  ) const;
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void	CleanUp();

	inline bool SupportsMotionState(CPedMotionData::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionData::MotionState_Idle:
		case CPedMotionData::MotionState_Walk:
		case CPedMotionData::MotionState_Run:
		case CPedMotionData::MotionState_Sprint: 
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionData::eMotionState state) const
	{
		switch (state)
		{
		case CPedMotionData::MotionState_Idle:
			return GetState()==State_Idle;
		case CPedMotionData::MotionState_Walk:
		case CPedMotionData::MotionState_Run:
		case CPedMotionData::MotionState_Sprint:
			return GetState()==State_Locomotion;
		default:
			return false;
		}
	}

public:

	virtual bool IsOnFoot() { return true; }

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds);

	virtual	bool CanFlyThroughAir()	{ return false; }

	virtual bool ShouldStickToFloor() { return true; }

	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return GetState()==State_Locomotion; }

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_clipSetId;
		outClip = CLIP_IDLE;
	}

	FSM_Return StateInitial_OnEnter(); 
	FSM_Return StateInitial_OnUpdate(); 

	FSM_Return StateIdle_OnEnter(); 
	FSM_Return StateIdle_OnUpdate(); 
	
	FSM_Return StateLocomotion_OnEnter(); 
	FSM_Return StateLocomotion_OnUpdate(); 

	FSM_Return StateTurnL_OnEnter(); 
	FSM_Return StateTurnL_OnUpdate(); 

	FSM_Return StateTurnR_OnEnter(); 
	FSM_Return StateTurnR_OnUpdate(); 

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 

	u16 m_initialState;
	u16 m_initialSubState;
	u32 m_blendType;		//corresponds to the motion task network blend type. (see fwMove::MotionTaskBlendType)
	fwMvClipSetId m_clipSetId;

	static const fwMvRequestId ms_startTurnLId;
	static const fwMvBooleanId ms_OnEnterTurnLId;

	static const fwMvRequestId ms_startTurnRId;
	static const fwMvBooleanId ms_OnEnterTurnRId;

	static const fwMvBooleanId ms_OnExitTurnId;

	static const fwMvRequestId ms_startIdleId;
	static const fwMvBooleanId ms_OnEnterIdleId;

	static const fwMvRequestId ms_startLocomotionId;
	static const fwMvBooleanId ms_OnEnterLocomotionId;

	static const fwMvFloatId ms_MoveDesiredSpeedId;
	static const fwMvFloatId ms_MoveDirectionId;

private:

	void SetNewHeading( const float assistanceAngle );
	bool ShouldBeMoving();

};


#endif // TASK_DOG_LOCOMOTION_H
