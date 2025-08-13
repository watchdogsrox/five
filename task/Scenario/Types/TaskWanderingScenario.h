#ifndef INC_TASK_WANDERING_SCENARIO_H
#define INC_TASK_WANDERING_SCENARIO_H

#include "Task/Helpers/PropManagementHelper.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/System/Tuning.h"

//-------------------------------------------------------------------------
// Default wandering scenario
//-------------------------------------------------------------------------
class CTaskWanderingScenario : public CTaskScenario
{
public:
	friend class CClonedTaskWanderingScenarioInfo;
	// PURPOSE:	Tuning parameters for this task.
	struct Tunables : public CTuning
	{
		Tunables();

		float		m_MaxTimeWaitingForBlockingArea;
		float		m_SwitchToNextPointDistWalking;
		float		m_SwitchToNextPointDistJogging;
		float		m_PreferNearWaterSurfaceArrivalRadius;
		u32			m_TimeBetweenBlockingAreaChecksMS;

		PAR_PARSABLE;
	};

	enum WanderingScenarioState
	{
		State_Start,
		State_MovingToWanderScenario,
		State_UsingScenario,
		State_Wandering,
		State_WaitForNextScenario,
		State_WaitForBlockingArea,
		State_Finish,

		kNumStates
	};
	
	enum ActionWhenNotUsingScenario
	{
		None,
		Quit,
		WaitForNextScenario,
	};


	explicit CTaskWanderingScenario( s32 scenarioType );
	~CTaskWanderingScenario();
	virtual aiTask* Copy() const;

	virtual void CleanUp();

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WANDERING_SCENARIO;}

	virtual bool	IsTaskValidForChatting() const		{ return false; }

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	// Basic FSM implementation
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const	{return State_Start;}

#if !__FINAL
	virtual void Debug() const;

	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	////////////////////////////////////////////////////////////////////////////////
	// Clone task implementation for CTaskWanderingScenario
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                IsInScope(const CPed* pPed);

	bool	AreClonesCurrentAndNextScenariosValid();

	mutable s32		m_iCachedCurrWorldScenarioPointIndex;	// Used for create clone queriable update
	mutable s32		m_iCachedNextWorldScenarioPointIndex;	// Used for create clone queriable update
	void ClearCachedCurrAndNextWorldScenarioPointIndex() { m_iCachedCurrWorldScenarioPointIndex = -1; m_iCachedNextWorldScenarioPointIndex = -1; }
	////////////////////////////////////////////////////////////////////////////////

	void SetNextScenario(CScenarioPoint* pScenarioPoint, int scenarioType, float overrideMoveBlendRatio);
	bool GotNextScenario() const
	{	return m_NextScenario != NULL;	}

	// PURPOSE: End the current scenario task
	void EndActiveScenarioTask();

	void SetQuitWhenNotUsingScenario()
	{	m_ActionWhenNotUsingScenario = Quit;	}
	
	void SetWaitForNextScenarioWhenNotUsingScenario()
	{	m_ActionWhenNotUsingScenario = WaitForNextScenario;	}

	virtual u32				GetAmbientFlags();

	// Scenario accessors
	virtual CScenarioPoint*	GetCurrentScenarioPoint() const			{ return m_CurrentScenario;	}
	int GetCurrentScenarioType() const								{ return m_CurrentScenarioType; }

	// PURPOSE:	Read the WanderScenarioToUseAfter parameter for a given scenario type and for a given ped,
	//			if applicable.
	// PARAMS:	realScenarioType	- The scenario type currently being used.
	//			ped					- The ped using the scenario, or NULL (used for picking real scenarios from a virtual group).
	// RETURNS:	The real scenario type index of a wander (or jogging) scenario, or -1.
	static int GetWanderScenarioToUseNext(int realScenarioType, CPed* pPed);

	// Gets the prop management helper
	virtual CPropManagementHelper* GetPropHelper() { return m_pPropHelper; }

	// PURPOSE:	Check if this task has detected that one of the points involved is inside a blocking area.
	bool IsBlockedByArea() const { return m_CurrentPositionIsBlocked | m_CurrentScenarioIsBlocked | m_NextScenarioIsBlocked; }

