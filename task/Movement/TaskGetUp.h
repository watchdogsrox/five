// Filename   :	TaskGetUp.h
// Description:	Task to handle blending from a potentially unknown pose back to a sensible in-game behaviour
//

#ifndef INC_TASKGETUP_H_
#define INC_TASKGETUP_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Game headers
#include "fwanimation/posematcher.h"
#include "peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/BlendFromNmData.h"
#include "Task/System/TaskHelpers.h"
#include "network/General/NetworkUtil.h"

// ------------------------------------------------------------------------------

class CTaskGetUp : public CTaskFSMClone
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fPreferInjuredGetupPlayerHealthThreshold;
		float m_fInjuredGetupImpulseMag2;
		float m_fMinTimeInGetUpToAllowCover;

		bool m_AllowNonPlayerHighFallAbort;
		bool m_AllowOffScreenHighFallAbort;
		u32 m_FallTimeBeforeHighFallAbort;
		float m_MinFallSpeedForHighFallAbort;
		float m_MinHeightAboveGroundForHighFallAbort;

		float m_PlayerMoverFixupMaxExtraHeadingChange;
		float m_AiMoverFixupMaxExtraHeadingChange;

		u32 GetStartClipWaitTime(const CPed* pPed);
		u32 GetStuckWaitTime(const CPed* pPed);

		u32 m_StartClipWaitTimePlayer;
		u32 m_StartClipWaitTime;
		u32 m_StuckWaitTime;
		u32 m_StuckWaitTimeMp;

		PAR_PARSABLE;
	};


	enum
	{
		State_Start = 0,
		State_ChooseGetupSet,
		State_PlayingGetUpClip,
		State_PlayReactionClip,
		State_PlayingGetUpBlend,
		State_AimingFromGround,
		State_ForceMotionState,
		State_SetBlendDuration,
		State_DirectBlendOut,
		State_StreamBlendOutClip_Clone,
		State_StreamBlendOutClipComplete_Clone,
		State_Crawl,
		State_DropDown,
		State_Finish
	};

	// Standard constructor. The task will determine which blend out set to use	based on a point cloud match. Other currently running tasks
	// can specify a specific set by implementing AddGetupSets().
	CTaskGetUp(BANK_ONLY(bool bForceEnableCollisionOnGetup = false));

	// Forces the task to use a specific nm blend out set (if in doubt use the default constructor)
	CTaskGetUp(eNmBlendOutSet forceBlendOutSet BANK_ONLY(, bool bForceEnableCollisionOnGetup = false));

	~CTaskGetUp();

	// Helper function for the constructors.
	void Init();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskGetUp(BANK_ONLY(m_bForceEnableCollisionOnGetup));
	}

	float GetPhase() const;

	////////////////////////////
	// CTask functions:
	static CTask* FindGetupControllingTask(CPed* pPed);
	static CTask* FindGetupControllingChild(CTask* pTask);

	// Clone task implementation
	virtual bool                ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType)	{ return false; }
	virtual bool                OverridesNetworkBlender(CPed*);
	virtual	bool				OverridesNetworkHeadingBlender(CPed* pPed);
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool				IsInScope(const CPed* pPed);
	
	virtual bool				HandlesRagdoll(const CPed* pPed) const 
	{ 
		if (GetState()<State_PlayingGetUpClip)
		{
			return true; 
		}
		else 
		{
			return CTask::HandlesRagdoll(pPed); 
		}
	}

	void						SetLocallyCreated() { m_bCloneTaskLocallyCreated = true; }
	bool						IsTaskRunningOverNetwork_Clone(CTaskTypes::eTaskType const _taskType) const;

	CNmBlendOutItem*			GetNextBlendOutItem();

	void						SetTarget(CAITarget& target) { m_Target = target; }
	void						SetMoveTask(CTask* pTask) { m_pMoveTask = pTask; }

	static eNmBlendOutSet PickStandardGetupSet(CPed* pPed);
	static bool ShouldUseInjuredGetup(CPed* pPed);

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	bool ShouldAbortExternal(const aiEvent* pEvent);

