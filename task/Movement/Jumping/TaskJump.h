#ifndef TASK_JUMP_H
#define TASK_JUMP_H

// Game headers
#include "Task/System/Task.h"
#include "GapHelper.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"

class CControl;

//////////////////////////////////////////////////////////////////////////
// JumpFlags
// Flags to control jump tasks
//////////////////////////////////////////////////////////////////////////

enum JumpFlags
{
	// Control flags
	JF_ForceVault					= BIT0,		// If there is a climb available, climbs up
	JF_DisableVault					= BIT1,		// Disables transitions to vaulting
	JF_DisableJumpingIfNoClimb		= BIT2,		// Prevent task from jumping if a climb cannot be found
	JF_AutoJump						= BIT3,		// Indicates we are automatically jumping over an obstacle.
	JF_JumpingUpHill				= BIT4,		// Jump is going uphill (to pick a different anim).
	JF_SteepHill					= BIT5,		// Jump is very steep. 
	JF_JumpLaunchFootSync			= BIT6,		// Do foot syncing (with motion anims) when launching
	JF_ScaleQuadrupedJump			= BIT7,		// Need to scale jump velocity to get over obstacles.
	JF_UseCachedHandhold			= BIT8,		// Use cached handhold, else we need to get a new one.
	JF_FromCover					= BIT9,		// Triggered from cover.
	JF_ForceParachuteJump			= BIT10,	// Force jump to be parachute version.
	JF_ForceDiveJump				= BIT11,	// Force jump to be dive version.
	JF_ForceNMFall					= BIT12,	// Force NM Fall behaviour.
	JF_ForceRunningLand				= BIT13,	// Force running land for small running drops.
	JF_ForceRightFootLand			= BIT14,	// Force a right foot land.
	JF_SuperJump					= BIT15,	// Do a super jump.
	JF_ShortJump					= BIT16,	// Used for shorter less significant jumps to get through small obstacles
	JF_BeastJump					= BIT17,	// Used for shorter less significant jumps to get through small obstacles
	JF_AIUseFullForceBeastJump		= BIT18,	// Make AI do a beast/super jump as if he was holding the jump button
};

//////////////////////////////////////////////////////////////////////////
// CTaskJumpVault
// Decision making task - works out which behaviour is required after pressing the jump button
//////////////////////////////////////////////////////////////////////////

class CTaskJumpVault : public CTask
{
public:

	CTaskJumpVault(fwFlags32 iFlags = 0);

	virtual void CleanUp();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskJumpVault(m_iFlags); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_JUMPVAULT; }

	// Static funcs to say if we will jump or vault from the flags passed in
	static bool WillVault(CPed* pPed, fwFlags32 iFlags);

	bool GetIsFalling();
	bool GetIsJumping() const {return GetState()==State_Jump;}
	void SetRateMultiplier(float fRate) {m_fRate = fRate;}

	bool CanBreakoutToMovementTask() const;

protected:

	typedef enum
	{
		State_Init = 0,				// Decides what to do
		State_Jump,					// Performs a jump
		State_Vault,				// Performs a vault
		State_Quit,					// Quit
	} State;

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	//
	// State functions
	//

	FSM_Return StateInit();
	FSM_Return StateJumpOnEnter();
	FSM_Return StateJumpOnUpdate();
	FSM_Return StateVaultOnEnter();
	FSM_Return StateVaultOnUpdate();

	void ProcessStreamingForParachute();

	//
	// Members
	//

	fwClipSetRequestHelper m_ClipSetRequestHelperForBase;
	fwClipSetRequestHelper m_ClipSetRequestHelperForLowLod;
	fwClipSetRequestHelper m_ClipSetRequestHelperForSkydiving;
	fwClipSetRequestHelper m_ClipSetRequestHelperForParachuting;

	strRequest m_ModelRequestHelper;

	// Flags
	fwFlags32 m_iFlags;
	float m_fRate;
};


