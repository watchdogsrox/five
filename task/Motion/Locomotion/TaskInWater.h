// 
// task/motion/locomotion/taskinwater.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_IN_WATER_H
#define TASK_IN_WATER_H


#include "math/angmath.h"

#include "Peds/ped.h"
#include "Task/System/Task.h"
#include "Task/Motion/TaskMotionBase.h"


class CTaskMotionSwimming : public CTaskMotionBase
{
public:
	
	struct PedVariation
	{
		PedVariation()
		: m_Component(PV_COMP_INVALID)
		, m_DrawableId(0)
		, m_DrawableAltId(0)
		{}

		ePedVarComp		m_Component;
		u32				m_DrawableId;
		u32				m_DrawableAltId;

		PAR_SIMPLE_PARSABLE;
	};

	struct PedVariationSet
	{
		PedVariationSet()
		: m_Component(PV_COMP_INVALID)
		{}

		ePedVarComp		m_Component;
		atArray<u32>	m_DrawableIds;

		PAR_SIMPLE_PARSABLE;
	};

	struct ScubaGearVariation
	{
		ScubaGearVariation()
		{}

		~ScubaGearVariation()
		{}

		atArray<PedVariationSet> m_Wearing;
		PedVariation			 m_ScubaGearWithLightsOn;
		PedVariation			 m_ScubaGearWithLightsOff;

		PAR_SIMPLE_PARSABLE;
	};

	struct ScubaGearVariations
	{
		ScubaGearVariations()
		{}

		~ScubaGearVariations()
		{}

		atHashWithStringNotFinal	m_ModelName;
		atArray<ScubaGearVariation> m_Variations;

		PAR_SIMPLE_PARSABLE;
	};

	struct Tunables : CTuning
	{
		struct ScubaMaskProp
		{
			ScubaMaskProp()
			: m_ModelName()
			, m_Index(-1)
			{}

			~ScubaMaskProp()
			{}

			atHashString	m_ModelName;
			s32				m_Index;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		int m_MinStruggleTime;
		int m_MaxStruggleTime;

		atArray<ScubaGearVariations> m_ScubaGearVariations;

		atArray<ScubaMaskProp> m_ScubaMaskProps;

		PAR_PARSABLE;
	};

	//Stores default initial values for water physics
	static const float s_fPreviousVelocityChange;
	static const float s_fHeightBelowWater;
	static const float s_fHeightBelowWaterGoal;

public:
	enum MotionState
	{
		State_Start,
		State_Idle,
		State_RapidsIdle,
		State_SwimStart,		
		State_Swimming,
		State_Exit
	};

	CTaskMotionSwimming( s32 initialState = State_Start, bool bAllowIdle = true,  float fPreviousVelocityChange = s_fPreviousVelocityChange, const float fHeightBelowWater = s_fHeightBelowWater, const float fHeightBelowWaterGoal = s_fHeightBelowWaterGoal):
			m_fHeightBelowWater(fHeightBelowWater),
			m_fHeightBelowWaterGoal(fHeightBelowWaterGoal),
			m_fSmoothedDirection(0.0f),
			m_fPrevDirection(0.0f),
			m_fSwimHeadingLerpRate(0.0f),
			m_fSwimRateScalar(1.0f),
			m_PreviousVelocityChange(fPreviousVelocityChange),
			m_initialState(initialState),
			m_bAnimationsReady(false),
			m_bAllowIdle(bAllowIdle),
			m_bPossibleGaitReduction(false),
			m_uNextStruggleTime(0),
			m_uBreathTimer(0),
			m_TurnPhaseGoal(0.0f),
			m_Flags(0),
			m_WeaponClipSetId(CLIP_SET_ID_INVALID),
			m_WeaponClipSet(NULL),
			m_WeaponFilterId(FILTER_ID_INVALID),
			m_pWeaponFilter(NULL),
			m_fLastWaveDelta(0),
			m_fTargetHeading(0.0f),
			m_bSwimGaitReduced(false),
			m_fGaitReductionHeading(0.0f)
		{
			SetInternalTaskType(CTaskTypes::TASK_MOTION_SWIMMING);
			// Initialise the weapon clip set
			SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID);
		};

	~CTaskMotionSwimming();

public:

