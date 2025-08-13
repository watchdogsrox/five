#ifndef TASK_PLAYER_HORSE_FOLLOW_H
#define TASK_PLAYER_HORSE_FOLLOW_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Task/System/Task.h"
#include "Task/System/TaskClassInfo.h"
#include "task/Vehicle/PlayerHorseSpeedMatch.h"
#include "Task/Vehicle/PlayerHorseFollowTargets.h"


// A task for following a companion on horseback.
class CTaskPlayerHorseFollow : public CTask
{
public:
	CTaskPlayerHorseFollow();
	virtual ~CTaskPlayerHorseFollow();

	virtual aiTask* Copy() const { return rage_new CTaskPlayerHorseFollow; }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_PLAYER_HORSE_FOLLOW; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);
	virtual s32 GetDefaultStateAfterAbort()	const { return State_NoTarget; }

	void SetOnlyMatchSpeed(bool bOnlyMatchSpeed)
	{
		m_OnlyMatchSpeed = bOnlyMatchSpeed;
	}

	float GetCurrentSpeedMatchSpeedNormalized() const
	{
		return m_CurrentSpeedMatchSpeedNormalized;
	}

private:
	enum 
	{
		State_NoTarget,
		State_OnlyMatchSpeed,
		State_Finish
	};

	s32 GetDesiredState() const;

	FSM_Return StateNoTarget_OnUpdate();

	FSM_Return StateOnlyMatchSpeed_OnEnter();
	FSM_Return StateOnlyMatchSpeed_OnUpdate();

	FSM_Return StateFinish_OnUpdate();

	const CPhysical* GetLastTarget() const
	{
		return m_LastTarget.target;
	}

	void ResetCurrentSpeedMatchSpeedNormalized()
	{
		m_CurrentSpeedMatchSpeedNormalized = -1.0f;
	}


	bool SelectIdealTarget(CPlayerHorseFollowTargets::FollowTargetEntry& target, bool hadTarget);

	void SetupParamForMoveGoto(CPlayerHorseFollowTargets::FollowTargetEntry& target);

	void ResetLastTarget();

	// Y-offset is negated!  y > 0 means we are behind the target.
	bool HelperShouldFollowAlongSide(float offsAlongXAxis, float offsAlongYAxis, CPlayerHorseFollowTargets::FollowTargetEntry& target);

	// Y-offset is negated!  y > 0 means we are behind the target.
	void UpdateFollowFormation(CPlayerHorseFollowTargets::FollowTargetEntry& target
		, float& offsetXOut, float& offsetYOut, float& offsAlongYAxisOut, float baseOffsetX = 0.0f);

	void SetupSpeed(const CPhysical* target);

	void CreateMoveTask(int iSubTaskType);

	CPed* GetMovementTaskPed();
	const CPed* GetMovementTaskPed() const;
	CTask* GetMovementTask();


	CPlayerHorseFollowTargets::FollowTargetEntry		m_LastTarget;
	CPlayerHorseFollowTargets::FollowTargetEntry		m_CurrentTarget;

	Vector3	m_vNavTarget;
	DEBUG_DRAW_ONLY(Vector3	m_vDesiredNavTarget;)

	float									m_CurrentSpeedMatchSpeedNormalized;
	float									m_fDistTargetAhead;

	bool									m_FollowReady;
	
	bool									m_OnlyMatchSpeed;

	bool									m_WasAlongSideTarget;
	bool									m_WasMatching;

#if !__FINAL
public:
	virtual void Debug() const;

	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif	// #if !__FINAL
};

#endif // ENABLE_HORSE

#endif	// #ifndef TASK_PLAYER_HORSE_FOLLOW_H
