// FILE :    TaskPatrol.h
// PURPOSE : Patrol and stand guard tasks 
// AUTHOR :  Phil H
// CREATED : 15-10-2008

#ifndef INC_TASK_PATROL_H
#define INC_TASK_PATROL_H

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"

class CPed;
class CPatrolNode;
class CChatHelper;

////////////////////////////
// STAND GUARD			  //
////////////////////////////

// PURPOSE: Ped stands guard at a position, will investigate if an appropriate event is given
//			Can talk to fellow friendly peds
//			Chat procedure:
//			Guards add best chat peds within ped aquaintance scanner.
//			They will choose the best chat ped to initiate a chat with - this sets the m_pChatPed pointer in the other ped's task
//			The guard tasks also scan for new m_pChatPed's

typedef enum
{
	GF_CanChatToPeds = BIT0,
	GF_UseHeadLookAt = BIT1
} GuardBehaviourFlags;

typedef enum
{
	TD_Forward			= BIT0,
	TD_Right			= BIT1,
	TD_Left				= BIT2,
	TD_Backward			= BIT3,
	TD_Forward_Right	= BIT4,
	TD_Forward_Left		= BIT5,
	TD_Backward_Right	= BIT6,
	TD_Backward_Left	= BIT7,
	TD_Max_Directions	= BIT8
} TestDirections;

class CTaskStandGuardFSM : public CTask
{
public:
	// FSM states
	enum
	{
		State_Start = 0,
		State_StandAtPosition,
		State_ReturnToGuardPosition,
		State_ChatToPed,
		State_Finish
	};

	CTaskStandGuardFSM(const Vector3& vPosition, float fHeading, s32 ScenarioType = -1);
	virtual ~CTaskStandGuardFSM();

	// Task required functions
	virtual aiTask*				Copy() const { return rage_new CTaskStandGuardFSM(m_vPosition, m_fHeading, m_ScenarioType); }
	virtual s32				GetTaskTypeInternal() const {return CTaskTypes::TASK_STAND_GUARD_FSM;}

	// FSM required functions
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_StandAtPosition; }

	// FSM Optional functions
	virtual FSM_Return			ProcessPreFSM();
	virtual	void				CleanUp();
	virtual CChatHelper*		GetChatHelper() { return m_pChatHelper; }

	s32					GetLowReactionClipDict() { return m_iLowReactionClipDict; }
	s32					GetHighReactionClipDict() { return m_iHighReactionClipDict; }

	fwFlags32		GetTestDirections() const { return m_iTestDirections; }
	static Vector3	GetDirectionVector(TestDirections probeDirection, const CPed* pPed);
// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual	void Debug( ) const;
#endif

private:
	void			IssueWorldProbe(const Vector3& vPositionToSearchFrom, Vector3& vPositionToEndSearch);

	FSM_Return		Start_OnUpdate					(CPed* pPed);

	void			StandAtPosition_OnEnter			(CPed* pPed);
	FSM_Return		StandAtPosition_OnUpdate		(CPed* pPed);

	void			PlayingScenario_OnEnter			(CPed* pPed);
	FSM_Return		PlayingScenario_OnUpdate		(CPed* pPed);

	void			ReturnToGuardPosition_OnEnter	(CPed* pPed);
	FSM_Return		ReturnToGuardPosition_OnUpdate	(CPed* pPed);

	void			ChatToPed_OnEnter		(CPed* pPed);
	FSM_Return		ChatToPed_OnUpdate		(CPed* pPed);

	CTaskGameTimer  m_checkForNewScenarioTimer; // Check for scenario conditions periodically (maybe just play ambient clips?)
	Vector3			m_vPosition;				// Position to stand guard at
	Vector3			m_vLookAtPoint;				// Point to look at
	float			m_fHeading;					// Heading to turn to
	
	s32				m_ScenarioType;

	static float	ms_fDistanceFromPatrolPositionTolerance;
	// Helper used to handle chatting peds
	CChatHelper*	m_pChatHelper;
	// Helper used to stream in reaction variations
	CReactionClipVariationHelper	m_lowReactionHelper;
	s32								m_iLowReactionClipDict;
	CReactionClipVariationHelper	m_highReactionHelper;
	s32								m_iHighReactionClipDict;
	CTaskTimer					m_alertnessResetTimer;
	bool			m_bTestedForWalls;
	fwFlags32			m_iTestDirections;
	CAsyncProbeHelper	m_probeTestHelper;			// Used to do the line tests
	TestDirections  m_eCurrentProbeDirection;
};

////////////////////////////
// PATROL				  //
////////////////////////////

// PURPOSE: Ped move along a patrol route, stopping at the patrol nodes for a predetermined period of time.
//			Can talk to fellow friendly peds if flag is set, go into an investigation state.

class CTaskPatrol : public CTaskFSMClone
{
public:
	static bank_float ms_fMaxTimeBeforeStartingNewHeadLookAt;
	static bank_float ms_fAlertnessResetTime;		// Decrease alertness by a level after this time

	// FSM states
	typedef enum
	{
		State_Start = 0,
		State_MoveToPatrolNode,
		State_ChatToPed,
		State_StandAtPatrolNode,
		State_PlayScenario,
		State_Finish
	} PatrolState;

	typedef enum 
	{ 
		PD_dontCare = 0, 
		PD_preferForwards, 
		PD_preferBackwards 
	} PatrolDirection;

