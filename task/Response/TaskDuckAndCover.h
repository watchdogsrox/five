#ifndef TASK_DUCK_FOR_COVER_H_
#define TASK_DUCK_FOR_COVER_H_

#include "Peds/ped.h"
#include "task/system/Task.h"

class CTaskDuckAndCover	: public CTask
{
public:

	CTaskDuckAndCover();
	virtual ~CTaskDuckAndCover();

	enum
	{
		State_Initial,
		State_EnterDuck, 
		State_Ducking,
		State_ExitDuck
	};

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*		Copy() const { return rage_new CTaskDuckAndCover(); }
	virtual s32			GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_DUCK_AND_COVER; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void		CleanUp();

protected:

private:

	FSM_Return			StateInitial_OnEnter();
	FSM_Return			StateInitial_OnUpdate();

	FSM_Return			StateEnterDuck_OnEnter();
	FSM_Return			StateEnterDuck_OnUpdate();

	FSM_Return			StateDucking_OnEnter();
	FSM_Return			StateDucking_OnUpdate();

	FSM_Return			StateExitDuck_OnEnter();
	FSM_Return			StateExitDuck_OnUpdate();

	fwMvClipSetId		m_clipSetId;

	CMoveNetworkHelper	m_networkHelper;	// Loads the parent move network 

	static const fwMvBooleanId	ms_OnEnterDuckId;
	static const fwMvRequestId	ms_EnterDuckCompleteId;
	static const fwMvBooleanId	ms_OnDoingDuckId;
	static const fwMvRequestId	ms_DoingDuckCompleteId;
	static const fwMvBooleanId	ms_OnExitDuckId;
	static const fwMvBooleanId	ms_OnCompletedId;

	static const fwMvBooleanId	ms_InterruptibleId;

};

#endif// TASK_DUCK_FOR_COVER_H_
