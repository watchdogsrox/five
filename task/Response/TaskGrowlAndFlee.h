#ifndef TASK_GROWL_AND_FLEE_H
#define TASK_GROWL_AND_FLEE_H

// Game headers
#include "Animation/AnimDefines.h"
#include "Scene/Entity.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

//************************************************
//Class:  CTaskGrowlAndFlee
//Purpose:  Faceoff and retreat task for cougars.
//************************************************

class CTaskGrowlAndFlee : public CTask
{
public:
	
	struct Tunables : public CTuning
	{
		Tunables();

		float		m_FleeMBR;

		PAR_PARSABLE;
	};


	enum
	{
		State_Initial,
		State_Face,
		State_StreamGrowl,
		State_Growl,
		State_Retreat,
		State_Finish
	};
	
public:

	CTaskGrowlAndFlee(CEntity* pTarget);
	~CTaskGrowlAndFlee();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskGrowlAndFlee(m_pTarget); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_GROWL_AND_FLEE; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// FSM State Functions
	FSM_Return					Initial_OnUpdate();

	void						Face_OnEnter();
	FSM_Return					Face_OnUpdate();

	FSM_Return					StreamGrowl_OnUpdate();

	void						Growl_OnEnter();
	FSM_Return					Growl_OnUpdate();

	void						Retreat_OnEnter();
	FSM_Return					Retreat_OnUpdate();

private:

	// Helper functions.


private:

	// Member variables
	RegdEnt								m_pTarget;

	fwMvClipSetId						m_ClipSet;
	fwMvClipId							m_Clip;

	CPrioritizedClipSetRequestHelper	m_AmbientClipSetRequestHelper;


	static Tunables						sm_Tunables;
};

#endif // TASK_GROWL_AND_FLEE_H
