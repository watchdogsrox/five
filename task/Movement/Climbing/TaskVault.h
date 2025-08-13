//
// task/taskvault.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_VAULT_H
#define TASK_VAULT_H

// Game headers
#include "Task/Movement/Climbing/ClimbHandHold.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"
#include "network/General/NetworkUtil.h"
#include "Task/System/Tuning.h"

enum VaultFlags
{
	// Control flags
	VF_RunningJumpVault				= BIT0,		// Indicates we are doing a running jump climb/vault.
	VF_AutoVault					= BIT1,		// Was triggered from auto-vault.
	VF_VaultFromCover				= BIT2,		// Was triggered from auto-vault.
	VF_ParachuteClamber				= BIT3,		// Go to parachute clamber pose.
	VF_DiveClamber					= BIT4,		// Go to dive clamber pose.
	VF_DontOrientToHandhold			= BIT5,		// Do not orient the ped to the handhold.
	VF_DontDisableVaultOver			= BIT6,		// Don't prefer to climb on top of something over vaulting over it.
    VF_CloneUseLocalHandhold        = BIT7,     // If running as a clone task use the local detected handhold rather than trying to find a new one
};

class CTaskVault : public CTaskFSMClone
{
	friend class CClonedTaskVaultInfo;

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_AngledClimbTheshold;
		float m_MinAngleForScaleVelocityExtension;
		float m_MaxAngleForScaleVelocityExtension;
		float m_AngledClimbScaleVelocityExtensionMax;
		float m_DisableVaultForwardDot;
		float m_SlideWalkAnimRate;

		PAR_PARSABLE;
	};

	enum eVaultSpeed
	{
		VaultSpeed_Stand = 0,
		VaultSpeed_Walk,
		VaultSpeed_Run,
	};

	CTaskVault(const CClimbHandHoldDetected& handHold, fwFlags8 iFlags = 0);
	
	virtual ~CTaskVault();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VAULT; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskVault(*m_pHandHold, m_iFlags); }

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// Clone task implementation for CTaskVault
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
	virtual bool                OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed));
	virtual bool				OverridesCollision()		{ return true; }
	virtual bool				OverridesVehicleState() const	{ return true; }
    virtual bool                UseInAirBlender() const { return GetState() == State_Falling; }
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType)	{ return false; }
	virtual bool                AllowsLocalHitReactions() const { return false; }

	void	AdjustSyncedValuesForVehicleOffset();
	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Access the hand hold
	const CClimbHandHoldDetected& GetHandHold() const { return *m_pHandHold; }

	void SetRateMultiplier(float fRate) {m_fRateMultiplier = fRate;}

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns true if we can break out to perform a new movement task.
	bool CanBreakoutToMovementTask() const;

	// PURPOSE: Determine if the ped vault speed type.
	eVaultSpeed GetVaultSpeed() const;

	bool IsVaultExitFlagSet(u32 eFlag) const { return m_nVaultExitConditions.IsFlagSet(1 << eFlag); }

	bool IsStepUp() const;

	bool CanUseFPSHeadBound() const { return m_bCanUseFPSHeadBound; }

#if __BANK
	// PURPOSE: Expose variables to RAG
	static void AddWidgets(bkBank& bank);

	// PURPOSE: Dump variables to log.
	void DumpToLog();