protected:
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32 GetDefaultStateAfterAbort() const {return State_Finish;}

	void CleanUp();

	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_ChooseGetupSet",
			"State_PlayingGetUpClip",
			"State_PlayReactionClip",
			"State_PlayingGetUpBlend",
			"State_AimingFromGround",
			"State_ForceMotionState",
			"State_SetBlendDuration",
			"State_DirectBlendOut",
			"State_StreamBlendOutClip_Clone",
			"State_StreamBlendOutClipComplete_Clone",
			"State_Crawl",
			"State_DropDown",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

private:

	//////////////////////////////////////////////
	// Helper functions for FSM state management:
	FSM_Return Start_OnUpdate(CPed* pPed);

	void       ChooseGetupSet_OnEnter(CPed* pPed);
	FSM_Return ChooseGetupSet_OnUpdate(CPed* pPed);
	void       ChooseGetupSet_OnExit(CPed* pPed);

	void       PlayingGetUpClip_OnEnter(CPed* pPed);
	FSM_Return PlayingGetUpClip_OnUpdate(CPed* pPed);
	void       PlayingGetUpClip_OnExit(CPed* pPed);

	void       PlayingReactionClip_OnEnter(CPed* pPed);
	FSM_Return PlayingReactionClip_OnUpdate(CPed* pPed);

	void       PlayingGetUpBlend_OnEnter(CPed* pPed);
	FSM_Return PlayingGetUpBlend_OnUpdate(CPed* pPed);

	void       AimingFromGround_OnEnter(CPed* pPed);
	FSM_Return AimingFromGround_OnUpdate(CPed* pPed);
	void       AimingFromGround_OnExit();

	void       ForceMotionState_OnEnter(CPed* pPed);
	FSM_Return ForceMotionState_OnUpdate(CPed* pPed);

	void       SetBlendDuration_OnEnter(CPed* pPed);
	FSM_Return SetBlendDuration_OnUpdate(CPed* pPed);

	void       DirectBlendOut_OnEnter(CPed* pPed);
	FSM_Return DirectBlendOut_OnUpdate(CPed* pPed);

	FSM_Return StreamBlendOutClip_Clone_OnUpdate(CPed* pPed);

	void       Crawl_OnEnter(CPed* pPed);
	FSM_Return Crawl_OnUpdate(CPed* pPed);
	void       Crawl_OnExit(CPed* pPed);

	void       DropDown_OnEnter(CPed* pPed);
	FSM_Return DropDown_OnUpdate(CPed* pPed);
	void       DropDown_OnExit(CPed* pPed);

	FSM_Return Finish_OnUpdate();

#if !__FINAL
	virtual void Debug() const;
#endif //!__FINAL

#if DEBUG_DRAW && __DEV && !__SPU
	void DebugDrawPedCapsule(const Matrix34& transform, const Color32& color, u32 uExpiryTime = 0, u32 uKey = 0, float fCapsuleRadiusMult = 1.0f, float fBoundHeading = 0.0f, float fBoundPitch = 0.0f, const Vector3& vBoundOffset = VEC3_ZERO);