	static void									ClearScubaGearVariationForPed(CPed& rPed);
	static void									ClearScubaMaskPropForPed(CPed& rPed);
	static const PedVariation*					FindScubaGearVariationForPed(const CPed& rPed, bool bLightOn);
	static const ScubaGearVariations*			FindScubaGearVariationsForModel(u32 uModelName);
	static const Tunables::ScubaMaskProp*		FindScubaMaskPropForPed(const CPed& rPed);
	static bool									GetScubaGearVariationForPed(const CPed& rPed, ePedVarComp& nComponent, u8& uDrawableId, u8& uDrawableAltId, u8& uTexId);
	static u32									GetTintIndexFromScubaGearVariationForPed(const CPed& rPed);
	static bool									IsScubaGearLightOnForPed(const CPed& rPed);
	static bool 								IsScubaGearVariationActiveForPed(const CPed& rPed);
	static bool 								IsScubaGearVariationActiveForPed(const CPed& rPed, bool bLightOn);
	static void									OnPedDiving(CPed& rPed);
	static void									OnPedNoLongerDiving(CPed& rPed, bool bForceSet = false);
	static void 								SetScubaGearVariationForPed(CPed& rPed);
	static void 								SetScubaGearVariationForPed(CPed& rPed, bool bLightOn);
	static void 								SetScubaMaskPropForPed(CPed& rPed);
	static void									UpdateScubaGearVariationForPed(CPed& rPed, bool bLightOn, bool bForceSet = false);
	static atHashWithStringNotFinal				GetModelForScubaGear(CPed& rPed);

public:

	virtual aiTask* Copy() const { return rage_new CTaskMotionSwimming( GetState(), m_bAllowIdle, m_PreviousVelocityChange, m_fHeightBelowWater, m_fHeightBelowWaterGoal); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_SWIMMING; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

	void SendParams();
	bool SyncToMoveState();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const; 

	FSM_Return		ProcessPreFSM	();
	FSM_Return		UpdateFSM		(const s32 iState, const FSM_Event iEvent);	
	virtual bool	ProcessMoveSignals();
	void			CleanUp			();

	static bool CanDiveFromPosition(CPed& rPed, Vec3V_In vPosition, bool bCanParachute, Vec3V_Ptr pOverridePedPosition = NULL, bool bCollideWithIgnoreEntity = false);

	// FSM optional functions
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	virtual bool IsInWater() { return true; }

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	//*************************************************************************************************************
	// This function takes the ped's animated velocity and modifies it based upon the orientation of the surface underfoot
	// It sets the desired velocity on the ped in world space.
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	//***********************************************************************************************
	// This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir()	{return(false);} // If theres a drop under them, then just fall

	//********************************************************************************************
	// This function returns the default main-task-tree player control task for this motion task
	virtual CTask * CreatePlayerControlTask();

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor() { return false; }

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	inline void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) { fMinOut = -ms_fMaxPitch; fMaxOut = ms_fMaxPitch; }

	inline bool SupportsMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state))
	{
		return false;
	}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const
	{
		switch (state)
		{
			case CPedMotionStates::MotionState_Swimming_TreadWater: return GetState() <= State_SwimStart ? true : false;
			default: break;
		}
		return false;
	}

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_MoveNetworkHelper.GetClipSetId();
		outClip = CLIP_IDLE;
	}

	void SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression = false);

	static bool CheckForRapids(CPed* pPed);
	static bool CheckForWaterfall(CPed* pPed);

	bool GetHorseGaitPhaseAndSpeed(Vector4& phase, float& speed, float& turnPhase, float &slopePhase);
	static void InterpBoundPitch(CPed* pPed, float fTarget); 
	
	const float GetPreviousVelocityChange() const { return m_PreviousVelocityChange; }
	const float GetHeightBelowWater() const { return m_fHeightBelowWater; }
	const float GetHeightBelowWaterGoal() const { return m_fHeightBelowWaterGoal; }
	const float GetLastWaveDelta() const {return m_fLastWaveDelta; }

	fwMvClipSetId GetSwimmingClipSetId();
	fwMvClipSetId GetCurrentSwimmingClipsetId() { return m_MoveNetworkHelper.GetClipSetId(); }

	// B*2257378: Tests water level at four offsets around position to check for large incoming waves. Returns highest water level.
	static float GetHighestWaterLevelAroundPos(const CPed *pPed, const Vector3 vPos, const float fWaterLevelAtPos, Vector3 &vHighestWaterLevelPosition, const float fOffset = 0.33f);

