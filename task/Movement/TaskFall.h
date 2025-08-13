#ifndef _TASK_FALL_H_
#define _TASK_FALL_H_

#include "Peds/ped.h"
#include "task/system/Task.h"
#include "task/Motion/TaskMotionBase.h"

enum FallFlags
{
	FF_ForceJumpFallLeft			= BIT0,		// Initially perform a jump fall (left foot).
	FF_ForceJumpFallRight			= BIT1,		// Initially perform a jump fall (right foot).
	FF_ForceSkyDive					= BIT2,		// Force the player to go straight to parachute skydiving
	FF_ForceHighFallNM				= BIT3,		// Force the player to go straight to parachute skydiving
	FF_VaultFall					= BIT4,		// Indicates fall is from a vault.
	FF_Plummeting					= BIT5,		// Indicates we are doing a high fall (but with no ragdoll).
	FF_DoneScream					= BIT6,		// Perform Audio scream when plummeting
	FF_ForceDive					= BIT7,		// Force the ped to use dive animations
	FF_DisableSkydiveTransition		= BIT8,		// Disables skydive transitions
	FF_LongBlendOnHighFalls			= BIT9,		// Increase blend time on high falls.
	FF_DisableNMFall				= BIT10,	// Disable NM Fall.
	FF_HasAirControl				= BIT11,	// Give air control to ped.
	FF_DampZVelocity				= BIT12,	// Damp in air z velocity.
	FF_CheckForDiveOrRagdoll		= BIT13,	// Allow ped to check for dive. If we fail, we NM.
	FF_LongerParachuteBlend			= BIT14,	// Force a longer blend for parachute state.
	FF_UseVaultFall					= BIT15,	// Use vault fall anims.
	FF_BeastFall					= BIT16,	// Use vault fall anims.
	FF_InstantBlend					= BIT17,	// Insta blend.
	FF_DivingOutOfVehicle			= BIT18,	// Dive out of vehicle.
	FF_CheckForGroundOnEntry		= BIT19,	// Bail if we are already at ground.
	FF_DisableLandRoll				= BIT20,	// Dont allow land roll.
	FF_CloneUsingAnimatedFallback	= BIT21,	// Set on the clone ped when the remote ped is doing a ragdoll high fall, but no local ragdoll is available
	FF_SkipToLand					= BIT22,	// Go straight to land.
	FF_PedIsStuck					= BIT23,	// Ped is stuck in fall (e.g. caught between geometry).
	FF_DontAbortRagdollOnCleanup	= BIT24,	// Another task is taking control of the ragdoll (so don't abort it when cleaning up)
	FF_DontClearInAirFlagOnCleanup	= BIT25,	// Set when the task is being swapped for a clone task or vice versa
	FF_DisableParachuteTask			= BIT26,	// Prevents NM task from starting parachute task

	FF_MAX_NUM_BITS = FF_InstantBlend   // Max bits for serializing
};

class CTaskFall : public CTaskFSMClone
{
public:

	CTaskFall(fwFlags32 iFlags = 0);
	virtual ~CTaskFall();

	struct Tunables : CTuning
	{
		Tunables();

		float m_ImmediateHighFallSpeedPlayer;
		float m_ImmediateHighFallSpeedAi;
		
		float m_HighFallProbeLength;

		float m_ContinuousGapHighFallSpeed;
		float m_ContinuousGapHighFallTime;

		float m_DeferFallBlockTestAngle;
		float m_DeferFallBlockTestDistance;
		float m_DeferFallBlockTestRadius;
		float m_DeferHighFallTime;

		float m_LandHeadingModifier;
		float m_StandingLandHeadingModifier;

		float m_InAirHeadingRate;
		float m_InAirMovementRate;
		float m_InAirMovementApproachRate;

		float m_FallLandThreshold;
		float m_ReenterFallLandThreshold;

		float m_PadShakeMinIntensity;
		float m_PadShakeMaxIntensity;
		float m_PadShakeMinHeight;
		float m_PadShakeMaxHeight;
		s32 m_PadShakeMinDuration;
		s32 m_PadShakeMaxDuration;

		float m_SuperJumpIntensityMult;
		float m_SuperJumpDurationMult;

		float m_DiveControlMaxFallDistance;
		float m_DiveControlExtraDistanceForDiveFromVehicle;
		float m_DiveControlExtraDistanceBlendOutSpeed;
		float m_DiveWaterOffsetToHitFullyInControlWeight;