#endif // DEBUG_DRAW && __DEV && !__SPU

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_GET_UP; }

	// Helper functions:
	bool FindSafePosition(CPed* pPed, const Vector3& vNearThisPosition, Vector3& vOut);
	bool IsValidBlockingEntity(CPed* pPed, CEntity* pEntity, bool bCapsuleTest = false) const;
	bool CanCrawl() const;
	bool FixupClipMatrix(CPed* pPed, Matrix34* pAnimMatrix, CNmBlendOutItem* pBlendOutItem, eNmBlendOutSet eBlendOutSet, float fBlendOutTime);
	void ActivateAnimatedInstance(CPed* pPed, Matrix34* pClipMatrix);

	void GetDefaultArmsDuration(const crClip* pClip);

	bool GetTreatAsPlayer(const CPed& ped) const;

	void DoDirectBlendOut();

	bool PickClipUsingPointCloud(CPed* pPed, Matrix34* pClipMatrix, CNmBlendOutItem*& outItem, eNmBlendOutSet& outSet, float& outTime);
	static bool ShouldUseArmedGetup(CPed* pPed);
	static bool PlayerWantsToAimOrFire(CPed* pPed);
	static int ExcludeUnderwaterBikes(CPed* pPed, const CEntity** const ppEntitiesToExclude, int iMaxEntitiesToExclude);

	bool ShouldBlendOutDirectly();
	bool ShouldPlayReactionClip(CPed *pPed);
	bool ShouldDoDirectionalGetup();
	static bool ShouldUseStandingBlendOutSet(CPed* pPed );
	bool ShouldBlockingEntityKillPlayer(const CEntity* pEnt);

	bool PickReactionClip(CPed *pPed, fwMvClipSetId &clipSet, fwMvClipId &clip);

	void UpdateBlendDirectionSignal();
	float CalcDesiredDirection();
	float ConvertRadianToSignal(float angle, float fMaxAngle = PI);

	atHashString GetNextItem() {return m_pMatchedItem!=NULL ? m_pMatchedItem->m_nextItemId : atHashString((u32)0); }
	void	DecideOnNextAction(atHashString nextItemId);
	void	SetStateFromBlendOutItem(CNmBlendOutItem* pItem);

	bool	TestWorldForFlatnessAndClearanceForLyingDownPed(CPed* pPed) const;

	float GetPlaybackRateModifier() const;

	void StartMoveNetwork();

	bool TestLOS(const Vector3& vStart, const Vector3& vEnd) const;

	const crClip* GetClip() const;

	// Helper function to handle firing guns during getup animations.
	void ProcessEarlyFiring(CPed* pPed);
	void ProcessUpperBodyAim(CPed* pPed, const crClip* pClip, float fPhase);
	void ProcessCoverInterrupt(CPed& rPed);
	void ProcessDropDown(CPed* pPed);

	bool IsAllowedWhenDead(eNmBlendOutSet blendOutSet);

	float ProcessMoverFixup(CPed* pPed, const crClip* pClip, float fPhase, float fReferenceHeading);

public:

	void TaskSetState(u32 const state);

	void SetRunningAsMotionTask(bool bIsMotionTask) { m_bRunningAsMotionTask=bIsMotionTask;}

	bool GetIsStuck() {return m_nStuckCounter > 0;}

	// Set by nm control when coming out of a balance behaviour (shouldn't do bumped reactions from things like shots or falls)
	void SetAllowBumpedReaction(bool bAllow) { m_bAllowBumpReaction = bAllow; }
	void SetBumpedByEntity(CEntity* pEnt) { m_pBumpedByEntity = pEnt; }

	void SetForceUpperBodyAim(bool bForce) { m_bForceUpperBodyAim = bForce; }

	void SetExclusionEntity(const CEntity* pEnt) { m_pExclusionEntity = pEnt; }

	void SetDisableVehicleImpacts(bool bDisableImpacts) { m_bDisableVehicleImpacts = bDisableImpacts; }

	// called by the collision event scanner to determine whether a balance or a roll up should be used when
	// the ped is bumped whilst doing a getup.
	bool ShouldRollupOnRagdollImpact() const { return false; }

	const CAITarget& GetTarget() const { return m_Target; }

	virtual const CEntity* GetTargetPositionEntity() const
	{ 
		if(GetSubTask() && !m_Target.GetEntity())
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return m_Target.GetEntity();
	}

	static void InitMetadata(u32 initMode);
	static void ShutdownMetadata(u32 shutdownMode);

	// PURPOSE: helper function to pick the correct nm blend out set to use
	// PARAMS:	pPed - pointer to the ped
	//			list - the pose set list that will be populated with the appropriate blend out sets
	//			rootMatrix - best guess at the animated position to match against the ragdolling ped (usually the upright root position).
	static void SelectBlendOutSets(CPed* pPed, CNmBlendOutSetList& list, Matrix34* rootMatrix, CTaskGetUp* pGetUpTask = NULL);

	static bool GetRequestedBlendOutSets(CNmBlendOutSetList& sets, CPed* pPed);
	void NotifyGetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed);
	static void RequestAssetsForGetup(CPed* pPed);
		
	static const fwClipSetWithGetup* FindClipSetWithGetup(const CPed* pPed);

	void ActivateSlopeFixup(CPed* pPed, float blendRate);

	bool IsSlopeFixupActive(const CPed* pPed);

	// PURPOSE: Decide whether we should branch to a new getup item based
	// on branch tags
	// RETURNS: True if a new branch was triggered
	// PARAMS: nextItemId - the id of the next item to go to. Unchanged if no branch is triggered.
	bool ProcessBranchTags(atHashString& nextItemId);

	static void ResetLastAimFromGroundStartTime() { ms_LastAimFromGroundStartTime = 0; }

	static void InitTunables(); 

