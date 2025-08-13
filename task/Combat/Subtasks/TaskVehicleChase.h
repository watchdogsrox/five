// FILE :    TaskVehicleChase.h
// PURPOSE : Subtask of combat used to chase a vehicle

#ifndef TASK_VEHICLE_CHASE_H
#define TASK_VEHICLE_CHASE_H

// Rage headers
#include "atl/array.h"

// Game headers
#include "scene\RegdRefTypes.h"
#include "Task\System\Task.h"
#include "Task\System\TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations (struct)
struct sVehicleMissionParams;

// Forward declarations (class)
class CTaskVehicleBlock;
class CTaskVehiclePursue;
class CTaskVehicleRam;

//=========================================================================
// CTaskVehicleChase
//=========================================================================

class CTaskVehicleChase : public CTask
{

public:

	enum State
	{
		State_Invalid = -1,
		State_Start,
		State_Analyze,
		State_CloseDistance,
		State_Block,
		State_Pursue,
		State_Ram,
		State_SpinOut,
		State_PullAlongside,
		State_PullAlongsideInfront,
		State_Fallback,
		State_Finish,
	};

	//Note: These should be kept in sync with TASK_VEHICLE_CHASE_BEHAVIOR_FLAGS in script.
	enum BehaviorFlags
	{
		BF_CantBlock					= BIT0,
		BF_CantBlockFromPursue			= BIT1,
		BF_CantPursue					= BIT2,
		BF_CantRam						= BIT3,
		BF_CantSpinOut					= BIT4,
		BF_CantMakeAggressiveMove		= BIT5,
		BF_CantCruiseInFrontDuringBlock	= BIT6,
		BF_UseContinuousRam				= BIT7,
		BF_CantPullAlongside			= BIT8,
		BF_CantPullAlongsideInfront		= BIT9,
	};

	enum RunningFlags
	{
		RF_IsMakingAggressiveMove		= BIT0,
		RF_AggressiveMoveWasSuccessful	= BIT1,
		RF_IsStuck						= BIT2,
		RF_IsLowSpeedChase				= BIT3,
		RF_DisableAggressiveMoves		= BIT4,
	};

public:

	struct Tunables : public CTuning
	{	
		struct CloseDistance
		{
			CloseDistance()
			{}
			