		float m_VaultFallTestAngle;
		float m_JumpFallTestAngle;
		float m_FallTestAngleBlendOutTime;

		float m_LandRollHeightFromVault;
		float m_LandRollHeightFromJump;
		float m_LandRollHeight;
		float m_LandRollSlopeThreshold;

		float m_HighFallWaterDepthMin;
		float m_HighFallWaterDepthMax;
		float m_HighFallExtraHeightWaterDepthMin;
		float m_HighFallExtraHeightWaterDepthMax;

		PAR_PARSABLE;
	};

	enum
	{
		State_Initial,
		State_Fall,
		State_HighFall,
		State_Dive,
		State_Parachute,
		State_Landing,
		State_CrashLanding,
		State_GetUp,
		State_Quit
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

	virtual s32			GetDefaultStateAfterAbort() const { return State_Quit; }
	virtual aiTask*		Copy() const { return rage_new CTaskFall(m_iFlags); }
	virtual s32			GetTaskTypeInternal() const { return CTaskTypes::TASK_FALL; }

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_networkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_networkHelper; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	virtual	void		CleanUp();

	virtual bool HandlesDeadPed() { return GetState()==State_Landing; }

	// Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
    virtual bool                UseInAirBlender() const { return GetState() < State_Landing; }
	virtual bool				HandlesRagdoll(const CPed*) const { return true; }
	virtual bool				IsInScope(const CPed* pPed);

	void						SetForceRunOnExit(bool bForceRun) { m_bForceRunOnExit = bForceRun; }
	void						SetForceWalkOnExit(bool bForceWalk) { m_bForceWalkOnExit = bForceWalk; }
	void						SetCloneVaultForceLand()			{ m_bCloneVaultForceLand = true; }

	void						SetHighFallNMDelay(float fDelay) { m_fHighFallNMDelay = fDelay; }

	static void					GenerateLandingNoise(CPed& ped);

	static void					DoLandingPadShake(CPed *pPed, float fInitialFallHeight, const fwFlags32 &iFlags);

	static float				GetFallLandThreshold();

	static float				GetHighFallHeight() { return sm_Tunables.m_HighFallProbeLength; }

	static void					UpdateHeading(CPed *pPed, const CControl *pControl, float fHeadingModifier);

	static bool					GetIsHighFall(CPed* pPed, bool bForceHighFallVelocity, float& fFallTime, float fFallAngle, float fWaterDepth, float fTimeStep);
	
	float						GetMaxFallDistance() const { return m_fMaxFallDistance; }

	// RETURNS: Returns true if we can break out to perform a new movement task.
	bool						CanBreakoutToMovementTask() const;

	bool						CanPedParachute(const CPed *pPed) const;

	bool						IsDiveFall() const {return GetState()==State_Dive;}

	void						SetDampZVelocity() { m_iFlags.SetFlag(FF_DampZVelocity); }

	void						SetDontAbortRagdollOnCleanUp() { m_iFlags.SetFlag(FF_DontAbortRagdollOnCleanup); }

	void						SetInitialPedZ(float fInitialZ) { m_fInitialPedZ = fInitialZ; }
	float						GetInitialPedZ() const { return m_fInitialPedZ; }

	static float				GetMaxPedFallHeight(const CPed *pPed);

	void SetPedChangingOwnership(bool val) { m_pedChangingOwnership = val; }
	bool IsPedChangingOwnership() { return m_pedChangingOwnership; }

	void CacheParachuteState(s32 oldState, ObjectId oldParachuteObjectId) { m_cachedParachuteState = oldState; m_cachedParachuteObjectId = oldParachuteObjectId; }

	static Tunables	sm_Tunables;

protected:

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool ProcessPostCamera();

private:

