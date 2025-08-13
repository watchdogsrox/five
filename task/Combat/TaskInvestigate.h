// FILE :    TaskInvestigate.h
// PURPOSE : Used to investigate possible threats
// AUTHOR :  Phil H
// CREATED : 13-10-2008

#ifndef TASK_INVESTIGATE_H
#define TASK_INVESTIGATE_H

// Game headers
#include "Cover/cover.h"
#include "Event/System/EventData.h"
#include "Scene/RegdRefTypes.h"
#include "Task/Combat/TaskSearch.h"
#include "Task/Default/TaskChat.h"
#include "Task/System/TaskHelpers.h"

class CTaskMoveFollowNavMesh;

class CTaskInvestigate : public CTask
{
public:

	// Statics 
	static bank_float ms_fMinDistanceToUseVehicle;
	static bank_float ms_fMinDistanceSavingToUseVehicle;
	static bank_float ms_fTimeToStandAtPerimeter;
	static bool		 ms_bToggleRenderInvestigationPosition;
	static bank_float ms_fNewPositionThreshold;

	static bool ShouldPedClearCorner(const CPed* pPed, CTaskMoveFollowNavMesh* pNavMeshTask, Vector3& vLastCornerCleared, Vector3& vOutStartPos, Vector3& vOutCornerPosition, Vector3& vOutBeyondCornerPosition, bool bCheckWalkingIntoInterior);

	// FSM states
	enum
	{
		State_Start = 0,
		State_WatchInvestigationPed,
		State_GotoInvestigatePosOnFoot,
		State_StandAtPerimeter,
		State_UseRadio,
		State_ChatToPed,
		State_SearchInvestigationPosition,
		State_ReturnToOriginalPositionOnFoot,
		State_EnterNearbyVehicle,
		State_GotoInvestigatePosInVehicle,
		State_ExitVehicleAtPos,
		State_EnterNearbyVehicleToReturn,
		State_ReturnToOriginalPositionInVehicle,
		State_Finish
	};

	struct Tunables : public CTuning
	{
		Tunables();

		s32 m_iTimeToStandAtSearchPoint;
		float m_fMinDistanceToUseVehicle;
		float m_fMinDistanceSavingToUseVehicle;
		float m_fTimeToStandAtPerimeter;
		float m_fNewPositionThreshold;
		PAR_PARSABLE;
	};
private:
	static Tunables sm_Tunables;

public:
	// Constructor/destructor
	CTaskInvestigate(const Vector3& vInvestigatePosition, CPed* pWatchPed = NULL, s32 iInvestigationEventType = EVENT_NONE, s32 iTimeToSpendAtSearchPoint = sm_Tunables.m_iTimeToStandAtSearchPoint, bool bReturnToPosition = true);
	~CTaskInvestigate();

	// Task required functions
	virtual aiTask* Copy() const { return rage_new CTaskInvestigate(m_vInvestigatePosition, m_pWatchPed, m_iInvestigationEventType, m_iTimeToSpendAtSearchPoint, m_bReturnToPositionAfterInvestigate); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_INVESTIGATE; }

	// FSM implementations
	virtual s32	GetDefaultStateAfterAbort()	const { return GetState(); }
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);

	// FSM Optional functions
	virtual	void			CleanUp			(); // Generic cleanup function, called when the task quits itself or is aborted

	// Function which gets  run before UpdateFSM
	FSM_Return				ProcessPreFSM		();

	// Return this task's chat helper
	virtual CChatHelper*	GetChatHelper() { return m_pChatHelper; }

	s32					GetInvestigationEventType() const { return m_iInvestigationEventType; }

	// Hacky - Store the dead ped in watch ped variable
	void					QuitForDeadPedInvestigation(CPed* pDeadPed) { m_pWatchPed = pDeadPed; m_bQuitForDeadPedInvestigation = true; } 

	// The investigation may change depending on new events received or by meeting other guard peds investigating
	void		SetInvestigationPos(const Vector3& vNewPos) { m_bRandomiseInvestigationPosition = false; m_vInvestigatePosition = vNewPos; m_bNewPosition = true; m_newPositionTimer.Reset(); m_bCanSetNewPosition = false; } 
	bool		GetCanSetNewPos() { return m_bCanSetNewPosition; }
	Vector3&	GetInvestigationPos() { return m_vInvestigatePosition; } 
	bool		IsInvestigatingWhistlingEvent() { return m_bInvestigatingWhistlingEvent; }

	// Internal state implementations
	void				Start_OnEnter								(CPed* pPed);
	FSM_Return	Start_OnUpdate							(CPed* pPed);

	void				WatchInvestigationPed_OnEnter				(CPed* pPed);
	FSM_Return	WatchInvestigationPed_OnUpdate			(CPed* pPed);

	void				GotoInvestigatePosOnFoot_OnEnter		(CPed* pPed);
	FSM_Return	GotoInvestigatePosOnFoot_OnUpdate		(CPed* pPed);

	void				StandAtPerimeter_OnEnter				(CPed* pPed);
	FSM_Return	StandAtPerimeter_OnUpdate				(CPed* pPed);

	void				UseRadio_OnEnter						(CPed* pPed);
	FSM_Return	UseRadio_OnUpdate						(CPed* pPed);

	void				ChatToPed_OnEnter							(CPed* pPed);
	FSM_Return	ChatToPed_OnUpdate						(CPed* pPed);

	void				SearchInvestigationPosition_OnEnter		(CPed* pPed);		
	FSM_Return	SearchInvestigationPosition_OnUpdate	(CPed* pPed);

	void				ReturnToOriginalPositionOnFoot_OnEnter	(CPed* pPed);
	FSM_Return	ReturnToOriginalPositionOnFoot_OnUpdate	(CPed* pPed);

	void				EnterNearbyVehicle_OnEnter				(CPed* pPed);
	FSM_Return	EnterNearbyVehicle_OnUpdate				(CPed* pPed);

	void				GotoInvestigatePosInVehicle_OnEnter		(CPed* pPed);
	FSM_Return	GotoInvestigatePosInVehicle_OnUpdate	(CPed* pPed);

	void				ExitVehicleAtPos_OnEnter				(CPed* pPed);
	FSM_Return	ExitVehicleAtPos_OnUpdate				(CPed* pPed);

	void				ReturnToOriginalPositionInVehicle_OnEnter		(CPed* pPed);
	FSM_Return	ReturnToOriginalPositionInVehicle_OnUpdate	(CPed* pPed);

	void				EnterNearbyVehicleToReturn_OnEnter		(CPed* pPed);
	FSM_Return	EnterNearbyVehicleToReturn_OnUpdate		(CPed* pPed);

