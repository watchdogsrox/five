// FILE :    TaskSearch.h
// PURPOSE : AI scour an area based on the last known position of the target and try to re-aquire.
//			 They can search using vehicles, on foot and transition between the two.
// AUTHOR :  Phil H

#ifndef TASK_SEARCH_H
#define TASK_SEARCH_H

// Game headers
#include "ai/AITarget.h"
#include "task/Combat/Subtasks/TaskSearchBase.h"
#include "task/System/Tuning.h"

//=========================================================================
// CTaskSearch
//=========================================================================

class CTaskSearch : public CTaskSearchBase
{

public:

	enum State
	{
		State_Start = 0,
		State_StareAtPosition,
		State_GoToPositionOnFoot,
		State_GoToPositionInVehicle,
		State_PassengerInVehicle,
		State_ExitVehicle,
		State_SearchOnFoot,
		State_SearchInAutomobile,
		State_SearchInHeli,
		State_SearchInBoat,
		State_Finish
	};
	
public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_TimeToStare;
		float m_MoveBlendRatio;
		float m_TargetReached;
		float m_CruiseSpeed;
		
		PAR_PARSABLE;
	};
	
public:

	CTaskSearch(const Params& rParams);
	virtual ~CTaskSearch();
	
public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	bool IsPositionOutsideDefensiveArea(Vec3V_In vPosition) const;
	bool ShouldStareAtPosition() const;

private:

	virtual aiTask* Copy() const { return rage_new CTaskSearch(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SEARCH; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

private:

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void				StareAtPosition_OnEnter();
	FSM_Return	StareAtPosition_OnUpdate();
	void				GoToPositionOnFoot_OnEnter();
	FSM_Return	GoToPositionOnFoot_OnUpdate();
	void				GoToPositionInVehicle_OnEnter();
	FSM_Return	GoToPositionInVehicle_OnUpdate();
	void				PassengerInVehicle_OnEnter();
	FSM_Return	PassengerInVehicle_OnUpdate();
	void				ExitVehicle_OnEnter();
	FSM_Return	ExitVehicle_OnUpdate();
	void				SearchOnFoot_OnEnter();
	FSM_Return	SearchOnFoot_OnUpdate();
	void				SearchInAutomobile_OnEnter();
	FSM_Return	SearchInAutomobile_OnUpdate();
	void				SearchInHeli_OnEnter();
	FSM_Return	SearchInHeli_OnUpdate();
	void				SearchInBoat_OnEnter();
	FSM_Return	SearchInBoat_OnUpdate();

private:

	static Tunables	sm_Tunables;
	
};

// A task used to search for threats that the ped doesn't know from where they came from.
// Peds will actively scan for nearby peds using the acquaintance scanners. If they meet friendly peds
// they will talk to them and inform them of the situation. If they meet unfriendly peds, they will question them.
// If they meet enemy peds, they will shout something like - you killed that guy or, it was you etc and go into combat.
class CCoverPoint;

class CTaskSearchForUnknownThreat : public CTask
{
public:
	// FSM States
	enum
	{
		State_Start = 0,
		State_GoToSearchPosition,
		State_SearchArea,
		State_StandForTime,
		State_QuestionPed,
		State_Finish
	};

	struct Tunables : public CTuning
	{
		Tunables();

		s32 m_iMinTimeBeforeSearchingForNewHidingPlace;
		s32 m_iMaxTimeBeforeSearchingForNewHidingPlace;
		PAR_PARSABLE;
	};

	CTaskSearchForUnknownThreat(const Vector3& vApproachDirection, s32 iEventType, s32 iTimeToSpendAtSearchPoint);
	~CTaskSearchForUnknownThreat();

	virtual aiTask* Copy() const {	return rage_new CTaskSearchForUnknownThreat(m_vApproachDirection, m_iEventType, m_iTimeToStand); }

	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SEARCH_FOR_UNKNOWN_THREAT; }

	// FSM Required Functions
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM Optional Functions
	FSM_Return		ProcessPreFSM();
	
	virtual CChatHelper*	GetChatHelper() { return m_pChatHelper; }

protected:

	// FSM States
	void		Start_OnEnter(CPed* pPed);
	FSM_Return	Start_OnUpdate(CPed* pPed);

	void		SearchArea_OnEnter(CPed* pPed);
	FSM_Return	SearchArea_OnUpdate(CPed* pPed);

	void		GoToSearchPosition_OnEnter(CPed* pPed);
	FSM_Return	GoToSearchPosition_OnUpdate(CPed* pPed);

	void		StandForTime_OnEnter(CPed* pPed);
	FSM_Return	StandForTime_OnUpdate(CPed* pPed);

	void		QuestionPed_OnEnter(CPed* pPed);
	FSM_Return	QuestionPed_OnUpdate(CPed* pPed);

// debug:
#if !__FINAL
	void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:
	CCoverPoint*						m_pHidingPoint;
	Vector3									m_vApproachDirection;
	Vector3									m_vNewLocation;
	s32											m_iTimeSpentStanding;
	s32											m_iTimeToStand;
	s32											m_iEventType;
	CClearanceSearchHelper	m_clearanceSearchHelper;	// A helper to determine the best clear direction from a point
	CAsyncProbeHelper				m_asyncProbeHelper;
	CTaskGameTimer					m_searchPositionTimer;
	fwClipSetRequestHelper	m_moveGroupClipSetRequestHelper;
	CChatHelper*						m_pChatHelper;
	u32											m_iPreviousWeaponHash;

	static Tunables					sm_Tunables;
};

#endif // TASK_SEARCH_H
