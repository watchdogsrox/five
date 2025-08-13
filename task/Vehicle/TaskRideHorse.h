#ifndef INC_TASK_RIDE_HORSE_H
#define INC_TASK_RIDE_HORSE_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/AssistedMovement.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/motion/TaskMotionBase.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"

//-------------------------------------------------------------------------
// Task to control the characters clips whilst on a horse
//-------------------------------------------------------------------------
class CTaskMotionRideHorse : public CTaskMotionBase
{
public:

	CTaskMotionRideHorse();
	virtual ~CTaskMotionRideHorse();

	enum eOnHorseState
	{
		State_Start,
		State_StreamClips,
		State_SitIdle,
		State_Locomotion, 
		State_QuickStop,
		State_Braking,
		State_Exit
	};

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32			GetDefaultStateAfterAbort() const { return State_SitIdle; }
	virtual aiTask*		Copy() const { return rage_new CTaskMotionRideHorse(); }
	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_RIDE_HORSE; }
	virtual FSM_Return	ProcessPreFSM();	
	virtual FSM_Return	ProcessPostFSM();
	virtual bool		ProcessPostPreRender();

	// FSM optional functions	
	virtual	void	CleanUp();
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }	
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;
	static u32 GetCameraHash()					{ return ms_CameraHash; }
	static s32 GetCameraInterpolationDuration()	{ return ms_CameraInterpolationDuration; }

	// Task motion base methods
	virtual bool IsInVehicle() { return true; } // ?
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);
	virtual	bool CanFlyThroughAir() { return false; }
	virtual CTask * CreatePlayerControlTask();
	virtual bool ShouldStickToFloor() { return false; }
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_moveNetworkHelper.GetClipSetId();
		outClip = CLIP_IDLE;
	}

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) const
	{return false;}

	static void UpdateStirrups(CPed* rider, CPed* horse);
	static void UpdateStirrup(CPed* rider, CPed* horse, eAnimBoneTag nRiderBoneId, eAnimBoneTag nStirrupBoneId, const Vector3& vOffset, float fBlendPhase);

	static const fwMvClipSetVarId ms_SpurClipSetVarId;
	static const fwMvRequestId ms_StartLocomotion;
	static const fwMvRequestId ms_StartIdle;
	static const fwMvRequestId ms_Spur;	
	static const fwMvRequestId ms_QuickStopId;
	static const fwMvRequestId ms_BrakingId;
	static const fwMvBooleanId ms_QuickStopClipEndedId;	
	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvBooleanId ms_OnEnterRideGaitsId;
	static const fwMvBooleanId ms_OnEnterQuickStopId;	
	static const fwMvBooleanId ms_OnEnterBrakingId;	
	static const fwMvBooleanId ms_OnExitRideGaitsId;
	static const fwMvFloatId ms_MountSpeed;	
	static const fwMvFloatId ms_WalkPhase;
	static const fwMvFloatId ms_TrotPhase;
	static const fwMvFloatId ms_CanterPhase;
	static const fwMvFloatId ms_GallopPhase;
	static const fwMvFloatId ms_TurnPhase;
	static const fwMvFloatId ms_MoveSlopePhaseId;

public:
	void RequestQuickStop() {m_bQuickStopRequested=true;}	

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual void Debug() const;
#endif


private:	
	// initial state
	void						Start_OnEnter			();
	FSM_Return					Start_OnUpdate			();

	// Stream clips	
	FSM_Return					StreamClips_OnUpdate	();

	// Sit and steer
	void						SitIdle_OnEnter			();
	FSM_Return					SitIdle_OnUpdate		();

	// Moving
	void						Locomotion_OnEnter		();
	FSM_Return					Locomotion_OnUpdate		();

	// Stop!
	void						Brake_OnEnter			();
	FSM_Return					Brake_OnUpdate			();

	// Stop!
	void						QuickStop_OnEnter		();
	FSM_Return					QuickStop_OnUpdate		();
		
	// Flags
	enum Flags
	{		
		Flag_WeaponClipSetChanged					= BIT0,
		Flag_WeaponFilterChanged					= BIT1
	};

	// Flags
	fwFlags8 m_Flags;

	void ApplyWeaponClipSet();
	void SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId);
	void SendWeaponClipSet();
	void StoreWeaponFilter();

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper			m_moveNetworkHelper;	
	fwClipSetRequestHelper		m_ClipSetRequestHelper;

	// Clip clipSetId to use for synced partial weapon clips
	fwClipSet* m_WeaponClipSet;
	fwMvClipSetId m_WeaponClipSetId;

	// The filter to be used for weapon clips 
	fwMvFilterId m_WeaponFilterId;
	crFrameFilterMultiWeight* m_pWeaponFilter;

	static u32					ms_CameraHash;
	static s32					ms_CameraInterpolationDuration;

	float						m_fTurnPhase;
	bool						m_bQuickStopRequested;
	bool						m_bMoVEInRideGaitsState;
};

