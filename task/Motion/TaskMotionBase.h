// 
// task/motion/TaskMotionBase.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_MOTION_BASE_H
#define TASK_MOTION_BASE_H


// Rage includes
#include "atl/functor.h"
#include "atl/slist.h"
#include "vector/vector2.h"


// Game includes
#include "task/system/Task.h"
#include "Peds/Pedmoveblend/PedMoveBlendBase.h"
#include "Peds/ped.h"

//**********************************************************************************************************
//
//	CTaskMotionBase
//	
//	Shared base class for entity specific primary motion tasks. An entities primary motion task
//	is responsible for creating and switching between the different forms of motion available
//	to that entity (e.g. walking, swimming, crouching, etc). It also acts as a single point of
//	contact for generic motion related information, such as move blend ratios, desired velocities,
//	etc.
//	
//**********************************************************************************************************

class CTaskMotionBase : public CTask
{

public:

	enum AlternateClipType
	{
		ACT_Idle,
		ACT_Walk,
		ACT_Run,
		ACT_Sprint,
		ACT_MAX,
	};

	struct sAlternateClip
	{
		sAlternateClip()
			: iDictionaryIndex(-1)
			, uClipHash(u32(0))
			, fBlendDuration(0.0f)
		{
		}

		sAlternateClip(const s32 dictionary, const atHashWithStringNotFinal& clip, float fBlendDuration)
			: iDictionaryIndex(dictionary)
			, uClipHash(clip)
			, fBlendDuration(fBlendDuration)
		{
		}

		sAlternateClip(const atHashWithStringNotFinal& dictionary, const atHashWithStringNotFinal& clip, float fBlendDuration)
			: iDictionaryIndex(fwAnimManager::FindSlot(dictionary).Get())
			, uClipHash(clip)
			, fBlendDuration(fBlendDuration)
		{
		}

		sAlternateClip(const fwMvClipSetId& clipSetId, const atHashWithStringNotFinal& clip, float fBlendDuration)
			: iDictionaryIndex(-1)
			, uClipHash(clip)
			, fBlendDuration(fBlendDuration)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
			if(taskVerifyf(pClipSet, "pClipSet is NULL"))
			{
				iDictionaryIndex = pClipSet->GetClipDictionaryIndex().Get();
			}
		}

		bool IsValid() const { return iDictionaryIndex>-1 && uClipHash.GetHash()!=0; }

		s32 iDictionaryIndex;
		atHashWithStringNotFinal uClipHash;
		float fBlendDuration;
	};
	
	// PURPOSE: Passes a clipSetId of persistent data from one ped motion task
	//			to another. Used to cache clip sets / etc across 
	//			flushes of the task tree.
	class CPersistentData
	{
	public:
		CPersistentData();
		~CPersistentData();

		void Init(CTaskMotionBase& oldTask);
		void Apply(CTaskMotionBase& newTask);
	private:
		fwMvClipSetId m_onFootClipSet;
		fwMvClipSetId m_strafingClipSet;
		fwMvClipSetId m_weaponOverrideClipSet;
		u32	m_inVehicleOverrideContext;
		sAlternateClip m_alternateClips[ACT_MAX];
		bool m_alternateClipsLooping[ACT_MAX];
	};
	