//////////////////////////////////////////////////////////////////////////
// 
// 
//////////////////////////////////////////////////////////////////////////






//////////////////////////////////////////////////////////////////////////
// CTaskJump
// All the associated logic for jump/in-air/land encompassed in one task.
//////////////////////////////////////////////////////////////////////////

class CTaskJump : public CTaskFSMClone
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinSuperJumpInitialVelocity;
		float m_MaxSuperJumpInitialVelocity;

		float m_MinBeastJumpInitialVelocity;
		float m_MaxBeastJumpInitialVelocity;

		float m_HighJumpMinAngleForVelScale;
		float m_HighJumpMaxAngleForVelScale;
		float m_HighJumpMinVelScale;
		float m_HighJumpMaxVelScale;

		bool m_DisableJumpOnSteepStairs;
		float m_MaxStairsJumpAngle;

		bool  m_bEnableJumpCollisions;
		bool  m_bEnableJumpCollisionsMp;
		bool  m_bBlockJumpCollisionAgainstRagdollBlocked;
		float m_PredictiveProbeZOffset;
		float m_PredictiveBraceStartDelay;
		float m_PredictiveBraceProbeLength;
		float m_PredictiveBraceBlendInDuration;
		float m_PredictiveBraceBlendOutDuration;
		float m_PredictiveBraceMaxUpDotSlope;
		float m_PredictiveRagdollIntersectionDot;
		float m_PredictiveRagdollStartDelay;
		float m_PredictiveRagdollProbeLength;
		float m_PredictiveRagdollProbeRadius;
		float m_PredictiveRagdollRequiredVelocityMag;

		PAR_PARSABLE;
	};

	friend class CClonedTaskJumpInfo;

	CTaskJump(fwFlags32 iFlags = 0);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskJump(m_iFlags); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_JUMP; }

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
	virtual bool				OverridesVehicleState() const	{ return true; }
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool				IsInScope(const CPed* pPed);
	virtual bool                HasSyncedVelocity() const { return true; }
    virtual Vector3             GetSyncedVelocity() const { return m_vSyncedVelocity; }
	
	bool GetIsFalling() {return m_bIsFalling;}
	bool GetIsDivingLaunch() const {return m_bDivingLaunch;}
	bool GetIsSkydivingLaunch() const {return m_bSkydiveLaunch;}
	bool GetIsStateFall() const { return (GetState() == State_Fall); }
	bool GetIsDoingSuperJump() const { return m_iFlags.IsFlagSet(JF_SuperJump); }
	bool GetIsDoingBeastJump() const { return m_iFlags.IsFlagSet(JF_BeastJump); }

	float GetSyncedHeading() {return m_SynchedHeading; }
	fwFlags32* GetFlags() { return &m_iFlags; }

	static bool IS_FOOTSYNC_ENABLED();

	static bool IsPedNearGround(const CPed *pPed, float fLandHeightThreshold, const Vector3 &vOffset, float &fOutLandZ);
	static bool IsPedNearGround(const CPed *pPed, float fLandHeightThreshold, const Vector3 &vOffset = Vector3(Vector3::ZeroType));
	static bool IsPedNearGroundIncRadius(const CPed *pPed, float fLandHeightThreshold, float fLandRadius, const Vector3 &vOffset, float &fOutLandZ);

	static float GetMinHorizJumpDistance();
	static float GetMaxHorizJumpDistance(const CPed *pPed);
	static void DoJumpGapTest(const CPed* pPed, JumpAnalysis &jumpHelper, bool bShortJump = false, float fOverideMaxDist = -1.0f);
	static bool CanDoParachuteJumpFromPosition(CPed* pPed, bool bDoCanReachTest);
	static bool CanDoDiveFromPosition(CPed* pPed, bool bCanSkyDive);
	static Vec3V_Out GetJumpTestEndPosition(const CPed* pPed);

	bool CanBreakoutToMovementTask() const;