	FSM_Return			StateInitial_OnEnter(CPed *pPed);
	FSM_Return			StateInitial_OnUpdate(CPed *pPed);
	FSM_Return			StateInitial_OnExit(CPed *pPed);
	FSM_Return			StateFall_OnEnter(CPed *pPed);
	FSM_Return			StateFall_OnUpdate(CPed *pPed);
	FSM_Return			StateDive_OnEnter(CPed *pPed);
	FSM_Return			StateDive_OnUpdate(CPed *pPed);
	FSM_Return			StateCrashLanding_OnEnter(CPed *pPed);
	FSM_Return			StateCrashLanding_OnUpdate(CPed *pPed);
	FSM_Return			StateHighFall_OnEnter(CPed *pPed);
	FSM_Return			StateHighFall_OnUpdate(CPed *pPed);
	FSM_Return			StateParachute_OnEnter(CPed *pPed);
	FSM_Return			StateParachute_OnUpdate(CPed *pPed);
	FSM_Return			StateLanding_OnEnter(CPed *pPed);
	FSM_Return			StateLanding_OnUpdate(CPed *pPed);
	FSM_Return			StateLanding_OnExit(CPed *pPed);
	FSM_Return			StateGetUp_OnEnter(CPed *pPed);
	FSM_Return			StateGetUp_OnUpdate(CPed *pPed);

	FSM_Return			StateGetUp_OnEnterClone(CPed *pPed);
	FSM_Return			StateGetUp_OnUpdateClone(CPed *pPed);

	// Task support funcs
	void				UpdateMoVE();
	void				ApplyFallDamage(CPed* pPed);
	bool				GetHasInterruptedClip(const CPed* pPed) const;
	bool				IsJumpFall() const;
	bool				IsSkydiveFall() const;
	bool				IsVaultFall() const;
	bool				GetIsAiming() const;
	bool				IsTryingToMove(const CPed* pPed) const;
	bool				IsLandingWithParachute() const;

	float				GetLandRollHeight() const;

	// Update high fall status.
	bool ProcessHighFall(CPed* pPed);

	void CalculateFallDistance();

	void InitFallStateData(CPed *pPed);

	static Vector3 GetTestEndUsingAngle(const CPed *pPed, const Vector3 &vStart, float fAngle, float fTestLength);
	float GetInitialFallTestAngle() const;

	void CalculateLocomotionExitParams(const CPed *pPed);

	// Current clip set
	fwMvClipSetId		m_clipSetId;
	// Network Helper
	CMoveNetworkHelper	m_networkHelper;

	fwClipSetRequestHelper m_SwimClipSetRequestHelper; //for dives

	// Add Ref for upper body clip set.
	fwMvClipSetId			m_UpperBodyClipSetID;
	fwClipSetRequestHelper	m_UpperBodyClipSetRequestHelper;

	// Record the max fall speed for the duration of the task
	Vector3 m_vMaxFallSpeed;
	Vector3 m_vInitialFallVelocity;
	Vector3 m_vStickVelocity;

	struct tFallDistanceSample
	{
		float fZHeight;
		u32 nTime;
	};

	static const u32 s_nNumDistSamples = 10;
	tFallDistanceSample m_aDistanceFallen[s_nNumDistSamples];
	u32 m_nDistanceFallenIndex;
	bool m_bCanUseDistanceSamples;

	s32 m_cachedParachuteState;
	ObjectId m_cachedParachuteObjectId;

	// Flags
	fwFlags32			m_iFlags;

	// Task vars
	float				m_fFallTime;
	float				m_fMaxFallDistance;
	float				m_fWaterDepth;
	float				m_fFallDistanceRemaining;
	float				m_fInitialMBR;
	float				m_fInitialPedZ;
	float				m_fLastPedZ;
	float				m_fStuckTimer;
	float				m_fHighFallNMDelay;
	float				m_fIntroBlendTime;
	float				m_fExtraDistanceForDiveControl;
	float				m_fDiveControlWeighting;
	float				m_fCachedDiveWaterHeight;
	float				m_fFallTestAngle;
	float				m_fCloneWantsToLandTime;
    float               m_fAirControlStickXNormalised;
    float               m_fAirControlStickYNormalised;

	bool				m_bStandingLand : 1;
	bool				m_bOffGround : 1;
	bool				m_bForceRunOnExit : 1;
	bool				m_bForceWalkOnExit : 1;
	bool				m_bProcessInterrupt : 1;
	bool				m_bRestartIdle : 1;
	bool				m_bWalkLand : 1;
	bool				m_bSetInitialVelocity : 1;
	bool				m_bCloneVaultForceLand : 1;
	bool				m_bLandComplete : 1;
	bool				m_bMovementTaskBreakout : 1;
	bool				m_bUseLeftFoot : 1;
	bool				m_bLandRoll : 1;
    bool                m_bRemoteJumpButtonDown : 1;
	bool				m_bWantsToEnterVehicleOrCover : 1;
	bool				m_bAllowLocalCloneLand : 1;
	bool				m_bDeferHighFall : 1;
	bool				m_bDisableTagSync : 1;
	bool				m_pedChangingOwnership : 1;

