// FILE :    TaskIntimidate.h

#ifndef TASK_INTIMIDATE_H
#define TASK_INTIMIDATE_H

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Game forward declarations
class CEvent;

////////////////////////////////////////////////////////////////////////////////
// CTaskIntimidate
////////////////////////////////////////////////////////////////////////////////

class CTaskIntimidate : public CTask
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		bool m_UseArmsOutForCops;
	
		PAR_PARSABLE;
	};
	
public:

	enum State
	{
		State_Start = 0,
		State_WaitForStreaming,
		State_Intro,
		State_Idle,
		State_IdleUpperBody,
		State_Outro,
		State_Finish,
	};

private:

	enum Mode
	{
		M_None,
		M_1H,
	};

	enum RunningFlags
	{
		RF_HasCreatedWeapon		= BIT0,
		RF_HasDestroyedWeapon	= BIT1,
		RF_PlayOutro			= BIT2,
	};
	
public:

	explicit CTaskIntimidate(const CPed* pTarget);
	virtual ~CTaskIntimidate();

public:

	static bool HasGunInHand(const CPed& rPed);
	static bool IsValid(const CPed& rPed);
	static bool ModifyLoadout(CPed& rPed);
	static bool ShouldDisableMoving(const CPed& rPed);
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	Mode			ChooseMode() const;
	void			CreateWeapon();
	void			DestroyWeapon();
	fwMvClipSetId	GetClipSetForIdle() const;
	fwMvClipSetId	GetClipSetForTransition() const;
	const char*		GetNameForConditionalAnims() const;
	bool			ProcessMode();
	bool			ProcessStreaming();
	bool			ShouldPlayOutroForEvent(const CEvent* pEvent) const;
	bool			ShouldPlayOutroForTaskType(int iTaskType) const;
	bool			ShouldUseUpperBodyIdle() const;

private:

	static Mode				ChooseMode(const CPed& rPed);
	static fwMvClipSetId	GetClipSetForIdle(const CPed& rPed, Mode nMode);
	static fwMvClipSetId	GetClipSetForTransition(const CPed& rPed, Mode nMode);
	static const char*		GetNameForConditionalAnims(const CPed& rPed, Mode nMode);

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_INTIMIDATE;
	}
	
private:

	virtual	void		CleanUp();
	virtual aiTask*		Copy() const;
	virtual FSM_Return	ProcessPreFSM();
	virtual bool		ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	WaitForStreaming_OnUpdate();
	void		Intro_OnEnter();
	FSM_Return	Intro_OnUpdate();
	void		Intro_OnExit();
	void		Idle_OnEnter();
	FSM_Return	Idle_OnUpdate();
	void		IdleUpperBody_OnEnter();
	FSM_Return	IdleUpperBody_OnUpdate();
	void		IdleUpperBody_OnExit();
	void		Outro_OnEnter();
	FSM_Return	Outro_OnUpdate();
	void		Outro_OnExit();

private:

	RegdConstPed						m_pTarget;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelperForTransition;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelperForIdle;
	Mode								m_nMode;
	fwFlags8							m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif //TASK_INTIMIDATE_H