protected:

	enum State
	{
		State_Init = 0,
		State_Launch,
		State_Land,
		State_Fall,
		State_Quit,
	};

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool ProcessPostMovement();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

	bool IsMovingUpSlope(const Vector3 &vSlopeNormal) const;

	void CalculateJumpAngle();

	bool IsJumpingUpHill(float fJumpAngleMin, float fJumpAngleMax, float *pfHillAngle = NULL) const;

	float GetJumpLandThreshold() const;
	float GetEntitySetZThreshold() const;

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;
#endif // !__FINAL

private:

	// FSM optional functions
	virtual	void CleanUp();

	// Update MoVE
	void	UpdateMoVE();

	//
	// State functions
	//

	// Init
	FSM_Return StateInit_OnEnter(CPed* pPed);
	FSM_Return StateInit_OnUpdate(CPed* pPed);
	// Launch (and in-air)
	FSM_Return StateLaunch_OnEnter(CPed* pPed);
	FSM_Return StateLaunch_OnUpdate(CPed* pPed);
	FSM_Return StateLaunch_OnExit(CPed* pPed);
	// Landing
	FSM_Return StateLand_OnEnter(CPed* pPed);
	FSM_Return StateLand_OnUpdate(CPed* pPed);
	FSM_Return StateLand_OnExit(CPed* pPed);
	// Fall
	FSM_Return StateFall_OnEnter(CPed* pPed);
	FSM_Return StateFall_OnUpdate(CPed* pPed);
	FSM_Return StateFall_OnExit(CPed* pPed);

	// Collision queries to check if immediate jump area is clear before jumping
	bool GetIsStandingJumpBlocked(const CPed* pPed);
	bool GetHasInterruptedClip(const CPed* pPed) const;

	bool ScaleDesiredVelocity(CPed *pPed, float timestep);
	void CalculateHeightAdjustedDistance(CPed *pPed);

	bool CanSnapToGround(CPed* pPed, float fLandPosZ) const;

	float GetAssistedJumpLandPhase(const crClip* pClip) const;

	bool IsPedTryingToMove(bool bCheckForwardDirection = false) const;

	bool GetDetectedLandPosition(const CPed *pPed, Vector3 &vLandPosition) const;

	void InitVelocityScaling(CPed* pPed);

	const crClip *GetJumpClip() const;

	const CControl *GetControlFromPlayer(const CPed* pPed) const;

	bool HasReachedApex() const;

	static bool CheckForBlockedJump(const CPed* pPed, float fTestDistance=1.0f, float fTestRadius = 0.125f, float fTestOffset = -0.1f);
	bool TestForObstruction(float fTestDistance, float fTestRadius, float& distanceToObstruction, CEntity** pObstructionEntity, Vec3V& normal) const;
	
	void ActivateRagdollOnCollision(CPhysical *pPhysical, float fPenetration, float fSlope, bool bAllowFixed = true);
	void ResetRagdollOnCollision();

	//
	// Members
	//

	// Jump landing stuff
	JumpAnalysis		m_JumpHelper;

	// Current clip set
	fwMvClipSetId		m_clipSetId;
	fwMvClipSetId		m_UpperBodyClipSetID;

	// Network Helper
	CMoveNetworkHelper	m_networkHelper;

	// PURPOSE: Add Ref for upper body clip set.
	fwClipSetRequestHelper m_UpperBodyClipSetRequestHelper;

	Vector3				m_vScaleDistance;
	Vector3				m_vHandHoldPoint;
	Vector3				m_vInitialFallVelocity;
	Vector3				m_vMaxScalePhase;
	Vector3				m_vAnimRemainingDist;

	Vector3				m_vGroundPosStart;
	Vector3				m_vOwnerAssistedJumpStartPosition;
	Vector3				m_vOwnerAssistedJumpStartPositionOffset;

	float				m_fBackDist;				// This distance that we need to shorten the anim by to account for the landing spot not being on the same height
	float				m_fInitialPedZ;				
	float				m_fScaleVelocityStartPhase;
	float				m_fClipLastPhase;

	float				m_fQuadrupedJumpDistance;

	float				m_fApexClipPhase;

	float				m_fPedLandPosZ;

	float				m_fJumpAngle;

	float				m_fTurnPhaseLaunch;
	float				m_fTurnPhase;

