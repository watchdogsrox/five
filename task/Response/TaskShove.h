// FILE :    TaskShove.h

#ifndef TASK_SHOVE_H
#define TASK_SHOVE_H

// Rage headers
#include "fwanimation/boneids.h"

// Game headers
#include "Peds/pedDefines.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskShove
////////////////////////////////////////////////////////////////////////////////

class CTaskShove : public CTask
{

public:

	struct Tunables : CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_Contact;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Rendering m_Rendering;

		float m_MaxDistance;
		float m_MinDot;
		float m_RadiusForContact;
	
		PAR_PARSABLE;
	};
	
public:

	enum ConfigFlags
	{
		CF_Interrupt	= BIT0,
	};

	enum State
	{
		State_Start = 0,
		State_Shove,
		State_Finish,
	};

private:

	enum RunningFlags
	{
		RF_HasMadeContact	= BIT0,
	};
	
public:

	explicit CTaskShove(CPed* pTarget, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskShove();

public:

	static bool IsValid(const CPed& rPed, const CPed& rTarget);
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	bool		CanInterrupt() const;
	fwMvClipId	ChooseClip() const;
	bool		HasMadeContact(RagdollComponent nRagdollComponent) const;
	void		ProcessContacts();
	bool		ShouldInterrupt() const;
	bool		ShouldProcessContacts() const;

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}

	virtual s32	GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_SHOVE;
	}
	
private:

	virtual	void		CleanUp();
	virtual aiTask*		Copy() const;
	virtual bool		ProcessPostPreRender();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Shove_OnEnter();
	FSM_Return	Shove_OnUpdate();
	void		Shove_OnExit();

private:

	RegdPed	m_pTarget;
	fwFlags8		m_uConfigFlags;
	fwFlags8		m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif //TASK_SHOVE_H
