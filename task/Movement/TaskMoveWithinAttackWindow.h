#ifndef INC_TASK_MOVE_WITHIN_ATTACK_WINDOW_H
#define INC_TASK_MOVE_WITHIN_ATTACK_WINDOW_H

// FILE :    TaskMoveWithinAttackWindow.h
// PURPOSE : Controls a peds movement within their attack window (to cover, using nav mesh, etc)
// CREATED : 21-02-2012

//Game headers
#include "parser/manager.h"
#include "Peds/CombatBehaviour.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////
//   TASK_MOVE_WITHIN_ATTACK_WINDOW   //
////////////////////////////////////////

class CTaskMoveWithinAttackWindow : public CTask
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_Resume,
		State_SearchForCover,
		State_FindTacticalPosition,
		State_FindAdvancePosition,
		State_FindFleePosition,
		State_CheckLosToPosition,
		State_CheckRouteToAdvancePosition,
		State_CheckRouteToFleePosition,
		State_MoveWithNavMesh,
		State_Wait,
		State_Finish
	};

	CTaskMoveWithinAttackWindow(const CPed* pPrimaryTarget);
	virtual ~CTaskMoveWithinAttackWindow();

	virtual void				CleanUp();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskMoveWithinAttackWindow(m_pPrimaryTarget); }
	virtual int					GetTaskTypeInternal() const			{ return CTaskTypes::TASK_MOVE_WITHIN_ATTACK_WINDOW; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Resume; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fMaxAngleOffset;
		float m_fMinAlliesForMaxAngleOffset;
		float m_fMaxAllyDistance;
		float m_fMaxRandomAdditionalOffset;
		float m_fMaxRouteDistanceModifier;
		float m_fMinTimeToWait;
		float m_fMaxTimeToWait;

		PAR_PARSABLE;
	};

	// our tunable
	static Tunables				ms_Tunables;

	////////////////////////////////////
	// Static attack window functions //
	////////////////////////////////////

	// Returns the ideal maximum and minimum attack window boundaries
	static void GetAttackWindowMinMax(const CPed * pPed, float& fMin, float& fMax, bool bUseCoverMax);

	// Checks if the ped/position is outside their attack window
	static bool IsOutsideAttackWindow(const CPed* pPed, const CPed* pTarget, bool bUseCoverMax);
	static bool IsPositionOutsideAttackWindow( const CPed* pPed, const CPed* pTarget, Vec3V_In vPosition, bool bUseCoverMax );
	static bool IsPositionOutsideAttackWindow( const CPed* pPed, const CPed* pTarget, Vec3V_In vPosition, bool bUseCoverMax, bool &bIsTooClose );

private:

	enum
	{
		RF_IsFleeRouteCheck = BIT0,
		RF_FindPositionFailed = BIT1
	};

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate					();
	FSM_Return					Resume_OnUpdate					();
	void						SearchForCover_OnEnter			();
	FSM_Return					SearchForCover_OnUpdate			();
	FSM_Return					FindTacticalPosition_OnUpdate	();
	void						FindGoToPosition_OnEnter		();
	FSM_Return					FindAdvancePosition_OnUpdate	();
	FSM_Return					FindFleePosition_OnUpdate		();
	void						CheckLosToPosition_OnEnter		();
	FSM_Return					CheckLosToPosition_OnUpdate		();
	void						CheckRouteToAdvancePosition_OnEnter	();
	FSM_Return					CheckRouteToAdvancePosition_OnUpdate();
	void						CheckRouteToFleePosition_OnEnter	();
	FSM_Return					CheckRouteToFleePosition_OnUpdate	();
	void						MoveWithNavMesh_OnEnter			();
	FSM_Return					MoveWithNavMesh_OnUpdate		();
	void						MoveWithNavMesh_OnExit			();
	void						Wait_OnEnter					();
	FSM_Return					Wait_OnUpdate					();

	// Helper function
	int							ChooseStateToResumeTo() const;

	// The offset we want to apply to our target position when moving into the attack window
	Vec3V m_vTargetOffset;

	// The distance away we want to be from out target (we check against proximity to this and to the go to position)
	float m_fTargetOffsetDistance;

	// Attack window min/max (able to be stored so we don't have to do a check on them each time)
	float m_fAttackWindowMin;
	float m_fAttackWindowMax;

	// The time we'll wait before checking for another valid position
	float m_fTimeToStayInWait;

	// Current target
	RegdConstPed m_pPrimaryTarget;

	// Have we already searched for cover? (used primarily for going to cover search area before doing the search)
	bool m_bHasSearchedForCover;

	// We need to test LOS from our offset target point to the target itself, making sure we can actually see the target from there
	CAsyncProbeHelper			m_asyncProbeHelper;

	// Route search helper (when using the nav mesh to move to a point we have to make sure we can get there without taking a super long route)
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;

	int							m_iHandleForNavMeshPoint;

	// Some helper running flags
	fwFlags8 m_iRunningFlags;

	// Our possible nav mesh task
	RegdTask m_pFollowNavMeshTask;

	// Static sets defining the attack distances during different states and defined ranges
	enum { CR_Min = 0, CR_Max, CR_NumTypes };
	static float ms_AttackDistances[CCombatBehaviour::CT_NumTypes][CCombatData::CR_NumRanges][CR_NumTypes];
};

#endif // INC_TASK_MOVE_WITHIN_ATTACK_WINDOW_H
