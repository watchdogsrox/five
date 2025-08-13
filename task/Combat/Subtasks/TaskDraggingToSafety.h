// FILE :    TaskDraggingToSafety.h
// PURPOSE : Combat subtask in control of dragging a ped to safety
// CREATED : 08-15-20011

#ifndef TASK_DRAGGING_TO_SAFETY_H
#define TASK_DRAGGING_TO_SAFETY_H

// Game headers
#include "physics/WorldProbe/shapetestresults.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CCoverPoint;
class CEventProvidingCover;
class CTaskMoveFollowNavMesh;

////////////////////////////////////////////////////////////////////////////////
// CTaskDraggingToSafety
////////////////////////////////////////////////////////////////////////////////

class CTaskDraggingToSafety : public CTask
{

public:

	enum ConfigFlags
	{
		CF_CoverNotRequired			= BIT0,
		CF_Interrupt				= BIT1,
		CF_DisableTargetTooClose	= BIT2,
	};

private:

	enum RunningFlags
	{
		RF_HasPositionToDragTo			= BIT0,
		RF_IsDraggingToSafety			= BIT1,
		RF_ShouldQuit					= BIT2,
		RF_HasCoverBeenProvided			= BIT3,
		RF_HasHolsteredWeapon			= BIT4,
		RF_HasReachedPositionToDragTo	= BIT5,
		RF_IsObstructed					= BIT6,
	};

public:

	struct Tunables : public CTuning
	{
		struct ObstructionProbe
		{
			ObstructionProbe()
			{}

			float m_Height;
			float m_Radius;
			float m_ExtraHeightForGround;

			PAR_SIMPLE_PARSABLE;
		};

		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_ObstructionProbe;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		ObstructionProbe	m_ObstructionProbe;
		Rendering			m_Rendering;

		float	m_MaxTimeForStream;
		float	m_CoverMinDistance;
		float	m_CoverMaxDistance;
		float	m_LookAtUpdateTime;
		int		m_LookAtTime;
		float	m_CoverWeightDistance;
		float	m_CoverWeightUsage;
		float	m_CoverWeightValue;
		Vector3	m_SeparationPickup;
		Vector3	m_SeparationDrag;
		Vector3	m_SeparationPutdown;
		float	m_AbortAimedAtMinDistance;
		float	m_CoverResponseTimeout;
		float	m_MinDotForPickupDirection;
		float	m_MaxDistanceForHolster;
		float	m_MaxDistanceForPedToBeVeryCloseToCover;
		int		m_MaxNumPedsAllowedToBeVeryCloseToCover;
		float	m_TimeBetweenCoverPointSearches;
		float	m_MinDistanceToSetApproachPosition;
		float	m_MaxDistanceToConsiderTooClose;
		float	m_MaxDistanceToAlwaysLookAtTarget;
		float	m_MaxHeightDifferenceToApproachTarget;
		float	m_MaxXYDistanceToApproachTarget;
		float	m_MaxTimeToBeObstructed;

		PAR_PARSABLE;
	};

public:

	//Note: CTaskDraggedToSafety depends on these states being in a specific order.
	enum DraggingToSafetyState
	{
		State_Start = 0,
		State_Stream,
		State_CallForCover,
		State_Approach,
		State_PickupEntry,
		State_Pickup,
		State_Dragging,
		State_Adjust,
		State_Putdown,
		State_Finish
	};
	
	enum PickupDirection
	{
		PD_Invalid = -1,
		PD_Front,
		PD_Back,
		PD_SideLeft,
		PD_SideRight,
	};
	
public:

	CTaskDraggingToSafety(CPed* pDragged, const CPed* pPrimaryTarget = NULL, fwFlags8 uFlags = 0);
	virtual ~CTaskDraggingToSafety();
	
public:

	PickupDirection GetPickupDirection() const { return m_nPickupDirection; }
	
public:

	void SetMoveBlendRatioForApproach(float fValue) { m_fMoveBlendRatioForApproach = fValue; }

public:

	static Tunables& GetTunables() { return sm_Tunables; }
	
public:

	void DragToPosition(Vec3V_In vPositionToDragTo);

public:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_DRAGGING_TO_SAFETY; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }
	
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	virtual	void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM();
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);
	
