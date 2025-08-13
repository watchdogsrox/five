#ifndef TASK_GOTO_VEHICLE_DOOR
#define TASK_GOTO_VEHICLE_DOOR

//************************************************************************************
//	TaskGoToVehicleDoor
//	Handles navigating a ped to the entry-point of a vehicle, ready to start the
//	get-in task.  The main task just wraps the movement task implementation which
//	contains most of the logic - which is done so that peds can potentially do other
//	things whilst getting into a vehicle.  (eg. shooting at enemies, etc)

// Rage includes

// R*N includes
#include "Control/Route.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskHelpers.h"
// Forwards Declarations
class CPed;
class CVehicle;
class CBaseCapsuleInfo;
//************************************************************
//  CTaskGoToCarDoorAndStandStill
//	Main task tree wrapper for the CTaskMoveGoToCarDoor
//	TODO - rename this to CTaskMoveToCarDoor throughout the
//	codebase once the conversion is complete.

class CTaskGoToCarDoorAndStandStill : public CTask
{
public:
	static const float ms_fCombatAlignRadius;
	static const float ms_fBehindTargetRadius;
	static const float ms_fTargetRadius;
	static const float ms_fLargerTargetRadius;
	static const float ms_fSlowDownDistance;
	static const float ms_fMaxSeekDistance;
	static const int ms_iMaxSeekTime;

	CTaskGoToCarDoorAndStandStill(
		CPhysical* pTargetVehicle,
		const float fMoveBlendRatio,
		const bool bIsDriver, 
		const s32 iEntryPoint, 
		const float fTargetRadius=ms_fTargetRadius, 
		const float fSlowDownDistance=ms_fSlowDownDistance, 
		const float fMaxSeekDistance=ms_fMaxSeekDistance, 
		const int iMaxSeekTime=ms_iMaxSeekTime,
		const bool bGetAsCloseAsPossibleIfRouteNotFound=true,
		const CPed* pTargetPed=NULL
	);
	virtual ~CTaskGoToCarDoorAndStandStill();

	virtual aiTask* Copy() const
	{
		CTaskGoToCarDoorAndStandStill * pClone = rage_new CTaskGoToCarDoorAndStandStill(
			m_pTargetVehicle,
			m_fMoveBlendRatio,
			m_bIsDriver,
			m_iEntryPoint,
			m_fTargetRadius,
			m_fSlowDownDistance,
			m_fMaxSeekDistance,
			m_iMaxSeekTime,
			m_bGetAsCloseAsPossibleIfRouteNotFound,
			m_pTargetPed);

		return pClone;
	}

	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL; }

	enum
	{
		State_Initial,
		State_Active,
		State_AutoVault,
	};

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_Active: return "Active";
			case State_AutoVault: return "AutoVault";
			default: { Assert(false); return NULL; }
		}
	}
#endif

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	bool GetTargetDoorPos(CPed * pPed, Vector3 & vOutPosDoor) const;
	inline bool HasAchievedDoor() const { return m_bHasAchievedDoor; }
	Vector3 GetLocalSpaceEntryPointModifier() { return m_vLocalSpaceEntryModifier; }
	inline CPhysical * GetTargetVehicle() const { return m_pTargetVehicle; }
	inline float GetTargetRadius() const { return m_fTargetRadius; }
	inline void ForceSucceed() { m_bForceSucceed = true; }
	inline void ForceFailed() { m_bForceFailed = true; }
	void SetMoveBlendRatio( float fMoveBlendRatio );

	bool HasFailed() const { return m_bForceFailed; }

protected:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateActive_OnEnter(CPed * pPed);
	FSM_Return StateActive_OnUpdate(CPed * pPed);
	FSM_Return StateAutoVault_OnEnter(CPed * pPed);
	FSM_Return StateAutoVault_OnUpdate(CPed * pPed);

	Vector3 m_vLocalSpaceEntryModifier;
	RegdPhy m_pTargetVehicle;
	RegdConstPed m_pTargetPed;
	float m_fMoveBlendRatio;
	float m_fTargetRadius;
	float m_fSlowDownDistance;
	float m_fMaxSeekDistance;
	int m_iMaxSeekTime;
	s32 m_iEntryPoint;
	
	bool m_bIsDriver;
	bool m_bGetAsCloseAsPossibleIfRouteNotFound;
	bool m_bHasAchievedDoor;
	bool m_bForceSucceed;
	bool m_bForceFailed;
};


//*****************************************************************
//	CTaskMoveGoToVehicleDoor
//

class CTaskMoveGoToVehicleDoor : public CTaskMove
{
public:

	static bank_float ms_fMaxRollInv;
	static bank_float ms_fCarMovedEpsSqr;
	static bank_float ms_fCarReorientatedDotEps;
	static bank_float ms_fCarChangedSpeedDelta;
	static bank_float ms_fCheckGoDirectlyToDoorFrequency;
	static bank_bool m_bUpdateGotoTasksEvenIfCarHasNotMoved;

	static bank_float ms_fMinUpdateFreq;
	static bank_float ms_fMaxUpdateFreq;
	static bank_float ms_fMinUpdateDist;
	static bank_float ms_fMaxUpdateDist;

	static bank_float ms_fTighterDistance;
	static bank_bool ms_bUseTighterTurnSettings;
	static bank_bool ms_bUseTighterTurnSettingsInWater;

	static float ComputeTargetRadius(const CPed& ped, const CEntity& ent, const Vector3& vEntryPos, bool bAdjustForMoveSpeed = false, bool bCombatEntry = false);