public:
 
	CTaskMotionBase();
	virtual ~CTaskMotionBase();

	//////////////////////////////////////////////////////////////////////////
	//	Motion state interface
	//	Together with the enum in CPedMotionData these methods defines a core set of calls
	//	that can be made to any motion task. Each motion task can define 
	//	(via the SupportsMotionState method) whether it supports the states
	//	in the above enum.
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Should return true if the derived motion task supports the given motion state
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) = 0;

	// PURPOSE: Should return true if the derived motion task is currently in the given motion state
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const = 0;

	//********************************************************************
	//	PURE VIRTUAL FUNCTIONS
	//	These must be implemented in any derived classes.
	//********************************************************************

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)=0;

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const =0;

	// Should return the clip set id and clip id to be used to blend from nm into the current state
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSetId, fwMvClipId& outClipId) = 0;

	//*****************************************************************************************
	//	VIRTUAL FUNCTIONS
	//	These may be implemented, or the default implementations may be used if not required
	//*****************************************************************************************


	// PURPOSE: Should set the peds desired velocity based on it's current movement in preparation for passing to the physics system
	// The default implementation takes the ped's animated velocity and modifies it based upon the orientation of the surface underfoot
	// as well as preventing the ped from walking up very steep slopes.
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	// PURPOSE: Should be defined if this is a primary motion task, and the ped may come under player control
	//			Defines the player control task to start.
	virtual CTask * CreatePlayerControlTask() { taskAssertf(0, "Task %s needs to define a player control task", GetName().c_str()); return NULL; }

	//	PURPOSE: This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir() { return false; }

	//	PURPOSE: Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor() { return false; }

	//	PURPOSE: Should return true if the motion task is in an on foot state
	virtual bool IsOnFoot() { return false; }
	
	//	PURPOSE: Should return true if the motion task is in a strafing state
	virtual bool IsStrafing() const { return false; }
	
	//	PURPOSE: Should return true if the motion task is in a swimming state
	virtual bool IsSwimming() const { return false; }
	
	//	PURPOSE: Should return true if the motion task is in a diving state
	virtual bool IsDiving() const { return false; }

	// PURPOSE: Should return true if the motion task is in an in water state
	virtual bool IsInWater() { return false; }

	// PURPOSE: Should return true if the motion task is currently handling underwater behaviour
	virtual bool IsUnderWater() const { return false; }

	// PURPOSE: Should return true in derived classes when in a vehicle motion state
	virtual bool IsInVehicle() { return false; }

	// PURPOSE: Should return true in derived classes when the moveblender is preserving animated velocity
	virtual bool IsUsingAnimatedVelocity() { return false; }
	
	// PURPOSE: Should return true in derived classes when in an aiming motion state
	virtual bool IsAiming() const { return false; }

    // PURPOSE: Should return true in derived classes when in stealth motion state
	virtual bool IsUsingStealth() const { return false; }

	// PURPOSE: Should be set to true if this tasks child task is not a CTaskMotionBase
	//			Allows running other types of tasks on the motion task tree for utility purposes.
	virtual bool ForceLeafTask() const { return false; }

	virtual Vec3V_Out CalcDesiredAngularVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

	// PURPOSE: should return the stopping distance given the current movement state of the task
	virtual float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL);

	// Purpose: returns the turn rates at walk, run, and sprint move-blend ratios
	virtual void GetTurnRates(CMoveBlendRatioTurnRates& rates);

	// PURPOSE: return true if the goto tasks should not slow down this ped (in the idle state and needs to activate runstarts, for example).
	virtual bool ShouldDisableSlowingDownForCornering() const { return false; }

	// PURPOSE: To override to make ladder run mounts more "intentional", fixes issues where peds just exit cover and mount etc...
	virtual bool AllowLadderRunMount() const { return true; }

	// This function allows an active task to override the game play camera and associated settings.
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& UNUSED_PARAM(settings)) const {}

	//*************************************************************************************************
	// This function works out the angular velocity required for normal non-strafing movement.  It is
	// intended as a helper function, and its not essential to use it.
	float CalcDesiredTurnVelocityFromHeadingDelta(const float fHeadingChangeRate, const float fHeadingApproachTime, float fHeadingDelta);

	//**************************************************************************************************
	// This function moves the entities' current turn velocity towards the desired.  It is intended
	// as a helper function and it's use is not essential.
	void ApproachCurrentTurnVelocityTowardsDesired(const float fDesiredTurnVelocity, const float fTurnRateAccel, const float fTurnRateDecel, const bool bLessenHdgChangeForSmallAngles, const bool bLessenHdgChangeBySquaring);

	//*****************************************************************************************************
	// This function moves the m_CurrentMoveBlendRatio towards the m_vDesiredMoveBlendRatio.  Whether
	// it moves both the X as well as the Y is based upon the current m_bStrafing state.  It is intended
	// as a helper function, and its not necessary to use this.

	void ApproachCurrentMBRTowardsDesired(const float fMoveAccel, const float fMoveDecel, const float fMaxMBR);

	//*************************************************************************************************
	// This function can be implemented to increase the radius on movement tasks' completion criteria.
	// It may be useful for movement modes which cause peds to move at extra-fast speeds.
	virtual	float GetExtraRadiusForGotoPoint() const { return(0.0f); }

	//************************************************************************************************
	// This function may be implemented to force a camera heading when the FollowPed camera is active
	virtual	float GetCameraHeading();

	//**************************************************************************************************
	// This function may pass back optional navigation flags used by pathfinding in this movement mode
	// eg. skiing passes through the prefer downhill flag
	virtual u64 GetNavFlags() const { return 0; }

	//******************************************************************************************************
	// Returns the max vel-change limit for a ped based upon movement mode.  (used in slide-to-coord tasks)
	virtual float GetStdVelChangeLimit() { return 30.0f; }

	//***************************************************************************************************
	// This is used by the ambient clips system to determine when is an appropriate time to play an clip
	virtual bool IsGoodTimeToDropInAmbientMoveClip(const int UNUSED_PARAM(iPedMoveState)) const { return true; }

	//*********************************************************************************************
	// Whether we should ignore the desired heading when applying angular heading change, if true
	// then we allow the ped to turn past their desired (ie. to overshoot)
	virtual bool IgnoreDesiredHeadingWhenApplyingMovement() { return false; }

	//*****************************************************************************************
	// Used to balance the standing spring capsule which keeps the ped suspended on the ground
	virtual float GetStandingSpringStrength() { return ms_fBaseStandSpringStrength; }
	virtual float GetStandingSpringDamping() { return ms_fBaseStandSpringDamping; }

	// PURPOSE: Returns the distance the ped currently intends to turn based on its
	//			current and desired headings (expressed in radians)
	virtual float CalcDesiredDirection() const;

	// PURPOSE: Convert an angle in radians to a MoVE signal mapping the range
	//			-PI->PI to 0.0f->1.0f (0.0f becomes 0.5f)
	inline static float ConvertRadianToSignal(float angle) { angle = Clamp( angle/PI, -1.0f, 1.0f);	return ((-angle * 0.5f) + 0.5f); }

	// PURPOSE: Helper method for clamping a velocity change against the limits
	static Vec3V_Out CalcVelChangeLimitAndClamp(const CPed& ped, Vec3V_In changeInV, ScalarV_In timestepV, const CPhysical* pGroundPhysical);

	// Check if we can perform a transition from our current state to the network requested target state
	virtual bool IsClonePedMoveNetworkStateTransitionAllowed(u32 const UNUSED_PARAM(targetState)) const						{	return false;	}
	virtual bool IsClonePedNetworkStateChangeAllowed(u32 const UNUSED_PARAM(state))												{	return false;	}
	virtual bool GetStateMoveNetworkRequestId(u32 const UNUSED_PARAM(state), fwMvRequestId& UNUSED_PARAM(requestId)) const	{	return false;	}

	//*****************************************************************************************
	//	REGULAR MEMBER FUNCTIONS
	//*****************************************************************************************

	inline CPedMotionData& GetMotionData() { aiAssert(GetPed()); return *(GetPed()->GetMotionData()); }

	inline const CPedMotionData& GetMotionData() const { aiAssert(GetPed()); return *(GetPed()->GetMotionData()); }

	inline void GetMoveVector(Vector3 & vMoveVec)
	{

		Vector3 vCurrMbr(GetMotionData().GetCurrentMbrX(), GetMotionData().GetCurrentMbrY(), 0.0f);

		vMoveVec = VEC3V_TO_VECTOR3(GetPed()->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vCurrMbr)));
		if(vMoveVec.Mag2() > 0.001f)
			vMoveVec.Normalize();
	}

	static void	 RetrieveMoveSpeeds(fwClipSet& clipSet, CMoveBlendRatioSpeeds& speeds, const fwMvClipId* moveSpeedClipNames);
	static float GetActualMoveSpeed(const float * fMoveSpeeds, float fMoveRatio);
	static float GetMoveBlendRatioForDesiredSpeed(const float * pMoveSpeeds, float fDesiredSpeed);

	//*****************************************************************************************
	// Clamp Move Blend Ratio to physical speed

	//Gait reduction
	bool IsGaitReduced() const { return GetMotionData().GetGaitReduction() != 0.0f; }
	void UpdateGaitReduction();
	void ResetGaitReduction();

	void SetVelocityOverride(const Vector3 &overrideSpeed) { m_UseVelocityOverride = true; m_VelocityOverride.Set(overrideSpeed); }
	void ClearVelocityOverride() { m_UseVelocityOverride = false; }
	void SetCleanupMotionTaskNetworkOnQuit(const bool bCleanupMotionTaskNetworkOnQuit) { m_CleanupMotionTaskNetworkOnQuit = bCleanupMotionTaskNetworkOnQuit; }
	
	fwMvClipSetId GetCrouchClipSet() { static fwMvClipSetId s_crouchSet = fwMvClipSetId("move_ped_crouched",0xBB21CA0C); return s_crouchSet; }

	fwMvClipSetId GetModelInfoOnFootClipSet(const CPed& ped) const { return ped.GetPedModelInfo()->GetRandomMovementClipSet(&ped); }
	fwMvClipSetId GetDefaultOnFootClipSet(const bool bReturnMemberVariable = false) const;
	fwMvClipSetId GetDefaultAimingStrafingClipSet(bool bCrouched) const;
	fwMvClipSetId GetDefaultDivingClipSet() const { return m_defaultDivingClipSet; }
	fwMvClipSetId GetDefaultSwimmingClipSet() const { return m_defaultSwimmingClipSet; }
	fwMvClipSetId GetDefaultSwimmingBaseClipSet() const { return m_defaultSwimmingBaseClipSet; }
	fwMvClipSetId GetOverrideWeaponClipSet() const { return m_overrideWeaponClipSet; }
	fwMvClipSetId GetOverrideStrafingClipSet() const { return m_overrideStrafingClipSet; }
	fwMvClipSetId GetOverrideCrouchedStrafingClipSet() const { return m_overrideCrouchedStrafingClipSet; }
	fwMvFilterId GetOverrideWeaponClipSetFilter() const { return m_overrideWeaponClipSetFilter; }
	u32 GetOverrideInVehicleContextHash() const { return m_overrideInVehicleContext; }