private:
		
	void ClampVelocityChange(Vector3& vChangeInAndOut, const float timeStep);
	void ProcessFlowOrientation();
	Vector3 ProcessSwimmingResistance(float fTimestep);

	CVehicle* GetVehAbovePed(CPed *ped, float testDistance);

	bool StartMoveNetwork();

	void SendWeaponClipSet();
	void StoreWeaponFilter();

	void ProcessMovement();

	void PitchCapsuleBound();

	void HideGun();

	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();
	void						Start_OnExit();
		
	FSM_Return					Idle_OnUpdate();

	void						RapidsIdle_OnEnter();
	FSM_Return					RapidsIdle_OnUpdate();
	void						RapidsIdle_OnExit();

	void						SwimStart_OnEnter();
	FSM_Return					SwimStart_OnUpdate();
	void						SwimStart_OnExit();

	// Swimming
	void						Swimming_OnEnter();
	FSM_Return					Swimming_OnUpdate();
	void						Swimming_OnExit();

	CMoveNetworkHelper m_MoveNetworkHelper;

	// Clip clipSetId to use for synced partial weapon clips
	fwClipSet* m_WeaponClipSet;
	fwMvClipSetId m_WeaponClipSetId;

	// The filter to be used for weapon clips 
	fwMvFilterId m_WeaponFilterId;
	crFrameFilterMultiWeight* m_pWeaponFilter;

	float		m_PreviousVelocityChange;

	float		m_fHeightBelowWater; //tracks the current height below the water line (even when swimming on the surface the ped will still be partially submerged)
	float		m_fHeightBelowWaterGoal;
	float		m_fSwimRateScalar;
	float		m_fPrevDirection;
	float		m_fSwimHeadingLerpRate;
	float		m_fSwimStartHeadingLerpRate;	

	float		m_fTargetHeading;

	u32			m_uNextStruggleTime;
	u32			m_uBreathTimer;

	s32			m_initialState;

	bool		m_bPossibleGaitReduction;

	float		m_fSmoothedDirection;	
	float		m_fLastWaveDelta;

	bool		m_bSwimGaitReduced;
	float		m_fGaitReductionHeading;

	// Flags
	enum Flags
	{		
		Flag_WeaponClipSetChanged					= BIT0,
		Flag_WeaponFilterChanged					= BIT1,				
		Flag_UpperBodyShadowExpression				= BIT2
	};

	// Flags
	fwFlags8 m_Flags;

	static const fwMvBooleanId ms_OnEnterIdleSwimmingId;
	static const fwMvBooleanId ms_OnEnterRapidsIdleSwimmingId;
	static const fwMvBooleanId ms_OnExitStruggleSwimmingId;
	static const fwMvBooleanId ms_OnEnterStopSwimmingId;
	static const fwMvBooleanId ms_OnEnterSwimmingId;
	static const fwMvBooleanId ms_OnEnterSwimStartId;
	static const fwMvBooleanId ms_BreathCompleteId;
	static const fwMvFlagId ms_AnimsLoadedId;
	static const fwMvFlagId ms_InRapidsId;
	static const fwMvFloatId ms_DesiredSpeedId;
	static const fwMvFloatId ms_HeadingId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_TargetHeading;
	static const fwMvFloatId ms_TargetHeadingDelta;
	static const fwMvFloatId ms_MoveRateOverrideId;
	static const fwMvFloatId ms_SwimPhaseId;
	static const fwMvRequestId ms_StruggleId;
	static const fwMvRequestId ms_BreathId;
	static const fwMvFlagId ms_FPSSwimming;
	static const fwMvClipId ms_WeaponHoldingClipId;

	bool		 m_bAnimationsReady;

	// Dont allow idling, just keep on moving, this ped can not just stay in place in water.
	bool		 m_bAllowIdle;

	// To inform our rider of how much we are turning
	float		m_TurnPhaseGoal;

public:
	// Allow min/max pitch to be steep, but not enough to disorientate
	static dev_float ms_fMaxPitch;

#if FPS_MODE_SUPPORTED
	// Used in swimming/aiming tasks for water height matching calculations
	static dev_float ms_fFPSClipHeightOffset;
#endif	//FPS_MODE_SUPPORTED

private:

	static Tunables sm_Tunables;
};

