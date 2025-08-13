// FILE :    TaskReactToDeadPed.h
// PURPOSE : A task to handle the reactions of peds when they acknowledge the fact a dead ped has been found
// AUTHOR :  Chi-Wai Chiu
// CREATED : 11-03-2009
// TODO:	 Use more clips
//			 Sound, cowardly peds to flee when seeing a dead body
//			 Questioning peds around the crime scene

#ifndef INC_TASK_REACT_TO_DEAD_PED_H
#define INC_TASK_REACT_TO_DEAD_PED_H

//Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Event/System/EventData.h"

// Class declaration
class CTaskReactToDeadPed : public CTask
{
public:
	// FSM states
	enum 
	{ 
		State_Start = 0, 
		State_ReactToDeadPed,
		State_Flee,
		State_Finish
	};

	// These are the possible responses in order to generate the appropriate event when task finishes
	typedef enum
	{
		Response_Investigate,			// We haven't seen the enemy
		Response_Threat_Analysis,		// We have seen the enemy or one of our guys is dead someone must have killed him!	
		Response_Flee,					// Cowardly peds flee
		Response_Ignore,						
	} ReactionResponse;

	// Statics
	static const int	ms_iMaxNumNearbyPeds;
	static const float	ms_fMaxQuestionDistance;
	static const u32 ms_iLastReactTimeResponseThreshold;

public:

	CTaskReactToDeadPed (CPed* pDeadPed, bool bWitnessedFirstHand);
	virtual ~CTaskReactToDeadPed();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskReactToDeadPed(m_pDeadPed, m_bWitnessedFirstHand); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_TO_DEAD_PED; }
	
	// FSM required implementations
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort() const { return State_Finish; }

	// Helper functions accessed outside of this class
	void SetThreatPed(CPed* pThreatPed) { m_pThreatPed = pThreatPed; }

	void SetPriorityEvent(const eEventType priority_event) 
	{ 
		if (m_priorityEvent != priority_event)
		{
			m_bNewPriorityEvent = true;
			m_priorityEvent = priority_event;
		}
	}

// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void						Debug() const;
#endif // !__FINAL

protected:
	// State Functions
	void						Start_OnEnter (CPed* pPed);
	FSM_Return					Start_OnUpdate (CPed* pPed);

	void						ReactToDeadPed_OnEnter (CPed* pPed);
	FSM_Return					ReactToDeadPed_OnUpdate (CPed* pPed);

	void						Finish_OnEnter(CPed* pPed);
	void						Finish_OnExit(CPed* pPed);

private:

	void ChooseReactionClip(CPed* pPed, float fDistanceToTarget, float fAngle, float fLeftOrRight);
	void CheckForNewPriorityEvent(CPed* pPed);

private:

	RegdPed					m_pDeadPed;				// The dead ped we are reacting to
	RegdPed					m_pThreatPed;			
	fwMvClipSetId			m_clipSetId;			// Clip dictionary id	
	fwMvClipId				m_clipId;				// Specific clip id
	bool					m_bWitnessedFirstHand;	
	ReactionResponse		m_response;				// Used to store the response
	eEventType				m_priorityEvent;
	bool					m_bNewPriorityEvent;
};

// Class declaration
class CTaskCheckPedIsDead : public CTask
{
public:
	// FSM states
	typedef enum 
	{ 
		State_Start = 0, 
		State_StandAiming,
		State_UseRadio,
		State_GoToDeadPed,
		State_ExamineDeadPed,
		State_Finish
	} CheckState;

	CTaskCheckPedIsDead (CPed* pDeadPed);
	virtual ~CTaskCheckPedIsDead();

	// Task required implementations
	virtual aiTask*				Copy() const { return rage_new CTaskCheckPedIsDead(m_pDeadPed); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_CHECK_PED_IS_DEAD; }

	// FSM required implementations
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort() const { return State_Finish; }

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void						Debug() const;
#endif // !__FINAL

protected:
	// State Functions
	FSM_Return					Start_OnUpdate (CPed* pPed);

	void						StandAiming_OnEnter (CPed* pPed);
	FSM_Return					StandAiming_OnUpdate (CPed* pPed);

	void						UseRadio_OnEnter (CPed* pPed);
	FSM_Return					UseRadio_OnUpdate (CPed* pPed);

	void						GoToDeadPed_OnEnter (CPed* pPed);
	FSM_Return					GoToDeadPed_OnUpdate (CPed* pPed);

	void						ExamineDeadPed_OnEnter (CPed* pPed);
	FSM_Return					ExamineDeadPed_OnUpdate (CPed* pPed);

private:

	RegdPed						m_pDeadPed;				// The dead ped we are reacting to
	CheckState					m_checkState;
};

#endif // INC_TASK_REACT_TO_DEAD_PED_H