public:

	// 
	// Clone target management functions (public so CClonedGetUpInfo can get / set)....
	//

	void			SetTargetOutOfScopeID(ObjectId _networkId )	{ m_TargetOutOfScopeId_Clone = _networkId;		 }
	inline ObjectId GetTargetOutOfScopeID(void )				{ return m_TargetOutOfScopeId_Clone; }

private:

	ObjectId m_TargetOutOfScopeId_Clone; 	// Target syncing
	bool m_bForceTreatAsPlayer;				// Treat clone ped as a player

	// Used to cache a the decision whether or not to perform the reaction clip in then
	// reaction clip state. internal use only.

	RegdConstEnt m_pExclusionEntity;

	bool m_bDoReactionClip : 1;
	bool m_bDroppingRifle : 1;

	bool m_bLookingAtCarJacker : 1;
public:

	bool m_bTagSyncBlend : 1;
	bool m_bRunningAsMotionTask : 1;
	bool m_bAllowBumpReaction : 1;	
	bool m_bFailedToLoadAssets : 1;	
	bool m_bInstantBlendInNetwork : 1; // if we're not coming from an anim, we need to instantly blend in the move network
	bool m_bInsertRagdollFrame : 1;	// if true, we're blenbding from animation to animation using the ragdoll frame, and need to include it in our move network.
	bool m_bBlendedInDefaultArms : 1;
	bool m_bDisableVehicleImpacts : 1;
	bool m_bForceUpperBodyAim : 1;
	bool m_bPressedToGoIntoCover : 1;
	bool m_bBranchTagsDirty : 1;
	bool m_bSearchedFromClipPos : 1;
	bool m_bDisableHighFallAbort : 1;
#if __BANK
	bool m_bForceEnableCollisionOnGetup : 1;
#endif

	static bool ms_bEnableBumpedReactionClips;
#if DEBUG_DRAW && __DEV
	static bool ms_bShowSafePositionChecks;