#endif // __BANK

	// PURPOSE: Check if we are using the auto vault
	static bank_bool GetUsingAutoVault();

	// PURPOSE: Check if we are using the step up
	static bank_bool GetUsingAutoStepUp();

	// PURPOSE: Check if we are using the new vault task
	static bank_bool GetUsingAutoVaultOnPress() { return ms_bUseAutoVaultButtonPress; }

	// PURPOSE: Check that the given handhold is supported by our task. It may not be, given the limited range of anims that we have
	// to play.
	static bool IsHandholdSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed,
		bool bJumpVault = false, 
		bool bAutoVault = false,
		bool bDoDeferredTests = false,
		bool *pDisableAutoVaultDelay = NULL);

	static bool IsSlideSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed,
		bool bJumpVault = false, 
		bool bAutoVault = false);

	static void SetUpRagdollOnCollision(CPed *pPed, CPhysical *pIgnorePhysical, bool bIgnoreShins = false);
	static void ResetRagdollOnCollision(CPed *pPed);

	// TO DO - Move to tunable file.
	static const float ms_fStepUpHeight;

	enum State
	{
		State_Init,
		State_Climb,
		State_ClamberVault,
		State_Falling,
		State_ClamberStand,
		State_Ragdoll,
		State_Slide,
		State_ClamberJump,
		State_Quit,
	};

	enum eVaultExitConditions {
		eVEC_StandLow		= 0,
		eVEC_RunningLow		= 1,
		eVEC_Default		= 2,
		eVEC_Shallow		= 3,
		eVEC_Wide			= 4,
		eVEC_UberWide		= 5,
		eVEC_Slide			= 6,
		eVEC_Parachute		= 7,
		eVEC_Dive			= 8,
		eVEC_Max			= 9,
	};

protected:

	enum eClimbExitConditions {
		eCEC_Default		= 0,
		eCEC_Max			= 1,
	};

	struct tVaultExitCondition {
		float fMinHeight;
		float fMaxHeight;
		float fMinDepth;
		float fMaxStandDepth;
		float fMaxClamberDepth;
		float fMinDrop;
		float fMaxDrop;
		float fVerticalClearance;
		float fHorizontalClearance;
		bool  bJumpVaultSupported;
		bool  bDeferTest;
		bool  bDisableDelay;
		bool  bDisableAutoVault;

		fwMvFlagId flagId;

		bool IsSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed,
			bool bJumpVault, 
			bool bAutoVault,
			bool bDoDeferredTest = false);
	};

	struct tClimbStandCondition {
		float fVerticalClearance;
		float fMinStandDepth;
		float fMinClamberDepth;
		float fMaxDrop;
		float fHorizontalClearance;
		float fHorizontalClearanceFirstPerson;
		fwMvFlagId flagId;

		bool IsSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed);
	};

	enum eHandIK
	{
		HandIK_Left = 0,
		HandIK_Right,
		HandIK_Max
	};

	struct tHandIK
	{
		tHandIK() : vPosition(Vector3::ZeroType), vInitialPosition(Vector3::ZeroType), pRelativePhysical(NULL), bSet(false), bDoTest(true), bFinished(false), nTimeSet(0) {}
		Vector3 vInitialPosition;
		Vector3 vPosition;
		u32 nTimeSet;
		RegdPhy pRelativePhysical;
		float fTimeActive;
		bool bSet : 1;
		bool bDoTest : 1;
		bool bFinished : 1;
	};

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

