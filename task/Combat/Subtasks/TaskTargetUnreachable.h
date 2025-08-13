// FILE :    TaskTargetUnreachable.h
// PURPOSE : Subtask of combat used for when the target is unreachable

#ifndef TASK_TARGET_UNREACHABLE_H
#define TASK_TARGET_UNREACHABLE_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

//=========================================================================
// CTaskTargetUnreachable
//=========================================================================

class CTaskTargetUnreachable : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_InInterior,
		State_InExterior,
		State_Finish,
	};
	
	enum ConfigFlags
	{
		CF_QuitIfLosClear = BIT0,
		CF_QuitIfRouteFound = BIT1
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fTimeBetweenRouteSearches;

		PAR_PARSABLE;
	};

	CTaskTargetUnreachable(const CAITarget& rTarget, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskTargetUnreachable();

	void SetTarget(const CAITarget& rTarget);

public:

	static bool IsTargetUnreachableUsingNavMesh(const CPed& rTarget);

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskTargetUnreachable(m_Target, m_uConfigFlags); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_TARGET_UNREACHABLE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		InInterior_OnEnter();
	FSM_Return	InInterior_OnUpdate();
	void		InExterior_OnEnter();
	FSM_Return	InExterior_OnUpdate();
	
private:

	Vec3V_Out	GenerateTargetPositionForRouteSearch() const;
	bool		ProcessRouteSearch();
	bool		ShouldQuitDueToLosClear() const;
	
private:

	CAITarget	m_Target;
	fwFlags8	m_uConfigFlags;

	// Route search helper to periodically check if we can reach our target
	CNavmeshRouteSearchHelper	m_routeSearchHelper;

	// The amount of time until our next route search
	float m_fTimeUntilNextRouteSearch;

private:
	
	static Tunables	sm_Tunables;

};

//=========================================================================
// CTaskTargetUnreachableInInterior
//=========================================================================

class CTaskTargetUnreachableInInterior : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_FaceDirection,
		State_Idle,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fDirectionTestProbeLength;

		PAR_PARSABLE;
	};

	CTaskTargetUnreachableInInterior(const CAITarget& rTarget);
	virtual ~CTaskTargetUnreachableInInterior();

	void SetTarget(const CAITarget& rTarget) { m_Target = rTarget; }

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskTargetUnreachableInInterior(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_TARGET_UNREACHABLE_IN_INTERIOR; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	void		FaceDirection_OnEnter();
	FSM_Return	FaceDirection_OnUpdate();
	void		Idle_OnEnter();
	FSM_Return	Idle_OnUpdate();

	/////////////
	// Helpers //
	/////////////

	// Processes the async probes that try to find a valid direction to face
	void		ProcessAsyncProbes();

private:

	CAITarget m_Target;

	// This probe helper is to make sure we aren't facing a wall or other obstruction
	CAsyncProbeHelper	m_asyncProbeHelper;

	// This is the starting direction which we'll add our probe rotation to
	float m_fOriginalDirection;

	// This is our current probe z rotation that we add to our original direction
	float m_fAdditionalProbeRotationZ;

	// The best direction we've found so far
	float m_fBestFacingDirection;

	// The distance to intersection for our best direction
	float m_fBestFacingDistSqToIntersection;

	// If we have a facing direction (we'll transition states when we do)
	bool m_bHasFacingDirection;

private:

	static Tunables	sm_Tunables;

};

//=========================================================================
// CTaskTargetUnreachableInExterior
//=========================================================================

class CTaskTargetUnreachableInExterior : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_UseCover,
		State_FindPosition,
		State_Reposition,
		State_Wait,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_RangePercentage;
		float m_MaxDistanceFromNavMesh;
		float m_TargetRadius;
		float m_MoveBlendRatio;
		float m_CompletionRadius;
		float m_MinTimeToWait;
		float m_MaxTimeToWait;

		PAR_PARSABLE;
	};

	CTaskTargetUnreachableInExterior(const CAITarget& rTarget);
	virtual ~CTaskTargetUnreachableInExterior();

	void SetTarget(const CAITarget& rTarget) { m_Target = rTarget; }

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskTargetUnreachableInExterior(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_TARGET_UNREACHABLE_IN_EXTERIOR; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		UseCover_OnEnter();
	FSM_Return	UseCover_OnUpdate();
	void		FindPosition_OnEnter();
	FSM_Return	FindPosition_OnUpdate();
	void		Reposition_OnEnter();
	FSM_Return	Reposition_OnUpdate();
	void		Wait_OnEnter();
	FSM_Return	Wait_OnUpdate();
	
private:

			CTask*	CreateSubTaskForCombat() const;	
	const	CPed*	GetTarget() const;
			bool	HasValidDesiredCover() const;
			void	ActivateTimeslicing();

private:

	Vec3V							m_vPosition;
	CNavmeshClosestPositionHelper	m_NavmeshClosestPositionHelper;
	CAITarget						m_Target;

private:

	static Tunables	sm_Tunables;

};

#endif // TASK_TARGET_UNREACHABLE_H