#if __DEV 
	//B*1828169: Made virtual on dev builds so we can override in TaskMotionPed to track down where dodgy clipsets are being set
	virtual void SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId);
#else		
	void SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_defaultOnFootClipSet = clipSetId; }
#endif
	void SetDefaultDivingClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_defaultDivingClipSet = clipSetId; }
	void SetDefaultSwimmingClipSet(const fwMvClipSetId &clipSetId) { m_defaultSwimmingClipSet = clipSetId; }
	void SetDefaultSwimmingBaseClipSet(const fwMvClipSetId &clipSetId) { m_defaultSwimmingBaseClipSet = clipSetId; }
	void SetOverrideWeaponClipSet(const fwMvClipSetId &clipSetId) { animAssert(clipSetId != CLIP_SET_ID_INVALID); m_overrideWeaponClipSet = clipSetId; }
	void SetOverrideStrafingClipSet(const fwMvClipSetId &clipSetId) { m_overrideStrafingClipSet = clipSetId; }
	void SetOverrideCrouchedStrafingClipSet(const fwMvClipSetId &clipSetId) { m_overrideCrouchedStrafingClipSet = clipSetId; }
	void SetOverrideWeaponClipSetFilter(const fwMvFilterId &filterId) { m_overrideWeaponClipSetFilter = filterId; }
	void SetOverrideUsesUpperBodyShadowExpression(bool bUseExpression) { m_overrideUsesUpperBodyShadowExpression = bUseExpression; }

	inline bool IsUsingInjuredMovementOverride() {return m_isUsingInjuredMovementOverride; }

	// PURPOSE: Returns the idle clip for the currently applied on foot clip set
	virtual const crClip* GetBaseIdleClip();
	
	void SetOnFootClipSet(const fwMvClipSetId &clipSetId, const float fBlendDuration = SLOW_BLEND_DURATION);
	void SetInVehicleContextHash(u32 inVehicleContextHash, bool bRestartState);
	
	virtual void StealthClipSetChanged(const fwMvClipSetId &UNUSED_PARAM(clipSetId)) {}
	virtual void InVehicleContextChanged(u32 UNUSED_PARAM(inVehicleContextHash)) {}

	void ClearOverrideWeaponClipSet();
	
	virtual void OverrideWeaponClipSetCleared() {}

	void ResetSwimmingClipSet(const CPed& ped);
	void ResetOnFootClipSet(bool bRestartState = false, float fBlendDuration = SLOW_BLEND_DURATION);
	void ResetOnFootClipSet(const CPed& ped);
	void ResetStrafeClipSet();
	void ResetInjuredStrafeClipSet();
	void ResetInVehicleContextHash(bool bRestartState = false);

	void SetDefaultClipSets(const CPed& ped);
	
	// PURPOSE: Used by network code to retrieve the active clip set
	fwMvClipSetId GetActiveClipSet()
	{
		if(IsStrafing() || IsAiming())
		{
			return GetOverrideStrafingClipSet();
		}
        else if(IsOnFoot())
		{
		    return GetDefaultOnFootClipSet();
		}
		else if(IsSwimming())
		{
			return GetDefaultSwimmingClipSet();
		}
		else if(IsDiving())
		{
			return GetDefaultDivingClipSet();
		}
		else
		{
			return CLIP_SET_ID_INVALID;
		}
	}
	
	// PURPOSE: Used by network code to clipSetId the clip clipSetId for the active motion state
	void SetActiveClipSet(const fwMvClipSetId &clipSetId)
	{
		if(IsStrafing() || IsAiming())
		{
			SetOverrideStrafingClipSet(clipSetId);
		}
        else if(IsOnFoot())
		{
		    SetOnFootClipSet(clipSetId);
		}
		else if(IsSwimming())
		{
			SetDefaultSwimmingClipSet(clipSetId);
		}
		else if(IsDiving())
		{
			SetDefaultDivingClipSet(clipSetId);
		}
	}
	
	bool IsUsingInjuredClipSet()
	{
		static atHashWithStringNotFinal m_injuredSet("move_injured_generic",0xBE27F702);
		return (IsOnFoot() && m_defaultOnFootClipSet.GetHash()==m_injuredSet.GetHash());
	}
	
	const sAlternateClip& GetAlternateClip(AlternateClipType type) const { return m_alternateClips[type]; }
	void SetAlternateClip(AlternateClipType type, const sAlternateClip& data, bool bLooping);
	void StopAlternateClip(AlternateClipType type, float fBlendDuration);
	virtual void AlternateClipChanged(AlternateClipType UNUSED_PARAM(type)) {}

	inline void SetWantsToDiveUnderwater() { m_wantsToDiveUnderwater = true; }

	inline bool IsFullyInVehicle() const { return m_isFullyInVehicle; }
	inline bool IsFullyAiming() const { return m_isFullyAiming; }

	void SetAlternateClipLooping(AlternateClipType type, bool bLooping);
	bool GetAlternateClipLooping(AlternateClipType type) const;

	// TODO - force the motion task to reset itself and jump to the given state
	void ForceReset (u32 UNUSED_PARAM(stateToForce) ) { }

	// TODO - force the motion task to reset itself ain the current state
	void ForceReset ( ) { }

	void RequestTaskStateForNetworkClone(s32 state) { aiAssert(GetPed()); aiAssert(GetPed()->IsNetworkClone()); m_requestedCloneStateChange=state; m_lastRequestedCloneState=state;}

	void SetIndependentMoverFrame(const float fHeadingDelta, const fwMvRequestId& requestId, const bool bActuallyApply);
	float GetIndependentMoverDofWeight() const;

	static float InterpValue(float& current, float target, float interpRate, bool angular = false);

	static float InterpValue(float& current, float target, float interpRate, bool angular, float fDt);

	// clip set override flags
	bool HasWeaponClipSetSetFromAnims() const { return m_WeaponClipSetSetFromAnims; }
	void SetWeaponClipSetFromAnims(bool bSet) { m_WeaponClipSetSetFromAnims = bSet; }

	bool IsWaitingForTargetState() { return m_WaitingForTargetState; }