private:

	////////////////////////////////////////////////////////////////////////////////

	FSM_Return StateInit_OnEnter();
	FSM_Return StateInit_OnUpdate();
	FSM_Return StateClimb_OnEnter();
	FSM_Return StateClimb_OnUpdate();
	FSM_Return StateClamberVault_OnEnter();
	FSM_Return StateClamberVault_OnUpdate();
	FSM_Return StateFalling_OnEnter();
	FSM_Return StateFalling_OnUpdate();
	FSM_Return StateClamberStand_OnEnter();
	FSM_Return StateClamberStand_OnUpdate();
	FSM_Return StateRagdoll_OnEnter();
	FSM_Return StateRagdoll_OnUpdate();
	FSM_Return StateSlide_OnEnter();
	FSM_Return StateSlide_OnUpdate();
	FSM_Return StateClamberJump_OnEnter();
	FSM_Return StateClamberJump_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Get the active clip
	const crClip* GetActiveClip(s32 iIndex = 0, s32 iForcedState = -1) const;

	// PURPOSE: Get the active phase
	float GetActivePhase(s32 iIndex = 0) const;

	// PURPOSE: Get the active weight
	float GetActiveWeight() const;

	// PURPOSE: Send the MoVE signals
	void UpdateMoVE();

	// PURPOSE: Arm IK for attaching hands to the object being vaulted
	void ProcessHandIK(CPed *pPed);

	// PURPOSE: Get total animated displacement/movement in anim.
	void CalcTotalAnimatedDisplacement();

	// PURPOSE: Get total animated displacement.
	Vector3 GetTotalAnimatedDisplacement(bool bWorldSpace = true) const;

	// PURPOSE: Setup the window where we scale the desired velocity
	void SetDesiredVelocityAdjustmentRange(const crClip* pClip, float fStart, float fEnd);

	// PURPOSE: Modify the desired velocity to get where we want to go
	bool ScaleDesiredVelocity(float fTimeStep);

	// PURPOSE: Determine if we should enter ragdoll
	bool ComputeAttachmentBreakOff(bool bTestClearance = false);

	// PURPOSE: Determine if we should interrupt the task and quit
	bool ComputeShouldInterrupt() const;

	// PURPOSE: Determine if the ped is on the ground
	bool ComputeIsOnGround() const;

	// PURPOSE: Gets handhold position/normal in world space.
	Vector3 GetHandholdPoint() const;
	Vector3 GetHandholdNormal() const;

	// PURPOSE: Get Hand ik position in world space.
	Vector3 GetHandIKPosWS(eHandIK eIK) const;

	// PURPOSE: Get Initial Hand ik position in world space.
	Vector3 GetHandIKInitialPosWS(eHandIK eIK) const;

	// PURPOSE: Set IK position for each hand.
	void SetHandIKPos(eHandIK eIK, CEntity *pEntity, const Vector3 &vWSPos);

	// PURPOSE: Update IK position for each hand.
	void UpdateHandIKPos(eHandIK eIK, const Vector3 &vWSPos);

	// PURPOSE: Update IK for each hand.
	void UpdateHandIK(eHandIK handIndex, int nHandBone, int nHelperBone);

	// PURPOSE: Determine if ped has moved outwith/external to this task.
	bool HasPedWarped() const;

	// PURPOSE: Is ped attached to handhold physical.
	bool GetIsAttachedToPhysical() const;

	bool IsClimbExitFlagSet(u32 eFlag) { return m_nClimbExitConditions.IsFlagSet(1 << eFlag); }

	void ClearVaultExitFlagSet(u32 eFlag) { m_nVaultExitConditions.ClearFlag(1 << eFlag); }
	void ClearClimbExitFlagSet(u32 eFlag) { m_nClimbExitConditions.ClearFlag(1 << eFlag); }

	void SetVaultExitFlagSet(u32 eFlag) { m_nVaultExitConditions.SetFlag(1 << eFlag); }
	void SetClimbExitFlagSet(u32 eFlag) { m_nClimbExitConditions.SetFlag(1 << eFlag); }

	void SetUpperBodyClipSet();
	void ProcessFPMoveSignals();

	bool IsSlideDirectionClear() const;

	Vector3 GetDropPointForSlide() const;

	Vector3 GetPedPositionIncludingAttachment() const;

	void ProcessDisableVaulting();

	void UpdateFPSHeadBound();

	static bool IsPushingStickInDirection(const CPed* pPed, float fTestDot);

	const float GetArcadeAbilityModifier() const;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Hand IK params.
	tHandIK m_HandIK[HandIK_Max];

	// PURPOSE: Lerp attach orientation, so we don't snap.
	Quaternion m_TargetAttachOrientation;

	// PURPOSE: Store the initial ped position
	Vector3 m_vInitialPedPos;

	// PURPOSE: The desired distance to move, as opposed to the animated distance
	Vector3 m_vTargetDist;
	Vector3 m_vTargetDistToDropPoint;

	// PURPOSE: Cache last velocity, so we can use it to prevent ped losing momentum at end of launch clip.
	Vector3 m_vLastAnimatedVelocity;

	// PURPOSE: Start + end phase values for x,y,z components.
	Vector3 m_vClipAdjustStartPhase;
	Vector3 m_vClipAdjustEndPhase;

	// Purpose: Cached animated displacement.
	Vector3 m_vTotalAnimatedDisplacement;

	// PURPOSE: For putting clones at correct start position
	Vector3	m_vCloneStartPosition;
	Vector3	m_vCloneStartDirection;
	Vector3	m_vCloneHandHoldPoint;
	Vector3	m_vCloneHandHoldNormal;
	float   m_fCloneClimbAngle;
	RegdPhy m_pClonePhysical;
	u16		m_uClonePhysicalComponent;
	u8		m_uCloneMinNumDepthTests;

	// PURPOSE: The current vault hand hold
	CClimbHandHoldDetected* m_pHandHold;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Add Ref for swim clip set (for swimming vaults).
	fwClipSetRequestHelper m_SwimClipSetRequestHelper;

	// Clip clipSetId to use for synced partial weapon clips
	fwClipSetRequestHelper m_UpperBodyClipSetRequestHelper;
	fwMvClipSetId m_UpperBodyClipSetId;

	// The filter to be used for weapon clips 
	fwMvFilterId m_UpperBodyFilterId;
	crFrameFilterMultiWeight* m_pUpperBodyFilter;

	// PURPOSE: Store the initial ped MBR
	float m_fInitialMBR;

	// PURPOSE: Store the initial ped XY distance to the hand hold
	float m_fInitialDistance;

	// PURPOSE: The climb rate
	float m_fClimbRate;

	// PURPOSE: The target rate
	float m_fClimbTargetRate;

	// PURPOSE: The rate to use when velocity scaling
	float m_fClimbScaleRate;

	// PURPOSE: Scale the rate we pass in to the climb movenetwork
	float m_fRateMultiplier;

	// PURPOSE: Store the last phase of the current clip
	float m_fClipLastPhase;

	// PURPOSE: Determine the start phase when we should adjust the animated velocity
	float m_fClipAdjustStartPhase;

	// PURPOSE: Determine the end phase when we should adjust the animated velocity
	float m_fClipAdjustEndPhase;

	// PURPOSE: The phase when to start the velocity scale
	float m_fStartScalePhase;

	// PURPOSE: The phase when we turn collision back on
	float m_fTurnOnCollisionPhase;
	float m_fDetachPhase;

	// PURPOSE: The phase to show weapon at.
	float m_fShowWeaponPhase;
	
	// PURPOSE: Next state to move to.
	s32 m_ProcessSignalsNextState;

	// PURPOSE: Bit flags (that can be passed in and easily replicated across the network).
	fwFlags8 m_iFlags;

	// Flags
	bool m_bNetworkBlendedIn : 1;		// PURPOSE: Flag whether we have blended in the network yet
	bool m_bSwimmingVault : 1;
	bool m_bLeftFootVault : 1;
	bool m_bLegIKDisabled : 1;
	bool m_bArmIKEnabled : 1;
	bool m_bPelvisFixup : 1;
	bool m_bCloneTaskNoLongerRunningOnOwner :1;
	bool m_bDisableVault : 1;
	bool m_bHandholdSlopesLeft : 1;
	bool m_bSlideVault : 1;
	bool m_bCloneClimbStandCondition :1;
	bool m_bCloneUseTestDirection : 1;
	bool m_bCloneUsedSlide : 1;
	bool m_bDeferredTestCompleted : 1;
	bool m_bCanUseFPSHeadBound : 1;
	bool m_bHasFPSUpperBodyAnim : 1;

	u16 m_iCloneUsesVaultExitCondition;

	fwFlags16 m_nClimbExitConditions;
	fwFlags16 m_nVaultExitConditions;

	eVaultSpeed m_eVaultSpeed;

	// PURPOSE: Use auto vault
	static bank_bool ms_bUseAutoVault;

	// PURPOSE: Use auto jump press vault - disables jump
	static bank_bool ms_bUseAutoVaultButtonPress;

	// PURPOSE: Use auto vault for step ups only.
	static bank_bool ms_bUseAutoVaultStepUp;

	static const fwMvBooleanId ms_EndFinishedId;
	static const fwMvBooleanId ms_EnterEndVaultId;
	static const fwMvBooleanId ms_EnterFallId;
	static const fwMvBooleanId ms_SlideStartedId;
	static const fwMvBooleanId ms_SlideExitStartedId;
	static const fwMvBooleanId ms_RunInterrupt;
	static const fwMvBooleanId ms_WalkInterrupt;
	static const fwMvBooleanId ms_DisableLegIKId;
	static const fwMvBooleanId ms_EnableLegIKId;
	static const fwMvBooleanId ms_EnableArmIKId;
	static const fwMvBooleanId ms_DisableArmIKId;
	static const fwMvBooleanId ms_EnablePelvisFixupId;
	static const fwMvBooleanId ms_EnableHeadingUpdateId;
	static const fwMvBooleanId ms_VaultLandWindowId;
	static const fwMvBooleanId ms_LandRightFootId;

	static const fwMvClipId ms_EndClipId;
	static const fwMvClipId ms_FallClipId;
	static const fwMvClipId ms_StartClip1Id;
	static const fwMvClipId ms_StartClip2Id;
	static const fwMvClipId ms_SlideClipId;
	static const fwMvFloatId ms_DepthId;
	static const fwMvFloatId ms_DistanceId;
	static const fwMvFloatId ms_DropHeightId;
	static const fwMvFloatId ms_ClearanceHeightId;
	static const fwMvFloatId ms_ClearanceWidthId;
	static const fwMvFloatId ms_EndPhaseId;
	static const fwMvFloatId ms_FallPhaseId;
	static const fwMvFloatId ms_SlidePhaseId;
	static const fwMvFloatId ms_HeightId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_StartPhase1Id;
	static const fwMvFloatId ms_StartPhase2Id;
	static const fwMvFloatId ms_ClimbRunningRateId;
	static const fwMvFloatId ms_ClimbStandRateId;
	static const fwMvFloatId ms_ClamberRateId;
	static const fwMvFloatId ms_ClimbSlideRateId;
	static const fwMvFloatId ms_StartWeightId;
	static const fwMvFloatId ms_SwimmingId;
	static const fwMvFlagId ms_AngledClimbId;
	static const fwMvFlagId ms_LeftFootVaultFlagId;
	static const fwMvFlagId ms_JumpVaultFlagId;
	static const fwMvFlagId ms_SlideLeftFlagId;
	static const fwMvFlagId ms_EnabledUpperBodyFlagId;
	static const fwMvFlagId ms_EnabledIKBounceFlagId;
	static const fwMvClipSetVarId ms_UpperBodyClipSetId;
	static const fwMvFilterId ms_UpperBodyFilterId;
	static const fwMvFlagId		ms_FirstPersonMode;
	static const fwMvFloatId	ms_PitchId;
	static const fwMvClipSetVarId	ms_FPSUpperBodyClipSetId;
	static const fwMvClipId		ms_WeaponBlockedClipId;

	static tVaultExitCondition ms_VaultExitConditions[eVEC_Max];
	static tClimbStandCondition ms_ClimbStandConditions[eCEC_Max];

	// our tunables
	static Tunables				sm_Tunables;
};