protected:
	void Start_OnEnter();
	FSM_Return Start_OnUpdate();

	void MovingToWanderScenario_OnEnter();
	FSM_Return MovingToWanderScenario_OnUpdate();
	void MovingToWanderScenario_OnExit();

	void UsingScenario_OnEnter();
	FSM_Return UsingScenario_OnUpdate();

	void Wandering_OnEnter();
	FSM_Return Wandering_OnUpdate();
	
	void WaitForNextScenario_OnEnter();
	FSM_Return WaitForNextScenario_OnUpdate();

	void WaitForBlockingArea_OnEnter();
	FSM_Return WaitForBlockingArea_OnUpdate();

	CTask* CreateTaskToMoveTo(Vec3V_ConstRef dest, float overrideMoveBlendRatio);
	float GetMoveBlendRatio(float overrideMoveBlendRatio) const;

	// PURPOSE: Helper to query the m_iScenarioIndex to create and start streaming in a prop.
	void CreatePropIfNecessary();

	// PURPOSE:	The point we're currently heading to in State_MovingToScenario.
	Vec3V		m_CurrentTargetPosV;

	RegdScenarioPnt m_CurrentScenario;
	RegdScenarioPnt m_NextScenario;

	// PURPOSE: Helper to hold on to a prop when CTaskAmbientClips isn't running.
	CPropManagementHelper::SPtr m_pPropHelper;

	// PURPOSE:	The real scenario type associated with m_CurrentScenario.
	int				m_CurrentScenarioType;

	// PURPOSE:	The real scenario type associated with m_NextScenario.
	int				m_NextScenarioType;

	// PURPOSE:	Time stamp for when we last checked for blocking areas, in ms.
	u32				m_BlockingAreaLastCheckTime;

	// PURPOSE:	If set, we have detected that the ped's position is inside of a blocking area.
	bool			m_CurrentPositionIsBlocked;

	// PURPOSE:	If set, we detected m_CurrentScenario as being blocked by a scenario blocking area.
	bool			m_CurrentScenarioIsBlocked;

	// PURPOSE:	If set, we detected m_NextScenario as being blocked by a scenario blocking area.
	bool			m_NextScenarioIsBlocked;

	// PURPOSE:	If set, don't use m_NextScenario in the MovingToWanderScenario and UsingScenario states
	//			instead, use m_CurrentScenario.
	bool			m_UseCurrentScenarioInsteadOfNext;

	float			m_CurrentScenarioOverrideMoveBlendRatio;
	float			m_NextScenarioOverrideMoveBlendRatio;

	ActionWhenNotUsingScenario	m_ActionWhenNotUsingScenario;

	// PURPOSE:	Tuning parameters for the class.
	static Tunables sm_Tunables;
};


// Info for syncing TaskUseScenario  
class CClonedTaskWanderingScenarioInfo : public CClonedScenarioFSMInfo
{
public:
	CClonedTaskWanderingScenarioInfo(	s32 state, 
										s16 scenarioType,
										CScenarioPoint* pScenarioPoint,
										bool bIsWorldScenario,
										int worldScenarioPointIndex,		
										s32 currScenarioPointIndex,
										s32 nextScenarioPointIndex,
										float 	fCurrentScenarioOverrideMBR,
										float	fNextScenarioOverrideMBR);

	CClonedTaskWanderingScenarioInfo();
	~CClonedTaskWanderingScenarioInfo(){}

	virtual s32 GetTaskInfoType() const		{ return INFO_TYPE_WANDER_SCENARIO; }

	CScenarioPoint* GetCurrScenarioPoint();
	CScenarioPoint* GetNextScenarioPoint();
	s32 GetCurrScenarioPointIndex() const { return m_CurrScenarioPointIndex; }
	s32 GetNextScenarioPointIndex() const { return m_NextScenarioPointIndex; }

	u32 GetCurrentScenarioOverrideMBR() const		{ return m_CurrentScenarioOverrideMBR; }
	u32 GetNextScenarioOverrideMBR()	const		{ return m_NextScenarioOverrideMBR; }

	u8		PackMoveBlendRatio(float fMBR);
	float	UnpackMoveBlendRatio(u32 iMoveBlendRatio);

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity* pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedScenarioFSMInfo::Serialise(serialiser);

		bool bHasCurrScenario = m_CurrScenarioPointIndex	!=	-1;
		bool bHasNextScenario = m_NextScenarioPointIndex	!=	-1;

		SERIALISE_BOOL(serialiser, bHasCurrScenario, "has previous scenario pt");
		SERIALISE_BOOL(serialiser, bHasNextScenario, "has next scenario pt");

		if(bHasCurrScenario || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_CurrScenarioPointIndex, SIZEOF_SCENARIO_ID, "Wandering current Scenario id");
		}
		else
		{
			m_CurrScenarioPointIndex	=	-1;
		}

		if(bHasNextScenario || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_NextScenarioPointIndex, SIZEOF_SCENARIO_ID, "Wandering next Scenario id");
		}
		else
		{
			m_NextScenarioPointIndex	=	-1;
		}

		bool bHasCurrOverrideMBR = m_CurrentScenarioOverrideMBR	!=	0;
		bool bHasNextOverrideMBR = m_NextScenarioOverrideMBR	!=	0;

		SERIALISE_BOOL(serialiser, bHasCurrOverrideMBR, "Has current override MBR");
		SERIALISE_BOOL(serialiser, bHasNextOverrideMBR, "Has next override MBR");
		
		if(bHasCurrOverrideMBR || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_CurrentScenarioOverrideMBR,	 SIZEOF_MBR,	"Current override MBR");
		}
		else
		{
			m_CurrentScenarioOverrideMBR	=	0;
		}

		if(bHasNextOverrideMBR || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_NextScenarioOverrideMBR,		 SIZEOF_MBR,	"Next override MBR");
		}
		else
		{
			m_NextScenarioOverrideMBR		=	0;
		}
	}

private:

	static const unsigned int SIZEOF_MBR		= 3;

	s32		m_CurrScenarioPointIndex;		// the index into the world Scenario store ,expected to be a always world scenario point
	s32		m_NextScenarioPointIndex;			// the index into the world Scenario store ,expected to be a always world scenario point

	u8			m_CurrentScenarioOverrideMBR;
	u8			m_NextScenarioOverrideMBR;
};

#endif//!INC_TASK_WANDERING_SCENARIO_H
