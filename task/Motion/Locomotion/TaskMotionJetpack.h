// 
// task/motion/locomotion/taskmotiojetpack.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_MOTION_JETPACK_H
#define TASK_MOTION_JETPACK_H

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Movement/JetpackObject.h"

#if ENABLE_JETPACK

class CTaskMotionJetpack : public CTaskMotionBase
{
public:
	enum MotionState
	{
		State_Start,
		State_Idle,
		State_Finish
	};

	CTaskMotionJetpack();
	virtual ~CTaskMotionJetpack();
	
	virtual CTask* CreatePlayerControlTask();

	virtual aiTask* Copy() const { return rage_new CTaskMotionJetpack(); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_JETPACK; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
	//Required motion base overrides
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state);
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);
	virtual	bool IsInMotion(const CPed* pPed) const;
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	
	//Optional motion base overrides
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);
	virtual void	GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);
	virtual bool	ShouldStickToFloor();

private:

	void ProcessTask();

private:

	FSM_Return	Start_OnUpdate();
	
	void		Idle_OnEnter(); 
	FSM_Return	Idle_OnUpdate();
	
private:

	RegdTask m_pTask;
	
};

#endif //ENABLE_JETPACK

#endif // TASK_MOTION_JETPACK_H
