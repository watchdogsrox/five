#ifndef INC_TASK_CHAT_SCENARIO_H
#define INC_TASK_CHAT_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"

class CTaskChatScenario : public CTaskScenario
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_AboutToSayStatement,
		State_AboutToSayResponse,
		State_SayStatement,
		State_SayResponse,
		State_AboutToListenToStatement,
		State_ListeningToStatement,
		State_ListeningToResponse
	} ChatState;
	CTaskChatScenario( s32 scenarioType, CPed* pOtherPed, s32 iChattingContext, CScenarioPoint* pScenarioPoint, ChatState initialChatState, s32 iSubTaskType = CTaskTypes::TASK_USE_SCENARIO );
	~CTaskChatScenario();
	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_CHAT_SCENARIO;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	CPed* GetOtherPed() { return m_pOtherPed; }
	s32 GetSubTaskType() { return m_iSubTaskType; }

	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM		(const s32 iState, const FSM_Event iEvent);

	void Start_OnEnter();
	FSM_Return Start_OnUpdate();
	void Listening_OnUpdate( s32 otherChatState, CPed* pPed );
	void Say_OnUpdate( CPed* pPed );
	void AboutTo_OnUpdate( CPed* pPed );
	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32				GetDefaultStateAfterAbort() const	{return State_AboutToSayStatement;}

	static float TIME_BETWEEN_STATEMENTS_MIN;
	static float TIME_BETWEEN_STATEMENTS_MAX;
	static float TIME_BETWEEN_RESPONSE_MIN;
	static float TIME_BETWEEN_RESPONSE_MAX;

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:
	const char* GetSayString( bool bStatement );
	RegdPed		m_pOtherPed;
	ChatState	m_initialChatState;
	float		m_fSpeechDelay;
	float		m_fListenDelay;
	s32			m_iSubTaskType;
	s32			m_iChattingContext;
	int			m_OtherPedChatTaskState;
	bool		m_WaitForOtherPed;
};

#endif//!INC_TASK_CHAT_SCENARIO_H