			float	m_MinDistanceToStart;
			float	m_MinDistanceToContinue;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Block
		{
			Block()
			{}
			
			float	m_MaxDotToStartFromAnalyze;
			float	m_MaxDotToContinueFromAnalyze;
			float	m_MinTargetSpeedToStartFromPursue;
			float	m_MinTargetSpeedToContinueFromPursue;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Pursue
		{
			Pursue()
			{}
			
			float	m_MinDotToStartFromAnalyze;
			float	m_MinDotToContinueFromAnalyze;
			float	m_IdealDistance;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Ram
		{
			Ram()
			{}
			
			float	m_StraightLineDistance;
			float	m_MinTargetSpeedToStartFromPursue;
			float	m_MinTargetSpeedToContinueFromPursue;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct SpinOut
		{
			SpinOut()
			{}
			
			float	m_StraightLineDistance;
			float	m_MinTargetSpeedToStartFromPursue;
			float	m_MinTargetSpeedToContinueFromPursue;
			
			PAR_SIMPLE_PARSABLE;
		};

		struct PullAlongside
		{
			PullAlongside()
			{}

			float	m_StraightLineDistance;
			float	m_MinTargetSpeedToStartFromPursue;
			float	m_MinTargetSpeedToContinueFromPursue;

			PAR_SIMPLE_PARSABLE;
		};

		struct PullAlongsideInfront
		{
			PullAlongsideInfront()
			{}

			float	m_StraightLineDistance;
			float	m_MinTargetSpeedToStartFromPursue;
			float	m_MinTargetSpeedToContinueFromPursue;

			PAR_SIMPLE_PARSABLE;
		};
		
		struct AggressiveMove
		{
			AggressiveMove()
			{}
			
			float	m_MaxDistanceToStartFromPursue;
			float	m_MinDotToStartFromPursue;
			float	m_MinSpeedLeewayToStartFromPursue;
			float	m_MaxTargetSteerAngleToStartFromPursue;
			float	m_MaxDistanceToContinueFromPursue;
			float	m_MinDotToContinueFromPursue;
			float	m_MaxTimeInStateToContinueFromPursue;
			float	m_MaxTargetSteerAngleToContinueFromPursue;
			float	m_MinDelay;
			float	m_MaxDelay;
			float	m_WeightToRamFromPursue;
			float	m_WeightToBlockFromPursue;
			float	m_WeightToSpinOutFromPursue;
			float	m_WeightToPullAlongsideFromPursue;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Cheat
		{	
			Cheat()
			{}
			
			float	m_MinSpeedDifferenceForPowerAdjustment;
			float	m_MaxSpeedDifferenceForPowerAdjustment;
			float	m_PowerForMinAdjustment;
			float	m_PowerForMaxAdjustment;
			float	m_PowerForMaxAdjustmentNetwork;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		CloseDistance	m_CloseDistance;
		Block			m_Block;
		Pursue			m_Pursue;
		Ram				m_Ram;
		SpinOut			m_SpinOut;
		PullAlongside	m_PullAlongside;
		PullAlongside	m_PullAlongsideInfront;
		AggressiveMove	m_AggressiveMove;
		Cheat			m_Cheat;
		float			m_MaxDotForHandBrake;
		float			m_TimeBetweenCarChaseShockingEvents;
		float			m_DistanceForCarChaseShockingEvents;

		PAR_PARSABLE;
	};

	struct Situation
	{
		float	m_fDistSq;
		float	m_fDot;
		float	m_fSpeed;
		float	m_fTargetSpeed;
		float	m_fSpeedLeeway;
		float	m_fTargetSteerAngle;
		float	m_fFacingDot;
		float	m_fSmoothedSpeedSq;
		float	m_fTargetSmoothedSpeedSq;
	};
	
	struct Overrides
	{
		//Note: These are prioritized from top to bottom.
		enum Source
		{
			FirstSource = 0,
			Script = FirstSource,
			Code,
			Director,
			MaxSources
		};
		
		struct Pursue
		{
			Pursue()
			: m_fDesiredOffset(0.0f)
			, m_fIdealDistance(-1.0f)
			{}
			
			float	m_fDesiredOffset;
			float	m_fIdealDistance;
		};
		
		Overrides()
		: m_Pursue()
		, m_uBehaviorFlags(0)
		{}
		
		Pursue		m_Pursue;
		fwFlags16	m_uBehaviorFlags;
	};

	CTaskVehicleChase(const CAITarget& rTarget);
	virtual ~CTaskVehicleChase();
	
public:

			u32			GetLastTimeTargetPositionWasOffRoad()	const { return m_uLastTimeTargetPositionWasOffRoad; }
	const	CAITarget&	GetTarget()								const { return m_Target; }

public:

	void SetAggressiveMoveDelay(float fMin, float fMax)
	{
		m_fAggressiveMoveDelayMin = fMin;
		m_fAggressiveMoveDelayMax = fMax;
	}
	
public:

	void		ChangeBehaviorFlag(Overrides::Source nSource, BehaviorFlags nBehaviorFlag, bool bValue);
	Vec3V_Out	GetDrift() const;
	bool		IsBlocking() const;
	bool		IsCruisingInFrontToBlock() const;
	bool		IsGettingInFrontToBlock() const;
	bool		IsMakingAggressiveMove() const;
	bool		IsPullingAlongside() const;
	bool		IsPursuing() const;
	bool		IsRamming() const;
	void		SetDesiredOffsetForPursue(Overrides::Source nSource, float fDesiredOffset);
	void		SetIdealDistanceForPursue(Overrides::Source nSource, float fIdealDistance);
	void		TargetPositionIsOffRoad();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleChase(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CHASE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool		ProcessMoveSignals();

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Analyze_OnUpdate();
	void		CloseDistance_OnEnter();
	FSM_Return	CloseDistance_OnUpdate();
	void		Block_OnEnter();
	FSM_Return	Block_OnUpdate();
	void		Block_OnExit();
	void		Pursue_OnEnter();
	FSM_Return	Pursue_OnUpdate();
	void		Pursue_OnExit();
	void		Ram_OnEnter();
	FSM_Return	Ram_OnUpdate();
	void		SpinOut_OnEnter();
	FSM_Return	SpinOut_OnUpdate();
	void		PullAlongside_OnEnter();
	FSM_Return	PullAlongside_OnUpdate();
	void		PullAlongsideInfront_OnEnter();
	FSM_Return	PullAlongsideInfront_OnUpdate();
	void		PullAlongsideInfront_OnExit();
	void		FallBack_OnEnter();
	FSM_Return	FallBack_OnUpdate();
	
private:

	struct IsSituationValidInput
	{
		enum Flags
		{
			CheckMinDistance			= BIT0,
			CheckMaxDistance			= BIT1,
			CheckMinDot					= BIT2,
			CheckMaxDot					= BIT3,
			CheckMinTimeInState			= BIT4,
			CheckMaxTimeInState			= BIT5,
			CheckMinTargetSpeed			= BIT6,
			CheckMaxTargetSpeed			= BIT7,
			CheckMinSpeedLeeway			= BIT8,
			CheckMaxTargetSteerAngle	= BIT9,
		};

		IsSituationValidInput()
		: m_fMinDistance(0.0f)
		, m_fMaxDistance(0.0f)
		, m_fMinDot(0.0f)
		, m_fMaxDot(0.0f)
		, m_fMinTimeInState(0.0f)
		, m_fMaxTimeInState(0.0f)
		, m_fMinTargetSpeed(0.0f)
		, m_fMaxTargetSpeed(0.0f)
		, m_fMinSpeedLeeway(0.0f)
		, m_fMaxTargetSteerAngle(0.0f)
		, m_uFlags(0)
		{

		}

		float		m_fMinDistance;
		float		m_fMaxDistance;
		float		m_fMinDot;
		float		m_fMaxDot;
		float		m_fMinTimeInState;
		float		m_fMaxTimeInState;
		float		m_fMinTargetSpeed;
		float		m_fMaxTargetSpeed;
		float		m_fMinSpeedLeeway;
		float		m_fMaxTargetSteerAngle;
		fwFlags16	m_uFlags;
	};
	bool IsSituationValid(const IsSituationValidInput& rInput) const;

private:

	void				AddToVehicleChaseDirector();
	bool				AreWeForcedIntoBlock() const;
	bool				AreWeForcedIntoPursue() const;
	float				CalculateAggressiveMoveDelay() const;
	float				CalculateCheatPowerIncrease() const;
	float				CalculateCruiseSpeed() const;
	float				CalculateDesiredOffsetForPursue() const;
	Vec3V_Out			CalculateDirectionForVehicle(const CVehicle& rVehicle) const;
	float				CalculateIdealDistanceForPursue() const;
	bool				CanAnyoneInVehicleDoDrivebys(const CVehicle& rVehicle) const;
	bool				CanBlock() const;
	bool				CanDoDrivebys() const;
	bool				CanMakeAggressiveMove() const;
	bool				CanPedDoDrivebys(const CPed& rPed) const;
	bool				CanPullAlongside() const;
	bool				CanPullAlongsideInfront() const;
	bool				CanPursue() const;
	bool				CanRam() const;
	bool				CanSpinOut() const;
	bool				CanUseHandBrakeForTurns() const;
	void				CheckForAggressiveMoveSuccess();
	s32					ChooseAggressiveMoveFromPursue() const;
	s32					ChooseAppropriateStateForAnalyze() const;
	s32					ChooseAppropriateStateForBlock();
	s32					ChooseAppropriateStateForCloseDistance() const;
	s32					ChooseAppropriateStateForPullAlongside() const;
	s32					ChooseAppropriateStateForPullAlongsideInfront();
	s32					ChooseAppropriateStateForPursue();
	s32					ChooseAppropriateStateForRam() const;
	s32					ChooseAppropriateStateForSpinOut() const;
	float				GetDesiredSpeed() const;
	const CPhysical*	GetDominantTarget() const;
	const CVehicle*		GetTargetVehicle() const;
	CTaskVehicleBlock*	GetVehicleTaskForBlock() const;
	CTaskVehiclePursue*	GetVehicleTaskForPursue() const;
	CTaskVehicleRam*	GetVehicleTaskForRam() const;
	bool				HasRamMadeContact() const;
	bool				HasSpinOutMadeContact() const;
	bool				IsAggressiveMoveSuccessful() const;
	bool				IsBehaviorFlagSet(BehaviorFlags nBehaviorFlag) const;
	bool				IsBlockAppropriate() const;
	bool				IsBlockAppropriateFromAnalyze() const;
	bool				IsBlockAppropriateFromPursue() const;
	bool				IsBlockNoLongerAppropriate() const;
	bool				IsBlockNoLongerAppropriateFromAnalyze() const;
	bool				IsBlockNoLongerAppropriateFromPursue() const;
	bool				IsBlockSuccessful() const;
	bool				IsCloseDistanceAppropriate() const;
	bool				IsCloseDistanceNoLongerAppropriate() const;
	bool				IsFrustrated() const;
	bool				IsMakingAggressiveMoveFromAnalyze() const;
	bool				IsMakingAggressiveMoveFromPursue() const;
	bool				IsPullAlongsideAppropriate() const;
	bool				IsPullAlongsideAppropriateFromPursue() const;
	bool				IsPullAlongsideNoLongerAppropriate() const;
	bool				IsPullAlongsideNoLongerAppropriateFromPursue() const;
	bool				IsPullAlongsideSuccessful() const;
	bool				IsPullAlongsideInfrontAppropriate() const;
	bool				IsPullAlongsideInfrontAppropriateFromAnalyze() const;
	bool				IsPullAlongsideInfrontNoLongerAppropriate() const;
	bool				IsPullAlongsideInfrontNoLongerAppropriateFromAnalyze() const;
	bool				IsPursueAppropriate() const;
	bool				IsPursueAppropriateFromAnalyze() const;
	bool				IsPursueNoLongerAppropriate() const;
	bool				IsPursueNoLongerAppropriateFromAnalyze() const;
	bool				IsRamAppropriate() const;
	bool				IsRamAppropriateFromPursue() const;
	bool				IsRamNoLongerAppropriate() const;
	bool				IsRamNoLongerAppropriateFromPursue() const;
	bool				IsRamSuccessful() const;
	bool				IsSpinOutAppropriate() const;
	bool				IsSpinOutAppropriateFromPursue() const;
	bool				IsSpinOutNoLongerAppropriate() const;
	bool				IsSpinOutNoLongerAppropriateFromPursue() const;
	bool				IsSpinOutSuccessful() const;
	bool				IsStuck() const;
	void				LoadSituationToContinueAggressiveMoveFromPursue(IsSituationValidInput& rInput) const;
	void				LoadSituationToStartAggressiveMoveFromPursue(IsSituationValidInput& rInput) const;
	void				LoadVehicleMissionParameters(sVehicleMissionParams& rParams);
	void				OnDesiredOffsetForPursueChanged();
	void				OnIdealDistanceForPursueChanged();
	void				ProcessAggressiveMove();
	void				ProcessCheatFlags();
	void				ProcessCheatPowerIncrease();
	void				ProcessHandBrake();
	void				ProcessSituation();
	void				ProcessShockingEvents();
	void				ProcessStuck();
	void				ProcessLowSpeedChase();
	void				RemoveFromVehicleChaseDirector();
	bool				ShouldMakeAggressiveMove() const;
	bool				ShouldUseCheatFlags() const;
	bool				ShouldUseCheatPowerIncrease() const;
	bool				ShouldUseContinuousRam() const;
	void				UpdatePursue();
	bool				ShouldFallBack() const;
	
private:

	static float	CalculateCruiseSpeed(const CPed& rPed, const CVehicle& rVehicle);
	static void		LoadVehicleMissionParameters(const CPed& rPed, const CVehicle& rVehicle, const CEntity& rTarget, sVehicleMissionParams& rParams);

private:
	
	atRangeArray<Overrides, Overrides::MaxSources>	m_Overrides;
	Situation										m_Situation;
	CAITarget										m_Target;
	float											m_fAggressiveMoveDelay;
	float											m_fAggressiveMoveDelayMin;
	float											m_fAggressiveMoveDelayMax;
	float											m_fCarChaseShockingEventCounter;
	float											m_fFrustrationTimer;
	u32												m_uLastTimeTargetPositionWasOffRoad;
	u32												m_uTimeStateBecameInvalid;
	fwFlags8										m_uRunningFlags;

private:
	
	static Tunables	sm_Tunables;

};

#endif // TASK_VEHICLE_CHASE_H