class CTaskMotionDiving : public CTaskMotionBase
{
	public:
	enum MotionState
	{
		State_DivingDown,
		State_Idle,
		State_StartSwimming,
		State_Swimming,
		State_Glide,
		State_Exit
	};
	CTaskMotionDiving( s32 initialState = State_DivingDown):
		m_fHeightBelowWater(0.0f),
			m_fGlideForwardsVel(0.0f),
			m_initialState(initialState), 
			m_smoothedDirection(0.0f),
			m_fPrevDirection(0.0f),
			m_fGroundClearance(-1.0f),
			m_fPitchBlend(0.5f),
			m_bAnimationsReady(false),
			m_bPositivePitchChange(true),
			m_fPitchRate(1.0f),
			m_fDivingSwimHeadingLerpRate(2.0f),
			m_bDiveGaitReduced(false),
			m_bCanRotateCamRel(false),
			m_bApplyForwardDrift(false),
			m_bDivingDownBottomedOut(false),
			m_fGaitReductionHeading(0),
			m_fDivingDownCorrectionGoal(0.0f),
			m_fHeadingApproachRate(0),
			m_fForwardDriftScale(0.0f),
			m_fDivingIdlePitchLerpRateLerpRate(0.0f),
#if FPS_MODE_SUPPORTED
			m_fFPSStrafeDirection(0.5f),
#endif	//FPS_MODE_SUPPORTED
			m_Flags(0),
			m_WeaponClipSetId(CLIP_SET_ID_INVALID),
			m_WeaponClipSet(NULL),
			m_WeaponFilterId(FILTER_ID_INVALID),
			m_pWeaponFilter(NULL),
			m_bForceScubaAnims(false),
			m_bResetAnims(false),
			m_bHasEnabledLight(false)
		{			
			SetInternalTaskType(CTaskTypes::TASK_MOTION_DIVING);
			// Initialise the weapon clip set
			SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID);
		};


	~CTaskMotionDiving();

	virtual aiTask* Copy() const { return rage_new CTaskMotionDiving( GetState()); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_DIVING; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

	void SendParams();
	bool SyncToMoveState();

	void StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping);
	void EndAlternateClip(AlternateClipType type, float fBlendDuration);
	
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const; 

	FSM_Return		ProcessPreFSM	();
	FSM_Return		ProcessPostFSM();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	// FSM optional functions
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }
	void			CleanUp			();

	virtual bool IsInWater() { return true; }

	virtual bool IsUnderWater() const { return true; }

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	//*************************************************************************************************************
	// This function takes the ped's animated velocity and modifies it based upon the orientation of the surface underfoot
	// It sets the desired velocity on the ped in world space.
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	//***********************************************************************************************
	// This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir() { return false; } // If theres a drop under them, then just fall

	//********************************************************************************************
	// This function returns the default main-task-tree player control task for this motion task
	virtual CTask * CreatePlayerControlTask();

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor() { return false; }

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_MoveNetworkHelper.GetClipSetId();
		outClip = CLIP_IDLE;
	}

	inline bool IsNormalSwimming() { return GetState()==State_Swimming && GetMotionData().GetCurrentMbrY()<MOVEBLENDRATIO_RUN; }
	inline bool IsSprintSwimming() { return GetState()==State_Swimming && GetMotionData().GetCurrentMbrY()>=MOVEBLENDRATIO_RUN; }
	inline bool IsFullSprintSwimming() { return GetState()==State_Swimming && GetMotionData().GetCurrentMbrY()==MOVEBLENDRATIO_SPRINT; }

	inline bool IsGliding() { return GetState()==State_Glide; }

	inline bool IsDivingDown()	{  return GetState()==State_DivingDown; }

	inline bool GetIsIdle() { return GetState()==State_Idle; }

	inline bool CanRotateCamRelative() { return m_bCanRotateCamRel; }

	inline void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) { fMinOut = -ms_fMaxPitch; fMaxOut = ms_fMaxPitch; }

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Diving_Idle:
		case CPedMotionStates::MotionState_Diving_Swim:
			return true;
		default:
			return false;
		}
	}

	void SetApplyForwardDrift() {m_bApplyForwardDrift = true;}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const
	{
		switch(state)
		{
		case CPedMotionStates::MotionState_Diving_Idle:
			return GetState()==State_Idle;
		case CPedMotionStates::MotionState_Diving_Swim:
			return GetState()==State_Swimming;
		default:
			return false;
		}
	}

	float			GetGroundClearance() const {return m_fGroundClearance;}
	void			SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression = false);
	static float	TestGroundClearance(CPed *pPed, float testDepth, float heightOffset = 0.0f, bool scaleStart = true);

	fwMvClipSetId GetDivingClipSetId();	
	fwMvClipSetId GetCurrentDivingClipsetId() { return m_MoveNetworkHelper.GetClipSetId(); }

	static void PitchCapsuleRelativeToPed(CPed *pPed);

	static void UpdateDiveGaitReduction(CPed* pPed, bool &bGaitReduced, float &fGaitReductionHeading, bool bFromSwimmingTask = false);