#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	CPed* GetDragged() const { return m_pDragged; }
	
	const CPed* GetPrimaryTarget() const { return m_pPrimaryTarget; }
	
	void OnGunAimedAt(CPed* pAimingPed);
	
	void OnProvidingCover(const CEventProvidingCover& rEvent);
	
	static bool	ConvertPedToHighLodRagdoll(CPed* pPed);
	
private:

	FSM_Return	Start_OnUpdate();
	void		Stream_OnEnter();
	FSM_Return	Stream_OnUpdate();
	void		CallForCover_OnEnter();
	FSM_Return	CallForCover_OnUpdate();
	void		Approach_OnEnter();
	FSM_Return	Approach_OnUpdate();
	void		PickupEntry_OnEnter();
	FSM_Return	PickupEntry_OnUpdate();
	void		PickupEntry_OnExit();
	void		Pickup_OnEnter();
	FSM_Return	Pickup_OnUpdate();
	void		Pickup_OnExit();
	void		Dragging_OnEnter();
	FSM_Return	Dragging_OnUpdate();
	void		Adjust_OnEnter();
	FSM_Return	Adjust_OnUpdate();
	void		Putdown_OnEnter();
	FSM_Return	Putdown_OnUpdate();
	void		Putdown_OnExit();
	
private:
	
	bool						ArePedsWithinDistance(const CPed& rPed, const CPed& rOther, float fMaxDistance) const;
	void						CalculateApproachTarget(Vector3& vTargetPosition) const;
	void						CalculateApproachTargetAndHeading(Vector3& vTargetPosition, float& fTargetHeading) const;
	void						CalculateTargetAndHeading(const Vector3& vOffset, Vector3& vTargetPosition, float& fTargetHeading) const;
	bool						CanGeneratePositionToDragTo() const;
	fwMvClipId					ChooseClipForPickupEntry() const;
	fwMvClipId					ChoosePickupClip() const;
	PickupDirection				ChoosePickupDirection() const;
	void						ClearCoverPoint();
	CTask*						CreateCombatTask() const;
	CTask*						CreateMoveTaskForApproach() const;
	bool						DoWeCareAboutObstructions() const;
	void						FindCoverPoint();
	bool						GeneratePositionToDragTo(Vec3V_InOut vPositionToDragTo) const;
	CTaskMoveFollowNavMesh*		GetSubTaskNavMesh();
	bool						HasCoverPoint() const;
	bool						IsCloseEnoughForPickupEntry() const;
	bool						IsCoverPointInvalid() const;
	bool						IsTargetTooClose() const;
	bool						IsWithinReasonableDistanceOfApproachTarget() const;
	bool						NavMeshIsUnableToFindRoute();
	bool						NeedsPositionToDragTo() const;
	void						ProcessCoverPoint();
	void						ProcessLookAt();
	bool						ProcessObstructions();
	void						ProcessProbes();
	bool						ProcessRagdolls();
	void						ProcessStreaming();
	void						ProcessTimers();
	void						SetCoverPoint(CCoverPoint* pCoverPoint);
	bool						ShouldFaceBackwards() const;
	bool						ShouldFindCoverPoint() const;
	void						UpdateHolsterForApproach();
	void						UpdateMoveTaskForApproach();
	
private:
	
	static float	CoverPointScoringCB(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData);
	static bool		CoverPointValidityCB(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData);
	
private:

	Vec3V							m_vPositionToDragTo;
	WorldProbe::CShapeTestResults	m_ObstructionProbeResults;
	fwClipSetRequestHelper			m_ClipSetRequestHelper;
	fwClipSetRequestHelper			m_ClipSetRequestHelperForPickupEntry;
	PickupDirection					m_nPickupDirection;
	CTaskTimer						m_HeadTimer;
	RegdPed							m_pDragged;
	RegdConstPed					m_pPrimaryTarget;
	CCoverPoint*					m_pCoverPoint;
	float							m_fTimeSinceLastCoverPointSearch;
	float							m_fMoveBlendRatioForApproach;
	float							m_fTimeWeHaveBeenObstructed;
	fwFlags8						m_uConfigFlags;
	fwFlags8						m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;
		
};

#endif //TASK_DRAGGING_TO_SAFETY_H