//////////////////////////////////////////////////////////////////////////

// PURPOSE:	Enable some debug functionality in CTaskMoveMountedSpurControl.
#define AI_RIDE_DBG	(__DEV)

/*
PURPOSE
	Movement task which is supposed to be used on horses (or other mounted animals)
	with AI riders.	It sits on top of CTaskMoveMounted, is given a desired
	move blend ratio as input, and decides when to spur or brake, based on the
	current speed and the state of the animal.
NOTES
	Adapted from bhPacketExecRide in RDR2.
*/
class CTaskMoveMountedSpurControl : public CTaskMove
{
public:
	enum // States
	{
		State_Start,
		State_Spurring,
		State_Braking,
		State_NoSim,
		State_Finish,

		kNumStates
	};

	CTaskMoveMountedSpurControl();
	virtual ~CTaskMoveMountedSpurControl();

	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL; }

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const { Assertf(false, "CTaskMoveMountedSpurControl : Get target called on a mounted ped" ); return VEC3_ZERO; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { Assertf(false, "CTaskMoveMountedSpurControl : Get target radius called on a mounted ped" ); return 0.f; }

	void SetInputTargetMoveBlendRatio(float tgtMoveBlendRatio)	{	m_InputTargetMoveBlendRatio = tgtMoveBlendRatio;	}
	float GetInputTargetMoveBlendRatio() const					{	return m_InputTargetMoveBlendRatio;	}

	float CalcTargetNormSpeed() const;

#if !__FINAL	
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	virtual void Debug() const;
#endif

protected:
	FSM_Return StateStart_OnEnter();
	FSM_Return StateStart_OnUpdate();
	FSM_Return StateSpurring_OnUpdate();
	FSM_Return StateBraking_OnUpdate();
	FSM_Return StateNoSim_OnUpdate();
	FSM_Return StateFinish_OnUpdate();

	void ApplyInput(bool spur, float brake, float stickY);

	bool ShouldUseHorseSim() const;

#if AI_RIDE_DBG
	void DebugMsgf(int level, const char *fmt, ...) const PRINTF_LIKE_N(3);
#endif

	class Tuning
	{
	public:
		enum AiRideParam
		{
			kAiRideMaxAgitationForSpur,
			kAiRideMinFreshnessForSpur,
			kAiRideMinTimeBetweenSpurs,
			kAiRideNormSpeedSpurThreshold,
			kAiRideNormSpeedStartBrakeThreshold,
			kAiRideNormSpeedStopBrakeThreshold,
			kAiRideStickYProportionalConstant,

			kNumAiRideParams
		};

		Tuning();

		void Reset();

		float Get(AiRideParam p) const
		{	FastAssert(p >= 0 && p < kNumAiRideParams);
			return m_Params[p];
		}

		void Set(AiRideParam p, float value)
		{	FastAssert(p >= 0 && p < kNumAiRideParams);
			m_Params[p] = value;
		}

	protected:
		float	m_Params[kNumAiRideParams];
	};

	Tuning					m_Tuning;

	float					m_InputTargetMoveBlendRatio;

	float					m_CurrentStickY;
	float					m_LastSpurTime;

	u8						m_FramesToDisableBreak; // To fix B* 1466355, where the horse would momentarily stop moving when ownership is transfered.

#if AI_RIDE_DBG
	int						m_DebugLevel;
#endif


	// PURPOSE:	True if we currently tell hrsHorseComponent to brake, and also
	//			allow it to play braking clips. Used for hysteresis
	//			in whether to allow braking clips or not (allowing them
	//			to continue if started).
	//bool					m_FlagClipatingBrake;

	static float			sm_ClipFilterSpurThreshold;
	static float			sm_ClipFilterSpurThresholdRandomness;
	//static float			sm_ClipFilterBrakeThreshold;

	// PURPOSE:	True to enable filtering of spur/brake clips unless there
	//			is a significant difference between the current speed and the
	//			desired speed, as they can otherwise look excessive.
	static bool				sm_ClipFilterEnabled;
};

