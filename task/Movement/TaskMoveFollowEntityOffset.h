// FILE :    TaskMoveFollowEntityOffset.h
// PURPOSE : Moves a ped to the offset of another entity constantly.
// AUTHOR :  Chi-Wai Chiu
// CREATED : 29-01-2009

#ifndef INC_TASK_FOLLOW_ENTITY_OFFSET_H
#define INC_TASK_FOLLOW_ENTITY_OFFSET_H

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskMove.h"

//---------------------------------------------------------------------------
// This can be used to keep a squad member near a leader ped out of combat.
//---------------------------------------------------------------------------

class CTaskMoveFollowEntityOffset : public CTaskMove
{
public:

	static bank_float  ms_fQuitDistance;		// Distance to quit task 
	static bank_float  ms_fSeekDistance;		// Distance to start seek task
	static bank_float  ms_fYOffsetDeltaWhilstMatching;
	static bank_float  ms_fMinTimeToStayInMatching;

	static bank_float  ms_fwaitBeforeRestartingSeek;
	static bank_float  ms_fSlowdownEps;		// The distance in front of the target position we begin to slow down
	static bank_float  ms_fMatchingEps;		// The zone in which we attempt to match speed with the partner
	static bank_float  ms_fSpeedUpEps;		// If we slip behind the partner by this amount, try to speed up
	static bank_s32  ms_iScanTime;			// Time to rescan for distance check
	static bank_float  ms_fResetTargetDistance;	// Distance to reset the target
	
	static bank_float  ms_fSlowDownDelta;
	static bank_float  ms_fSpeedUpDelta;

	static bank_bool	  ms_bUseWallDetection;

	static bank_float  ms_fLowProbeOffset;
	static bank_float  ms_fHighProbeOffset;
	static bank_float  ms_fProbeDistance;

	static bank_float  ms_fProbeResetTime;

	static bank_float  ms_fDefaultYWalkBehindOffset;
	static bank_float  ms_fDefaultTimeToUseNavMeshIfCatchingUp;
	static bank_float  ms_fDefaultCheckForStuckTime;
	static bank_float  ms_fStuckDistanceTolerance;

	static bank_float  ms_fStartMovingDistance;

	static bank_float  ms_fToleranceWhilstMatching;

	// FSM state
	enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_CatchingUp,
		State_Matching,
		State_SlowingDown,
		State_FollowNavMesh,
		State_IdleAtOffset,
		State_Seeking,
		State_Blocked,
		State_Finish,
	};

	// Probe height
	typedef enum
	{
		Probe_Low = 0,
		Probe_Medium,
		Probe_High,
		Num_Probes
	} eCurrentProbeStatus;

	CTaskMoveFollowEntityOffset(const CEntity* m_pEntityTarget, const float fMoveBlendRatio, const float fTargetRadius, const Vector3& vOffset, int iTimeToFollow, bool bRelativeOffset = true);
	virtual ~CTaskMoveFollowEntityOffset();

	// FSM required implementations
	virtual aiTask*				Copy() const;
	virtual int					GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET;}
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const {return State_Start;}

	virtual FSM_Return			ProcessPreFSM();

	// CTaskMove required implementations
	
	bool						HasTarget() const { return true; }	 // Whether this is a move task which has a target
	Vector3						GetTarget() const;
	Vector3 GetLookAheadTarget() const { return GetTarget(); }
	float						GetTargetRadius() const;
	void						RequestStop() { m_bRequestStop  = true; }

	// Interface functions
	void						SetNewOffset(const Vector3& vOffset) {m_vOffset = vOffset; }
	
	// Getters, Setters
	const bool GetClampMoveBlendRatio() const { return m_bClampMoveBlendRatio; }
	void SetClampMoveBlendRatio(const bool bClamp) { m_bClampMoveBlendRatio = bClamp; }

#if __BANK
	static	void				InitWidgets();
#endif // __BANK

// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual	void				Debug() const;
#endif

protected:

	// State functions
	FSM_Return					Start_OnUpdate			(CPed* pPed); // Initialise variables and set the goto task

	// Behind the matching zone
	FSM_Return					CatchingUp_OnUpdate		(CPed* pPed);  

	// In the matching zone
	FSM_Return					Matching_OnUpdate		(CPed* pPed); 

	// In front of the matching zone
	FSM_Return					SlowingDown_OnUpdate	(CPed* pPed); 

	void						FollowNavMesh_OnEnter	(CPed* pPed); // Dropped behind for some reason, start nav mesh task
	FSM_Return					FollowNavMesh_OnUpdate	(CPed* pPed); //

	void						Seeking_OnEnter			(CPed* pPed);		  // Dropped behind the seek distance
	FSM_Return					Seeking_OnUpdate		(CPed* pPed);	  //

	// Wait until entity moves before moving again
	void						IdleAtOffset_OnEnter	(CPed* pPed);
	FSM_Return					IdleAtOffset_OnUpdate	(CPed* pPed);

	void						Blocked_OnEnter			(CPed* pPed);
	FSM_Return					Blocked_OnUpdate		(CPed* pPed);

private:

	void CheckForStateChange(CPed* pPed);
	void UpdateProbes(CPed* pPed);
	void CalculateWorldPosition();
	void IssueProbe(CPed* pPed);
#if !__FINAL
	void RenderProbeLine(const Vector3& vFrom, const Vector3& vTo, bool bBlocked) const;
#endif // !__FINAL

private:

	RegdConstEnt		m_pEntityTarget;
	float				m_fTargetRadius;
	CTaskGameTimer		m_timer;
	CTaskGameTimer		m_followTimer;
	CTaskTimer			m_waitBeforeRestartingSeekTimer;
	Vector3				m_vWorldSpacePosition;
	Vector3				m_vOffset;
	bool				m_bRelativeOffset;
	int					m_iTimeToFollow;
	Vector3				m_vPreviousPosition;
	CAsyncProbeHelper	m_asyncProbeHelper;
	eCurrentProbeStatus m_eCurrentProbeStatus;
	bool				m_aCurrentProbeResults[Num_Probes];
	CTaskTimer			m_probeTimer;
	bool				m_bFollowingBehind;
	CTaskTimer			m_catchUpTimer;
	bool				m_bAllClear;
	CTaskTimer			m_stuckTimer;
	CTaskTimer			m_matchingTimer;
	bool				m_bRequestStop;
	bool				m_bClampMoveBlendRatio;
};

#if __BANK
class CMoveFollowEntityOffsetDebug
{
public:

	static void InitWidgets();

	static bool ms_bRenderDebug;
};
#endif // __BANK

#endif // INC_TASK_FOLLOW_ENTITY_OFFSET_H