#if FPS_MODE_SUPPORTED
	float				m_fFPSInitialDesiredHeading;
	float				m_fFPSExtraHeadingChange;
#endif // FPS_MODE_SUPPORTED
	Vector3 m_vSyncedVelocity; // The velocity the owner is travelling at - needed to get the clone at the same velocity on startup...
	float m_SynchedHeading;
	u32					m_RagdollStartTime;

	// Flags
	fwFlags32			m_iFlags;

	CGenericClipHelper  m_BraceClipHelper;

	// Flags for progression through the jump
	bool				m_bStandingJump : 1;			// A flag to specify if we want to do a standing jump
	bool				m_bStandingLand : 1;			// A flag to specify if we want to do a standing land
	bool				m_bSkydiveLaunch : 1;			// A flag to specify whether this is a skydive launch
	bool				m_bDivingLaunch : 1;			// A flag to specify whether this is a diving launch
	bool				m_bLeftFootLaunch : 1;			// A flag to specify which foot to launch to moving jump on
	bool				m_bOffGround : 1;
	bool				m_bIsFalling : 1;
	bool				m_IsQuadrupedJump : 1;
	bool				m_bScalingVelocity : 1;
	bool				m_bReactivateLegIK : 1;
	bool				m_bWaitingForJumpRelease : 1;
	bool				m_bTwoHandedJump : 1;
	bool				m_bUseLeftFoot : 1;
	bool				m_bTagSyncOnExit : 1;
	bool				m_bStuntJumpRequested : 1;
	bool				m_bJumpingUpwards : 1;
	bool				m_bUseProjectileGrip : 1;
#if FPS_MODE_SUPPORTED
	bool				m_bFPSAchievedInitialDesiredHeading : 1;
#endif // FPS_MODE_SUPPORTED
	bool				m_bStayStrafing : 1;
    bool                m_bHasAppliedSuperJumpVelocityLocally  : 1;
    bool                m_bHasAppliedSuperJumpVelocityRemotely : 1;
    bool                m_bUseMaximumSuperJumpVelocity : 1;

	//
	// Static Members
	//
	static const fwMvBooleanId	ms_ExitLaunchId;
	static const fwMvBooleanId	ms_AssistedJumpLandPhaseId;
	static const fwMvRequestId	ms_EnterLandId;
	static const fwMvBooleanId	ms_ExitLandId;
	static const atHashString	ms_RagdollOnCollisionId;
	static const fwMvBooleanId	ms_TakeOffId;
	static const fwMvBooleanId	ms_EnableLegIKId;
	static const fwMvFlagId		ms_StandingLaunchFlagId;
	static const fwMvFlagId		ms_StandingLandFlagId;
	static const fwMvFlagId		ms_SkydiveLaunchFlagId;
	static const fwMvFlagId		ms_DivingLaunchFlagId;	
	static const fwMvFlagId		ms_LeftFootLaunchFlagId;
	static const fwMvFlagId		ms_FallingFlagId;
	static const fwMvFlagId		ms_SkipLaunchFlagId;
	static const fwMvFlagId		ms_AutoJumpFlagId;
	static const fwMvFlagId		ms_ForceRightFootLandFlagId;
	static const fwMvFlagId		ms_UseProjectileGripFlagId;
	static const fwMvBooleanId	ms_InterruptibleId;
	static const fwMvBooleanId	ms_SlopeBreakoutId;
	static const fwMvBooleanId	ms_AimInterruptId;
	static const fwMvBooleanId	ms_StandLandWalkId;
	static const fwMvBooleanId	ms_ExitLaunchTag;

	static const fwMvFlagId		ms_PredictingJumpId;
	static const fwMvFloatId	ms_PredictedJumpDistanceId;
	static const fwMvFlagId		ms_JumpingUpHillId;
	static const fwMvFlagId		ms_ScalingJumpId;

	// TEMP
	static const fwMvClipId		ms_JumpClipId;
	static const fwMvFloatId	ms_LaunchPhaseId;
	static const fwMvFloatId	ms_MoveSpeedId;

	static const fwMvFloatId	ms_TurnPhaseLaunchId;
	static const fwMvFloatId	ms_TurnPhaseId;

	static const fwMvClipSetVarId	ms_UpperBodyClipSetId;

	// our tunables
	static Tunables				sm_Tunables;
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskJumpInfo
// Task serialization info for TaskJump2
//////////////////////////////////////////////////////////////////////////

class CClonedTaskJumpInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskJumpInfo();
	CClonedTaskJumpInfo(s32 state, 
		u32 flags, 
		const Vector3 &vHandHoldPoint, 
		float fQuadrupedJumpDistance, 
		bool bStandingLand, 
		float fJumpAngle,
		bool bAssistedJump,
		const Vector3 &vAssistedJumpPosition,
		const Vector3 &vAssistedJumpStartPosition,
		CPhysical *pAssistedJumpPhysical,
		float fAssistedJumpDistance
#if FPS_MODE_SUPPORTED
        , float fFPSInitialDesiredHeading
#endif // FPS_MODE_SUPPORTED
		, Vector3 velocity
		, float heading
        , bool bUseMaximumSuperJumpVelocity
        , bool bHasAppliedSuperJumpVelocityRemotely
        );
	~CClonedTaskJumpInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_JUMP; }

	u32		GetFlags() const				{ return m_Flags;	}
	const Vector3& GetHandholdPoint() const	{ return m_vHandHoldPoint;	}
	float GetQuadrupedJumpDistance() const	{ return m_fQuadrupedJumpDistance;	}
	bool GetStandingLand() const {return m_bStandingLand; }
	float GetJumpAngle() const {return m_fJumpAngle; }
	bool IsAssistedJump() const {return m_bAssistedJump; }
	float GetAssistedJumpDistance() const {return m_fAssistedJumpDistance; }
	const Vector3& GetAssistedJumpPosition() const {return m_vAssistedJumpPosition; }
	const Vector3& GetAssistedJumpStartPosition() const {return m_vAssistedJumpStartPosition; }
	const Vector3& GetAssistedJumpStartPositionOffset() const {return m_vAssistedJumpStartPositionOffset; }
	CEntity *GetAssistedJumpEntity()	    { return m_pAssistedJumpPhysical.GetEntity(); }
	inline Vector3 const&	GetPedVelocity() const				{ return m_pedVelocity; }
	inline float const&	GetPedHeading() const				{ return m_pedHeading; }
#if FPS_MODE_SUPPORTED
    float GetFPSInitialDesiredHeading() const { return m_fFPSInitialDesiredHeading; }