	CTaskMoveGoToVehicleDoor(
		CPhysical* pTargetVehicle,
		const float fMoveBlendRatio,
		const bool bIsDriver,
		const s32 iEntryPoint,
		const float fTargetRadius,
		const float fSlowDownDistance,
		const float fMaxSeekDistance,
		const int iMaxSeekTime,
		const bool bGetAsCloseAsPossibleIfRouteNotFound
	);

	~CTaskMoveGoToVehicleDoor();

	virtual aiTask* Copy() const
	{
		CTaskMoveGoToVehicleDoor * pClone = rage_new CTaskMoveGoToVehicleDoor(
			m_pTargetVehicle,
			m_fMoveBlendRatio,
			m_bIsDriver,
			m_iEntryPoint,
			m_fTargetRadius,
			m_fSlowDownDistance,
			m_fMaxSeekDistance,
			m_iMaxSeekTime,
			m_bGetAsCloseAsPossibleIfRouteNotFound);

		return pClone;
	}

	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR; }

	enum
	{
		State_Initial,

		State_GoDirectlyToBoardingPoint,
		State_FollowPointRouteToBoardingPoint,
		State_FollowNavMeshToBoardingPoint,

		State_GoToDoor
	};

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_GoDirectlyToBoardingPoint: return "GoDirectlyToBoardingPoint";
			case State_FollowPointRouteToBoardingPoint: return "FollowPointRouteToBoardingPoint";
			case State_FollowNavMeshToBoardingPoint: return "FollowNavMeshToBoardingPoint";
			case State_GoToDoor: return "GoToDoor";
			default: { Assert(false); return NULL; }
		}
	}
#endif

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	bool HasAchievedDoor();
	inline Vector3 GetTargetDoorPos() const { return m_vTargetDoorPos; }
	inline Vector3 GetTargetBoardingPointPos() const { return m_vTargetBoardingPoint; }

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget() const { return m_vTargetDoorPos; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return m_fTargetRadius; }

	CPhysical * GetTargetVehicle() { return m_pTargetVehicle; }

	bool HasFailedToNavigateToVehicle() const { return m_bFailedToNavigateToVehicle; }

private:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateGoDirectlyToBoardingPoint_OnEnter(CPed * pPed);
	FSM_Return StateGoDirectlyToBoardingPoint_OnUpdate(CPed * pPed);
	FSM_Return StateFollowPointRouteToBoardingPoint_OnEnter(CPed * pPed);
	FSM_Return StateFollowPointRouteToBoardingPoint_OnUpdate(CPed * pPed);
	FSM_Return StateFollowNavMeshToBoardingPoint_OnEnter(CPed * pPed);
	FSM_Return StateFollowNavMeshToBoardingPoint_OnUpdate(CPed * pPed);
	
	FSM_Return StateGoToDoor_OnEnter(CPed * pPed);
	FSM_Return StateGoToDoor_OnUpdate(CPed * pPed);

	bool WarpIfPossible();
	bool ShouldQuit(CPed * pPed) const;

	bool IsVehicleInRange(CPed * pPed) const;
	void CalcTargetPositions(const CPed * pPed, Vector3 & vOutTargetDoorPos, Vector3 & vOutTargetBoardingPointPos);
	void AdjustPosForVehicleSpeed(const CPed * pPed, Vector3 & vPos) const;
	bool UseBoardingPointsForVehicle() const;

	int ChooseGoToDoorState(CPed * pPed);
	float ChooseGoToDoorMBR(CPed * pPed);

	bool ShouldNavigateToDoor(CPed * pPed);
	bool HasCarMoved() const;
	bool HasCarChangedSpeed() const;
	bool HasTargetPosChanged();	// Expensive
	float GetUpdateFrequencyMultiplier(CPed * pPed) const;
	void CalcTaskUpdateFrequency(CPed * pPed);
	bool CanGoDirectlyToBoardingPoint(CPed * pPed) const;
	bool IsAlreadyInPositionToEnter(CPed * pPed) const;
	void StringPullStartOfRoute(CPointRoute * pRoute, CPed * pPed) const;
	bool TestIsSegmentClear(const Vector3 & vStartIn, const Vector3 & vEndIn, const CBaseCapsuleInfo * pPedCapsuleInfo, bool bIncludeTargetEntity = true, const float fIgnoreCollisionDistSqrXY=0.0f, s32 * pExcludeComponents = NULL, const float fRadiusExtra=0.0f) const;

	bool ProcessClosingRearDoor(CPed * pPed) const;

	bool IsExcludingAllMapTypes(const CPed *pPed) const;

	bool IsVehicleLandedOnBlimp() const;

	Vector3 m_vTargetDoorPos;
	Vector3 m_vTargetBoardingPoint;
	Vector3 m_vInitialCarPos;
	Vector3 m_vInitialCarXAxis;
	Vector3 m_vInitialCarYAxis;
	Vector3 m_vOffsetModifier;
	RegdPhy m_pTargetVehicle;
	float m_fTargetRadius;
	float m_fSlowDownDistance;
	float m_fMaxSeekDistance;
	int m_iMaxSeekTime;
	s32 m_iEntryPoint;
	float m_fInitialSpeed;
	float m_fLeadersLastMBR;
	CPointRoute * m_pPointRouteRouteToDoor;
	u32 m_iLastTaskUpdateTimeMs;
	float m_fCheckGoDirectlyToDoorTimer;
	s32 m_iCurrentHullDirection;
	s32 m_iChooseStateCounter;
	bool m_bFailedToNavigateToVehicle;
	bool m_bIsDriver;
	bool m_bGetAsCloseAsPossibleIfRouteNotFound;
	bool m_bCanUpdateTaskThisFrame;
	bool m_bCarHasChangedSpeed;
};



#endif // TASK_GOTO_VEHICLE_DOOR