//////////////////////////////////////////////////////////////////////////

#define __PLAYER_HORSE_ASSISTED_MOVEMENT 1
//Move horse ridden by actor
class CTaskMoveMounted : public CTaskMove
{
public:
	CTaskMoveMounted();
	virtual ~CTaskMoveMounted();

	enum
	{
		State_AcceptingInput,
	};

	enum eJumpRequestType
	{
		eJR_None,
		eJR_Manual,
		eJR_Auto,
	};

	virtual aiTask* Copy() const { return rage_new CTaskMoveMounted; }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_MOUNTED; }

	virtual s32 GetDefaultStateAfterAbort() const { return State_AcceptingInput; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const { Assertf(false, "CTaskMoveMounted : Get target called on a mounted ped"); return VEC3_ZERO; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { Assertf(false, "CTaskMoveMounted : Get target radius called on a mounted ped" ); return 0.f; }

	FSM_Return StateAcceptingInput_OnUpdate(CPed* pPed);

	void ApplyInput(float fDesiredHeading, bool bSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bIsAiming, Vector2& vStick, float fMatchSpeed);
	void ResetInput();
	void RequestJump(eJumpRequestType requestType);
	void RequestReverse();
    
    void RequestSlowdownToStop(float fTimeToHalt);
    bool IsSlowingDownToStop() const { return (m_fSlowdownToStopTime > 0.0f); }
    void CancelSlowdownToStop() { m_fSlowdownToStopTime = 0.0f; }
    void RequestForcedStop(float fTimeToStayHalted);
    bool IsForciblyStopped() const { return (m_fForcedStopTime > 0.0f); }
    void CancelForcedStop() { m_fForcedStopTime = 0.0f; }

	float GetDesiredHeading() const { return m_fDesiredHeading; }
	void SetDesiredHeading(float fDesiredHeading);

#if !__FINAL	
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 state) { Assert(state == State_AcceptingInput); return (state == State_AcceptingInput) ? "State_AcceptingInput" : "invalid state"; }

	virtual void Debug() const;
#endif
	

private:		
#if __PLAYER_HORSE_ASSISTED_MOVEMENT
	CPlayerAssistedMovementControl* m_pAssistedMovementControl;
#endif

	float m_fDesiredHeading;
	float m_fTimeSinceSpur;
	float m_fMatchSpeed;
	float m_fBrake;
	float m_fReverseDelay;
	float m_fBuckDelay;
    float m_fSlowdownToStopTime;
    float m_fForcedStopTime;

	Vector2 m_vStick;
	bool  m_bHardBrake;
	bool  m_bSpurRequested;
	bool  m_bMaintainSpeed;
	bool  m_bIsAiming;
	bool  m_bReverseRequested;

	eJumpRequestType m_eJumpRequestType;
};

class CTaskJumpMounted : public CTaskFSMClone
{
public:
	CTaskJumpMounted();

	enum State
	{
		State_Init = 0,
		State_Launch,
		State_Finish,
	};

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskJumpMounted(); }
	virtual int			GetTaskTypeInternal() const			{ return CTaskTypes::TASK_JUMP_MOUNTED; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; } 

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

protected:
	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	// FSM optional functions
	virtual	void CleanUp();

		//
	// State functions
	//

	// Init
	FSM_Return StateInit_OnEnter();
	FSM_Return StateInit_OnUpdate();
	// Launch (and in-air)
	FSM_Return StateLaunch_OnEnter();
	FSM_Return StateLaunch_OnUpdate();

	//
	// Members
	//
	void SendMoveParams();

	// Current clip set
	fwMvClipSetId		m_clipSetId;
	// Network Helper
	CMoveNetworkHelper	m_networkHelper;	

	//MoVE
	static const fwMvFloatId	ms_MoveSpeedId;
	static const fwMvFlagId		ms_FallingFlagId;
	static const fwMvBooleanId	ms_ExitLandId;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif
};

#endif // ENABLE_HORSE

#endif // INC_TASK_RIDE_HORSE_H