#endif // DEBUG_DRAW && __DEV

	u32 m_nStartClipCounter;
	u32 m_nStuckCounter;
	u32 m_nLastStandingTime;
	
	fwMvClipId	m_clipId;
	fwMvClipSetId m_clipSetId;

	float m_fBlendDuration;	
	float m_fBlendOutRootIkSlopeFixupPhase;

	RegdEnt m_pBumpedByEntity;
	s32 m_iBumpedByComponent;

	CMoveNetworkHelper m_networkHelper;

	float m_fGetupBlendReferenceHeading;
	float m_fGetupBlendInitialTargetHeading;
	float m_fGetupBlendCurrentHeading;
	float m_fLastExtraTurnAmount;

	eNmBlendOutSet m_forcedSet;			

	float m_matchedTime;
	eNmBlendOutSet m_matchedSet;		
	CNmBlendOutItem* m_pMatchedItem;	

	CNmBlendOutSetList m_pointCloudPoseSets;

	float m_defaultArmsBlendDuration;

	// A movement task to be run during the getup.
	// Allows making navmesh requests while playing getup anims.
	RegdTask m_pMoveTask;

	// The ai target entity for aim from ground and early firing
	CAITarget m_Target;	

	Vector3 m_vCrawlStartPosition;

	// Handles the upper body aiming anims
	CUpperBodyAimPitchHelper m_UpperBodyAimPitchHelper;

	class CWritheClipSetRequestHelper* pWritheClipSetRequestHelper;
	u32 m_NextEarlyFireTime;

	// refs the getup set selected by the task
	CGetupSetRequestHelper m_GetupRequestHelper;

	// This is a big structure (because it needs to store data for asynchronous probes) so only allocate if necessary
	CGetupProbeHelper* m_ProbeHelper;

	// Clone only - loads the blend out sets on the clone as TaskNM ::RequestGetupAssets( ) made by the controlling task aren't made on a clone....
	static const u32 ms_iMaxCombatGetupHelpers = 5; 
	CGetupSetRequestHelper m_cloneSetRequestHelpers[ms_iMaxCombatGetupHelpers]; 

	// our tunables
	static Tunables				sm_Tunables;

	static u32 ms_LastAimFromGroundStartTime;
	static u32 ms_NumPedsAimingFromGround;

	//////////////////////////////////////////////////////////////////////////
	// MoVE network signals
	//////////////////////////////////////////////////////////////////////////

	static fwMvRequestId ms_GetUpClipRequest;
	static fwMvRequestId ms_GetUpBlendRequest;
	static fwMvRequestId ms_ReactionClipRequest;

	static fwMvBooleanId ms_OnEnterGetUpClip;
	static fwMvBooleanId ms_OnEnterGetUpBlend;
	static fwMvBooleanId ms_OnEnterReactionClip;
	static fwMvBooleanId ms_CanFireWeapon;
	static fwMvBooleanId ms_DropRifle;

	static fwMvBooleanId ms_ReactionClipFinished;
	static fwMvBooleanId ms_GetUpBlendFinished;
	static fwMvBooleanId ms_GetUpClipFinished;

	static fwMvFloatId		ms_BlendDuration;
	static fwMvRequestId	ms_TagSyncBlend;

	static fwMvFloatId		ms_RagdollFrameBlendDuration;
	static fwMvFrameId		ms_RagdollFrame;

	static fwMvClipSetVarId ms_GetUpBlendSet;
	static fwMvFloatId	ms_GetUpBlendDirection;

	static fwMvFlagId	ms_No180Blend;

	static fwMvFlagId	ms_HasWeaponEquipped;
	static fwMvBooleanId	ms_OnEnterWeaponArms;
	

	static fwMvClipId	ms_GetUpClip;
	static fwMvFloatId	ms_GetUpPhase180;
	static fwMvFloatId	ms_GetUpPhase90;
	static fwMvFloatId	ms_GetUpPhase0;
	static fwMvFloatId	ms_GetUpPhaseNeg90;
	static fwMvFloatId	ms_GetUpPhaseNeg180;
	static fwMvFloatId	ms_GetUpRate;
	static fwMvFloatId	ms_GetUpClip180PhaseOut;
	static fwMvFloatId	ms_GetUpClip90PhaseOut;
	static fwMvFloatId	ms_GetUpClip0PhaseOut;
	static fwMvFloatId	ms_GetUpClipNeg90PhaseOut;
	static fwMvFloatId	ms_GetUpClipNeg180PhaseOut;
	static fwMvBooleanId ms_GetUpLooped;

	static fwMvClipId	ms_ReactionClip;
	static fwMvFloatId	ms_ReactionClipPhase;
	static fwMvFloatId	ms_ReactionClipPhaseOut;

	static fwMvClipId	ms_GetUpBlendClipOut;

	static fwMvClipId	ms_GetUpSwimmingUnderwaterBackClip;

	static fwMvRequestId ms_BlendInDefaultArmsRequest;
	static fwMvFloatId	ms_DefaultArmsBlendInDuration;

