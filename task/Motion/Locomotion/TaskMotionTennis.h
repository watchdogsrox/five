#ifndef TASK_MOTION_TENNIS_H
#define TASK_MOTION_TENNIS_H

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"

//*********************************************************************
//	CTaskMotionTennis
//	Tennis ped strafing behaviour, supporting 360 degree
//	strafing movement
//*********************************************************************

class CTaskMotionTennis 
	: public CTaskMotionBase
{
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_StrafeDirectionLerpRateMinAI;
		float m_StrafeDirectionLerpRateMaxAI;
		float m_StrafeDirectionLerpRateMinPlayer;
		float m_StrafeDirectionLerpRateMaxPlayer;
		PAR_PARSABLE;
	};

	CTaskMotionTennis(bool bUseFemaleClipSet, bool bAllowCloneOverride=false);
	virtual ~CTaskMotionTennis();

	enum
	{
		State_Initial,
		State_Idle,
		State_Moving,
		State_Start,
		State_Stop,
		State_Swing,
	};

	// CTaskMotionBase virtuals
	virtual FSM_Return		ProcessPreFSM				( void );
	virtual FSM_Return		ProcessPostFSM				( void );
	virtual void			ProcessMovement				( void );
	virtual FSM_Return		UpdateFSM					( const s32 iState, const FSM_Event iEvent );
	virtual CTask*			CreatePlayerControlTask		( void );

	virtual s32				GetDefaultStateAfterAbort	( void ) const { return State_Initial; }
	virtual aiTask*			Copy						( void ) const { return rage_new CTaskMotionTennis(m_bUseFemaleClipSet); }
	virtual s32				GetTaskTypeInternal			( void ) const { return CTaskTypes::TASK_MOTION_TENNIS; }
	
	virtual bool			IsOnFoot					( void ) { return true; }
	virtual	bool			CanFlyThroughAir			( void ) { return false; }
	virtual bool			ShouldStickToFloor			( void ) { return true; }
	virtual	bool			IsInMotion					( const CPed* ) const { return GetState()==State_Moving; }
	virtual bool			IsInMotionState				( CPedMotionStates::eMotionState ) const { return false; }
	virtual bool			SupportsMotionState			( CPedMotionStates::eMotionState ) { return false; }

	virtual void			GetMoveSpeeds				( CMoveBlendRatioSpeeds &speeds );
	virtual void			GetNMBlendOutClip			( fwMvClipSetId& outClipSet, fwMvClipId& outClip );

	// MoVE interface helper methods
	bool					SyncToMoveState				( void );	// Synchronizes the code FSM to the MoVE network state
	void					SendParams					( void );	// Send per frame signals to the move network
	bool					StartMoveNetwork			( void );	// helper method to start the move network off

	// MoVE network helper accessors
	CMoveNetworkHelper*		GetMoveNetworkHelper		( void ) { return  &m_moveNetworkHelper; }
	const CMoveNetworkHelper* GetMoveNetworkHelper		( void ) const { return  &m_moveNetworkHelper; }

	void PlayTennisSwingAnim(atHashWithStringNotFinal animDictName, atHashWithStringNotFinal animName, float fStartPhase, float fPlayRate, bool bSlowBlend);
	void PlayTennisSwingAnim(const char *pAnimDictName, const char *pAnimName, float fStartPhase, float fPlayRate, bool bSlowBlend);
	void SetSwingAnimPlayRate(float fPlayRate);
	bool GetTennisSwingAnimComplete();
	bool GetTennisSwingAnimCanBeInterrupted();
	void SetAllowDampenStrafeDirection(bool bValue) { m_bAllowDampen = bValue;}
	bool GetTennisSwingAnimSwung();

	enum eDiveDirection
	{
		Dive_BH,
		Dive_FH
	};
	void PlayDiveAnim(bool bDirection, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend);
	void PlayDiveAnim(eDiveDirection direction, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// For clone syncing
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bool GetCloneAllowApplySwing();
	void SetNetSyncTennisSwingAnimData(atHashWithStringNotFinal animDictName, atHashWithStringNotFinal animName, float fStartPhase, float fPlayRate, bool bSlowBlend);
	void SetNetSyncTennisDiveAnimData(eDiveDirection direction, float fDiveHorizontal, float fDiveVertical, float fPlayRate, bool bSlowBlend);
	void GetNetSyncTennisSwingAnimData(u32 &dictHash, u32 &clipHash, float &fStartPhase, float &fPlayRate, bool &bSlowBlend);
	void GetNetSyncTennisDiveAnimData(bool &diveDirection, float &fDiveHorizontal, float &m_fDiveVertical, float &fPlayRate, bool &bSlowBlend);
	bool IsNetSyncTennisSwingAnimDataValid();
	bool IsNetSyncTennisDiveAnimDataValid();
	void ClearNetSyncData();

	void SetAllowOverrideCloneUpdate(bool bOverride) {m_bAllowOverrideCloneUpdate =bOverride;}
	bool GetAllowOverrideCloneUpdate() { return m_bAllowOverrideCloneUpdate ;}

	void SetControlOutOfDeadZone(bool bControlOutOfDeadZone) {m_bControlOutOfDeadZone =bControlOutOfDeadZone;}
	bool GetControlOutOfDeadZone() { return m_bControlOutOfDeadZone ;}

	void SetSignalFloat( const char* szSignal, float fSignal );

	static const Tunables& GetTunables() { return ms_Tunables; }

private:
	
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM State methods
	FSM_Return				StateInitial_OnEnter		( void );
	FSM_Return				StateInitial_OnUpdate		( void );

	FSM_Return				StateIdle_OnEnter			( void );
	FSM_Return				StateIdle_OnUpdate			( void );
	FSM_Return				StateIdle_OnExit			( void );

	FSM_Return				StateMoving_OnEnter			( void );
	FSM_Return				StateMoving_OnUpdate		( void );
	FSM_Return				StateMoving_OnExit			( void );

	FSM_Return				StateStart_OnEnter			( void );
	FSM_Return				StateStart_OnUpdate			( void );
	FSM_Return				StateStart_OnExit			( void );
	
	FSM_Return				StateStop_OnEnter			( void );
	FSM_Return				StateStop_OnUpdate			( void );
	FSM_Return				StateStop_OnExit			( void );

	FSM_Return				StateSwing_OnEnter			( void );
	FSM_Return				StateSwing_OnUpdate			( void );
	FSM_Return				StateSwing_OnExit			( void );

	void					UpdateDesiredHeading		( CPed* pPed );
	const fwMvNetworkDefId& GetMoveNetworkDefId			( void ) const;

	fwMvClipSetId			GetClipSetId() const;
	
	CMoveNetworkHelper		m_moveNetworkHelper;
	u16						m_initialState;

	// PURPOSE: Desired backwards blend parameter
	float					m_fDesiredBackwardsBlend;

	// PURPOSE: Current backwards blend parameter
	float					m_fCurrentBackwardsBlend;

	// PURPOSE: Strafe speed calculated from move blend ratio
	float					m_fStrafeSpeed;

	// PURPOSE: Desired strafe direction
	float					m_fDesiredStrafeDirection;

	// PURPOSE: Actual strafe direction
	float					m_fStrafeDirection;

	// PURPOSE: Strafe stop direction
	float					m_fStrafeStopDirection;

	// PURPOSE: Strafe start direction
	float					m_fStrafeStartDirection;

	// PURPOSE: When the strafe stop starts/ends.
	bool					m_bOverrideStrafeDirectionWithStopDirection;
	bool					m_bStopEnd;

	bool					m_bUseDampening;
	bool					m_bAllowDampen;

	bool					m_bCWStart;
	bool					m_bRequestStart;
	bool					m_bRequestStop;
	bool					m_bRequestSwing;

	//For clone
	float						m_fCloneDiveHorizontal;
	float						m_fCloneDiveVertical;
	atHashWithStringNotFinal	m_CloneAnimDictName;
	atHashWithStringNotFinal	m_CloneAnimName;
	float						m_CloneStartPhase;
	float						m_ClonePlayRate;
	bool						m_bDiveMode		: 1;
	bool						m_bDiveDir		: 1;
	bool						m_bCloneSlowBlend	: 1;
	bool						m_bAllowOverrideCloneUpdate : 1;
	bool						m_bForceResendSyncData : 1;
	bool						m_bControlOutOfDeadZone : 1;

	// For clip set selection
	bool					m_bUseFemaleClipSet;

	// Store the swing/dive anim info
	atHashWithStringNotFinal m_swingAnimDictName;
	atHashWithStringNotFinal m_swingAnimName;
	float					 m_fSwingStartPhase;
	float					 m_fPlayRate;
	bool					 m_bSlowBlend;
	eDiveDirection			 m_DiveDirection;
	float					 m_fDiveHorizontal;
	float					 m_fDiveVertical;
	bool					 m_bSendingDiveRequest;

	// MoVE network signals
	static const fwMvFloatId ms_StrafeDirectionId;
	static const fwMvFloatId ms_StrafeSpeedId;
	static const fwMvFloatId ms_BackwardsStrafeBlend;
	static const fwMvFloatId ms_DesiredStrafeSpeedId;
	static const fwMvFloatId ms_StrafingRateId;	
	static const fwMvFloatId ms_DesiredStrafeDirectionId;
	static const fwMvFloatId ms_StrafeStopDirectionId;
	static const fwMvFloatId ms_StrafeStartDirectionId;

	static const fwMvRequestId ms_LocoStartId;
	static const fwMvRequestId ms_LocoStopId;

	static const fwMvBooleanId ms_InterruptMoveId;
	static const fwMvBooleanId ms_InterruptStopId;
	static const fwMvBooleanId ms_InterruptSwingId;
	static const fwMvBooleanId ms_SwungId;

	static const fwMvClipId ms_SwingClipId;
	static const fwMvRequestId ms_SwingRequestId;
	static const fwMvFloatId ms_PhaseFloatId;
	static const fwMvFloatId ms_PlayRateFloatId;

	static const fwMvFlagId ms_DiveBHId;
	static const fwMvFlagId ms_DiveFHId;
	static const fwMvFloatId ms_DiveHorizontalFloatId;
	static const fwMvFloatId ms_DiveVerticalFloatId;
	
	static const fwMvFlagId ms_SlowBlendId;

	static Tunables		 ms_Tunables;

	static fwMvClipSetId defaultClipSetId;
	static fwMvClipSetId defaultClipSetIdFemale;

#if __BANK
public:
#define		MAX_SWING_NAME_LEN	 64
	static bool		ms_ForceSend;
	static bool		ms_bankDiveLeft;
	static bool		ms_bankOverrideCloneUpdate;
	static char		ms_NameOfSwing[MAX_SWING_NAME_LEN];
	static float	ms_fPlayRate;
	static float	ms_fStartPhase;
	static float	ms_fDiveHoriz;
	static float	ms_fDiveVert;

	static void InitWidgets();
	static CTaskMotionTennis* GetActiveTennisMotionTaskOnFocusPed();
	//static void SendDive();
	static void SendSwing();
	static void SendDive();
	static void SetAllowOverrideCloneUpdate();
#endif

};

#endif // TASK_MOTION_TENNIS_H