#endif // FPS_MODE_SUPPORTED
    bool IsUsingMaximumSuperJumpVelocity() const { return m_bUseMaximumSuperJumpVelocity; }
    bool HasAppliedSuperJumpVelocityRemotely() const { return m_bHasAppliedSuperJumpVelocityRemotely; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskJump::State_Quit>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Jump 2 State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_BOOL(serialiser, m_bStandingLand, "Standing Land");
		SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Jump Flags");
		SERIALISE_POSITION(serialiser, m_vHandHoldPoint,		"hand hold point" );
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fQuadrupedJumpDistance, 10.0f, SIZEOF_JUMP_DISTANCE, "quadruped jump distance" );
		SERIALISE_PACKED_FLOAT(serialiser, m_fJumpAngle, 90.0f, SIZEOF_JUMP_ANGLE, "jump angle");

		SERIALISE_BOOL(serialiser, m_bAssistedJump, "Assisted jump");
		if (m_bAssistedJump || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_POSITION(serialiser, m_vAssistedJumpPosition,		"Assisted land position" );
			SERIALISE_POSITION(serialiser, m_vAssistedJumpStartPosition,		"Assisted jump start position" );
			SERIALISE_POSITION(serialiser, m_vAssistedJumpStartPositionOffset,		"Assisted jump start position offset" );
			SERIALISE_PACKED_FLOAT(serialiser, m_fAssistedJumpDistance, 10.0f, SIZEOF_JUMP_DISTANCE, "Assisted jump distance");

			bool bHasEntity = m_pAssistedJumpPhysical.GetEntity()!=NULL;

			SERIALISE_BOOL(serialiser, bHasEntity, "Has attach entity");
			if(bHasEntity || serialiser.GetIsMaximumSizeSerialiser())
			{
				ObjectId targetID = m_pAssistedJumpPhysical.GetEntityID();
				SERIALISE_OBJECTID(serialiser, targetID, "Assisted jump attach entity");
				m_pAssistedJumpPhysical.SetEntityID(targetID);
			}
		}

#if FPS_MODE_SUPPORTED
        bool bHasFPSInitialDesiredHeading = m_fFPSInitialDesiredHeading != FLT_MAX;

        SERIALISE_BOOL(serialiser, bHasFPSInitialDesiredHeading, "Has FPS Initial Desired Heading");

        if(bHasFPSInitialDesiredHeading || serialiser.GetIsMaximumSizeSerialiser())
        {
            const unsigned SIZEOF_HEADING = 8;
            SERIALISE_PACKED_FLOAT(serialiser, m_fFPSInitialDesiredHeading, TWO_PI, SIZEOF_HEADING, "Target Heading");
        }
        else
        {
            m_fFPSInitialDesiredHeading = FLT_MAX;
        }
#endif // FPS_MODE_SUPPORTED
		
        const float MAX_VELOCITY_VALUE = 150.0f;

		SERIALISE_PACKED_FLOAT(serialiser, m_pedHeading, TWO_PI, SIZEOF_HEADING, "Ped heading");

		SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.x, MAX_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_X, "Ped Velocity X"); 
		SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.y, MAX_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_Y, "Ped Velocity Y"); 
		SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.z, MAX_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_Z, "Ped Velocity Z");

        SERIALISE_BOOL(serialiser, m_bUseMaximumSuperJumpVelocity, "Use Max Superjump Velocity");
        SERIALISE_BOOL(serialiser, m_bHasAppliedSuperJumpVelocityRemotely, "Has Applied Superjump Velocity Remotely");
	}

private:
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_X = 24;
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_Y = 24;
	static const unsigned int SIZEOF_PED_VELOCITY_COMP_Z = 24;
	static const unsigned int SIZEOF_FLAGS               = 32;
	static const unsigned int SIZEOF_JUMP_DISTANCE       = 16;
	static const unsigned int SIZEOF_JUMP_ANGLE          = 16;
    static const unsigned int SIZEOF_HEADING             = 8;
	u32			m_Flags;

	Vector3 m_vHandHoldPoint;
	float	m_fQuadrupedJumpDistance;
	bool	m_bStandingLand;
	float	m_fJumpAngle;

	Vector3 m_vAssistedJumpPosition;
	Vector3 m_vAssistedJumpStartPosition;
	Vector3 m_vAssistedJumpStartPositionOffset;
	Vector3 m_pedVelocity;
	float   m_pedHeading;
	bool    m_bAssistedJump;
	CSyncedEntity m_pAssistedJumpPhysical;
	float m_fAssistedJumpDistance;
	
#if FPS_MODE_SUPPORTED
    float m_fFPSInitialDesiredHeading;
#endif // FPS_MODE_SUPPORTED

    bool m_bUseMaximumSuperJumpVelocity;
    bool m_bHasAppliedSuperJumpVelocityRemotely;
};

#endif // TASK_JUMP_H
