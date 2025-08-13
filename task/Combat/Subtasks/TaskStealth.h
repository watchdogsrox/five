// FILE :    TaskStealth.h
// PURPOSE : Subtask of combat used for AI stealth

#ifndef TASK_STEALTH_H
#define TASK_STEALTH_H

// Rage headers
#include "atl/array.h"

// Game headers
#include "scene\RegdRefTypes.h"
#include "Task\System\Task.h"
#include "Task\System\TaskHelpers.h"

class CPed;
class CActionDefinition;

//=========================================================================
// CTaskStealthKill
//=========================================================================

class CTaskStealthKill : public CTask
{
public:
	enum State
	{
		State_Invalid = -1,
		State_Start,
		State_Decide,
		State_Approach,
		State_Streaming,
		State_Kill,
		State_Finish,
	};

	// Melee flags
	enum StealthKillTaskFlags
	{
		SKTF_ForceStealthMode				= BIT0,
	};

	CTaskStealthKill( const CPed* pTarget, const CActionResult* pActionResult, float fDesiredMoveBlendRatio = 1.0f, fwFlags32 iFlags = 0 );
	virtual ~CTaskStealthKill();

#if !__FINAL
	void Debug() const;
	static const char* GetStaticStateName( s32 );
#endif // !__FINAL

	bool		GetIsTaskFlagSet				(u32 flag)	{ return m_nFlags.IsFlagSet( flag ); }
	fwFlags32&	GetTaskFlags					(void)		{ return m_nFlags; }
	float		GetDesiredMoveBlendRatio		(void)		{ return m_fDesiredMoveBlendRatio; }

private:

	void CalculateTargetPosition( CPed* pPed );

	virtual aiTask* Copy() const { return rage_new CTaskStealthKill( m_pTargetPed, m_pActionResult, m_fDesiredMoveBlendRatio, m_nFlags ); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_STEALTH_KILL; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Start; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );

private:

	FSM_Return	Start_OnUpdate( CPed* pPed );
	FSM_Return	Decide_OnUpdate( CPed* pPed );
	void		Approach_OnEnter( void );
	FSM_Return	Approach_OnUpdate( CPed* pPed );
	FSM_Return	Streaming_OnUpdate( CPed* pPed );
	void		Kill_OnEnter( void );
	FSM_Return	Kill_OnUpdate( void );
	
	RegdConstPed			m_pTargetPed;
	RegdConstActionResult	m_pActionResult;

	Vec3V					m_vDesiredWorldPosition;
	float					m_fDesiredHeading;
	float					m_fDesiredMoveBlendRatio;
	fwFlags32				m_nFlags;

	static dev_float		m_fMinKillDistThesholdSq;
	static dev_float		m_fMinApproachDistThreshold;
};

#endif // TASK_STEALTH_H
