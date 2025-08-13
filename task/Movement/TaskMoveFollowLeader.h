#ifndef TASK_MOVE_FOLLOW_LEADER_H
#define TASK_MOVE_FOLLOW_LEADER_H

#include "Task/Movement/TaskGoTo.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"

class CEntity;
class CPed;
class CPedGroup;

//****************************************************************************
//	CTaskMoveBeInFormation
//	Task to keep peds in formation.  This task should be created as the
//	main movement task, by whichever top-level task controls peds in groups.
//****************************************************************************
class CTaskMoveBeInFormation : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_AttachedToSomething,
		State_FollowingLeader,
		State_Surfacing
	};

	CTaskMoveBeInFormation(CPedGroup * pGroup);
	CTaskMoveBeInFormation(CPed * pNonGroupLeader);
	~CTaskMoveBeInFormation();

	void Init();

	inline void SetUseLargerSearchExtents(const bool b) { m_bUseLargerSearchExtents = b; }
	inline bool GetUseLargerSearchExtents() const { return m_bUseLargerSearchExtents; }

	static const float ms_fXYDiffSqrToSetNewTarget;
	static const float ms_fXYDiffSqrToSetNewTargetWhenWalkingAlongsideLeader;
	static const float ms_fZDiffToSetNewTarget;
	static const float ms_fXYDiffSqrToSwitchToNavMeshRoute;
	static const u32 ms_iTestWalkAlongProbeFreq;
	static const bool ms_bStopPlayerGroupMembersWalkingOverEdges;
	static const float ms_fLeaveInteriorSeekEntityTargetRadiusXY;
	static const bool ms_bHasUnarmedStrafingAnims;

	virtual aiTask* Copy() const
	{
		if(m_bFollowingGroup)
		{
			CTaskMoveBeInFormation * pClone = rage_new CTaskMoveBeInFormation(m_pPedGroup);
			pClone->SetMaxSlopeNavigable( GetMaxSlopeNavigable() );
			return pClone;
		}
		else
		{
			CTaskMoveBeInFormation * pClone = rage_new CTaskMoveBeInFormation(m_pNonGroupLeader);
			pClone->SetNonGroupSpacing(m_fNonGroupSpacing);
			pClone->SetMaxSlopeNavigable( GetMaxSlopeNavigable() );
			return pClone;
		}
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_BE_IN_FORMATION;}

#if !__FINAL
	virtual void Debug() const;
	//virtual atString GetName() const {return atString("MoveInFormation");}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_AttachedToSomething: return "AttachedToSomething";
		case State_FollowingLeader: return "FollowingLeader";
		case State_Surfacing: return "Surfacing";
		}
		return NULL;
	}
#endif

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const;

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return AttachedToSomething_OnEnter(CPed* pPed);
	FSM_Return AttachedToSomething_OnUpdate(CPed* pPed);
	FSM_Return FollowingLeader_OnEnter(CPed* pPed);
	FSM_Return FollowingLeader_OnUpdate(CPed* pPed);
	FSM_Return Surfacing_OnEnter(CPed* pPed);
	FSM_Return Surfacing_OnUpdate(CPed* pPed);

	CTask * CreateSubTask(const int iSubTaskType, CPed* pPed);

	inline void SetNonGroupSpacing(float val)
	{
		Assert(!m_bFollowingGroup);
		m_fNonGroupSpacing = val;
	}
	inline void SetNonGroupWalkAlongsideLeader(bool b)
	{
		Assert(!m_bFollowingGroup);
		m_bNonGroupWalkAlongsideLeader = b;
	}

	inline void SetMaxSlopeNavigable(const float a) { Assert(a >= 0.0f && a <= PI/2.0f); m_fMaxSlopeNavigable = a; }
	inline float GetMaxSlopeNavigable() const { return m_fMaxSlopeNavigable; }

	bool IsInPosition();
	bool IsTooCloseToLeader();
	bool IsLooseFormation();