	typedef enum 
	{ 
		AS_casual = 0, 
		AS_alert,
	} AlertStatus;

public:

	CTaskPatrol( u32 iRouteNameHash, AlertStatus alertStatus, s32 patrolBehaviourFlags = 0);
	virtual ~CTaskPatrol();

	// Task required functions
	virtual aiTask*				Copy() const;
	virtual s32				GetTaskTypeInternal() const {return CTaskTypes::TASK_PATROL;}

	// FSM required functions
	virtual FSM_Return			UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const {return State_MoveToPatrolNode;}

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();
	virtual CChatHelper*		GetChatHelper() { return m_pChatHelper; }

	/// Helper functions
	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void				CleanUp			();
	// Patrol helper functions
	s32						GetTimeLeftAtNode() const;
	s32						GetCurrentNode() const;
	CPatrolNode*				GetCurrentPatrolNode() const	{ return m_pCurrentPatrolNode.Get(); }
	void						SetCurrentPatrolNode(Vector3 pos, u32 routeHash);
	void						MoveToNextPatrolNode();

	s32					GetLowReactionClipGroupId() { return m_iLowReactionClipDictionary; }
	s32					GetHighReactionClipGroupId() { return m_iHighReactionClipDictionary; }

	// returns the full set of flags currently being used
	fwFlags32&	GetFlags() { return m_behaviourFlags; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual void Debug( ) const;
#endif

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

private:
	void						SetState(s32 iState);

protected:
	void					HeadLookAt(CPed* pPed);

	// State functions
	void		Start_OnEnter					(CPed* pPed);
	FSM_Return  Start_OnUpdate					(CPed* pPed);

	void		MoveToPatrolNode_OnEnter		(CPed* pPed);
	FSM_Return  MoveToPatrolNode_OnUpdate		(CPed* pPed);

	void		StandAtPatrolNode_OnEnter		(CPed* pPed);
	FSM_Return  StandAtPatrolNode_OnUpdate		(CPed* pPed);

	void		PlayScenario_OnEnter			(CPed* pPed);
	FSM_Return  PlayScenario_OnUpdate			(CPed* pPed);

	void 		ChatToPed_OnEnter		(CPed* pPed);
	FSM_Return  ChatToPed_OnUpdate		(CPed* pPed);

	// Helper functions
	CPatrolNode* GetNextPatrolNode();
	void		 SetPatrollingAnimationGroup( CPed* pPed );

	// Task member variables
	u32							m_iRouteNameHash;
	RegdPatrolNode				m_pCurrentPatrolNode;
	PatrolDirection				m_patrolDirection;
	float						m_fTimeAtPatrolNode;
	Vector3						m_vLookAtPoint;
	fwFlags32					m_behaviourFlags;
	CTaskTimer					m_headLookAtTimer;			// Timer to hold the last head look at time
	bool						m_bStartedHeadLookAt;
	CTaskTimer					m_alertnessResetTimer;

	// Patrol clip stuff
	AlertStatus					m_eAlertStatus;
	fwClipSetRequestHelper		m_moveGroupClipSetRequestHelper;
	CAmbientClipRequestHelper	m_ambientClipRequestHelper;

	// Helper used to handle chatting peds
	CChatHelper*					m_pChatHelper;
	// Helper used to stream in reaction variations
	CReactionClipVariationHelper	m_lowReactionHelper;
	s32								m_iLowReactionClipDictionary;
	CReactionClipVariationHelper	m_highReactionHelper;
	s32								m_iHighReactionClipDictionary;
};

class CClonesTaskPatrolInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonesTaskPatrolInfo() {}

	CClonesTaskPatrolInfo(s32 iState, u32 iRouteNameHash, Vector3 currentRouteNode, u8 alertStatus, s32 patrolBehaviourFlags)
		: m_iRouteNameHash(iRouteNameHash)
		, m_alertStatus(alertStatus)
		, m_patrolBehaviourFlags(patrolBehaviourFlags)
		, m_currentNodePosition(currentRouteNode)
	{
		SetStatusFromMainTaskState(iState);
	}
	
	~CClonesTaskPatrolInfo() {}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_PATROL; }

	virtual bool			HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskPatrol::State_Finish>::COUNT; }
	virtual const char*	GetStatusName(u32) const { return "State"; }

	virtual CTaskFSMClone*	CreateCloneFSMTask()
	{
		return rage_new CTaskPatrol(m_iRouteNameHash, (CTaskPatrol::AlertStatus)m_alertStatus, m_patrolBehaviourFlags);
	}

	void Serialise(CSyncDataBase& serialiser);

	u32 GetRouteHash()							{ return m_iRouteNameHash; }
	CTaskPatrol::AlertStatus GetAlertStatus()	{ return (CTaskPatrol::AlertStatus)m_alertStatus; }
	s32 GetPatrolBehaviousFlags()				{ return m_patrolBehaviourFlags; }
	Vector3 GetCurrentNodePosition()			{ return m_currentNodePosition; }

private:

	CClonesTaskPatrolInfo(const CClonesTaskPatrolInfo &);
	CClonesTaskPatrolInfo &operator=(const CClonesTaskPatrolInfo &);

private:
	u32		m_iRouteNameHash;
	u8		m_alertStatus;
	s32		m_patrolBehaviourFlags;
	Vector3 m_currentNodePosition;
};

#endif // INC_TASK_PATROL_H