//-------------------------------------------------------------------------
// Task info for vault
//-------------------------------------------------------------------------
class CClonedTaskVaultInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskVaultInfo();
	CClonedTaskVaultInfo(s32            state,
                         const Vector3 &vPedStartPosition,
						 const Vector3 &vHandHoldPoint,
						 const Vector3 &vHandHoldNormal,
                         const Vector3 &vPedStartDirection,
		                 CEntity       *pPhysicalEntity, 
		                 u8             iVaultSpeed, 
		                 u8             iFlags,
						 u16			iExitCondition,
                         float          fInitialDistance,
		                 float          fRateMultiplier,
						 float			fHandHoldClimbAngle,
						 bool			bDisableVault,
						 bool			bClimbStandCondition,
						 bool			bCloneUseTestDirection,
						 bool			bCloneUsedSlide,
						 u16			iPhysicalComponent,
						 u8				uMinNumDepthTests);
	~CClonedTaskVaultInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_VAULT;}

	const Vector3  &GetStartPos()        const { return m_vPedStartPosition; }
	const Vector3  &GetStartDir()    const		{ return m_vPedStartDirection; }
	const Vector3  &GetHandHoldPoint()   const { return m_vHandHoldPoint; }
	const Vector3  &GetHandHoldNormal()  const { return m_vHandHoldNormal; }
	u8	     GetVaultSpeed()      const { return m_iVaultSpeed;		 }
	u8		 GetFlags()           const	{ return m_iFlags;			 }	
	u16		 GetExitCondition()   const	{ return m_iVaultExitCondition;	 }	
    float    GetInitialDistance() const { return m_fInitialDistance; }
	float    GetRateMultiplier()  const	{ return m_fRateMultiplier;  }
	float    GetHandHoldClimbAngle()  const	{ return m_fHandHoldClimbAngle;  }
    CEntity *GetPhysicalEntity()	    { return m_pPhysicalEntity.GetEntity(); }
	bool	 GetDisableVault()   const  { return m_bDisableVault;	 }
	bool	 GetClimbStandCondition()   const  { return m_bClimbStandCondition;	 }
	bool	 GetCloneUseTestDirection()   const  { return m_bCloneUseTestDirection;	 }
	bool	 GetCloneUsedSlide()   const  { return m_bCloneUsedSlide;	 }
	u16		 GetPhysicalComponent() const { return m_iPhysicalComponent; }
	u8		 GetMinNumDepthTests() const { return m_uMinNumDepthTests; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskVault::State_Quit>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Vault State";}

	void Serialise(CSyncDataBase& serialiser)
	{
        const float MAX_DISTANCE = 5.2f; //Accommodates ms_DefaultHorizontalTestDistance in ClimbDetector
		const float MAX_VEHICLE_OFFSET_DISTANCE = 80.0f;  //allow for large vehicles such as freight cars

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bPosIsOffset = m_bPosIsOffset;
		SERIALISE_BOOL(serialiser, bPosIsOffset, "Pos is offset");
		m_bPosIsOffset = bPosIsOffset;

		bool bClimbStandCondition = m_bClimbStandCondition;
		SERIALISE_BOOL(serialiser, bClimbStandCondition, "Climb stand condition");
		m_bClimbStandCondition = bClimbStandCondition;

		bool bCloneUseTestDirection = m_bCloneUseTestDirection;
		SERIALISE_BOOL(serialiser, bCloneUseTestDirection, "Climb detector use test direction");
		m_bCloneUseTestDirection = bCloneUseTestDirection;

		bool bCloneUsedSlide = m_bCloneUsedSlide;
		SERIALISE_BOOL(serialiser, bCloneUsedSlide, "Used slide");
		m_bCloneUsedSlide = bCloneUsedSlide;
		
		SERIALISE_VECTOR(serialiser, m_vHandHoldNormal,	1.1f, SIZEOF_HANDHOLD_NORMAL,	"Handhold normal" );
		SERIALISE_VECTOR(serialiser, m_vHandHoldPoint,	WORLDLIMITS_XMAX,	SIZEOF_HANDHOLD_POINT,	"Handhold point" );

		if(!m_bPosIsOffset || serialiser.GetIsMaximumSizeSerialiser())
		{   //SerialisePosition uses 19 bits per float so it's bigger than SIZEOF_OFFSFET_POS currently
			SERIALISE_POSITION(serialiser, m_vPedStartPosition,		"Ped start position" );
		}
		else
		{
			SERIALISE_VECTOR(serialiser, m_vPedStartPosition, MAX_VEHICLE_OFFSET_DISTANCE, SIZEOF_OFFSFET_POS,	"Ped start position offset" );
		}
		SERIALISE_VECTOR(serialiser, m_vPedStartDirection,	1.1f, SIZEOF_HANDHOLD_NORMAL,	"Ped direction" );
		SERIALISE_PACKED_FLOAT(serialiser, m_fHandHoldClimbAngle, 90.0f /*degrees*/, SIZEOF_CLIMB_ANGLE, "Hand hold climb angle");

		bool bHasEntity = m_pPhysicalEntity.GetEntity()!=NULL;

		SERIALISE_BOOL(serialiser, bHasEntity, "Has attach entity");
		if(bHasEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			ObjectId targetID = m_pPhysicalEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "attach entity");
			m_pPhysicalEntity.SetEntityID(targetID);
		}

		SERIALISE_UNSIGNED(serialiser, m_iVaultSpeed, SIZEOF_VAULT_SPEED, "speed");
		SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Vault Flags");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fInitialDistance, MAX_DISTANCE, SIZEOF_DIST, "Initial Distance");
        SERIALISE_PACKED_FLOAT(serialiser, m_fRateMultiplier, 2.0f, SIZEOF_RATE, "Rate");
		SERIALISE_UNSIGNED(serialiser, m_iVaultExitCondition, SIZEOF_EXIT_CONDITION, "Vault Exit Condition");

		bool bDisableVault = m_bDisableVault;
		SERIALISE_BOOL(serialiser, bDisableVault, "Disable Vault");
		m_bDisableVault = bDisableVault;

		SERIALISE_UNSIGNED(serialiser, m_iPhysicalComponent, 16, "Physical Component");
		SERIALISE_UNSIGNED(serialiser, m_uMinNumDepthTests, SIZEOF_MIN_NUM_DEPTH_TESTS, "Min number of depth tests");
	}

