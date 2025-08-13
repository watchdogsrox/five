#ifndef TASK_MOUNT_FOLLOW_NEAREST_ROAD_H
#define TASK_MOUNT_FOLLOW_NEAREST_ROAD_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

// Rage headers

// Framework headers

// Game headers
#include "Task/System/TaskMove.h"
#include "Task/System/Tuning.h"
#include "Task/Vehicle/FollowNearestPathHelper.h"

// Forward declarations


/*PURPOSE
 *	Wrapper move task to take a ped and have it follow the nearest road network
 *	using the CFollowNearestPathHelper.
 */

class CTaskMountFollowNearestRoad : public CTaskMove  
{ 
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_fCutoffDistForNodeSearch;	
		float	m_fMinDistanceBetweenNodes;
		float	m_fForwardProjectionInSeconds;
		float	m_fDistSqBeforeMovingToNextNode;
		float	m_fDistBeforeMovingToNextNode;
		float	m_fMaxHeadingAdjustDeg;
		float	m_fMaxDistSqFromRoad;
		float	m_fTimeBeforeRestart;
		bool	m_bIgnoreSwitchedOffNodes;
		bool	m_bUseWaterNodes;
		bool	m_bUseOnlyHighwayNodes;
		bool	m_bSearchUpFromPosition; 

		PAR_PARSABLE;
	};

	enum
	{
		State_Find_Route,
		State_Follow_Route,
	};

	CTaskMountFollowNearestRoad();
	virtual ~CTaskMountFollowNearestRoad();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMountFollowNearestRoad();
	}

	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_MOUNT_FOLLOW_NEAREST_ROAD; }

	virtual void CleanUp();

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState)
	{
		switch (iState)
		{
		case State_Find_Route:			return "Find_Route";
		case State_Follow_Route:		return "Follow_Route";
		}
		return NULL;
	}
#endif // __FINAL

	virtual s32 GetDefaultStateAfterAbort() const { return State_Find_Route; }
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Move task interface functions.
	virtual Vector3 GetTarget(void) const;
	virtual bool HasTarget(void) const { return true; }
	virtual float GetTargetRadius(void) const { return 1.0f; }
	virtual Vector3 GetLookAheadTarget(void) const { return GetTarget(); }

	// PURPOSE:	Get the task's tunables.
	static const Tunables& GetTunables() { return sm_Tunables; }

private:
	void		FindRoute_OnEnter();
	FSM_Return	FindRoute_OnUpdate();
	void		FollowRoute_OnEnter();
	FSM_Return	FollowRoute_OnUpdate();

	CFollowNearestPathHelper	m_FollowNearestPathHelper;

	// PURPOSE:	Task tuning data.
	static Tunables		sm_Tunables;
};

#endif // ENABLE_HORSE

#endif // TASK_MOUNT_FOLLOW_NEAREST_ROAD_H