	// MoVE Stuff

	static	const fwMvFlagId	ms_bOnGround;
	static	const fwMvFlagId	ms_bStandingLand;
	static	const fwMvFlagId	ms_bWalkLand;
	static	const fwMvFlagId	ms_bPlummeting;
	static	const fwMvFlagId	ms_bLandRoll;
	static  const fwMvFlagId	ms_bJumpFallLeft;
	static  const fwMvFlagId	ms_bJumpFallRight;
	static  const fwMvFlagId	ms_bDive;
	static  const fwMvFlagId	ms_bVaultFall;
	static	const fwMvFloatId	ms_fMaxFallDistance;
	static	const fwMvFloatId	ms_fFallDistanceRemaining;
	static	const fwMvFloatId   ms_fDiveControl;
	static	const fwMvFloatId	ms_fFallTime;
	static  const fwMvFloatId	ms_MoveSpeedId;
	static	const fwMvBooleanId ms_InterruptibleId;
	static	const fwMvBooleanId ms_bLandComplete;
	static	const fwMvBooleanId ms_AimInterruptId;
	
	static  const fwMvClipSetVarId ms_UpperBodyClipSetId;
};

//-------------------------------------------------------------------------
// Task info for TaskFall
//-------------------------------------------------------------------------
class CClonedFallInfo : public CSerialisedFSMTaskInfo
{
private:

	static const int SIZEOF_FLAGS = datBitsNeeded<FF_MAX_NUM_BITS>::COUNT;;
	static const int SIZEOF_INITIALMBR = 32; // NEED TO CHANGE!

public:

	CClonedFallInfo( s32 const _state, u32 const _flags, float const _initialMBR, bool bWalkLand, bool bStandingLand, bool bLandRoll, bool bJumpButtonDown, float fAirControlStickXNormalised, float fAirControlStickYNormalised );
	CClonedFallInfo();
	~CClonedFallInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_FALL;}
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool	HasState() const			{ return true; }
	virtual u32		GetSizeOfState() const		{ return datBitsNeeded<CTaskFall::State_Quit>::COUNT; }
	virtual const char*	GetStatusName(u32) const	{ return "Status";}

	inline u32		GetFlags(void) const		{ return static_cast<u32>(m_iFlags); }
	inline float	GetInitialMBR(void) const	{ return m_fInitialMBR; }
    float           GetAirControlStickXNormalised() const { return m_fAirControlStickXNormalised;}
    float           GetAirControlStickYNormalised() const { return m_fAirControlStickYNormalised;}

	bool		GetStandingLand() const { return m_bStandingLand; }
	bool		GetWalkLand() const { return m_bWalkLand; }
	bool		GetLandRoll() const { return m_bLandRoll; }
    bool        GetJumpButtonDown() const { return m_bJumpButtonDown; }
	
	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

        static const unsigned SIZEOF_STICK_VALUE = 8;

		SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Flags");
		SERIALISE_PACKED_FLOAT(serialiser, m_fInitialMBR, 100.0f, SIZEOF_INITIALMBR, "InitialMBR");
        SERIALISE_PACKED_FLOAT(serialiser, m_fAirControlStickXNormalised, 1.0f, SIZEOF_STICK_VALUE, "Air Control Stick X");
        SERIALISE_PACKED_FLOAT(serialiser, m_fAirControlStickYNormalised, 1.0f, SIZEOF_STICK_VALUE, "Air Control Stick Y");

		SERIALISE_BOOL(serialiser, m_bStandingLand, "Standing Land");
		SERIALISE_BOOL(serialiser, m_bWalkLand, "Walk Land");
		SERIALISE_BOOL(serialiser, m_bLandRoll, "Land Roll");
        SERIALISE_BOOL(serialiser, m_bJumpButtonDown, "Jump button down");
	}

private:

	CClonedFallInfo(const CClonedFallInfo &);
	CClonedFallInfo &operator=(const CClonedFallInfo &);

	float	m_fInitialMBR;
    float   m_fAirControlStickXNormalised;
    float   m_fAirControlStickYNormalised;
	u32		m_iFlags;

	bool	m_bStandingLand;
	bool	m_bWalkLand;
	bool	m_bLandRoll;
    bool    m_bJumpButtonDown;
};

#endif	//_TASK_FALL_H_
