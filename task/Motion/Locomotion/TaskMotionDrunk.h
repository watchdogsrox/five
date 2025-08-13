// 
// task/motion/locomotion/taskinwater.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_MOTION_DRUNK_H
#define TASK_MOTION_DRUNK_H

#include "Peds/ped.h"

#if ENABLE_DRUNK

#include "Task/System/Task.h"
#include "Task/Motion/TaskMotionBase.h"


class CTaskMotionDrunk : public CTaskMotionBase
{
public:
	enum MotionState
	{
		State_Start,
		State_NMBehaviour,
		State_Exit
	};

	CTaskMotionDrunk();
	~CTaskMotionDrunk();

	virtual aiTask* Copy() const { return rage_new CTaskMotionDrunk(); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_DRUNK; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const; 

	FSM_Return		ProcessPreFSM	();
	FSM_Return		UpdateFSM		(const s32 iState, const FSM_Event iEvent);	
	void			CleanUp			();

	virtual bool IsInWater() { return false; }

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	//***********************************************************************************************
	// This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir()	{return(false);} // If theres a drop under them, then just fall

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor() { return false; }

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	virtual bool ForceLeafTask() const { return true; }

	inline bool SupportsMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state))
	{
		return false;
	}

	CTask* CreatePlayerControlTask();

	bool IsInMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state)) const {return false;}

	void GetNMBlendOutClip(fwMvClipSetId&  UNUSED_PARAM(outClipSet),  fwMvClipId& UNUSED_PARAM(outClip))
	{
		return ;
	}

private:

	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();
	
	void						NmBehaviour_OnEnter(); 
	FSM_Return					NmBehaviour_OnUpdate();

	CGetupSetRequestHelper		m_GetupReqHelper;
};

#endif // ENABLE_DRUNK

#endif // TASK_MOTION_DRUNK_H