#if __ASSERT
	static bool ms_SpewRagdollTaskInfoOnGetUpSelection;
#endif // __ASSERT

	static bool sm_bCloneGetupFix;

	private:

		bool m_bCloneTaskLocallyCreated : 1; // is this task locally created or was it created over the network - decides if we should generate our own data or use whatever the network sends us....
};

inline CNmBlendOutItem*	CTaskGetUp::GetNextBlendOutItem()
{
	if (m_matchedSet!=NMBS_INVALID && m_pMatchedItem && m_pMatchedItem->m_nextItemId.GetHash()!=0)
	{
		return CNmBlendOutSetManager::FindItem(m_matchedSet, m_pMatchedItem->m_nextItemId);
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Task info for getting up
//-------------------------------------------------------------------------
class CClonedGetUpInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams(
						u32 const state, 
						CAITarget const * target, 
						CNmBlendOutSetList const* blendOutSetList,
						bool forceTreatAsPlayer
					)
		:
			m_state(state),
			m_target(target),
			m_blendOutSetList(blendOutSetList),
			m_forceTreatAsPlayer(forceTreatAsPlayer)
		{}

		u32							m_state;
		CAITarget const*			m_target;
		CNmBlendOutSetList const *	m_blendOutSetList;
		bool						m_forceTreatAsPlayer;
	};

public:
	
	CClonedGetUpInfo(InitParams const& initParams);

	CClonedGetUpInfo();

	~CClonedGetUpInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_GET_UP;}

    virtual bool			HasState() const				{ return true; }
	virtual u32				GetSizeOfState() const			{ return datBitsNeeded<CTaskGetUp::State_Finish>::COUNT; }
    virtual const char*			GetStatusName(u32) const		{ return "Status";}

	virtual bool			AutoCreateCloneTask(CPed* pPed);
	virtual CTaskFSMClone*	CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		m_weaponTargetHelper.Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_numNmBlendOutSets, SIZEOF_NUM_BLEND_OUT_SETS, "Num NM Blend Out Sets");
		for(int i = 0; i < m_numNmBlendOutSets; ++i)
		{
			SERIALISE_INTEGER(serialiser, m_NmBlendOutSets[i], SIZEOF_BLEND_OUT_SET_HASH, "NM Blend Out Set Hash");	
			Assert(m_NmBlendOutSets[i] != 0);
		}

		SERIALISE_BOOL(serialiser, m_forceTreatAsPlayer);
	}

public:

	inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
	inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

	inline int				GetNumNmBlendOutSets()			const	{ return m_numNmBlendOutSets; } 
	inline int				GetNmBlendOutSet(int const i)	const	{ Assertf(i < m_numNmBlendOutSets, "ERROR : CClonedGetUpInfo::GetNmBlendOutSet : Invalid Index!");	return m_NmBlendOutSets[i];	}

	bool					GetForceTreatAsPlayer()			const	{ return m_forceTreatAsPlayer; }

private:

	CClonedGetUpInfo(const CClonedGetUpInfo &);
	CClonedGetUpInfo &operator=(const CClonedGetUpInfo &);

private:

	static const int SIZEOF_NUM_BLEND_OUT_SETS	= 4;
	static const int SIZEOF_BLEND_OUT_SET_HASH	= 32;

	void BuildBlendOutSetArray(CNmBlendOutSetList const * blendOutSetList);
	void ClearBlendOutSetArray(void);

	static const u32 MAX_NUM_BLEND_OUT_SETS = 10;

	int				m_numNmBlendOutSets;
	int				m_NmBlendOutSets[MAX_NUM_BLEND_OUT_SETS];

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;

	bool			m_forceTreatAsPlayer;
};

#endif // ! INC_TASKBLENDFROMNM_H_
