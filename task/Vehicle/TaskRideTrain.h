//
// Task/Vehicle/TaskRideTrain.h
//
// Copyright (C) 2012-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_RIDE_TRAIN_H
#define INC_TASK_RIDE_TRAIN_H

////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "ai/task/taskregdref.h"

// Game Headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"
#include "Vehicles/handlingMgr.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CTrain;

////////////////////////////////////////////////////////////////////////////////

class CTaskRideTrain : public CTask
{
public:

	enum State
	{
		State_Start,
		State_FindScenario,
		State_GetOn,
		State_UseScenario,
		State_Wait,
		State_GetOff,
		State_Finish
	};
	
	enum RunningFlags
	{
		RF_GetOff	= BIT0,
	};
	
public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_MinDelayForGetOff;
		float m_MaxDelayForGetOff;
		float m_fMaxWaitSeconds;
		PAR_PARSABLE;
	};

public:

	CTaskRideTrain(CTrain* pTrain);
	CTaskRideTrain(CTrain* pTrain, CTask* pScenarioTask);
	CTaskRideTrain(const CTaskRideTrain& rOther);
	virtual ~CTaskRideTrain();

	virtual aiTask*	Copy() const { return rage_new CTaskRideTrain(*this); }

	virtual int	GetTaskTypeInternal() const					{ return CTaskTypes::TASK_RIDE_TRAIN; }
	virtual s32	GetDefaultStateAfterAbort() const	{ return GetState(); }
	
	virtual	CScenarioPoint* GetScenarioPoint() const;
	virtual int GetScenarioPointType() const;

	CTrain* GetTrain() const { return m_pTrain; }
	
public:
	
	void GetOff();
	bool IsGettingOff() const;
	bool IsGettingOn() const;

private:

	virtual	void		CleanUp();
	virtual void		DoAbort(const AbortPriority priority, const aiEvent* pEvent);
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return			Start_OnUpdate();
	void				FindScenario_OnEnter();
	FSM_Return			FindScenario_OnUpdate();
	void				GetOn_OnEnter();
	FSM_Return			GetOn_OnUpdate();
	void				UseScenario_OnEnter();
	FSM_Return			UseScenario_OnUpdate();
	void				Wait_OnEnter();
	FSM_Return			Wait_OnUpdate();
	void				GetOff_OnEnter();
	FSM_Return			GetOff_OnUpdate();
	void				GetOff_OnExit();

public:

#if !__FINAL
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	CTask*	CreateScenarioTask(CScenarioPoint* pScenarioPoint, int realScenarioType) const;
	void	ExitScenario();
	bool	FailedToGetOffInTime() const;
	bool	FailedToGetOnInTime() const;
	bool	IsPhysicallyOn() const;
	void	ProcessDelayForGetOff();
	void	ProcessLod();
	bool	ShouldExitScenario() const;
	bool	ShouldGetOff() const;
	bool	ShouldWait() const;

private:

	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

private:

	RegdTrain	m_pTrain;
	RegdaiTask	m_pScenarioTask;
	float		m_fDelayForGetOff;
	fwFlags8	m_uRunningFlags;
	bool		m_bOriginalDontActivateRagdollFromExplosionsFlag;
private:

	static Tunables	sm_Tunables;
};

////////////////////////////////////////////////////////////////////////////////

class CTaskTrainBase : public CTask
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_TargetRadius;
		float m_CompletionRadius;
		float m_SlowDownDistance;

		PAR_PARSABLE;
	};

public:

	CTaskTrainBase(CTrain* pTrain);
	CTaskTrainBase(const CTaskTrainBase& rOther);
	virtual ~CTaskTrainBase();

private:

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	
protected:

	CTask*		CreateNavTask(Vec3V_In vPosition, float fHeading) const;
	Mat34V		GenerateMatrixForBoardingPoint() const;
	void		GeneratePositionAndHeadingToEnter(Vec3V_InOut vPosition, float& fHeading) const;
	void		GeneratePositionAndHeadingToExit(Vec3V_InOut vPosition, float& fHeading) const;
	Vec3V_Out	GeneratePositionToEnter() const;
	Vec3V_Out	GeneratePositionToExit() const;
	bool		LoadBoardingPoint();

public:

#if !__FINAL
	virtual void	Debug() const;
#endif // !__FINAL

protected:

	RegdTrain		m_pTrain;
	CBoardingPoint	m_BoardingPoint;
	
private:

	static Tunables	sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////

class CTaskGetOnTrain : public CTaskTrainBase
{
public:

	enum GetOnTrainState
	{
		State_Start,
		State_MoveToBoardingPoint,
		State_GetOn,
		State_Finish
	};

public:

	CTaskGetOnTrain(CTrain* pTrain);
	CTaskGetOnTrain(const CTaskGetOnTrain& rOther);
	virtual ~CTaskGetOnTrain();

	virtual aiTask*	Copy() const { return rage_new CTaskGetOnTrain(*this); }

	virtual int	GetTaskTypeInternal() const					{ return CTaskTypes::TASK_GET_ON_TRAIN; }
	virtual s32	GetDefaultStateAfterAbort() const	{ return State_Finish; }

private:

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return			Start_OnUpdate();
	void				MoveToBoardingPoint_OnEnter();
	FSM_Return			MoveToBoardingPoint_OnUpdate();
	void				GetOn_OnEnter();
	FSM_Return			GetOn_OnUpdate();

public:

#if !__FINAL
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

};

////////////////////////////////////////////////////////////////////////////////

class CTaskGetOffTrain : public CTaskTrainBase
{
public:

	enum GetOffTrainState
	{
		State_Start,
		State_MoveToBoardingPoint,
		State_GetOff,
		State_Finish
	};

public:

	CTaskGetOffTrain(CTrain* pTrain);
	CTaskGetOffTrain(const CTaskGetOffTrain& rOther);
	virtual ~CTaskGetOffTrain();

	virtual aiTask*	Copy() const { return rage_new CTaskGetOffTrain(*this); }

	virtual int	GetTaskTypeInternal() const					{ return CTaskTypes::TASK_GET_OFF_TRAIN; }
	virtual s32	GetDefaultStateAfterAbort() const	{ return State_Finish; }

private:

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return			Start_OnUpdate();
	void				MoveToBoardingPoint_OnEnter();
	FSM_Return			MoveToBoardingPoint_OnUpdate();
	void				GetOff_OnEnter();
	FSM_Return			GetOff_OnUpdate();

public:

#if !__FINAL
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_TASK_RIDE_TRAIN_H

