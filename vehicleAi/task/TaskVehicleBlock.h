#ifndef TASK_VEHICLE_BLOCK_H
#define TASK_VEHICLE_BLOCK_H

// Game headers
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehicleBlock
//=========================================================================

class CTaskVehicleBlock : public CTaskVehicleMissionBase
{

public:

	enum ConfigFlags
	{
		CF_DisableCruiseInFront			= BIT0,
		CF_DisableAvoidanceProjection	= BIT1,
	};

private:

	enum RunningFlags
	{
		RF_IsObstructedByMapGeometry	= BIT0,
		RF_DisableBlockInFront			= BIT1,
		RF_IsOnRoad						= BIT2,
		RF_IsTargetOnRoad				= BIT3,
		RF_IsOnSameRoad					= BIT4,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_DistanceToCapSpeed;
		float	m_DistanceToStartCappingSpeed;
		float	m_AdditionalSpeedCap;
		float	m_MaxDistanceFromTargetToForceStraightLineMode;
		float	m_TimeToLookAhead;
		float	m_MinDistanceToLookAhead;
		float	m_MinDotTargetMovingTowardsUsToStartBackAndForth;
		float	m_MinDotTargetMovingTowardsOurSideToStartBackAndForth;
		float	m_MinDotTargetMovingTowardsUsToContinueBackAndForth;
		float	m_MinDotTargetMovingTowardsUsToStartBrakeInFront;
		float	m_MinDotMovingTowardsTargetToStartBrakeInFront;
		float	m_MinDotTargetMovingTowardsUsToContinueBrakeInFront;
		float	m_MinDotMovingTowardsTargetToContinueBrakeInFront;
		float	m_MinDotTargetMovingTowardsUsToStartCruiseInFront;
		float	m_MinDotMovingAwayFromTargetToStartCruiseInFront;
		float	m_MinDotTargetMovingTowardsUsToContinueCruiseInFront;

		PAR_PARSABLE;
	};

	struct Situation
	{
		ScalarV	m_scDistanceSq;
		ScalarV	m_scDotTargetMovingTowardsUs;
		ScalarV	m_scDotTargetMovingTowardsOurLeftSide;
		ScalarV	m_scDotTargetMovingTowardsOurRightSide;
		ScalarV	m_scDotMovingAwayFromTarget;
		ScalarV	m_scDotMovingTowardsTarget;
		ScalarV	m_scDotDirections;
		Vec3V	m_vTargetVehicleDirection;
	};

	enum VehicleBlockState
	{
		State_Invalid = -1,
		State_Start,
		State_GetInFront,
		State_CruiseInFront,
		State_WaitInFront,
		State_BrakeInFront,
		State_BackAndForth,
		State_Seek,
		State_Finish,
	};

	CTaskVehicleBlock(const sVehicleMissionParams& rParams, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskVehicleBlock();

public:

	fwFlags8&	GetConfigFlags()			{ return m_uConfigFlags; }
	fwFlags8	GetConfigFlags()	const	{ return m_uConfigFlags; }

public:

	bool GetWidthOfRoad(float& fWidth) const
	{
		fWidth = m_fWidthOfRoad;
		return (fWidth >= 0.0f);
	}

public:

	bool IsCruisingInFront() const;
	bool IsGettingInFront() const;

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleBlock>; }

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleBlock(m_Params, m_uConfigFlags); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BLOCK; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		GetInFront_OnEnter();
	FSM_Return	GetInFront_OnUpdate();
	void		GetInFront_OnExit();
	void		CruiseInFront_OnEnter();
	FSM_Return	CruiseInFront_OnUpdate();
	void		WaitInFront_OnEnter();
	FSM_Return	WaitInFront_OnUpdate();
	void		BrakeInFront_OnEnter();
	FSM_Return	BrakeInFront_OnUpdate();
	void		BackAndForth_OnEnter();
	FSM_Return	BackAndForth_OnUpdate();
	void		BackAndForth_OnExit();
	void		Seek_OnEnter();
	FSM_Return	Seek_OnUpdate();

private:

	bool			AreWeMovingAwayFromTarget(float fThreshold) const;
	bool			AreWeMovingTowardsTarget(float fThreshold) const;
	s32				ChooseAppropriateState() const;
	s32				ChooseAppropriateStateForBackAndForth() const;
	s32				ChooseAppropriateStateForBrakeInFront() const;
	const CPed*		GetTargetPed() const;
	const CVehicle*	GetTargetVehicle() const;
	bool			IsBackAndForthAppropriate() const;
	bool			IsBackAndForthNoLongerAppropriate() const;
	bool			IsBrakeInFrontAppropriate() const;
	bool			IsBrakeInFrontNoLongerAppropriate() const;
	bool			IsCruiseInFrontAppropriate() const;
	bool			IsCruiseInFrontNoLongerAppropriate() const;
	bool			IsSeekAppropriate() const;
	bool			IsSeekNoLongerAppropriate() const;
	bool			IsStuck() const;
	bool			IsTargetInRange(float fMinDistance, float fMaxDistance) const;
	bool			IsTargetMovingTowardsOurSide(float fThreshold) const;
	bool			IsTargetMovingTowardsUs(float fThreshold) const;
	bool			IsTargetSpeedAndDistanceAppropriateForBackAndForth() const;
	void			ProcessBrakeInFront();
	void			ProcessProbes();
	void			ProcessRoad();
	void			ProcessSituation();
	bool			ShouldDisableBrakeInFront() const;
	void			UpdateCruiseSpeedForGetInFront(CTaskVehicleMissionBase& rTask);
	void			UpdateStraightLineForGetInFront(CTaskVehicleMissionBase& rTask);
	void			UpdateTargetPositionForGetInFront(CTaskVehicleMissionBase& rTask);
	void			UpdateVehicleMissionForGetInFront();

private:

