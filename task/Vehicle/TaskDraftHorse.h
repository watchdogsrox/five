#ifndef INC_TASK_DRAFT_HORSE_H
#define INC_TASK_DRAFT_HORSE_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/motion/TaskMotionBase.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"

//////////////////////////////////////////////////////////////////////////

//// PURPOSE:	Enable some debug functionality in CTaskMoveMountedSpurControl.
//#define AI_RIDE_DBG	(__DEV)
//
///*
//PURPOSE
//	Movement task which is supposed to be used on horses (or other mounted animals)
//	with AI riders.	It sits on top of CTaskMoveMounted, is given a desired
//	move blend ratio as input, and decides when to spur or brake, based on the
//	current speed and the state of the animal.
//NOTES
//	Adapted from bhPacketExecRide in RDR2.
//*/
//class CTaskMoveMountedSpurControl : public CTaskMove
//{
//public:
//	enum // States
//	{
//		State_Start,
//		State_Spurring,
//		State_Braking,
//		State_NoSim,
//		State_Finish,
//
//		kNumStates
//	};
//
//	CTaskMoveMountedSpurControl();
//
//	virtual aiTask* Copy() const;
//
//	virtual int GetTaskType() const { return CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL; }
//
//	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }
//	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
//
//	virtual bool HasTarget() const { return false; }
//	virtual Vector3 GetTarget() const {Assertf(0, "CTaskMoveMountedSpurControl : Get target called on a mounted ped" ); return Vector3(0.0f, 0.0f, 0.0f);}
//	virtual float GetTargetRadius() const {Assertf(0, "CTaskMoveMountedSpurControl : Get target radius called on a mounted ped" ); return 0.0f;}
//
//	void SetInputTargetMoveBlendRatio(float tgtMoveBlendRatio)	{	m_InputTargetMoveBlendRatio = tgtMoveBlendRatio;	}
//	float GetInputTargetMoveBlendRatio() const					{	return m_InputTargetMoveBlendRatio;	}
//
//	float CalcTargetNormSpeed() const;
//
//#if !__FINAL	
//	friend class CTaskClassInfoManager;
//	static const char * GetStaticStateName( s32  );
//#endif
//
//protected:
//	FSM_Return StateStart_OnEnter();
//	FSM_Return StateStart_OnUpdate();
//	FSM_Return StateSpurring_OnUpdate();
//	FSM_Return StateBraking_OnUpdate();
//	FSM_Return StateNoSim_OnUpdate();
//	FSM_Return StateFinish_OnUpdate();
//
//	void ApplyInput(bool spur, float brake, float stickY);
//
//	bool ShouldUseHorseSim() const;
//
//#if AI_RIDE_DBG
//	void DebugMsgf(int level, const char *fmt, ...) const PRINTF_LIKE_N(3);
//#endif
//
//	class Tuning
//	{
//	public:
//		enum AiRideParam
//		{
//			kAiRideMaxAgitationForSpur,
//			kAiRideMinFreshnessForSpur,
//			kAiRideMinTimeBetweenSpurs,
//			kAiRideNormSpeedSpurThreshold,
//			kAiRideNormSpeedStartBrakeThreshold,
//			kAiRideNormSpeedStopBrakeThreshold,
//			kAiRideStickYProportionalConstant,
//
//			kNumAiRideParams
//		};
//
//		Tuning();
//
//		void Reset();
//
//		float Get(AiRideParam p) const
//		{	FastAssert(p >= 0 && p < kNumAiRideParams);
//			return m_Params[p];
//		}
//
//		void Set(AiRideParam p, float value)
//		{	FastAssert(p >= 0 && p < kNumAiRideParams);
//			m_Params[p] = value;
//		}
//
//	protected:
//		float	m_Params[kNumAiRideParams];
//	};
//
//	Tuning					m_Tuning;
//
//	float					m_InputTargetMoveBlendRatio;
//
//	float					m_CurrentStickY;
//	float					m_LastSpurTime;
//
//#if AI_RIDE_DBG
//	int						m_DebugLevel;
//#endif
//
//
//// TODO: Perhaps re-enable this, if needed.
//#if 0
//	// PURPOSE:	True if we currently tell hrsHorseComponent to brake, and also
//	//			allow it to play braking clips. Used for hysteresis
//	//			in whether to allow braking clips or not (allowing them
//	//			to continue if started).
//	bool					m_FlagClipatingBrake;
//
//	static float			sm_ClipFilterSpurThreshold;
//	static float			sm_ClipFilterSpurThresholdRandomness;
//	static float			sm_ClipFilterBrakeThreshold;
//
//	// PURPOSE:	True to enable filtering of spur/brake clips unless there
//	//			is a significant difference between the current speed and the
//	//			desired speed, as they can otherwise look excessive.
//	static bool				sm_ClipFilterEnabled;
//#endif
//};

//////////////////////////////////////////////////////////////////////////

//Move horse ridden by actor
class CTaskMoveDraftAnimal : public CTaskMove
{
public:
	CTaskMoveDraftAnimal();
	~CTaskMoveDraftAnimal();

	enum // States
	{
		AcceptingInput
	};

	virtual aiTask* Copy() const {return rage_new CTaskMoveDraftAnimal();}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_DRAFT_ANIMAL; }

	virtual s32 GetDefaultStateAfterAbort() const { return AcceptingInput; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const {Assertf(0, "CTaskMoveDraftAnimal : Get target called on a mounted ped" ); return Vector3(0.0f, 0.0f, 0.0f);}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const {Assertf(0, "CTaskMoveDraftAnimal : Get target radius called on a mounted ped" ); return 0.0f;}

	FSM_Return StateAcceptingInput_OnUpdate(CPed * pPed);

	void ApplyInput(float fDesiredHeading, bool bSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bIsAiming, Vector2& vStick, float fMatchSpeed);
	void ResetInput();
	void RequestReverse();

	float GetDesiredHeading() const {return m_DesiredHeading;}
	void SetDesiredHeading(float heading) { m_DesiredHeading = heading;}

#if !__FINAL	
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "State_AcceptingInput";  };
#endif
	

private:		
	float m_DesiredHeading;
	float m_fTimeSinceSpur;
	float m_CurrentMatchSpeed;
	float m_fSoftBrake;
	float m_fReverseDelay;

	Vector2 m_Stick;
	bool  m_bHardBrake;
	bool  m_bSpurredThisFrame;
	bool  m_bMaintainSpeed;
	bool  m_bIsAiming;
	bool  m_bReverseRequested;
};

#endif // ENABLE_HORSE

#endif // INC_TASK_DRAFT_HORSE_H
