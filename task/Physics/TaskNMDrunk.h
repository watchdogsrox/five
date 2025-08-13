// Filename   :	TaskNMDrunk.h
// Description:	Code version of original script drunk routine.

#ifndef INC_TASKNMDRUNK_H_
#define INC_TASKNMDRUNK_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "Task/Physics/TaskNM.h"

#if ENABLE_DRUNK

// ------------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMDrunk //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMDrunk : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_fMinHeadingDeltaToFixTurn;
		float m_fHeadingRandomizationRange;
		s32	m_iHeadingRandomizationTimeMin;
		s32	m_iHeadingRandomizationTimeMax;

		float m_fForceLeanInDirectionAmountMax;
		float m_fForceLeanInDirectionAmountMin;

		float m_fForceRampMinSpeed;
		float m_fForceRampMaxSpeed;

		float m_fHeadLookHeadingRandomizationRange;
		float m_fHeadLookPitchRandomizationRange;
		s32	m_iHeadLookRandomizationTimeMin;
		s32	m_iHeadlookRandomizationTimeMax;

		float m_fMinHeadingDeltaToIdleTurn;

		s32	m_iRunningTimeForVelocityBasedStayupright;
		float m_fStayUprightForceNonVelocityBased;
		float m_fStayUprightForceMoving;
		float m_fStayUprightForceIdle;

		float m_fFallingSpeedForHighFall;

		bool m_bDrawIdleHeadLookTarget;
		bool m_bUseStayUpright;

		CNmTuningSet m_Start;
		CNmTuningSet m_Base;
		CNmTuningSet m_Moving;
		CNmTuningSet m_Idle;

		PAR_PARSABLE;
	};

	CTaskNMDrunk(u32 nMinTime, u32 nMaxTime);
	~CTaskNMDrunk();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMDrunk(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_DRUNK;}

	///////////////////////////
	// CTaskNMBehaviour functions
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		taskAssertf(false, "[CTaskNMDrunk::CreateQueriableState() const] This task has not been networked yet.");
		return NULL;
	}

	//////////////////////////
	// Local functions and data
private:
	enum eStates // for internal state machine.
	{
		STANDING,
		STAGGERING,
		TEETERING,
		FALLING,
		ON_GROUND
	};
	eStates m_eState;

	// Tunable parameters: /////////////////
#if __BANK
public:
#else // __BANK
private:
#endif // __BANK

	bool m_bBalanceFailed;
	bool m_bWasInFixedTurnRange;
	bool m_bFixedTurnRight;

	// variables for slightly randomising the target sent to nm.
	u32 m_iNextChangeTime;
	float m_fExtraHeading;
	float m_fExtraPitch;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL

	static Tunables	sm_Tunables;
};

#endif // ENABLE_DRUNK

#endif // ! INC_TASKNMDRUNK_H_
