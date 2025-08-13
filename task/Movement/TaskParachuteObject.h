#ifndef __TASK_PARACHUTE_OBJECT_H__
#define __TASK_PARACHUTE_OBJECT_H__

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "network/Debug/NetworkDebug.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"

//////////////////////////////////////////////////////////////////////////
// CTaskParachuteObject
//////////////////////////////////////////////////////////////////////////

class CTaskParachuteObject : public CTaskFSMClone
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_PhaseDuringDeployToConsiderOut;

		PAR_PARSABLE;
	};

	enum ParachuteObjectState
	{
		State_Start,
		State_Stream,
		State_WaitForDeploy,
		State_Deploy,
		State_Idle,
		State_Skydiving,
		State_Crumple, 
		State_Finish
	};

	enum ParachuteObjectRunningFlags
	{
		PORF_CanDeploy	= BIT0,
		PORF_Deploy		= BIT1,
		PORF_IsOut		= BIT2,
		PORF_Crumple	= BIT3,
		PORF_MAX_Flags
	};

	CTaskParachuteObject();
	virtual ~CTaskParachuteObject();

	virtual	void CleanUp();
	virtual aiTask*	Copy() const;
	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_PARACHUTE_OBJECT; }
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

public:

	bool CanDeploy() const;
	void Crumple();
	void Deploy();
	bool IsOut() const;
	void ProcessMoveParameters(float fBlendXAxisTurns, float fBlendYAxisTurns, float fBlendXAndYAxesTurns, float fBlendIdleAndMotion);
	void TaskSetState(const s32 iState);

public:

	static void MakeVisible(CObject& rObject, bool bVisible);

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif

public:

	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool				OverridesNetworkBlender(CPed*);
	virtual bool                OverridesNetworkAttachment(CPed* pPed);
    virtual bool 				IsInScope(const CPed* pPed);

	inline fwFlags8 &			GetFlags() { return m_uFlags; }
	inline const fwFlags8 &		GetFlags() const { return m_uFlags; }

private:

	bool UsesStateFromNetwork();

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	Stream_OnUpdate();
	void		WaitForDeploy_OnEnter();
	FSM_Return	WaitForDeploy_OnUpdate();
	void		Deploy_OnEnter();
	FSM_Return	Deploy_OnUpdate();
	void		Idle_OnEnter();
	FSM_Return	Idle_OnUpdate();
	void		SkyDiving_OnEnter();
	FSM_Return	SkyDiving_OnUpdate();
	void		Crumple_OnEnter();
	FSM_Return	Crumple_OnUpdate();

private:

	void MakeVisible(bool bVisible);
	void ProcessStreaming();
	void ProcessVisibility();

private:

	CMoveNetworkHelper		m_MoveNetworkHelper;
	fwClipSetRequestHelper	m_ClipSetRequestHelper;
	fwFlags8				m_uFlags;
	
private:

	static const fwMvFloatId ms_BlendXAxisTurns;
	static const fwMvFloatId ms_BlendYAxisTurns;
	static const fwMvFloatId ms_BlendXAndYAxesTurns;
	static const fwMvFloatId ms_BlendIdleAndMotion;
	static const fwMvFloatId ms_Phase;
	
	static const fwMvRequestId ms_Deploy;
	static const fwMvRequestId ms_Idle;
	static const fwMvRequestId ms_Crumple;
	
	static const fwMvBooleanId ms_Deploy_OnEnter;
	static const fwMvBooleanId ms_DeployClipEnded;
	static const fwMvBooleanId ms_Idle_OnEnter;
	static const fwMvBooleanId ms_Crumple_OnEnter;
	static const fwMvBooleanId ms_CrumpleClipEnded;
	
private:

	static Tunables ms_Tunables;

	bool m_hasAlreadyDeployed;
};

class CClonedParachuteObjectInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedParachuteObjectInfo()
	{}

	CClonedParachuteObjectInfo(s32 iState)
	{
		m_iState = iState;
	}

	virtual ~CClonedParachuteObjectInfo()
	{}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_PARACHUTE_OBJECT;}
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool HasState() const { return true; }
	virtual u32  GetSizeOfState() const { return datBitsNeeded<CTaskParachuteObject::State_Finish>::COUNT; }
};

#endif
