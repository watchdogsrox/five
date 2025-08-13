// FILE :    TaskAnimatedHitByExplosion.h
// CREATED : 1/25/2013

#ifndef TASK_ANIMATED_HIT_BY_EXPLOSION_H
#define TASK_ANIMATED_HIT_BY_EXPLOSION_H

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

class CTaskAnimatedHitByExplosion : public CTask
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		bool	m_AllowPitchAndRoll;
		float	m_InitialRagdollDelay;

		PAR_PARSABLE;
	};

	enum AnimatedHitByExplosionState
	{
		State_Dive = 0,
		State_GetUp,
		State_Finish
	};

	explicit CTaskAnimatedHitByExplosion(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, float startPhase = 0.0f);
	virtual ~CTaskAnimatedHitByExplosion();

	virtual aiTask*	Copy() const { return rage_new CTaskAnimatedHitByExplosion(m_clipSetId, m_clipId, m_startPhase); }
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_ANIMATED_HIT_BY_EXPLOSION; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual void CleanUp();

#if !__FINAL
	virtual void	Debug() const;
	static const char*		GetStaticStateName(s32 iState);
#endif // !__FINAL

private:

	// States
	void						Dive_OnEnter(CPed* pPed);
	FSM_Return					Dive_OnUpdate(CPed* pPed);

	FSM_Return					GetUp_OnUpdate(CPed* pPed);

	void						FreeMoverCapsule();

private:

	eRagdollBlockingFlagsBitSet	m_ragdollBlockingFlags; //we'll block ragdoll from a few sources for a while at the start of the task

#if __DEV
	Matrix34 m_lastMatrix;
#endif //__DEV

	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	float m_startPhase;

	static Tunables sm_Tunables;

};

#endif //TASK_ANIMATED_HIT_BY_EXPLOSION_H