private:
	static const unsigned SIZEOF_HANDHOLD_POINT    = 32;
	static const unsigned SIZEOF_HANDHOLD_NORMAL   = 8;
	static const unsigned SIZEOF_OFFSFET_POS    = 16;
	static const unsigned SIZEOF_HEADING       = 16;
	static const unsigned SIZEOF_INITIAL_SPEED = 16;
	static const unsigned SIZEOF_CLIMB_ANGLE   = 10;
	static const unsigned SIZEOF_VAULT_SPEED   = 3;
	static const unsigned SIZEOF_FLAGS         = 8;
    static const unsigned SIZEOF_DIST          = 10;
	static const unsigned SIZEOF_RATE          = 16;
	static const unsigned SIZEOF_EXIT_CONDITION  = CTaskVault::eVEC_Max;
	static const unsigned SIZEOF_MIN_NUM_DEPTH_TESTS = 3; //In CClimbDetector::ValidateHandHoldAndDetermineVolume, currently Dec 2013, need 3 bits -> static const u32	DEPTH_TESTS = 5;

	Vector3		m_vHandHoldPoint;
	Vector3		m_vHandHoldNormal;
	Vector3		m_vPedStartPosition;
	Vector3		m_vPedStartDirection;
	CSyncedEntity m_pPhysicalEntity;
	u8			m_iVaultSpeed;
	u8			m_iFlags;
	u16			m_iVaultExitCondition;
	u16			m_iPhysicalComponent;
	u8			m_uMinNumDepthTests;  //force the clone to do the same number of tests that were done on the remote so that gaps are not seen locally that were missed on the remote

    float       m_fInitialDistance;
	float		m_fRateMultiplier;
	float		m_fHandHoldClimbAngle;
	bool		m_bDisableVault : 1;
	bool		m_bPosIsOffset : 1;
	bool		m_bClimbStandCondition : 1;
	bool		m_bCloneUseTestDirection :1;
	bool		m_bCloneUsedSlide :1;
};

#endif // TASK_VAULT_H
