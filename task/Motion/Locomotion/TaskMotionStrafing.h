#ifndef TASK_MOTION_STRAFING_H
#define TASK_MOTION_STRAFING_H

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"

//*********************************************************************
//	CTaskMotionStrafing
//	Basic ped strafing behaviour, supporting 360 degree
//	strafing movement
//*********************************************************************

class CTaskMotionStrafing 
	: public CTaskMotionBase
{
public:

	CTaskMotionStrafing( u16 initialState = 0 );
	virtual ~CTaskMotionStrafing();

	enum
	{
		State_NetworkSetup,
		State_Initial,
		State_IdleIntro,
		State_Idle,
		State_Moving
	};

	// CTaskMotionBase virtuals
	virtual FSM_Return		ProcessPreFSM				( void );
	virtual void			ProcessMovement				( void );
	virtual FSM_Return		UpdateFSM					( const s32 iState, const FSM_Event iEvent );
	virtual FSM_Return		ProcessPostFSM				( void );
	virtual CTask*			CreatePlayerControlTask		( void );

	virtual s32				GetDefaultStateAfterAbort	( void ) const { return State_NetworkSetup; }
	virtual aiTask*			Copy						( void ) const { return rage_new CTaskMotionStrafing(m_initialState); }
	virtual s32				GetTaskTypeInternal			( void ) const { return CTaskTypes::TASK_MOTION_STRAFING; }
	
	virtual bool			IsOnFoot					( void ) { return true; }
	virtual	bool			CanFlyThroughAir			( void ) { return false; }
	virtual bool			ShouldStickToFloor			( void ) { return true; }
	virtual	bool			IsInMotion					( const CPed* ) const { return GetState()==State_Moving; }
	virtual bool			IsInMotionState				( CPedMotionStates::eMotionState ) const { return false; }
	virtual bool			SupportsMotionState			( CPedMotionStates::eMotionState ) { return false; }

	virtual void			GetMoveSpeeds				( CMoveBlendRatioSpeeds &speeds );
	virtual void			GetNMBlendOutClip			( fwMvClipSetId& outClipSet, fwMvClipId& outClip );

	bool					ShouldPhaseSync				( void ) { return m_bShouldPhaseSync; }
	void					ResetPhaseSync				( void ) { m_bShouldPhaseSync = false; }
	void					SetInstantPhaseSync			( float fInstantPhaseSync ) { m_fInstantPhaseSync = fInstantPhaseSync; m_bShouldPhaseSync = true; }

	// MoVE interface helper methods
	void					SendParams					( void );	// Send per frame signals to the move network
	bool					StartMoveNetwork			( void );	// helper method to start the move network off

	// Stealth mode 
	bool					IsInStealthMode				( void ) const { return m_bIsInStealthMode; }

	// MoVE network helper accessors
	CMoveNetworkHelper*		GetMoveNetworkHelper		( void ) { return  &m_moveNetworkHelper; }
	const CMoveNetworkHelper* GetMoveNetworkHelper		( void ) const { return  &m_moveNetworkHelper; }

	void					SetUpperbodyClipSet			(const fwMvClipSetId& clipSetId);

	static bool				ShouldPedBeInStealthMode	( CPed* pPed );

	bool					CanRestartTask				( CPed *pPed );

	virtual float GetStandingSpringStrength();

private:
	
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	// FSM State methods
	FSM_Return				StateNetworkSetup_OnUpdate	( void );

	FSM_Return				StateInitial_OnUpdate		( void );

	FSM_Return				StateIdleIntro_OnEnter		( void );
	FSM_Return				StateIdleIntro_OnUpdate		( void );

	FSM_Return				StateIdle_OnEnter			( void );
	FSM_Return				StateIdle_OnUpdate			( void );

	FSM_Return				StateMoving_OnEnter			( void );
	FSM_Return				StateMoving_OnUpdate		( void );

	void					UpdateDesiredHeading		( CPed* pPed );
	bool					HasStealthModeChanged		( CPed* pPed );
	const fwMvNetworkDefId& GetMoveNetworkDefId			( void ) const;
	fwMvClipSetId			GetClipSet					( void );

	// Update the move network with the upperbody signals
	void					SendUpperbodyClipSet();

	// Do we want to move?
	bool					GetWantsToMove() const;

	//
	void					RecordStateTransition();

	//
	bool					CanChangeState() const;

	CMoveNetworkHelper		m_moveNetworkHelper;
	u16						m_initialState;

	// PURPOSE: Turn idle blend parameter
	float					m_fTurnIdleSignal;

	// PURPOSE: Desired strafe direction
	float					m_fDesiredStrafeDirection;

	// PURPOSE: Actual strafe direction
	float					m_fStrafeDirection;

	//  PURPOSE: Override starting phase
	float					m_fInstantPhaseSync;

	// PURPOSE: Upperbody clip set id
	fwMvClipSetId			m_UpperbodyClipSetId;

	//
	static const u32 MAX_ACTIVE_STATE_TRANSITIONS = 3;
	u32 m_uStateTransitionTimes[MAX_ACTIVE_STATE_TRANSITIONS];

	bool					m_bUpperbodyClipSetChanged : 1;	// PURPOSE: Flag the upperbody clipset as having changed
	bool					m_bShouldPhaseSync : 1;
	bool					m_bIsInStealthMode : 1;			// PURPOSE: Toggle back and forth between default and stealth idle

	// MoVE network signals
	static const fwMvFloatId ms_StrafeDirectionId;
	static const fwMvFloatId ms_StrafeSpeedId;
	static const fwMvFloatId ms_DesiredStrafeSpeedId;
	static const fwMvFloatId ms_StrafingIdleTurnRateId;
	static const fwMvFloatId ms_DesiredHeadingId;
	static const fwMvFloatId ms_InstantPhaseSyncId;
	static const fwMvBooleanId ms_IdleIntroOnEnterId;
	static const fwMvBooleanId ms_IdleOnEnterId;
	static const fwMvBooleanId ms_MovingOnEnterId;
	static const fwMvBooleanId ms_BlendOutIdleIntroId;
	static const fwMvRequestId ms_IdleRequestId;
	static const fwMvRequestId ms_MovingRequestId;
	static const fwMvRequestId ms_RestartUpperbodyAnimId;
	static const fwMvFlagId ms_UseUpperbodyAnimId;
	static const fwMvClipSetVarId ms_UpperbodyAnimClipSetId;
};

#endif // TASK_MOTION_STRAFING_H