private:
	// The group in which this ped exists.
	CPedGroup * m_pPedGroup;

	// This task can be overriden to follow a single leader ped.  This ped should not be in the group.
	RegdPed m_pNonGroupLeader;

	// Where the ped is supposed to be wrt the formation
	Vector3 m_vDesiredPosition;
	Vector3 m_vLastTargetPos;	// this is used to stop the ped's idle last continually being interrupted (MaybeInterruptIdleTaskToStartMoving())
	Vector3 m_vStartCrowdingLocation;

	float m_fDesiredHeading;
	float m_fDesiredTargetRadius;
	float m_fStartCrowdingDistFromPlayer;
	float m_fMaxSlopeNavigable;

	// The distance non-group peds will keep apart
	float m_fNonGroupSpacing;
	// The speed of the formation (typically the movespeed of the leader)
	float m_fFormationMoveBlendRatio;
	// The speed with which to move when getting into formation (even if formation is stationary)
	float m_fDesiredMoveBlendRatio;

	float m_fLeadersLastMoveBlendRatio;

	CTaskGameTimer m_SwitchStrafingTimer;

	// These 4 variables are used only when the peds is trying to walk alongside the leader
	Vector3 m_vLastWalkAlongSideLeaderTarget;
	CTaskGameTimer m_TestWalkAlongSidePositionTimer;
	WorldProbe::CShapeTestSingleResult m_hLeaderToTargetProbe;
	WorldProbe::CShapeTestSingleResult m_hTargetToGroundProbe;

	bool m_bFollowingGroup;
	bool m_bSetStrafing;
	bool m_bLeaderHasJustStoppedMoving;
	bool m_bIsTryingToWalkAlongsideLeader;
	// The following 2 flags are used only when the ped is trying to walk alongside the leader
	bool m_bLosFromLeaderToTargetIsClear;
	bool m_bLosToFindGroundIsClear;
	bool m_bNonGroupWalkAlongsideLeader;
	bool m_bUseLargerSearchExtents;

	// Set vPos to be m_vDesiredPosition, adjusted by the leader ped's current move speed
	void GetTargetPosAdjustedForLeaderMovement(Vector3 & vPos);
	// Update the member variables which hold where the ped should be.  Returns false if the ped has been removed from the group.
	bool UpdatePedPositioning(CPed * pPed);
	// If a movement task is running, this function will update its target pos if necessary.  It may also
	// change the subtask type (eg. switching between goto-point & follow navmesh when beyond a certain distance)
	CTask * MaybeUpdateMovementSubTask(CPed * pPed, const Vector3 & vTargetPos);
	// If the formation moves whilst we are idling/turning, then interrupt subtask to create a movement subtask
	CTask * MaybeInterruptIdleTaskToStartMoving(CPed * pPed, const Vector3 & vTargetPos);
	//
	float GetTargetRadiusForNavMeshTask(CPed* pPed);
	bool ArePedsMovingTowardsEachOther(const CPed * pPed, const CPed * pLeader, const float fTestDistance) const;

	void SetStrafingState(CPed * pPed, const Vector3 & vTargetPos);

	float GetMoveBlendRatioToUse(CPed * pPed, const Vector3 & vTargetPos, const float fCurrentMBR, const bool bTaskAlreadyMoving);

	void IssueWalkAlongsideLeaderProbes(const Vector3 & vLeaderPos, const Vector3 & vTargetPos);
	bool RetrieveWalkAlongsideLeaderProbes(const Vector3 & vLeaderPos, const Vector3 & vTargetPos);
	void CancelProbes();
	bool IsPositionClearToWalkAlongsideLeader() const;
	bool NeedsToLeaveInterior(CPed * pPed);
	bool GetCrowdRoundLeaderWhenStationary(CPed * pPed);
	bool GetWalkAlongsideLeaderWhenClose(CPed * pPed);
	bool GetAddLeadersVelocityOntoGotoTarget();

	CPed * GetLeader();
	float GetMinDistToAdjustSpeed();
	float GetMaxDistToAdjustSpeed();
	float GetFormationSpacing();
	float GetFormationTargetRadius(CPed * pPed);
};

class CTaskMoveTrackingEntity : public CTaskMoveBase
{
public:
	static const float ms_fQuitDistance;

	CTaskMoveTrackingEntity(const float fMoveBlend, CEntity * pEntity, const Vector3 & vOffset, bool bOffsetRotatesWithEntity);
	~CTaskMoveTrackingEntity();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMoveTrackingEntity(
			m_fInitialMoveBlendRatio,
			m_pEntity,
			m_vOffset,
			m_bOffsetRotatesWithEntity
		);
	}

#if !__FINAL
	virtual void Debug() const;
#endif

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_TRACKING_ENTITY; }

#if !__FINAL
    virtual atString GetName() const {return atString("MoveTrackingEntity");}
#endif

	virtual bool HasTarget() const { return true; }
	// This function must generate an appropriate target position
	virtual Vector3 GetTarget(void) const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }

	void CalcTarget(void);
	virtual bool ProcessMove(CPed* pPed);

	inline void SetOffsetVector(const Vector3 & vOffset) { m_vOffset = vOffset; }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	RegdEnt m_pEntity;
	Vector3 m_vOffset;
	Vector3 m_vCalculatedTarget;
	float m_fBlend;
	// When first starting to move from a standstill - there must be a 1 second delay during which
	// the moveBlend remains at 1.0f  During this period the idle anim is automatically blended out.
	float m_fWalkDelay;

	u8 m_bOffsetRotatesWithEntity		:1;
	u8 m_bAborting						:1;

	virtual const Vector3 & GetCurrentTarget() { return m_vCalculatedTarget; }
};



#endif	// TASK_MOVE_FOLLOW_LEADER_H
