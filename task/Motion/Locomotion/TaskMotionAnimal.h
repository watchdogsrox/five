// 
// task/motion/Locomotion/taskmotionanimal.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_MOTION_ANIMAL_H
#define TASK_MOTION_ANIMAL_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "task/motion/TaskMotionBase.h"

class CTaskMotionAnimal : public CTaskMotionBase
{
public:

	enum State
	{
		State_Start,
		State_OnFoot,
		State_Swimming,
		State_SwimToRun,
		State_Exit,
	};

	CTaskMotionAnimal();
	~CTaskMotionAnimal();

	virtual aiTask* Copy() const { return rage_new CTaskMotionAnimal(); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_ANIMAL; }
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Exit; }
	virtual bool	MayDeleteOnAbort() const { return true; }	// This task doesn't resume if it gets aborted, should be safe to delete.

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);	
	FSM_Return		ProcessPostFSM();	

	CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	virtual void CleanUp();

	//	PURPOSE: Should return true if the motion task is in an on foot state
	virtual bool IsOnFoot()	{ return GetState() == State_OnFoot; }
	
	//	PURPOSE: Should return true if the motion task is in a swimming state
	virtual bool IsSwimming() const { return GetState() == State_Swimming; }

	// PURPOSE: Should return true if the motion task is in an in water state
	virtual bool IsInWater()
	{
		if (GetSubTask())
		{
			CTaskMotionBase * pSub = static_cast<CTaskMotionBase *>(GetSubTask());
			return pSub->IsInWater();
		}
		return CTaskMotionBase::IsInWater();
	}

	// PURPOSE: Should return true if the motion task is in an in an underwater state
	virtual bool IsUnderWater() const
	{
		if (GetSubTask())
		{
			const CTaskMotionBase * pSub = static_cast<const CTaskMotionBase *>(GetSubTask());
			return pSub->IsUnderWater();
		}
		return CTaskMotionBase::IsUnderWater();
	}

	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
	{
		if (GetSubTask())
		{
			CTaskMotionBase * pSub = static_cast<CTaskMotionBase *>(GetSubTask());
			return pSub->CalcDesiredVelocity(updatedPedMatrix, fTimestep);
		}
		return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);
	}

	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
	{
		if (GetSubTask())
		{
			CTaskMotionBase * pSub = static_cast<CTaskMotionBase *>(GetSubTask());
			return pSub->GetPitchConstraintLimits(fMinOut, fMaxOut);
		}
		return CTaskMotionBase::GetPitchConstraintLimits(fMinOut, fMaxOut);
	}

	float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL)
	{
		if (GetSubTask())
		{
			CTaskMotionBase * pSub = static_cast<CTaskMotionBase *>(GetSubTask());
			return pSub->GetStoppingDistance(fStopDirection, bOutIsClipActive);
		}
		return CTaskMotionBase::GetStoppingDistance(fStopDirection, bOutIsClipActive);
	}

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	//***********************************************************************************************
	// This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir();

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor();

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSetId, fwMvClipId& outClipId);

	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;
	inline bool SupportsMotionState( CPedMotionStates::eMotionState state )
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
			return true;
		default:
			return false;
		}
	}

private:

	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();

	void						OnFoot_OnEnter();
	FSM_Return					OnFoot_OnUpdate();
	void						OnFoot_OnExit();

	void						Swimming_OnEnter();
	FSM_Return					Swimming_OnUpdate();
	void						Swimming_OnExit();

	void						SwimToRun_OnEnter();
	FSM_Return					SwimToRun_OnUpdate();

	void SetDefaultMotionState();

	CMoveNetworkHelper m_MoveNetworkHelper;

	fwClipSetRequestHelper m_SwimClipSetRequestHelper;

	static const fwMvNetworkId ms_OnFootNetworkId;
	static const fwMvRequestId ms_OnFootId;
	static const fwMvBooleanId ms_OnEnterOnFootId;

	static const fwMvClipSetVarId ms_SwimmingClipSetId;
	static const fwMvNetworkId ms_SwimmingNetworkId;
	static const fwMvRequestId ms_SwimmingId;
	static const fwMvBooleanId ms_OnEnterSwimmingId;

	static const fwMvRequestId ms_SwimToRunId;
	static const fwMvBooleanId ms_OnEnterSwimToRunId;
	static const fwMvFloatId   ms_SwimToRunPhaseId;
	static const fwMvBooleanId ms_TransitionClipFinishedId;
	static const fwMvClipId	   ms_SwimToRunClipId;
};

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif //TASK_MOTION_PED_H