private:

	void ClampVelocityChange(Vector3& vChangeInAndOut, const float timeStep);
	Vector3 ProcessSwimmingResistance(float fTimestep);

	CVehicle* GetVehAbovePed(CPed *ped, float testDistance);

	bool StartMoveNetwork();

	void ResetDivingClipSet();
	void SendWeaponClipSet();
	void StoreWeaponFilter();

	void ProcessMovement();
	virtual bool	ProcessPostMovement();
	void PitchCapsuleBound();

	void UpdateRiverReorientation(CPed *pPed); 

	// State Functions
	// Start
	void						DivingDown_OnEnter();
	FSM_Return					DivingDown_OnUpdate();
		
	void						Idle_OnEnter();
	FSM_Return					Idle_OnUpdate();
	void						Idle_OnExit();

	void						StartSwimming_OnEnter();
	FSM_Return					StartSwimming_OnUpdate();
	void						StartSwimming_OnExit();

	// Swimming
	void						Swimming_OnEnter();
	FSM_Return					Swimming_OnUpdate();
	void						Swimming_OnExit();

	void						Glide_OnEnter();
	FSM_Return					Glide_OnUpdate();	

	void SendAlternateParams();

	void HolsterWeapon(bool bHolster = true);

	void SetWeaponHoldingClip(bool bIsGun);

	CMoveNetworkHelper m_MoveNetworkHelper;

	// Clip clipSetId to use for synced partial weapon clips
	fwClipSet* m_WeaponClipSet;
	fwMvClipSetId m_WeaponClipSetId;

	// The filter to be used for weapon clips 
	fwMvFilterId m_WeaponFilterId;
	crFrameFilterMultiWeight* m_pWeaponFilter;

	float		m_fHeightBelowWater; //tracks the current height below the water line (even when swimming on the surface the ped will still be partially submerged)

	float		m_fGlideForwardsVel; // Member variable used to control speed in the gliding state.

	s32			m_initialState;

	float		m_smoothedDirection; 
	float		m_fPrevDirection; 
	float		m_fGroundClearance;
	float		m_fPitchBlend;
	float		m_fIdleHeadingApproachRate;
	float		m_fHeadingApproachRate;
	float		m_fDivingSwimHeadingLerpRate;
	float		m_fGaitReductionHeading;
	float		m_fDivingDownCorrectionGoal;
	float		m_fDivingDownCorrectionCurrent;
	float		m_fForwardDriftScale;
	float		m_fDivingIdlePitchLerpRateLerpRate;
#if FPS_MODE_SUPPORTED
	float		m_fFPSStrafeDirection;
#endif	//FPS_MODE_SUPPORTED

	bool		m_bCanRotateCamRel; 
	bool		m_bAnimationsReady;
	bool		m_bDiveGaitReduced;
	bool		m_bApplyForwardDrift;
	bool		m_bDivingDownBottomedOut;
	bool		m_bForceScubaAnims;
	bool		m_bResetAnims;
	bool		m_bHasEnabledLight;

	// Flags
	enum Flags
	{		
		Flag_WeaponClipSetChanged					= BIT0,
		Flag_WeaponFilterChanged					= BIT1,				
		Flag_UpperBodyShadowExpression				= BIT2
	};


	// Flags
	fwFlags8 m_Flags;

	static const fwMvBooleanId ms_OnEnterGlideId;
	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvBooleanId ms_OnEnterStartSwimmingId;
	static const fwMvBooleanId ms_OnEnterSwimmingId;
	static const fwMvFlagId ms_AnimsLoadedId;
	static const fwMvFlagId ms_AnimsLoadedToIdleId;
	static const fwMvFlagId ms_UseAlternateRunFlagId;
	static const fwMvClipId ms_AlternateRunClipId;
	static const fwMvFloatId ms_DesiredSpeedId;
	static const fwMvFloatId ms_HeadingId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_PitchPhaseId;
	static const fwMvFloatId ms_MoveRateOverrideId;
	static const fwMvRequestId ms_RestartState;
	static const fwMvBooleanId ms_IsFPSMode;
	static const fwMvFloatId ms_StrafeDirectionId;


	static const fwMvClipId ms_IdleWeaponHoldingClipId;
	static const fwMvClipId ms_GlideWeaponHoldingClipId;
	static const fwMvClipId ms_SwimRunWeaponHoldingClipId;
	static const fwMvClipId ms_SwimSprintWeaponHoldingClipId;

public:

	// Allow min/max pitch to be steep, but not enough to disorientate
	static dev_float ms_fMaxPitch;
	// Dive-under pitch is a little less than the max pitch
	static dev_float ms_fDiveUnderPitch;

	bool m_bPositivePitchChange;
	float m_fPitchRate;  
	

};

#endif // TASK_IN_WATER_H