	Situation							m_Situation;
	WorldProbe::CShapeTestSingleResult	m_ProbeResults;
	float								m_fWidthOfRoad;
	u32									m_uLastTimeProcessedRoad;
	fwFlags8							m_uConfigFlags;
	fwFlags8							m_uRunningFlags;

private:

	static Tunables	sm_Tunables;

};

//=========================================================================
// CTaskVehicleBlockCruiseInFront
//=========================================================================

class CTaskVehicleBlockCruiseInFront : public CTaskVehicleMissionBase
{

private:

	enum RunningFlags
	{
		RF_IsCollisionImminent = BIT0,
	};

public:

	struct Tunables : public CTuning
	{
		struct Probes
		{
			Probes()
			{}

			struct Collision
			{
				Collision()
				{}

				float m_HeightAboveGround;
				float m_SpeedForMinLength;
				float m_SpeedForMaxLength;
				float m_MinLength;
				float m_MaxLength;

				PAR_SIMPLE_PARSABLE;
			};

			Collision m_Collision;

			PAR_SIMPLE_PARSABLE;
		};

		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_Probe;
			bool m_ProbeResults;
			bool m_CollisionReflectionDirection;
			bool m_CollisionReflectedTargetPosition;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Probes		m_Probes;
		Rendering	m_Rendering;

		float	m_StraightLineDistance;
		float	m_TimeToLookAhead;
		float	m_MinDistanceToLookAhead;
		float	m_MinDotForSlowdown;
		float	m_MinDistanceForSlowdown;
		float	m_MaxDistanceForSlowdown;
		float	m_CruiseSpeedMultiplierForMinSlowdown;
		float	m_CruiseSpeedMultiplierForMaxSlowdown;
		float	m_IdealDistance;
		float	m_MinDistanceToAdjustSpeed;
		float	m_MaxDistanceToAdjustSpeed;
		float	m_MinCruiseSpeedMultiplier;
		float	m_MaxCruiseSpeedMultiplier;

		PAR_PARSABLE;
	};

	enum BlockCruiseInFrontState
	{
		State_Start = 0,
		State_Cruise,
		State_Finish,
	};

	CTaskVehicleBlockCruiseInFront(const sVehicleMissionParams& rParams);
	~CTaskVehicleBlockCruiseInFront();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleBlockCruiseInFront(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BLOCK_CRUISE_IN_FRONT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Cruise_OnEnter();
	FSM_Return	Cruise_OnUpdate();

private:

	const CVehicle*	GetTargetVehicle() const;
	void			ProcessProbes();
	void			UpdateTargetPositionAndCruiseSpeed();

private:

	WorldProbe::CShapeTestSingleResult	m_ProbeResults;
	Vec3V								m_vCollisionPosition;
	Vec3V								m_vCollisionNormal;
	fwFlags8							m_uRunningFlags;

private:

	static Tunables	sm_Tunables;

};

//=========================================================================
// CTaskVehicleBlockBrakeInFront
//=========================================================================

class CTaskVehicleBlockBrakeInFront : public CTaskVehicleMissionBase
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_TimeAheadForGetInPosition;
		float	m_MinOffsetForGetInPosition;
		float	m_TimeAheadForBrake;
		float	m_TimeAheadForBrakeOnWideRoads;
		float	m_MaxTimeForBrake;
		float	m_FutureDistanceForMinSteerAngle;
		float	m_FutureDistanceForMaxSteerAngle;
		float	m_MaxSpeedToUseHandBrake;
		float	m_MinDotToClampCruiseSpeed;
		float	m_MaxDistanceToClampCruiseSpeed;
		float	m_MaxCruiseSpeedWhenClamped;

		PAR_PARSABLE;
	};

	enum BlockBrakeInFrontState
	{
		State_Start = 0,
		State_GetInPosition,
		State_Brake,
		State_Finish,
	};

	CTaskVehicleBlockBrakeInFront(const sVehicleMissionParams& rParams);
	~CTaskVehicleBlockBrakeInFront();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleBlockBrakeInFront(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BLOCK_BRAKE_IN_FRONT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		GetInPosition_OnEnter();
	FSM_Return	GetInPosition_OnUpdate();
	void		Brake_OnEnter();
	FSM_Return	Brake_OnUpdate();

private:

	bool			CalculateFutureCollisionDistanceAndDirection(float& fDistance, bool& bIsOnRight) const;
	const CVehicle*	GetTargetVehicle() const;
	bool			GetWidthOfRoad(float& fWidth) const;
	bool			UpdateControlsForBrake();
	void			UpdateVehicleMissionForGetInFront();
	
private:

	CVehControls m_IdealControls;

private:

	static Tunables	sm_Tunables;

};

//=========================================================================
// CTaskVehicleBlockBackAndForth
//=========================================================================

class CTaskVehicleBlockBackAndForth : public CTaskVehicleMissionBase
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_ThrottleMultiplier;

		PAR_PARSABLE;
	};

	enum BlockBackAndForthState
	{
		State_Start = 0,
		State_Block,
		State_Finish,
	};

	CTaskVehicleBlockBackAndForth(const sVehicleMissionParams& rParams);
	~CTaskVehicleBlockBackAndForth();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleBlockBackAndForth(m_Params); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BLOCK_BACK_AND_FORTH; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Block_OnEnter();
	FSM_Return	Block_OnUpdate();

private:

	const CVehicle*	GetTargetVehicle() const;
	void			UpdateIdealControls();

private:

	CVehControls	m_IdealControls;

private:

	static Tunables	sm_Tunables;

};

#endif