protected:

	// Get the float property from the passed in clip
	float GetFloatPropertyFromClip(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, const char* propertyName) const;

	void UpdateHeadIK();

	sAlternateClip m_alternateClips[ACT_MAX];

	WorldProbe::CShapeTestResults m_CapsuleResults;		

	Vector3 m_VelocityOverride;

	// PURPOSE: Used to select the clip clipSetId to use when switching motion tasks
	fwMvClipSetId	m_defaultOnFootClipSet;
	//fwMvClipSetId	m_defaultInjuredStrafingClipSet;
	fwMvClipSetId	m_defaultSwimmingClipSet;
	fwMvClipSetId	m_defaultSwimmingBaseClipSet;
	fwMvClipSetId	m_defaultDivingClipSet;
	fwMvClipSetId	m_overrideWeaponClipSet;
	fwMvClipSetId	m_overrideStrafingClipSet;
	fwMvClipSetId	m_overrideCrouchedStrafingClipSet;
	fwMvFilterId	m_overrideWeaponClipSetFilter;
	u32				m_overrideInVehicleContext;

	s32 m_requestedCloneStateChange;	// Should only ever be used on network clones. Will perform a state change in Task<blah>::UpdateFSM
	s32 m_lastRequestedCloneState;

	f32 m_onFootBlendDuration;

	float m_fVelocityRotation;
	u32 m_uLastTimeVelocityRotationActive;
	u32 m_uLastTimeVelocityRotationInactive;

	//! Note: This is contained as a bitset to reduce memory. Use accessors to get and set.
	u8 m_alternateClipsLooping;

	bool m_GaitReductionBlockedOnCollision : 1;
	bool m_wantsToDiveUnderwater : 1;
	bool m_isFullyInVehicle : 1;
	bool m_isFullyAiming : 1;
	bool m_UseVelocityOverride : 1;
	bool m_CleanupMotionTaskNetworkOnQuit : 1;
	bool m_overrideUsesUpperBodyShadowExpression : 1;
	bool m_isUsingInjuredMovementOverride : 1;
	bool m_WeaponClipSetSetFromAnims : 1;	// set if weapon clipset override was set via OverrideWeaponClipSetWithBase in CTaskAmbientClips
	bool m_WaitingForTargetState : 1;	// Set every frame by certain states that need to be monitored by NetObjPed clones

#if __BANK
	bool m_bHasBeenDeleted : 1;
#endif // __BANK

	//**************************************************************
	// Static members
	//**************************************************************
public:

	static float ms_fBaseStandSpringStrength;
	// The exact damping to create a critically damped spring
	static float ms_fBaseStandSpringDamping;
	static const fwMvClipSetId ms_AimingStrafingFirstPersonClipSetId;


	// Used for clamping the extracted velocity
	static dev_float ms_fAccelLimitStd;
	static dev_float ms_fAccelLimitQpd;
	static dev_float ms_fAccelLimitHigher;
	static dev_float ms_fAccelLimitDead;
	static dev_float ms_fAccelLimitPushedByDoor;

#if __BANK
	static sysPerformanceTimer * m_pPerfTimer;
	static float ms_fUpdateTimeRunningTotal;
	static float ms_fUpdateTimeForAllPeds;
	static bool ms_bLockPlayerInput;
#endif // __BANK
};

#endif //TASK_MOTION_BASE_H