// debug:
#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:
	// Sets the ped's movement group
	void		SetInvestigatingAnimationGroup				( CPed* pPed );
	bool		ShouldInvestigateOtherEvent(s32 iOtherEvent);

private:

	// Member variables
	Vector3					m_vInvestigatePosition;
	Vector3					m_vInitialPosition;
	Vector3					m_vLastCornerCleared;
	Vector3					m_vVehicleInvestigatePosition;
	RegdPed					m_pWatchPed;
	s32							m_InvestigationState;
	s32							m_iInvestigationEventType;			// Store the event type that triggered the investigation, since if this gets called via the reaction task, peds current event will be react_investigate
	s32							m_iTimeToSpendAtSearchPoint;
	fwClipSetRequestHelper	m_moveGroupClipSetRequestHelper;			// Helper to get the movement clips
	CChatHelper*		m_pChatHelper;
	CTaskTimer			m_standTimer;
	CTaskTimer			m_newPositionTimer;
	u32							m_iPreviousWeaponHash;
	bool						m_bClearingCorner : 1;
	bool						m_bQuitForDeadPedInvestigation : 1;
	bool						m_bNewPosition : 1;
	bool						m_bCanSetNewPosition : 1;
	bool						m_bInvestigatingWhistlingEvent : 1;
	bool						m_bRandomiseInvestigationPosition : 1;
	bool						m_bAllowedToDropCached : 1;
	bool						m_bReturnToPositionAfterInvestigate : 1;
};

// Assumes we know who and where we're going to watch
// Watch out for in vehicle conditions
class CTaskWatchInvestigation : public CTask
{
public:

	enum
	{
		State_Start = 0,
		State_PlayClip,
		State_FollowInvestigationPed,
		State_StandAndWatchInvestigationPed,
		State_ReturnToOriginalPositionOnFoot,
		State_Finish
	};

	static bank_s32 ms_iTimeToStandAndWatchPed; 

	// Constructor/destructor
	CTaskWatchInvestigation(CPed* pWatchPed, const Vector3& vInvestigationPosition);
	~CTaskWatchInvestigation();

	virtual aiTask* Copy() const { return rage_new CTaskWatchInvestigation(m_pWatchPed, m_vInvestigationPosition); }
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_WATCH_INVESTIGATION;}

	// FSM implementations
	FSM_Return		ProcessPreFSM		();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);

	virtual s32	GetDefaultStateAfterAbort()	const {return GetState();}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// Internal state implementations
	FSM_Return	Start_OnUpdate							(CPed* pPed);

	void		GotoInvestigatePosOnFoot_OnEnter		(CPed* pPed);
	FSM_Return	GotoInvestigatePosOnFoot_OnUpdate		(CPed* pPed);

	void		PlayClip_OnEnter						(CPed* pPed);
	FSM_Return	PlayClip_OnUpdate						(CPed* pPed);

	void		FollowInvestigationPed_OnEnter			(CPed* pPed);
	FSM_Return	FollowInvestigationPed_OnUpdate			(CPed* pPed);

	void		StandAndWatchInvestigationPed_OnEnter	(CPed* pPed);
	FSM_Return	StandAndWatchInvestigationPed_OnUpdate	(CPed* pPed);

	void		ReturnToOriginalPositionOnFoot_OnEnter	(CPed* pPed);
	FSM_Return	ReturnToOriginalPositionOnFoot_OnUpdate	(CPed* pPed);

private:

 	Vector3				m_vInitialPosition;
	Vector3				m_vInvestigationPosition;
	RegdPed				m_pWatchPed;
	CTaskGameTimer		m_Timer;
	CAsyncProbeHelper	m_probeHelper;
	bool				m_bCanSeeWatchPed;
};

#endif // TASK_INVESTIGATE_H
